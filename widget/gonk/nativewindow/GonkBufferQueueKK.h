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

#ifndef NATIVEWINDOW_GONKBUFFERQUEUE_KK_H
#define NATIVEWINDOW_GONKBUFFERQUEUE_KK_H

#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/IGraphicBufferProducer.h>
#include "IGonkGraphicBufferConsumer.h"

#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>

#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/threads.h>

#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/TextureClient.h"

namespace android {
// ----------------------------------------------------------------------------

class GonkBufferQueue : public BnGraphicBufferProducer,
                        public BnGonkGraphicBufferConsumer,
                        private IBinder::DeathRecipient
{
    typedef mozilla::layers::TextureClient TextureClient;

public:
    enum { MIN_UNDEQUEUED_BUFFERS = 2 };
    enum { NUM_BUFFER_SLOTS = 32 };
    enum { NO_CONNECTED_API = 0 };
    enum { INVALID_BUFFER_SLOT = -1 };
    enum { STALE_BUFFER_SLOT = 1, NO_BUFFER_AVAILABLE, PRESENT_LATER };

    // When in async mode we reserve two slots in order to guarantee that the
    // producer and consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = NUM_BUFFER_SLOTS - 2 };

    // for backward source compatibility
    typedef ::android::ConsumerListener ConsumerListener;

    // ProxyConsumerListener is a ConsumerListener implementation that keeps a weak
    // reference to the actual consumer object.  It forwards all calls to that
    // consumer object so long as it exists.
    //
    // This class exists to avoid having a circular reference between the
    // GonkBufferQueue object and the consumer object.  The reason this can't be a weak
    // reference in the GonkBufferQueue class is because we're planning to expose the
    // consumer side of a GonkBufferQueue as a binder interface, which doesn't support
    // weak references.
    class ProxyConsumerListener : public BnConsumerListener {
    public:
        ProxyConsumerListener(const wp<ConsumerListener>& consumerListener);
        virtual ~ProxyConsumerListener();
        virtual void onFrameAvailable();
        virtual void onBuffersReleased();
    private:
        // mConsumerListener is a weak reference to the IConsumerListener.  This is
        // the raison d'etre of ProxyConsumerListener.
        wp<ConsumerListener> mConsumerListener;
    };


    // BufferQueue manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the
    // needed gralloc buffers.
    GonkBufferQueue(bool allowSynchronousMode = true,
            const sp<IGraphicBufferAlloc>& allocator = NULL);
    virtual ~GonkBufferQueue();

    /*
     * IBinder::DeathRecipient interface
     */

    virtual void binderDied(const wp<IBinder>& who);

    /*
     * IGraphicBufferProducer interface
     */

    // Query native window attributes.  The "what" values are enumerated in
    // window.h (e.g. NATIVE_WINDOW_FORMAT).
    virtual int query(int what, int* value);

    // setBufferCount updates the number of available buffer slots.  If this
    // method succeeds, buffer slots will be both unallocated and owned by
    // the GonkBufferQueue object (i.e. they are not owned by the producer or
    // consumer).
    //
    // This will fail if the producer has dequeued any buffers, or if
    // bufferCount is invalid.  bufferCount must generally be a value
    // between the minimum undequeued buffer count and NUM_BUFFER_SLOTS
    // (inclusive).  It may also be set to zero (the default) to indicate
    // that the producer does not wish to set a value.  The minimum value
    // can be obtained by calling query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
    // ...).
    //
    // This may only be called by the producer.  The consumer will be told
    // to discard buffers through the onBuffersReleased callback.
    virtual status_t setBufferCount(int bufferCount);

    // requestBuffer returns the GraphicBuffer for slot N.
    //
    // In normal operation, this is called the first time slot N is returned
    // by dequeueBuffer.  It must be called again if dequeueBuffer returns
    // flags indicating that previously-returned buffers are no longer valid.
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf);

