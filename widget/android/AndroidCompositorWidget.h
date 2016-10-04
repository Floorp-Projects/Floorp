/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidCompositorWidget_h
#define mozilla_widget_AndroidCompositorWidget_h

#include "mozilla/widget/InProcessCompositorWidget.h"

namespace mozilla {
namespace widget {

/**
 * AndroidCompositorWidget inherits from InProcessCompositorWidget because
 * Android does not support OOP compositing yet. Once it does,
 * AndroidCompositorWidget will be made to inherit from CompositorWidget
 * instead.
 */
class AndroidCompositorWidget final : public InProcessCompositorWidget
{
public:
    using InProcessCompositorWidget::InProcessCompositorWidget;

    AndroidCompositorWidget* AsAndroid() override { return this; }

    void SetFirstPaintViewport(const LayerIntPoint& aOffset,
                               const CSSToLayerScale& aZoom,
                               const CSSRect& aCssPageRect);

    void SetPageRect(const CSSRect& aCssPageRect);

    void SyncViewportInfo(const LayerIntRect& aDisplayPort,
                          const CSSToLayerScale& aDisplayResolution,
                          bool aLayersUpdated,
                          int32_t aPaintSyncId,
                          ParentLayerRect& aScrollRect,
                          CSSToParentLayerScale& aScale,
                          ScreenMargin& aFixedLayerMargins);

    void SyncFrameMetrics(const ParentLayerPoint& aScrollOffset,
                          const CSSToParentLayerScale& aZoom,
                          const CSSRect& aCssPageRect,
                          const CSSRect& aDisplayPort,
                          const CSSToLayerScale& aPaintedResolution,
                          bool aLayersUpdated,
                          int32_t aPaintSyncId,
                          ScreenMargin& aFixedLayerMargins);
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_AndroidCompositorWidget_h
