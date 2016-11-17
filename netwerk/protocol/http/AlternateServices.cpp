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
#include "nsThreadUtils.h"
#include "nsHttpTransaction.h"
#include "NullHttpTransaction.h"
#include "nsISSLStatusProvider.h"
#include "nsISSLStatus.h"
#include "nsISSLSocketControl.h"
#include "nsIWellKnownOpportunisticUtils.h"

/* RFC 7838 Alternative Services
   http://httpwg.org/http-extensions/opsec.html
    note that connections currently do not do mixed-scheme (the I attribute
    in the ConnectionInfo prevents it) but could, do not honor tls-commit and should
    not, and always require authentication
*/

namespace mozilla {
namespace net {

// function places true in outIsHTTPS if scheme is https, false if
// http, and returns an error if neither. originScheme passed into
// alternate service should already be normalized to those lower case
// strings by the URI parser (and so there is an assert)- this is an extra check.
static nsresult
SchemeIsHTTPS(const nsACString &originScheme, bool &outIsHTTPS)
{
  outIsHTTPS = originScheme.Equals(NS_LITERAL_CSTRING("https"));

  if (!outIsHTTPS && !originScheme.Equals(NS_LITERAL_CSTRING("http"))) {
      MOZ_ASSERT(false, "unexpected scheme");
      return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

void
AltSvcMapping::ProcessHeader(const nsCString &buf, const nsCString &originScheme,
                             const nsCString &originHost, int32_t originPort,
                             const nsACString &username, bool privateBrowsing,
                             nsIInterfaceRequestor *callbacks, nsProxyInfo *proxyInfo,
                             uint32_t caps, const NeckoOriginAttributes &originAttributes)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("AltSvcMapping::ProcessHeader: %s\n", buf.get()));
  if (!callbacks) {
    return;
  }

  if (proxyInfo && !proxyInfo->IsDirect()) {
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

  for (uint32_t index = 0; index < parsedAltSvc.mValues.Length(); ++index) {
    uint32_t maxage = 86400; // default
    nsAutoCString hostname;
    nsAutoCString npnToken;
    int32_t portno = originPort;
    bool clearEntry = false;

    for (uint32_t pairIndex = 0;
         pairIndex < parsedAltSvc.mValues[index].mValues.Length();
         ++pairIndex) {
      nsDependentCSubstring &currentName =
        parsedAltSvc.mValues[index].mValues[pairIndex].mName;
      nsDependentCSubstring &currentValue =
        parsedAltSvc.mValues[index].mValues[pairIndex].mValue;

      if (!pairIndex) {
        if (currentName.Equals(NS_LITERAL_CSTRING("clear"))) {
          clearEntry = true;
          break;
        }

        // h2=[hostname]:443
        npnToken = currentName;
        int32_t colonIndex = currentValue.FindChar(':');
        if (colonIndex >= 0) {
          portno =
            atoi(PromiseFlatCString(currentValue).get() + colonIndex + 1);
        } else {
          colonIndex = 0;
        }
        hostname.Assign(currentValue.BeginReading(), colonIndex);
      } else if (currentName.Equals(NS_LITERAL_CSTRING("ma"))) {
        maxage = atoi(PromiseFlatCString(currentValue).get());
        break;
      } else {
        LOG(("Alt Svc ignoring parameter %s", currentName.BeginReading()));
      }
    }

    if (clearEntry) {
      LOG(("Alt Svc clearing mapping for %s:%d", originHost.get(), originPort));
      gHttpHandler->ConnMgr()->ClearHostMapping(originHost, originPort);
      continue;
    }

    // unescape modifies a c string in place, so afterwards
    // update nsCString length
    nsUnescape(npnToken.BeginWriting());
    npnToken.SetLength(strlen(npnToken.BeginReading()));

    uint32_t spdyIndex;
    SpdyInformation *spdyInfo = gHttpHandler->SpdyInfo();
    if (!(NS_SUCCEEDED(spdyInfo->GetNPNIndex(npnToken, &spdyIndex)) &&
          spdyInfo->ProtocolEnabled(spdyIndex))) {
      LOG(("Alt Svc unknown protocol %s, ignoring", npnToken.get()));
      continue;
    }

    RefPtr<AltSvcMapping> mapping = new AltSvcMapping(gHttpHandler->ConnMgr()->GetStoragePtr(),
                                                      gHttpHandler->ConnMgr()->StorageEpoch(),
                                                      originScheme,
                                                      originHost, originPort,
                                                      username, privateBrowsing,
                                                      NowInSeconds() + maxage,
                                                      hostname, portno, npnToken);
    if (mapping->TTL() <= 0) {
      LOG(("Alt Svc invalid map"));
      mapping = nullptr;
      // since this isn't a parse error, let's clear any existing mapping
      // as that would have happened if we had accepted the parameters.
      gHttpHandler->ConnMgr()->ClearHostMapping(originHost, originPort);
    } else {
      gHttpHandler->UpdateAltServiceMapping(mapping, proxyInfo, callbacks, caps,
                                            originAttributes);
    }
  }
}

AltSvcMapping::AltSvcMapping(DataStorage *storage, int32_t epoch,
                             const nsACString &originScheme,
                             const nsACString &originHost,
                             int32_t originPort,
                             const nsACString &username,
                             bool privateBrowsing,
                             uint32_t expiresAt,
                             const nsACString &alternateHost,
                             int32_t alternatePort,
                             const nsACString &npnToken)
  : mStorage(storage)
  , mStorageEpoch(epoch)
  , mAlternateHost(alternateHost)
  , mAlternatePort(alternatePort)
  , mOriginHost(originHost)
  , mOriginPort(originPort)
  , mUsername(username)
  , mPrivate(privateBrowsing)
  , mExpiresAt(expiresAt)
  , mValidated(false)
  , mMixedScheme(false)
  , mNPNToken(npnToken)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(SchemeIsHTTPS(originScheme, mHttps))) {
    LOG(("AltSvcMapping ctor %p invalid scheme\n", this));
    mExpiresAt = 0; // invalid
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
      mAlternateHost.EqualsIgnoreCase(mOriginHost.get())) {
    LOG(("Alt Svc is also origin Svc - ignoring\n"));
    mExpiresAt = 0; // invalid
  }

