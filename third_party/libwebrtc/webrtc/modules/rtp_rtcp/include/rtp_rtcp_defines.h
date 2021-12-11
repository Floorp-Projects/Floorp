/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_

#include <stddef.h>
#include <list>
#include <vector>

#include "api/audio_codecs/audio_format.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "rtc_base/deprecation.h"
#include "system_wrappers/include/clock.h"
#include "typedefs.h"  // NOLINT(build/include)

#define RTCP_CNAME_SIZE 256    // RFC 3550 page 44, including null termination
#define IP_PACKET_SIZE 1500    // we assume ethernet
#define MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS 10

namespace webrtc {
namespace rtcp {
class TransportFeedback;
}

const int kVideoPayloadTypeFrequency = 90000;
// TODO(solenberg): RTP time stamp rate for RTCP is fixed at 8k, this is legacy
// and should be fixed.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=6458
const int kBogusRtpRateForAudioRtcp = 8000;

// Minimum RTP header size in bytes.
const uint8_t kRtpHeaderSize = 12;

struct AudioPayload {
  SdpAudioFormat format;
  uint32_t rate;
};

struct VideoPayload {
  RtpVideoCodecTypes videoCodecType;
  // The H264 profile only matters if videoCodecType == kRtpVideoH264.
  H264::Profile h264_profile;
};

class PayloadUnion {
 public:
  explicit PayloadUnion(const AudioPayload& payload);
  explicit PayloadUnion(const VideoPayload& payload);
  PayloadUnion(const PayloadUnion&);
  PayloadUnion(PayloadUnion&&);
  ~PayloadUnion();

  PayloadUnion& operator=(const PayloadUnion&);
  PayloadUnion& operator=(PayloadUnion&&);

  bool is_audio() const { return audio_payload_.has_value(); }
  bool is_video() const { return video_payload_.has_value(); }
  const AudioPayload& audio_payload() const {
    RTC_DCHECK(audio_payload_);
    return *audio_payload_;
  }
  const VideoPayload& video_payload() const {
    RTC_DCHECK(video_payload_);
    return *video_payload_;
  }
  AudioPayload& audio_payload() {
    RTC_DCHECK(audio_payload_);
    return *audio_payload_;
  }
  VideoPayload& video_payload() {
    RTC_DCHECK(video_payload_);
    return *video_payload_;
  }

 private:
  rtc::Optional<AudioPayload> audio_payload_;
  rtc::Optional<VideoPayload> video_payload_;
};

enum RTPAliveType { kRtpDead = 0, kRtpNoRtp = 1, kRtpAlive = 2 };

enum ProtectionType {
  kUnprotectedPacket,
  kProtectedPacket
};

enum StorageType {
  kDontRetransmit,
  kAllowRetransmission
};

enum RTPExtensionType {
  kRtpExtensionNone,
  kRtpExtensionTransmissionTimeOffset,
  kRtpExtensionAudioLevel,
  kRtpExtensionAbsoluteSendTime,
  kRtpExtensionVideoRotation,
  kRtpExtensionTransportSequenceNumber,
  kRtpExtensionPlayoutDelay,
  kRtpExtensionVideoContentType,
  kRtpExtensionVideoTiming,
  kRtpExtensionRtpStreamId,
  kRtpExtensionRepairedRtpStreamId,
  kRtpExtensionMid,
  kRtpExtensionCsrcAudioLevel,
  kRtpExtensionNumberOfExtensions  // Must be the last entity in the enum.
};

enum RTCPAppSubTypes { kAppSubtypeBwe = 0x00 };

// TODO(sprang): Make this an enum class once rtcp_receiver has been cleaned up.
enum RTCPPacketType : uint32_t {
  kRtcpReport = 0x0001,
  kRtcpSr = 0x0002,
  kRtcpRr = 0x0004,
  kRtcpSdes = 0x0008,
  kRtcpBye = 0x0010,
  kRtcpPli = 0x0020,
  kRtcpNack = 0x0040,
  kRtcpFir = 0x0080,
  kRtcpTmmbr = 0x0100,
  kRtcpTmmbn = 0x0200,
  kRtcpSrReq = 0x0400,
  kRtcpXrVoipMetric = 0x0800,
  kRtcpApp = 0x1000,
  kRtcpRemb = 0x10000,
  kRtcpTransmissionTimeOffset = 0x20000,
  kRtcpXrReceiverReferenceTime = 0x40000,
  kRtcpXrDlrrReportBlock = 0x80000,
  kRtcpTransportFeedback = 0x100000,
  kRtcpXrTargetBitrate = 0x200000
};

enum KeyFrameRequestMethod { kKeyFrameReqPliRtcp, kKeyFrameReqFirRtcp };

enum RtpRtcpPacketType { kPacketRtp = 0, kPacketKeepAlive = 1 };

// kConditionallyRetransmitHigherLayers allows retransmission of video frames
// in higher layers if either the last frame in that layer was too far back in
// time, or if we estimate that a new frame will be available in a lower layer
// in a shorter time than it would take to request and receive a retransmission.
enum RetransmissionMode : uint8_t {
  kRetransmitOff = 0x0,
  kRetransmitFECPackets = 0x1,
  kRetransmitBaseLayer = 0x2,
  kRetransmitHigherLayers = 0x4,
  kConditionallyRetransmitHigherLayers = 0x8,
  kRetransmitAllPackets = 0xFF
};

enum RtxMode {
  kRtxOff                 = 0x0,
  kRtxRetransmitted       = 0x1,  // Only send retransmissions over RTX.
  kRtxRedundantPayloads   = 0x2   // Preventively send redundant payloads
                                  // instead of padding.
};

const size_t kRtxHeaderSize = 2;

struct RTCPReportBlock {
  RTCPReportBlock()
      : sender_ssrc(0),
        source_ssrc(0),
        fraction_lost(0),
        packets_lost(0),
        extended_highest_sequence_number(0),
        jitter(0),
        last_sender_report_timestamp(0),
        delay_since_last_sender_report(0) {}

