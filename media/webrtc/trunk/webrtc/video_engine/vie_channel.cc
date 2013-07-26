/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_channel.h"

#include <algorithm>
#include <vector>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/pacing/include/paced_sender.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/utility/interface/process_thread.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/thread_wrapper.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/call_stats.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/include/vie_image_process.h"
#include "video_engine/include/vie_rtp_rtcp.h"
#include "video_engine/vie_defines.h"

namespace webrtc {

const int kMaxDecodeWaitTimeMs = 50;
const int kInvalidRtpExtensionId = 0;
static const int kMaxTargetDelayMs = 10000;

// Helper class receiving statistics callbacks.
class ChannelStatsObserver : public CallStatsObserver {
 public:
  explicit ChannelStatsObserver(ViEChannel* owner) : owner_(owner) {}
  virtual ~ChannelStatsObserver() {}

  // Implements StatsObserver.
  virtual void OnRttUpdate(uint32_t rtt) {
    owner_->OnRttUpdate(rtt);
  }

 private:
  ViEChannel* owner_;
};

ViEChannel::ViEChannel(int32_t channel_id,
                       int32_t engine_id,
                       uint32_t number_of_cores,
                       ProcessThread& module_process_thread,
                       RtcpIntraFrameObserver* intra_frame_observer,
                       RtcpBandwidthObserver* bandwidth_observer,
                       RemoteBitrateEstimator* remote_bitrate_estimator,
                       RtcpRttObserver* rtt_observer,
                       PacedSender* paced_sender,
                       RtpRtcp* default_rtp_rtcp,
                       bool sender)
    : ViEFrameProviderBase(channel_id, engine_id),
      channel_id_(channel_id),
      engine_id_(engine_id),
      number_of_cores_(number_of_cores),
      num_socket_threads_(kViESocketThreads),
      callback_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      rtp_rtcp_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      default_rtp_rtcp_(default_rtp_rtcp),
      rtp_rtcp_(NULL),
      vcm_(*VideoCodingModule::Create(ViEModuleId(engine_id, channel_id))),
      vie_receiver_(channel_id, &vcm_, remote_bitrate_estimator),
      vie_sender_(channel_id),
      vie_sync_(&vcm_, this),
      stats_observer_(new ChannelStatsObserver(this)),
      module_process_thread_(module_process_thread),
      codec_observer_(NULL),
      do_key_frame_callbackRequest_(false),
      rtp_observer_(NULL),
      rtcp_observer_(NULL),
      networkObserver_(NULL),
      intra_frame_observer_(intra_frame_observer),
      rtt_observer_(rtt_observer),
      paced_sender_(paced_sender),
      bandwidth_observer_(bandwidth_observer),
      rtp_packet_timeout_(false),
      send_timestamp_extension_id_(kInvalidRtpExtensionId),
      using_packet_spread_(false),
      external_transport_(NULL),
      decoder_reset_(true),
      wait_for_key_frame_(false),
      decode_thread_(NULL),
      external_encryption_(NULL),
      effect_filter_(NULL),
      color_enhancement_(false),
      file_recorder_(channel_id),
      mtu_(0),
      sender_(sender),
      nack_history_size_sender_(kSendSidePacketHistorySize),
      max_nack_reordering_threshold_(kMaxPacketAgeToNack) {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, ViEId(engine_id, channel_id),
               "ViEChannel::ViEChannel(channel_id: %d, engine_id: %d)",
               channel_id, engine_id);

  RtpRtcp::Configuration configuration;
  configuration.id = ViEModuleId(engine_id, channel_id);
  configuration.audio = false;
  configuration.default_module = default_rtp_rtcp;
  configuration.incoming_data = &vie_receiver_;
  configuration.incoming_messages = this;
  configuration.outgoing_transport = &vie_sender_;
  configuration.rtcp_feedback = this;
  configuration.intra_frame_callback = intra_frame_observer;
  configuration.bandwidth_callback = bandwidth_observer;
  configuration.rtt_observer = rtt_observer;
  configuration.remote_bitrate_estimator = remote_bitrate_estimator;
  configuration.paced_sender = paced_sender;

  rtp_rtcp_.reset(RtpRtcp::CreateRtpRtcp(configuration));
  vie_receiver_.SetRtpRtcpModule(rtp_rtcp_.get());
  vcm_.SetNackSettings(kMaxNackListSize, max_nack_reordering_threshold_);
}

int32_t ViEChannel::Init() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: channel_id: %d, engine_id: %d)", __FUNCTION__, channel_id_,
               engine_id_);

  // RTP/RTCP initialization.
  if (rtp_rtcp_->SetSendingMediaStatus(false) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::SetSendingMediaStatus failure", __FUNCTION__);
    return -1;
  }
  if (module_process_thread_.RegisterModule(rtp_rtcp_.get()) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::RegisterModule failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_->SetKeyFrameRequestMethod(kKeyFrameReqFirRtp) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::SetKeyFrameRequestMethod failure", __FUNCTION__);
  }
  if (rtp_rtcp_->SetRTCPStatus(kRtcpCompound) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTP::SetRTCPStatus failure", __FUNCTION__);
  }
  if (paced_sender_) {
    if (rtp_rtcp_->SetStorePacketsStatus(true, nack_history_size_sender_) !=
        0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s:SetStorePacketsStatus failure", __FUNCTION__);
    }
  }
  // VCM initialization
  if (vcm_.InitializeReceiver() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(engine_id_, channel_id_),
                 "%s: VCM::InitializeReceiver failure", __FUNCTION__);
    return -1;
  }
  if (vcm_.RegisterReceiveCallback(this) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterReceiveCallback failure", __FUNCTION__);
    return -1;
  }
  if (vcm_.RegisterFrameTypeCallback(this) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterFrameTypeCallback failure", __FUNCTION__);
  }
  if (vcm_.RegisterReceiveStatisticsCallback(this) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterReceiveStatisticsCallback failure",
                 __FUNCTION__);
  }
  if (vcm_.SetRenderDelay(kViEDefaultRenderDelayMs) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::SetRenderDelay failure", __FUNCTION__);
  }
  if (module_process_thread_.RegisterModule(&vcm_) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: VCM::RegisterModule(vcm) failure", __FUNCTION__);
    return -1;
  }
#ifdef VIDEOCODEC_VP8
  VideoCodec video_codec;
  if (vcm_.Codec(kVideoCodecVP8, &video_codec) == VCM_OK) {
    rtp_rtcp_->RegisterSendPayload(video_codec);
    rtp_rtcp_->RegisterReceivePayload(video_codec);
    vcm_.RegisterReceiveCodec(&video_codec, number_of_cores_);
    vcm_.RegisterSendCodec(&video_codec, number_of_cores_,
                           rtp_rtcp_->MaxDataPayloadLength());
  } else {
    assert(false);
  }
#endif

  return 0;
}

