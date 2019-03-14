/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/UrlClassifierCommon.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/StaticPrefs.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "mozilla/dom/Document.h"
#include "nsIDocShell.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIParentChannel.h"
#include "nsIScriptError.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

const nsCString::size_type UrlClassifierCommon::sMaxSpecLength = 128;

// MOZ_LOG=nsChannelClassifier:5
LazyLogModule UrlClassifierCommon::sLog("nsChannelClassifier");

/* static */
bool UrlClassifierCommon::AddonMayLoad(nsIChannel* aChannel, nsIURI* aURI) {
  nsCOMPtr<nsILoadInfo> channelLoadInfo = aChannel->LoadInfo();
  // loadingPrincipal is used here to ensure we are loading into an
  // addon principal.  This allows an addon, with explicit permission, to
  // call out to API endpoints that may otherwise get blocked.
  nsIPrincipal* loadingPrincipal = channelLoadInfo->LoadingPrincipal();
  if (!loadingPrincipal) {
    return false;
  }

  return BasePrincipal::Cast(loadingPrincipal)->AddonAllowsLoad(aURI, true);
}

/* static */
void UrlClassifierCommon::NotifyChannelClassifierProtectionDisabled(
    nsIChannel* aChannel, uint32_t aEvent) {
  // Can be called in EITHER the parent or child process.
  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request.
    // Tell the child process channel to do this instead.
    parentChannel->NotifyChannelClassifierProtectionDisabled(aEvent);
    return;
  }

  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingCommon::MaybeGetDocumentURIBeingLoaded(aChannel);
  NotifyChannelBlocked(aChannel, uriBeingLoaded, aEvent);
}

/* static */
void UrlClassifierCommon::NotifyChannelBlocked(nsIChannel* aChannel,
                                               nsIURI* aURIBeingLoaded,
                                               unsigned aBlockedReason) {
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (NS_WARN_IF(!thirdPartyUtil)) {
    return;
  }

  nsCOMPtr<mozIDOMWindowProxy> win;
  nsresult rv = thirdPartyUtil->GetTopWindowForChannel(
      aChannel, aURIBeingLoaded, getter_AddRefs(win));
  NS_ENSURE_SUCCESS_VOID(rv);
  auto* pwin = nsPIDOMWindowOuter::From(win);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  if (!docShell) {
    return;
  }
  RefPtr<dom::Document> doc = docShell->GetDocument();
  NS_ENSURE_TRUE_VOID(doc);

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  pwin->NotifyContentBlockingEvent(aBlockedReason, aChannel, true, uri);
}

