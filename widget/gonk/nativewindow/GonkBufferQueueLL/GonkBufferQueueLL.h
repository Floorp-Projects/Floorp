/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef NATIVEWINDOW_GONKBUFFERQUEUE_LL_H
#define NATIVEWINDOW_GONKBUFFERQUEUE_LL_H

#include "GonkBufferQueueDefs.h"
#include "IGonkGraphicBufferConsumerLL.h"
#include <gui/IGraphicBufferProducer.h>
#include <gui/IConsumerListener.h>

// These are only required to keep other parts of the framework with incomplete
// dependencies building successfully
#include <gui/IGraphicBufferAlloc.h>

namespace android {

class GonkBufferQueue {
public:
    // GonkBufferQueue will keep track of at most this value of buffers.
    // Attempts at runtime to increase the number of buffers past this will fail.
    enum { NUM_BUFFER_SLOTS = GonkBufferQueueDefs::NUM_BUFFER_SLOTS };
    // Used as a placeholder slot# when the value isn't pointing to an existing buffer.
    enum { INVALID_BUFFER_SLOT = IGonkGraphicBufferConsumer::BufferItem::INVALID_BUFFER_SLOT };
    // Alias to <IGonkGraphicBufferConsumer.h> -- please scope from there in future code!
    enum {
        NO_BUFFER_AVAILABLE = IGonkGraphicBufferConsumer::NO_BUFFER_AVAILABLE,
        PRESENT_LATER = IGonkGraphicBufferConsumer::PRESENT_LATER,
    };

    // When in async mode we reserve two slots in order to guarantee that the
    // producer and consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = NUM_BUFFER_SLOTS - 2 };

    // for backward source compatibility
    typedef ::android::ConsumerListener ConsumerListener;
    typedef IGonkGraphicBufferConsumer::BufferItem BufferItem;

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
#if ANDROID_VERSION == 21
        virtual void onFrameAvailable();
#else
        virtual void onFrameAvailable(const ::android::BufferItem& item);
        virtual void onFrameReplaced(const ::android::BufferItem& item);
#endif
        virtual void onBuffersReleased();
        virtual void onSidebandStreamChanged();
    private:
        // mConsumerListener is a weak reference to the IConsumerListener.  This is
        // the raison d'etre of ProxyConsumerListener.
        wp<ConsumerListener> mConsumerListener;
    };

    // GonkBufferQueue manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the
    // needed gralloc buffers.
    static void createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
            sp<IGonkGraphicBufferConsumer>* outConsumer,
            const sp<IGraphicBufferAlloc>& allocator = NULL);

private:
    GonkBufferQueue(); // Create through createBufferQueue
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // NATIVEWINDOW_GONKBUFFERQUEUE_LL_H
