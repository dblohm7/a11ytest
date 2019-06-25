#include <windows.h>
#include <windowsx.h>
#include <ShellScalingApi.h>
#include <uxtheme.h>
#include <vssym32.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#include "ArrayLength.h"
#include "DynamicallyLinkedFunctionPtr.h"
#include "winselect.h"

#define DECLARE_UNIQUE_HANDLE_TYPE(name, deleter_type) \
  using name = std::unique_ptr<std::remove_pointer<deleter_type::pointer>::type, deleter_type>

#if defined(min)
#undef min
#endif  // defined(min)

#if defined(max)
#undef max
#endif  // defined(max)

static const WCHAR kClassName[] = L"ASPKWindowSelector";

static HWND target = NULL;
static HWND lastTarget = NULL;
static HCURSOR prevCursor = NULL;
static bool breakMsgLoop = false;

static const int kDefaultWindowWidth = 273;

struct GdiObjectDeleter {
  using pointer = HGDIOBJ;

  void operator()(pointer aPtr) {
    ::DeleteObject(aPtr);
  }
};

struct ThemeDeleter {
  using pointer = HTHEME;

  void operator()(pointer aPtr) {
    ::CloseThemeData(aPtr);
  }
};

DECLARE_UNIQUE_HANDLE_TYPE(UniqueGdiObject, GdiObjectDeleter);
DECLARE_UNIQUE_HANDLE_TYPE(UniqueTheme, ThemeDeleter);

static const UINT kDefaultDpi = 96U;

