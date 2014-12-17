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

#include <inttypes.h>

#define LOG_TAG "BufferQueueProducer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define EGL_EGLEXT_PROTOTYPES

#include <gui/BufferItem.h>
#include <gui/BufferQueueCore.h>
#include <gui/BufferQueueProducer.h>
#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/IProducerListener.h>

#include <utils/Log.h>
#include <utils/Trace.h>

namespace android {

BufferQueueProducer::BufferQueueProducer(const sp<BufferQueueCore>& core) :
    mCore(core),
    mSlots(core->mSlots),
    mConsumerName(),
    mStickyTransform(0) {}

BufferQueueProducer::~BufferQueueProducer() {}

status_t BufferQueueProducer::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ATRACE_CALL();
    BQ_LOGV("requestBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("requestBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGE("requestBuffer: slot index %d out of range [0, %d)",
                slot, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
        BQ_LOGE("requestBuffer: slot %d is not owned by the producer "
                "(state = %d)", slot, mSlots[slot].mBufferState);
        return BAD_VALUE;
    }

    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}

status_t BufferQueueProducer::setBufferCount(int bufferCount) {
    ATRACE_CALL();
    BQ_LOGV("setBufferCount: count = %d", bufferCount);

    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (mCore->mIsAbandoned) {
            BQ_LOGE("setBufferCount: BufferQueue has been abandoned");
            return NO_INIT;
        }

        if (bufferCount > BufferQueueDefs::NUM_BUFFER_SLOTS) {
            BQ_LOGE("setBufferCount: bufferCount %d too large (max %d)",
                    bufferCount, BufferQueueDefs::NUM_BUFFER_SLOTS);
            return BAD_VALUE;
        }

        // There must be no dequeued buffers when changing the buffer count.
        for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            if (mSlots[s].mBufferState == BufferSlot::DEQUEUED) {
                BQ_LOGE("setBufferCount: buffer owned by producer");
                return BAD_VALUE;
            }
        }

        if (bufferCount == 0) {
            mCore->mOverrideMaxBufferCount = 0;
            mCore->mDequeueCondition.broadcast();
            return NO_ERROR;
        }

        const int minBufferSlots = mCore->getMinMaxBufferCountLocked(false);
        if (bufferCount < minBufferSlots) {
            BQ_LOGE("setBufferCount: requested buffer count %d is less than "
                    "minimum %d", bufferCount, minBufferSlots);
            return BAD_VALUE;
        }

        // Here we are guaranteed that the producer doesn't have any dequeued
        // buffers and will release all of its buffer references. We don't
        // clear the queue, however, so that currently queued buffers still
        // get displayed.
        mCore->freeAllBuffersLocked();
        mCore->mOverrideMaxBufferCount = bufferCount;
        mCore->mDequeueCondition.broadcast();
        listener = mCore->mConsumerListener;
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return NO_ERROR;
}

