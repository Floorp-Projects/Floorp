/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingUtils.h"

#include "AntiTrackingLog.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/PermissionManager.h"
#include "mozIThirdPartyUtil.h"
#include "nsEffectiveTLDService.h"
#include "nsGlobalWindowInner.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIPermission.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsSandboxFlags.h"
#include "nsScriptSecurityManager.h"
#include "PartitioningExceptionList.h"

#define ANTITRACKING_PERM_KEY "3rdPartyStorage"

using namespace mozilla;
using namespace mozilla::dom;

/* static */ already_AddRefed<nsPIDOMWindowInner>
AntiTrackingUtils::GetInnerWindow(BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);

  nsCOMPtr<nsPIDOMWindowOuter> outer = aBrowsingContext->GetDOMWindow();
  if (!outer) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = outer->GetCurrentInnerWindow();
  return inner.forget();
}

/* static */ already_AddRefed<nsPIDOMWindowOuter>
AntiTrackingUtils::GetTopWindow(nsPIDOMWindowInner* aWindow) {
  Document* document = aWindow->GetExtantDoc();
  if (!document) {
    return nullptr;
  }

  nsIChannel* channel = document->GetChannel();
  if (!channel) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin =
      aWindow->GetBrowsingContext()->Top()->GetDOMWindow();

  if (!pwin) {
    return nullptr;
  }

  return pwin.forget();
}

/* static */
already_AddRefed<nsIURI> AntiTrackingUtils::MaybeGetDocumentURIBeingLoaded(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uriBeingLoaded;
  nsLoadFlags loadFlags = 0;
  nsresult rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
    // If the channel being loaded is a document channel, this call may be
    // coming from an OnStopRequest notification, which might mean that our
    // document may still be in the loading process, so we may need to pass in
    // the uriBeingLoaded argument explicitly.
    rv = aChannel->GetURI(getter_AddRefs(uriBeingLoaded));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  return uriBeingLoaded.forget();
}

// static
void AntiTrackingUtils::CreateStoragePermissionKey(
    const nsACString& aTrackingOrigin, nsACString& aPermissionKey) {
  MOZ_ASSERT(aPermissionKey.IsEmpty());

  static const nsLiteralCString prefix =
      nsLiteralCString(ANTITRACKING_PERM_KEY "^");

  aPermissionKey.SetCapacity(prefix.Length() + aTrackingOrigin.Length());
  aPermissionKey.Append(prefix);
  aPermissionKey.Append(aTrackingOrigin);
}

