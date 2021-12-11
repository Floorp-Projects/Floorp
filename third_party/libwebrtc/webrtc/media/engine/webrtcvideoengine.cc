/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/webrtcvideoengine.h"

#include <stdio.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "api/video/i420_buffer.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/call.h"
#include "common_video/h264/profile_level_id.h"
#include "media/engine/constants.h"
#include "media/engine/convert_legacy_video_factory.h"
#include "media/engine/simulcast.h"
#include "media/engine/webrtcmediaengine.h"
#include "media/engine/webrtcvoiceengine.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"

using DegradationPreference = webrtc::VideoSendStream::DegradationPreference;

namespace cricket {

// Hack in order to pass in |receive_stream_id| to legacy clients.
// TODO(magjed): Remove once WebRtcVideoDecoderFactory is deprecated and
// webrtc:7925 is fixed.
class DecoderFactoryAdapter {
 public:
  explicit DecoderFactoryAdapter(
      std::unique_ptr<WebRtcVideoDecoderFactory> external_video_decoder_factory)
      : cricket_decoder_with_params_(new CricketDecoderWithParams(
            std::move(external_video_decoder_factory))),
        decoder_factory_(ConvertVideoDecoderFactory(
            std::unique_ptr<WebRtcVideoDecoderFactory>(
                cricket_decoder_with_params_))) {}

  explicit DecoderFactoryAdapter(
      std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory)
      : cricket_decoder_with_params_(nullptr),
        decoder_factory_(std::move(video_decoder_factory)) {}

  void SetReceiveStreamId(const std::string& receive_stream_id) {
    if (cricket_decoder_with_params_)
      cricket_decoder_with_params_->SetReceiveStreamId(receive_stream_id);
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const {
    return decoder_factory_->GetSupportedFormats();
  }

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) {
    return decoder_factory_->CreateVideoDecoder(format);
  }

 private:
  // WebRtcVideoDecoderFactory implementation that allows to override
  // |receive_stream_id|.
  class CricketDecoderWithParams : public WebRtcVideoDecoderFactory {
   public:
    explicit CricketDecoderWithParams(
        std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory)
        : external_decoder_factory_(std::move(external_decoder_factory)) {}

    void SetReceiveStreamId(const std::string& receive_stream_id) {
      receive_stream_id_ = receive_stream_id;
    }

   private:
    webrtc::VideoDecoder* CreateVideoDecoderWithParams(
        const VideoCodec& codec,
        VideoDecoderParams params) override {
      if (!external_decoder_factory_)
        return nullptr;
      params.receive_stream_id = receive_stream_id_;
      return external_decoder_factory_->CreateVideoDecoderWithParams(codec,
                                                                     params);
    }

    webrtc::VideoDecoder* CreateVideoDecoderWithParams(
        webrtc::VideoCodecType type,
        VideoDecoderParams params) override {
      RTC_NOTREACHED();
      return nullptr;
    }

    void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override {
      if (external_decoder_factory_) {
        external_decoder_factory_->DestroyVideoDecoder(decoder);
      } else {
        delete decoder;
      }
    }

    const std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory_;
    std::string receive_stream_id_;
  };

  // If |cricket_decoder_with_params_| is non-null, it's owned by
  // |decoder_factory_|.
  CricketDecoderWithParams* const cricket_decoder_with_params_;
  std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory_;
};

namespace {

// Video decoder class to be used for unknown codecs. Doesn't support decoding
// but logs messages to LS_ERROR.
class NullVideoDecoder : public webrtc::VideoDecoder {
 public:
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    RTC_LOG(LS_ERROR) << "Can't initialize NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::RTPFragmentationHeader* fragmentation,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    RTC_LOG(LS_ERROR) << "The NullVideoDecoder doesn't support decoding.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    RTC_LOG(LS_ERROR)
        << "Can't register decode complete callback on NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  const char* ImplementationName() const override { return "NullVideoDecoder"; }
};

// If this field trial is enabled, we will enable sending FlexFEC and disable
// sending ULPFEC whenever the former has been negotiated in the SDPs.
bool IsFlexfecFieldTrialEnabled() {
  return webrtc::field_trial::IsEnabled("WebRTC-FlexFEC-03");
}

// If this field trial is enabled, the "flexfec-03" codec will be advertised
// as being supported. This means that "flexfec-03" will appear in the default
// SDP offer, and we therefore need to be ready to receive FlexFEC packets from
// the remote. It also means that FlexFEC SSRCs will be generated by
// MediaSession and added as "a=ssrc:" and "a=ssrc-group:" lines in the local
// SDP.
bool IsFlexfecAdvertisedFieldTrialEnabled() {
  return webrtc::field_trial::IsEnabled("WebRTC-FlexFEC-03-Advertised");
}

void AddDefaultFeedbackParams(VideoCodec* codec) {
  // Don't add any feedback params for RED and ULPFEC.
  if (codec->name == kRedCodecName || codec->name == kUlpfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
  codec->AddFeedbackParam(
      FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
  // Don't add any more feedback params for FLEXFEC.
  if (codec->name == kFlexfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamCcm, kRtcpFbCcmParamFir));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kRtcpFbNackParamPli));
}


// This function will assign dynamic payload types (in the range [96, 127]) to
// the input codecs, and also add ULPFEC, RED, FlexFEC, and associated RTX
// codecs for recognized codecs (VP8, VP9, H264, and RED). It will also add
// default feedback params to the codecs.
std::vector<VideoCodec> AssignPayloadTypesAndDefaultCodecs(
    std::vector<webrtc::SdpVideoFormat> input_formats) {
  if (input_formats.empty())
    return std::vector<VideoCodec>();
  static const int kFirstDynamicPayloadType = 96;
  static const int kLastDynamicPayloadType = 127;
  int payload_type = kFirstDynamicPayloadType;

  input_formats.push_back(webrtc::SdpVideoFormat(kRedCodecName));
  input_formats.push_back(webrtc::SdpVideoFormat(kUlpfecCodecName));

  if (IsFlexfecAdvertisedFieldTrialEnabled()) {
    webrtc::SdpVideoFormat flexfec_format(kFlexfecCodecName);
    // This value is currently arbitrarily set to 10 seconds. (The unit
    // is microseconds.) This parameter MUST be present in the SDP, but
    // we never use the actual value anywhere in our code however.
    // TODO(brandtr): Consider honouring this value in the sender and receiver.
    flexfec_format.parameters = {{kFlexfecFmtpRepairWindow, "10000000"}};
    input_formats.push_back(flexfec_format);
  }

  std::vector<VideoCodec> output_codecs;
  for (const webrtc::SdpVideoFormat& format : input_formats) {
    VideoCodec codec(format);
    codec.id = payload_type;
    AddDefaultFeedbackParams(&codec);
    output_codecs.push_back(codec);

    // Increment payload type.
    ++payload_type;
    if (payload_type > kLastDynamicPayloadType)
      break;

    // Add associated RTX codec for recognized codecs.
    // TODO(deadbeef): Should we add RTX codecs for external codecs whose names
    // we don't recognize?
    if (CodecNamesEq(codec.name, kVp8CodecName) ||
        CodecNamesEq(codec.name, kVp9CodecName) ||
        CodecNamesEq(codec.name, kH264CodecName) ||
        CodecNamesEq(codec.name, kRedCodecName)) {
      output_codecs.push_back(
          VideoCodec::CreateRtxCodec(payload_type, codec.id));

      // Increment payload type.
      ++payload_type;
      if (payload_type > kLastDynamicPayloadType)
        break;
    }
  }
  return output_codecs;
}

std::vector<VideoCodec> AssignPayloadTypesAndDefaultCodecs(
    const webrtc::VideoEncoderFactory* encoder_factory) {
  return encoder_factory ? AssignPayloadTypesAndDefaultCodecs(
                               encoder_factory->GetSupportedFormats())
                         : std::vector<VideoCodec>();
}

static std::string CodecVectorToString(const std::vector<VideoCodec>& codecs) {
  std::stringstream out;
  out << '{';
  for (size_t i = 0; i < codecs.size(); ++i) {
    out << codecs[i].ToString();
    if (i != codecs.size() - 1) {
      out << ", ";
    }
  }
  out << '}';
  return out.str();
}

static bool ValidateCodecFormats(const std::vector<VideoCodec>& codecs) {
  bool has_video = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!codecs[i].ValidateCodecFormat()) {
      return false;
    }
    if (codecs[i].GetCodecType() == VideoCodec::CODEC_VIDEO) {
      has_video = true;
    }
  }
  if (!has_video) {
    RTC_LOG(LS_ERROR) << "Setting codecs without a video codec is invalid: "
                      << CodecVectorToString(codecs);
    return false;
  }
  return true;
}

static bool ValidateStreamParams(const StreamParams& sp) {
  if (sp.ssrcs.empty()) {
    RTC_LOG(LS_ERROR) << "No SSRCs in stream parameters: " << sp.ToString();
    return false;
  }

  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  std::vector<uint32_t> rtx_ssrcs;
  sp.GetFidSsrcs(primary_ssrcs, &rtx_ssrcs);
  for (uint32_t rtx_ssrc : rtx_ssrcs) {
    bool rtx_ssrc_present = false;
    for (uint32_t sp_ssrc : sp.ssrcs) {
      if (sp_ssrc == rtx_ssrc) {
        rtx_ssrc_present = true;
        break;
      }
    }
    if (!rtx_ssrc_present) {
      RTC_LOG(LS_ERROR) << "RTX SSRC '" << rtx_ssrc
                        << "' missing from StreamParams ssrcs: "
                        << sp.ToString();
      return false;
    }
  }
  if (!rtx_ssrcs.empty() && primary_ssrcs.size() != rtx_ssrcs.size()) {
    RTC_LOG(LS_ERROR)
        << "RTX SSRCs exist, but don't cover all SSRCs (unsupported): "
        << sp.ToString();
    return false;
  }

  return true;
}

// Returns true if the given codec is disallowed from doing simulcast.
bool IsCodecBlacklistedForSimulcast(const std::string& codec_name) {
  return CodecNamesEq(codec_name, kH264CodecName) ||
         CodecNamesEq(codec_name, kVp9CodecName);
}

// The selected thresholds for QVGA and VGA corresponded to a QP around 10.
// The change in QP declined above the selected bitrates.
static int GetMaxDefaultVideoBitrateKbps(int width, int height) {
  if (width * height <= 320 * 240) {
    return 600;
  } else if (width * height <= 640 * 480) {
    return 1700;
  } else if (width * height <= 960 * 540) {
    return 2000;
  } else {
    return 2500;
  }
}

bool GetVp9LayersFromFieldTrialGroup(int* num_spatial_layers,
                                     int* num_temporal_layers) {
  std::string group = webrtc::field_trial::FindFullName("WebRTC-SupportVP9SVC");
  if (group.empty())
    return false;

  if (sscanf(group.c_str(), "EnabledByFlag_%dSL%dTL", num_spatial_layers,
             num_temporal_layers) != 2) {
    return false;
  }
  const int kMaxSpatialLayers = 2;
  if (*num_spatial_layers > kMaxSpatialLayers || *num_spatial_layers < 1)
    return false;

  const int kMaxTemporalLayers = 3;
  if (*num_temporal_layers > kMaxTemporalLayers || *num_temporal_layers < 1)
    return false;

  return true;
}

int GetDefaultVp9SpatialLayers() {
  int num_sl;
  int num_tl;
  if (GetVp9LayersFromFieldTrialGroup(&num_sl, &num_tl)) {
    return num_sl;
  }
  return 1;
}

int GetDefaultVp9TemporalLayers() {
  int num_sl;
  int num_tl;
  if (GetVp9LayersFromFieldTrialGroup(&num_sl, &num_tl)) {
    return num_tl;
  }
  return 1;
}

const char kForcedFallbackFieldTrial[] =
    "WebRTC-VP8-Forced-Fallback-Encoder-v2";

rtc::Optional<int> GetFallbackMinBpsFromFieldTrial() {
  if (!webrtc::field_trial::IsEnabled(kForcedFallbackFieldTrial))
    return rtc::nullopt;

  std::string group =
      webrtc::field_trial::FindFullName(kForcedFallbackFieldTrial);
  if (group.empty())
    return rtc::nullopt;

  int min_pixels;
  int max_pixels;
  int min_bps;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d", &min_pixels, &max_pixels,
             &min_bps) != 3) {
    return rtc::nullopt;
  }

  if (min_bps <= 0)
    return rtc::nullopt;

  return min_bps;
}

