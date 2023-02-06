// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/memory_manager.h"

namespace jpegli {

jpeg_memory_mgr* CreateMemoryManager() {
  return reinterpret_cast<struct jpeg_memory_mgr*>(new MemoryManager);
}

void ReleaseMemory(jpeg_memory_mgr* p) {
  if (!p) return;
  MemoryManager* mem = reinterpret_cast<MemoryManager*>(p);
  for (void* ptr : mem->owned_ptrs) {
    free(ptr);
  }
  mem->owned_ptrs.clear();
}

void DestroyMemoryManager(jpeg_memory_mgr* p) {
  if (!p) return;
  ReleaseMemory(p);
  delete reinterpret_cast<MemoryManager*>(p);
}

}  // namespace jpegli
