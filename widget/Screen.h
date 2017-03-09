/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_Screen_h
#define mozilla_widget_Screen_h

#include "nsIScreen.h"

#include "Units.h"

namespace mozilla {
namespace dom {
class ScreenDetails;
} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace widget {

class Screen final : public nsIScreen
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREEN

  Screen(LayoutDeviceIntRect aRect, LayoutDeviceIntRect aAvailRect,
         uint32_t aPixelDepth, uint32_t aColorDepth,
         DesktopToLayoutDeviceScale aContentsScale,
         CSSToLayoutDeviceScale aDefaultCssScale);
  explicit Screen(const mozilla::dom::ScreenDetails& aScreenDetails);
  Screen(const Screen& aOther);

  mozilla::dom::ScreenDetails ToScreenDetails();

private:
  virtual ~Screen() {}

  LayoutDeviceIntRect mRect;
  LayoutDeviceIntRect mAvailRect;
  DesktopIntRect mRectDisplayPix;
  DesktopIntRect mAvailRectDisplayPix;
  uint32_t mPixelDepth;
  uint32_t mColorDepth;
  DesktopToLayoutDeviceScale mContentsScale;
  CSSToLayoutDeviceScale mDefaultCssScale;
};

} // namespace widget
} // namespace mozilla

#endif

