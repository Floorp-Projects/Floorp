/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIA_CHANNEL_SHIM_H_
#define MEDIA_BASE_MEDIA_CHANNEL_SHIM_H_

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
#include "api/crypto/frame_decryptor_interface.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_channel_impl.h"
#include "media/base/stream_params.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"

namespace cricket {

// The VideoMediaShimChannel is replacing the VideoMediaChannel
// interface.
// If called with both send_impl and receive_impl, it operates in kBoth
// mode; if called with only one, it will shim that one and DCHECK if one
// tries to do functions belonging to the other.
class VoiceMediaShimChannel : public VoiceMediaChannel {
 public:
  VoiceMediaShimChannel(
      std::unique_ptr<VoiceMediaSendChannelInterface> send_impl,
      std::unique_ptr<VoiceMediaReceiveChannelInterface> receive_impl);

  VoiceMediaSendChannelInterface* AsVoiceSendChannel() override { return this; }
  VoiceMediaReceiveChannelInterface* AsVoiceReceiveChannel() override {
    return this;
  }
  VideoMediaSendChannelInterface* AsVideoSendChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }
  VideoMediaReceiveChannelInterface* AsVideoReceiveChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  // SetInterface needs to run on both send and receive channels.
  void SetInterface(MediaChannelNetworkInterface* iface) override {
    if (send_impl_) {
      send_impl()->SetInterface(iface);
    }
    if (receive_impl_) {
      receive_impl()->SetInterface(iface);
    }
  }

  // Not really valid for this mode
  MediaChannel* ImplForTesting() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  // Implementation of MediaBaseChannelInterface
  cricket::MediaType media_type() const override { return MEDIA_TYPE_VIDEO; }

  // Implementation of MediaSendChannelInterface
  void OnPacketSent(const rtc::SentPacket& sent_packet) override {
    send_impl()->OnPacketSent(sent_packet);
  }
  void OnReadyToSend(bool ready) override { send_impl()->OnReadyToSend(ready); }
  void OnNetworkRouteChanged(absl::string_view transport_name,
                             const rtc::NetworkRoute& network_route) override {
    send_impl()->OnNetworkRouteChanged(transport_name, network_route);
  }
  void SetExtmapAllowMixed(bool extmap_allow_mixed) override {
    send_impl()->SetExtmapAllowMixed(extmap_allow_mixed);
  }
  bool HasNetworkInterface() const override {
    return send_impl()->HasNetworkInterface();
  }
  bool ExtmapAllowMixed() const override {
    return send_impl()->ExtmapAllowMixed();
  }

