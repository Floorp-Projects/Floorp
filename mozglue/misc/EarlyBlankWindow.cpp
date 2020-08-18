/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyBlankWindow.h"

#include "mozilla/Assertions.h"
#include "mozilla/glue/Debug.h"
#include "mozilla/WindowsVersion.h"
#include "nsDebug.h"

namespace mozilla {

static const wchar_t kEarlyBlankWindowKeyPath[] =
    L"SOFTWARE"
    L"\\" MOZ_APP_VENDOR L"\\" MOZ_APP_BASENAME L"\\EarlyBlankWindowSettings";

static bool sEarlyBlankWindowKeyOpen = false;
static HWND sWnd;
static HKEY sEarlyBlankWindowKey;
static LPWSTR const gStockApplicationIcon = MAKEINTRESOURCEW(32512);

typedef BOOL(WINAPI* EnableNonClientDpiScalingProc)(HWND);
static EnableNonClientDpiScalingProc sEnableNonClientDpiScaling = NULL;
typedef int(WINAPI* GetSystemMetricsForDpiProc)(int, UINT);
GetSystemMetricsForDpiProc sGetSystemMetricsForDpi = NULL;
typedef UINT(WINAPI* GetDpiForWindowProc)(HWND);
GetDpiForWindowProc sGetDpiForWindow = NULL;

#if WINVER < 0x0605
WINUSERAPI DPI_AWARENESS_CONTEXT WINAPI GetThreadDpiAwarenessContext();
WINUSERAPI BOOL WINAPI AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT,
                                                    DPI_AWARENESS_CONTEXT);
#endif /* WINVER < 0x0605 */

static uint32_t sWindowWidth;
static uint32_t sWindowHeight;
static double sCSSToDevPixelScaling;

// We style our initial blank window as a WS_POPUP to eliminate the window
// caption and all that jazz. Alternatively, we could do a big dance in our
// window proc to paint into the nonclient area similarly to what we do in
// nsWindow, but it would be nontrivial code duplication, and the added
// complexity would not be worth it, given that we can just change the
// window style to our liking when we consume sWnd from nsWindow.
static DWORD sWindowStyle = WS_POPUP;

// We add WS_EX_TOOLWINDOW here so that we do not produce a toolbar entry.
// We were not able to avoid flickering in the toolbar without this change,
// as the call to ::SetWindowLongPtrW to restyle the window inside
// nsWindow causes the toolbar entry to momentarily disappear. Not sure of
// the cause of this, but it doesn't feel too wrong to be missing a toolbar
// entry only so long as we are displaying a skeleton UI.
static DWORD sWindowStyleEx = WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW;

int CSSToDevPixels(int cssPixels, double scaling) {
  double asDouble = (double)cssPixels * scaling;
  return floor(asDouble + 0.5);
}

void DrawSkeletonUI(HWND hWnd) {
  // TODO - here is where we will draw a skeleton UI for the application
}

LRESULT WINAPI EarlyBlankWindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  if (msg == WM_NCCREATE && sEnableNonClientDpiScaling) {
    sEnableNonClientDpiScaling(hWnd);
  }

  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool EnsureEarlyBlankWindowKeyOpen() {
  if (sEarlyBlankWindowKeyOpen) {
    return true;
  }

  DWORD disposition;
  LSTATUS result = ::RegCreateKeyExW(
      HKEY_CURRENT_USER, kEarlyBlankWindowKeyPath, 0, nullptr, 0,
      KEY_ALL_ACCESS, nullptr, &sEarlyBlankWindowKey, &disposition);

  if (result != ERROR_SUCCESS) {
    return false;
  }

  if (disposition == REG_CREATED_NEW_KEY) {
    sEarlyBlankWindowKeyOpen = true;
    return false;
  }

  if (disposition == REG_OPENED_EXISTING_KEY) {
    sEarlyBlankWindowKeyOpen = true;
    return true;
  }

  return false;
}

