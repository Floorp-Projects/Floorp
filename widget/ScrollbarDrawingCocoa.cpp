/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingCocoa.h"

#include "mozilla/gfx/Helpers.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsContainerFrame.h"
#include "nsAlgorithm.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsLookAndFeel.h"
#include "nsNativeTheme.h"

using namespace mozilla::gfx;
namespace mozilla::widget {

using ScrollbarKind = ScrollbarDrawing::ScrollbarKind;

struct ColoredRect {
  LayoutDeviceRect mRect;
  nscolor mColor = 0;
};

// The caller can draw this rectangle with rounded corners as appropriate.
struct ThumbRect {
  LayoutDeviceRect mRect;
  nscolor mFillColor = 0;
  nscolor mStrokeColor = 0;
  float mStrokeWidth = 0.0f;
  float mStrokeOutset = 0.0f;
};

using ScrollbarTrackRects = Array<ColoredRect, 4>;
using ScrollCornerRects = Array<ColoredRect, 7>;

struct ScrollbarParams {
  bool isOverlay = false;
  bool isRolledOver = false;
  bool isSmall = false;
  bool isHorizontal = false;
  bool isRtl = false;
  bool isDark = false;
  bool isCustom = false;
  // Two colors only used when custom is true.
  nscolor trackColor = NS_RGBA(0, 0, 0, 0);
  nscolor faceColor = NS_RGBA(0, 0, 0, 0);
};

static ScrollbarParams ComputeScrollbarParams(nsIFrame* aFrame,
                                              const ComputedStyle& aStyle,
                                              const ThemeColors& aColors,
                                              ScrollbarKind aScrollbarKind) {
  ScrollbarParams params;
  params.isOverlay =
      nsLookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars) != 0;
  params.isRolledOver = ScrollbarDrawing::IsParentScrollbarRolledOver(aFrame);
  params.isSmall =
      aStyle.StyleUIReset()->ScrollbarWidth() == StyleScrollbarWidth::Thin;
  params.isRtl = aScrollbarKind == ScrollbarKind::VerticalLeft;
  params.isHorizontal = aScrollbarKind == ScrollbarKind::Horizontal;
  params.isDark = aColors.IsDark();

  const nsStyleUI* ui = aStyle.StyleUI();
  if (ui->HasCustomScrollbars()) {
    const auto& colors = ui->mScrollbarColor.AsColors();
    params.isCustom = true;
    params.trackColor = colors.track.CalcColor(aStyle);
    params.faceColor = colors.thumb.CalcColor(aStyle);
  }

  return params;
}

LayoutDeviceIntSize ScrollbarDrawingCocoa::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));

  auto minSize = [&] {
    switch (aAppearance) {
      case StyleAppearance::ScrollbarthumbHorizontal:
        return IntSize{26, 0};
      case StyleAppearance::ScrollbarthumbVertical:
        return IntSize{0, 26};
      case StyleAppearance::ScrollbarVertical:
      case StyleAppearance::ScrollbarHorizontal:
      case StyleAppearance::ScrollbartrackVertical:
      case StyleAppearance::ScrollbartrackHorizontal: {
        ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
        auto scrollbarWidth = style->StyleUIReset()->ScrollbarWidth();
        auto size = GetScrollbarSize(
            scrollbarWidth,
            LookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars));
        return IntSize{size, size};
      }
      case StyleAppearance::ScrollbarbuttonUp:
      case StyleAppearance::ScrollbarbuttonDown:
        return IntSize{15, 16};
      case StyleAppearance::ScrollbarbuttonLeft:
      case StyleAppearance::ScrollbarbuttonRight:
        return IntSize{16, 15};
      default:
        return IntSize{};
    }
  }();

  auto dpi = GetDPIRatioForScrollbarPart(aPresContext).scale;
  if (dpi >= 2.0f) {
    return LayoutDeviceIntSize{minSize.width * 2, minSize.height * 2};
  }
  return LayoutDeviceIntSize{minSize.width, minSize.height};
}

