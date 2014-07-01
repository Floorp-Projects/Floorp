/*
 **
 ** Copyright 2012 The Android Open Source Project
 **
 ** Licensed under the Apache License Version 2.0(the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing software
 ** distributed under the License is distributed on an "AS IS" BASIS
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <cutils/log.h>

#include <utils/String8.h>

#include <ui/Rect.h>

#include <EGL/egl.h>

#include <hardware/hardware.h>
#if ANDROID_VERSION == 17
#include <gui/SurfaceTextureClient.h>
#endif
#include <ui/GraphicBuffer.h>

#include "FramebufferSurface.h"
#include "GraphicBufferAlloc.h"

#ifndef NUM_FRAMEBUFFER_SURFACE_BUFFERS
#define NUM_FRAMEBUFFER_SURFACE_BUFFERS (2)
#endif

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

/*
 * This implements the (main) framebuffer management. This class
 * was adapted from the version in SurfaceFlinger
 */
FramebufferSurface::FramebufferSurface(int disp, uint32_t width, uint32_t height, uint32_t format,
        sp<BufferQueue>& bq) :
#if ANDROID_VERSION >= 19
    ConsumerBase(bq, true),
#else
    ConsumerBase(bq),
#endif
    mDisplayType(disp),
    mCurrentBufferSlot(-1),
    mCurrentBuffer(0),
    lastHandle(0)
{
    mName = "FramebufferSurface";

#if ANDROID_VERSION >= 19
    sp<IGraphicBufferConsumer> consumer = mConsumer;
#else
    sp<BufferQueue> consumer = mBufferQueue;
    consumer->setSynchronousMode(true);
#endif
    consumer->setConsumerName(mName);
    consumer->setConsumerUsageBits(GRALLOC_USAGE_HW_FB |
                                   GRALLOC_USAGE_HW_RENDER |
                                   GRALLOC_USAGE_HW_COMPOSER);
    consumer->setDefaultBufferFormat(format);
    consumer->setDefaultBufferSize(width, height);
    consumer->setDefaultMaxBufferCount(NUM_FRAMEBUFFER_SURFACE_BUFFERS);
}

status_t FramebufferSurface::nextBuffer(sp<GraphicBuffer>& outBuffer, sp<Fence>& outFence) {
    Mutex::Autolock lock(mMutex);

    BufferQueue::BufferItem item;
#if ANDROID_VERSION >= 19
    status_t err = acquireBufferLocked(&item, 0);
#else
    status_t err = acquireBufferLocked(&item);
#endif
    if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
        outBuffer = mCurrentBuffer;
        return NO_ERROR;
    } else if (err != NO_ERROR) {
        ALOGE("error acquiring buffer: %s (%d)", strerror(-err), err);
        return err;
    }

    // If the BufferQueue has freed and reallocated a buffer in mCurrentSlot
    // then we may have acquired the slot we already own.  If we had released
    // our current buffer before we call acquireBuffer then that release call
    // would have returned STALE_BUFFER_SLOT, and we would have called
    // freeBufferLocked on that slot.  Because the buffer slot has already
    // been overwritten with the new buffer all we have to do is skip the
    // releaseBuffer call and we should be in the same state we'd be in if we
    // had released the old buffer first.
    if (mCurrentBufferSlot != BufferQueue::INVALID_BUFFER_SLOT &&
        item.mBuf != mCurrentBufferSlot) {
        // Release the previous buffer.
#if ANDROID_VERSION >= 19
        err = releaseBufferLocked(mCurrentBufferSlot, mCurrentBuffer,
                EGL_NO_DISPLAY, EGL_NO_SYNC_KHR);
#else
        err = releaseBufferLocked(mCurrentBufferSlot, EGL_NO_DISPLAY,
                EGL_NO_SYNC_KHR);
#endif
        if (err != NO_ERROR && err != BufferQueue::STALE_BUFFER_SLOT) {
            ALOGE("error releasing buffer: %s (%d)", strerror(-err), err);
            return err;
        }
    }
    mCurrentBufferSlot = item.mBuf;
    mCurrentBuffer = mSlots[mCurrentBufferSlot].mGraphicBuffer;
    outFence = item.mFence;
    outBuffer = mCurrentBuffer;
    return NO_ERROR;
}

// Overrides ConsumerBase::onFrameAvailable(), does not call base class impl.
void FramebufferSurface::onFrameAvailable() {
    sp<GraphicBuffer> buf;
    sp<Fence> acquireFence;
    status_t err = nextBuffer(buf, acquireFence);
    if (err != NO_ERROR) {
        ALOGE("error latching nnext FramebufferSurface buffer: %s (%d)",
                strerror(-err), err);
        return;
    }
    if (acquireFence.get() && acquireFence->isValid())
        mPrevFBAcquireFence = new Fence(acquireFence->dup());
    else
        mPrevFBAcquireFence = Fence::NO_FENCE;

    lastHandle = buf->handle;
}

void FramebufferSurface::freeBufferLocked(int slotIndex) {
    ConsumerBase::freeBufferLocked(slotIndex);
    if (slotIndex == mCurrentBufferSlot) {
        mCurrentBufferSlot = BufferQueue::INVALID_BUFFER_SLOT;
    }
}

status_t FramebufferSurface::setReleaseFenceFd(int fenceFd) {
    status_t err = NO_ERROR;
    if (fenceFd >= 0) {
        sp<Fence> fence(new Fence(fenceFd));
        if (mCurrentBufferSlot != BufferQueue::INVALID_BUFFER_SLOT) {
#if ANDROID_VERSION >= 19
            status_t err = addReleaseFence(mCurrentBufferSlot, mCurrentBuffer,  fence);
#else
            status_t err = addReleaseFence(mCurrentBufferSlot, fence);
#endif
            ALOGE_IF(err, "setReleaseFenceFd: failed to add the fence: %s (%d)",
                    strerror(-err), err);
        }
    }
    return err;
}

int FramebufferSurface::GetPrevFBAcquireFd() {
  return mPrevFBAcquireFence.get() ? mPrevFBAcquireFence->dup() : -1;
}

status_t FramebufferSurface::setUpdateRectangle(const Rect& r)
{
    return INVALID_OPERATION;
}

status_t FramebufferSurface::compositionComplete()
{
    return NO_ERROR;
}

void FramebufferSurface::dump(String8& result) {
    ConsumerBase::dump(result);
}

void FramebufferSurface::dump(String8& result, const char* prefix) {
    ConsumerBase::dump(result);
}

// ----------------------------------------------------------------------------
}; // namespace android
// ----------------------------------------------------------------------------
