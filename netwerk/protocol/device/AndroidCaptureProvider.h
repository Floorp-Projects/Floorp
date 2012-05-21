/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDeviceCaptureProvide_h_
#define AndroidDeviceCaptureProvide_h_

#include "nsDeviceCaptureProvider.h"
#include "nsIAsyncInputStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "mozilla/net/CameraStreamImpl.h"
#include "nsIEventTarget.h"
#include "nsDeque.h"
#include "mozilla/ReentrantMonitor.h"

class AndroidCaptureProvider : public nsDeviceCaptureProvider {
  public:
    AndroidCaptureProvider();
    ~AndroidCaptureProvider();

    NS_DECL_ISUPPORTS

    nsresult Init(nsACString& aContentType, nsCaptureParams* aParams, nsIInputStream** aStream);
    static AndroidCaptureProvider* sInstance;
};

class AndroidCameraInputStream : public nsIAsyncInputStream, mozilla::net::CameraStreamImpl::FrameCallback {
  public:
    AndroidCameraInputStream();
    ~AndroidCameraInputStream();

    NS_IMETHODIMP Init(nsACString& aContentType, nsCaptureParams* aParams);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM

    void ReceiveFrame(char* frame, PRUint32 length);

  protected:
    void NotifyListeners();
    void doClose();

    PRUint32 mAvailable;
    nsCString mContentType;
    PRUint32 mWidth;
    PRUint32 mHeight;
    PRUint32 mCamera;
    bool mHeaderSent;
    bool mClosed;
    nsDeque *mFrameQueue;
    PRUint32 mFrameSize;
    mozilla::ReentrantMonitor mMonitor;

    nsCOMPtr<nsIInputStreamCallback> mCallback;
    nsCOMPtr<nsIEventTarget> mCallbackTarget;
};

already_AddRefed<AndroidCaptureProvider> GetAndroidCaptureProvider();

#endif
