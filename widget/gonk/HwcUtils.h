/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef mozilla_HwcUtils
#define mozilla_HwcUtils

#include "Layers.h"
#include <vector>
#include "hardware/hwcomposer.h"

namespace mozilla {

class HwcUtils {
public:

enum {
    HWC_USE_GPU = HWC_FRAMEBUFFER,
    HWC_USE_OVERLAY = HWC_OVERLAY,
    HWC_USE_COPYBIT
};

// HWC layer flags
enum {
    // Draw a solid color rectangle
    // The color should be set on the transform member of the hwc_layer_t struct
    // The expected format is a 32 bit ABGR with 8 bits per component
    HWC_COLOR_FILL = 0x8,
    // Swap the RB pixels of gralloc buffer, like RGBA<->BGRA or RGBX<->BGRX
    // The flag will be set inside LayerRenderState
    HWC_FORMAT_RB_SWAP = 0x40
};

typedef std::vector<hwc_rect_t> RectVector;

/* Utility functions - implemented in HwcUtils.cpp */

/**
 * Calculates the layer's clipping rectangle
 *
 * @param aTransform Input. A transformation matrix
 *        It transforms the clip rect to screen space
 * @param aLayerClip Input. The layer's internal clipping rectangle.
 *        This may be NULL which means the layer has no internal clipping
 *        The origin is the top-left corner of the layer
 * @param aParentClip Input. The parent layer's rendering clipping rectangle
 *        The origin is the top-left corner of the screen
 * @param aRenderClip Output. The layer's rendering clipping rectangle
 *        The origin is the top-left corner of the screen
 * @return true if the layer should be rendered.
 *         false if the layer can be skipped
 */
static bool CalculateClipRect(const gfxMatrix& aTransform,
                              const nsIntRect* aLayerClip,
                              nsIntRect aParentClip, nsIntRect* aRenderClip);


/**
 * Prepares hwc layer visible region required for hwc composition
 *
 * @param aVisible Input. Layer's unclipped visible region
 *        The origin is the top-left corner of the layer
 * @param aTransform Input. Layer's transformation matrix
 *        It transforms from layer space to screen space
 * @param aClip Input. A clipping rectangle.
 *        The origin is the top-left corner of the screen
 * @param aBufferRect Input. The layer's buffer bounds
 *        The origin is the top-left corner of the layer
 * @param aVisibleRegionScreen Output. Visible region in screen space.
 *        The origin is the top-left corner of the screen
 * @return true if the layer should be rendered.
 *         false if the layer can be skipped
 */
static bool PrepareVisibleRegion(const nsIntRegion& aVisible,
                                 const gfxMatrix& aTransform,
                                 nsIntRect aClip, nsIntRect aBufferRect,
                                 RectVector* aVisibleRegionScreen);


/**
 * Sets hwc layer rectangles required for hwc composition
 *
 * @param aVisible Input. Layer's unclipped visible rectangle
 *        The origin is the top-left corner of the layer
 * @param aTransform Input. Layer's transformation matrix
 *        It transforms from layer space to screen space
 * @param aClip Input. A clipping rectangle.
 *        The origin is the top-left corner of the screen
 * @param aBufferRect Input. The layer's buffer bounds
 *        The origin is the top-left corner of the layer
 * @param aYFlipped Input. true if the buffer is rendered as Y flipped
 * @param aSurceCrop Output. Area of the source to consider,
 *        the origin is the top-left corner of the buffer
 * @param aVisibleRegionScreen Output. Visible region in screen space.
 *        The origin is the top-left corner of the screen
 * @return true if the layer should be rendered.
 *         false if the layer can be skipped
 */
static bool PrepareLayerRects(nsIntRect aVisible, const gfxMatrix& aTransform,
                              nsIntRect aClip, nsIntRect aBufferRect,
                              bool aYFlipped,
                              hwc_rect_t* aSourceCrop,
                              hwc_rect_t* aVisibleRegionScreen);

};

} // namespace mozilla

#endif // mozilla_HwcUtils
