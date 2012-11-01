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

#include <vector>

#define NS_EXPORT __attribute__ ((visibility("default")))

/* Android doesn't have pthread_atfork(), so we need to use our own. */
struct AtForkFuncs {
  void (*prepare)(void);
  void (*parent)(void);
  void (*child)(void);
};
static std::vector<AtForkFuncs> atfork;

#ifdef MOZ_WIDGET_GONK
#include "cpuacct.h"
#define WRAP(x) x

extern "C" NS_EXPORT int
timer_create(clockid_t, struct sigevent*, timer_t*)
{
  __android_log_print(ANDROID_LOG_ERROR, "BionicGlue", "timer_create not supported!");
  abort();
  return -1;
}

#else
#define cpuacct_add(x)
#define WRAP(x) __wrap_##x
#endif

extern "C" NS_EXPORT int
WRAP(pthread_atfork)(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
  AtForkFuncs funcs;
  funcs.prepare = prepare;
  funcs.parent = parent;
  funcs.child = child;
  atfork.push_back(funcs);
  return 0;
}

extern "C" pid_t __fork(void);

extern "C" NS_EXPORT pid_t
WRAP(fork)(void)
{
  pid_t pid;
  for (std::vector<AtForkFuncs>::reverse_iterator it = atfork.rbegin();
       it < atfork.rend(); ++it)
    if (it->prepare)
      it->prepare();

  switch ((pid = __fork())) {
  case 0:
    cpuacct_add(getuid());
    for (std::vector<AtForkFuncs>::iterator it = atfork.begin();
         it < atfork.end(); ++it)
      if (it->child)
        it->child();
    break;
  default:
    for (std::vector<AtForkFuncs>::iterator it = atfork.begin();
         it < atfork.end(); ++it)
      if (it->parent)
        it->parent();
  }
  return pid;
}

extern "C" NS_EXPORT int
WRAP(raise)(int sig)
{
  return pthread_kill(pthread_self(), sig);
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
