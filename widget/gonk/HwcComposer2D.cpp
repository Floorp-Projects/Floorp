/*
 * Copyright (c) 2012, 2013 The Linux Foundation. All rights reserved.
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
#include <string.h>

#include "libdisplay/GonkDisplay.h"
#include "Framebuffer.h"
#include "HwcUtils.h"
#include "HwcComposer2D.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/StaticPtr.h"
#include "cutils/properties.h"

#define LOG_TAG "HWComposer"

#if (LOG_NDEBUG == 0)
#define LOGD(args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ## args)
#else
#define LOGD(args...) ((void)0)
#endif

#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ## args)

#define LAYER_COUNT_INCREMENTS 5

using namespace android;
using namespace mozilla::layers;

namespace mozilla {

static StaticRefPtr<HwcComposer2D> sInstance;

HwcComposer2D::HwcComposer2D()
    : mMaxLayerCount(0)
    , mList(nullptr)
    , mHwc(nullptr)
{
}

HwcComposer2D::~HwcComposer2D() {
    free(mList);
}

int
HwcComposer2D::Init(hwc_display_t dpy, hwc_surface_t sur)
{
    MOZ_ASSERT(!Initialized());

    mHwc = (hwc_composer_device_t*)GetGonkDisplay()->GetHWCDevice();
    if (!mHwc) {
        LOGE("Failed to initialize hwc");
        return -1;
    }

    nsIntSize screenSize;

    mozilla::Framebuffer::GetSize(&screenSize);
    mScreenRect  = nsIntRect(nsIntPoint(0, 0), screenSize);

    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.display.colorfill", propValue, "0");
    mColorFill = (atoi(propValue) == 1) ? true : false;

    mDpy = dpy;
    mSur = sur;

    return 0;
}

HwcComposer2D*
HwcComposer2D::GetInstance()
{
    if (!sInstance) {
        LOGD("Creating new instance");
        sInstance = new HwcComposer2D();
    }
    return sInstance;
}

bool
HwcComposer2D::ReallocLayerList()
{
    int size = sizeof(hwc_layer_list_t) +
        ((mMaxLayerCount + LAYER_COUNT_INCREMENTS) * sizeof(hwc_layer_t));

    hwc_layer_list_t* listrealloc = (hwc_layer_list_t*)realloc(mList, size);

    if (!listrealloc) {
        return false;
    }

    if (!mList) {
        //first alloc, initialize
        listrealloc->numHwLayers = 0;
        listrealloc->flags = 0;
    }

    mList = listrealloc;
    mMaxLayerCount += LAYER_COUNT_INCREMENTS;
    return true;
}

bool
HwcComposer2D::PrepareLayerList(Layer* aLayer,
                                const nsIntRect& aClip,
                                const gfxMatrix& aParentTransform,
                                const gfxMatrix& aGLWorldTransform)
{
    // NB: we fall off this path whenever there are container layers
    // that require intermediate surfaces.  That means all the
    // GetEffective*() coordinates are relative to the framebuffer.

    bool fillColor = false;

    const nsIntRegion& visibleRegion = aLayer->GetEffectiveVisibleRegion();
    if (visibleRegion.IsEmpty()) {
        return true;
    }

    float opacity = aLayer->GetEffectiveOpacity();
    if (opacity < 1) {
        LOGD("%s Layer has planar semitransparency which is unsupported", aLayer->Name());
        return false;
    }

    nsIntRect clip;
    if (!HwcUtils::CalculateClipRect(aParentTransform * aGLWorldTransform,
                                     aLayer->GetEffectiveClipRect(),
                                     aClip,
                                     &clip))
    {
        LOGD("%s Clip rect is empty. Skip layer", aLayer->Name());
        return true;
    }

    // HWC supports only the following 2D transformations:
    //
    // Scaling via the sourceCrop and displayFrame in hwc_layer_t
    // Translation via the sourceCrop and displayFrame in hwc_layer_t
    // Rotation (in square angles only) via the HWC_TRANSFORM_ROT_* flags
    // Reflection (horizontal and vertical) via the HWC_TRANSFORM_FLIP_* flags
    //
    // A 2D transform with PreservesAxisAlignedRectangles() has all the attributes
    // above
    gfxMatrix transform;
    const gfx3DMatrix& transform3D = aLayer->GetEffectiveTransform();
    if (!transform3D.Is2D(&transform) || !transform.PreservesAxisAlignedRectangles()) {
        LOGD("Layer has a 3D transform or a non-square angle rotation");
        return false;
    }


    if (ContainerLayer* container = aLayer->AsContainerLayer()) {
        if (container->UseIntermediateSurface()) {
            LOGD("Container layer needs intermediate surface");
            return false;
        }
        nsAutoTArray<Layer*, 12> children;
        container->SortChildrenBy3DZOrder(children);

        for (uint32_t i = 0; i < children.Length(); i++) {
            if (!PrepareLayerList(children[i], clip, transform, aGLWorldTransform)) {
                return false;
            }
        }
        return true;
    }

    LayerRenderState state = aLayer->GetRenderState();
    nsIntSize surfaceSize;

    if (state.mSurface.get()) {
        surfaceSize = state.mSize;
    } else {
        if (aLayer->AsColorLayer() && mColorFill) {
            fillColor = true;
        } else {
            LOGD("%s Layer doesn't have a gralloc buffer", aLayer->Name());
            return false;
        }
    }
    // Buffer rotation is not to be confused with the angled rotation done by a transform matrix
    // It's a fancy ThebesLayer feature used for scrolling
    if (state.BufferRotated()) {
        LOGD("%s Layer has a rotated buffer", aLayer->Name());
        return false;
    }


    // OK!  We can compose this layer with hwc.

    int current = mList ? mList->numHwLayers : 0;
    if (!mList || current >= mMaxLayerCount) {
        if (!ReallocLayerList() || current >= mMaxLayerCount) {
            LOGE("PrepareLayerList failed! Could not increase the maximum layer count");
            return false;
        }
    }

    nsIntRect visibleRect = visibleRegion.GetBounds();

    nsIntRect bufferRect;
    if (fillColor) {
        bufferRect = nsIntRect(visibleRect);
    } else {
        if(state.mHasOwnOffset) {
            bufferRect = nsIntRect(state.mOffset.x, state.mOffset.y,
                                   state.mSize.width, state.mSize.height);
        } else {
            //Since the buffer doesn't have its own offset, assign the whole
            //surface size as its buffer bounds
            bufferRect = nsIntRect(0, 0, state.mSize.width, state.mSize.height);
        }
    }

    hwc_layer_t& hwcLayer = mList->hwLayers[current];

    if(!HwcUtils::PrepareLayerRects(visibleRect,
                          transform * aGLWorldTransform,
                          clip,
                          bufferRect,
                          &(hwcLayer.sourceCrop),
                          &(hwcLayer.displayFrame)))
    {
        return true;
    }

    buffer_handle_t handle = fillColor ? nullptr : state.mSurface->getNativeBuffer()->handle;
    hwcLayer.handle = handle;

    hwcLayer.flags = 0;
    hwcLayer.hints = 0;
    hwcLayer.blending = HWC_BLENDING_PREMULT;
    hwcLayer.compositionType = HwcUtils::HWC_USE_COPYBIT;

    if (!fillColor) {
        if (state.FormatRBSwapped()) {
            hwcLayer.flags |= HwcUtils::HWC_FORMAT_RB_SWAP;
        }

        // Translation and scaling have been addressed in PrepareLayerRects().
        // Given the above and that we checked for PreservesAxisAlignedRectangles()
        // the only possible transformations left to address are
        // square angle rotation and horizontal/vertical reflection.
        //
        // The rotation and reflection permutations total 16 but can be
        // reduced to 8 transformations after eliminating redundancies.
        //
        // All matrices represented here are in the form
        //
        // | xx  xy |
        // | yx  yy |
        //
        // And ignore scaling.
        //
        // Reflection is applied before rotation
        gfxMatrix rotation = transform * aGLWorldTransform;
        // Compute fuzzy zero like PreservesAxisAlignedRectangles()
        if (fabs(rotation.xx) < 1e-6) {
            if (rotation.xy < 0) {
                if (rotation.yx > 0) {
                    // 90 degree rotation
                    //
                    // |  0  -1  |
                    // |  1   0  |
                    //
                    hwcLayer.transform = HWC_TRANSFORM_ROT_90;
                    LOGD("Layer rotated 90 degrees");
                }
                else {
                    // Horizontal reflection then 90 degree rotation
                    //
                    // |  0  -1  | | -1   0  | = |  0  -1  |
                    // |  1   0  | |  0   1  |   | -1   0  |
                    //
                    // same as vertical reflection then 270 degree rotation
                    //
                    // |  0   1  | |  1   0  | = |  0  -1  |
                    // | -1   0  | |  0  -1  |   | -1   0  |
                    //
                    hwcLayer.transform = HWC_TRANSFORM_ROT_90 | HWC_TRANSFORM_FLIP_H;
                    LOGD("Layer vertically reflected then rotated 270 degrees");
                }
            } else {
                if (rotation.yx < 0) {
                    // 270 degree rotation
                    //
                    // |  0   1  |
                    // | -1   0  |
                    //
                    hwcLayer.transform = HWC_TRANSFORM_ROT_270;
                    LOGD("Layer rotated 270 degrees");
                }
                else {
                    // Vertical reflection then 90 degree rotation
                    //
                    // |  0   1  | | -1   0  | = |  0   1  |
                    // | -1   0  | |  0   1  |   |  1   0  |
                    //
                    // Same as horizontal reflection then 270 degree rotation
                    //
                    // |  0  -1  | |  1   0  | = |  0   1  |
                    // |  1   0  | |  0  -1  |   |  1   0  |
                    //
                    hwcLayer.transform = HWC_TRANSFORM_ROT_90 | HWC_TRANSFORM_FLIP_V;
                    LOGD("Layer horizontally reflected then rotated 270 degrees");
                }
            }
        } else if (rotation.xx < 0) {
            if (rotation.yy > 0) {
                // Horizontal reflection
                //
                // | -1   0  |
                // |  0   1  |
                //
                hwcLayer.transform = HWC_TRANSFORM_FLIP_H;
                LOGD("Layer rotated 180 degrees");
            }
            else {
                // 180 degree rotation
                //
                // | -1   0  |
                // |  0  -1  |
                //
                // Same as horizontal and vertical reflection
                //
                // | -1   0  | |  1   0  | = | -1   0  |
                // |  0   1  | |  0  -1  |   |  0  -1  |
                //
                hwcLayer.transform = HWC_TRANSFORM_ROT_180;
                LOGD("Layer rotated 180 degrees");
            }
        } else {
            if (rotation.yy < 0) {
                // Vertical reflection
                //
                // |  1   0  |
                // |  0  -1  |
                //
                hwcLayer.transform = HWC_TRANSFORM_FLIP_V;
                LOGD("Layer rotated 180 degrees");
            }
            else {
                // No rotation or reflection
                //
                // |  1   0  |
                // |  0   1  |
                //
                hwcLayer.transform = 0;
            }
        }

        if (state.YFlipped()) {
           // Invert vertical reflection flag if it was already set
           hwcLayer.transform ^= HWC_TRANSFORM_FLIP_V;
        }
        hwc_region_t region;
        if (visibleRegion.GetNumRects() > 1) {
            mVisibleRegions.push_back(HwcUtils::RectVector());
            HwcUtils::RectVector* visibleRects = &(mVisibleRegions.back());
            if(!HwcUtils::PrepareVisibleRegion(visibleRegion,
                                     transform * aGLWorldTransform,
                                     clip,
                                     bufferRect,
                                     visibleRects)) {
                return true;
            }
            region.numRects = visibleRects->size();
            region.rects = &((*visibleRects)[0]);
        } else {
            region.numRects = 1;
            region.rects = &(hwcLayer.displayFrame);
        }
        hwcLayer.visibleRegionScreen = region;
    } else {
        hwcLayer.flags |= HwcUtils::HWC_COLOR_FILL;
        ColorLayer* colorLayer = aLayer->AsColorLayer();
        if (colorLayer->GetColor().a < 1.0) {
            LOGD("Color layer has semitransparency which is unsupported");
            return false;
        }
        hwcLayer.transform = colorLayer->GetColor().Packed();
    }

    mList->numHwLayers++;
    return true;
}

bool
HwcComposer2D::TryRender(Layer* aRoot,
                         const gfxMatrix& aGLWorldTransform)
{
    if (!aGLWorldTransform.PreservesAxisAlignedRectangles()) {
        LOGD("Render aborted. World transform has non-square angle rotation");
        return false;
    }

    MOZ_ASSERT(Initialized());
    if (mList) {
        mList->numHwLayers = 0;
    }

    // XXX: The clear() below means all rect vectors will be have to be
    // reallocated. We may want to avoid this if possible
    mVisibleRegions.clear();

    if (!PrepareLayerList(aRoot,
                          mScreenRect,
                          gfxMatrix(),
                          aGLWorldTransform))
    {
        LOGD("Render aborted. Nothing was drawn to the screen");
        return false;
    }

    if (mHwc->set(mHwc, mDpy, mSur, mList)) {
        LOGE("Hardware device failed to render");
        return false;
    }

    LOGD("Frame rendered");
    return true;
}

} // namespace mozilla
