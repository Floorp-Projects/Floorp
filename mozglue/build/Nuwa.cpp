/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <map>
#include <memory>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <alloca.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <vector>

#include "mozilla/Alignment.h"
#include "mozilla/LinkedList.h"
#include "mozilla/TaggedAnonymousMemory.h"
#include "Nuwa.h"


/* Support for telling Valgrind about the stack pointer changes that
   Nuwa makes.  Without this, Valgrind is unusable in Nuwa child
   processes due to the large number of false positives resulting from
   Nuwa's stack pointer changes.  See bug 1125091.
*/

#if defined(MOZ_VALGRIND)
# include <valgrind/memcheck.h>
#endif

#define DEBUG_VALGRIND_ANNOTATIONS 1

/* Call this as soon as possible after a setjmp() that has returned
   non-locally (that is, it is restoring some previous context).  This
   paints a small area -- half a page -- above SP as containing
   defined data in any area which is currently marked accessible.

   Note that in fact there are a few memory references to the stack
   after the setjmp but before the use of this macro, even when they
   appear consecutively in the source code.  But those accesses all
   appear to be stores, and since that part of the stack -- before we
   get to the VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE client request
   -- is marked as accessible-but-undefined, Memcheck doesn't
   complain.  Of course, once we get past the client request then even
   reading from the stack is "safe".

   VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE and VALGRIND_PRINTF each
   require 6 words of stack space.  In the worst case, in which the
   compiler allocates two different pieces of stack, the required
   extra stack is therefore 12 words, that is, 48 bytes on arm32.
*/
#if defined(MOZ_VALGRIND) && defined(VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE) \
    && defined(__arm__) && !defined(__aarch64__)
# define POST_SETJMP_RESTORE(_who) \
    do { \
      /* setjmp returned 1 (meaning "restored").  Paint the area */ \
      /* immediately above SP as "defined where it is accessible". */ \
      register unsigned long int sp; \
      __asm__ __volatile__("mov %0, sp" : "=r"(sp)); \
      unsigned long int len = 1024*2; \
      VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(sp, len); \
      if (DEBUG_VALGRIND_ANNOTATIONS) { \
        VALGRIND_PRINTF("Nuwa: POST_SETJMP_RESTORE: marking [0x%lx, +%ld) as " \
                        "Defined-if-Addressible, called by %s\n", \
                        sp, len, (_who)); \
      } \
    } while (0)
#else
# define POST_SETJMP_RESTORE(_who) /* */
#endif


using namespace mozilla;

/**
 * Provides the wrappers to a selected set of pthread and system-level functions
 * as the basis for implementing Zygote-like preforking mechanism.
 */

/**
 * Real functions for the wrappers.
 */
extern "C" {
#pragma GCC visibility push(default)
int __real_pthread_create(pthread_t *thread,
                          const pthread_attr_t *attr,
                          void *(*start_routine) (void *),
                          void *arg);
int __real_pthread_key_create(pthread_key_t *key, void (*destructor)(void*));
int __real_pthread_key_delete(pthread_key_t key);
pthread_t __real_pthread_self();
int __real_pthread_join(pthread_t thread, void **retval);
int __real_epoll_wait(int epfd,
                      struct epoll_event *events,
                      int maxevents,
                      int timeout);
int __real_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx);
int __real_pthread_cond_timedwait(pthread_cond_t *cond,
                                  pthread_mutex_t *mtx,
                                  const struct timespec *abstime);
int __real_pthread_mutex_lock(pthread_mutex_t *mtx);
int __real_pthread_mutex_trylock(pthread_mutex_t *mtx);
int __real_poll(struct pollfd *fds, nfds_t nfds, int timeout);
int __real_epoll_create(int size);
int __real_socketpair(int domain, int type, int protocol, int sv[2]);
int __real_pipe2(int __pipedes[2], int flags);
int __real_pipe(int __pipedes[2]);
int __real_epoll_ctl(int aEpollFd, int aOp, int aFd, struct epoll_event *aEvent);
int __real_close(int aFd);
#pragma GCC visibility pop
}

#define REAL(s) __real_##s

/**
 * A Nuwa process is started by preparing.  After preparing, it waits
 * for all threads becoming frozen. Then, it is ready while all
 * threads are frozen.
 */
static bool sIsNuwaProcess = false; // This process is a Nuwa process.
static bool sIsNuwaChildProcess = false; // This process is spawned from Nuwa.
static bool sIsFreezing = false; // Waiting for all threads getting frozen.
static bool sNuwaReady = false;  // Nuwa process is ready.
static bool sNuwaPendingSpawn = false; // Are there any pending spawn requests?
static bool sNuwaForking = false;

// Fds of transports of top level protocols.
static NuwaProtoFdInfo sProtoFdInfos[NUWA_TOPLEVEL_MAX];
static int sProtoFdInfosSize = 0;

typedef std::vector<std::pair<pthread_key_t, void *> >
TLSInfoList;

/**
 * Return the system's page size
 */
static size_t getPageSize(void) {
#ifdef HAVE_GETPAGESIZE
  return getpagesize();
#elif defined(_SC_PAGESIZE)
  return sysconf(_SC_PAGESIZE);
#elif defined(PAGE_SIZE)
  return PAGE_SIZE;
#else
  #warning "Hard-coding page size to 4096 bytes"
  return 4096
#endif
}

/**
 * Use 1 MiB stack size as Android does.
 */
#ifndef NUWA_STACK_SIZE
#define NUWA_STACK_SIZE (1024 * 1024)
#endif

#define NATIVE_THREAD_NAME_LENGTH 16

typedef struct nuwa_construct {
  typedef void(*construct_t)(void*);

  construct_t construct;
  void *arg;

  nuwa_construct(construct_t aConstruct, void *aArg)
    : construct(aConstruct)
    , arg(aArg)
  { }

  nuwa_construct(const nuwa_construct&) = default;
  nuwa_construct& operator=(const nuwa_construct&) = default;

} nuwa_construct_t;

struct thread_info : public mozilla::LinkedListElement<thread_info> {
  pthread_t origThreadID;
  pthread_t recreatedThreadID;
  pthread_attr_t threadAttr;
  jmp_buf jmpEnv;
  jmp_buf retEnv;

  int flags;

  void *(*startupFunc)(void *arg);
  void *startupArg;

  // The thread specific function to recreate the new thread. It's executed
  // after the thread is recreated.

  std::vector<nuwa_construct_t> *recrFunctions;
  void addThreadConstructor(const nuwa_construct_t *construct) {
    if (!recrFunctions) {
      recrFunctions = new std::vector<nuwa_construct_t>();
    }

    recrFunctions->push_back(*construct);
  }

  TLSInfoList tlsInfo;

  /**
   * We must ensure that the recreated thread has entered pthread_cond_wait() or
   * similar functions before proceeding to recreate the next one. Otherwise, if
   * the next thread depends on the same mutex, it may be used in an incorrect
   * state.  To do this, the main thread must unconditionally acquire the mutex.
   * The mutex is unconditionally released when the recreated thread enters
   * pthread_cond_wait().  The recreated thread may have locked the mutex itself
   * (if the pthread_mutex_trylock succeeded) or another thread may have already
   * held the lock.  If the recreated thread did lock the mutex we must balance
   * that with another unlock on the main thread, which is signaled by
   * condMutexNeedsBalancing.
   */
  pthread_mutex_t *condMutex;
  bool condMutexNeedsBalancing;

  size_t stackSize;
  size_t guardSize;
  void *stk;

  pid_t origNativeThreadID;
  pid_t recreatedNativeThreadID;
  char nativeThreadName[NATIVE_THREAD_NAME_LENGTH];
};

typedef struct thread_info thread_info_t;

static thread_info_t *sCurrentRecreatingThread = nullptr;

/**
 * This function runs the custom recreation function registered when calling
 * NuwaMarkCurrentThread() after thread stack is restored.
 */
static void
RunCustomRecreation() {
  thread_info_t *tinfo = sCurrentRecreatingThread;
  if (tinfo->recrFunctions) {
    for (auto iter = tinfo->recrFunctions->begin();
         iter != tinfo->recrFunctions->end();
         iter++) {
      iter->construct(iter->arg);
    }
  }
}

/**
 * Every thread should be marked as either TINFO_FLAG_NUWA_SUPPORT or
 * TINFO_FLAG_NUWA_SKIP, or it means a potential error.  We force
 * Gecko code to mark every single thread to make sure there are no accidents
 * when recreating threads with Nuwa.
 *
 * Threads marked as TINFO_FLAG_NUWA_SUPPORT can be checkpointed explicitly, by
 * calling NuwaCheckpointCurrentThread(), or implicitly when they call into wrapped
 * functions like pthread_mutex_lock(), epoll_wait(), etc.
 * TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT denotes the explicitly checkpointed thread.
 */
