/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_Screen_h
#define mozilla_widget_Screen_h

#include "nsIScreen.h"

#include "Units.h"
#include "mozilla/HalScreenConfiguration.h"  // For hal::ScreenOrientation

namespace mozilla {
namespace dom {
class ScreenDetails;
}  // namespace dom

namespace widget {

class Screen final : public nsIScreen {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREEN

  using OrientationAngle = uint16_t;

  Screen(LayoutDeviceIntRect aRect, LayoutDeviceIntRect aAvailRect,
         uint32_t aPixelDepth, uint32_t aColorDepth,
         DesktopToLayoutDeviceScale aContentsScale,
         CSSToLayoutDeviceScale aDefaultCssScale, float aDpi,
         hal::ScreenOrientation = hal::ScreenOrientation::None,
         OrientationAngle = 0);
  explicit Screen(const dom::ScreenDetails& aScreenDetails);
  Screen(const Screen& aOther);

  dom::ScreenDetails ToScreenDetails() const;

  OrientationAngle GetOrientationAngle() const { return mOrientationAngle; }
  hal::ScreenOrientation GetOrientationType() const {
    return mScreenOrientation;
  }

  float GetDPI() const { return mDPI; }

 private:
  virtual ~Screen() = default;

  const LayoutDeviceIntRect mRect;
  const LayoutDeviceIntRect mAvailRect;
  const DesktopIntRect mRectDisplayPix;
  const DesktopIntRect mAvailRectDisplayPix;
  const uint32_t mPixelDepth;
  const uint32_t mColorDepth;
  const DesktopToLayoutDeviceScale mContentsScale;
  const CSSToLayoutDeviceScale mDefaultCssScale;
  const float mDPI;
  const hal::ScreenOrientation mScreenOrientation;
  const OrientationAngle mOrientationAngle;
};

}  // namespace widget
}  // namespace mozilla

#endif
