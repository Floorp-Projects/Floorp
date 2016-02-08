/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h>
#include "HwcUtils.h"
#include "gfxUtils.h"
#include "gfx2DGlue.h"

#define LOG_TAG "HwcUtils"

#if (LOG_NDEBUG == 0)
#define LOGD(args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ## args)
#else
#define LOGD(args...) ((void)0)
#endif

#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ## args)


namespace mozilla {

/* Utility functions for HwcComposer */



/* static */ bool
HwcUtils::PrepareLayerRects(nsIntRect aVisible,
                            const gfx::Matrix& aLayerTransform,
                            const gfx::Matrix& aLayerBufferTransform,
                            nsIntRect aClip, nsIntRect aBufferRect,
                            bool aYFlipped,
                            hwc_rect_t* aSourceCrop, hwc_rect_t* aVisibleRegionScreen) {

    gfxMatrix aTransform = gfx::ThebesMatrix(aLayerTransform);
    gfxRect visibleRect(ThebesRect(aVisible));
    gfxRect clip(ThebesRect(aClip));
    gfxRect visibleRectScreen = aTransform.TransformBounds(visibleRect);
    // |clip| is guaranteed to be integer
    visibleRectScreen.IntersectRect(visibleRectScreen, clip);

    if (visibleRectScreen.IsEmpty()) {
        return false;
    }

    gfxMatrix inverse = gfx::ThebesMatrix(aLayerBufferTransform);
    inverse.Invert();
    gfxRect crop = inverse.TransformBounds(visibleRectScreen);

    //clip to buffer size
    crop.IntersectRect(crop, ThebesRect(aBufferRect));
    crop.Round();

    if (crop.IsEmpty()) {
        return false;
    }

    //propagate buffer clipping back to visible rect
    gfxMatrix layerBufferTransform = gfx::ThebesMatrix(aLayerBufferTransform);
    visibleRectScreen = layerBufferTransform.TransformBounds(crop);
    visibleRectScreen.Round();

    // Map from layer space to buffer space
    crop -= aBufferRect.TopLeft();
    if (aYFlipped) {
        crop.y = aBufferRect.height - (crop.y + crop.height);
    }

    aSourceCrop->left = crop.x;
    aSourceCrop->top  = crop.y;
    aSourceCrop->right  = crop.x + crop.width;
    aSourceCrop->bottom = crop.y + crop.height;

    aVisibleRegionScreen->left = visibleRectScreen.x;
    aVisibleRegionScreen->top  = visibleRectScreen.y;
    aVisibleRegionScreen->right  = visibleRectScreen.x + visibleRectScreen.width;
    aVisibleRegionScreen->bottom = visibleRectScreen.y + visibleRectScreen.height;

    return true;
}

/* static */ bool
HwcUtils::PrepareVisibleRegion(const nsIntRegion& aVisible,
                               const gfx::Matrix& aLayerTransform,
                               const gfx::Matrix& aLayerBufferTransform,
                               nsIntRect aClip, nsIntRect aBufferRect,
                               RectVector* aVisibleRegionScreen,
                               bool& aIsVisible) {
    const float MIN_SRC_WIDTH = 2.f;
    const float MIN_SRC_HEIGHT = 2.f;

    gfxMatrix layerTransform = gfx::ThebesMatrix(aLayerTransform);
    gfxMatrix layerBufferTransform = gfx::ThebesMatrix(aLayerBufferTransform);
    gfxRect bufferRect =
        layerBufferTransform.TransformBounds(ThebesRect(aBufferRect));
    gfxMatrix inverse = gfx::ThebesMatrix(aLayerBufferTransform);
    inverse.Invert();
    aIsVisible = false;

    for (auto iter = aVisible.RectIter(); !iter.Done(); iter.Next()) {
        gfxRect screenRect =
          layerTransform.TransformBounds(ThebesRect(iter.Get()));
        screenRect.IntersectRect(screenRect, bufferRect);
        screenRect.IntersectRect(screenRect, ThebesRect(aClip));
        screenRect.Round();
        if (screenRect.IsEmpty()) {
            continue;
        }

        hwc_rect_t visibleRectScreen;
        visibleRectScreen.left = screenRect.x;
        visibleRectScreen.top  = screenRect.y;
        visibleRectScreen.right  = screenRect.XMost();
        visibleRectScreen.bottom = screenRect.YMost();

        gfxRect srcCrop = inverse.TransformBounds(screenRect);
        // When src crop is very small, HWC could not render correctly in some cases.
        // See Bug 1169093
        if(srcCrop.Width() < MIN_SRC_WIDTH || srcCrop.Height() < MIN_SRC_HEIGHT) {
            return false;
        }

        aVisibleRegionScreen->push_back(visibleRectScreen);
        aIsVisible = true;
    }

    return true;
}

/* static */ bool
HwcUtils::CalculateClipRect(const gfx::Matrix& transform,
                            const nsIntRect* aLayerClip,
                            nsIntRect aParentClip, nsIntRect* aRenderClip) {

    gfxMatrix aTransform = gfx::ThebesMatrix(transform);
    *aRenderClip = aParentClip;

    if (!aLayerClip) {
        return true;
    }

    if (aLayerClip->IsEmpty()) {
        return false;
    }

    nsIntRect clip = *aLayerClip;

    gfxRect r = ThebesRect(clip);
    gfxRect trClip = aTransform.TransformBounds(r);
    trClip.Round();
    gfxUtils::GfxRectToIntRect(trClip, &clip);

    aRenderClip->IntersectRect(*aRenderClip, clip);
    return true;
}

} // namespace mozilla