status_t BufferQueueProducer::waitForFreeSlotThenRelock(const char* caller,
        bool async, int* found, status_t* returnFlags) const {
    bool tryAgain = true;
    while (tryAgain) {
        if (mCore->mIsAbandoned) {
            BQ_LOGE("%s: BufferQueue has been abandoned", caller);
            return NO_INIT;
        }

        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
        if (async && mCore->mOverrideMaxBufferCount) {
            // FIXME: Some drivers are manually setting the buffer count
            // (which they shouldn't), so we do this extra test here to
            // handle that case. This is TEMPORARY until we get this fixed.
            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
                BQ_LOGE("%s: async mode is invalid with buffer count override",
                        caller);
                return BAD_VALUE;
            }
        }

        // Free up any buffers that are in slots beyond the max buffer count
        for (int s = maxBufferCount; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            assert(mSlots[s].mBufferState == BufferSlot::FREE);
            if (mSlots[s].mGraphicBuffer != NULL) {
                mCore->freeBufferLocked(s);
                *returnFlags |= RELEASE_ALL_BUFFERS;
            }
        }

        // Look for a free buffer to give to the client
        *found = BufferQueueCore::INVALID_BUFFER_SLOT;
        int dequeuedCount = 0;
        int acquiredCount = 0;
        for (int s = 0; s < maxBufferCount; ++s) {
            switch (mSlots[s].mBufferState) {
                case BufferSlot::DEQUEUED:
                    ++dequeuedCount;
                    break;
                case BufferSlot::ACQUIRED:
                    ++acquiredCount;
                    break;
                case BufferSlot::FREE:
                    // We return the oldest of the free buffers to avoid
                    // stalling the producer if possible, since the consumer
                    // may still have pending reads of in-flight buffers
                    if (*found == BufferQueueCore::INVALID_BUFFER_SLOT ||
                            mSlots[s].mFrameNumber < mSlots[*found].mFrameNumber) {
                        *found = s;
                    }
                    break;
                default:
                    break;
            }
        }

        // Producers are not allowed to dequeue more than one buffer if they
        // did not set a buffer count
        if (!mCore->mOverrideMaxBufferCount && dequeuedCount) {
            BQ_LOGE("%s: can't dequeue multiple buffers without setting the "
                    "buffer count", caller);
            return INVALID_OPERATION;
        }

        // See whether a buffer has been queued since the last
        // setBufferCount so we know whether to perform the min undequeued
        // buffers check below
        if (mCore->mBufferHasBeenQueued) {
            // Make sure the producer is not trying to dequeue more buffers
            // than allowed
            const int newUndequeuedCount =
                maxBufferCount - (dequeuedCount + 1);
            const int minUndequeuedCount =
                mCore->getMinUndequeuedBufferCountLocked(async);
            if (newUndequeuedCount < minUndequeuedCount) {
                BQ_LOGE("%s: min undequeued buffer count (%d) exceeded "
                        "(dequeued=%d undequeued=%d)",
                        caller, minUndequeuedCount,
                        dequeuedCount, newUndequeuedCount);
                return INVALID_OPERATION;
            }
        }

        // If we disconnect and reconnect quickly, we can be in a state where
        // our slots are empty but we have many buffers in the queue. This can
        // cause us to run out of memory if we outrun the consumer. Wait here if
        // it looks like we have too many buffers queued up.
        bool tooManyBuffers = mCore->mQueue.size()
                            > static_cast<size_t>(maxBufferCount);
        if (tooManyBuffers) {
            BQ_LOGV("%s: queue size is %zu, waiting", caller,
                    mCore->mQueue.size());
        }

        // If no buffer is found, or if the queue has too many buffers
        // outstanding, wait for a buffer to be acquired or released, or for the
        // max buffer count to change.
        tryAgain = (*found == BufferQueueCore::INVALID_BUFFER_SLOT) ||
                   tooManyBuffers;
        if (tryAgain) {
            // Return an error if we're in non-blocking mode (producer and
            // consumer are controlled by the application).
            // However, the consumer is allowed to briefly acquire an extra
            // buffer (which could cause us to have to wait here), which is
            // okay, since it is only used to implement an atomic acquire +
            // release (e.g., in GLConsumer::updateTexImage())
            if (mCore->mDequeueBufferCannotBlock &&
                    (acquiredCount <= mCore->mMaxAcquiredBufferCount)) {
                return WOULD_BLOCK;
            }
            mCore->mDequeueCondition.wait(mCore->mMutex);
        }
    } // while (tryAgain)

    return NO_ERROR;
}

