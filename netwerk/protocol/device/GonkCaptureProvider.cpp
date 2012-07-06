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

#include <dlfcn.h>
#include "android/log.h"
#include "cutils/properties.h"

#include "base/basictypes.h"
#include "GonkCaptureProvider.h"
#include "nsXULAppAPI.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsRawStructs.h"
#include "prinit.h"

#define USE_GS2_LIBCAMERA
#define CameraHardwareInterface CameraHardwareInterface_SGS2
#define HAL_openCameraHardware HAL_openCameraHardware_SGS2
#include "gonk/CameraHardwareInterface.h"
#undef CameraHardwareInterface
#undef USE_GS2_LIBCAMERA
#undef HAL_openCameraHardware
#undef ANDROID_HARDWARE_CAMERA_HARDWARE_INTERFACE_H

// Needed to prevent a redefinition of 'struct android::image_rect_struct'...
#define image_rect_type image_rect_type2
#define image_rect_struct image_rect_struct2

#define USE_MAGURO_LIBCAMERA
#define CameraHardwareInterface CameraHardwareInterface_MAGURO
#define HAL_openCameraHardware HAL_openCameraHardware_MAGURO
#include "gonk/CameraHardwareInterface.h"
#undef CameraHardwareInterface
#undef USE_MAGURO_LIBCAMERA
#undef HAL_openCameraHardware
#undef ANDROID_HARDWARE_CAMERA_HARDWARE_INTERFACE_H
#undef image_rect_type
#undef image_rect_struct

#define image_rect_type image_rect_type3
#define image_rect_struct image_rect_struct3
#define CameraHardwareInterface CameraHardwareInterface_DEFAULT
#include "gonk/CameraHardwareInterface.h"
#undef CameraHardwareInterface

using namespace android;
using namespace mozilla;

class CameraHardwareInterface {
  public:
    enum Type {
      CAMERA_SGS2,
      CAMERA_MAGURO,
      CAMERA_DEFAULT
    };

    static Type getType() {
      char propValue[PROPERTY_VALUE_MAX];
      property_get("ro.product.board", propValue, NULL);
      if (!strcmp(propValue, "GT-I9100"))
        return CAMERA_SGS2;

      if (!strcmp(propValue, "msm7627a_sku1") || !strcmp(propValue, "MSM7627A_SKU3"))
        return CAMERA_MAGURO;

      printf_stderr("CameraHardwareInterface : unsupported camera for device %s\n", propValue);
      return CAMERA_DEFAULT;
    }

    static CameraHardwareInterface* openCamera(PRUint32 aCamera);
    
    virtual ~CameraHardwareInterface() { }
    
    virtual bool ok() = 0;
    virtual void enableMsgType(int32_t msgType) = 0;
    virtual void disableMsgType(int32_t msgType) = 0;
    virtual bool msgTypeEnabled(int32_t msgType) = 0;
    virtual void setCallbacks(notify_callback notify_cb,
                              data_callback data_cb,
                              data_callback_timestamp data_cb_timestamp,
                              void* user) = 0;
    virtual status_t startPreview() = 0;
    virtual void stopPreview() = 0;
    virtual void release() = 0;
    virtual status_t setParameters(const CameraParameters& params) = 0;
    virtual CameraParameters getParameters() const = 0;

  protected:
    CameraHardwareInterface(PRUint32 aCamera = 0) { };
};

// Intentionally not trying to dlclose() this handle.  That's playing
// Russian roulette with security bugs.
static void* sCameraLib;
static PRCallOnceType sInitCameraLib;

static PRStatus
InitCameraLib()
{
  sCameraLib = dlopen("/system/lib/libcamera.so", RTLD_LAZY);
  // We might fail to open the camera lib.  That's OK.
  return PR_SUCCESS;
}

static void*
GetCameraLibHandle()
{
  PR_CallOnce(&sInitCameraLib, InitCameraLib);
  return sCameraLib;
}

