/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CHANNEL_H_
#define PC_CHANNEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "api/call/audio_sink.h"
#include "api/function_view.h"
#include "api/jsep.h"
#include "api/rtp_receiver_interface.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "call/rtp_packet_sink_interface.h"
#include "media/base/media_channel.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "pc/channel_interface.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/media_session.h"
#include "pc/rtp_transport.h"
#include "pc/srtp_filter.h"
#include "pc/srtp_transport.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/network.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"

namespace webrtc {
class AudioSinkInterface;
}  // namespace webrtc

namespace cricket {

struct CryptoParams;

// BaseChannel contains logic common to voice and video, including enable,
// marshaling calls to a worker and network threads, and connection and media
// monitors.
//
// BaseChannel assumes signaling and other threads are allowed to make
// synchronous calls to the worker thread, the worker thread makes synchronous
// calls only to the network thread, and the network thread can't be blocked by
// other threads.
// All methods with _n suffix must be called on network thread,
//     methods with _w suffix on worker thread
// and methods with _s suffix on signaling thread.
// Network and worker threads may be the same thread.
//
// WARNING! SUBCLASSES MUST CALL Deinit() IN THEIR DESTRUCTORS!
// This is required to avoid a data race between the destructor modifying the
// vtable, and the media channel's thread using BaseChannel as the
// NetworkInterface.

class BaseChannel : public ChannelInterface,
                    public rtc::MessageHandlerAutoCleanup,
                    public sigslot::has_slots<>,
                    public MediaChannel::NetworkInterface,
                    public webrtc::RtpPacketSinkInterface {
 public:
  // If |srtp_required| is true, the channel will not send or receive any
  // RTP/RTCP packets without using SRTP (either using SDES or DTLS-SRTP).
  // The BaseChannel does not own the UniqueRandomIdGenerator so it is the
  // responsibility of the user to ensure it outlives this object.
  // TODO(zhihuang:) Create a BaseChannel::Config struct for the parameter lists
  // which will make it easier to change the constructor.
  BaseChannel(rtc::Thread* worker_thread,
              rtc::Thread* network_thread,
              rtc::Thread* signaling_thread,
              std::unique_ptr<MediaChannel> media_channel,
              const std::string& content_name,
              bool srtp_required,
              webrtc::CryptoOptions crypto_options,
              rtc::UniqueRandomIdGenerator* ssrc_generator);
  virtual ~BaseChannel();
  virtual void Init_w(webrtc::RtpTransportInternal* rtp_transport);

  // Deinit may be called multiple times and is simply ignored if it's already
  // done.
  void Deinit();

  rtc::Thread* worker_thread() const { return worker_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }
  const std::string& content_name() const override { return content_name_; }
  // TODO(deadbeef): This is redundant; remove this.
  const std::string& transport_name() const override { return transport_name_; }
  bool enabled() const override { return enabled_; }

  // This function returns true if using SRTP (DTLS-based keying or SDES).
  bool srtp_active() const {
    RTC_DCHECK_RUN_ON(network_thread());
    return rtp_transport_ && rtp_transport_->IsSrtpActive();
  }

  // Version of the above that can be called from any thread.
  bool SrtpActiveForTesting() const {
    if (!network_thread_->IsCurrent()) {
      return network_thread_->Invoke<bool>(RTC_FROM_HERE,
                                           [this] { return srtp_active(); });
    }
    RTC_DCHECK_RUN_ON(network_thread());
    return srtp_active();
  }
  // Set an RTP level transport which could be an RtpTransport without
  // encryption, an SrtpTransport for SDES or a DtlsSrtpTransport for DTLS-SRTP.
  // This can be called from any thread and it hops to the network thread
  // internally. It would replace the |SetTransports| and its variants.
  bool SetRtpTransport(webrtc::RtpTransportInternal* rtp_transport) override;

  webrtc::RtpTransportInternal* rtp_transport() const {
    RTC_DCHECK_RUN_ON(network_thread());
    return rtp_transport_;
  }

  // Version of the above that can be called from any thread.
  webrtc::RtpTransportInternal* RtpTransportForTesting() const {
    if (!network_thread_->IsCurrent()) {
      return network_thread_->Invoke<webrtc::RtpTransportInternal*>(
          RTC_FROM_HERE, [this] { return rtp_transport(); });
    }
    RTC_DCHECK_RUN_ON(network_thread());
    return rtp_transport();
  }

  // Channel control. Must call UpdateRtpTransport afterwards to apply any
  // changes to the RtpTransport on the network thread.
  bool SetLocalContent(const MediaContentDescription* content,
                       webrtc::SdpType type,
                       std::string* error_desc) override;
  bool SetRemoteContent(const MediaContentDescription* content,
                        webrtc::SdpType type,
                        std::string* error_desc) override;
  // Controls whether this channel will receive packets on the basis of
  // matching payload type alone. This is needed for legacy endpoints that
  // don't signal SSRCs or use MID/RID, but doesn't make sense if there is
  // more than channel of specific media type, As that creates an ambiguity.
  //
  // This method will also remove any existing streams that were bound to this
  // channel on the basis of payload type, since one of these streams might
  // actually belong to a new channel. See: crbug.com/webrtc/11477
  //
  // As with SetLocalContent/SetRemoteContent, must call UpdateRtpTransport
  // afterwards to apply changes to the RtpTransport on the network thread.
  void SetPayloadTypeDemuxingEnabled(bool enabled) override;
  bool UpdateRtpTransport(std::string* error_desc) override;

  bool Enable(bool enable) override;

  const std::vector<StreamParams>& local_streams() const override {
    return local_streams_;
  }
  const std::vector<StreamParams>& remote_streams() const override {
    return remote_streams_;
  }

  // Used for latency measurements.
  sigslot::signal1<ChannelInterface*>& SignalFirstPacketReceived() override;

  // Forward SignalSentPacket to worker thread.
  sigslot::signal1<const rtc::SentPacket&>& SignalSentPacket();

  // From RtpTransport - public for testing only
  void OnTransportReadyToSend(bool ready);

  // Only public for unit tests.  Otherwise, consider protected.
  int SetOption(SocketType type, rtc::Socket::Option o, int val) override;
  int SetOption_n(SocketType type, rtc::Socket::Option o, int val)
      RTC_RUN_ON(network_thread());

  // RtpPacketSinkInterface overrides.
  void OnRtpPacket(const webrtc::RtpPacketReceived& packet) override;

  // Used by the RTCStatsCollector tests to set the transport name without
  // creating RtpTransports.
  void set_transport_name_for_testing(const std::string& transport_name) {
    transport_name_ = transport_name;
  }

  MediaChannel* media_channel() const override {
    return media_channel_.get();
  }

 protected:
  bool was_ever_writable() const {
    RTC_DCHECK_RUN_ON(worker_thread());
    return was_ever_writable_;
  }
  void set_local_content_direction(webrtc::RtpTransceiverDirection direction) {
    RTC_DCHECK_RUN_ON(worker_thread());
    local_content_direction_ = direction;
  }
  void set_remote_content_direction(webrtc::RtpTransceiverDirection direction) {
    RTC_DCHECK_RUN_ON(worker_thread());
    remote_content_direction_ = direction;
  }
  // These methods verify that:
  // * The required content description directions have been set.
  // * The channel is enabled.
  // * And for sending:
  //   - The SRTP filter is active if it's needed.
  //   - The transport has been writable before, meaning it should be at least
  //     possible to succeed in sending a packet.
  //
  // When any of these properties change, UpdateMediaSendRecvState_w should be
  // called.
  bool IsReadyToReceiveMedia_w() const RTC_RUN_ON(worker_thread());
  bool IsReadyToSendMedia_w() const RTC_RUN_ON(worker_thread());
  rtc::Thread* signaling_thread() const { return signaling_thread_; }

  void FlushRtcpMessages_n() RTC_RUN_ON(network_thread());

  // NetworkInterface implementation, called by MediaEngine
  bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options) override;
  bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                const rtc::PacketOptions& options) override;

  // From RtpTransportInternal
  void OnWritableState(bool writable);

  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route);

  bool PacketIsRtcp(const rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t len);
  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options);

  void EnableMedia_w() RTC_RUN_ON(worker_thread());
  void DisableMedia_w() RTC_RUN_ON(worker_thread());

  // Performs actions if the RTP/RTCP writable state changed. This should
  // be called whenever a channel's writable state changes or when RTCP muxing
  // becomes active/inactive.
  void UpdateWritableState_n() RTC_RUN_ON(network_thread());
  void ChannelWritable_n() RTC_RUN_ON(network_thread());
  void ChannelNotWritable_n() RTC_RUN_ON(network_thread());

  bool AddRecvStream_w(const StreamParams& sp) RTC_RUN_ON(worker_thread());
  bool RemoveRecvStream_w(uint32_t ssrc) RTC_RUN_ON(worker_thread());
  void ResetUnsignaledRecvStream_w() RTC_RUN_ON(worker_thread());
  void SetPayloadTypeDemuxingEnabled_w(bool enabled)
      RTC_RUN_ON(worker_thread());
  bool AddSendStream_w(const StreamParams& sp) RTC_RUN_ON(worker_thread());
  bool RemoveSendStream_w(uint32_t ssrc) RTC_RUN_ON(worker_thread());

  // Should be called whenever the conditions for
  // IsReadyToReceiveMedia/IsReadyToSendMedia are satisfied (or unsatisfied).
  // Updates the send/recv state of the media channel.
  virtual void UpdateMediaSendRecvState_w() = 0;

  bool UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                            webrtc::SdpType type,
                            std::string* error_desc)
      RTC_RUN_ON(worker_thread());
  bool UpdateRemoteStreams_w(const std::vector<StreamParams>& streams,
                             webrtc::SdpType type,
                             std::string* error_desc)
      RTC_RUN_ON(worker_thread());
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 webrtc::SdpType type,
                                 std::string* error_desc) = 0;
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  webrtc::SdpType type,
                                  std::string* error_desc) = 0;
  // Return a list of RTP header extensions with the non-encrypted extensions
  // removed depending on the current crypto_options_ and only if both the
  // non-encrypted and encrypted extension is present for the same URI.
  RtpHeaderExtensions GetFilteredRtpHeaderExtensions(
      const RtpHeaderExtensions& extensions);
  // Set a list of RTP extensions we should prepare to receive on the next
  // UpdateRtpTransport call.
  void SetReceiveExtensions(const RtpHeaderExtensions& extensions);

  // From MessageHandler
  void OnMessage(rtc::Message* pmsg) override;

  // Helper function template for invoking methods on the worker thread.
  template <class T>
  T InvokeOnWorker(const rtc::Location& posted_from,
                   rtc::FunctionView<T()> functor) {
    return worker_thread_->Invoke<T>(posted_from, functor);
  }

  // Add |payload_type| to |demuxer_criteria_| if payload type demuxing is
  // enabled.
  void MaybeAddHandledPayloadType(int payload_type) RTC_RUN_ON(worker_thread());

  void ClearHandledPayloadTypes() RTC_RUN_ON(worker_thread());
  // Return description of media channel to facilitate logging
  std::string ToString() const;

  void SetNegotiatedHeaderExtensions_w(const RtpHeaderExtensions& extensions);

  // ChannelInterface overrides
  RtpHeaderExtensions GetNegotiatedRtpHeaderExtensions() const override;

  bool has_received_packet_ = false;

 private:
  bool ConnectToRtpTransport();
  void DisconnectFromRtpTransport();
  void SignalSentPacket_n(const rtc::SentPacket& sent_packet)
      RTC_RUN_ON(network_thread());

  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  rtc::Thread* const signaling_thread_;
  rtc::AsyncInvoker invoker_;
  sigslot::signal1<ChannelInterface*> SignalFirstPacketReceived_
      RTC_GUARDED_BY(signaling_thread_);
  sigslot::signal1<const rtc::SentPacket&> SignalSentPacket_
      RTC_GUARDED_BY(worker_thread_);

  const std::string content_name_;

  // Won't be set when using raw packet transports. SDP-specific thing.
  // TODO(bugs.webrtc.org/12230): Written on network thread, read on
  // worker thread (at least).
  std::string transport_name_;

  webrtc::RtpTransportInternal* rtp_transport_
      RTC_GUARDED_BY(network_thread()) = nullptr;

  std::vector<std::pair<rtc::Socket::Option, int> > socket_options_
      RTC_GUARDED_BY(network_thread());
  std::vector<std::pair<rtc::Socket::Option, int> > rtcp_socket_options_
      RTC_GUARDED_BY(network_thread());
  bool writable_ RTC_GUARDED_BY(network_thread()) = false;
  bool was_ever_writable_n_ RTC_GUARDED_BY(network_thread()) = false;
  bool was_ever_writable_ RTC_GUARDED_BY(worker_thread()) = false;
  const bool srtp_required_ = true;
  const webrtc::CryptoOptions crypto_options_;

  // MediaChannel related members that should be accessed from the worker
  // thread.
  const std::unique_ptr<MediaChannel> media_channel_;
  // Currently the |enabled_| flag is accessed from the signaling thread as
  // well, but it can be changed only when signaling thread does a synchronous
  // call to the worker thread, so it should be safe.
  bool enabled_ = false;
  bool payload_type_demuxing_enabled_ RTC_GUARDED_BY(worker_thread()) = true;
  std::vector<StreamParams> local_streams_ RTC_GUARDED_BY(worker_thread());
  std::vector<StreamParams> remote_streams_ RTC_GUARDED_BY(worker_thread());
  // TODO(bugs.webrtc.org/12230): local_content_direction and
  // remote_content_direction are set on the worker thread, but accessed on the
  // network thread.
  webrtc::RtpTransceiverDirection local_content_direction_ =
      webrtc::RtpTransceiverDirection::kInactive;
  webrtc::RtpTransceiverDirection remote_content_direction_ =
      webrtc::RtpTransceiverDirection::kInactive;

  // Cached list of payload types, used if payload type demuxing is re-enabled.
  std::set<uint8_t> payload_types_ RTC_GUARDED_BY(worker_thread());
  // TODO(bugs.webrtc.org/12239): These two variables are modified on the worker
  // thread, accessed on the network thread in UpdateRtpTransport.
  webrtc::RtpDemuxerCriteria demuxer_criteria_;
  RtpHeaderExtensions receive_rtp_header_extensions_;
  // This generator is used to generate SSRCs for local streams.
  // This is needed in cases where SSRCs are not negotiated or set explicitly
  // like in Simulcast.
  // This object is not owned by the channel so it must outlive it.
  rtc::UniqueRandomIdGenerator* const ssrc_generator_;

  RtpHeaderExtensions negotiated_header_extensions_
      RTC_GUARDED_BY(signaling_thread());
};

