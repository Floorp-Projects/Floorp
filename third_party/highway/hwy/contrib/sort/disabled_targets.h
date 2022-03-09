// Copyright 2022 Google LLC
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

// Speed up MSVC builds by building fewer targets. This header must be included
// from all TUs that contain a HWY_DYNAMIC_DISPATCH to vqsort, i.e. vqsort_*.cc.
// However, users of vqsort.h are unaffected.

#ifndef HIGHWAY_HWY_CONTRIB_SORT_DISABLED_TARGETS_H_
#define HIGHWAY_HWY_CONTRIB_SORT_DISABLED_TARGETS_H_

#include "hwy/base.h"

#if HWY_COMPILER_MSVC
#undef HWY_DISABLED_TARGETS
// HWY_SCALAR remains, so there will still be a valid target to call.
#define HWY_DISABLED_TARGETS (HWY_SSSE3 | HWY_SSE4)
#endif  // HWY_COMPILER_MSVC

#endif  // HIGHWAY_HWY_CONTRIB_SORT_DISABLED_TARGETS_H_