  if (mExpiresAt) {
    MakeHashKey(mHashKey, originScheme, mOriginHost, mOriginPort, mPrivate);
  }
}

void
AltSvcMapping::MakeHashKey(nsCString &outKey,
                           const nsACString &originScheme,
                           const nsACString &originHost,
                           int32_t originPort,
                           bool privateBrowsing)
{
  outKey.Truncate();

  if (originPort == -1) {
    bool isHttps = originScheme.Equals("https");
    originPort = isHttps ? NS_HTTPS_DEFAULT_PORT : NS_HTTP_DEFAULT_PORT;
  }

  outKey.Append(originScheme);
  outKey.Append(':');
  outKey.Append(originHost);
  outKey.Append(':');
  outKey.AppendInt(originPort);
  outKey.Append(':');
  outKey.Append(privateBrowsing ? 'P' : '.');
}

int32_t
AltSvcMapping::TTL()
{
  return mExpiresAt - NowInSeconds();
}

void
AltSvcMapping::SyncString(const nsCString& str)
{
  MOZ_ASSERT(NS_IsMainThread());
  mStorage->Put(HashKey(), str,
                mPrivate ? DataStorage_Private : DataStorage_Persistent);
}

void
AltSvcMapping::Sync()
{
  if (!mStorage) {
    return;
  }
  nsCString value;
  Serialize(value);

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> r;
    r = NewRunnableMethod<nsCString>(this,
                                     &AltSvcMapping::SyncString,
                                     value);
    NS_DispatchToMainThread(r, NS_DISPATCH_NORMAL);
    return;
  }

  mStorage->Put(HashKey(), value,
                mPrivate ? DataStorage_Private : DataStorage_Persistent);
}

void
AltSvcMapping::SetValidated(bool val)
{
  mValidated = val;
  Sync();
}

void
AltSvcMapping::SetMixedScheme(bool val)
{
  mMixedScheme = val;
  Sync();
}

void
AltSvcMapping::SetExpiresAt(int32_t val)
{
  mExpiresAt = val;
  Sync();
}

void
AltSvcMapping::SetExpired()
{
  LOG(("AltSvcMapping SetExpired %p origin %s alternate %s\n", this,
       mOriginHost.get(), mAlternateHost.get()));
  mExpiresAt = NowInSeconds() - 1;
  Sync();
}

