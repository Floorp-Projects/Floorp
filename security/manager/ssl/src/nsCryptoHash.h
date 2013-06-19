/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCryptoHash_h_
#define _nsCryptoHash_h_

#include "nsICryptoHash.h"
#include "nsICryptoHMAC.h"
#include "nsNSSShutDown.h"
#include "hasht.h"
#include "secmodt.h"

class nsIInputStream;

#define NS_CRYPTO_HASH_CID {0x36a1d3b3, 0xd886, 0x4317, {0x96, 0xff, 0x87, 0xb0, 0x00, 0x5c, 0xfe, 0xf7}}
#define NS_CRYPTO_HMAC_CID {0xa496d0a2, 0xdff7, 0x4e23, {0xbd, 0x65, 0x1c, 0xa7, 0x42, 0xfa, 0x17, 0x8a}}

class nsCryptoHash : public nsICryptoHash, public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRYPTOHASH

  nsCryptoHash();

private:
  ~nsCryptoHash();

  HASHContext* mHashContext;
  bool mInitialized;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsCryptoHMAC : public nsICryptoHMAC, public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRYPTOHMAC

  nsCryptoHMAC();

private:
  ~nsCryptoHMAC();
  PK11Context* mHMACContext;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

#endif // _nsCryptoHash_h_

