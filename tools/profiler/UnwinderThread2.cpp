/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef MOZ_VALGRIND
# include <valgrind/helgrind.h>
# include <valgrind/memcheck.h>
#else
# define VALGRIND_HG_MUTEX_LOCK_PRE(_mx,_istry)  /* */
# define VALGRIND_HG_MUTEX_LOCK_POST(_mx)        /* */
# define VALGRIND_HG_MUTEX_UNLOCK_PRE(_mx)       /* */
# define VALGRIND_HG_MUTEX_UNLOCK_POST(_mx)      /* */
# define VALGRIND_MAKE_MEM_DEFINED(_addr,_len)   ((void)0)
# define VALGRIND_MAKE_MEM_UNDEFINED(_addr,_len) ((void)0)
#endif

#include "prenv.h"
#include "mozilla/arm.h"
#include "mozilla/DebugOnly.h"
#include <stdint.h>
#include "PlatformMacros.h"

#include "platform.h"
#include <ostream>
#include <string>

#include "ProfileEntry.h"
#include "SyncProfile.h"
#include "AutoObjectMapper.h"
#include "UnwinderThread2.h"

#if !defined(SPS_OS_windows)
# include <sys/mman.h>
#endif

#if defined(SPS_OS_android) || defined(SPS_OS_linux)
# include <ucontext.h>
# include "LulMain.h"
#endif

#include "shared-libraries.h"


// Verbosity of this module, for debugging:
//   0  silent
//   1  adds info about debuginfo load success/failure
//   2  adds slow-summary stats for buffer fills/misses (RECOMMENDED)
//   3  adds per-sample summary lines
//   4  adds per-sample frame listing
// Note that level 3 and above produces risk of deadlock, and 
// are not recommended for extended use.
#define LOGLEVEL 2

// The maximum number of frames that the native unwinder will
// produce.  Setting it too high gives a risk of it wasting a
// lot of time looping on corrupted stacks.
#define MAX_NATIVE_FRAMES 256


// The 'else' of this covers the entire rest of the file
#if defined(SPS_OS_windows) || defined(SPS_OS_darwin)

//////////////////////////////////////////////////////////
//// BEGIN externally visible functions (WINDOWS and OSX STUBS)

// On Windows and OSX this will all need reworking.
// GeckoProfilerImpl.h will ensure these functions are never actually
// called, so just provide no-op stubs for now.

void uwt__init()
{
}

void uwt__stop()
{
}

void uwt__deinit()
{
}

void uwt__register_thread_for_profiling ( void* stackTop )
{
}

void uwt__unregister_thread_for_profiling()
{
}

LinkedUWTBuffer* utb__acquire_sync_buffer(void* stackTop)
{
  return nullptr;
}

// RUNS IN SIGHANDLER CONTEXT
UnwinderThreadBuffer* uwt__acquire_empty_buffer()
{
  return nullptr;
}

void
utb__finish_sync_buffer(ThreadProfile* aProfile,
                        UnwinderThreadBuffer* utb,
                        void* /* ucontext_t*, really */ ucV)
{
}

void
utb__release_sync_buffer(LinkedUWTBuffer* utb)
{
}

void
utb__end_sync_buffer_unwind(LinkedUWTBuffer* utb)
{
}

// RUNS IN SIGHANDLER CONTEXT
void
uwt__release_full_buffer(ThreadProfile* aProfile,
                         UnwinderThreadBuffer* utb,
                         void* /* ucontext_t*, really */ ucV )
{
}

// RUNS IN SIGHANDLER CONTEXT
void
utb__addEntry(/*MODIFIED*/UnwinderThreadBuffer* utb, ProfileEntry ent)
{
}

//// END externally visible functions (WINDOWS and OSX STUBS)
//////////////////////////////////////////////////////////

#else // a supported target

//////////////////////////////////////////////////////////
//// BEGIN externally visible functions

// Forward references
// the unwinder thread ID, its fn, and a stop-now flag
static void* unwind_thr_fn ( void* exit_nowV );
static pthread_t unwind_thr;
static int       unwind_thr_exit_now = 0; // RACED ON

// Threads must be registered with this file before they can be
// sampled.  So that we know the max safe stack address for each
// registered thread.
static void thread_register_for_profiling ( void* stackTop );

// Unregister a thread.
static void thread_unregister_for_profiling();

// Empties out the buffer queue.  Used when the unwinder thread is
// shut down.
static void empty_buffer_queue();

// Allocate a buffer for synchronous unwinding
static LinkedUWTBuffer* acquire_sync_buffer(void* stackTop);

// RUNS IN SIGHANDLER CONTEXT
// Acquire an empty buffer and mark it as FILLING
static UnwinderThreadBuffer* acquire_empty_buffer();

static void finish_sync_buffer(ThreadProfile* aProfile,
                               UnwinderThreadBuffer* utb,
                               void* /* ucontext_t*, really */ ucV);

// Release an empty synchronous unwind buffer.
static void release_sync_buffer(LinkedUWTBuffer* utb);

// Unwind complete, mark a synchronous unwind buffer as empty
static void end_sync_buffer_unwind(LinkedUWTBuffer* utb);

// RUNS IN SIGHANDLER CONTEXT
// Put this buffer in the queue of stuff going to the unwinder
// thread, and mark it as FULL.  Before doing that, fill in stack
// chunk and register fields if a native unwind is requested.
// APROFILE is where the profile data should be added to.  UTB
// is the partially-filled-in buffer, containing ProfileEntries.
// UCV is the ucontext_t* from the signal handler.  If non-nullptr,
// is taken as a cue to request native unwind.
static void release_full_buffer(ThreadProfile* aProfile,
                                UnwinderThreadBuffer* utb,
                                void* /* ucontext_t*, really */ ucV );

// RUNS IN SIGHANDLER CONTEXT
static void utb_add_prof_ent(UnwinderThreadBuffer* utb, ProfileEntry ent);

// Do a store memory barrier.
static void do_MBAR();


// This is the single instance of the LUL unwind library that we will
// use.  Currently the library is operated with multiple sampling
// threads but only one unwinder thread.  It should also be possible
// to use the library with multiple unwinder threads, to improve
// throughput.  The setup here makes it possible to use multiple
// unwinder threads, although that is as-yet untested.
//
// |sLULmutex| protects |sLUL| and |sLULcount| and also is used to
// ensure that only the first unwinder thread requests |sLUL| to read
// debug info.  |sLUL| may only be assigned to (and the object it
// points at may only be created/destroyed) when |sLULcount| is zero.
// |sLULcount| holds the number of unwinder threads currently in
// existence.
static pthread_mutex_t sLULmutex = PTHREAD_MUTEX_INITIALIZER;
static lul::LUL*       sLUL      = nullptr;
static int             sLULcount = 0;


void uwt__init()
{
  // Create the unwinder thread.
  MOZ_ASSERT(unwind_thr_exit_now == 0);
  int r = pthread_create( &unwind_thr, nullptr,
                          unwind_thr_fn, (void*)&unwind_thr_exit_now );
  MOZ_ALWAYS_TRUE(r == 0);
}

void uwt__stop()
{
  // Shut down the unwinder thread.
  MOZ_ASSERT(unwind_thr_exit_now == 0);
  unwind_thr_exit_now = 1;
  do_MBAR();
  int r = pthread_join(unwind_thr, nullptr);
  MOZ_ALWAYS_TRUE(r == 0);
}

void uwt__deinit()
{
  empty_buffer_queue();
}

void uwt__register_thread_for_profiling(void* stackTop)
{
  thread_register_for_profiling(stackTop);
}

void uwt__unregister_thread_for_profiling()
{
  thread_unregister_for_profiling();
}

LinkedUWTBuffer* utb__acquire_sync_buffer(void* stackTop)
{
  return acquire_sync_buffer(stackTop);
}

void utb__finish_sync_buffer(ThreadProfile* profile,
                             UnwinderThreadBuffer* buff,
                             void* /* ucontext_t*, really */ ucV)
{
  finish_sync_buffer(profile, buff, ucV);
}

void utb__release_sync_buffer(LinkedUWTBuffer* buff)
{
  release_sync_buffer(buff);
}

void
utb__end_sync_buffer_unwind(LinkedUWTBuffer* utb)
{
  end_sync_buffer_unwind(utb);
}

// RUNS IN SIGHANDLER CONTEXT
UnwinderThreadBuffer* uwt__acquire_empty_buffer()
{
  return acquire_empty_buffer();
}

// RUNS IN SIGHANDLER CONTEXT
void
uwt__release_full_buffer(ThreadProfile* aProfile,
                         UnwinderThreadBuffer* utb,
                         void* /* ucontext_t*, really */ ucV )
{
  release_full_buffer( aProfile, utb, ucV );
}

// RUNS IN SIGHANDLER CONTEXT
void
utb__addEntry(/*MODIFIED*/UnwinderThreadBuffer* utb, ProfileEntry ent)
{
  utb_add_prof_ent(utb, ent);
}

