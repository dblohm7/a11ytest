#include <windows.h>
#include <windowsx.h>
#include <ShellScalingApi.h>
#include <uxtheme.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#include "winselect.h"

#define DECLARE_UNIQUE_HANDLE_TYPE(name, handle_type, deleter_type) \
  using name = std::unique_ptr<std::remove_pointer<handle_type>::type, deleter_type>
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

struct ModuleDeleter {
  using pointer = HMODULE;

  void operator()(pointer aPtr) {
    ::FreeLibrary(aPtr);
  }
};

DECLARE_UNIQUE_HANDLE_TYPE(UniqueModule, HMODULE, ModuleDeleter);
using GetDpiForMonitorPtr = LRESULT (WINAPI*)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*);

static const UINT kDefaultDpi = 96U;

static UINT GetEffectiveDpi(HMONITOR aMonitor) {
  static UniqueModule shcore(::LoadLibraryW(L"shcore.dll"));
  if (!shcore) {
    return kDefaultDpi;
  }

  static auto pGetDpiForMonitor =
    reinterpret_cast<GetDpiForMonitorPtr>(::GetProcAddress(shcore.get(),
                                                           "GetDpiForMonitor"));
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

template <typename HandleT>
static int ScaleWidth(HandleT aHandle, int aWidth) {
  return aWidth * GetEffectiveDpi(aHandle) / kDefaultDpi;
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

static RECT ComputeWindowPos(HWND aHwnd) {
  return ComputeWindowPos(::MonitorFromWindow(aHwnd, MONITOR_DEFAULTTONEAREST));
}

static RECT GetInitialWindowPos() {
  const POINT zeros = {};
  HMONITOR defaultMonitor = ::MonitorFromPoint(zeros, MONITOR_DEFAULTTOPRIMARY);

  return ComputeWindowPos(defaultMonitor);
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

static void Erase(HDC aDc, const RECT& aRect) {
  WNDCLASSEX clsEx = { sizeof(clsEx) };
  GetClassInfoEx((HINSTANCE) ::GetModuleHandle(nullptr), kClassName, &clsEx);

  HBRUSH bgBrush = clsEx.hbrBackground;
  // If the window class specifies a system brush color, convert it to a real
  // HBRUSH
  if (bgBrush <= ((HBRUSH)(COLOR_MENUBAR + 1))) {
    bgBrush = GetSysColorBrush(((intptr_t)bgBrush) - 1);
  }

  ::SelectObject(aDc, bgBrush);
  ::FillRect(aDc, &aRect, bgBrush);
}

static void PaintCrossHairs(HWND aHwnd, HDC aDc) {
  if (prevCursor) {
    // We don't paint the cursor during mouse capture
    return;
  }

  RECT rect;
  if (!::GetClientRect(aHwnd, &rect)) {
    return;
  }

  int w = GetScaledSystemMetric(aHwnd, SM_CXCURSOR);
  int h = GetScaledSystemMetric(aHwnd, SM_CYCURSOR);

  int x = (rect.right - w) / 2;
  int y = (rect.bottom - h) / 2;

  ::DrawIconEx(aDc, x, y, (HICON) ::LoadCursor(nullptr, IDC_CROSS), w, h, 0,
               nullptr, DI_NORMAL);
}

static
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg) {
    case WM_LBUTTONDOWN:
      prevCursor = SetCursor(LoadCursor(NULL, IDC_CROSS));
      SetCapture(hwnd);
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
      // Not using lParam here because it gives us weird dimensions; let's
      // just compute new width and height based on hwnd
      RECT newRect = ComputeWindowPos(hwnd);
      ::SetWindowPos(hwnd, nullptr, 0, 0, newRect.right,
                     newRect.bottom, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
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

  RECT initialWindowPos = GetInitialWindowPos();

  HWND hwnd = CreateWindowW(kClassName, L"Window Selector",
                            WS_OVERLAPPEDWINDOW, initialWindowPos.left,
                            initialWindowPos.top, initialWindowPos.right,
                            initialWindowPos.bottom, NULL, NULL, hInst, nullptr);
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

  UnregisterClassW(kClassName, hInst);

  return target;
}

} // namespace aspk

