/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rrtr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/voip_metric.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace rtcp {

static const int kCommonFbFmtLength = 12;
static const int kReportBlockLength = 24;

class RawPacket;

// Class for building RTCP packets.
//
//  Example:
//  ReportBlock report_block;
//  report_block.To(234)
//  report_block.FractionLost(10);
//
//  ReceiverReport rr;
//  rr.From(123);
//  rr.WithReportBlock(&report_block)
//
//  Fir fir;
//  fir.From(123);
//  fir.To(234)
//  fir.WithCommandSeqNum(123);
//
//  size_t length = 0;                     // Builds an intra frame request
//  uint8_t packet[kPacketSize];           // with sequence number 123.
//  fir.Build(packet, &length, kPacketSize);
//
//  RawPacket packet = fir.Build();        // Returns a RawPacket holding
//                                         // the built rtcp packet.
//
//  rr.Append(&fir)                        // Builds a compound RTCP packet with
//  RawPacket packet = rr.Build();         // a receiver report, report block
//                                         // and fir message.

class RtcpPacket {
 public:
  virtual ~RtcpPacket() {}

  void Append(RtcpPacket* packet);

  // Callback used to signal that an RTCP packet is ready. Note that this may
  // not contain all data in this RtcpPacket; if a packet cannot fit in
  // max_length bytes, it will be fragmented and multiple calls to this
  // callback will be made.
  class PacketReadyCallback {
   public:
    PacketReadyCallback() {}
    virtual ~PacketReadyCallback() {}

    virtual void OnPacketReady(uint8_t* data, size_t length) = 0;
  };

  // Convenience method mostly used for test. Max length of IP_PACKET_SIZE is
  // used, will cause assertion error if fragmentation occurs.
  rtc::scoped_ptr<RawPacket> Build() const;

  // Returns true if all calls to Create succeeded. A buffer of size
  // IP_PACKET_SIZE will be allocated and reused between calls to callback.
  bool Build(PacketReadyCallback* callback) const;

  // Returns true if all calls to Create succeeded. Provided buffer reference
  // will be used for all calls to callback.
  bool BuildExternalBuffer(uint8_t* buffer,
                           size_t max_length,
                           PacketReadyCallback* callback) const;

  // Size of this packet in bytes (including headers, excluding nested packets).
  virtual size_t BlockLength() const = 0;

 protected:
  RtcpPacket() {}

  virtual bool Create(uint8_t* packet,
                      size_t* index,
                      size_t max_length,
                      PacketReadyCallback* callback) const = 0;

  static void CreateHeader(uint8_t count_or_format,
                           uint8_t packet_type,
                           size_t block_length,  // Size in 32bit words - 1.
                           uint8_t* buffer,
                           size_t* pos);

  bool OnBufferFull(uint8_t* packet,
                    size_t* index,
                    RtcpPacket::PacketReadyCallback* callback) const;

  size_t HeaderLength() const;

  static const size_t kHeaderLength = 4;
  std::vector<RtcpPacket*> appended_packets_;

 private:
  bool CreateAndAddAppended(uint8_t* packet,
                            size_t* index,
                            size_t max_length,
                            PacketReadyCallback* callback) const;
};

// TODO(sprang): Move RtcpPacket subclasses out to separate files.

// RTCP sender report (RFC 3550).
//
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=SR=200   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         SSRC of sender                        |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |              NTP timestamp, most significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |             NTP timestamp, least significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         RTP timestamp                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     sender's packet count                     |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      sender's octet count                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                         report block(s)                       |
//  |                            ....                               |

class SenderReport : public RtcpPacket {
 public:
  SenderReport() : RtcpPacket() {
    memset(&sr_, 0, sizeof(sr_));
  }

  virtual ~SenderReport() {}

  void From(uint32_t ssrc) {
    sr_.SenderSSRC = ssrc;
  }
  void WithNtpSec(uint32_t sec) {
    sr_.NTPMostSignificant = sec;
  }
  void WithNtpFrac(uint32_t frac) {
    sr_.NTPLeastSignificant = frac;
  }
  void WithRtpTimestamp(uint32_t rtp_timestamp) {
    sr_.RTPTimestamp = rtp_timestamp;
  }
  void WithPacketCount(uint32_t packet_count) {
    sr_.SenderPacketCount = packet_count;
  }
  void WithOctetCount(uint32_t octet_count) {
    sr_.SenderOctetCount = octet_count;
  }
  bool WithReportBlock(const ReportBlock& block);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfReportBlocks = 0x1f;

  size_t BlockLength() const {
    const size_t kSrHeaderLength = 8;
    const size_t kSenderInfoLength = 20;
    return kSrHeaderLength + kSenderInfoLength +
           report_blocks_.size() * kReportBlockLength;
  }

  RTCPUtility::RTCPPacketSR sr_;
  std::vector<ReportBlock> report_blocks_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SenderReport);
};

// Source Description (SDES) (RFC 3550).
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// header |V=2|P|    SC   |  PT=SDES=202  |             length            |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_1                          |
//   1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_2                          |
//   2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// Canonical End-Point Identifier SDES Item (CNAME)
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |    CNAME=1    |     length    | user and domain name        ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class Sdes : public RtcpPacket {
 public:
  Sdes() : RtcpPacket() {}

  virtual ~Sdes() {}

  bool WithCName(uint32_t ssrc, const std::string& cname);

  struct Chunk {
    uint32_t ssrc;
    std::string name;
    int null_octets;
  };

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfChunks = 0x1f;

  size_t BlockLength() const;

  std::vector<Chunk> chunks_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Sdes);
};

