/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/stereo/include/stereo_decoder_adapter.h"

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/sdp_video_format.h"
#include "common_video/include/video_frame.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"

namespace {
void KeepBufferRefs(rtc::scoped_refptr<webrtc::VideoFrameBuffer>,
                    rtc::scoped_refptr<webrtc::VideoFrameBuffer>) {}
}  // anonymous namespace

namespace webrtc {

class StereoDecoderAdapter::AdapterDecodedImageCallback
    : public webrtc::DecodedImageCallback {
 public:
  AdapterDecodedImageCallback(webrtc::StereoDecoderAdapter* adapter,
                              AlphaCodecStream stream_idx)
      : adapter_(adapter), stream_idx_(stream_idx) {}

  void Decoded(VideoFrame& decodedImage,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp) override {
    if (!adapter_)
      return;
    adapter_->Decoded(stream_idx_, &decodedImage, decode_time_ms, qp);
  }
  int32_t Decoded(VideoFrame& decodedImage) override {
    RTC_NOTREACHED();
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) override {
    RTC_NOTREACHED();
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  StereoDecoderAdapter* adapter_;
  const AlphaCodecStream stream_idx_;
};

struct StereoDecoderAdapter::DecodedImageData {
  explicit DecodedImageData(AlphaCodecStream stream_idx)
      : stream_idx_(stream_idx),
        decodedImage_(I420Buffer::Create(1 /* width */, 1 /* height */),
                      0,
                      0,
                      kVideoRotation_0) {
    RTC_DCHECK_EQ(kAXXStream, stream_idx);
  }
  DecodedImageData(AlphaCodecStream stream_idx,
                   const VideoFrame& decodedImage,
                   const rtc::Optional<int32_t>& decode_time_ms,
                   const rtc::Optional<uint8_t>& qp)
      : stream_idx_(stream_idx),
        decodedImage_(decodedImage),
        decode_time_ms_(decode_time_ms),
        qp_(qp) {}
  const AlphaCodecStream stream_idx_;
  VideoFrame decodedImage_;
  const rtc::Optional<int32_t> decode_time_ms_;
  const rtc::Optional<uint8_t> qp_;

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DecodedImageData);
};

StereoDecoderAdapter::StereoDecoderAdapter(VideoDecoderFactory* factory)
    : factory_(factory) {}

StereoDecoderAdapter::~StereoDecoderAdapter() {
  Release();
}

int32_t StereoDecoderAdapter::InitDecode(const VideoCodec* codec_settings,
                                         int32_t number_of_cores) {
  VideoCodec settings = *codec_settings;
  settings.codecType = kVideoCodecVP9;
  for (size_t i = 0; i < kAlphaCodecStreams; ++i) {
    const SdpVideoFormat format("VP9");
    std::unique_ptr<VideoDecoder> decoder =
        factory_->CreateVideoDecoder(format);
    const int32_t rv = decoder->InitDecode(&settings, number_of_cores);
    if (rv)
      return rv;
    adapter_callbacks_.emplace_back(
        new StereoDecoderAdapter::AdapterDecodedImageCallback(
            this, static_cast<AlphaCodecStream>(i)));
    decoder->RegisterDecodeCompleteCallback(adapter_callbacks_.back().get());
    decoders_.emplace_back(std::move(decoder));
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t StereoDecoderAdapter::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* /*fragmentation*/,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  // TODO(emircan): Read |codec_specific_info->stereoInfo| to split frames.
  int32_t rv =
      decoders_[kYUVStream]->Decode(input_image, missing_frames, nullptr,
                                    codec_specific_info, render_time_ms);
  if (rv)
    return rv;
  rv = decoders_[kAXXStream]->Decode(input_image, missing_frames, nullptr,
                                     codec_specific_info, render_time_ms);
  return rv;
}

int32_t StereoDecoderAdapter::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t StereoDecoderAdapter::Release() {
  for (auto& decoder : decoders_) {
    const int32_t rv = decoder->Release();
    if (rv)
      return rv;
  }
  decoders_.clear();
  adapter_callbacks_.clear();
  return WEBRTC_VIDEO_CODEC_OK;
}

void StereoDecoderAdapter::Decoded(AlphaCodecStream stream_idx,
                                   VideoFrame* decoded_image,
                                   rtc::Optional<int32_t> decode_time_ms,
                                   rtc::Optional<uint8_t> qp) {
  const auto& other_decoded_data_it =
      decoded_data_.find(decoded_image->timestamp());
  if (other_decoded_data_it != decoded_data_.end()) {
    auto& other_image_data = other_decoded_data_it->second;
    if (stream_idx == kYUVStream) {
      RTC_DCHECK_EQ(kAXXStream, other_image_data.stream_idx_);
      MergeAlphaImages(decoded_image, decode_time_ms, qp,
                       &other_image_data.decodedImage_,
                       other_image_data.decode_time_ms_, other_image_data.qp_);
    } else {
      RTC_DCHECK_EQ(kYUVStream, other_image_data.stream_idx_);
      RTC_DCHECK_EQ(kAXXStream, stream_idx);
      MergeAlphaImages(&other_image_data.decodedImage_,
                       other_image_data.decode_time_ms_, other_image_data.qp_,
                       decoded_image, decode_time_ms, qp);
    }
    decoded_data_.erase(decoded_data_.begin(), other_decoded_data_it);
    return;
  }
  RTC_DCHECK(decoded_data_.find(decoded_image->timestamp()) ==
             decoded_data_.end());
  decoded_data_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(decoded_image->timestamp()),
      std::forward_as_tuple(stream_idx, *decoded_image, decode_time_ms, qp));
}

void StereoDecoderAdapter::MergeAlphaImages(
    VideoFrame* decodedImage,
    const rtc::Optional<int32_t>& decode_time_ms,
    const rtc::Optional<uint8_t>& qp,
    VideoFrame* alpha_decodedImage,
    const rtc::Optional<int32_t>& alpha_decode_time_ms,
    const rtc::Optional<uint8_t>& alpha_qp) {
  rtc::scoped_refptr<webrtc::I420BufferInterface> yuv_buffer =
      decodedImage->video_frame_buffer()->ToI420();
  rtc::scoped_refptr<webrtc::I420BufferInterface> alpha_buffer =
      alpha_decodedImage->video_frame_buffer()->ToI420();
  RTC_DCHECK_EQ(yuv_buffer->width(), alpha_buffer->width());
  RTC_DCHECK_EQ(yuv_buffer->height(), alpha_buffer->height());
  rtc::scoped_refptr<I420ABufferInterface> merged_buffer = WrapI420ABuffer(
      yuv_buffer->width(), yuv_buffer->height(), yuv_buffer->DataY(),
      yuv_buffer->StrideY(), yuv_buffer->DataU(), yuv_buffer->StrideU(),
      yuv_buffer->DataV(), yuv_buffer->StrideV(), alpha_buffer->DataY(),
      alpha_buffer->StrideY(),
      rtc::Bind(&KeepBufferRefs, yuv_buffer, alpha_buffer));

  VideoFrame merged_image(merged_buffer, decodedImage->timestamp(),
                          0 /* render_time_ms */, decodedImage->rotation());
  decoded_complete_callback_->Decoded(merged_image, decode_time_ms, qp);
}

}  // namespace webrtc
