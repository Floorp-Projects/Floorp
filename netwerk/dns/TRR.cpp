/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNS.h"
#include "DNSUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsHttpHandler.h"
#include "nsHostResolver.h"
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
#include "nsQueryObject.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"
#include "ODoH.h"
#include "TRR.h"
#include "TRRService.h"
#include "TRRServiceChannel.h"
#include "TRRLoadInfo.h"

#include "mozilla/Base64.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/UniquePtr.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(TRR, nsIHttpPushListener, nsIInterfaceRequestor,
                  nsIStreamListener, nsIRunnable, nsITimerCallback)

// when firing off a normal A or AAAA query
TRR::TRR(AHostResolver* aResolver, nsHostRecord* aRec, enum TrrType aType)
    : mozilla::Runnable("TRR"),
      mRec(aRec),
      mHostResolver(aResolver),
      mType(aType),
      mOriginSuffix(aRec->originSuffix) {
  mHost = aRec->host;
  mPB = aRec->pb;
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess(),
                        "TRR must be in parent or socket process");
}

// when following CNAMEs
TRR::TRR(AHostResolver* aResolver, nsHostRecord* aRec, nsCString& aHost,
         enum TrrType& aType, unsigned int aLoopCount, bool aPB)
    : mozilla::Runnable("TRR"),
      mHost(aHost),
      mRec(aRec),
      mHostResolver(aResolver),
      mType(aType),
      mPB(aPB),
      mCnameLoop(aLoopCount),
      mOriginSuffix(aRec ? aRec->originSuffix : ""_ns) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess(),
                        "TRR must be in parent or socket process");
}

// used on push
TRR::TRR(AHostResolver* aResolver, bool aPB)
    : mozilla::Runnable("TRR"), mHostResolver(aResolver), mPB(aPB) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess(),
                        "TRR must be in parent or socket process");
}

// to verify a domain
TRR::TRR(AHostResolver* aResolver, nsACString& aHost, enum TrrType aType,
         const nsACString& aOriginSuffix, bool aPB, bool aUseFreshConnection)
    : mozilla::Runnable("TRR"),
      mHost(aHost),
      mRec(nullptr),
      mHostResolver(aResolver),
      mType(aType),
      mPB(aPB),
      mOriginSuffix(aOriginSuffix),
      mUseFreshConnection(aUseFreshConnection) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess(),
                        "TRR must be in parent or socket process");
}

void TRR::HandleTimeout() {
  mTimeout = nullptr;
  RecordReason(TRRSkippedReason::TRR_TIMEOUT);
  Cancel(NS_ERROR_NET_TIMEOUT_EXTERNAL);
}

NS_IMETHODIMP
TRR::Notify(nsITimer* aTimer) {
  if (aTimer == mTimeout) {
    HandleTimeout();
  } else {
    MOZ_CRASH("Unknown timer");
  }

  return NS_OK;
}

NS_IMETHODIMP
TRR::Run() {
  MOZ_ASSERT_IF(XRE_IsParentProcess() && TRRService::Get(),
                NS_IsMainThread() || TRRService::Get()->IsOnTRRThread());
  MOZ_ASSERT_IF(XRE_IsSocketProcess(), NS_IsMainThread());

  if ((TRRService::Get() == nullptr) || NS_FAILED(SendHTTPRequest())) {
    RecordReason(TRRSkippedReason::TRR_SEND_FAILED);
    FailData(NS_ERROR_FAILURE);
    // The dtor will now be run
  }
  return NS_OK;
}

DNSPacket* TRR::GetOrCreateDNSPacket() {
  if (!mPacket) {
    mPacket = MakeUnique<DNSPacket>();
  }

  return mPacket.get();
}

