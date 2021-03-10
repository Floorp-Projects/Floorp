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

template <typename PaintBackendData>
void nsNativeBasicThemeCocoa::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
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
      aPaintData, thumbRect, thumbRect, sRGBColor::FromABGR(thumb.mFillColor),
      sRGBColor::White(0.0f), 0.0f, radius / aDpiRatio, aDpiRatio);
  if (!thumb.mStrokeColor) {
    return;
  }

  // Paint the stroke if needed.
  thumbRect.Inflate(thumb.mStrokeOutset + thumb.mStrokeWidth);
  radius = (params.horizontal ? thumbRect.Height() : thumbRect.Width()) / 2.0f;
  PaintRoundedRectWithRadius(aPaintData, thumbRect, sRGBColor::White(0.0f),
                             sRGBColor::FromABGR(thumb.mStrokeColor),
                             thumb.mStrokeWidth, radius / aDpiRatio, aDpiRatio);
}

bool nsNativeBasicThemeCocoa::PaintScrollbarThumb(
    DrawTarget& aDt, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  DoPaintScrollbarThumb(aDt, aRect, aHorizontal, aFrame, aStyle, aElementState,
                        aDocumentState, aDpiRatio);
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                        aElementState, aDocumentState, aDpiRatio);
  return true;
}

template <typename PaintBackendData>
void nsNativeBasicThemeCocoa::DoPaintScrollbarTrack(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, aHorizontal);
  ScrollbarDrawingMac::ScrollbarTrackRects rects;
  if (ScrollbarDrawingMac::GetScrollbarTrackRects(aRect.ToUnknownRect(), params,
                                                  aDpiRatio.scale, rects)) {
    for (const auto& rect : rects) {
      FillRect(aPaintData, LayoutDeviceRect::FromUnknownRect(rect.mRect),
               sRGBColor::FromABGR(rect.mColor));
    }
  }
}

bool nsNativeBasicThemeCocoa::PaintScrollbarTrack(
    DrawTarget& aDt, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  DoPaintScrollbarTrack(aDt, aRect, aHorizontal, aFrame, aStyle, aDocumentState,
                        aDpiRatio);
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollbarTrack(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  DoPaintScrollbarTrack(aWrData, aRect, aHorizontal, aFrame, aStyle,
                        aDocumentState, aDpiRatio);
  return true;
}

template <typename PaintBackendData>
void nsNativeBasicThemeCocoa::DoPaintScrollCorner(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, false);
  ScrollbarDrawingMac::ScrollCornerRects rects;
  if (ScrollbarDrawingMac::GetScrollCornerRects(aRect.ToUnknownRect(), params,
                                                aDpiRatio.scale, rects)) {
    for (const auto& rect : rects) {
      FillRect(aPaintData, LayoutDeviceRect::FromUnknownRect(rect.mRect),
               sRGBColor::FromABGR(rect.mColor));
    }
  }
}

bool nsNativeBasicThemeCocoa::PaintScrollCorner(
    DrawTarget& aDt, const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  DoPaintScrollCorner(aDt, aRect, aFrame, aStyle, aDocumentState, aDpiRatio);
  return true;
}

bool nsNativeBasicThemeCocoa::PaintScrollCorner(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, DPIRatio aDpiRatio) {
  DoPaintScrollCorner(aWrData, aRect, aFrame, aStyle, aDocumentState,
                      aDpiRatio);
  return true;
}
