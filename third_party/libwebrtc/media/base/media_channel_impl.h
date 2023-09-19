/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIA_CHANNEL_IMPL_H_
#define MEDIA_BASE_MEDIA_CHANNEL_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/audio_options.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/stream_params.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/thread_annotations.h"
// This file contains the base classes for classes that implement
// the MediaChannel interfaces.
// These implementation classes used to be the exposed interface names,
// but this is in the process of being changed.
// TODO(bugs.webrtc.org/13931): Remove the MediaChannel class.

namespace cricket {

class VoiceMediaChannel;
class VideoMediaChannel;

// The `MediaChannelUtil` class provides functionality that is used by
// multiple MediaChannel-like objects, of both sending and receiving
// types.
class MediaChannelUtil {
 public:
  MediaChannelUtil(webrtc::TaskQueueBase* network_thread,
                   bool enable_dscp = false);
  virtual ~MediaChannelUtil();
  // Returns the absolute sendtime extension id value from media channel.
  virtual int GetRtpSendTimeExtnId() const;
  // Base method to send packet using MediaChannelNetworkInterface.
  bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options);

  bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                const rtc::PacketOptions& options);

  int SetOption(MediaChannelNetworkInterface::SocketType type,
                rtc::Socket::Option opt,
                int option);

  // Functions that form part of one or more interface classes.
  // Not marked override, since this class does not inherit from the
  // interfaces.

  // Corresponds to the SDP attribute extmap-allow-mixed, see RFC8285.
  // Set to true if it's allowed to mix one- and two-byte RTP header extensions
  // in the same stream. The setter and getter must only be called from
  // worker_thread.
  void SetExtmapAllowMixed(bool extmap_allow_mixed);
  bool ExtmapAllowMixed() const;

  void SetInterface(MediaChannelNetworkInterface* iface);
  // Returns `true` if a non-null MediaChannelNetworkInterface pointer is held.
  // Must be called on the network thread.
  bool HasNetworkInterface() const;

  void SetFrameEncryptor(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor);
  void SetFrameDecryptor(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor);

  void SetEncoderToPacketizerFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer);
  void SetDepacketizerToDecoderFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer);

 protected:
  int SetOptionLocked(MediaChannelNetworkInterface::SocketType type,
                      rtc::Socket::Option opt,
                      int option) RTC_RUN_ON(network_thread_);

  bool DscpEnabled() const;

  // This is the DSCP value used for both RTP and RTCP channels if DSCP is
  // enabled. It can be changed at any time via `SetPreferredDscp`.
  rtc::DiffServCodePoint PreferredDscp() const;
  void SetPreferredDscp(rtc::DiffServCodePoint new_dscp);

  rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> network_safety();

  // Utility implementation for derived classes (video/voice) that applies
  // the packet options and passes the data onwards to `SendPacket`.
  void SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options);

  void SendRtcp(const uint8_t* data, size_t len);

 private:
  // Apply the preferred DSCP setting to the underlying network interface RTP
  // and RTCP channels. If DSCP is disabled, then apply the default DSCP value.
  void UpdateDscp() RTC_RUN_ON(network_thread_);

  bool DoSendPacket(rtc::CopyOnWriteBuffer* packet,
                    bool rtcp,
                    const rtc::PacketOptions& options);

  const bool enable_dscp_;
  const rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> network_safety_
      RTC_PT_GUARDED_BY(network_thread_);
  webrtc::TaskQueueBase* const network_thread_;
  MediaChannelNetworkInterface* network_interface_
      RTC_GUARDED_BY(network_thread_) = nullptr;
  rtc::DiffServCodePoint preferred_dscp_ RTC_GUARDED_BY(network_thread_) =
      rtc::DSCP_DEFAULT;
  bool extmap_allow_mixed_ = false;
};