nsresult TRR::CreateQueryURI(nsIURI** aOutURI) {
  nsAutoCString uri;
  nsCOMPtr<nsIURI> dnsURI;
  if (UseDefaultServer()) {
    TRRService::Get()->GetURI(uri);
  } else {
    uri = mRec->mTrrServer;
  }

  nsresult rv = NS_NewURI(getter_AddRefs(dnsURI), uri);
  if (NS_FAILED(rv)) {
    return rv;
  }

  dnsURI.forget(aOutURI);
  return NS_OK;
}

bool TRR::MaybeBlockRequest() {
  if (((mType == TRRTYPE_A) || (mType == TRRTYPE_AAAA)) &&
      mRec->mEffectiveTRRMode != nsIRequest::TRR_ONLY_MODE) {
    // let NS resolves skip the blocklist check
    // we also don't check the blocklist for TRR only requests
    MOZ_ASSERT(mRec);

    // If TRRService isn't enabled anymore for the req, don't do TRR.
    if (!TRRService::Get()->Enabled(mRec->mEffectiveTRRMode)) {
      RecordReason(TRRSkippedReason::TRR_MODE_NOT_ENABLED);
      return true;
    }

    if (!StaticPrefs::network_trr_strict_native_fallback() &&
        UseDefaultServer() &&
        TRRService::Get()->IsTemporarilyBlocked(mHost, mOriginSuffix, mPB,
                                                true)) {
      if (mType == TRRTYPE_A) {
        // count only blocklist for A records to avoid double counts
        Telemetry::Accumulate(Telemetry::DNS_TRR_BLACKLISTED3,
                              TRRService::ProviderKey(), true);
      }

      RecordReason(TRRSkippedReason::TRR_HOST_BLOCKED_TEMPORARY);
      // not really an error but no TRR is issued
      return true;
    }

    if (TRRService::Get()->IsExcludedFromTRR(mHost)) {
      RecordReason(TRRSkippedReason::TRR_EXCLUDED);
      return true;
    }

    if (UseDefaultServer() && (mType == TRRTYPE_A)) {
      Telemetry::Accumulate(Telemetry::DNS_TRR_BLACKLISTED3,
                            TRRService::ProviderKey(), false);
    }
  }

  return false;
}

