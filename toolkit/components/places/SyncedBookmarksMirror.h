/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_SyncedBookmarksMirror_h_
#define mozilla_places_SyncedBookmarksMirror_h_

#include "mozISyncedBookmarksMirror.h"
#include "nsCOMPtr.h"

extern "C" {

// Implemented in Rust, in the `bookmark_sync` crate.
void NS_NewSyncedBookmarksMerger(mozISyncedBookmarksMerger** aResult);

}  // extern "C"

namespace mozilla {
namespace places {

already_AddRefed<mozISyncedBookmarksMerger> NewSyncedBookmarksMerger() {
  nsCOMPtr<mozISyncedBookmarksMerger> merger;
  NS_NewSyncedBookmarksMerger(getter_AddRefs(merger));
  return merger.forget();
}

}  // namespace places
}  // namespace mozilla

#endif  // mozilla_places_SyncedBookmarksMirror_h_
