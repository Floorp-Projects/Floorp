/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadStackHelper.h"
#include "MainThreadUtils.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"
#include "jsfriendapi.h"
#include "prprf.h"

#include "js/OldDebugAPI.h"

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"

#include <string.h>

#ifdef XP_LINUX
#include <unistd.h>
#include <sys/syscall.h>
#endif

#ifdef ANDROID
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif
#ifndef SYS_tgkill
#define SYS_tgkill __NR_tgkill
#endif
#endif

namespace mozilla {

void
ThreadStackHelper::Startup()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInitialized) {
    MOZ_ALWAYS_TRUE(!::sem_init(&sSem, 0, 0));
  }
  sInitialized++;
#endif
}

void
ThreadStackHelper::Shutdown()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (sInitialized == 1) {
    MOZ_ALWAYS_TRUE(!::sem_destroy(&sSem));
  }
  sInitialized--;
#endif
}

ThreadStackHelper::ThreadStackHelper()
  :
#ifdef MOZ_ENABLE_PROFILER_SPS
    mPseudoStack(mozilla_get_pseudo_stack()),
#endif
    mStackToFill(nullptr)
  , mMaxStackSize(Stack::sMaxInlineStorage)
  , mMaxBufferSize(0)
{
#if defined(XP_LINUX)
  mThreadID = ::syscall(SYS_gettid);
#elif defined(XP_WIN)
  mInitialized = !!::DuplicateHandle(
    ::GetCurrentProcess(), ::GetCurrentThread(),
    ::GetCurrentProcess(), &mThreadID,
    THREAD_SUSPEND_RESUME, FALSE, 0);
  MOZ_ASSERT(mInitialized);
#elif defined(XP_MACOSX)
  mThreadID = mach_thread_self();
#endif
}

ThreadStackHelper::~ThreadStackHelper()
{
#if defined(XP_WIN)
  if (mInitialized) {
    MOZ_ALWAYS_TRUE(!!::CloseHandle(mThreadID));
  }
#endif
}

#if defined(XP_LINUX) && defined(__arm__)
// Some (old) Linux kernels on ARM have a bug where a signal handler
// can be called without clearing the IT bits in CPSR first. The result
// is that the first few instructions of the handler could be skipped,
// ultimately resulting in crashes. To workaround this bug, the handler
// on ARM is a trampoline that starts with enough NOP instructions, so
// that even if the IT bits are not cleared, only the NOP instructions
// will be skipped over.

template <void (*H)(int, siginfo_t*, void*)>
__attribute__((naked)) void
SignalTrampoline(int aSignal, siginfo_t* aInfo, void* aContext)
{
  asm volatile (
    "nop; nop; nop; nop"
    : : : "memory");

  // Because the assembler may generate additional insturctions below, we
  // need to ensure NOPs are inserted first by separating them out above.

  asm volatile (
    "bx %0"
    :
    : "r"(H), "l"(aSignal), "l"(aInfo), "l"(aContext)
    : "memory");
}
#endif // XP_LINUX && __arm__

