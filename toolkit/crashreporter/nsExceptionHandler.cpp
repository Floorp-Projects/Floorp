/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExceptionHandler.h"
#include "nsExceptionHandlerUtils.h"

#include "json/json.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryService.h"
#include "nsString.h"
#include "nsTHashMap.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "mozilla/Unused.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Printf.h"
#include "mozilla/RuntimeExceptionModule.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TimeStamp.h"

#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsThread.h"
#include "jsfriendapi.h"
#include "private/pprio.h"
#include "base/process_util.h"
#include "common/basictypes.h"

#include "mozilla/toolkit/crashreporter/mozannotation_client_ffi_generated.h"
#include "mozilla/toolkit/crashreporter/mozannotation_server_ffi_generated.h"

#if defined(XP_WIN)
#  ifdef WIN32_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#  endif

#  include "nsXULAppAPI.h"
#  include "nsIXULAppInfo.h"
#  include "nsIWindowsRegKey.h"
#  include "breakpad-client/windows/crash_generation/client_info.h"
#  include "breakpad-client/windows/crash_generation/crash_generation_server.h"
#  include "breakpad-client/windows/handler/exception_handler.h"
#  include <dbghelp.h>
#  include <string.h>
#  include "nsDirectoryServiceUtils.h"

#  include "nsWindowsDllInterceptor.h"
#  include "mozilla/WindowsDllBlocklist.h"
#  include "psapi.h"  // For PERFORMANCE_INFORMATION and K32GetPerformanceInfo()
#elif defined(XP_MACOSX)
#  include "breakpad-client/mac/crash_generation/client_info.h"
#  include "breakpad-client/mac/crash_generation/crash_generation_server.h"
#  include "breakpad-client/mac/handler/exception_handler.h"
#  include <string>
#  include <Carbon/Carbon.h>
#  include <CoreFoundation/CoreFoundation.h>
#  include <crt_externs.h>
#  include <fcntl.h>
#  include <mach/mach.h>
#  include <mach/vm_statistics.h>
#  include <sys/sysctl.h>
#  include <sys/types.h>
#  include <spawn.h>
#  include <unistd.h>
#  include "mac_utils.h"
#elif defined(XP_LINUX)
#  include "nsIINIParser.h"
#  include "common/linux/linux_libc_support.h"
#  include "third_party/lss/linux_syscall_support.h"
#  include "breakpad-client/linux/crash_generation/client_info.h"
#  include "breakpad-client/linux/crash_generation/crash_generation_server.h"
#  include "breakpad-client/linux/handler/exception_handler.h"
#  include "common/linux/eintr_wrapper.h"
#  include <fcntl.h>
#  include <sys/types.h>
#  include "sys/sysinfo.h"
#  include <sys/wait.h>
#  include <unistd.h>
#else
#  error "Not yet implemented for this platform"
#endif  // defined(XP_WIN)

#ifdef XP_WIN
#  include <filesystem>
#endif
#include <fstream>
#include <optional>

#include <stdlib.h>
#include <time.h>
#include <prenv.h>
#include <prio.h>
#include "mozilla/Mutex.h"
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsIFile.h"

#include "mozilla/IOInterposer.h"
#include "mozilla/mozalloc_oom.h"

#if defined(XP_MACOSX)
CFStringRef reporterClientAppID = CFSTR("org.mozilla.crashreporter");
#endif
#if defined(MOZ_WIDGET_ANDROID)
#  include "common/linux/file_id.h"
#endif

using google_breakpad::ClientInfo;
using google_breakpad::CrashGenerationServer;
#ifdef XP_LINUX
using google_breakpad::MinidumpDescriptor;
#elif defined(XP_WIN)
using google_breakpad::ExceptionHandler;
#endif
#if defined(MOZ_WIDGET_ANDROID)
using google_breakpad::auto_wasteful_vector;
using google_breakpad::FileID;
using google_breakpad::kDefaultBuildIdSize;
using google_breakpad::PageAllocator;
#endif
using namespace mozilla;

namespace mozilla::phc {

// Global instance that is retrieved by the process generating the crash report
mozilla::phc::AddrInfo gAddrInfo;

}  // namespace mozilla::phc

namespace CrashReporter {

#ifdef XP_WIN
typedef wchar_t XP_CHAR;
typedef std::wstring xpstring;
#  define XP_TEXT(x) L##x
#  define CONVERT_XP_CHAR_TO_UTF16(x) x
#  define XP_STRLEN(x) wcslen(x)
#  define my_strlen strlen
#  define my_memchr memchr
#  define CRASH_REPORTER_FILENAME u"crashreporter.exe"_ns
#  define XP_PATH_SEPARATOR L"\\"
#  define XP_PATH_SEPARATOR_CHAR L'\\'
#  define XP_PATH_MAX (MAX_PATH + 1)
// "<reporter path>" "<minidump path>"
#  define CMDLINE_SIZE ((XP_PATH_MAX * 2) + 6)
#  define XP_TTOA(time, buffer) _i64toa((time), (buffer), 10)
#  define XP_STOA(size, buffer) _ui64toa((size), (buffer), 10)
#else
typedef char XP_CHAR;
typedef std::string xpstring;
#  define XP_TEXT(x) x
#  define CONVERT_XP_CHAR_TO_UTF16(x) NS_ConvertUTF8toUTF16(x)
#  define CRASH_REPORTER_FILENAME u"crashreporter"_ns
#  define XP_PATH_SEPARATOR "/"
#  define XP_PATH_SEPARATOR_CHAR '/'
#  define XP_PATH_MAX PATH_MAX
#  ifdef XP_LINUX
#    define XP_STRLEN(x) my_strlen(x)
#    define XP_TTOA(time, buffer) \
      my_u64tostring(uint64_t(time), (buffer), sizeof(buffer))
#    define XP_STOA(size, buffer) \
      my_u64tostring((size), (buffer), sizeof(buffer))
#  else
#    define XP_STRLEN(x) strlen(x)
#    define XP_TTOA(time, buffer) sprintf(buffer, "%" PRIu64, uint64_t(time))
#    define XP_STOA(size, buffer) sprintf(buffer, "%zu", size_t(size))
#    define my_strlen strlen
#    define my_memchr memchr
#    define sys_close close
#    define sys_fork fork
#    define sys_open open
#    define sys_read read
#    define sys_write write
#  endif
#endif  // XP_WIN

#if defined(__GNUC__)
#  define MAYBE_UNUSED __attribute__((unused))
#else
#  define MAYBE_UNUSED
#endif  // defined(__GNUC__)

#ifndef XP_LINUX
static const XP_CHAR dumpFileExtension[] = XP_TEXT(".dmp");
#endif

static const XP_CHAR extraFileExtension[] = XP_TEXT(".extra");
static const XP_CHAR memoryReportExtension[] = XP_TEXT(".memory.json.gz");
static std::optional<xpstring> defaultMemoryReportPath = {};

static const char kCrashMainID[] = "crash.main.3\n";

static google_breakpad::ExceptionHandler* gExceptionHandler = nullptr;
static mozilla::Atomic<bool> gEncounteredChildException(false);
static nsCString gServerURL;

static xpstring pendingDirectory;
static xpstring crashReporterPath;
static xpstring memoryReportPath;

// Where crash events should go.
static xpstring eventsDirectory;

// If this is false, we don't launch the crash reporter
static bool doReport = true;

// if this is true, we pass the exception on to the OS crash reporter
static bool showOSCrashReporter = false;

// The time of the last recorded crash, as a time_t value.
static time_t lastCrashTime = 0;
// The pathname of a file to store the crash time in
static XP_CHAR lastCrashTimeFilename[XP_PATH_MAX] = {0};

#if defined(MOZ_WIDGET_ANDROID)
// on Android 4.2 and above there is a user serial number associated
// with the current process that gets lost when we fork so we need to
// explicitly pass it to am
static char* androidUserSerial = nullptr;

// Before Android 8 we needed to use "startservice" to start the crash reporting
// service. After Android 8 we need to use "start-foreground-service"
static const char* androidStartServiceCommand = nullptr;
#endif

// this holds additional data sent via the API
static Mutex* notesFieldLock;
static nsCString* notesField = nullptr;
static bool isGarbageCollecting;
static uint32_t eventloopNestingLevel = 0;
static time_t inactiveStateStart = 0;

static
#if defined(XP_UNIX)
    pthread_t
#elif defined(XP_WIN)  // defined(XP_UNIX)
    DWORD
#endif                 // defined(XP_WIN)
        gMainThreadId = 0;

// Avoid a race during application termination.
static Mutex* dumpSafetyLock;
static bool isSafeToDump = false;

// Whether to include heap regions of the crash context.
static bool sIncludeContextHeap = false;

// OOP crash reporting
static CrashGenerationServer* crashServer;  // chrome process has this

static std::terminate_handler oldTerminateHandler = nullptr;

#if defined(XP_WIN) || defined(XP_MACOSX)
// If crash reporting is disabled, we hand out this "null" pipe to the
// child process and don't attempt to connect to a parent server.
static const char kNullNotifyPipe[] = "-";
static char* childCrashNotifyPipe;

#elif defined(XP_LINUX)
static int serverSocketFd = -1;
static int clientSocketFd = -1;

// On Linux these file descriptors are created in the parent process and
// remapped in the child ones. See PosixProcessLauncher::DoSetup() for more
// details.
static FileHandle gMagicChildCrashReportFd =
#  if defined(MOZ_WIDGET_ANDROID)
    // On android the fd is set at the time of child creation.
    kInvalidFileHandle
#  else
    4
#  endif  // defined(MOZ_WIDGET_ANDROID)
    ;
#endif

// |dumpMapLock| must protect all access to |pidToMinidump|.
static Mutex* dumpMapLock;
struct ChildProcessData : public nsUint32HashKey {
  explicit ChildProcessData(KeyTypePointer aKey)
      : nsUint32HashKey(aKey), annotations(nullptr) {}

  nsCOMPtr<nsIFile> minidump;
  UniquePtr<AnnotationTable> annotations;
};

typedef nsTHashtable<ChildProcessData> ChildMinidumpMap;
static ChildMinidumpMap* pidToMinidump;
static bool OOPInitialized();

void RecordMainThreadId() {
  gMainThreadId =
#if defined(XP_UNIX)
      pthread_self()
#elif defined(XP_WIN)  // defined(XP_UNIX)
      GetCurrentThreadId()
#endif                 // defined(XP_WIN)
      ;
}

bool SignalSafeIsMainThread() {
  // We can't rely on NS_IsMainThread() because we are in a signal handler, and
  // sTLSIsMainThread is a thread local variable and it can be lazy allocated
  // i.e., we could hit code path where this variable has not been accessed
  // before and needs to be allocated right now, which will lead to spinlock
  // deadlock effectively hanging the process, as in bug 1756407.

#if defined(XP_UNIX)
  pthread_t th = pthread_self();
  return pthread_equal(th, gMainThreadId);
#elif defined(XP_WIN)  // defined(XP_UNIX)
  DWORD th = GetCurrentThreadId();
  return th == gMainThreadId;
#endif                 // defined(XP_WIN)
}

#if defined(XP_WIN)
// the following are used to prevent other DLLs reverting the last chance
// exception handler to the windows default. Any attempt to change the
// unhandled exception filter or to reset it is ignored and our crash
// reporter is loaded instead (in case it became unloaded somehow)
typedef LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI* SetUnhandledExceptionFilter_func)(
    LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);
static WindowsDllInterceptor::FuncHookType<SetUnhandledExceptionFilter_func>
    stub_SetUnhandledExceptionFilter;
static LPTOP_LEVEL_EXCEPTION_FILTER previousUnhandledExceptionFilter = nullptr;
static WindowsDllInterceptor gKernel32Intercept;
static bool gBlockUnhandledExceptionFilter = true;

static LPTOP_LEVEL_EXCEPTION_FILTER GetUnhandledExceptionFilter() {
  // Set a dummy value to get the current filter, then restore
  LPTOP_LEVEL_EXCEPTION_FILTER current = SetUnhandledExceptionFilter(nullptr);
  SetUnhandledExceptionFilter(current);
  return current;
}

static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI patched_SetUnhandledExceptionFilter(
    LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter) {
  if (!gBlockUnhandledExceptionFilter) {
    // don't intercept
    return stub_SetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
  }

  if (lpTopLevelExceptionFilter == previousUnhandledExceptionFilter) {
    // OK to swap back and forth between the previous filter
    previousUnhandledExceptionFilter =
        stub_SetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
    return previousUnhandledExceptionFilter;
  }

  // intercept attempts to change the filter
  return nullptr;
}

#  if defined(HAVE_64BIT_BUILD)
static LPTOP_LEVEL_EXCEPTION_FILTER sUnhandledExceptionFilter = nullptr;

static long JitExceptionHandler(void* exceptionRecord, void* context) {
  EXCEPTION_POINTERS pointers = {(PEXCEPTION_RECORD)exceptionRecord,
                                 (PCONTEXT)context};
  return sUnhandledExceptionFilter(&pointers);
}

static void SetJitExceptionHandler() {
  sUnhandledExceptionFilter = GetUnhandledExceptionFilter();
  if (sUnhandledExceptionFilter)
    js::SetJitExceptionHandler(JitExceptionHandler);
}
#  endif

/**
 * Reserve some VM space. In the event that we crash because VM space is
 * being leaked without leaking memory, freeing this space before taking
 * the minidump will allow us to collect a minidump.
 *
 * This size is bigger than xul.dll plus some extra for MinidumpWriteDump
 * allocations.
 */
static const SIZE_T kReserveSize = 0x5000000;  // 80 MB
static void* gBreakpadReservedVM;
#endif

#ifdef XP_LINUX
static inline void my_u64tostring(uint64_t aValue, char* aBuffer,
                                  size_t aBufferLength) {
  my_memset(aBuffer, 0, aBufferLength);
  my_uitos(aBuffer, aValue, my_uint_len(aValue));
}
#endif

#ifdef XP_WIN
static void CreateFileFromPath(const xpstring& path, nsIFile** file) {
  NS_NewLocalFile(nsDependentString(path.c_str()), false, file);
}

static std::optional<xpstring> CreatePathFromFile(nsIFile* file) {
  nsAutoString path;
  nsresult rv = file->GetPath(path);
  if (NS_FAILED(rv)) {
    return {};
  }
  return xpstring(static_cast<wchar_t*>(path.get()), path.Length());
}
#else
static void CreateFileFromPath(const xpstring& path, nsIFile** file) {
  NS_NewNativeLocalFile(nsDependentCString(path.c_str()), false, file);
}

MAYBE_UNUSED static std::optional<xpstring> CreatePathFromFile(nsIFile* file) {
  nsAutoCString path;
  nsresult rv = file->GetNativePath(path);
  if (NS_FAILED(rv)) {
    return {};
  }
  return xpstring(path.get(), path.Length());
}
#endif

static time_t GetCurrentTimeForCrashTime() {
#ifdef XP_LINUX
  struct kernel_timeval tv;
  sys_gettimeofday(&tv, nullptr);
  return tv.tv_sec;
#else
  return time(nullptr);
#endif
}

static XP_CHAR* Concat(XP_CHAR* str, const XP_CHAR* toAppend, size_t* size) {
  size_t appendLen = XP_STRLEN(toAppend);
  if (appendLen >= *size) {
    appendLen = *size - 1;
  }

  memcpy(str, toAppend, appendLen * sizeof(XP_CHAR));
  str += appendLen;
  *str = '\0';
  *size -= appendLen;

  return str;
}

void AnnotateOOMAllocationSize(size_t size) { gOOMAllocationSize = size; }

static size_t gTexturesSize = 0;

void AnnotateTexturesSize(size_t size) { gTexturesSize = size; }

