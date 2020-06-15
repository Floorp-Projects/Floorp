/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinThemeData.h"
#include <windows.h>

namespace mozilla {
namespace widget {

bool WinThemeData::Init(LPCWSTR aClassList) {
  mHandle = ::OpenThemeData(nullptr /*hwnd*/, aClassList);
  return !!mHandle;
}

WinThemeData::~WinThemeData() {
  if (mHandle) {
    MOZ_ALWAYS_TRUE(SUCCEEDED(::CloseThemeData(mHandle)));
  }
}

bool WinThemeData::DrawThemeBackground(HDC aDC, int aPartId, int aStateId,
                                       LPCRECT aRect, LPCRECT aClipRect) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(
      ::DrawThemeBackground(mHandle, aDC, aPartId, aStateId, aRect, aClipRect));
}

bool WinThemeData::DrawThemeBackgroundEx(HDC aDC, int aPartId, int aStateId,
                                         LPCRECT aRect,
                                         const DTBGOPTS* aOptions) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(::DrawThemeBackgroundEx(mHandle, aDC, aPartId, aStateId,
                                           aRect, aOptions));
}

bool WinThemeData::DrawThemeEdge(HDC aDC, int aPartId, int aStateId,
                                 LPCRECT aDestRect, UINT aEdge, UINT aFlags,
                                 LPRECT aContentRect) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(::DrawThemeEdge(mHandle, aDC, aPartId, aStateId, aDestRect,
                                   aEdge, aFlags, aContentRect));
}

bool WinThemeData::GetThemeBackgroundContentRect(HDC aDC, int aPartId,
                                                 int aStateId,
                                                 LPCRECT aBoundingRect,
                                                 LPRECT aContentRect) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(::GetThemeBackgroundContentRect(
      mHandle, aDC, aPartId, aStateId, aBoundingRect, aContentRect));
}

bool WinThemeData::GetThemeBackgroundRegion(HDC aDC, int aPartId, int aStateId,
                                            LPCRECT aRect, HRGN* aRegion) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(::GetThemeBackgroundRegion(mHandle, aDC, aPartId, aStateId,
                                              aRect, aRegion));
}

bool WinThemeData::GetThemeColor(int aPartId, int aStateId, int aPropId,
                                 COLORREF* aColor) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(
      ::GetThemeColor(mHandle, aPartId, aStateId, aPropId, aColor));
}

bool WinThemeData::GetThemeMargins(HDC aDC, int aPartId, int aStateId,
                                   int aPropId, LPCRECT aDrawRect,
                                   MARGINS* aMargins) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(::GetThemeMargins(mHandle, aDC, aPartId, aStateId, aPropId,
                                     aDrawRect, aMargins));
}

bool WinThemeData::GetThemePartSize(HDC aDC, int aPartId, int aStateId,
                                    LPCRECT aDrawRect, THEMESIZE aSizeType,
                                    SIZE* aPartSize) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(::GetThemePartSize(mHandle, aDC, aPartId, aStateId,
                                      aDrawRect, aSizeType, aPartSize));
}

bool WinThemeData::IsThemeBackgroundPartiallyTransparent(int aPartId,
                                                         int aStateId) {
  MOZ_ASSERT(mHandle);
  return SUCCEEDED(
      ::IsThemeBackgroundPartiallyTransparent(mHandle, aPartId, aStateId));
}

}  // namespace widget
}  // namespace mozilla