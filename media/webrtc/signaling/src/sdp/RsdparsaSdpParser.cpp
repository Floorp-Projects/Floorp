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
  const char* rawString = sdpText.c_str();
  RustSdpSession* result;
  RustSdpError* err;
  nsresult rv = parse_sdp(rawString, sdpText.length() + 1, false,
                          &result, &err);
  if (rv != NS_OK) {
    // TODO: err should eventually never be null if rv != NS_OK
    // see Bug 1433529
    if (err != nullptr) {
      size_t line = sdp_get_error_line_num(err);
      std::string errMsg = convertStringView(sdp_get_error_message(err));
      sdp_free_error(err);
      AddParseError(line, errMsg);
    } else {
      AddParseError(0, "Unhandled Rsdparsa parsing error");
    }
    return nullptr;
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