status_t BufferQueueProducer::dequeueBuffer(int *outSlot,
        sp<android::Fence> *outFence, bool async,
        uint32_t width, uint32_t height, uint32_t format, uint32_t usage) {
    ATRACE_CALL();
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mConsumerName = mCore->mConsumerName;
    } // Autolock scope

    BQ_LOGV("dequeueBuffer: async=%s w=%u h=%u format=%#x, usage=%#x",
            async ? "true" : "false", width, height, format, usage);

    if ((width && !height) || (!width && height)) {
        BQ_LOGE("dequeueBuffer: invalid size: w=%u h=%u", width, height);
        return BAD_VALUE;
    }

    status_t returnFlags = NO_ERROR;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSyncKHR eglFence = EGL_NO_SYNC_KHR;
    bool attachedByConsumer = false;

    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (format == 0) {
            format = mCore->mDefaultBufferFormat;
        }

        // Enable the usage bits the consumer requested
        usage |= mCore->mConsumerUsageBits;

        int found;
        status_t status = waitForFreeSlotThenRelock("dequeueBuffer", async,
                &found, &returnFlags);
        if (status != NO_ERROR) {
            return status;
        }

        // This should not happen
        if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
            BQ_LOGE("dequeueBuffer: no available buffer slots");
            return -EBUSY;
        }

        *outSlot = found;
        ATRACE_BUFFER_INDEX(found);

        attachedByConsumer = mSlots[found].mAttachedByConsumer;

        const bool useDefaultSize = !width && !height;
        if (useDefaultSize) {
            width = mCore->mDefaultWidth;
            height = mCore->mDefaultHeight;
        }

        mSlots[found].mBufferState = BufferSlot::DEQUEUED;

        const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);
        if ((buffer == NULL) ||
                (static_cast<uint32_t>(buffer->width) != width) ||
                (static_cast<uint32_t>(buffer->height) != height) ||
                (static_cast<uint32_t>(buffer->format) != format) ||
                ((static_cast<uint32_t>(buffer->usage) & usage) != usage))
        {
            mSlots[found].mAcquireCalled = false;
            mSlots[found].mGraphicBuffer = NULL;
            mSlots[found].mRequestBufferCalled = false;
            mSlots[found].mEglDisplay = EGL_NO_DISPLAY;
            mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
            mSlots[found].mFence = Fence::NO_FENCE;

            returnFlags |= BUFFER_NEEDS_REALLOCATION;
        }

        if (CC_UNLIKELY(mSlots[found].mFence == NULL)) {
            BQ_LOGE("dequeueBuffer: about to return a NULL fence - "
                    "slot=%d w=%d h=%d format=%u",
                    found, buffer->width, buffer->height, buffer->format);
        }

        eglDisplay = mSlots[found].mEglDisplay;
        eglFence = mSlots[found].mEglFence;
        *outFence = mSlots[found].mFence;
        mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
        mSlots[found].mFence = Fence::NO_FENCE;
    } // Autolock scope

    if (returnFlags & BUFFER_NEEDS_REALLOCATION) {
        status_t error;
        BQ_LOGV("dequeueBuffer: allocating a new buffer for slot %d", *outSlot);
        sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
                    width, height, format, usage, &error));
        if (graphicBuffer == NULL) {
            BQ_LOGE("dequeueBuffer: createGraphicBuffer failed");
            return error;
        }

        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);

            if (mCore->mIsAbandoned) {
                BQ_LOGE("dequeueBuffer: BufferQueue has been abandoned");
                return NO_INIT;
            }

            mSlots[*outSlot].mFrameNumber = UINT32_MAX;
            mSlots[*outSlot].mGraphicBuffer = graphicBuffer;
        } // Autolock scope
    }

    if (attachedByConsumer) {
        returnFlags |= BUFFER_NEEDS_REALLOCATION;
    }

    if (eglFence != EGL_NO_SYNC_KHR) {
        EGLint result = eglClientWaitSyncKHR(eglDisplay, eglFence, 0,
                1000000000);
        // If something goes wrong, log the error, but return the buffer without
        // synchronizing access to it. It's too late at this point to abort the
        // dequeue operation.
        if (result == EGL_FALSE) {
            BQ_LOGE("dequeueBuffer: error %#x waiting for fence",
                    eglGetError());
        } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
            BQ_LOGE("dequeueBuffer: timeout waiting for fence");
        }
        eglDestroySyncKHR(eglDisplay, eglFence);
    }

    BQ_LOGV("dequeueBuffer: returning slot=%d/%" PRIu64 " buf=%p flags=%#x",
            *outSlot,
            mSlots[*outSlot].mFrameNumber,
            mSlots[*outSlot].mGraphicBuffer->handle, returnFlags);

    return returnFlags;
}

