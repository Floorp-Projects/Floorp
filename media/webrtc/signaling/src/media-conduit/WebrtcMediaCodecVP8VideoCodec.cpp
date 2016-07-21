/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <iostream>
#include <queue>

#include "CSFLog.h"
#include "nspr.h"

#include "MediaCodec.h"
#include "WebrtcMediaCodecVP8VideoCodec.h"
#include "AndroidJNIWrapper.h"
#include "mozilla/ArrayUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"
#include "runnable_utils.h"

#include "AudioConduit.h"
#include "VideoConduit.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert.h"
#include "libyuv/row.h"

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

using namespace mozilla;
using namespace mozilla::java::sdk;

static const int32_t DECODER_TIMEOUT = 10 * PR_USEC_PER_MSEC; // 10ms
static const char MEDIACODEC_VIDEO_MIME_VP8[] = "video/x-vnd.on2.vp8";

namespace mozilla {

static const char* logTag ="WebrtcMediaCodecVP8VideoCodec";

static MediaCodec::LocalRef CreateDecoder(const char* aMimeType)
{
  if (!aMimeType) {
    return nullptr;
  }

  MediaCodec::LocalRef codec;
  MediaCodec::CreateDecoderByType(aMimeType, &codec);
  return codec;
}

static MediaCodec::LocalRef CreateEncoder(const char* aMimeType)
{
  if (!aMimeType) {
    return nullptr;
  }

  MediaCodec::LocalRef codec;
  MediaCodec::CreateEncoderByType(aMimeType, &codec);
  return codec;
}

static void
ShutdownThread(nsCOMPtr<nsIThread>& aThread)
{
  aThread->Shutdown();
}

// Base runnable class to repeatly pull MediaCodec output buffers in seperate thread.
// How to use:
// - implementing DrainOutput() to get output. Remember to return false to tell
//   drain not to pop input queue.
// - call QueueInput() to schedule a run to drain output. The input, aFrame,
//   should contains corresponding info such as image size and timestamps for
//   DrainOutput() implementation to construct data needed by encoded/decoded
//   callbacks.
class MediaCodecOutputDrain : public Runnable
{
public:
  void Start() {
    MonitorAutoLock lock(mMonitor);
    if (mThread == nullptr) {
      NS_NewNamedThread("OutputDrain", getter_AddRefs(mThread));
    }
    mEnding = false;
    mThread->Dispatch(this, NS_DISPATCH_NORMAL);
  }

  void Stop() {
    MonitorAutoLock lock(mMonitor);
    mEnding = true;
    lock.NotifyAll(); // In case Run() is waiting.

    if (mThread != nullptr) {
      MonitorAutoUnlock unlock(mMonitor);
      NS_DispatchToMainThread(
        WrapRunnableNM<decltype(&ShutdownThread),
                       nsCOMPtr<nsIThread> >(&ShutdownThread, mThread));
      mThread = nullptr;
    }
  }

  void QueueInput(const EncodedFrame& aFrame)
  {
    MonitorAutoLock lock(mMonitor);

    MOZ_ASSERT(mThread);

    mInputFrames.push(aFrame);
    // Notify Run() about queued input and it can start working.
    lock.NotifyAll();
  }

  NS_IMETHODIMP Run() override
  {
    MOZ_ASSERT(mThread);

    MonitorAutoLock lock(mMonitor);
    while (true) {
      if (mInputFrames.empty()) {
        // Wait for new input.
        lock.Wait();
      }

      if (mEnding) {
        // Stop draining.
        break;
      }

      MOZ_ASSERT(!mInputFrames.empty());
      {
        // Release monitor while draining because it's blocking.
        MonitorAutoUnlock unlock(mMonitor);
        DrainOutput();
      }
    }

    return NS_OK;
  }

protected:
  MediaCodecOutputDrain()
    : mMonitor("MediaCodecOutputDrain monitor")
    , mEnding(false)
  {}

  // Drain output buffer for input frame queue mInputFrames.
  // mInputFrames contains info such as size and time of the input frames.
  // We have to give a queue to handle encoder frame skips - we can input 10
  // frames and get one back.  NOTE: any access of aInputFrames MUST be preceded
  // locking mMonitor!

