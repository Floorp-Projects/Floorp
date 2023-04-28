/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CachePurgeLock.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsAppRunner.h"
#include "mozilla/MultiInstanceLock.h"
#include "nsLocalFile.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(CachePurgeLock, nsICachePurgeLock)

static nsresult PrepareLockArguments(const nsACString& profileName,
                                     nsCString& lockName,
                                     nsString& appDirPath) {
  nsresult rv;

  nsCOMPtr<nsIFile> appFile = mozilla::GetNormalizedAppFile(nullptr);
  if (!appFile) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIFile> appDirFile;
  rv = appFile->GetParent(getter_AddRefs(appDirFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appDirFile->GetPath(appDirPath);
  NS_ENSURE_SUCCESS(rv, rv);

  lockName = profileName;
  lockName.Append("-cachePurge");

  return NS_OK;
}

NS_IMETHODIMP
CachePurgeLock::GetLockFile(const nsACString& profileName, nsIFile** aResult) {
  nsresult rv;
  nsCString lockName;
  nsString appDirPath;
  rv = PrepareLockArguments(profileName, lockName, appDirPath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString filePath;
  if (!GetMultiInstanceLockFileName(lockName.get(), appDirPath.get(),
                                    filePath)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIFile> lockFile = new nsLocalFile();
  rv = lockFile->InitWithNativePath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  lockFile.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
CachePurgeLock::Lock(const nsACString& profileName) {
  nsresult rv;
  if (mLock != MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    // Lock is already open.
    return NS_OK;
  }

  nsCString lockName;
  nsString appDirPath;
  rv = PrepareLockArguments(profileName, lockName, appDirPath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mLock = mozilla::OpenMultiInstanceLock(lockName.get(), appDirPath.get());
  if (mLock == MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
CachePurgeLock::IsOtherInstanceRunning(bool* aResult) {
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

NS_IMETHODIMP
CachePurgeLock::Unlock() {
  if (mLock == MULTI_INSTANCE_LOCK_HANDLE_ERROR) {
    // Lock is already released.
    return NS_OK;
  }

  mozilla::ReleaseMultiInstanceLock(mLock);
  mLock = MULTI_INSTANCE_LOCK_HANDLE_ERROR;

  return NS_OK;
}

}  // namespace mozilla::net
