/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNS.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsHttpHandler.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsISupportsBase.h"
#include "nsISupportsUtils.h"
#include "nsITimedChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"
#include "TRR.h"
#include "TRRService.h"
#include "TRRServiceChannel.h"
#include "TRRLoadInfo.h"

#include "mozilla/Base64.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace net {

#undef LOG
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(TRR, nsIHttpPushListener, nsIInterfaceRequestor,
                  nsIStreamListener, nsIRunnable)

const uint8_t kDNS_CLASS_IN = 1;

NS_IMETHODIMP
TRR::Notify(nsITimer* aTimer) {
  if (aTimer == mTimeout) {
    mTimeout = nullptr;
    Cancel();
  } else {
    MOZ_CRASH("Unknown timer");
  }

  return NS_OK;
}

// convert a given host request to a DOH 'body'
//
nsresult TRR::DohEncode(nsCString& aBody, bool aDisableECS) {
  aBody.Truncate();
  // Header
  aBody += '\0';
  aBody += '\0';  // 16 bit id
  aBody += 0x01;  // |QR|   Opcode  |AA|TC|RD| Set the RD bit
  aBody += '\0';  // |RA|   Z    |   RCODE   |
  aBody += '\0';
  aBody += 1;  // QDCOUNT (number of entries in the question section)
  aBody += '\0';
  aBody += '\0';  // ANCOUNT
  aBody += '\0';
  aBody += '\0';  // NSCOUNT

  aBody += '\0';                    // ARCOUNT
  aBody += aDisableECS ? 1 : '\0';  // ARCOUNT low byte for EDNS(0)

  // Question

  // The input host name should be converted to a sequence of labels, where
  // each label consists of a length octet followed by that number of
  // octets.  The domain name terminates with the zero length octet for the
  // null label of the root.
  // Followed by 16 bit QTYPE and 16 bit QCLASS

  int32_t index = 0;
  int32_t offset = 0;
  do {
    bool dotFound = false;
    int32_t labelLength;
    index = mHost.FindChar('.', offset);
    if (kNotFound != index) {
      dotFound = true;
      labelLength = index - offset;
    } else {
      labelLength = mHost.Length() - offset;
    }
    if (labelLength > 63) {
      // too long label!
      return NS_ERROR_ILLEGAL_VALUE;
    }
    if (labelLength > 0) {
      aBody += static_cast<unsigned char>(labelLength);
      nsDependentCSubstring label = Substring(mHost, offset, labelLength);
      aBody.Append(label);
    }
    if (!dotFound) {
      aBody += '\0';  // terminate with a final zero
      break;
    }
    offset += labelLength + 1;  // move over label and dot
  } while (true);

  aBody += static_cast<uint8_t>(mType >> 8);  // upper 8 bit TYPE
  aBody += static_cast<uint8_t>(mType);
  aBody += '\0';           // upper 8 bit CLASS
  aBody += kDNS_CLASS_IN;  // IN - "the Internet"

  if (aDisableECS) {
    // EDNS(0) is RFC 6891, ECS is RFC 7871
    aBody += '\0';  // NAME       | domain name  | MUST be 0 (root domain) |
    aBody += '\0';
    aBody += 41;  // TYPE       | u_int16_t    | OPT (41)                     |
    aBody += 16;  // CLASS      | u_int16_t    | requestor's UDP payload size |
    aBody +=
        '\0';  // advertise 4K (high-byte: 16 | low-byte: 0), ignored by DoH
    aBody += '\0';  // TTL        | u_int32_t    | extended RCODE and flags |
    aBody += '\0';
    aBody += '\0';
    aBody += '\0';

    aBody += '\0';  // upper 8 bit RDLEN
    aBody += 8;  // RDLEN      | u_int16_t    | length of all RDATA          |

    // RDATA      | octet stream | {attribute,value} pairs      |
    // The RDATA is just the ECS option setting zero subnet prefix

    aBody += '\0';  // upper 8 bit OPTION-CODE ECS
    aBody += 8;     // OPTION-CODE, 2 octets, for ECS is 8

    aBody += '\0';  // upper 8 bit OPTION-LENGTH
    aBody += 4;  // OPTION-LENGTH, 2 octets, contains the length of the payload
                 // after OPTION-LENGTH
    aBody += '\0';  // upper 8 bit FAMILY. IANA Address Family Numbers registry,
                    // not the AF_* constants!
    aBody += 1;     // FAMILY (Ipv4), 2 octets

    aBody += '\0';  // SOURCE PREFIX-LENGTH      |     SCOPE PREFIX-LENGTH |
    aBody += '\0';

    // ADDRESS, minimum number of octets == nothing because zero bits
  }
  return NS_OK;
}

NS_IMETHODIMP
TRR::Run() {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && gTRRService,
                NS_IsMainThread() || gTRRService->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  if ((gTRRService == nullptr) || NS_FAILED(SendHTTPRequest())) {
    RecordReason(nsHostRecord::TRR_SEND_FAILED);
    FailData(NS_ERROR_FAILURE);
    // The dtor will now be run
  }
  return NS_OK;
}

static void InitHttpHandler() {
  nsresult rv;
  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ios->GetProtocolHandler("http", getter_AddRefs(handler));
  if (NS_FAILED(rv)) {
    return;
  }
}

nsresult TRR::CreateChannelHelper(nsIURI* aUri, nsIChannel** aResult) {
  *aResult = nullptr;

  if (NS_IsMainThread() && !XRE_IsSocketProcess()) {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_NewChannel(
        aResult, aUri, nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_OTHER,
        nullptr,  // nsICookieJarSettings
        nullptr,  // PerformanceStorage
        nullptr,  // aLoadGroup
        nullptr,  // aCallbacks
        nsIRequest::LOAD_NORMAL, ios);
  }

  // Unfortunately, we can only initialize gHttpHandler on main thread.
  if (!gHttpHandler) {
    nsCOMPtr<nsIEventTarget> main = GetMainThreadEventTarget();
    if (main) {
      // Forward to the main thread synchronously.
      SyncRunnable::DispatchToThread(
          main, new SyncRunnable(NS_NewRunnableFunction(
                    "InitHttpHandler", []() { InitHttpHandler(); })));
    }
  }

  if (!gHttpHandler) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<TRRLoadInfo> loadInfo =
      new TRRLoadInfo(aUri, nsIContentPolicy::TYPE_OTHER);
  return gHttpHandler->CreateTRRServiceChannel(aUri,
                                               nullptr,   // givenProxyInfo
                                               0,         // proxyResolveFlags
                                               nullptr,   // proxyURI
                                               loadInfo,  // aLoadInfo
                                               aResult);
}

