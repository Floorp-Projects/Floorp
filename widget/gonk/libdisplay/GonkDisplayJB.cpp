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
#if ANDROID_VERSION == 17
#include <gui/SurfaceTextureClient.h>
#else
#include <gui/Surface.h>
#include <gui/GraphicBufferAlloc.h>
#endif

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware/power.h>
#include <suspend/autosuspend.h>

#if ANDROID_VERSION == 17
#include "GraphicBufferAlloc.h"
#endif
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
    , mPowerModule(nullptr)
{
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &mFBModule);
    ALOGW_IF(err, "%s module not found", GRALLOC_HARDWARE_MODULE_ID);
    if (!err) {
        err = framebuffer_open(mFBModule, &mFBDevice);
        ALOGW_IF(err, "could not open framebuffer");
    }

    if (!err && mFBDevice) {
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

    /* Fallback on the FB rendering path instead of trying to support HWC 1.0 */
    if (!err && mHwc->common.version == HWC_DEVICE_API_VERSION_1_0) {
        hwc_close_1(mHwc);
        mHwc = nullptr;
    }

    if (!err && mHwc) {
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
        xdpi = values[2] / 1000.0f;
        surfaceformat = HAL_PIXEL_FORMAT_RGBA_8888;
    }

    err = hw_get_module(POWER_HARDWARE_MODULE_ID,
                                           (hw_module_t const**)&mPowerModule);
    if (!err)
        mPowerModule->init(mPowerModule);
    ALOGW_IF(err, "Couldn't load %s module (%s)", POWER_HARDWARE_MODULE_ID, strerror(-err));

    mAlloc = new GraphicBufferAlloc();

    status_t error;
    uint32_t usage = GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;
    mBootAnimBuffer = mAlloc->createGraphicBuffer(mWidth, mHeight, surfaceformat, usage, &error);
    if (error != NO_ERROR || !mBootAnimBuffer.get()) {
        ALOGI("Trying to create BRGA format framebuffer");
        surfaceformat = HAL_PIXEL_FORMAT_BGRA_8888;
        mBootAnimBuffer = mAlloc->createGraphicBuffer(mWidth, mHeight, surfaceformat, usage, &error);
    }

    mFBSurface = new FramebufferSurface(0, mWidth, mHeight, surfaceformat, mAlloc);

#if ANDROID_VERSION == 17
    sp<SurfaceTextureClient> stc = new SurfaceTextureClient(static_cast<sp<ISurfaceTexture> >(mFBSurface->getBufferQueue()));
#else
    sp<Surface> stc = new Surface(static_cast<sp<IGraphicBufferProducer> >(mFBSurface->getBufferQueue()));
#endif
    mSTClient = stc;

    mList = (hwc_display_contents_1_t *)malloc(sizeof(*mList) + (sizeof(hwc_layer_1_t)*2));
    if (mHwc)
        mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, 0);

    if (error == NO_ERROR && mBootAnimBuffer.get()) {
        ALOGI("Starting bootanimation with (%d) format framebuffer", surfaceformat);
        StartBootAnimation();
    } else
        ALOGW("Couldn't show bootanimation (%s)", strerror(-error));
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
    if (enabled) {
        autosuspend_disable();
        mPowerModule->setInteractive(mPowerModule, true);
    }

    if (mHwc)
        mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, !enabled);
    else if (mFBDevice->enableScreen)
        mFBDevice->enableScreen(mFBDevice, enabled);

    if (mEnabledCallback)
        mEnabledCallback(enabled);

    if (!enabled) {
        autosuspend_enable();
        mPowerModule->setInteractive(mPowerModule, false);
    }
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

void*
GonkDisplayJB::GetFBSurface()
{
    return mFBSurface.get();
}

bool
GonkDisplayJB::SwapBuffers(EGLDisplay dpy, EGLSurface sur)
{
    StopBootAnimation();
    mBootAnimBuffer = nullptr;

    // Should be called when composition rendering is complete for a frame.
    // Only HWC v1.0 needs this call.
    // HWC > v1.0 case, do not call compositionComplete().
    // mFBDevice is present only when HWC is v1.0.
    if (mFBDevice && mFBDevice->compositionComplete) {
        mFBDevice->compositionComplete(mFBDevice);
    }

#if ANDROID_VERSION == 17
    mList->dpy = dpy;
    mList->sur = sur;
#else
    mList->outbuf = nullptr;
    mList->outbufAcquireFenceFd = -1;
#endif
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
    /* Skip this layer so the hwc module doesn't complain about null handles */
    mList->hwLayers[0].flags = HWC_SKIP_LAYER;
    mList->hwLayers[0].backgroundColor = {0};
    mList->hwLayers[0].acquireFenceFd = -1;
    mList->hwLayers[0].releaseFenceFd = -1;
    /* hwc module checks displayFrame even though it shouldn't */
    mList->hwLayers[0].displayFrame = r;
    mList->hwLayers[1].compositionType = HWC_FRAMEBUFFER_TARGET;
    mList->hwLayers[1].hints = 0;
    mList->hwLayers[1].flags = 0;
    mList->hwLayers[1].handle = buf;
    mList->hwLayers[1].transform = 0;
    mList->hwLayers[1].blending = HWC_BLENDING_PREMULT;
    mList->hwLayers[1].sourceCrop = r;
    mList->hwLayers[1].displayFrame = r;
    mList->hwLayers[1].visibleRegionScreen.numRects = 1;
    mList->hwLayers[1].visibleRegionScreen.rects = &mList->hwLayers[1].sourceCrop;
    mList->hwLayers[1].acquireFenceFd = fence;
    mList->hwLayers[1].releaseFenceFd = -1;
#if ANDROID_VERSION == 18
    mList->hwLayers[1].planeAlpha = 0xFF;
#endif
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

void
GonkDisplayJB::UpdateFBSurface(EGLDisplay dpy, EGLSurface sur)
{
    StopBootAnimation();
    mBootAnimBuffer = nullptr;
    eglSwapBuffers(dpy, sur);
}

void
GonkDisplayJB::SetFBReleaseFd(int fd)
{
    mFBSurface->setReleaseFenceFd(fd);
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
