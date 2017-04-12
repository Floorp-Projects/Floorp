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

#include <string.h>
#include <list>

using namespace mozilla;

/* static */ Thread::tid_t
Thread::GetCurrentId()
{
  return gettid();
}

static void
FillInSample(TickSample& aSample, ucontext_t* aContext)
{
  aSample.mContext = aContext;
  mcontext_t& mcontext = aContext->uc_mcontext;

  // Extracting the sample from the context is extremely machine dependent.
#if defined(GP_ARCH_x86)
  aSample.mPC = reinterpret_cast<Address>(mcontext.gregs[REG_EIP]);
  aSample.mSP = reinterpret_cast<Address>(mcontext.gregs[REG_ESP]);
  aSample.mFP = reinterpret_cast<Address>(mcontext.gregs[REG_EBP]);
#elif defined(GP_ARCH_amd64)
  aSample.mPC = reinterpret_cast<Address>(mcontext.gregs[REG_RIP]);
  aSample.mSP = reinterpret_cast<Address>(mcontext.gregs[REG_RSP]);
  aSample.mFP = reinterpret_cast<Address>(mcontext.gregs[REG_RBP]);
#elif defined(GP_ARCH_arm)
  aSample.mPC = reinterpret_cast<Address>(mcontext.arm_pc);
  aSample.mSP = reinterpret_cast<Address>(mcontext.arm_sp);
  aSample.mFP = reinterpret_cast<Address>(mcontext.arm_fp);
  aSample.mLR = reinterpret_cast<Address>(mcontext.arm_lr);
#else
# error "bad platform"
#endif
}

#if defined(GP_OS_android)
# define SYS_tgkill __NR_tgkill
#endif

int
tgkill(pid_t tgid, pid_t tid, int signalno)
{
  return syscall(SYS_tgkill, tgid, tid, signalno);
}

static void
SleepMicro(int aMicroseconds)
{
  aMicroseconds = std::max(0, aMicroseconds);

  if (aMicroseconds >= 1000000) {
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

class PlatformData
{
public:
  explicit PlatformData(int aThreadId)
  {
    MOZ_COUNT_CTOR(PlatformData);
  }

  ~PlatformData()
  {
    MOZ_COUNT_DTOR(PlatformData);
  }
};

////////////////////////////////////////////////////////////////////////
// BEGIN SamplerThread target specifics

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

struct SigHandlerCoordinator* SamplerThread::sSigHandlerCoordinator = nullptr;

static void
SigprofHandler(int aSignal, siginfo_t* aInfo, void* aContext)
{
  // Avoid TSan warning about clobbering errno.
  int savedErrno = errno;

  MOZ_ASSERT(aSignal == SIGPROF);
  MOZ_ASSERT(SamplerThread::sSigHandlerCoordinator);

  // By sending us this signal, the sampler thread has sent us message 1 in
  // the comment above, with the meaning "|sSigHandlerCoordinator| is ready
  // for use, please copy your register context into it."
  SamplerThread::sSigHandlerCoordinator->mUContext =
    *static_cast<ucontext_t*>(aContext);

  // Send message 2: tell the sampler thread that the context has been copied
  // into |sSigHandlerCoordinator->mUContext|.  sem_post can never fail by
  // being interrupted by a signal, so there's no loop around this call.
  int r = sem_post(&SamplerThread::sSigHandlerCoordinator->mMessage2);
  MOZ_ASSERT(r == 0);

  // At this point, the sampler thread assumes we are suspended, so we must
  // not touch any global state here.

  // Wait for message 3: the sampler thread tells us to resume.
  while (true) {
    r = sem_wait(&SamplerThread::sSigHandlerCoordinator->mMessage3);
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
  r = sem_post(&SamplerThread::sSigHandlerCoordinator->mMessage4);
  MOZ_ASSERT(r == 0);

  errno = savedErrno;
}

static void*
ThreadEntry(void* aArg)
{
  auto thread = static_cast<SamplerThread*>(aArg);
  prctl(PR_SET_NAME, "SamplerThread", 0, 0, 0);
  thread->mSamplerTid = gettid();
  thread->Run();
  return nullptr;
}

SamplerThread::SamplerThread(PS::LockRef aLock, uint32_t aActivityGeneration,
                             double aIntervalMilliseconds)
  : mActivityGeneration(aActivityGeneration)
  , mIntervalMicroseconds(
      std::max(1, int(floor(aIntervalMilliseconds * 1000 + 0.5))))
  , mMyPid(getpid())
  // We don't know what the sampler thread's ID will be until it runs, so set
  // mSamplerTid to a dummy value and fill it in for real in ThreadEntry().
  , mSamplerTid(-1)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

#if defined(USE_EHABI_STACKWALK)
  mozilla::EHABIStackWalkInit();
#elif defined(USE_LUL_STACKWALK)
  bool createdLUL = false;
  lul::LUL* lul = gPS->LUL(aLock);
  if (!lul) {
    lul = new lul::LUL(logging_sink_for_LUL);
    gPS->SetLUL(aLock, lul);
    // Read all the unwind info currently available.
    read_procmaps(lul);
    createdLUL = true;
  }
#endif

  // Request profiling signals.
  struct sigaction sa;
  sa.sa_sigaction = MOZ_SIGNAL_TRAMPOLINE(SigprofHandler);
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGPROF, &sa, &mOldSigprofHandler) != 0) {
    MOZ_CRASH("Error installing SIGPROF handler in the profiler");
  }

#if defined(USE_LUL_STACKWALK)
  if (createdLUL) {
    // Switch into unwind mode. After this point, we can't add or remove any
    // unwind info to/from this LUL instance. The only thing we can do with
    // it is Unwind() calls.
    lul->EnableUnwinding();

    // Has a test been requested?
    if (PR_GetEnv("MOZ_PROFILER_LUL_TEST")) {
      int nTests = 0, nTestsPassed = 0;
      RunLulUnitTests(&nTests, &nTestsPassed, lul);
    }
  }
#endif

  // Start the sampling thread. It repeatedly sends a SIGPROF signal. Sending
  // the signal ourselves instead of relying on itimer provides much better
  // accuracy.
  if (pthread_create(&mThread, nullptr, ThreadEntry, this) != 0) {
    MOZ_CRASH("pthread_create failed");
  }
}

