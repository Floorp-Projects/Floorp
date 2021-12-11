/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_WEBRTCVOICEENGINE_H_
#define MEDIA_ENGINE_WEBRTCVOICEENGINE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/rtpreceiverinterface.h"
#include "call/audio_state.h"
#include "call/call.h"
#include "media/base/rtputils.h"
#include "media/engine/apm_helpers.h"
#include "media/engine/webrtcvoe.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "pc/channel.h"
#include "rtc_base/buffer.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
namespace voe {
class TransmitMixer;
}  // namespace voe
}  // namespace webrtc

namespace cricket {

class AudioDeviceModule;
class AudioMixer;
class AudioSource;
class VoEWrapper;
class WebRtcVoiceMediaChannel;

// WebRtcVoiceEngine is a class to be used with CompositeMediaEngine.
// It uses the WebRtc VoiceEngine library for audio handling.
class WebRtcVoiceEngine final {
  friend class WebRtcVoiceMediaChannel;
 public:
  WebRtcVoiceEngine(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
      rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing);
  // Dependency injection for testing.
  WebRtcVoiceEngine(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
      rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing,
      VoEWrapper* voe_wrapper);
  ~WebRtcVoiceEngine();

  // Does initialization that needs to occur on the worker thread.
  void Init();

  rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const;
  VoiceMediaChannel* CreateChannel(webrtc::Call* call,
                                   const MediaConfig& config,
                                   const AudioOptions& options);

  int GetInputLevel();

  const std::vector<AudioCodec>& send_codecs() const;
  const std::vector<AudioCodec>& recv_codecs() const;
  RtpCapabilities GetCapabilities() const;

  // For tracking WebRtc channels. Needed because we have to pause them
  // all when switching devices.
  // May only be called by WebRtcVoiceMediaChannel.
  void RegisterChannel(WebRtcVoiceMediaChannel* channel);
  void UnregisterChannel(WebRtcVoiceMediaChannel* channel);

  VoEWrapper* voe() { return voe_wrapper_.get(); }

  // Starts AEC dump using an existing file. A maximum file size in bytes can be
  // specified. When the maximum file size is reached, logging is stopped and
  // the file is closed. If max_size_bytes is set to <= 0, no limit will be
  // used.
  bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes);

  // Stops AEC dump.
  void StopAecDump();

  const webrtc::AudioProcessing::Config GetApmConfigForTest() const {
    return apm()->GetConfig();
  }

 private:
  // Every option that is "set" will be applied. Every option not "set" will be
  // ignored. This allows us to selectively turn on and off different options
  // easily at any time.
  bool ApplyOptions(const AudioOptions& options);

  void StartAecDump(const std::string& filename);
  int CreateVoEChannel();

  std::unique_ptr<rtc::TaskQueue> low_priority_worker_queue_;

  webrtc::AudioDeviceModule* adm();
  webrtc::AudioProcessing* apm() const;
  webrtc::voe::TransmitMixer* transmit_mixer();

  AudioCodecs CollectCodecs(
      const std::vector<webrtc::AudioCodecSpec>& specs) const;

  rtc::ThreadChecker signal_thread_checker_;
  rtc::ThreadChecker worker_thread_checker_;

  // The audio device manager.
  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm_;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> encoder_factory_;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory_;
  rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer_;
  // Reference to the APM, owned by VoE.
  rtc::scoped_refptr<webrtc::AudioProcessing> apm_;
  // Reference to the TransmitMixer, owned by VoE.
  webrtc::voe::TransmitMixer* transmit_mixer_ = nullptr;
  // The primary instance of WebRtc VoiceEngine.
  std::unique_ptr<VoEWrapper> voe_wrapper_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  std::vector<AudioCodec> send_codecs_;
  std::vector<AudioCodec> recv_codecs_;
  std::vector<WebRtcVoiceMediaChannel*> channels_;
  webrtc::VoEBase::ChannelConfig channel_config_;
  bool is_dumping_aec_ = false;
  bool initialized_ = false;

  webrtc::AgcConfig default_agc_config_;
  // Cache received extended_filter_aec, delay_agnostic_aec, experimental_ns
  // level controller, and intelligibility_enhancer values, and apply them
  // in case they are missing in the audio options. We need to do this because
  // SetExtraOptions() will revert to defaults for options which are not
  // provided.
  rtc::Optional<bool> extended_filter_aec_;
  rtc::Optional<bool> delay_agnostic_aec_;
  rtc::Optional<bool> experimental_ns_;
  rtc::Optional<bool> intelligibility_enhancer_;
  rtc::Optional<bool> level_control_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcVoiceEngine);
};