status_t BufferQueueProducer::detachBuffer(int slot) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(slot);
    BQ_LOGV("detachBuffer(P): slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("detachBuffer(P): BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGE("detachBuffer(P): slot index %d out of range [0, %d)",
                slot, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
        BQ_LOGE("detachBuffer(P): slot %d is not owned by the producer "
                "(state = %d)", slot, mSlots[slot].mBufferState);
        return BAD_VALUE;
    } else if (!mSlots[slot].mRequestBufferCalled) {
        BQ_LOGE("detachBuffer(P): buffer in slot %d has not been requested",
                slot);
        return BAD_VALUE;
    }

    mCore->freeBufferLocked(slot);
    mCore->mDequeueCondition.broadcast();

    return NO_ERROR;
}

status_t BufferQueueProducer::detachNextBuffer(sp<GraphicBuffer>* outBuffer,
        sp<Fence>* outFence) {
    ATRACE_CALL();

    if (outBuffer == NULL) {
        BQ_LOGE("detachNextBuffer: outBuffer must not be NULL");
        return BAD_VALUE;
    } else if (outFence == NULL) {
        BQ_LOGE("detachNextBuffer: outFence must not be NULL");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mCore->mMutex);
    mCore->waitWhileAllocatingLocked();

    if (mCore->mIsAbandoned) {
        BQ_LOGE("detachNextBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    // Find the oldest valid slot
    int found = BufferQueueCore::INVALID_BUFFER_SLOT;
    for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        if (mSlots[s].mBufferState == BufferSlot::FREE &&
                mSlots[s].mGraphicBuffer != NULL) {
            if (found == BufferQueueCore::INVALID_BUFFER_SLOT ||
                    mSlots[s].mFrameNumber < mSlots[found].mFrameNumber) {
                found = s;
            }
        }
    }

    if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
        return NO_MEMORY;
    }

    BQ_LOGV("detachNextBuffer detached slot %d", found);

    *outBuffer = mSlots[found].mGraphicBuffer;
    *outFence = mSlots[found].mFence;
    mCore->freeBufferLocked(found);

    return NO_ERROR;
}

status_t BufferQueueProducer::attachBuffer(int* outSlot,
        const sp<android::GraphicBuffer>& buffer) {
    ATRACE_CALL();

    if (outSlot == NULL) {
        BQ_LOGE("attachBuffer(P): outSlot must not be NULL");
        return BAD_VALUE;
    } else if (buffer == NULL) {
        BQ_LOGE("attachBuffer(P): cannot attach NULL buffer");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mCore->mMutex);
    mCore->waitWhileAllocatingLocked();

    status_t returnFlags = NO_ERROR;
    int found;
    // TODO: Should we provide an async flag to attachBuffer? It seems
    // unlikely that buffers which we are attaching to a BufferQueue will
    // be asynchronous (droppable), but it may not be impossible.
    status_t status = waitForFreeSlotThenRelock("attachBuffer(P)", false,
            &found, &returnFlags);
    if (status != NO_ERROR) {
        return status;
    }

    // This should not happen
    if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
        BQ_LOGE("attachBuffer(P): no available buffer slots");
        return -EBUSY;
    }

    *outSlot = found;
    ATRACE_BUFFER_INDEX(*outSlot);
    BQ_LOGV("attachBuffer(P): returning slot %d flags=%#x",
            *outSlot, returnFlags);

    mSlots[*outSlot].mGraphicBuffer = buffer;
    mSlots[*outSlot].mBufferState = BufferSlot::DEQUEUED;
    mSlots[*outSlot].mEglFence = EGL_NO_SYNC_KHR;
    mSlots[*outSlot].mFence = Fence::NO_FENCE;
    mSlots[*outSlot].mRequestBufferCalled = true;

    return returnFlags;
}