  // Blocks waiting for decoded buffers, but for a limited period because
  // we need to check for shutdown.
  virtual bool DrainOutput() = 0;

protected:
  // This monitor protects all things below it, and is also used to
  // wait/notify queued input.
  Monitor mMonitor;
  std::queue<EncodedFrame> mInputFrames;

private:
  // also protected by mMonitor
  nsCOMPtr<nsIThread> mThread;
  bool mEnding;
};

class WebrtcAndroidMediaCodec {
public:
  WebrtcAndroidMediaCodec()
    : mEncoderCallback(nullptr)
    , mDecoderCallback(nullptr)
    , isStarted(false)
    , mEnding(false) {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  }

  nsresult Configure(uint32_t width,
                     uint32_t height,
                     const jobject aSurface,
                     uint32_t flags,
                     const char* mime,
                     bool encoder) {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);
    nsresult res = NS_OK;

    if (!mCoder) {
      mWidth = width;
      mHeight = height;

      MediaFormat::LocalRef format;

      res = MediaFormat::CreateVideoFormat(nsCString(mime),
                                     mWidth,
                                     mHeight,
                                     &format);

      if (NS_FAILED(res)) {
        CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, CreateVideoFormat failed err = %d", __FUNCTION__, (int)res);
        return NS_ERROR_FAILURE;
      }

      if (encoder) {
        mCoder = CreateEncoder(mime);

        if (NS_FAILED(res)) {
          CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, CreateEncoderByType failed err = %d", __FUNCTION__, (int)res);
          return NS_ERROR_FAILURE;
        }

        res = format->SetInteger(nsCString("bitrate"), 1000*300);
        res = format->SetInteger(nsCString("bitrate-mode"), 2);
        res = format->SetInteger(nsCString("color-format"), 21);
        res = format->SetInteger(nsCString("frame-rate"), 30);
        res = format->SetInteger(nsCString("i-frame-interval"), 100);

      } else {
        mCoder = CreateDecoder(mime);
        if (NS_FAILED(res)) {
          CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, CreateDecoderByType failed err = %d", __FUNCTION__, (int)res);
          return NS_ERROR_FAILURE;
        }
      }
      res = mCoder->Configure(format, nullptr, nullptr, flags);
      if (NS_FAILED(res)) {
        CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, err = %d", __FUNCTION__, (int)res);
      }
    }

    return res;
  }

  nsresult Start() {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);

    if (!mCoder) {
      return NS_ERROR_FAILURE;
    }

    mEnding = false;

    nsresult res;
    res = mCoder->Start();
    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, mCoder->start() return err = %d",
                  __FUNCTION__, (int)res);
      return res;
    }
    isStarted = true;
    return NS_OK;
  }

  nsresult Stop() {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);
    mEnding = true;

    if (mOutputDrain != nullptr) {
      mOutputDrain->Stop();
      mOutputDrain = nullptr;
    }

    mCoder->Stop();
    mCoder->Release();
    isStarted = false;
    return NS_OK;
  }

  void GenerateVideoFrame(
      size_t width, size_t height, uint32_t timeStamp,
      void* decoded,
      webrtc::I420VideoFrame* videoFrame, int color_format) {

    CSFLogDebug(logTag,  "%s ", __FUNCTION__);

    // TODO: eliminate extra pixel copy/color conversion
    size_t widthUV = (width + 1) / 2;
    if (videoFrame->CreateEmptyFrame(width, height, width, widthUV, widthUV)) {
      return;
    }

    uint8_t* src_nv12 = static_cast<uint8_t *>(decoded);
    int src_nv12_y_size = width * height;

    uint8_t* dstY = videoFrame->buffer(webrtc::kYPlane);
    uint8_t* dstU = videoFrame->buffer(webrtc::kUPlane);
    uint8_t* dstV = videoFrame->buffer(webrtc::kVPlane);

    libyuv::NV12ToI420(src_nv12, width,
                       src_nv12 + src_nv12_y_size, (width + 1) & ~1,
                       dstY, width,
                       dstU, (width + 1) / 2,
                       dstV,
                       (width + 1) / 2,
                       width, height);

    videoFrame->set_timestamp(timeStamp);
  }

  int32_t
  FeedMediaCodecInput(
      const webrtc::EncodedImage& inputImage,
      int64_t renderTimeMs) {

#ifdef WEBRTC_MEDIACODEC_DEBUG
    uint32_t time = PR_IntervalNow();
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);
#endif

    int inputIndex = DequeueInputBuffer(DECODER_TIMEOUT);
    if (inputIndex == -1) {
      CSFLogError(logTag,  "%s equeue input buffer failed", __FUNCTION__);
      return inputIndex;
    }

