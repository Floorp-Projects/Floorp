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

#ifndef TOOLS_PLATFORM_H_
#define TOOLS_PLATFORM_H_

#ifdef ANDROID
#include <android/log.h>
#else
#define __android_log_print(a, ...)
#endif

#ifdef XP_UNIX
#include <pthread.h>
#endif

#include <stdint.h>
#include <math.h>
#include "MainThreadUtils.h"
#include "mozilla/Mutex.h"
#include "ThreadResponsiveness.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "PlatformMacros.h"
#include "v8-support.h"
#include <vector>
#include "StackTop.h"

// We need a definition of gettid(), but glibc doesn't provide a
// wrapper for it.
#if defined(__GLIBC__)
#include <unistd.h>
#include <sys/syscall.h>
static inline pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#elif defined(XP_MACOSX)
#include <unistd.h>
#include <sys/syscall.h>
static inline pid_t gettid()
{
  return (pid_t) syscall(SYS_thread_selfid);
}
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

bool profiler_verbose();

#ifdef ANDROID
# if defined(__arm__) || defined(__thumb__)
#  define ENABLE_LEAF_DATA
# endif
# define LOG(text) \
    do { if (profiler_verbose()) \
           __android_log_write(ANDROID_LOG_ERROR, "Profiler", text); \
    } while (0)
# define LOGF(format, ...) \
    do { if (profiler_verbose()) \
           __android_log_print(ANDROID_LOG_ERROR, "Profiler", format, \
                               __VA_ARGS__); \
    } while (0)

#else
# define LOG(text) \
    do { if (profiler_verbose()) fprintf(stderr, "Profiler: %s\n", text); \
    } while (0)
# define LOGF(format, ...) \
    do { if (profiler_verbose()) fprintf(stderr, "Profiler: " format "\n", \
                                         __VA_ARGS__); \
    } while (0)

#endif

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_LINUX)
#define ENABLE_LEAF_DATA
#endif

typedef int32_t Atomic32;

extern mozilla::TimeStamp sStartTime;

typedef uint8_t* Address;

// ----------------------------------------------------------------------------
// OS
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class OS {
public:
  // Sleep for a number of milliseconds.
  static void Sleep(const int milliseconds);

  // Sleep for a number of microseconds.
  static void SleepMicro(const int microseconds);

  // Called on startup to initialize platform specific things
  static void Startup();
};

// ----------------------------------------------------------------------------
// Thread
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class Thread {
public:
#ifdef XP_WIN
  typedef DWORD tid_t;
#else
  typedef ::pid_t tid_t;
#endif

  static tid_t GetCurrentId();
};

// ----------------------------------------------------------------------------
// HAVE_NATIVE_UNWIND
//
// Pseudo backtraces are available on all platforms.  Native
// backtraces are available only on selected platforms.  Breakpad is
// the only supported native unwinder.  HAVE_NATIVE_UNWIND is set at
// build time to indicate whether native unwinding is possible on this
// platform.

#undef HAVE_NATIVE_UNWIND
#if defined(MOZ_PROFILING) \
    && (defined(SPS_PLAT_amd64_linux) || defined(SPS_PLAT_arm_android) \
        || (defined(MOZ_WIDGET_ANDROID) && defined(__arm__)) \
        || defined(SPS_PLAT_x86_linux) \
        || defined(SPS_OS_windows) \
        || defined(SPS_OS_darwin))
# define HAVE_NATIVE_UNWIND
#endif

/* Some values extracted at startup from environment variables, that
   control the behaviour of the breakpad unwinder. */
extern const char* PROFILER_INTERVAL;
extern const char* PROFILER_ENTRIES;
extern const char* PROFILER_STACK;
extern const char* PROFILER_FEATURES;

void read_profiler_env_vars();
void profiler_usage();

// Helper methods to expose modifying profiler behavior
bool set_profiler_interval(const char*);
bool set_profiler_entries(const char*);
bool set_profiler_scan(const char*);
bool is_native_unwinding_avail();

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

class ThreadInfo;

// TickSample captures the information collected for each sample.
class TickSample {
 public:
  TickSample()
      : pc(NULL)
      , sp(NULL)
      , fp(NULL)
      , lr(NULL)
      , context(NULL)
      , isSamplingCurrentThread(false)
      , threadInfo(nullptr)
      , rssMemory(0)
      , ussMemory(0)
  {}

  void PopulateContext(void* aContext);

  Address pc;  // Instruction pointer.
  Address sp;  // Stack pointer.
  Address fp;  // Frame pointer.
  Address lr;  // ARM link register
  void*   context;   // The context from the signal handler, if available. On
                     // Win32 this may contain the windows thread context.
  bool    isSamplingCurrentThread;
  ThreadInfo* threadInfo;
  mozilla::TimeStamp timestamp;
  int64_t rssMemory;
  int64_t ussMemory;
};

struct JSContext;
class JSObject;
class PlatformData;
class ProfileBuffer;
struct PseudoStack;
class SpliceableJSONWriter;
class SyncProfile;

namespace mozilla {
class ProfileGatherer;

namespace dom {
class Promise;
}
}

typedef mozilla::Vector<std::string> ThreadNameFilterList;
typedef mozilla::Vector<std::string> FeatureList;

extern int sFrameNumber;
extern int sLastFrameNumber;

class Sampler {
public:
  // Initialize sampler.
  Sampler(double aInterval, int aEntrySize,
          const char** aFeatures, uint32_t aFeatureCount,
          const char** aThreadNameFilters, uint32_t aFilterCount);
  ~Sampler();

  double interval() const { return interval_; }