ViEChannel::~ViEChannel() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, ViEId(engine_id_, channel_id_),
               "ViEChannel Destructor, channel_id: %d, engine_id: %d",
               channel_id_, engine_id_);

  // Make sure we don't get more callbacks from the RTP module.
  module_process_thread_.DeRegisterModule(rtp_rtcp_.get());
  module_process_thread_.DeRegisterModule(&vcm_);
  module_process_thread_.DeRegisterModule(&vie_sync_);
  while (simulcast_rtp_rtcp_.size() > 0) {
    std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
    RtpRtcp* rtp_rtcp = *it;
    module_process_thread_.DeRegisterModule(rtp_rtcp);
    delete rtp_rtcp;
    simulcast_rtp_rtcp_.erase(it);
  }
  while (removed_rtp_rtcp_.size() > 0) {
    std::list<RtpRtcp*>::iterator it = removed_rtp_rtcp_.begin();
    delete *it;
    removed_rtp_rtcp_.erase(it);
  }
  if (decode_thread_) {
    StopDecodeThread();
  }
  // Release modules.
  VideoCodingModule::Destroy(&vcm_);
}

int32_t ViEChannel::SetSendCodec(const VideoCodec& video_codec,
                                 bool new_stream) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: codec_type: %d", __FUNCTION__, video_codec.codecType);

  if (!sender_) {
    return 0;
  }
  if (video_codec.codecType == kVideoCodecRED ||
      video_codec.codecType == kVideoCodecULPFEC) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: codec_type: %d is not a valid send codec.", __FUNCTION__,
                 video_codec.codecType);
    return -1;
  }
  if (kMaxSimulcastStreams < video_codec.numberOfSimulcastStreams) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Too many simulcast streams", __FUNCTION__);
    return -1;
  }
  // Update the RTP module with the settings.
  // Stop and Start the RTP module -> trigger new SSRC, if an SSRC hasn't been
  // set explicitly.
  bool restart_rtp = false;
  if (rtp_rtcp_->Sending() && new_stream) {
    restart_rtp = true;
    rtp_rtcp_->SetSendingStatus(false);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); ++it) {
      (*it)->SetSendingStatus(false);
      (*it)->SetSendingMediaStatus(false);
    }
  }
  NACKMethod nack_method = rtp_rtcp_->NACK();

  bool fec_enabled = false;
  uint8_t payload_type_red;
  uint8_t payload_type_fec;
  rtp_rtcp_->GenericFECStatus(fec_enabled, payload_type_red, payload_type_fec);

  CriticalSectionScoped cs(rtp_rtcp_cs_.get());

  if (video_codec.numberOfSimulcastStreams > 0) {
    // Set correct bitrate to base layer.
    // Create our simulcast RTP modules.
    int num_modules_to_add = video_codec.numberOfSimulcastStreams -
        simulcast_rtp_rtcp_.size() - 1;
    if (num_modules_to_add < 0) {
      num_modules_to_add = 0;
    }

    while (removed_rtp_rtcp_.size() > 0 && num_modules_to_add > 0) {
      RtpRtcp* rtp_rtcp = removed_rtp_rtcp_.front();
      removed_rtp_rtcp_.pop_front();
      simulcast_rtp_rtcp_.push_back(rtp_rtcp);
      rtp_rtcp->SetSendingStatus(rtp_rtcp_->Sending());
      rtp_rtcp->SetSendingMediaStatus(rtp_rtcp_->SendingMedia());
      module_process_thread_.RegisterModule(rtp_rtcp);
      --num_modules_to_add;
    }

    for (int i = 0; i < num_modules_to_add; ++i) {
      RtpRtcp::Configuration configuration;
      configuration.id = ViEModuleId(engine_id_, channel_id_);
      configuration.audio = false;  // Video.
      configuration.default_module = default_rtp_rtcp_;
      configuration.outgoing_transport = &vie_sender_;
      configuration.intra_frame_callback = intra_frame_observer_;
      configuration.bandwidth_callback = bandwidth_observer_.get();
      configuration.rtt_observer = rtt_observer_;
      configuration.paced_sender = paced_sender_;

      RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(configuration);

      // Silently ignore error.
      module_process_thread_.RegisterModule(rtp_rtcp);
      if (rtp_rtcp->SetRTCPStatus(rtp_rtcp_->RTCP()) != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: RTP::SetRTCPStatus failure", __FUNCTION__);
      }
      if (nack_method != kNackOff) {
        rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
        rtp_rtcp->SetNACKStatus(nack_method, max_nack_reordering_threshold_);
      } else if (paced_sender_) {
        rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
      }
      if (fec_enabled) {
        rtp_rtcp->SetGenericFECStatus(fec_enabled, payload_type_red,
            payload_type_fec);
      }
      rtp_rtcp->SetSendingMediaStatus(rtp_rtcp_->SendingMedia());
      simulcast_rtp_rtcp_.push_back(rtp_rtcp);
    }
    // Remove last in list if we have too many.
    for (int j = simulcast_rtp_rtcp_.size();
         j > (video_codec.numberOfSimulcastStreams - 1);
         j--) {
      RtpRtcp* rtp_rtcp = simulcast_rtp_rtcp_.back();
      module_process_thread_.DeRegisterModule(rtp_rtcp);
      rtp_rtcp->SetSendingStatus(false);
      rtp_rtcp->SetSendingMediaStatus(false);
      simulcast_rtp_rtcp_.pop_back();
      removed_rtp_rtcp_.push_front(rtp_rtcp);
    }
    uint8_t idx = 0;
    // Configure all simulcast modules.
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      idx++;
      RtpRtcp* rtp_rtcp = *it;
      rtp_rtcp->DeRegisterSendPayload(video_codec.plType);
      if (rtp_rtcp->RegisterSendPayload(video_codec) != 0) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: could not register payload type", __FUNCTION__);
        return -1;
      }
      if (mtu_ != 0) {
        rtp_rtcp->SetMaxTransferUnit(mtu_);
      }
      if (restart_rtp) {
        rtp_rtcp->SetSendingStatus(true);
        rtp_rtcp->SetSendingMediaStatus(true);
      }
      if (send_timestamp_extension_id_ != kInvalidRtpExtensionId) {
        // Deregister in case the extension was previously enabled.
        rtp_rtcp->DeregisterSendRtpHeaderExtension(
            kRtpExtensionTransmissionTimeOffset);
        if (rtp_rtcp->RegisterSendRtpHeaderExtension(
            kRtpExtensionTransmissionTimeOffset,
            send_timestamp_extension_id_) != 0) {
          WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                       "%s: could not register transmission time extension",
                       __FUNCTION__);
        }
      } else {
        rtp_rtcp->DeregisterSendRtpHeaderExtension(
            kRtpExtensionTransmissionTimeOffset);
      }
    }
    // |RegisterSimulcastRtpRtcpModules| resets all old weak pointers and old
    // modules can be deleted after this step.
    vie_receiver_.RegisterSimulcastRtpRtcpModules(simulcast_rtp_rtcp_);
  } else {
    while (!simulcast_rtp_rtcp_.empty()) {
      RtpRtcp* rtp_rtcp = simulcast_rtp_rtcp_.back();
      module_process_thread_.DeRegisterModule(rtp_rtcp);
      rtp_rtcp->SetSendingStatus(false);
      rtp_rtcp->SetSendingMediaStatus(false);
      simulcast_rtp_rtcp_.pop_back();
      removed_rtp_rtcp_.push_front(rtp_rtcp);
    }
    // Clear any previous modules.
    vie_receiver_.RegisterSimulcastRtpRtcpModules(simulcast_rtp_rtcp_);
  }
  // Enable this if H264 is available.
  // This sets the wanted packetization mode.
  // if (video_codec.plType == kVideoCodecH264) {
  //   if (video_codec.codecSpecific.H264.packetization ==  kH264SingleMode) {
  //     rtp_rtcp_->SetH264PacketizationMode(H264_SINGLE_NAL_MODE);
  //   } else {
  //     rtp_rtcp_->SetH264PacketizationMode(H264_NON_INTERLEAVED_MODE);
  //   }
  //   if (video_codec.codecSpecific.H264.configParametersSize > 0) {
  //     rtp_rtcp_->SetH264SendModeNALU_PPS_SPS(true);
  //   }
  // }

  // Don't log this error, no way to check in advance if this pl_type is
  // registered or not...
  rtp_rtcp_->DeRegisterSendPayload(video_codec.plType);
  if (rtp_rtcp_->RegisterSendPayload(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not register payload type", __FUNCTION__);
    return -1;
  }
  if (restart_rtp) {
    rtp_rtcp_->SetSendingStatus(true);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); ++it) {
      (*it)->SetSendingStatus(true);
      (*it)->SetSendingMediaStatus(true);
    }
  }
  return 0;
}

