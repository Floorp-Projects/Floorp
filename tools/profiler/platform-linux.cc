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

/*
# vim: sw=2
*/
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
#ifdef ANDROID
#include <android/log.h>
#else
#define __android_log_print(a, ...)
#endif
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
#include "platform.h"
#include "GeckoProfiler.h"
#include "mozilla/Mutex.h"
#include "mozilla/Atomics.h"
#include "mozilla/LinuxSignal.h"
#include "ProfileEntry.h"
#include "nsThreadUtils.h"
#include "TableTicker.h"
#include "ThreadResponsiveness.h"
#include "UnwinderThread2.h"
#if defined(__ARM_EABI__) && defined(MOZ_WIDGET_GONK)
 // Should also work on other Android and ARM Linux, but not tested there yet.
#define USE_EHABI_STACKWALK
#include "EHABIStackWalk.h"
#endif

// Memory profile
#include "nsMemoryReporterManager.h"

#include <string.h>
#include <stdio.h>
#include <list>

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

#define SIGNAL_SAVE_PROFILE SIGUSR2

#if defined(__GLIBC__)
// glibc doesn't implement gettid(2).
#include <sys/syscall.h>
pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#endif

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return gettid();
}

#if !defined(ANDROID)
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
static bool was_paused = false;

// In the parent, just before the fork, record the pausedness state,
// and then pause.
static void paf_prepare(void) {
  if (Sampler::GetActiveSampler()) {
    was_paused = Sampler::GetActiveSampler()->IsPaused();
    Sampler::GetActiveSampler()->SetPaused(true);
  } else {
    was_paused = false;
  }
}

// In the parent, just after the fork, return pausedness to the
// pre-fork state.
static void paf_parent(void) {
  if (Sampler::GetActiveSampler())
    Sampler::GetActiveSampler()->SetPaused(was_paused);
}

// Set up the fork handlers.
static void* setup_atfork() {
  pthread_atfork(paf_prepare, paf_parent, NULL);
  return NULL;
}
#endif /* !defined(ANDROID) */

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

static mozilla::Atomic<ThreadProfile*> sCurrentThreadProfile;
static sem_t sSignalHandlingDone;

static void ProfilerSaveSignalHandler(int signal, siginfo_t* info, void* context) {
  Sampler::GetActiveSampler()->RequestSave();
}

static void SetSampleContext(TickSample* sample, void* context)
{
  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  mcontext_t& mcontext = ucontext->uc_mcontext;
#if V8_HOST_ARCH_IA32
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_EIP]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_ESP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_EBP]);
#elif V8_HOST_ARCH_X64
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_RIP]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_RSP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_RBP]);
#elif V8_HOST_ARCH_ARM
// An undefined macro evaluates to 0, so this applies to Android's Bionic also.
#if !defined(ANDROID) && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 3))
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[R15]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[R13]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[R11]);
#ifdef ENABLE_ARM_LR_SAVING
  sample->lr = reinterpret_cast<Address>(mcontext.gregs[R14]);
#endif
#else
  sample->pc = reinterpret_cast<Address>(mcontext.arm_pc);
  sample->sp = reinterpret_cast<Address>(mcontext.arm_sp);
  sample->fp = reinterpret_cast<Address>(mcontext.arm_fp);
#ifdef ENABLE_ARM_LR_SAVING
  sample->lr = reinterpret_cast<Address>(mcontext.arm_lr);
#endif
#endif
#elif V8_HOST_ARCH_MIPS
  // Implement this on MIPS.
  UNIMPLEMENTED();
#endif
}

#ifdef ANDROID
#define V8_HOST_ARCH_ARM 1
#define SYS_gettid __NR_gettid
#define SYS_tgkill __NR_tgkill
#else
#define V8_HOST_ARCH_X64 1
#endif

namespace {

void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
  if (!Sampler::GetActiveSampler()) {
    sem_post(&sSignalHandlingDone);
    return;
  }

  TickSample sample_obj;
  TickSample* sample = &sample_obj;
  sample->context = context;

  // If profiling, we extract the current pc and sp.
  if (Sampler::GetActiveSampler()->IsProfiling()) {
    SetSampleContext(sample, context);
  }
  sample->threadProfile = sCurrentThreadProfile;
  sample->timestamp = mozilla::TimeStamp::Now();
  sample->rssMemory = sample->threadProfile->mRssMemory;
  sample->ussMemory = sample->threadProfile->mUssMemory;

  Sampler::GetActiveSampler()->Tick(sample);

  sCurrentThreadProfile = NULL;
  sem_post(&sSignalHandlingDone);
}

} // namespace

static void ProfilerSignalThread(ThreadProfile *profile,
                                 bool isFirstProfiledThread)
{
  if (isFirstProfiledThread && Sampler::GetActiveSampler()->ProfileMemory()) {
    profile->mRssMemory = nsMemoryReporterManager::ResidentFast();
    profile->mUssMemory = nsMemoryReporterManager::ResidentUnique();
  } else {
    profile->mRssMemory = 0;
    profile->mUssMemory = 0;
  }
}

