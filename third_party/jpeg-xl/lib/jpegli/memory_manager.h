// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_MEMORY_MANAGER_H_
#define LIB_JPEGLI_MEMORY_MANAGER_H_

/* clang-format off */
#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
/* clang-format on */

#include <vector>

namespace jpegli {

struct MemoryManager {
  struct jpeg_memory_mgr pub;
  std::vector<void*> owned_ptrs;
};

template <typename T>
T* Allocate(j_common_ptr cinfo, size_t len) {
  T* p = reinterpret_cast<T*>(malloc(len * sizeof(T)));
  auto mem = reinterpret_cast<jpegli::MemoryManager*>(cinfo->mem);
  mem->owned_ptrs.push_back(p);
  return p;
}

template <typename T>
T* Allocate(j_decompress_ptr cinfo, size_t len) {
  return Allocate<T>(reinterpret_cast<j_common_ptr>(cinfo), len);
}

template <typename T>
T* Allocate(j_compress_ptr cinfo, size_t len) {
  return Allocate<T>(reinterpret_cast<j_common_ptr>(cinfo), len);
}

}  // namespace jpegli

#endif  // LIB_JPEGLI_MEMORY_MANAGER_H_
