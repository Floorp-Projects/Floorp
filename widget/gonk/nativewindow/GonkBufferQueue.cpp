/*
 * Copyright (C) 2012 The Android Open Source Project
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

#define LOG_TAG "GonkBufferQueue"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_NDEBUG 0

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <utils/Log.h>

#include "mozilla/layers/ImageBridgeChild.h"

#include "GonkBufferQueue.h"

// Macros for including the GonkBufferQueue name in log messages
#define ST_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ST_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ST_LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ST_LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ST_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define ATRACE_BUFFER_INDEX(index)

using namespace mozilla::layers;

namespace android {

// Get an ID that's unique within this process.
static int32_t createProcessUniqueId() {
    static volatile int32_t globalCounter = 0;
    return android_atomic_inc(&globalCounter);
}

static const char* scalingModeName(int scalingMode) {
    switch (scalingMode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE: return "FREEZE";
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW: return "SCALE_TO_WINDOW";
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP: return "SCALE_CROP";
        default: return "Unknown";
    }
}

GonkBufferQueue::GonkBufferQueue(bool allowSynchronousMode,
        const sp<IGraphicBufferAlloc>& allocator) :
    mDefaultWidth(1),
    mDefaultHeight(1),
    mMaxAcquiredBufferCount(1),
    mDefaultMaxBufferCount(2),
    mOverrideMaxBufferCount(0),
    mSynchronousMode(true), // GonkBufferQueue always works in sync mode.
    mAllowSynchronousMode(allowSynchronousMode),
    mConnectedApi(NO_CONNECTED_API),
    mAbandoned(false),
    mFrameCounter(0),
    mBufferHasBeenQueued(false),
    mDefaultBufferFormat(PIXEL_FORMAT_RGBA_8888),
    mConsumerUsageBits(0),
    mTransformHint(0),
    mGeneration(0)
{
    // Choose a name using the PID and a process-unique ID.
    mConsumerName = String8::format("unnamed-%d-%d", getpid(), createProcessUniqueId());

    ST_LOGV("GonkBufferQueue");
}

GonkBufferQueue::~GonkBufferQueue() {
    ST_LOGV("~GonkBufferQueue");
}

status_t GonkBufferQueue::setDefaultMaxBufferCountLocked(int count) {
    if (count < 2 || count > NUM_BUFFER_SLOTS)
        return BAD_VALUE;

    mDefaultMaxBufferCount = count;
    mDequeueCondition.broadcast();

    return NO_ERROR;
}

bool GonkBufferQueue::isSynchronousMode() const {
    Mutex::Autolock lock(mMutex);
    return mSynchronousMode;
}

void GonkBufferQueue::setConsumerName(const String8& name) {
    Mutex::Autolock lock(mMutex);
    mConsumerName = name;
}

status_t GonkBufferQueue::setDefaultBufferFormat(uint32_t defaultFormat) {
    Mutex::Autolock lock(mMutex);
    mDefaultBufferFormat = defaultFormat;
    return NO_ERROR;
}

status_t GonkBufferQueue::setConsumerUsageBits(uint32_t usage) {
    Mutex::Autolock lock(mMutex);
    mConsumerUsageBits = usage;
    return NO_ERROR;
}

status_t GonkBufferQueue::setTransformHint(uint32_t hint) {
    ST_LOGV("setTransformHint: %02x", hint);
    Mutex::Autolock lock(mMutex);
    mTransformHint = hint;
    return NO_ERROR;
}

int GonkBufferQueue::getGeneration() {
    return mGeneration;
}

mozilla::layers::SurfaceDescriptor*
GonkBufferQueue::getSurfaceDescriptorFromBuffer(ANativeWindowBuffer* buffer)
{
    Mutex::Autolock _l(mMutex);
    if (buffer == NULL) {
        ST_LOGE("getSlotFromBufferLocked: encountered NULL buffer");
        return nullptr;
    }

    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].mGraphicBuffer != NULL && mSlots[i].mGraphicBuffer->handle == buffer->handle) {
            return &mSlots[i].mSurfaceDescriptor;
        }
    }
    ST_LOGE("getSlotFromBufferLocked: unknown buffer: %p", buffer->handle);
    return nullptr;
}

status_t GonkBufferQueue::setBufferCount(int bufferCount) {
    ST_LOGV("setBufferCount: count=%d", bufferCount);

    sp<ConsumerListener> listener;
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    {
        Mutex::Autolock lock(mMutex);

        if (mAbandoned) {
            ST_LOGE("setBufferCount: GonkBufferQueue has been abandoned!");
            return NO_INIT;
        }
        if (bufferCount > NUM_BUFFER_SLOTS) {
            ST_LOGE("setBufferCount: bufferCount too large (max %d)",
                    NUM_BUFFER_SLOTS);
            return BAD_VALUE;
        }

        // Error out if the user has dequeued buffers
        int maxBufferCount = getMaxBufferCountLocked();
        for (int i=0 ; i<maxBufferCount; i++) {
            if (mSlots[i].mBufferState == BufferSlot::DEQUEUED) {
                ST_LOGE("setBufferCount: client owns some buffers");
                return -EINVAL;
            }
        }

        const int minBufferSlots = getMinMaxBufferCountLocked();
        if (bufferCount == 0) {
            mOverrideMaxBufferCount = 0;
            mDequeueCondition.broadcast();
            return NO_ERROR;
        }

        if (bufferCount < minBufferSlots) {
            ST_LOGE("setBufferCount: requested buffer count (%d) is less than "
                    "minimum (%d)", bufferCount, minBufferSlots);
            return BAD_VALUE;
        }

        // here we're guaranteed that the client doesn't have dequeued buffers
        // and will release all of its buffer references.
        //
        // XXX: Should this use drainQueueAndFreeBuffersLocked instead?
        freeAllBuffersLocked(freeList);
        mOverrideMaxBufferCount = bufferCount;
        mBufferHasBeenQueued = false;
        mDequeueCondition.broadcast();
        listener = mConsumerListener;
    } // scope for lock
    releaseBufferFreeListUnlocked(freeList);

    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return NO_ERROR;
}

int GonkBufferQueue::query(int what, int* outValue)
{
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("query: GonkBufferQueue has been abandoned!");
        return NO_INIT;
    }

    int value;
    switch (what) {
    case NATIVE_WINDOW_WIDTH:
        value = mDefaultWidth;
        break;
    case NATIVE_WINDOW_HEIGHT:
        value = mDefaultHeight;
        break;
    case NATIVE_WINDOW_FORMAT:
        value = mDefaultBufferFormat;
        break;
    case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
        value = getMinUndequeuedBufferCountLocked();
        break;
    case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND:
        value = (mQueue.size() >= 2);
        break;
    default:
        return BAD_VALUE;
    }
    outValue[0] = value;
    return NO_ERROR;
}

status_t GonkBufferQueue::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ST_LOGV("requestBuffer: slot=%d", slot);
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        ST_LOGE("requestBuffer: GonkBufferQueue has been abandoned!");
        return NO_INIT;
    }
    int maxBufferCount = getMaxBufferCountLocked();
    if (slot < 0 || maxBufferCount <= slot) {
        ST_LOGE("requestBuffer: slot index out of range [0, %d]: %d",
                maxBufferCount, slot);
        return BAD_VALUE;
    } else if (mSlots[slot].mBufferState != BufferSlot::DEQUEUED) {
        // XXX: I vaguely recall there was some reason this can be valid, but
        // for the life of me I can't recall under what circumstances that's
        // the case.
        ST_LOGE("requestBuffer: slot %d is not owned by the client (state=%d)",
                slot, mSlots[slot].mBufferState);
        return BAD_VALUE;
    }
    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}

status_t GonkBufferQueue::dequeueBuffer(int *outBuf, sp<Fence>* outFence,
        uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
    ST_LOGV("dequeueBuffer: w=%d h=%d fmt=%#x usage=%#x", w, h, format, usage);

    if ((w && !h) || (!w && h)) {
        ST_LOGE("dequeueBuffer: invalid size: w=%u, h=%u", w, h);
        return BAD_VALUE;
    }

    status_t returnFlags(OK);
    uint32_t generation;
    int buf = INVALID_BUFFER_SLOT;
    SurfaceDescriptor descOld;

    { // Scope for the lock
        Mutex::Autolock lock(mMutex);
        generation = mGeneration;

        if (format == 0) {
            format = mDefaultBufferFormat;
        }
        // turn on usage bits the consumer requested
        usage |= mConsumerUsageBits;

        int found = -1;
        int dequeuedCount = 0;
        bool tryAgain = true;
        while (tryAgain) {
            if (mAbandoned) {
                ST_LOGE("dequeueBuffer: GonkBufferQueue has been abandoned!");
                return NO_INIT;
            }

            const int maxBufferCount = getMaxBufferCountLocked();

            // Free up any buffers that are in slots beyond the max buffer
            // count.
            //for (int i = maxBufferCount; i < NUM_BUFFER_SLOTS; i++) {
            //    assert(mSlots[i].mBufferState == BufferSlot::FREE);
            //    if (mSlots[i].mGraphicBuffer != NULL) {
            //        freeBufferLocked(i);
            //        returnFlags |= IGraphicBufferProducer::RELEASE_ALL_BUFFERS;
            //    }
            //}

            // look for a free buffer to give to the client
            found = INVALID_BUFFER_SLOT;
            dequeuedCount = 0;
            for (int i = 0; i < maxBufferCount; i++) {
                const int state = mSlots[i].mBufferState;
                if (state == BufferSlot::DEQUEUED) {
                    dequeuedCount++;
                }

                if (state == BufferSlot::FREE) {
                    /* We return the oldest of the free buffers to avoid
                     * stalling the producer if possible.  This is because
                     * the consumer may still have pending reads of the
                     * buffers in flight.
                     */
                    if ((found < 0) ||
                            mSlots[i].mFrameNumber < mSlots[found].mFrameNumber) {
                        found = i;
                    }
                }
            }

            // clients are not allowed to dequeue more than one buffer
            // if they didn't set a buffer count.
            if (!mOverrideMaxBufferCount && dequeuedCount) {
                ST_LOGE("dequeueBuffer: can't dequeue multiple buffers without "
                        "setting the buffer count");
                return -EINVAL;
            }

            // See whether a buffer has been queued since the last
            // setBufferCount so we know whether to perform the min undequeued
            // buffers check below.
            if (mBufferHasBeenQueued) {
                // make sure the client is not trying to dequeue more buffers
                // than allowed.
                const int newUndequeuedCount = maxBufferCount - (dequeuedCount+1);
                const int minUndequeuedCount = getMinUndequeuedBufferCountLocked();
                if (newUndequeuedCount < minUndequeuedCount) {
                    ST_LOGE("dequeueBuffer: min undequeued buffer count (%d) "
                            "exceeded (dequeued=%d undequeudCount=%d)",
                            minUndequeuedCount, dequeuedCount,
                            newUndequeuedCount);
                    return -EBUSY;
                }
            }

            // If no buffer is found, wait for a buffer to be released or for
            // the max buffer count to change.
            tryAgain = found == INVALID_BUFFER_SLOT;
            if (tryAgain) {
                mDequeueCondition.wait(mMutex);
            }
        }


        if (found == INVALID_BUFFER_SLOT) {
            // This should not happen.
            ST_LOGE("dequeueBuffer: no available buffer slots");
            return -EBUSY;
        }

        buf = found;
        *outBuf = found;

        const bool useDefaultSize = !w && !h;
        if (useDefaultSize) {
            // use the default size
            w = mDefaultWidth;
            h = mDefaultHeight;
        }

        mSlots[buf].mBufferState = BufferSlot::DEQUEUED;

        const sp<GraphicBuffer>& buffer(mSlots[buf].mGraphicBuffer);
        if ((buffer == NULL) ||
            (uint32_t(buffer->width)  != w) ||
            (uint32_t(buffer->height) != h) ||
            (uint32_t(buffer->format) != format) ||
            ((uint32_t(buffer->usage) & usage) != usage))
        {
            mSlots[buf].mAcquireCalled = false;
            mSlots[buf].mGraphicBuffer = NULL;
            mSlots[buf].mRequestBufferCalled = false;
            mSlots[buf].mFence = Fence::NO_FENCE;
            descOld = mSlots[buf].mSurfaceDescriptor;
            mSlots[buf].mSurfaceDescriptor = SurfaceDescriptor();

            returnFlags |= IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION;
        }

        *outFence = mSlots[buf].mFence;
        mSlots[buf].mFence = Fence::NO_FENCE;
    }  // end lock scope

    SurfaceDescriptor desc;
    ImageBridgeChild* ibc;
    sp<GraphicBuffer> graphicBuffer;
    if (returnFlags & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) {
        usage |= GraphicBuffer::USAGE_HW_TEXTURE;
        status_t error;
        ibc = ImageBridgeChild::GetSingleton();
        ST_LOGD("dequeueBuffer: about to alloc surface descriptor");
        ibc->AllocSurfaceDescriptorGralloc(gfxIntSize(w, h),
                                           format,
                                           usage,
                                           &desc);
        // We can only use a gralloc buffer here.  If we didn't get
        // one back, something went wrong.
        ST_LOGD("dequeueBuffer: got surface descriptor");
        if (SurfaceDescriptor::TSurfaceDescriptorGralloc != desc.type()) {
            MOZ_ASSERT(SurfaceDescriptor::T__None == desc.type());
            ST_LOGE("dequeueBuffer: failed to alloc gralloc buffer");
            return -ENOMEM;
        }
        graphicBuffer = GrallocBufferActor::GetFrom(desc.get_SurfaceDescriptorGralloc());
        error = graphicBuffer->initCheck();
        if (error != NO_ERROR) {
            ST_LOGE("dequeueBuffer: createGraphicBuffer failed with error %d", error);
            return error;
        }

        bool tooOld = false;
        { // Scope for the lock
            Mutex::Autolock lock(mMutex);

            if (mAbandoned) {
                ST_LOGE("dequeueBuffer: SurfaceTexture has been abandoned!");
                return NO_INIT;
            }

            if (generation == mGeneration) {
                mSlots[buf].mGraphicBuffer = graphicBuffer;
                mSlots[buf].mSurfaceDescriptor = desc;
                mSlots[buf].mSurfaceDescriptor.get_SurfaceDescriptorGralloc().external() = true;
                ST_LOGD("dequeueBuffer: returning slot=%d buf=%p ", buf,
                        mSlots[buf].mGraphicBuffer->handle);
            } else {
                tooOld = true;
            }
            //mSlots[*outBuf].mGraphicBuffer = graphicBuffer;
        }

        if (IsSurfaceDescriptorValid(descOld)) {
            ibc->DeallocSurfaceDescriptorGralloc(descOld);
        }

        if (tooOld) {
            ibc->DeallocSurfaceDescriptorGralloc(desc);
        }
    }

    ST_LOGV("dequeueBuffer: returning slot=%d buf=%p flags=%#x", *outBuf,
            mSlots[*outBuf].mGraphicBuffer->handle, returnFlags);

    return returnFlags;
}

