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

#include "base/basictypes.h"
#include "AndroidCaptureProvider.h"
#include "nsXULAppAPI.h"
#include "AndroidBridge.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsMemory.h"
#include "nsRawStructs.h"

// The maximum number of frames we keep in our queue. Don't live in the past.
#define MAX_FRAMES_QUEUED 10

using namespace mozilla::net;

NS_IMPL_THREADSAFE_ISUPPORTS2(AndroidCameraInputStream, nsIInputStream, nsIAsyncInputStream)

AndroidCameraInputStream::AndroidCameraInputStream() :
  mWidth(0), mHeight(0), mCamera(0), mHeaderSent(false), mClosed(true), mFrameSize(0),
  mMonitor("AndroidCamera.Monitor")
{
  mAvailable = sizeof(nsRawVideoHeader);
  mFrameQueue = new nsDeque();
}

AndroidCameraInputStream::~AndroidCameraInputStream() {
  // clear the frame queue
  while (mFrameQueue->GetSize() > 0) {
    nsMemory::Free(mFrameQueue->PopFront());
  }
  delete mFrameQueue;
}

NS_IMETHODIMP
AndroidCameraInputStream::Init(nsACString& aContentType, nsCaptureParams* aParams)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default)
    return NS_ERROR_NOT_IMPLEMENTED;

  mContentType = aContentType;
  mWidth = aParams->width;
  mHeight = aParams->height;
  mCamera = aParams->camera;
  
  CameraStreamImpl *impl = CameraStreamImpl::GetInstance(0);
  if (!impl)
    return NS_ERROR_OUT_OF_MEMORY;
  if (impl->Init(mContentType, mCamera, mWidth, mHeight, this)) {
    mWidth = impl->GetWidth();
    mHeight = impl->GetHeight();
    mClosed = false;
  }
  return NS_OK;
}

void AndroidCameraInputStream::ReceiveFrame(char* frame, PRUint32 length) {
  {
    mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);
    if (mFrameQueue->GetSize() > MAX_FRAMES_QUEUED) {
      nsMemory::Free(mFrameQueue->PopFront());
      mAvailable -= mFrameSize;
    }
  }
  
  mFrameSize = sizeof(nsRawPacketHeader) + length;
  
  char* fullFrame = (char*)nsMemory::Alloc(mFrameSize);

  if (!fullFrame)
    return;
  
  nsRawPacketHeader* header = (nsRawPacketHeader*) fullFrame;
  header->packetID = 0xFF;
  header->codecID = 0x595556; // "YUV"
  
  // we copy the Y plane, and de-interlace the CrCb
  
  PRUint32 yFrameSize = mWidth * mHeight;
  PRUint32 uvFrameSize = yFrameSize / 4;

  memcpy(fullFrame + sizeof(nsRawPacketHeader), frame, yFrameSize);
  
  char* uFrame = fullFrame + yFrameSize;
  char* vFrame = fullFrame + yFrameSize + uvFrameSize;
  char* yFrame = frame + yFrameSize;
  for (PRUint32 i = 0; i < uvFrameSize; i++) {
    uFrame[i] = yFrame[2 * i + 1];
    vFrame[i] = yFrame[2 * i];
  }
  
  {
    mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);
    mAvailable += mFrameSize;
    mFrameQueue->Push((void*)fullFrame);
  }

  NotifyListeners();
}

NS_IMETHODIMP
AndroidCameraInputStream::Available(PRUint32 *aAvailable)
{
  mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);

  *aAvailable = mAvailable;

  return NS_OK;
}

NS_IMETHODIMP AndroidCameraInputStream::IsNonBlocking(bool *aNonBlock) {
  *aNonBlock = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP AndroidCameraInputStream::Read(char *aBuffer, PRUint32 aCount, PRUint32 *aRead NS_OUTPARAM) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aRead);
}

