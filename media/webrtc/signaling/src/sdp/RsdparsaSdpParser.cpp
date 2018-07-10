/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"

#include "mozilla/UniquePtr.h"

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpEnum.h"
#include "signaling/src/sdp/RsdparsaSdp.h"
#include "signaling/src/sdp/RsdparsaSdpParser.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"
#include "signaling/src/sdp/RsdparsaSdpGlue.h"

namespace mozilla
{

UniquePtr<Sdp>
RsdparsaSdpParser::Parse(const std::string &sdpText)
{
  ClearParseErrors();
  RustSdpSession* result;
  RustSdpError* err;
  StringView sdpTextView{sdpText.c_str(), sdpText.length()};
  nsresult rv = parse_sdp(sdpTextView, false, &result, &err);
  if (rv != NS_OK) {
    size_t line = sdp_get_error_line_num(err);
    std::string errMsg = convertStringView(sdp_get_error_message(err));
    sdp_free_error(err);
    AddParseError(line, errMsg);
    return nullptr;
  }

  if(err) {
    size_t line = sdp_get_error_line_num(err);
    std::string warningMsg = convertStringView(sdp_get_error_message(err));
    sdp_free_error(err);
    AddParseWarnings(line, warningMsg);
  }

  RsdparsaSessionHandle uniqueResult;
  uniqueResult.reset(result);
  RustSdpOrigin rustOrigin = sdp_get_origin(uniqueResult.get());
  sdp::AddrType addrType = convertAddressType(rustOrigin.addr.addrType);
  SdpOrigin origin(convertStringView(rustOrigin.username),
                   rustOrigin.sessionId, rustOrigin.sessionVersion,
                   addrType, std::string(rustOrigin.addr.unicastAddr));

    return MakeUnique<RsdparsaSdp>(std::move(uniqueResult), origin);
}

} // namespace mozilla
