/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AvailableMemoryWatcher.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsXULAppAPI.h"

namespace mozilla {

StaticRefPtr<nsAvailableMemoryWatcherBase>
    nsAvailableMemoryWatcherBase::sSingleton;

/*static*/
already_AddRefed<nsAvailableMemoryWatcherBase>
nsAvailableMemoryWatcherBase::GetSingleton() {
  if (!sSingleton) {
    sSingleton = CreateAvailableMemoryWatcher();
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

NS_IMPL_ISUPPORTS(nsAvailableMemoryWatcherBase, nsIAvailableMemoryWatcherBase);

nsAvailableMemoryWatcherBase::nsAvailableMemoryWatcherBase() {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Watching memory only in the main process.");
}

// Define the fallback method for a platform for which a platform-specific
// CreateAvailableMemoryWatcher() is not defined.
#if !defined(XP_WIN)
already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher() {
  RefPtr instance(new nsAvailableMemoryWatcherBase);
  return do_AddRef(instance);
}
#endif

}  // namespace mozilla
