/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClassifierDummyChannel.h"

#include "mozilla/ContentBlocking.h"
#include "mozilla/net/ClassifierDummyChannelChild.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsContentSecurityManager.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace net {

/* static */ ClassifierDummyChannel::StorageAllowedState
ClassifierDummyChannel::StorageAllowed(
    nsIChannel* aChannel, const std::function<void(bool)>& aCallback) {
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

  if (StaticPrefs::privacy_trackingprotection_annotate_channels()) {
    dom::ContentChild* cc =
        static_cast<dom::ContentChild*>(gNeckoChild->Manager());
    if (cc->IsShuttingDown()) {
      return eStorageDenied;
    }

    if (!ClassifierDummyChannelChild::Create(httpChannel, uri, aCallback)) {
      return eStorageDenied;
    }

    return eAsyncNeeded;
  }

  if (ContentBlocking::ShouldAllowAccessFor(httpChannel, uri, nullptr)) {
    return eStorageGranted;
  }

  return eStorageDenied;
}

NS_IMPL_ADDREF(ClassifierDummyChannel)
NS_IMPL_RELEASE(ClassifierDummyChannel)

NS_INTERFACE_MAP_BEGIN(ClassifierDummyChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY(nsIClassifiedChannel)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(ClassifierDummyChannel)
NS_INTERFACE_MAP_END

ClassifierDummyChannel::ClassifierDummyChannel(nsIURI* aURI,
                                               nsIURI* aTopWindowURI,
                                               nsresult aTopWindowURIResult,
                                               nsILoadInfo* aLoadInfo)
    : mTopWindowURI(aTopWindowURI),
      mTopWindowURIResult(aTopWindowURIResult),
      mFirstPartyClassificationFlags(0),
      mThirdPartyClassificationFlags(0) {
  MOZ_ASSERT(XRE_IsParentProcess());

  SetOriginalURI(aURI);
  SetLoadInfo(aLoadInfo);
}

ClassifierDummyChannel::~ClassifierDummyChannel() {
  NS_ReleaseOnMainThreadSystemGroup("ClassifierDummyChannel::mLoadInfo",
                                    mLoadInfo.forget());
  NS_ReleaseOnMainThreadSystemGroup("ClassifierDummyChannel::mURI",
                                    mURI.forget());
  NS_ReleaseOnMainThreadSystemGroup("ClassifierDummyChannel::mTopWindowURI",
                                    mTopWindowURI.forget());
}

void ClassifierDummyChannel::AddClassificationFlags(
    uint32_t aClassificationFlags, bool aThirdParty) {
  if (aThirdParty) {
    mThirdPartyClassificationFlags |= aClassificationFlags;
  } else {
    mFirstPartyClassificationFlags |= aClassificationFlags;
  }
}

//-----------------------------------------------------------------------------
// ClassifierDummyChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ClassifierDummyChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  NS_IF_ADDREF(*aOriginalURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetOriginalURI(nsIURI* aOriginalURI) {
  mURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetURI(nsIURI** aURI) {
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetOwner(nsISupports** aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetOwner(nsISupports* aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetSecurityInfo(nsISupports** aSecurityInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetContentType(nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetContentType(const nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetContentCharset(nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetContentCharset(const nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetContentLength(int64_t* aContentLength) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetContentLength(int64_t aContentLength) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener);
}

NS_IMETHODIMP
ClassifierDummyChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetContentDisposition(uint32_t aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ClassifierDummyChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ClassifierDummyChannel::GetName(nsACString& aName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::IsPending(bool* aRetval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetStatus(nsresult* aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::Cancel(nsresult aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::Suspend() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
ClassifierDummyChannel::Resume() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
ClassifierDummyChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetIsDocument(bool* aIsDocument) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// ClassifierDummyChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ClassifierDummyChannel::GetDocumentURI(nsIURI** aDocumentURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetDocumentURI(nsIURI* aDocumentURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetRequestVersion(uint32_t* aMajor, uint32_t* aMinor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetResponseVersion(uint32_t* aMajor, uint32_t* aMinor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::TakeAllSecurityMessages(
    nsCOMArray<nsISecurityConsoleMessage>& aMessages) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetCookie(const nsACString& aCookieHeader) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetupFallbackChannel(const char* aFallbackKey) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetThirdPartyFlags(uint32_t* aThirdPartyFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetThirdPartyFlags(uint32_t aThirdPartyFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetForceAllowThirdPartyCookie(
    bool* aForceAllowThirdPartyCookie) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetForceAllowThirdPartyCookie(
    bool aForceAllowThirdPartyCookie) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetCanceled(bool* aCanceled) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetChannelIsForDownload(bool* aChannlIsForDownload) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetChannelIsForDownload(bool aChannlIsForDownload) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetLocalAddress(nsACString& aLocalAddress) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetLocalPort(int32_t* aLocalPort) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetRemoteAddress(nsACString& aRemoteAddress) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetRemotePort(int32_t* aRemotePort) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetCacheKeysRedirectChain(
    nsTArray<nsCString>* aCacheKeys) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::HTTPUpgrade(const nsACString& aProtocolName,
                                    nsIHttpUpgradeListener* aListener) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetOnlyConnect(bool* aOnlyConnect) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetConnectOnly() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
ClassifierDummyChannel::GetAllowSpdy(bool* aAllowSpdy) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetAllowSpdy(bool aAllowSpdy) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetResponseTimeoutEnabled(
    bool* aResponseTimeoutEnabled) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetResponseTimeoutEnabled(
    bool aResponseTimeoutEnabled) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetInitialRwin(uint32_t* aInitialRwin) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetInitialRwin(uint32_t aInitialRwin) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetApiRedirectToURI(nsIURI** aApiRedirectToURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetAllowAltSvc(bool* aAllowAltSvc) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetAllowAltSvc(bool aAllowAltSvc) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetBeConservative(bool* aBeConservative) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetBeConservative(bool aBeConservative) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetIsTRRServiceChannel(bool* aTrr) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetIsTRRServiceChannel(bool aTrr) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetIsResolvedByTRR(bool* aResolvedByTRR) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetTlsFlags(uint32_t* aTlsFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetTlsFlags(uint32_t aTlsFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetLastModifiedTime(PRTime* aLastModifiedTime) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetCorsIncludeCredentials(
    bool* aCorsIncludeCredentials) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetCorsIncludeCredentials(
    bool aCorsIncludeCredentials) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetCorsMode(uint32_t* aCorsMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetCorsMode(uint32_t aCorsMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetRedirectMode(uint32_t* aRedirectMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetRedirectMode(uint32_t aRedirectMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetFetchCacheMode(uint32_t* aFetchCacheMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetFetchCacheMode(uint32_t aFetchCacheMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetTopWindowURI(nsIURI** aTopWindowURI) {
  nsCOMPtr<nsIURI> topWindowURI = mTopWindowURI;
  topWindowURI.forget(aTopWindowURI);
  return mTopWindowURIResult;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetTopWindowURIIfUnknown(nsIURI* aTopWindowURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetProxyURI(nsIURI** aProxyURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void ClassifierDummyChannel::SetCorsPreflightParameters(
    const nsTArray<nsCString>& aUnsafeHeaders) {}

void ClassifierDummyChannel::SetAltDataForChild(bool aIsForChild) {}

NS_IMETHODIMP
ClassifierDummyChannel::GetBlockAuthPrompt(bool* aBlockAuthPrompt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetBlockAuthPrompt(bool aBlockAuthPrompt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetIntegrityMetadata(nsAString& aIntegrityMetadata) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetIntegrityMetadata(
    const nsAString& aIntegrityMetadata) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetConnectionInfoHashKey(
    nsACString& aConnectionInfoHashKey) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetLastRedirectFlags(uint32_t* aLastRedirectFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetLastRedirectFlags(uint32_t aLastRedirectFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetNavigationStartTimeStamp(
    TimeStamp* aNavigationStartTimeStamp) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::SetNavigationStartTimeStamp(
    TimeStamp aNavigationStartTimeStamp) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::CancelByURLClassifier(nsresult aErrorCode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void ClassifierDummyChannel::SetIPv4Disabled() {}

void ClassifierDummyChannel::SetIPv6Disabled() {}

bool ClassifierDummyChannel::GetHasNonEmptySandboxingFlag() { return false; }

void ClassifierDummyChannel::SetHasNonEmptySandboxingFlag(
    bool aHasNonEmptySandboxingFlag) {}

NS_IMETHODIMP ClassifierDummyChannel::ComputeCrossOriginOpenerPolicy(
    nsILoadInfo::CrossOriginOpenerPolicy aInitiatorPolicy,
    nsILoadInfo::CrossOriginOpenerPolicy* aOutPolicy) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ClassifierDummyChannel::GetCrossOriginOpenerPolicy(
    nsILoadInfo::CrossOriginOpenerPolicy* aPolicy) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::SetMatchedInfo(
    const nsACString& aList, const nsACString& aProvider,
    const nsACString& aFullHash) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::GetMatchedList(nsACString& aMatchedList) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::GetMatchedProvider(
    nsACString& aMatchedProvider) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::GetMatchedFullHash(
    nsACString& aMatchedFullHash) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::SetMatchedTrackingInfo(
    const nsTArray<nsCString>& aLists, const nsTArray<nsCString>& aFullHashes) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::GetMatchedTrackingLists(
    nsTArray<nsCString>& aMatchedTrackingLists) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::GetMatchedTrackingFullHashes(
    nsTArray<nsCString>& aMatchedTrackingFullHashes) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ClassifierDummyChannel::GetFirstPartyClassificationFlags(
    uint32_t* aFirstPartyClassificationFlags) {
  *aFirstPartyClassificationFlags = mFirstPartyClassificationFlags;
  return NS_OK;
}

NS_IMETHODIMP ClassifierDummyChannel::GetThirdPartyClassificationFlags(
    uint32_t* aThirdPartyClassificationFlags) {
  *aThirdPartyClassificationFlags = mThirdPartyClassificationFlags;
  return NS_OK;
}

NS_IMETHODIMP ClassifierDummyChannel::GetClassificationFlags(
    uint32_t* aClassificationFlags) {
  if (mThirdPartyClassificationFlags) {
    *aClassificationFlags = mThirdPartyClassificationFlags;
  } else {
    *aClassificationFlags = mFirstPartyClassificationFlags;
  }
  return NS_OK;
}

NS_IMETHODIMP ClassifierDummyChannel::IsThirdPartyTrackingResource(
    bool* aIsTrackingResource) {
  MOZ_ASSERT(
      !(mFirstPartyClassificationFlags && mThirdPartyClassificationFlags));
  *aIsTrackingResource = UrlClassifierCommon::IsTrackingClassificationFlag(
      mThirdPartyClassificationFlags);
  return NS_OK;
}

NS_IMETHODIMP ClassifierDummyChannel::IsThirdPartySocialTrackingResource(
    bool* aIsThirdPartySocialTrackingResource) {
  MOZ_ASSERT(!mFirstPartyClassificationFlags ||
             !mThirdPartyClassificationFlags);
  *aIsThirdPartySocialTrackingResource =
      UrlClassifierCommon::IsSocialTrackingClassificationFlag(
          mThirdPartyClassificationFlags);
  return NS_OK;
}

void ClassifierDummyChannel::DoDiagnosticAssertWhenOnStopNotCalledOnDestroy() {}

}  // namespace net
}  // namespace mozilla
