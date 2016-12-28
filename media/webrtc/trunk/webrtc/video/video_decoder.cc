/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_decoder.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"

namespace webrtc {
VideoDecoder* VideoDecoder::Create(VideoDecoder::DecoderType codec_type) {
  switch (codec_type) {
    case kH264:
      RTC_DCHECK(H264Decoder::IsSupported());
      return H264Decoder::Create();
    case kVp8:
      return VP8Decoder::Create();
    case kVp9:
      return VP9Decoder::Create();
    case kUnsupportedCodec:
      RTC_NOTREACHED();
      return nullptr;
  }
  RTC_NOTREACHED();
  return nullptr;
}

VideoDecoder::DecoderType CodecTypeToDecoderType(VideoCodecType codec_type) {
  switch (codec_type) {
    case kVideoCodecH264:
      return VideoDecoder::kH264;
    case kVideoCodecVP8:
      return VideoDecoder::kVp8;
    case kVideoCodecVP9:
      return VideoDecoder::kVp9;
    default:
      return VideoDecoder::kUnsupportedCodec;
  }
}

VideoDecoderSoftwareFallbackWrapper::VideoDecoderSoftwareFallbackWrapper(
    VideoCodecType codec_type,
    VideoDecoder* decoder)
    : decoder_type_(CodecTypeToDecoderType(codec_type)),
      decoder_(decoder),
      callback_(nullptr) {
}

int32_t VideoDecoderSoftwareFallbackWrapper::InitDecode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  return decoder_->InitDecode(codec_settings, number_of_cores);
}

bool VideoDecoderSoftwareFallbackWrapper::InitFallbackDecoder() {
  RTC_CHECK(decoder_type_ != kUnsupportedCodec)
      << "Decoder requesting fallback to codec not supported in software.";
  LOG(LS_WARNING) << "Decoder falling back to software decoding.";
  fallback_decoder_.reset(VideoDecoder::Create(decoder_type_));
  if (fallback_decoder_->InitDecode(&codec_settings_, number_of_cores_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_ERROR) << "Failed to initialize software-decoder fallback.";
    fallback_decoder_.reset();
    return false;
  }
  if (callback_ != nullptr)
    fallback_decoder_->RegisterDecodeCompleteCallback(callback_);
  fallback_implementation_name_ =
      std::string(fallback_decoder_->ImplementationName()) +
      " (fallback from: " + decoder_->ImplementationName() + ")";
  return true;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  // Try decoding with the provided decoder on every keyframe or when there's no
  // fallback decoder. This is the normal case.
  if (!fallback_decoder_ || input_image._frameType == kVideoFrameKey) {
    int32_t ret = decoder_->Decode(input_image, missing_frames, fragmentation,
                                   codec_specific_info, render_time_ms);
    if (ret == WEBRTC_VIDEO_CODEC_OK) {
      if (fallback_decoder_) {
        // Decode OK -> stop using fallback decoder.
        fallback_decoder_->Release();
        fallback_decoder_.reset();
        return WEBRTC_VIDEO_CODEC_OK;
      }
    }
    if (ret != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE)
      return ret;
    if (!fallback_decoder_) {
      // Try to initialize fallback decoder.
      if (!InitFallbackDecoder())
        return ret;
    }
  }
  return fallback_decoder_->Decode(input_image, missing_frames, fragmentation,
                                   codec_specific_info, render_time_ms);
}

int32_t VideoDecoderSoftwareFallbackWrapper::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  int32_t ret = decoder_->RegisterDecodeCompleteCallback(callback);
  if (fallback_decoder_)
    return fallback_decoder_->RegisterDecodeCompleteCallback(callback);
  return ret;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Release() {
  if (fallback_decoder_)
    fallback_decoder_->Release();
  return decoder_->Release();
}

int32_t VideoDecoderSoftwareFallbackWrapper::Reset() {
  if (fallback_decoder_)
    fallback_decoder_->Reset();
  return decoder_->Reset();
}

bool VideoDecoderSoftwareFallbackWrapper::PrefersLateDecoding() const {
  if (fallback_decoder_)
    return fallback_decoder_->PrefersLateDecoding();
  return decoder_->PrefersLateDecoding();
}

const char* VideoDecoderSoftwareFallbackWrapper::ImplementationName() const {
  if (fallback_decoder_)
    return fallback_implementation_name_.c_str();
  return decoder_->ImplementationName();
}

}  // namespace webrtc
