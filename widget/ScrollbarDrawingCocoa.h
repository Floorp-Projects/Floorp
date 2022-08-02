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
  ScrollbarDrawingCocoa() : ScrollbarDrawing(Kind::Cocoa) {}
  virtual ~ScrollbarDrawingCocoa() = default;

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

  template <typename PaintBackendData>
  void DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect& aRect,
                             ScrollbarKind, nsIFrame* aFrame,
                             const ComputedStyle& aStyle, const ElementState&,
                             const DocumentState&, const Colors&,
                             const DPIRatio&);
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&, ScrollbarKind,
                           nsIFrame*, const ComputedStyle&, const ElementState&,
                           const DocumentState&, const Colors&,
                           const DPIRatio&) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect&,
                           ScrollbarKind, nsIFrame*, const ComputedStyle&,
                           const ElementState&, const DocumentState&,
                           const Colors&, const DPIRatio&) override;

  template <typename PaintBackendData>
  void DoPaintScrollbarTrack(PaintBackendData&, const LayoutDeviceRect&,
                             ScrollbarKind, nsIFrame*, const ComputedStyle&,
                             const DocumentState&, const Colors&,
                             const DPIRatio&);
  bool PaintScrollbarTrack(DrawTarget&, const LayoutDeviceRect& aRect,
                           ScrollbarKind, nsIFrame* aFrame,
                           const ComputedStyle& aStyle, const DocumentState&,
                           const Colors&, const DPIRatio&) override;
  bool PaintScrollbarTrack(WebRenderBackendData&, const LayoutDeviceRect& aRect,
                           ScrollbarKind, nsIFrame* aFrame,
                           const ComputedStyle& aStyle, const DocumentState&,
                           const Colors&, const DPIRatio&) override;

  template <typename PaintBackendData>
  void DoPaintScrollCorner(PaintBackendData&, const LayoutDeviceRect&,
                           ScrollbarKind, nsIFrame*, const ComputedStyle&,
                           const DocumentState&, const Colors&,
                           const DPIRatio&);
  bool PaintScrollCorner(DrawTarget&, const LayoutDeviceRect&, ScrollbarKind,
                         nsIFrame*, const ComputedStyle&, const DocumentState&,
                         const Colors&, const DPIRatio&) override;
  bool PaintScrollCorner(WebRenderBackendData&, const LayoutDeviceRect&,
                         ScrollbarKind, nsIFrame*, const ComputedStyle&,
                         const DocumentState&, const Colors&,
                         const DPIRatio&) override;

  void RecomputeScrollbarParams() override;

  bool ShouldDrawScrollbarButtons() override { return false; }
};

}  // namespace mozilla::widget

#endif
