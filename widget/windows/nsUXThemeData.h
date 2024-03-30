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
#include "mozilla/Maybe.h"
#include "WinUtils.h"

#include "nsWindowDefs.h"

enum nsUXThemeClass {
  eUXButton = 0,
  eUXEdit,
  eUXToolbar,
  eUXProgress,
  eUXTab,
  eUXTrackbar,
  eUXCombobox,
  eUXHeader,
  eUXListview,
  eUXMenu,
  eUXNumClasses
};

class nsUXThemeData {
  // This class makes sure we don't attempt to open a theme if the previous
  // loading attempt has failed because OpenThemeData is a heavy task and
  // it's less likely that the API returns a different result.
  class ThemeHandle final {
    mozilla::Maybe<HANDLE> mHandle;

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
  static const wchar_t* GetClassName(nsUXThemeClass);

 public:
  static bool sIsHighContrastOn;

  static void Invalidate();
  static HANDLE GetTheme(nsUXThemeClass cls);
  static HMODULE GetThemeDLL();

  static void UpdateNativeThemeInfo();
  static bool IsHighContrastOn() { return sIsHighContrastOn; }
};
#endif  // __UXThemeData_h__
