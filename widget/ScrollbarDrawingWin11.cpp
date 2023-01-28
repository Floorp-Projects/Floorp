/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingWin11.h"

#include "mozilla/gfx/Helpers.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLayoutUtils.h"
#include "Theme.h"
#include "nsNativeTheme.h"

using mozilla::gfx::sRGBColor;

namespace mozilla::widget {

// There are effectively three kinds of scrollbars in Windows 11:
//
//  * Overlay scrollbars (the ones where the scrollbar disappears automatically
//    and doesn't take space)
//  * Non-overlay scrollbar with thin (overlay-like) thumb.
//  * Non-overlay scrollbar with thick thumb.
//
// See bug 1755193 for some discussion on non-overlay scrollbar styles.
enum class Style {
  Overlay,
  ThinThumb,
  ThickThumb,
};

static Style ScrollbarStyle(nsPresContext* aPresContext) {
  if (aPresContext->UseOverlayScrollbars()) {
    return Style::Overlay;
  }
  if (StaticPrefs::
          widget_non_native_theme_win11_scrollbar_force_overlay_style()) {
    return Style::ThinThumb;
  }
  return Style::ThickThumb;
}

static constexpr CSSIntCoord kDefaultWinOverlayScrollbarSize = CSSIntCoord(12);
static constexpr CSSIntCoord kDefaultWinOverlayThinScrollbarSize =
    CSSIntCoord(10);

LayoutDeviceIntSize ScrollbarDrawingWin11::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));
  if (ScrollbarStyle(aPresContext) != Style::ThinThumb) {
    return ScrollbarDrawingWin::GetMinimumWidgetSize(aPresContext, aAppearance,
                                                     aFrame);
  }
  constexpr float kArrowRatio = 14.0f / kDefaultWinScrollbarSize;
  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight: {
      if (IsScrollbarWidthThin(aFrame)) {
        return {};
      }
      const LayoutDeviceIntCoord size =
          ScrollbarDrawing::GetScrollbarSize(aPresContext, aFrame);
      return LayoutDeviceIntSize{
          size, (kArrowRatio * LayoutDeviceCoord(size)).Rounded()};
    }
    default:
      return ScrollbarDrawingWin::GetMinimumWidgetSize(aPresContext,
                                                       aAppearance, aFrame);
  }
}

sRGBColor ScrollbarDrawingWin11::ComputeScrollbarTrackColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ScrollbarDrawingWin::ComputeScrollbarTrackColor(
        aFrame, aStyle, aDocumentState, aColors);
  }
  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->mScrollbarColor.IsColors()) {
    return sRGBColor::FromABGR(
        ui->mScrollbarColor.AsColors().track.CalcColor(aStyle));
  }
  return aColors.IsDark() ? sRGBColor::FromU8(23, 23, 23, 255)
                          : sRGBColor::FromU8(240, 240, 240, 255);
}

sRGBColor ScrollbarDrawingWin11::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ScrollbarDrawingWin::ComputeScrollbarThumbColor(
        aFrame, aStyle, aElementState, aDocumentState, aColors);
  }
  const nscolor baseColor = [&] {
    const nsStyleUI* ui = aStyle.StyleUI();
    if (ui->mScrollbarColor.IsColors()) {
      return ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle);
    }
    return aColors.IsDark() ? NS_RGBA(149, 149, 149, 255)
                            : NS_RGBA(133, 133, 133, 255);
  }();
  ElementState state = aElementState;
  if (!IsScrollbarWidthThin(aStyle)) {
    // non-thin scrollbars get hover feedback by changing thumb shape, so we
    // only provide active feedback (and we use the hover state for that as it's
    // more subtle).
    state &= ~ElementState::HOVER;
    if (state.HasState(ElementState::ACTIVE)) {
      state &= ~ElementState::ACTIVE;
      state |= ElementState::HOVER;
    }
  }
  return sRGBColor::FromABGR(
      ThemeColors::AdjustUnthemedScrollbarThumbColor(baseColor, state));
}