NS_IMETHODIMP AndroidCameraInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, PRUint32 aCount, PRUint32 *aRead NS_OUTPARAM) {
  *aRead = 0;
  
  nsresult rv;

  if (mAvailable == 0)
    return NS_BASE_STREAM_WOULD_BLOCK;
  
  if (aCount > mAvailable)
    aCount = mAvailable;

  if (!mHeaderSent) {
    CameraStreamImpl *impl = CameraStreamImpl::GetInstance(0);
    nsRawVideoHeader header;
    header.headerPacketID = 0;
    header.codecID = 0x595556; // "YUV"
    header.majorVersion = 0;
    header.minorVersion = 1;
    header.options = 1 | 1 << 1; // color, 4:2:2

    header.alphaChannelBpp = 0;
    header.lumaChannelBpp = 8;
    header.chromaChannelBpp = 4;
    header.colorspace = 1;

    header.frameWidth = mWidth;
    header.frameHeight = mHeight;
    header.aspectNumerator = 1;
    header.aspectDenominator = 1;

    header.framerateNumerator = impl->GetFps();
    header.framerateDenominator = 1;

    rv = aWriter(this, aClosure, (const char*)&header, 0, sizeof(nsRawVideoHeader), aRead);
   
    if (NS_FAILED(rv))
      return NS_OK;
    
    mHeaderSent = true;
    aCount -= sizeof(nsRawVideoHeader);
    mAvailable -= sizeof(nsRawVideoHeader);
  }
  
  {
    mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);
    while ((mAvailable > 0) && (aCount >= mFrameSize)) {
      PRUint32 readThisTime = 0;

      char* frame = (char*)mFrameQueue->PopFront();
      rv = aWriter(this, aClosure, (const char*)frame, *aRead, mFrameSize, &readThisTime);

      if (readThisTime != mFrameSize) {
        mFrameQueue->PushFront((void*)frame);
        return NS_OK;
      }
  
      // nsRawReader does a copy when calling VideoData::Create()
      nsMemory::Free(frame);
  
      if (NS_FAILED(rv))
        return NS_OK;

      aCount -= readThisTime;
      mAvailable -= readThisTime;
      *aRead += readThisTime;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP AndroidCameraInputStream::Close() {
  return CloseWithStatus(NS_OK);
}


/**
 * must be called on the main (java) thread
 */
void AndroidCameraInputStream::doClose() {
  NS_ASSERTION(!mClosed, "Camera is already closed");

  CameraStreamImpl *impl = CameraStreamImpl::GetInstance(0);
  impl->Close();
  mClosed = true;
}


void AndroidCameraInputStream::NotifyListeners() {
  mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);
  
  if (mCallback && (mAvailable > sizeof(nsRawVideoHeader))) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      NS_NewInputStreamReadyEvent(getter_AddRefs(callback), mCallback, mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback!");

    // Null the callback first because OnInputStreamReady may reenter AsyncWait
    mCallback = nsnull;
    mCallbackTarget = nsnull;

    callback->OnInputStreamReady(this);
  }
}

NS_IMETHODIMP AndroidCameraInputStream::AsyncWait(nsIInputStreamCallback *aCallback, PRUint32 aFlags, PRUint32 aRequestedCount, nsIEventTarget *aTarget)
{
  if (aFlags != 0)
    return NS_ERROR_NOT_IMPLEMENTED;

  if (mCallback || mCallbackTarget)
    return NS_ERROR_UNEXPECTED;

  mCallbackTarget = aTarget;
  mCallback = aCallback;

  // What we are being asked for may be present already
  NotifyListeners();
  return NS_OK;
}


NS_IMETHODIMP AndroidCameraInputStream::CloseWithStatus(PRUint32 status)
{
  AndroidCameraInputStream::doClose();
  return NS_OK;
}

/**
 * AndroidCaptureProvider implementation
 */

NS_IMPL_THREADSAFE_ISUPPORTS0(AndroidCaptureProvider)

AndroidCaptureProvider* AndroidCaptureProvider::sInstance = NULL;

AndroidCaptureProvider::AndroidCaptureProvider() {
}

AndroidCaptureProvider::~AndroidCaptureProvider() {
  AndroidCaptureProvider::sInstance = NULL;
}

nsresult AndroidCaptureProvider::Init(nsACString& aContentType,
                        nsCaptureParams* aParams,
                        nsIInputStream** aStream) {

  NS_ENSURE_ARG_POINTER(aParams);

  NS_ASSERTION(aParams->frameLimit == 0 || aParams->timeLimit == 0,
    "Cannot set both a frame limit and a time limit!");

  nsRefPtr<AndroidCameraInputStream> stream;

  if (aContentType.EqualsLiteral("video/x-raw-yuv")) {
    stream = new AndroidCameraInputStream();
    if (stream) {
      nsresult rv = stream->Init(aContentType, aParams);
      if (NS_FAILED(rv))
        return rv;
    }
    else
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
    NS_NOTREACHED("Should not have asked Android for this type!");
  }
  return CallQueryInterface(stream, aStream);
}

already_AddRefed<AndroidCaptureProvider> GetAndroidCaptureProvider() {
  if (!AndroidCaptureProvider::sInstance) {
    AndroidCaptureProvider::sInstance = new AndroidCaptureProvider();
  }
  AndroidCaptureProvider::sInstance->AddRef();
  return AndroidCaptureProvider::sInstance;
}