// static
bool AntiTrackingUtils::CreateStoragePermissionKey(nsIPrincipal* aPrincipal,
                                                   nsACString& aKey) {
  if (!aPrincipal) {
    return false;
  }

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOriginNoSuffix(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  CreateStoragePermissionKey(origin, aKey);
  return true;
}

// static
bool AntiTrackingUtils::CreateStorageRequestPermissionKey(
    nsIURI* aURI, nsACString& aPermissionKey) {
  MOZ_ASSERT(aPermissionKey.IsEmpty());
  RefPtr<nsEffectiveTLDService> eTLDService =
      nsEffectiveTLDService::GetInstance();
  if (!eTLDService) {
    return false;
  }
  nsCString site;
  nsresult rv = eTLDService->GetSite(aURI, site);
  if (NS_FAILED(rv)) {
    return false;
  }
  static const nsLiteralCString prefix =
      nsLiteralCString("AllowStorageAccessRequest^");
  aPermissionKey.SetCapacity(prefix.Length() + site.Length());
  aPermissionKey.Append(prefix);
  aPermissionKey.Append(site);
  return true;
}

// static
bool AntiTrackingUtils::IsStorageAccessPermission(nsIPermission* aPermission,
                                                  nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPermission);
  MOZ_ASSERT(aPrincipal);

  // The permission key may belong either to a tracking origin on the same
  // origin as the granted origin, or on another origin as the granted origin
  // (for example when a tracker in a third-party context uses window.open to
  // open another origin where that second origin would be the granted origin.)
  // But even in the second case, the type of the permission would still be
  // formed by concatenating the granted origin to the end of the type name
  // (see CreatePermissionKey).  Therefore, we pass in the same argument to
  // both tracking origin and granted origin here in order to compute the
  // shorter permission key and will then do a prefix match on the type of the
  // input permission to see if it is a storage access permission or not.
  nsAutoCString permissionKey;
  bool result = CreateStoragePermissionKey(aPrincipal, permissionKey);
  if (NS_WARN_IF(!result)) {
    return false;
  }

  nsAutoCString type;
  nsresult rv = aPermission->GetType(type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return StringBeginsWith(type, permissionKey);
}

// static
Maybe<size_t> AntiTrackingUtils::CountSitesAllowStorageAccess(
    nsIPrincipal* aPrincipal) {
  PermissionManager* permManager = PermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    return Nothing();
  }

  nsAutoCString prefix;
  AntiTrackingUtils::CreateStoragePermissionKey(aPrincipal, prefix);

  using Permissions = nsTArray<RefPtr<nsIPermission>>;
  Permissions perms;
  nsresult rv = permManager->GetAllWithTypePrefix(prefix, perms);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Nothing();
  }

  // Create a set of unique origins
  using Origins = nsTArray<nsCString>;
  Origins origins;

  // Iterate over all permissions that have a prefix equal to the expected
  // permission we are looking for. This includes two things we need to remove:
  // duplicates and origin strings that have a prefix of aPrincipal's origin
  // string, e.g. https://example.company when aPrincipal is
  // https://example.com.
  for (const auto& perm : perms) {
    nsAutoCString type;
    rv = perm->GetType(type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Nothing();
    }
    // Let's make sure that we're not looking at a permission for
    // https://exampletracker.company when we mean to look for the
    // permission for https://exampletracker.com!
    if (type != prefix) {
      continue;
    }

    nsCOMPtr<nsIPrincipal> principal;
    rv = perm->GetPrincipal(getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Nothing();
    }

    nsAutoCString origin;
    rv = principal->GetOrigin(origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Nothing();
    }

    ToLowerCase(origin);

    // Append if it would not be a duplicate
    if (origins.IndexOf(origin) == Origins::NoIndex) {
      origins.AppendElement(origin);
    }
  }

  return Some(origins.Length());
}

