/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"

#include <assert.h>  // assert
#include <string.h>  // memcpy

#include <algorithm>  // min
#include <limits>     // max
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

using RTCPUtility::RTCPCnameInformation;

NACKStringBuilder::NACKStringBuilder()
    : stream_(""), count_(0), prevNack_(0), consecutive_(false) {}

NACKStringBuilder::~NACKStringBuilder() {}

void NACKStringBuilder::PushNACK(uint16_t nack) {
  if (count_ == 0) {
    stream_ << nack;
  } else if (nack == prevNack_ + 1) {
    consecutive_ = true;
  } else {
    if (consecutive_) {
      stream_ << "-" << prevNack_;
      consecutive_ = false;
    }
    stream_ << "," << nack;
  }
  count_++;
  prevNack_ = nack;
}

std::string NACKStringBuilder::GetResult() {
  if (consecutive_) {
    stream_ << "-" << prevNack_;
    consecutive_ = false;
  }
  return stream_.str();
}

RTCPSender::FeedbackState::FeedbackState()
    : send_payload_type(0),
      frequency_hz(0),
      packets_sent(0),
      media_bytes_sent(0),
      send_bitrate(0),
      last_rr_ntp_secs(0),
      last_rr_ntp_frac(0),
      remote_sr(0),
      has_last_xr_rr(false),
      module(nullptr) {}

class PacketContainer : public rtcp::CompoundPacket,
                        public rtcp::RtcpPacket::PacketReadyCallback {
 public:
  explicit PacketContainer(Transport* transport)
      : transport_(transport), bytes_sent_(0) {}
  virtual ~PacketContainer() {
    for (RtcpPacket* packet : appended_packets_)
      delete packet;
  }

  void OnPacketReady(uint8_t* data, size_t length) override {
    if (transport_->SendRtcp(data, length))
      bytes_sent_ += length;
  }

  size_t SendPackets() {
    rtcp::CompoundPacket::Build(this);
    return bytes_sent_;
  }

 private:
  Transport* transport_;
  size_t bytes_sent_;
};

class RTCPSender::RtcpContext {
 public:
  RtcpContext(const FeedbackState& feedback_state,
              int32_t nack_size,
              const uint16_t* nack_list,
              bool repeat,
              uint64_t picture_id,
              uint32_t ntp_sec,
              uint32_t ntp_frac,
              PacketContainer* container)
      : feedback_state_(feedback_state),
        nack_size_(nack_size),
        nack_list_(nack_list),
        repeat_(repeat),
        picture_id_(picture_id),
        ntp_sec_(ntp_sec),
        ntp_frac_(ntp_frac),
        container_(container) {}

  virtual ~RtcpContext() {}

  const FeedbackState& feedback_state_;
  const int32_t nack_size_;
  const uint16_t* nack_list_;
  const bool repeat_;
  const uint64_t picture_id_;
  const uint32_t ntp_sec_;
  const uint32_t ntp_frac_;

  PacketContainer* const container_;
};

RTCPSender::RTCPSender(
    bool audio,
    Clock* clock,
    ReceiveStatistics* receive_statistics,
    RtcpPacketTypeCounterObserver* packet_type_counter_observer,
    Transport* outgoing_transport)
    : audio_(audio),
      clock_(clock),
      random_(clock_->TimeInMicroseconds()),
      method_(RtcpMode::kOff),
      transport_(outgoing_transport),

      critical_section_rtcp_sender_(
          CriticalSectionWrapper::CreateCriticalSection()),
      using_nack_(false),
      sending_(false),
      remb_enabled_(false),
      next_time_to_send_rtcp_(clock->TimeInMilliseconds()),
      start_timestamp_(0),
      last_rtp_timestamp_(0),
      last_frame_capture_time_ms_(-1),
      ssrc_(0),
      remote_ssrc_(0),
      receive_statistics_(receive_statistics),

      sequence_number_fir_(0),

      remb_bitrate_(0),

      tmmbr_help_(),
      tmmbr_send_(0),
      packet_oh_send_(0),

      app_sub_type_(0),
      app_name_(0),
      app_data_(nullptr),
      app_length_(0),

      xr_send_receiver_reference_time_enabled_(false),
      packet_type_counter_observer_(packet_type_counter_observer) {
  memset(last_send_report_, 0, sizeof(last_send_report_));
  memset(last_rtcp_time_, 0, sizeof(last_rtcp_time_));
  memset(lastSRPacketCount_, 0, sizeof(lastSRPacketCount_));
  memset(lastSROctetCount_, 0, sizeof(lastSROctetCount_));
  RTC_DCHECK(transport_ != nullptr);

  builders_[kRtcpSr] = &RTCPSender::BuildSR;
  builders_[kRtcpRr] = &RTCPSender::BuildRR;
  builders_[kRtcpSdes] = &RTCPSender::BuildSDES;
  builders_[kRtcpPli] = &RTCPSender::BuildPLI;
  builders_[kRtcpFir] = &RTCPSender::BuildFIR;
  builders_[kRtcpSli] = &RTCPSender::BuildSLI;
  builders_[kRtcpRpsi] = &RTCPSender::BuildRPSI;
  builders_[kRtcpRemb] = &RTCPSender::BuildREMB;
  builders_[kRtcpBye] = &RTCPSender::BuildBYE;
  builders_[kRtcpApp] = &RTCPSender::BuildAPP;
  builders_[kRtcpTmmbr] = &RTCPSender::BuildTMMBR;
  builders_[kRtcpTmmbn] = &RTCPSender::BuildTMMBN;
  builders_[kRtcpNack] = &RTCPSender::BuildNACK;
  builders_[kRtcpXrVoipMetric] = &RTCPSender::BuildVoIPMetric;
  builders_[kRtcpXrReceiverReferenceTime] =
      &RTCPSender::BuildReceiverReferenceTime;
  builders_[kRtcpXrDlrrReportBlock] = &RTCPSender::BuildDlrr;
}

