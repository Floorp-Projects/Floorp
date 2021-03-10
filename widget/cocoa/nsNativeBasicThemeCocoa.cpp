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
  auto thumb = ScrollbarDrawingMac::GetThumbRect(aRect.ToUnknownRect(), params,
                                                 aDpiRatio.scale);
  auto thumbRect = LayoutDeviceRect::FromUnknownRect(thumb.mRect);
  LayoutDeviceCoord radius =
      (params.horizontal ? thumbRect.Height() : thumbRect.Width()) / 2.0f;
  PaintRoundedRectWithRadius(
      aDrawTarget, thumbRect, thumbRect, sRGBColor::FromABGR(thumb.mFillColor),
      sRGBColor::White(0.0f), 0.0f, radius / aDpiRatio, aDpiRatio);
  if (!thumb.mStrokeColor) {
    return true;
  }

  // Paint the stroke if needed.
  thumbRect.Inflate(thumb.mStrokeOutset + thumb.mStrokeWidth);
  radius = (params.horizontal ? thumbRect.Height() : thumbRect.Width()) / 2.0f;
  PaintRoundedRectWithRadius(aDrawTarget, thumbRect,
                             sRGBColor::White(0.0f),
                             sRGBColor::FromABGR(thumb.mStrokeColor),
                             thumb.mStrokeWidth, radius / aDpiRatio, aDpiRatio);
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollbarTrack(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, aHorizontal);
  ScrollbarDrawingMac::ScrollbarTrackRects rects;
  if (ScrollbarDrawingMac::GetScrollbarTrackRects(aRect.ToUnknownRect(), params,
                                                  aDpiRatio.scale, rects)) {
    for (const auto& rect : rects) {
      FillRect(aDrawTarget, LayoutDeviceRect::FromUnknownRect(rect.mRect),
               sRGBColor::FromABGR(rect.mColor));
    }
  }
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollCorner(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, false);
  ScrollbarDrawingMac::ScrollCornerRects rects;
  if (ScrollbarDrawingMac::GetScrollCornerRects(aRect.ToUnknownRect(), params,
                                                aDpiRatio.scale, rects)) {
    for (const auto& rect : rects) {
      FillRect(aDrawTarget, LayoutDeviceRect::FromUnknownRect(rect.mRect),
               sRGBColor::FromABGR(rect.mColor));
    }
  }
  return true;
}