#define TINFO_FLAG_NUWA_SUPPORT 0x1
#define TINFO_FLAG_NUWA_SKIP 0x2
#define TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT 0x4

static std::vector<nuwa_construct_t> sConstructors;
static std::vector<nuwa_construct_t> sFinalConstructors;

class TLSKey
: public std::pair<pthread_key_t, void (*)(void*)>
, public LinkedListElement<TLSKey>
{
public:
  TLSKey() {}

  TLSKey(pthread_key_t aKey, void (*aDestructor)(void*))
  : std::pair<pthread_key_t, void (*)(void*)>(aKey, aDestructor)
  {}

  static void* operator new(size_t size) {
    if (sUsed)
      return ::operator new(size);
    sUsed = true;
    return sFirstElement.addr();
  }

  static void operator delete(void* ptr) {
    if (ptr == sFirstElement.addr()) {
      sUsed = false;
      return;
    }
    ::operator delete(ptr);
  }

private:
  static bool sUsed;
  static AlignedStorage2<TLSKey> sFirstElement;
};

bool TLSKey::sUsed = false;
AlignedStorage2<TLSKey> TLSKey::sFirstElement;

static AutoCleanLinkedList<TLSKey> sTLSKeys;

/**
 * This mutex is used to block the running threads and freeze their contexts.
 * PrepareNuwaProcess() is the first one to acquire the lock. Further attempts
 * to acquire this mutex (in the freeze point macros) will block and freeze the
 * calling thread.
 */
static pthread_mutex_t sThreadFreezeLock = PTHREAD_MUTEX_INITIALIZER;

static thread_info_t sMainThread;
static int sThreadCount = 0;
static int sThreadFreezeCount = 0;

// Bug 1008254: LinkedList's destructor asserts that the list is empty.
// But here, on exit, when the global sAllThreads list
// is destroyed, it may or may not be empty. Bug 1008254 comment 395 has a log
// when there were 8 threads remaining on exit. So this assertion was
// intermittently (almost every second time) failing.
// As a work-around to avoid this intermittent failure, we clear the list on
// exit just before it gets destroyed. This is the only purpose of that
// AllThreadsListType subclass.
struct AllThreadsListType : public AutoCleanLinkedList<thread_info_t>
{
  ~AllThreadsListType()
  {
    if (!isEmpty()) {
      __android_log_print(ANDROID_LOG_WARN, "Nuwa",
                          "Threads remaining at exit:\n");
      int n = 0;
      for (const thread_info_t* t = getFirst(); t; t = t->getNext()) {
        __android_log_print(ANDROID_LOG_WARN, "Nuwa",
                            "  %.*s (origNativeThreadID=%d recreatedNativeThreadID=%d)\n",
                            NATIVE_THREAD_NAME_LENGTH,
                            t->nativeThreadName,
                            t->origNativeThreadID,
                            t->recreatedNativeThreadID);
        n++;
      }
      __android_log_print(ANDROID_LOG_WARN, "Nuwa",
                          "total: %d outstanding threads. "
                          "Please fix them so they're destroyed before this point!\n", n);
      __android_log_print(ANDROID_LOG_WARN, "Nuwa",
                          "note: sThreadCount=%d, sThreadFreezeCount=%d\n",
                          sThreadCount,
                          sThreadFreezeCount);
    }
  }
};
static AllThreadsListType sAllThreads;
static AllThreadsListType sExitingThreads;

/**
 * This mutex protects the access to thread info:
 * sAllThreads, sThreadCount, sThreadFreezeCount, sRecreateVIPCount.
 */
static pthread_mutex_t sThreadCountLock = PTHREAD_MUTEX_INITIALIZER;
/**
 * This condition variable lets MakeNuwaProcess() wait until all recreated
 * threads are frozen.
 */
static pthread_cond_t sThreadChangeCond = PTHREAD_COND_INITIALIZER;

/**
 * This mutex and condition variable is used to serialize the fork requests
 * from the parent process.
 */
static pthread_mutex_t sForkLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sForkWaitCond = PTHREAD_COND_INITIALIZER;

/**
 * sForkWaitCondChanged will be reset to false on the IPC thread before
 * and will be changed to true on the main thread to indicate that the condition
 * that the IPC thread is waiting for has already changed.
 */
static bool sForkWaitCondChanged = false;

/**
 * This mutex protects the access to sTLSKeys, which keeps track of existing
 * TLS Keys.
 */
static pthread_mutex_t sTLSKeyLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
static int sThreadSkipCount = 0;

static thread_info_t *
GetThreadInfoInner(pthread_t threadID) {
  for (thread_info_t *tinfo = sAllThreads.getFirst();
       tinfo;
       tinfo = tinfo->getNext()) {
    if (pthread_equal(tinfo->origThreadID, threadID)) {
      return tinfo;
    }
  }

  for (thread_info_t *tinfo = sExitingThreads.getFirst();
       tinfo;
       tinfo = tinfo->getNext()) {
    if (pthread_equal(tinfo->origThreadID, threadID)) {
      return tinfo;
    }
  }

  return nullptr;
}

/**
 * Get thread info using the specified thread ID.
 *
 * @return thread_info_t which has threadID == specified threadID
 */
static thread_info_t *
GetThreadInfo(pthread_t threadID) {
  REAL(pthread_mutex_lock)(&sThreadCountLock);

  thread_info_t *tinfo = GetThreadInfoInner(threadID);

  pthread_mutex_unlock(&sThreadCountLock);
  return tinfo;
}

#if !defined(HAVE_THREAD_TLS_KEYWORD)
/**
 * Get thread info of the current thread.
 *
 * @return thread_info_t for the current thread.
 */
static thread_info_t *
GetCurThreadInfo() {
  pthread_t threadID = REAL(pthread_self)();
  pthread_t thread_info_t::*threadIDptr =
      (sIsNuwaProcess ?
         &thread_info_t::origThreadID :
         &thread_info_t::recreatedThreadID);

  REAL(pthread_mutex_lock)(&sThreadCountLock);
  thread_info_t *tinfo;
  for (tinfo = sAllThreads.getFirst();
       tinfo;
       tinfo = tinfo->getNext()) {
    if (pthread_equal(tinfo->*threadIDptr, threadID)) {
      break;
    }
  }
  pthread_mutex_unlock(&sThreadCountLock);
  return tinfo;
}
#define CUR_THREAD_INFO GetCurThreadInfo()
#define SET_THREAD_INFO(x) /* Nothing to do. */
#else
// Is not nullptr only for threads created by pthread_create() in an Nuwa process.
// It is always nullptr for the main thread.
static __thread thread_info_t *sCurThreadInfo = nullptr;
#define CUR_THREAD_INFO sCurThreadInfo
#define SET_THREAD_INFO(x) do { sCurThreadInfo = (x); } while(0)
#endif  // HAVE_THREAD_TLS_KEYWORD

/*
 * Track all epoll fds and handling events.
 */
class EpollManager {
public:
  class EpollInfo {
  public:
    typedef struct epoll_event Events;
    typedef std::map<int, Events> EpollEventsMap;
    typedef EpollEventsMap::iterator iterator;
    typedef EpollEventsMap::const_iterator const_iterator;

    EpollInfo(): mBackSize(0) {}
    EpollInfo(int aBackSize): mBackSize(aBackSize) {}
    EpollInfo(const EpollInfo &aOther): mEvents(aOther.mEvents)
                                      , mBackSize(aOther.mBackSize) {
    }
    ~EpollInfo() {
      mEvents.clear();
    }

    void AddEvents(int aFd, Events &aEvents) {
      std::pair<iterator, bool> pair =
        mEvents.insert(std::make_pair(aFd, aEvents));
      if (!pair.second) {
        abort();
      }
    }

    void RemoveEvents(int aFd) {
      if (!mEvents.erase(aFd)) {
        abort();
      }
    }

    void ModifyEvents(int aFd, Events &aEvents) {
      iterator it = mEvents.find(aFd);
      if (it == mEvents.end()) {
        abort();
      }
      it->second = aEvents;
    }

    const Events &FindEvents(int aFd) const {
      const_iterator it = mEvents.find(aFd);
      if (it == mEvents.end()) {
        abort();
      }
      return it->second;
    }

    int Size() const { return mEvents.size(); }

    // Iterator with values of <fd, Events> pairs.
    const_iterator begin() const { return mEvents.begin(); }
    const_iterator end() const { return mEvents.end(); }

    int BackSize() const { return mBackSize; }

  private:
    EpollEventsMap mEvents;
    int mBackSize;

    friend class EpollManager;
  };

  typedef std::map<int, EpollInfo> EpollInfoMap;
  typedef EpollInfoMap::iterator iterator;
  typedef EpollInfoMap::const_iterator const_iterator;

public:
  void AddEpollInfo(int aEpollFd, int aBackSize) {
    EpollInfo *oldinfo = FindEpollInfo(aEpollFd);
    if (oldinfo != nullptr) {
      abort();
    }
    mEpollFdsInfo[aEpollFd] = EpollInfo(aBackSize);
  }

