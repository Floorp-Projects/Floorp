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

#ifndef LIB_EXTRAS_TONE_MAPPING_H_
#define LIB_EXTRAS_TONE_MAPPING_H_

#include "lib/jxl/codec_in_out.h"

namespace jxl {

Status ToneMapTo(std::pair<float, float> display_nits, CodecInOut* io,
                 ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_EXTRAS_TONE_MAPPING_H_