#ifdef WEBRTC_MEDIACODEC_DEBUG
    CSFLogDebug(logTag,  "%s dequeue input buffer took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
    time = PR_IntervalNow();
#endif

    size_t size = inputImage._length;

    JNIEnv* env = jsjni_GetJNIForThread();
    jobject buffer = env->GetObjectArrayElement(mInputBuffers, inputIndex);
    void* directBuffer = env->GetDirectBufferAddress(buffer);

    PodCopy((uint8_t*)directBuffer, inputImage._buffer, size);

    if (inputIndex >= 0) {
      CSFLogError(logTag,  "%s queue input buffer inputIndex = %d", __FUNCTION__, inputIndex);
      QueueInputBuffer(inputIndex, 0, size, renderTimeMs, 0);

      {
        if (mOutputDrain == nullptr) {
          mOutputDrain = new OutputDrain(this);
          mOutputDrain->Start();
        }
        EncodedFrame frame;
        frame.width_ = mWidth;
        frame.height_ = mHeight;
        frame.timeStamp_ = inputImage._timeStamp;
        frame.decode_timestamp_ = renderTimeMs;
        mOutputDrain->QueueInput(frame);
      }
      env->DeleteLocalRef(buffer);
    }

    return inputIndex;
  }

  nsresult
  DrainOutput(std::queue<EncodedFrame>& aInputFrames, Monitor& aMonitor) {
    MOZ_ASSERT(mCoder != nullptr);
    if (mCoder == nullptr) {
      return NS_ERROR_FAILURE;
    }

#ifdef WEBRTC_MEDIACODEC_DEBUG
    uint32_t time = PR_IntervalNow();
#endif
    nsresult res;
    BufferInfo::LocalRef bufferInfo;
    res = BufferInfo::New(&bufferInfo);
    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, BufferInfo::New return err = %d",
                  __FUNCTION__, (int)res);
      return res;
    }
    int32_t outputIndex = DequeueOutputBuffer(bufferInfo);

    if (outputIndex == MediaCodec::INFO_TRY_AGAIN_LATER) {
      // Not an error: output not available yet. Try later.
      CSFLogDebug(logTag,  "%s dequeue output buffer try again:%d", __FUNCTION__, outputIndex);
    } else if (outputIndex == MediaCodec::INFO_OUTPUT_FORMAT_CHANGED) {
      // handle format change
      CSFLogDebug(logTag,  "%s dequeue output buffer format changed:%d", __FUNCTION__, outputIndex);
    } else if (outputIndex == MediaCodec::INFO_OUTPUT_BUFFERS_CHANGED) {
      CSFLogDebug(logTag,  "%s dequeue output buffer changed:%d", __FUNCTION__, outputIndex);
      GetOutputBuffers();
    } else if (outputIndex < 0) {
      CSFLogDebug(logTag,  "%s dequeue output buffer unknow error:%d", __FUNCTION__, outputIndex);
      MonitorAutoLock lock(aMonitor);
      aInputFrames.pop();
    } else {
#ifdef WEBRTC_MEDIACODEC_DEBUG
      CSFLogDebug(logTag,  "%s dequeue output buffer# return status is %d took %u ms", __FUNCTION__, outputIndex, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif
      EncodedFrame frame;
      {
        MonitorAutoLock lock(aMonitor);
        frame = aInputFrames.front();
        aInputFrames.pop();
      }

      if (mEnding) {
        ReleaseOutputBuffer(outputIndex, false);
        return NS_OK;
      }

      JNIEnv* env = jsjni_GetJNIForThread();
      jobject buffer = env->GetObjectArrayElement(mOutputBuffers, outputIndex);
      if (buffer) {
        // The buffer will be null on Android L if we are decoding to a Surface
        void* directBuffer = env->GetDirectBufferAddress(buffer);

        int color_format = 0;

        CSFLogDebug(logTag,  "%s generate video frame, width = %d, height = %d, timeStamp_ = %d", __FUNCTION__, frame.width_, frame.height_, frame.timeStamp_);
        GenerateVideoFrame(frame.width_, frame.height_, frame.timeStamp_, directBuffer, &mVideoFrame, color_format);
        mDecoderCallback->Decoded(mVideoFrame);

        ReleaseOutputBuffer(outputIndex, false);
        env->DeleteLocalRef(buffer);
      }
    }
    return NS_OK;
  }

  int32_t DequeueInputBuffer(int64_t time) {
    nsresult res;
    int32_t inputIndex;
    res = mCoder->DequeueInputBuffer(time, &inputIndex);

    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, mCoder->DequeueInputBuffer() return err = %d",
                  __FUNCTION__, (int)res);
      return -1;
    }
    return inputIndex;
  }

  void QueueInputBuffer(int32_t inputIndex, int32_t offset, size_t size, int64_t renderTimes, int32_t flags) {
    nsresult res = NS_OK;
    res = mCoder->QueueInputBuffer(inputIndex, offset, size, renderTimes, flags);

    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, mCoder->QueueInputBuffer() return err = %d",
                  __FUNCTION__, (int)res);
    }
  }

  int32_t DequeueOutputBuffer(BufferInfo::Param aInfo) {
    nsresult res;

    int32_t outputStatus;
    res = mCoder->DequeueOutputBuffer(aInfo, DECODER_TIMEOUT, &outputStatus);

    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, mCoder->DequeueOutputBuffer() return err = %d",
                  __FUNCTION__, (int)res);
      return -1;
    }

    return outputStatus;
  }

  void ReleaseOutputBuffer(int32_t index, bool flag) {
    mCoder->ReleaseOutputBuffer(index, flag);
  }

  jobjectArray GetInputBuffers() {
    JNIEnv* env = jsjni_GetJNIForThread();

    if (mInputBuffers) {
      env->DeleteGlobalRef(mInputBuffers);
    }

    nsresult res;
    jni::ObjectArray::LocalRef inputBuffers;
    res = mCoder->GetInputBuffers(&inputBuffers);
    mInputBuffers = (jobjectArray) env->NewGlobalRef(inputBuffers.Get());
    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, GetInputBuffers return err = %d",
                  __FUNCTION__, (int)res);
      return nullptr;
    }

    return mInputBuffers;
  }

  jobjectArray GetOutputBuffers() {
    JNIEnv* env = jsjni_GetJNIForThread();

    if (mOutputBuffers) {
      env->DeleteGlobalRef(mOutputBuffers);
    }

    nsresult res;
    jni::ObjectArray::LocalRef outputBuffers;
    res = mCoder->GetOutputBuffers(&outputBuffers);
    mOutputBuffers = (jobjectArray) env->NewGlobalRef(outputBuffers.Get());
    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcAndroidMediaCodec::%s, GetOutputBuffers return err = %d",
                  __FUNCTION__, (int)res);
      return nullptr;
    }

    return mOutputBuffers;
  }

  void SetDecoderCallback(webrtc::DecodedImageCallback* aCallback) {
    mDecoderCallback = aCallback;
  }

  void SetEncoderCallback(webrtc::EncodedImageCallback* aCallback) {
    mEncoderCallback = aCallback;
  }

