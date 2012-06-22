/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_encoder.h"

#include <cassert>

#include "critical_section_wrapper.h"
#include "process_thread.h"
#include "rtp_rtcp.h"
#include "tick_util.h"
#include "trace.h"
#include "video_codec_interface.h"
#include "video_coding.h"
#include "video_coding_defines.h"
#include "vie_codec.h"
#include "vie_defines.h"
#include "vie_image_process.h"

namespace webrtc {

class QMVideoSettingsCallback : public VCMQMSettingsCallback {
 public:
  QMVideoSettingsCallback(WebRtc_Word32 engine_id,
                          WebRtc_Word32 channel_id,
                          VideoProcessingModule* vpm,
                          VideoCodingModule* vcm,
                          WebRtc_Word32 num_of_cores,
                          WebRtc_Word32 max_payload_length);
  ~QMVideoSettingsCallback();

  // Update VPM with QM (quality modes: frame size & frame rate) settings.
  WebRtc_Word32 SetVideoQMSettings(const WebRtc_UWord32 frame_rate,
                                   const WebRtc_UWord32 width,
                                   const WebRtc_UWord32 height);

  void SetMaxPayloadLength(WebRtc_Word32 max_payload_length);

 private:
  WebRtc_Word32 engine_id_;
  WebRtc_Word32 channel_id_;
  VideoProcessingModule* vpm_;
  VideoCodingModule* vcm_;
  WebRtc_Word32 num_cores_;
  WebRtc_Word32 max_payload_length_;
};


ViEEncoder::ViEEncoder(WebRtc_Word32 engine_id, WebRtc_Word32 channel_id,
                       WebRtc_UWord32 number_of_cores,
                       ProcessThread& module_process_thread)
  : engine_id_(engine_id),
    channel_id_(channel_id),
    number_of_cores_(number_of_cores),
    vcm_(*webrtc::VideoCodingModule::Create(ViEModuleId(engine_id,
                                                        channel_id))),
    vpm_(*webrtc::VideoProcessingModule::Create(ViEModuleId(engine_id,
                                                            channel_id))),
    default_rtp_rtcp_(*RtpRtcp::CreateRtpRtcp(
        ViEModuleId(engine_id, channel_id), false)),
    callback_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    paused_(false),
    channels_dropping_delta_frames_(0),
    drop_next_frame_(false),
    fec_enabled_(false),
    nack_enabled_(false),
    codec_observer_(NULL),
    effect_filter_(NULL),
    module_process_thread_(module_process_thread),
    has_received_sli_(false),
    picture_id_sli_(0),
    has_received_rpsi_(false),
    picture_id_rpsi_(0),
    file_recorder_(channel_id),
    qm_callback_(NULL) {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
               ViEId(engine_id, channel_id),
               "%s(engine_id: %d) 0x%p - Constructor", __FUNCTION__, engine_id,
               this);

  time_last_intra_request_ms_ = 0;

}

bool ViEEncoder::Init() {
  if (vcm_.InitializeSender() != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s InitializeSender failure", __FUNCTION__);
    return false;
  }
  vpm_.EnableTemporalDecimation(true);

  // Enable/disable content analysis: off by default for now.
  vpm_.EnableContentAnalysis(false);

  if (module_process_thread_.RegisterModule(&vcm_) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterModule failure", __FUNCTION__);
    return false;
  }
  if (default_rtp_rtcp_.InitSender() != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s InitSender failure", __FUNCTION__);
    return false;
  }
  if (default_rtp_rtcp_.RegisterIncomingVideoCallback(this) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterIncomingVideoCallback failure", __FUNCTION__);
    return false;
  }
  if (default_rtp_rtcp_.RegisterIncomingRTCPCallback(this) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterIncomingRTCPCallback failure", __FUNCTION__);
    return false;
  }
  if (module_process_thread_.RegisterModule(&default_rtp_rtcp_) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterModule failure", __FUNCTION__);
    return false;
  }

  if (qm_callback_) {
    delete qm_callback_;
  }
  qm_callback_ = new QMVideoSettingsCallback(
      engine_id_,
      channel_id_,
      &vpm_,
      &vcm_,
      number_of_cores_,
      default_rtp_rtcp_.MaxDataPayloadLength());

#ifdef VIDEOCODEC_VP8
  VideoCodec video_codec;
  if (vcm_.Codec(webrtc::kVideoCodecVP8, &video_codec) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s Codec failure", __FUNCTION__);
    return false;
  }
  if (vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                             default_rtp_rtcp_.MaxDataPayloadLength()) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterSendCodec failure", __FUNCTION__);
    return false;
  }
  if (default_rtp_rtcp_.RegisterSendPayload(video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s RegisterSendPayload failure", __FUNCTION__);
    return false;
  }
#else
  VideoCodec video_codec;
  if (vcm_.Codec(webrtc::kVideoCodecI420, &video_codec) == VCM_OK) {
    vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                           default_rtp_rtcp_.MaxDataPayloadLength());
    default_rtp_rtcp_.RegisterSendPayload(video_codec);
  } else {
    return false;
  }