bool
AltSvcMapping::RouteEquals(AltSvcMapping *map)
{
  MOZ_ASSERT(map->mHashKey.Equals(mHashKey));
  return mAlternateHost.Equals(map->mAlternateHost) &&
    (mAlternatePort == map->mAlternatePort) &&
    mNPNToken.Equals(map->mNPNToken);
}

void
AltSvcMapping::GetConnectionInfo(nsHttpConnectionInfo **outCI,
                                 nsProxyInfo *pi,
                                 const NeckoOriginAttributes &originAttributes)
{
  RefPtr<nsHttpConnectionInfo> ci =
    new nsHttpConnectionInfo(mOriginHost, mOriginPort, mNPNToken,
                             mUsername, pi, originAttributes,
                             mAlternateHost, mAlternatePort);

  // http:// without the mixed-scheme attribute needs to be segmented in the
  // connection manager connection information hash with this attribute
  if (!mHttps && !mMixedScheme) {
    ci->SetInsecureScheme(true);
  }
  ci->SetPrivate(mPrivate);
  ci.forget(outCI);
}

void
AltSvcMapping::Serialize(nsCString &out)
{
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
}

AltSvcMapping::AltSvcMapping(DataStorage *storage, int32_t epoch, const nsCString &str)
  : mStorage(storage)
  , mStorageEpoch(epoch)
{
  mValidated = false;
  nsresult code;

  // The the do {} while(0) loop acts like try/catch(e){} with the break in _NS_NEXT_TOKEN
  do {
#ifdef _NS_NEXT_TOKEN
COMPILER ERROR
#endif
    #define _NS_NEXT_TOKEN start = idx + 1; idx = str.FindChar(':', start); if (idx < 0) break;
    int32_t start = 0;
    int32_t idx;
    idx = str.FindChar(':', start); if (idx < 0) break;
    mHttps = Substring(str, start, idx - start).Equals(NS_LITERAL_CSTRING("https"));
    _NS_NEXT_TOKEN;
    mOriginHost = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mOriginPort = nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mAlternateHost = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mAlternatePort = nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mUsername = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mPrivate = Substring(str, start, idx - start).Equals(NS_LITERAL_CSTRING("y"));
    _NS_NEXT_TOKEN;
    mExpiresAt = nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mNPNToken = Substring(str, start, idx - start);
    _NS_NEXT_TOKEN;
    mValidated = Substring(str, start, idx - start).Equals(NS_LITERAL_CSTRING("y"));
    _NS_NEXT_TOKEN;
    mStorageEpoch = nsCString(Substring(str, start, idx - start)).ToInteger(&code);
    _NS_NEXT_TOKEN;
    mMixedScheme = Substring(str, start, idx - start).Equals(NS_LITERAL_CSTRING("y"));
    #undef _NS_NEXT_TOKEN

    MakeHashKey(mHashKey, mHttps ? NS_LITERAL_CSTRING("https") : NS_LITERAL_CSTRING("http"),
                mOriginHost, mOriginPort, mPrivate);
  } while (false);
}

// This is the asynchronous null transaction used to validate
// an alt-svc advertisement only for https://
class AltSvcTransaction final : public NullHttpTransaction
{
public:
    AltSvcTransaction(AltSvcMapping *map,
                      nsHttpConnectionInfo *ci,
                      nsIInterfaceRequestor *callbacks,
                      uint32_t caps)
    : NullHttpTransaction(ci, callbacks, caps & ~NS_HTTP_ALLOW_KEEPALIVE)
    , mMapping(map)
    , mRunning(true)
    , mTriedToValidate(false)
    , mTriedToWrite(false)
  {
    LOG(("AltSvcTransaction ctor %p map %p [%s -> %s]",
         this, map, map->OriginHost().get(), map->AlternateHost().get()));
    MOZ_ASSERT(mMapping);
    MOZ_ASSERT(mMapping->HTTPS());
  }