//// END externally visible functions
//////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////
//// BEGIN type UnwindThreadBuffer

static_assert(sizeof(uint32_t) == 4, "uint32_t size incorrect");
static_assert(sizeof(uint64_t) == 8, "uint64_t size incorrect");
static_assert(sizeof(uintptr_t) == sizeof(void*),
              "uintptr_t size incorrect");

typedef
  struct { 
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rip; 
  }
  AMD64Regs;

typedef
  struct {
    uint32_t r15;
    uint32_t r14;
    uint32_t r13;
    uint32_t r12;
    uint32_t r11;
    uint32_t r7;
  }
  ARMRegs;

typedef
  struct {
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
  }
  X86Regs;

#if defined(SPS_ARCH_amd64)
typedef  AMD64Regs  ArchRegs;
#elif defined(SPS_ARCH_arm)
typedef  ARMRegs  ArchRegs;
#elif defined(SPS_ARCH_x86)
typedef  X86Regs  ArchRegs;
#else
# error "Unknown plat"
#endif

#if defined(SPS_ARCH_amd64) || defined(SPS_ARCH_arm) || defined(SPS_ARCH_x86)
# define SPS_PAGE_SIZE 4096
#else
# error "Unknown plat"
#endif

typedef  enum { S_EMPTY, S_FILLING, S_EMPTYING, S_FULL }  State;

typedef  struct { uintptr_t val; }  SpinLock;

/* CONFIGURABLE */
/* The number of fixed ProfileEntry slots.  If more are required, they
   are placed in mmap'd pages. */
#define N_FIXED_PROF_ENTS 20

/* CONFIGURABLE */
/* The number of extra pages of ProfileEntries.  If (on arm) each
   ProfileEntry is 8 bytes, then a page holds 512, and so 100 pages
   is enough to hold 51200. */
#define N_PROF_ENT_PAGES 100

/* DERIVATIVE */
#define N_PROF_ENTS_PER_PAGE (SPS_PAGE_SIZE / sizeof(ProfileEntry))

/* A page of ProfileEntrys.  This might actually be slightly smaller
   than a page if SPS_PAGE_SIZE is not an exact multiple of
   sizeof(ProfileEntry). */
typedef
  struct { ProfileEntry ents[N_PROF_ENTS_PER_PAGE]; }
  ProfEntsPage;

#define ProfEntsPage_INVALID ((ProfEntsPage*)1)


/* Fields protected by the spinlock are marked SL */

struct _UnwinderThreadBuffer {
  /*SL*/ State  state;
  /* The rest of these are protected, in some sense, by ::state.  If
     ::state is S_FILLING, they are 'owned' by the sampler thread
     that set the state to S_FILLING.  If ::state is S_EMPTYING,
     they are 'owned' by the unwinder thread that set the state to
     S_EMPTYING.  If ::state is S_EMPTY or S_FULL, the buffer isn't
     owned by any thread, and so no thread may access these
     fields. */
  /* Sample number, needed to process samples in order */
  uint64_t       seqNo;
  /* The ThreadProfile into which the results are eventually to be
     dumped. */
  ThreadProfile* aProfile;
  /* Pseudostack and other info, always present */
  ProfileEntry   entsFixed[N_FIXED_PROF_ENTS];
  ProfEntsPage*  entsPages[N_PROF_ENT_PAGES];
  uintptr_t      entsUsed;
  /* Do we also have data to do a native unwind? */
  bool           haveNativeInfo;
  /* If so, here is the register state and stack.  Unset if
     .haveNativeInfo is false. */
  lul::UnwindRegs startRegs;
  lul::StackImage stackImg;
  void* stackMaxSafe; /* Address for max safe stack reading. */
};
/* Indexing scheme for ents:
     0 <= i < N_FIXED_PROF_ENTS
       is at entsFixed[i]

     i >= N_FIXED_PROF_ENTS
       is at let j = i - N_FIXED_PROF_ENTS
             in  entsPages[j / N_PROFENTS_PER_PAGE]
                  ->ents[j % N_PROFENTS_PER_PAGE]
     
   entsPages[] are allocated on demand.  Because zero can
   theoretically be a valid page pointer, use 
   ProfEntsPage_INVALID == (ProfEntsPage*)1 to mark invalid pages.

   It follows that the max entsUsed value is N_FIXED_PROF_ENTS +
   N_PROFENTS_PER_PAGE * N_PROFENTS_PAGES, and at that point no more
   ProfileEntries can be storedd.
*/


typedef
  struct {
    pthread_t thrId;
    void*     stackTop;
    uint64_t  nSamples; 
  }
  StackLimit;

/* Globals -- the buffer array */
#define N_UNW_THR_BUFFERS 10
/*SL*/ static UnwinderThreadBuffer** g_buffers     = nullptr;
/*SL*/ static uint64_t               g_seqNo       = 0;
/*SL*/ static SpinLock               g_spinLock    = { 0 };

/* Globals -- the thread array.  The array is dynamically expanded on
   demand.  The spinlock must be held when accessing g_stackLimits,
   g_stackLimits[some index], g_stackLimitsUsed and g_stackLimitsSize.
   However, the spinlock must not be held when calling malloc to
   allocate or expand the array, as that would risk deadlock against a
   sampling thread that holds the malloc lock and is trying to acquire
   the spinlock. */
/*SL*/ static StackLimit* g_stackLimits     = nullptr;
/*SL*/ static size_t      g_stackLimitsUsed = 0;
/*SL*/ static size_t      g_stackLimitsSize = 0;

/* Stats -- atomically incremented, no lock needed */
static uintptr_t g_stats_totalSamples = 0; // total # sample attempts
static uintptr_t g_stats_noBuffAvail  = 0; // # failed due to no buffer avail
static uintptr_t g_stats_thrUnregd    = 0; // # failed due to unregistered thr

/* We must be VERY CAREFUL what we do with the spinlock held.  The
   only thing it is safe to do with it held is modify (viz, read or
   write) g_buffers, g_buffers[], g_seqNo, g_buffers[]->state,
   g_stackLimits, g_stackLimits[], g_stackLimitsUsed and
   g_stackLimitsSize.  No arbitrary computations, no syscalls, no
   printfs, no file IO, and absolutely no dynamic memory allocation
   (else we WILL eventually deadlock).

   This applies both to the signal handler and to the unwinder thread.
*/

//// END type UnwindThreadBuffer
//////////////////////////////////////////////////////////

// This is the interface to LUL.
typedef  struct { u_int64_t pc; u_int64_t sp; }  PCandSP;

// Forward declaration.  Implementation is below.
static
void do_lul_unwind_Buffer(/*OUT*/PCandSP** pairs,
                          /*OUT*/unsigned int* nPairs,
                          UnwinderThreadBuffer* buff,
                          int buffNo /* for debug printing only */);

static bool is_page_aligned(void* v)
{
  uintptr_t w = (uintptr_t) v;
  return (w & (SPS_PAGE_SIZE-1)) == 0  ? true  : false;
}


/* Implement machine-word sized atomic compare-and-swap.  Returns true
   if success, false if failure. */
static bool do_CASW(uintptr_t* addr, uintptr_t expected, uintptr_t nyu)
{
#if defined(__GNUC__)
  return __sync_bool_compare_and_swap(addr, expected, nyu);
#else
# error "Unhandled compiler"
#endif
}

/* Hint to the CPU core that we are in a spin-wait loop, and that
   other processors/cores/threads-running-on-the-same-core should be
   given priority on execute resources, if that is possible.  Not
   critical if this is a no-op on some targets. */
static void do_SPINLOOP_RELAX()
{
#if (defined(SPS_ARCH_amd64) || defined(SPS_ARCH_x86)) && defined(__GNUC__)
  __asm__ __volatile__("rep; nop");
#elif defined(SPS_PLAT_arm_android) && MOZILLA_ARM_ARCH >= 7
  __asm__ __volatile__("wfe");
#endif
}

/* Tell any cores snoozing in spin loops to wake up. */
static void do_SPINLOOP_NUDGE()
{
#if (defined(SPS_ARCH_amd64) || defined(SPS_ARCH_x86)) && defined(__GNUC__)
  /* this is a no-op */
#elif defined(SPS_PLAT_arm_android) && MOZILLA_ARM_ARCH >= 7
  __asm__ __volatile__("sev");
#endif
}

/* Perform a full memory barrier. */
static void do_MBAR()
{
#if defined(__GNUC__)
  __sync_synchronize();
#else
# error "Unhandled compiler"
#endif
}

static void spinLock_acquire(SpinLock* sl)
{
  uintptr_t* val = &sl->val;
  VALGRIND_HG_MUTEX_LOCK_PRE(sl, 0/*!isTryLock*/);
  while (1) {
    bool ok = do_CASW( val, 0, 1 );
    if (ok) break;
    do_SPINLOOP_RELAX();
  }
  do_MBAR();
  VALGRIND_HG_MUTEX_LOCK_POST(sl);
}