#ifndef XP_WIN
// Like Windows CopyFile for *nix
//
// This function is not declared static even though it's not used outside of
// this file because of an issue in Fennec which prevents breakpad's exception
// handler from invoking the MinidumpCallback function. See bug 1424304.
bool copy_file(const char* from, const char* to) {
  const int kBufSize = 4096;
  int fdfrom = sys_open(from, O_RDONLY, 0);
  if (fdfrom < 0) {
    return false;
  }

  bool ok = false;

  int fdto = sys_open(to, O_WRONLY | O_CREAT, 0666);
  if (fdto < 0) {
    sys_close(fdfrom);
    return false;
  }

  char buf[kBufSize];
  while (true) {
    int r = sys_read(fdfrom, buf, kBufSize);
    if (r == 0) {
      ok = true;
      break;
    }
    if (r < 0) {
      break;
    }
    char* wbuf = buf;
    while (r) {
      int w = sys_write(fdto, wbuf, r);
      if (w > 0) {
        r -= w;
        wbuf += w;
      } else if (errno != EINTR) {
        break;
      }
    }
    if (r) {
      break;
    }
  }

  sys_close(fdfrom);
  sys_close(fdto);

  return ok;
}
#endif

/**
 * The PlatformWriter class provides a tool to create and write to a file that
 * is safe to call from within an exception handler. To use it this way the
 * file path needs to be provided as a bare C string.
 */
class PlatformWriter {
 public:
  PlatformWriter() : mBuffer{}, mPos(0), mFD(kInvalidFileHandle) {}
  explicit PlatformWriter(const XP_CHAR* aPath) : PlatformWriter() {
    Open(aPath);
  }

  ~PlatformWriter() {
    if (Valid()) {
      Flush();
#ifdef XP_WIN
      CloseHandle(mFD);
#elif defined(XP_UNIX)
      sys_close(mFD);
#endif
    }
  }

  void Open(const XP_CHAR* aPath) {
#ifdef XP_WIN
    mFD = CreateFile(aPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
#elif defined(XP_UNIX)
    mFD = sys_open(aPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
#endif
  }

  void OpenHandle(FileHandle aFD) { mFD = aFD; }
  bool Valid() { return mFD != kInvalidFileHandle; }

  void WriteBuffer(const char* aBuffer, size_t aLen) {
    if (!Valid()) {
      return;
    }

    while (aLen-- > 0) {
      WriteChar(*aBuffer++);
    }
  }

  void WriteString(const char* aStr) { WriteBuffer(aStr, my_strlen(aStr)); }

  template <int N>
  void WriteLiteral(const char (&aStr)[N]) {
    WriteBuffer(aStr, N - 1);
  }

  FileHandle FileDesc() { return mFD; }

 private:
  PlatformWriter(const PlatformWriter&) = delete;

  const PlatformWriter& operator=(const PlatformWriter&) = delete;

  void WriteChar(char aChar) {
    if (mPos == kBufferSize) {
      Flush();
    }

    mBuffer[mPos++] = aChar;
  }

  void Flush() {
    if (mPos > 0) {
      char* buffer = mBuffer;
      size_t length = mPos;
      while (length > 0) {
#ifdef XP_WIN
        DWORD written_bytes = 0;
        if (!WriteFile(mFD, buffer, length, &written_bytes, nullptr)) {
          break;
        }
#elif defined(XP_UNIX)
        ssize_t written_bytes = sys_write(mFD, buffer, length);
        if (written_bytes < 0) {
          if (errno == EAGAIN) {
            continue;
          }

          break;
        }
#endif
        buffer += written_bytes;
        length -= written_bytes;
      }

      mPos = 0;
    }
  }

  static const size_t kBufferSize = 512;

  char mBuffer[kBufferSize];
  size_t mPos;
  FileHandle mFD;
};

class JSONAnnotationWriter : public AnnotationWriter {
 public:
  explicit JSONAnnotationWriter(PlatformWriter& aPlatformWriter)
      : mWriter(aPlatformWriter), mEmpty(true) {
    mWriter.WriteBuffer("{", 1);
  }

  ~JSONAnnotationWriter() { mWriter.WriteBuffer("}", 1); }

  void Write(Annotation aAnnotation, const char* aValue,
             size_t aLen = 0) override {
    size_t len = aLen ? aLen : my_strlen(aValue);
    const char* annotationStr = AnnotationToString(aAnnotation);

    if (len && CrashReporter::ShouldIncludeAnnotation(aAnnotation, aValue)) {
      WritePrefix();
      mWriter.WriteBuffer(annotationStr, my_strlen(annotationStr));
      WriteSeparator();
      WriteEscapedString(aValue, len);
      WriteSuffix();
    }
  };

  void Write(Annotation aAnnotation, bool aValue) override {
    Write(aAnnotation, aValue ? "1" : "0", 1);
  };

  void Write(Annotation aAnnotation, uint64_t aValue) override {
    char buffer[32] = {};
    XP_STOA(aValue, buffer);
    Write(aAnnotation, buffer);
  };

 private:
  void WritePrefix() {
    if (mEmpty) {
      mWriter.WriteBuffer("\"", 1);
      mEmpty = false;
    } else {
      mWriter.WriteBuffer(",\"", 2);
    }
  }

  void WriteSeparator() { mWriter.WriteBuffer("\":\"", 3); }
  void WriteSuffix() { mWriter.WriteBuffer("\"", 1); }
  void WriteEscapedString(const char* aStr, size_t aLen) {
    for (size_t i = 0; i < aLen; i++) {
      uint8_t c = aStr[i];
      if (c <= 0x1f || c == '\\' || c == '\"') {
        mWriter.WriteBuffer("\\u00", 4);
        WriteHexDigitAsAsciiChar((c & 0x00f0) >> 4);
        WriteHexDigitAsAsciiChar(c & 0x000f);
      } else {
        mWriter.WriteBuffer(aStr + i, 1);
      }
    }
  }

  void WriteHexDigitAsAsciiChar(uint8_t u) {
    char buf[1];
    buf[0] = static_cast<unsigned>((u < 10) ? '0' + u : 'a' + (u - 10));
    mWriter.WriteBuffer(buf, 1);
  }

  PlatformWriter& mWriter;
  bool mEmpty;
};

class BinaryAnnotationWriter : public AnnotationWriter {
 public:
  explicit BinaryAnnotationWriter(PlatformWriter& aPlatformWriter)
      : mPlatformWriter(aPlatformWriter) {}

  void Write(Annotation aAnnotation, const char* aValue,
             size_t aLen = 0) override {
    uint64_t len = aLen ? aLen : my_strlen(aValue);
    mPlatformWriter.WriteBuffer((const char*)&aAnnotation, sizeof(aAnnotation));
    mPlatformWriter.WriteBuffer((const char*)&len, sizeof(len));
    mPlatformWriter.WriteBuffer(aValue, len);
  };

  void Write(Annotation aAnnotation, uint64_t aValue) override {
    char buffer[32] = {};
    XP_STOA(aValue, buffer);
    Write(aAnnotation, buffer);
  };

 private:
  PlatformWriter& mPlatformWriter;
};

#ifdef MOZ_PHC

// 21 is the max length of a 64-bit decimal address entry, including the
// trailing comma or '\0'. And then we add another 32 just to be safe.
const size_t phcStringifiedAnnotationSize =
    (mozilla::phc::StackTrace::kMaxFrames * 21) + 32;

static void PHCStackTraceToString(char* aBuffer, size_t aBufferLen,
                                  const phc::StackTrace& aStack) {
  char addrString[32];
  *aBuffer = 0;
  for (size_t i = 0; i < aStack.mLength; i++) {
    if (i != 0) {
      strcat(aBuffer, ",");
    }
    XP_STOA(uintptr_t(aStack.mPcs[i]), addrString);
    strncat(aBuffer, addrString, aBufferLen);
  }
}

// The stack traces are encoded as a comma-separated list of decimal
// (not hexadecimal!) addresses, e.g. "12345678,12345679,12345680".
static void WritePHCStackTrace(AnnotationWriter& aWriter,
                               const Annotation aName,
                               const Maybe<phc::StackTrace>& aStack) {
  if (aStack.isNothing()) {
    return;
  }

  // 21 is the max length of a 64-bit decimal address entry, including the
  // trailing comma or '\0'. And then we add another 32 just to be safe.
  char addrsString[phcStringifiedAnnotationSize];
  PHCStackTraceToString(addrsString, sizeof(addrsString), *aStack);
  aWriter.Write(aName, addrsString);
}

static void WritePHCAddrInfo(AnnotationWriter& writer,
                             const phc::AddrInfo* aAddrInfo) {
  // Is this a PHC allocation needing special treatment?
  if (aAddrInfo && aAddrInfo->mKind != phc::AddrInfo::Kind::Unknown) {
    const char* kindString;
    switch (aAddrInfo->mKind) {
      case phc::AddrInfo::Kind::Unknown:
        kindString = "Unknown(?!)";
        break;
      case phc::AddrInfo::Kind::NeverAllocatedPage:
        kindString = "NeverAllocatedPage";
        break;
      case phc::AddrInfo::Kind::InUsePage:
        kindString = "InUsePage(?!)";
        break;
      case phc::AddrInfo::Kind::FreedPage:
        kindString = "FreedPage";
        break;
      case phc::AddrInfo::Kind::GuardPage:
        kindString = "GuardPage";
        break;
      default:
        kindString = "Unmatched(?!)";
        break;
    }
    writer.Write(Annotation::PHCKind, kindString);
    writer.Write(Annotation::PHCBaseAddress,
                 reinterpret_cast<uint64_t>(aAddrInfo->mBaseAddr));
    writer.Write(Annotation::PHCUsableSize,
                 static_cast<uint64_t>(aAddrInfo->mUsableSize));

    WritePHCStackTrace(writer, Annotation::PHCAllocStack,
                       aAddrInfo->mAllocStack);
    WritePHCStackTrace(writer, Annotation::PHCFreeStack, aAddrInfo->mFreeStack);
  }
}

static void PopulatePHCStackTraceAnnotation(
    AnnotationTable& aAnnotations, const Annotation aName,
    const Maybe<phc::StackTrace>& aStack) {
  if (aStack.isNothing()) {
    return;
  }

  char addrsString[phcStringifiedAnnotationSize];
  PHCStackTraceToString(addrsString, sizeof(addrsString), *aStack);
  aAnnotations[aName] = addrsString;
}

static void PopulatePHCAnnotations(AnnotationTable& aAnnotations,
                                   const phc::AddrInfo* aAddrInfo) {
  // Is this a PHC allocation needing special treatment?
  if (aAddrInfo && aAddrInfo->mKind != phc::AddrInfo::Kind::Unknown) {
    const char* kindString;
    switch (aAddrInfo->mKind) {
      case phc::AddrInfo::Kind::Unknown:
        kindString = "Unknown(?!)";
        break;
      case phc::AddrInfo::Kind::NeverAllocatedPage:
        kindString = "NeverAllocatedPage";
        break;
      case phc::AddrInfo::Kind::InUsePage:
        kindString = "InUsePage(?!)";
        break;
      case phc::AddrInfo::Kind::FreedPage:
        kindString = "FreedPage";
        break;
      case phc::AddrInfo::Kind::GuardPage:
        kindString = "GuardPage";
        break;
      default:
        kindString = "Unmatched(?!)";
        break;
    }

    aAnnotations[Annotation::PHCKind] = kindString;
    aAnnotations[Annotation::PHCBaseAddress] =
        nsPrintfCString("%zu", uintptr_t(aAddrInfo->mBaseAddr));
    aAnnotations[Annotation::PHCUsableSize] =
        nsPrintfCString("%zu", aAddrInfo->mUsableSize);
    PopulatePHCStackTraceAnnotation(aAnnotations, Annotation::PHCAllocStack,
                                    aAddrInfo->mAllocStack);
    PopulatePHCStackTraceAnnotation(aAnnotations, Annotation::PHCFreeStack,
                                    aAddrInfo->mFreeStack);
  }
}
#endif

/**
 * If minidump_id is null, we assume that dump_path contains the full
 * dump file path.
 */
static void OpenAPIData(PlatformWriter& aWriter, const XP_CHAR* dump_path,
                        const XP_CHAR* minidump_id = nullptr) {
  static XP_CHAR extraDataPath[XP_PATH_MAX];
  size_t size = XP_PATH_MAX;
  XP_CHAR* p;
  if (minidump_id) {
    p = Concat(extraDataPath, dump_path, &size);
    p = Concat(p, XP_PATH_SEPARATOR, &size);
    p = Concat(p, minidump_id, &size);
  } else {
    p = Concat(extraDataPath, dump_path, &size);
    // Skip back past the .dmp extension, if any.
    if (*(p - 4) == XP_TEXT('.')) {
      p -= 4;
      size += 4;
    }
  }
  Concat(p, extraFileExtension, &size);
  aWriter.Open(extraDataPath);
}

#ifdef XP_WIN
static void AnnotateMemoryStatus(AnnotationWriter& aWriter) {
  MEMORYSTATUSEX statex;
  statex.dwLength = sizeof(statex);
  if (GlobalMemoryStatusEx(&statex)) {
    aWriter.Write(Annotation::SystemMemoryUsePercentage,
                  static_cast<uint64_t>(statex.dwMemoryLoad));
    aWriter.Write(Annotation::TotalVirtualMemory, statex.ullTotalVirtual);
    aWriter.Write(Annotation::AvailableVirtualMemory, statex.ullAvailVirtual);
    aWriter.Write(Annotation::TotalPhysicalMemory, statex.ullTotalPhys);
    aWriter.Write(Annotation::AvailablePhysicalMemory, statex.ullAvailPhys);
  }

  PERFORMANCE_INFORMATION info;
  if (K32GetPerformanceInfo(&info, sizeof(info))) {
    aWriter.Write(Annotation::TotalPageFile,
                  static_cast<uint64_t>(info.CommitLimit * info.PageSize));
    aWriter.Write(Annotation::AvailablePageFile,
                  static_cast<uint64_t>((info.CommitLimit - info.CommitTotal) *
                                        info.PageSize));
  }
}
#elif XP_MACOSX
// Extract the total physical memory of the system.
static void WritePhysicalMemoryStatus(AnnotationWriter& aWriter) {
  uint64_t physicalMemoryByteSize = 0;
  const size_t NAME_LEN = 2;
  int name[NAME_LEN] = {/* Hardware */ CTL_HW,
                        /* 64-bit physical memory size */ HW_MEMSIZE};
  size_t infoByteSize = sizeof(physicalMemoryByteSize);
  if (sysctl(name, NAME_LEN, &physicalMemoryByteSize, &infoByteSize,
             /* We do not replace data */ nullptr,
             /* We do not replace data */ 0) != -1) {
    aWriter.Write(Annotation::TotalPhysicalMemory, physicalMemoryByteSize);
  }
}

// Extract available and purgeable physical memory.
static void WriteAvailableMemoryStatus(AnnotationWriter& aWriter) {
  auto host = mach_host_self();
  vm_statistics64_data_t stats;
  unsigned int count = HOST_VM_INFO64_COUNT;
  if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&stats, &count) ==
      KERN_SUCCESS) {
    aWriter.Write(Annotation::AvailablePhysicalMemory,
                  static_cast<uint64_t>(stats.free_count * vm_page_size));
    aWriter.Write(Annotation::PurgeablePhysicalMemory,
                  static_cast<uint64_t>(stats.purgeable_count * vm_page_size));
  }
}

// Extract the status of the swap.
static void WriteSwapFileStatus(AnnotationWriter& aWriter) {
  const size_t NAME_LEN = 2;
  int name[] = {/* Hardware */ CTL_VM,
                /* 64-bit physical memory size */ VM_SWAPUSAGE};
  struct xsw_usage swapUsage;
  size_t infoByteSize = sizeof(swapUsage);
  if (sysctl(name, NAME_LEN, &swapUsage, &infoByteSize,
             /* We do not replace data */ nullptr,
             /* We do not replace data */ 0) != -1) {
    aWriter.Write(Annotation::AvailableSwapMemory, swapUsage.xsu_avail);
  }
}
static void AnnotateMemoryStatus(AnnotationWriter& aWriter) {
  WritePhysicalMemoryStatus(aWriter);
  WriteAvailableMemoryStatus(aWriter);
  WriteSwapFileStatus(aWriter);
}

