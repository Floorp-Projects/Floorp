/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawing.h"

#include "mozilla/RelativeLuminanceUtils.h"
#include "nsContainerFrame.h"
#include "nsDeviceContext.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsLookAndFeel.h"
#include "nsNativeTheme.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

using ScrollbarParams = ScrollbarDrawing::ScrollbarParams;
using mozilla::RelativeLuminanceUtils;

/* static */
auto ScrollbarDrawing::GetDPIRatioForScrollbarPart(nsPresContext* aPc)
    -> DPIRatio {
  if (auto* rootPc = aPc->GetRootPresContext()) {
    if (nsCOMPtr<nsIWidget> widget = rootPc->GetRootWidget()) {
      return widget->GetDefaultScale();
    }
  }
  return DPIRatio(
      float(AppUnitsPerCSSPixel()) /
      float(aPc->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom()));
}

/*static*/
nsIFrame* ScrollbarDrawing::GetParentScrollbarFrame(nsIFrame* aFrame) {
  // Walk our parents to find a scrollbar frame
  nsIFrame* scrollbarFrame = aFrame;
  do {
    if (scrollbarFrame->IsScrollbarFrame()) {
      break;
    }
  } while ((scrollbarFrame = scrollbarFrame->GetParent()));

  // We return null if we can't find a parent scrollbar frame
  return scrollbarFrame;
}

/*static*/
bool ScrollbarDrawing::IsParentScrollbarRolledOver(nsIFrame* aFrame) {
  nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
  return nsLookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars) != 0
             ? nsNativeTheme::CheckBooleanAttr(scrollbarFrame, nsGkAtoms::hover)
             : nsNativeTheme::GetContentState(scrollbarFrame,
                                              StyleAppearance::None)
                   .HasState(NS_EVENT_STATE_HOVER);
}

/*static*/
bool ScrollbarDrawing::IsParentScrollbarHoveredOrActive(nsIFrame* aFrame) {
  nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
  return scrollbarFrame && scrollbarFrame->GetContent()
                               ->AsElement()
                               ->State()
                               .HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER |
                                                      NS_EVENT_STATE_ACTIVE);
}

/*static*/
bool ScrollbarDrawing::IsScrollbarWidthThin(const ComputedStyle& aStyle) {
  auto scrollbarWidth = aStyle.StyleUIReset()->mScrollbarWidth;
  return scrollbarWidth == StyleScrollbarWidth::Thin;
}

/*static*/
bool ScrollbarDrawing::IsScrollbarWidthThin(nsIFrame* aFrame) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  return IsScrollbarWidthThin(*style);
}

auto ScrollbarDrawing::GetScrollbarSizes(nsPresContext* aPresContext,
                                         StyleScrollbarWidth aWidth, Overlay)
    -> ScrollbarSizes {
  uint32_t h = GetHorizontalScrollbarHeight();
  uint32_t w = GetVerticalScrollbarWidth();
  if (aWidth == StyleScrollbarWidth::Thin) {
    h /= 2;
    w /= 2;
  }
  auto dpi = GetDPIRatioForScrollbarPart(aPresContext);
  return {(CSSCoord(w) * dpi).Rounded(), (CSSCoord(h) * dpi).Rounded()};
}

/*static*/
bool ScrollbarDrawing::IsScrollbarTrackOpaque(nsIFrame* aFrame) {
  auto trackColor = ComputeScrollbarTrackColor(
      aFrame, *nsLayoutUtils::StyleForScrollbar(aFrame),
      aFrame->PresContext()->Document()->GetDocumentState(), Colors(aFrame));
  return trackColor.a == 1.0f;
}

