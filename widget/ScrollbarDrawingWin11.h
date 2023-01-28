/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ScrollbarDrawingWin11_h
#define mozilla_widget_ScrollbarDrawingWin11_h

#include "nsITheme.h"
#include "nsNativeTheme.h"
#include "ScrollbarDrawing.h"
#include "ScrollbarDrawingWin.h"

namespace mozilla::widget {

class ScrollbarDrawingWin11 final : public ScrollbarDrawingWin {
 public:
  ScrollbarDrawingWin11() : ScrollbarDrawingWin(Kind::Win11) {}
  virtual ~ScrollbarDrawingWin11() = default;

  LayoutDeviceIntSize GetMinimumWidgetSize(nsPresContext*,
                                           StyleAppearance aAppearance,
                                           nsIFrame* aFrame) override;

  sRGBColor ComputeScrollbarTrackColor(nsIFrame*, const ComputedStyle&,
                                       const DocumentState& aDocumentState,
                                       const Colors&) override;
  sRGBColor ComputeScrollbarThumbColor(nsIFrame*, const ComputedStyle&,
                                       const ElementState& aElementState,
                                       const DocumentState& aDocumentState,
                                       const Colors&) override;

  // Returned colors are button, arrow.
  std::pair<sRGBColor, sRGBColor> ComputeScrollbarButtonColors(
      nsIFrame*, StyleAppearance, const ComputedStyle&,
      const ElementState& aElementState, const DocumentState& aDocumentState,
      const Colors&) override;

  bool PaintScrollbarButton(DrawTarget&, StyleAppearance,
                            const LayoutDeviceRect&, ScrollbarKind, nsIFrame*,
                            const ComputedStyle&,
                            const ElementState& aElementState,
                            const DocumentState& aDocumentState, const Colors&,
                            const DPIRatio&) override;

  template <typename PaintBackendData>
  bool DoPaintScrollbarThumb(PaintBackendData&, const LayoutDeviceRect&,
                             ScrollbarKind, nsIFrame*, const ComputedStyle&,
                             const ElementState& aElementState,
                             const DocumentState& aDocumentState, const Colors&,
                             const DPIRatio&);
  bool PaintScrollbarThumb(DrawTarget&, const LayoutDeviceRect&, ScrollbarKind,
                           nsIFrame*, const ComputedStyle&,
                           const ElementState& aElementState,
                           const DocumentState& aDocumentState, const Colors&,
                           const DPIRatio&) override;
  bool PaintScrollbarThumb(WebRenderBackendData&, const LayoutDeviceRect&,
                           ScrollbarKind, nsIFrame*, const ComputedStyle&,
                           const ElementState& aElementState,
                           const DocumentState& aDocumentState, const Colors&,
                           const DPIRatio&) override;

  void RecomputeScrollbarParams() override;
};

}  // namespace mozilla::widget

#endif
