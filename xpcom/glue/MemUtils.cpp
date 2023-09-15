/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MemUtils.h"

#if defined(XP_WIN)
#  include <windows.h>
#  include "mozilla/Maybe.h"
#else
#  include <sys/mman.h>
#endif

#if defined(XP_WIN)
typedef BOOL(WINAPI* PrefetchVirtualMemoryFn)(HANDLE, ULONG_PTR, PVOID, ULONG);

static mozilla::Maybe<PrefetchVirtualMemoryFn> sPrefetchVirtualMemory;

void MaybeInitPrefetchVirtualMemory() {
  if (sPrefetchVirtualMemory.isNothing()) {
    sPrefetchVirtualMemory.emplace(
        reinterpret_cast<PrefetchVirtualMemoryFn>(GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "PrefetchVirtualMemory")));
  }
}
#endif

bool mozilla::CanPrefetchMemory() {
#if defined(XP_SOLARIS) || defined(XP_UNIX)
  return true;
#elif defined(XP_WIN)
  MaybeInitPrefetchVirtualMemory();
  return *sPrefetchVirtualMemory;
#else
  return false;
#endif
}

void mozilla::PrefetchMemory(uint8_t* aStart, size_t aNumBytes) {
  if (aNumBytes == 0) {
    return;
  }

#if defined(XP_SOLARIS)
  posix_madvise(aStart, aNumBytes, POSIX_MADV_WILLNEED);
#elif defined(XP_UNIX)
  madvise(aStart, aNumBytes, MADV_WILLNEED);
#elif defined(XP_WIN)
  MaybeInitPrefetchVirtualMemory();
  if (*sPrefetchVirtualMemory) {
    WIN32_MEMORY_RANGE_ENTRY entry;
    entry.VirtualAddress = aStart;
    entry.NumberOfBytes = aNumBytes;
    (*sPrefetchVirtualMemory)(GetCurrentProcess(), 1, &entry, 0);
    return;
  }
#endif
}