#elif XP_LINUX

static void AnnotateMemoryStatus(AnnotationWriter& aWriter) {
  // We can't simply call `sysinfo` as this requires libc.
  // So we need to parse /proc/meminfo.

  // We read the entire file to memory prior to parsing
  // as it makes the parser code a little bit simpler.
  // As /proc/meminfo is synchronized via `proc_create_single`,
  // there's no risk of race condition regardless of how we
  // read it.

  // The buffer in which we're going to load the entire file.
  // A typical size for /proc/meminfo is 1KiB, so 4KiB should
  // be large enough until further notice.
  const size_t BUFFER_SIZE_BYTES = 4096;
  char buffer[BUFFER_SIZE_BYTES];

  size_t bufferLen = 0;
  {
    // Read and load into memory.
    int fd = sys_open("/proc/meminfo", O_RDONLY, /* chmod */ 0);
    if (fd == -1) {
      // No /proc/meminfo? Well, fail silently.
      return;
    }
    auto Guard = MakeScopeExit([fd]() { mozilla::Unused << sys_close(fd); });

    ssize_t bytesRead = 0;
    do {
      if ((bytesRead = sys_read(fd, buffer + bufferLen,
                                BUFFER_SIZE_BYTES - bufferLen)) < 0) {
        if ((errno == EAGAIN) || (errno == EINTR)) {
          continue;
        }

        // Cannot read for some reason. Let's give up.
        return;
      }

      bufferLen += bytesRead;

      if (bufferLen == BUFFER_SIZE_BYTES) {
        // The file is too large, bail out
        return;
      }
    } while (bytesRead != 0);
  }

  // Each line of /proc/meminfo looks like
  // SomeLabel:       number unit
  // The last line is empty.
  // Let's write a parser.
  // Note that we don't care about writing a normative parser, so
  // we happily skip whitespaces without checking that it's necessary.

  // A stack-allocated structure containing a 0-terminated string.
  // We could avoid the memory copies and make it a slice at the cost
  // of a slightly more complicated parser. Since we're not in a
  // performance-critical section, we didn't.
  struct DataBuffer {
    DataBuffer() : data{0}, pos(0) {}
    // Clear the buffer.
    void reset() {
      pos = 0;
      data[0] = 0;
    }
    // Append a character.
    //
    // In case of error (if c is '\0' or the buffer is full), does nothing.
    void append(char c) {
      if (c == 0 || pos >= sizeof(data) - 1) {
        return;
      }
      data[pos++] = c;
      data[pos] = 0;
    }
    // Compare the buffer against a nul-terminated string.
    bool operator==(const char* s) const {
      for (size_t i = 0; i < pos; ++i) {
        if (s[i] != data[i]) {
          // Note: Since `data` never contains a '0' in positions [0,pos)
          // this will bailout once we have reached the end of `s`.
          return false;
        }
      }
      return true;
    }

    // A NUL-terminated string of `pos + 1` chars (the +1 is for the 0).
    char data[256];

    // Invariant: < 256.
    size_t pos;
  };

  // A DataBuffer holding the string representation of a non-negative number.
  struct NumberBuffer : DataBuffer {
    // If possible, convert the string into a number.
    // Returns `true` in case of success, `false` in case of failure.
    bool asNumber(size_t* number) {
      int result;
      if (!my_strtoui(&result, data)) {
        return false;
      }
      *number = result;
      return true;
    }
  };

  // A DataBuffer holding the string representation of a unit. As of this
  // writing, we only support unit `kB`, which seems to be the only unit used in
  // `/proc/meminfo`.
  struct UnitBuffer : DataBuffer {
    // If possible, convert the string into a multiplier, e.g. `kB => 1024`.
    // Return `true` in case of success, `false` in case of failure.
    bool asMultiplier(size_t* multiplier) {
      if (*this == "kB") {
        *multiplier = 1024;
        return true;
      }
      // Other units don't seem to be specified/used.
      return false;
    }
  };

  // The state of the mini-parser.
  enum class State {
    // Reading the label, including the trailing ':'.
    Label,
    // Reading the number, ignoring any whitespace.
    Number,
    // Reading the unit, ignoring any whitespace.
    Unit,
  };

  // A single measure being read from /proc/meminfo, e.g.
  // the total physical memory available on the system.
  struct Measure {
    Measure() : state(State::Label) {}
    // Reset the measure for a new read.
    void reset() {
      state = State::Label;
      label.reset();
      number.reset();
      unit.reset();
    }
    // Attempt to convert the measure into a number.
    // Return `true` if both the number and the multiplier could be
    // converted, `false` otherwise.
    // In case of overflow, produces the maximal possible `size_t`.
    bool asValue(size_t* result) {
      size_t numberAsSize = 0;
      if (!number.asNumber(&numberAsSize)) {
        return false;
      }
      size_t unitAsMultiplier = 0;
      if (!unit.asMultiplier(&unitAsMultiplier)) {
        return false;
      }
      if (numberAsSize * unitAsMultiplier >= numberAsSize) {
        *result = numberAsSize * unitAsMultiplier;
      } else {
        // Overflow. Unlikely, but just in case, let's return
        // the maximal possible value.
        *result = size_t(-1);
      }
      return true;
    }

    // The label being read, e.g. `MemFree`. Does not include the trailing ':'.
    DataBuffer label;

    // The number being read, e.g. "1024".
    NumberBuffer number;

    // The unit being read, e.g. "kB".
    UnitBuffer unit;

    // What we're reading at the moment.
    State state;
  };

  // A value we wish to store for later processing.
  // e.g. to compute `AvailablePageFile`, we need to
  // store `CommitLimit` and `Committed_AS`.
  struct ValueStore {
    ValueStore() : value(0), found(false) {}
    size_t value;
    bool found;
  };
  ValueStore commitLimit;
  ValueStore committedAS;
  ValueStore memTotal;
  ValueStore swapTotal;

  // The current measure.
  Measure measure;

  for (size_t pos = 0; pos < size_t(bufferLen); ++pos) {
    const char c = buffer[pos];
    switch (measure.state) {
      case State::Label:
        if (c == ':') {
          // We have finished reading the label.
          measure.state = State::Number;
        } else {
          measure.label.append(c);
        }
        break;
      case State::Number:
        if (c == ' ') {
          // Ignore whitespace
        } else if ('0' <= c && c <= '9') {
          // Accumulate numbers.
          measure.number.append(c);
        } else {
          // We have jumped to the unit.
          measure.unit.append(c);
          measure.state = State::Unit;
        }
        break;
      case State::Unit:
        if (c == ' ') {
          // Ignore whitespace
        } else if (c == '\n') {
          // Flush line.
          // - If this one of the measures we're interested in, write it.
          // - Once we're done, reset the parser.
          auto Guard = MakeScopeExit([&measure]() { measure.reset(); });

          struct PointOfInterest {
            // The label we're looking for, e.g. "MemTotal".
            const char* label;
            // If non-nullptr, store the value at this address.
            ValueStore* dest;
            // If other than Annotation::Count, write the value for this
            // annotation.
            Annotation annotation;
          };
          const PointOfInterest POINTS_OF_INTEREST[] = {
              {"MemTotal", &memTotal, Annotation::TotalPhysicalMemory},
              {"MemFree", nullptr, Annotation::AvailablePhysicalMemory},
              {"MemAvailable", nullptr, Annotation::AvailableVirtualMemory},
              {"SwapFree", nullptr, Annotation::AvailableSwapMemory},
              {"SwapTotal", &swapTotal, Annotation::Count},
              {"CommitLimit", &commitLimit, Annotation::Count},
              {"Committed_AS", &committedAS, Annotation::Count},
          };
          for (const auto& pointOfInterest : POINTS_OF_INTEREST) {
            if (measure.label == pointOfInterest.label) {
              size_t value;
              if (measure.asValue(&value)) {
                if (pointOfInterest.dest != nullptr) {
                  pointOfInterest.dest->found = true;
                  pointOfInterest.dest->value = value;
                }
                if (pointOfInterest.annotation != Annotation::Count) {
                  aWriter.Write(pointOfInterest.annotation,
                                static_cast<uint64_t>(value));
                }
              }
              break;
            }
          }
          // Otherwise, ignore.
        } else {
          measure.unit.append(c);
        }
        break;
    }
  }

  if (commitLimit.found && committedAS.found) {
    // If available, attempt to determine the available virtual memory.
    // As `commitLimit` is not guaranteed to be larger than `committedAS`,
    // we return `0` in case the commit limit has already been exceeded.
    uint64_t availablePageFile = (committedAS.value <= commitLimit.value)
                                     ? (commitLimit.value - committedAS.value)
                                     : 0;
    aWriter.Write(Annotation::AvailablePageFile, availablePageFile);
  }
  if (memTotal.found && swapTotal.found) {
    // If available, attempt to determine the available virtual memory.
    aWriter.Write(Annotation::TotalPageFile,
                  static_cast<uint64_t>(memTotal.value + swapTotal.value));
  }
}

#else

static void AnnotateMemoryStatus(AnnotationTable&) {
  // No memory data for other platforms yet.
}

#endif  // XP_WIN || XP_MACOSX || XP_LINUX || else

#if !defined(MOZ_WIDGET_ANDROID)

/**
 * Launches the program specified in aProgramPath with aMinidumpPath as its
 * sole argument.
 *
 * @param aProgramPath The path of the program to be launched
 * @param aMinidumpPath The path of the minidump file, passed as an argument
 *        to the launched program
 */