/*static*/
sRGBColor ScrollbarDrawing::ComputeScrollbarTrackColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, const Colors& aColors) {
  const nsStyleUI* ui = aStyle.StyleUI();
  if (aColors.HighContrast()) {
    return aColors.System(StyleSystemColor::Window);
  }
  if (ShouldUseDarkScrollbar(aFrame, aStyle)) {
    return sRGBColor::FromU8(20, 20, 25, 77);
  }
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(
        ui->mScrollbarColor.AsColors().track.CalcColor(aStyle));
  }
  if (aDocumentState.HasAllStates(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    return aColors.SystemOrElse(StyleSystemColor::ThemedScrollbarInactive,
                                [] { return sScrollbarColor; });
  }
  return aColors.SystemOrElse(StyleSystemColor::ThemedScrollbar,
                              [] { return sScrollbarColor; });
}

// Don't use the theme color for dark scrollbars if it's not a color (if it's
// grey-ish), as that'd either lack enough contrast, or be close to what we'd do
// by default anyways.
static bool ShouldUseColorForActiveDarkScrollbarThumb(nscolor aColor) {
  auto IsDifferentEnough = [](int32_t aChannel, int32_t aOtherChannel) {
    return std::abs(aChannel - aOtherChannel) > 10;
  };
  return IsDifferentEnough(NS_GET_R(aColor), NS_GET_G(aColor)) ||
         IsDifferentEnough(NS_GET_R(aColor), NS_GET_B(aColor));
}

/*static*/
sRGBColor ScrollbarDrawing::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors) {
  if (!aColors.HighContrast() && ShouldUseDarkScrollbar(aFrame, aStyle)) {
    if (aElementState.HasState(NS_EVENT_STATE_ACTIVE) &&
        StaticPrefs::widget_non_native_theme_scrollbar_active_always_themed()) {
      auto color = LookAndFeel::GetColor(
          StyleSystemColor::ThemedScrollbarThumbActive,
          LookAndFeel::ColorScheme::Light, LookAndFeel::UseStandins::No);
      if (color && ShouldUseColorForActiveDarkScrollbarThumb(*color)) {
        return sRGBColor::FromABGR(*color);
      }
    }
    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        NS_RGBA(249, 249, 250, 102), aElementState));
  }

  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle), aElementState));
  }

  auto systemColor = [&] {
    if (aDocumentState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
      return StyleSystemColor::ThemedScrollbarThumbInactive;
    }
    if (aElementState.HasState(NS_EVENT_STATE_ACTIVE)) {
      if (aColors.HighContrast()) {
        return StyleSystemColor::Selecteditem;
      }
      return StyleSystemColor::ThemedScrollbarThumbActive;
    }
    if (aElementState.HasState(NS_EVENT_STATE_HOVER)) {
      if (aColors.HighContrast()) {
        return StyleSystemColor::Selecteditem;
      }
      return StyleSystemColor::ThemedScrollbarThumbHover;
    }
    if (aColors.HighContrast()) {
      return StyleSystemColor::Windowtext;
    }
    return StyleSystemColor::ThemedScrollbarThumb;
  }();

  return aColors.SystemOrElse(systemColor, [&] {
    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        sScrollbarThumbColor.ToABGR(), aElementState));
  });
}

/*static*/
ScrollbarParams ScrollbarDrawing::ComputeScrollbarParams(
    nsIFrame* aFrame, const ComputedStyle& aStyle, bool aIsHorizontal) {
  ScrollbarParams params;
  params.isOverlay =
      nsLookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars) != 0;
  params.isRolledOver = IsParentScrollbarRolledOver(aFrame);
  params.isSmall =
      aStyle.StyleUIReset()->mScrollbarWidth == StyleScrollbarWidth::Thin;
  params.isRtl = nsNativeTheme::IsFrameRTL(aFrame);
  params.isHorizontal = aIsHorizontal;
  params.isOnDarkBackground = !StaticPrefs::widget_disable_dark_scrollbar() &&
                              nsNativeTheme::IsDarkBackground(aFrame);
  // Don't use custom scrollbars for overlay scrollbars since they are
  // generally good enough for use cases of custom scrollbars.
  if (!params.isOverlay) {
    const nsStyleUI* ui = aStyle.StyleUI();
    if (ui->HasCustomScrollbars()) {
      const auto& colors = ui->mScrollbarColor.AsColors();
      params.isCustom = true;
      params.trackColor = colors.track.CalcColor(aStyle);
      params.faceColor = colors.thumb.CalcColor(aStyle);
    }
  }

  return params;
}