RTCPSender::~RTCPSender() {}

RtcpMode RTCPSender::Status() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return method_;
}

void RTCPSender::SetRTCPStatus(RtcpMode method) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  method_ = method;

  if (method == RtcpMode::kOff)
    return;
  next_time_to_send_rtcp_ =
    clock_->TimeInMilliseconds() + RTCP_INTERVAL_RAPID_SYNC_MS / 2;
}

bool RTCPSender::Sending() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return sending_;
}

int32_t RTCPSender::SetSendingStatus(const FeedbackState& feedback_state,
                                     bool sending) {
  bool sendRTCPBye = false;
  {
    CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

    if (method_ != RtcpMode::kOff) {
      if (sending == false && sending_ == true) {
        // Trigger RTCP bye
        sendRTCPBye = true;
      }
    }
    sending_ = sending;
  }
  if (sendRTCPBye)
    return SendRTCP(feedback_state, kRtcpBye);
  return 0;
}

bool RTCPSender::REMB() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return remb_enabled_;
}

void RTCPSender::SetREMBStatus(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  remb_enabled_ = enable;
}

void RTCPSender::SetREMBData(uint32_t bitrate,
                             const std::vector<uint32_t>& ssrcs) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  remb_bitrate_ = bitrate;
  remb_ssrcs_ = ssrcs;

  if (remb_enabled_)
    SetFlag(kRtcpRemb, false);
  // Send a REMB immediately if we have a new REMB. The frequency of REMBs is
  // throttled by the caller.
  next_time_to_send_rtcp_ = clock_->TimeInMilliseconds();
}

bool RTCPSender::TMMBR() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return IsFlagPresent(RTCPPacketType::kRtcpTmmbr);
}

void RTCPSender::SetTMMBRStatus(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  if (enable) {
    SetFlag(RTCPPacketType::kRtcpTmmbr, false);
  } else {
    ConsumeFlag(RTCPPacketType::kRtcpTmmbr, true);
  }
}

void RTCPSender::SetStartTimestamp(uint32_t start_timestamp) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  start_timestamp_ = start_timestamp;
}

void RTCPSender::SetLastRtpTime(uint32_t rtp_timestamp,
                                int64_t capture_time_ms) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  last_rtp_timestamp_ = rtp_timestamp;
  if (capture_time_ms < 0) {
    // We don't currently get a capture time from VoiceEngine.
    last_frame_capture_time_ms_ = clock_->TimeInMilliseconds();
  } else {
    last_frame_capture_time_ms_ = capture_time_ms;
  }
}

void RTCPSender::SetSSRC(uint32_t ssrc) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (ssrc_ != 0) {
    // not first SetSSRC, probably due to a collision
    // schedule a new RTCP report
    // make sure that we send a RTP packet
    next_time_to_send_rtcp_ = clock_->TimeInMilliseconds() + 100;
  }
  ssrc_ = ssrc;
}

void RTCPSender::SetRemoteSSRC(uint32_t ssrc) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  remote_ssrc_ = ssrc;
}

