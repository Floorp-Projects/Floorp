/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This custom library loading code is only meant to be called
 * during initialization. As a result, it takes no special
 * precautions to be threadsafe. Any of the library loading functions
 * like mozload should not be available to other code.
 */

#include <jni.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include "dlfcn.h"
#include "APKOpen.h"
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include "sqlite3.h"
#include "SQLiteBridge.h"
#include "NSSBridge.h"
#include "ElfLoader.h"
#include "application.ini.h"

#include "mozilla/arm.h"
#include "mozilla/Bootstrap.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "XREChildData.h"

/* Android headers don't define RUSAGE_THREAD */
#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD 1
#endif

#ifndef RELEASE_OR_BETA
/* Official builds have the debuggable flag set to false, which disables
 * the backtrace dumper from bionic. However, as it is useful for native
 * crashes happening before the crash reporter is registered, re-enable
 * it on non release builds (i.e. nightly and aurora).
 * Using a constructor so that it is re-enabled as soon as libmozglue.so
 * is loaded.
 */
__attribute__((constructor))
void make_dumpable() {
  prctl(PR_SET_DUMPABLE, 1);
}
#endif

typedef int mozglueresult;

enum StartupEvent {
#define mozilla_StartupTimeline_Event(ev, z) ev,
#include "StartupTimeline.h"
#undef mozilla_StartupTimeline_Event
  MAX_STARTUP_EVENT_ID
};

using namespace mozilla;

static const int MAX_MAPPING_INFO = 32;
static mapping_info lib_mapping[MAX_MAPPING_INFO];

APKOPEN_EXPORT const struct mapping_info *
getLibraryMapping()
{
  return lib_mapping;
}

void
JNI_Throw(JNIEnv* jenv, const char* classname, const char* msg)
{
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Throw\n");
    jclass cls = jenv->FindClass(classname);
    if (cls == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't find exception class (or exception pending) %s\n", classname);
        exit(FAILURE);
    }
    int rc = jenv->ThrowNew(cls, msg);
    if (rc < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Error throwing exception %s\n", msg);
        exit(FAILURE);
    }
    jenv->DeleteLocalRef(cls);
}

namespace {
    JavaVM* sJavaVM;
}

void
abortThroughJava(const char* msg)
{
    struct sigaction sigact = {};
    if (SEGVHandler::__wrap_sigaction(SIGSEGV, nullptr, &sigact)) {
        return; // sigaction call failed.
    }

    Dl_info info = {};
    if ((sigact.sa_flags & SA_SIGINFO) &&
        __wrap_dladdr(reinterpret_cast<void*>(sigact.sa_sigaction), &info) &&
        info.dli_fname && strstr(info.dli_fname, "libxul.so")) {

        return; // Existing signal handler is in libxul (i.e. we have crash reporter).
    }

    JNIEnv* env = nullptr;
    if (!sJavaVM || sJavaVM->AttachCurrentThreadAsDaemon(&env, nullptr) != JNI_OK) {
        return;
    }

    if (!env || env->PushLocalFrame(2) != JNI_OK) {
        return;
    }

    jclass loader = env->FindClass("org/mozilla/gecko/mozglue/GeckoLoader");
    if (!loader) {
        return;
    }

    jmethodID method = env->GetStaticMethodID(loader, "abort", "(Ljava/lang/String;)V");
    jstring str = env->NewStringUTF(msg);

    if (method && str) {
        env->CallStaticVoidMethod(loader, method, str);
    }

    env->PopLocalFrame(nullptr);
}

Bootstrap::UniquePtr gBootstrap;
#ifndef MOZ_FOLD_LIBS
static void * sqlite_handle = nullptr;
static void * nspr_handle = nullptr;
static void * plc_handle = nullptr;
#else
#define sqlite_handle nss_handle
#define nspr_handle nss_handle
#define plc_handle nss_handle
#endif
static void * nss_handle = nullptr;

static int mapping_count = 0;

extern "C" void
report_mapping(char *name, void *base, uint32_t len, uint32_t offset)
{
  if (mapping_count >= MAX_MAPPING_INFO)
    return;

  struct mapping_info *info = &lib_mapping[mapping_count++];
  info->name = strdup(name);
  info->base = (uintptr_t)base;
  info->len = len;
  info->offset = offset;
}

