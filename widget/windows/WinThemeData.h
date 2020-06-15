/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_WIDGET_WINDOWS_WINTHEMEDATA_H_
#define MOZILLA_WIDGET_WINDOWS_WINTHEMEDATA_H_
#include <windows.h>

namespace mozilla {
namespace widget {

class WinThemeData {
 public:
  bool Init(LPCWSTR aClassList);
  ~WinThemeData();

  bool DrawThemeBackground(HDC aDC, int aPartId, int aStateId, LPCRECT aRect,
                           LPCRECT aClipRect);

  bool DrawThemeBackgroundEx(HDC aDC, int aPartId, int aStateId, LPCRECT aRect,
                             const DTBGOPTS* aOptions);

  bool DrawThemeEdge(HDC aDC, int aPartId, int aStateId, LPCRECT aDestRect,
                     UINT aEdge, UINT aFlags, LPRECT aContentRect);

  bool GetThemeBackgroundContentRect(HDC aDC, int aPartId, int aStateId,
                                     LPCRECT aBoundingRect,
                                     LPRECT aContentRect);

  bool GetThemeBackgroundRegion(HDC aDC, int aPartId, int aStateId,
                                LPCRECT aRect, HRGN* aRegion);

  bool GetThemeColor(int aPartId, int aStateId, int aPropId, COLORREF* aColor);

  bool GetThemeMargins(HDC aDC, int aPartId, int aStateId, int aPropId,
                       LPCRECT aDrawRect, MARGINS* aMargins);

  bool GetThemePartSize(HDC aDC, int aPartId, int aStateId, LPCRECT aDrawRect,
                        THEMESIZE aSizeType, SIZE* aPartSize);

  bool IsThemeBackgroundPartiallyTransparent(int aPartId, int aStateId);

  WinThemeData() = default;
  WinThemeData(const WinThemeData&) = delete;
  WinThemeData(WinThemeData&&) = delete;
  WinThemeData& operator=(const WinThemeData&) = delete;
  WinThemeData& operator=(WinThemeData&&) = delete;

 private:
  HTHEME mHandle{nullptr};
};

// This class exists to prevent a WinThemeData* from degrading into a void*
// and being passed directly into Win32 functions by accident
// It will be removed when I merge the code
class WinThemeDataPtr {
 public:
  explicit WinThemeDataPtr(WinThemeData* impl) : mImpl(impl) {}
  MOZ_IMPLICIT WinThemeDataPtr(nullptr_t) : mImpl(nullptr) {}
  ~WinThemeDataPtr() = default;

  WinThemeDataPtr(const WinThemeDataPtr&) = default;
  WinThemeDataPtr(WinThemeDataPtr&&) = default;
  WinThemeDataPtr& operator=(const WinThemeDataPtr&) = default;
  WinThemeDataPtr& operator=(WinThemeDataPtr&&) = default;

  WinThemeData* operator->() { return mImpl; }
  MOZ_IMPLICIT operator bool() { return !!mImpl; }

 private:
  WinThemeData* mImpl;
};

}  // namespace widget
}  // namespace mozilla

#endif  // MOZILLA_WIDGET_WINDOWS_WINTHEMEDATA_H_