/*static*/
CSSIntCoord ScrollbarDrawingCocoa::GetScrollbarSize(StyleScrollbarWidth aWidth,
                                                    bool aOverlay) {
  bool isSmall = aWidth == StyleScrollbarWidth::Thin;
  if (aOverlay) {
    return isSmall ? 14 : 16;
  }
  return isSmall ? 11 : 15;
}

/*static*/
LayoutDeviceIntCoord ScrollbarDrawingCocoa::GetScrollbarSize(
    StyleScrollbarWidth aWidth, bool aOverlay, DPIRatio aDpiRatio) {
  CSSIntCoord size = GetScrollbarSize(aWidth, aOverlay);
  if (aDpiRatio.scale >= 2.0f) {
    return int32_t(size) * 2;
  }
  return int32_t(size);
}

auto ScrollbarDrawingCocoa::GetScrollbarSizes(nsPresContext* aPresContext,
                                              StyleScrollbarWidth aWidth,
                                              Overlay aOverlay)
    -> ScrollbarSizes {
  auto size = GetScrollbarSize(aWidth, aOverlay == Overlay::Yes,
                               GetDPIRatioForScrollbarPart(aPresContext));
  return {size, size};
}

static ThumbRect GetThumbRect(const LayoutDeviceRect& aRect,
                              const ScrollbarParams& aParams, float aScale) {
  // This matches the sizing checks in GetMinimumWidgetSize etc.
  aScale = aScale >= 2.0f ? 2.0f : 1.0f;

  // Compute the thumb thickness. This varies based on aParams.small,
  // aParams.overlay and aParams.rolledOver. non-overlay: 6 / 8, overlay
  // non-hovered: 5 / 7, overlay hovered: 9 / 11
  float thickness = aParams.isSmall ? 6.0f : 8.0f;
  if (aParams.isOverlay) {
    thickness -= 1.0f;
    if (aParams.isRolledOver) {
      thickness += 4.0f;
    }
  }
  thickness *= aScale;

  // Compute the thumb rect.
  const float outerSpacing =
      ((aParams.isOverlay || aParams.isSmall) ? 1.0f : 2.0f) * aScale;
  LayoutDeviceRect thumbRect = aRect;
  thumbRect.Deflate(1.0f * aScale);
  if (aParams.isHorizontal) {
    float bottomEdge = thumbRect.YMost() - outerSpacing;
    thumbRect.SetBoxY(bottomEdge - thickness, bottomEdge);
  } else {
    if (aParams.isRtl) {
      float leftEdge = thumbRect.X() + outerSpacing;
      thumbRect.SetBoxX(leftEdge, leftEdge + thickness);
    } else {
      float rightEdge = thumbRect.XMost() - outerSpacing;
      thumbRect.SetBoxX(rightEdge - thickness, rightEdge);
    }
  }

  // Compute the thumb fill color.
  nscolor faceColor;
  if (aParams.isCustom) {
    faceColor = aParams.faceColor;
  } else {
    if (aParams.isOverlay) {
      faceColor =
          aParams.isDark ? NS_RGBA(255, 255, 255, 128) : NS_RGBA(0, 0, 0, 128);
    } else if (aParams.isDark) {
      faceColor = aParams.isRolledOver ? NS_RGBA(158, 158, 158, 255)
                                       : NS_RGBA(117, 117, 117, 255);
    } else {
      faceColor = aParams.isRolledOver ? NS_RGBA(125, 125, 125, 255)
                                       : NS_RGBA(194, 194, 194, 255);
    }
  }

  nscolor strokeColor = 0;
  float strokeOutset = 0.0f;
  float strokeWidth = 0.0f;

  // Overlay scrollbars have an additional stroke around the fill.
  if (aParams.isOverlay) {
    // For the default alpha of 128 we want to end up with 48 in the outline.
    constexpr float kAlphaScaling = 48.0f / 128.0f;
    const uint8_t strokeAlpha =
        uint8_t(clamped(NS_GET_A(faceColor) * kAlphaScaling, 0.0f, 48.0f));
    if (strokeAlpha) {
      strokeOutset = (aParams.isDark ? 0.3f : 0.5f) * aScale;
      strokeWidth = (aParams.isDark ? 0.6f : 0.8f) * aScale;

      strokeColor = aParams.isDark ? NS_RGBA(0, 0, 0, strokeAlpha)
                                   : NS_RGBA(255, 255, 255, strokeAlpha);
    }
  }

  return {thumbRect, faceColor, strokeColor, strokeWidth, strokeOutset};
}

