/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingAndroid.h"

#include "nsIFrame.h"
#include "nsNativeTheme.h"

using namespace mozilla;
using namespace mozilla::widget;

LayoutDeviceIntSize ScrollbarDrawingAndroid::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));

  auto sizes =
      GetScrollbarSizes(aPresContext, StyleScrollbarWidth::Auto, Overlay::Yes);
  MOZ_ASSERT(sizes.mHorizontal == sizes.mVertical);

  return LayoutDeviceIntSize{sizes.mHorizontal, sizes.mVertical};
}

auto ScrollbarDrawingAndroid::GetScrollbarSizes(nsPresContext* aPresContext,
                                                StyleScrollbarWidth aWidth,
                                                Overlay aOverlay)
    -> ScrollbarSizes {
  // We force auto-width scrollbars because scrollbars on android are already
  // thin enough.
  return ScrollbarDrawing::GetScrollbarSizes(
      aPresContext, StyleScrollbarWidth::Auto, aOverlay);
}

template <typename PaintBackendData>
void ScrollbarDrawingAndroid::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  // TODO(emilio): Maybe do like macOS and draw a stroke?
  const auto color = ComputeScrollbarThumbColor(aFrame, aStyle, aElementState,
                                                aDocumentState, aColors);

  // Draw the thumb rect centered in the scrollbar.
  LayoutDeviceRect thumbRect(aRect);
  if (aHorizontal) {
    thumbRect.height *= 0.5f;
    thumbRect.y += thumbRect.height * 0.5f;
  } else {
    thumbRect.width *= 0.5f;
    thumbRect.x += thumbRect.width * 0.5f;
  }

  const LayoutDeviceCoord radius =
      (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f;
  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, color,
                                           sRGBColor::White(0.0f), 0.0f,
                                           radius / aDpiRatio, aDpiRatio);
}

bool ScrollbarDrawingAndroid::PaintScrollbarThumb(
    DrawTarget& aDt, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  DoPaintScrollbarThumb(aDt, aRect, aHorizontal, aFrame, aStyle, aElementState,
                        aDocumentState, aColors, aDpiRatio);
  return true;
}

bool ScrollbarDrawingAndroid::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                        aElementState, aDocumentState, aColors, aDpiRatio);
  return true;
}

void ScrollbarDrawingAndroid::RecomputeScrollbarParams() {
  uint32_t defaultSize = 6;
  uint32_t overrideSize =
      StaticPrefs::widget_non_native_theme_scrollbar_size_override();
  if (overrideSize > 0) {
    defaultSize = overrideSize;
  }
  mHorizontalScrollbarHeight = mVerticalScrollbarWidth = defaultSize;
}
