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
#include "mozilla/Sprintf.h"
#include "mozilla/Atomics.h"
#include "mozilla/UniquePtrExtensions.h"
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
// When rotate option is added to the modules list, this is the hardcoded
// number of files we create and rotate.  When there is rotate:40,
// we will keep four files per process, each limited to 10MB.  Sum is 40MB,
// the given limit.
const uint32_t kRotateFilesNumber = 4;

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

} // detail

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

namespace detail {

/**
 * A helper class providing reference counting for FILE*.
 * It encapsulates the following:
 *  - the FILE handle
 *  - the order number it was created for when rotating (actual path)
 *  - number of active references
 */
class LogFile
{
  FILE* mFile;
  uint32_t mFileNum;

public:
  LogFile(FILE* aFile, uint32_t aFileNum)
    : mFile(aFile)
    , mFileNum(aFileNum)
    , mNextToRelease(nullptr)
  {
  }

  ~LogFile()
  {
    fclose(mFile);
    delete mNextToRelease;
  }

  FILE* File() const { return mFile; }
  uint32_t Num() const { return mFileNum; }

  LogFile* mNextToRelease;
};

const char*
ExpandPIDMarker(const char* aFilename, char (&buffer)[2048])
{
  MOZ_ASSERT(aFilename);
  static const char kPIDToken[] = "%PID";
  const char* pidTokenPtr = strstr(aFilename, kPIDToken);
  if (pidTokenPtr &&
    SprintfLiteral(buffer, "%.*s%s%d%s",
                   static_cast<int>(pidTokenPtr - aFilename), aFilename,
                   XRE_IsParentProcess() ? "-main." : "-child.",
                   base::GetCurrentProcId(),
                   pidTokenPtr + strlen(kPIDToken)) > 0)
  {
    return buffer;
  }

  return aFilename;
}

} // detail

class LogModuleManager
{
public:
  LogModuleManager()
    : mModulesLock("logmodules")
    , mModules(kInitialModuleCount)
    , mPrintEntryCount(0)
    , mOutFile(nullptr)
    , mToReleaseFile(nullptr)
    , mOutFileNum(0)
    , mOutFilePath(strdup(""))
    , mMainThread(PR_GetCurrentThread())
    , mSetFromEnv(false)
    , mAddTimestamp(false)
    , mIsSync(false)
    , mRotate(0)
  {
  }

  ~LogModuleManager()
  {
    detail::LogFile* logFile = mOutFile.exchange(nullptr);
    delete logFile;
  }

  /**
   * Loads config from env vars if present.
   */
  void Init()
  {
    bool shouldAppend = false;
    bool addTimestamp = false;
    bool isSync = false;
    int32_t rotate = 0;
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
        [&shouldAppend, &addTimestamp, &isSync, &rotate]
            (const char* aName, LogLevel aLevel, int32_t aValue) mutable {
          if (strcmp(aName, "append") == 0) {
            shouldAppend = true;
          } else if (strcmp(aName, "timestamp") == 0) {
            addTimestamp = true;
          } else if (strcmp(aName, "sync") == 0) {
            isSync = true;
          } else if (strcmp(aName, "rotate") == 0) {
            rotate = (aValue << 20) / kRotateFilesNumber;
          } else {
            LogModule::Get(aName)->SetLevel(aLevel);
          }
    });

    // Rotate implies timestamp to make the files readable
    mAddTimestamp = addTimestamp || rotate > 0;
    mIsSync = isSync;
    mRotate = rotate;

    if (rotate > 0 && shouldAppend) {
      NS_WARNING("MOZ_LOG: when you rotate the log, you cannot use append!");
    }

    const char* logFile = PR_GetEnv("MOZ_LOG_FILE");
    if (!logFile || !logFile[0]) {
      logFile = PR_GetEnv("NSPR_LOG_FILE");
    }

