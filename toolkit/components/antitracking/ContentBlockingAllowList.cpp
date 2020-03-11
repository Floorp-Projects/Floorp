/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "ContentBlockingAllowList.h"
#include "ContentBlockingAllowListCache.h"
#include "SettingsChangeObserver.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ScopeExit.h"
#include "nsContentUtils.h"
#include "nsIHttpChannelInternal.h"
#include "nsPermissionManager.h"

using namespace mozilla;

/* static */ ContentBlockingAllowListCache& ContentBlockingAllowList::Cache() {
  static bool initialized = false;
  static ContentBlockingAllowListCache cache;
  if (!initialized) {
    SettingsChangeObserver::OnAntiTrackingSettingsChanged([&] {
      // Drop everything in the cache, since the result of content blocking
      // allow list checks may change past this point.
      cache.Clear();
    });
    initialized = true;
  }
  return cache;
}

/* static */ bool ContentBlockingAllowList::Check(
    nsIPrincipal* aTopWinPrincipal, bool aIsPrivateBrowsing) {
  bool isAllowed = false;
  nsresult rv = Check(aTopWinPrincipal, aIsPrivateBrowsing, isAllowed);
  if (NS_SUCCEEDED(rv) && isAllowed) {
    LOG(
        ("The top-level window is on the content blocking allow list, "
         "bail out early"));
    return true;
  }
  if (NS_FAILED(rv)) {
    LOG(("Checking the content blocking allow list for failed with %" PRIx32,
         static_cast<uint32_t>(rv)));
  }
  return false;
}

/* static */ bool ContentBlockingAllowList::Check(nsPIDOMWindowInner* aWindow) {
  ContentBlockingAllowListKey cacheKey(aWindow);
  auto entry = Cache().Lookup(cacheKey);
  if (entry) {
    // We've recently performed a content blocking allow list check for this
    // window, so let's quickly return the answer instead of continuing with the
    // rest of this potentially expensive computation.
    return entry.Data().mResult;
  }

  nsPIDOMWindowOuter* top =
      aWindow->GetBrowsingContext()->Top()->GetDOMWindow();
  dom::Document* doc = top ? top->GetExtantDoc() : nullptr;
  if (doc) {
    bool isPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(doc);

    const bool result = ContentBlockingAllowList::Check(
        doc->GetContentBlockingAllowListPrincipal(), isPrivateBrowsing);

    entry.Set(ContentBlockingAllowListEntry(aWindow, result));

    return result;
  }

  LOG(
      ("Could not check the content blocking allow list because the top "
       "window wasn't accessible"));
  entry.Set(ContentBlockingAllowListEntry(aWindow, false));
  return false;
}

/* static */ bool ContentBlockingAllowList::Check(nsIHttpChannel* aChannel) {
  ContentBlockingAllowListKey cacheKey(aChannel);
  auto entry = Cache().Lookup(cacheKey);
  if (entry) {
    // We've recently performed a content blocking allow list check for this
    // channel, so let's quickly return the answer instead of continuing with
    // the rest of this potentially expensive computation.
    return entry.Data().mResult;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIHttpChannelInternal> httpChan = do_QueryInterface(aChannel);
  if (httpChan) {
    nsresult rv = httpChan->GetContentBlockingAllowListPrincipal(
        getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal) {
      LOG(
          ("Could not check the content blocking allow list because the top "
           "window wasn't accessible"));
      entry.Set(ContentBlockingAllowListEntry(aChannel, false));
      return false;
    }
  }

  const bool result = ContentBlockingAllowList::Check(
      principal, NS_UsePrivateBrowsing(aChannel));
  entry.Set(ContentBlockingAllowListEntry(aChannel, result));
  return result;
}

nsresult ContentBlockingAllowList::Check(
    nsIPrincipal* aContentBlockingAllowListPrincipal, bool aIsPrivateBrowsing,
    bool& aIsAllowListed) {
  aIsAllowListed = false;

  if (!aContentBlockingAllowListPrincipal) {
    // Nothing to do!
    return NS_OK;
  }

  LOG_PRIN(("Deciding whether the user has overridden content blocking for %s",
            _spec),
           aContentBlockingAllowListPrincipal);

  nsPermissionManager* permManager = nsPermissionManager::GetInstance();
  NS_ENSURE_TRUE(permManager, NS_ERROR_FAILURE);

  // Check both the normal mode and private browsing mode user override
  // permissions.
  Pair<const nsLiteralCString, bool> types[] = {
      {NS_LITERAL_CSTRING("trackingprotection"), false},
      {NS_LITERAL_CSTRING("trackingprotection-pb"), true}};

  for (const auto& type : types) {
    if (aIsPrivateBrowsing != type.second()) {
      continue;
    }

    uint32_t permissions = nsIPermissionManager::UNKNOWN_ACTION;
    nsresult rv = permManager->TestPermissionFromPrincipal(
        aContentBlockingAllowListPrincipal, type.first(), &permissions);
    NS_ENSURE_SUCCESS(rv, rv);

    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
      aIsAllowListed = true;
      LOG(("Found user override type %s", type.first().get()));
      // Stop checking the next permisson type if we decided to override.
      break;
    }
  }

  if (!aIsAllowListed) {
    LOG(("No user override found"));
  }

  return NS_OK;
}