int GetMinVideoBitrateBps() {
  return GetFallbackMinBpsFromFieldTrial().value_or(kMinVideoBitrateBps);
}
}  // namespace

// This constant is really an on/off, lower-level configurable NACK history
// duration hasn't been implemented.
static const int kNackHistoryMs = 1000;

static const int kDefaultRtcpReceiverReportSsrc = 1;

// Minimum time interval for logging stats.
static const int64_t kStatsLogIntervalMs = 10000;

rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings>
WebRtcVideoChannel::WebRtcVideoSendStream::ConfigureVideoEncoderSettings(
    const VideoCodec& codec) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  bool is_screencast = parameters_.options.is_screencast.value_or(false);
  // No automatic resizing when using simulcast or screencast.
  bool automatic_resize =
      !is_screencast && parameters_.config.rtp.ssrcs.size() == 1;
  bool frame_dropping = !is_screencast;
  bool denoising;
  bool codec_default_denoising = false;
  if (is_screencast) {
    denoising = false;
  } else {
    // Use codec default if video_noise_reduction is unset.
    codec_default_denoising = !parameters_.options.video_noise_reduction;
    denoising = parameters_.options.video_noise_reduction.value_or(false);
  }

  if (CodecNamesEq(codec.name, kH264CodecName)) {
    webrtc::VideoCodecH264 h264_settings =
        webrtc::VideoEncoder::GetDefaultH264Settings();
    h264_settings.frameDroppingOn = frame_dropping;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::H264EncoderSpecificSettings>(h264_settings);
  }
  if (CodecNamesEq(codec.name, kVp8CodecName)) {
    webrtc::VideoCodecVP8 vp8_settings =
        webrtc::VideoEncoder::GetDefaultVp8Settings();
    vp8_settings.automaticResizeOn = automatic_resize;
    // VP8 denoising is enabled by default.
    vp8_settings.denoisingOn = codec_default_denoising ? true : denoising;
    vp8_settings.frameDroppingOn = frame_dropping;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
  }
  if (CodecNamesEq(codec.name, kVp9CodecName)) {
    webrtc::VideoCodecVP9 vp9_settings =
        webrtc::VideoEncoder::GetDefaultVp9Settings();
    if (is_screencast) {
      // TODO(asapersson): Set to 2 for now since there is a DCHECK in
      // VideoSendStream::ReconfigureVideoEncoder.
      vp9_settings.numberOfSpatialLayers = 2;
    } else {
      vp9_settings.numberOfSpatialLayers = GetDefaultVp9SpatialLayers();
    }
    // VP9 denoising is disabled by default.
    vp9_settings.denoisingOn = codec_default_denoising ? true : denoising;
    vp9_settings.frameDroppingOn = frame_dropping;
    vp9_settings.automaticResizeOn = automatic_resize;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
  }
  return nullptr;
}

DefaultUnsignalledSsrcHandler::DefaultUnsignalledSsrcHandler()
    : default_sink_(nullptr) {}

UnsignalledSsrcHandler::Action DefaultUnsignalledSsrcHandler::OnUnsignalledSsrc(
    WebRtcVideoChannel* channel,
    uint32_t ssrc) {
  rtc::Optional<uint32_t> default_recv_ssrc =
      channel->GetDefaultReceiveStreamSsrc();

  if (default_recv_ssrc) {
    RTC_LOG(LS_INFO) << "Destroying old default receive stream for SSRC="
                     << ssrc << ".";
    channel->RemoveRecvStream(*default_recv_ssrc);
  }

  StreamParams sp;
  sp.ssrcs.push_back(ssrc);
  RTC_LOG(LS_INFO) << "Creating default receive stream for SSRC=" << ssrc
                   << ".";
  if (!channel->AddRecvStream(sp, true)) {
    RTC_LOG(LS_WARNING) << "Could not create default receive stream.";
  }

  channel->SetSink(ssrc, default_sink_);
  return kDeliverPacket;
}

rtc::VideoSinkInterface<webrtc::VideoFrame>*
DefaultUnsignalledSsrcHandler::GetDefaultSink() const {
  return default_sink_;
}

void DefaultUnsignalledSsrcHandler::SetDefaultSink(
    WebRtcVideoChannel* channel,
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  default_sink_ = sink;
  rtc::Optional<uint32_t> default_recv_ssrc =
      channel->GetDefaultReceiveStreamSsrc();
  if (default_recv_ssrc) {
    channel->SetSink(*default_recv_ssrc, default_sink_);
  }
}

WebRtcVideoEngine::WebRtcVideoEngine(
    std::unique_ptr<WebRtcVideoEncoderFactory> external_video_encoder_factory,
    std::unique_ptr<WebRtcVideoDecoderFactory> external_video_decoder_factory)
    : decoder_factory_(
          new DecoderFactoryAdapter(std::move(external_video_decoder_factory))),
      encoder_factory_(ConvertVideoEncoderFactory(
          std::move(external_video_encoder_factory))) {
  RTC_LOG(LS_INFO) << "WebRtcVideoEngine::WebRtcVideoEngine()";
}

WebRtcVideoEngine::WebRtcVideoEngine(
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory)
    : decoder_factory_(
          new DecoderFactoryAdapter(std::move(video_decoder_factory))),
      encoder_factory_(std::move(video_encoder_factory)) {
  RTC_LOG(LS_INFO) << "WebRtcVideoEngine::WebRtcVideoEngine()";
}

WebRtcVideoEngine::~WebRtcVideoEngine() {
  RTC_LOG(LS_INFO) << "WebRtcVideoEngine::~WebRtcVideoEngine";
}

WebRtcVideoChannel* WebRtcVideoEngine::CreateChannel(
    webrtc::Call* call,
    const MediaConfig& config,
    const VideoOptions& options) {
  RTC_LOG(LS_INFO) << "CreateChannel. Options: " << options.ToString();
  return new WebRtcVideoChannel(call, config, options, encoder_factory_.get(),
                                decoder_factory_.get());
}

std::vector<VideoCodec> WebRtcVideoEngine::codecs() const {
  return AssignPayloadTypesAndDefaultCodecs(encoder_factory_.get());
}

RtpCapabilities WebRtcVideoEngine::GetCapabilities() const {
  RtpCapabilities capabilities;
  capabilities.header_extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kTimestampOffsetUri,
                           webrtc::RtpExtension::kTimestampOffsetDefaultId));
  capabilities.header_extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kAbsSendTimeUri,
                           webrtc::RtpExtension::kAbsSendTimeDefaultId));
  capabilities.header_extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kVideoRotationUri,
                           webrtc::RtpExtension::kVideoRotationDefaultId));
  capabilities.header_extensions.push_back(webrtc::RtpExtension(
      webrtc::RtpExtension::kTransportSequenceNumberUri,
      webrtc::RtpExtension::kTransportSequenceNumberDefaultId));
  capabilities.header_extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kPlayoutDelayUri,
                           webrtc::RtpExtension::kPlayoutDelayDefaultId));
  capabilities.header_extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kVideoContentTypeUri,
                           webrtc::RtpExtension::kVideoContentTypeDefaultId));
  capabilities.header_extensions.push_back(
        webrtc::RtpExtension(webrtc::RtpExtension::kVideoTimingUri,
                             webrtc::RtpExtension::kVideoTimingDefaultId));
  return capabilities;
}

WebRtcVideoChannel::WebRtcVideoChannel(
    webrtc::Call* call,
    const MediaConfig& config,
    const VideoOptions& options,
    webrtc::VideoEncoderFactory* encoder_factory,
    DecoderFactoryAdapter* decoder_factory)
    : VideoMediaChannel(config),
      call_(call),
      unsignalled_ssrc_handler_(&default_unsignalled_ssrc_handler_),
      video_config_(config.video),
      encoder_factory_(encoder_factory),
      decoder_factory_(decoder_factory),
      default_send_options_(options),
      last_stats_log_ms_(-1) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  rtcp_receiver_report_ssrc_ = kDefaultRtcpReceiverReportSsrc;
  sending_ = false;
  recv_codecs_ =
      MapCodecs(AssignPayloadTypesAndDefaultCodecs(encoder_factory_));
  recv_flexfec_payload_type_ = recv_codecs_.front().flexfec_payload_type;
}

WebRtcVideoChannel::~WebRtcVideoChannel() {
  for (auto& kv : send_streams_)
    delete kv.second;
  for (auto& kv : receive_streams_)
    delete kv.second;
}

rtc::Optional<WebRtcVideoChannel::VideoCodecSettings>
WebRtcVideoChannel::SelectSendVideoCodec(
    const std::vector<VideoCodecSettings>& remote_mapped_codecs) const {
  const std::vector<VideoCodec> local_supported_codecs =
      AssignPayloadTypesAndDefaultCodecs(encoder_factory_);
  // Select the first remote codec that is supported locally.
  for (const VideoCodecSettings& remote_mapped_codec : remote_mapped_codecs) {
    // For H264, we will limit the encode level to the remote offered level
    // regardless if level asymmetry is allowed or not. This is strictly not
    // following the spec in https://tools.ietf.org/html/rfc6184#section-8.2.2
    // since we should limit the encode level to the lower of local and remote
    // level when level asymmetry is not allowed.
    if (FindMatchingCodec(local_supported_codecs, remote_mapped_codec.codec))
      return remote_mapped_codec;
  }
  // No remote codec was supported.
  return rtc::nullopt;
}

bool WebRtcVideoChannel::NonFlexfecReceiveCodecsHaveChanged(
    std::vector<VideoCodecSettings> before,
    std::vector<VideoCodecSettings> after) {
  if (before.size() != after.size()) {
    return true;
  }

  // The receive codec order doesn't matter, so we sort the codecs before
  // comparing. This is necessary because currently the
  // only way to change the send codec is to munge SDP, which causes
  // the receive codec list to change order, which causes the streams
  // to be recreates which causes a "blink" of black video.  In order
  // to support munging the SDP in this way without recreating receive
  // streams, we ignore the order of the received codecs so that
  // changing the order doesn't cause this "blink".
  auto comparison =
      [](const VideoCodecSettings& codec1, const VideoCodecSettings& codec2) {
        return codec1.codec.id > codec2.codec.id;
      };
  std::sort(before.begin(), before.end(), comparison);
  std::sort(after.begin(), after.end(), comparison);

  // Changes in FlexFEC payload type are handled separately in
  // WebRtcVideoChannel::GetChangedRecvParameters, so disregard FlexFEC in the
  // comparison here.
  return !std::equal(before.begin(), before.end(), after.begin(),
                     VideoCodecSettings::EqualsDisregardingFlexfec);
}

bool WebRtcVideoChannel::GetChangedSendParameters(
    const VideoSendParameters& params,
    ChangedSendParameters* changed_params) const {
  if (!ValidateCodecFormats(params.codecs) ||
      !ValidateRtpExtensions(params.extensions)) {
    return false;
  }

  // Select one of the remote codecs that will be used as send codec.
  rtc::Optional<VideoCodecSettings> selected_send_codec =
      SelectSendVideoCodec(MapCodecs(params.codecs));

  if (!selected_send_codec) {
    RTC_LOG(LS_ERROR) << "No video codecs supported.";
    return false;
  }

  // Never enable sending FlexFEC, unless we are in the experiment.
  if (!IsFlexfecFieldTrialEnabled()) {
    if (selected_send_codec->flexfec_payload_type != -1) {
      RTC_LOG(LS_INFO)
          << "Remote supports flexfec-03, but we will not send since "
          << "WebRTC-FlexFEC-03 field trial is not enabled.";
    }
    selected_send_codec->flexfec_payload_type = -1;
  }

  if (!send_codec_ || *selected_send_codec != *send_codec_)
    changed_params->codec = selected_send_codec;

  // Handle RTP header extensions.
  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForVideo, true);
  if (!send_rtp_extensions_ || (*send_rtp_extensions_ != filtered_extensions)) {
    changed_params->rtp_header_extensions =
        rtc::Optional<std::vector<webrtc::RtpExtension>>(filtered_extensions);
  }

  // Handle max bitrate.
  if (params.max_bandwidth_bps != send_params_.max_bandwidth_bps &&
      params.max_bandwidth_bps >= -1) {
    // 0 or -1 uncaps max bitrate.
    // TODO(pbos): Reconsider how 0 should be treated. It is not mentioned as a
    // special value and might very well be used for stopping sending.
    changed_params->max_bandwidth_bps =
        params.max_bandwidth_bps == 0 ? -1 : params.max_bandwidth_bps;
  }

  // Handle conference mode.
  if (params.conference_mode != send_params_.conference_mode) {
    changed_params->conference_mode = params.conference_mode;
  }

  // Handle RTCP mode.
  if (params.rtcp.reduced_size != send_params_.rtcp.reduced_size) {
    changed_params->rtcp_mode = params.rtcp.reduced_size
                                    ? webrtc::RtcpMode::kReducedSize
                                    : webrtc::RtcpMode::kCompound;
  }

  return true;
}

