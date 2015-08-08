/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#include "HwcHAL.h"
#include "libdisplay/GonkDisplay.h"
#include "mozilla/Assertions.h"

namespace mozilla {

HwcHAL::HwcHAL()
    : HwcHALBase()
{
    // Some HALs don't want to open hwc twice.
    // If GetDisplay already load hwc module, we don't need to load again
    mHwc = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();
    if (!mHwc) {
        printf_stderr("HwcHAL Error: Cannot load hwcomposer");
        return;
    }
}

HwcHAL::~HwcHAL()
{
    mHwc = nullptr;
}

bool
HwcHAL::Query(QueryType aType)
{
    if (!mHwc || !mHwc->query) {
        return false;
    }

    bool value = false;
    int supported = 0;
    if (mHwc->query(mHwc, static_cast<int>(aType), &supported) == 0/*android::NO_ERROR*/) {
        value = !!supported;
    }
    return value;
}

int
HwcHAL::Set(HwcList *aList,
            uint32_t aDisp)
{
    MOZ_ASSERT(mHwc);
    if (!mHwc) {
        return -1;
    }

    HwcList *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };
    displays[aDisp] = aList;
    return mHwc->set(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
}

int
HwcHAL::ResetHwc()
{
    return Set(nullptr, HWC_DISPLAY_PRIMARY);
}

int
HwcHAL::Prepare(HwcList *aList,
                uint32_t aDisp,
                hwc_rect_t aDispRect,
                buffer_handle_t aHandle,
                int aFenceFd)
{
    MOZ_ASSERT(mHwc);
    if (!mHwc) {
        printf_stderr("HwcHAL Error: HwcDevice doesn't exist. A fence might be leaked.");
        return -1;
    }

    HwcList *displays[HWC_NUM_DISPLAY_TYPES] = { nullptr };
    displays[aDisp] = aList;
#if ANDROID_VERSION >= 18
    aList->outbufAcquireFenceFd = -1;
    aList->outbuf = nullptr;
#endif
    aList->retireFenceFd = -1;

    const auto idx = aList->numHwLayers - 1;
    aList->hwLayers[idx].hints = 0;
    aList->hwLayers[idx].flags = 0;
    aList->hwLayers[idx].transform = 0;
    aList->hwLayers[idx].handle = aHandle;
    aList->hwLayers[idx].blending = HWC_BLENDING_PREMULT;
    aList->hwLayers[idx].compositionType = HWC_FRAMEBUFFER_TARGET;
    SetCrop(aList->hwLayers[idx], aDispRect);
    aList->hwLayers[idx].displayFrame = aDispRect;
    aList->hwLayers[idx].visibleRegionScreen.numRects = 1;
    aList->hwLayers[idx].visibleRegionScreen.rects = &aList->hwLayers[idx].displayFrame;
    aList->hwLayers[idx].acquireFenceFd = aFenceFd;
    aList->hwLayers[idx].releaseFenceFd = -1;
#if ANDROID_VERSION >= 18
    aList->hwLayers[idx].planeAlpha = 0xFF;
#endif
    return mHwc->prepare(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
}

bool
HwcHAL::SupportTransparency() const
{
#if ANDROID_VERSION >= 18
    return true;
#endif
    return false;
}

uint32_t
HwcHAL::GetGeometryChangedFlag(bool aGeometryChanged) const
{
#if ANDROID_VERSION >= 19
    return aGeometryChanged ? HWC_GEOMETRY_CHANGED : 0;
#else
    return HWC_GEOMETRY_CHANGED;
#endif
}

void
HwcHAL::SetCrop(HwcLayer &aLayer,
                const hwc_rect_t &aSrcCrop) const
{
    if (GetAPIVersion() >= HwcAPIVersion(1, 3)) {
#if ANDROID_VERSION >= 19
        aLayer.sourceCropf.left = aSrcCrop.left;
        aLayer.sourceCropf.top = aSrcCrop.top;
        aLayer.sourceCropf.right = aSrcCrop.right;
        aLayer.sourceCropf.bottom = aSrcCrop.bottom;
#endif
    } else {
        aLayer.sourceCrop = aSrcCrop;
    }
}

bool
HwcHAL::EnableVsync(bool aEnable)
{
    // Only support hardware vsync on kitkat, L and up due to inaccurate timings
    // with JellyBean.
#if (ANDROID_VERSION == 19 || ANDROID_VERSION >= 21)
    if (!mHwc) {
        return false;
    }
    return !mHwc->eventControl(mHwc,
                               HWC_DISPLAY_PRIMARY,
                               HWC_EVENT_VSYNC,
                               aEnable);
#else
    return false;
#endif
}

bool
HwcHAL::RegisterHwcEventCallback(const HwcHALProcs_t &aProcs)
{
    if (!mHwc || !mHwc->registerProcs) {
        printf_stderr("Failed to get hwc\n");
        return false;
    }

    // Disable Vsync first, and then register callback functions.
    mHwc->eventControl(mHwc,
                       HWC_DISPLAY_PRIMARY,
                       HWC_EVENT_VSYNC,
                       false);
    static const hwc_procs_t sHwcJBProcs = {aProcs.invalidate,
                                            aProcs.vsync,
                                            aProcs.hotplug};
    mHwc->registerProcs(mHwc, &sHwcJBProcs);

    // Only support hardware vsync on kitkat, L and up due to inaccurate timings
    // with JellyBean.
#if (ANDROID_VERSION == 19 || ANDROID_VERSION >= 21)
    return true;
#else
    return false;
#endif
}

uint32_t
HwcHAL::GetAPIVersion() const
{
    if (!mHwc) {
        // default value: HWC_MODULE_API_VERSION_0_1
        return 1;
    }
    return mHwc->common.version;
}

// Create HwcHAL
UniquePtr<HwcHALBase>
HwcHALBase::CreateHwcHAL()
{
    return Move(MakeUnique<HwcHAL>());
}

} // namespace mozilla
