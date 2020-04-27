/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpLog.h"

#include "AlternateServices.h"
#include "LoadInfo.h"
#include "nsEscape.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsIOService.h"
#include "nsThreadUtils.h"
#include "nsHttpTransaction.h"
#include "nsISSLSocketControl.h"
#include "nsIWellKnownOpportunisticUtils.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/net/AltSvcTransactionParent.h"

/* RFC 7838 Alternative Services
   http://httpwg.org/http-extensions/opsec.html
    note that connections currently do not do mixed-scheme (the I attribute
    in the ConnectionInfo prevents it) but could, do not honor tls-commit and
   should not, and always require authentication
*/

namespace mozilla {
namespace net {

// function places true in outIsHTTPS if scheme is https, false if
// http, and returns an error if neither. originScheme passed into
// alternate service should already be normalized to those lower case
// strings by the URI parser (and so there is an assert)- this is an extra
// check.
static nsresult SchemeIsHTTPS(const nsACString& originScheme,
                              bool& outIsHTTPS) {
  outIsHTTPS = originScheme.EqualsLiteral("https");

  if (!outIsHTTPS && !originScheme.EqualsLiteral("http")) {
    MOZ_ASSERT(!originScheme.LowerCaseEqualsLiteral("https") &&
                   !originScheme.LowerCaseEqualsLiteral("http"),
               "The scheme should already be lowercase");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

bool AltSvcMapping::AcceptableProxy(nsProxyInfo* proxyInfo) {
  return !proxyInfo || proxyInfo->IsDirect() || proxyInfo->IsSOCKS();
}

void AltSvcMapping::ProcessHeader(
    const nsCString& buf, const nsCString& originScheme,
    const nsCString& originHost, int32_t originPort, const nsACString& username,
    const nsACString& topWindowOrigin, bool privateBrowsing, bool isolated,
    nsIInterfaceRequestor* callbacks, nsProxyInfo* proxyInfo, uint32_t caps,
    const OriginAttributes& originAttributes) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("AltSvcMapping::ProcessHeader: %s\n", buf.get()));

  if (StaticPrefs::network_http_altsvc_proxy_checks() &&
      !AcceptableProxy(proxyInfo)) {
    LOG(("AltSvcMapping::ProcessHeader ignoring due to proxy\n"));
    return;
  }

  bool isHTTPS;
  if (NS_FAILED(SchemeIsHTTPS(originScheme, isHTTPS))) {
    return;
  }
  if (!isHTTPS && !gHttpHandler->AllowAltSvcOE()) {
    LOG(("Alt-Svc Response Header for http:// origin but OE disabled\n"));
    return;
  }

  LOG(("Alt-Svc Response Header %s\n", buf.get()));
  ParsedHeaderValueListList parsedAltSvc(buf);
  int32_t numEntriesInHeader = parsedAltSvc.mValues.Length();

  for (uint32_t index = 0; index < parsedAltSvc.mValues.Length(); ++index) {
    uint32_t maxage = 86400;  // default
    nsAutoCString hostname;
    nsAutoCString npnToken;
    int32_t portno = originPort;
    bool clearEntry = false;
    bool isHttp3 = false;

    for (uint32_t pairIndex = 0;
         pairIndex < parsedAltSvc.mValues[index].mValues.Length();
         ++pairIndex) {
      nsDependentCSubstring& currentName =
          parsedAltSvc.mValues[index].mValues[pairIndex].mName;
      nsDependentCSubstring& currentValue =
          parsedAltSvc.mValues[index].mValues[pairIndex].mValue;

      if (!pairIndex) {
        if (currentName.EqualsLiteral("clear")) {
          clearEntry = true;
          --numEntriesInHeader;  // Only want to keep track of actual alt-svc
                                 // maps, not clearing
          break;
        }

        // h2=[hostname]:443 or h3=[hostname]:port;quic="ff0000XX"
        // or h3-xx=[hostname]:port
        // XX is current version we support and it is define in nsHttp.h.
        if (currentName.EqualsLiteral("h3")) {
          isHttp3 = true;
        } else if (currentName.Equals(kHttp3Version)) {
          isHttp3 = true;
          npnToken = kHttp3Version;
        } else {
          npnToken = currentName;
        }
        int32_t colonIndex = currentValue.FindChar(':');
        if (colonIndex >= 0) {
          portno =
              atoi(PromiseFlatCString(currentValue).get() + colonIndex + 1);
        } else {
          colonIndex = 0;
        }
        hostname.Assign(currentValue.BeginReading(), colonIndex);
      } else if (currentName.EqualsLiteral("ma")) {
        maxage = atoi(PromiseFlatCString(currentValue).get());
      } else if ((currentName.EqualsLiteral("quic")) && isHttp3) {
        LOG(("Alt Svc versions string: %s", currentValue.BeginReading()));
        for (const nsACString& ver : currentValue.Split(',')) {
          LOG(("Alt Svc versions %s", PromiseFlatCString(ver).get()));
          if (npnToken.IsEmpty()) {
            nsAutoCString version(ver);
            version.Trim(" \t");
            if (gHttpHandler->IsHttp3VersionSupportedHex(version)) {
              LOG(("Alt Svc found supported version: %s", version.get()));
              npnToken.Assign(gHttpHandler->Http3Version());
              break;
            }
          }
        }
      } else {
        LOG(("Alt Svc ignoring parameter %s", currentName.BeginReading()));
      }
    }

    if (clearEntry) {
      nsCString suffix;
      originAttributes.CreateSuffix(suffix);
      LOG(("Alt Svc clearing mapping for %s:%d:%s", originHost.get(),
           originPort, suffix.get()));
      gHttpHandler->AltServiceCache()->ClearHostMapping(
          originHost, originPort, originAttributes, topWindowOrigin);
      continue;
    }

    if (NS_FAILED(NS_CheckPortSafety(portno, originScheme.get()))) {
      LOG(("Alt Svc doesn't allow port %d, ignoring", portno));
      continue;
    }

    // unescape modifies a c string in place, so afterwards
    // update nsCString length
    nsUnescape(npnToken.BeginWriting());
    npnToken.SetLength(strlen(npnToken.BeginReading()));

    uint32_t spdyIndex;
    SpdyInformation* spdyInfo = gHttpHandler->SpdyInfo();
    if (!(NS_SUCCEEDED(spdyInfo->GetNPNIndex(npnToken, &spdyIndex)) &&
          spdyInfo->ProtocolEnabled(spdyIndex)) &&
        !(isHttp3 && gHttpHandler->IsHttp3Enabled() && !npnToken.IsEmpty())) {
      LOG(("Alt Svc unknown protocol %s, ignoring", npnToken.get()));
      continue;
    }

    RefPtr<AltSvcMapping> mapping = new AltSvcMapping(
        gHttpHandler->AltServiceCache()->GetStoragePtr(),
        gHttpHandler->AltServiceCache()->StorageEpoch(), originScheme,
        originHost, originPort, username, topWindowOrigin, privateBrowsing,
        isolated, NowInSeconds() + maxage, hostname, portno, npnToken,
        originAttributes, isHttp3);
    if (mapping->TTL() <= 0) {
      LOG(("Alt Svc invalid map"));
      mapping = nullptr;
      // since this isn't a parse error, let's clear any existing mapping
      // as that would have happened if we had accepted the parameters.
      gHttpHandler->AltServiceCache()->ClearHostMapping(
          originHost, originPort, originAttributes, topWindowOrigin);
    } else {
      gHttpHandler->UpdateAltServiceMapping(mapping, proxyInfo, callbacks, caps,
                                            originAttributes);
    }
  }

  if (numEntriesInHeader) {  // Ignore headers that were just "alt-svc: clear"
    Telemetry::Accumulate(Telemetry::HTTP_ALTSVC_ENTRIES_PER_HEADER,
                          numEntriesInHeader);
  }
}

AltSvcMapping::AltSvcMapping(
    DataStorage* storage, int32_t epoch, const nsACString& originScheme,
    const nsACString& originHost, int32_t originPort,
    const nsACString& username, const nsACString& topWindowOrigin,
    bool privateBrowsing, bool isolated, uint32_t expiresAt,
    const nsACString& alternateHost, int32_t alternatePort,
    const nsACString& npnToken, const OriginAttributes& originAttributes,
    bool aIsHttp3)
    : mStorage(storage),
      mStorageEpoch(epoch),
      mAlternateHost(alternateHost),
      mAlternatePort(alternatePort),
      mOriginHost(originHost),
      mOriginPort(originPort),
      mUsername(username),
      mTopWindowOrigin(topWindowOrigin),
      mPrivate(privateBrowsing),
      mIsolated(isolated),
      mExpiresAt(expiresAt),
      mValidated(false),
      mMixedScheme(false),
      mNPNToken(npnToken),
      mOriginAttributes(originAttributes),
      mSyncOnlyOnSuccess(false),
      mIsHttp3(aIsHttp3) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(SchemeIsHTTPS(originScheme, mHttps))) {
    LOG(("AltSvcMapping ctor %p invalid scheme\n", this));
    mExpiresAt = 0;  // invalid
  }

