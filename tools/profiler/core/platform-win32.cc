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
#include "GeckoSampler.h"
#include "ThreadResponsiveness.h"
#include "ProfileEntry.h"

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
  PlatformData(int aThreadId) : profiled_thread_(OpenThread(THREAD_GET_CONTEXT |
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

/* static */ PlatformData*
Sampler::AllocPlatformData(int aThreadId)
{
  return new PlatformData(aThreadId);
}

/* static */ void
Sampler::FreePlatformData(PlatformData* aData)
{
  delete aData;
}

uintptr_t
Sampler::GetThreadHandle(PlatformData* aData)
{
  return (uintptr_t) aData->profiled_thread();
}

class SamplerThread : public Thread {
 public:
  SamplerThread(double interval, Sampler* sampler)
      : Thread("SamplerThread")
      , sampler_(sampler)
      , interval_(interval)
  {
    interval_ = floor(interval + 0.5);
    if (interval_ <= 0) {
      interval_ = 1;
    }
  }

  static void StartSampler(Sampler* sampler) {
    if (instance_ == NULL) {
      instance_ = new SamplerThread(sampler->interval(), sampler);
      instance_->Start();
    } else {
      ASSERT(instance_->interval_ == sampler->interval());
    }
  }

  static void StopSampler() {
    instance_->Join();
    delete instance_;
    instance_ = NULL;
  }

  // Implement Thread::Run().
  virtual void Run() {

    // By default we'll not adjust the timer resolution which tends to be around
    // 16ms. However, if the requested interval is sufficiently low we'll try to
    // adjust the resolution to match.
    if (interval_ < 10)
        ::timeBeginPeriod(interval_);

    while (sampler_->IsActive()) {
      sampler_->DeleteExpiredMarkers();

      if (!sampler_->IsPaused()) {
        ::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
        std::vector<ThreadInfo*> threads =
          sampler_->GetRegisteredThreads();
        bool isFirstProfiledThread = true;
        for (uint32_t i = 0; i < threads.size(); i++) {
          ThreadInfo* info = threads[i];

          // This will be null if we're not interested in profiling this thread.
          if (!info->Profile() || info->IsPendingDelete())
            continue;

          PseudoStack::SleepState sleeping = info->Stack()->observeSleeping();
          if (sleeping == PseudoStack::SLEEPING_AGAIN) {
            info->Profile()->DuplicateLastSample();
            continue;
          }

          info->Profile()->GetThreadResponsiveness()->Update();

          ThreadProfile* thread_profile = info->Profile();

          SampleContext(sampler_, thread_profile, isFirstProfiledThread);
          isFirstProfiledThread = false;
        }
      }
      OS::Sleep(interval_);
    }

    // disable any timer resolution changes we've made
    if (interval_ < 10)
        ::timeEndPeriod(interval_);
  }

  void SampleContext(Sampler* sampler, ThreadProfile* thread_profile,
                     bool isFirstProfiledThread)
  {
    uintptr_t thread = Sampler::GetThreadHandle(
                               thread_profile->GetPlatformData());
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
    sample->threadProfile = thread_profile;

    if (isFirstProfiledThread && Sampler::GetActiveSampler()->ProfileMemory()) {
      sample->rssMemory = nsMemoryReporterManager::ResidentFast();
    } else {
      sample->rssMemory = 0;
    }

    // Unique Set Size is not supported on Windows.
    sample->ussMemory = 0;

    static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (SuspendThread(profiled_thread) == kSuspendFailed)
      return;

    // Threads that may invoke JS require extra attention. Since, on windows,
    // the jits also need to modify the same dynamic function table that we need
    // to get a stack trace, we have to be wary of that to avoid deadlock.
    //
    // When embedded in Gecko, for threads that aren't the main thread,
    // CanInvokeJS consults an unlocked value in the nsIThread, so we must
    // consult this after suspending the profiled thread to avoid racing
    // against a value change.
    if (thread_profile->CanInvokeJS()) {
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

    // Using only CONTEXT_CONTROL is faster but on 64-bit it causes crashes in
    // RtlVirtualUnwind (see bug 1120126) so we set all the flags.
#if V8_HOST_ARCH_X64
    context.ContextFlags = CONTEXT_FULL;
#else
    context.ContextFlags = CONTEXT_CONTROL;
#endif
    if (GetThreadContext(profiled_thread, &context) != 0) {
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
    }
    ResumeThread(profiled_thread);
  }

  Sampler* sampler_;
  int interval_; // units: ms

  // Protects the process wide state below.
  static SamplerThread* instance_;

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};

SamplerThread* SamplerThread::instance_ = NULL;


Sampler::Sampler(double interval, bool profiling, int entrySize)
    : interval_(interval),
      profiling_(profiling),
      paused_(false),
      active_(false),
      entrySize_(entrySize) {
}

Sampler::~Sampler() {
  ASSERT(!IsActive());
}

void Sampler::Start() {
  ASSERT(!IsActive());
  SetActive(true);
  SamplerThread::StartSampler(this);
}

void Sampler::Stop() {
  ASSERT(IsActive());
  SetActive(false);
  SamplerThread::StopSampler();
}


static const HANDLE kNoThread = INVALID_HANDLE_VALUE;

static unsigned int __stdcall ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  thread->Run();
  return 0;
}

// Initialize a Win32 thread object. The thread has an invalid thread
// handle until it is started.
Thread::Thread(const char* name)
    : stack_size_(0) {
  thread_ = kNoThread;
  set_name(name);
}

void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}

// Close our own handle for the thread.
Thread::~Thread() {
  if (thread_ != kNoThread) CloseHandle(thread_);
}

// Create a new thread. It is important to use _beginthreadex() instead of
// the Win32 function CreateThread(), because the CreateThread() does not
// initialize thread specific structures in the C runtime library.
void Thread::Start() {
  thread_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     static_cast<unsigned>(stack_size_),
                     ThreadEntry,
                     this,
                     0,
                     (unsigned int*) &thread_id_));
}