status_t GonkBufferQueue::setSynchronousMode(bool enabled) {
    return NO_ERROR;
}

status_t GonkBufferQueue::queueBuffer(int buf,
        const QueueBufferInput& input, QueueBufferOutput* output) {

    Rect crop;
    uint32_t transform;
    int scalingMode;
    int64_t timestamp;
    sp<Fence> fence;

    input.deflate(&timestamp, &crop, &scalingMode, &transform, &fence);

#if ANDROID_VERSION >= 18
    if (fence == NULL) {
        ST_LOGE("queueBuffer: fence is NULL");
        return BAD_VALUE;
    }
#endif

    ST_LOGV("queueBuffer: slot=%d time=%#llx crop=[%d,%d,%d,%d] tr=%#x "
            "scale=%s",
            buf, timestamp, crop.left, crop.top, crop.right, crop.bottom,
            transform, scalingModeName(scalingMode));

    sp<ConsumerListener> listener;

    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        if (mAbandoned) {
            ST_LOGE("queueBuffer: GonkBufferQueue has been abandoned!");
            return NO_INIT;
        }
        int maxBufferCount = getMaxBufferCountLocked();
        if (buf < 0 || buf >= maxBufferCount) {
            ST_LOGE("queueBuffer: slot index out of range [0, %d]: %d",
                    maxBufferCount, buf);
            return -EINVAL;
        } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
            ST_LOGE("queueBuffer: slot %d is not owned by the client "
                    "(state=%d)", buf, mSlots[buf].mBufferState);
            return -EINVAL;
        } else if (!mSlots[buf].mRequestBufferCalled) {
            ST_LOGE("queueBuffer: slot %d was enqueued without requesting a "
                    "buffer", buf);
            return -EINVAL;
        }

        const sp<GraphicBuffer>& graphicBuffer(mSlots[buf].mGraphicBuffer);
        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
        Rect croppedCrop;
        crop.intersect(bufferRect, &croppedCrop);
        if (croppedCrop != crop) {
            ST_LOGE("queueBuffer: crop rect is not contained within the "
                    "buffer in slot %d", buf);
            return -EINVAL;
        }

        if (mSynchronousMode) {
            // In synchronous mode we queue all buffers in a FIFO.
            mQueue.push_back(buf);

            // Synchronous mode always signals that an additional frame should
            // be consumed.
            listener = mConsumerListener;
        } else {
            // In asynchronous mode we only keep the most recent buffer.
            if (mQueue.empty()) {
                mQueue.push_back(buf);

                // Asynchronous mode only signals that a frame should be
                // consumed if no previous frame was pending. If a frame were
                // pending then the consumer would have already been notified.
                listener = mConsumerListener;
            } else {
                Fifo::iterator front(mQueue.begin());
                // buffer currently queued is freed
                mSlots[*front].mBufferState = BufferSlot::FREE;
                // and we record the new buffer index in the queued list
                *front = buf;
            }
        }

        mSlots[buf].mTimestamp = timestamp;
        mSlots[buf].mCrop = crop;
        mSlots[buf].mTransform = transform;
        mSlots[buf].mFence = fence;

        switch (scalingMode) {
            case NATIVE_WINDOW_SCALING_MODE_FREEZE:
            case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
            case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
                break;
            default:
                ST_LOGE("unknown scaling mode: %d (ignoring)", scalingMode);
                scalingMode = mSlots[buf].mScalingMode;
                break;
        }

        mSlots[buf].mBufferState = BufferSlot::QUEUED;
        mSlots[buf].mScalingMode = scalingMode;
        mFrameCounter++;
        mSlots[buf].mFrameNumber = mFrameCounter;

        mBufferHasBeenQueued = true;
        mDequeueCondition.broadcast();

        output->inflate(mDefaultWidth, mDefaultHeight, mTransformHint,
                mQueue.size());
    } // scope for the lock

    // call back without lock held
    if (listener != 0) {
        listener->onFrameAvailable();
    }
    return NO_ERROR;
}