int32_t ViEChannel::SetReceiveCodec(const VideoCodec& video_codec) {
  // We will not receive simulcast streams, so no need to handle that use case.
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  int8_t old_pltype = -1;
  if (rtp_rtcp_->ReceivePayloadType(video_codec, &old_pltype) != -1) {
    rtp_rtcp_->DeRegisterReceivePayload(old_pltype);
  }

  if (rtp_rtcp_->RegisterReceivePayload(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not register receive payload type", __FUNCTION__);
    return -1;
  }

  if (video_codec.codecType != kVideoCodecRED &&
      video_codec.codecType != kVideoCodecULPFEC) {
    // Register codec type with VCM, but do not register RED or ULPFEC.
    if (vcm_.RegisterReceiveCodec(&video_codec, number_of_cores_,
                                  wait_for_key_frame_) != VCM_OK) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not register decoder", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

int32_t ViEChannel::GetReceiveCodec(VideoCodec* video_codec) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (vcm_.ReceiveCodec(video_codec) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get receive codec", __FUNCTION__);
    return -1;
  }
  return 0;
}

int32_t ViEChannel::RegisterCodecObserver(ViEDecoderObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    if (codec_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: already added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    codec_observer_ = observer;
  } else {
    if (!codec_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    codec_observer_ = NULL;
  }
  return 0;
}

int32_t ViEChannel::RegisterExternalDecoder(const uint8_t pl_type,
                                            VideoDecoder* decoder,
                                            bool buffered_rendering,
                                            int32_t render_delay) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  int32_t result;
  result = vcm_.RegisterExternalDecoder(decoder, pl_type, buffered_rendering);
  if (result != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not register external decoder with VCM.",
                 __FUNCTION__);
    return result;
  }
  return vcm_.SetRenderDelay(render_delay);
}

int32_t ViEChannel::DeRegisterExternalDecoder(
    const uint8_t pl_type) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s pl_type", __FUNCTION__, pl_type);

  VideoCodec current_receive_codec;
  int32_t result = 0;
  result = vcm_.ReceiveCodec(&current_receive_codec);
  if (vcm_.RegisterExternalDecoder(NULL, pl_type, false) != VCM_OK) {
    return -1;
  }

  if (result == 0 && current_receive_codec.plType == pl_type) {
    result = vcm_.RegisterReceiveCodec(&current_receive_codec, number_of_cores_,
                                       wait_for_key_frame_);
  }
  return result;
}

int32_t ViEChannel::ReceiveCodecStatistics(
    uint32_t* num_key_frames, uint32_t* num_delta_frames) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  VCMFrameCount received_frames;
  if (vcm_.ReceivedFrameCount(received_frames) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get received frame information", __FUNCTION__);
    return -1;
  }
  *num_key_frames = received_frames.numKeyFrames;
  *num_delta_frames = received_frames.numDeltaFrames;
  return 0;
}

uint32_t ViEChannel::DiscardedPackets() const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  return vcm_.DiscardedPackets();
}

int ViEChannel::ReceiveDelay() const {
  return vcm_.Delay();
}

int32_t ViEChannel::WaitForKeyFrame(bool wait) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(wait: %d)", __FUNCTION__, wait);
  wait_for_key_frame_ = wait;
  return 0;
}

int32_t ViEChannel::SetSignalPacketLossStatus(bool enable,
                                                    bool only_key_frames) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);
  if (enable) {
    if (only_key_frames) {
      vcm_.SetVideoProtection(kProtectionKeyOnLoss, false);
      if (vcm_.SetVideoProtection(kProtectionKeyOnKeyLoss, true) != VCM_OK) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s failed %d", __FUNCTION__, enable);
        return -1;
      }
    } else {
      vcm_.SetVideoProtection(kProtectionKeyOnKeyLoss, false);
      if (vcm_.SetVideoProtection(kProtectionKeyOnLoss, true) != VCM_OK) {
        WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s failed %d", __FUNCTION__, enable);
        return -1;
      }
    }
  } else {
    vcm_.SetVideoProtection(kProtectionKeyOnLoss, false);
    vcm_.SetVideoProtection(kProtectionKeyOnKeyLoss, false);
  }
  return 0;
}

int32_t ViEChannel::SetRTCPMode(const RTCPMethod rtcp_mode) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, rtcp_mode);

  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetRTCPStatus(rtcp_mode);
  }
  return rtp_rtcp_->SetRTCPStatus(rtcp_mode);
}

int32_t ViEChannel::GetRTCPMode(RTCPMethod* rtcp_mode) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  *rtcp_mode = rtp_rtcp_->RTCP();
  return 0;
}

int32_t ViEChannel::SetNACKStatus(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);

  // Update the decoding VCM.
  if (vcm_.SetVideoProtection(kProtectionNack, enable) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                 enable);
    return -1;
  }
  if (enable) {
    // Disable possible FEC.
    SetFECStatus(false, 0, 0);
  }
  // Update the decoding VCM.
  if (vcm_.SetVideoProtection(kProtectionNack, enable) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                 enable);
    return -1;
  }
  return ProcessNACKRequest(enable);
}