SamplerThread::~SamplerThread()
{
  pthread_join(mThread, nullptr);
}

void
SamplerThread::Stop(PS::LockRef aLock)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Restore old signal handler. This is global state so it's important that
  // we do it now, while gPSMutex is locked. It's safe to do this now even
  // though this SamplerThread is still alive, because the next time the main
  // loop of Run() iterates it won't get past the mActivityGeneration check,
  // and so won't send any signals.
  sigaction(SIGPROF, &mOldSigprofHandler, 0);
}

void
SamplerThread::SuspendAndSampleAndResumeThread(PS::LockRef aLock,
                                               TickSample& aSample)
{
  // Only one sampler thread can be sampling at once.  So we expect to have
  // complete control over |sSigHandlerCoordinator|.
  MOZ_ASSERT(!sSigHandlerCoordinator);

  int sampleeTid = aSample.mThreadId;
  MOZ_RELEASE_ASSERT(sampleeTid != mSamplerTid);

  //----------------------------------------------------------------//
  // Suspend the samplee thread and get its context.

  SigHandlerCoordinator coord;   // on sampler thread's stack
  sSigHandlerCoordinator = &coord;

  // Send message 1 to the samplee (the thread to be sampled), by
  // signalling at it.
  int r = tgkill(mMyPid, sampleeTid, SIGPROF);
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

  //----------------------------------------------------------------//
  // Sample the target thread.

  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  //
  // The profiler's "critical section" begins here.  In the critical section,
  // we must not do any dynamic memory allocation, nor try to acquire any lock
  // or any other unshareable resource.  This is because the thread to be
  // sampled has been suspended at some entirely arbitrary point, and we have
  // no idea which unsharable resources (locks, essentially) it holds.  So any
  // attempt to acquire any lock, including the implied locks used by the
  // malloc implementation, risks deadlock.

  // The samplee thread is now frozen and sSigHandlerCoordinator->mUContext is
  // valid.  We can poke around in it and unwind its stack as we like.

  // Extract the current PC and sp.
  FillInSample(aSample, &sSigHandlerCoordinator->mUContext);

  Tick(aLock, gPS->Buffer(aLock), aSample);

  //----------------------------------------------------------------//
  // Resume the target thread.

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

  // The profiler's critical section ends here.  After this point, none of the
  // critical section limitations documented above apply.
  //
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // This isn't strictly necessary, but doing so does help pick up anomalies
  // in which the signal handler is running when it shouldn't be.
  sSigHandlerCoordinator = nullptr;
}

// END SamplerThread target specifics
////////////////////////////////////////////////////////////////////////

#if defined(GP_OS_linux)

// We use pthread_atfork() to temporarily disable signal delivery during any
// fork() call. Without that, fork() can be repeatedly interrupted by signal
// delivery, requiring it to be repeatedly restarted, which can lead to *long*
// delays. See bug 837390.
//
// We provide no paf_child() function to run in the child after forking. This
// is fine because we always immediately exec() after fork(), and exec()
// clobbers all process state. (At one point we did have a paf_child()
// function, but it caused problems related to locking gPSMutex. See bug
// 1348374.)
//
// Unfortunately all this is only doable on non-Android because Bionic doesn't
// have pthread_atfork.

// In the parent, before the fork, record IsPaused, and then pause.
static void
paf_prepare()
{
  // This function can run off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PS::AutoLock lock(gPSMutex);

  gPS->SetWasPaused(lock, gPS->IsPaused(lock));
  gPS->SetIsPaused(lock, true);
}

// In the parent, after the fork, return IsPaused to the pre-fork state.
static void
paf_parent()
{
  // This function can run off the main thread.

  MOZ_RELEASE_ASSERT(gPS);

  PS::AutoLock lock(gPSMutex);

  gPS->SetIsPaused(lock, gPS->WasPaused(lock));
  gPS->SetWasPaused(lock, false);
}

static void
PlatformInit(PS::LockRef aLock)
{
  // Set up the fork handlers.
  pthread_atfork(paf_prepare, paf_parent, nullptr);
}

#else

static void
PlatformInit(PS::LockRef aLock)
{
}

#endif

void
TickSample::PopulateContext(ucontext_t* aContext)
{
  MOZ_ASSERT(mIsSynchronous);
  MOZ_ASSERT(aContext);

  if (!getcontext(aContext)) {
    FillInSample(*this, aContext);
  }
}

