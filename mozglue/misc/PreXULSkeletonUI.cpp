/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreXULSkeletonUI.h"

#include <algorithm>
#include <math.h>
#include <limits.h>
#include <cmath>
#include <locale>
#include <string>
#include <objbase.h>
#include <shlobj.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/glue/Debug.h"
#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/WindowsDpiAwareness.h"
#include "mozilla/WindowsVersion.h"

namespace mozilla {

// ColorRect defines an optionally-rounded, optionally-bordered rectangle of a
// particular color that we will draw.
struct ColorRect {
  uint32_t color;
  uint32_t borderColor;
  int x;
  int y;
  int width;
  int height;
  int borderWidth;
  int borderRadius;
};

// DrawRect is mostly the same as ColorRect, but exists as an implementation
// detail to simplify drawing borders. We draw borders as a strokeOnly rect
// underneath an inner rect of a particular color. We also need to keep
// track of the backgroundColor for rounding rects, in order to correctly
// anti-alias.
struct DrawRect {
  uint32_t color;
  uint32_t backgroundColor;
  int x;
  int y;
  int width;
  int height;
  int borderRadius;
  int borderWidth;
  bool strokeOnly;
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
// sPreXULSkeletonUIDisallowed means that we don't even have the capacity to
// enable the skeleton UI, whether because we're on a platform that doesn't
// support it or because we launched with command line arguments that we don't
// support. Some of these situations are transient, so we want to make sure we
// don't mess with registry values in these scenarios that we may use in
// other scenarios in which the skeleton UI is actually enabled.
static bool sPreXULSkeletonUIDisallowed = false;
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
static uint32_t sAnimationColor;
static uint32_t sToolbarForegroundColor;

static ThemeMode sTheme = ThemeMode::Invalid;

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

static int sWindowWidth;
static int sWindowHeight;
static double sCSSToDevPixelScaling;

static const int kAnimationCSSPixelsPerFrame = 21;
static const int kAnimationCSSExtraWindowSize = 300;

// NOTE: these values were pulled out of thin air as round numbers that are
// likely to be too big to be seen in practice. If we legitimately see windows
// this big, we probably don't want to be drawing them on the CPU anyway.
static const uint32_t kMaxWindowWidth = 1 << 16;
static const uint32_t kMaxWindowHeight = 1 << 16;

static const wchar_t* sEnabledRegSuffix = L"|Enabled";
static const wchar_t* sScreenXRegSuffix = L"|ScreenX";
static const wchar_t* sScreenYRegSuffix = L"|ScreenY";
static const wchar_t* sWidthRegSuffix = L"|Width";
static const wchar_t* sHeightRegSuffix = L"|Height";
static const wchar_t* sMaximizedRegSuffix = L"|Maximized";
static const wchar_t* sUrlbarCSSRegSuffix = L"|UrlbarCSSSpan";
static const wchar_t* sCssToDevPixelScalingRegSuffix = L"|CssToDevPixelScaling";
static const wchar_t* sSearchbarRegSuffix = L"|SearchbarCSSSpan";
static const wchar_t* sSpringsCSSRegSuffix = L"|SpringsCSSSpan";
static const wchar_t* sThemeRegSuffix = L"|Theme";
static const wchar_t* sMenubarShownRegSuffix = L"|MenubarShown";

struct LoadedCoTaskMemFreeDeleter {
  void operator()(void* ptr) {
    static decltype(CoTaskMemFree)* coTaskMemFree = nullptr;
    if (!coTaskMemFree) {
      // Just let this get cleaned up when the process is terminated, because
      // we're going to load it anyway elsewhere.
      HMODULE ole32Dll = ::LoadLibraryW(L"ole32");
      if (!ole32Dll) {
        printf_stderr(
            "Could not load ole32 - will not free with CoTaskMemFree");
        return;
      }
      coTaskMemFree = reinterpret_cast<decltype(coTaskMemFree)>(
          ::GetProcAddress(ole32Dll, "CoTaskMemFree"));
      if (!coTaskMemFree) {
        printf_stderr("Could not find CoTaskMemFree");
        return;
      }
    }
    coTaskMemFree(ptr);
  }
};

std::wstring GetRegValueName(const wchar_t* prefix, const wchar_t* suffix) {
  std::wstring result(prefix);
  result.append(suffix);
  return result;
}

// This is paraphrased from WinHeaderOnlyUtils.h. The fact that this file is
// included in standalone SpiderMonkey builds prohibits us from including that
// file directly, and it hardly warrants its own header. Bug 1674920 tracks
// only including this file for gecko-related builds.
UniquePtr<wchar_t[]> GetBinaryPath() {
  DWORD bufLen = MAX_PATH;
  UniquePtr<wchar_t[]> buf;
  while (true) {
    buf = MakeUnique<wchar_t[]>(bufLen);
    DWORD retLen = ::GetModuleFileNameW(nullptr, buf.get(), bufLen);
    if (!retLen) {
      return nullptr;
    }

    if (retLen == bufLen && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      bufLen *= 2;
      continue;
    }

    break;
  }

  return buf;
}

static UniquePtr<wchar_t, LoadedCoTaskMemFreeDeleter> GetKnownFolderPath(
    REFKNOWNFOLDERID folderId) {
  static decltype(SHGetKnownFolderPath)* shGetKnownFolderPath = nullptr;
  if (!shGetKnownFolderPath) {
    // We could go out of our way to `FreeLibrary` on this, decrementing its
    // ref count and potentially unloading it. However doing so would be either
    // effectively a no-op, or counterproductive. Just let it get cleaned up
    // when the process is terminated, because we're going to load it anyway
    // elsewhere.
    HMODULE shell32Dll = ::LoadLibraryW(L"shell32");
    if (!shell32Dll) {
      return nullptr;
    }
    shGetKnownFolderPath = reinterpret_cast<decltype(shGetKnownFolderPath)>(
        ::GetProcAddress(shell32Dll, "SHGetKnownFolderPath"));
    if (!shGetKnownFolderPath) {
      return nullptr;
    }
  }
  PWSTR path = nullptr;
  shGetKnownFolderPath(folderId, 0, nullptr, &path);
  return UniquePtr<wchar_t, LoadedCoTaskMemFreeDeleter>(path);
}

// Note: this is specifically *not* a robust, multi-locale lowercasing
// operation. It is not intended to be such. It is simply intended to match the
// way in which we look for other instances of firefox to remote into.
// See
// https://searchfox.org/mozilla-central/rev/71621bfa47a371f2b1ccfd33c704913124afb933/toolkit/components/remote/nsRemoteService.cpp#56
static void MutateStringToLowercase(wchar_t* ptr) {
  while (*ptr) {
    wchar_t ch = *ptr;
    if (ch >= L'A' && ch <= L'Z') {
      *ptr = ch + (L'a' - L'A');
    }
    ++ptr;
  }
}

static bool TryGetSkeletonUILock() {
  auto localAppDataPath = GetKnownFolderPath(FOLDERID_LocalAppData);
  if (!localAppDataPath) {
    return false;
  }

  // Note: because we're in mozglue, we cannot easily access things from
  // toolkit, like `GetInstallHash`. We could move `GetInstallHash` into
  // mozglue, and rip out all of its usage of types defined in toolkit headers.
  // However, it seems cleaner to just hash the bin path ourselves. We don't
  // get quite the same robustness that `GetInstallHash` might provide, but
  // we already don't have that with how we key our registry values, so it
  // probably makes sense to just match those.
  UniquePtr<wchar_t[]> binPath = GetBinaryPath();
  if (!binPath) {
    return false;
  }

  // Lowercase the binpath to match how we look for remote instances.
  MutateStringToLowercase(binPath.get());

  // The number of bytes * 2 characters per byte + 1 for the null terminator
  uint32_t hexHashSize = sizeof(uint32_t) * 2 + 1;
  UniquePtr<wchar_t[]> installHash = MakeUnique<wchar_t[]>(hexHashSize);
  // This isn't perfect - it's a 32-bit hash of the path to our executable. It
  // could reasonably collide, or casing could potentially affect things, but
  // the theory is that that should be uncommon enough and the failure case
  // mild enough that this is fine.
  uint32_t binPathHash = HashString(binPath.get());
  swprintf(installHash.get(), hexHashSize, L"%08x", binPathHash);

  std::wstring lockFilePath;
  lockFilePath.append(localAppDataPath.get());
  lockFilePath.append(
      L"\\" MOZ_APP_VENDOR L"\\" MOZ_APP_BASENAME L"\\SkeletonUILock-");
  lockFilePath.append(installHash.get());

  // We intentionally leak this file - that is okay, and (kind of) the point.
  // We want to hold onto this handle until the application exits, and hold
  // onto it with exclusive rights. If this check fails, then we assume that
  // another instance of the executable is holding it, and thus return false.
  HANDLE lockFile =
      ::CreateFileW(lockFilePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                    0,  // No sharing - this is how the lock works
                    nullptr, CREATE_ALWAYS,
                    FILE_FLAG_DELETE_ON_CLOSE,  // Don't leave this lying around
                    nullptr);

  return lockFile != INVALID_HANDLE_VALUE;
}

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

int CSSToDevPixelsFloor(double cssPixels, double scaling) {
  return floor(cssPixels * scaling);
}

// Some things appear to floor to device pixels rather than rounding. A good
// example of this is border widths.
int CSSToDevPixelsFloor(int cssPixels, double scaling) {
  return CSSToDevPixelsFloor((double)cssPixels, scaling);
}

double SignedDistanceToCircle(double x, double y, double radius) {
  return sqrt(x * x + y * y) - radius;
}

// For more details, see
// https://searchfox.org/mozilla-central/rev/a5d9abfda1e26b1207db9549549ab0bdd73f735d/gfx/wr/webrender/res/shared.glsl#141-187
// which was a reference for this function.
double DistanceAntiAlias(double signedDistance) {
  // Distance assumed to be in device pixels. We use an aa range of 0.5 for
  // reasons detailed in the linked code above.
  const double aaRange = 0.5;
  double dist = 0.5 * signedDistance / aaRange;
  if (dist <= -0.5 + std::numeric_limits<double>::epsilon()) return 1.0;
  if (dist >= 0.5 - std::numeric_limits<double>::epsilon()) return 0.0;
  return 0.5 + dist * (0.8431027 * dist * dist - 1.14453603);
}

void RasterizeRoundedRectTopAndBottom(const DrawRect& rect) {
  if (rect.height <= 2 * rect.borderRadius) {
    MOZ_ASSERT(false, "Skeleton UI rect height too small for border radius.");
    return;
  }
  if (rect.width <= 2 * rect.borderRadius) {
    MOZ_ASSERT(false, "Skeleton UI rect width too small for border radius.");
    return;
  }

  NormalizedRGB rgbBase = UintToRGB(rect.backgroundColor);
  NormalizedRGB rgbBlend = UintToRGB(rect.color);

  for (int rowIndex = 0; rowIndex < rect.borderRadius; ++rowIndex) {
    int yTop = rect.y + rect.borderRadius - 1 - rowIndex;
    int yBottom = rect.y + rect.height - rect.borderRadius + rowIndex;

    uint32_t* lineStartTop = &sPixelBuffer[yTop * sWindowWidth];
    uint32_t* innermostPixelTopLeft =
        lineStartTop + rect.x + rect.borderRadius - 1;
    uint32_t* innermostPixelTopRight =
        lineStartTop + rect.x + rect.width - rect.borderRadius;
    uint32_t* lineStartBottom = &sPixelBuffer[yBottom * sWindowWidth];
    uint32_t* innermostPixelBottomLeft =
        lineStartBottom + rect.x + rect.borderRadius - 1;
    uint32_t* innermostPixelBottomRight =
        lineStartBottom + rect.x + rect.width - rect.borderRadius;

    // Add 0.5 to x and y to get the pixel center.
    double pixelY = (double)rowIndex + 0.5;
    for (int columnIndex = 0; columnIndex < rect.borderRadius; ++columnIndex) {
      double pixelX = (double)columnIndex + 0.5;
      double distance =
          SignedDistanceToCircle(pixelX, pixelY, (double)rect.borderRadius);
      double alpha = DistanceAntiAlias(distance);
      NormalizedRGB rgb = Lerp(rgbBase, rgbBlend, alpha);
      uint32_t color = RGBToUint(rgb);

      innermostPixelTopLeft[-columnIndex] = color;
      innermostPixelTopRight[columnIndex] = color;
      innermostPixelBottomLeft[-columnIndex] = color;
      innermostPixelBottomRight[columnIndex] = color;
    }

    std::fill(innermostPixelTopLeft + 1, innermostPixelTopRight, rect.color);
    std::fill(innermostPixelBottomLeft + 1, innermostPixelBottomRight,
              rect.color);
  }
}

void RasterizeAnimatedRoundedRectTopAndBottom(
    const ColorRect& colorRect, const uint32_t* animationLookup,
    int priorUpdateAreaMin, int priorUpdateAreaMax, int currentUpdateAreaMin,
    int currentUpdateAreaMax, int animationMin) {
  // We iterate through logical pixel rows here, from inside to outside, which
  // for the top of the rounded rect means from bottom to top, and for the
  // bottom of the rect means top to bottom. We paint pixels from left to
  // right on the top and bottom rows at the same time for the entire animation
  // window. (If the animation window does not overlap any rounded corners,
  // however, we won't be called at all)
  for (int rowIndex = 0; rowIndex < colorRect.borderRadius; ++rowIndex) {
    int yTop = colorRect.y + colorRect.borderRadius - 1 - rowIndex;
    int yBottom =
        colorRect.y + colorRect.height - colorRect.borderRadius + rowIndex;

    uint32_t* lineStartTop = &sPixelBuffer[yTop * sWindowWidth];
    uint32_t* lineStartBottom = &sPixelBuffer[yBottom * sWindowWidth];

    // Add 0.5 to x and y to get the pixel center.
    double pixelY = (double)rowIndex + 0.5;
    for (int x = priorUpdateAreaMin; x < currentUpdateAreaMax; ++x) {
      // The column index is the distance from the innermost pixel, which
      // is different depending on whether we're on the left or right
      // side of the rect. It will always be the max here, and if it's
      // negative that just means we're outside the rounded area.
      int columnIndex =
          std::max((int)colorRect.x + (int)colorRect.borderRadius - x - 1,
                   x - ((int)colorRect.x + (int)colorRect.width -
                        (int)colorRect.borderRadius));

      double alpha = 1.0;
      if (columnIndex >= 0) {
        double pixelX = (double)columnIndex + 0.5;
        double distance = SignedDistanceToCircle(
            pixelX, pixelY, (double)colorRect.borderRadius);
        alpha = DistanceAntiAlias(distance);
      }
      // We don't do alpha blending for the antialiased pixels at the
      // shape's border. It is not noticeable in the animation.
      if (alpha > 1.0 - std::numeric_limits<double>::epsilon()) {
        // Overwrite the tail end of last frame's animation with the
        // rect's normal, unanimated color.
        uint32_t color = x < priorUpdateAreaMax
                             ? colorRect.color
                             : animationLookup[x - animationMin];
        lineStartTop[x] = color;
        lineStartBottom[x] = color;
      }
    }
  }
}

void RasterizeColorRect(const ColorRect& colorRect) {
  // We sometimes split our rect into two, to simplify drawing borders. If we
  // have a border, we draw a stroke-only rect first, and then draw the smaller
  // inner rect on top of it.
  Vector<DrawRect, 2> drawRects;
  Unused << drawRects.reserve(2);
  if (colorRect.borderWidth == 0) {
    DrawRect rect = {};
    rect.color = colorRect.color;
    rect.backgroundColor =
        sPixelBuffer[colorRect.y * sWindowWidth + colorRect.x];
    rect.x = colorRect.x;
    rect.y = colorRect.y;
    rect.width = colorRect.width;
    rect.height = colorRect.height;
    rect.borderRadius = colorRect.borderRadius;
    rect.strokeOnly = false;
    drawRects.infallibleAppend(rect);
  } else {
    DrawRect borderRect = {};
    borderRect.color = colorRect.borderColor;
    borderRect.backgroundColor =
        sPixelBuffer[colorRect.y * sWindowWidth + colorRect.x];
    borderRect.x = colorRect.x;
    borderRect.y = colorRect.y;
    borderRect.width = colorRect.width;
    borderRect.height = colorRect.height;
    borderRect.borderRadius = colorRect.borderRadius;
    borderRect.borderWidth = colorRect.borderWidth;
    borderRect.strokeOnly = true;
    drawRects.infallibleAppend(borderRect);

    DrawRect baseRect = {};
    baseRect.color = colorRect.color;
    baseRect.backgroundColor = borderRect.color;
    baseRect.x = colorRect.x + colorRect.borderWidth;
    baseRect.y = colorRect.y + colorRect.borderWidth;
    baseRect.width = colorRect.width - 2 * colorRect.borderWidth;
    baseRect.height = colorRect.height - 2 * colorRect.borderWidth;
    baseRect.borderRadius =
        std::max(0, (int)colorRect.borderRadius - (int)colorRect.borderWidth);
    baseRect.borderWidth = 0;
    baseRect.strokeOnly = false;
    drawRects.infallibleAppend(baseRect);
  }

  for (const DrawRect& rect : drawRects) {
    if (rect.height <= 0 || rect.width <= 0) {
      continue;
    }

    // For rounded rectangles, the first thing we do is draw the top and
    // bottom of the rectangle, with the more complicated logic below. After
    // that we can just draw the vertically centered part of the rect like
    // normal.
    RasterizeRoundedRectTopAndBottom(rect);

    // We then draw the flat, central portion of the rect (which in the case of
    // non-rounded rects, is just the entire thing.)
    int solidRectStartY =
        std::clamp(rect.y + rect.borderRadius, 0, sTotalChromeHeight);
    int solidRectEndY = std::clamp(rect.y + rect.height - rect.borderRadius, 0,
                                   sTotalChromeHeight);
    for (int y = solidRectStartY; y < solidRectEndY; ++y) {
      // For strokeOnly rects (used to draw borders), we just draw the left
      // and right side here. Looping down a column of pixels is not the most
      // cache-friendly thing, but it shouldn't be a big deal given the height
      // of the urlbar.
      // Also, if borderRadius is less than borderWidth, we need to ensure
      // that we fully draw the top and bottom lines, so we make sure to check
      // that we're inside the middle range range before excluding pixels.
      if (rect.strokeOnly && y - rect.y > rect.borderWidth &&
          rect.y + rect.height - y > rect.borderWidth) {
        int startXLeft = std::clamp(rect.x, 0, sWindowWidth);
        int endXLeft = std::clamp(rect.x + rect.borderWidth, 0, sWindowWidth);
        int startXRight =
            std::clamp(rect.x + rect.width - rect.borderWidth, 0, sWindowWidth);
        int endXRight = std::clamp(rect.x + rect.width, 0, sWindowWidth);

        uint32_t* lineStart = &sPixelBuffer[y * sWindowWidth];
        uint32_t* dataStartLeft = lineStart + startXLeft;
        uint32_t* dataEndLeft = lineStart + endXLeft;
        uint32_t* dataStartRight = lineStart + startXRight;
        uint32_t* dataEndRight = lineStart + endXRight;
        std::fill(dataStartLeft, dataEndLeft, rect.color);
        std::fill(dataStartRight, dataEndRight, rect.color);
      } else {
        int startX = std::clamp(rect.x, 0, sWindowWidth);
        int endX = std::clamp(rect.x + rect.width, 0, sWindowWidth);
        uint32_t* lineStart = &sPixelBuffer[y * sWindowWidth];
        uint32_t* dataStart = lineStart + startX;
        uint32_t* dataEnd = lineStart + endX;
        std::fill(dataStart, dataEnd, rect.color);
      }
    }
  }
}

// Paints the pixels to sPixelBuffer for the skeleton UI animation (a light
// gradient which moves from left to right across the grey placeholder rects).
// Takes in the rect to draw, together with a lookup table for the gradient,
// and the bounds of the previous and current frame of the animation.
bool RasterizeAnimatedRect(const ColorRect& colorRect,
                           const uint32_t* animationLookup,
                           int priorAnimationMin, int animationMin,
                           int animationMax) {
  int rectMin = colorRect.x;
  int rectMax = colorRect.x + colorRect.width;
  bool animationWindowOverlaps =
      rectMax >= priorAnimationMin && rectMin < animationMax;

  int priorUpdateAreaMin = std::max(rectMin, priorAnimationMin);
  int priorUpdateAreaMax = std::min(rectMax, animationMin);
  int currentUpdateAreaMin = std::max(rectMin, animationMin);
  int currentUpdateAreaMax = std::min(rectMax, animationMax);

  if (!animationWindowOverlaps) {
    return false;
  }

  bool animationWindowOverlapsBorderRadius =
      rectMin + colorRect.borderRadius > priorAnimationMin ||
      rectMax - colorRect.borderRadius <= animationMax;

  // If we don't overlap the left or right side of the rounded rectangle,
  // just pretend it's not rounded. This is a small optimization but
  // there's no point in doing all of this rounded rectangle checking if
  // we aren't even overlapping
  int borderRadius =
      animationWindowOverlapsBorderRadius ? colorRect.borderRadius : 0;

  if (borderRadius > 0) {
    // Similarly to how we draw the rounded rects in DrawSkeletonUI, we
    // first draw the rounded top and bottom, and then we draw the center
    // rect.
    RasterizeAnimatedRoundedRectTopAndBottom(
        colorRect, animationLookup, priorUpdateAreaMin, priorUpdateAreaMax,
        currentUpdateAreaMin, currentUpdateAreaMax, animationMin);
  }

  for (int y = colorRect.y + borderRadius;
       y < colorRect.y + colorRect.height - borderRadius; ++y) {
    uint32_t* lineStart = &sPixelBuffer[y * sWindowWidth];
    // Overwrite the tail end of last frame's animation with the rect's
    // normal, unanimated color.
    for (int x = priorUpdateAreaMin; x < priorUpdateAreaMax; ++x) {
      lineStart[x] = colorRect.color;
    }
    // Then apply the animated color
    for (int x = currentUpdateAreaMin; x < currentUpdateAreaMax; ++x) {
      lineStart[x] = animationLookup[x - animationMin];
    }
  }

  return true;
}

void DrawSkeletonUI(HWND hWnd, CSSPixelSpan urlbarCSSSpan,
                    CSSPixelSpan searchbarCSSSpan,
                    const Vector<CSSPixelSpan>& springs,
                    const ThemeColors& currentTheme, const bool& menubarShown) {
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

  sAnimationColor = currentTheme.animationColor;
  sToolbarForegroundColor = currentTheme.toolbarForegroundColor;

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
  int titlebarSpacerWidth = horizontalOffset;
  if (!sMaximized && menubarShown == false) {
    titlebarSpacerWidth += CSSToDevPixels(40, sCSSToDevPixelScaling);
  }
  // found in tabs.inc.css, ".tab-line"
  int tabLineHeight = CSSToDevPixels(2, sCSSToDevPixelScaling) + verticalOffset;
  int selectedTabWidth = CSSToDevPixels(224, sCSSToDevPixelScaling);
  int toolbarHeight = CSSToDevPixels(39, sCSSToDevPixelScaling);
  // found in urlbar-searchbar.inc.css, "#urlbar[breakout]"
  int urlbarTopOffset = CSSToDevPixels(5, sCSSToDevPixelScaling);
  int urlbarHeight = CSSToDevPixels(30, sCSSToDevPixelScaling);
  // found in browser-aero.css, "#navigator-toolbox::after" border-bottom
  int chromeContentDividerHeight = CSSToDevPixels(1, sCSSToDevPixelScaling);

  int tabPlaceholderBarMarginTop = CSSToDevPixels(13, sCSSToDevPixelScaling);
  int tabPlaceholderBarMarginLeft = CSSToDevPixels(10, sCSSToDevPixelScaling);
  int tabPlaceholderBarHeight = CSSToDevPixels(8, sCSSToDevPixelScaling);
  int tabPlaceholderBarWidth = CSSToDevPixels(120, sCSSToDevPixelScaling);

  int toolbarPlaceholderMarginLeft = CSSToDevPixels(9, sCSSToDevPixelScaling);
  int toolbarPlaceholderMarginRight = CSSToDevPixels(11, sCSSToDevPixelScaling);
  int toolbarPlaceholderHeight = CSSToDevPixels(10, sCSSToDevPixelScaling);

  int placeholderMargin = CSSToDevPixels(8, sCSSToDevPixelScaling);

  int menubarHeightDevPixels =
      menubarShown ? CSSToDevPixels(28, sCSSToDevPixelScaling) : 0;

  // controlled by css variable urlbarMarginInline in urlbar-searchbar.inc.css
  int urlbarMargin =
      CSSToDevPixels(5, sCSSToDevPixelScaling) + horizontalOffset;

  int urlbarTextPlaceholderMarginTop =
      CSSToDevPixels(10, sCSSToDevPixelScaling);
  int urlbarTextPlaceholderMarginLeft =
      CSSToDevPixels(10, sCSSToDevPixelScaling);
  int urlbarTextPlaceHolderWidth = CSSToDevPixels(
      std::clamp(urlbarCSSSpan.end - urlbarCSSSpan.start - 10.0, 0.0, 260.0),
      sCSSToDevPixelScaling);
  int urlbarTextPlaceholderHeight = CSSToDevPixels(10, sCSSToDevPixelScaling);

  int searchbarTextPlaceholderWidth = CSSToDevPixels(62, sCSSToDevPixelScaling);

  auto scopeExit = MakeScopeExit([&] {
    delete sAnimatedRects;
    sAnimatedRects = nullptr;
    return;
  });

  Vector<ColorRect> rects;

  ColorRect topBorder = {};
  topBorder.color = 0x00000000;
  topBorder.x = 0;
  topBorder.y = 0;
  topBorder.width = sWindowWidth;
  topBorder.height = topBorderHeight;
  if (!rects.append(topBorder)) {
    return;
  }

  ColorRect menubar = {};
  menubar.color = currentTheme.tabBarColor;
  menubar.x = 0;
  menubar.y = topBorder.height;
  menubar.width = sWindowWidth;
  menubar.height = menubarHeightDevPixels;
  if (!rects.append(menubar)) {
    return;
  }

  int placeholderBorderRadius = CSSToDevPixels(2, sCSSToDevPixelScaling);
  // found in browser.css "--toolbarbutton-border-radius"
  int urlbarBorderRadius = CSSToDevPixels(2, sCSSToDevPixelScaling);
  // found in urlbar-searchbar.inc.css "#urlbar-background"
  int urlbarBorderWidth = CSSToDevPixelsFloor(1, sCSSToDevPixelScaling);
  int urlbarBorderColor = 0xbebebe;

  // The (traditionally dark blue on Windows) background of the tab bar.
  ColorRect tabBar = {};
  tabBar.color = currentTheme.tabBarColor;
  tabBar.x = 0;
  tabBar.y = menubar.height;
  tabBar.width = sWindowWidth;
  tabBar.height = tabBarHeight;
  if (!rects.append(tabBar)) {
    return;
  }

  // The blue highlight at the top of the initial selected tab
  ColorRect tabLine = {};
  tabLine.color = currentTheme.tabLineColor;
  tabLine.x = titlebarSpacerWidth;
  tabLine.y = menubar.height;
  tabLine.width = selectedTabWidth;
  tabLine.height = tabLineHeight;
  if (!rects.append(tabLine)) {
    return;
  }

  // The initial selected tab
  ColorRect selectedTab = {};
  selectedTab.color = currentTheme.backgroundColor;
  selectedTab.x = titlebarSpacerWidth;
  selectedTab.y = tabLine.y + tabLineHeight;
  selectedTab.width = selectedTabWidth;
  selectedTab.height = tabBar.y + tabBar.height - selectedTab.y;
  if (!rects.append(selectedTab)) {
    return;
  }

  // A placeholder rect representing text that will fill the selected tab title
  ColorRect tabTextPlaceholder = {};
  tabTextPlaceholder.color = sToolbarForegroundColor;
  tabTextPlaceholder.x = selectedTab.x + tabPlaceholderBarMarginLeft;
  tabTextPlaceholder.y = selectedTab.y + tabPlaceholderBarMarginTop;
  tabTextPlaceholder.width = tabPlaceholderBarWidth;
  tabTextPlaceholder.height = tabPlaceholderBarHeight;
  tabTextPlaceholder.borderRadius = placeholderBorderRadius;
  if (!rects.append(tabTextPlaceholder)) {
    return;
  }

  // The toolbar background
  ColorRect toolbar = {};
  toolbar.color = currentTheme.backgroundColor;
  toolbar.x = 0;
  toolbar.y = tabBar.y + tabBarHeight;
  toolbar.width = sWindowWidth;
  toolbar.height = toolbarHeight;
  if (!rects.append(toolbar)) {
    return;
  }

  // The single-pixel divider line below the toolbar
  ColorRect chromeContentDivider = {};
  chromeContentDivider.color = currentTheme.chromeContentDividerColor;
  chromeContentDivider.x = 0;
  chromeContentDivider.y = toolbar.y + toolbar.height;
  chromeContentDivider.width = sWindowWidth;
  chromeContentDivider.height = chromeContentDividerHeight;
  if (!rects.append(chromeContentDivider)) {
    return;
  }

  // The urlbar
  ColorRect urlbar = {};
  urlbar.color = currentTheme.urlbarColor;
  urlbar.x = CSSToDevPixels(urlbarCSSSpan.start, sCSSToDevPixelScaling) +
             horizontalOffset;
  urlbar.y = tabBar.y + tabBarHeight + urlbarTopOffset;
  urlbar.width = CSSToDevPixels((urlbarCSSSpan.end - urlbarCSSSpan.start),
                                sCSSToDevPixelScaling);
  urlbar.height = urlbarHeight;
  urlbar.borderRadius = urlbarBorderRadius;
  urlbar.borderWidth = urlbarBorderWidth;
  urlbar.borderColor = urlbarBorderColor;
  if (!rects.append(urlbar)) {
    return;
  }

  // The urlbar placeholder rect representating text that will fill the urlbar
  ColorRect urlbarTextPlaceholder = {};
  urlbarTextPlaceholder.color = sToolbarForegroundColor;
  urlbarTextPlaceholder.x = urlbar.x + urlbarTextPlaceholderMarginLeft;
  urlbarTextPlaceholder.y = urlbar.y + urlbarTextPlaceholderMarginTop;
  urlbarTextPlaceholder.width = urlbarTextPlaceHolderWidth;
  urlbarTextPlaceholder.height = urlbarTextPlaceholderHeight;
  urlbarTextPlaceholder.borderRadius = placeholderBorderRadius;
  if (!rects.append(urlbarTextPlaceholder)) {
    return;
  }

  // The searchbar and placeholder text, if present
  // This is y-aligned with the urlbar
  bool hasSearchbar = searchbarCSSSpan.start != 0 && searchbarCSSSpan.end != 0;
  ColorRect searchbarRect = {};
  if (hasSearchbar == true) {
    searchbarRect.color = currentTheme.urlbarColor;
    searchbarRect.x =
        CSSToDevPixels(searchbarCSSSpan.start, sCSSToDevPixelScaling) +
        horizontalOffset;
    searchbarRect.y = urlbar.y;
    searchbarRect.width = CSSToDevPixels(
        searchbarCSSSpan.end - searchbarCSSSpan.start, sCSSToDevPixelScaling);
    searchbarRect.height = urlbarHeight;
    searchbarRect.borderRadius = urlbarBorderRadius;
    searchbarRect.borderWidth = urlbarBorderWidth;
    searchbarRect.borderColor = urlbarBorderColor;
    if (!rects.append(searchbarRect)) {
      return;
    }

    // The placeholder rect representating text that will fill the searchbar
    // This uses the same margins as the urlbarTextPlaceholder
    ColorRect searchbarTextPlaceholder = {};
    searchbarTextPlaceholder.color = sToolbarForegroundColor;
    searchbarTextPlaceholder.x =
        searchbarRect.x + urlbarTextPlaceholderMarginLeft;
    searchbarTextPlaceholder.y =
        searchbarRect.y + urlbarTextPlaceholderMarginTop;
    searchbarTextPlaceholder.width = searchbarTextPlaceholderWidth;
    searchbarTextPlaceholder.height = urlbarTextPlaceholderHeight;
    if (!rects.append(searchbarTextPlaceholder) ||
        !sAnimatedRects->append(searchbarTextPlaceholder)) {
      return;
    }
  }

  // Determine where the placeholder rectangles should not go. This is
  // anywhere occupied by a spring, urlbar, or searchbar
  Vector<DevPixelSpan> noPlaceholderSpans;

  DevPixelSpan urlbarSpan;
  urlbarSpan.start = urlbar.x - urlbarMargin;
  urlbarSpan.end = urlbar.width + urlbar.x + urlbarMargin;

  DevPixelSpan searchbarSpan;
  if (hasSearchbar) {
    searchbarSpan.start = searchbarRect.x - urlbarMargin;
    searchbarSpan.end = searchbarRect.width + searchbarRect.x + urlbarMargin;
  }

  DevPixelSpan marginLeftPlaceholder;
  marginLeftPlaceholder.start = toolbarPlaceholderMarginLeft;
  marginLeftPlaceholder.end = toolbarPlaceholderMarginLeft;
  if (!noPlaceholderSpans.append(marginLeftPlaceholder)) {
    return;
  }

  for (auto spring : springs) {
    DevPixelSpan springDevPixels;
    springDevPixels.start =
        CSSToDevPixels(spring.start, sCSSToDevPixelScaling) + horizontalOffset;
    springDevPixels.end =
        CSSToDevPixels(spring.end, sCSSToDevPixelScaling) + horizontalOffset;
    if (!noPlaceholderSpans.append(springDevPixels)) {
      return;
    }
  }

  DevPixelSpan marginRightPlaceholder;
  marginRightPlaceholder.start = sWindowWidth - toolbarPlaceholderMarginRight;
  marginRightPlaceholder.end = sWindowWidth - toolbarPlaceholderMarginRight;
  if (!noPlaceholderSpans.append(marginRightPlaceholder)) {
    return;
  }

  Vector<DevPixelSpan, 2> spansToAdd;
  Unused << spansToAdd.reserve(2);
  spansToAdd.infallibleAppend(urlbarSpan);
  if (hasSearchbar) {
    spansToAdd.infallibleAppend(searchbarSpan);
  }

  for (auto& toAdd : spansToAdd) {
    for (auto& span : noPlaceholderSpans) {
      if (span.start > toAdd.start) {
        if (!noPlaceholderSpans.insert(&span, toAdd)) {
          return;
        }
        break;
      }
    }
  }

  for (int i = 1; i < noPlaceholderSpans.length(); i++) {
    int start = noPlaceholderSpans[i - 1].end + placeholderMargin;
    int end = noPlaceholderSpans[i].start - placeholderMargin;
    if (start + 2 * placeholderBorderRadius >= end) {
      continue;
    }

    // The placeholder rects should all be y-aligned.
    ColorRect placeholderRect = {};
    placeholderRect.color = sToolbarForegroundColor;
    placeholderRect.x = start;
    placeholderRect.y = urlbarTextPlaceholder.y;
    placeholderRect.width = end - start;
    placeholderRect.height = toolbarPlaceholderHeight;
    placeholderRect.borderRadius = placeholderBorderRadius;
    if (!rects.append(placeholderRect) ||
        !sAnimatedRects->append(placeholderRect)) {
      return;
    }
  }

  sTotalChromeHeight = chromeContentDivider.y + chromeContentDivider.height;
  if (sTotalChromeHeight > sWindowHeight) {
    printf_stderr("Exiting drawing skeleton UI because window is too small.\n");
    return;
  }

  if (!sAnimatedRects->append(tabTextPlaceholder) ||
      !sAnimatedRects->append(urlbarTextPlaceholder)) {
    return;
  }

  sPixelBuffer =
      (uint32_t*)calloc(sWindowWidth * sTotalChromeHeight, sizeof(uint32_t));

  for (auto& rect : *sAnimatedRects) {
    rect.x = std::clamp(rect.x, 0, sWindowWidth);
    rect.width = std::clamp(rect.width, 0, sWindowWidth - rect.x);
    rect.y = std::clamp(rect.y, 0, sTotalChromeHeight);
    rect.height = std::clamp(rect.height, 0, sTotalChromeHeight - rect.y);
  }

  for (auto& rect : rects) {
    rect.x = std::clamp(rect.x, 0, sWindowWidth);
    rect.width = std::clamp(rect.width, 0, sWindowWidth - rect.x);
    rect.y = std::clamp(rect.y, 0, sTotalChromeHeight);
    rect.height = std::clamp(rect.height, 0, sTotalChromeHeight - rect.y);
    RasterizeColorRect(rect);
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
  RECT rect = {0, sTotalChromeHeight, sWindowWidth, sWindowHeight};
  HBRUSH brush = sCreateSolidBrush(currentTheme.backgroundColor);
  sFillRect(hdc, &rect, brush);

  scopeExit.release();
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
  uint32_t animationColor = sAnimationColor;
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
      bool hadUpdates =
          RasterizeAnimatedRect(rect, animationLookup.get(), priorAnimationMin,
                                animationMin, animationMax);
      updatedAnything = updatedAnything || hadUpdates;
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

bool IsSystemDarkThemeEnabled() {
  DWORD result;
  HKEY themeKey;
  DWORD dataLen = sizeof(uint32_t);
  LPCWSTR keyName =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

  result = ::RegOpenKeyExW(HKEY_CURRENT_USER, keyName, 0, KEY_READ, &themeKey);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  AutoCloseRegKey closeKey(themeKey);

  uint32_t lightThemeEnabled;
  result = ::RegGetValueW(
      themeKey, nullptr, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr,
      reinterpret_cast<PBYTE>(&lightThemeEnabled), &dataLen);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  return !lightThemeEnabled;
}

ThemeColors GetTheme(ThemeMode themeId) {
  ThemeColors theme = {};
  switch (themeId) {
    case ThemeMode::Dark:
      // Dark theme or default theme when in dark mode

      // controlled by css variable --toolbar-bgcolor
      theme.backgroundColor = 0x323234;
      theme.toolbarForegroundColor = 0x6a6a6b;
      // controlled by css variable --lwt-accent-color
      theme.tabBarColor = 0x0c0c0d;
      // controlled by --toolbar-non-lwt-textcolor in browser.css
      theme.chromeContentDividerColor = 0x0c0c0d;
      // controlled by css variable --tab-line-color
      theme.tabLineColor = 0x0a84ff;
      // controlled by css variable --lwt-toolbar-field-background-colo
      theme.urlbarColor = 0x474749;
      theme.animationColor = theme.urlbarColor;
      return theme;
    case ThemeMode::Light:
      // Light theme

      // controlled by --toolbar-bgcolor
      theme.backgroundColor = 0xf5f6f7;
      theme.toolbarForegroundColor = 0xd9dadb;
      // controlled by css variable --lwt-accent-color
      theme.tabBarColor = 0xe3e4e6;
      // --chrome-content-separator-color in browser.css
      theme.chromeContentDividerColor = 0x9e9fa1;
      // controlled by css variable --tab-line-color
      theme.tabLineColor = 0x0a84ff;
      // by css variable --lwt-toolbar-field-background-color
      theme.urlbarColor = 0xffffff;
      theme.animationColor = theme.backgroundColor;
      return theme;
    case ThemeMode::Default:
    default:
      // Default theme when not in dark mode
      MOZ_ASSERT(themeId == ThemeMode::Default);

      // --toolbar-non-lwt-bgcolor in browser.css
      theme.backgroundColor = 0xf9f9fa;
      theme.toolbarForegroundColor = 0xe5e5e5;
      // found in browser-aero.css ":root[tabsintitlebar]:not(:-moz-lwtheme)"
      // (set to "hsl(235,33%,19%)")
      theme.tabBarColor = 0x202340;
      // --chrome-content-separator-color in browser.css
      theme.chromeContentDividerColor = 0xe2e1e3;
      // controlled by css variable --tab-line-color
      theme.tabLineColor = 0x0a84ff;
      // controlled by css variable --toolbar-color
      theme.urlbarColor = 0xffffff;
      theme.animationColor = theme.backgroundColor;
      return theme;
  }
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

// Strips "--", "-", and "/" from the front of the arg if one of those exists,
// returning `arg + 2`, `arg + 1`, and `arg + 1` respectively. If none of these
// prefixes are found, the argument is not a flag, and nullptr is returned.
const char* NormalizeFlag(const char* arg) {
  if (strstr(arg, "--") == arg) {
    return arg + 2;
  }

  if (arg[0] == '-') {
    return arg + 1;
  }

  if (arg[0] == '/') {
    return arg + 1;
  }

  return nullptr;
}

static bool EnvHasValue(const char* name) {
  const char* val = getenv(name);
  return (val && *val);
}

// Ensures that we only see arguments in the command line which are acceptable.
// This is based on manual inspection of the list of arguments listed in the MDN
// page for Gecko/Firefox commandline options:
// https://developer.mozilla.org/en-US/docs/Mozilla/Command_Line_Options
// Broadly speaking, we want to reject any argument which causes us to show
// something other than the default window at its normal size. Here is a non-
// exhaustive list of command line options we want to *exclude*:
//
//   -ProfileManager : This will display the profile manager window, which does
//                     not match the skeleton UI at all.
//
//   -CreateProfile  : This will display a firefox window with the default
//                     screen position and size, and not the position and size
//                     which we have recorded in the registry.
//
//   -P <profile>    : This could cause us to display firefox with a position
//                     and size of a different profile than that in which we
//                     were previously running.
//
//   -width, -height : This will cause the width and height values in the
//                     registry to be incorrect.
//
//   -kiosk          : See above.
//
//   -headless       : This one should be rather obvious.
//
//   -migration      : This will start with the import wizard, which of course
//                     does not match the skeleton UI.
//
//   -private-window : This is tricky, but the colors of the main content area
//                     make this not feel great with the white content of the
//                     default skeleton UI.
//
// NOTE: we generally want to skew towards erroneous rejections of the command
// line rather than erroneous approvals. The consequence of a bad rejection
// is that we don't show the skeleton UI, which is business as usual. The
// consequence of a bad approval is that we show it when we're not supposed to,
// which is visually jarring and can also be unpredictable - there's no
// guarantee that the code which handles the non-default window is set up to
// properly handle the transition from the skeleton UI window.
bool AreAllCmdlineArgumentsApproved(int argc, char** argv) {
  const char* approvedArgumentsArray[] = {
      // These won't cause the browser to be visualy different in any way
      "new-instance", "no-remote", "browser", "foreground", "setDefaultBrowser",
      "attach-console", "wait-for-browser", "osint",

      // These will cause the chrome to be a bit different or extra windows to
      // be created, but overall the skeleton UI should still be broadly
      // correct enough.
      "new-tab", "new-window",

      // To the extent possible, we want to ensure that existing tests cover
      // the skeleton UI, so we need to allow marionette
      "marionette",

      // These will cause the content area to appear different, but won't
      // meaningfully affect the chrome
      "preferences", "search", "url",

#ifndef MOZILLA_OFFICIAL
      // On local builds, we want to allow -profile, because it's how `mach run`
      // operates, and excluding that would create an unnecessary blind spot for
      // Firefox devs.
      "profile"
#endif

      // There are other arguments which are likely okay. However, they are
      // not included here because this list is not intended to be
      // exhaustive - it only intends to green-light some somewhat commonly
      // used arguments. We want to err on the side of an unnecessary
      // rejection of the command line.
  };

  int approvedArgumentsArraySize =
      sizeof(approvedArgumentsArray) / sizeof(approvedArgumentsArray[0]);
  Vector<const char*> approvedArguments;
  if (!approvedArguments.reserve(approvedArgumentsArraySize)) {
    return false;
  }

  for (int i = 0; i < approvedArgumentsArraySize; ++i) {
    approvedArguments.infallibleAppend(approvedArgumentsArray[i]);
  }

#ifdef MOZILLA_OFFICIAL
  // If we're running mochitests or direct marionette tests, those specify a
  // temporary profile, and we want to ensure that we get the added coverage
  // from those.
  for (int i = 1; i < argc; ++i) {
    const char* flag = NormalizeFlag(argv[i]);
    if (flag && !strcmp(flag, "marionette")) {
      if (!approvedArguments.append("profile")) {
        return false;
      }

      break;
    }
  }
#endif

  for (int i = 1; i < argc; ++i) {
    const char* flag = NormalizeFlag(argv[i]);
    if (!flag) {
      // If this is not a flag, then we interpret it as a URL, similar to
      // BrowserContentHandler.jsm. Some command line options take additional
      // arguments, which may or may not be URLs. We don't need to know this,
      // because we don't need to parse them out; we just rely on the
      // assumption that if arg X is actually a parameter for the preceding
      // arg Y, then X must not look like a flag (starting with "--", "-",
      // or "/").
      //
      // The most important thing here is the assumption that if something is
      // going to meaningfully alter the appearance of the window itself, it
      // must be a flag.
      continue;
    }

    bool approved = false;
    for (const char* approvedArg : approvedArguments) {
      // We do a case-insensitive compare here with _stricmp. Even though some
      // of these arguments are *not* read as case-insensitive, others *are*.
      // Similar to the flag logic above, we don't really care about this
      // distinction, because we don't need to parse the arguments - we just
      // rely on the assumption that none of the listed flags in our
      // approvedArguments are overloaded in such a way that a different
      // casing would visually alter the firefox window.
      if (!_stricmp(flag, approvedArg)) {
        approved = true;
        break;
      }
    }

    if (!approved) {
      return false;
    }
  }

  return true;
}

static bool VerifyWindowDimensions(uint32_t windowWidth,
                                   uint32_t windowHeight) {
  return windowWidth <= kMaxWindowWidth && windowHeight <= kMaxWindowHeight;
}

void CreateAndStorePreXULSkeletonUI(HINSTANCE hInstance, int argc,
                                    char** argv) {
#ifdef MOZ_GECKO_PROFILER
  const TimeStamp skeletonStart = TimeStamp::NowUnfuzzed();
#endif

  if (!AreAllCmdlineArgumentsApproved(argc, argv) ||
      EnvHasValue("MOZ_SAFE_MODE_RESTART") || EnvHasValue("XRE_PROFILE_PATH") ||
      EnvHasValue("MOZ_RESET_PROFILE_RESTART")) {
    sPreXULSkeletonUIDisallowed = true;
    return;
  }

  HKEY regKey;
  if (!IsWin10OrLater() || !OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);

  UniquePtr<wchar_t[]> binPath = GetBinaryPath();

  DWORD dataLen = sizeof(uint32_t);
  uint32_t enabled;
  LSTATUS result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sEnabledRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&enabled), &dataLen);
  if (result != ERROR_SUCCESS || enabled == 0) {
    return;
  }
  sPreXULSkeletonUIEnabled = true;

  MOZ_ASSERT(!sAnimatedRects);
  sAnimatedRects = new Vector<ColorRect>();

  if (!LoadGdi32AndUser32Procedures()) {
    return;
  }

  if (!TryGetSkeletonUILock()) {
    printf_stderr("Error trying to get skeleton UI lock %lu\n", GetLastError());
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
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sScreenXRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&screenX), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading screenX %lu\n", GetLastError());
    return;
  }

  uint32_t screenY;
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sScreenYRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&screenY), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading screenY %lu\n", GetLastError());
    return;
  }