nsresult TRR::SendHTTPRequest() {
  // This is essentially the "run" method - created from nsHostResolver
  if (mCancelled) {
    return NS_ERROR_FAILURE;
  }

  if ((mType != TRRTYPE_A) && (mType != TRRTYPE_AAAA) &&
      (mType != TRRTYPE_NS) && (mType != TRRTYPE_TXT) &&
      (mType != TRRTYPE_HTTPSSVC)) {
    // limit the calling interface because nsHostResolver has explicit slots for
    // these types
    return NS_ERROR_FAILURE;
  }

  if (MaybeBlockRequest()) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  LOG(("TRR::SendHTTPRequest resolve %s type %u\n", mHost.get(), mType));

  nsAutoCString body;
  bool disableECS = StaticPrefs::network_trr_disable_ECS();
  nsresult rv =
      GetOrCreateDNSPacket()->EncodeRequest(body, mHost, mType, disableECS);
  if (NS_FAILED(rv)) {
    HandleEncodeError(rv);
    return rv;
  }

  bool useGet = StaticPrefs::network_trr_useGET();
  nsCOMPtr<nsIURI> dnsURI;
  rv = CreateQueryURI(getter_AddRefs(dnsURI));
  if (NS_FAILED(rv)) {
    LOG(("TRR:SendHTTPRequest: NewURI failed!\n"));
    return rv;
  }

  if (useGet) {
    /* For GET requests, the outgoing packet needs to be Base64url-encoded and
       then appended to the end of the URI. */
    nsAutoCString encoded;
    rv = Base64URLEncode(body.Length(),
                         reinterpret_cast<const unsigned char*>(body.get()),
                         Base64URLEncodePaddingPolicy::Omit, encoded);
    NS_ENSURE_SUCCESS(rv, rv);

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
    query.Append(encoded);

    rv = NS_MutateURI(dnsURI).SetQuery(query).Finalize(dnsURI);
    LOG(("TRR::SendHTTPRequest GET dns=%s\n", body.get()));
  }

  nsCOMPtr<nsIChannel> channel;
  rv = DNSUtils::CreateChannelHelper(dnsURI, getter_AddRefs(channel));
  if (NS_FAILED(rv) || !channel) {
    LOG(("TRR:SendHTTPRequest: NewChannel failed!\n"));
    return rv;
  }

  auto loadFlags = nsIRequest::LOAD_ANONYMOUS | nsIRequest::INHIBIT_CACHING |
                   nsIRequest::LOAD_BYPASS_CACHE |
                   nsIChannel::LOAD_BYPASS_URL_CLASSIFIER;
  if (mUseFreshConnection) {
    // Causes TRRServiceChannel to tell the connection manager
    // to clear out any connection with the current conn info.
    loadFlags |= nsIRequest::LOAD_FRESH_CONNECTION;
  }
  channel->SetLoadFlags(loadFlags);
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

  nsCString contentType(ContentType());
  rv = httpChannel->SetRequestHeader("Accept"_ns, contentType, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString cred;
  if (UseDefaultServer()) {
    TRRService::Get()->GetCredentials(cred);
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

  if (UseDefaultServer() && StaticPrefs::network_trr_async_connInfo()) {
    RefPtr<nsHttpConnectionInfo> trrConnInfo =
        TRRService::Get()->TRRConnectionInfo();
    if (trrConnInfo) {
      nsAutoCString host;
      dnsURI->GetHost(host);
      if (host.Equals(trrConnInfo->GetOrigin())) {
        internalChannel->SetConnectionInfo(trrConnInfo);
        LOG(("TRR::SendHTTPRequest use conn info:%s\n",
             trrConnInfo->HashKey().get()));
      } else {
        MOZ_DIAGNOSTIC_ASSERT(false);
      }
    } else {
      TRRService::Get()->InitTRRConnectionInfo();
    }
  }

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

    rv = uploadChannel->ExplicitSetUploadStream(uploadStream, contentType,
                                                streamLength, "POST"_ns, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = SetupTRRServiceChannelInternal(httpChannel, useGet, contentType);
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
    addrRec->mResolverType = ResolverType();
  }

  NS_NewTimerWithCallback(
      getter_AddRefs(mTimeout), this,
      mTimeoutMs ? mTimeoutMs : TRRService::Get()->GetRequestTimeout(),
      nsITimer::TYPE_ONE_SHOT);

  mChannel = channel;
  return NS_OK;
}

// static
nsresult TRR::SetupTRRServiceChannelInternal(nsIHttpChannel* aChannel,
                                             bool aUseGet,
                                             const nsACString& aContentType) {
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
  if (NS_FAILED(httpChannel->SetContentType(aContentType))) {
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

  if (NS_FAILED(DohDecodeQuery(query, mHost, mType)) ||
      HostIsIPLiteral(mHost)) {  // literal
    LOG(("TRR::ReceivePush failed to decode %s\n", mHost.get()));
    return NS_ERROR_UNEXPECTED;
  }

  if ((mType != TRRTYPE_A) && (mType != TRRTYPE_AAAA) &&
      (mType != TRRTYPE_TXT) && (mType != TRRTYPE_HTTPSSVC)) {
    LOG(("TRR::ReceivePush unknown type %d\n", mType));
    return NS_ERROR_UNEXPECTED;
  }

  if (TRRService::Get()->IsExcludedFromTRR(mHost)) {
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
      RecordReason(TRRSkippedReason::TRR_IS_OFFLINE);
    }

    switch (status) {
      case NS_ERROR_UNKNOWN_HOST:
        RecordReason(TRRSkippedReason::TRR_CHANNEL_DNS_FAIL);
        break;
      case NS_ERROR_OFFLINE:
        RecordReason(TRRSkippedReason::TRR_IS_OFFLINE);
        break;
      case NS_ERROR_NET_RESET:
        RecordReason(TRRSkippedReason::TRR_NET_RESET);
        break;
      case NS_ERROR_NET_TIMEOUT:
      case NS_ERROR_NET_TIMEOUT_EXTERNAL:
        RecordReason(TRRSkippedReason::TRR_NET_TIMEOUT);
        break;
      case NS_ERROR_PROXY_CONNECTION_REFUSED:
        RecordReason(TRRSkippedReason::TRR_NET_REFUSED);
        break;
      case NS_ERROR_NET_INTERRUPT:
        RecordReason(TRRSkippedReason::TRR_NET_INTERRUPT);
        break;
      case NS_ERROR_NET_INADEQUATE_SECURITY:
        RecordReason(TRRSkippedReason::TRR_NET_INADEQ_SEQURITY);
        break;
      default:
        RecordReason(TRRSkippedReason::TRR_UNKNOWN_CHANNEL_FAILURE);
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
  for (const auto& recordEntry : aRecords) {
    if (recordEntry.GetData() && recordEntry.GetData()->mAddresses.IsEmpty()) {
      // no point in adding empty records.
      continue;
    }
    RefPtr<nsHostRecord> hostRecord;
    rv = mHostResolver->GetHostRecord(
        recordEntry.GetKey(), EmptyCString(),
        nsIDNSService::RESOLVE_TYPE_DEFAULT, mRec->flags, AF_UNSPEC, mRec->pb,
        mRec->originSuffix, getter_AddRefs(hostRecord));
    if (NS_FAILED(rv)) {
      LOG(("Failed to get host record for additional record %s",
           nsCString(recordEntry.GetKey()).get()));
      continue;
    }
    RefPtr<AddrInfo> ai(
        new AddrInfo(recordEntry.GetKey(), ResolverType(), TRRTYPE_A,
                     std::move(recordEntry.GetData()->mAddresses),
                     recordEntry.GetData()->mTtl));
    mHostResolver->MaybeRenewHostRecord(hostRecord);

    // Since we're not actually calling NameLookup for this record, we need
    // to set these fields to avoid assertions in CompleteLookup.
    // This is quite hacky, and should be fixed.
    hostRecord->mResolving++;
    hostRecord->mEffectiveTRRMode = mRec->mEffectiveTRRMode;
    LOG(("Completing lookup for additional: %s",
         nsCString(recordEntry.GetKey()).get()));
    (void)mHostResolver->CompleteLookup(hostRecord, NS_OK, ai, mPB,
                                        mOriginSuffix, TRRSkippedReason::TRR_OK,
                                        this);
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
      mRec->flags | nsIDNSService::RESOLVE_IP_HINT, AF_UNSPEC, mRec->pb,
      mRec->originSuffix, getter_AddRefs(hostRecord));
  if (NS_FAILED(rv)) {
    LOG(("Failed to get host record"));
    return;
  }

  mHostResolver->MaybeRenewHostRecord(hostRecord);

  uint32_t ttl = AddrInfo::NO_TTL_DATA;
  RefPtr<AddrInfo> ai(new AddrInfo(aSVCBRecord.mSvcDomainName, ResolverType(),
                                   TRRTYPE_A, std::move(addresses), ttl));

  // Since we're not actually calling NameLookup for this record, we need
  // to set these fields to avoid assertions in CompleteLookup.
  // This is quite hacky, and should be fixed.
  hostRecord->mResolving++;
  hostRecord->mEffectiveTRRMode = mRec->mEffectiveTRRMode;
  (void)mHostResolver->CompleteLookup(hostRecord, NS_OK, ai, mPB, mOriginSuffix,
                                      TRRSkippedReason::TRR_OK, this);
}

nsresult TRR::ReturnData(nsIChannel* aChannel) {
  if (mType != TRRTYPE_TXT && mType != TRRTYPE_HTTPSSVC) {
    // create and populate an AddrInfo instance to pass on
    RefPtr<AddrInfo> ai(new AddrInfo(mHost, ResolverType(), mType,
                                     nsTArray<NetAddr>(), mDNS.mTtl));
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
                                        mTRRSkippedReason, this);
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
  RecordReason(TRRSkippedReason::TRR_FAILED);

  if (mType == TRRTYPE_TXT || mType == TRRTYPE_HTTPSSVC) {
    TypeRecordResultType empty(Nothing{});
    (void)mHostResolver->CompleteLookupByType(mRec, error, empty, 0, mPB);
  } else {
    // create and populate an TRR AddrInfo instance to pass on to signal that
    // this comes from TRR
    nsTArray<NetAddr> noAddresses;
    RefPtr<AddrInfo> ai =
        new AddrInfo(mHost, ResolverType(), mType, std::move(noAddresses));

    (void)mHostResolver->CompleteLookup(mRec, error, ai, mPB, mOriginSuffix,
                                        mTRRSkippedReason, this);
  }

  mHostResolver = nullptr;
  mRec = nullptr;
  return NS_OK;
}

void TRR::HandleDecodeError(nsresult aStatusCode) {
  auto rcode = mPacket->GetRCode();
  if (rcode.isOk() && rcode.unwrap() != 0) {
    if (rcode.unwrap() == 0x03) {
      RecordReason(TRRSkippedReason::TRR_NXDOMAIN);
    } else {
      RecordReason(TRRSkippedReason::TRR_RCODE_FAIL);
    }
  } else if (aStatusCode == NS_ERROR_UNKNOWN_HOST ||
             aStatusCode == NS_ERROR_DEFINITIVE_UNKNOWN_HOST) {
    RecordReason(TRRSkippedReason::TRR_NO_ANSWERS);
  } else {
    RecordReason(TRRSkippedReason::TRR_DECODE_FAILED);
  }
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
    rv = GetOrCreateDNSPacket()->Decode(
        cname, mType, mCname, StaticPrefs::network_trr_allow_rfc1918(), mDNS,
        mResult, additionalRecords, mTTL);
    if (NS_FAILED(rv)) {
      LOG(("TRR::FollowCname DohDecode %x\n", (int)rv));
      HandleDecodeError(rv);
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
      ResolverType() == DNSResolverType::ODoH
          ? new ODoH(mHostResolver, mRec, mCname, mType, mCnameLoop, mPB)
          : new TRR(mHostResolver, mRec, mCname, mType, mCnameLoop, mPB);
  if (!TRRService::Get()) {
    return NS_ERROR_FAILURE;
  }
  return TRRService::Get()->DispatchTRRRequest(trr);
}

nsresult TRR::On200Response(nsIChannel* aChannel) {
  // decode body and create an AddrInfo struct for the response
  nsClassHashtable<nsCStringHashKey, DOHresp> additionalRecords;
  RefPtr<TypeHostRecord> typeRec = do_QueryObject(mRec);
  if (typeRec && typeRec->mOriginHost) {
    GetOrCreateDNSPacket()->SetOriginHost(typeRec->mOriginHost);
  }
  nsresult rv = GetOrCreateDNSPacket()->Decode(
      mHost, mType, mCname, StaticPrefs::network_trr_allow_rfc1918(), mDNS,
      mResult, additionalRecords, mTTL);
  if (NS_FAILED(rv)) {
    LOG(("TRR::On200Response DohDecode %x\n", (int)rv));
    HandleDecodeError(rv);
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

void TRR::RecordProcessingTime(nsIChannel* aChannel) {
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

void TRR::ReportStatus(nsresult aStatusCode) {
  // If the TRR was cancelled by nsHostResolver, then we don't need to report
  // it as failed; otherwise it can cause the confirmation to fail.
  if (UseDefaultServer() && aStatusCode != NS_ERROR_ABORT) {
    // Bad content is still considered "okay" if the HTTP response is okay
    TRRService::Get()->RecordTRRStatus(aStatusCode);
  }
}

static void RecordHttpVersion(nsIHttpChannel* aHttpChannel) {
  nsCOMPtr<nsIHttpChannelInternal> internalChannel =
      do_QueryInterface(aHttpChannel);
  if (!internalChannel) {
    LOG(("RecordHttpVersion: Failed to QI nsIHttpChannelInternal"));
    return;
  }

  uint32_t major, minor;
  if (NS_FAILED(internalChannel->GetResponseVersion(&major, &minor))) {
    LOG(("RecordHttpVersion: Failed to get protocol version"));
    return;
  }

  auto label = Telemetry::LABELS_DNS_TRR_HTTP_VERSION2::h_1;
  if (major == 2) {
    label = Telemetry::LABELS_DNS_TRR_HTTP_VERSION2::h_2;
  } else if (major == 3) {
    label = Telemetry::LABELS_DNS_TRR_HTTP_VERSION2::h_3;
  }

  Telemetry::AccumulateCategoricalKeyed(TRRService::ProviderKey(), label);

  LOG(("RecordHttpVersion: Provider responded using HTTP version: %d", major));
}

NS_IMETHODIMP
TRR::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  // The dtor will be run after the function returns
  LOG(("TRR:OnStopRequest %p %s %d failed=%d code=%X\n", this, mHost.get(),
       mType, mFailed, (unsigned int)aStatusCode));
  nsCOMPtr<nsIChannel> channel;
  channel.swap(mChannel);

  mChannelStatus = aStatusCode;

  {
    // Cancel the timer since we don't need it anymore.
    nsCOMPtr<nsITimer> timer;
    mTimeout.swap(timer);
    if (timer) {
      timer->Cancel();
    }
  }

  ReportStatus(aStatusCode);

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
        !contentType.LowerCaseEqualsASCII(ContentType())) {
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
        RecordReason(TRRSkippedReason::TRR_OK);
        RecordProcessingTime(channel);
        RecordHttpVersion(httpChannel);
        return rv;
      }
    } else {
      RecordReason(TRRSkippedReason::TRR_SERVER_RESPONSE_ERR);
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

  nsresult rv = GetOrCreateDNSPacket()->OnDataAvailable(aRequest, aInputStream,
                                                        aOffset, aCount);
  if (NS_FAILED(rv)) {
    LOG(("TRR::OnDataAvailable:%d fail\n", __LINE__));
    mFailed = true;
    return rv;
  }
  return NS_OK;
}

void TRR::Cancel(nsresult aStatus) {
  RefPtr<TRRServiceChannel> trrServiceChannel = do_QueryObject(mChannel);
  if (trrServiceChannel && !XRE_IsSocketProcess()) {
    if (TRRService::Get()) {
      nsCOMPtr<nsIThread> thread = TRRService::Get()->TRRThread();
      if (thread && !thread->IsOnCurrentThread()) {
        thread->Dispatch(NS_NewRunnableFunction(
            "TRR::Cancel",
            [self = RefPtr(this), aStatus]() { self->Cancel(aStatus); }));
        return;
      }
    }
  } else {
    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "TRR::Cancel",
          [self = RefPtr(this), aStatus]() { self->Cancel(aStatus); }));
      return;
    }
  }

  if (mCancelled) {
    return;
  }
  mCancelled = true;

  if (mChannel) {
    RecordReason(TRRSkippedReason::TRR_REQ_CANCELLED);
    LOG(("TRR: %p canceling Channel %p %s %d status=%" PRIx32 "\n", this,
         mChannel.get(), mHost.get(), mType, static_cast<uint32_t>(aStatus)));
    mChannel->Cancel(aStatus);
  }
}

bool TRR::UseDefaultServer() { return !mRec || mRec->mTrrServer.IsEmpty(); }

}  // namespace net
}  // namespace mozilla
