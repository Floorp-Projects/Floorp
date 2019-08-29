/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/UrlClassifierCommon.h"

#include "ClassifierDummyChannel.h"
#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_channelclassifier.h"
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
#include "nsQueryObject.h"

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
  pwin->NotifyContentBlockingEvent(aBlockedReason, aChannel, true, uri,
                                   aChannel);
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
  if (NS_FAILED(rv)) {
    // Skipping top-level load.
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
nsresult UrlClassifierCommon::SetTrackingInfo(
    nsIChannel* aChannel, const nsTArray<nsCString>& aLists,
    const nsTArray<nsCString>& aFullHashes) {
  NS_ENSURE_ARG(!aLists.IsEmpty());

  // Can be called in EITHER the parent or child process.
  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request.
    // Tell the child process channel to do this instead.
    nsAutoCString strLists, strHashes;
    TablesToString(aLists, strLists);
    TablesToString(aFullHashes, strHashes);

    parentChannel->SetClassifierMatchedTrackingInfo(strLists, strHashes);
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (classifiedChannel) {
    classifiedChannel->SetMatchedTrackingInfo(aLists, aFullHashes);
  }

  return NS_OK;
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
  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(uri->GetSpecOrDefault(), *params.AppendElement());
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
                                  params);

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

namespace {

void SetClassificationFlagsHelper(nsIChannel* aChannel,
                                  uint32_t aClassificationFlags,
                                  bool aIsThirdParty) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process
    // request. We should notify the child process as well.
    parentChannel->NotifyClassificationFlags(aClassificationFlags,
                                             aIsThirdParty);
  }

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(aChannel);
  if (httpChannel) {
    httpChannel->AddClassificationFlags(aClassificationFlags, aIsThirdParty);
  }

  RefPtr<ClassifierDummyChannel> dummyChannel = do_QueryObject(aChannel);
  if (dummyChannel) {
    dummyChannel->AddClassificationFlags(aClassificationFlags);
  }
}

void LowerPriorityHelper(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  bool isBlockingResource = false;

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(aChannel));
  if (cos) {
    if (StaticPrefs::network_http_tailing_enabled()) {
      uint32_t cosFlags = 0;
      cos->GetClassFlags(&cosFlags);
      isBlockingResource =
          cosFlags & (nsIClassOfService::UrgentStart |
                      nsIClassOfService::Leader | nsIClassOfService::Unblocked);

      // Requests not allowed to be tailed are usually those with higher
      // prioritization.  That overweights being a tracker: don't throttle
      // them when not in background.
      if (!(cosFlags & nsIClassOfService::TailForbidden)) {
        cos->AddClassFlags(nsIClassOfService::Throttleable);
      }
    } else {
      // Yes, we even don't want to evaluate the isBlockingResource when tailing
      // is off see bug 1395525.

      cos->AddClassFlags(nsIClassOfService::Throttleable);
    }
  }

  if (!isBlockingResource) {
    nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(aChannel);
    if (p) {
      if (UC_LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        aChannel->GetURI(getter_AddRefs(uri));
        nsAutoCString spec;
        uri->GetAsciiSpec(spec);
        spec.Truncate(
            std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
        UC_LOG(("Setting PRIORITY_LOWEST for channel[%p] (%s)", aChannel,
                spec.get()));
      }
      p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
    }
  }
}

}  // namespace

// static
void UrlClassifierCommon::AnnotateChannel(nsIChannel* aChannel,
                                          uint32_t aClassificationFlags,
                                          uint32_t aLoadingState) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIURI> chanURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(chanURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    UC_LOG(
        ("UrlClassifierCommon::AnnotateChannel nsIChannel::GetURI(%p) failed",
         (void*)aChannel));
    return;
  }

  bool isThirdPartyWithTopLevelWinURI =
      nsContentUtils::IsThirdPartyWindowOrChannel(nullptr, aChannel, chanURI);

  UC_LOG(("UrlClassifierCommon::AnnotateChannel, annotating channel[%p]",
          aChannel));

  SetClassificationFlagsHelper(aChannel, aClassificationFlags,
                               isThirdPartyWithTopLevelWinURI);

  // We consider valid tracking flags (based on the current strict vs basic list
  // prefs) and cryptomining (which is not considered as tracking).
  bool validClassificationFlags =
      IsTrackingClassificationFlag(aClassificationFlags) ||
      IsCryptominingClassificationFlag(aClassificationFlags);

  if (validClassificationFlags && isThirdPartyWithTopLevelWinURI) {
    UrlClassifierCommon::NotifyChannelClassifierProtectionDisabled(
        aChannel, aLoadingState);
  }

  if (isThirdPartyWithTopLevelWinURI &&
      StaticPrefs::privacy_trackingprotection_lower_network_priority()) {
    LowerPriorityHelper(aChannel);
  }
}