  ~AltSvcTransaction() override
  {
    LOG(("AltSvcTransaction dtor %p map %p running %d",
         this, mMapping.get(), mRunning));

    if (mRunning) {
      MaybeValidate(NS_OK);
    }
    if (!mMapping->Validated()) {
      // try again later
      mMapping->SetExpiresAt(NowInSeconds() + 2);
    }
    LOG(("AltSvcTransaction dtor %p map %p validated %d [%s]",
         this, mMapping.get(), mMapping->Validated(),
         mMapping->HashKey().get()));
  }

private:
  // check on alternate route.
  // also evaluate 'reasonable assurances' for opportunistic security
  void MaybeValidate(nsresult reason)
  {
    MOZ_ASSERT(mMapping->HTTPS()); // http:// uses the .wk path

    if (mTriedToValidate) {
      return;
    }
    mTriedToValidate = true;

    LOG(("AltSvcTransaction::MaybeValidate() %p reason=%x running=%d conn=%p write=%d",
         this, reason, mRunning, mConnection.get(), mTriedToWrite));

    if (mTriedToWrite && reason == NS_BASE_STREAM_CLOSED) {
      // The normal course of events is to cause the transaction to fail with CLOSED
      // on a write - so that's a success that means the HTTP/2 session is setup.
      reason = NS_OK;
    }

    if (NS_FAILED(reason) || !mRunning || !mConnection) {
      LOG(("AltSvcTransaction::MaybeValidate %p Failed due to precondition", this));
      return;
    }

    // insist on >= http/2
    uint32_t version = mConnection->Version();
    LOG(("AltSvcTransaction::MaybeValidate() %p version %d\n", this, version));
    if (version != HTTP_VERSION_2) {
      LOG(("AltSvcTransaction::MaybeValidate %p Failed due to protocol version", this));
      return;
    }

    nsCOMPtr<nsISupports> secInfo;
    mConnection->GetSecurityInfo(getter_AddRefs(secInfo));
    nsCOMPtr<nsISSLSocketControl> socketControl = do_QueryInterface(secInfo);

    LOG(("AltSvcTransaction::MaybeValidate() %p socketControl=%p\n",
         this, socketControl.get()));

    if (socketControl->GetFailedVerification()) {
      LOG(("AltSvcTransaction::MaybeValidate() %p "
           "not validated due to auth error", this));
      return;
    }

    LOG(("AltSvcTransaction::MaybeValidate() %p "
         "validating alternate service with successful auth check", this));
    mMapping->SetValidated(true);
  }

public:
  void Close(nsresult reason) override
  {
    LOG(("AltSvcTransaction::Close() %p reason=%x running %d",
         this, reason, mRunning));

    MaybeValidate(reason);
    if (!mMapping->Validated() && mConnection) {
      mConnection->DontReuse();
    }
    NullHttpTransaction::Close(reason);
  }

  nsresult ReadSegments(nsAHttpSegmentReader *reader,
                        uint32_t count, uint32_t *countRead) override
  {
    LOG(("AltSvcTransaction::ReadSegements() %p\n"));
    mTriedToWrite = true;
    return NullHttpTransaction::ReadSegments(reader, count, countRead);
  }

private:
  RefPtr<AltSvcMapping>   mMapping;
  uint32_t                mRunning : 1;
  uint32_t                mTriedToValidate : 1;
  uint32_t                mTriedToWrite : 1;
};

class WellKnownChecker
{
public:
  WellKnownChecker(nsIURI *uri, const nsCString &origin, uint32_t caps, nsHttpConnectionInfo *ci, AltSvcMapping *mapping)
    : mWaiting(2) // waiting for 2 channels (default and alternate) to complete
    , mOrigin(origin)
    , mAlternatePort(ci->RoutedPort())
    , mMapping(mapping)
    , mCI(ci)
    , mURI(uri)
    , mCaps(caps)
  {
    LOG(("WellKnownChecker ctor %p\n", this));
    MOZ_ASSERT(!mMapping->HTTPS());
  }

