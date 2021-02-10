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
using mozilla::gfx::sRGBColor;

static bool ShouldUseDarkScrollbar(nsIFrame* aFrame,
                                   const ComputedStyle& aStyle) {
  if (StaticPrefs::widget_disable_dark_scrollbar()) {
    return false;
  }
  if (aStyle.StyleUI()->mScrollbarColor.IsColors()) {
    return false;
  }
  return nsNativeTheme::IsDarkBackground(aFrame);
}

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
    auto docState = aFrame->PresContext()->Document()->GetDocumentState();
    const auto* style = nsLayoutUtils::StyleForScrollbar(aFrame);
    auto [trackColor, borderColor] =
        ComputeScrollbarColors(aFrame, *style, docState);
    Unused << borderColor;
    return trackColor.a == 1.0 ? eOpaque : eTransparent;
  }
  return nsNativeBasicTheme::GetWidgetTransparency(aFrame, aAppearance);
}

bool nsNativeBasicThemeGTK::ThemeSupportsScrollbarButtons() {
  return StaticPrefs::widget_gtk_non_native_scrollbar_allow_buttons();
}

auto nsNativeBasicThemeGTK::GetScrollbarSizes(nsPresContext* aPresContext,
                                              StyleScrollbarWidth aWidth,
                                              Overlay) -> ScrollbarSizes {
  DPIRatio dpiRatio = GetDPIRatioForScrollbarPart(aPresContext);
  CSSCoord size =
      aWidth == StyleScrollbarWidth::Thin
          ? StaticPrefs::widget_gtk_non_native_scrollbar_thin_size()
          : StaticPrefs::widget_gtk_non_native_scrollbar_normal_size();
  LayoutDeviceIntCoord s = (size * dpiRatio).Truncated();
  return {s, s};
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeGTK::ComputeScrollbarColors(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState) {
  if (ShouldUseDarkScrollbar(aFrame, aStyle)) {
    auto color = sRGBColor::FromU8(20, 20, 25, 77);
    return {color, color};
  }
  return nsNativeBasicTheme::ComputeScrollbarColors(aFrame, aStyle,
                                                    aDocumentState);
}

sRGBColor nsNativeBasicThemeGTK::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState) {
  if (ShouldUseDarkScrollbar(aFrame, aStyle)) {
    return sRGBColor::FromABGR(AdjustUnthemedScrollbarThumbColor(
        NS_RGBA(249, 249, 250, 102), aElementState));
  }
  return nsNativeBasicTheme::ComputeScrollbarThumbColor(
      aFrame, aStyle, aElementState, aDocumentState);
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
        StaticPrefs::widget_gtk_non_native_scrollbar_thumb_cross_size());
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

void nsNativeBasicThemeGTK::PaintScrollbarThumb(
    DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  sRGBColor thumbColor =
      ComputeScrollbarThumbColor(aFrame, aStyle, aElementState, aDocumentState);

  LayoutDeviceRect thumbRect(aRect);

  {
    float factor = std::max(
        0.0f, 1.0f - StaticPrefs::widget_gtk_non_native_scrollbar_thumb_size());
    thumbRect.Deflate((aHorizontal ? aRect.height : aRect.width) * factor);
  }

  LayoutDeviceCoord radius =
      StaticPrefs::widget_gtk_non_native_round_thumb()
          ? (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f
          : 0.0f;

  PaintRoundedRectWithRadius(aDrawTarget, thumbRect, thumbColor, sRGBColor(), 0,
                             radius / aDpiRatio, aDpiRatio);
}

void nsNativeBasicThemeGTK::PaintScrollbar(DrawTarget* aDrawTarget,
                                           const LayoutDeviceRect& aRect,
                                           bool aHorizontal, nsIFrame* aFrame,
                                           const ComputedStyle& aStyle,
                                           const EventStates& aDocumentState,
                                           DPIRatio aDpiRatio) {
  auto [trackColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  Unused << borderColor;
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        gfx::ColorPattern(ToDeviceColor(trackColor)));
}

void nsNativeBasicThemeGTK::PaintScrollCorner(DrawTarget* aDrawTarget,
                                              const LayoutDeviceRect& aRect,
                                              nsIFrame* aFrame,
                                              const ComputedStyle& aStyle,
                                              const EventStates& aDocumentState,
                                              DPIRatio aDpiRatio) {
  auto [trackColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  Unused << borderColor;
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        gfx::ColorPattern(ToDeviceColor(trackColor)));
}