// VoiceChannel is a specialization that adds support for early media, DTMF,
// and input/output level monitoring.
class VoiceChannel : public BaseChannel {
 public:
  VoiceChannel(rtc::Thread* worker_thread,
               rtc::Thread* network_thread,
               rtc::Thread* signaling_thread,
               std::unique_ptr<VoiceMediaChannel> channel,
               const std::string& content_name,
               bool srtp_required,
               webrtc::CryptoOptions crypto_options,
               rtc::UniqueRandomIdGenerator* ssrc_generator);
  ~VoiceChannel();

  // downcasts a MediaChannel
  VoiceMediaChannel* media_channel() const override {
    return static_cast<VoiceMediaChannel*>(BaseChannel::media_channel());
  }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }
  void Init_w(webrtc::RtpTransportInternal* rtp_transport) override;

 private:
  // overrides from BaseChannel
  void UpdateMediaSendRecvState_w() override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string* error_desc) override;

  // Last AudioSendParameters sent down to the media_channel() via
  // SetSendParameters.
  AudioSendParameters last_send_params_;
  // Last AudioRecvParameters sent down to the media_channel() via
  // SetRecvParameters.
  AudioRecvParameters last_recv_params_;
};

// VideoChannel is a specialization for video.
class VideoChannel : public BaseChannel {
 public:
  VideoChannel(rtc::Thread* worker_thread,
               rtc::Thread* network_thread,
               rtc::Thread* signaling_thread,
               std::unique_ptr<VideoMediaChannel> media_channel,
               const std::string& content_name,
               bool srtp_required,
               webrtc::CryptoOptions crypto_options,
               rtc::UniqueRandomIdGenerator* ssrc_generator);
  ~VideoChannel();