    // dequeueBuffer gets the next buffer slot index for the producer to use.
    // If a buffer slot is available then that slot index is written to the
    // location pointed to by the buf argument and a status of OK is returned.
    // If no slot is available then a status of -EBUSY is returned and buf is
    // unmodified.
    //
    // The fence parameter will be updated to hold the fence associated with
    // the buffer. The contents of the buffer must not be overwritten until the
    // fence signals. If the fence is Fence::NO_FENCE, the buffer may be
    // written immediately.
    //
    // The width and height parameters must be no greater than the minimum of
    // GL_MAX_VIEWPORT_DIMS and GL_MAX_TEXTURE_SIZE (see: glGetIntegerv).
    // An error due to invalid dimensions might not be reported until
    // updateTexImage() is called.  If width and height are both zero, the
    // default values specified by setDefaultBufferSize() are used instead.
    //
    // The pixel formats are enumerated in graphics.h, e.g.
    // HAL_PIXEL_FORMAT_RGBA_8888.  If the format is 0, the default format
    // will be used.
    //
    // The usage argument specifies gralloc buffer usage flags.  The values
    // are enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER.  These
    // will be merged with the usage flags specified by setConsumerUsageBits.
    //
    // The return value may be a negative error value or a non-negative
    // collection of flags.  If the flags are set, the return values are
    // valid, but additional actions must be performed.
    //
    // If IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION is set, the
    // producer must discard cached GraphicBuffer references for the slot
    // returned in buf.
    // If IGraphicBufferProducer::RELEASE_ALL_BUFFERS is set, the producer
    // must discard cached GraphicBuffer references for all slots.
    //
    // In both cases, the producer will need to call requestBuffer to get a
    // GraphicBuffer handle for the returned slot.
    virtual status_t dequeueBuffer(int *buf, sp<Fence>* fence, bool async,
            uint32_t width, uint32_t height, uint32_t format, uint32_t usage);

    // queueBuffer returns a filled buffer to the GonkBufferQueue.
    //
    // Additional data is provided in the QueueBufferInput struct.  Notably,
    // a timestamp must be provided for the buffer. The timestamp is in
    // nanoseconds, and must be monotonically increasing. Its other semantics
    // (zero point, etc) are producer-specific and should be documented by the
    // producer.
    //
    // The caller may provide a fence that signals when all rendering
    // operations have completed.  Alternatively, NO_FENCE may be used,
    // indicating that the buffer is ready immediately.
    //
    // Some values are returned in the output struct: the current settings
    // for default width and height, the current transform hint, and the
    // number of queued buffers.
    virtual status_t queueBuffer(int buf,
            const QueueBufferInput& input, QueueBufferOutput* output);

    // cancelBuffer returns a dequeued buffer to the GonkBufferQueue, but doesn't
    // queue it for use by the consumer.
    //
    // The buffer will not be overwritten until the fence signals.  The fence
    // will usually be the one obtained from dequeueBuffer.
    virtual void cancelBuffer(int buf, const sp<Fence>& fence);

    // setSynchronousMode sets whether dequeueBuffer is synchronous or
    // asynchronous. In synchronous mode, dequeueBuffer blocks until
    // a buffer is available, the currently bound buffer can be dequeued and
    // queued buffers will be acquired in order.  In asynchronous mode,
    // a queued buffer may be replaced by a subsequently queued buffer.
    //
    // The default mode is synchronous.
    // This should be called only during initialization.
    virtual status_t setSynchronousMode(bool enabled);

    // connect attempts to connect a producer API to the GonkBufferQueue.  This
    // must be called before any other IGraphicBufferProducer methods are
    // called except for getAllocator.  A consumer must already be connected.
    //
    // This method will fail if connect was previously called on the
    // GonkBufferQueue and no corresponding disconnect call was made (i.e. if
    // it's still connected to a producer).
    //
    // APIs are enumerated in window.h (e.g. NATIVE_WINDOW_API_CPU).
    virtual status_t connect(const sp<IBinder>& token,
            int api, bool producerControlledByApp, QueueBufferOutput* output);

    // disconnect attempts to disconnect a producer API from the GonkBufferQueue.
    // Calling this method will cause any subsequent calls to other
    // IGraphicBufferProducer methods to fail except for getAllocator and connect.
    // Successfully calling connect after this will allow the other methods to
    // succeed again.
    //
    // This method will fail if the the GonkBufferQueue is not currently
    // connected to the specified producer API.
    virtual status_t disconnect(int api);

    /*
     * IGraphicBufferConsumer interface
     */

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the GonkBufferQueue.  If no buffer is pending then it returns -EINVAL.  If a
    // buffer is successfully acquired, the information about the buffer is
    // returned in BufferItem.  If the buffer returned had previously been
    // acquired then the BufferItem::mGraphicBuffer field of buffer is set to
    // NULL and it is assumed that the consumer still holds a reference to the
    // buffer.
    //
    // If presentWhen is nonzero, it indicates the time when the buffer will
    // be displayed on screen.  If the buffer's timestamp is farther in the
    // future, the buffer won't be acquired, and PRESENT_LATER will be
    // returned.  The presentation time is in nanoseconds, and the time base
    // is CLOCK_MONOTONIC.
    virtual status_t acquireBuffer(BufferItem *buffer, nsecs_t presentWhen);