#endif

  if (vcm_.RegisterTransportCallback(this) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "ViEEncoder: VCM::RegisterTransportCallback failure");
    return false;
  }
  if (vcm_.RegisterSendStatisticsCallback(this) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "ViEEncoder: VCM::RegisterSendStatisticsCallback failure");
    return false;
  }
  if (vcm_.RegisterVideoQMCallback(qm_callback_) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "VCM::RegisterQMCallback failure");
    return false;
  }
  return true;
}

ViEEncoder::~ViEEncoder() {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "ViEEncoder Destructor 0x%p, engine_id: %d", this, engine_id_);

  if (default_rtp_rtcp_.NumberChildModules() > 0) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Channels still attached %d, leaking memory",
                 default_rtp_rtcp_.NumberChildModules());
    return;
  }
  module_process_thread_.DeRegisterModule(&vcm_);
  module_process_thread_.DeRegisterModule(&vpm_);
  module_process_thread_.DeRegisterModule(&default_rtp_rtcp_);
  delete &vcm_;
  delete &vpm_;
  delete &default_rtp_rtcp_;
  delete qm_callback_;
}

int ViEEncoder::Owner() const {
  return channel_id_;
}

void ViEEncoder::Pause() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  CriticalSectionScoped cs(data_cs_.get());
  paused_ = true;
}

void ViEEncoder::Restart() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  CriticalSectionScoped cs(data_cs_.get());
  paused_ = false;
}

