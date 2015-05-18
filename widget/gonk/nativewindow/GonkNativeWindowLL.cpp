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

//#define LOG_NDEBUG 0
#define LOG_TAG "GonkNativeWindow"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Log.h>

#include "GonkNativeWindowLL.h"

using namespace mozilla;
using namespace mozilla::layers;

namespace android {

GonkNativeWindow::GonkNativeWindow(
        const sp<IGonkGraphicBufferConsumer>& consumer, int bufferCount) :
    GonkConsumerBase(consumer, false),
    mNewFrameCallback(nullptr)
{
    if (bufferCount != DEFAULT_MAX_BUFFERS) {
        status_t err = mConsumer->setMaxAcquiredBufferCount(bufferCount);
        LOG_ALWAYS_FATAL_IF(err != OK,
                "Failed to set max acquired buffer count to %d", bufferCount);
    }
}

GonkNativeWindow::GonkNativeWindow(
        const sp<IGonkGraphicBufferConsumer>& consumer, uint32_t consumerUsage,
        int bufferCount, bool controlledByApp) :
    GonkConsumerBase(consumer, controlledByApp)
{
    status_t err = mConsumer->setConsumerUsageBits(consumerUsage);
    LOG_ALWAYS_FATAL_IF(err != OK,
            "Failed to set consumer usage bits to %#x", consumerUsage);
    if (bufferCount != DEFAULT_MAX_BUFFERS) {
        err = mConsumer->setMaxAcquiredBufferCount(bufferCount);
        LOG_ALWAYS_FATAL_IF(err != OK,
                "Failed to set max acquired buffer count to %d", bufferCount);
    }
}

GonkNativeWindow::~GonkNativeWindow() {
}

void GonkNativeWindow::setName(const String8& name) {
    Mutex::Autolock _l(mMutex);
    mName = name;
    mConsumer->setConsumerName(name);
}

status_t GonkNativeWindow::acquireBuffer(BufferItem *item,
        nsecs_t presentWhen, bool waitForFence) {
    status_t err;

    if (!item) return BAD_VALUE;

    Mutex::Autolock _l(mMutex);

    err = acquireBufferLocked(item, presentWhen);
    if (err != OK) {
        if (err != NO_BUFFER_AVAILABLE) {
            ALOGE("Error acquiring buffer: %s (%d)", strerror(err), err);
        }
        return err;
    }

    if (waitForFence) {
        err = item->mFence->waitForever("GonkNativeWindow::acquireBuffer");
        if (err != OK) {
            ALOGE("Failed to wait for fence of acquired buffer: %s (%d)",
                    strerror(-err), err);
            return err;
        }
    }

    item->mGraphicBuffer = mSlots[item->mBuf].mGraphicBuffer;

    return OK;
}

status_t GonkNativeWindow::releaseBuffer(const BufferItem &item,
        const sp<Fence>& releaseFence) {
    status_t err;

    Mutex::Autolock _l(mMutex);

    err = addReleaseFenceLocked(item.mBuf, item.mGraphicBuffer, releaseFence);

    err = releaseBufferLocked(item.mBuf, item.mGraphicBuffer);
    if (err != OK) {
        ALOGE("Failed to release buffer: %s (%d)",
                strerror(-err), err);
    }
    return err;
}

status_t GonkNativeWindow::setDefaultBufferSize(uint32_t w, uint32_t h) {
    Mutex::Autolock _l(mMutex);
    return mConsumer->setDefaultBufferSize(w, h);
}

status_t GonkNativeWindow::setDefaultBufferFormat(uint32_t defaultFormat) {
    Mutex::Autolock _l(mMutex);
    return mConsumer->setDefaultBufferFormat(defaultFormat);
}

TemporaryRef<TextureClient>
GonkNativeWindow::getCurrentBuffer() {
    Mutex::Autolock _l(mMutex);
    BufferItem item;

    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    status_t err = acquireBufferLocked(&item, 0); //???
    if (err != NO_ERROR) {
        return NULL;
    }

    RefPtr<TextureClient> textureClient =
        mConsumer->getTextureClientFromBuffer(item.mGraphicBuffer.get());
    if (!textureClient) {
        return NULL;
    }
    textureClient->SetRecycleCallback(GonkNativeWindow::RecycleCallback, this);
    return textureClient;
}

/* static */ void
GonkNativeWindow::RecycleCallback(TextureClient* client, void* closure) {
    GonkNativeWindow* nativeWindow =
        static_cast<GonkNativeWindow*>(closure);

    client->ClearRecycleCallback();
    nativeWindow->returnBuffer(client);
}

void GonkNativeWindow::returnBuffer(TextureClient* client) {
    ALOGD("GonkNativeWindow::returnBuffer");
    Mutex::Autolock lock(mMutex);

    int index = mConsumer->getSlotFromTextureClientLocked(client);
    if (index < 0) {
        return;
    }

    FenceHandle handle = client->GetAndResetReleaseFenceHandle();
    nsRefPtr<FenceHandle::FdObj> fdObj = handle.GetAndResetFdObj();
    sp<Fence> fence = new Fence(fdObj->GetAndResetFd());

    status_t err;
    err = addReleaseFenceLocked(index,
                                mSlots[index].mGraphicBuffer,
                                fence);

    err = releaseBufferLocked(index, mSlots[index].mGraphicBuffer);

    if (err != OK) {
        ALOGE("Failed to return buffer: %s (%d)", strerror(-err), err);
    }
}

TemporaryRef<TextureClient>
GonkNativeWindow::getTextureClientFromBuffer(ANativeWindowBuffer* buffer) {
    Mutex::Autolock lock(mMutex);
    return mConsumer->getTextureClientFromBuffer(buffer);
}

void GonkNativeWindow::setNewFrameCallback(
        GonkNativeWindowNewFrameCallback* callback) {
    ALOGD("setNewFrameCallback");
    Mutex::Autolock lock(mMutex);
    mNewFrameCallback = callback;
}

#if ANDROID_VERSION == 21
void GonkNativeWindow::onFrameAvailable() {
    GonkConsumerBase::onFrameAvailable();
#else
void GonkNativeWindow::onFrameAvailable(const ::android::BufferItem &item) {
    GonkConsumerBase::onFrameAvailable(item);
#endif

    if (mNewFrameCallback) {
        mNewFrameCallback->OnNewFrame();
    }
}

} // namespace android
