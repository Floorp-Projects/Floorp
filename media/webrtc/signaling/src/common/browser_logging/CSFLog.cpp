/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "CSFLog.h"
#include "base/basictypes.h"

#include <map>
#include "cpr_threads.h"
#include "prrwlock.h"
#include "prthread.h"
#include "nsThreadUtils.h"
#ifndef WIN32
#include <pthread.h>
#endif
#ifdef OS_MACOSX
#include <dlfcn.h>
#endif
#ifdef OS_LINUX
#include <sys/prctl.h>
#endif

static PRLogModuleInfo *gLogModuleInfo = NULL;

PRLogModuleInfo *GetSignalingLogInfo()
{
  if (gLogModuleInfo == NULL)
    gLogModuleInfo = PR_NewLogModule("signaling");

  return gLogModuleInfo;
}

static PRLogModuleInfo *gWebRTCLogModuleInfo = NULL;
int gWebrtcTraceLoggingOn = 0;

PRLogModuleInfo *GetWebRTCLogInfo()
{
  if (gWebRTCLogModuleInfo == NULL)
    gWebRTCLogModuleInfo = PR_NewLogModule("webrtc_trace");

  return gWebRTCLogModuleInfo;
}

extern "C" {
  void CSFLogRegisterThread(const cprThread_t thread);
  void CSFLogUnregisterThread(const cprThread_t thread);
#ifndef WIN32
  pthread_t cprGetThreadId(cprThread_t thread);
#endif
}

#ifdef WIN32
typedef unsigned int thread_key_t;
#else
typedef pthread_t thread_key_t;
#endif
static PRRWLock *maplock = PR_NewRWLock(0,"thread map");
typedef std::map<thread_key_t,const cpr_thread_t*> threadMap_t;
static threadMap_t threadMap;

void CSFLogRegisterThread(const cprThread_t thread) {
  const cpr_thread_t *t = reinterpret_cast<cpr_thread_t *>(thread);
  thread_key_t key;
#ifdef WIN32
  key = t->threadId;
#else
  key = cprGetThreadId(thread);
#endif

  CSFLog(CSF_LOG_DEBUG, __FILE__, __LINE__, "log",
         "Registering new thread with logging system: %s", t->name);
  PR_RWLock_Wlock(maplock);
  threadMap[key] = t;
  PR_RWLock_Unlock(maplock);
}

void CSFLogUnregisterThread(const cprThread_t thread) {
  const cpr_thread_t *t = reinterpret_cast<cpr_thread_t *>(thread);
  thread_key_t key;
#ifdef WIN32
  key = t->threadId;
#else
  key = cprGetThreadId(thread);
#endif
  CSFLog(CSF_LOG_DEBUG, __FILE__, __LINE__, "log",
         "Unregistering thread from logging system: %s", t->name);
  PR_RWLock_Wlock(maplock);
  threadMap.erase(key);
  PR_RWLock_Unlock(maplock);
}

const char *CSFCurrentThreadName() {
  const char *name = nullptr;
#ifdef WIN32
  thread_key_t key = GetCurrentThreadId();
#else
  thread_key_t key = pthread_self();
#endif
  PR_RWLock_Rlock(maplock);
  threadMap_t::iterator i = threadMap.find(key);
  if (i != threadMap.end()) {
    name = i->second->name;
  }
  PR_RWLock_Unlock(maplock);
  return name;
}

#ifdef OS_MACOSX
// pthread_getname_np isn't available on all versions of OS X, so
// we need to load it in dynamically and check for its presence
static int (*dynamic_pthread_getname_np)(pthread_t,char*,size_t);
bool init_pthread_getname() {
  *reinterpret_cast<void**>(&dynamic_pthread_getname_np) =
      dlsym(RTLD_DEFAULT, "pthread_getname_np");
  return dynamic_pthread_getname_np;
}
static bool have_pthread_getname_np = init_pthread_getname();
#endif

void CSFLogV(CSFLogLevel priority, const char* sourceFile, int sourceLine, const char* tag , const char* format, va_list args)
{
#ifdef STDOUT_LOGGING
  printf("%s\n:",tag);
  vprintf(format, args);
#else

  PRLogModuleLevel level = static_cast<PRLogModuleLevel>(priority);

  GetSignalingLogInfo();

  // Skip doing any of this work if we're not logging the indicated level...
  if (!PR_LOG_TEST(gLogModuleInfo,level)) {
    return;
  }

  // Trim the path component from the filename
  const char *lastSlash = sourceFile;
  while (*sourceFile) {
    if (*sourceFile == '/' || *sourceFile == '\\') {
      lastSlash = sourceFile;
    }
    sourceFile++;
  }
  sourceFile = lastSlash;
  if (*sourceFile == '/' || *sourceFile == '\\') {
    sourceFile++;
  }

#define MAX_MESSAGE_LENGTH 1024
  char message[MAX_MESSAGE_LENGTH];
  char buffer[64] = "";

  const char *threadName = CSFCurrentThreadName();

  // Check if we're the main thread...
  if (!threadName && NS_IsMainThread()) {
    threadName = "main";
  }

  // If null, the name wasn't set up by CPR -- try NSPR
  if (!threadName) {
    threadName = PR_GetThreadName(PR_GetCurrentThread());
  }

  // If not NSPR, it might be from some other imported library that uses
  // one of the variety of non-portable means of naming threads.
#ifdef OS_LINUX
  if (!threadName &&
    !prctl(PR_GET_NAME,reinterpret_cast<uintptr_t>(buffer),0,0,0)) {
    buffer[16]='\0';
    if (buffer[0] != '\0') {
      threadName = buffer;
    }
  }
#endif
#ifdef OS_MACOSX
  if (!threadName && have_pthread_getname_np) {
    dynamic_pthread_getname_np(pthread_self(), buffer, sizeof(buffer));
    if (buffer[0] != '\0') {
      threadName = buffer;
    }
  }
#endif

  // If we can't find it anywhere, use a blank string
  if (!threadName) {
    threadName = "";
  }

  vsnprintf(message, MAX_MESSAGE_LENGTH, format, args);
  PR_LOG(gLogModuleInfo, level, ("[%s|%s] %s:%d: %s",
                                  threadName, tag, sourceFile, sourceLine,
                                  message));
#endif

}

void CSFLog( CSFLogLevel priority, const char* sourceFile, int sourceLine, const char* tag , const char* format, ...)
{
	va_list ap;
  va_start(ap, format);

  CSFLogV(priority, sourceFile, sourceLine, tag, format, ap);
  va_end(ap);
}

