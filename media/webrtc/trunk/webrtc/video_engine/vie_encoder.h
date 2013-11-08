/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_

#include <list>
#include <map>

#include "webrtc/common_types.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_frame_provider_base.h"

namespace webrtc {

class Config;
class CriticalSectionWrapper;
class PacedSender;
class ProcessThread;
class QMVideoSettingsCallback;
class RtpRtcp;
class ViEBitrateObserver;
class ViEEffectFilter;
class ViEEncoderObserver;
class VideoCodingModule;
class ViEPacedSenderCallback;

class ViEEncoder
    : public RtcpIntraFrameObserver,
      public VCMPacketizationCallback,
      public VCMProtectionCallback,
      public VCMSendStatisticsCallback,
      public ViEFrameCallback {
 public:
  friend class ViEBitrateObserver;
  friend class ViEPacedSenderCallback;

  ViEEncoder(int32_t engine_id,
             int32_t channel_id,
             uint32_t number_of_cores,
             const Config& config,
             ProcessThread& module_process_thread,
             BitrateController* bitrate_controller);
  ~ViEEncoder();

  bool Init();

  void SetNetworkTransmissionState(bool is_transmitting);

  // Returns the id of the owning channel.
  int Owner() const;

  // Drops incoming packets before they get to the encoder.
  void Pause();
  void Restart();

  int32_t DropDeltaAfterKey(bool enable);

  // Codec settings.
  uint8_t NumberOfCodecs();
  int32_t GetCodec(uint8_t list_index, VideoCodec* video_codec);
  int32_t RegisterExternalEncoder(VideoEncoder* encoder,
                                  uint8_t pl_type,
                                  bool internal_source);
  int32_t DeRegisterExternalEncoder(uint8_t pl_type);
  int32_t SetEncoder(const VideoCodec& video_codec);
  int32_t GetEncoder(VideoCodec* video_codec);

  int32_t GetCodecConfigParameters(
    unsigned char config_parameters[kConfigParameterSize],
    unsigned char& config_parameters_size);

  PacedSender* GetPacedSender();

  // Scale or crop/pad image.
  int32_t ScaleInputImage(bool enable);

  // RTP settings.
  RtpRtcp* SendRtpRtcpModule();

  // Implementing ViEFrameCallback.
  virtual void DeliverFrame(int id,
                            I420VideoFrame* video_frame,
                            int num_csrcs = 0,
                            const uint32_t CSRC[kRtpCsrcSize] = NULL);
  virtual void DelayChanged(int id, int frame_delay);
  virtual int GetPreferedFrameSettings(int* width,
                                       int* height,
                                       int* frame_rate);

  virtual void ProviderDestroyed(int id) {
    return;
  }

  int32_t SendKeyFrame();
  int32_t SendCodecStatistics(uint32_t* num_key_frames,
                              uint32_t* num_delta_frames);

  int32_t EstimatedSendBandwidth(
        uint32_t* available_bandwidth) const;

  int CodecTargetBitrate(uint32_t* bitrate) const;
  // Loss protection.
  int32_t UpdateProtectionMethod(bool enable_nack);
  bool nack_enabled() const { return nack_enabled_; }

  // Buffering mode.
  void SetSenderBufferingMode(int target_delay_ms);

  // Implements VCMPacketizationCallback.
  virtual int32_t SendData(
    FrameType frame_type,
    uint8_t payload_type,
    uint32_t time_stamp,
    int64_t capture_time_ms,
    const uint8_t* payload_data,
    uint32_t payload_size,
    const RTPFragmentationHeader& fragmentation_header,
    const RTPVideoHeader* rtp_video_hdr);

  // Implements VideoProtectionCallback.
  virtual int ProtectionRequest(
      const FecProtectionParams* delta_fec_params,
      const FecProtectionParams* key_fec_params,
      uint32_t* sent_video_rate_bps,
      uint32_t* sent_nack_rate_bps,
      uint32_t* sent_fec_rate_bps);

  // Implements VideoSendStatisticsCallback.
  virtual int32_t SendStatistics(const uint32_t bit_rate,
                                 const uint32_t frame_rate);
  int32_t RegisterCodecObserver(ViEEncoderObserver* observer);

  // Implements RtcpIntraFrameObserver.
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc);
  virtual void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id);
  virtual void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id);
  virtual void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc);

  // Sets SSRCs for all streams.
  bool SetSsrcs(const std::list<unsigned int>& ssrcs);

  // Effect filter.
  int32_t RegisterEffectFilter(ViEEffectFilter* effect_filter);

  // Enables recording of debugging information.
  virtual int StartDebugRecording(const char* fileNameUTF8);

  // Disables recording of debugging information.
  virtual int StopDebugRecording();

  int channel_id() const { return channel_id_; }
 protected:
  // Called by BitrateObserver.
  void OnNetworkChanged(const uint32_t bitrate_bps,
                        const uint8_t fraction_lost,
                        const uint32_t round_trip_time_ms);

  // Called by PacedSender.
  bool TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                        int64_t capture_time_ms);
  int TimeToSendPadding(int bytes);

 private:
  bool EncoderPaused() const;

  int32_t engine_id_;
  const int channel_id_;
  const uint32_t number_of_cores_;

  VideoCodingModule& vcm_;
  VideoProcessingModule& vpm_;
  scoped_ptr<RtpRtcp> default_rtp_rtcp_;
  scoped_ptr<CriticalSectionWrapper> callback_cs_;
  scoped_ptr<CriticalSectionWrapper> data_cs_;
  scoped_ptr<BitrateObserver> bitrate_observer_;
  scoped_ptr<PacedSender> paced_sender_;
  scoped_ptr<ViEPacedSenderCallback> pacing_callback_;

  BitrateController* bitrate_controller_;

  bool send_padding_;
  int target_delay_ms_;
  bool network_is_transmitting_;
  bool encoder_paused_;
  bool encoder_paused_and_dropped_frame_;
  std::map<unsigned int, int64_t> time_last_intra_request_ms_;
  int32_t channels_dropping_delta_frames_;
  bool drop_next_frame_;

  bool fec_enabled_;
  bool nack_enabled_;

  ViEEncoderObserver* codec_observer_;
  ViEEffectFilter* effect_filter_;
  ProcessThread& module_process_thread_;

  bool has_received_sli_;
  uint8_t picture_id_sli_;
  bool has_received_rpsi_;
  uint64_t picture_id_rpsi_;
  std::map<unsigned int, int> ssrc_streams_;

  // Quality modes callback
  QMVideoSettingsCallback* qm_callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_
