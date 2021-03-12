/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeGTK.h"

#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_widget.h"

using namespace mozilla;

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeBasicThemeGTK();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}

nsITheme::Transparency nsNativeBasicThemeGTK::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::ScrollbarVertical ||
      aAppearance == StyleAppearance::ScrollbarHorizontal) {
    nsPresContext* pc = aFrame->PresContext();
    auto docState = pc->Document()->GetDocumentState();
    const auto useSystemColors = ShouldUseSystemColors(*pc->Document());
    const auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
    auto trackColor =
        ComputeScrollbarColor(aFrame, *style, docState, useSystemColors);
    return trackColor.a == 1.0 ? eOpaque : eTransparent;
  }
  return nsNativeBasicTheme::GetWidgetTransparency(aFrame, aAppearance);
}

bool nsNativeBasicThemeGTK::ThemeSupportsScrollbarButtons() {
  return StaticPrefs::widget_non_native_theme_gtk_scrollbar_allow_buttons();
}

auto nsNativeBasicThemeGTK::GetScrollbarSizes(nsPresContext* aPresContext,
                                              StyleScrollbarWidth aWidth,
                                              Overlay) -> ScrollbarSizes {
  DPIRatio dpiRatio = GetDPIRatioForScrollbarPart(aPresContext);
  CSSCoord size =
      aWidth == StyleScrollbarWidth::Thin
          ? StaticPrefs::widget_non_native_theme_gtk_scrollbar_thin_size()
          : StaticPrefs::widget_non_native_theme_gtk_scrollbar_normal_size();
  LayoutDeviceIntCoord s = (size * dpiRatio).Truncated();
  return {s, s};
}

NS_IMETHODIMP
nsNativeBasicThemeGTK::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                            nsIFrame* aFrame,
                                            StyleAppearance aAppearance,
                                            LayoutDeviceIntSize* aResult,
                                            bool* aIsOverridable) {
  if (!IsWidgetScrollbarPart(aAppearance)) {
    return nsNativeBasicTheme::GetMinimumWidgetSize(
        aPresContext, aFrame, aAppearance, aResult, aIsOverridable);
  }

  DPIRatio dpiRatio = GetDPIRatioForScrollbarPart(aPresContext);
  ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
  auto sizes = GetScrollbarSizes(
      aPresContext, style->StyleUIReset()->mScrollbarWidth, Overlay::No);
  MOZ_ASSERT(sizes.mHorizontal == sizes.mVertical);
  aResult->SizeTo(sizes.mHorizontal, sizes.mHorizontal);

  if (aAppearance == StyleAppearance::ScrollbarHorizontal ||
      aAppearance == StyleAppearance::ScrollbarVertical ||
      aAppearance == StyleAppearance::ScrollbarthumbHorizontal ||
      aAppearance == StyleAppearance::ScrollbarthumbVertical) {
    CSSCoord thumbSize(
        StaticPrefs::widget_non_native_theme_gtk_scrollbar_thumb_cross_size());
    const bool isVertical =
        aAppearance == StyleAppearance::ScrollbarVertical ||
        aAppearance == StyleAppearance::ScrollbarthumbVertical;
    if (isVertical) {
      aResult->height = thumbSize * dpiRatio;
    } else {
      aResult->width = thumbSize * dpiRatio;
    }
  }

  *aIsOverridable = true;
  return NS_OK;
}

template <typename PaintBackendData>
bool nsNativeBasicThemeGTK::DoPaintScrollbarThumb(
    PaintBackendData& aPaintData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    UseSystemColors aUseSystemColors, DPIRatio aDpiRatio) {
  sRGBColor thumbColor = ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState, aUseSystemColors);

  LayoutDeviceRect thumbRect(aRect);

  {
    float factor = std::max(
        0.0f,
        1.0f - StaticPrefs::widget_non_native_theme_gtk_scrollbar_thumb_size());
    thumbRect.Deflate((aHorizontal ? aRect.height : aRect.width) * factor);
  }

  LayoutDeviceCoord radius =
      StaticPrefs::widget_non_native_theme_gtk_scrollbar_round_thumb()
          ? (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f
          : 0.0f;

  PaintRoundedRectWithRadius(aPaintData, thumbRect, thumbColor, sRGBColor(), 0,
                             radius / aDpiRatio, aDpiRatio);
  return true;
}

bool nsNativeBasicThemeGTK::PaintScrollbarThumb(
    DrawTarget& aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    UseSystemColors aUseSystemColors, DPIRatio aDpiRatio) {
  return DoPaintScrollbarThumb(aDrawTarget, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aUseSystemColors,
                               aDpiRatio);
}

bool nsNativeBasicThemeGTK::PaintScrollbarThumb(
    WebRenderBackendData& aWrData, const LayoutDeviceRect& aRect,
    bool aHorizontal, nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    UseSystemColors aUseSystemColors, DPIRatio aDpiRatio) {
  return DoPaintScrollbarThumb(aWrData, aRect, aHorizontal, aFrame, aStyle,
                               aElementState, aDocumentState, aUseSystemColors,
                               aDpiRatio);
}