/* static */ void ContentBlockingAllowList::ComputePrincipal(
    nsIPrincipal* aDocumentPrincipal, nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  auto returnInputArgument =
      MakeScopeExit([&] { NS_IF_ADDREF(*aPrincipal = aDocumentPrincipal); });

  BasePrincipal* bp = BasePrincipal::Cast(aDocumentPrincipal);
  if (!bp || !bp->IsContentPrincipal()) {
    // If we have something other than a content principal, just return what we
    // have.  This includes the case where we were passed a nullptr.
    return;
  }

  // Take the host/port portion so we can allowlist by site. Also ignore the
  // scheme, since users who put sites on the allowlist probably don't expect
  // allowlisting to depend on scheme.
  nsAutoCString escaped(NS_LITERAL_CSTRING("https://"));
  nsAutoCString temp;
  nsresult rv = aDocumentPrincipal->GetHostPort(temp);
  // view-source URIs will be handled by the next block.
  if (NS_FAILED(rv) && !aDocumentPrincipal->SchemeIs("view-source")) {
    // Normal for some loads, no need to print a warning
    return;
  }

  // GetHostPort returns an empty string (with a success error code) for file://
  // URIs.
  if (temp.IsEmpty()) {
    // In this case we want to make sure that our allow list principal would be
    // computed as null.
    returnInputArgument.release();
    *aPrincipal = nullptr;
    return;
  }
  escaped.Append(temp);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), escaped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = BasePrincipal::CreateContentPrincipal(
      uri, aDocumentPrincipal->OriginAttributesRef());
  if (NS_WARN_IF(!principal)) {
    return;
  }

  returnInputArgument.release();
  principal.forget(aPrincipal);
}

/* static */ void ContentBlockingAllowList::RecomputePrincipal(
    nsIURI* aURIBeingLoaded, const OriginAttributes& aAttrs,
    nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  auto returnInputArgument = MakeScopeExit([&] { *aPrincipal = nullptr; });

  // Take the host/port portion so we can allowlist by site. Also ignore the
  // scheme, since users who put sites on the allowlist probably don't expect
  // allowlisting to depend on scheme.
  nsAutoCString escaped(NS_LITERAL_CSTRING("https://"));
  nsAutoCString temp;
  nsresult rv = aURIBeingLoaded->GetHostPort(temp);
  // view-source URIs will be handled by the next block.
  if (NS_FAILED(rv) && !aURIBeingLoaded->SchemeIs("view-source")) {
    // Normal for some loads, no need to print a warning
    return;
  }

  // GetHostPort returns an empty string (with a success error code) for file://
  // URIs.
  if (temp.IsEmpty()) {
    return;
  }
  escaped.Append(temp);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), escaped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(uri, aAttrs);
  if (NS_WARN_IF(!principal)) {
    return;
  }

  returnInputArgument.release();
  principal.forget(aPrincipal);
}