static bool LaunchProgram(const XP_CHAR* aProgramPath,
                          const XP_CHAR* aMinidumpPath) {
#  ifdef XP_WIN
  XP_CHAR cmdLine[CMDLINE_SIZE];
  XP_CHAR* p;

  size_t size = CMDLINE_SIZE;
  p = Concat(cmdLine, L"\"", &size);
  p = Concat(p, aProgramPath, &size);
  p = Concat(p, L"\" \"", &size);
  p = Concat(p, aMinidumpPath, &size);
  Concat(p, L"\"", &size);

  PROCESS_INFORMATION pi = {};
  STARTUPINFO si = {};
  si.cb = sizeof(si);

  // If CreateProcess() fails don't do anything.
  if (CreateProcess(
          /* lpApplicationName */ nullptr, (LPWSTR)cmdLine,
          /* lpProcessAttributes */ nullptr, /* lpThreadAttributes */ nullptr,
          /* bInheritHandles */ FALSE,
          NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB,
          /* lpEnvironment */ nullptr, /* lpCurrentDirectory */ nullptr, &si,
          &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
#  elif defined(XP_MACOSX)
  pid_t pid = 0;
  char* const my_argv[] = {const_cast<char*>(aProgramPath),
                           const_cast<char*>(aMinidumpPath), nullptr};

  char** env = nullptr;
  char*** nsEnv = _NSGetEnviron();
  if (nsEnv) {
    env = *nsEnv;
  }

  int rv = posix_spawnp(&pid, my_argv[0], nullptr, nullptr, my_argv, env);

  if (rv != 0) {
    return false;
  }
#  else   // !XP_MACOSX
  pid_t pid = sys_fork();

  if (pid == -1) {
    return false;
  } else if (pid == 0) {
    Unused << execl(aProgramPath, aProgramPath, aMinidumpPath, nullptr);
    _exit(1);
  }
#  endif  // XP_MACOSX

  return true;
}

#else

/**
 * Launch the crash reporter activity on Android
 *
 * @param aProgramPath The path of the program to be launched
 * @param aMinidumpPath The path to the crash minidump file
 */

static bool LaunchCrashHandlerService(const XP_CHAR* aProgramPath,
                                      const XP_CHAR* aMinidumpPath) {
  static XP_CHAR extrasPath[XP_PATH_MAX];
  size_t size = XP_PATH_MAX;

  XP_CHAR* p = Concat(extrasPath, aMinidumpPath, &size);
  p = Concat(p - 3, "extra", &size);

  pid_t pid = sys_fork();

  if (pid == -1)
    return false;
  else if (pid == 0) {
    // Invoke the crash handler service using am
    if (androidUserSerial) {
      Unused << execlp("/system/bin/am", "/system/bin/am",
                       androidStartServiceCommand, "--user", androidUserSerial,
                       "-a", "org.mozilla.gecko.ACTION_CRASHED", "-n",
                       aProgramPath, "--es", "minidumpPath", aMinidumpPath,
                       "--es", "extrasPath", extrasPath, "--ez", "fatal",
                       "true", "--es", "processType", "MAIN", (char*)0);
    } else {
      Unused << execlp(
          "/system/bin/am", "/system/bin/am", androidStartServiceCommand, "-a",
          "org.mozilla.gecko.ACTION_CRASHED", "-n", aProgramPath, "--es",
          "minidumpPath", aMinidumpPath, "--es", "extrasPath", extrasPath,
          "--ez", "fatal", "true", "--es", "processType", "MAIN", (char*)0);
    }
    _exit(1);

  } else {
    // We need to wait on the 'am start' command above to finish, otherwise
    // everything will be killed by the ActivityManager as soon as the signal
    // handler exits
    int status;
    Unused << HANDLE_EINTR(sys_waitpid(pid, &status, __WALL));
  }

  return true;
}

#endif

static void WriteAnnotations(AnnotationWriter& aWriter,
                             const AnnotationTable& aAnnotations) {
  for (auto key : MakeEnumeratedRange(Annotation::Count)) {
    const nsCString& value = aAnnotations[key];
    if (!value.IsEmpty()) {
      aWriter.Write(key, value.get(), value.Length());
    }
  }
}

static void WriteSynthesizedAnnotations(AnnotationWriter& aWriter) {
  AnnotateMemoryStatus(aWriter);
}

static void WriteAnnotationsForMainProcessCrash(PlatformWriter& pw,
                                                const phc::AddrInfo* addrInfo,
                                                time_t crashTime) {
  JSONAnnotationWriter writer(pw);

  for (auto key : MakeEnumeratedRange(Annotation::Count)) {
    AnnotationContents contents = {};
    size_t address =
        mozannotation_get_contents(static_cast<uint32_t>(key), &contents);
    if (address != 0) {
      switch (TypeOfAnnotation(key)) {
        case AnnotationType::String:
          switch (contents.tag) {
            case AnnotationContents::Tag::NSCStringPointer: {
              const nsCString* string =
                  reinterpret_cast<const nsCString*>(address);
              writer.Write(key, string->Data(), string->Length());
            } break;
            case AnnotationContents::Tag::CStringPointer:
              address = *(reinterpret_cast<size_t*>(address));
              if (address == 0) {
                break;
              }
              // FALLTHROUGH
            case AnnotationContents::Tag::CString: {
              writer.Write(key, reinterpret_cast<const char*>(address));
            } break;
            case AnnotationContents::Tag::ByteBuffer:
              writer.Write(key, reinterpret_cast<const char*>(address),
                           static_cast<size_t>(contents.byte_buffer._0));
              break;
            case AnnotationContents::Tag::OwnedByteBuffer:
              writer.Write(key, reinterpret_cast<const char*>(address),
                           static_cast<size_t>(contents.owned_byte_buffer._0));
              break;
            case AnnotationContents::Tag::Empty:
              break;
          }
          break;
        case AnnotationType::Boolean:
          writer.Write(key, *reinterpret_cast<const bool*>(address));
          break;
        case AnnotationType::U32:
          writer.Write(key, static_cast<uint64_t>(
                                *reinterpret_cast<uint32_t*>(address)));
          break;
        case AnnotationType::U64:
          writer.Write(key, *reinterpret_cast<uint64_t*>(address));
          break;
        case AnnotationType::USize:
          writer.Write(
              key, static_cast<uint64_t>(*reinterpret_cast<size_t*>(address)));
          break;
      }
    }
  }

  WriteSynthesizedAnnotations(writer);
  writer.Write(Annotation::CrashTime, uint64_t(crashTime));

  if (inactiveStateStart) {
    writer.Write(Annotation::LastInteractionDuration,
                 static_cast<uint64_t>(crashTime - inactiveStateStart));
  }

  double uptimeTS = (TimeStamp::NowLoRes() - TimeStamp::ProcessCreation())
                        .ToSecondsSigDigits();
  char uptimeTSString[64] = {};
  SimpleNoCLibDtoA(uptimeTS, uptimeTSString, sizeof(uptimeTSString));
  writer.Write(Annotation::UptimeTS, uptimeTSString);

  // calculate time since last crash (if possible).
  if (lastCrashTime != 0) {
    uint64_t timeSinceLastCrash = crashTime - lastCrashTime;

    writer.Write(Annotation::SecondsSinceLastCrash, timeSinceLastCrash);
  }

#if defined(XP_WIN) && defined(HAS_DLL_BLOCKLIST)
  // HACK: The DLL blocklist code will manually write its annotations as JSON
  DllBlocklist_WriteNotes();
#endif  // defined(XP_WIN) && defined(HAS_DLL_BLOCKLIST)

#ifdef MOZ_PHC
  WritePHCAddrInfo(writer, addrInfo);
#endif
}

static void WriteCrashEventFile(time_t crashTime, const char* crashTimeString,
                                const phc::AddrInfo* addrInfo,
#ifdef XP_LINUX
                                const MinidumpDescriptor& descriptor
#else
                                const XP_CHAR* minidump_id
#endif
) {
  // Minidump IDs are UUIDs (36) + NULL.
  static char id_ascii[37] = {};
#ifdef XP_LINUX
  const char* index = strrchr(descriptor.path(), '/');
  MOZ_ASSERT(index);
  MOZ_ASSERT(strlen(index) == 1 + 36 + 4);  // "/" + UUID + ".dmp"
  for (uint32_t i = 0; i < 36; i++) {
    id_ascii[i] = *(index + 1 + i);
  }
#else
  MOZ_ASSERT(XP_STRLEN(minidump_id) == 36);
  for (uint32_t i = 0; i < 36; i++) {
    id_ascii[i] = *((char*)(minidump_id + i));
  }
#endif

  PlatformWriter eventFile;

  if (!eventsDirectory.empty()) {
    static XP_CHAR crashEventPath[XP_PATH_MAX];
    size_t size = XP_PATH_MAX;
    XP_CHAR* p;
    p = Concat(crashEventPath, eventsDirectory.c_str(), &size);
    p = Concat(p, XP_PATH_SEPARATOR, &size);
#ifdef XP_LINUX
    Concat(p, id_ascii, &size);
#else
    Concat(p, minidump_id, &size);
#endif

    eventFile.Open(crashEventPath);
    eventFile.WriteLiteral(kCrashMainID);
    eventFile.WriteString(crashTimeString);
    eventFile.WriteLiteral("\n");
    eventFile.WriteString(id_ascii);
    eventFile.WriteLiteral("\n");
    WriteAnnotationsForMainProcessCrash(eventFile, addrInfo, crashTime);
  }
}

// Callback invoked from breakpad's exception handler, this writes out the
// last annotations after a crash occurs and launches the crash reporter client.
//
// This function is not declared static even though it's not used outside of
// this file because of an issue in Fennec which prevents breakpad's exception
// handler from invoking it. See bug 1424304.
bool MinidumpCallback(
#ifdef XP_LINUX
    const MinidumpDescriptor& descriptor,
#else
    const XP_CHAR* dump_path, const XP_CHAR* minidump_id,
#endif
    void* context,
#ifdef XP_WIN
    EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion,
#endif
    const phc::AddrInfo* addrInfo, bool succeeded) {
  bool returnValue = showOSCrashReporter ? false : succeeded;

  static XP_CHAR minidumpPath[XP_PATH_MAX];
  size_t size = XP_PATH_MAX;
  XP_CHAR* p;
#ifndef XP_LINUX
  p = Concat(minidumpPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
  Concat(p, dumpFileExtension, &size);
#else
  Concat(minidumpPath, descriptor.path(), &size);
#endif

  static XP_CHAR memoryReportLocalPath[XP_PATH_MAX];
  size = XP_PATH_MAX;
#ifndef XP_LINUX
  p = Concat(memoryReportLocalPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
#else
  p = Concat(memoryReportLocalPath, descriptor.path(), &size);
  // Skip back past the .dmp extension
  p -= 4;
#endif
  Concat(p, memoryReportExtension, &size);

  if (!memoryReportPath.empty()) {
#ifdef XP_WIN
    CopyFile(memoryReportPath.c_str(), memoryReportLocalPath, false);
#else
    copy_file(memoryReportPath.c_str(), memoryReportLocalPath);
#endif
  }

  time_t crashTime = GetCurrentTimeForCrashTime();
  char crashTimeString[32];
  XP_TTOA(crashTime, crashTimeString);

  // write crash time to file
  if (lastCrashTimeFilename[0] != 0) {
    PlatformWriter lastCrashFile(lastCrashTimeFilename);
    lastCrashFile.WriteString(crashTimeString);
  }

  WriteCrashEventFile(crashTime, crashTimeString, addrInfo,
#ifdef XP_LINUX
                      descriptor
#else
                      minidump_id
#endif
  );

  {
    PlatformWriter apiData;
#ifdef XP_LINUX
    OpenAPIData(apiData, descriptor.path());
#else
    OpenAPIData(apiData, dump_path, minidump_id);
#endif
    WriteAnnotationsForMainProcessCrash(apiData, addrInfo, crashTime);
  }

  if (doReport && isSafeToDump) {
    // We launch the crash reporter client/dialog only if we've been explicitly
    // asked to report crashes and if we weren't already trying to unset the
    // exception handler (which is indicated by isSafeToDump being false).
#if defined(MOZ_WIDGET_ANDROID)  // Android
    returnValue =
        LaunchCrashHandlerService(crashReporterPath.c_str(), minidumpPath);
#else  // Windows, Mac, Linux, etc...
    returnValue = LaunchProgram(crashReporterPath.c_str(), minidumpPath);
#endif
  }

#ifdef XP_WIN
  TerminateProcess(GetCurrentProcess(), 1);
#endif

  return returnValue;
}

#if defined(XP_MACOSX) || defined(__ANDROID__) || defined(XP_LINUX)
static size_t EnsureTrailingSlash(XP_CHAR* aBuf, size_t aBufLen) {
  size_t len = XP_STRLEN(aBuf);
  if ((len + 1) < aBufLen && len > 0 &&
      aBuf[len - 1] != XP_PATH_SEPARATOR_CHAR) {
    aBuf[len] = XP_PATH_SEPARATOR_CHAR;
    ++len;
    aBuf[len] = 0;
  }
  return len;
}
#endif

#if defined(XP_WIN)

static size_t BuildTempPath(wchar_t* aBuf, size_t aBufLen) {
  // first figure out buffer size
  DWORD pathLen = GetTempPath(0, nullptr);
  if (pathLen == 0 || pathLen >= aBufLen) {
    return 0;
  }

  return GetTempPath(pathLen, aBuf);
}

static size_t BuildTempPath(char16_t* aBuf, size_t aBufLen) {
  return BuildTempPath(reinterpret_cast<wchar_t*>(aBuf), aBufLen);
}

#elif defined(XP_MACOSX)

static size_t BuildTempPath(char* aBuf, size_t aBufLen) {
  if (aBufLen < PATH_MAX) {
    return 0;
  }

  FSRef fsRef;
  OSErr err =
      FSFindFolder(kUserDomain, kTemporaryFolderType, kCreateFolder, &fsRef);
  if (err != noErr) {
    return 0;
  }

  OSStatus status = FSRefMakePath(&fsRef, (UInt8*)aBuf, PATH_MAX);
  if (status != noErr) {
    return 0;
  }

  return EnsureTrailingSlash(aBuf, aBufLen);
}

#elif defined(__ANDROID__)

static size_t BuildTempPath(char* aBuf, size_t aBufLen) {
  // GeckoAppShell sets this in the environment
  const char* tempenv = PR_GetEnv("TMPDIR");
  if (!tempenv) {
    return false;
  }
  size_t size = aBufLen;
  Concat(aBuf, tempenv, &size);
  return EnsureTrailingSlash(aBuf, aBufLen);
}

#elif defined(XP_UNIX)

static size_t BuildTempPath(char* aBuf, size_t aBufLen) {
  const char* tempenv = PR_GetEnv("TMPDIR");
  const char* tmpPath = "/tmp/";
  if (!tempenv) {
    tempenv = tmpPath;
  }
  size_t size = aBufLen;
  Concat(aBuf, tempenv, &size);
  return EnsureTrailingSlash(aBuf, aBufLen);
}

#else
#  error "Implement this for your platform"
#endif

template <typename CharT, size_t N>
static size_t BuildTempPath(CharT (&aBuf)[N]) {
  static_assert(N >= XP_PATH_MAX, "char array length is too small");
  return BuildTempPath(&aBuf[0], N);
}

template <typename PathStringT>
static bool BuildTempPath(PathStringT& aResult) {
  aResult.SetLength(XP_PATH_MAX);
  size_t actualLen = BuildTempPath(aResult.BeginWriting(), XP_PATH_MAX);
  if (!actualLen) {
    return false;
  }
  aResult.SetLength(actualLen);
  return true;
}

#ifdef XP_WIN

static void ReserveBreakpadVM() {
  if (!gBreakpadReservedVM) {
    gBreakpadReservedVM =
        VirtualAlloc(nullptr, kReserveSize, MEM_RESERVE, PAGE_NOACCESS);
  }
}

static void FreeBreakpadVM() {
  if (gBreakpadReservedVM) {
    VirtualFree(gBreakpadReservedVM, 0, MEM_RELEASE);
  }
}

static bool IsCrashingException(EXCEPTION_POINTERS* exinfo) {
  if (!exinfo) {
    return true;
  }

  PEXCEPTION_RECORD e = (PEXCEPTION_RECORD)exinfo->ExceptionRecord;
  switch (e->ExceptionCode) {
    case STATUS_FLOAT_DENORMAL_OPERAND:
    case STATUS_FLOAT_DIVIDE_BY_ZERO:
    case STATUS_FLOAT_INEXACT_RESULT:
    case STATUS_FLOAT_INVALID_OPERATION:
    case STATUS_FLOAT_OVERFLOW:
    case STATUS_FLOAT_STACK_CHECK:
    case STATUS_FLOAT_UNDERFLOW:
    case STATUS_FLOAT_MULTIPLE_FAULTS:
    case STATUS_FLOAT_MULTIPLE_TRAPS:
      return false;  // Don't write minidump, continue exception search
    default:
      return true;
  }
}

#endif  // XP_WIN

// Do various actions to prepare the child process for minidump generation.
// This includes disabling the I/O interposer and DLL blocklist which both
// would get in the way. We also free the address space we had reserved in
// 32-bit builds to free room for the minidump generation to do its work.
static void PrepareForMinidump() {
  mozilla::IOInterposer::Disable();
#if defined(XP_WIN)
#  if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
  DllBlocklist_Shutdown();
#  endif
  FreeBreakpadVM();
#endif  // XP_WIN
}

#ifdef XP_WIN

/**
 * Filters out floating point exceptions which are handled by nsSigHandlers.cpp
 * and should not be handled as crashes.
 */
static ExceptionHandler::FilterResult Filter(void* context,
                                             EXCEPTION_POINTERS* exinfo,
                                             MDRawAssertionInfo* assertion) {
  if (!IsCrashingException(exinfo)) {
    return ExceptionHandler::FilterResult::ContinueSearch;
  }

  PrepareForMinidump();
  return ExceptionHandler::FilterResult::HandleException;
}

static ExceptionHandler::FilterResult ChildFilter(
    void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion) {
  if (!IsCrashingException(exinfo)) {
    return ExceptionHandler::FilterResult::ContinueSearch;
  }

  if (gEncounteredChildException.exchange(true)) {
    return ExceptionHandler::FilterResult::AbortWithoutMinidump;
  }

  PrepareForMinidump();
  return ExceptionHandler::FilterResult::HandleException;
}

static MINIDUMP_TYPE GetMinidumpType() {
  MINIDUMP_TYPE minidump_type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithFullMemoryInfo | MiniDumpWithUnloadedModules |
      MiniDumpWithHandleData);

#  ifdef NIGHTLY_BUILD
  minidump_type = static_cast<MINIDUMP_TYPE>(
      minidump_type |
      // This is Nightly only because this doubles the size of minidumps based
      // on the experimental data.
      MiniDumpWithProcessThreadData |
      // This allows us to examine heap objects referenced from stack objects
      // at the cost of further doubling the size of minidumps.
      MiniDumpWithIndirectlyReferencedMemory);
#  endif

  const char* e = PR_GetEnv("MOZ_CRASHREPORTER_FULLDUMP");
  if (e && *e) {
    minidump_type = MiniDumpWithFullMemory;
  }

  return minidump_type;
}

#else

static bool Filter(void* context) {
  PrepareForMinidump();
  return true;
}

static bool ChildFilter(void* context) {
  if (gEncounteredChildException.exchange(true)) {
    return false;
  }

  PrepareForMinidump();
  return true;
}

#endif  // !defined(XP_WIN)

static bool ChildMinidumpCallback(
#if defined(XP_WIN)
    const wchar_t* dump_path, const wchar_t* minidump_id,
#elif defined(XP_LINUX)
    const MinidumpDescriptor& descriptor,
#else  // defined(XP_MACOSX)
    const char* dump_dir, const char* minidump_id,
#endif
    void* context,
#if defined(XP_WIN)
    EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion,
#endif  // defined(XP_WIN)
    const mozilla::phc::AddrInfo* addr_info, bool succeeded) {
  return succeeded;
}

static bool ShouldReport() {
  // this environment variable prevents us from launching
  // the crash reporter client
  const char* envvar = PR_GetEnv("MOZ_CRASHREPORTER_NO_REPORT");
  if (envvar && *envvar) {
    return false;
  }

  envvar = PR_GetEnv("MOZ_CRASHREPORTER_FULLDUMP");
  if (envvar && *envvar) {
    return false;
  }

  return true;
}

static void TerminateHandler() { MOZ_CRASH("Unhandled exception"); }

#if !defined(MOZ_WIDGET_ANDROID)

// Locate the specified executable and store its path as a native string in
// the |aPath| so we can later invoke it from within the exception handler.
static nsresult LocateExecutable(nsIFile* aXREDirectory, const nsAString& aName,
                                 PathString& aPath) {
  nsCOMPtr<nsIFile> exePath;
  nsresult rv = aXREDirectory->Clone(getter_AddRefs(exePath));
  NS_ENSURE_SUCCESS(rv, rv);

#  ifdef XP_MACOSX
  exePath->SetNativeLeafName("MacOS"_ns);
  exePath->Append(u"crashreporter.app"_ns);
  exePath->Append(u"Contents"_ns);
  exePath->Append(u"MacOS"_ns);
#  endif

  exePath->Append(aName);
  aPath = exePath->NativePath();
  return NS_OK;
}

#endif  // !defined(MOZ_WIDGET_ANDROID)

static void InitializeAppNotes() {
  notesFieldLock = new Mutex("notesFieldLock");
  notesField = new nsCString();
}

// Register crash annotations that are present in both main and child processes
static void RegisterAnnotations() {
  mozannotation_register_cstring_ptr(
      static_cast<uint32_t>(Annotation::MozCrashReason), &gMozCrashReason);
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  mozannotation_register_cstring(
      static_cast<uint32_t>(Annotation::MainThreadRunnableName),
      &nsThread::sMainThreadRunnableName[0]);
#endif
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::EventLoopNestingLevel),
      &eventloopNestingLevel, sizeof(uint32_t));
  mozannotation_register_nscstring(static_cast<uint32_t>(Annotation::Notes),
                                   notesField);
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::OOMAllocationSize), &gOOMAllocationSize,
      sizeof(size_t));
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::IsGarbageCollecting),
      &isGarbageCollecting, sizeof(bool));
  mozannotation_register_nscstring(static_cast<uint32_t>(Annotation::ServerURL),
                                   &gServerURL);
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::TextureUsage), &gTexturesSize,
      sizeof(size_t));
