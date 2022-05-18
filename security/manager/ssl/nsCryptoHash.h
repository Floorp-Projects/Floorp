/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCryptoHash_h
#define nsCryptoHash_h

#include "ScopedNSSTypes.h"
#include "hasht.h"
#include "nsICryptoHash.h"
#include "secmodt.h"

class nsIInputStream;

#define NS_CRYPTO_HASH_CID                           \
  {                                                  \
    0x36a1d3b3, 0xd886, 0x4317, {                    \
      0x96, 0xff, 0x87, 0xb0, 0x00, 0x5c, 0xfe, 0xf7 \
    }                                                \
  }

class nsCryptoHash final : public nsICryptoHash {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRYPTOHASH

  nsCryptoHash();

 private:
  virtual ~nsCryptoHash() = default;

  mozilla::UniqueHASHContext mHashContext;
  bool mInitialized;
};

#endif  // nsCryptoHash_h
