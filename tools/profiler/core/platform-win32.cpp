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

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return GetCurrentThreadId();
}

class PlatformData
{
public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need to make a copy of the handle because we are
  // going to use it in the sampler thread. Using GetThreadHandle() will
  // not work in this case. We're using OpenThread because DuplicateHandle
  // for some reason doesn't work in Chrome's sandbox.
  explicit PlatformData(int aThreadId)
    : mProfiledThread(OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME |
                                  THREAD_QUERY_INFORMATION,
                                  false,
                                  aThreadId))
  {
    MOZ_COUNT_CTOR(PlatformData);
  }

  ~PlatformData()
  {
    if (mProfiledThread != nullptr) {
      CloseHandle(mProfiledThread);
      mProfiledThread = nullptr;
    }
    MOZ_COUNT_DTOR(PlatformData);
  }

  HANDLE ProfiledThread() { return mProfiledThread; }

private:
  HANDLE mProfiledThread;
};

uintptr_t
GetThreadHandle(PlatformData* aData)
{
  return (uintptr_t) aData->ProfiledThread();
}

static const HANDLE kNoThread = INVALID_HANDLE_VALUE;

////////////////////////////////////////////////////////////////////////
// BEGIN Sampler target specifics

Sampler::Sampler(PSLockRef aLock)
{
}

void
Sampler::Disable(PSLockRef aLock)
{
}

void
Sampler::SuspendAndSampleAndResumeThread(PSLockRef aLock,
                                         TickController& aController,
                                         TickSample& aSample)
{
  HANDLE profiled_thread = aSample.mPlatformData->ProfiledThread();
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

#if defined(GP_ARCH_amd64)
  aSample.mPC = reinterpret_cast<Address>(context.Rip);
  aSample.mSP = reinterpret_cast<Address>(context.Rsp);
  aSample.mFP = reinterpret_cast<Address>(context.Rbp);
#else
  aSample.mPC = reinterpret_cast<Address>(context.Eip);
  aSample.mSP = reinterpret_cast<Address>(context.Esp);
  aSample.mFP = reinterpret_cast<Address>(context.Ebp);
#endif

  aController.Tick(aLock, aSample);

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

static unsigned int __stdcall
ThreadEntry(void* aArg)
{
  auto thread = static_cast<SamplerThread*>(aArg);
  thread->Run();
  return 0;
}

SamplerThread::SamplerThread(PSLockRef aLock, uint32_t aActivityGeneration,
                             double aIntervalMilliseconds)
    : Sampler(aLock)
    , mActivityGeneration(aActivityGeneration)
    , mIntervalMicroseconds(
        std::max(1, int(floor(aIntervalMilliseconds * 1000 + 0.5))))
{
  // By default we'll not adjust the timer resolution which tends to be
  // around 16ms. However, if the requested interval is sufficiently low
  // we'll try to adjust the resolution to match.
  if (mIntervalMicroseconds < 10*1000) {
    ::timeBeginPeriod(mIntervalMicroseconds / 1000);
  }

  // Create a new thread. It is important to use _beginthreadex() instead of
  // the Win32 function CreateThread(), because the CreateThread() does not
  // initialize thread-specific structures in the C runtime library.
  mThread = reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr,
                     /* stack_size */ 0,
                     ThreadEntry,
                     this,
                     /* initflag */ 0,
                     nullptr));
  if (mThread == 0) {
    MOZ_CRASH("_beginthreadex failed");
  }
}

SamplerThread::~SamplerThread()
{
  WaitForSingleObject(mThread, INFINITE);

  // Close our own handle for the thread.
  if (mThread != kNoThread) {
    CloseHandle(mThread);
  }
}

void
SamplerThread::SleepMicro(uint32_t aMicroseconds)
{
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
      _mm_pause();
    }
  }
}

void
SamplerThread::Stop(PSLockRef aLock)
{
  // Disable any timer resolution changes we've made. Do it now while
  // gPSMutex is locked, i.e. before any other SamplerThread can be created
  // and call ::timeBeginPeriod().
  //
  // It's safe to do this now even though this SamplerThread is still alive,
  // because the next time the main loop of Run() iterates it won't get past
  // the mActivityGeneration check, and so it won't make any more ::Sleep()
  // calls.
  if (mIntervalMicroseconds < 10 * 1000) {
    ::timeEndPeriod(mIntervalMicroseconds / 1000);
  }

  Sampler::Disable(aLock);
}

// END SamplerThread target specifics
////////////////////////////////////////////////////////////////////////

static void
PlatformInit(PSLockRef aLock)
{
}

void
TickSample::PopulateContext()
{
  MOZ_ASSERT(mIsSynchronous);

  CONTEXT context;
  RtlCaptureContext(&context);

#if defined(GP_ARCH_amd64)
  mPC = reinterpret_cast<Address>(context.Rip);
  mSP = reinterpret_cast<Address>(context.Rsp);
  mFP = reinterpret_cast<Address>(context.Rbp);
#elif defined(GP_ARCH_x86)
  mPC = reinterpret_cast<Address>(context.Eip);
  mSP = reinterpret_cast<Address>(context.Esp);
  mFP = reinterpret_cast<Address>(context.Ebp);
#endif
}