// The `MediaChannel` class implements both the SendChannel and
// ReceiveChannel interface. It is used in legacy code that does not
// use the split interfaces.
class MediaChannel : public MediaChannelUtil,
                     public MediaSendChannelInterface,
                     public MediaReceiveChannelInterface {
 public:
  // Role of the channel. Used to describe which interface it supports.
  // This is temporary until we stop using the same implementation for both
  // interfaces.
  enum class Role {
    kSend,
    kReceive,
    kBoth  // Temporary value for non-converted test and downstream code
    // TODO(bugs.webrtc.org/13931): Remove kBoth when usage is removed.
  };

  explicit MediaChannel(Role role,
                        webrtc::TaskQueueBase* network_thread,
                        bool enable_dscp = false);
  virtual ~MediaChannel() = default;

  Role role() const { return role_; }

  // Downcasting to the subclasses.
  virtual VideoMediaChannel* AsVideoChannel() {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  virtual VoiceMediaChannel* AsVoiceChannel() {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }
  // Must declare the methods inherited from the base interface template,
  // even when abstract, to tell the compiler that all instances of the name
  // referred to by subclasses of this share the same implementation.
  cricket::MediaType media_type() const override = 0;
  void OnPacketReceived(const webrtc::RtpPacketReceived& packet) override = 0;
  void OnPacketSent(const rtc::SentPacket& sent_packet) override = 0;
  void OnReadyToSend(bool ready) override = 0;
  void OnNetworkRouteChanged(absl::string_view transport_name,
                             const rtc::NetworkRoute& network_route) override =
      0;
  void SetSendCodecChangedCallback(
      absl::AnyInvocable<void()> callback) override = 0;

  // Methods from the APIs that are implemented in MediaChannelUtil
  using MediaChannelUtil::ExtmapAllowMixed;
  using MediaChannelUtil::HasNetworkInterface;
  using MediaChannelUtil::SetExtmapAllowMixed;
  using MediaChannelUtil::SetInterface;

 private:
  const Role role_;
};

// Base class for implementation classes

class VideoMediaChannel : public MediaChannel,
                          public VideoMediaSendChannelInterface,
                          public VideoMediaReceiveChannelInterface {
 public:
  explicit VideoMediaChannel(MediaChannel::Role role,
                             webrtc::TaskQueueBase* network_thread,
                             bool enable_dscp = false)
      : MediaChannel(role, network_thread, enable_dscp) {}
  ~VideoMediaChannel() override {}

  // Downcasting to the implemented interfaces.
  VideoMediaSendChannelInterface* AsVideoSendChannel() override { return this; }
  VoiceMediaSendChannelInterface* AsVoiceSendChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  VideoMediaReceiveChannelInterface* AsVideoReceiveChannel() override {
    return this;
  }
  VoiceMediaReceiveChannelInterface* AsVoiceReceiveChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }
  cricket::MediaType media_type() const override;

  // Downcasting to the subclasses.
  VideoMediaChannel* AsVideoChannel() override { return this; }

  void SetExtmapAllowMixed(bool mixed) override {
    MediaChannel::SetExtmapAllowMixed(mixed);
  }
  bool ExtmapAllowMixed() const override {
    return MediaChannel::ExtmapAllowMixed();
  }
  void SetInterface(MediaChannelNetworkInterface* iface) override {
    return MediaChannel::SetInterface(iface);
  }
  // Declared here in order to avoid "found by multiple paths" compile error
  bool AddSendStream(const StreamParams& sp) override = 0;
  void ChooseReceiverReportSsrc(const std::set<uint32_t>& choices) override = 0;
  void SetSsrcListChangedCallback(
      absl::AnyInvocable<void(const std::set<uint32_t>&)> callback) override =
      0;
  bool AddRecvStream(const StreamParams& sp) override = 0;
  void OnPacketReceived(const webrtc::RtpPacketReceived& packet) override = 0;
  void SetEncoderSelector(uint32_t ssrc,
                          webrtc::VideoEncoderFactory::EncoderSelectorInterface*
                              encoder_selector) override {}

  // This fills the "bitrate parts" (rtx, video bitrate) of the
  // BandwidthEstimationInfo, since that part that isn't possible to get
  // through webrtc::Call::GetStats, as they are statistics of the send
  // streams.
  // TODO(holmer): We should change this so that either BWE graphs doesn't
  // need access to bitrates of the streams, or change the (RTC)StatsCollector
  // so that it's getting the send stream stats separately by calling
  // GetStats(), and merges with BandwidthEstimationInfo by itself.
  void FillBitrateInfo(BandwidthEstimationInfo* bwe_info) override = 0;
  // Gets quality stats for the channel.
  virtual bool GetSendStats(VideoMediaSendInfo* info) = 0;
  virtual bool GetReceiveStats(VideoMediaReceiveInfo* info) = 0;
  bool GetStats(VideoMediaSendInfo* info) override {
    return GetSendStats(info);
  }
  bool GetStats(VideoMediaReceiveInfo* info) override {
    return GetReceiveStats(info);
  }

  // TODO(bugs.webrtc.org/13931): Remove when configuration is more sensible
  void SetSendCodecChangedCallback(
      absl::AnyInvocable<void()> callback) override = 0;
  // Enable network condition based codec switching.
  // Note: should have been pure virtual.
  void SetVideoCodecSwitchingEnabled(bool enabled) override;

 private:
  // Functions not implemented on this interface
  bool HasNetworkInterface() const override {
    return MediaChannel::HasNetworkInterface();
  }
  MediaChannel* ImplForTesting() override {
    // This class and its subclasses are not interface classes.
    RTC_CHECK_NOTREACHED();
  }
};

