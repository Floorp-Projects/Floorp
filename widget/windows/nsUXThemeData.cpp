/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/WindowsVersion.h"

#include "nsUXThemeData.h"
#include "nsDebug.h"
#include "nsToolkit.h"
#include "nsUXThemeConstants.h"
#include "gfxWindowsPlatform.h"

using namespace mozilla;
using namespace mozilla::widget;

nsUXThemeData::ThemeHandle nsUXThemeData::sThemes[eUXNumClasses];

nsUXThemeData::ThemeHandle::~ThemeHandle() { Close(); }

void nsUXThemeData::ThemeHandle::OpenOnce(HWND aWindow, LPCWSTR aClassList) {
  if (mHandle.isSome()) {
    return;
  }

  mHandle = Some(OpenThemeData(aWindow, aClassList));
}

void nsUXThemeData::ThemeHandle::Close() {
  if (mHandle.isNothing()) {
    return;
  }

  if (HANDLE rawHandle = mHandle.extract()) {
    CloseThemeData(rawHandle);
  }
}

nsUXThemeData::ThemeHandle::operator HANDLE() {
  return mHandle.valueOr(nullptr);
}

void nsUXThemeData::Invalidate() {
  for (auto& theme : sThemes) {
    theme.Close();
  }
}

HANDLE
nsUXThemeData::GetTheme(nsUXThemeClass cls) {
  NS_ASSERTION(cls < eUXNumClasses, "Invalid theme class!");
  sThemes[cls].OpenOnce(nullptr, GetClassName(cls));
  return sThemes[cls];
}

const wchar_t* nsUXThemeData::GetClassName(nsUXThemeClass cls) {
  switch (cls) {
    case eUXButton:
      return L"Button";
    case eUXEdit:
      return L"Edit";
    case eUXRebar:
      return L"Rebar";
    case eUXToolbar:
      return L"Toolbar";
    case eUXProgress:
      return L"Progress";
    case eUXTab:
      return L"Tab";
    case eUXTrackbar:
      return L"Trackbar";
    case eUXCombobox:
      return L"Combobox";
    case eUXHeader:
      return L"Header";
    case eUXListview:
      return L"Listview";
    case eUXMenu:
      return L"Menu";
    default:
      MOZ_ASSERT_UNREACHABLE("unknown uxtheme class");
      return L"";
  }
}

bool nsUXThemeData::sIsHighContrastOn = false;

// static
void nsUXThemeData::UpdateNativeThemeInfo() {
  HIGHCONTRAST highContrastInfo;
  highContrastInfo.cbSize = sizeof(HIGHCONTRAST);
  sIsHighContrastOn =
      SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &highContrastInfo, 0) &&
      highContrastInfo.dwFlags & HCF_HIGHCONTRASTON;
}