void
ThreadStackHelper::GetStack(Stack& aStack)
{
  // Always run PrepareStackBuffer first to clear aStack
  if (!PrepareStackBuffer(aStack)) {
    // Skip and return empty aStack
    return;
  }

#if defined(XP_LINUX)
  if (profiler_is_active()) {
    // Profiler can interfere with our Linux signal handling
    return;
  }
  if (!sInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  sCurrent = this;
  struct sigaction sigact = {};
#ifdef __arm__
  sigact.sa_sigaction = SignalTrampoline<SigAction>;
#else
  sigact.sa_sigaction = SigAction;
#endif
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_SIGINFO | SA_RESTART;
  if (::sigaction(SIGPROF, &sigact, &sOldSigAction)) {
    MOZ_ASSERT(false);
    return;
  }
  MOZ_ALWAYS_TRUE(!::syscall(SYS_tgkill, getpid(), mThreadID, SIGPROF));
  MOZ_ALWAYS_TRUE(!::sem_wait(&sSem));

#elif defined(XP_WIN)
  if (!mInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  if (::SuspendThread(mThreadID) == DWORD(-1)) {
    MOZ_ASSERT(false);
    return;
  }
  FillStackBuffer();
  MOZ_ALWAYS_TRUE(::ResumeThread(mThreadID) != DWORD(-1));

#elif defined(XP_MACOSX)
  if (::thread_suspend(mThreadID) != KERN_SUCCESS) {
    MOZ_ASSERT(false);
    return;
  }
  FillStackBuffer();
  MOZ_ALWAYS_TRUE(::thread_resume(mThreadID) == KERN_SUCCESS);

#endif
}

#ifdef XP_LINUX

int ThreadStackHelper::sInitialized;
sem_t ThreadStackHelper::sSem;
struct sigaction ThreadStackHelper::sOldSigAction;
ThreadStackHelper* ThreadStackHelper::sCurrent;

void
ThreadStackHelper::SigAction(int aSignal, siginfo_t* aInfo, void* aContext)
{
  ::sigaction(SIGPROF, &sOldSigAction, nullptr);
  sCurrent->FillStackBuffer();
  sCurrent = nullptr;
  ::sem_post(&sSem);
}

#endif // XP_LINUX

bool
ThreadStackHelper::PrepareStackBuffer(Stack& aStack)
{
  // Return false to skip getting the stack and return an empty stack
  aStack.clear();
#ifdef MOZ_ENABLE_PROFILER_SPS
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
  mStackToFill = &aStack;
  return true;
#else
  return false;
#endif
}

#ifdef MOZ_ENABLE_PROFILER_SPS

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

} // namespace

const char*
ThreadStackHelper::AppendJSEntry(const volatile StackEntry* aEntry,
                                 intptr_t& aAvailableBufferSize,
                                 const char* aPrevLabel)
{
  // May be called from another thread or inside a signal handler.
  // We assume querying the script is safe but we must not manupulate it.
  // Also we must not allocate any memory from heap.
  MOZ_ASSERT(aEntry->isJs());
  MOZ_ASSERT(aEntry->script());

  const char* label;
  if (IsChromeJSScript(aEntry->script())) {
    const char* const filename = JS_GetScriptFilename(aEntry->script());
    unsigned lineno = JS_PCToLineNumber(nullptr, aEntry->script(),
                                        aEntry->pc());
    MOZ_ASSERT(filename);

    char buffer[64]; // Enough to fit longest js file name from the tree
    const char* const basename = strrchr(filename, '/');
    size_t len = PR_snprintf(buffer, sizeof(buffer), "%s:%u",
                             basename ? basename + 1 : filename, lineno);
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

#endif // MOZ_ENABLE_PROFILER_SPS

void
ThreadStackHelper::FillStackBuffer()
{
  MOZ_ASSERT(mStackToFill->empty());

#ifdef MOZ_ENABLE_PROFILER_SPS
  size_t reservedSize = mStackToFill->capacity();
  size_t reservedBufferSize = mStackToFill->AvailableBufferSize();
  intptr_t availableBufferSize = intptr_t(reservedBufferSize);

  // Go from front to back
  const volatile StackEntry* entry = mPseudoStack->mStack;
  const volatile StackEntry* end = entry + mPseudoStack->stackSize();
  // Deduplicate identical, consecutive frames
  const char* prevLabel = nullptr;
  for (; reservedSize-- && entry != end; entry++) {
    /* We only accept non-copy labels, including js::RunScript,
       because we only want static labels in the hang stack. */
    if (entry->isCopyLabel()) {
      continue;
    }
    if (entry->isJs()) {
      prevLabel = AppendJSEntry(entry, availableBufferSize, prevLabel);
      continue;
    }
    const char* const label = entry->label();
    if (mStackToFill->IsSameAsEntry(prevLabel, label)) {
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
