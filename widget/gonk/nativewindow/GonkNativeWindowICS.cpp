/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012 Mozilla Foundation
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
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "GonkNativeWindow.h"
#include "nsDebug.h"

/**
 * DOM_CAMERA_LOGI() is enabled in debug builds, and turned on by setting
 * NSPR_LOG_MODULES=Camera:N environment variable, where N >= 3.
 *
 * CNW_LOGE() is always enabled.
 */
#define CNW_LOGD(...)   DOM_CAMERA_LOGI(__VA_ARGS__)
#define CNW_LOGE(...)   {(void)printf_stderr(__VA_ARGS__);}

using namespace android;
using namespace mozilla::layers;

GonkNativeWindow::GonkNativeWindow() :
    mDefaultWidth(1),
    mDefaultHeight(1),
    mPixelFormat(PIXEL_FORMAT_RGBA_8888),
    mBufferCount(MIN_BUFFER_SLOTS + 1),
    mConnectedApi(NO_CONNECTED_API),
    mAbandoned(false),
    mFrameCounter(0),
    mGeneration(0),
    mNewFrameCallback(nullptr) {
}

GonkNativeWindow::~GonkNativeWindow() {
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    freeAllBuffersLocked(freeList);
    releaseBufferFreeListUnlocked(freeList);
}

void GonkNativeWindow::abandon()
{
    CNW_LOGD("abandon");
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    {
        Mutex::Autolock lock(mMutex);
        mQueue.clear();
        mAbandoned = true;
        freeAllBuffersLocked(freeList);
    }

    releaseBufferFreeListUnlocked(freeList);
    mDequeueCondition.signal();
}

void GonkNativeWindow::freeAllBuffersLocked(nsTArray<SurfaceDescriptor>& freeList)
{
    CNW_LOGD("freeAllBuffersLocked: from generation %d", mGeneration);
    ++mGeneration;

    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mGraphicBuffer != NULL) {
            // Don't try to destroy the gralloc buffer if it is still in the
            // video stream awaiting rendering.
            if (mSlots[i].mBufferState != BufferSlot::RENDERING) {
                SurfaceDescriptor* desc = freeList.AppendElement();
                *desc = mSlots[i].mSurfaceDescriptor;
            }
            mSlots[i].mGraphicBuffer = NULL;
            mSlots[i].mBufferState = BufferSlot::FREE;
            mSlots[i].mFrameNumber = 0;
        }
    }
}

void GonkNativeWindow::releaseBufferFreeListUnlocked(nsTArray<SurfaceDescriptor>& freeList)
{
    // This function MUST ONLY be called with mMutex unlocked; else there
    // is a risk of deadlock with the ImageBridge thread.

    CNW_LOGD("releaseBufferFreeListUnlocked: E");
    ImageBridgeChild *ibc = ImageBridgeChild::GetSingleton();

    for (uint32_t i = 0; i < freeList.Length(); ++i) {
        ibc->DeallocSurfaceDescriptorGralloc(freeList[i]);
    }

    freeList.Clear();
    CNW_LOGD("releaseBufferFreeListUnlocked: X");
}

