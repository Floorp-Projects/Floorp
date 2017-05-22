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
#ifdef MOZ_THREADSTACKHELPER_NATIVE
#include "shared-libraries.h"
#endif
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
#include "PseudoStack.h"
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

#ifdef MOZ_THREADSTACKHELPER_NATIVE
#if defined(MOZ_THREADSTACKHELPER_X86) || \
    defined(MOZ_THREADSTACKHELPER_X64)
// On these architectures, the stack grows downwards (toward lower addresses).
#define MOZ_THREADSTACKHELPER_STACK_GROWS_DOWN
#else
#error "Unsupported architecture"
#endif
#endif // MOZ_THREADSTACKHELPER_NATIVE

namespace mozilla {

void
ThreadStackHelper::Startup()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInitialized) {
    // TODO: centralize signal number allocation
    sFillStackSignum = SIGRTMIN + 4;
    if (sFillStackSignum > SIGRTMAX) {
      // Leave uninitialized
      MOZ_ASSERT(false);
      return;
    }
    struct sigaction sigact = {};
    sigact.sa_sigaction = FillStackHandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO | SA_RESTART;
    MOZ_ALWAYS_TRUE(!::sigaction(sFillStackSignum, &sigact, nullptr));
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
    struct sigaction sigact = {};
    sigact.sa_handler = SIG_DFL;
    MOZ_ALWAYS_TRUE(!::sigaction(sFillStackSignum, &sigact, nullptr));
  }
  sInitialized--;
#endif
}

ThreadStackHelper::ThreadStackHelper()
  : mStackToFill(nullptr)
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  , mPseudoStack(profiler_get_pseudo_stack())
  , mMaxStackSize(Stack::sMaxInlineStorage)
  , mMaxBufferSize(512)
#endif
{
#if defined(XP_LINUX)
  MOZ_ALWAYS_TRUE(!::sem_init(&mSem, 0, 0));
  mThreadID = ::syscall(SYS_gettid);
#elif defined(XP_WIN)
  mInitialized = !!::DuplicateHandle(
    ::GetCurrentProcess(), ::GetCurrentThread(),
    ::GetCurrentProcess(), &mThreadID,
    THREAD_SUSPEND_RESUME
#ifdef MOZ_THREADSTACKHELPER_NATIVE
    | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION
#endif
    , FALSE, 0);
  mStackTop = profiler_get_stack_top();
  MOZ_ASSERT(mInitialized);
#elif defined(XP_MACOSX)
  mThreadID = mach_thread_self();
#endif
}

