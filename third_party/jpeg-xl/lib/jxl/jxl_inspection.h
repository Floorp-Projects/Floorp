// Copyright (c) the JPEG XL Project
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

#ifndef LIB_JXL_JXL_INSPECTION_H_
#define LIB_JXL_JXL_INSPECTION_H_

#include <functional>

#include "lib/jxl/image.h"

namespace jxl {
// Type of the inspection-callback which, if enabled, will be called on various
// intermediate data during image processing, allowing inspection access.
//
// Returns false if processing can be stopped at that point, true otherwise.
// This is only advisory - it is always OK to just continue processing.
using InspectorImage3F = std::function<bool(const char*, const Image3F&)>;
}  // namespace jxl

#endif  // LIB_JXL_JXL_INSPECTION_H_
