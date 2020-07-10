/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StoragePrincipalHelper_h
#define mozilla_StoragePrincipalHelper_h

#include "nsError.h"

/**
 * StoragePrincipal
 * ~~~~~~~~~~~~~~~~

 * StoragePrincipal is the nsIPrincipal to be used to open the cookie jar of a
 * resource's origin. Normally, the StoragePrincipal corresponds to the
 * resource's origin, but, in some scenarios, it can be different: it has the
 * `firstPartyDomain` attribute set to the top-level “site” (i.e., scheme plus
 * eTLD+1 of the origin of the top-level document).
 *
 * Each storage component should always use the StoragePrincipal instead of the
 * 'real' one in order to implement the partitioning correctly. See the list of
 * the components here: https://privacycg.github.io/storage-partitioning/
 *
 * On the web, each resource has its own origin (see
 * https://html.spec.whatwg.org/multipage/origin.html#concept-origin) and each
 * origin has its own cookie jar, containing cookies, storage data, cache and so
 * on.
 *
 * In gecko-world, the origin and its attributes are stored and managed by the
 * nsIPrincipal interface. Both a resource's Principal and a resource's
 * StoragePrincipal are nsIPrincipal interfaces and, normally, they are the same
 * object.
 *
 * Naming and usage
 * ~~~~~~~~~~~~~~~~
 *
 * StoragePrincipal exposes three types of principals for a resource:
 * - Regular Principal:
 *     A “first-party” principal derived from the origin of the resource. This
 *     does not have the `first-party-domain` origin attribute set.
 * - Partitioned Principal:
 *     The regular principal plus the first-party-domain origin attribute set to
 *     the site of the top-level document (i.e., scheme plus eTLD+1).
 * - Storage Access Principal:
 *     A dynamic principal that changes when a resource receives storage access.
 *     By default, when storage access is denied, this is equal to the
 *     Partitioned Principal. When storage access is granted, this is equal to
 *     the Regular Principal.
 *
 * Consumers of StoragePrincipal can request the principal type that meets their
 * needs. For example, storage that should always be partitioned should choose
 * the Partitioned Principal, while storage that should change with storage
 * access grants should choose the Storage Access Principal.
 *
 * You can obtain these nsIPrincipal objects:
 *
 * From a Document:
 * - Regular Principal: nsINode::NodePrincipal
 * - Storage Access Principal: Document::EffectiveStoragePrincipal
 * - Partitioned Principal: Document::PartitionedPrincipal
 *
 * From a Global object:
 * - Regular Principal: nsIScriptObjectPrincipal::GetPrincipal
 * - Storage Access Principal:
 *     nsIScriptObjectPrincipal::GetEffectiveStoragePrincipal
 * - Partitioned Principal: nsIScriptObjectPrincipal::PartitionedPrincipal
 *
 * From a Worker:
 * - Regular Principal: WorkerPrivate::GetPrincipal (main-thread)
 * - Regular Principal: WorkerPrivate::GetPrincipalInfo (worker thread)
 * - Storage Access Principal: WorkerPrivate::GetEffectiveStoragePrincipalInfo
 *                  (worker-thread)
 *
 * For a nsIChannel, the final principals must be calculated and they can be
 * obtained by calling:
 * - Regular Principal: nsIScriptSecurityManager::getChannelResultPrincipal
 * - Storage Access Principal:
 *     nsIScriptSecurityManager::getChannelResultStoragePrincipal
 * - Partitioned and regular Principal:
 *     nsIScriptSecurityManager::getChannelResultPrincipals
 *
 * Each use of nsIPrincipal is unique and it should be reviewed by anti-tracking
 * peers. But we can group the use of nsIPrincipal in these categories:
 *
 * - Network loading: use the Regular Principal
 * - Cache, not directly visible by content (network cache, HSTS, image cache,
 *   etc): Use the Storage Access Principal (in the future we will use the
 *   Partitioned Principal, but this part is not done yet)
 * - Storage APIs or anything that is written on disk (or kept in memory in
 *   private-browsing): use the Storage Access Principal
 * - PostMessage: if in the agent-cluster, use the Regular Principal. Otherwise,
 *   use the Storage Access Principal
 *
 * Storage access permission
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * When the storage access permission is granted, any of the Storage Access
 * Principal getter methods will return the Regular Principal instead of the
 * Partitioned Principal, and each storage component should consider the new
 * principal only.
 *
 * The trackers and the 3rd parties (in dFPI) will have access to its
 first-party
 * cookie jar, escaping from its partitioning.
 *
 * Storage access permissions can be granted in several ways:
 * - The Storage Access API
 *     (https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API)
 * - ETP’s heuristics
 *
 (https://developer.mozilla.org/en-US/docs/Mozilla/Firefox/Privacy/Storage_access_policy#Storage_access_grants)
 * - A dFPI-specific login heuristic
 *     (https://bugzilla.mozilla.org/show_bug.cgi?id=1616585#c12)
 *
 * There are several ways to receive storage-permission notifications. You can
 * use these notifications to re-initialize components, to nullify or enable
 them
 * to use the “new” effective StoragePrincipal. The list of the notifications
 is:
 *
 * - Add some code in nsGlobalWindowInner::StorageAccessPermissionGranted().
 * - WorkerScope::StorageAccessPermissionGranted for Workers.
 * - observe the permission changes (not recommended)
 *
 * Scope of Storage Access
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Immediately after access is granted, the permission is propagated and
 notified
 * to any contexts (windows and workers) in the same agent-cluster
 * (BrowserContextGroup).
 *
 * This means that if A.com has 2 iframes with B.com, and one of the 2 Bs
 obtains
 * the storage access, the other B will be notified too. Other B.com, 3rd
 parties
 * in other agent clusters will not obtain the storage permission.
 *
 * When the page is reloaded or is loaded for the first time, if it contains
 * B.com, and B.com has received the storage permission for the same first-party
 * in a previous loading, B.com will have the storage access permission granted
 * immediately.
 *
 * Cookies, LocalStorage, indexedDB
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * When granting storage permission, several storage and channel API getters and
 * constructors will start exposing first-party cookie jar objects
 (localStorage,
 * BroadcastChannel, etc).
 *
 * There is a side effect of this change: If a tracker has a reference to these
 * objects pre-storage permission granting, it will be able to interact with the
 * partitioned and the non-partitioned cookie jar at the same time. Note that
 * similar synchronization can be done server-side too. Because of this, we
 don’t
 * think that privacy-wise, this is an issue.
 *
 * localStorage supports StoragePrincipal, and will be switched after storage
 * access is granted. Trackers listed in the pref
 * privacy.restrict3rdpartystorage.partitionedHosts will use another special
 * partitioned session-only storage called PartitionedLocalStorage.
 *
 * sessionStorage is not covered by StoragePrincipal, but is double-keyed using
 * the top-level site when dFPI is active
 * (https://bugzilla.mozilla.org/show_bug.cgi?id=1629707).
 *
 * SharedWorkers and BroadcastChannels
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * SharedWorker and BroadcastChannel instances latch the effective storage
 * principal at the moment of their creation. Existing bindings to the
 * partitioned storage principal will continue to exist and operate even as it
 * becomes possible to create bindings associated with the Regular Principal.
 * This makes it possible for such globals to bi-directionally bridge
 information
 * between partitioned and non-partitioned principals.
 *
 * This is true until the page is reloaded. After the reload, the partitioned
 * cookie jar will no longer be accessible.
 *
 * We are planning to clear the partitioned site-data as soon as the page is
 * reloaded or dismissed (not done yet - bug 1628313).
 *
 * {Dedicated,Shared,Service}Workers
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The storage access permission propagation happens with a ControlRunnable.
 This
 * could impact the use of sync event-loops. Take a reference of the principal
 * you want to use because it can change!
 *
 * ServiceWorkers are currently disabled for partitioned contexts.
 *
 * Client API uses the regular nsIPrincipal always because there is not a direct
 * connection between this API and the cookie jar. If we want to support
 * ServiceWorkers in partitioned context, this part must be revisited.
 */

