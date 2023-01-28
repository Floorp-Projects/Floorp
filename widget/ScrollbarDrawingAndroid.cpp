/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollbarDrawingAndroid.h"

#include "nsIFrame.h"
#include "nsNativeTheme.h"
#include "mozilla/StaticPrefs_widget.h"

using namespace mozilla;
using namespace mozilla::widget;

LayoutDeviceIntSize ScrollbarDrawingAndroid::GetMinimumWidgetSize(
    nsPresContext* aPresContext, StyleAppearance aAppearance,
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsNativeTheme::IsWidgetScrollbarPart(aAppearance));
  auto size =
      GetScrollbarSize(aPresContext, StyleScrollbarWidth::Auto, Overlay::Yes);
  return LayoutDeviceIntSize{size, size};
}

template <typename PaintBackendData>
void ScrollbarDrawingAndroid::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  // TODO(emilio): Maybe do like macOS and draw a stroke?
  const auto color = ComputeScrollbarThumbColor(aFrame, aStyle, aElementState,
                                                aDocumentState, aColors);
  const bool horizontal = aScrollbarKind == ScrollbarKind::Horizontal;

  // Draw the thumb rect centered in the scrollbar.
  LayoutDeviceRect thumbRect(aRect);
  if (horizontal) {
    thumbRect.height *= 0.5f;
    thumbRect.y += thumbRect.height * 0.5f;
  } else {
    thumbRect.width *= 0.5f;
    thumbRect.x += thumbRect.width * 0.5f;
  }

  const LayoutDeviceCoord radius =
      (horizontal ? thumbRect.height : thumbRect.width) / 2.0f;
  ThemeDrawing::PaintRoundedRectWithRadius(aPaintData, thumbRect, color,
                                           sRGBColor::White(0.0f), 0.0f,
                                           radius / aDpiRatio, aDpiRatio);
}

bool ScrollbarDrawingAndroid::PaintScrollbarThumb(
    DrawTarget& aDt, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  DoPaintScrollbarThumb(aDt, aRect, aScrollbarKind, aFrame, aStyle,
                        aElementState, aDocumentState, aColors, aDpiRatio);
  return true;
}

bool ScrollbarDrawingAndroid::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    ScrollbarKind aScrollbarKind, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const ElementState& aElementState, const DocumentState& aDocumentState,
    const Colors& aColors, const DPIRatio& aDpiRatio) {
  DoPaintScrollbarThumb(aWrData, aRect, aScrollbarKind, aFrame, aStyle,
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
  ConfigureScrollbarSize(defaultSize);
  // We make thin scrollbars as wide as auto ones because auto scrollbars on
  // android are already thin enough.
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::Yes, defaultSize);
  ConfigureScrollbarSize(StyleScrollbarWidth::Thin, Overlay::No, defaultSize);
}
