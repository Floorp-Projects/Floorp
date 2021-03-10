/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeCocoa.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoStyleConsts.h"
#include "MacThemeGeometryType.h"

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static mozilla::StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeBasicThemeCocoa();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}

NS_IMETHODIMP
nsNativeBasicThemeCocoa::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    mozilla::LayoutDeviceIntSize* aResult, bool* aIsOverridable) {
  if (!IsWidgetScrollbarPart(aAppearance)) {
    return nsNativeBasicTheme::GetMinimumWidgetSize(
        aPresContext, aFrame, aAppearance, aResult, aIsOverridable);
  }

  DPIRatio dpiRatio = GetDPIRatioForScrollbarPart(aPresContext);
  *aIsOverridable = false;
  *aResult = ScrollbarDrawingMac::GetMinimumWidgetSize(aAppearance, aFrame,
                                                       dpiRatio.scale);
  return NS_OK;
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

auto nsNativeBasicThemeCocoa::GetScrollbarSizes(nsPresContext* aPresContext,
                                                StyleScrollbarWidth aWidth,
                                                Overlay aOverlay)
    -> ScrollbarSizes {
  auto size = ScrollbarDrawingMac::GetScrollbarSize(
      aWidth, aOverlay == Overlay::Yes,
      GetDPIRatioForScrollbarPart(aPresContext).scale);
  return {size, size};
}

bool nsNativeBasicThemeCocoa::PaintScrollbarThumb(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, aHorizontal);
  auto rect = aRect.ToUnknownRect();
  if (aDpiRatio.scale >= 2.0f) {
    mozilla::gfx::AutoRestoreTransform autoRestoreTransform(&aDrawTarget);
    aDrawTarget.SetTransform(aDrawTarget.GetTransform().PreScale(2.0f, 2.0f));
    rect.Scale(1.0f / 2.0f);
    ScrollbarDrawingMac::DrawScrollbarThumb(aDrawTarget, rect, params);
  } else {
    ScrollbarDrawingMac::DrawScrollbarThumb(aDrawTarget, rect, params);
  }
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollbarTrack(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, aHorizontal);
  auto rect = aRect.ToUnknownRect();
  if (aDpiRatio.scale >= 2.0f) {
    mozilla::gfx::AutoRestoreTransform autoRestoreTransform(&aDrawTarget);
    aDrawTarget.SetTransform(aDrawTarget.GetTransform().PreScale(2.0f, 2.0f));
    rect.Scale(1.0f / 2.0f);
    ScrollbarDrawingMac::DrawScrollbarTrack(aDrawTarget, rect, params);
  } else {
    ScrollbarDrawingMac::DrawScrollbarTrack(aDrawTarget, rect, params);
  }
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollCorner(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, false);
  if (aDpiRatio.scale >= 2.0f) {
    mozilla::gfx::AutoRestoreTransform autoRestoreTransform(&aDrawTarget);
    aDrawTarget.SetTransform(aDrawTarget.GetTransform().PreScale(2.0f, 2.0f));
    auto rect = aRect.ToUnknownRect();
    rect.Scale(1 / 2.0f);
    ScrollbarDrawingMac::DrawScrollCorner(aDrawTarget, rect, params);
  } else {
    auto rect = aRect.ToUnknownRect();
    ScrollbarDrawingMac::DrawScrollCorner(aDrawTarget, rect, params);
  }
  return true;
}