int32_t RTCPSender::SetCNAME(const char* c_name) {
  if (!c_name)
    return -1;

  RTC_DCHECK_LT(strlen(c_name), static_cast<size_t>(RTCP_CNAME_SIZE));
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  cname_ = c_name;
  return 0;
}

int32_t RTCPSender::AddMixedCNAME(uint32_t SSRC, const char* c_name) {
  assert(c_name);
  RTC_DCHECK_LT(strlen(c_name), static_cast<size_t>(RTCP_CNAME_SIZE));
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  if (csrc_cnames_.size() >= kRtpCsrcSize)
    return -1;

  csrc_cnames_[SSRC] = c_name;
  return 0;
}

int32_t RTCPSender::RemoveMixedCNAME(uint32_t SSRC) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  auto it = csrc_cnames_.find(SSRC);

  if (it == csrc_cnames_.end())
    return -1;

  csrc_cnames_.erase(it);
  return 0;
}

bool RTCPSender::TimeToSendRTCPReport(bool sendKeyframeBeforeRTP) const {
  /*
      For audio we use a fix 5 sec interval

      For video we use 1 sec interval fo a BW smaller than 360 kbit/s,
          technicaly we break the max 5% RTCP BW for video below 10 kbit/s but
          that should be extremely rare


  From RFC 3550

      MAX RTCP BW is 5% if the session BW
          A send report is approximately 65 bytes inc CNAME
          A receiver report is approximately 28 bytes

      The RECOMMENDED value for the reduced minimum in seconds is 360
        divided by the session bandwidth in kilobits/second.  This minimum
        is smaller than 5 seconds for bandwidths greater than 72 kb/s.

      If the participant has not yet sent an RTCP packet (the variable
        initial is true), the constant Tmin is set to 2.5 seconds, else it
        is set to 5 seconds.

      The interval between RTCP packets is varied randomly over the
        range [0.5,1.5] times the calculated interval to avoid unintended
        synchronization of all participants

      if we send
      If the participant is a sender (we_sent true), the constant C is
        set to the average RTCP packet size (avg_rtcp_size) divided by 25%
        of the RTCP bandwidth (rtcp_bw), and the constant n is set to the
        number of senders.

      if we receive only
        If we_sent is not true, the constant C is set
        to the average RTCP packet size divided by 75% of the RTCP
        bandwidth.  The constant n is set to the number of receivers
        (members - senders).  If the number of senders is greater than
        25%, senders and receivers are treated together.

      reconsideration NOT required for peer-to-peer
        "timer reconsideration" is
        employed.  This algorithm implements a simple back-off mechanism
        which causes users to hold back RTCP packet transmission if the
        group sizes are increasing.

        n = number of members
        C = avg_size/(rtcpBW/4)

     3. The deterministic calculated interval Td is set to max(Tmin, n*C).

     4. The calculated interval T is set to a number uniformly distributed
        between 0.5 and 1.5 times the deterministic calculated interval.

     5. The resulting value of T is divided by e-3/2=1.21828 to compensate
        for the fact that the timer reconsideration algorithm converges to
        a value of the RTCP bandwidth below the intended average
  */

  int64_t now = clock_->TimeInMilliseconds();

  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (method_ == RtcpMode::kOff)
    return false;

  if (!audio_ && sendKeyframeBeforeRTP) {
    // for video key-frames we want to send the RTCP before the large key-frame
    // if we have a 100 ms margin
    now += RTCP_SEND_BEFORE_KEY_FRAME_MS;
  }

  if (now >= next_time_to_send_rtcp_) {
    return true;
  } else if (now < 0x0000ffff &&
             next_time_to_send_rtcp_ > 0xffff0000) {  // 65 sec margin
    // wrap
    return true;
  }
  return false;
}

bool
RTCPSender::GetSendReportMetadata(const uint32_t sendReport,
                                  uint64_t *timeOfSend,
                                  uint32_t *packetCount,
                                  uint64_t *octetCount)
{
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  // This is only saved when we are the sender
  if ((last_send_report_[0] == 0) || (sendReport == 0)) {
    return false;
  } else {
    for (int i = 0; i < RTCP_NUMBER_OF_SR; ++i) {
      if (last_send_report_[i] == sendReport) {
        *timeOfSend = last_rtcp_time_[i];
        *packetCount = lastSRPacketCount_[i];
        *octetCount = lastSROctetCount_[i];
        return true;
      }
    }
  }
  return false;
}