  uint32_t windowWidth;
  result = ::RegGetValueW(
      regKey, nullptr, GetRegValueName(binPath.get(), sWidthRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&windowWidth),
      &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading width %lu\n", GetLastError());
    return;
  }

  uint32_t windowHeight;
  result = ::RegGetValueW(
      regKey, nullptr, GetRegValueName(binPath.get(), sHeightRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&windowHeight),
      &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading height %lu\n", GetLastError());
    return;
  }

  uint32_t maximized;
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sMaximizedRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&maximized), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading maximized %lu\n", GetLastError());
    return;
  }
  sMaximized = maximized != 0;

  uint32_t menubarShown;
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sMenubarShownRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&menubarShown),
      &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading menubarShown %lu\n", GetLastError());
    return;
  }

  dataLen = sizeof(double);
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sCssToDevPixelScalingRegSuffix).c_str(),
      RRF_RT_REG_BINARY, nullptr,
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

  dataLen = 2 * sizeof(double);
  auto buffer = MakeUniqueFallible<wchar_t[]>(2 * sizeof(double));
  if (!buffer) {
    return;
  }
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sUrlbarCSSRegSuffix).c_str(),
      RRF_RT_REG_BINARY, nullptr, reinterpret_cast<PBYTE>(buffer.get()),
      &dataLen);
  if (result != ERROR_SUCCESS || dataLen % (2 * sizeof(double)) != 0) {
    printf_stderr("Error reading urlbar %lu\n", GetLastError());
    return;
  }

  double* asDoubles = reinterpret_cast<double*>(buffer.get());
  CSSPixelSpan urlbar;
  urlbar.start = *(asDoubles++);
  urlbar.end = *(asDoubles++);

  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sSearchbarRegSuffix).c_str(),
      RRF_RT_REG_BINARY, nullptr, reinterpret_cast<PBYTE>(buffer.get()),
      &dataLen);
  if (result != ERROR_SUCCESS || dataLen % (2 * sizeof(double)) != 0) {
    printf_stderr("Error reading searchbar %lu\n", GetLastError());
    return;
  }

  asDoubles = reinterpret_cast<double*>(buffer.get());
  CSSPixelSpan searchbar;
  searchbar.start = *(asDoubles++);
  searchbar.end = *(asDoubles++);

  result = ::RegQueryValueExW(
      regKey, GetRegValueName(binPath.get(), sSpringsCSSRegSuffix).c_str(),
      nullptr, nullptr, nullptr, &dataLen);
  if (result != ERROR_SUCCESS || dataLen % (2 * sizeof(double)) != 0) {
    printf_stderr("Error reading springsCSS %lu\n", GetLastError());
    return;
  }

  buffer = MakeUniqueFallible<wchar_t[]>(dataLen);
  if (!buffer) {
    return;
  }
  result = ::RegGetValueW(
      regKey, nullptr,
      GetRegValueName(binPath.get(), sSpringsCSSRegSuffix).c_str(),
      RRF_RT_REG_BINARY, nullptr, reinterpret_cast<PBYTE>(buffer.get()),
      &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading springsCSS %lu\n", GetLastError());
    return;
  }

  Vector<CSSPixelSpan> springs;
  asDoubles = reinterpret_cast<double*>(buffer.get());
  for (int i = 0; i < dataLen / (2 * sizeof(double)); i++) {
    CSSPixelSpan spring;
    spring.start = *(asDoubles++);
    spring.end = *(asDoubles++);
    if (!springs.append(spring)) {
      return;
    }
  }

  dataLen = sizeof(uint32_t);
  uint32_t theme;
  result = ::RegGetValueW(
      regKey, nullptr, GetRegValueName(binPath.get(), sThemeRegSuffix).c_str(),
      RRF_RT_REG_DWORD, nullptr, reinterpret_cast<PBYTE>(&theme), &dataLen);
  if (result != ERROR_SUCCESS) {
    printf_stderr("Error reading theme %lu\n", GetLastError());
    return;
  }
  ThemeMode themeMode = static_cast<ThemeMode>(theme);
  if (themeMode == ThemeMode::Default) {
    if (IsSystemDarkThemeEnabled() == true) {
      themeMode = ThemeMode::Dark;
    }
  }
  ThemeColors currentTheme = GetTheme(themeMode);

  if (!VerifyWindowDimensions(windowWidth, windowHeight)) {
    printf_stderr("Bad window dimensions for skeleton UI.");
    return;
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
    sWindowWidth = static_cast<int>(windowWidth);
    sWindowHeight = static_cast<int>(windowHeight);
  }

  sSetWindowPos(sPreXULSkeletonUIWindow, 0, 0, 0, 0, 0,
                SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
                    SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
  DrawSkeletonUI(sPreXULSkeletonUIWindow, urlbar, searchbar, springs,
                 currentTheme, menubarShown);
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

void PersistPreXULSkeletonUIValues(const SkeletonUISettings& settings) {
  if (!sPreXULSkeletonUIEnabled) {
    return;
  }

  HKEY regKey;
  if (!OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);

  UniquePtr<wchar_t[]> binPath = GetBinaryPath();

  LSTATUS result;
  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sScreenXRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<const BYTE*>(&settings.screenX),
      sizeof(settings.screenX));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting screenX to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sScreenYRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<const BYTE*>(&settings.screenY),
      sizeof(settings.screenY));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting screenY to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sWidthRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<const BYTE*>(&settings.width),
      sizeof(settings.width));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting width to Windows registry\n");
    return;
  }

  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sHeightRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<const BYTE*>(&settings.height),
      sizeof(settings.height));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting height to Windows registry\n");
    return;
  }

  DWORD maximizedDword = settings.maximized ? 1 : 0;
  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sMaximizedRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<const BYTE*>(&maximizedDword),
      sizeof(maximizedDword));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting maximized to Windows registry\n");
  }

  DWORD menubarShownDword = settings.menubarShown ? 1 : 0;
  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sMenubarShownRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<const BYTE*>(&menubarShownDword),
      sizeof(menubarShownDword));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting menubarShown to Windows registry\n");
  }

  result = ::RegSetValueExW(
      regKey,
      GetRegValueName(binPath.get(), sCssToDevPixelScalingRegSuffix).c_str(), 0,
      REG_BINARY, reinterpret_cast<const BYTE*>(&settings.cssToDevPixelScaling),
      sizeof(settings.cssToDevPixelScaling));
  if (result != ERROR_SUCCESS) {
    printf_stderr(
        "Failed persisting cssToDevPixelScaling to Windows registry\n");
    return;
  }

  double urlbar[2];
  urlbar[0] = settings.urlbarSpan.start;
  urlbar[1] = settings.urlbarSpan.end;
  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sUrlbarCSSRegSuffix).c_str(), 0,
      REG_BINARY, reinterpret_cast<const BYTE*>(urlbar), sizeof(urlbar));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting urlbar to Windows registry\n");
    return;
  }

  double searchbar[2];
  searchbar[0] = settings.searchbarSpan.start;
  searchbar[1] = settings.searchbarSpan.end;
  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sSearchbarRegSuffix).c_str(), 0,
      REG_BINARY, reinterpret_cast<const BYTE*>(searchbar), sizeof(searchbar));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting searchbar to Windows registry\n");
    return;
  }

  Vector<double> springValues;
  if (!springValues.reserve(settings.springs.length() * 2)) {
    return;
  }

  for (auto spring : settings.springs) {
    springValues.infallibleAppend(spring.start);
    springValues.infallibleAppend(spring.end);
  }

  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sSpringsCSSRegSuffix).c_str(), 0,
      REG_BINARY, reinterpret_cast<const BYTE*>(springValues.begin()),
      springValues.length() * sizeof(double));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting springsCSS to Windows registry\n");
    return;
  }
}

