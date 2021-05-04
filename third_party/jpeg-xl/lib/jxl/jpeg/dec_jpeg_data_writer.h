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

// Functions for writing a JPEGData object into a jpeg byte stream.

#ifndef LIB_JXL_JPEG_DEC_JPEG_DATA_WRITER_H_
#define LIB_JXL_JPEG_DEC_JPEG_DATA_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>

#include "lib/jxl/jpeg/jpeg_data.h"

namespace jxl {
namespace jpeg {

// Function type used to write len bytes into buf. Returns the number of bytes
// written.
using JPEGOutput = std::function<size_t(const uint8_t* buf, size_t len)>;

Status WriteJpeg(const JPEGData& jpg, const JPEGOutput& out);

}  // namespace jpeg
}  // namespace jxl

#endif  // LIB_JXL_JPEG_DEC_JPEG_DATA_WRITER_H_
