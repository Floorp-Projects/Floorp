/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

// Memory profile
#include "nsMemoryReporterManager.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

// this port is based off of v8 svn revision 9837

class PlatformData {
 public:
  PlatformData() : profiled_thread_(mach_thread_self())
  {
  }

  ~PlatformData() {
    // Deallocate Mach port for thread.
    mach_port_deallocate(mach_task_self(), profiled_thread_);
  }

  thread_act_t profiled_thread() { return profiled_thread_; }

 private:
  // Note: for profiled_thread_ Mach primitives are used instead of PThread's
  // because the latter doesn't provide thread manipulation primitives required.
  // For details, consult "Mac OS X Internals" book, Section 7.3.
  thread_act_t profiled_thread_;
};

UniquePlatformData
AllocPlatformData(int aThreadId)
{
  return UniquePlatformData(new PlatformData);
}

void
PlatformDataDestructor::operator()(PlatformData* aData)
{
  delete aData;
}

// The sampler thread controls sampling and runs whenever the profiler is
// active. It periodically runs through all registered threads, finds those
// that should be sampled, then pauses and samples them.
class SamplerThread
{
private:
  static void SetThreadName() {
    // pthread_setname_np is only available in 10.6 or later, so test
    // for it at runtime.
    int (*dynamic_pthread_setname_np)(const char*);
    *reinterpret_cast<void**>(&dynamic_pthread_setname_np) =
      dlsym(RTLD_DEFAULT, "pthread_setname_np");
    if (!dynamic_pthread_setname_np)
      return;

    dynamic_pthread_setname_np("SamplerThread");
  }

  static void* ThreadEntry(void* aArg) {
    auto thread = static_cast<SamplerThread*>(aArg);
    SetThreadName();
    thread->Run();
    return nullptr;
  }

public:
  SamplerThread(PS::LockRef aLock, uint32_t aActivityGeneration,
                double aInterval)
    : mActivityGeneration(aActivityGeneration)
    , mIntervalMicro(std::max(1, int(floor(aInterval * 1000 + 0.5))))
  {
    pthread_attr_t* attr_ptr = nullptr;
    if (pthread_create(&mThread, attr_ptr, ThreadEntry, this) != 0) {
      MOZ_CRASH("pthread_create failed");
    }
  }

  ~SamplerThread() {
    pthread_join(mThread, nullptr);
  }

  void Stop(PS::LockRef aLock) {}

  void Run() {
    // This function runs on the sampler thread.

    TimeDuration lastSleepOverhead = 0;
    TimeStamp sampleStart = TimeStamp::Now();

    while (true) {
      // This scope is for |lock|. It ends before we sleep below.
      {
        PS::AutoLock lock(gPSMutex);

        // See the corresponding code in platform-linux-android.cpp for an
        // explanation of what's happening here.
        if (PS::ActivityGeneration(lock) != mActivityGeneration) {
          return;
        }

        gPS->Buffer(lock)->deleteExpiredStoredMarkers();

        if (!gPS->IsPaused(lock)) {
          bool isFirstProfiledThread = true;

          const PS::ThreadVector& threads = gPS->Threads(lock);
          for (uint32_t i = 0; i < threads.size(); i++) {
            ThreadInfo* info = threads[i];

            if (!info->HasProfile() || info->IsPendingDelete()) {
              // We are not interested in profiling this thread.
              continue;
            }

            if (info->Stack()->CanDuplicateLastSampleDueToSleep() &&
                gPS->Buffer(lock)->DuplicateLastSample(gPS->StartTime(lock),
                                                       info->LastSample())) {
              continue;
            }

            info->UpdateThreadResponsiveness();

            SampleContext(lock, info, isFirstProfiledThread);

            isFirstProfiledThread = false;
          }
        }
        // gPSMutex is unlocked here.
      }

      TimeStamp targetSleepEndTime = sampleStart + TimeDuration::FromMicroseconds(mIntervalMicro);
      TimeStamp beforeSleep = TimeStamp::Now();
      TimeDuration targetSleepDuration = targetSleepEndTime - beforeSleep;
      double sleepTime = std::max(0.0, (targetSleepDuration - lastSleepOverhead).ToMicroseconds());
      usleep(sleepTime);
      sampleStart = TimeStamp::Now();
      lastSleepOverhead = sampleStart - (beforeSleep + TimeDuration::FromMicroseconds(sleepTime));
    }
  }

