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

#ifndef NATIVEWINDOW_GONKBUFFERQUEUE_H
#define NATIVEWINDOW_GONKBUFFERQUEUE_H

#include <gui/IGraphicBufferAlloc.h>
#if ANDROID_VERSION == 17
#include <gui/ISurfaceTexture.h>
#else
#include <gui/IGraphicBufferProducer.h>
#endif

#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>

#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/threads.h>

#include "mozilla/layers/LayersSurfaces.h"

#if ANDROID_VERSION == 17
#define IGraphicBufferProducer ISurfaceTexture
#endif

namespace android {
// ----------------------------------------------------------------------------

#if ANDROID_VERSION == 17
class GonkBufferQueue : public BnSurfaceTexture {
#else
class GonkBufferQueue : public BnGraphicBufferProducer {
#endif
    typedef mozilla::layers::SurfaceDescriptor SurfaceDescriptor;

public:
    enum { MIN_UNDEQUEUED_BUFFERS = 2 };
    enum { NUM_BUFFER_SLOTS = 32 };
    enum { NO_CONNECTED_API = 0 };
    enum { INVALID_BUFFER_SLOT = -1 };
    enum { STALE_BUFFER_SLOT = 1, NO_BUFFER_AVAILABLE };

    // When in async mode we reserve two slots in order to guarantee that the
    // producer and consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = NUM_BUFFER_SLOTS - 2 };

    // ConsumerListener is the interface through which the GonkBufferQueue notifies
    // the consumer of events that the consumer may wish to react to.  Because
    // the consumer will generally have a mutex that is locked during calls from
    // the consumer to the GonkBufferQueue, these calls from the GonkBufferQueue to the
    // consumer *MUST* be called only when the GonkBufferQueue mutex is NOT locked.
    struct ConsumerListener : public virtual RefBase {
        // onFrameAvailable is called from queueBuffer each time an additional
        // frame becomes available for consumption. This means that frames that
        // are queued while in asynchronous mode only trigger the callback if no
        // previous frames are pending. Frames queued while in synchronous mode
        // always trigger the callback.
        //
        // This is called without any lock held and can be called concurrently
        // by multiple threads.
        virtual void onFrameAvailable() = 0;

        // onBuffersReleased is called to notify the buffer consumer that the
        // GonkBufferQueue has released its references to one or more GraphicBuffers
        // contained in its slots.  The buffer consumer should then call
        // GonkBufferQueue::getReleasedBuffers to retrieve the list of buffers
        //
        // This is called without any lock held and can be called concurrently
        // by multiple threads.
        virtual void onBuffersReleased() = 0;
    };

    // ProxyConsumerListener is a ConsumerListener implementation that keeps a weak
    // reference to the actual consumer object.  It forwards all calls to that
    // consumer object so long as it exists.
    //
    // This class exists to avoid having a circular reference between the
    // GonkBufferQueue object and the consumer object.  The reason this can't be a weak
    // reference in the GonkBufferQueue class is because we're planning to expose the
    // consumer side of a GonkBufferQueue as a binder interface, which doesn't support
    // weak references.
    class ProxyConsumerListener : public GonkBufferQueue::ConsumerListener {
    public:

        ProxyConsumerListener(const wp<GonkBufferQueue::ConsumerListener>& consumerListener);
        virtual ~ProxyConsumerListener();
        virtual void onFrameAvailable();
        virtual void onBuffersReleased();

    private:

        // mConsumerListener is a weak reference to the ConsumerListener.  This is
        // the raison d'etre of ProxyConsumerListener.
        wp<GonkBufferQueue::ConsumerListener> mConsumerListener;
    };


    // GonkBufferQueue manages a pool of gralloc memory slots to be used by
    // producers and consumers. allowSynchronousMode specifies whether or not
    // synchronous mode can be enabled by the producer. allocator is used to
    // allocate all the needed gralloc buffers.
    GonkBufferQueue(bool allowSynchronousMode = true,
            const sp<IGraphicBufferAlloc>& allocator = NULL);
    virtual ~GonkBufferQueue();

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
#if ANDROID_VERSION == 17
    virtual status_t dequeueBuffer(int *buf, sp<Fence>& fence,
            uint32_t width, uint32_t height, uint32_t format, uint32_t usage) {
        return dequeueBuffer(buf, &fence, width, height, format, usage);
    }
#endif