#if ANDROID_VERSION == 17
void GonkBufferQueue::cancelBuffer(int buf, sp<Fence> fence) {
#else
void GonkBufferQueue::cancelBuffer(int buf, const sp<Fence>& fence) {
#endif

    ST_LOGV("cancelBuffer: slot=%d", buf);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGW("cancelBuffer: GonkBufferQueue has been abandoned!");
        return;
    }

    int maxBufferCount = getMaxBufferCountLocked();
    if (buf < 0 || buf >= maxBufferCount) {
        ST_LOGE("cancelBuffer: slot index out of range [0, %d]: %d",
                maxBufferCount, buf);
        return;
    } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
        ST_LOGE("cancelBuffer: slot %d is not owned by the client (state=%d)",
                buf, mSlots[buf].mBufferState);
        return;
#if ANDROID_VERSION >= 18
    } else if (fence == NULL) {
        ST_LOGE("cancelBuffer: fence is NULL");
        return;
#endif
    }
    mSlots[buf].mBufferState = BufferSlot::FREE;
    mSlots[buf].mFrameNumber = 0;
    mSlots[buf].mFence = fence;
    mDequeueCondition.broadcast();
}

status_t GonkBufferQueue::connect(int api, QueueBufferOutput* output) {
    ST_LOGV("connect: api=%d", api);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("connect: GonkBufferQueue has been abandoned!");
        return NO_INIT;
    }

    if (mConsumerListener == NULL) {
        ST_LOGE("connect: GonkBufferQueue has no consumer!");
        return NO_INIT;
    }

    int err = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            if (mConnectedApi != NO_CONNECTED_API) {
                ST_LOGE("connect: already connected (cur=%d, req=%d)",
                        mConnectedApi, api);
                err = -EINVAL;
            } else {
                mConnectedApi = api;
                output->inflate(mDefaultWidth, mDefaultHeight, mTransformHint,
                        mQueue.size());
            }
            break;
        default:
            err = -EINVAL;
            break;
    }

    mBufferHasBeenQueued = false;

    return err;
}

