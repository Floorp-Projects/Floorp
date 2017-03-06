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

// Memory profile
#include "nsMemoryReporterManager.h"

class PlatformData {
 public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need to make a copy of the handle because we are
  // going to use it in the sampler thread. Using GetThreadHandle() will
  // not work in this case. We're using OpenThread because DuplicateHandle
  // for some reason doesn't work in Chrome's sandbox.
  explicit PlatformData(int aThreadId) : profiled_thread_(OpenThread(
                                               THREAD_GET_CONTEXT |
                                               THREAD_SUSPEND_RESUME |
                                               THREAD_QUERY_INFORMATION,
                                               false,
                                               aThreadId)) {}

  ~PlatformData() {
    if (profiled_thread_ != nullptr) {
      CloseHandle(profiled_thread_);
      profiled_thread_ = nullptr;
    }
  }

  HANDLE profiled_thread() { return profiled_thread_; }

 private:
  HANDLE profiled_thread_;
};

UniquePlatformData
AllocPlatformData(int aThreadId)
{
  return UniquePlatformData(new PlatformData(aThreadId));
}

void
PlatformDataDestructor::operator()(PlatformData* aData)
{
  delete aData;
}

uintptr_t
GetThreadHandle(PlatformData* aData)
{
  return (uintptr_t) aData->profiled_thread();
}

static const HANDLE kNoThread = INVALID_HANDLE_VALUE;

// The sampler thread controls sampling and runs whenever the profiler is
// active. It periodically runs through all registered threads, finds those
// that should be sampled, then pauses and samples them.
class SamplerThread
{
private:
  static unsigned int __stdcall ThreadEntry(void* aArg) {
    auto thread = static_cast<SamplerThread*>(aArg);
    thread->Run();
    return 0;
  }

public:
  explicit SamplerThread(double aInterval)
    : mInterval(std::max(1, int(floor(aInterval + 0.5))))
  {
    // Create a new thread. It is important to use _beginthreadex() instead of
    // the Win32 function CreateThread(), because the CreateThread() does not
    // initialize thread-specific structures in the C runtime library.
    mThread = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr,
                       /* stack_size */ 0,
                       ThreadEntry,
                       this,
                       /* initflag */ 0,
                       (unsigned int*) &mThreadId));
    if (mThread == 0) {
      MOZ_CRASH("_beginthreadex failed");
    }
  }

  ~SamplerThread() {
    // Close our own handle for the thread.
    if (mThread != kNoThread) {
      CloseHandle(mThread);
    }
  }

  void Join() {
    if (mThreadId != Thread::GetCurrentId()) {
      WaitForSingleObject(mThread, INFINITE);
    }
  }

  static void StartSampler(double aInterval) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    MOZ_RELEASE_ASSERT(!sInstance);

    sInstance = new SamplerThread(aInterval);
  }

  static void StopSampler() {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());

    sInstance->Join();
    delete sInstance;
    sInstance = nullptr;
  }

  void Run() {
    // This function runs on the sampler thread.

    // By default we'll not adjust the timer resolution which tends to be around
    // 16ms. However, if the requested interval is sufficiently low we'll try to
    // adjust the resolution to match.
    if (mInterval < 10)
        ::timeBeginPeriod(mInterval);

    while (gIsActive) {
      gBuffer->deleteExpiredStoredMarkers();

      if (!gIsPaused) {
        mozilla::StaticMutexAutoLock lock(gRegisteredThreadsMutex);

        bool isFirstProfiledThread = true;
        for (uint32_t i = 0; i < gRegisteredThreads->size(); i++) {
          ThreadInfo* info = (*gRegisteredThreads)[i];

          // This will be null if we're not interested in profiling this thread.
          if (!info->HasProfile() || info->IsPendingDelete()) {
            continue;
          }

          if (info->Stack()->CanDuplicateLastSampleDueToSleep() &&
              gBuffer->DuplicateLastSample(info->ThreadId(), gStartTime)) {
            continue;
          }

          info->UpdateThreadResponsiveness();

          SampleContext(info, isFirstProfiledThread);
          isFirstProfiledThread = false;
        }
      }
      ::Sleep(mInterval);
    }

    // disable any timer resolution changes we've made
    if (mInterval < 10)
        ::timeEndPeriod(mInterval);
  }

  void SampleContext(ThreadInfo* aThreadInfo, bool isFirstProfiledThread)
  {
    uintptr_t thread = GetThreadHandle(aThreadInfo->GetPlatformData());
    HANDLE profiled_thread = reinterpret_cast<HANDLE>(thread);
    if (profiled_thread == nullptr)
      return;

    // Context used for sampling the register state of the profiled thread.
    CONTEXT context;
    memset(&context, 0, sizeof(context));

    TickSample sample;

    // Grab the timestamp before pausing the thread, to avoid deadlocks.
    sample.timestamp = mozilla::TimeStamp::Now();
    sample.threadInfo = aThreadInfo;

    if (isFirstProfiledThread && gProfileMemory) {
      sample.rssMemory = nsMemoryReporterManager::ResidentFast();
    } else {
      sample.rssMemory = 0;
    }

    // Unique Set Size is not supported on Windows.
    sample.ussMemory = 0;

    static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (SuspendThread(profiled_thread) == kSuspendFailed)
      return;

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

#if defined(GP_ARCH_amd64)
    sample.pc = reinterpret_cast<Address>(context.Rip);
    sample.sp = reinterpret_cast<Address>(context.Rsp);
    sample.fp = reinterpret_cast<Address>(context.Rbp);
#else
    sample.pc = reinterpret_cast<Address>(context.Eip);
    sample.sp = reinterpret_cast<Address>(context.Esp);
    sample.fp = reinterpret_cast<Address>(context.Ebp);
#endif

    sample.context = &context;

    Tick(gBuffer, &sample);

    ResumeThread(profiled_thread);
  }

private:
  HANDLE mThread;
  Thread::tid_t mThreadId;

  // The interval between samples, measured in milliseconds.
  const int mInterval;

  static SamplerThread* sInstance;

  SamplerThread(const SamplerThread&) = delete;
  void operator=(const SamplerThread&) = delete;
};

SamplerThread* SamplerThread::sInstance = nullptr;

static void
PlatformInit()
{
}

static void
PlatformStart(double aInterval)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  SamplerThread::StartSampler(aInterval);
}

static void
PlatformStop()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  SamplerThread::StopSampler();
}

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return GetCurrentThreadId();
}

void TickSample::PopulateContext(void* aContext)
{
  MOZ_ASSERT(aContext);
  CONTEXT* pContext = reinterpret_cast<CONTEXT*>(aContext);
  context = pContext;
  RtlCaptureContext(pContext);

#if defined(GP_ARCH_amd64)
  pc = reinterpret_cast<Address>(pContext->Rip);
  sp = reinterpret_cast<Address>(pContext->Rsp);
  fp = reinterpret_cast<Address>(pContext->Rbp);
#elif defined(GP_ARCH_x86)
  pc = reinterpret_cast<Address>(pContext->Eip);
  sp = reinterpret_cast<Address>(pContext->Esp);
  fp = reinterpret_cast<Address>(pContext->Ebp);
#endif
}

