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