// Reference picture selection indication (RPSI) (RFC 4585).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |      PB       |0| Payload Type|    Native RPSI bit string     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   defined per codec          ...                | Padding (0) |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class Rpsi : public RtcpPacket {
 public:
  Rpsi()
      : RtcpPacket(),
        padding_bytes_(0) {
    memset(&rpsi_, 0, sizeof(rpsi_));
  }

  virtual ~Rpsi() {}

  void From(uint32_t ssrc) {
    rpsi_.SenderSSRC = ssrc;
  }
  void To(uint32_t ssrc) {
    rpsi_.MediaSSRC = ssrc;
  }
  void WithPayloadType(uint8_t payload) {
    assert(payload <= 0x7f);
    rpsi_.PayloadType = payload;
  }
  void WithPictureId(uint64_t picture_id);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const {
    size_t fci_length = 2 + (rpsi_.NumberOfValidBits / 8) + padding_bytes_;
    return kCommonFbFmtLength + fci_length;
  }

  uint8_t padding_bytes_;
  RTCPUtility::RTCPPacketPSFBRPSI rpsi_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Rpsi);
};

// Full intra request (FIR) (RFC 5104).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | Seq nr.       |    Reserved                                   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class Fir : public RtcpPacket {
 public:
  Fir() : RtcpPacket() {
    memset(&fir_, 0, sizeof(fir_));
    memset(&fir_item_, 0, sizeof(fir_item_));
  }

  virtual ~Fir() {}

  void From(uint32_t ssrc) {
    fir_.SenderSSRC = ssrc;
  }
  void To(uint32_t ssrc) {
    fir_item_.SSRC = ssrc;
  }
  void WithCommandSeqNum(uint8_t seq_num) {
    fir_item_.CommandSequenceNumber = seq_num;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const {
    const size_t kFciLength = 8;
    return kCommonFbFmtLength + kFciLength;
  }

  RTCPUtility::RTCPPacketPSFBFIR fir_;
  RTCPUtility::RTCPPacketPSFBFIRItem fir_item_;
};

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P| FMT=15  |   PT=206      |             length            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Unique identifier 'R' 'E' 'M' 'B'                            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   SSRC feedback                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ...

class Remb : public RtcpPacket {
 public:
  Remb() : RtcpPacket() {
    memset(&remb_, 0, sizeof(remb_));
    memset(&remb_item_, 0, sizeof(remb_item_));
  }

  virtual ~Remb() {}

  void From(uint32_t ssrc) {
    remb_.SenderSSRC = ssrc;
  }
  void AppliesTo(uint32_t ssrc);

  void WithBitrateBps(uint32_t bitrate_bps) {
    remb_item_.BitRate = bitrate_bps;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfSsrcs = 0xff;

  size_t BlockLength() const {
    return (remb_item_.NumberOfSSRCs + 5) * 4;
  }

  RTCPUtility::RTCPPacketPSFBAPP remb_;
  RTCPUtility::RTCPPacketPSFBREMBItem remb_item_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Remb);
};

// From RFC 3611: RTP Control Protocol Extended Reports (RTCP XR).
//
// Format for XR packets:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|reserved |   PT=XR=207   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              SSRC                             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  :                         report blocks                         :
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class Xr : public RtcpPacket {
 public:
  typedef std::vector<RTCPUtility::RTCPPacketXRDLRRReportBlockItem> DlrrBlock;
  Xr() : RtcpPacket() {
    memset(&xr_header_, 0, sizeof(xr_header_));
  }

  virtual ~Xr() {}

  void From(uint32_t ssrc) {
    xr_header_.OriginatorSSRC = ssrc;
  }

  // Max 50 items of each of {Rrtr, Dlrr, VoipMetric} allowed per Xr.
  bool WithRrtr(Rrtr* rrtr);
  bool WithDlrr(Dlrr* dlrr);
  bool WithVoipMetric(VoipMetric* voip_metric);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfRrtrBlocks = 50;
  static const int kMaxNumberOfDlrrBlocks = 50;
  static const int kMaxNumberOfVoipMetricBlocks = 50;

  size_t BlockLength() const {
    const size_t kXrHeaderLength = 8;
    return kXrHeaderLength + RrtrLength() + DlrrLength() + VoipMetricLength();
  }

  size_t RrtrLength() const { return Rrtr::kLength * rrtr_blocks_.size(); }

  size_t DlrrLength() const;

  size_t VoipMetricLength() const {
    return VoipMetric::kLength * voip_metric_blocks_.size();
  }

  RTCPUtility::RTCPPacketXR xr_header_;
  std::vector<Rrtr> rrtr_blocks_;
  std::vector<Dlrr> dlrr_blocks_;
  std::vector<VoipMetric> voip_metric_blocks_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Xr);
};

// Class holding a RTCP packet.
//
// Takes a built rtcp packet.
//  RawPacket raw_packet(buffer, length);
//
// To access the raw packet:
//  raw_packet.Buffer();         - pointer to the raw packet
//  raw_packet.BufferLength();   - the length of the raw packet

class RawPacket {
 public:
  explicit RawPacket(size_t buffer_length);
  RawPacket(const uint8_t* packet, size_t packet_length);

  const uint8_t* Buffer() const;
  uint8_t* MutableBuffer();
  size_t BufferLength() const;
  size_t Length() const;
  void SetLength(size_t length);

 private:
  const size_t buffer_length_;
  size_t length_;
  rtc::scoped_ptr<uint8_t[]> buffer_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_H_
