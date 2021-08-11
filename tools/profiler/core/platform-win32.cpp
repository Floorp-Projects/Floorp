/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of Google, Inc. nor the names of its contributors
//    may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include <windows.h>
#include <mmsystem.h>
#include <process.h>

#include "nsWindowsDllInterceptor.h"
#include "mozilla/StackWalk_windows.h"
#include "mozilla/WindowsVersion.h"

void* GetStackTop(void* aGuess) {
  PNT_TIB pTib = reinterpret_cast<PNT_TIB>(NtCurrentTeb());
  return reinterpret_cast<void*>(pTib->StackBase);
}

static void PopulateRegsFromContext(Registers& aRegs, CONTEXT* aContext) {
#if defined(GP_ARCH_amd64)
  aRegs.mPC = reinterpret_cast<Address>(aContext->Rip);
  aRegs.mSP = reinterpret_cast<Address>(aContext->Rsp);
  aRegs.mFP = reinterpret_cast<Address>(aContext->Rbp);
#elif defined(GP_ARCH_x86)
  aRegs.mPC = reinterpret_cast<Address>(aContext->Eip);
  aRegs.mSP = reinterpret_cast<Address>(aContext->Esp);
  aRegs.mFP = reinterpret_cast<Address>(aContext->Ebp);
#elif defined(GP_ARCH_arm64)
  aRegs.mPC = reinterpret_cast<Address>(aContext->Pc);
  aRegs.mSP = reinterpret_cast<Address>(aContext->Sp);
  aRegs.mFP = reinterpret_cast<Address>(aContext->Fp);
#else
#  error "bad arch"
#endif
  aRegs.mLR = 0;
}

// Gets a real (i.e. not pseudo) handle for the current thread, with the
// permissions needed for profiling.
// @return a real HANDLE for the current thread.
static HANDLE GetRealCurrentThreadHandleForProfiling() {
  HANDLE realCurrentThreadHandle;
  if (!::DuplicateHandle(
          ::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(),
          &realCurrentThreadHandle,
          THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
          FALSE, 0)) {
    return nullptr;
  }

  return realCurrentThreadHandle;
}

class PlatformData {
 public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need a real handle because we are going to use it in
  // the sampler thread.
  explicit PlatformData(ProfilerThreadId aThreadId)
      : mProfiledThread(GetRealCurrentThreadHandleForProfiling()) {
    MOZ_ASSERT(aThreadId == profiler_current_thread_id());
    MOZ_COUNT_CTOR(PlatformData);
  }

  ~PlatformData() {
    if (mProfiledThread != nullptr) {
      CloseHandle(mProfiledThread);
      mProfiledThread = nullptr;
    }
    MOZ_COUNT_DTOR(PlatformData);
  }

  HANDLE ProfiledThread() { return mProfiledThread; }

  RunningTimes& PreviousThreadRunningTimesRef() {
    return mPreviousThreadRunningTimes;
  }

 private:
  HANDLE mProfiledThread;
  RunningTimes mPreviousThreadRunningTimes;
};

#if defined(USE_MOZ_STACK_WALK)
HANDLE
GetThreadHandle(PlatformData* aData) { return aData->ProfiledThread(); }
#endif

static const HANDLE kNoThread = INVALID_HANDLE_VALUE;

////////////////////////////////////////////////////////////////////////
// BEGIN Sampler target specifics

Sampler::Sampler(PSLockRef aLock) {}

void Sampler::Disable(PSLockRef aLock) {}

static void StreamMetaPlatformSampleUnits(PSLockRef aLock,
                                          SpliceableJSONWriter& aWriter) {
  aWriter.StringProperty("threadCPUDelta", "variable CPU cycles");
}

static RunningTimes GetThreadRunningTimesDiff(
    PSLockRef aLock, const RegisteredThread& aRegisteredThread) {
  AUTO_PROFILER_STATS(GetRunningTimes);

  PlatformData* const platformData = aRegisteredThread.GetPlatformData();
  MOZ_RELEASE_ASSERT(platformData);
  const HANDLE profiledThread = platformData->ProfiledThread();

  const RunningTimes newRunningTimes = GetRunningTimesWithTightTimestamp(
      [profiledThread](RunningTimes& aRunningTimes) {
        AUTO_PROFILER_STATS(GetRunningTimes_QueryThreadCycleTime);
        if (ULONG64 cycles;
            QueryThreadCycleTime(profiledThread, &cycles) != 0) {
          aRunningTimes.ResetThreadCPUDelta(cycles);
        } else {
          aRunningTimes.ClearThreadCPUDelta();
        }
      });

  const RunningTimes diff =
      newRunningTimes - platformData->PreviousThreadRunningTimesRef();
  platformData->PreviousThreadRunningTimesRef() = newRunningTimes;
  return diff;
}

static void ClearThreadRunningTimes(PSLockRef aLock,
                                    const RegisteredThread& aRegisteredThread) {
  PlatformData* const platformData = aRegisteredThread.GetPlatformData();
  MOZ_RELEASE_ASSERT(platformData);
  platformData->PreviousThreadRunningTimesRef().Clear();
}