rtc::DiffServCodePoint WebRtcVideoChannel::PreferredDscp() const {
  return rtc::DSCP_AF41;
}

bool WebRtcVideoChannel::SetSendParameters(const VideoSendParameters& params) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetSendParameters");
  RTC_LOG(LS_INFO) << "SetSendParameters: " << params.ToString();
  ChangedSendParameters changed_params;
  if (!GetChangedSendParameters(params, &changed_params)) {
    return false;
  }

  if (changed_params.codec) {
    const VideoCodecSettings& codec_settings = *changed_params.codec;
    send_codec_ = codec_settings;
    RTC_LOG(LS_INFO) << "Using codec: " << codec_settings.codec.ToString();
  }

  if (changed_params.rtp_header_extensions) {
    send_rtp_extensions_ = changed_params.rtp_header_extensions;
  }

  if (changed_params.codec || changed_params.max_bandwidth_bps) {
    if (params.max_bandwidth_bps == -1) {
      // Unset the global max bitrate (max_bitrate_bps) if max_bandwidth_bps is
      // -1, which corresponds to no "b=AS" attribute in SDP. Note that the
      // global max bitrate may be set below in GetBitrateConfigForCodec, from
      // the codec max bitrate.
      // TODO(pbos): This should be reconsidered (codec max bitrate should
      // probably not affect global call max bitrate).
      bitrate_config_.max_bitrate_bps = -1;
    }
    if (send_codec_) {
      // TODO(holmer): Changing the codec parameters shouldn't necessarily mean
      // that we change the min/max of bandwidth estimation. Reevaluate this.
      bitrate_config_ = GetBitrateConfigForCodec(send_codec_->codec);
      if (!changed_params.codec) {
        // If the codec isn't changing, set the start bitrate to -1 which means
        // "unchanged" so that BWE isn't affected.
        bitrate_config_.start_bitrate_bps = -1;
      }
    }
    if (params.max_bandwidth_bps >= 0) {
      // Note that max_bandwidth_bps intentionally takes priority over the
      // bitrate config for the codec. This allows FEC to be applied above the
      // codec target bitrate.
      // TODO(pbos): Figure out whether b=AS means max bitrate for this
      // WebRtcVideoChannel (in which case we're good), or per sender (SSRC),
      // in which case this should not set a Call::BitrateConfig but rather
      // reconfigure all senders.
      bitrate_config_.max_bitrate_bps =
          params.max_bandwidth_bps == 0 ? -1 : params.max_bandwidth_bps;
    }
    call_->SetBitrateConfig(bitrate_config_);
  }

  {
    rtc::CritScope stream_lock(&stream_crit_);
    for (auto& kv : send_streams_) {
      kv.second->SetSendParameters(changed_params);
    }
    if (changed_params.codec || changed_params.rtcp_mode) {
      // Update receive feedback parameters from new codec or RTCP mode.
      RTC_LOG(LS_INFO)
          << "SetFeedbackOptions on all the receive streams because the send "
             "codec or RTCP mode has changed.";
      for (auto& kv : receive_streams_) {
        RTC_DCHECK(kv.second != nullptr);
        kv.second->SetFeedbackParameters(
            HasNack(send_codec_->codec), HasRemb(send_codec_->codec),
            HasTransportCc(send_codec_->codec),
            params.rtcp.reduced_size ? webrtc::RtcpMode::kReducedSize
                                     : webrtc::RtcpMode::kCompound);
      }
    }
  }
  send_params_ = params;
  return true;
}

webrtc::RtpParameters WebRtcVideoChannel::GetRtpSendParameters(
    uint32_t ssrc) const {
  rtc::CritScope stream_lock(&stream_crit_);
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to get RTP send parameters for stream "
                        << "with ssrc " << ssrc << " which doesn't exist.";
    return webrtc::RtpParameters();
  }

  webrtc::RtpParameters rtp_params = it->second->GetRtpParameters();
  // Need to add the common list of codecs to the send stream-specific
  // RTP parameters.
  for (const VideoCodec& codec : send_params_.codecs) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

bool WebRtcVideoChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetRtpSendParameters");
  rtc::CritScope stream_lock(&stream_crit_);
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Attempting to set RTP send parameters for stream "
                      << "with ssrc " << ssrc << " which doesn't exist.";
    return false;
  }

  // TODO(deadbeef): Handle setting parameters with a list of codecs in a
  // different order (which should change the send codec).
  webrtc::RtpParameters current_parameters = GetRtpSendParameters(ssrc);
  if (current_parameters.codecs != parameters.codecs) {
    RTC_LOG(LS_ERROR) << "Using SetParameters to change the set of codecs "
                      << "is not currently supported.";
    return false;
  }

  return it->second->SetRtpParameters(parameters);
}

webrtc::RtpParameters WebRtcVideoChannel::GetRtpReceiveParameters(
    uint32_t ssrc) const {
  webrtc::RtpParameters rtp_params;
  rtc::CritScope stream_lock(&stream_crit_);
  // SSRC of 0 represents an unsignaled receive stream.
  if (ssrc == 0) {
    if (!default_unsignalled_ssrc_handler_.GetDefaultSink()) {
      RTC_LOG(LS_WARNING)
          << "Attempting to get RTP parameters for the default, "
             "unsignaled video receive stream, but not yet "
             "configured to receive such a stream.";
      return rtp_params;
    }
    rtp_params.encodings.emplace_back();
  } else {
    auto it = receive_streams_.find(ssrc);
    if (it == receive_streams_.end()) {
      RTC_LOG(LS_WARNING)
          << "Attempting to get RTP receive parameters for stream "
          << "with SSRC " << ssrc << " which doesn't exist.";
      return webrtc::RtpParameters();
    }
    // TODO(deadbeef): Return stream-specific parameters, beyond just SSRC.
    rtp_params.encodings.emplace_back();
    rtp_params.encodings[0].ssrc = it->second->GetFirstPrimarySsrc();
  }

  // Add codecs, which any stream is prepared to receive.
  for (const VideoCodec& codec : recv_params_.codecs) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

bool WebRtcVideoChannel::SetRtpReceiveParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetRtpReceiveParameters");
  rtc::CritScope stream_lock(&stream_crit_);

  // SSRC of 0 represents an unsignaled receive stream.
  if (ssrc == 0) {
    if (!default_unsignalled_ssrc_handler_.GetDefaultSink()) {
      RTC_LOG(LS_WARNING)
          << "Attempting to set RTP parameters for the default, "
             "unsignaled video receive stream, but not yet "
             "configured to receive such a stream.";
      return false;
    }
  } else {
    auto it = receive_streams_.find(ssrc);
    if (it == receive_streams_.end()) {
      RTC_LOG(LS_WARNING)
          << "Attempting to set RTP receive parameters for stream "
          << "with SSRC " << ssrc << " which doesn't exist.";
      return false;
    }
  }

  webrtc::RtpParameters current_parameters = GetRtpReceiveParameters(ssrc);
  if (current_parameters != parameters) {
    RTC_LOG(LS_ERROR) << "Changing the RTP receive parameters is currently "
                      << "unsupported.";
    return false;
  }
  return true;
}

bool WebRtcVideoChannel::GetChangedRecvParameters(
    const VideoRecvParameters& params,
    ChangedRecvParameters* changed_params) const {
  if (!ValidateCodecFormats(params.codecs) ||
      !ValidateRtpExtensions(params.extensions)) {
    return false;
  }

  // Handle receive codecs.
  const std::vector<VideoCodecSettings> mapped_codecs =
      MapCodecs(params.codecs);
  if (mapped_codecs.empty()) {
    RTC_LOG(LS_ERROR) << "SetRecvParameters called without any video codecs.";
    return false;
  }

  // Verify that every mapped codec is supported locally.
  const std::vector<VideoCodec> local_supported_codecs =
      AssignPayloadTypesAndDefaultCodecs(encoder_factory_);
  for (const VideoCodecSettings& mapped_codec : mapped_codecs) {
    if (!FindMatchingCodec(local_supported_codecs, mapped_codec.codec)) {
      RTC_LOG(LS_ERROR)
          << "SetRecvParameters called with unsupported video codec: "
          << mapped_codec.codec.ToString();
      return false;
    }
  }

  if (NonFlexfecReceiveCodecsHaveChanged(recv_codecs_, mapped_codecs)) {
    changed_params->codec_settings =
        rtc::Optional<std::vector<VideoCodecSettings>>(mapped_codecs);
  }

  // Handle RTP header extensions.
  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForVideo, false);
  if (filtered_extensions != recv_rtp_extensions_) {
    changed_params->rtp_header_extensions =
        rtc::Optional<std::vector<webrtc::RtpExtension>>(filtered_extensions);
  }

  int flexfec_payload_type = mapped_codecs.front().flexfec_payload_type;
  if (flexfec_payload_type != recv_flexfec_payload_type_) {
    changed_params->flexfec_payload_type = flexfec_payload_type;
  }

  return true;
}

bool WebRtcVideoChannel::SetRecvParameters(const VideoRecvParameters& params) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetRecvParameters");
  RTC_LOG(LS_INFO) << "SetRecvParameters: " << params.ToString();
  ChangedRecvParameters changed_params;
  if (!GetChangedRecvParameters(params, &changed_params)) {
    return false;
  }
  if (changed_params.flexfec_payload_type) {
    RTC_LOG(LS_INFO) << "Changing FlexFEC payload type (recv) from "
                     << recv_flexfec_payload_type_ << " to "
                     << *changed_params.flexfec_payload_type;
    recv_flexfec_payload_type_ = *changed_params.flexfec_payload_type;
  }
  if (changed_params.rtp_header_extensions) {
    recv_rtp_extensions_ = *changed_params.rtp_header_extensions;
  }
  if (changed_params.codec_settings) {
    RTC_LOG(LS_INFO) << "Changing recv codecs from "
                     << CodecSettingsVectorToString(recv_codecs_) << " to "
                     << CodecSettingsVectorToString(
                            *changed_params.codec_settings);
    recv_codecs_ = *changed_params.codec_settings;
  }

  {
    rtc::CritScope stream_lock(&stream_crit_);
    for (auto& kv : receive_streams_) {
      kv.second->SetRecvParameters(changed_params);
    }
  }
  recv_params_ = params;
  return true;
}

std::string WebRtcVideoChannel::CodecSettingsVectorToString(
    const std::vector<VideoCodecSettings>& codecs) {
  std::stringstream out;
  out << '{';
  for (size_t i = 0; i < codecs.size(); ++i) {
    out << codecs[i].codec.ToString();
    if (i != codecs.size() - 1) {
      out << ", ";
    }
  }
  out << '}';
  return out.str();
}

bool WebRtcVideoChannel::GetSendCodec(VideoCodec* codec) {
  if (!send_codec_) {
    RTC_LOG(LS_VERBOSE) << "GetSendCodec: No send codec set.";
    return false;
  }
  *codec = send_codec_->codec;
  return true;
}