// static
bool AntiTrackingUtils::CheckStoragePermission(nsIPrincipal* aPrincipal,
                                               const nsAutoCString& aType,
                                               bool aIsInPrivateBrowsing,
                                               uint32_t* aRejectedReason,
                                               uint32_t aBlockedReason) {
  PermissionManager* permManager = PermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    LOG(("Failed to obtain the permission manager"));
    return false;
  }

  uint32_t result = 0;
  if (aIsInPrivateBrowsing) {
    LOG_PRIN(("Querying the permissions for private modei looking for a "
              "permission of type %s for %s",
              aType.get(), _spec),
             aPrincipal);
    if (!permManager->PermissionAvailable(aPrincipal, aType)) {
      LOG(
          ("Permission isn't available for this principal in the current "
           "process"));
      return false;
    }
    nsTArray<RefPtr<nsIPermission>> permissions;
    nsresult rv = permManager->GetAllForPrincipal(aPrincipal, permissions);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Failed to get the list of permissions"));
      return false;
    }

    bool found = false;
    for (const auto& permission : permissions) {
      if (!permission) {
        LOG(("Couldn't get the permission for unknown reasons"));
        continue;
      }

      nsAutoCString permissionType;
      if (NS_SUCCEEDED(permission->GetType(permissionType)) &&
          permissionType != aType) {
        LOG(("Non-matching permission type: %s", aType.get()));
        continue;
      }

      uint32_t capability = 0;
      if (NS_SUCCEEDED(permission->GetCapability(&capability)) &&
          capability != nsIPermissionManager::ALLOW_ACTION) {
        LOG(("Non-matching permission capability: %d", capability));
        continue;
      }

      uint32_t expirationType = 0;
      if (NS_SUCCEEDED(permission->GetExpireType(&expirationType)) &&
          expirationType != nsIPermissionManager ::EXPIRE_SESSION) {
        LOG(("Non-matching permission expiration type: %d", expirationType));
        continue;
      }

      int64_t expirationTime = 0;
      if (NS_SUCCEEDED(permission->GetExpireTime(&expirationTime)) &&
          expirationTime != 0) {
        LOG(("Non-matching permission expiration time: %" PRId64,
             expirationTime));
        continue;
      }

      LOG(("Found a matching permission"));
      found = true;
      break;
    }

    if (!found) {
      if (aRejectedReason) {
        *aRejectedReason = aBlockedReason;
      }
      return false;
    }
  } else {
    nsresult rv = permManager->TestPermissionWithoutDefaultsFromPrincipal(
        aPrincipal, aType, &result);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Failed to test the permission"));
      return false;
    }

    LOG_PRIN(
        ("Testing permission type %s for %s resulted in %d (%s)", aType.get(),
         _spec, int(result),
         result == nsIPermissionManager::ALLOW_ACTION ? "success" : "failure"),
        aPrincipal);

    if (result != nsIPermissionManager::ALLOW_ACTION) {
      if (aRejectedReason) {
        *aRejectedReason = aBlockedReason;
      }
      return false;
    }
  }

  return true;
}

/* static */
nsILoadInfo::StoragePermissionState
AntiTrackingUtils::GetStoragePermissionStateInParent(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;

  auto policyType = loadInfo->GetExternalContentPolicyType();

  // The channel is for the document load of the top-level window. The top-level
  // window should always has 'hasStoragePermission' flag as false. So, we can
  // return here directly.
  if (policyType == ExtContentPolicy::TYPE_DOCUMENT) {
    return nsILoadInfo::NoStoragePermission;
  }

  nsresult rv =
      loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsILoadInfo::NoStoragePermission;
  }

  int32_t cookieBehavior = cookieJarSettings->GetCookieBehavior();

  // We only need to check the storage permission if the cookie behavior is
  // BEHAVIOR_REJECT_TRACKER, BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN or
  // BEHAVIOR_REJECT_FOREIGN with exceptions. Because ContentBlocking wouldn't
  // update or check the storage permission if the cookie behavior is not
  // belongs to these three.
  if (!net::CookieJarSettings::IsRejectThirdPartyContexts(cookieBehavior)) {
    return nsILoadInfo::NoStoragePermission;
  }

  RefPtr<BrowsingContext> bc;
  rv = loadInfo->GetTargetBrowsingContext(getter_AddRefs(bc));
  if (NS_WARN_IF(NS_FAILED(rv)) || !bc) {
    return nsILoadInfo::NoStoragePermission;
  }

  uint64_t targetWindowId = GetTopLevelAntiTrackingWindowId(bc);
  nsCOMPtr<nsIPrincipal> targetPrincipal;

  if (targetWindowId) {
    RefPtr<WindowGlobalParent> wgp =
        WindowGlobalParent::GetByInnerWindowId(targetWindowId);

    if (NS_WARN_IF(!wgp)) {
      return nsILoadInfo::NoStoragePermission;
    }

    targetPrincipal = wgp->DocumentPrincipal();
  } else {
    // We try to use the loading principal if there is no AntiTrackingWindowId.
    targetPrincipal = loadInfo->GetLoadingPrincipal();
  }

  if (!targetPrincipal) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);

    if (httpChannel) {
      // We don't have a loading principal, let's see if this is a document
      // channel which belongs to a top-level window.
      bool isDocument = false;
      rv = httpChannel->GetIsMainDocumentChannel(&isDocument);
      if (NS_SUCCEEDED(rv) && isDocument) {
        nsIScriptSecurityManager* ssm =
            nsScriptSecurityManager::GetScriptSecurityManager();
        Unused << ssm->GetChannelResultPrincipal(
            aChannel, getter_AddRefs(targetPrincipal));
      }
    }
  }

  // Let's use the triggering principal if we still have nothing on the hand.
  if (!targetPrincipal) {
    targetPrincipal = loadInfo->TriggeringPrincipal();
  }

  // Cannot get the target principal, bail out.
  if (NS_WARN_IF(!targetPrincipal)) {
    return nsILoadInfo::NoStoragePermission;
  }

  nsCOMPtr<nsIURI> trackingURI;
  rv = aChannel->GetURI(getter_AddRefs(trackingURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsILoadInfo::NoStoragePermission;
  }

  nsAutoCString trackingOrigin;
  rv = nsContentUtils::GetASCIIOrigin(trackingURI, trackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nsILoadInfo::NoStoragePermission;
  }

  if (IsThirdPartyChannel(aChannel)) {
    nsAutoCString targetOrigin;
    if (NS_FAILED(targetPrincipal->GetAsciiOrigin(targetOrigin))) {
      return nsILoadInfo::NoStoragePermission;
    }

    if (PartitioningExceptionList::Check(targetOrigin, trackingOrigin)) {
      return nsILoadInfo::StoragePermissionAllowListed;
    }
  }

  nsAutoCString type;
  AntiTrackingUtils::CreateStoragePermissionKey(trackingOrigin, type);

  uint32_t unusedReason = 0;

  if (AntiTrackingUtils::CheckStoragePermission(targetPrincipal, type,
                                                NS_UsePrivateBrowsing(aChannel),
                                                &unusedReason, unusedReason)) {
    return nsILoadInfo::HasStoragePermission;
  }

  return nsILoadInfo::NoStoragePermission;
}

