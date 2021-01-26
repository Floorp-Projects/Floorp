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
#undef LOG_ENABLED
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(mozilla::net::gHostResolverLog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(TRR, nsIHttpPushListener, nsIInterfaceRequestor,
                  nsIStreamListener, nsIRunnable)

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
    rv = DNSPacket::EncodeRequest(tmp, mHost, mType, disableECS);
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
    rv = DNSPacket::EncodeRequest(body, mHost, mType, disableECS);
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
  nsAutoCString data;
  for (const nsACString& token :
       nsCCharSeparatedTokenizer(query, '&').ToRange()) {
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
    LOG(("Completing lookup for additional: %s", nsCString(iter.Key()).get()));
    (void)mHostResolver->CompleteLookup(hostRecord, NS_OK, ai, mPB,
                                        mOriginSuffix, AddrHostRecord::TRR_OK);
  }
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

nsresult TRR::FailData(nsresult error) {
  if (!mHostResolver) {
    return NS_ERROR_FAILURE;
  }

  // If we didn't record a reason until now, record a default one.
  RecordReason(nsHostRecord::TRR_FAILED);

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
    nsClassHashtable<nsCStringHashKey, DOHresp> additionalRecords;
    rv = mPacket.Decode(
        cname, mType, mCname, StaticPrefs::network_trr_allow_rfc1918(),
        mTRRSkippedReason, mDNS, mResult, additionalRecords, mTTL);
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
  nsClassHashtable<nsCStringHashKey, DOHresp> additionalRecords;
  nsresult rv = mPacket.Decode(
      mHost, mType, mCname, StaticPrefs::network_trr_allow_rfc1918(),
      mTRRSkippedReason, mDNS, mResult, additionalRecords, mTTL);

  if (NS_FAILED(rv)) {
    LOG(("TRR::On200Response DohDecode %x\n", (int)rv));
    RecordReason(nsHostRecord::TRR_DECODE_FAILED);
    return rv;
  }
  SaveAdditionalRecords(additionalRecords);

  if (mResult.is<TypeRecordHTTPSSVC>()) {
    auto& results = mResult.as<TypeRecordHTTPSSVC>();
    for (const auto& rec : results) {
      StoreIPHintAsDNSRecord(rec);
    }
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

  nsresult rv = NS_OK;
  // if status was "fine", parse the response and pass on the answer
  if (!mFailed && NS_SUCCEEDED(aStatusCode)) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    if (!httpChannel) {
      return NS_ERROR_UNEXPECTED;
    }
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
  FailData(NS_SUCCEEDED(rv) ? NS_ERROR_UNKNOWN_HOST : rv);
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

  nsresult rv =
      mPacket.OnDataAvailable(aRequest, aInputStream, aOffset, aCount);
  if (NS_FAILED(rv)) {
    LOG(("TRR::OnDataAvailable:%d fail\n", __LINE__));
    mFailed = true;
    return rv;
  }
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
