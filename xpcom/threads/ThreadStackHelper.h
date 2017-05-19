/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ThreadStackHelper_h
#define mozilla_ThreadStackHelper_h

#include "mozilla/ThreadHangStats.h"
#include "js/ProfilingStack.h"

#include <stddef.h>

#if defined(XP_LINUX)
#include <signal.h>
#include <semaphore.h>
#include <sys/types.h>
#elif defined(XP_WIN)
#include <windows.h>
#elif defined(XP_MACOSX)
#include <mach/mach.h>
#endif

// Support pseudostack on these platforms.
#if defined(XP_LINUX) || defined(XP_WIN) || defined(XP_MACOSX)
#  ifdef MOZ_GECKO_PROFILER
#    define MOZ_THREADSTACKHELPER_PSEUDO
#  endif
#endif

#if defined(MOZ_THREADSTACKHELPER_PSEUDO) && defined(XP_WIN)
#  define MOZ_THREADSTACKHELPER_NATIVE
#  if defined(__i386__) || defined(_M_IX86)
#    define MOZ_THREADSTACKHELPER_X86
#  elif defined(__x86_64__) || defined(_M_X64)
#    define MOZ_THREADSTACKHELPER_X64
#  elif defined(__arm__) || defined(_M_ARM)
#    define MOZ_THREADSTACKHELPER_ARM
#  else
     // Unsupported architecture
#    undef MOZ_THREADSTACKHELPER_NATIVE
#  endif
#endif

namespace mozilla {

/**
 * ThreadStackHelper is used to retrieve the profiler pseudo-stack of a
 * thread, as an alternative of using the profiler to take a profile.
 * The target thread first declares an ThreadStackHelper instance;
 * then another thread can call ThreadStackHelper::GetStack to retrieve
 * the pseudo-stack of the target thread at that instant.
 *
 * Only non-copying labels are included in the stack, which means labels
 * with custom text and markers are not included.
 */
class ThreadStackHelper
{
public:
  typedef Telemetry::HangStack Stack;

  // When a native stack is gathered, this vector holds the raw program counter
  // values that FramePointerStackWalk will return to us after it walks the
  // stack. When gathering the Telemetry payload, Telemetry will take care of
  // mapping these program counters to proper addresses within modules.
  typedef Telemetry::NativeHangStack NativeStack;

private:
  Stack* mStackToFill;
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  const PseudoStack* const mPseudoStack;
  size_t mMaxStackSize;
  size_t mMaxBufferSize;
#endif

  bool PrepareStackBuffer(Stack& aStack);
  void FillStackBuffer();
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  const char* AppendJSEntry(const volatile js::ProfileEntry* aEntry,
                            intptr_t& aAvailableBufferSize,
                            const char* aPrevLabel);
#endif

public:
  /**
   * Initialize ThreadStackHelper. Must be called from main thread.
   */
  static void Startup();
  /**
   * Uninitialize ThreadStackHelper. Must be called from main thread.
   */
  static void Shutdown();

  /**
   * Create a ThreadStackHelper instance targeting the current thread.
   */
  ThreadStackHelper();

  ~ThreadStackHelper();

  /**
   * Retrieve the current pseudostack of the thread associated
   * with this ThreadStackHelper.
   *
   * @param aStack Stack instance to be filled.
   */
  void GetPseudoStack(Stack& aStack);

  /**
   * Retrieve the current native stack of the thread associated
   * with this ThreadStackHelper.
   *
   * @param aNativeStack NativeStack instance to be filled.
   */
  void GetNativeStack(NativeStack& aNativeStack);

  /**
   * Retrieve the current pseudostack and native stack of the thread associated
   * with this ThreadStackHelper. This method only pauses the target thread once
   * to get both stacks.
   *
   * @param aStack        Stack instance to be filled with the pseudostack.
   * @param aNativeStack  NativeStack instance to be filled with the native stack.
   */
  void GetPseudoAndNativeStack(Stack& aStack, NativeStack& aNativeStack);

private:
  // Fill in the passed aStack and aNativeStack datastructures with backtraces.
  // If only aStack needs to be collected, nullptr may be passed for
  // aNativeStack, and vice versa.
  void GetStacksInternal(Stack* aStack, NativeStack* aNativeStack);
#if defined(XP_LINUX)
private:
  static int sInitialized;
  static int sFillStackSignum;

  static void FillStackHandler(int aSignal, siginfo_t* aInfo, void* aContext);

  sem_t mSem;
  pid_t mThreadID;

#elif defined(XP_WIN)
private:
  bool mInitialized;
  HANDLE mThreadID;
  void* mStackTop;

#elif defined(XP_MACOSX)
private:
  thread_act_t mThreadID;

#endif
};

} // namespace mozilla

#endif // mozilla_ThreadStackHelper_h
