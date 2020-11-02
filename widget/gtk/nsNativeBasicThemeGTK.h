/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicThemeGTK_h
#define nsNativeBasicThemeGTK_h

#include "nsNativeBasicTheme.h"

class nsNativeBasicThemeGTK : public nsNativeBasicTheme {
 public:
  nsNativeBasicThemeGTK() = default;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  nsITheme::Transparency GetWidgetTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;
  void PaintScrollbarThumb(DrawTarget* aDrawTarget, const Rect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState,
                           uint32_t aDpiRatio) override;
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
  virtual ~nsNativeBasicThemeGTK() = default;
};

#endif
