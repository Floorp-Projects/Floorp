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

#include "FramebufferSurface.h"
#if ANDROID_VERSION == 17
#include "GraphicBufferAlloc.h"
#endif
#include "BootAnimation.h"

using namespace android;

namespace mozilla {

static GonkDisplayJB* sGonkDisplay = nullptr;

GonkDisplayJB::GonkDisplayJB()
    : mModule(nullptr)
    , mFBModule(nullptr)
    , mHwc(nullptr)
    , mFBDevice(nullptr)
    , mPowerModule(nullptr)
    , mList(nullptr)
    , mEnabledCallback(nullptr)
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

#if ANDROID_VERSION >= 21
    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer, mAlloc);
#elif ANDROID_VERSION >= 19
    sp<BufferQueue> consumer = new BufferQueue(mAlloc);
    sp<IGraphicBufferProducer> producer = consumer;
#elif ANDROID_VERSION >= 18
    sp<BufferQueue> consumer = new BufferQueue(true, mAlloc);
    sp<IGraphicBufferProducer> producer = consumer;
#else
    sp<BufferQueue> consumer = new BufferQueue(true, mAlloc);
#endif

    mDispSurface = new FramebufferSurface(0, mWidth, mHeight, surfaceformat, consumer);

#if ANDROID_VERSION == 17
    sp<SurfaceTextureClient> stc = new SurfaceTextureClient(
        static_cast<sp<ISurfaceTexture> >(mDispSurface->getBufferQueue()));
#else
    sp<Surface> stc = new Surface(producer);
#endif
    mSTClient = stc;
    mList = (hwc_display_contents_1_t *)malloc(sizeof(*mList) + (sizeof(hwc_layer_1_t)*2));

    uint32_t usage = GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;
    if (mFBDevice) {
        // If device uses fb, they can not use single buffer for boot animation
        mSTClient->perform(mSTClient.get(), NATIVE_WINDOW_SET_BUFFER_COUNT, 2);
        mSTClient->perform(mSTClient.get(), NATIVE_WINDOW_SET_USAGE, usage);
    } else if (mHwc) {
#if ANDROID_VERSION >= 21
        if (mHwc->common.version >= HWC_DEVICE_API_VERSION_1_4) {
            mHwc->setPowerMode(mHwc, HWC_DISPLAY_PRIMARY, HWC_POWER_MODE_NORMAL);
        } else {
            mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, 0);
        }
#else
        mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, 0);
#endif
        // For devices w/ hwc v1.0 or no hwc, this buffer can not be created,
        // only create this buffer for devices w/ hwc version > 1.0.
        mBootAnimBuffer = mAlloc->createGraphicBuffer(mWidth, mHeight, surfaceformat, usage, &err);
    }

    ALOGI("Starting bootanimation with (%d) format framebuffer", surfaceformat);
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
    StopBootAnim();

    return mSTClient.get();
}

void
GonkDisplayJB::SetEnabled(bool enabled)
{
    if (enabled) {
        autosuspend_disable();
        mPowerModule->setInteractive(mPowerModule, true);
    }

    if (!enabled && mEnabledCallback) {
        mEnabledCallback(enabled);
    }

#if ANDROID_VERSION >= 21
    if (mHwc) {
        if (mHwc->common.version >= HWC_DEVICE_API_VERSION_1_4) {
            mHwc->setPowerMode(mHwc, HWC_DISPLAY_PRIMARY,
                (enabled ? HWC_POWER_MODE_NORMAL : HWC_POWER_MODE_OFF));
        } else {
            mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, !enabled);
        }
    } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
    }
#else
    if (mHwc && mHwc->blank) {
        mHwc->blank(mHwc, HWC_DISPLAY_PRIMARY, !enabled);
    } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
    }
#endif

    if (enabled && mEnabledCallback) {
        mEnabledCallback(enabled);
    }

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
GonkDisplayJB::GetDispSurface()
{
    return mDispSurface.get();
}

