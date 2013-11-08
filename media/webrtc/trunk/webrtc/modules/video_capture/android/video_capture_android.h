/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_VIDEO_CAPTURE_ANDROID_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_VIDEO_CAPTURE_ANDROID_H_

#include <jni.h>
#include <assert.h>
#include "trace.h"

#include "webrtc/modules/video_capture/android/device_info_android.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"

#define AndroidJavaCaptureClass "org/webrtc/videoengine/VideoCaptureAndroid"

namespace webrtc {
namespace videocapturemodule {

class VideoCaptureAndroid : public VideoCaptureImpl {
 public:
  static int32_t SetAndroidObjects(void* javaVM, void* javaContext);
  VideoCaptureAndroid(const int32_t id);
  virtual int32_t Init(const int32_t id, const char* deviceUniqueIdUTF8);

  virtual int32_t StartCapture(
      const VideoCaptureCapability& capability);
  virtual int32_t StopCapture();
  virtual bool CaptureStarted();
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings);
  virtual int32_t SetCaptureRotation(VideoCaptureRotation rotation);

  friend class AutoLocalJNIFrame;

 protected:
  virtual ~VideoCaptureAndroid();
  static void JNICALL ProvideCameraFrame (JNIEnv * env,
                                          jobject,
                                          jbyteArray javaCameraFrame,
                                          jint length,
                                          jint rotation,
                                          jlong context);
  DeviceInfoAndroid _capInfo;
  jobject _javaCaptureObj; // Java Camera object.
  VideoCaptureCapability _frameInfo;
  bool _captureStarted;

  static JavaVM* g_jvm;
  static jclass g_javaCmClass;
  static jclass g_javaCmDevInfoClass;
  //Static java object implementing the needed device info functions;
  static jobject g_javaCmDevInfoObject;
};

// Reworked version of what is available in AndroidBridge,
// can attach/deatch in addition to push/pop frames.
class AutoLocalJNIFrame {
public:
 AutoLocalJNIFrame(int nEntries = 128)
     : mEntries(nEntries), mHasFrameBeenPushed(false), mAttached(false)
    {
        mJNIEnv = InitJNIEnv();
        Push();
    }

    JNIEnv* GetEnv() {
        return mJNIEnv;
    }

    jclass GetCmDevInfoClass() {
        assert(VideoCaptureAndroid::g_javaCmDevInfoClass != nullptr);
        return VideoCaptureAndroid::g_javaCmDevInfoClass;
    }

    jobject GetCmDevInfoObject() {
        assert(VideoCaptureAndroid::g_javaCmDevInfoObject != nullptr);
        return VideoCaptureAndroid::g_javaCmDevInfoObject;
    }

    bool CheckForException() {
        if (mJNIEnv->ExceptionCheck()) {
            mJNIEnv->ExceptionDescribe();
            mJNIEnv->ExceptionClear();
            return true;
        }

        return false;
    }

    ~AutoLocalJNIFrame() {
        if (!mJNIEnv)
            return;

        CheckForException();

        if (mHasFrameBeenPushed)
            mJNIEnv->PopLocalFrame(NULL);

        if (mAttached) {
            int res = VideoCaptureAndroid::g_jvm->DetachCurrentThread();
            if (res < 0) {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                         "%s: JVM Detach failed.", __FUNCTION__);
            }
        }
    }

private:
    void Push() {
        if (!mJNIEnv)
            return;

        // Make sure there is enough space to store a local ref to the
        // exception.  I am not completely sure this is needed, but does
        // not hurt.
        jint ret = mJNIEnv->PushLocalFrame(mEntries + 1);
        assert(ret == 0);
        if (ret < 0)
            CheckForException();
        else
            mHasFrameBeenPushed = true;
    }

    JNIEnv* InitJNIEnv()
    {
        JNIEnv* env = nullptr;

        // Get the JNI env for this thread.
        if (!VideoCaptureAndroid::g_jvm) {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                         "%s: SetAndroidObjects not called with a valid JVM.",
                         __FUNCTION__);
            return nullptr;
        }

        jint res = VideoCaptureAndroid::g_jvm->GetEnv((void**) &env, JNI_VERSION_1_4);
        if (res == JNI_EDETACHED) {
            // Try to attach this thread to the JVM and get the env.
            res = VideoCaptureAndroid::g_jvm->AttachCurrentThread(&env, NULL);
            if ((res < 0) || !env) {
                // Attaching failed, error out.
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                             "%s: Could not attach thread to JVM (%d, %p)",
                             __FUNCTION__, res, env);
                return nullptr;
            }
            mAttached = true;
            WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideoCapture, -1,
                         "%s: attach success", __FUNCTION__);
        } else if (res == JNI_OK) {
            // Already attached, GetEnv succeeded.
            WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideoCapture, -1,
                         "%s: did not attach because JVM Env already present",
                         __FUNCTION__);
        } else {
            // Non-recoverable error in GetEnv.
            return nullptr;
        }

        return env;
    }

    int mEntries;
    JNIEnv* mJNIEnv;
    bool mHasFrameBeenPushed;
    bool mAttached;
};

}  // namespace videocapturemodule
}  // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_VIDEO_CAPTURE_ANDROID_H_