extern "C" void
delete_mapping(const char *name)
{
  for (int pos = 0; pos < mapping_count; ++pos) {
    struct mapping_info *info = &lib_mapping[pos];
    if (!strcmp(info->name, name)) {
      struct mapping_info *last = &lib_mapping[mapping_count - 1];
      free(info->name);
      *info = *last;
      --mapping_count;
      break;
    }
  }
}

static UniquePtr<char[]>
getAPKLibraryName(const char* apkName, const char* libraryName)
{
#define APK_ASSETS_PATH "!/assets/" ANDROID_CPU_ARCH "/"
  size_t filenameLength = strlen(apkName) +
    sizeof(APK_ASSETS_PATH) + 	// includes \0 terminator
    strlen(libraryName);
  auto file = MakeUnique<char[]>(filenameLength);
  snprintf(file.get(), filenameLength, "%s" APK_ASSETS_PATH "%s",
	   apkName, libraryName);
  return file;
#undef APK_ASSETS_PATH
}

static void*
dlopenAPKLibrary(const char* apkName, const char* libraryName)
{
  return __wrap_dlopen(getAPKLibraryName(apkName, libraryName).get(), RTLD_GLOBAL | RTLD_LAZY);
}

static mozglueresult
loadGeckoLibs(const char *apkName)
{
  TimeStamp t0 = TimeStamp::Now();
  struct rusage usage1_thread, usage1;
  getrusage(RUSAGE_THREAD, &usage1_thread);
  getrusage(RUSAGE_SELF, &usage1);

  gBootstrap = GetBootstrap(getAPKLibraryName(apkName, "libxul.so").get());
  if (!gBootstrap) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libxul!");
    return FAILURE;
  }

  TimeStamp t1 = TimeStamp::Now();
  struct rusage usage2_thread, usage2;
  getrusage(RUSAGE_THREAD, &usage2_thread);
  getrusage(RUSAGE_SELF, &usage2);

#define RUSAGE_TIMEDIFF(u1, u2, field) \
  ((u2.ru_ ## field.tv_sec - u1.ru_ ## field.tv_sec) * 1000 + \
   (u2.ru_ ## field.tv_usec - u1.ru_ ## field.tv_usec) / 1000)

  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Loaded libs in %fms total, %ldms(%ldms) user, %ldms(%ldms) system, %ld(%ld) faults",
                      (t1 - t0).ToMilliseconds(),
                      RUSAGE_TIMEDIFF(usage1_thread, usage2_thread, utime),
                      RUSAGE_TIMEDIFF(usage1, usage2, utime),
                      RUSAGE_TIMEDIFF(usage1_thread, usage2_thread, stime),
                      RUSAGE_TIMEDIFF(usage1, usage2, stime),
                      usage2_thread.ru_majflt - usage1_thread.ru_majflt,
                      usage2.ru_majflt - usage1.ru_majflt);

  gBootstrap->XRE_StartupTimelineRecord(LINKER_INITIALIZED, t0);
  gBootstrap->XRE_StartupTimelineRecord(LIBRARIES_LOADED, t1);
  return SUCCESS;
}

static mozglueresult loadNSSLibs(const char *apkName);

static mozglueresult
loadSQLiteLibs(const char *apkName)
{
  if (sqlite_handle)
    return SUCCESS;

#ifdef MOZ_FOLD_LIBS
  if (loadNSSLibs(apkName) != SUCCESS)
    return FAILURE;
#else

  sqlite_handle = dlopenAPKLibrary(apkName, "libmozsqlite3.so");
  if (!sqlite_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libmozsqlite3!");
    return FAILURE;
  }
#endif

  setup_sqlite_functions(sqlite_handle);
  return SUCCESS;
}

static mozglueresult
loadNSSLibs(const char *apkName)
{
  if (nss_handle && nspr_handle && plc_handle)
    return SUCCESS;

  nss_handle = dlopenAPKLibrary(apkName, "libnss3.so");

#ifndef MOZ_FOLD_LIBS
  nspr_handle = dlopenAPKLibrary(apkName, "libnspr4.so");

  plc_handle = dlopenAPKLibrary(apkName, "libplc4.so");
#endif

  if (!nss_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libnss3!");
    return FAILURE;
  }

#ifndef MOZ_FOLD_LIBS
  if (!nspr_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libnspr4!");
    return FAILURE;
  }

  if (!plc_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libplc4!");
    return FAILURE;
  }
#endif

  return setup_nss_functions(nss_handle, nspr_handle, plc_handle);
}

extern "C" APKOPEN_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_loadGeckoLibsNative(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName)
{
  jenv->GetJavaVM(&sJavaVM);

  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, nullptr);
  if (str == nullptr)
    return;

  int res = loadGeckoLibs(str);
  if (res != SUCCESS) {
    JNI_Throw(jenv, "java/lang/Exception", "Error loading gecko libraries");
  }
  jenv->ReleaseStringUTFChars(jApkName, str);
}

