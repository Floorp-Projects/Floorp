/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"

#include <assert.h>
#include <string.h>

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef MATLAB
#include "webrtc/modules/rtp_rtcp/test/BWEStandAlone/MatlabPlot.h"
extern MatlabEngine eng;  // Global variable defined elsewhere.
#endif

// Local for this file.
namespace {

const float kFracMs = 4.294967296E6f;

}  // namespace

#ifdef _WIN32
// Disable warning C4355: 'this' : used in base member initializer list.
#pragma warning(disable : 4355)
#endif

namespace webrtc {

RtpRtcp::Configuration::Configuration()
    : id(-1),
      audio(false),
      clock(NULL),
      default_module(NULL),
      receive_statistics(NullObjectReceiveStatistics()),
      outgoing_transport(NULL),
      rtcp_feedback(NULL),
      intra_frame_callback(NULL),
      bandwidth_callback(NULL),
      rtt_observer(NULL),
      audio_messages(NullObjectRtpAudioFeedback()),
      remote_bitrate_estimator(NULL),
      paced_sender(NULL) {
}

RtpRtcp* RtpRtcp::CreateRtpRtcp(const RtpRtcp::Configuration& configuration) {
  if (configuration.clock) {
    return new ModuleRtpRtcpImpl(configuration);
  } else {
    RtpRtcp::Configuration configuration_copy;
    memcpy(&configuration_copy, &configuration,
           sizeof(RtpRtcp::Configuration));
    configuration_copy.clock = Clock::GetRealTimeClock();
    ModuleRtpRtcpImpl* rtp_rtcp_instance =
        new ModuleRtpRtcpImpl(configuration_copy);
    return rtp_rtcp_instance;
  }
}

ModuleRtpRtcpImpl::ModuleRtpRtcpImpl(const Configuration& configuration)
    : rtp_sender_(configuration.id,
                  configuration.audio,
                  configuration.clock,
                  configuration.outgoing_transport,
                  configuration.audio_messages,
                  configuration.paced_sender),
      rtcp_sender_(configuration.id, configuration.audio, configuration.clock,
                   configuration.receive_statistics),
      rtcp_receiver_(configuration.id, configuration.clock, this),
      clock_(configuration.clock),
      id_(configuration.id),
      audio_(configuration.audio),
      collision_detected_(false),
      last_process_time_(configuration.clock->TimeInMilliseconds()),
      last_bitrate_process_time_(configuration.clock->TimeInMilliseconds()),
      last_rtt_process_time_(configuration.clock->TimeInMilliseconds()),
      packet_overhead_(28),  // IPV4 UDP.
      critical_section_module_ptrs_(
          CriticalSectionWrapper::CreateCriticalSection()),
      critical_section_module_ptrs_feedback_(
          CriticalSectionWrapper::CreateCriticalSection()),
      default_module_(
          static_cast<ModuleRtpRtcpImpl*>(configuration.default_module)),
      nack_method_(kNackOff),
      nack_last_time_sent_full_(0),
      nack_last_seq_number_sent_(0),
      simulcast_(false),
      key_frame_req_method_(kKeyFrameReqFirRtp),
      remote_bitrate_(configuration.remote_bitrate_estimator),
#ifdef MATLAB
      , plot1_(NULL),
#endif
      rtt_observer_(configuration.rtt_observer) {
  send_video_codec_.codecType = kVideoCodecUnknown;

  if (default_module_) {
    default_module_->RegisterChildModule(this);
  }
  // TODO(pwestin) move to constructors of each rtp/rtcp sender/receiver object.
  rtcp_receiver_.RegisterRtcpObservers(configuration.intra_frame_callback,
                                       configuration.bandwidth_callback,
                                       configuration.rtcp_feedback);
  rtcp_sender_.RegisterSendTransport(configuration.outgoing_transport);

  // Make sure that RTCP objects are aware of our SSRC.
  uint32_t SSRC = rtp_sender_.SSRC();
  rtcp_sender_.SetSSRC(SSRC);
  SetRtcpReceiverSsrcs(SSRC);

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s created", __FUNCTION__);
}

ModuleRtpRtcpImpl::~ModuleRtpRtcpImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s deleted", __FUNCTION__);

  // All child modules MUST be deleted before deleting the default.
  assert(child_modules_.empty());

  // Deregister for the child modules.
  // Will go in to the default and remove it self.
  if (default_module_) {
    default_module_->DeRegisterChildModule(this);
  }
#ifdef MATLAB
  if (plot1_) {
    eng.DeletePlot(plot1_);
    plot1_ = NULL;
  }
#endif
}

void ModuleRtpRtcpImpl::RegisterChildModule(RtpRtcp* module) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterChildModule(module:0x%x)",
               module);

  CriticalSectionScoped lock(
      critical_section_module_ptrs_.get());
  CriticalSectionScoped double_lock(
      critical_section_module_ptrs_feedback_.get());

  // We use two locks for protecting child_modules_, one
  // (critical_section_module_ptrs_feedback_) for incoming
  // messages (BitrateSent) and critical_section_module_ptrs_
  // for all outgoing messages sending packets etc.
  child_modules_.push_back(static_cast<ModuleRtpRtcpImpl*>(module));
}

void ModuleRtpRtcpImpl::DeRegisterChildModule(RtpRtcp* remove_module) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "DeRegisterChildModule(module:0x%x)", remove_module);

  CriticalSectionScoped lock(
      critical_section_module_ptrs_.get());
  CriticalSectionScoped double_lock(
      critical_section_module_ptrs_feedback_.get());

  std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
  while (it != child_modules_.end()) {
    RtpRtcp* module = *it;
    if (module == remove_module) {
      child_modules_.erase(it);
      return;
    }
    it++;
  }
}

// Returns the number of milliseconds until the module want a worker thread
// to call Process.
int32_t ModuleRtpRtcpImpl::TimeUntilNextProcess() {
    const int64_t now = clock_->TimeInMilliseconds();
  return kRtpRtcpMaxIdleTimeProcess - (now - last_process_time_);
}