// WebRtcVoiceMediaChannel is an implementation of VoiceMediaChannel that uses
// WebRtc Voice Engine.
class WebRtcVoiceMediaChannel final : public VoiceMediaChannel,
                                      public webrtc::Transport {
 public:
  WebRtcVoiceMediaChannel(WebRtcVoiceEngine* engine,
                          const MediaConfig& config,
                          const AudioOptions& options,
                          webrtc::Call* call);
  ~WebRtcVoiceMediaChannel() override;

  const AudioOptions& options() const { return options_; }

  rtc::DiffServCodePoint PreferredDscp() const override;

  bool SetSendParameters(const AudioSendParameters& params) override;
  bool SetRecvParameters(const AudioRecvParameters& params) override;
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const override;
  bool SetRtpSendParameters(uint32_t ssrc,
                            const webrtc::RtpParameters& parameters) override;
  webrtc::RtpParameters GetRtpReceiveParameters(uint32_t ssrc) const override;
  bool SetRtpReceiveParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters) override;

  void SetPlayout(bool playout) override;
  void SetSend(bool send) override;
  bool SetAudioSend(uint32_t ssrc,
                    bool enable,
                    const AudioOptions* options,
                    AudioSource* source) override;
  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32_t ssrc) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool RemoveRecvStream(uint32_t ssrc) override;
  bool GetActiveStreams(StreamList* actives) override;
  int GetOutputLevel() override;
  // SSRC=0 will apply the new volume to current and future unsignaled streams.
  bool SetOutputVolume(uint32_t ssrc, double volume) override;

  bool CanInsertDtmf() override;
  bool InsertDtmf(uint32_t ssrc, int event, int duration) override;

  void OnPacketReceived(rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void OnRtcpReceived(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketTime& packet_time) override;
  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override;
  void OnReadyToSend(bool ready) override;
  bool GetStats(VoiceMediaInfo* info) override;

  // SSRC=0 will set the audio sink on the latest unsignaled stream, future or
  // current. Only one stream at a time will use the sink.
  void SetRawAudioSink(
      uint32_t ssrc,
      std::unique_ptr<webrtc::AudioSinkInterface> sink) override;

  std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const override;

  // implements Transport interface
  bool SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options) override {
    rtc::CopyOnWriteBuffer packet(data, len, kMaxRtpPacketLen);
    rtc::PacketOptions rtc_options;
    rtc_options.packet_id = options.packet_id;
    return VoiceMediaChannel::SendPacket(&packet, rtc_options);
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    rtc::CopyOnWriteBuffer packet(data, len, kMaxRtpPacketLen);
    return VoiceMediaChannel::SendRtcp(&packet, rtc::PacketOptions());
  }

  int GetReceiveChannelId(uint32_t ssrc) const;
  int GetSendChannelId(uint32_t ssrc) const;

 private:
  bool SetOptions(const AudioOptions& options);
  bool SetRecvCodecs(const std::vector<AudioCodec>& codecs);
  bool SetSendCodecs(const std::vector<AudioCodec>& codecs);
  bool SetLocalSource(uint32_t ssrc, AudioSource* source);
  bool MuteStream(uint32_t ssrc, bool mute);

  WebRtcVoiceEngine* engine() { return engine_; }
  void ChangePlayout(bool playout);
  int CreateVoEChannel();
  bool DeleteVoEChannel(int channel);
  bool SetMaxSendBitrate(int bps);
  bool ValidateRtpParameters(const webrtc::RtpParameters& parameters);
  void SetupRecording();
  // Check if 'ssrc' is an unsignaled stream, and if so mark it as not being
  // unsignaled anymore (i.e. it is now removed, or signaled), and return true.
  bool MaybeDeregisterUnsignaledRecvStream(uint32_t ssrc);

  rtc::ThreadChecker worker_thread_checker_;

  WebRtcVoiceEngine* const engine_ = nullptr;
  std::vector<AudioCodec> send_codecs_;

  // TODO(kwiberg): decoder_map_ and recv_codecs_ store the exact same
  // information, in slightly different formats. Eliminate recv_codecs_.
  std::map<int, webrtc::SdpAudioFormat> decoder_map_;
  std::vector<AudioCodec> recv_codecs_;

  int max_send_bitrate_bps_ = 0;
  AudioOptions options_;
  rtc::Optional<int> dtmf_payload_type_;
  int dtmf_payload_freq_ = -1;
  bool recv_transport_cc_enabled_ = false;
  bool recv_nack_enabled_ = false;
  bool desired_playout_ = false;
  bool playout_ = false;
  bool send_ = false;
  webrtc::Call* const call_ = nullptr;

  // Queue of unsignaled SSRCs; oldest at the beginning.
  std::vector<uint32_t> unsignaled_recv_ssrcs_;

  // Volume for unsignaled streams, which may be set before the stream exists.
  double default_recv_volume_ = 1.0;
  // Sink for latest unsignaled stream - may be set before the stream exists.
  std::unique_ptr<webrtc::AudioSinkInterface> default_sink_;
  // Default SSRC to use for RTCP receiver reports in case of no signaled
  // send streams. See: https://code.google.com/p/webrtc/issues/detail?id=4740
  // and https://code.google.com/p/chromium/issues/detail?id=547661
  uint32_t receiver_reports_ssrc_ = 0xFA17FA17u;

  class WebRtcAudioSendStream;
  std::map<uint32_t, WebRtcAudioSendStream*> send_streams_;
  std::vector<webrtc::RtpExtension> send_rtp_extensions_;

  class WebRtcAudioReceiveStream;
  std::map<uint32_t, WebRtcAudioReceiveStream*> recv_streams_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;

  rtc::Optional<webrtc::AudioSendStream::Config::SendCodecSpec>
      send_codec_spec_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcVoiceMediaChannel);
};
}  // namespace cricket

#endif  // MEDIA_ENGINE_WEBRTCVOICEENGINE_H_