  EpollInfo *FindEpollInfo(int aEpollFd) {
    iterator it = mEpollFdsInfo.find(aEpollFd);
    if (it == mEpollFdsInfo.end()) {
      return nullptr;
    }
    return &it->second;
  }

  void RemoveEpollInfo(int aEpollFd) {
    if (!mEpollFdsInfo.erase(aEpollFd)) {
      abort();
    }
  }

  int Size() const { return mEpollFdsInfo.size(); }

  // Iterator of <epollfd, EpollInfo> pairs.
  const_iterator begin() const { return mEpollFdsInfo.begin(); }
  const_iterator end() const { return mEpollFdsInfo.end(); }

  static EpollManager *Singleton() {
    if (!sInstance) {
      sInstance = new EpollManager();
    }
    return sInstance;
  }

  static void Shutdown() {
    if (!sInstance) {
      abort();
    }

    delete sInstance;
    sInstance = nullptr;
  }

private:
  static EpollManager *sInstance;
  ~EpollManager() {
    mEpollFdsInfo.clear();
  }

  EpollInfoMap mEpollFdsInfo;

  EpollManager() {}
};

EpollManager* EpollManager::sInstance;

static thread_info_t *
thread_info_new(void) {
  /* link tinfo to sAllThreads */
  thread_info_t *tinfo = new thread_info_t();
  tinfo->flags = 0;
  tinfo->recrFunctions = nullptr;
  tinfo->recreatedThreadID = 0;
  tinfo->recreatedNativeThreadID = 0;
  tinfo->condMutex = nullptr;
  tinfo->condMutexNeedsBalancing = false;

  // Default stack and guard size.
  tinfo->stackSize = NUWA_STACK_SIZE;
  tinfo->guardSize = getPageSize();

  REAL(pthread_mutex_lock)(&sThreadCountLock);
  // Insert to the tail.
  sAllThreads.insertBack(tinfo);

  sThreadCount++;
  pthread_cond_signal(&sThreadChangeCond);
  pthread_mutex_unlock(&sThreadCountLock);

  return tinfo;
}

static void
thread_attr_init(thread_info_t *tinfo, const pthread_attr_t *tattr)
{
  pthread_attr_init(&tinfo->threadAttr);

  if (tattr) {
    // Override default thread stack and guard size with tattr.
    pthread_attr_getstacksize(tattr, &tinfo->stackSize);
    pthread_attr_getguardsize(tattr, &tinfo->guardSize);

    size_t pageSize = getPageSize();

    tinfo->stackSize = (tinfo->stackSize + pageSize - 1) % pageSize;
    tinfo->guardSize = (tinfo->guardSize + pageSize - 1) % pageSize;

    int detachState = 0;
    pthread_attr_getdetachstate(tattr, &detachState);
    pthread_attr_setdetachstate(&tinfo->threadAttr, detachState);
  }

  tinfo->stk = MozTaggedAnonymousMmap(nullptr,
                                      tinfo->stackSize + tinfo->guardSize,
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS,
                                      /* fd */ -1,
                                      /* offset */ 0,
                                      "nuwa-thread-stack");

  // Add protection to stack overflow: mprotect() stack top (the page at the
  // lowest address) so we crash instead of corrupt other content that is
  // malloc()'d.
  mprotect(tinfo->stk, tinfo->guardSize, PROT_NONE);

  pthread_attr_setstack(&tinfo->threadAttr,
                        (char*)tinfo->stk + tinfo->guardSize,
                        tinfo->stackSize);
}

static void
thread_info_cleanup(void *arg) {
  if (sNuwaForking) {
    // We shouldn't have any thread exiting when we are forking a new process.
    abort();
  }

  thread_info_t *tinfo = (thread_info_t *)arg;
  pthread_attr_destroy(&tinfo->threadAttr);

  munmap(tinfo->stk, tinfo->stackSize + tinfo->guardSize);

  REAL(pthread_mutex_lock)(&sThreadCountLock);
  /* unlink tinfo from sAllThreads */
  tinfo->remove();
  pthread_mutex_unlock(&sThreadCountLock);

  if (tinfo->recrFunctions) {
    delete tinfo->recrFunctions;
  }

  // while sThreadCountLock is held, since delete calls wrapped functions
  // which try to lock sThreadCountLock. This results in deadlock. And we
  // need to delete |tinfo| before decreasing sThreadCount, so Nuwa won't
  // get ready before tinfo is cleaned.
  delete tinfo;

  REAL(pthread_mutex_lock)(&sThreadCountLock);
  sThreadCount--;
  pthread_cond_signal(&sThreadChangeCond);
  pthread_mutex_unlock(&sThreadCountLock);
}

static void
EnsureThreadExited(thread_info_t *tinfo) {
  pid_t thread = sIsNuwaProcess ? tinfo->origNativeThreadID
                                     : tinfo->recreatedNativeThreadID;
  // Wait until the target thread exits. Note that we use tgkill() instead of
  // pthread_kill() because of:
  // 1. Use after free inside pthread implementation.
  // 2. Race due to pthread_t reuse when a thread is created.
  while (!syscall(__NR_tgkill, getpid(), thread, 0)) {
    sched_yield();
  }
}

static void*
safe_thread_info_cleanup(void *arg)
{
  thread_info_t *tinfo = (thread_info_t *)arg;

  // We need to ensure the thread is really dead before cleaning up tinfo.
  EnsureThreadExited(tinfo);
  thread_info_cleanup(tinfo);

  return nullptr;
}

static void
MaybeCleanUpDetachedThread(thread_info_t *tinfo)
{
  if (pthread_getattr_np(REAL(pthread_self()), &tinfo->threadAttr)) {
    return;
  }

  int detachState = 0;
  if (pthread_attr_getdetachstate(&tinfo->threadAttr, &detachState) ||
      detachState == PTHREAD_CREATE_JOINABLE) {
    // We only clean up tinfo of a detached thread. A joinable thread
    // will be cleaned up in __wrap_pthread_join().
    return;
  }

  // Create a detached thread to safely clean up the current thread.
  pthread_t thread;
  if (!REAL(pthread_create)(&thread,
                            nullptr,
                            safe_thread_info_cleanup,
                            tinfo)) {
    pthread_detach(thread);
  }
}

static void
invalidate_thread_info(void *arg) {
  REAL(pthread_mutex_lock)(&sThreadCountLock);

  // Unlink tinfo from sAllThreads to make it invisible from CUR_THREAD_INFO so
  // it won't be misused by a newly created thread.
  thread_info_t *tinfo = (thread_info_t*) arg;
  tinfo->remove();
  sExitingThreads.insertBack(tinfo);

  pthread_mutex_unlock(&sThreadCountLock);

  MaybeCleanUpDetachedThread(tinfo);
}

static void *
_thread_create_startup(void *arg) {
  thread_info_t *tinfo = (thread_info_t *)arg;
  void *r;

  // Save thread info; especially, stackaddr & stacksize.
  // Reuse the stack in the new thread.
  pthread_getattr_np(REAL(pthread_self)(), &tinfo->threadAttr);

  SET_THREAD_INFO(tinfo);
  tinfo->origThreadID = REAL(pthread_self)();
  tinfo->origNativeThreadID = gettid();

  r = tinfo->startupFunc(tinfo->startupArg);

  return r;
}

// reserve STACK_RESERVED_SZ * 4 bytes for thread_recreate_startup().
#define STACK_RESERVED_SZ 96
#define STACK_SENTINEL(v) ((v)[0])
#define STACK_SENTINEL_VALUE(v) ((uint32_t)(v) ^ 0xdeadbeef)

static void *
thread_create_startup(void *arg) {
  /*
   * Dark Art!! Never try to do the same unless you are ABSOLUTELY sure of
   * what you are doing!
   *
   * This function is here for reserving stack space before calling
   * _thread_create_startup().  see also thread_create_startup();
   */
  void *r;
  volatile uint32_t reserved[STACK_RESERVED_SZ];

  // Reserve stack space.
  STACK_SENTINEL(reserved) = STACK_SENTINEL_VALUE(reserved);

  r = _thread_create_startup(arg);

  // Check if the reservation is enough.
  if (STACK_SENTINEL(reserved) != STACK_SENTINEL_VALUE(reserved)) {
    abort();                    // Did not reserve enough stack space.
  }

  // Get tinfo before invalidating it. Note that we cannot use arg directly here
  // because thread_recreate_startup() also runs on the same stack area and
  // could corrupt the value.
  thread_info_t *tinfo = CUR_THREAD_INFO;
  invalidate_thread_info(tinfo);

  if (!sIsNuwaProcess) {
    longjmp(tinfo->retEnv, 1);

    // Never go here!
    abort();
  }

  return r;
}

