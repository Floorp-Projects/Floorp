/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef PreXULSkeletonUI_h_
#define PreXULSkeletonUI_h_

#include <windows.h>
#include "mozilla/EnumSet.h"
#include "mozilla/Types.h"
#include "mozilla/Vector.h"

namespace mozilla {

// These unfortunately need to be kept in sync with the window style and
// extended window style computations in nsWindow. Luckily those styles seem
// to not vary based off of any user settings for the initial toplevel window,
// so we're safe here for now.
static const DWORD kPreXULSkeletonUIWindowStyle =
    WS_CLIPCHILDREN | WS_DLGFRAME | WS_BORDER | WS_MAXIMIZEBOX |
    WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU;
static const DWORD kPreXULSkeletonUIWindowStyleEx = WS_EX_WINDOWEDGE;

struct CSSPixelSpan {
  double start;
  double end;
};

struct DevPixelSpan {
  int start;
  int end;
};

struct SkeletonUISettings {
  uint32_t screenX;
  uint32_t screenY;
  uint32_t width;
  uint32_t height;
  CSSPixelSpan urlbarSpan;
  CSSPixelSpan searchbarSpan;
  double cssToDevPixelScaling;
  Vector<CSSPixelSpan> springs;
  bool maximized;
  bool menubarShown;
  bool bookmarksToolbarShown;
  bool rtlEnabled;
};

enum class ThemeMode : uint32_t { Invalid, Default, Dark, Light };

enum class SkeletonUIFlag : uint8_t {
  MenubarShown,
  BookmarksToolbarShown,
  RtlEnabled,
};

struct ThemeColors {
  uint32_t backgroundColor;
  uint32_t toolbarForegroundColor;
  uint32_t tabBarColor;
  uint32_t chromeContentDividerColor;
  uint32_t tabLineColor;
  uint32_t urlbarColor;
  uint32_t urlbarBorderColor;
  uint32_t animationColor;
};

MFBT_API void CreateAndStorePreXULSkeletonUI(HINSTANCE hInstance, int argc,
                                             char** argv);
MFBT_API HWND ConsumePreXULSkeletonUIHandle();
MFBT_API bool WasPreXULSkeletonUIMaximized();
MFBT_API void PersistPreXULSkeletonUIValues(const SkeletonUISettings& settings);
MFBT_API bool GetPreXULSkeletonUIEnabled();
MFBT_API void SetPreXULSkeletonUIEnabledIfAllowed(bool value);
MFBT_API void PollPreXULSkeletonUIEvents();
MFBT_API void SetPreXULSkeletonUIThemeId(ThemeMode theme);

}  // namespace mozilla

#endif
