/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtils.h"
#include "mozilla/LateWriteChecks.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Printf.h"
#include "mozilla/Atomics.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtrExtensions.h"
#include "MainThreadUtils.h"
#include "nsClassHashtable.h"
#include "nsDebug.h"
#include "nsDebugImpl.h"
#include "NSPRLogModulesParser.h"
#include "LogCommandLineHandler.h"

#include "prenv.h"
#ifdef XP_WIN
#  include <fcntl.h>
#  include <process.h>
#else
#  include <sys/stat.h>  // for umask()
#  include <sys/types.h>
#  include <unistd.h>
#endif

// NB: Initial amount determined by auditing the codebase for the total amount
//     of unique module names and padding up to the next power of 2.
const uint32_t kInitialModuleCount = 256;
// When rotate option is added to the modules list, this is the hardcoded
// number of files we create and rotate.  When there is rotate:40,
// we will keep four files per process, each limited to 10MB.  Sum is 40MB,
// the given limit.
//
// (Note: When this is changed to be >= 10, SandboxBroker::LaunchApp must add
// another rule to allow logfile.?? be written by content processes.)
const uint32_t kRotateFilesNumber = 4;

namespace mozilla {

namespace detail {

void log_print(const LogModule* aModule, LogLevel aLevel, const char* aFmt,
               ...) {
  va_list ap;
  va_start(ap, aFmt);
  aModule->Printv(aLevel, aFmt, ap);
  va_end(ap);
}

void log_print(const LogModule* aModule, LogLevel aLevel, TimeStamp* aStart,
               const char* aFmt, ...) {
  va_list ap;
  va_start(ap, aFmt);
  aModule->Printv(aLevel, aStart, aFmt, ap);
  va_end(ap);
}

}  // namespace detail

LogLevel ToLogLevel(int32_t aLevel) {
  aLevel = std::min(aLevel, static_cast<int32_t>(LogLevel::Verbose));
  aLevel = std::max(aLevel, static_cast<int32_t>(LogLevel::Disabled));
  return static_cast<LogLevel>(aLevel);
}

static const char* ToLogStr(LogLevel aLevel) {
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
class LogFile {
  FILE* mFile;
  uint32_t mFileNum;

 public:
  LogFile(FILE* aFile, uint32_t aFileNum)
      : mFile(aFile), mFileNum(aFileNum), mNextToRelease(nullptr) {}

  ~LogFile() {
    fclose(mFile);
    delete mNextToRelease;
  }

  FILE* File() const { return mFile; }
  uint32_t Num() const { return mFileNum; }

  LogFile* mNextToRelease;
};

static const char* ExpandLogFileName(const char* aFilename,
                                     char (&buffer)[2048]) {
  MOZ_ASSERT(aFilename);
  static const char kPIDToken[] = MOZ_LOG_PID_TOKEN;
  static const char kMOZLOGExt[] = MOZ_LOG_FILE_EXTENSION;

  bool hasMozLogExtension = StringEndsWith(nsDependentCString(aFilename),
                                           nsLiteralCString(kMOZLOGExt));

  const char* pidTokenPtr = strstr(aFilename, kPIDToken);
  if (pidTokenPtr &&
      SprintfLiteral(buffer, "%.*s%s%d%s%s",
                     static_cast<int>(pidTokenPtr - aFilename), aFilename,
                     XRE_IsParentProcess() ? "-main." : "-child.",
                     base::GetCurrentProcId(), pidTokenPtr + strlen(kPIDToken),
                     hasMozLogExtension ? "" : kMOZLOGExt) > 0) {
    return buffer;
  }

  if (!hasMozLogExtension &&
      SprintfLiteral(buffer, "%s%s", aFilename, kMOZLOGExt) > 0) {
    return buffer;
  }

  return aFilename;
}

// Drop initial lines from the given file until it is less than or equal to the
// given size.
//
// For simplicity and to reduce memory consumption, lines longer than the given
// long line size may be broken.
//
// This function uses `mkstemp` and `rename` on POSIX systems and `_mktemp_s`
// and `ReplaceFileA` on Win32 systems.  `ReplaceFileA` was introduced in
// Windows 7 so it's available.
bool LimitFileToLessThanSize(const char* aFilename, uint32_t aSize,
                             uint16_t aLongLineSize = 16384) {
  // `tempFilename` will be further updated below.
  char tempFilename[2048];
  SprintfLiteral(tempFilename, "%s.tempXXXXXX", aFilename);

  bool failedToWrite = false;

  {  // Scope `file` and `temp`, so that they are definitely closed.
    ScopedCloseFile file(fopen(aFilename, "rb"));
    if (!file) {
      return false;
    }

    if (fseek(file, 0, SEEK_END)) {
      // If we can't seek for some reason, better to just not limit the log at
      // all and hope to sort out large logs upon further analysis.
      return false;
    }

    // `ftell` returns a positive `long`, which might be more than 32 bits.
    uint64_t fileSize = static_cast<uint64_t>(ftell(file));

    if (fileSize <= aSize) {
      return true;
    }

    uint64_t minBytesToDrop = fileSize - aSize;
    uint64_t numBytesDropped = 0;

    if (fseek(file, 0, SEEK_SET)) {
      // Same as above: if we can't seek, hope for the best.
      return false;
    }

    ScopedCloseFile temp;

#if defined(OS_WIN)
    // This approach was cribbed from
    // https://searchfox.org/mozilla-central/rev/868935867c6241e1302e64cf9be8f56db0fd0d1c/xpcom/build/LateWriteChecks.cpp#158.
    HANDLE hFile;
    do {
      // mkstemp isn't supported so keep trying until we get a file.
      _mktemp_s(tempFilename, strlen(tempFilename) + 1);
      hFile = CreateFileA(tempFilename, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    } while (GetLastError() == ERROR_FILE_EXISTS);

    if (hFile == INVALID_HANDLE_VALUE) {
      NS_WARNING("INVALID_HANDLE_VALUE");
      return false;
    }

    int fd = _open_osfhandle((intptr_t)hFile, _O_APPEND);
    if (fd == -1) {
      NS_WARNING("_open_osfhandle failed!");
      return false;
    }

    temp.reset(_fdopen(fd, "ab"));
#elif defined(OS_POSIX)

    // Coverity would prefer us to set a secure umask before using `mkstemp`.
    // However, the umask is process-wide, so setting it may lead to difficult
    // to debug complications; and it is fine for this particular short-lived
    // temporary file to be insecure.
    //
    // coverity[SECURE_TEMP : FALSE]
    int fd = mkstemp(tempFilename);
    if (fd == -1) {
      NS_WARNING("mkstemp failed!");
      return false;
    }
    temp.reset(fdopen(fd, "ab"));
#else
#  error Do not know how to open named temporary file
#endif

    if (!temp) {
      NS_WARNING(nsPrintfCString("could not open named temporary file %s",
                                 tempFilename)
                     .get());
      return false;
    }

    // `fgets` always null terminates.  If the line is too long, it won't
    // include a trailing '\n' but will be null-terminated.
    UniquePtr<char[]> line = MakeUnique<char[]>(aLongLineSize + 1);
    while (fgets(line.get(), aLongLineSize + 1, file)) {
      if (numBytesDropped >= minBytesToDrop) {
        if (fputs(line.get(), temp) < 0) {
          NS_WARNING(
              nsPrintfCString("fputs failed: ferror %d\n", ferror(temp)).get());
          failedToWrite = true;
          break;
        }
      } else {
        // Binary mode avoids platform-specific wrinkles with text streams.  In
        // particular, on Windows, `\r\n` gets read as `\n` (and the reverse
        // when writing), complicating this calculation.
        numBytesDropped += strlen(line.get());
      }
    }
  }

  // At this point, `file` and `temp` are closed, so we can remove and rename.
  if (failedToWrite) {
    remove(tempFilename);
    return false;
  }

#if defined(OS_WIN)
  if (!::ReplaceFileA(aFilename, tempFilename, nullptr, 0, 0, 0)) {
    NS_WARNING(
        nsPrintfCString("ReplaceFileA failed: %d\n", GetLastError()).get());
    return false;
  }
#elif defined(OS_POSIX)
  if (rename(tempFilename, aFilename)) {
    NS_WARNING(
        nsPrintfCString("rename failed: %s (%d)\n", strerror(errno), errno)
            .get());
    return false;
  }
#else
#  error Do not know how to atomically replace file
#endif

  return true;
}

}  // namespace detail

namespace {
// Helper method that initializes an empty va_list to be empty.
void empty_va(va_list* va, ...) {
  va_start(*va, va);
  va_end(*va);
}
}  // namespace

class LogModuleManager {
 public:
  LogModuleManager()
      : mModulesLock("logmodules"),
        mModules(kInitialModuleCount),
        mPrintEntryCount(0),
        mOutFile(nullptr),
        mToReleaseFile(nullptr),
        mOutFileNum(0),
        mOutFilePath(strdup("")),
        mMainThread(PR_GetCurrentThread()),
        mSetFromEnv(false),
        mAddTimestamp(false),
        mAddProfilerMarker(false),
        mIsRaw(false),
        mIsSync(false),
        mRotate(0),
        mInitialized(false) {}

  ~LogModuleManager() {
    detail::LogFile* logFile = mOutFile.exchange(nullptr);
    delete logFile;
  }

  /**
   * Loads config from command line args or env vars if present, in
   * this specific order of priority.
   *
   * Notes:
   *
   * 1) This function is only intended to be called once per session.
   * 2) None of the functions used in Init should rely on logging.
   */
  void Init(int argc, char* argv[]) {
    MOZ_DIAGNOSTIC_ASSERT(!mInitialized);
    mInitialized = true;

    LoggingHandleCommandLineArgs(argc, static_cast<char const* const*>(argv),
                                 [](nsACString const& env) {
                                   // We deliberately set/rewrite the
                                   // environment variables so that when child
                                   // processes are spawned w/o passing the
                                   // arguments they still inherit the logging
                                   // settings as well as sandboxing can be
                                   // correctly set. Scripts can pass
                                   // -MOZ_LOG=$MOZ_LOG,modules as an argument
                                   // to merge existing settings, if required.

                                   // PR_SetEnv takes ownership of the string.
                                   PR_SetEnv(ToNewCString(env));
                                 });

    bool shouldAppend = false;
    bool addTimestamp = false;
    bool isSync = false;
    bool isRaw = false;
    bool isMarkers = false;
    int32_t rotate = 0;
    int32_t maxSize = 0;
    bool prependHeader = false;
    const char* modules = PR_GetEnv("MOZ_LOG");
    if (!modules || !modules[0]) {
      modules = PR_GetEnv("MOZ_LOG_MODULES");
      if (modules) {
        NS_WARNING(
            "MOZ_LOG_MODULES is deprecated."
            "\nPlease use MOZ_LOG instead.");
      }
    }
    if (!modules || !modules[0]) {
      modules = PR_GetEnv("NSPR_LOG_MODULES");
      if (modules) {
        NS_WARNING(
            "NSPR_LOG_MODULES is deprecated."
            "\nPlease use MOZ_LOG instead.");
      }
    }

    // Need to capture `this` since `sLogModuleManager` is not set until after
    // initialization is complete.
    NSPRLogModulesParser(
        modules,
        [this, &shouldAppend, &addTimestamp, &isSync, &isRaw, &rotate, &maxSize,
         &prependHeader, &isMarkers](const char* aName, LogLevel aLevel,
                                     int32_t aValue) mutable {
          if (strcmp(aName, "append") == 0) {
            shouldAppend = true;
          } else if (strcmp(aName, "timestamp") == 0) {
            addTimestamp = true;
          } else if (strcmp(aName, "sync") == 0) {
            isSync = true;
          } else if (strcmp(aName, "raw") == 0) {
            isRaw = true;
          } else if (strcmp(aName, "rotate") == 0) {
            rotate = (aValue << 20) / kRotateFilesNumber;
          } else if (strcmp(aName, "maxsize") == 0) {
            maxSize = aValue << 20;
          } else if (strcmp(aName, "prependheader") == 0) {
            prependHeader = true;
          } else if (strcmp(aName, "profilermarkers") == 0) {
            isMarkers = true;
          } else {
            this->CreateOrGetModule(aName)->SetLevel(aLevel);
          }
        });

    // Rotate implies timestamp to make the files readable
    mAddTimestamp = addTimestamp || rotate > 0;
    mIsSync = isSync;
    mIsRaw = isRaw;
    mRotate = rotate;
    mAddProfilerMarker = isMarkers;

    if (rotate > 0 && shouldAppend) {
      NS_WARNING("MOZ_LOG: when you rotate the log, you cannot use append!");
    }

    if (rotate > 0 && maxSize > 0) {
      NS_WARNING(
          "MOZ_LOG: when you rotate the log, you cannot use maxsize! (ignoring "
          "maxsize)");
      maxSize = 0;
    }

    if (maxSize > 0 && !shouldAppend) {
      NS_WARNING(
          "MOZ_LOG: when you limit the log to maxsize, you must use append! "
          "(ignorning maxsize)");
      maxSize = 0;
    }

    if (rotate > 0 && prependHeader) {
      NS_WARNING(
          "MOZ_LOG: when you rotate the log, you cannot use prependheader!");
      prependHeader = false;
    }

    const char* logFile = PR_GetEnv("MOZ_LOG_FILE");
    if (!logFile || !logFile[0]) {
      logFile = PR_GetEnv("NSPR_LOG_FILE");
    }

    if (logFile && logFile[0]) {
      char buf[2048];
      logFile = detail::ExpandLogFileName(logFile, buf);
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

      mOutFile = OpenFile(shouldAppend, mOutFileNum, maxSize);
      mSetFromEnv = true;
    }

    if (prependHeader && XRE_IsParentProcess()) {
      va_list va;
      empty_va(&va);
      Print("Logger", LogLevel::Info, nullptr, "\n***\n\n", "Opening log\n",
            va);
    }
  }

  void SetLogFile(const char* aFilename) {
    // For now we don't allow you to change the file at runtime.
    if (mSetFromEnv) {
      NS_WARNING(
          "LogModuleManager::SetLogFile - Log file was set from the "
          "MOZ_LOG_FILE environment variable.");
      return;
    }

    const char* filename = aFilename ? aFilename : "";
    char buf[2048];
    filename = detail::ExpandLogFileName(filename, buf);

    // Can't use rotate or maxsize at runtime yet.
    MOZ_ASSERT(mRotate == 0,
               "We don't allow rotate for runtime logfile changes");
    mOutFilePath.reset(strdup(filename));

    // Exchange mOutFile and set it to be released once all the writes are done.
    detail::LogFile* newFile = OpenFile(false, 0);
    detail::LogFile* oldFile = mOutFile.exchange(newFile);

    // Since we don't allow changing the logfile if MOZ_LOG_FILE is already set,
    // and we don't allow log rotation when setting it at runtime,
    // mToReleaseFile will be null, so we're not leaking.
    DebugOnly<detail::LogFile*> prevFile = mToReleaseFile.exchange(oldFile);
    MOZ_ASSERT(!prevFile, "Should be null because rotation is not allowed");

    // If we just need to release a file, we must force print, in order to
    // trigger the closing and release of mToReleaseFile.
    if (oldFile) {
      va_list va;
      empty_va(&va);
      Print("Logger", LogLevel::Info, "Flushing old log files\n", va);
    }
  }

  uint32_t GetLogFile(char* aBuffer, size_t aLength) {
    uint32_t len = strlen(mOutFilePath.get());
    if (len + 1 > aLength) {
      return 0;
    }
    snprintf(aBuffer, aLength, "%s", mOutFilePath.get());
    return len;
  }

  void SetIsSync(bool aIsSync) { mIsSync = aIsSync; }

  void SetAddTimestamp(bool aAddTimestamp) { mAddTimestamp = aAddTimestamp; }

  detail::LogFile* OpenFile(bool aShouldAppend, uint32_t aFileNum,
                            uint32_t aMaxSize = 0) {
    FILE* file;

    if (mRotate > 0) {
      char buf[2048];
      SprintfLiteral(buf, "%s.%d", mOutFilePath.get(), aFileNum);

      // rotate doesn't support append (or maxsize).
      file = fopen(buf, "w");
    } else if (aShouldAppend && aMaxSize > 0) {
      detail::LimitFileToLessThanSize(mOutFilePath.get(), aMaxSize >> 1);
      file = fopen(mOutFilePath.get(), "a");
    } else {
      file = fopen(mOutFilePath.get(), aShouldAppend ? "a" : "w");
    }

    if (!file) {
      return nullptr;
    }

    return new detail::LogFile(file, aFileNum);
  }

  void RemoveFile(uint32_t aFileNum) {
    char buf[2048];
    SprintfLiteral(buf, "%s.%d", mOutFilePath.get(), aFileNum);
    remove(buf);
  }

  LogModule* CreateOrGetModule(const char* aName) {
    OffTheBooksMutexAutoLock guard(mModulesLock);
    return mModules
        .LookupOrInsertWith(aName,
                            [&] {
                              return UniquePtr<LogModule>(
                                  new LogModule{aName, LogLevel::Disabled});
                            })
        .get();
  }

  void Print(const char* aName, LogLevel aLevel, const char* aFmt,
             va_list aArgs) MOZ_FORMAT_PRINTF(4, 0) {
    Print(aName, aLevel, nullptr, "", aFmt, aArgs);
  }

  void Print(const char* aName, LogLevel aLevel, const TimeStamp* aStart,
             const char* aPrepend, const char* aFmt, va_list aArgs)
      MOZ_FORMAT_PRINTF(6, 0) {
    AutoSuspendLateWriteChecks suspendLateWriteChecks;
    long pid = static_cast<long>(base::GetCurrentProcId());
    const size_t kBuffSize = 1024;
    char buff[kBuffSize];

    char* buffToWrite = buff;
    SmprintfPointer allocatedBuff;

    va_list argsCopy;
    va_copy(argsCopy, aArgs);
    int charsWritten = VsprintfLiteral(buff, aFmt, argsCopy);
    va_end(argsCopy);

    if (charsWritten < 0) {
      // Print out at least something.  We must copy to the local buff,
      // can't just assign aFmt to buffToWrite, since when
      // buffToWrite != buff, we try to release it.
      MOZ_ASSERT(false, "Probably incorrect format string in LOG?");
      strncpy(buff, aFmt, kBuffSize - 1);
      buff[kBuffSize - 1] = '\0';
      charsWritten = strlen(buff);
    } else if (static_cast<size_t>(charsWritten) >= kBuffSize - 1) {
      // We may have maxed out, allocate a buffer instead.
      allocatedBuff = mozilla::Vsmprintf(aFmt, aArgs);
      buffToWrite = allocatedBuff.get();
      charsWritten = strlen(buffToWrite);
    }

#ifdef MOZ_GECKO_PROFILER
    if (mAddProfilerMarker && profiler_can_accept_markers()) {
      struct LogMarker {
        static constexpr Span<const char> MarkerTypeName() {
          return MakeStringSpan("Log");
        }
        static void StreamJSONMarkerData(
            baseprofiler::SpliceableJSONWriter& aWriter,
            const ProfilerString8View& aModule,
            const ProfilerString8View& aText) {
          aWriter.StringProperty("module", aModule);
          aWriter.StringProperty("name", aText);
        }
        static MarkerSchema MarkerTypeDisplay() {
          using MS = MarkerSchema;
          MS schema{MS::Location::markerTable};
          schema.SetTableLabel("({marker.data.module}) {marker.data.name}");
          schema.AddKeyLabelFormat("module", "Module", MS::Format::string);
          schema.AddKeyLabelFormat("name", "Name", MS::Format::string);
          return schema;
        }
      };

      profiler_add_marker(
          "LogMessages", geckoprofiler::category::OTHER,
          aStart ? MarkerTiming::IntervalUntilNowFrom(*aStart)
                 : MarkerTiming::InstantNow(),
          LogMarker{}, ProfilerString8View::WrapNullTerminatedString(aName),
          ProfilerString8View::WrapNullTerminatedString(buffToWrite));
    }
#endif

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
    PRThread* currentThread = PR_GetCurrentThread();
    const char* currentThreadName = (mMainThread == currentThread)
                                        ? "Main Thread"
                                        : PR_GetThreadName(currentThread);

    char noNameThread[40];
    if (!currentThreadName) {
      SprintfLiteral(noNameThread, "Unnamed thread %p", currentThread);
      currentThreadName = noNameThread;
    }

    if (!mAddTimestamp && !aStart) {
      if (!mIsRaw) {
        fprintf_stderr(out, "%s[%s %ld: %s]: %s/%s %s%s", aPrepend,
                       nsDebugImpl::GetMultiprocessMode(), pid,
                       currentThreadName, ToLogStr(aLevel), aName, buffToWrite,
                       newline);
      } else {
        fprintf_stderr(out, "%s%s%s", aPrepend, buffToWrite, newline);
      }
    } else {
      if (aStart) {
        // XXX is there a reasonable way to convert one to the other?  this is
        // bad
        PRTime prnow = PR_Now();
        TimeStamp tmnow = TimeStamp::NowUnfuzzed();
        TimeDuration duration = tmnow - *aStart;
        PRTime prstart = prnow - duration.ToMicroseconds();

        PRExplodedTime now;
        PRExplodedTime start;
        PR_ExplodeTime(prnow, PR_GMTParameters, &now);
        PR_ExplodeTime(prstart, PR_GMTParameters, &start);
        // Ignore that the start time might be in a different day
        fprintf_stderr(
            out,
            "%s%04d-%02d-%02d %02d:%02d:%02d.%06d -> %02d:%02d:%02d.%06d UTC "
            "(%.1gms)- [%s %ld: %s]: %s/%s %s%s",
            aPrepend, now.tm_year, now.tm_month + 1, start.tm_mday,
            start.tm_hour, start.tm_min, start.tm_sec, start.tm_usec,
            now.tm_hour, now.tm_min, now.tm_sec, now.tm_usec,
            duration.ToMilliseconds(), nsDebugImpl::GetMultiprocessMode(), pid,
            currentThreadName, ToLogStr(aLevel), aName, buffToWrite, newline);
      } else {
        PRExplodedTime now;
        PR_ExplodeTime(PR_Now(), PR_GMTParameters, &now);
        fprintf_stderr(out,
                       "%s%04d-%02d-%02d %02d:%02d:%02d.%06d UTC - [%s %ld: "
                       "%s]: %s/%s %s%s",
                       aPrepend, now.tm_year, now.tm_month + 1, now.tm_mday,
                       now.tm_hour, now.tm_min, now.tm_sec, now.tm_usec,
                       nsDebugImpl::GetMultiprocessMode(), pid,
                       currentThreadName, ToLogStr(aLevel), aName, buffToWrite,
                       newline);
      }
    }

    if (mIsSync) {
      fflush(out);
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

  PRThread* mMainThread;
  bool mSetFromEnv;
  Atomic<bool, Relaxed> mAddTimestamp;
  Atomic<bool, Relaxed> mAddProfilerMarker;
  Atomic<bool, Relaxed> mIsRaw;
  Atomic<bool, Relaxed> mIsSync;
  int32_t mRotate;
  bool mInitialized;
};

StaticAutoPtr<LogModuleManager> sLogModuleManager;

LogModule* LogModule::Get(const char* aName) {
  // This is just a pass through to the LogModuleManager so
  // that the LogModuleManager implementation can be kept internal.
  MOZ_ASSERT(sLogModuleManager != nullptr);
  return sLogModuleManager->CreateOrGetModule(aName);
}

void LogModule::SetLogFile(const char* aFilename) {
  MOZ_ASSERT(sLogModuleManager);
  sLogModuleManager->SetLogFile(aFilename);
}

uint32_t LogModule::GetLogFile(char* aBuffer, size_t aLength) {
  MOZ_ASSERT(sLogModuleManager);
  return sLogModuleManager->GetLogFile(aBuffer, aLength);
}

void LogModule::SetAddTimestamp(bool aAddTimestamp) {
  sLogModuleManager->SetAddTimestamp(aAddTimestamp);
}

void LogModule::SetIsSync(bool aIsSync) {
  sLogModuleManager->SetIsSync(aIsSync);
}

// This function is defined in gecko_logger/src/lib.rs
// We mirror the level in rust code so we don't get forwarded all of the
// rust logging and have to create an LogModule for each rust component.
extern "C" void set_rust_log_level(const char* name, uint8_t level);

void LogModule::SetLevel(LogLevel level) {
  mLevel = level;

  // If the log module contains `::` it is likely a rust module, so we
  // pass the level into the rust code so it will know to forward the logging
  // to Gecko.
  if (strstr(mName, "::")) {
    set_rust_log_level(mName, static_cast<uint8_t>(level));
  }
}

void LogModule::Init(int argc, char* argv[]) {
  // NB: This method is not threadsafe; it is expected to be called very early
  //     in startup prior to any other threads being run.
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  if (sLogModuleManager) {
    // Already initialized.
    return;
  }

  // NB: We intentionally do not register for ClearOnShutdown as that happens
  //     before all logging is complete. And, yes, that means we leak, but
  //     we're doing that intentionally.

  // Don't assign the pointer until after Init is called. This should help us
  // detect if any of the functions called by Init somehow rely on logging.
  auto mgr = new LogModuleManager();
  mgr->Init(argc, argv);
  sLogModuleManager = mgr;
}

void LogModule::Printv(LogLevel aLevel, const char* aFmt, va_list aArgs) const {
  MOZ_ASSERT(sLogModuleManager != nullptr);

  // Forward to LogModule manager w/ level and name
  sLogModuleManager->Print(Name(), aLevel, aFmt, aArgs);
}

void LogModule::Printv(LogLevel aLevel, const TimeStamp* aStart,
                       const char* aFmt, va_list aArgs) const {
  MOZ_ASSERT(sLogModuleManager != nullptr);

  // Forward to LogModule manager w/ level and name
  sLogModuleManager->Print(Name(), aLevel, aStart, "", aFmt, aArgs);
}

}  // namespace mozilla

extern "C" {

// This function is called by external code (rust) to log to one of our
// log modules.
void ExternMozLog(const char* aModule, mozilla::LogLevel aLevel,
                  const char* aMsg) {
  MOZ_ASSERT(sLogModuleManager != nullptr);

  LogModule* m = sLogModuleManager->CreateOrGetModule(aModule);
  if (MOZ_LOG_TEST(m, aLevel)) {
    mozilla::detail::log_print(m, aLevel, "%s", aMsg);
  }
}

}  // extern "C"