status_t GonkBufferQueue::disconnect(int api) {
    ST_LOGV("disconnect: api=%d", api);

    int err = NO_ERROR;
    sp<ConsumerListener> listener;
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;

    { // Scope for the lock
        Mutex::Autolock lock(mMutex);

        if (mAbandoned) {
            // it is not really an error to disconnect after the surface
            // has been abandoned, it should just be a no-op.
            return NO_ERROR;
        }

        switch (api) {
            case NATIVE_WINDOW_API_EGL:
            case NATIVE_WINDOW_API_CPU:
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                if (mConnectedApi == api) {
                    freeAllBuffersLocked(freeList);
                    mConnectedApi = NO_CONNECTED_API;
                    mDequeueCondition.broadcast();
                    listener = mConsumerListener;
                } else {
                    ST_LOGE("disconnect: connected to another api (cur=%d, req=%d)",
                            mConnectedApi, api);
                    err = -EINVAL;
                }
                break;
            default:
                ST_LOGE("disconnect: unknown API %d", api);
                err = -EINVAL;
                break;
        }
    }
    releaseBufferFreeListUnlocked(freeList);

    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return err;
}

void GonkBufferQueue::dump(String8& result) const
{
    char buffer[1024];
    GonkBufferQueue::dump(result, "", buffer, 1024);
}

