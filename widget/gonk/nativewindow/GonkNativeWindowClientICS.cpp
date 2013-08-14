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

#include "CameraCommon.h"
#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"
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

GonkNativeWindowClient::GonkNativeWindowClient(const sp<GonkNativeWindow>& window)
  : mNativeWindow(window) {
    GonkNativeWindowClient::init();
}

GonkNativeWindowClient::~GonkNativeWindowClient() {
    if (mConnectedToCpu) {
        GonkNativeWindowClient::disconnect(NATIVE_WINDOW_API_CPU);
    }
}

void GonkNativeWindowClient::init() {
    // Initialize the ANativeWindow function pointers.
    ANativeWindow::setSwapInterval  = hook_setSwapInterval;
    ANativeWindow::dequeueBuffer    = hook_dequeueBuffer;
    ANativeWindow::cancelBuffer     = hook_cancelBuffer;
    ANativeWindow::lockBuffer       = hook_lockBuffer;
    ANativeWindow::queueBuffer      = hook_queueBuffer;
    ANativeWindow::query            = hook_query;
    ANativeWindow::perform          = hook_perform;

    mReqWidth = 0;
    mReqHeight = 0;
    mReqFormat = 0;
    mReqUsage = 0;
    mTimestamp = NATIVE_WINDOW_TIMESTAMP_AUTO;
    mDefaultWidth = 0;
    mDefaultHeight = 0;
    mTransformHint = 0;
    mConnectedToCpu = false;
}


int GonkNativeWindowClient::hook_setSwapInterval(ANativeWindow* window, int interval) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->setSwapInterval(interval);
}

int GonkNativeWindowClient::hook_dequeueBuffer(ANativeWindow* window,
        ANativeWindowBuffer** buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->dequeueBuffer(buffer);
}

int GonkNativeWindowClient::hook_cancelBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->cancelBuffer(buffer);
}

int GonkNativeWindowClient::hook_lockBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->lockBuffer(buffer);
}

int GonkNativeWindowClient::hook_queueBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->queueBuffer(buffer);
}

int GonkNativeWindowClient::hook_query(const ANativeWindow* window,
                                int what, int* value) {
    const GonkNativeWindowClient* c = getSelf(window);
    return c->query(what, value);
}

int GonkNativeWindowClient::hook_perform(ANativeWindow* window, int operation, ...) {
    va_list args;
    va_start(args, operation);
    GonkNativeWindowClient* c = getSelf(window);
    return c->perform(operation, args);
}

int GonkNativeWindowClient::setSwapInterval(int interval) {
    return NO_ERROR;
}

int GonkNativeWindowClient::dequeueBuffer(android_native_buffer_t** buffer) {
    CNW_LOGD("GonkNativeWindowClient::dequeueBuffer");
    Mutex::Autolock lock(mMutex);
    int buf = -1;
    status_t result = mNativeWindow->dequeueBuffer(&buf, mReqWidth, mReqHeight,
            mReqFormat, mReqUsage);
    if (result < 0) {
        CNW_LOGD("dequeueBuffer: ISurfaceTexture::dequeueBuffer(%d, %d, %d, %d)"
             "failed: %d", mReqWidth, mReqHeight, mReqFormat, mReqUsage,
             result);
        return result;
    }
    sp<GraphicBuffer>& gbuf(mSlots[buf]);
    if (result & ISurfaceTexture::RELEASE_ALL_BUFFERS) {
        freeAllBuffers();
    }

    if ((result & ISurfaceTexture::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {
        result = mNativeWindow->requestBuffer(buf, &gbuf);
        if (result != NO_ERROR) {
            CNW_LOGE("dequeueBuffer: ISurfaceTexture::requestBuffer failed: %d",
                    result);
            return result;
        }
    }
    *buffer = gbuf.get();
    return OK;
}

int GonkNativeWindowClient::cancelBuffer(ANativeWindowBuffer* buffer) {
    CNW_LOGD("GonkNativeWindowClient::cancelBuffer");
    Mutex::Autolock lock(mMutex);
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        return i;
    }
    mNativeWindow->cancelBuffer(i);
    return OK;
}

int GonkNativeWindowClient::getSlotFromBufferLocked(
        android_native_buffer_t* buffer) const {
    bool dumpedState = false;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        // XXX: Dump the slots whenever we hit a NULL entry while searching for
        // a buffer.
        if (mSlots[i] == NULL) {
            if (!dumpedState) {
                CNW_LOGD("getSlotFromBufferLocked: encountered NULL buffer in slot %d "
                        "looking for buffer %p", i, buffer->handle);
                for (int j = 0; j < NUM_BUFFER_SLOTS; j++) {
                    if (mSlots[j] == NULL) {
                        CNW_LOGD("getSlotFromBufferLocked:   %02d: NULL", j);
                    } else {
                        CNW_LOGD("getSlotFromBufferLocked:   %02d: %p", j, mSlots[j]->handle);
                    }
                }
                dumpedState = true;
            }
        }

        if (mSlots[i] != NULL && mSlots[i]->handle == buffer->handle) {
            return i;
        }
    }
    CNW_LOGE("getSlotFromBufferLocked: unknown buffer: %p", buffer->handle);
    return BAD_VALUE;
}