extern "C" APKOPEN_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_loadSQLiteLibsNative(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName) {
  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, nullptr);
  if (str == nullptr)
    return;

  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load sqlite start\n");
  mozglueresult rv = loadSQLiteLibs(str);
  if (rv != SUCCESS) {
      JNI_Throw(jenv, "java/lang/Exception", "Error loading sqlite libraries");
  }
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load sqlite done\n");
  jenv->ReleaseStringUTFChars(jApkName, str);
}

extern "C" APKOPEN_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_loadNSSLibsNative(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName) {
  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, nullptr);
  if (str == nullptr)
    return;

  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load nss start\n");
  mozglueresult rv = loadNSSLibs(str);
  if (rv != SUCCESS) {
    JNI_Throw(jenv, "java/lang/Exception", "Error loading nss libraries");
  }
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Load nss done\n");
  jenv->ReleaseStringUTFChars(jApkName, str);
}

static char**
CreateArgvFromObjectArray(JNIEnv *jenv, jobjectArray jargs, int* length)
{
  size_t stringCount = jenv->GetArrayLength(jargs);

  if (length) {
    *length = stringCount;
  }

  if (!stringCount) {
    return nullptr;
  }

  char** argv = new char*[stringCount + 1];

  argv[stringCount] = nullptr;

  for (size_t ix = 0; ix < stringCount; ix++) {
    jstring string = (jstring) (jenv->GetObjectArrayElement(jargs, ix));
    const char* rawString = jenv->GetStringUTFChars(string, nullptr);
    const int strLength = jenv->GetStringUTFLength(string);
    argv[ix] = strndup(rawString, strLength);
    jenv->ReleaseStringUTFChars(string, rawString);
    jenv->DeleteLocalRef(string);
  }

  return argv;
}

static void
FreeArgv(char** argv, int argc)
{
  for (int ix=0; ix < argc; ix++) {
    // String was allocated with strndup, so need to use free to deallocate.
    free(argv[ix]);
  }
  delete[](argv);
}

extern "C" APKOPEN_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_nativeRun(JNIEnv *jenv, jclass jc, jobjectArray jargs, int prefsFd, int ipcFd, int crashFd, int crashAnnotationFd)
{
  int argc = 0;
  char** argv = CreateArgvFromObjectArray(jenv, jargs, &argc);

  if (ipcFd < 0) {
    if (gBootstrap == nullptr) {
      FreeArgv(argv, argc);
      return;
    }

    ElfLoader::Singleton.ExpectShutdown(false);
    gBootstrap->GeckoStart(jenv, argv, argc, sAppData);
    ElfLoader::Singleton.ExpectShutdown(true);
  } else {
    gBootstrap->XRE_SetAndroidChildFds(jenv, prefsFd, ipcFd, crashFd, crashAnnotationFd);
    gBootstrap->XRE_SetProcessType(argv[argc - 1]);

    XREChildData childData;
    gBootstrap->XRE_InitChildProcess(argc - 1, argv, &childData);
  }

  gBootstrap.reset();
  FreeArgv(argv, argc);
}

extern "C" APKOPEN_EXPORT mozglueresult
ChildProcessInit(int argc, char* argv[])
{
  int i;
  for (i = 0; i < (argc - 1); i++) {
    if (strcmp(argv[i], "-greomni"))
      continue;

    i = i + 1;
    break;
  }

  if (loadNSSLibs(argv[i]) != SUCCESS) {
    return FAILURE;
  }
  if (loadSQLiteLibs(argv[i]) != SUCCESS) {
    return FAILURE;
  }
  if (loadGeckoLibs(argv[i]) != SUCCESS) {
    return FAILURE;
  }

  gBootstrap->XRE_SetProcessType(argv[--argc]);

  XREChildData childData;
  return NS_FAILED(gBootstrap->XRE_InitChildProcess(argc, argv, &childData));
}

