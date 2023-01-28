/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThemeCocoa.h"

#include "cocoa/MacThemeGeometryType.h"
#include "gfxPlatform.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoStyleConsts.h"

namespace mozilla::widget {

LayoutDeviceIntSize ThemeCocoa::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame,
    StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::MozMenulistArrowButton) {
    auto size =
        GetScrollbarSize(aPresContext, StyleScrollbarWidth::Auto, Overlay::No);
    return {size, size};
  }
  return Theme::GetMinimumWidgetSize(aPresContext, aFrame, aAppearance);
}

}  // namespace mozilla::widget
