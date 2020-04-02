/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StartupCacheInfo.h"

#include "mozilla/Components.h"
#include "mozilla/RefPtr.h"

using namespace mozilla;
using namespace mozilla::scache;

NS_IMPL_ISUPPORTS(StartupCacheInfo, nsIStartupCacheInfo)

nsresult StartupCacheInfo::GetIgnoreDiskCache(bool* aIgnore) {
  *aIgnore = StartupCache::gIgnoreDiskCache;
  return NS_OK;
}

nsresult StartupCacheInfo::GetFoundDiskCacheOnInit(bool* aFound) {
  *aFound = StartupCache::gFoundDiskCacheOnInit;
  return NS_OK;
}

nsresult StartupCacheInfo::GetWroteToDiskCache(bool* aWrote) {
  if (!StartupCache::gStartupCache) {
    *aWrote = false;
  } else {
    *aWrote = StartupCache::gStartupCache->mWrittenOnce;
  }
  return NS_OK;
}

nsresult StartupCacheInfo::GetDiskCachePath(nsAString& aResult) {
  if (!StartupCache::gStartupCache || !StartupCache::gStartupCache->mFile) {
    return NS_OK;
  }
  nsAutoString path;
  StartupCache::gStartupCache->mFile->GetPath(path);
  aResult.Assign(path);
  return NS_OK;
}

NS_IMPL_COMPONENT_FACTORY(nsIStartupCacheInfo) {
  return MakeAndAddRef<StartupCacheInfo>().downcast<nsISupports>();
}