static UINT GetEffectiveDpi(HMONITOR aMonitor) {
  static const mozilla::DynamicallyLinkedFunctionPtr<decltype(&::GetDpiForMonitor)>
    pGetDpiForMonitor(L"shcore.dll", "GetDpiForMonitor");
  if (!pGetDpiForMonitor) {
    return kDefaultDpi;
  }

  UINT dpiX, dpiY;
  if (FAILED(pGetDpiForMonitor(aMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
    return kDefaultDpi;
  }

  return std::max(dpiX, dpiY);
}

static UINT GetEffectiveDpi(HWND aHwnd) {
  HMONITOR monitor = ::MonitorFromWindow(aHwnd, MONITOR_DEFAULTTONEAREST);
  return GetEffectiveDpi(monitor);
}

static UINT GetEffectiveDpi(UINT aDpi) {
  // Identity
  return aDpi;
}

template <typename HandleT>
static int ScaleWidth(HandleT aHandle, int aWidth) {
  return ::MulDiv(aWidth, GetEffectiveDpi(aHandle), kDefaultDpi);
}

static int GetScaledSystemMetric(HWND aHwnd, int aSmIndex) {
  return ScaleWidth(aHwnd, ::GetSystemMetrics(aSmIndex));
}

static RECT ComputeWindowPos(HMONITOR aMonitor) {
  UINT dpi = GetEffectiveDpi(aMonitor);

  int scaled = ScaleWidth(aMonitor, kDefaultWindowWidth);

  RECT result = {CW_USEDEFAULT, CW_USEDEFAULT, scaled, scaled};
  return result;
}

static const wchar_t kTextMsg[] =
  L"Click and drag the crosshairs over the window you want to select:";

template <typename FnT>
static bool WithThemedFont(HWND aHwnd, HDC aDc, FnT aFunc) {
  const wchar_t kThemeClass[] = L"CompositedWindow::Window";
  UniqueTheme theme(::OpenThemeData(aHwnd, kThemeClass));

  LOGFONT logFont;
  if (FAILED(::GetThemeSysFont(theme.get(), TMT_MSGBOXFONT, &logFont))) {
    return false;
  }

  // GetThemeSysFont is using GetDeviceCaps(LOGPIXELSY) for its DPI scaling,
  // so it is not multi-monitor aware.
  if (::GetMapMode(aDc) == MM_TEXT && logFont.lfHeight < 0 &&
      GetEffectiveDpi(aHwnd) != ::GetDeviceCaps(aDc, LOGPIXELSY)) {
    logFont.lfHeight = ScaleWidth(aHwnd, logFont.lfHeight);
  }

  UniqueGdiObject font(::CreateFontIndirect(&logFont));
  if (!font) {
    return false;
  }

  HGDIOBJ prev = ::SelectObject(aDc, font.get());
  bool ok = aFunc(aHwnd, aDc, theme.get());
  ::SelectObject(aDc, prev);
  return ok;
}

static RECT ComputeWindowPos(HWND aHwnd) {
  RECT defaultPos = ComputeWindowPos(::MonitorFromWindow(aHwnd,
                                                         MONITOR_DEFAULTTONEAREST));

  auto getExtents = [&defaultPos](HWND aHwnd, HDC aDc, HTHEME aTheme) -> bool {
    RECT boundingRect;
    if (!::GetClientRect(aHwnd, &boundingRect)) {
      return false;
    }

    RECT extents;
    HRESULT hr = ::GetThemeTextExtent(aTheme, aDc, 0, 0, kTextMsg,
                                      ArrayLength(kTextMsg) - 1,
                                      DT_CENTER | DT_TOP | DT_SINGLELINE,
                                      &boundingRect, &extents);
    if (FAILED(hr)) {
      return false;
    }

    // extents are for client, so we need to adjust for window
    WINDOWINFO wi;
    if (!::GetWindowInfo(aHwnd, &wi)) {
      // Just use the defaults if we fail
      return true;
    }

    if (!::AdjustWindowRectEx(&extents, wi.dwStyle, FALSE, wi.dwExStyle)) {
      // Just use the defaults if we fail
      return true;
    }

    // We need to adjust extents.right to be based from 0, 0
    extents.right -= extents.left;

    // Also add some padding
    extents.right += 2 * GetScaledSystemMetric(aHwnd, SM_CYCAPTION);

    int w = std::max(defaultPos.right, extents.right);
    defaultPos.right = w;
    defaultPos.bottom = w;
    return true;
  };

  HDC dc = ::GetDC(aHwnd);

  WithThemedFont(aHwnd, dc, getExtents);

  ::ReleaseDC(aHwnd, dc);

  return defaultPos;
}

static HMONITOR GetDefaultMonitor() {
  const POINT zeros = {};
  return ::MonitorFromPoint(zeros, MONITOR_DEFAULTTOPRIMARY);
}

static void
Highlight(HWND hwnd)
{
  if (!hwnd) {
    return;
  }

  int cxBorder = GetScaledSystemMetric(hwnd, SM_CXBORDER);

  RECT rect;
  GetWindowRect(hwnd, &rect);

  HDC hdc = GetWindowDC(hwnd);
  SetROP2(hdc, R2_NOT);
  HPEN pen = CreatePen(PS_INSIDEFRAME, 3 * cxBorder, RGB(0, 0, 0));
  HGDIOBJ oldPen = SelectObject(hdc, pen);
  HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

  Rectangle(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

  SelectObject(hdc, oldBrush);
  SelectObject(hdc, oldPen);
  DeleteObject(pen);
  ReleaseDC(hwnd, hdc);
  // This call makes the difference between something that worked and something that didn't.
  InvalidateRect(hwnd, &rect, TRUE);
}

static void ReviseWindowSize(HWND aHwnd) {
  RECT newRect = ComputeWindowPos(aHwnd);
  ::SetWindowPos(aHwnd, nullptr, 0, 0, newRect.right,
                 newRect.bottom, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

static void Erase(HDC aDc, const RECT& aRect) {
  WNDCLASSEX clsEx = { sizeof(clsEx) };
  GetClassInfoEx((HINSTANCE) ::GetModuleHandle(nullptr), kClassName, &clsEx);

  HBRUSH bgBrush = clsEx.hbrBackground;
  // If the window class specifies a system brush color, convert it to a real
  // HBRUSH
  if (bgBrush <= ((HBRUSH)(COLOR_MENUBAR + 1))) {
    bgBrush = GetSysColorBrush(((intptr_t)bgBrush) - 1);
  }

  HGDIOBJ prev = ::SelectObject(aDc, bgBrush);
  ::FillRect(aDc, &aRect, bgBrush);
  ::SelectObject(aDc, prev);
}

static void PaintCrossHairs(HWND aHwnd, HDC aDc) {
  RECT rect;
  if (!::GetClientRect(aHwnd, &rect)) {
    return;
  }

  WithThemedFont(aHwnd, aDc, [&rect](HWND aHwnd, HDC aDc, HTHEME aTheme) -> bool {
    DTTOPTS dttOpts = { sizeof(DTTOPTS) };
    dttOpts.dwFlags = DTT_COMPOSITED;
    HRESULT hr = ::DrawThemeTextEx(aTheme, aDc, 0, 0, kTextMsg,
                                   ArrayLength(kTextMsg) - 1,
                                   DT_CENTER | DT_SINGLELINE | DT_TOP |
                                     DT_END_ELLIPSIS, &rect, &dttOpts);
    return SUCCEEDED(hr);
  });

  if (prevCursor) {
    // We don't paint the cursor during mouse capture
    return;
  }

  int w = GetScaledSystemMetric(aHwnd, SM_CXCURSOR);
  int h = GetScaledSystemMetric(aHwnd, SM_CYCURSOR);

  int x = (rect.right - w) / 2;
  int y = (rect.bottom - h) / 2;

  HANDLE cursor = ::LoadImage(nullptr, IDC_CROSS, IMAGE_CURSOR, 0, 0, LR_SHARED);
  if (cursor) {
    ::DrawIconEx(aDc, x, y, (HICON) cursor, w, h, 0, nullptr, DI_NORMAL);
  }
}

static
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg) {
    case WM_CREATE: {
      ReviseWindowSize(hwnd);
      return 0;
    }
    case WM_LBUTTONDOWN:
      prevCursor = SetCursor(LoadCursor(NULL, IDC_CROSS));
      SetCapture(hwnd);
      InvalidateRect(hwnd, nullptr, TRUE);
      break;
    case WM_LBUTTONUP:
      SetCursor(prevCursor);
      prevCursor = NULL;
      ReleaseCapture();
      lastTarget = nullptr;
      Highlight(target); // Target was highlighted, turn it off
      // Don't kill ourselves unless the mouse had moved outside our hwnd
      if (target == hwnd) {
        target = nullptr;
        InvalidateRect(hwnd, nullptr, TRUE);
      } else {
        SendMessage(hwnd, WM_CLOSE, 0, 0);
      }
      break;
    case WM_MOUSEMOVE:
      if (GetCapture() == hwnd) {
        POINT pt;
        pt.x = GET_X_LPARAM(lparam);
        pt.y = GET_Y_LPARAM(lparam);
        if (ClientToScreen(hwnd, &pt)) {
          target = WindowFromPoint(pt);
          if (target != lastTarget) {
            // Turn on indicator for target and turn off indicator for lastTarget
            Highlight(target);
            Highlight(lastTarget);
            lastTarget = target;
          }
        }
      }
      break;
    case WM_NCDESTROY:
      breakMsgLoop = true;
      break;
    case WM_PAINT: {
      bool remote = ::GetSystemMetrics(SM_REMOTESESSION);
      PAINTSTRUCT ps;
      HDC dc = ::BeginPaint(hwnd, &ps);
      HDC paintDc = dc;
      HPAINTBUFFER buffer = nullptr;
      if (!remote) {
        buffer = ::BeginBufferedPaint(dc, &ps.rcPaint, BPBF_TOPDOWNDIB,
                                      nullptr, &paintDc);
      }
      if (ps.fErase) {
        Erase(paintDc, ps.rcPaint);
      }
      PaintCrossHairs(hwnd, paintDc);
      if (!remote && buffer) {
        ::EndBufferedPaint(buffer, TRUE);
      }
      ::EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 0;
    case WM_DPICHANGED: {
      auto newRect = reinterpret_cast<RECT*>(lparam);
      ::MoveWindow(hwnd, newRect->left, newRect->top,
                   newRect->right - newRect->left,
                   newRect->bottom - newRect->top, TRUE);
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

namespace aspk {

HWND
SelectWindow()
{
  HINSTANCE hInst = (HINSTANCE) ::GetModuleHandle(nullptr);

  WNDCLASSW wc = {0};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &WindowProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = kClassName;

  if (!RegisterClassW(&wc)) {
    return nullptr;
  }

  bool buffered = SUCCEEDED(::BufferedPaintInit());

  HWND hwnd = CreateWindowW(kClassName, L"Window Selector",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL,
                            NULL, hInst, nullptr);
  if (!hwnd) {
    UnregisterClassW(kClassName, hInst);
    return nullptr;
  }
  ShowWindow(hwnd, SW_SHOWNORMAL);

  MSG msg;
  BOOL bRet;
  while (!breakMsgLoop && (bRet = GetMessage(&msg, nullptr, 0, 0))) {
    if (bRet == -1) {
      return nullptr;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  if (buffered) {
    BufferedPaintUnInit();
  }

  UnregisterClassW(kClassName, hInst);

  return target;
}

} // namespace aspk