// Process any pending tasks such as timeouts (non time critical events).
int32_t ModuleRtpRtcpImpl::Process() {
  const int64_t now = clock_->TimeInMilliseconds();
  last_process_time_ = now;

  if (now >= last_bitrate_process_time_ + kRtpRtcpBitrateProcessTimeMs) {
    rtp_sender_.ProcessBitrate();
    last_bitrate_process_time_ = now;
  }

  bool default_instance = false;
  {
    CriticalSectionScoped cs(critical_section_module_ptrs_.get());
    if (!child_modules_.empty())
      default_instance = true;
  }
  if (!default_instance) {
    if (rtcp_sender_.Sending()) {
      // Process RTT if we have received a receiver report and we haven't
      // processed RTT for at least |kRtpRtcpRttProcessTimeMs| milliseconds.
      if (rtcp_receiver_.LastReceivedReceiverReport() >
          last_rtt_process_time_ && now >= last_rtt_process_time_ +
          kRtpRtcpRttProcessTimeMs) {
        last_rtt_process_time_ = now;
        std::vector<RTCPReportBlock> receive_blocks;
        rtcp_receiver_.StatisticsReceived(&receive_blocks);
        uint16_t max_rtt = 0;
        for (std::vector<RTCPReportBlock>::iterator it = receive_blocks.begin();
             it != receive_blocks.end(); ++it) {
          uint16_t rtt = 0;
          rtcp_receiver_.RTT(it->remoteSSRC, &rtt, NULL, NULL, NULL);
          max_rtt = (rtt > max_rtt) ? rtt : max_rtt;
        }
        // Report the rtt.
        if (rtt_observer_ && max_rtt != 0)
          rtt_observer_->OnRttUpdate(max_rtt);
      }

      // Verify receiver reports are delivered and the reported sequence number
      // is increasing.
      int64_t rtcp_interval = RtcpReportInterval();
      if (rtcp_receiver_.RtcpRrTimeout(rtcp_interval)) {
        LOG_F(LS_WARNING) << "Timeout: No RTCP RR received.";
      } else if (rtcp_receiver_.RtcpRrSequenceNumberTimeout(rtcp_interval)) {
        LOG_F(LS_WARNING) <<
            "Timeout: No increase in RTCP RR extended highest sequence number.";
      }

      if (remote_bitrate_ && rtcp_sender_.TMMBR()) {
        unsigned int target_bitrate = 0;
        std::vector<unsigned int> ssrcs;
        if (remote_bitrate_->LatestEstimate(&ssrcs, &target_bitrate)) {
          if (!ssrcs.empty()) {
            target_bitrate = target_bitrate / ssrcs.size();
          }
          rtcp_sender_.SetTargetBitrate(target_bitrate);
        }
      }
    }
    if (rtcp_sender_.TimeToSendRTCPReport()) {
      RTCPSender::FeedbackState feedback_state(this);
      rtcp_sender_.SendRTCP(feedback_state, kRtcpReport);
    }
  }

  if (UpdateRTCPReceiveInformationTimers()) {
    // A receiver has timed out
    rtcp_receiver_.UpdateTMMBR();
  }
  return 0;
}

int32_t ModuleRtpRtcpImpl::SetRTXSendStatus(RtxMode mode, bool set_ssrc,
                                            uint32_t ssrc) {
  rtp_sender_.SetRTXStatus(mode, set_ssrc, ssrc);


  return 0;
}

int32_t ModuleRtpRtcpImpl::RTXSendStatus(RtxMode* mode, uint32_t* ssrc,
                                         int* payload_type) const {
  rtp_sender_.RTXStatus(mode, ssrc, payload_type);
  return 0;
}

void ModuleRtpRtcpImpl::SetRtxSendPayloadType(int payload_type) {
  rtp_sender_.SetRtxPayloadType(payload_type);
}

int32_t ModuleRtpRtcpImpl::IncomingRtcpPacket(
    const uint8_t* rtcp_packet,
    const uint16_t length) {
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
               "IncomingRtcpPacket(packet_length:%u)", length);
  // Minimum RTP is 12 bytes.
  // Minimum RTCP is 8 bytes (RTCP BYE).
  if (length == 8) {
    WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, -1,
                 "IncomingRtcpPacket invalid length");
    return false;
  }
  // Check RTP version.
  const uint8_t version = rtcp_packet[0] >> 6;
  if (version != 2) {
    WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, -1,
                 "IncomingRtcpPacket invalid RTP version");
    return false;
  }
  // Allow receive of non-compound RTCP packets.
  RTCPUtility::RTCPParserV2 rtcp_parser(rtcp_packet, length, true);

  const bool valid_rtcpheader = rtcp_parser.IsValid();
  if (!valid_rtcpheader) {
    WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, id_,
                 "IncomingRtcpPacket invalid RTCP packet");
    return -1;
  }
  RTCPHelp::RTCPPacketInformation rtcp_packet_information;
  int32_t ret_val = rtcp_receiver_.IncomingRTCPPacket(
      rtcp_packet_information, &rtcp_parser);
  if (ret_val == 0) {
    rtcp_receiver_.TriggerCallbacksFromRTCPPacket(rtcp_packet_information);
  }
  return ret_val;
}

int32_t ModuleRtpRtcpImpl::RegisterSendPayload(
    const CodecInst& voice_codec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterSendPayload(pl_name:%s pl_type:%d frequency:%u)",
               voice_codec.plname,
               voice_codec.pltype,
               voice_codec.plfreq);

  return rtp_sender_.RegisterPayload(
           voice_codec.plname,
           voice_codec.pltype,
           voice_codec.plfreq,
           voice_codec.channels,
           (voice_codec.rate < 0) ? 0 : voice_codec.rate);
}

int32_t ModuleRtpRtcpImpl::RegisterSendPayload(
    const VideoCodec& video_codec) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RegisterSendPayload(pl_name:%s pl_type:%d)",
               video_codec.plName,
               video_codec.plType);

  send_video_codec_ = video_codec;
  simulcast_ = (video_codec.numberOfSimulcastStreams > 1) ? true : false;
  return rtp_sender_.RegisterPayload(video_codec.plName,
                                     video_codec.plType,
                                     90000,
                                     0,
                                     video_codec.maxBitrate);
}

int32_t ModuleRtpRtcpImpl::DeRegisterSendPayload(
    const int8_t payload_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "DeRegisterSendPayload(%d)", payload_type);

  return rtp_sender_.DeRegisterSendPayload(payload_type);
}

