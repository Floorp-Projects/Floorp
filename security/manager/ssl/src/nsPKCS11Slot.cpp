/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

#include "nsPKCS11Slot.h"
#include "nsPK11TokenDB.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#include "secmod.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

NS_IMPL_ISUPPORTS1(nsPKCS11Slot, nsIPKCS11Slot)

nsPKCS11Slot::nsPKCS11Slot(PK11SlotInfo *slot)
{
  NS_INIT_ISUPPORTS();
  PK11_ReferenceSlot(slot);
  mSlot = slot;

  CK_SLOT_INFO slot_info;
  if (PK11_GetSlotInfo(mSlot, &slot_info) == SECSuccess) {
    // Set the Description field
    mSlotDesc.AssignWithConversion((char *)slot_info.slotDescription, 
                                   sizeof(slot_info.slotDescription));
    mSlotDesc.Trim(" ", PR_FALSE, PR_TRUE);
    // Set the Manufacturer field
    mSlotManID.AssignWithConversion((char *)slot_info.manufacturerID, 
                                    sizeof(slot_info.manufacturerID));
    mSlotManID.Trim(" ", PR_FALSE, PR_TRUE);
    // Set the Hardware Version field
    mSlotHWVersion.AppendInt(slot_info.hardwareVersion.major);
    mSlotHWVersion.AppendWithConversion(".");
    mSlotHWVersion.AppendInt(slot_info.hardwareVersion.minor);
    // Set the Firmware Version field
    mSlotFWVersion.AppendInt(slot_info.firmwareVersion.major);
    mSlotFWVersion.AppendWithConversion(".");
    mSlotFWVersion.AppendInt(slot_info.firmwareVersion.minor);
  }

}

nsPKCS11Slot::~nsPKCS11Slot()
{
  if (mSlot) PK11_FreeSlot(mSlot);
}