static void spinLock_release(SpinLock* sl)
{
  uintptr_t* val = &sl->val;
  VALGRIND_HG_MUTEX_UNLOCK_PRE(sl);
  do_MBAR();
  bool ok = do_CASW( val, 1, 0 );
  /* This must succeed at the first try.  To fail would imply that
     the lock was unheld. */
  MOZ_ALWAYS_TRUE(ok);
  do_SPINLOOP_NUDGE();
  VALGRIND_HG_MUTEX_UNLOCK_POST(sl);
}

static void sleep_ms(unsigned int ms)
{
  struct timespec req;
  req.tv_sec = ((time_t)ms) / 1000;
  req.tv_nsec = 1000 * 1000 * (((unsigned long)ms) % 1000);
  nanosleep(&req, nullptr);
}

/* Use CAS to implement standalone atomic increment. */
static void atomic_INC(uintptr_t* loc)
{
  while (1) {
    uintptr_t old = *loc;
    uintptr_t nyu = old + 1;
    bool ok = do_CASW( loc, old, nyu );
    if (ok) break;
  }
}

// Empties out the buffer queue.
static void empty_buffer_queue()
{
  spinLock_acquire(&g_spinLock);

  UnwinderThreadBuffer** tmp_g_buffers = g_buffers;
  g_stackLimitsUsed = 0;
  g_seqNo = 0;
  g_buffers = nullptr;

  spinLock_release(&g_spinLock);

  // Can't do any malloc/free when holding the spinlock.
  free(tmp_g_buffers);

  // We could potentially free up g_stackLimits; but given the
  // complications above involved in resizing it, it's probably
  // safer just to leave it in place.
}


// Registers a thread for profiling.  Detects and ignores duplicate
// registration.
static void thread_register_for_profiling(void* stackTop)
{
  pthread_t me = pthread_self();

  spinLock_acquire(&g_spinLock);

  // tmp copy of g_stackLimitsUsed, to avoid racing in message printing
  int n_used;

  // Ignore spurious calls which aren't really registering anything.
  if (stackTop == nullptr) {
    n_used = g_stackLimitsUsed;
    spinLock_release(&g_spinLock);
    LOGF("BPUnw: [%d total] thread_register_for_profiling"
         "(me=%p, stacktop=NULL) (IGNORED)", n_used, (void*)me);
    return;
  }

  /* Minimal sanity check on stackTop */
  MOZ_ASSERT((void*)&n_used/*any auto var will do*/ < stackTop);

  bool is_dup = false;
  for (size_t i = 0; i < g_stackLimitsUsed; i++) {
    if (g_stackLimits[i].thrId == me) {
      is_dup = true;
      break;
    }
  }

  if (is_dup) {
    /* It's a duplicate registration.  Ignore it: drop the lock and
       return. */
    n_used = g_stackLimitsUsed;
    spinLock_release(&g_spinLock);

    LOGF("BPUnw: [%d total] thread_register_for_profiling"
         "(me=%p, stacktop=%p) (DUPLICATE)", n_used, (void*)me, stackTop);
    return;
  }

  /* Make sure the g_stackLimits array is large enough to accommodate
     this new entry.  This is tricky.  If it isn't large enough, we
     can malloc a larger version, but we have to do that without
     holding the spinlock, else we risk deadlock.  The deadlock
     scenario is:

     Some other thread that is being sampled
                                        This thread

     call malloc                        call this function
     acquire malloc lock                acquire the spinlock
     (sampling signal)                  discover thread array not big enough,
     call uwt__acquire_empty_buffer       call malloc to make it larger
     acquire the spinlock               acquire malloc lock

     This gives an inconsistent lock acquisition order on the malloc
     lock and spinlock, hence risk of deadlock.

     Allocating more space for the array without holding the spinlock
     implies tolerating races against other thread(s) who are also
     trying to expand the array.  How can we detect if we have been
     out-raced?  Every successful expansion of g_stackLimits[] results
     in an increase in g_stackLimitsSize.  Hence we can detect if we
     got out-raced by remembering g_stackLimitsSize before we dropped
     the spinlock and checking if it has changed after the spinlock is
     reacquired. */

  MOZ_ASSERT(g_stackLimitsUsed <= g_stackLimitsSize);

  if (g_stackLimitsUsed == g_stackLimitsSize) {
    /* g_stackLimits[] is full; resize it. */

    size_t old_size = g_stackLimitsSize;
    size_t new_size = old_size == 0 ? 4 : (2 * old_size);

    spinLock_release(&g_spinLock);
    StackLimit* new_arr  = (StackLimit*)malloc(new_size * sizeof(StackLimit));
    if (!new_arr)
      return;

    spinLock_acquire(&g_spinLock);

    if (old_size != g_stackLimitsSize) {
      /* We've been outraced.  Instead of trying to deal in-line with
         this extremely rare case, just start all over again by
         tail-calling this routine. */
      spinLock_release(&g_spinLock);
      free(new_arr);
      thread_register_for_profiling(stackTop);
      return;
    }

    memcpy(new_arr, g_stackLimits, old_size * sizeof(StackLimit));
    if (g_stackLimits)
      free(g_stackLimits);

    g_stackLimits = new_arr;

    MOZ_ASSERT(g_stackLimitsSize < new_size);
    g_stackLimitsSize = new_size;
  }

  MOZ_ASSERT(g_stackLimitsUsed < g_stackLimitsSize);

  /* Finally, we have a safe place to put the new entry. */

  // Round |stackTop| up to the end of the containing page.  We may
  // as well do this -- there's no danger of a fault, and we might
  // get a few more base-of-the-stack frames as a result.  This
  // assumes that no target has a page size smaller than 4096.
  uintptr_t stackTopR = (uintptr_t)stackTop;
  stackTopR = (stackTopR & ~(uintptr_t)4095) + (uintptr_t)4095;

  g_stackLimits[g_stackLimitsUsed].thrId    = me;
  g_stackLimits[g_stackLimitsUsed].stackTop = (void*)stackTopR;
  g_stackLimits[g_stackLimitsUsed].nSamples = 0;
  g_stackLimitsUsed++;

  n_used = g_stackLimitsUsed;
  spinLock_release(&g_spinLock);

  LOGF("BPUnw: [%d total] thread_register_for_profiling"
       "(me=%p, stacktop=%p)", n_used, (void*)me, stackTop);
}

// Deregisters a thread from profiling.  Detects and ignores attempts
// to deregister a not-registered thread.
static void thread_unregister_for_profiling()
{
  spinLock_acquire(&g_spinLock);

  // tmp copy of g_stackLimitsUsed, to avoid racing in message printing
  size_t n_used;

  size_t i;
  bool found = false;
  pthread_t me = pthread_self();
  for (i = 0; i < g_stackLimitsUsed; i++) {
    if (g_stackLimits[i].thrId == me)
      break;
  }
  if (i < g_stackLimitsUsed) {
    // found this entry.  Slide the remaining ones down one place.
    for (; i+1 < g_stackLimitsUsed; i++) {
      g_stackLimits[i] = g_stackLimits[i+1];
    }
    g_stackLimitsUsed--;
    found = true;
  }

  n_used = g_stackLimitsUsed;

  spinLock_release(&g_spinLock);
  LOGF("BPUnw: [%d total] thread_unregister_for_profiling(me=%p) %s", 
       (int)n_used, (void*)me, found ? "" : " (NOT REGISTERED) ");
}


__attribute__((unused))
static void show_registered_threads()
{
  size_t i;
  spinLock_acquire(&g_spinLock);
  for (i = 0; i < g_stackLimitsUsed; i++) {
    LOGF("[%d]  pthread_t=%p  nSamples=%lld",
         (int)i, (void*)g_stackLimits[i].thrId, 
                 (unsigned long long int)g_stackLimits[i].nSamples);
  }
  spinLock_release(&g_spinLock);
}

// RUNS IN SIGHANDLER CONTEXT
/* The calling thread owns the buffer, as denoted by its state being
   S_FILLING.  So we can mess with it without further locking. */
static void init_empty_buffer(UnwinderThreadBuffer* buff, void* stackTop)
{
  /* Now we own the buffer, initialise it. */
  buff->aProfile            = nullptr;
  buff->entsUsed            = 0;
  buff->haveNativeInfo      = false;
  buff->stackImg.mLen       = 0;
  buff->stackImg.mStartAvma = 0;
  buff->stackMaxSafe        = stackTop; /* We will need this in
                                           release_full_buffer() */
  for (size_t i = 0; i < N_PROF_ENT_PAGES; i++)
    buff->entsPages[i] = ProfEntsPage_INVALID;
}

struct SyncUnwinderThreadBuffer : public LinkedUWTBuffer
{
  UnwinderThreadBuffer* GetBuffer()
  {
    return &mBuff;
  }
  
