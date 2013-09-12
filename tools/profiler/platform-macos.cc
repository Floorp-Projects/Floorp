/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>

#include <AvailabilityMacros.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/vm_statistics.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "nsThreadUtils.h"

#include "platform.h"
#include "TableTicker.h"
#include "UnwinderThread2.h"  /* uwt__register_thread_for_profiling */

// this port is based off of v8 svn revision 9837

// XXX: this is a very stubbed out implementation
// that only supports a single Sampler
struct SamplerRegistry {
  static void AddActiveSampler(Sampler *sampler) {
    ASSERT(!SamplerRegistry::sampler);
    SamplerRegistry::sampler = sampler;
  }
  static void RemoveActiveSampler(Sampler *sampler) {
    SamplerRegistry::sampler = NULL;
  }
  static Sampler *sampler;
};

Sampler *SamplerRegistry::sampler = NULL;

// 0 is never a valid thread id on MacOSX since a ptread_t is
// a pointer.
static const pthread_t kNoThread = (pthread_t) 0;

class MacOSMutex : public Mutex {
 public:
  MacOSMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &attr);
  }

  virtual ~MacOSMutex() { pthread_mutex_destroy(&mutex_); }

  virtual int Lock() { return pthread_mutex_lock(&mutex_); }
  virtual int Unlock() { return pthread_mutex_unlock(&mutex_); }

  virtual bool TryLock() {
    int result = pthread_mutex_trylock(&mutex_);
    // Return false if the lock is busy and locking failed.
    if (result == EBUSY) {
      return false;
    }
    ASSERT(result == 0);  // Verify no other errors.
    return true;
  }

 private:
  pthread_mutex_t mutex_;
};


Mutex* OS::CreateMutex() {
  return new MacOSMutex();
}

void OS::Sleep(int milliseconds) {
  usleep(1000 * milliseconds);
}

void OS::SleepMicro(int microseconds) {
  usleep(microseconds);
}

Thread::Thread(const char* name)
    : stack_size_(0) {
  set_name(name);
}


Thread::~Thread() {
}


static void SetThreadName(const char* name) {
  // pthread_setname_np is only available in 10.6 or later, so test
  // for it at runtime.
  int (*dynamic_pthread_setname_np)(const char*);
  *reinterpret_cast<void**>(&dynamic_pthread_setname_np) =
    dlsym(RTLD_DEFAULT, "pthread_setname_np");
  if (!dynamic_pthread_setname_np)
    return;

  // Mac OS X does not expose the length limit of the name, so hardcode it.
  static const int kMaxNameLength = 63;
  USE(kMaxNameLength);
  ASSERT(Thread::kMaxThreadNameLength <= kMaxNameLength);
  dynamic_pthread_setname_np(name);
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);

  thread->thread_ = pthread_self();
  SetThreadName(thread->name());
  ASSERT(thread->thread_ != kNoThread);
  thread->Run();
  return NULL;
}


void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}


void Thread::Start() {
  pthread_attr_t* attr_ptr = NULL;
  pthread_attr_t attr;
  if (stack_size_ > 0) {
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, static_cast<size_t>(stack_size_));
    attr_ptr = &attr;
  }
  pthread_create(&thread_, attr_ptr, ThreadEntry, this);
  ASSERT(thread_ != kNoThread);
}

void Thread::Join() {
  pthread_join(thread_, NULL);
}

class PlatformData : public Malloced {
 public:
  PlatformData() : profiled_thread_(mach_thread_self())
  {
    profiled_pthread_ = pthread_from_mach_thread_np(profiled_thread_);
  }

  ~PlatformData() {
    // Deallocate Mach port for thread.
    mach_port_deallocate(mach_task_self(), profiled_thread_);
  }

  thread_act_t profiled_thread() { return profiled_thread_; }
  pthread_t profiled_pthread() { return profiled_pthread_; }

 private:
  // Note: for profiled_thread_ Mach primitives are used instead of PThread's
  // because the latter doesn't provide thread manipulation primitives required.
  // For details, consult "Mac OS X Internals" book, Section 7.3.
  thread_act_t profiled_thread_;
  // we also store the pthread because Mach threads have no concept of stack
  // and we want to be able to get the stack size when we need to unwind the
  // stack using frame pointers.
  pthread_t profiled_pthread_;
};

/* static */ PlatformData*
Sampler::AllocPlatformData(int aThreadId)
{
  return new PlatformData;
}

/* static */ void
Sampler::FreePlatformData(PlatformData* aData)
{
  delete aData;
}

class SamplerThread : public Thread {
 public:
  explicit SamplerThread(double interval)
      : Thread("SamplerThread")
      , intervalMicro_(floor(interval * 1000 + 0.5))
  {
    if (intervalMicro_ <= 0) {
      intervalMicro_ = 1;
    }
  }

  static void AddActiveSampler(Sampler* sampler) {
    mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
    SamplerRegistry::AddActiveSampler(sampler);
    if (instance_ == NULL) {
      instance_ = new SamplerThread(sampler->interval());
      instance_->Start();
    }
  }