bool WebRtcVideoChannel::SetSend(bool send) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetSend");
  RTC_LOG(LS_VERBOSE) << "SetSend: " << (send ? "true" : "false");
  if (send && !send_codec_) {
    RTC_LOG(LS_ERROR) << "SetSend(true) called before setting codec.";
    return false;
  }
  {
    rtc::CritScope stream_lock(&stream_crit_);
    for (const auto& kv : send_streams_) {
      kv.second->SetSend(send);
    }
  }
  sending_ = send;
  return true;
}

// TODO(nisse): The enable argument was used for mute logic which has
// been moved to VideoBroadcaster. So remove the argument from this
// method.
bool WebRtcVideoChannel::SetVideoSend(
    uint32_t ssrc,
    bool enable,
    const VideoOptions* options,
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source) {
  TRACE_EVENT0("webrtc", "SetVideoSend");
  RTC_DCHECK(ssrc != 0);
  RTC_LOG(LS_INFO) << "SetVideoSend (ssrc= " << ssrc << ", enable = " << enable
                   << ", options: "
                   << (options ? options->ToString() : "nullptr")
                   << ", source = " << (source ? "(source)" : "nullptr") << ")";

  rtc::CritScope stream_lock(&stream_crit_);
  const auto& kv = send_streams_.find(ssrc);
  if (kv == send_streams_.end()) {
    // Allow unknown ssrc only if source is null.
    RTC_CHECK(source == nullptr);
    RTC_LOG(LS_ERROR) << "No sending stream on ssrc " << ssrc;
    return false;
  }

  return kv->second->SetVideoSend(enable, options, source);
}

bool WebRtcVideoChannel::ValidateSendSsrcAvailability(
    const StreamParams& sp) const {
  for (uint32_t ssrc : sp.ssrcs) {
    if (send_ssrcs_.find(ssrc) != send_ssrcs_.end()) {
      RTC_LOG(LS_ERROR) << "Send stream with SSRC '" << ssrc
                        << "' already exists.";
      return false;
    }
  }
  return true;
}

bool WebRtcVideoChannel::ValidateReceiveSsrcAvailability(
    const StreamParams& sp) const {
  for (uint32_t ssrc : sp.ssrcs) {
    if (receive_ssrcs_.find(ssrc) != receive_ssrcs_.end()) {
      RTC_LOG(LS_ERROR) << "Receive stream with SSRC '" << ssrc
                        << "' already exists.";
      return false;
    }
  }
  return true;
}

bool WebRtcVideoChannel::AddSendStream(const StreamParams& sp) {
  RTC_LOG(LS_INFO) << "AddSendStream: " << sp.ToString();
  if (!ValidateStreamParams(sp))
    return false;

  rtc::CritScope stream_lock(&stream_crit_);

  if (!ValidateSendSsrcAvailability(sp))
    return false;

  for (uint32_t used_ssrc : sp.ssrcs)
    send_ssrcs_.insert(used_ssrc);

  webrtc::VideoSendStream::Config config(this);
  config.suspend_below_min_bitrate = video_config_.suspend_below_min_bitrate;
  config.periodic_alr_bandwidth_probing =
      video_config_.periodic_alr_bandwidth_probing;
  WebRtcVideoSendStream* stream = new WebRtcVideoSendStream(
      call_, sp, std::move(config), default_send_options_, encoder_factory_,
      video_config_.enable_cpu_overuse_detection,
      bitrate_config_.max_bitrate_bps, send_codec_, send_rtp_extensions_,
      send_params_);

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(ssrc != 0);
  send_streams_[ssrc] = stream;

  if (rtcp_receiver_report_ssrc_ == kDefaultRtcpReceiverReportSsrc) {
    rtcp_receiver_report_ssrc_ = ssrc;
    RTC_LOG(LS_INFO)
        << "SetLocalSsrc on all the receive streams because we added "
           "a send stream.";
    for (auto& kv : receive_streams_)
      kv.second->SetLocalSsrc(ssrc);
  }
  if (sending_) {
    stream->SetSend(true);
  }

  return true;
}

bool WebRtcVideoChannel::RemoveSendStream(uint32_t ssrc) {
  RTC_LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  WebRtcVideoSendStream* removed_stream;
  {
    rtc::CritScope stream_lock(&stream_crit_);
    std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
        send_streams_.find(ssrc);
    if (it == send_streams_.end()) {
      return false;
    }

    for (uint32_t old_ssrc : it->second->GetSsrcs())
      send_ssrcs_.erase(old_ssrc);

    removed_stream = it->second;
    send_streams_.erase(it);

    // Switch receiver report SSRCs, the one in use is no longer valid.
    if (rtcp_receiver_report_ssrc_ == ssrc) {
      rtcp_receiver_report_ssrc_ = send_streams_.empty()
                                       ? kDefaultRtcpReceiverReportSsrc
                                       : send_streams_.begin()->first;
      RTC_LOG(LS_INFO) << "SetLocalSsrc on all the receive streams because the "
                          "previous local SSRC was removed.";

      for (auto& kv : receive_streams_) {
        kv.second->SetLocalSsrc(rtcp_receiver_report_ssrc_);
      }
    }
  }

  delete removed_stream;

  return true;
}

void WebRtcVideoChannel::DeleteReceiveStream(
    WebRtcVideoChannel::WebRtcVideoReceiveStream* stream) {
  for (uint32_t old_ssrc : stream->GetSsrcs())
    receive_ssrcs_.erase(old_ssrc);
  delete stream;
}

bool WebRtcVideoChannel::AddRecvStream(const StreamParams& sp) {
  return AddRecvStream(sp, false);
}

bool WebRtcVideoChannel::AddRecvStream(const StreamParams& sp,
                                       bool default_stream) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  RTC_LOG(LS_INFO) << "AddRecvStream"
                   << (default_stream ? " (default stream)" : "") << ": "
                   << sp.ToString();
  if (!ValidateStreamParams(sp))
    return false;

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(ssrc != 0);  // TODO(pbos): Is this ever valid?

  rtc::CritScope stream_lock(&stream_crit_);
  // Remove running stream if this was a default stream.
  const auto& prev_stream = receive_streams_.find(ssrc);
  if (prev_stream != receive_streams_.end()) {
    if (default_stream || !prev_stream->second->IsDefaultStream()) {
      RTC_LOG(LS_ERROR) << "Receive stream for SSRC '" << ssrc
                        << "' already exists.";
      return false;
    }
    DeleteReceiveStream(prev_stream->second);
    receive_streams_.erase(prev_stream);
  }

  if (!ValidateReceiveSsrcAvailability(sp))
    return false;

  for (uint32_t used_ssrc : sp.ssrcs)
    receive_ssrcs_.insert(used_ssrc);

  webrtc::VideoReceiveStream::Config config(this);
  webrtc::FlexfecReceiveStream::Config flexfec_config(this);
  ConfigureReceiverRtp(&config, &flexfec_config, sp);

  config.disable_prerenderer_smoothing =
      video_config_.disable_prerenderer_smoothing;
  config.sync_group = sp.sync_label;

  receive_streams_[ssrc] = new WebRtcVideoReceiveStream(
      call_, sp, std::move(config), decoder_factory_, default_stream,
      recv_codecs_, flexfec_config);

  return true;
}

void WebRtcVideoChannel::ConfigureReceiverRtp(
    webrtc::VideoReceiveStream::Config* config,
    webrtc::FlexfecReceiveStream::Config* flexfec_config,
    const StreamParams& sp) const {
  uint32_t ssrc = sp.first_ssrc();

  config->rtp.remote_ssrc = ssrc;
  config->rtp.local_ssrc = rtcp_receiver_report_ssrc_;

  // TODO(pbos): This protection is against setting the same local ssrc as
  // remote which is not permitted by the lower-level API. RTCP requires a
  // corresponding sender SSRC. Figure out what to do when we don't have
  // (receive-only) or know a good local SSRC.
  if (config->rtp.remote_ssrc == config->rtp.local_ssrc) {
    if (config->rtp.local_ssrc != kDefaultRtcpReceiverReportSsrc) {
      config->rtp.local_ssrc = kDefaultRtcpReceiverReportSsrc;
    } else {
      config->rtp.local_ssrc = kDefaultRtcpReceiverReportSsrc + 1;
    }
  }

  // Whether or not the receive stream sends reduced size RTCP is determined
  // by the send params.
  // TODO(deadbeef): Once we change "send_params" to "sender_params" and
  // "recv_params" to "receiver_params", we should get this out of
  // receiver_params_.
  config->rtp.rtcp_mode = send_params_.rtcp.reduced_size
                              ? webrtc::RtcpMode::kReducedSize
                              : webrtc::RtcpMode::kCompound;

  config->rtp.remb = send_codec_ ? HasRemb(send_codec_->codec) : false;
  config->rtp.transport_cc =
      send_codec_ ? HasTransportCc(send_codec_->codec) : false;

  sp.GetFidSsrc(ssrc, &config->rtp.rtx_ssrc);

  config->rtp.extensions = recv_rtp_extensions_;

  // TODO(brandtr): Generalize when we add support for multistream protection.
  flexfec_config->payload_type = recv_flexfec_payload_type_;
  if (IsFlexfecAdvertisedFieldTrialEnabled() &&
      sp.GetFecFrSsrc(ssrc, &flexfec_config->remote_ssrc)) {
    flexfec_config->protected_media_ssrcs = {ssrc};
    flexfec_config->local_ssrc = config->rtp.local_ssrc;
    flexfec_config->rtcp_mode = config->rtp.rtcp_mode;
    // TODO(brandtr): We should be spec-compliant and set |transport_cc| here
    // based on the rtcp-fb for the FlexFEC codec, not the media codec.
    flexfec_config->transport_cc = config->rtp.transport_cc;
    flexfec_config->rtp_header_extensions = config->rtp.extensions;
  }
}

bool WebRtcVideoChannel::RemoveRecvStream(uint32_t ssrc) {
  RTC_LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;
  if (ssrc == 0) {
    RTC_LOG(LS_ERROR) << "RemoveRecvStream with 0 ssrc is not supported.";
    return false;
  }

  rtc::CritScope stream_lock(&stream_crit_);
  std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator stream =
      receive_streams_.find(ssrc);
  if (stream == receive_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Stream not found for ssrc: " << ssrc;
    return false;
  }
  DeleteReceiveStream(stream->second);
  receive_streams_.erase(stream);

  return true;
}

bool WebRtcVideoChannel::SetSink(
    uint32_t ssrc,
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_LOG(LS_INFO) << "SetSink: ssrc:" << ssrc << " "
                   << (sink ? "(ptr)" : "nullptr");
  if (ssrc == 0) {
    // Do not hold |stream_crit_| here, since SetDefaultSink will call
    // WebRtcVideoChannel::GetDefaultReceiveStreamSsrc().
    default_unsignalled_ssrc_handler_.SetDefaultSink(this, sink);
    return true;
  }

  rtc::CritScope stream_lock(&stream_crit_);
  std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
      receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    return false;
  }

  it->second->SetSink(sink);
  return true;
}

bool WebRtcVideoChannel::GetStats(VideoMediaInfo* info) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::GetStats");

  // Log stats periodically.
  bool log_stats = false;
  int64_t now_ms = rtc::TimeMillis();
  if (last_stats_log_ms_ == -1 ||
      now_ms - last_stats_log_ms_ > kStatsLogIntervalMs) {
    last_stats_log_ms_ = now_ms;
    log_stats = true;
  }

  info->Clear();
  FillSenderStats(info, log_stats);
  FillReceiverStats(info, log_stats);
  FillSendAndReceiveCodecStats(info);
  // TODO(holmer): We should either have rtt available as a metric on
  // VideoSend/ReceiveStreams, or we should remove rtt from VideoSenderInfo.
  webrtc::Call::Stats stats = call_->GetStats();
  if (stats.rtt_ms != -1) {
    for (size_t i = 0; i < info->senders.size(); ++i) {
      info->senders[i].rtt_ms = stats.rtt_ms;
    }
  }

  if (log_stats)
    RTC_LOG(LS_INFO) << stats.ToString(now_ms);

  return true;
}

void WebRtcVideoChannel::FillSenderStats(VideoMediaInfo* video_media_info,
                                          bool log_stats) {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end(); ++it) {
    video_media_info->senders.push_back(
        it->second->GetVideoSenderInfo(log_stats));
  }
}

