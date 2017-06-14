/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "AndroidCaptureProvider.h"
#include "nsXULAppAPI.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsMemory.h"
#include "RawStructs.h"

// The maximum number of frames we keep in our queue. Don't live in the past.
#define MAX_FRAMES_QUEUED 10

using namespace mozilla::net;

NS_IMPL_ISUPPORTS(AndroidCameraInputStream, nsIInputStream, nsIAsyncInputStream)

AndroidCameraInputStream::AndroidCameraInputStream() :
  mWidth(0), mHeight(0), mCamera(0), mHeaderSent(false), mClosed(true), mFrameSize(0),
  mMonitor("AndroidCamera.Monitor")
{
  mAvailable = sizeof(RawVideoHeader);
  mFrameQueue = new nsDeque();
}

AndroidCameraInputStream::~AndroidCameraInputStream() {
  // clear the frame queue
  while (mFrameQueue->GetSize() > 0) {
    free(mFrameQueue->PopFront());
  }
  delete mFrameQueue;
}

NS_IMETHODIMP
AndroidCameraInputStream::Init(nsACString& aContentType, nsCaptureParams* aParams)
{
  if (!XRE_IsParentProcess())
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

void AndroidCameraInputStream::ReceiveFrame(char* frame, uint32_t length) {
  {
    mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);
    if (mFrameQueue->GetSize() > MAX_FRAMES_QUEUED) {
      free(mFrameQueue->PopFront());
      mAvailable -= mFrameSize;
    }
  }
  
  mFrameSize = sizeof(RawPacketHeader) + length;
  
  char* fullFrame = (char*)moz_xmalloc(mFrameSize);

  if (!fullFrame)
    return;
  
  RawPacketHeader* header = (RawPacketHeader*) fullFrame;
  header->packetID = 0xFF;
  header->codecID = 0x595556; // "YUV"
  
  // we copy the Y plane, and de-interlace the CrCb
  
  uint32_t yFrameSize = mWidth * mHeight;
  uint32_t uvFrameSize = yFrameSize / 4;

  memcpy(fullFrame + sizeof(RawPacketHeader), frame, yFrameSize);
  
  char* uFrame = fullFrame + yFrameSize;
  char* vFrame = fullFrame + yFrameSize + uvFrameSize;
  char* yFrame = frame + yFrameSize;
  for (uint32_t i = 0; i < uvFrameSize; i++) {
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
AndroidCameraInputStream::Available(uint64_t *aAvailable)
{
  mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);

  *aAvailable = mAvailable;

  return NS_OK;
}

NS_IMETHODIMP AndroidCameraInputStream::IsNonBlocking(bool *aNonBlock) {
  *aNonBlock = true;
  return NS_OK;
}

NS_IMETHODIMP AndroidCameraInputStream::Read(char *aBuffer, uint32_t aCount, uint32_t *aRead) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aRead);
}

NS_IMETHODIMP AndroidCameraInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, uint32_t aCount, uint32_t *aRead) {
  *aRead = 0;
  
  nsresult rv;

  if (mAvailable == 0)
    return NS_BASE_STREAM_WOULD_BLOCK;
  
  if (aCount > mAvailable)
    aCount = mAvailable;

  if (!mHeaderSent) {
    CameraStreamImpl *impl = CameraStreamImpl::GetInstance(0);
    RawVideoHeader header;
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

    rv = aWriter(this, aClosure, (const char*)&header, 0, sizeof(RawVideoHeader), aRead);
   
    if (NS_FAILED(rv))
      return NS_OK;
    
    mHeaderSent = true;
    aCount -= sizeof(RawVideoHeader);
    mAvailable -= sizeof(RawVideoHeader);
  }
  
  {
    mozilla::ReentrantMonitorAutoEnter autoMonitor(mMonitor);
    while ((mAvailable > 0) && (aCount >= mFrameSize)) {
      uint32_t readThisTime = 0;

      char* frame = (char*)mFrameQueue->PopFront();
      rv = aWriter(this, aClosure, (const char*)frame, *aRead, mFrameSize, &readThisTime);

      if (readThisTime != mFrameSize) {
        mFrameQueue->PushFront((void*)frame);
        return NS_OK;
      }
  
      // RawReader does a copy when calling VideoData::Create()
      free(frame);
  
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
  
  if (mCallback && (mAvailable > sizeof(RawVideoHeader))) {
    nsCOMPtr<nsIInputStreamCallback> callback;
    if (mCallbackTarget) {
      callback = NS_NewInputStreamReadyEvent(mCallback, mCallbackTarget);
    } else {
      callback = mCallback;
    }

    NS_ASSERTION(callback, "Shouldn't fail to make the callback!");

    // Null the callback first because OnInputStreamReady may reenter AsyncWait
    mCallback = nullptr;
    mCallbackTarget = nullptr;

    callback->OnInputStreamReady(this);
  }
}

NS_IMETHODIMP AndroidCameraInputStream::AsyncWait(nsIInputStreamCallback *aCallback, uint32_t aFlags, uint32_t aRequestedCount, nsIEventTarget *aTarget)
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


NS_IMETHODIMP AndroidCameraInputStream::CloseWithStatus(nsresult status)
{
  AndroidCameraInputStream::doClose();
  return NS_OK;
}

/**
 * AndroidCaptureProvider implementation
 */

NS_IMPL_ISUPPORTS0(AndroidCaptureProvider)

AndroidCaptureProvider* AndroidCaptureProvider::sInstance = nullptr;

AndroidCaptureProvider::AndroidCaptureProvider() {
}

AndroidCaptureProvider::~AndroidCaptureProvider() {
  AndroidCaptureProvider::sInstance = nullptr;
}

nsresult AndroidCaptureProvider::Init(nsACString& aContentType,
                        nsCaptureParams* aParams,
                        nsIInputStream** aStream) {

  NS_ENSURE_ARG_POINTER(aParams);

  NS_ASSERTION(aParams->frameLimit == 0 || aParams->timeLimit == 0,
    "Cannot set both a frame limit and a time limit!");

  RefPtr<AndroidCameraInputStream> stream;

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
  stream.forget(aStream);
  return NS_OK;
}

already_AddRefed<AndroidCaptureProvider> GetAndroidCaptureProvider() {
  if (!AndroidCaptureProvider::sInstance) {
    AndroidCaptureProvider::sInstance = new AndroidCaptureProvider();
  }
  RefPtr<AndroidCaptureProvider> ret = AndroidCaptureProvider::sInstance;
  return ret.forget();
}