  if (mAlternatePort == -1) {
    mAlternatePort = mHttps ? NS_HTTPS_DEFAULT_PORT : NS_HTTP_DEFAULT_PORT;
  }
  if (mOriginPort == -1) {
    mOriginPort = mHttps ? NS_HTTPS_DEFAULT_PORT : NS_HTTP_DEFAULT_PORT;
  }

  LOG(("AltSvcMapping ctor %p %s://%s:%d to %s:%d\n", this,
       nsCString(originScheme).get(), mOriginHost.get(), mOriginPort,
       mAlternateHost.get(), mAlternatePort));

  if (mAlternateHost.IsEmpty()) {
    mAlternateHost = mOriginHost;
  }

  if ((mAlternatePort == mOriginPort) &&
      mAlternateHost.EqualsIgnoreCase(mOriginHost.get()) && !mIsHttp3) {
    // Http2 on the same host:port does not make sense because we are
    // connecting to the same end point over the same protocol (TCP) as with
    // original host. On the other hand, for Http3 alt-svc can be hosted on
    // the same host:port because protocol(UDP vs. TCP) is always different and
    // we are not connecting to the same end point.
    LOG(("Alt Svc is also origin Svc - ignoring\n"));
    mExpiresAt = 0;  // invalid
  }

  if (mExpiresAt) {
    MakeHashKey(mHashKey, originScheme, mOriginHost, mOriginPort, mPrivate,
                mIsolated, mTopWindowOrigin, mOriginAttributes);
  }
}

void AltSvcMapping::MakeHashKey(nsCString& outKey,
                                const nsACString& originScheme,
                                const nsACString& originHost,
                                int32_t originPort, bool privateBrowsing,
                                bool isolated,
                                const nsACString& topWindowOrigin,
                                const OriginAttributes& originAttributes) {
  outKey.Truncate();

  if (originPort == -1) {
    bool isHttps = originScheme.EqualsLiteral("https");
    originPort = isHttps ? NS_HTTPS_DEFAULT_PORT : NS_HTTP_DEFAULT_PORT;
  }

  outKey.Append(originScheme);
  outKey.Append(':');
  outKey.Append(originHost);
  outKey.Append(':');
  outKey.AppendInt(originPort);
  outKey.Append(':');
  outKey.Append(privateBrowsing ? 'P' : '.');
  outKey.Append(':');
  nsAutoCString suffix;
  originAttributes.CreateSuffix(suffix);
  outKey.Append(suffix);

  if (isolated) {
    outKey.Append(':');
    outKey.Append('I');
    outKey.Append(':');
    outKey.Append(topWindowOrigin);
    outKey.Append(
        '|');  // Be careful, the top window origin may contain colons!
  }
}

