/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include <map>
#include <vector>

#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"

namespace webrtc {
const char* RtpExtension::kTOffset = "urn:ietf:params:rtp-hdrext:toffset";
const char* RtpExtension::kAbsSendTime =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
namespace internal {

class CpuOveruseObserverProxy : public webrtc::CpuOveruseObserver {
 public:
  CpuOveruseObserverProxy(OveruseCallback* overuse_callback)
      : crit_(CriticalSectionWrapper::CreateCriticalSection()),
        overuse_callback_(overuse_callback) {
    assert(overuse_callback != NULL);
  }

  virtual ~CpuOveruseObserverProxy() {}

  virtual void OveruseDetected() OVERRIDE {
    CriticalSectionScoped cs(crit_.get());
    overuse_callback_->OnOveruse();
  }

  virtual void NormalUsage() OVERRIDE {
    CriticalSectionScoped cs(crit_.get());
    overuse_callback_->OnNormalUse();
  }

 private:
  scoped_ptr<CriticalSectionWrapper> crit_;
  OveruseCallback* overuse_callback_;
};

class Call : public webrtc::Call, public PacketReceiver {
 public:
  Call(webrtc::VideoEngine* video_engine, const Call::Config& config);
  virtual ~Call();

  virtual PacketReceiver* Receiver() OVERRIDE;
  virtual std::vector<VideoCodec> GetVideoCodecs() OVERRIDE;

  virtual VideoSendStream::Config GetDefaultSendConfig() OVERRIDE;

  virtual VideoSendStream* CreateVideoSendStream(
      const VideoSendStream::Config& config) OVERRIDE;

  virtual void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream)
      OVERRIDE;

  virtual VideoReceiveStream::Config GetDefaultReceiveConfig() OVERRIDE;

  virtual VideoReceiveStream* CreateVideoReceiveStream(
      const VideoReceiveStream::Config& config) OVERRIDE;

  virtual void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) OVERRIDE;

  virtual uint32_t SendBitrateEstimate() OVERRIDE;
  virtual uint32_t ReceiveBitrateEstimate() OVERRIDE;

  virtual bool DeliverPacket(const uint8_t* packet, size_t length) OVERRIDE;

 private:
  bool DeliverRtcp(const uint8_t* packet, size_t length);
  bool DeliverRtp(const RTPHeader& header,
                  const uint8_t* packet,
                  size_t length);

  Call::Config config_;

  std::map<uint32_t, VideoReceiveStream*> receive_ssrcs_;
  scoped_ptr<RWLockWrapper> receive_lock_;

  std::map<uint32_t, VideoSendStream*> send_ssrcs_;
  scoped_ptr<RWLockWrapper> send_lock_;

  scoped_ptr<RtpHeaderParser> rtp_header_parser_;

  scoped_ptr<CpuOveruseObserverProxy> overuse_observer_proxy_;

  VideoEngine* video_engine_;
  ViERTP_RTCP* rtp_rtcp_;
  ViECodec* codec_;
  ViEBase* base_;
  int base_channel_id_;

  DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

class TraceDispatcher : public TraceCallback {
 public:
  TraceDispatcher()
      : lock_(CriticalSectionWrapper::CreateCriticalSection()),
        filter_(kTraceNone) {
    Trace::CreateTrace();
    VideoEngine::SetTraceCallback(this);
    VideoEngine::SetTraceFilter(kTraceNone);
  }

  ~TraceDispatcher() {
    Trace::ReturnTrace();
    VideoEngine::SetTraceCallback(NULL);
  }

  virtual void Print(TraceLevel level,
                     const char* message,
                     int length) OVERRIDE {
    CriticalSectionScoped crit(lock_.get());
    for (std::map<Call*, Call::Config*>::iterator it = callbacks_.begin();
         it != callbacks_.end();
         ++it) {
      if ((level & it->second->trace_filter) != kTraceNone)
        it->second->trace_callback->Print(level, message, length);
    }
  }

