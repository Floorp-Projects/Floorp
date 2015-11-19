/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <map>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"

namespace webrtc {
const char* RtpExtension::kTOffset = "urn:ietf:params:rtp-hdrext:toffset";
const char* RtpExtension::kAbsSendTime =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
const char* RtpExtension::kVideoRotation = "urn:3gpp:video-orientation";

bool RtpExtension::IsSupported(const std::string& name) {
  return name == webrtc::RtpExtension::kTOffset ||
         name == webrtc::RtpExtension::kAbsSendTime ||
         name == webrtc::RtpExtension::kVideoRotation;
}

VideoEncoder* VideoEncoder::Create(VideoEncoder::EncoderType codec_type) {
  switch (codec_type) {
    case kVp8:
      return VP8Encoder::Create();
    case kVp9:
      return VP9Encoder::Create();
  }
  RTC_NOTREACHED();
  return nullptr;
}

VideoDecoder* VideoDecoder::Create(VideoDecoder::DecoderType codec_type) {
  switch (codec_type) {
    case kVp8:
      return VP8Decoder::Create();
    case kVp9:
      return VP9Decoder::Create();
  }
  RTC_NOTREACHED();
  return nullptr;
}

const int Call::Config::kDefaultStartBitrateBps = 300000;

namespace internal {

class CpuOveruseObserverProxy : public webrtc::CpuOveruseObserver {
 public:
  explicit CpuOveruseObserverProxy(LoadObserver* overuse_callback)
      : crit_(CriticalSectionWrapper::CreateCriticalSection()),
        overuse_callback_(overuse_callback) {
    DCHECK(overuse_callback != nullptr);
  }

  virtual ~CpuOveruseObserverProxy() {}

  void OveruseDetected() override {
    CriticalSectionScoped lock(crit_.get());
    overuse_callback_->OnLoadUpdate(LoadObserver::kOveruse);
  }

  void NormalUsage() override {
    CriticalSectionScoped lock(crit_.get());
    overuse_callback_->OnLoadUpdate(LoadObserver::kUnderuse);
  }

 private:
  const rtc::scoped_ptr<CriticalSectionWrapper> crit_;
  LoadObserver* overuse_callback_ GUARDED_BY(crit_);
};

class Call : public webrtc::Call, public PacketReceiver {
 public:
  Call(webrtc::VideoEngine* video_engine, const Call::Config& config);
  virtual ~Call();

  PacketReceiver* Receiver() override;

  VideoSendStream* CreateVideoSendStream(
      const VideoSendStream::Config& config,
      const VideoEncoderConfig& encoder_config) override;

  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  VideoReceiveStream* CreateVideoReceiveStream(
      const VideoReceiveStream::Config& config) override;

  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;

  Stats GetStats() const override;

  DeliveryStatus DeliverPacket(const uint8_t* packet, size_t length) override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SignalNetworkState(NetworkState state) override;

 private:
  DeliveryStatus DeliverRtcp(const uint8_t* packet, size_t length);
  DeliveryStatus DeliverRtp(const uint8_t* packet, size_t length);

  Call::Config config_;

  // Needs to be held while write-locking |receive_crit_| or |send_crit_|. This
  // ensures that we have a consistent network state signalled to all senders
  // and receivers.
  rtc::scoped_ptr<CriticalSectionWrapper> network_enabled_crit_;
  bool network_enabled_ GUARDED_BY(network_enabled_crit_);

  rtc::scoped_ptr<RWLockWrapper> receive_crit_;
  std::map<uint32_t, VideoReceiveStream*> receive_ssrcs_
      GUARDED_BY(receive_crit_);

  rtc::scoped_ptr<RWLockWrapper> send_crit_;
  std::map<uint32_t, VideoSendStream*> send_ssrcs_ GUARDED_BY(send_crit_);

  rtc::scoped_ptr<CpuOveruseObserverProxy> overuse_observer_proxy_;

  VideoSendStream::RtpStateMap suspended_send_ssrcs_;

  VideoEngine* video_engine_;
  ViERTP_RTCP* rtp_rtcp_;
  ViECodec* codec_;
  ViERender* render_;
  ViEBase* base_;
  ViENetwork* network_;
  int base_channel_id_;

  rtc::scoped_ptr<VideoRender> external_render_;

  DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

Call* Call::Create(const Call::Config& config) {
  VideoEngine* video_engine = config.webrtc_config != nullptr
                                  ? VideoEngine::Create(*config.webrtc_config)
                                  : VideoEngine::Create();
  DCHECK(video_engine != nullptr);

  return new internal::Call(video_engine, config);
}

namespace internal {

Call::Call(webrtc::VideoEngine* video_engine, const Call::Config& config)
    : config_(config),
      network_enabled_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      network_enabled_(true),
      receive_crit_(RWLockWrapper::CreateRWLock()),
      send_crit_(RWLockWrapper::CreateRWLock()),
      video_engine_(video_engine),
      base_channel_id_(-1),
      external_render_(
          VideoRender::CreateVideoRender(42, nullptr, false, kRenderExternal)) {
  DCHECK(video_engine != nullptr);
  DCHECK(config.send_transport != nullptr);

  DCHECK_GE(config.bitrate_config.min_bitrate_bps, 0);
  DCHECK_GE(config.bitrate_config.start_bitrate_bps,
            config.bitrate_config.min_bitrate_bps);
  if (config.bitrate_config.max_bitrate_bps != -1) {
    DCHECK_GE(config.bitrate_config.max_bitrate_bps,
              config.bitrate_config.start_bitrate_bps);
  }

  if (config.overuse_callback) {
    overuse_observer_proxy_.reset(
        new CpuOveruseObserverProxy(config.overuse_callback));
  }

  render_ = ViERender::GetInterface(video_engine_);
  DCHECK(render_ != nullptr);

  render_->RegisterVideoRenderModule(*external_render_.get());

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine_);
  DCHECK(rtp_rtcp_ != nullptr);

  codec_ = ViECodec::GetInterface(video_engine_);
  DCHECK(codec_ != nullptr);

  network_ = ViENetwork::GetInterface(video_engine_);

  // As a workaround for non-existing calls in the old API, create a base
  // channel used as default channel when creating send and receive streams.
  base_ = ViEBase::GetInterface(video_engine_);
  DCHECK(base_ != nullptr);

  base_->CreateChannel(base_channel_id_);
  DCHECK(base_channel_id_ != -1);

