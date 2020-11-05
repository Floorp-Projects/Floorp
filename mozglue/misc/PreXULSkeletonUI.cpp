/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreXULSkeletonUI.h"

#include <algorithm>
#include <math.h>
#include <limits.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/glue/Debug.h"
#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WindowsDpiAwareness.h"
#include "mozilla/WindowsVersion.h"

namespace mozilla {

struct ColorRect {
  uint32_t color;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

struct NormalizedRGB {
  double r;
  double g;
  double b;
};

NormalizedRGB UintToRGB(uint32_t color) {
  double r = static_cast<double>(color >> 16 & 0xff) / 255.0;
  double g = static_cast<double>(color >> 8 & 0xff) / 255.0;
  double b = static_cast<double>(color >> 0 & 0xff) / 255.0;
  return NormalizedRGB{r, g, b};
}

uint32_t RGBToUint(const NormalizedRGB& rgb) {
  return (static_cast<uint32_t>(rgb.r * 255.0) << 16) |
         (static_cast<uint32_t>(rgb.g * 255.0) << 8) |
         (static_cast<uint32_t>(rgb.b * 255.0) << 0);
}

double Lerp(double a, double b, double x) { return a + x * (b - a); }

NormalizedRGB Lerp(const NormalizedRGB& a, const NormalizedRGB& b, double x) {
  return NormalizedRGB{Lerp(a.r, b.r, x), Lerp(a.g, b.g, x), Lerp(a.b, b.b, x)};
}

// Produces a smooth curve in [0,1] based on a linear input in [0,1]
double SmoothStep3(double x) { return x * x * (3.0 - 2.0 * x); }

static const wchar_t kPreXULSkeletonUIKeyPath[] =
    L"SOFTWARE"
    L"\\" MOZ_APP_VENDOR L"\\" MOZ_APP_BASENAME L"\\PreXULSkeletonUISettings";

static bool sPreXULSkeletonUIEnabled = false;
static HWND sPreXULSkeletonUIWindow;
static LPWSTR const gStockApplicationIcon = MAKEINTRESOURCEW(32512);
static LPWSTR const gIDCWait = MAKEINTRESOURCEW(32514);
static HANDLE sPreXULSKeletonUIAnimationThread;

static uint32_t* sPixelBuffer = nullptr;
static Vector<ColorRect>* sAnimatedRects = nullptr;
static int sTotalChromeHeight = 0;
static volatile LONG sAnimationControlFlag = 0;
static bool sMaximized = false;
static int sNonClientVerticalMargins = 0;
static int sNonClientHorizontalMargins = 0;
static uint32_t sDpi = 0;

// Color values needed by the animation loop
static uint32_t sBackgroundColor;
static uint32_t sToolbarForegroundColor;

typedef BOOL(WINAPI* EnableNonClientDpiScalingProc)(HWND);
static EnableNonClientDpiScalingProc sEnableNonClientDpiScaling = NULL;
typedef int(WINAPI* GetSystemMetricsForDpiProc)(int, UINT);
GetSystemMetricsForDpiProc sGetSystemMetricsForDpi = NULL;
typedef UINT(WINAPI* GetDpiForWindowProc)(HWND);
GetDpiForWindowProc sGetDpiForWindow = NULL;
typedef ATOM(WINAPI* RegisterClassWProc)(const WNDCLASSW*);
RegisterClassWProc sRegisterClassW = NULL;
typedef HICON(WINAPI* LoadIconWProc)(HINSTANCE, LPCWSTR);
LoadIconWProc sLoadIconW = NULL;
typedef HICON(WINAPI* LoadCursorWProc)(HINSTANCE, LPCWSTR);
LoadCursorWProc sLoadCursorW = NULL;
typedef HWND(WINAPI* CreateWindowExWProc)(DWORD, LPCWSTR, LPCWSTR, DWORD, int,
                                          int, int, int, HWND, HMENU, HINSTANCE,
                                          LPVOID);
CreateWindowExWProc sCreateWindowExW = NULL;
typedef BOOL(WINAPI* ShowWindowProc)(HWND, int);
ShowWindowProc sShowWindow = NULL;
typedef BOOL(WINAPI* SetWindowPosProc)(HWND, HWND, int, int, int, int, UINT);
SetWindowPosProc sSetWindowPos = NULL;
typedef HDC(WINAPI* GetWindowDCProc)(HWND);
GetWindowDCProc sGetWindowDC = NULL;
typedef int(WINAPI* FillRectProc)(HDC, const RECT*, HBRUSH);
FillRectProc sFillRect = NULL;
typedef BOOL(WINAPI* DeleteObjectProc)(HGDIOBJ);
DeleteObjectProc sDeleteObject = NULL;
typedef int(WINAPI* ReleaseDCProc)(HWND, HDC);
ReleaseDCProc sReleaseDC = NULL;
typedef HMONITOR(WINAPI* MonitorFromWindowProc)(HWND, DWORD);
MonitorFromWindowProc sMonitorFromWindow = NULL;
typedef BOOL(WINAPI* GetMonitorInfoWProc)(HMONITOR, LPMONITORINFO);
GetMonitorInfoWProc sGetMonitorInfoW = NULL;
typedef LONG_PTR(WINAPI* SetWindowLongPtrWProc)(HWND, int, LONG_PTR);
SetWindowLongPtrWProc sSetWindowLongPtrW = NULL;
typedef int(WINAPI* StretchDIBitsProc)(HDC, int, int, int, int, int, int, int,
                                       int, const VOID*, const BITMAPINFO*,
                                       UINT, DWORD);
StretchDIBitsProc sStretchDIBits = NULL;
typedef HBRUSH(WINAPI* CreateSolidBrushProc)(COLORREF);
CreateSolidBrushProc sCreateSolidBrush = NULL;

static uint32_t sWindowWidth;
static uint32_t sWindowHeight;
static double sCSSToDevPixelScaling;

static const int kAnimationCSSPixelsPerFrame = 21;
static const int kAnimationCSSExtraWindowSize = 300;

// We could use nsAutoRegKey, but including nsWindowsHelpers.h causes build
// failures in random places because we're in mozglue. Overall it should be
// simpler and cleaner to just step around that issue with this class:
class MOZ_RAII AutoCloseRegKey {
 public:
  explicit AutoCloseRegKey(HKEY key) : mKey(key) {}
  ~AutoCloseRegKey() { ::RegCloseKey(mKey); }

