/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Camera.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Fabrice Desré <fabrice@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CameraStreamImpl.h"
#include "nsCRTGlue.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Monitor.h"

/**
 * JNI part & helper runnable
 */

extern "C" {
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_cameraCallbackBridge(JNIEnv *, jclass, jbyteArray data);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_cameraCallbackBridge(JNIEnv *env, jclass, jbyteArray data) {
    mozilla::net::CameraStreamImpl* impl = mozilla::net::CameraStreamImpl::GetInstance(0);
    
    impl->transmitFrame(env, &data);
}

using namespace mozilla;

namespace mozilla {
namespace net {

static CameraStreamImpl* mCamera0 = NULL;
static CameraStreamImpl* mCamera1 = NULL;

/**
 * CameraStreamImpl
 */

void CameraStreamImpl::transmitFrame(JNIEnv *env, jbyteArray *data) {
    jboolean isCopy;
    jbyte* jFrame = env->GetByteArrayElements(*data, &isCopy);
    PRUint32 length = env->GetArrayLength(*data);
    if (length > 0) {
        mCallback->ReceiveFrame((char*)jFrame, length);
    }
    env->ReleaseByteArrayElements(*data, jFrame, 0);
}

CameraStreamImpl* CameraStreamImpl::GetInstance(PRUint32 aCamera) {
    CameraStreamImpl* res = NULL;
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


CameraStreamImpl::CameraStreamImpl(PRUint32 aCamera) :
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

bool CameraStreamImpl::Init(const nsCString& contentType, const PRUint32& camera, const PRUint32& width, const PRUint32& height, FrameCallback* aCallback)
{
    mCallback = aCallback;
    mWidth = width;
    mHeight = height;
    return AndroidBridge::Bridge()->InitCamera(contentType, camera, &mWidth, &mHeight, &mFps);
}

void CameraStreamImpl::Close() {
    AndroidBridge::Bridge()->CloseCamera();
    mCallback = NULL;
}

} // namespace net
} // namespace mozilla
