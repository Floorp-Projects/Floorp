/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012-2013 Mozilla Foundation
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

#include "base/basictypes.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "GonkNativeWindow.h"
#include "nsDebug.h"

/**
 * DOM_CAMERA_LOGI() is enabled in debug builds, and turned on by setting
 * MOZ_LOG=Camera:N environment variable, where N >= 3.
 *
 * CNW_LOGE() is always enabled.
 */
#define CNW_LOGD(...)   DOM_CAMERA_LOGI(__VA_ARGS__)
#define CNW_LOGE(...)   {(void)printf_stderr(__VA_ARGS__);}

using namespace android;
using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

GonkNativeWindow::GonkNativeWindow() :
    mAbandoned(false),
    mDefaultWidth(1),
    mDefaultHeight(1),
    mPixelFormat(PIXEL_FORMAT_RGBA_8888),
    mBufferCount(MIN_BUFFER_SLOTS + 1),
    mConnectedApi(NO_CONNECTED_API),
    mFrameCounter(0),
    mNewFrameCallback(nullptr) {
}

GonkNativeWindow::~GonkNativeWindow() {
    freeAllBuffersLocked();
}

void GonkNativeWindow::abandon()
{
    CNW_LOGD("abandon");
    Mutex::Autolock lock(mMutex);
    mQueue.clear();
    mAbandoned = true;
    freeAllBuffersLocked();
    mDequeueCondition.signal();
}

void GonkNativeWindow::freeAllBuffersLocked()
{
    CNW_LOGD("freeAllBuffersLocked");

    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mGraphicBuffer != NULL) {
            if (mSlots[i].mTextureClient) {
              mSlots[i].mTextureClient->ClearRecycleCallback();
              // release TextureClient in ImageBridge thread
              RefPtr<TextureClientReleaseTask> task =
                MakeAndAddRef<TextureClientReleaseTask>(mSlots[i].mTextureClient);
              mSlots[i].mTextureClient = NULL;
              ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(task.forget());
            }
            mSlots[i].mGraphicBuffer = NULL;
            mSlots[i].mBufferState = BufferSlot::FREE;
            mSlots[i].mFrameNumber = 0;
        }
    }
}

void GonkNativeWindow::clearRenderingStateBuffersLocked()
{
    CNW_LOGD("clearRenderingStateBuffersLocked");

    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mGraphicBuffer != NULL) {
            // Clear RENDERING state buffer
            if (mSlots[i].mBufferState == BufferSlot::RENDERING) {
                if (mSlots[i].mTextureClient) {
                  mSlots[i].mTextureClient->ClearRecycleCallback();
                  // release TextureClient in ImageBridge thread
                  RefPtr<TextureClientReleaseTask> task =
                    MakeAndAddRef<TextureClientReleaseTask>(mSlots[i].mTextureClient);
                  mSlots[i].mTextureClient = NULL;
                  ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(task.forget());
                }
                mSlots[i].mGraphicBuffer = NULL;
                mSlots[i].mBufferState = BufferSlot::FREE;
                mSlots[i].mFrameNumber = 0;
            }
        }
    }
}