int32_t ViEChannel::ProcessNACKRequest(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);

  if (enable) {
    // Turn on NACK.
    NACKMethod nackMethod = kNackRtcp;
    if (rtp_rtcp_->RTCP() == kRtcpOff) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not enable NACK, RTPC not on ", __FUNCTION__);
      return -1;
    }
    if (rtp_rtcp_->SetNACKStatus(nackMethod,
                                 max_nack_reordering_threshold_) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not set NACK method %d", __FUNCTION__,
                   nackMethod);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Using NACK method %d", __FUNCTION__, nackMethod);
    rtp_rtcp_->SetStorePacketsStatus(true, nack_history_size_sender_);

    vcm_.RegisterPacketRequestCallback(this);

    CriticalSectionScoped cs(rtp_rtcp_cs_.get());

    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      RtpRtcp* rtp_rtcp = *it;
      rtp_rtcp->SetNACKStatus(nackMethod, max_nack_reordering_threshold_);
      rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
    }
  } else {
    CriticalSectionScoped cs(rtp_rtcp_cs_.get());
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end();
         it++) {
      RtpRtcp* rtp_rtcp = *it;
      if (paced_sender_ == NULL) {
        rtp_rtcp->SetStorePacketsStatus(false, 0);
      }
      rtp_rtcp->SetNACKStatus(kNackOff, max_nack_reordering_threshold_);
    }
    vcm_.RegisterPacketRequestCallback(NULL);
    if (paced_sender_ == NULL) {
      rtp_rtcp_->SetStorePacketsStatus(false, 0);
    }
    if (rtp_rtcp_->SetNACKStatus(kNackOff,
                                 max_nack_reordering_threshold_) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: Could not turn off NACK", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

int32_t ViEChannel::SetFECStatus(const bool enable,
                                       const unsigned char payload_typeRED,
                                       const unsigned char payload_typeFEC) {
  // Disable possible NACK.
  if (enable) {
    SetNACKStatus(false);
  }

  return ProcessFECRequest(enable, payload_typeRED, payload_typeFEC);
}

int32_t ViEChannel::ProcessFECRequest(
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d, payload_typeRED: %u, payload_typeFEC: %u)",
               __FUNCTION__, enable, payload_typeRED, payload_typeFEC);

  if (rtp_rtcp_->SetGenericFECStatus(enable, payload_typeRED,
                                    payload_typeFEC) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not change FEC status to %d", __FUNCTION__,
                 enable);
    return -1;
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetGenericFECStatus(enable, payload_typeRED, payload_typeFEC);
  }
  return 0;
}

int32_t ViEChannel::SetHybridNACKFECStatus(
    const bool enable,
    const unsigned char payload_typeRED,
    const unsigned char payload_typeFEC) {
  // Update the decoding VCM with hybrid mode.
  if (vcm_.SetVideoProtection(kProtectionNackFEC, enable) != VCM_OK) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set VCM NACK protection: %d", __FUNCTION__,
                 enable);
    return -1;
  }

  int32_t ret_val = 0;
  ret_val = ProcessNACKRequest(enable);
  if (ret_val < 0) {
    return ret_val;
  }
  return ProcessFECRequest(enable, payload_typeRED, payload_typeFEC);
}

int ViEChannel::SetSenderBufferingMode(int target_delay_ms) {
  if ((target_delay_ms < 0) || (target_delay_ms > kMaxTargetDelayMs)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Target sender buffering delay out of bounds: %d",
                 __FUNCTION__, target_delay_ms);
    return -1;
  }
  if (target_delay_ms == 0) {
    // Real-time mode.
    nack_history_size_sender_ = kSendSidePacketHistorySize;
  } else {
    nack_history_size_sender_ = GetRequiredNackListSize(target_delay_ms);
    // Don't allow a number lower than the default value.
    if (nack_history_size_sender_ < kSendSidePacketHistorySize) {
      nack_history_size_sender_ = kSendSidePacketHistorySize;
    }
  }
  // Setting nack_history_size_.
  // First disabling (forcing free) and then resetting to desired value.
  if (rtp_rtcp_->SetStorePacketsStatus(false, 0) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s:SetStorePacketsStatus failure", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_->SetStorePacketsStatus(true, nack_history_size_sender_) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s:SetStorePacketsStatus failure", __FUNCTION__);
    return -1;
  }
  return 0;
}

int ViEChannel::SetReceiverBufferingMode(int target_delay_ms) {
  if ((target_delay_ms < 0) || (target_delay_ms > kMaxTargetDelayMs)) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Target receiver buffering delay out of bounds: %d",
                 __FUNCTION__, target_delay_ms);
    return -1;
  }
  int max_nack_list_size;
  if (target_delay_ms == 0) {
    // Real-time mode - restore default settings.
    max_nack_reordering_threshold_ = kMaxPacketAgeToNack;
    max_nack_list_size = kMaxNackListSize;
  } else {
    max_nack_list_size =  3 * GetRequiredNackListSize(target_delay_ms) / 4;
    max_nack_reordering_threshold_ = max_nack_list_size;
  }
  vcm_.SetNackSettings(max_nack_list_size, max_nack_reordering_threshold_);
  vcm_.SetMinReceiverDelay(target_delay_ms);
  if (vie_sync_.SetTargetBufferingDelay(target_delay_ms) < 0)
    return -1;
  return 0;
}

int ViEChannel::GetRequiredNackListSize(int target_delay_ms) {
  // The max size of the nack list should be large enough to accommodate the
  // the number of packets (frames) resulting from the increased delay.
  // Roughly estimating for ~40 packets per frame @ 30fps.
  return target_delay_ms * 40 * 30 / 1000;
}

int32_t ViEChannel::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, method);
  return rtp_rtcp_->SetKeyFrameRequestMethod(method);
}

bool ViEChannel::EnableRemb(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "ViEChannel::EnableRemb: %d", enable);
  if (rtp_rtcp_->SetREMBStatus(enable) != 0)
    return false;
  return true;
}

int ViEChannel::SetSendTimestampOffsetStatus(bool enable, int id) {
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  int error = 0;
  if (enable) {
    // Enable the extension, but disable possible old id to avoid errors.
    send_timestamp_extension_id_ = id;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset);
    error = rtp_rtcp_->RegisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset, id);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset);
      error |= (*it)->RegisterSendRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset, id);
    }
  } else {
    // Disable the extension.
    send_timestamp_extension_id_ = kInvalidRtpExtensionId;
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset);
    for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
         it != simulcast_rtp_rtcp_.end(); it++) {
      (*it)->DeregisterSendRtpHeaderExtension(
          kRtpExtensionTransmissionTimeOffset);
    }
  }
  return error;
}

int ViEChannel::SetReceiveTimestampOffsetStatus(bool enable, int id) {
  if (enable) {
    return rtp_rtcp_->RegisterReceiveRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset, id);
  } else {
    return rtp_rtcp_->DeregisterReceiveRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset);
  }
}

void ViEChannel::SetTransmissionSmoothingStatus(bool enable) {
  assert(paced_sender_ && "No paced sender registered.");
  paced_sender_->SetStatus(enable);
}

int32_t ViEChannel::EnableTMMBR(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, enable);
  return rtp_rtcp_->SetTMMBRStatus(enable);
}

int32_t ViEChannel::EnableKeyFrameRequestCallback(const bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %d", __FUNCTION__, enable);

  CriticalSectionScoped cs(callback_cs_.get());
  if (enable && !codec_observer_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: No ViECodecObserver set", __FUNCTION__, enable);
    return -1;
  }
  do_key_frame_callbackRequest_ = enable;
  return 0;
}