WebRtc_Word32 ViEEncoder::DropDeltaAfterKey(bool enable) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(%d)", __FUNCTION__, enable);
  CriticalSectionScoped cs(data_cs_.get());

  if (enable) {
    channels_dropping_delta_frames_++;
  } else {
    channels_dropping_delta_frames_--;
    if (channels_dropping_delta_frames_ < 0) {
      channels_dropping_delta_frames_ = 0;
      WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Called too many times", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

WebRtc_UWord8 ViEEncoder::NumberOfCodecs() {
  return vcm_.NumberOfCodecs();
}

WebRtc_Word32 ViEEncoder::GetCodec(WebRtc_UWord8 list_index,
                                   webrtc::VideoCodec& video_codec) {
  if (vcm_.Codec(list_index, &video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: Could not get codec",
                 __FUNCTION__);
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::RegisterExternalEncoder(webrtc::VideoEncoder* encoder,
                                                  WebRtc_UWord8 pl_type) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s: pltype %u", __FUNCTION__,
               pl_type);

  if (encoder == NULL)
    return -1;

  if (vcm_.RegisterExternalEncoder(encoder, pl_type) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not register external encoder");
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::DeRegisterExternalEncoder(WebRtc_UWord8 pl_type) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s: pltype %u", __FUNCTION__, pl_type);

  webrtc::VideoCodec current_send_codec;
  if (vcm_.SendCodec(&current_send_codec) == VCM_OK) {
    if (vcm_.Bitrate(&current_send_codec.startBitrate) != 0) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "Failed to get the current encoder target bitrate.");
    }
  }

  if (vcm_.RegisterExternalEncoder(NULL, pl_type) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not deregister external encoder");
    return -1;
  }

  // If the external encoder is the current send codeci, use vcm internal
  // encoder.
  if (current_send_codec.plType == pl_type) {
    WebRtc_UWord16 max_data_payload_length =
        default_rtp_rtcp_.MaxDataPayloadLength();
    if (vcm_.RegisterSendCodec(&current_send_codec, number_of_cores_,
                               max_data_payload_length) != VCM_OK) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "Could not use internal encoder");
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::SetEncoder(const webrtc::VideoCodec& video_codec) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s: CodecType: %d, width: %u, height: %u", __FUNCTION__,
               video_codec.codecType, video_codec.width, video_codec.height);

  // Convert from kbps to bps.
  default_rtp_rtcp_.SetSendBitrate(video_codec.startBitrate * 1000,
                                   video_codec.minBitrate,
                                   video_codec.maxBitrate);

  // Setting target width and height for VPM.
  if (vpm_.SetTargetResolution(video_codec.width, video_codec.height,
                               video_codec.maxFramerate) != VPM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not set VPM target dimensions");
    return -1;
  }

  if (default_rtp_rtcp_.RegisterSendPayload(video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could register RTP module video payload");
    return -1;
  }

  WebRtc_UWord16 max_data_payload_length =
      default_rtp_rtcp_.MaxDataPayloadLength();

  qm_callback_->SetMaxPayloadLength(max_data_payload_length);

  if (vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                             max_data_payload_length) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not register send codec");
    return -1;
  }

  data_cs_->Enter();
  memcpy(&send_codec_, &video_codec, sizeof(send_codec_));
  data_cs_->Leave();

  // Set this module as sending right away, let the slave module in the channel
  // start and stop sending.
  if (default_rtp_rtcp_.Sending() == false) {
    if (default_rtp_rtcp_.SetSendingStatus(true) != 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "Could start RTP module sending");
      return -1;
    }
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::GetEncoder(webrtc::VideoCodec& video_codec) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  if (vcm_.SendCodec(&video_codec) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not get VCM send codec");
    return -1;
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::GetCodecConfigParameters(
    unsigned char config_parameters[kConfigParameterSize],
    unsigned char& config_parameters_size) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  WebRtc_Word32 num_parameters =
      vcm_.CodecConfigParameters(config_parameters, kConfigParameterSize);
  if (num_parameters <= 0) {
    config_parameters_size = 0;
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not get config parameters");
    return -1;
  }
  config_parameters_size = static_cast<unsigned char>(num_parameters);
  return 0;
}

WebRtc_Word32 ViEEncoder::ScaleInputImage(bool enable) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s(enable %d)", __FUNCTION__,
               enable);

  VideoFrameResampling resampling_mode = kFastRescaling;
  if (enable == true) {
    // kInterpolation is currently not supported.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s not supported",
                 __FUNCTION__, enable);
    return -1;
  }
  vpm_.SetInputFrameResampleMode(resampling_mode);

  return 0;
}

RtpRtcp* ViEEncoder::SendRtpRtcpModule() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  return &default_rtp_rtcp_;
}

void ViEEncoder::DeliverFrame(int id, webrtc::VideoFrame& video_frame,
                              int num_csrcs,
                              const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s: %llu", __FUNCTION__,
               video_frame.TimeStamp());

  {
    CriticalSectionScoped cs(data_cs_.get());
    if (paused_ || default_rtp_rtcp_.SendingMedia() == false) {
      // We've paused or we have no channels attached, don't encode.
      return;
    }
    if (drop_next_frame_) {
      // Drop this frame.
      WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Dropping frame %llu after a key fame", __FUNCTION__,
                   video_frame.TimeStamp());
      drop_next_frame_ = false;
      return;
    }
  }

  // Convert render time, in ms, to RTP timestamp.
  const WebRtc_UWord32 time_stamp =
      90 * static_cast<WebRtc_UWord32>(video_frame.RenderTimeMs());
  video_frame.SetTimeStamp(time_stamp);
  {
    CriticalSectionScoped cs(callback_cs_.get());
    if (effect_filter_) {
      effect_filter_->Transform(video_frame.Length(), video_frame.Buffer(),
                                video_frame.TimeStamp(),
                                video_frame.Width(), video_frame.Height());
    }
  }
  // Record raw frame.
  file_recorder_.RecordVideoFrame(video_frame);

  // Make sure the CSRC list is correct.
  if (num_csrcs > 0) {
    WebRtc_UWord32 tempCSRC[kRtpCsrcSize];
    for (int i = 0; i < num_csrcs; i++) {
      if (CSRC[i] == 1) {
        tempCSRC[i] = default_rtp_rtcp_.SSRC();
      } else {
        tempCSRC[i] = CSRC[i];
      }
    }
    default_rtp_rtcp_.SetCSRCs(tempCSRC, (WebRtc_UWord8) num_csrcs);
  }

