/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeGTK.h"

#include "nsLayoutUtils.h"

using namespace mozilla;

static constexpr CSSIntCoord kGtkMinimumScrollbarSize = 12;
static constexpr CSSIntCoord kGtkMinimumThinScrollbarSize = 6;
static constexpr CSSIntCoord kGtkMinimumScrollbarThumbSize = 40;

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
  uint32_t dpiRatio = GetDPIRatio(aFrame);

  switch (aAppearance) {
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::Scrollcorner: {
      ComputedStyle* style = nsLayoutUtils::StyleForScrollbar(aFrame);
      if (style->StyleUIReset()->mScrollbarWidth == StyleScrollbarWidth::Thin) {
        aResult->SizeTo(
            static_cast<uint32_t>(kGtkMinimumThinScrollbarSize) * dpiRatio,
            static_cast<uint32_t>(kGtkMinimumThinScrollbarSize) * dpiRatio);
      } else {
        aResult->SizeTo(
            static_cast<uint32_t>(kGtkMinimumScrollbarSize) * dpiRatio,
            static_cast<uint32_t>(kGtkMinimumScrollbarSize) * dpiRatio);
      }
      break;
    }
    default:
      return nsNativeBasicTheme::GetMinimumWidgetSize(
          aPresContext, aFrame, aAppearance, aResult, aIsOverridable);
  }

  switch (aAppearance) {
    case StyleAppearance::ScrollbarthumbHorizontal:
      aResult->width =
          static_cast<uint32_t>(kGtkMinimumScrollbarThumbSize) * dpiRatio;
      break;
    case StyleAppearance::ScrollbarthumbVertical:
      aResult->height =
          static_cast<uint32_t>(kGtkMinimumScrollbarThumbSize) * dpiRatio;
      break;
    default:
      break;
  }

  *aIsOverridable = true;
  return NS_OK;
}

void nsNativeBasicThemeGTK::PaintScrollbarThumb(
    DrawTarget* aDrawTarget, const Rect& aRect, bool aHorizontal,
    const ComputedStyle& aStyle, const EventStates& aElementState,
    const EventStates& aDocumentState, uint32_t aDpiRatio) {
  sRGBColor thumbColor =
      ComputeScrollbarthumbColor(aStyle, aElementState, aDocumentState);
  Rect thumbRect(aRect);
  thumbRect.Deflate(floorf((aHorizontal ? aRect.height : aRect.width) / 4.0f));
  auto radius = (aHorizontal ? thumbRect.height : thumbRect.width) / 2.0f;
  PaintRoundedRectWithRadius(aDrawTarget, thumbRect, thumbColor, sRGBColor(), 0,
                             radius, 1);
}

void nsNativeBasicThemeGTK::PaintScrollbar(DrawTarget* aDrawTarget,
                                           const Rect& aRect, bool aHorizontal,
                                           const ComputedStyle& aStyle,
                                           const EventStates& aDocumentState,
                                           uint32_t aDpiRatio, bool aIsRoot) {
  sRGBColor trackColor = ComputeScrollbarColor(aStyle, aDocumentState, aIsRoot);
  aDrawTarget->FillRect(aRect, gfx::ColorPattern(ToDeviceColor(trackColor)));
}

void nsNativeBasicThemeGTK::PaintScrollCorner(
    DrawTarget* aDrawTarget, const Rect& aRect, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, uint32_t aDpiRatio, bool aIsRoot) {
  sRGBColor trackColor = ComputeScrollbarColor(aStyle, aDocumentState, aIsRoot);
  aDrawTarget->FillRect(aRect, gfx::ColorPattern(ToDeviceColor(trackColor)));
}
