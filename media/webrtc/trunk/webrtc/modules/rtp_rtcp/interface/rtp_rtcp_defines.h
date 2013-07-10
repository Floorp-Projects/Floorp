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

#include "typedefs.h"
#include "module_common_types.h"
#include "webrtc/system_wrappers/interface/clock.h"

#ifndef NULL
    #define NULL    0
#endif

#define RTCP_CNAME_SIZE 256    // RFC 3550 page 44, including null termination
#define IP_PACKET_SIZE 1500    // we assume ethernet
#define MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS 10
#define TIMEOUT_SEI_MESSAGES_MS 30000   // in milliseconds

namespace webrtc{

const int32_t kDefaultVideoFrequency = 90000;

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

enum StorageType {
  kDontStore,
  kDontRetransmit,
  kAllowRetransmission
};

enum RTPExtensionType
{
   kRtpExtensionNone,
   kRtpExtensionTransmissionTimeOffset,
   kRtpExtensionAudioLevel,
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
    kRtcpTransmissionTimeOffset = 0x20000
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
  kRtxOff = 0,
  kRtxRetransmitted = 1,  // Apply RTX only to retransmitted packets.
  kRtxAll = 2  // Apply RTX to all packets (source + retransmissions).
};

const int kRtxHeaderSize = 2;

struct RTCPSenderInfo
{
    uint32_t NTPseconds;
    uint32_t NTPfraction;
    uint32_t RTPtimeStamp;
    uint32_t sendPacketCount;
    uint32_t sendOctetCount;
};

struct RTCPReportBlock
{
  // Fields as described by RFC 3550 6.4.2.
    uint32_t remoteSSRC;  // SSRC of sender of this report.
    uint32_t sourceSSRC;  // SSRC of the RTP packet sender.
    uint8_t fractionLost;
    uint32_t cumulativeLost;  // 24 bits valid
    uint32_t extendedHighSeqNum;
    uint32_t jitter;
    uint32_t lastSR;
    uint32_t delaySinceLastSR;
};

class RtpData
{
public:
    virtual int32_t OnReceivedPayloadData(
        const uint8_t* payloadData,
        const uint16_t payloadSize,
        const WebRtcRTPHeader* rtpHeader) = 0;
protected:
    virtual ~RtpData() {}
};

class RtcpFeedback
{
public:
    virtual void OnApplicationDataReceived(const int32_t /*id*/,
                                           const uint8_t /*subType*/,
                                           const uint32_t /*name*/,
                                           const uint16_t /*length*/,
                                           const uint8_t* /*data*/)  {};

    virtual void OnXRVoIPMetricReceived(
        const int32_t /*id*/,
        const RTCPVoIPMetric* /*metric*/)  {};

    virtual void OnRTCPPacketTimeout(const int32_t /*id*/)  {};

    // |ntp_secs|, |ntp_frac| and |timestamp| are the NTP time and RTP timestamp
    // parsed from the RTCP sender report from the sender with ssrc
    // |senderSSRC|.
    virtual void OnSendReportReceived(const int32_t id,
                                      const uint32_t senderSSRC,
                                      uint32_t ntp_secs,
                                      uint32_t ntp_frac,
                                      uint32_t timestamp)  {};

    virtual void OnReceiveReportReceived(const int32_t id,
                                         const uint32_t senderSSRC)  {};

protected:
    virtual ~RtcpFeedback() {}
};

class RtpFeedback
{
public:
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

    virtual void OnPacketTimeout(const int32_t id) = 0;

    virtual void OnReceivedPacket(const int32_t id,
                                  const RtpRtcpPacketType packetType) = 0;

    virtual void OnPeriodicDeadOrAlive(const int32_t id,
                                       const RTPAliveType alive) = 0;

    virtual void OnIncomingSSRCChanged( const int32_t id,
                                        const uint32_t SSRC) = 0;

    virtual void OnIncomingCSRCChanged( const int32_t id,
                                        const uint32_t CSRC,
                                        const bool added) = 0;

protected:
    virtual ~RtpFeedback() {}
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
  virtual void OnReceivedEstimatedBitrate(const uint32_t bitrate) = 0;

  virtual void OnReceivedRtcpReceiverReport(
      const uint32_t ssrc,
      const uint8_t fraction_loss,
      const uint32_t rtt,
      const uint32_t last_received_extended_high_seqNum,
      const uint32_t now_ms) = 0;

  virtual ~RtcpBandwidthObserver() {}
};

class RtcpRttObserver {
 public:
  virtual void OnRttUpdate(uint32_t rtt) = 0;

  virtual ~RtcpRttObserver() {};
};

// Null object version of RtpFeedback.
class NullRtpFeedback : public RtpFeedback {
 public:
  virtual ~NullRtpFeedback() {}

  virtual int32_t OnInitializeDecoder(
      const int32_t id,
      const int8_t payloadType,
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const uint8_t channels,
      const uint32_t rate) {
   return 0;
 }

 virtual void OnPacketTimeout(const int32_t id) {}

 virtual void OnReceivedPacket(const int32_t id,
                               const RtpRtcpPacketType packetType) {}

 virtual void OnPeriodicDeadOrAlive(const int32_t id,
                                    const RTPAliveType alive) {}

 virtual void OnIncomingSSRCChanged(const int32_t id,
                                    const uint32_t SSRC) {}

 virtual void OnIncomingCSRCChanged(const int32_t id,
                                    const uint32_t CSRC,
                                    const bool added) {}
};

// Null object version of RtpData.
class NullRtpData : public RtpData {
 public:
  virtual ~NullRtpData() {}
  virtual int32_t OnReceivedPayloadData(
      const uint8_t* payloadData,
      const uint16_t payloadSize,
      const WebRtcRTPHeader* rtpHeader) {
   return 0;
 }
};

// Null object version of RtpAudioFeedback.
class NullRtpAudioFeedback : public RtpAudioFeedback {
 public:
  virtual ~NullRtpAudioFeedback() {}

  virtual void OnPlayTelephoneEvent(const int32_t id,
                                    const uint8_t event,
                                    const uint16_t lengthMs,
                                    const uint8_t volume) {}
};

} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RTP_RTCP_DEFINES_H_