  void RegisterCallback(Call* call, Call::Config* config) {
    if (config->trace_callback == NULL)
      return;

    CriticalSectionScoped crit(lock_.get());
    callbacks_[call] = config;

    filter_ |= config->trace_filter;
    VideoEngine::SetTraceFilter(filter_);
  }

  void DeregisterCallback(Call* call) {
    CriticalSectionScoped crit(lock_.get());
    callbacks_.erase(call);

    filter_ = kTraceNone;
    for (std::map<Call*, Call::Config*>::iterator it = callbacks_.begin();
         it != callbacks_.end();
         ++it) {
      filter_ |= it->second->trace_filter;
    }

    VideoEngine::SetTraceFilter(filter_);
  }

 private:
  scoped_ptr<CriticalSectionWrapper> lock_;
  unsigned int filter_;
  std::map<Call*, Call::Config*> callbacks_;
};

namespace internal {
TraceDispatcher* global_trace_dispatcher = NULL;
}  // internal

void CreateTraceDispatcher() {
  if (internal::global_trace_dispatcher == NULL) {
    TraceDispatcher* dispatcher = new TraceDispatcher();
    // TODO(pbos): Atomic compare and exchange.
    if (internal::global_trace_dispatcher == NULL) {
      internal::global_trace_dispatcher = dispatcher;
    } else {
      delete dispatcher;
    }
  }
}

Call* Call::Create(const Call::Config& config) {
  CreateTraceDispatcher();

  VideoEngine* video_engine = config.webrtc_config != NULL
                                  ? VideoEngine::Create(*config.webrtc_config)
                                  : VideoEngine::Create();
  assert(video_engine != NULL);

  return new internal::Call(video_engine, config);
}

namespace internal {

Call::Call(webrtc::VideoEngine* video_engine, const Call::Config& config)
    : config_(config),
      receive_lock_(RWLockWrapper::CreateRWLock()),
      send_lock_(RWLockWrapper::CreateRWLock()),
      rtp_header_parser_(RtpHeaderParser::Create()),
      video_engine_(video_engine),
      base_channel_id_(-1) {
  assert(video_engine != NULL);
  assert(config.send_transport != NULL);

  if (config.overuse_callback) {
    overuse_observer_proxy_.reset(
        new CpuOveruseObserverProxy(config.overuse_callback));
  }

  global_trace_dispatcher->RegisterCallback(this, &config_);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine_);
  assert(rtp_rtcp_ != NULL);

  codec_ = ViECodec::GetInterface(video_engine_);
  assert(codec_ != NULL);

  // As a workaround for non-existing calls in the old API, create a base
  // channel used as default channel when creating send and receive streams.
  base_ = ViEBase::GetInterface(video_engine_);
  assert(base_ != NULL);

