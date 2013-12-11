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

#if ANDROID_VERSION >= 18
#include "libdisplay/FramebufferSurface.h"
#endif

#define LOG_TAG "HWComposer"

/*
 * By default the debug message of hwcomposer (LOG_DEBUG level) are undefined,
 * but can be enabled by uncommenting HWC_DEBUG below.
 */
//#define HWC_DEBUG

#ifdef HWC_DEBUG
#define LOGD(args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, ## args)
#else
#define LOGD(args...) ((void)0)
#endif

#define LOGI(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, ## args)
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
    , mColorFill(false)
    , mRBSwapSupport(false)
    , mPrevRetireFence(-1)
{
}

HwcComposer2D::~HwcComposer2D() {
    free(mList);
}

int
HwcComposer2D::Init(hwc_display_t dpy, hwc_surface_t sur)
{
    MOZ_ASSERT(!Initialized());

    mHwc = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();
    if (!mHwc) {
        LOGE("Failed to initialize hwc");
        return -1;
    }

    nsIntSize screenSize;

    mozilla::Framebuffer::GetSize(&screenSize);
    mScreenRect  = nsIntRect(nsIntPoint(0, 0), screenSize);

#if ANDROID_VERSION >= 18
    int supported = 0;
    if (mHwc->query(mHwc, HwcUtils::HWC_COLOR_FILL, &supported) == NO_ERROR) {
        mColorFill = supported ? true : false;
    }
    if (mHwc->query(mHwc, HwcUtils::HWC_FORMAT_RB_SWAP, &supported) == NO_ERROR) {
        mRBSwapSupport = supported ? true : false;
    }
#else
    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.display.colorfill", propValue, "0");
    mColorFill = (atoi(propValue) == 1) ? true : false;
    mRBSwapSupport = true;
#endif

    mDpy = dpy;
    mSur = sur;

    return 0;
}

HwcComposer2D*
HwcComposer2D::GetInstance()
{
    if (!sInstance) {
        LOGI("Creating new instance");
        sInstance = new HwcComposer2D();
    }
    return sInstance;
}