protected:
  virtual ~WebrtcAndroidMediaCodec() {
  }

private:
class OutputDrain : public MediaCodecOutputDrain
  {
  public:
    OutputDrain(WebrtcAndroidMediaCodec* aMediaCodec)
      : MediaCodecOutputDrain()
      , mMediaCodec(aMediaCodec)
    {}

  protected:
    virtual bool DrainOutput() override
    {
      return (mMediaCodec->DrainOutput(mInputFrames, mMonitor) == NS_OK);
    }

  private:
    WebrtcAndroidMediaCodec* mMediaCodec;
  };

  friend class WebrtcMediaCodecVP8VideoEncoder;
  friend class WebrtcMediaCodecVP8VideoDecoder;

  MediaCodec::GlobalRef mCoder;
  webrtc::EncodedImageCallback* mEncoderCallback;
  webrtc::DecodedImageCallback* mDecoderCallback;
  webrtc::I420VideoFrame mVideoFrame;

  jobjectArray mInputBuffers;
  jobjectArray mOutputBuffers;

  RefPtr<OutputDrain> mOutputDrain;
  uint32_t mWidth;
  uint32_t mHeight;
  bool isStarted;
  bool mEnding;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcAndroidMediaCodec)
};

static bool I420toNV12(uint8_t* dstY, uint16_t* dstUV, const webrtc::I420VideoFrame& inputImage) {
  uint8_t* buffer = dstY;
  uint8_t* dst_y = buffer;
  int dst_stride_y = inputImage.stride(webrtc::kYPlane);
  uint8_t* dst_uv = buffer + inputImage.stride(webrtc::kYPlane) *
                    inputImage.height();
  int dst_stride_uv = inputImage.stride(webrtc::kUPlane) * 2;

  // Why NV12?  Because COLOR_FORMAT_YUV420_SEMIPLANAR.  Most hardware is NV12-friendly.
  bool converted = !libyuv::I420ToNV12(inputImage.buffer(webrtc::kYPlane),
                                       inputImage.stride(webrtc::kYPlane),
                                       inputImage.buffer(webrtc::kUPlane),
                                       inputImage.stride(webrtc::kUPlane),
                                       inputImage.buffer(webrtc::kVPlane),
                                       inputImage.stride(webrtc::kVPlane),
                                       dst_y,
                                       dst_stride_y,
                                       dst_uv,
                                       dst_stride_uv,
                                       inputImage.width(),
                                       inputImage.height());
  return converted;
}