  // downcasts a MediaChannel
  VideoMediaChannel* media_channel() const override {
    return static_cast<VideoMediaChannel*>(BaseChannel::media_channel());
  }

  void FillBitrateInfo(BandwidthEstimationInfo* bwe_info);

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }

 private:
  // overrides from BaseChannel
  void UpdateMediaSendRecvState_w() override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string* error_desc) override;

  // Last VideoSendParameters sent down to the media_channel() via
  // SetSendParameters.
  VideoSendParameters last_send_params_;
  // Last VideoRecvParameters sent down to the media_channel() via
  // SetRecvParameters.
  VideoRecvParameters last_recv_params_;
};

// RtpDataChannel is a specialization for data.
class RtpDataChannel : public BaseChannel {
 public:
  RtpDataChannel(rtc::Thread* worker_thread,
                 rtc::Thread* network_thread,
                 rtc::Thread* signaling_thread,
                 std::unique_ptr<DataMediaChannel> channel,
                 const std::string& content_name,
                 bool srtp_required,
                 webrtc::CryptoOptions crypto_options,
                 rtc::UniqueRandomIdGenerator* ssrc_generator);
  ~RtpDataChannel();
  // TODO(zhihuang): Remove this once the RtpTransport can be shared between
  // BaseChannels.
  void Init_w(DtlsTransportInternal* rtp_dtls_transport,
              DtlsTransportInternal* rtcp_dtls_transport,
              rtc::PacketTransportInternal* rtp_packet_transport,
              rtc::PacketTransportInternal* rtcp_packet_transport);
  void Init_w(webrtc::RtpTransportInternal* rtp_transport) override;

