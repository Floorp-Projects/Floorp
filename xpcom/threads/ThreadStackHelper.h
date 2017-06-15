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

// Support pseudostack and native stack on these platforms.
#if defined(XP_LINUX) || defined(XP_WIN) || defined(XP_MACOSX)
#  ifdef MOZ_GECKO_PROFILER
#    define MOZ_THREADSTACKHELPER_PSEUDO
#    define MOZ_THREADSTACKHELPER_NATIVE
#  endif
#endif

// NOTE: Currently, due to a problem with LUL stackwalking initialization taking
// a long time (bug 1365309), we don't perform pseudostack or native stack
// walking on Linux.
#if defined(XP_LINUX)
#  undef MOZ_THREADSTACKHELPER_NATIVE
#  undef MOZ_THREADSTACKHELPER_PSEUDO
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
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  Stack* mStackToFill;
  const PseudoStack* const mPseudoStack;
  size_t mMaxStackSize;
  size_t mMaxBufferSize;
#endif
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  NativeStack* mNativeStackToFill;
#endif

  bool PrepareStackBuffer(Stack& aStack);
  void FillStackBuffer();
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  const char* AppendJSEntry(const js::ProfileEntry* aEntry,
                            intptr_t& aAvailableBufferSize,
                            const char* aPrevLabel);
#endif

public:
  /**
   * Create a ThreadStackHelper instance targeting the current thread.
   */
  ThreadStackHelper();

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

  // The profiler's unique thread identifier for the target thread.
  int mThreadId;
};

} // namespace mozilla

#endif // mozilla_ThreadStackHelper_h
