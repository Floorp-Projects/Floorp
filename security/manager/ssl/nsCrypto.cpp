/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCrypto.h"

#include "nsNSSComponent.h"
#include "nsNativeCharsetUtils.h"
#include "nsServiceManagerUtils.h"
#include "ScopedNSSTypes.h"

// QueryInterface implementation for nsPkcs11
NS_INTERFACE_MAP_BEGIN(nsPkcs11)
  NS_INTERFACE_MAP_ENTRY(nsIPKCS11)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPkcs11)
NS_IMPL_RELEASE(nsPkcs11)

nsPkcs11::nsPkcs11()
{
}

nsPkcs11::~nsPkcs11()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}

// Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
nsPkcs11::DeleteModule(const nsAString& aModuleName)
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
    mozilla::UniqueSECMODModule module(SECMOD_FindModule(moduleName.get()));
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

// Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
nsPkcs11::AddModule(const nsAString& aModuleName,
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

#ifndef MOZ_NO_SMART_CARDS
  mozilla::UniqueSECMODModule module(SECMOD_FindModule(moduleName.get()));
  if (!module) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsINSSComponent> nssComponent(
    do_GetService(PSM_COMPONENT_CONTRACTID));
  nssComponent->LaunchSmartCardThread(module.get());
#endif

  return NS_OK;
}
