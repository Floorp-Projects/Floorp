// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_MMAP_H_
#define LIB_EXTRAS_MMAP_H_

#include <memory>

#include "lib/jxl/base/status.h"

namespace jxl {
struct MemoryMappedFileImpl;

class MemoryMappedFile {
 public:
  static StatusOr<MemoryMappedFile> Init(const char* path);
  const uint8_t* data() const;
  size_t size() const;
  MemoryMappedFile();
  ~MemoryMappedFile();
  MemoryMappedFile(MemoryMappedFile&&) noexcept;
  MemoryMappedFile& operator=(MemoryMappedFile&&) noexcept;

 private:
  std::unique_ptr<MemoryMappedFileImpl> impl_;
};
}  // namespace jxl

#endif