struct ScrollbarTrackDecorationColors {
  nscolor mInnerColor = 0;
  nscolor mShadowColor = 0;
  nscolor mOuterColor = 0;
};

static ScrollbarTrackDecorationColors ComputeScrollbarTrackDecorationColors(
    nscolor aTrackColor) {
  ScrollbarTrackDecorationColors result;
  float luminance = RelativeLuminanceUtils::Compute(aTrackColor);
  if (luminance >= 0.5f) {
    result.mInnerColor =
        RelativeLuminanceUtils::Adjust(aTrackColor, luminance * 0.836f);
    result.mShadowColor =
        RelativeLuminanceUtils::Adjust(aTrackColor, luminance * 0.982f);
    result.mOuterColor =
        RelativeLuminanceUtils::Adjust(aTrackColor, luminance * 0.886f);
  } else {
    result.mInnerColor =
        RelativeLuminanceUtils::Adjust(aTrackColor, luminance * 1.196f);
    result.mShadowColor =
        RelativeLuminanceUtils::Adjust(aTrackColor, luminance * 1.018f);
    result.mOuterColor =
        RelativeLuminanceUtils::Adjust(aTrackColor, luminance * 1.129f);
  }
  return result;
}

static bool GetScrollbarTrackRects(const LayoutDeviceRect& aRect,
                                   const ScrollbarParams& aParams, float aScale,
                                   ScrollbarTrackRects& aRects) {
  if (aParams.isOverlay && !aParams.isRolledOver) {
    // Non-hovered overlay scrollbars don't have a track. Draw nothing.
    return false;
  }

  // This matches the sizing checks in GetMinimumWidgetSize etc.
  aScale = aScale >= 2.0f ? 2.0f : 1.0f;

  nscolor trackColor;
  if (aParams.isCustom) {
    trackColor = aParams.trackColor;
  } else {
    if (aParams.isOverlay) {
      trackColor = aParams.isDark ? NS_RGBA(201, 201, 201, 38)
                                  : NS_RGBA(250, 250, 250, 191);
    } else {
      trackColor = aParams.isDark ? NS_RGBA(46, 46, 46, 255)
                                  : NS_RGBA(250, 250, 250, 255);
    }
  }

  float thickness = aParams.isHorizontal ? aRect.height : aRect.width;

  // The scrollbar track is drawn as multiple non-overlapping segments, which
  // make up lines of different widths and with slightly different shading.
  ScrollbarTrackDecorationColors colors =
      ComputeScrollbarTrackDecorationColors(trackColor);
  struct {
    nscolor color;
    float thickness;
  } segments[] = {
      {colors.mInnerColor, 1.0f * aScale},
      {colors.mShadowColor, 1.0f * aScale},
      {trackColor, thickness - 3.0f * aScale},
      {colors.mOuterColor, 1.0f * aScale},
  };

  // Iterate over the segments "from inside to outside" and fill each segment.
  // For horizontal scrollbars, iterate top to bottom.
  // For vertical scrollbars, iterate left to right or right to left based on
  // aParams.isRtl.
  auto current = aRects.begin();
  float accumulatedThickness = 0.0f;
  for (const auto& segment : segments) {
    LayoutDeviceRect segmentRect = aRect;
    float startThickness = accumulatedThickness;
    float endThickness = startThickness + segment.thickness;
    if (aParams.isHorizontal) {
      segmentRect.SetBoxY(aRect.Y() + startThickness, aRect.Y() + endThickness);
    } else {
      if (aParams.isRtl) {
        segmentRect.SetBoxX(aRect.XMost() - endThickness,
                            aRect.XMost() - startThickness);
      } else {
        segmentRect.SetBoxX(aRect.X() + startThickness,
                            aRect.X() + endThickness);
      }
    }
    accumulatedThickness = endThickness;
    *current++ = {segmentRect, segment.color};
  }

  return true;
}

