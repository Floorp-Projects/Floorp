/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ThreadStackHelper_h
#define mozilla_ThreadStackHelper_h

#include "mozilla/ThreadHangStats.h"

#include "GeckoProfiler.h"

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

private:
#ifdef MOZ_ENABLE_PROFILER_SPS
  const PseudoStack* const mPseudoStack;
#endif
  Stack* mStackToFill;
  size_t mMaxStackSize;
  size_t mMaxBufferSize;

  bool PrepareStackBuffer(Stack& aStack);
  void FillStackBuffer();
#ifdef MOZ_ENABLE_PROFILER_SPS
  const char* AppendJSEntry(const volatile StackEntry* aEntry,
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
  void GetStack(Stack& aStack);

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

#elif defined(XP_MACOSX)
private:
  thread_act_t mThreadID;

#endif
};

} // namespace mozilla

#endif // mozilla_ThreadStackHelper_h