// static
bool UrlClassifierCommon::IsAllowListed(nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannelInternal> channel = do_QueryInterface(aChannel);
  if (!channel) {
    UC_LOG(("nsChannelClassifier: Not an HTTP channel"));
    return false;
  }

  nsCOMPtr<nsIPrincipal> cbAllowListPrincipal;
  nsresult rv = channel->GetContentBlockingAllowListPrincipal(
      getter_AddRefs(cbAllowListPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (!cbAllowListPrincipal &&
      StaticPrefs::channelclassifier_allowlist_example()) {
    UC_LOG(("nsChannelClassifier: Allowlisting test domain"));
    nsCOMPtr<nsIIOService> ios = services::GetIOService();
    if (NS_WARN_IF(!ios)) {
      return false;
    }

    nsCOMPtr<nsIURI> uri;
    rv = ios->NewURI(NS_LITERAL_CSTRING("http://allowlisted.example.com"),
                     nullptr, nullptr, getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    RefPtr<BasePrincipal> bp = BasePrincipal::CreateContentPrincipal(
        uri, loadInfo->GetOriginAttributes());
    cbAllowListPrincipal = bp.forget();
  }

  bool isAllowListed = false;
  rv = AntiTrackingCommon::IsOnContentBlockingAllowList(
      cbAllowListPrincipal, NS_UsePrivateBrowsing(aChannel), isAllowListed);
  if (NS_FAILED(rv)) {  // normal for some loads, no need to print a warning
    return false;
  }

  if (isAllowListed) {
    if (UC_LOG_ENABLED()) {
      nsCOMPtr<nsIURI> chanURI;
      nsresult rv = aChannel->GetURI(getter_AddRefs(chanURI));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return isAllowListed;
      }

      nsCString chanSpec = chanURI->GetSpecOrDefault();
      chanSpec.Truncate(
          std::min(chanSpec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(("nsChannelClassifier: User override on channel[%p] (%s)",
              aChannel, chanSpec.get()));
    }
  }

  return isAllowListed;
}

// static
bool UrlClassifierCommon::IsTrackingClassificationFlag(uint32_t aFlag) {
  if (StaticPrefs::privacy_annotate_channels_strict_list_enabled() &&
      (aFlag &
       nsIHttpChannel::ClassificationFlags::CLASSIFIED_ANY_STRICT_TRACKING)) {
    return true;
  }

  if (StaticPrefs::privacy_socialtracking_block_cookies_enabled() &&
      (aFlag &
       nsIHttpChannel::ClassificationFlags::CLASSIFIED_ANY_SOCIAL_TRACKING)) {
    return true;
  }

  return (aFlag &
          nsIHttpChannel::ClassificationFlags::CLASSIFIED_ANY_BASIC_TRACKING);
}

// static
bool UrlClassifierCommon::IsCryptominingClassificationFlag(uint32_t aFlag) {
  if (aFlag & nsIHttpChannel::ClassificationFlags::CLASSIFIED_CRYPTOMINING) {
    return true;
  }

  if (StaticPrefs::privacy_annotate_channels_strict_list_enabled() &&
      (aFlag &
       nsIHttpChannel::ClassificationFlags::CLASSIFIED_CRYPTOMINING_CONTENT)) {
    return true;
  }

  return false;
}

void UrlClassifierCommon::TablesToString(const nsTArray<nsCString>& aList,
                                         nsACString& aString) {
  aString.Truncate();

  for (const nsCString& table : aList) {
    if (!aString.IsEmpty()) {
      aString.Append(",");
    }
    aString.Append(table);
  }
}

uint32_t UrlClassifierCommon::TablesToClassificationFlags(
    const nsTArray<nsCString>& aList,
    const std::vector<ClassificationData>& aData, uint32_t aDefaultFlag) {
  uint32_t flags = 0;
  for (const nsCString& table : aList) {
    flags |= TableToClassificationFlag(table, aData);
  }

  if (flags == 0) {
    flags |= aDefaultFlag;
  }

  return flags;
}

uint32_t UrlClassifierCommon::TableToClassificationFlag(
    const nsACString& aTable, const std::vector<ClassificationData>& aData) {
  for (const ClassificationData& data : aData) {
    if (StringBeginsWith(aTable, data.mPrefix)) {
      return data.mFlag;
    }
  }

  return 0;
}

}  // namespace net
}  // namespace mozilla
