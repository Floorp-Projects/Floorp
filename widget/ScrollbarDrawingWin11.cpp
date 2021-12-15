/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingWin11.h"

#include "mozilla/gfx/Helpers.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLayoutUtils.h"
#include "nsNativeBasicTheme.h"
#include "nsNativeTheme.h"

namespace mozilla::widget {

static LayoutDeviceIntCoord CSSToScrollbarDeviceSize(
    CSSCoord aCoord, nsPresContext* aPresContext) {
  return (aCoord * ScrollbarDrawing::GetDPIRatioForScrollbarPart(aPresContext))
      .Rounded();
}

LayoutDeviceIntSize ScrollbarDrawingWin11::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));

  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
      return LayoutDeviceIntSize{GetVerticalScrollbarWidth(),
                                 CSSToScrollbarDeviceSize(14, aPresContext)};
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      return LayoutDeviceIntSize{CSSToScrollbarDeviceSize(14, aPresContext),
                                 GetHorizontalScrollbarHeight()};
    default:
      return ScrollbarDrawingWin::GetMinimumWidgetSize(aPresContext,
                                                       aAppearance, aFrame);
  }
}

sRGBColor ScrollbarDrawingWin11::ComputeScrollbarTrackColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ScrollbarDrawingWin::ComputeScrollbarTrackColor(
        aFrame, aStyle, aDocumentState, aColors);
  }
  if (ShouldUseDarkScrollbar(aFrame, aStyle)) {
    return sRGBColor::FromU8(23, 23, 23, 255);
  }
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(
        ui->mScrollbarColor.AsColors().track.CalcColor(aStyle));
  }
  return sRGBColor::FromU8(240, 240, 240, 255);
}

sRGBColor ScrollbarDrawingWin11::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ScrollbarDrawingWin::ComputeScrollbarThumbColor(
        aFrame, aStyle, aElementState, aDocumentState, aColors);
  }
  if (ShouldUseDarkScrollbar(aFrame, aStyle)) {
    return sRGBColor::FromU8(149, 149, 149, 255);
  }
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle), aElementState));
  }
  return sRGBColor::FromU8(133, 133, 133, 255);
}

std::pair<sRGBColor, sRGBColor>
ScrollbarDrawingWin11::ComputeScrollbarButtonColors(
    nsIFrame* aFrame, StyleAppearance aAppearance, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ScrollbarDrawingWin::ComputeScrollbarButtonColors(
        aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  }

  auto buttonColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  sRGBColor arrowColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aColors);

  if (aElementState.HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER |
                                          NS_EVENT_STATE_ACTIVE)) {
    if (ShouldUseDarkScrollbar(aFrame, aStyle)) {
      arrowColor = sRGBColor::FromU8(205, 205, 205, 255);
    } else {
      arrowColor = sRGBColor::FromU8(102, 102, 102, 255);
    }
  }
  return {buttonColor, arrowColor};
}

bool ScrollbarDrawingWin11::PaintScrollbarButton(
    DrawTarget& aDrawTarget, StyleAppearance aAppearance,
    const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aElementState,
    const EventStates& aDocumentState, const Colors& aColors) {
  if (!ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame)) {
    return true;
  }

  auto [buttonColor, arrowColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  aDrawTarget.FillRect(aRect.ToUnknownRect(),
                       ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  float arrowPolygonX[] = {-5.5f, 3.5f, 3.5f, -0.5f, -1.5f, -5.5f, -5.5f};
  float arrowPolygonXActive[] = {-5.0f,  3.0f,  3.0f, -0.75f,
                                 -1.25f, -5.0f, -5.0f};
  float arrowPolygonXHover[] = {-6.0f,  4.0f,  4.0f, -0.25f,
                                -1.75f, -6.0f, -6.0f};
  float arrowPolygonY[] = {2.5f, 2.5f, 1.0f, -4.0f, -4.0f, 1.0f, 2.5f};
  float arrowPolygonYActive[] = {2.0f, 2.0f, 0.5f, -3.5f, -3.5f, 0.5f, 2.0f};
  float arrowPolygonYHover[] = {3.0f, 3.0f, 1.5f, -4.5f, -4.5f, 1.5f, 3.0f};
  float* arrowX = arrowPolygonX;
  float* arrowY = arrowPolygonY;
  const float offset = aFrame->GetWritingMode().IsPhysicalLTR() ? 1.5f : -1.5f;
  const float kPolygonSize = 17;
  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);

  if (aElementState.HasState(NS_EVENT_STATE_ACTIVE)) {
    arrowX = arrowPolygonXActive;
    arrowY = arrowPolygonYActive;
  } else if (aElementState.HasState(NS_EVENT_STATE_HOVER)) {
    arrowX = arrowPolygonXHover;
    arrowY = arrowPolygonYHover;
  }

  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonDown:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowY[i] *= -1;
      }
      [[fallthrough]];
    case StyleAppearance::ScrollbarbuttonUp:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowX[i] += offset;
      }
      break;
    case StyleAppearance::ScrollbarbuttonRight:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowX[i] *= -1;
      }
      [[fallthrough]];
    case StyleAppearance::ScrollbarbuttonLeft:
      std::swap(arrowX, arrowY);
      break;
    default:
      return false;
  }

  ThemeDrawing::PaintArrow(aDrawTarget, aRect, arrowX, arrowY, kPolygonSize,
                           arrowNumPoints, arrowColor);
  return true;
}

template <typename PaintBackendData>
bool ScrollbarDrawingWin11::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  sRGBColor thumbColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aColors);

  LayoutDeviceRect thumbRect(aRect);

  if (ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame)) {
    if (aHorizontal) {
      thumbRect.height = 6 * aDpiRatio.scale;
      thumbRect.y += 5 * aDpiRatio.scale;
    } else {
      thumbRect.width = 6 * aDpiRatio.scale;
      if (aFrame->GetWritingMode().IsPhysicalLTR()) {
        thumbRect.x += 6 * aDpiRatio.scale;
      } else {
        thumbRect.x += 5 * aDpiRatio.scale;
      }
    }
    LayoutDeviceCoord radius =
        (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f;

    MOZ_ASSERT(aRect.Contains(thumbRect));

    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor,
                                             sRGBColor(), 0, radius / aDpiRatio,
                                             aDpiRatio);
    return true;
  }

  if (aHorizontal) {
    thumbRect.height = 2 * aDpiRatio.scale;
    thumbRect.y += 7 * aDpiRatio.scale;
  } else {
    thumbRect.width = 2 * aDpiRatio.scale;
    if (aFrame->GetWritingMode().IsPhysicalLTR()) {
      thumbRect.x += 8 * aDpiRatio.scale;
    } else {
      thumbRect.x += 7 * aDpiRatio.scale;
    }
  }
  ThemeDrawing::FillRect(aPaintData, thumbRect, thumbColor);
  return true;
}

bool ScrollbarDrawingWin11::PaintScrollbarThumb(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aDrawTarget, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

bool ScrollbarDrawingWin11::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

}  // namespace mozilla::widget
