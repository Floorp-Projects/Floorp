// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include "platform.h"
#include <process.h>


class Sampler::PlatformData : public Malloced {
 public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need to make a copy of the handle because we are
  // going to use it in the sampler thread. Using GetThreadHandle() will
  // not work in this case. We're using OpenThread because DuplicateHandle
  // for some reason doesn't work in Chrome's sandbox.
  PlatformData() : profiled_thread_(OpenThread(THREAD_GET_CONTEXT |
                                               THREAD_SUSPEND_RESUME |
                                               THREAD_QUERY_INFORMATION,
                                               false,
                                               GetCurrentThreadId())) {}

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

uintptr_t
Sampler::GetThreadHandle(Sampler::PlatformData* aData)
{
  return (uintptr_t) aData->profiled_thread();
}

class SamplerThread : public Thread {
 public:
  SamplerThread(int interval, Sampler* sampler)
      : Thread("SamplerThread"),
        interval_(interval),
        sampler_(sampler) {}

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
    while (sampler_->IsActive()) {
      if (!sampler_->IsPaused())
        SampleContext(sampler_);
      OS::Sleep(interval_);
    }
  }

  void SampleContext(Sampler* sampler) {
    HANDLE profiled_thread = sampler->platform_data()->profiled_thread();
    if (profiled_thread == NULL)
      return;

    // Context used for sampling the register state of the profiled thread.
    CONTEXT context;
    memset(&context, 0, sizeof(context));

    TickSample sample_obj;
    TickSample* sample = &sample_obj;

    // Grab the timestamp before pausing the thread, to avoid deadlocks.
    sample->timestamp = mozilla::TimeStamp::Now();

    static const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (SuspendThread(profiled_thread) == kSuspendFailed)
      return;

    context.ContextFlags = CONTEXT_FULL;
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
      sampler->SampleStack(sample);
      sampler->Tick(sample);
    }
    ResumeThread(profiled_thread);
  }

  Sampler* sampler_;
  const int interval_;

  // Protects the process wide state below.
  static SamplerThread* instance_;

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};

SamplerThread* SamplerThread::instance_ = NULL;


Sampler::Sampler(int interval, bool profiling)
    : interval_(interval),
      profiling_(profiling),
      active_(false),
      paused_(false),
      data_(new PlatformData) {
}

Sampler::~Sampler() {
  ASSERT(!IsActive());
  delete data_;
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

class Thread::PlatformData : public Malloced {
 public:
  explicit PlatformData(HANDLE thread) : thread_(thread) {}
  HANDLE thread_;
  unsigned thread_id_;
};

// Initialize a Win32 thread object. The thread has an invalid thread
// handle until it is started.
Thread::Thread(const char* name)
    : stack_size_(0) {
  data_ = new PlatformData(kNoThread);
  set_name(name);
}

void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}

// Close our own handle for the thread.
Thread::~Thread() {
  if (data_->thread_ != kNoThread) CloseHandle(data_->thread_);
  delete data_;
}

// Create a new thread. It is important to use _beginthreadex() instead of
// the Win32 function CreateThread(), because the CreateThread() does not
// initialize thread specific structures in the C runtime library.
void Thread::Start() {
  data_->thread_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     static_cast<unsigned>(stack_size_),
                     ThreadEntry,
                     this,
                     0,
                     &data_->thread_id_));
}

// Wait for thread to terminate.
void Thread::Join() {
  if (data_->thread_id_ != GetCurrentThreadId()) {
    WaitForSingleObject(data_->thread_, INFINITE);
  }
}

void OS::Sleep(int milliseconds) {
  ::Sleep(milliseconds);
}