// Encoder.
WebrtcMediaCodecVP8VideoEncoder::WebrtcMediaCodecVP8VideoEncoder()
  : mTimestamp(0)
  , mCallback(nullptr)
  , mMediaCodecEncoder(nullptr) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  memset(&mEncodedImage, 0, sizeof(mEncodedImage));
}

bool WebrtcMediaCodecVP8VideoEncoder::ResetInputBuffers() {
  mInputBuffers = mMediaCodecEncoder->GetInputBuffers();

  if (!mInputBuffers)
    return false;

  return true;
}

bool WebrtcMediaCodecVP8VideoEncoder::ResetOutputBuffers() {
  mOutputBuffers = mMediaCodecEncoder->GetOutputBuffers();

  if (!mOutputBuffers)
    return false;

  return true;
}

int32_t
WebrtcMediaCodecVP8VideoEncoder::VerifyAndAllocate(const uint32_t minimumSize)
{
    if(minimumSize > mEncodedImage._size)
    {
        // create buffer of sufficient size
        uint8_t* newBuffer = new uint8_t[minimumSize];
        if (newBuffer == nullptr) {
            return -1;
        }
        if(mEncodedImage._buffer) {
            // copy old data
            memcpy(newBuffer, mEncodedImage._buffer, mEncodedImage._size);
            delete [] mEncodedImage._buffer;
        }
        mEncodedImage._buffer = newBuffer;
        mEncodedImage._size = minimumSize;
    }
    return 0;
}

