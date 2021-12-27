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

using mozilla::gfx::sRGBColor;

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
  if (!UseOverlayStyle(aPresContext)) {
    return ScrollbarDrawingWin::GetMinimumWidgetSize(aPresContext, aAppearance,
                                                     aFrame);
  }
  constexpr float kArrowRatio = 14.0f / kDefaultWinScrollbarSize;
  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown: {
      const uint32_t size = GetVerticalScrollbarWidth();
      return LayoutDeviceIntSize{
          size, CSSToScrollbarDeviceSize(kArrowRatio * size, aPresContext)};
    }
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight: {
      const uint32_t size = GetHorizontalScrollbarHeight();
      return LayoutDeviceIntSize{
          CSSToScrollbarDeviceSize(kArrowRatio * size, aPresContext), size};
    }
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
    const EventStates& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  if (!ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame)) {
    return true;
  }

  const bool overlay = UseOverlayStyle(aFrame->PresContext());
  auto [buttonColor, arrowColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  aDrawTarget.FillRect(aRect.ToUnknownRect(),
                       gfx::ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  float arrowPolygonX[] = {-4.5f, 4.5f, 4.5f, 0.5f, -0.5f, -4.5f, -4.5f};
  float arrowPolygonXActive[] = {-4.0f,  4.0f,  4.0f, -0.25f,
                                 -0.25f, -4.0f, -4.0f};
  float arrowPolygonXHover[] = {-5.0f, 5.0f, 5.0f, 0.75f, -0.75f, -5.0f, -5.0f};
  float arrowPolygonY[] = {2.5f, 2.5f, 1.0f, -4.0f, -4.0f, 1.0f, 2.5f};
  float arrowPolygonYActive[] = {2.0f, 2.0f, 0.5f, -3.5f, -3.5f, 0.5f, 2.0f};
  float arrowPolygonYHover[] = {3.0f, 3.0f, 1.5f, -4.5f, -4.5f, 1.5f, 3.0f};
  float* arrowX = arrowPolygonX;
  float* arrowY = arrowPolygonY;
  const bool horizontal =
      aAppearance == StyleAppearance::ScrollbarbuttonRight ||
      aAppearance == StyleAppearance::ScrollbarbuttonLeft;

  const float offset = [&] {
    if (!overlay) {
      return 0.0f;  // Always center it in the rect.
    }
    // Compensate for the displacement we do of the thumb position by displacing
    // the arrow as well, see comment in DoPaintScrollbarThumb.
    if (horizontal) {
      return -0.5f;
    }
    return aFrame->GetWritingMode().IsPhysicalLTR() ? 0.5f : -0.5f;
  }();
  const float kPolygonSize = kDefaultWinScrollbarSize;
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
    case StyleAppearance::ScrollbarbuttonRight:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowY[i] *= -1;
      }
      [[fallthrough]];
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonLeft:
      if (offset != 0.0f) {
        for (int32_t i = 0; i < arrowNumPoints; i++) {
          arrowX[i] += offset;
        }
      }
      break;
    default:
      return false;
  }

  if (horizontal) {
    std::swap(arrowX, arrowY);
  }

  LayoutDeviceRect arrowRect(aRect);
  if (!overlay) {
    auto margin = CSSCoord(2) * aDpiRatio;
    arrowRect.Deflate(margin, margin);
  }

  ThemeDrawing::PaintArrow(aDrawTarget, arrowRect, arrowX, arrowY, kPolygonSize,
                           arrowNumPoints, arrowColor);
  return true;
}

bool ScrollbarDrawingWin11::UseOverlayStyle(nsPresContext* aPresContext) {
  return StaticPrefs::
             widget_non_native_theme_win11_scrollbar_force_overlay_style() ||
         aPresContext->UseOverlayScrollbars();
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

  const bool overlay = UseOverlayStyle(aFrame->PresContext());
  const bool hovered =
      ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame);
  if (!overlay) {
    constexpr float kHoveredThumbRatio =
        (1.0f - (11.0f / kDefaultWinScrollbarSize)) / 2.0f;
    constexpr float kUnhoveredThumbRatio =
        (1.0f - (9.0f / kDefaultWinScrollbarSize)) / 2.0f;
    const float ratio = hovered ? kHoveredThumbRatio : kUnhoveredThumbRatio;
    if (aHorizontal) {
      thumbRect.Deflate(0, thumbRect.height * ratio);
    } else {
      thumbRect.Deflate(thumbRect.width * ratio, 0);
    }

    auto radius = CSSCoord(hovered ? 2 : 0);
    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor,
                                             sRGBColor(), 0, radius, aDpiRatio);
    return true;
  }

  if (hovered) {
    constexpr float kHoverThumbSize = 6.0f / kDefaultWinScrollbarSize;
    if (aHorizontal) {
      // Scrollbar is 17px high by default. We make the thumb 6px tall and move
      // it 5px towards the bottom, so the center (8.5 initially) is displaced
      // by:
      //   (5px + 6px / 2) - 8.5px = -0.5px
      constexpr float kShift = 5.0f / kDefaultWinScrollbarSize;

      thumbRect.y += thumbRect.height * kShift;
      thumbRect.height *= kHoverThumbSize;
    } else {
      // Scrollbar is 17px wide by default. We make the thumb 6px wide and move
      // it 5px or 6px towards the right (depending on writing-mode), so the
      // center (8.5px initially) is displaced by:
      //   (6px + 6px / 2) - 8.5px = 0.5px for LTR
      //   (5px + 6px / 2) - 8.5px = -0.5px for RTL
      constexpr float kLtrShift = 6.0f / kDefaultWinScrollbarSize;
      constexpr float kRtlShift = 5.0f / kDefaultWinScrollbarSize;

      if (aFrame->GetWritingMode().IsPhysicalLTR()) {
        thumbRect.x += thumbRect.width * kLtrShift;
      } else {
        thumbRect.x += thumbRect.width * kRtlShift;
      }
      thumbRect.width *= kHoverThumbSize;
    }
    LayoutDeviceCoord radius =
        (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f;

    MOZ_ASSERT(aRect.Contains(thumbRect));

    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor,
                                             sRGBColor(), 0, radius / aDpiRatio,
                                             aDpiRatio);
    return true;
  }

  constexpr float kThumbSize = 2.0f / kDefaultWinScrollbarSize;
  if (aHorizontal) {
    constexpr float kShift = 7.0f / kDefaultWinScrollbarSize;
    thumbRect.y += thumbRect.height * kShift;
    thumbRect.height *= kThumbSize;
  } else {
    constexpr float kLtrShift = 8.0f / kDefaultWinScrollbarSize;
    constexpr float kRtlShift = 7.0f / kDefaultWinScrollbarSize;

    if (aFrame->GetWritingMode().IsPhysicalLTR()) {
      thumbRect.x += thumbRect.width * kLtrShift;
    } else {
      thumbRect.x += thumbRect.width * kRtlShift;
    }
    thumbRect.width *= kThumbSize;
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