int8_t ModuleRtpRtcpImpl::SendPayloadType() const {
  return rtp_sender_.SendPayloadType();
}

uint32_t ModuleRtpRtcpImpl::StartTimestamp() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "StartTimestamp()");

  return rtp_sender_.StartTimestamp();
}

// Configure start timestamp, default is a random number.
int32_t ModuleRtpRtcpImpl::SetStartTimestamp(
    const uint32_t timestamp) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetStartTimestamp(%d)",
               timestamp);
  rtcp_sender_.SetStartTimestamp(timestamp);
  rtp_sender_.SetStartTimestamp(timestamp, true);
  return 0;  // TODO(pwestin): change to void.
}

uint16_t ModuleRtpRtcpImpl::SequenceNumber() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SequenceNumber()");

  return rtp_sender_.SequenceNumber();
}

// Set SequenceNumber, default is a random number.
int32_t ModuleRtpRtcpImpl::SetSequenceNumber(
    const uint16_t seq_num) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetSequenceNumber(%d)",
               seq_num);

  rtp_sender_.SetSequenceNumber(seq_num);
  return 0;  // TODO(pwestin): change to void.
}

uint32_t ModuleRtpRtcpImpl::SSRC() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SSRC()");

  return rtp_sender_.SSRC();
}

// Configure SSRC, default is a random number.
int32_t ModuleRtpRtcpImpl::SetSSRC(const uint32_t ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetSSRC(%d)", ssrc);

  rtp_sender_.SetSSRC(ssrc);
  rtcp_sender_.SetSSRC(ssrc);
  SetRtcpReceiverSsrcs(ssrc);

  return 0;  // TODO(pwestin): change to void.
}

int32_t ModuleRtpRtcpImpl::SetCSRCStatus(const bool include) {
  rtcp_sender_.SetCSRCStatus(include);
  rtp_sender_.SetCSRCStatus(include);
  return 0;  // TODO(pwestin): change to void.
}

int32_t ModuleRtpRtcpImpl::CSRCs(
  uint32_t arr_of_csrc[kRtpCsrcSize]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "CSRCs()");

  return rtp_sender_.CSRCs(arr_of_csrc);
}

int32_t ModuleRtpRtcpImpl::SetCSRCs(
    const uint32_t arr_of_csrc[kRtpCsrcSize],
    const uint8_t arr_length) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetCSRCs(arr_length:%d)",
               arr_length);

  const bool default_instance(child_modules_.empty() ? false : true);

  if (default_instance) {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCSRCs(arr_of_csrc, arr_length);
      }
      it++;
    }
  } else {
    for (int i = 0; i < arr_length; ++i) {
      WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "\tidx:%d CSRC:%u", i,
                   arr_of_csrc[i]);
    }
    rtcp_sender_.SetCSRCs(arr_of_csrc, arr_length);
    rtp_sender_.SetCSRCs(arr_of_csrc, arr_length);
  }
  return 0;  // TODO(pwestin): change to void.
}

uint32_t ModuleRtpRtcpImpl::PacketCountSent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "PacketCountSent()");

  return rtp_sender_.Packets();
}

uint32_t ModuleRtpRtcpImpl::ByteCountSent() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "ByteCountSent()");

  return rtp_sender_.Bytes();
}

int ModuleRtpRtcpImpl::CurrentSendFrequencyHz() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "CurrentSendFrequencyHz()");

  return rtp_sender_.SendPayloadFrequency();
}

int32_t ModuleRtpRtcpImpl::SetSendingStatus(const bool sending) {
  if (sending) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingStatus(sending)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingStatus(stopped)");
  }
  if (rtcp_sender_.Sending() != sending) {
    // Sends RTCP BYE when going from true to false
    RTCPSender::FeedbackState feedback_state(this);
    if (rtcp_sender_.SetSendingStatus(feedback_state, sending) != 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                   "Failed to send RTCP BYE");
    }

    collision_detected_ = false;

    // Generate a new time_stamp if true and not configured via API
    // Generate a new SSRC for the next "call" if false
    rtp_sender_.SetSendingStatus(sending);
    if (sending) {
      // Make sure the RTCP sender has the same timestamp offset.
      rtcp_sender_.SetStartTimestamp(rtp_sender_.StartTimestamp());
    }

    // Make sure that RTCP objects are aware of our SSRC (it could have changed
    // Due to collision)
    uint32_t SSRC = rtp_sender_.SSRC();
    rtcp_sender_.SetSSRC(SSRC);
    SetRtcpReceiverSsrcs(SSRC);

    return 0;
  }
  return 0;
}

bool ModuleRtpRtcpImpl::Sending() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "Sending()");

  return rtcp_sender_.Sending();
}

int32_t ModuleRtpRtcpImpl::SetSendingMediaStatus(const bool sending) {
  if (sending) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingMediaStatus(sending)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetSendingMediaStatus(stopped)");
  }
  rtp_sender_.SetSendingMediaStatus(sending);
  return 0;
}

bool ModuleRtpRtcpImpl::SendingMedia() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "Sending()");

  const bool have_child_modules(child_modules_.empty() ? false : true);
  if (!have_child_modules) {
    return rtp_sender_.SendingMedia();
  }

  CriticalSectionScoped lock(critical_section_module_ptrs_.get());
  std::list<ModuleRtpRtcpImpl*>::const_iterator it = child_modules_.begin();
  while (it != child_modules_.end()) {
    RTPSender& rtp_sender = (*it)->rtp_sender_;
    if (rtp_sender.SendingMedia()) {
      return true;
    }
    it++;
  }
  return false;
}

