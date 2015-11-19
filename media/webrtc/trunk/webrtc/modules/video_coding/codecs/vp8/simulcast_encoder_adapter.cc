/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"

#include <algorithm>

// NOTE(ajm): Path provided by gyp.
#include "libyuv/scale.h"  // NOLINT

#include "webrtc/common.h"
#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"

namespace {

const unsigned int kDefaultMinQp = 2;
const unsigned int kDefaultMaxQp = 56;
// Max qp for lowest spatial resolution when doing simulcast.
const unsigned int kLowestResMaxQp = 45;

uint32_t SumStreamTargetBitrate(int streams, const webrtc::VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].targetBitrate;
  }
  return bitrate_sum;
}

uint32_t SumStreamMaxBitrate(int streams, const webrtc::VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].maxBitrate;
  }
  return bitrate_sum;
}

int NumberOfStreams(const webrtc::VideoCodec& codec) {
  int streams =
      codec.numberOfSimulcastStreams < 1 ? 1 : codec.numberOfSimulcastStreams;
  uint32_t simulcast_max_bitrate = SumStreamMaxBitrate(streams, codec);
  if (simulcast_max_bitrate == 0) {
    streams = 1;
  }
  return streams;
}

bool ValidSimulcastResolutions(const webrtc::VideoCodec& codec,
                               int num_streams) {
  if (codec.width != codec.simulcastStream[num_streams - 1].width ||
      codec.height != codec.simulcastStream[num_streams - 1].height) {
    return false;
  }
  for (int i = 0; i < num_streams; ++i) {
    if (codec.width * codec.simulcastStream[i].height !=
        codec.height * codec.simulcastStream[i].width) {
      return false;
    }
  }
  return true;
}

