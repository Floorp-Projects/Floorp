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

#ifndef LIB_JXL_JPEG_DEC_JPEG_OUTPUT_CHUNK_H_
#define LIB_JXL_JPEG_DEC_JPEG_OUTPUT_CHUNK_H_

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <vector>

namespace jxl {
namespace jpeg {

/**
 * A chunk of output data.
 *
 * Data producer creates OutputChunks and adds them to the end output queue.
 * Once control flow leaves the producer code, it is considered that chunk of
 * data is final and can not be changed; to underline this fact |next| is a
 * const-pointer.
 *
 * Data consumer removes OutputChunks from the beginning of the output queue.
 * It is possible to consume OutputChunks partially, by updating |next| and
 * |len|.
 *
 * There are 2 types of output chunks:
 *  - owning: actual data is stored in |buffer| field; producer fills data after
 *    the instance it created; it is legal to reduce |len| to show that not all
 *    the capacity of |buffer| is used
 *  - non-owning: represents the data stored (owned) somewhere else
 */
struct OutputChunk {
  // Non-owning
  template <typename Bytes>
  explicit OutputChunk(Bytes& bytes) : len(bytes.size()) {
    // Deal both with const qualifier and data type.
    const void* src = bytes.data();
    next = reinterpret_cast<const uint8_t*>(src);
  }

  // Non-owning
  OutputChunk(const uint8_t* data, size_t size) : next(data), len(size) {}

  // Owning
  explicit OutputChunk(size_t size = 0) {
    buffer.reset(new std::vector<uint8_t>(size));
    next = buffer->data();
    len = size;
  }

  // Owning
  OutputChunk(std::initializer_list<uint8_t> bytes) {
    buffer.reset(new std::vector<uint8_t>(bytes));
    next = buffer->data();
    len = bytes.size();
  }

  const uint8_t* next;
  size_t len;
  // TODO(veluca): consider removing the unique_ptr.
  std::unique_ptr<std::vector<uint8_t>> buffer;
};

}  // namespace jpeg
}  // namespace jxl

#endif  // LIB_JXL_JPEG_DEC_JPEG_OUTPUT_CHUNK_H_
