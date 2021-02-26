/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsErrorService.h"
#include "nsCRTGlue.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"

namespace {

mozilla::StaticRefPtr<nsErrorService> gSingleton;

}

NS_IMPL_ISUPPORTS(nsErrorService, nsIErrorService)

// static
already_AddRefed<nsIErrorService> nsErrorService::GetOrCreate() {
  // Be careful to not recreate the service for a second time if GetOrCreate is
  // called super late during shutdown.
  static bool serviceCreated = false;
  RefPtr<nsErrorService> svc;
  if (gSingleton) {
    svc = gSingleton;
  } else if (!serviceCreated) {
    gSingleton = new nsErrorService();
    mozilla::ClearOnShutdown(&gSingleton);
    svc = gSingleton;
    serviceCreated = true;
  }

  return svc.forget();
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundle(int16_t aErrorModule,
                                          const char* aStringBundleURL) {
  mErrorStringBundleURLMap.InsertOrUpdate(
      aErrorModule, MakeUnique<nsCString>(aStringBundleURL));
  return NS_OK;
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundle(int16_t aErrorModule) {
  mErrorStringBundleURLMap.Remove(aErrorModule);
  return NS_OK;
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundle(int16_t aErrorModule, char** aResult) {
  nsCString* bundleURL = mErrorStringBundleURLMap.Get(aErrorModule);
  if (!bundleURL) {
    return NS_ERROR_FAILURE;
  }
  *aResult = ToNewCString(*bundleURL);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
