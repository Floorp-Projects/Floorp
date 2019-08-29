/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Screen.h"

#include "mozilla/dom/DOMTypes.h"
#include "mozilla/StaticPrefs_layout.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(Screen, nsIScreen)

Screen::Screen(LayoutDeviceIntRect aRect, LayoutDeviceIntRect aAvailRect,
               uint32_t aPixelDepth, uint32_t aColorDepth,
               DesktopToLayoutDeviceScale aContentsScale,
               CSSToLayoutDeviceScale aDefaultCssScale, float aDPI)
    : mRect(aRect),
      mAvailRect(aAvailRect),
      mRectDisplayPix(RoundedToInt(aRect / aContentsScale)),
      mAvailRectDisplayPix(RoundedToInt(aAvailRect / aContentsScale)),
      mPixelDepth(aPixelDepth),
      mColorDepth(aColorDepth),
      mContentsScale(aContentsScale),
      mDefaultCssScale(aDefaultCssScale),
      mDPI(aDPI) {}

Screen::Screen(const mozilla::dom::ScreenDetails& aScreen)
    : mRect(aScreen.rect()),
      mAvailRect(aScreen.availRect()),
      mRectDisplayPix(aScreen.rectDisplayPix()),
      mAvailRectDisplayPix(aScreen.availRectDisplayPix()),
      mPixelDepth(aScreen.pixelDepth()),
      mColorDepth(aScreen.colorDepth()),
      mContentsScale(aScreen.contentsScaleFactor()),
      mDefaultCssScale(aScreen.defaultCSSScaleFactor()),
      mDPI(aScreen.dpi()) {}

Screen::Screen(const Screen& aOther)
    : mRect(aOther.mRect),
      mAvailRect(aOther.mAvailRect),
      mRectDisplayPix(aOther.mRectDisplayPix),
      mAvailRectDisplayPix(aOther.mAvailRectDisplayPix),
      mPixelDepth(aOther.mPixelDepth),
      mColorDepth(aOther.mColorDepth),
      mContentsScale(aOther.mContentsScale),
      mDefaultCssScale(aOther.mDefaultCssScale),
      mDPI(aOther.mDPI) {}

mozilla::dom::ScreenDetails Screen::ToScreenDetails() {
  return mozilla::dom::ScreenDetails(
      mRect, mRectDisplayPix, mAvailRect, mAvailRectDisplayPix, mPixelDepth,
      mColorDepth, mContentsScale, mDefaultCssScale, mDPI);
}

NS_IMETHODIMP
Screen::GetRect(int32_t* aOutLeft, int32_t* aOutTop, int32_t* aOutWidth,
                int32_t* aOutHeight) {
  mRect.GetRect(aOutLeft, aOutTop, aOutWidth, aOutHeight);
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetRectDisplayPix(int32_t* aOutLeft, int32_t* aOutTop,
                          int32_t* aOutWidth, int32_t* aOutHeight) {
  mRectDisplayPix.GetRect(aOutLeft, aOutTop, aOutWidth, aOutHeight);
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetAvailRect(int32_t* aOutLeft, int32_t* aOutTop, int32_t* aOutWidth,
                     int32_t* aOutHeight) {
  mAvailRect.GetRect(aOutLeft, aOutTop, aOutWidth, aOutHeight);
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetAvailRectDisplayPix(int32_t* aOutLeft, int32_t* aOutTop,
                               int32_t* aOutWidth, int32_t* aOutHeight) {
  mAvailRectDisplayPix.GetRect(aOutLeft, aOutTop, aOutWidth, aOutHeight);
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetPixelDepth(int32_t* aPixelDepth) {
  *aPixelDepth = mPixelDepth;
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetColorDepth(int32_t* aColorDepth) {
  *aColorDepth = mColorDepth;
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetContentsScaleFactor(double* aOutScale) {
  *aOutScale = mContentsScale.scale;
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetDefaultCSSScaleFactor(double* aOutScale) {
  double scale = StaticPrefs::layout_css_devPixelsPerPx();
  if (scale > 0.0) {
    *aOutScale = scale;
  } else {
    *aOutScale = mDefaultCssScale.scale;
  }
  return NS_OK;
}

NS_IMETHODIMP
Screen::GetDpi(float* aDPI) {
  *aDPI = mDPI;
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
