/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CAMERASTREAMIMPL_H__
#define __CAMERASTREAMIMPL_H__

#include "mozilla/jni/Refs.h"

#include "nsString.h"

/**
 * This singleton class handles communication with the Android camera
 * through JNI. It is used by the IPDL parent or directly from the chrome process
 */

namespace mozilla {
namespace net {

class CameraStreamImpl {
public:
    class FrameCallback {
    public:
        virtual void ReceiveFrame(char* frame, uint32_t length) = 0;
    };

    /**
     * instance bound to a given camera
     */
    static CameraStreamImpl* GetInstance(uint32_t aCamera);

    bool initNeeded() {
        return !mInit;
    }

    FrameCallback* GetFrameCallback() {
        return mCallback;
    }

    bool Init(const nsCString& contentType, const uint32_t& camera, const uint32_t& width, const uint32_t& height, FrameCallback* callback);
    void Close();

    uint32_t GetWidth() { return mWidth; }
    uint32_t GetHeight() { return mHeight; }
    uint32_t GetFps() { return mFps; }

    void takePicture(const nsAString& aFileName);

private:
    class Callback;

    CameraStreamImpl(uint32_t aCamera);
    CameraStreamImpl(const CameraStreamImpl&);
    CameraStreamImpl& operator=(const CameraStreamImpl&);

    ~CameraStreamImpl();

    void TransmitFrame(jni::ByteArray::Param aData);

    bool mInit;
    uint32_t mCamera;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mFps;
    FrameCallback* mCallback;
};

} // namespace net
} // namespace mozilla

#endif