    if (logFile && logFile[0]) {
      char buf[2048];
      logFile = detail::ExpandPIDMarker(logFile, buf);
      mOutFilePath.reset(strdup(logFile));

      if (mRotate > 0) {
        // Delete all the previously captured files, including non-rotated
        // log files, so that users don't complain our logs eat space even
        // after the rotate option has been added and don't happen to send
        // us old large logs along with the rotated files.
        remove(mOutFilePath.get());
        for (uint32_t i = 0; i < kRotateFilesNumber; ++i) {
          RemoveFile(i);
        }
      }

      mOutFile = OpenFile(shouldAppend, mOutFileNum);
      mSetFromEnv = true;
    }
  }

  void SetLogFile(const char* aFilename)
  {
    // For now we don't allow you to change the file at runtime.
    if (mSetFromEnv) {
      NS_WARNING("LogModuleManager::SetLogFile - Log file was set from the "
                 "MOZ_LOG_FILE environment variable.");
      return;
    }

    const char * filename = aFilename ? aFilename : "";
    char buf[2048];
    filename = detail::ExpandPIDMarker(filename, buf);

    // Can't use rotate at runtime yet.
    MOZ_ASSERT(mRotate == 0, "We don't allow rotate for runtime logfile changes");
    mOutFilePath.reset(strdup(filename));

    // Exchange mOutFile and set it to be released once all the writes are done.
    detail::LogFile* newFile = OpenFile(false, 0);
    detail::LogFile* oldFile = mOutFile.exchange(newFile);

    // Since we don't allow changing the logfile if MOZ_LOG_FILE is already set,
    // and we don't allow log rotation when setting it at runtime, mToReleaseFile
    // will be null, so we're not leaking.
    DebugOnly<detail::LogFile*> prevFile = mToReleaseFile.exchange(oldFile);
    MOZ_ASSERT(!prevFile, "Should be null because rotation is not allowed");
  }

  uint32_t GetLogFile(char *aBuffer, size_t aLength)
  {
    uint32_t len = strlen(mOutFilePath.get());
    if (len + 1 > aLength) {
      return 0;
    }
    snprintf(aBuffer, aLength, "%s", mOutFilePath.get());
    return len;
  }

  void SetIsSync(bool aIsSync)
  {
    mIsSync = aIsSync;
  }

  void SetAddTimestamp(bool aAddTimestamp)
  {
    mAddTimestamp = aAddTimestamp;
  }

  detail::LogFile* OpenFile(bool aShouldAppend, uint32_t aFileNum)
  {
    FILE* file;

    if (mRotate > 0) {
      char buf[2048];
      SprintfLiteral(buf, "%s.%d", mOutFilePath.get(), aFileNum);

      // rotate doesn't support append.
      file = fopen(buf, "w");
    } else {
      file = fopen(mOutFilePath.get(), aShouldAppend ? "a" : "w");
    }

    if (!file) {
      return nullptr;
    }

    return new detail::LogFile(file, aFileNum);
  }

  void RemoveFile(uint32_t aFileNum)
  {
    char buf[2048];
    SprintfLiteral(buf, "%s.%d", mOutFilePath.get(), aFileNum);
    remove(buf);
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

    FILE* out = stderr;

    // In case we use rotate, this ensures the FILE is kept alive during
    // its use.  Increased before we load mOutFile.
    ++mPrintEntryCount;

    detail::LogFile* outFile = mOutFile;
    if (outFile) {
      out = outFile->File();
    }

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
      SprintfLiteral(noNameThread, "Unnamed thread %p", currentThread);
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

    if (mRotate > 0 && outFile) {
      int32_t fileSize = ftell(out);
      if (fileSize > mRotate) {
        uint32_t fileNum = outFile->Num();

        uint32_t nextFileNum = fileNum + 1;
        if (nextFileNum >= kRotateFilesNumber) {
          nextFileNum = 0;
        }

        // And here is the trick.  The current out-file remembers its order
        // number.  When no other thread shifted the global file number yet,
        // we are the thread to open the next file.
        if (mOutFileNum.compareExchange(fileNum, nextFileNum)) {
          // We can work with mToReleaseFile because we are sure the
          // mPrintEntryCount can't drop to zero now - the condition
          // to actually delete what's stored in that member.
          // And also, no other thread can enter this piece of code
          // because mOutFile is still holding the current file with
          // the non-shifted number.  The compareExchange() above is
          // a no-op for other threads.
          outFile->mNextToRelease = mToReleaseFile;
          mToReleaseFile = outFile;

          mOutFile = OpenFile(false, nextFileNum);
        }
      }
    }

    if (--mPrintEntryCount == 0 && mToReleaseFile) {
      // We were the last Print() entered, if there is a file to release
      // do it now.  exchange() is atomic and makes sure we release the file
      // only once on one thread.
      detail::LogFile* release = mToReleaseFile.exchange(nullptr);
      delete release;
    }
  }

private:
  OffTheBooksMutex mModulesLock;
  nsClassHashtable<nsCharPtrHashKey, LogModule> mModules;

  // Print() entry counter, actually reflects concurrent use of the current
  // output file.  ReleaseAcquire ensures that manipulation with mOutFile
  // and mToReleaseFile is synchronized by manipulation with this value.
  Atomic<uint32_t, ReleaseAcquire> mPrintEntryCount;
  // File to write to.  ReleaseAcquire because we need to sync mToReleaseFile
  // with this.
  Atomic<detail::LogFile*, ReleaseAcquire> mOutFile;
  // File to be released when reference counter drops to zero.  This member
  // is assigned mOutFile when the current file has reached the limit.
  // It can be Relaxed, since it's synchronized with mPrintEntryCount
  // manipulation and we do atomic exchange() on it.
  Atomic<detail::LogFile*, Relaxed> mToReleaseFile;
  // The next file number.  This is mostly only for synchronization sake.
  // Can have relaxed ordering, since we only do compareExchange on it which
  // is atomic regardless ordering.
  Atomic<uint32_t, Relaxed> mOutFileNum;
  // Just keeps the actual file path for further use.
  UniqueFreePtr<char[]> mOutFilePath;

  PRThread *mMainThread;
  bool mSetFromEnv;
  Atomic<bool, Relaxed> mAddTimestamp;
  Atomic<bool, Relaxed> mIsSync;
  int32_t mRotate;
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
LogModule::SetLogFile(const char* aFilename)
{
  MOZ_ASSERT(sLogModuleManager);
  sLogModuleManager->SetLogFile(aFilename);
}

uint32_t
LogModule::GetLogFile(char *aBuffer, size_t aLength)
{
  MOZ_ASSERT(sLogModuleManager);
  return sLogModuleManager->GetLogFile(aBuffer, aLength);
}

void
LogModule::SetAddTimestamp(bool aAddTimestamp)
{
  sLogModuleManager->SetAddTimestamp(aAddTimestamp);
}

void
LogModule::SetIsSync(bool aIsSync)
{
  sLogModuleManager->SetIsSync(aIsSync);
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
