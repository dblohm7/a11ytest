#include <windows.h>
#include <windowsx.h>

#include "winselect.h"

static HWND target = NULL;
static HWND lastTarget = NULL;
static HCURSOR prevCursor = NULL;
static bool breakMsgLoop = false;

static void
Highlight(HWND hwnd)
{
  if (!hwnd) {
    return;
  }

  int cxScreen = GetSystemMetrics(SM_CXSCREEN);
  int cyScreen = GetSystemMetrics(SM_CYSCREEN);
  int cxBorder = GetSystemMetrics(SM_CXBORDER);
  int cxFrame = GetSystemMetrics(SM_CXFRAME);
  int cyFrame = GetSystemMetrics(SM_CYFRAME);

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
      SendMessage(hwnd, WM_CLOSE, 0, 0);
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
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static const WCHAR sClassName[] = L"ASPKWindowSelector";

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
  wc.lpszClassName = sClassName;

  if (!RegisterClassW(&wc)) {
    return nullptr;
  }

  HWND hwnd = CreateWindowW(sClassName, L"Window Selector",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL,
                            NULL, hInst, nullptr);
  if (!hwnd) {
    UnregisterClassW(sClassName, hInst);
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

  UnregisterClassW(sClassName, hInst);

  return target;
}

} // namespace aspk

