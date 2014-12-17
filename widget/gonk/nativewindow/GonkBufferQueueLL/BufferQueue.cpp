/*
 * Copyright (C) 2012 The Android Open Source Project
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

#define LOG_TAG "BufferQueue"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <gui/BufferQueue.h>
#include <gui/BufferQueueConsumer.h>
#include <gui/BufferQueueCore.h>
#include <gui/BufferQueueProducer.h>

namespace android {

BufferQueue::ProxyConsumerListener::ProxyConsumerListener(
        const wp<ConsumerListener>& consumerListener):
        mConsumerListener(consumerListener) {}

BufferQueue::ProxyConsumerListener::~ProxyConsumerListener() {}

void BufferQueue::ProxyConsumerListener::onFrameAvailable() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable();
    }
}

void BufferQueue::ProxyConsumerListener::onBuffersReleased() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onBuffersReleased();
    }
}

void BufferQueue::ProxyConsumerListener::onSidebandStreamChanged() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onSidebandStreamChanged();
    }
}

void BufferQueue::createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
        sp<IGraphicBufferConsumer>* outConsumer,
        const sp<IGraphicBufferAlloc>& allocator) {
    LOG_ALWAYS_FATAL_IF(outProducer == NULL,
            "BufferQueue: outProducer must not be NULL");
    LOG_ALWAYS_FATAL_IF(outConsumer == NULL,
            "BufferQueue: outConsumer must not be NULL");

    sp<BufferQueueCore> core(new BufferQueueCore(allocator));
    LOG_ALWAYS_FATAL_IF(core == NULL,
            "BufferQueue: failed to create BufferQueueCore");

    sp<IGraphicBufferProducer> producer(new BufferQueueProducer(core));
    LOG_ALWAYS_FATAL_IF(producer == NULL,
            "BufferQueue: failed to create BufferQueueProducer");

    sp<IGraphicBufferConsumer> consumer(new BufferQueueConsumer(core));
    LOG_ALWAYS_FATAL_IF(consumer == NULL,
            "BufferQueue: failed to create BufferQueueConsumer");

    *outProducer = producer;
    *outConsumer = consumer;
}

}; // namespace android
