/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CameraStreamImpl.h"
#include "GeneratedJNINatives.h"
#include "nsCRTGlue.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Monitor.h"

using namespace mozilla;

namespace mozilla {
namespace net {

static CameraStreamImpl* mCamera0 = nullptr;
static CameraStreamImpl* mCamera1 = nullptr;

class CameraStreamImpl::Callback
    : public java::GeckoAppShell::CameraCallback::Natives<Callback>
{
public:
    static void OnFrameData(int32_t aCamera, jni::ByteArray::Param aData)
    {
        MOZ_ASSERT(NS_IsMainThread());

        CameraStreamImpl* impl = GetInstance(uint32_t(aCamera));
        if (impl) {
            impl->TransmitFrame(aData);
        }
    }
};

/**
 * CameraStreamImpl
 */

void CameraStreamImpl::TransmitFrame(jni::ByteArray::Param aData) {
    if (!mCallback) {
        return;
    }

    JNIEnv* const env = jni::GetGeckoThreadEnv();
    const size_t length = size_t(env->GetArrayLength(aData.Get()));

    if (!length) {
        return;
    }

    jbyte* const data = env->GetByteArrayElements(aData.Get(), nullptr);
    mCallback->ReceiveFrame(reinterpret_cast<char*>(data), length);
    env->ReleaseByteArrayElements(aData.Get(), data, JNI_ABORT);
}

CameraStreamImpl* CameraStreamImpl::GetInstance(uint32_t aCamera) {
    CameraStreamImpl* res = nullptr;
    switch(aCamera) {
        case 0:
            if (mCamera0)
                res = mCamera0;
            else
                res = mCamera0 = new CameraStreamImpl(aCamera);
            break;
        case 1:
            if (mCamera1)
                res = mCamera1;
            else
                res = mCamera1 = new CameraStreamImpl(aCamera);
            break;
    }
    return res;
}


CameraStreamImpl::CameraStreamImpl(uint32_t aCamera) :
 mInit(false), mCamera(aCamera)
{
    NS_WARNING("CameraStreamImpl::CameraStreamImpl()");
    mWidth = 0;
    mHeight = 0;
    mFps = 0;
}

CameraStreamImpl::~CameraStreamImpl()
{
    NS_WARNING("CameraStreamImpl::~CameraStreamImpl()");
}

bool CameraStreamImpl::Init(const nsCString& contentType, const uint32_t& camera, const uint32_t& width, const uint32_t& height, FrameCallback* aCallback)
{
    mCallback = aCallback;
    mWidth = width;
    mHeight = height;

    Callback::Init();
    jni::IntArray::LocalRef retArray = java::GeckoAppShell::InitCamera(
            contentType, int32_t(camera), int32_t(width), int32_t(height));
    nsTArray<int32_t> ret = retArray->GetElements();

    mWidth = uint32_t(ret[1]);
    mHeight = uint32_t(ret[2]);
    mFps = uint32_t(ret[3]);

    return !!ret[0];
}

void CameraStreamImpl::Close() {
    java::GeckoAppShell::CloseCamera();
    mCallback = nullptr;
}

} // namespace net
} // namespace mozilla
