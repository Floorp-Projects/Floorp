/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_temporaryaccessgrantobserver_h
#define mozilla_temporaryaccessgrantobserver_h

#include "mozilla/PrincipalHashKey.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsHashtablesFwd.h"
#include "nsTHashMap.h"
#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "PLDHashTable.h"

class nsITimer;
class TemporaryAccessGrantCacheKey;

namespace mozilla {

class PermissionManager;

class TemporaryAccessGrantCacheKey : public PrincipalHashKey {
 public:
  typedef std::pair<nsCOMPtr<nsIPrincipal>, nsCString> KeyType;
  typedef const KeyType* KeyTypePointer;

  explicit TemporaryAccessGrantCacheKey(KeyTypePointer aKey)
      : PrincipalHashKey(aKey->first), mType(aKey->second) {}
  TemporaryAccessGrantCacheKey(TemporaryAccessGrantCacheKey&& aOther) = default;

  ~TemporaryAccessGrantCacheKey() = default;

  KeyType GetKey() const { return std::make_pair(mPrincipal, mType); }
  bool KeyEquals(KeyTypePointer aKey) const {
    return PrincipalHashKey::KeyEquals(aKey->first) && mType == aKey->second;
  }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    if (!aKey) {
      return 0;
    }

    return HashGeneric(PrincipalHashKey::HashKey(aKey->first),
                       HashString(aKey->second));
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  nsCString mType;
};

class TemporaryAccessGrantObserver final : public nsIObserver, public nsINamed {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSINAMED

  static void Create(PermissionManager* aPM, nsIPrincipal* aPrincipal,
                     const nsACString& aType);

  void SetTimer(nsITimer* aTimer);

 private:
  TemporaryAccessGrantObserver(PermissionManager* aPM, nsIPrincipal* aPrincipal,
                               const nsACString& aType);
  ~TemporaryAccessGrantObserver() = default;

 private:
  using ObserversTable =
      nsTHashMap<TemporaryAccessGrantCacheKey, nsCOMPtr<nsITimer>>;
  static StaticAutoPtr<ObserversTable> sObservers;
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<PermissionManager> mPM;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
};

}  // namespace mozilla

#endif  // mozilla_temporaryaccessgrantobserver_h
