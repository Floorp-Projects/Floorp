/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEER_CONNECTION_H_
#define PC_PEER_CONNECTION_H_

#include <stdint.h>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/async_resolver_factory.h"
#include "api/audio_options.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/data_channel_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/packet_socket_factory.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/enums.h"
#include "api/turn_customizer.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "call/call.h"
#include "media/base/media_channel.h"
#include "media/base/media_engine.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "pc/channel.h"
#include "pc/channel_interface.h"
#include "pc/channel_manager.h"
#include "pc/connection_context.h"
#include "pc/data_channel_controller.h"
#include "pc/data_channel_utils.h"
#include "pc/dtls_transport.h"
#include "pc/jsep_transport_controller.h"
#include "pc/peer_connection_internal.h"
#include "pc/peer_connection_message_handler.h"
#include "pc/rtc_stats_collector.h"
#include "pc/rtp_data_channel.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_transceiver.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/sdp_offer_answer.h"
#include "pc/session_description.h"
#include "pc/stats_collector.h"
#include "pc/stream_collection.h"
#include "pc/transceiver_list.h"
#include "pc/transport_stats.h"
#include "pc/usage_pattern.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"

namespace webrtc {

// PeerConnection is the implementation of the PeerConnection object as defined
// by the PeerConnectionInterface API surface.
// The class currently is solely responsible for the following:
// - Managing the session state machine (signaling state).
// - Creating and initializing lower-level objects, like PortAllocator and
//   BaseChannels.
// - Owning and managing the life cycle of the RtpSender/RtpReceiver and track
//   objects.
// - Tracking the current and pending local/remote session descriptions.
// The class currently is jointly responsible for the following:
// - Parsing and interpreting SDP.
// - Generating offers and answers based on the current state.
// - The ICE state machine.
// - Generating stats.
class PeerConnection : public PeerConnectionInternal,
                       public JsepTransportController::Observer,
                       public RtpSenderBase::SetStreamsObserver,
                       public sigslot::has_slots<> {
 public:
  explicit PeerConnection(rtc::scoped_refptr<ConnectionContext> context,
                          std::unique_ptr<RtcEventLog> event_log,
                          std::unique_ptr<Call> call);

  bool Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies);

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override;
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override;
  bool AddStream(MediaStreamInterface* local_stream) override;
  void RemoveStream(MediaStreamInterface* local_stream) override;

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) override;
  bool RemoveTrack(RtpSenderInterface* sender) override;
  RTCError RemoveTrackNew(
      rtc::scoped_refptr<RtpSenderInterface> sender) override;

  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      const RtpTransceiverInit& init) override;

  // Gets the DTLS SSL certificate associated with the audio transport on the
  // remote side. This will become populated once the DTLS connection with the
  // peer has been completed, as indicated by the ICE connection state
  // transitioning to kIceConnectionCompleted.
  // Deprecated - users should insted query the DTLS transpport for this info.
  // See https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface

  RTC_DEPRECATED std::unique_ptr<rtc::SSLCertificate>
  GetRemoteAudioSSLCertificate();

  // Version of the above method that returns the full certificate chain.
  RTC_DEPRECATED std::unique_ptr<rtc::SSLCertChain>
  GetRemoteAudioSSLCertChain();

  rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) override;

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const override;
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> GetTransceivers()
      const override;

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) override;
  // WARNING: LEGACY. See peerconnectioninterface.h
  bool GetStats(StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;
  // Spec-complaint GetStats(). See peerconnectioninterface.h
  void GetStats(RTCStatsCollectorCallback* callback) override;
  void GetStats(
      rtc::scoped_refptr<RtpSenderInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) override;
  void GetStats(
      rtc::scoped_refptr<RtpReceiverInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) override;
  void ClearStatsCache() override;

  SignalingState signaling_state() override;

  IceConnectionState ice_connection_state() override;
  IceConnectionState standardized_ice_connection_state() override;
  PeerConnectionState peer_connection_state() override;
  IceGatheringState ice_gathering_state() override;
  absl::optional<bool> can_trickle_ice_candidates() override;

  const SessionDescriptionInterface* local_description() const override;
  const SessionDescriptionInterface* remote_description() const override;
  const SessionDescriptionInterface* current_local_description() const override;
  const SessionDescriptionInterface* current_remote_description()
      const override;
  const SessionDescriptionInterface* pending_local_description() const override;
  const SessionDescriptionInterface* pending_remote_description()
      const override;

  void RestartIce() override;

  // JSEP01
  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;

  void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer)
      override;
  void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer)
      override;
  // TODO(https://crbug.com/webrtc/11798): Delete these methods in favor of the
  // ones taking SetLocalDescriptionObserverInterface as argument.
  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc) override;
  void SetLocalDescription(SetSessionDescriptionObserver* observer) override;

  void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer)
      override;
  // TODO(https://crbug.com/webrtc/11798): Delete this methods in favor of the
  // ones taking SetRemoteDescriptionObserverInterface as argument.
  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc) override;

  PeerConnectionInterface::RTCConfiguration GetConfiguration() override;
  RTCError SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration) override;
  bool AddIceCandidate(const IceCandidateInterface* candidate) override;
  void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                       std::function<void(RTCError)> callback) override;
  bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) override;

  RTCError SetBitrate(const BitrateSettings& bitrate) override;

  void SetAudioPlayout(bool playout) override;
  void SetAudioRecording(bool recording) override;

  rtc::scoped_refptr<DtlsTransportInterface> LookupDtlsTransportByMid(
      const std::string& mid) override;
  rtc::scoped_refptr<DtlsTransport> LookupDtlsTransportByMidInternal(
      const std::string& mid);

  rtc::scoped_refptr<SctpTransportInterface> GetSctpTransport() const override;

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override;

  bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output,
                        int64_t output_period_ms) override;
  bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output) override;
  void StopRtcEventLog() override;

  void Close() override;

  rtc::Thread* signaling_thread() const final {
    return context_->signaling_thread();
  }

  // PeerConnectionInternal implementation.
  rtc::Thread* network_thread() const final {
    return context_->network_thread();
  }
  rtc::Thread* worker_thread() const final { return context_->worker_thread(); }

  std::string session_id() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return session_id_;
  }

  bool initial_offerer() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return transport_controller_ && transport_controller_->initial_offerer();
  }

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetTransceiversInternal() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return transceivers_.List();
  }

  sigslot::signal1<RtpDataChannel*>& SignalRtpDataChannelCreated() override {
    return data_channel_controller_.SignalRtpDataChannelCreated();
  }

  sigslot::signal1<SctpDataChannel*>& SignalSctpDataChannelCreated() override {
    return data_channel_controller_.SignalSctpDataChannelCreated();
  }

  cricket::RtpDataChannel* rtp_data_channel() const override {
    return data_channel_controller_.rtp_data_channel();
  }

  std::vector<DataChannelStats> GetDataChannelStats() const override;

  absl::optional<std::string> sctp_transport_name() const override;

  cricket::CandidateStatsList GetPooledCandidateStats() const override;
  std::map<std::string, std::string> GetTransportNamesByMid() const override;
  std::map<std::string, cricket::TransportStats> GetTransportStatsByNames(
      const std::set<std::string>& transport_names) override;
  Call::Stats GetCallStats() override;

  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override;
  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain(
      const std::string& transport_name) override;
  bool IceRestartPending(const std::string& content_name) const override;
  bool NeedsIceRestart(const std::string& content_name) const override;
  bool GetSslRole(const std::string& content_name, rtc::SSLRole* role) override;

  // Functions needed by DataChannelController
  void NoteDataAddedEvent() { NoteUsageEvent(UsageEvent::DATA_ADDED); }
  // Returns the observer. Will crash on CHECK if the observer is removed.
  PeerConnectionObserver* Observer() const;
  bool IsClosed() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return sdp_handler_.signaling_state() == PeerConnectionInterface::kClosed;
  }
  // Get current SSL role used by SCTP's underlying transport.
  bool GetSctpSslRole(rtc::SSLRole* role);
  // Handler for the "channel closed" signal
  void OnSctpDataChannelClosed(DataChannelInterface* channel);

  bool ShouldFireNegotiationNeededEvent(uint32_t event_id) override;

  // Functions needed by SdpOfferAnswerHandler
  StatsCollector* stats() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return stats_.get();
  }
  DataChannelController* data_channel_controller() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return &data_channel_controller_;
  }
  bool dtls_enabled() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return dtls_enabled_;
  }
  const PeerConnectionInterface::RTCConfiguration* configuration() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return &configuration_;
  }
  absl::optional<std::string> sctp_mid() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return sctp_mid_s_;
  }
  PeerConnectionMessageHandler* message_handler() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return &message_handler_;
  }

  // Functions made public for testing.
  void ReturnHistogramVeryQuicklyForTesting() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return_histogram_very_quickly_ = true;
  }
  void RequestUsagePatternReportForTesting();

 protected:
  ~PeerConnection() override;

 private:
  // While refactoring: Allow access from SDP negotiation
  // TOOD(https://bugs.webrtc.org/11995): Remove friendship.
  friend class SdpOfferAnswerHandler;

  struct RtpSenderInfo {
    RtpSenderInfo() : first_ssrc(0) {}
    RtpSenderInfo(const std::string& stream_id,
                  const std::string sender_id,
                  uint32_t ssrc)
        : stream_id(stream_id), sender_id(sender_id), first_ssrc(ssrc) {}
    bool operator==(const RtpSenderInfo& other) {
      return this->stream_id == other.stream_id &&
             this->sender_id == other.sender_id &&
             this->first_ssrc == other.first_ssrc;
    }
    std::string stream_id;
    std::string sender_id;
    // An RtpSender can have many SSRCs. The first one is used as a sort of ID
    // for communicating with the lower layers.
    uint32_t first_ssrc;
  };

  // Plan B helpers for getting the voice/video media channels for the single
  // audio/video transceiver, if it exists.
  cricket::VoiceMediaChannel* voice_media_channel() const
      RTC_RUN_ON(signaling_thread());
  cricket::VideoMediaChannel* video_media_channel() const
      RTC_RUN_ON(signaling_thread());

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  GetSendersInternal() const;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  GetReceiversInternal() const RTC_RUN_ON(signaling_thread());

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetAudioTransceiver() const;
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetVideoTransceiver() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetFirstAudioTransceiver() const RTC_RUN_ON(signaling_thread());


  void CreateAudioReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info)
      RTC_RUN_ON(signaling_thread());

  void CreateVideoReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info)
      RTC_RUN_ON(signaling_thread());
  rtc::scoped_refptr<RtpReceiverInterface> RemoveAndStopReceiver(
      const RtpSenderInfo& remote_sender_info) RTC_RUN_ON(signaling_thread());

  // May be called either by AddStream/RemoveStream, or when a track is
  // added/removed from a stream previously added via AddStream.
  void AddAudioTrack(AudioTrackInterface* track, MediaStreamInterface* stream);
  void RemoveAudioTrack(AudioTrackInterface* track,
                        MediaStreamInterface* stream);
  void AddVideoTrack(VideoTrackInterface* track, MediaStreamInterface* stream);
  void RemoveVideoTrack(VideoTrackInterface* track,
                        MediaStreamInterface* stream);

  // AddTrack implementation when Unified Plan is specified.
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackUnifiedPlan(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids)
      RTC_RUN_ON(signaling_thread());
  // AddTrack implementation when Plan B is specified.
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackPlanB(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids)
      RTC_RUN_ON(signaling_thread());

  // Returns the first RtpTransceiver suitable for a newly added track, if such
  // transceiver is available.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindFirstTransceiverForAddedTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track)
      RTC_RUN_ON(signaling_thread());

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindTransceiverBySender(rtc::scoped_refptr<RtpSenderInterface> sender)
      RTC_RUN_ON(signaling_thread());

  // Internal implementation for AddTransceiver family of methods. If
  // |fire_callback| is set, fires OnRenegotiationNeeded callback if successful.
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init,
      bool fire_callback = true);

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  CreateSender(cricket::MediaType media_type,
               const std::string& id,
               rtc::scoped_refptr<MediaStreamTrackInterface> track,
               const std::vector<std::string>& stream_ids,
               const std::vector<RtpEncodingParameters>& send_encodings);

  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  CreateReceiver(cricket::MediaType media_type, const std::string& receiver_id);

  // Create a new RtpTransceiver of the given type and add it to the list of
  // transceivers.
  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  CreateAndAddTransceiver(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);

  void SetIceConnectionState(IceConnectionState new_state);
  void SetStandardizedIceConnectionState(
      PeerConnectionInterface::IceConnectionState new_state)
      RTC_RUN_ON(signaling_thread());
  void SetConnectionState(
      PeerConnectionInterface::PeerConnectionState new_state)
      RTC_RUN_ON(signaling_thread());

  // Called any time the IceGatheringState changes.
  void OnIceGatheringChange(IceGatheringState new_state)
      RTC_RUN_ON(signaling_thread());
  // New ICE candidate has been gathered.
  void OnIceCandidate(std::unique_ptr<IceCandidateInterface> candidate)
      RTC_RUN_ON(signaling_thread());
  // Gathering of an ICE candidate failed.
  void OnIceCandidateError(const std::string& address,
                           int port,
                           const std::string& url,
                           int error_code,
                           const std::string& error_text)
      RTC_RUN_ON(signaling_thread());
  // Some local ICE candidates have been removed.
  void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates)
      RTC_RUN_ON(signaling_thread());

  void OnSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& event)
      RTC_RUN_ON(signaling_thread());

  // Signals from MediaStreamObserver.
  void OnAudioTrackAdded(AudioTrackInterface* track,
                         MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());
  void OnAudioTrackRemoved(AudioTrackInterface* track,
                           MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());
  void OnVideoTrackAdded(VideoTrackInterface* track,
                         MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());
  void OnVideoTrackRemoved(VideoTrackInterface* track,
                           MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());

  void OnNegotiationNeeded();


  // Returns the MID for the data section associated with either the
  // RtpDataChannel or SCTP data channel, if it has been set. If no data
  // channels are configured this will return nullopt.
  absl::optional<std::string> GetDataMid() const;

  // Triggered when a remote sender has been seen for the first time in a remote
  // session description. It creates a remote MediaStreamTrackInterface
  // implementation and triggers CreateAudioReceiver or CreateVideoReceiver.
  void OnRemoteSenderAdded(const RtpSenderInfo& sender_info,
                           MediaStreamInterface* stream,
                           cricket::MediaType media_type);

  // Triggered when a remote sender has been removed from a remote session
  // description. It removes the remote sender with id |sender_id| from a remote
  // MediaStream and triggers DestroyAudioReceiver or DestroyVideoReceiver.
  void OnRemoteSenderRemoved(const RtpSenderInfo& sender_info,
                             MediaStreamInterface* stream,
                             cricket::MediaType media_type);

  // Triggered when a local sender has been seen for the first time in a local
  // session description.
  // This method triggers CreateAudioSender or CreateVideoSender if the rtp
  // streams in the local SessionDescription can be mapped to a MediaStreamTrack
  // in a MediaStream in |local_streams_|
  void OnLocalSenderAdded(const RtpSenderInfo& sender_info,
                          cricket::MediaType media_type);

  // Triggered when a local sender has been removed from a local session
  // description.
  // This method triggers DestroyAudioSender or DestroyVideoSender if a stream
  // has been removed from the local SessionDescription and the stream can be
  // mapped to a MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalSenderRemoved(const RtpSenderInfo& sender_info,
                            cricket::MediaType media_type);

  // Returns true if the PeerConnection is configured to use Unified Plan
  // semantics for creating offers/answers and setting local/remote
  // descriptions. If this is true the RtpTransceiver API will also be available
  // to the user. If this is false, Plan B semantics are assumed.
  // TODO(bugs.webrtc.org/8530): Flip the default to be Unified Plan once
  // sufficient time has passed.
  bool IsUnifiedPlan() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return configuration_.sdp_semantics == SdpSemantics::kUnifiedPlan;
  }

  // Return the RtpSender with the given track attached.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderForTrack(MediaStreamTrackInterface* track) const
      RTC_RUN_ON(signaling_thread());

  // Return the RtpSender with the given id, or null if none exists.
  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderById(const std::string& sender_id) const
      RTC_RUN_ON(signaling_thread());

  // Return the RtpReceiver with the given id, or null if none exists.
  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  FindReceiverById(const std::string& receiver_id) const
      RTC_RUN_ON(signaling_thread());

  std::vector<RtpSenderInfo>* GetRemoteSenderInfos(
      cricket::MediaType media_type);
  std::vector<RtpSenderInfo>* GetLocalSenderInfos(
      cricket::MediaType media_type);
  const RtpSenderInfo* FindSenderInfo(const std::vector<RtpSenderInfo>& infos,
                                      const std::string& stream_id,
                                      const std::string sender_id) const;

  // Returns the specified SCTP DataChannel in sctp_data_channels_,
  // or nullptr if not found.
  SctpDataChannel* FindDataChannelBySid(int sid) const
      RTC_RUN_ON(signaling_thread());

  // Called when first configuring the port allocator.
  struct InitializePortAllocatorResult {
    bool enable_ipv6;
  };
  InitializePortAllocatorResult InitializePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      const RTCConfiguration& configuration);
  // Called when SetConfiguration is called to apply the supported subset
  // of the configuration on the network thread.
  bool ReconfigurePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      IceTransportsType type,
      int candidate_pool_size,
      PortPrunePolicy turn_port_prune_policy,
      webrtc::TurnCustomizer* turn_customizer,
      absl::optional<int> stun_candidate_keepalive_interval,
      bool have_local_description);

  // Starts output of an RTC event log to the given output object.
  // This function should only be called from the worker thread.
  bool StartRtcEventLog_w(std::unique_ptr<RtcEventLogOutput> output,
                          int64_t output_period_ms);

  // Stops recording an RTC event log.
  // This function should only be called from the worker thread.
  void StopRtcEventLog_w();

  // Ensures the configuration doesn't have any parameters with invalid values,
  // or values that conflict with other parameters.
  //
  // Returns RTCError::OK() if there are no issues.
  RTCError ValidateConfiguration(const RTCConfiguration& config) const;

  cricket::ChannelManager* channel_manager() const;

  cricket::ChannelInterface* GetChannel(const std::string& content_name);

  cricket::IceConfig ParseIceConfig(
      const PeerConnectionInterface::RTCConfiguration& config) const;

  cricket::DataChannelType data_channel_type() const;

  // Called when an RTCCertificate is generated or retrieved by
  // WebRTCSessionDescriptionFactory. Should happen before setLocalDescription.
  void OnCertificateReady(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  static bool GetTransportDescription(
      const cricket::SessionDescription* description,
      const std::string& content_name,
      cricket::TransportDescription* info);

  // Returns the media index for a local ice candidate given the content name.
  // Returns false if the local session description does not have a media
  // content called  |content_name|.
  bool GetLocalCandidateMediaIndex(const std::string& content_name,
                                   int* sdp_mline_index)
      RTC_RUN_ON(signaling_thread());

  bool SetupDataChannelTransport_n(const std::string& mid)
      RTC_RUN_ON(network_thread());
  void TeardownDataChannelTransport_n() RTC_RUN_ON(network_thread());

  bool ValidateBundleSettings(const cricket::SessionDescription* desc);
  bool HasRtcpMuxEnabled(const cricket::ContentInfo* content);

  // Verifies a=setup attribute as per RFC 5763.
  bool ValidateDtlsSetupAttribute(const cricket::SessionDescription* desc,
                                  SdpType type);

  // Returns true if SRTP (either using DTLS-SRTP or SDES) is required by
  // this session.
  bool SrtpRequired() const RTC_RUN_ON(signaling_thread());

  // JsepTransportController signal handlers.
  void OnTransportControllerConnectionState(cricket::IceConnectionState state)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerGatheringState(cricket::IceGatheringState state)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidatesGathered(
      const std::string& transport_name,
      const std::vector<cricket::Candidate>& candidates)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidateError(
      const cricket::IceCandidateErrorEvent& event)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidateChanged(
      const cricket::CandidatePairChangeEvent& event)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerDtlsHandshakeError(rtc::SSLHandshakeError error);

  // Report the UMA metric SdpFormatReceived for the given remote offer.
  void ReportSdpFormatReceived(const SessionDescriptionInterface& remote_offer);

  // Invoked when TransportController connection completion is signaled.
  // Reports stats for all transports in use.
  void ReportTransportStats() RTC_RUN_ON(signaling_thread());

  // Gather the usage of IPv4/IPv6 as best connection.
  void ReportBestConnectionState(const cricket::TransportStats& stats);

  void ReportNegotiatedCiphers(const cricket::TransportStats& stats,
                               const std::set<cricket::MediaType>& media_types)
      RTC_RUN_ON(signaling_thread());
  void ReportIceCandidateCollected(const cricket::Candidate& candidate)
      RTC_RUN_ON(signaling_thread());

  void NoteUsageEvent(UsageEvent event);
  void ReportUsagePattern() const RTC_RUN_ON(signaling_thread());

  void OnSentPacket_w(const rtc::SentPacket& sent_packet);

  // JsepTransportController::Observer override.
  //
  // Called by |transport_controller_| when processing transport information
  // from a session description, and the mapping from m= sections to transports
  // changed (as a result of BUNDLE negotiation, or m= sections being
  // rejected).
  bool OnTransportChanged(
      const std::string& mid,
      RtpTransportInternal* rtp_transport,
      rtc::scoped_refptr<DtlsTransport> dtls_transport,
      DataChannelTransportInterface* data_channel_transport) override;

  // RtpSenderBase::SetStreamsObserver override.
  void OnSetStreams() override;

  // Returns the CryptoOptions for this PeerConnection. This will always
  // return the RTCConfiguration.crypto_options if set and will only default
  // back to the PeerConnectionFactory settings if nothing was set.
  CryptoOptions GetCryptoOptions();

  // Returns rtp transport, result can not be nullptr.
  RtpTransportInternal* GetRtpTransport(const std::string& mid) {
    RTC_DCHECK_RUN_ON(signaling_thread());
    auto rtp_transport = transport_controller_->GetRtpTransport(mid);
    RTC_DCHECK(rtp_transport);
    return rtp_transport;
  }

  std::function<void(const rtc::CopyOnWriteBuffer& packet,
                     int64_t packet_time_us)>
  InitializeRtcpCallback();

  // Storing the factory as a scoped reference pointer ensures that the memory
  // in the PeerConnectionFactoryImpl remains available as long as the
  // PeerConnection is running. It is passed to PeerConnection as a raw pointer.
  // However, since the reference counting is done in the
  // PeerConnectionFactoryInterface all instances created using the raw pointer
  // will refer to the same reference count.
  const rtc::scoped_refptr<ConnectionContext> context_;
  PeerConnectionObserver* observer_ RTC_GUARDED_BY(signaling_thread()) =
      nullptr;

  // The EventLog needs to outlive |call_| (and any other object that uses it).
  std::unique_ptr<RtcEventLog> event_log_ RTC_GUARDED_BY(worker_thread());

  // Points to the same thing as `event_log_`. Since it's const, we may read the
  // pointer (but not touch the object) from any thread.
  RtcEventLog* const event_log_ptr_ RTC_PT_GUARDED_BY(worker_thread());

  IceConnectionState ice_connection_state_ RTC_GUARDED_BY(signaling_thread()) =
      kIceConnectionNew;
  PeerConnectionInterface::IceConnectionState standardized_ice_connection_state_
      RTC_GUARDED_BY(signaling_thread()) = kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState connection_state_
      RTC_GUARDED_BY(signaling_thread()) = PeerConnectionState::kNew;

  IceGatheringState ice_gathering_state_ RTC_GUARDED_BY(signaling_thread()) =
      kIceGatheringNew;
  PeerConnectionInterface::RTCConfiguration configuration_
      RTC_GUARDED_BY(signaling_thread());

  // TODO(zstein): |async_resolver_factory_| can currently be nullptr if it
  // is not injected. It should be required once chromium supplies it.
  std::unique_ptr<AsyncResolverFactory> async_resolver_factory_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<rtc::PacketSocketFactory> packet_socket_factory_;
  std::unique_ptr<cricket::PortAllocator>
      port_allocator_;  // TODO(bugs.webrtc.org/9987): Accessed on both
                        // signaling and network thread.
  std::unique_ptr<webrtc::IceTransportFactory>
      ice_transport_factory_;  // TODO(bugs.webrtc.org/9987): Accessed on the
                               // signaling thread but the underlying raw
                               // pointer is given to
                               // |jsep_transport_controller_| and used on the
                               // network thread.
  std::unique_ptr<rtc::SSLCertificateVerifier>
      tls_cert_verifier_;  // TODO(bugs.webrtc.org/9987): Accessed on both
                           // signaling and network thread.

  // These lists store sender info seen in local/remote descriptions.
  std::vector<RtpSenderInfo> remote_audio_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> remote_video_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> local_audio_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> local_video_sender_infos_
      RTC_GUARDED_BY(signaling_thread());

  // The unique_ptr belongs to the worker thread, but the Call object manages
  // its own thread safety.
  std::unique_ptr<Call> call_ RTC_GUARDED_BY(worker_thread());
  std::unique_ptr<ScopedTaskSafety> call_safety_
      RTC_GUARDED_BY(worker_thread());

  // Points to the same thing as `call_`. Since it's const, we may read the
  // pointer from any thread.
  // TODO(bugs.webrtc.org/11992): Remove this workaround (and potential dangling
  // pointer).
  Call* const call_ptr_;

  std::unique_ptr<StatsCollector> stats_
      RTC_GUARDED_BY(signaling_thread());  // A pointer is passed to senders_
  rtc::scoped_refptr<RTCStatsCollector> stats_collector_
      RTC_GUARDED_BY(signaling_thread());
  TransceiverList transceivers_;

  std::string session_id_ RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<JsepTransportController>
      transport_controller_;  // TODO(bugs.webrtc.org/9987): Accessed on both
                              // signaling and network thread.

  // |sctp_mid_| is the content name (MID) in SDP.
  // Note: this is used as the data channel MID by both SCTP and data channel
  // transports.  It is set when either transport is initialized and unset when
  // both transports are deleted.
  // There is one copy on the signaling thread and another copy on the
  // networking thread. Changes are always initiated from the signaling
  // thread, but applied first on the networking thread via an invoke().
  absl::optional<std::string> sctp_mid_s_ RTC_GUARDED_BY(signaling_thread());
  absl::optional<std::string> sctp_mid_n_ RTC_GUARDED_BY(network_thread());

  // The machinery for handling offers and answers.
  SdpOfferAnswerHandler sdp_handler_ RTC_GUARDED_BY(signaling_thread());

  bool dtls_enabled_ RTC_GUARDED_BY(signaling_thread()) = false;

  // Member variables for caching global options.
  cricket::AudioOptions audio_options_ RTC_GUARDED_BY(signaling_thread());
  cricket::VideoOptions video_options_ RTC_GUARDED_BY(signaling_thread());

  UsagePattern usage_pattern_ RTC_GUARDED_BY(signaling_thread());
  bool return_histogram_very_quickly_ RTC_GUARDED_BY(signaling_thread()) =
      false;

  // This object should be used to generate any SSRC that is not explicitly
  // specified by the user (or by the remote party).
  // The generator is not used directly, instead it is passed on to the
  // channel manager and the session description factory.
  rtc::UniqueRandomIdGenerator ssrc_generator_
      RTC_GUARDED_BY(signaling_thread());

  // A video bitrate allocator factory.
  // This can injected using the PeerConnectionDependencies,
  // or else the CreateBuiltinVideoBitrateAllocatorFactory() will be called.
  // Note that one can still choose to override this in a MediaEngine
  // if one wants too.
  std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
      video_bitrate_allocator_factory_;

  DataChannelController data_channel_controller_;

  // Machinery for handling messages posted to oneself
  PeerConnectionMessageHandler message_handler_;
};

}  // namespace webrtc

#endif  // PC_PEER_CONNECTION_H_
