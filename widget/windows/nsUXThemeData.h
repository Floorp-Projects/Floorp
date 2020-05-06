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

#include "nsWindowDefs.h"

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
  WINTHEME_CLASSIC = 1,  // no theme
  WINTHEME_AERO = 2,
  WINTHEME_LUNA = 3,
  WINTHEME_ROYALE = 4,
  WINTHEME_ZUNE = 5,
  WINTHEME_AERO_LITE = 6
};
enum WindowsThemeColor {
  WINTHEMECOLOR_UNRECOGNIZED = 0,
  WINTHEMECOLOR_NORMAL = 1,
  WINTHEMECOLOR_HOMESTEAD = 2,
  WINTHEMECOLOR_METALLIC = 3
};
enum CmdButtonIdx {
  CMDBUTTONIDX_MINIMIZE = 0,
  CMDBUTTONIDX_RESTORE,
  CMDBUTTONIDX_CLOSE,
  CMDBUTTONIDX_BUTTONBOX
};

class nsUXThemeData {
  // This class makes sure we don't attempt to open a theme if the previous
  // loading attempt has failed because OpenThemeData is a heavy task and
  // it's less likely that the API returns a different result.
  class ThemeHandle final {
    Maybe<HANDLE> mHandle;

   public:
    ThemeHandle() = default;
    ~ThemeHandle();

    // Disallow copy and move
    ThemeHandle(const ThemeHandle&) = delete;
    ThemeHandle(ThemeHandle&&) = delete;
    ThemeHandle& operator=(const ThemeHandle&) = delete;
    ThemeHandle& operator=(ThemeHandle&&) = delete;

    operator HANDLE();
    void OpenOnce(HWND aWindow, LPCWSTR aClassList);
    void Close();
  };

  static ThemeHandle sThemes[eUXNumClasses];

  // We initialize sCommandButtonBoxMetrics separately as a performance
  // optimization to avoid fetching dummy values for sCommandButtonMetrics
  // when we don't need those.
  static SIZE sCommandButtonMetrics[3];
  static bool sCommandButtonMetricsInitialized;
  static SIZE sCommandButtonBoxMetrics;
  static bool sCommandButtonBoxMetricsInitialized;

  static const wchar_t* GetClassName(nsUXThemeClass);
  static void EnsureCommandButtonMetrics();
  static void EnsureCommandButtonBoxMetrics();

 public:
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

  static SIZE GetCommandButtonMetrics(CmdButtonIdx aMetric) {
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
};
#endif  // __UXThemeData_h__
