// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/service_resolver.h"

#include "base/memory/scoped_ptr.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/win_utils.h"

namespace {
#pragma pack(push, 1)

const ULONG kMmovR10EcxMovEax = 0xB8D18B4C;
const USHORT kSyscall = 0x050F;
const BYTE kRetNp = 0xC3;
const ULONG64 kMov1 = 0x54894808244C8948;
const ULONG64 kMov2 = 0x4C182444894C1024;
const ULONG kMov3 = 0x20244C89;

// Service code for 64 bit systems.
struct ServiceEntry {
  // This struct contains roughly the following code:
  // 00 mov     r10,rcx
  // 03 mov     eax,52h
  // 08 syscall
  // 0a ret
  // 0b xchg    ax,ax
  // 0e xchg    ax,ax

  ULONG mov_r10_rcx_mov_eax;  // = 4C 8B D1 B8
  ULONG service_id;
  USHORT syscall;             // = 0F 05
  BYTE ret;                   // = C3
  BYTE pad;                   // = 66
  USHORT xchg_ax_ax1;         // = 66 90
  USHORT xchg_ax_ax2;         // = 66 90
};

// Service code for 64 bit Windows 8.
struct ServiceEntryW8 {
  // This struct contains the following code:
  // 00 48894c2408      mov     [rsp+8], rcx
  // 05 4889542410      mov     [rsp+10], rdx
  // 0a 4c89442418      mov     [rsp+18], r8
  // 0f 4c894c2420      mov     [rsp+20], r9
  // 14 4c8bd1          mov     r10,rcx
  // 17 b825000000      mov     eax,25h
  // 1c 0f05            syscall
  // 1e c3              ret
  // 1f 90              nop

  ULONG64 mov_1;              // = 48 89 4C 24 08 48 89 54
  ULONG64 mov_2;              // = 24 10 4C 89 44 24 18 4C
  ULONG mov_3;                // = 89 4C 24 20
  ULONG mov_r10_rcx_mov_eax;  // = 4C 8B D1 B8
  ULONG service_id;
  USHORT syscall;             // = 0F 05
  BYTE ret;                   // = C3
  BYTE nop;                   // = 90
};

// We don't have an internal thunk for x64.
struct ServiceFullThunk {
  union {
    ServiceEntry original;
    ServiceEntryW8 original_w8;
  };
};

#pragma pack(pop)

bool IsService(const void* source) {
  const ServiceEntry* service =
      reinterpret_cast<const ServiceEntry*>(source);

  return (kMmovR10EcxMovEax == service->mov_r10_rcx_mov_eax &&
          kSyscall == service->syscall && kRetNp == service->ret);
}

};  // namespace

namespace sandbox {

NTSTATUS ServiceResolverThunk::Setup(const void* target_module,
                                     const void* interceptor_module,
                                     const char* target_name,
                                     const char* interceptor_name,
                                     const void* interceptor_entry_point,
                                     void* thunk_storage,
                                     size_t storage_bytes,
                                     size_t* storage_used) {
  NTSTATUS ret = Init(target_module, interceptor_module, target_name,
                      interceptor_name, interceptor_entry_point,
                      thunk_storage, storage_bytes);
  if (!NT_SUCCESS(ret))
    return ret;

  size_t thunk_bytes = GetThunkSize();
  scoped_ptr<char[]> thunk_buffer(new char[thunk_bytes]);
  ServiceFullThunk* thunk = reinterpret_cast<ServiceFullThunk*>(
                                thunk_buffer.get());

  if (!IsFunctionAService(&thunk->original))
    return STATUS_UNSUCCESSFUL;

  ret = PerformPatch(thunk, thunk_storage);

  if (NULL != storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

size_t ServiceResolverThunk::GetThunkSize() const {
  return sizeof(ServiceFullThunk);
}

NTSTATUS ServiceResolverThunk::CopyThunk(const void* target_module,
                                         const char* target_name,
                                         BYTE* thunk_storage,
                                         size_t storage_bytes,
                                         size_t* storage_used) {
  NTSTATUS ret = ResolveTarget(target_module, target_name, &target_);
  if (!NT_SUCCESS(ret))
    return ret;

  size_t thunk_bytes = GetThunkSize();
  if (storage_bytes < thunk_bytes)
    return STATUS_UNSUCCESSFUL;

  ServiceFullThunk* thunk = reinterpret_cast<ServiceFullThunk*>(thunk_storage);

  if (!IsFunctionAService(&thunk->original))
    return STATUS_UNSUCCESSFUL;

  if (NULL != storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

bool ServiceResolverThunk::IsFunctionAService(void* local_thunk) const {
  ServiceFullThunk function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (!IsService(&function_code)) {
    // See if it's the Win8 signature.
    ServiceEntryW8* w8_service = &function_code.original_w8;
    if (!IsService(&w8_service->mov_r10_rcx_mov_eax) ||
        w8_service->mov_1 != kMov1 || w8_service->mov_1 != kMov1 ||
        w8_service->mov_1 != kMov1) {
      return false;
    }
  }

  // Save the verified code.
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

NTSTATUS ServiceResolverThunk::PerformPatch(void* local_thunk,
                                            void* remote_thunk) {
  // Patch the original code.
  ServiceEntry local_service;
  DCHECK_NT(GetInternalThunkSize() >= sizeof(local_service));
  if (!SetInternalThunk(&local_service, sizeof(local_service), NULL,
                        interceptor_))
    return STATUS_UNSUCCESSFUL;

  // Copy the local thunk buffer to the child.
  SIZE_T actual;
  if (!::WriteProcessMemory(process_, remote_thunk, local_thunk,
                            sizeof(ServiceFullThunk), &actual))
    return STATUS_UNSUCCESSFUL;

  if (sizeof(ServiceFullThunk) != actual)
    return STATUS_UNSUCCESSFUL;

  // And now change the function to intercept, on the child.
  if (NULL != ntdll_base_) {
    // Running a unit test.
    if (!::WriteProcessMemory(process_, target_, &local_service,
                              sizeof(local_service), &actual))
      return STATUS_UNSUCCESSFUL;
  } else {
    if (!WriteProtectedChildMemory(process_, target_, &local_service,
                                   sizeof(local_service)))
      return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

bool Wow64ResolverThunk::IsFunctionAService(void* local_thunk) const {
  NOTREACHED_NT();
  return false;
}

}  // namespace sandbox