template<class T> class CameraImpl : public CameraHardwareInterface {
  public:
    typedef sp<T> (*HAL_openCameraHardware_DEFAULT)(int);
    typedef sp<T> (*HAL_openCameraHardware_SGS2)(int);
    typedef sp<T> (*HAL_openCameraHardware_MAGURO)(int, int);

    CameraImpl(PRUint32 aCamera = 0) : mOk(false), mCamera(nsnull) {
      void* cameraLib = GetCameraLibHandle();
      if (!cameraLib) {
        printf_stderr("CameraImpl: Failed to dlopen() camera library.");
        return;
      }

      void *hal = dlsym(cameraLib, "HAL_openCameraHardware");
      HAL_openCameraHardware_DEFAULT funct0;
      HAL_openCameraHardware_SGS2 funct1;
      HAL_openCameraHardware_MAGURO funct2;
      switch(getType()) {
        case CAMERA_SGS2:
          funct1 = reinterpret_cast<HAL_openCameraHardware_SGS2> (hal);       
          mCamera = funct1(aCamera);
          break;
        case CAMERA_MAGURO:
          funct2 = reinterpret_cast<HAL_openCameraHardware_MAGURO> (hal);  
          mCamera = funct2(aCamera, 1);
          break;
        case CAMERA_DEFAULT:
          funct0 = reinterpret_cast<HAL_openCameraHardware_DEFAULT> (hal);  
          mCamera = funct0(aCamera);
          break;
      }

      mOk = mCamera != nsnull;
      if (!mOk) {
        printf_stderr("CameraImpl: HAL_openCameraHardware() returned NULL (no camera interface).");
      }
    }

    bool ok() {
      return mOk;
    };

    void enableMsgType(int32_t msgType) {
      mCamera->enableMsgType(msgType);
    };

    void disableMsgType(int32_t msgType) {
      mCamera->disableMsgType(msgType);
    };

    bool msgTypeEnabled(int32_t msgType) {
      return mCamera->msgTypeEnabled(msgType); 
    };

    void setCallbacks(notify_callback notify_cb,
                              data_callback data_cb,
                              data_callback_timestamp data_cb_timestamp,
                              void* user) {
      mCamera->setCallbacks(notify_cb, data_cb, data_cb_timestamp, user);                          
    };

    status_t startPreview() {
      return mCamera->startPreview();
    };

    void stopPreview() {
      mCamera->stopPreview();
    };

    void release() {
      return mCamera->release();
    };

    status_t setParameters(const CameraParameters& params) {
      return mCamera->setParameters(params);
    }

    CameraParameters getParameters() const {
      return mCamera->getParameters();
    };
  protected:
    bool mOk;
    sp<T> mCamera;  
};

CameraHardwareInterface* CameraHardwareInterface::openCamera(PRUint32 aCamera)  {
  nsAutoPtr<CameraHardwareInterface> instance;
  switch(getType()) {
    case CAMERA_SGS2:
      instance = new CameraImpl<CameraHardwareInterface_SGS2>(aCamera);
      break;
    case CAMERA_MAGURO:
      instance = new CameraImpl<CameraHardwareInterface_MAGURO>(aCamera);
      break;
    case CAMERA_DEFAULT:
      instance = new CameraImpl<CameraHardwareInterface_DEFAULT>(aCamera);
      break;
  }

  if (!instance->ok()) {
    return nsnull;
  }

  return instance.forget();
};


// The maximum number of frames we keep in our queue. Don't live in the past.
#define MAX_FRAMES_QUEUED 5

NS_IMPL_THREADSAFE_ISUPPORTS2(GonkCameraInputStream, nsIInputStream, nsIAsyncInputStream)

GonkCameraInputStream::GonkCameraInputStream() :
  mAvailable(sizeof(nsRawVideoHeader)), mWidth(0), mHeight(0), mFps(30), mCamera(0), 
  mHeaderSent(false), mClosed(true), mIs420p(false), mFrameSize(0), mMonitor("GonkCamera.Monitor")
{

}

GonkCameraInputStream::~GonkCameraInputStream() {
  // clear the frame queue
  while (mFrameQueue.GetSize() > 0) {
    free(mFrameQueue.PopFront());
  }

  // no need to close Close() since the stream is opened here :
  // http://mxr.mozilla.org/mozilla-central/source/netwerk/base/src/nsBaseChannel.cpp#239
  // and this leads to http://mxr.mozilla.org/mozilla-central/source/netwerk/base/src/nsBaseChannel.cpp#259
  // that creates and input pump with closeWhenDone == true
  // the pump cleans up properly at http://mxr.mozilla.org/mozilla-central/source/netwerk/base/src/nsInputStreamPump.cpp#565
}

void
GonkCameraInputStream::DataCallback(int32_t aMsgType, const sp<IMemory>& aDataPtr, void *aUser) {
  GonkCameraInputStream* stream = (GonkCameraInputStream*)(aUser);
  stream->ReceiveFrame((char*)aDataPtr->pointer(), aDataPtr->size());
}

PRUint32
GonkCameraInputStream::getNumberOfCameras() {
  typedef int (*HAL_getNumberOfCamerasFunct)(void);
  void* cameraLib = GetCameraLibHandle();
  if (!cameraLib)
    return 0;
  
  void *hal = dlsym(cameraLib, "HAL_getNumberOfCameras");
  if (nsnull == hal)
    return 0;

  HAL_getNumberOfCamerasFunct funct = reinterpret_cast<HAL_getNumberOfCamerasFunct> (hal);       
  return funct();
}

