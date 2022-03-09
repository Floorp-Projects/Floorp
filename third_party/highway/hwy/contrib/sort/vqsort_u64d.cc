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
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort_u64d.cc"
#include "hwy/foreach_target.h"

// After foreach_target
#include "hwy/contrib/sort/traits-inl.h"
#include "hwy/contrib/sort/vqsort-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void SortU64Desc(uint64_t* HWY_RESTRICT keys, size_t num,
                 uint64_t* HWY_RESTRICT buf) {
  SortTag<uint64_t> d;
  detail::SharedTraits<detail::LaneTraits<detail::OrderDescending>> st;
  Sort(d, st, keys, num, buf);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(SortU64Desc);
}  // namespace

void Sorter::operator()(uint64_t* HWY_RESTRICT keys, size_t n,
                        SortDescending) const {
  HWY_DYNAMIC_DISPATCH(SortU64Desc)(keys, n, Get<uint64_t>());
}

}  // namespace hwy
#endif  // HWY_ONCE
