/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Document.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StorageAccess.h"
#include "nsContentUtils.h"
#include "nsICookiePermission.h"
#include "nsICookieService.h"
#include "nsICookieJarSettings.h"
#include "nsIPermission.h"
#include "nsIWebProgressListener.h"
#include "nsSandboxFlags.h"

using namespace mozilla;
using namespace mozilla::dom;

/**
 * Gets the cookie lifetime policy for a given cookieJarSettings and a given
 * principal by checking the permission value.
 *
 * Used in the implementation of InternalStorageAllowedCheck.
 */
static void GetCookieLifetimePolicyFromCookieJarSettings(
    nsICookieJarSettings* aCookieJarSettings, nsIPrincipal* aPrincipal,
    uint32_t* aLifetimePolicy) {
  *aLifetimePolicy = StaticPrefs::network_cookie_lifetimePolicy();

  if (aCookieJarSettings) {
    uint32_t cookiePermission = 0;
    nsresult rv =
        aCookieJarSettings->CookiePermission(aPrincipal, &cookiePermission);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    switch (cookiePermission) {
      case nsICookiePermission::ACCESS_ALLOW:
        *aLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
        break;
      case nsICookiePermission::ACCESS_DENY:
        *aLifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
        break;
      case nsICookiePermission::ACCESS_SESSION:
        *aLifetimePolicy = nsICookieService::ACCEPT_SESSION;
        break;
    }
  }
}

/*
 * Checks if storage for a given principal is permitted by the user's
 * preferences. If aWindow is non-null, its principal must be passed as
 * aPrincipal, and the third-party iframe and sandboxing status of the window
 * are also checked.  If aURI is non-null, then it is used as the comparison
 * against aWindow to determine if this is a third-party load.  We also
 * allow a channel instead of the window reference when determining 3rd party
 * status.
 *
 * Used in the implementation of StorageAllowedForWindow,
 * StorageAllowedForChannel and StorageAllowedForServiceWorker.
 */
static StorageAccess InternalStorageAllowedCheck(
    nsIPrincipal* aPrincipal, nsPIDOMWindowInner* aWindow, nsIURI* aURI,
    nsIChannel* aChannel, nsICookieJarSettings* aCookieJarSettings,
    uint32_t& aRejectedReason) {
  MOZ_ASSERT(aPrincipal);

  aRejectedReason = 0;

  StorageAccess access = StorageAccess::eAllow;

  // We don't allow storage on the null principal, in general. Even if the
  // calling context is chrome.
  if (aPrincipal->GetIsNullPrincipal()) {
    return StorageAccess::eDeny;
  }

  nsCOMPtr<nsIURI> documentURI;
  if (aWindow) {
    // If the document is sandboxed, then it is not permitted to use storage
    Document* document = aWindow->GetExtantDoc();
    if (document && document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
      return StorageAccess::eDeny;
    }

    // Check if we are in private browsing, and record that fact
    if (nsContentUtils::IsInPrivateBrowsing(document)) {
      access = StorageAccess::ePrivateBrowsing;
    }

    // Get the document URI for the below about: URI check.
    documentURI = document ? document->GetDocumentURI() : nullptr;
  }

  uint32_t lifetimePolicy;

  // WebExtensions principals always get BEHAVIOR_ACCEPT as cookieBehavior
  // and ACCEPT_NORMALLY as lifetimePolicy (See Bug 1406675 for rationale).
  auto policy = BasePrincipal::Cast(aPrincipal)->AddonPolicy();

  if (policy) {
    lifetimePolicy = nsICookieService::ACCEPT_NORMALLY;
  } else {
    GetCookieLifetimePolicyFromCookieJarSettings(aCookieJarSettings, aPrincipal,
                                                 &lifetimePolicy);
  }

  // Check if we should only allow storage for the session, and record that fact
  if (lifetimePolicy == nsICookieService::ACCEPT_SESSION) {
    // Storage could be StorageAccess::ePrivateBrowsing or StorageAccess::eAllow
    // so perform a std::min comparison to make sure we preserve
    // ePrivateBrowsing if it has been set.
    access = std::min(StorageAccess::eSessionScoped, access);
  }

  // About URIs are allowed to access storage, even if they don't have chrome
  // privileges. If this is not desired, than the consumer will have to
  // implement their own restriction functionality.
  //
  // This is due to backwards-compatibility and the state of storage access
  // before the introducton of InternalStorageAllowedCheck:
  //
  // BEFORE:
  // localStorage, caches: allowed in 3rd-party iframes always
  // IndexedDB: allowed in 3rd-party iframes only if 3rd party URI is an about:
  //   URI within a specific whitelist
  //
  // AFTER:
  // localStorage, caches: allowed in 3rd-party iframes by default. Preference
  //   can be set to disable in 3rd-party, which will not disallow in about:
  //   URIs.
  // IndexedDB: allowed in 3rd-party iframes by default. Preference can be set
  //   to disable in 3rd-party, which will disallow in about: URIs, unless they
  //   are within a specific whitelist.
  //
  // This means that behavior for storage with internal about: URIs should not
  // be affected, which is desireable due to the lack of automated testing for
  // about: URIs with these preferences set, and the importance of the correct
  // functioning of these URIs even with custom preferences.
  //
  // We need to check the aURI or the document URI here instead of only checking
  // the URI from the principal. Because the principal might not have a URI if
  // it is a system principal.
  if ((aURI && aURI->SchemeIs("about")) ||
      (documentURI && documentURI->SchemeIs("about")) ||
      aPrincipal->SchemeIs("about")) {
    return access;
  }

  if (!StorageDisabledByAntiTracking(aWindow, aChannel, aPrincipal, aURI,
                                     aRejectedReason)) {
    return access;
  }

  // We want to have a partitioned storage only for trackers.
  if (aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER) {
    return StorageAccess::ePartitionTrackersOrDeny;
  }

  // We want to have a partitioned storage for all third parties.
  if (aRejectedReason ==
      nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN) {
    return StorageAccess::ePartitionForeignOrDeny;
  }

  return StorageAccess::eDeny;
}