void GonkBufferQueue::dump(String8& result, const char* prefix,
        char* buffer, size_t SIZE) const
{
    Mutex::Autolock _l(mMutex);

    String8 fifo;
    int fifoSize = 0;
    Fifo::const_iterator i(mQueue.begin());
    while (i != mQueue.end()) {
       snprintf(buffer, SIZE, "%02d ", *i++);
       fifoSize++;
       fifo.append(buffer);
    }

    int maxBufferCount = getMaxBufferCountLocked();

    snprintf(buffer, SIZE,
            "%s-BufferQueue maxBufferCount=%d, mSynchronousMode=%d, default-size=[%dx%d], "
            "default-format=%d, transform-hint=%02x, FIFO(%d)={%s}\n",
            prefix, maxBufferCount, mSynchronousMode, mDefaultWidth,
            mDefaultHeight, mDefaultBufferFormat, mTransformHint,
            fifoSize, fifo.string());
    result.append(buffer);


    struct {
        const char * operator()(int state) const {
            switch (state) {
                case BufferSlot::DEQUEUED: return "DEQUEUED";
                case BufferSlot::QUEUED: return "QUEUED";
                case BufferSlot::FREE: return "FREE";
                case BufferSlot::ACQUIRED: return "ACQUIRED";
                default: return "Unknown";
            }
        }
    } stateName;

    for (int i=0 ; i<maxBufferCount ; i++) {
        const BufferSlot& slot(mSlots[i]);
        snprintf(buffer, SIZE,
                "%s%s[%02d] "
                "state=%-8s, crop=[%d,%d,%d,%d], "
                "xform=0x%02x, time=%#llx, scale=%s",
                prefix, (slot.mBufferState == BufferSlot::ACQUIRED)?">":" ", i,
                stateName(slot.mBufferState),
                slot.mCrop.left, slot.mCrop.top, slot.mCrop.right,
                slot.mCrop.bottom, slot.mTransform, slot.mTimestamp,
                scalingModeName(slot.mScalingMode)
        );
        result.append(buffer);

        const sp<GraphicBuffer>& buf(slot.mGraphicBuffer);
        if (buf != NULL) {
            snprintf(buffer, SIZE,
                    ", %p [%4ux%4u:%4u,%3X]",
                    buf->handle, buf->width, buf->height, buf->stride,
                    buf->format);
            result.append(buffer);
        }
        result.append("\n");
    }
}

