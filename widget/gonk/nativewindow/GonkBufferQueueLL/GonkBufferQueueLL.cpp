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

#define LOG_TAG "GonkBufferQueue"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_NDEBUG 0

#include "GonkBufferQueue.h"
#include "GonkBufferQueueConsumer.h"
#include "GonkBufferQueueCore.h"
#include "GonkBufferQueueProducer.h"

namespace android {

GonkBufferQueue::ProxyConsumerListener::ProxyConsumerListener(
        const wp<ConsumerListener>& consumerListener):
        mConsumerListener(consumerListener) {}

GonkBufferQueue::ProxyConsumerListener::~ProxyConsumerListener() {}

#if ANDROID_VERSION == 21
void GonkBufferQueue::ProxyConsumerListener::onFrameAvailable() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable();
    }
}
#else
void GonkBufferQueue::ProxyConsumerListener::onFrameAvailable(const ::android::BufferItem& item) {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable(item);
    }
}

void GonkBufferQueue::ProxyConsumerListener::onFrameReplaced(const ::android::BufferItem& item) {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameReplaced(item);
    }
}
#endif

void GonkBufferQueue::ProxyConsumerListener::onBuffersReleased() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onBuffersReleased();
    }
}

void GonkBufferQueue::ProxyConsumerListener::onSidebandStreamChanged() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onSidebandStreamChanged();
    }
}

void GonkBufferQueue::createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
        sp<IGonkGraphicBufferConsumer>* outConsumer,
        const sp<IGraphicBufferAlloc>& allocator) {
    LOG_ALWAYS_FATAL_IF(outProducer == NULL,
            "GonkBufferQueue: outProducer must not be NULL");
    LOG_ALWAYS_FATAL_IF(outConsumer == NULL,
            "GonkBufferQueue: outConsumer must not be NULL");

    sp<GonkBufferQueueCore> core(new GonkBufferQueueCore(allocator));
    LOG_ALWAYS_FATAL_IF(core == NULL,
            "GonkBufferQueue: failed to create GonkBufferQueueCore");

    sp<IGraphicBufferProducer> producer(new GonkBufferQueueProducer(core));
    LOG_ALWAYS_FATAL_IF(producer == NULL,
            "GonkBufferQueue: failed to create GonkBufferQueueProducer");

    sp<IGonkGraphicBufferConsumer> consumer(new GonkBufferQueueConsumer(core));
    LOG_ALWAYS_FATAL_IF(consumer == NULL,
            "GonkBufferQueue: failed to create GonkBufferQueueConsumer");

    *outProducer = producer;
    *outConsumer = consumer;
}

}; // namespace android
