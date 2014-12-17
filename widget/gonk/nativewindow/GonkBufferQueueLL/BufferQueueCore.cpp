/*
 * Copyright 2014 The Android Open Source Project
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

#define LOG_TAG "BufferQueueCore"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define EGL_EGLEXT_PROTOTYPES

#include <inttypes.h>

#include <gui/BufferItem.h>
#include <gui/BufferQueueCore.h>
#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/IProducerListener.h>
#include <gui/ISurfaceComposer.h>
#include <private/gui/ComposerService.h>

template <typename T>
static inline T max(T a, T b) { return a > b ? a : b; }

namespace android {

static String8 getUniqueName() {
    static volatile int32_t counter = 0;
    return String8::format("unnamed-%d-%d", getpid(),
            android_atomic_inc(&counter));
}

BufferQueueCore::BufferQueueCore(const sp<IGraphicBufferAlloc>& allocator) :
    mAllocator(allocator),
    mMutex(),
    mIsAbandoned(false),
    mConsumerControlledByApp(false),
    mConsumerName(getUniqueName()),
    mConsumerListener(),
    mConsumerUsageBits(0),
    mConnectedApi(NO_CONNECTED_API),
    mConnectedProducerListener(),
    mSlots(),
    mQueue(),
    mOverrideMaxBufferCount(0),
    mDequeueCondition(),
    mUseAsyncBuffer(true),
    mDequeueBufferCannotBlock(false),
    mDefaultBufferFormat(PIXEL_FORMAT_RGBA_8888),
    mDefaultWidth(1),
    mDefaultHeight(1),
    mDefaultMaxBufferCount(2),
    mMaxAcquiredBufferCount(1),
    mBufferHasBeenQueued(false),
    mFrameCounter(0),
    mTransformHint(0),
    mIsAllocating(false),
    mIsAllocatingCondition()
{
    if (allocator == NULL) {
        sp<ISurfaceComposer> composer(ComposerService::getComposerService());
        mAllocator = composer->createGraphicBufferAlloc();
        if (mAllocator == NULL) {
            BQ_LOGE("createGraphicBufferAlloc failed");
        }
    }
}

BufferQueueCore::~BufferQueueCore() {}

void BufferQueueCore::dump(String8& result, const char* prefix) const {
    Mutex::Autolock lock(mMutex);

    String8 fifo;
    Fifo::const_iterator current(mQueue.begin());
    while (current != mQueue.end()) {
        fifo.appendFormat("%02d:%p crop=[%d,%d,%d,%d], "
                "xform=0x%02x, time=%#" PRIx64 ", scale=%s\n",
                current->mSlot, current->mGraphicBuffer.get(),
                current->mCrop.left, current->mCrop.top, current->mCrop.right,
                current->mCrop.bottom, current->mTransform, current->mTimestamp,
                BufferItem::scalingModeName(current->mScalingMode));
        ++current;
    }

    result.appendFormat("%s-BufferQueue mMaxAcquiredBufferCount=%d, "
            "mDequeueBufferCannotBlock=%d, default-size=[%dx%d], "
            "default-format=%d, transform-hint=%02x, FIFO(%zu)={%s}\n",
            prefix, mMaxAcquiredBufferCount, mDequeueBufferCannotBlock,
            mDefaultWidth, mDefaultHeight, mDefaultBufferFormat, mTransformHint,
            mQueue.size(), fifo.string());

    // Trim the free buffers so as to not spam the dump
    int maxBufferCount = 0;
    for (int s = BufferQueueDefs::NUM_BUFFER_SLOTS - 1; s >= 0; --s) {
        const BufferSlot& slot(mSlots[s]);
        if (slot.mBufferState != BufferSlot::FREE ||
                slot.mGraphicBuffer != NULL) {
            maxBufferCount = s + 1;
            break;
        }
    }

    for (int s = 0; s < maxBufferCount; ++s) {
        const BufferSlot& slot(mSlots[s]);
        const sp<GraphicBuffer>& buffer(slot.mGraphicBuffer);
        result.appendFormat("%s%s[%02d:%p] state=%-8s", prefix,
                (slot.mBufferState == BufferSlot::ACQUIRED) ? ">" : " ",
                s, buffer.get(),
                BufferSlot::bufferStateName(slot.mBufferState));

        if (buffer != NULL) {
            result.appendFormat(", %p [%4ux%4u:%4u,%3X]", buffer->handle,
                    buffer->width, buffer->height, buffer->stride,
                    buffer->format);
        }

        result.append("\n");
    }
}

int BufferQueueCore::getMinUndequeuedBufferCountLocked(bool async) const {
    // If dequeueBuffer is allowed to error out, we don't have to add an
    // extra buffer.
    if (!mUseAsyncBuffer) {
        return mMaxAcquiredBufferCount;
    }

    if (mDequeueBufferCannotBlock || async) {
        return mMaxAcquiredBufferCount + 1;
    }

    return mMaxAcquiredBufferCount;
}

int BufferQueueCore::getMinMaxBufferCountLocked(bool async) const {
    return getMinUndequeuedBufferCountLocked(async) + 1;
}

int BufferQueueCore::getMaxBufferCountLocked(bool async) const {
    int minMaxBufferCount = getMinMaxBufferCountLocked(async);

    int maxBufferCount = max(mDefaultMaxBufferCount, minMaxBufferCount);
    if (mOverrideMaxBufferCount != 0) {
        assert(mOverrideMaxBufferCount >= minMaxBufferCount);
        maxBufferCount = mOverrideMaxBufferCount;
    }

    // Any buffers that are dequeued by the producer or sitting in the queue
    // waiting to be consumed need to have their slots preserved. Such buffers
    // will temporarily keep the max buffer count up until the slots no longer
    // need to be preserved.
    for (int s = maxBufferCount; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        BufferSlot::BufferState state = mSlots[s].mBufferState;
        if (state == BufferSlot::QUEUED || state == BufferSlot::DEQUEUED) {
            maxBufferCount = s + 1;
        }
    }

    return maxBufferCount;
}

status_t BufferQueueCore::setDefaultMaxBufferCountLocked(int count) {
    const int minBufferCount = mUseAsyncBuffer ? 2 : 1;
    if (count < minBufferCount || count > BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGV("setDefaultMaxBufferCount: invalid count %d, should be in "
                "[%d, %d]",
                count, minBufferCount, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    }

    BQ_LOGV("setDefaultMaxBufferCount: setting count to %d", count);
    mDefaultMaxBufferCount = count;
    mDequeueCondition.broadcast();

    return NO_ERROR;
}

void BufferQueueCore::freeBufferLocked(int slot) {
    BQ_LOGV("freeBufferLocked: slot %d", slot);
    mSlots[slot].mGraphicBuffer.clear();
    if (mSlots[slot].mBufferState == BufferSlot::ACQUIRED) {
        mSlots[slot].mNeedsCleanupOnRelease = true;
    }
    mSlots[slot].mBufferState = BufferSlot::FREE;
    mSlots[slot].mFrameNumber = UINT32_MAX;
    mSlots[slot].mAcquireCalled = false;

    // Destroy fence as BufferQueue now takes ownership
    if (mSlots[slot].mEglFence != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(mSlots[slot].mEglDisplay, mSlots[slot].mEglFence);
        mSlots[slot].mEglFence = EGL_NO_SYNC_KHR;
    }
    mSlots[slot].mFence = Fence::NO_FENCE;
}

void BufferQueueCore::freeAllBuffersLocked() {
    mBufferHasBeenQueued = false;
    for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        freeBufferLocked(s);
    }
}

bool BufferQueueCore::stillTracking(const BufferItem* item) const {
    const BufferSlot& slot = mSlots[item->mSlot];

    BQ_LOGV("stillTracking: item { slot=%d/%" PRIu64 " buffer=%p } "
            "slot { slot=%d/%" PRIu64 " buffer=%p }",
            item->mSlot, item->mFrameNumber,
            (item->mGraphicBuffer.get() ? item->mGraphicBuffer->handle : 0),
            item->mSlot, slot.mFrameNumber,
            (slot.mGraphicBuffer.get() ? slot.mGraphicBuffer->handle : 0));

    // Compare item with its original buffer slot. We can check the slot as
    // the buffer would not be moved to a different slot by the producer.
    return (slot.mGraphicBuffer != NULL) &&
           (item->mGraphicBuffer->handle == slot.mGraphicBuffer->handle);
}

void BufferQueueCore::waitWhileAllocatingLocked() const {
    ATRACE_CALL();
    while (mIsAllocating) {
        mIsAllocatingCondition.wait(mMutex);
    }
}

} // namespace android