extern "C" MFBT_API int
__wrap_pthread_create(pthread_t *thread,
                      const pthread_attr_t *attr,
                      void *(*start_routine) (void *),
                      void *arg) {
  if (!sIsNuwaProcess) {
    return REAL(pthread_create)(thread, attr, start_routine, arg);
  }

  thread_info_t *tinfo = thread_info_new();
  thread_attr_init(tinfo, attr);
  tinfo->startupFunc = start_routine;
  tinfo->startupArg = arg;

  int rv = REAL(pthread_create)(thread,
                                &tinfo->threadAttr,
                                thread_create_startup,
                                tinfo);
  if (rv) {
    thread_info_cleanup(tinfo);
  } else {
    tinfo->origThreadID = *thread;
  }

  return rv;
}

// TLS related

/**
 * Iterates over the existing TLS keys and store the TLS data for the current
 * thread in tinfo.
 */
static void
SaveTLSInfo(thread_info_t *tinfo) {
  MOZ_RELEASE_ASSERT(REAL(pthread_mutex_lock)(&sTLSKeyLock) == 0);
  tinfo->tlsInfo.clear();
  for (TLSKey *it = sTLSKeys.getFirst(); it != nullptr; it = it->getNext()) {
    void *value = pthread_getspecific(it->first);
    if (value == nullptr) {
      continue;
    }

    pthread_key_t key = it->first;
    tinfo->tlsInfo.push_back(TLSInfoList::value_type(key, value));
  }
  MOZ_RELEASE_ASSERT(pthread_mutex_unlock(&sTLSKeyLock) == 0);
}

/**
 * Restores the TLS data for the current thread from tinfo.
 */
static void
RestoreTLSInfo(thread_info_t *tinfo) {
  for (TLSInfoList::const_iterator it = tinfo->tlsInfo.begin();
       it != tinfo->tlsInfo.end();
       it++) {
    pthread_key_t key = it->first;
    const void *value = it->second;
    if (pthread_setspecific(key, value)) {
      abort();
    }
  }

  SET_THREAD_INFO(tinfo);
  tinfo->recreatedThreadID = REAL(pthread_self)();
  tinfo->recreatedNativeThreadID = gettid();
}

extern "C" MFBT_API int
__wrap_pthread_key_create(pthread_key_t *key, void (*destructor)(void*)) {
  int rv = REAL(pthread_key_create)(key, destructor);
  if (rv != 0) {
    return rv;
  }
  MOZ_RELEASE_ASSERT(REAL(pthread_mutex_lock)(&sTLSKeyLock) == 0);
  sTLSKeys.insertBack(new TLSKey(*key, destructor));
  MOZ_RELEASE_ASSERT(pthread_mutex_unlock(&sTLSKeyLock) == 0);
  return 0;
}

extern "C" MFBT_API int
__wrap_pthread_key_delete(pthread_key_t key) {
  // Don't call pthread_key_delete() for Nuwa-forked processes because bionic's
  // pthread_key_delete() implementation can touch the thread stack that was
  // freed in thread_info_cleanup().
  int rv = sIsNuwaChildProcess ?
             0 : REAL(pthread_key_delete)(key);
  if (rv != 0) {
    return rv;
  }
  MOZ_RELEASE_ASSERT(REAL(pthread_mutex_lock)(&sTLSKeyLock) == 0);
  for (TLSKey *it = sTLSKeys.getFirst(); it != nullptr; it = it->getNext()) {
    if (key == it->first) {
      delete it;
      break;
    }
  }
  MOZ_RELEASE_ASSERT(pthread_mutex_unlock(&sTLSKeyLock) == 0);
  return 0;
}

extern "C" MFBT_API pthread_t
__wrap_pthread_self() {
  thread_info_t *tinfo = CUR_THREAD_INFO;
  if (tinfo) {
    // For recreated thread, masquerade as the original thread in the Nuwa
    // process.
    return tinfo->origThreadID;
  }
  return REAL(pthread_self)();
}

extern "C" MFBT_API int
__wrap_pthread_join(pthread_t thread, void **retval) {
  thread_info_t *tinfo = GetThreadInfo(thread);
  if (tinfo == nullptr) {
    return REAL(pthread_join)(thread, retval);
  }

  pthread_t thread_info_t::*threadIDptr =
        (sIsNuwaProcess ?
           &thread_info_t::origThreadID :
           &thread_info_t::recreatedThreadID);

  // pthread_join() uses the origThreadID or recreatedThreadID depending on
  // whether we are in Nuwa or forked processes.
  int rc = REAL(pthread_join)(tinfo->*threadIDptr, retval);

  // Before Android L, bionic wakes up the caller of pthread_join() with
  // pthread_cond_signal() so the thread can still use the stack for some while.
  // Call safe_thread_info_cleanup() to destroy tinfo after the thread really
  // exits.
  safe_thread_info_cleanup(tinfo);

  return rc;
}

/**
 * The following are used to synchronize between the main thread and the
 * thread being recreated. The main thread will wait until the thread is woken
 * up from the freeze points or the blocking intercepted functions and then
 * proceed to recreate the next frozen thread.
 *
 * In thread recreation, the main thread recreates the frozen threads one by
 * one. The recreated threads will be "gated" until the main thread "opens the
 * gate" to let them run freely as if they were created from scratch. The VIP
 * threads gets the chance to run first after their thread stacks are recreated
 * (using longjmp()) so they can adjust their contexts to a valid, consistent
 * state. The threads frozen waiting for pthread condition variables are VIP
 * threads. After woken up they need to run first to make the associated mutex
 * in a valid state to maintain the semantics of the intercepted function calls
 * (like pthread_cond_wait()).
 */

// Used to synchronize the main thread and the thread being recreated so that
// only one thread is allowed to be recreated at a time.
static pthread_mutex_t sRecreateWaitLock = PTHREAD_MUTEX_INITIALIZER;
// Used to block recreated threads until the main thread "opens the gate".
static pthread_mutex_t sRecreateGateLock = PTHREAD_MUTEX_INITIALIZER;
// Used to block the main thread from "opening the gate" until all VIP threads
// have been recreated.
static pthread_mutex_t sRecreateVIPGateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sRecreateVIPCond = PTHREAD_COND_INITIALIZER;
static int sRecreateVIPCount = 0;
static int sRecreateGatePassed = 0;

/**
 * Thread recreation macros.
 *
 * The following macros are used in the forked process to synchronize and
 * control the progress of thread recreation.
 *
 * 1. RECREATE_START() is first called in the beginning of thread
 *    recreation to set sRecreateWaitLock and sRecreateGateLock in locked
 *    state.
 * 2. For each frozen thread:
 *    2.1. RECREATE_BEFORE() to set the thread being recreated.
 *    2.2. thread_recreate() to recreate the frozen thread.
 *    2.3. Main thread calls RECREATE_WAIT() to wait on sRecreateWaitLock until
 *         the thread is recreated from the freeze point and calls
 *         RECREATE_CONTINUE() to release sRecreateWaitLock.
 *    2.3. Non-VIP threads are blocked on RECREATE_GATE(). VIP threads calls
 *         RECREATE_PASS_VIP() to mark that a VIP thread is successfully
 *         recreated and then is blocked by calling RECREATE_GATE_VIP().
 * 3. RECREATE_WAIT_ALL_VIP() to wait until all VIP threads passed, that is,
 *    VIP threads already has their contexts (mainly pthread mutex) in a valid
 *    state.
 * 4. RECREATE_OPEN_GATE() to unblock threads blocked by sRecreateGateLock.
 * 5. RECREATE_FINISH() to complete thread recreation.
 */
#define RECREATE_START()                          \
  do {                                            \
    REAL(pthread_mutex_lock)(&sRecreateWaitLock); \
    REAL(pthread_mutex_lock)(&sRecreateGateLock); \
  } while(0)
#define RECREATE_BEFORE(info) do { sCurrentRecreatingThread = info; } while(0)
#define RECREATE_WAIT() REAL(pthread_mutex_lock)(&sRecreateWaitLock)
#define RECREATE_CONTINUE() do {                \
    RunCustomRecreation();                      \
    pthread_mutex_unlock(&sRecreateWaitLock);   \
  } while(0)
#define RECREATE_FINISH() pthread_mutex_unlock(&sRecreateWaitLock)
#define RECREATE_GATE()                           \
  do {                                            \
    REAL(pthread_mutex_lock)(&sRecreateGateLock); \
    sRecreateGatePassed++;                        \
    pthread_mutex_unlock(&sRecreateGateLock);     \
  } while(0)
#define RECREATE_OPEN_GATE() pthread_mutex_unlock(&sRecreateGateLock)
#define RECREATE_GATE_VIP()                       \
  do {                                            \
    REAL(pthread_mutex_lock)(&sRecreateGateLock); \
    pthread_mutex_unlock(&sRecreateGateLock);     \
  } while(0)