    // releaseBuffer releases a buffer slot from the consumer back to the
    // GonkBufferQueue.  This may be done while the buffer's contents are still
    // being accessed.  The fence will signal when the buffer is no longer
    // in use. frameNumber is used to indentify the exact buffer returned.
    //
    // If releaseBuffer returns STALE_BUFFER_SLOT, then the consumer must free
    // any references to the just-released buffer that it might have, as if it
    // had received a onBuffersReleased() call with a mask set for the released
    // buffer.
    //
    // Note that the dependencies on EGL will be removed once we switch to using
    // the Android HW Sync HAL.
    virtual status_t releaseBuffer(int buf, uint64_t frameNumber,
                    const sp<Fence>& releaseFence);

    // consumerConnect connects a consumer to the GonkBufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // GonkBufferQueue is placed into the "abandoned" state, causing most
    // interactions with the GonkBufferQueue by the producer to fail.
    // controlledByApp indicates whether the consumer is controlled by
    // the application.
    //
    // consumer may not be NULL.
    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer, bool controlledByApp);

    // consumerDisconnect disconnects a consumer from the GonkBufferQueue. All
    // buffers will be freed and the GonkBufferQueue is placed in the "abandoned"
    // state, causing most interactions with the GonkBufferQueue by the producer to
    // fail.
    virtual status_t consumerDisconnect();

    // getReleasedBuffers sets the value pointed to by slotMask to a bit mask
    // indicating which buffer slots have been released by the GonkBufferQueue
    // but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    virtual status_t getReleasedBuffers(uint32_t* slotMask);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    virtual status_t setDefaultBufferSize(uint32_t w, uint32_t h);

    // setDefaultMaxBufferCount sets the default value for the maximum buffer
    // count (the initial default is 2). If the producer has requested a
    // buffer count using setBufferCount, the default buffer count will only
    // take effect if the producer sets the count back to zero.
    //
    // The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    virtual status_t setDefaultMaxBufferCount(int bufferCount);

    // disableAsyncBuffer disables the extra buffer used in async mode
    // (when both producer and consumer have set their "isControlledByApp"
    // flag) and has dequeueBuffer() return WOULD_BLOCK instead.
    //
    // This can only be called before consumerConnect().
    virtual status_t disableAsyncBuffer();

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1).  This call will
    // fail if a producer is connected to the GonkBufferQueue.
    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers);

    // setConsumerName sets the name used in logging
    virtual void setConsumerName(const String8& name);

    // setDefaultBufferFormat allows the GonkBufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer.  Formats are enumerated in graphics.h; the
    // initial default is HAL_PIXEL_FORMAT_RGBA_8888.
    virtual status_t setDefaultBufferFormat(uint32_t defaultFormat);

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    virtual status_t setConsumerUsageBits(uint32_t usage);

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    virtual status_t setTransformHint(uint32_t hint);

    // dump our state in a String
    virtual void dumpToString(String8& result, const char* prefix) const;

     already_AddRefed<TextureClient> getTextureClientFromBuffer(ANativeWindowBuffer* buffer);

    int getSlotFromTextureClientLocked(TextureClient* client) const;