  // This method is called for each sampling period with the current
  // program counter. This function must be re-entrant.
  void Tick(TickSample* sample);

  // Immediately captures the calling thread's call stack and returns it.
  SyncProfile* GetBacktrace();

  // Delete markers which are no longer part of the profile due to buffer
  // wraparound.
  void DeleteExpiredMarkers();

  // Start and stop sampler.
  void Start();
  void Stop();

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return active_; }

  // Low overhead way to stop the sampler from ticking
  bool IsPaused() const { return paused_; }
  void SetPaused(bool value) { NoBarrier_Store(&paused_, value); }

  int EntrySize() { return entrySize_; }

  // We can't new/delete the type safely without defining it
  // (-Wdelete-incomplete).  Use these to hide the details from
  // clients.
  struct PlatformDataDestructor {
    void operator()(PlatformData*);
  };

  typedef mozilla::UniquePtr<PlatformData, PlatformDataDestructor>
    UniquePlatformData;
  static UniquePlatformData AllocPlatformData(int aThreadId);

  // If we move the backtracing code into the platform files we won't
  // need to have these hacks
#ifdef XP_WIN
  // xxxehsan sucky hack :(
  static uintptr_t GetThreadHandle(PlatformData*);
#endif

  static const std::vector<ThreadInfo*>& GetRegisteredThreads() {
    return *sRegisteredThreads;
  }

  static bool RegisterCurrentThread(const char* aName,
                                    PseudoStack* aPseudoStack,
                                    bool aIsMainThread, void* stackTop);
  static void UnregisterCurrentThread();

  static void Startup();
  // Should only be called on shutdown
  static void Shutdown();

  static mozilla::UniquePtr<mozilla::Mutex> sRegisteredThreadsMutex;

  static bool CanNotifyObservers() {
#ifdef MOZ_WIDGET_GONK
    // We use profile.sh on b2g to manually select threads and options per process.
    return false;
#elif defined(SPS_OS_android) && !defined(MOZ_WIDGET_GONK)
    // Android ANR reporter uses the profiler off the main thread
    return NS_IsMainThread();
#else
    MOZ_ASSERT(NS_IsMainThread());
    return true;
#endif
  }

  void RegisterThread(ThreadInfo* aInfo);

  bool ProfileJS() const { return mProfileJS; }
  bool ProfileJava() const { return mProfileJava; }
  bool ProfileGPU() const { return mProfileGPU; }
  bool ProfileThreads() const { return mProfileThreads; }
  bool InPrivacyMode() const { return mPrivacyMode; }
  bool AddMainThreadIO() const { return mAddMainThreadIO; }
  bool ProfileMemory() const { return mProfileMemory; }
  bool TaskTracer() const { return mTaskTracer; }
  bool LayersDump() const { return mLayersDump; }
  bool DisplayListDump() const { return mDisplayListDump; }
  bool ProfileRestyle() const { return mProfileRestyle; }
  const ThreadNameFilterList& ThreadNameFilters() { return mThreadNameFilters; }
  const FeatureList& Features() { return mFeatures; }

  void ToStreamAsJSON(std::ostream& stream, double aSinceTime = 0);
  JSObject *ToJSObject(JSContext *aCx, double aSinceTime = 0);
  void GetGatherer(nsISupports** aRetVal);
  mozilla::UniquePtr<char[]> ToJSON(double aSinceTime = 0);
  void ToJSObjectAsync(double aSinceTime = 0,
                       mozilla::dom::Promise* aPromise = 0);
  void ToFileAsync(const nsACString& aFileName, double aSinceTime = 0);
  void StreamMetaJSCustomObject(SpliceableJSONWriter& aWriter);
  void StreamTaskTracer(SpliceableJSONWriter& aWriter);
  void FlushOnJSShutdown(JSContext* aContext);

  void GetBufferInfo(uint32_t *aCurrentPosition, uint32_t *aTotalSize, uint32_t *aGeneration);

private:
  // Not implemented on platforms which do not support backtracing
  void doNativeBacktrace(ThreadInfo& aInfo, TickSample* aSample);

  void StreamJSON(SpliceableJSONWriter& aWriter, double aSinceTime);

  // Called within a signal. This function must be reentrant
  void InplaceTick(TickSample* sample);

  void SetActive(bool value) { NoBarrier_Store(&active_, value); }

  static std::vector<ThreadInfo*>* sRegisteredThreads;

  const double interval_;
  Atomic32 paused_;
  Atomic32 active_;
  const int entrySize_;

  // Refactor me!
#if defined(SPS_OS_linux) || defined(SPS_OS_android)
  bool signal_handler_installed_;
  struct sigaction old_sigprof_signal_handler_;
  struct sigaction old_sigsave_signal_handler_;
  bool signal_sender_launched_;
  pthread_t signal_sender_thread_;
#endif

  RefPtr<ProfileBuffer> mBuffer;
  bool mAddLeafAddresses;
  bool mUseStackWalk;
  bool mProfileJS;
  bool mProfileGPU;
  bool mProfileThreads;
  bool mProfileJava;
  bool mLayersDump;
  bool mDisplayListDump;
  bool mProfileRestyle;

  // Keep the thread filter to check against new thread that
  // are started while profiling
  ThreadNameFilterList mThreadNameFilters;
  FeatureList mFeatures;
  bool mPrivacyMode;
  bool mAddMainThreadIO;
  bool mProfileMemory;
  bool mTaskTracer;

  RefPtr<mozilla::ProfileGatherer> mGatherer;
};

#endif /* ndef TOOLS_PLATFORM_H_ */
