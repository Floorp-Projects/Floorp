/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeGTK.h"

using namespace mozilla;

static constexpr CSSIntCoord kMinimumScrollbarSize = 12;

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
  switch (aAppearance) {
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
      // Make scrollbar tracks opaque on the window's scroll frame to prevent
      // leaf layers from overlapping. See bug 1179780.
      return IsRootScrollbar(aFrame) ? eOpaque : eTransparent;
    default:
      return nsNativeBasicTheme::GetWidgetTransparency(aFrame, aAppearance);
  }
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

  uint32_t dpiRatio = GetDPIRatio(aFrame);
  auto size = static_cast<uint32_t>(kMinimumScrollbarSize) * dpiRatio;
  aResult->SizeTo(size, size);

  *aIsOverridable = true;
  return NS_OK;
}

static sRGBColor GetScrollbarthumbColor(const EventStates& aState) {
  if (aState.HasAllStates(NS_EVENT_STATE_ACTIVE)) {
    return widget::sScrollbarThumbColorActive;
  }
  if (aState.HasAllStates(NS_EVENT_STATE_HOVER)) {
    return widget::sScrollbarThumbColorHover;
  }
  return widget::sScrollbarThumbColor;
}

void nsNativeBasicThemeGTK::PaintScrollbarthumbHorizontal(
    DrawTarget* aDrawTarget, const Rect& aRect, const EventStates& aState) {
  sRGBColor thumbColor = GetScrollbarthumbColor(aState);
  Rect thumbRect(aRect);
  thumbRect.Deflate(aRect.height / 4.0f);
  PaintRoundedRectWithRadius(aDrawTarget, thumbRect, thumbColor, sRGBColor(), 0,
                             thumbRect.height / 2.0f, 1);
}

void nsNativeBasicThemeGTK::PaintScrollbarthumbVertical(
    DrawTarget* aDrawTarget, const Rect& aRect, const EventStates& aState) {
  sRGBColor thumbColor = GetScrollbarthumbColor(aState);
  Rect thumbRect(aRect);
  thumbRect.Deflate(aRect.width / 4.0f);
  PaintRoundedRectWithRadius(aDrawTarget, thumbRect, thumbColor, sRGBColor(), 0,
                             thumbRect.width / 2.0f, 1);
}

void nsNativeBasicThemeGTK::PaintScrollbarHorizontal(DrawTarget* aDrawTarget,
                                                     const Rect& aRect,
                                                     bool aIsRoot) {
  if (!aIsRoot) {
    return;
  }
  aDrawTarget->FillRect(aRect,
                        ColorPattern(ToDeviceColor(widget::sScrollbarColor)));
}

void nsNativeBasicThemeGTK::PaintScrollbarVerticalAndCorner(
    DrawTarget* aDrawTarget, const Rect& aRect, uint32_t aDpiRatio,
    bool aIsRoot) {
  if (!aIsRoot) {
    return;
  }
  aDrawTarget->FillRect(aRect,
                        ColorPattern(ToDeviceColor(widget::sScrollbarColor)));
}