bool
HwcComposer2D::ReallocLayerList()
{
    int size = sizeof(HwcList) +
        ((mMaxLayerCount + LAYER_COUNT_INCREMENTS) * sizeof(HwcLayer));

    HwcList* listrealloc = (HwcList*)realloc(mList, size);

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

    uint8_t opacity = std::min(0xFF, (int)(aLayer->GetEffectiveOpacity() * 256.0));
#if ANDROID_VERSION < 18
    if (opacity < 0xFF) {
        LOGD("%s Layer has planar semitransparency which is unsupported", aLayer->Name());
        return false;
    }
#endif

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
    // Scaling via the sourceCrop and displayFrame in HwcLayer
    // Translation via the sourceCrop and displayFrame in HwcLayer
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

    HwcLayer& hwcLayer = mList->hwLayers[current];

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
#if ANDROID_VERSION >= 18
    hwcLayer.compositionType = HWC_FRAMEBUFFER;

    hwcLayer.acquireFenceFd = -1;
    hwcLayer.releaseFenceFd = -1;
    hwcLayer.planeAlpha = opacity;
#else
    hwcLayer.compositionType = HwcUtils::HWC_USE_COPYBIT;
#endif

    if (!fillColor) {
        if (state.FormatRBSwapped()) {
            if (!mRBSwapSupport) {
                LOGD("No R/B swap support in H/W Composer");
                return false;
            }
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

    mHwcLayerMap.AppendElement(static_cast<LayerComposite*>(aLayer->ImplData()));
    mList->numHwLayers++;
    return true;
}


#if ANDROID_VERSION >= 18
bool
HwcComposer2D::TryHwComposition()
{
    FramebufferSurface* fbsurface = (FramebufferSurface*)(GetGonkDisplay()->GetFBSurface());

    if (!(fbsurface && fbsurface->lastHandle)) {
        LOGD("H/W Composition failed. FBSurface not initialized.");
        mList->numHwLayers = 0;
        return false;
    }

    // Add FB layer
    int idx = mList->numHwLayers++;
    if (idx >= mMaxLayerCount) {
        if (!ReallocLayerList() || idx >= mMaxLayerCount) {
            LOGE("TryHwComposition failed! Could not add FB layer");
            mList->numHwLayers = 0;
            return false;
        }
    }

    Prepare(fbsurface->lastHandle, -1);

    bool fullHwcComposite = true;
    for (int j = 0; j < idx; j++) {
        if (mList->hwLayers[j].compositionType == HWC_FRAMEBUFFER) {
            // After prepare, if there is an HWC_FRAMEBUFFER layer,
            // it means full HWC Composition is not possible this time
            LOGD("GPU or Partial HWC Composition");
            fullHwcComposite = false;
            break;
        }
    }

    if (!fullHwcComposite) {
        for (int k=0; k < idx; k++) {
            if (mList->hwLayers[k].compositionType == HWC_OVERLAY) {
                // HWC will compose HWC_OVERLAY layers in partial
                // HWC Composition, so set layer composition flag
                // on mapped LayerComposite to skip GPU composition
                mHwcLayerMap[k]->SetLayerComposited(true);
            }
        }
        return false;
    }

    // Full HWC Composition
    Commit();

    // No composition on FB layer, so closing releaseFenceFd
    close(mList->hwLayers[idx].releaseFenceFd);
    mList->numHwLayers = 0;
    return true;
}

bool
HwcComposer2D::Render(EGLDisplay dpy, EGLSurface sur)
{
    if (!mList) {
        // After boot, HWC list hasn't been created yet
        return GetGonkDisplay()->SwapBuffers(dpy, sur);
    }

    GetGonkDisplay()->UpdateFBSurface(dpy, sur);

    FramebufferSurface* fbsurface = (FramebufferSurface*)(GetGonkDisplay()->GetFBSurface());
    if (!fbsurface) {
        LOGE("H/W Composition failed. FBSurface not initialized.");
        return false;
    }

    if (mList->numHwLayers != 0) {
        // No mHwc prepare, if already prepared in current draw cycle
        mList->hwLayers[mList->numHwLayers - 1].handle = fbsurface->lastHandle;
        mList->hwLayers[mList->numHwLayers - 1].acquireFenceFd = fbsurface->lastFenceFD;
    } else {
        mList->numHwLayers = 2;
        mList->hwLayers[0].hints = 0;
        mList->hwLayers[0].compositionType = HWC_BACKGROUND;
        mList->hwLayers[0].flags = HWC_SKIP_LAYER;
        mList->hwLayers[0].backgroundColor = {0};
        mList->hwLayers[0].acquireFenceFd = -1;
        mList->hwLayers[0].releaseFenceFd = -1;
        mList->hwLayers[0].displayFrame = {0, 0, mScreenRect.width, mScreenRect.height};
        Prepare(fbsurface->lastHandle, fbsurface->lastFenceFD);
    }

    // GPU or partial HWC Composition
    Commit();

    GetGonkDisplay()->SetFBReleaseFd(mList->hwLayers[mList->numHwLayers - 1].releaseFenceFd);
    mList->numHwLayers = 0;
    return true;
}

void
HwcComposer2D::Prepare(buffer_handle_t fbHandle, int fence)
{
    int idx = mList->numHwLayers - 1;
    const hwc_rect_t r = {0, 0, mScreenRect.width, mScreenRect.height};
    hwc_display_contents_1_t *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };

    displays[HWC_DISPLAY_PRIMARY] = mList;
    mList->flags = HWC_GEOMETRY_CHANGED;
    mList->outbufAcquireFenceFd = -1;
    mList->outbuf = nullptr;
    mList->retireFenceFd = -1;

    mList->hwLayers[idx].hints = 0;
    mList->hwLayers[idx].flags = 0;
    mList->hwLayers[idx].transform = 0;
    mList->hwLayers[idx].handle = fbHandle;
    mList->hwLayers[idx].blending = HWC_BLENDING_PREMULT;
    mList->hwLayers[idx].compositionType = HWC_FRAMEBUFFER_TARGET;
    mList->hwLayers[idx].sourceCrop = r;
    mList->hwLayers[idx].displayFrame = r;
    mList->hwLayers[idx].visibleRegionScreen.numRects = 1;
    mList->hwLayers[idx].visibleRegionScreen.rects = &mList->hwLayers[idx].sourceCrop;
    mList->hwLayers[idx].acquireFenceFd = fence;
    mList->hwLayers[idx].releaseFenceFd = -1;
    mList->hwLayers[idx].planeAlpha = 0xFF;

    mHwc->prepare(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
}

bool
HwcComposer2D::Commit()
{
    hwc_display_contents_1_t *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };
    displays[HWC_DISPLAY_PRIMARY] = mList;

    int err = mHwc->set(mHwc, HWC_NUM_DISPLAY_TYPES, displays);

    // To avoid tearing, workaround for missing releaseFenceFd
    // waits in Gecko layers, see Bug 925444.
    if (!mPrevReleaseFds.IsEmpty()) {
        // Wait for previous retire Fence to signal.
        // Denotes contents on display have been replaced.
        // For buffer-sync, framework should not over-write
        // prev buffers until we close prev releaseFenceFds
        sp<Fence> fence = new Fence(mPrevRetireFence);
        if (fence->wait(1000) == -ETIME) {
            LOGE("Wait timed-out for retireFenceFd %d", mPrevRetireFence);
        }
        for (int i = 0; i < mPrevReleaseFds.Length(); i++) {
            close(mPrevReleaseFds[i]);
        }
        close(mPrevRetireFence);
        mPrevReleaseFds.Clear();
    }

    for (uint32_t j=0; j < (mList->numHwLayers - 1); j++) {
        if (mList->hwLayers[j].releaseFenceFd >= 0) {
            mPrevReleaseFds.AppendElement(mList->hwLayers[j].releaseFenceFd);
        }
    }

    if (mList->retireFenceFd >= 0) {
        if (!mPrevReleaseFds.IsEmpty()) {
            mPrevRetireFence = mList->retireFenceFd;
        } else { // GPU Composition
            close(mList->retireFenceFd);
        }
    }

    return !err;
}
#else
bool
HwcComposer2D::TryHwComposition()
{
    return !mHwc->set(mHwc, mDpy, mSur, mList);
}

bool
HwcComposer2D::Render(EGLDisplay dpy, EGLSurface sur)
{
    return GetGonkDisplay()->SwapBuffers(dpy, sur);
}
#endif

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
        mHwcLayerMap.Clear();
    }

    // XXX: The clear() below means all rect vectors will be have to be
    // reallocated. We may want to avoid this if possible
    mVisibleRegions.clear();

    MOZ_ASSERT(mHwcLayerMap.IsEmpty());
    if (!PrepareLayerList(aRoot,
                          mScreenRect,
                          gfxMatrix(),
                          aGLWorldTransform))
    {
        LOGD("Render aborted. Nothing was drawn to the screen");
        if (mList) {
           mList->numHwLayers = 0;
        }
        return false;
    }

    if (!TryHwComposition()) {
        LOGD("H/W Composition failed");
        return false;
    }

    LOGD("Frame rendered");
    return true;
}

} // namespace mozilla
