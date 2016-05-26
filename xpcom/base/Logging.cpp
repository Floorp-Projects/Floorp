/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Snprintf.h"
#include "nsClassHashtable.h"
#include "nsDebug.h"
#include "NSPRLogModulesParser.h"

#include "prenv.h"
#include "prprf.h"
#ifdef XP_WIN
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

// NB: Initial amount determined by auditing the codebase for the total amount
//     of unique module names and padding up to the next power of 2.
const uint32_t kInitialModuleCount = 256;

namespace mozilla {

namespace detail {

void log_print(const PRLogModuleInfo* aModule,
               LogLevel aLevel,
               const char* aFmt, ...)
{
  va_list ap;
  va_start(ap, aFmt);
  char* buff = PR_vsmprintf(aFmt, ap);
  PR_LogPrint("%s", buff);
  PR_smprintf_free(buff);
  va_end(ap);
}

void log_print(const LogModule* aModule,
               LogLevel aLevel,
               const char* aFmt, ...)
{
  va_list ap;
  va_start(ap, aFmt);
  aModule->Printv(aLevel, aFmt, ap);
  va_end(ap);
}

int log_pid()
{
#ifdef XP_WIN
  return _getpid();
#else
  return getpid();
#endif
}

}

LogLevel
ToLogLevel(int32_t aLevel)
{
  aLevel = std::min(aLevel, static_cast<int32_t>(LogLevel::Verbose));
  aLevel = std::max(aLevel, static_cast<int32_t>(LogLevel::Disabled));
  return static_cast<LogLevel>(aLevel);
}

const char*
ToLogStr(LogLevel aLevel) {
  switch (aLevel) {
    case LogLevel::Error:
      return "E";
    case LogLevel::Warning:
      return "W";
    case LogLevel::Info:
      return "I";
    case LogLevel::Debug:
      return "D";
    case LogLevel::Verbose:
      return "V";
    case LogLevel::Disabled:
    default:
      MOZ_CRASH("Invalid log level.");
      return "";
  }
}

class LogModuleManager
{
public:
  LogModuleManager()
    : mModulesLock("logmodules")
    , mModules(kInitialModuleCount)
    , mOutFile(nullptr)
    , mMainThread(PR_GetCurrentThread())
    , mAddTimestamp(false)
    , mIsSync(false)
  {
  }

  ~LogModuleManager()
  {
    // NB: mModules owns all of the log modules, they will get destroyed by
    //     its destructor.
  }

  /**
   * Loads config from env vars if present.
   */
  void Init()
  {
    bool shouldAppend = false;
    bool addTimestamp = false;
    bool isSync = false;
    const char* modules = PR_GetEnv("MOZ_LOG");
    if (!modules || !modules[0]) {
      modules = PR_GetEnv("MOZ_LOG_MODULES");
      if (modules) {
        NS_WARNING("MOZ_LOG_MODULES is deprecated."
            "\nPlease use MOZ_LOG instead.");
      }
    }
    if (!modules || !modules[0]) {
      modules = PR_GetEnv("NSPR_LOG_MODULES");
      if (modules) {
        NS_WARNING("NSPR_LOG_MODULES is deprecated."
            "\nPlease use MOZ_LOG instead.");
      }
    }

    NSPRLogModulesParser(modules,
        [&shouldAppend, &addTimestamp, &isSync]
            (const char* aName, LogLevel aLevel) mutable {
          if (strcmp(aName, "append") == 0) {
            shouldAppend = true;
          } else if (strcmp(aName, "timestamp") == 0) {
            addTimestamp = true;
          } else if (strcmp(aName, "sync") == 0) {
            isSync = true;
          } else {
            LogModule::Get(aName)->SetLevel(aLevel);
          }
    });

    mAddTimestamp = addTimestamp;
    mIsSync = isSync;

    const char* logFile = PR_GetEnv("MOZ_LOG_FILE");
    if (!logFile || !logFile[0]) {
      logFile = PR_GetEnv("NSPR_LOG_FILE");
    }

    if (logFile && logFile[0]) {
      static const char kPIDToken[] = "%PID";
      const char* pidTokenPtr = strstr(logFile, kPIDToken);
      char buf[2048];
      if (pidTokenPtr &&
          snprintf_literal(buf, "%.*s%d%s",
            static_cast<int>(pidTokenPtr - logFile), logFile,
            detail::log_pid(),
            pidTokenPtr + strlen(kPIDToken)) > 0)
      {
        logFile = buf;
      }

      mOutFile = fopen(logFile, shouldAppend ? "a" : "w");
    }
  }