  nsresult Start()
  {
    LOG(("WellKnownChecker::Start %p\n", this));
    nsCOMPtr<nsILoadInfo> loadInfo = new LoadInfo(nsContentUtils::GetSystemPrincipal(),
                                                  nullptr, nullptr,
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

  void Done(TransactionObserver *finished)
  {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(("WellKnownChecker::Done %p waiting for %d\n", this, mWaiting));

    mWaiting--; // another channel is complete
    if (!mWaiting) { // there are all complete!
      nsAutoCString mAlternateCT, mOriginCT;
      mTransactionOrigin->mChannel->GetContentType(mOriginCT);
      mTransactionAlternate->mChannel->GetContentType(mAlternateCT);
      nsCOMPtr<nsIWellKnownOpportunisticUtils> uu = do_CreateInstance(NS_WELLKNOWNOPPORTUNISTICUTILS_CONTRACTID);
      bool accepted = false;

      if (!mTransactionOrigin->mStatusOK) {
        LOG(("WellKnownChecker::Done %p origin was not 200 response code\n", this));
      } else if (!mTransactionAlternate->mAuthOK) {
        LOG(("WellKnownChecker::Done %p alternate was not TLS authenticated\n", this));
      } else if (!mTransactionAlternate->mStatusOK) {
        LOG(("WellKnownChecker::Done %p alternate was not 200 response code\n", this));
      } else if (!mTransactionAlternate->mVersionOK) {
        LOG(("WellKnownChecker::Done %p alternate was not at least h2\n", this));
      } else if (!mTransactionAlternate->mWKResponse.Equals(mTransactionOrigin->mWKResponse)) {
        LOG(("WellKnownChecker::Done %p alternate and origin "
             ".wk representations don't match\norigin: %s\alternate:%s\n", this,
             mTransactionOrigin->mWKResponse.get(),
             mTransactionAlternate->mWKResponse.get()));
      } else if (!mAlternateCT.Equals(mOriginCT)) {
        LOG(("WellKnownChecker::Done %p alternate and origin content types dont match\n", this));
      } else if (!mAlternateCT.Equals(NS_LITERAL_CSTRING("application/json"))) {
        LOG(("WellKnownChecker::Done %p .wk content type is %s\n", this, mAlternateCT.get()));
      } else if (!uu) {
        LOG(("WellKnownChecker::Done %p json parser service unavailable\n", this));
      } else {
        accepted = true;
      }

      if (accepted) {
        MOZ_ASSERT(!mMapping->HTTPS()); // https:// does not use .wk

        nsresult rv = uu->Verify(mTransactionAlternate->mWKResponse, mOrigin, mAlternatePort);
        if (NS_SUCCEEDED(rv)) {
          bool validWK = false;
          bool mixedScheme = false;
          int32_t lifetime = 0;
          uu->GetValid(&validWK);
          uu->GetLifetime(&lifetime);
          uu->GetMixed(&mixedScheme);
          if (!validWK) {
            LOG(("WellKnownChecker::Done %p json parser declares invalid\n%s\n", this, mTransactionAlternate->mWKResponse.get()));
            accepted = false;
          }
          if (accepted && (lifetime > 0)) {
            if (mMapping->TTL() > lifetime) {
              LOG(("WellKnownChecker::Done %p atl-svc lifetime reduced by .wk\n", this));
              mMapping->SetExpiresAt(NowInSeconds() + lifetime);
            } else {
              LOG(("WellKnownChecker::Done %p .wk lifetime exceeded alt-svc ma so ignored\n", this));
            }
          }
          if (accepted && mixedScheme) {
            mMapping->SetMixedScheme(true);
            LOG(("WellKnownChecker::Done %p atl-svc .wk allows mixed scheme\n", this));
          }
        } else {
          LOG(("WellKnownChecker::Done %p .wk jason eval failed to run\n", this));
          accepted = false;
        }
      }

      MOZ_ASSERT(!mMapping->Validated());
      if (accepted) {
        LOG(("WellKnownChecker::Done %p Alternate for %s ACCEPTED\n", this, mOrigin.get()));
        mMapping->SetValidated(true);
      } else {
        LOG(("WellKnownChecker::Done %p Alternate for %s FAILED\n", this, mOrigin.get()));
        // try again soon
        mMapping->SetExpiresAt(NowInSeconds() + 2);
      }

      delete this;
    }
  }

  ~WellKnownChecker()
  {
    LOG(("WellKnownChecker dtor %p\n", this));
  }

private:
  nsresult
  MakeChannel(nsHttpChannel *chan, TransactionObserver *obs, nsHttpConnectionInfo *ci,
              nsIURI *uri, uint32_t caps, nsILoadInfo *loadInfo)
  {
    nsID channelId;
    nsLoadFlags flags;
    if (NS_FAILED(gHttpHandler->NewChannelId(&channelId)) ||
        NS_FAILED(chan->Init(uri, caps, nullptr, 0, nullptr, channelId)) ||
        NS_FAILED(chan->SetAllowAltSvc(false)) ||
        NS_FAILED(chan->SetRedirectMode(nsIHttpChannelInternal::REDIRECT_MODE_ERROR)) ||
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
    return chan->AsyncOpen2(obs);
  }

  RefPtr<TransactionObserver>  mTransactionAlternate;
  RefPtr<TransactionObserver>  mTransactionOrigin;
  uint32_t                     mWaiting; // semaphore
  nsCString                    mOrigin;
  int32_t                      mAlternatePort;
  RefPtr<AltSvcMapping>        mMapping;
  RefPtr<nsHttpConnectionInfo> mCI;
  nsCOMPtr<nsIURI>             mURI;
  uint32_t                     mCaps;
};

NS_IMPL_ISUPPORTS(TransactionObserver, nsIStreamListener)

TransactionObserver::TransactionObserver(nsHttpChannel *channel, WellKnownChecker *checker)
  : mChannel(channel)
  , mChecker(checker)
  , mRanOnce(false)
  , mAuthOK(false)
  , mVersionOK(false)
  , mStatusOK(false)
{
  LOG(("TransactionObserver ctor %p channel %p checker %p\n", this, channel, checker));
  mChannelRef = do_QueryInterface((nsIHttpChannel *)channel);
}

void
TransactionObserver::Complete(nsHttpTransaction *aTrans, nsresult reason)
{
  // socket thread
  MOZ_ASSERT(!NS_IsMainThread());
  if (mRanOnce) {
    return;
  }
  mRanOnce = true;

  RefPtr<nsAHttpConnection> conn = aTrans->GetConnectionReference();
  LOG(("TransactionObserver::Complete %p aTrans %p reason %x conn %p\n",
       this, aTrans, reason, conn.get()));
  if (!conn) {
    return;
  }
  uint32_t version = conn->Version();
  mVersionOK = (((reason == NS_BASE_STREAM_CLOSED) || (reason == NS_OK)) &&
                conn->Version() == HTTP_VERSION_2);

  nsCOMPtr<nsISupports> secInfo;
  conn->GetSecurityInfo(getter_AddRefs(secInfo));
  nsCOMPtr<nsISSLSocketControl> socketControl = do_QueryInterface(secInfo);
  LOG(("TransactionObserver::Complete version %u socketControl %p\n",
       version, socketControl.get()));
  if (!socketControl) {
    return;
  }

  mAuthOK = !socketControl->GetFailedVerification();
  LOG(("TransactionObserve::Complete %p trans %p authOK %d versionOK %d\n",
       this, aTrans, mAuthOK, mVersionOK));
}

#define MAX_WK 32768

NS_IMETHODIMP
TransactionObserver::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  // only consider the first 32KB.. because really.
  mWKResponse.SetCapacity(MAX_WK);
  return NS_OK;
}

NS_IMETHODIMP
TransactionObserver::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                     nsIInputStream *aStream, uint64_t aOffset, uint32_t aCount)
{
  MOZ_ASSERT(NS_IsMainThread());
  uint64_t newLen = aCount + mWKResponse.Length();
  if (newLen < MAX_WK) {
    char *startByte =  reinterpret_cast<char *>(mWKResponse.BeginWriting()) + mWKResponse.Length();
    uint32_t amtRead;
    if (NS_SUCCEEDED(aStream->Read(startByte, aCount, &amtRead))) {
      MOZ_ASSERT(mWKResponse.Length() + amtRead < MAX_WK);
      mWKResponse.SetLength(mWKResponse.Length() + amtRead);
      LOG(("TransactionObserver onDataAvailable %p read %d of .wk [%d]\n",
           this, amtRead, mWKResponse.Length()));
    } else {
      LOG(("TransactionObserver onDataAvailable %p read error\n", this));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
TransactionObserver::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext, nsresult code)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG(("TransactionObserver onStopRequest %p code %x\n", this, code));
  if (NS_SUCCEEDED(code)) {
    nsHttpResponseHead *hdrs = mChannel->GetResponseHead();
    LOG(("TransactionObserver onStopRequest %p http resp %d\n",
         this, hdrs ? hdrs->Status() : -1));
    mStatusOK = hdrs && (hdrs->Status() == 200);
  }
  if (mChecker) {
    mChecker->Done(this);
  }
  return NS_OK;
}

already_AddRefed<AltSvcMapping>
AltSvcCache::LookupMapping(const nsCString &key, bool privateBrowsing)
{
  LOG(("AltSvcCache::LookupMapping %p %s\n", this, key.get()));
  if (!mStorage) {
    LOG(("AltSvcCache::LookupMapping %p no backing store\n", this));
    return nullptr;
  }
  nsCString val(mStorage->Get(key,
                              privateBrowsing ? DataStorage_Private : DataStorage_Persistent));
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
    mStorage->Remove(key,
                     rv->Private() ? DataStorage_Private : DataStorage_Persistent);
    return nullptr;
  }

  if (rv->TTL() <= 0) {
    LOG(("AltSvcCache::LookupMapping %p expired hit - MISS\n", this));
    mStorage->Remove(key,
                     rv->Private() ? DataStorage_Private : DataStorage_Persistent);
    return nullptr;
  }

  MOZ_ASSERT(rv->Private() == privateBrowsing);
  LOG(("AltSvcCache::LookupMapping %p HIT %p\n", this, rv.get()));
  return rv.forget();
}

void
AltSvcCache::UpdateAltServiceMapping(AltSvcMapping *map, nsProxyInfo *pi,
                                     nsIInterfaceRequestor *aCallbacks,
                                     uint32_t caps,
                                     const NeckoOriginAttributes &originAttributes)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mStorage) {
    return;
  }
  RefPtr<AltSvcMapping> existing = LookupMapping(map->HashKey(), map->Private());
  LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p existing %p %s validated=%d",
       this, map, existing.get(), map->AlternateHost().get(),
       existing ? existing->Validated() : 0));

  if (existing && existing->Validated()) {
    if (existing->RouteEquals(map)){
      // update expires in storage
      // if this is http:// then a ttl can only be extended via .wk, so ignore this
      // header path unless it is making things shorter
      if (existing->HTTPS()) {
        LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p updates ttl of %p\n",
             this, map, existing.get()));
        existing->SetExpiresAt(map->GetExpiresAt());
      } else {
        if (map->GetExpiresAt() < existing->GetExpiresAt()) {
          LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p reduces ttl of %p\n",
               this, map, existing.get()));
          existing->SetExpiresAt(map->GetExpiresAt());
        } else {
          LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p tries to extend %p but"
               " cannot as without .wk\n",
               this, map, existing.get()));
        }
      }
      return;
    }

    // new alternate. remove old entry and start new validation
    LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p overwrites %p\n",
         this, map, existing.get()));
    existing = nullptr;
    mStorage->Remove(map->HashKey(),
                     map->Private() ? DataStorage_Private : DataStorage_Persistent);
  }

  if (existing && !existing->Validated()) {
    LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p ignored because %p "
         "still in progress\n", this, map, existing.get()));
    return;
  }

  // start new validation
  MOZ_ASSERT(!map->Validated());
  map->Sync();

  RefPtr<nsHttpConnectionInfo> ci;
  map->GetConnectionInfo(getter_AddRefs(ci), pi, originAttributes);
  caps |= ci->GetAnonymous() ? NS_HTTP_LOAD_ANONYMOUS : 0;
  caps |= NS_HTTP_ERROR_SOFTLY;

  if (map->HTTPS()) {
    LOG(("AltSvcCache::UpdateAltServiceMapping %p validation via "
         "speculative connect started\n", this));
    // for https resources we only establish a connection
    nsCOMPtr<nsIInterfaceRequestor> callbacks = new AltSvcOverride(aCallbacks);
    RefPtr<AltSvcTransaction> nullTransaction =
      new AltSvcTransaction(map, ci, aCallbacks, caps);
    gHttpHandler->ConnMgr()->SpeculativeConnect(ci, callbacks, caps, nullTransaction);
  } else {
    // for http:// resources we fetch .well-known too
    nsAutoCString origin (NS_LITERAL_CSTRING("http://") + map->OriginHost());
    if (map->OriginPort() != NS_HTTP_DEFAULT_PORT) {
      origin.Append(':');
      origin.AppendInt(map->OriginPort());
    }

    nsCOMPtr<nsIURI> wellKnown;
    nsAutoCString uri(origin);
    uri.Append(NS_LITERAL_CSTRING("/.well-known/http-opportunistic"));
    NS_NewURI(getter_AddRefs(wellKnown), uri);

    auto *checker = new WellKnownChecker(wellKnown, origin, caps, ci, map);
    if (NS_FAILED(checker->Start())) {
      LOG(("AltSvcCache::UpdateAltServiceMapping %p .wk checker failed to start\n", this));
      map->SetExpired();
      delete checker;
      checker = nullptr;
    } else {
      // object deletes itself when done if started
      LOG(("AltSvcCache::UpdateAltServiceMapping %p .wk checker started %p\n", this, checker));
    }
  }
}

