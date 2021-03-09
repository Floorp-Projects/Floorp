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

  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

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
                           DPIRatio aDpiRatio) override {
    // TODO: Seems this should be relatively straight-forward.
    return false;
  }

  bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState,
                           DPIRatio aDpiRatio) override;
  bool PaintScrollbarTrack(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState,
                           DPIRatio aDpiRatio) override {
    // TODO: Seems this should be relatively straight-forward.
    return false;
  }

  bool PaintScrollbar(DrawTarget&, const LayoutDeviceRect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aDocumentState,
                      DPIRatio aDpiRatio) override {
    // Draw nothing; the scrollbar track is drawn in PaintScrollbarTrack.
    return true;
  }
  bool PaintScrollbar(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aDocumentState,
                      DPIRatio aDpiRatio) override {
    // Draw nothing; the scrollbar track is drawn in PaintScrollbarTrack.
    return true;
  }

  bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState,
                         DPIRatio aDpiRatio) override;
  bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState,
                         DPIRatio aDpiRatio) override {
    // TODO: Seems this should be relatively straight-forward.
    return false;
  }

  ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame*,
                                               StyleAppearance) override;
  bool ThemeSupportsWidget(nsPresContext*, nsIFrame*, StyleAppearance) override;

 protected:
  virtual ~nsNativeBasicThemeCocoa() = default;
};

#endif