int32_t ModuleRtpRtcpImpl::SendOutgoingData(
    FrameType frame_type,
    int8_t payload_type,
    uint32_t time_stamp,
    int64_t capture_time_ms,
    const uint8_t* payload_data,
    uint32_t payload_size,
    const RTPFragmentationHeader* fragmentation,
    const RTPVideoHeader* rtp_video_hdr) {
  WEBRTC_TRACE(
    kTraceStream,
    kTraceRtpRtcp,
    id_,
    "SendOutgoingData(frame_type:%d payload_type:%d time_stamp:%u size:%u)",
    frame_type, payload_type, time_stamp, payload_size);

  rtcp_sender_.SetLastRtpTime(time_stamp, capture_time_ms);

  const bool have_child_modules(child_modules_.empty() ? false : true);
  if (!have_child_modules) {
    // Don't send RTCP from default module.
    if (rtcp_sender_.TimeToSendRTCPReport(kVideoFrameKey == frame_type)) {
      RTCPSender::FeedbackState feedback_state(this);
      rtcp_sender_.SendRTCP(feedback_state, kRtcpReport);
    }
    return rtp_sender_.SendOutgoingData(frame_type,
                                        payload_type,
                                        time_stamp,
                                        capture_time_ms,
                                        payload_data,
                                        payload_size,
                                        fragmentation,
                                        NULL,
                                        &(rtp_video_hdr->codecHeader));
  }
  int32_t ret_val = -1;
  if (simulcast_) {
    if (rtp_video_hdr == NULL) {
      return -1;
    }
    int idx = 0;
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    for (; idx < rtp_video_hdr->simulcastIdx; ++it) {
      if (it == child_modules_.end()) {
        return -1;
      }
      if ((*it)->SendingMedia()) {
        ++idx;
      }
    }
    for (; it != child_modules_.end(); ++it) {
      if ((*it)->SendingMedia()) {
        break;
      }
      ++idx;
    }
    if (it == child_modules_.end()) {
      return -1;
    }
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SendOutgoingData(SimulcastIdx:%u size:%u, ssrc:0x%x)",
                 idx, payload_size, (*it)->rtp_sender_.SSRC());
    return (*it)->SendOutgoingData(frame_type,
                                   payload_type,
                                   time_stamp,
                                   capture_time_ms,
                                   payload_data,
                                   payload_size,
                                   fragmentation,
                                   rtp_video_hdr);
  } else {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    // Send to all "child" modules
    while (it != child_modules_.end()) {
      if ((*it)->SendingMedia()) {
        ret_val = (*it)->SendOutgoingData(frame_type,
                                          payload_type,
                                          time_stamp,
                                          capture_time_ms,
                                          payload_data,
                                          payload_size,
                                          fragmentation,
                                          rtp_video_hdr);
      }
      it++;
    }
  }
  return ret_val;
}

bool ModuleRtpRtcpImpl::TimeToSendPacket(uint32_t ssrc,
                                         uint16_t sequence_number,
                                         int64_t capture_time_ms) {
  WEBRTC_TRACE(
    kTraceStream,
    kTraceRtpRtcp,
    id_,
    "TimeToSendPacket(ssrc:0x%x sequence_number:%u capture_time_ms:%ll)",
    ssrc, sequence_number, capture_time_ms);

  bool no_child_modules = false;
  {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    no_child_modules = child_modules_.empty();
  }
  if (no_child_modules) {
    // Don't send from default module.
    if (SendingMedia() && ssrc == rtp_sender_.SSRC()) {
      return rtp_sender_.TimeToSendPacket(sequence_number, capture_time_ms);
    }
  } else {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      if ((*it)->SendingMedia() && ssrc == (*it)->rtp_sender_.SSRC()) {
        return (*it)->rtp_sender_.TimeToSendPacket(sequence_number,
                                                   capture_time_ms);
      }
      ++it;
    }
  }
  // No RTP sender is interested in sending this packet.
  return true;
}

int ModuleRtpRtcpImpl::TimeToSendPadding(int bytes) {
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_, "TimeToSendPadding(bytes: %d)",
               bytes);

  bool no_child_modules = false;
  {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    no_child_modules = child_modules_.empty();
  }
  if (no_child_modules) {
    // Don't send from default module.
    if (SendingMedia()) {
      return rtp_sender_.TimeToSendPadding(bytes);
    }
  } else {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      // Send padding on one of the modules sending media.
      if ((*it)->SendingMedia()) {
        return (*it)->rtp_sender_.TimeToSendPadding(bytes);
      }
      ++it;
    }
  }
  return 0;
}

uint16_t ModuleRtpRtcpImpl::MaxPayloadLength() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "MaxPayloadLength()");

  return rtp_sender_.MaxPayloadLength();
}

uint16_t ModuleRtpRtcpImpl::MaxDataPayloadLength() const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "MaxDataPayloadLength()");

  // Assuming IP/UDP.
  uint16_t min_data_payload_length = IP_PACKET_SIZE - 28;

  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance) {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        uint16_t data_payload_length =
          module->MaxDataPayloadLength();
        if (data_payload_length < min_data_payload_length) {
          min_data_payload_length = data_payload_length;
        }
      }
      it++;
    }
  }

  uint16_t data_payload_length = rtp_sender_.MaxDataPayloadLength();
  if (data_payload_length < min_data_payload_length) {
    min_data_payload_length = data_payload_length;
  }
  return min_data_payload_length;
}

int32_t ModuleRtpRtcpImpl::SetTransportOverhead(
    const bool tcp,
    const bool ipv6,
    const uint8_t authentication_overhead) {
  WEBRTC_TRACE(
    kTraceModuleCall,
    kTraceRtpRtcp,
    id_,
    "SetTransportOverhead(TCP:%d, IPV6:%d authentication_overhead:%u)",
    tcp, ipv6, authentication_overhead);

  uint16_t packet_overhead = 0;
  if (ipv6) {
    packet_overhead = 40;
  } else {
    packet_overhead = 20;
  }
  if (tcp) {
    // TCP.
    packet_overhead += 20;
  } else {
    // UDP.
    packet_overhead += 8;
  }
  packet_overhead += authentication_overhead;

  if (packet_overhead == packet_overhead_) {
    // Ok same as before.
    return 0;
  }
  // Calc diff.
  int16_t packet_over_head_diff = packet_overhead - packet_overhead_;

  // Store new.
  packet_overhead_ = packet_overhead;

  uint16_t length =
      rtp_sender_.MaxPayloadLength() - packet_over_head_diff;
  return rtp_sender_.SetMaxPayloadLength(length, packet_overhead_);
}

int32_t ModuleRtpRtcpImpl::SetMaxTransferUnit(const uint16_t mtu) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetMaxTransferUnit(%u)",
               mtu);

  if (mtu > IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "Invalid in argument to SetMaxTransferUnit(%u)", mtu);
    return -1;
  }
  return rtp_sender_.SetMaxPayloadLength(mtu - packet_overhead_,
                                         packet_overhead_);
}

