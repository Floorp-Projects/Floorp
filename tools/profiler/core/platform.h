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
#include "mozilla/StaticMutex.h"
#include "ThreadResponsiveness.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
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
// Miscellaneous

class PlatformData;

// We can't new/delete the type safely without defining it
// (-Wdelete-incomplete).  Use these to hide the details from clients.
struct PlatformDataDestructor {
  void operator()(PlatformData*);
};

typedef mozilla::UniquePtr<PlatformData, PlatformDataDestructor>
  UniquePlatformData;
UniquePlatformData AllocPlatformData(int aThreadId);

mozilla::UniquePtr<char[]> ToJSON(double aSinceTime);

#endif /* ndef TOOLS_PLATFORM_H_ */
