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

#ifndef LIB_JXL_TESTDATA_H_
#define LIB_JXL_TESTDATA_H_

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <string>

#include "lib/jxl/base/file_io.h"

namespace jxl {

static inline PaddedBytes ReadTestData(const std::string& filename) {
  std::string full_path = std::string(TEST_DATA_PATH "/") + filename;
  PaddedBytes data;
  bool ok = ReadFile(full_path, &data);
#ifdef __EMSCRIPTEN__
  // Fallback in case FS is not supported in current JS engine.
  if (!ok) {
    // {size_t size, uint8_t* bytes} pair.
    uint32_t size_bytes[2] = {0, 0};
    EM_ASM(
        {
          let buffer = null;
          try {
            buffer = readbuffer(UTF8ToString($0));
          } catch {
          }
          if (!buffer) return;
          let bytes = new Uint8Array(buffer);
          let size = bytes.length;
          let out = _malloc(size);
          if (!out) return;
          HEAP8.set(bytes, out);
          HEAP32[$1 >> 2] = size;
          HEAP32[($1 + 4) >> 2] = out;
        },
        full_path.c_str(), size_bytes);
    size_t size = size_bytes[0];
    uint8_t* bytes = reinterpret_cast<uint8_t*>(size_bytes[1]);
    if (size) {
      data.append(bytes, bytes + size);
      free(reinterpret_cast<void*>(bytes));
      ok = true;
    }
  }
#endif
  JXL_CHECK(ok);
  return data;
}

}  // namespace jxl

#endif  // LIB_JXL_TESTDATA_H_