int32_t ViEChannel::SetSSRC(const uint32_t SSRC,
                                  const StreamType usage,
                                  const uint8_t simulcast_idx) {
  WEBRTC_TRACE(webrtc::kTraceInfo,
               webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(usage:%d, SSRC: 0x%x, idx:%u)",
               __FUNCTION__, usage, SSRC, simulcast_idx);
  if (simulcast_idx == 0) {
    return rtp_rtcp_->SetSSRC(SSRC);
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  if (simulcast_idx > simulcast_rtp_rtcp_.size()) {
      return -1;
  }
  std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
  for (int i = 1; i < simulcast_idx; ++i, ++it) {
    if (it ==  simulcast_rtp_rtcp_.end()) {
      return -1;
    }
  }
  RtpRtcp* rtp_rtcp = *it;
  if (usage == kViEStreamTypeRtx) {
    return rtp_rtcp->SetRTXSendStatus(kRtxRetransmitted, true, SSRC);
  }
  return rtp_rtcp->SetSSRC(SSRC);
}

int32_t ViEChannel::SetRemoteSSRCType(const StreamType usage,
                                            const uint32_t SSRC) const {
  WEBRTC_TRACE(webrtc::kTraceInfo,
               webrtc::kTraceVideo,
               ViEId(engine_id_, channel_id_),
               "%s(usage:%d, SSRC: 0x%x)",
               __FUNCTION__, usage, SSRC);

  return rtp_rtcp_->SetRTXReceiveStatus(true, SSRC);
}

// TODO(mflodman) Add kViEStreamTypeRtx.
int32_t ViEChannel::GetLocalSSRC(uint8_t idx, unsigned int* ssrc) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (idx == 0) {
    *ssrc = rtp_rtcp_->SSRC();
    return 0;
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  if (idx > simulcast_rtp_rtcp_.size()) {
    return -1;
  }
  std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
  for (int i = 1; i < idx; ++i, ++it) {
    if (it ==  simulcast_rtp_rtcp_.end()) {
      return -1;
    }
  }
  *ssrc = (*it)->SSRC();
  return 0;
}

int32_t ViEChannel::GetRemoteSSRC(uint32_t* ssrc) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  *ssrc = rtp_rtcp_->RemoteSSRC();
  return 0;
}

int32_t ViEChannel::GetRemoteCSRC(uint32_t CSRCs[kRtpCsrcSize]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  uint32_t arrayCSRC[kRtpCsrcSize];
  memset(arrayCSRC, 0, sizeof(arrayCSRC));

  int num_csrcs = rtp_rtcp_->RemoteCSRCs(arrayCSRC);
  if (num_csrcs > 0) {
    memcpy(CSRCs, arrayCSRC, num_csrcs * sizeof(uint32_t));
    for (int idx = 0; idx < num_csrcs; idx++) {
      WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "\tCSRC[%d] = %lu", idx, CSRCs[idx]);
    }
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: CSRC list is empty", __FUNCTION__);
  }
  return 0;
}

int ViEChannel::SetRtxSendPayloadType(int payload_type) {
  if (rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: already sending", __FUNCTION__);
    return -1;
  }
  rtp_rtcp_->SetRtxSendPayloadType(payload_type);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); it++) {
    (*it)->SetRtxSendPayloadType(payload_type);
  }
  return 0;
}

void ViEChannel::SetRtxReceivePayloadType(int payload_type) {
  rtp_rtcp_->SetRtxReceivePayloadType(payload_type);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); it++) {
    (*it)->SetRtxReceivePayloadType(payload_type);
  }
}

int32_t ViEChannel::SetStartSequenceNumber(uint16_t sequence_number) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: already sending", __FUNCTION__);
    return -1;
  }
  return rtp_rtcp_->SetSequenceNumber(sequence_number);
}

int32_t ViEChannel::SetRTCPCName(const char rtcp_cname[]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  if (rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: already sending", __FUNCTION__);
    return -1;
  }
  return rtp_rtcp_->SetCNAME(rtcp_cname);
}

int32_t ViEChannel::GetRTCPCName(char rtcp_cname[]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  return rtp_rtcp_->CNAME(rtcp_cname);
}

int32_t ViEChannel::GetRemoteRTCPCName(char rtcp_cname[]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  uint32_t remoteSSRC = rtp_rtcp_->RemoteSSRC();
  return rtp_rtcp_->RemoteCNAME(remoteSSRC, rtcp_cname);
}

int32_t ViEChannel::RegisterRtpObserver(ViERTPObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    if (rtp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: observer alread added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    rtp_observer_ = observer;
  } else {
    if (!rtp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    rtp_observer_ = NULL;
  }
  return 0;
}

int32_t ViEChannel::RegisterRtcpObserver(ViERTCPObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    if (rtcp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: observer alread added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    rtcp_observer_ = observer;
  } else {
    if (!rtcp_observer_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    rtcp_observer_ = NULL;
  }
  return 0;
}

int32_t ViEChannel::SendApplicationDefinedRTCPPacket(
    const uint8_t sub_type,
    uint32_t name,
    const uint8_t* data,
    uint16_t data_length_in_bytes) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (!rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: not sending", __FUNCTION__);
    return -1;
  }
  if (!data) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: no input argument", __FUNCTION__);
    return -1;
  }
  if (data_length_in_bytes % 4 != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: input length error", __FUNCTION__);
    return -1;
  }
  RTCPMethod rtcp_method = rtp_rtcp_->RTCP();
  if (rtcp_method == kRtcpOff) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: RTCP not enabled", __FUNCTION__);
    return -1;
  }
  // Create and send packet.
  if (rtp_rtcp_->SetRTCPApplicationSpecificData(sub_type, name, data,
                                               data_length_in_bytes) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not send RTCP application data", __FUNCTION__);
    return -1;
  }
  return 0;
}

int32_t ViEChannel::GetSendRtcpStatistics(uint16_t* fraction_lost,
                                                uint32_t* cumulative_lost,
                                                uint32_t* extended_max,
                                                uint32_t* jitter_samples,
                                                int32_t* rtt_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  // TODO(pwestin) how do we do this for simulcast ? average for all
  // except cumulative_lost that is the sum ?
  // CriticalSectionScoped cs(rtp_rtcp_cs_.get());

  // for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
  //      it != simulcast_rtp_rtcp_.end();
  //      it++) {
  //   RtpRtcp* rtp_rtcp = *it;
  // }
  uint32_t remote_ssrc = rtp_rtcp_->RemoteSSRC();

  // Get all RTCP receiver report blocks that have been received on this
  // channel. If we receive RTP packets from a remote source we know the
  // remote SSRC and use the report block from him.
  // Otherwise use the first report block.
  std::vector<RTCPReportBlock> remote_stats;
  if (rtp_rtcp_->RemoteRTCPStat(&remote_stats) != 0 || remote_stats.empty()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get remote stats", __FUNCTION__);
    return -1;
  }
  std::vector<RTCPReportBlock>::const_iterator statistics =
      remote_stats.begin();
  for (; statistics != remote_stats.end(); ++statistics) {
    if (statistics->remoteSSRC == remote_ssrc)
      break;
  }

  if (statistics == remote_stats.end()) {
    // If we have not received any RTCP packets from this SSRC it probably means
    // we have not received any RTP packets.
    // Use the first received report block instead.
    statistics = remote_stats.begin();
    remote_ssrc = statistics->remoteSSRC;
  }

  *fraction_lost = statistics->fractionLost;
  *cumulative_lost = statistics->cumulativeLost;
  *extended_max = statistics->extendedHighSeqNum;
  *jitter_samples = statistics->jitter;

  uint16_t dummy;
  uint16_t rtt = 0;
  if (rtp_rtcp_->RTT(remote_ssrc, &rtt, &dummy, &dummy, &dummy) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get RTT", __FUNCTION__);
    return -1;
  }
  *rtt_ms = rtt;
  return 0;
}

