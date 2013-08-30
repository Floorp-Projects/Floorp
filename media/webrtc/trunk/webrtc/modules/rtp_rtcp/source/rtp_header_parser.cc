/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

class RtpHeaderParserImpl : public RtpHeaderParser {
 public:
  RtpHeaderParserImpl();
  virtual ~RtpHeaderParserImpl() {}

  virtual bool Parse(const uint8_t* packet, int length,
                     RTPHeader* header) const;

  virtual bool RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id);

  virtual bool DeregisterRtpHeaderExtension(RTPExtensionType type);

 private:
  scoped_ptr<CriticalSectionWrapper> critical_section_;
  RtpHeaderExtensionMap rtp_header_extension_map_;
};

RtpHeaderParser* RtpHeaderParser::Create() {
  return new RtpHeaderParserImpl;
}

RtpHeaderParserImpl::RtpHeaderParserImpl()
    : critical_section_(CriticalSectionWrapper::CreateCriticalSection()) {}

bool RtpHeaderParser::IsRtcp(const uint8_t* packet, int length) {
  ModuleRTPUtility::RTPHeaderParser rtp_parser(packet, length);
  return rtp_parser.RTCP();
}

bool RtpHeaderParserImpl::Parse(const uint8_t* packet, int length,
                            RTPHeader* header) const {
  ModuleRTPUtility::RTPHeaderParser rtp_parser(packet, length);
  memset(header, 0, sizeof(*header));

  RtpHeaderExtensionMap map;
  {
    CriticalSectionScoped cs(critical_section_.get());
    rtp_header_extension_map_.GetCopy(&map);
  }

  const bool valid_rtpheader = rtp_parser.Parse(*header, &map);
  if (!valid_rtpheader) {
    WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, -1,
                 "IncomingPacket invalid RTP header");
    return false;
  }
  return true;
}

bool RtpHeaderParserImpl::RegisterRtpHeaderExtension(RTPExtensionType type,
                                                     uint8_t id) {
  CriticalSectionScoped cs(critical_section_.get());
  return rtp_header_extension_map_.Register(type, id) == 0;
}

bool RtpHeaderParserImpl::DeregisterRtpHeaderExtension(RTPExtensionType type) {
  CriticalSectionScoped cs(critical_section_.get());
  return rtp_header_extension_map_.Deregister(type) == 0;
}
}  // namespace webrtc
