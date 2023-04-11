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

#include "absl/strings/string_view.h"
#include "api/call/transport.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_types.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "media/base/media_channel.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/dscp.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/thread_annotations.h"
// This file contains the base classes for classes that implement
// the MediaChannel interfaces.
// These implementation classes used to be the exposed interface names,
// but this is in the process of being changed.
// TODO(bugs.webrtc.org/13931): Consider removing these classes.

namespace cricket {

class VoiceMediaChannel;
class VideoMediaChannel;

class MediaChannel : public MediaSendChannelInterface,
                     public MediaReceiveChannelInterface {
 public:
  explicit MediaChannel(webrtc::TaskQueueBase* network_thread,
                        bool enable_dscp = false);
  virtual ~MediaChannel();

  // Downcasting to the implemented interfaces.
  MediaSendChannelInterface* AsSendChannel() { return this; }

  MediaReceiveChannelInterface* AsReceiveChannel() { return this; }

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
  void OnPacketReceived(rtc::CopyOnWriteBuffer packet,
                        int64_t packet_time_us) override = 0;
  void OnPacketSent(const rtc::SentPacket& sent_packet) override = 0;
  void OnReadyToSend(bool ready) override = 0;
  void OnNetworkRouteChanged(absl::string_view transport_name,
                             const rtc::NetworkRoute& network_route) override =
      0;

  // Sets the abstract interface class for sending RTP/RTCP data.
  virtual void SetInterface(MediaChannelNetworkInterface* iface);
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

  // Corresponds to the SDP attribute extmap-allow-mixed, see RFC8285.
  // Set to true if it's allowed to mix one- and two-byte RTP header extensions
  // in the same stream. The setter and getter must only be called from
  // worker_thread.
  void SetExtmapAllowMixed(bool extmap_allow_mixed) override;
  bool ExtmapAllowMixed() const override;

  // Returns `true` if a non-null MediaChannelNetworkInterface pointer is held.
  // Must be called on the network thread.
  bool HasNetworkInterface() const;

  void SetFrameEncryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameEncryptorInterface>
                             frame_encryptor) override;
  void SetFrameDecryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override;

  void SetEncoderToPacketizerFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;
  void SetDepacketizerToDecoderFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;

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

// Base class for implementation classes

class VideoMediaChannel : public MediaChannel,
                          public VideoMediaSendChannelInterface,
                          public VideoMediaReceiveChannelInterface {
 public:
  explicit VideoMediaChannel(webrtc::TaskQueueBase* network_thread,
                             bool enable_dscp = false)
      : MediaChannel(network_thread, enable_dscp) {}
  ~VideoMediaChannel() override {}

  // Downcasting to the implemented interfaces.
  VideoMediaSendChannelInterface* AsVideoSendChannel() override { return this; }

  VideoMediaReceiveChannelInterface* AsVideoReceiveChannel() override {
    return this;
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
  // This fills the "bitrate parts" (rtx, video bitrate) of the
  // BandwidthEstimationInfo, since that part that isn't possible to get
  // through webrtc::Call::GetStats, as they are statistics of the send
  // streams.
  // TODO(holmer): We should change this so that either BWE graphs doesn't
  // need access to bitrates of the streams, or change the (RTC)StatsCollector
  // so that it's getting the send stream stats separately by calling
  // GetStats(), and merges with BandwidthEstimationInfo by itself.
  virtual void FillBitrateInfo(BandwidthEstimationInfo* bwe_info) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VideoMediaInfo* info) = 0;
  // Enable network condition based codec switching.
  void SetVideoCodecSwitchingEnabled(bool enabled) override;
};

// Base class for implementation classes
class VoiceMediaChannel : public MediaChannel,
                          public VoiceMediaSendChannelInterface,
                          public VoiceMediaReceiveChannelInterface {
 public:
  MediaType media_type() const override;
  VoiceMediaChannel(webrtc::TaskQueueBase* network_thread,
                    bool enable_dscp = false)
      : MediaChannel(network_thread, enable_dscp) {}
  ~VoiceMediaChannel() override {}

  // Downcasting to the implemented interfaces.
  VoiceMediaSendChannelInterface* AsVoiceSendChannel() override { return this; }

  VoiceMediaReceiveChannelInterface* AsVoiceReceiveChannel() override {
    return this;
  }

  VoiceMediaChannel* AsVoiceChannel() override { return this; }

  void SetExtmapAllowMixed(bool mixed) override {
    MediaChannel::SetExtmapAllowMixed(mixed);
  }
  bool ExtmapAllowMixed() const override {
    return MediaChannel::ExtmapAllowMixed();
  }

  // Gets quality stats for the channel.
  virtual bool GetStats(VoiceMediaInfo* info,
                        bool get_and_clear_legacy_stats) = 0;
};

}  // namespace cricket

#endif  // MEDIA_BASE_MEDIA_CHANNEL_IMPL_H_