static bool StorageDisabledByAntiTrackingInternal(
    nsPIDOMWindowInner* aWindow, nsIChannel* aChannel, nsIPrincipal* aPrincipal,
    nsIURI* aURI, nsICookieJarSettings* aCookieJarSettings,
    uint32_t& aRejectedReason) {
  MOZ_ASSERT(aWindow || aChannel || aPrincipal);

  if (aWindow) {
    nsIURI* documentURI = aURI ? aURI : aWindow->GetDocumentURI();
    return !documentURI || !ContentBlocking::ShouldAllowAccessFor(
                               aWindow, documentURI, &aRejectedReason);
  }

  if (aChannel) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    return !ContentBlocking::ShouldAllowAccessFor(aChannel, uri,
                                                  &aRejectedReason);
  }

  MOZ_ASSERT(aPrincipal);
  return !ContentBlocking::ShouldAllowAccessFor(aPrincipal, aCookieJarSettings);
}

namespace mozilla {

StorageAccess StorageAllowedForWindow(nsPIDOMWindowInner* aWindow,
                                      uint32_t* aRejectedReason) {
  uint32_t rejectedReason;
  if (!aRejectedReason) {
    aRejectedReason = &rejectedReason;
  }

  *aRejectedReason = 0;

  if (Document* document = aWindow->GetExtantDoc()) {
    nsCOMPtr<nsIPrincipal> principal = document->NodePrincipal();
    // Note that GetChannel() below may return null, but that's OK, since the
    // callee is able to deal with a null channel argument, and if passed null,
    // will only fail to notify the UI in case storage gets blocked.
    nsIChannel* channel = document->GetChannel();
    return InternalStorageAllowedCheck(principal, aWindow, nullptr, channel,
                                       document->CookieJarSettings(),
                                       *aRejectedReason);
  }

  // No document? Let's return a generic rejected reason.
  return StorageAccess::eDeny;
}

StorageAccess StorageAllowedForDocument(const Document* aDoc) {
  MOZ_ASSERT(aDoc);

  if (nsPIDOMWindowInner* inner = aDoc->GetInnerWindow()) {
    nsCOMPtr<nsIPrincipal> principal = aDoc->NodePrincipal();
    // Note that GetChannel() below may return null, but that's OK, since the
    // callee is able to deal with a null channel argument, and if passed null,
    // will only fail to notify the UI in case storage gets blocked.
    nsIChannel* channel = aDoc->GetChannel();

    uint32_t rejectedReason = 0;
    return InternalStorageAllowedCheck(
        principal, inner, nullptr, channel,
        const_cast<Document*>(aDoc)->CookieJarSettings(), rejectedReason);
  }

  return StorageAccess::eDeny;
}

StorageAccess StorageAllowedForNewWindow(nsIPrincipal* aPrincipal, nsIURI* aURI,
                                         nsPIDOMWindowInner* aParent) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aURI);
  // parent may be nullptr

  uint32_t rejectedReason = 0;
  nsCOMPtr<nsICookieJarSettings> cjs;
  if (aParent && aParent->GetExtantDoc()) {
    cjs = aParent->GetExtantDoc()->CookieJarSettings();
  } else {
    cjs = net::CookieJarSettings::Create();
  }
  return InternalStorageAllowedCheck(aPrincipal, aParent, aURI, nullptr, cjs,
                                     rejectedReason);
}

