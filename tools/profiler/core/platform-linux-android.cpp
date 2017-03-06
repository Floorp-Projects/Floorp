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

// This file is used for both Linux and Android.

#include <stdio.h>
#include <math.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/prctl.h> // set name
#include <stdlib.h>
#include <sched.h>
#include <ucontext.h>
// Ubuntu Dapper requires memory pages to be marked as
// executable. Otherwise, OS raises an exception when executing code
// in that page.
#include <sys/types.h>  // mmap & munmap
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <fcntl.h>      // open
#include <unistd.h>     // sysconf
#include <semaphore.h>
#ifdef __GLIBC__
#include <execinfo.h>   // backtrace, backtrace_symbols
#endif  // def __GLIBC__
#include <strings.h>    // index
#include <errno.h>
#include <stdarg.h>

#include "prenv.h"
#include "mozilla/LinuxSignal.h"
#include "mozilla/PodOperations.h"
#include "mozilla/DebugOnly.h"

// Memory profile
#include "nsMemoryReporterManager.h"

#include <string.h>
#include <list>

using namespace mozilla;

#if defined(USE_LUL_STACKWALK)
// A singleton instance of the library.  It is initialised at first
// use.  Currently only the main thread can call PlatformStart(), so
// there is no need for a mechanism to ensure that it is only
// created once in a multi-thread-use situation.
lul::LUL* gLUL = nullptr;
#endif

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return gettid();
}

#if !defined(GP_OS_android)

// Keep track of when any of our threads calls fork(), so we can
// temporarily disable signal delivery during the fork() call.  Not
// doing so appears to cause a kind of race, in which signals keep
// getting delivered to the thread doing fork(), which keeps causing
// it to fail and be restarted; hence forward progress is delayed a
// great deal.  A side effect of this is to permanently disable
// sampling in the child process.  See bug 837390.

// Unfortunately this is only doable on non-Android, since Bionic
// doesn't have pthread_atfork.

// This records the current state at the time we paused it.
static bool gWasPaused = false;

// In the parent, before the fork, record the pausedness state, and then pause.
static void
paf_prepare()
{
  // This function can run off the main thread.

  gWasPaused = gIsPaused;
  gIsPaused = true;
}

// In the parent, after the fork, return pausedness to the pre-fork state.
static void
paf_parent()
{
  // This function can run off the main thread.

  gIsPaused = gWasPaused;
  gWasPaused = false;
}

// In the child, after the fork, leave the profiler paused.
static void
paf_child()
{
  // This function can run off the main thread.

  gWasPaused = false;
}

// Set up the fork handlers.
static void*
setup_atfork()
{
  pthread_atfork(paf_prepare, paf_parent, paf_child);
  return nullptr;
}

#endif /* !defined(GP_OS_android) */

static void SetSampleContext(TickSample* sample, mcontext_t& mcontext)
{
  // Extracting the sample from the context is extremely machine dependent.
#if defined(GP_ARCH_x86)
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_EIP]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_ESP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_EBP]);
#elif defined(GP_ARCH_amd64)
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_RIP]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_RSP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_RBP]);
#elif defined(GP_ARCH_arm)
  sample->pc = reinterpret_cast<Address>(mcontext.arm_pc);
  sample->sp = reinterpret_cast<Address>(mcontext.arm_sp);
  sample->fp = reinterpret_cast<Address>(mcontext.arm_fp);
  sample->lr = reinterpret_cast<Address>(mcontext.arm_lr);
#else
# error "bad platform"
#endif
}

#if defined(GP_OS_android)
#define SYS_tgkill __NR_tgkill
#endif

int tgkill(pid_t tgid, pid_t tid, int signalno) {
  return syscall(SYS_tgkill, tgid, tid, signalno);
}

class PlatformData {
 public:
  PlatformData()
  {
    MOZ_COUNT_CTOR(PlatformData);
  }