    virtual status_t dequeueBuffer(int *buf, sp<Fence>* fence,
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
#if ANDROID_VERSION == 17
    virtual void cancelBuffer(int buf, sp<Fence> fence);
#else
    virtual void cancelBuffer(int buf, const sp<Fence>& fence);
#endif

    // setSynchronousMode sets whether dequeueBuffer is synchronous or
    // asynchronous. In synchronous mode, dequeueBuffer blocks until
    // a buffer is available, the currently bound buffer can be dequeued and
    // queued buffers will be acquired in order.  In asynchronous mode,
    // a queued buffer may be replaced by a subsequently queued buffer.
    //
    // The default mode is asynchronous.
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
    virtual status_t connect(int api, QueueBufferOutput* output);

    // disconnect attempts to disconnect a producer API from the GonkBufferQueue.
    // Calling this method will cause any subsequent calls to other
    // IGraphicBufferProducer methods to fail except for getAllocator and connect.
    // Successfully calling connect after this will allow the other methods to
    // succeed again.
    //
    // This method will fail if the the GonkBufferQueue is not currently
    // connected to the specified producer API.
    virtual status_t disconnect(int api);

    // dump our state in a String
    virtual void dump(String8& result) const;
    virtual void dump(String8& result, const char* prefix, char* buffer, size_t SIZE) const;

    // public facing structure for BufferSlot
    struct BufferItem {

        BufferItem()
         :
           mSurfaceDescriptor(SurfaceDescriptor()),
           mTransform(0),
           mScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
           mTimestamp(0),
           mFrameNumber(0),
           mBuf(INVALID_BUFFER_SLOT) {
             mCrop.makeInvalid();
        }
        // mGraphicBuffer points to the buffer allocated for this slot, or is NULL
        // if the buffer in this slot has been acquired in the past (see
        // BufferSlot.mAcquireCalled).
        sp<GraphicBuffer> mGraphicBuffer;

        // mSurfaceDescriptor is the token to remotely allocated GraphicBuffer.
        SurfaceDescriptor mSurfaceDescriptor;

        // mCrop is the current crop rectangle for this buffer slot.
        Rect mCrop;

        // mTransform is the current transform flags for this buffer slot.
        uint32_t mTransform;

        // mScalingMode is the current scaling mode for this buffer slot.
        uint32_t mScalingMode;

        // mTimestamp is the current timestamp for this buffer slot. This gets
        // to set by queueBuffer each time this slot is queued.
        int64_t mTimestamp;

        // mFrameNumber is the number of the queued frame for this slot.
        uint64_t mFrameNumber;

        // mBuf is the slot index of this buffer
        int mBuf;

        // mFence is a fence that will signal when the buffer is idle.
        sp<Fence> mFence;
    };

    // The following public functions are the consumer-facing interface

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the GonkBufferQueue.  If no buffer is pending then it returns -EINVAL.  If a
    // buffer is successfully acquired, the information about the buffer is
    // returned in BufferItem.  If the buffer returned had previously been
    // acquired then the BufferItem::mGraphicBuffer field of buffer is set to
    // NULL and it is assumed that the consumer still holds a reference to the
    // buffer.
    status_t acquireBuffer(BufferItem *buffer);

    // releaseBuffer releases a buffer slot from the consumer back to the
    // GonkBufferQueue.  This may be done while the buffer's contents are still
    // being accessed.  The fence will signal when the buffer is no longer
    // in use.
    //
    // If releaseBuffer returns STALE_BUFFER_SLOT, then the consumer must free
    // any references to the just-released buffer that it might have, as if it
    // had received a onBuffersReleased() call with a mask set for the released
    // buffer.
    //
    // Note that the dependencies on EGL will be removed once we switch to using
    // the Android HW Sync HAL.
    status_t releaseBuffer(int buf, const sp<Fence>& releaseFence);

    // consumerConnect connects a consumer to the GonkBufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // GonkBufferQueue is placed into the "abandoned" state, causing most
    // interactions with the GonkBufferQueue by the producer to fail.
    //
    // consumer may not be NULL.
    status_t consumerConnect(const sp<ConsumerListener>& consumer);

    // consumerDisconnect disconnects a consumer from the GonkBufferQueue. All
    // buffers will be freed and the GonkBufferQueue is placed in the "abandoned"
    // state, causing most interactions with the GonkBufferQueue by the producer to
    // fail.
    status_t consumerDisconnect();

    // getReleasedBuffers sets the value pointed to by slotMask to a bit mask
    // indicating which buffer slots have been released by the GonkBufferQueue
    // but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    status_t getReleasedBuffers(uint32_t* slotMask);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    status_t setDefaultBufferSize(uint32_t w, uint32_t h);

    // setDefaultMaxBufferCount sets the default value for the maximum buffer
    // count (the initial default is 2). If the producer has requested a
    // buffer count using setBufferCount, the default buffer count will only
    // take effect if the producer sets the count back to zero.
    //
    // The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    status_t setDefaultMaxBufferCount(int bufferCount);

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1).  This call will
    // fail if a producer is connected to the GonkBufferQueue.
    status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers);

    // isSynchronousMode returns whether the GonkBufferQueue is currently in
    // synchronous mode.
    bool isSynchronousMode() const;

    // setConsumerName sets the name used in logging
    void setConsumerName(const String8& name);

