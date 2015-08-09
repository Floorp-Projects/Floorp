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

#include "HwcICS.h"
#include "cutils/properties.h"
#include "libdisplay/GonkDisplay.h"

namespace mozilla {

HwcICS::HwcICS()
    : HwcHALBase()
{
    mHwc = (HwcDevice*)GetGonkDisplay()->GetHWCDevice();
    if (!mHwc) {
        printf_stderr("HwcHAL Error: Cannot load hwcomposer");
    }
}

HwcICS::~HwcICS()
{
    mHwc = nullptr;
    mEGLDisplay = nullptr;
    mEGLSurface = nullptr;
}

void
HwcICS::SetEGLInfo(hwc_display_t aEGLDisplay,
                   hwc_surface_t aEGLSurface)
{
    mEGLDisplay = aEGLDisplay;
    mEGLSurface = aEGLSurface;
}

bool
HwcICS::Query(QueryType aType)
{
    bool value = false;
    switch (aType) {
        case QueryType::COLOR_FILL: {
            char propValue[PROPERTY_VALUE_MAX];
            property_get("ro.display.colorfill", propValue, "0");
            value = (atoi(propValue) == 1) ? true : false;
            break;
        }
        case QueryType::RB_SWAP:
            value = true;
            break;

        default:
            value = false;
    }
    return value;
}

int
HwcICS::Set(HwcList* aList,
            uint32_t aDisp)
{
    if (!mHwc) {
        return -1;
    }
    return mHwc->set(mHwc, mEGLDisplay, mEGLSurface, aList);
}

int
HwcICS::ResetHwc()
{
    return -1;
}

int
HwcICS::Prepare(HwcList *aList,
                uint32_t aDisp,
                hwc_rect_t aDispRect,
                buffer_handle_t aHandle,
                int aFenceFd)
{
    return mHwc->prepare(mHwc, aList);
}

bool
HwcICS::SupportTransparency() const
{
    return false;
}

uint32_t
HwcICS::GetGeometryChangedFlag(bool aGeometryChanged) const
{
    return HWC_GEOMETRY_CHANGED;
}

void
HwcICS::SetCrop(HwcLayer& aLayer,
                const hwc_rect_t &aSrcCrop) const
{
    aLayer.sourceCrop = aSrcCrop;
}

bool
HwcICS::EnableVsync(bool aEnable)
{
    return false;
}

bool
HwcICS::RegisterHwcEventCallback(const HwcHALProcs_t &aProcs)
{
    return false;
}

// Create HwcICS
UniquePtr<HwcHALBase>
HwcHALBase::CreateHwcHAL()
{
    return Move(MakeUnique<HwcICS>());
}

} // namespace mozilla
