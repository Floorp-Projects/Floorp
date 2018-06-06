/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPK11TokenDB_h
#define nsPK11TokenDB_h

#include "nsCOMPtr.h"
#include "nsIPK11Token.h"
#include "nsIPK11TokenDB.h"
#include "nsISupports.h"
#include "nsNSSHelper.h"
#include "nsString.h"
#include "pk11func.h"
#include "ScopedNSSTypes.h"

class nsPK11Token : public nsIPK11Token
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPK11TOKEN

  explicit nsPK11Token(PK11SlotInfo *slot);

protected:
  virtual ~nsPK11Token() {}

private:
  friend class nsPK11TokenDB;
  nsresult refreshTokenInfo();

  nsCString mTokenName;
  nsCString mTokenManufacturerID;
  nsCString mTokenHWVersion;
  nsCString mTokenFWVersion;
  nsCString mTokenSerialNum;
  mozilla::UniquePK11SlotInfo mSlot;
  // True if this is the "PKCS#11 token" that provides cryptographic functions.
  bool mIsInternalCryptoToken;
  // True if this is the "PKCS#11 token" where private keys are stored.
  bool mIsInternalKeyToken;
  int mSeries;
  nsCOMPtr<nsIInterfaceRequestor> mUIContext;
  nsresult GetAttributeHelper(const nsACString& attribute,
                      /*out*/ nsACString& xpcomOutParam);
};

class nsPK11TokenDB : public nsIPK11TokenDB
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPK11TOKENDB

  nsPK11TokenDB() {}

protected:
  virtual ~nsPK11TokenDB() {}
};

#define NS_PK11TOKENDB_CID \
{ 0xb084a2ce, 0x1dd1, 0x11b2, \
  { 0xbf, 0x10, 0x83, 0x24, 0xf8, 0xe0, 0x65, 0xcc }}

#endif // nsPK11TokenDB_h