status_t GonkNativeWindow::setBufferCount(int bufferCount)
{
    CNW_LOGD("setBufferCount: count=%d", bufferCount);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("setBufferCount: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }

    if (bufferCount > NUM_BUFFER_SLOTS) {
        CNW_LOGE("setBufferCount: bufferCount larger than slots available");
        return BAD_VALUE;
    }

    if (bufferCount < MIN_BUFFER_SLOTS) {
        CNW_LOGE("setBufferCount: requested buffer count (%d) is less than "
                "minimum (%d)", bufferCount, MIN_BUFFER_SLOTS);
        return BAD_VALUE;
    }

    // Error out if the user has dequeued buffers.
    for (int i=0 ; i<mBufferCount ; i++) {
        if (mSlots[i].mBufferState == BufferSlot::DEQUEUED) {
            CNW_LOGE("setBufferCount: client owns some buffers");
            return -EINVAL;
        }
    }

    if (bufferCount >= mBufferCount) {
        mBufferCount = bufferCount;
        //clear only buffers in RENDERING state.
        clearRenderingStateBuffersLocked();
        mQueue.clear();
        mDequeueCondition.signal();
        return OK;
    }

    // here we're guaranteed that the client doesn't have dequeued buffers
    // and will release all of its buffer references.
    freeAllBuffersLocked();
    mBufferCount = bufferCount;
    mQueue.clear();
    mDequeueCondition.signal();
    return OK;
}

status_t GonkNativeWindow::setDefaultBufferSize(uint32_t w, uint32_t h)
{
    CNW_LOGD("setDefaultBufferSize: w=%d, h=%d", w, h);
    if (!w || !h) {
        CNW_LOGE("setDefaultBufferSize: dimensions cannot be 0 (w=%d, h=%d)",
                w, h);
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);
    mDefaultWidth = w;
    mDefaultHeight = h;
    return OK;
}

status_t GonkNativeWindow::requestBuffer(int slot, sp<GraphicBuffer>* buf)
{
    CNW_LOGD("requestBuffer: slot=%d", slot);
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        CNW_LOGE("requestBuffer: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }
    if (slot < 0 || mBufferCount <= slot) {
        CNW_LOGE("requestBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, slot);
        return BAD_VALUE;
    }
    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}

status_t GonkNativeWindow::dequeueBuffer(int *outBuf, uint32_t w, uint32_t h,
        uint32_t format, uint32_t usage)
{
    if ((w && !h) || (!w && h)) {
        CNW_LOGE("dequeueBuffer: invalid size: w=%u, h=%u", w, h);
        return BAD_VALUE;
    }

    status_t returnFlags(OK);
    bool updateFormat = false;
    bool alloc = false;
    int buf = INVALID_BUFFER_SLOT;

    {
        Mutex::Autolock lock(mMutex);

        int found = -1;
        int dequeuedCount = 0;
        int renderingCount = 0;
        bool tryAgain = true;

        CNW_LOGD("dequeueBuffer: E");
        while (tryAgain) {
            if (mAbandoned) {
                CNW_LOGE("dequeueBuffer: GonkNativeWindow has been abandoned!");
                return NO_INIT;
            }
            // look for a free buffer to give to the client
            found = INVALID_BUFFER_SLOT;
            dequeuedCount = 0;
            renderingCount = 0;
            for (int i = 0; i < mBufferCount; i++) {
                const int state = mSlots[i].mBufferState;
                switch (state) {
                    case BufferSlot::DEQUEUED:
                        CNW_LOGD("dequeueBuffer: slot %d is DEQUEUED\n", i);
                        dequeuedCount++;
                        break;

                    case BufferSlot::RENDERING:
                        CNW_LOGD("dequeueBuffer: slot %d is RENDERING\n", i);
                        renderingCount++;
                        break;

                    case BufferSlot::FREE:
                        CNW_LOGD("dequeueBuffer: slot %d is FREE\n", i);
                        /* We return the oldest of the free buffers to avoid
                         * stalling the producer if possible.  This is because
                         * the consumer may still have pending reads of the
                         * buffers in flight.
                         */
                        if (found < 0 ||
                            mSlots[i].mFrameNumber < mSlots[found].mFrameNumber) {
                            found = i;
                        }
                        break;

                    default:
                        CNW_LOGD("dequeueBuffer: slot %d is %d\n", i, state);
                        break;
                }
            }

            // See whether a buffer has been in RENDERING state since the last
            // setBufferCount so we know whether to perform the
            // MIN_UNDEQUEUED_BUFFERS check below.
            if (renderingCount > 0) {
                // make sure the client is not trying to dequeue more buffers
                // than allowed.
                const int avail = mBufferCount - (dequeuedCount + 1);
                if (avail < MIN_UNDEQUEUED_BUFFERS) {
                    CNW_LOGE("dequeueBuffer: MIN_UNDEQUEUED_BUFFERS=%d exceeded "
                            "(dequeued=%d)",
                            MIN_UNDEQUEUED_BUFFERS,
                            dequeuedCount);
                    return -EBUSY;
                }
            }

            // we're in synchronous mode and didn't find a buffer, we need to
            // wait for some buffers to be consumed
            tryAgain = (found == INVALID_BUFFER_SLOT);
            if (tryAgain) {
                CNW_LOGD("dequeueBuffer: Try again");
                mDequeueCondition.wait(mMutex);
                CNW_LOGD("dequeueBuffer: Now");
            }
        }

        if (found == INVALID_BUFFER_SLOT) {
            // This should not happen.
            CNW_LOGE("dequeueBuffer: no available buffer slots");
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

        updateFormat = (format != 0);
        if (!updateFormat) {
            // keep the current (or default) format
            format = mPixelFormat;
        }

        mSlots[buf].mBufferState = BufferSlot::DEQUEUED;

        const sp<GraphicBuffer>& gbuf(mSlots[buf].mGraphicBuffer);
        if ((gbuf == NULL) ||
           ((uint32_t(gbuf->width)  != w) ||
            (uint32_t(gbuf->height) != h) ||
            (uint32_t(gbuf->format) != format) ||
            ((uint32_t(gbuf->usage) & usage) != usage))) {
            mSlots[buf].mGraphicBuffer = NULL;
            mSlots[buf].mRequestBufferCalled = false;
            if (mSlots[buf].mTextureClient) {
                mSlots[buf].mTextureClient->ClearRecycleCallback();
                // release TextureClient in ImageBridge thread
                RefPtr<TextureClientReleaseTask> task =
                  MakeAndAddRef<TextureClientReleaseTask>(mSlots[buf].mTextureClient);
                mSlots[buf].mTextureClient = NULL;
                ImageBridgeChild::GetSingleton()->GetMessageLoop()->PostTask(task.forget());
            }
            alloc = true;
        }
    }  // end lock scope

    if (alloc) {
        ClientIPCAllocator* allocator = ImageBridgeChild::GetSingleton();
        usage |= GraphicBuffer::USAGE_HW_TEXTURE;
        GrallocTextureData* texData = GrallocTextureData::Create(IntSize(w, h), format,
                                                                 gfx::BackendType::NONE, usage,
                                                                 allocator);
        if (!texData) {
            return -ENOMEM;
        }

        RefPtr<TextureClient> textureClient = new TextureClient(texData, TextureFlags::RECYCLE | TextureFlags::DEALLOCATE_CLIENT, allocator);

        { // Scope for the lock
            Mutex::Autolock lock(mMutex);

            if (mAbandoned) {
                CNW_LOGE("dequeueBuffer: SurfaceTexture has been abandoned!");
                return NO_INIT;
            }

            if (updateFormat) {
                mPixelFormat = format;
            }
            mSlots[buf].mGraphicBuffer = texData->GetGraphicBuffer();
            mSlots[buf].mTextureClient = textureClient;

            returnFlags |= ISurfaceTexture::BUFFER_NEEDS_REALLOCATION;

            CNW_LOGD("dequeueBuffer: returning slot=%d buf=%p ", buf,
                    mSlots[buf].mGraphicBuffer->handle);
        }
    }

    CNW_LOGD("dequeueBuffer: returning slot=%d buf=%p ", buf,
            mSlots[buf].mGraphicBuffer->handle );

    CNW_LOGD("dequeueBuffer: X");
    return returnFlags;
}

status_t GonkNativeWindow::setSynchronousMode(bool enabled)
{
    return NO_ERROR;
}

int GonkNativeWindow::getSlotFromBufferLocked(
        android_native_buffer_t* buffer) const
{
    if (buffer == NULL) {
        CNW_LOGE("getSlotFromBufferLocked: encountered NULL buffer");
        return BAD_VALUE;
    }

    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].mGraphicBuffer != NULL && mSlots[i].mGraphicBuffer->handle == buffer->handle) {
            return i;
        }
    }
    CNW_LOGE("getSlotFromBufferLocked: unknown buffer: %p", buffer->handle);
    return BAD_VALUE;
}

int GonkNativeWindow::getSlotFromTextureClientLocked(
        TextureClient* client) const
{
    if (client == NULL) {
        CNW_LOGE("getSlotFromBufferLocked: encountered NULL buffer");
        return BAD_VALUE;
    }

    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].mTextureClient == client) {
            return i;
        }
    }
    CNW_LOGE("getSlotFromBufferLocked: unknown TextureClient: %p", client);
    return BAD_VALUE;
}

