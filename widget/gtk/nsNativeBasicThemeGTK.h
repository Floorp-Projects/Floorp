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
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState,
                           DPIRatio aDpiRatio) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState,
                           DPIRatio aDpiRatio) override;
  template <typename PaintBackendData>
  bool DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect& aRect,
                             bool aHorizontal, nsIFrame* aFrame,
                             const ComputedStyle& aStyle,
                             const EventStates& aElementState,
                             const EventStates& aDocumentState,
                             DPIRatio aDpiRatio);

  bool PaintScrollbar(DrawTarget&, const LayoutDeviceRect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aDocumentState,
                      DPIRatio aDpiRatio) override;
  bool PaintScrollbar(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aDocumentState,
                      DPIRatio aDpiRatio) override;
  template <typename PaintBackendData>
  bool DoPaintScrollbar(PaintBackendData&, const LayoutDeviceRect& aRect,
                        bool aHorizontal, nsIFrame* aFrame,
                        const ComputedStyle& aStyle,
                        const EventStates& aDocumentState, DPIRatio aDpiRatio);

  bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState,
                         DPIRatio aDpiRatio) override;
  bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState,
                         DPIRatio aDpiRatio) override;
  template <typename PaintBackendData>
  bool DoPaintScrollCorner(PaintBackendData&, const LayoutDeviceRect& aRect,
                           nsIFrame* aFrame, const ComputedStyle& aStyle,
                           const EventStates& aDocumentState,
                           DPIRatio aDpiRatio);

  bool ThemeSupportsScrollbarButtons() override;
  sRGBColor ComputeScrollbarThumbColor(
      nsIFrame*, const ComputedStyle&, const EventStates& aElementState,
      const EventStates& aDocumentState) override;
  std::pair<sRGBColor, sRGBColor> ComputeScrollbarColors(
      nsIFrame*, const ComputedStyle&,
      const EventStates& aDocumentState) override;
  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

 protected:
  virtual ~nsNativeBasicThemeGTK() = default;
};

#endif
