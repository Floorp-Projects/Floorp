/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingservice_h
#define mozilla_antitrackingservice_h

#include "nsString.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"

#define USER_INTERACTION_PERM NS_LITERAL_CSTRING("storageAccessAPI")

class nsIChannel;
class nsICookieSettings;
class nsIPermission;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowInner;

namespace mozilla {

class OriginAttributes;

class AntiTrackingCommon final {
 public:
  // Normally we would include PContentParent.h here and use the
  // ipc::FirstPartyStorageAccessGrantedForOriginResolver type which maps to
  // the same underlying type, but that results in Windows compilation errors,
  // so we use the underlying type to avoid the #include here.
  typedef std::function<void(const bool&)>
      FirstPartyStorageAccessGrantedForOriginResolver;

  // This method returns true if the URI has first party storage access when
  // loaded inside the passed 3rd party context tracking resource window.
  // If the window is first party context, please use
  // MaybeIsFirstPartyStorageAccessGrantedFor();
  //
  // aRejectedReason could be set to one of these values if passed and if the
  // storage permission is not granted:
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
  static bool IsFirstPartyStorageAccessGrantedFor(
      nsPIDOMWindowInner* a3rdPartyTrackingWindow, nsIURI* aURI,
      uint32_t* aRejectedReason);

  // Note: you should use IsFirstPartyStorageAccessGrantedFor() passing the
  // nsIChannel! Use this method _only_ if the channel is not available.
  // For first party window, it's impossible to know if the aURI is a tracking
  // resource synchronously, so here we return the best guest: if we are sure
  // that the permission is granted for the origin of aURI, this method returns
  // true, otherwise false.
  static bool MaybeIsFirstPartyStorageAccessGrantedFor(
      nsPIDOMWindowInner* aFirstPartyWindow, nsIURI* aURI);

  // It returns true if the URI has access to the first party storage.
  // aChannel can be a 3rd party channel, or not.
  // See IsFirstPartyStorageAccessGrantedFor(window) to see the possible values
  // of aRejectedReason.
  static bool IsFirstPartyStorageAccessGrantedFor(nsIChannel* aChannel,
                                                  nsIURI* aURI,
                                                  uint32_t* aRejectedReason);

  // This method checks if the principal has the permission to access to the
  // first party storage.
  static bool IsFirstPartyStorageAccessGrantedFor(
      nsIPrincipal* aPrincipal, nsICookieSettings* aCookieSettings);

  enum StorageAccessGrantedReason {
    eStorageAccessAPI,
    eOpenerAfterUserInteraction,
    eOpener
  };
  enum StorageAccessPromptChoices { eAllow, eAllowAutoGrant, eAllowOnAnySite };

  // Grant the permission for aOrigin to have access to the first party storage.
  // This method can handle 2 different scenarios:
  // - aParentWindow is a 3rd party context, it opens an aOrigin window and the
  //   user interacts with it. We want to grant the permission at the
  //   combination: top-level + aParentWindow + aOrigin.
  //   Ex: example.net loads an iframe tracker.com, which opens a popup
  //   tracker.prg and the user interacts with it. tracker.org is allowed if
  //   loaded by tracker.com when loaded by example.net.
  // - aParentWindow is a first party context and a 3rd party resource (probably
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
  static MOZ_MUST_USE RefPtr<StorageAccessGrantPromise>
  AddFirstPartyStorageAccessGrantedFor(
      nsIPrincipal* aPrincipal, nsPIDOMWindowInner* aParentWindow,
      StorageAccessGrantedReason aReason,
      const PerformFinalChecks& aPerformFinalChecks = nullptr);

  // Given a principal, returns the storage permission key that will be used for
  // the principal.  Returns true on success.
  static bool CreateStoragePermissionKey(nsIPrincipal* aPrincipal,
                                         nsACString& aKey);

  // Returns true if the permission passed in is a storage access permission
  // for the passed in principal argument.
  static bool IsStorageAccessPermission(nsIPermission* aPermission,
                                        nsIPrincipal* aPrincipal);

  static void StoreUserInteractionFor(nsIPrincipal* aPrincipal);

  static bool HasUserInteraction(nsIPrincipal* aPrincipal);

  // This API allows consumers to get notified when the anti-tracking component
  // settings change.  After this callback is called, an anti-tracking check
  // that has been previously performed with the same parameters may now return
  // a different result.
  typedef std::function<void()> AntiTrackingSettingsChangedCallback;
  static void OnAntiTrackingSettingsChanged(
      const AntiTrackingSettingsChangedCallback& aCallback);

  // For IPC only.
  typedef MozPromise<nsresult, bool, true> FirstPartyStorageAccessGrantPromise;
  static RefPtr<FirstPartyStorageAccessGrantPromise>
  SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(
      nsIPrincipal* aPrincipal, nsIPrincipal* aTrackingPrinciapl,
      const nsCString& aParentOrigin, const nsCString& aGrantedOrigin,
      int aAllowMode);

  // Check whether a top window URI is on the content blocking allow list.
  static nsresult IsOnContentBlockingAllowList(nsIURI* aTopWinURI,
                                               bool aIsPrivateBrowsing,
                                               bool& aIsAllowListed);

  // Computes the principal used to check the content blocking allow list for a
  // top-level document based on the document principal.  This function is used
  // right after setting up the document principal.
  static void ComputeContentBlockingAllowListPrincipal(
      nsIPrincipal* aDocumentPrincipal, nsIPrincipal** aPrincipal);

  static void RecomputeContentBlockingAllowListPrincipal(
      nsIURI* aURIBeingLoaded, const OriginAttributes& aAttrs,
      nsIPrincipal** aPrincipal);

  enum class BlockingDecision {
    eBlock,
    eAllow,
  };

  // This method can be called on the parent process or on the content process.
  // The notification is propagated to the child channel if aChannel is a parent
  // channel proxy.
  //
  // aDecision can be eBlock if we have decided to block some content, or eAllow
  // if we have decided to allow the content through.
  //
  // aRejectedReason must be one of these values:
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL
  //  * nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
  static void NotifyBlockingDecision(nsIChannel* aChannel,
                                     BlockingDecision aDecision,
                                     uint32_t aRejectedReason);

  static void NotifyBlockingDecision(nsPIDOMWindowInner* aWindow,
                                     BlockingDecision aDecision,
                                     uint32_t aRejectedReason);

  // Get the current document URI from a document channel as it is being loaded.
  static already_AddRefed<nsIURI> MaybeGetDocumentURIBeingLoaded(
      nsIChannel* aChannel);
};

}  // namespace mozilla

#endif  // mozilla_antitrackingservice_h
