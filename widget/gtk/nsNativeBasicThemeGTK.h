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

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame*,
                                  StyleAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  nsITheme::Transparency GetWidgetTransparency(nsIFrame*,
                                               StyleAppearance) override;
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&,
                           bool aHorizontal, nsIFrame*,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState, UseSystemColors,
                           DPIRatio) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect&,
                           bool aHorizontal, nsIFrame*,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState, UseSystemColors,
                           DPIRatio) override;
  template <typename PaintBackendData>
  bool DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect&,
                             bool aHorizontal, nsIFrame*, const ComputedStyle&,
                             const EventStates& aElementState,
                             const EventStates& aDocumentState, UseSystemColors,
                             DPIRatio);

  bool ThemeSupportsScrollbarButtons() override;
  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

 protected:
  virtual ~nsNativeBasicThemeGTK() = default;
};

#endif
