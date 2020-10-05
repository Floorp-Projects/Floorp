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
  void PaintScrollbarthumbHorizontal(DrawTarget* aDrawTarget, const Rect& aRect,
                                     const EventStates& aState) override;
  void PaintScrollbarthumbVertical(DrawTarget* aDrawTarget, const Rect& aRect,
                                   const EventStates& aState) override;
  void PaintScrollbarHorizontal(DrawTarget* aDrawTarget, const Rect& aRect,
                                bool aIsRoot) override;
  void PaintScrollbarVerticalAndCorner(DrawTarget* aDrawTarget,
                                       const Rect& aRect,
                                       uint32_t aDpiRatio,
                                       bool aIsRoot) override;

 protected:
  virtual ~nsNativeBasicThemeGTK() = default;
};

#endif