NS_IMETHODIMP
GonkCameraInputStream::Init(nsACString& aContentType, nsCaptureParams* aParams)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default)
    return NS_ERROR_NOT_IMPLEMENTED;

  mContentType = aContentType;
  mWidth = aParams->width;
  mHeight = aParams->height;
  mCamera = aParams->camera;

  PRUint32 maxNumCameras = getNumberOfCameras();

  if (maxNumCameras == 0)
    return NS_ERROR_FAILURE;

  if (mCamera >= maxNumCameras)
    mCamera = 0;

  mHardware = CameraHardwareInterface::openCamera(mCamera);

  if (!mHardware)
    return NS_ERROR_FAILURE;

  mHardware->setCallbacks(NULL, GonkCameraInputStream::DataCallback, NULL, this);

  mHardware->enableMsgType(CAMERA_MSG_PREVIEW_FRAME);

  CameraParameters params = mHardware->getParameters();

  printf_stderr("Preview format : %s\n", params.get(params.KEY_SUPPORTED_PREVIEW_FORMATS));

  Vector<Size> previewSizes;
  params.getSupportedPreviewSizes(previewSizes);

  // find the available preview size closest to the requested size.
  PRUint32 minSizeDelta = PR_UINT32_MAX;
  PRUint32 bestWidth = mWidth;
  PRUint32 bestHeight = mHeight;
  for (PRUint32 i = 0; i < previewSizes.size(); i++) {
    Size size = previewSizes[i];
    PRUint32 delta = abs(size.width * size.height - mWidth * mHeight); 
    if (delta < minSizeDelta) {
      minSizeDelta = delta;
      bestWidth = size.width;
      bestHeight = size.height;
    }
  }
  mWidth = bestWidth;
  mHeight = bestHeight;
  params.setPreviewSize(mWidth, mHeight);

  // try to set preferred image format
  params.setPreviewFormat("yuv420p");

  params.setPreviewFrameRate(mFps);
  mHardware->setParameters(params);
  params = mHardware->getParameters();
  mFps = params.getPreviewFrameRate();

  mIs420p = !strcmp(params.getPreviewFormat(), "yuv420p");

  mHardware->startPreview();

  mClosed = false;
  return NS_OK;
}


void 
GonkCameraInputStream::ReceiveFrame(char* frame, PRUint32 length) {
  {
    ReentrantMonitorAutoEnter enter(mMonitor);
    if (mFrameQueue.GetSize() > MAX_FRAMES_QUEUED) {
      free(mFrameQueue.PopFront());
      mAvailable -= mFrameSize;
    }
  }

  mFrameSize = sizeof(nsRawPacketHeader) + length;

  char* fullFrame = (char*)moz_malloc(mFrameSize);

  if (!fullFrame)
    return;

  nsRawPacketHeader* header = reinterpret_cast<nsRawPacketHeader*> (fullFrame);
  header->packetID = 0xFF;
  header->codecID = RAW_ID;

  if (mIs420p) {
    memcpy(fullFrame + sizeof(nsRawPacketHeader), frame, length);
  } else {
    // we copy the Y plane, and de-interlace the CrCb
    PRUint32 yFrameSize = mWidth * mHeight;
    PRUint32 uvFrameSize = yFrameSize / 4;
    memcpy(fullFrame + sizeof(nsRawPacketHeader), frame, yFrameSize);

    char* uFrame = fullFrame + sizeof(nsRawPacketHeader) + yFrameSize;
    char* vFrame = fullFrame + sizeof(nsRawPacketHeader) + yFrameSize + uvFrameSize;
    const char* yFrame = frame + yFrameSize;
    for (PRUint32 i = 0; i < uvFrameSize; i++) {
      uFrame[i] = yFrame[2 * i + 1];
      vFrame[i] = yFrame[2 * i];
    }
  }
  

  {
    ReentrantMonitorAutoEnter enter(mMonitor);
    mAvailable += mFrameSize;
    mFrameQueue.Push((void*)fullFrame);
  }

  NotifyListeners();
}

NS_IMETHODIMP
GonkCameraInputStream::Available(PRUint32 *aAvailable)
{
  ReentrantMonitorAutoEnter enter(mMonitor);

  *aAvailable = mAvailable;

  return NS_OK;
}

NS_IMETHODIMP GonkCameraInputStream::IsNonBlocking(bool *aNonBlock) {
  *aNonBlock = true;
  return NS_OK;
}