#ifdef VIDEOCODEC_VP8
  if (vcm_.SendCodec() == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;
    if (has_received_sli_ || has_received_rpsi_) {
      {
        codec_specific_info.codecSpecific.VP8.hasReceivedRPSI =
          has_received_rpsi_;
        codec_specific_info.codecSpecific.VP8.hasReceivedSLI =
          has_received_sli_;
        codec_specific_info.codecSpecific.VP8.pictureIdRPSI =
          picture_id_rpsi_;
        codec_specific_info.codecSpecific.VP8.pictureIdSLI  =
          picture_id_sli_;
      }
      has_received_sli_ = false;
      has_received_rpsi_ = false;
    }
    VideoFrame* decimated_frame = NULL;
    const int ret = vpm_.PreprocessFrame(&video_frame, &decimated_frame);
    if (ret == 1) {
      // Drop this frame.
      return;
    } else if (ret != VPM_OK) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Error preprocessing frame %u", __FUNCTION__,
                   video_frame.TimeStamp());
      return;
    }

    VideoContentMetrics* content_metrics = NULL;
    content_metrics = vpm_.ContentMetrics();

    // Frame was not re-sampled => use original.
    if (decimated_frame == NULL)  {
      decimated_frame = &video_frame;
    }

    if (vcm_.AddVideoFrame(*decimated_frame, content_metrics,
                           &codec_specific_info) != VCM_OK) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Error encoding frame %u", __FUNCTION__,
                   video_frame.TimeStamp());
    }
    return;
  }
#endif
  // TODO(mflodman) Rewrite this to use code common to VP8 case.
  // Pass frame via preprocessor.
  VideoFrame* decimated_frame = NULL;
  const int ret = vpm_.PreprocessFrame(&video_frame, &decimated_frame);
  if (ret == 1) {
    // Drop this frame.
    return;
  } else if (ret != VPM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Error preprocessing frame %u", __FUNCTION__,
                 video_frame.TimeStamp());
    return;
  }

  // Frame was not sampled => use original.
  if (decimated_frame == NULL)  {
    decimated_frame = &video_frame;
  }
  if (vcm_.AddVideoFrame(*decimated_frame) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: Error encoding frame %u",
                 __FUNCTION__, video_frame.TimeStamp());
  }
}

void ViEEncoder::DelayChanged(int id, int frame_delay) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s: %u", __FUNCTION__,
               frame_delay);

  default_rtp_rtcp_.SetCameraDelay(frame_delay);
  file_recorder_.SetFrameDelay(frame_delay);
}

int ViEEncoder::GetPreferedFrameSettings(int& width,
                                         int& height,
                                         int& frame_rate) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  if (vcm_.SendCodec(&video_codec) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "Could not get VCM send codec");
    return -1;
  }

  width = video_codec.width;
  height = video_codec.height;
  frame_rate = video_codec.maxFramerate;
  return 0;
}

int ViEEncoder::SendKeyFrame() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);
  return vcm_.IntraFrameRequest();
}

WebRtc_Word32 ViEEncoder::SendCodecStatistics(
    WebRtc_UWord32& num_key_frames, WebRtc_UWord32& num_delta_frames) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  webrtc::VCMFrameCount sent_frames;
  if (vcm_.SentFrameCount(sent_frames) != VCM_OK) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Could not get sent frame information", __FUNCTION__);
    return -1;
  }
  num_key_frames = sent_frames.numKeyFrames;
  num_delta_frames = sent_frames.numDeltaFrames;
  return 0;
}

WebRtc_Word32 ViEEncoder::EstimatedSendBandwidth(
    WebRtc_UWord32* available_bandwidth) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  return default_rtp_rtcp_.EstimatedSendBandwidth(available_bandwidth);
}

int ViEEncoder::CodecTargetBitrate(WebRtc_UWord32* bitrate) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (vcm_.Bitrate(bitrate) != 0)
    return -1;
  return 0;
}

