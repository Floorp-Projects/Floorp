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

#include <stdint.h>
#include <math.h>
#include "MainThreadUtils.h"
#include "mozilla/StaticMutex.h"
#include "ThreadResponsiveness.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "PlatformMacros.h"
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
#elif defined(GP_OS_darwin)
#include <unistd.h>
#include <sys/syscall.h>
static inline pid_t gettid()
{
  return (pid_t) syscall(SYS_thread_selfid);
}
#endif

#if defined(GP_OS_windows)
#include <windows.h>
#endif

bool profiler_verbose();

#if defined(GP_OS_android)
# include <android/log.h>
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

#if defined(GP_OS_android) && !defined(MOZ_WIDGET_GONK)
#define PROFILE_JAVA
#endif

typedef uint8_t* Address;

// ----------------------------------------------------------------------------
// Thread
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class Thread {
public:
#if defined(GP_OS_windows)
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
#if defined(MOZ_PROFILING) && \
    (defined(GP_OS_windows) || \
     defined(GP_OS_darwin) || \
     defined(GP_OS_linux) || \
     defined(GP_PLAT_arm_android))
# define HAVE_NATIVE_UNWIND
#endif

// ----------------------------------------------------------------------------
// ProfilerState auxiliaries

// There is a single instance of ProfilerState that holds the profiler's global
// state. Both the class and the instance are defined in platform.cpp.
// ProfilerStateMutex is the type of the mutex that guards all accesses to
// ProfilerState. We use a distinct type for it to make it hard to mix up with
// any other StaticMutex.
class ProfilerStateMutex : public mozilla::StaticMutex {};

// Values of this type are passed around as a kind of token indicating which
// functions are called while the global ProfilerStateMutex is held, i.e. while
// the ProfilerState can be modified. (All of ProfilerState's getters and
// setters require this token.) Some such functions are outside platform.cpp,
// so this type must be declared here.
typedef const mozilla::BaseAutoLock<ProfilerStateMutex>& PSLockRef;

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

mozilla::UniquePtr<char[]>
ToJSON(PSLockRef aLock, double aSinceTime);

#endif /* ndef TOOLS_PLATFORM_H_ */