void WebRtcVideoChannel::FillReceiverStats(VideoMediaInfo* video_media_info,
                                            bool log_stats) {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end(); ++it) {
    video_media_info->receivers.push_back(
        it->second->GetVideoReceiverInfo(log_stats));
  }
}

void WebRtcVideoChannel::FillBitrateInfo(BandwidthEstimationInfo* bwe_info) {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator stream =
           send_streams_.begin();
       stream != send_streams_.end(); ++stream) {
    stream->second->FillBitrateInfo(bwe_info);
  }
}

void WebRtcVideoChannel::FillSendAndReceiveCodecStats(
    VideoMediaInfo* video_media_info) {
  for (const VideoCodec& codec : send_params_.codecs) {
    webrtc::RtpCodecParameters codec_params = codec.ToCodecParameters();
    video_media_info->send_codecs.insert(
        std::make_pair(codec_params.payload_type, std::move(codec_params)));
  }
  for (const VideoCodec& codec : recv_params_.codecs) {
    webrtc::RtpCodecParameters codec_params = codec.ToCodecParameters();
    video_media_info->receive_codecs.insert(
        std::make_pair(codec_params.payload_type, std::move(codec_params)));
  }
}

void WebRtcVideoChannel::OnPacketReceived(
    rtc::CopyOnWriteBuffer* packet,
    const rtc::PacketTime& packet_time) {
  const webrtc::PacketTime webrtc_packet_time(packet_time.timestamp,
                                              packet_time.not_before);
  const webrtc::PacketReceiver::DeliveryStatus delivery_result =
      call_->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO,
          packet->cdata(), packet->size(),
          webrtc_packet_time);
  switch (delivery_result) {
    case webrtc::PacketReceiver::DELIVERY_OK:
      return;
    case webrtc::PacketReceiver::DELIVERY_PACKET_ERROR:
      return;
    case webrtc::PacketReceiver::DELIVERY_UNKNOWN_SSRC:
      break;
  }

  uint32_t ssrc = 0;
  if (!GetRtpSsrc(packet->cdata(), packet->size(), &ssrc)) {
    return;
  }

  int payload_type = 0;
  if (!GetRtpPayloadType(packet->cdata(), packet->size(), &payload_type)) {
    return;
  }

  // See if this payload_type is registered as one that usually gets its own
  // SSRC (RTX) or at least is safe to drop either way (FEC). If it is, and
  // it wasn't handled above by DeliverPacket, that means we don't know what
  // stream it associates with, and we shouldn't ever create an implicit channel
  // for these.
  for (auto& codec : recv_codecs_) {
    if (payload_type == codec.rtx_payload_type ||
        payload_type == codec.ulpfec.red_rtx_payload_type ||
        payload_type == codec.ulpfec.ulpfec_payload_type) {
      return;
    }
  }
  if (payload_type == recv_flexfec_payload_type_) {
    return;
  }

  switch (unsignalled_ssrc_handler_->OnUnsignalledSsrc(this, ssrc)) {
    case UnsignalledSsrcHandler::kDropPacket:
      return;
    case UnsignalledSsrcHandler::kDeliverPacket:
      break;
  }

  if (call_->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO,
          packet->cdata(), packet->size(),
          webrtc_packet_time) != webrtc::PacketReceiver::DELIVERY_OK) {
    RTC_LOG(LS_WARNING) << "Failed to deliver RTP packet on re-delivery.";
    return;
  }
}

void WebRtcVideoChannel::OnRtcpReceived(
    rtc::CopyOnWriteBuffer* packet,
    const rtc::PacketTime& packet_time) {
  const webrtc::PacketTime webrtc_packet_time(packet_time.timestamp,
                                              packet_time.not_before);
  // TODO(pbos): Check webrtc::PacketReceiver::DELIVERY_OK once we deliver
  // for both audio and video on the same path. Since BundleFilter doesn't
  // filter RTCP anymore incoming RTCP packets could've been going to audio (so
  // logging failures spam the log).
  call_->Receiver()->DeliverPacket(
      webrtc::MediaType::VIDEO,
      packet->cdata(), packet->size(),
      webrtc_packet_time);
}

void WebRtcVideoChannel::OnReadyToSend(bool ready) {
  RTC_LOG(LS_VERBOSE) << "OnReadyToSend: " << (ready ? "Ready." : "Not ready.");
  call_->SignalChannelNetworkState(
      webrtc::MediaType::VIDEO,
      ready ? webrtc::kNetworkUp : webrtc::kNetworkDown);
}

void WebRtcVideoChannel::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  // TODO(zhihaung): Merge these two callbacks.
  call_->OnNetworkRouteChanged(transport_name, network_route);
  call_->OnTransportOverheadChanged(webrtc::MediaType::VIDEO,
                                    network_route.packet_overhead);
}

void WebRtcVideoChannel::SetInterface(NetworkInterface* iface) {
  MediaChannel::SetInterface(iface);
  // Set the RTP recv/send buffer to a bigger size
  MediaChannel::SetOption(NetworkInterface::ST_RTP,
                          rtc::Socket::OPT_RCVBUF,
                          kVideoRtpBufferSize);

  // Speculative change to increase the outbound socket buffer size.
  // In b/15152257, we are seeing a significant number of packets discarded
  // due to lack of socket buffer space, although it's not yet clear what the
  // ideal value should be.
  MediaChannel::SetOption(NetworkInterface::ST_RTP,
                          rtc::Socket::OPT_SNDBUF,
                          kVideoRtpBufferSize);
}

rtc::Optional<uint32_t> WebRtcVideoChannel::GetDefaultReceiveStreamSsrc() {
  rtc::CritScope stream_lock(&stream_crit_);
  rtc::Optional<uint32_t> ssrc;
  for (auto it = receive_streams_.begin(); it != receive_streams_.end(); ++it) {
    if (it->second->IsDefaultStream()) {
      ssrc.emplace(it->first);
      break;
    }
  }
  return ssrc;
}

bool WebRtcVideoChannel::SendRtp(const uint8_t* data,
                                 size_t len,
                                 const webrtc::PacketOptions& options) {
  rtc::CopyOnWriteBuffer packet(data, len, kMaxRtpPacketLen);
  rtc::PacketOptions rtc_options;
  rtc_options.packet_id = options.packet_id;
  return MediaChannel::SendPacket(&packet, rtc_options);
}

bool WebRtcVideoChannel::SendRtcp(const uint8_t* data, size_t len) {
  rtc::CopyOnWriteBuffer packet(data, len, kMaxRtpPacketLen);
  return MediaChannel::SendRtcp(&packet, rtc::PacketOptions());
}

WebRtcVideoChannel::WebRtcVideoSendStream::VideoSendStreamParameters::
    VideoSendStreamParameters(
        webrtc::VideoSendStream::Config config,
        const VideoOptions& options,
        int max_bitrate_bps,
        const rtc::Optional<VideoCodecSettings>& codec_settings)
    : config(std::move(config)),
      options(options),
      max_bitrate_bps(max_bitrate_bps),
      conference_mode(false),
      codec_settings(codec_settings) {}

WebRtcVideoChannel::WebRtcVideoSendStream::WebRtcVideoSendStream(
    webrtc::Call* call,
    const StreamParams& sp,
    webrtc::VideoSendStream::Config config,
    const VideoOptions& options,
    webrtc::VideoEncoderFactory* encoder_factory,
    bool enable_cpu_overuse_detection,
    int max_bitrate_bps,
    const rtc::Optional<VideoCodecSettings>& codec_settings,
    const rtc::Optional<std::vector<webrtc::RtpExtension>>& rtp_extensions,
    // TODO(deadbeef): Don't duplicate information between send_params,
    // rtp_extensions, options, etc.
    const VideoSendParameters& send_params)
    : worker_thread_(rtc::Thread::Current()),
      ssrcs_(sp.ssrcs),
      ssrc_groups_(sp.ssrc_groups),
      call_(call),
      enable_cpu_overuse_detection_(enable_cpu_overuse_detection),
      source_(nullptr),
      encoder_factory_(encoder_factory),
      stream_(nullptr),
      encoder_sink_(nullptr),
      parameters_(std::move(config), options, max_bitrate_bps, codec_settings),
      rtp_parameters_(CreateRtpParametersWithOneEncoding()),
      sending_(false) {
  parameters_.config.rtp.max_packet_size = kVideoMtu;
  parameters_.conference_mode = send_params.conference_mode;

  sp.GetPrimarySsrcs(&parameters_.config.rtp.ssrcs);

  // ValidateStreamParams should prevent this from happening.
  RTC_CHECK(!parameters_.config.rtp.ssrcs.empty());
  rtp_parameters_.encodings[0].ssrc = parameters_.config.rtp.ssrcs[0];

  // RTX.
  sp.GetFidSsrcs(parameters_.config.rtp.ssrcs,
                 &parameters_.config.rtp.rtx.ssrcs);

  // FlexFEC SSRCs.
  // TODO(brandtr): This code needs to be generalized when we add support for
  // multistream protection.
  if (IsFlexfecFieldTrialEnabled()) {
    uint32_t flexfec_ssrc;
    bool flexfec_enabled = false;
    for (uint32_t primary_ssrc : parameters_.config.rtp.ssrcs) {
      if (sp.GetFecFrSsrc(primary_ssrc, &flexfec_ssrc)) {
        if (flexfec_enabled) {
          RTC_LOG(LS_INFO)
              << "Multiple FlexFEC streams in local SDP, but "
                 "our implementation only supports a single FlexFEC "
                 "stream. Will not enable FlexFEC for proposed "
                 "stream with SSRC: "
              << flexfec_ssrc << ".";
          continue;
        }

        flexfec_enabled = true;
        parameters_.config.rtp.flexfec.ssrc = flexfec_ssrc;
        parameters_.config.rtp.flexfec.protected_media_ssrcs = {primary_ssrc};
      }
    }
  }

  parameters_.config.rtp.c_name = sp.cname;
  parameters_.config.track_id = sp.id;
  if (rtp_extensions) {
    parameters_.config.rtp.extensions = *rtp_extensions;
  }
  parameters_.config.rtp.rtcp_mode = send_params.rtcp.reduced_size
                                         ? webrtc::RtcpMode::kReducedSize
                                         : webrtc::RtcpMode::kCompound;
  if (codec_settings) {
    bool force_encoder_allocation = false;
    SetCodec(*codec_settings, force_encoder_allocation);
  }
}

WebRtcVideoChannel::WebRtcVideoSendStream::~WebRtcVideoSendStream() {
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }
  // Release |allocated_encoder_|.
  allocated_encoder_.reset();
}

bool WebRtcVideoChannel::WebRtcVideoSendStream::SetVideoSend(
    bool enable,
    const VideoOptions* options,
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source) {
  TRACE_EVENT0("webrtc", "WebRtcVideoSendStream::SetVideoSend");
  RTC_DCHECK_RUN_ON(&thread_checker_);

  // Ignore |options| pointer if |enable| is false.
  bool options_present = enable && options;

  if (options_present) {
    VideoOptions old_options = parameters_.options;
    parameters_.options.SetAll(*options);
    if (parameters_.options.is_screencast.value_or(false) !=
            old_options.is_screencast.value_or(false) &&
        parameters_.codec_settings) {
      // If screen content settings change, we may need to recreate the codec
      // instance so that the correct type is used.

      bool force_encoder_allocation = true;
      SetCodec(*parameters_.codec_settings, force_encoder_allocation);
      // Mark screenshare parameter as being updated, then test for any other
      // changes that may require codec reconfiguration.
      old_options.is_screencast = options->is_screencast;
    }
    if (parameters_.options != old_options) {
      ReconfigureEncoder();
    }
  }

  if (source_ && stream_) {
    stream_->SetSource(nullptr, DegradationPreference::kDegradationDisabled);
  }
  // Switch to the new source.
  source_ = source;
  if (source && stream_) {
    stream_->SetSource(this, GetDegradationPreference());
  }
  return true;
}