nsresult TRR::SendHTTPRequest() {
  // This is essentially the "run" method - created from nsHostResolver

  if ((mType != TRRTYPE_A) && (mType != TRRTYPE_AAAA) &&
      (mType != TRRTYPE_NS) && (mType != TRRTYPE_TXT) &&
      (mType != TRRTYPE_HTTPSSVC)) {
    // limit the calling interface because nsHostResolver has explicit slots for
    // these types
    return NS_ERROR_FAILURE;
  }

  if (((mType == TRRTYPE_A) || (mType == TRRTYPE_AAAA)) &&
      mRec->mEffectiveTRRMode != nsIRequest::TRR_ONLY_MODE) {
    // let NS resolves skip the blocklist check
    // we also don't check the blocklist for TRR only requests
    MOZ_ASSERT(mRec);

    if (UseDefaultServer() &&
        gTRRService->IsTemporarilyBlocked(mHost, mOriginSuffix, mPB, true)) {
      if (mType == TRRTYPE_A) {
        // count only blocklist for A records to avoid double counts
        Telemetry::Accumulate(Telemetry::DNS_TRR_BLACKLISTED2,
                              TRRService::AutoDetectedKey(), true);
      }

      RecordReason(nsHostRecord::TRR_HOST_BLOCKED_TEMPORARY);
      // not really an error but no TRR is issued
      return NS_ERROR_UNKNOWN_HOST;
    }

    if (gTRRService->IsExcludedFromTRR(mHost)) {
      RecordReason(nsHostRecord::TRR_EXCLUDED);
      return NS_ERROR_UNKNOWN_HOST;
    }

    if (UseDefaultServer() && (mType == TRRTYPE_A)) {
      Telemetry::Accumulate(Telemetry::DNS_TRR_BLACKLISTED2,
                            TRRService::AutoDetectedKey(), false);
    }
  }

  bool useGet = StaticPrefs::network_trr_useGET();
  nsAutoCString body;
  nsCOMPtr<nsIURI> dnsURI;
  bool disableECS = StaticPrefs::network_trr_disable_ECS();
  nsresult rv;

  LOG(("TRR::SendHTTPRequest resolve %s type %u\n", mHost.get(), mType));

  if (useGet) {
    nsAutoCString tmp;
    rv = DohEncode(tmp, disableECS);
    NS_ENSURE_SUCCESS(rv, rv);

    /* For GET requests, the outgoing packet needs to be Base64url-encoded and
       then appended to the end of the URI. */
    rv = Base64URLEncode(tmp.Length(),
                         reinterpret_cast<const unsigned char*>(tmp.get()),
                         Base64URLEncodePaddingPolicy::Omit, body);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString uri;
    if (UseDefaultServer()) {
      gTRRService->GetURI(uri);
    } else {
      uri = mRec->mTrrServer;
    }

    rv = NS_NewURI(getter_AddRefs(dnsURI), uri);
    if (NS_FAILED(rv)) {
      LOG(("TRR:SendHTTPRequest: NewURI failed!\n"));
      return rv;
    }

    nsAutoCString query;
    rv = dnsURI->GetQuery(query);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (query.IsEmpty()) {
      query.Assign("?dns="_ns);
    } else {
      query.Append("&dns="_ns);
    }
    query.Append(body);

    rv = NS_MutateURI(dnsURI).SetQuery(query).Finalize(dnsURI);
    LOG(("TRR::SendHTTPRequest GET dns=%s\n", body.get()));
  } else {
    rv = DohEncode(body, disableECS);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString uri;
    if (UseDefaultServer()) {
      gTRRService->GetURI(uri);
    } else {
      uri = mRec->mTrrServer;
    }
    rv = NS_NewURI(getter_AddRefs(dnsURI), uri);
  }
  if (NS_FAILED(rv)) {
    LOG(("TRR:SendHTTPRequest: NewURI failed!\n"));
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = CreateChannelHelper(dnsURI, getter_AddRefs(channel));
  if (NS_FAILED(rv) || !channel) {
    LOG(("TRR:SendHTTPRequest: NewChannel failed!\n"));
    return rv;
  }

  channel->SetLoadFlags(
      nsIRequest::LOAD_ANONYMOUS | nsIRequest::INHIBIT_CACHING |
      nsIRequest::LOAD_BYPASS_CACHE | nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->SetNotificationCallbacks(this);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  if (!httpChannel) {
    return NS_ERROR_UNEXPECTED;
  }

  // This connection should not use TRR
  rv = httpChannel->SetTRRMode(nsIRequest::TRR_DISABLED_MODE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = httpChannel->SetRequestHeader("Accept"_ns, "application/dns-message"_ns,
                                     false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString cred;
  if (UseDefaultServer()) {
    gTRRService->GetCredentials(cred);
  }
  if (!cred.IsEmpty()) {
    rv = httpChannel->SetRequestHeader("Authorization"_ns, cred, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(channel);
  if (!internalChannel) {
    return NS_ERROR_UNEXPECTED;
  }

  // setting a small stream window means the h2 stack won't pipeline a window
  // update with each HEADERS or reply to a DATA with a WINDOW UPDATE
  rv = internalChannel->SetInitialRwin(127 * 1024);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = internalChannel->SetIsTRRServiceChannel(true);
  NS_ENSURE_SUCCESS(rv, rv);

  mAllowRFC1918 = StaticPrefs::network_trr_allow_rfc1918();

  if (useGet) {
    rv = httpChannel->SetRequestMethod("GET"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(httpChannel);
    if (!uploadChannel) {
      return NS_ERROR_UNEXPECTED;
    }
    uint32_t streamLength = body.Length();
    nsCOMPtr<nsIInputStream> uploadStream;
    rv =
        NS_NewCStringInputStream(getter_AddRefs(uploadStream), std::move(body));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = uploadChannel->ExplicitSetUploadStream(uploadStream,
                                                "application/dns-message"_ns,
                                                streamLength, "POST"_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = SetupTRRServiceChannelInternal(httpChannel, useGet);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = httpChannel->AsyncOpen(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // If the asyncOpen succeeded we can say that we actually attempted to
  // use the TRR connection.
  RefPtr<AddrHostRecord> addrRec = do_QueryObject(mRec);
  if (addrRec) {
    addrRec->mTRRUsed = true;
  }

  NS_NewTimerWithCallback(getter_AddRefs(mTimeout), this,
                          gTRRService->GetRequestTimeout(),
                          nsITimer::TYPE_ONE_SHOT);

  mChannel = channel;
  return NS_OK;
}

// static
nsresult TRR::SetupTRRServiceChannelInternal(nsIHttpChannel* aChannel,
                                             bool aUseGet) {
  nsCOMPtr<nsIHttpChannel> httpChannel = aChannel;
  MOZ_ASSERT(httpChannel);

  nsresult rv = NS_OK;
  if (!aUseGet) {
    rv =
        httpChannel->SetRequestHeader("Cache-Control"_ns, "no-store"_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Sanitize the request by removing the Accept-Language header so we minimize
  // the amount of fingerprintable information we send to the server.
  if (!StaticPrefs::network_trr_send_accept_language_headers()) {
    rv = httpChannel->SetRequestHeader("Accept-Language"_ns, ""_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Sanitize the request by removing the User-Agent
  if (!StaticPrefs::network_trr_send_user_agent_headers()) {
    rv = httpChannel->SetRequestHeader("User-Agent"_ns, ""_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (StaticPrefs::network_trr_send_empty_accept_encoding_headers()) {
    rv = httpChannel->SetEmptyRequestHeader("Accept-Encoding"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // set the *default* response content type
  if (NS_FAILED(httpChannel->SetContentType("application/dns-message"_ns))) {
    LOG(("TRR::SetupTRRServiceChannelInternal: couldn't set content-type!\n"));
  }

  nsCOMPtr<nsITimedChannel> timedChan(do_QueryInterface(httpChannel));
  if (timedChan) {
    timedChan->SetTimingEnabled(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
TRR::GetInterface(const nsIID& iid, void** result) {
  if (!iid.Equals(NS_GET_IID(nsIHttpPushListener))) {
    return NS_ERROR_NO_INTERFACE;
  }

  nsCOMPtr<nsIHttpPushListener> copy(this);
  *result = copy.forget().take();
  return NS_OK;
}

nsresult TRR::DohDecodeQuery(const nsCString& query, nsCString& host,
                             enum TrrType& type) {
  FallibleTArray<uint8_t> binary;
  bool found_dns = false;
  LOG(("TRR::DohDecodeQuery %s!\n", query.get()));

  // extract "dns=" from the query string
  nsCCharSeparatedTokenizer tokenizer(query, '&');
  nsAutoCString data;
  while (tokenizer.hasMoreTokens()) {
    const nsACString& token = tokenizer.nextToken();
    nsDependentCSubstring dns = Substring(token, 0, 4);
    nsAutoCString check(dns);
    if (check.Equals("dns=")) {
      nsDependentCSubstring q = Substring(token, 4, -1);
      data = q;
      found_dns = true;
      break;
    }
  }
  if (!found_dns) {
    LOG(("TRR::DohDecodeQuery no dns= in pushed URI query string\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult rv =
      Base64URLDecode(data, Base64URLDecodePaddingPolicy::Ignore, binary);
  NS_ENSURE_SUCCESS(rv, rv);
  uint32_t avail = binary.Length();
  if (avail < 12) {
    return NS_ERROR_FAILURE;
  }
  // check the query bit and the opcode
  if ((binary[2] & 0xf8) != 0) {
    return NS_ERROR_FAILURE;
  }
  uint32_t qdcount = (binary[4] << 8) + binary[5];
  if (!qdcount) {
    return NS_ERROR_FAILURE;
  }

  uint32_t index = 12;
  uint32_t length = 0;
  host.Truncate();
  do {
    if (avail < (index + 1)) {
      return NS_ERROR_UNEXPECTED;
    }

    length = binary[index];
    if (length) {
      if (host.Length()) {
        host.Append(".");
      }
      if (avail < (index + 1 + length)) {
        return NS_ERROR_UNEXPECTED;
      }
      host.Append((const char*)(&binary[0]) + index + 1, length);
    }
    index += 1 + length;  // skip length byte + label
  } while (length);

  LOG(("TRR::DohDecodeQuery host %s\n", host.get()));

  if (avail < (index + 2)) {
    return NS_ERROR_UNEXPECTED;
  }
  uint16_t i16 = 0;
  i16 += binary[index] << 8;
  i16 += binary[index + 1];
  type = (enum TrrType)i16;

  LOG(("TRR::DohDecodeQuery type %d\n", (int)type));

  return NS_OK;
}

nsresult TRR::ReceivePush(nsIHttpChannel* pushed, nsHostRecord* pushedRec) {
  if (!mHostResolver) {
    return NS_ERROR_UNEXPECTED;
  }

  LOG(("TRR::ReceivePush: PUSH incoming!\n"));

  nsCOMPtr<nsIURI> uri;
  pushed->GetURI(getter_AddRefs(uri));
  nsAutoCString query;
  if (uri) {
    uri->GetQuery(query);
  }

  PRNetAddr tempAddr;
  if (NS_FAILED(DohDecodeQuery(query, mHost, mType)) ||
      (PR_StringToNetAddr(mHost.get(), &tempAddr) == PR_SUCCESS)) {  // literal
    LOG(("TRR::ReceivePush failed to decode %s\n", mHost.get()));
    return NS_ERROR_UNEXPECTED;
  }

  if ((mType != TRRTYPE_A) && (mType != TRRTYPE_AAAA) &&
      (mType != TRRTYPE_TXT) && (mType != TRRTYPE_HTTPSSVC)) {
    LOG(("TRR::ReceivePush unknown type %d\n", mType));
    return NS_ERROR_UNEXPECTED;
  }

  if (gTRRService->IsExcludedFromTRR(mHost)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t type = nsIDNSService::RESOLVE_TYPE_DEFAULT;
  if (mType == TRRTYPE_TXT) {
    type = nsIDNSService::RESOLVE_TYPE_TXT;
  } else if (mType == TRRTYPE_HTTPSSVC) {
    type = nsIDNSService::RESOLVE_TYPE_HTTPSSVC;
  }

  RefPtr<nsHostRecord> hostRecord;
  nsresult rv;
  rv = mHostResolver->GetHostRecord(
      mHost, ""_ns, type, pushedRec->flags, pushedRec->af, pushedRec->pb,
      pushedRec->originSuffix, getter_AddRefs(hostRecord));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Since we don't ever call nsHostResolver::NameLookup for this record,
  // we need to copy the trr mode from the previous record
  if (hostRecord->mEffectiveTRRMode == nsIRequest::TRR_DEFAULT_MODE) {
    hostRecord->mEffectiveTRRMode = pushedRec->mEffectiveTRRMode;
  }

  rv = mHostResolver->TrrLookup_unlocked(hostRecord, this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = pushed->AsyncOpen(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // OK!
  mChannel = pushed;
  mRec.swap(hostRecord);

  return NS_OK;
}

NS_IMETHODIMP
TRR::OnPush(nsIHttpChannel* associated, nsIHttpChannel* pushed) {
  LOG(("TRR::OnPush entry\n"));
  MOZ_ASSERT(associated == mChannel);
  if (!mRec) {
    return NS_ERROR_FAILURE;
  }
  if (!UseDefaultServer()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<TRR> trr = new TRR(mHostResolver, mPB);
  return trr->ReceivePush(pushed, mRec);
}

NS_IMETHODIMP
TRR::OnStartRequest(nsIRequest* aRequest) {
  LOG(("TRR::OnStartRequest %p %s %d\n", this, mHost.get(), mType));

  nsresult status = NS_OK;
  aRequest->GetStatus(&status);

  if (NS_FAILED(status)) {
    if (NS_IsOffline()) {
      RecordReason(nsHostRecord::TRR_IS_OFFLINE);
    }

    switch (status) {
      case NS_ERROR_UNKNOWN_HOST:
        RecordReason(nsHostRecord::TRR_CHANNEL_DNS_FAIL);
        break;
      case NS_ERROR_OFFLINE:
        RecordReason(nsHostRecord::TRR_IS_OFFLINE);
        break;
      case NS_ERROR_NET_RESET:
        RecordReason(nsHostRecord::TRR_NET_RESET);
        break;
      case NS_ERROR_NET_TIMEOUT:
        RecordReason(nsHostRecord::TRR_NET_TIMEOUT);
        break;
      case NS_ERROR_PROXY_CONNECTION_REFUSED:
        RecordReason(nsHostRecord::TRR_NET_REFUSED);
        break;
      case NS_ERROR_NET_INTERRUPT:
        RecordReason(nsHostRecord::TRR_NET_INTERRUPT);
        break;
      case NS_ERROR_NET_INADEQUATE_SECURITY:
        RecordReason(nsHostRecord::TRR_NET_INADEQ_SEQURITY);
        break;
      default:
        RecordReason(nsHostRecord::TRR_UNKNOWN_CHANNEL_FAILURE);
    }
  }

  return NS_OK;
}

static uint16_t get16bit(const unsigned char* aData, unsigned int index) {
  return ((aData[index] << 8) | aData[index + 1]);
}

static uint32_t get32bit(const unsigned char* aData, unsigned int index) {
  return (aData[index] << 24) | (aData[index + 1] << 16) |
         (aData[index + 2] << 8) | aData[index + 3];
}

nsresult TRR::PassQName(unsigned int& index) {
  uint8_t length;
  do {
    if (mBodySize < (index + 1)) {
      LOG(("TRR: PassQName:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    length = static_cast<uint8_t>(mResponse[index]);
    if ((length & 0xc0) == 0xc0) {
      // name pointer, advance over it and be done
      if (mBodySize < (index + 2)) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      index += 2;
      break;
    }
    if (length & 0xc0) {
      LOG(("TRR: illegal label length byte (%x) at index %d\n", length, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    // pass label
    if (mBodySize < (index + 1 + length)) {
      LOG(("TRR: PassQName:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 1 + length;
  } while (length);
  return NS_OK;
}

// GetQname: retrieves the qname (stores in 'aQname') and stores the index
// after qname was parsed into the 'aIndex'.

nsresult TRR::GetQname(nsACString& aQname, unsigned int& aIndex) {
  uint8_t clength = 0;
  unsigned int cindex = aIndex;
  unsigned int loop = 128;    // a valid DNS name can never loop this much
  unsigned int endindex = 0;  // index position after this data
  do {
    if (cindex >= mBodySize) {
      LOG(("TRR: bad Qname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    clength = static_cast<uint8_t>(mResponse[cindex]);
    if ((clength & 0xc0) == 0xc0) {
      // name pointer, get the new offset (14 bits)
      if ((cindex + 1) >= mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      // extract the new index position for the next label
      uint16_t newpos = (clength & 0x3f) << 8 | mResponse[cindex + 1];
      if (!endindex) {
        // only update on the first "jump"
        endindex = cindex + 2;
      }
      cindex = newpos;
      continue;
    }
    if (clength & 0xc0) {
      // any of those bits set individually is an error
      LOG(("TRR: bad Qname packet\n"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    cindex++;

    if (clength) {
      if (!aQname.IsEmpty()) {
        aQname.Append(".");
      }
      if ((cindex + clength) > mBodySize) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
      aQname.Append((const char*)(&mResponse[cindex]), clength);
      cindex += clength;  // skip label
    }
  } while (clength && --loop);

  if (!loop) {
    LOG(("TRR::DohDecode pointer loop error\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (!endindex) {
    // there was no "jump"
    endindex = cindex;
  }
  aIndex = endindex;
  return NS_OK;
}

//
// DohDecode() collects the TTL and the IP addresses in the response
//
nsresult TRR::DohDecode(nsCString& aHost) {
  // The response has a 12 byte header followed by the question (returned)
  // and then the answer. The answer section itself contains the name, type
  // and class again and THEN the record data.

  // www.example.com response:
  // header:
  // abcd 8180 0001 0001 0000 0000
  // the question:
  // 0377 7777 0765 7861 6d70 6c65 0363 6f6d 0000 0100 01
  // the answer:
  // 03 7777 7707 6578 616d 706c 6503 636f 6d00 0001 0001
  // 0000 0080 0004 5db8 d822

  unsigned int index = 12;
  uint8_t length;
  nsAutoCString host;
  nsresult rv;

  LOG(("doh decode %s %d bytes\n", aHost.get(), mBodySize));

  mCname.Truncate();

  if (mBodySize < 12 || mResponse[0] || mResponse[1]) {
    LOG(("TRR bad incoming DOH, eject!\n"));
    return NS_ERROR_ILLEGAL_VALUE;
  }
  uint8_t rcode = mResponse[3] & 0x0F;
  if (rcode) {
    LOG(("TRR Decode %s RCODE %d\n", aHost.get(), rcode));
    return NS_ERROR_FAILURE;
  }

  uint16_t questionRecords = get16bit(mResponse, 4);  // qdcount
  // iterate over the single(?) host name in question
  while (questionRecords) {
    do {
      if (mBodySize < (index + 1)) {
        LOG(("TRR Decode 1 index: %u size: %u", index, mBodySize));
        return NS_ERROR_ILLEGAL_VALUE;
      }
      length = static_cast<uint8_t>(mResponse[index]);
      if (length) {
        if (host.Length()) {
          host.Append(".");
        }
        if (mBodySize < (index + 1 + length)) {
          LOG(("TRR Decode 2 index: %u size: %u len: %u", index, mBodySize,
               length));
          return NS_ERROR_ILLEGAL_VALUE;
        }
        host.Append(((char*)mResponse) + index + 1, length);
      }
      index += 1 + length;  // skip length byte + label
    } while (length);
    if (mBodySize < (index + 4)) {
      LOG(("TRR Decode 3 index: %u size: %u", index, mBodySize));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 4;  // skip question's type, class
    questionRecords--;
  }

  // Figure out the number of answer records from ANCOUNT
  uint16_t answerRecords = get16bit(mResponse, 6);

  LOG(("TRR Decode: %d answer records (%u bytes body) %s index=%u\n",
       answerRecords, mBodySize, host.get(), index));

  while (answerRecords) {
    nsAutoCString qname;
    rv = GetQname(qname, index);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // 16 bit TYPE
    if (mBodySize < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index + 2));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t TYPE = get16bit(mResponse, index);

    if ((TYPE != TRRTYPE_CNAME) && (TYPE != TRRTYPE_HTTPSSVC) &&
        (TYPE != static_cast<uint16_t>(mType))) {
      // Not the same type as was asked for nor CNAME
      LOG(("TRR: Dohdecode:%d asked for type %d got %d\n", __LINE__, mType,
           TYPE));
      return NS_ERROR_UNEXPECTED;
    }
    index += 2;

    // 16 bit class
    if (mBodySize < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index + 2));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t CLASS = get16bit(mResponse, index);
    if (kDNS_CLASS_IN != CLASS) {
      LOG(("TRR bad CLASS (%u) at index %d\n", CLASS, index));
      return NS_ERROR_UNEXPECTED;
    }
    index += 2;

    // 32 bit TTL (seconds)
    if (mBodySize < (index + 4)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint32_t TTL = get32bit(mResponse, index);
    index += 4;

    // 16 bit RDLENGTH
    if (mBodySize < (index + 2)) {
      LOG(("TRR: Dohdecode:%d fail at index %d\n", __LINE__, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t RDLENGTH = get16bit(mResponse, index);
    index += 2;

    if (mBodySize < (index + RDLENGTH)) {
      LOG(("TRR: Dohdecode:%d fail RDLENGTH=%d at index %d\n", __LINE__,
           RDLENGTH, index));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    // We check if the qname is a case-insensitive match for the host or the
    // FQDN version of the host
    bool responseMatchesQuestion =
        (qname.Length() == aHost.Length() ||
         (aHost.Length() == qname.Length() + 1 && aHost.Last() == '.')) &&
        qname.Compare(aHost.BeginReading(), true, qname.Length()) == 0;

    if (responseMatchesQuestion) {
      // RDATA
      // - A (TYPE 1):  4 bytes
      // - AAAA (TYPE 28): 16 bytes
      // - NS (TYPE 2): N bytes

      switch (TYPE) {
        case TRRTYPE_A:
          if (RDLENGTH != 4) {
            LOG(("TRR bad length for A (%u)\n", RDLENGTH));
            return NS_ERROR_UNEXPECTED;
          }
          rv = mDNS.Add(TTL, mResponse, index, RDLENGTH, mAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(
                ("TRR:DohDecode failed: local IP addresses or unknown IP "
                 "family\n"));
            return rv;
          }
          break;
        case TRRTYPE_AAAA:
          if (RDLENGTH != 16) {
            LOG(("TRR bad length for AAAA (%u)\n", RDLENGTH));
            return NS_ERROR_UNEXPECTED;
          }
          rv = mDNS.Add(TTL, mResponse, index, RDLENGTH, mAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(("TRR got unique/local IPv6 address!\n"));
            return rv;
          }
          break;

        case TRRTYPE_NS:
          break;
        case TRRTYPE_CNAME:
          if (mCname.IsEmpty()) {
            nsAutoCString qname;
            unsigned int qnameindex = index;
            rv = GetQname(qname, qnameindex);
            if (NS_FAILED(rv)) {
              return rv;
            }
            if (!qname.IsEmpty()) {
              ToLowerCase(qname);
              mCname = qname;
              LOG(("TRR::DohDecode CNAME host %s => %s\n", host.get(),
                   mCname.get()));
            } else {
              LOG(("TRR::DohDecode empty CNAME for host %s!\n", host.get()));
            }
          } else {
            LOG(("TRR::DohDecode CNAME - ignoring another entry\n"));
          }
          break;
        case TRRTYPE_TXT: {
          // TXT record RRDATA sections are a series of character-strings
          // each character string is a length byte followed by that many data
          // bytes
          nsAutoCString txt;
          unsigned int txtIndex = index;
          uint16_t available = RDLENGTH;

          while (available > 0) {
            uint8_t characterStringLen = mResponse[txtIndex++];
            available--;
            if (characterStringLen > available) {
              LOG(("TRR::DohDecode MALFORMED TXT RECORD\n"));
              break;
            }
            txt.Append((const char*)(&mResponse[txtIndex]), characterStringLen);
            txtIndex += characterStringLen;
            available -= characterStringLen;
          }

          if (!mResult.is<TypeRecordTxt>()) {
            mResult = AsVariant(CopyableTArray<nsCString>());
          }

          {
            auto& results = mResult.as<TypeRecordTxt>();
            results.AppendElement(txt);
          }
          if (mTTL > TTL) {
            mTTL = TTL;
          }
          LOG(("TRR::DohDecode TXT host %s => %s\n", host.get(), txt.get()));

          break;
        }
        case TRRTYPE_HTTPSSVC: {
          struct SVCB parsed;
          int32_t lastSvcParamKey = -1;

          unsigned int svcbIndex = index;
          CheckedInt<uint16_t> available = RDLENGTH;

          // Should have at least 2 bytes for the priority and one for the
          // qname length.
          if (available.value() < 3) {
            return NS_ERROR_UNEXPECTED;
          }

          parsed.mSvcFieldPriority = get16bit(mResponse, svcbIndex);
          svcbIndex += 2;

          rv = GetQname(parsed.mSvcDomainName, svcbIndex);
          if (NS_FAILED(rv)) {
            return rv;
          }

          if (parsed.mSvcDomainName.IsEmpty()) {
            if (parsed.mSvcFieldPriority == 0) {
              // For AliasMode SVCB RRs, a TargetName of "." indicates that the
              // service is not available or does not exist.
              continue;
            }

            // For ServiceMode SVCB RRs, if TargetName has the value ".",
            // then the owner name of this record MUST be used as
            // the effective TargetName.
            parsed.mSvcDomainName = qname;
          }

          available -= (svcbIndex - index);
          if (!available.isValid()) {
            return NS_ERROR_UNEXPECTED;
          }
          while (available.value() >= 4) {
            // Every SvcFieldValues must have at least 4 bytes for the
            // SvcParamKey (2 bytes) and length of SvcParamValue (2 bytes)
            // If the length ever goes above the available data, meaning if
            // available ever underflows, then that is an error.
            struct SvcFieldValue value;
            uint16_t key = get16bit(mResponse, svcbIndex);
            svcbIndex += 2;

            // 2.2 Clients MUST consider an RR malformed if SvcParamKeys are
            // not in strictly increasing numeric order.
            if (key <= lastSvcParamKey) {
              LOG(("SvcParamKeys not in increasing order"));
              return NS_ERROR_UNEXPECTED;
            }
            lastSvcParamKey = key;

            uint16_t len = get16bit(mResponse, svcbIndex);
            svcbIndex += 2;

            available -= 4 + len;
            if (!available.isValid()) {
              return NS_ERROR_UNEXPECTED;
            }

            rv = ParseSvcParam(svcbIndex, key, value, len);
            if (NS_FAILED(rv)) {
              return rv;
            }
            svcbIndex += len;

            // If this is an unknown key, we will simply ignore it.
            // We also don't need to record SvcParamKeyMandatory
            if (key == SvcParamKeyMandatory || key > SvcParamKeyLast) {
              continue;
            }

            if (value.mValue.is<SvcParamIpv4Hint>() ||
                value.mValue.is<SvcParamIpv6Hint>()) {
              parsed.mHasIPHints = true;
            }
            if (value.mValue.is<SvcParamEchConfig>()) {
              parsed.mHasEchConfig = true;
              parsed.mEchConfig = value.mValue.as<SvcParamEchConfig>().mValue;
            }
            parsed.mSvcFieldValue.AppendElement(value);
          }

          // Check for AliasForm
          if (mCname.IsEmpty() && parsed.mSvcFieldPriority == 0) {
            // Alias form SvcDomainName must not have the "." value (empty)
            if (parsed.mSvcDomainName.IsEmpty()) {
              return NS_ERROR_UNEXPECTED;
            }
            mCname = parsed.mSvcDomainName;
            ToLowerCase(mCname);
            LOG(("TRR::DohDecode HTTPSSVC AliasForm host %s => %s\n",
                 host.get(), mCname.get()));
            break;
          }

          if (mType != TRRTYPE_HTTPSSVC) {
            // Ignore the entry that we just parsed if we didn't ask for it.
            break;
          }

          if (!mResult.is<TypeRecordHTTPSSVC>()) {
            mResult = mozilla::AsVariant(CopyableTArray<SVCB>());
          }
          {
            auto& results = mResult.as<TypeRecordHTTPSSVC>();
            results.AppendElement(parsed);
            StoreIPHintAsDNSRecord(parsed);
          }
          break;
        }
        default:
          // skip unknown record types
          LOG(("TRR unsupported TYPE (%u) RDLENGTH %u\n", TYPE, RDLENGTH));
          break;
      }
    } else {
      LOG(("TRR asked for %s data but got %s\n", aHost.get(), qname.get()));
    }

    index += RDLENGTH;
    LOG(("done with record type %u len %u index now %u of %u\n", TYPE, RDLENGTH,
         index, mBodySize));
    answerRecords--;
  }

  // NSCOUNT
  uint16_t nsRecords = get16bit(mResponse, 8);
  LOG(("TRR Decode: %d ns records (%u bytes body)\n", nsRecords, mBodySize));
  while (nsRecords) {
    rv = PassQName(index);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mBodySize < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += 2;  // type
    index += 2;  // class
    index += 4;  // ttl

    // 16 bit RDLENGTH
    if (mBodySize < (index + 2)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t RDLENGTH = get16bit(mResponse, index);
    index += 2;
    if (mBodySize < (index + RDLENGTH)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    index += RDLENGTH;
    LOG(("done with nsRecord now %u of %u\n", index, mBodySize));
    nsRecords--;
  }

  // additional resource records
  uint16_t arRecords = get16bit(mResponse, 10);
  LOG(("TRR Decode: %d additional resource records (%u bytes body)\n",
       arRecords, mBodySize));

  nsClassHashtable<nsCStringHashKey, DOHresp> additionalRecords;
  while (arRecords) {
    nsAutoCString qname;
    rv = GetQname(qname, index);
    if (NS_FAILED(rv)) {
      LOG(("Bad qname for additional record"));
      return rv;
    }

    if (mBodySize < (index + 8)) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    uint16_t type = get16bit(mResponse, index);
    index += 2;
    // The next two bytes encode class
    // (or udpPayloadSize when type is TRRTYPE_OPT)
    uint16_t cls = get16bit(mResponse, index);
    index += 2;
    // The next 4 bytes encode TTL
    // (or extRCode + ednsVersion + flags when type is TRRTYPE_OPT)
    uint32_t ttl = get32bit(mResponse, index);
    index += 4;
    // cls and ttl are unused when type is TRRTYPE_OPT

    // 16 bit RDLENGTH
    if (mBodySize < (index + 2)) {
      LOG(("Record too small"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    uint16_t rdlength = get16bit(mResponse, index);
    index += 2;
    if (mBodySize < (index + rdlength)) {
      LOG(("rdlength too big"));
      return NS_ERROR_ILLEGAL_VALUE;
    }

    auto parseRecord = [&]() {
      LOG(("Parsing additional record type: %u", type));
      auto& entry = additionalRecords.GetOrInsert(qname);
      if (!entry) {
        entry.reset(new DOHresp());
      }

      switch (type) {
        case TRRTYPE_A:
          if (kDNS_CLASS_IN != cls) {
            LOG(("NOT IN - returning"));
            return;
          }
          if (rdlength != 4) {
            LOG(("TRR bad length for A (%u)\n", rdlength));
            return;
          }
          rv = entry->Add(ttl, mResponse, index, rdlength, mAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(
                ("TRR:DohDecode failed: local IP addresses or unknown IP "
                 "family\n"));
            return;
          }
          break;
        case TRRTYPE_AAAA:
          if (kDNS_CLASS_IN != cls) {
            LOG(("NOT IN - returning"));
            return;
          }
          if (rdlength != 16) {
            LOG(("TRR bad length for AAAA (%u)\n", rdlength));
            return;
          }
          rv = entry->Add(ttl, mResponse, index, rdlength, mAllowRFC1918);
          if (NS_FAILED(rv)) {
            LOG(("TRR got unique/local IPv6 address!\n"));
            return;
          }
          break;
        case TRRTYPE_OPT: {  // OPT
          LOG(("Parsing opt rdlen: %u", rdlength));
          unsigned int offset = 0;
          while (offset + 2 <= rdlength) {
            uint16_t optCode = get16bit(mResponse, index + offset);
            LOG(("optCode: %u", optCode));
            offset += 2;
            if (offset + 2 > rdlength) {
              break;
            }
            uint16_t optLen = get16bit(mResponse, index + offset);
            LOG(("optLen: %u", optLen));
            offset += 2;
            if (offset + optLen > rdlength) {
              LOG(("offset: %u, optLen: %u, rdlen: %u", offset, optLen,
                   rdlength));
              break;
            }

            LOG(("OPT: code: %u len:%u", optCode, optLen));

            if (optCode != 15) {
              offset += optLen;
              continue;
            }

            // optCode == 15; Extended DNS error

            if (offset + 2 > rdlength || optLen < 2) {
              break;
            }
            mExtendedError = get16bit(mResponse, index + offset);

            LOG((
                "Extended error code: %u message: %s", mExtendedError,
                nsAutoCString((char*)mResponse + index + offset + 2, optLen - 2)
                    .get()));
            offset += optLen;
          }
          break;
        }
        default:
          break;
      }
    };

    parseRecord();

    index += rdlength;
    LOG(("done with additional rr now %u of %u\n", index, mBodySize));
    arRecords--;
  }

  SaveAdditionalRecords(additionalRecords);

  if (index != mBodySize) {
    LOG(("DohDecode failed to parse entire response body, %u out of %u bytes\n",
         index, mBodySize));
    // failed to parse 100%, do not continue
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if ((mType != TRRTYPE_NS) && mCname.IsEmpty() && mDNS.mAddresses.IsEmpty() &&
      mResult.is<TypeRecordEmpty>()) {
    // no entries were stored!
    LOG(("TRR: No entries were stored!\n"));
    return NS_ERROR_FAILURE;
  }

  // https://tools.ietf.org/html/draft-ietf-dnsop-svcb-httpssvc-03#page-14
  // If one or more SVCB records of ServiceForm SvcRecordType are returned for
  // HOST, clients should select the highest-priority option with acceptable
  // parameters.
  if (mResult.is<TypeRecordHTTPSSVC>()) {
    auto& results = mResult.as<TypeRecordHTTPSSVC>();
    results.Sort();
  }

  return NS_OK;
}

void TRR::SaveAdditionalRecords(
    const nsClassHashtable<nsCStringHashKey, DOHresp>& aRecords) {
  if (!mRec) {
    return;
  }
  nsresult rv;
  for (auto iter = aRecords.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Data() && iter.Data()->mAddresses.IsEmpty()) {
      // no point in adding empty records.
      continue;
    }
    RefPtr<nsHostRecord> hostRecord;
    rv = mHostResolver->GetHostRecord(
        iter.Key(), EmptyCString(), nsIDNSService::RESOLVE_TYPE_DEFAULT,
        mRec->flags, AF_UNSPEC, mRec->pb, mRec->originSuffix,
        getter_AddRefs(hostRecord));
    if (NS_FAILED(rv)) {
      LOG(("Failed to get host record for additional record %s",
           nsCString(iter.Key()).get()));
      continue;
    }
    RefPtr<AddrInfo> ai(new AddrInfo(iter.Key(), TRRTYPE_A,
                                     std::move(iter.Data()->mAddresses),
                                     iter.Data()->mTtl));
    mHostResolver->MaybeRenewHostRecord(hostRecord);

    // Since we're not actually calling NameLookup for this record, we need
    // to set these fields to avoid assertions in CompleteLookup.
    // This is quite hacky, and should be fixed.
    hostRecord->mResolving++;
    hostRecord->mEffectiveTRRMode = mRec->mEffectiveTRRMode;
    RefPtr<AddrHostRecord> addrRec = do_QueryObject(hostRecord);
    addrRec->mTrrStart = TimeStamp::Now();
    addrRec->mTrrA = this;  // Hack!
    LOG(("Completing lookup for additional: %s", nsCString(iter.Key()).get()));
    (void)mHostResolver->CompleteLookup(hostRecord, NS_OK, ai, mPB,
                                        mOriginSuffix, AddrHostRecord::TRR_OK);
  }
}

nsresult TRR::ParseSvcParam(unsigned int svcbIndex, uint16_t key,
                            SvcFieldValue& field, uint16_t length) {
  switch (key) {
    case SvcParamKeyMandatory: {
      if (length % 2 != 0) {
        // This key should encode a list of uint16_t
        return NS_ERROR_UNEXPECTED;
      }
      while (length > 0) {
        uint16_t mandatoryKey = get16bit(mResponse, svcbIndex);
        length -= 2;
        svcbIndex += 2;

        if (mandatoryKey > SvcParamKeyLast) {
          LOG(("The mandatory field includes a key we don't support %u",
               mandatoryKey));
          return NS_ERROR_UNEXPECTED;
        }
      }
      break;
    }
    case SvcParamKeyAlpn: {
      field.mValue = AsVariant(SvcParamAlpn{
          .mValue = nsCString((const char*)(&mResponse[svcbIndex]), length)});
      break;
    }
    case SvcParamKeyNoDefaultAlpn: {
      if (length != 0) {
        // This key should not contain a value
        return NS_ERROR_UNEXPECTED;
      }
      field.mValue = AsVariant(SvcParamNoDefaultAlpn{});
      break;
    }
    case SvcParamKeyPort: {
      if (length != 2) {
        // This key should only encode a uint16_t
        return NS_ERROR_UNEXPECTED;
      }
      field.mValue =
          AsVariant(SvcParamPort{.mValue = get16bit(mResponse, svcbIndex)});
      break;
    }
    case SvcParamKeyIpv4Hint: {
      if (length % 4 != 0) {
        // This key should only encode IPv4 addresses
        return NS_ERROR_UNEXPECTED;
      }

      field.mValue = AsVariant(SvcParamIpv4Hint());
      auto& ipv4array = field.mValue.as<SvcParamIpv4Hint>().mValue;
      while (length > 0) {
        NetAddr addr;
        addr.inet.family = AF_INET;
        addr.inet.port = 0;
        addr.inet.ip = ntohl(get32bit(mResponse, svcbIndex));
        ipv4array.AppendElement(addr);
        length -= 4;
        svcbIndex += 4;
      }
      break;
    }
    case SvcParamKeyEchConfig: {
      field.mValue = AsVariant(SvcParamEchConfig{
          .mValue = nsCString((const char*)(&mResponse[svcbIndex]), length)});
      break;
    }
    case SvcParamKeyIpv6Hint: {
      if (length % 16 != 0) {
        // This key should only encode IPv6 addresses
        return NS_ERROR_UNEXPECTED;
      }

      field.mValue = AsVariant(SvcParamIpv6Hint());
      auto& ipv6array = field.mValue.as<SvcParamIpv6Hint>().mValue;
      while (length > 0) {
        NetAddr addr;
        addr.inet6.family = AF_INET6;
        addr.inet6.port = 0;      // unknown
        addr.inet6.flowinfo = 0;  // unknown
        addr.inet6.scope_id = 0;  // unknown
        for (int i = 0; i < 16; i++, svcbIndex++) {
          addr.inet6.ip.u8[i] = mResponse[svcbIndex];
        }
        ipv6array.AppendElement(addr);
        length -= 16;
        // no need to increase svcbIndex - we did it in the for above.
      }
      break;
    }
    default: {
      // Unespected type. We'll just ignore it.
      return NS_OK;
      break;
    }
  }
  return NS_OK;
}

void TRR::StoreIPHintAsDNSRecord(const struct SVCB& aSVCBRecord) {
  LOG(("TRR::StoreIPHintAsDNSRecord [%p] [%s]", this,
       aSVCBRecord.mSvcDomainName.get()));
  CopyableTArray<NetAddr> addresses;
  aSVCBRecord.GetIPHints(addresses);
  if (addresses.IsEmpty()) {
    return;
  }

  RefPtr<nsHostRecord> hostRecord;
  nsresult rv = mHostResolver->GetHostRecord(
      aSVCBRecord.mSvcDomainName, EmptyCString(),
      nsIDNSService::RESOLVE_TYPE_DEFAULT,
      mRec->flags | nsHostResolver::RES_IP_HINT, AF_UNSPEC, mRec->pb,
      mRec->originSuffix, getter_AddRefs(hostRecord));
  if (NS_FAILED(rv)) {
    LOG(("Failed to get host record"));
    return;
  }

  mHostResolver->MaybeRenewHostRecord(hostRecord);

  uint32_t ttl = AddrInfo::NO_TTL_DATA;
  RefPtr<AddrInfo> ai(new AddrInfo(aSVCBRecord.mSvcDomainName, TRRTYPE_A,
                                   std::move(addresses), ttl));

  // Since we're not actually calling NameLookup for this record, we need
  // to set these fields to avoid assertions in CompleteLookup.
  // This is quite hacky, and should be fixed.
  hostRecord->mResolving++;
  hostRecord->mEffectiveTRRMode = mRec->mEffectiveTRRMode;
  RefPtr<AddrHostRecord> addrRec = do_QueryObject(hostRecord);
  addrRec->mTrrStart = TimeStamp::Now();
  addrRec->mTrrA = this;  // Hack!

  (void)mHostResolver->CompleteLookup(hostRecord, NS_OK, ai, mPB, mOriginSuffix,
                                      AddrHostRecord::TRR_OK);
}

nsresult TRR::ReturnData(nsIChannel* aChannel) {
  if (mType != TRRTYPE_TXT && mType != TRRTYPE_HTTPSSVC) {
    // create and populate an AddrInfo instance to pass on
    RefPtr<AddrInfo> ai(
        new AddrInfo(mHost, mType, nsTArray<NetAddr>(), mDNS.mTtl));
    auto builder = ai->Build();
    builder.SetAddresses(std::move(mDNS.mAddresses));
    builder.SetCanonicalHostname(mCname);

    // Set timings.
    nsCOMPtr<nsITimedChannel> timedChan = do_QueryInterface(aChannel);
    if (timedChan) {
      TimeStamp asyncOpen, start, end;
      if (NS_SUCCEEDED(timedChan->GetAsyncOpen(&asyncOpen)) &&
          !asyncOpen.IsNull()) {
        builder.SetTrrFetchDuration(
            (TimeStamp::Now() - asyncOpen).ToMilliseconds());
      }
      if (NS_SUCCEEDED(timedChan->GetRequestStart(&start)) &&
          NS_SUCCEEDED(timedChan->GetResponseEnd(&end)) && !start.IsNull() &&
          !end.IsNull()) {
        builder.SetTrrFetchDurationNetworkOnly((end - start).ToMilliseconds());
      }
    }
    ai = builder.Finish();

    if (!mHostResolver) {
      return NS_ERROR_FAILURE;
    }
    (void)mHostResolver->CompleteLookup(mRec, NS_OK, ai, mPB, mOriginSuffix,
                                        mTRRSkippedReason);
    mHostResolver = nullptr;
    mRec = nullptr;
  } else {
    (void)mHostResolver->CompleteLookupByType(mRec, NS_OK, mResult, mTTL, mPB);
  }
  return NS_OK;
}

// https://datatracker.ietf.org/doc/html/draft-ietf-dnsop-extended-error-16#section-4
// This is a list of errors for which we should not fallback to Do53.
// These are normally DNSSEC failures or explicit filtering performed by the
// recursive resolver.
bool hardFail(uint16_t code) {
  const uint16_t noFallbackErrors[] = {
      4,   // Forged answer (malware filtering)
      6,   // DNSSEC Boggus
      7,   // Signature expired
      8,   // Signature not yet valid
      9,   // DNSKEY Missing
      10,  // RRSIG missing
      11,  // No ZONE Key Bit set
      12,  // NSEC Missing
      17,  // Filtered
  };

  for (const auto& err : noFallbackErrors) {
    if (code == err) {
      return true;
    }
  }
  return false;
}

nsresult TRR::FailData(nsresult error) {
  if (!mHostResolver) {
    return NS_ERROR_FAILURE;
  }

  // If we didn't record a reason until now, record a default one.
  RecordReason(nsHostRecord::TRR_FAILED);

  if (mExtendedError != UINT16_MAX && hardFail(mExtendedError)) {
    error = NS_ERROR_DEFINITIVE_UNKNOWN_HOST;
  }

  if (mType == TRRTYPE_TXT || mType == TRRTYPE_HTTPSSVC) {
    TypeRecordResultType empty(Nothing{});
    (void)mHostResolver->CompleteLookupByType(mRec, error, empty, 0, mPB);
  } else {
    // create and populate an TRR AddrInfo instance to pass on to signal that
    // this comes from TRR
    nsTArray<NetAddr> noAddresses;
    RefPtr<AddrInfo> ai = new AddrInfo(mHost, mType, std::move(noAddresses));

    (void)mHostResolver->CompleteLookup(mRec, error, ai, mPB, mOriginSuffix,
                                        mTRRSkippedReason);
  }

  mHostResolver = nullptr;
  mRec = nullptr;
  return NS_OK;
}

nsresult TRR::FollowCname(nsIChannel* aChannel) {
  nsresult rv = NS_OK;
  nsAutoCString cname;
  while (NS_SUCCEEDED(rv) && mDNS.mAddresses.IsEmpty() && !mCname.IsEmpty() &&
         mCnameLoop > 0) {
    mCnameLoop--;
    LOG(("TRR::On200Response CNAME %s => %s (%u)\n", mHost.get(), mCname.get(),
         mCnameLoop));
    cname = mCname;
    mCname.Truncate();

    LOG(("TRR: check for CNAME record for %s within previous response\n",
         cname.get()));
    rv = DohDecode(cname);
    if (NS_FAILED(rv)) {
      LOG(("TRR::On200Response DohDecode %x\n", (int)rv));
    }
  }

  // restore mCname as DohDecode() change it
  mCname = cname;
  if (NS_SUCCEEDED(rv) && !mDNS.mAddresses.IsEmpty()) {
    ReturnData(aChannel);
    return NS_OK;
  }

  if (!mCnameLoop) {
    LOG(("TRR::On200Response CNAME loop, eject!\n"));
    return NS_ERROR_REDIRECT_LOOP;
  }

  LOG(("TRR::On200Response CNAME %s => %s (%u)\n", mHost.get(), mCname.get(),
       mCnameLoop));
  RefPtr<TRR> trr =
      new TRR(mHostResolver, mRec, mCname, mType, mCnameLoop, mPB);
  if (!gTRRService) {
    return NS_ERROR_FAILURE;
  }
  return gTRRService->DispatchTRRRequest(trr);
}

nsresult TRR::On200Response(nsIChannel* aChannel) {
  // decode body and create an AddrInfo struct for the response
  nsresult rv = DohDecode(mHost);

  if (NS_FAILED(rv)) {
    LOG(("TRR::On200Response DohDecode %x\n", (int)rv));
    RecordReason(nsHostRecord::TRR_DECODE_FAILED);
    return NS_ERROR_FAILURE;
  }

  if (!mDNS.mAddresses.IsEmpty() || mType == TRRTYPE_TXT || mCname.IsEmpty()) {
    // pass back the response data
    ReturnData(aChannel);
    return NS_OK;
  }

  LOG(("TRR::On200Response trying CNAME %s", mCname.get()));
  return FollowCname(aChannel);
}

static void RecordProcessingTime(nsIChannel* aChannel) {
  // This method records the time it took from the last received byte of the
  // DoH response until we've notified the consumer with a host record.
  nsCOMPtr<nsITimedChannel> timedChan = do_QueryInterface(aChannel);
  if (!timedChan) {
    return;
  }
  TimeStamp end;
  if (NS_FAILED(timedChan->GetResponseEnd(&end))) {
    return;
  }

  if (end.IsNull()) {
    return;
  }

  Telemetry::AccumulateTimeDelta(Telemetry::DNS_TRR_PROCESSING_TIME, end);

  LOG(("Processing DoH response took %f ms",
       (TimeStamp::Now() - end).ToMilliseconds()));
}

NS_IMETHODIMP
TRR::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  // The dtor will be run after the function returns
  LOG(("TRR:OnStopRequest %p %s %d failed=%d code=%X\n", this, mHost.get(),
       mType, mFailed, (unsigned int)aStatusCode));
  nsCOMPtr<nsIChannel> channel;
  channel.swap(mChannel);

  {
    // Cancel the timer since we don't need it anymore.
    nsCOMPtr<nsITimer> timer;
    mTimeout.swap(timer);
    if (timer) {
      timer->Cancel();
    }
  }

  if (UseDefaultServer()) {
    // Bad content is still considered "okay" if the HTTP response is okay
    gTRRService->TRRIsOkay(NS_SUCCEEDED(aStatusCode) ? TRRService::OKAY_NORMAL
                                                     : TRRService::OKAY_BAD);
  }

  // if status was "fine", parse the response and pass on the answer
  if (!mFailed && NS_SUCCEEDED(aStatusCode)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    if (!httpChannel) {
      return NS_ERROR_UNEXPECTED;
    }
    nsresult rv = NS_OK;
    nsAutoCString contentType;
    httpChannel->GetContentType(contentType);
    if (contentType.Length() &&
        !contentType.LowerCaseEqualsLiteral("application/dns-message")) {
      LOG(("TRR:OnStopRequest %p %s %d wrong content type %s\n", this,
           mHost.get(), mType, contentType.get()));
      FailData(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

    uint32_t httpStatus;
    rv = httpChannel->GetResponseStatus(&httpStatus);
    if (NS_SUCCEEDED(rv) && httpStatus == 200) {
      rv = On200Response(channel);
      if (NS_SUCCEEDED(rv) && UseDefaultServer()) {
        RecordReason(nsHostRecord::TRR_OK);
        RecordProcessingTime(channel);
        return rv;
      }
    } else {
      RecordReason(nsHostRecord::TRR_SERVER_RESPONSE_ERR);
      LOG(("TRR:OnStopRequest:%d %p rv %x httpStatus %d\n", __LINE__, this,
           (int)rv, httpStatus));
    }
  }

  LOG(("TRR:OnStopRequest %p status %x mFailed %d\n", this, (int)aStatusCode,
       mFailed));
  FailData(NS_ERROR_UNKNOWN_HOST);
  return NS_OK;
}

NS_IMETHODIMP
TRR::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                     uint64_t aOffset, const uint32_t aCount) {
  LOG(("TRR:OnDataAvailable %p %s %d failed=%d aCount=%u\n", this, mHost.get(),
       mType, mFailed, (unsigned int)aCount));
  // receive DNS response into the local buffer
  if (mFailed) {
    return NS_ERROR_FAILURE;
  }

  if (aCount + mBodySize > kMaxSize) {
    LOG(("TRR::OnDataAvailable:%d fail\n", __LINE__));
    mFailed = true;
    return NS_ERROR_FAILURE;
  }

  uint32_t count;
  nsresult rv =
      aInputStream->Read((char*)mResponse + mBodySize, aCount, &count);
  if (NS_FAILED(rv)) {
    LOG(("TRR::OnDataAvailable:%d fail\n", __LINE__));
    mFailed = true;
    return rv;
  }
  MOZ_ASSERT(count == aCount);
  mBodySize += aCount;
  return NS_OK;
}

nsresult DOHresp::Add(uint32_t TTL, unsigned char* dns, unsigned int index,
                      uint16_t len, bool aLocalAllowed) {
  NetAddr addr;
  if (4 == len) {
    // IPv4
    addr.inet.family = AF_INET;
    addr.inet.port = 0;  // unknown
    addr.inet.ip = ntohl(get32bit(dns, index));
  } else if (16 == len) {
    // IPv6
    addr.inet6.family = AF_INET6;
    addr.inet6.port = 0;      // unknown
    addr.inet6.flowinfo = 0;  // unknown
    addr.inet6.scope_id = 0;  // unknown
    for (int i = 0; i < 16; i++, index++) {
      addr.inet6.ip.u8[i] = dns[index];
    }
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  if (addr.IsIPAddrLocal() && !aLocalAllowed) {
    return NS_ERROR_FAILURE;
  }

  // While the DNS packet might return individual TTLs for each address,
  // we can only return one value in the AddrInfo class so pick the
  // lowest number.
  if (mTtl < TTL) {
    mTtl = TTL;
  }

  if (LOG_ENABLED()) {
    char buf[128];
    addr.ToStringBuffer(buf, sizeof(buf));
    LOG(("DOHresp:Add %s\n", buf));
  }
  mAddresses.AppendElement(addr);
  return NS_OK;
}

class ProxyCancel : public Runnable {
 public:
  explicit ProxyCancel(TRR* aTRR) : Runnable("proxyTrrCancel"), mTRR(aTRR) {}

  NS_IMETHOD Run() override {
    mTRR->Cancel();
    mTRR = nullptr;
    return NS_OK;
  }

 private:
  RefPtr<TRR> mTRR;
};

void TRR::Cancel() {
  RefPtr<TRRServiceChannel> trrServiceChannel = do_QueryObject(mChannel);
  if (trrServiceChannel && !XRE_IsSocketProcess()) {
    if (gTRRService) {
      nsCOMPtr<nsIThread> thread = gTRRService->TRRThread();
      if (thread && !thread->IsOnCurrentThread()) {
        nsCOMPtr<nsIRunnable> r = new ProxyCancel(this);
        thread->Dispatch(r.forget());
        return;
      }
    }
  } else {
    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(new ProxyCancel(this));
      return;
    }
  }

  if (mChannel) {
    RecordReason(nsHostRecord::TRR_TIMEOUT);
    LOG(("TRR: %p canceling Channel %p %s %d\n", this, mChannel.get(),
         mHost.get(), mType));
    mChannel->Cancel(NS_ERROR_ABORT);
    if (UseDefaultServer()) {
      gTRRService->TRRIsOkay(TRRService::OKAY_TIMEOUT);
    }
  }
}

bool TRR::UseDefaultServer() { return !mRec || mRec->mTrrServer.IsEmpty(); }

#undef LOG

// namespace
}  // namespace net
}  // namespace mozilla