already_AddRefed<AltSvcMapping>
AltSvcCache::GetAltServiceMapping(const nsACString &scheme, const nsACString &host,
                                  int32_t port, bool privateBrowsing)
{
  bool isHTTPS;
  MOZ_ASSERT(NS_IsMainThread());
  if (!mStorage) {
    // DataStorage gives synchronous access to a memory based hash table
    // that is backed by disk where those writes are done asynchronously
    // on another thread
    mStorage = DataStorage::Get(NS_LITERAL_STRING("AlternateServices.txt"));
    if (mStorage) {
      bool storageWillPersist = false;
      if (NS_FAILED(mStorage->Init(storageWillPersist))) {
        mStorage = nullptr;
      }
    }
    if (!mStorage) {
      LOG(("AltSvcCache::GetAltServiceMapping WARN NO STORAGE\n"));
    }
    mStorageEpoch = NowInSeconds();
  }

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
  AltSvcMapping::MakeHashKey(key, scheme, host, port, privateBrowsing);
  RefPtr<AltSvcMapping> existing = LookupMapping(key, privateBrowsing);
  LOG(("AltSvcCache::GetAltServiceMapping %p key=%s "
       "existing=%p validated=%d ttl=%d",
       this, key.get(), existing.get(), existing ? existing->Validated() : 0,
       existing ? existing->TTL() : 0));
  if (existing && !existing->Validated()) {
    existing = nullptr;
  }
  return existing.forget();
}