#define RECREATE_PASS_VIP()                          \
  do {                                               \
    REAL(pthread_mutex_lock)(&sRecreateVIPGateLock); \
    sRecreateGatePassed++;                           \
    pthread_cond_signal(&sRecreateVIPCond);          \
    pthread_mutex_unlock(&sRecreateVIPGateLock);     \
  } while(0)
#define RECREATE_WAIT_ALL_VIP()                        \
  do {                                                 \
    REAL(pthread_mutex_lock)(&sRecreateVIPGateLock);   \
    while(sRecreateGatePassed < sRecreateVIPCount) {   \
      REAL(pthread_cond_wait)(&sRecreateVIPCond,       \
                        &sRecreateVIPGateLock);        \
    }                                                  \
    pthread_mutex_unlock(&sRecreateVIPGateLock);       \
  } while(0)

/**
 * Thread freeze points. Note that the freeze points are implemented as macros
 * so as not to garble the content of the stack after setjmp().
 *
 * In the nuwa process, when a thread supporting nuwa calls a wrapper
 * function, freeze point 1 setjmp()s to save the state. We only allow the
 * thread to be frozen in the wrapper functions. If thread freezing is not
 * enabled yet, the wrapper functions act like their wrapped counterparts,
 * except for the extra actions in the freeze points. If thread freezing is
 * enabled, the thread will be frozen by calling one of the wrapper functions.
 * The threads can be frozen in any of the following points:
 *
 * 1) Freeze point 1: this is the point where we setjmp() in the nuwa process
 *    and longjmp() in the spawned process. If freezing is enabled, then the
 *    current thread blocks by acquiring an already locked mutex,
 *    sThreadFreezeLock.
 * 2) The wrapped function: the function that might block waiting for some
 *    resource or condition.
 * 3) Freeze point 2: blocks the current thread by acquiring sThreadFreezeLock.
 *    If freezing is not enabled then revert the counter change in freeze
 *    point 1.
 *
 * Note: the purpose of the '(void) variable;' statements is to avoid
 *       -Wunused-but-set-variable warnings.
 */
#define THREAD_FREEZE_POINT1()                                 \
  bool freezeCountChg = false;                                 \
  bool recreated = false;                                      \
  (void) recreated;                                            \
  volatile bool freezePoint2 = false;                          \
  (void) freezePoint2;                                         \
  thread_info_t *tinfo;                                        \
  if (sIsNuwaProcess &&                                        \
      (tinfo = CUR_THREAD_INFO) &&                             \
      (tinfo->flags & TINFO_FLAG_NUWA_SUPPORT) &&              \
      !(tinfo->flags & TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT)) { \
    if (!setjmp(tinfo->jmpEnv)) {                              \
      REAL(pthread_mutex_lock)(&sThreadCountLock);             \
      SaveTLSInfo(tinfo);                                      \
      sThreadFreezeCount++;                                    \
      freezeCountChg = true;                                   \
      pthread_cond_signal(&sThreadChangeCond);                 \
      pthread_mutex_unlock(&sThreadCountLock);                 \
                                                               \
      if (sIsFreezing) {                                       \
        REAL(pthread_mutex_lock)(&sThreadFreezeLock);          \
        /* Never return from the pthread_mutex_lock() call. */ \
        abort();                                               \
      }                                                        \
    } else {                                                   \
      POST_SETJMP_RESTORE("THREAD_FREEZE_POINT1");             \
      RECREATE_CONTINUE();                                     \
      RECREATE_GATE();                                         \
      freezeCountChg = false;                                  \
      recreated = true;                                        \
    }                                                          \
  }

#define THREAD_FREEZE_POINT1_VIP()                             \
  bool freezeCountChg = false;                                 \
  bool recreated = false;                                      \
  volatile bool freezePoint1 = false;                          \
  volatile bool freezePoint2 = false;                          \
  thread_info_t *tinfo;                                        \
  if (sIsNuwaProcess &&                                        \
      (tinfo = CUR_THREAD_INFO) &&                             \
      (tinfo->flags & TINFO_FLAG_NUWA_SUPPORT) &&              \
      !(tinfo->flags & TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT)) { \
    if (!setjmp(tinfo->jmpEnv)) {                              \
      REAL(pthread_mutex_lock)(&sThreadCountLock);             \
      SaveTLSInfo(tinfo);                                      \
      sThreadFreezeCount++;                                    \
      sRecreateVIPCount++;                                     \
      freezeCountChg = true;                                   \
      pthread_cond_signal(&sThreadChangeCond);                 \
      pthread_mutex_unlock(&sThreadCountLock);                 \
                                                               \
      if (sIsFreezing) {                                       \
        freezePoint1 = true;                                   \
        REAL(pthread_mutex_lock)(&sThreadFreezeLock);          \
        /* Never return from the pthread_mutex_lock() call. */ \
        abort();                                               \
      }                                                        \
    } else {                                                   \
      POST_SETJMP_RESTORE("THREAD_FREEZE_POINT1_VIP");         \
      freezeCountChg = false;                                  \
      recreated = true;                                        \
    }                                                          \
  }

#define THREAD_FREEZE_POINT2()                               \
  if (freezeCountChg) {                                      \
    REAL(pthread_mutex_lock)(&sThreadCountLock);             \
    if (sNuwaReady && sIsNuwaProcess) {                      \
      pthread_mutex_unlock(&sThreadCountLock);               \
      freezePoint2 = true;                                   \
      REAL(pthread_mutex_lock)(&sThreadFreezeLock);          \
      /* Never return from the pthread_mutex_lock() call. */ \
      abort();                                               \
    }                                                        \
    sThreadFreezeCount--;                                    \
    pthread_cond_signal(&sThreadChangeCond);                 \
    pthread_mutex_unlock(&sThreadCountLock);                 \
  }

#define THREAD_FREEZE_POINT2_VIP()                           \
  if (freezeCountChg) {                                      \
    REAL(pthread_mutex_lock)(&sThreadCountLock);             \
    if (sNuwaReady && sIsNuwaProcess) {                      \
      pthread_mutex_unlock(&sThreadCountLock);               \
      freezePoint2 = true;                                   \
      REAL(pthread_mutex_lock)(&sThreadFreezeLock);          \
      /* Never return from the pthread_mutex_lock() call. */ \
      abort();                                               \
    }                                                        \
    sThreadFreezeCount--;                                    \
    sRecreateVIPCount--;                                     \
    pthread_cond_signal(&sThreadChangeCond);                 \
    pthread_mutex_unlock(&sThreadCountLock);                 \
  }

/**
 * Wrapping the blocking functions: epoll_wait(), poll(), pthread_mutex_lock(),
 * pthread_cond_wait() and pthread_cond_timedwait():
 *
 * These functions are wrapped by the above freeze point macros. Once a new
 * process is forked, the recreated thread will be blocked in one of the wrapper
 * functions. When recreating the thread, we longjmp() to
 * THREAD_FREEZE_POINT1() to recover the thread stack. Care must be taken to
 * maintain the semantics of the wrapped function:
 *
 * - epoll_wait() and poll(): just retry the function.
 * - pthread_mutex_lock(): don't lock if frozen at freeze point 2 (lock is
 *   already acquired).
 * - pthread_cond_wait() and pthread_cond_timedwait(): if the thread is frozen
 *   waiting the condition variable, the mutex is already released, we need to
 *   reacquire the mutex before calling the wrapped function again so the mutex
 *   will be in a valid state.
 */

extern "C" MFBT_API int
__wrap_epoll_wait(int epfd,
                  struct epoll_event *events,
                  int maxevents,
                  int timeout) {
  int rv;

  THREAD_FREEZE_POINT1();
  rv = REAL(epoll_wait)(epfd, events, maxevents, timeout);
  THREAD_FREEZE_POINT2();

  return rv;
}

extern "C" MFBT_API int
__wrap_pthread_cond_wait(pthread_cond_t *cond,
                         pthread_mutex_t *mtx) {
  int rv = 0;

  THREAD_FREEZE_POINT1_VIP();
  if (freezePoint2) {
    RECREATE_CONTINUE();
    RECREATE_PASS_VIP();
    RECREATE_GATE_VIP();
    return rv;
  }
  if (recreated && mtx) {
    if (!freezePoint1) {
      tinfo->condMutex = mtx;
      // The thread was frozen in pthread_cond_wait() after releasing mtx in the
      // Nuwa process. In recreating this thread, We failed to reacquire mtx
      // with the pthread_mutex_trylock() call, that is, mtx was acquired by
      // another thread. Because of this, we need the main thread's help to
      // reacquire mtx so that it will be in a valid state.
      if (!pthread_mutex_trylock(mtx)) {
        tinfo->condMutexNeedsBalancing = true;
      }
    }
    RECREATE_CONTINUE();
    RECREATE_PASS_VIP();
  }
  rv = REAL(pthread_cond_wait)(cond, mtx);
  if (recreated && mtx) {
    // We have reacquired mtx. The main thread also wants to acquire mtx to
    // synchronize with us. If the main thread didn't get a chance to acquire
    // mtx let it do that now. The main thread clears condMutex after acquiring
    // it to signal us.
    if (tinfo->condMutex) {
      // We need mtx to end up locked, so tell the main thread not to unlock
      // mtx after it locks it.
      tinfo->condMutexNeedsBalancing = false;
      pthread_mutex_unlock(mtx);
    }
    // We still need to be gated as not to acquire another mutex associated with
    // another VIP thread and interfere with it.
    RECREATE_GATE_VIP();
  }
  THREAD_FREEZE_POINT2_VIP();

  return rv;
}