void GonkBufferQueue::releaseBufferFreeListUnlocked(nsTArray<SurfaceDescriptor>& freeList)
{
    // This function MUST ONLY be called with mMutex unlocked; else there
    // is a risk of deadlock with the ImageBridge thread.

    ST_LOGD("releaseBufferFreeListUnlocked: E");
    ImageBridgeChild *ibc = ImageBridgeChild::GetSingleton();

    for (uint32_t i = 0; i < freeList.Length(); ++i) {
        ibc->DeallocSurfaceDescriptorGralloc(freeList[i]);
    }

    freeList.Clear();
    ST_LOGD("releaseBufferFreeListUnlocked: X");
}

void GonkBufferQueue::freeAllBuffersLocked(nsTArray<SurfaceDescriptor>& freeList)
{
    ALOGW_IF(!mQueue.isEmpty(),
            "freeAllBuffersLocked called but mQueue is not empty");
    ++mGeneration;
    mQueue.clear();
    mBufferHasBeenQueued = false;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].mGraphicBuffer.get() && mSlots[i].mBufferState != BufferSlot::ACQUIRED) {
            SurfaceDescriptor* desc = freeList.AppendElement();
            *desc = mSlots[i].mSurfaceDescriptor;
        }
        mSlots[i].mGraphicBuffer = 0;
        mSlots[i].mSurfaceDescriptor = SurfaceDescriptor();
        if (mSlots[i].mBufferState == BufferSlot::ACQUIRED) {
            mSlots[i].mNeedsCleanupOnRelease = true;
        }
        mSlots[i].mBufferState = BufferSlot::FREE;
        mSlots[i].mFrameNumber = 0;
        mSlots[i].mAcquireCalled = false;
        // destroy fence as GonkBufferQueue now takes ownership
        mSlots[i].mFence = Fence::NO_FENCE;
    }
}

