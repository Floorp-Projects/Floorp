/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PKCS11.h"

#include "ScopedNSSTypes.h"
#include "mozilla/Telemetry.h"
#include "nsCRTGlue.h"
#include "nsNSSComponent.h"
#include "nsNativeCharsetUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla { namespace psm {

NS_INTERFACE_MAP_BEGIN(PKCS11)
  NS_INTERFACE_MAP_ENTRY(nsIPKCS11)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(PKCS11)
NS_IMPL_RELEASE(PKCS11)

PKCS11::PKCS11()
{
}

PKCS11::~PKCS11()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  shutdown(ShutdownCalledFrom::Object);
}

// Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
PKCS11::DeleteModule(const nsAString& aModuleName)
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
    result.AssignWithConversion(module->dllName);
    int32_t separatorIndex = result.RFind(FILE_PATH_SEPARATOR);
    if (separatorIndex != kNotFound) {
      result = Substring(result, separatorIndex + 1);
    }
  } else {
    result.AssignWithConversion(module->commonName);
  }
  if (result.Length() >= 70) {
    result.Truncate(69);
  }
}

// Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
PKCS11::AddModule(const nsAString& aModuleName,
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

} } // namespace mozilla::psm