webrtc::VideoSendStream::DegradationPreference
WebRtcVideoChannel::WebRtcVideoSendStream::GetDegradationPreference() const {
  // Do not adapt resolution for screen content as this will likely
  // result in blurry and unreadable text.
  // |this| acts like a VideoSource to make sure SinkWants are handled on the
  // correct thread.
  DegradationPreference degradation_preference;
  if (!enable_cpu_overuse_detection_) {
    degradation_preference = DegradationPreference::kDegradationDisabled;
  } else {
    if (parameters_.options.is_screencast.value_or(false)) {
      degradation_preference = DegradationPreference::kMaintainResolution;
    } else if (webrtc::field_trial::IsEnabled(
                   "WebRTC-Video-BalancedDegradation")) {
      degradation_preference = DegradationPreference::kBalanced;
    } else {
      degradation_preference = DegradationPreference::kMaintainFramerate;
    }
  }
  return degradation_preference;
}

const std::vector<uint32_t>&
WebRtcVideoChannel::WebRtcVideoSendStream::GetSsrcs() const {
  return ssrcs_;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetCodec(
    const VideoCodecSettings& codec_settings,
    bool force_encoder_allocation) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  parameters_.encoder_config = CreateVideoEncoderConfig(codec_settings.codec);
  RTC_DCHECK_GT(parameters_.encoder_config.number_of_streams, 0);

  // Do not re-create encoders of the same type. We can't overwrite
  // |allocated_encoder_| immediately, because we need to release it after the
  // RecreateWebRtcStream() call.
  std::unique_ptr<webrtc::VideoEncoder> new_encoder;
  if (force_encoder_allocation || !allocated_encoder_ ||
      allocated_codec_ != codec_settings.codec) {
    const webrtc::SdpVideoFormat format(codec_settings.codec.name,
                                        codec_settings.codec.params);
    new_encoder = encoder_factory_->CreateVideoEncoder(format);

    parameters_.config.encoder_settings.encoder = new_encoder.get();

    const webrtc::VideoEncoderFactory::CodecInfo info =
        encoder_factory_->QueryVideoEncoder(format);
    parameters_.config.encoder_settings.full_overuse_time =
        info.is_hardware_accelerated;
    parameters_.config.encoder_settings.internal_source =
        info.has_internal_source;
  } else {
    new_encoder = std::move(allocated_encoder_);
  }
  parameters_.config.encoder_settings.payload_name = codec_settings.codec.name;
  parameters_.config.encoder_settings.payload_type = codec_settings.codec.id;
  parameters_.config.rtp.ulpfec = codec_settings.ulpfec;
  parameters_.config.rtp.flexfec.payload_type =
      codec_settings.flexfec_payload_type;

  // Set RTX payload type if RTX is enabled.
  if (!parameters_.config.rtp.rtx.ssrcs.empty()) {
    if (codec_settings.rtx_payload_type == -1) {
      RTC_LOG(LS_WARNING)
          << "RTX SSRCs configured but there's no configured RTX "
             "payload type. Ignoring.";
      parameters_.config.rtp.rtx.ssrcs.clear();
    } else {
      parameters_.config.rtp.rtx.payload_type = codec_settings.rtx_payload_type;
    }
  }

  parameters_.config.rtp.nack.rtp_history_ms =
      HasNack(codec_settings.codec) ? kNackHistoryMs : 0;

  parameters_.codec_settings = codec_settings;

  RTC_LOG(LS_INFO) << "RecreateWebRtcStream (send) because of SetCodec.";
  RecreateWebRtcStream();
  allocated_encoder_ = std::move(new_encoder);
  allocated_codec_ = codec_settings.codec;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetSendParameters(
    const ChangedSendParameters& params) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  // |recreate_stream| means construction-time parameters have changed and the
  // sending stream needs to be reset with the new config.
  bool recreate_stream = false;
  if (params.rtcp_mode) {
    parameters_.config.rtp.rtcp_mode = *params.rtcp_mode;
    recreate_stream = true;
  }
  if (params.rtp_header_extensions) {
    parameters_.config.rtp.extensions = *params.rtp_header_extensions;
    recreate_stream = true;
  }
  if (params.max_bandwidth_bps) {
    parameters_.max_bitrate_bps = *params.max_bandwidth_bps;
    ReconfigureEncoder();
  }
  if (params.conference_mode) {
    parameters_.conference_mode = *params.conference_mode;
  }

  // Set codecs and options.
  if (params.codec) {
    bool force_encoder_allocation = false;
    SetCodec(*params.codec, force_encoder_allocation);
    recreate_stream = false;  // SetCodec has already recreated the stream.
  } else if (params.conference_mode && parameters_.codec_settings) {
    bool force_encoder_allocation = false;
    SetCodec(*parameters_.codec_settings, force_encoder_allocation);
    recreate_stream = false;  // SetCodec has already recreated the stream.
  }
  if (recreate_stream) {
    RTC_LOG(LS_INFO)
        << "RecreateWebRtcStream (send) because of SetSendParameters";
    RecreateWebRtcStream();
  }
}

bool WebRtcVideoChannel::WebRtcVideoSendStream::SetRtpParameters(
    const webrtc::RtpParameters& new_parameters) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!ValidateRtpParameters(new_parameters)) {
    return false;
  }

  bool reconfigure_encoder = new_parameters.encodings[0].max_bitrate_bps !=
                             rtp_parameters_.encodings[0].max_bitrate_bps;
  rtp_parameters_ = new_parameters;
  // Codecs are currently handled at the WebRtcVideoChannel level.
  rtp_parameters_.codecs.clear();
  if (reconfigure_encoder) {
    ReconfigureEncoder();
  }
  // Encoding may have been activated/deactivated.
  UpdateSendState();
  return true;
}

webrtc::RtpParameters
WebRtcVideoChannel::WebRtcVideoSendStream::GetRtpParameters() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return rtp_parameters_;
}

bool WebRtcVideoChannel::WebRtcVideoSendStream::ValidateRtpParameters(
    const webrtc::RtpParameters& rtp_parameters) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (rtp_parameters.encodings.size() != 1) {
    RTC_LOG(LS_ERROR)
        << "Attempted to set RtpParameters without exactly one encoding";
    return false;
  }
  if (rtp_parameters.encodings[0].ssrc != rtp_parameters_.encodings[0].ssrc) {
    RTC_LOG(LS_ERROR) << "Attempted to set RtpParameters with modified SSRC";
    return false;
  }
  return true;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::UpdateSendState() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  // TODO(deadbeef): Need to handle more than one encoding in the future.
  RTC_DCHECK(rtp_parameters_.encodings.size() == 1u);
  if (sending_ && rtp_parameters_.encodings[0].active) {
    RTC_DCHECK(stream_ != nullptr);
    stream_->Start();
  } else {
    if (stream_ != nullptr) {
      stream_->Stop();
    }
  }
}

webrtc::VideoEncoderConfig
WebRtcVideoChannel::WebRtcVideoSendStream::CreateVideoEncoderConfig(
    const VideoCodec& codec) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  webrtc::VideoEncoderConfig encoder_config;
  bool is_screencast = parameters_.options.is_screencast.value_or(false);
  if (is_screencast) {
    encoder_config.min_transmit_bitrate_bps =
        1000 * parameters_.options.screencast_min_bitrate_kbps.value_or(0);
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kScreen;
  } else {
    encoder_config.min_transmit_bitrate_bps = 0;
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo;
  }

  // By default, the stream count for the codec configuration should match the
  // number of negotiated ssrcs. But if the codec is blacklisted for simulcast
  // or a screencast (and not in simulcast screenshare experiment), only
  // configure a single stream.
  encoder_config.number_of_streams = parameters_.config.rtp.ssrcs.size();
  if (IsCodecBlacklistedForSimulcast(codec.name) ||
      (is_screencast &&
       (!UseSimulcastScreenshare() || !parameters_.conference_mode))) {
    encoder_config.number_of_streams = 1;
  }

  int stream_max_bitrate = parameters_.max_bitrate_bps;
  if (rtp_parameters_.encodings[0].max_bitrate_bps) {
    stream_max_bitrate =
        webrtc::MinPositive(*(rtp_parameters_.encodings[0].max_bitrate_bps),
                            parameters_.max_bitrate_bps);
  }

  int codec_max_bitrate_kbps;
  if (codec.GetParam(kCodecParamMaxBitrate, &codec_max_bitrate_kbps)) {
    stream_max_bitrate = codec_max_bitrate_kbps * 1000;
  }
  encoder_config.max_bitrate_bps = stream_max_bitrate;

  int max_qp = kDefaultQpMax;
  codec.GetParam(kCodecParamMaxQuantization, &max_qp);
  encoder_config.video_stream_factory =
      new rtc::RefCountedObject<EncoderStreamFactory>(
          codec.name, max_qp, kDefaultVideoMaxFramerate, is_screencast,
          parameters_.conference_mode);
  return encoder_config;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::ReconfigureEncoder() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!stream_) {
    // The webrtc::VideoSendStream |stream_| has not yet been created but other
    // parameters has changed.
    return;
  }

  RTC_DCHECK_GT(parameters_.encoder_config.number_of_streams, 0);

  RTC_CHECK(parameters_.codec_settings);
  VideoCodecSettings codec_settings = *parameters_.codec_settings;

  webrtc::VideoEncoderConfig encoder_config =
      CreateVideoEncoderConfig(codec_settings.codec);

  encoder_config.encoder_specific_settings = ConfigureVideoEncoderSettings(
      codec_settings.codec);

  stream_->ReconfigureVideoEncoder(encoder_config.Copy());

  encoder_config.encoder_specific_settings = NULL;

  parameters_.encoder_config = std::move(encoder_config);
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetSend(bool send) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  sending_ = send;
  UpdateSendState();
}

void WebRtcVideoChannel::WebRtcVideoSendStream::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(encoder_sink_ == sink);
  encoder_sink_ = nullptr;
  source_->RemoveSink(sink);
}

void WebRtcVideoChannel::WebRtcVideoSendStream::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  if (worker_thread_ == rtc::Thread::Current()) {
    // AddOrUpdateSink is called on |worker_thread_| if this is the first
    // registration of |sink|.
    RTC_DCHECK_RUN_ON(&thread_checker_);
    encoder_sink_ = sink;
    source_->AddOrUpdateSink(encoder_sink_, wants);
  } else {
    // Subsequent calls to AddOrUpdateSink will happen on the encoder task
    // queue.
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, worker_thread_, [this, sink, wants] {
          RTC_DCHECK_RUN_ON(&thread_checker_);
          // |sink| may be invalidated after this task was posted since
          // RemoveSink is called on the worker thread.
          bool encoder_sink_valid = (sink == encoder_sink_);
          if (source_ && encoder_sink_valid) {
            source_->AddOrUpdateSink(encoder_sink_, wants);
          }
        });
  }
}

