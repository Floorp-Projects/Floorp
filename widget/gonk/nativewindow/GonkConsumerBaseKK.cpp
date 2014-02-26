/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2013 Mozilla Foundation
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

#define LOG_TAG "GonkConsumerBase"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define EGL_EGLEXT_PROTOTYPES

#include <hardware/hardware.h>

#include <gui/IGraphicBufferAlloc.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include "GonkConsumerBaseKK.h"

// Macros for including the GonkConsumerBase name in log messages
#define CB_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define CB_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define CB_LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define CB_LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define CB_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace android {

// Get an ID that's unique within this process.
static int32_t createProcessUniqueId() {
    static volatile int32_t globalCounter = 0;
    return android_atomic_inc(&globalCounter);
}

GonkConsumerBase::GonkConsumerBase(const sp<GonkBufferQueue>& bufferQueue, bool controlledByApp) :
        mAbandoned(false),
        mConsumer(bufferQueue) {
    // Choose a name using the PID and a process-unique ID.
    mName = String8::format("unnamed-%d-%d", getpid(), createProcessUniqueId());

    // Note that we can't create an sp<...>(this) in a ctor that will not keep a
    // reference once the ctor ends, as that would cause the refcount of 'this'
    // dropping to 0 at the end of the ctor.  Since all we need is a wp<...>
    // that's what we create.
    wp<ConsumerListener> listener = static_cast<ConsumerListener*>(this);
    sp<IConsumerListener> proxy = new GonkBufferQueue::ProxyConsumerListener(listener);

    status_t err = mConsumer->consumerConnect(proxy, controlledByApp);
    if (err != NO_ERROR) {
        CB_LOGE("GonkConsumerBase: error connecting to GonkBufferQueue: %s (%d)",
                strerror(-err), err);
    } else {
        mConsumer->setConsumerName(mName);
    }
}

GonkConsumerBase::~GonkConsumerBase() {
    CB_LOGV("~GonkConsumerBase");
    Mutex::Autolock lock(mMutex);

    // Verify that abandon() has been called before we get here.  This should
    // be done by GonkConsumerBase::onLastStrongRef(), but it's possible for a
    // derived class to override that method and not call
    // GonkConsumerBase::onLastStrongRef().
    LOG_ALWAYS_FATAL_IF(!mAbandoned, "[%s] ~GonkConsumerBase was called, but the "
        "consumer is not abandoned!", mName.string());
}

void GonkConsumerBase::onLastStrongRef(const void* id) {
    abandon();
}

void GonkConsumerBase::freeBufferLocked(int slotIndex) {
    CB_LOGV("freeBufferLocked: slotIndex=%d", slotIndex);
    mSlots[slotIndex].mGraphicBuffer = 0;
    mSlots[slotIndex].mFence = Fence::NO_FENCE;
    mSlots[slotIndex].mFrameNumber = 0;
}

// Used for refactoring, should not be in final interface
sp<GonkBufferQueue> GonkConsumerBase::getBufferQueue() const {
    Mutex::Autolock lock(mMutex);
    return mConsumer;
}

void GonkConsumerBase::onFrameAvailable() {
    CB_LOGV("onFrameAvailable");

    sp<FrameAvailableListener> listener;
    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        listener = mFrameAvailableListener.promote();
    }

    if (listener != NULL) {
        CB_LOGV("actually calling onFrameAvailable");
        listener->onFrameAvailable();
    }
}

void GonkConsumerBase::onBuffersReleased() {
    Mutex::Autolock lock(mMutex);

    CB_LOGV("onBuffersReleased");

    if (mAbandoned) {
        // Nothing to do if we're already abandoned.
        return;
    }

    uint32_t mask = 0;
    mConsumer->getReleasedBuffers(&mask);
    for (int i = 0; i < GonkBufferQueue::NUM_BUFFER_SLOTS; i++) {
        if (mask & (1 << i)) {
            freeBufferLocked(i);
        }
    }
}

void GonkConsumerBase::abandon() {
    CB_LOGV("abandon");
    Mutex::Autolock lock(mMutex);

    if (!mAbandoned) {
        abandonLocked();
        mAbandoned = true;
    }
}