already_AddRefed<TextureClient>
GonkNativeWindow::getTextureClientFromBuffer(ANativeWindowBuffer* buffer)
{
  int buf = getSlotFromBufferLocked(buffer);
  if (buf < 0 || buf >= mBufferCount ||
      mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
    return nullptr;
  }

  RefPtr<TextureClient> client(mSlots[buf].mTextureClient);
  return client.forget();
}

status_t GonkNativeWindow::queueBuffer(int buf, int64_t timestamp,
        uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform)
{
    {
        Mutex::Autolock lock(mMutex);
        CNW_LOGD("queueBuffer: E");
        CNW_LOGD("queueBuffer: buf=%d", buf);

        if (mAbandoned) {
            CNW_LOGE("queueBuffer: GonkNativeWindow has been abandoned!");
            return NO_INIT;
        }
        if (buf < 0 || buf >= mBufferCount) {
            CNW_LOGE("queueBuffer: slot index out of range [0, %d]: %d",
                     mBufferCount, buf);
            return -EINVAL;
        } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
            CNW_LOGE("queueBuffer: slot %d is not owned by the client "
                     "(state=%d)", buf, mSlots[buf].mBufferState);
            return -EINVAL;
        } else if (!mSlots[buf].mRequestBufferCalled) {
            CNW_LOGE("queueBuffer: slot %d was enqueued without requesting a "
                    "buffer", buf);
            return -EINVAL;
        }

        mQueue.push_back(buf);

        mSlots[buf].mBufferState = BufferSlot::QUEUED;
        mSlots[buf].mTimestamp = timestamp;
        mFrameCounter++;
        mSlots[buf].mFrameNumber = mFrameCounter;

        mDequeueCondition.signal();

        *outWidth = mDefaultWidth;
        *outHeight = mDefaultHeight;
        *outTransform = 0;
    }

    // OnNewFrame might call lockCurrentBuffer so we must release the
    // mutex first.
    if (mNewFrameCallback) {
      mNewFrameCallback->OnNewFrame();
    }
    CNW_LOGD("queueBuffer: X");
    return OK;
}


