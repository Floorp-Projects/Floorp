/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeAndroid.h"

#include "mozilla/ClearOnShutdown.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"

auto nsNativeBasicThemeAndroid::GetScrollbarSizes(nsPresContext* aPresContext,
                                                  StyleScrollbarWidth aWidth,
                                                  Overlay aOverlay)
    -> ScrollbarSizes {
  // We force auto-width scrollbars because scrollbars on android are already
  // thin enough.
  return nsNativeBasicTheme::GetScrollbarSizes(
      aPresContext, StyleScrollbarWidth::Auto, aOverlay);
}

NS_IMETHODIMP
nsNativeBasicThemeAndroid::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    mozilla::LayoutDeviceIntSize* aResult, bool* aIsOverridable) {
  if (!IsWidgetScrollbarPart(aAppearance)) {
    return nsNativeBasicTheme::GetMinimumWidgetSize(
        aPresContext, aFrame, aAppearance, aResult, aIsOverridable);
  }

  auto sizes =
      GetScrollbarSizes(aPresContext, StyleScrollbarWidth::Auto, Overlay::Yes);
  aResult->SizeTo(sizes.mHorizontal, sizes.mHorizontal);
  MOZ_ASSERT(sizes.mHorizontal == sizes.mVertical);

  *aIsOverridable = true;
  return NS_OK;
}

template <typename PaintBackendData>
void nsNativeBasicThemeAndroid::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, DPIRatio aDpiRatio) {
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
  PaintRoundedRectWithRadius(aPaintData, thumbRect, color,
                             sRGBColor::White(0.0f), 0.0f, radius / aDpiRatio,
                             aDpiRatio);
}

bool nsNativeBasicThemeAndroid::PaintScrollbarThumb(
    DrawTarget& aDt, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, DPIRatio aDpiRatio) {
  DoPaintScrollbarThumb(aDt, aRect, aHorizontal, aFrame, aStyle, aElementState,
                        aDocumentState, aColors, aDpiRatio);
  return true;
}

bool nsNativeBasicThemeAndroid::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    const Colors& aColors, DPIRatio aDpiRatio) {
  DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                        aElementState, aDocumentState, aColors, aDpiRatio);
  return true;
}

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static mozilla::StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeBasicThemeAndroid();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}

already_AddRefed<nsITheme> do_GetNativeThemeDoNotUseDirectly() {
  // Android doesn't have a native theme.
  return do_GetBasicNativeThemeDoNotUseDirectly();
}