template <typename Func>
void Sampler::SuspendAndSampleAndResumeThread(
    PSLockRef aLock, const RegisteredThread& aRegisteredThread,
    const TimeStamp& aNow, const Func& aProcessRegs) {
  HANDLE profiled_thread =
      aRegisteredThread.GetPlatformData()->ProfiledThread();
  if (profiled_thread == nullptr) {
    return;
  }

  // Context used for sampling the register state of the profiled thread.
  CONTEXT context;
  memset(&context, 0, sizeof(context));

  //----------------------------------------------------------------//
  // Suspend the samplee thread and get its context.

  static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
  if (SuspendThread(profiled_thread) == kSuspendFailed) {
    return;
  }

  // SuspendThread is asynchronous, so the thread may still be running.
  // Call GetThreadContext first to ensure the thread is really suspended.
  // See https://blogs.msdn.microsoft.com/oldnewthing/20150205-00/?p=44743.

  // Using only CONTEXT_CONTROL is faster but on 64-bit it causes crashes in
  // RtlVirtualUnwind (see bug 1120126) so we set all the flags.
#if defined(GP_ARCH_amd64)
  context.ContextFlags = CONTEXT_FULL;
#else
  context.ContextFlags = CONTEXT_CONTROL;
#endif
  if (!GetThreadContext(profiled_thread, &context)) {
    ResumeThread(profiled_thread);
    return;
  }

  //----------------------------------------------------------------//
  // Sample the target thread.

  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  //
  // The profiler's "critical section" begins here.  We must be very careful
  // what we do here, or risk deadlock.  See the corresponding comment in
  // platform-linux-android.cpp for details.

  Registers regs;
  PopulateRegsFromContext(regs, &context);
  aProcessRegs(regs, aNow);

  //----------------------------------------------------------------//
  // Resume the target thread.

  ResumeThread(profiled_thread);

  // The profiler's critical section ends here.
  //
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
}

// END Sampler target specifics
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// BEGIN SamplerThread target specifics

static unsigned int __stdcall ThreadEntry(void* aArg) {
  auto thread = static_cast<SamplerThread*>(aArg);
  thread->Run();
  return 0;
}

SamplerThread::SamplerThread(PSLockRef aLock, uint32_t aActivityGeneration,
                             double aIntervalMilliseconds,
                             bool aStackWalkEnabled,
                             bool aNoTimerResolutionChange)
    : mSampler(aLock),
      mActivityGeneration(aActivityGeneration),
      mIntervalMicroseconds(
          std::max(1, int(floor(aIntervalMilliseconds * 1000 + 0.5)))),
      mNoTimerResolutionChange(aNoTimerResolutionChange) {
  if ((!aNoTimerResolutionChange) && (mIntervalMicroseconds < 10 * 1000)) {
    // By default the timer resolution (which tends to be 1/64Hz, around 16ms)
    // is not changed. However, if the requested interval is sufficiently low,
    // the resolution will be adjusted to match. Note that this affects all
    // timers in Firefox, and could therefore hide issues while profiling. This
    // change may be prevented with the "notimerresolutionchange" feature.
    ::timeBeginPeriod(mIntervalMicroseconds / 1000);
  }

  // Create a new thread. It is important to use _beginthreadex() instead of
  // the Win32 function CreateThread(), because the CreateThread() does not
  // initialize thread-specific structures in the C runtime library.
  mThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr,
                                                    /* stack_size */ 0,
                                                    ThreadEntry, this,
                                                    /* initflag */ 0, nullptr));
  if (mThread == 0) {
    MOZ_CRASH("_beginthreadex failed");
  }
}

SamplerThread::~SamplerThread() {
  WaitForSingleObject(mThread, INFINITE);

  // Close our own handle for the thread.
  if (mThread != kNoThread) {
    CloseHandle(mThread);
  }

  // Just in the unlikely case some callbacks were added between the end of the
  // thread and now.
  InvokePostSamplingCallbacks(std::move(mPostSamplingCallbackList),
                              SamplingState::JustStopped);
}

void SamplerThread::SleepMicro(uint32_t aMicroseconds) {
  // For now, keep the old behaviour of minimum Sleep(1), even for
  // smaller-than-usual sleeps after an overshoot, unless the user has
  // explicitly opted into a sub-millisecond profiler interval.
  if (mIntervalMicroseconds >= 1000) {
    ::Sleep(std::max(1u, aMicroseconds / 1000));
  } else {
    TimeStamp start = TimeStamp::Now();
    TimeStamp end = start + TimeDuration::FromMicroseconds(aMicroseconds);

    // First, sleep for as many whole milliseconds as possible.
    if (aMicroseconds >= 1000) {
      ::Sleep(aMicroseconds / 1000);
    }

    // Then, spin until enough time has passed.
    while (TimeStamp::Now() < end) {
      YieldProcessor();
    }
  }
}

void SamplerThread::Stop(PSLockRef aLock) {
  if ((!mNoTimerResolutionChange) && (mIntervalMicroseconds < 10 * 1000)) {
    // Disable any timer resolution changes we've made. Do it now while
    // gPSMutex is locked, i.e. before any other SamplerThread can be created
    // and call ::timeBeginPeriod().
    //
    // It's safe to do this now even though this SamplerThread is still alive,
    // because the next time the main loop of Run() iterates it won't get past
    // the mActivityGeneration check, and so it won't make any more ::Sleep()
    // calls.
    ::timeEndPeriod(mIntervalMicroseconds / 1000);
  }

  mSampler.Disable(aLock);
}

// END SamplerThread target specifics
////////////////////////////////////////////////////////////////////////

static void PlatformInit(PSLockRef aLock) {}

#if defined(HAVE_NATIVE_UNWIND)
void Registers::SyncPopulate() {
  CONTEXT context;
  RtlCaptureContext(&context);
  PopulateRegsFromContext(*this, &context);
}
#endif

#if defined(GP_PLAT_amd64_windows)

// Use InitializeWin64ProfilerHooks from the base profiler.

namespace mozilla {
namespace baseprofiler {
MFBT_API void InitializeWin64ProfilerHooks();
}  // namespace baseprofiler
}  // namespace mozilla

using mozilla::baseprofiler::InitializeWin64ProfilerHooks;

#endif  // defined(GP_PLAT_amd64_windows)
