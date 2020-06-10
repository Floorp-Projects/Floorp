/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingutils_h
#define mozilla_antitrackingutils_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsStringFwd.h"
#include "ContentBlockingNotifier.h"

class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;
class nsIChannel;
class nsIPermission;
class nsIPrincipal;
class nsIURI;

namespace mozilla {
namespace dom {
class BrowsingContext;
class CanonicalBrowsingContext;
class WindowGlobalParent;
}  // namespace dom

class AntiTrackingUtils final {
 public:
  static already_AddRefed<nsPIDOMWindowInner> GetInnerWindow(
      dom::BrowsingContext* aBrowsingContext);

  static already_AddRefed<nsPIDOMWindowOuter> GetTopWindow(
      nsPIDOMWindowInner* aWindow);

  // Get the current document URI from a document channel as it is being loaded.
  static already_AddRefed<nsIURI> MaybeGetDocumentURIBeingLoaded(
      nsIChannel* aChannel);

  static void CreateStoragePermissionKey(const nsACString& aTrackingOrigin,
                                         nsACString& aPermissionKey);

  // Given a principal, returns the storage permission key that will be used for
  // the principal.  Returns true on success.
  static bool CreateStoragePermissionKey(nsIPrincipal* aPrincipal,
                                         nsACString& aKey);

  // Returns true if the permission passed in is a storage access permission
  // for the passed in principal argument.
  static bool IsStorageAccessPermission(nsIPermission* aPermission,
                                        nsIPrincipal* aPrincipal);

  // Returns true if the storage permission is granted for the given principal
  // and the storage permission key.
  static bool CheckStoragePermission(nsIPrincipal* aPrincipal,
                                     const nsAutoCString& aType,
                                     bool aIsInPrivateBrowsing,
                                     uint32_t* aRejectedReason,
                                     uint32_t aBlockedReason);

  // Returns true if the storage permission is granted for the given channel.
  // And this is meant to be called in the parent process. This only reflects
  // the fact that whether the channel has the storage permission. It doesn't
  // take the window hierarchy into account. i.e. this will return true even
  // for a nested iframe that has storage permission.
  static bool HasStoragePermissionInParent(nsIChannel* aChannel);

  // Returns the toplevel inner window id, returns 0 if this is a toplevel
  // window.
  static uint64_t GetTopLevelAntiTrackingWindowId(
      dom::BrowsingContext* aBrowsingContext);

  // Returns the parent inner window id, returns 0 if this or the parent are not
  // a toplevel window. This is mainly used to determine the anti-tracking
  // storage area.
  static uint64_t GetTopLevelStorageAreaWindowId(
      dom::BrowsingContext* aBrowsingContext);

  // Returns the principal of the given browsing context.
  // This API should only be used either in child processes with an in-process
  // browsing context or in the parent process.
  static already_AddRefed<nsIPrincipal> GetPrincipal(
      dom::BrowsingContext* aBrowsingContext);

  // Returns the principal of the given browsing context and tracking origin.
  // This API should only be used either in child processes with an in-process
  // browsing context or in the parent process.
  static bool GetPrincipalAndTrackingOrigin(
      dom::BrowsingContext* aBrowsingContext, nsIPrincipal** aPrincipal,
      nsACString& aTrackingOrigin);

  // Retruns true if the given browsingContext is a first-level sub context,
  // i.e. a first-level iframe.
  static bool IsFirstLevelSubContext(dom::BrowsingContext* aBrowsingContext);

  // Retruns the cookie behavior of the given browsingContext,
  // return BEHAVIOR_REJECT when fail.
  static uint32_t GetCookieBehavior(dom::BrowsingContext* aBrowsingContext);

  // Returns the top-level global window parent. But we would stop at the
  // content window which is loaded by addons and consider this window as a top.
  //
  // Note that this is the parent-process implementation of
  // nsGlobalWindowOuter::GetTopExcludingExtensionAccessibleContentFrames
  static already_AddRefed<dom::WindowGlobalParent>
  GetTopWindowExcludingExtensionAccessibleContentFrames(
      dom::CanonicalBrowsingContext* aBrowsingContext, nsIURI* aURIBeingLoaded);

  // Given a channel, compute and set the IsThirdPartyContextToTopWindow for
  // this channel. This function is supposed to be called in the parent process.
  static void ComputeIsThirdPartyToTopWindow(nsIChannel* aChannel);

  // Given a channel, this function determines if this channel is a third party.
  // Note that this function also considers the top-level window. The channel
  // will be considered as a third party only when it's a third party to both
  // its parent and the top-level window.
  static bool IsThirdPartyChannel(nsIChannel* aChannel);

  // Given a window and a URI, this function first determines if the window is
  // third-party with respect to the URI. The function returns if it's true.
  // Otherwise, it will continue to check if the window is third-party.
  static bool IsThirdPartyWindow(nsPIDOMWindowInner* aWindow, nsIURI* aURI);

  static nsCString GrantedReasonToString(
      ContentBlockingNotifier::StorageAccessPermissionGrantedReason aReason);
};

}  // namespace mozilla

#endif  // mozilla_antitrackingutils_h
