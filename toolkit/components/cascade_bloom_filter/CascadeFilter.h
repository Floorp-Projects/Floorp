/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CASCADE_BLOOM_FILTER_CASCADE_FILTER_H_
#define CASCADE_BLOOM_FILTER_CASCADE_FILTER_H_

#include "nsICascadeFilter.h"
#include "nsCOMPtr.h"

namespace mozilla {

already_AddRefed<nsICascadeFilter> ConstructCascadeFilter();

}  // namespace mozilla

#endif  // CASCADE_BLOOM_FILTER_CASCADE_FILTER_H_