int32_t AltSvcMapping::TTL() { return mExpiresAt - NowInSeconds(); }

void AltSvcMapping::SyncString(const nsCString& str) {
  MOZ_ASSERT(NS_IsMainThread());
  mStorage->Put(HashKey(), str,
                mPrivate ? DataStorage_Private : DataStorage_Persistent);
}

void AltSvcMapping::Sync() {
  if (!mStorage) {
    return;
  }
  if (mSyncOnlyOnSuccess && !mValidated) {
    return;
  }
  nsCString value;
  Serialize(value);

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> r;
    r = NewRunnableMethod<nsCString>("net::AltSvcMapping::SyncString", this,
                                     &AltSvcMapping::SyncString, value);
    NS_DispatchToMainThread(r, NS_DISPATCH_NORMAL);
    return;
  }

  mStorage->Put(HashKey(), value,
                mPrivate ? DataStorage_Private : DataStorage_Persistent);
}

void AltSvcMapping::SetValidated(bool val) {
  mValidated = val;
  Sync();
}

void AltSvcMapping::SetMixedScheme(bool val) {
  mMixedScheme = val;
  Sync();
}

void AltSvcMapping::SetExpiresAt(int32_t val) {
  mExpiresAt = val;
  Sync();
}

void AltSvcMapping::SetExpired() {
  LOG(("AltSvcMapping SetExpired %p origin %s alternate %s\n", this,
       mOriginHost.get(), mAlternateHost.get()));
  mExpiresAt = NowInSeconds() - 1;
  Sync();
}

bool AltSvcMapping::RouteEquals(AltSvcMapping* map) {
  MOZ_ASSERT(map->mHashKey.Equals(mHashKey));
  return mAlternateHost.Equals(map->mAlternateHost) &&
         (mAlternatePort == map->mAlternatePort) &&
         mNPNToken.Equals(map->mNPNToken);
}

void AltSvcMapping::GetConnectionInfo(
    nsHttpConnectionInfo** outCI, nsProxyInfo* pi,
    const OriginAttributes& originAttributes) {
  RefPtr<nsHttpConnectionInfo> ci = new nsHttpConnectionInfo(
      mOriginHost, mOriginPort, mNPNToken, mUsername, mTopWindowOrigin, pi,
      originAttributes, mAlternateHost, mAlternatePort, mIsHttp3);

  // http:// without the mixed-scheme attribute needs to be segmented in the
  // connection manager connection information hash with this attribute
  if (!mHttps && !mMixedScheme) {
    ci->SetInsecureScheme(true);
  }
  ci->SetPrivate(mPrivate);
  ci->SetIsolated(mIsolated);
  ci.forget(outCI);
}

void AltSvcMapping::Serialize(nsCString& out) {
  // Be careful, when serializing new members, add them to the end of this list.
  out = mHttps ? NS_LITERAL_CSTRING("https:") : NS_LITERAL_CSTRING("http:");
  out.Append(mOriginHost);
  out.Append(':');
  out.AppendInt(mOriginPort);
  out.Append(':');
  out.Append(mAlternateHost);
  out.Append(':');
  out.AppendInt(mAlternatePort);
  out.Append(':');
  out.Append(mUsername);
  out.Append(':');
  out.Append(mPrivate ? 'y' : 'n');
  out.Append(':');
  out.AppendInt(mExpiresAt);
  out.Append(':');
  out.Append(mNPNToken);
  out.Append(':');
  out.Append(mValidated ? 'y' : 'n');
  out.Append(':');
  out.AppendInt(mStorageEpoch);
  out.Append(':');
  out.Append(mMixedScheme ? 'y' : 'n');
  out.Append(':');
  nsAutoCString suffix;
  mOriginAttributes.CreateSuffix(suffix);
  out.Append(suffix);
  out.Append(':');
  out.Append(mTopWindowOrigin);
  out.Append('|');  // Be careful, the top window origin may contain colons!
  out.Append(mIsolated ? 'y' : 'n');
  out.Append(':');
  out.Append(mIsHttp3 ? 'y' : 'n');
  out.Append(':');
  // Add code to serialize new members here!
}

AltSvcMapping::AltSvcMapping(DataStorage* storage, int32_t epoch,
                             const nsCString& str)
    : mStorage(storage),
      mStorageEpoch(epoch),
      mSyncOnlyOnSuccess(false),
      mIsHttp3(false) {
  mValidated = false;
  nsresult code;
  char separator = ':';

  // The the do {} while(0) loop acts like try/catch(e){} with the break in
  // _NS_NEXT_TOKEN
  do {
#ifdef _NS_NEXT_TOKEN
    COMPILER ERROR
#endif
#define _NS_NEXT_TOKEN                  \
  start = idx + 1;                      \
  idx = str.FindChar(separator, start); \
  if (idx < 0) break;
        int32_t start = 0;
    int32_t idx;
    idx = str.FindChar(separator, start);
    if (idx < 0) break;
    // Be careful, when deserializing new members, add them to the end of this
    // list.
    mHttps = Substring(str, start, idx - start).EqualsLiteral("https");
    _NS_NEXT_TOKEN;
    mOriginHost = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mOriginPort =
        nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mAlternateHost = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mAlternatePort =
        nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mUsername = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mPrivate = Substring(str, start, idx - start).EqualsLiteral("y");
    _NS_NEXT_TOKEN;
    mExpiresAt = nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mNPNToken = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mValidated = Substring(str, start, idx - start).EqualsLiteral("y");
    _NS_NEXT_TOKEN;
    mStorageEpoch =
        nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mMixedScheme = Substring(str, start, idx - start).EqualsLiteral("y");
    _NS_NEXT_TOKEN;
    Unused << mOriginAttributes.PopulateFromSuffix(
        Substring(str, start, idx - start));
    // The separator after the top window origin is a pipe character since the
    // origin string can contain colons.
    separator = '|';
    _NS_NEXT_TOKEN;
    mTopWindowOrigin = Substring(str, start, idx - start);
    separator = ':';
    _NS_NEXT_TOKEN;
    mIsolated = Substring(str, start, idx - start).EqualsLiteral("y");
    _NS_NEXT_TOKEN;
    mIsHttp3 = Substring(str, start, idx - start).EqualsLiteral("y");
    // Add code to deserialize new members here!
#undef _NS_NEXT_TOKEN

    MakeHashKey(
        mHashKey,
        mHttps ? NS_LITERAL_CSTRING("https") : NS_LITERAL_CSTRING("http"),
        mOriginHost, mOriginPort, mPrivate, mIsolated, mTopWindowOrigin,
        mOriginAttributes);
  } while (false);
}

