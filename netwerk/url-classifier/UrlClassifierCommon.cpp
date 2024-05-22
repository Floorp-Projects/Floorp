/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/UrlClassifierCommon.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_channelclassifier.h"
#include "mozilla/StaticPrefs_security.h"
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
#include "nsReadableUtils.h"

namespace mozilla {
namespace net {

const nsCString::size_type UrlClassifierCommon::sMaxSpecLength = 128;

// MOZ_LOG=nsChannelClassifier:5
LazyLogModule UrlClassifierCommon::sLog("nsChannelClassifier");
LazyLogModule UrlClassifierCommon::sLogLeak("nsChannelClassifierLeak");

/* static */
bool UrlClassifierCommon::AddonMayLoad(nsIChannel* aChannel, nsIURI* aURI) {
  nsCOMPtr<nsILoadInfo> channelLoadInfo = aChannel->LoadInfo();
  // loadingPrincipal is used here to ensure we are loading into an
  // addon principal.  This allows an addon, with explicit permission, to
  // call out to API endpoints that may otherwise get blocked.
  nsIPrincipal* loadingPrincipal = channelLoadInfo->GetLoadingPrincipal();
  if (!loadingPrincipal) {
    return false;
  }

  return BasePrincipal::Cast(loadingPrincipal)->AddonAllowsLoad(aURI, true);
}

/* static */
bool UrlClassifierCommon::ShouldEnableProtectionForChannel(
    nsIChannel* aChannel) {
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
  if (NS_WARN_IF(!channel)) {
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  MOZ_ASSERT(loadInfo);

  auto policyType = loadInfo->GetExternalContentPolicyType();
  if (policyType == ExtContentPolicy::TYPE_DOCUMENT) {
    UC_LOG(
        ("UrlClassifierCommon::ShouldEnableProtectionForChannel - "
         "skipping top-level load for channel %p",
         aChannel));
    return false;
  }

  // Tracking protection will be enabled so return without updating
  // the security state. If any channels are subsequently cancelled
  // (page elements blocked) the state will be then updated.

  return true;
}

/* static */
nsresult UrlClassifierCommon::SetTrackingInfo(
    nsIChannel* aChannel, const nsTArray<nsCString>& aLists,
    const nsTArray<nsCString>& aFullHashes) {
  NS_ENSURE_ARG(!aLists.IsEmpty());

  // Can be called in EITHER the parent or child process.
  nsresult rv;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(aChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (classifiedChannel) {
    classifiedChannel->SetMatchedTrackingInfo(aLists, aFullHashes);
  }

  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request.
    // Tell the child process channel to do this as well.
    // TODO: We can remove the code sending the IPC to content to update
    //       tracking info once we move the ContentBlockingLog into the parent.
    //       This would be done in Bug 1599046.
    nsAutoCString strLists, strHashes;
    TablesToString(aLists, strLists);
    TablesToString(aFullHashes, strHashes);

    parentChannel->SetClassifierMatchedTrackingInfo(strLists, strHashes);
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

  switch (aErrorCode) {
    case NS_ERROR_MALWARE_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_MALWARE_URI);
      break;
    case NS_ERROR_PHISHING_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_PHISHING_URI);
      break;
    case NS_ERROR_UNWANTED_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_UNWANTED_URI);
      break;
    case NS_ERROR_TRACKING_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_TRACKING_URI);
      break;
    case NS_ERROR_BLOCKED_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_BLOCKED_URI);
      break;
    case NS_ERROR_HARMFUL_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_HARMFUL_URI);
      break;
    case NS_ERROR_CRYPTOMINING_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_CRYPTOMINING_URI);
      break;
    case NS_ERROR_FINGERPRINTING_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_FINGERPRINTING_URI);
      break;
    case NS_ERROR_SOCIALTRACKING_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_SOCIALTRACKING_URI);
      break;
    case NS_ERROR_EMAILTRACKING_URI:
      NS_SetRequestBlockingReason(
          channel, nsILoadInfo::BLOCKING_REASON_CLASSIFY_EMAILTRACKING_URI);
      break;
    default:
      MOZ_CRASH(
          "Missing nsILoadInfo::BLOCKING_REASON* for the classification error");
      break;
  }

  // Can be called in EITHER the parent or child process.
  nsresult rv;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (classifiedChannel) {
    classifiedChannel->SetMatchedInfo(aList, aProvider, aFullHash);
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIParentChannel> parentChannel;
    NS_QueryNotificationCallbacks(channel, parentChannel);
    if (parentChannel) {
      // This channel is a parent-process proxy for a child process request.
      // Tell the child process channel to do this as well.
      // TODO: We can remove the code sending the IPC to content to update
      //       matched info once we move the ContentBlockingLog into the parent.
      //       This would be done in Bug 1601063.
      parentChannel->SetClassifierMatchedInfo(aList, aProvider, aFullHash);
    }

    unsigned state =
        UrlClassifierFeatureFactory::GetClassifierBlockingEventCode(aErrorCode);
    if (!state) {
      state = nsIWebProgressListener::STATE_BLOCKED_UNSAFE_CONTENT;
    }
    ContentBlockingNotifier::OnEvent(channel, state);

    return NS_OK;
  }

  // TODO: ReportToConsole is called in the child process,
  // If nsContentUtils::ReportToConsole is not fission compatiable(cannot report
  // to correct top-level window), we need to do this in the parent process
  // instead (find the top-level window in the parent and send an IPC to child
  // processes to report console).
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil;
  thirdPartyUtil = mozilla::components::ThirdPartyUtil::Service();
  if (NS_WARN_IF(!thirdPartyUtil)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingUtils::MaybeGetDocumentURIBeingLoaded(channel);
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
    category = "Safe Browsing"_ns;
  }

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, category, doc,
                                  nsContentUtils::eNECKO_PROPERTIES, message,
                                  params);

  return NS_OK;
}