bool RTCPSender::SendTimeOfXrRrReport(uint32_t mid_ntp,
                                      int64_t* time_ms) const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (last_xr_rr_.empty()) {
    return false;
  }
  std::map<uint32_t, int64_t>::const_iterator it = last_xr_rr_.find(mid_ntp);
  if (it == last_xr_rr_.end()) {
    return false;
  }
  *time_ms = it->second;
  return true;
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildSR(const RtcpContext& ctx) {
  for (int i = (RTCP_NUMBER_OF_SR - 2); i >= 0; i--) {
    // shift old
    last_send_report_[i + 1] = last_send_report_[i];
    last_rtcp_time_[i + 1] = last_rtcp_time_[i];
    lastSRPacketCount_[i+1] = lastSRPacketCount_[i];
    lastSROctetCount_[i+1] = lastSROctetCount_[i];
  }

  last_rtcp_time_[0] = Clock::NtpToMs(ctx.ntp_sec_, ctx.ntp_frac_);
  last_send_report_[0] = (ctx.ntp_sec_ << 16) + (ctx.ntp_frac_ >> 16);
  lastSRPacketCount_[0] = ctx.feedback_state_.packets_sent;
  lastSROctetCount_[0] = ctx.feedback_state_.media_bytes_sent;

  // The timestamp of this RTCP packet should be estimated as the timestamp of
  // the frame being captured at this moment. We are calculating that
  // timestamp as the last frame's timestamp + the time since the last frame
  // was captured.
  uint32_t rtp_timestamp =
      start_timestamp_ + last_rtp_timestamp_ +
      (clock_->TimeInMilliseconds() - last_frame_capture_time_ms_) *
          (ctx.feedback_state_.frequency_hz / 1000);

  rtcp::SenderReport* report = new rtcp::SenderReport();
  report->From(ssrc_);
  report->WithNtpSec(ctx.ntp_sec_);
  report->WithNtpFrac(ctx.ntp_frac_);
  report->WithRtpTimestamp(rtp_timestamp);
  report->WithPacketCount(ctx.feedback_state_.packets_sent);
  report->WithOctetCount(ctx.feedback_state_.media_bytes_sent);

  for (auto it : report_blocks_)
    report->WithReportBlock(it.second);

  report_blocks_.clear();

  return rtc::scoped_ptr<rtcp::SenderReport>(report);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildSDES(
    const RtcpContext& ctx) {
  size_t length_cname = cname_.length();
  RTC_CHECK_LT(length_cname, static_cast<size_t>(RTCP_CNAME_SIZE));

  rtcp::Sdes* sdes = new rtcp::Sdes();
  sdes->WithCName(ssrc_, cname_);

  for (const auto it : csrc_cnames_)
    sdes->WithCName(it.first, it.second);

  return rtc::scoped_ptr<rtcp::Sdes>(sdes);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildRR(const RtcpContext& ctx) {
  rtcp::ReceiverReport* report = new rtcp::ReceiverReport();
  report->From(ssrc_);
  for (auto it : report_blocks_)
    report->WithReportBlock(it.second);

  report_blocks_.clear();
  return rtc::scoped_ptr<rtcp::ReceiverReport>(report);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildPLI(const RtcpContext& ctx) {
  rtcp::Pli* pli = new rtcp::Pli();
  pli->From(ssrc_);
  pli->To(remote_ssrc_);

  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::PLI");
  ++packet_type_counter_.pli_packets;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RTCP_PLICount",
                    ssrc_, packet_type_counter_.pli_packets);

  return rtc::scoped_ptr<rtcp::Pli>(pli);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildFIR(const RtcpContext& ctx) {
  if (!ctx.repeat_)
    ++sequence_number_fir_;  // Do not increase if repetition.

  rtcp::Fir* fir = new rtcp::Fir();
  fir->From(ssrc_);
  fir->To(remote_ssrc_);
  fir->WithCommandSeqNum(sequence_number_fir_);

  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::FIR");
  ++packet_type_counter_.fir_packets;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RTCP_FIRCount",
                    ssrc_, packet_type_counter_.fir_packets);

  return rtc::scoped_ptr<rtcp::Fir>(fir);
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            First        |        Number           | PictureID |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildSLI(const RtcpContext& ctx) {
  rtcp::Sli* sli = new rtcp::Sli();
  sli->From(ssrc_);
  sli->To(remote_ssrc_);
  // Crop picture id to 6 least significant bits.
  sli->WithPictureId(ctx.picture_id_ & 0x3F);

  return rtc::scoped_ptr<rtcp::Sli>(sli);
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native RPSI bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
/*
*    Note: not generic made for VP8
*/
rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildRPSI(
    const RtcpContext& ctx) {
  if (ctx.feedback_state_.send_payload_type == 0xFF)
    return nullptr;

  rtcp::Rpsi* rpsi = new rtcp::Rpsi();
  rpsi->From(ssrc_);
  rpsi->To(remote_ssrc_);
  rpsi->WithPayloadType(ctx.feedback_state_.send_payload_type);
  rpsi->WithPictureId(ctx.picture_id_);

  return rtc::scoped_ptr<rtcp::Rpsi>(rpsi);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildREMB(
    const RtcpContext& ctx) {
  rtcp::Remb* remb = new rtcp::Remb();
  remb->From(ssrc_);
  for (uint32_t ssrc : remb_ssrcs_)
    remb->AppliesTo(ssrc);
  remb->WithBitrateBps(remb_bitrate_);

  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::REMB");

  return rtc::scoped_ptr<rtcp::Remb>(remb);
}

void RTCPSender::SetTargetBitrate(unsigned int target_bitrate) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  tmmbr_send_ = target_bitrate / 1000;
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildTMMBR(
    const RtcpContext& ctx) {
  if (ctx.feedback_state_.module == nullptr)
    return nullptr;
  // Before sending the TMMBR check the received TMMBN, only an owner is
  // allowed to raise the bitrate:
  // * If the sender is an owner of the TMMBN -> send TMMBR
  // * If not an owner but the TMMBR would enter the TMMBN -> send TMMBR

  // get current bounding set from RTCP receiver
  bool tmmbrOwner = false;
  // store in candidateSet, allocates one extra slot
  TMMBRSet* candidateSet = tmmbr_help_.CandidateSet();

  // holding critical_section_rtcp_sender_ while calling RTCPreceiver which
  // will accuire criticalSectionRTCPReceiver_ is a potental deadlock but
  // since RTCPreceiver is not doing the reverse we should be fine
  int32_t lengthOfBoundingSet =
      ctx.feedback_state_.module->BoundingSet(&tmmbrOwner, candidateSet);

  if (lengthOfBoundingSet > 0) {
    for (int32_t i = 0; i < lengthOfBoundingSet; i++) {
      if (candidateSet->Tmmbr(i) == tmmbr_send_ &&
          candidateSet->PacketOH(i) == packet_oh_send_) {
        // Do not send the same tuple.
        return nullptr;
      }
    }
    if (!tmmbrOwner) {
      // use received bounding set as candidate set
      // add current tuple
      candidateSet->SetEntry(lengthOfBoundingSet, tmmbr_send_, packet_oh_send_,
                             ssrc_);
      int numCandidates = lengthOfBoundingSet + 1;

      // find bounding set
      TMMBRSet* boundingSet = nullptr;
      int numBoundingSet = tmmbr_help_.FindTMMBRBoundingSet(boundingSet);
      if (numBoundingSet > 0 || numBoundingSet <= numCandidates)
        tmmbrOwner = tmmbr_help_.IsOwner(ssrc_, numBoundingSet);
      if (!tmmbrOwner) {
        // Did not enter bounding set, no meaning to send this request.
        return nullptr;
      }
    }
  }

  if (!tmmbr_send_)
    return nullptr;

  rtcp::Tmmbr* tmmbr = new rtcp::Tmmbr();
  tmmbr->From(ssrc_);
  tmmbr->To(remote_ssrc_);
  tmmbr->WithBitrateKbps(tmmbr_send_);
  tmmbr->WithOverhead(packet_oh_send_);

  return rtc::scoped_ptr<rtcp::Tmmbr>(tmmbr);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildTMMBN(
    const RtcpContext& ctx) {
  TMMBRSet* boundingSet = tmmbr_help_.BoundingSetToSend();
  if (boundingSet == nullptr)
    return nullptr;

  rtcp::Tmmbn* tmmbn = new rtcp::Tmmbn();
  tmmbn->From(ssrc_);
  for (uint32_t i = 0; i < boundingSet->lengthOfSet(); i++) {
    if (boundingSet->Tmmbr(i) > 0) {
      tmmbn->WithTmmbr(boundingSet->Ssrc(i), boundingSet->Tmmbr(i),
                       boundingSet->PacketOH(i));
    }
  }

  return rtc::scoped_ptr<rtcp::Tmmbn>(tmmbn);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildAPP(const RtcpContext& ctx) {
  rtcp::App* app = new rtcp::App();
  app->From(ssrc_);
  app->WithSubType(app_sub_type_);
  app->WithName(app_name_);
  app->WithData(app_data_.get(), app_length_);

  return rtc::scoped_ptr<rtcp::App>(app);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildNACK(
    const RtcpContext& ctx) {
  rtcp::Nack* nack = new rtcp::Nack();
  nack->From(ssrc_);
  nack->To(remote_ssrc_);
  nack->WithList(ctx.nack_list_, ctx.nack_size_);

  // Report stats.
  NACKStringBuilder stringBuilder;
  for (int idx = 0; idx < ctx.nack_size_; ++idx) {
    stringBuilder.PushNACK(ctx.nack_list_[idx]);
    nack_stats_.ReportRequest(ctx.nack_list_[idx]);
  }
  packet_type_counter_.nack_requests = nack_stats_.requests();
  packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "RTCPSender::NACK", "nacks",
                       TRACE_STR_COPY(stringBuilder.GetResult().c_str()));
  ++packet_type_counter_.nack_packets;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "RTCP_NACKCount",
                    ssrc_, packet_type_counter_.nack_packets);

  return rtc::scoped_ptr<rtcp::Nack>(nack);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildBYE(const RtcpContext& ctx) {
  rtcp::Bye* bye = new rtcp::Bye();
  bye->From(ssrc_);
  for (uint32_t csrc : csrcs_)
    bye->WithCsrc(csrc);

  return rtc::scoped_ptr<rtcp::Bye>(bye);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildReceiverReferenceTime(
    const RtcpContext& ctx) {
  if (last_xr_rr_.size() >= RTCP_NUMBER_OF_SR)
    last_xr_rr_.erase(last_xr_rr_.begin());
  last_xr_rr_.insert(std::pair<uint32_t, int64_t>(
      RTCPUtility::MidNtp(ctx.ntp_sec_, ctx.ntp_frac_),
      Clock::NtpToMs(ctx.ntp_sec_, ctx.ntp_frac_)));

  rtcp::Xr* xr = new rtcp::Xr();
  xr->From(ssrc_);

  rtcp::Rrtr rrtr;
  rrtr.WithNtp(NtpTime(ctx.ntp_sec_, ctx.ntp_frac_));

  xr->WithRrtr(&rrtr);

  // TODO(sprang): Merge XR report sending to contain all of RRTR, DLRR, VOIP?

  return rtc::scoped_ptr<rtcp::Xr>(xr);
}

rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildDlrr(
    const RtcpContext& ctx) {
  rtcp::Xr* xr = new rtcp::Xr();
  xr->From(ssrc_);

  rtcp::Dlrr dlrr;
  const RtcpReceiveTimeInfo& info = ctx.feedback_state_.last_xr_rr;
  dlrr.WithDlrrItem(info.sourceSSRC, info.lastRR, info.delaySinceLastRR);

  xr->WithDlrr(&dlrr);

  return rtc::scoped_ptr<rtcp::Xr>(xr);
}

// TODO(sprang): Add a unit test for this, or remove if the code isn't used.
rtc::scoped_ptr<rtcp::RtcpPacket> RTCPSender::BuildVoIPMetric(
    const RtcpContext& context) {
  rtcp::Xr* xr = new rtcp::Xr();
  xr->From(ssrc_);

  rtcp::VoipMetric voip;
  voip.To(remote_ssrc_);
  voip.WithVoipMetric(xr_voip_metric_);

  xr->WithVoipMetric(&voip);

  return rtc::scoped_ptr<rtcp::Xr>(xr);
}

int32_t RTCPSender::SendRTCP(const FeedbackState& feedback_state,
                             RTCPPacketType packetType,
                             int32_t nack_size,
                             const uint16_t* nack_list,
                             bool repeat,
                             uint64_t pictureID) {
  return SendCompoundRTCP(
      feedback_state, std::set<RTCPPacketType>(&packetType, &packetType + 1),
      nack_size, nack_list, repeat, pictureID);
}

int32_t RTCPSender::SendCompoundRTCP(
    const FeedbackState& feedback_state,
    const std::set<RTCPPacketType>& packet_types,
    int32_t nack_size,
    const uint16_t* nack_list,
    bool repeat,
    uint64_t pictureID) {
  PacketContainer container(transport_);
  {
    CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
    if (method_ == RtcpMode::kOff) {
      LOG(LS_WARNING) << "Can't send rtcp if it is disabled.";
      return -1;
    }

    // We need to send our NTP even if we haven't received any reports.
    uint32_t ntp_sec;
    uint32_t ntp_frac;
    clock_->CurrentNtp(ntp_sec, ntp_frac);
    RtcpContext context(feedback_state, nack_size, nack_list, repeat, pictureID,
                        ntp_sec, ntp_frac, &container);

    PrepareReport(packet_types, feedback_state);

    auto it = report_flags_.begin();
    while (it != report_flags_.end()) {
      auto builder_it = builders_.find(it->type);
      RTC_DCHECK(builder_it != builders_.end());
      if (it->is_volatile) {
        report_flags_.erase(it++);
      } else {
        ++it;
      }

      BuilderFunc func = builder_it->second;
      rtc::scoped_ptr<rtcp::RtcpPacket> packet = (this->*func)(context);
      if (packet.get() == nullptr)
        return -1;
      container.Append(packet.release());
    }

    if (packet_type_counter_observer_ != nullptr) {
      packet_type_counter_observer_->RtcpPacketTypesCounterUpdated(
          remote_ssrc_, packet_type_counter_);
    }

    RTC_DCHECK(AllVolatileFlagsConsumed());
  }

  size_t bytes_sent = container.SendPackets();
  return bytes_sent == 0 ? -1 : 0;
}

void RTCPSender::PrepareReport(const std::set<RTCPPacketType>& packetTypes,
                               const FeedbackState& feedback_state) {
  // Add all flags as volatile. Non volatile entries will not be overwritten
  // and all new volatile flags added will be consumed by the end of this call.
  SetFlags(packetTypes, true);

  if (packet_type_counter_.first_packet_time_ms == -1)
    packet_type_counter_.first_packet_time_ms = clock_->TimeInMilliseconds();

  bool generate_report;
  if (IsFlagPresent(kRtcpSr) || IsFlagPresent(kRtcpRr)) {
    // Report type already explicitly set, don't automatically populate.
    generate_report = true;
    RTC_DCHECK(ConsumeFlag(kRtcpReport) == false);
  } else {
    generate_report =
        (ConsumeFlag(kRtcpReport) && method_ == RtcpMode::kReducedSize) ||
        method_ == RtcpMode::kCompound;
    if (generate_report)
      SetFlag(sending_ ? kRtcpSr : kRtcpRr, true);
  }

  if (IsFlagPresent(kRtcpSr) || (IsFlagPresent(kRtcpRr) && !cname_.empty()))
    SetFlag(kRtcpSdes, true);

  if (generate_report) {
    if (!sending_ && xr_send_receiver_reference_time_enabled_)
      SetFlag(kRtcpXrReceiverReferenceTime, true);
    if (feedback_state.has_last_xr_rr)
      SetFlag(kRtcpXrDlrrReportBlock, true);

    // generate next time to send an RTCP report
    uint32_t minIntervalMs = RTCP_INTERVAL_AUDIO_MS;

    if (!audio_) {
      if (sending_) {
        // Calculate bandwidth for video; 360 / send bandwidth in kbit/s.
        uint32_t send_bitrate_kbit = feedback_state.send_bitrate / 1000;
        if (send_bitrate_kbit != 0)
          minIntervalMs = 360000 / send_bitrate_kbit;
      }
      if (minIntervalMs > RTCP_INTERVAL_VIDEO_MS)
        minIntervalMs = RTCP_INTERVAL_VIDEO_MS;
    }
    // The interval between RTCP packets is varied randomly over the
    // range [1/2,3/2] times the calculated interval.
    uint32_t timeToNext =
        random_.Rand(minIntervalMs * 1 / 2, minIntervalMs * 3 / 2);
    next_time_to_send_rtcp_ = clock_->TimeInMilliseconds() + timeToNext;

    StatisticianMap statisticians =
        receive_statistics_->GetActiveStatisticians();
    RTC_DCHECK(report_blocks_.empty());
    for (auto& it : statisticians) {
      AddReportBlock(feedback_state, it.first, it.second);
    }
  }
}

bool RTCPSender::AddReportBlock(const FeedbackState& feedback_state,
                                uint32_t ssrc,
                                StreamStatistician* statistician) {
  // Do we have receive statistics to send?
  RtcpStatistics stats;
  if (!statistician->GetStatistics(&stats, true))
    return false;

  if (report_blocks_.size() >= RTCP_MAX_REPORT_BLOCKS) {
    LOG(LS_WARNING) << "Too many report blocks.";
    return false;
  }
  RTC_DCHECK(report_blocks_.find(ssrc) == report_blocks_.end());
  rtcp::ReportBlock* block = &report_blocks_[ssrc];
  block->To(ssrc);
  block->WithFractionLost(stats.fraction_lost);
  if (!block->WithCumulativeLost(stats.cumulative_lost)) {
    report_blocks_.erase(ssrc);
    LOG(LS_WARNING) << "Cumulative lost is oversized.";
    return false;
  }
  block->WithExtHighestSeqNum(stats.extended_max_sequence_number);
  block->WithJitter(stats.jitter);
  block->WithLastSr(feedback_state.remote_sr);

  // TODO(sprang): Do we really need separate time stamps for each report?
  // Get our NTP as late as possible to avoid a race.
  uint32_t ntp_secs;
  uint32_t ntp_frac;
  clock_->CurrentNtp(ntp_secs, ntp_frac);

  // Delay since last received report.
  if ((feedback_state.last_rr_ntp_secs != 0) ||
      (feedback_state.last_rr_ntp_frac != 0)) {
    // Get the 16 lowest bits of seconds and the 16 highest bits of fractions.
    uint32_t now = ntp_secs & 0x0000FFFF;
    now <<= 16;
    now += (ntp_frac & 0xffff0000) >> 16;

    uint32_t receiveTime = feedback_state.last_rr_ntp_secs & 0x0000FFFF;
    receiveTime <<= 16;
    receiveTime += (feedback_state.last_rr_ntp_frac & 0xffff0000) >> 16;

    block->WithDelayLastSr(now - receiveTime);
  }
  return true;
}

void RTCPSender::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  assert(csrcs.size() <= kRtpCsrcSize);
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  csrcs_ = csrcs;
}

int32_t RTCPSender::SetApplicationSpecificData(uint8_t subType,
                                               uint32_t name,
                                               const uint8_t* data,
                                               uint16_t length) {
  if (length % 4 != 0) {
    LOG(LS_ERROR) << "Failed to SetApplicationSpecificData.";
    return -1;
  }
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  SetFlag(kRtcpApp, true);
  app_sub_type_ = subType;
  app_name_ = name;
  app_data_.reset(new uint8_t[length]);
  app_length_ = length;
  memcpy(app_data_.get(), data, length);
  return 0;
}

int32_t RTCPSender::SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  memcpy(&xr_voip_metric_, VoIPMetric, sizeof(RTCPVoIPMetric));

  SetFlag(kRtcpXrVoipMetric, true);
  return 0;
}

void RTCPSender::SendRtcpXrReceiverReferenceTime(bool enable) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  xr_send_receiver_reference_time_enabled_ = enable;
}

bool RTCPSender::RtcpXrReceiverReferenceTime() const {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());
  return xr_send_receiver_reference_time_enabled_;
}

// no callbacks allowed inside this function
int32_t RTCPSender::SetTMMBN(const TMMBRSet* boundingSet,
                             uint32_t maxBitrateKbit) {
  CriticalSectionScoped lock(critical_section_rtcp_sender_.get());

  if (0 == tmmbr_help_.SetTMMBRBoundingSetToSend(boundingSet, maxBitrateKbit)) {
    SetFlag(kRtcpTmmbn, true);
    return 0;
  }
  return -1;
}

void RTCPSender::SetFlag(RTCPPacketType type, bool is_volatile) {
  report_flags_.insert(ReportFlag(type, is_volatile));
}

void RTCPSender::SetFlags(const std::set<RTCPPacketType>& types,
                          bool is_volatile) {
  for (RTCPPacketType type : types)
    SetFlag(type, is_volatile);
}

bool RTCPSender::IsFlagPresent(RTCPPacketType type) const {
  return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
}

bool RTCPSender::ConsumeFlag(RTCPPacketType type, bool forced) {
  auto it = report_flags_.find(ReportFlag(type, false));
  if (it == report_flags_.end())
    return false;
  if (it->is_volatile || forced)
    report_flags_.erase((it));
  return true;
}

bool RTCPSender::AllVolatileFlagsConsumed() const {
  for (const ReportFlag& flag : report_flags_) {
    if (flag.is_volatile)
      return false;
  }
  return true;
}

bool RTCPSender::SendFeedbackPacket(const rtcp::TransportFeedback& packet) {
  class Sender : public rtcp::RtcpPacket::PacketReadyCallback {
   public:
    explicit Sender(Transport* transport)
        : transport_(transport), send_failure_(false) {}

    void OnPacketReady(uint8_t* data, size_t length) override {
      if (!transport_->SendRtcp(data, length))
        send_failure_ = true;
    }

    Transport* const transport_;
    bool send_failure_;
  } sender(transport_);

  uint8_t buffer[IP_PACKET_SIZE];
  return packet.BuildExternalBuffer(buffer, IP_PACKET_SIZE, &sender) &&
         !sender.send_failure_;
}

}  // namespace webrtc