AltSvcMappingValidator::AltSvcMappingValidator(AltSvcMapping* aMap)
    : mMapping(aMap) {
  LOG(("AltSvcMappingValidator ctor %p map %p [%s -> %s]", this, aMap,
       aMap->OriginHost().get(), aMap->AlternateHost().get()));
  MOZ_ASSERT(mMapping);
  MOZ_ASSERT(mMapping->HTTPS());  // http:// uses the .wk path
}

void AltSvcMappingValidator::OnTransactionDestroy(bool aValidateResult) {
  mMapping->SetValidated(aValidateResult);
  if (!mMapping->Validated()) {
    // try again later
    mMapping->SetExpiresAt(NowInSeconds() + 2);
  }
  LOG(
      ("AltSvcMappingValidator::OnTransactionDestroy %p map %p validated %d "
       "[%s]",
       this, mMapping.get(), mMapping->Validated(), mMapping->HashKey().get()));
}

void AltSvcMappingValidator::OnTransactionClose(bool aValidateResult) {
  mMapping->SetValidated(aValidateResult);
  LOG(
      ("AltSvcMappingValidator::OnTransactionClose %p map %p validated %d "
       "[%s]",
       this, mMapping.get(), mMapping->Validated(), mMapping->HashKey().get()));
}

template <class Validator>
AltSvcTransaction<Validator>::AltSvcTransaction(
    nsHttpConnectionInfo* ci, nsIInterfaceRequestor* callbacks, uint32_t caps,
    Validator* aValidator, bool aIsHttp3)
    : NullHttpTransaction(ci, callbacks, caps & ~NS_HTTP_ALLOW_KEEPALIVE),
      mValidator(aValidator),
      mIsHttp3(aIsHttp3),
      mRunning(true),
      mTriedToValidate(false),
      mTriedToWrite(false),
      mValidatedResult(false) {
  MOZ_ASSERT_IF(nsIOService::UseSocketProcess(), XRE_IsSocketProcess());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(), XRE_IsParentProcess());
}

template <class Validator>
AltSvcTransaction<Validator>::~AltSvcTransaction() {
  LOG(("AltSvcTransaction dtor %p running %d", this, mRunning));

  if (mRunning) {
    mValidatedResult = MaybeValidate(NS_OK);
    mValidator->OnTransactionDestroy(mValidatedResult);
  }
}

template <class Validator>
bool AltSvcTransaction<Validator>::MaybeValidate(nsresult reason) {
  if (mTriedToValidate) {
    return mValidatedResult;
  }
  mTriedToValidate = true;

  LOG(("AltSvcTransaction::MaybeValidate() %p reason=%" PRIx32
       " running=%d conn=%p write=%d",
       this, static_cast<uint32_t>(reason), mRunning, mConnection.get(),
       mTriedToWrite));

  if (mTriedToWrite && reason == NS_BASE_STREAM_CLOSED) {
    // The normal course of events is to cause the transaction to fail with
    // CLOSED on a write - so that's a success that means the HTTP/2 session
    // is setup.
    reason = NS_OK;
  }

  if (NS_FAILED(reason) || !mRunning || !mConnection) {
    LOG(("AltSvcTransaction::MaybeValidate %p Failed due to precondition",
         this));
    return false;
  }

  // insist on >= http/2
  HttpVersion version = mConnection->Version();
  LOG(("AltSvcTransaction::MaybeValidate() %p version %d\n", this,
       static_cast<int32_t>(version)));
  if ((!mIsHttp3 && (version != HttpVersion::v2_0)) ||
      (mIsHttp3 && (version != HttpVersion::v3_0))) {
    LOG(
        ("AltSvcTransaction::MaybeValidate %p Failed due to protocol version"
         " expacted %s.",
         this, mIsHttp3 ? "Http3" : "Http2"));
    return false;
  }

  nsCOMPtr<nsISupports> secInfo;
  mConnection->GetSecurityInfo(getter_AddRefs(secInfo));
  nsCOMPtr<nsISSLSocketControl> socketControl = do_QueryInterface(secInfo);

  LOG(("AltSvcTransaction::MaybeValidate() %p socketControl=%p\n", this,
       socketControl.get()));

  if (socketControl->GetFailedVerification()) {
    LOG(
        ("AltSvcTransaction::MaybeValidate() %p "
         "not validated due to auth error",
         this));
    return false;
  }

  LOG(
      ("AltSvcTransaction::MaybeValidate() %p "
       "validating alternate service with successful auth check",
       this));

  return true;
}

template <class Validator>
void AltSvcTransaction<Validator>::Close(nsresult reason) {
  LOG(("AltSvcTransaction::Close() %p reason=%" PRIx32 " running %d", this,
       static_cast<uint32_t>(reason), mRunning));

  mValidatedResult = MaybeValidate(reason);
  mValidator->OnTransactionClose(mValidatedResult);
  if (!mValidatedResult && mConnection) {
    mConnection->DontReuse();
  }
  NullHttpTransaction::Close(reason);
}

template <class Validator>
nsresult AltSvcTransaction<Validator>::ReadSegments(
    nsAHttpSegmentReader* reader, uint32_t count, uint32_t* countRead) {
  LOG(("AltSvcTransaction::ReadSegements() %p\n", this));
  mTriedToWrite = true;
  return NullHttpTransaction::ReadSegments(reader, count, countRead);
}