already_AddRefed<TextureClient>
GonkNativeWindow::getCurrentBuffer() {
  CNW_LOGD("GonkNativeWindow::getCurrentBuffer");
  Mutex::Autolock lock(mMutex);

  if (mAbandoned) {
    CNW_LOGE("getCurrentBuffer: GonkNativeWindow has been abandoned!");
    return NULL;
  }

  if(mQueue.empty()) {
    mDequeueCondition.signal();
    return nullptr;
  }

  Fifo::iterator front(mQueue.begin());
  int buf = *front;
  CNW_LOGD("getCurrentBuffer: buf=%d", buf);

  mSlots[buf].mBufferState = BufferSlot::RENDERING;

  mQueue.erase(front);
  mDequeueCondition.signal();

  mSlots[buf].mTextureClient->SetRecycleCallback(GonkNativeWindow::RecycleCallback, this);
  RefPtr<TextureClient> client(mSlots[buf].mTextureClient);
  return client.forget();
}


/* static */ void
GonkNativeWindow::RecycleCallback(TextureClient* client, void* closure) {
  GonkNativeWindow* nativeWindow =
    static_cast<GonkNativeWindow*>(closure);

  MOZ_ASSERT(client && !client->IsDead());
  client->ClearRecycleCallback();
  nativeWindow->returnBuffer(client);
}

