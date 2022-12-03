/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_
#define MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_

#include "ErrorList.h"
#include "mozIStorageConnection.h"
#include "mozilla/AlreadyAddRefed.h"
#include "nsIIdentityCredentialStorageService.h"
#include "mozilla/dom/FlippedOnce.h"
#include "nsIObserver.h"
#include "nsISupports.h"

namespace mozilla {

class IdentityCredentialStorageService final
    : public nsIIdentityCredentialStorageService,
      public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDENTITYCREDENTIALSTORAGESERVICE
  NS_DECL_NSIOBSERVER

  // Returns the singleton instance which is addreffed.
  static already_AddRefed<IdentityCredentialStorageService> GetSingleton();

  IdentityCredentialStorageService(const IdentityCredentialStorageService&) =
      delete;
  IdentityCredentialStorageService& operator=(
      const IdentityCredentialStorageService&) = delete;

 private:
  IdentityCredentialStorageService() = default;
  ~IdentityCredentialStorageService();
  nsresult Init();
  static nsresult ValidatePrincipal(nsIPrincipal* aPrincipal);

  FlippedOnce<false> mInitialized;
};

}  // namespace mozilla

#endif /* MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_ */