#if defined(XP_WIN) && defined(HAS_DLL_BLOCKLIST)
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::BlocklistInitFailed),
      DllBlocklist_GetBlocklistInitFailedPointer(), sizeof(bool));
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::User32BeforeBlocklist),
      DllBlocklist_GetUser32BeforeBlocklistPointer(), sizeof(bool));
  mozannotation_register_cstring(
      static_cast<uint32_t>(Annotation::BlockedDllList),
      DllBlocklist_GetBlocklistWriterData());
#endif  // defined(XP_WIN) && defined(HAS_DLL_BLOCKLIST)
}

static void TeardownAppNotes() {
  delete notesFieldLock;
  notesFieldLock = nullptr;

  delete notesField;
  notesField = nullptr;
}

nsresult SetExceptionHandler(nsIFile* aXREDirectory, bool force /*=false*/) {
  if (gExceptionHandler) return NS_ERROR_ALREADY_INITIALIZED;

#if defined(DEBUG)
  // In debug builds, disable the crash reporter by default, and allow to
  // enable it with the MOZ_CRASHREPORTER environment variable.
  const char* envvar = PR_GetEnv("MOZ_CRASHREPORTER");
  if ((!envvar || !*envvar) && !force) return NS_OK;
#else
  // In other builds, enable the crash reporter by default, and allow
  // disabling it with the MOZ_CRASHREPORTER_DISABLE environment variable.
  const char* envvar = PR_GetEnv("MOZ_CRASHREPORTER_DISABLE");
  if (envvar && *envvar && !force) return NS_OK;
#endif

  // this environment variable prevents us from launching
  // the crash reporter client
  doReport = ShouldReport();

  RegisterRuntimeExceptionModule();
  InitializeAppNotes();
  RegisterAnnotations();

#if !defined(MOZ_WIDGET_ANDROID)
  // Locate the crash reporter executable
  PathString crashReporterPath_temp;
  nsresult rv = LocateExecutable(aXREDirectory, CRASH_REPORTER_FILENAME,
                                 crashReporterPath_temp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  crashReporterPath = crashReporterPath_temp.get();
#else
  // On Android, we launch a service defined via MOZ_ANDROID_CRASH_HANDLER
  const char* androidCrashHandler = PR_GetEnv("MOZ_ANDROID_CRASH_HANDLER");
  if (androidCrashHandler) {
    crashReporterPath = xpstring(androidCrashHandler);
  } else {
    NS_WARNING("No Android crash handler set");
  }

  const char* deviceAndroidVersion =
      PR_GetEnv("MOZ_ANDROID_DEVICE_SDK_VERSION");
  if (deviceAndroidVersion != nullptr) {
    const int deviceSdkVersion = atol(deviceAndroidVersion);
    if (deviceSdkVersion >= 26) {
      androidStartServiceCommand = (char*)"start-foreground-service";
    } else {
      androidStartServiceCommand = (char*)"startservice";
    }
  }
#endif  // !defined(MOZ_WIDGET_ANDROID)

  // get temp path to use for minidump path
  PathString tempPath;
  if (!BuildTempPath(tempPath)) {
    return NS_ERROR_FAILURE;
  }

#ifdef XP_WIN
  ReserveBreakpadVM();

  // Pre-load psapi.dll to prevent it from being loaded during exception
  // handling.
  ::LoadLibraryW(L"psapi.dll");
#endif  // XP_WIN

#ifdef MOZ_WIDGET_ANDROID
  androidUserSerial = getenv("MOZ_ANDROID_USER_SERIAL_NUMBER");
#endif

  // Initialize the flag and mutex used to avoid dump processing
  // once browser termination has begun.
  NS_ASSERTION(!dumpSafetyLock, "Shouldn't have a lock yet");
  // Do not deallocate this lock while it is still possible for
  // isSafeToDump to be tested on another thread.
  dumpSafetyLock = new Mutex("dumpSafetyLock");
  MutexAutoLock lock(*dumpSafetyLock);
  isSafeToDump = true;

  // now set the exception handler
#ifdef XP_LINUX
  MinidumpDescriptor descriptor(tempPath.get());
#endif

#ifdef XP_WIN
  previousUnhandledExceptionFilter = GetUnhandledExceptionFilter();
#endif

  gExceptionHandler = new google_breakpad::ExceptionHandler(
#ifdef XP_LINUX
      descriptor,
#elif defined(XP_WIN)
      std::wstring(tempPath.get()),
#else
                     tempPath.get(),
#endif

      Filter, MinidumpCallback, nullptr,
#ifdef XP_WIN
      google_breakpad::ExceptionHandler::HANDLER_ALL, GetMinidumpType(),
      (const wchar_t*)nullptr, nullptr);
#else
      true
#  ifdef XP_MACOSX
      ,
      nullptr
#  endif
#  ifdef XP_LINUX
      ,
      -1
#  endif
  );
#endif  // XP_WIN

  if (!gExceptionHandler) return NS_ERROR_OUT_OF_MEMORY;

#ifdef XP_WIN
  gExceptionHandler->set_handle_debug_exceptions(true);

  // Initially set sIncludeContextHeap to true for debugging startup crashes
  // even if the controlling pref value is false.
  SetIncludeContextHeap(true);
#  if defined(HAVE_64BIT_BUILD)
  // Tell JS about the new filter before we disable SetUnhandledExceptionFilter
  SetJitExceptionHandler();
#  endif

  RecordMainThreadId();

  // protect the crash reporter from being unloaded
  gBlockUnhandledExceptionFilter = true;
  gKernel32Intercept.Init("kernel32.dll");
  DebugOnly<bool> ok = stub_SetUnhandledExceptionFilter.Set(
      gKernel32Intercept, "SetUnhandledExceptionFilter",
      &patched_SetUnhandledExceptionFilter);

#  ifdef DEBUG
  if (!ok)
    printf_stderr(
        "SetUnhandledExceptionFilter hook failed; crash reporter is "
        "vulnerable.\n");
#  endif
#endif

  // store application start time
  RecordAnnotationU64(Annotation::StartupTime,
                      static_cast<uint64_t>(time(nullptr)));

#if defined(XP_MACOSX)
  // On OS X, many testers like to see the OS crash reporting dialog
  // since it offers immediate stack traces.  We allow them to set
  // a default to pass exceptions to the OS handler.
  Boolean keyExistsAndHasValidFormat = false;
  Boolean prefValue = ::CFPreferencesGetAppBooleanValue(
      CFSTR("OSCrashReporter"), kCFPreferencesCurrentApplication,
      &keyExistsAndHasValidFormat);
  if (keyExistsAndHasValidFormat) showOSCrashReporter = prefValue;
#endif

  oldTerminateHandler = std::set_terminate(&TerminateHandler);

  return NS_OK;
}

bool GetEnabled() { return gExceptionHandler != nullptr; }

bool GetMinidumpPath(nsAString& aPath) {
  if (!gExceptionHandler) return false;

#ifndef XP_LINUX
  aPath = CONVERT_XP_CHAR_TO_UTF16(gExceptionHandler->dump_path().c_str());
#else
  aPath = CONVERT_XP_CHAR_TO_UTF16(
      gExceptionHandler->minidump_descriptor().directory().c_str());
#endif
  return true;
}

nsresult SetMinidumpPath(const nsAString& aPath) {
  if (!gExceptionHandler) return NS_ERROR_NOT_INITIALIZED;

#ifdef XP_WIN
  gExceptionHandler->set_dump_path(
      std::wstring(char16ptr_t(aPath.BeginReading())));
#elif defined(XP_LINUX)
  gExceptionHandler->set_minidump_descriptor(
      MinidumpDescriptor(NS_ConvertUTF16toUTF8(aPath).BeginReading()));
#else
  gExceptionHandler->set_dump_path(NS_ConvertUTF16toUTF8(aPath).BeginReading());
#endif
  return NS_OK;
}

static nsresult WriteDataToFile(nsIFile* aFile, const nsACString& data) {
  PRFileDesc* fd;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, 00600, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_OK;
  if (PR_Write(fd, data.Data(), data.Length()) == -1) {
    rv = NS_ERROR_FAILURE;
  }
  PR_Close(fd);
  return rv;
}

static nsresult GetFileContents(nsIFile* aFile, nsACString& data) {
  PRFileDesc* fd;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_OK;
  int32_t filesize = PR_Available(fd);
  if (filesize <= 0) {
    rv = NS_ERROR_FILE_NOT_FOUND;
  } else {
    data.SetLength(filesize);
    if (PR_Read(fd, data.BeginWriting(), filesize) == -1) {
      rv = NS_ERROR_FAILURE;
    }
  }
  PR_Close(fd);
  return rv;
}

// Function typedef for initializing a piece of data that we
// don't already have.
typedef nsresult (*InitDataFunc)(nsACString&);

// Attempt to read aFile's contents into aContents, if aFile
// does not exist, create it and initialize its contents
// by calling aInitFunc for the data.
static nsresult GetOrInit(nsIFile* aDir, const nsACString& filename,
                          nsACString& aContents, InitDataFunc aInitFunc) {
  bool exists;

  nsCOMPtr<nsIFile> dataFile;
  nsresult rv = aDir->Clone(getter_AddRefs(dataFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataFile->AppendNative(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    if (aInitFunc) {
      // get the initial value and write it to the file
      rv = aInitFunc(aContents);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = WriteDataToFile(dataFile, aContents);
    } else {
      // didn't pass in an init func
      rv = NS_ERROR_FAILURE;
    }
  } else {
    // just get the file's contents
    rv = GetFileContents(dataFile, aContents);
  }

  return rv;
}

// Init the "install time" data.  We're taking an easy way out here
// and just setting this to "the time when this version was first run".
static nsresult InitInstallTime(nsACString& aInstallTime) {
  time_t t = time(nullptr);
  aInstallTime = nsPrintfCString("%" PRIu64, static_cast<uint64_t>(t));

  return NS_OK;
}

// Ensure a directory exists and create it if missing.
static nsresult EnsureDirectoryExists(nsIFile* dir) {
  nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);

  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)) {
    return rv;
  }

  return NS_OK;
}

// Creates a directory that will be accessible by the crash reporter. The
// directory will live under Firefox default data directory and will use the
// specified name. The directory path will be passed to the crashreporter via
// the specified environment variable.
static nsresult SetupCrashReporterDirectory(nsIFile* aAppDataDirectory,
                                            const char* aDirName,
                                            const XP_CHAR* aEnvVarName,
                                            nsIFile** aDirectory = nullptr) {
  nsCOMPtr<nsIFile> directory;
  nsresult rv = aAppDataDirectory->Clone(getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->AppendNative(nsDependentCString(aDirName));
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureDirectoryExists(directory);
  std::optional<xpstring> directoryPath = CreatePathFromFile(directory);

  if (!directoryPath) {
    return NS_ERROR_FAILURE;
  }

#if defined(XP_WIN)
  SetEnvironmentVariableW(aEnvVarName, directoryPath->c_str());
#else
  setenv(aEnvVarName, directoryPath->c_str(), /* overwrite */ 1);
#endif

  if (aDirectory) {
    directory.forget(aDirectory);
  }

  return NS_OK;
}

// Annotate the crash report with a Unique User ID and time
// since install.  Also do some prep work for recording
// time since last crash, which must be calculated at
// crash time.
// If any piece of data doesn't exist, initialize it first.
nsresult SetupExtraData(nsIFile* aAppDataDirectory,
                        const nsACString& aBuildID) {
  nsCOMPtr<nsIFile> dataDirectory;
  nsresult rv =
      SetupCrashReporterDirectory(aAppDataDirectory, "Crash Reports",
                                  XP_TEXT("MOZ_CRASHREPORTER_DATA_DIRECTORY"),
                                  getter_AddRefs(dataDirectory));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetupCrashReporterDirectory(aAppDataDirectory, "Pending Pings",
                                   XP_TEXT("MOZ_CRASHREPORTER_PING_DIRECTORY"));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString data;
  if (NS_SUCCEEDED(GetOrInit(dataDirectory, "InstallTime"_ns + aBuildID, data,
                             InitInstallTime))) {
    RecordAnnotationNSCString(Annotation::InstallTime, data);
  }

  // this is a little different, since we can't init it with anything,
  // since it's stored at crash time, and we can't annotate the
  // crash report with the stored value, since we really want
  // (now - LastCrash), so we just get a value if it exists,
  // and store it in a time_t value.
  if (NS_SUCCEEDED(GetOrInit(dataDirectory, "LastCrash"_ns, data, nullptr))) {
    lastCrashTime = (time_t)atol(data.get());
  }

  // not really the best place to init this, but I have the path I need here
  nsCOMPtr<nsIFile> lastCrashFile;
  rv = dataDirectory->Clone(getter_AddRefs(lastCrashFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = lastCrashFile->AppendNative("LastCrash"_ns);
  NS_ENSURE_SUCCESS(rv, rv);
  memset(lastCrashTimeFilename, 0, sizeof(lastCrashTimeFilename));

  PathString filename;
#if defined(XP_WIN)
  rv = lastCrashFile->GetPath(filename);
#else
  rv = lastCrashFile->GetNativePath(filename);
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.Length() < XP_PATH_MAX) {
#if defined(XP_WIN)
    wcsncpy(lastCrashTimeFilename, filename.get(), filename.Length());
#else
    strncpy(lastCrashTimeFilename, filename.get(), filename.Length());
#endif
  }

  return NS_OK;
}

static void OOPDeinit();

nsresult UnsetExceptionHandler() {
  if (isSafeToDump) {
    MutexAutoLock lock(*dumpSafetyLock);
    isSafeToDump = false;
  }

#ifdef XP_WIN
  // allow SetUnhandledExceptionFilter
  gBlockUnhandledExceptionFilter = false;
#endif

  delete gExceptionHandler;

  gServerURL = "";
  TeardownAppNotes();

  if (!gExceptionHandler) return NS_ERROR_NOT_INITIALIZED;

  gExceptionHandler = nullptr;

  OOPDeinit();

  delete dumpSafetyLock;
  dumpSafetyLock = nullptr;

  std::set_terminate(oldTerminateHandler);

  return NS_OK;
}

const bool* RegisterAnnotationBool(Annotation aKey, const bool* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::Boolean,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return nullptr;
  }

  return reinterpret_cast<const bool*>(mozannotation_register_bytebuffer(
      static_cast<uint32_t>(aKey), aData, sizeof(bool)));
}

const uint32_t* RegisterAnnotationU32(Annotation aKey, const uint32_t* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::U32,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return nullptr;
  }

  return reinterpret_cast<const uint32_t*>(mozannotation_register_bytebuffer(
      static_cast<uint32_t>(aKey), aData, sizeof(uint32_t)));
}

const uint64_t* RegisterAnnotationU64(Annotation aKey, const uint64_t* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::U64,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return nullptr;
  }

  return reinterpret_cast<const uint64_t*>(mozannotation_register_bytebuffer(
      static_cast<uint32_t>(aKey), aData, sizeof(uint64_t)));
}

const size_t* RegisterAnnotationUSize(Annotation aKey, const size_t* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::USize,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return nullptr;
  }

  return reinterpret_cast<const size_t*>(mozannotation_register_bytebuffer(
      static_cast<uint32_t>(aKey), aData, sizeof(size_t)));
}

const char* RegisterAnnotationCString(Annotation aKey, const char* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::String,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return nullptr;
  }

  return mozannotation_register_cstring(static_cast<uint32_t>(aKey), aData);
}