class WellKnownChecker {
 public:
  WellKnownChecker(nsIURI* uri, const nsCString& origin, uint32_t caps,
                   nsHttpConnectionInfo* ci, AltSvcMapping* mapping)
      : mWaiting(
            2)  // waiting for 2 channels (default and alternate) to complete
        ,
        mOrigin(origin),
        mAlternatePort(ci->RoutedPort()),
        mMapping(mapping),
        mCI(ci),
        mURI(uri),
        mCaps(caps) {
    LOG(("WellKnownChecker ctor %p\n", this));
    MOZ_ASSERT(!mMapping->HTTPS());
  }

  nsresult Start() {
    LOG(("WellKnownChecker::Start %p\n", this));
    nsCOMPtr<nsILoadInfo> loadInfo =
        new LoadInfo(nsContentUtils::GetSystemPrincipal(), nullptr, nullptr,
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
    loadInfo->SetOriginAttributes(mCI->GetOriginAttributes());

    RefPtr<nsHttpChannel> chan = new nsHttpChannel();
    nsresult rv;

    mTransactionAlternate = new TransactionObserver(chan, this);
    RefPtr<nsHttpConnectionInfo> newCI = mCI->Clone();
    rv = MakeChannel(chan, mTransactionAlternate, newCI, mURI, mCaps, loadInfo);
    if (NS_FAILED(rv)) {
      return rv;
    }
    chan = new nsHttpChannel();
    mTransactionOrigin = new TransactionObserver(chan, this);
    newCI = nullptr;
    return MakeChannel(chan, mTransactionOrigin, newCI, mURI, mCaps, loadInfo);
  }

  void Done(TransactionObserver* finished) {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(("WellKnownChecker::Done %p waiting for %d\n", this, mWaiting));

    mWaiting--;       // another channel is complete
    if (!mWaiting) {  // there are all complete!
      nsAutoCString mAlternateCT, mOriginCT;
      mTransactionOrigin->mChannel->GetContentType(mOriginCT);
      mTransactionAlternate->mChannel->GetContentType(mAlternateCT);
      nsCOMPtr<nsIWellKnownOpportunisticUtils> uu =
          do_CreateInstance(NS_WELLKNOWNOPPORTUNISTICUTILS_CONTRACTID);
      bool accepted = false;

      if (!mTransactionOrigin->mStatusOK) {
        LOG(("WellKnownChecker::Done %p origin was not 200 response code\n",
             this));
      } else if (!mTransactionAlternate->mAuthOK) {
        LOG(("WellKnownChecker::Done %p alternate was not TLS authenticated\n",
             this));
      } else if (!mTransactionAlternate->mStatusOK) {
        LOG(("WellKnownChecker::Done %p alternate was not 200 response code\n",
             this));
      } else if (!mTransactionAlternate->mVersionOK) {
        LOG(("WellKnownChecker::Done %p alternate was not at least h2 or h3\n",
             this));
      } else if (!mTransactionAlternate->mWKResponse.Equals(
                     mTransactionOrigin->mWKResponse)) {
        LOG(
            ("WellKnownChecker::Done %p alternate and origin "
             ".wk representations don't match\norigin: %s\alternate:%s\n",
             this, mTransactionOrigin->mWKResponse.get(),
             mTransactionAlternate->mWKResponse.get()));
      } else if (!mAlternateCT.Equals(mOriginCT)) {
        LOG(
            ("WellKnownChecker::Done %p alternate and origin content types "
             "dont match\n",
             this));
      } else if (!mAlternateCT.EqualsLiteral("application/json")) {
        LOG(("WellKnownChecker::Done %p .wk content type is %s\n", this,
             mAlternateCT.get()));
      } else if (!uu) {
        LOG(("WellKnownChecker::Done %p json parser service unavailable\n",
             this));
      } else {
        accepted = true;
      }

      if (accepted) {
        MOZ_ASSERT(!mMapping->HTTPS());  // https:// does not use .wk

        nsresult rv = uu->Verify(mTransactionAlternate->mWKResponse, mOrigin);
        if (NS_SUCCEEDED(rv)) {
          bool validWK = false;
          Unused << uu->GetValid(&validWK);
          if (!validWK) {
            LOG(("WellKnownChecker::Done %p json parser declares invalid\n%s\n",
                 this, mTransactionAlternate->mWKResponse.get()));
            accepted = false;
          }
        } else {
          LOG(("WellKnownChecker::Done %p .wk jason eval failed to run\n",
               this));
          accepted = false;
        }
      }

      MOZ_ASSERT(!mMapping->Validated());
      if (accepted) {
        LOG(("WellKnownChecker::Done %p Alternate for %s ACCEPTED\n", this,
             mOrigin.get()));
        mMapping->SetValidated(true);
      } else {
        LOG(("WellKnownChecker::Done %p Alternate for %s FAILED\n", this,
             mOrigin.get()));
        // try again soon
        mMapping->SetExpiresAt(NowInSeconds() + 2);
      }

      delete this;
    }
  }

  ~WellKnownChecker() { LOG(("WellKnownChecker dtor %p\n", this)); }

 private:
  nsresult MakeChannel(nsHttpChannel* chan, TransactionObserver* obs,
                       nsHttpConnectionInfo* ci, nsIURI* uri, uint32_t caps,
                       nsILoadInfo* loadInfo) {
    uint64_t channelId;
    nsLoadFlags flags;

    nsContentPolicyType contentPolicyType =
        loadInfo->GetExternalContentPolicyType();

    if (NS_FAILED(gHttpHandler->NewChannelId(channelId)) ||
        NS_FAILED(chan->Init(uri, caps, nullptr, 0, nullptr, channelId,
                             contentPolicyType)) ||
        NS_FAILED(chan->SetAllowAltSvc(false)) ||
        NS_FAILED(chan->SetRedirectMode(
            nsIHttpChannelInternal::REDIRECT_MODE_ERROR)) ||
        NS_FAILED(chan->SetLoadInfo(loadInfo)) ||
        NS_FAILED(chan->GetLoadFlags(&flags))) {
      return NS_ERROR_FAILURE;
    }
    flags |= HttpBaseChannel::LOAD_BYPASS_CACHE;
    if (NS_FAILED(chan->SetLoadFlags(flags))) {
      return NS_ERROR_FAILURE;
    }
    chan->SetTransactionObserver(obs);
    chan->SetConnectionInfo(ci);
    return chan->AsyncOpen(obs);
  }

  RefPtr<TransactionObserver> mTransactionAlternate;
  RefPtr<TransactionObserver> mTransactionOrigin;
  uint32_t mWaiting;  // semaphore
  nsCString mOrigin;
  int32_t mAlternatePort;
  RefPtr<AltSvcMapping> mMapping;
  RefPtr<nsHttpConnectionInfo> mCI;
  nsCOMPtr<nsIURI> mURI;
  uint32_t mCaps;
};

NS_IMPL_ISUPPORTS(TransactionObserver, nsIStreamListener)

TransactionObserver::TransactionObserver(nsHttpChannel* channel,
                                         WellKnownChecker* checker)
    : mChannel(channel),
      mChecker(checker),
      mRanOnce(false),
      mStatusOK(false),
      mAuthOK(false),
      mVersionOK(false) {
  LOG(("TransactionObserver ctor %p channel %p checker %p\n", this, channel,
       checker));
  mChannelRef = do_QueryInterface((nsIHttpChannel*)channel);
}

void TransactionObserver::Complete(bool versionOK, bool authOK,
                                   nsresult reason) {
  if (mRanOnce) {
    return;
  }
  mRanOnce = true;

  mVersionOK = versionOK;
  mAuthOK = authOK;

  LOG(
      ("TransactionObserve::Complete %p authOK %d versionOK %d"
       " reason %" PRIx32,
       this, authOK, versionOK, static_cast<uint32_t>(reason)));
}

#define MAX_WK 32768

NS_IMETHODIMP
TransactionObserver::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  // only consider the first 32KB.. because really.
  mWKResponse.SetCapacity(MAX_WK);
  return NS_OK;
}

