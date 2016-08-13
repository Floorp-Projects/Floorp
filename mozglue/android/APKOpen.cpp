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
#include <sys/resource.h>
#include <sys/prctl.h>
#include "sqlite3.h"
#include "SQLiteBridge.h"
#include "NSSBridge.h"
#include "ElfLoader.h"
#include "application.ini.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "XREChildData.h"

/* Android headers don't define RUSAGE_THREAD */
#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD 1
#endif

#ifndef RELEASE_BUILD
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

extern "C" {
/*
 * To work around http://code.google.com/p/android/issues/detail?id=23203
 * we don't link with the crt objects. In some configurations, this means
 * a lack of the __dso_handle symbol because it is defined there, and
 * depending on the android platform and ndk versions used, it may or may
 * not be defined in libc.so. In the latter case, we fail to link. Defining
 * it here as weak makes us provide the symbol when it's not provided by
 * the crt objects, making the change transparent for future NDKs that
 * would fix the original problem. On older NDKs, it is not a problem
 * either because the way __dso_handle was used was already broken (and
 * the custom linker works around it).
 */
  NS_EXPORT __attribute__((weak)) void *__dso_handle;
}

typedef int mozglueresult;

enum StartupEvent {
#define mozilla_StartupTimeline_Event(ev, z) ev,
#include "StartupTimeline.h"
#undef mozilla_StartupTimeline_Event
  MAX_STARTUP_EVENT_ID
};

using namespace mozilla;

static struct mapping_info * lib_mapping = nullptr;

NS_EXPORT const struct mapping_info *
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
    pthread_t sJavaUiThread;
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

NS_EXPORT pthread_t
getJavaUiThread()
{
    return sJavaUiThread;
}

extern "C" NS_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_GeckoThread_registerUiThread(JNIEnv*, jclass)
{
    sJavaUiThread = pthread_self();
}

#define JNI_STUBS
#include "jni-stubs.inc"
#undef JNI_STUBS

static void * xul_handle = nullptr;
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

template <typename T> inline void
xul_dlsym(const char *symbolName, T *value)
{
  *value = (T) (uintptr_t) __wrap_dlsym(xul_handle, symbolName);
}

static int mapping_count = 0;

#define MAX_MAPPING_INFO 32

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

static void*
dlopenAPKLibrary(const char* apkName, const char* libraryName)
{
#define APK_ASSETS_PATH "!/assets/" ANDROID_CPU_ARCH "/"
  size_t filenameLength = strlen(apkName) +
    sizeof(APK_ASSETS_PATH) + 	// includes \0 terminator
    strlen(libraryName);
  auto file = MakeUnique<char[]>(filenameLength);
  snprintf(file.get(), filenameLength, "%s" APK_ASSETS_PATH "%s",
	   apkName, libraryName);
  return __wrap_dlopen(file.get(), RTLD_GLOBAL | RTLD_LAZY);
#undef APK_ASSETS_PATH
}
static mozglueresult
loadGeckoLibs(const char *apkName)
{
  TimeStamp t0 = TimeStamp::Now();
  struct rusage usage1_thread, usage1;
  getrusage(RUSAGE_THREAD, &usage1_thread);
  getrusage(RUSAGE_SELF, &usage1);

  xul_handle = dlopenAPKLibrary(apkName, "libxul.so");
  if (!xul_handle) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libxul!");
    return FAILURE;
  }

#define JNI_BINDINGS
#include "jni-stubs.inc"
#undef JNI_BINDINGS

  void (*XRE_StartupTimelineRecord)(int, TimeStamp);
  xul_dlsym("XRE_StartupTimelineRecord", &XRE_StartupTimelineRecord);

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

  XRE_StartupTimelineRecord(LINKER_INITIALIZED, t0);
  XRE_StartupTimelineRecord(LIBRARIES_LOADED, t1);
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
  if (!lib_mapping) {
    lib_mapping = (struct mapping_info *)calloc(MAX_MAPPING_INFO, sizeof(*lib_mapping));
  }

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

  if (!lib_mapping) {
    lib_mapping = (struct mapping_info *)calloc(MAX_MAPPING_INFO, sizeof(*lib_mapping));
  }

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

extern "C" NS_EXPORT void MOZ_JNICALL
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

extern "C" NS_EXPORT void MOZ_JNICALL
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

extern "C" NS_EXPORT void MOZ_JNICALL
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

typedef void (*GeckoStart_t)(JNIEnv*, char*, const nsXREAppData*);

extern "C" NS_EXPORT void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_nativeRun(JNIEnv *jenv, jclass jc, jstring jargs)
{
  GeckoStart_t GeckoStart;
  xul_dlsym("GeckoStart", &GeckoStart);
  if (GeckoStart == nullptr)
    return;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  int len = jenv->GetStringUTFLength(jargs);
  // GeckoStart needs to write in the args buffer, so we need a copy.
  char *args = (char *) malloc(len + 1);
  jenv->GetStringUTFRegion(jargs, 0, len, args);
  args[len] = '\0';
  ElfLoader::Singleton.ExpectShutdown(false);
  GeckoStart(jenv, args, &sAppData);
  ElfLoader::Singleton.ExpectShutdown(true);
  free(args);
}

typedef int GeckoProcessType;

extern "C" NS_EXPORT mozglueresult
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

  void (*fXRE_SetProcessType)(char*);
  xul_dlsym("XRE_SetProcessType", &fXRE_SetProcessType);

  mozglueresult (*fXRE_InitChildProcess)(int, char**, void*);
  xul_dlsym("XRE_InitChildProcess", &fXRE_InitChildProcess);

  fXRE_SetProcessType(argv[--argc]);

  XREChildData childData;
  return fXRE_InitChildProcess(argc, argv, &childData);
}