const nsCString* RegisterAnnotationNSCString(Annotation aKey,
                                             const nsCString* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::String,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return nullptr;
  }

  return mozannotation_register_nscstring(static_cast<uint32_t>(aKey), aData);
}

nsresult RecordAnnotationBool(Annotation aKey, bool aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::Boolean,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mozannotation_record_bytebuffer(static_cast<uint32_t>(aKey), &aData,
                                  sizeof(bool));
  return NS_OK;
}

nsresult RecordAnnotationU32(Annotation aKey, uint32_t aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::U32,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mozannotation_record_bytebuffer(static_cast<uint32_t>(aKey), &aData,
                                  sizeof(uint32_t));
  return NS_OK;
}

nsresult RecordAnnotationU64(Annotation aKey, uint64_t aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::U64,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mozannotation_record_bytebuffer(static_cast<uint64_t>(aKey), &aData,
                                  sizeof(uint64_t));
  return NS_OK;
}

nsresult RecordAnnotationUSize(Annotation aKey, size_t aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::USize,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mozannotation_record_bytebuffer(static_cast<size_t>(aKey), &aData,
                                  sizeof(size_t));
  return NS_OK;
}

nsresult RecordAnnotationCString(Annotation aKey, const char* aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::String,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mozannotation_record_cstring(static_cast<uint32_t>(aKey), aData);
  return NS_OK;
}

nsresult RecordAnnotationNSCString(Annotation aKey, const nsACString& aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::String,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mozannotation_record_nscstring_from_raw_parts(
      static_cast<uint32_t>(aKey),
      reinterpret_cast<const uint8_t*>(aData.Data()), aData.Length());
  return NS_OK;
}

nsresult RecordAnnotationNSString(Annotation aKey, const nsAString& aData) {
  MOZ_ASSERT(TypeOfAnnotation(aKey) == AnnotationType::String,
             "Wrong annotation type");

  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsAutoCString data = NS_ConvertUTF16toUTF8(aData);
  RecordAnnotationNSCString(aKey, data);
  return NS_OK;
}

