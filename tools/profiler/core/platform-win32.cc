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
#include "platform.h"
#include "ProfileEntry.h"
#include "ThreadInfo.h"
#include "ThreadResponsiveness.h"

// Memory profile
#include "nsMemoryReporterManager.h"

#include "mozilla/StackWalk_windows.h"


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
    if (profiled_thread_ != NULL) {
      CloseHandle(profiled_thread_);
      profiled_thread_ = NULL;
    }
  }

  HANDLE profiled_thread() { return profiled_thread_; }

 private:
  HANDLE profiled_thread_;
};

/* static */ auto
Sampler::AllocPlatformData(int aThreadId) -> UniquePlatformData
{
  return UniquePlatformData(new PlatformData(aThreadId));
}

void
Sampler::PlatformDataDestructor::operator()(PlatformData* aData)
{
  delete aData;
}

uintptr_t
Sampler::GetThreadHandle(PlatformData* aData)
{
  return (uintptr_t) aData->profiled_thread();
}

static const HANDLE kNoThread = INVALID_HANDLE_VALUE;

// SamplerThread objects are used for creating and running threads. When the
// Start() method is called the new thread starts running the Run() method in
// the new thread. The SamplerThread object should not be deallocated before
// the thread has terminated.
class SamplerThread
{
 public:
  // Initialize a Win32 thread object. The thread has an invalid thread
  // handle until it is started.
  SamplerThread(double interval, Sampler* sampler)
    : mStackSize(0)
    , mThread(kNoThread)
    , mSampler(sampler)
    , mInterval(interval)
  {
    mInterval = floor(interval + 0.5);
    if (mInterval <= 0) {
      mInterval = 1;
    }
  }

  ~SamplerThread() {
    // Close our own handle for the thread.
    if (mThread != kNoThread) {
      CloseHandle(mThread);
    }
  }

  static unsigned int __stdcall ThreadEntry(void* aArg) {
    SamplerThread* thread = reinterpret_cast<SamplerThread*>(aArg);
    thread->Run();
    return 0;
  }

  // Create a new thread. It is important to use _beginthreadex() instead of
  // the Win32 function CreateThread(), because the CreateThread() does not
  // initialize thread-specific structures in the C runtime library.
  void Start() {
    mThread = reinterpret_cast<HANDLE>(
        _beginthreadex(NULL,
                       static_cast<unsigned>(mStackSize),
                       ThreadEntry,
                       this,
                       0,
                       (unsigned int*) &mThreadId));
  }

  void Join() {
    if (mThreadId != Thread::GetCurrentId()) {
      WaitForSingleObject(mThread, INFINITE);
    }
  }

  static void StartSampler(Sampler* sampler) {
    if (mInstance == NULL) {
      mInstance = new SamplerThread(sampler->interval(), sampler);
      mInstance->Start();
    } else {
      MOZ_ASSERT(mInstance->mInterval == sampler->interval());
    }
  }

  static void StopSampler() {
    mInstance->Join();
    delete mInstance;
    mInstance = NULL;
  }

  void Run() {
    // By default we'll not adjust the timer resolution which tends to be around
    // 16ms. However, if the requested interval is sufficiently low we'll try to
    // adjust the resolution to match.
    if (mInterval < 10)
        ::timeBeginPeriod(mInterval);

    while (mSampler->IsActive()) {
      mSampler->DeleteExpiredMarkers();

      if (!mSampler->IsPaused()) {
        mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
        const std::vector<ThreadInfo*>& threads =
          mSampler->GetRegisteredThreads();
        bool isFirstProfiledThread = true;
        for (uint32_t i = 0; i < threads.size(); i++) {
          ThreadInfo* info = threads[i];

          // This will be null if we're not interested in profiling this thread.
          if (!info->hasProfile() || info->IsPendingDelete()) {
            continue;
          }

          PseudoStack::SleepState sleeping = info->Stack()->observeSleeping();
          if (sleeping == PseudoStack::SLEEPING_AGAIN) {
            info->DuplicateLastSample();
            continue;
          }

          info->UpdateThreadResponsiveness();

          SampleContext(mSampler, info, isFirstProfiledThread);
          isFirstProfiledThread = false;
        }
      }
      OS::Sleep(mInterval);
    }

    // disable any timer resolution changes we've made
    if (mInterval < 10)
        ::timeEndPeriod(mInterval);
  }

