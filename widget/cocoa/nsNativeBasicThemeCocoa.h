/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicThemeCocoa_h
#define nsNativeBasicThemeCocoa_h

#include "nsNativeBasicTheme.h"

#include "ScrollbarDrawingMac.h"

class nsNativeBasicThemeCocoa : public nsNativeBasicTheme {
 public:
  nsNativeBasicThemeCocoa() = default;

  using ScrollbarParams = mozilla::widget::ScrollbarParams;
  using ScrollbarDrawingMac = mozilla::widget::ScrollbarDrawingMac;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  void PaintScrollbarThumb(DrawTarget* aDrawTarget, const Rect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState,
                           uint32_t aDpiRatio) override;
  void PaintScrollbarTrack(DrawTarget* aDrawTarget, const Rect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState,
                           uint32_t aDpiRatio, bool aIsRoot) override;
  void PaintScrollbar(DrawTarget* aDrawTarget, const Rect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aDocumentState, uint32_t aDpiRatio,
                      bool aIsRoot) override;
  void PaintScrollCorner(DrawTarget* aDrawTarget, const Rect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState, uint32_t aDpiRatio,
                         bool aIsRoot) override;

 protected:
  virtual ~nsNativeBasicThemeCocoa() = default;
};

#endif
