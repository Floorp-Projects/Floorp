/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_DEFINES_H_
#define WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_DEFINES_H_

#include <stddef.h>
#include <list>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/typedefs.h"

#define RTCP_CNAME_SIZE 256    // RFC 3550 page 44, including null termination
#define IP_PACKET_SIZE 1500    // we assume ethernet
#define MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS 10
#define TIMEOUT_SEI_MESSAGES_MS 30000   // in milliseconds

namespace webrtc {

const int kVideoPayloadTypeFrequency = 90000;

// Minimum RTP header size in bytes.
const uint8_t kRtpHeaderSize = 12;

struct AudioPayload
{
    uint32_t    frequency;
    uint8_t     channels;
    uint32_t    rate;
};

struct VideoPayload
{
    RtpVideoCodecTypes   videoCodecType;
    uint32_t       maxRate;
};

union PayloadUnion
{
    AudioPayload Audio;
    VideoPayload Video;
};

enum RTCPMethod
{
    kRtcpOff          = 0,
    kRtcpCompound     = 1,
    kRtcpNonCompound = 2
};

enum RTPAliveType
{
    kRtpDead   = 0,
    kRtpNoRtp = 1,
    kRtpAlive  = 2
};

enum ProtectionType {
  kUnprotectedPacket,
  kProtectedPacket
};

enum StorageType {
  kDontStore,
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
  kRtpExtensionRID,
};

enum RTCPAppSubTypes
{
    kAppSubtypeBwe     = 0x00
};

enum RTCPPacketType
{
    kRtcpReport         = 0x0001,
    kRtcpSr             = 0x0002,
    kRtcpRr             = 0x0004,
    kRtcpBye            = 0x0008,
    kRtcpPli            = 0x0010,
    kRtcpNack           = 0x0020,
    kRtcpFir            = 0x0040,
    kRtcpTmmbr          = 0x0080,
    kRtcpTmmbn          = 0x0100,
    kRtcpSrReq          = 0x0200,
    kRtcpXrVoipMetric   = 0x0400,
    kRtcpApp            = 0x0800,
    kRtcpSli            = 0x4000,
    kRtcpRpsi           = 0x8000,
    kRtcpRemb           = 0x10000,
    kRtcpTransmissionTimeOffset = 0x20000,
    kRtcpXrReceiverReferenceTime = 0x40000,
    kRtcpXrDlrrReportBlock = 0x80000
};

enum KeyFrameRequestMethod
{
    kKeyFrameReqFirRtp    = 1,
    kKeyFrameReqPliRtcp   = 2,
    kKeyFrameReqFirRtcp   = 3
};

enum RtpRtcpPacketType
{
    kPacketRtp        = 0,
    kPacketKeepAlive = 1
};

enum NACKMethod
{
    kNackOff      = 0,
    kNackRtcp     = 2
};

enum RetransmissionMode {
  kRetransmitOff          = 0x0,
  kRetransmitFECPackets   = 0x1,
  kRetransmitBaseLayer    = 0x2,
  kRetransmitHigherLayers = 0x4,
  kRetransmitAllPackets   = 0xFF
};

enum RtxMode {
  kRtxOff                 = 0x0,
  kRtxRetransmitted       = 0x1,  // Only send retransmissions over RTX.
  kRtxRedundantPayloads   = 0x2   // Preventively send redundant payloads
                                  // instead of padding.
};

const size_t kRtxHeaderSize = 2;

struct RTCPSenderInfo
{
    uint32_t NTPseconds;
    uint32_t NTPfraction;
    uint32_t RTPtimeStamp;
    uint32_t sendPacketCount;
    uint32_t sendOctetCount;
};

struct RTCPReportBlock {
  RTCPReportBlock()
      : remoteSSRC(0), sourceSSRC(0), fractionLost(0), cumulativeLost(0),
        extendedHighSeqNum(0), jitter(0), lastSR(0),
        delaySinceLastSR(0) {}

  RTCPReportBlock(uint32_t remote_ssrc,
                  uint32_t source_ssrc,
                  uint8_t fraction_lost,
                  uint32_t cumulative_lost,
                  uint32_t extended_high_sequence_number,
                  uint32_t jitter,
                  uint32_t last_sender_report,
                  uint32_t delay_since_last_sender_report)
      : remoteSSRC(remote_ssrc),
        sourceSSRC(source_ssrc),
        fractionLost(fraction_lost),
        cumulativeLost(cumulative_lost),
        extendedHighSeqNum(extended_high_sequence_number),
        jitter(jitter),
        lastSR(last_sender_report),
        delaySinceLastSR(delay_since_last_sender_report) {}

