/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UncensoredAllocator.h"

#include "mozilla/Assertions.h"
#include "mozilla/unused.h"

#include "MainThreadUtils.h"
#include "jsfriendapi.h"
#include "nsDebug.h"
#include "prlock.h"
#ifdef MOZ_REPLACE_MALLOC
#include "replace_malloc_bridge.h"
#endif

namespace mozilla {

#ifdef MOZ_REPLACE_MALLOC
ThreadLocal<bool> MallocHook::mEnabledTLS;
NativeProfiler* MallocHook::mNativeProfiler;
malloc_hook_table_t MallocHook::mMallocHook;
#endif

AutoUseUncensoredAllocator::AutoUseUncensoredAllocator()
{
#ifdef MOZ_REPLACE_MALLOC
  MallocHook::mEnabledTLS.set(false);
#endif
}

AutoUseUncensoredAllocator::~AutoUseUncensoredAllocator()
{
#ifdef MOZ_REPLACE_MALLOC
  MallocHook::mEnabledTLS.set(true);
#endif
}

bool
MallocHook::Enabled()
{
#ifdef MOZ_REPLACE_MALLOC
  return mEnabledTLS.get() && mNativeProfiler;
#else
  return false;
#endif
}

void*
MallocHook::SampleNative(void* aAddr, size_t aSize)
{
#ifdef MOZ_REPLACE_MALLOC
  if (MallocHook::Enabled()) {
    mNativeProfiler->sampleNative(aAddr, aSize);
  }
#endif
  return aAddr;
}

void
MallocHook::RemoveNative(void* aAddr)
{
#ifdef MOZ_REPLACE_MALLOC
  if (MallocHook::Enabled()) {
    mNativeProfiler->removeNative(aAddr);
  }
#endif
}

void
MallocHook::Initialize()
{
#ifdef MOZ_REPLACE_MALLOC
  MOZ_ASSERT(NS_IsMainThread());
  mMallocHook.free_hook = RemoveNative;
  mMallocHook.malloc_hook = SampleNative;
  ReplaceMallocBridge* bridge = ReplaceMallocBridge::Get(3);
  if (bridge) {
    mozilla::unused << bridge->RegisterHook("memory-profiler", nullptr, nullptr);
  }
  if (!mEnabledTLS.initialized()) {
    bool success = mEnabledTLS.init();
    if (NS_WARN_IF(!success)) {
      return;
    }
    mEnabledTLS.set(false);
  }
#endif
}

void
MallocHook::Enable(NativeProfiler* aNativeProfiler)
{
#ifdef MOZ_REPLACE_MALLOC
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!mEnabledTLS.initialized())) {
    return;
  }
  ReplaceMallocBridge* bridge = ReplaceMallocBridge::Get(3);
  if (bridge) {
    const malloc_table_t* alloc_funcs =
      bridge->RegisterHook("memory-profiler", nullptr, &mMallocHook);
    if (alloc_funcs) {
      mNativeProfiler = aNativeProfiler;
    }
  }
#endif
}

void
MallocHook::Disable()
{
#ifdef MOZ_REPLACE_MALLOC
  MOZ_ASSERT(NS_IsMainThread());
  ReplaceMallocBridge* bridge = ReplaceMallocBridge::Get(3);
  if (bridge) {
    bridge->RegisterHook("memory-profiler", nullptr, nullptr);
    mNativeProfiler = nullptr;
  }
#endif
}

} // namespace mozilla
