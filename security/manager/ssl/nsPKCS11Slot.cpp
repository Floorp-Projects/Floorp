/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPKCS11Slot.h"

#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsPK11TokenDB.h"
#include "secmod.h"

using mozilla::LogLevel;

extern mozilla::LazyLogModule gPIPNSSLog;

NS_IMPL_ISUPPORTS(nsPKCS11Slot, nsIPKCS11Slot)

nsPKCS11Slot::nsPKCS11Slot(PK11SlotInfo* slot)
{
  MOZ_ASSERT(slot);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  mSlot.reset(PK11_ReferenceSlot(slot));
  mSeries = PK11_GetSlotSeries(slot);
  Unused << refreshSlotInfo(locker);
}

nsresult
nsPKCS11Slot::refreshSlotInfo(const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  CK_SLOT_INFO slotInfo;
  nsresult rv = MapSECStatus(PK11_GetSlotInfo(mSlot.get(), &slotInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set the Description field
  const char* ccDesc =
    mozilla::BitwiseCast<char*, CK_UTF8CHAR*>(slotInfo.slotDescription);
  const nsACString& cDesc = Substring(
    ccDesc,
    ccDesc + PL_strnlen(ccDesc, sizeof(slotInfo.slotDescription)));
  mSlotDesc = NS_ConvertUTF8toUTF16(cDesc);
  mSlotDesc.Trim(" ", false, true);

  // Set the Manufacturer field
  const char* ccManID =
    mozilla::BitwiseCast<char*, CK_UTF8CHAR*>(slotInfo.manufacturerID);
  const nsACString& cManID = Substring(
    ccManID,
    ccManID + PL_strnlen(ccManID, sizeof(slotInfo.manufacturerID)));
  mSlotManID = NS_ConvertUTF8toUTF16(cManID);
  mSlotManID.Trim(" ", false, true);

  // Set the Hardware Version field
  mSlotHWVersion = EmptyString();
  mSlotHWVersion.AppendInt(slotInfo.hardwareVersion.major);
  mSlotHWVersion.Append('.');
  mSlotHWVersion.AppendInt(slotInfo.hardwareVersion.minor);

  // Set the Firmware Version field
  mSlotFWVersion = EmptyString();
  mSlotFWVersion.AppendInt(slotInfo.firmwareVersion.major);
  mSlotFWVersion.Append('.');
  mSlotFWVersion.AppendInt(slotInfo.firmwareVersion.minor);

  return NS_OK;
}

nsPKCS11Slot::~nsPKCS11Slot()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(ShutdownCalledFrom::Object);
}

void
nsPKCS11Slot::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
nsPKCS11Slot::destructorSafeDestroyNSSReference()
{
  mSlot = nullptr;
}

NS_IMETHODIMP
nsPKCS11Slot::GetName(char16_t** aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  // |csn| is non-owning.
  char* csn = PK11_GetSlotName(mSlot.get());
  if (*csn) {
    *aName = ToNewUnicode(NS_ConvertUTF8toUTF16(csn));
  } else if (PK11_HasRootCerts(mSlot.get())) {
    // This is a workaround to an Root Module bug - the root certs module has
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

NS_IMETHODIMP
nsPKCS11Slot::GetDesc(char16_t** aDesc)
{
  NS_ENSURE_ARG_POINTER(aDesc);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aDesc = ToNewUnicode(mSlotDesc);
  if (!*aDesc) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetManID(char16_t** aManID)
{
  NS_ENSURE_ARG_POINTER(aManID);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aManID = ToNewUnicode(mSlotManID);
  if (!*aManID) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetHWVersion(char16_t** aHWVersion)
{
  NS_ENSURE_ARG_POINTER(aHWVersion);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aHWVersion = ToNewUnicode(mSlotHWVersion);
  if (!*aHWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetFWVersion(char16_t** aFWVersion)
{
  NS_ENSURE_ARG_POINTER(aFWVersion);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  *aFWVersion = ToNewUnicode(mSlotFWVersion);
  if (!*aFWVersion) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetToken(nsIPK11Token** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(mSlot.get());
  token.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetTokenName(char16_t** aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (!PK11_IsPresent(mSlot.get())) {
    *aName = nullptr;
    return NS_OK;
  }

  if (PK11_GetSlotSeries(mSlot.get()) != mSeries) {
    nsresult rv = refreshSlotInfo(locker);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aName = ToNewUnicode(NS_ConvertUTF8toUTF16(PK11_GetTokenName(mSlot.get())));
  if (!*aName) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Slot::GetStatus(uint32_t* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  mModule.reset(SECMOD_ReferenceModule(module));
}

nsPKCS11Module::~nsPKCS11Module()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(ShutdownCalledFrom::Object);
}

void
nsPKCS11Module::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
nsPKCS11Module::destructorSafeDestroyNSSReference()
{
  mModule = nullptr;
}

NS_IMETHODIMP
nsPKCS11Module::GetName(char16_t** aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aName = ToNewUnicode(NS_ConvertUTF8toUTF16(mModule->commonName));
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Module::GetLibName(char16_t** aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if ( mModule->dllName ) {
    *aName = ToNewUnicode(NS_ConvertUTF8toUTF16(mModule->dllName));
  } else {
    *aName = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11Module::FindSlotByName(const char16_t* aName, nsIPKCS11Slot** _retval)
{
  // Note: It's OK for |aName| to be null.
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ConvertUTF16toUTF8 asciiname(aName);
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("Getting \"%s\"\n", asciiname.get()));
  UniquePK11SlotInfo slotInfo;
  UniquePK11SlotList slotList(PK11_FindSlotsByNames(mModule->dllName,
                                                    asciiname.get() /*slotName*/,
                                                    nullptr /*tokenName*/,
                                                    false));
  if (!slotList) {
    /* name must be the token name */
    slotList.reset(PK11_FindSlotsByNames(mModule->dllName, nullptr /*slotName*/,
                                         asciiname.get() /*tokenName*/, false));
  }
  if (slotList && slotList->head && slotList->head->slot) {
    slotInfo.reset(PK11_ReferenceSlot(slotList->head->slot));
  }
  if (!slotInfo) {
    // workaround - the builtin module has no name
    if (!asciiname.EqualsLiteral("Root Certificates")) {
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

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
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
      nsresult rv = array->AppendElement(slot, false);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return array->Enumerate(_retval);
}

NS_IMPL_ISUPPORTS(nsPKCS11ModuleDB, nsIPKCS11ModuleDB, nsICryptoFIPSInfo)

nsPKCS11ModuleDB::nsPKCS11ModuleDB()
{
}

nsPKCS11ModuleDB::~nsPKCS11ModuleDB()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(ShutdownCalledFrom::Object);
}

NS_IMETHODIMP
nsPKCS11ModuleDB::GetInternal(nsIPKCS11Module** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniqueSECMODModule nssMod(
    SECMOD_CreateModule(nullptr, SECMOD_INT_NAME, nullptr, SECMOD_INT_FLAGS));
  if (!nssMod) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(nssMod.get());
  module.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11ModuleDB::GetInternalFIPS(nsIPKCS11Module** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniqueSECMODModule nssMod(
    SECMOD_CreateModule(nullptr, SECMOD_FIPS_NAME, nullptr, SECMOD_FIPS_FLAGS));
  if (!nssMod) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(nssMod.get());
  module.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11ModuleDB::FindModuleByName(const char16_t* aName,
                                   nsIPKCS11Module** _retval)
{
  // Note: It's OK for |aName| to be null.
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ConvertUTF16toUTF8 utf8Name(aName);
  UniqueSECMODModule mod(SECMOD_FindModule(const_cast<char*>(utf8Name.get())));
  if (!mod) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(mod.get());
  module.forget(_retval);
  return NS_OK;
}

/* This is essentially the same as nsIPK11Token::findTokenByName, except
 * that it returns an nsIPKCS11Slot, which may be desired.
 */
NS_IMETHODIMP
nsPKCS11ModuleDB::FindSlotByName(const char16_t* aName,
                                 nsIPKCS11Slot** _retval)
{
  // Note: It's OK for |aName| to be null.
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ConvertUTF16toUTF8 utf8Name(aName);
  UniquePK11SlotInfo slotInfo(
    PK11_FindSlotByName(const_cast<char*>(utf8Name.get())));
  if (!slotInfo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPKCS11Slot> slot = new nsPKCS11Slot(slotInfo.get());
  slot.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11ModuleDB::ListModules(nsISimpleEnumerator** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  /* lock down the list for reading */
  AutoSECMODListReadLock lock;
  for (SECMODModuleList* list = SECMOD_GetDefaultModuleList(); list;
       list = list->next) {
    nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(list->module);
    nsresult rv = array->AppendElement(module, false);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  /* Get the modules in the database that didn't load */
  for (SECMODModuleList* list = SECMOD_GetDeadModuleList(); list;
       list = list->next) {
    nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(list->module);
    nsresult rv = array->AppendElement(module, false);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return array->Enumerate(_retval);
}

NS_IMETHODIMP
nsPKCS11ModuleDB::GetCanToggleFIPS(bool* aCanToggleFIPS)
{
  NS_ENSURE_ARG_POINTER(aCanToggleFIPS);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aCanToggleFIPS = SECMOD_CanDeleteInternalModule();
  return NS_OK;
}


NS_IMETHODIMP
nsPKCS11ModuleDB::ToggleFIPSMode()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The way to toggle FIPS mode in NSS is extremely obscure. Basically, we
  // delete the internal module, and it gets replaced with the opposite module
  // (i.e. if it was FIPS before, then it becomes non-FIPS next).
  // SECMOD_GetInternalModule() returns a pointer to a local copy of the
  // internal module stashed in NSS.  We don't want to delete it since it will
  // cause much pain in NSS.
  SECMODModule* internal = SECMOD_GetInternalModule();
  if (!internal) {
    return NS_ERROR_FAILURE;
  }

  if (SECMOD_DeleteInternalModule(internal->commonName) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if (PK11_IsFIPS()) {
    Telemetry::Accumulate(Telemetry::FIPS_ENABLED, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11ModuleDB::GetIsFIPSEnabled(bool* aIsFIPSEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsFIPSEnabled);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aIsFIPSEnabled = PK11_IsFIPS();
  return NS_OK;
}

NS_IMETHODIMP
nsPKCS11ModuleDB::GetIsFIPSModeActive(bool* aIsFIPSModeActive)
{
  return GetIsFIPSEnabled(aIsFIPSModeActive);
}
