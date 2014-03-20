/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef mozilla_HwcComposer2D
#define mozilla_HwcComposer2D

#include "Composer2D.h"
#include "Layers.h"
#include <vector>
#include <list>

#include <hardware/hwcomposer.h>
#if ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif

namespace mozilla {

namespace layers {
class ContainerLayer;
class Layer;
}

//Holds a dynamically allocated vector of rectangles
//used to decribe the complex visible region of a layer
typedef std::vector<hwc_rect_t> RectVector;
#if ANDROID_VERSION >= 17
typedef hwc_composer_device_1_t HwcDevice;
typedef hwc_display_contents_1_t HwcList;
typedef hwc_layer_1_t HwcLayer;
#else
typedef hwc_composer_device_t HwcDevice;
typedef hwc_layer_list_t HwcList;
typedef hwc_layer_t HwcLayer;
#endif

/*
 * HwcComposer2D provides a way for gecko to render frames
 * using hwcomposer.h in the AOSP HAL.
 *
 * hwcomposer.h defines an interface for display composition
 * using dedicated hardware. This hardware is usually faster
 * or more power efficient than the GPU. However, in exchange
 * for better performance, generality has to be sacrificed:
 * no 3d transforms, no intermediate surfaces, no special shader effects,
 * and loss of other goodies depending on the platform.
 *
 * In general, when hwc is enabled gecko tries to compose
 * its frames using HwcComposer2D first. Then if HwcComposer2D is
 * unable to compose a frame then it falls back to compose it
 * using the GPU with OpenGL.
 *
 */
class HwcComposer2D : public mozilla::layers::Composer2D {
public:
    HwcComposer2D();
    virtual ~HwcComposer2D();

    int Init(hwc_display_t aDisplay, hwc_surface_t aSurface);

    bool Initialized() const { return mHwc; }

    static HwcComposer2D* GetInstance();

    // Returns TRUE if the container has been succesfully rendered
    // Returns FALSE if the container cannot be fully rendered
    // by this composer so nothing was rendered at all
    bool TryRender(layers::Layer* aRoot, const gfx::Matrix& aGLWorldTransform) MOZ_OVERRIDE;

    bool Render(EGLDisplay dpy, EGLSurface sur);

private:
    void Reset();
    void Prepare(buffer_handle_t fbHandle, int fence);
    bool Commit();
    bool TryHwComposition();
    bool ReallocLayerList();
    bool PrepareLayerList(layers::Layer* aContainer, const nsIntRect& aClip,
          const gfxMatrix& aParentTransform, const gfxMatrix& aGLWorldTransform);
    void setCrop(HwcLayer* layer, hwc_rect_t srcCrop);

    HwcDevice*              mHwc;
    HwcList*                mList;
    hwc_display_t           mDpy;
    hwc_surface_t           mSur;
    nsIntRect               mScreenRect;
    int                     mMaxLayerCount;
    bool                    mColorFill;
    bool                    mRBSwapSupport;
    //Holds all the dynamically allocated RectVectors needed
    //to render the current frame
    std::list<RectVector>   mVisibleRegions;
#if ANDROID_VERSION >= 17
    android::sp<android::Fence> mPrevRetireFence;
    android::sp<android::Fence> mPrevDisplayFence;
#endif
    nsTArray<layers::LayerComposite*> mHwcLayerMap;
    bool                    mPrepared;
};

} // namespace mozilla

#endif // mozilla_HwcComposer2D
