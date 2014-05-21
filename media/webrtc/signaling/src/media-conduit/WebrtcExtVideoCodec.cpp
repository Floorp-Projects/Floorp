/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <iostream>

#include "CSFLog.h"
#include "nspr.h"
#include "runnable_utils.h"

#include "WebrtcExtVideoCodec.h"
#include "mozilla/Scoped.h"
#include "mozilla/ArrayUtils.h"

#include "AudioConduit.h"
#include "VideoConduit.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert.h"
#include "libyuv/row.h"

#include <utils/Log.h>
#include <ICrypto.h>
#include <IOMX.h>
#include <OMX_VideoExt.h>
#include <gui/Surface.h>
#include <stagefright/MediaCodec.h>
#include <stagefright/MediaDefs.h>
#include <stagefright/MediaErrors.h>
#include <stagefright/foundation/ABuffer.h>
#include <stagefright/foundation/AMessage.h>
#include <vie_external_codec.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

using namespace android;

static const int64_t DEQUEUE_BUFFER_TIMEOUT_US = 100 * 1000ll; // 100ms
static const int64_t START_DEQUEUE_BUFFER_TIMEOUT_US = 10 * DEQUEUE_BUFFER_TIMEOUT_US; // 1s
static const char MEDIACODEC_VIDEO_MIME_VP8[] = "video/x-vnd.on2.vp8";

namespace mozilla {

static const char* logTag ="WebrtcExtVideoCodec";

// Link to Stagefright
class WebrtcOMX {
public:
  WebrtcOMX()
    : mLooper(new ALooper),
      mMsg(new AMessage),
      mOmx(nullptr),
      isStarted(false) {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);

    mLooper->start();
  }

  virtual ~WebrtcOMX() {
    if (mMsg != nullptr) {
      mMsg.clear();
    }
    if (mOmx != nullptr) {
      mOmx->release();
    }
    if (mLooper != nullptr) {
      mLooper.clear();
    }
  }

  status_t Configure(const sp<Surface>& nativeWindow,
                     const sp<ICrypto>& crypto,
                     uint32_t flags,
                     const char* mime,
                     bool encoder) {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);
    status_t err = BAD_VALUE;

    if (mOmx == nullptr) {
      mOmx = MediaCodec::CreateByType(mLooper, mime, encoder);
      if (mOmx == nullptr) {
        return BAD_VALUE;
      }
    }

    err = mOmx->configure(mMsg, nativeWindow, crypto, flags);
    CSFLogDebug(logTag, "WebrtcOMX::%s, err = %d", __FUNCTION__, err);

    return err;
  }

  status_t Start() {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);

    if (mOmx == nullptr) {
      return BAD_VALUE;
    }

    status_t err = mOmx->start();
    CSFLogDebug(logTag, "WebrtcOMX::%s, mOmx->start() return err = %d",
                __FUNCTION__, err);

    mOmx->getInputBuffers(&mInput);
    mOmx->getOutputBuffers(&mOutput);

    return err;
  }

  status_t Stop() {
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);

    return mOmx->stop();
  }

  Vector<sp<ABuffer>>* getInput() {
    return &mInput;
  }

  Vector<sp<ABuffer>>* getOutput() {
    return &mOutput;
  }

private:
  friend class WebrtcExtVideoEncoder;
  friend class WebrtcExtVideoDecoder;

  sp<ALooper> mLooper;
  sp<MediaCodec> mOmx;
  sp<AMessage> mMsg;
  bool isStarted;
  Vector<sp<ABuffer>> mInput;
  Vector<sp<ABuffer>> mOutput;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcOMX)
};


// Encoder.
WebrtcExtVideoEncoder::WebrtcExtVideoEncoder()
: mTimestamp(0),
  mCallback(nullptr),
  mOmxEncoder(nullptr) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
}

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