class ProxyClearHostMapping : public Runnable {
public:
  explicit ProxyClearHostMapping(const nsACString &host, int32_t port)
    : mHost(host)
    , mPort(port)
    {}

    NS_IMETHOD Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      gHttpHandler->ConnMgr()->ClearHostMapping(mHost, mPort);
      return NS_OK;
    }
private:
    nsCString mHost;
    int32_t mPort;
};

void
AltSvcCache::ClearHostMapping(const nsACString &host, int32_t port)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> event = new ProxyClearHostMapping(host, port);
    if (event) {
      NS_DispatchToMainThread(event);
    }
    return;
  }
  nsAutoCString key;
  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("http"), host, port, true);
  RefPtr<AltSvcMapping> existing = LookupMapping(key, true);
  if (existing) {
    existing->SetExpired();
  }

  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("https"), host, port, true);
  existing = LookupMapping(key, true);
  if (existing) {
    existing->SetExpired();
  }

  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("http"), host, port, false);
  existing = LookupMapping(key, false);
  if (existing) {
    existing->SetExpired();
  }

  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("https"), host, port, false);
  existing = LookupMapping(key, false);
  if (existing) {
    existing->SetExpired();
  }
}

void
AltSvcCache::ClearHostMapping(nsHttpConnectionInfo *ci)
{
  if (!ci->GetOrigin().IsEmpty()) {
    ClearHostMapping(ci->GetOrigin(), ci->OriginPort());
  }
}