template <typename PaintBackendData>
bool ScrollbarDrawing::DoPaintDefaultScrollbar(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  if (aFrame->PresContext()->UseOverlayScrollbars() &&
      !aElementState.HasAtLeastOneOfStates(NS_EVENT_STATE_HOVER |
                                           NS_EVENT_STATE_ACTIVE)) {
    return true;
  }
  auto scrollbarColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  ThemeDrawing::FillRect(aPaintData, aRect, scrollbarColor);
  return true;
}

bool ScrollbarDrawing::PaintScrollbar(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollbar(aDrawTarget, aRect, aHorizontal, aFrame,
                                 aStyle, aElementState, aDocumentState, aColors,
                                 aDpiRatio);
}

bool ScrollbarDrawing::PaintScrollbar(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollbar(aWrData, aRect, aHorizontal, aFrame, aStyle,
                                 aElementState, aDocumentState, aColors,
                                 aDpiRatio);
}

template <typename PaintBackendData>
bool ScrollbarDrawing::DoPaintDefaultScrollCorner(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  auto scrollbarColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  ThemeDrawing::FillRect(aPaintData, aRect, scrollbarColor);
  return true;
}

bool ScrollbarDrawing::PaintScrollCorner(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollCorner(aDrawTarget, aRect, aFrame, aStyle,
                                    aDocumentState, aColors, aDpiRatio);
}

bool ScrollbarDrawing::PaintScrollCorner(WebRenderBackendData& aWrData,
                                         const LayoutDeviceRect& aRect,
                                         nsIFrame* aFrame,
                                         const ComputedStyle& aStyle,
                                         const EventStates& aDocumentState,
                                         const Colors& aColors,
                                         const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollCorner(aWrData, aRect, aFrame, aStyle,
                                    aDocumentState, aColors, aDpiRatio);
}

/*static*/
bool ScrollbarDrawing::ShouldUseDarkScrollbar(nsIFrame* aFrame,
                                              const ComputedStyle& aStyle) {
  if (StaticPrefs::widget_disable_dark_scrollbar()) {
    return false;
  }
  if (aStyle.StyleUI()->mScrollbarColor.IsColors()) {
    return false;
  }
  return nsNativeTheme::IsDarkBackground(aFrame);
}

/*static*/
nscolor ScrollbarDrawing::GetScrollbarButtonColor(nscolor aTrackColor,
                                                  EventStates aStates) {
  // See numbers in GetScrollbarArrowColor.
  // This function is written based on ratios between values listed there.

  bool isActive = aStates.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHover = aStates.HasState(NS_EVENT_STATE_HOVER);
  if (!isActive && !isHover) {
    return aTrackColor;
  }
  float luminance = RelativeLuminanceUtils::Compute(aTrackColor);
  if (isActive) {
    if (luminance >= 0.18f) {
      luminance *= 0.134f;
    } else {
      luminance /= 0.134f;
      luminance = std::min(luminance, 1.0f);
    }
  } else {
    if (luminance >= 0.18f) {
      luminance *= 0.805f;
    } else {
      luminance /= 0.805f;
    }
  }
  return RelativeLuminanceUtils::Adjust(aTrackColor, luminance);
}