  ~PlatformData()
  {
    MOZ_COUNT_DTOR(PlatformData);
  }
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

static void
SleepMicro(int aMicroseconds)
{
  if (MOZ_UNLIKELY(aMicroseconds >= 1000000)) {
    // Use usleep for larger intervals, because the nanosleep
    // code below only supports intervals < 1 second.
    MOZ_ALWAYS_TRUE(!::usleep(aMicroseconds));
    return;
  }

  struct timespec ts;
  ts.tv_sec  = 0;
  ts.tv_nsec = aMicroseconds * 1000UL;

  int rv = ::nanosleep(&ts, &ts);

  while (rv != 0 && errno == EINTR) {
    // Keep waiting in case of interrupt.
    // nanosleep puts the remaining time back into ts.
    rv = ::nanosleep(&ts, &ts);
  }

  MOZ_ASSERT(!rv, "nanosleep call failed");
}

// The sampler thread controls sampling and runs whenever the profiler is
// active. It periodically runs through all registered threads, finds those
// that should be sampled, then pauses and samples them.
//
// The only way to reliably interrupt a Linux thread and inspect its register
// and stack state is by sending a signal to it, and doing the work inside the
// signal handler.  But we don't want to run much code inside the signal
// handler, since POSIX severely restricts what we can do in signal handlers.
// So we use a system of semaphores to suspend the thread and allow the
// sampler thread to do all the work of unwinding and copying out whatever
// data it wants.
//
// A four-message protocol is used to reliably suspend and later resume the
// thread to be sampled (the samplee):
//
// Sampler (signal sender) thread              Samplee (thread to be sampled)
//
// Prepare the SigHandlerCoordinator
// and point sSigHandlerCoordinator at it
//
// send SIGPROF to samplee ------- MSG 1 ----> (enter signal handler)
// wait(mMessage2)                             Copy register state
//                                               into sSigHandlerCoordinator
//                         <------ MSG 2 ----- post(mMessage2)
// Samplee is now suspended.                   wait(mMessage3)
//   Examine its stack/register
//   state at leisure
//
// Release samplee:
//   post(mMessage3)       ------- MSG 3 ----->
// wait(mMessage4)                              Samplee now resumes.  Tell
//                                                the sampler that we are done.
//                         <------ MSG 4 ------ post(mMessage4)
// Now we know the samplee's signal             (leave signal handler)
//   handler has finished using
//   sSigHandlerCoordinator.  We can
//   safely reuse it for some other thread.
//
class SamplerThread
{
private:
  // A type used to coordinate between the sampler (signal sending) thread and
  // the thread currently being sampled (the samplee, which receives the
  // signals).
  //
  // The first message is sent using a SIGPROF signal delivery.  The subsequent
  // three are sent using sem_wait/sem_post pairs.  They are named accordingly
  // in the following struct.
  struct SigHandlerCoordinator
  {
    SigHandlerCoordinator()
    {
      PodZero(&mUContext);
      int r = sem_init(&mMessage2, /* pshared */ 0, 0);
      r    |= sem_init(&mMessage3, /* pshared */ 0, 0);
      r    |= sem_init(&mMessage4, /* pshared */ 0, 0);
      MOZ_ASSERT(r == 0);
    }

    ~SigHandlerCoordinator()
    {
      int r = sem_destroy(&mMessage2);
      r    |= sem_destroy(&mMessage3);
      r    |= sem_destroy(&mMessage4);
      MOZ_ASSERT(r == 0);
    }

    sem_t mMessage2; // To sampler: "context is in sSigHandlerCoordinator"
    sem_t mMessage3; // To samplee: "resume"
    sem_t mMessage4; // To sampler: "finished with sSigHandlerCoordinator"
    ucontext_t mUContext; // Context at signal
  };

  static void* ThreadEntry(void* aArg)
  {
    auto thread = static_cast<SamplerThread*>(aArg);
    prctl(PR_SET_NAME, "SamplerThread", 0, 0, 0);
    thread->Run();
    return nullptr;
  }

public:
  explicit SamplerThread(double aInterval)
    : mIntervalMicro(std::max(1, int(floor(aInterval * 1000 + 0.5))))
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());

#if defined(USE_EHABI_STACKWALK)
    mozilla::EHABIStackWalkInit();
#elif defined(USE_LUL_STACKWALK)
    bool createdLUL = false;
    if (!gLUL) {
      gLUL = new lul::LUL(logging_sink_for_LUL);
      // Read all the unwind info currently available.
      read_procmaps(gLUL);
      createdLUL = true;
    }
#endif

