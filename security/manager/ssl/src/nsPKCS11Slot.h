/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_PKCS11SLOT_H__
#define __NS_PKCS11SLOT_H__

#include "nsISupports.h"
#include "nsIPKCS11Slot.h"
#include "nsIPKCS11Module.h"
#include "nsIPKCS11ModuleDB.h"
#include "nsICryptoFIPSInfo.h"
#include "nsString.h"
#include "pk11func.h"
#include "nsNSSShutDown.h"

class nsPKCS11Slot : public nsIPKCS11Slot,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11SLOT

  explicit nsPKCS11Slot(PK11SlotInfo *slot);

protected:
  virtual ~nsPKCS11Slot();

private:

  PK11SlotInfo *mSlot;
  nsString mSlotDesc, mSlotManID, mSlotHWVersion, mSlotFWVersion;
  int mSeries;

  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();
  void refreshSlotInfo();
};

class nsPKCS11Module : public nsIPKCS11Module,
                       public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULE

  explicit nsPKCS11Module(SECMODModule *module);

protected:
  virtual ~nsPKCS11Module();

private:
  SECMODModule *mModule;

  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();
};

class nsPKCS11ModuleDB : public nsIPKCS11ModuleDB,
                         public nsICryptoFIPSInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULEDB
  NS_DECL_NSICRYPTOFIPSINFO

  nsPKCS11ModuleDB();

protected:
  virtual ~nsPKCS11ModuleDB();
  /* additional members */
};

#define NS_PKCS11MODULEDB_CID \
{ 0xff9fbcd7, 0x9517, 0x4334, \
  { 0xb9, 0x7a, 0xce, 0xed, 0x78, 0x90, 0x99, 0x74 }}

#endif
