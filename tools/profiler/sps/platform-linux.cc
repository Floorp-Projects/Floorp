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
#include "v8-support.h"
#include "platform.h"

#include <string.h>
#include <stdio.h>

// Real time signals are not supported on android.
// This behaves as a standard signal.
#define SIGNAL_SAVE_PROFILE 42

#define PATH_MAX_TOSTRING(x) #x
#define PATH_MAX_STRING(x) PATH_MAX_TOSTRING(x)

#ifdef ENABLE_SPS_LEAF_DATA
/* a crapy version of getline, because it's not included in bionic */
static ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
 char *ret;
 if (!*lineptr) {
   *lineptr = (char*)malloc(4096);
 }
 ret = fgets(*lineptr, 4096, stream);
 if (!ret)
   return 0;
 return strlen(*lineptr);
}

MapInfo getmaps(pid_t pid)
{
 MapInfo info;
 char path[PATH_MAX];
 snprintf(path, PATH_MAX, "/proc/%d/maps", pid);
 FILE *maps = fopen(path, "r");
 char *line = NULL;
 int count = 0;
 size_t line_size = 0;
 while (maps && getline (&line, &line_size, maps) > 0) {
   int ret;
   //XXX: needs input sanitizing
   unsigned long start;
   unsigned long end;
   char perm[6] = "";
   unsigned long offset;
   char name[PATH_MAX] = "";
   ret = sscanf(line,
                "%lx-%lx %6s %lx %*s %*x %" PATH_MAX_STRING(PATH_MAX) "s\n",
                &start, &end, perm, &offset, name);
   if (!strchr(perm, 'x')) {
     // Ignore non executable entries
     continue;
   }
   if (ret != 5 && ret != 4) {
     LOG("Get maps line failed");
     continue;
   }
   MapEntry entry(start, end, offset, name);
   info.AddMapEntry(entry);
   if (count > 10000) {
     LOG("Get maps failed");
     break;
   }
   count++;
 }
 free(line);
 return info;
}
#endif

static Sampler* sActiveSampler = NULL;


#if !defined(__GLIBC__) && (defined(__arm__) || defined(__thumb__))
// Android runs a fairly new Linux kernel, so signal info is there,
// but the C library doesn't have the structs defined.

struct sigcontext {
  uint32_t trap_no;
  uint32_t error_code;
  uint32_t oldmask;
  uint32_t gregs[16];
  uint32_t arm_cpsr;
  uint32_t fault_address;
};
typedef uint32_t __sigset_t;
typedef struct sigcontext mcontext_t;
typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  __sigset_t uc_sigmask;
} ucontext_t;
enum ArmRegisters {R15 = 15, R13 = 13, R11 = 11};

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

#ifndef ENABLE_SPS_LEAF_DATA
  TickSample* sample = NULL;
#else
  TickSample sample_obj;
  TickSample* sample = &sample_obj;

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
  sActiveSampler->Tick(sample);
}

void tgkill(pid_t tgid, pid_t tid, int signalno) {
  syscall(SYS_tgkill, tgid, tid, signalno);
}

class Sampler::PlatformData : public Malloced {
 public:
  explicit PlatformData(Sampler* sampler)
      : sampler_(sampler),
        signal_handler_installed_(false),
        vm_tgid_(getpid()),
        // Glibc doesn't provide a wrapper for gettid(2).
        vm_tid_(gettid()),
        signal_sender_launched_(false) {
  }

  void SignalSender() {
    while (sampler_->IsActive()) {
      sampler_->HandleSaveRequest();

      // Glibc doesn't provide a wrapper for tgkill(2).
      tgkill(vm_tgid_, vm_tid_, SIGPROF);
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
      synchronous_(profiling),
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
  active_ = true;
  if (pthread_create(
          &data_->signal_sender_thread_, NULL, SenderEntry, data_) == 0) {
    data_->signal_sender_launched_ = true;
  }
  LOG("Profiler thread started");

  // Set this sampler as the active sampler.
  sActiveSampler = this;
}


void Sampler::Stop() {
  active_ = false;

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