bool
GonkDisplayJB::SwapBuffers(EGLDisplay dpy, EGLSurface sur)
{
    StopBootAnim();

    // Should be called when composition rendering is complete for a frame.
    // Only HWC v1.0 needs this call.
    // HWC > v1.0 case, do not call compositionComplete().
    // mFBDevice is present only when HWC is v1.0.
    if (mFBDevice && mFBDevice->compositionComplete) {
        mFBDevice->compositionComplete(mFBDevice);
    }
    return Post(mDispSurface->lastHandle, mDispSurface->GetPrevDispAcquireFd());
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
    const hwc_rect_t r = { 0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight) };
    displays[HWC_DISPLAY_PRIMARY] = mList;
    mList->retireFenceFd = -1;
    mList->numHwLayers = 2;
    mList->flags = HWC_GEOMETRY_CHANGED;
#if ANDROID_VERSION >= 18
    mList->outbuf = nullptr;
    mList->outbufAcquireFenceFd = -1;
#endif
    mList->hwLayers[0].compositionType = HWC_FRAMEBUFFER;
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
    mList->hwLayers[1].blending = HWC_BLENDING_NONE;
#if ANDROID_VERSION >= 19
    if (mHwc->common.version >= HWC_DEVICE_API_VERSION_1_3) {
        mList->hwLayers[1].sourceCropf.left = 0;
        mList->hwLayers[1].sourceCropf.top = 0;
        mList->hwLayers[1].sourceCropf.right = mWidth;
        mList->hwLayers[1].sourceCropf.bottom = mHeight;
    } else {
        mList->hwLayers[1].sourceCrop = r;
    }
#else
    mList->hwLayers[1].sourceCrop = r;
#endif
    mList->hwLayers[1].displayFrame = r;
    mList->hwLayers[1].visibleRegionScreen.numRects = 1;
    mList->hwLayers[1].visibleRegionScreen.rects = &mList->hwLayers[1].displayFrame;
    mList->hwLayers[1].acquireFenceFd = fence;
    mList->hwLayers[1].releaseFenceFd = -1;
#if ANDROID_VERSION >= 18
    mList->hwLayers[1].planeAlpha = 0xFF;
#endif
    mHwc->prepare(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
    int err = mHwc->set(mHwc, HWC_NUM_DISPLAY_TYPES, displays);
    mDispSurface->setReleaseFenceFd(mList->hwLayers[1].releaseFenceFd);
    if (mList->retireFenceFd >= 0)
        close(mList->retireFenceFd);
    return !err;
}

ANativeWindowBuffer*
GonkDisplayJB::DequeueBuffer()
{
    if (mBootAnimBuffer.get()) {
        return static_cast<ANativeWindowBuffer*>(mBootAnimBuffer.get());
    }
    ANativeWindowBuffer *buf;
    mSTClient->dequeueBuffer(mSTClient.get(), &buf, &mFence);
    return buf;
}

bool
GonkDisplayJB::QueueBuffer(ANativeWindowBuffer* buf)
{
    bool success = Post(buf->handle, -1);
    int error = 0;
    if (!mBootAnimBuffer.get()) {
        error = mSTClient->queueBuffer(mSTClient.get(), buf, mFence);
    }
    return error == 0 && success;
}

void
GonkDisplayJB::UpdateDispSurface(EGLDisplay dpy, EGLSurface sur)
{
    StopBootAnim();

    eglSwapBuffers(dpy, sur);
}

void
GonkDisplayJB::SetDispReleaseFd(int fd)
{
    mDispSurface->setReleaseFenceFd(fd);
}

int
GonkDisplayJB::GetPrevDispAcquireFd()
{
    return mDispSurface->GetPrevDispAcquireFd();
}

void
GonkDisplayJB::StopBootAnim()
{
    StopBootAnimation();
    if (mBootAnimBuffer.get()) {
        mBootAnimBuffer = nullptr;
    }
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