  static void RemoveActiveSampler(Sampler* sampler) {
    mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
    instance_->Join();
    //XXX: unlike v8 we need to remove the active sampler after doing the Join
    // because we drop the sampler immediately
    SamplerRegistry::RemoveActiveSampler(sampler);
    delete instance_;
    instance_ = NULL;
  }

  // Implement Thread::Run().
  virtual void Run() {
    while (SamplerRegistry::sampler->IsActive()) {
      {
        mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
        std::vector<ThreadInfo*> threads =
          SamplerRegistry::sampler->GetRegisteredThreads();
        for (uint32_t i = 0; i < threads.size(); i++) {
          ThreadInfo* info = threads[i];

          // This will be null if we're not interested in profiling this thread.
          if (!info->Profile())
            continue;

          ThreadProfile* thread_profile = info->Profile();

          if (!SamplerRegistry::sampler->IsPaused())
            SampleContext(SamplerRegistry::sampler, thread_profile);
        }
      }
      OS::SleepMicro(intervalMicro_);
    }
  }

  void SampleContext(Sampler* sampler, ThreadProfile* thread_profile) {
    thread_act_t profiled_thread =
      thread_profile->GetPlatformData()->profiled_thread();

    TickSample sample_obj;
    TickSample* sample = &sample_obj;

    if (KERN_SUCCESS != thread_suspend(profiled_thread)) return;

#if V8_HOST_ARCH_X64
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
    x86_thread_state64_t state;
    mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __r ## name
#else
#define REGISTER_FIELD(name) r ## name
#endif  // __DARWIN_UNIX03
#elif V8_HOST_ARCH_IA32
    thread_state_flavor_t flavor = i386_THREAD_STATE;
    i386_thread_state_t state;
    mach_msg_type_number_t count = i386_THREAD_STATE_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __e ## name
#else
#define REGISTER_FIELD(name) e ## name
#endif  // __DARWIN_UNIX03
#else
#error Unsupported Mac OS X host architecture.
#endif  // V8_HOST_ARCH

    if (thread_get_state(profiled_thread,
                         flavor,
                         reinterpret_cast<natural_t*>(&state),
                         &count) == KERN_SUCCESS) {
      sample->pc = reinterpret_cast<Address>(state.REGISTER_FIELD(ip));
      sample->sp = reinterpret_cast<Address>(state.REGISTER_FIELD(sp));
      sample->fp = reinterpret_cast<Address>(state.REGISTER_FIELD(bp));
      sample->timestamp = mozilla::TimeStamp::Now();
      sample->threadProfile = thread_profile;
      sampler->Tick(sample);
    }
    thread_resume(profiled_thread);
  }

  int intervalMicro_;
  //RuntimeProfilerRateLimiter rate_limiter_;

  // Protects the process wide state below.
  static Mutex* mutex_;
  static SamplerThread* instance_;

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};

#undef REGISTER_FIELD

SamplerThread* SamplerThread::instance_ = NULL;

Sampler::Sampler(double interval, bool profiling, int entrySize)
    : // isolate_(isolate),
      interval_(interval),
      profiling_(profiling),
      paused_(false),
      active_(false),
      entrySize_(entrySize) /*,
      samples_taken_(0)*/ {
}


Sampler::~Sampler() {
  ASSERT(!IsActive());
}


void Sampler::Start() {
  ASSERT(!IsActive());
  SetActive(true);
  SamplerThread::AddActiveSampler(this);
}


void Sampler::Stop() {
  ASSERT(IsActive());
  SetActive(false);
  SamplerThread::RemoveActiveSampler(this);
}

pthread_t
Sampler::GetProfiledThread(PlatformData* aData)
{
  return aData->profiled_pthread();
}

#include <sys/syscall.h>
pid_t gettid()
{
  return (pid_t) syscall(SYS_thread_selfid);
}

bool Sampler::RegisterCurrentThread(const char* aName,
                                    PseudoStack* aPseudoStack,
                                    bool aIsMainThread, void* stackTop)
{
  if (!Sampler::sRegisteredThreadsMutex)
    return false;

  mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);

  ThreadInfo* info = new ThreadInfo(aName, gettid(),
    aIsMainThread, aPseudoStack, stackTop);

  if (sActiveSampler) {
    sActiveSampler->RegisterThread(info);
  }

  sRegisteredThreads->push_back(info);

  uwt__register_thread_for_profiling(stackTop);
  return true;
}

void Sampler::UnregisterCurrentThread()
{
  if (!Sampler::sRegisteredThreadsMutex)
    return;

  mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);

  int id = gettid();

  for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
    ThreadInfo* info = sRegisteredThreads->at(i);
    if (info->ThreadId() == id) {
      delete info;
      sRegisteredThreads->erase(sRegisteredThreads->begin() + i);
      break;
    }
  }
}