  // Fields as described by RFC 3550 6.4.2.
  uint32_t remoteSSRC;  // SSRC of sender of this report.
  uint32_t sourceSSRC;  // SSRC of the RTP packet sender.
  uint8_t fractionLost;
  uint32_t cumulativeLost;  // 24 bits valid.
  uint32_t extendedHighSeqNum;
  uint32_t jitter;
  uint32_t lastSR;
  uint32_t delaySinceLastSR;
};

struct RtcpReceiveTimeInfo {
  // Fields as described by RFC 3611 4.5.
  uint32_t sourceSSRC;
  uint32_t lastRR;
  uint32_t delaySinceLastRR;
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

class RtpData
{
public:
    virtual ~RtpData() {}

    virtual int32_t OnReceivedPayloadData(
        const uint8_t* payloadData,
        const size_t payloadSize,
        const WebRtcRTPHeader* rtpHeader) = 0;

    virtual bool OnRecoveredPacket(const uint8_t* packet,
                                   size_t packet_length) = 0;
};

class RtpFeedback
{
public:
    virtual ~RtpFeedback() {}

    // Receiving payload change or SSRC change. (return success!)
    /*
    *   channels    - number of channels in codec (1 = mono, 2 = stereo)
    */
    virtual int32_t OnInitializeDecoder(
        const int32_t id,
        const int8_t payloadType,
        const char payloadName[RTP_PAYLOAD_NAME_SIZE],
        const int frequency,
        const uint8_t channels,
        const uint32_t rate) = 0;

    virtual void OnIncomingSSRCChanged( const int32_t id,
                                        const uint32_t ssrc) = 0;

    virtual void OnIncomingCSRCChanged( const int32_t id,
                                        const uint32_t CSRC,
                                        const bool added) = 0;

    virtual void ResetStatistics(uint32_t ssrc) = 0;
};

class RtpAudioFeedback {
 public:

  virtual void OnPlayTelephoneEvent(const int32_t id,
                                    const uint8_t event,
                                    const uint16_t lengthMs,
                                    const uint8_t volume) = 0;
 protected:
  virtual ~RtpAudioFeedback() {}
};

class RtcpIntraFrameObserver {
 public:
  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) = 0;

  virtual void OnReceivedSLI(uint32_t ssrc,
                             uint8_t picture_id) = 0;

  virtual void OnReceivedRPSI(uint32_t ssrc,
                              uint64_t picture_id) = 0;

  virtual void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) = 0;

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

class RtcpRttStats {
 public:
  virtual void OnRttUpdate(int64_t rtt) = 0;

  virtual int64_t LastProcessedRtt() const = 0;

  virtual ~RtcpRttStats() {};
};

// Null object version of RtpFeedback.
class NullRtpFeedback : public RtpFeedback {
 public:
  virtual ~NullRtpFeedback() {}

  int32_t OnInitializeDecoder(const int32_t id,
                              const int8_t payloadType,
                              const char payloadName[RTP_PAYLOAD_NAME_SIZE],
                              const int frequency,
                              const uint8_t channels,
                              const uint32_t rate) override {
    return 0;
  }

  void OnIncomingSSRCChanged(const int32_t id, const uint32_t ssrc) override {}

  void OnIncomingCSRCChanged(const int32_t id,
                             const uint32_t CSRC,
                             const bool added) override {}

  void ResetStatistics(uint32_t ssrc) override {}
};

// Null object version of RtpData.
class NullRtpData : public RtpData {
 public:
  virtual ~NullRtpData() {}

  int32_t OnReceivedPayloadData(const uint8_t* payloadData,
                                const size_t payloadSize,
                                const WebRtcRTPHeader* rtpHeader) override {
    return 0;
  }

  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override {
    return true;
  }
};

// Null object version of RtpAudioFeedback.
class NullRtpAudioFeedback : public RtpAudioFeedback {
 public:
  virtual ~NullRtpAudioFeedback() {}

  void OnPlayTelephoneEvent(const int32_t id,
                            const uint8_t event,
                            const uint16_t lengthMs,
                            const uint8_t volume) override {}
};

}  // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_DEFINES_H_
