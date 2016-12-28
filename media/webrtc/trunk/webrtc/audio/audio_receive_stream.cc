/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_receive_stream.h"

#include <string>
#include <utility>

#include "webrtc/audio/audio_sink.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/call/congestion_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/voice_engine/channel_proxy.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_neteq_stats.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {
namespace {

bool UseSendSideBwe(const webrtc::AudioReceiveStream::Config& config) {
  if (!config.rtp.transport_cc) {
    return false;
  }
  for (const auto& extension : config.rtp.extensions) {
    if (extension.name == RtpExtension::kTransportSequenceNumber) {
      return true;
    }
  }
  return false;
}
}  // namespace

std::string AudioReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", receive_transport: "
     << (receive_transport ? "(Transport)" : "nullptr");
  ss << ", rtcp_send_transport: "
     << (rtcp_send_transport ? "(Transport)" : "nullptr");
  ss << ", voe_channel_id: " << voe_channel_id;
  if (!sync_group.empty()) {
    ss << ", sync_group: " << sync_group;
  }
  ss << ", combined_audio_video_bwe: "
     << (combined_audio_video_bwe ? "true" : "false");
  ss << '}';
  return ss.str();
}

namespace internal {
AudioReceiveStream::AudioReceiveStream(
    CongestionController* congestion_controller,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state)
    : config_(config),
      audio_state_(audio_state),
      rtp_header_parser_(RtpHeaderParser::Create()) {
  LOG(LS_INFO) << "AudioReceiveStream: " << config_.ToString();
  RTC_DCHECK_NE(config_.voe_channel_id, -1);
  RTC_DCHECK(audio_state_.get());
  RTC_DCHECK(congestion_controller);
  RTC_DCHECK(rtp_header_parser_);

  VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine());
  channel_proxy_ = voe_impl->GetChannelProxy(config_.voe_channel_id);
  channel_proxy_->SetLocalSSRC(config.rtp.local_ssrc);
  for (const auto& extension : config.rtp.extensions) {
    if (extension.name == RtpExtension::kAudioLevel) {
      channel_proxy_->SetReceiveAudioLevelIndicationStatus(true, extension.id);
      bool registered = rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAudioLevel, extension.id);
      RTC_DCHECK(registered);
    } else if (extension.name == RtpExtension::kAbsSendTime) {
      channel_proxy_->SetReceiveAbsoluteSenderTimeStatus(true, extension.id);
      bool registered = rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, extension.id);
      RTC_DCHECK(registered);
    } else if (extension.name == RtpExtension::kTransportSequenceNumber) {
      bool registered = rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransportSequenceNumber, extension.id);
      RTC_DCHECK(registered);
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }
  // Configure bandwidth estimation.
  channel_proxy_->SetCongestionControlObjects(
      nullptr, nullptr, congestion_controller->packet_router());
  if (config.combined_audio_video_bwe) {
    if (UseSendSideBwe(config)) {
      remote_bitrate_estimator_ =
          congestion_controller->GetRemoteBitrateEstimator(true);
    } else {
      remote_bitrate_estimator_ =
          congestion_controller->GetRemoteBitrateEstimator(false);
    }
    RTC_DCHECK(remote_bitrate_estimator_);
  }
}

AudioReceiveStream::~AudioReceiveStream() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "~AudioReceiveStream: " << config_.ToString();
  channel_proxy_->SetCongestionControlObjects(nullptr, nullptr, nullptr);
  if (remote_bitrate_estimator_) {
    remote_bitrate_estimator_->RemoveStream(config_.rtp.remote_ssrc);
  }
}

void AudioReceiveStream::Start() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioReceiveStream::Stop() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioReceiveStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

bool AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  return false;
}

bool AudioReceiveStream::DeliverRtp(const uint8_t* packet,
                                    size_t length,
                                    const PacketTime& packet_time) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  RTPHeader header;
  if (!rtp_header_parser_->Parse(packet, length, &header)) {
    return false;
  }

  // Only forward if the parsed header has one of the headers necessary for
  // bandwidth estimation. RTP timestamps has different rates for audio and
  // video and shouldn't be mixed.
  if (remote_bitrate_estimator_ &&
      (header.extension.hasAbsoluteSendTime ||
       header.extension.hasTransportSequenceNumber)) {
    int64_t arrival_time_ms = TickTime::MillisecondTimestamp();
    if (packet_time.timestamp >= 0)
      arrival_time_ms = (packet_time.timestamp + 500) / 1000;
    size_t payload_size = length - header.headerLength;
    remote_bitrate_estimator_->IncomingPacket(arrival_time_ms, payload_size,
                                              header, false);
  }
  return true;
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::AudioReceiveStream::Stats stats;
  stats.remote_ssrc = config_.rtp.remote_ssrc;
  ScopedVoEInterface<VoECodec> codec(voice_engine());

  webrtc::CallStatistics call_stats = channel_proxy_->GetRTCPStatistics();
  webrtc::CodecInst codec_inst = {0};
  if (codec->GetRecCodec(config_.voe_channel_id, codec_inst) == -1) {
    return stats;
  }

  stats.bytes_rcvd = call_stats.bytesReceived;
  stats.packets_rcvd = call_stats.packetsReceived;
  stats.packets_lost = call_stats.cumulativeLost;
  stats.fraction_lost = Q8ToFloat(call_stats.fractionLost);
  stats.capture_start_ntp_time_ms = call_stats.capture_start_ntp_time_ms_;
  if (codec_inst.pltype != -1) {
    stats.codec_name = codec_inst.plname;
  }
  stats.ext_seqnum = call_stats.extendedMax;
  if (codec_inst.plfreq / 1000 > 0) {
    stats.jitter_ms = call_stats.jitterSamples / (codec_inst.plfreq / 1000);
  }
  stats.delay_estimate_ms = channel_proxy_->GetDelayEstimate();
  stats.audio_level = channel_proxy_->GetSpeechOutputLevelFullRange();

  // Get jitter buffer and total delay (alg + jitter + playout) stats.
  auto ns = channel_proxy_->GetNetworkStatistics();
  stats.jitter_buffer_ms = ns.currentBufferSize;
  stats.jitter_buffer_preferred_ms = ns.preferredBufferSize;
  stats.expand_rate = Q14ToFloat(ns.currentExpandRate);
  stats.speech_expand_rate = Q14ToFloat(ns.currentSpeechExpandRate);
  stats.secondary_decoded_rate = Q14ToFloat(ns.currentSecondaryDecodedRate);
  stats.accelerate_rate = Q14ToFloat(ns.currentAccelerateRate);
  stats.preemptive_expand_rate = Q14ToFloat(ns.currentPreemptiveRate);

  auto ds = channel_proxy_->GetDecodingCallStatistics();
  stats.decoding_calls_to_silence_generator = ds.calls_to_silence_generator;
  stats.decoding_calls_to_neteq = ds.calls_to_neteq;
  stats.decoding_normal = ds.decoded_normal;
  stats.decoding_plc = ds.decoded_plc;
  stats.decoding_cng = ds.decoded_cng;
  stats.decoding_plc_cng = ds.decoded_plc_cng;

  return stats;
}

void AudioReceiveStream::SetSink(rtc::scoped_ptr<AudioSinkInterface> sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel_proxy_->SetSink(std::move(sink));
}

const webrtc::AudioReceiveStream::Config& AudioReceiveStream::config() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

VoiceEngine* AudioReceiveStream::voice_engine() const {
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  VoiceEngine* voice_engine = audio_state->voice_engine();
  RTC_DCHECK(voice_engine);
  return voice_engine;
}
}  // namespace internal
}  // namespace webrtc