status_t GonkBufferQueue::acquireBuffer(BufferItem *buffer) {
    Mutex::Autolock _l(mMutex);

    // Check that the consumer doesn't currently have the maximum number of
    // buffers acquired.  We allow the max buffer count to be exceeded by one
    // buffer, so that the consumer can successfully set up the newly acquired
    // buffer before releasing the old one.
    int numAcquiredBuffers = 0;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].mBufferState == BufferSlot::ACQUIRED) {
            numAcquiredBuffers++;
        }
    }
    if (numAcquiredBuffers >= mMaxAcquiredBufferCount+1) {
        ST_LOGE("acquireBuffer: max acquired buffer count reached: %d (max=%d)",
                numAcquiredBuffers, mMaxAcquiredBufferCount);
        return INVALID_OPERATION;
    }

    // check if queue is empty
    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    if (!mQueue.empty()) {
        Fifo::iterator front(mQueue.begin());
        int buf = *front;

        // In android, when the buffer is aquired by BufferConsumer,
        // BufferQueue releases a reference to the buffer and
        // it's ownership moves to the BufferConsumer.
        // In b2g, GonkBufferQueue continues to have a buffer ownership.
        // It is necessary to free buffer via ImageBridgeChild.

        //if (mSlots[buf].mAcquireCalled) {
        //    buffer->mGraphicBuffer = NULL;
        //} else {
        //    buffer->mGraphicBuffer = mSlots[buf].mGraphicBuffer;
        //}
        buffer->mGraphicBuffer = mSlots[buf].mGraphicBuffer;
        buffer->mSurfaceDescriptor = mSlots[buf].mSurfaceDescriptor;
        buffer->mCrop = mSlots[buf].mCrop;
        buffer->mTransform = mSlots[buf].mTransform;
        buffer->mScalingMode = mSlots[buf].mScalingMode;
        buffer->mFrameNumber = mSlots[buf].mFrameNumber;
        buffer->mTimestamp = mSlots[buf].mTimestamp;
        buffer->mBuf = buf;
        buffer->mFence = mSlots[buf].mFence;

        mSlots[buf].mAcquireCalled = true;
        mSlots[buf].mNeedsCleanupOnRelease = false;
        mSlots[buf].mBufferState = BufferSlot::ACQUIRED;
        mSlots[buf].mFence = Fence::NO_FENCE;

        mQueue.erase(front);
        mDequeueCondition.broadcast();
    } else {
        return NO_BUFFER_AVAILABLE;
    }

    return NO_ERROR;
}

status_t GonkBufferQueue::releaseBuffer(int buf, const sp<Fence>& fence) {
    Mutex::Autolock _l(mMutex);

#if ANDROID_VERSION == 17
    if (buf == INVALID_BUFFER_SLOT) {
#else
    if (buf == INVALID_BUFFER_SLOT || fence == NULL) {
#endif
        return BAD_VALUE;
    }

    mSlots[buf].mFence = fence;

    // The buffer can now only be released if its in the acquired state
    if (mSlots[buf].mBufferState == BufferSlot::ACQUIRED) {
        mSlots[buf].mBufferState = BufferSlot::FREE;
    } else if (mSlots[buf].mNeedsCleanupOnRelease) {
        ST_LOGV("releasing a stale buf %d its state was %d", buf, mSlots[buf].mBufferState);
        mSlots[buf].mNeedsCleanupOnRelease = false;
        return STALE_BUFFER_SLOT;
    } else {
        ST_LOGE("attempted to release buf %d but its state was %d", buf, mSlots[buf].mBufferState);
        return -EINVAL;
    }

    mDequeueCondition.broadcast();
    return NO_ERROR;
}

status_t GonkBufferQueue::consumerConnect(const sp<ConsumerListener>& consumerListener) {
    ST_LOGV("consumerConnect");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("consumerConnect: GonkBufferQueue has been abandoned!");
        return NO_INIT;
    }
    if (consumerListener == NULL) {
        ST_LOGE("consumerConnect: consumerListener may not be NULL");
        return BAD_VALUE;
    }

    mConsumerListener = consumerListener;

    return NO_ERROR;
}

