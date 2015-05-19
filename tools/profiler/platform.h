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
#include "mozilla/unused.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Mutex.h"
#include "PlatformMacros.h"
#include "ThreadResponsiveness.h"
#include "v8-support.h"
#include <vector>

#ifdef XP_WIN
#include <windows.h>
#endif

#define ASSERT(a) MOZ_ASSERT(a)

bool moz_profiler_verbose();

#ifdef ANDROID
# if defined(__arm__) || defined(__thumb__)
#  define ENABLE_SPS_LEAF_DATA
#  define ENABLE_ARM_LR_SAVING
# endif
# define LOG(text) \
    do { if (moz_profiler_verbose()) \
           __android_log_write(ANDROID_LOG_ERROR, "Profiler", text); \
    } while (0)
# define LOGF(format, ...) \
    do { if (moz_profiler_verbose()) \
           __android_log_print(ANDROID_LOG_ERROR, "Profiler", format, \
                               __VA_ARGS__); \
    } while (0)

#else
# define LOG(text) \
    do { if (moz_profiler_verbose()) fprintf(stderr, "Profiler: %s\n", text); \
    } while (0)
# define LOGF(format, ...) \
    do { if (moz_profiler_verbose()) fprintf(stderr, "Profiler: " format \
                                             "\n", __VA_ARGS__);        \
    } while (0)

#endif

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_LINUX)
#define ENABLE_SPS_LEAF_DATA
#endif

extern mozilla::TimeStamp sStartTime;

typedef uint8_t* Address;

// ----------------------------------------------------------------------------
// Mutex
//
// Mutexes are used for serializing access to non-reentrant sections of code.
// The implementations of mutex should allow for nested/recursive locking.

class Mutex {
 public:
  virtual ~Mutex() {}

  // Locks the given mutex. If the mutex is currently unlocked, it becomes
  // locked and owned by the calling thread, and immediately. If the mutex
  // is already locked by another thread, suspends the calling thread until
  // the mutex is unlocked.
  virtual int Lock() = 0;

  // Unlocks the given mutex. The mutex is assumed to be locked and owned by
  // the calling thread on entrance.
  virtual int Unlock() = 0;

  // Tries to lock the given mutex. Returns whether the mutex was
  // successfully locked.
  virtual bool TryLock() = 0;
};

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

 private:
  static const int msPerSecond = 1000;

};




// ----------------------------------------------------------------------------
// Thread
//
// Thread objects are used for creating and running threads. When the start()
// method is called the new thread starts running the run() method in the new
// thread. The Thread object should not be deallocated before the thread has
// terminated.

class Thread {
 public:
  // Create new thread.
  explicit Thread(const char* name);
  virtual ~Thread();

  // Start new thread by calling the Run() method in the new thread.
  void Start();

  void Join();

  inline const char* name() const {
    return name_;
  }

  // Abstract method for run handler.
  virtual void Run() = 0;

  // The thread name length is limited to 16 based on Linux's implementation of
  // prctl().
  static const int kMaxThreadNameLength = 16;

#ifdef XP_WIN
  HANDLE thread_;
  typedef DWORD tid_t;
  tid_t thread_id_;
#else
  typedef ::pid_t tid_t;
#endif
#if defined(XP_MACOSX)
  pthread_t thread_;
#endif

  static tid_t GetCurrentId();

 private:
  void set_name(const char *name);

  char name_[kMaxThreadNameLength];
  int stack_size_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
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

void set_tls_stack_top(void* stackTop);

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

struct PseudoStack;
class ThreadProfile;

// TickSample captures the information collected for each sample.
class TickSample {
 public:
  TickSample()
      :
        pc(NULL),
        sp(NULL),
        fp(NULL),
#ifdef ENABLE_ARM_LR_SAVING
        lr(NULL),
#endif
        context(NULL),
        isSamplingCurrentThread(false),
        threadProfile(nullptr),
        rssMemory(0),
        ussMemory(0) {}

  void PopulateContext(void* aContext);

  Address pc;  // Instruction pointer.
  Address sp;  // Stack pointer.
  Address fp;  // Frame pointer.
#ifdef ENABLE_ARM_LR_SAVING
  Address lr;  // ARM link register
#endif
  void*   context;   // The context from the signal handler, if available. On
                     // Win32 this may contain the windows thread context.
  bool    isSamplingCurrentThread;
  ThreadProfile* threadProfile;
  mozilla::TimeStamp timestamp;
  int64_t rssMemory;
  int64_t ussMemory;
};

class ThreadInfo;
class PlatformData;
class TableTicker;
class SyncProfile;
class Sampler {
 public:
  // Initialize sampler.
  explicit Sampler(double interval, bool profiling, int entrySize);
  virtual ~Sampler();