WebRtc_Word32 ViEEncoder::UpdateProtectionMethod() {
  bool fec_enabled = false;
  WebRtc_UWord8 dummy_ptype_red = 0;
  WebRtc_UWord8 dummy_ptypeFEC = 0;

  // Updated protection method to VCM to get correct packetization sizes.
  // FEC has larger overhead than NACK -> set FEC if used.
  WebRtc_Word32 error = default_rtp_rtcp_.GenericFECStatus(fec_enabled,
                                                           dummy_ptype_red,
                                                           dummy_ptypeFEC);
  if (error) {
    return -1;
  }

  bool nack_enabled = (default_rtp_rtcp_.NACK() == kNackOff) ? false : true;
  if (fec_enabled_ == fec_enabled && nack_enabled_ == nack_enabled) {
    // No change needed, we're already in correct state.
    return 0;
  }
  fec_enabled_ = fec_enabled;
  nack_enabled_ = nack_enabled;

  // Set Video Protection for VCM.
  if (fec_enabled && nack_enabled) {
    vcm_.SetVideoProtection(webrtc::kProtectionNackFEC, true);
  } else {
    vcm_.SetVideoProtection(webrtc::kProtectionFEC, fec_enabled_);
    vcm_.SetVideoProtection(webrtc::kProtectionNack, nack_enabled_);
    vcm_.SetVideoProtection(webrtc::kProtectionNackFEC, false);
  }

  if (fec_enabled || nack_enabled) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: FEC status ",
                 __FUNCTION__, fec_enabled);
    vcm_.RegisterProtectionCallback(this);
    // The send codec must be registered to set correct MTU.
    webrtc::VideoCodec codec;
    if (vcm_.SendCodec(&codec) == 0) {
      WebRtc_UWord16 max_pay_load = default_rtp_rtcp_.MaxDataPayloadLength();
      if (vcm_.Bitrate(&codec.startBitrate) != 0) {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                     ViEId(engine_id_, channel_id_),
                     "Failed to get the current encoder target bitrate.");
      }
      if (vcm_.RegisterSendCodec(&codec, number_of_cores_, max_pay_load) != 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(engine_id_, channel_id_),
                     "%s: Failed to update Sendcodec when enabling FEC",
                     __FUNCTION__, fec_enabled);
        return -1;
      }
    }
    return 0;
  } else {
    // FEC and NACK are disabled.
    vcm_.RegisterProtectionCallback(NULL);
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::SendData(
    const FrameType frame_type,
    const WebRtc_UWord8 payload_type,
    const WebRtc_UWord32 time_stamp,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord32 payload_size,
    const webrtc::RTPFragmentationHeader& fragmentation_header,
    const RTPVideoHeader* rtp_video_hdr) {
  {
    CriticalSectionScoped cs(data_cs_.get());
    if (paused_) {
      // Paused, don't send this packet.
      return 0;
    }
    if (channels_dropping_delta_frames_ &&
        frame_type == webrtc::kVideoFrameKey) {
      WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: Sending key frame, drop next frame", __FUNCTION__);
      drop_next_frame_ = true;
    }
  }

  // New encoded data, hand over to the rtp module.
  return default_rtp_rtcp_.SendOutgoingData(frame_type, payload_type,
                                            time_stamp, payload_data,
                                            payload_size, &fragmentation_header,
                                            rtp_video_hdr);
}

WebRtc_Word32 ViEEncoder::ProtectionRequest(
    const FecProtectionParams* delta_fec_params,
    const FecProtectionParams* key_fec_params,
    WebRtc_UWord32* sent_video_rate_bps,
    WebRtc_UWord32* sent_nack_rate_bps,
    WebRtc_UWord32* sent_fec_rate_bps) {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s, deltaFECRate: %u, key_fecrate: %u, "
               "delta_use_uep_protection: %d, key_use_uep_protection: %d, ",
               __FUNCTION__,
               delta_fec_params->fec_rate,
               key_fec_params->fec_rate,
               delta_fec_params->use_uep_protection,
               key_fec_params->use_uep_protection);

  if (default_rtp_rtcp_.SetFecParameters(delta_fec_params,
                                         key_fec_params) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Could not update FEC parameters", __FUNCTION__);
  }
  default_rtp_rtcp_.BitrateSent(NULL,
                                sent_video_rate_bps,
                                sent_fec_rate_bps,
                                sent_nack_rate_bps);
  return 0;
}