// If the Nuwa process is enabled, we need to use the wrapper of tgkill() to
// perform the mapping of thread ID.
#ifdef MOZ_NUWA_PROCESS
extern "C" MFBT_API int tgkill(pid_t tgid, pid_t tid, int signalno);
#else
int tgkill(pid_t tgid, pid_t tid, int signalno) {
  return syscall(SYS_tgkill, tgid, tid, signalno);
}
#endif

class PlatformData : public Malloced {
 public:
  PlatformData()
  {}
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

static void* SignalSender(void* arg) {
  // Taken from platform_thread_posix.cc
  prctl(PR_SET_NAME, "SamplerThread", 0, 0, 0);

#ifdef MOZ_NUWA_PROCESS
  // If the Nuwa process is enabled, we need to mark and freeze the sampler
  // thread in the Nuwa process and have this thread recreated in the spawned
  // child.
  if(IsNuwaProcess()) {
    NuwaMarkCurrentThread(nullptr, nullptr);
    // Freeze the thread here so the spawned child will get the correct tgid
    // from the getpid() call below.
    NuwaFreezeCurrentThread();
  }
#endif

  int vm_tgid_ = getpid();

  while (SamplerRegistry::sampler->IsActive()) {
    SamplerRegistry::sampler->HandleSaveRequest();

    if (!SamplerRegistry::sampler->IsPaused()) {
      mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);
      std::vector<ThreadInfo*> threads =
        SamplerRegistry::sampler->GetRegisteredThreads();

      bool isFirstProfiledThread = true;
      for (uint32_t i = 0; i < threads.size(); i++) {
        ThreadInfo* info = threads[i];

        // This will be null if we're not interested in profiling this thread.
        if (!info->Profile())
          continue;

        PseudoStack::SleepState sleeping = info->Stack()->observeSleeping();
        if (sleeping == PseudoStack::SLEEPING_AGAIN) {
          info->Profile()->DuplicateLastSample();
          //XXX: This causes flushes regardless of jank-only mode
          info->Profile()->flush();
          continue;
        }

        info->Profile()->GetThreadResponsiveness()->Update();

        // We use sCurrentThreadProfile the ThreadProfile for the
        // thread we're profiling to the signal handler
        sCurrentThreadProfile = info->Profile();

        int threadId = info->ThreadId();

        // Profile from the signal sender for information which is not signal
        // safe, and will have low variation between the emission of the signal
        // and the signal handler catch.
        ProfilerSignalThread(sCurrentThreadProfile, isFirstProfiledThread);

        // Profile from the signal handler for information which is signal safe
        // and needs to be precise too, such as the stack of the interrupted
        // thread.
        if (tgkill(vm_tgid_, threadId, SIGPROF) != 0) {
          printf_stderr("profiler failed to signal tid=%d\n", threadId);
#ifdef DEBUG
          abort();
#endif
          continue;
        }

        // Wait for the signal handler to run before moving on to the next one
        sem_wait(&sSignalHandlingDone);
        isFirstProfiledThread = false;
      }
    }

    // Convert ms to us and subtract 100 us to compensate delays
    // occuring during signal delivery.
    // TODO measure and confirm this.
    int interval = floor(SamplerRegistry::sampler->interval() * 1000 + 0.5) - 100;
    if (interval <= 0) {
      interval = 1;
    }
    OS::SleepMicro(interval);
  }
  return 0;
}

Sampler::Sampler(double interval, bool profiling, int entrySize)
    : interval_(interval),
      profiling_(profiling),
      paused_(false),
      active_(false),
      entrySize_(entrySize) {
}

Sampler::~Sampler() {
  ASSERT(!signal_sender_launched_);
}


void Sampler::Start() {
  LOG("Sampler started");

#ifdef USE_EHABI_STACKWALK
  mozilla::EHABIStackWalkInit();
#endif
  SamplerRegistry::AddActiveSampler(this);

  // Initialize signal handler communication
  sCurrentThreadProfile = NULL;
  if (sem_init(&sSignalHandlingDone, /* pshared: */ 0, /* value: */ 0) != 0) {
    LOG("Error initializing semaphore");
    return;
  }

  // Request profiling signals.
  LOG("Request signal");
  struct sigaction sa;
  sa.sa_sigaction = MOZ_SIGNAL_TRAMPOLINE(ProfilerSignalHandler);
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGPROF, &sa, &old_sigprof_signal_handler_) != 0) {
    LOG("Error installing signal");
    return;
  }

  // Request save profile signals
  struct sigaction sa2;
  sa2.sa_sigaction = ProfilerSaveSignalHandler;
  sigemptyset(&sa2.sa_mask);
  sa2.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGNAL_SAVE_PROFILE, &sa2, &old_sigsave_signal_handler_) != 0) {
    LOG("Error installing start signal");
    return;
  }
  LOG("Signal installed");
  signal_handler_installed_ = true;

  // Start a thread that sends SIGPROF signal to VM thread.
  // Sending the signal ourselves instead of relying on itimer provides
  // much better accuracy.
  SetActive(true);
  if (pthread_create(
        &signal_sender_thread_, NULL, SignalSender, NULL) == 0) {
    signal_sender_launched_ = true;
  }
  LOG("Profiler thread started");
}


