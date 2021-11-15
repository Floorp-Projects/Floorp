/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicThemeAndroid_h
#define nsNativeBasicThemeAndroid_h

#include "nsNativeBasicTheme.h"

class nsNativeBasicThemeAndroid final : public nsNativeBasicTheme {
 public:
  nsNativeBasicThemeAndroid() = default;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext*, nsIFrame*, StyleAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

  template <typename PaintBackendData>
  void DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect& aRect,
                             bool aHorizontal, nsIFrame* aFrame,
                             const ComputedStyle& aStyle,
                             const EventStates& aElementState,
                             const EventStates& aDocumentState, const Colors&,
                             DPIRatio);
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState, const Colors&,
                           DPIRatio) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState, const Colors&,
                           DPIRatio) override;

  bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState, const Colors&,
                           DPIRatio) override {
    // There's no visible track on android.
    return true;
  }
  bool PaintScrollbarTrack(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState, const Colors&,
                           DPIRatio) override {
    // There's no visible track on Android.
    return true;
  }

  bool PaintScrollbar(DrawTarget&, const LayoutDeviceRect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aElementState,
                      const EventStates& aDocumentState, const Colors&,
                      DPIRatio) override {
    // Draw nothing, we only draw the thumb.
    return true;
  }
  bool PaintScrollbar(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                      bool aHorizontal, nsIFrame* aFrame,
                      const ComputedStyle& aStyle,
                      const EventStates& aElementState,
                      const EventStates& aDocumentState, const Colors&,
                      DPIRatio) override {
    // Draw nothing, we only draw the thumb.
    return true;
  }

  bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState, const Colors&,
                         DPIRatio aDpiRatio) override {
    // Draw nothing, we only draw the thumb.
    return true;
  }
  bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState, const Colors&,
                         DPIRatio aDpiRatio) override {
    // Draw nothing, we only draw the thumb.
    return true;
  }

  bool ThemeSupportsScrollbarButtons() override { return false; }

 protected:
  virtual ~nsNativeBasicThemeAndroid() = default;
};

#endif
