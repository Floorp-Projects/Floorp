/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
