// Copyright 2022 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

#include "hwy/per_target.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/per_target.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
// On SVE, Lanes rounds down to a power of two, but we want to know the actual
// size here. Otherwise, hypothetical SVE with 48 bytes would round down to 32
// and we'd enable HWY_SVE_256, and then fail reverse_test because Reverse on
// HWY_SVE_256 requires the actual vector to be a power of two.
#if HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2 || HWY_TARGET == HWY_SVE_256
size_t GetVectorBytes() { return detail::AllHardwareLanes(hwy::SizeTag<1>()); }
#else
size_t GetVectorBytes() { return Lanes(ScalableTag<uint8_t>()); }
#endif
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE

}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(GetVectorBytes);  // Local function.
}  // namespace

size_t VectorBytes() { return HWY_DYNAMIC_DISPATCH(GetVectorBytes)(); }

}  // namespace hwy
#endif  // HWY_ONCE
