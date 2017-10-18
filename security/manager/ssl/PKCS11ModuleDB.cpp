/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PKCS11ModuleDB.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Telemetry.h"
#include "nsCRTGlue.h"
#include "nsIMutableArray.h"
#include "nsNSSComponent.h"
#include "nsNativeCharsetUtils.h"
#include "nsPKCS11Slot.h"
#include "nsServiceManagerUtils.h"

namespace mozilla { namespace psm {

NS_IMPL_ISUPPORTS(PKCS11ModuleDB, nsIPKCS11ModuleDB)

PKCS11ModuleDB::PKCS11ModuleDB()
{
}

PKCS11ModuleDB::~PKCS11ModuleDB()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}

// Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
PKCS11ModuleDB::DeleteModule(const nsAString& aModuleName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aModuleName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ConvertUTF16toUTF8 moduleName(aModuleName);
  // Introduce additional scope for module so all references to it are released
  // before we call SECMOD_DeleteModule, below.
#ifndef MOZ_NO_SMART_CARDS
  {
    UniqueSECMODModule module(SECMOD_FindModule(moduleName.get()));
    if (!module) {
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsINSSComponent> nssComponent(
      do_GetService(PSM_COMPONENT_CONTRACTID));
    nssComponent->ShutdownSmartCardThread(module.get());
  }
#endif

  // modType is an output variable. We ignore it.
  int32_t modType;
  SECStatus srv = SECMOD_DeleteModule(moduleName.get(), &modType);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// Given a PKCS#11 module, determines an appropriate name to identify it for the
// purposes of gathering telemetry. For 3rd party PKCS#11 modules, this should
// be the name of the dynamic library that implements the module. For built-in
// NSS modules, it will be the common name of the module.
// Because the result will be used as a telemetry scalar key (which must be less
// than 70 characters), this function will also truncate the result if it
// exceeds this limit. (Note that unfortunately telemetry doesn't expose a way
// to programmatically query the scalar key length limit, so we have to
// hard-code the value here.)
void
GetModuleNameForTelemetry(/*in*/ const SECMODModule* module,
                          /*out*/nsString& result)
{
  result.Truncate();
  if (module->dllName) {
    CopyASCIItoUTF16(module->dllName, result);
    int32_t separatorIndex = result.RFind(FILE_PATH_SEPARATOR);
    if (separatorIndex != kNotFound) {
      result = Substring(result, separatorIndex + 1);
    }
  } else {
    CopyASCIItoUTF16(module->commonName, result);
  }
  if (result.Length() >= 70) {
    result.Truncate(69);
  }
}

// Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
PKCS11ModuleDB::AddModule(const nsAString& aModuleName,
                          const nsAString& aLibraryFullPath,
                          int32_t aCryptoMechanismFlags,
                          int32_t aCipherFlags)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aModuleName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // "Root Certs" is the name some NSS command-line utilities will give the
  // roots module if they decide to load it when there happens to be a
  // `DLL_PREFIX "nssckbi" DLL_SUFFIX` file in the directory being operated on.
  // This causes failures, so as a workaround, the PSM initialization code will
  // unconditionally remove any module named "Root Certs". We should prevent the
  // user from adding an unrelated module named "Root Certs" in the first place
  // so PSM doesn't delete it. See bug 1406396.
  if (aModuleName.EqualsLiteral("Root Certs")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // There appears to be a deadlock if we try to load modules concurrently, so
  // just wait until the loadable roots module has been loaded.
  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ConvertUTF16toUTF8 moduleName(aModuleName);
  nsCString fullPath;
  // NSS doesn't support Unicode path.  Use native charset
  NS_CopyUnicodeToNative(aLibraryFullPath, fullPath);
  uint32_t mechFlags = SECMOD_PubMechFlagstoInternal(aCryptoMechanismFlags);
  uint32_t cipherFlags = SECMOD_PubCipherFlagstoInternal(aCipherFlags);
  SECStatus srv = SECMOD_AddNewModule(moduleName.get(), fullPath.get(),
                                      mechFlags, cipherFlags);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  UniqueSECMODModule module(SECMOD_FindModule(moduleName.get()));
  if (!module) {
    return NS_ERROR_FAILURE;
  }

#ifndef MOZ_NO_SMART_CARDS
  nsCOMPtr<nsINSSComponent> nssComponent(
    do_GetService(PSM_COMPONENT_CONTRACTID));
  nssComponent->LaunchSmartCardThread(module.get());
#endif

  nsAutoString scalarKey;
  GetModuleNameForTelemetry(module.get(), scalarKey);
  // Scalar keys must be between 0 and 70 characters (exclusive).
  // GetModuleNameForTelemetry takes care of keys that are too long.
  // If for some reason it couldn't come up with an appropriate name and
  // returned an empty result, however, we need to not attempt to record this
  // (it wouldn't give us anything useful anyway).
  if (scalarKey.Length() > 0) {
    Telemetry::ScalarSet(Telemetry::ScalarID::SECURITY_PKCS11_MODULES_LOADED,
                         scalarKey, true);
  }
  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::FindModuleByName(const nsACString& name,
                         /*out*/ nsIPKCS11Module** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  UniqueSECMODModule mod(SECMOD_FindModule(PromiseFlatCString(name).get()));
  if (!mod) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(mod.get());
  module.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::ListModules(nsISimpleEnumerator** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
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
PKCS11ModuleDB::GetCanToggleFIPS(bool* aCanToggleFIPS)
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
PKCS11ModuleDB::ToggleFIPSMode()
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
PKCS11ModuleDB::GetIsFIPSEnabled(bool* aIsFIPSEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsFIPSEnabled);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aIsFIPSEnabled = PK11_IsFIPS();
  return NS_OK;
}

} } // namespace mozilla::psm