int GonkNativeWindowClient::lockBuffer(android_native_buffer_t* buffer) {
    CNW_LOGD("GonkNativeWindowClient::lockBuffer");
    Mutex::Autolock lock(mMutex);
    return OK;
}

int GonkNativeWindowClient::queueBuffer(android_native_buffer_t* buffer) {
    CNW_LOGD("GonkNativeWindowClient::queueBuffer");
    Mutex::Autolock lock(mMutex);
    int64_t timestamp;
    if (mTimestamp == NATIVE_WINDOW_TIMESTAMP_AUTO) {
        timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
        CNW_LOGD("GonkNativeWindowClient::queueBuffer making up timestamp: %.2f ms",
             timestamp / 1000000.f);
    } else {
        timestamp = mTimestamp;
    }
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        return i;
    }
    status_t err = mNativeWindow->queueBuffer(i, timestamp,
            &mDefaultWidth, &mDefaultHeight, &mTransformHint);
    if (err != OK)  {
        CNW_LOGE("queueBuffer: error queuing buffer to GonkNativeWindow, %d", err);
    }
    return err;
}

int GonkNativeWindowClient::query(int what, int* value) const {
    CNW_LOGD("query");
    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        switch (what) {
            case NATIVE_WINDOW_FORMAT:
                if (mReqFormat) {
                    *value = mReqFormat;
                    return NO_ERROR;
                }
                break;
            case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER:
                *value = 0;
                return NO_ERROR;
            case NATIVE_WINDOW_CONCRETE_TYPE:
                *value = NATIVE_WINDOW_SURFACE_TEXTURE_CLIENT;
                return NO_ERROR;
            case NATIVE_WINDOW_DEFAULT_WIDTH:
                *value = mDefaultWidth;
                return NO_ERROR;
            case NATIVE_WINDOW_DEFAULT_HEIGHT:
                *value = mDefaultHeight;
                return NO_ERROR;
            case NATIVE_WINDOW_TRANSFORM_HINT:
                *value = mTransformHint;
                return NO_ERROR;
        }
    }
    return mNativeWindow->query(what, value);
}