RTCPMethod ModuleRtpRtcpImpl::RTCP() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RTCP()");

  if (rtcp_sender_.Status() != kRtcpOff) {
    return rtcp_receiver_.Status();
  }
  return kRtcpOff;
}

// Configure RTCP status i.e on/off.
int32_t ModuleRtpRtcpImpl::SetRTCPStatus(const RTCPMethod method) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetRTCPStatus(%d)",
               method);

  if (rtcp_sender_.SetRTCPStatus(method) == 0) {
    return rtcp_receiver_.SetRTCPStatus(method);
  }
  return -1;
}

// Only for internal test.
uint32_t ModuleRtpRtcpImpl::LastSendReport(
    uint32_t& last_rtcptime) {
  return rtcp_sender_.LastSendReport(last_rtcptime);
}

int32_t ModuleRtpRtcpImpl::SetCNAME(const char c_name[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetCNAME(%s)", c_name);
  return rtcp_sender_.SetCNAME(c_name);
}

int32_t ModuleRtpRtcpImpl::CNAME(char c_name[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "CNAME()");
  return rtcp_sender_.CNAME(c_name);
}

int32_t ModuleRtpRtcpImpl::AddMixedCNAME(
  const uint32_t ssrc,
  const char c_name[RTCP_CNAME_SIZE]) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "AddMixedCNAME(SSRC:%u)", ssrc);

  return rtcp_sender_.AddMixedCNAME(ssrc, c_name);
}

int32_t ModuleRtpRtcpImpl::RemoveMixedCNAME(const uint32_t ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "RemoveMixedCNAME(SSRC:%u)", ssrc);
  return rtcp_sender_.RemoveMixedCNAME(ssrc);
}

int32_t ModuleRtpRtcpImpl::RemoteCNAME(
    const uint32_t remote_ssrc,
    char c_name[RTCP_CNAME_SIZE]) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "RemoteCNAME(SSRC:%u)", remote_ssrc);

  return rtcp_receiver_.CNAME(remote_ssrc, c_name);
}

int32_t ModuleRtpRtcpImpl::RemoteNTP(
    uint32_t* received_ntpsecs,
    uint32_t* received_ntpfrac,
    uint32_t* rtcp_arrival_time_secs,
    uint32_t* rtcp_arrival_time_frac,
    uint32_t* rtcp_timestamp) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteNTP()");

  return rtcp_receiver_.NTP(received_ntpsecs,
                            received_ntpfrac,
                            rtcp_arrival_time_secs,
                            rtcp_arrival_time_frac,
                            rtcp_timestamp);
}

// Get RoundTripTime.
int32_t ModuleRtpRtcpImpl::RTT(const uint32_t remote_ssrc,
                               uint16_t* rtt,
                               uint16_t* avg_rtt,
                               uint16_t* min_rtt,
                               uint16_t* max_rtt) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RTT()");

  return rtcp_receiver_.RTT(remote_ssrc, rtt, avg_rtt, min_rtt, max_rtt);
}

// Reset RoundTripTime statistics.
int32_t ModuleRtpRtcpImpl::ResetRTT(const uint32_t remote_ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "ResetRTT(SSRC:%u)",
               remote_ssrc);

  return rtcp_receiver_.ResetRTT(remote_ssrc);
}

void ModuleRtpRtcpImpl:: SetRtt(uint32_t rtt) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetRtt(rtt: %u)", rtt);
  rtcp_receiver_.SetRTT(static_cast<uint16_t>(rtt));
}

// Reset RTP data counters for the sending side.
int32_t ModuleRtpRtcpImpl::ResetSendDataCountersRTP() {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "ResetSendDataCountersRTP()");

  rtp_sender_.ResetDataCounters();
  return 0;  // TODO(pwestin): change to void.
}

// Force a send of an RTCP packet.
// Normal SR and RR are triggered via the process function.
int32_t ModuleRtpRtcpImpl::SendRTCP(uint32_t rtcp_packet_type) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SendRTCP(0x%x)",
               rtcp_packet_type);
  RTCPSender::FeedbackState feedback_state(this);
  return rtcp_sender_.SendRTCP(feedback_state, rtcp_packet_type);
}

int32_t ModuleRtpRtcpImpl::SetRTCPApplicationSpecificData(
    const uint8_t sub_type,
    const uint32_t name,
    const uint8_t* data,
    const uint16_t length) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetRTCPApplicationSpecificData(sub_type:%d name:0x%x)",
               sub_type, name);

  return  rtcp_sender_.SetApplicationSpecificData(sub_type, name, data, length);
}

// (XR) VOIP metric.
int32_t ModuleRtpRtcpImpl::SetRTCPVoIPMetrics(
  const RTCPVoIPMetric* voip_metric) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetRTCPVoIPMetrics()");

  return  rtcp_sender_.SetRTCPVoIPMetrics(voip_metric);
}

int32_t ModuleRtpRtcpImpl::DataCountersRTP(
    uint32_t* bytes_sent,
    uint32_t* packets_sent) const {
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_, "DataCountersRTP()");

  if (bytes_sent) {
    *bytes_sent = rtp_sender_.Bytes();
  }
  if (packets_sent) {
    *packets_sent = rtp_sender_.Packets();
  }
  return 0;
}

int32_t ModuleRtpRtcpImpl::RemoteRTCPStat(RTCPSenderInfo* sender_info) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteRTCPStat()");

  return rtcp_receiver_.SenderInfoReceived(sender_info);
}

// Received RTCP report.
int32_t ModuleRtpRtcpImpl::RemoteRTCPStat(
    std::vector<RTCPReportBlock>* receive_blocks) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoteRTCPStat()");

  return rtcp_receiver_.StatisticsReceived(receive_blocks);
}

int32_t ModuleRtpRtcpImpl::AddRTCPReportBlock(
    const uint32_t ssrc,
    const RTCPReportBlock* report_block) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "AddRTCPReportBlock()");

  return rtcp_sender_.AddExternalReportBlock(ssrc, report_block);
}

int32_t ModuleRtpRtcpImpl::RemoveRTCPReportBlock(
  const uint32_t ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemoveRTCPReportBlock()");

  return rtcp_sender_.RemoveExternalReportBlock(ssrc);
}

// (REMB) Receiver Estimated Max Bitrate.
bool ModuleRtpRtcpImpl::REMB() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "REMB()");

  return rtcp_sender_.REMB();
}