nsresult UnrecordAnnotation(Annotation aKey) {
  if (!GetEnabled()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mozannotation_unregister(static_cast<uint32_t>(aKey));
  return NS_OK;
}

// TODO: The first three methods here should be migrated to use native
// types instead of turning the values into strings. They're not currently
// used in mozilla-central so it doesn't really matter in the short term.
AutoRecordAnnotation::AutoRecordAnnotation(Annotation key, bool data)
    : AutoRecordAnnotation(key, data ? "1"_ns : "0"_ns) {}

AutoRecordAnnotation::AutoRecordAnnotation(Annotation key, int data)
    : AutoRecordAnnotation(key, nsPrintfCString("%d", data)) {}

AutoRecordAnnotation::AutoRecordAnnotation(Annotation key, unsigned data)
    : AutoRecordAnnotation(key, nsPrintfCString("%u", data)) {}

AutoRecordAnnotation::AutoRecordAnnotation(Annotation key,
                                           const nsACString& data)
    : mKey(key), mCurrent(data) {
  if (GetEnabled()) {
    mPrevious =
        mozannotation_register_nscstring(static_cast<uint32_t>(key), &mCurrent);
  }
}

AutoRecordAnnotation::~AutoRecordAnnotation() {
  if (GetEnabled()) {
    Unused << mozannotation_register_nscstring(static_cast<uint32_t>(mKey),
                                               mPrevious);
  }
}

void MergeCrashAnnotations(AnnotationTable& aDst, const AnnotationTable& aSrc) {
  for (auto key : MakeEnumeratedRange(Annotation::Count)) {
    const nsCString& value = aSrc[key];
    if (!value.IsEmpty()) {
      aDst[key] = value;
    }
  }
}

// Adds crash time, uptime and memory report annotations
static void AddCommonAnnotations(AnnotationTable& aAnnotations) {
  const time_t crashTime = time(nullptr);
  nsAutoCString crashTimeStr;
  crashTimeStr.AppendInt(static_cast<uint64_t>(crashTime));
  aAnnotations[Annotation::CrashTime] = crashTimeStr;

  if (inactiveStateStart) {
    nsAutoCString inactiveDuration;
    inactiveDuration.AppendInt(
        static_cast<uint64_t>(crashTime - inactiveStateStart));
    aAnnotations[Annotation::LastInteractionDuration] = inactiveDuration;
  }

  double uptimeTS = (TimeStamp::NowLoRes() - TimeStamp::ProcessCreation())
                        .ToSecondsSigDigits();
  nsAutoCString uptimeStr;
  uptimeStr.AppendFloat(uptimeTS);
  aAnnotations[Annotation::UptimeTS] = uptimeStr;
}

nsresult SetGarbageCollecting(bool collecting) {
  if (!GetEnabled()) return NS_ERROR_NOT_INITIALIZED;

  isGarbageCollecting = collecting;

  return NS_OK;
}

void SetEventloopNestingLevel(uint32_t level) { eventloopNestingLevel = level; }

void ClearInactiveStateStart() { inactiveStateStart = 0; }
void SetInactiveStateStart() {
  if (!inactiveStateStart) {
    inactiveStateStart = GetCurrentTimeForCrashTime();
  }
}

void SetMinidumpAnalysisAllThreads() {
  char* env = strdup("MOZ_CRASHREPORTER_DUMP_ALL_THREADS=1");
  PR_SetEnv(env);
}

nsresult AppendAppNotesToCrashReport(const nsACString& data) {
  if (!GetEnabled()) return NS_ERROR_NOT_INITIALIZED;

  MutexAutoLock lock(*notesFieldLock);
  notesField->Append(data);
  return NS_OK;
}

nsresult RegisterAppMemory(void* ptr, size_t length) {
  if (!GetEnabled()) return NS_ERROR_NOT_INITIALIZED;

#if defined(XP_LINUX) || defined(XP_WIN)
  gExceptionHandler->RegisterAppMemory(ptr, length);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult UnregisterAppMemory(void* ptr) {
  if (!GetEnabled()) return NS_ERROR_NOT_INITIALIZED;

#if defined(XP_LINUX) || defined(XP_WIN)
  gExceptionHandler->UnregisterAppMemory(ptr);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

void SetIncludeContextHeap(bool aValue) {
  sIncludeContextHeap = aValue;

#ifdef XP_WIN
  if (gExceptionHandler) {
    gExceptionHandler->set_include_context_heap(sIncludeContextHeap);
  }
#endif
}

bool GetServerURL(nsACString& aServerURL) {
  if (!gExceptionHandler) {
    return false;
  }

  aServerURL = gServerURL;
  return true;
}

nsresult SetServerURL(const nsACString& aServerURL) {
  // Store the server URL as an annotation, the crash reporter client knows how
  // to handle this specially.
  gServerURL = aServerURL;
  return NS_OK;
}

nsresult SetRestartArgs(int argc, char** argv) {
  if (!gExceptionHandler) return NS_OK;

  int i;
  nsAutoCString envVar;
  char* env;
  for (i = 1; i < argc; i++) {
    envVar = "MOZ_CRASHREPORTER_RESTART_ARG_";
    envVar.AppendInt(i);
    envVar += "=";
    envVar += argv[i];

    // PR_SetEnv() wants the string to be available for the lifetime
    // of the app, so dup it here. This conversion is not lossy.
    env = ToNewCString(envVar, mozilla::fallible);
    if (!env) return NS_ERROR_OUT_OF_MEMORY;

    PR_SetEnv(env);
  }

  // make sure the arg list is terminated
  envVar = "MOZ_CRASHREPORTER_RESTART_ARG_";
  envVar.AppendInt(i);
  envVar += "=";

  // PR_SetEnv() wants the string to be available for the lifetime
  // of the app, so dup it here. This conversion is not lossy.
  env = ToNewCString(envVar, mozilla::fallible);
  if (!env) return NS_ERROR_OUT_OF_MEMORY;

  PR_SetEnv(env);

  // make sure we save the info in XUL_APP_FILE for the reporter
  const char* appfile = PR_GetEnv("XUL_APP_FILE");
  if (appfile && *appfile) {
    envVar = "MOZ_CRASHREPORTER_RESTART_XUL_APP_FILE=";
    envVar += appfile;

    // PR_SetEnv() wants the string to be available for the lifetime
    // of the app, so dup it here. This conversion is not lossy.
    env = ToNewCString(envVar);
    PR_SetEnv(env);
  }

  return NS_OK;
}

#ifdef XP_WIN
nsresult WriteMinidumpForException(EXCEPTION_POINTERS* aExceptionInfo) {
  if (!gExceptionHandler) return NS_ERROR_NOT_INITIALIZED;

  return gExceptionHandler->WriteMinidumpForException(aExceptionInfo)
             ? NS_OK
             : NS_ERROR_FAILURE;
}
#endif

#ifdef XP_LINUX
bool WriteMinidumpForSigInfo(int signo, siginfo_t* info, void* uc) {
  if (!gExceptionHandler) {
    // Crash reporting is disabled.
    return false;
  }
  return gExceptionHandler->HandleSignal(signo, info, uc);
}
#endif

#ifdef XP_MACOSX
nsresult AppendObjCExceptionInfoToAppNotes(void* inException) {
  nsAutoCString excString;
  GetObjCExceptionInfo(inException, excString);
  AppendAppNotesToCrashReport(excString);
  return NS_OK;
}
#endif

/*
 * Combined code to get/set the crash reporter submission pref on
 * different platforms.
 */
static nsresult PrefSubmitReports(bool* aSubmitReports, bool writePref) {
  nsresult rv;
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_UNIX)
  /*
   * NOTE! This needs to stay in sync with the code in
   * toolkit/crashreporter/client/app/src/{logic,settings}.rs
   */
  nsCOMPtr<nsIFile> reporterSettings;
  rv = NS_GetSpecialDirectory(XRE_USER_APP_DATA_DIR,
                              getter_AddRefs(reporterSettings));
  NS_ENSURE_SUCCESS(rv, rv);
  reporterSettings->AppendNative("Crash Reports"_ns);
  reporterSettings->AppendNative("crashreporter_settings.json"_ns);

  // On e.g. Linux, std::filesystem requires sometimes linking libstdc++fs,
  // and we don't do that yet, so limit the use of std::filesystem::path where
  // it's really needed, which is Windows, because implicit conversions from
  // wstring in fstream constructors are not supported as of
  // https://cplusplus.github.io/LWG/issue3430.
#  ifdef XP_WIN
  std::optional<std::filesystem::path> file_path =
      CreatePathFromFile(reporterSettings);
#  else
  std::optional<xpstring> file_path = CreatePathFromFile(reporterSettings);
#  endif

  if (!file_path) {
    return NS_ERROR_FAILURE;
  }

  Json::Value root;

  bool exists;
  rv = reporterSettings->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    if (!writePref) {
      // If reading the pref, default to true if the settings file doesn't
      // exist.
      *aSubmitReports = true;
      return NS_OK;
    }
    // Create the file so the JSON processor can write to it.
    rv = reporterSettings->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Read the root value
    std::ifstream file(*file_path);
    file >> root;
  }

  if (writePref) {
    root["submit_report"] = *aSubmitReports;
    std::ofstream file(*file_path);
    file << root;
  } else if (root["submit_report"].isBool()) {
    *aSubmitReports = root["submit_report"].asBool();
  } else {
    // Default to "true" if the pref can't be found.
    *aSubmitReports = true;
  }

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult GetSubmitReports(bool* aSubmitReports) {
  return PrefSubmitReports(aSubmitReports, false);
}

nsresult SetSubmitReports(bool aSubmitReports) {
  nsresult rv;

  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (!obsServ) {
    return NS_ERROR_FAILURE;
  }

  rv = PrefSubmitReports(&aSubmitReports, true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  obsServ->NotifyObservers(nullptr, "submit-reports-pref-changed", nullptr);
  return NS_OK;
}

static void SetCrashEventsDir(nsIFile* aDir) {
  static const XP_CHAR eventsDirectoryEnv[] =
      XP_TEXT("MOZ_CRASHREPORTER_EVENTS_DIRECTORY");

  nsCOMPtr<nsIFile> eventsDir = aDir;

  const char* env = PR_GetEnv("CRASHES_EVENTS_DIR");
  if (env && *env) {
    NS_NewNativeLocalFile(nsDependentCString(env), false,
                          getter_AddRefs(eventsDir));
    EnsureDirectoryExists(eventsDir);
  }

  std::optional<xpstring> path = CreatePathFromFile(eventsDir);
  if (!path) {
    return;  // There's no clean failure from this
  }

  eventsDirectory = *path;
#ifdef XP_WIN
  SetEnvironmentVariableW(eventsDirectoryEnv, path->c_str());
#else
  setenv(eventsDirectoryEnv, path->c_str(), /* overwrite */ 1);
#endif
}

void SetProfileDirectory(nsIFile* aDir) {
  nsCOMPtr<nsIFile> dir;
  aDir->Clone(getter_AddRefs(dir));

  dir->Append(u"crashes"_ns);
  EnsureDirectoryExists(dir);
  dir->Append(u"events"_ns);
  EnsureDirectoryExists(dir);
  SetCrashEventsDir(dir);
}

void SetUserAppDataDirectory(nsIFile* aDir) {
  nsCOMPtr<nsIFile> dir;
  aDir->Clone(getter_AddRefs(dir));

  dir->Append(u"Crash Reports"_ns);
  EnsureDirectoryExists(dir);
  dir->Append(u"events"_ns);
  EnsureDirectoryExists(dir);
  SetCrashEventsDir(dir);
}

void UpdateCrashEventsDir() {
  const char* env = PR_GetEnv("CRASHES_EVENTS_DIR");
  if (env && *env) {
    SetCrashEventsDir(nullptr);
  }

  nsCOMPtr<nsIFile> eventsDir;
  nsresult rv = NS_GetSpecialDirectory("ProfD", getter_AddRefs(eventsDir));
  if (NS_SUCCEEDED(rv)) {
    SetProfileDirectory(eventsDir);
    return;
  }

  rv = NS_GetSpecialDirectory(XRE_USER_APP_DATA_DIR, getter_AddRefs(eventsDir));
  if (NS_SUCCEEDED(rv)) {
    SetUserAppDataDirectory(eventsDir);
    return;
  }

  NS_WARNING(
      "Couldn't get the user appdata directory. Crash events may not be "
      "produced.");
}

bool GetCrashEventsDir(nsAString& aPath) {
  if (eventsDirectory.empty()) {
    return false;
  }
  aPath = CONVERT_XP_CHAR_TO_UTF16(eventsDirectory.c_str());
  return true;
}

void SetMemoryReportFile(nsIFile* aFile) {
  if (!gExceptionHandler) {
    return;
  }

  PathString path;
#ifdef XP_WIN
  aFile->GetPath(path);
#else
  aFile->GetNativePath(path);
#endif
  memoryReportPath = xpstring(path.get());
}

nsresult GetDefaultMemoryReportFile(nsIFile** aFile) {
  nsCOMPtr<nsIFile> defaultMemoryReportFile;
  if (!defaultMemoryReportPath) {
    nsresult rv = NS_GetSpecialDirectory(
        NS_APP_PROFILE_DIR_STARTUP, getter_AddRefs(defaultMemoryReportFile));
    if (NS_FAILED(rv)) {
      return rv;
    }
    defaultMemoryReportFile->AppendNative("memory-report.json.gz"_ns);
    defaultMemoryReportPath = CreatePathFromFile(defaultMemoryReportFile);
    if (!defaultMemoryReportPath) {
      return NS_ERROR_FAILURE;
    }
  } else {
    CreateFileFromPath(*defaultMemoryReportPath,
                       getter_AddRefs(defaultMemoryReportFile));
    if (!defaultMemoryReportFile) {
      return NS_ERROR_FAILURE;
    }
  }
  defaultMemoryReportFile.forget(aFile);
  return NS_OK;
}

static void FindPendingDir() {
  if (!pendingDirectory.empty()) {
    return;
  }
  nsCOMPtr<nsIFile> pendingDir;
  nsresult rv =
      NS_GetSpecialDirectory(XRE_USER_APP_DATA_DIR, getter_AddRefs(pendingDir));
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "Couldn't get the user appdata directory, crash dumps will go in an "
        "unusual location");
  } else {
    pendingDir->Append(u"Crash Reports"_ns);
    pendingDir->Append(u"pending"_ns);

    PathString path;
#ifdef XP_WIN
    pendingDir->GetPath(path);
#else
    pendingDir->GetNativePath(path);
#endif
    pendingDirectory = xpstring(path.get());
  }
}

// The "pending" dir is Crash Reports/pending, from which minidumps
// can be submitted. Because this method may be called off the main thread,
// we store the pending directory as a path.
static bool GetPendingDir(nsIFile** dir) {
  // MOZ_ASSERT(OOPInitialized());
  if (pendingDirectory.empty()) {
    return false;
  }

  nsCOMPtr<nsIFile> pending = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (!pending) {
    NS_WARNING("Can't set up pending directory during shutdown.");
    return false;
  }
#ifdef XP_WIN
  pending->InitWithPath(nsDependentString(pendingDirectory.c_str()));
#else
  pending->InitWithNativePath(nsDependentCString(pendingDirectory.c_str()));
#endif
  pending.swap(*dir);
  return true;
}

// The "limbo" dir is where minidumps go to wait for something else to
// use them.  If we're |ShouldReport()|, then the "something else" is
// a minidump submitter, and they're coming from the
// Crash Reports/pending/ dir.  Otherwise, we don't know what the
// "somthing else" is, but the minidumps stay in [profile]/minidumps/
// limbo.
static bool GetMinidumpLimboDir(nsIFile** dir) {
  if (ShouldReport()) {
    return GetPendingDir(dir);
  } else {
#ifndef XP_LINUX
    CreateFileFromPath(gExceptionHandler->dump_path(), dir);
#else
    CreateFileFromPath(gExceptionHandler->minidump_descriptor().directory(),
                       dir);
#endif
    return nullptr != *dir;
  }
}

void DeleteMinidumpFilesForID(const nsAString& aId,
                              const Maybe<nsString>& aAdditionalMinidump) {
  nsCOMPtr<nsIFile> minidumpFile;
  if (GetMinidumpForID(aId, getter_AddRefs(minidumpFile))) {
    minidumpFile->Remove(false);
  }

  nsCOMPtr<nsIFile> extraFile;
  if (GetExtraFileForID(aId, getter_AddRefs(extraFile))) {
    extraFile->Remove(false);
  }

  if (aAdditionalMinidump && GetMinidumpForID(aId, getter_AddRefs(minidumpFile),
                                              aAdditionalMinidump)) {
    minidumpFile->Remove(false);
  }
}

bool GetMinidumpForID(const nsAString& id, nsIFile** minidump,
                      const Maybe<nsString>& aAdditionalMinidump) {
  if (!GetMinidumpLimboDir(minidump)) {
    return false;
  }

  nsAutoString fileName(id);

  if (aAdditionalMinidump) {
    fileName.Append('-');
    fileName.Append(*aAdditionalMinidump);
  }

  fileName.Append(u".dmp"_ns);
  (*minidump)->Append(fileName);

  bool exists;
  if (NS_FAILED((*minidump)->Exists(&exists)) || !exists) {
    return false;
  }

  return true;
}

bool GetIDFromMinidump(nsIFile* minidump, nsAString& id) {
  if (minidump && NS_SUCCEEDED(minidump->GetLeafName(id))) {
    id.ReplaceLiteral(id.Length() - 4, 4, u"");
    return true;
  }
  return false;
}

bool GetExtraFileForID(const nsAString& id, nsIFile** extraFile) {
  if (!GetMinidumpLimboDir(extraFile)) {
    return false;
  }

  (*extraFile)->Append(id + u".extra"_ns);

  bool exists;
  if (NS_FAILED((*extraFile)->Exists(&exists)) || !exists) {
    return false;
  }

  return true;
}

bool GetExtraFileForMinidump(nsIFile* minidump, nsIFile** extraFile) {
  nsAutoString leafName;
  nsresult rv = minidump->GetLeafName(leafName);
  if (NS_FAILED(rv)) return false;

  nsCOMPtr<nsIFile> extraF;
  rv = minidump->Clone(getter_AddRefs(extraF));
  if (NS_FAILED(rv)) return false;

  leafName.Replace(leafName.Length() - 3, 3, u"extra"_ns);
  rv = extraF->SetLeafName(leafName);
  if (NS_FAILED(rv)) return false;

  *extraFile = nullptr;
  extraF.swap(*extraFile);
  return true;
}

static bool WriteExtraFile(PlatformWriter& pw,
                           const AnnotationTable& aAnnotations) {
  if (!pw.Valid()) {
    return false;
  }

  JSONAnnotationWriter writer(pw);
  WriteAnnotations(writer, aAnnotations);
  WriteSynthesizedAnnotations(writer);

  return true;
}

bool WriteExtraFile(const nsAString& id, const AnnotationTable& annotations) {
  nsCOMPtr<nsIFile> extra;
  if (!GetMinidumpLimboDir(getter_AddRefs(extra))) {
    return false;
  }

  extra->Append(id + u".extra"_ns);
  PathString path;
#ifdef XP_WIN
  NS_ENSURE_SUCCESS(extra->GetPath(path), false);
#elif defined(XP_UNIX)
  NS_ENSURE_SUCCESS(extra->GetNativePath(path), false);
#endif

  PlatformWriter pw(path.get());
  return WriteExtraFile(pw, annotations);
}

template <typename T>
static bool IsFixedSizeAnnotation(AnnotationContents& contents) {
  return ((contents.tag == AnnotationContents::Tag::ByteBuffer) &&
          (contents.byte_buffer._0 == sizeof(T))) ||
         ((contents.tag == AnnotationContents::Tag::OwnedByteBuffer) &&
          (contents.owned_byte_buffer._0 == sizeof(T)));
}

// This adds annotations that were populated in the main process but are not
// present among the ones that were passed in. Additionally common annotations
// which are present in every crash report are added, including crash time,
// uptime, etc...
static void AddSharedAnnotations(AnnotationTable& aAnnotations) {
  for (auto key : MakeEnumeratedRange(Annotation::Count)) {
    AnnotationContents contents = {};
    nsAutoCString value;
    size_t address =
        mozannotation_get_contents(static_cast<uint32_t>(key), &contents);

    if (address) {
      switch (TypeOfAnnotation(key)) {
        case AnnotationType::String:
          switch (contents.tag) {
            case AnnotationContents::Tag::Empty:
              break;
            case AnnotationContents::Tag::CStringPointer:
              address = *reinterpret_cast<size_t*>(address);
              if (address == 0) {
                break;
              }
              // FALLTHROUGH
            case AnnotationContents::Tag::CString:
              value.Assign(reinterpret_cast<const char*>(address));
              break;
            case AnnotationContents::Tag::NSCStringPointer:
              value.Assign(*reinterpret_cast<nsCString*>(address));
              break;
            case AnnotationContents::Tag::ByteBuffer:
              value.Assign(reinterpret_cast<const char*>(address),
                           contents.byte_buffer._0);
              break;
            case AnnotationContents::Tag::OwnedByteBuffer:
              value.Assign(reinterpret_cast<const char*>(address),
                           contents.owned_byte_buffer._0);
              break;
          }

          break;
        case AnnotationType::Boolean:
          if (IsFixedSizeAnnotation<bool>(contents)) {
            value.Assign(*reinterpret_cast<const bool*>(address) ? "1" : "0");
          }
          break;
        case AnnotationType::U32:
          if (IsFixedSizeAnnotation<uint32_t>(contents)) {
            value.AppendInt(*reinterpret_cast<const uint32_t*>(address));
          }
          break;
        case AnnotationType::U64:
          if (IsFixedSizeAnnotation<uint64_t>(contents)) {
            value.AppendInt(*reinterpret_cast<const uint64_t*>(address));
          }
          break;
        case AnnotationType::USize:
          if (IsFixedSizeAnnotation<size_t>(contents)) {
#ifdef XP_MACOSX
            // macOS defines size_t as unsigned long, which causes ambiguity
            // when it comes to function overload, use a 64-bit integer instead
            value.AppendInt(*reinterpret_cast<const uint64_t*>(address));
#else
            value.AppendInt(*reinterpret_cast<const size_t*>(address));
#endif
          }
          break;
      }

      if (!value.IsEmpty() && aAnnotations[key].IsEmpty() &&
          ShouldIncludeAnnotation(key, value.get())) {
        aAnnotations[key] = value;
      }
    }
  }

  AddCommonAnnotations(aAnnotations);
}

static void AddChildProcessAnnotations(
    AnnotationTable& aAnnotations, nsTArray<CAnnotation>* aChildAnnotations) {
  if (!aChildAnnotations) {
    // TODO: We should probably make a list of errors that occurred when
    // generating a crash report as more than one can occurr.
    aAnnotations[Annotation::DumperError] = "MissingAnnotations";
    return;
  }

  for (const auto& annotation : *aChildAnnotations) {
    Annotation id = static_cast<Annotation>(annotation.id);
    const AnnotationData& data = annotation.data;

    if ((id == Annotation::PHCBaseAddress) &&
        (data.tag == AnnotationData::Tag::ByteBuffer)) {
      // PHC is special for now, let's deal with it here
#ifdef MOZ_PHC
      const auto& buffer = data.byte_buffer._0;
      mozilla::phc::AddrInfo addr_info;
      memcpy(&addr_info, buffer.Elements(), sizeof(addr_info));
      PopulatePHCAnnotations(aAnnotations, &addr_info);
#endif
      continue;
    }

    if (data.tag == AnnotationData::Tag::Empty) {
      continue;
    }

    nsAutoCString value;
    const uint8_t* buffer = data.byte_buffer._0.Elements();
    const size_t length = data.byte_buffer._0.Length();

    switch (TypeOfAnnotation(id)) {
      case AnnotationType::String:
        value.Assign(reinterpret_cast<const char*>(buffer), length);
        break;
      case AnnotationType::Boolean:
        if (length == sizeof(bool)) {
          value.Assign(*reinterpret_cast<const bool*>(buffer) ? "1" : "0");
        }
        break;
      case AnnotationType::U32:
        if (length == sizeof(uint32_t)) {
          value.AppendInt(*reinterpret_cast<const uint32_t*>(buffer));
        }
        break;
      case AnnotationType::U64:
        if (length == sizeof(uint64_t)) {
          value.AppendInt(*reinterpret_cast<const uint64_t*>(buffer));
        }
        break;
      case AnnotationType::USize:
        if (length == sizeof(size_t)) {
#ifdef XP_MACOSX
          // macOS defines size_t as unsigned long, which causes ambiguity
          // when it comes to function overload, use a 64-bit integer instead
          value.AppendInt(*reinterpret_cast<const uint64_t*>(buffer));
#else
          value.AppendInt(*reinterpret_cast<const size_t*>(buffer));
#endif
        }
        break;
    }

    if (!value.IsEmpty() && ShouldIncludeAnnotation(id, value.get())) {
      aAnnotations[id] = value;
    }
  }
}

// It really only makes sense to call this function when
// ShouldReport() is true.
// Uses dumpFile's filename to generate memoryReport's filename (same name
// with a different extension)
static bool MoveToPending(nsIFile* dumpFile, nsIFile* extraFile,
                          nsIFile* memoryReport) {
  nsCOMPtr<nsIFile> pendingDir;
  if (!GetPendingDir(getter_AddRefs(pendingDir))) return false;

  if (NS_FAILED(dumpFile->MoveTo(pendingDir, u""_ns))) {
    return false;
  }

  if (extraFile && NS_FAILED(extraFile->MoveTo(pendingDir, u""_ns))) {
    return false;
  }

  if (memoryReport) {
    nsAutoString leafName;
    nsresult rv = dumpFile->GetLeafName(leafName);
    if (NS_FAILED(rv)) {
      return false;
    }
    // Generate the correct memory report filename from the dumpFile's name
    leafName.Replace(
        leafName.Length() - 4, 4,
        static_cast<nsString>(CONVERT_XP_CHAR_TO_UTF16(memoryReportExtension)));
    if (NS_FAILED(memoryReport->MoveTo(pendingDir, leafName))) {
      return false;
    }
  }

  return true;
}

static void MaybeAnnotateDumperError(const ClientInfo& aClientInfo,
                                     AnnotationTable& aAnnotations) {
#if defined(MOZ_OXIDIZED_BREAKPAD)
  if (aClientInfo.had_error()) {
    aAnnotations[Annotation::DumperError] = *aClientInfo.error_msg();
  }
#endif
}

static void OnChildProcessDumpRequested(
    void* aContext, const ClientInfo& aClientInfo,
    const xpstring& aFilePath) MOZ_NO_THREAD_SAFETY_ANALYSIS {
  nsCOMPtr<nsIFile> minidump;

  // Hold the mutex until the current dump request is complete, to
  // prevent UnsetExceptionHandler() from pulling the rug out from
  // under us.
  MutexAutoLock lock(*dumpSafetyLock);
  if (!isSafeToDump) return;

  CreateFileFromPath(aFilePath, getter_AddRefs(minidump));

  ProcessId pid = aClientInfo.pid();
  if (ShouldReport()) {
    nsCOMPtr<nsIFile> memoryReport;
    if (!memoryReportPath.empty()) {
      CreateFileFromPath(memoryReportPath, getter_AddRefs(memoryReport));
      MOZ_ASSERT(memoryReport);
    }
    MoveToPending(minidump, nullptr, memoryReport);
  }

#if XP_WIN
  nsTArray<CAnnotation>* child_annotations = mozannotation_retrieve(
      reinterpret_cast<uintptr_t>(aClientInfo.process_handle()),
      static_cast<size_t>(Annotation::Count));
#elif defined(XP_MACOSX)
  nsTArray<CAnnotation>* child_annotations = mozannotation_retrieve(
      aClientInfo.task(), static_cast<size_t>(Annotation::Count));
#else
  nsTArray<CAnnotation>* child_annotations =
      mozannotation_retrieve(pid, static_cast<size_t>(Annotation::Count));
#endif

  // TODO: Write a minimal set of annotations if we fail to read them, and
  // add an error to the minidump to highlight this fact.

  {
    MutexAutoLock lock(*dumpMapLock);
    ChildProcessData* pd = pidToMinidump->PutEntry(pid);
    MOZ_ASSERT(!pd->minidump);
    pd->minidump = minidump;
    pd->annotations = MakeUnique<AnnotationTable>();
    AnnotationTable& annotations = *(pd->annotations);
    AddSharedAnnotations(annotations);
    AddChildProcessAnnotations(annotations, child_annotations);

    MaybeAnnotateDumperError(aClientInfo, annotations);
  }

  if (child_annotations) {
    mozannotation_free(child_annotations);
  }
}

static bool OOPInitialized() { return pidToMinidump != nullptr; }

void OOPInit() {
  class ProxyToMainThread : public Runnable {
   public:
    ProxyToMainThread() : Runnable("nsExceptionHandler::ProxyToMainThread") {}
    NS_IMETHOD Run() override {
      OOPInit();
      return NS_OK;
    }
  };
  if (!NS_IsMainThread()) {
    // This logic needs to run on the main thread
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    mozilla::SyncRunnable::DispatchToThread(mainThread,
                                            new ProxyToMainThread());
    return;
  }

  if (OOPInitialized()) return;

  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(gExceptionHandler != nullptr,
             "attempt to initialize OOP crash reporter before in-process "
             "crashreporter!");

#if defined(XP_WIN)
  childCrashNotifyPipe =
      mozilla::Smprintf("\\\\.\\pipe\\gecko-crash-server-pipe.%i",
                        static_cast<int>(::GetCurrentProcessId()))
          .release();

  const std::wstring dumpPath = gExceptionHandler->dump_path();
  crashServer = new CrashGenerationServer(
      std::wstring(NS_ConvertASCIItoUTF16(childCrashNotifyPipe).get()),
      nullptr,           // default security attributes
      nullptr, nullptr,  // we don't care about process connect here
      OnChildProcessDumpRequested, nullptr, nullptr, nullptr,
      nullptr,           // we don't care about process exit here
      nullptr, nullptr,  // we don't care about upload request here
      true,              // automatically generate dumps
      &dumpPath);

  if (sIncludeContextHeap) {
    crashServer->set_include_context_heap(sIncludeContextHeap);
  }

#elif defined(XP_LINUX)
  if (!CrashGenerationServer::CreateReportChannel(&serverSocketFd,
                                                  &clientSocketFd))
    MOZ_CRASH("can't create crash reporter socketpair()");

  const std::string dumpPath =
      gExceptionHandler->minidump_descriptor().directory();
  crashServer =
      new CrashGenerationServer(serverSocketFd, OnChildProcessDumpRequested,
                                nullptr, nullptr, nullptr, true, &dumpPath);

#elif defined(XP_MACOSX)
  childCrashNotifyPipe = mozilla::Smprintf("gecko-crash-server-pipe.%i",
                                           static_cast<int>(getpid()))
                             .release();
  const std::string dumpPath = gExceptionHandler->dump_path();

  crashServer = new CrashGenerationServer(childCrashNotifyPipe, nullptr,
                                          nullptr, OnChildProcessDumpRequested,
                                          nullptr, nullptr, nullptr,
                                          true,  // automatically generate dumps
                                          dumpPath);
#endif

  if (!crashServer->Start()) MOZ_CRASH("can't start crash reporter server()");

  pidToMinidump = new ChildMinidumpMap();

  dumpMapLock = new Mutex("CrashReporter::dumpMapLock");

  FindPendingDir();
  UpdateCrashEventsDir();
}

static void OOPDeinit() {
  if (!OOPInitialized()) {
    NS_WARNING("OOPDeinit() without successful OOPInit()");
    return;
  }

  delete crashServer;
  crashServer = nullptr;

  delete dumpMapLock;
  dumpMapLock = nullptr;

  delete pidToMinidump;
  pidToMinidump = nullptr;

#if defined(XP_WIN) || defined(XP_MACOSX)
  free(childCrashNotifyPipe);
  childCrashNotifyPipe = nullptr;
#endif
}

#if defined(XP_WIN) || defined(XP_MACOSX)
// Parent-side API for children
const char* GetChildNotificationPipe() {
  if (!GetEnabled()) return kNullNotifyPipe;

  MOZ_ASSERT(OOPInitialized());

  return childCrashNotifyPipe;
}
#endif

#if defined(XP_LINUX)

// Parent-side API for children
bool CreateNotificationPipeForChild(int* childCrashFd, int* childCrashRemapFd) {
  if (!GetEnabled()) {
    *childCrashFd = -1;
    *childCrashRemapFd = -1;
    return true;
  }

  MOZ_ASSERT(OOPInitialized());

  *childCrashFd = clientSocketFd;
  *childCrashRemapFd = gMagicChildCrashReportFd;

  return true;
}

#endif  // defined(XP_LINUX)

bool SetRemoteExceptionHandler(const char* aCrashPipe) {
  MOZ_ASSERT(!gExceptionHandler, "crash client already init'd");
  RegisterRuntimeExceptionModule();
  InitializeAppNotes();
  RegisterAnnotations();

#ifdef MOZ_PHC
  // HACK: We're using the PHCBaseAddress annotation to point to the actual
  // PHC address information object. This is because we currently have no
  // difference between the internal representation of annotations and their
  // external representation. Once we remove the old annotation API this
  // will be properly addressed.
  mozannotation_register_bytebuffer(
      static_cast<uint32_t>(Annotation::PHCBaseAddress),
      &mozilla::phc::gAddrInfo, sizeof(mozilla::phc::gAddrInfo));
#endif

#if defined(XP_WIN)
  gExceptionHandler = new google_breakpad::ExceptionHandler(
      L"", ChildFilter, ChildMinidumpCallback,
      nullptr,  // no callback context
      google_breakpad::ExceptionHandler::HANDLER_ALL, GetMinidumpType(),
      NS_ConvertASCIItoUTF16(aCrashPipe).get(), nullptr);
  gExceptionHandler->set_handle_debug_exceptions(true);

#  if defined(HAVE_64BIT_BUILD)
  SetJitExceptionHandler();
#  endif
#elif defined(XP_LINUX)
  // MinidumpDescriptor requires a non-empty path.
  google_breakpad::MinidumpDescriptor path(".");

  gExceptionHandler = new google_breakpad::ExceptionHandler(
      path, ChildFilter, ChildMinidumpCallback,
      nullptr,  // no callback context
      true,     // install signal handlers
      gMagicChildCrashReportFd);
#elif defined(XP_MACOSX)
  gExceptionHandler = new google_breakpad::ExceptionHandler(
      "", ChildFilter, ChildMinidumpCallback,
      nullptr,  // no callback context
      true,     // install signal handlers
      aCrashPipe);
#endif

  RecordMainThreadId();

  oldTerminateHandler = std::set_terminate(&TerminateHandler);

  // we either do remote or nothing, no fallback to regular crash reporting
  return gExceptionHandler->IsOutOfProcess();
}

void GetAnnotation(ProcessId childPid, Annotation annotation,
                   nsACString& outStr) {
  if (!GetEnabled()) {
    return;
  }

  MutexAutoLock lock(*dumpMapLock);

  ChildProcessData* pd = pidToMinidump->GetEntry(childPid);
  if (!pd) {
    return;
  }

  outStr = (*pd->annotations)[annotation];
}

bool TakeMinidumpForChild(ProcessId childPid, nsIFile** dump,
                          AnnotationTable& aAnnotations) {
  if (!GetEnabled()) return false;

  MutexAutoLock lock(*dumpMapLock);

  ChildProcessData* pd = pidToMinidump->GetEntry(childPid);
  if (!pd) return false;

  NS_IF_ADDREF(*dump = pd->minidump);
  aAnnotations = *(pd->annotations);

  pidToMinidump->RemoveEntry(pd);

  return !!*dump;
}

bool FinalizeOrphanedMinidump(ProcessId aChildPid, GeckoProcessType aType,
                              nsString* aDumpId) {
  AnnotationTable annotations;
  nsCOMPtr<nsIFile> minidump;

  if (!TakeMinidumpForChild(aChildPid, getter_AddRefs(minidump), annotations)) {
    return false;
  }

  nsAutoString id;
  if (!GetIDFromMinidump(minidump, id)) {
    return false;
  }

  if (aDumpId) {
    *aDumpId = id;
  }

  annotations[Annotation::ProcessType] =
      XRE_ChildProcessTypeToAnnotation(aType);

  return WriteExtraFile(id, annotations);
}

#ifdef XP_WIN

// Function invoked by the WER runtime exception handler running in an
// external process. This function isn't used anywhere inside Gecko directly
// but rather invoked via CreateRemoteThread() in the main process.

// Store this global in a section called mozwerpt where we can find it by just
// looking at the program headers.
#  pragma section("mozwerpt", read, executable, shared)

__declspec(allocate("mozwerpt")) MOZ_EXPORT DWORD WINAPI
WerNotifyProc(LPVOID aParameter) {
  const WindowsErrorReportingData* werData =
      static_cast<const WindowsErrorReportingData*>(aParameter);

  auto freeParameterOnExit = MakeScopeExit([&aParameter] {
    VirtualFree(aParameter, sizeof(WindowsErrorReportingData), MEM_RELEASE);
  });

  // Hold the mutex until the current dump request is complete, to
  // prevent UnsetExceptionHandler() from pulling the rug out from
  // under us.
  MutexAutoLock safetyLock(*dumpSafetyLock);
  if (!isSafeToDump || !ShouldReport()) {
    return S_OK;
  }

  ProcessId pid = werData->mChildPid;
  nsCOMPtr<nsIFile> minidump;
  if (!GetPendingDir(getter_AddRefs(minidump))) {
    return S_OK;
  }
  xpstring minidump_native_name(werData->mMinidumpFile,
                                werData->mMinidumpFile + 40);
  nsString minidump_name(minidump_native_name.c_str());
  minidump->Append(minidump_name);

  {
    MutexAutoLock lock(*dumpMapLock);
    ChildProcessData* pd = pidToMinidump->PutEntry(pid);
    MOZ_ASSERT(!pd->minidump);
    pd->minidump = minidump;
    pd->annotations = MakeUnique<AnnotationTable>();
    (*pd->annotations)[Annotation::WindowsErrorReporting] = "1"_ns;
    AddSharedAnnotations(*(pd->annotations));
  }

  return S_OK;
}

#endif  // XP_WIN

//-----------------------------------------------------------------------------
// CreateMinidumpsAndPair() and helpers
//

/*
 * Renames the stand alone dump file aDumpFile to:
 *  |aOwnerDumpFile-aDumpFileProcessType.dmp|
 * and moves it into the same directory as aOwnerDumpFile. Does not
 * modify aOwnerDumpFile in any way.
 *
 * @param aDumpFile - the dump file to associate with aOwnerDumpFile.
 * @param aOwnerDumpFile - the new owner of aDumpFile.
 * @param aDumpFileProcessType - process name associated with aDumpFile.
 */
static void RenameAdditionalHangMinidump(nsIFile* minidump,
                                         nsIFile* childMinidump,
                                         const nsACString& name) {
  nsCOMPtr<nsIFile> directory;
  childMinidump->GetParent(getter_AddRefs(directory));
  if (!directory) return;

  nsAutoCString leafName;
  childMinidump->GetNativeLeafName(leafName);

  // turn "<id>.dmp" into "<id>-<name>.dmp
  leafName.Insert("-"_ns + name, leafName.Length() - 4);

  if (NS_FAILED(minidump->MoveToNative(directory, leafName))) {
    NS_WARNING("RenameAdditionalHangMinidump failed to move minidump.");
  }
}

// Stores the minidump in the nsIFile pointed by the |context| parameter.
static bool PairedDumpCallback(
#ifdef XP_LINUX
    const MinidumpDescriptor& descriptor,
#else
    const XP_CHAR* dump_path, const XP_CHAR* minidump_id,
#endif
    void* context,
#ifdef XP_WIN
    EXCEPTION_POINTERS* /*unused*/, MDRawAssertionInfo* /*unused*/,
#endif
    const phc::AddrInfo* addrInfo, bool succeeded) {
  XP_CHAR* path = static_cast<XP_CHAR*>(context);
  size_t size = XP_PATH_MAX;

#ifdef XP_LINUX
  Concat(path, descriptor.path(), &size);
#else
  path = Concat(path, dump_path, &size);
  path = Concat(path, XP_PATH_SEPARATOR, &size);
  path = Concat(path, minidump_id, &size);
  Concat(path, dumpFileExtension, &size);
#endif

  return true;
}

ThreadId CurrentThreadId() {
#if defined(XP_WIN)
  return ::GetCurrentThreadId();
#elif defined(XP_LINUX)
  return sys_gettid();
#elif defined(XP_MACOSX)
  // Just return an index, since Mach ports can't be directly serialized
  thread_act_port_array_t threads_for_task;
  mach_msg_type_number_t thread_count;

  if (task_threads(mach_task_self(), &threads_for_task, &thread_count))
    return -1;

  for (unsigned int i = 0; i < thread_count; ++i) {
    if (threads_for_task[i] == mach_thread_self()) return i;
  }
  abort();
#else
#  error "Unsupported platform"
#endif
}

#ifdef XP_MACOSX
static mach_port_t GetChildThread(ProcessHandle childPid,
                                  ThreadId childBlamedThread) {
  mach_port_t childThread = MACH_PORT_NULL;
  thread_act_port_array_t threads_for_task;
  mach_msg_type_number_t thread_count;

  if (task_threads(childPid, &threads_for_task, &thread_count) ==
          KERN_SUCCESS &&
      childBlamedThread < thread_count) {
    childThread = threads_for_task[childBlamedThread];
  }

  return childThread;
}
#endif

bool CreateMinidumpsAndPair(ProcessHandle aTargetHandle,
                            ThreadId aTargetBlamedThread,
                            const nsACString& aIncomingPairName,
                            AnnotationTable& aTargetAnnotations,
                            nsIFile** aMainDumpOut) {
  if (!GetEnabled()) {
    return false;
  }

  AutoIOInterposerDisable disableIOInterposition;

#ifdef XP_MACOSX
  mach_port_t targetThread = GetChildThread(aTargetHandle, aTargetBlamedThread);
#else
  ThreadId targetThread = aTargetBlamedThread;
#endif

  xpstring dump_path;
#ifndef XP_LINUX
  dump_path = gExceptionHandler->dump_path();
#else
  dump_path = gExceptionHandler->minidump_descriptor().directory();
#endif

  // Ugly, but due to Breakpad limitations we can't allocate memory in the
  // callback when generating a dump of the calling process.
  XP_CHAR minidumpPath[XP_PATH_MAX] = {};

  // dump the target
  if (!google_breakpad::ExceptionHandler::WriteMinidumpForChild(
          aTargetHandle, targetThread, dump_path, PairedDumpCallback,
          static_cast<void*>(minidumpPath)
#ifdef XP_WIN
              ,
          GetMinidumpType()
#endif
              )) {
    return false;
  }

  nsCOMPtr<nsIFile> targetMinidump;
  CreateFileFromPath(xpstring(minidumpPath), getter_AddRefs(targetMinidump));

  // Create a dump of this process.
  if (!google_breakpad::ExceptionHandler::WriteMinidump(
          dump_path,
#ifdef XP_MACOSX
          true,
#endif
          PairedDumpCallback, static_cast<void*>(minidumpPath)
#ifdef XP_WIN
                                  ,
          GetMinidumpType()
#endif
              )) {
    targetMinidump->Remove(false);
    return false;
  }

  nsCOMPtr<nsIFile> incomingDump;
  CreateFileFromPath(xpstring(minidumpPath), getter_AddRefs(incomingDump));

  RenameAdditionalHangMinidump(incomingDump, targetMinidump, aIncomingPairName);

  if (ShouldReport()) {
    MoveToPending(targetMinidump, nullptr, nullptr);
    MoveToPending(incomingDump, nullptr, nullptr);
  }
#if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
  DllBlocklist_Shutdown();
#endif

  AddSharedAnnotations(aTargetAnnotations);
#if XP_WIN
  nsTArray<CAnnotation>* child_annotations =
      mozannotation_retrieve(reinterpret_cast<uintptr_t>(aTargetHandle),
                             static_cast<size_t>(Annotation::Count));
#else
  nsTArray<CAnnotation>* child_annotations = mozannotation_retrieve(
      aTargetHandle, static_cast<size_t>(Annotation::Count));
#endif
  AddChildProcessAnnotations(aTargetAnnotations, child_annotations);
  if (child_annotations) {
    mozannotation_free(child_annotations);
  }

  targetMinidump.forget(aMainDumpOut);

  return true;
}

bool UnsetRemoteExceptionHandler(bool wasSet) {
  // On Linux we don't unset breakpad's exception handler if the sandbox is
  // enabled because it requires invoking `sigaltstack` and we don't want to
  // allow that syscall in the sandbox. See bug 1622452.
#if !defined(XP_LINUX) || !defined(MOZ_SANDBOX)
  if (wasSet) {
    std::set_terminate(oldTerminateHandler);
    delete gExceptionHandler;
    gExceptionHandler = nullptr;
  }
#endif
  gServerURL = "";
  TeardownAppNotes();

  return true;
}

#if defined(MOZ_WIDGET_ANDROID)
void SetNotificationPipeForChild(int childCrashFd) {
  gMagicChildCrashReportFd = childCrashFd;
}
#endif

}  // namespace CrashReporter