ThreadStackHelper::~ThreadStackHelper()
{
#if defined(XP_LINUX)
  MOZ_ALWAYS_TRUE(!::sem_destroy(&mSem));
#elif defined(XP_WIN)
  if (mInitialized) {
    MOZ_ALWAYS_TRUE(!!::CloseHandle(mThreadID));
  }
#endif
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
ThreadStackHelper::GetStacksInternal(Stack* aStack, NativeStack* aNativeStack)
{
  // Always run PrepareStackBuffer first to clear aStack
  if (aStack && !PrepareStackBuffer(*aStack)) {
    // Skip and return empty aStack
    return;
  }

  ScopedSetPtr<Stack> stackPtr(mStackToFill, aStack);

#if defined(XP_LINUX)
  if (!sInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  if (aStack) {
    siginfo_t uinfo = {};
    uinfo.si_signo = sFillStackSignum;
    uinfo.si_code = SI_QUEUE;
    uinfo.si_pid = getpid();
    uinfo.si_uid = getuid();
    uinfo.si_value.sival_ptr = this;
    if (::syscall(SYS_rt_tgsigqueueinfo, uinfo.si_pid,
                  mThreadID, sFillStackSignum, &uinfo)) {
      // rt_tgsigqueueinfo was added in Linux 2.6.31.
      // Could have failed because the syscall did not exist.
      return;
    }
    MOZ_ALWAYS_TRUE(!::sem_wait(&mSem));
  }

#elif defined(XP_WIN)
  if (!mInitialized) {
    MOZ_ASSERT(false);
    return;
  }

  // NOTE: We can only perform frame pointer stack walking on non win64
  // platforms, because Win64 always omits frame pointers. We don't want to use
  // MozStackWalk here, so we just skip collecting stacks entirely.
#ifndef MOZ_THREADSTACKHELPER_X64
  if (aNativeStack) {
    aNativeStack->reserve(Telemetry::HangStack::sMaxNativeFrames);
  }
#endif

  if (::SuspendThread(mThreadID) == DWORD(-1)) {
    MOZ_ASSERT(false);
    return;
  }

  // SuspendThread is asynchronous, so the thread may still be running. Use
  // GetThreadContext to ensure it's really suspended.
  // See https://blogs.msdn.microsoft.com/oldnewthing/20150205-00/?p=44743.
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_CONTROL;
  if (::GetThreadContext(mThreadID, &context)) {
    if (aStack) {
      FillStackBuffer();
    }

#ifndef MOZ_THREADSTACKHELPER_X64
    if (aNativeStack) {
      auto callback = [](uint32_t, void* aPC, void*, void* aClosure) {
        NativeStack* stack = static_cast<NativeStack*>(aClosure);
        stack->push_back(reinterpret_cast<uintptr_t>(aPC));
      };

      // Now we need to get our frame pointer, our stack pointer, and our stack
      // top. Rather than registering and storing the stack tops ourselves, we use
      // the gecko profiler to look it up.
      void** framePointer = reinterpret_cast<void**>(context.Ebp);
      void** stackPointer = reinterpret_cast<void**>(context.Esp);

      MOZ_ASSERT(mStackTop, "The thread should be registered by the profiler");

      // Double check that the values we pulled for the thread make sense before
      // walking the stack.
      if (mStackTop && framePointer >= stackPointer && framePointer < mStackTop) {
        // NOTE: In bug 1346415 this was changed to use FramePointerStackWalk.
        // This was done because lowering the background hang timer threshold
        // would cause it to fire on infra early during the boot process, causing
        // a deadlock in MozStackWalk when the target thread was holding the
        // windows-internal lock on the function table, as it would be suspended
        // before we tried to grab the lock to walk its stack.
        //
        // FramePointerStackWalk is implemented entirely in userspace and thus
        // doesn't have the same issues with deadlocking. Unfortunately as 64-bit
        // windows is not guaranteed to have frame pointers, the stack walking
        // code is only enabled on 32-bit windows builds (bug 1357829).
        FramePointerStackWalk(callback, /* skipFrames */ 0,
                              /* maxFrames */ Telemetry::HangStack::sMaxNativeFrames,
                              reinterpret_cast<void*>(aNativeStack), framePointer,
                              mStackTop);
      }
    }
#endif
  }

  MOZ_ALWAYS_TRUE(::ResumeThread(mThreadID) != DWORD(-1));

#elif defined(XP_MACOSX)
# if defined(MOZ_VALGRIND) && defined(RUNNING_ON_VALGRIND)
  if (RUNNING_ON_VALGRIND) {
    /* thread_suspend and thread_resume sometimes hang runs on Valgrind,
       for unknown reasons.  So, just avoid them.  See bug 1100911. */
    return;
  }
# endif

  if (aStack) {
    if (::thread_suspend(mThreadID) != KERN_SUCCESS) {
      MOZ_ASSERT(false);
      return;
    }

    FillStackBuffer();

    MOZ_ALWAYS_TRUE(::thread_resume(mThreadID) == KERN_SUCCESS);
  }

#endif
}

void
ThreadStackHelper::GetNativeStack(NativeStack& aNativeStack)
{
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  GetStacksInternal(nullptr, &aNativeStack);
#endif // MOZ_THREADSTACKHELPER_NATIVE
}

void
ThreadStackHelper::GetPseudoAndNativeStack(Stack& aStack, NativeStack& aNativeStack)
{
  GetStacksInternal(&aStack, &aNativeStack);
}

#ifdef XP_LINUX

int ThreadStackHelper::sInitialized;
int ThreadStackHelper::sFillStackSignum;

void
ThreadStackHelper::FillStackHandler(int aSignal, siginfo_t* aInfo,
                                    void* aContext)
{
  ThreadStackHelper* const helper =
    reinterpret_cast<ThreadStackHelper*>(aInfo->si_value.sival_ptr);
  helper->FillStackBuffer();
  ::sem_post(&helper->mSem);
}

#endif // XP_LINUX

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
ThreadStackHelper::AppendJSEntry(const volatile js::ProfileEntry* aEntry,
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
  MOZ_ASSERT(mStackToFill->empty());

#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  size_t reservedSize = mStackToFill->capacity();
  size_t reservedBufferSize = mStackToFill->AvailableBufferSize();
  intptr_t availableBufferSize = intptr_t(reservedBufferSize);

  // Go from front to back
  const volatile js::ProfileEntry* entry = mPseudoStack->mStack;
  const volatile js::ProfileEntry* end = entry + mPseudoStack->stackSize();
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
