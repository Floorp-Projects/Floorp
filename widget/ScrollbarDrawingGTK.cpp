/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingGTK.h"

#include "mozilla/gfx/Helpers.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsLayoutUtils.h"
#include "nsNativeTheme.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

LayoutDeviceIntSize ScrollbarDrawingGTK::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));

  const ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  auto sizes = GetScrollbarSizes(
      aPresContext, style->StyleUIReset()->mScrollbarWidth, Overlay::No);
  MOZ_ASSERT(sizes.mHorizontal == sizes.mVertical);
  LayoutDeviceIntSize size{sizes.mHorizontal, sizes.mVertical};
  if (aAppearance == StyleAppearance::ScrollbarHorizontal ||
      aAppearance == StyleAppearance::ScrollbarVertical ||
      aAppearance == StyleAppearance::ScrollbarthumbHorizontal ||
      aAppearance == StyleAppearance::ScrollbarthumbVertical) {
    CSSCoord thumbSize(
        StaticPrefs::widget_non_native_theme_gtk_scrollbar_thumb_cross_size());
    const bool isVertical =
        aAppearance == StyleAppearance::ScrollbarVertical ||
        aAppearance == StyleAppearance::ScrollbarthumbVertical;
    auto dpi = GetDPIRatioForScrollbarPart(aPresContext);
    if (isVertical) {
      size.height = thumbSize * dpi;
    } else {
      size.width = thumbSize * dpi;
    }
  }
  return size;
}

Maybe<nsITheme::Transparency> ScrollbarDrawingGTK::GetScrollbarPartTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (!aFrame->PresContext()->UseOverlayScrollbars() &&
      (aAppearance == StyleAppearance::ScrollbarVertical ||
       aAppearance == StyleAppearance::ScrollbarHorizontal) &&
      IsScrollbarTrackOpaque(aFrame)) {
    return Some(nsITheme::eOpaque);
  }

  return Nothing();
}

template <typename PaintBackendData>
bool ScrollbarDrawingGTK::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  sRGBColor thumbColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aColors);

  LayoutDeviceRect thumbRect(aRect);

  if (aFrame->PresContext()->UseOverlayScrollbars() &&
      !ScrollbarDrawing::IsParentScrollbarHoveredOrActive(aFrame)) {
    if (aHorizontal) {
      thumbRect.height *= 0.5;
      thumbRect.y += thumbRect.height;
    } else {
      thumbRect.width *= 0.5;
      if (aFrame->GetWritingMode().IsPhysicalLTR()) {
        thumbRect.x += thumbRect.width;
      }
    }
  }

  {
    float factor = std::max(
        0.0f,
        1.0f - StaticPrefs::widget_non_native_theme_gtk_scrollbar_thumb_size());
    thumbRect.Deflate((aHorizontal ? thumbRect.height : thumbRect.width) *
                      factor);
  }

  LayoutDeviceCoord radius =
      StaticPrefs::widget_non_native_theme_gtk_scrollbar_round_thumb()
          ? (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f
          : 0.0f;

  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor,
                                           sRGBColor(), 0, radius / aDpiRatio,
                                           aDpiRatio);
  return true;
}

bool ScrollbarDrawingGTK::PaintScrollbarThumb(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aDrawTarget, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

bool ScrollbarDrawingGTK::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  return DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aColors,
                               aDpiRatio);
}

bool ScrollbarDrawingGTK::ShouldDrawScrollbarButtons() {
  if (StaticPrefs::widget_non_native_theme_enabled()) {
    return StaticPrefs::widget_non_native_theme_gtk_scrollbar_allow_buttons();
  }
  return true;
}

void ScrollbarDrawingGTK::RecomputeScrollbarParams() {
  uint32_t defaultSize = 12;
  uint32_t overrideSize =
      StaticPrefs::widget_non_native_theme_scrollbar_size_override();
  if (overrideSize > 0) {
    defaultSize = overrideSize;
  }
  mHorizontalScrollbarHeight = mVerticalScrollbarWidth = defaultSize;

  // On GTK, widgets don't account for text scale factor, but that's included
  // in the usual DPI computations, so we undo that here, just like
  // GetMonitorScaleFactor does it in nsNativeThemeGTK.
  float scale =
      LookAndFeel::GetFloat(LookAndFeel::FloatID::TextScaleFactor, 1.0f);
  if (scale != 1.0f) {
    mVerticalScrollbarWidth =
        uint32_t(round(float(mVerticalScrollbarWidth) / scale));
    mHorizontalScrollbarHeight =
        uint32_t(round(float(mHorizontalScrollbarHeight) / scale));
  }
}