  bool AddSendStream(const StreamParams& sp) override {
    return send_impl()->AddSendStream(sp);
  }
  bool RemoveSendStream(uint32_t ssrc) override {
    return send_impl()->RemoveSendStream(ssrc);
  }
  void SetFrameEncryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameEncryptorInterface>
                             frame_encryptor) override {
    send_impl()->SetFrameEncryptor(ssrc, frame_encryptor);
  }
  webrtc::RTCError SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters,
      webrtc::SetParametersCallback callback = nullptr) override {
    return send_impl()->SetRtpSendParameters(ssrc, parameters,
                                             std::move(callback));
  }

  void SetEncoderToPacketizerFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override {
    return send_impl()->SetEncoderToPacketizerFrameTransformer(
        ssrc, frame_transformer);
  }
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const override {
    return send_impl()->GetRtpSendParameters(ssrc);
  }
  // Implementation of MediaReceiveChannelInterface
  void OnPacketReceived(const webrtc::RtpPacketReceived& packet) override {
    receive_impl()->OnPacketReceived(packet);
  }
  bool AddRecvStream(const StreamParams& sp) override {
    return receive_impl()->AddRecvStream(sp);
  }
  bool RemoveRecvStream(uint32_t ssrc) override {
    return receive_impl()->RemoveRecvStream(ssrc);
  }
  void ResetUnsignaledRecvStream() override {
    return receive_impl()->ResetUnsignaledRecvStream();
  }
  absl::optional<uint32_t> GetUnsignaledSsrc() const override {
    return receive_impl()->GetUnsignaledSsrc();
  }
  void ChooseReceiverReportSsrc(const std::set<uint32_t>& choices) override {
    return receive_impl()->ChooseReceiverReportSsrc(choices);
  }
  void OnDemuxerCriteriaUpdatePending() override {
    receive_impl()->OnDemuxerCriteriaUpdatePending();
  }
  void OnDemuxerCriteriaUpdateComplete() override {
    receive_impl()->OnDemuxerCriteriaUpdateComplete();
  }
  void SetFrameDecryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override {
    receive_impl()->SetFrameDecryptor(ssrc, frame_decryptor);
  }
  void SetDepacketizerToDecoderFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override {
    receive_impl()->SetDepacketizerToDecoderFrameTransformer(ssrc,
                                                             frame_transformer);
  }
  bool SendCodecHasNack() const override {
    return send_impl()->SendCodecHasNack();
  }
  void SetSendCodecChangedCallback(
      absl::AnyInvocable<void()> callback) override {
    send_impl()->SetSendCodecChangedCallback(std::move(callback));
  }
  // Implementation of VoiceMediaSendChannel
  bool SetSendParameters(const AudioSendParameters& params) override {
    return send_impl()->SetSendParameters(params);
  }
  void SetSend(bool send) override { return send_impl()->SetSend(send); }
  bool SetAudioSend(uint32_t ssrc,
                    bool enable,
                    const AudioOptions* options,
                    AudioSource* source) override {
    return send_impl()->SetAudioSend(ssrc, enable, options, source);
  }
  bool CanInsertDtmf() override { return send_impl()->CanInsertDtmf(); }
  bool InsertDtmf(uint32_t ssrc, int event, int duration) override {
    return send_impl()->InsertDtmf(ssrc, event, duration);
  }
  bool GetStats(VoiceMediaSendInfo* info) override {
    return send_impl()->GetStats(info);
  }
  bool SenderNackEnabled() const override {
    return send_impl()->SenderNackEnabled();
  }
  bool SenderNonSenderRttEnabled() const override {
    return send_impl()->SenderNonSenderRttEnabled();
  }
  // Implementation of VoiceMediaReceiveChannelInterface
  bool SetRecvParameters(const AudioRecvParameters& params) override {
    return receive_impl()->SetRecvParameters(params);
  }
  webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const override {
    return receive_impl()->GetRtpReceiveParameters(ssrc);
  }
  std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const override {
    return receive_impl()->GetSources(ssrc);
  }
  webrtc::RtpParameters GetDefaultRtpReceiveParameters() const override {
    return receive_impl()->GetDefaultRtpReceiveParameters();
  }
  void SetPlayout(bool playout) override {
    return receive_impl()->SetPlayout(playout);
  }
  bool SetOutputVolume(uint32_t ssrc, double volume) override {
    return receive_impl()->SetOutputVolume(ssrc, volume);
  }
  bool SetDefaultOutputVolume(double volume) override {
    return receive_impl()->SetDefaultOutputVolume(volume);
  }
  void SetRawAudioSink(
      uint32_t ssrc,
      std::unique_ptr<webrtc::AudioSinkInterface> sink) override {
    return receive_impl()->SetRawAudioSink(ssrc, std::move(sink));
  }
  void SetDefaultRawAudioSink(
      std::unique_ptr<webrtc::AudioSinkInterface> sink) override {
    return receive_impl()->SetDefaultRawAudioSink(std::move(sink));
  }
  bool GetStats(VoiceMediaReceiveInfo* info, bool reset_legacy) override {
    return receive_impl_->GetStats(info, reset_legacy);
  }
  void SetReceiveNackEnabled(bool enabled) override {
    receive_impl_->SetReceiveNackEnabled(enabled);
  }
  void SetReceiveNonSenderRttEnabled(bool enabled) override {
    receive_impl_->SetReceiveNonSenderRttEnabled(enabled);
  }
  void SetSsrcListChangedCallback(
      absl::AnyInvocable<void(const std::set<uint32_t>&)> callback) override {
    send_impl()->SetSsrcListChangedCallback(std::move(callback));
  }
  // Implementation of Delayable
  bool SetBaseMinimumPlayoutDelayMs(uint32_t ssrc, int delay_ms) override {
    return receive_impl()->SetBaseMinimumPlayoutDelayMs(ssrc, delay_ms);
  }
  absl::optional<int> GetBaseMinimumPlayoutDelayMs(
      uint32_t ssrc) const override {
    return receive_impl()->GetBaseMinimumPlayoutDelayMs(ssrc);
  }
  bool GetSendStats(VoiceMediaSendInfo* info) override {
    return send_impl()->GetStats(info);
  }
  bool GetReceiveStats(VoiceMediaReceiveInfo* info,
                       bool reset_legacy) override {
    return receive_impl()->GetStats(info, reset_legacy);
  }

  // Only for testing of implementations - these will be used to static_cast the
  // pointers to the implementations, so can only be safely used in conjunction
  // with the corresponding create functions.
  VoiceMediaSendChannelInterface* SendImplForTesting() {
    return send_impl_.get();
  }
  VoiceMediaReceiveChannelInterface* ReceiveImplForTesting() {
    return receive_impl_.get();
  }

 private:
  VoiceMediaSendChannelInterface* send_impl() { return send_impl_.get(); }
  VoiceMediaReceiveChannelInterface* receive_impl() {
    RTC_DCHECK(receive_impl_);
    return receive_impl_.get();
  }
  const VoiceMediaSendChannelInterface* send_impl() const {
    RTC_DCHECK(send_impl_);
    return send_impl_.get();
  }
  const VoiceMediaReceiveChannelInterface* receive_impl() const {
    return receive_impl_.get();
  }

  std::unique_ptr<VoiceMediaSendChannelInterface> send_impl_;
  std::unique_ptr<VoiceMediaReceiveChannelInterface> receive_impl_;
};