  virtual bool SendData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer& payload,
                        SendDataResult* result);

  // Should be called on the signaling thread only.
  bool ready_to_send_data() const { return ready_to_send_data_; }

  sigslot::signal2<const ReceiveDataParams&, const rtc::CopyOnWriteBuffer&>
      SignalDataReceived;
  // Signal for notifying when the channel becomes ready to send data.
  // That occurs when the channel is enabled, the transport is writable,
  // both local and remote descriptions are set, and the channel is unblocked.
  sigslot::signal1<bool> SignalReadyToSendData;
  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_DATA;
  }

 protected:
  // downcasts a MediaChannel.
  DataMediaChannel* media_channel() const override {
    return static_cast<DataMediaChannel*>(BaseChannel::media_channel());
  }

 private:
  struct SendDataMessageData : public rtc::MessageData {
    SendDataMessageData(const SendDataParams& params,
                        const rtc::CopyOnWriteBuffer* payload,
                        SendDataResult* result)
        : params(params), payload(payload), result(result), succeeded(false) {}

    const SendDataParams& params;
    const rtc::CopyOnWriteBuffer* payload;
    SendDataResult* result;
    bool succeeded;
  };

  struct DataReceivedMessageData : public rtc::MessageData {
    // We copy the data because the data will become invalid after we
    // handle DataMediaChannel::SignalDataReceived but before we fire
    // SignalDataReceived.
    DataReceivedMessageData(const ReceiveDataParams& params,
                            const char* data,
                            size_t len)
        : params(params), payload(data, len) {}
    const ReceiveDataParams params;
    const rtc::CopyOnWriteBuffer payload;
  };

  typedef rtc::TypedMessageData<bool> DataChannelReadyToSendMessageData;

  // overrides from BaseChannel
  // Checks that data channel type is RTP.
  bool CheckDataChannelTypeFromContent(const MediaContentDescription* content,
                                       std::string* error_desc);
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string* error_desc) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string* error_desc) override;
  void UpdateMediaSendRecvState_w() override;

  void OnMessage(rtc::Message* pmsg) override;
  void OnDataReceived(const ReceiveDataParams& params,
                      const char* data,
                      size_t len);
  void OnDataChannelReadyToSend(bool writable);

  bool ready_to_send_data_ = false;

  // Last DataSendParameters sent down to the media_channel() via
  // SetSendParameters.
  DataSendParameters last_send_params_;
  // Last DataRecvParameters sent down to the media_channel() via
  // SetRecvParameters.
  DataRecvParameters last_recv_params_;
};

}  // namespace cricket

#endif  // PC_CHANNEL_H_
