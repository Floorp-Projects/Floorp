/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"

#include "CascadeFilter.h"

namespace {
extern "C" {

// Implemented in Rust.
void cascade_filter_construct(nsICascadeFilter** aResult);
}
}  // namespace

namespace mozilla {

already_AddRefed<nsICascadeFilter> ConstructCascadeFilter() {
  nsCOMPtr<nsICascadeFilter> filter;
  cascade_filter_construct(getter_AddRefs(filter));
  return filter.forget();
}

}  // namespace mozilla