status_t BufferQueueProducer::queueBuffer(int slot,
        const QueueBufferInput &input, QueueBufferOutput *output) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(slot);

    int64_t timestamp;
    bool isAutoTimestamp;
    Rect crop;
    int scalingMode;
    uint32_t transform;
    uint32_t stickyTransform;
    bool async;
    sp<Fence> fence;
    input.deflate(&timestamp, &isAutoTimestamp, &crop, &scalingMode, &transform,
            &async, &fence, &stickyTransform);

    if (fence == NULL) {
        BQ_LOGE("queueBuffer: fence is NULL");
        // Temporary workaround for b/17946343: soldier-on instead of returning an error. This
        // prevents the client from dying, at the risk of visible corruption due to hwcomposer
        // reading the buffer before the producer is done rendering it. Unless the buffer is the
        // last frame of an animation, the corruption will be transient.
        fence = Fence::NO_FENCE;
        // return BAD_VALUE;
    }

    switch (scalingMode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
        case NATIVE_WINDOW_SCALING_MODE_NO_SCALE_CROP:
            break;
        default:
            BQ_LOGE("queueBuffer: unknown scaling mode %d", scalingMode);
            return BAD_VALUE;
    }

    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);

        if (mCore->mIsAbandoned) {
            BQ_LOGE("queueBuffer: BufferQueue has been abandoned");
            return NO_INIT;
        }

        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
        if (async && mCore->mOverrideMaxBufferCount) {
            // FIXME: Some drivers are manually setting the buffer count
            // (which they shouldn't), so we do this extra test here to
            // handle that case. This is TEMPORARY until we get this fixed.
            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
                BQ_LOGE("queueBuffer: async mode is invalid with "
                        "buffer count override");
                return BAD_VALUE;
            }
        }

        if (slot < 0 || slot >= maxBufferCount) {
            BQ_LOGE("queueBuffer: slot index %d out of range [0, %d)",
                    slot, maxBufferCount);
            return BAD_VALUE;
        } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
            BQ_LOGE("queueBuffer: slot %d is not owned by the producer "
                    "(state = %d)", slot, mSlots[slot].mBufferState);
            return BAD_VALUE;
        } else if (!mSlots[slot].mRequestBufferCalled) {
            BQ_LOGE("queueBuffer: slot %d was queued without requesting "
                    "a buffer", slot);
            return BAD_VALUE;
        }

        BQ_LOGV("queueBuffer: slot=%d/%" PRIu64 " time=%" PRIu64
                " crop=[%d,%d,%d,%d] transform=%#x scale=%s",
                slot, mCore->mFrameCounter + 1, timestamp,
                crop.left, crop.top, crop.right, crop.bottom,
                transform, BufferItem::scalingModeName(scalingMode));

        const sp<GraphicBuffer>& graphicBuffer(mSlots[slot].mGraphicBuffer);
        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
        Rect croppedRect;
        crop.intersect(bufferRect, &croppedRect);
        if (croppedRect != crop) {
            BQ_LOGE("queueBuffer: crop rect is not contained within the "
                    "buffer in slot %d", slot);
            return BAD_VALUE;
        }

        mSlots[slot].mFence = fence;
        mSlots[slot].mBufferState = BufferSlot::QUEUED;
        ++mCore->mFrameCounter;
        mSlots[slot].mFrameNumber = mCore->mFrameCounter;

        BufferItem item;
        item.mAcquireCalled = mSlots[slot].mAcquireCalled;
        item.mGraphicBuffer = mSlots[slot].mGraphicBuffer;
        item.mCrop = crop;
        item.mTransform = transform & ~NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY;
        item.mTransformToDisplayInverse =
                bool(transform & NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY);
        item.mScalingMode = scalingMode;
        item.mTimestamp = timestamp;
        item.mIsAutoTimestamp = isAutoTimestamp;
        item.mFrameNumber = mCore->mFrameCounter;
        item.mSlot = slot;
        item.mFence = fence;
        item.mIsDroppable = mCore->mDequeueBufferCannotBlock || async;

        mStickyTransform = stickyTransform;

        if (mCore->mQueue.empty()) {
            // When the queue is empty, we can ignore mDequeueBufferCannotBlock
            // and simply queue this buffer
            mCore->mQueue.push_back(item);
            listener = mCore->mConsumerListener;
        } else {
            // When the queue is not empty, we need to look at the front buffer
            // state to see if we need to replace it
            BufferQueueCore::Fifo::iterator front(mCore->mQueue.begin());
            if (front->mIsDroppable) {
                // If the front queued buffer is still being tracked, we first
                // mark it as freed
                if (mCore->stillTracking(front)) {
                    mSlots[front->mSlot].mBufferState = BufferSlot::FREE;
                    // Reset the frame number of the freed buffer so that it is
                    // the first in line to be dequeued again
                    mSlots[front->mSlot].mFrameNumber = 0;
                }
                // Overwrite the droppable buffer with the incoming one
                *front = item;
            } else {
                mCore->mQueue.push_back(item);
                listener = mCore->mConsumerListener;
            }
        }

        mCore->mBufferHasBeenQueued = true;
        mCore->mDequeueCondition.broadcast();

        output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
                mCore->mTransformHint, mCore->mQueue.size());

        ATRACE_INT(mCore->mConsumerName.string(), mCore->mQueue.size());
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
        listener->onFrameAvailable();
    }

    return NO_ERROR;
}

