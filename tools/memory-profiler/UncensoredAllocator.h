/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_profiler_UncensoredAllocator_h
#define memory_profiler_UncensoredAllocator_h

#include "mozilla/ThreadLocal.h"

#ifdef MOZ_REPLACE_MALLOC
#include "replace_malloc_bridge.h"
#endif

class NativeProfiler;

namespace mozilla {

class MallocHook final
{
public:
  static void Initialize();
  static void Enable(NativeProfiler* aNativeProfiler);
  static void Disable();
  static bool Enabled();
private:
  static void* SampleNative(void* aAddr, size_t aSize);
  static void RemoveNative(void* aAddr);
#ifdef MOZ_REPLACE_MALLOC
  static ThreadLocal<bool> mEnabledTLS;
  static NativeProfiler* mNativeProfiler;
  static malloc_hook_table_t mMallocHook;
#endif
  friend class AutoUseUncensoredAllocator;
};

class AutoUseUncensoredAllocator final
{
public:
  AutoUseUncensoredAllocator();
  ~AutoUseUncensoredAllocator();
};

} // namespace mozilla

#endif // memory_profiler_UncensoredAllocator_h
