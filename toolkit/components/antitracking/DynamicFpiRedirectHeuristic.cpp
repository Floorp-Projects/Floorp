/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "DynamicFpiRedirectHeuristic.h"
#include "ContentBlocking.h"
#include "ContentBlockingAllowList.h"
#include "ContentBlockingUserInteraction.h"

#include "mozilla/net/HttpBaseChannel.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsICookieService.h"
#include "nsIEffectiveTLDService.h"
#include "nsINavHistoryService.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsScriptSecurityManager.h"
#include "nsToolkitCompsCID.h"

namespace mozilla {

// check if there's any interacting visit within the given seconds
static bool HasEligibleVisit(
    const nsACString& aBaseDomain,
    int64_t aSinceInSec = StaticPrefs::
        privacy_restrict3rdpartystorage_heuristic_recently_visited_time()) {
  nsresult rv;

  nsCOMPtr<nsINavHistoryService> histSrv =
      do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
  if (!histSrv) {
    return false;
  }
  nsCOMPtr<nsINavHistoryQuery> histQuery;
  rv = histSrv->GetNewQuery(getter_AddRefs(histQuery));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = histQuery->SetDomain(aBaseDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = histQuery->SetDomainIsHost(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  PRTime beginTime = PR_Now() - PRTime(PR_USEC_PER_SEC) * aSinceInSec;
  rv = histQuery->SetBeginTime(beginTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCOMPtr<nsINavHistoryQueryOptions> histQueryOpts;
  rv = histSrv->GetNewQueryOptions(getter_AddRefs(histQueryOpts));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv =
      histQueryOpts->SetResultType(nsINavHistoryQueryOptions::RESULTS_AS_VISIT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = histQueryOpts->SetMaxResults(1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = histQueryOpts->SetQueryType(
      nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCOMPtr<nsINavHistoryResult> histResult;
  rv = histSrv->ExecuteQuery(histQuery, histQueryOpts,
                             getter_AddRefs(histResult));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCOMPtr<nsINavHistoryContainerResultNode> histResultContainer;
  rv = histResult->GetRoot(getter_AddRefs(histResultContainer));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = histResultContainer->SetContainerOpen(true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t childCount = 0;
  rv = histResultContainer->GetChildCount(&childCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = histResultContainer->SetContainerOpen(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return childCount > 0;
}

static nsresult GetBaseDomain(nsIURI* aURI, nsACString& aBaseDomain) {
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

  if (!tldService) {
    return NS_ERROR_FAILURE;
  }

  return tldService->GetBaseDomain(aURI, 0, aBaseDomain);
}

static void AddConsoleReport(nsIChannel* aNewChannel, nsIURI* aNewURI,
                             const nsACString& aOldOrigin,
                             const nsACString& aNewOrigin) {
  nsCOMPtr<net::HttpBaseChannel> httpChannel = do_QueryInterface(aNewChannel);
  if (!httpChannel) {
    return;
  }

  nsAutoCString uri;
  nsresult rv = aNewURI->GetSpec(uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  AutoTArray<nsString, 2> params = {NS_ConvertUTF8toUTF16(aNewOrigin),
                                    NS_ConvertUTF8toUTF16(aOldOrigin)};

  httpChannel->AddConsoleReport(
      nsIScriptError::warningFlag, ANTITRACKING_CONSOLE_CATEGORY,
      nsContentUtils::eNECKO_PROPERTIES, uri, 0, 0,
      NS_LITERAL_CSTRING("CookieAllowedForFpiByHeuristic"), params);
}

void DynamicFpiRedirectHeuristic(nsIChannel* aOldChannel, nsIURI* aOldURI,
                                 nsIChannel* aNewChannel, nsIURI* aNewURI) {
  MOZ_ASSERT(aOldChannel);
  MOZ_ASSERT(aOldURI);
  MOZ_ASSERT(aNewChannel);
  MOZ_ASSERT(aNewURI);

  nsresult rv;

  if (!StaticPrefs::
          privacy_restrict3rdpartystorage_heuristic_recently_visited()) {
    return;
  }

  nsCOMPtr<nsIHttpChannel> oldChannel = do_QueryInterface(aOldChannel);
  nsCOMPtr<nsIHttpChannel> newChannel = do_QueryInterface(aNewChannel);
  if (!oldChannel || !newChannel) {
    return;
  }

  LOG_SPEC(("Checking dfpi redirect-heuristic for %s", _spec), aOldURI);

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

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  rv = oldLoadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't get the cookieJarSettings"));
    return;
  }

  int32_t behavior = cookieJarSettings->GetCookieBehavior();
  if (behavior !=
      nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN) {
    LOG(
        ("Disabled by network.cookie.cookieBehavior pref (%d), bailing out "
         "early",
         behavior));
    return;
  }

  nsIScriptSecurityManager* ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();
  MOZ_ASSERT(ssm);

  nsCOMPtr<nsIPrincipal> oldPrincipal;
  const nsTArray<nsCOMPtr<nsIRedirectHistoryEntry>>& chain =
      oldLoadInfo->RedirectChain();
  if (!chain.IsEmpty()) {
    rv = chain[0]->GetPrincipal(getter_AddRefs(oldPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Can't obtain the principal from the redirect chain"));
      return;
    }
  } else {
    rv = ssm->GetChannelResultPrincipal(aOldChannel,
                                        getter_AddRefs(oldPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Can't obtain the principal from the old channel"));
      return;
    }
  }

  nsCOMPtr<nsIPrincipal> newPrincipal;
  rv =
      ssm->GetChannelResultPrincipal(aNewChannel, getter_AddRefs(newPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Can't obtain the principal from the new channel"));
    return;
  }

  if (oldPrincipal->Equals(newPrincipal)) {
    LOG(("No permission needed for same principals."));
    return;
  }

  if (!ContentBlockingUserInteraction::Exists(oldPrincipal) ||
      !ContentBlockingUserInteraction::Exists(newPrincipal)) {
    LOG_SPEC2(("Ignoring redirect for %s to %s because no user-interaction on "
               "both pages",
               _spec1, _spec2),
              aOldURI, aNewURI);
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

  nsAutoCString baseDomainOld, baseDomainNew;
  GetBaseDomain(aOldURI, baseDomainOld);
  GetBaseDomain(aNewURI, baseDomainNew);
  if (!HasEligibleVisit(baseDomainOld) || !HasEligibleVisit(baseDomainNew)) {
    LOG(("No previous visit record, bailing out early."));
    return;
  }

  LOG(("Adding a first-party storage exception for %s...",
       PromiseFlatCString(newOrigin).get()));

  LOG(("Saving the permission: oldOrigin=%s, grantedOrigin=%s", oldOrigin.get(),
       newOrigin.get()));

  AddConsoleReport(aNewChannel, aNewURI, oldOrigin, newOrigin);

  // We don't care about this promise because the operation is actually sync.
  RefPtr<ContentBlocking::ParentAccessGrantPromise> promise =
      ContentBlocking::SaveAccessForOriginOnParentProcess(
          newPrincipal, oldPrincipal, oldOrigin,
          ContentBlocking::StorageAccessPromptChoices::eAllow,
          StaticPrefs::privacy_restrict3rdpartystorage_expiration_visited());
  Unused << promise;
}

}  // namespace mozilla