status_t GonkBufferQueue::consumerDisconnect() {
    ST_LOGV("consumerDisconnect");
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    {
        Mutex::Autolock lock(mMutex);

        if (mConsumerListener == NULL) {
            ST_LOGE("consumerDisconnect: No consumer is connected!");
            return -EINVAL;
        }

        mAbandoned = true;
        mConsumerListener = NULL;
        mQueue.clear();
        freeAllBuffersLocked(freeList);
    }
    releaseBufferFreeListUnlocked(freeList);
    mDequeueCondition.broadcast();

    return NO_ERROR;
}

status_t GonkBufferQueue::getReleasedBuffers(uint32_t* slotMask) {
    ST_LOGV("getReleasedBuffers");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("getReleasedBuffers: GonkBufferQueue has been abandoned!");
        return NO_INIT;
    }

    uint32_t mask = 0;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (!mSlots[i].mAcquireCalled) {
            mask |= 1 << i;
        }
    }
    *slotMask = mask;

    ST_LOGV("getReleasedBuffers: returning mask %#x", mask);
    return NO_ERROR;
}

status_t GonkBufferQueue::setDefaultBufferSize(uint32_t w, uint32_t h)
{
    ST_LOGV("setDefaultBufferSize: w=%d, h=%d", w, h);
    if (!w || !h) {
        ST_LOGE("setDefaultBufferSize: dimensions cannot be 0 (w=%d, h=%d)",
                w, h);
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);
    mDefaultWidth = w;
    mDefaultHeight = h;
    return NO_ERROR;
}

status_t GonkBufferQueue::setDefaultMaxBufferCount(int bufferCount) {
    Mutex::Autolock lock(mMutex);
    return setDefaultMaxBufferCountLocked(bufferCount);
}

status_t GonkBufferQueue::setMaxAcquiredBufferCount(int maxAcquiredBuffers) {
    Mutex::Autolock lock(mMutex);
    if (maxAcquiredBuffers < 1 || maxAcquiredBuffers > MAX_MAX_ACQUIRED_BUFFERS) {
        ST_LOGE("setMaxAcquiredBufferCount: invalid count specified: %d",
                maxAcquiredBuffers);
        return BAD_VALUE;
    }
    if (mConnectedApi != NO_CONNECTED_API) {
        return INVALID_OPERATION;
    }
    mMaxAcquiredBufferCount = maxAcquiredBuffers;
    return NO_ERROR;
}

int GonkBufferQueue::getMinMaxBufferCountLocked() const {
    return getMinUndequeuedBufferCountLocked() + 1;
}

int GonkBufferQueue::getMinUndequeuedBufferCountLocked() const {
    return mSynchronousMode ? mMaxAcquiredBufferCount :
            mMaxAcquiredBufferCount + 1;
}

int GonkBufferQueue::getMaxBufferCountLocked() const {
    int minMaxBufferCount = getMinMaxBufferCountLocked();

    int maxBufferCount = mDefaultMaxBufferCount;
    if (maxBufferCount < minMaxBufferCount) {
        maxBufferCount = minMaxBufferCount;
    }
    if (mOverrideMaxBufferCount != 0) {
        assert(mOverrideMaxBufferCount >= minMaxBufferCount);
        maxBufferCount = mOverrideMaxBufferCount;
    }

    // Any buffers that are dequeued by the producer or sitting in the queue
    // waiting to be consumed need to have their slots preserved.  Such
    // buffers will temporarily keep the max buffer count up until the slots
    // no longer need to be preserved.
    for (int i = maxBufferCount; i < NUM_BUFFER_SLOTS; i++) {
        BufferSlot::BufferState state = mSlots[i].mBufferState;
        if (state == BufferSlot::QUEUED || state == BufferSlot::DEQUEUED) {
            maxBufferCount = i + 1;
        }
    }

    return maxBufferCount;
}

GonkBufferQueue::ProxyConsumerListener::ProxyConsumerListener(
        const wp<GonkBufferQueue::ConsumerListener>& consumerListener):
        mConsumerListener(consumerListener) {}

GonkBufferQueue::ProxyConsumerListener::~ProxyConsumerListener() {}

void GonkBufferQueue::ProxyConsumerListener::onFrameAvailable() {
    sp<GonkBufferQueue::ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable();
    }
}

void GonkBufferQueue::ProxyConsumerListener::onBuffersReleased() {
    sp<GonkBufferQueue::ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onBuffersReleased();
    }
}

}; // namespace android
