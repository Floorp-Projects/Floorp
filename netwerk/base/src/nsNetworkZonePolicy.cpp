/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"
#include "nsAutoPtr.h"
#include "nsNetworkZonePolicy.h"
#include "nsIChannel.h"
#include "nsIDocShell.h"
#include "nsIDocumentLoader.h"
#include "nsILoadGroup.h"
#include "nsILoadGroupChild.h"
#include "nsIObserverService.h"
#include "nsIRequestObserver.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#ifdef PR_LOGGING
#include "nsString.h"
#endif

namespace mozilla
{
namespace net
{

#ifdef PR_LOGGING
static PRLogModuleInfo *gNZPLog;
#define NZPLOG(msg, ...) \
  PR_LOG(gNZPLog, PR_LOG_DEBUG, ("[NZP] %p: " msg, this, ##__VA_ARGS__))
#else
#define NZPLOG(msg, ...)
#endif

/* Keeps track of whether or not NZP is enabled */
bool nsNetworkZonePolicy::sNZPEnabled = true;

/* True if shutdown notification has been received. */
bool nsNetworkZonePolicy::sShutdown = false;

/* Singleton pointer. */
StaticRefPtr<nsNetworkZonePolicy> nsNetworkZonePolicy::sSingleton;

nsNetworkZonePolicy::nsNetworkZonePolicy()
{
  Preferences::AddBoolVarCache(&sNZPEnabled, "network.zonepolicy.enabled");

  // Register for shutdown notification.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  } else {
    NS_WARNING("failed to get observer service");
  }

#ifdef PR_LOGGING
  if (!gNZPLog) {
    gNZPLog = PR_NewLogModule("NZP");
  }
#endif
}

already_AddRefed<nsNetworkZonePolicy>
nsNetworkZonePolicy::GetSingleton()
{
  if (sShutdown) {
    return nullptr;
  }

  if (!sSingleton) {
    sSingleton = new nsNetworkZonePolicy();
  }

  // Return a ref ptr to the singleton.
  nsRefPtr<nsNetworkZonePolicy> nzp = sSingleton.get();
  return nzp.forget();
}

nsNetworkZonePolicy::~nsNetworkZonePolicy() {}

NS_IMPL_ISUPPORTS(nsNetworkZonePolicy,
                  nsINetworkZonePolicy,
                  nsIObserver)

// nsIObserver interface
NS_IMETHODIMP
nsNetworkZonePolicy::Observe(nsISupports *aSubject,
                             const char *aTopic,
                             const char16_t *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    sShutdown = true;
    sSingleton = nullptr;
  }
  return NS_OK;
}

already_AddRefed<nsILoadGroup>
nsNetworkZonePolicy::GetLoadGroupParent(nsILoadGroup *aLoadGroup)
{
  if (NS_WARN_IF(!aLoadGroup)) {
    return nullptr;
  }
  MOZ_ASSERT(aLoadGroup);

  DebugOnly<nsresult> rv = NS_OK;
  nsCOMPtr<nsILoadGroup> parent;
  nsCOMPtr<nsILoadGroupChild> loadGroupAsChild = do_QueryInterface(aLoadGroup);
  if (!loadGroupAsChild) {
    return nullptr;
  }
  rv = loadGroupAsChild->GetParentLoadGroup(getter_AddRefs(parent));
  if (!parent) {
    return nullptr;
  }
  NZPLOG("loadgroup %p getting parent loadgroup %p", aLoadGroup,
         parent.get());
  return parent.forget();
}

already_AddRefed<nsILoadGroup>
nsNetworkZonePolicy::GetOwningLoadGroup(nsILoadGroup *aLoadGroup)
{
  if (NS_WARN_IF(!aLoadGroup)) {
    return nullptr;
  }
  MOZ_ASSERT(aLoadGroup);

  DebugOnly<nsresult> rv = NS_OK;
  nsCOMPtr<nsILoadGroup> owner;
  rv = aLoadGroup->GetLoadGroup(getter_AddRefs(owner));
  if (!owner) {
    return nullptr;
  }
  NZPLOG("loadgroup %p getting owning loadgroup %p", aLoadGroup, owner.get());
  return owner.forget();
}

already_AddRefed<nsILoadGroup>
nsNetworkZonePolicy::GetParentDocShellsLoadGroup(nsILoadGroup *aLoadGroup)
{
  if (NS_WARN_IF(!aLoadGroup)) {
    return nullptr;
  }
  MOZ_ASSERT(aLoadGroup);

  DebugOnly<nsresult> rv = NS_OK;
  nsCOMPtr<nsIRequestObserver> observer;
  rv = aLoadGroup->GetGroupObserver(getter_AddRefs(observer));
  if (!observer) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(observer);
  if (!docShell) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem;
  docShell->GetSameTypeParent(getter_AddRefs(parentAsTreeItem));
  if (!parentAsTreeItem) {
    return nullptr;
  }
  nsCOMPtr<nsIDocumentLoader> parentAsDocLoader =
    do_QueryInterface(parentAsTreeItem);
  if (!parentAsDocLoader) {
    return nullptr;
  }
  nsCOMPtr<nsILoadGroup> dsParent;
  rv = parentAsDocLoader->GetLoadGroup(getter_AddRefs(dsParent));
  if (!dsParent) {
    return nullptr;
  }
  NZPLOG("loadgroup %p getting docshell parent's loadgroup %p",
         aLoadGroup, dsParent.get());
  return dsParent.forget();
}

bool
nsNetworkZonePolicy::CheckLoadGroupAncestorHierarchies(nsILoadGroup *aLoadGroup)
{
  if (NS_WARN_IF(!aLoadGroup)) {
    return false;
  }
  MOZ_ASSERT(aLoadGroup);

  // Check the hierarchies of appropriate ancestors.
  // 1. Parent loadgroup.
  nsCOMPtr<nsILoadGroup> ancestor = GetLoadGroupParent(aLoadGroup);
  if (ancestor) {
    bool ancestorAllows = CheckLoadGroupHierarchy(ancestor);

    NZPLOG("Loadgroup %p's parent loadgroup hierarchy %s private loads.",
           aLoadGroup, ancestorAllows ? "allows" : "forbids");
    if (!ancestorAllows) {
      return false;
    }
  }

  // 2. Owning loadgroup.
  ancestor = GetOwningLoadGroup(aLoadGroup);
  if (ancestor) {
    bool ancestorAllows = CheckLoadGroupHierarchy(ancestor);

    NZPLOG("Loadgroup %p's owning loadgroup hierarchy %s private loads.",
           aLoadGroup, ancestorAllows ? "allows" : "forbids");
    if (!ancestorAllows) {
      return false;
    }
  }

  // 3. Parent docshell's loadgroup.
  ancestor = GetParentDocShellsLoadGroup(aLoadGroup);
  if (ancestor) {
    bool ancestorAllows = CheckLoadGroupHierarchy(ancestor);

    NZPLOG("Loadgroup %p's parent docshell's loadgroup hierarchy %s private "
           "loads.",
           aLoadGroup, ancestorAllows ? "allows" : "forbids");
    if (!ancestorAllows) {
      return false;
    }
  }

  // If there is no ancestor or they all have permission to load from private
  // networks, return true.
  NZPLOG("Loadgroup %p: no ancestor forbids loads from private networks.",
         aLoadGroup);
  return true;
}

bool
nsNetworkZonePolicy::CheckLoadGroupHierarchy(nsILoadGroup *aLoadGroup)
{
  if (NS_WARN_IF(!aLoadGroup)) {
    return false;
  }
  MOZ_ASSERT(aLoadGroup);

  // Recurse until root load group of same type, or until ancestor forbids
  // private loads.

  // If current loadgroup does not allow private loads, just return.
  bool allowed = false;
  aLoadGroup->GetAllowLoadsFromPrivateNetworks(&allowed);
  if (!allowed) {
    NZPLOG("Loadgroup %p forbids loads from private networks.", aLoadGroup);
    return false;
  }

  // Else, check the hierarchies of appropriate ancestors.
  return CheckLoadGroupAncestorHierarchies(aLoadGroup);
}

/*
 * nsNetworkZonePolicy : nsINetworkZonePolicy
 */
NS_IMETHODIMP
nsNetworkZonePolicy::CheckPrivateNetworkPermission(nsIRequest *aRequest,
                                                   bool *aAllowed)
{
  if (NS_WARN_IF(!aRequest)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (NS_WARN_IF(!aAllowed)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!aRequest || !aAllowed)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!sNZPEnabled) {
    *aAllowed = true;
    return NS_OK;
  }

#ifdef PR_LOGGING
  nsAutoCString nameStr;
  aRequest->GetName(nameStr);
#endif
  NZPLOG("CheckPrivateNetworkPermission for request %p [%s].", aRequest,
         nameStr.get());

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = aRequest->GetLoadGroup(getter_AddRefs(loadGroup));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!loadGroup) {
    NZPLOG("No loadgroup for request %p [%s]; private networks allowed.",
           aRequest, nameStr.get());
    *aAllowed = true;
    return NS_OK;
  }