NS_IMETHODIMP
TransactionObserver::OnDataAvailable(nsIRequest* aRequest,
                                     nsIInputStream* aStream, uint64_t aOffset,
                                     uint32_t aCount) {
  MOZ_ASSERT(NS_IsMainThread());
  uint32_t oldLen = mWKResponse.Length();
  uint64_t newLen = aCount + oldLen;
  if (newLen < MAX_WK) {
    nsresult rv;
    auto handle = mWKResponse.BulkWrite(newLen, oldLen, false, rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    uint32_t amtRead;
    if (NS_SUCCEEDED(
            aStream->Read(handle.Elements() + oldLen, aCount, &amtRead))) {
      MOZ_ASSERT(oldLen + amtRead <= newLen);
      handle.Finish(oldLen + amtRead, false);
      LOG(("TransactionObserver onDataAvailable %p read %d of .wk [%d]\n", this,
           amtRead, mWKResponse.Length()));
    } else {
      LOG(("TransactionObserver onDataAvailable %p read error\n", this));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
TransactionObserver::OnStopRequest(nsIRequest* aRequest, nsresult code) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("TransactionObserver onStopRequest %p code %" PRIx32 "\n", this,
       static_cast<uint32_t>(code)));
  if (NS_SUCCEEDED(code)) {
    nsHttpResponseHead* hdrs = mChannel->GetResponseHead();
    LOG(("TransactionObserver onStopRequest %p http resp %d\n", this,
         hdrs ? hdrs->Status() : -1));
    mStatusOK = hdrs && (hdrs->Status() == 200);
  }
  if (mChecker) {
    mChecker->Done(this);
  }
  return NS_OK;
}

void AltSvcCache::EnsureStorageInited() {
  if (mStorage) {
    return;
  }

  auto initTask = [&]() {
    MOZ_ASSERT(NS_IsMainThread());

    // DataStorage gives synchronous access to a memory based hash table
    // that is backed by disk where those writes are done asynchronously
    // on another thread
    mStorage = DataStorage::Get(DataStorageClass::AlternateServices);
    if (!mStorage) {
      LOG(("AltSvcCache::EnsureStorageInited WARN NO STORAGE\n"));
      return;
    }

    if (NS_FAILED(mStorage->Init(nullptr))) {
      mStorage = nullptr;
    }

    mStorageEpoch = NowInSeconds();
  };

  if (NS_IsMainThread()) {
    initTask();
    return;
  }

  nsCOMPtr<nsIEventTarget> main = GetMainThreadEventTarget();
  if (!main) {
    return;
  }

  SyncRunnable::DispatchToThread(
      main, new SyncRunnable(NS_NewRunnableFunction(
                "AltSvcCache::EnsureStorageInited", initTask)));
}

already_AddRefed<AltSvcMapping> AltSvcCache::LookupMapping(
    const nsCString& key, bool privateBrowsing) {
  LOG(("AltSvcCache::LookupMapping %p %s\n", this, key.get()));
  if (!mStorage) {
    LOG(("AltSvcCache::LookupMapping %p no backing store\n", this));
    return nullptr;
  }

  if (NS_IsMainThread() && !mStorage->IsReady()) {
    LOG(("AltSvcCache::LookupMapping %p skip when storage is not ready\n",
         this));
    return nullptr;
  }

  nsCString val(mStorage->Get(
      key, privateBrowsing ? DataStorage_Private : DataStorage_Persistent));
  if (val.IsEmpty()) {
    LOG(("AltSvcCache::LookupMapping %p MISS\n", this));
    return nullptr;
  }
  RefPtr<AltSvcMapping> rv = new AltSvcMapping(mStorage, mStorageEpoch, val);
  if (!rv->Validated() && (rv->StorageEpoch() != mStorageEpoch)) {
    // this was an in progress validation abandoned in a different session
    // rare edge case will not detect session change - that's ok as only impact
    // will be loss of alt-svc to this origin for this session.
    LOG(("AltSvcCache::LookupMapping %p invalid hit - MISS\n", this));
    mStorage->Remove(
        key, rv->Private() ? DataStorage_Private : DataStorage_Persistent);
    return nullptr;
  }

  if (rv->IsHttp3() && (!gHttpHandler->IsHttp3Enabled() ||
                        !rv->NPNToken().Equals(gHttpHandler->Http3Version()))) {
    // If Http3 is disabled or the version not supported anymore, remove the
    // mapping.
    mStorage->Remove(
        key, rv->Private() ? DataStorage_Private : DataStorage_Persistent);
    return nullptr;
  }

  if (rv->TTL() <= 0) {
    LOG(("AltSvcCache::LookupMapping %p expired hit - MISS\n", this));
    mStorage->Remove(
        key, rv->Private() ? DataStorage_Private : DataStorage_Persistent);
    return nullptr;
  }

  MOZ_ASSERT(rv->Private() == privateBrowsing);
  LOG(("AltSvcCache::LookupMapping %p HIT %p\n", this, rv.get()));
  return rv.forget();
}

void AltSvcCache::UpdateAltServiceMapping(
    AltSvcMapping* map, nsProxyInfo* pi, nsIInterfaceRequestor* aCallbacks,
    uint32_t caps, const OriginAttributes& originAttributes) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mStorage) {
    return;
  }
  RefPtr<AltSvcMapping> existing =
      LookupMapping(map->HashKey(), map->Private());
  LOG(
      ("AltSvcCache::UpdateAltServiceMapping %p map %p existing %p %s "
       "validated=%d",
       this, map, existing.get(), map->AlternateHost().get(),
       existing ? existing->Validated() : 0));

  if (existing && existing->Validated()) {
    if (existing->RouteEquals(map)) {
      // update expires in storage
      // if this is http:// then a ttl can only be extended via .wk, so ignore
      // this header path unless it is making things shorter
      if (existing->HTTPS()) {
        LOG(
            ("AltSvcCache::UpdateAltServiceMapping %p map %p updates ttl of "
             "%p\n",
             this, map, existing.get()));
        existing->SetExpiresAt(map->GetExpiresAt());
      } else {
        if (map->GetExpiresAt() < existing->GetExpiresAt()) {
          LOG(
              ("AltSvcCache::UpdateAltServiceMapping %p map %p reduces ttl of "
               "%p\n",
               this, map, existing.get()));
          existing->SetExpiresAt(map->GetExpiresAt());
        } else {
          LOG(
              ("AltSvcCache::UpdateAltServiceMapping %p map %p tries to extend "
               "%p but"
               " cannot as without .wk\n",
               this, map, existing.get()));
        }
      }
      Telemetry::Accumulate(Telemetry::HTTP_ALTSVC_MAPPING_CHANGED_TARGET,
                            false);
      return;
    }

    if (map->GetExpiresAt() < existing->GetExpiresAt()) {
      LOG(
          ("AltSvcCache::UpdateAltServiceMapping %p map %p ttl shorter than "
           "%p, ignoring",
           this, map, existing.get()));
      return;
    }

    // new alternate. start new validation
    LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p may overwrite %p\n",
         this, map, existing.get()));
    Telemetry::Accumulate(Telemetry::HTTP_ALTSVC_MAPPING_CHANGED_TARGET, true);
  }

  if (existing && !existing->Validated()) {
    LOG(
        ("AltSvcCache::UpdateAltServiceMapping %p map %p ignored because %p "
         "still in progress\n",
         this, map, existing.get()));
    return;
  }

  // start new validation, but don't overwrite a valid existing mapping unless
  // this completes successfully
  MOZ_ASSERT(!map->Validated());
  if (!existing) {
    map->Sync();
  } else {
    map->SetSyncOnlyOnSuccess(true);
  }

  RefPtr<nsHttpConnectionInfo> ci;
  map->GetConnectionInfo(getter_AddRefs(ci), pi, originAttributes);
  caps |= ci->GetAnonymous() ? NS_HTTP_LOAD_ANONYMOUS : 0;
  caps |= NS_HTTP_ERROR_SOFTLY;

  if (map->HTTPS()) {
    LOG(
        ("AltSvcCache::UpdateAltServiceMapping %p validation via "
         "speculative connect started\n",
         this));
    // for https resources we only establish a connection
    nsCOMPtr<nsIInterfaceRequestor> callbacks = new AltSvcOverride(aCallbacks);
    RefPtr<AltSvcMappingValidator> validator = new AltSvcMappingValidator(map);
    RefPtr<NullHttpTransaction> nullTransaction;
    if (nsIOService::UseSocketProcess()) {
      RefPtr<AltSvcTransactionParent> parent =
          new AltSvcTransactionParent(ci, aCallbacks, caps, validator);
      if (!parent->Init()) {
        return;
      }
      nullTransaction = parent;
    } else {
      nullTransaction = new AltSvcTransaction<AltSvcMappingValidator>(
          ci, aCallbacks, caps, validator, map->IsHttp3());
    }

    nsresult rv =
        gHttpHandler->SpeculativeConnect(ci, callbacks, caps, nullTransaction);
    if (NS_FAILED(rv)) {
      LOG(
          ("AltSvcCache::UpdateAltServiceMapping %p "
           "speculative connect failed with code %08x\n",
           this, static_cast<uint32_t>(rv)));
    }
  } else {
    // for http:// resources we fetch .well-known too
    nsAutoCString origin(NS_LITERAL_CSTRING("http://"));

    // Check whether origin is an ipv6 address. In that case we need to add
    // '[]'.
    if (map->OriginHost().FindChar(':') != kNotFound) {
      origin.Append('[');
      origin.Append(map->OriginHost());
      origin.Append(']');
    } else {
      origin.Append(map->OriginHost());
    }
    if (map->OriginPort() != NS_HTTP_DEFAULT_PORT) {
      origin.Append(':');
      origin.AppendInt(map->OriginPort());
    }

    nsCOMPtr<nsIURI> wellKnown;
    nsAutoCString uri(origin);
    uri.AppendLiteral("/.well-known/http-opportunistic");
    NS_NewURI(getter_AddRefs(wellKnown), uri);

    auto* checker = new WellKnownChecker(wellKnown, origin, caps, ci, map);
    if (NS_FAILED(checker->Start())) {
      LOG(
          ("AltSvcCache::UpdateAltServiceMapping %p .wk checker failed to "
           "start\n",
           this));
      map->SetExpired();
      delete checker;
      checker = nullptr;
    } else {
      // object deletes itself when done if started
      LOG(("AltSvcCache::UpdateAltServiceMapping %p .wk checker started %p\n",
           this, checker));
    }
  }
}

