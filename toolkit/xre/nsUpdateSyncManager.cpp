/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUpdateSyncManager.h"

#include "mozilla/Unused.h"
#include "mozilla/Services.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIProperties.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

#define UPDATE_SEMAPHORE_NAME_TOKEN "MozillaUpdateSemaphore"

nsUpdateSyncManager* gUpdateSyncManager = nullptr;

NS_IMPL_ISUPPORTS(nsUpdateSyncManager, nsIUpdateSyncManager, nsIObserver)

nsUpdateSyncManager::nsUpdateSyncManager() { OpenSemaphore(); }

nsUpdateSyncManager::~nsUpdateSyncManager() {
  ReleaseSemaphore();
  gUpdateSyncManager = nullptr;
}

already_AddRefed<nsUpdateSyncManager> nsUpdateSyncManager::GetSingleton() {
  if (!gUpdateSyncManager) {
    gUpdateSyncManager = new nsUpdateSyncManager();
  }
  return do_AddRef(gUpdateSyncManager);
}

NS_IMETHODIMP nsUpdateSyncManager::Observe(nsISupports* aSubject,
                                           const char* aTopic,
                                           const char16_t* aData) {
  mozilla::Unused << aSubject;
  mozilla::Unused << aData;

  // We want to hold the semaphore for as much of the lifetime of the app
  // as we can, so we observe xpcom-startup so we get constructed as early as
  // possible, which triggers constructing the singleton.
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_STARTUP_OBSERVER_ID)) {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      return observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                          false);
    }
    return NS_ERROR_SERVICE_NOT_AVAILABLE;
  }
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    ReleaseSemaphore();
  }

  return NS_OK;
}

nsresult nsUpdateSyncManager::OpenSemaphore() {
  if (mSemaphore) {
    // Semaphore is already open.
    return NS_OK;
  }

  // Only open the semaphore from the browser process.
  // Our component registration should already have made sure of this.
  if (NS_WARN_IF(XRE_GetProcessType() != GeckoProcessType_Default)) {
    return NS_OK;
  }

  nsCOMPtr<nsIProperties> dirSvc =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(dirSvc, NS_ERROR_SERVICE_NOT_AVAILABLE);

  nsCOMPtr<nsIFile> appFile;
  nsresult rv = dirSvc->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                            getter_AddRefs(appFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> appDirFile;
  rv = appFile->GetParent(getter_AddRefs(appDirFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString appDirPath;
  rv = appDirFile->GetPath(appDirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  mSemaphore = mozilla::OpenGlobalSemaphore(
      UPDATE_SEMAPHORE_NAME_TOKEN, PromiseFlatString(appDirPath).get());
  NS_ENSURE_TRUE(mSemaphore, NS_ERROR_FAILURE);

  return NS_OK;
}

void nsUpdateSyncManager::ReleaseSemaphore() {
  if (!mSemaphore) {
    // Semaphore is already released.
    return;
  }

  mozilla::ReleaseGlobalSemaphore(mSemaphore);
  mSemaphore = nullptr;
}

NS_IMETHODIMP nsUpdateSyncManager::IsOtherInstanceRunning(bool* aResult) {
  if (NS_WARN_IF(XRE_GetProcessType() != GeckoProcessType_Default)) {
    return NS_ERROR_SERVICE_NOT_AVAILABLE;
  }

  if (!mSemaphore) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool rv = mozilla::IsOtherInstanceRunning(mSemaphore, aResult);
  NS_ENSURE_TRUE(rv, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsUpdateSyncManager::ResetSemaphore() {
  ReleaseSemaphore();
  return OpenSemaphore();
}
