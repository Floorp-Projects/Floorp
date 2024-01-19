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

#include "nsIUrlClassifierFeature.h"

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
class Document;
}  // namespace dom

class StorageAccessAPIHelper final {
 public:
  enum StorageAccessPromptChoices { eAllow, eAllowAutoGrant };

  // Grant the permission for aOrigin to have access to the first party storage.
  // This methods can handle 2 different scenarios:
  // - aParentContext is a 3rd party context, it opens an aOrigin window and the
  //   user interacts with it. We want to grant the permission at the
  //   combination: top-level + aParentWindow + aOrigin.
  //   Ex: example.net loads an iframe tracker.com, which opens a popup
  //   tracker.org and the user interacts with it. tracker.org is allowed if
  //   loaded by tracker.com when loaded by example.net.
  // - aParentContext is a first party context and a 3rd party resource
  //   (probably becuase of a script) opens a popup and the user interacts with
  //   it. We want to grant the permission for the 3rd party context to have
  //   access to the first party stoage when loaded in aParentWindow. Ex:
  //   example.net import tracker.com/script.js which does opens a popup and the
  //   user interacts with it. tracker.com is allowed when loaded by
  //   example.net.
  typedef MozPromise<int, bool, true> StorageAccessPermissionGrantPromise;
  typedef std::function<RefPtr<StorageAccessPermissionGrantPromise>()>
      PerformPermissionGrant;
  [[nodiscard]] static RefPtr<StorageAccessPermissionGrantPromise>
  AllowAccessForOnParentProcess(
      nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      const PerformPermissionGrant& aPerformFinalChecks = nullptr);

  [[nodiscard]] static RefPtr<StorageAccessPermissionGrantPromise>
  AllowAccessForOnChildProcess(
      nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      const PerformPermissionGrant& aPerformFinalChecks = nullptr);

  // This function handles tasks that have to be done in the process
  // of the window that we just grant permission for.
  static void OnAllowAccessFor(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason);

  // For IPC only.
  typedef MozPromise<nsresult, bool, true> ParentAccessGrantPromise;
  static RefPtr<ParentAccessGrantPromise> SaveAccessForOriginOnParentProcess(
      nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrincipal,
      int aAllowMode, bool aFrameOnly,
      uint64_t aExpirationTime =
          StaticPrefs::privacy_restrict3rdpartystorage_expiration());

  static RefPtr<ParentAccessGrantPromise> SaveAccessForOriginOnParentProcess(
      uint64_t aTopLevelWindowId, dom::BrowsingContext* aParentContext,
      nsIPrincipal* aTrackingPrincipal, int aAllowMode, bool aFrameOnly,
      uint64_t aExpirationTime =
          StaticPrefs::privacy_restrict3rdpartystorage_expiration());

  // This function checks if the document has explicit permission either to
  // allow or deny access to cookies. This may be because of the "cookie"
  // permission or because the domain is on the ContentBlockingAllowList
  // e.g. because the user flipped the sheild.
  // This returns:
  //   Some(true) if unpartitioned cookies will be permitted
  //   Some(false) if unpartitioned cookies will be blocked
  //   None if it is not clear from permission alone what to do
  static Maybe<bool> CheckCookiesPermittedDecidesStorageAccessAPI(
      nsICookieJarSettings* aCookieJarSettings,
      nsIPrincipal* aRequestingPrincipal);

  // Calls CheckCookiesPermittedDecidesStorageAccessAPI in the Content Parent
  // using aBrowsingContext's Top's Window Global's CookieJarSettings.
  static RefPtr<MozPromise<Maybe<bool>, nsresult, true>>
  AsyncCheckCookiesPermittedDecidesStorageAccessAPIOnChildProcess(
      dom::BrowsingContext* aBrowsingContext,
      nsIPrincipal* aRequestingPrincipal);

  // This function checks if the browser settings give explicit permission
  // either to allow or deny access to cookies. This only checks the
  // cookieBehavior setting. This requires an additional bool to indicate
  // whether or not the context considered is third-party. This returns:
  //   Some(true) if unpartitioned cookies will be permitted
  //   Some(false) if unpartitioned cookies will be blocked
  //   None if it is not clear from settings alone what to do
  static Maybe<bool> CheckBrowserSettingsDecidesStorageAccessAPI(
      nsICookieJarSettings* aCookieJarSettings, bool aThirdParty,
      bool aIsOnThirdPartySkipList, bool aIsThirdPartyTracker);