extern "C" APKOPEN_EXPORT jboolean MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_neonCompatible(JNIEnv *jenv, jclass jc)
{
#ifdef __ARM_EABI__
  return mozilla::supports_neon();
#else
  return true;
#endif // __ARM_EABI__
}

// Does current process name end with ':media'?
static bool
IsMediaProcess()
{
  pid_t pid = getpid();
  char str[256];
  SprintfLiteral(str, "/proc/%d/cmdline", pid);
  FILE* f = fopen(str, "r");
  if (f) {
    fgets(str, sizeof(str), f);
    fclose(f);
    const size_t strLen = strlen(str);
    const char suffix[] = ":media";
    const size_t suffixLen = sizeof(suffix) - 1;
    if (strLen >= suffixLen &&
        !strncmp(str + strLen - suffixLen, suffix, suffixLen)) {
      return true;
    }
  }
  return false;
}

#ifndef SYS_rt_tgsigqueueinfo
#define SYS_rt_tgsigqueueinfo __NR_rt_tgsigqueueinfo
#endif
/* Copy of http://androidxref.com/7.1.1_r6/xref/bionic/linker/debugger.cpp#262,
 * with debuggerd related code stripped.
 *
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
static void
CatchFatalSignals(int num, siginfo_t *info, void *context)
{
  // It's possible somebody cleared the SA_SIGINFO flag, which would mean
  // our "info" arg holds an undefined value.
  struct sigaction action = {};
  if ((sigaction(num, nullptr, &action) < 0) ||
      !(action.sa_flags & SA_SIGINFO)) {
    info = nullptr;
  }

  // We need to return from the signal handler so that debuggerd can dump the
  // thread that crashed, but returning here does not guarantee that the signal
  // will be thrown again, even for SIGSEGV and friends, since the signal could
  // have been sent manually. Resend the signal with rt_tgsigqueueinfo(2) to
  // preserve the SA_SIGINFO contents.
  signal(num, SIG_DFL);

  struct siginfo si;
  if (!info) {
    memset(&si, 0, sizeof(si));
    si.si_code = SI_USER;
    si.si_pid = getpid();
    si.si_uid = getuid();
    info = &si;
  } else if (info->si_code >= 0 || info->si_code == SI_TKILL) {
    // rt_tgsigqueueinfo(2)'s documentation appears to be incorrect on kernels
    // that contain commit 66dd34a (3.9+). The manpage claims to only allow
    // negative si_code values that are not SI_TKILL, but 66dd34a changed the
    // check to allow all si_code values in calls coming from inside the house.
  }

  int rc = syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), num, info);
  if (rc != 0) {
    __android_log_print(ANDROID_LOG_FATAL, "mozglue",
                        "failed to resend signal during crash: %s",
                        strerror(errno));
    _exit(0);
  }
}

extern "C" APKOPEN_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_suppressCrashDialog(JNIEnv *jenv, jclass jc)
{
  MOZ_RELEASE_ASSERT(IsMediaProcess(), "Suppress crash dialog only for media process");
  // Restoring to SIG_DFL will crash on x86/Android M devices (see bug 1374556)
  // so copy Android code (http://androidxref.com/7.1.1_r6/xref/bionic/linker/debugger.cpp#302).
  // See comments above CatchFatalSignals() for copyright notice.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = &CatchFatalSignals;
  action.sa_flags = SA_RESTART | SA_SIGINFO;

  // Use the alternate signal stack if available so we can catch stack overflows.
  action.sa_flags |= SA_ONSTACK;

  sigaction(SIGABRT, &action, nullptr);
  sigaction(SIGBUS, &action, nullptr);
  sigaction(SIGFPE, &action, nullptr);
  sigaction(SIGILL, &action, nullptr);
  sigaction(SIGSEGV, &action, nullptr);
#if defined(SIGSTKFLT)
  sigaction(SIGSTKFLT, &action, nullptr);
#endif
  sigaction(SIGTRAP, &action, nullptr);
}
