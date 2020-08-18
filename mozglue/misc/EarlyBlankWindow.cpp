/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyBlankWindow.h"

#include <algorithm>

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

struct ColorRect {
  uint32_t color;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

void DrawSkeletonUI(HWND hWnd) {
  if (!sGetSystemMetricsForDpi || !sGetDpiForWindow) {
    return;
  }

  // NOTE: we opt here to paint a pixel buffer for the application chrome by
  // hand, without using native UI library methods. Why do we do this?
  //
  // 1) It gives us a little bit more control, especially if we want to animate
  //    any of this.
  // 2) It's actually more portable. We can do this on any platform where we
  //    can blit a pixel buffer to the screen, and it only has to change
  //    insofar as the UI is different on those platforms (and thus would have
  //    to change anyway.)
  //
  // The performance impact of this ought to be negligible. As far as has been
  // observed, on slow reference hardware this might take up to a millisecond,
  // for a startup which otherwise takes 30 seconds.
  //
  // The readability and maintainability are a greater concern. When the
  // silhouette of Firefox's core UI changes, this code will likely need to
  // change. However, for the foreseeable future, our skeleton UI will be mostly
  // axis-aligned geometric shapes, and the thought is that any code which is
  // manipulating raw pixels should not be *too* hard to maintain and
  // understand so long as it is only painting such simple shapes.

  // found in browser-aero.css ":root[tabsintitlebar]:not(:-moz-lwtheme)"
  // (set to "hsl(235,33%,19%)")
  uint32_t tabBarColor = 0x202340;
  // --toolbar-non-lwt-bgcolor in browser.css
  uint32_t backgroundColor = 0xf9f9fa;
  // --chrome-content-separator-color in browser.css
  uint32_t chromeContentDividerColor = 0xe2e1e3;
  // We define this, but it will need to differ based on theme
  uint32_t toolbarForegroundColor = 0xe5e5e5;
  // controlled by css variable --tab-line-color
  uint32_t tabLineColor = 0x0a75d3;

  int chromeHorMargin = CSSToDevPixels(2, sCSSToDevPixelScaling);
  int dpi = sGetDpiForWindow(sWnd);
  int topOffset = sGetSystemMetricsForDpi(SM_CYBORDER, dpi);
  int nonClientHorMargins = sGetSystemMetricsForDpi(SM_CXFRAME, dpi) +
                            sGetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
  int horizontolOffset = nonClientHorMargins - chromeHorMargin;

  // found in tabs.inc.css, "--tab-min-height" - depends on uidensity variable
  int tabBarHeight = CSSToDevPixels(33, sCSSToDevPixelScaling) + topOffset;
  // found in tabs.inc.css, ".titlebar-spacer"
  int titlebarSpacerWidth =
      CSSToDevPixels(40, sCSSToDevPixelScaling) + horizontolOffset;
  // found in tabs.inc.css, ".tab-line"
  int tabLineHeight = CSSToDevPixels(2, sCSSToDevPixelScaling) + topOffset;
  int selectedTabWidth = CSSToDevPixels(224, sCSSToDevPixelScaling);

  int toolbarHeight = CSSToDevPixels(39, sCSSToDevPixelScaling);

  int tabPlaceholderBarMarginTop = CSSToDevPixels(13, sCSSToDevPixelScaling);
  int tabPlaceholderBarMarginLeft = CSSToDevPixels(10, sCSSToDevPixelScaling);
  int tabPlaceholderBarHeight = CSSToDevPixels(8, sCSSToDevPixelScaling);
  int tabPlaceholderBarWidth = CSSToDevPixels(120, sCSSToDevPixelScaling);

  int toolbarPlaceholderMarginTop = CSSToDevPixels(16, sCSSToDevPixelScaling);
  int toolbarPlaceholderMarginLeft = CSSToDevPixels(9, sCSSToDevPixelScaling);
  int toolbarPlaceholderMarginRight = CSSToDevPixels(11, sCSSToDevPixelScaling);
  int toolbarPlaceholderWidth = CSSToDevPixels(90, sCSSToDevPixelScaling);
  int toolbarPlaceholderHeight = CSSToDevPixels(10, sCSSToDevPixelScaling);

  // The (traditionally dark blue on Windows) background of the tab bar.
  ColorRect tabBar = {};
  tabBar.color = tabBarColor;
  tabBar.x = 0;
  tabBar.y = 0;
  tabBar.width = sWindowWidth;
  tabBar.height = tabBarHeight;

  // The blue highlight at the top of the initial selected tab
  ColorRect tabLine = {};
  tabLine.color = tabLineColor;
  tabLine.x = titlebarSpacerWidth;
  tabLine.y = 0;
  tabLine.width = selectedTabWidth;
  tabLine.height = tabLineHeight;

  // The initial selected tab
  ColorRect selectedTab = {};
  selectedTab.color = backgroundColor;
  selectedTab.x = titlebarSpacerWidth;
  selectedTab.y = tabLineHeight;
  selectedTab.width = selectedTabWidth;
  selectedTab.height = tabBarHeight;

  // A placeholder rect representing text that will fill the selected tab title
  ColorRect tabTextPlaceholder = {};
  tabTextPlaceholder.color = toolbarForegroundColor;
  tabTextPlaceholder.x = selectedTab.x + tabPlaceholderBarMarginLeft;
  tabTextPlaceholder.y = selectedTab.y + tabPlaceholderBarMarginTop;
  tabTextPlaceholder.width = tabPlaceholderBarWidth;
  tabTextPlaceholder.height = tabPlaceholderBarHeight;

  // The toolbar background
  ColorRect toolbar = {};
  toolbar.color = backgroundColor;
  toolbar.x = 0;
  toolbar.y = tabBarHeight;
  toolbar.width = sWindowWidth;
  toolbar.height = toolbarHeight;

  // A placeholder rect representing UI elements that will fill the left part
  // of the toolbar
  ColorRect leftToolbarPlaceholder = {};
  leftToolbarPlaceholder.color = toolbarForegroundColor;
  leftToolbarPlaceholder.x =
      toolbar.x + toolbarPlaceholderMarginLeft + horizontolOffset;
  leftToolbarPlaceholder.y = toolbar.y + toolbarPlaceholderMarginTop;
  leftToolbarPlaceholder.width = toolbarPlaceholderWidth;
  leftToolbarPlaceholder.height = toolbarPlaceholderHeight;

  // A placeholder rect representing UI elements that will fill the right part
  // of the toolbar
  ColorRect rightToolbarPlaceholder = {};
  rightToolbarPlaceholder.color = toolbarForegroundColor;
  rightToolbarPlaceholder.x = sWindowWidth - horizontolOffset -
                              toolbarPlaceholderMarginRight -
                              toolbarPlaceholderWidth;
  rightToolbarPlaceholder.y = toolbar.y + toolbarPlaceholderMarginTop;
  rightToolbarPlaceholder.width = toolbarPlaceholderWidth;
  rightToolbarPlaceholder.height = toolbarPlaceholderHeight;

  // The single-pixel divider line below the toolbar
  ColorRect chromeContentDivider = {};
  chromeContentDivider.color = chromeContentDividerColor;
  chromeContentDivider.x = 0;
  chromeContentDivider.y = toolbar.y + toolbar.height;
  chromeContentDivider.width = sWindowWidth;
  chromeContentDivider.height = 1;

  ColorRect rects[] = {
      tabBar,
      tabLine,
      selectedTab,
      tabTextPlaceholder,
      toolbar,
      leftToolbarPlaceholder,
      rightToolbarPlaceholder,
      chromeContentDivider,
  };

  int totalChromeHeight = chromeContentDivider.y + chromeContentDivider.height;

  uint32_t* pixelBuffer =
      (uint32_t*)calloc(sWindowWidth * totalChromeHeight, sizeof(uint32_t));

  for (int i = 0; i < sizeof(rects) / sizeof(rects[0]); ++i) {
    ColorRect rect = rects[i];
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
      uint32_t* lineStart = &pixelBuffer[y * sWindowWidth];
      uint32_t* dataStart = lineStart + rect.x;
      std::fill(dataStart, dataStart + rect.width, rect.color);
    }
  }

  HDC hdc = ::GetWindowDC(hWnd);

  BITMAPINFO chromeBMI = {};
  chromeBMI.bmiHeader.biSize = sizeof(chromeBMI.bmiHeader);
  chromeBMI.bmiHeader.biWidth = sWindowWidth;
  chromeBMI.bmiHeader.biHeight = -totalChromeHeight;
  chromeBMI.bmiHeader.biPlanes = 1;
  chromeBMI.bmiHeader.biBitCount = 32;
  chromeBMI.bmiHeader.biCompression = BI_RGB;

  // First, we just paint the chrome area with our pixel buffer
  ::StretchDIBits(hdc, 0, 0, sWindowWidth, totalChromeHeight, 0, 0,
                  sWindowWidth, totalChromeHeight, pixelBuffer, &chromeBMI,
                  DIB_RGB_COLORS, SRCCOPY);

  // Then, we just fill the rest with FillRect
  RECT rect = {0, totalChromeHeight, (LONG)sWindowWidth, (LONG)sWindowHeight};
  HBRUSH brush = ::CreateSolidBrush(backgroundColor);
  ::FillRect(hdc, &rect, brush);

  ::ReleaseDC(hWnd, hdc);

  free(pixelBuffer);
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