  base_->CreateChannel(base_channel_id_);
  assert(base_channel_id_ != -1);
}

Call::~Call() {
  global_trace_dispatcher->DeregisterCallback(this);
  base_->DeleteChannel(base_channel_id_);
  base_->Release();
  codec_->Release();
  rtp_rtcp_->Release();
  webrtc::VideoEngine::Delete(video_engine_);
}

PacketReceiver* Call::Receiver() { return this; }

std::vector<VideoCodec> Call::GetVideoCodecs() {
  std::vector<VideoCodec> codecs;

  VideoCodec codec;
  for (size_t i = 0; i < static_cast<size_t>(codec_->NumberOfCodecs()); ++i) {
    if (codec_->GetCodec(static_cast<unsigned char>(i), codec) == 0) {
      codecs.push_back(codec);
    }
  }
  return codecs;
}

VideoSendStream::Config Call::GetDefaultSendConfig() {
  VideoSendStream::Config config;
  codec_->GetCodec(0, config.codec);
  return config;
}

VideoSendStream* Call::CreateVideoSendStream(
    const VideoSendStream::Config& config) {
  assert(config.rtp.ssrcs.size() > 0);
  assert(config.rtp.ssrcs.size() >= config.codec.numberOfSimulcastStreams);

  VideoSendStream* send_stream = new VideoSendStream(
      config_.send_transport,
      overuse_observer_proxy_.get(),
      video_engine_,
      config,
      base_channel_id_);

  WriteLockScoped write_lock(*send_lock_);
  for (size_t i = 0; i < config.rtp.ssrcs.size(); ++i) {
    assert(send_ssrcs_.find(config.rtp.ssrcs[i]) == send_ssrcs_.end());
    send_ssrcs_[config.rtp.ssrcs[i]] = send_stream;
  }
  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  assert(send_stream != NULL);

  VideoSendStream* send_stream_impl = NULL;
  {
    WriteLockScoped write_lock(*send_lock_);
    for (std::map<uint32_t, VideoSendStream*>::iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      if (it->second == static_cast<VideoSendStream*>(send_stream)) {
        send_stream_impl = it->second;
        send_ssrcs_.erase(it);
        break;
      }
    }
  }

  assert(send_stream_impl != NULL);
  delete send_stream_impl;
}

VideoReceiveStream::Config Call::GetDefaultReceiveConfig() {
  VideoReceiveStream::Config config;
  config.rtp.remb = true;
  return config;
}

VideoReceiveStream* Call::CreateVideoReceiveStream(
    const VideoReceiveStream::Config& config) {
  VideoReceiveStream* receive_stream =
      new VideoReceiveStream(video_engine_,
                             config,
                             config_.send_transport,
                             config_.voice_engine,
                             base_channel_id_);

  WriteLockScoped write_lock(*receive_lock_);
  assert(receive_ssrcs_.find(config.rtp.remote_ssrc) == receive_ssrcs_.end());
  receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  // TODO(pbos): Configure different RTX payloads per receive payload.
  VideoReceiveStream::Config::Rtp::RtxMap::const_iterator it =
      config.rtp.rtx.begin();
  if (it != config.rtp.rtx.end())
    receive_ssrcs_[it->second.ssrc] = receive_stream;

  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  assert(receive_stream != NULL);

  VideoReceiveStream* receive_stream_impl = NULL;
  {
    WriteLockScoped write_lock(*receive_lock_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    std::map<uint32_t, VideoReceiveStream*>::iterator it =
        receive_ssrcs_.begin();
    while (it != receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        assert(receive_stream_impl == NULL ||
            receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        receive_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  assert(receive_stream_impl != NULL);
  delete receive_stream_impl;
}

uint32_t Call::SendBitrateEstimate() {
  // TODO(pbos): Return send-bitrate estimate
  return 0;
}

uint32_t Call::ReceiveBitrateEstimate() {
  // TODO(pbos): Return receive-bitrate estimate
  return 0;
}

bool Call::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  bool rtcp_delivered = false;
  {
    ReadLockScoped read_lock(*receive_lock_);
    for (std::map<uint32_t, VideoReceiveStream*>::iterator it =
             receive_ssrcs_.begin();
         it != receive_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }

  {
    ReadLockScoped read_lock(*send_lock_);
    for (std::map<uint32_t, VideoSendStream*>::iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  return rtcp_delivered;
}

bool Call::DeliverRtp(const RTPHeader& header,
                      const uint8_t* packet,
                      size_t length) {
  ReadLockScoped read_lock(*receive_lock_);
  std::map<uint32_t, VideoReceiveStream*>::iterator it =
      receive_ssrcs_.find(header.ssrc);
  if (it == receive_ssrcs_.end()) {
    // TODO(pbos): Log some warning, SSRC without receiver.
    return false;
  }
  return it->second->DeliverRtp(static_cast<const uint8_t*>(packet), length);
}

bool Call::DeliverPacket(const uint8_t* packet, size_t length) {
  // TODO(pbos): ExtensionMap if there are extensions.
  if (RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)))
    return DeliverRtcp(packet, length);

  RTPHeader rtp_header;
  if (!rtp_header_parser_->Parse(packet, static_cast<int>(length), &rtp_header))
    return false;

  return DeliverRtp(rtp_header, packet, length);
}

}  // namespace internal
}  // namespace webrtc