/* readonly attribute wstring name; */
NS_IMETHODIMP 
nsPKCS11Slot::GetName(PRUnichar **aName)
{
  char *csn = PK11_GetSlotName(mSlot);
  if (strlen(csn) > 0) {
    *aName = ToNewUnicode(NS_ConvertUTF8toUCS2(csn));
  } else if (PK11_HasRootCerts(mSlot)) {
    // This is a workaround to an NSS bug - the root certs module has
    // no slot name.  Not bothering to localize, because this is a workaround
    // and for now all the slot names returned by NSS are char * anyway.
    *aName = ToNewUnicode(NS_LITERAL_STRING("Root Certificates"));
  } else {
    // same as above, this is a catch-all
    *aName = ToNewUnicode(NS_LITERAL_STRING("Unnamed Slot"));
  }
  if (!*aName) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring desc; */
NS_IMETHODIMP 
nsPKCS11Slot::GetDesc(PRUnichar **aDesc)
{
  *aDesc = ToNewUnicode(mSlotDesc);
  if (!*aDesc) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring manID; */
NS_IMETHODIMP 
nsPKCS11Slot::GetManID(PRUnichar **aManID)
{
  *aManID = ToNewUnicode(mSlotManID);
  if (!*aManID) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring HWVersion; */
NS_IMETHODIMP 
nsPKCS11Slot::GetHWVersion(PRUnichar **aHWVersion)
{
  *aHWVersion = ToNewUnicode(mSlotHWVersion);
  if (!*aHWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* readonly attribute wstring FWVersion; */
NS_IMETHODIMP 
nsPKCS11Slot::GetFWVersion(PRUnichar **aFWVersion)
{
  *aFWVersion = ToNewUnicode(mSlotFWVersion);
  if (!*aFWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

/* nsIPK11Token getToken (); */
NS_IMETHODIMP 
nsPKCS11Slot::GetToken(nsIPK11Token **_retval)
{
  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(mSlot);
  if (!token)
    return NS_ERROR_OUT_OF_MEMORY;
  *_retval = token;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* readonly attribute wstring tokenName; */
NS_IMETHODIMP 
nsPKCS11Slot::GetTokenName(PRUnichar **aName)
{
  *aName = ToNewUnicode(NS_ConvertUTF8toUCS2(PK11_GetTokenName(mSlot)));
  if (!*aName) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetStatus(PRUint32 *_retval)
{
  if (PK11_IsDisabled(mSlot))
    *_retval = SLOT_DISABLED;
  else if (!PK11_IsPresent(mSlot))
    *_retval = SLOT_NOT_PRESENT;
  else if (PK11_NeedLogin(mSlot) && PK11_NeedUserInit(mSlot))
    *_retval = SLOT_UNINITIALIZED;
  else if (PK11_NeedLogin(mSlot) && !PK11_IsLoggedIn(mSlot, NULL))
    *_retval = SLOT_NOT_LOGGED_IN;
  else if (PK11_NeedLogin(mSlot))
    *_retval = SLOT_LOGGED_IN;
  else
    *_retval = SLOT_READY;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsPKCS11Module, nsIPKCS11Module)

nsPKCS11Module::nsPKCS11Module(SECMODModule *module)
{
  NS_INIT_ISUPPORTS();
  SECMOD_ReferenceModule(module);
  mModule = module;
}

nsPKCS11Module::~nsPKCS11Module()
{
  SECMOD_DestroyModule(mModule);
}

/* readonly attribute wstring name; */
NS_IMETHODIMP 
nsPKCS11Module::GetName(PRUnichar **aName)
{
  *aName = ToNewUnicode(NS_ConvertUTF8toUCS2(mModule->commonName));
  return NS_OK;
}

/* readonly attribute wstring libName; */
NS_IMETHODIMP 
nsPKCS11Module::GetLibName(PRUnichar **aName)
{
  *aName = ToNewUnicode(NS_ConvertUTF8toUCS2(mModule->dllName));
  return NS_OK;
}

/*  nsIPKCS11Slot findSlotByName(in wstring name); */
NS_IMETHODIMP 
nsPKCS11Module::FindSlotByName(const PRUnichar *aName,
                               nsIPKCS11Slot **_retval)
{
  char *asciiname = NULL;
  asciiname = ToNewUTF8String(nsDependentString(aName));
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting \"%s\"\n", asciiname));
  PK11SlotInfo *slotinfo = SECMOD_FindSlot(mModule, asciiname);
  if (!slotinfo) {
    // XXX *sigh*  if token is present, SECMOD_FindSlot goes by token
    // name (huh?)  reimplement it here for the fun of it.
    for (int i=0; i<mModule->slotCount; i++) {
      if (nsCRT::strcmp(asciiname, PK11_GetSlotName(mModule->slots[i])) == 0) {
        slotinfo = PK11_ReferenceSlot(mModule->slots[i]);
      }
    }
    if (!slotinfo) {
      // XXX another workaround - the builtin module has no name
      if (nsCRT::strcmp(asciiname, "Root Certificates") == 0) {
        slotinfo = PK11_ReferenceSlot(mModule->slots[0]);
      } else {
        // give up
        nsMemory::Free(asciiname);
        return NS_ERROR_FAILURE;
      }
    }
  }
  nsMemory::Free(asciiname);
  nsCOMPtr<nsIPKCS11Slot> slot = new nsPKCS11Slot(slotinfo);
  if (!slot)
    return NS_ERROR_OUT_OF_MEMORY;
  *_retval = slot;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIEnumerator listSlots (); */
NS_IMETHODIMP 
nsPKCS11Module::ListSlots(nsIEnumerator **_retval)
{
  nsresult rv = NS_OK;
  int i;
  /* get isupports array */
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) return rv;
  for (i=0; i<mModule->slotCount; i++) {
    if (mModule->slots[i]) {
      nsCOMPtr<nsIPKCS11Slot> slot = new nsPKCS11Slot(mModule->slots[i]);
      array->AppendElement(slot);
    }
  }
  rv = array->Enumerate(_retval);
  return rv;
}

NS_IMPL_ISUPPORTS1(nsPKCS11ModuleDB, nsIPKCS11ModuleDB)

nsPKCS11ModuleDB::nsPKCS11ModuleDB()
{
  NS_INIT_ISUPPORTS();
}

nsPKCS11ModuleDB::~nsPKCS11ModuleDB()
{
}

/* nsIPKCS11Module getInternal (); */
NS_IMETHODIMP 
nsPKCS11ModuleDB::GetInternal(nsIPKCS11Module **_retval)
{
  nsCOMPtr<nsIPKCS11Module> module = 
     new nsPKCS11Module(SECMOD_GetInternalModule());
  if (!module)
    return NS_ERROR_OUT_OF_MEMORY;
  *_retval = module;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIPKCS11Module getInternalFIPS (); */
NS_IMETHODIMP 
nsPKCS11ModuleDB::GetInternalFIPS(nsIPKCS11Module **_retval)
{
  nsCOMPtr<nsIPKCS11Module> module = 
     new nsPKCS11Module(SECMOD_GetFIPSInternal());
  if (!module)
    return NS_ERROR_OUT_OF_MEMORY;
  *_retval = module;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIPKCS11Module findModuleByName(in wstring name); */
NS_IMETHODIMP 
nsPKCS11ModuleDB::FindModuleByName(const PRUnichar *aName,
                                   nsIPKCS11Module **_retval)
{
  SECMODModule *mod =
    SECMOD_FindModule(NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(aName).get()));
  if (!mod)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(mod);
  if (!module)
    return NS_ERROR_OUT_OF_MEMORY;
  *_retval = module;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* This is essentially the same as nsIPK11Token::findTokenByName, except
 * that it returns an nsIPKCS11Slot, which may be desired.
 */
/* nsIPKCS11Module findSlotByName(in wstring name); */
NS_IMETHODIMP 
nsPKCS11ModuleDB::FindSlotByName(const PRUnichar *aName,
                                 nsIPKCS11Slot **_retval)
{
  PK11SlotInfo *slotinfo =
   PK11_FindSlotByName(NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(aName).get()));
  if (!slotinfo)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPKCS11Slot> slot = new nsPKCS11Slot(slotinfo);
  if (!slot)
    return NS_ERROR_OUT_OF_MEMORY;
  *_retval = slot;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIEnumerator listModules (); */
NS_IMETHODIMP 
nsPKCS11ModuleDB::ListModules(nsIEnumerator **_retval)
{
  nsresult rv = NS_OK;
  /* get isupports array */
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) return rv;
  /* get the default list of modules */
  SECMODModuleList *list = SECMOD_GetDefaultModuleList();
  /* lock down the list for reading */
  SECMODListLock *lock = SECMOD_GetDefaultModuleListLock();
  SECMOD_GetReadLock(lock);
  while (list) {
    nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(list->module);
    array->AppendElement(module);
    list = list->next;
  }
  SECMOD_ReleaseReadLock(lock);
  rv = array->Enumerate(_retval);
  return rv;
}

/* void toggleFIPSMode (); */
NS_IMETHODIMP nsPKCS11ModuleDB::ToggleFIPSMode()
{
  // The way to toggle FIPS mode in NSS is extremely obscure.
  // Basically, we delete the internal module, and voila it
  // gets replaced with the opposite module, ie if it was 
  // FIPS before, then it becomes non-FIPS next.
  SECMODModule *internal;

  // This function returns us a pointer to a local copy of 
  // the internal module stashed in NSS.  We don't want to
  // delete it since it will cause much pain in NSS.
  internal = SECMOD_GetInternalModule();
  if (!internal)
    return NS_ERROR_FAILURE;

  SECStatus srv = SECMOD_DeleteInternalModule(internal->commonName);
  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

/* readonly attribute boolean isFIPSEnabled; */
NS_IMETHODIMP nsPKCS11ModuleDB::GetIsFIPSEnabled(PRBool *aIsFIPSEnabled)
{
  *aIsFIPSEnabled = PK11_IsFIPS();
  return NS_OK;
}
