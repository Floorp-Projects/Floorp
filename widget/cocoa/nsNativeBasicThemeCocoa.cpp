/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeCocoa.h"
#include "mozilla/gfx/Helpers.h"

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static mozilla::StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeBasicThemeCocoa();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}

NS_IMETHODIMP
nsNativeBasicThemeCocoa::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame, StyleAppearance aAppearance,
    mozilla::LayoutDeviceIntSize* aResult, bool* aIsOverridable) {
  uint32_t dpiRatio = GetDPIRatio(aFrame);

  switch (aAppearance) {
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight: {
      *aIsOverridable = false;
      *aResult = ScrollbarDrawingMac::GetMinimumWidgetSize(aAppearance, aFrame,
                                                           dpiRatio);
      break;
    }

    default:
      return nsNativeBasicTheme::GetMinimumWidgetSize(
          aPresContext, aFrame, aAppearance, aResult, aIsOverridable);
  }

  return NS_OK;
}

void nsNativeBasicThemeCocoa::PaintScrollbarThumb(
    DrawTarget* aDrawTarget, const Rect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState,
    uint32_t aDpiRatio) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, aHorizontal);
  if (aDpiRatio >= 2.0f) {
    mozilla::gfx::AutoRestoreTransform autoRestoreTransform(aDrawTarget);
    aDrawTarget->SetTransform(aDrawTarget->GetTransform().PreScale(2.0f, 2.0f));
    Rect rect = aRect;
    rect.Scale(1.0f / 2.0f);
    ScrollbarDrawingMac::DrawScrollbarThumb(*aDrawTarget, rect, params);
  } else {
    ScrollbarDrawingMac::DrawScrollbarThumb(*aDrawTarget, aRect, params);
  }
}

void nsNativeBasicThemeCocoa::PaintScrollbarTrack(
    DrawTarget* aDrawTarget, const Rect& aRect, bool aHorizontal,
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState, uint32_t aDpiRatio, bool aIsRoot) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, aHorizontal);
  if (aDpiRatio >= 2.0f) {
    mozilla::gfx::AutoRestoreTransform autoRestoreTransform(aDrawTarget);
    aDrawTarget->SetTransform(aDrawTarget->GetTransform().PreScale(2.0f, 2.0f));
    Rect rect = aRect;
    rect.Scale(1.0f / 2.0f);
    ScrollbarDrawingMac::DrawScrollbarTrack(*aDrawTarget, rect, params);
  } else {
    ScrollbarDrawingMac::DrawScrollbarTrack(*aDrawTarget, aRect, params);
  }
}

void nsNativeBasicThemeCocoa::PaintScrollbar(DrawTarget* aDrawTarget,
                                             const Rect& aRect,
                                             bool aHorizontal, nsIFrame* aFrame,
                                             const ComputedStyle& aStyle,
                                             const EventStates& aDocumentState,
                                             uint32_t aDpiRatio, bool aIsRoot) {
  // Draw nothing; the scrollbar track is drawn in PaintScrollbarTrack.
}

void nsNativeBasicThemeCocoa::PaintScrollCorner(
    DrawTarget* aDrawTarget, const Rect& aRect, nsIFrame* aFrame,
    const ComputedStyle& aStyle, const EventStates& aDocumentState,
    uint32_t aDpiRatio, bool aIsRoot) {
  ScrollbarParams params =
      ScrollbarDrawingMac::ComputeScrollbarParams(aFrame, aStyle, false);
  if (aDpiRatio >= 2.0f) {
    mozilla::gfx::AutoRestoreTransform autoRestoreTransform(aDrawTarget);
    aDrawTarget->SetTransform(aDrawTarget->GetTransform().PreScale(2.0f, 2.0f));
    Rect rect = aRect;
    rect.Scale(1 / 2.0f);
    ScrollbarDrawingMac::DrawScrollCorner(*aDrawTarget, rect, params);
  } else {
    ScrollbarDrawingMac::DrawScrollCorner(*aDrawTarget, aRect, params);
  }
}
