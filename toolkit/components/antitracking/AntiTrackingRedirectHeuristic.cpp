/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "AntiTrackingRedirectHeuristic.h"
#include "ContentBlocking.h"
#include "ContentBlockingAllowList.h"
#include "ContentBlockingUserInteraction.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsICookieService.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "nsScriptSecurityManager.h"

namespace mozilla {

namespace {

bool ShouldRedirectHeuristicApplyETP(nsIChannel* aOldChannel, nsIURI* aOldURI,
                                     nsIPrincipal* aOldPrincipal,
                                     nsIChannel* aNewChannel, nsIURI* aNewURI) {
  nsCOMPtr<nsIClassifiedChannel> classifiedOldChannel =
      do_QueryInterface(aOldChannel);
  nsCOMPtr<nsIClassifiedChannel> classifiedNewChannel =
      do_QueryInterface(aNewChannel);
  if (!classifiedOldChannel || !classifiedNewChannel) {
    LOG_SPEC2(("Ignoring redirect for %s to %s because there is not "
               "nsIClassifiedChannel interface",
               _spec1, _spec2),
              aOldURI, aNewURI);
    return false;
  }

  nsCOMPtr<nsILoadInfo> oldLoadInfo = aOldChannel->LoadInfo();
  MOZ_ASSERT(oldLoadInfo);

  nsCOMPtr<nsILoadInfo> newLoadInfo = aNewChannel->LoadInfo();
  MOZ_ASSERT(newLoadInfo);

  bool allowedByPreviousRedirect =
      oldLoadInfo->GetAllowListFutureDocumentsCreatedFromThisRedirectChain();

  // We're looking at the first-party classification flags because we're
  // interested in first-party redirects.
  uint32_t newClassificationFlags =
      classifiedNewChannel->GetFirstPartyClassificationFlags();

  if (net::UrlClassifierCommon::IsTrackingClassificationFlag(
          newClassificationFlags)) {
    // This is not a tracking -> non-tracking redirect.
    LOG_SPEC2(("Redirect for %s to %s because it's not tracking to "
               "non-tracking. Part of a chain of granted redirects: %d",
               _spec1, _spec2, allowedByPreviousRedirect),
              aOldURI, aNewURI);
    newLoadInfo->SetAllowListFutureDocumentsCreatedFromThisRedirectChain(
        allowedByPreviousRedirect);
    return false;
  }

  if (!ContentBlockingUserInteraction::Exists(aOldPrincipal)) {
    LOG_SPEC2(("Ignoring redirect for %s to %s because no user-interaction on "
               "tracker",
               _spec1, _spec2),
              aOldURI, aNewURI);
    return false;
  }

  uint32_t oldClassificationFlags =
      classifiedOldChannel->GetFirstPartyClassificationFlags();

  if (!net::UrlClassifierCommon::IsTrackingClassificationFlag(
          oldClassificationFlags) &&
      !allowedByPreviousRedirect) {
    // This is not a tracking -> non-tracking redirect.
    LOG_SPEC2(
        ("Redirect for %s to %s because it's not tracking to non-tracking.",
         _spec1, _spec2),
        aOldURI, aNewURI);
    return false;
  }

  return true;
}

bool ShouldRedirectHeuristicApplyRejectForeign(nsIChannel* aOldChannel,
                                               nsIURI* aOldURI,
                                               nsIPrincipal* aOldPrincipal,
                                               nsIChannel* aNewChannel,
                                               nsIURI* aNewURI) {
  if (!ContentBlockingUserInteraction::Exists(aOldPrincipal)) {
    LOG_SPEC2(("Ignoring redirect for %s to %s because no user-interaction on "
               "tracker",
               _spec1, _spec2),
              aOldURI, aNewURI);
    return false;
  }

  return true;
}

bool ShouldRedirectHeuristicApply(nsIChannel* aOldChannel, nsIURI* aOldURI,
                                  nsIPrincipal* aOldPrincipal,
                                  nsIChannel* aNewChannel, nsIURI* aNewURI) {
  nsCOMPtr<nsILoadInfo> oldLoadInfo = aOldChannel->LoadInfo();
  MOZ_ASSERT(oldLoadInfo);

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;

  nsresult rv =
      oldLoadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't obtain the principal from the redirect chain"));
    return false;
  }

  uint32_t cookieBehavior = cookieJarSettings->GetCookieBehavior();
  if (cookieBehavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      cookieBehavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    return ShouldRedirectHeuristicApplyETP(aOldChannel, aOldURI, aOldPrincipal,
                                           aNewChannel, aNewURI);
  }

  if (cookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
      StaticPrefs::network_cookie_rejectForeignWithExceptions_enabled()) {
    return ShouldRedirectHeuristicApplyRejectForeign(
        aOldChannel, aOldURI, aOldPrincipal, aNewChannel, aNewURI);
  }

  LOG((
      "Heuristic doesn't apply because the cookieBehavior doesn't require it"));
  return false;
}

}  // namespace

