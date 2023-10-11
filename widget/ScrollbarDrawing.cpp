/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawing.h"

#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsContainerFrame.h"
#include "nsDeviceContext.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsLookAndFeel.h"
#include "nsNativeTheme.h"

using namespace mozilla::gfx;

namespace mozilla::widget {

using mozilla::RelativeLuminanceUtils;

/* static */
auto ScrollbarDrawing::GetDPIRatioForScrollbarPart(const nsPresContext* aPc)
    -> DPIRatio {
  DPIRatio ratio(
      float(AppUnitsPerCSSPixel()) /
      float(aPc->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom()));
  if (aPc->IsPrintPreview()) {
    ratio.scale *= aPc->GetPrintPreviewScaleForSequenceFrameOrScrollbars();
  }
  if (mKind == Kind::Cocoa) {
    return DPIRatio(ratio.scale >= 2.0f ? 2.0f : 1.0f);
  }
  return ratio;
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
  return aFrame->PresContext()->UseOverlayScrollbars()
             ? nsNativeTheme::CheckBooleanAttr(scrollbarFrame, nsGkAtoms::hover)
             : nsNativeTheme::GetContentState(scrollbarFrame,
                                              StyleAppearance::None)
                   .HasState(ElementState::HOVER);
}

/*static*/
bool ScrollbarDrawing::IsParentScrollbarHoveredOrActive(nsIFrame* aFrame) {
  nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
  return scrollbarFrame &&
         scrollbarFrame->GetContent()
             ->AsElement()
             ->State()
             .HasAtLeastOneOfStates(ElementState::HOVER | ElementState::ACTIVE);
}

/*static*/
bool ScrollbarDrawing::IsScrollbarWidthThin(const ComputedStyle& aStyle) {
  auto scrollbarWidth = aStyle.StyleUIReset()->ScrollbarWidth();
  return scrollbarWidth == StyleScrollbarWidth::Thin;
}

/*static*/
bool ScrollbarDrawing::IsScrollbarWidthThin(nsIFrame* aFrame) {
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  return IsScrollbarWidthThin(*style);
}

CSSIntCoord ScrollbarDrawing::GetCSSScrollbarSize(StyleScrollbarWidth aWidth,
                                                  Overlay aOverlay) const {
  return mScrollbarSize[aWidth == StyleScrollbarWidth::Thin]
                       [aOverlay == Overlay::Yes];
}

void ScrollbarDrawing::ConfigureScrollbarSize(StyleScrollbarWidth aWidth,
                                              Overlay aOverlay,
                                              CSSIntCoord aSize) {
  mScrollbarSize[aWidth == StyleScrollbarWidth::Thin]
                [aOverlay == Overlay::Yes] = aSize;
}

void ScrollbarDrawing::ConfigureScrollbarSize(CSSIntCoord aSize) {
  ConfigureScrollbarSize(StyleScrollbarWidth::Auto, Overlay::No, aSize);
  ConfigureScrollbarSize(StyleScrollbarWidth::Auto, Overlay::Yes, aSize);
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::No, aSize / 2);
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::Yes, aSize / 2);
}

LayoutDeviceIntCoord ScrollbarDrawing::GetScrollbarSize(
    const nsPresContext* aPresContext, StyleScrollbarWidth aWidth,
    Overlay aOverlay) {
  return (CSSCoord(GetCSSScrollbarSize(aWidth, aOverlay)) *
          GetDPIRatioForScrollbarPart(aPresContext))
      .Rounded();
}

LayoutDeviceIntCoord ScrollbarDrawing::GetScrollbarSize(
    const nsPresContext* aPresContext, nsIFrame* aFrame) {
  auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  auto width = style->StyleUIReset()->ScrollbarWidth();
  auto overlay =
      aPresContext->UseOverlayScrollbars() ? Overlay::Yes : Overlay::No;
  return GetScrollbarSize(aPresContext, width, overlay);
}

bool ScrollbarDrawing::IsScrollbarTrackOpaque(nsIFrame* aFrame) {
  auto trackColor = ComputeScrollbarTrackColor(
      aFrame, *nsLayoutUtils::StyleForScrollbar(aFrame),
      aFrame->PresContext()->Document()->State(),
      Colors(aFrame, StyleAppearance::ScrollbartrackVertical));
  return trackColor.a == 1.0f;
}

