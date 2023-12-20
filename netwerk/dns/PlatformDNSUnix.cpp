/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetAddrInfo.h"
#include "mozilla/net/DNSPacket.h"
#include "nsIDNSService.h"
#include "mozilla/Maybe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <resolv.h>

namespace mozilla::net {

#define LOG(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Debug, ("[DNS]: " msg, ##__VA_ARGS__))

nsresult ResolveHTTPSRecordImpl(const nsACString& aHost, uint16_t aFlags,
                                TypeRecordResultType& aResult, uint32_t& aTTL) {
  DNSPacket packet;
  nsAutoCString host(aHost);
  nsAutoCString cname;
  nsresult rv;

  LOG("resolving %s\n", host.get());
  // Perform the query
  rv = packet.FillBuffer(
      [&](unsigned char response[DNSPacket::MAX_SIZE]) -> int {
        int len = 0;
#if defined(XP_LINUX)
        len = res_nquery(&_res, host.get(), ns_c_in,
                         nsIDNSService::RESOLVE_TYPE_HTTPSSVC, response,
                         DNSPacket::MAX_SIZE);
#elif defined(XP_MACOSX)
        len =
            res_query(host.get(), ns_c_in, nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
                      response, DNSPacket::MAX_SIZE);
#endif

        if (len < 0) {
          LOG("DNS query failed");
        }
        return len;
      });
  if (NS_FAILED(rv)) {
    return rv;
  }
  packet.SetNativePacket(true);

  int32_t loopCount = 64;
  while (loopCount > 0 && aResult.is<Nothing>()) {
    loopCount--;
    DOHresp resp;
    nsClassHashtable<nsCStringHashKey, DOHresp> additionalRecords;
    rv = packet.Decode(host, TRRTYPE_HTTPSSVC, cname, true, resp, aResult,
                       additionalRecords, aTTL);
    if (NS_FAILED(rv)) {
      LOG("Decode failed %x", static_cast<uint32_t>(rv));
      return rv;
    }
    if (!cname.IsEmpty() && aResult.is<Nothing>()) {
      host = cname;
      cname.Truncate();
      continue;
    }
  }

  if (aResult.is<Nothing>()) {
    LOG("Result is nothing");
    // The call succeeded, but no HTTPS records were found.
    return NS_ERROR_UNKNOWN_HOST;
  }

  return NS_OK;
}

}  // namespace mozilla::net
