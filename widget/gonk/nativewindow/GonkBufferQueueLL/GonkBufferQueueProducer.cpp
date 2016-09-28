/*
 * Copyright 2014 The Android Open Source Project
 * Copyright (C) 2014 Mozilla Foundation
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

#define LOG_TAG "GonkBufferQueueProducer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include "GonkBufferItem.h"
#include "GonkBufferQueueCore.h"
#include "GonkBufferQueueProducer.h"
#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/IProducerListener.h>

#include <cutils/compiler.h>
#include <utils/Log.h>
#include <utils/Trace.h>

#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureClient.h"

namespace android {

GonkBufferQueueProducer::GonkBufferQueueProducer(const sp<GonkBufferQueueCore>& core) :
    mCore(core),
    mSlots(core->mSlots),
    mConsumerName(),
    mSynchronousMode(true),
    mStickyTransform(0) {}

GonkBufferQueueProducer::~GonkBufferQueueProducer() {}

status_t GonkBufferQueueProducer::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ATRACE_CALL();
    ALOGV("requestBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        ALOGE("requestBuffer: GonkBufferQueue has been abandoned");
        return NO_INIT;
    }

    if (slot < 0 || slot >= GonkBufferQueueDefs::NUM_BUFFER_SLOTS) {
        ALOGE("requestBuffer: slot index %d out of range [0, %d)",
                slot, GonkBufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (mSlots[slot].mBufferState != GonkBufferSlot::DEQUEUED) {
        ALOGE("requestBuffer: slot %d is not owned by the producer "
                "(state = %d)", slot, mSlots[slot].mBufferState);
        return BAD_VALUE;
    }

    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}

status_t GonkBufferQueueProducer::setBufferCount(int bufferCount) {
    ATRACE_CALL();
    ALOGV("setBufferCount: count = %d", bufferCount);

    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (mCore->mIsAbandoned) {
            ALOGE("setBufferCount: GonkBufferQueue has been abandoned");
            return NO_INIT;
        }

        if (bufferCount > GonkBufferQueueDefs::NUM_BUFFER_SLOTS) {
            ALOGE("setBufferCount: bufferCount %d too large (max %d)",
                    bufferCount, GonkBufferQueueDefs::NUM_BUFFER_SLOTS);
            return BAD_VALUE;
        }

        // There must be no dequeued buffers when changing the buffer count.
        for (int s = 0; s < GonkBufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            if (mSlots[s].mBufferState == GonkBufferSlot::DEQUEUED) {
                ALOGE("setBufferCount: buffer owned by producer");
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
            ALOGE("setBufferCount: requested buffer count %d is less than "
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

status_t GonkBufferQueueProducer::waitForFreeSlotThenRelock(const char* caller,
        bool async, int* found, status_t* returnFlags) const {
    bool tryAgain = true;
    while (tryAgain) {
        if (mCore->mIsAbandoned) {
            ALOGE("%s: GonkBufferQueue has been abandoned", caller);
            return NO_INIT;
        }

        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
        if (async && mCore->mOverrideMaxBufferCount) {
            // FIXME: Some drivers are manually setting the buffer count
            // (which they shouldn't), so we do this extra test here to
            // handle that case. This is TEMPORARY until we get this fixed.
            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
                ALOGE("%s: async mode is invalid with buffer count override",
                        caller);
                return BAD_VALUE;
            }
        }

        // Free up any buffers that are in slots beyond the max buffer count
        //for (int s = maxBufferCount; s < GonkBufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        //    assert(mSlots[s].mBufferState == GonkBufferSlot::FREE);
        //    if (mSlots[s].mGraphicBuffer != NULL) {
        //        mCore->freeBufferLocked(s);
        //        *returnFlags |= RELEASE_ALL_BUFFERS;
        //    }
        //}

        // Look for a free buffer to give to the client
        *found = GonkBufferQueueCore::INVALID_BUFFER_SLOT;
        int dequeuedCount = 0;
        int acquiredCount = 0;
        for (int s = 0; s < maxBufferCount; ++s) {
            switch (mSlots[s].mBufferState) {
                case GonkBufferSlot::DEQUEUED:
                    ++dequeuedCount;
                    break;
                case GonkBufferSlot::ACQUIRED:
                    ++acquiredCount;
                    break;
                case GonkBufferSlot::FREE:
                    // We return the oldest of the free buffers to avoid
                    // stalling the producer if possible, since the consumer
                    // may still have pending reads of in-flight buffers
                    if (*found == GonkBufferQueueCore::INVALID_BUFFER_SLOT ||
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
            ALOGE("%s: can't dequeue multiple buffers without setting the "
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
                ALOGE("%s: min undequeued buffer count (%d) exceeded "
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
            ALOGV("%s: queue size is %zu, waiting", caller,
                    mCore->mQueue.size());
        }

        // If no buffer is found, or if the queue has too many buffers
        // outstanding, wait for a buffer to be acquired or released, or for the
        // max buffer count to change.
        tryAgain = (*found == GonkBufferQueueCore::INVALID_BUFFER_SLOT) ||
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

status_t GonkBufferQueueProducer::dequeueBuffer(int *outSlot,
        sp<android::Fence> *outFence, bool async,
        uint32_t width, uint32_t height, uint32_t format, uint32_t usage) {
    ATRACE_CALL();
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mConsumerName = mCore->mConsumerName;
    } // Autolock scope

    ALOGV("dequeueBuffer: async=%s w=%u h=%u format=%#x, usage=%#x",
            async ? "true" : "false", width, height, format, usage);

    if ((width && !height) || (!width && height)) {
        ALOGE("dequeueBuffer: invalid size: w=%u h=%u", width, height);
        return BAD_VALUE;
    }

    status_t returnFlags = NO_ERROR;
    // Reset slot
    *outSlot = GonkBufferQueueCore::INVALID_BUFFER_SLOT;

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
        if (found == GonkBufferQueueCore::INVALID_BUFFER_SLOT) {
            ALOGE("dequeueBuffer: no available buffer slots");
            return -EBUSY;
        }

        *outSlot = found;

        attachedByConsumer = mSlots[found].mAttachedByConsumer;

        const bool useDefaultSize = !width && !height;
        if (useDefaultSize) {
            width = mCore->mDefaultWidth;
            height = mCore->mDefaultHeight;
        }

        mSlots[found].mBufferState = GonkBufferSlot::DEQUEUED;

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
            mSlots[found].mFence = Fence::NO_FENCE;

            if (mSlots[found].mTextureClient) {
                mSlots[found].mTextureClient->ClearRecycleCallback();
                // release TextureClient in ImageBridge thread
                RefPtr<TextureClientReleaseTask> task =
                  MakeAndAddRef<TextureClientReleaseTask>(mSlots[found].mTextureClient);
                mSlots[found].mTextureClient = NULL;
                ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(task.forget());
            }

            returnFlags |= BUFFER_NEEDS_REALLOCATION;
        }

        if (CC_UNLIKELY(mSlots[found].mFence == NULL)) {
            ALOGE("dequeueBuffer: about to return a NULL fence - "
                    "slot=%d w=%d h=%d format=%u",
                    found, buffer->width, buffer->height, buffer->format);
        }

        *outFence = mSlots[found].mFence;
        mSlots[found].mFence = Fence::NO_FENCE;
    } // Autolock scope

    if (returnFlags & BUFFER_NEEDS_REALLOCATION) {
        RefPtr<LayersIPCChannel> allocator = ImageBridgeChild::GetSingleton();
        usage |= GraphicBuffer::USAGE_HW_TEXTURE;
        GrallocTextureData* texData = GrallocTextureData::Create(IntSize(width,height), format,
                                                                 gfx::BackendType::NONE,
                                                                 usage, allocator);
        if (!texData) {
            ALOGE("dequeueBuffer: failed to alloc gralloc buffer");
            return -ENOMEM;
        }
        RefPtr<TextureClient> textureClient = TextureClient::CreateWithData(
            texData, TextureFlags::RECYCLE | TextureFlags::DEALLOCATE_CLIENT, allocator);

        sp<GraphicBuffer> graphicBuffer = texData->GetGraphicBuffer();

        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);

            if (mCore->mIsAbandoned) {
                ALOGE("dequeueBuffer: GonkBufferQueue has been abandoned");
                return NO_INIT;
            }

            mSlots[*outSlot].mFrameNumber = UINT32_MAX;
            mSlots[*outSlot].mGraphicBuffer = graphicBuffer;
            mSlots[*outSlot].mTextureClient = textureClient;
        } // Autolock scope
    }

    if (attachedByConsumer) {
        returnFlags |= BUFFER_NEEDS_REALLOCATION;
    }

    ALOGV("dequeueBuffer: returning slot=%d/%" PRIu64 " buf=%p flags=%#x",
            *outSlot,
            mSlots[*outSlot].mFrameNumber,
            mSlots[*outSlot].mGraphicBuffer->handle, returnFlags);

    return returnFlags;
}

status_t GonkBufferQueueProducer::detachBuffer(int slot) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(slot);
    ALOGV("detachBuffer(P): slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        ALOGE("detachBuffer(P): GonkBufferQueue has been abandoned");
        return NO_INIT;
    }

    if (slot < 0 || slot >= GonkBufferQueueDefs::NUM_BUFFER_SLOTS) {
        ALOGE("detachBuffer(P): slot index %d out of range [0, %d)",
                slot, GonkBufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (mSlots[slot].mBufferState != GonkBufferSlot::DEQUEUED) {
        ALOGE("detachBuffer(P): slot %d is not owned by the producer "
                "(state = %d)", slot, mSlots[slot].mBufferState);
        return BAD_VALUE;
    } else if (!mSlots[slot].mRequestBufferCalled) {
        ALOGE("detachBuffer(P): buffer in slot %d has not been requested",
                slot);
        return BAD_VALUE;
    }

    mCore->freeBufferLocked(slot);
    mCore->mDequeueCondition.broadcast();

    return NO_ERROR;
}

status_t GonkBufferQueueProducer::detachNextBuffer(sp<GraphicBuffer>* outBuffer,
        sp<Fence>* outFence) {
    ATRACE_CALL();

    if (outBuffer == NULL) {
        ALOGE("detachNextBuffer: outBuffer must not be NULL");
        return BAD_VALUE;
    } else if (outFence == NULL) {
        ALOGE("detachNextBuffer: outFence must not be NULL");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mCore->mMutex);
    mCore->waitWhileAllocatingLocked();

    if (mCore->mIsAbandoned) {
        ALOGE("detachNextBuffer: GonkBufferQueue has been abandoned");
        return NO_INIT;
    }

    // Find the oldest valid slot
    int found = GonkBufferQueueCore::INVALID_BUFFER_SLOT;
    for (int s = 0; s < GonkBufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        if (mSlots[s].mBufferState == GonkBufferSlot::FREE &&
                mSlots[s].mGraphicBuffer != NULL) {
            if (found == GonkBufferQueueCore::INVALID_BUFFER_SLOT ||
                    mSlots[s].mFrameNumber < mSlots[found].mFrameNumber) {
                found = s;
            }
        }
    }

    if (found == GonkBufferQueueCore::INVALID_BUFFER_SLOT) {
        return NO_MEMORY;
    }

    ALOGV("detachNextBuffer detached slot %d", found);

    *outBuffer = mSlots[found].mGraphicBuffer;
    *outFence = mSlots[found].mFence;
    mCore->freeBufferLocked(found);

    return NO_ERROR;
}

status_t GonkBufferQueueProducer::attachBuffer(int* outSlot,
        const sp<android::GraphicBuffer>& buffer) {
    ATRACE_CALL();

    if (outSlot == NULL) {
        ALOGE("attachBuffer(P): outSlot must not be NULL");
        return BAD_VALUE;
    } else if (buffer == NULL) {
        ALOGE("attachBuffer(P): cannot attach NULL buffer");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mCore->mMutex);
    mCore->waitWhileAllocatingLocked();

    status_t returnFlags = NO_ERROR;
    int found;
    // TODO: Should we provide an async flag to attachBuffer? It seems
    // unlikely that buffers which we are attaching to a GonkBufferQueue will
    // be asynchronous (droppable), but it may not be impossible.
    status_t status = waitForFreeSlotThenRelock("attachBuffer(P)", false,
            &found, &returnFlags);
    if (status != NO_ERROR) {
        return status;
    }

    // This should not happen
    if (found == GonkBufferQueueCore::INVALID_BUFFER_SLOT) {
        ALOGE("attachBuffer(P): no available buffer slots");
        return -EBUSY;
    }

    *outSlot = found;
    ATRACE_BUFFER_INDEX(*outSlot);
    ALOGV("attachBuffer(P): returning slot %d flags=%#x",
            *outSlot, returnFlags);

    mSlots[*outSlot].mGraphicBuffer = buffer;
    mSlots[*outSlot].mBufferState = GonkBufferSlot::DEQUEUED;
    mSlots[*outSlot].mFence = Fence::NO_FENCE;
    mSlots[*outSlot].mRequestBufferCalled = true;

    return returnFlags;
}

status_t GonkBufferQueueProducer::setSynchronousMode(bool enabled) {
    ALOGV("setSynchronousMode: enabled=%d", enabled);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        ALOGE("setSynchronousMode: BufferQueue has been abandoned!");
        return NO_INIT;
    }

    if (mSynchronousMode != enabled) {
        mSynchronousMode = enabled;
        mCore->mDequeueCondition.broadcast();
    }
    return OK;
}

status_t GonkBufferQueueProducer::queueBuffer(int slot,
        const QueueBufferInput &input, QueueBufferOutput *output) {
    ATRACE_CALL();

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
        ALOGE("queueBuffer: fence is NULL");
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
            ALOGE("queueBuffer: unknown scaling mode %d", scalingMode);
            return BAD_VALUE;
    }

    GonkBufferItem item;
    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);

        if (mCore->mIsAbandoned) {
            ALOGE("queueBuffer: GonkBufferQueue has been abandoned");
            return NO_INIT;
        }

        const int maxBufferCount = mCore->getMaxBufferCountLocked(async);
        if (async && mCore->mOverrideMaxBufferCount) {
            // FIXME: Some drivers are manually setting the buffer count
            // (which they shouldn't), so we do this extra test here to
            // handle that case. This is TEMPORARY until we get this fixed.
            if (mCore->mOverrideMaxBufferCount < maxBufferCount) {
                ALOGE("queueBuffer: async mode is invalid with "
                        "buffer count override");
                return BAD_VALUE;
            }
        }

        if (slot < 0 || slot >= maxBufferCount) {
            ALOGE("queueBuffer: slot index %d out of range [0, %d)",
                    slot, maxBufferCount);
            return BAD_VALUE;
        } else if (mSlots[slot].mBufferState != GonkBufferSlot::DEQUEUED) {
            ALOGE("queueBuffer: slot %d is not owned by the producer "
                    "(state = %d)", slot, mSlots[slot].mBufferState);
            return BAD_VALUE;
        } else if (!mSlots[slot].mRequestBufferCalled) {
            ALOGE("queueBuffer: slot %d was queued without requesting "
                    "a buffer", slot);
            return BAD_VALUE;
        }

        ALOGV("queueBuffer: slot=%d/%" PRIu64 " time=%" PRIu64
                " crop=[%d,%d,%d,%d] transform=%#x scale=%s",
                slot, mCore->mFrameCounter + 1, timestamp,
                crop.left, crop.top, crop.right, crop.bottom,
                transform, GonkBufferItem::scalingModeName(scalingMode));

        const sp<GraphicBuffer>& graphicBuffer(mSlots[slot].mGraphicBuffer);
        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
        Rect croppedRect;
        crop.intersect(bufferRect, &croppedRect);
        if (croppedRect != crop) {
            ALOGE("queueBuffer: crop rect is not contained within the "
                    "buffer in slot %d", slot);
            return BAD_VALUE;
        }

        mSlots[slot].mFence = fence;
        mSlots[slot].mBufferState = GonkBufferSlot::QUEUED;
        ++mCore->mFrameCounter;
        mSlots[slot].mFrameNumber = mCore->mFrameCounter;

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
            GonkBufferQueueCore::Fifo::iterator front(mCore->mQueue.begin());
            if (front->mIsDroppable || !mSynchronousMode) {
                // If the front queued buffer is still being tracked, we first
                // mark it as freed
                if (mCore->stillTracking(front)) {
                    mSlots[front->mSlot].mBufferState = GonkBufferSlot::FREE;
                    // Reset the frame number of the freed buffer so that it is
                    // the first in line to be dequeued again
                    mSlots[front->mSlot].mFrameNumber = 0;
                }
                // Overwrite the droppable buffer with the incoming one
                *front = item;
                listener = mCore->mConsumerListener;
            } else {
                mCore->mQueue.push_back(item);
                listener = mCore->mConsumerListener;
            }
        }

        mCore->mBufferHasBeenQueued = true;
        mCore->mDequeueCondition.broadcast();

        output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
                mCore->mTransformHint, mCore->mQueue.size());

        item.mGraphicBuffer.clear();
        item.mSlot = GonkBufferItem::INVALID_BUFFER_SLOT;
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
#if ANDROID_VERSION == 21
        listener->onFrameAvailable();
#else
        listener->onFrameAvailable(reinterpret_cast<::android::BufferItem&>(item));
#endif
    }

    return NO_ERROR;
}

void GonkBufferQueueProducer::cancelBuffer(int slot, const sp<Fence>& fence) {
    ATRACE_CALL();
    ALOGV("cancelBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        ALOGE("cancelBuffer: GonkBufferQueue has been abandoned");
        return;
    }

    if (slot < 0 || slot >= GonkBufferQueueDefs::NUM_BUFFER_SLOTS) {
        ALOGE("cancelBuffer: slot index %d out of range [0, %d)",
                slot, GonkBufferQueueDefs::NUM_BUFFER_SLOTS);
        return;
    } else if (mSlots[slot].mBufferState != GonkBufferSlot::DEQUEUED) {
        ALOGE("cancelBuffer: slot %d is not owned by the producer "
                "(state = %d)", slot, mSlots[slot].mBufferState);
        return;
    } else if (fence == NULL) {
        ALOGE("cancelBuffer: fence is NULL");
        return;
    }

    mSlots[slot].mBufferState = GonkBufferSlot::FREE;
    mSlots[slot].mFrameNumber = 0;
    mSlots[slot].mFence = fence;
    mCore->mDequeueCondition.broadcast();
}

int GonkBufferQueueProducer::query(int what, int *outValue) {
    ATRACE_CALL();
    Mutex::Autolock lock(mCore->mMutex);

    if (outValue == NULL) {
        ALOGE("query: outValue was NULL");
        return BAD_VALUE;
    }

    if (mCore->mIsAbandoned) {
        ALOGE("query: GonkBufferQueue has been abandoned");
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

    ALOGV("query: %d? %d", what, value);
    *outValue = value;
    return NO_ERROR;
}

status_t GonkBufferQueueProducer::connect(const sp<IProducerListener>& listener,
        int api, bool producerControlledByApp, QueueBufferOutput *output) {
    ATRACE_CALL();
    Mutex::Autolock lock(mCore->mMutex);
    mConsumerName = mCore->mConsumerName;
    ALOGV("connect(P): api=%d producerControlledByApp=%s", api,
            producerControlledByApp ? "true" : "false");

    if (mCore->mIsAbandoned) {
        ALOGE("connect(P): GonkBufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConsumerListener == NULL) {
        ALOGE("connect(P): GonkBufferQueue has no consumer");
        return NO_INIT;
    }

    if (output == NULL) {
        ALOGE("connect(P): output was NULL");
        return BAD_VALUE;
    }

    if (mCore->mConnectedApi != GonkBufferQueueCore::NO_CONNECTED_API) {
        ALOGE("connect(P): already connected (cur=%d req=%d)", mCore->mConnectedApi,
              api);
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
                    ALOGE("connect(P): linkToDeath failed: %s (%d)",
                            strerror(-status), status);
                }
            }
            mCore->mConnectedProducerListener = listener;
            break;
        default:
            ALOGE("connect(P): unknown API %d", api);
            status = BAD_VALUE;
            break;
    }

    mCore->mBufferHasBeenQueued = false;
    mCore->mDequeueBufferCannotBlock =
            mCore->mConsumerControlledByApp && producerControlledByApp;

    return status;
}

status_t GonkBufferQueueProducer::disconnect(int api) {
    ATRACE_CALL();
    ALOGV("disconnect(P): api %d", api);

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
                    mCore->mConnectedApi = GonkBufferQueueCore::NO_CONNECTED_API;
                    mCore->mSidebandStream.clear();
                    mCore->mDequeueCondition.broadcast();
                    listener = mCore->mConsumerListener;
                } else {
                    ALOGE("disconnect(P): connected to another API "
                            "(cur=%d req=%d)", mCore->mConnectedApi, api);
                    status = BAD_VALUE;
                }
                break;
            default:
                ALOGE("disconnect(P): unknown API %d", api);
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

status_t GonkBufferQueueProducer::setSidebandStream(const sp<NativeHandle>& stream) {
    return INVALID_OPERATION;
}

void GonkBufferQueueProducer::allocateBuffers(bool async, uint32_t width,
        uint32_t height, uint32_t format, uint32_t usage) {
    ALOGE("allocateBuffers: no op");
}

void GonkBufferQueueProducer::binderDied(const wp<android::IBinder>& /* who */) {
    // If we're here, it means that a producer we were connected to died.
    // We're guaranteed that we are still connected to it because we remove
    // this callback upon disconnect. It's therefore safe to read mConnectedApi
    // without synchronization here.
    int api = mCore->mConnectedApi;
    disconnect(api);
}

} // namespace android
