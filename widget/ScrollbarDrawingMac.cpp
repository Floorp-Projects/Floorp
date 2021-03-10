/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingMac.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsLookAndFeel.h"
#include "nsContainerFrame.h"
#include "nsNativeTheme.h"

namespace mozilla {

using namespace gfx;

namespace widget {

static nsIFrame* GetParentScrollbarFrame(nsIFrame* aFrame) {
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

static bool IsParentScrollbarRolledOver(nsIFrame* aFrame) {
  nsIFrame* scrollbarFrame = GetParentScrollbarFrame(aFrame);
  return nsLookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars) != 0
             ? nsNativeTheme::CheckBooleanAttr(scrollbarFrame, nsGkAtoms::hover)
             : nsNativeTheme::GetContentState(scrollbarFrame,
                                              StyleAppearance::None)
                   .HasState(NS_EVENT_STATE_HOVER);
}

CSSIntCoord ScrollbarDrawingMac::GetScrollbarSize(StyleScrollbarWidth aWidth,
                                                  bool aOverlay) {
  bool isSmall = aWidth == StyleScrollbarWidth::Thin;
  if (aOverlay) {
    return isSmall ? 14 : 16;
  }
  return isSmall ? 11 : 15;
}

LayoutDeviceIntCoord ScrollbarDrawingMac::GetScrollbarSize(
    StyleScrollbarWidth aWidth, bool aOverlay, float aDpiRatio) {
  CSSIntCoord size = GetScrollbarSize(aWidth, aOverlay);
  if (aDpiRatio >= 2.0f) {
    return int32_t(size) * 2;
  }
  return int32_t(size);
}

LayoutDeviceIntSize ScrollbarDrawingMac::GetMinimumWidgetSize(
    StyleAppearance aAppearance, nsIFrame* aFrame, float aDpiRatio) {
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
        auto scrollbarWidth = style->StyleUIReset()->mScrollbarWidth;
        auto size = GetScrollbarSize(
            scrollbarWidth,
            LookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars));
        return IntSize{size, size};
      }
      case StyleAppearance::MozMenulistArrowButton: {
        auto size =
            GetScrollbarSize(StyleScrollbarWidth::Auto, /* aOverlay = */ false);
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

  if (aDpiRatio >= 2.0f) {
    return LayoutDeviceIntSize{minSize.width * 2, minSize.height * 2};
  }
  return LayoutDeviceIntSize{minSize.width, minSize.height};
}

ScrollbarParams ScrollbarDrawingMac::ComputeScrollbarParams(
    nsIFrame* aFrame, const ComputedStyle& aStyle, bool aIsHorizontal) {
  ScrollbarParams params;
  params.overlay =
      nsLookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars) != 0;
  params.rolledOver = IsParentScrollbarRolledOver(aFrame);
  params.small =
      aStyle.StyleUIReset()->mScrollbarWidth == StyleScrollbarWidth::Thin;
  params.rtl = nsNativeTheme::IsFrameRTL(aFrame);
  params.horizontal = aIsHorizontal;
  params.onDarkBackground = nsNativeTheme::IsDarkBackground(aFrame);
  // Don't use custom scrollbars for overlay scrollbars since they are
  // generally good enough for use cases of custom scrollbars.
  if (!params.overlay) {
    const nsStyleUI* ui = aStyle.StyleUI();
    if (ui->HasCustomScrollbars()) {
      const auto& colors = ui->mScrollbarColor.AsColors();
      params.custom = true;
      params.trackColor = colors.track.CalcColor(aStyle);
      params.faceColor = colors.thumb.CalcColor(aStyle);
    }
  }

  return params;
}

