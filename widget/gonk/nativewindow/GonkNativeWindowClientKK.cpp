/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "GonkNativeWindowClient"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <android/native_window.h>

#include <binder/Parcel.h>

#include <utils/Log.h>
#include <utils/Trace.h>

#include <ui/Fence.h>

#include "GonkNativeWindowClientKK.h"

namespace android {

GonkNativeWindowClient::GonkNativeWindowClient(
        const sp<IGraphicBufferProducer>& bufferProducer,
        bool controlledByApp)
    : mGraphicBufferProducer(bufferProducer)
{
    // Initialize the ANativeWindow function pointers.
    ANativeWindow::setSwapInterval  = hook_setSwapInterval;
    ANativeWindow::dequeueBuffer    = hook_dequeueBuffer;
    ANativeWindow::cancelBuffer     = hook_cancelBuffer;
    ANativeWindow::queueBuffer      = hook_queueBuffer;
    ANativeWindow::query            = hook_query;
    ANativeWindow::perform          = hook_perform;

    ANativeWindow::dequeueBuffer_DEPRECATED = hook_dequeueBuffer_DEPRECATED;
    ANativeWindow::cancelBuffer_DEPRECATED  = hook_cancelBuffer_DEPRECATED;
    ANativeWindow::lockBuffer_DEPRECATED    = hook_lockBuffer_DEPRECATED;
    ANativeWindow::queueBuffer_DEPRECATED   = hook_queueBuffer_DEPRECATED;

    const_cast<int&>(ANativeWindow::minSwapInterval) = 0;
    const_cast<int&>(ANativeWindow::maxSwapInterval) = 1;

    mReqWidth = 0;
    mReqHeight = 0;
    mReqFormat = 0;
    mReqUsage = 0;
    mTimestamp = NATIVE_WINDOW_TIMESTAMP_AUTO;
    mCrop.clear();
    mScalingMode = NATIVE_WINDOW_SCALING_MODE_FREEZE;
    mTransform = 0;
    mDefaultWidth = 0;
    mDefaultHeight = 0;
    mUserWidth = 0;
    mUserHeight = 0;
    mTransformHint = 0;
    mConsumerRunningBehind = false;
    mConnectedToCpu = false;
    mProducerControlledByApp = controlledByApp;
    mSwapIntervalZero = false;
}

GonkNativeWindowClient::~GonkNativeWindowClient() {
    if (mConnectedToCpu) {
        GonkNativeWindowClient::disconnect(NATIVE_WINDOW_API_CPU);
    }
}

sp<IGraphicBufferProducer> GonkNativeWindowClient::getIGraphicBufferProducer() const {
    return mGraphicBufferProducer;
}

int GonkNativeWindowClient::hook_setSwapInterval(ANativeWindow* window, int interval) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->setSwapInterval(interval);
}

int GonkNativeWindowClient::hook_dequeueBuffer(ANativeWindow* window,
        ANativeWindowBuffer** buffer, int* fenceFd) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->dequeueBuffer(buffer, fenceFd);
}

int GonkNativeWindowClient::hook_cancelBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer, int fenceFd) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->cancelBuffer(buffer, fenceFd);
}

int GonkNativeWindowClient::hook_queueBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer, int fenceFd) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->queueBuffer(buffer, fenceFd);
}

int GonkNativeWindowClient::hook_dequeueBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer** buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    ANativeWindowBuffer* buf;
    int fenceFd = -1;
    int result = c->dequeueBuffer(&buf, &fenceFd);
    sp<Fence> fence(new Fence(fenceFd));
    int waitResult = fence->waitForever("dequeueBuffer_DEPRECATED");
    if (waitResult != OK) {
        ALOGE("dequeueBuffer_DEPRECATED: Fence::wait returned an error: %d",
                waitResult);
        c->cancelBuffer(buf, -1);
        return waitResult;
    }
    *buffer = buf;
    return result;
}

int GonkNativeWindowClient::hook_cancelBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->cancelBuffer(buffer, -1);
}

int GonkNativeWindowClient::hook_lockBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->lockBuffer_DEPRECATED(buffer);
}

int GonkNativeWindowClient::hook_queueBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    GonkNativeWindowClient* c = getSelf(window);
    return c->queueBuffer(buffer, -1);
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
    ATRACE_CALL();
    // EGL specification states:
    //  interval is silently clamped to minimum and maximum implementation
    //  dependent values before being stored.

    if (interval < minSwapInterval)
        interval = minSwapInterval;

    if (interval > maxSwapInterval)
        interval = maxSwapInterval;

    return NO_ERROR;
}

