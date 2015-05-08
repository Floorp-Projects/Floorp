/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UncensoredAllocator.h"

#include "mozilla/unused.h"

#include "jsfriendapi.h"
#ifdef MOZ_REPLACE_MALLOC
#include "replace_malloc_bridge.h"
#endif

namespace mozilla {

static void* (*uncensored_malloc)(size_t size);
static void (*uncensored_free)(void* ptr);

#ifdef MOZ_REPLACE_MALLOC

static bool sMemoryHookEnabled = false;
static NativeProfiler* sNativeProfiler;
static malloc_hook_table_t sMallocHook;

static void*
SampleNative(void* addr, size_t size)
{
  if (sMemoryHookEnabled) {
    sNativeProfiler->sampleNative(addr, size);
  }
  return addr;
}

static void
RemoveNative(void* addr)
{
  if (sMemoryHookEnabled) {
    sNativeProfiler->removeNative(addr);
  }
}
#endif

void*
u_malloc(size_t size)
{
  if (uncensored_malloc) {
    return uncensored_malloc(size);
  } else {
    return malloc(size);
  }
}

void
u_free(void* ptr)
{
  if (uncensored_free) {
    uncensored_free(ptr);
  } else {
    free(ptr);
  }
}

void InitializeMallocHook()
{
#ifdef MOZ_REPLACE_MALLOC
  sMallocHook.free_hook = RemoveNative;
  sMallocHook.malloc_hook = SampleNative;
  ReplaceMallocBridge* bridge = ReplaceMallocBridge::Get(3);
  if (bridge) {
    mozilla::unused << bridge->RegisterHook("memory-profiler", nullptr, nullptr);
  }
#endif
  if (!uncensored_malloc && !uncensored_free) {
    uncensored_malloc = malloc;
    uncensored_free = free;
  }
}

void EnableMallocHook(NativeProfiler* aNativeProfiler)
{
#ifdef MOZ_REPLACE_MALLOC
  ReplaceMallocBridge* bridge = ReplaceMallocBridge::Get(3);
  if (bridge) {
    const malloc_table_t* alloc_funcs =
      bridge->RegisterHook("memory-profiler", nullptr, &sMallocHook);
    if (alloc_funcs) {
      uncensored_malloc = alloc_funcs->malloc;
      uncensored_free = alloc_funcs->free;
      sNativeProfiler = aNativeProfiler;
      sMemoryHookEnabled = true;
    }
  }
#endif
}

void DisableMallocHook()
{
#ifdef MOZ_REPLACE_MALLOC
  ReplaceMallocBridge* bridge = ReplaceMallocBridge::Get(3);
  if (bridge) {
    bridge->RegisterHook("memory-profiler", nullptr, nullptr);
    sMemoryHookEnabled = false;
  }
#endif
}

} // namespace mozilla