  void SampleContext(Sampler* sampler, ThreadInfo* aThreadInfo,
                     bool isFirstProfiledThread)
  {
    uintptr_t thread = Sampler::GetThreadHandle(aThreadInfo->GetPlatformData());
    HANDLE profiled_thread = reinterpret_cast<HANDLE>(thread);
    if (profiled_thread == NULL)
      return;

    // Context used for sampling the register state of the profiled thread.
    CONTEXT context;
    memset(&context, 0, sizeof(context));

    TickSample sample_obj;
    TickSample* sample = &sample_obj;

    // Grab the timestamp before pausing the thread, to avoid deadlocks.
    sample->timestamp = mozilla::TimeStamp::Now();
    sample->threadInfo = aThreadInfo;

    // XXX: this is an off-main-thread use of gSampler
    if (isFirstProfiledThread && gSampler->ProfileMemory()) {
      sample->rssMemory = nsMemoryReporterManager::ResidentFast();
    } else {
      sample->rssMemory = 0;
    }

    // Unique Set Size is not supported on Windows.
    sample->ussMemory = 0;

    static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (SuspendThread(profiled_thread) == kSuspendFailed)
      return;

    // SuspendThread is asynchronous, so the thread may still be running.
    // Call GetThreadContext first to ensure the thread is really suspended.
    // See https://blogs.msdn.microsoft.com/oldnewthing/20150205-00/?p=44743.

    // Using only CONTEXT_CONTROL is faster but on 64-bit it causes crashes in
    // RtlVirtualUnwind (see bug 1120126) so we set all the flags.
#if V8_HOST_ARCH_X64
    context.ContextFlags = CONTEXT_FULL;
#else
    context.ContextFlags = CONTEXT_CONTROL;
#endif
    if (!GetThreadContext(profiled_thread, &context)) {
      ResumeThread(profiled_thread);
      return;
    }

    // Threads that may invoke JS require extra attention. Since, on windows,
    // the jits also need to modify the same dynamic function table that we need
    // to get a stack trace, we have to be wary of that to avoid deadlock.
    //
    // When embedded in Gecko, for threads that aren't the main thread,
    // CanInvokeJS consults an unlocked value in the nsIThread, so we must
    // consult this after suspending the profiled thread to avoid racing
    // against a value change.
    if (aThreadInfo->CanInvokeJS()) {
      if (!TryAcquireStackWalkWorkaroundLock()) {
        ResumeThread(profiled_thread);
        return;
      }

      // It is safe to immediately drop the lock. We only need to contend with
      // the case in which the profiled thread held needed system resources.
      // If the profiled thread had held those resources, the trylock would have
      // failed. Anyone else who grabs those resources will continue to make
      // progress, since those threads are not suspended. Because of this,
      // we cannot deadlock with them, and should let them run as they please.
      ReleaseStackWalkWorkaroundLock();
    }

#if V8_HOST_ARCH_X64
    sample->pc = reinterpret_cast<Address>(context.Rip);
    sample->sp = reinterpret_cast<Address>(context.Rsp);
    sample->fp = reinterpret_cast<Address>(context.Rbp);
#else
    sample->pc = reinterpret_cast<Address>(context.Eip);
    sample->sp = reinterpret_cast<Address>(context.Esp);
    sample->fp = reinterpret_cast<Address>(context.Ebp);
#endif

    sample->context = &context;
    sampler->Tick(sample);

    ResumeThread(profiled_thread);
  }

private:
  int mStackSize;
  HANDLE mThread;
  Thread::tid_t mThreadId;

  Sampler* mSampler;
  int mInterval; // units: ms

  // Protects the process wide state below.
  static SamplerThread* mInstance;

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};

SamplerThread* SamplerThread::mInstance = NULL;

void Sampler::Start() {
  MOZ_ASSERT(!IsActive());
  SetActive(true);
  SamplerThread::StartSampler(this);
}

void Sampler::Stop() {
  MOZ_ASSERT(IsActive());
  SetActive(false);
  SamplerThread::StopSampler();
}

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return GetCurrentThreadId();
}

void OS::Startup() {
}

void OS::Sleep(int milliseconds) {
  ::Sleep(milliseconds);
}

void TickSample::PopulateContext(void* aContext)
{
  MOZ_ASSERT(aContext);
  CONTEXT* pContext = reinterpret_cast<CONTEXT*>(aContext);
  context = pContext;
  RtlCaptureContext(pContext);

#if defined(SPS_PLAT_amd64_windows)

  pc = reinterpret_cast<Address>(pContext->Rip);
  sp = reinterpret_cast<Address>(pContext->Rsp);
  fp = reinterpret_cast<Address>(pContext->Rbp);

#elif defined(SPS_PLAT_x86_windows)

  pc = reinterpret_cast<Address>(pContext->Eip);
  sp = reinterpret_cast<Address>(pContext->Esp);
  fp = reinterpret_cast<Address>(pContext->Ebp);

#endif
}

