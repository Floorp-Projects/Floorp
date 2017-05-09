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
#include "ThreadResponsiveness.h"
#include "mozilla/Logging.h"
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
#elif defined(GP_OS_android)
#include <unistd.h>
#elif defined(GP_OS_windows)
#include <windows.h>
#include <process.h>
#ifndef getpid
#define getpid _getpid
#endif
#endif

extern mozilla::LazyLogModule gProfilerLog;

// These are for MOZ_LOG="prof:3" or higher. It's the default logging level for
// the profiler, and should be used sparingly.
#define LOG_TEST \
  MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Info)
#define LOG(arg, ...) \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Info, \
          ("[%d] " arg, getpid(), ##__VA_ARGS__))

// These are for MOZ_LOG="prof:4" or higher. It should be used for logging that
// is somewhat more verbose than LOG.
#define DEBUG_LOG_TEST \
  MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Debug)
#define DEBUG_LOG(arg, ...) \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Debug, \
          ("[%d] " arg, getpid(), ##__VA_ARGS__))

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

namespace mozilla {
class JSONWriter;
}
void AppendSharedLibraries(mozilla::JSONWriter& aWriter);

#endif /* ndef TOOLS_PLATFORM_H_ */
