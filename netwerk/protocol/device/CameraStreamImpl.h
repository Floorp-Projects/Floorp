/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CAMERASTREAMIMPL_H__
#define __CAMERASTREAMIMPL_H__

#include "nsString.h"
#include "AndroidBridge.h"

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
        virtual void ReceiveFrame(char* frame, PRUint32 length) = 0;
    };
    
    /**
     * instance bound to a given camera
     */
    static CameraStreamImpl* GetInstance(PRUint32 aCamera);
    
    bool initNeeded() {
        return !mInit;
    }
    
    FrameCallback* GetFrameCallback() {
        return mCallback;
    }
    
    bool Init(const nsCString& contentType, const PRUint32& camera, const PRUint32& width, const PRUint32& height, FrameCallback* callback);
    void Close();
    
    PRUint32 GetWidth() { return mWidth; }
    PRUint32 GetHeight() { return mHeight; }
    PRUint32 GetFps() { return mFps; }
    
    void takePicture(const nsAString& aFileName);
    
    void transmitFrame(JNIEnv *env, jbyteArray *data);
    
private:
    CameraStreamImpl(PRUint32 aCamera);
    CameraStreamImpl(const CameraStreamImpl&);
    CameraStreamImpl& operator=(const CameraStreamImpl&);

    ~CameraStreamImpl();

    bool mInit;
    PRUint32 mCamera;
    PRUint32 mWidth;
    PRUint32 mHeight;
    PRUint32 mFps;
    FrameCallback* mCallback;
};

} // namespace net
} // namespace mozilla

#endif
