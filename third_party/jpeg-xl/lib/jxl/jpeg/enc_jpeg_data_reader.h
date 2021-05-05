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

// Functions for reading a jpeg byte stream into a JPEGData object.

#ifndef LIB_JXL_JPEG_ENC_JPEG_DATA_READER_H_
#define LIB_JXL_JPEG_ENC_JPEG_DATA_READER_H_

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/jpeg/jpeg_data.h"

namespace jxl {
namespace jpeg {

enum class JpegReadMode {
  kReadHeader,  // only basic headers
  kReadTables,  // headers and tables (quant, Huffman, ...)
  kReadAll,     // everything
};

// Parses the JPEG stream contained in data[*pos ... len) and fills in *jpg with
// the parsed information.
// If mode is kReadHeader, it fills in only the image dimensions in *jpg.
// Returns false if the data is not valid JPEG, or if it contains an unsupported
// JPEG feature.
bool ReadJpeg(const uint8_t* data, const size_t len, JpegReadMode mode,
              JPEGData* jpg);

}  // namespace jpeg
}  // namespace jxl

#endif  // LIB_JXL_JPEG_ENC_JPEG_DATA_READER_H_
