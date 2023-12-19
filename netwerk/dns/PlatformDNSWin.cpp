/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetAddrInfo.h"
#include "mozilla/net/DNSPacket.h"
#include "nsIDNSService.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"

#ifdef DNSQUERY_AVAILABLE
// There is a bug in windns.h where the type of parameter ppQueryResultsSet for
// DnsQuery_A is dependent on UNICODE being set. It should *always* be
// PDNS_RECORDA, but if UNICODE is set it is PDNS_RECORDW. To get around this
// we make sure that UNICODE is unset.
#  undef UNICODE
#  include <ws2tcpip.h>
#  undef GetAddrInfo
#  include <windns.h>
#endif  // DNSQUERY_AVAILABLE

namespace mozilla::net {

#define LOG(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Debug, ("[DNS]: " msg, ##__VA_ARGS__))

nsresult ResolveHTTPSRecordImpl(const nsACString& aHost, uint16_t aFlags,
                                TypeRecordResultType& aResult, uint32_t& aTTL) {
  nsAutoCString host(aHost);
  PDNS_RECORD result = nullptr;
  nsAutoCString cname;
  aTTL = UINT32_MAX;

  DNS_STATUS status =
      DnsQuery_A(host.get(), nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
                 DNS_QUERY_STANDARD, nullptr, &result, nullptr);
  if (status != ERROR_SUCCESS) {
    LOG("DnsQuery_A failed with error: %ld\n", status);
    return NS_ERROR_UNKNOWN_HOST;
  }

  // This will free the record if we exit early from this function.
  auto freeDnsRecord =
      MakeScopeExit([&]() { DnsRecordListFree(result, DnsFreeRecordList); });

  auto CheckRecords = [&aResult, &cname, &aTTL](
                          PDNS_RECORD result,
                          const nsCString& aHost) -> nsresult {
    PDNS_RECORD current = result;

    for (current = result; current; current = current->pNext) {
      if (strcmp(current->pName, aHost.get()) != 0) {
        continue;
      }
      if (current->wType == nsIDNSService::RESOLVE_TYPE_HTTPSSVC) {
        const unsigned char* ptr = (const unsigned char*)&(current->Data);
        struct SVCB parsed;
        nsresult rv = DNSPacket::ParseHTTPS(current->wDataLength, parsed, 0,
                                            ptr, current->wDataLength, aHost);
        if (NS_FAILED(rv)) {
          return rv;
        }

        if (parsed.mSvcDomainName.IsEmpty() && parsed.mSvcFieldPriority == 0) {
          // For AliasMode SVCB RRs, a TargetName of "." indicates that the
          // service is not available or does not exist.
          continue;
        }

        if (parsed.mSvcFieldPriority == 0) {
          // Alias form SvcDomainName must not have the "." value (empty)
          if (parsed.mSvcDomainName.IsEmpty()) {
            return NS_ERROR_UNEXPECTED;
          }
          cname = parsed.mSvcDomainName;
          ToLowerCase(cname);
          break;
        }

        if (!aResult.is<TypeRecordHTTPSSVC>()) {
          aResult = mozilla::AsVariant(CopyableTArray<SVCB>());
        }
        auto& results = aResult.as<TypeRecordHTTPSSVC>();
        results.AppendElement(parsed);
        aTTL = std::min<uint32_t>(aTTL, current->dwTtl);
      } else if (current->wType == DNS_TYPE_CNAME) {
        cname = current->Data.Cname.pNameHost;
        ToLowerCase(cname);
        aTTL = std::min<uint32_t>(aTTL, current->dwTtl);
        break;
      }
    }
    return NS_OK;
  };

  int32_t loopCount = 64;
  while (loopCount > 0 && aResult.is<Nothing>()) {
    loopCount--;

    nsresult rv = CheckRecords(result, host);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (aResult.is<Nothing>() && !cname.IsEmpty()) {
      host = cname;
      cname.Truncate();
      continue;
    }

    if (aResult.is<Nothing>()) {
      return NS_ERROR_UNKNOWN_HOST;
    }
  }

  // CNAME loop
  if (loopCount == 0) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  if (aResult.is<Nothing>()) {
    // The call succeeded, but no HTTPS records were found.
    return NS_ERROR_UNKNOWN_HOST;
  }

  if (aTTL == UINT32_MAX) {
    aTTL = 60;  // Defaults to 60 seconds
  }
  return NS_OK;
}

}  // namespace mozilla::net