void GonkConsumerBase::abandonLocked() {
	CB_LOGV("abandonLocked");
    for (int i =0; i < GonkBufferQueue::NUM_BUFFER_SLOTS; i++) {
        freeBufferLocked(i);
    }
    // disconnect from the BufferQueue
    mConsumer->consumerDisconnect();
    mConsumer.clear();
}

void GonkConsumerBase::setFrameAvailableListener(
        const wp<FrameAvailableListener>& listener) {
    CB_LOGV("setFrameAvailableListener");
    Mutex::Autolock lock(mMutex);
    mFrameAvailableListener = listener;
}

void GonkConsumerBase::dump(String8& result) const {
    dump(result, "");
}

void GonkConsumerBase::dump(String8& result, const char* prefix) const {
    Mutex::Autolock _l(mMutex);
    dumpLocked(result, prefix);
}

void GonkConsumerBase::dumpLocked(String8& result, const char* prefix) const {
    result.appendFormat("%smAbandoned=%d\n", prefix, int(mAbandoned));

    if (!mAbandoned) {
        mConsumer->dump(result, prefix);
    }
}

status_t GonkConsumerBase::acquireBufferLocked(IGonkGraphicBufferConsumer::BufferItem *item,
        nsecs_t presentWhen) {
    status_t err = mConsumer->acquireBuffer(item, presentWhen);
    if (err != NO_ERROR) {
        return err;
    }

    if (item->mGraphicBuffer != NULL) {
        mSlots[item->mBuf].mGraphicBuffer = item->mGraphicBuffer;
    }

    mSlots[item->mBuf].mFrameNumber = item->mFrameNumber;
    mSlots[item->mBuf].mFence = item->mFence;

    CB_LOGV("acquireBufferLocked: -> slot=%d", item->mBuf);

    return OK;
}

status_t GonkConsumerBase::addReleaseFence(int slot,
        const sp<GraphicBuffer> graphicBuffer, const sp<Fence>& fence) {
    Mutex::Autolock lock(mMutex);
    return addReleaseFenceLocked(slot, graphicBuffer, fence);
}

status_t GonkConsumerBase::addReleaseFenceLocked(int slot,
        const sp<GraphicBuffer> graphicBuffer, const sp<Fence>& fence) {
    CB_LOGV("addReleaseFenceLocked: slot=%d", slot);

    // If consumer no longer tracks this graphicBuffer, we can safely
    // drop this fence, as it will never be received by the producer.
    if (!stillTracking(slot, graphicBuffer)) {
        return OK;
    }

    if (!mSlots[slot].mFence.get()) {
        mSlots[slot].mFence = fence;
    } else {
        sp<Fence> mergedFence = Fence::merge(
                String8::format("%.28s:%d", mName.string(), slot),
                mSlots[slot].mFence, fence);
        if (!mergedFence.get()) {
            CB_LOGE("failed to merge release fences");
            // synchronization is broken, the best we can do is hope fences
            // signal in order so the new fence will act like a union
            mSlots[slot].mFence = fence;
            return BAD_VALUE;
        }
        mSlots[slot].mFence = mergedFence;
    }

    return OK;
}

status_t GonkConsumerBase::releaseBufferLocked(int slot, const sp<GraphicBuffer> graphicBuffer) {
    // If consumer no longer tracks this graphicBuffer (we received a new
    // buffer on the same slot), the buffer producer is definitely no longer
    // tracking it.
    if (!stillTracking(slot, graphicBuffer)) {
        return OK;
    }

    CB_LOGV("releaseBufferLocked: slot=%d/%llu",
            slot, mSlots[slot].mFrameNumber);
    status_t err = mConsumer->releaseBuffer(slot, mSlots[slot].mFrameNumber, mSlots[slot].mFence);
    if (err == GonkBufferQueue::STALE_BUFFER_SLOT) {
        freeBufferLocked(slot);
    }

    mSlots[slot].mFence = Fence::NO_FENCE;

    return err;
}

bool GonkConsumerBase::stillTracking(int slot,
        const sp<GraphicBuffer> graphicBuffer) {
    if (slot < 0 || slot >= GonkBufferQueue::NUM_BUFFER_SLOTS) {
        return false;
    }
    return (mSlots[slot].mGraphicBuffer != NULL &&
            mSlots[slot].mGraphicBuffer->handle == graphicBuffer->handle);
}

} // namespace android