sRGBColor ScrollbarDrawing::ComputeScrollbarTrackColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return aColors.System(StyleSystemColor::Window);
  }
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(
        ui->mScrollbarColor.AsColors().track.CalcColor(aStyle));
  }
  static constexpr sRGBColor sDefaultDarkTrackColor =
      sRGBColor::FromU8(20, 20, 25, 77);
  static constexpr sRGBColor sDefaultTrackColor(
      gfx::sRGBColor::UnusualFromARGB(0xfff0f0f0));

  auto systemColor = aDocumentState.HasAllStates(DocumentState::WINDOW_INACTIVE)
                         ? StyleSystemColor::ThemedScrollbarInactive
                         : StyleSystemColor::ThemedScrollbar;
  return aColors.SystemOrElse(systemColor, [&] {
    return aColors.IsDark() ? sDefaultDarkTrackColor : sDefaultTrackColor;
  });
}

// Don't use the theme color for dark scrollbars if it's not a color (if it's
// grey-ish), as that'd either lack enough contrast, or be close to what we'd do
// by default anyways.
sRGBColor ScrollbarDrawing::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors) {
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle), aElementState));
  }

  auto systemColor = [&] {
    if (aDocumentState.HasState(DocumentState::WINDOW_INACTIVE)) {
      return StyleSystemColor::ThemedScrollbarThumbInactive;
    }
    if (aElementState.HasState(ElementState::ACTIVE)) {
      if (aColors.HighContrast()) {
        return StyleSystemColor::Selecteditem;
      }
      return StyleSystemColor::ThemedScrollbarThumbActive;
    }
    if (aElementState.HasState(ElementState::HOVER)) {
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
    const nscolor unthemedColor = aColors.IsDark() ? NS_RGBA(249, 249, 250, 102)
                                                   : NS_RGB(0xcd, 0xcd, 0xcd);

    return sRGBColor::FromABGR(ThemeColors::AdjustUnthemedScrollbarThumbColor(
        unthemedColor, aElementState));
  });
}

template <typename PaintBackendData>
bool ScrollbarDrawing::DoPaintDefaultScrollbar(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  const bool overlay = aFrame->PresContext()->UseOverlayScrollbars();
  if (overlay && !aElementState.HasAtLeastOneOfStates(ElementState::HOVER |
                                                      ElementState::ACTIVE)) {
    return true;
  }
  const auto color =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  if (overlay && mKind == Kind::Win11) {
    LayoutDeviceCoord radius =
        (aScrollbarKind == ScrollbarKind::Horizontal ? aRect.height
                                                     : aRect.width) /
        2.0f;
    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, aRect, color,
                                             sRGBColor(), 0, radius / aDpiRatio,
                                             aDpiRatio);
  } else {
    ThemeDrawing::FillRect(aPaintData, aRect, color);
  }
  return true;
}

bool ScrollbarDrawing::PaintScrollbar(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollbar(aDrawTarget, aRect, aScrollbarKind, aFrame,
                                 aStyle, aElementState, aDocumentState, aColors,
                                 aDpiRatio);
}

bool ScrollbarDrawing::PaintScrollbar(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollbar(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
                                 aElementState, aDocumentState, aColors,
                                 aDpiRatio);
}

template <typename PaintBackendData>
bool ScrollbarDrawing::DoPaintDefaultScrollCorner(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  auto scrollbarColor =
      ComputeScrollbarTrackColor(aFrame, aStyle, aDocumentState, aColors);
  ThemeDrawing::FillRect(aPaintData, aRect, scrollbarColor);
  return true;
}

bool ScrollbarDrawing::PaintScrollCorner(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollCorner(aDrawTarget, aRect, aScrollbarKind, aFrame,
                                    aStyle, aDocumentState, aColors, aDpiRatio);
}

bool ScrollbarDrawing::PaintScrollCorner(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  return DoPaintDefaultScrollCorner(aWrData, aRect, aScrollbarKind, aFrame,
                                    aStyle, aDocumentState, aColors, aDpiRatio);
}

nscolor ScrollbarDrawing::GetScrollbarButtonColor(nscolor aTrackColor,
                                                  ElementState aStates) {
  // See numbers in GetScrollbarArrowColor.
  // This function is written based on ratios between values listed there.

  bool isActive = aStates.HasState(ElementState::ACTIVE);
  bool isHover = aStates.HasState(ElementState::HOVER);
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
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    if (aElementState.HasAtLeastOneOfStates(ElementState::ACTIVE |
                                            ElementState::HOVER)) {
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
    const LayoutDeviceRect& aRect, ScrollbarKind aScrollbarKind,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio&) {
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

}  // namespace mozilla::widget
