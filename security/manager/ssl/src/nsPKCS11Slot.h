/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

class nsPKCS11ModuleDB : public nsIPKCS11ModuleDB,
                         public nsICryptoFIPSInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPKCS11MODULEDB
  NS_DECL_NSICRYPTOFIPSINFO

  nsPKCS11ModuleDB();
  virtual ~nsPKCS11ModuleDB();
  /* additional members */
};

#define NS_PKCS11MODULEDB_CID \
{ 0xff9fbcd7, 0x9517, 0x4334, \
  { 0xb9, 0x7a, 0xce, 0xed, 0x78, 0x90, 0x99, 0x74 }}

#endif