static bool GetScrollCornerRects(const LayoutDeviceRect& aRect,
                                 const ScrollbarParams& aParams, float aScale,
                                 ScrollCornerRects& aRects) {
  if (aParams.isOverlay && !aParams.isRolledOver) {
    // Non-hovered overlay scrollbars don't have a corner. Draw nothing.
    return false;
  }

  // This matches the sizing checks in GetMinimumWidgetSize etc.
  aScale = aScale >= 2.0f ? 2.0f : 1.0f;

  // Draw the following scroll corner.
  //
  //        Output:                      Rectangles:
  // +---+---+----------+---+     +---+---+----------+---+
  // | I | S | T ...  T | O |     | I | S | T ...  T | O |
  // +---+   |          |   |     +---+---+          |   |
  // | S   S | T ...  T |   |     | S   S | T ...  T | . |
  // +-------+          | . |     +-------+----------+ . |
  // | T      ...     T | . |     | T      ...     T | . |
  // | .              . | . |     | .              . |   |
  // | T      ...     T |   |     | T      ...     T | O |
  // +------------------+   |     +------------------+---+
  // | O       ...        O |     | O       ...        O |
  // +----------------------+     +----------------------+

  float width = aRect.width;
  float height = aRect.height;
  nscolor trackColor;
  if (aParams.isCustom) {
    trackColor = aParams.trackColor;
  } else {
    trackColor =
        aParams.isDark ? NS_RGBA(46, 46, 46, 255) : NS_RGBA(250, 250, 250, 255);
  }
  ScrollbarTrackDecorationColors colors =
      ComputeScrollbarTrackDecorationColors(trackColor);
  struct {
    nscolor color;
    LayoutDeviceRect relativeRect;
  } pieces[] = {
      {colors.mInnerColor, {0.0f, 0.0f, 1.0f * aScale, 1.0f * aScale}},
      {colors.mShadowColor,
       {1.0f * aScale, 0.0f, 1.0f * aScale, 1.0f * aScale}},
      {colors.mShadowColor,
       {0.0f, 1.0f * aScale, 2.0f * aScale, 1.0f * aScale}},
      {trackColor, {2.0f * aScale, 0.0f, width - 3.0f * aScale, 2.0f * aScale}},
      {trackColor,
       {0.0f, 2.0f * aScale, width - 1.0f * aScale, height - 3.0f * aScale}},
      {colors.mOuterColor,
       {width - 1.0f * aScale, 0.0f, 1.0f * aScale, height - 1.0f * aScale}},
      {colors.mOuterColor,
       {0.0f, height - 1.0f * aScale, width, 1.0f * aScale}},
  };

  auto current = aRects.begin();
  for (const auto& piece : pieces) {
    LayoutDeviceRect pieceRect = piece.relativeRect + aRect.TopLeft();
    if (aParams.isRtl) {
      pieceRect.x = aRect.XMost() - piece.relativeRect.XMost();
    }
    *current++ = {pieceRect, piece.color};
  }
  return true;
}

