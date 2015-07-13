/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpLog.h"

#include "AlternateServices.h"
#include "nsEscape.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpHandler.h"
#include "nsThreadUtils.h"
#include "NullHttpTransaction.h"
#include "nsISSLStatusProvider.h"
#include "nsISSLStatus.h"
#include "nsISSLSocketControl.h"

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
                             uint32_t caps)
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
    nsAutoCString hostname; // Always empty in the header form
    nsAutoCString npnToken;
    int32_t portno = originPort;

    for (uint32_t pairIndex = 0;
         pairIndex < parsedAltSvc.mValues[index].mValues.Length();
         ++pairIndex) {
      nsDependentCSubstring &currentName =
        parsedAltSvc.mValues[index].mValues[pairIndex].mName;
      nsDependentCSubstring &currentValue =
        parsedAltSvc.mValues[index].mValues[pairIndex].mValue;

      if (!pairIndex) {
        // h2=:443
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
      }
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

    nsRefPtr<AltSvcMapping> mapping = new AltSvcMapping(originScheme,
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
      gHttpHandler->UpdateAltServiceMapping(mapping, proxyInfo, callbacks, caps);
    }
  }
}

AltSvcMapping::AltSvcMapping(const nsACString &originScheme,
                             const nsACString &originHost,
                             int32_t originPort,
                             const nsACString &username,
                             bool privateBrowsing,
                             uint32_t expiresAt,
                             const nsACString &alternateHost,
                             int32_t alternatePort,
                             const nsACString &npnToken)
  : mAlternateHost(alternateHost)
  , mAlternatePort(alternatePort)
  , mOriginHost(originHost)
  , mOriginPort(originPort)
  , mUsername(username)
  , mPrivate(privateBrowsing)
  , mExpiresAt(expiresAt)
  , mValidated(false)
  , mRunning(false)
  , mNPNToken(npnToken)
{
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
AltSvcMapping::SetExpired()
{
  mExpiresAt = NowInSeconds() - 1;
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
                                 nsProxyInfo *pi)
{
  nsRefPtr<nsHttpConnectionInfo> ci =
    new nsHttpConnectionInfo(mOriginHost, mOriginPort, mNPNToken,
                             mUsername, pi, mAlternateHost, mAlternatePort);
  ci->SetInsecureScheme(!mHttps);
  ci->SetPrivate(mPrivate);
  ci.forget(outCI);
}

// This is the asynchronous null transaction used to validate
// an alt-svc advertisement
class AltSvcTransaction final : public NullHttpTransaction
{
public:
    AltSvcTransaction(AltSvcMapping *map,
                      nsHttpConnectionInfo *ci,
                      nsIInterfaceRequestor *callbacks,
                      uint32_t caps)
    : NullHttpTransaction(ci, callbacks, caps)
    , mMapping(map)
    , mRunning(false)
    , mTriedToValidate(false)
    , mTriedToWrite(false)
  {
    MOZ_ASSERT(mMapping);
    LOG(("AltSvcTransaction ctor %p map %p [%s -> %s]",
         this, map, map->OriginHost().get(), map->AlternateHost().get()));
  }

  ~AltSvcTransaction()
  {
    LOG(("AltSvcTransaction dtor %p map %p running %d",
         this, mMapping.get(), mRunning));

    if (mRunning) {
      MOZ_ASSERT(mMapping->IsRunning());
      MaybeValidate(NS_OK);
    }
    if (!mMapping->Validated()) {
      // try again later
      mMapping->SetExpiresAt(NowInSeconds() + 2);
    }
    LOG(("AltSvcTransaction dtor %p map %p validated %d [%s]",
         this, mMapping.get(), mMapping->Validated(),
         mMapping->HashKey().get()));
    mMapping->SetRunning(false);
  }

  void StartTransaction()
  {
    LOG(("AltSvcTransaction::StartTransaction() %p", this));

    MOZ_ASSERT(!mRunning);
    MOZ_ASSERT(!mMapping->IsRunning());
    mCaps &= ~NS_HTTP_ALLOW_KEEPALIVE;
    mRunning = true;
    mMapping->SetRunning(true);
  }

  void MaybeValidate(nsresult reason)
  {
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

    // insist on spdy/3* or >= http/2
    uint32_t version = mConnection->Version();
    LOG(("AltSvcTransaction::MaybeValidate() %p version %d\n", this, version));
    if ((version < HTTP_VERSION_2) && (version != SPDY_VERSION_31)) {
      LOG(("AltSvcTransaction::MaybeValidate %p Failed due to protocol version", this));
      return;
    }

    nsCOMPtr<nsISupports> secInfo;
    mConnection->GetSecurityInfo(getter_AddRefs(secInfo));
    nsCOMPtr<nsISSLSocketControl> socketControl = do_QueryInterface(secInfo);
    bool bypassAuth = false;

    if (!socketControl ||
        NS_FAILED(socketControl->GetBypassAuthentication(&bypassAuth))) {
      bypassAuth = false;
    }

    LOG(("AltSvcTransaction::MaybeValidate() %p socketControl=%p bypass=%d",
         this, socketControl.get(), bypassAuth));

    if (bypassAuth) {
      if (mMapping->HTTPS()) {
        MOZ_ASSERT(false); // cannot happen but worth the runtime sanity check
        LOG(("AltSvcTransaction::MaybeValidate %p"
             "somehow indicates bypassAuth on https:// origin\n", this));
        return;
      }

      LOG(("AltSvcTransaction::MaybeValidate() %p "
           "validating alternate service because relaxed", this));
      mMapping->SetValidated(true);
      return;
    }

    if (socketControl->GetFailedVerification()) {
      LOG(("AltSvcTransaction::MaybeValidate() %p "
           "not validated due to auth error", this));
      return;
    }

    LOG(("AltSvcTransaction::MaybeValidate() %p "
         "validating alternate service with auth check", this));
    mMapping->SetValidated(true);
  }

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
  nsRefPtr<AltSvcMapping> mMapping;
  uint32_t                mRunning : 1;
  uint32_t                mTriedToValidate : 1;
  uint32_t                mTriedToWrite : 1;
};

void
AltSvcCache::UpdateAltServiceMapping(AltSvcMapping *map, nsProxyInfo *pi,
                                     nsIInterfaceRequestor *aCallbacks,
                                     uint32_t caps)
{
  MOZ_ASSERT(NS_IsMainThread());
  AltSvcMapping *existing = mHash.GetWeak(map->mHashKey);
  LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p existing %p %s",
       this, map, existing, map->AlternateHost().get()));

  if (existing && (existing->TTL() <= 0)) {
    LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p is expired",
         this, map));
    existing = nullptr;
    mHash.Remove(map->mHashKey);
  }

  if (existing && existing->mValidated) {
    if (existing->RouteEquals(map)) {
      // update expires
      LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p updates ttl of %p\n",
           this, map, existing));
      existing->SetExpiresAt(map->GetExpiresAt());
      return;
    }

    LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p overwrites %p\n",
         this, map, existing));
    existing = nullptr;
    mHash.Remove(map->mHashKey);
  }

  if (existing) {
    LOG(("AltSvcCache::UpdateAltServiceMapping %p map %p ignored because %p "
         "still in progress\n", this, map, existing));
    return;
  }

  mHash.Put(map->mHashKey, map);

  nsRefPtr<nsHttpConnectionInfo> ci;
  map->GetConnectionInfo(getter_AddRefs(ci), pi);
  caps |= ci->GetAnonymous() ? NS_HTTP_LOAD_ANONYMOUS : 0;

  nsCOMPtr<nsIInterfaceRequestor> callbacks = new AltSvcOverride(aCallbacks);

  nsRefPtr<AltSvcTransaction> nullTransaction =
    new AltSvcTransaction(map, ci, aCallbacks, caps);
  nullTransaction->StartTransaction();
  gHttpHandler->ConnMgr()->SpeculativeConnect(ci, callbacks, caps, nullTransaction);
}