 private:
  HKEY mKey;
};

int CSSToDevPixels(double cssPixels, double scaling) {
  return floor(cssPixels * scaling + 0.5);
}

int CSSToDevPixels(int cssPixels, double scaling) {
  return CSSToDevPixels((double)cssPixels, scaling);
}

void DrawSkeletonUI(HWND hWnd, double urlbarHorizontalOffsetCSS,
                    double urlbarWidthCSS) {
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

  // NOTE: these could be constants, but eventually they won't be, and they will
  // need to be set here.
  // --toolbar-non-lwt-bgcolor in browser.css
  sBackgroundColor = 0xf9f9fa;
  // We define this, but it will need to differ based on theme
  sToolbarForegroundColor = 0xe5e5e5;

  // found in browser-aero.css ":root[tabsintitlebar]:not(:-moz-lwtheme)"
  // (set to "hsl(235,33%,19%)")
  uint32_t tabBarColor = 0x202340;
  // --chrome-content-separator-color in browser.css
  uint32_t chromeContentDividerColor = 0xe2e1e3;
  // controlled by css variable --tab-line-color
  uint32_t tabLineColor = 0x0a75d3;
  // controlled by css variable --toolbar-color
  uint32_t urlbarColor = 0xffffff;

  int chromeHorMargin = CSSToDevPixels(2, sCSSToDevPixelScaling);
  int verticalOffset = sMaximized ? sNonClientVerticalMargins : 0;
  int horizontalOffset =
      sNonClientHorizontalMargins - (sMaximized ? 0 : chromeHorMargin);

  // found in browser-aero.css, ":root[sizemode=normal][tabsintitlebar]"
  int topBorderHeight =
      sMaximized ? 0 : CSSToDevPixels(1, sCSSToDevPixelScaling);
  // found in tabs.inc.css, "--tab-min-height" - depends on uidensity variable
  int tabBarHeight = CSSToDevPixels(33, sCSSToDevPixelScaling) + verticalOffset;
  // found in tabs.inc.css, ".titlebar-spacer"
  int titlebarSpacerWidth =
      (sMaximized ? 0 : CSSToDevPixels(40, sCSSToDevPixelScaling)) +
      horizontalOffset;
  // found in tabs.inc.css, ".tab-line"
  int tabLineHeight = CSSToDevPixels(2, sCSSToDevPixelScaling) + verticalOffset;
  int selectedTabWidth = CSSToDevPixels(224, sCSSToDevPixelScaling);
  int toolbarHeight = CSSToDevPixels(39, sCSSToDevPixelScaling);
  // found in urlbar-searchbar.inc.css, "#urlbar[breakout]"
  int urlbarTopOffset = CSSToDevPixels(5, sCSSToDevPixelScaling);
  int urlbarHeight = CSSToDevPixels(30, sCSSToDevPixelScaling);

  int tabPlaceholderBarMarginTop = CSSToDevPixels(13, sCSSToDevPixelScaling);
  int tabPlaceholderBarMarginLeft = CSSToDevPixels(10, sCSSToDevPixelScaling);
  int tabPlaceholderBarHeight = CSSToDevPixels(8, sCSSToDevPixelScaling);
  int tabPlaceholderBarWidth = CSSToDevPixels(120, sCSSToDevPixelScaling);

  int toolbarPlaceholderMarginTop = CSSToDevPixels(16, sCSSToDevPixelScaling);
  int toolbarPlaceholderMarginLeft = CSSToDevPixels(9, sCSSToDevPixelScaling);
  int toolbarPlaceholderMarginRight = CSSToDevPixels(11, sCSSToDevPixelScaling);
  int toolbarPlaceholderWidth = CSSToDevPixels(90, sCSSToDevPixelScaling);
  int toolbarPlaceholderHeight = CSSToDevPixels(10, sCSSToDevPixelScaling);

  int urlbarTextPlaceholderMarginTop =
      CSSToDevPixels(10, sCSSToDevPixelScaling);
  int urlbarTextPlaceholderMarginLeft =
      CSSToDevPixels(10, sCSSToDevPixelScaling);
  int urlbarTextPlaceHolderWidth = CSSToDevPixels(
      std::min((int)urlbarWidthCSS - 10, 260), sCSSToDevPixelScaling);
  int urlbarTextPlaceholderHeight = CSSToDevPixels(10, sCSSToDevPixelScaling);

  ColorRect topBorder = {};
  topBorder.color = 0x00000000;
  topBorder.x = 0;
  topBorder.y = 0;
  topBorder.width = sWindowWidth;
  topBorder.height = topBorderHeight;

  // The (traditionally dark blue on Windows) background of the tab bar.
  ColorRect tabBar = {};
  tabBar.color = tabBarColor;
  tabBar.x = 0;
  tabBar.y = topBorder.height;
  tabBar.width = sWindowWidth;
  tabBar.height = tabBarHeight;

  // The blue highlight at the top of the initial selected tab
  ColorRect tabLine = {};
  tabLine.color = tabLineColor;
  tabLine.x = titlebarSpacerWidth;
  tabLine.y = topBorder.height;
  tabLine.width = selectedTabWidth;
  tabLine.height = tabLineHeight;

  // The initial selected tab
  ColorRect selectedTab = {};
  selectedTab.color = sBackgroundColor;
  selectedTab.x = titlebarSpacerWidth;
  selectedTab.y = tabLine.y + tabLineHeight;
  selectedTab.width = selectedTabWidth;
  selectedTab.height = tabBar.y + tabBar.height - selectedTab.y;

  // A placeholder rect representing text that will fill the selected tab title
  ColorRect tabTextPlaceholder = {};
  tabTextPlaceholder.color = sToolbarForegroundColor;
  tabTextPlaceholder.x = selectedTab.x + tabPlaceholderBarMarginLeft;
  tabTextPlaceholder.y = selectedTab.y + tabPlaceholderBarMarginTop;
  tabTextPlaceholder.width = tabPlaceholderBarWidth;
  tabTextPlaceholder.height = tabPlaceholderBarHeight;

  // The toolbar background
  ColorRect toolbar = {};
  toolbar.color = sBackgroundColor;
  toolbar.x = 0;
  toolbar.y = tabBar.y + tabBarHeight;
  toolbar.width = sWindowWidth;
  toolbar.height = toolbarHeight;

  // A placeholder rect representing UI elements that will fill the left part
  // of the toolbar
  ColorRect leftToolbarPlaceholder = {};
  leftToolbarPlaceholder.color = sToolbarForegroundColor;
  leftToolbarPlaceholder.x =
      toolbar.x + toolbarPlaceholderMarginLeft + horizontalOffset;
  leftToolbarPlaceholder.y = toolbar.y + toolbarPlaceholderMarginTop;
  leftToolbarPlaceholder.width = toolbarPlaceholderWidth;
  leftToolbarPlaceholder.height = toolbarPlaceholderHeight;

  // A placeholder rect representing UI elements that will fill the right part
  // of the toolbar
  ColorRect rightToolbarPlaceholder = {};
  rightToolbarPlaceholder.color = sToolbarForegroundColor;
  rightToolbarPlaceholder.x = sWindowWidth - horizontalOffset -
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

  // The urlbar
  ColorRect urlbar = {};
  urlbar.color = urlbarColor;
  urlbar.x = CSSToDevPixels(urlbarHorizontalOffsetCSS, sCSSToDevPixelScaling) +
             horizontalOffset;
  urlbar.y = tabBar.y + tabBarHeight + urlbarTopOffset;
  urlbar.width = CSSToDevPixels(urlbarWidthCSS, sCSSToDevPixelScaling);
  urlbar.height = urlbarHeight;

  // The urlbar placeholder rect representating text that will fill the urlbar
  // The placeholder rects should all be y-aligned.
  ColorRect urlbarTextPlaceholder = {};
  urlbarTextPlaceholder.color = sToolbarForegroundColor;
  urlbarTextPlaceholder.x = urlbar.x + urlbarTextPlaceholderMarginLeft;
  // This is equivalent to rightToolbarPlaceholder.y and
  // leftToolbarPlaceholder.y
  urlbarTextPlaceholder.y = urlbar.y + urlbarTextPlaceholderMarginTop;
  urlbarTextPlaceholder.width = urlbarTextPlaceHolderWidth;
  urlbarTextPlaceholder.height = urlbarTextPlaceholderHeight;

  ColorRect rects[] = {
      topBorder,
      tabBar,
      tabLine,
      selectedTab,
      tabTextPlaceholder,
      toolbar,
      leftToolbarPlaceholder,
      rightToolbarPlaceholder,
      chromeContentDivider,
      urlbar,
      urlbarTextPlaceholder,
  };

  sTotalChromeHeight = chromeContentDivider.y + chromeContentDivider.height;
  if (sTotalChromeHeight > sWindowHeight) {
    printf_stderr("Exiting drawing skeleton UI because window is too small.\n");
    return;
  }

  if (!sAnimatedRects->append(tabTextPlaceholder) ||
      !sAnimatedRects->append(leftToolbarPlaceholder) ||
      !sAnimatedRects->append(rightToolbarPlaceholder) ||
      !sAnimatedRects->append(urlbarTextPlaceholder)) {
    delete sAnimatedRects;
    sAnimatedRects = nullptr;
    return;
  }

  sPixelBuffer =
      (uint32_t*)calloc(sWindowWidth * sTotalChromeHeight, sizeof(uint32_t));

  for (int i = 0; i < sizeof(rects) / sizeof(rects[0]); ++i) {
    ColorRect rect = rects[i];
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
      uint32_t* lineStart = &sPixelBuffer[y * sWindowWidth];
      uint32_t* dataStart = lineStart + rect.x;
      std::fill_n(dataStart, rect.width, rect.color);
    }
  }