uint64_t AntiTrackingUtils::GetTopLevelAntiTrackingWindowId(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);

  RefPtr<WindowContext> winContext =
      aBrowsingContext->GetCurrentWindowContext();
  if (!winContext || winContext->GetCookieBehavior().isNothing()) {
    return 0;
  }

  // Do not check BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN her because when
  // a third-party subresource is inside the main frame, we need to return the
  // top-level window id to partition its cookies correctly.
  uint32_t behavior = *winContext->GetCookieBehavior();
  if (behavior == nsICookieService::BEHAVIOR_REJECT_TRACKER &&
      aBrowsingContext->IsTop()) {
    return 0;
  }

  return aBrowsingContext->Top()->GetCurrentInnerWindowId();
}

uint64_t AntiTrackingUtils::GetTopLevelStorageAreaWindowId(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);

  if (Document::StorageAccessSandboxed(aBrowsingContext->GetSandboxFlags())) {
    return 0;
  }

  BrowsingContext* parentBC = aBrowsingContext->GetParent();
  if (!parentBC) {
    // No parent browsing context available!
    return 0;
  }

  if (!parentBC->IsTop()) {
    return 0;
  }

  return parentBC->GetCurrentInnerWindowId();
}

/* static */
already_AddRefed<nsIPrincipal> AntiTrackingUtils::GetPrincipal(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);

  nsCOMPtr<nsIPrincipal> principal;

  if (XRE_IsContentProcess()) {
    // Passing an out-of-process browsing context in child processes to
    // this API won't get any result, so just assert.
    MOZ_ASSERT(aBrowsingContext->IsInProcess());

    nsPIDOMWindowOuter* outer = aBrowsingContext->GetDOMWindow();
    if (NS_WARN_IF(!outer)) {
      return nullptr;
    }

    nsPIDOMWindowInner* inner = outer->GetCurrentInnerWindow();
    if (NS_WARN_IF(!inner)) {
      return nullptr;
    }

    principal = nsGlobalWindowInner::Cast(inner)->GetPrincipal();
  } else {
    WindowGlobalParent* wgp =
        aBrowsingContext->Canonical()->GetCurrentWindowGlobal();
    if (NS_WARN_IF(!wgp)) {
      return nullptr;
    }

    principal = wgp->DocumentPrincipal();
  }
  return principal.forget();
}