int32_t WebrtcMediaCodecVP8VideoEncoder::InitEncode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores,
    size_t maxPayloadSize) {
  mMaxPayloadSize = maxPayloadSize;
  CSFLogDebug(logTag,  "%s, w = %d, h = %d", __FUNCTION__, codecSettings->width, codecSettings->height);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaCodecVP8VideoEncoder::Encode(
    const webrtc::I420VideoFrame& inputImage,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  CSFLogDebug(logTag,  "%s, w = %d, h = %d", __FUNCTION__, inputImage.width(), inputImage.height());

  if (!mMediaCodecEncoder) {
    mMediaCodecEncoder = new WebrtcAndroidMediaCodec();
  }

  if (!mMediaCodecEncoder->isStarted) {
    if (inputImage.width() == 0 || inputImage.height() == 0) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    } else {
      mFrameWidth = inputImage.width();
      mFrameHeight = inputImage.height();
    }

    mMediaCodecEncoder->SetEncoderCallback(mCallback);
    nsresult res = mMediaCodecEncoder->Configure(mFrameWidth, mFrameHeight, nullptr, MediaCodec::CONFIGURE_FLAG_ENCODE, MEDIACODEC_VIDEO_MIME_VP8, true /* encoder */);

    if (res != NS_OK) {
      CSFLogDebug(logTag,  "%s, encoder configure return err = %d",
                  __FUNCTION__, (int)res);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    res = mMediaCodecEncoder->Start();

    if (NS_FAILED(res)) {
      mMediaCodecEncoder->isStarted = false;
      CSFLogDebug(logTag,  "%s start encoder. err = %d", __FUNCTION__, (int)res);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    bool retBool = ResetInputBuffers();
    if (!retBool) {
      CSFLogDebug(logTag,  "%s ResetInputBuffers failed.", __FUNCTION__);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    retBool = ResetOutputBuffers();
    if (!retBool) {
      CSFLogDebug(logTag,  "%s ResetOutputBuffers failed.", __FUNCTION__);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    mMediaCodecEncoder->isStarted = true;
  }

#ifdef WEBRTC_MEDIACODEC_DEBUG
  uint32_t time = PR_IntervalNow();
#endif

  size_t sizeY = inputImage.allocated_size(webrtc::kYPlane);
  size_t sizeUV = inputImage.allocated_size(webrtc::kUPlane);
  size_t size = sizeY + 2 * sizeUV;

  int inputIndex = mMediaCodecEncoder->DequeueInputBuffer(DECODER_TIMEOUT);
  if (inputIndex == -1) {
    CSFLogError(logTag,  "%s dequeue input buffer failed", __FUNCTION__);
    return inputIndex;
  }

#ifdef WEBRTC_MEDIACODEC_DEBUG
  CSFLogDebug(logTag,  "%s WebrtcMediaCodecVP8VideoEncoder::Encode() dequeue OMX input buffer took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

  if (inputIndex >= 0) {
    JNIEnv* env = jsjni_GetJNIForThread();
    jobject buffer = env->GetObjectArrayElement(mInputBuffers, inputIndex);
    void* directBuffer = env->GetDirectBufferAddress(buffer);

    uint8_t* dstY = static_cast<uint8_t*>(directBuffer);
    uint16_t* dstUV = reinterpret_cast<uint16_t*>(dstY + sizeY);

    bool converted = I420toNV12(dstY, dstUV, inputImage);
    if (!converted) {
      CSFLogError(logTag,  "%s WebrtcMediaCodecVP8VideoEncoder::Encode() convert input buffer to NV12 error.", __FUNCTION__);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    env->DeleteLocalRef(buffer);

#ifdef WEBRTC_MEDIACODEC_DEBUG
    time = PR_IntervalNow();
    CSFLogError(logTag,  "%s queue input buffer inputIndex = %d", __FUNCTION__, inputIndex);
#endif

    mMediaCodecEncoder->QueueInputBuffer(inputIndex, 0, size, inputImage.render_time_ms() * PR_USEC_PER_MSEC /* ms to us */, 0);
#ifdef WEBRTC_MEDIACODEC_DEBUG
    CSFLogDebug(logTag,  "%s WebrtcMediaCodecVP8VideoEncoder::Encode() queue input buffer took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif
    mEncodedImage._encodedWidth = inputImage.width();
    mEncodedImage._encodedHeight = inputImage.height();
    mEncodedImage._timeStamp = inputImage.timestamp();
    mEncodedImage.capture_time_ms_ = inputImage.timestamp();

    nsresult res;
    BufferInfo::LocalRef bufferInfo;
    res = BufferInfo::New(&bufferInfo);
    if (NS_FAILED(res)) {
      CSFLogDebug(logTag, "WebrtcMediaCodecVP8VideoEncoder::%s, BufferInfo::New return err = %d",
                  __FUNCTION__, (int)res);
      return -1;
    }

    int32_t outputIndex = mMediaCodecEncoder->DequeueOutputBuffer(bufferInfo);

    if (outputIndex == MediaCodec::INFO_TRY_AGAIN_LATER) {
      // Not an error: output not available yet. Try later.
      CSFLogDebug(logTag,  "%s dequeue output buffer try again:%d", __FUNCTION__, outputIndex);
    } else if (outputIndex == MediaCodec::INFO_OUTPUT_FORMAT_CHANGED) {
      // handle format change
      CSFLogDebug(logTag,  "%s dequeue output buffer format changed:%d", __FUNCTION__, outputIndex);
    } else if (outputIndex == MediaCodec::INFO_OUTPUT_BUFFERS_CHANGED) {
      CSFLogDebug(logTag,  "%s dequeue output buffer changed:%d", __FUNCTION__, outputIndex);
      mMediaCodecEncoder->GetOutputBuffers();
    } else if (outputIndex < 0) {
      CSFLogDebug(logTag,  "%s dequeue output buffer unknow error:%d", __FUNCTION__, outputIndex);
    } else {
#ifdef WEBRTC_MEDIACODEC_DEBUG
      CSFLogDebug(logTag,  "%s dequeue output buffer return status is %d took %u ms", __FUNCTION__, outputIndex, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

      JNIEnv* env = jsjni_GetJNIForThread();
      jobject buffer = env->GetObjectArrayElement(mOutputBuffers, outputIndex);
      if (buffer) {
        int32_t offset;
        bufferInfo->Offset(&offset);
        int32_t flags;
        bufferInfo->Flags(&flags);

        // The buffer will be null on Android L if we are decoding to a Surface
        void* directBuffer = reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(buffer)) + offset;

        if (flags == MediaCodec::BUFFER_FLAG_SYNC_FRAME) {
          mEncodedImage._frameType = webrtc::kKeyFrame;
        } else {
          mEncodedImage._frameType = webrtc::kDeltaFrame;
        }
        mEncodedImage._completeFrame = true;

        int32_t size;
        bufferInfo->Size(&size);
#ifdef WEBRTC_MEDIACODEC_DEBUG
        CSFLogDebug(logTag,  "%s dequeue output buffer ok, index:%d, buffer size = %d, buffer offset = %d, flags = %d", __FUNCTION__, outputIndex, size, offset, flags);
#endif

        if(VerifyAndAllocate(size) == -1) {
          CSFLogDebug(logTag,  "%s VerifyAndAllocate buffers failed", __FUNCTION__);
          return WEBRTC_VIDEO_CODEC_ERROR;
        }

        mEncodedImage._length = size;

        // xxx It's too bad the mediacodec API forces us to memcpy this....
        // we should find a way that able to 'hold' the buffer or transfer it from inputImage (ping-pong
        // buffers or select them from a small pool)
        memcpy(mEncodedImage._buffer, directBuffer, mEncodedImage._length);

        webrtc::CodecSpecificInfo info;
        info.codecType = webrtc::kVideoCodecVP8;
        info.codecSpecific.VP8.pictureId = -1;
        info.codecSpecific.VP8.tl0PicIdx = -1;
        info.codecSpecific.VP8.keyIdx = -1;
        info.codecSpecific.VP8.temporalIdx = 1;

        // Generate a header describing a single fragment.
        webrtc::RTPFragmentationHeader header;
        memset(&header, 0, sizeof(header));
        header.VerifyAndAllocateFragmentationHeader(1);
        header.fragmentationLength[0] = mEncodedImage._length;

        mCallback->Encoded(mEncodedImage, &info, &header);

        mMediaCodecEncoder->ReleaseOutputBuffer(outputIndex, false);
        env->DeleteLocalRef(buffer);
      }
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaCodecVP8VideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) {
  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  mCallback = callback;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaCodecVP8VideoEncoder::Release() {

  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  delete mMediaCodecEncoder;
  mMediaCodecEncoder = nullptr;

  delete [] mEncodedImage._buffer;
  mEncodedImage._buffer = nullptr;
  mEncodedImage._size = 0;

  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcMediaCodecVP8VideoEncoder::~WebrtcMediaCodecVP8VideoEncoder() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  Release();
}

int32_t WebrtcMediaCodecVP8VideoEncoder::SetChannelParameters(uint32_t packetLoss, int64_t rtt) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaCodecVP8VideoEncoder::SetRates(uint32_t newBitRate, uint32_t frameRate) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  if (!mMediaCodecEncoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // XXX
  // 1. implement MediaCodec's setParameters method
  // 2.find a way to initiate a Java Bundle instance as parameter for MediaCodec setParameters method.
  // mMediaCodecEncoder->setParameters

  return WEBRTC_VIDEO_CODEC_OK;
}

// Decoder.
WebrtcMediaCodecVP8VideoDecoder::WebrtcMediaCodecVP8VideoDecoder()
  : mCallback(nullptr)
  , mFrameWidth(0)
  , mFrameHeight(0)
  , mMediaCodecDecoder(nullptr) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
}

bool WebrtcMediaCodecVP8VideoDecoder::ResetInputBuffers() {
  mInputBuffers = mMediaCodecDecoder->GetInputBuffers();

  if (!mInputBuffers)
    return false;

  return true;
}

bool WebrtcMediaCodecVP8VideoDecoder::ResetOutputBuffers() {
  mOutputBuffers = mMediaCodecDecoder->GetOutputBuffers();

  if (!mOutputBuffers)
    return false;

  return true;
}


int32_t WebrtcMediaCodecVP8VideoDecoder::InitDecode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores) {

  if (!mMediaCodecDecoder) {
    mMediaCodecDecoder  = new WebrtcAndroidMediaCodec();
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaCodecVP8VideoDecoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool missingFrames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    int64_t renderTimeMs) {

  CSFLogDebug(logTag,  "%s, renderTimeMs = %lld ", __FUNCTION__, renderTimeMs);

  if (inputImage._length== 0 || !inputImage._buffer) {
    CSFLogDebug(logTag,  "%s, input Image invalid. length = %d", __FUNCTION__, inputImage._length);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (inputImage._frameType == webrtc::kKeyFrame) {
    CSFLogDebug(logTag,  "%s, inputImage is Golden frame",
                  __FUNCTION__);
    mFrameWidth = inputImage._encodedWidth;
    mFrameHeight = inputImage._encodedHeight;
  }

  if (!mMediaCodecDecoder->isStarted) {
    if (mFrameWidth == 0 || mFrameHeight == 0) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    mMediaCodecDecoder->SetDecoderCallback(mCallback);
    nsresult res = mMediaCodecDecoder->Configure(mFrameWidth, mFrameHeight, nullptr, 0, MEDIACODEC_VIDEO_MIME_VP8, false /* decoder */);

    if (res != NS_OK) {
      CSFLogDebug(logTag,  "%s, decoder configure return err = %d",
                  __FUNCTION__, (int)res);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    res = mMediaCodecDecoder->Start();

    if (NS_FAILED(res)) {
      mMediaCodecDecoder->isStarted = false;
      CSFLogDebug(logTag,  "%s start decoder. err = %d", __FUNCTION__, (int)res);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    bool retBool = ResetInputBuffers();
    if (!retBool) {
      CSFLogDebug(logTag,  "%s ResetInputBuffers failed.", __FUNCTION__);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    retBool = ResetOutputBuffers();
    if (!retBool) {
      CSFLogDebug(logTag,  "%s ResetOutputBuffers failed.", __FUNCTION__);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    mMediaCodecDecoder->isStarted = true;
  }
#ifdef WEBRTC_MEDIACODEC_DEBUG
  uint32_t time = PR_IntervalNow();
  CSFLogDebug(logTag,  "%s start decoder took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

  bool feedFrame = true;
  int32_t ret = WEBRTC_VIDEO_CODEC_ERROR;

  while (feedFrame) {
    ret = mMediaCodecDecoder->FeedMediaCodecInput(inputImage, renderTimeMs);
    feedFrame = (ret == -1);
  }

  CSFLogDebug(logTag,  "%s end, ret = %d", __FUNCTION__, ret);

  return ret;
}

void WebrtcMediaCodecVP8VideoDecoder::DecodeFrame(EncodedFrame* frame) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
}

int32_t WebrtcMediaCodecVP8VideoDecoder::RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  mCallback = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaCodecVP8VideoDecoder::Release() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  delete mMediaCodecDecoder;
  mMediaCodecDecoder = nullptr;

  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcMediaCodecVP8VideoDecoder::~WebrtcMediaCodecVP8VideoDecoder() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  Release();
}

int32_t WebrtcMediaCodecVP8VideoDecoder::Reset() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  return WEBRTC_VIDEO_CODEC_OK;
}

}
