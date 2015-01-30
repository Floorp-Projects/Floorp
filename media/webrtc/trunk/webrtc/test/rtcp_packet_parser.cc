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

namespace webrtc {
namespace test {

RtcpPacketParser::RtcpPacketParser() {}

RtcpPacketParser::~RtcpPacketParser() {}

void RtcpPacketParser::Parse(const void *data, int len) {
  const uint8_t* packet = static_cast<const uint8_t*>(data);
  RTCPUtility::RTCPParserV2 parser(packet, len, true);
  for (RTCPUtility::RTCPPacketTypes type = parser.Begin();
      type != RTCPUtility::kRtcpNotValidCode;
      type = parser.Iterate()) {
    switch (type) {
      case RTCPUtility::kRtcpSrCode:
        sender_report_.Set(parser.Packet().SR);
        break;
      case RTCPUtility::kRtcpRrCode:
        receiver_report_.Set(parser.Packet().RR);
        break;
      case RTCPUtility::kRtcpReportBlockItemCode:
        report_block_.Set(parser.Packet().ReportBlockItem);
        ++report_blocks_per_ssrc_[parser.Packet().ReportBlockItem.SSRC];
        break;
      case RTCPUtility::kRtcpSdesCode:
        sdes_.Set();
        break;
      case RTCPUtility::kRtcpSdesChunkCode:
        sdes_chunk_.Set(parser.Packet().CName);
        break;
      case RTCPUtility::kRtcpByeCode:
        bye_.Set(parser.Packet().BYE);
        break;
      case RTCPUtility::kRtcpAppCode:
        app_.Set(parser.Packet().APP);
        break;
      case RTCPUtility::kRtcpAppItemCode:
        app_item_.Set(parser.Packet().APP);
        break;
      case RTCPUtility::kRtcpExtendedIjCode:
        ij_.Set();
        break;
      case RTCPUtility::kRtcpExtendedIjItemCode:
        ij_item_.Set(parser.Packet().ExtendedJitterReportItem);
        break;
      case RTCPUtility::kRtcpPsfbPliCode:
        pli_.Set(parser.Packet().PLI);
        break;
      case RTCPUtility::kRtcpPsfbSliCode:
        sli_.Set(parser.Packet().SLI);
        break;
      case RTCPUtility::kRtcpPsfbSliItemCode:
        sli_item_.Set(parser.Packet().SLIItem);
        break;
      case RTCPUtility::kRtcpPsfbRpsiCode:
        rpsi_.Set(parser.Packet().RPSI);
        break;
      case RTCPUtility::kRtcpPsfbFirCode:
        fir_.Set(parser.Packet().FIR);
        break;
      case RTCPUtility::kRtcpPsfbFirItemCode:
        fir_item_.Set(parser.Packet().FIRItem);
        break;
      case RTCPUtility::kRtcpRtpfbNackCode:
        nack_.Set(parser.Packet().NACK);
        nack_item_.Clear();
        break;
      case RTCPUtility::kRtcpRtpfbNackItemCode:
        nack_item_.Set(parser.Packet().NACKItem);
        break;
      case RTCPUtility::kRtcpPsfbAppCode:
        psfb_app_.Set(parser.Packet().PSFBAPP);
        break;
      case RTCPUtility::kRtcpPsfbRembItemCode:
        remb_item_.Set(parser.Packet().REMBItem);
        break;
      case RTCPUtility::kRtcpRtpfbTmmbrCode:
        tmmbr_.Set(parser.Packet().TMMBR);
        break;
      case RTCPUtility::kRtcpRtpfbTmmbrItemCode:
        tmmbr_item_.Set(parser.Packet().TMMBRItem);
        break;
      case RTCPUtility::kRtcpRtpfbTmmbnCode:
        tmmbn_.Set(parser.Packet().TMMBN);
        tmmbn_items_.Clear();
        break;
      case RTCPUtility::kRtcpRtpfbTmmbnItemCode:
        tmmbn_items_.Set(parser.Packet().TMMBNItem);
        break;
      case RTCPUtility::kRtcpXrHeaderCode:
        xr_header_.Set(parser.Packet().XR);
        dlrr_items_.Clear();
        break;
      case RTCPUtility::kRtcpXrReceiverReferenceTimeCode:
        rrtr_.Set(parser.Packet().XRReceiverReferenceTimeItem);
        break;
      case RTCPUtility::kRtcpXrDlrrReportBlockCode:
        dlrr_.Set();
        break;
      case RTCPUtility::kRtcpXrDlrrReportBlockItemCode:
        dlrr_items_.Set(parser.Packet().XRDLRRReportBlockItem);
        break;
      case RTCPUtility::kRtcpXrVoipMetricCode:
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