    // Request profiling signals.
    LOG("Request signal");
    struct sigaction sa;
    sa.sa_sigaction = MOZ_SIGNAL_TRAMPOLINE(SigprofHandler);
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGPROF, &sa, &mOldSigprofHandler) != 0) {
      MOZ_CRASH("Error installing SIGPROF handler in the profiler");
    }
    LOG("Signal installed");

#if defined(USE_LUL_STACKWALK)
    if (createdLUL) {
      // Switch into unwind mode. After this point, we can't add or remove any
      // unwind info to/from this LUL instance. The only thing we can do with
      // it is Unwind() calls.
      gLUL->EnableUnwinding();

      // Has a test been requested?
      if (PR_GetEnv("MOZ_PROFILER_LUL_TEST")) {
         int nTests = 0, nTestsPassed = 0;
         RunLulUnitTests(&nTests, &nTestsPassed, gLUL);
      }
    }
#endif

    // Start the sampling thread. It repeatedly sends a SIGPROF signal. Sending
    // the signal ourselves instead of relying on itimer provides much better
    // accuracy.
    if (pthread_create(&mThread, nullptr, ThreadEntry, this) != 0) {
      MOZ_CRASH("pthread_create failed");
    }
    LOG("Sampler thread started");
  }

  void Join() {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());

    // Wait for Run() termination (Run() will exit because gIsActive has been
    // set to false).
    pthread_join(mThread, nullptr);

    // Restore old signal handler
    sigaction(SIGPROF, &mOldSigprofHandler, 0);
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

  static void SigprofHandler(int aSignal, siginfo_t* aInfo, void* aContext) {
    // Avoid TSan warning about clobbering errno.
    int savedErrno = errno;

    MOZ_ASSERT(aSignal == SIGPROF);
    MOZ_ASSERT(sSigHandlerCoordinator);

    // By sending us this signal, the sampler thread has sent us message 1 in
    // the comment above, with the meaning "|sSigHandlerCoordinator| is ready
    // for use, please copy your register context into it."
    sSigHandlerCoordinator->mUContext = *static_cast<ucontext_t*>(aContext);

    // Send message 2: tell the sampler thread that the context has been copied
    // into |sSigHandlerCoordinator->mUContext|.  sem_post can never fail by
    // being interrupted by a signal, so there's no loop around this call.
    int r = sem_post(&sSigHandlerCoordinator->mMessage2);
    MOZ_ASSERT(r == 0);

    // At this point, the sampler thread assumes we are suspended, so we must
    // not touch any global state here.

    // Wait for message 3: the sampler thread tells us to resume.
    while (true) {
      r = sem_wait(&sSigHandlerCoordinator->mMessage3);
      if (r == -1 && errno == EINTR) {
        // Interrupted by a signal.  Try again.
        continue;
      }
      // We don't expect any other kind of failure
      MOZ_ASSERT(r == 0);
      break;
    }

    // Send message 4: tell the sampler thread that we are finished accessing
    // |sSigHandlerCoordinator|.  After this point it is not safe to touch
    // |sSigHandlerCoordinator|.
    r = sem_post(&sSigHandlerCoordinator->mMessage4);
    MOZ_ASSERT(r == 0);

    errno = savedErrno;
  }

  void Run() {
    // This function runs on the sampler thread.

    int vm_tgid_ = getpid();
    DebugOnly<int> my_tid = gettid();

    TimeDuration lastSleepOverhead = 0;
    TimeStamp sampleStart = TimeStamp::Now();

    while (gIsActive) {
      gBuffer->deleteExpiredStoredMarkers();

      if (!gIsPaused) {
        StaticMutexAutoLock lock(gRegisteredThreadsMutex);

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

          int threadId = info->ThreadId();
          MOZ_ASSERT(threadId != my_tid);

          int64_t rssMemory = 0;
          int64_t ussMemory = 0;
          if (isFirstProfiledThread && gProfileMemory) {
            rssMemory = nsMemoryReporterManager::ResidentFast();
            ussMemory = nsMemoryReporterManager::ResidentUnique();
          }

          // Suspend the samplee thread and get its context.
          SigHandlerCoordinator coord;   // on sampler thread's stack
          sSigHandlerCoordinator = &coord;

          // Send message 1 to the samplee (the thread to be sampled), by
          // signalling at it.
          int r = tgkill(vm_tgid_, threadId, SIGPROF);
          MOZ_ASSERT(r == 0);

          // Wait for message 2 from the samplee, indicating that the context
          // is available and that the thread is suspended.
          while (true) {
            r = sem_wait(&sSigHandlerCoordinator->mMessage2);
            if (r == -1 && errno == EINTR) {
              // Interrupted by a signal.  Try again.
              continue;
            }
            // We don't expect any other kind of failure.
            MOZ_ASSERT(r == 0);
            break;
          }

          // The samplee thread is now frozen and
          // sSigHandlerCoordinator->mUContext is valid.  We can poke around
          // in it and unwind its stack as we like.

          TickSample sample;
          sample.context = &sSigHandlerCoordinator->mUContext;

          // Extract the current pc and sp.
          SetSampleContext(&sample,
                           sSigHandlerCoordinator->mUContext.uc_mcontext);
          sample.threadInfo = info;
          sample.timestamp = mozilla::TimeStamp::Now();
          sample.rssMemory = rssMemory;
          sample.ussMemory = ussMemory;

          Tick(gBuffer, &sample);

          // Send message 3 to the samplee, which tells it to resume.
          r = sem_post(&sSigHandlerCoordinator->mMessage3);
          MOZ_ASSERT(r == 0);

          // Wait for message 4 from the samplee, which tells us that it has
          // finished with |sSigHandlerCoordinator|.
          while (true) {
            r = sem_wait(&sSigHandlerCoordinator->mMessage4);
            if (r == -1 && errno == EINTR) {
              continue;
            }
            MOZ_ASSERT(r == 0);
            break;
          }

          // This isn't strictly necessary, but doing so does help pick up
          // anomalies in which the signal handler is running even though
          // this loop thinks it shouldn't be.
          sSigHandlerCoordinator = nullptr;

          isFirstProfiledThread = false;
        }
#if defined(USE_LUL_STACKWALK)
        // The LUL unwind object accumulates frame statistics. Periodically
        // we should poke it to give it a chance to print those statistics.
        // This involves doing I/O (fprintf, __android_log_print, etc.) and
        // so can't safely be done from the unwinder threads, which is why it
        // is done here.
        gLUL->MaybeShowStats();
#endif
      }

      TimeStamp targetSleepEndTime =
        sampleStart + TimeDuration::FromMicroseconds(mIntervalMicro);
      TimeStamp beforeSleep = TimeStamp::Now();
      TimeDuration targetSleepDuration = targetSleepEndTime - beforeSleep;
      double sleepTime = std::max(0.0, (targetSleepDuration - lastSleepOverhead).ToMicroseconds());
      SleepMicro(sleepTime);
      sampleStart = TimeStamp::Now();
      lastSleepOverhead = sampleStart - (beforeSleep + TimeDuration::FromMicroseconds(sleepTime));
    }
  }