already_AddRefed<AltSvcMapping> AltSvcCache::GetAltServiceMapping(
    const nsACString& scheme, const nsACString& host, int32_t port,
    bool privateBrowsing, bool isolated, const nsACString& topWindowOrigin,
    const OriginAttributes& originAttributes, bool aHttp3Allowed) {
  EnsureStorageInited();

  bool isHTTPS;
  if (NS_FAILED(SchemeIsHTTPS(scheme, isHTTPS))) {
    return nullptr;
  }
  if (!gHttpHandler->AllowAltSvc()) {
    return nullptr;
  }
  if (!gHttpHandler->AllowAltSvcOE() && !isHTTPS) {
    return nullptr;
  }

  nsAutoCString key;
  AltSvcMapping::MakeHashKey(key, scheme, host, port, privateBrowsing, isolated,
                             topWindowOrigin, originAttributes);
  RefPtr<AltSvcMapping> existing = LookupMapping(key, privateBrowsing);
  LOG(
      ("AltSvcCache::GetAltServiceMapping %p key=%s "
       "existing=%p validated=%d ttl=%d",
       this, key.get(), existing.get(), existing ? existing->Validated() : 0,
       existing ? existing->TTL() : 0));
  if (existing && !existing->Validated()) {
    existing = nullptr;
  }

  if (existing && existing->IsHttp3() && !aHttp3Allowed) {
    existing = nullptr;
  }

  return existing.forget();
}

