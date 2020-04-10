/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_temporaryaccessgrantobserver_h
#define mozilla_temporaryaccessgrantobserver_h

#include "mozilla/BasePrincipal.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "PLDHashTable.h"

template <class, class>
class nsDataHashtable;
class nsITimer;
class nsPermissionManager;
class TemporaryAccessGrantCacheKey;

namespace mozilla {

class TemporaryAccessGrantCacheKey : public PLDHashEntryHdr {
 public:
  typedef std::pair<nsCOMPtr<nsIPrincipal>, nsCString> KeyType;
  typedef const KeyType* KeyTypePointer;

  explicit TemporaryAccessGrantCacheKey(KeyTypePointer aKey)
      : mPrincipal(aKey->first), mType(aKey->second) {}
  TemporaryAccessGrantCacheKey(TemporaryAccessGrantCacheKey&& aOther) = default;

  ~TemporaryAccessGrantCacheKey() = default;

  KeyType GetKey() const { return std::make_pair(mPrincipal, mType); }
  bool KeyEquals(KeyTypePointer aKey) const {
    return !!mPrincipal == !!aKey->first && mType == aKey->second &&
           (mPrincipal ? (mPrincipal->Equals(aKey->first)) : true);
  }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    if (!aKey) {
      return 0;
    }

    BasePrincipal* bp = BasePrincipal::Cast(aKey->first);
    return HashGeneric(bp->GetOriginNoSuffixHash(), bp->GetOriginSuffixHash(),
                       HashString(aKey->second));
  }

  enum { ALLOW_MEMMOVE = true };

 private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
};

class TemporaryAccessGrantObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Create(nsPermissionManager* aPM, nsIPrincipal* aPrincipal,
                     const nsACString& aType);

  void SetTimer(nsITimer* aTimer);

 private:
  TemporaryAccessGrantObserver(nsPermissionManager* aPM,
                               nsIPrincipal* aPrincipal,
                               const nsACString& aType);
  ~TemporaryAccessGrantObserver() = default;

 private:
  typedef nsDataHashtable<TemporaryAccessGrantCacheKey, nsCOMPtr<nsITimer>>
      ObserversTable;
  static UniquePtr<ObserversTable> sObservers;
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<nsPermissionManager> mPM;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCString mType;
};

}  // namespace mozilla

#endif  // mozilla_temporaryaccessgrantobserver_h
