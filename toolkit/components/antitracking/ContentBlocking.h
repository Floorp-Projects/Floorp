/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingservice_h
#define mozilla_antitrackingservice_h

#include "nsString.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_privacy.h"

class nsIChannel;
class nsICookieJarSettings;
class nsIPermission;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;

namespace mozilla {

class OriginAttributes;

namespace dom {
class BrowsingContext;
class ContentParent;
}  // namespace dom

class ContentBlocking final {
 public:
  // This method returns true if the URI has first party storage access when
  // loaded inside the passed 3rd party context tracking resource window.
  // If the window is first party context, please use
  // ApproximateAllowAccessForWithoutChannel();
  //
  // aRejectedReason could be set to one of these values if passed and if the
  // storage permission is not granted:
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
  static bool ShouldAllowAccessFor(nsPIDOMWindowInner* a3rdPartyTrackingWindow,
                                   nsIURI* aURI, uint32_t* aRejectedReason);

  // Note: you should use ShouldAllowAccessFor() passing the nsIChannel! Use
  // this method _only_ if the channel is not available.  For first party
  // window, it's impossible to know if the aURI is a tracking resource
  // synchronously, so here we return the best guest: if we are sure that the
  // permission is granted for the origin of aURI, this method returns true,
  // otherwise false.
  static bool ApproximateAllowAccessForWithoutChannel(
      nsPIDOMWindowInner* aFirstPartyWindow, nsIURI* aURI);

  // It returns true if the URI has access to the first party storage.
  // aChannel can be a 3rd party channel, or not.
  // See ShouldAllowAccessFor(window) to see the possible values of
  // aRejectedReason.
  static bool ShouldAllowAccessFor(nsIChannel* aChannel, nsIURI* aURI,
                                   uint32_t* aRejectedReason);

  // This method checks if the principal has the permission to access to the
  // first party storage.
  static bool ShouldAllowAccessFor(nsIPrincipal* aPrincipal,
                                   nsICookieJarSettings* aCookieJarSettings);

  enum StorageAccessPromptChoices { eAllow, eAllowAutoGrant };

  // Grant the permission for aOrigin to have access to the first party storage.
  // This method can handle 2 different scenarios:
  // - aParentContext is a 3rd party context, it opens an aOrigin window and the
  //   user interacts with it. We want to grant the permission at the
  //   combination: top-level + aParentWindow + aOrigin.
  //   Ex: example.net loads an iframe tracker.com, which opens a popup
  //   tracker.prg and the user interacts with it. tracker.org is allowed if
  //   loaded by tracker.com when loaded by example.net.
  // - aParentContext is a first party context and a 3rd party resource
  // (probably
  //   becuase of a script) opens a popup and the user interacts with it. We
  //   want to grant the permission for the 3rd party context to have access to
  //   the first party stoage when loaded in aParentWindow.
  //   Ex: example.net import tracker.com/script.js which does opens a popup and
  //   the user interacts with it. tracker.com is allowed when loaded by
  //   example.net.
  typedef MozPromise<int, bool, true> StorageAccessFinalCheckPromise;
  typedef std::function<RefPtr<StorageAccessFinalCheckPromise>()>
      PerformFinalChecks;
  typedef MozPromise<int, bool, true> StorageAccessGrantPromise;
  static MOZ_MUST_USE RefPtr<StorageAccessGrantPromise> AllowAccessFor(
      nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
      ContentBlockingNotifier::StorageAccessGrantedReason aReason,
      const PerformFinalChecks& aPerformFinalChecks = nullptr);

  // This function handles tasks that have to be done in the process
  // of the window that we just grant permission for.
  static void OnAllowAccessFor(
      dom::BrowsingContext* aParentContext, const nsCString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessGrantedReason aReason);

  // For IPC only.
  typedef MozPromise<nsresult, bool, true> ParentAccessGrantPromise;
  static RefPtr<ParentAccessGrantPromise> SaveAccessForOriginOnParentProcess(
      nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrinciapl,
      const nsCString& aTrackingOrigin, int aAllowMode,
      uint64_t aExpirationTime =
          StaticPrefs::privacy_restrict3rdpartystorage_expiration());

  static RefPtr<ParentAccessGrantPromise> SaveAccessForOriginOnParentProcess(
      uint64_t aTopLevelWindowId, dom::BrowsingContext* aParentContext,
      nsIPrincipal* aTrackingPrinciapl, const nsCString& aTrackingOrigin,
      int aAllowMode,
      uint64_t aExpirationTime =
          StaticPrefs::privacy_restrict3rdpartystorage_expiration());

 private:
  friend class dom::ContentParent;
  // This should be running either in the parent process or in the child
  // processes with an in-process browsing context.
  static MOZ_MUST_USE RefPtr<StorageAccessGrantPromise> CompleteAllowAccessFor(
      dom::BrowsingContext* aParentContext, uint64_t aTopLevelWindowId,
      nsIPrincipal* aTrackingPrincipal, const nsCString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessGrantedReason aReason,
      const PerformFinalChecks& aPerformFinalChecks = nullptr);

  friend class dom::Document;
  static bool HasStorageAccessGranted(nsPIDOMWindowInner* aWindow);

  static bool HasStorageAccessGranted(dom::BrowsingContext* aBrowsingContext,
                                      const nsACString& aPermissionKey);

  static void UpdateAllowAccessOnCurrentProcess(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin);

  static void UpdateAllowAccessOnParentProcess(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin);
};

}  // namespace mozilla

#endif  // mozilla_antitrackingservice_h
