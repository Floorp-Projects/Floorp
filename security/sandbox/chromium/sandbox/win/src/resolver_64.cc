// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/resolver.h"

#include <stddef.h>

// For placement new. This file must not depend on the CRT at runtime, but
// placement operator new is inline.
#include <new>

#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

const USHORT kMovRax = 0xB848;
const USHORT kJmpRax = 0xe0ff;

#pragma pack(push, 1)
struct InternalThunk {
  // This struct contains roughly the following code:
  // 01 48b8f0debc9a78563412  mov   rax,123456789ABCDEF0h
  // ff e0                    jmp   rax
  //
  // The code modifies rax, but that's fine for x64 ABI.

  InternalThunk() {
    mov_rax = kMovRax;
    jmp_rax = kJmpRax;
    interceptor_function = 0;
  };
  USHORT mov_rax;       // = 48 B8
  ULONG_PTR interceptor_function;
  USHORT jmp_rax;  // = ff e0
};
#pragma pack(pop)

} // namespace.

namespace sandbox {

size_t ResolverThunk::GetInternalThunkSize() const {
  return sizeof(InternalThunk);
}

bool ResolverThunk::SetInternalThunk(void* storage, size_t storage_bytes,
                                     const void* original_function,
                                     const void* interceptor) {
  if (storage_bytes < sizeof(InternalThunk))
    return false;

  InternalThunk* thunk = new(storage) InternalThunk;
  thunk->interceptor_function = reinterpret_cast<ULONG_PTR>(interceptor);

  return true;
}

NTSTATUS ResolverThunk::ResolveTarget(const void* module,
                                      const char* function_name,
                                      void** address) {
  // We don't support sidestep & co.
  return STATUS_NOT_IMPLEMENTED;
}

}  // namespace sandbox