  RTCPReportBlock(uint32_t sender_ssrc,
                  uint32_t source_ssrc,
                  uint8_t fraction_lost,
                  uint32_t packets_lost,
                  uint32_t extended_highest_sequence_number,
                  uint32_t jitter,
                  uint32_t last_sender_report_timestamp,
                  uint32_t delay_since_last_sender_report)
      : sender_ssrc(sender_ssrc),
        source_ssrc(source_ssrc),
        fraction_lost(fraction_lost),
        packets_lost(packets_lost),
        extended_highest_sequence_number(extended_highest_sequence_number),
        jitter(jitter),
        last_sender_report_timestamp(last_sender_report_timestamp),
        delay_since_last_sender_report(delay_since_last_sender_report) {}

  // Fields as described by RFC 3550 6.4.2.
  uint32_t sender_ssrc;  // SSRC of sender of this report.
  uint32_t source_ssrc;  // SSRC of the RTP packet sender.
  uint8_t fraction_lost;
  uint32_t packets_lost;  // 24 bits valid.
  uint32_t extended_highest_sequence_number;
  uint32_t jitter;
  uint32_t last_sender_report_timestamp;
  uint32_t delay_since_last_sender_report;
};

typedef std::list<RTCPReportBlock> ReportBlockList;

struct RtpState {
  RtpState()
      : sequence_number(0),
        start_timestamp(0),
        timestamp(0),
        capture_time_ms(-1),
        last_timestamp_time_ms(-1),
        media_has_been_sent(false) {}
  uint16_t sequence_number;
  uint32_t start_timestamp;
  uint32_t timestamp;
  int64_t capture_time_ms;
  int64_t last_timestamp_time_ms;
  bool media_has_been_sent;
};

class RtpData {
 public:
  virtual ~RtpData() {}

  virtual int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                        size_t payload_size,
                                        const WebRtcRTPHeader* rtp_header) = 0;
};

// Callback interface for packets recovered by FlexFEC or ULPFEC. In
// the FlexFEC case, the implementation should be able to demultiplex
// the recovered RTP packets based on SSRC.
class RecoveredPacketReceiver {
 public:
  virtual void OnRecoveredPacket(const uint8_t* packet, size_t length) = 0;

 protected:
  virtual ~RecoveredPacketReceiver() = default;
};

class RtpFeedback {
 public:
  virtual ~RtpFeedback() {}