  UnwinderThreadBuffer  mBuff;
};

static LinkedUWTBuffer* acquire_sync_buffer(void* stackTop)
{
  MOZ_ASSERT(stackTop);
  SyncUnwinderThreadBuffer* buff = new SyncUnwinderThreadBuffer();
  // We can set state without locking here because this thread owns the buffer
  // and it is going to fill it itself.
  buff->GetBuffer()->state = S_FILLING;
  init_empty_buffer(buff->GetBuffer(), stackTop);
  return buff;
}

// RUNS IN SIGHANDLER CONTEXT
static UnwinderThreadBuffer* acquire_empty_buffer()
{
  /* acq lock
     if buffers == nullptr { rel lock; exit }
     scan to find a free buff; if none { rel lock; exit }
     set buff state to S_FILLING
     fillseqno++; and remember it
     rel lock
  */
  size_t i;

  atomic_INC( &g_stats_totalSamples );

  /* This code is critical.  We are in a signal handler and possibly
     with the malloc lock held.  So we can't allocate any heap, and
     can't safely call any C library functions, not even the pthread_
     functions.  And we certainly can't do any syscalls.  In short,
     this function needs to be self contained, not do any allocation,
     and not hold on to the spinlock for any significant length of
     time. */

  spinLock_acquire(&g_spinLock);

  /* First of all, look for this thread's entry in g_stackLimits[].
     We need to find it in order to figure out how much stack we can
     safely copy into the sample.  This assumes that pthread_self()
     is safe to call in a signal handler, which strikes me as highly
     likely. */
  pthread_t me = pthread_self();
  MOZ_ASSERT(g_stackLimitsUsed <= g_stackLimitsSize);
  for (i = 0; i < g_stackLimitsUsed; i++) {
    if (g_stackLimits[i].thrId == me)
      break;
  }

  /* If the thread isn't registered for profiling, just ignore the call
     and return nullptr. */
  if (i == g_stackLimitsUsed) {
    spinLock_release(&g_spinLock);
    atomic_INC( &g_stats_thrUnregd );
    return nullptr;
  }

  /* "this thread is registered for profiling" */
  MOZ_ASSERT(i < g_stackLimitsUsed);

  /* The furthest point that we can safely scan back up the stack. */
  void* myStackTop = g_stackLimits[i].stackTop;
  g_stackLimits[i].nSamples++;

  /* Try to find a free buffer to use. */
  if (g_buffers == nullptr) {
    /* The unwinder thread hasn't allocated any buffers yet.
       Nothing we can do. */
    spinLock_release(&g_spinLock);
    atomic_INC( &g_stats_noBuffAvail );
    return nullptr;
  }

  for (i = 0; i < N_UNW_THR_BUFFERS; i++) {
    if (g_buffers[i]->state == S_EMPTY)
      break;
  }
  MOZ_ASSERT(i <= N_UNW_THR_BUFFERS);

  if (i == N_UNW_THR_BUFFERS) {
    /* Again, no free buffers .. give up. */
    spinLock_release(&g_spinLock);
    atomic_INC( &g_stats_noBuffAvail );
    if (LOGLEVEL >= 3)
      LOG("BPUnw: handler:  no free buffers");
    return nullptr;
  }

  /* So we can use this one safely.  Whilst still holding the lock,
     mark the buffer as belonging to us, and increment the sequence
     number. */
  UnwinderThreadBuffer* buff = g_buffers[i];
  MOZ_ASSERT(buff->state == S_EMPTY);
  buff->state = S_FILLING;
  buff->seqNo = g_seqNo;
  g_seqNo++;

  /* And drop the lock.  We own the buffer, so go on and fill it. */
  spinLock_release(&g_spinLock);

  /* Now we own the buffer, initialise it. */
  init_empty_buffer(buff, myStackTop);
  return buff;
}

// RUNS IN SIGHANDLER CONTEXT
/* The calling thread owns the buffer, as denoted by its state being
   S_FILLING.  So we can mess with it without further locking. */
static void fill_buffer(ThreadProfile* aProfile,
                        UnwinderThreadBuffer* buff,
                        void* /* ucontext_t*, really */ ucV)
{
  MOZ_ASSERT(buff->state == S_FILLING);

  ////////////////////////////////////////////////////
  // BEGIN fill

  /* The buffer already will have some of its ProfileEntries filled
     in, but everything else needs to be filled in at this point. */
  //LOGF("Release full buffer: %lu ents", buff->entsUsed);
  /* Where the resulting info is to be dumped */
  buff->aProfile = aProfile;

  /* And, if we have register state, that and the stack top */
  buff->haveNativeInfo = ucV != nullptr;
  if (buff->haveNativeInfo) {
#   if defined(SPS_PLAT_amd64_linux)
    ucontext_t* uc = (ucontext_t*)ucV;
    mcontext_t* mc = &(uc->uc_mcontext);
    buff->startRegs.xip = lul::TaggedUWord(mc->gregs[REG_RIP]);
    buff->startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_RSP]);
    buff->startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_RBP]);
#   elif defined(SPS_PLAT_amd64_darwin)
    ucontext_t* uc = (ucontext_t*)ucV;
    struct __darwin_mcontext64* mc = uc->uc_mcontext;
    struct __darwin_x86_thread_state64* ss = &mc->__ss;
    buff->regs.rip = ss->__rip;
    buff->regs.rsp = ss->__rsp;
    buff->regs.rbp = ss->__rbp;
#   elif defined(SPS_PLAT_arm_android)
    ucontext_t* uc = (ucontext_t*)ucV;
    mcontext_t* mc = &(uc->uc_mcontext);
    buff->startRegs.r15 = lul::TaggedUWord(mc->arm_pc);
    buff->startRegs.r14 = lul::TaggedUWord(mc->arm_lr);
    buff->startRegs.r13 = lul::TaggedUWord(mc->arm_sp);
    buff->startRegs.r12 = lul::TaggedUWord(mc->arm_ip);
    buff->startRegs.r11 = lul::TaggedUWord(mc->arm_fp);
    buff->startRegs.r7  = lul::TaggedUWord(mc->arm_r7);
#   elif defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_android)
    ucontext_t* uc = (ucontext_t*)ucV;
    mcontext_t* mc = &(uc->uc_mcontext);
    buff->startRegs.xip = lul::TaggedUWord(mc->gregs[REG_EIP]);
    buff->startRegs.xsp = lul::TaggedUWord(mc->gregs[REG_ESP]);
    buff->startRegs.xbp = lul::TaggedUWord(mc->gregs[REG_EBP]);
#   elif defined(SPS_PLAT_x86_darwin)
    ucontext_t* uc = (ucontext_t*)ucV;
    struct __darwin_mcontext32* mc = uc->uc_mcontext;
    struct __darwin_i386_thread_state* ss = &mc->__ss;
    buff->regs.eip = ss->__eip;
    buff->regs.esp = ss->__esp;
    buff->regs.ebp = ss->__ebp;
#   else
#     error "Unknown plat"
#   endif

    /* Copy up to N_STACK_BYTES from rsp-REDZONE upwards, but not
       going past the stack's registered top point.  Do some basic
       sanity checks too.  This assumes that the TaggedUWord holding
       the stack pointer value is valid, but it should be, since it
       was constructed that way in the code just above. */
    { 
#     if defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_amd64_darwin)
      uintptr_t rEDZONE_SIZE = 128;
      uintptr_t start = buff->startRegs.xsp.Value() - rEDZONE_SIZE;
#     elif defined(SPS_PLAT_arm_android)
      uintptr_t rEDZONE_SIZE = 0;
      uintptr_t start = buff->startRegs.r13.Value() - rEDZONE_SIZE;
#     elif defined(SPS_PLAT_x86_linux) || defined(SPS_PLAT_x86_darwin) \
           || defined(SPS_PLAT_x86_android)
      uintptr_t rEDZONE_SIZE = 0;
      uintptr_t start = buff->startRegs.xsp.Value() - rEDZONE_SIZE;
#     else
#       error "Unknown plat"
#     endif
      uintptr_t end   = (uintptr_t)buff->stackMaxSafe;
      uintptr_t ws    = sizeof(void*);
      start &= ~(ws-1);
      end   &= ~(ws-1);
      uintptr_t nToCopy = 0;
      if (start < end) {
        nToCopy = end - start;
        if (nToCopy > lul::N_STACK_BYTES)
          nToCopy = lul::N_STACK_BYTES;
      }
      MOZ_ASSERT(nToCopy <= lul::N_STACK_BYTES);
      buff->stackImg.mLen       = nToCopy;
      buff->stackImg.mStartAvma = start;
      if (nToCopy > 0) {
        memcpy(&buff->stackImg.mContents[0], (void*)start, nToCopy);
        (void)VALGRIND_MAKE_MEM_DEFINED(&buff->stackImg.mContents[0], nToCopy);
      }
    }
  } /* if (buff->haveNativeInfo) */
  // END fill
  ////////////////////////////////////////////////////
}