// Wait for thread to terminate.
void Thread::Join() {
  if (thread_id_ != GetCurrentId()) {
    WaitForSingleObject(thread_, INFINITE);
  }
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

bool Sampler::RegisterCurrentThread(const char* aName,
                                    PseudoStack* aPseudoStack,
                                    bool aIsMainThread, void* stackTop)
{
  if (!Sampler::sRegisteredThreadsMutex)
    return false;


  ::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);

  int id = GetCurrentThreadId();

  for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
    ThreadInfo* info = sRegisteredThreads->at(i);
    if (info->ThreadId() == id && !info->IsPendingDelete()) {
      // Thread already registered. This means the first unregister will be
      // too early.
      ASSERT(false);
      return false;
    }
  }

  set_tls_stack_top(stackTop);

  ThreadInfo* info = new StackOwningThreadInfo(aName, id,
    aIsMainThread, aPseudoStack, stackTop);

  if (sActiveSampler) {
    sActiveSampler->RegisterThread(info);
  }

  sRegisteredThreads->push_back(info);

  return true;
}

void Sampler::UnregisterCurrentThread()
{
  if (!Sampler::sRegisteredThreadsMutex)
    return;

  tlsStackTop.set(nullptr);

  ::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);

  int id = GetCurrentThreadId();

  for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
    ThreadInfo* info = sRegisteredThreads->at(i);
    if (info->ThreadId() == id && !info->IsPendingDelete()) {
      if (profiler_is_active()) {
        // We still want to show the results of this thread if you
        // save the profile shortly after a thread is terminated.
        // For now we will defer the delete to profile stop.
        info->SetPendingDelete();
        break;
      } else {
        delete info;
        sRegisteredThreads->erase(sRegisteredThreads->begin() + i);
        break;
      }
    }
  }
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