void CreateAndStoreEarlyBlankWindow(HINSTANCE hInstance) {
  if (!IsWin10OrLater() || !EnsureEarlyBlankWindowKeyOpen()) {
    return;
  }

  DWORD dataLen = sizeof(uint32_t);
  uint32_t enabled;
  LSTATUS result = ::RegGetValueW(sEarlyBlankWindowKey, nullptr, L"enabled",
                                  RRF_RT_REG_DWORD, nullptr,
                                  reinterpret_cast<PBYTE>(&enabled), &dataLen);
  if (result != ERROR_SUCCESS || enabled == 0) {
    return;
  }

  // EnableNonClientDpiScaling must be called during the initialization of
  // the window, so we have to find it and store it before we create our
  // window in order to run it in our WndProc.
  HMODULE user32Dll = ::GetModuleHandleW(L"user32");

  if (user32Dll) {
    auto getThreadDpiAwarenessContext =
        (decltype(GetThreadDpiAwarenessContext)*)::GetProcAddress(
            user32Dll, "GetThreadDpiAwarenessContext");
    auto areDpiAwarenessContextsEqual =
        (decltype(AreDpiAwarenessContextsEqual)*)::GetProcAddress(
            user32Dll, "AreDpiAwarenessContextsEqual");
    if (getThreadDpiAwarenessContext && areDpiAwarenessContextsEqual &&
        areDpiAwarenessContextsEqual(getThreadDpiAwarenessContext(),
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
      // Only per-monitor v1 requires these workarounds.
      sEnableNonClientDpiScaling =
          (EnableNonClientDpiScalingProc)::GetProcAddress(
              user32Dll, "EnableNonClientDpiScaling");
    }

    sGetSystemMetricsForDpi = (GetSystemMetricsForDpiProc)::GetProcAddress(
        user32Dll, "GetSystemMetricsForDpi");
    sGetDpiForWindow =
        (GetDpiForWindowProc)::GetProcAddress(user32Dll, "GetDpiForWindow");
  }

  WNDCLASSW wc;
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = EarlyBlankWindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = ::LoadIconW(::GetModuleHandleW(nullptr), gStockApplicationIcon);
  wc.hCursor = ::LoadCursor(nullptr, IDC_WAIT);
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;

  // TODO: just ensure we disable this if we've overridden the window class
  wc.lpszClassName = L"MozillaWindowClass";

  if (!::RegisterClassW(&wc)) {
    printf_stderr("RegisterClassW error %lu\n", GetLastError());
    return;
  }

  uint32_t screenX;
  result = ::RegGetValueW(sEarlyBlankWindowKey, nullptr, L"screenX",
                          RRF_RT_REG_DWORD, nullptr,
                          reinterpret_cast<PBYTE>(&screenX), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading screenX %lu\n", GetLastError());
    return;
  }

  uint32_t screenY;
  result = ::RegGetValueW(sEarlyBlankWindowKey, nullptr, L"screenY",
                          RRF_RT_REG_DWORD, nullptr,
                          reinterpret_cast<PBYTE>(&screenY), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading screenY %lu\n", GetLastError());
    return;
  }

  result =
      ::RegGetValueW(sEarlyBlankWindowKey, nullptr, L"width", RRF_RT_REG_DWORD,
                     nullptr, reinterpret_cast<PBYTE>(&sWindowWidth), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading width %lu\n", GetLastError());
    return;
  }

  result = ::RegGetValueW(sEarlyBlankWindowKey, nullptr, L"height",
                          RRF_RT_REG_DWORD, nullptr,
                          reinterpret_cast<PBYTE>(&sWindowHeight), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading height %lu\n", GetLastError());
    return;
  }

  dataLen = sizeof(double);
  result = ::RegGetValueW(
      sEarlyBlankWindowKey, nullptr, L"cssToDevPixelScaling", RRF_RT_REG_BINARY,
      nullptr, reinterpret_cast<PBYTE>(&sCSSToDevPixelScaling), &dataLen);
  if (result != ERROR_SUCCESS || dataLen != sizeof(double)) {
    printf_stderr("Error reading cssToDevPixelScaling %lu\n", GetLastError());
    return;
  }

  sWnd = ::CreateWindowExW(sWindowStyleEx, L"MozillaWindowClass", L"",
                           sWindowStyle, screenX, screenY, sWindowWidth,
                           sWindowHeight, nullptr, nullptr, hInstance, nullptr);

  ::ShowWindow(sWnd, SW_SHOWNORMAL);
  ::SetWindowPos(sWnd, 0, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
                     SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
  DrawSkeletonUI(sWnd);
  ::RedrawWindow(sWnd, NULL, NULL, RDW_INVALIDATE);
}

HWND ConsumeEarlyBlankWindowHandle() {
  HWND result = sWnd;
  sWnd = nullptr;
  return result;
}

void PersistEarlyBlankWindowValues(int screenX, int screenY, int width,
                                   int height, double cssToDevPixelScaling) {
  LSTATUS result;
  result = ::RegSetValueExW(sEarlyBlankWindowKey, L"screenX", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&screenX), sizeof(screenX));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting screenX to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(sEarlyBlankWindowKey, L"screenY", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&screenY), sizeof(screenY));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting screenY to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(sEarlyBlankWindowKey, L"width", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&width), sizeof(width));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting width to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(sEarlyBlankWindowKey, L"height", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&height), sizeof(height));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting height to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(sEarlyBlankWindowKey, L"cssToDevPixelScaling", 0,
                            REG_BINARY,
                            reinterpret_cast<PBYTE>(&cssToDevPixelScaling),
                            sizeof(cssToDevPixelScaling));
  if (result != ERROR_SUCCESS) {
    printf_stderr(
        "Failed persisting cssToDevPixelScaling to Windows registry\n");
    return;
  }
}

}  // namespace mozilla