class ProxyClearHostMapping : public Runnable {
 public:
  explicit ProxyClearHostMapping(const nsACString& host, int32_t port,
                                 const OriginAttributes& originAttributes,
                                 const nsACString& topWindowOrigin)
      : Runnable("net::ProxyClearHostMapping"),
        mHost(host),
        mPort(port),
        mOriginAttributes(originAttributes),
        mTopWindowOrigin(topWindowOrigin) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    gHttpHandler->AltServiceCache()->ClearHostMapping(
        mHost, mPort, mOriginAttributes, mTopWindowOrigin);
    return NS_OK;
  }

 private:
  nsCString mHost;
  int32_t mPort;
  OriginAttributes mOriginAttributes;
  nsCString mTopWindowOrigin;
};

void AltSvcCache::ClearHostMapping(const nsACString& host, int32_t port,
                                   const OriginAttributes& originAttributes,
                                   const nsACString& topWindowOrigin) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> event = new ProxyClearHostMapping(
        host, port, originAttributes, topWindowOrigin);
    if (event) {
      NS_DispatchToMainThread(event);
    }
    return;
  }
  nsAutoCString key;
  for (int secure = 0; secure < 2; ++secure) {
    NS_NAMED_LITERAL_CSTRING(http, "http");
    NS_NAMED_LITERAL_CSTRING(https, "https");
    const nsLiteralCString& scheme = secure ? https : http;
    for (int pb = 1; pb >= 0; --pb) {
      for (int isolate = 0; isolate < 2; ++isolate) {
        AltSvcMapping::MakeHashKey(key, scheme, host, port, bool(pb),
                                   bool(isolate), topWindowOrigin,
                                   originAttributes);
        RefPtr<AltSvcMapping> existing = LookupMapping(key, bool(pb));
        if (existing) {
          existing->SetExpired();
        }
      }
    }
  }
}

void AltSvcCache::ClearHostMapping(nsHttpConnectionInfo* ci) {
  if (!ci->GetOrigin().IsEmpty()) {
    ClearHostMapping(ci->GetOrigin(), ci->OriginPort(),
                     ci->GetOriginAttributes(), ci->GetTopWindowOrigin());
  }
}

void AltSvcCache::ClearAltServiceMappings() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mStorage) {
    mStorage->Clear();
  }
}

nsresult AltSvcCache::GetAltSvcCacheKeys(nsTArray<nsCString>& value) {
  MOZ_ASSERT(NS_IsMainThread());
  if (gHttpHandler->AllowAltSvc() && mStorage) {
    nsTArray<mozilla::psm::DataStorageItem> items;
    mStorage->GetAll(&items);

    for (const auto& item : items) {
      value.AppendElement(item.key());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetInterface(const nsIID& iid, void** result) {
  if (NS_SUCCEEDED(QueryInterface(iid, result)) && *result) {
    return NS_OK;
  }

  if (mCallbacks) {
    return mCallbacks->GetInterface(iid, result);
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
AltSvcOverride::GetIgnoreIdle(bool* ignoreIdle) {
  *ignoreIdle = true;
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetParallelSpeculativeConnectLimit(
    uint32_t* parallelSpeculativeConnectLimit) {
  *parallelSpeculativeConnectLimit = 32;
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetIsFromPredictor(bool* isFromPredictor) {
  *isFromPredictor = false;
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetAllow1918(bool* allow) {
  // normally we don't do speculative connects to 1918.. and we use
  // speculative connects for the mapping validation, so override
  // that default here for alt-svc
  *allow = true;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(AltSvcOverride, nsIInterfaceRequestor,
                  nsISpeculativeConnectionOverrider)

}  // namespace net
}  // namespace mozilla