  // This function checks if the document's context (like if it is third-party
  // or an iframe) gives an answer of how a the StorageAccessAPI call, that is
  // meant to be called by an embedded third party, should return.
  // This requires an argument that allows some checks to be run only if the
  // caller of this function is performing a request for storage access.
  // This returns:
  //   Some(true) if the calling context has access to cookies if it is not
  //              disallowed by the browser settings and cookie permissions
  //   Some(false) if the calling context should not have access to cookies if
  //               it is not expressly allowed by the browser settings and
  //               cookie permissions
  //   None if the calling context does not determine the document's access to
  //        unpartitioned cookies
  static Maybe<bool> CheckCallingContextDecidesStorageAccessAPI(
      dom::Document* aDocument, bool aRequestingStorageAccess);

  // This function checks if the document's context (like if it is third-party
  // or an iframe) gives an answer of how a the StorageAccessAPI call that is
  // meant to be called in a top-level context, should return.
  // This returns:
  //   Some(true) if the calling context indicates calls to the top-level
  //              API must resolve if it is not
  //              disallowed by the browser settings and cookie permissions
  //   Some(false) if the calling context must reject when calling top level
  //               portions of the API if it is not expressly allowed by the
  //               browser settings and cookie permissions
  //   None if the calling context does not determine the outcome of the
  //        document's use of the top-level portions of the Storage Access API.
  static Maybe<bool> CheckSameSiteCallingContextDecidesStorageAccessAPI(
      dom::Document* aDocument, bool aRequireUserActivation);

  // This function checks if the document has already been granted or denied
  // access to its unpartitioned cookies by the StorageAccessAPI
  // This returns:
  //   Some(true) if the document has been granted access by the Storage Access
  //              API before
  //   Some(false) if the document has been denied access by the Storage Access
  //               API before
  //   None if the document has not been granted or denied access by the Storage
  //        Access API before
  static Maybe<bool> CheckExistingPermissionDecidesStorageAccessAPI(
      dom::Document* aDocument, bool aRequestingStorageAccess);

  // This function performs the asynchronous portion of checking if requests
  // for storage access will be successful or not. This includes calling
  // Document member functions that creating a permission prompt request and
  // trying to perform an "autogrant" if aRequireGrant is true.
  // This will return a promise whose values correspond to those of a
  // ContentBlocking::AllowAccessFor call that ends the function.
  static RefPtr<StorageAccessPermissionGrantPromise>
  RequestStorageAccessAsyncHelper(
      dom::Document* aDocument, nsPIDOMWindowInner* aInnerWindow,
      dom::BrowsingContext* aBrowsingContext, nsIPrincipal* aPrincipal,
      bool aHasUserInteraction, bool aRequireUserInteraction, bool aFrameOnly,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aNotifier,
      bool aRequireGrant);

 private:
  friend class dom::ContentParent;

  // This function performs browser setting, cookie behavior and requesting
  // context checks that might grant/reject storage access immediately using
  // information provided by the inputs aPrincipal and aParentContext. To reduce
  // redundancy the following out parameters with information also required in
  // AllowAccessFor() are set in the function: aTrackingPrinciple,
  // aTrackingOrigin, aTopLevelWindowId, aBehavior. If storage access can be
  // granted/rejected due to settings/behavior returns a promise, else returns
  // nullptr.
  [[nodiscard]] static RefPtr<
      StorageAccessAPIHelper::StorageAccessPermissionGrantPromise>
  AllowAccessForHelper(
      nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      nsCOMPtr<nsIPrincipal>* aTrackingPrincipal, nsACString& aTrackingOrigin,
      uint64_t* aTopLevelWindowId, uint32_t* aBehavior);

  [[nodiscard]] static RefPtr<StorageAccessPermissionGrantPromise>
  CompleteAllowAccessForOnParentProcess(
      dom::BrowsingContext* aParentContext, uint64_t aTopLevelWindowId,
      nsIPrincipal* aTrackingPrincipal, const nsACString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      const PerformPermissionGrant& aPerformFinalChecks = nullptr);

  [[nodiscard]] static RefPtr<StorageAccessPermissionGrantPromise>
  CompleteAllowAccessForOnChildProcess(
      dom::BrowsingContext* aParentContext, uint64_t aTopLevelWindowId,
      nsIPrincipal* aTrackingPrincipal, const nsACString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      const PerformPermissionGrant& aPerformFinalChecks = nullptr);

  static void UpdateAllowAccessOnCurrentProcess(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin);

  static void UpdateAllowAccessOnParentProcess(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin);
};

}  // namespace mozilla

#endif  // mozilla_antitrackingservice_h