int32_t ModuleRtpRtcpImpl::SetREMBStatus(const bool enable) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetREMBStatus(enable)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetREMBStatus(disable)");
  }
  return rtcp_sender_.SetREMBStatus(enable);
}

int32_t ModuleRtpRtcpImpl::SetREMBData(const uint32_t bitrate,
                                       const uint8_t number_of_ssrc,
                                       const uint32_t* ssrc) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetREMBData(bitrate:%d,?,?)", bitrate);
  return rtcp_sender_.SetREMBData(bitrate, number_of_ssrc, ssrc);
}

// (IJ) Extended jitter report.
bool ModuleRtpRtcpImpl::IJ() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "IJ()");

  return rtcp_sender_.IJ();
}

int32_t ModuleRtpRtcpImpl::SetIJStatus(const bool enable) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetIJStatus(%s)", enable ? "true" : "false");

  return rtcp_sender_.SetIJStatus(enable);
}

int32_t ModuleRtpRtcpImpl::RegisterSendRtpHeaderExtension(
    const RTPExtensionType type,
    const uint8_t id) {
  return rtp_sender_.RegisterRtpHeaderExtension(type, id);
}

int32_t ModuleRtpRtcpImpl::DeregisterSendRtpHeaderExtension(
    const RTPExtensionType type) {
  return rtp_sender_.DeregisterRtpHeaderExtension(type);
}

// (TMMBR) Temporary Max Media Bit Rate.
bool ModuleRtpRtcpImpl::TMMBR() const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "TMMBR()");

  return rtcp_sender_.TMMBR();
}

int32_t ModuleRtpRtcpImpl::SetTMMBRStatus(const bool enable) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetTMMBRStatus(enable)");
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetTMMBRStatus(disable)");
  }
  return rtcp_sender_.SetTMMBRStatus(enable);
}

int32_t ModuleRtpRtcpImpl::SetTMMBN(const TMMBRSet* bounding_set) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SetTMMBN()");

  uint32_t max_bitrate_kbit =
      rtp_sender_.MaxConfiguredBitrateVideo() / 1000;
  return rtcp_sender_.SetTMMBN(bounding_set, max_bitrate_kbit);
}

// Returns the currently configured retransmission mode.
int ModuleRtpRtcpImpl::SelectiveRetransmissions() const {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SelectiveRetransmissions()");
  return rtp_sender_.SelectiveRetransmissions();
}

// Enable or disable a retransmission mode, which decides which packets will
// be retransmitted if NACKed.
int ModuleRtpRtcpImpl::SetSelectiveRetransmissions(uint8_t settings) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetSelectiveRetransmissions(%u)",
               settings);
  return rtp_sender_.SetSelectiveRetransmissions(settings);
}

// Send a Negative acknowledgment packet.
int32_t ModuleRtpRtcpImpl::SendNACK(const uint16_t* nack_list,
                                    const uint16_t size) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SendNACK(size:%u)", size);

  uint16_t avg_rtt = 0;
  rtcp_receiver_.RTT(rtcp_receiver_.RemoteSSRC(), NULL, &avg_rtt, NULL, NULL);

  int64_t wait_time = 5 + ((avg_rtt * 3) >> 1);  // 5 + RTT * 1.5.
  if (wait_time == 5) {
    wait_time = 100;  // During startup we don't have an RTT.
  }
  const int64_t now = clock_->TimeInMilliseconds();
  const int64_t time_limit = now - wait_time;
  uint16_t nackLength = size;
  uint16_t start_id = 0;

  if (nack_last_time_sent_full_ < time_limit) {
    // Send list. Set the timer to make sure we only send a full NACK list once
    // within every time_limit.
    nack_last_time_sent_full_ = now;
  } else {
    // Only send if extended list.
    if (nack_last_seq_number_sent_ == nack_list[size - 1]) {
      // Last seq num is the same don't send list.
      return 0;
    } else {
      // Send NACKs only for new sequence numbers to avoid re-sending
      // NACKs for sequences we have already sent.
      for (int i = 0; i < size; ++i)  {
        if (nack_last_seq_number_sent_ == nack_list[i]) {
          start_id = i + 1;
          break;
        }
      }
      nackLength = size - start_id;
    }
  }
  // Our RTCP NACK implementation is limited to kRtcpMaxNackFields sequence
  // numbers per RTCP packet.
  if (nackLength > kRtcpMaxNackFields) {
    nackLength = kRtcpMaxNackFields;
  }
  nack_last_seq_number_sent_ = nack_list[start_id + nackLength - 1];

  RTCPSender::FeedbackState feedback_state(this);
  return rtcp_sender_.SendRTCP(
      feedback_state, kRtcpNack, nackLength, &nack_list[start_id]);
}

// Store the sent packets, needed to answer to a Negative acknowledgment
// requests.
int32_t ModuleRtpRtcpImpl::SetStorePacketsStatus(
    const bool enable,
    const uint16_t number_to_store) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetStorePacketsStatus(enable, number_to_store:%d)",
                 number_to_store);
  } else {
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
                 "SetStorePacketsStatus(disable)");
  }
  rtp_sender_.SetStorePacketsStatus(enable, number_to_store);
  return 0;  // TODO(pwestin): change to void.
}

bool ModuleRtpRtcpImpl::StorePackets() const {
  return rtp_sender_.StorePackets();
}

// Send a TelephoneEvent tone using RFC 2833 (4733).
int32_t ModuleRtpRtcpImpl::SendTelephoneEventOutband(
    const uint8_t key,
    const uint16_t time_ms,
    const uint8_t level) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SendTelephoneEventOutband(key:%u, time_ms:%u, level:%u)", key,
               time_ms, level);

  return rtp_sender_.SendTelephoneEvent(key, time_ms, level);
}

bool ModuleRtpRtcpImpl::SendTelephoneEventActive(
    int8_t& telephone_event) const {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SendTelephoneEventActive()");

  return rtp_sender_.SendTelephoneEventActive(&telephone_event);
}

// Set audio packet size, used to determine when it's time to send a DTMF
// packet in silence (CNG).
int32_t ModuleRtpRtcpImpl::SetAudioPacketSize(
    const uint16_t packet_size_samples) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetAudioPacketSize(%u)",
               packet_size_samples);

  return rtp_sender_.SetAudioPacketSize(packet_size_samples);
}

