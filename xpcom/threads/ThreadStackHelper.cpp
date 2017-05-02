/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadStackHelper.h"
#include "MainThreadUtils.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"
#include "jsfriendapi.h"
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
#include "js/ProfilingStack.h"
#endif

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Sprintf.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#if defined(MOZ_VALGRIND)
# include <valgrind/valgrind.h>
#endif

#include <string.h>
#include <vector>
#include <cstdlib>

#ifdef XP_LINUX
#include <ucontext.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#ifdef __GNUC__
# pragma GCC diagnostic pop // -Wshadow
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX)
#include <pthread.h>
#endif

#ifdef ANDROID
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif
#if defined(__arm__) && !defined(__NR_rt_tgsigqueueinfo)
// Some NDKs don't define this constant even though the kernel supports it.
#define __NR_rt_tgsigqueueinfo (__NR_SYSCALL_BASE+363)
#endif
#ifndef SYS_rt_tgsigqueueinfo
#define SYS_rt_tgsigqueueinfo __NR_rt_tgsigqueueinfo
#endif
#endif

namespace mozilla {

ThreadStackHelper::ThreadStackHelper()
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  : mStackToFill(nullptr)
  , mPseudoStack(profiler_get_pseudo_stack())
  , mMaxStackSize(Stack::sMaxInlineStorage)
  , mMaxBufferSize(512)
#endif
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  , mNativeStackToFill(nullptr)
#endif
{
  mThreadId = profiler_current_thread_id();
}

namespace {
template<typename T>
class ScopedSetPtr
{
private:
  T*& mPtr;
public:
  ScopedSetPtr(T*& p, T* val) : mPtr(p) { mPtr = val; }
  ~ScopedSetPtr() { mPtr = nullptr; }
};
} // namespace

void
ThreadStackHelper::GetPseudoStack(Stack& aStack)
{
  GetStacksInternal(&aStack, nullptr);
}

void
ThreadStackHelper::GetNativeStack(NativeStack& aNativeStack)
{
  GetStacksInternal(nullptr, &aNativeStack);
}

void
ThreadStackHelper::GetPseudoAndNativeStack(Stack& aStack, NativeStack& aNativeStack)
{
  GetStacksInternal(&aStack, &aNativeStack);
}

void
ThreadStackHelper::GetStacksInternal(Stack* aStack, NativeStack* aNativeStack)
{
#if defined(MOZ_THREADSTACKHELPER_PSEUDO) || defined(MOZ_THREADSTACKHELPER_NATIVE)
  // Always run PrepareStackBuffer first to clear aStack
  if (aStack && !PrepareStackBuffer(*aStack)) {
    // Skip and return empty aStack
    return;
  }

  // Prepare the native stack
  if (aNativeStack) {
    aNativeStack->clear();
    aNativeStack->reserve(Telemetry::HangStack::sMaxNativeFrames);
  }

#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  ScopedSetPtr<Stack> stackPtr(mStackToFill, aStack);
#endif
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  ScopedSetPtr<NativeStack> nativeStackPtr(mNativeStackToFill, aNativeStack);
#endif

  auto callback = [&, this] (void** aPCs, size_t aCount) {
    FillStackBuffer();

#ifdef MOZ_THREADSTACKHELPER_NATIVE
    if (mNativeStackToFill) {
      while (aCount-- &&
             mNativeStackToFill->size() < mNativeStackToFill->capacity()) {
        mNativeStackToFill->push_back(reinterpret_cast<uintptr_t>(aPCs[aCount]));
      }
    }
#endif
  };

  profiler_suspend_and_sample_thread(mThreadId,
                                     callback,
                                     /* aSampleNative = */ !!aNativeStack);
#endif
}

bool
ThreadStackHelper::PrepareStackBuffer(Stack& aStack)
{
  // Return false to skip getting the stack and return an empty stack
  aStack.clear();
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  /* Normally, provided the profiler is enabled, it would be an error if we
     don't have a pseudostack here (the thread probably forgot to call
     profiler_register_thread). However, on B2G, profiling secondary threads
     may be disabled despite profiler being enabled. This is by-design and
     is not an error. */
#ifdef MOZ_WIDGET_GONK
  if (!mPseudoStack) {
    return false;
  }
#endif
  MOZ_ASSERT(mPseudoStack);
  if (!aStack.reserve(mMaxStackSize) ||
      !aStack.reserve(aStack.capacity()) || // reserve up to the capacity
      !aStack.EnsureBufferCapacity(mMaxBufferSize)) {
    return false;
  }
  return true;
#else
  return false;
#endif
}

#ifdef MOZ_THREADSTACKHELPER_PSEUDO

namespace {

bool
IsChromeJSScript(JSScript* aScript)
{
  // May be called from another thread or inside a signal handler.
  // We assume querying the script is safe but we must not manipulate it.

  nsIScriptSecurityManager* const secman =
    nsScriptSecurityManager::GetScriptSecurityManager();
  NS_ENSURE_TRUE(secman, false);

  JSPrincipals* const principals = JS_GetScriptPrincipals(aScript);
  return secman->IsSystemPrincipal(nsJSPrincipals::get(principals));
}

// Get the full path after the URI scheme, if the URI matches the scheme.
// For example, GetFullPathForScheme("a://b/c/d/e", "a://") returns "b/c/d/e".
template <size_t LEN>
const char*
GetFullPathForScheme(const char* filename, const char (&scheme)[LEN]) {
  // Account for the null terminator included in LEN.
  if (!strncmp(filename, scheme, LEN - 1)) {
    return filename + LEN - 1;
  }
  return nullptr;
}

// Get the full path after a URI component, if the URI contains the component.
// For example, GetPathAfterComponent("a://b/c/d/e", "/c/") returns "d/e".
template <size_t LEN>
const char*
GetPathAfterComponent(const char* filename, const char (&component)[LEN]) {
  const char* found = nullptr;
  const char* next = strstr(filename, component);
  while (next) {
    // Move 'found' to end of the component, after the separator '/'.
    // 'LEN - 1' accounts for the null terminator included in LEN,
    found = next + LEN - 1;
    // Resume searching before the separator '/'.
    next = strstr(found - 1, component);
  }
  return found;
}

} // namespace

const char*
ThreadStackHelper::AppendJSEntry(const js::ProfileEntry* aEntry,
                                 intptr_t& aAvailableBufferSize,
                                 const char* aPrevLabel)
{
  // May be called from another thread or inside a signal handler.
  // We assume querying the script is safe but we must not manupulate it.
  // Also we must not allocate any memory from heap.
  MOZ_ASSERT(aEntry->isJs());

  const char* label;
  JSScript* script = aEntry->script();
  if (!script) {
    label = "(profiling suppressed)";
  } else if (IsChromeJSScript(aEntry->script())) {
    const char* filename = JS_GetScriptFilename(aEntry->script());
    const unsigned lineno = JS_PCToLineNumber(aEntry->script(), aEntry->pc());
    MOZ_ASSERT(filename);

    char buffer[128]; // Enough to fit longest js file name from the tree

    // Some script names are in the form "foo -> bar -> baz".
    // Here we find the origin of these redirected scripts.
    const char* basename = GetPathAfterComponent(filename, " -> ");
    if (basename) {
      filename = basename;
    }

    basename = GetFullPathForScheme(filename, "chrome://");
    if (!basename) {
      basename = GetFullPathForScheme(filename, "resource://");
    }
    if (!basename) {
      // If the (add-on) script is located under the {profile}/extensions
      // directory, extract the path after the /extensions/ part.
      basename = GetPathAfterComponent(filename, "/extensions/");
    }
    if (!basename) {
      // Only keep the file base name for paths outside the above formats.
      basename = strrchr(filename, '/');
      basename = basename ? basename + 1 : filename;
      // Look for Windows path separator as well.
      filename = strrchr(basename, '\\');
      if (filename) {
        basename = filename + 1;
      }
    }

    size_t len = SprintfLiteral(buffer, "%s:%u", basename, lineno);
    if (len < sizeof(buffer)) {
      if (mStackToFill->IsSameAsEntry(aPrevLabel, buffer)) {
        return aPrevLabel;
      }

      // Keep track of the required buffer size
      aAvailableBufferSize -= (len + 1);
      if (aAvailableBufferSize >= 0) {
        // Buffer is big enough.
        return mStackToFill->InfallibleAppendViaBuffer(buffer, len);
      }
      // Buffer is not big enough; fall through to using static label below.
    }
    // snprintf failed or buffer is not big enough.
    label = "(chrome script)";
  } else {
    label = "(content script)";
  }

  if (mStackToFill->IsSameAsEntry(aPrevLabel, label)) {
    return aPrevLabel;
  }
  mStackToFill->infallibleAppend(label);
  return label;
}

#endif // MOZ_THREADSTACKHELPER_PSEUDO

void
ThreadStackHelper::FillStackBuffer()
{
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  MOZ_ASSERT(mStackToFill->empty());

  size_t reservedSize = mStackToFill->capacity();
  size_t reservedBufferSize = mStackToFill->AvailableBufferSize();
  intptr_t availableBufferSize = intptr_t(reservedBufferSize);

  // Go from front to back
  const js::ProfileEntry* entry = mPseudoStack->entries;
  const js::ProfileEntry* end = entry + mPseudoStack->stackSize();
  // Deduplicate identical, consecutive frames
  const char* prevLabel = nullptr;
  for (; reservedSize-- && entry != end; entry++) {
    if (entry->isJs()) {
      prevLabel = AppendJSEntry(entry, availableBufferSize, prevLabel);
      continue;
    }
    const char* const label = entry->label();
    if (mStackToFill->IsSameAsEntry(prevLabel, label)) {
      // Avoid duplicate labels to save space in the stack.
      continue;
    }
    mStackToFill->infallibleAppend(label);
    prevLabel = label;
  }

  // end != entry if we exited early due to not enough reserved frames.
  // Expand the number of reserved frames for next time.
  mMaxStackSize = mStackToFill->capacity() + (end - entry);

  // availableBufferSize < 0 if we needed a larger buffer than we reserved.
  // Calculate a new reserve size for next time.
  if (availableBufferSize < 0) {
    mMaxBufferSize = reservedBufferSize - availableBufferSize;
  }
#endif
}

} // namespace mozilla