/*static*/
Maybe<nscolor> ScrollbarDrawing::GetScrollbarArrowColor(nscolor aButtonColor) {
  // In Windows 10 scrollbar, there are several gray colors used:
  //
  // State  | Background (lum) | Arrow   | Contrast
  // -------+------------------+---------+---------
  // Normal | Gray 240 (87.1%) | Gray 96 |     5.5
  // Hover  | Gray 218 (70.1%) | Black   |    15.0
  // Active | Gray 96  (11.7%) | White   |     6.3
  //
  // Contrast value is computed based on the definition in
  // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
  //
  // This function is written based on these values.

  if (NS_GET_A(aButtonColor) == 0) {
    // If the button color is transparent, because of e.g.
    // scrollbar-color: <something> transparent, then use
    // the thumb color, which is expected to have enough
    // contrast.
    return Nothing();
  }

  float luminance = RelativeLuminanceUtils::Compute(aButtonColor);
  // Color with luminance larger than 0.72 has contrast ratio over 4.6
  // to color with luminance of gray 96, so this value is chosen for
  // this range. It is the luminance of gray 221.
  if (luminance >= 0.72) {
    // ComputeRelativeLuminanceFromComponents(96). That function cannot
    // be constexpr because of std::pow.
    const float GRAY96_LUMINANCE = 0.117f;
    return Some(RelativeLuminanceUtils::Adjust(aButtonColor, GRAY96_LUMINANCE));
  }
  // The contrast ratio of a color to black equals that to white when its
  // luminance is around 0.18, with a contrast ratio ~4.6 to both sides,
  // thus the value below. It's the lumanince of gray 118.
  //
  // TODO(emilio): Maybe the button alpha is not the best thing to use here and
  // we should use the thumb alpha? It seems weird that the color of the arrow
  // depends on the opacity of the scrollbar thumb...
  if (luminance >= 0.18) {
    return Some(NS_RGBA(0, 0, 0, NS_GET_A(aButtonColor)));
  }
  return Some(NS_RGBA(255, 255, 255, NS_GET_A(aButtonColor)));
}

std::pair<sRGBColor, sRGBColor> ScrollbarDrawing::ComputeScrollbarButtonColors(
    nsIFrame* aFrame, StyleAppearance aAppearance, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    if (aElementState.HasAtLeastOneOfStates(NS_EVENT_STATE_ACTIVE |
                                            NS_EVENT_STATE_HOVER)) {
      return aColors.SystemPair(StyleSystemColor::Selecteditem,
                                StyleSystemColor::Buttonface);
    }
    return aColors.SystemPair(StyleSystemColor::Window,
                              StyleSystemColor::Windowtext);
  }

  auto trackColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  nscolor buttonColor =
      GetScrollbarButtonColor(trackColor.ToABGR(), aElementState);
  auto arrowColor =
      GetScrollbarArrowColor(buttonColor)
          .map(sRGBColor::FromABGR)
          .valueOrFrom([&] {
            return ComputeScrollbarThumbColor(aFrame, aStyle, aElementState,
                                              aDocumentState, aColors);
          });
  return {sRGBColor::FromABGR(buttonColor), arrowColor};
}

bool ScrollbarDrawing::PaintScrollbarButton(
    DrawTarget& aDrawTarget, StyleAppearance aAppearance,
    const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aElementState,
    const EventStates& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  auto [buttonColor, arrowColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  aDrawTarget.FillRect(aRect.ToUnknownRect(),
                       ColorPattern(ToDeviceColor(buttonColor)));

  // Start with Up arrow.
  float arrowPolygonX[] = {-4.0f, 0.0f, 4.0f, 4.0f, 0.0f, -4.0f};
  float arrowPolygonY[] = {0.0f, -4.0f, 0.0f, 3.0f, -1.0f, 3.0f};

  const float kPolygonSize = 17;

  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);
  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
      break;
    case StyleAppearance::ScrollbarbuttonDown:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowPolygonY[i] *= -1;
      }
      break;
    case StyleAppearance::ScrollbarbuttonLeft:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        float temp = arrowPolygonX[i];
        arrowPolygonX[i] = arrowPolygonY[i];
        arrowPolygonY[i] = temp;
      }
      break;
    case StyleAppearance::ScrollbarbuttonRight:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        float temp = arrowPolygonX[i];
        arrowPolygonX[i] = arrowPolygonY[i] * -1;
        arrowPolygonY[i] = temp;
      }
      break;
    default:
      return false;
  }
  ThemeDrawing::PaintArrow(aDrawTarget, aRect, arrowPolygonX, arrowPolygonY,
                           kPolygonSize, arrowNumPoints, arrowColor);
  return true;
}