  // Find out if this loadgroup's hierarchy allows private loads.
  bool hierarchyAllows = CheckLoadGroupHierarchy(loadGroup);

  NZPLOG("LoadGroup %p for request %p [%s] is %s private loads.",
         loadGroup.get(), aRequest, nameStr.get(),
         hierarchyAllows ? "allowed" : "forbidden");

  *aAllowed = hierarchyAllows;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkZonePolicy::SetPrivateNetworkPermission(nsIRequest *aRequest,
                                                 bool aAllowed)
{
  if (NS_WARN_IF(!aRequest)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!sNZPEnabled) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = aRequest->GetLoadGroup(getter_AddRefs(loadGroup));
  if (!loadGroup || NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef PR_LOGGING
  nsAutoCString nameStr;
  aRequest->GetName(nameStr);
#endif
  NZPLOG("SetPrivateNetworkPermission: try to %s for loadgroup %p of request "
         "%p [%s].",
         aAllowed ? "allow" : "forbid", loadGroup.get(), aRequest,
         nameStr.get());

  // Only allow a document request to set its loadgroup's permissions.
  nsLoadFlags flags;
  aRequest->GetLoadFlags(&flags);

  if (!(flags & nsIChannel::LOAD_DOCUMENT_URI)) {
    NZPLOG("Skipping request %p [%s] - not a document load", aRequest,
           nameStr.get());
    return NS_OK;
  }

  // Do NOT allow a document load to override the loadgroup's hierarchy.
  bool ancestorsAllow = CheckLoadGroupAncestorHierarchies(loadGroup);

  NZPLOG("LoadGroup %p ancestors for request %p [%s] %s private loads.",
         loadGroup.get(), aRequest, nameStr.get(),
         ancestorsAllow ? "allows" : "forbids");

  if (!ancestorsAllow) {
    NZPLOG("Request %p [%s] can't override hierarchy.", aRequest,
           nameStr.get());
    return NS_OK;
  }

  return loadGroup->SetAllowLoadsFromPrivateNetworks(aAllowed);
}

} // namespace net
} // namespace mozilla
