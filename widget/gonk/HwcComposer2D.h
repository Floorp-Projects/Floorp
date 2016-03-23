/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
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
#include "hwchal/HwcHALBase.h"              // for HwcHAL
#include "HwcUtils.h"                       // for RectVector
#include "Layers.h"
#include "mozilla/Mutex.h"
#include "mozilla/layers/FenceUtils.h"      // for FenceHandle
#include "mozilla/UniquePtr.h"              // for HwcHAL

#include <vector>
#include <list>

#include <utils/Timers.h>

class nsScreenGonk;

namespace mozilla {

namespace gl {
    class GLContext;
}

namespace layers {
class CompositorBridgeParent;
class Layer;
}

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

    static HwcComposer2D* GetInstance();

    // Returns TRUE if the container has been succesfully rendered
    // Returns FALSE if the container cannot be fully rendered
    // by this composer so nothing was rendered at all
    virtual bool TryRenderWithHwc(layers::Layer* aRoot,
                                  nsIWidget* aWidget,
                                  bool aGeometryChanged,
                                  bool aHasImageHostOverlays) override;

    virtual bool Render(nsIWidget* aWidget) override;

    virtual bool HasHwc() override { return mHal->HasHwc(); }

    bool EnableVsync(bool aEnable);
    bool RegisterHwcEventCallback();
    void Vsync(int aDisplay, int64_t aTimestamp);
    void Invalidate();
    void Hotplug(int aDisplay, int aConnected);
    void SetCompositorBridgeParent(layers::CompositorBridgeParent* aCompositorBridgeParent);

private:
    void Reset();
    void Prepare(buffer_handle_t dispHandle, int fence, nsScreenGonk* screen);
    bool Commit(nsScreenGonk* aScreen);
    bool TryHwComposition(nsScreenGonk* aScreen);
    bool ReallocLayerList();
    bool PrepareLayerList(layers::Layer* aContainer, const nsIntRect& aClip,
          const gfx::Matrix& aParentTransform,
          bool aFindSidebandStreams);
    void SendtoLayerScope();

    UniquePtr<HwcHALBase>   mHal;
    HwcList*                mList;
    nsIntRect               mScreenRect;
    int                     mMaxLayerCount;
    bool                    mColorFill;
    bool                    mRBSwapSupport;
    //Holds all the dynamically allocated RectVectors needed
    //to render the current frame
    std::list<HwcUtils::RectVector>   mVisibleRegions;
    layers::FenceHandle mPrevRetireFence;
    layers::FenceHandle mPrevDisplayFence;
    nsTArray<HwcLayer>      mCachedSidebandLayers;
    nsTArray<layers::LayerComposite*> mHwcLayerMap;
    bool                    mPrepared;
    bool                    mHasHWVsync;
    layers::CompositorBridgeParent* mCompositorBridgeParent;
    Mutex mLock;
};

} // namespace mozilla

#endif // mozilla_HwcComposer2D
