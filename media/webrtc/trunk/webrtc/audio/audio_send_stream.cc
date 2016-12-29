/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_send_stream.h"

#include <string>

#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/call/congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/voice_engine/channel_proxy.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {
std::string AudioSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrc: " << ssrc;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

std::string AudioSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", voe_channel_id: " << voe_channel_id;
  // TODO(solenberg): Encoder config.
  ss << ", cng_payload_type: " << cng_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << '}';
  return ss.str();
}

namespace internal {
AudioSendStream::AudioSendStream(
    const webrtc::AudioSendStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    CongestionController* congestion_controller)
    : config_(config), audio_state_(audio_state) {
  LOG(LS_INFO) << "AudioSendStream: " << config_.ToString();
  RTC_DCHECK_NE(config_.voe_channel_id, -1);
  RTC_DCHECK(audio_state_.get());
  RTC_DCHECK(congestion_controller);

  VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine());
  channel_proxy_ = voe_impl->GetChannelProxy(config_.voe_channel_id);
  channel_proxy_->SetCongestionControlObjects(
      congestion_controller->pacer(),
      congestion_controller->GetTransportFeedbackObserver(),
      congestion_controller->packet_router());
  channel_proxy_->SetRTCPStatus(true);
  channel_proxy_->SetLocalSSRC(config.rtp.ssrc);
  channel_proxy_->SetRTCP_CNAME(config.rtp.c_name);

  for (const auto& extension : config.rtp.extensions) {
    if (extension.name == RtpExtension::kAbsSendTime) {
      channel_proxy_->SetSendAbsoluteSenderTimeStatus(true, extension.id);
    } else if (extension.name == RtpExtension::kAudioLevel) {
      channel_proxy_->SetSendAudioLevelIndicationStatus(true, extension.id);
    } else if (extension.name == RtpExtension::kTransportSequenceNumber) {
      channel_proxy_->EnableSendTransportSequenceNumber(extension.id);
    } else {
      RTC_NOTREACHED() << "Registering unsupported RTP extension.";
    }
  }
}

AudioSendStream::~AudioSendStream() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "~AudioSendStream: " << config_.ToString();
  channel_proxy_->SetCongestionControlObjects(nullptr, nullptr, nullptr);
}

void AudioSendStream::Start() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioSendStream::Stop() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioSendStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

bool AudioSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  return false;
}

bool AudioSendStream::SendTelephoneEvent(int payload_type, uint8_t event,
                                         uint32_t duration_ms) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel_proxy_->SetSendTelephoneEventPayloadType(payload_type) &&
         channel_proxy_->SendTelephoneEventOutband(event, duration_ms);
}

webrtc::AudioSendStream::Stats AudioSendStream::GetStats() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::AudioSendStream::Stats stats;
  stats.local_ssrc = config_.rtp.ssrc;
  ScopedVoEInterface<VoEAudioProcessing> processing(voice_engine());
  ScopedVoEInterface<VoECodec> codec(voice_engine());
  ScopedVoEInterface<VoEVolumeControl> volume(voice_engine());

  webrtc::CallStatistics call_stats = channel_proxy_->GetRTCPStatistics();
  stats.bytes_sent = call_stats.bytesSent;
  stats.packets_sent = call_stats.packetsSent;
  // RTT isn't known until a RTCP report is received. Until then, VoiceEngine
  // returns 0 to indicate an error value.
  if (call_stats.rttMs > 0) {
    stats.rtt_ms = call_stats.rttMs;
  }
  // TODO(solenberg): [was ajm]: Re-enable this metric once we have a reliable
  //                  implementation.
  stats.aec_quality_min = -1;

  webrtc::CodecInst codec_inst = {0};
  if (codec->GetSendCodec(config_.voe_channel_id, codec_inst) != -1) {
    RTC_DCHECK_NE(codec_inst.pltype, -1);
    stats.codec_name = codec_inst.plname;

    // Get data from the last remote RTCP report.
    for (const auto& block : channel_proxy_->GetRemoteRTCPReportBlocks()) {
      // Lookup report for send ssrc only.
      if (block.source_SSRC == stats.local_ssrc) {
        stats.packets_lost = block.cumulative_num_packets_lost;
        stats.fraction_lost = Q8ToFloat(block.fraction_lost);
        stats.ext_seqnum = block.extended_highest_sequence_number;
        // Convert samples to milliseconds.
        if (codec_inst.plfreq / 1000 > 0) {
          stats.jitter_ms =
              block.interarrival_jitter / (codec_inst.plfreq / 1000);
        }
        break;
      }
    }
  }

  // Local speech level.
  {
    unsigned int level = 0;
    int error = volume->GetSpeechInputLevelFullRange(level);
    RTC_DCHECK_EQ(0, error);
    stats.audio_level = static_cast<int32_t>(level);
  }

  bool echo_metrics_on = false;
  int error = processing->GetEcMetricsStatus(echo_metrics_on);
  RTC_DCHECK_EQ(0, error);
  if (echo_metrics_on) {
    // These can also be negative, but in practice -1 is only used to signal
    // insufficient data, since the resolution is limited to multiples of 4 ms.
    int median = -1;
    int std = -1;
    float dummy = 0.0f;
    error = processing->GetEcDelayMetrics(median, std, dummy);
    RTC_DCHECK_EQ(0, error);
    stats.echo_delay_median_ms = median;
    stats.echo_delay_std_ms = std;

    // These can take on valid negative values, so use the lowest possible level
    // as default rather than -1.
    int erl = -100;
    int erle = -100;
    int dummy1 = 0;
    int dummy2 = 0;
    error = processing->GetEchoMetrics(erl, erle, dummy1, dummy2);
    RTC_DCHECK_EQ(0, error);
    stats.echo_return_loss = erl;
    stats.echo_return_loss_enhancement = erle;
  }

  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  stats.typing_noise_detected = audio_state->typing_noise_detected();

  return stats;
}

const webrtc::AudioSendStream::Config& AudioSendStream::config() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

VoiceEngine* AudioSendStream::voice_engine() const {
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  VoiceEngine* voice_engine = audio_state->voice_engine();
  RTC_DCHECK(voice_engine);
  return voice_engine;
}
}  // namespace internal
}  // namespace webrtc
