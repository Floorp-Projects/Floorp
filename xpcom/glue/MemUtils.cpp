/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MemUtils.h"

#if defined(XP_WIN)
#  include <windows.h>
#else
#  include <sys/mman.h>
#endif

bool mozilla::MaybePrefetchMemory(uint8_t* aStart, size_t aNumBytes) {
  if (aNumBytes == 0) {
    return true;
  }

#if defined(XP_SOLARIS)
  posix_madvise(aStart, aNumBytes, POSIX_MADV_WILLNEED);
  return true;
#elif defined(XP_UNIX)
  madvise(aStart, aNumBytes, MADV_WILLNEED);
  return true;
#elif defined(XP_WIN)
  static auto prefetchVirtualMemory =
      reinterpret_cast<BOOL(WINAPI*)(HANDLE, ULONG_PTR, PVOID, ULONG)>(
          GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                         "PrefetchVirtualMemory"));

  if (prefetchVirtualMemory) {
    // Normally, we'd use WIN32_MEMORY_RANGE_ENTRY, but that requires
    // a different _WIN32_WINNT value before including windows.h, but
    // that causes complications with unified sources. It's a simple
    // enough struct anyways.
    struct {
      PVOID VirtualAddress;
      SIZE_T NumberOfBytes;
    } entry;
    entry.VirtualAddress = aStart;
    entry.NumberOfBytes = aNumBytes;
    prefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
    return true;
  }
  return false;
#else
  return false;
#endif
}
