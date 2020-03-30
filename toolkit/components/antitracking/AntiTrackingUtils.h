/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingutils_h
#define mozilla_antitrackingutils_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsStringFwd.h"

class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;
class nsIChannel;
class nsIPermission;
class nsIPrincipal;
class nsIURI;

namespace mozilla {

class AntiTrackingUtils final {
 public:
  static already_AddRefed<nsPIDOMWindowOuter> GetTopWindow(
      nsPIDOMWindowInner* aWindow);

  // Get the current document URI from a document channel as it is being loaded.
  static already_AddRefed<nsIURI> MaybeGetDocumentURIBeingLoaded(
      nsIChannel* aChannel);

  static void CreateStoragePermissionKey(const nsCString& aTrackingOrigin,
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
};

}  // namespace mozilla

#endif  // mozilla_antitrackingutils_h
