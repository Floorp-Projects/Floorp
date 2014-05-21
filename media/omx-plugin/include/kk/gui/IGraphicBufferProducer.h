/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ANDROID_GUI_IGRAPHICBUFFERPRODUCER_H
#define ANDROID_GUI_IGRAPHICBUFFERPRODUCER_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>

#include <binder/IInterface.h>

#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>

namespace android {
// ----------------------------------------------------------------------------

class Surface;

/*
 * This class defines the Binder IPC interface for the producer side of
 * a queue of graphics buffers.  It's used to send graphics data from one
 * component to another.  For example, a class that decodes video for
 * playback might use this to provide frames.  This is typically done
 * indirectly, through Surface.
 *
 * The underlying mechanism is a BufferQueue, which implements
 * BnGraphicBufferProducer.  In normal operation, the producer calls
 * dequeueBuffer() to get an empty buffer, fills it with data, then
 * calls queueBuffer() to make it available to the consumer.
 *
 * This class was previously called ISurfaceTexture.
 */
class IGraphicBufferProducer : public IInterface
{
public:
    DECLARE_META_INTERFACE(GraphicBufferProducer);

    enum {
        BUFFER_NEEDS_REALLOCATION = 0x1,
        RELEASE_ALL_BUFFERS       = 0x2,
    };

    // requestBuffer requests a new buffer for the given index. The server (i.e.
    // the IGraphicBufferProducer implementation) assigns the newly created
    // buffer to the given slot index, and the client is expected to mirror the
    // slot->buffer mapping so that it's not necessary to transfer a
    // GraphicBuffer for every dequeue operation.
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf) = 0;

    // setBufferCount sets the number of buffer slots available. Calling this
    // will also cause all buffer slots to be emptied. The caller should empty
    // its mirrored copy of the buffer slots when calling this method.
    virtual status_t setBufferCount(int bufferCount) = 0;

    // dequeueBuffer requests a new buffer slot for the client to use. Ownership
    // of the slot is transfered to the client, meaning that the server will not
    // use the contents of the buffer associated with that slot. The slot index
    // returned may or may not contain a buffer. If the slot is empty the client
    // should call requestBuffer to assign a new buffer to that slot. The client
    // is expected to either call cancelBuffer on the dequeued slot or to fill
    // in the contents of its associated buffer contents and call queueBuffer.
    // If dequeueBuffer return BUFFER_NEEDS_REALLOCATION, the client is
    // expected to call requestBuffer immediately.
    //
    // The fence parameter will be updated to hold the fence associated with
    // the buffer. The contents of the buffer must not be overwritten until the
    // fence signals. If the fence is NULL, the buffer may be written
    // immediately.
    virtual status_t dequeueBuffer(int *slot, sp<Fence>* fence,
            uint32_t w, uint32_t h, uint32_t format, uint32_t usage) = 0;

    // queueBuffer indicates that the client has finished filling in the
    // contents of the buffer associated with slot and transfers ownership of
    // that slot back to the server. It is not valid to call queueBuffer on a
    // slot that is not owned by the client or one for which a buffer associated
    // via requestBuffer. In addition, a timestamp must be provided by the
    // client for this buffer. The timestamp is measured in nanoseconds, and
    // must be monotonically increasing. Its other properties (zero point, etc)
    // are client-dependent, and should be documented by the client.
    //
    // outWidth, outHeight and outTransform are filled with the default width
    // and height of the window and current transform applied to buffers,
    // respectively.

    struct QueueBufferInput : public Flattenable {
        inline QueueBufferInput(const Parcel& parcel);
        inline QueueBufferInput(int64_t timestamp,
                const Rect& crop, int scalingMode, uint32_t transform,
                sp<Fence> fence)
        : timestamp(timestamp), crop(crop), scalingMode(scalingMode),
          transform(transform), fence(fence) { }
        inline void deflate(int64_t* outTimestamp, Rect* outCrop,
                int* outScalingMode, uint32_t* outTransform,
                sp<Fence>* outFence) const {
            *outTimestamp = timestamp;
            *outCrop = crop;
            *outScalingMode = scalingMode;
            *outTransform = transform;
            *outFence = fence;
        }

        // Flattenable interface
        virtual size_t getFlattenedSize() const;
        virtual size_t getFdCount() const;
        virtual status_t flatten(void* buffer, size_t size,
                int fds[], size_t count) const;
        virtual status_t unflatten(void const* buffer, size_t size,
                int fds[], size_t count);

    private:
        int64_t timestamp;
        Rect crop;
        int scalingMode;
        uint32_t transform;
        sp<Fence> fence;
    };

    // QueueBufferOutput must be a POD structure
    struct QueueBufferOutput {
        inline QueueBufferOutput() { }
        inline void deflate(uint32_t* outWidth,
                uint32_t* outHeight,
                uint32_t* outTransformHint,
                uint32_t* outNumPendingBuffers) const {
            *outWidth = width;
            *outHeight = height;
            *outTransformHint = transformHint;
            *outNumPendingBuffers = numPendingBuffers;
        }
        inline void inflate(uint32_t inWidth, uint32_t inHeight,
                uint32_t inTransformHint, uint32_t inNumPendingBuffers) {
            width = inWidth;
            height = inHeight;
            transformHint = inTransformHint;
            numPendingBuffers = inNumPendingBuffers;
        }
    private:
        uint32_t width;
        uint32_t height;
        uint32_t transformHint;
        uint32_t numPendingBuffers;
    };

    virtual status_t queueBuffer(int slot,
            const QueueBufferInput& input, QueueBufferOutput* output) = 0;

    // cancelBuffer indicates that the client does not wish to fill in the
    // buffer associated with slot and transfers ownership of the slot back to
    // the server.
    virtual void cancelBuffer(int slot, const sp<Fence>& fence) = 0;

    // query retrieves some information for this surface
    // 'what' tokens allowed are that of android_natives.h
    virtual int query(int what, int* value) = 0;

    // setSynchronousMode set whether dequeueBuffer is synchronous or
    // asynchronous. In synchronous mode, dequeueBuffer blocks until
    // a buffer is available, the currently bound buffer can be dequeued and
    // queued buffers will be retired in order.
    // The default mode is asynchronous.
    virtual status_t setSynchronousMode(bool enabled) = 0;

    // connect attempts to connect a client API to the IGraphicBufferProducer.
    // This must be called before any other IGraphicBufferProducer methods are
    // called except for getAllocator.
    //
    // This method will fail if the connect was previously called on the
    // IGraphicBufferProducer and no corresponding disconnect call was made.
    //
    // outWidth, outHeight and outTransform are filled with the default width
    // and height of the window and current transform applied to buffers,
    // respectively.
    virtual status_t connect(int api, QueueBufferOutput* output) = 0;

    // disconnect attempts to disconnect a client API from the
    // IGraphicBufferProducer.  Calling this method will cause any subsequent
    // calls to other IGraphicBufferProducer methods to fail except for
    // getAllocator and connect.  Successfully calling connect after this will
    // allow the other methods to succeed again.
    //
    // This method will fail if the the IGraphicBufferProducer is not currently
    // connected to the specified client API.
    virtual status_t disconnect(int api) = 0;
};

// ----------------------------------------------------------------------------

class BnGraphicBufferProducer : public BnInterface<IGraphicBufferProducer>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_IGRAPHICBUFFERPRODUCER_H
