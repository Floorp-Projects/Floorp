/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPKCS11Slot_h
#define nsPKCS11Slot_h

#include "ScopedNSSTypes.h"
#include "nsIPKCS11Module.h"
#include "nsIPKCS11Slot.h"
#include "nsISupports.h"
#include "nsString.h"
#include "pk11func.h"

class nsPKCS11Slot : public nsIPKCS11Slot
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11SLOT

  explicit nsPKCS11Slot(PK11SlotInfo* slot);

protected:
  virtual ~nsPKCS11Slot() {}

private:
  mozilla::UniquePK11SlotInfo mSlot;
  // True if this is the "PKCS#11 slot" that provides cryptographic functions.
  bool mIsInternalCryptoSlot;
  // True if this is the "PKCS#11 slot" where private keys are stored.
  bool mIsInternalKeySlot;
  nsCString mSlotDesc;
  nsCString mSlotManufacturerID;
  nsCString mSlotHWVersion;
  nsCString mSlotFWVersion;
  int mSeries;

  nsresult refreshSlotInfo();
  nsresult GetAttributeHelper(const nsACString& attribute,
                      /*out*/ nsACString& xpcomOutParam);
};

class nsPKCS11Module : public nsIPKCS11Module
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULE

  explicit nsPKCS11Module(SECMODModule* module);

protected:
  virtual ~nsPKCS11Module() {}

private:
  mozilla::UniqueSECMODModule mModule;
};

#endif // nsPKCS11Slot_h