  double interval() const { return interval_; }

  // This method is called for each sampling period with the current
  // program counter.
  virtual void Tick(TickSample* sample) = 0;

  // Immediately captures the calling thread's call stack and returns it.
  virtual SyncProfile* GetBacktrace() = 0;

  // Request a save from a signal handler
  virtual void RequestSave() = 0;
  // Process any outstanding request outside a signal handler.
  virtual void HandleSaveRequest() = 0;
  // Delete markers which are no longer part of the profile due to buffer wraparound.
  virtual void DeleteExpiredMarkers() = 0;

  // Start and stop sampler.
  void Start();
  void Stop();

  // Is the sampler used for profiling?
  bool IsProfiling() const { return profiling_; }

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return active_; }

  // Low overhead way to stop the sampler from ticking
  bool IsPaused() const { return paused_; }
  void SetPaused(bool value) { NoBarrier_Store(&paused_, value); }

  virtual bool ProfileThreads() const = 0;

  int EntrySize() { return entrySize_; }

  // We can't new/delete the type safely without defining it
  // (-Wdelete-incomplete). Use these Alloc/Free functions instead.
  static PlatformData* AllocPlatformData(int aThreadId);
  static void FreePlatformData(PlatformData*);

  // If we move the backtracing code into the platform files we won't
  // need to have these hacks
#ifdef XP_WIN
  // xxxehsan sucky hack :(
  static uintptr_t GetThreadHandle(PlatformData*);
#endif
#ifdef XP_MACOSX
  static pthread_t GetProfiledThread(PlatformData*);
#endif

  static std::vector<ThreadInfo*> GetRegisteredThreads() {
    return *sRegisteredThreads;
  }

  static bool RegisterCurrentThread(const char* aName,
                                    PseudoStack* aPseudoStack,
                                    bool aIsMainThread, void* stackTop);
  static void UnregisterCurrentThread();

  static void Startup();
  // Should only be called on shutdown
  static void Shutdown();

  static TableTicker* GetActiveSampler() { return sActiveSampler; }
  static void SetActiveSampler(TableTicker* sampler) { sActiveSampler = sampler; }

  static mozilla::Mutex* sRegisteredThreadsMutex;

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

 protected:
  static std::vector<ThreadInfo*>* sRegisteredThreads;
  static TableTicker* sActiveSampler;

 private:
  void SetActive(bool value) { NoBarrier_Store(&active_, value); }

  const double interval_;
  const bool profiling_;
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
};

class ThreadInfo {
 public:
  ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread, PseudoStack* aPseudoStack, void* aStackTop);

  virtual ~ThreadInfo();

  const char* Name() const { return mName; }
  int ThreadId() const { return mThreadId; }

  bool IsMainThread() const { return mIsMainThread; }
  PseudoStack* Stack() const { return mPseudoStack; }

  void SetProfile(ThreadProfile* aProfile) { mProfile = aProfile; }
  ThreadProfile* Profile() const { return mProfile; }

  PlatformData* GetPlatformData() const { return mPlatformData; }
  void* StackTop() const { return mStackTop; }

  virtual void SetPendingDelete();
  bool IsPendingDelete() const { return mPendingDelete; }

#ifdef MOZ_NUWA_PROCESS
  void SetThreadId(int aThreadId) { mThreadId = aThreadId; }
#endif

  /**
   * May be null for the main thread if the profiler was started during startup
   */
  nsIThread* GetThread() const { return mThread.get(); }
 private:
  char* mName;
  int mThreadId;
  const bool mIsMainThread;
  PseudoStack* mPseudoStack;
  PlatformData* mPlatformData;
  ThreadProfile* mProfile;
  void* const mStackTop;
  nsCOMPtr<nsIThread> mThread;
  bool mPendingDelete;
};

// Just like ThreadInfo, but owns a reference to the PseudoStack.
class StackOwningThreadInfo : public ThreadInfo {
 public:
  StackOwningThreadInfo(const char* aName, int aThreadId, bool aIsMainThread, PseudoStack* aPseudoStack, void* aStackTop);
  virtual ~StackOwningThreadInfo();

  virtual void SetPendingDelete();
};

#endif /* ndef TOOLS_PLATFORM_H_ */
