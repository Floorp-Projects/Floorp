/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 */

#ifndef __NS_PKCS11SLOT_H__
#define __NS_PKCS11SLOT_H__

#include "nsISupports.h"
#include "nsIPKCS11Slot.h"
#include "nsIPKCS11Module.h"
#include "nsIPKCS11ModuleDB.h"
#include "nsString.h"
#include "pk11func.h"
#include "nsNSSShutDown.h"

class nsPKCS11Slot : public nsIPKCS11Slot,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11SLOT

  nsPKCS11Slot(PK11SlotInfo *slot);
  virtual ~nsPKCS11Slot();

private:

  PK11SlotInfo *mSlot;
  nsString mSlotDesc, mSlotManID, mSlotHWVersion, mSlotFWVersion;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsPKCS11Module : public nsIPKCS11Module,
                       public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULE

  nsPKCS11Module(SECMODModule *module);
  virtual ~nsPKCS11Module();

private:
  SECMODModule *mModule;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsPKCS11ModuleDB : public nsIPKCS11ModuleDB
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULEDB

  nsPKCS11ModuleDB();
  virtual ~nsPKCS11ModuleDB();
  /* additional members */
};

#define NS_PKCS11MODULEDB_CID \
{ 0xff9fbcd7, 0x9517, 0x4334, \
  { 0xb9, 0x7a, 0xce, 0xed, 0x78, 0x90, 0x99, 0x74 }}

#endif
