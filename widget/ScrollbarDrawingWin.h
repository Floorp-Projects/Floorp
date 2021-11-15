/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawingWin_h
#define mozilla_widget_ScrollbarDrawingWin_h

#include "nsITheme.h"
#include "nsNativeTheme.h"
#include "ScrollbarDrawing.h"

namespace mozilla::widget {

class ScrollbarDrawingWin final : public ScrollbarDrawing {
 public:
  ScrollbarDrawingWin() = default;
  virtual ~ScrollbarDrawingWin() = default;

  LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*,
                                           StyleAppearance aAppearance,
                                           nsIFrame* aFrame) override;

  Maybe<nsITheme::Transparency> GetScrollbarPartTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;

  static ComputedStyle* GetCustomScrollbarStyle(nsIFrame* aFrame,
                                                bool* aDarkScrollbar = nullptr);

  template <typename PaintBackendData>
  bool DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect&,
                             bool aHorizontal, nsIFrame*, const ComputedStyle&,
                             const EventStates& aElementState,
                             const EventStates& aDocumentState, const Colors&,
                             const DPIRatio&);
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&,
                           bool aHorizontal, nsIFrame*, const ComputedStyle&,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState, const Colors&,
                           const DPIRatio&) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect&,
                           bool aHorizontal, nsIFrame*, const ComputedStyle&,
                           const EventStates& aElementState,
                           const EventStates& aDocumentState, const Colors&,
                           const DPIRatio&) override;

  void RecomputeScrollbarParams() override;
};

}  // namespace mozilla::widget

#endif