int32_t ModuleRtpRtcpImpl::SetRTPAudioLevelIndicationStatus(
    const bool enable,
    const uint8_t id) {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetRTPAudioLevelIndicationStatus(enable=%d, ID=%u)",
               enable,
               id);

  return rtp_sender_.SetAudioLevelIndicationStatus(enable, id);
}

int32_t ModuleRtpRtcpImpl::GetRTPAudioLevelIndicationStatus(
    bool& enable,
    uint8_t& id) const {

  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "GetRTPAudioLevelIndicationStatus()");
  return rtp_sender_.AudioLevelIndicationStatus(&enable, &id);
}

int32_t ModuleRtpRtcpImpl::SetAudioLevel(
    const uint8_t level_d_bov) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetAudioLevel(level_d_bov:%u)",
               level_d_bov);
  return rtp_sender_.SetAudioLevel(level_d_bov);
}

// Set payload type for Redundant Audio Data RFC 2198.
int32_t ModuleRtpRtcpImpl::SetSendREDPayloadType(
    const int8_t payload_type) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetSendREDPayloadType(%d)",
               payload_type);

  return rtp_sender_.SetRED(payload_type);
}

// Get payload type for Redundant Audio Data RFC 2198.
int32_t ModuleRtpRtcpImpl::SendREDPayloadType(
    int8_t& payload_type) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "SendREDPayloadType()");

  return rtp_sender_.RED(&payload_type);
}

RtpVideoCodecTypes ModuleRtpRtcpImpl::SendVideoCodec() const {
  return rtp_sender_.VideoCodecType();
}

void ModuleRtpRtcpImpl::SetTargetSendBitrate(
    const std::vector<uint32_t>& stream_bitrates) {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_,
               "SetTargetSendBitrate: %ld streams", stream_bitrates.size());

  const bool have_child_modules(child_modules_.empty() ? false : true);
  if (have_child_modules) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    if (simulcast_) {
      std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
      for (size_t i = 0;
           it != child_modules_.end() && i < stream_bitrates.size(); ++it) {
        if ((*it)->SendingMedia()) {
          RTPSender& rtp_sender = (*it)->rtp_sender_;
          rtp_sender.SetTargetSendBitrate(stream_bitrates[i]);
          ++i;
        }
      }
    } else {
      assert(stream_bitrates.size() == 1);
      std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
      for (; it != child_modules_.end(); ++it) {
        RTPSender& rtp_sender = (*it)->rtp_sender_;
        rtp_sender.SetTargetSendBitrate(stream_bitrates[0]);
      }
    }
  } else {
    assert(stream_bitrates.size() == 1);
    rtp_sender_.SetTargetSendBitrate(stream_bitrates[0]);
  }
}

int32_t ModuleRtpRtcpImpl::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetKeyFrameRequestMethod(method:%u)",
               method);

  key_frame_req_method_ = method;
  return 0;
}

int32_t ModuleRtpRtcpImpl::RequestKeyFrame() {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "RequestKeyFrame");

  switch (key_frame_req_method_) {
    case kKeyFrameReqFirRtp:
      return rtp_sender_.SendRTPIntraRequest();
    case kKeyFrameReqPliRtcp:
      return SendRTCP(kRtcpPli);
    case kKeyFrameReqFirRtcp:
      return SendRTCP(kRtcpFir);
  }
  return -1;
}

int32_t ModuleRtpRtcpImpl::SendRTCPSliceLossIndication(
    const uint8_t picture_id) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SendRTCPSliceLossIndication (picture_id:%d)",
               picture_id);

  RTCPSender::FeedbackState feedback_state(this);
  return rtcp_sender_.SendRTCP(
      feedback_state, kRtcpSli, 0, 0, false, picture_id);
}

int32_t ModuleRtpRtcpImpl::SetCameraDelay(const int32_t delay_ms) {
  WEBRTC_TRACE(kTraceModuleCall,
               kTraceRtpRtcp,
               id_,
               "SetCameraDelay(%d)",
               delay_ms);
  const bool default_instance(child_modules_.empty() ? false : true);

  if (default_instance) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCameraDelay(delay_ms);
      }
      it++;
    }
    return 0;
  }
  return rtcp_sender_.SetCameraDelay(delay_ms);
}

int32_t ModuleRtpRtcpImpl::SetGenericFECStatus(
    const bool enable,
    const uint8_t payload_type_red,
    const uint8_t payload_type_fec) {
  if (enable) {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetGenericFECStatus(enable, %u)",
                 payload_type_red);
  } else {
    WEBRTC_TRACE(kTraceModuleCall,
                 kTraceRtpRtcp,
                 id_,
                 "SetGenericFECStatus(disable)");
  }
  return rtp_sender_.SetGenericFECStatus(enable,
                                         payload_type_red,
                                         payload_type_fec);
}

int32_t ModuleRtpRtcpImpl::GenericFECStatus(
    bool& enable,
    uint8_t& payload_type_red,
    uint8_t& payload_type_fec) {

  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "GenericFECStatus()");

  bool child_enabled = false;
  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance) {
    // For default we need to check all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module)  {
        bool enabled = false;
        uint8_t dummy_ptype_red = 0;
        uint8_t dummy_ptype_fec = 0;
        if (module->GenericFECStatus(enabled,
                                     dummy_ptype_red,
                                     dummy_ptype_fec) == 0 && enabled) {
          child_enabled = true;
          break;
        }
      }
      it++;
    }
  }
  int32_t ret_val = rtp_sender_.GenericFECStatus(&enable,
                                                 &payload_type_red,
                                                 &payload_type_fec);
  if (child_enabled) {
    // Returns true if enabled for any child module.
    enable = child_enabled;
  }
  return ret_val;
}

int32_t ModuleRtpRtcpImpl::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  const bool default_instance(child_modules_.empty() ? false : true);
  if (default_instance)  {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::list<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetFecParameters(delta_params, key_params);
      }
      it++;
    }
    return 0;
  }
  return rtp_sender_.SetFecParameters(delta_params, key_params);
}

