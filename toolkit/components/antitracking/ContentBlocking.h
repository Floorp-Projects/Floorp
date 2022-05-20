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

  typedef MozPromise<nsresult, uint32_t, true> AsyncShouldAllowAccessForPromise;
  [[nodiscard]] static RefPtr<AsyncShouldAllowAccessForPromise>
  AsyncShouldAllowAccessFor(dom::BrowsingContext* aBrowsingContext,
                            nsIPrincipal* aPrincipal);

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
  typedef MozPromise<int, bool, true> StorageAccessPermissionGrantPromise;
  [[nodiscard]] static RefPtr<StorageAccessPermissionGrantPromise>
  AllowAccessFor(
      nsIPrincipal* aPrincipal, dom::BrowsingContext* aParentContext,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      const PerformFinalChecks& aPerformFinalChecks = nullptr);

  // This function handles tasks that have to be done in the process
  // of the window that we just grant permission for.
  static void OnAllowAccessFor(
      dom::BrowsingContext* aParentContext, const nsCString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason);

  // For IPC only.
  typedef MozPromise<nsresult, bool, true> ParentAccessGrantPromise;
  static RefPtr<ParentAccessGrantPromise> SaveAccessForOriginOnParentProcess(
      nsIPrincipal* aParentPrincipal, nsIPrincipal* aTrackingPrincipal,
      int aAllowMode,
      uint64_t aExpirationTime =
          StaticPrefs::privacy_restrict3rdpartystorage_expiration());

  static RefPtr<ParentAccessGrantPromise> SaveAccessForOriginOnParentProcess(
      uint64_t aTopLevelWindowId, dom::BrowsingContext* aParentContext,
      nsIPrincipal* aTrackingPrincipal, int aAllowMode,
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

  // This function checks if the browser settings give explicit permission
  // either to allow or deny access to cookies. This only checks the
  // cookieBehavior setting. This requires an additional bool to indicate
  // whether or not the context considered is third-party. This returns:
  //   Some(true) if unpartitioned cookies will be permitted
  //   Some(false) if unpartitioned cookies will be blocked
  //   None if it is not clear from settings alone what to do
  static Maybe<bool> CheckBrowserSettingsDecidesStorageAccessAPI(
      nsICookieJarSettings* aCookieJarSettings, bool aThirdParty);

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
      dom::Document* aDocument);

 private:
  friend class dom::ContentParent;
  // This should be running either in the parent process or in the child
  // processes with an in-process browsing context.
  [[nodiscard]] static RefPtr<StorageAccessPermissionGrantPromise>
  CompleteAllowAccessFor(
      dom::BrowsingContext* aParentContext, uint64_t aTopLevelWindowId,
      nsIPrincipal* aTrackingPrincipal, const nsCString& aTrackingOrigin,
      uint32_t aCookieBehavior,
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason,
      const PerformFinalChecks& aPerformFinalChecks = nullptr);

  static void UpdateAllowAccessOnCurrentProcess(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin);

  static void UpdateAllowAccessOnParentProcess(
      dom::BrowsingContext* aParentContext, const nsACString& aTrackingOrigin);

  typedef MozPromise<uint32_t, nsresult, true> CheckTrackerForPrincipalPromise;
  class TrackerClassifierFeatureCallback final
      : public nsIUrlClassifierFeatureCallback {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURLCLASSIFIERFEATURECALLBACK

    RefPtr<CheckTrackerForPrincipalPromise> Promise() {
      return mHolder.Ensure(__func__);
    }

    void Reject(nsresult rv) { mHolder.Reject(rv, __func__); }

    TrackerClassifierFeatureCallback() = default;

   private:
    ~TrackerClassifierFeatureCallback() = default;

    MozPromiseHolder<CheckTrackerForPrincipalPromise> mHolder;
  };

  // This method checks if the given princpal belongs to a tracker or a social
  // tracker.
  [[nodiscard]] static RefPtr<CheckTrackerForPrincipalPromise>
  CheckTrackerForPrincipal(nsIPrincipal* aPrincipal);
};

}  // namespace mozilla

#endif  // mozilla_antitrackingservice_h