void GonkNativeWindow::clearRenderingStateBuffersLocked()
{
    ++mGeneration;

    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mGraphicBuffer != NULL) {
            // Don't try to destroy the gralloc buffer if it is still in the
            // video stream awaiting rendering.
            if (mSlots[i].mBufferState == BufferSlot::RENDERING) {
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
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;

    {
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
        freeAllBuffersLocked(freeList);
        mBufferCount = bufferCount;
        mQueue.clear();
        mDequeueCondition.signal();
    }

    releaseBufferFreeListUnlocked(freeList);
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
    uint32_t generation;
    bool alloc = false;
    int buf = INVALID_BUFFER_SLOT;
    SurfaceDescriptor descOld;

    {
        Mutex::Autolock lock(mMutex);
        generation = mGeneration;

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
                if (state == BufferSlot::DEQUEUED) {
                    dequeuedCount++;
                }
                else if (state == BufferSlot::RENDERING) {
                    renderingCount++;
                }
                else if (state == BufferSlot::FREE) {
                    /* We return the oldest of the free buffers to avoid
                     * stalling the producer if possible.  This is because
                     * the consumer may still have pending reads of the
                     * buffers in flight.
                     */
                    if (found < 0 ||
                        mSlots[i].mFrameNumber < mSlots[found].mFrameNumber) {
                        found = i;
                    }
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
        alloc = (gbuf == NULL);
        if ((gbuf!=NULL) &&
           ((uint32_t(gbuf->width)  != w) ||
            (uint32_t(gbuf->height) != h) ||
            (uint32_t(gbuf->format) != format) ||
            ((uint32_t(gbuf->usage) & usage) != usage))) {
            alloc = true;
            descOld = mSlots[buf].mSurfaceDescriptor;
        }
    }
    
    // At this point, the buffer is now marked DEQUEUED, and no one else
    // should touch it, except for freeAllBuffersLocked(); we handle that
    // after trying to create the surface descriptor below.
    //
    // So we don't need mMutex locked, which would otherwise run the risk
    // of a deadlock on calling AllocSurfaceDescriptorGralloc().    

    SurfaceDescriptor desc;
    ImageBridgeChild* ibc;
    sp<GraphicBuffer> graphicBuffer;
    if (alloc) {
        usage |= GraphicBuffer::USAGE_HW_TEXTURE;
        status_t error;
        ibc = ImageBridgeChild::GetSingleton();
        CNW_LOGD("dequeueBuffer: about to alloc surface descriptor");
        ibc->AllocSurfaceDescriptorGralloc(gfxIntSize(w, h),
                                           format,
                                           usage,
                                           &desc);
        // We can only use a gralloc buffer here.  If we didn't get
        // one back, something went wrong.
        CNW_LOGD("dequeueBuffer: got surface descriptor");
        if (SurfaceDescriptor::TSurfaceDescriptorGralloc != desc.type()) {
            MOZ_ASSERT(SurfaceDescriptor::T__None == desc.type());
            CNW_LOGE("dequeueBuffer: failed to alloc gralloc buffer");
            return -ENOMEM;
        }
        graphicBuffer = GrallocBufferActor::GetFrom(desc.get_SurfaceDescriptorGralloc());
        error = graphicBuffer->initCheck();
        if (error != NO_ERROR) {
            CNW_LOGE("dequeueBuffer: createGraphicBuffer failed with error %d", error);
            return error;
        }
    }

    bool tooOld = false;
    {
        Mutex::Autolock lock(mMutex);
        if (generation == mGeneration) {
            if (updateFormat) {
                mPixelFormat = format;
            }
            if (alloc) {
                mSlots[buf].mGraphicBuffer = graphicBuffer;
                mSlots[buf].mSurfaceDescriptor = desc;
                mSlots[buf].mSurfaceDescriptor.get_SurfaceDescriptorGralloc().external() = true;
                mSlots[buf].mRequestBufferCalled = false;

                returnFlags |= ISurfaceTexture::BUFFER_NEEDS_REALLOCATION;
            }
            CNW_LOGD("dequeueBuffer: returning slot=%d buf=%p ", buf,
                    mSlots[buf].mGraphicBuffer->handle);
        } else {
            tooOld = true;
        }
    }

    if (alloc && IsSurfaceDescriptorValid(descOld)) {
        ibc->DeallocSurfaceDescriptorGralloc(descOld);
    }

    if (alloc && tooOld) {
        ibc->DeallocSurfaceDescriptorGralloc(desc);
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

mozilla::layers::SurfaceDescriptor *
GonkNativeWindow::getSurfaceDescriptorFromBuffer(ANativeWindowBuffer* buffer)
{
  int buf = getSlotFromBufferLocked(buffer);
  if (buf < 0 || buf >= mBufferCount ||
      mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
    return nullptr;
  }

  return &mSlots[buf].mSurfaceDescriptor;
}

status_t GonkNativeWindow::queueBuffer(int buf, int64_t timestamp,
        uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform)
{
    {
        Mutex::Autolock lock(mMutex);
        CNW_LOGD("queueBuffer: E");

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


already_AddRefed<GraphicBufferLocked>
GonkNativeWindow::getCurrentBuffer()
{
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

  mSlots[buf].mBufferState = BufferSlot::RENDERING;

  nsRefPtr<GraphicBufferLocked> ret =
    new CameraGraphicBuffer(this, buf, mGeneration, mSlots[buf].mSurfaceDescriptor);
  mQueue.erase(front);
  mDequeueCondition.signal();
  return ret.forget();
}

bool
GonkNativeWindow::returnBuffer(uint32_t aIndex, uint32_t aGeneration)
{
  CNW_LOGD("GonkNativeWindow::returnBuffer: slot=%d (generation=%d)", aIndex, aGeneration);
  Mutex::Autolock lock(mMutex);

  if (mAbandoned) {
    CNW_LOGD("returnBuffer: GonkNativeWindow has been abandoned!");
    return false;
  }

  if (aGeneration != mGeneration) {
    CNW_LOGD("returnBuffer: buffer is from generation %d (current is %d)",
      aGeneration, mGeneration);
    return false;
  }
  if (aIndex >= mBufferCount) {
    CNW_LOGE("returnBuffer: slot index out of range [0, %d]: %d",
             mBufferCount, aIndex);
    return false;
  }
  if (mSlots[aIndex].mBufferState != BufferSlot::RENDERING) {
    CNW_LOGE("returnBuffer: slot %d is not owned by the compositor (state=%d)",
                  aIndex, mSlots[aIndex].mBufferState);
    return false;
  }

  mSlots[aIndex].mBufferState = BufferSlot::FREE;
  mDequeueCondition.signal();
  return true;
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
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    {
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
                    freeAllBuffersLocked(freeList);
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
    }
    releaseBufferFreeListUnlocked(freeList);
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
