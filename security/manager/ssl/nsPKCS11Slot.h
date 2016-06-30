/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPKCS11Slot_h
#define nsPKCS11Slot_h

#include "ScopedNSSTypes.h"
#include "nsICryptoFIPSInfo.h"
#include "nsIPKCS11Module.h"
#include "nsIPKCS11ModuleDB.h"
#include "nsIPKCS11Slot.h"
#include "nsISupports.h"
#include "nsNSSShutDown.h"
#include "nsString.h"
#include "pk11func.h"

class nsPKCS11Slot : public nsIPKCS11Slot,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11SLOT

  explicit nsPKCS11Slot(PK11SlotInfo* slot);

protected:
  virtual ~nsPKCS11Slot();

private:
  mozilla::UniquePK11SlotInfo mSlot;
  nsString mSlotDesc, mSlotManID, mSlotHWVersion, mSlotFWVersion;
  int mSeries;

  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();
  nsresult refreshSlotInfo(const nsNSSShutDownPreventionLock& proofOfLock);
};

class nsPKCS11Module : public nsIPKCS11Module,
                       public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULE

  explicit nsPKCS11Module(SECMODModule* module);

protected:
  virtual ~nsPKCS11Module();

private:
  mozilla::UniqueSECMODModule mModule;

  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();
};

class nsPKCS11ModuleDB : public nsIPKCS11ModuleDB
                       , public nsICryptoFIPSInfo
                       , public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULEDB
  NS_DECL_NSICRYPTOFIPSINFO

  nsPKCS11ModuleDB();

protected:
  virtual ~nsPKCS11ModuleDB();

  // Nothing to release.
  virtual void virtualDestroyNSSReference() override {}
};

#define NS_PKCS11MODULEDB_CID \
{ 0xff9fbcd7, 0x9517, 0x4334, \
  { 0xb9, 0x7a, 0xce, 0xed, 0x78, 0x90, 0x99, 0x74 }}

#endif // nsPKCS11Slot_h