void BufferQueueProducer::cancelBuffer(int slot, const sp<Fence>& fence) {
    ATRACE_CALL();
    BQ_LOGV("cancelBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("cancelBuffer: BufferQueue has been abandoned");
        return;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGE("cancelBuffer: slot index %d out of range [0, %d)",
                slot, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return;
    } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
        BQ_LOGE("cancelBuffer: slot %d is not owned by the producer "
                "(state = %d)", slot, mSlots[slot].mBufferState);
        return;
    } else if (fence == NULL) {
        BQ_LOGE("cancelBuffer: fence is NULL");
        return;
    }

    mSlots[slot].mBufferState = BufferSlot::FREE;
    mSlots[slot].mFrameNumber = 0;
    mSlots[slot].mFence = fence;
    mCore->mDequeueCondition.broadcast();
}

int BufferQueueProducer::query(int what, int *outValue) {
    ATRACE_CALL();
    Mutex::Autolock lock(mCore->mMutex);

    if (outValue == NULL) {
        BQ_LOGE("query: outValue was NULL");
        return BAD_VALUE;
    }

    if (mCore->mIsAbandoned) {
        BQ_LOGE("query: BufferQueue has been abandoned");
        return NO_INIT;
    }

    int value;
    switch (what) {
        case NATIVE_WINDOW_WIDTH:
            value = mCore->mDefaultWidth;
            break;
        case NATIVE_WINDOW_HEIGHT:
            value = mCore->mDefaultHeight;
            break;
        case NATIVE_WINDOW_FORMAT:
            value = mCore->mDefaultBufferFormat;
            break;
        case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
            value = mCore->getMinUndequeuedBufferCountLocked(false);
            break;
        case NATIVE_WINDOW_STICKY_TRANSFORM:
            value = static_cast<int>(mStickyTransform);
            break;
        case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND:
            value = (mCore->mQueue.size() > 1);
            break;
        case NATIVE_WINDOW_CONSUMER_USAGE_BITS:
            value = mCore->mConsumerUsageBits;
            break;
        default:
            return BAD_VALUE;
    }

    BQ_LOGV("query: %d? %d", what, value);
    *outValue = value;
    return NO_ERROR;
}