int32_t ViEChannel::GetReceivedRtcpStatistics(uint16_t* fraction_lost,
                                                    uint32_t* cumulative_lost,
                                                    uint32_t* extended_max,
                                                    uint32_t* jitter_samples,
                                                    int32_t* rtt_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  uint8_t frac_lost = 0;
  if (rtp_rtcp_->StatisticsRTP(&frac_lost, cumulative_lost, extended_max,
                              jitter_samples) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get received RTP statistics", __FUNCTION__);
    return -1;
  }
  *fraction_lost = frac_lost;

  uint32_t remote_ssrc = rtp_rtcp_->RemoteSSRC();
  uint16_t dummy = 0;
  uint16_t rtt = 0;
  if (rtp_rtcp_->RTT(remote_ssrc, &rtt, &dummy, &dummy, &dummy) != 0) {
    WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get RTT", __FUNCTION__);
  }
  *rtt_ms = rtt;
  return 0;
}

int32_t ViEChannel::GetRtpStatistics(uint32_t* bytes_sent,
                                           uint32_t* packets_sent,
                                           uint32_t* bytes_received,
                                           uint32_t* packets_received) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (rtp_rtcp_->DataCountersRTP(bytes_sent,
                                 packets_sent,
                                 bytes_received,
                                 packets_received) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not get counters", __FUNCTION__);
    return -1;
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    uint32_t bytes_sent_temp = 0;
    uint32_t packets_sent_temp = 0;
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->DataCountersRTP(&bytes_sent_temp, &packets_sent_temp, NULL, NULL);
    bytes_sent += bytes_sent_temp;
    packets_sent += packets_sent_temp;
  }
  return 0;
}

void ViEChannel::GetBandwidthUsage(uint32_t* total_bitrate_sent,
                                   uint32_t* video_bitrate_sent,
                                   uint32_t* fec_bitrate_sent,
                                   uint32_t* nackBitrateSent) const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  rtp_rtcp_->BitrateSent(total_bitrate_sent, video_bitrate_sent,
                         fec_bitrate_sent, nackBitrateSent);
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end(); it++) {
    uint32_t stream_rate = 0;
    uint32_t video_rate = 0;
    uint32_t fec_rate = 0;
    uint32_t nackRate = 0;
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->BitrateSent(&stream_rate, &video_rate, &fec_rate, &nackRate);
    *total_bitrate_sent += stream_rate;
    *fec_bitrate_sent += fec_rate;
    *nackBitrateSent += nackRate;
  }
}

void ViEChannel::GetEstimatedReceiveBandwidth(
    uint32_t* estimated_bandwidth) const {
  vie_receiver_.EstimatedReceiveBandwidth(estimated_bandwidth);
}

int32_t ViEChannel::StartRTPDump(const char file_nameUTF8[1024],
                                       RTPDirections direction) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (direction != kRtpIncoming && direction != kRtpOutgoing) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: invalid input", __FUNCTION__);
    return -1;
  }

  if (direction == kRtpIncoming) {
    return vie_receiver_.StartRTPDump(file_nameUTF8);
  } else {
    return vie_sender_.StartRTPDump(file_nameUTF8);
  }
}

int32_t ViEChannel::StopRTPDump(RTPDirections direction) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (direction != kRtpIncoming && direction != kRtpOutgoing) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: invalid input", __FUNCTION__);
    return -1;
  }

  if (direction == kRtpIncoming) {
    return vie_receiver_.StopRTPDump();
  } else {
    return vie_sender_.StopRTPDump();
  }
}

int32_t ViEChannel::StartSend() {
  CriticalSectionScoped cs(callback_cs_.get());
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);

  if (!external_transport_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: send sockets not initialized", __FUNCTION__);
    return -1;
  }
  rtp_rtcp_->SetSendingMediaStatus(true);

  if (rtp_rtcp_->Sending()) {
    // Already sending.
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Already sending", __FUNCTION__);
    return kViEBaseAlreadySending;
  }
  if (rtp_rtcp_->SetSendingStatus(true) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not start sending RTP", __FUNCTION__);
    return -1;
  }
  CriticalSectionScoped cs_rtp(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::const_iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingMediaStatus(true);
    rtp_rtcp->SetSendingStatus(true);
  }
  vie_receiver_.StartRTCPReceive();
  return 0;
}

int32_t ViEChannel::StopSend() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  rtp_rtcp_->SetSendingMediaStatus(false);
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetSendingMediaStatus(false);
  }
  if (!rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Not sending", __FUNCTION__);
    return kViEBaseNotSending;
  }

  // Reset.
  rtp_rtcp_->ResetSendDataCountersRTP();
  if (rtp_rtcp_->SetSendingStatus(false) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not stop RTP sending", __FUNCTION__);
    return -1;
  }
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->ResetSendDataCountersRTP();
    rtp_rtcp->SetSendingStatus(false);
  }
  vie_receiver_.StopRTCPReceive();
  return 0;
}

bool ViEChannel::Sending() {
  return rtp_rtcp_->Sending();
}

int32_t ViEChannel::StartReceive() {
  CriticalSectionScoped cs(callback_cs_.get());
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (StartDecodeThread() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not start decoder thread", __FUNCTION__);

    vie_receiver_.StopReceive();
    return -1;
  }
  vie_receiver_.StartReceive();
  return 0;
}

int32_t ViEChannel::StopReceive() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  vie_receiver_.StopReceive();
  StopDecodeThread();
  vcm_.ResetDecoder();
  return 0;
}

int32_t ViEChannel::RegisterSendTransport(Transport* transport) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Sending", __FUNCTION__);
    return -1;
  }

  CriticalSectionScoped cs(callback_cs_.get());
  if (external_transport_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: transport already registered", __FUNCTION__);
    return -1;
  }
  external_transport_ = transport;
  vie_sender_.RegisterSendTransport(transport);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: Transport registered: 0x%p", __FUNCTION__,
               &external_transport_);

  return 0;
}

int32_t ViEChannel::DeregisterSendTransport() {
  CriticalSectionScoped cs(callback_cs_.get());
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  if (!external_transport_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: no transport registered", __FUNCTION__);
    return -1;
  }
  if (rtp_rtcp_->Sending()) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Sending", __FUNCTION__);
    return -1;
  }
  external_transport_ = NULL;
  vie_sender_.DeregisterSendTransport();
  return 0;
}

int32_t ViEChannel::ReceivedRTPPacket(
    const void* rtp_packet, const int32_t rtp_packet_length) {
  {
    CriticalSectionScoped cs(callback_cs_.get());
    if (!external_transport_) {
      return -1;
    }
  }
  return vie_receiver_.ReceivedRTPPacket(rtp_packet, rtp_packet_length);
}

int32_t ViEChannel::ReceivedRTCPPacket(
  const void* rtcp_packet, const int32_t rtcp_packet_length) {
  {
    CriticalSectionScoped cs(callback_cs_.get());
    if (!external_transport_) {
      return -1;
    }
  }
  return vie_receiver_.ReceivedRTCPPacket(rtcp_packet, rtcp_packet_length);
}

int32_t ViEChannel::SetMTU(uint16_t mtu) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (rtp_rtcp_->SetMaxTransferUnit(mtu) != 0) {
    // Logging done.
    return -1;
  }
  CriticalSectionScoped cs(rtp_rtcp_cs_.get());
  for (std::list<RtpRtcp*>::iterator it = simulcast_rtp_rtcp_.begin();
       it != simulcast_rtp_rtcp_.end();
       it++) {
    RtpRtcp* rtp_rtcp = *it;
    rtp_rtcp->SetMaxTransferUnit(mtu);
  }
  mtu_ = mtu;
  return 0;
}