private:
    // freeBufferLocked frees the GraphicBuffer and sync resources for the
    // given slot.
    //void freeBufferLocked(int index);

    // freeAllBuffersLocked frees the GraphicBuffer and sync resources for
    // all slots.
    void freeAllBuffersLocked();

    // setDefaultMaxBufferCountLocked sets the maximum number of buffer slots
    // that will be used if the producer does not override the buffer slot
    // count.  The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    // The initial default is 2.
    status_t setDefaultMaxBufferCountLocked(int count);

    // getMinUndequeuedBufferCount returns the minimum number of buffers
    // that must remain in a state other than DEQUEUED.
    // The async parameter tells whether we're in asynchronous mode.
    int getMinUndequeuedBufferCount(bool async) const;

    // getMinBufferCountLocked returns the minimum number of buffers allowed
    // given the current GonkBufferQueue state.
    // The async parameter tells whether we're in asynchronous mode.
    int getMinMaxBufferCountLocked(bool async) const;

    // getMaxBufferCountLocked returns the maximum number of buffers that can
    // be allocated at once.  This value depends upon the following member
    // variables:
    //
    //      mDequeueBufferCannotBlock
    //      mMaxAcquiredBufferCount
    //      mDefaultMaxBufferCount
    //      mOverrideMaxBufferCount
    //      async parameter
    //
    // Any time one of these member variables is changed while a producer is
    // connected, mDequeueCondition must be broadcast.
    int getMaxBufferCountLocked(bool async) const;

    // stillTracking returns true iff the buffer item is still being tracked
    // in one of the slots.
    bool stillTracking(const BufferItem *item) const;

    struct BufferSlot {

        BufferSlot()
        : mBufferState(BufferSlot::FREE),
          mRequestBufferCalled(false),
          mFrameNumber(0),
          mAcquireCalled(false),
          mNeedsCleanupOnRelease(false) {
        }

        // mGraphicBuffer points to the buffer allocated for this slot or is NULL
        // if no buffer has been allocated.
        sp<GraphicBuffer> mGraphicBuffer;

        // mTextureClient is a thin abstraction over remotely allocated GraphicBuffer.
        RefPtr<TextureClient> mTextureClient;

        // BufferState represents the different states in which a buffer slot
        // can be.  All slots are initially FREE.
        enum BufferState {
            // FREE indicates that the buffer is available to be dequeued
            // by the producer.  The buffer may be in use by the consumer for
            // a finite time, so the buffer must not be modified until the
            // associated fence is signaled.
            //
            // The slot is "owned" by GonkBufferQueue.  It transitions to DEQUEUED
            // when dequeueBuffer is called.
            FREE = 0,

            // DEQUEUED indicates that the buffer has been dequeued by the
            // producer, but has not yet been queued or canceled.  The
            // producer may modify the buffer's contents as soon as the
            // associated ready fence is signaled.
            //
            // The slot is "owned" by the producer.  It can transition to
            // QUEUED (via queueBuffer) or back to FREE (via cancelBuffer).
            DEQUEUED = 1,

            // QUEUED indicates that the buffer has been filled by the
            // producer and queued for use by the consumer.  The buffer
            // contents may continue to be modified for a finite time, so
            // the contents must not be accessed until the associated fence
            // is signaled.
            //
            // The slot is "owned" by GonkBufferQueue.  It can transition to
            // ACQUIRED (via acquireBuffer) or to FREE (if another buffer is
            // queued in asynchronous mode).
            QUEUED = 2,

            // ACQUIRED indicates that the buffer has been acquired by the
            // consumer.  As with QUEUED, the contents must not be accessed
            // by the consumer until the fence is signaled.
            //
            // The slot is "owned" by the consumer.  It transitions to FREE
            // when releaseBuffer is called.
            ACQUIRED = 3
        };

        // mBufferState is the current state of this buffer slot.
        BufferState mBufferState;

        // mRequestBufferCalled is used for validating that the producer did
        // call requestBuffer() when told to do so. Technically this is not
        // needed but useful for debugging and catching producer bugs.
        bool mRequestBufferCalled;

        // mFrameNumber is the number of the queued frame for this slot.  This
        // is used to dequeue buffers in LRU order (useful because buffers
        // may be released before their release fence is signaled).
        uint64_t mFrameNumber;

        // mFence is a fence which will signal when work initiated by the
        // previous owner of the buffer is finished. When the buffer is FREE,
        // the fence indicates when the consumer has finished reading
        // from the buffer, or when the producer has finished writing if it
        // called cancelBuffer after queueing some writes. When the buffer is
        // QUEUED, it indicates when the producer has finished filling the
        // buffer. When the buffer is DEQUEUED or ACQUIRED, the fence has been
        // passed to the consumer or producer along with ownership of the
        // buffer, and mFence is set to NO_FENCE.
        sp<Fence> mFence;

        // Indicates whether this buffer has been seen by a consumer yet
        bool mAcquireCalled;

        // Indicates whether this buffer needs to be cleaned up by the
        // consumer.  This is set when a buffer in ACQUIRED state is freed.
        // It causes releaseBuffer to return STALE_BUFFER_SLOT.
        bool mNeedsCleanupOnRelease;
    };

    // mSlots is the array of buffer slots that must be mirrored on the
    // producer side. This allows buffer ownership to be transferred between
    // the producer and consumer without sending a GraphicBuffer over binder.
    // The entire array is initialized to NULL at construction time, and
    // buffers are allocated for a slot when requestBuffer is called with
    // that slot's index.
    BufferSlot mSlots[NUM_BUFFER_SLOTS];

    // mDefaultWidth holds the default width of allocated buffers. It is used
    // in dequeueBuffer() if a width and height of zero is specified.
    uint32_t mDefaultWidth;

    // mDefaultHeight holds the default height of allocated buffers. It is used
    // in dequeueBuffer() if a width and height of zero is specified.
    uint32_t mDefaultHeight;

    // mMaxAcquiredBufferCount is the number of buffers that the consumer may
    // acquire at one time.  It defaults to 1 and can be changed by the
    // consumer via the setMaxAcquiredBufferCount method, but this may only be
    // done when no producer is connected to the GonkBufferQueue.
    //
    // This value is used to derive the value returned for the
    // MIN_UNDEQUEUED_BUFFERS query by the producer.
    int mMaxAcquiredBufferCount;

    // mDefaultMaxBufferCount is the default limit on the number of buffers
    // that will be allocated at one time.  This default limit is set by the
    // consumer.  The limit (as opposed to the default limit) may be
    // overridden by the producer.
    int mDefaultMaxBufferCount;

    // mOverrideMaxBufferCount is the limit on the number of buffers that will
    // be allocated at one time. This value is set by the image producer by
    // calling setBufferCount. The default is zero, which means the producer
    // doesn't care about the number of buffers in the pool. In that case
    // mDefaultMaxBufferCount is used as the limit.
    int mOverrideMaxBufferCount;

    // mGraphicBufferAlloc is the connection to SurfaceFlinger that is used to
    // allocate new GraphicBuffer objects.
    sp<IGraphicBufferAlloc> mGraphicBufferAlloc;

    // mConsumerListener is used to notify the connected consumer of
    // asynchronous events that it may wish to react to.  It is initially set
    // to NULL and is written by consumerConnect and consumerDisconnect.
    sp<IConsumerListener> mConsumerListener;

    // mSynchronousMode whether we're in synchronous mode or not
    bool mSynchronousMode;

    // mConsumerControlledByApp whether the connected consumer is controlled by the
    // application.
    bool mConsumerControlledByApp;

    // mDequeueBufferCannotBlock whether dequeueBuffer() isn't allowed to block.
    // this flag is set during connect() when both consumer and producer are controlled
    // by the application.
    bool mDequeueBufferCannotBlock;

    // mUseAsyncBuffer whether an extra buffer is used in async mode to prevent
    // dequeueBuffer() from ever blocking.
    bool mUseAsyncBuffer;

    // mConnectedApi indicates the producer API that is currently connected
    // to this GonkBufferQueue.  It defaults to NO_CONNECTED_API (= 0), and gets
    // updated by the connect and disconnect methods.
    int mConnectedApi;

    // mDequeueCondition condition used for dequeueBuffer in synchronous mode
    mutable Condition mDequeueCondition;

    // mQueue is a FIFO of queued buffers used in synchronous mode
    typedef Vector<BufferItem> Fifo;
    Fifo mQueue;

    // mAbandoned indicates that the GonkBufferQueue will no longer be used to
    // consume image buffers pushed to it using the IGraphicBufferProducer
    // interface.  It is initialized to false, and set to true in the
    // consumerDisconnect method.  A GonkBufferQueue that has been abandoned will
    // return the NO_INIT error from all IGraphicBufferProducer methods
    // capable of returning an error.
    bool mAbandoned;

    // mConsumerName is a string used to identify the GonkBufferQueue in log
    // messages.  It is set by the setConsumerName method.
    String8 mConsumerName;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of GonkBufferQueue objects. It must be locked whenever the
    // member variables are accessed.
    mutable Mutex mMutex;

    // mFrameCounter is the free running counter, incremented on every
    // successful queueBuffer call, and buffer allocation.
    uint64_t mFrameCounter;

    // mBufferHasBeenQueued is true once a buffer has been queued.  It is
    // reset when something causes all buffers to be freed (e.g. changing the
    // buffer count).
    bool mBufferHasBeenQueued;

    // mDefaultBufferFormat can be set so it will override
    // the buffer format when it isn't specified in dequeueBuffer
    uint32_t mDefaultBufferFormat;

    // mConsumerUsageBits contains flags the consumer wants for GraphicBuffers
    uint32_t mConsumerUsageBits;

    // mTransformHint is used to optimize for screen rotations
    uint32_t mTransformHint;

    // mConnectedProducerToken is used to set a binder death notification on the producer
    sp<IBinder> mConnectedProducerToken;
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_BUFFERQUEUE_H
