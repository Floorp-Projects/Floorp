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

#ifndef NATIVEWINDOW_GONKNATIVEWINDOW_KK_H
#define NATIVEWINDOW_GONKNATIVEWINDOW_KK_H

#include <ui/GraphicBuffer.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/threads.h>

#include "CameraCommon.h"
#include "GonkConsumerBaseKK.h"
#include "GrallocImages.h"
#include "IGonkGraphicBufferConsumer.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/LayersSurfaces.h"

namespace mozilla {
namespace layers {
    class PGrallocBufferChild;
}
}

namespace android {

// The user of GonkNativeWindow who wants to receive notification of
// new frames should implement this interface.
class GonkNativeWindowNewFrameCallback {
public:
    virtual void OnNewFrame() = 0;
};

/**
 * GonkNativeWindow is a GonkBufferQueue consumer endpoint that allows clients
 * access to the whole BufferItem entry from GonkBufferQueue. Multiple buffers may
 * be acquired at once, to be used concurrently by the client. This consumer can
 * operate either in synchronous or asynchronous mode.
 */
class GonkNativeWindow: public GonkConsumerBase
{
    typedef mozilla::layers::TextureClient TextureClient;
  public:
    typedef GonkConsumerBase::FrameAvailableListener FrameAvailableListener;
    typedef IGonkGraphicBufferConsumer::BufferItem BufferItem;

    enum { INVALID_BUFFER_SLOT = GonkBufferQueue::INVALID_BUFFER_SLOT };
    enum { NO_BUFFER_AVAILABLE = GonkBufferQueue::NO_BUFFER_AVAILABLE };

    // Create a new buffer item consumer. The consumerUsage parameter determines
    // the consumer usage flags passed to the graphics allocator. The
    // bufferCount parameter specifies how many buffers can be locked for user
    // access at the same time.
    // controlledByApp tells whether this consumer is controlled by the
    // application.
    B2G_ACL_EXPORT GonkNativeWindow(int bufferCount = GonkBufferQueue::MIN_UNDEQUEUED_BUFFERS);
    GonkNativeWindow(const sp<GonkBufferQueue>& bq, uint32_t consumerUsage,
            int bufferCount = GonkBufferQueue::MIN_UNDEQUEUED_BUFFERS,
            bool controlledByApp = false);

    virtual ~GonkNativeWindow();

    // set the name of the GonkNativeWindow that will be used to identify it in
    // log messages.
    void setName(const String8& name);

    // Gets the next graphics buffer from the producer, filling out the
    // passed-in BufferItem structure. Returns NO_BUFFER_AVAILABLE if the queue
    // of buffers is empty, and INVALID_OPERATION if the maximum number of
    // buffers is already acquired.
    //
    // Only a fixed number of buffers can be acquired at a time, determined by
    // the construction-time bufferCount parameter. If INVALID_OPERATION is
    // returned by acquireBuffer, then old buffers must be returned to the
    // queue by calling releaseBuffer before more buffers can be acquired.
    //
    // If waitForFence is true, and the acquired BufferItem has a valid fence object,
    // acquireBuffer will wait on the fence with no timeout before returning.
    status_t acquireBuffer(BufferItem *item, nsecs_t presentWhen,
        bool waitForFence = true);

    // Returns an acquired buffer to the queue, allowing it to be reused. Since
    // only a fixed number of buffers may be acquired at a time, old buffers
    // must be released by calling releaseBuffer to ensure new buffers can be
    // acquired by acquireBuffer. Once a BufferItem is released, the caller must
    // not access any members of the BufferItem, and should immediately remove
    // all of its references to the BufferItem itself.
    status_t releaseBuffer(const BufferItem &item,
            const sp<Fence>& releaseFence = Fence::NO_FENCE);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // requestBuffers when a with and height of zero is requested.
    status_t setDefaultBufferSize(uint32_t w, uint32_t h);

    // setDefaultBufferFormat allows the BufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer
    status_t setDefaultBufferFormat(uint32_t defaultFormat);

    // Get next frame from the queue, caller owns the returned buffer.
    B2G_ACL_EXPORT already_AddRefed<TextureClient> getCurrentBuffer();

    // Return the buffer to the queue and mark it as FREE. After that
    // the buffer is useable again for the decoder.
    void returnBuffer(TextureClient* client);

    already_AddRefed<TextureClient> getTextureClientFromBuffer(ANativeWindowBuffer* buffer);

    B2G_ACL_EXPORT void setNewFrameCallback(GonkNativeWindowNewFrameCallback* callback);

    static void RecycleCallback(TextureClient* client, void* closure);

protected:
    virtual void onFrameAvailable();

private:
    GonkNativeWindowNewFrameCallback* mNewFrameCallback;
};

} // namespace android

#endif // NATIVEWINDOW_GONKNATIVEWINDOW_JB_H