private:
  // The pthread_t for the sampler thread.
  pthread_t mThread;

  // The interval between samples, measured in microseconds.
  const int mIntervalMicro;

  // Used to restore the SIGPROF handler when ours is removed.
  struct sigaction mOldSigprofHandler;

  static SamplerThread* sInstance;

  // This is the one-and-only variable used to communicate between the sampler
  // thread and the samplee thread's signal handler. It's static because the
  // samplee thread's signal handler is static.
  static SigHandlerCoordinator* sSigHandlerCoordinator;

  SamplerThread(const SamplerThread&) = delete;
  void operator=(const SamplerThread&) = delete;
};

SamplerThread* SamplerThread::sInstance = nullptr;
SamplerThread::SigHandlerCoordinator* SamplerThread::sSigHandlerCoordinator =
  nullptr;

#if defined(GP_OS_android)
static struct sigaction gOldSigstartHandler;
const int SIGSTART = SIGUSR2;

static void freeArray(const char** array, int size) {
  for (int i = 0; i < size; i++) {
    free((void*) array[i]);
  }
}

static uint32_t readCSVArray(char* csvList, const char** buffer) {
  uint32_t count;
  char* savePtr;
  int newlinePos = strlen(csvList) - 1;
  if (csvList[newlinePos] == '\n') {
    csvList[newlinePos] = '\0';
  }

  char* item = strtok_r(csvList, ",", &savePtr);
  for (count = 0; item; item = strtok_r(nullptr, ",", &savePtr)) {
    int length = strlen(item) + 1;  // Include \0
    char* newBuf = (char*) malloc(sizeof(char) * length);
    buffer[count] = newBuf;
    strncpy(newBuf, item, length);
    count++;
  }

  return count;
}

