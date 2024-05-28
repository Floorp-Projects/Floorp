/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_contentblockingallowlist_h
#define mozilla_contentblockingallowlist_h

#include "mozilla/dom/BrowsingContext.h"
#include "nsIContentBlockingAllowList.h"
#include "nsIPermission.h"
#include "nsTHashSet.h"

class nsICookieJarSettings;
class nsIHttpChannel;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowInner;

namespace mozilla {

class OriginAttributes;

/**
 * @class ContentBlockingAllowListCache
 *
 * @brief This class represents a cache for the content blocking allow list. It
 *        is used for repeated lookups of the allow list for a specific base
 *        domain. Only use it if you need base domain lookups. In most cases
 *        this is not what you want. For regular allow-list checks by principal
 *        please use ContentBlockingAllowList.
 */
class ContentBlockingAllowListCache {
 public:
  /**
   * @brief Checks if a given base domain is allow-listed. This method considers
   * the domain to be allow list if either the base domain or any of its
   * subdomains are allow-listed.
   * This is different from regular allow-list checks,
   * @see{ContentBlockingAllowList::Check} where allow-listed state is only
   * inherited to subdomains if the base domain is allow-listed.
   *
   *  Example:
   *  If "example.com" is allow-listed, then "www.example.com" is also
   *  considered allow-listed.
   *  If foobar.example.org is allow-listed, then "example.org" is not
   *  considered allow-listed.
   *
   * @param aBaseDomain The base domain to check.
   * @param aOriginAttributes The origin attributes associated with the base
   * domain.
   * @param aIsAllowListed [out] Set to true if the base domain is allow-listed,
   * false otherwise.
   *
   * @return NS_OK if the check is successful, or an error code otherwise.
   */
  nsresult CheckForBaseDomain(const nsACString& aBaseDomain,
                              const OriginAttributes& aOriginAttributes,
                              bool& aIsAllowListed);

 protected:
  // The following methods may be overridden by subclasses for adding custom
  // allow-list permissions.

  /**
   * @brief Returns the list of permission types that are used to check if a
   *        site is allow-listed.
   *
   * @return An array of permission types.
   */
  virtual nsTArray<nsCString> GetAllowListPermissionTypes();

  /**
   * @brief Checks if a permission has an allow-list state by inspecting its
   * fields e.g. capability. Permission type is not checked here, it is assumed
   * that the permission type is one of the types returned by
   * GetAllowListPermissionTypes.
   *
   * @param aPermission The permission to check.
   * @param aResult [out] Set to true if the permission is an allow-list
   * permission, false otherwise.
   *
   * @return NS_OK if the check is successful, or an error code otherwise.
   */
  virtual nsresult IsAllowListPermission(nsIPermission* aPermission,
                                         bool* aResult);

 private:
  bool mIsInitialized = false;

  // The cache is a hash set of base domains. If a base domain is in the set, it
  // is allow-listed for that context (normal browsing, private browsing.)
  nsTHashSet<nsCString> mEntries;
  nsTHashSet<nsCString> mEntriesPrivateBrowsing;

  /**
   * @brief Initializes the content blocking allow list cache if needed.
   *
   * @return NS_OK if initialization is successful, or an error code otherwise.
   */
  nsresult EnsureInit();
};

class ContentBlockingAllowList final : public nsIContentBlockingAllowList {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTBLOCKINGALLOWLIST
  // Check whether a principal is on the content blocking allow list.
  // aPrincipal should be a "content blocking allow list principal".
  // This principal can be obtained from the load info object for top-level
  // windows.
  static nsresult Check(nsIPrincipal* aContentBlockingAllowListPrincipal,
                        bool aIsPrivateBrowsing, bool& aIsAllowListed);

  static bool Check(nsIHttpChannel* aChannel);
  // Utility APIs for ContentBlocking.
  static bool Check(nsPIDOMWindowInner* aWindow);
  static bool Check(nsIPrincipal* aTopWinPrincipal, bool aIsPrivateBrowsing);
  static bool Check(nsICookieJarSettings* aCookieJarSettings);

  // Computes the principal used to check the content blocking allow list for a
  // top-level document based on the document principal.  This function is used
  // right after setting up the document principal.
  static void ComputePrincipal(nsIPrincipal* aDocumentPrincipal,
                               nsIPrincipal** aPrincipal);

  static void RecomputePrincipal(nsIURI* aURIBeingLoaded,
                                 const OriginAttributes& aAttrs,
                                 nsIPrincipal** aPrincipal);

 private:
  ~ContentBlockingAllowList() = default;
};

}  // namespace mozilla

#endif  // mozilla_contentblockingallowlist_h