int GonkNativeWindowClient::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) {
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::dequeueBuffer");
    Mutex::Autolock lock(mMutex);
    int buf = -1;
    int reqW = mReqWidth ? mReqWidth : mUserWidth;
    int reqH = mReqHeight ? mReqHeight : mUserHeight;
    sp<Fence> fence;
    status_t result = mGraphicBufferProducer->dequeueBuffer(&buf, &fence, mSwapIntervalZero,
            reqW, reqH, mReqFormat, mReqUsage);
    if (result < 0) {
        ALOGV("dequeueBuffer: IGraphicBufferProducer::dequeueBuffer(%d, %d, %d, %d)"
             "failed: %d", mReqWidth, mReqHeight, mReqFormat, mReqUsage,
             result);
        return result;
    }
    sp<GraphicBuffer>& gbuf(mSlots[buf].buffer);

    // this should never happen
    ALOGE_IF(fence == NULL, "Surface::dequeueBuffer: received null Fence! buf=%d", buf);

    if (result & IGraphicBufferProducer::RELEASE_ALL_BUFFERS) {
        freeAllBuffers();
    }

    if ((result & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {
        result = mGraphicBufferProducer->requestBuffer(buf, &gbuf);
        if (result != NO_ERROR) {
            ALOGE("dequeueBuffer: IGraphicBufferProducer::requestBuffer failed: %d", result);
            return result;
        }
    }

    if (fence->isValid()) {
        *fenceFd = fence->dup();
        if (*fenceFd == -1) {
            ALOGE("dequeueBuffer: error duping fence: %d", errno);
            // dup() should never fail; something is badly wrong. Soldier on
            // and hope for the best; the worst that should happen is some
            // visible corruption that lasts until the next frame.
        }
    } else {
        *fenceFd = -1;
    }

    *buffer = gbuf.get();
    return OK;
}

int GonkNativeWindowClient::cancelBuffer(android_native_buffer_t* buffer,
        int fenceFd) {
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::cancelBuffer");
    Mutex::Autolock lock(mMutex);
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        return i;
    }
    sp<Fence> fence(fenceFd >= 0 ? new Fence(fenceFd) : Fence::NO_FENCE);
    mGraphicBufferProducer->cancelBuffer(i, fence);
    return OK;
}

int GonkNativeWindowClient::getSlotFromBufferLocked(
        android_native_buffer_t* buffer) const {
    bool dumpedState = false;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].buffer != NULL &&
                mSlots[i].buffer->handle == buffer->handle) {
            return i;
        }
    }
    ALOGE("getSlotFromBufferLocked: unknown buffer: %p", buffer->handle);
    return BAD_VALUE;
}

int GonkNativeWindowClient::lockBuffer_DEPRECATED(android_native_buffer_t* buffer) {
    ALOGV("GonkNativeWindowClient::lockBuffer");
    Mutex::Autolock lock(mMutex);
    return OK;
}

int GonkNativeWindowClient::queueBuffer(android_native_buffer_t* buffer, int fenceFd) {
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::queueBuffer");
    Mutex::Autolock lock(mMutex);
    int64_t timestamp;
    bool isAutoTimestamp = false;
    if (mTimestamp == NATIVE_WINDOW_TIMESTAMP_AUTO) {
        timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
        isAutoTimestamp = true;
        ALOGV("GonkNativeWindowClient::queueBuffer making up timestamp: %.2f ms",
             timestamp / 1000000.f);
    } else {
        timestamp = mTimestamp;
    }
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        return i;
    }


    // Make sure the crop rectangle is entirely inside the buffer.
    Rect crop;
    mCrop.intersect(Rect(buffer->width, buffer->height), &crop);

    sp<Fence> fence(fenceFd >= 0 ? new Fence(fenceFd) : Fence::NO_FENCE);
    IGraphicBufferProducer::QueueBufferOutput output;
    IGraphicBufferProducer::QueueBufferInput input(timestamp, isAutoTimestamp,
            crop, mScalingMode, mTransform, mSwapIntervalZero, fence);
    status_t err = mGraphicBufferProducer->queueBuffer(i, input, &output);
    if (err != OK)  {
        ALOGE("queueBuffer: error queuing buffer to SurfaceTexture, %d", err);
    }
    uint32_t numPendingBuffers = 0;
    output.deflate(&mDefaultWidth, &mDefaultHeight, &mTransformHint,
            &numPendingBuffers);

    mConsumerRunningBehind = (numPendingBuffers >= 2);

    return err;
}