void AntiTrackingRedirectHeuristic(nsIChannel* aOldChannel, nsIURI* aOldURI,
                                   nsIChannel* aNewChannel, nsIURI* aNewURI) {
  MOZ_ASSERT(aOldChannel);
  MOZ_ASSERT(aOldURI);
  MOZ_ASSERT(aNewChannel);
  MOZ_ASSERT(aNewURI);

  nsresult rv;

  // This heuristic works only on the parent process.
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (!StaticPrefs::privacy_restrict3rdpartystorage_heuristic_redirect()) {
    return;
  }

  nsCOMPtr<nsIHttpChannel> oldChannel = do_QueryInterface(aOldChannel);
  if (!oldChannel) {
    return;
  }

  nsCOMPtr<nsIHttpChannel> newChannel = do_QueryInterface(aNewChannel);
  if (!newChannel) {
    return;
  }

  LOG_SPEC(("Checking redirect-heuristic for %s", _spec), aOldURI);

  nsCOMPtr<nsILoadInfo> oldLoadInfo = aOldChannel->LoadInfo();
  MOZ_ASSERT(oldLoadInfo);

  nsCOMPtr<nsILoadInfo> newLoadInfo = aNewChannel->LoadInfo();
  MOZ_ASSERT(newLoadInfo);

  nsContentPolicyType contentType = oldLoadInfo->GetExternalContentPolicyType();
  if (contentType != nsIContentPolicy::TYPE_DOCUMENT ||
      !aOldChannel->IsDocument()) {
    LOG_SPEC(("Ignoring redirect for %s because it's not a document", _spec),
             aOldURI);
    // We care about document redirects only.
    return;
  }

  nsIScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  MOZ_ASSERT(ssm);

  nsCOMPtr<nsIPrincipal> oldPrincipal;

  const nsTArray<nsCOMPtr<nsIRedirectHistoryEntry>>& chain =
      oldLoadInfo->RedirectChain();

  if (oldLoadInfo->GetAllowListFutureDocumentsCreatedFromThisRedirectChain() &&
      !chain.IsEmpty()) {
    rv = chain[0]->GetPrincipal(getter_AddRefs(oldPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Can't obtain the principal from the redirect chain"));
      return;
    }
  } else {
    rv = ssm->GetChannelResultPrincipal(aOldChannel,
                                        getter_AddRefs(oldPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Can't obtain the principal from the previous channel"));
      return;
    }
  }

  if (!ShouldRedirectHeuristicApply(aOldChannel, aOldURI, oldPrincipal,
                                    aNewChannel, aNewURI)) {
    return;
  }

  nsAutoCString oldOrigin;
  rv = oldPrincipal->GetOrigin(oldOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the origin from the Principal"));
    return;
  }

  nsAutoCString newOrigin;
  rv = nsContentUtils::GetASCIIOrigin(aNewURI, newOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the origin from the URI"));
    return;
  }

  LOG(("Adding a first-party storage exception for %s...",
       PromiseFlatCString(newOrigin).get()));

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  rv = oldLoadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the cookieJarSettings"));
    return;
  }

  int32_t behavior = cookieJarSettings->GetCookieBehavior();

  if (!cookieJarSettings->GetRejectThirdPartyContexts()) {
    LOG(
        ("Disabled by network.cookie.cookieBehavior pref (%d), bailing out "
         "early",
         behavior));
    return;
  }

  MOZ_ASSERT(
      behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER ||
      behavior ==
          nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN ||
      net::CookieJarSettings::IsRejectThirdPartyWithExceptions(behavior));

  if (ContentBlockingAllowList::Check(newChannel)) {
    return;
  }

  LOG(("Saving the permission: oldOrigin=%s, grantedOrigin=%s", oldOrigin.get(),
       newOrigin.get()));

  // Any new redirect from this loadInfo must be considered as granted.
  newLoadInfo->SetAllowListFutureDocumentsCreatedFromThisRedirectChain(true);

  uint64_t innerWindowID;
  Unused << newChannel->GetTopLevelContentWindowId(&innerWindowID);

  nsAutoString errorText;
  AutoTArray<nsString, 2> params = {NS_ConvertUTF8toUTF16(newOrigin),
                                    NS_ConvertUTF8toUTF16(oldOrigin)};
  rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eNECKO_PROPERTIES, "CookieAllowedForOriginByHeuristic",
      params, errorText);
  if (NS_SUCCEEDED(rv)) {
    nsContentUtils::ReportToConsoleByWindowID(
        errorText, nsIScriptError::warningFlag, ANTITRACKING_CONSOLE_CATEGORY,
        innerWindowID);
  }

  nsCOMPtr<nsIPrincipal> newPrincipal;
  rv =
      ssm->GetChannelResultPrincipal(aNewChannel, getter_AddRefs(newPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't obtain the principal from the redirected"));
    return;
  }

  // We don't care about this promise because the operation is actually sync.
  RefPtr<ContentBlocking::ParentAccessGrantPromise> promise =
      ContentBlocking::SaveAccessForOriginOnParentProcess(
          newPrincipal, oldPrincipal, oldOrigin,
          ContentBlocking::StorageAccessPromptChoices::eAllow,
          StaticPrefs::privacy_restrict3rdpartystorage_expiration_redirect());
  Unused << promise;
}

}  // namespace mozilla