int VerifyCodec(const webrtc::VideoCodec* inst) {
  if (inst == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // allow zero to represent an unspecified maxBitRate
  if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->width <= 1 || inst->height <= 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->codecSpecific.VP8.feedbackModeOn &&
      inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->codecSpecific.VP8.automaticResizeOn &&
      inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

// TL1 FrameDropper's max time to drop frames.
const float kTl1MaxTimeToDropFrames = 20.0f;

struct ScreenshareTemporalLayersFactory : webrtc::TemporalLayers::Factory {
  ScreenshareTemporalLayersFactory()
      : tl1_frame_dropper_(kTl1MaxTimeToDropFrames) {}

  virtual ~ScreenshareTemporalLayersFactory() {}

  virtual webrtc::TemporalLayers* Create(int num_temporal_layers,
                                         uint8_t initial_tl0_pic_idx) const {
    return new webrtc::ScreenshareLayers(num_temporal_layers,
                                         rand(),
                                         &tl0_frame_dropper_,
                                         &tl1_frame_dropper_);
  }

  mutable webrtc::FrameDropper tl0_frame_dropper_;
  mutable webrtc::FrameDropper tl1_frame_dropper_;
};

}  // namespace

namespace webrtc {

SimulcastEncoderAdapter::SimulcastEncoderAdapter(VideoEncoderFactory* factory)
    : factory_(factory), encoded_complete_callback_(NULL) {
  memset(&codec_, 0, sizeof(webrtc::VideoCodec));
}

SimulcastEncoderAdapter::~SimulcastEncoderAdapter() {
  Release();
}

int SimulcastEncoderAdapter::Release() {
  // TODO(pbos): Keep the last encoder instance but call ::Release() on it, then
  // re-use this instance in ::InitEncode(). This means that changing
  // resolutions doesn't require reallocation of the first encoder, but only
  // reinitialization, which makes sense. Then Destroy this instance instead in
  // ~SimulcastEncoderAdapter().
  while (!streaminfos_.empty()) {
    VideoEncoder* encoder = streaminfos_.back().encoder;
    factory_->Destroy(encoder);
    streaminfos_.pop_back();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::InitEncode(const VideoCodec* inst,
                                        int number_of_cores,
                                        size_t max_payload_size) {
  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int ret = VerifyCodec(inst);
  if (ret < 0) {
    return ret;
  }

  ret = Release();
  if (ret < 0) {
    return ret;
  }

  int number_of_streams = NumberOfStreams(*inst);
  bool doing_simulcast = (number_of_streams > 1);

  if (doing_simulcast && !ValidSimulcastResolutions(*inst, number_of_streams)) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  codec_ = *inst;

  // Special mode when screensharing on a single stream.
  if (number_of_streams == 1 && inst->mode == kScreensharing) {
    screensharing_extra_options_.reset(new Config());
    screensharing_extra_options_->Set<TemporalLayers::Factory>(
        new ScreenshareTemporalLayersFactory());
    codec_.extra_options = screensharing_extra_options_.get();
  }

  // Create |number_of_streams| of encoder instances and init them.
  for (int i = 0; i < number_of_streams; ++i) {
    VideoCodec stream_codec;
    bool send_stream = true;
    if (!doing_simulcast) {
      stream_codec = codec_;
      stream_codec.numberOfSimulcastStreams = 1;
    } else {
      bool highest_resolution_stream = (i == (number_of_streams - 1));
      PopulateStreamCodec(&codec_, i, highest_resolution_stream,
                          &stream_codec, &send_stream);
    }

    // TODO(ronghuawu): Remove once this is handled in VP8EncoderImpl.
    if (stream_codec.qpMax < kDefaultMinQp) {
      stream_codec.qpMax = kDefaultMaxQp;
    }

    VideoEncoder* encoder = factory_->Create();
    ret = encoder->InitEncode(&stream_codec,
                              number_of_cores,
                              max_payload_size);
    if (ret < 0) {
      Release();
      return ret;
    }
    encoder->RegisterEncodeCompleteCallback(this);
    streaminfos_.push_back(StreamInfo(encoder,
                                      stream_codec.width,
                                      stream_codec.height,
                                      send_stream));
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::Encode(
    const I420VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<VideoFrameType>* frame_types) {
  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoded_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // All active streams should generate a key frame if
  // a key frame is requested by any stream.
  bool send_key_frame = false;
  if (frame_types) {
    for (size_t i = 0; i < frame_types->size(); ++i) {
      if (frame_types->at(i) == kKeyFrame) {
        send_key_frame = true;
        break;
      }
    }
  }
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    if (streaminfos_[stream_idx].key_frame_request &&
        streaminfos_[stream_idx].send_stream) {
      send_key_frame = true;
      break;
    }
  }

  int src_width = input_image.width();
  int src_height = input_image.height();
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    std::vector<VideoFrameType> stream_frame_types;
    if (send_key_frame) {
      stream_frame_types.push_back(kKeyFrame);
      streaminfos_[stream_idx].key_frame_request = false;
    } else {
      stream_frame_types.push_back(kDeltaFrame);
    }

    int dst_width = streaminfos_[stream_idx].width;
    int dst_height = streaminfos_[stream_idx].height;
    // If scaling isn't required, because the input resolution
    // matches the destination or the input image is empty (e.g.
    // a keyframe request for encoders with internal camera
    // sources), pass the image on directly. Otherwise, we'll
    // scale it to match what the encoder expects (below).
    if ((dst_width == src_width && dst_height == src_height) ||
        input_image.IsZeroSize()) {
      streaminfos_[stream_idx].encoder->Encode(input_image,
                                               codec_specific_info,
                                               &stream_frame_types);
    } else {
      I420VideoFrame dst_frame;
      // Making sure that destination frame is of sufficient size.
      // Aligning stride values based on width.
      dst_frame.CreateEmptyFrame(dst_width, dst_height,
                                 dst_width, (dst_width + 1) / 2,
                                 (dst_width + 1) / 2);
      libyuv::I420Scale(input_image.buffer(kYPlane),
                        input_image.stride(kYPlane),
                        input_image.buffer(kUPlane),
                        input_image.stride(kUPlane),
                        input_image.buffer(kVPlane),
                        input_image.stride(kVPlane),
                        src_width, src_height,
                        dst_frame.buffer(kYPlane),
                        dst_frame.stride(kYPlane),
                        dst_frame.buffer(kUPlane),
                        dst_frame.stride(kUPlane),
                        dst_frame.buffer(kVPlane),
                        dst_frame.stride(kVPlane),
                        dst_width, dst_height,
                        libyuv::kFilterBilinear);
      dst_frame.set_timestamp(input_image.timestamp());
      dst_frame.set_render_time_ms(input_image.render_time_ms());
      streaminfos_[stream_idx].encoder->Encode(dst_frame,
                                               codec_specific_info,
                                               &stream_frame_types);
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::SetChannelParameters(uint32_t packet_loss,
                                                  int64_t rtt) {
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    streaminfos_[stream_idx].encoder->SetChannelParameters(packet_loss, rtt);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::SetRates(uint32_t new_bitrate_kbit,
                                      uint32_t new_framerate) {
  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (new_framerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_.maxBitrate > 0 && new_bitrate_kbit > codec_.maxBitrate) {
    new_bitrate_kbit = codec_.maxBitrate;
  }
  if (new_bitrate_kbit < codec_.minBitrate) {
    new_bitrate_kbit = codec_.minBitrate;
  }
  if (codec_.numberOfSimulcastStreams > 0 &&
      new_bitrate_kbit < codec_.simulcastStream[0].minBitrate) {
    new_bitrate_kbit = codec_.simulcastStream[0].minBitrate;
  }
  codec_.maxFramerate = new_framerate;

  bool send_stream = true;
  uint32_t stream_bitrate = 0;
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    stream_bitrate = GetStreamBitrate(stream_idx,
                                      new_bitrate_kbit,
                                      &send_stream);
    // Need a key frame if we have not sent this stream before.
    if (send_stream && !streaminfos_[stream_idx].send_stream) {
      streaminfos_[stream_idx].key_frame_request = true;
    }
    streaminfos_[stream_idx].send_stream = send_stream;

    // TODO(holmer): This is a temporary hack for screensharing, where we
    // interpret the startBitrate as the encoder target bitrate. This is
    // to allow for a different max bitrate, so if the codec can't meet
    // the target we still allow it to overshoot up to the max before dropping
    // frames. This hack should be improved.
    if (codec_.targetBitrate > 0 &&
        (codec_.codecSpecific.VP8.numberOfTemporalLayers == 2 ||
         codec_.simulcastStream[0].numberOfTemporalLayers == 2)) {
      stream_bitrate = std::min(codec_.maxBitrate, stream_bitrate);
      // TODO(ronghuawu): Can't change max bitrate via the VideoEncoder
      // interface. And VP8EncoderImpl doesn't take negative framerate.
      // max_bitrate = std::min(codec_.maxBitrate, stream_bitrate);
      // new_framerate = -1;
    }

    streaminfos_[stream_idx].encoder->SetRates(stream_bitrate, new_framerate);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t SimulcastEncoderAdapter::Encoded(
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentation) {
  size_t stream_idx = GetStreamIndex(encodedImage);

  CodecSpecificInfo stream_codec_specific = *codecSpecificInfo;
  CodecSpecificInfoVP8* vp8Info = &(stream_codec_specific.codecSpecific.VP8);
  vp8Info->simulcastIdx = stream_idx;

  if (streaminfos_[stream_idx].send_stream) {
    return encoded_complete_callback_->Encoded(encodedImage,
                                               &stream_codec_specific,
                                               fragmentation);
  } else {
    EncodedImage dummy_image;
    // Required in case padding is applied to dropped frames.
    dummy_image._timeStamp = encodedImage._timeStamp;
    dummy_image.capture_time_ms_ = encodedImage.capture_time_ms_;
    dummy_image._encodedWidth = encodedImage._encodedWidth;
    dummy_image._encodedHeight = encodedImage._encodedHeight;
    dummy_image._length = 0;
    dummy_image._frameType = kSkipFrame;
    vp8Info->keyIdx = kNoKeyIdx;
    return encoded_complete_callback_->Encoded(dummy_image,
                                               &stream_codec_specific, NULL);
  }
}

uint32_t SimulcastEncoderAdapter::GetStreamBitrate(int stream_idx,
                                                   uint32_t new_bitrate_kbit,
                                                   bool* send_stream) const {
  if (streaminfos_.size() == 1) {
    *send_stream = true;
    return new_bitrate_kbit;
  }

  // The bitrate needed to start sending this stream is given by the
  // minimum bitrate allowed for encoding this stream, plus the sum target
  // rates of all lower streams.
  uint32_t sum_target_lower_streams =
      SumStreamTargetBitrate(stream_idx, codec_);
  uint32_t bitrate_to_send_this_layer =
      codec_.simulcastStream[stream_idx].minBitrate + sum_target_lower_streams;
  if (new_bitrate_kbit >= bitrate_to_send_this_layer) {
    // We have enough bandwidth to send this stream.
    *send_stream = true;
    // Bitrate for this stream is the new bitrate (|new_bitrate_kbit|) minus the
    // sum target rates of the lower streams, and capped to a maximum bitrate.
    // The maximum cap depends on whether we send the next higher stream.
    // If we will be sending the next higher stream, |max_rate| is given by
    // current stream's |targetBitrate|, otherwise it's capped by |maxBitrate|.
    if (stream_idx < codec_.numberOfSimulcastStreams - 1) {
      unsigned int max_rate = codec_.simulcastStream[stream_idx].maxBitrate;
      if (new_bitrate_kbit >= SumStreamTargetBitrate(stream_idx + 1, codec_) +
          codec_.simulcastStream[stream_idx + 1].minBitrate) {
        max_rate = codec_.simulcastStream[stream_idx].targetBitrate;
      }
      return std::min(new_bitrate_kbit - sum_target_lower_streams, max_rate);
    } else {
        // For the highest stream (highest resolution), the |targetBitRate| and
        // |maxBitrate| are not used. Any excess bitrate (above the targets of
        // all lower streams) is given to this (highest resolution) stream.
        return new_bitrate_kbit - sum_target_lower_streams;
    }
  } else {
    // Not enough bitrate for this stream.
    // Return our max bitrate of |stream_idx| - 1, but we don't send it. We need
    // to keep this resolution coding in order for the multi-encoder to work.
    *send_stream = false;
    return codec_.simulcastStream[stream_idx - 1].maxBitrate;
  }
}

void SimulcastEncoderAdapter::PopulateStreamCodec(
    const webrtc::VideoCodec* inst,
    int stream_index,
    bool highest_resolution_stream,
    webrtc::VideoCodec* stream_codec,
    bool* send_stream) {
  *stream_codec = *inst;

  // Stream specific settings.
  stream_codec->codecSpecific.VP8.numberOfTemporalLayers =
      inst->simulcastStream[stream_index].numberOfTemporalLayers;
  stream_codec->numberOfSimulcastStreams = 0;
  stream_codec->width = inst->simulcastStream[stream_index].width;
  stream_codec->height = inst->simulcastStream[stream_index].height;
  stream_codec->maxBitrate = inst->simulcastStream[stream_index].maxBitrate;
  stream_codec->minBitrate = inst->simulcastStream[stream_index].minBitrate;
  stream_codec->qpMax = inst->simulcastStream[stream_index].qpMax;
  // Settings that are based on stream/resolution.
  if (stream_index == 0) {
    // Settings for lowest spatial resolutions.
    stream_codec->qpMax = kLowestResMaxQp;
  }
  if (!highest_resolution_stream) {
    // For resolutions below CIF, set the codec |complexity| parameter to
    // kComplexityHigher, which maps to cpu_used = -4.
    int pixels_per_frame = stream_codec->width * stream_codec->height;
    if (pixels_per_frame < 352 * 288) {
      stream_codec->codecSpecific.VP8.complexity = webrtc::kComplexityHigher;
    }
    // Turn off denoising for all streams but the highest resolution.
    stream_codec->codecSpecific.VP8.denoisingOn = false;
  }
  // TODO(ronghuawu): what to do with targetBitrate.

  int stream_bitrate = GetStreamBitrate(stream_index,
                                        inst->startBitrate,
                                        send_stream);
  stream_codec->startBitrate = stream_bitrate;
}

size_t SimulcastEncoderAdapter::GetStreamIndex(
    const EncodedImage& encodedImage) {
  uint32_t width = encodedImage._encodedWidth;
  uint32_t height = encodedImage._encodedHeight;
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    if (streaminfos_[stream_idx].width == width &&
        streaminfos_[stream_idx].height == height) {
      return stream_idx;
    }
  }
  // should not be here
  assert(false);
  return 0;
}

bool SimulcastEncoderAdapter::Initialized() const {
  return !streaminfos_.empty();
}

}  // namespace webrtc