void ModuleRtpRtcpImpl::SetRemoteSSRC(const uint32_t ssrc) {
  // Inform about the incoming SSRC.
  rtcp_sender_.SetRemoteSSRC(ssrc);
  rtcp_receiver_.SetRemoteSSRC(ssrc);

  // Check for a SSRC collision.
  if (rtp_sender_.SSRC() == ssrc && !collision_detected_) {
    // If we detect a collision change the SSRC but only once.
    collision_detected_ = true;
    uint32_t new_ssrc = rtp_sender_.GenerateNewSSRC();
    if (new_ssrc == 0) {
      // Configured via API ignore.
      return;
    }
    if (kRtcpOff != rtcp_sender_.Status()) {
      // Send RTCP bye on the current SSRC.
      SendRTCP(kRtcpBye);
    }
    // Change local SSRC and inform all objects about the new SSRC.
    rtcp_sender_.SetSSRC(new_ssrc);
    SetRtcpReceiverSsrcs(new_ssrc);
  }
}

void ModuleRtpRtcpImpl::BitrateSent(uint32_t* total_rate,
                                    uint32_t* video_rate,
                                    uint32_t* fec_rate,
                                    uint32_t* nack_rate) const {
  const bool default_instance(child_modules_.empty() ? false : true);

  if (default_instance) {
    // For default we need to update the send bitrate.
    CriticalSectionScoped lock(critical_section_module_ptrs_feedback_.get());

    if (total_rate != NULL)
      *total_rate = 0;
    if (video_rate != NULL)
      *video_rate = 0;
    if (fec_rate != NULL)
      *fec_rate = 0;
    if (nack_rate != NULL)
      *nack_rate = 0;

    std::list<ModuleRtpRtcpImpl*>::const_iterator it =
      child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        uint32_t child_total_rate = 0;
        uint32_t child_video_rate = 0;
        uint32_t child_fec_rate = 0;
        uint32_t child_nack_rate = 0;
        module->BitrateSent(&child_total_rate,
                            &child_video_rate,
                            &child_fec_rate,
                            &child_nack_rate);
        if (total_rate != NULL && child_total_rate > *total_rate)
          *total_rate = child_total_rate;
        if (video_rate != NULL && child_video_rate > *video_rate)
          *video_rate = child_video_rate;
        if (fec_rate != NULL && child_fec_rate > *fec_rate)
          *fec_rate = child_fec_rate;
        if (nack_rate != NULL && child_nack_rate > *nack_rate)
          *nack_rate = child_nack_rate;
      }
      it++;
    }
    return;
  }
  if (total_rate != NULL)
    *total_rate = rtp_sender_.BitrateLast();
  if (video_rate != NULL)
    *video_rate = rtp_sender_.VideoBitrateSent();
  if (fec_rate != NULL)
    *fec_rate = rtp_sender_.FecOverheadRate();
  if (nack_rate != NULL)
    *nack_rate = rtp_sender_.NackOverheadRate();
}

// Bad state of RTP receiver request a keyframe.
void ModuleRtpRtcpImpl::OnRequestIntraFrame() {
  RequestKeyFrame();
}

void ModuleRtpRtcpImpl::OnRequestSendReport() {
  SendRTCP(kRtcpSr);
}

int32_t ModuleRtpRtcpImpl::SendRTCPReferencePictureSelection(
    const uint64_t picture_id) {
  RTCPSender::FeedbackState feedback_state(this);
  return rtcp_sender_.SendRTCP(
      feedback_state, kRtcpRpsi, 0, 0, false, picture_id);
}

uint32_t ModuleRtpRtcpImpl::SendTimeOfSendReport(
    const uint32_t send_report) {
  return rtcp_sender_.SendTimeOfSendReport(send_report);
}

void ModuleRtpRtcpImpl::OnReceivedNACK(
    const std::list<uint16_t>& nack_sequence_numbers) {
  if (!rtp_sender_.StorePackets() ||
      nack_sequence_numbers.size() == 0) {
    return;
  }
  uint16_t avg_rtt = 0;
  rtcp_receiver_.RTT(rtcp_receiver_.RemoteSSRC(), NULL, &avg_rtt, NULL, NULL);
  rtp_sender_.OnReceivedNACK(nack_sequence_numbers, avg_rtt);
}

int32_t ModuleRtpRtcpImpl::LastReceivedNTP(
    uint32_t& rtcp_arrival_time_secs,  // When we got the last report.
    uint32_t& rtcp_arrival_time_frac,
    uint32_t& remote_sr) {
  // Remote SR: NTP inside the last received (mid 16 bits from sec and frac).
  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;

  if (-1 == rtcp_receiver_.NTP(&ntp_secs,
                               &ntp_frac,
                               &rtcp_arrival_time_secs,
                               &rtcp_arrival_time_frac,
                               NULL)) {
    return -1;
  }
  remote_sr = ((ntp_secs & 0x0000ffff) << 16) + ((ntp_frac & 0xffff0000) >> 16);
  return 0;
}

bool ModuleRtpRtcpImpl::UpdateRTCPReceiveInformationTimers() {
  // If this returns true this channel has timed out.
  // Periodically check if this is true and if so call UpdateTMMBR.
  return rtcp_receiver_.UpdateRTCPReceiveInformationTimers();
}

// Called from RTCPsender.
int32_t ModuleRtpRtcpImpl::BoundingSet(bool& tmmbr_owner,
                                       TMMBRSet*& bounding_set) {
  return rtcp_receiver_.BoundingSet(tmmbr_owner, bounding_set);
}

int64_t ModuleRtpRtcpImpl::RtcpReportInterval() {
  if (audio_)
    return RTCP_INTERVAL_AUDIO_MS;
  else
    return RTCP_INTERVAL_VIDEO_MS;
}

void ModuleRtpRtcpImpl::SetRtcpReceiverSsrcs(uint32_t main_ssrc) {
  std::set<uint32_t> ssrcs;
  ssrcs.insert(main_ssrc);
  RtxMode rtx_mode = kRtxOff;
  uint32_t rtx_ssrc = 0;
  int rtx_payload_type = 0;
  rtp_sender_.RTXStatus(&rtx_mode, &rtx_ssrc, &rtx_payload_type);
  if (rtx_mode != kRtxOff)
    ssrcs.insert(rtx_ssrc);
  rtcp_receiver_.SetSsrcs(main_ssrc, ssrcs);
}

}  // Namespace webrtc
