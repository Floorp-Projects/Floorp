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

#include "common_types.h"  // NOLINT
#include "typedefs.h"  //NOLINT
#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"
#include "modules/video_processing/main/interface/video_processing.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_file_recorder.h"
#include "video_engine/vie_frame_provider_base.h"

namespace webrtc {

class CriticalSectionWrapper;
class PacedSender;
class ProcessThread;
class QMVideoSettingsCallback;
class RtpRtcp;
class VideoCodingModule;
class ViEBitrateObserver;
class ViEEffectFilter;
class ViEEncoderObserver;
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

  ViEEncoder(WebRtc_Word32 engine_id,
             WebRtc_Word32 channel_id,
             WebRtc_UWord32 number_of_cores,
             ProcessThread& module_process_thread,
             BitrateController* bitrate_controller);
  ~ViEEncoder();

  bool Init();

  // Returns the id of the owning channel.
  int Owner() const;

  // Drops incoming packets before they get to the encoder.
  void Pause();
  void Restart();

  WebRtc_Word32 DropDeltaAfterKey(bool enable);

  // Codec settings.
  WebRtc_UWord8 NumberOfCodecs();
  WebRtc_Word32 GetCodec(WebRtc_UWord8 list_index, VideoCodec* video_codec);
  WebRtc_Word32 RegisterExternalEncoder(VideoEncoder* encoder,
                                        WebRtc_UWord8 pl_type);
  WebRtc_Word32 DeRegisterExternalEncoder(WebRtc_UWord8 pl_type);
  WebRtc_Word32 SetEncoder(const VideoCodec& video_codec);
  WebRtc_Word32 GetEncoder(VideoCodec* video_codec);

  WebRtc_Word32 GetCodecConfigParameters(
    unsigned char config_parameters[kConfigParameterSize],
    unsigned char& config_parameters_size);

  PacedSender* GetPacedSender();

  // Scale or crop/pad image.
  WebRtc_Word32 ScaleInputImage(bool enable);

  // RTP settings.
  RtpRtcp* SendRtpRtcpModule();

  // Implementing ViEFrameCallback.
  virtual void DeliverFrame(int id,
                            I420VideoFrame* video_frame,
                            int num_csrcs = 0,
                            const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL);
  virtual void DelayChanged(int id, int frame_delay);
  virtual int GetPreferedFrameSettings(int* width,
                                       int* height,
                                       int* frame_rate);

  virtual void ProviderDestroyed(int id) {
    return;
  }

  WebRtc_Word32 SendKeyFrame();
  WebRtc_Word32 SendCodecStatistics(WebRtc_UWord32* num_key_frames,
                                    WebRtc_UWord32* num_delta_frames);

  WebRtc_Word32 EstimatedSendBandwidth(
        WebRtc_UWord32* available_bandwidth) const;

  int CodecTargetBitrate(WebRtc_UWord32* bitrate) const;
  // Loss protection.
  WebRtc_Word32 UpdateProtectionMethod();

  // Implements VCMPacketizationCallback.
  virtual WebRtc_Word32 SendData(
    FrameType frame_type,
    WebRtc_UWord8 payload_type,
    WebRtc_UWord32 time_stamp,
    int64_t capture_time_ms,
    const WebRtc_UWord8* payload_data,
    WebRtc_UWord32 payload_size,
    const RTPFragmentationHeader& fragmentation_header,
    const RTPVideoHeader* rtp_video_hdr);

  // Implements VideoProtectionCallback.
  virtual int ProtectionRequest(
      const FecProtectionParams* delta_fec_params,
      const FecProtectionParams* key_fec_params,
      WebRtc_UWord32* sent_video_rate_bps,
      WebRtc_UWord32* sent_nack_rate_bps,
      WebRtc_UWord32* sent_fec_rate_bps);

  // Implements VideoSendStatisticsCallback.
  virtual WebRtc_Word32 SendStatistics(const WebRtc_UWord32 bit_rate,
                                       const WebRtc_UWord32 frame_rate);
  WebRtc_Word32 RegisterCodecObserver(ViEEncoderObserver* observer);

  // Implements RtcpIntraFrameObserver.
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc);
  virtual void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id);
  virtual void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id);
  virtual void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc);

  // Sets SSRCs for all streams.
  bool SetSsrcs(const std::list<unsigned int>& ssrcs);

  // Effect filter.
  WebRtc_Word32 RegisterEffectFilter(ViEEffectFilter* effect_filter);

  // Recording.
  ViEFileRecorder& GetOutgoingFileRecorder();

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
  void TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                        int64_t capture_time_ms);

 private:
  WebRtc_Word32 engine_id_;
  const int channel_id_;
  const WebRtc_UWord32 number_of_cores_;

  VideoCodingModule& vcm_;
  VideoProcessingModule& vpm_;
  scoped_ptr<RtpRtcp> default_rtp_rtcp_;
  scoped_ptr<CriticalSectionWrapper> callback_cs_;
  scoped_ptr<CriticalSectionWrapper> data_cs_;
  scoped_ptr<BitrateObserver> bitrate_observer_;
  scoped_ptr<PacedSender> paced_sender_;
  scoped_ptr<ViEPacedSenderCallback> pacing_callback_;

  BitrateController* bitrate_controller_;

  bool paused_;
  std::map<unsigned int, int64_t> time_last_intra_request_ms_;
  WebRtc_Word32 channels_dropping_delta_frames_;
  bool drop_next_frame_;

  bool fec_enabled_;
  bool nack_enabled_;

  ViEEncoderObserver* codec_observer_;
  ViEEffectFilter* effect_filter_;
  ProcessThread& module_process_thread_;

  bool has_received_sli_;
  WebRtc_UWord8 picture_id_sli_;
  bool has_received_rpsi_;
  WebRtc_UWord64 picture_id_rpsi_;
  std::map<unsigned int, int> ssrc_streams_;

  ViEFileRecorder file_recorder_;

  // Quality modes callback
  QMVideoSettingsCallback* qm_callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_
