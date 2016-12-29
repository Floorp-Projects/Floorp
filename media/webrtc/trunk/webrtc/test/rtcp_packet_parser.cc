/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/rtcp_packet_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {
namespace test {

using namespace RTCPUtility;

RtcpPacketParser::RtcpPacketParser() {}

RtcpPacketParser::~RtcpPacketParser() {}

void RtcpPacketParser::Parse(const void *data, size_t len) {
  const uint8_t* packet = static_cast<const uint8_t*>(data);
  RTCPUtility::RTCPParserV2 parser(packet, len, true);
  EXPECT_TRUE(parser.IsValid());
  for (RTCPUtility::RTCPPacketTypes type = parser.Begin();
       type != RTCPPacketTypes::kInvalid; type = parser.Iterate()) {
    switch (type) {
      case RTCPPacketTypes::kSr:
        sender_report_.Set(parser.Packet().SR);
        break;
      case RTCPPacketTypes::kRr:
        receiver_report_.Set(parser.Packet().RR);
        break;
      case RTCPPacketTypes::kReportBlockItem:
        report_block_.Set(parser.Packet().ReportBlockItem);
        ++report_blocks_per_ssrc_[parser.Packet().ReportBlockItem.SSRC];
        break;
      case RTCPPacketTypes::kSdes:
        sdes_.Set();
        break;
      case RTCPPacketTypes::kSdesChunk:
        sdes_chunk_.Set(parser.Packet().CName);
        break;
      case RTCPPacketTypes::kBye:
        bye_.Set(parser.Packet().BYE);
        break;
      case RTCPPacketTypes::kApp:
        app_.Set(parser.Packet().APP);
        break;
      case RTCPPacketTypes::kAppItem:
        app_item_.Set(parser.Packet().APP);
        break;
      case RTCPPacketTypes::kExtendedIj:
        ij_.Set();
        break;
      case RTCPPacketTypes::kExtendedIjItem:
        ij_item_.Set(parser.Packet().ExtendedJitterReportItem);
        break;
      case RTCPPacketTypes::kPsfbPli:
        pli_.Set(parser.Packet().PLI);
        break;
      case RTCPPacketTypes::kPsfbSli:
        sli_.Set(parser.Packet().SLI);
        break;
      case RTCPPacketTypes::kPsfbSliItem:
        sli_item_.Set(parser.Packet().SLIItem);
        break;
      case RTCPPacketTypes::kPsfbRpsi:
        rpsi_.Set(parser.Packet().RPSI);
        break;
      case RTCPPacketTypes::kPsfbFir:
        fir_.Set(parser.Packet().FIR);
        break;
      case RTCPPacketTypes::kPsfbFirItem:
        fir_item_.Set(parser.Packet().FIRItem);
        break;
      case RTCPPacketTypes::kRtpfbNack:
        nack_.Set(parser.Packet().NACK);
        nack_item_.Clear();
        break;
      case RTCPPacketTypes::kRtpfbNackItem:
        nack_item_.Set(parser.Packet().NACKItem);
        break;
      case RTCPPacketTypes::kPsfbApp:
        psfb_app_.Set(parser.Packet().PSFBAPP);
        break;
      case RTCPPacketTypes::kPsfbRembItem:
        remb_item_.Set(parser.Packet().REMBItem);
        break;
      case RTCPPacketTypes::kRtpfbTmmbr:
        tmmbr_.Set(parser.Packet().TMMBR);
        break;
      case RTCPPacketTypes::kRtpfbTmmbrItem:
        tmmbr_item_.Set(parser.Packet().TMMBRItem);
        break;
      case RTCPPacketTypes::kRtpfbTmmbn:
        tmmbn_.Set(parser.Packet().TMMBN);
        tmmbn_items_.Clear();
        break;
      case RTCPPacketTypes::kRtpfbTmmbnItem:
        tmmbn_items_.Set(parser.Packet().TMMBNItem);
        break;
      case RTCPPacketTypes::kXrHeader:
        xr_header_.Set(parser.Packet().XR);
        dlrr_items_.Clear();
        break;
      case RTCPPacketTypes::kXrReceiverReferenceTime:
        rrtr_.Set(parser.Packet().XRReceiverReferenceTimeItem);
        break;
      case RTCPPacketTypes::kXrDlrrReportBlock:
        dlrr_.Set();
        break;
      case RTCPPacketTypes::kXrDlrrReportBlockItem:
        dlrr_items_.Set(parser.Packet().XRDLRRReportBlockItem);
        break;
      case RTCPPacketTypes::kXrVoipMetric:
        voip_metric_.Set(parser.Packet().XRVOIPMetricItem);
        break;
      default:
        break;
    }
  }
}

uint64_t Rpsi::PictureId() const {
  assert(num_packets_ > 0);
  uint16_t num_bytes = rpsi_.NumberOfValidBits / 8;
  assert(num_bytes > 0);
  uint64_t picture_id = 0;
  for (uint16_t i = 0; i < num_bytes - 1; ++i) {
    picture_id += (rpsi_.NativeBitString[i] & 0x7f);
    picture_id <<= 7;
  }
  picture_id += (rpsi_.NativeBitString[num_bytes - 1] & 0x7f);
  return picture_id;
}

}  // namespace test
}  // namespace webrtc