MFBT_API bool GetPreXULSkeletonUIEnabled() { return sPreXULSkeletonUIEnabled; }

MFBT_API void SetPreXULSkeletonUIEnabledIfAllowed(bool value) {
  // If the pre-XUL skeleton UI was disallowed for some reason, we just want to
  // ignore changes to the registry. An example of how things could be bad if
  // we didn't: someone running firefox with the -profile argument could
  // turn the skeleton UI on or off for the default profile. Turning it off
  // maybe isn't so bad (though it's likely still incorrect), but turning it
  // on could be bad if the user had specifically disabled it for a profile for
  // some reason. Ultimately there's no correct decision here, and the
  // messiness of this is just a consequence of sharing the registry values
  // across profiles. However, whatever ill effects we observe should be
  // correct themselves after one session.
  if (sPreXULSkeletonUIDisallowed) {
    return;
  }

  HKEY regKey;
  if (!OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);

  UniquePtr<wchar_t[]> binPath = GetBinaryPath();
  DWORD enabled = value;
  LSTATUS result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sEnabledRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<PBYTE>(&enabled), sizeof(enabled));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting enabled to Windows registry\n");
    return;
  }

  if (!sPreXULSkeletonUIEnabled && value) {
    // We specifically don't care if we fail to get this lock. We just want to
    // do our best effort to lock it so that future instances don't create
    // skeleton UIs while we're still running, since they will immediately exit
    // and tell us to open a new window.
    Unused << TryGetSkeletonUILock();
  }

  sPreXULSkeletonUIEnabled = value;
}

MFBT_API void SetPreXULSkeletonUIThemeId(ThemeMode theme) {
  if (theme == sTheme) {
    return;
  }

  HKEY regKey;
  if (!OpenPreXULSkeletonUIRegKey(regKey)) {
    return;
  }
  AutoCloseRegKey closeKey(regKey);

  UniquePtr<wchar_t[]> binPath = GetBinaryPath();
  uint32_t themeId = (uint32_t)theme;
  LSTATUS result;
  result = ::RegSetValueExW(
      regKey, GetRegValueName(binPath.get(), sThemeRegSuffix).c_str(), 0,
      REG_DWORD, reinterpret_cast<PBYTE>(&themeId), sizeof(themeId));
  if (result != ERROR_SUCCESS) {
    printf_stderr("Failed persisting theme to Windows registry\n");
    sTheme = ThemeMode::Invalid;
    return;
  }
  sTheme = static_cast<ThemeMode>(themeId);
}

MFBT_API void PollPreXULSkeletonUIEvents() {
  if (sPreXULSkeletonUIEnabled && sPreXULSkeletonUIWindow) {
    MSG outMsg = {};
    PeekMessageW(&outMsg, sPreXULSkeletonUIWindow, 0, 0, 0);
  }
}

}  // namespace mozilla