VideoSenderInfo WebRtcVideoChannel::WebRtcVideoSendStream::GetVideoSenderInfo(
    bool log_stats) {
  VideoSenderInfo info;
  RTC_DCHECK_RUN_ON(&thread_checker_);
  for (uint32_t ssrc : parameters_.config.rtp.ssrcs)
    info.add_ssrc(ssrc);

  if (parameters_.codec_settings) {
    info.codec_name = parameters_.codec_settings->codec.name;
    info.codec_payload_type = parameters_.codec_settings->codec.id;
  }

  if (stream_ == NULL)
    return info;

  webrtc::VideoSendStream::Stats stats = stream_->GetStats();

  if (log_stats)
    RTC_LOG(LS_INFO) << stats.ToString(rtc::TimeMillis());

  info.adapt_changes = stats.number_of_cpu_adapt_changes;
  info.adapt_reason =
      stats.cpu_limited_resolution ? ADAPTREASON_CPU : ADAPTREASON_NONE;
  info.has_entered_low_resolution = stats.has_entered_low_resolution;

  // Get bandwidth limitation info from stream_->GetStats().
  // Input resolution (output from video_adapter) can be further scaled down or
  // higher video layer(s) can be dropped due to bitrate constraints.
  // Note, adapt_changes only include changes from the video_adapter.
  if (stats.bw_limited_resolution)
    info.adapt_reason |= ADAPTREASON_BANDWIDTH;

  info.encoder_implementation_name = stats.encoder_implementation_name;
  info.ssrc_groups = ssrc_groups_;
  info.framerate_input = stats.input_frame_rate;
  info.framerate_sent = stats.encode_frame_rate;
  info.avg_encode_ms = stats.avg_encode_time_ms;
  info.encode_usage_percent = stats.encode_usage_percent;
  info.frames_encoded = stats.frames_encoded;
  info.qp_sum = stats.qp_sum;

  info.nominal_bitrate = stats.media_bitrate_bps;
  info.preferred_bitrate = stats.preferred_media_bitrate_bps;

  info.content_type = stats.content_type;

  info.send_frame_width = 0;
  info.send_frame_height = 0;
  for (std::map<uint32_t, webrtc::VideoSendStream::StreamStats>::iterator it =
           stats.substreams.begin();
       it != stats.substreams.end(); ++it) {
    // TODO(pbos): Wire up additional stats, such as padding bytes.
    webrtc::VideoSendStream::StreamStats stream_stats = it->second;
    info.bytes_sent += stream_stats.rtp_stats.transmitted.payload_bytes +
                       stream_stats.rtp_stats.transmitted.header_bytes +
                       stream_stats.rtp_stats.transmitted.padding_bytes;
    info.packets_sent += stream_stats.rtp_stats.transmitted.packets;
    info.packets_lost += stream_stats.rtcp_stats.packets_lost;
    if (stream_stats.width > info.send_frame_width)
      info.send_frame_width = stream_stats.width;
    if (stream_stats.height > info.send_frame_height)
      info.send_frame_height = stream_stats.height;
    info.firs_rcvd += stream_stats.rtcp_packet_type_counts.fir_packets;
    info.nacks_rcvd += stream_stats.rtcp_packet_type_counts.nack_packets;
    info.plis_rcvd += stream_stats.rtcp_packet_type_counts.pli_packets;
  }

  if (!stats.substreams.empty()) {
    // TODO(pbos): Report fraction lost per SSRC.
    webrtc::VideoSendStream::StreamStats first_stream_stats =
        stats.substreams.begin()->second;
    info.fraction_lost =
        static_cast<float>(first_stream_stats.rtcp_stats.fraction_lost) /
        (1 << 8);
  }

  return info;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::FillBitrateInfo(
    BandwidthEstimationInfo* bwe_info) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (stream_ == NULL) {
    return;
  }
  webrtc::VideoSendStream::Stats stats = stream_->GetStats();
  for (std::map<uint32_t, webrtc::VideoSendStream::StreamStats>::iterator it =
           stats.substreams.begin();
       it != stats.substreams.end(); ++it) {
    bwe_info->transmit_bitrate += it->second.total_bitrate_bps;
    bwe_info->retransmit_bitrate += it->second.retransmit_bitrate_bps;
  }
  bwe_info->target_enc_bitrate += stats.target_media_bitrate_bps;
  bwe_info->actual_enc_bitrate += stats.media_bitrate_bps;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::RecreateWebRtcStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }

  RTC_CHECK(parameters_.codec_settings);
  RTC_DCHECK_EQ((parameters_.encoder_config.content_type ==
                 webrtc::VideoEncoderConfig::ContentType::kScreen),
                parameters_.options.is_screencast.value_or(false))
      << "encoder content type inconsistent with screencast option";
  parameters_.encoder_config.encoder_specific_settings =
      ConfigureVideoEncoderSettings(parameters_.codec_settings->codec);

  webrtc::VideoSendStream::Config config = parameters_.config.Copy();
  if (!config.rtp.rtx.ssrcs.empty() && config.rtp.rtx.payload_type == -1) {
    RTC_LOG(LS_WARNING) << "RTX SSRCs configured but there's no configured RTX "
                           "payload type the set codec. Ignoring RTX.";
    config.rtp.rtx.ssrcs.clear();
  }
  stream_ = call_->CreateVideoSendStream(std::move(config),
                                         parameters_.encoder_config.Copy());

  parameters_.encoder_config.encoder_specific_settings = NULL;

  if (source_) {
    stream_->SetSource(this, GetDegradationPreference());
  }

  // Call stream_->Start() if necessary conditions are met.
  UpdateSendState();
}

WebRtcVideoChannel::WebRtcVideoReceiveStream::WebRtcVideoReceiveStream(
    webrtc::Call* call,
    const StreamParams& sp,
    webrtc::VideoReceiveStream::Config config,
    DecoderFactoryAdapter* decoder_factory,
    bool default_stream,
    const std::vector<VideoCodecSettings>& recv_codecs,
    const webrtc::FlexfecReceiveStream::Config& flexfec_config)
    : call_(call),
      stream_params_(sp),
      stream_(NULL),
      default_stream_(default_stream),
      config_(std::move(config)),
      flexfec_config_(flexfec_config),
      flexfec_stream_(nullptr),
      decoder_factory_(decoder_factory),
      sink_(NULL),
      first_frame_timestamp_(-1),
      estimated_remote_start_ntp_time_ms_(0) {
  config_.renderer = this;
  DecoderMap old_decoders;
  ConfigureCodecs(recv_codecs, &old_decoders);
  ConfigureFlexfecCodec(flexfec_config.payload_type);
  MaybeRecreateWebRtcFlexfecStream();
  RecreateWebRtcVideoStream();
  RTC_DCHECK(old_decoders.empty());
}

WebRtcVideoChannel::WebRtcVideoReceiveStream::~WebRtcVideoReceiveStream() {
  if (flexfec_stream_) {
    MaybeDissociateFlexfecFromVideo();
    call_->DestroyFlexfecReceiveStream(flexfec_stream_);
  }
  call_->DestroyVideoReceiveStream(stream_);
  allocated_decoders_.clear();
}

const std::vector<uint32_t>&
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetSsrcs() const {
  return stream_params_.ssrcs;
}