extern "C" MFBT_API int
__wrap_pthread_cond_timedwait(pthread_cond_t *cond,
                              pthread_mutex_t *mtx,
                              const struct timespec *abstime) {
  int rv = 0;

  THREAD_FREEZE_POINT1_VIP();
  if (freezePoint2) {
    RECREATE_CONTINUE();
    RECREATE_PASS_VIP();
    RECREATE_GATE_VIP();
    return rv;
  }
  if (recreated && mtx) {
    if (!freezePoint1) {
      tinfo->condMutex = mtx;
      if (!pthread_mutex_trylock(mtx)) {
        tinfo->condMutexNeedsBalancing = true;
      }
    }
    RECREATE_CONTINUE();
    RECREATE_PASS_VIP();
  }
  rv = REAL(pthread_cond_timedwait)(cond, mtx, abstime);
  if (recreated && mtx) {
    if (tinfo->condMutex) {
      tinfo->condMutexNeedsBalancing = false;
      pthread_mutex_unlock(mtx);
    }
    RECREATE_GATE_VIP();
  }
  THREAD_FREEZE_POINT2_VIP();

  return rv;
}


extern "C" MFBT_API int
__wrap_pthread_mutex_trylock(pthread_mutex_t *mtx) {
  int rv = 0;

  THREAD_FREEZE_POINT1();
  if (freezePoint2) {
    return rv;
  }
  rv = REAL(pthread_mutex_trylock)(mtx);
  THREAD_FREEZE_POINT2();

  return rv;
}

extern "C" MFBT_API int
__wrap_pthread_mutex_lock(pthread_mutex_t *mtx) {
  int rv = 0;

  THREAD_FREEZE_POINT1();
  if (freezePoint2) {
    return rv;
  }
  rv = REAL(pthread_mutex_lock)(mtx);
  THREAD_FREEZE_POINT2();

  return rv;
}

extern "C" MFBT_API int
__wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  int rv;

  THREAD_FREEZE_POINT1();
  rv = REAL(poll)(fds, nfds, timeout);
  THREAD_FREEZE_POINT2();

  return rv;
}

extern "C" MFBT_API int
__wrap_epoll_create(int size) {
  int epollfd = REAL(epoll_create)(size);

  if (!sIsNuwaProcess) {
    return epollfd;
  }

  if (epollfd >= 0) {
    EpollManager::Singleton()->AddEpollInfo(epollfd, size);
  }

  return epollfd;
}

/**
 * Wrapping the functions to create file descriptor pairs. In the child process
 * FD pairs are created for intra-process signaling. The generation of FD pairs
 * need to be tracked in the nuwa process so they can be recreated in the
 * spawned process.
 */
struct FdPairInfo {
  enum {
    kPipe,
    kSocketpair
  } call;

  int FDs[2];
  int flags;
  int domain;
  int type;
  int protocol;
};

/**
 * Protects the access to sSingalFds.
 */
static pthread_mutex_t  sSignalFdLock = PTHREAD_MUTEX_INITIALIZER;
static std::vector<FdPairInfo> sSignalFds;

extern "C" MFBT_API int
__wrap_socketpair(int domain, int type, int protocol, int sv[2])
{
  int rv = REAL(socketpair)(domain, type, protocol, sv);

  if (!sIsNuwaProcess || rv < 0) {
    return rv;
  }

  REAL(pthread_mutex_lock)(&sSignalFdLock);
  FdPairInfo signalFd;
  signalFd.call = FdPairInfo::kSocketpair;
  signalFd.FDs[0] = sv[0];
  signalFd.FDs[1] = sv[1];
  signalFd.domain = domain;
  signalFd.type = type;
  signalFd.protocol = protocol;

  sSignalFds.push_back(signalFd);
  pthread_mutex_unlock(&sSignalFdLock);

  return rv;
}

extern "C" MFBT_API int
__wrap_pipe2(int __pipedes[2], int flags)
{
  int rv = REAL(pipe2)(__pipedes, flags);
  if (!sIsNuwaProcess || rv < 0) {
    return rv;
  }

  REAL(pthread_mutex_lock)(&sSignalFdLock);
  FdPairInfo signalFd;
  signalFd.call = FdPairInfo::kPipe;
  signalFd.FDs[0] = __pipedes[0];
  signalFd.FDs[1] = __pipedes[1];
  signalFd.flags = flags;
  sSignalFds.push_back(signalFd);
  pthread_mutex_unlock(&sSignalFdLock);
  return rv;
}

extern "C" MFBT_API int
__wrap_pipe(int __pipedes[2])
{
  return __wrap_pipe2(__pipedes, 0);
}

static void
DupeSingleFd(int newFd, int origFd)
{
  struct stat sb;
  if (fstat(origFd, &sb)) {
    // Maybe the original FD is closed.
    return;
  }
  int fd = fcntl(origFd, F_GETFD);
  int fl = fcntl(origFd, F_GETFL);
  dup2(newFd, origFd);
  fcntl(origFd, F_SETFD, fd);
  fcntl(origFd, F_SETFL, fl);
  REAL(close)(newFd);
}

extern "C" MFBT_API void
ReplaceSignalFds()
{
  for (std::vector<FdPairInfo>::iterator it = sSignalFds.begin();
       it < sSignalFds.end(); ++it) {
    int fds[2];
    int rc = 0;
    switch (it->call) {
    case FdPairInfo::kPipe:
      rc = REAL(pipe2)(fds, it->flags);
      break;
    case FdPairInfo::kSocketpair:
      rc = REAL(socketpair)(it->domain, it->type, it->protocol, fds);
      break;
    default:
      continue;
    }

    if (rc == 0) {
      DupeSingleFd(fds[0], it->FDs[0]);
      DupeSingleFd(fds[1], it->FDs[1]);
    }
  }
}

extern "C" MFBT_API int
__wrap_epoll_ctl(int aEpollFd, int aOp, int aFd, struct epoll_event *aEvent) {
  int rv = REAL(epoll_ctl)(aEpollFd, aOp, aFd, aEvent);

  if (!sIsNuwaProcess || rv == -1) {
    return rv;
  }

  EpollManager::EpollInfo *info =
    EpollManager::Singleton()->FindEpollInfo(aEpollFd);
  if (info == nullptr) {
    abort();
  }

  switch(aOp) {
  case EPOLL_CTL_ADD:
    info->AddEvents(aFd, *aEvent);
    break;

  case EPOLL_CTL_MOD:
    info->ModifyEvents(aFd, *aEvent);
    break;

  case EPOLL_CTL_DEL:
    info->RemoveEvents(aFd);
    break;

  default:
    abort();
  }

  return rv;
}

// XXX: thinker: Maybe, we should also track dup, dup2, and other functions.
extern "C" MFBT_API int
__wrap_close(int aFd) {
  int rv = REAL(close)(aFd);
  if (!sIsNuwaProcess || rv == -1) {
    return rv;
  }

  EpollManager::EpollInfo *info =
    EpollManager::Singleton()->FindEpollInfo(aFd);
  if (info) {
    EpollManager::Singleton()->RemoveEpollInfo(aFd);
  }

  return rv;
}

static void *
thread_recreate_startup(void *arg) {
  /*
   * Dark Art!! Never do the same unless you are ABSOLUTELY sure what you are
   * doing!
   *
   * The stack space collapsed by this frame had been reserved by
   * thread_create_startup().  And thread_create_startup() will
   * return immediately after returning from real start routine, so
   * all collapsed values does not affect the result.
   *
   * All outer frames of thread_create_startup() and
   * thread_recreate_startup() are equivalent, so
   * thread_create_startup() will return successfully.
   */
  thread_info_t *tinfo = (thread_info_t *)arg;

  prctl(PR_SET_NAME, (unsigned long)&tinfo->nativeThreadName, 0, 0, 0);
  RestoreTLSInfo(tinfo);

  if (setjmp(tinfo->retEnv) != 0) {
    POST_SETJMP_RESTORE("thread_recreate_startup");
    return nullptr;
  }

  // longjump() to recreate the stack on the new thread.
  longjmp(tinfo->jmpEnv, 1);

  // Never go here!
  abort();

  return nullptr;
}

