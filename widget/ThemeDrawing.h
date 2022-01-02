/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ThemeDrawing_h
#define mozilla_widget_ThemeDrawing_h

#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "RetainedDisplayListBuilder.h"

namespace mozilla::widget {

struct WebRenderBackendData {
  wr::DisplayListBuilder& mBuilder;
  wr::IpcResourceUpdateQueue& mResources;
  const layers::StackingContextHelper& mSc;
  layers::RenderRootStateManager* mManager;
};

class ThemeDrawing {
 protected:
  using DrawTarget = gfx::DrawTarget;
  using sRGBColor = gfx::sRGBColor;
  using DPIRatio = CSSToLayoutDeviceScale;

 public:
  virtual ~ThemeDrawing() = 0;

  static void FillRect(DrawTarget&, const LayoutDeviceRect&, const sRGBColor&);
  static void FillRect(WebRenderBackendData&, const LayoutDeviceRect&,
                       const sRGBColor&);

  // Returns the right scale for points in a aSize x aSize sized box, centered
  // at 0x0 to fill aRect in the smaller dimension.
  static float ScaleToFillRect(const LayoutDeviceRect& aRect,
                               const float& aSize) {
    return std::min(aRect.width, aRect.height) / aSize;
  }

  static LayoutDeviceIntCoord SnapBorderWidth(const CSSCoord& aCssWidth,
                                              const DPIRatio& aDpiRatio);

  static void PaintArrow(DrawTarget&, const LayoutDeviceRect&,
                         const float aArrowPolygonX[],
                         const float aArrowPolygonY[],
                         const float aArrowPolygonSize,
                         const int32_t aArrowNumPoints,
                         const sRGBColor aFillColor);

  static void PaintRoundedRectWithRadius(
      DrawTarget&, const LayoutDeviceRect& aRect,
      const LayoutDeviceRect& aClipRect, const sRGBColor& aBackgroundColor,
      const sRGBColor& aBorderColor, const CSSCoord& aBorderWidth,
      const CSSCoord& aRadius, const DPIRatio&);
  static void PaintRoundedRectWithRadius(
      WebRenderBackendData&, const LayoutDeviceRect& aRect,
      const LayoutDeviceRect& aClipRect, const sRGBColor& aBackgroundColor,
      const sRGBColor& aBorderColor, const CSSCoord& aBorderWidth,
      const CSSCoord& aRadius, const DPIRatio&);
  template <typename PaintBackendData>
  static void PaintRoundedRectWithRadius(PaintBackendData& aData,
                                         const LayoutDeviceRect& aRect,
                                         const sRGBColor& aBackgroundColor,
                                         const sRGBColor& aBorderColor,
                                         const CSSCoord& aBorderWidth,
                                         const CSSCoord& aRadius,
                                         const DPIRatio& aDpiRatio) {
    PaintRoundedRectWithRadius(aData, aRect, aRect, aBackgroundColor,
                               aBorderColor, aBorderWidth, aRadius, aDpiRatio);
  }
};

}  // namespace mozilla::widget

#endif