void
AltSvcCache::ClearAltServiceMappings()
{
    MOZ_ASSERT(NS_IsMainThread());
    if (mStorage) {
      mStorage->Clear();
    }
}

NS_IMETHODIMP
AltSvcOverride::GetInterface(const nsIID &iid, void **result)
{
  if (NS_SUCCEEDED(QueryInterface(iid, result)) && *result) {
    return NS_OK;
  }
  return mCallbacks->GetInterface(iid, result);
}

NS_IMETHODIMP
AltSvcOverride::GetIgnoreIdle(bool *ignoreIdle)
{
  *ignoreIdle = true;
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetParallelSpeculativeConnectLimit(
  uint32_t *parallelSpeculativeConnectLimit)
{
  *parallelSpeculativeConnectLimit = 32;
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetIsFromPredictor(bool *isFromPredictor)
{
  *isFromPredictor = false;
  return NS_OK;
}

NS_IMETHODIMP
AltSvcOverride::GetAllow1918(bool *allow)
{
  // normally we don't do speculative connects to 1918.. and we use
  // speculative connects for the mapping validation, so override
  // that default here for alt-svc
  *allow = true;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(AltSvcOverride, nsIInterfaceRequestor, nsISpeculativeConnectionOverrider)

} // namespace net
} // namespace mozilla