class nsIChannel;
class nsILoadGroup;
class nsIPrincipal;

namespace mozilla {

namespace dom {
class Document;
}

namespace ipc {
class PrincipalInfo;
}

class OriginAttributes;

class StoragePrincipalHelper final {
 public:
  static nsresult Create(nsIChannel* aChannel, nsIPrincipal* aPrincipal,
                         bool aForceIsolation,
                         nsIPrincipal** aStoragePrincipal);

  static nsresult CreatePartitionedPrincipalForServiceWorker(
      nsIPrincipal* aPrincipal, nsICookieJarSettings* aCookieJarSettings,
      nsIPrincipal** aPartitionedPrincipal);

  static nsresult PrepareEffectiveStoragePrincipalOriginAttributes(
      nsIChannel* aChannel, OriginAttributes& aOriginAttributes);

  static bool VerifyValidStoragePrincipalInfoForPrincipalInfo(
      const mozilla::ipc::PrincipalInfo& aStoragePrincipalInfo,
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

  enum PrincipalType {
    // This is the first-party principal.
    eRegularPrincipal,

    // This is a dynamic principal based on the current state of the origin. If
    // the origin has the storage permission granted, effective storagePrincipal
    // will be the regular principal, otherwise, the partitioned Principal
    // will be used.
    eStorageAccessPrincipal,

    // This is the first-party principal, plus, First-party isolation attribute
    // set.
    ePartitionedPrincipal,
  };

  /**
   * Extract the right OriginAttributes from the channel's triggering principal.
   */
  static bool GetOriginAttributes(nsIChannel* aChannel,
                                  OriginAttributes& aAttributes,
                                  PrincipalType aPrincipalType);

  static bool GetRegularPrincipalOriginAttributes(
      dom::Document* aDocument, OriginAttributes& aAttributes);

  static bool GetRegularPrincipalOriginAttributes(
      nsILoadGroup* aLoadGroup, OriginAttributes& aAttributes);

  // These methods return the correct originAttributes to be used for network
  // state components (HSTS, network cache, image-cache, and so on).
  static bool GetOriginAttributesForNetworkState(nsIChannel* aChanel,
                                                 OriginAttributes& aAttributes);
  static void GetOriginAttributesForNetworkState(dom::Document* aDocument,
                                                 OriginAttributes& aAttributes);
  static void UpdateOriginAttributesForNetworkState(
      nsIURI* aFirstPartyURI, OriginAttributes& aAttributes);
};

}  // namespace mozilla

#endif  // mozilla_StoragePrincipalHelper_h
