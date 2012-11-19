/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ANDROID_SF_HWCOMPOSER_H
#define ANDROID_SF_HWCOMPOSER_H

#include <utils/Vector.h>

#include "hardware/hwcomposer.h"

namespace android {
// ---------------------------------------------------------------------------

class String8;

class HWComposer
{
public:

    HWComposer();
    ~HWComposer();

    int init();

    // swap buffers using vendor specific implementation
    status_t swapBuffers(hwc_display_t dpy, hwc_surface_t surf) const;

protected:
    struct cb_context {
        hwc_procs_t procs;
        HWComposer* hwc;
    };
    void invalidate();

    hw_module_t const*      mModule;
    hwc_composer_device_t*  mHwc;
    hwc_display_t           mDpy;
    hwc_surface_t           mSur;
    cb_context              mCBContext;
};


// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SF_HWCOMPOSER_H
