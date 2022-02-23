/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawingCocoa_h
#define mozilla_widget_ScrollbarDrawingCocoa_h

#include "ScrollbarDrawing.h"

#include "mozilla/Array.h"

namespace mozilla::widget {

class ScrollbarDrawingCocoa final : public ScrollbarDrawing {
 public:
  ScrollbarDrawingCocoa() = default;
  virtual ~ScrollbarDrawingCocoa() = default;

  struct FillRectType {
    gfx::Rect mRect;
    nscolor mColor;
  };

  // The caller can draw this rectangle with rounded corners as appropriate.
  struct ThumbRect {
    gfx::Rect mRect;
    nscolor mFillColor;
    nscolor mStrokeColor;
    float mStrokeWidth;
    float mStrokeOutset;
  };

  using ScrollbarTrackRects = Array<FillRectType, 4>;
  using ScrollCornerRects = Array<FillRectType, 7>;

  LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*,
                                           StyleAppearance aAppearance,
                                           nsIFrame* aFrame) override;

  static CSSIntCoord GetScrollbarSize(StyleScrollbarWidth aWidth,
                                      bool aOverlay);
  static LayoutDeviceIntCoord GetScrollbarSize(StyleScrollbarWidth aWidth,
                                               bool aOverlay,
                                               DPIRatio aDpiRatio);
  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

  static ThumbRect GetThumbRect(const gfx::Rect& aRect,
                                const ScrollbarParams& aParams, float aScale);
  static bool GetScrollbarTrackRects(const gfx::Rect& aRect,
                                     const ScrollbarParams& aParams,
                                     float aScale, ScrollbarTrackRects& aRects);
  static bool GetScrollCornerRects(const gfx::Rect& aRect,
                                   const ScrollbarParams& aParams, float aScale,
                                   ScrollCornerRects& aRects);

  template <typename PaintBackendData>
  void DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect& aRect,
                             bool aHorizontal, nsIFrame* aFrame,
                             const ComputedStyle& aStyle,
                             const EventStates& aElementState,
                             const EventStates& aDocumentState,
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

  template <typename PaintBackendData>
  void DoPaintScrollbarTrack(PaintBackendData&, const LayoutDeviceRect&, bool,
                             nsIFrame*, const ComputedStyle&,
                             const EventStates&, const DPIRatio&);
  bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState, const Colors&,
                           const DPIRatio&) override;
  bool PaintScrollbarTrack(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                           bool aHorizontal, nsIFrame* aFrame,
                           const ComputedStyle& aStyle,
                           const EventStates& aDocumentState, const Colors&,
                           const DPIRatio&) override;

  template <typename PaintBackendData>
  void DoPaintScrollCorner(PaintBackendData&, const LayoutDeviceRect&,
                           nsIFrame*, const ComputedStyle&, const EventStates&,
                           const DPIRatio&);
  bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState, const Colors&,
                         const DPIRatio&) override;
  bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                         nsIFrame* aFrame, const ComputedStyle& aStyle,
                         const EventStates& aDocumentState, const Colors&,
                         const DPIRatio&) override;

  void RecomputeScrollbarParams() override;

  bool ShouldDrawScrollbarButtons() override { return false; }
};

}  // namespace mozilla::widget

#endif
