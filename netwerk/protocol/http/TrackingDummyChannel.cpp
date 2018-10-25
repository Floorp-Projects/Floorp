/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackingDummyChannel.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/net/TrackingDummyChannelChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "nsIChannel.h"
#include "nsIURI.h"

namespace mozilla {
namespace net {

namespace {

// We need TrackingDummyChannel any time
// privacy.trackingprotection.annotate_channels prefs is set to true.
bool
ChannelNeedsToBeAnnotated()
{
  static bool sChannelAnnotationNeededInited = false;
  static bool sChannelAnnotationNeeded = false;

  if (!sChannelAnnotationNeededInited) {
    sChannelAnnotationNeededInited = true;
    Preferences::AddBoolVarCache(&sChannelAnnotationNeeded,
                                 "privacy.trackingprotection.annotate_channels");
  }

  return sChannelAnnotationNeeded;
}

} // anonymous

/* static */ TrackingDummyChannel::StorageAllowedState
TrackingDummyChannel::StorageAllowed(nsIChannel* aChannel,
                                     const std::function<void(bool)>& aCallback)
{
  MOZ_ASSERT(!XRE_IsParentProcess());

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    // Any non-http channel is allowed.
    return eStorageGranted;
  }

  if (!nsContentUtils::IsNonSubresourceRequest(aChannel)) {
    // If this is a sub-resource, we don't need to check if the channel is
    // annotated as tracker because:
    // - if the SW doesn't respond, or it sends us back to the network, the
    //   channel will be annotated on the parent process.
    // - if the SW answers, the content is taken from the cache, which is
    //   considered trusted.
    return eStorageGranted;
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  if (ChannelNeedsToBeAnnotated()) {
    ContentChild* cc = static_cast<ContentChild*>(gNeckoChild->Manager());
    if (cc->IsShuttingDown()) {
      return eStorageDenied;
    }

    if (!TrackingDummyChannelChild::Create(httpChannel, uri, aCallback)) {
      return eStorageDenied;
    }

    return eAsyncNeeded;
  }

  if (AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(httpChannel, uri,
                                                              nullptr)) {
    return eStorageGranted;
  }

  return eStorageDenied;
}

NS_IMPL_ADDREF(TrackingDummyChannel)
NS_IMPL_RELEASE(TrackingDummyChannel)

NS_INTERFACE_MAP_BEGIN(TrackingDummyChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(TrackingDummyChannel)
NS_INTERFACE_MAP_END

TrackingDummyChannel::TrackingDummyChannel(nsIURI* aURI,
                                           nsIURI* aTopWindowURI,
                                           nsresult aTopWindowURIResult,
                                           nsILoadInfo* aLoadInfo)
  : mTopWindowURI(aTopWindowURI)
  , mTopWindowURIResult(aTopWindowURIResult)
  , mIsTrackingResource(false)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  SetOriginalURI(aURI);
  SetLoadInfo(aLoadInfo);
}

TrackingDummyChannel::~TrackingDummyChannel() = default;

bool
TrackingDummyChannel::IsTrackingResource() const
{
  return mIsTrackingResource;
}

void
TrackingDummyChannel::SetIsTrackingResource()
{
  mIsTrackingResource = true;
}

//-----------------------------------------------------------------------------
// TrackingDummyChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
TrackingDummyChannel::GetOriginalURI(nsIURI** aOriginalURI)
{
  NS_IF_ADDREF(*aOriginalURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
TrackingDummyChannel::SetOriginalURI(nsIURI* aOriginalURI)
{
  mURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
TrackingDummyChannel::GetURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
TrackingDummyChannel::GetOwner(nsISupports** aOwner)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetOwner(nsISupports* aOwner)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetNotificationCallbacks(nsIInterfaceRequestor** aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetSecurityInfo(nsISupports** aSecurityInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetContentType(nsACString& aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetContentType(const nsACString& aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetContentCharset(nsACString& aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetContentCharset(const nsACString& aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetContentLength(int64_t* aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetContentLength(int64_t aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::Open(nsIInputStream** aRetval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

NS_IMETHODIMP
TrackingDummyChannel::AsyncOpen(nsIStreamListener* aListener, nsISupports* aContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::AsyncOpen2(nsIStreamListener* aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener, nullptr);
}

NS_IMETHODIMP
TrackingDummyChannel::GetContentDisposition(uint32_t* aContentDisposition)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetContentDispositionFilename(nsAString& aContentDispositionFilename)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetContentDispositionFilename(const nsAString& aContentDispositionFilename)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetContentDispositionHeader(nsACString& aContentDispositionHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLoadInfo(nsILoadInfo** aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
TrackingDummyChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// TrackingDummyChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
TrackingDummyChannel::GetName(nsACString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::IsPending(bool* aRetval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetStatus(nsresult* aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::Cancel(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLoadGroup(nsILoadGroup** aLoadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLoadFlags(nsLoadFlags* aLoadFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetIsDocument(bool* aIsDocument)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// TrackingDummyChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
TrackingDummyChannel::GetDocumentURI(nsIURI** aDocumentURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetDocumentURI(nsIURI* aDocumentURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetRequestVersion(uint32_t* aMajor, uint32_t* aMinor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetResponseVersion(uint32_t* aMajor, uint32_t* aMinor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::TakeAllSecurityMessages(nsCOMArray<nsISecurityConsoleMessage>& aMessages)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetCookie(const char* aCookieHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetupFallbackChannel(const char* aFallbackKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetThirdPartyFlags(uint32_t* aThirdPartyFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetThirdPartyFlags(uint32_t aThirdPartyFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetForceAllowThirdPartyCookie(bool* aForceAllowThirdPartyCookie)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetForceAllowThirdPartyCookie(bool aForceAllowThirdPartyCookie)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetCanceled(bool* aCanceled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetChannelIsForDownload(bool* aChannlIsForDownload)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetChannelIsForDownload(bool aChannlIsForDownload)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLocalAddress(nsACString& aLocalAddress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLocalPort(int32_t* aLocalPort)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetRemoteAddress(nsACString& aRemoteAddress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetRemotePort(int32_t* aRemotePort)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetCacheKeysRedirectChain(nsTArray<nsCString>* aCacheKeys)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::HTTPUpgrade(const nsACString& aProtocolName,
                                  nsIHttpUpgradeListener* aListener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetAllowSpdy(bool* aAllowSpdy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetAllowSpdy(bool aAllowSpdy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetResponseTimeoutEnabled(bool* aResponseTimeoutEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetResponseTimeoutEnabled(bool aResponseTimeoutEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetInitialRwin(uint32_t* aInitialRwin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetInitialRwin(uint32_t aInitialRwin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetApiRedirectToURI(nsIURI** aApiRedirectToURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetAllowAltSvc(bool* aAllowAltSvc)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetAllowAltSvc(bool aAllowAltSvc)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetBeConservative(bool* aBeConservative)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetBeConservative(bool aBeConservative)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetTrr(bool* aTrr)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetTrr(bool aTrr)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetTlsFlags(uint32_t* aTlsFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetTlsFlags(uint32_t aTlsFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLastModifiedTime(PRTime* aLastModifiedTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetCorsIncludeCredentials(bool* aCorsIncludeCredentials)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetCorsIncludeCredentials(bool aCorsIncludeCredentials)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetCorsMode(uint32_t* aCorsMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetCorsMode(uint32_t aCorsMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetRedirectMode(uint32_t* aRedirectMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetRedirectMode(uint32_t aRedirectMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetFetchCacheMode(uint32_t* aFetchCacheMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetFetchCacheMode(uint32_t aFetchCacheMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetTopWindowURI(nsIURI** aTopWindowURI)
{
  nsCOMPtr<nsIURI> topWindowURI = mTopWindowURI;
  topWindowURI.forget(aTopWindowURI);
  return mTopWindowURIResult;
}

NS_IMETHODIMP
TrackingDummyChannel::SetTopWindowURIIfUnknown(nsIURI* aTopWindowURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetProxyURI(nsIURI** aProxyURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
TrackingDummyChannel::SetCorsPreflightParameters(const nsTArray<nsCString>& aUnsafeHeaders)
{}

void
TrackingDummyChannel::SetAltDataForChild(bool aIsForChild)
{}

NS_IMETHODIMP
TrackingDummyChannel::GetBlockAuthPrompt(bool* aBlockAuthPrompt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetBlockAuthPrompt(bool aBlockAuthPrompt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetIntegrityMetadata(nsAString& aIntegrityMetadata)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetIntegrityMetadata(const nsAString& aIntegrityMetadata)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetConnectionInfoHashKey(nsACString& aConnectionInfoHashKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetLastRedirectFlags(uint32_t* aLastRedirectFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetLastRedirectFlags(uint32_t aLastRedirectFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::GetNavigationStartTimeStamp(TimeStamp* aNavigationStartTimeStamp)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::SetNavigationStartTimeStamp(TimeStamp aNavigationStartTimeStamp)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TrackingDummyChannel::CancelForTrackingProtection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // net namespace
} // mozilla namespace
