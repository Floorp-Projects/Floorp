// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/test_memory_manager.h"

#include <jxl/memory_manager.h>

#include <cstdlib>

namespace jxl {
namespace test {

namespace {
void* TestAlloc(void* /* opaque*/, size_t size) { return malloc(size); }
void TestFree(void* /* opaque*/, void* address) { free(address); }
JxlMemoryManager kMemoryManager{nullptr, &TestAlloc, &TestFree};
}  // namespace

JxlMemoryManager* MemoryManager() { return &kMemoryManager; };

}  // namespace test
}  // namespace jxl