/* static */
bool UrlClassifierCommon::ShouldEnableClassifier(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIURI> chanURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(chanURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (UrlClassifierCommon::AddonMayLoad(aChannel, chanURI)) {
    return false;
  }

  nsCOMPtr<nsIURI> topWinURI;
  nsCOMPtr<nsIHttpChannelInternal> channel = do_QueryInterface(aChannel);
  if (!channel) {
    UC_LOG(("nsChannelClassifier: Not an HTTP channel"));
    return false;
  }

  rv = channel->GetTopWindowURI(getter_AddRefs(topWinURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Tracking protection will be enabled so return without updating
  // the security state. If any channels are subsequently cancelled
  // (page elements blocked) the state will be then updated.
  if (UC_LOG_ENABLED()) {
    nsCString chanSpec = chanURI->GetSpecOrDefault();
    chanSpec.Truncate(
        std::min(chanSpec.Length(), UrlClassifierCommon::sMaxSpecLength));
    nsCString topWinSpec = topWinURI ? topWinURI->GetSpecOrDefault()
                                     : NS_LITERAL_CSTRING("(null)");
    topWinSpec.Truncate(
        std::min(topWinSpec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(
        ("nsChannelClassifier: Enabling url classifier checks on "
         "channel[%p] with uri %s for toplevel window uri %s",
         aChannel, chanSpec.get(), topWinSpec.get()));
  }

  return true;
}

/* static */
nsresult UrlClassifierCommon::SetBlockedContent(nsIChannel* channel,
                                                nsresult aErrorCode,
                                                const nsACString& aList,
                                                const nsACString& aProvider,
                                                const nsACString& aFullHash) {
  NS_ENSURE_ARG(!aList.IsEmpty());

  // Can be called in EITHER the parent or child process.
  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(channel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request.
    // Tell the child process channel to do this instead.
    parentChannel->SetClassifierMatchedInfo(aList, aProvider, aFullHash);
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (classifiedChannel) {
    classifiedChannel->SetMatchedInfo(aList, aProvider, aFullHash);
  }

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = services::GetThirdPartyUtil();
  if (NS_WARN_IF(!thirdPartyUtil)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingCommon::MaybeGetDocumentURIBeingLoaded(channel);
  nsCOMPtr<mozIDOMWindowProxy> win;
  rv = thirdPartyUtil->GetTopWindowForChannel(channel, uriBeingLoaded,
                                              getter_AddRefs(win));
  NS_ENSURE_SUCCESS(rv, NS_OK);
  auto* pwin = nsPIDOMWindowOuter::From(win);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  if (!docShell) {
    return NS_OK;
  }
  RefPtr<dom::Document> doc = docShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_OK);

  unsigned state =
      UrlClassifierFeatureFactory::GetClassifierBlockingEventCode(aErrorCode);
  if (!state) {
    state = nsIWebProgressListener::STATE_BLOCKED_UNSAFE_CONTENT;
  }

  UrlClassifierCommon::NotifyChannelBlocked(channel, uriBeingLoaded, state);

  // Log a warning to the web console.
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  NS_ConvertUTF8toUTF16 spec(uri->GetSpecOrDefault());
  const char16_t* params[] = {spec.get()};
  const char* message;
  nsCString category;

  if (UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(aErrorCode)) {
    message = UrlClassifierFeatureFactory::
        ClassifierBlockingErrorCodeToConsoleMessage(aErrorCode, category);
  } else {
    message = "UnsafeUriBlocked";
    category = NS_LITERAL_CSTRING("Safe Browsing");
  }

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, category, doc,
                                  nsContentUtils::eNECKO_PROPERTIES, message,
                                  params, ArrayLength(params));

  return NS_OK;
}

/* static */
nsresult UrlClassifierCommon::CreatePairwiseWhiteListURI(nsIChannel* aChannel,
                                                         nsIURI** aURI) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aURI);

  nsresult rv;
  nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(aChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!chan) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> topWinURI;
  rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!topWinURI) {
    if (UC_LOG_ENABLED()) {
      nsresult rv;
      nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(aChannel, &rv);
      nsCOMPtr<nsIURI> uri;
      rv = httpChan->GetURI(getter_AddRefs(uri));
      nsAutoCString spec;
      uri->GetAsciiSpec(spec);
      spec.Truncate(
          std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(("CreatePairwiseWhiteListURI: No window URI associated with %s",
              spec.get()));
    }
    return NS_OK;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> chanPrincipal;
  rv = securityManager->GetChannelURIPrincipal(aChannel,
                                               getter_AddRefs(chanPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Craft a whitelist URL like "toplevel.page/?resource=third.party.domain"
  nsAutoCString pageHostname, resourceDomain;
  rv = topWinURI->GetHost(pageHostname);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = chanPrincipal->GetBaseDomain(resourceDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString whitelistEntry = NS_LITERAL_CSTRING("http://") + pageHostname +
                                 NS_LITERAL_CSTRING("/?resource=") +
                                 resourceDomain;
  UC_LOG(
      ("CreatePairwiseWhiteListURI: Looking for %s in the whitelist "
       "(channel=%p)",
       whitelistEntry.get(), aChannel));

  nsCOMPtr<nsIURI> whitelistURI;
  rv = NS_NewURI(getter_AddRefs(whitelistURI), whitelistEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  whitelistURI.forget(aURI);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