// Base class for implementation classes
class VoiceMediaChannel : public MediaChannel,
                          public VoiceMediaSendChannelInterface,
                          public VoiceMediaReceiveChannelInterface {
 public:
  MediaType media_type() const override;
  VoiceMediaChannel(MediaChannel::Role role,
                    webrtc::TaskQueueBase* network_thread,
                    bool enable_dscp = false)
      : MediaChannel(role, network_thread, enable_dscp) {}
  ~VoiceMediaChannel() override {}

  // Downcasting to the implemented interfaces.
  VoiceMediaSendChannelInterface* AsVoiceSendChannel() override { return this; }

  VoiceMediaReceiveChannelInterface* AsVoiceReceiveChannel() override {
    return this;
  }

  VoiceMediaChannel* AsVoiceChannel() override { return this; }

  VideoMediaSendChannelInterface* AsVideoSendChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }
  VideoMediaReceiveChannelInterface* AsVideoReceiveChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  // Declared here in order to avoid "found by multiple paths" compile error
  bool AddSendStream(const StreamParams& sp) override = 0;
  bool AddRecvStream(const StreamParams& sp) override = 0;
  void OnPacketReceived(const webrtc::RtpPacketReceived& packet) override = 0;
  void SetEncoderSelector(uint32_t ssrc,
                          webrtc::VideoEncoderFactory::EncoderSelectorInterface*
                              encoder_selector) override {}
  void ChooseReceiverReportSsrc(const std::set<uint32_t>& choices) override = 0;
  void SetSsrcListChangedCallback(
      absl::AnyInvocable<void(const std::set<uint32_t>&)> callback) override =
      0;
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const override = 0;
  webrtc::RTCError SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters,
      webrtc::SetParametersCallback callback = nullptr) override = 0;

  void SetExtmapAllowMixed(bool mixed) override {
    MediaChannel::SetExtmapAllowMixed(mixed);
  }
  bool ExtmapAllowMixed() const override {
    return MediaChannel::ExtmapAllowMixed();
  }
  void SetInterface(MediaChannelNetworkInterface* iface) override {
    return MediaChannel::SetInterface(iface);
  }
  bool HasNetworkInterface() const override {
    return MediaChannel::HasNetworkInterface();
  }

  // Gets quality stats for the channel.
  virtual bool GetSendStats(VoiceMediaSendInfo* info) = 0;
  virtual bool GetReceiveStats(VoiceMediaReceiveInfo* info,
                               bool get_and_clear_legacy_stats) = 0;
  bool GetStats(VoiceMediaSendInfo* info) override {
    return GetSendStats(info);
  }
  bool GetStats(VoiceMediaReceiveInfo* info,
                bool get_and_clear_legacy_stats) override {
    return GetReceiveStats(info, get_and_clear_legacy_stats);
  }

 private:
  // Functions not implemented on this interface
  MediaChannel* ImplForTesting() override {
    // This class and its subclasses are not interface classes.
    RTC_CHECK_NOTREACHED();
  }
};

}  // namespace cricket

#endif  // MEDIA_BASE_MEDIA_CHANNEL_IMPL_H_