WebRtc_Word32 ViEEncoder::SendStatistics(const WebRtc_UWord32 bit_rate,
                                         const WebRtc_UWord32 frame_rate) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (codec_observer_) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: bitrate %u, framerate %u",
                 __FUNCTION__, bit_rate, frame_rate);
    codec_observer_->OutgoingRate(channel_id_, frame_rate, bit_rate);
  }
  return 0;
}

WebRtc_Word32 ViEEncoder::RegisterCodecObserver(ViEEncoderObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: observer added",
                 __FUNCTION__);
    if (codec_observer_) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_), "%s: observer already set.",
                   __FUNCTION__);
      return -1;
    }
    codec_observer_ = observer;
  } else {
    if (codec_observer_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: observer does not exist.", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: observer removed",
                 __FUNCTION__);
    codec_observer_ = NULL;
  }
  return 0;
}

void ViEEncoder::OnSLIReceived(const WebRtc_Word32 id,
                               const WebRtc_UWord8 picture_id) {
  picture_id_sli_ = picture_id;
  has_received_sli_ = true;
}

void ViEEncoder::OnRPSIReceived(const WebRtc_Word32 id,
                                const WebRtc_UWord64 picture_id) {
  picture_id_rpsi_ = picture_id;
  has_received_rpsi_ = true;
}

void ViEEncoder::OnReceivedIntraFrameRequest(const WebRtc_Word32 /*id*/,
                                             const FrameType /*type*/,
                                             const WebRtc_UWord8 /*idx*/) {
  // Key frame request from remote side, signal to VCM.
  WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_), "%s", __FUNCTION__);

  WebRtc_Word64 now = TickTime::MillisecondTimestamp();
  if (time_last_intra_request_ms_ + kViEMinKeyRequestIntervalMs >
      now) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: Not not encoding new intra due to timing", __FUNCTION__);
    return;
  }
  vcm_.IntraFrameRequest();
  time_last_intra_request_ms_ = now;
}

void ViEEncoder::OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 bitrate_bps,
                                  const WebRtc_UWord8 fraction_lost,
                                  const WebRtc_UWord16 round_trip_time_ms) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(bitrate_bps: %u, fraction_lost: %u, rtt_ms: %u",
               __FUNCTION__, bitrate_bps, fraction_lost, round_trip_time_ms);

  vcm_.SetChannelParameters(bitrate_bps / 1000, fraction_lost,
                            round_trip_time_ms);
}

WebRtc_Word32 ViEEncoder::RegisterEffectFilter(ViEEffectFilter* effect_filter) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (effect_filter == NULL) {
    if (effect_filter_ == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_), "%s: no effect filter added",
                   __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: deregister effect filter",
                 __FUNCTION__);
  } else {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                 ViEId(engine_id_, channel_id_), "%s: register effect",
                 __FUNCTION__);
    if (effect_filter_) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(engine_id_, channel_id_),
                   "%s: effect filter already added ", __FUNCTION__);
      return -1;
    }
  }
  effect_filter_ = effect_filter;
  return 0;
}

ViEFileRecorder& ViEEncoder::GetOutgoingFileRecorder() {
  return file_recorder_;
}

QMVideoSettingsCallback::QMVideoSettingsCallback(
    WebRtc_Word32 engine_id,
    WebRtc_Word32 channel_id,
    VideoProcessingModule* vpm,
    VideoCodingModule* vcm,
    WebRtc_Word32 num_cores,
    WebRtc_Word32 max_payload_length)
    : engine_id_(engine_id),
      channel_id_(channel_id),
      vpm_(vpm),
      vcm_(vcm),
      num_cores_(num_cores),
      max_payload_length_(max_payload_length) {
}

QMVideoSettingsCallback::~QMVideoSettingsCallback() {
}

WebRtc_Word32 QMVideoSettingsCallback::SetVideoQMSettings(
    const WebRtc_UWord32 frame_rate,
    const WebRtc_UWord32 width,
    const WebRtc_UWord32 height) {
  return vpm_->SetTargetResolution(width, height, frame_rate);
}

void QMVideoSettingsCallback::SetMaxPayloadLength(
    WebRtc_Word32 max_payload_length) {
  max_payload_length_ = max_payload_length;
}

}  // namespace webrtc
