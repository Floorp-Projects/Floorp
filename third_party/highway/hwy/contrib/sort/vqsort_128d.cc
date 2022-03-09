// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/contrib/sort/disabled_targets.h"
#include "hwy/contrib/sort/vqsort.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort_128d.cc"
#include "hwy/foreach_target.h"

// After foreach_target
#include "hwy/contrib/sort/traits128-inl.h"
#include "hwy/contrib/sort/vqsort-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void Sort128Desc(uint64_t* HWY_RESTRICT keys, size_t num,
                 uint64_t* HWY_RESTRICT buf) {
  SortTag<uint64_t> d;
  detail::SharedTraits<detail::Traits128<detail::OrderDescending128>> st;
  Sort(d, st, keys, num, buf);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(Sort128Desc);
}  // namespace

void Sorter::operator()(uint128_t* HWY_RESTRICT keys, size_t n,
                        SortDescending) const {
  HWY_DYNAMIC_DISPATCH(Sort128Desc)
  (reinterpret_cast<uint64_t*>(keys), n * 2, Get<uint64_t>());
}

}  // namespace hwy
#endif  // HWY_ONCE
