// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
# vim: sw=2
*/
#include <stdio.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#ifdef ANDROID
#include <android/log.h>
#else
#define __android_log_print(a, ...)
#endif
// Ubuntu Dapper requires memory pages to be marked as
// executable. Otherwise, OS raises an exception when executing code
// in that page.
#include <sys/types.h>  // mmap & munmap
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <fcntl.h>      // open
#include <unistd.h>     // sysconf
#ifdef __GLIBC__
#include <execinfo.h>   // backtrace, backtrace_symbols
#endif  // def __GLIBC__
#include <strings.h>    // index
#include <errno.h>
#include <stdarg.h>
#include "platform.h"
#include "sps_sampler.h"

#include <string.h>
#include <stdio.h>

#define SIGNAL_SAVE_PROFILE SIGUSR2

#if defined(__GLIBC__)
// glibc doesn't implement gettid(2).
#include <sys/syscall.h>
pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#endif

static Sampler* sActiveSampler = NULL;


#if !defined(__GLIBC__) && (defined(__arm__) || defined(__thumb__))
#include "android-signal-defs.h"
#endif

static void ProfilerSaveSignalHandler(int signal, siginfo_t* info, void* context) {
  sActiveSampler->RequestSave();
}

#ifdef ANDROID
#define V8_HOST_ARCH_ARM 1
#define SYS_gettid __NR_gettid
#define SYS_tgkill __NR_tgkill
#else
#define V8_HOST_ARCH_X64 1
#endif
static void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
  if (!sActiveSampler)
    return;

  TickSample sample_obj;
  TickSample* sample = &sample_obj;
  sample->context = context;

#ifdef ENABLE_SPS_LEAF_DATA
  // If profiling, we extract the current pc and sp.
  if (sActiveSampler->IsProfiling()) {
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
#if (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 3))
    sample->pc = reinterpret_cast<Address>(mcontext.gregs[R15]);
    sample->sp = reinterpret_cast<Address>(mcontext.gregs[R13]);
    sample->fp = reinterpret_cast<Address>(mcontext.gregs[R11]);
#else
    sample->pc = reinterpret_cast<Address>(mcontext.arm_pc);
    sample->sp = reinterpret_cast<Address>(mcontext.arm_sp);
    sample->fp = reinterpret_cast<Address>(mcontext.arm_fp);
#endif
#elif V8_HOST_ARCH_MIPS
    // Implement this on MIPS.
    UNIMPLEMENTED();
#endif
  }
#endif
  sample->timestamp = mozilla::TimeStamp::Now();

  sActiveSampler->Tick(sample);
}

#ifndef XP_MACOSX
void tgkill(pid_t tgid, pid_t tid, int signalno) {
  syscall(SYS_tgkill, tgid, tid, signalno);
}
#endif

class Sampler::PlatformData : public Malloced {
 public:
  explicit PlatformData(Sampler* sampler)
      : sampler_(sampler),
        signal_handler_installed_(false),
        vm_tgid_(getpid()),
#ifndef XP_MACOSX
        vm_tid_(gettid()),
#endif
        signal_sender_launched_(false)
#ifdef XP_MACOSX
        , signal_receiver_(pthread_self())
#endif
  {
  }

  void SignalSender() {
    while (sampler_->IsActive()) {
      sampler_->HandleSaveRequest();

#ifdef XP_MACOSX
      pthread_kill(signal_receiver_, SIGPROF);
#else
      // Glibc doesn't provide a wrapper for tgkill(2).
      tgkill(vm_tgid_, vm_tid_, SIGPROF);
#endif
      // Convert ms to us and subtract 100 us to compensate delays
      // occuring during signal delivery.

      // TODO measure and confirm this.
      const useconds_t interval = sampler_->interval_ * 1000 - 100;
      //int result = usleep(interval);
      usleep(interval);
      // sometimes usleep is defined as returning void
      int result = 0;
#ifdef DEBUG
      if (result != 0 && errno != EINTR) {
        LOG("SignalSender usleep error");
        ASSERT(result == 0 || errno == EINTR);
      }
#endif
      mozilla::unused << result;
    }
  }

  Sampler* sampler_;
  bool signal_handler_installed_;
  struct sigaction old_sigprof_signal_handler_;
  struct sigaction old_sigsave_signal_handler_;
  pid_t vm_tgid_;
  pid_t vm_tid_;
  bool signal_sender_launched_;
  pthread_t signal_sender_thread_;
#ifdef XP_MACOSX
  pthread_t signal_receiver_;
#endif
};


static void* SenderEntry(void* arg) {
  Sampler::PlatformData* data =
      reinterpret_cast<Sampler::PlatformData*>(arg);
  data->SignalSender();
  return 0;
}


Sampler::Sampler(int interval, bool profiling)
    : interval_(interval),
      profiling_(profiling),
      active_(false) {
  data_ = new PlatformData(this);
}

Sampler::~Sampler() {
  ASSERT(!data_->signal_sender_launched_);
  delete data_;
}


void Sampler::Start() {
  LOG("Sampler Started");
  if (sActiveSampler != NULL) return;

  // Request profiling signals.
  LOG("Request signal");
  struct sigaction sa;
  sa.sa_sigaction = ProfilerSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGPROF, &sa, &data_->old_sigprof_signal_handler_) != 0) {
    LOG("Error installing signal");
    return;
  }

  // Request save profile signals
  struct sigaction sa2;
  sa2.sa_sigaction = ProfilerSaveSignalHandler;
  sigemptyset(&sa2.sa_mask);
  sa2.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGNAL_SAVE_PROFILE, &sa2, &data_->old_sigsave_signal_handler_) != 0) {
    LOG("Error installing start signal");
    return;
  }
  LOG("Signal installed");
  data_->signal_handler_installed_ = true;

  // Start a thread that sends SIGPROF signal to VM thread.
  // Sending the signal ourselves instead of relying on itimer provides
  // much better accuracy.
  SetActive(true);
  if (pthread_create(
          &data_->signal_sender_thread_, NULL, SenderEntry, data_) == 0) {
    data_->signal_sender_launched_ = true;
  }
  LOG("Profiler thread started");

  // Set this sampler as the active sampler.
  sActiveSampler = this;
}


void Sampler::Stop() {
  SetActive(false);

  // Wait for signal sender termination (it will exit after setting
  // active_ to false).
  if (data_->signal_sender_launched_) {
    pthread_join(data_->signal_sender_thread_, NULL);
    data_->signal_sender_launched_ = false;
  }

  // Restore old signal handler
  if (data_->signal_handler_installed_) {
    sigaction(SIGPROF, &data_->old_sigsave_signal_handler_, 0);
    sigaction(SIGPROF, &data_->old_sigprof_signal_handler_, 0);
    data_->signal_handler_installed_ = false;
  }

  // This sampler is no longer the active sampler.
  sActiveSampler = NULL;
}

static struct sigaction old_sigstartstop_signal_handler;
const int SIGSTARTSTOP = SIGUSR1;

static void StartStopSignalHandler(int signal, siginfo_t* info, void* context) {
  mozilla_sampler_start(PROFILE_DEFAULT_ENTRY, PROFILE_DEFAULT_INTERVAL,
                        PROFILE_DEFAULT_FEATURES, PROFILE_DEFAULT_FEATURE_COUNT);
}

void OS::RegisterStartStopHandlers()
{
  LOG("Registering start/stop signal");
  struct sigaction sa;
  sa.sa_sigaction = StartStopSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGSTARTSTOP, &sa, &old_sigstartstop_signal_handler) != 0) {
    LOG("Error installing signal");
  }
}