  network_->SetBitrateConfig(base_channel_id_,
                             config_.bitrate_config.min_bitrate_bps,
                             config_.bitrate_config.start_bitrate_bps,
                             config_.bitrate_config.max_bitrate_bps);
}

Call::~Call() {
  CHECK_EQ(0u, send_ssrcs_.size());
  CHECK_EQ(0u, receive_ssrcs_.size());
  base_->DeleteChannel(base_channel_id_);

  render_->DeRegisterVideoRenderModule(*external_render_.get());

  base_->Release();
  network_->Release();
  codec_->Release();
  render_->Release();
  rtp_rtcp_->Release();
  CHECK(webrtc::VideoEngine::Delete(video_engine_));
}

PacketReceiver* Call::Receiver() { return this; }

VideoSendStream* Call::CreateVideoSendStream(
    const VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoSendStream");
  LOG(LS_INFO) << "CreateVideoSendStream: " << config.ToString();
  DCHECK(!config.rtp.ssrcs.empty());

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  VideoSendStream* send_stream = new VideoSendStream(
      config_.send_transport, overuse_observer_proxy_.get(), video_engine_,
      config, encoder_config, suspended_send_ssrcs_, base_channel_id_);

  // This needs to be taken before send_crit_ as both locks need to be held
  // while changing network state.
  CriticalSectionScoped lock(network_enabled_crit_.get());
  WriteLockScoped write_lock(*send_crit_);
  for (size_t i = 0; i < config.rtp.ssrcs.size(); ++i) {
    DCHECK(send_ssrcs_.find(config.rtp.ssrcs[i]) == send_ssrcs_.end());
    send_ssrcs_[config.rtp.ssrcs[i]] = send_stream;
  }
  if (!network_enabled_)
    send_stream->SignalNetworkState(kNetworkDown);
  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoSendStream");
  DCHECK(send_stream != nullptr);

  send_stream->Stop();

  VideoSendStream* send_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*send_crit_);
    std::map<uint32_t, VideoSendStream*>::iterator it = send_ssrcs_.begin();
    while (it != send_ssrcs_.end()) {
      if (it->second == static_cast<VideoSendStream*>(send_stream)) {
        send_stream_impl = it->second;
        send_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
  }
  CHECK(send_stream_impl != nullptr);

  VideoSendStream::RtpStateMap rtp_state = send_stream_impl->GetRtpStates();

  for (VideoSendStream::RtpStateMap::iterator it = rtp_state.begin();
       it != rtp_state.end();
       ++it) {
    suspended_send_ssrcs_[it->first] = it->second;
  }

  delete send_stream_impl;
}

VideoReceiveStream* Call::CreateVideoReceiveStream(
    const VideoReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoReceiveStream");
  LOG(LS_INFO) << "CreateVideoReceiveStream: " << config.ToString();
  VideoReceiveStream* receive_stream =
      new VideoReceiveStream(video_engine_,
                             config,
                             config_.send_transport,
                             config_.voice_engine,
                             base_channel_id_);

  // This needs to be taken before receive_crit_ as both locks need to be held
  // while changing network state.
  CriticalSectionScoped lock(network_enabled_crit_.get());
  WriteLockScoped write_lock(*receive_crit_);
  DCHECK(receive_ssrcs_.find(config.rtp.remote_ssrc) == receive_ssrcs_.end());
  receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  // TODO(pbos): Configure different RTX payloads per receive payload.
  VideoReceiveStream::Config::Rtp::RtxMap::const_iterator it =
      config.rtp.rtx.begin();
  if (it != config.rtp.rtx.end())
    receive_ssrcs_[it->second.ssrc] = receive_stream;

  if (!network_enabled_)
    receive_stream->SignalNetworkState(kNetworkDown);
  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoReceiveStream");
  DCHECK(receive_stream != nullptr);

  VideoReceiveStream* receive_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    std::map<uint32_t, VideoReceiveStream*>::iterator it =
        receive_ssrcs_.begin();
    while (it != receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        if (receive_stream_impl != nullptr)
          DCHECK(receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        receive_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
  }
  CHECK(receive_stream_impl != nullptr);
  delete receive_stream_impl;
}

Call::Stats Call::GetStats() const {
  Stats stats;
  // Ignoring return values.
  uint32_t send_bandwidth = 0;
  rtp_rtcp_->GetEstimatedSendBandwidth(base_channel_id_, &send_bandwidth);
  stats.send_bandwidth_bps = send_bandwidth;
  uint32_t recv_bandwidth = 0;
  rtp_rtcp_->GetEstimatedReceiveBandwidth(base_channel_id_, &recv_bandwidth);
  stats.recv_bandwidth_bps = recv_bandwidth;
  {
    ReadLockScoped read_lock(*send_crit_);
    for (std::map<uint32_t, VideoSendStream*>::const_iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      stats.pacer_delay_ms =
          std::max(it->second->GetPacerQueuingDelayMs(), stats.pacer_delay_ms);
      int rtt_ms = it->second->GetRtt();
      if (rtt_ms > 0)
        stats.rtt_ms = rtt_ms;
    }
  }
  return stats;
}

void Call::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  TRACE_EVENT0("webrtc", "Call::SetBitrateConfig");
  DCHECK_GE(bitrate_config.min_bitrate_bps, 0);
  if (bitrate_config.max_bitrate_bps != -1)
    DCHECK_GT(bitrate_config.max_bitrate_bps, 0);
  if (config_.bitrate_config.min_bitrate_bps ==
          bitrate_config.min_bitrate_bps &&
      (bitrate_config.start_bitrate_bps <= 0 ||
       config_.bitrate_config.start_bitrate_bps ==
           bitrate_config.start_bitrate_bps) &&
      config_.bitrate_config.max_bitrate_bps ==
          bitrate_config.max_bitrate_bps) {
    // Nothing new to set, early abort to avoid encoder reconfigurations.
    return;
  }
  config_.bitrate_config = bitrate_config;
  network_->SetBitrateConfig(base_channel_id_, bitrate_config.min_bitrate_bps,
                             bitrate_config.start_bitrate_bps,
                             bitrate_config.max_bitrate_bps);
}

void Call::SignalNetworkState(NetworkState state) {
  // Take crit for entire function, it needs to be held while updating streams
  // to guarantee a consistent state across streams.
  CriticalSectionScoped lock(network_enabled_crit_.get());
  network_enabled_ = state == kNetworkUp;
  {
    ReadLockScoped write_lock(*send_crit_);
    for (std::map<uint32_t, VideoSendStream*>::iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      it->second->SignalNetworkState(state);
    }
  }
  {
    ReadLockScoped write_lock(*receive_crit_);
    for (std::map<uint32_t, VideoReceiveStream*>::iterator it =
             receive_ssrcs_.begin();
         it != receive_ssrcs_.end();
         ++it) {
      it->second->SignalNetworkState(state);
    }
  }
}

PacketReceiver::DeliveryStatus Call::DeliverRtcp(const uint8_t* packet,
                                                       size_t length) {
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  //             Return DELIVERY_UNKNOWN_SSRC if it can be determined that
  //             there's no receiver of the packet.
  bool rtcp_delivered = false;
  {
    ReadLockScoped read_lock(*receive_crit_);
    for (std::map<uint32_t, VideoReceiveStream*>::iterator it =
             receive_ssrcs_.begin();
         it != receive_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }

  {
    ReadLockScoped read_lock(*send_crit_);
    for (std::map<uint32_t, VideoSendStream*>::iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(const uint8_t* packet,
                                                size_t length) {
  // Minimum RTP header size.
  if (length < 12)
    return DELIVERY_PACKET_ERROR;

  uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&packet[8]);

  ReadLockScoped read_lock(*receive_crit_);
  std::map<uint32_t, VideoReceiveStream*>::iterator it =
      receive_ssrcs_.find(ssrc);

  if (it == receive_ssrcs_.end())
    return DELIVERY_UNKNOWN_SSRC;

  return it->second->DeliverRtp(packet, length) ? DELIVERY_OK
                                                : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(const uint8_t* packet,
                                                   size_t length) {
  if (RtpHeaderParser::IsRtcp(packet, length))
    return DeliverRtcp(packet, length);

  return DeliverRtp(packet, length);
}

}  // namespace internal
}  // namespace webrtc