/**
 * Recreate the context given by tinfo at a new thread.
 */
static void
thread_recreate(thread_info_t *tinfo) {
  pthread_t thread;

  // Note that the thread_recreate_startup() runs on the stack specified by
  // tinfo.
  pthread_create(&thread, &tinfo->threadAttr, thread_recreate_startup, tinfo);
}

/**
 * Recreate all threads in a process forked from an Nuwa process.
 */
static void
RecreateThreads() {
  sIsNuwaProcess = false;
  sIsFreezing = false;

  sMainThread.recreatedThreadID = pthread_self();
  sMainThread.recreatedNativeThreadID = gettid();

  // Run registered constructors.
  for (std::vector<nuwa_construct_t>::iterator ctr = sConstructors.begin();
       ctr != sConstructors.end();
       ctr++) {
    (*ctr).construct((*ctr).arg);
  }
  sConstructors.clear();

  REAL(pthread_mutex_lock)(&sThreadCountLock);
  thread_info_t *tinfo = sAllThreads.getFirst();
  pthread_mutex_unlock(&sThreadCountLock);

  RECREATE_START();
  while (tinfo != nullptr) {
    if (tinfo->flags & TINFO_FLAG_NUWA_SUPPORT) {
      RECREATE_BEFORE(tinfo);
      thread_recreate(tinfo);
      RECREATE_WAIT();
      if (tinfo->condMutex) {
        // Synchronize with the recreated thread in pthread_cond_wait().
        REAL(pthread_mutex_lock)(tinfo->condMutex);
        // Tell the other thread that we have successfully locked the mutex.
        // NB: condMutex can only be touched while it is held, so we must clear
        // it here and store the mutex locally.
        pthread_mutex_t *mtx = tinfo->condMutex;
        tinfo->condMutex = nullptr;
        if (tinfo->condMutexNeedsBalancing) {
          pthread_mutex_unlock(mtx);
        }
      }
    } else if(!(tinfo->flags & TINFO_FLAG_NUWA_SKIP)) {
      // An unmarked thread is found other than the main thread.

      // All threads should be marked as one of SUPPORT or SKIP, or
      // abort the process to make sure all threads in the Nuwa
      // process are Nuwa-aware.
      abort();
    }

    tinfo = tinfo->getNext();
  }
  RECREATE_WAIT_ALL_VIP();
  RECREATE_OPEN_GATE();

  RECREATE_FINISH();

  // Run registered final constructors.
  for (std::vector<nuwa_construct_t>::iterator ctr = sFinalConstructors.begin();
       ctr != sFinalConstructors.end();
       ctr++) {
    (*ctr).construct((*ctr).arg);
  }
  sFinalConstructors.clear();
}