// The VideoMediaShimChannel is replacing the VideoMediaChannel
// interface.
// If called with both send_impl and receive_impl, it operates in kBoth
// mode; if called with only one, it will shim that one and DCHECK if one
// tries to do functions belonging to the other.

class VideoMediaShimChannel : public VideoMediaChannel {
 public:
  VideoMediaShimChannel(
      std::unique_ptr<VideoMediaSendChannelInterface> send_impl,
      std::unique_ptr<VideoMediaReceiveChannelInterface> receive_impl);

  VideoMediaSendChannelInterface* AsVideoSendChannel() override { return this; }
  VideoMediaReceiveChannelInterface* AsVideoReceiveChannel() override {
    return this;
  }
  VoiceMediaSendChannelInterface* AsVoiceSendChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }
  VoiceMediaReceiveChannelInterface* AsVoiceReceiveChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  // SetInterface needs to run on both send and receive channels.
  void SetInterface(MediaChannelNetworkInterface* iface) override {
    if (send_impl_) {
      send_impl()->SetInterface(iface);
    }
    if (receive_impl_) {
      receive_impl()->SetInterface(iface);
    }
  }

  // Not really valid for this mode
  MediaChannel* ImplForTesting() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  // Implementation of MediaBaseChannelInterface
  cricket::MediaType media_type() const override { return MEDIA_TYPE_VIDEO; }

  // Implementation of MediaSendChannelInterface
  void OnPacketSent(const rtc::SentPacket& sent_packet) override {
    send_impl()->OnPacketSent(sent_packet);
  }
  void OnReadyToSend(bool ready) override { send_impl()->OnReadyToSend(ready); }
  void OnNetworkRouteChanged(absl::string_view transport_name,
                             const rtc::NetworkRoute& network_route) override {
    send_impl()->OnNetworkRouteChanged(transport_name, network_route);
  }
  void SetExtmapAllowMixed(bool extmap_allow_mixed) override {
    send_impl()->SetExtmapAllowMixed(extmap_allow_mixed);
  }
  bool HasNetworkInterface() const override {
    return send_impl()->HasNetworkInterface();
  }
  bool ExtmapAllowMixed() const override {
    return send_impl()->ExtmapAllowMixed();
  }

  bool AddSendStream(const StreamParams& sp) override {
    return send_impl()->AddSendStream(sp);
  }
  bool RemoveSendStream(uint32_t ssrc) override {
    return send_impl()->RemoveSendStream(ssrc);
  }
  void SetFrameEncryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameEncryptorInterface>
                             frame_encryptor) override {
    send_impl()->SetFrameEncryptor(ssrc, frame_encryptor);
  }
  webrtc::RTCError SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters,
      webrtc::SetParametersCallback callback = nullptr) override {
    return send_impl()->SetRtpSendParameters(ssrc, parameters,
                                             std::move(callback));
  }

  void SetEncoderToPacketizerFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override {
    return send_impl()->SetEncoderToPacketizerFrameTransformer(
        ssrc, frame_transformer);
  }
  void SetEncoderSelector(uint32_t ssrc,
                          webrtc::VideoEncoderFactory::EncoderSelectorInterface*
                              encoder_selector) override {
    send_impl()->SetEncoderSelector(ssrc, encoder_selector);
  }
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const override {
    return send_impl()->GetRtpSendParameters(ssrc);
  }
  // Send_Implementation of VideoMediaSendChannelInterface
  bool SetSendParameters(const VideoSendParameters& params) override {
    return send_impl()->SetSendParameters(params);
  }
  bool GetSendCodec(VideoCodec* send_codec) override {
    return send_impl()->GetSendCodec(send_codec);
  }
  bool SetSend(bool send) override { return send_impl()->SetSend(send); }
  bool SetVideoSend(
      uint32_t ssrc,
      const VideoOptions* options,
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source) override {
    return send_impl()->SetVideoSend(ssrc, options, source);
  }
  void GenerateSendKeyFrame(uint32_t ssrc,
                            const std::vector<std::string>& rids) override {
    return send_impl()->GenerateSendKeyFrame(ssrc, rids);
  }
  void SetVideoCodecSwitchingEnabled(bool enabled) override {
    return send_impl()->SetVideoCodecSwitchingEnabled(enabled);
  }
  bool GetStats(VideoMediaSendInfo* info) override {
    return send_impl_->GetStats(info);
  }
  bool GetSendStats(VideoMediaSendInfo* info) override {
    return send_impl_->GetStats(info);
  }
  void FillBitrateInfo(BandwidthEstimationInfo* bwe_info) override {
    return send_impl_->FillBitrateInfo(bwe_info);
  }
  // Information queries to support SetReceiverFeedbackParameters
  webrtc::RtcpMode SendCodecRtcpMode() const override {
    return send_impl()->SendCodecRtcpMode();
  }
  bool SendCodecHasLntf() const override {
    return send_impl()->SendCodecHasLntf();
  }
  bool SendCodecHasNack() const override {
    return send_impl()->SendCodecHasNack();
  }
  absl::optional<int> SendCodecRtxTime() const override {
    return send_impl()->SendCodecRtxTime();
  }
  void SetSsrcListChangedCallback(
      absl::AnyInvocable<void(const std::set<uint32_t>&)> callback) override {
    send_impl()->SetSsrcListChangedCallback(std::move(callback));
  }
  void SetSendCodecChangedCallback(
      absl::AnyInvocable<void()> callback) override {
    // This callback is used internally by the shim, so should not be called by
    // users for the "both" case.
    if (send_impl_ && receive_impl_) {
      RTC_CHECK_NOTREACHED();
    }
    send_impl()->SetSendCodecChangedCallback(std::move(callback));
  }

  // Implementation of Delayable
  bool SetBaseMinimumPlayoutDelayMs(uint32_t ssrc, int delay_ms) override {
    return receive_impl()->SetBaseMinimumPlayoutDelayMs(ssrc, delay_ms);
  }
  absl::optional<int> GetBaseMinimumPlayoutDelayMs(
      uint32_t ssrc) const override {
    return receive_impl()->GetBaseMinimumPlayoutDelayMs(ssrc);
  }
  // Implementation of MediaReceiveChannelInterface
  void OnPacketReceived(const webrtc::RtpPacketReceived& packet) override {
    receive_impl()->OnPacketReceived(packet);
  }
  bool AddRecvStream(const StreamParams& sp) override {
    return receive_impl()->AddRecvStream(sp);
  }
  bool RemoveRecvStream(uint32_t ssrc) override {
    return receive_impl()->RemoveRecvStream(ssrc);
  }
  void ResetUnsignaledRecvStream() override {
    return receive_impl()->ResetUnsignaledRecvStream();
  }
  absl::optional<uint32_t> GetUnsignaledSsrc() const override {
    return receive_impl()->GetUnsignaledSsrc();
  }
  void ChooseReceiverReportSsrc(const std::set<uint32_t>& choices) override {
    return receive_impl()->ChooseReceiverReportSsrc(choices);
  }
  void OnDemuxerCriteriaUpdatePending() override {
    receive_impl()->OnDemuxerCriteriaUpdatePending();
  }
  void OnDemuxerCriteriaUpdateComplete() override {
    receive_impl()->OnDemuxerCriteriaUpdateComplete();
  }
  void SetFrameDecryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override {
    receive_impl()->SetFrameDecryptor(ssrc, frame_decryptor);
  }
  void SetDepacketizerToDecoderFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override {
    receive_impl()->SetDepacketizerToDecoderFrameTransformer(ssrc,
                                                             frame_transformer);
  }
  // Implementation of VideoMediaReceiveChannelInterface
  bool SetRecvParameters(const VideoRecvParameters& params) override {
    return receive_impl()->SetRecvParameters(params);
  }
  webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const override {
    return receive_impl()->GetRtpReceiveParameters(ssrc);
  }
  webrtc::RtpParameters GetDefaultRtpReceiveParameters() const override {
    return receive_impl()->GetDefaultRtpReceiveParameters();
  }
  bool SetSink(uint32_t ssrc,
               rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
    return receive_impl()->SetSink(ssrc, sink);
  }
  void SetDefaultSink(
      rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
    return receive_impl()->SetDefaultSink(sink);
  }
  void RequestRecvKeyFrame(uint32_t ssrc) override {
    return receive_impl()->RequestRecvKeyFrame(ssrc);
  }
  std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const override {
    return receive_impl()->GetSources(ssrc);
  }
  // Set recordable encoded frame callback for `ssrc`
  void SetRecordableEncodedFrameCallback(
      uint32_t ssrc,
      std::function<void(const webrtc::RecordableEncodedFrame&)> callback)
      override {
    return receive_impl()->SetRecordableEncodedFrameCallback(
        ssrc, std::move(callback));
  }
  // Clear recordable encoded frame callback for `ssrc`
  void ClearRecordableEncodedFrameCallback(uint32_t ssrc) override {
    receive_impl()->ClearRecordableEncodedFrameCallback(ssrc);
  }
  bool GetStats(VideoMediaReceiveInfo* info) override {
    return receive_impl()->GetStats(info);
  }
  bool GetReceiveStats(VideoMediaReceiveInfo* info) override {
    return receive_impl()->GetStats(info);
  }
  void SetReceiverFeedbackParameters(bool lntf_enabled,
                                     bool nack_enabled,
                                     webrtc::RtcpMode rtcp_mode,
                                     absl::optional<int> rtx_time) override {
    receive_impl()->SetReceiverFeedbackParameters(lntf_enabled, nack_enabled,
                                                  rtcp_mode, rtx_time);
  }
  void SetReceive(bool receive) override {
    receive_impl()->SetReceive(receive);
  }
  bool AddDefaultRecvStreamForTesting(const StreamParams& sp) override {
    return receive_impl()->AddDefaultRecvStreamForTesting(sp);
  }

  // Only for testing of implementations - these will be used to static_cast the
  // pointers to the implementations, so can only be safely used in conjunction
  // with the corresponding create functions.
  VideoMediaSendChannelInterface* SendImplForTesting() {
    return send_impl_.get();
  }
  VideoMediaReceiveChannelInterface* ReceiveImplForTesting() {
    return receive_impl_.get();
  }

 private:
  VideoMediaSendChannelInterface* send_impl() { return send_impl_.get(); }
  VideoMediaReceiveChannelInterface* receive_impl() {
    RTC_DCHECK(receive_impl_);
    return receive_impl_.get();
  }
  const VideoMediaSendChannelInterface* send_impl() const {
    RTC_DCHECK(send_impl_);
    return send_impl_.get();
  }
  const VideoMediaReceiveChannelInterface* receive_impl() const {
    return receive_impl_.get();
  }

  std::unique_ptr<VideoMediaSendChannelInterface> send_impl_;
  std::unique_ptr<VideoMediaReceiveChannelInterface> receive_impl_;
};

}  // namespace cricket

#endif  // MEDIA_BASE_MEDIA_CHANNEL_SHIM_H_