int GonkNativeWindowClient::perform(int operation, va_list args)
{
    int res = NO_ERROR;
    switch (operation) {
    case NATIVE_WINDOW_CONNECT:
        // deprecated. must return NO_ERROR.
        break;
    case NATIVE_WINDOW_DISCONNECT:
        // deprecated. must return NO_ERROR.
        break;
    case NATIVE_WINDOW_SET_SCALING_MODE:
        return NO_ERROR;
    case NATIVE_WINDOW_SET_USAGE:
        res = dispatchSetUsage(args);
        break;
    case NATIVE_WINDOW_SET_CROP:
        //not implemented
        break;
    case NATIVE_WINDOW_SET_BUFFER_COUNT:
        res = dispatchSetBufferCount(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY:
        res = dispatchSetBuffersGeometry(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
        //not implemented
        break;
    case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP:
        res = dispatchSetBuffersTimestamp(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
        res = dispatchSetBuffersDimensions(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
        res = dispatchSetBuffersFormat(args);
        break;
    case NATIVE_WINDOW_LOCK:
        res = INVALID_OPERATION;// not supported
        break;
    case NATIVE_WINDOW_UNLOCK_AND_POST:
        res = INVALID_OPERATION;// not supported
        break;
    case NATIVE_WINDOW_API_CONNECT:
        res = dispatchConnect(args);
        break;
    case NATIVE_WINDOW_API_DISCONNECT:
        res = dispatchDisconnect(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_SIZE:
        //not implemented
        break;
    default:
        res = NAME_NOT_FOUND;
        break;
    }
    return res;
}

int GonkNativeWindowClient::dispatchConnect(va_list args) {
    int api = va_arg(args, int);
    return connect(api);
}

int GonkNativeWindowClient::dispatchDisconnect(va_list args) {
    int api = va_arg(args, int);
    return disconnect(api);
}

int GonkNativeWindowClient::dispatchSetUsage(va_list args) {
    int usage = va_arg(args, int);
    return setUsage(usage);
}

int GonkNativeWindowClient::dispatchSetBufferCount(va_list args) {
    size_t bufferCount = va_arg(args, size_t);
    return setBufferCount(bufferCount);
}

int GonkNativeWindowClient::dispatchSetBuffersGeometry(va_list args) {
    int w = va_arg(args, int);
    int h = va_arg(args, int);
    int f = va_arg(args, int);
    int err = setBuffersDimensions(w, h);
    if (err != 0) {
        return err;
    }
    return setBuffersFormat(f);
}

int GonkNativeWindowClient::dispatchSetBuffersDimensions(va_list args) {
    int w = va_arg(args, int);
    int h = va_arg(args, int);
    return setBuffersDimensions(w, h);
}

int GonkNativeWindowClient::dispatchSetBuffersFormat(va_list args) {
    int f = va_arg(args, int);
    return setBuffersFormat(f);
}

int GonkNativeWindowClient::dispatchSetBuffersTimestamp(va_list args) {
    int64_t timestamp = va_arg(args, int64_t);
    return setBuffersTimestamp(timestamp);
}

int GonkNativeWindowClient::connect(int api) {
    CNW_LOGD("GonkNativeWindowClient::connect");
    Mutex::Autolock lock(mMutex);
    int err = mNativeWindow->connect(api,
            &mDefaultWidth, &mDefaultHeight, &mTransformHint);
    if (!err && api == NATIVE_WINDOW_API_CPU) {
        mConnectedToCpu = true;
    }
    return err;
}

int GonkNativeWindowClient::disconnect(int api) {
    CNW_LOGD("GonkNativeWindowClient::disconnect");
    Mutex::Autolock lock(mMutex);
    freeAllBuffers();
    int err = mNativeWindow->disconnect(api);
    if (!err) {
        mReqFormat = 0;
        mReqWidth = 0;
        mReqHeight = 0;
        mReqUsage = 0;
        if (api == NATIVE_WINDOW_API_CPU) {
            mConnectedToCpu = false;
        }
    }
    return err;
}

int GonkNativeWindowClient::setUsage(uint32_t reqUsage)
{
    CNW_LOGD("GonkNativeWindowClient::setUsage");
    Mutex::Autolock lock(mMutex);
    mReqUsage = reqUsage;
    return OK;
}

int GonkNativeWindowClient::setBufferCount(int bufferCount)
{
    CNW_LOGD("GonkNativeWindowClient::setBufferCount");
    Mutex::Autolock lock(mMutex);

    status_t err = mNativeWindow->setBufferCount(bufferCount);
    if (err == NO_ERROR) {
        freeAllBuffers();
    }

    return err;
}

int GonkNativeWindowClient::setBuffersDimensions(int w, int h)
{
    CNW_LOGD("GonkNativeWindowClient::setBuffersDimensions");
    Mutex::Autolock lock(mMutex);

    if (w<0 || h<0)
        return BAD_VALUE;

    if ((w && !h) || (!w && h))
        return BAD_VALUE;

    mReqWidth = w;
    mReqHeight = h;

    status_t err = OK;

    return err;
}

int GonkNativeWindowClient::setBuffersFormat(int format)
{
    CNW_LOGD("GonkNativeWindowClient::setBuffersFormat");
    Mutex::Autolock lock(mMutex);

    if (format<0)
        return BAD_VALUE;

    mReqFormat = format;

    return NO_ERROR;
}


int GonkNativeWindowClient::setBuffersTimestamp(int64_t timestamp)
{
    CNW_LOGD("GonkNativeWindowClient::setBuffersTimestamp");
    Mutex::Autolock lock(mMutex);
    mTimestamp = timestamp;
    return NO_ERROR;
}

void GonkNativeWindowClient::freeAllBuffers() {
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        mSlots[i] = 0;
    }
}

