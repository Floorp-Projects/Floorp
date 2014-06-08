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

//#define LOG_NDEBUG 0
#define LOG_TAG "GonkNativeWindow"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Log.h>

#include "GonkNativeWindowJB.h"
#include "GrallocImages.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/RefPtr.h"

#define BI_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define BI_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define BI_LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define BI_LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define BI_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace mozilla;
using namespace mozilla::layers;

namespace android {

GonkNativeWindow::GonkNativeWindow(int bufferCount) :
    GonkConsumerBase(new GonkBufferQueue(true) )
{
    mBufferQueue->setMaxAcquiredBufferCount(bufferCount);
}

GonkNativeWindow::~GonkNativeWindow() {
}

void GonkNativeWindow::setName(const String8& name) {
    Mutex::Autolock _l(mMutex);
    mName = name;
    mBufferQueue->setConsumerName(name);
}
#if ANDROID_VERSION >= 18
status_t GonkNativeWindow::acquireBuffer(BufferItem *item, bool waitForFence) {
    status_t err;

    if (!item) return BAD_VALUE;

    Mutex::Autolock _l(mMutex);

    err = acquireBufferLocked(item);
    if (err != OK) {
        if (err != NO_BUFFER_AVAILABLE) {
            BI_LOGE("Error acquiring buffer: %s (%d)", strerror(err), err);
        }
        return err;
    }

    if (waitForFence) {
        err = item->mFence->waitForever("GonkNativeWindow::acquireBuffer");
        if (err != OK) {
            BI_LOGE("Failed to wait for fence of acquired buffer: %s (%d)",
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

    err = addReleaseFenceLocked(item.mBuf, releaseFence);

    err = releaseBufferLocked(item.mBuf);
    if (err != OK) {
        BI_LOGE("Failed to release buffer: %s (%d)",
                strerror(-err), err);
    }
    return err;
}
#endif

status_t GonkNativeWindow::setDefaultBufferSize(uint32_t w, uint32_t h) {
    Mutex::Autolock _l(mMutex);
    return mBufferQueue->setDefaultBufferSize(w, h);
}

status_t GonkNativeWindow::setDefaultBufferFormat(uint32_t defaultFormat) {
    Mutex::Autolock _l(mMutex);
    return mBufferQueue->setDefaultBufferFormat(defaultFormat);
}

TemporaryRef<TextureClient>
GonkNativeWindow::getCurrentBuffer() {
    Mutex::Autolock _l(mMutex);
    GonkBufferQueue::BufferItem item;

    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    status_t err = acquireBufferLocked(&item);
    if (err != NO_ERROR) {
        return NULL;
    }

    RefPtr<TextureClient> textureClient =
      mBufferQueue->getTextureClientFromBuffer(item.mGraphicBuffer.get());
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
    BI_LOGD("GonkNativeWindow::returnBuffer");
    Mutex::Autolock lock(mMutex);

    int index =  mBufferQueue->getSlotFromTextureClientLocked(client);
    if (index < 0) {
    }

    sp<Fence> fence = client->GetReleaseFenceHandle().mFence;
    if (!fence.get()) {
      fence = Fence::NO_FENCE;
    }

    status_t err;
    err = addReleaseFenceLocked(index, fence);

    err = releaseBufferLocked(index);
}

TemporaryRef<TextureClient>
GonkNativeWindow::getTextureClientFromBuffer(ANativeWindowBuffer* buffer) {
    Mutex::Autolock lock(mMutex);
    return mBufferQueue->getTextureClientFromBuffer(buffer);
}

void GonkNativeWindow::setNewFrameCallback(
        GonkNativeWindowNewFrameCallback* callback) {
    BI_LOGD("setNewFrameCallback");
    Mutex::Autolock lock(mMutex);
    mNewFrameCallback = callback;
}

void GonkNativeWindow::onFrameAvailable() {
    GonkConsumerBase::onFrameAvailable();

    if (mNewFrameCallback) {
      mNewFrameCallback->OnNewFrame();
    }
}

} // namespace android
