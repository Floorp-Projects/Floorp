/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "minidump_callback.h"

#include <algorithm>
#include <cassert>

namespace google_breakpad {

static const DWORD sHeapRegionSize= 1024;
static DWORD sPageSize = 0;

bool GetAppMemoryFromRegister(HANDLE aProcess,
                              RegisterValueType aRegister,
                              AppMemoryList::iterator aResult)
{
  static_assert(sizeof(RegisterValueType) == sizeof(void*),
                "Size mismatch between DWORD/DWORD64 and void*");

  if (!sPageSize) {
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    sPageSize = systemInfo.dwPageSize;
  }

  RegisterValueType addr = aRegister;
  addr &= ~(static_cast<RegisterValueType>(sPageSize) - 1);

  MEMORY_BASIC_INFORMATION memInfo;
  memset(&memInfo, 0, sizeof(memInfo));
  SIZE_T rv = ::VirtualQueryEx(aProcess,
                               reinterpret_cast<void*>(addr),
                               &memInfo,
                               sizeof(memInfo));
  if (!rv) {
    // VirtualQuery fails: aAddr is not on heap.
    return false;
  }

  // Check protection and type of the memory region. Include the region if it's
  // 1. read-write: heap, or
  // 2. read-executable and private: likely to be JIT code.
  if (memInfo.Protect != PAGE_READWRITE &&
      memInfo.Protect != PAGE_EXECUTE_READ) {
    return false;
  }

  // Try to include a region of size sHeapRegionSize around aRegister, bounded
  // by the [BaseAddress, BaseAddress + RegionSize].
  RegisterValueType lower =
    std::max(aRegister - sHeapRegionSize / 2,
             reinterpret_cast<RegisterValueType>(memInfo.BaseAddress));

  RegisterValueType upper =
    std::min(lower + sHeapRegionSize,
             reinterpret_cast<RegisterValueType>(memInfo.BaseAddress) +
               memInfo.RegionSize);

  aResult->ptr = lower;
  aResult->length = upper - lower;

  return true;
}

static AppMemoryList::iterator
FindNextPreallocated(AppMemoryList& aList, AppMemoryList::iterator aBegin) {
  auto it = aBegin;
  for (auto it = aBegin; it != aList.end(); it++) {
    if (it->preallocated) {
      return it;
    }
  }

  assert(it == aList.end());
  return it;
}

void IncludeAppMemoryFromExceptionContext(HANDLE aProcess,
                                          DWORD aThreadId,
                                          AppMemoryList& aList,
                                          PCONTEXT aExceptionContext,
                                          bool aInstructionPointerOnly) {
  RegisterValueType heapAddrCandidates[kExceptionAppMemoryRegions];
  size_t numElements = 0;

  // Add registers that might have a heap address to heapAddrCandidates.
  // Note that older versions of DbgHelp.dll don't correctly put the memory
  // around the faulting instruction pointer into the minidump. Include Rip/Eip
  // unconditionally ensures it gets included.
#if defined(_M_IX86)
  if (!aInstructionPointerOnly) {
    heapAddrCandidates[numElements++] = aExceptionContext->Eax;
    heapAddrCandidates[numElements++] = aExceptionContext->Ebx;
    heapAddrCandidates[numElements++] = aExceptionContext->Ecx;
    heapAddrCandidates[numElements++] = aExceptionContext->Edx;
    heapAddrCandidates[numElements++] = aExceptionContext->Esi;
    heapAddrCandidates[numElements++] = aExceptionContext->Edi;
  }
  heapAddrCandidates[numElements++] = aExceptionContext->Eip;
#elif defined(_M_AMD64)
  if (!aInstructionPointerOnly) {
    heapAddrCandidates[numElements++] = aExceptionContext->Rax;
    heapAddrCandidates[numElements++] = aExceptionContext->Rbx;
    heapAddrCandidates[numElements++] = aExceptionContext->Rcx;
    heapAddrCandidates[numElements++] = aExceptionContext->Rdx;
    heapAddrCandidates[numElements++] = aExceptionContext->Rsi;
    heapAddrCandidates[numElements++] = aExceptionContext->Rdi;
    heapAddrCandidates[numElements++] = aExceptionContext->R8;
    heapAddrCandidates[numElements++] = aExceptionContext->R9;
    heapAddrCandidates[numElements++] = aExceptionContext->R10;
    heapAddrCandidates[numElements++] = aExceptionContext->R11;
    heapAddrCandidates[numElements++] = aExceptionContext->R12;
    heapAddrCandidates[numElements++] = aExceptionContext->R13;
    heapAddrCandidates[numElements++] = aExceptionContext->R14;
    heapAddrCandidates[numElements++] = aExceptionContext->R15;
  }
  heapAddrCandidates[numElements++] = aExceptionContext->Rip;
#endif

  auto appMemory = aList.begin();
  for (size_t i = 0; i < numElements; i++) {
    appMemory = FindNextPreallocated(aList, appMemory);
    if (appMemory == aList.end()) {
      break;
    }

    if (GetAppMemoryFromRegister(aProcess, heapAddrCandidates[i], appMemory)) {
      appMemory++;
    }
  }
}

BOOL CALLBACK MinidumpWriteDumpCallback(
    PVOID context,
    const PMINIDUMP_CALLBACK_INPUT callback_input,
    PMINIDUMP_CALLBACK_OUTPUT callback_output) {
  switch (callback_input->CallbackType) {
  case MemoryCallback: {
    MinidumpCallbackContext* callback_context =
        reinterpret_cast<MinidumpCallbackContext*>(context);

    // Skip unused preallocated AppMemory elements.
    while (callback_context->iter != callback_context->end &&
           callback_context->iter->preallocated &&
           !callback_context->iter->ptr) {
      callback_context->iter++;
    }

    if (callback_context->iter == callback_context->end)
      return FALSE;

    // Include the specified memory region.
    callback_output->MemoryBase = callback_context->iter->ptr;
    callback_output->MemorySize = callback_context->iter->length;
    callback_context->iter++;
    return TRUE;
  }

    // Include all modules.
  case IncludeModuleCallback:
  case ModuleCallback:
    return TRUE;

    // Include all threads.
  case IncludeThreadCallback:
  case ThreadCallback:
    return TRUE;

    // Stop receiving cancel callbacks.
  case CancelCallback:
    callback_output->CheckCancel = FALSE;
    callback_output->Cancel = FALSE;
    return TRUE;
  }
  // Ignore other callback types.
  return FALSE;
}

}  // namespace google_breakpad

