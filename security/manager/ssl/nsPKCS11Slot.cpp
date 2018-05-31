/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPKCS11Slot.h"

#include <string.h>

#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsNSSComponent.h"
#include "nsPK11TokenDB.h"
#include "nsPromiseFlatString.h"
#include "secmod.h"

using mozilla::LogLevel;

extern mozilla::LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsPKCS11Slot, nsIPKCS11Slot)

nsPKCS11Slot::nsPKCS11Slot(PK11SlotInfo* slot)
{
  MOZ_ASSERT(slot);
  mSlot.reset(PK11_ReferenceSlot(slot));
  mSeries = PK11_GetSlotSeries(slot);
  Unused << refreshSlotInfo();
}

nsresult
nsPKCS11Slot::refreshSlotInfo()
{
  CK_SLOT_INFO slotInfo;
  nsresult rv = MapSECStatus(PK11_GetSlotInfo(mSlot.get(), &slotInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set the Description field
  const char* ccDesc =
    mozilla::BitwiseCast<char*, CK_UTF8CHAR*>(slotInfo.slotDescription);
  mSlotDesc.Assign(ccDesc, strnlen(ccDesc, sizeof(slotInfo.slotDescription)));
  mSlotDesc.Trim(" ", false, true);

  // Set the Manufacturer field
  const char* ccManID =
    mozilla::BitwiseCast<char*, CK_UTF8CHAR*>(slotInfo.manufacturerID);
  mSlotManufacturerID.Assign(
    ccManID,
    strnlen(ccManID, sizeof(slotInfo.manufacturerID)));
  mSlotManufacturerID.Trim(" ", false, true);

  // Set the Hardware Version field
  mSlotHWVersion.Truncate();
  mSlotHWVersion.AppendInt(slotInfo.hardwareVersion.major);
  mSlotHWVersion.Append('.');
  mSlotHWVersion.AppendInt(slotInfo.hardwareVersion.minor);

  // Set the Firmware Version field
  mSlotFWVersion.Truncate();
  mSlotFWVersion.AppendInt(slotInfo.firmwareVersion.major);
  mSlotFWVersion.Append('.');
  mSlotFWVersion.AppendInt(slotInfo.firmwareVersion.minor);

  return NS_OK;
}

nsresult
nsPKCS11Slot::GetAttributeHelper(const nsACString& attribute,
                         /*out*/ nsACString& xpcomOutParam)
{
  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  xpcomOutParam = attribute;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetName(/*out*/ nsACString& name)
{
  // |csn| is non-owning.
  char* csn = PK11_GetSlotName(mSlot.get());
  if (csn && *csn) {
    name = csn;
  } else if (PK11_HasRootCerts(mSlot.get())) {
    // This is a workaround to an Root Module bug - the root certs module has
    // no slot name.  Not bothering to localize, because this is a workaround
    // and for now all the slot names returned by NSS are char * anyway.
    name = NS_LITERAL_CSTRING("Root Certificates");
  } else {
    // same as above, this is a catch-all
    name = NS_LITERAL_CSTRING("Unnamed Slot");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetDesc(/*out*/ nsACString& desc)
{
  return GetAttributeHelper(mSlotDesc, desc);
}

NS_IMETHODIMP
nsPKCS11Slot::GetManID(/*out*/ nsACString& manufacturerID)
{
  return GetAttributeHelper(mSlotManufacturerID, manufacturerID);
}

NS_IMETHODIMP
nsPKCS11Slot::GetHWVersion(/*out*/ nsACString& hwVersion)
{
  return GetAttributeHelper(mSlotHWVersion, hwVersion);
}

NS_IMETHODIMP
nsPKCS11Slot::GetFWVersion(/*out*/ nsACString& fwVersion)
{
  return GetAttributeHelper(mSlotFWVersion, fwVersion);
}

NS_IMETHODIMP
nsPKCS11Slot::GetToken(nsIPK11Token** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(mSlot.get());
  token.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetTokenName(/*out*/ nsACString& tokenName)
{
  if (!PK11_IsPresent(mSlot.get())) {
    tokenName.SetIsVoid(true);
    return NS_OK;
  }

  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  tokenName = PK11_GetTokenName(mSlot.get());
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetStatus(uint32_t* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (PK11_IsDisabled(mSlot.get())) {
    *_retval = SLOT_DISABLED;
  } else if (!PK11_IsPresent(mSlot.get())) {
    *_retval = SLOT_NOT_PRESENT;
  } else if (PK11_NeedLogin(mSlot.get()) && PK11_NeedUserInit(mSlot.get())) {
    *_retval = SLOT_UNINITIALIZED;
  } else if (PK11_NeedLogin(mSlot.get()) &&
             !PK11_IsLoggedIn(mSlot.get(), nullptr)) {
    *_retval = SLOT_NOT_LOGGED_IN;
  } else if (PK11_NeedLogin(mSlot.get())) {
    *_retval = SLOT_LOGGED_IN;
  } else {
    *_retval = SLOT_READY;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsPKCS11Module, nsIPKCS11Module)

nsPKCS11Module::nsPKCS11Module(SECMODModule* module)
{
  MOZ_ASSERT(module);
  mModule.reset(SECMOD_ReferenceModule(module));
}

// Convert the UTF8 internal name of the module to how it should appear to the
// user. In most cases this involves simply passing back the module's name.
// However, the builtin roots module has a non-localized name internally that we
// must map to the localized version when we display it to the user.
static nsresult
NormalizeModuleNameOut(const char* moduleNameIn, nsACString& moduleNameOut)
{
  // Easy case: this isn't the builtin roots module.
  if (strnlen(moduleNameIn, kRootModuleNameLen + 1) != kRootModuleNameLen ||
      strncmp(kRootModuleName, moduleNameIn, kRootModuleNameLen) != 0) {
    moduleNameOut.Assign(moduleNameIn);
    return NS_OK;
  }

  nsAutoString localizedRootModuleName;
  nsresult rv = GetPIPNSSBundleString("RootCertModuleName",
                                      localizedRootModuleName);
  if (NS_FAILED(rv)) {
    return rv;
  }
  moduleNameOut.Assign(NS_ConvertUTF16toUTF8(localizedRootModuleName));
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Module::GetName(/*out*/ nsACString& name)
{
  return NormalizeModuleNameOut(mModule->commonName, name);
}

NS_IMETHODIMP
nsPKCS11Module::GetLibName(/*out*/ nsACString& libName)
{
  if (mModule->dllName) {
    libName = mModule->dllName;
  } else {
    libName.SetIsVoid(true);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Module::FindSlotByName(const nsACString& name,
                       /*out*/ nsIPKCS11Slot** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  const nsCString& flatName = PromiseFlatCString(name);
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Getting \"%s\"", flatName.get()));
  UniquePK11SlotInfo slotInfo;
  UniquePK11SlotList slotList(PK11_FindSlotsByNames(mModule->dllName,
                                                    flatName.get() /*slotName*/,
                                                    nullptr /*tokenName*/,
                                                    false));
  if (!slotList) {
    /* name must be the token name */
    slotList.reset(PK11_FindSlotsByNames(mModule->dllName, nullptr /*slotName*/,
                                         flatName.get() /*tokenName*/, false));
  }
  if (slotList && slotList->head && slotList->head->slot) {
    slotInfo.reset(PK11_ReferenceSlot(slotList->head->slot));
  }
  if (!slotInfo) {
    // workaround - the builtin module has no name
    if (!flatName.EqualsLiteral("Root Certificates")) {
      // Give up.
      return NS_ERROR_FAILURE;
    }

    slotInfo.reset(PK11_ReferenceSlot(mModule->slots[0]));
  }

  nsCOMPtr<nsIPKCS11Slot> slot = new nsPKCS11Slot(slotInfo.get());
  slot.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Module::ListSlots(nsISimpleEnumerator** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = CheckForSmartCardChanges();
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  /* applications which allow new slot creation (which Firefox now does
   * since it uses the WaitForSlotEvent call) need to hold the
   * ModuleList Read lock to prevent the slot array from changing out
   * from under it. */
  AutoSECMODListReadLock lock;
  for (int i = 0; i < mModule->slotCount; i++) {
    if (mModule->slots[i]) {
      nsCOMPtr<nsIPKCS11Slot> slot = new nsPKCS11Slot(mModule->slots[i]);
      rv = array->AppendElement(slot);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return array->Enumerate(_retval);
}