int GonkNativeWindowClient::query(int what, int* value) const {
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::query");
    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        switch (what) {
            case NATIVE_WINDOW_FORMAT:
                if (mReqFormat) {
                    *value = mReqFormat;
                    return NO_ERROR;
                }
                break;
            case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER: {
                //sp<ISurfaceComposer> composer(
                //        ComposerService::getComposerService());
                //if (composer->authenticateSurfaceTexture(mGraphicBufferProducer)) {
                //    *value = 1;
                //} else {
                    *value = 0;
                //}
                return NO_ERROR;
            }
            case NATIVE_WINDOW_CONCRETE_TYPE:
                *value = NATIVE_WINDOW_SURFACE;
                return NO_ERROR;
            case NATIVE_WINDOW_DEFAULT_WIDTH:
                *value = mUserWidth ? mUserWidth : mDefaultWidth;
                return NO_ERROR;
            case NATIVE_WINDOW_DEFAULT_HEIGHT:
                *value = mUserHeight ? mUserHeight : mDefaultHeight;
                return NO_ERROR;
            case NATIVE_WINDOW_TRANSFORM_HINT:
                *value = mTransformHint;
                return NO_ERROR;
            case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND: {
                status_t err = NO_ERROR;
                if (!mConsumerRunningBehind) {
                    *value = 0;
                } else {
                    err = mGraphicBufferProducer->query(what, value);
                    if (err == NO_ERROR) {
                        mConsumerRunningBehind = *value;
                    }
                }
                return err;
            }
        }
    }
    return mGraphicBufferProducer->query(what, value);
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
    case NATIVE_WINDOW_SET_USAGE:
        res = dispatchSetUsage(args);
        break;
    case NATIVE_WINDOW_SET_CROP:
        res = dispatchSetCrop(args);
        break;
    case NATIVE_WINDOW_SET_BUFFER_COUNT:
        res = dispatchSetBufferCount(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY:
        res = dispatchSetBuffersGeometry(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
        res = dispatchSetBuffersTransform(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP:
        res = dispatchSetBuffersTimestamp(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
        res = dispatchSetBuffersDimensions(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS:
        res = dispatchSetBuffersUserDimensions(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
        res = dispatchSetBuffersFormat(args);
        break;
    case NATIVE_WINDOW_LOCK:
        res = dispatchLock(args);
        break;
    case NATIVE_WINDOW_UNLOCK_AND_POST:
        res = dispatchUnlockAndPost(args);
        break;
    case NATIVE_WINDOW_SET_SCALING_MODE:
        res = dispatchSetScalingMode(args);
        break;
    case NATIVE_WINDOW_API_CONNECT:
        res = dispatchConnect(args);
        break;
    case NATIVE_WINDOW_API_DISCONNECT:
        res = dispatchDisconnect(args);
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

int GonkNativeWindowClient::dispatchSetCrop(va_list args) {
    android_native_rect_t const* rect = va_arg(args, android_native_rect_t*);
    return setCrop(reinterpret_cast<Rect const*>(rect));
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

int GonkNativeWindowClient::dispatchSetBuffersUserDimensions(va_list args) {
    int w = va_arg(args, int);
    int h = va_arg(args, int);
    return setBuffersUserDimensions(w, h);
}

int GonkNativeWindowClient::dispatchSetBuffersFormat(va_list args) {
    int f = va_arg(args, int);
    return setBuffersFormat(f);
}

int GonkNativeWindowClient::dispatchSetScalingMode(va_list args) {
    int m = va_arg(args, int);
    return setScalingMode(m);
}

int GonkNativeWindowClient::dispatchSetBuffersTransform(va_list args) {
    int transform = va_arg(args, int);
    return setBuffersTransform(transform);
}

int GonkNativeWindowClient::dispatchSetBuffersTimestamp(va_list args) {
    int64_t timestamp = va_arg(args, int64_t);
    return setBuffersTimestamp(timestamp);
}

int GonkNativeWindowClient::dispatchLock(va_list args) {
    ANativeWindow_Buffer* outBuffer = va_arg(args, ANativeWindow_Buffer*);
    ARect* inOutDirtyBounds = va_arg(args, ARect*);
    return lock(outBuffer, inOutDirtyBounds);
}

int GonkNativeWindowClient::dispatchUnlockAndPost(va_list args) {
    return unlockAndPost();
}


int GonkNativeWindowClient::connect(int api) {
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::connect");
    static sp<BBinder> sLife = new BBinder();
    Mutex::Autolock lock(mMutex);
    IGraphicBufferProducer::QueueBufferOutput output;
    int err = mGraphicBufferProducer->connect(sLife, api, true, &output);
    if (err == NO_ERROR) {
        uint32_t numPendingBuffers = 0;
        output.deflate(&mDefaultWidth, &mDefaultHeight, &mTransformHint,
                &numPendingBuffers);
        mConsumerRunningBehind = (numPendingBuffers >= 2);
    }
    if (!err && api == NATIVE_WINDOW_API_CPU) {
        mConnectedToCpu = true;
    }
    return err;
}


int GonkNativeWindowClient::disconnect(int api) {
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::disconnect");
    Mutex::Autolock lock(mMutex);
    freeAllBuffers();
    int err = mGraphicBufferProducer->disconnect(api);
    if (!err) {
        mReqFormat = 0;
        mReqWidth = 0;
        mReqHeight = 0;
        mReqUsage = 0;
        mCrop.clear();
        mScalingMode = NATIVE_WINDOW_SCALING_MODE_FREEZE;
        mTransform = 0;
        if (api == NATIVE_WINDOW_API_CPU) {
            mConnectedToCpu = false;
        }
    }
    return err;
}

int GonkNativeWindowClient::setUsage(uint32_t reqUsage)
{
    ALOGV("GonkNativeWindowClient::setUsage");
    Mutex::Autolock lock(mMutex);
    mReqUsage = reqUsage;
    return OK;
}

int GonkNativeWindowClient::setCrop(Rect const* rect)
{
    ATRACE_CALL();

    Rect realRect;
    if (rect == NULL || rect->isEmpty()) {
        realRect.clear();
    } else {
        realRect = *rect;
    }

    ALOGV("GonkNativeWindowClient::setCrop rect=[%d %d %d %d]",
            realRect.left, realRect.top, realRect.right, realRect.bottom);

    Mutex::Autolock lock(mMutex);
    mCrop = realRect;
    return NO_ERROR;
}

int GonkNativeWindowClient::setBufferCount(int bufferCount)
{
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::setBufferCount");
    Mutex::Autolock lock(mMutex);

    status_t err = mGraphicBufferProducer->setBufferCount(bufferCount);
    ALOGE_IF(err, "IGraphicBufferProducer::setBufferCount(%d) returned %s",
            bufferCount, strerror(-err));

    if (err == NO_ERROR) {
        freeAllBuffers();
    }

    return err;
}

int GonkNativeWindowClient::setBuffersDimensions(int w, int h)
{
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::setBuffersDimensions");

    if (w<0 || h<0)
        return BAD_VALUE;

    if ((w && !h) || (!w && h))
        return BAD_VALUE;

    Mutex::Autolock lock(mMutex);
    mReqWidth = w;
    mReqHeight = h;
    return NO_ERROR;
}

int GonkNativeWindowClient::setBuffersUserDimensions(int w, int h)
{
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::setBuffersUserDimensions");

    if (w<0 || h<0)
        return BAD_VALUE;

    if ((w && !h) || (!w && h))
        return BAD_VALUE;

    Mutex::Autolock lock(mMutex);
    mUserWidth = w;
    mUserHeight = h;
    return NO_ERROR;
}

int GonkNativeWindowClient::setBuffersFormat(int format)
{
    ALOGV("GonkNativeWindowClient::setBuffersFormat");

    if (format<0)
        return BAD_VALUE;

    Mutex::Autolock lock(mMutex);
    mReqFormat = format;
    return NO_ERROR;
}

int GonkNativeWindowClient::setScalingMode(int mode)
{
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::setScalingMode(%d)", mode);

    switch (mode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
            break;
        default:
            ALOGE("unknown scaling mode: %d", mode);
            return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);
    mScalingMode = mode;
    return NO_ERROR;
}

int GonkNativeWindowClient::setBuffersTransform(int transform)
{
    ATRACE_CALL();
    ALOGV("GonkNativeWindowClient::setBuffersTransform");
    Mutex::Autolock lock(mMutex);
    mTransform = transform;
    return NO_ERROR;
}

int GonkNativeWindowClient::setBuffersTimestamp(int64_t timestamp)
{
    ALOGV("GonkNativeWindowClient::setBuffersTimestamp");
    Mutex::Autolock lock(mMutex);
    mTimestamp = timestamp;
    return NO_ERROR;
}

void GonkNativeWindowClient::freeAllBuffers() {
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        mSlots[i].buffer = 0;
    }
}

// ----------------------------------------------------------------------
// the lock/unlock APIs must be used from the same thread

// ----------------------------------------------------------------------------

status_t GonkNativeWindowClient::lock(
        ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds)
{
    return INVALID_OPERATION;
}

status_t GonkNativeWindowClient::unlockAndPost()
{
    return INVALID_OPERATION;
}

}; // namespace android
