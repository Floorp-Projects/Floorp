// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sidestep_resolver.h"

#include "base/win/pe_image.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sidestep/preamble_patcher.h"

namespace {

const size_t kSizeOfSidestepStub = sidestep::kMaxPreambleStubSize;

struct SidestepThunk {
  char sidestep[kSizeOfSidestepStub];  // Storage for the sidestep stub.
  int internal_thunk;  // Dummy member to the beginning of the internal thunk.
};

struct SmartThunk {
  const void* module_base;  // Target module's base.
  const void* interceptor;  // Real interceptor.
  SidestepThunk sidestep;  // Standard sidestep thunk.
};

}  // namespace

namespace sandbox {

NTSTATUS SidestepResolverThunk::Setup(const void* target_module,
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

  SidestepThunk* thunk = reinterpret_cast<SidestepThunk*>(thunk_storage);

  size_t internal_bytes = storage_bytes - kSizeOfSidestepStub;
  if (!SetInternalThunk(&thunk->internal_thunk, internal_bytes, thunk_storage,
                        interceptor_))
    return STATUS_BUFFER_TOO_SMALL;

  AutoProtectMemory memory;
  ret = memory.ChangeProtection(target_, kSizeOfSidestepStub, PAGE_READWRITE);
  if (!NT_SUCCESS(ret))
    return ret;

  sidestep::SideStepError rv = sidestep::PreamblePatcher::Patch(
      target_, reinterpret_cast<void*>(&thunk->internal_thunk), thunk_storage,
      kSizeOfSidestepStub);

  if (sidestep::SIDESTEP_INSUFFICIENT_BUFFER == rv)
    return STATUS_BUFFER_TOO_SMALL;

  if (sidestep::SIDESTEP_SUCCESS != rv)
    return STATUS_UNSUCCESSFUL;

  if (storage_used)
    *storage_used = GetThunkSize();

  return ret;
}

size_t SidestepResolverThunk::GetThunkSize() const {
  return GetInternalThunkSize() + kSizeOfSidestepStub;
}

// This is basically a wrapper around the normal sidestep patch that extends
// the thunk to use a chained interceptor. It uses the fact that
// SetInternalThunk generates the code to pass as the first parameter whatever
// it receives as original_function; we let SidestepResolverThunk set this value
// to its saved code, and then we change it to our thunk data.
NTSTATUS SmartSidestepResolverThunk::Setup(const void* target_module,
                                           const void* interceptor_module,
                                           const char* target_name,
                                           const char* interceptor_name,
                                           const void* interceptor_entry_point,
                                           void* thunk_storage,
                                           size_t storage_bytes,
                                           size_t* storage_used) {
  if (storage_bytes < GetThunkSize())
    return STATUS_BUFFER_TOO_SMALL;

  SmartThunk* thunk = reinterpret_cast<SmartThunk*>(thunk_storage);
  thunk->module_base = target_module;

  NTSTATUS ret;
  if (interceptor_entry_point) {
    thunk->interceptor = interceptor_entry_point;
  } else {
    ret = ResolveInterceptor(interceptor_module, interceptor_name,
                             &thunk->interceptor);
    if (!NT_SUCCESS(ret))
      return ret;
  }

  // Perform a standard sidestep patch on the last part of the thunk, but point
  // to our internal smart interceptor.
  size_t standard_bytes = storage_bytes - offsetof(SmartThunk, sidestep);
  ret = SidestepResolverThunk::Setup(target_module, interceptor_module,
                                     target_name, NULL, &SmartStub,
                                     &thunk->sidestep, standard_bytes, NULL);
  if (!NT_SUCCESS(ret))
    return ret;

  // Fix the internal thunk to pass the whole buffer to the interceptor.
  SetInternalThunk(&thunk->sidestep.internal_thunk, GetInternalThunkSize(),
                   thunk_storage, &SmartStub);

  if (storage_used)
    *storage_used = GetThunkSize();

  return ret;
}

size_t SmartSidestepResolverThunk::GetThunkSize() const {
  return GetInternalThunkSize() + kSizeOfSidestepStub +
         offsetof(SmartThunk, sidestep);
}

// This code must basically either call the intended interceptor or skip the
// call and invoke instead the original function. In any case, we are saving
// the registers that may be trashed by our c++ code.
//
// This function is called with a first parameter inserted by us, that points
// to our SmartThunk. When we call the interceptor we have to replace this
// parameter with the one expected by that function (stored inside our
// structure); on the other hand, when we skip the interceptor we have to remove
// that extra argument before calling the original function.
//
// When we skip the interceptor, the transformation of the stack looks like:
//  On Entry:                         On Use:                     On Exit:
//  [param 2] = first real argument   [param 2] (esp+1c)          [param 2]
//  [param 1] = our SmartThunk        [param 1] (esp+18)          [ret address]
//  [ret address] = real caller       [ret address] (esp+14)      [xxx]
//  [xxx]                             [addr to jump to] (esp+10)  [xxx]
//  [xxx]                             [saved eax]                 [xxx]
//  [xxx]                             [saved ebx]                 [xxx]
//  [xxx]                             [saved ecx]                 [xxx]
//  [xxx]                             [saved edx]                 [xxx]
__declspec(naked)
void SmartSidestepResolverThunk::SmartStub() {
  __asm {
    push eax                  // Space for the jump.
    push eax                  // Save registers.
    push ebx
    push ecx
    push edx
    mov ebx, [esp + 0x18]     // First parameter = SmartThunk.
    mov edx, [esp + 0x14]     // Get the return address.
    mov eax, [ebx]SmartThunk.module_base
    push edx
    push eax
    call SmartSidestepResolverThunk::IsInternalCall
    add esp, 8

    test eax, eax
    lea edx, [ebx]SmartThunk.sidestep   // The original function.
    jz call_interceptor

    // Skip this call
    mov ecx, [esp + 0x14]               // Return address.
    mov [esp + 0x18], ecx               // Remove first parameter.
    mov [esp + 0x10], edx
    pop edx                             // Restore registers.
    pop ecx
    pop ebx
    pop eax
    ret 4                               // Jump to original function.

  call_interceptor:
    mov ecx, [ebx]SmartThunk.interceptor
    mov [esp + 0x18], edx               // Replace first parameter.
    mov [esp + 0x10], ecx
    pop edx                             // Restore registers.
    pop ecx
    pop ebx
    pop eax
    ret                                 // Jump to original function.
  }
}

bool SmartSidestepResolverThunk::IsInternalCall(const void* base,
                                                void* return_address) {
  DCHECK_NT(base);
  DCHECK_NT(return_address);

  base::win::PEImage pe(base);
  if (pe.GetImageSectionFromAddr(return_address))
    return true;
  return false;
}

}  // namespace sandbox