std::pair<sRGBColor, sRGBColor>
ScrollbarDrawingWin11::ComputeScrollbarButtonColors(
    nsIFrame* aFrame, StyleAppearance aAppearance, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors) {
  if (aColors.HighContrast()) {
    return ScrollbarDrawingWin::ComputeScrollbarButtonColors(
        aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  }
  // The button always looks transparent (the track behind it is visible), so we
  // can hardcode it.
  sRGBColor arrowColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aColors);
  return {sRGBColor::White(0.0f), arrowColor};
}

bool ScrollbarDrawingWin11::PaintScrollbarButton(
    DrawTarget& aDrawTarget, StyleAppearance aAppearance,
    const LayoutDeviceRect& aRect, ScrollbarKind aScrollbarKind,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  if (!ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame)) {
    return true;
  }

  const auto style = ScrollbarStyle(aFrame->PresContext());
  auto [buttonColor, arrowColor] = ComputeScrollbarButtonColors(
      aFrame, aAppearance, aStyle, aElementState, aDocumentState, aColors);
  if (style != Style::Overlay) {
    aDrawTarget.FillRect(aRect.ToUnknownRect(),
                         gfx::ColorPattern(ToDeviceColor(buttonColor)));
  }

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
  const bool horizontal = aScrollbarKind == ScrollbarKind::Horizontal;

  const float verticalOffset = [&] {
    if (style != Style::Overlay) {
      return 0.0f;
    }
    // To compensate for the scrollbar track radius we shift stuff vertically a
    // bit. This 1px is arbitrary, but enough for the triangle not to overflow.
    return 1.0f;
  }();
  const float horizontalOffset = [&] {
    if (style != Style::ThinThumb) {
      return 0.0f;  // Always center it in the rect.
    }
    // Compensate for the displacement we do of the thumb position by displacing
    // the arrow as well, see comment in DoPaintScrollbarThumb.
    if (horizontal) {
      return -0.5f;
    }
    return aScrollbarKind == ScrollbarKind::VerticalRight ? 0.5f : -0.5f;
  }();
  const float polygonSize = style == Style::Overlay
                                ? float(kDefaultWinOverlayScrollbarSize)
                                : float(kDefaultWinScrollbarSize);
  const int32_t arrowNumPoints = ArrayLength(arrowPolygonX);

  if (aElementState.HasState(ElementState::ACTIVE)) {
    arrowX = arrowPolygonXActive;
    arrowY = arrowPolygonYActive;
  } else if (aElementState.HasState(ElementState::HOVER)) {
    arrowX = arrowPolygonXHover;
    arrowY = arrowPolygonYHover;
  }

  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonRight:
      for (int32_t i = 0; i < arrowNumPoints; i++) {
        arrowY[i] += verticalOffset;
        arrowY[i] *= -1;
      }
      [[fallthrough]];
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonLeft:
      if (horizontalOffset != 0.0f) {
        for (int32_t i = 0; i < arrowNumPoints; i++) {
          arrowX[i] += horizontalOffset;
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
  if (style != Style::ThinThumb) {
    auto margin = CSSCoord(style == Style::Overlay ? 1 : 2) * aDpiRatio;
    arrowRect.Deflate(margin, margin);
  }

  ThemeDrawing::PaintArrow(aDrawTarget, arrowRect, arrowX, arrowY, polygonSize,
                           arrowNumPoints, arrowColor);
  return true;
}

template <typename PaintBackendData>
bool ScrollbarDrawingWin11::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  sRGBColor thumbColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aColors);

  LayoutDeviceRect thumbRect(aRect);

  const auto style = ScrollbarStyle(aFrame->PresContext());
  const bool hovered =
      ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame) ||
      (style != Style::Overlay && IsScrollbarWidthThin(aStyle));
  const bool horizontal = aScrollbarKind == ScrollbarKind::Horizontal;
  if (style == Style::ThickThumb) {
    constexpr float kHoveredThumbRatio =
        (1.0f - (11.0f / kDefaultWinScrollbarSize)) / 2.0f;
    constexpr float kUnhoveredThumbRatio =
        (1.0f - (9.0f / kDefaultWinScrollbarSize)) / 2.0f;
    const float ratio = hovered ? kHoveredThumbRatio : kUnhoveredThumbRatio;
    if (horizontal) {
      thumbRect.Deflate(0, thumbRect.height * ratio);
    } else {
      thumbRect.Deflate(thumbRect.width * ratio, 0);
    }

    auto radius = CSSCoord(hovered ? 2 : 0);
    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor,
                                             sRGBColor(), 0, radius, aDpiRatio);
    return true;
  }

  const float defaultTrackSize = style == Style::Overlay
                                     ? float(kDefaultWinOverlayScrollbarSize)
                                     : float(kDefaultWinScrollbarSize);
  const float trackSize = horizontal ? thumbRect.height : thumbRect.width;
  const float thumbSizeInPixels = hovered ? 6.0f : 2.0f;

  // The thumb might be a bit off-center, depending on our scrollbar styles.
  //
  // Hovered shifts, if any, need to be accounted for in PaintScrollbarButton.
  // For example, for the hovered horizontal thin scrollbar shift:
  //
  //   Scrollbar is 17px high by default. We make the thumb 6px tall and move
  //   it 5px towards the bottom, so the center (8.5 initially) is displaced
  //   by:
  //     (5px + 6px / 2) - 8.5px = -0.5px
  //
  // Same calculations apply to other shifts.
  const float shiftInPixels = [&] {
    if (style == Style::Overlay) {
      if (hovered) {
        // Keep the center intact.
        return (defaultTrackSize - thumbSizeInPixels) / 2.0f;
      }
      // We want logical pixels from the thumb to the edge. For LTR and
      // horizontal scrollbars that means shifting down the scrollbar size minus
      // the thumb.
      constexpr float kSpaceToEdge = 3.0f;
      if (horizontal || aScrollbarKind == ScrollbarKind::VerticalRight) {
        return defaultTrackSize - thumbSizeInPixels - kSpaceToEdge;
      }
      // For rtl is simpler.
      return kSpaceToEdge;
    }
    if (horizontal) {
      return hovered ? 5.0f : 7.0f;
    }
    const bool ltr = aScrollbarKind == ScrollbarKind::VerticalRight;
    return ltr ? (hovered ? 6.0f : 8.0f) : (hovered ? 5.0f : 7.0f);
  }();

  if (horizontal) {
    thumbRect.y += shiftInPixels * trackSize / defaultTrackSize;
    thumbRect.height *= thumbSizeInPixels / defaultTrackSize;
  } else {
    thumbRect.x += shiftInPixels * trackSize / defaultTrackSize;
    thumbRect.width *= thumbSizeInPixels / defaultTrackSize;
  }

  if (style == Style::Overlay || hovered) {
    LayoutDeviceCoord radius =
        (horizontal ? thumbRect.height : thumbRect.width) / 2.0f;

    MOZ_ASSERT(aRect.Contains(thumbRect));
    ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor,
                                             sRGBColor(), 0, radius / aDpiRatio,
                                             aDpiRatio);
    return true;
  }

  ThemeDrawing::FillRect(aPaintData, thumbRect, thumbColor);
  return true;
}

bool ScrollbarDrawingWin11::PaintScrollbarThumb(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aDrawTarget, aRect, aScrollbarKind, aFrame,
                               aStyle, aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

bool ScrollbarDrawingWin11::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

void ScrollbarDrawingWin11::RecomputeScrollbarParams() {
  ScrollbarDrawingWin::RecomputeScrollbarParams();
  // TODO(emilio): Maybe make this configurable? Though this doesn't respect
  // classic Windows registry settings, and cocoa overlay scrollbars also don't
  // respect the override it seems, so this should be fine.
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::Yes,
                         kDefaultWinOverlayThinScrollbarSize);
  ConfigureScrollbarSize(StyleScrollbarWidth::Auto, Overlay::Yes,
                         kDefaultWinOverlayScrollbarSize);
}

}  // namespace mozilla::widget
