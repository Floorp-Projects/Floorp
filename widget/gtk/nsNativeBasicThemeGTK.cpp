/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeGTK.h"

#include "nsLayoutUtils.h"
#include "mozilla/dom/Document.h"

using namespace mozilla;

static constexpr CSSCoord kGtkMinimumScrollbarSize = 12;
static constexpr CSSCoord kGtkMinimumThinScrollbarSize = 6;
static constexpr CSSCoord kGtkMinimumScrollbarThumbSize = 40;

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static mozilla::StaticRefPtr<nsITheme> gInstance;
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

auto nsNativeBasicThemeGTK::GetScrollbarSizes(nsPresContext* aPresContext,
                                              StyleScrollbarWidth aWidth,
                                              Overlay) -> ScrollbarSizes {
  DPIRatio dpiRatio = GetDPIRatioForScrollbarPart(aPresContext);
  CSSCoord size = aWidth == StyleScrollbarWidth::Thin
                      ? kGtkMinimumThinScrollbarSize
                      : kGtkMinimumScrollbarSize;
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

  switch (aAppearance) {
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarthumbHorizontal:
      aResult->width = kGtkMinimumScrollbarThumbSize * dpiRatio;
      break;
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarthumbVertical:
      aResult->height = kGtkMinimumScrollbarThumbSize * dpiRatio;
      break;
    default:
      break;
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
  thumbRect.Deflate(floorf((aHorizontal ? aRect.height : aRect.width) / 4.0f));
  LayoutDeviceCoord radius =
      (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f;
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

void nsNativeBasicThemeGTK::PaintScrollCorner(
    DrawTarget* aDrawTarget, const LayoutDeviceRect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    DPIRatio aDpiRatio) {
  auto [trackColor, borderColor] =
      ComputeScrollbarColors(aFrame, aStyle, aDocumentState);
  Unused << borderColor;
  aDrawTarget->FillRect(aRect.ToUnknownRect(),
                        gfx::ColorPattern(ToDeviceColor(trackColor)));
}
