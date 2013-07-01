/* Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GonkDisplayJB.h"
#include <gui/SurfaceTextureClient.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <suspend/autosuspend.h>

#include "GraphicBufferAlloc.h"
#include "BootAnimation.h"

using namespace android;

namespace mozilla {

static GonkDisplayJB* sGonkDisplay = nullptr;

GonkDisplayJB::GonkDisplayJB()
    : mList(nullptr)
    , mModule(nullptr)
    , mFBModule(nullptr)
    , mHwc(nullptr)
    , mFBDevice(nullptr)
    , mEnabledCallback(nullptr)
{
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &mFBModule);
    ALOGW_IF(err, "%s module not found", GRALLOC_HARDWARE_MODULE_ID);
    if (!err) {
        err = framebuffer_open(mFBModule, &mFBDevice);
        ALOGW_IF(err, "could not open framebuffer");
    }

    if (!err) {
        mWidth = mFBDevice->width;
        mHeight = mFBDevice->height;
        xdpi = mFBDevice->xdpi;
        /* The emulator actually reports RGBA_8888, but EGL doesn't return
         * any matching configuration. We force RGBX here to fix it. */
        surfaceformat = HAL_PIXEL_FORMAT_RGBX_8888;
    }

    err = hw_get_module(HWC_HARDWARE_MODULE_ID, &mModule);
    ALOGW_IF(err, "%s module not found", HWC_HARDWARE_MODULE_ID);
    if (!err) {
        err = hwc_open_1(mModule, &mHwc);
        ALOGE_IF(err, "%s device failed to initialize (%s)",
                 HWC_HARDWARE_COMPOSER, strerror(-err));
    }

    if (!err) {
        if (mFBDevice) {
            framebuffer_close(mFBDevice);
            mFBDevice = nullptr;
        }

        int32_t values[3];
        const uint32_t attrs[] = {
            HWC_DISPLAY_WIDTH,
            HWC_DISPLAY_HEIGHT,
            HWC_DISPLAY_DPI_X,
            HWC_DISPLAY_NO_ATTRIBUTE
        };
        mHwc->getDisplayAttributes(mHwc, 0, 0, attrs, values);

        mWidth = values[0];
        mHeight = values[1];
        xdpi = values[2];
        surfaceformat = HAL_PIXEL_FORMAT_RGBA_8888;
    }

    mAlloc = new GraphicBufferAlloc();
    mFBSurface = new FramebufferSurface(0, mWidth, mHeight, surfaceformat, mAlloc);

    sp<SurfaceTextureClient> stc = new SurfaceTextureClient(static_cast<sp<ISurfaceTexture> >(mFBSurface->getBufferQueue()));
    mSTClient = stc;

    mList = (hwc_display_contents_1_t *)malloc(sizeof(*mList) + (sizeof(hwc_layer_1_t)*2));
    SetEnabled(true);

    status_t error;
    mBootAnimBuffer = mAlloc->createGraphicBuffer(mWidth, mHeight, surfaceformat, GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER, &error);
    StartBootAnimation();
}

GonkDisplayJB::~GonkDisplayJB()
{
    if (mHwc)
        hwc_close_1(mHwc);
    if (mFBDevice)
        framebuffer_close(mFBDevice);
    free(mList);
}

ANativeWindow*
GonkDisplayJB::GetNativeWindow()
{
    return mSTClient.get();
}

void
GonkDisplayJB::SetEnabled(bool enabled)
{
    if (enabled)
        autosuspend_disable();

    if (mHwc)
        mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, !enabled);
    else if (mFBDevice->enableScreen)
        mFBDevice->enableScreen(mFBDevice, enabled);

    if (mEnabledCallback)
        mEnabledCallback(enabled);

    if (!enabled)
        autosuspend_enable();
}

void
GonkDisplayJB::OnEnabled(OnEnabledCallbackType callback)
{
    mEnabledCallback = callback;
}

void*
GonkDisplayJB::GetHWCDevice()
{
    return mHwc;
}

bool
GonkDisplayJB::SwapBuffers(EGLDisplay dpy, EGLSurface sur)
{
    StopBootAnimation();
    mBootAnimBuffer = nullptr;

    mList->dpy = dpy;
    mList->sur = sur;
    eglSwapBuffers(dpy, sur);
    return Post(mFBSurface->lastHandle, mFBSurface->lastFenceFD);
}

bool
GonkDisplayJB::Post(buffer_handle_t buf, int fence)
{
    if (!mHwc) {
        if (fence >= 0)
            close(fence);
        return !mFBDevice->post(mFBDevice, buf);
    }

    hwc_display_contents_1_t *displays[HWC_NUM_DISPLAY_TYPES] = {NULL};
    const hwc_rect_t r = { 0, 0, mWidth, mHeight };
    displays[HWC_DISPLAY_PRIMARY] = mList;
    mList->retireFenceFd = -1;
    mList->numHwLayers = 2;
    mList->flags = HWC_GEOMETRY_CHANGED;
    mList->hwLayers[0].compositionType = HWC_BACKGROUND;
    mList->hwLayers[0].hints = 0;
    mList->hwLayers[0].flags = 0;
    mList->hwLayers[0].backgroundColor = {0};
    mList->hwLayers[1].compositionType = HWC_FRAMEBUFFER_TARGET;
    mList->hwLayers[1].hints = 0;
    mList->hwLayers[1].flags = 0;
    mList->hwLayers[1].handle = buf;
    mList->hwLayers[1].transform = 0;
    mList->hwLayers[1].blending = HWC_BLENDING_PREMULT;
    mList->hwLayers[1].sourceCrop = r;
    mList->hwLayers[1].displayFrame = r;
    mList->hwLayers[1].visibleRegionScreen.numRects = 1;
    mList->hwLayers[1].visibleRegionScreen.rects = &mList->hwLayers[0].sourceCrop;
    mList->hwLayers[1].acquireFenceFd = fence;
    mList->hwLayers[1].releaseFenceFd = -1;
    mHwc->prepare(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
    int err = mHwc->set(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
    mFBSurface->setReleaseFenceFd(mList->hwLayers[1].releaseFenceFd);
    if (mList->retireFenceFd >= 0)
        close(mList->retireFenceFd);
    return !err;
}

ANativeWindowBuffer*
GonkDisplayJB::DequeueBuffer()
{
    return static_cast<ANativeWindowBuffer*>(mBootAnimBuffer.get());
}

bool
GonkDisplayJB::QueueBuffer(ANativeWindowBuffer* buf)
{
    bool success = Post(buf->handle, -1);
    return success;
}

__attribute__ ((visibility ("default")))
GonkDisplay*
GetGonkDisplay()
{
    if (!sGonkDisplay)
        sGonkDisplay = new GonkDisplayJB();
    return sGonkDisplay;
}

} // namespace mozilla