// RUNS IN SIGHANDLER CONTEXT
/* The calling thread owns the buffer, as denoted by its state being
   S_FILLING.  So we can mess with it without further locking. */
static void release_full_buffer(ThreadProfile* aProfile,
                                UnwinderThreadBuffer* buff,
                                void* /* ucontext_t*, really */ ucV )
{
  fill_buffer(aProfile, buff, ucV);
  /* And now relinquish ownership of the buff, so that an unwinder
     thread can pick it up. */
  spinLock_acquire(&g_spinLock);
  buff->state = S_FULL;
  spinLock_release(&g_spinLock);
}

// RUNS IN SIGHANDLER CONTEXT
// Allocate a ProfEntsPage, without using malloc, or return
// ProfEntsPage_INVALID if we can't for some reason.
static ProfEntsPage* mmap_anon_ProfEntsPage()
{
# if defined(SPS_OS_darwin)
  void* v = ::mmap(nullptr, sizeof(ProfEntsPage), PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE | MAP_ANON,      -1, 0);
# else
  void* v = ::mmap(nullptr, sizeof(ProfEntsPage), PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
# endif
  if (v == MAP_FAILED) {
    return ProfEntsPage_INVALID;
  } else {
    return (ProfEntsPage*)v;
  }
}

// Runs in the unwinder thread
// Free a ProfEntsPage as allocated by mmap_anon_ProfEntsPage
static void munmap_ProfEntsPage(ProfEntsPage* pep)
{
  MOZ_ALWAYS_TRUE(is_page_aligned(pep));
  ::munmap(pep, sizeof(ProfEntsPage));
}


// RUNS IN SIGHANDLER CONTEXT
void
utb_add_prof_ent(/*MODIFIED*/UnwinderThreadBuffer* utb, ProfileEntry ent)
{
  uintptr_t limit
    = N_FIXED_PROF_ENTS + (N_PROF_ENTS_PER_PAGE * N_PROF_ENT_PAGES);
  if (utb->entsUsed == limit) {
    /* We're full.  Now what? */
    LOG("BPUnw: utb__addEntry: NO SPACE for ProfileEntry; ignoring.");
    return;
  }
  MOZ_ASSERT(utb->entsUsed < limit);

  /* Will it fit in the fixed array? */
  if (utb->entsUsed < N_FIXED_PROF_ENTS) {
    utb->entsFixed[utb->entsUsed] = ent;
    utb->entsUsed++;
    return;
  }

  /* No.  Put it in the extras. */
  uintptr_t i     = utb->entsUsed;
  uintptr_t j     = i - N_FIXED_PROF_ENTS;
  uintptr_t j_div = j / N_PROF_ENTS_PER_PAGE; /* page number */
  uintptr_t j_mod = j % N_PROF_ENTS_PER_PAGE; /* page offset */
  ProfEntsPage* pep = utb->entsPages[j_div];
  if (pep == ProfEntsPage_INVALID) {
    pep = mmap_anon_ProfEntsPage();
    if (pep == ProfEntsPage_INVALID) {
      /* Urr, we ran out of memory.  Now what? */
      LOG("BPUnw: utb__addEntry: MMAP FAILED for ProfileEntry; ignoring.");
      return;
    }
    utb->entsPages[j_div] = pep;
  }
  pep->ents[j_mod] = ent;
  utb->entsUsed++;
}


// misc helper
static ProfileEntry utb_get_profent(UnwinderThreadBuffer* buff, uintptr_t i)
{
  MOZ_ASSERT(i < buff->entsUsed);
  if (i < N_FIXED_PROF_ENTS) {
    return buff->entsFixed[i];
  } else {
    uintptr_t j     = i - N_FIXED_PROF_ENTS;
    uintptr_t j_div = j / N_PROF_ENTS_PER_PAGE; /* page number */
    uintptr_t j_mod = j % N_PROF_ENTS_PER_PAGE; /* page offset */
    MOZ_ASSERT(buff->entsPages[j_div] != ProfEntsPage_INVALID);
    return buff->entsPages[j_div]->ents[j_mod];
  }
}

/* Forward declaration for process_sync_buffer */
static void process_buffer(UnwinderThreadBuffer* buff, int oldest_ix);

static void process_sync_buffer(ProfileEntry& ent)
{
  UnwinderThreadBuffer* buff = (UnwinderThreadBuffer*)ent.get_tagPtr();
  buff->state = S_EMPTYING;
  process_buffer(buff, -1);
}

/* Copy ProfileEntries presented to us by the sampling thread.
   Most of them are copied verbatim into |buff->aProfile|,
   except for 'hint' tags, which direct us to do something
   different. */
static void process_buffer(UnwinderThreadBuffer* buff, int oldest_ix)
{
  /* Need to lock |aProfile| so nobody tries to copy out entries
     whilst we are putting them in. */
  buff->aProfile->BeginUnwind();

  /* The buff is a sequence of ProfileEntries (ents).  It has
     this grammar:

     | --pre-tags-- | (h 'P' .. h 'Q')* | --post-tags-- |
                      ^               ^
                      ix_first_hP     ix_last_hQ

     Each (h 'P' .. h 'Q') subsequence represents one pseudostack
     entry.  These, if present, are in the order
     outermost-frame-first, and that is the order that they should
     be copied into aProfile.  The --pre-tags-- and --post-tags--
     are to be copied into the aProfile verbatim, except that they
     may contain the hints "h 'F'" for a flush and "h 'N'" to
     indicate that a native unwind is also required, and must be
     interleaved with the pseudostack entries.

     The hint tags that bound each pseudostack entry, "h 'P'" and "h
     'Q'", are not to be copied into the aProfile -- they are
     present only to make parsing easy here.  Also, the pseudostack
     entries may contain an "'S' (void*)" entry, which is the stack
     pointer value for that entry, and these are also not to be
     copied.
  */
  /* The first thing to do is therefore to find the pseudostack
     entries, if any, and to find out also whether a native unwind
     has been requested. */
  const uintptr_t infUW = ~(uintptr_t)0; // infinity
  bool  need_native_unw = false;
  uintptr_t ix_first_hP = infUW; // "not found"
  uintptr_t ix_last_hQ  = infUW; // "not found"

  uintptr_t k;
  for (k = 0; k < buff->entsUsed; k++) {
    ProfileEntry ent = utb_get_profent(buff, k);
    if (ent.is_ent_hint('N')) {
      need_native_unw = true;
    }
    else if (ent.is_ent_hint('P') && ix_first_hP == ~(uintptr_t)0) {
      ix_first_hP = k;
    }
    else if (ent.is_ent_hint('Q')) {
      ix_last_hQ = k;
    }
  }

  if (0) LOGF("BPUnw: ix_first_hP %llu  ix_last_hQ %llu  need_native_unw %llu",
              (unsigned long long int)ix_first_hP,
              (unsigned long long int)ix_last_hQ,
              (unsigned long long int)need_native_unw);

  /* There are four possibilities: native-only, pseudostack-only,
     combined (both), and neither.  We handle all four cases. */

  MOZ_ASSERT( (ix_first_hP == infUW && ix_last_hQ == infUW) ||
              (ix_first_hP != infUW && ix_last_hQ != infUW) );
  bool have_P = ix_first_hP != infUW;
  if (have_P) {
    MOZ_ASSERT(ix_first_hP < ix_last_hQ);
    MOZ_ASSERT(ix_last_hQ <= buff->entsUsed);
  }

  /* Neither N nor P.  This is very unusual but has been observed to happen.
     Just copy to the output. */
  if (!need_native_unw && !have_P) {
    for (k = 0; k < buff->entsUsed; k++) {
      ProfileEntry ent = utb_get_profent(buff, k);
      // action flush-hints
      if (ent.is_ent_hint('F')) { buff->aProfile->flush(); continue; }
      // skip ones we can't copy
      if (ent.is_ent_hint() || ent.is_ent('S')) { continue; }
      // handle GetBacktrace()
      if (ent.is_ent('B')) {
        process_sync_buffer(ent);
        continue;
      }
      // and copy everything else
      buff->aProfile->addTag( ent );
    }
  }
  else /* Native only-case. */
  if (need_native_unw && !have_P) {
    for (k = 0; k < buff->entsUsed; k++) {
      ProfileEntry ent = utb_get_profent(buff, k);
      // action a native-unwind-now hint
      if (ent.is_ent_hint('N')) {
        MOZ_ASSERT(buff->haveNativeInfo);
        PCandSP* pairs = nullptr;
        unsigned int nPairs = 0;
        do_lul_unwind_Buffer(&pairs, &nPairs, buff, oldest_ix);
        buff->aProfile->addTag( ProfileEntry('s', "(root)") );
        for (unsigned int i = 0; i < nPairs; i++) {
          /* Skip any outermost frames that
             do_lul_unwind_Buffer didn't give us.  See comments
             on that function for details. */
          if (pairs[i].pc == 0 && pairs[i].sp == 0)
            continue;
          buff->aProfile
              ->addTag( ProfileEntry('l', reinterpret_cast<void*>(pairs[i].pc)) );
        }
        if (pairs)
          free(pairs);
        continue;
      }
      // action flush-hints
      if (ent.is_ent_hint('F')) { buff->aProfile->flush(); continue; }
      // skip ones we can't copy
      if (ent.is_ent_hint() || ent.is_ent('S')) { continue; }
      // handle GetBacktrace()
      if (ent.is_ent('B')) {
        process_sync_buffer(ent);
        continue;
      }
      // and copy everything else
      buff->aProfile->addTag( ent );
    }
  }
  else /* Pseudostack-only case */
  if (!need_native_unw && have_P) {
    /* If there's no request for a native stack, it's easy: just
       copy the tags verbatim into aProfile, skipping the ones that
       can't be copied -- 'h' (hint) tags, and "'S' (void*)"
       stack-pointer tags.  Except, insert a sample-start tag when
       we see the start of the first pseudostack frame. */
    for (k = 0; k < buff->entsUsed; k++) {
      ProfileEntry ent = utb_get_profent(buff, k);
      // We need to insert a sample-start tag before the first frame
      if (k == ix_first_hP) {
        buff->aProfile->addTag( ProfileEntry('s', "(root)") );
      }
      // action flush-hints
      if (ent.is_ent_hint('F')) { buff->aProfile->flush(); continue; }
      // skip ones we can't copy
      if (ent.is_ent_hint() || ent.is_ent('S')) { continue; }
      // handle GetBacktrace()
      if (ent.is_ent('B')) {
        process_sync_buffer(ent);
        continue;
      }
      // and copy everything else
      buff->aProfile->addTag( ent );
    }
  }
  else /* Combined case */
  if (need_native_unw && have_P)
  {
    /* We need to get a native stacktrace and merge it with the
       pseudostack entries.  This isn't too simple.  First, copy all
       the tags up to the start of the pseudostack tags.  Then
       generate a combined set of tags by native unwind and
       pseudostack.  Then, copy all the stuff after the pseudostack
       tags. */
    MOZ_ASSERT(buff->haveNativeInfo);

    // Get native unwind info
    PCandSP* pairs = nullptr;
    unsigned int n_pairs = 0;
    do_lul_unwind_Buffer(&pairs, &n_pairs, buff, oldest_ix);

    // Entries before the pseudostack frames
    for (k = 0; k < ix_first_hP; k++) {
      ProfileEntry ent = utb_get_profent(buff, k);
      // action flush-hints
      if (ent.is_ent_hint('F')) { buff->aProfile->flush(); continue; }
      // skip ones we can't copy
      if (ent.is_ent_hint() || ent.is_ent('S')) { continue; }
      // handle GetBacktrace()
      if (ent.is_ent('B')) {
        process_sync_buffer(ent);
        continue;
      }
      // and copy everything else
      buff->aProfile->addTag( ent );
    }

    // BEGIN merge
    buff->aProfile->addTag( ProfileEntry('s', "(root)") );
    unsigned int next_N = 0; // index in pairs[]
    unsigned int next_P = ix_first_hP; // index in buff profent array
    bool last_was_P = false;
    if (0) LOGF("at mergeloop: n_pairs %llu ix_last_hQ %llu",
                (unsigned long long int)n_pairs,
                (unsigned long long int)ix_last_hQ);
    /* Skip any outermost frames that do_lul_unwind_Buffer
       didn't give us.  See comments on that function for
       details. */
    while (next_N < n_pairs && pairs[next_N].pc == 0 && pairs[next_N].sp == 0)
      next_N++;

    while (true) {
      if (next_P <= ix_last_hQ) {
        // Assert that next_P points at the start of an P entry
        MOZ_ASSERT(utb_get_profent(buff, next_P).is_ent_hint('P'));
      }
      if (next_N >= n_pairs && next_P > ix_last_hQ) {
        // both stacks empty
        break;
      }
      /* Decide which entry to use next:
         If N is empty, must use P, and vice versa
         else
         If the last was P and current P has zero SP, use P
         else
         we assume that both P and N have valid SP, in which case
            use the one with the larger value
      */
      bool use_P = true;
      if (next_N >= n_pairs) {
        // N empty, use P
        use_P = true;
        if (0) LOG("  P  <=  no remaining N entries");
      }
      else if (next_P > ix_last_hQ) {
        // P empty, use N
        use_P = false;
        if (0) LOG("  N  <=  no remaining P entries");
      }
      else {
        // We have at least one N and one P entry available.
        // Scan forwards to find the SP of the current P entry
        u_int64_t sp_cur_P = 0;
        unsigned int m = next_P + 1;
        while (1) {
          /* This assertion should hold because in a well formed
             input, we must eventually find the hint-Q that marks
             the end of this frame's entries. */
          MOZ_ASSERT(m < buff->entsUsed);
          ProfileEntry ent = utb_get_profent(buff, m);
          if (ent.is_ent_hint('Q'))
            break;
          if (ent.is_ent('S')) {
            sp_cur_P = reinterpret_cast<u_int64_t>(ent.get_tagPtr());
            break;
          }
          m++;
        }
        if (last_was_P && sp_cur_P == 0) {
          if (0) LOG("  P  <=  last_was_P && sp_cur_P == 0");
          use_P = true;
        } else {
          u_int64_t sp_cur_N = pairs[next_N].sp;
          use_P = (sp_cur_P > sp_cur_N);
          if (0) LOGF("  %s  <=  sps P %p N %p",
                      use_P ? "P" : "N", (void*)(intptr_t)sp_cur_P, 
                                         (void*)(intptr_t)sp_cur_N);
        }
      }
      /* So, we know which we are going to use. */
      if (use_P) {
        unsigned int m = next_P + 1;
        while (true) {
          MOZ_ASSERT(m < buff->entsUsed);
          ProfileEntry ent = utb_get_profent(buff, m);
          if (ent.is_ent_hint('Q')) {
            next_P = m + 1;
            break;
          }
          // we don't expect a flush-hint here
          MOZ_ASSERT(!ent.is_ent_hint('F'));
          // skip ones we can't copy
          if (ent.is_ent_hint() || ent.is_ent('S')) { m++; continue; }
          // and copy everything else
          buff->aProfile->addTag( ent );
          m++;
        }
      } else {
        buff->aProfile
            ->addTag( ProfileEntry('l', reinterpret_cast<void*>(pairs[next_N].pc)) );
        next_N++;
      }
      /* Remember what we chose, for next time. */
      last_was_P = use_P;
    }

    MOZ_ASSERT(next_P == ix_last_hQ + 1);
    MOZ_ASSERT(next_N == n_pairs);
    // END merge

    // Entries after the pseudostack frames
    for (k = ix_last_hQ+1; k < buff->entsUsed; k++) {
      ProfileEntry ent = utb_get_profent(buff, k);
      // action flush-hints
      if (ent.is_ent_hint('F')) { buff->aProfile->flush(); continue; }
      // skip ones we can't copy
      if (ent.is_ent_hint() || ent.is_ent('S')) { continue; }
      // and copy everything else
      buff->aProfile->addTag( ent );
    }

    // free native unwind info
    if (pairs)
      free(pairs);
  }

#if 0
  bool show = true;
  if (show) LOG("----------------");
  for (k = 0; k < buff->entsUsed; k++) {
    ProfileEntry ent = utb_get_profent(buff, k);
    if (show) ent.log();
    if (ent.is_ent_hint('F')) {
      /* This is a flush-hint */
      buff->aProfile->flush();
    } 
    else if (ent.is_ent_hint('N')) {
      /* This is a do-a-native-unwind-right-now hint */
      MOZ_ASSERT(buff->haveNativeInfo);
      PCandSP* pairs = nullptr;
      unsigned int nPairs = 0;
      do_lul_unwind_Buffer(&pairs, &nPairs, buff, oldest_ix);
      buff->aProfile->addTag( ProfileEntry('s', "(root)") );
      for (unsigned int i = 0; i < nPairs; i++) {
        buff->aProfile
            ->addTag( ProfileEntry('l', reinterpret_cast<void*>(pairs[i].pc)) );
      }
      if (pairs)
        free(pairs);
    } else {
      /* Copy in verbatim */
      buff->aProfile->addTag( ent );
    }
  }
#endif

  buff->aProfile->EndUnwind();
}


// Find out, in a platform-dependent way, where the code modules got
// mapped in the process' virtual address space, and get |aLUL| to
// load unwind info for them.
void
read_procmaps(lul::LUL* aLUL)
{
  MOZ_ASSERT(aLUL->CountMappings() == 0);

# if defined(SPS_OS_linux) || defined(SPS_OS_android) || defined(SPS_OS_darwin)
  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();

  for (size_t i = 0; i < info.GetSize(); i++) {
    const SharedLibrary& lib = info.GetEntry(i);

#if defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
    // We're using faulty.lib.  Use a special-case object mapper.
    AutoObjectMapperFaultyLib mapper(aLUL->mLog);
#else
    // We can use the standard POSIX-based mapper.
    AutoObjectMapperPOSIX mapper(aLUL->mLog);
#endif

    // Ask |mapper| to map the object.  Then hand its mapped address
    // to NotifyAfterMap().
    void*  image = nullptr;
    size_t size  = 0;
    bool ok = mapper.Map(&image, &size, lib.GetName());
    if (ok && image && size > 0) {
      aLUL->NotifyAfterMap(lib.GetStart(), lib.GetEnd()-lib.GetStart(),
                           lib.GetName().c_str(), image);
    } else if (!ok && lib.GetName() == "") {
      // The object has no name and (as a consequence) the mapper
      // failed to map it.  This happens on Linux, where
      // GetInfoForSelf() produces two such mappings: one for the
      // executable and one for the VDSO.  The executable one isn't a
      // big deal since there's not much interesting code in there,
      // but the VDSO one is a problem on x86-{linux,android} because
      // lack of knowledge about the mapped area inhibits LUL's
      // special __kernel_syscall handling.  Hence notify |aLUL| at
      // least of the mapping, even though it can't read any unwind
      // information for the area.
      aLUL->NotifyExecutableArea(lib.GetStart(), lib.GetEnd()-lib.GetStart());
    }

    // |mapper| goes out of scope at this point and so its destructor
    // unmaps the object.
  }

# else
#  error "Unknown platform"
# endif
}

// LUL needs a callback for its logging sink.
static void
logging_sink_for_LUL(const char* str) {
  // Ignore any trailing \n, since LOG will add one anyway.
  size_t n = strlen(str);
  if (n > 0 && str[n-1] == '\n') {
    char* tmp = strdup(str);
    tmp[n-1] = 0;
    LOG(tmp);
    free(tmp);
  } else {
    LOG(str);
  }
}

// Runs in the unwinder thread -- well, this _is_ the unwinder thread.
static void* unwind_thr_fn(void* exit_nowV)
{
  // This is the unwinder thread function.  The first thread in must
  // create the unwinder library and request it to read the debug
  // info.  The last thread out must deallocate the library.  These
  // three tasks (create library, read debuginfo, destroy library) are
  // sequentialised by |sLULmutex|.  |sLUL| and |sLULcount| may only
  // be modified whilst |sLULmutex| is held.
  //
  // Once the threads are up and running, |sLUL| (the pointer itself,
  // that is) stays constant, and the multiple threads may make
  // concurrent calls into |sLUL| to do concurrent unwinding.
  LOG("unwind_thr_fn: START");

  // A hook for testing LUL: at the first entrance here, check env var
  // MOZ_PROFILER_LUL_TEST, and if set, run tests on LUL.  Note that
  // it is preferable to run the LUL tests via gtest, but gtest is not
  // currently supported on all targets that LUL runs on.  Hence the
  // auxiliary mechanism here is also needed.
  bool doLulTest = false;

  mozilla::DebugOnly<int> r = pthread_mutex_lock(&sLULmutex);
  MOZ_ASSERT(!r);

  if (!sLUL) {
    // sLUL hasn't been allocated, so we must be the first thread in.
    sLUL = new lul::LUL(logging_sink_for_LUL);
    MOZ_ASSERT(sLUL);
    MOZ_ASSERT(sLULcount == 0);
    // Register this thread so it can read unwind info and do unwinding.
    sLUL->RegisterUnwinderThread();
    // Read all the unwind info currently available.
    read_procmaps(sLUL);
    // Has a test been requested?
    if (PR_GetEnv("MOZ_PROFILER_LUL_TEST")) {
      doLulTest = true;
    }
  } else {
    // sLUL has already been allocated, so we can't be the first
    // thread in.
    MOZ_ASSERT(sLULcount > 0);
    // Register this thread so it can do unwinding.
    sLUL->RegisterUnwinderThread();
  }

  sLULcount++;

  r = pthread_mutex_unlock(&sLULmutex);
  MOZ_ASSERT(!r);

  // If a test has been requested for LUL, run it.  Summary results
  // are sent to sLUL's logging sink.  Note that this happens after
  // read_procmaps has read unwind information into sLUL, so that the
  // tests have something to unwind against.  Without that they'd be
  // pretty meaningless.
  if (doLulTest) {
    int nTests = 0, nTestsPassed = 0;
    RunLulUnitTests(&nTests, &nTestsPassed, sLUL);
  }

  // At this point, sLUL -- the single instance of the library -- is
  // allocated and has read the required unwind info.  All running
  // threads can now make Unwind() requests of it concurrently, if
  // they wish.

  // Now go on to allocate the array of buffers used for communication
  // between the sampling threads and the unwinder threads.

  // If we're the first thread in, we'll need to allocate the buffer
  // array g_buffers plus the Buffer structs that it points at. */
  spinLock_acquire(&g_spinLock);
  if (g_buffers == nullptr) {
    // Drop the lock, make a complete copy in memory, reacquire the
    // lock, and try to install it -- which might fail, if someone
    // else beat us to it. */
    spinLock_release(&g_spinLock);
    UnwinderThreadBuffer** buffers
      = (UnwinderThreadBuffer**)malloc(N_UNW_THR_BUFFERS
                                        * sizeof(UnwinderThreadBuffer*));
    MOZ_ASSERT(buffers);
    int i;
    for (i = 0; i < N_UNW_THR_BUFFERS; i++) {
      /* These calloc-ations are shared between the sampling and
         unwinding threads.  They must be free after all such threads
         have terminated. */
      buffers[i] = (UnwinderThreadBuffer*)
                   calloc(sizeof(UnwinderThreadBuffer), 1);
      MOZ_ASSERT(buffers[i]);
      buffers[i]->state = S_EMPTY;
    }
    /* Try to install it */
    spinLock_acquire(&g_spinLock);
    if (g_buffers == nullptr) {
      g_buffers = buffers;
      spinLock_release(&g_spinLock);
    } else {
      /* Someone else beat us to it.  Release what we just allocated
         so as to avoid a leak. */
      spinLock_release(&g_spinLock);
      for (i = 0; i < N_UNW_THR_BUFFERS; i++) {
        free(buffers[i]);
      }
      free(buffers);
    }
  } else {
    /* They are already allocated, so just drop the lock and continue. */
    spinLock_release(&g_spinLock);
  }

  /* 
    while (1) {
      acq lock
      scan to find oldest full
         if none { rel lock; sleep; continue }
      set buff state to emptying
      rel lock
      acq MLock // implicitly
      process buffer
      rel MLock // implicitly
      acq lock
      set buff state to S_EMPTY
      rel lock
    }
  */
  int* exit_now = (int*)exit_nowV;
  int ms_to_sleep_if_empty = 1;

  const int longest_sleep_ms = 1000;
  bool show_sleep_message = true;

  while (1) {

    if (*exit_now != 0) {
      *exit_now = 0;
      break;
    }

    spinLock_acquire(&g_spinLock);

    /* Find the oldest filled buffer, if any. */
    uint64_t oldest_seqNo = ~0ULL; /* infinity */
    int      oldest_ix    = -1;
    int      i;
    for (i = 0; i < N_UNW_THR_BUFFERS; i++) {
      UnwinderThreadBuffer* buff = g_buffers[i];
      if (buff->state != S_FULL) continue;
      if (buff->seqNo < oldest_seqNo) {
        oldest_seqNo = buff->seqNo;
        oldest_ix    = i;
      }
    }
    if (oldest_ix == -1) {
      /* We didn't find a full buffer.  Snooze and try again later. */
      MOZ_ASSERT(oldest_seqNo == ~0ULL);
      spinLock_release(&g_spinLock);
      if (ms_to_sleep_if_empty > 100 && LOGLEVEL >= 2) {
        if (show_sleep_message)
          LOGF("BPUnw: unwinder: sleep for %d ms", ms_to_sleep_if_empty);
        /* If we've already shown the message for the longest sleep,
           don't show it again, until the next round of sleeping
           starts. */
        if (ms_to_sleep_if_empty == longest_sleep_ms)
          show_sleep_message = false;
      }
      sleep_ms(ms_to_sleep_if_empty);
      if (ms_to_sleep_if_empty < 20) {
        ms_to_sleep_if_empty += 2;
      } else {
        ms_to_sleep_if_empty = (15 * ms_to_sleep_if_empty) / 10;
        if (ms_to_sleep_if_empty > longest_sleep_ms)
          ms_to_sleep_if_empty = longest_sleep_ms;
      }
      continue;
    }

    /* We found a full a buffer.  Mark it as 'ours' and drop the
       lock; then we can safely throw breakpad at it. */
    UnwinderThreadBuffer* buff = g_buffers[oldest_ix];
    MOZ_ASSERT(buff->state == S_FULL);
    buff->state = S_EMPTYING;
    spinLock_release(&g_spinLock);

    /* unwind .. in which we can do anything we like, since any
       resource stalls that we may encounter (eg malloc locks) in
       competition with signal handler instances, will be short
       lived since the signal handler is guaranteed nonblocking. */
    if (0) LOGF("BPUnw: unwinder: seqNo %llu: emptying buf %d\n",
                (unsigned long long int)oldest_seqNo, oldest_ix);

    process_buffer(buff, oldest_ix);

    /* And .. we're done.  Mark the buffer as empty so it can be
       reused.  First though, unmap any of the entsPages that got
       mapped during filling. */
    for (i = 0; i < N_PROF_ENT_PAGES; i++) {
      if (buff->entsPages[i] == ProfEntsPage_INVALID)
        continue;
      munmap_ProfEntsPage(buff->entsPages[i]);
      buff->entsPages[i] = ProfEntsPage_INVALID;
    }

    (void)VALGRIND_MAKE_MEM_UNDEFINED(&buff->stackImg.mContents[0],
                                      lul::N_STACK_BYTES);
    spinLock_acquire(&g_spinLock);
    MOZ_ASSERT(buff->state == S_EMPTYING);
    buff->state = S_EMPTY;
    spinLock_release(&g_spinLock);
    ms_to_sleep_if_empty = 1;
    show_sleep_message = true;
  }

  // This unwinder thread is exiting.  If it's the last one out,
  // shut down and deallocate the unwinder library.
  r = pthread_mutex_lock(&sLULmutex);
  MOZ_ASSERT(!r);

  MOZ_ASSERT(sLULcount > 0);
  if (sLULcount == 1) {
    // Tell the library to discard unwind info for the entire address
    // space.
    sLUL->NotifyBeforeUnmapAll();

    delete sLUL;
    sLUL = nullptr;
  }

  sLULcount--;

  r = pthread_mutex_unlock(&sLULmutex);
  MOZ_ASSERT(!r);

  LOG("unwind_thr_fn: STOP");
  return nullptr;
}

static void finish_sync_buffer(ThreadProfile* profile,
                               UnwinderThreadBuffer* buff,
                               void* /* ucontext_t*, really */ ucV)
{
  SyncProfile* syncProfile = profile->AsSyncProfile();
  MOZ_ASSERT(syncProfile);
  SyncUnwinderThreadBuffer* utb = static_cast<SyncUnwinderThreadBuffer*>(
                                                   syncProfile->GetUWTBuffer());
  fill_buffer(profile, utb->GetBuffer(), ucV);
  utb->GetBuffer()->state = S_FULL;
  PseudoStack* stack = profile->GetPseudoStack();
  stack->addLinkedUWTBuffer(utb);
}

static void release_sync_buffer(LinkedUWTBuffer* buff)
{
  SyncUnwinderThreadBuffer* data = static_cast<SyncUnwinderThreadBuffer*>(buff);
  /* If state is S_FULL or S_EMPTYING then it is not safe to delete data */
  MOZ_ASSERT(data->GetBuffer()->state == S_EMPTY ||
             data->GetBuffer()->state == S_FILLING);
  delete data;
}

static void end_sync_buffer_unwind(LinkedUWTBuffer* buff)
{
  SyncUnwinderThreadBuffer* data = static_cast<SyncUnwinderThreadBuffer*>(buff);
  data->GetBuffer()->state = S_EMPTY;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

// Keeps count of how frames are recovered, which is useful for
// diagnostic purposes.
static void stats_notify_frame(int n_context, int n_cfi, int n_scanned)
{
  // Gather stats in intervals.
  static unsigned int nf_total    = 0; // total frames since last printout
  static unsigned int nf_CONTEXT  = 0;
  static unsigned int nf_CFI      = 0;
  static unsigned int nf_SCANNED  = 0;

  nf_CONTEXT += n_context;
  nf_CFI     += n_cfi;
  nf_SCANNED += n_scanned;
  nf_total   += (n_context + n_cfi + n_scanned);

  if (nf_total >= 5000) {
    LOGF("BPUnw frame stats: TOTAL %5u"
         "    CTX %4u    CFI %4u    SCAN %4u",
         nf_total, nf_CONTEXT, nf_CFI, nf_SCANNED);
    nf_total    = 0;
    nf_CONTEXT  = 0;
    nf_CFI      = 0;
    nf_SCANNED  = 0;
  }
}

static
void do_lul_unwind_Buffer(/*OUT*/PCandSP** pairs,
                          /*OUT*/unsigned int* nPairs,
                          UnwinderThreadBuffer* buff,
                          int buffNo /* for debug printing only */)
{
# if defined(SPS_ARCH_amd64) || defined(SPS_ARCH_x86)
  lul::UnwindRegs startRegs = buff->startRegs;
  if (0) {
    LOGF("Initial RIP = 0x%llx", (unsigned long long int)startRegs.xip.Value());
    LOGF("Initial RSP = 0x%llx", (unsigned long long int)startRegs.xsp.Value());
    LOGF("Initial RBP = 0x%llx", (unsigned long long int)startRegs.xbp.Value());
  }

# elif defined(SPS_ARCH_arm)
  lul::UnwindRegs startRegs = buff->startRegs;
  if (0) {
    LOGF("Initial R15 = 0x%llx", (unsigned long long int)startRegs.r15.Value());
    LOGF("Initial R13 = 0x%llx", (unsigned long long int)startRegs.r13.Value());
  }

# else
#   error "Unknown plat"
# endif

  // FIXME: should we reinstate the ability to use separate debug objects?
  // /* Make up a list of places where the debug objects might be. */
  // std::vector<std::string> debug_dirs;
# if defined(SPS_OS_linux)
  //  debug_dirs.push_back("/usr/lib/debug/lib");
  //  debug_dirs.push_back("/usr/lib/debug/usr/lib");
  //  debug_dirs.push_back("/usr/lib/debug/lib/x86_64-linux-gnu");
  //  debug_dirs.push_back("/usr/lib/debug/usr/lib/x86_64-linux-gnu");
# elif defined(SPS_OS_android)
  //  debug_dirs.push_back("/sdcard/symbols/system/lib");
  //  debug_dirs.push_back("/sdcard/symbols/system/bin");
# elif defined(SPS_OS_darwin)
  //  /* Nothing */
# else
#   error "Unknown plat"
# endif

  // Set the max number of scanned or otherwise dubious frames
  // to the user specified limit
  size_t scannedFramesAllowed
    = std::min(std::max(0, sUnwindStackScan), MAX_NATIVE_FRAMES);

  // The max number of frames is MAX_NATIVE_FRAMES, so as to avoid
  // the unwinder wasting a lot of time looping on corrupted stacks.
  uintptr_t framePCs[MAX_NATIVE_FRAMES];
  uintptr_t frameSPs[MAX_NATIVE_FRAMES];
  size_t framesAvail = mozilla::ArrayLength(framePCs);
  size_t framesUsed  = 0;
  size_t scannedFramesAcquired = 0;
  sLUL->Unwind( &framePCs[0], &frameSPs[0], 
                &framesUsed, &scannedFramesAcquired,
                framesAvail, scannedFramesAllowed,
                &startRegs, &buff->stackImg );

  if (LOGLEVEL >= 2)
    stats_notify_frame(/* context */ 1,
                       /* cfi     */ framesUsed - 1 - scannedFramesAcquired,
                       /* scanned */ scannedFramesAcquired);

  // PC values are now in framePCs[0 .. framesUsed-1], with [0] being
  // the innermost frame.  SP values are likewise in frameSPs[].
  *pairs  = (PCandSP*)calloc(framesUsed, sizeof(PCandSP));
  *nPairs = framesUsed;
  if (*pairs == nullptr) {
    *nPairs = 0;
    return;
  }

  if (framesUsed > 0) {
    for (unsigned int frame_index = 0; 
         frame_index < framesUsed; ++frame_index) {
      (*pairs)[framesUsed-1-frame_index].pc = framePCs[frame_index];
      (*pairs)[framesUsed-1-frame_index].sp = frameSPs[frame_index];
    }
  }

  if (LOGLEVEL >= 3) {
    LOGF("BPUnw: unwinder: seqNo %llu, buf %d: got %u frames",
         (unsigned long long int)buff->seqNo, buffNo,
         (unsigned int)framesUsed);
  }

  if (LOGLEVEL >= 2) {
    if (0 == (g_stats_totalSamples % 1000))
      LOGF("BPUnw: %llu total samples, %llu failed (buffer unavail), "
                   "%llu failed (thread unreg'd), ",
           (unsigned long long int)g_stats_totalSamples,
           (unsigned long long int)g_stats_noBuffAvail,
           (unsigned long long int)g_stats_thrUnregd);
  }
}

#endif /* defined(SPS_OS_windows) */
