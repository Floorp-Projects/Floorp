/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/simulcast_encoder_adapter.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "media/base/video_common.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace {

const unsigned int kDefaultMinQp = 2;
const unsigned int kDefaultMaxQp = 56;
// Max qp for lowest spatial resolution when doing simulcast.
const unsigned int kLowestResMaxQp = 45;

absl::optional<unsigned int> GetScreenshareBoostedQpValue() {
  std::string experiment_group =
      webrtc::field_trial::FindFullName("WebRTC-BoostedScreenshareQp");
  unsigned int qp;
  if (sscanf(experiment_group.c_str(), "%u", &qp) != 1)
    return absl::nullopt;
  qp = std::min(qp, 63u);
  qp = std::max(qp, 1u);
  return qp;
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

struct StreamDimensions {
  size_t num_active_streams;
  size_t first_active_stream_idx;
};
StreamDimensions ActiveStreams(const webrtc::VideoCodec& codec) {
  size_t num_configured_streams = NumberOfStreams(codec);
  StreamDimensions dimensions{0, 0};
  for (size_t i = 0; i < num_configured_streams; ++i) {
    if (codec.simulcastStream[i].active) {
      ++dimensions.num_active_streams;
      if (dimensions.num_active_streams == 1) {
        dimensions.first_active_stream_idx = i;
      }
    }
  }
  return dimensions;
}

int VerifyCodec(const webrtc::VideoCodec* inst) {
  if (inst == nullptr) {
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
  if (inst->codecType == webrtc::kVideoCodecVP8 &&
      inst->VP8().automaticResizeOn &&
      ActiveStreams(*inst).num_active_streams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool StreamResolutionCompare(const webrtc::SpatialLayer& a,
                             const webrtc::SpatialLayer& b) {
  return std::tie(a.height, a.width, a.maxBitrate, a.maxFramerate) <
         std::tie(b.height, b.width, b.maxBitrate, b.maxFramerate);
}

}  // namespace

namespace webrtc {

SimulcastEncoderAdapter::EncoderContext::EncoderContext(
    SimulcastEncoderAdapter* parent,
    std::unique_ptr<VideoEncoder> encoder,
    std::unique_ptr<FramerateController> framerate_controller,
    int stream_idx,
    uint16_t width,
    uint16_t height,
    bool send_stream)
    : parent_(parent),
      encoder_(std::move(encoder)),
      framerate_controller_(std::move(framerate_controller)),
      stream_idx_(stream_idx),
      width_(width),
      height_(height),
      needs_keyframe_(false),
      send_stream_(send_stream) {
  if (parent) {
    encoder_->RegisterEncodeCompleteCallback(this);
  }
}

SimulcastEncoderAdapter::EncoderContext::EncoderContext(EncoderContext&& rhs)
    : parent_(rhs.parent_),
      encoder_(std::move(rhs.encoder_)),
      framerate_controller_(std::move(rhs.framerate_controller_)),
      stream_idx_(rhs.stream_idx_),
      width_(rhs.width_),
      height_(rhs.height_),
      needs_keyframe_(rhs.needs_keyframe_),
      send_stream_(rhs.send_stream_) {
  if (parent_) {
    encoder_->RegisterEncodeCompleteCallback(this);
  }
}

SimulcastEncoderAdapter::EncoderContext::~EncoderContext() {
  if (encoder_) {
    encoder_->RegisterEncodeCompleteCallback(nullptr);
    encoder_->Release();
  }
}

std::unique_ptr<VideoEncoder>
SimulcastEncoderAdapter::EncoderContext::Release() && {
  encoder_->RegisterEncodeCompleteCallback(nullptr);
  encoder_->Release();
  return std::move(encoder_);
}

void SimulcastEncoderAdapter::EncoderContext::OnKeyframe(Timestamp timestamp) {
  needs_keyframe_ = false;
  if (framerate_controller_) {
    framerate_controller_->AddFrame(timestamp.ms());
  }
}

bool SimulcastEncoderAdapter::EncoderContext::ShouldDropFrame(
    Timestamp timestamp) {
  if (!framerate_controller_) {
    return false;
  }

  if (framerate_controller_->DropFrame(timestamp.ms())) {
    return true;
  }
  framerate_controller_->AddFrame(timestamp.ms());
  return false;
}

EncodedImageCallback::Result
SimulcastEncoderAdapter::EncoderContext::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info) {
  RTC_CHECK(parent_);  // If null, this method should never be called.
  return parent_->OnEncodedImage(stream_idx_, encoded_image,
                                 codec_specific_info);
}

void SimulcastEncoderAdapter::EncoderContext::OnDroppedFrame(
    DropReason /*reason*/) {
  RTC_CHECK(parent_);  // If null, this method should never be called.
  parent_->OnDroppedFrame(stream_idx_);
}

SimulcastEncoderAdapter::SimulcastEncoderAdapter(VideoEncoderFactory* factory,
                                                 const SdpVideoFormat& format)
    : SimulcastEncoderAdapter(factory, nullptr, format) {}

SimulcastEncoderAdapter::SimulcastEncoderAdapter(
    VideoEncoderFactory* primary_factory,
    VideoEncoderFactory* fallback_factory,
    const SdpVideoFormat& format)
    : inited_(0),
      primary_encoder_factory_(primary_factory),
      fallback_encoder_factory_(fallback_factory),
      video_format_(format),
      encoded_complete_callback_(nullptr),
      first_active_stream_idx_(0),
      num_active_streams_(0),
      experimental_boosted_screenshare_qp_(GetScreenshareBoostedQpValue()),
      boost_base_layer_quality_(RateControlSettings::ParseFromFieldTrials()
                                    .Vp8BoostBaseLayerQuality()),
      prefer_temporal_support_on_base_layer_(field_trial::IsEnabled(
          "WebRTC-Video-PreferTemporalSupportOnBaseLayer")) {
  RTC_DCHECK(primary_factory);

  ParseFieldTrial({&requested_resolution_alignment_override_,
                   &apply_alignment_to_all_simulcast_layers_override_},
                  field_trial::FindFullName(
                      "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride"));

  // The adapter is typically created on the worker thread, but operated on
  // the encoder task queue.
  encoder_queue_.Detach();
}

SimulcastEncoderAdapter::~SimulcastEncoderAdapter() {
  RTC_DCHECK(!Initialized());
  DestroyStoredEncoders();
}

void SimulcastEncoderAdapter::SetFecControllerOverride(
    FecControllerOverride* /*fec_controller_override*/) {
  // Ignored.
}

int SimulcastEncoderAdapter::Release() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  while (!encoder_contexts_.empty()) {
    // Move the encoder instances and put it on the |stored_encoders_| where it
    // it may possibly be reused from (ordering does not matter).
    stored_encoders_.push(std::move(encoder_contexts_.back()).Release());
    encoder_contexts_.pop_back();
  }

  num_active_streams_ = 0;
  first_active_stream_idx_ = 0;

  // It's legal to move the encoder to another queue now.
  encoder_queue_.Detach();

  rtc::AtomicOps::ReleaseStore(&inited_, 0);

  return WEBRTC_VIDEO_CODEC_OK;
}

// TODO(eladalon): s/inst/codec_settings/g.
int SimulcastEncoderAdapter::InitEncode(
    const VideoCodec* inst,
    const VideoEncoder::Settings& settings) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  if (settings.number_of_cores < 1) {
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
  RTC_DCHECK_LE(number_of_streams, kMaxSimulcastStreams);
  bool doing_simulcast_using_adapter = (number_of_streams > 1);
  auto active_streams = ActiveStreams(*inst);
  num_active_streams_ = active_streams.num_active_streams;
  first_active_stream_idx_ = active_streams.first_active_stream_idx;

  codec_ = *inst;
  std::unique_ptr<VideoBitrateAllocator> rate_allocator =
      std::make_unique<SimulcastRateAllocator>(codec_);

  VideoBitrateAllocation allocation =
      rate_allocator->Allocate(VideoBitrateAllocationParameters(
          codec_.startBitrate * 1000, codec_.maxFramerate));
  std::vector<uint32_t> start_bitrates;
  for (int i = 0; i < kMaxSimulcastStreams; ++i) {
    uint32_t stream_bitrate = allocation.GetSpatialLayerSum(i) / 1000;
    start_bitrates.push_back(stream_bitrate);
  }

  // Create |number_of_streams| of encoder instances and init them.
  auto spatial_layers =
      rtc::ArrayView<SpatialLayer>(codec_.simulcastStream, number_of_streams);
  const auto minmax =
      absl::c_minmax_element(spatial_layers, StreamResolutionCompare);
  const auto lowest_resolution_stream_index =
      minmax.first - spatial_layers.begin();
  const auto highest_resolution_stream_index =
      minmax.second - spatial_layers.begin();

  RTC_DCHECK_LT(lowest_resolution_stream_index, number_of_streams);
  RTC_DCHECK_LT(highest_resolution_stream_index, number_of_streams);

  for (int i = 0; i < number_of_streams; ++i) {
    // If an existing encoder instance exists, reuse it.
    // TODO(brandtr): Set initial RTP state (e.g., picture_id/tl0_pic_idx) here,
    // when we start storing that state outside the encoder wrappers.
    std::unique_ptr<VideoEncoder> encoder;
    if (!stored_encoders_.empty()) {
      encoder = std::move(stored_encoders_.top());
      stored_encoders_.pop();
    } else {
      encoder = primary_encoder_factory_->CreateVideoEncoder(video_format_);
      if (fallback_encoder_factory_ != nullptr) {
        encoder = CreateVideoEncoderSoftwareFallbackWrapper(
            fallback_encoder_factory_->CreateVideoEncoder(video_format_),
            std::move(encoder),
            i == lowest_resolution_stream_index &&
                prefer_temporal_support_on_base_layer_);
      }
    }

    bool encoder_initialized = false;
    if (doing_simulcast_using_adapter && i == 0 &&
        encoder->GetEncoderInfo().supports_simulcast) {
      ret = encoder->InitEncode(&codec_, settings);
      if (ret < 0) {
        encoder->Release();
      } else {
        doing_simulcast_using_adapter = false;
        number_of_streams = 1;
        encoder_initialized = true;
      }
    }

    VideoCodec stream_codec;
    uint32_t start_bitrate_kbps = start_bitrates[i];
    const bool send_stream = doing_simulcast_using_adapter
                                 ? start_bitrate_kbps > 0
                                 : num_active_streams_ > 0;
    if (!doing_simulcast_using_adapter) {
      stream_codec = codec_;
      stream_codec.numberOfSimulcastStreams =
          std::max<uint8_t>(1, stream_codec.numberOfSimulcastStreams);
    } else {
      // Cap start bitrate to the min bitrate in order to avoid strange codec
      // behavior. Since sending will be false, this should not matter.
      StreamResolution stream_resolution =
          i == highest_resolution_stream_index
              ? StreamResolution::HIGHEST
              : i == lowest_resolution_stream_index ? StreamResolution::LOWEST
                                                    : StreamResolution::OTHER;

      start_bitrate_kbps =
          std::max(spatial_layers[i].minBitrate, start_bitrate_kbps);
      PopulateStreamCodec(codec_, i, start_bitrate_kbps, stream_resolution,
                          &stream_codec);
    }

    // TODO(ronghuawu): Remove once this is handled in LibvpxVp8Encoder.
    if (stream_codec.qpMax < kDefaultMinQp) {
      stream_codec.qpMax = kDefaultMaxQp;
    }

    if (!encoder_initialized) {
      ret = encoder->InitEncode(&stream_codec, settings);
      if (ret < 0) {
        // Explicitly destroy the current encoder; because we haven't registered
        // a StreamInfo for it yet, Release won't do anything about it.
        encoder.reset();
        Release();
        return ret;
      }
    }

    if (!doing_simulcast_using_adapter) {
      // Without simulcast, let the encoder call callbacks and do frame
      // dropping directly, without delegating to this adapter.
      encoder->RegisterEncodeCompleteCallback(encoded_complete_callback_);
      encoder_contexts_.emplace_back(
          /*parent=*/nullptr, std::move(encoder),
          /*framerate_controller=*/nullptr, /*stream_idx=*/0,
          stream_codec.width, stream_codec.height, send_stream);
      break;
    }
    encoder_contexts_.emplace_back(
        this, std::move(encoder),
        std::make_unique<FramerateController>(stream_codec.maxFramerate),
        /*stream_idx=*/i, stream_codec.width, stream_codec.height, send_stream);
  }

  // To save memory, don't store encoders that we don't use.
  DestroyStoredEncoders();

  rtc::AtomicOps::ReleaseStore(&inited_, 1);

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::Encode(
    const VideoFrame& input_image,
    const std::vector<VideoFrameType>* frame_types) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoded_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (requested_resolution_alignment_override_) {
    const int alignment = *requested_resolution_alignment_override_;
    if (input_image.width() % alignment != 0 ||
        input_image.height() % alignment != 0) {
      RTC_LOG(LS_WARNING) << "Frame " << input_image.width() << "x"
                          << input_image.height() << " not divisible by "
                          << alignment;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (apply_alignment_to_all_simulcast_layers_override_.Get()) {
      for (const auto& layer : encoder_contexts_) {
        if (layer.width() % alignment != 0 || layer.height() % alignment != 0) {
          RTC_LOG(LS_WARNING)
              << "Codec " << layer.width() << "x" << layer.height()
              << " not divisible by " << alignment;
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
      }
    }
  }

  // All active streams should generate a key frame if
  // a key frame is requested by any stream.
  bool send_key_frame = false;
  if (frame_types) {
    for (size_t i = 0; i < frame_types->size(); ++i) {
      if (frame_types->at(i) == VideoFrameType::kVideoFrameKey) {
        send_key_frame = true;
        break;
      }
    }
  }

  for (const auto& layer : encoder_contexts_) {
    if (layer.needs_keyframe()) {
      send_key_frame = true;
      break;
    }
  }

  // Temporary thay may hold the result of texture to i420 buffer conversion.
  rtc::scoped_refptr<VideoFrameBuffer> src_buffer;
  int src_width = input_image.width();
  int src_height = input_image.height();

  for (auto& layer : encoder_contexts_) {
    // Don't encode frames in resolutions that we don't intend to send.
    if (!layer.send_stream()) {
      continue;
    }

    // Convert timestamp from RTP 90kHz clock.
    const Timestamp frame_timestamp =
        Timestamp::Micros((1000 * input_image.timestamp()) / 90);

    // If adapter is passed through and only one sw encoder does simulcast,
    // frame types for all streams should be passed to the encoder unchanged.
    // Otherwise a single per-encoder frame type is passed.
    std::vector<VideoFrameType> stream_frame_types(
        encoder_contexts_.size() == 1 ? NumberOfStreams(codec_) : 1);
    if (send_key_frame) {
      std::fill(stream_frame_types.begin(), stream_frame_types.end(),
                VideoFrameType::kVideoFrameKey);
      layer.OnKeyframe(frame_timestamp);
    } else {
      if (layer.ShouldDropFrame(frame_timestamp)) {
        continue;
      }
      std::fill(stream_frame_types.begin(), stream_frame_types.end(),
                VideoFrameType::kVideoFrameDelta);
    }

    // If scaling isn't required, because the input resolution
    // matches the destination or the input image is empty (e.g.
    // a keyframe request for encoders with internal camera
    // sources) or the source image has a native handle, pass the image on
    // directly. Otherwise, we'll scale it to match what the encoder expects
    // (below).
    // For texture frames, the underlying encoder is expected to be able to
    // correctly sample/scale the source texture.
    // TODO(perkj): ensure that works going forward, and figure out how this
    // affects webrtc:5683.
    if ((layer.width() == src_width && layer.height() == src_height) ||
        (input_image.video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kNative &&
         layer.encoder().GetEncoderInfo().supports_native_handle)) {
      int ret = layer.encoder().Encode(input_image, &stream_frame_types);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
      }
    } else {
      if (src_buffer == nullptr) {
        src_buffer = input_image.video_frame_buffer();
      }
      rtc::scoped_refptr<VideoFrameBuffer> dst_buffer =
          src_buffer->Scale(layer.width(), layer.height());
      if (!dst_buffer) {
        RTC_LOG(LS_ERROR) << "Failed to scale video frame";
        return WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
      }

      // UpdateRect is not propagated to lower simulcast layers currently.
      // TODO(ilnik): Consider scaling UpdateRect together with the buffer.
      VideoFrame frame(input_image);
      frame.set_video_frame_buffer(dst_buffer);
      frame.set_rotation(webrtc::kVideoRotation_0);
      frame.set_update_rect(
          VideoFrame::UpdateRect{0, 0, frame.width(), frame.height()});
      int ret = layer.encoder().Encode(frame, &stream_frame_types);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
      }
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  encoded_complete_callback_ = callback;
  if (encoder_contexts_.size() == 1) {
    encoder_contexts_.front().encoder().RegisterEncodeCompleteCallback(
        callback);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void SimulcastEncoderAdapter::SetRates(
    const RateControlParameters& parameters) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  if (!Initialized()) {
    RTC_LOG(LS_WARNING) << "SetRates while not initialized";
    return;
  }

  if (parameters.framerate_fps < 1.0) {
    RTC_LOG(LS_WARNING) << "Invalid framerate: " << parameters.framerate_fps;
    return;
  }

  codec_.maxFramerate = static_cast<uint32_t>(parameters.framerate_fps + 0.5);

  if (encoder_contexts_.size() == 1) {
    // Not doing simulcast.
    encoder_contexts_.front().encoder().SetRates(parameters);
    return;
  }

  num_active_streams_ = 0;
  first_active_stream_idx_ = 0;
  for (size_t stream_idx = 0; stream_idx < encoder_contexts_.size();
       ++stream_idx) {
    EncoderContext& layer = encoder_contexts_[stream_idx];
    uint32_t stream_bitrate_kbps =
        parameters.bitrate.GetSpatialLayerSum(stream_idx) / 1000;
    if (stream_bitrate_kbps > 0) {
      if (num_active_streams_ == 0) {
        first_active_stream_idx_ = stream_idx;
      }
      ++num_active_streams_;
    }

    // Need a key frame if we have not sent this stream before.
    if (stream_bitrate_kbps > 0 && !layer.send_stream()) {
      layer.set_keyframe_needed();
    }
    layer.set_send_stream(stream_bitrate_kbps > 0);

    // Slice the temporal layers out of the full allocation and pass it on to
    // the encoder handling the current simulcast stream.
    RateControlParameters stream_parameters = parameters;
    stream_parameters.bitrate = VideoBitrateAllocation();
    for (int i = 0; i < kMaxTemporalStreams; ++i) {
      if (parameters.bitrate.HasBitrate(stream_idx, i)) {
        stream_parameters.bitrate.SetBitrate(
            0, i, parameters.bitrate.GetBitrate(stream_idx, i));
      }
    }

    // Assign link allocation proportionally to spatial layer allocation.
    if (!parameters.bandwidth_allocation.IsZero() &&
        parameters.bitrate.get_sum_bps() > 0) {
      stream_parameters.bandwidth_allocation =
          DataRate::BitsPerSec((parameters.bandwidth_allocation.bps() *
                                stream_parameters.bitrate.get_sum_bps()) /
                               parameters.bitrate.get_sum_bps());
      // Make sure we don't allocate bandwidth lower than target bitrate.
      if (stream_parameters.bandwidth_allocation.bps() <
          stream_parameters.bitrate.get_sum_bps()) {
        stream_parameters.bandwidth_allocation =
            DataRate::BitsPerSec(stream_parameters.bitrate.get_sum_bps());
      }
    }

    stream_parameters.framerate_fps =
        std::min<double>(parameters.framerate_fps,
                         layer.target_fps().value_or(parameters.framerate_fps));

    layer.encoder().SetRates(stream_parameters);
  }
}

void SimulcastEncoderAdapter::OnPacketLossRateUpdate(float packet_loss_rate) {
  for (auto& c : encoder_contexts_) {
    c.encoder().OnPacketLossRateUpdate(packet_loss_rate);
  }
}

void SimulcastEncoderAdapter::OnRttUpdate(int64_t rtt_ms) {
  for (auto& c : encoder_contexts_) {
    c.encoder().OnRttUpdate(rtt_ms);
  }
}

void SimulcastEncoderAdapter::OnLossNotification(
    const LossNotification& loss_notification) {
  for (auto& c : encoder_contexts_) {
    c.encoder().OnLossNotification(loss_notification);
  }
}

// TODO(brandtr): Add task checker to this member function, when all encoder
// callbacks are coming in on the encoder queue.
EncodedImageCallback::Result SimulcastEncoderAdapter::OnEncodedImage(
    size_t stream_idx,
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo) {
  EncodedImage stream_image(encodedImage);
  CodecSpecificInfo stream_codec_specific = *codecSpecificInfo;

  stream_image.SetSpatialIndex(stream_idx);

  return encoded_complete_callback_->OnEncodedImage(stream_image,
                                                    &stream_codec_specific);
}

void SimulcastEncoderAdapter::OnDroppedFrame(size_t stream_idx) {
  // Not yet implemented.
}

void SimulcastEncoderAdapter::PopulateStreamCodec(
    const webrtc::VideoCodec& inst,
    int stream_index,
    uint32_t start_bitrate_kbps,
    StreamResolution stream_resolution,
    webrtc::VideoCodec* stream_codec) {
  *stream_codec = inst;

  // Stream specific simulcast settings.
  const SpatialLayer* spatial_layers = inst.simulcastStream;

  stream_codec->numberOfSimulcastStreams = 0;
  stream_codec->width = spatial_layers[stream_index].width;
  stream_codec->height = spatial_layers[stream_index].height;
  stream_codec->maxBitrate = spatial_layers[stream_index].maxBitrate;
  stream_codec->minBitrate = spatial_layers[stream_index].minBitrate;
  stream_codec->maxFramerate = spatial_layers[stream_index].maxFramerate;
  stream_codec->qpMax = spatial_layers[stream_index].qpMax;
  stream_codec->active = spatial_layers[stream_index].active;
  // Settings that are based on stream/resolution.
  if (stream_resolution == StreamResolution::LOWEST) {
    // Settings for lowest spatial resolutions.
    if (inst.mode == VideoCodecMode::kScreensharing) {
      if (experimental_boosted_screenshare_qp_) {
        stream_codec->qpMax = *experimental_boosted_screenshare_qp_;
      }
    } else if (boost_base_layer_quality_) {
      stream_codec->qpMax = kLowestResMaxQp;
    }
  }
  if (inst.codecType == webrtc::kVideoCodecVP8) {
    stream_codec->VP8()->numberOfTemporalLayers =
        spatial_layers[stream_index].numberOfTemporalLayers;
    if (stream_resolution != StreamResolution::HIGHEST) {
      // For resolutions below CIF, set the codec |complexity| parameter to
      // kComplexityHigher, which maps to cpu_used = -4.
      int pixels_per_frame = stream_codec->width * stream_codec->height;
      if (pixels_per_frame < 352 * 288) {
        stream_codec->VP8()->complexity =
            webrtc::VideoCodecComplexity::kComplexityHigher;
      }
      // Turn off denoising for all streams but the highest resolution.
      stream_codec->VP8()->denoisingOn = false;
    }
  } else if (inst.codecType == webrtc::kVideoCodecH264) {
    stream_codec->H264()->numberOfTemporalLayers =
        spatial_layers[stream_index].numberOfTemporalLayers;
  }

  stream_codec->startBitrate = start_bitrate_kbps;

  // Legacy screenshare mode is only enabled for the first simulcast layer
  stream_codec->legacy_conference_mode =
      inst.legacy_conference_mode && stream_index == 0;
}

bool SimulcastEncoderAdapter::Initialized() const {
  return rtc::AtomicOps::AcquireLoad(&inited_) == 1;
}

void SimulcastEncoderAdapter::DestroyStoredEncoders() {
  while (!stored_encoders_.empty()) {
    stored_encoders_.pop();
  }
}

void SimulcastEncoderAdapter::OverrideFromFieldTrial(
    VideoEncoder::EncoderInfo* info) const {
  if (requested_resolution_alignment_override_) {
    info->requested_resolution_alignment =
        *requested_resolution_alignment_override_;
    info->apply_alignment_to_all_simulcast_layers =
        apply_alignment_to_all_simulcast_layers_override_.Get();
  }
}

VideoEncoder::EncoderInfo SimulcastEncoderAdapter::GetEncoderInfo() const {
  if (encoder_contexts_.size() == 1) {
    // Not using simulcast adapting functionality, just pass through.
    VideoEncoder::EncoderInfo info =
        encoder_contexts_.front().encoder().GetEncoderInfo();
    OverrideFromFieldTrial(&info);
    return info;
  }

  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.implementation_name = "SimulcastEncoderAdapter";
  encoder_info.requested_resolution_alignment = 1;
  encoder_info.apply_alignment_to_all_simulcast_layers = false;
  encoder_info.supports_native_handle = true;
  encoder_info.scaling_settings.thresholds = absl::nullopt;
  if (encoder_contexts_.empty()) {
    OverrideFromFieldTrial(&encoder_info);
    return encoder_info;
  }

  encoder_info.scaling_settings = VideoEncoder::ScalingSettings::kOff;
  auto active_streams = ActiveStreams(codec_);

  for (size_t i = 0; i < encoder_contexts_.size(); ++i) {
    VideoEncoder::EncoderInfo encoder_impl_info =
        encoder_contexts_[i].encoder().GetEncoderInfo();

    if (i == 0) {
      // Encoder name indicates names of all sub-encoders.
      encoder_info.implementation_name += " (";
      encoder_info.implementation_name += encoder_impl_info.implementation_name;

      encoder_info.supports_native_handle =
          encoder_impl_info.supports_native_handle;
      encoder_info.has_trusted_rate_controller =
          encoder_impl_info.has_trusted_rate_controller;
      encoder_info.is_hardware_accelerated =
          encoder_impl_info.is_hardware_accelerated;
      encoder_info.has_internal_source = encoder_impl_info.has_internal_source;
    } else {
      encoder_info.implementation_name += ", ";
      encoder_info.implementation_name += encoder_impl_info.implementation_name;

      // Native handle supported if any encoder supports it.
      encoder_info.supports_native_handle |=
          encoder_impl_info.supports_native_handle;

      // Trusted rate controller only if all encoders have it.
      encoder_info.has_trusted_rate_controller &=
          encoder_impl_info.has_trusted_rate_controller;

      // Uses hardware support if any of the encoders uses it.
      // For example, if we are having issues with down-scaling due to
      // pipelining delay in HW encoders we need higher encoder usage
      // thresholds in CPU adaptation.
      encoder_info.is_hardware_accelerated |=
          encoder_impl_info.is_hardware_accelerated;

      // Has internal source only if all encoders have it.
      encoder_info.has_internal_source &= encoder_impl_info.has_internal_source;
    }
    encoder_info.fps_allocation[i] = encoder_impl_info.fps_allocation[0];
    encoder_info.requested_resolution_alignment = cricket::LeastCommonMultiple(
        encoder_info.requested_resolution_alignment,
        encoder_impl_info.requested_resolution_alignment);
    if (encoder_impl_info.apply_alignment_to_all_simulcast_layers) {
      encoder_info.apply_alignment_to_all_simulcast_layers = true;
    }
    if (active_streams.num_active_streams == 1 &&
        codec_.simulcastStream[i].active) {
      encoder_info.scaling_settings = encoder_impl_info.scaling_settings;
    }
  }
  encoder_info.implementation_name += ")";

  OverrideFromFieldTrial(&encoder_info);

  return encoder_info;
}

}  // namespace webrtc