/* static */
bool AntiTrackingUtils::GetPrincipalAndTrackingOrigin(
    BrowsingContext* aBrowsingContext, nsIPrincipal** aPrincipal,
    nsACString& aTrackingOrigin) {
  MOZ_ASSERT(aBrowsingContext);

  // Passing an out-of-process browsing context in child processes to
  // this API won't get any result, so just assert.
  MOZ_ASSERT_IF(XRE_IsContentProcess(), aBrowsingContext->IsInProcess());

  // Let's take the principal and the origin of the tracker.
  nsCOMPtr<nsIPrincipal> principal =
      AntiTrackingUtils::GetPrincipal(aBrowsingContext);
  if (NS_WARN_IF(!principal)) {
    return false;
  }

  nsresult rv = principal->GetOriginNoSuffix(aTrackingOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (aPrincipal) {
    principal.forget(aPrincipal);
  }

  return true;
};

/* static */
uint32_t AntiTrackingUtils::GetCookieBehavior(
    BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);

  RefPtr<dom::WindowContext> win = aBrowsingContext->GetCurrentWindowContext();
  if (!win || win->GetCookieBehavior().isNothing()) {
    return nsICookieService::BEHAVIOR_REJECT;
  }

  return *win->GetCookieBehavior();
}

/* static */
already_AddRefed<WindowGlobalParent>
AntiTrackingUtils::GetTopWindowExcludingExtensionAccessibleContentFrames(
    CanonicalBrowsingContext* aBrowsingContext, nsIURI* aURIBeingLoaded) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aBrowsingContext);

  CanonicalBrowsingContext* bc = aBrowsingContext;
  RefPtr<WindowGlobalParent> prev;
  while (RefPtr<WindowGlobalParent> parent = bc->GetParentWindowContext()) {
    CanonicalBrowsingContext* parentBC = parent->BrowsingContext();

    nsIPrincipal* parentPrincipal = parent->DocumentPrincipal();
    nsIURI* uri = prev ? prev->GetDocumentURI() : aURIBeingLoaded;

    // If the new parent has permission to load the current page, we're
    // at a moz-extension:// frame which has a host permission that allows
    // it to load the document that we've loaded.  In that case, stop at
    // this frame and consider it the top-level frame.
    if (uri &&
        BasePrincipal::Cast(parentPrincipal)->AddonAllowsLoad(uri, true)) {
      break;
    }

    bc = parentBC;
    prev = parent;
  }
  if (!prev) {
    prev = bc->GetCurrentWindowGlobal();
  }
  return prev.forget();
}