AltSvcMapping *
AltSvcCache::GetAltServiceMapping(const nsACString &scheme, const nsACString &host,
                                  int32_t port, bool privateBrowsing)
{
  bool isHTTPS;
  MOZ_ASSERT(NS_IsMainThread());
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
  AltSvcMapping *existing = mHash.GetWeak(key);
  LOG(("AltSvcCache::GetAltServiceMapping %p key=%s "
       "existing=%p validated=%d running=%d ttl=%d",
       this, key.get(), existing, existing ? existing->mValidated : 0,
       existing ? existing->mRunning : 0,
       existing ? existing->TTL() : 0));
  if (existing && (existing->TTL() <= 0)) {
    LOG(("AltSvcCache::GetAltServiceMapping %p map %p is expired", this, existing));
    mHash.Remove(existing->mHashKey);
    existing = nullptr;
  }
  if (existing && existing->mValidated)
    return existing;
  return nullptr;
}

class ProxyClearHostMapping : public nsRunnable {
public:
  explicit ProxyClearHostMapping(const nsACString &host, int32_t port)
    : mHost(host)
    , mPort(port)
    {}

    NS_IMETHOD Run()
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
  AltSvcMapping *existing = mHash.GetWeak(key);
  if (existing) {
    existing->SetExpired();
  }

  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("https"), host, port, true);
  existing = mHash.GetWeak(key);
  if (existing) {
    existing->SetExpired();
  }

  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("http"), host, port, false);
  existing = mHash.GetWeak(key);
  if (existing) {
    existing->SetExpired();
  }

  AltSvcMapping::MakeHashKey(key, NS_LITERAL_CSTRING("https"), host, port, false);
  existing = mHash.GetWeak(key);
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
    mHash.Clear();
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
AltSvcOverride::GetIgnorePossibleSpdyConnections(bool *ignorePossibleSpdyConnections)
{
  *ignorePossibleSpdyConnections = true;
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