template <typename PaintBackendData>
void ScrollbarDrawingCocoa::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  ScrollbarParams params =
      ComputeScrollbarParams(aFrame, aStyle, aColors, aScrollbarKind);
  auto thumb = GetThumbRect(aRect, params, aDpiRatio.scale);
  LayoutDeviceCoord radius =
      (params.isHorizontal ? thumb.mRect.Height() : thumb.mRect.Width()) / 2.0f;
  ThemeDrawing::PaintRoundedRectWithRadius(
      aPaintData, thumb.mRect, thumb.mRect,
      sRGBColor::FromABGR(thumb.mFillColor), sRGBColor::White(0.0f), 0.0f,
      radius / aDpiRatio, aDpiRatio);
  if (!thumb.mStrokeColor) {
    return;
  }

  // Paint the stroke if needed.
  auto strokeRect = thumb.mRect;
  strokeRect.Inflate(thumb.mStrokeOutset + thumb.mStrokeWidth);
  radius =
      (params.isHorizontal ? strokeRect.Height() : strokeRect.Width()) / 2.0f;
  ThemeDrawing::PaintRoundedRectWithRadius(
      aPaintData, strokeRect, sRGBColor::White(0.0f),
      sRGBColor::FromABGR(thumb.mStrokeColor), thumb.mStrokeWidth,
      radius / aDpiRatio, aDpiRatio);
}

bool ScrollbarDrawingCocoa::PaintScrollbarThumb(
    DrawTarget& aDt, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  DoPaintScrollbarThumb(aDt, aRect, aScrollbarKind, aFrame, aStyle,
                        aElementState, aDocumentState, aColors, aDpiRatio);
  return true;
}

bool ScrollbarDrawingCocoa::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  DoPaintScrollbarThumb(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
                        aElementState, aDocumentState, aColors, aDpiRatio);
  return true;
}

template <typename PaintBackendData>
void ScrollbarDrawingCocoa::DoPaintScrollbarTrack(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  ScrollbarParams params =
      ComputeScrollbarParams(aFrame, aStyle, aColors, aScrollbarKind);
  ScrollbarTrackRects rects;
  if (GetScrollbarTrackRects(aRect, params, aDpiRatio.scale, rects)) {
    for (const auto& rect : rects) {
      ThemeDrawing::FillRect(aPaintData, rect.mRect,
                             sRGBColor::FromABGR(rect.mColor));
    }
  }
}

bool ScrollbarDrawingCocoa::PaintScrollbarTrack(
    DrawTarget& aDt, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  DoPaintScrollbarTrack(aDt, aRect, aScrollbarKind, aFrame, aStyle,
                        aDocumentState, aColors, aDpiRatio);
  return true;
}

bool ScrollbarDrawingCocoa::PaintScrollbarTrack(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  DoPaintScrollbarTrack(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
                        aDocumentState, aColors, aDpiRatio);
  return true;
}

template <typename PaintBackendData>
void ScrollbarDrawingCocoa::DoPaintScrollCorner(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  ScrollbarParams params =
      ComputeScrollbarParams(aFrame, aStyle, aColors, aScrollbarKind);
  ScrollCornerRects rects;
  if (GetScrollCornerRects(aRect, params, aDpiRatio.scale, rects)) {
    for (const auto& rect : rects) {
      ThemeDrawing::FillRect(aPaintData, rect.mRect,
                             sRGBColor::FromABGR(rect.mColor));
    }
  }
}

bool ScrollbarDrawingCocoa::PaintScrollCorner(
    DrawTarget& aDt, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  DoPaintScrollCorner(aDt, aRect, aScrollbarKind, aFrame, aStyle,
                      aDocumentState, aColors, aDpiRatio);
  return true;
}

bool ScrollbarDrawingCocoa::PaintScrollCorner(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const DocumentState& aDocumentState, const Colors& aColors,
    const DPIRatio& aDpiRatio) {
  DoPaintScrollCorner(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
                      aDocumentState, aColors, aDpiRatio);
  return true;
}

void ScrollbarDrawingCocoa::RecomputeScrollbarParams() {
  uint32_t defaultSize = 17;
  uint32_t overrideSize =
      StaticPrefs::widget_non_native_theme_scrollbar_size_override();
  if (overrideSize > 0) {
    defaultSize = overrideSize;
  }
  mHorizontalScrollbarHeight = mVerticalScrollbarWidth = defaultSize;
}

}  // namespace mozilla::widget