NS_IMETHODIMP GonkCameraInputStream::Read(char *aBuffer, PRUint32 aCount, PRUint32 *aRead) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aRead);
}

NS_IMETHODIMP GonkCameraInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure, PRUint32 aCount, PRUint32 *aRead) {
  *aRead = 0;
  
  nsresult rv;

  if (mAvailable == 0)
    return NS_BASE_STREAM_WOULD_BLOCK;
  
  if (aCount > mAvailable)
    aCount = mAvailable;

  if (!mHeaderSent) {
    nsRawVideoHeader header;
    header.headerPacketID = 0;
    header.codecID = RAW_ID;
    header.majorVersion = 0;
    header.minorVersion = 1;
    header.options = 1 | 1 << 1; // color, 4:2:0

    header.alphaChannelBpp = 0;
    header.lumaChannelBpp = 8;
    header.chromaChannelBpp = 4;
    header.colorspace = 1;

    header.frameWidth = mWidth;
    header.frameHeight = mHeight;
    header.aspectNumerator = 1;
    header.aspectDenominator = 1;

    header.framerateNumerator = mFps;
    header.framerateDenominator = 1;

    rv = aWriter(this, aClosure, (const char*)&header, 0, sizeof(nsRawVideoHeader), aRead);
   
    if (NS_FAILED(rv))
      return NS_OK;
    
    mHeaderSent = true;
    aCount -= sizeof(nsRawVideoHeader);
    mAvailable -= sizeof(nsRawVideoHeader);
  }
  
  {
    ReentrantMonitorAutoEnter enter(mMonitor);
    while ((mAvailable > 0) && (aCount >= mFrameSize)) {
      PRUint32 readThisTime = 0;

      char* frame = (char*)mFrameQueue.PopFront();
      rv = aWriter(this, aClosure, (const char*)frame, *aRead, mFrameSize, &readThisTime);

      if (readThisTime != mFrameSize) {
        mFrameQueue.PushFront((void*)frame);
        return NS_OK;
      }

      // nsRawReader does a copy when calling VideoData::Create()
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

NS_IMETHODIMP GonkCameraInputStream::Close() {
  return CloseWithStatus(NS_OK);
}

void GonkCameraInputStream::doClose() {
  ReentrantMonitorAutoEnter enter(mMonitor);

  if (mClosed)
    return;

  mHardware->disableMsgType(CAMERA_MSG_ALL_MSGS);
  mHardware->stopPreview();
  mHardware->release();
  delete mHardware;

  mClosed = true;
}


void GonkCameraInputStream::NotifyListeners() {
  ReentrantMonitorAutoEnter enter(mMonitor);
  
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

NS_IMETHODIMP GonkCameraInputStream::AsyncWait(nsIInputStreamCallback *aCallback, PRUint32 aFlags, PRUint32 aRequestedCount, nsIEventTarget *aTarget)
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


NS_IMETHODIMP GonkCameraInputStream::CloseWithStatus(PRUint32 status)
{
  GonkCameraInputStream::doClose();
  return NS_OK;
}

/**
 * GonkCaptureProvider implementation
 */
NS_IMPL_ISUPPORTS0(GonkCaptureProvider)

GonkCaptureProvider* GonkCaptureProvider::sInstance = NULL;

GonkCaptureProvider::GonkCaptureProvider() {
}

GonkCaptureProvider::~GonkCaptureProvider() {
  GonkCaptureProvider::sInstance = NULL;
}

nsresult GonkCaptureProvider::Init(nsACString& aContentType,
                        nsCaptureParams* aParams,
                        nsIInputStream** aStream) {

  NS_ENSURE_ARG_POINTER(aParams);

  NS_ASSERTION(aParams->frameLimit == 0 || aParams->timeLimit == 0,
    "Cannot set both a frame limit and a time limit!");

  nsRefPtr<GonkCameraInputStream> stream;

  if (aContentType.EqualsLiteral("video/x-raw-yuv")) {
    stream = new GonkCameraInputStream();
    if (stream) {
      nsresult rv = stream->Init(aContentType, aParams);
      if (NS_FAILED(rv))
        return rv;
    }
    else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    NS_NOTREACHED("Should not have asked Gonk for this type!");
  }
  return CallQueryInterface(stream, aStream);
}

already_AddRefed<GonkCaptureProvider> GetGonkCaptureProvider() {
  if (!GonkCaptureProvider::sInstance) {
    GonkCaptureProvider::sInstance = new GonkCaptureProvider();
  }
  GonkCaptureProvider::sInstance->AddRef();
  return GonkCaptureProvider::sInstance;
}