/* static */
nsresult UrlClassifierCommon::GetTopWindowURI(nsIChannel* aChannel,
                                              nsIURI** aURI) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  MOZ_ASSERT(loadInfo);

  RefPtr<dom::BrowsingContext> browsingContext;
  nsresult rv =
      loadInfo->GetTargetBrowsingContext(getter_AddRefs(browsingContext));
  if (NS_WARN_IF(NS_FAILED(rv)) || !browsingContext) {
    return NS_ERROR_FAILURE;
  }

  dom::CanonicalBrowsingContext* top = browsingContext->Canonical()->Top();
  dom::WindowGlobalParent* wgp = top->GetCurrentWindowGlobal();
  if (!wgp) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsIURI> uri = wgp->GetDocumentURI();
  if (!uri) {
    return NS_ERROR_FAILURE;
  }

  uri.forget(aURI);
  return NS_OK;
}

/* static */
nsresult UrlClassifierCommon::CreatePairwiseEntityListURI(nsIChannel* aChannel,
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
  rv =
      UrlClassifierCommon::GetTopWindowURI(aChannel, getter_AddRefs(topWinURI));
  if (NS_FAILED(rv) || !topWinURI) {
    // SharedWorker and ServiceWorker don't have an associated window, use
    // client's URI instead.
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    MOZ_ASSERT(loadInfo);

    Maybe<dom::ClientInfo> clientInfo = loadInfo->GetClientInfo();
    if (clientInfo.isSome()) {
      if ((clientInfo->Type() == dom::ClientType::Sharedworker) ||
          (clientInfo->Type() == dom::ClientType::Serviceworker)) {
        UC_LOG(
            ("UrlClassifierCommon::CreatePairwiseEntityListURI - "
             "channel %p initiated by worker, get uri from client",
             aChannel));

        auto clientPrincipalOrErr = clientInfo->GetPrincipal();
        if (clientPrincipalOrErr.isOk()) {
          nsCOMPtr<nsIPrincipal> principal = clientPrincipalOrErr.unwrap();
          if (principal) {
            auto* basePrin = BasePrincipal::Cast(principal);
            rv = basePrin->GetURI(getter_AddRefs(topWinURI));
            Unused << NS_WARN_IF(NS_FAILED(rv));
          }
        }
      }
    }

    if (!topWinURI) {
      UC_LOG(
          ("UrlClassifierCommon::CreatePairwiseEntityListURI - "
           "no top-level window associated with channel %p, "
           "get uri from loading principal",
           aChannel));

      nsCOMPtr<nsIPrincipal> principal = loadInfo->GetLoadingPrincipal();
      if (principal) {
        auto* basePrin = BasePrincipal::Cast(principal);
        rv = basePrin->GetURI(getter_AddRefs(topWinURI));
        Unused << NS_WARN_IF(NS_FAILED(rv));
      }
    }
  }

  if (!topWinURI) {
    UC_LOG(
        ("UrlClassifierCommon::CreatePairwiseEntityListURI - "
         "fail to get top-level window uri for channel %p",
         aChannel));

    // Return success because we want to continue to look up even without
    // whitelist.
    return NS_OK;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager;
  securityManager = mozilla::components::ScriptSecurityManager::Service(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> chanPrincipal;
  rv = securityManager->GetChannelURIPrincipal(aChannel,
                                               getter_AddRefs(chanPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Craft a entitylist URL like "toplevel.page/?resource=third.party.domain"
  nsAutoCString pageHostname, resourceDomain;
  rv = topWinURI->GetHost(pageHostname);
  if (NS_FAILED(rv)) {
    // When the top-level page doesn't support GetHost, for example, about:home,
    // we don't return an error here; instead, we return success to make sure
    // that the lookup process calling this API continues to run.
    if (UC_LOG_ENABLED()) {
      nsCString topWinSpec =
          topWinURI ? topWinURI->GetSpecOrDefault() : "(null)"_ns;
      topWinSpec.Truncate(
          std::min(topWinSpec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(
          ("UrlClassifierCommon::CreatePairwiseEntityListURI - "
           "cannot get host from the top-level uri %s of channel %p",
           topWinSpec.get(), aChannel));
    }
    return NS_OK;
  }

  rv = chanPrincipal->GetBaseDomain(resourceDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString entitylistEntry =
      "http://"_ns + pageHostname + "/?resource="_ns + resourceDomain;
  UC_LOG(
      ("UrlClassifierCommon::CreatePairwiseEntityListURI - looking for %s in "
       "the entitylist on channel %p",
       entitylistEntry.get(), aChannel));

  nsCOMPtr<nsIURI> entitylistURI;
  rv = NS_NewURI(getter_AddRefs(entitylistURI), entitylistEntry);
  if (NS_FAILED(rv)) {
    return rv;
  }

  entitylistURI.forget(aURI);
  return NS_OK;
}

namespace {

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
      UC_LOG(
          ("UrlClassifierCommon::LowerPriorityHelper - "
           "setting PRIORITY_LOWEST for channel %p",
           aChannel));
      p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
    }
  }
}

}  // namespace

// static
void UrlClassifierCommon::SetClassificationFlagsHelper(
    nsIChannel* aChannel, uint32_t aClassificationFlags, bool aIsThirdParty) {
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
}

// static
void UrlClassifierCommon::AnnotateChannel(nsIChannel* aChannel,
                                          uint32_t aClassificationFlags,
                                          uint32_t aLoadingState) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIURI> chanURI;
  nsresult rv = aChannel->GetURI(getter_AddRefs(chanURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  bool isThirdPartyWithTopLevelWinURI =
      AntiTrackingUtils::IsThirdPartyChannel(aChannel);

  SetClassificationFlagsHelper(aChannel, aClassificationFlags,
                               isThirdPartyWithTopLevelWinURI);

  // We consider valid tracking flags (based on the current strict vs basic list
  // prefs) and cryptomining (which is not considered as tracking).
  bool validClassificationFlags =
      IsTrackingClassificationFlag(aClassificationFlags,
                                   NS_UsePrivateBrowsing(aChannel)) ||
      IsCryptominingClassificationFlag(aClassificationFlags,
                                       NS_UsePrivateBrowsing(aChannel));

  if (validClassificationFlags && isThirdPartyWithTopLevelWinURI) {
    ContentBlockingNotifier::OnEvent(aChannel, aLoadingState);
  }

  if (isThirdPartyWithTopLevelWinURI &&
      StaticPrefs::privacy_trackingprotection_lower_network_priority()) {
    LowerPriorityHelper(aChannel);
  }
}

// static
bool UrlClassifierCommon::IsAllowListed(nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannelInternal> channel = do_QueryInterface(aChannel);
  if (NS_WARN_IF(!channel)) {
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  bool isAllowListed = false;
  if (StaticPrefs::channelclassifier_allowlist_example()) {
    UC_LOG(
        ("UrlClassifierCommon::IsAllowListed - "
         "check allowlisting test domain on channel %p",
         aChannel));

    nsCOMPtr<nsIIOService> ios = components::IO::Service();
    if (NS_WARN_IF(!ios)) {
      return false;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = ios->NewURI("http://allowlisted.example.com"_ns, nullptr,
                              nullptr, getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    nsCOMPtr<nsIPrincipal> cbAllowListPrincipal =
        BasePrincipal::CreateContentPrincipal(uri,
                                              loadInfo->GetOriginAttributes());

    rv = ContentBlockingAllowList::Check(
        cbAllowListPrincipal, NS_UsePrivateBrowsing(aChannel), isAllowListed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
  } else {
    nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
    MOZ_ALWAYS_SUCCEEDS(
        loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings)));
    isAllowListed = cookieJarSettings->GetIsOnContentBlockingAllowList();
  }

  if (isAllowListed) {
    UC_LOG(("UrlClassifierCommon::IsAllowListed - user override on channel %p",
            aChannel));
  }

  return isAllowListed;
}

// static
bool UrlClassifierCommon::IsTrackingClassificationFlag(uint32_t aFlag,
                                                       bool aIsPrivate) {
  bool isLevel2ListEnabled =
      aIsPrivate
          ? StaticPrefs::privacy_annotate_channels_strict_list_pbmode_enabled()
          : StaticPrefs::privacy_annotate_channels_strict_list_enabled();

  if (isLevel2ListEnabled &&
      (aFlag & nsIClassifiedChannel::ClassificationFlags::
                   CLASSIFIED_ANY_STRICT_TRACKING)) {
    return true;
  }

  if (StaticPrefs::privacy_socialtracking_block_cookies_enabled() &&
      IsSocialTrackingClassificationFlag(aFlag)) {
    return true;
  }

  return (
      aFlag &
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_ANY_BASIC_TRACKING);
}

// static
bool UrlClassifierCommon::IsSocialTrackingClassificationFlag(uint32_t aFlag) {
  return (aFlag & nsIClassifiedChannel::ClassificationFlags::
                      CLASSIFIED_ANY_SOCIAL_TRACKING) != 0;
}

// static
bool UrlClassifierCommon::IsCryptominingClassificationFlag(uint32_t aFlag,
                                                           bool aIsPrivate) {
  if (aFlag &
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_CRYPTOMINING) {
    return true;
  }

  bool isLevel2ListEnabled =
      aIsPrivate
          ? StaticPrefs::privacy_annotate_channels_strict_list_pbmode_enabled()
          : StaticPrefs::privacy_annotate_channels_strict_list_enabled();

  if (isLevel2ListEnabled &&
      (aFlag & nsIClassifiedChannel::ClassificationFlags::
                   CLASSIFIED_CRYPTOMINING_CONTENT)) {
    return true;
  }

  return false;
}

void UrlClassifierCommon::TablesToString(const nsTArray<nsCString>& aList,
                                         nsACString& aString) {
  // Truncate and append rather than assigning because that's more efficient if
  // aString is an nsAutoCString.
  aString.Truncate();
  StringJoinAppend(aString, ","_ns, aList);
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

/* static */
bool UrlClassifierCommon::IsPassiveContent(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType contentType = loadInfo->GetExternalContentPolicyType();

  // Return true if aChannel is loading passive display content, as
  // defined by the mixed content blocker.
  // https://searchfox.org/mozilla-central/rev/c80fa7258c935223fe319c5345b58eae85d4c6ae/dom/security/nsMixedContentBlocker.cpp#532
  return contentType == ExtContentPolicy::TYPE_IMAGE ||
         contentType == ExtContentPolicy::TYPE_MEDIA ||
         (contentType == ExtContentPolicy::TYPE_OBJECT_SUBREQUEST &&
          !StaticPrefs::security_mixed_content_block_object_subrequest());
}

}  // namespace net
}  // namespace mozilla
