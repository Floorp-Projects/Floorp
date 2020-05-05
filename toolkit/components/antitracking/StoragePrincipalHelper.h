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
 *
 * StoragePrincipal is the nsIPrincipal to be used to open the cookie jar of a
 * resource's origin. Normally, the StoragePrincipal corresponds to the
 * resource's origin, but, in some scenarios, it can be different: it can have
 * some extra origin attributes.
 *
 * Each storage component, should always use the StoragePrincipal instead of the
 * 'real' one in order to implement the partitioning correctly.
 *
 * On the web, each resource has its own origin (see
 * https://html.spec.whatwg.org/multipage/origin.html#concept-origin) and each
 * origin has its own cookie jar, containing cookies, storage data, cache and so
 * on.
 *
 * In addition, gecko has a set of attributes to differentiate the same origin
 * in different contexts (OriginAttributes). The main ones are:
 * - privateBrowsingId, for private browsing navigation.
 * - userContextId, for containers
 * - firstPartyIsolation, for TOR.
 *
 * In gecko-world, the origin and its attributes are stored and managed by the
 * nsIPrincipal interface. Both resource's Principal and resource's
 * StoragePrincipal are nsIPrincipal interfaces and, normally, they are the same
 * object.
 *
 * Here is the way you can obtain the two Principals:
 * From a Document:
 * - Document's principal: nsINode::NodePrincipal
 * - Document's StoragePrincipal: Document::EffectiveStoragePrincipal
 * From a Global object:
 * - nsIScriptObjectPrincipal::getPrincipal
 * - nsIScriptObjectPrincipal::getEffectiveStoragePrincipal
 * From a Worker:
 * - WorkerPrivate::GetPrincipal().
 * - WorkerPrivate::GetEffectiveStoragePrincipal();
 * For a nsIChannel, the final principals must be calculated and they can be
 * obtained by calling:
 * - nsIScriptSecurityManager::getChannelResultPrincipal() or
 * - nsIScriptSecurityManager::getChannelResultStoragePrincipal().
 *
 * Principal VS StoragePrincipal
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * At the moment, we are experimenting the partitioning of cookie jars for 3rd
 * party trackers: each 3rd party origin, detected as a tracker, will have a
 * partitioned cookie jar, created by the tracker's origin, plus, the
 * first-party domain.
 *
 * This means that, for those origins, StoragePrincipal will be  equal to the
 * main Principal but it will also have the 'first-party-domain' attribute set
 * as the first-party URL's domain. Because of this, the tracker's cookie jar
 * will be partitioned and it will be unique per first-party domain.
 *
 * The naminig is important. This is why Document has the StoragePrincipal
 * stored in a member variable called mIntrinsicStoragePrincipal: this
 * storagePrincipal is immutable even when Document::EffectiveStoragePrincipal
 * returns the main principal.
 *
 * Storage access permission
 * ~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * A resource's origin and its attributes are immutable. StoragePrincipal can
 * change: when a tracker has the storage permission granted, its
 * StoragePrincipal becomes equal to its document's principal. In this way, the
 * tracker will have access to its first-party cookie jar, escaping from the
 * its partitioning.
 *
 * To know more about when the storage access permission is granted, see the
 * anti-tracking project's documentation.
 * See: https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API and
 * https://developer.mozilla.org/en-US/docs/Mozilla/Firefox/Privacy/Storage_access_policy#Storage_access_grants
 *
 *
 * When the storage access permission is granted, any of the StoragePrincipal
 * getter methods will return the main principal instead of the storage one, and
 * each storage component should consider the new Principal only.
 *
 * There are several ways to receive storage-permission notifications:
 * - Add some code in nsGlobalWindowInner::StorageAccessGranted().
 * - WorkerScope::FirstPartyStorageAccessGranted for Workers.
 * - observe the permission changes (not recommended)
 *
 * SharedWorkers and BroadcastChannels
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * SharedWorker and BroadcastChannel instances latch the effective storage
 * principal at the moment of their creation.  Existing bindings to the
 * partitioned storage principal will continue to exist and operate even as it
 * becomes possible to create bindings associated with the non-partitioned node
 * principal.  This makes it possible for such globals to bi-directionally
 * bridge information between partitioned and non-partitioned principals.
 *
 * {Dedicated,Shared,Service}Workers
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The storage access permission propagation happens with a ControlRunnable.
 * This could impact the use of sync event-loops. Take a reference of the
 * principal you want to use because it can change!
 *
 * ServiceWorkers are currently disabled for partitioned contexts.
 *
 * Client API uses the main principal always because there is not a direct
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
                         nsIPrincipal** aStoragePrincipal);

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
    // will be the regular principal, otherwise, the intrinsic storagePrincipal
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
};

}  // namespace mozilla

#endif  // mozilla_StoragePrincipalHelper_h