rtc::Optional<uint32_t>
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetFirstPrimarySsrc() const {
  std::vector<uint32_t> primary_ssrcs;
  stream_params_.GetPrimarySsrcs(&primary_ssrcs);

  if (primary_ssrcs.empty()) {
    RTC_LOG(LS_WARNING)
        << "Empty primary ssrcs vector, returning empty optional";
    return rtc::nullopt;
  } else {
    return primary_ssrcs[0];
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::ConfigureCodecs(
    const std::vector<VideoCodecSettings>& recv_codecs,
    DecoderMap* old_decoders) {
  RTC_DCHECK(!recv_codecs.empty());
  *old_decoders = std::move(allocated_decoders_);
  config_.decoders.clear();
  config_.rtp.rtx_associated_payload_types.clear();
  for (const auto& recv_codec : recv_codecs) {
    webrtc::SdpVideoFormat video_format(recv_codec.codec.name,
                                        recv_codec.codec.params);
    std::unique_ptr<webrtc::VideoDecoder> new_decoder;

    auto it = old_decoders->find(video_format);
    if (it != old_decoders->end()) {
      new_decoder = std::move(it->second);
      old_decoders->erase(it);
    }

    if (!new_decoder && decoder_factory_) {
      decoder_factory_->SetReceiveStreamId(stream_params_.id);
      new_decoder = decoder_factory_->CreateVideoDecoder(webrtc::SdpVideoFormat(
          recv_codec.codec.name, recv_codec.codec.params));
    }

    // If we still have no valid decoder, we have to create a "Null" decoder
    // that ignores all calls. The reason we can get into this state is that
    // the old decoder factory interface doesn't have a way to query supported
    // codecs.
    if (!new_decoder)
      new_decoder.reset(new NullVideoDecoder());

    webrtc::VideoReceiveStream::Decoder decoder;
    decoder.decoder = new_decoder.get();
    decoder.payload_type = recv_codec.codec.id;
    decoder.payload_name = recv_codec.codec.name;
    decoder.codec_params = recv_codec.codec.params;
    config_.decoders.push_back(decoder);
    config_.rtp.rtx_associated_payload_types[recv_codec.rtx_payload_type] =
        recv_codec.codec.id;

    const bool did_insert =
        allocated_decoders_
            .insert(std::make_pair(video_format, std::move(new_decoder)))
            .second;
    RTC_CHECK(did_insert);
  }

  const auto& codec = recv_codecs.front();
  config_.rtp.ulpfec_payload_type = codec.ulpfec.ulpfec_payload_type;
  config_.rtp.red_payload_type = codec.ulpfec.red_payload_type;

  config_.rtp.nack.rtp_history_ms = HasNack(codec.codec) ? kNackHistoryMs : 0;
  if (codec.ulpfec.red_rtx_payload_type != -1) {
    config_.rtp
        .rtx_associated_payload_types[codec.ulpfec.red_rtx_payload_type] =
        codec.ulpfec.red_payload_type;
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::ConfigureFlexfecCodec(
    int flexfec_payload_type) {
  flexfec_config_.payload_type = flexfec_payload_type;
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetLocalSsrc(
    uint32_t local_ssrc) {
  // TODO(pbos): Consider turning this sanity check into a RTC_DCHECK. You
  // should not be able to create a sender with the same SSRC as a receiver, but
  // right now this can't be done due to unittests depending on receiving what
  // they are sending from the same MediaChannel.
  if (local_ssrc == config_.rtp.remote_ssrc) {
    RTC_LOG(LS_INFO) << "Ignoring call to SetLocalSsrc because parameters are "
                        "unchanged; local_ssrc="
                     << local_ssrc;
    return;
  }

  config_.rtp.local_ssrc = local_ssrc;
  flexfec_config_.local_ssrc = local_ssrc;
  RTC_LOG(LS_INFO)
      << "RecreateWebRtcStream (recv) because of SetLocalSsrc; local_ssrc="
      << local_ssrc;
  MaybeRecreateWebRtcFlexfecStream();
  RecreateWebRtcVideoStream();
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetFeedbackParameters(
    bool nack_enabled,
    bool remb_enabled,
    bool transport_cc_enabled,
    webrtc::RtcpMode rtcp_mode) {
  int nack_history_ms = nack_enabled ? kNackHistoryMs : 0;
  if (config_.rtp.nack.rtp_history_ms == nack_history_ms &&
      config_.rtp.remb == remb_enabled &&
      config_.rtp.transport_cc == transport_cc_enabled &&
      config_.rtp.rtcp_mode == rtcp_mode) {
    RTC_LOG(LS_INFO)
        << "Ignoring call to SetFeedbackParameters because parameters are "
           "unchanged; nack="
        << nack_enabled << ", remb=" << remb_enabled
        << ", transport_cc=" << transport_cc_enabled;
    return;
  }
  config_.rtp.remb = remb_enabled;
  config_.rtp.nack.rtp_history_ms = nack_history_ms;
  config_.rtp.transport_cc = transport_cc_enabled;
  config_.rtp.rtcp_mode = rtcp_mode;
  // TODO(brandtr): We should be spec-compliant and set |transport_cc| here
  // based on the rtcp-fb for the FlexFEC codec, not the media codec.
  flexfec_config_.transport_cc = config_.rtp.transport_cc;
  flexfec_config_.rtcp_mode = config_.rtp.rtcp_mode;
  RTC_LOG(LS_INFO)
      << "RecreateWebRtcStream (recv) because of SetFeedbackParameters; nack="
      << nack_enabled << ", remb=" << remb_enabled
      << ", transport_cc=" << transport_cc_enabled;
  MaybeRecreateWebRtcFlexfecStream();
  RecreateWebRtcVideoStream();
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetRecvParameters(
    const ChangedRecvParameters& params) {
  bool video_needs_recreation = false;
  bool flexfec_needs_recreation = false;
  DecoderMap old_decoders;
  if (params.codec_settings) {
    ConfigureCodecs(*params.codec_settings, &old_decoders);
    video_needs_recreation = true;
  }
  if (params.rtp_header_extensions) {
    config_.rtp.extensions = *params.rtp_header_extensions;
    flexfec_config_.rtp_header_extensions = *params.rtp_header_extensions;
    video_needs_recreation = true;
    flexfec_needs_recreation = true;
  }
  if (params.flexfec_payload_type) {
    ConfigureFlexfecCodec(*params.flexfec_payload_type);
    flexfec_needs_recreation = true;
  }
  if (flexfec_needs_recreation) {
    RTC_LOG(LS_INFO) << "MaybeRecreateWebRtcFlexfecStream (recv) because of "
                        "SetRecvParameters";
    MaybeRecreateWebRtcFlexfecStream();
  }
  if (video_needs_recreation) {
    RTC_LOG(LS_INFO)
        << "RecreateWebRtcVideoStream (recv) because of SetRecvParameters";
    RecreateWebRtcVideoStream();
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    RecreateWebRtcVideoStream() {
  if (stream_) {
    MaybeDissociateFlexfecFromVideo();
    call_->DestroyVideoReceiveStream(stream_);
    stream_ = nullptr;
  }
  webrtc::VideoReceiveStream::Config config = config_.Copy();
  config.rtp.protected_by_flexfec = (flexfec_stream_ != nullptr);
  stream_ = call_->CreateVideoReceiveStream(std::move(config));
  MaybeAssociateFlexfecWithVideo();
  stream_->Start();
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    MaybeRecreateWebRtcFlexfecStream() {
  if (flexfec_stream_) {
    MaybeDissociateFlexfecFromVideo();
    call_->DestroyFlexfecReceiveStream(flexfec_stream_);
    flexfec_stream_ = nullptr;
  }
  if (flexfec_config_.IsCompleteAndEnabled()) {
    flexfec_stream_ = call_->CreateFlexfecReceiveStream(flexfec_config_);
    MaybeAssociateFlexfecWithVideo();
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    MaybeAssociateFlexfecWithVideo() {
  if (stream_ && flexfec_stream_) {
    stream_->AddSecondarySink(flexfec_stream_);
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    MaybeDissociateFlexfecFromVideo() {
  if (stream_ && flexfec_stream_) {
    stream_->RemoveSecondarySink(flexfec_stream_);
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::OnFrame(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&sink_lock_);

  if (first_frame_timestamp_ < 0)
    first_frame_timestamp_ = frame.timestamp();
  int64_t rtp_time_elapsed_since_first_frame =
      (timestamp_wraparound_handler_.Unwrap(frame.timestamp()) -
       first_frame_timestamp_);
  int64_t elapsed_time_ms = rtp_time_elapsed_since_first_frame /
                            (cricket::kVideoCodecClockrate / 1000);
  if (frame.ntp_time_ms() > 0)
    estimated_remote_start_ntp_time_ms_ = frame.ntp_time_ms() - elapsed_time_ms;

  if (sink_ == NULL) {
    RTC_LOG(LS_WARNING) << "VideoReceiveStream not connected to a VideoSink.";
    return;
  }

  sink_->OnFrame(frame);
}

bool WebRtcVideoChannel::WebRtcVideoReceiveStream::IsDefaultStream() const {
  return default_stream_;
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  rtc::CritScope crit(&sink_lock_);
  sink_ = sink;
}

std::string
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetCodecNameFromPayloadType(
    int payload_type) {
  for (const webrtc::VideoReceiveStream::Decoder& decoder : config_.decoders) {
    if (decoder.payload_type == payload_type) {
      return decoder.payload_name;
    }
  }
  return "";
}

VideoReceiverInfo
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetVideoReceiverInfo(
    bool log_stats) {
  VideoReceiverInfo info;
  info.ssrc_groups = stream_params_.ssrc_groups;
  info.add_ssrc(config_.rtp.remote_ssrc);
  webrtc::VideoReceiveStream::Stats stats = stream_->GetStats();
  info.decoder_implementation_name = stats.decoder_implementation_name;
  if (stats.current_payload_type != -1) {
    info.codec_payload_type = stats.current_payload_type;
  }
  info.bytes_rcvd = stats.rtp_stats.transmitted.payload_bytes +
                    stats.rtp_stats.transmitted.header_bytes +
                    stats.rtp_stats.transmitted.padding_bytes;
  info.packets_rcvd = stats.rtp_stats.transmitted.packets;
  info.packets_lost = stats.rtcp_stats.packets_lost;
  info.fraction_lost =
      static_cast<float>(stats.rtcp_stats.fraction_lost) / (1 << 8);

  info.framerate_rcvd = stats.network_frame_rate;
  info.framerate_decoded = stats.decode_frame_rate;
  info.framerate_output = stats.render_frame_rate;
  info.frame_width = stats.width;
  info.frame_height = stats.height;

  {
    rtc::CritScope frame_cs(&sink_lock_);
    info.capture_start_ntp_time_ms = estimated_remote_start_ntp_time_ms_;
  }

  info.decode_ms = stats.decode_ms;
  info.max_decode_ms = stats.max_decode_ms;
  info.current_delay_ms = stats.current_delay_ms;
  info.target_delay_ms = stats.target_delay_ms;
  info.jitter_buffer_ms = stats.jitter_buffer_ms;
  info.min_playout_delay_ms = stats.min_playout_delay_ms;
  info.render_delay_ms = stats.render_delay_ms;
  info.frames_received = stats.frame_counts.key_frames +
                         stats.frame_counts.delta_frames;
  info.frames_decoded = stats.frames_decoded;
  info.frames_rendered = stats.frames_rendered;
  info.qp_sum = stats.qp_sum;

  info.interframe_delay_max_ms = stats.interframe_delay_max_ms;

  info.content_type = stats.content_type;

  info.codec_name = GetCodecNameFromPayloadType(stats.current_payload_type);

  info.firs_sent = stats.rtcp_packet_type_counts.fir_packets;
  info.plis_sent = stats.rtcp_packet_type_counts.pli_packets;
  info.nacks_sent = stats.rtcp_packet_type_counts.nack_packets;

  info.timing_frame_info = stats.timing_frame_info;

  if (log_stats)
    RTC_LOG(LS_INFO) << stats.ToString(rtc::TimeMillis());

  return info;
}

WebRtcVideoChannel::VideoCodecSettings::VideoCodecSettings()
    : flexfec_payload_type(-1), rtx_payload_type(-1) {}

bool WebRtcVideoChannel::VideoCodecSettings::operator==(
    const WebRtcVideoChannel::VideoCodecSettings& other) const {
  return codec == other.codec && ulpfec == other.ulpfec &&
         flexfec_payload_type == other.flexfec_payload_type &&
         rtx_payload_type == other.rtx_payload_type;
}

bool WebRtcVideoChannel::VideoCodecSettings::EqualsDisregardingFlexfec(
    const WebRtcVideoChannel::VideoCodecSettings& a,
    const WebRtcVideoChannel::VideoCodecSettings& b) {
  return a.codec == b.codec && a.ulpfec == b.ulpfec &&
         a.rtx_payload_type == b.rtx_payload_type;
}

bool WebRtcVideoChannel::VideoCodecSettings::operator!=(
    const WebRtcVideoChannel::VideoCodecSettings& other) const {
  return !(*this == other);
}

std::vector<WebRtcVideoChannel::VideoCodecSettings>
WebRtcVideoChannel::MapCodecs(const std::vector<VideoCodec>& codecs) {
  RTC_DCHECK(!codecs.empty());

  std::vector<VideoCodecSettings> video_codecs;
  std::map<int, bool> payload_used;
  std::map<int, VideoCodec::CodecType> payload_codec_type;
  // |rtx_mapping| maps video payload type to rtx payload type.
  std::map<int, int> rtx_mapping;

  webrtc::UlpfecConfig ulpfec_config;
  int flexfec_payload_type = -1;

  for (size_t i = 0; i < codecs.size(); ++i) {
    const VideoCodec& in_codec = codecs[i];
    int payload_type = in_codec.id;

    if (payload_used[payload_type]) {
      RTC_LOG(LS_ERROR) << "Payload type already registered: "
                        << in_codec.ToString();
      return std::vector<VideoCodecSettings>();
    }
    payload_used[payload_type] = true;
    payload_codec_type[payload_type] = in_codec.GetCodecType();

    switch (in_codec.GetCodecType()) {
      case VideoCodec::CODEC_RED: {
        // RED payload type, should not have duplicates.
        RTC_DCHECK_EQ(-1, ulpfec_config.red_payload_type);
        ulpfec_config.red_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_ULPFEC: {
        // ULPFEC payload type, should not have duplicates.
        RTC_DCHECK_EQ(-1, ulpfec_config.ulpfec_payload_type);
        ulpfec_config.ulpfec_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_FLEXFEC: {
        // FlexFEC payload type, should not have duplicates.
        RTC_DCHECK_EQ(-1, flexfec_payload_type);
        flexfec_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_RTX: {
        int associated_payload_type;
        if (!in_codec.GetParam(kCodecParamAssociatedPayloadType,
                               &associated_payload_type) ||
            !IsValidRtpPayloadType(associated_payload_type)) {
          RTC_LOG(LS_ERROR)
              << "RTX codec with invalid or no associated payload type: "
              << in_codec.ToString();
          return std::vector<VideoCodecSettings>();
        }
        rtx_mapping[associated_payload_type] = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_VIDEO:
        break;
    }

    video_codecs.push_back(VideoCodecSettings());
    video_codecs.back().codec = in_codec;
  }

  // One of these codecs should have been a video codec. Only having FEC
  // parameters into this code is a logic error.
  RTC_DCHECK(!video_codecs.empty());

  for (std::map<int, int>::const_iterator it = rtx_mapping.begin();
       it != rtx_mapping.end();
       ++it) {
    if (!payload_used[it->first]) {
      RTC_LOG(LS_ERROR) << "RTX mapped to payload not in codec list.";
      return std::vector<VideoCodecSettings>();
    }
    if (payload_codec_type[it->first] != VideoCodec::CODEC_VIDEO &&
        payload_codec_type[it->first] != VideoCodec::CODEC_RED) {
      RTC_LOG(LS_ERROR)
          << "RTX not mapped to regular video codec or RED codec.";
      return std::vector<VideoCodecSettings>();
    }

    if (it->first == ulpfec_config.red_payload_type) {
      ulpfec_config.red_rtx_payload_type = it->second;
    }
  }

  for (size_t i = 0; i < video_codecs.size(); ++i) {
    video_codecs[i].ulpfec = ulpfec_config;
    video_codecs[i].flexfec_payload_type = flexfec_payload_type;
    if (rtx_mapping[video_codecs[i].codec.id] != 0 &&
        rtx_mapping[video_codecs[i].codec.id] !=
            ulpfec_config.red_payload_type) {
      video_codecs[i].rtx_payload_type = rtx_mapping[video_codecs[i].codec.id];
    }
  }

  return video_codecs;
}

EncoderStreamFactory::EncoderStreamFactory(std::string codec_name,
                                           int max_qp,
                                           int max_framerate,
                                           bool is_screencast,
                                           bool conference_mode)
    : codec_name_(codec_name),
      max_qp_(max_qp),
      max_framerate_(max_framerate),
      is_screencast_(is_screencast),
      conference_mode_(conference_mode) {}

std::vector<webrtc::VideoStream> EncoderStreamFactory::CreateEncoderStreams(
    int width,
    int height,
    const webrtc::VideoEncoderConfig& encoder_config) {
  if (is_screencast_ &&
      (!conference_mode_ || !cricket::UseSimulcastScreenshare())) {
    RTC_DCHECK_EQ(1, encoder_config.number_of_streams);
  }
  if (encoder_config.number_of_streams > 1 ||
      (CodecNamesEq(codec_name_, kVp8CodecName) && is_screencast_ &&
       conference_mode_)) {
    return GetSimulcastConfig(encoder_config.number_of_streams, width, height,
                              encoder_config.max_bitrate_bps, max_qp_,
                              max_framerate_, is_screencast_);
  }

  // For unset max bitrates set default bitrate for non-simulcast.
  int max_bitrate_bps =
      (encoder_config.max_bitrate_bps > 0)
          ? encoder_config.max_bitrate_bps
          : GetMaxDefaultVideoBitrateKbps(width, height) * 1000;

  webrtc::VideoStream stream;
  stream.width = width;
  stream.height = height;
  stream.max_framerate = max_framerate_;
  stream.min_bitrate_bps = GetMinVideoBitrateBps();
  stream.target_bitrate_bps = stream.max_bitrate_bps = max_bitrate_bps;
  stream.max_qp = max_qp_;

  if (CodecNamesEq(codec_name_, kVp9CodecName) && !is_screencast_) {
    stream.temporal_layer_thresholds_bps.resize(GetDefaultVp9TemporalLayers() -
                                                1);
  }

  std::vector<webrtc::VideoStream> streams;
  streams.push_back(stream);
  return streams;
}

}  // namespace cricket