static void VideoCodecSettings2AMessage(
    sp<AMessage> &format,
    const webrtc::VideoCodec* codecSettings,
    const char* mime,
    bool isEncoder) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  format->setString("mime", mime);
  format->setInt32("width", codecSettings->width);
  format->setInt32("height", codecSettings->height);
  format->setInt32("bitrate", codecSettings->startBitrate * 1000); // kbps->bps
  format->setInt32("frame-rate", 30);
  // QCOM encoder only support this format. See
  // <B2G>/hardware/qcom/media/mm-video/vidc/venc/src/video_encoder_device.cpp:
  // venc_dev::venc_set_color_format()
  format->setInt32("color-format", OMX_COLOR_FormatYUV420SemiPlanar);
  if (isEncoder) {
    format->setInt32("i-frame-interval", std::numeric_limits<int>::max()); // should generate I-frame on demand
  }
}

int32_t WebrtcExtVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores,
    uint32_t maxPayloadSize) {
  mMaxPayloadSize = maxPayloadSize;
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  if (!mOmxEncoder) {
    mOmxEncoder = new WebrtcOMX();
  } else {
    VideoCodecSettings2AMessage(mOmxEncoder->mMsg, codecSettings, MEDIACODEC_VIDEO_MIME_VP8, true /*encoder*/);

    status_t err = mOmxEncoder->Configure(nullptr, nullptr, MediaCodec::CONFIGURE_FLAG_ENCODE, MEDIACODEC_VIDEO_MIME_VP8, true /* encoder */);
    if (err != OK) {
      CSFLogDebug(logTag, "MediaCodecEncoderImpl::%s, err = %d", __FUNCTION__, err);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    mOmxEncoder->isStarted = false;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcExtVideoEncoder::Encode(
    const webrtc::I420VideoFrame& inputImage,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  status_t err = OK;

  if (!mOmxEncoder) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

#ifdef DEBUG
  uint32_t time = PR_IntervalNow();
#endif
  if (!mOmxEncoder->isStarted) {
    err = mOmxEncoder->Start();
    if (err != OK) {
      mOmxEncoder->isStarted = false;
      CSFLogDebug(logTag,  "%s mOmxEncoder->start(), err = %d", __FUNCTION__, err);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    mOmxEncoder->isStarted = true;
  }

  size_t sizeY = inputImage.allocated_size(webrtc::kYPlane);
  size_t sizeUV = inputImage.allocated_size(webrtc::kUPlane);
  size_t size = sizeY + 2 * sizeUV;

  sp<MediaCodec> omx = mOmxEncoder->mOmx;
  size_t index = 0;
  err = omx->dequeueInputBuffer(&index, DEQUEUE_BUFFER_TIMEOUT_US);
  if (err != OK) {
    CSFLogError(logTag,  "%s WebrtcExtVideoEncoder::Encode() dequeue OMX input buffer error:%d", __FUNCTION__, err);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

#ifdef DEBUG
  CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() dequeue OMX input buffer took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

  const sp<ABuffer>& omxIn = mOmxEncoder->getInput()->itemAt(index);
  omxIn->setRange(0, size);
  uint8_t* dstY = omxIn->data();
  uint16_t* dstUV = reinterpret_cast<uint16_t*>(dstY + sizeY);

  bool converted = I420toNV12(dstY, dstUV, inputImage);
  if (!converted) {
    CSFLogError(logTag,  "%s WebrtcExtVideoEncoder::Encode() convert input buffer to NV12 error.", __FUNCTION__);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

#ifdef DEBUG
  time = PR_IntervalNow();
#endif

  err = omx->queueInputBuffer(index, 0, size, inputImage.render_time_ms() * PR_USEC_PER_MSEC /* ms to us */, 0);
  if (err != OK) {
    CSFLogError(logTag,  "%s WebrtcExtVideoEncoder::Encode() queue OMX input buffer error:%d", __FUNCTION__, err);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

#ifdef DEBUG
  CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() queue OMX input buffer took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

  mEncodedImage._encodedWidth = inputImage.width();
  mEncodedImage._encodedHeight = inputImage.height();
  mEncodedImage._timeStamp = inputImage.timestamp();
  mEncodedImage.capture_time_ms_ = inputImage.timestamp();

  size_t outOffset;
  size_t outSize;
  int64_t outTime;
  uint32_t outFlags;
  err = omx->dequeueOutputBuffer(&index, &outOffset, &outSize, &outTime, &outFlags, 0);
  if (err == INFO_FORMAT_CHANGED) {
    CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() dequeue OMX output format change", __FUNCTION__);
    return WEBRTC_VIDEO_CODEC_OK;
  } else if (err == INFO_OUTPUT_BUFFERS_CHANGED) {
    err = omx->getOutputBuffers(mOmxEncoder->getOutput());
    CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() dequeue OMX output buffer change", __FUNCTION__);
    return WEBRTC_VIDEO_CODEC_OK;
  } else if (err != OK) {
    CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() dequeue OMX output buffer error:%d", __FUNCTION__, err);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

#ifdef DEBUG
  CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() dequeue OMX output buffer err:%d len:%u time:%lld flags:0x%08x took %u ms",
              __FUNCTION__, err, outSize, outTime, outFlags, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

  sp<ABuffer> omxOut = mOmxEncoder->getOutput()->itemAt(index);
  mEncodedImage._length = omxOut->size();
  uint8_t* data = omxOut->data();

  // xxx It's too bad the mediacodec native API forces us to memcpy this....
  // we should find a way that able to 'hold' the buffer or transfer it from inputImage (ping-pong
  // buffers or select them from a small pool)
  memcpy(mEncodedImage._buffer, data, mEncodedImage._length);
  omx->releaseOutputBuffer(index);

  CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() outFlags:%d ", __FUNCTION__, outFlags);

  mEncodedImage._completeFrame = true;
  mEncodedImage._frameType =
    (outFlags & (MediaCodec::BUFFER_FLAG_SYNCFRAME | MediaCodec::BUFFER_FLAG_CODECCONFIG)) ?
    webrtc::kKeyFrame : webrtc::kDeltaFrame;

  CSFLogDebug(logTag,  "%s WebrtcExtVideoEncoder::Encode() frame type:%d size:%u", __FUNCTION__, mEncodedImage._frameType, mEncodedImage._length);

  webrtc::CodecSpecificInfo info;
  info.codecType = webrtc::kVideoCodecVP8;
  info.codecSpecific.VP8.pictureId = -1;
  info.codecSpecific.VP8.tl0PicIdx = -1;
  info.codecSpecific.VP8.keyIdx = -1;

  // Generate a header describing a single fragment.
  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));
  header.VerifyAndAllocateFragmentationHeader(1);
  header.fragmentationLength[0] = mEncodedImage._length;

  mCallback->Encoded(mEncodedImage, &info, &header);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcExtVideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) {
  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  mCallback = callback;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcExtVideoEncoder::Release() {

  CSFLogDebug(logTag, "%s ", __FUNCTION__);
  delete mOmxEncoder;
  mOmxEncoder = nullptr;

  delete [] mEncodedImage._buffer;
  mEncodedImage._buffer = nullptr;
  mEncodedImage._size = 0;

  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcExtVideoEncoder::~WebrtcExtVideoEncoder() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  Release();
}

int32_t WebrtcExtVideoEncoder::SetChannelParameters(uint32_t packetLoss, int rtt) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcExtVideoEncoder::SetRates(uint32_t newBitRate, uint32_t frameRate) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  if (!mOmxEncoder || !mOmxEncoder->mOmx.get()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  sp<MediaCodec> omx = mOmxEncoder->mOmx;
  sp<AMessage> msg = new AMessage;
  msg->setInt32("videoBitrate", newBitRate * 1000 /* kbps -> bps */);
  msg->setInt32("frame-rate", frameRate);
  omx->setParameters(msg);

  return WEBRTC_VIDEO_CODEC_OK;
}


// Decoder.
WebrtcExtVideoDecoder::WebrtcExtVideoDecoder()
  : mCallback(nullptr)
  , mFrameWidth(0)
  , mFrameHeight(0)
  , mOmxDecoder(nullptr) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
}

int32_t WebrtcExtVideoDecoder::InitDecode(
    const webrtc::VideoCodec* codecSettings,
    int32_t numberOfCores) {

  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  if (!mOmxDecoder) {
    mOmxDecoder  = new WebrtcOMX();
  } else {
    VideoCodecSettings2AMessage(mOmxDecoder->mMsg, codecSettings, MEDIACODEC_VIDEO_MIME_VP8, false /* decoder */);

    status_t err = mOmxDecoder->Configure(nullptr, nullptr, 0, MEDIACODEC_VIDEO_MIME_VP8, false /* decoder */);

    if (err != OK) {
      CSFLogDebug(logTag,  "MediaCodecDecoderImpl::%s, decoder configure return err = %d",
                  __FUNCTION__, err);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

  }
  mOmxDecoder->isStarted = false;

  return WEBRTC_VIDEO_CODEC_OK;
}

static void GenerateVideoFrame(
    size_t width, size_t height, uint32_t timeStamp,
    const sp<ABuffer>& decoded,
    webrtc::I420VideoFrame* videoFrame) {

  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  // TODO: eliminate extra pixel copy/color conversion
  // TODO: The hardware output buffer is likely to contain the pixels in a vendor-specific,
  //        opaque/non-standard format.  It's not possible to negotiate the decoder
  //        to emit a specific colorspace
  //        For example, QCOM HW only support OMX_QCOM_COLOR_FormatYVU420PackedSemiPlanar32m4ka
  //                     INTEL HW support OMX_COLOR_FormatYUV420PackedSemiPlanar
  size_t widthUV = (width + 1) / 2;
  if (videoFrame->CreateEmptyFrame(width, height, width, widthUV, widthUV)) {
    return;
  }

  uint8_t* src_nv12 = decoded->data();
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

static status_t FeedOMXInput(
    WebrtcOMX* decoder,
    const sp<MediaCodec>& omx,
    const webrtc::EncodedImage& inputImage,
    int64_t renderTimeMs) {

  static int64_t firstTime = -1;
  size_t index;

#ifdef DEBUG
  uint32_t time = PR_IntervalNow();
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
#endif

  status_t err = omx->dequeueInputBuffer(&index,
                                         (firstTime < 0) ? START_DEQUEUE_BUFFER_TIMEOUT_US : DEQUEUE_BUFFER_TIMEOUT_US);
  if (err != OK) {
    CSFLogError(logTag,  "%s WebrtcExtVideoDecoder::Decode() dequeue input buffer error:%d", __FUNCTION__, err);
    return err;
  }

#ifdef DEBUG
  CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() dequeue input buffer took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
  time = PR_IntervalNow();
#endif

  uint32_t flags = 0;
  if (inputImage._frameType == webrtc::kKeyFrame) {
    flags |= (firstTime < 0) ? MediaCodec::BUFFER_FLAG_CODECCONFIG : MediaCodec::BUFFER_FLAG_SYNCFRAME;
  }
  size_t size = inputImage._length;
  const sp<ABuffer>& omxIn = decoder->getInput()->itemAt(index);
  omxIn->setRange(0, size);

  // TODO, find a better way to eliminate this memcpy ...
  memcpy(omxIn->data(), inputImage._buffer, size);

  if (firstTime < 0) {
    firstTime = inputImage._timeStamp;
  }

  err = omx->queueInputBuffer(index, 0, size, renderTimeMs, flags);

#ifdef DEBUG
  CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() queue input buffer len:%u flags:%u time:%lld took %u ms",
              __FUNCTION__, size, flags, *timeUs, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

  return err;
}

static status_t GetOMXOutput(
    WebrtcOMX* decoder,
    const sp<MediaCodec>& omx,
    const webrtc::EncodedImage& inputImage,
    webrtc::I420VideoFrame* video_frame,
    webrtc::DecodedImageCallback* mCallback,
    uint32_t width,
    uint32_t height) {

  sp<ABuffer> omxOut;

  CSFLogDebug(logTag,  "%s , encodedWidth = %d, encodedHeight = %d",
              __FUNCTION__, inputImage._encodedWidth, inputImage._encodedHeight);

#ifdef DEBUG
  uint32_t time = PR_IntervalNow();
#endif

  size_t index;
  size_t outOffset;
  size_t outSize;
  int64_t outTime;
  uint32_t outFlags;
  status_t err = omx->dequeueOutputBuffer(&index, &outOffset, &outSize, &outTime, &outFlags);
  if (err == INFO_FORMAT_CHANGED) {
    // handle format change
    CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  } else if (err == INFO_OUTPUT_BUFFERS_CHANGED) {
    CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() dequeue OMX output buffer change:%d", __FUNCTION__, err);
    err = omx->getOutputBuffers(decoder->getOutput());
    MOZ_ASSERT(err == OK);
    err = INFO_OUTPUT_BUFFERS_CHANGED;
  } else if (err != OK) {
    CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() dequeue OMX output buffer error:%d", __FUNCTION__, err);
  } else {
    omxOut = decoder->getOutput()->itemAt(index);

#ifdef DEBUG
    CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() dequeue output buffer#%u(%p) err:%d len:%u time:%lld flags:0x%08x took %u ms", __FUNCTION__, index, omxOut.get(), err, outSize, outTime, outFlags, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif

    CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() generate video frame", __FUNCTION__);
    GenerateVideoFrame(width, height, inputImage._timeStamp, omxOut, video_frame);
    mCallback->Decoded(*video_frame);
    omx->releaseOutputBuffer(index);
  }
  return err;
}

int32_t WebrtcExtVideoDecoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool missingFrames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo* codecSpecificInfo,
    int64_t renderTimeMs) {

  CSFLogDebug(logTag,  "%s, renderTimeMs = %lld ", __FUNCTION__, renderTimeMs);

  if (inputImage._length== 0 || !inputImage._buffer) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (inputImage._frameType == webrtc::kKeyFrame) {
    mFrameWidth = inputImage._encodedWidth;
    mFrameHeight = inputImage._encodedHeight;
  }

  if (!mOmxDecoder->isStarted) {
#ifdef DEBUG
    uint32_t time = PR_IntervalNow();
#endif
    status_t err = mOmxDecoder->Start();

    if (err != OK) {
      mOmxDecoder->isStarted = false;
      CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() start decoder. err = %d", __FUNCTION__, err);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    mOmxDecoder->isStarted = true;

#ifdef DEBUG
    CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() start decoder took %u ms", __FUNCTION__, PR_IntervalToMilliseconds(PR_IntervalNow()-time));
#endif
  }

  sp<MediaCodec> omx = mOmxDecoder->mOmx;
  bool feedFrame = true;
  status_t err;

  while (feedFrame) {
    err = FeedOMXInput(mOmxDecoder, omx, inputImage, renderTimeMs);
    feedFrame = (err == -EAGAIN);
    do {
      err = GetOMXOutput(mOmxDecoder, omx, inputImage, &mVideoFrame, mCallback, mFrameWidth, mFrameHeight);
    } while (err == INFO_OUTPUT_BUFFERS_CHANGED);
  }

  CSFLogDebug(logTag,  "%s WebrtcExtVideoDecoder::Decode() end, err = %d", __FUNCTION__, err);

  if (err != OK) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcExtVideoDecoder::DecodeFrame(EncodedFrame* frame) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
}

int32_t WebrtcExtVideoDecoder::RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  mCallback = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcExtVideoDecoder::Release() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  delete mOmxDecoder;
  mOmxDecoder = nullptr;

  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcExtVideoDecoder::~WebrtcExtVideoDecoder() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);

  Release();
}

int32_t WebrtcExtVideoDecoder::Reset() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  return WEBRTC_VIDEO_CODEC_OK;
}

}
