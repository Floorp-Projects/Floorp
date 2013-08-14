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

#ifndef NATIVEWINDOW_GONKNATIVEWINDOWCLIENT_ICS_H
#define NATIVEWINDOW_GONKNATIVEWINDOWCLIENT_ICS_H

#include <ui/egl/android_natives.h>

#include "GonkNativeWindow.h"

namespace android {

class GonkNativeWindowClient : public EGLNativeBase<ANativeWindow, GonkNativeWindowClient, RefBase>
{
public:
    GonkNativeWindowClient(const sp<GonkNativeWindow>& window);
    ~GonkNativeWindowClient(); // this class cannot be overloaded

private:
    void init();

    // ANativeWindow hooks
    static int hook_cancelBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer);
    static int hook_dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer);
    static int hook_lockBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer);
    static int hook_perform(ANativeWindow* window, int operation, ...);
    static int hook_query(const ANativeWindow* window, int what, int* value);
    static int hook_queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer);
    static int hook_setSwapInterval(ANativeWindow* window, int interval);

    int dispatchConnect(va_list args);
    int dispatchDisconnect(va_list args);
    int dispatchSetBufferCount(va_list args);
    int dispatchSetBuffersGeometry(va_list args);
    int dispatchSetBuffersDimensions(va_list args);
    int dispatchSetBuffersFormat(va_list args);
    int dispatchSetBuffersTimestamp(va_list args);
    int dispatchSetUsage(va_list args);

protected:
    virtual int cancelBuffer(ANativeWindowBuffer* buffer);
    virtual int dequeueBuffer(ANativeWindowBuffer** buffer);
    virtual int lockBuffer(ANativeWindowBuffer* buffer);
    virtual int perform(int operation, va_list args);
    virtual int query(int what, int* value) const;
    virtual int queueBuffer(ANativeWindowBuffer* buffer);
    virtual int setSwapInterval(int interval);

    virtual int connect(int api);
    virtual int disconnect(int api);
    virtual int setBufferCount(int bufferCount);
    virtual int setBuffersDimensions(int w, int h);
    virtual int setBuffersFormat(int format);
    virtual int setBuffersTimestamp(int64_t timestamp);
    virtual int setUsage(uint32_t reqUsage);

    int getNumberOfArgsForOperation(int operation);

    enum { MIN_UNDEQUEUED_BUFFERS = GonkNativeWindow::MIN_UNDEQUEUED_BUFFERS };
    enum { NUM_BUFFER_SLOTS = GonkNativeWindow::NUM_BUFFER_SLOTS };
    enum { DEFAULT_FORMAT = PIXEL_FORMAT_RGBA_8888 };
    enum { NATIVE_WINDOW_SET_BUFFERS_SIZE = 0x10000000 };

private:
    void freeAllBuffers();
    int getSlotFromBufferLocked(android_native_buffer_t* buffer) const;

    sp<GonkNativeWindow> mNativeWindow;

    // mSlots stores the buffers that have been allocated for each buffer slot.
    // It is initialized to null pointers, and gets filled in with the result of
    // ISurfaceTexture::requestBuffer when the client dequeues a buffer from a
    // slot that has not yet been used. The buffer allocated to a slot will also
    // be replaced if the requested buffer usage or geometry differs from that
    // of the buffer allocated to a slot.
    sp<GraphicBuffer> mSlots[NUM_BUFFER_SLOTS];

    // mReqWidth is the buffer width that will be requested at the next dequeue
    // operation. It is initialized to 1.
    uint32_t mReqWidth;

    // mReqHeight is the buffer height that will be requested at the next deuque
    // operation. It is initialized to 1.
    uint32_t mReqHeight;

    // mReqFormat is the buffer pixel format that will be requested at the next
    // deuque operation. It is initialized to PIXEL_FORMAT_RGBA_8888.
    uint32_t mReqFormat;

    // mReqUsage is the set of buffer usage flags that will be requested
    // at the next deuque operation. It is initialized to 0.
    uint32_t mReqUsage;

    // mTimestamp is the timestamp that will be used for the next buffer queue
    // operation. It defaults to NATIVE_WINDOW_TIMESTAMP_AUTO, which means that
    // a timestamp is auto-generated when queueBuffer is called.
    int64_t mTimestamp;

    // mDefaultWidth is default width of the window, regardless of the
    // native_window_set_buffers_dimensions call
    uint32_t mDefaultWidth;

    // mDefaultHeight is default width of the window, regardless of the
    // native_window_set_buffers_dimensions call
    uint32_t mDefaultHeight;

    // mTransformHint is the transform probably applied to buffers of this
    // window. this is only a hint, actual transform may differ.
    uint32_t mTransformHint;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of GonkNativeWindow objects. It must be locked whenever the
    // member variables are accessed.
    mutable Mutex mMutex;

    bool mConnectedToCpu;
};


}; // namespace android

#endif // NATIVEWINDOW_GONKNATIVEWINDOWCLIENT_ICS_H