uint16_t ViEChannel::MaxDataPayloadLength() const {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  return rtp_rtcp_->MaxDataPayloadLength();
}

int32_t ViEChannel::SetPacketTimeoutNotification(
    bool enable, uint32_t timeout_seconds) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (enable) {
    uint32_t timeout_ms = 1000 * timeout_seconds;
    if (rtp_rtcp_->SetPacketTimeout(timeout_ms, 0) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s", __FUNCTION__);
      return -1;
    }
  } else {
    if (rtp_rtcp_->SetPacketTimeout(0, 0) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s", __FUNCTION__);
      return -1;
    }
  }
  return 0;
}

int32_t ViEChannel::RegisterNetworkObserver(
    ViENetworkObserver* observer) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (observer) {
    if (networkObserver_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: observer alread added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer added", __FUNCTION__);
    networkObserver_ = observer;
  } else {
    if (!networkObserver_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no observer added", __FUNCTION__);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: observer removed", __FUNCTION__);
    networkObserver_ = NULL;
  }
  return 0;
}

bool ViEChannel::NetworkObserverRegistered() {
  CriticalSectionScoped cs(callback_cs_.get());
  return networkObserver_ != NULL;
}

int32_t ViEChannel::SetPeriodicDeadOrAliveStatus(
  const bool enable, const uint32_t sample_time_seconds) {
  WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callback_cs_.get());
  if (!networkObserver_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: no observer added", __FUNCTION__);
    return -1;
  }

  bool enabled = false;
  uint8_t current_sampletime_seconds = 0;

  // Get old settings.
  rtp_rtcp_->PeriodicDeadOrAliveStatus(enabled, current_sampletime_seconds);
  // Set new settings.
  if (rtp_rtcp_->SetPeriodicDeadOrAliveStatus(
        enable, static_cast<uint8_t>(sample_time_seconds)) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: Could not set periodic dead-or-alive status",
                 __FUNCTION__);
    return -1;
  }
  if (!enable) {
    // Restore last utilized sample time.
    // Without this trick, the sample time would always be reset to default
    // (2 sec), each time dead-or-alive was disabled without sample-time
    // parameter.
    rtp_rtcp_->SetPeriodicDeadOrAliveStatus(enable, current_sampletime_seconds);
  }
  return 0;
}

int32_t ViEChannel::EnableColorEnhancement(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(enable: %d)", __FUNCTION__, enable);

  CriticalSectionScoped cs(callback_cs_.get());
  color_enhancement_ = enable;
  return 0;
}

RtpRtcp* ViEChannel::rtp_rtcp() {
  return rtp_rtcp_.get();
}

CallStatsObserver* ViEChannel::GetStatsObserver() {
  return stats_observer_.get();
}

int32_t ViEChannel::FrameToRender(
    I420VideoFrame& video_frame) {  // NOLINT
  CriticalSectionScoped cs(callback_cs_.get());

  if (decoder_reset_) {
    // Trigger a callback to the user if the incoming codec has changed.
    if (codec_observer_) {
      VideoCodec decoder;
      memset(&decoder, 0, sizeof(decoder));
      if (vcm_.ReceiveCodec(&decoder) == VCM_OK) {
        // VCM::ReceiveCodec returns the codec set by
        // RegisterReceiveCodec, which might not be the size we're
        // actually decoding.
        decoder.width = static_cast<uint16_t>(video_frame.width());
        decoder.height = static_cast<uint16_t>(video_frame.height());
        codec_observer_->IncomingCodecChanged(channel_id_, decoder);
      } else {
        assert(false);
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                     "%s: Could not get receive codec", __FUNCTION__);
      }
    }
    decoder_reset_ = false;
  }
  if (effect_filter_) {
    unsigned int length = CalcBufferSize(kI420,
                                         video_frame.width(),
                                         video_frame.height());
    scoped_array<uint8_t> video_buffer(new uint8_t[length]);
    ExtractBuffer(video_frame, length, video_buffer.get());
    effect_filter_->Transform(length, video_buffer.get(),
                              video_frame.timestamp(), video_frame.width(),
                              video_frame.height());
  }
  if (color_enhancement_) {
    VideoProcessingModule::ColorEnhancement(&video_frame);
  }

  // Record videoframe.
  file_recorder_.RecordVideoFrame(video_frame);

  uint32_t arr_ofCSRC[kRtpCsrcSize];
  int32_t no_of_csrcs = rtp_rtcp_->RemoteCSRCs(arr_ofCSRC);
  if (no_of_csrcs <= 0) {
    arr_ofCSRC[0] = rtp_rtcp_->RemoteSSRC();
    no_of_csrcs = 1;
  }
  WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(timestamp:%u)", __FUNCTION__, video_frame.timestamp());
  DeliverFrame(&video_frame, no_of_csrcs, arr_ofCSRC);
  return 0;
}

int32_t ViEChannel::ReceivedDecodedReferenceFrame(
  const uint64_t picture_id) {
  return rtp_rtcp_->SendRTCPReferencePictureSelection(picture_id);
}

int32_t ViEChannel::StoreReceivedFrame(
  const EncodedVideoData& frame_to_store) {
  return 0;
}

int32_t ViEChannel::ReceiveStatistics(const uint32_t bit_rate,
                                            const uint32_t frame_rate) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (codec_observer_) {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: bitrate %u, framerate %u", __FUNCTION__, bit_rate,
                 frame_rate);
    codec_observer_->IncomingRate(channel_id_, frame_rate, bit_rate);
  }
  return 0;
}

int32_t ViEChannel::RequestKeyFrame() {
  WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", __FUNCTION__);
  {
    CriticalSectionScoped cs(callback_cs_.get());
    if (codec_observer_ && do_key_frame_callbackRequest_) {
      codec_observer_->RequestNewKeyFrame(channel_id_);
    }
  }
  return rtp_rtcp_->RequestKeyFrame();
}

int32_t ViEChannel::SliceLossIndicationRequest(
  const uint64_t picture_id) {
  return rtp_rtcp_->SendRTCPSliceLossIndication((uint8_t) picture_id);
}

int32_t ViEChannel::ResendPackets(const uint16_t* sequence_numbers,
                                        uint16_t length) {
  WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(length: %d)", __FUNCTION__, length);
  return rtp_rtcp_->SendNACK(sequence_numbers, length);
}

bool ViEChannel::ChannelDecodeThreadFunction(void* obj) {
  return static_cast<ViEChannel*>(obj)->ChannelDecodeProcess();
}

bool ViEChannel::ChannelDecodeProcess() {
  vcm_.Decode(kMaxDecodeWaitTimeMs);
  return true;
}

void ViEChannel::OnRttUpdate(uint32_t rtt) {
  vcm_.SetReceiveChannelParameters(rtt);
  if (!sender_)
    rtp_rtcp_->SetRtt(rtt);
}

