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

#ifdef XP_WIN
#  include "WinUtils.h"
#endif

// The lock code generates a path that already includes the vendor name,
// so this only needs to name the specific lock.
#define UPDATE_LOCK_NAME_TOKEN "UpdateLock"

nsUpdateSyncManager* gUpdateSyncManager = nullptr;

NS_IMPL_ISUPPORTS(nsUpdateSyncManager, nsIUpdateSyncManager, nsIObserver)

nsUpdateSyncManager::nsUpdateSyncManager(nsIFile* anAppFile /* = nullptr */) {
  gUpdateSyncManager = this;
  OpenLock(anAppFile);
}

nsUpdateSyncManager::~nsUpdateSyncManager() {
  ReleaseLock();
  gUpdateSyncManager = nullptr;
}

already_AddRefed<nsUpdateSyncManager> nsUpdateSyncManager::GetSingleton() {
  if (!gUpdateSyncManager) {
    new nsUpdateSyncManager();  // This sets gUpdateSyncManager.
  }
  return do_AddRef(gUpdateSyncManager);
}

NS_IMETHODIMP nsUpdateSyncManager::Observe(nsISupports* aSubject,
                                           const char* aTopic,
                                           const char16_t* aData) {
  mozilla::Unused << aSubject;
  mozilla::Unused << aData;

  // We want to hold the lock for as much of the lifetime of the app as we can,
  // so we observe xpcom-startup so we get constructed as early as possible,
  // which triggers constructing the singleton.
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
    ReleaseLock();
  }

  return NS_OK;
}

nsresult nsUpdateSyncManager::OpenLock(nsIFile* anAppFile) {
  if (mLock != MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    // Lock is already open.
    return NS_OK;
  }

  // Only open the lock from the browser process.
  // Our component registration should already have made sure of this.
  if (NS_WARN_IF(XRE_GetProcessType() != GeckoProcessType_Default)) {
    return NS_OK;
  }

  // If we're given an app file, use it; otherwise, get it from the ambient
  // directory service.
  nsresult rv;
  nsCOMPtr<nsIFile> appFile;
  if (anAppFile) {
    rv = anAppFile->Clone(getter_AddRefs(appFile));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsIProperties> dirSvc =
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(dirSvc, NS_ERROR_SERVICE_NOT_AVAILABLE);

    rv = dirSvc->Get(XRE_EXECUTABLE_FILE, NS_GET_IID(nsIFile),
                     getter_AddRefs(appFile));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // It is possible that the path we have is on a case insensitive
  // filesystem in which case the path may vary depending on how the
  // application is called. We want to normalize the case somehow.
  // On Linux XRE_EXECUTABLE_FILE already seems to be set to the correct path.
  //
  // See similar nsXREDirProvider::GetInstallHash. The main difference here is
  // to allow lookup to fail on OSX, because some tests use a nonexistent
  // appFile.
#ifdef XP_WIN
  // Windows provides a way to get the correct case.
  if (!mozilla::widget::WinUtils::ResolveJunctionPointsAndSymLinks(appFile)) {
    NS_WARNING("Failed to resolve install directory.");
  }
#elif defined(MOZ_WIDGET_COCOA)
  // On OSX roundtripping through an FSRef fixes the case.
  FSRef ref;
  nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(appFile);
  if (macFile && NS_SUCCEEDED(macFile->GetFSRef(&ref)) &&
      NS_SUCCEEDED(
          NS_NewLocalFileWithFSRef(&ref, true, getter_AddRefs(macFile)))) {
    appFile = static_cast<nsIFile*>(macFile);
  } else {
    NS_WARNING("Failed to resolve install directory.");
  }
#endif

  nsCOMPtr<nsIFile> appDirFile;
  rv = appFile->GetParent(getter_AddRefs(appDirFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString appDirPath;
  rv = appDirFile->GetPath(appDirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  mLock = mozilla::OpenMultiInstanceLock(UPDATE_LOCK_NAME_TOKEN,
                                         PromiseFlatString(appDirPath).get());
  NS_ENSURE_TRUE(mLock, NS_ERROR_FAILURE);

  return NS_OK;
}

void nsUpdateSyncManager::ReleaseLock() {
  if (mLock == MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    // Lock is already released.
    return;
  }

  mozilla::ReleaseMultiInstanceLock(mLock);
  mLock = MULTI_INSTANCE_LOCK_HANDLE_ERROR;
}

NS_IMETHODIMP nsUpdateSyncManager::IsOtherInstanceRunning(bool* aResult) {
  if (NS_WARN_IF(XRE_GetProcessType() != GeckoProcessType_Default)) {
    return NS_ERROR_SERVICE_NOT_AVAILABLE;
  }

  if (mLock == MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool rv = mozilla::IsOtherInstanceRunning(mLock, aResult);
  NS_ENSURE_TRUE(rv, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsUpdateSyncManager::ResetLock(nsIFile* anAppFile = nullptr) {
  ReleaseLock();
  return OpenLock(anAppFile);
}
