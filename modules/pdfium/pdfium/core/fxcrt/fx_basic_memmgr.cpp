// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <stdlib.h>  // For abort().

#include "core/fxcrt/fx_memory.h"

void* FXMEM_DefaultAlloc(size_t byte_size, int flags) {
  return (void*)malloc(byte_size);
}
void* FXMEM_DefaultRealloc(void* pointer, size_t new_size, int flags) {
  return realloc(pointer, new_size);
}
void FXMEM_DefaultFree(void* pointer, int flags) {
  free(pointer);
}

NEVER_INLINE void FX_OutOfMemoryTerminate() {
  // Termimate cleanly if we can, else crash at a specific address (0xbd).
  abort();
#ifndef _WIN32
  reinterpret_cast<void (*)()>(0xbd)();
#endif
}