extern "C" {

/**
 * Recreate all epoll fds and restore status; include all events.
 */
static void
RecreateEpollFds() {
  EpollManager *man = EpollManager::Singleton();

  for (EpollManager::const_iterator info_it = man->begin();
       info_it != man->end();
       info_it++) {
    int epollfd = info_it->first;
    const EpollManager::EpollInfo *info = &info_it->second;

    int fdflags = fcntl(epollfd, F_GETFD);
    if (fdflags == -1) {
      abort();
    }
    int fl = fcntl(epollfd, F_GETFL);
    if (fl == -1) {
      abort();
    }

    int newepollfd = REAL(epoll_create)(info->BackSize());
    if (newepollfd == -1) {
      abort();
    }
    int rv = REAL(close)(epollfd);
    if (rv == -1) {
      abort();
    }
    rv = dup2(newepollfd, epollfd);
    if (rv == -1) {
      abort();
    }
    rv = REAL(close)(newepollfd);
    if (rv == -1) {
      abort();
    }

    rv = fcntl(epollfd, F_SETFD, fdflags);
    if (rv == -1) {
      abort();
    }
    rv = fcntl(epollfd, F_SETFL, fl);
    if (rv == -1) {
      abort();
    }

    for (EpollManager::EpollInfo::const_iterator events_it = info->begin();
         events_it != info->end();
         events_it++) {
      int fd = events_it->first;
      epoll_event events;
      events = events_it->second;
      rv = REAL(epoll_ctl)(epollfd, EPOLL_CTL_ADD, fd, &events);
      if (rv == -1) {
        abort();
      }
    }
  }

  // Shutdown EpollManager. It won't be needed in the spawned process.
  EpollManager::Shutdown();
}

/**
 * Fix IPC to make it ready.
 *
 * Especially, fix ContentChild.
 */
static void
ReplaceIPC(NuwaProtoFdInfo *aInfoList, int aInfoSize) {
  int i;
  int rv;

  for (i = 0; i < aInfoSize; i++) {
    int fd = fcntl(aInfoList[i].originFd, F_GETFD);
    if (fd == -1) {
      abort();
    }

    int fl = fcntl(aInfoList[i].originFd, F_GETFL);
    if (fl == -1) {
      abort();
    }

    rv = dup2(aInfoList[i].newFds[NUWA_NEWFD_CHILD], aInfoList[i].originFd);
    if (rv == -1) {
      abort();
    }

    rv = fcntl(aInfoList[i].originFd, F_SETFD, fd);
    if (rv == -1) {
      abort();
    }

    rv = fcntl(aInfoList[i].originFd, F_SETFL, fl);
    if (rv == -1) {
      abort();
    }
  }
}

/**
 * Add a new content process at the chrome process.
 */
static void
AddNewProcess(pid_t pid, NuwaProtoFdInfo *aInfoList, int aInfoSize) {
  static bool (*AddNewIPCProcess)(pid_t, NuwaProtoFdInfo *, int) = nullptr;

  if (AddNewIPCProcess == nullptr) {
    AddNewIPCProcess = (bool (*)(pid_t, NuwaProtoFdInfo *, int))
      dlsym(RTLD_DEFAULT, "AddNewIPCProcess");
  }
  AddNewIPCProcess(pid, aInfoList, aInfoSize);
}

static void
PrepareProtoSockets(NuwaProtoFdInfo *aInfoList, int aInfoSize) {
  int i;
  int rv;

  for (i = 0; i < aInfoSize; i++) {
    rv = REAL(socketpair)(PF_UNIX, SOCK_STREAM, 0, aInfoList[i].newFds);
    if (rv == -1) {
      abort();
    }
  }
}

static void
CloseAllProtoSockets(NuwaProtoFdInfo *aInfoList, int aInfoSize) {
  int i;

  for (i = 0; i < aInfoSize; i++) {
    REAL(close)(aInfoList[i].newFds[0]);
    REAL(close)(aInfoList[i].newFds[1]);
  }
}

static void
AfterForkHook()
{
  void (*AfterNuwaFork)();

  // This is defined in dom/ipc/ContentChild.cpp
  AfterNuwaFork = (void (*)())
    dlsym(RTLD_DEFAULT, "AfterNuwaFork");
  AfterNuwaFork();
}

/**
 * Fork a new process that is ready for running IPC.
 *
 * @return the PID of the new process.
 */
static int
ForkIPCProcess() {
  int pid;

  REAL(pthread_mutex_lock)(&sForkLock);

  PrepareProtoSockets(sProtoFdInfos, sProtoFdInfosSize);

  sNuwaForking = true;
  pid = fork();
  sNuwaForking = false;
  if (pid == -1) {
    abort();
  }

  if (pid > 0) {
    // in the parent
    AddNewProcess(pid, sProtoFdInfos, sProtoFdInfosSize);
    CloseAllProtoSockets(sProtoFdInfos, sProtoFdInfosSize);
  } else {
    // in the child
    sIsNuwaChildProcess = true;
    if (getenv("MOZ_DEBUG_CHILD_PROCESS")) {
      printf("\n\nNUWA CHILDCHILDCHILDCHILD\n  debug me @ %d\n\n", getpid());
      sleep(30);
    }
    AfterForkHook();
    ReplaceSignalFds();
    ReplaceIPC(sProtoFdInfos, sProtoFdInfosSize);
    RecreateEpollFds();
    RecreateThreads();
    CloseAllProtoSockets(sProtoFdInfos, sProtoFdInfosSize);
  }

  sForkWaitCondChanged = true;
  pthread_cond_signal(&sForkWaitCond);
  pthread_mutex_unlock(&sForkLock);

  return pid;
}

/**
 * Prepare for spawning a new process. Called on the IPC thread.
 */
MFBT_API void
NuwaSpawnPrepare() {
  REAL(pthread_mutex_lock)(&sForkLock);

  sForkWaitCondChanged = false; // Will be modified on the main thread.
}

/**
 * Let IPC thread wait until fork action on the main thread has completed.
 */
MFBT_API void
NuwaSpawnWait() {
  while (!sForkWaitCondChanged) {
    REAL(pthread_cond_wait)(&sForkWaitCond, &sForkLock);
  }
  pthread_mutex_unlock(&sForkLock);
}

/**
 * Spawn a new process. If not ready for spawn (still waiting for some threads
 * to freeze), postpone the spawn request until ready.
 *
 * @return the pid of the new process, or 0 if not ready.
 */
MFBT_API pid_t
NuwaSpawn() {
  if (gettid() != getpid()) {
    // Not the main thread.
    abort();
  }

  pid_t pid = 0;

  if (sNuwaReady) {
    pid = ForkIPCProcess();
  } else {
    sNuwaPendingSpawn = true;
  }

  return pid;
}

/**
 * Prepare to freeze the Nuwa-supporting threads.
 */
MFBT_API void
PrepareNuwaProcess() {
  sIsNuwaProcess = true;
  // Explicitly ignore SIGCHLD so we don't have to call watpid() to reap
  // dead child processes.
  signal(SIGCHLD, SIG_IGN);

  // Make marked threads block in one freeze point.
  REAL(pthread_mutex_lock)(&sThreadFreezeLock);

  // Populate sMainThread for mapping of tgkill.
  sMainThread.origThreadID = pthread_self();
  sMainThread.origNativeThreadID = gettid();
}

// Make current process as a Nuwa process.
MFBT_API void
MakeNuwaProcess() {
  void (*GetProtoFdInfos)(NuwaProtoFdInfo *, int, int *) = nullptr;
  void (*OnNuwaProcessReady)() = nullptr;
  sIsFreezing = true;

  REAL(pthread_mutex_lock)(&sThreadCountLock);

  // wait until all threads are frozen.
  while ((sThreadFreezeCount + sThreadSkipCount) != sThreadCount) {
    REAL(pthread_cond_wait)(&sThreadChangeCond, &sThreadCountLock);
  }

  GetProtoFdInfos = (void (*)(NuwaProtoFdInfo *, int, int *))
    dlsym(RTLD_DEFAULT, "GetProtoFdInfos");
  GetProtoFdInfos(sProtoFdInfos, NUWA_TOPLEVEL_MAX, &sProtoFdInfosSize);

  sNuwaReady = true;

  pthread_mutex_unlock(&sThreadCountLock);

  OnNuwaProcessReady = (void (*)())dlsym(RTLD_DEFAULT, "OnNuwaProcessReady");
  OnNuwaProcessReady();

  if (sNuwaPendingSpawn) {
    sNuwaPendingSpawn = false;
    NuwaSpawn();
  }
}

/**
 * Mark the current thread as supporting Nuwa. The thread will be recreated in
 * the spawned process.
 */
MFBT_API void
NuwaMarkCurrentThread(void (*recreate)(void *), void *arg) {
  if (!sIsNuwaProcess) {
    return;
  }

  thread_info_t *tinfo = CUR_THREAD_INFO;
  if (tinfo == nullptr) {
    abort();
  }

  tinfo->flags |= TINFO_FLAG_NUWA_SUPPORT;
  if (recreate) {
    nuwa_construct_t construct(recreate, arg);
    tinfo->addThreadConstructor(&construct);
  }

  // XXX Thread name might be set later than this call. If this is the case, we
  // might need to delay getting the thread name.
  prctl(PR_GET_NAME, (unsigned long)&tinfo->nativeThreadName, 0, 0, 0);
}

/**
 * Mark the current thread as not supporting Nuwa. Don't recreate this thread in
 * the spawned process.
 */
MFBT_API void
NuwaSkipCurrentThread() {
  if (!sIsNuwaProcess) return;

  thread_info_t *tinfo = CUR_THREAD_INFO;
  if (tinfo == nullptr) {
    abort();
  }

  if (!(tinfo->flags & TINFO_FLAG_NUWA_SKIP)) {
    sThreadSkipCount++;
  }
  tinfo->flags |= TINFO_FLAG_NUWA_SKIP;
}

/**
 * Force to freeze the current thread.
 *
 * This method does not return in Nuwa process.  It returns for the
 * recreated thread.
 */
MFBT_API void
NuwaFreezeCurrentThread() {
  thread_info_t *tinfo = CUR_THREAD_INFO;
  if (sIsNuwaProcess &&
      (tinfo = CUR_THREAD_INFO) &&
      (tinfo->flags & TINFO_FLAG_NUWA_SUPPORT)) {
    if (!setjmp(tinfo->jmpEnv)) {
      REAL(pthread_mutex_lock)(&sThreadCountLock);
      SaveTLSInfo(tinfo);
      sThreadFreezeCount++;
      pthread_cond_signal(&sThreadChangeCond);
      pthread_mutex_unlock(&sThreadCountLock);

      REAL(pthread_mutex_lock)(&sThreadFreezeLock);
    } else {
      POST_SETJMP_RESTORE("NuwaFreezeCurrentThread");
      RECREATE_CONTINUE();
      RECREATE_GATE();
    }
  }
}

/**
 * The caller of NuwaCheckpointCurrentThread() is at the line it wishes to
 * return after the thread is recreated.
 *
 * The checkpointed thread will restart at the calling line of
 * NuwaCheckpointCurrentThread(). This macro returns true in the Nuwa process
 * and false on the recreated thread in the forked process.
 *
 * NuwaCheckpointCurrentThread() is implemented as a macro so we can place the
 * setjmp() call in the calling method without changing its stack pointer. This
 * is essential for not corrupting the stack when the calling thread continues
 * to request the main thread for forking a new process. The caller of
 * NuwaCheckpointCurrentThread() should not return before the process forking
 * finishes.
 *
 * @return true for Nuwa process, and false in the forked process.
 */
MFBT_API jmp_buf*
NuwaCheckpointCurrentThread1() {
  thread_info_t *tinfo = CUR_THREAD_INFO;
  if (sIsNuwaProcess &&
      (tinfo = CUR_THREAD_INFO) &&
      (tinfo->flags & TINFO_FLAG_NUWA_SUPPORT)) {
    return &tinfo->jmpEnv;
  }
  abort();
  return nullptr;
}

MFBT_API bool
NuwaCheckpointCurrentThread2(int setjmpCond) {
  thread_info_t *tinfo = CUR_THREAD_INFO;
  if (setjmpCond == 0) {
    REAL(pthread_mutex_lock)(&sThreadCountLock);
    if (!(tinfo->flags & TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT)) {
      tinfo->flags |= TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT;
      SaveTLSInfo(tinfo);
      sThreadFreezeCount++;
    }
    pthread_cond_signal(&sThreadChangeCond);
    pthread_mutex_unlock(&sThreadCountLock);
    return true;
  }
  POST_SETJMP_RESTORE("NuwaCheckpointCurrentThread2");
  RECREATE_CONTINUE();
  RECREATE_GATE();
  return false;               // Recreated thread.
}

/**
 * Register methods to be invoked before recreating threads in the spawned
 * process.
 */
MFBT_API void
NuwaAddConstructor(void (*construct)(void *), void *arg) {
  nuwa_construct_t ctr(construct, arg);
  sConstructors.push_back(ctr);
}

/**
 * Register methods to be invoked after recreating threads in the spawned
 * process.
 */
MFBT_API void
NuwaAddFinalConstructor(void (*construct)(void *), void *arg) {
  nuwa_construct_t ctr(construct, arg);
  sFinalConstructors.push_back(ctr);
}

MFBT_API void
NuwaAddThreadConstructor(void (*aConstruct)(void *), void *aArg) {
  thread_info *tinfo = CUR_THREAD_INFO;
  if (!tinfo || !aConstruct) {
    return;
  }

  nuwa_construct_t construct(aConstruct, aArg);
  tinfo->addThreadConstructor(&construct);
}

/**
 * @return if the current process is the nuwa process.
 */
MFBT_API bool
IsNuwaProcess() {
  return sIsNuwaProcess;
}

/**
 * @return if the nuwa process is ready for spawning new processes.
 */
MFBT_API bool
IsNuwaReady() {
  return sNuwaReady;
}

#if defined(DEBUG) || defined(ENABLE_TESTS)
MFBT_API void
NuwaAssertNotFrozen(unsigned int aThread, const char* aThreadName) {
  if (!sIsNuwaProcess || !sIsFreezing) {
    return;
  }

  thread_info_t *tinfo = GetThreadInfo(static_cast<pthread_t>(aThread));
  if (!tinfo) {
    return;
  }

  if ((tinfo->flags & TINFO_FLAG_NUWA_SUPPORT) &&
      !(tinfo->flags & TINFO_FLAG_NUWA_EXPLICIT_CHECKPOINT)) {
    __android_log_print(ANDROID_LOG_FATAL, "Nuwa",
                        "Fatal error: the Nuwa process is about to deadlock in "
                        "accessing a frozen thread (%s, tid=%d).",
                        aThreadName ? aThreadName : "(unnamed)",
                        tinfo->origNativeThreadID);
    abort();
  }
}
#endif

}      // extern "C"