status_t BufferQueueProducer::connect(const sp<IProducerListener>& listener,
        int api, bool producerControlledByApp, QueueBufferOutput *output) {
    ATRACE_CALL();
    Mutex::Autolock lock(mCore->mMutex);
    mConsumerName = mCore->mConsumerName;
    BQ_LOGV("connect(P): api=%d producerControlledByApp=%s", api,
            producerControlledByApp ? "true" : "false");

    if (mCore->mIsAbandoned) {
        BQ_LOGE("connect(P): BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConsumerListener == NULL) {
        BQ_LOGE("connect(P): BufferQueue has no consumer");
        return NO_INIT;
    }

    if (output == NULL) {
        BQ_LOGE("connect(P): output was NULL");
        return BAD_VALUE;
    }

    if (mCore->mConnectedApi != BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("connect(P): already connected (cur=%d req=%d)",
                mCore->mConnectedApi, api);
        return BAD_VALUE;
    }

    int status = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            mCore->mConnectedApi = api;
            output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
                    mCore->mTransformHint, mCore->mQueue.size());

            // Set up a death notification so that we can disconnect
            // automatically if the remote producer dies
            if (listener != NULL &&
                    listener->asBinder()->remoteBinder() != NULL) {
                status = listener->asBinder()->linkToDeath(
                        static_cast<IBinder::DeathRecipient*>(this));
                if (status != NO_ERROR) {
                    BQ_LOGE("connect(P): linkToDeath failed: %s (%d)",
                            strerror(-status), status);
                }
            }
            mCore->mConnectedProducerListener = listener;
            break;
        default:
            BQ_LOGE("connect(P): unknown API %d", api);
            status = BAD_VALUE;
            break;
    }

    mCore->mBufferHasBeenQueued = false;
    mCore->mDequeueBufferCannotBlock =
            mCore->mConsumerControlledByApp && producerControlledByApp;

    return status;
}

status_t BufferQueueProducer::disconnect(int api) {
    ATRACE_CALL();
    BQ_LOGV("disconnect(P): api %d", api);

    int status = NO_ERROR;
    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (mCore->mIsAbandoned) {
            // It's not really an error to disconnect after the surface has
            // been abandoned; it should just be a no-op.
            return NO_ERROR;
        }

        switch (api) {
            case NATIVE_WINDOW_API_EGL:
            case NATIVE_WINDOW_API_CPU:
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                if (mCore->mConnectedApi == api) {
                    mCore->freeAllBuffersLocked();

                    // Remove our death notification callback if we have one
                    if (mCore->mConnectedProducerListener != NULL) {
                        sp<IBinder> token =
                                mCore->mConnectedProducerListener->asBinder();
                        // This can fail if we're here because of the death
                        // notification, but we just ignore it
                        token->unlinkToDeath(
                                static_cast<IBinder::DeathRecipient*>(this));
                    }
                    mCore->mConnectedProducerListener = NULL;
                    mCore->mConnectedApi = BufferQueueCore::NO_CONNECTED_API;
                    mCore->mSidebandStream.clear();
                    mCore->mDequeueCondition.broadcast();
                    listener = mCore->mConsumerListener;
                } else {
                    BQ_LOGE("disconnect(P): connected to another API "
                            "(cur=%d req=%d)", mCore->mConnectedApi, api);
                    status = BAD_VALUE;
                }
                break;
            default:
                BQ_LOGE("disconnect(P): unknown API %d", api);
                status = BAD_VALUE;
                break;
        }
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return status;
}

status_t BufferQueueProducer::setSidebandStream(const sp<NativeHandle>& stream) {
    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock _l(mCore->mMutex);
        mCore->mSidebandStream = stream;
        listener = mCore->mConsumerListener;
    } // Autolock scope

    if (listener != NULL) {
        listener->onSidebandStreamChanged();
    }
    return NO_ERROR;
}

