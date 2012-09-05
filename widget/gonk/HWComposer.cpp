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

#include <string.h>
#include "HWComposer.h"
#include <hardware/hardware.h>
#include <EGL/egl.h>

namespace android {
// ---------------------------------------------------------------------------

HWComposer::HWComposer()
    : mModule(0), mHwc(0),
      mDpy(EGL_NO_DISPLAY), mSur(EGL_NO_SURFACE)
{
}

HWComposer::~HWComposer() {
    if (mHwc) {
        hwc_close(mHwc);
    }
}

int HWComposer::init() {
    int err = hw_get_module(HWC_HARDWARE_MODULE_ID, &mModule);
    LOGW_IF(err, "%s module not found", HWC_HARDWARE_MODULE_ID);
    if (err)
        return err;

    err = hwc_open(mModule, &mHwc);
    LOGE_IF(err, "%s device failed to initialize (%s)",
            HWC_HARDWARE_COMPOSER, strerror(-err));
    if (err) {
        mHwc = NULL;
        return err;
    }

    if (mHwc->registerProcs) {
        mCBContext.hwc = this;
        mHwc->registerProcs(mHwc, &mCBContext.procs);
    }

    return 0;
}

status_t HWComposer::swapBuffers(hwc_display_t dpy, hwc_surface_t surf) const {
    mHwc->prepare(mHwc, NULL);
    int err = mHwc->set(mHwc, dpy, surf, 0);
    return (status_t)err;
}

// ---------------------------------------------------------------------------
}; // namespace android
