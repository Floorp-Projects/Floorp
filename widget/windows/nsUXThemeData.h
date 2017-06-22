/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __UXThemeData_h__
#define __UXThemeData_h__
#include <windows.h>
#include <uxtheme.h>

#include "nscore.h"
#include "mozilla/LookAndFeel.h"
#include "WinUtils.h"

#include <dwmapi.h>

#include "nsWindowDefs.h"

// These window messages are not defined in dwmapi.h
#ifndef WM_DWMCOMPOSITIONCHANGED
#define WM_DWMCOMPOSITIONCHANGED        0x031E
#endif

// Windows 7 additions
#ifndef WM_DWMSENDICONICTHUMBNAIL
#define WM_DWMSENDICONICTHUMBNAIL 0x0323
#define WM_DWMSENDICONICLIVEPREVIEWBITMAP 0x0326
#endif

#define DWMWA_FORCE_ICONIC_REPRESENTATION 7
#define DWMWA_HAS_ICONIC_BITMAP           10

enum nsUXThemeClass {
  eUXButton = 0,
  eUXEdit,
  eUXTooltip,
  eUXRebar,
  eUXMediaRebar,
  eUXCommunicationsRebar,
  eUXBrowserTabBarRebar,
  eUXToolbar,
  eUXMediaToolbar,
  eUXCommunicationsToolbar,
  eUXProgress,
  eUXTab,
  eUXScrollbar,
  eUXTrackbar,
  eUXSpin,
  eUXStatus,
  eUXCombobox,
  eUXHeader,
  eUXListview,
  eUXMenu,
  eUXWindowFrame,
  eUXNumClasses
};

// Native windows style constants
enum WindowsTheme {
  WINTHEME_UNRECOGNIZED = 0,
  WINTHEME_CLASSIC      = 1, // no theme
  WINTHEME_AERO         = 2,
  WINTHEME_LUNA         = 3,
  WINTHEME_ROYALE       = 4,
  WINTHEME_ZUNE         = 5,
  WINTHEME_AERO_LITE    = 6
};
enum WindowsThemeColor {
  WINTHEMECOLOR_UNRECOGNIZED = 0,
  WINTHEMECOLOR_NORMAL       = 1,
  WINTHEMECOLOR_HOMESTEAD    = 2,
  WINTHEMECOLOR_METALLIC     = 3
};

#define CMDBUTTONIDX_MINIMIZE    0
#define CMDBUTTONIDX_RESTORE     1
#define CMDBUTTONIDX_CLOSE       2
#define CMDBUTTONIDX_BUTTONBOX   3

class nsUXThemeData {
  static HMODULE sThemeDLL;
  static HANDLE sThemes[eUXNumClasses];

  // We initialize sCommandButtonBoxMetrics separately as a performance
  // optimization to avoid fetching dummy values for sCommandButtonMetrics
  // when we don't need those.
  static SIZE sCommandButtonMetrics[3];
  static bool sCommandButtonMetricsInitialized;
  static SIZE sCommandButtonBoxMetrics;
  static bool sCommandButtonBoxMetricsInitialized;

  static const wchar_t *GetClassName(nsUXThemeClass);
  static void EnsureCommandButtonMetrics();
  static void EnsureCommandButtonBoxMetrics();

public:
  static const wchar_t kThemeLibraryName[];
  static bool sFlatMenus;
  static bool sTitlebarInfoPopulatedAero;
  static bool sTitlebarInfoPopulatedThemed;
  static mozilla::LookAndFeel::WindowsTheme sThemeId;
  static bool sIsDefaultWindowsTheme;
  static bool sIsHighContrastOn;

  static void Initialize();
  static void Teardown();
  static void Invalidate();
  static HANDLE GetTheme(nsUXThemeClass cls);
  static HMODULE GetThemeDLL();

  // nsWindow calls this to update desktop settings info
  static void UpdateTitlebarInfo(HWND aWnd);

  static SIZE GetCommandButtonMetrics(int aMetric) {
    EnsureCommandButtonMetrics();
    return sCommandButtonMetrics[aMetric];
  }
  static SIZE GetCommandButtonBoxMetrics() {
    EnsureCommandButtonBoxMetrics();
    return sCommandButtonBoxMetrics;
  }
  static void UpdateNativeThemeInfo();
  static mozilla::LookAndFeel::WindowsTheme GetNativeThemeId();
  static bool IsDefaultWindowTheme();
  static bool IsHighContrastOn();

  // This method returns the cached compositor state. Most
  // callers should call without the argument. The cache
  // should be modified only when the application receives
  // WM_DWMCOMPOSITIONCHANGED. This rule prevents inconsistent
  // results for two or more calls which check the state during
  // composition transition.
  static bool CheckForCompositor(bool aUpdateCache = false);
};
#endif // __UXThemeData_h__
