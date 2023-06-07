/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICryptoHash.h"

extern "C" {
nsresult crypto_hash_constructor(REFNSIID iid, void** result);
};

nsresult NS_NewCryptoHash(uint32_t aHashType, nsICryptoHash** aOutHasher) {
  MOZ_ASSERT(aOutHasher);

  nsCOMPtr<nsICryptoHash> hasher;
  nsresult rv =
      crypto_hash_constructor(NS_ICRYPTOHASH_IID, getter_AddRefs(hasher));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = hasher->Init(aHashType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  hasher.forget(aOutHasher);

  return NS_OK;
}

nsresult NS_NewCryptoHash(const nsACString& aHashType,
                          nsICryptoHash** aOutHasher) {
  MOZ_ASSERT(aOutHasher);

  nsCOMPtr<nsICryptoHash> hasher;
  nsresult rv =
      crypto_hash_constructor(NS_ICRYPTOHASH_IID, getter_AddRefs(hasher));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = hasher->InitWithString(aHashType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  hasher.forget(aOutHasher);

  return NS_OK;
}