  void SampleContext(PS::LockRef aLock, ThreadInfo* aThreadInfo,
                     bool isFirstProfiledThread)
  {
    thread_act_t profiled_thread =
      aThreadInfo->GetPlatformData()->profiled_thread();

    TickSample sample;

    // Unique Set Size is not supported on Mac.
    sample.rssMemory = (isFirstProfiledThread && gPS->FeatureMemory(aLock))
                     ? nsMemoryReporterManager::ResidentFast()
                     : 0;
    sample.ussMemory = 0;

    // We're using thread_suspend on OS X because pthread_kill (which is what
    // we're using on Linux) has less consistent performance and causes
    // strange crashes, see bug 1166778 and bug 1166808.

    if (KERN_SUCCESS != thread_suspend(profiled_thread)) {
      return;
    }

#if defined(GP_ARCH_amd64)
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
    x86_thread_state64_t state;
    mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __r ## name
#else
#define REGISTER_FIELD(name) r ## name
#endif  // __DARWIN_UNIX03
#elif defined(GP_ARCH_x86)
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
#endif  // GP_ARCH_*

    if (thread_get_state(profiled_thread,
                         flavor,
                         reinterpret_cast<natural_t*>(&state),
                         &count) == KERN_SUCCESS) {
      sample.pc = reinterpret_cast<Address>(state.REGISTER_FIELD(ip));
      sample.sp = reinterpret_cast<Address>(state.REGISTER_FIELD(sp));
      sample.fp = reinterpret_cast<Address>(state.REGISTER_FIELD(bp));
      sample.timestamp = mozilla::TimeStamp::Now();
      sample.threadInfo = aThreadInfo;

#undef REGISTER_FIELD

      Tick(aLock, gPS->Buffer(aLock), &sample);
    }
    thread_resume(profiled_thread);
  }

private:
  // The activity generation, for detecting when the sampler thread must stop.
  const uint32_t mActivityGeneration;

  // The pthread_t for the sampler thread.
  pthread_t mThread;

  // The interval between samples, measured in microseconds.
  const int mIntervalMicro;

  SamplerThread(const SamplerThread&) = delete;
  void operator=(const SamplerThread&) = delete;
};

static void
PlatformInit(PS::LockRef aLock)
{
}

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return gettid();
}

void TickSample::PopulateContext(void* aContext)
{
  // Note that this asm changes if PopulateContext's parameter list is altered
#if defined(GP_ARCH_amd64)
  asm (
      // Compute caller's %rsp by adding to %rbp:
      // 8 bytes for previous %rbp, 8 bytes for return address
      "leaq 0x10(%%rbp), %0\n\t"
      // Dereference %rbp to get previous %rbp
      "movq (%%rbp), %1\n\t"
      :
      "=r"(sp),
      "=r"(fp)
  );
#elif defined(GP_ARCH_x86)
  asm (
      // Compute caller's %esp by adding to %ebp:
      // 4 bytes for aContext + 4 bytes for return address +
      // 4 bytes for previous %ebp
      "leal 0xc(%%ebp), %0\n\t"
      // Dereference %ebp to get previous %ebp
      "movl (%%ebp), %1\n\t"
      :
      "=r"(sp),
      "=r"(fp)
  );
#else
# error "Unsupported architecture"
#endif
  pc = reinterpret_cast<Address>(__builtin_extract_return_addr(
                                    __builtin_return_address(0)));
}

