/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_storage_ExtensionStorageComponents_h_
#define mozilla_extensions_storage_ExtensionStorageComponents_h_

#include "mozIExtensionStorageArea.h"
#include "nsCOMPtr.h"

extern "C" {

// Implemented in Rust, in the `webext_storage_bridge` crate.
nsresult NS_NewExtensionStorageSyncArea(
    mozIConfigurableExtensionStorageArea** aResult);

}  // extern "C"

namespace mozilla {
namespace extensions {
namespace storage {

// The C++ constructor for a `storage.sync` area. This wrapper exists because
// `components.conf` requires a component class constructor to return an
// `already_AddRefed<T>`, but Rust doesn't have such a type. So we call the
// Rust constructor using a `nsCOMPtr` (which is compatible with Rust's
// `xpcom::RefPtr`) out param, and return that.
already_AddRefed<mozIConfigurableExtensionStorageArea> NewSyncArea() {
  nsCOMPtr<mozIConfigurableExtensionStorageArea> storage;
  nsresult rv = NS_NewExtensionStorageSyncArea(getter_AddRefs(storage));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return storage.forget();
}

}  // namespace storage
}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_storage_ExtensionStorageComponents_h_