    // setDefaultBufferFormat allows the GonkBufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer.  Formats are enumerated in graphics.h; the
    // initial default is HAL_PIXEL_FORMAT_RGBA_8888.
    status_t setDefaultBufferFormat(uint32_t defaultFormat);

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    status_t setConsumerUsageBits(uint32_t usage);

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    status_t setTransformHint(uint32_t hint);

    int getGeneration();

    SurfaceDescriptor *getSurfaceDescriptorFromBuffer(ANativeWindowBuffer* buffer);

private:
    // releaseBufferFreeListUnlocked releases the resources in the freeList;
    // this must be called with mMutex unlocked.
    void releaseBufferFreeListUnlocked(nsTArray<SurfaceDescriptor>& freeList);

    // freeBufferLocked frees the GraphicBuffer and sync resources for the
    // given slot.
    //void freeBufferLocked(int index);

    // freeAllBuffersLocked frees the GraphicBuffer and sync resources for
    // all slots.
    //void freeAllBuffersLocked();
    void freeAllBuffersLocked(nsTArray<SurfaceDescriptor>& freeList);

    // setDefaultMaxBufferCountLocked sets the maximum number of buffer slots
    // that will be used if the producer does not override the buffer slot
    // count.  The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    // The initial default is 2.
    status_t setDefaultMaxBufferCountLocked(int count);

    // getMinBufferCountLocked returns the minimum number of buffers allowed
    // given the current GonkBufferQueue state.
    int getMinMaxBufferCountLocked() const;

    // getMinUndequeuedBufferCountLocked returns the minimum number of buffers
    // that must remain in a state other than DEQUEUED.
    int getMinUndequeuedBufferCountLocked() const;

    // getMaxBufferCountLocked returns the maximum number of buffers that can
    // be allocated at once.  This value depends upon the following member
    // variables:
    //
    //      mSynchronousMode
    //      mMaxAcquiredBufferCount
    //      mDefaultMaxBufferCount
    //      mOverrideMaxBufferCount
    //
    // Any time one of these member variables is changed while a producer is
    // connected, mDequeueCondition must be broadcast.
    int getMaxBufferCountLocked() const;

    struct BufferSlot {

        BufferSlot()
        : mSurfaceDescriptor(SurfaceDescriptor()),
          mBufferState(BufferSlot::FREE),
          mRequestBufferCalled(false),
          mTransform(0),
          mScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
          mTimestamp(0),
          mFrameNumber(0),
          mAcquireCalled(false),
          mNeedsCleanupOnRelease(false) {
            mCrop.makeInvalid();
        }

        // mGraphicBuffer points to the buffer allocated for this slot or is NULL
        // if no buffer has been allocated.
        sp<GraphicBuffer> mGraphicBuffer;

        // mSurfaceDescriptor is the token to remotely allocated GraphicBuffer.
        SurfaceDescriptor mSurfaceDescriptor;

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

        // mCrop is the current crop rectangle for this buffer slot.
        Rect mCrop;

        // mTransform is the current transform flags for this buffer slot.
        // (example: NATIVE_WINDOW_TRANSFORM_ROT_90)
        uint32_t mTransform;

        // mScalingMode is the current scaling mode for this buffer slot.
        // (example: NATIVE_WINDOW_SCALING_MODE_FREEZE)
        uint32_t mScalingMode;

        // mTimestamp is the current timestamp for this buffer slot. This gets
        // to set by queueBuffer each time this slot is queued.
        int64_t mTimestamp;

        // mFrameNumber is the number of the queued frame for this slot.  This
        // is used to dequeue buffers in LRU order (useful because buffers
        // may be released before their release fence is signaled).
        uint64_t mFrameNumber;

        // mEglFence is the EGL sync object that must signal before the buffer
        // associated with this buffer slot may be dequeued. It is initialized
        // to EGL_NO_SYNC_KHR when the buffer is created and may be set to a
        // new sync object in releaseBuffer.  (This is deprecated in favor of
        // mFence, below.)
        //EGLSyncKHR mEglFence;

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
    sp<ConsumerListener> mConsumerListener;

    // mSynchronousMode whether we're in synchronous mode or not
    bool mSynchronousMode;

    // mAllowSynchronousMode whether we allow synchronous mode or not.  Set
    // when the GonkBufferQueue is created (by the consumer).
    const bool mAllowSynchronousMode;

    // mConnectedApi indicates the producer API that is currently connected
    // to this GonkBufferQueue.  It defaults to NO_CONNECTED_API (= 0), and gets
    // updated by the connect and disconnect methods.
    int mConnectedApi;

    // mDequeueCondition condition used for dequeueBuffer in synchronous mode
    mutable Condition mDequeueCondition;

    // mQueue is a FIFO of queued buffers used in synchronous mode
    typedef Vector<int> Fifo;
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
    // successful queueBuffer call.
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

    // mGeneration is the current generation of buffer slots
    uint32_t mGeneration;
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_BUFFERQUEUE_H