  // Receiving payload change or SSRC change. (return success!)
  /*
  *   channels    - number of channels in codec (1 = mono, 2 = stereo)
  */
  virtual int32_t OnInitializeDecoder(int payload_type,
                                      const SdpAudioFormat& audio_format,
                                      uint32_t rate) = 0;

  virtual void OnIncomingSSRCChanged(uint32_t ssrc) = 0;

  virtual void OnIncomingCSRCChanged(uint32_t csrc, bool added) = 0;
};

class RtcpIntraFrameObserver {
 public:
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) = 0;

  RTC_DEPRECATED virtual void OnReceivedSLI(uint32_t ssrc,
                             uint8_t picture_id) {}

  RTC_DEPRECATED virtual void OnReceivedRPSI(uint32_t ssrc,
                              uint64_t picture_id) {}

  virtual ~RtcpIntraFrameObserver() {}
};

class RtcpBandwidthObserver {
 public:
  // REMB or TMMBR
  virtual void OnReceivedEstimatedBitrate(uint32_t bitrate) = 0;

  virtual void OnReceivedRtcpReceiverReport(
      const ReportBlockList& report_blocks,
      int64_t rtt,
      int64_t now_ms) = 0;

  virtual ~RtcpBandwidthObserver() {}
};

class RtcpEventObserver {
 public:
  virtual void OnRtcpBye() = 0;
  virtual void OnRtcpTimeout() = 0;

  virtual ~RtcpEventObserver() {}
};

struct PacketFeedback {
  PacketFeedback(int64_t arrival_time_ms, uint16_t sequence_number)
      : PacketFeedback(-1,
                       arrival_time_ms,
                       -1,
                       sequence_number,
                       0,
                       0,
                       0,
                       PacedPacketInfo()) {}

  PacketFeedback(int64_t arrival_time_ms,
                 int64_t send_time_ms,
                 uint16_t sequence_number,
                 size_t payload_size,
                 const PacedPacketInfo& pacing_info)
      : PacketFeedback(-1,
                       arrival_time_ms,
                       send_time_ms,
                       sequence_number,
                       payload_size,
                       0,
                       0,
                       pacing_info) {}

  PacketFeedback(int64_t creation_time_ms,
                 uint16_t sequence_number,
                 size_t payload_size,
                 uint16_t local_net_id,
                 uint16_t remote_net_id,
                 const PacedPacketInfo& pacing_info)
      : PacketFeedback(creation_time_ms,
                       -1,
                       -1,
                       sequence_number,
                       payload_size,
                       local_net_id,
                       remote_net_id,
                       pacing_info) {}

  PacketFeedback(int64_t creation_time_ms,
                 int64_t arrival_time_ms,
                 int64_t send_time_ms,
                 uint16_t sequence_number,
                 size_t payload_size,
                 uint16_t local_net_id,
                 uint16_t remote_net_id,
                 const PacedPacketInfo& pacing_info)
      : creation_time_ms(creation_time_ms),
        arrival_time_ms(arrival_time_ms),
        send_time_ms(send_time_ms),
        sequence_number(sequence_number),
        payload_size(payload_size),
        local_net_id(local_net_id),
        remote_net_id(remote_net_id),
        pacing_info(pacing_info) {}

  static constexpr int kNotAProbe = -1;
  static constexpr int64_t kNotReceived = -1;

  // NOTE! The variable |creation_time_ms| is not used when testing equality.
  //       This is due to |creation_time_ms| only being used by SendTimeHistory
  //       for book-keeping, and is of no interest outside that class.
  // TODO(philipel): Remove |creation_time_ms| from PacketFeedback when cleaning
  //                 up SendTimeHistory.
  bool operator==(const PacketFeedback& rhs) const {
    return arrival_time_ms == rhs.arrival_time_ms &&
           send_time_ms == rhs.send_time_ms &&
           sequence_number == rhs.sequence_number &&
           payload_size == rhs.payload_size && pacing_info == rhs.pacing_info;
  }

