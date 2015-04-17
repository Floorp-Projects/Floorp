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

#include "ImageLayers.h"
#include "libdisplay/GonkDisplay.h"
#include "HwcComposer2D.h"
#include "HwcUtils.h"
#include "LayerScope.h"
#include "Units.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/StaticPtr.h"
#include "cutils/properties.h"
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "VsyncSource.h"

#if ANDROID_VERSION >= 17
#include "libdisplay/FramebufferSurface.h"
#include "gfxPrefs.h"
#include "nsThreadUtils.h"
#endif

#if ANDROID_VERSION >= 21
#ifndef HWC_BLIT
#define HWC_BLIT 0xFF
#endif
#elif ANDROID_VERSION >= 17
#ifndef HWC_BLIT
#define HWC_BLIT (HWC_FRAMEBUFFER_TARGET + 1)
#endif
#endif

#ifdef LOG_TAG
#undef LOG_TAG
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
using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {

#if ANDROID_VERSION >= 17
static void
HookInvalidate(const struct hwc_procs* aProcs)
{
    HwcComposer2D::GetInstance()->Invalidate();
}

static void
HookVsync(const struct hwc_procs* aProcs, int aDisplay,
          int64_t aTimestamp)
{
    HwcComposer2D::GetInstance()->Vsync(aDisplay, aTimestamp);
}

static void
HookHotplug(const struct hwc_procs* aProcs, int aDisplay,
            int aConnected)
{
    // no op
}

static const hwc_procs_t sHWCProcs = {
    &HookInvalidate, // 1st: void (*invalidate)(...)
    &HookVsync,      // 2nd: void (*vsync)(...)
    &HookHotplug     // 3rd: void (*hotplug)(...)
};
#endif

static StaticRefPtr<HwcComposer2D> sInstance;

HwcComposer2D::HwcComposer2D()
    : mHwc(nullptr)
    , mList(nullptr)
    , mGLContext(nullptr)
    , mMaxLayerCount(0)
    , mColorFill(false)
    , mRBSwapSupport(false)
#if ANDROID_VERSION >= 17
    , mPrevRetireFence(Fence::NO_FENCE)
    , mPrevDisplayFence(Fence::NO_FENCE)
#endif
    , mPrepared(false)
    , mHasHWVsync(false)
    , mLock("mozilla.HwcComposer2D.mLock")
{
#if ANDROID_VERSION >= 17
    RegisterHwcEventCallback();
#endif
}

HwcComposer2D::~HwcComposer2D() {
    free(mList);
}

int
HwcComposer2D::Init(hwc_display_t dpy, hwc_surface_t sur, gl::GLContext* aGLContext)
{
    MOZ_ASSERT(!Initialized());

    mHwc = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();
    if (!mHwc) {
        LOGE("Failed to initialize hwc");
        return -1;
    }

    nsIntSize screenSize;

    ANativeWindow *win = GetGonkDisplay()->GetNativeWindow();
    win->query(win, NATIVE_WINDOW_WIDTH, &screenSize.width);
    win->query(win, NATIVE_WINDOW_HEIGHT, &screenSize.height);
    mScreenRect = nsIntRect(nsIntPoint(0, 0), screenSize);

#if ANDROID_VERSION >= 17
    int supported = 0;

    if (mHwc->query) {
        if (mHwc->query(mHwc, HwcUtils::HWC_COLOR_FILL, &supported) == NO_ERROR) {
            mColorFill = !!supported;
        }
        if (mHwc->query(mHwc, HwcUtils::HWC_FORMAT_RB_SWAP, &supported) == NO_ERROR) {
            mRBSwapSupport = !!supported;
        }
    } else {
        mColorFill = false;
        mRBSwapSupport = false;
    }
#else
    char propValue[PROPERTY_VALUE_MAX];
    property_get("ro.display.colorfill", propValue, "0");
    mColorFill = (atoi(propValue) == 1) ? true : false;
    mRBSwapSupport = true;
#endif

    mDpy = dpy;
    mSur = sur;
    mGLContext = aGLContext;

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
HwcComposer2D::EnableVsync(bool aEnable)
{
#if ANDROID_VERSION >= 17
    MOZ_ASSERT(NS_IsMainThread());
    if (!mHasHWVsync) {
      return false;
    }

    HwcDevice* device = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();
    if (!device) {
      return false;
    }

    return !device->eventControl(device, HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, aEnable) && aEnable;
#else
    return false;
#endif
}

#if ANDROID_VERSION >= 17
bool
HwcComposer2D::RegisterHwcEventCallback()
{
    HwcDevice* device = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();
    if (!device || !device->registerProcs) {
        LOGE("Failed to get hwc");
        return false;
    }

    // Disable Vsync first, and then register callback functions.
    device->eventControl(device, HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, false);
    device->registerProcs(device, &sHWCProcs);

// Only support actual hardware vsync on kitkat due to innaccurate timings
// with JellyBean, and HwcComposer bugs with L. Reenable for L later
#if ANDROID_VERSION == 19
    mHasHWVsync = gfxPrefs::HardwareVsyncEnabled();
#else
    mHasHWVsync = false;
#endif
    return mHasHWVsync;
}

void
HwcComposer2D::Vsync(int aDisplay, nsecs_t aVsyncTimestamp)
{
    TimeStamp vsyncTime = mozilla::TimeStamp::FromSystemTime(aVsyncTimestamp);
    gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay().NotifyVsync(vsyncTime);
}

// Called on the "invalidator" thread (run from HAL).
void
HwcComposer2D::Invalidate()
{
    if (!Initialized()) {
        LOGE("HwcComposer2D::Invalidate failed!");
        return;
    }

    MutexAutoLock lock(mLock);
    if (mCompositorParent) {
        mCompositorParent->ScheduleRenderOnCompositorThread();
    }
}
#endif

void
HwcComposer2D::SetCompositorParent(CompositorParent* aCompositorParent)
{
    MutexAutoLock lock(mLock);
    mCompositorParent = aCompositorParent;
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

void
HwcComposer2D::setCrop(HwcLayer* layer, hwc_rect_t srcCrop)
{
#if ANDROID_VERSION >= 19
    if (mHwc->common.version >= HWC_DEVICE_API_VERSION_1_3) {
        layer->sourceCropf.left = srcCrop.left;
        layer->sourceCropf.top = srcCrop.top;
        layer->sourceCropf.right = srcCrop.right;
        layer->sourceCropf.bottom = srcCrop.bottom;
    } else {
        layer->sourceCrop = srcCrop;
    }
#else
    layer->sourceCrop = srcCrop;
#endif
}

void
HwcComposer2D::setHwcGeometry(bool aGeometryChanged)
{
#if ANDROID_VERSION >= 19
    mList->flags = aGeometryChanged ? HWC_GEOMETRY_CHANGED : 0;
#else
    mList->flags = HWC_GEOMETRY_CHANGED;
#endif
}

bool
HwcComposer2D::PrepareLayerList(Layer* aLayer,
                                const nsIntRect& aClip,
                                const Matrix& aParentTransform)
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
    if (opacity == 0) {
        LOGD("%s Layer has zero opacity; skipping", aLayer->Name());
        return true;
    }
#if ANDROID_VERSION < 18
    if (opacity < 0xFF) {
        LOGD("%s Layer has planar semitransparency which is unsupported by hwcomposer", aLayer->Name());
        return false;
    }
#endif

    if (aLayer->GetMaskLayer()) {
      LOGD("%s Layer has MaskLayer which is unsupported by hwcomposer", aLayer->Name());
      return false;
    }

    nsIntRect clip;
    nsIntRect layerClip = aLayer->GetEffectiveClipRect() ?
                          ParentLayerIntRect::ToUntyped(*aLayer->GetEffectiveClipRect()) : nsIntRect();
    nsIntRect* layerClipPtr = aLayer->GetEffectiveClipRect() ? &layerClip : nullptr;
    if (!HwcUtils::CalculateClipRect(aParentTransform,
                                     layerClipPtr,
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
    Matrix layerTransform;
    if (!aLayer->GetEffectiveTransform().Is2D(&layerTransform) ||
        !layerTransform.PreservesAxisAlignedRectangles()) {
        LOGD("Layer EffectiveTransform has a 3D transform or a non-square angle rotation");
        return false;
    }

    Matrix layerBufferTransform;
    if (!aLayer->GetEffectiveTransformForBuffer().Is2D(&layerBufferTransform) ||
        !layerBufferTransform.PreservesAxisAlignedRectangles()) {
        LOGD("Layer EffectiveTransformForBuffer has a 3D transform or a non-square angle rotation");
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
            if (!PrepareLayerList(children[i], clip, layerTransform)) {
                return false;
            }
        }
        return true;
    }

    LayerRenderState state = aLayer->GetRenderState();

    if (!state.mSurface.get()) {
      if (aLayer->AsColorLayer() && mColorFill) {
        fillColor = true;
      } else {
          LOGD("%s Layer doesn't have a gralloc buffer", aLayer->Name());
          return false;
      }
    }

    nsIntRect visibleRect = visibleRegion.GetBounds();

    nsIntRect bufferRect;
    if (fillColor) {
        bufferRect = nsIntRect(visibleRect);
    } else {
        nsIntRect layerRect;
        if (state.mHasOwnOffset) {
            bufferRect = nsIntRect(state.mOffset.x, state.mOffset.y,
                                   state.mSize.width, state.mSize.height);
            layerRect = bufferRect;
        } else {
            //Since the buffer doesn't have its own offset, assign the whole
            //surface size as its buffer bounds
            bufferRect = nsIntRect(0, 0, state.mSize.width, state.mSize.height);
            layerRect = bufferRect;
            if (aLayer->GetType() == Layer::TYPE_IMAGE) {
                ImageLayer* imageLayer = static_cast<ImageLayer*>(aLayer);
                if(imageLayer->GetScaleMode() != ScaleMode::SCALE_NONE) {
                  layerRect = nsIntRect(0, 0, imageLayer->GetScaleToSize().width, imageLayer->GetScaleToSize().height);
                }
            }
        }
        // In some cases the visible rect assigned to the layer can be larger
        // than the layer's surface, e.g., an ImageLayer with a small Image
        // in it.
        visibleRect.IntersectRect(visibleRect, layerRect);
    }

    // Buffer rotation is not to be confused with the angled rotation done by a transform matrix
    // It's a fancy PaintedLayer feature used for scrolling
    if (state.BufferRotated()) {
        LOGD("%s Layer has a rotated buffer", aLayer->Name());
        return false;
    }

    const bool needsYFlip = state.OriginBottomLeft() ? true
                                                     : false;

    hwc_rect_t sourceCrop, displayFrame;
    if(!HwcUtils::PrepareLayerRects(visibleRect,
                          layerTransform,
                          layerBufferTransform,
                          clip,
                          bufferRect,
                          needsYFlip,
                          &(sourceCrop),
                          &(displayFrame)))
    {
        return true;
    }

    // OK!  We can compose this layer with hwc.
    int current = mList ? mList->numHwLayers : 0;

    // Do not compose any layer below full-screen Opaque layer
    // Note: It can be generalized to non-fullscreen Opaque layers.
    bool isOpaque = opacity == 0xFF &&
        (state.mFlags & LayerRenderStateFlags::OPAQUE);
    // Currently we perform opacity calculation using the *bounds* of the layer.
    // We can only make this assumption if we're not dealing with a complex visible region.
    bool isSimpleVisibleRegion = visibleRegion.Contains(visibleRect);
    if (current && isOpaque && isSimpleVisibleRegion) {
        nsIntRect displayRect = nsIntRect(displayFrame.left, displayFrame.top,
            displayFrame.right - displayFrame.left, displayFrame.bottom - displayFrame.top);
        if (displayRect.Contains(mScreenRect)) {
            // In z-order, all previous layers are below
            // the current layer. We can ignore them now.
            mList->numHwLayers = current = 0;
            mHwcLayerMap.Clear();
        }
    }

    if (!mList || current >= mMaxLayerCount) {
        if (!ReallocLayerList() || current >= mMaxLayerCount) {
            LOGE("PrepareLayerList failed! Could not increase the maximum layer count");
            return false;
        }
    }

    HwcLayer& hwcLayer = mList->hwLayers[current];
    hwcLayer.displayFrame = displayFrame;
    setCrop(&hwcLayer, sourceCrop);
    buffer_handle_t handle = fillColor ? nullptr : state.mSurface->getNativeBuffer()->handle;
    hwcLayer.handle = handle;

    hwcLayer.flags = 0;
    hwcLayer.hints = 0;
    hwcLayer.blending = isOpaque ? HWC_BLENDING_NONE : HWC_BLENDING_PREMULT;
#if ANDROID_VERSION >= 17
    hwcLayer.compositionType = HWC_FRAMEBUFFER;

    hwcLayer.acquireFenceFd = -1;
    hwcLayer.releaseFenceFd = -1;
#if ANDROID_VERSION >= 18
    hwcLayer.planeAlpha = opacity;
#endif
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
        gfx::Matrix rotation = layerTransform;
        // Compute fuzzy zero like PreservesAxisAlignedRectangles()
        if (fabs(rotation._11) < 1e-6) {
            if (rotation._21 < 0) {
                if (rotation._12 > 0) {
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
                if (rotation._12 < 0) {
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
        } else if (rotation._11 < 0) {
            if (rotation._22 > 0) {
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
            if (rotation._22 < 0) {
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

        const bool needsYFlip = state.OriginBottomLeft() ? true
                                                         : false;

        if (needsYFlip) {
           // Invert vertical reflection flag if it was already set
           hwcLayer.transform ^= HWC_TRANSFORM_FLIP_V;
        }
        hwc_region_t region;
        if (visibleRegion.GetNumRects() > 1) {
            mVisibleRegions.push_back(HwcUtils::RectVector());
            HwcUtils::RectVector* visibleRects = &(mVisibleRegions.back());
            if(!HwcUtils::PrepareVisibleRegion(visibleRegion,
                                     layerTransform,
                                     layerBufferTransform,
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


#if ANDROID_VERSION >= 17
bool
HwcComposer2D::TryHwComposition()
{
    FramebufferSurface* fbsurface = (FramebufferSurface*)(GetGonkDisplay()->GetFBSurface());

    if (!(fbsurface && fbsurface->lastHandle)) {
        LOGD("H/W Composition failed. FBSurface not initialized.");
        return false;
    }

    // Add FB layer
    int idx = mList->numHwLayers++;
    if (idx >= mMaxLayerCount) {
        if (!ReallocLayerList() || idx >= mMaxLayerCount) {
            LOGE("TryHwComposition failed! Could not add FB layer");
            return false;
        }
    }

    Prepare(fbsurface->lastHandle, -1);

    /* Possible composition paths, after hwc prepare:
    1. GPU Composition
    2. BLIT Composition
    3. Full OVERLAY Composition
    4. Partial OVERLAY Composition (GPU + OVERLAY) */

    bool gpuComposite = false;
    bool blitComposite = false;
    bool overlayComposite = true;

    for (int j=0; j < idx; j++) {
        if (mList->hwLayers[j].compositionType == HWC_FRAMEBUFFER ||
            mList->hwLayers[j].compositionType == HWC_BLIT) {
            // Full OVERLAY composition is not possible on this frame
            // It is either GPU / BLIT / partial OVERLAY composition.
            overlayComposite = false;
            break;
        }
    }

    if (!overlayComposite) {
        for (int k=0; k < idx; k++) {
            switch (mList->hwLayers[k].compositionType) {
                case HWC_FRAMEBUFFER:
                    gpuComposite = true;
                    break;
                case HWC_BLIT:
                    blitComposite = true;
                    break;
                case HWC_OVERLAY:
                    // HWC will compose HWC_OVERLAY layers in partial
                    // Overlay Composition, set layer composition flag
                    // on mapped LayerComposite to skip GPU composition
                    mHwcLayerMap[k]->SetLayerComposited(true);
                    if ((mList->hwLayers[k].hints & HWC_HINT_CLEAR_FB) &&
                        (mList->hwLayers[k].blending == HWC_BLENDING_NONE)) {
                        // Clear visible rect on FB with transparent pixels.
                        hwc_rect_t r = mList->hwLayers[k].displayFrame;
                        mHwcLayerMap[k]->SetClearRect(nsIntRect(r.left, r.top,
                                                                r.right - r.left,
                                                                r.bottom - r.top));
                    }
                    break;
                default:
                    break;
            }
        }

        if (gpuComposite) {
            // GPU or partial OVERLAY Composition
            return false;
        } else if (blitComposite) {
            // BLIT Composition, flip FB target
            GetGonkDisplay()->UpdateFBSurface(mDpy, mSur);
            FramebufferSurface* fbsurface = (FramebufferSurface*)(GetGonkDisplay()->GetFBSurface());
            if (!fbsurface) {
                LOGE("H/W Composition failed. NULL FBSurface.");
                return false;
            }
            mList->hwLayers[idx].handle = fbsurface->lastHandle;
            mList->hwLayers[idx].acquireFenceFd = fbsurface->GetPrevFBAcquireFd();
        }
    }

    // BLIT or full OVERLAY Composition
    Commit();

    GetGonkDisplay()->SetFBReleaseFd(mList->hwLayers[idx].releaseFenceFd);
    mList->hwLayers[idx].releaseFenceFd = -1;
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

    if (mPrepared) {
        // No mHwc prepare, if already prepared in current draw cycle
        mList->hwLayers[mList->numHwLayers - 1].handle = fbsurface->lastHandle;
        mList->hwLayers[mList->numHwLayers - 1].acquireFenceFd = fbsurface->GetPrevFBAcquireFd();
    } else {
        mList->flags = HWC_GEOMETRY_CHANGED;
        mList->numHwLayers = 2;
        mList->hwLayers[0].hints = 0;
        mList->hwLayers[0].compositionType = HWC_FRAMEBUFFER;
        mList->hwLayers[0].flags = HWC_SKIP_LAYER;
        mList->hwLayers[0].backgroundColor = {0};
        mList->hwLayers[0].acquireFenceFd = -1;
        mList->hwLayers[0].releaseFenceFd = -1;
        mList->hwLayers[0].displayFrame = {0, 0, mScreenRect.width, mScreenRect.height};
        Prepare(fbsurface->lastHandle, fbsurface->GetPrevFBAcquireFd());
    }

    // GPU or partial HWC Composition
    Commit();

    GetGonkDisplay()->SetFBReleaseFd(mList->hwLayers[mList->numHwLayers - 1].releaseFenceFd);
    mList->hwLayers[mList->numHwLayers - 1].releaseFenceFd = -1;
    return true;
}

void
HwcComposer2D::Prepare(buffer_handle_t fbHandle, int fence)
{
    int idx = mList->numHwLayers - 1;
    const hwc_rect_t r = {0, 0, mScreenRect.width, mScreenRect.height};
    hwc_display_contents_1_t *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };

    displays[HWC_DISPLAY_PRIMARY] = mList;
    mList->outbufAcquireFenceFd = -1;
    mList->outbuf = nullptr;
    mList->retireFenceFd = -1;

    mList->hwLayers[idx].hints = 0;
    mList->hwLayers[idx].flags = 0;
    mList->hwLayers[idx].transform = 0;
    mList->hwLayers[idx].handle = fbHandle;
    mList->hwLayers[idx].blending = HWC_BLENDING_PREMULT;
    mList->hwLayers[idx].compositionType = HWC_FRAMEBUFFER_TARGET;
    setCrop(&mList->hwLayers[idx], r);
    mList->hwLayers[idx].displayFrame = r;
    mList->hwLayers[idx].visibleRegionScreen.numRects = 1;
    mList->hwLayers[idx].visibleRegionScreen.rects = &mList->hwLayers[idx].displayFrame;
    mList->hwLayers[idx].acquireFenceFd = fence;
    mList->hwLayers[idx].releaseFenceFd = -1;
#if ANDROID_VERSION >= 18
    mList->hwLayers[idx].planeAlpha = 0xFF;
#endif
    if (mPrepared) {
        LOGE("Multiple hwc prepare calls!");
    }
    mHwc->prepare(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
    mPrepared = true;
}

bool
HwcComposer2D::Commit()
{
    hwc_display_contents_1_t *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };
    displays[HWC_DISPLAY_PRIMARY] = mList;

    for (uint32_t j=0; j < (mList->numHwLayers - 1); j++) {
        mList->hwLayers[j].acquireFenceFd = -1;
        if (mHwcLayerMap.IsEmpty() ||
            (mList->hwLayers[j].compositionType == HWC_FRAMEBUFFER)) {
            continue;
        }
        LayerRenderState state = mHwcLayerMap[j]->GetLayer()->GetRenderState();
        if (!state.mTexture) {
            continue;
        }
        TextureHostOGL* texture = state.mTexture->AsHostOGL();
        if (!texture) {
            continue;
        }
        sp<Fence> fence = texture->GetAndResetAcquireFence();
        if (fence.get() && fence->isValid()) {
            mList->hwLayers[j].acquireFenceFd = fence->dup();
        }
    }

    int err = mHwc->set(mHwc, HWC_NUM_DISPLAY_TYPES, displays);

    mPrevDisplayFence = mPrevRetireFence;
    mPrevRetireFence = Fence::NO_FENCE;

    for (uint32_t j=0; j < (mList->numHwLayers - 1); j++) {
        if (mList->hwLayers[j].releaseFenceFd >= 0) {
            int fd = mList->hwLayers[j].releaseFenceFd;
            mList->hwLayers[j].releaseFenceFd = -1;
            sp<Fence> fence = new Fence(fd);

            LayerRenderState state = mHwcLayerMap[j]->GetLayer()->GetRenderState();
            if (!state.mTexture) {
                continue;
            }
            TextureHostOGL* texture = state.mTexture->AsHostOGL();
            if (!texture) {
                continue;
            }
            texture->SetReleaseFence(fence);
       }
   }

    if (mList->retireFenceFd >= 0) {
        mPrevRetireFence = new Fence(mList->retireFenceFd);
    }

    mPrepared = false;
    return !err;
}

void
HwcComposer2D::Reset()
{
    LOGD("hwcomposer is already prepared, reset with null set");
    hwc_display_contents_1_t *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };
    displays[HWC_DISPLAY_PRIMARY] = nullptr;
    mHwc->set(mHwc, HWC_DISPLAY_PRIMARY, displays);
    mPrepared = false;
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

void
HwcComposer2D::Reset()
{
    mPrepared = false;
}
#endif

bool
HwcComposer2D::TryRender(Layer* aRoot,
                         bool aGeometryChanged)
{
    MOZ_ASSERT(Initialized());
    if (mList) {
        setHwcGeometry(aGeometryChanged);
        mList->numHwLayers = 0;
        mHwcLayerMap.Clear();
    }

    if (mPrepared) {
        Reset();
    }

    // XXX: The clear() below means all rect vectors will be have to be
    // reallocated. We may want to avoid this if possible
    mVisibleRegions.clear();

    MOZ_ASSERT(mHwcLayerMap.IsEmpty());
    if (!PrepareLayerList(aRoot,
                          mScreenRect,
                          gfx::Matrix()))
    {
        mHwcLayerMap.Clear();
        LOGD("Render aborted. Nothing was drawn to the screen");
        return false;
    }

    // Send data to LayerScope for debugging
    SendtoLayerScope();

    if (!TryHwComposition()) {
        LOGD("H/W Composition failed");
        LayerScope::CleanLayer();
        return false;
    }

    LOGD("Frame rendered");
    return true;
}

void
HwcComposer2D::SendtoLayerScope()
{
    if (!LayerScope::CheckSendable()) {
        return;
    }

    const int len = mList->numHwLayers;
    for (int i = 0; i < len; ++i) {
        LayerComposite* layer = mHwcLayerMap[i];
        const hwc_rect_t r = mList->hwLayers[i].displayFrame;
        LayerScope::SendLayer(layer, r.right - r.left, r.bottom - r.top);
    }
}

} // namespace mozilla
