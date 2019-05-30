/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include <windows.h>

#if !defined(MOZ_MEMORY)
#  include <malloc.h>
#  define malloc_impl malloc
#  define calloc_impl calloc
#  define realloc_impl realloc
#  define free_impl free
#endif

// Warning: C4273: 'HeapAlloc': inconsistent dll linkage
// The Windows headers define HeapAlloc as dllimport, but we define it as
// dllexport, which is a voluntary inconsistency.
#pragma warning(disable : 4273)

MFBT_API
LPVOID WINAPI HeapAlloc(_In_ HANDLE hHeap, _In_ DWORD dwFlags,
                        _In_ SIZE_T dwBytes) {
  if (dwFlags & HEAP_ZERO_MEMORY) {
    return calloc_impl(1, dwBytes);
  }
  return malloc_impl(dwBytes);
}

MFBT_API
LPVOID WINAPI HeapReAlloc(_In_ HANDLE hHeap, _In_ DWORD dwFlags,
                          _In_ LPVOID lpMem, _In_ SIZE_T dwBytes) {
  // The HeapReAlloc contract is that failures preserve the existing
  // allocation. We can't try to realloc in-place without possibly
  // freeing the original allocation, breaking the contract.
  // We also can't guarantee we zero all the memory from the end of
  // the original allocation to the end of the new one because of the
  // difference between the originally requested size and what
  // malloc_usable_size would return us.
  // So for both cases, just tell the caller we can't do what they
  // requested.
  if (dwFlags & (HEAP_REALLOC_IN_PLACE_ONLY | HEAP_ZERO_MEMORY)) {
    return NULL;
  }
  return realloc_impl(lpMem, dwBytes);
}

MFBT_API
BOOL WINAPI HeapFree(_In_ HANDLE hHeap, _In_ DWORD dwFlags, _In_ LPVOID lpMem) {
  free_impl(lpMem);
  return true;
}
