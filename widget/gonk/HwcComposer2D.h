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

#include "hardware/hwcomposer.h"

namespace mozilla {

namespace layers {
class ContainerLayer;
class Layer;
}

//Holds a dynamically allocated vector of rectangles
//used to decribe the complex visible region of a layer
typedef std::vector<hwc_rect_t> RectVector;

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
    bool TryRender(layers::Layer* aRoot, const gfxMatrix& aGLWorldTransform) MOZ_OVERRIDE;

private:
    bool ReallocLayerList();
    bool PrepareLayerList(layers::Layer* aContainer, const nsIntRect& aClip,
          const gfxMatrix& aParentTransform, const gfxMatrix& aGLWorldTransform);

    hwc_composer_device_t*  mHwc;
    hwc_layer_list_t*       mList;
    hwc_display_t           mDpy;
    hwc_surface_t           mSur;
    nsIntRect               mScreenRect;
    int                     mMaxLayerCount;
    bool                    mColorFill;
    //Holds all the dynamically allocated RectVectors needed
    //to render the current frame
    std::list<RectVector>   mVisibleRegions;
};

} // namespace mozilla

#endif // mozilla_HwcComposer2D
