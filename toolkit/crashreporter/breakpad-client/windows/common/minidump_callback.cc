/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "minidump_callback.h"

#include <winternl.h>

#include <algorithm>
#include <cassert>

namespace google_breakpad {

static const DWORD sHeapRegionSize= 1024;
static DWORD sPageSize = 0;

using NtQueryInformationThreadFunc = decltype(::NtQueryInformationThread);
static NtQueryInformationThreadFunc* sNtQueryInformationThread = nullptr;


namespace {
enum {
  ThreadBasicInformation,
};

struct CLIENT_ID {
  PVOID UniqueProcess;
  PVOID UniqueThread;
};

struct THREAD_BASIC_INFORMATION {
  NTSTATUS ExitStatus;
  PVOID TebBaseAddress;
  CLIENT_ID ClientId;
  KAFFINITY AffMask;
  DWORD Priority;
  DWORD BasePriority;
};
}

void InitAppMemoryInternal()
{
  if (!sPageSize) {
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    sPageSize = systemInfo.dwPageSize;
  }

  if (!sNtQueryInformationThread) {
    sNtQueryInformationThread = (NtQueryInformationThreadFunc*)
      (::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"),
                                           "NtQueryInformationThread"));
  }
}

bool GetAppMemoryFromRegister(HANDLE aProcess,
                              const NT_TIB* aTib,
                              RegisterValueType aRegister,
                              AppMemory* aResult)
{
  static_assert(sizeof(RegisterValueType) == sizeof(void*),
                "Size mismatch between DWORD/DWORD64 and void*");

  if (!sPageSize) {
    // GetSystemInfo() should not fail, but bail out just in case it fails.
    return false;
  }

  RegisterValueType addr = aRegister;
  addr &= ~(static_cast<RegisterValueType>(sPageSize) - 1);

  if (aTib) {
    if (aRegister >= (RegisterValueType)aTib->StackLimit &&
        aRegister <= (RegisterValueType)aTib->StackBase) {
      // aRegister points to the stack.
      return false;
    }
  }

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

static bool
GetThreadTib(HANDLE aProcess, DWORD aThreadId, NT_TIB* aTib) {
  HANDLE threadHandle = ::OpenThread(THREAD_QUERY_INFORMATION,
                                     FALSE,
                                     aThreadId);
  if (!threadHandle) {
    return false;
  }

  if (!sNtQueryInformationThread) {
    return false;
  }

  THREAD_BASIC_INFORMATION threadInfo;
  auto status = (*sNtQueryInformationThread)(threadHandle,
                                             (THREADINFOCLASS)ThreadBasicInformation,
                                             &threadInfo,
                                             sizeof(threadInfo),
                                             NULL);
  if (!NT_SUCCESS(status)) {
    return false;
  }

  auto readSuccess = ::ReadProcessMemory(aProcess,
                                         threadInfo.TebBaseAddress,
                                         aTib,
                                         sizeof(*aTib),
                                         NULL);
  if (!readSuccess) {
    return false;
  }

  ::CloseHandle(threadHandle);
  return true;
}

void IncludeAppMemoryFromExceptionContext(HANDLE aProcess,
                                          DWORD aThreadId,
                                          AppMemoryList& aList,
                                          PCONTEXT aExceptionContext,
                                          bool aInstructionPointerOnly) {
  RegisterValueType heapAddrCandidates[kExceptionAppMemoryRegions];
  size_t numElements = 0;

  NT_TIB tib;
  memset(&tib, 0, sizeof(tib));
  if (!GetThreadTib(aProcess, aThreadId, &tib)) {
    // Fail to query thread stack range: only safe to include the region around
    // the instruction pointer.
    aInstructionPointerOnly = true;
  }

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
#elif defined(_M_ARM64)
  if (!aInstructionPointerOnly) {
    for (auto reg : aExceptionContext->X) {
      heapAddrCandidates[numElements++] = reg;
    }
    heapAddrCandidates[numElements++] = aExceptionContext->Sp;
  }
  heapAddrCandidates[numElements++] = aExceptionContext->Pc;
#endif

  // Inplace sort the candidates for excluding or merging memory regions.
  auto begin = &heapAddrCandidates[0], end = &heapAddrCandidates[numElements];
  std::make_heap(begin, end);
  std::sort_heap(begin, end);

  auto appMemory = FindNextPreallocated(aList, aList.begin());
  for (size_t i = 0; i < numElements; i++) {
    if (appMemory == aList.end()) {
      break;
    }

    AppMemory tmp{};
    if (!GetAppMemoryFromRegister(aProcess,
                                  aInstructionPointerOnly ? nullptr : &tib,
                                  heapAddrCandidates[i],
                                  &tmp)) {
      continue;
    }

    if (!(tmp.ptr && tmp.length)) {
      // Something unexpected happens. Skip this candidate.
      continue;
    }

    if (!appMemory->ptr) {
      *appMemory = tmp;
      continue;
    }

    if (appMemory->ptr + appMemory->length > tmp.ptr) {
      // The beginning of the next region fall within the range of the previous
      // region: merge into one. Note that we don't merge adjacent regions like
      // [0, 99] and [100, 199] in case we cross the border of memory allocation
      // regions.
      appMemory->length = tmp.ptr + tmp.length - appMemory->ptr;
      continue;
    }

    appMemory = FindNextPreallocated(aList, ++appMemory);
    if (appMemory == aList.end()) {
      break;
    }

    *appMemory = tmp;
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