  HDC hdc = sGetWindowDC(hWnd);

  BITMAPINFO chromeBMI = {};
  chromeBMI.bmiHeader.biSize = sizeof(chromeBMI.bmiHeader);
  chromeBMI.bmiHeader.biWidth = sWindowWidth;
  chromeBMI.bmiHeader.biHeight = -sTotalChromeHeight;
  chromeBMI.bmiHeader.biPlanes = 1;
  chromeBMI.bmiHeader.biBitCount = 32;
  chromeBMI.bmiHeader.biCompression = BI_RGB;

  // First, we just paint the chrome area with our pixel buffer
  sStretchDIBits(hdc, 0, 0, sWindowWidth, sTotalChromeHeight, 0, 0,
                 sWindowWidth, sTotalChromeHeight, sPixelBuffer, &chromeBMI,
                 DIB_RGB_COLORS, SRCCOPY);

  // Then, we just fill the rest with FillRect
  RECT rect = {0, sTotalChromeHeight, (LONG)sWindowWidth, (LONG)sWindowHeight};
  HBRUSH brush = sCreateSolidBrush(sBackgroundColor);
  sFillRect(hdc, &rect, brush);

  sReleaseDC(hWnd, hdc);
  sDeleteObject(brush);
}

DWORD WINAPI AnimateSkeletonUI(void* aUnused) {
  if (!sPixelBuffer || sAnimatedRects->empty()) {
    return 0;
  }

  // On each of the animated rects (which happen to all be placeholder UI
  // rects sharing the same color), we want to animate a gradient moving across
  // the screen from left to right. The gradient starts as the rect's color on,
  // the left side, changes to the background color of the window by the middle
  // of the gradient, and then goes back down to the rect's color. To make this
  // faster than interpolating between the two colors for each pixel for each
  // frame, we simply create a lookup buffer in which we can look up the color
  // for a particular offset into the gradient.
  //
  // To do this we just interpolate between the two values, and to give the
  // gradient a smoother transition between colors, we transform the linear
  // blend amount via the cubic smooth step function (SmoothStep3) to produce
  // a smooth start and stop for the gradient. We do this for the first half
  // of the gradient, and then simply copy that backwards for the second half.
  //
  // The CSS width of 80 chosen here is effectively is just to match the size
  // of the animation provided in the design mockup. We define it in CSS pixels
  // simply because the rest of our UI is based off of CSS scalings.
  int animationWidth = CSSToDevPixels(80, sCSSToDevPixelScaling);
  UniquePtr<uint32_t[]> animationLookup =
      MakeUnique<uint32_t[]>(animationWidth);
  uint32_t animationColor = sBackgroundColor;
  NormalizedRGB rgbBlend = UintToRGB(animationColor);

  // Build the first half of the lookup table
  for (int i = 0; i < animationWidth / 2; ++i) {
    uint32_t baseColor = sToolbarForegroundColor;
    double blendAmountLinear =
        static_cast<double>(i) / (static_cast<double>(animationWidth / 2));
    double blendAmount = SmoothStep3(blendAmountLinear);

    NormalizedRGB rgbBase = UintToRGB(baseColor);
    NormalizedRGB rgb = Lerp(rgbBase, rgbBlend, blendAmount);
    animationLookup[i] = RGBToUint(rgb);
  }

  // Copy the first half of the lookup table into the second half backwards
  for (int i = animationWidth / 2; i < animationWidth; ++i) {
    int j = animationWidth - 1 - i;
    if (j == animationWidth / 2) {
      // If animationWidth is odd, we'll be left with one pixel at the center.
      // Just color that as the animation color.
      animationLookup[i] = animationColor;
    } else {
      animationLookup[i] = animationLookup[j];
    }
  }

  // The bitmap info remains unchanged throughout the animation - this just
  // effectively describes the contents of sPixelBuffer
  BITMAPINFO chromeBMI = {};
  chromeBMI.bmiHeader.biSize = sizeof(chromeBMI.bmiHeader);
  chromeBMI.bmiHeader.biWidth = sWindowWidth;
  chromeBMI.bmiHeader.biHeight = -sTotalChromeHeight;
  chromeBMI.bmiHeader.biPlanes = 1;
  chromeBMI.bmiHeader.biBitCount = 32;
  chromeBMI.bmiHeader.biCompression = BI_RGB;

  uint32_t animationIteration = 0;

  int devPixelsPerFrame =
      CSSToDevPixels(kAnimationCSSPixelsPerFrame, sCSSToDevPixelScaling);
  int devPixelsExtraWindowSize =
      CSSToDevPixels(kAnimationCSSExtraWindowSize, sCSSToDevPixelScaling);

  if (::InterlockedCompareExchange(&sAnimationControlFlag, 0, 0)) {
    // The window got consumed before we were able to draw anything.
    return 0;
  }

  while (true) {
    // The gradient will move across the screen at devPixelsPerFrame at
    // 60fps, and then loop back to the beginning. However, we add a buffer of
    // devPixelsExtraWindowSize around the edges so it doesn't immediately
    // jump back, giving it a more pulsing feel.
    int animationMin = ((animationIteration * devPixelsPerFrame) %
                        (sWindowWidth + devPixelsExtraWindowSize)) -
                       devPixelsExtraWindowSize / 2;
    int animationMax = animationMin + animationWidth;
    // The priorAnimationMin is the beginning of the previous frame's animation.
    // Since we only want to draw the bits of the image that we updated, we need
    // to overwrite the left bit of the animation we drew last frame with the
    // default color.
    int priorAnimationMin = animationMin - devPixelsPerFrame;
    animationMin = std::max(0, animationMin);
    priorAnimationMin = std::max(0, priorAnimationMin);
    animationMax = std::min((int)sWindowWidth, animationMax);

    // The gradient only affects the specific rects that we put into
    // sAnimatedRects. So we simply update those rects, and maintain a flag
    // to avoid drawing when we don't need to.
    bool updatedAnything = false;
    for (ColorRect rect : *sAnimatedRects) {
      int rectMin = rect.x;
      int rectMax = rect.x + rect.width;
      bool animationWindowOverlaps =
          rectMax >= priorAnimationMin && rectMin < animationMax;

      int priorUpdateAreaMin = std::max(rectMin, priorAnimationMin);
      int currentUpdateAreaMin = std::max(rectMin, animationMin);
      int priorUpdateAreaMax = std::min(rectMax, animationMin);
      int currentUpdateAreaMax = std::min(rectMax, animationMax);

      if (animationWindowOverlaps) {
        updatedAnything = true;
        for (int y = rect.y; y < rect.y + rect.height; ++y) {
          uint32_t* lineStart = &sPixelBuffer[y * sWindowWidth];
          // Overwrite the tail end of last frame's animation with the rect's
          // normal, unanimated color.
          for (int x = priorUpdateAreaMin; x < priorUpdateAreaMax; ++x) {
            lineStart[x] = rect.color;
          }
          // Then apply the animated color
          for (int x = currentUpdateAreaMin; x < currentUpdateAreaMax; ++x) {
            lineStart[x] = animationLookup[x - animationMin];
          }
        }
      }
    }

    if (updatedAnything) {
      HDC hdc = sGetWindowDC(sPreXULSkeletonUIWindow);

      sStretchDIBits(hdc, priorAnimationMin, 0,
                     animationMax - priorAnimationMin, sTotalChromeHeight,
                     priorAnimationMin, 0, animationMax - priorAnimationMin,
                     sTotalChromeHeight, sPixelBuffer, &chromeBMI,
                     DIB_RGB_COLORS, SRCCOPY);

      sReleaseDC(sPreXULSkeletonUIWindow, hdc);
    }

    animationIteration++;

    // We coordinate around our sleep here to ensure that the main thread does
    // not wait on us if we're sleeping. If we don't get 1 here, it means the
    // window has been consumed and we don't need to sleep. If in
    // ConsumePreXULSkeletonUIHandle we get a value other than 1 after
    // incrementing, it means we're sleeping, and that function can assume that
    // we will safely exit after the sleep because of the observed value of
    // sAnimationControlFlag.
    if (InterlockedIncrement(&sAnimationControlFlag) != 1) {
      return 0;
    }

    // Note: Sleep does not guarantee an exact time interval. If the system is
    // busy, for instance, we could easily end up taking several frames longer,
    // and really we could be left unscheduled for an arbitrarily long time.
    // This is fine, and we don't really care. We could track how much time this
    // actually took and jump the animation forward the appropriate amount, but
    // its not even clear that that's a better user experience. So we leave this
    // as simple as we can.
    ::Sleep(16);

    // Here we bring sAnimationControlFlag back down - again, if we don't get a
    // 0 here it means we consumed the skeleton UI window in the mean time, so
    // we can simply exit.
    if (InterlockedDecrement(&sAnimationControlFlag) != 0) {
      return 0;
    }
  }

  return 0;
}

LRESULT WINAPI PreXULSkeletonUIProc(HWND hWnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  // NOTE: this block was copied from WinUtils.cpp, and needs to be kept in
  // sync.
  if (msg == WM_NCCREATE && sEnableNonClientDpiScaling) {
    sEnableNonClientDpiScaling(hWnd);
  }

  // NOTE: this block was paraphrased from the WM_NCCALCSIZE handler in
  // nsWindow.cpp, and will need to be kept in sync.
  if (msg == WM_NCCALCSIZE) {
    RECT* clientRect =
        wParam ? &(reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam))->rgrc[0]
               : (reinterpret_cast<RECT*>(lParam));

    // These match the margins set in browser-tabsintitlebar.js with
    // default prefs on Windows. Bug 1673092 tracks lining this up with
    // that more correctly instead of hard-coding it.
    int horizontalOffset =
        sNonClientHorizontalMargins -
        (sMaximized ? 0 : CSSToDevPixels(2, sCSSToDevPixelScaling));
    int verticalOffset =
        sNonClientHorizontalMargins -
        (sMaximized ? 0 : CSSToDevPixels(2, sCSSToDevPixelScaling));
    clientRect->top = clientRect->top;
    clientRect->left += horizontalOffset;
    clientRect->right -= horizontalOffset;
    clientRect->bottom -= verticalOffset;
    return 0;
  }

  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool OpenPreXULSkeletonUIRegKey(HKEY& key) {
  DWORD disposition;
  LSTATUS result =
      ::RegCreateKeyExW(HKEY_CURRENT_USER, kPreXULSkeletonUIKeyPath, 0, nullptr,
                        0, KEY_ALL_ACCESS, nullptr, &key, &disposition);

  if (result != ERROR_SUCCESS) {
    return false;
  }

  if (disposition == REG_CREATED_NEW_KEY) {
    return false;
  }

  if (disposition == REG_OPENED_EXISTING_KEY) {
    return true;
  }

  ::RegCloseKey(key);
  return false;
}

bool LoadGdi32AndUser32Procedures() {
  HMODULE user32Dll = ::LoadLibraryW(L"user32");
  HMODULE gdi32Dll = ::LoadLibraryW(L"gdi32");

  if (!user32Dll || !gdi32Dll) {
    return false;
  }

  auto getThreadDpiAwarenessContext =
      (decltype(GetThreadDpiAwarenessContext)*)::GetProcAddress(
          user32Dll, "GetThreadDpiAwarenessContext");
  auto areDpiAwarenessContextsEqual =
      (decltype(AreDpiAwarenessContextsEqual)*)::GetProcAddress(
          user32Dll, "AreDpiAwarenessContextsEqual");
  if (getThreadDpiAwarenessContext && areDpiAwarenessContextsEqual &&
      areDpiAwarenessContextsEqual(getThreadDpiAwarenessContext(),
                                   DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
    // EnableNonClientDpiScaling is optional - we can handle not having it.
    sEnableNonClientDpiScaling =
        (EnableNonClientDpiScalingProc)::GetProcAddress(
            user32Dll, "EnableNonClientDpiScaling");
  }

  sGetSystemMetricsForDpi = (GetSystemMetricsForDpiProc)::GetProcAddress(
      user32Dll, "GetSystemMetricsForDpi");
  if (!sGetSystemMetricsForDpi) {
    return false;
  }
  sGetDpiForWindow =
      (GetDpiForWindowProc)::GetProcAddress(user32Dll, "GetDpiForWindow");
  if (!sGetDpiForWindow) {
    return false;
  }
  sRegisterClassW =
      (RegisterClassWProc)::GetProcAddress(user32Dll, "RegisterClassW");
  if (!sRegisterClassW) {
    return false;
  }
  sCreateWindowExW =
      (CreateWindowExWProc)::GetProcAddress(user32Dll, "CreateWindowExW");
  if (!sCreateWindowExW) {
    return false;
  }
  sShowWindow = (ShowWindowProc)::GetProcAddress(user32Dll, "ShowWindow");
  if (!sShowWindow) {
    return false;
  }
  sSetWindowPos = (SetWindowPosProc)::GetProcAddress(user32Dll, "SetWindowPos");
  if (!sSetWindowPos) {
    return false;
  }
  sGetWindowDC = (GetWindowDCProc)::GetProcAddress(user32Dll, "GetWindowDC");
  if (!sGetWindowDC) {
    return false;
  }
  sFillRect = (FillRectProc)::GetProcAddress(user32Dll, "FillRect");
  if (!sFillRect) {
    return false;
  }
  sReleaseDC = (ReleaseDCProc)::GetProcAddress(user32Dll, "ReleaseDC");
  if (!sReleaseDC) {
    return false;
  }
  sLoadIconW = (LoadIconWProc)::GetProcAddress(user32Dll, "LoadIconW");
  if (!sLoadIconW) {
    return false;
  }
  sLoadCursorW = (LoadCursorWProc)::GetProcAddress(user32Dll, "LoadCursorW");
  if (!sLoadCursorW) {
    return false;
  }
  sMonitorFromWindow =
      (MonitorFromWindowProc)::GetProcAddress(user32Dll, "MonitorFromWindow");
  if (!sMonitorFromWindow) {
    return false;
  }
  sGetMonitorInfoW =
      (GetMonitorInfoWProc)::GetProcAddress(user32Dll, "GetMonitorInfoW");
  if (!sGetMonitorInfoW) {
    return false;
  }
  sSetWindowLongPtrW =
      (SetWindowLongPtrWProc)::GetProcAddress(user32Dll, "SetWindowLongPtrW");
  if (!sSetWindowLongPtrW) {
    return false;
  }
  sStretchDIBits =
      (StretchDIBitsProc)::GetProcAddress(gdi32Dll, "StretchDIBits");
  if (!sStretchDIBits) {
    return false;
  }
  sCreateSolidBrush =
      (CreateSolidBrushProc)::GetProcAddress(gdi32Dll, "CreateSolidBrush");
  if (!sCreateSolidBrush) {
    return false;
  }
  sDeleteObject = (DeleteObjectProc)::GetProcAddress(gdi32Dll, "DeleteObject");
  if (!sDeleteObject) {
    return false;
  }

  return true;
}

void CreateAndStorePreXULSkeletonUI(HINSTANCE hInstance) {
#ifdef MOZ_GECKO_PROFILER
  const TimeStamp skeletonStart = TimeStamp::NowUnfuzzed();
#endif

  HKEY regKey;
  if (!IsWin10OrLater() || !OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);

  DWORD dataLen = sizeof(uint32_t);
  uint32_t enabled;
  LSTATUS result =
      ::RegGetValueW(regKey, nullptr, L"enabled", RRF_RT_REG_DWORD, nullptr,
                     reinterpret_cast<PBYTE>(&enabled), &dataLen);
  if (result != ERROR_SUCCESS || enabled == 0) {
    return;
  }
  sPreXULSkeletonUIEnabled = true;

  MOZ_ASSERT(!sAnimatedRects);
  sAnimatedRects = new Vector<ColorRect>();

  if (!LoadGdi32AndUser32Procedures()) {
    return;
  }

  WNDCLASSW wc;
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = PreXULSkeletonUIProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = sLoadIconW(::GetModuleHandleW(nullptr), gStockApplicationIcon);
  wc.hCursor = sLoadCursorW(hInstance, gIDCWait);
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;

  // TODO: just ensure we disable this if we've overridden the window class
  wc.lpszClassName = L"MozillaWindowClass";

  if (!sRegisterClassW(&wc)) {
    printf_stderr("RegisterClassW error %lu\n", GetLastError());
    return;
  }

  uint32_t screenX;
  result = ::RegGetValueW(regKey, nullptr, L"screenX", RRF_RT_REG_DWORD,
                          nullptr, reinterpret_cast<PBYTE>(&screenX), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading screenX %lu\n", GetLastError());
    return;
  }

  uint32_t screenY;
  result = ::RegGetValueW(regKey, nullptr, L"screenY", RRF_RT_REG_DWORD,
                          nullptr, reinterpret_cast<PBYTE>(&screenY), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading screenY %lu\n", GetLastError());
    return;
  }

  uint32_t windowWidth;
  result = ::RegGetValueW(regKey, nullptr, L"width", RRF_RT_REG_DWORD, nullptr,
                          reinterpret_cast<PBYTE>(&windowWidth), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading width %lu\n", GetLastError());
    return;
  }

  uint32_t windowHeight;
  result = ::RegGetValueW(regKey, nullptr, L"height", RRF_RT_REG_DWORD, nullptr,
                          reinterpret_cast<PBYTE>(&windowHeight), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading height %lu\n", GetLastError());
    return;
  }

  uint32_t maximized;
  result =
      ::RegGetValueW(regKey, nullptr, L"maximized", RRF_RT_REG_DWORD, nullptr,
                     reinterpret_cast<PBYTE>(&maximized), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading maximized %lu\n", GetLastError());
    return;
  }
  sMaximized = maximized != 0;

  dataLen = sizeof(double);
  double urlbarHorizontalOffsetCSS;
  result = ::RegGetValueW(
      regKey, nullptr, L"urlbarHorizontalOffsetCSS", RRF_RT_REG_BINARY, nullptr,
      reinterpret_cast<PBYTE>(&urlbarHorizontalOffsetCSS), &dataLen);
  if (result != ERROR_SUCCESS || dataLen != sizeof(double)) {
    printf_stderr("Error reading urlbarHorizontalOffsetCSS %lu\n",
                  GetLastError());
    return;
  }

  double urlbarWidthCSS;
  result = ::RegGetValueW(regKey, nullptr, L"urlbarWidthCSS", RRF_RT_REG_BINARY,
                          nullptr, reinterpret_cast<PBYTE>(&urlbarWidthCSS),
                          &dataLen);
  if (result != ERROR_SUCCESS || dataLen != sizeof(double)) {
    printf_stderr("Error reading urlbarWidthCSS %lu\n", GetLastError());
    return;
  }

  result = ::RegGetValueW(
      regKey, nullptr, L"cssToDevPixelScaling", RRF_RT_REG_BINARY, nullptr,
      reinterpret_cast<PBYTE>(&sCSSToDevPixelScaling), &dataLen);
  if (result != ERROR_SUCCESS || dataLen != sizeof(double)) {
    printf_stderr("Error reading cssToDevPixelScaling %lu\n", GetLastError());
    return;
  }

  int showCmd = SW_SHOWNORMAL;
  DWORD windowStyle = kPreXULSkeletonUIWindowStyle;
  if (sMaximized) {
    showCmd = SW_SHOWMAXIMIZED;
    windowStyle |= WS_MAXIMIZE;
  }

  sPreXULSkeletonUIWindow =
      sCreateWindowExW(kPreXULSkeletonUIWindowStyleEx, L"MozillaWindowClass",
                       L"", windowStyle, screenX, screenY, windowWidth,
                       windowHeight, nullptr, nullptr, hInstance, nullptr);
  sShowWindow(sPreXULSkeletonUIWindow, showCmd);

  sDpi = sGetDpiForWindow(sPreXULSkeletonUIWindow);
  sNonClientHorizontalMargins =
      sGetSystemMetricsForDpi(SM_CXFRAME, sDpi) +
      sGetSystemMetricsForDpi(SM_CXPADDEDBORDER, sDpi);
  sNonClientVerticalMargins = sGetSystemMetricsForDpi(SM_CYFRAME, sDpi) +
                              sGetSystemMetricsForDpi(SM_CXPADDEDBORDER, sDpi);

  if (sMaximized) {
    HMONITOR monitor =
        sMonitorFromWindow(sPreXULSkeletonUIWindow, MONITOR_DEFAULTTONULL);
    if (!monitor) {
      // NOTE: we specifically don't clean up the window here. If we're unable
      // to finish setting up the window how we want it, we still need to keep
      // it around and consume it with the first real toplevel window we
      // create, to avoid flickering.
      return;
    }
    MONITORINFO mi = {sizeof(MONITORINFO)};
    if (!sGetMonitorInfoW(monitor, &mi)) {
      return;
    }

    sWindowWidth =
        mi.rcWork.right - mi.rcWork.left + sNonClientHorizontalMargins * 2;
    sWindowHeight =
        mi.rcWork.bottom - mi.rcWork.top + sNonClientVerticalMargins * 2;
  } else {
    sWindowWidth = windowWidth;
    sWindowHeight = windowHeight;
  }

  sSetWindowPos(sPreXULSkeletonUIWindow, 0, 0, 0, 0, 0,
                SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
                    SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
  DrawSkeletonUI(sPreXULSkeletonUIWindow, urlbarHorizontalOffsetCSS,
                 urlbarWidthCSS);

  if (sAnimatedRects) {
    sPreXULSKeletonUIAnimationThread = ::CreateThread(
        nullptr, 256 * 1024, AnimateSkeletonUI, nullptr, 0, nullptr);
  }

  BASE_PROFILER_MARKER_UNTYPED(
      "CreatePreXULSkeletonUI", OTHER,
      MarkerTiming::IntervalUntilNowFrom(skeletonStart));
}

bool WasPreXULSkeletonUIMaximized() { return sMaximized; }

HWND ConsumePreXULSkeletonUIHandle() {
  // NOTE: we need to make sure that everything that runs here is a no-op if
  // it failed to be set, which is a possibility. If anything fails to be set
  // we don't want to clean everything up right away, because if we have a
  // blank window up, we want that to stick around and get consumed by nsWindow
  // as normal, otherwise the window will flicker in and out, which we imagine
  // is unpleasant.

  // If we don't get 1 here, it means the thread is actually just sleeping, so
  // we don't need to worry about giving out ownership of the window, because
  // the thread will simply exit after its sleep. However, if it is 1, we need
  // to wait for the thread to exit to be safe, as it could be doing anything.
  if (InterlockedIncrement(&sAnimationControlFlag) == 1) {
    ::WaitForSingleObject(sPreXULSKeletonUIAnimationThread, INFINITE);
  }
  ::CloseHandle(sPreXULSKeletonUIAnimationThread);
  sPreXULSKeletonUIAnimationThread = nullptr;
  HWND result = sPreXULSkeletonUIWindow;
  sPreXULSkeletonUIWindow = nullptr;
  free(sPixelBuffer);
  sPixelBuffer = nullptr;
  delete sAnimatedRects;
  sAnimatedRects = nullptr;
  return result;
}

void PersistPreXULSkeletonUIValues(int screenX, int screenY, int width,
                                   int height, bool maximized,
                                   double urlbarHorizontalOffsetCSS,
                                   double urlbarWidthCSS,
                                   double cssToDevPixelScaling) {
  if (!sPreXULSkeletonUIEnabled) {
    return;
  }

  HKEY regKey;
  if (!OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);

  LSTATUS result;
  result = ::RegSetValueExW(regKey, L"screenX", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&screenX), sizeof(screenX));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting screenX to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(regKey, L"screenY", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&screenY), sizeof(screenY));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting screenY to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(regKey, L"width", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&width), sizeof(width));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting width to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(regKey, L"height", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&height), sizeof(height));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting height to Windows registry\n");
    return;
  }

  DWORD maximizedDword = maximized ? 1 : 0;
  result = ::RegSetValueExW(regKey, L"maximized", 0, REG_DWORD,
                            reinterpret_cast<PBYTE>(&maximizedDword),
                            sizeof(maximizedDword));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting maximized to Windows registry\n");
  }

  result = ::RegSetValueExW(regKey, L"urlbarHorizontalOffsetCSS", 0, REG_BINARY,
                            reinterpret_cast<PBYTE>(&urlbarHorizontalOffsetCSS),
                            sizeof(urlbarHorizontalOffsetCSS));
  if (result != ERROR_SUCCESS) {
    printf_stderr(
        "Failed persisting urlbarHorizontalOffsetCSS to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(regKey, L"urlbarWidthCSS", 0, REG_BINARY,
                            reinterpret_cast<PBYTE>(&urlbarWidthCSS),
                            sizeof(urlbarWidthCSS));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting urlbarWidthCSS to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(regKey, L"cssToDevPixelScaling", 0, REG_BINARY,
                            reinterpret_cast<PBYTE>(&cssToDevPixelScaling),
                            sizeof(cssToDevPixelScaling));
  if (result != ERROR_SUCCESS) {
    printf_stderr(
        "Failed persisting cssToDevPixelScaling to Windows registry\n");
    return;
  }
}

MFBT_API bool GetPreXULSkeletonUIEnabled() { return sPreXULSkeletonUIEnabled; }

MFBT_API void SetPreXULSkeletonUIEnabled(bool value) {
  HKEY regKey;
  if (!OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);
  DWORD enabled = value;
  LSTATUS result =
      ::RegSetValueExW(regKey, L"enabled", 0, REG_DWORD,
                       reinterpret_cast<PBYTE>(&enabled), sizeof(enabled));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting enabled to Windows registry\n");
    return;
  }

  sPreXULSkeletonUIEnabled = true;
}

MFBT_API void PollPreXULSkeletonUIEvents() {
  if (sPreXULSkeletonUIEnabled && sPreXULSkeletonUIWindow) {
    MSG outMsg = {};
    PeekMessageW(&outMsg, sPreXULSkeletonUIWindow, 0, 0, 0);
  }
}

}  // namespace mozilla
