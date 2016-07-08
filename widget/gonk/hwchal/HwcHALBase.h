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

#ifndef mozilla_HwcHALBase
#define mozilla_HwcHALBase

#include "mozilla/UniquePtr.h"
#include "nsRect.h"

#include <hardware/hwcomposer.h>

#ifndef HWC_BLIT
#if ANDROID_VERSION >= 21
#define HWC_BLIT 0xFF
#elif ANDROID_VERSION >= 17
#define HWC_BLIT (HWC_FRAMEBUFFER_TARGET + 1)
#else
// ICS didn't support this. However, we define this
// for passing compilation
#define HWC_BLIT 0xFF
#endif // #if ANDROID_VERSION
#endif // #ifndef HWC_BLIT

namespace mozilla {

#if ANDROID_VERSION >= 17
using HwcDevice = hwc_composer_device_1_t;
using HwcList   = hwc_display_contents_1_t;
using HwcLayer  = hwc_layer_1_t;
#else
using HwcDevice = hwc_composer_device_t;
using HwcList   = hwc_layer_list_t;
using HwcLayer  = hwc_layer_t;
#endif

// HwcHAL definition for HwcEvent callback types
// Note: hwc_procs is different between ICS and later,
//       and the signature of invalidate is also different.
//       Use this wrap struct to hide the detail. BTW,
//       we don't have to register callback functions on ICS, so
//       there is no callbacks for ICS in HwcHALProcs.
typedef struct HwcHALProcs {
    void (*invalidate)(const struct hwc_procs* procs);
    void (*vsync)(const struct hwc_procs* procs, int disp, int64_t timestamp);
    void (*hotplug)(const struct hwc_procs* procs, int disp, int connected);
} HwcHALProcs_t;

// HwcHAL class
// This class handle all the HAL related work
// The purpose of HwcHAL is to make HwcComposer2D simpler.
class HwcHALBase {

public:
    // Query Types. We can add more types easily in the future
    enum class QueryType {
        COLOR_FILL = 0x8,
        RB_SWAP = 0x40
    };

public:
    explicit HwcHALBase() = default;

    virtual ~HwcHALBase() {}

    // Create HwcHAL module, Only HwcComposer2D calls this.
    // If other modules want to use HwcHAL, please use APIs in
    // HwcComposer2D
    static UniquePtr<HwcHALBase> CreateHwcHAL();

    // Check if mHwc exists
    virtual bool HasHwc() const = 0;

    // Set EGL info (only ICS need this info)
    virtual void SetEGLInfo(hwc_display_t aEGLDisplay,
                            hwc_surface_t aEGLSurface) = 0;

    // HwcDevice query properties
    virtual bool Query(QueryType aType) = 0;

    // HwcDevice set
    virtual int Set(HwcList *aList,
                    uint32_t aDisp) = 0;

    // Reset HwcDevice
    virtual int ResetHwc() = 0;

    // HwcDevice prepare
    virtual int Prepare(HwcList *aList,
                        uint32_t aDisp,
                        hwc_rect_t aDispRect,
                        buffer_handle_t aHandle,
                        int aFenceFd) = 0;

    // Check transparency support
    virtual bool SupportTransparency() const = 0;

    // Get a geometry change flag
    virtual uint32_t GetGeometryChangedFlag(bool aGeometryChanged) const = 0;

    // Set crop help
    virtual void SetCrop(HwcLayer &aLayer,
                         const hwc_rect_t &aSrcCrop) const = 0;

    // Enable HW Vsync
    virtual bool EnableVsync(bool aEnable) = 0;

    // Register HW event callback functions
    virtual bool RegisterHwcEventCallback(const HwcHALProcs_t &aProcs) = 0;

protected:
    constexpr static uint32_t HwcAPIVersion(uint32_t aMaj, uint32_t aMin) {
        // HARDWARE_MAKE_API_VERSION_2, from Android hardware.h
        return (((aMaj & 0xff) << 24) | ((aMin & 0xff) << 16) | (1 & 0xffff));
    }
};

} // namespace mozilla

#endif // mozilla_HwcHALBase