auto ScrollbarDrawingMac::GetThumbRect(const Rect& aRect,
                                       const ScrollbarParams& aParams,
                                       float aScale) -> ThumbRect {
  // This matches the sizing checks in GetMinimumWidgetSize etc.
  aScale = aScale >= 2.0f ? 2.0f : 1.0f;

  // Compute the thumb thickness. This varies based on aParams.small,
  // aParams.overlay and aParams.rolledOver. non-overlay: 6 / 8, overlay
  // non-hovered: 5 / 7, overlay hovered: 9 / 11
  float thickness = aParams.small ? 6.0f : 8.0f;
  if (aParams.overlay) {
    thickness -= 1.0f;
    if (aParams.rolledOver) {
      thickness += 4.0f;
    }
  }
  thickness *= aScale;

  // Compute the thumb rect.
  const float outerSpacing =
      ((aParams.overlay || aParams.small) ? 1.0f : 2.0f) * aScale;
  Rect thumbRect = aRect;
  thumbRect.Deflate(1.0f * aScale);
  if (aParams.horizontal) {
    float bottomEdge = thumbRect.YMost() - outerSpacing;
    thumbRect.SetBoxY(bottomEdge - thickness, bottomEdge);
  } else {
    if (aParams.rtl) {
      float leftEdge = thumbRect.X() + outerSpacing;
      thumbRect.SetBoxX(leftEdge, leftEdge + thickness);
    } else {
      float rightEdge = thumbRect.XMost() - outerSpacing;
      thumbRect.SetBoxX(rightEdge - thickness, rightEdge);
    }
  }

  // Compute the thumb fill color.
  nscolor faceColor;
  if (aParams.custom) {
    faceColor = aParams.faceColor;
  } else {
    if (aParams.overlay) {
      faceColor = aParams.onDarkBackground ? NS_RGBA(255, 255, 255, 128)
                                           : NS_RGBA(0, 0, 0, 128);
    } else {
      faceColor = aParams.rolledOver ? NS_RGBA(125, 125, 125, 255)
                                     : NS_RGBA(194, 194, 194, 255);
    }
  }

  nscolor strokeColor = 0;
  float strokeOutset = 0.0f;
  float strokeWidth = 0.0f;

  // Overlay scrollbars have an additional stroke around the fill.
  if (aParams.overlay) {
    strokeOutset = (aParams.onDarkBackground ? 0.3f : 0.5f) * aScale;
    strokeWidth = (aParams.onDarkBackground ? 0.6f : 0.8f) * aScale;

    strokeColor = aParams.onDarkBackground ? NS_RGBA(0, 0, 0, 48)
                                           : NS_RGBA(255, 255, 255, 48);
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

bool ScrollbarDrawingMac::GetScrollbarTrackRects(const Rect& aRect,
                                                 const ScrollbarParams& aParams,
                                                 float aScale,
                                                 ScrollbarTrackRects& aRects) {
  if (aParams.overlay && !aParams.rolledOver) {
    // Non-hovered overlay scrollbars don't have a track. Draw nothing.
    return false;
  }

  // This matches the sizing checks in GetMinimumWidgetSize etc.
  aScale = aScale >= 2.0f ? 2.0f : 1.0f;

  nscolor trackColor;
  if (aParams.custom) {
    trackColor = aParams.trackColor;
  } else {
    if (aParams.overlay) {
      trackColor = aParams.onDarkBackground ? NS_RGBA(201, 201, 201, 38)
                                            : NS_RGBA(250, 250, 250, 191);
    } else {
      trackColor = NS_RGBA(250, 250, 250, 255);
    }
  }

  float thickness = aParams.horizontal ? aRect.height : aRect.width;

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
  // aParams.rtl.
  auto current = aRects.begin();
  float accumulatedThickness = 0.0f;
  for (const auto& segment : segments) {
    Rect segmentRect = aRect;
    float startThickness = accumulatedThickness;
    float endThickness = startThickness + segment.thickness;
    if (aParams.horizontal) {
      segmentRect.SetBoxY(aRect.Y() + startThickness, aRect.Y() + endThickness);
    } else {
      if (aParams.rtl) {
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

bool ScrollbarDrawingMac::GetScrollCornerRects(const Rect& aRect,
                                               const ScrollbarParams& aParams,
                                               float aScale,
                                               ScrollCornerRects& aRects) {
  if (aParams.overlay && !aParams.rolledOver) {
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
  nscolor trackColor =
      aParams.custom ? aParams.trackColor : NS_RGBA(250, 250, 250, 255);
  ScrollbarTrackDecorationColors colors =
      ComputeScrollbarTrackDecorationColors(trackColor);
  struct {
    nscolor color;
    Rect relativeRect;
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
    Rect pieceRect = piece.relativeRect + aRect.TopLeft();
    if (aParams.rtl) {
      pieceRect.x = aRect.XMost() - piece.relativeRect.XMost();
    }
    *current++ = {pieceRect, piece.color};
  }
  return true;
}

}  // namespace widget
}  // namespace mozilla