/* static */
void AntiTrackingUtils::ComputeIsThirdPartyToTopWindow(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  // When a top-level load is opened by window.open, BrowsingContext from
  // LoadInfo is its opener, which may make the third-party caculation code
  // below returns an incorrect result. So we use TYPE_DOCUMENT to
  // ensure a top-level load is not considered 3rd-party.
  auto policyType = loadInfo->GetExternalContentPolicyType();
  if (policyType == ExtContentPolicy::TYPE_DOCUMENT) {
    loadInfo->SetIsThirdPartyContextToTopWindow(false);
    return;
  }

  RefPtr<BrowsingContext> bc;
  loadInfo->GetBrowsingContext(getter_AddRefs(bc));

  nsCOMPtr<nsIURI> uri;
  Unused << aChannel->GetURI(getter_AddRefs(uri));

  // In some cases we don't have a browsingContext. For example, in xpcshell
  // tests, channels that are used to download images and channels for loading
  // worker script.
  if (!bc) {
    // If the flag was set before, we don't need to compute again. This could
    // happen for the channels used to load worker scripts.
    //
    // Note that we cannot stop computing the flag in general even it has set
    // before because sometimes we need to get the up-to-date flag, e.g.
    // redirects.
    if (static_cast<net::LoadInfo*>(loadInfo.get())
            ->HasIsThirdPartyContextToTopWindowSet()) {
      return;
    }

    // We turn to check the loading principal if there is no browsing context.
    auto* loadingPrincipal =
        BasePrincipal::Cast(loadInfo->GetLoadingPrincipal());

    if (uri && loadingPrincipal) {
      bool isThirdParty = true;
      nsresult rv = loadingPrincipal->IsThirdPartyURI(uri, &isThirdParty);

      if (NS_SUCCEEDED(rv)) {
        loadInfo->SetIsThirdPartyContextToTopWindow(isThirdParty);
      }
    }
    return;
  }

  RefPtr<WindowGlobalParent> topWindow =
      GetTopWindowExcludingExtensionAccessibleContentFrames(bc->Canonical(),
                                                            uri);

  if (NS_WARN_IF(!topWindow)) {
    return;
  }

  nsCOMPtr<nsIPrincipal> topWindowPrincipal = topWindow->DocumentPrincipal();
  if (topWindowPrincipal && !topWindowPrincipal->GetIsNullPrincipal()) {
    auto* basePrin = BasePrincipal::Cast(topWindowPrincipal);
    bool isThirdParty = true;

    // For about:blank and about:srcdoc, we can't just compare uri to determine
    // whether the page is third-party, so we use channel result principal
    // instead. By doing this, an the resource inherits the principal from
    // its parent is considered not a third-party.
    if (NS_IsAboutBlank(uri) || NS_IsAboutSrcdoc(uri)) {
      nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
      if (NS_WARN_IF(!ssm)) {
        return;
      }

      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv =
          ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(principal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      basePrin->IsThirdPartyPrincipal(principal, &isThirdParty);
    } else {
      basePrin->IsThirdPartyURI(uri, &isThirdParty);
    }

    loadInfo->SetIsThirdPartyContextToTopWindow(isThirdParty);
  }
}

/* static */
bool AntiTrackingUtils::IsThirdPartyChannel(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  // We only care whether the channel is 3rd-party with respect to
  // the top-level.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  return loadInfo->GetIsThirdPartyContextToTopWindow();
}

/* static */
bool AntiTrackingUtils::IsThirdPartyWindow(nsPIDOMWindowInner* aWindow,
                                           nsIURI* aURI) {
  MOZ_ASSERT(aWindow);

  // We assume that the window is foreign to the URI by default.
  bool thirdParty = true;

  // We will skip checking URIs for about:blank and about:srcdoc because they
  // have no domain. So, comparing them will always fail.
  if (aURI && !NS_IsAboutBlank(aURI) && !NS_IsAboutSrcdoc(aURI)) {
    nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin =
        do_QueryInterface(aWindow);
    if (!scriptObjPrin) {
      return thirdParty;
    }

    nsCOMPtr<nsIPrincipal> prin = scriptObjPrin->GetPrincipal();
    if (!prin) {
      return thirdParty;
    }

    // Determine whether aURI is foreign with respect to the current principal.
    nsresult rv = prin->IsThirdPartyURI(aURI, &thirdParty);
    if (NS_FAILED(rv)) {
      return thirdParty;
    }

    if (thirdParty) {
      return thirdParty;
    }
  }

  RefPtr<Document> doc = aWindow->GetDoc();
  if (!doc) {
    // If we can't get the document from the window, ex, about:blank, fallback
    // to use IsThirdPartyWindow check that examine the whole hierarchy.
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        components::ThirdPartyUtil::Service();
    Unused << thirdPartyUtil->IsThirdPartyWindow(aWindow->GetOuterWindow(),
                                                 nullptr, &thirdParty);
    return thirdParty;
  }

  return IsThirdPartyDocument(doc);
}

/* static */
bool AntiTrackingUtils::IsThirdPartyDocument(Document* aDocument) {
  MOZ_ASSERT(aDocument);
  if (!aDocument->GetChannel()) {
    // If we can't get the channel from the document, i.e. initial about:blank
    // page, we use the browsingContext of the document to check if it's in the
    // third-party context. If the browsing context is still not available, we
    // will treat the window as third-party.
    RefPtr<BrowsingContext> bc = aDocument->GetBrowsingContext();
    return bc ? IsThirdPartyContext(bc) : true;
  }

  // We only care whether the channel is 3rd-party with respect to
  // the top-level.
  nsCOMPtr<nsILoadInfo> loadInfo = aDocument->GetChannel()->LoadInfo();
  return loadInfo->GetIsThirdPartyContextToTopWindow();
}

/* static */
bool AntiTrackingUtils::IsThirdPartyContext(BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(aBrowsingContext);
  MOZ_ASSERT(aBrowsingContext->IsInProcess());

  if (aBrowsingContext->IsTopContent()) {
    return false;
  }

  // If the top browsing context is not in the same process, it's cross-origin.
  if (!aBrowsingContext->Top()->IsInProcess()) {
    return true;
  }

  nsIDocShell* docShell = aBrowsingContext->GetDocShell();
  if (!docShell) {
    return true;
  }
  Document* doc = docShell->GetExtantDocument();
  if (!doc) {
    return true;
  }
  nsIPrincipal* principal = doc->NodePrincipal();

  nsIDocShell* topDocShell = aBrowsingContext->Top()->GetDocShell();
  if (!topDocShell) {
    return true;
  }
  Document* topDoc = topDocShell->GetDocument();
  if (!topDoc) {
    return true;
  }
  nsIPrincipal* topPrincipal = topDoc->NodePrincipal();

  auto* topBasePrin = BasePrincipal::Cast(topPrincipal);
  bool isThirdParty = true;

  topBasePrin->IsThirdPartyPrincipal(principal, &isThirdParty);

  return isThirdParty;
}

/* static */
nsCString AntiTrackingUtils::GrantedReasonToString(
    ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason) {
  switch (aReason) {
    case ContentBlockingNotifier::eOpener:
      return "opener"_ns;
    case ContentBlockingNotifier::eOpenerAfterUserInteraction:
      return "user interaction"_ns;
    default:
      return "stroage access API"_ns;
  }
}

/* static */
void AntiTrackingUtils::UpdateAntiTrackingInfoForChannel(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  if (!XRE_IsParentProcess()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  AntiTrackingUtils::ComputeIsThirdPartyToTopWindow(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  Unused << loadInfo->SetStoragePermission(
      AntiTrackingUtils::GetStoragePermissionStateInParent(aChannel));

  // We only update the IsOnContentBlockingAllowList flag and the partition key
  // for the top-level http channel.
  //
  // The IsOnContentBlockingAllowList is only for http. For other types of
  // channels, such as 'file:', there will be no interface to modify this. So,
  // we only update it in http channels.
  //
  // The partition key is computed based on the site, so it's no point to set it
  // for channels other than http channels.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel || loadInfo->GetExternalContentPolicyType() !=
                          ExtContentPolicy::TYPE_DOCUMENT) {
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  Unused << loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));

  // Update the IsOnContentBlockingAllowList flag in the CookieJarSettings
  // if this is a top level loading. For sub-document loading, this flag
  // would inherit from the parent.
  net::CookieJarSettings::Cast(cookieJarSettings)
      ->UpdateIsOnContentBlockingAllowList(aChannel);

  // We only need to set FPD for top-level loads. FPD will automatically be
  // propagated to non-top level loads via CookieJarSetting.
  nsCOMPtr<nsIURI> uri;
  Unused << aChannel->GetURI(getter_AddRefs(uri));
  net::CookieJarSettings::Cast(cookieJarSettings)->SetPartitionKey(uri);
}
