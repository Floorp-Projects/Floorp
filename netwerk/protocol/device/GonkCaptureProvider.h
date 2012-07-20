/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GonkDeviceCaptureProvider_h_
#define GonkDeviceCaptureProvider_h_

#include "nsDeviceCaptureProvider.h"
#include "nsIAsyncInputStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsIEventTarget.h"
#include "nsDeque.h"
#include "mozilla/ReentrantMonitor.h"

#include "binder/IMemory.h"

using namespace android;

class CameraHardwareInterface;

class GonkCaptureProvider : public nsDeviceCaptureProvider {
  public:
    GonkCaptureProvider();
    ~GonkCaptureProvider();

    NS_DECL_ISUPPORTS

    nsresult Init(nsACString& aContentType, nsCaptureParams* aParams, nsIInputStream** aStream);
    static GonkCaptureProvider* sInstance;
};

class GonkCameraInputStream : public nsIAsyncInputStream {
  public:
    GonkCameraInputStream();
    ~GonkCameraInputStream();

    NS_IMETHODIMP Init(nsACString& aContentType, nsCaptureParams* aParams);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM

    void ReceiveFrame(char* frame, PRUint32 length);

    static void  DataCallback(int32_t aMsgType, const sp<IMemory>& aDataPtr, void *aUser);
    static PRUint32 getNumberOfCameras();

  protected:
    void NotifyListeners();
    void doClose();

  private:
    PRUint32 mAvailable;
    nsCString mContentType;
    PRUint32 mWidth;
    PRUint32 mHeight;
    PRUint32 mFps;
    PRUint32 mCamera;
    bool mHeaderSent;
    bool mClosed;
    bool mIs420p;
    nsDeque mFrameQueue;
    PRUint32 mFrameSize;
    mozilla::ReentrantMonitor mMonitor;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
    nsCOMPtr<nsIEventTarget> mCallbackTarget;
    CameraHardwareInterface* mHardware;
};

already_AddRefed<GonkCaptureProvider> GetGonkCaptureProvider();

#endif