int32_t ViEChannel::StartDecodeThread() {
  // Start the decode thread
  if (decode_thread_) {
    // Already started.
    return 0;
  }
  decode_thread_ = ThreadWrapper::CreateThread(ChannelDecodeThreadFunction,
                                                   this, kHighestPriority,
                                                   "DecodingThread");
  if (!decode_thread_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not create decode thread", __FUNCTION__);
    return -1;
  }

  unsigned int thread_id;
  if (decode_thread_->Start(thread_id) == false) {
    delete decode_thread_;
    decode_thread_ = NULL;
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not start decode thread", __FUNCTION__);
    return -1;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: decode thread with id %u started", __FUNCTION__);
  return 0;
}

int32_t ViEChannel::StopDecodeThread() {
  if (!decode_thread_) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: decode thread not running", __FUNCTION__);
    return 0;
  }

  decode_thread_->SetNotAlive();
  if (decode_thread_->Stop()) {
    delete decode_thread_;
  } else {
    // Couldn't stop the thread, leak instead of crash.
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: could not stop decode thread", __FUNCTION__);
    assert(false && "could not stop decode thread");
  }
  decode_thread_ = NULL;
  return 0;
}

int32_t ViEChannel::RegisterExternalEncryption(Encryption* encryption) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callback_cs_.get());
  if (external_encryption_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external encryption already registered", __FUNCTION__);
    return -1;
  }

  external_encryption_ = encryption;

  vie_receiver_.RegisterExternalDecryption(encryption);
  vie_sender_.RegisterExternalEncryption(encryption);

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s", "external encryption object registerd with channel=%d",
               channel_id_);
  return 0;
}

int32_t ViEChannel::DeRegisterExternalEncryption() {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callback_cs_.get());
  if (!external_encryption_) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: external encryption is not registered", __FUNCTION__);
    return -1;
  }

  external_transport_ = NULL;
  vie_receiver_.DeregisterExternalDecryption();
  vie_sender_.DeregisterExternalEncryption();
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s external encryption object de-registerd with channel=%d",
               __FUNCTION__, channel_id_);
  return 0;
}

int32_t ViEChannel::SetVoiceChannel(int32_t ve_channel_id,
                                          VoEVideoSync* ve_sync_interface) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s, audio channel %d, video channel %d", __FUNCTION__,
               ve_channel_id, channel_id_);

  if (ve_sync_interface) {
    // Register lip sync
    module_process_thread_.RegisterModule(&vie_sync_);
  } else {
    module_process_thread_.DeRegisterModule(&vie_sync_);
  }
  return vie_sync_.ConfigureSync(ve_channel_id, ve_sync_interface,
                                 rtp_rtcp_.get());
}

int32_t ViEChannel::VoiceChannel() {
  return vie_sync_.VoiceChannel();
}

int32_t ViEChannel::RegisterEffectFilter(ViEEffectFilter* effect_filter) {
  CriticalSectionScoped cs(callback_cs_.get());
  if (!effect_filter) {
    if (!effect_filter_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: no effect filter added for channel %d",
                   __FUNCTION__, channel_id_);
      return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: deregister effect filter for device %d", __FUNCTION__,
                 channel_id_);
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s: register effect filter for device %d", __FUNCTION__,
                 channel_id_);
    if (effect_filter_) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, channel_id_),
                   "%s: effect filter already added for channel %d",
                   __FUNCTION__, channel_id_);
      return -1;
    }
  }
  effect_filter_ = effect_filter;
  return 0;
}

ViEFileRecorder& ViEChannel::GetIncomingFileRecorder() {
  // Start getting callback of all frames before they are decoded.
  vcm_.RegisterFrameStorageCallback(this);
  return file_recorder_;
}

void ViEChannel::ReleaseIncomingFileRecorder() {
  // Stop getting callback of all frames before they are decoded.
  vcm_.RegisterFrameStorageCallback(NULL);
}

void ViEChannel::OnApplicationDataReceived(const int32_t id,
                                           const uint8_t sub_type,
                                           const uint32_t name,
                                           const uint16_t length,
                                           const uint8_t* data) {
  if (channel_id_ != ChannelId(id)) {
    WEBRTC_TRACE(kTraceStream, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }
  CriticalSectionScoped cs(callback_cs_.get());
  {
    if (rtcp_observer_) {
      rtcp_observer_->OnApplicationDataReceived(
          channel_id_, sub_type, name, reinterpret_cast<const char*>(data),
          length);
    }
  }
}

void ViEChannel::OnSendReportReceived(const int32_t id,
                                      const uint32_t senderSSRC,
                                      uint32_t ntp_secs,
                                      uint32_t ntp_frac,
                                      uint32_t timestamp) {
  vie_receiver_.OnSendReportReceived(id, senderSSRC, ntp_secs, ntp_frac,
                                     timestamp);
}

int32_t ViEChannel::OnInitializeDecoder(
    const int32_t id,
    const int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const uint8_t channels,
    const uint32_t rate) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: payload_type %d, payload_name %s", __FUNCTION__,
               payload_type, payload_name);
  vcm_.ResetDecoder();

  callback_cs_->Enter();
  decoder_reset_ = true;
  callback_cs_->Leave();
  return 0;
}

void ViEChannel::OnPacketTimeout(const int32_t id) {
  assert(ChannelId(id) == channel_id_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);

  CriticalSectionScoped cs(callback_cs_.get());
  if (networkObserver_) {
    networkObserver_->PacketTimeout(channel_id_, NoPacket);
    rtp_packet_timeout_ = true;
  }
}

void ViEChannel::OnReceivedPacket(const int32_t id,
                                  const RtpRtcpPacketType packet_type) {
  assert(ChannelId(id) == channel_id_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_), "%s",
               __FUNCTION__);
  if (rtp_packet_timeout_ && packet_type == kPacketRtp) {
    CriticalSectionScoped cs(callback_cs_.get());
    if (networkObserver_) {
      networkObserver_->PacketTimeout(channel_id_, PacketReceived);
    }

    // Reset even if no observer set, might have been removed during timeout.
    rtp_packet_timeout_ = false;
  }
}

void ViEChannel::OnPeriodicDeadOrAlive(const int32_t id,
                                       const RTPAliveType alive) {
  assert(ChannelId(id) == channel_id_);
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s(id=%d, alive=%d)", __FUNCTION__, id, alive);

  CriticalSectionScoped cs(callback_cs_.get());
  if (!networkObserver_) {
    return;
  }
  bool is_alive = true;
  if (alive == kRtpDead) {
    is_alive = false;
  }
  networkObserver_->OnPeriodicDeadOrAlive(channel_id_, is_alive);
  return;
}

void ViEChannel::OnIncomingSSRCChanged(const int32_t id,
                                       const uint32_t SSRC) {
  if (channel_id_ != ChannelId(id)) {
    assert(false);
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %u", __FUNCTION__, SSRC);

  CriticalSectionScoped cs(callback_cs_.get());
  {
    if (rtp_observer_) {
      rtp_observer_->IncomingSSRCChanged(channel_id_, SSRC);
    }
  }
}

void ViEChannel::OnIncomingCSRCChanged(const int32_t id,
                                       const uint32_t CSRC,
                                       const bool added) {
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %u added: %d", __FUNCTION__, CSRC, added);

  if (channel_id_ != ChannelId(id)) {
    assert(false);
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
                 "%s, incorrect id", __FUNCTION__, id);
    return;
  }

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, channel_id_),
               "%s: %u", __FUNCTION__, CSRC);

  CriticalSectionScoped cs(callback_cs_.get());
  {
    if (rtp_observer_) {
      rtp_observer_->IncomingCSRCChanged(channel_id_, CSRC, added);
    }
  }
}

}  // namespace webrtc