  // Time corresponding to when this object was created.
  int64_t creation_time_ms;
  // Time corresponding to when the packet was received. Timestamped with the
  // receiver's clock. For unreceived packet, the sentinel value kNotReceived
  // is used.
  int64_t arrival_time_ms;
  // Time corresponding to when the packet was sent, timestamped with the
  // sender's clock.
  int64_t send_time_ms;
  // Packet identifier, incremented with 1 for every packet generated by the
  // sender.
  uint16_t sequence_number;
  // Size of the packet excluding RTP headers.
  size_t payload_size;
  // The network route ids that this packet is associated with.
  uint16_t local_net_id;
  uint16_t remote_net_id;
  // Pacing information about this packet.
  PacedPacketInfo pacing_info;
};

class PacketFeedbackComparator {
 public:
  inline bool operator()(const PacketFeedback& lhs, const PacketFeedback& rhs) {
    if (lhs.arrival_time_ms != rhs.arrival_time_ms)
      return lhs.arrival_time_ms < rhs.arrival_time_ms;
    if (lhs.send_time_ms != rhs.send_time_ms)
      return lhs.send_time_ms < rhs.send_time_ms;
    return lhs.sequence_number < rhs.sequence_number;
  }
};

class TransportFeedbackObserver {
 public:
  TransportFeedbackObserver() {}
  virtual ~TransportFeedbackObserver() {}

  // Note: Transport-wide sequence number as sequence number.
  virtual void AddPacket(uint32_t ssrc,
                         uint16_t sequence_number,
                         size_t length,
                         const PacedPacketInfo& pacing_info) = 0;

  virtual void OnTransportFeedback(const rtcp::TransportFeedback& feedback) = 0;

  virtual std::vector<PacketFeedback> GetTransportFeedbackVector() const = 0;
};

class PacketFeedbackObserver {
 public:
  virtual ~PacketFeedbackObserver() = default;

  virtual void OnPacketAdded(uint32_t ssrc, uint16_t seq_num) = 0;
  virtual void OnPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector) = 0;
};

class RtcpRttStats {
 public:
  virtual void OnRttUpdate(int64_t rtt) = 0;

  virtual int64_t LastProcessedRtt() const = 0;

  virtual ~RtcpRttStats() {}
};

// Null object version of RtpFeedback.
class NullRtpFeedback : public RtpFeedback {
 public:
  ~NullRtpFeedback() override {}

  int32_t OnInitializeDecoder(int payload_type,
                              const SdpAudioFormat& audio_format,
                              uint32_t rate) override;

  void OnIncomingSSRCChanged(uint32_t ssrc) override {}
  void OnIncomingCSRCChanged(uint32_t csrc, bool added) override {}
};

inline int32_t NullRtpFeedback::OnInitializeDecoder(
    int payload_type,
    const SdpAudioFormat& audio_format,
    uint32_t rate) {
  return 0;
}

// Statistics about packet loss for a single directional connection. All values
// are totals since the connection initiated.
struct RtpPacketLossStats {
  // The number of packets lost in events where no adjacent packets were also
  // lost.
  uint64_t single_packet_loss_count;
  // The number of events in which more than one adjacent packet was lost.
  uint64_t multiple_packet_loss_event_count;
  // The number of packets lost in events where more than one adjacent packet
  // was lost.
  uint64_t multiple_packet_loss_packet_count;
};

class RtpPacketSender {
 public:
  RtpPacketSender() {}
  virtual ~RtpPacketSender() {}

  enum Priority {
    kHighPriority = 0,    // Pass through; will be sent immediately.
    kNormalPriority = 2,  // Put in back of the line.
    kLowPriority = 3,     // Put in back of the low priority line.
  };
  // Low priority packets are mixed with the normal priority packets
  // while we are paused.

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  virtual void InsertPacket(Priority priority,
                            uint32_t ssrc,
                            uint16_t sequence_number,
                            int64_t capture_time_ms,
                            size_t bytes,
                            bool retransmission) = 0;

  // Currently audio traffic is not accounted by pacer and passed through.
  // With the introduction of audio BWE audio traffic will be accounted for
  // the pacer budget calculation. The audio traffic still will be injected
  // at high priority.
  // TODO(alexnarest): Make it pure virtual after rtp_sender_unittest will be
  // updated to support it
  virtual void SetAccountForAudioPackets(bool account_for_audio) {}
};

class TransportSequenceNumberAllocator {
 public:
  TransportSequenceNumberAllocator() {}
  virtual ~TransportSequenceNumberAllocator() {}

  virtual uint16_t AllocateSequenceNumber() = 0;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_