  LogModule* CreateOrGetModule(const char* aName)
  {
    OffTheBooksMutexAutoLock guard(mModulesLock);
    LogModule* module = nullptr;
    if (!mModules.Get(aName, &module)) {
      module = new LogModule(aName, LogLevel::Disabled);
      mModules.Put(aName, module);
    }

    return module;
  }

  void Print(const char* aName, LogLevel aLevel, const char* aFmt, va_list aArgs)
  {
    const size_t kBuffSize = 1024;
    char buff[kBuffSize];

    char* buffToWrite = buff;

    // For backwards compat we need to use the NSPR format string versions
    // of sprintf and friends and then hand off to printf.
    va_list argsCopy;
    va_copy(argsCopy, aArgs);
    size_t charsWritten = PR_vsnprintf(buff, kBuffSize, aFmt, argsCopy);
    va_end(argsCopy);

    if (charsWritten == kBuffSize - 1) {
      // We may have maxed out, allocate a buffer instead.
      buffToWrite = PR_vsmprintf(aFmt, aArgs);
      charsWritten = strlen(buffToWrite);
    }

    // Determine if a newline needs to be appended to the message.
    const char* newline = "";
    if (charsWritten == 0 || buffToWrite[charsWritten - 1] != '\n') {
      newline = "\n";
    }

    FILE* out = mOutFile ? mOutFile : stderr;

    // This differs from the NSPR format in that we do not output the
    // opaque system specific thread pointer (ie pthread_t) cast
    // to a long. The address of the current PR_Thread continues to be
    // prefixed.
    //
    // Additionally we prefix the output with the abbreviated log level
    // and the module name.
    PRThread *currentThread = PR_GetCurrentThread();
    const char *currentThreadName = (mMainThread == currentThread)
      ? "Main Thread"
      : PR_GetThreadName(currentThread);

    char noNameThread[40];
    if (!currentThreadName) {
      snprintf_literal(noNameThread, "Unnamed thread %p", currentThread);
      currentThreadName = noNameThread;
    }

    if (!mAddTimestamp) {
      fprintf_stderr(out,
                     "[%s]: %s/%s %s%s",
                     currentThreadName, ToLogStr(aLevel),
                     aName, buffToWrite, newline);
    } else {
      PRExplodedTime now;
      PR_ExplodeTime(PR_Now(), PR_GMTParameters, &now);
      fprintf_stderr(
          out,
          "%04d-%02d-%02d %02d:%02d:%02d.%06d UTC - [%s]: %s/%s %s%s",
          now.tm_year, now.tm_month + 1, now.tm_mday,
          now.tm_hour, now.tm_min, now.tm_sec, now.tm_usec,
          currentThreadName, ToLogStr(aLevel),
          aName, buffToWrite, newline);
    }

    if (mIsSync) {
      fflush(out);
    }

    if (buffToWrite != buff) {
      PR_smprintf_free(buffToWrite);
    }
  }

private:
  OffTheBooksMutex mModulesLock;
  nsClassHashtable<nsCharPtrHashKey, LogModule> mModules;
  ScopedCloseFile mOutFile;
  PRThread *mMainThread;
  bool mAddTimestamp;
  bool mIsSync;
};

StaticAutoPtr<LogModuleManager> sLogModuleManager;

LogModule*
LogModule::Get(const char* aName)
{
  // This is just a pass through to the LogModuleManager so
  // that the LogModuleManager implementation can be kept internal.
  MOZ_ASSERT(sLogModuleManager != nullptr);
  return sLogModuleManager->CreateOrGetModule(aName);
}

void
LogModule::Init()
{
  // NB: This method is not threadsafe; it is expected to be called very early
  //     in startup prior to any other threads being run.
  if (sLogModuleManager) {
    // Already initialized.
    return;
  }

  // NB: We intentionally do not register for ClearOnShutdown as that happens
  //     before all logging is complete. And, yes, that means we leak, but
  //     we're doing that intentionally.
  sLogModuleManager = new LogModuleManager();
  sLogModuleManager->Init();
}

void
LogModule::Printv(LogLevel aLevel, const char* aFmt, va_list aArgs) const
{
  MOZ_ASSERT(sLogModuleManager != nullptr);

  // Forward to LogModule manager w/ level and name
  sLogModuleManager->Print(Name(), aLevel, aFmt, aArgs);
}

} // namespace mozilla