void GonkNativeWindow::returnBuffer(TextureClient* client) {
  CNW_LOGD("GonkNativeWindow::returnBuffer");
  Mutex::Autolock lock(mMutex);

  if (mAbandoned) {
    CNW_LOGD("returnBuffer: GonkNativeWindow has been abandoned!");
    return;
  }

  int index = getSlotFromTextureClientLocked(client);
  if (index < 0 || index >= mBufferCount) {
    CNW_LOGE("returnBuffer: slot index out of range [0, %d]: %d",
             mBufferCount, index);
    return;
  }

  if (mSlots[index].mBufferState != BufferSlot::RENDERING) {
    CNW_LOGE("returnBuffer: slot %d is not owned by the compositor (state=%d)",
                  index, mSlots[index].mBufferState);
    return;
  }

  mSlots[index].mBufferState = BufferSlot::FREE;
  mDequeueCondition.signal();
  return;
}

void GonkNativeWindow::cancelBuffer(int buf) {
    CNW_LOGD("cancelBuffer: slot=%d", buf);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGD("cancelBuffer: GonkNativeWindow has been abandoned!");
        return;
    }

    if (buf < 0 || buf >= mBufferCount) {
        CNW_LOGE("cancelBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, buf);
        return;
    } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
        CNW_LOGE("cancelBuffer: slot %d is not owned by the client (state=%d)",
                buf, mSlots[buf].mBufferState);
        return;
    }
    mSlots[buf].mBufferState = BufferSlot::FREE;
    mSlots[buf].mFrameNumber = 0;
    mDequeueCondition.signal();
}

status_t GonkNativeWindow::setCrop(const Rect& crop) {
    return OK;
}

status_t GonkNativeWindow::setTransform(uint32_t transform) {
    return OK;
}

status_t GonkNativeWindow::connect(int api,
        uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform) {
    CNW_LOGD("connect: api=%d", api);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("connect: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }

    int err = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            if (mConnectedApi != NO_CONNECTED_API) {
                CNW_LOGE("connect: already connected (cur=%d, req=%d)",
                        mConnectedApi, api);
                err = -EINVAL;
            } else {
                mConnectedApi = api;
                *outWidth = mDefaultWidth;
                *outHeight = mDefaultHeight;
                *outTransform = 0;
            }
            break;
        default:
            err = -EINVAL;
            break;
    }
    return err;
}

status_t GonkNativeWindow::disconnect(int api) {
    CNW_LOGD("disconnect: api=%d", api);

    int err = NO_ERROR;
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
                mQueue.clear();
                freeAllBuffersLocked();
                mConnectedApi = NO_CONNECTED_API;
                mDequeueCondition.signal();
            } else {
                CNW_LOGE("disconnect: connected to another api (cur=%d, req=%d)",
                        mConnectedApi, api);
                err = -EINVAL;
            }
            break;
        default:
            CNW_LOGE("disconnect: unknown API %d", api);
            err = -EINVAL;
            break;
    }
    return err;
}

status_t GonkNativeWindow::setScalingMode(int mode) {
    return OK;
}

void GonkNativeWindow::setNewFrameCallback(
        GonkNativeWindowNewFrameCallback* aCallback) {
    CNW_LOGD("setNewFrameCallback");
    Mutex::Autolock lock(mMutex);
    mNewFrameCallback = aCallback;
}

int GonkNativeWindow::query(int what, int* outValue)
{
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("query: GonkNativeWindow has been abandoned!");
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
        value = mPixelFormat;
        break;
    case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
        value = MIN_UNDEQUEUED_BUFFERS;
        break;
    default:
        return BAD_VALUE;
    }
    outValue[0] = value;
    return NO_ERROR;
}