// Support some of the env variables reported in ReadProfilerEnvVars, plus some
// extra stuff.
static void
ReadProfilerVars(const char* fileName,
                 const char** features, uint32_t* featureCount,
                 const char** threadNames, uint32_t* threadCount) {
  FILE* file = fopen(fileName, "r");
  const int bufferSize = 1024;
  char line[bufferSize];
  char* feature;
  char* value;
  char* savePtr;

  if (file) {
    while (fgets(line, bufferSize, file) != nullptr) {
      feature = strtok_r(line, "=", &savePtr);
      value = strtok_r(nullptr, "", &savePtr);

      if (strncmp(feature, "MOZ_PROFILER_INTERVAL", bufferSize) == 0) {
        set_profiler_interval(value);
      } else if (strncmp(feature, "MOZ_PROFILER_ENTRIES", bufferSize) == 0) {
        set_profiler_entries(value);
      } else if (strncmp(feature, "MOZ_PROFILER_FEATURES", bufferSize) == 0) {
        *featureCount = readCSVArray(value, features);
      } else if (strncmp(feature, "threads", bufferSize) == 0) {
        *threadCount = readCSVArray(value, threadNames);
      }
    }

    fclose(file);
  }
}

static void DoStartTask() {
  uint32_t featureCount = 0;
  uint32_t threadCount = 0;

  // Just allocate 10 features for now
  // FIXME: these don't really point to const chars*
  // So we free them later, but we don't want to change the const char**
  // declaration in profiler_start. Annoying but ok for now.
  const char* threadNames[10];
  const char* features[10];
  const char* profilerConfigFile = "/data/local/tmp/profiler.options";

  ReadProfilerVars(profilerConfigFile, features, &featureCount, threadNames, &threadCount);
  MOZ_ASSERT(featureCount < 10);
  MOZ_ASSERT(threadCount < 10);

  profiler_start(PROFILE_DEFAULT_ENTRIES, /* interval */ 1,
                 features, featureCount, threadNames, threadCount);

  freeArray(threadNames, threadCount);
  freeArray(features, featureCount);
}

static void StartSignalHandler(int signal, siginfo_t* info, void* context) {
  class StartTask : public Runnable {
  public:
    NS_IMETHOD Run() override {
      DoStartTask();
      return NS_OK;
    }
  };
  // XXX: technically NS_DispatchToMainThread is NOT async signal safe. We risk
  // nasty things like deadlocks, but the probability is very low and we
  // typically only do this once so it tends to be ok. See bug 909403.
  NS_DispatchToMainThread(new StartTask());
}

static void
PlatformInit()
{
  LOG("Registering start signal");
  struct sigaction sa;
  sa.sa_sigaction = StartSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGSTART, &sa, &gOldSigstartHandler) != 0) {
    MOZ_CRASH("Error installing SIGSTART handler in the profiler");
  }
}

#else

static void
PlatformInit()
{
  // Set up the fork handlers.
  setup_atfork();
}

#endif

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

void TickSample::PopulateContext(void* aContext)
{
  MOZ_ASSERT(aContext);
  ucontext_t* pContext = reinterpret_cast<ucontext_t*>(aContext);
  if (!getcontext(pContext)) {
    context = pContext;
    SetSampleContext(this,
                     reinterpret_cast<ucontext_t*>(aContext)->uc_mcontext);
  }
}

