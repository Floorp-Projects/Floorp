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

/* Amazon Kindle Fire HD's libc provides most of the
 * functions in string.h as weak symbols, which dlsym
 * cannot resolve. Thus, we must wrap these functions.
 * See bug 791419.
 */

#ifndef MOZ_WIDGET_GONK
#include <string.h>
extern "C" NS_EXPORT void* __wrap_memccpy(void * a0, const void * a1, int a2, size_t a3) { return memccpy(a0, a1, a2, a3); }
extern "C" NS_EXPORT void* __wrap_memchr(const void * a0, int a1, size_t a2) { return memchr(a0, a1, a2); }
extern "C" NS_EXPORT void* __wrap_memrchr(const void * a0, int a1, size_t a2) { return memrchr(a0, a1, a2); }
extern "C" NS_EXPORT int __wrap_memcmp(const void * a0, const void * a1, size_t a2) { return memcmp(a0, a1, a2); }
extern "C" NS_EXPORT void* __wrap_memcpy(void * a0, const void * a1, size_t a2) { return memcpy(a0, a1, a2); }
extern "C" NS_EXPORT void* __wrap_memmove(void * a0, const void * a1, size_t a2) { return memmove(a0, a1, a2); }
extern "C" NS_EXPORT void* __wrap_memset(void * a0, int a1, size_t a2) { return memset(a0, a1, a2); }
extern "C" NS_EXPORT void* __wrap_memmem(const void * a0, size_t a1, const void * a2, size_t a3) { return memmem(a0, a1, a2, a3); }
extern "C" NS_EXPORT void __wrap_memswap(void * a0, void * a1, size_t a2) { memswap(a0, a1, a2); }
extern "C" NS_EXPORT char* __wrap_index(const char * a0, int a1) { return index(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strchr(const char * a0, int a1) { return strchr(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strrchr(const char * a0, int a1) { return strrchr(a0, a1); }
extern "C" NS_EXPORT size_t __wrap_strlen(const char * a0) { return strlen(a0); }
extern "C" NS_EXPORT int __wrap_strcmp(const char * a0, const char * a1) { return strcmp(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strcpy(char * a0, const char * a1) { return strcpy(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strcat(char * a0, const char * a1) { return strcat(a0, a1); }
extern "C" NS_EXPORT int __wrap_strcasecmp(const char * a0, const char * a1) { return strcasecmp(a0, a1); }
extern "C" NS_EXPORT int __wrap_strncasecmp(const char * a0, const char * a1, size_t a2) { return strncasecmp(a0, a1, a2); }
extern "C" NS_EXPORT char* __wrap_strstr(const char * a0, const char * a1) { return strstr(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strcasestr(const char * a0, const char * a1) { return strcasestr(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strtok(char * a0, const char * a1) { return strtok(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strtok_r(char * a0, const char * a1, char** a2) { return strtok_r(a0, a1, a2); }
extern "C" NS_EXPORT char* __wrap_strerror(int a0) { return strerror(a0); }
extern "C" NS_EXPORT int __wrap_strerror_r(int a0, char * a1, size_t a2) { return strerror_r(a0, a1, a2); }
extern "C" NS_EXPORT size_t __wrap_strnlen(const char * a0, size_t a1) { return strnlen(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strncat(char * a0, const char * a1, size_t a2) { return strncat(a0, a1, a2); }
extern "C" NS_EXPORT int __wrap_strncmp(const char * a0, const char * a1, size_t a2) { return strncmp(a0, a1, a2); }
extern "C" NS_EXPORT char* __wrap_strncpy(char * a0, const char * a1, size_t a2) { return strncpy(a0, a1, a2); }
extern "C" NS_EXPORT size_t __wrap_strlcat(char * a0, const char * a1, size_t a2) { return strlcat(a0, a1, a2); }
extern "C" NS_EXPORT size_t __wrap_strlcpy(char * a0, const char * a1, size_t a2) { return strlcpy(a0, a1, a2); }
extern "C" NS_EXPORT size_t __wrap_strcspn(const char * a0, const char * a1) { return strcspn(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strpbrk(const char * a0, const char * a1) { return strpbrk(a0, a1); }
extern "C" NS_EXPORT char* __wrap_strsep(char ** a0, const char * a1) { return strsep(a0, a1); }
extern "C" NS_EXPORT size_t __wrap_strspn(const char * a0, const char * a1) { return strspn(a0, a1); }
extern "C" NS_EXPORT int __wrap_strcoll(const char * a0, const char * a1) { return strcoll(a0, a1); }
extern "C" NS_EXPORT size_t __wrap_strxfrm(char * a0, const char * a1, size_t a2) { return strxfrm(a0, a1, a2); }
#endif