StorageAccess StorageAllowedForChannel(nsIChannel* aChannel) {
  MOZ_DIAGNOSTIC_ASSERT(nsContentUtils::GetSecurityManager());
  MOZ_DIAGNOSTIC_ASSERT(aChannel);

  nsCOMPtr<nsIPrincipal> principal;
  Unused << nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      aChannel, getter_AddRefs(principal));
  NS_ENSURE_TRUE(principal, StorageAccess::eDeny);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  nsresult rv =
      loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  NS_ENSURE_SUCCESS(rv, StorageAccess::eDeny);

  uint32_t rejectedReason = 0;
  StorageAccess result = InternalStorageAllowedCheck(
      principal, nullptr, nullptr, aChannel, cookieJarSettings, rejectedReason);

  return result;
}

StorageAccess StorageAllowedForServiceWorker(
    nsIPrincipal* aPrincipal, nsICookieJarSettings* aCookieJarSettings) {
  uint32_t rejectedReason = 0;
  return InternalStorageAllowedCheck(aPrincipal, nullptr, nullptr, nullptr,
                                     aCookieJarSettings, rejectedReason);
}

bool StorageDisabledByAntiTracking(nsPIDOMWindowInner* aWindow,
                                   nsIChannel* aChannel,
                                   nsIPrincipal* aPrincipal, nsIURI* aURI,
                                   uint32_t& aRejectedReason) {
  MOZ_ASSERT(aWindow || aChannel || aPrincipal);
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (aWindow) {
    if (aWindow->GetExtantDoc()) {
      cookieJarSettings = aWindow->GetExtantDoc()->CookieJarSettings();
    }
  } else if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    Unused << loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  }
  if (!cookieJarSettings) {
    cookieJarSettings = net::CookieJarSettings::Create();
  }
  bool disabled = StorageDisabledByAntiTrackingInternal(
      aWindow, aChannel, aPrincipal, aURI, cookieJarSettings, aRejectedReason);
  if (aWindow) {
    ContentBlockingNotifier::OnDecision(
        aWindow,
        disabled ? ContentBlockingNotifier::BlockingDecision::eBlock
                 : ContentBlockingNotifier::BlockingDecision::eAllow,
        aRejectedReason);
  } else if (aChannel) {
    ContentBlockingNotifier::OnDecision(
        aChannel,
        disabled ? ContentBlockingNotifier::BlockingDecision::eBlock
                 : ContentBlockingNotifier::BlockingDecision::eAllow,
        aRejectedReason);
  }
  return disabled;
}

bool ShouldPartitionStorage(StorageAccess aAccess) {
  return aAccess == StorageAccess::ePartitionTrackersOrDeny ||
         aAccess == StorageAccess::ePartitionForeignOrDeny;
}

bool ShouldPartitionStorage(uint32_t aRejectedReason) {
  return aRejectedReason ==
             nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
         aRejectedReason ==
             nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER ||
         aRejectedReason ==
             nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
}

bool StoragePartitioningEnabled(StorageAccess aAccess,
                                nsICookieJarSettings* aCookieJarSettings) {
  if (aAccess == StorageAccess::ePartitionTrackersOrDeny) {
    return aCookieJarSettings->GetCookieBehavior() ==
               nsICookieService::BEHAVIOR_REJECT_TRACKER &&
           StaticPrefs::privacy_storagePrincipal_enabledForTrackers();
  }
  if (aAccess == StorageAccess::ePartitionForeignOrDeny) {
    return aCookieJarSettings->GetCookieBehavior() ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  }
  return false;
}

bool StoragePartitioningEnabled(uint32_t aRejectedReason,
                                nsICookieJarSettings* aCookieJarSettings) {
  if (aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aRejectedReason ==
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER) {
    return aCookieJarSettings->GetCookieBehavior() ==
               nsICookieService::BEHAVIOR_REJECT_TRACKER &&
           StaticPrefs::privacy_storagePrincipal_enabledForTrackers();
  }
  if (aRejectedReason ==
      nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN) {
    return aCookieJarSettings->GetCookieBehavior() ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  }
  return false;
}

}  // namespace mozilla
