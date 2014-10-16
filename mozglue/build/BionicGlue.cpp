/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <android/log.h>
#include <sys/syscall.h>

#include "mozilla/Alignment.h"

#include <vector>

#define NS_EXPORT __attribute__ ((visibility("default")))

#if ANDROID_VERSION < 17 || defined(MOZ_WIDGET_ANDROID)
/* Android doesn't have pthread_atfork(), so we need to use our own. */
struct AtForkFuncs {
  void (*prepare)(void);
  void (*parent)(void);
  void (*child)(void);
};

/* jemalloc's initialization calls pthread_atfork. When pthread_atfork (see
 * further below) stores the corresponding data, it's going to allocate memory,
 * which will loop back to jemalloc's initialization, leading to a dead-lock.
 * So, for that specific vector, we use a special allocator that returns a
 * static buffer for small sizes, and force the initial vector capacity to
 * a size enough to store one atfork function table. */
template <typename T>
struct SpecialAllocator: public std::allocator<T>
{
  SpecialAllocator(): bufUsed(false) {}

  inline typename std::allocator<T>::pointer allocate(typename std::allocator<T>::size_type n, const void * = 0) {
    if (!bufUsed && n == 1) {
      bufUsed = true;
      return buf.addr();
    }
    return reinterpret_cast<T *>(::operator new(sizeof(T) * n));
  }

  inline void deallocate(typename std::allocator<T>::pointer p, typename std::allocator<T>::size_type n) {
    if (p == buf.addr())
      bufUsed = false;
    else
      ::operator delete(p);
  }

  template<typename U>
  struct rebind {
    typedef SpecialAllocator<U> other;
  };

private:
  mozilla::AlignedStorage2<T> buf;
  bool bufUsed;
};

static std::vector<AtForkFuncs, SpecialAllocator<AtForkFuncs> > atfork;
#endif

#ifdef MOZ_WIDGET_GONK
#include "cpuacct.h"

#if ANDROID_VERSION < 17 || defined(MOZ_WIDGET_ANDROID)
extern "C" NS_EXPORT int
timer_create(clockid_t, struct sigevent*, timer_t*)
{
  __android_log_print(ANDROID_LOG_ERROR, "BionicGlue", "timer_create not supported!");
  abort();
  return -1;
}
#endif

#else
#define cpuacct_add(x)
#endif

#if ANDROID_VERSION < 17 || defined(MOZ_WIDGET_ANDROID)
extern "C" NS_EXPORT int
pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
  AtForkFuncs funcs;
  funcs.prepare = prepare;
  funcs.parent = parent;
  funcs.child = child;
  if (!atfork.capacity())
    atfork.reserve(1);
  atfork.push_back(funcs);
  return 0;
}

extern "C" NS_EXPORT pid_t __fork(void);

extern "C" NS_EXPORT pid_t
fork(void)
{
  pid_t pid;
  for (auto it = atfork.rbegin();
       it < atfork.rend(); ++it)
    if (it->prepare)
      it->prepare();

  switch ((pid = syscall(__NR_clone, SIGCHLD, NULL, NULL, NULL, NULL))) {
  case 0:
    cpuacct_add(getuid());
    for (auto it = atfork.begin();
         it < atfork.end(); ++it)
      if (it->child)
        it->child();
    break;
  default:
    for (auto it = atfork.begin();
         it < atfork.end(); ++it)
      if (it->parent)
        it->parent();
  }
  return pid;
}
#endif

extern "C" NS_EXPORT int
raise(int sig)
{
  // Bug 741272: Bionic incorrectly uses kill(), which signals the
  // process, and thus could signal another thread (and let this one
  // return "successfully" from raising a fatal signal).
  //
  // Bug 943170: POSIX specifies pthread_kill(pthread_self(), sig) as
  // equivalent to raise(sig), but Bionic also has a bug with these
  // functions, where a forked child will kill its parent instead.

  extern pid_t gettid(void);
  return syscall(__NR_tgkill, getpid(), gettid(), sig);
}

/*
 * The following wrappers for PR_Xxx are needed until we can get
 * PR_DuplicateEnvironment landed in NSPR.
 * See see bug 772734 and bug 773414.
 *
 * We can't #include the pr headers here, and we can't call any of the
 * PR/PL functions either, so we just reimplemnt using native code.
 */

static pthread_mutex_t  _pr_envLock = PTHREAD_MUTEX_INITIALIZER;

extern "C" NS_EXPORT char*
__wrap_PR_GetEnv(const char *var)
{
    char *ev;

    pthread_mutex_lock(&_pr_envLock);
    ev = getenv(var);
    pthread_mutex_unlock(&_pr_envLock);
    return ev;
}

extern "C" NS_EXPORT int
__wrap_PR_SetEnv(const char *string)
{
    int result;

    if ( !strchr(string, '=')) return(-1);

    pthread_mutex_lock(&_pr_envLock);
    result = putenv(string);
    pthread_mutex_unlock(&_pr_envLock);
    return (result)? -1 : 0;
}

extern "C" NS_EXPORT pthread_mutex_t *
PR_GetEnvLock(void)
{
  return &_pr_envLock;
}

/* Flash plugin uses symbols that are not present in Android >= 4.4 */
#ifndef MOZ_WIDGET_GONK
namespace android {
  namespace VectorImpl {
    NS_EXPORT void reservedVectorImpl1(void) { }
    NS_EXPORT void reservedVectorImpl2(void) { }
    NS_EXPORT void reservedVectorImpl3(void) { }
    NS_EXPORT void reservedVectorImpl4(void) { }
    NS_EXPORT void reservedVectorImpl5(void) { }
    NS_EXPORT void reservedVectorImpl6(void) { }
    NS_EXPORT void reservedVectorImpl7(void) { }
    NS_EXPORT void reservedVectorImpl8(void) { }
  }
}
#endif

