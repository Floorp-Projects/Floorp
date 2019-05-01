/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/sdp/SipccSdp.h"

#include <utility>
extern "C" {
#include "signaling/src/sdp/sipcc/sdp.h"
}

namespace mozilla {

extern "C" {

void sipcc_sdp_parser_error_handler(void* context, uint32_t line,
                                    const char* message) {
  SdpErrorHolder* errorHolder = static_cast<SdpErrorHolder*>(context);
  std::string err(message);
  errorHolder->AddParseError(line, err);
}

}  // extern "C"

UniquePtr<Sdp> SipccSdpParser::Parse(const std::string& sdpText) {
  ClearParseErrors();

  sdp_conf_options_t* sipcc_config = sdp_init_config();
  if (!sipcc_config) {
    return UniquePtr<Sdp>();
  }

  sdp_nettype_supported(sipcc_config, SDP_NT_INTERNET, true);
  sdp_addrtype_supported(sipcc_config, SDP_AT_IP4, true);
  sdp_addrtype_supported(sipcc_config, SDP_AT_IP6, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPSAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPSAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_UDPTLSRTPSAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_UDPTLSRTPSAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_TCPTLSRTPSAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_TCPTLSRTPSAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_DTLSSCTP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_UDPDTLSSCTP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_TCPDTLSSCTP, true);
  sdp_require_session_name(sipcc_config, false);

  sdp_config_set_error_handler(sipcc_config, &sipcc_sdp_parser_error_handler,
                               this);

  // Takes ownership of |sipcc_config| iff it succeeds
  sdp_t* sdp = sdp_init_description(sipcc_config);
  if (!sdp) {
    sdp_free_config(sipcc_config);
    return UniquePtr<Sdp>();
  }

  const char* rawString = sdpText.c_str();
  sdp_result_e sdpres = sdp_parse(sdp, rawString, sdpText.length());
  if (sdpres != SDP_SUCCESS) {
    sdp_free_description(sdp);
    return UniquePtr<Sdp>();
  }

  UniquePtr<SipccSdp> sipccSdp(new SipccSdp);

  bool success = sipccSdp->Load(sdp, *this);
  sdp_free_description(sdp);
  if (!success) {
    return UniquePtr<Sdp>();
  }

  return UniquePtr<Sdp>(std::move(sipccSdp));
}

}  // namespace mozilla
