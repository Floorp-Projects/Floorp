/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_contentblockingallowlist_h
#define mozilla_contentblockingallowlist_h

class nsICookieJarSettings;
class nsIHttpChannel;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowInner;

namespace mozilla {

class OriginAttributes;
struct ContentBlockingAllowListCache;

class ContentBlockingAllowList final {
 public:
  // Check whether a principal is on the content blocking allow list.
  // aPrincipal should be a "content blocking allow list principal".
  // This principal can be obtained from the load info object for top-level
  // windows.
  static nsresult Check(nsIPrincipal* aContentBlockingAllowListPrincipal,
                        bool aIsPrivateBrowsing, bool& aIsAllowListed);

  static bool Check(nsIHttpChannel* aChannel);

  // Computes the principal used to check the content blocking allow list for a
  // top-level document based on the document principal.  This function is used
  // right after setting up the document principal.
  static void ComputePrincipal(nsIPrincipal* aDocumentPrincipal,
                               nsIPrincipal** aPrincipal);

  static void RecomputePrincipal(nsIURI* aURIBeingLoaded,
                                 const OriginAttributes& aAttrs,
                                 nsIPrincipal** aPrincipal);

 private:
  // Utility APIs for ContentBlocking.
  static bool Check(nsIPrincipal* aTopWinPrincipal, bool aIsPrivateBrowsing);
  static bool Check(nsPIDOMWindowInner* aWindow);
  static bool Check(nsICookieJarSettings* aCookieJarSettings);

  friend class ContentBlocking;
};

}  // namespace mozilla

#endif  // mozilla_contentblockingallowlist_h
