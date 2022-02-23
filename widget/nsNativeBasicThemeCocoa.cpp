/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeCocoa.h"

#include "cocoa/MacThemeGeometryType.h"
#include "gfxPlatform.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoStyleConsts.h"

using ScrollbarDrawingCocoa = mozilla::widget::ScrollbarDrawingCocoa;
using namespace mozilla::gfx;

NS_IMETHODIMP
nsNativeBasicThemeCocoa::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    mozilla::LayoutDeviceIntSize* aResult, bool* aIsOverridable) {
  if (aAppearance == StyleAppearance::MozMenulistArrowButton) {
    auto size = ScrollbarDrawingCocoa::GetScrollbarSize(
        StyleScrollbarWidth::Auto, /* aOverlay = */ false,
        GetDPIRatio(aFrame, aAppearance));
    aResult->SizeTo(size, size);
    return NS_OK;
  }

  return nsNativeBasicTheme::GetMinimumWidgetSize(
      aPresContext, aFrame, aAppearance, aResult, aIsOverridable);
}

nsITheme::ThemeGeometryType nsNativeBasicThemeCocoa::ThemeGeometryTypeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Tooltip:
      return eThemeGeometryTypeTooltip;
    default:
      break;
  }
  return nsNativeBasicTheme::ThemeGeometryTypeForWidget(aFrame, aAppearance);
}

bool nsNativeBasicThemeCocoa::ThemeSupportsWidget(nsPresContext* aPc,
                                                  nsIFrame* aFrame,
                                                  StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Tooltip:
      return true;
    default:
      break;
  }
  return nsNativeBasicTheme::ThemeSupportsWidget(aPc, aFrame, aAppearance);
}
