/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataStorageManager.h"
#include "MainThreadUtils.h"
#include "nsIMemoryReporter.h"
#include "nsString.h"

using VoidPtrToSizeFn = uintptr_t (*)(const void* ptr);

extern "C" nsresult make_data_storage(const nsAString* basename,
                                      size_t valueLength,
                                      VoidPtrToSizeFn sizeOfOp,
                                      VoidPtrToSizeFn enclosingSizeOfOp,
                                      nsIDataStorage** result);

MOZ_DEFINE_MALLOC_SIZE_OF(DataStorageMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(DataStorageMallocEnclosingSizeOf)

namespace mozilla {

NS_IMPL_ISUPPORTS(DataStorageManager, nsIDataStorageManager)

NS_IMETHODIMP
DataStorageManager::Get(nsIDataStorageManager::DataStorage aDataStorage,
                        nsIDataStorage** aResult) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  nsAutoString filename;
  size_t valueLength = 1024;
  switch (aDataStorage) {
    case nsIDataStorageManager::AlternateServices:
      if (mAlternateServicesCreated) {
        return NS_ERROR_ALREADY_INITIALIZED;
      }
      mAlternateServicesCreated = true;
      filename.Assign(u"AlternateServices"_ns);
      break;
    case nsIDataStorageManager::ClientAuthRememberList:
      if (mClientAuthRememberListCreated) {
        return NS_ERROR_ALREADY_INITIALIZED;
      }
      mClientAuthRememberListCreated = true;
      filename.Assign(u"ClientAuthRememberList"_ns);
      break;
    case nsIDataStorageManager::SiteSecurityServiceState:
      if (mSiteSecurityServiceStateCreated) {
        return NS_ERROR_ALREADY_INITIALIZED;
      }
      mSiteSecurityServiceStateCreated = true;
      filename.Assign(u"SiteSecurityServiceState"_ns);
      // For most nsIDataStorage use cases, values can be quite long (1024
      // bytes by default). For HSTS, much less information is stored, so save
      // space by limiting values to 24 bytes.
      valueLength = 24;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return make_data_storage(&filename, valueLength, &DataStorageMallocSizeOf,
                           &DataStorageMallocEnclosingSizeOf, aResult);
}

}  // namespace mozilla
