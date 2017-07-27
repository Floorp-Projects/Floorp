/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TlsAllocationTracker.h"

#include <stdint.h>

#include <windows.h>

#include "mozilla/Atomics.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Move.h"
#include "mozilla/StackWalk.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"

#include "nsDebug.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsWindowsDllInterceptor.h"

namespace mozilla {
namespace {

static StaticAutoPtr<nsCString> sTlsAllocationStacks;

struct nsCStringWriter : public JSONWriteFunc {
  explicit nsCStringWriter(nsCString* aData)
    : mData(aData)
  {
    MOZ_ASSERT(mData);
  }

  void Write(const char* aStr)
  {
    mData->AppendASCII(aStr);
  }

  nsCString* mData;
};

// Start recording TlsAlloc() call stacks when we observed kStartTrackingTlsAt
// allocations. We choose this value close to the maximum number of TLS indexes
// in a Windows process, which is 1088, in the hope of catching TLS leaks
// without impacting normal users.
const uint32_t kStartTrackingTlsAt = 950;

using stack_t = nsTArray<uintptr_t>;

struct StackEntry : public nsUint32HashKey {
  explicit StackEntry(KeyTypePointer aKey)
    : nsUint32HashKey(aKey)
  { }

  stack_t mStack;
};

using stacks_t = nsTHashtable<StackEntry>;

static StaticAutoPtr<stacks_t> sRecentTlsAllocationStacks;

static Atomic<bool> sInitialized;
static StaticMutex sMutex;
static Atomic<uint32_t> sCurrentTlsSlots{0};

using TlsAllocFn = DWORD (WINAPI *)();
using TlsFreeFn = BOOL (WINAPI *)(DWORD);

TlsAllocFn gOriginalTlsAlloc;
TlsFreeFn gOriginalTlsFree;

static void
MaybeRecordCurrentStack(DWORD aTlsIndex)
{
  if (sCurrentTlsSlots < kStartTrackingTlsAt) {
    return;
  }

  stack_t rawStack;
  auto callback = [](uint32_t, void* aPC, void*, void* aClosure) {
    auto stack = static_cast<stack_t*>(aClosure);
    stack->AppendElement(reinterpret_cast<uintptr_t>(aPC));
  };
  MozStackWalk(callback, /* skip 2 frames */ 2, /* maxFrames */ 0, &rawStack);

  StaticMutexAutoLock lock(sMutex);
  if (!sRecentTlsAllocationStacks) {
    return;
  }

  StackEntry* stack = sRecentTlsAllocationStacks->PutEntry(aTlsIndex);

  MOZ_ASSERT(!stack->mStack.IsEmpty());
  stack->mStack = Move(rawStack);
}

static void
MaybeDeleteRecordedStack(DWORD aTlsIndex)
{
  StaticMutexAutoLock lock(sMutex);
  if (!sRecentTlsAllocationStacks) {
    return;
  }

  auto entry = sRecentTlsAllocationStacks->GetEntry(aTlsIndex);
  if (entry) {
    sRecentTlsAllocationStacks->RemoveEntry(aTlsIndex);
  }
}


static void
AnnotateRecentTlsStacks()
{
  StaticMutexAutoLock lock(sMutex);
  if (!sRecentTlsAllocationStacks) {
    // Maybe another thread steals the stack vector content and is dumping the
    // stacks
    return;
  }

  // Move the content to prevent further requests to this function.
  UniquePtr<stacks_t> stacks = MakeUnique<stacks_t>();
  sRecentTlsAllocationStacks->SwapElements(*stacks.get());
  sRecentTlsAllocationStacks = nullptr;

  sTlsAllocationStacks = new nsCString();
  JSONWriter output(
    MakeUnique<nsCStringWriter, nsCString*>(sTlsAllocationStacks.get()));

  output.Start(JSONWriter::SingleLineStyle);
  for (auto iter = stacks->Iter(); !iter.Done(); iter.Next()) {
    const stack_t& stack = iter.Get()->mStack;

    output.StartArrayElement();
    for (auto pc : stack) {
      output.IntElement(pc);
    }
    output.EndArray();
  }
  output.End();
}

DWORD WINAPI
InterposedTlsAlloc()
{
  if (!sInitialized) {
    // Don't interpose if we didn't fully initialize both hooks or after we
    // already shutdown the tracker.
    return gOriginalTlsAlloc();
  }

  sCurrentTlsSlots += 1;

  DWORD tlsAllocRv = gOriginalTlsAlloc();

  MaybeRecordCurrentStack(tlsAllocRv);

  if (tlsAllocRv == TLS_OUT_OF_INDEXES) {
    AnnotateRecentTlsStacks();
  }

  return tlsAllocRv;
}

BOOL WINAPI
InterposedTlsFree(DWORD aTlsIndex)
{
  if (!sInitialized) {
    // Don't interpose if we didn't fully initialize both hooks or after we
    // already shutdown the tracker.
    return gOriginalTlsFree(aTlsIndex);
  }

  sCurrentTlsSlots -= 1;

  MaybeDeleteRecordedStack(aTlsIndex);

  return gOriginalTlsFree(aTlsIndex);
}

}  // Anonymous namespace.

void
InitTlsAllocationTracker()
{
  if (sInitialized) {
    return;
  }

  sRecentTlsAllocationStacks = new stacks_t();

  // Windows DLL interceptor
  static WindowsDllInterceptor sKernel32DllInterceptor{};

  // Initialize dll interceptor and add hook.
  sKernel32DllInterceptor.Init("kernel32.dll");
  bool succeeded = sKernel32DllInterceptor.AddHook(
    "TlsAlloc",
    reinterpret_cast<intptr_t>(InterposedTlsAlloc),
    reinterpret_cast<void**>(&gOriginalTlsAlloc));

  if (!succeeded) {
    return;
  }

  succeeded = sKernel32DllInterceptor.AddHook(
    "TlsFree",
    reinterpret_cast<intptr_t>(InterposedTlsFree),
    reinterpret_cast<void**>(&gOriginalTlsFree));

  if (!succeeded) {
    return;
  }

  sInitialized = true;
}

const char*
GetTlsAllocationStacks()
{
  StaticMutexAutoLock lock(sMutex);

  if (!sTlsAllocationStacks) {
    return nullptr;
  }

  return sTlsAllocationStacks->BeginReading();
}

void
ShutdownTlsAllocationTracker()
{
  if (!sInitialized) {
    return;
  }
  sInitialized = false;

  StaticMutexAutoLock lock(sMutex);

  sRecentTlsAllocationStacks = nullptr;
  sTlsAllocationStacks = nullptr;
}

}  // namespace mozilla