void Sampler::Stop() {
  SetActive(false);

  // Wait for signal sender termination (it will exit after setting
  // active_ to false).
  if (signal_sender_launched_) {
    pthread_join(signal_sender_thread_, NULL);
    signal_sender_launched_ = false;
  }

  SamplerRegistry::RemoveActiveSampler(this);

  // Restore old signal handler
  if (signal_handler_installed_) {
    sigaction(SIGNAL_SAVE_PROFILE, &old_sigsave_signal_handler_, 0);
    sigaction(SIGPROF, &old_sigprof_signal_handler_, 0);
    signal_handler_installed_ = false;
  }
}

bool Sampler::RegisterCurrentThread(const char* aName,
                                    PseudoStack* aPseudoStack,
                                    bool aIsMainThread, void* stackTop)
{
  if (!Sampler::sRegisteredThreadsMutex)
    return false;

  mozilla::MutexAutoLock lock(*Sampler::sRegisteredThreadsMutex);

  int id = gettid();
  for (uint32_t i = 0; i < sRegisteredThreads->size(); i++) {
    ThreadInfo* info = sRegisteredThreads->at(i);
    if (info->ThreadId() == id) {
      // Thread already registered. This means the first unregister will be
      // too early.
      ASSERT(false);
      return false;
    }
  }

  set_tls_stack_top(stackTop);

  ThreadInfo* info = new ThreadInfo(aName, id,
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

  tlsStackTop.set(nullptr);

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

  uwt__unregister_thread_for_profiling();
}

#ifdef ANDROID
static struct sigaction old_sigstart_signal_handler;
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
  for (count = 0; item; item = strtok_r(NULL, ",", &savePtr)) {
    int length = strlen(item) + 1;  // Include \0
    char* newBuf = (char*) malloc(sizeof(char) * length);
    buffer[count] = newBuf;
    strncpy(newBuf, item, length);
    count++;
  }

  return count;
}

// Currently support only the env variables
// reported in read_profiler_env
static void ReadProfilerVars(const char* fileName, const char** features,
                            uint32_t* featureCount, const char** threadNames, uint32_t* threadCount) {
  FILE* file = fopen(fileName, "r");
  const int bufferSize = 1024;
  char line[bufferSize];
  char* feature;
  char* value;
  char* savePtr;

  if (file) {
    while (fgets(line, bufferSize, file) != NULL) {
      feature = strtok_r(line, "=", &savePtr);
      value = strtok_r(NULL, "", &savePtr);

      if (strncmp(feature, PROFILER_MODE, bufferSize) == 0) {
        set_profiler_mode(value);
      } else if (strncmp(feature, PROFILER_INTERVAL, bufferSize) == 0) {
        set_profiler_interval(value);
      } else if (strncmp(feature, PROFILER_ENTRIES, bufferSize) == 0) {
        set_profiler_entries(value);
      } else if (strncmp(feature, PROFILER_STACK, bufferSize) == 0) {
        set_profiler_scan(value);
      } else if (strncmp(feature, PROFILER_FEATURES, bufferSize) == 0) {
        *featureCount = readCSVArray(value, features);
      } else if (strncmp(feature, "threads", bufferSize) == 0) {
        *threadCount = readCSVArray(value, threadNames);
      }
    }

    fclose(file);
  }
}


static void StartSignalHandler(int signal, siginfo_t* info, void* context) {
  // XXX: Everything we do here is NOT async signal safe. We risk nasty things
  // like deadlocks but we typically only do this once so it tends to be ok.
  // See bug 909403
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

  profiler_start(PROFILE_DEFAULT_ENTRY, 1,
      features, featureCount,
      threadNames, threadCount);

  freeArray(threadNames, threadCount);
  freeArray(features, featureCount);
}

void OS::Startup()
{
  LOG("Registering start signal");
  struct sigaction sa;
  sa.sa_sigaction = StartSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGSTART, &sa, &old_sigstart_signal_handler) != 0) {
    LOG("Error installing signal");
  }
}

#else

void OS::Startup() {
  // Set up the fork handlers.
  setup_atfork();
}

#endif



void TickSample::PopulateContext(void* aContext)
{
  MOZ_ASSERT(aContext);
  ucontext_t* pContext = reinterpret_cast<ucontext_t*>(aContext);
  if (!getcontext(pContext)) {
    context = pContext;
    SetSampleContext(this, aContext);
  }
}

// WARNING: Works with values up to 1 second
void OS::SleepMicro(int microseconds)
{
  struct timespec ts;
  ts.tv_sec  = 0;
  ts.tv_nsec = microseconds * 1000UL;

  while (true) {
    // in the case of interrupt we keep waiting
    // nanosleep puts the remaining to back into ts
    if (!nanosleep(&ts, &ts) || errno != EINTR) {
      return;
    }
  }
}

