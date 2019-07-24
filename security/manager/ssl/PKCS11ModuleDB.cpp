/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PKCS11ModuleDB.h"

#include "ScopedNSSTypes.h"
#include "nsIMutableArray.h"
#include "nsNSSCertHelper.h"
#include "nsNSSComponent.h"
#include "nsNativeCharsetUtils.h"
#include "nsPKCS11Slot.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace psm {

NS_IMPL_ISUPPORTS(PKCS11ModuleDB, nsIPKCS11ModuleDB)

// Convert the UTF16 name of the module as it appears to the user to the
// internal representation. For most modules this just involves converting from
// UTF16 to UTF8. For the builtin root module, it also involves mapping from the
// localized name to the internal, non-localized name.
static nsresult NormalizeModuleNameIn(const nsAString& moduleNameIn,
                                      nsCString& moduleNameOut) {
  nsAutoString localizedRootModuleName;
  nsresult rv =
      GetPIPNSSBundleString("RootCertModuleName", localizedRootModuleName);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (moduleNameIn.Equals(localizedRootModuleName)) {
    moduleNameOut.Assign(kRootModuleName);
    return NS_OK;
  }
  moduleNameOut.Assign(NS_ConvertUTF16toUTF8(moduleNameIn));
  return NS_OK;
}

// Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
PKCS11ModuleDB::DeleteModule(const nsAString& aModuleName) {
  if (aModuleName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString moduleNameNormalized;
  nsresult rv = NormalizeModuleNameIn(aModuleName, moduleNameNormalized);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // modType is an output variable. We ignore it.
  int32_t modType;
  SECStatus srv = SECMOD_DeleteModule(moduleNameNormalized.get(), &modType);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
PKCS11ModuleDB::AddModule(const nsAString& aModuleName,
                          const nsAString& aLibraryFullPath,
                          int32_t aCryptoMechanismFlags, int32_t aCipherFlags) {
  if (aModuleName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // "Root Certs" is the name some NSS command-line utilities will give the
  // roots module if they decide to load it when there happens to be a
  // `MOZ_DLL_PREFIX "nssckbi" MOZ_DLL_SUFFIX` file in the directory being
  // operated on.  This causes failures, so as a workaround, the PSM
  // initialization code will unconditionally remove any module named "Root
  // Certs". We should prevent the user from adding an unrelated module named
  // "Root Certs" in the first place so PSM doesn't delete it. See bug 1406396.
  if (aModuleName.EqualsLiteral("Root Certs")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // There appears to be a deadlock if we try to load modules concurrently, so
  // just wait until the loadable roots module has been loaded.
  nsresult rv = BlockUntilLoadableRootsLoaded();
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString moduleNameNormalized;
  rv = NormalizeModuleNameIn(aModuleName, moduleNameNormalized);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCString fullPath;
  // NSS doesn't support Unicode path.  Use native charset
  NS_CopyUnicodeToNative(aLibraryFullPath, fullPath);
  uint32_t mechFlags = SECMOD_PubMechFlagstoInternal(aCryptoMechanismFlags);
  uint32_t cipherFlags = SECMOD_PubCipherFlagstoInternal(aCipherFlags);
  SECStatus srv = SECMOD_AddNewModule(moduleNameNormalized.get(),
                                      fullPath.get(), mechFlags, cipherFlags);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::ListModules(nsISimpleEnumerator** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

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
    nsresult rv = array->AppendElement(module);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  /* Get the modules in the database that didn't load */
  for (SECMODModuleList* list = SECMOD_GetDeadModuleList(); list;
       list = list->next) {
    nsCOMPtr<nsIPKCS11Module> module = new nsPKCS11Module(list->module);
    nsresult rv = array->AppendElement(module);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return array->Enumerate(_retval, NS_GET_IID(nsIPKCS11Module));
}

NS_IMETHODIMP
PKCS11ModuleDB::GetCanToggleFIPS(bool* aCanToggleFIPS) {
  NS_ENSURE_ARG_POINTER(aCanToggleFIPS);

  *aCanToggleFIPS = SECMOD_CanDeleteInternalModule();
  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::ToggleFIPSMode() {
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

  return NS_OK;
}

NS_IMETHODIMP
PKCS11ModuleDB::GetIsFIPSEnabled(bool* aIsFIPSEnabled) {
  NS_ENSURE_ARG_POINTER(aIsFIPSEnabled);

  *aIsFIPSEnabled = PK11_IsFIPS();
  return NS_OK;
}

}  // namespace psm
}  // namespace mozilla
