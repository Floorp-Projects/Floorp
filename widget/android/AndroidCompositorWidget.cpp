/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidCompositorWidget.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

void
AndroidCompositorWidget::SetFirstPaintViewport(const LayerIntPoint& aOffset,
                                               const CSSToLayerScale& aZoom,
                                               const CSSRect& aCssPageRect)
{
    auto layerClient = static_cast<nsWindow*>(RealWidget())->GetLayerClient();
    if (!layerClient) {
        return;
    }

    layerClient->SetFirstPaintViewport(
            float(aOffset.x), float(aOffset.y), aZoom.scale, aCssPageRect.x,
            aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidCompositorWidget::SetPageRect(const CSSRect& aCssPageRect)
{
    auto layerClient = static_cast<nsWindow*>(RealWidget())->GetLayerClient();
    if (!layerClient) {
        return;
    }

    layerClient->SetPageRect(aCssPageRect.x, aCssPageRect.y,
                             aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidCompositorWidget::SyncViewportInfo(const LayerIntRect& aDisplayPort,
                                          const CSSToLayerScale& aDisplayResolution,
                                          bool aLayersUpdated,
                                          int32_t aPaintSyncId,
                                          ParentLayerRect& aScrollRect,
                                          CSSToParentLayerScale& aScale,
                                          ScreenMargin& aFixedLayerMargins)
{
    auto layerClient = static_cast<nsWindow*>(RealWidget())->GetLayerClient();
    if (!layerClient) {
        return;
    }

    java::ViewTransform::LocalRef viewTransform = layerClient->SyncViewportInfo(
            aDisplayPort.x, aDisplayPort.y, aDisplayPort.width,
            aDisplayPort.height, aDisplayResolution.scale, aLayersUpdated,
            aPaintSyncId);

    MOZ_ASSERT(viewTransform, "No view transform object!");

    aScrollRect = ParentLayerRect(
        viewTransform->X(), viewTransform->Y(),
        viewTransform->Width(), viewTransform->Height());

    aScale.scale = viewTransform->Scale();

    aFixedLayerMargins.top = viewTransform->FixedLayerMarginTop();
    aFixedLayerMargins.right = viewTransform->FixedLayerMarginRight();
    aFixedLayerMargins.bottom = viewTransform->FixedLayerMarginBottom();
    aFixedLayerMargins.left = viewTransform->FixedLayerMarginLeft();
}

void
AndroidCompositorWidget::SyncFrameMetrics(const ParentLayerPoint& aScrollOffset,
                                          const CSSToParentLayerScale& aZoom,
                                          const CSSRect& aCssPageRect,
                                          const CSSRect& aDisplayPort,
                                          const CSSToLayerScale& aPaintedResolution,
                                          bool aLayersUpdated,
                                          int32_t aPaintSyncId,
                                          ScreenMargin& aFixedLayerMargins)
{
    auto layerClient = static_cast<nsWindow*>(RealWidget())->GetLayerClient();
    if (!layerClient) {
        return;
    }

    // convert the displayport rect from document-relative CSS pixels to
    // document-relative device pixels
    LayerIntRect dp = gfx::RoundedToInt(aDisplayPort * aPaintedResolution);

    java::ViewTransform::LocalRef viewTransform = layerClient->SyncFrameMetrics(
            aScrollOffset.x, aScrollOffset.y, aZoom.scale,
            aCssPageRect.x, aCssPageRect.y,
            aCssPageRect.XMost(), aCssPageRect.YMost(),
            dp.x, dp.y, dp.width, dp.height,
            aPaintedResolution.scale, aLayersUpdated, aPaintSyncId);

    MOZ_ASSERT(viewTransform, "No view transform object!");

    aFixedLayerMargins.top = viewTransform->FixedLayerMarginTop();
    aFixedLayerMargins.right = viewTransform->FixedLayerMarginRight();
    aFixedLayerMargins.bottom = viewTransform->FixedLayerMarginBottom();
    aFixedLayerMargins.left = viewTransform->FixedLayerMarginLeft();
}

} // namespace widget
} // namespace mozilla