void BufferQueueProducer::allocateBuffers(bool async, uint32_t width,
        uint32_t height, uint32_t format, uint32_t usage) {
    ATRACE_CALL();
    while (true) {
        Vector<int> freeSlots;
        size_t newBufferCount = 0;
        uint32_t allocWidth = 0;
        uint32_t allocHeight = 0;
        uint32_t allocFormat = 0;
        uint32_t allocUsage = 0;
        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);
            mCore->waitWhileAllocatingLocked();

            int currentBufferCount = 0;
            for (int slot = 0; slot < BufferQueueDefs::NUM_BUFFER_SLOTS; ++slot) {
                if (mSlots[slot].mGraphicBuffer != NULL) {
                    ++currentBufferCount;
                } else {
                    if (mSlots[slot].mBufferState != BufferSlot::FREE) {
                        BQ_LOGE("allocateBuffers: slot %d without buffer is not FREE",
                                slot);
                        continue;
                    }

                    freeSlots.push_back(slot);
                }
            }

            int maxBufferCount = mCore->getMaxBufferCountLocked(async);
            BQ_LOGV("allocateBuffers: allocating from %d buffers up to %d buffers",
                    currentBufferCount, maxBufferCount);
            if (maxBufferCount <= currentBufferCount)
                return;
            newBufferCount = maxBufferCount - currentBufferCount;
            if (freeSlots.size() < newBufferCount) {
                BQ_LOGE("allocateBuffers: ran out of free slots");
                return;
            }
            allocWidth = width > 0 ? width : mCore->mDefaultWidth;
            allocHeight = height > 0 ? height : mCore->mDefaultHeight;
            allocFormat = format != 0 ? format : mCore->mDefaultBufferFormat;
            allocUsage = usage | mCore->mConsumerUsageBits;

            mCore->mIsAllocating = true;
        } // Autolock scope

        Vector<sp<GraphicBuffer> > buffers;
        for (size_t i = 0; i <  newBufferCount; ++i) {
            status_t result = NO_ERROR;
            sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
                    allocWidth, allocHeight, allocFormat, allocUsage, &result));
            if (result != NO_ERROR) {
                BQ_LOGE("allocateBuffers: failed to allocate buffer (%u x %u, format"
                        " %u, usage %u)", width, height, format, usage);
                Mutex::Autolock lock(mCore->mMutex);
                mCore->mIsAllocating = false;
                mCore->mIsAllocatingCondition.broadcast();
                return;
            }
            buffers.push_back(graphicBuffer);
        }

        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);
            uint32_t checkWidth = width > 0 ? width : mCore->mDefaultWidth;
            uint32_t checkHeight = height > 0 ? height : mCore->mDefaultHeight;
            uint32_t checkFormat = format != 0 ? format : mCore->mDefaultBufferFormat;
            uint32_t checkUsage = usage | mCore->mConsumerUsageBits;
            if (checkWidth != allocWidth || checkHeight != allocHeight ||
                checkFormat != allocFormat || checkUsage != allocUsage) {
                // Something changed while we released the lock. Retry.
                BQ_LOGV("allocateBuffers: size/format/usage changed while allocating. Retrying.");
                mCore->mIsAllocating = false;
                mCore->mIsAllocatingCondition.broadcast();
                continue;
            }

            for (size_t i = 0; i < newBufferCount; ++i) {
                int slot = freeSlots[i];
                if (mSlots[slot].mBufferState != BufferSlot::FREE) {
                    // A consumer allocated the FREE slot with attachBuffer. Discard the buffer we
                    // allocated.
                    BQ_LOGV("allocateBuffers: slot %d was acquired while allocating. "
                            "Dropping allocated buffer.", slot);
                    continue;
                }
                mCore->freeBufferLocked(slot); // Clean up the slot first
                mSlots[slot].mGraphicBuffer = buffers[i];
                mSlots[slot].mFrameNumber = 0;
                mSlots[slot].mFence = Fence::NO_FENCE;
                BQ_LOGV("allocateBuffers: allocated a new buffer in slot %d", slot);
            }

            mCore->mIsAllocating = false;
            mCore->mIsAllocatingCondition.broadcast();
        } // Autolock scope
    }
}

void BufferQueueProducer::binderDied(const wp<android::IBinder>& /* who */) {
    // If we're here, it means that a producer we were connected to died.
    // We're guaranteed that we are still connected to it because we remove
    // this callback upon disconnect. It's therefore safe to read mConnectedApi
    // without synchronization here.
    int api = mCore->mConnectedApi;
    disconnect(api);
}

} // namespace android
