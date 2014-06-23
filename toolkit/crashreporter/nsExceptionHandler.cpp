/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExceptionHandler.h"
#include "nsDataHashtable.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/CrashReporterChild.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "mozilla/unused.h"

#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#if defined(XP_WIN32)
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "nsXULAppAPI.h"
#include "nsIXULAppInfo.h"
#include "nsIWindowsRegKey.h"
#include "client/windows/crash_generation/client_info.h"
#include "client/windows/crash_generation/crash_generation_server.h"
#include "client/windows/handler/exception_handler.h"
#include <dbghelp.h>
#include <string.h>
#include "nsDirectoryServiceUtils.h"

#include "nsWindowsDllInterceptor.h"
#elif defined(XP_MACOSX)
#include "client/mac/crash_generation/client_info.h"
#include "client/mac/crash_generation/crash_generation_server.h"
#include "client/mac/handler/exception_handler.h"
#include <string>
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <crt_externs.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <spawn.h>
#include <unistd.h>
#include "mac_utils.h"
#elif defined(XP_LINUX)
#include "nsIINIParser.h"
#include "common/linux/linux_libc_support.h"
#include "third_party/lss/linux_syscall_support.h"
#include "client/linux/crash_generation/client_info.h"
#include "client/linux/crash_generation/crash_generation_server.h"
#include "client/linux/handler/exception_handler.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(XP_SOLARIS)
#include "client/solaris/handler/exception_handler.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#else
#error "Not yet implemented for this platform"
#endif // defined(XP_WIN32)

#ifdef MOZ_CRASHREPORTER_INJECTOR
#include "InjectCrashReporter.h"
using mozilla::InjectCrashRunnable;
#endif

#include <stdlib.h>
#include <time.h>
#include <prenv.h>
#include <prio.h>
#include <prmem.h>
#include "mozilla/Mutex.h"
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "prprf.h"
#include <map>
#include <vector>

#include "mozilla/IOInterposer.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/WindowsDllBlocklist.h"

#if defined(XP_MACOSX)
CFStringRef reporterClientAppID = CFSTR("org.mozilla.crashreporter");
#endif
#if defined(MOZ_WIDGET_ANDROID)
#include "common/linux/file_id.h"
#endif

using google_breakpad::CrashGenerationServer;
using google_breakpad::ClientInfo;
#ifdef XP_LINUX
using google_breakpad::MinidumpDescriptor;
#endif
using namespace mozilla;
using mozilla::dom::CrashReporterChild;
using mozilla::dom::PCrashReporterChild;

namespace CrashReporter {

#ifdef XP_WIN32
typedef wchar_t XP_CHAR;
typedef std::wstring xpstring;
#define CONVERT_XP_CHAR_TO_UTF16(x) x
#define XP_STRLEN(x) wcslen(x)
#define my_strlen strlen
#define CRASH_REPORTER_FILENAME "crashreporter.exe"
#define PATH_SEPARATOR "\\"
#define XP_PATH_SEPARATOR L"\\"
// sort of arbitrary, but MAX_PATH is kinda small
#define XP_PATH_MAX 4096
// "<reporter path>" "<minidump path>"
#define CMDLINE_SIZE ((XP_PATH_MAX * 2) + 6)
#ifdef _USE_32BIT_TIME_T
#define XP_TTOA(time, buffer, base) ltoa(time, buffer, base)
#else
#define XP_TTOA(time, buffer, base) _i64toa(time, buffer, base)
#endif
#define XP_STOA(size, buffer, base) _ui64toa(size, buffer, base)
#else
typedef char XP_CHAR;
typedef std::string xpstring;
#define CONVERT_XP_CHAR_TO_UTF16(x) NS_ConvertUTF8toUTF16(x)
#define CRASH_REPORTER_FILENAME "crashreporter"
#define PATH_SEPARATOR "/"
#define XP_PATH_SEPARATOR "/"
#define XP_PATH_MAX PATH_MAX
#ifdef XP_LINUX
#define XP_STRLEN(x) my_strlen(x)
#define XP_TTOA(time, buffer, base) my_inttostring(time, buffer, sizeof(buffer))
#define XP_STOA(size, buffer, base) my_inttostring(size, buffer, sizeof(buffer))
#else
#define XP_STRLEN(x) strlen(x)
#define XP_TTOA(time, buffer, base) sprintf(buffer, "%ld", time)
#define XP_STOA(size, buffer, base) sprintf(buffer, "%zu", size)
#define my_strlen strlen
#define sys_close close
#define sys_fork fork
#define sys_open open
#define sys_write write
#endif
#endif // XP_WIN32

#ifndef XP_LINUX
static const XP_CHAR dumpFileExtension[] = {'.', 'd', 'm', 'p',
                                            '\0'}; // .dmp
#endif

static const XP_CHAR extraFileExtension[] = {'.', 'e', 'x', 't',
                                             'r', 'a', '\0'}; // .extra

static const char kCrashMainID[] = "crash.main.1\n";

static google_breakpad::ExceptionHandler* gExceptionHandler = nullptr;

static XP_CHAR* pendingDirectory;
static XP_CHAR* crashReporterPath;

// Where crash events should go.
static XP_CHAR* eventsDirectory;

// If this is false, we don't launch the crash reporter
static bool doReport = true;

// If this is true, we don't have a crash reporter
static bool headlessClient = false;

// if this is true, we pass the exception on to the OS crash reporter
static bool showOSCrashReporter = false;

// The time of the last recorded crash, as a time_t value.
static time_t lastCrashTime = 0;
// The pathname of a file to store the crash time in
static XP_CHAR lastCrashTimeFilename[XP_PATH_MAX] = {0};

// A marker file to hold the path to the last dump written, which
// will be checked on startup.
static XP_CHAR crashMarkerFilename[XP_PATH_MAX] = {0};

// Whether we've already looked for the marker file.
static bool lastRunCrashID_checked = false;
// The minidump ID contained in the marker file.
static nsString* lastRunCrashID = nullptr;

#if defined(MOZ_WIDGET_ANDROID)
// on Android 4.2 and above there is a user serial number associated
// with the current process that gets lost when we fork so we need to
// explicitly pass it to am
static char* androidUserSerial = nullptr;
#endif

// these are just here for readability
static const char kCrashTimeParameter[] = "CrashTime=";
static const int kCrashTimeParameterLen = sizeof(kCrashTimeParameter)-1;

static const char kTimeSinceLastCrashParameter[] = "SecondsSinceLastCrash=";
static const int kTimeSinceLastCrashParameterLen =
                                     sizeof(kTimeSinceLastCrashParameter)-1;

static const char kOOMAllocationSizeParameter[] = "OOMAllocationSize=";
static const int kOOMAllocationSizeParameterLen =
  sizeof(kOOMAllocationSizeParameter)-1;


#ifdef XP_WIN32
static const char kSysMemoryParameter[] = "SystemMemoryUsePercentage=";
static const int kSysMemoryParameterLen = sizeof(kSysMemoryParameter)-1;

static const char kTotalVirtualMemoryParameter[] = "TotalVirtualMemory=";
static const int kTotalVirtualMemoryParameterLen =
  sizeof(kTotalVirtualMemoryParameter)-1;

static const char kAvailableVirtualMemoryParameter[] = "AvailableVirtualMemory=";
static const int kAvailableVirtualMemoryParameterLen =
  sizeof(kAvailableVirtualMemoryParameter)-1;

static const char kTotalPageFileParameter[] = "TotalPageFile=";
static const int kTotalPageFileParameterLen =
  sizeof(kTotalPageFileParameter)-1;

static const char kAvailablePageFileParameter[] = "AvailablePageFile=";
static const int kAvailablePageFileParameterLen =
  sizeof(kAvailablePageFileParameter)-1;

static const char kTotalPhysicalMemoryParameter[] = "TotalPhysicalMemory=";
static const int kTotalPhysicalMemoryParameterLen =
  sizeof(kTotalPhysicalMemoryParameter)-1;

static const char kAvailablePhysicalMemoryParameter[] = "AvailablePhysicalMemory=";
static const int kAvailablePhysicalMemoryParameterLen =
  sizeof(kAvailablePhysicalMemoryParameter)-1;
#endif

static const char kIsGarbageCollectingParameter[] = "IsGarbageCollecting=";
static const int kIsGarbageCollectingParameterLen =
  sizeof(kIsGarbageCollectingParameter)-1;

static const char kEventLoopNestingLevelParameter[] = "EventLoopNestingLevel=";
static const int kEventLoopNestingLevelParameterLen =
  sizeof(kEventLoopNestingLevelParameter)-1;

#ifdef XP_WIN
static const char kBreakpadReserveAddressParameter[] = "BreakpadReserveAddress=";
static const int kBreakpadReserveAddressParameterLen =
  sizeof(kBreakpadReserveAddressParameter)-1;

static const char kBreakpadReserveSizeParameter[] = "BreakpadReserveSize=";
static const int kBreakpadReserveSizeParameterLen =
  sizeof(kBreakpadReserveSizeParameter)-1;
#endif

// this holds additional data sent via the API
static Mutex* crashReporterAPILock;
static Mutex* notesFieldLock;
static AnnotationTable* crashReporterAPIData_Hash;
static nsCString* crashReporterAPIData = nullptr;
static nsCString* notesField = nullptr;
static bool isGarbageCollecting;
static uint32_t eventloopNestingLevel = 0;

// Avoid a race during application termination.
static Mutex* dumpSafetyLock;
static bool isSafeToDump = false;

// OOP crash reporting
static CrashGenerationServer* crashServer; // chrome process has this

#  if defined(XP_WIN) || defined(XP_MACOSX)
// If crash reporting is disabled, we hand out this "null" pipe to the
// child process and don't attempt to connect to a parent server.
static const char kNullNotifyPipe[] = "-";
static char* childCrashNotifyPipe;

#  elif defined(XP_LINUX)
static int serverSocketFd = -1;
static int clientSocketFd = -1;
static const int kMagicChildCrashReportFd = 4;

#  endif

// |dumpMapLock| must protect all access to |pidToMinidump|.
static Mutex* dumpMapLock;
struct ChildProcessData : public nsUint32HashKey
{
  ChildProcessData(KeyTypePointer aKey)
    : nsUint32HashKey(aKey)
    , sequence(0)
#ifdef MOZ_CRASHREPORTER_INJECTOR
    , callback(nullptr)
#endif
  { }

  nsCOMPtr<nsIFile> minidump;
  // Each crashing process is assigned an increasing sequence number to
  // indicate which process crashed first.
  uint32_t sequence;
#ifdef MOZ_CRASHREPORTER_INJECTOR
  InjectorCrashCallback* callback;
#endif
};

typedef nsTHashtable<ChildProcessData> ChildMinidumpMap;
static ChildMinidumpMap* pidToMinidump;
static uint32_t crashSequence;
static bool OOPInitialized();

#ifdef MOZ_CRASHREPORTER_INJECTOR
static nsIThread* sInjectorThread;

class ReportInjectedCrash : public nsRunnable
{
public:
  ReportInjectedCrash(uint32_t pid) : mPID(pid) { }

  NS_IMETHOD Run();

private:
  uint32_t mPID;
};
#endif // MOZ_CRASHREPORTER_INJECTOR

// Crashreporter annotations that we don't send along in subprocess
// reports
static const char* kSubprocessBlacklist[] = {
  "FramePoisonBase",
  "FramePoisonSize",
  "StartupTime",
  "URL"
};

// If annotations are attempted before the crash reporter is enabled,
// they queue up here.
class DelayedNote;
nsTArray<nsAutoPtr<DelayedNote> >* gDelayedAnnotations;

#if defined(XP_WIN)
// the following are used to prevent other DLLs reverting the last chance
// exception handler to the windows default. Any attempt to change the 
// unhandled exception filter or to reset it is ignored and our crash
// reporter is loaded instead (in case it became unloaded somehow)
typedef LPTOP_LEVEL_EXCEPTION_FILTER (WINAPI *SetUnhandledExceptionFilter_func)
  (LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);
static SetUnhandledExceptionFilter_func stub_SetUnhandledExceptionFilter = 0;
static LPTOP_LEVEL_EXCEPTION_FILTER previousUnhandledExceptionFilter = nullptr;
static WindowsDllInterceptor gKernel32Intercept;
static bool gBlockUnhandledExceptionFilter = true;

static void NotePreviousUnhandledExceptionFilter()
{
  // Set a dummy value to get the previous filter, then restore
  previousUnhandledExceptionFilter = SetUnhandledExceptionFilter(nullptr);
  SetUnhandledExceptionFilter(previousUnhandledExceptionFilter);
}

static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI
patched_SetUnhandledExceptionFilter (LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
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

/**
 * Reserve some VM space. In the event that we crash because VM space is
 * being leaked without leaking memory, freeing this space before taking
 * the minidump will allow us to collect a minidump.
 *
 * This size is bigger than xul.dll plus some extra for MinidumpWriteDump
 * allocations.
 */
static const SIZE_T kReserveSize = 0x2400000; // 36 MB
static void* gBreakpadReservedVM;
#endif

#ifdef XP_MACOSX
static cpu_type_t pref_cpu_types[2] = {
#if defined(__i386__)
                                 CPU_TYPE_X86,
#elif defined(__x86_64__)
                                 CPU_TYPE_X86_64,
#elif defined(__ppc__)
                                 CPU_TYPE_POWERPC,
#endif
                                 CPU_TYPE_ANY };

static posix_spawnattr_t spawnattr;
#endif

#if defined(MOZ_WIDGET_ANDROID)
// Android builds use a custom library loader,
// so the embedding will provide a list of shared
// libraries that are mapped into anonymous mappings.
typedef struct {
  std::string name;
  uintptr_t   start_address;
  size_t      length;
  size_t      file_offset;
} mapping_info;
static std::vector<mapping_info> library_mappings;
typedef std::map<uint32_t,google_breakpad::MappingList> MappingMap;
#endif

#ifdef XP_LINUX
inline void
my_inttostring(intmax_t t, char* buffer, size_t buffer_length)
{
  my_memset(buffer, 0, buffer_length);
  my_uitos(buffer, t, my_uint_len(t));
}
#endif

#ifdef XP_WIN
static void
CreateFileFromPath(const xpstring& path, nsIFile** file)
{
  NS_NewLocalFile(nsDependentString(path.c_str()), false, file);
}
#else
static void
CreateFileFromPath(const xpstring& path, nsIFile** file)
{
  NS_NewNativeLocalFile(nsDependentCString(path.c_str()), false, file);
}
#endif

static XP_CHAR*
Concat(XP_CHAR* str, const XP_CHAR* toAppend, int* size)
{
  int appendLen = XP_STRLEN(toAppend);
  if (appendLen >= *size) appendLen = *size - 1;

  memcpy(str, toAppend, appendLen * sizeof(XP_CHAR));
  str += appendLen;
  *str = '\0';
  *size -= appendLen;

  return str;
}

static size_t gOOMAllocationSize = 0;

void AnnotateOOMAllocationSize(size_t size)
{
  gOOMAllocationSize = size;
}

bool MinidumpCallback(
#ifdef XP_LINUX
                      const MinidumpDescriptor& descriptor,
#else
                      const XP_CHAR* dump_path,
                      const XP_CHAR* minidump_id,
#endif
                      void* context,
#ifdef XP_WIN32
                      EXCEPTION_POINTERS* exinfo,
                      MDRawAssertionInfo* assertion,
#endif
                      bool succeeded)
{
  bool returnValue = showOSCrashReporter ? false : succeeded;

  static XP_CHAR minidumpPath[XP_PATH_MAX];
  int size = XP_PATH_MAX;
  XP_CHAR* p;
#ifndef XP_LINUX
  p = Concat(minidumpPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
  Concat(p, dumpFileExtension, &size);
#else
  Concat(minidumpPath, descriptor.path(), &size);
#endif

  static XP_CHAR extraDataPath[XP_PATH_MAX];
  size = XP_PATH_MAX;
#ifndef XP_LINUX
  p = Concat(extraDataPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
#else
  p = Concat(extraDataPath, descriptor.path(), &size);
  // Skip back past the .dmp extension.
  p -= 4;
#endif
  Concat(p, extraFileExtension, &size);

  if (headlessClient) {
    // Leave a marker indicating that there was a crash.
#if defined(XP_WIN32)
    HANDLE hFile = CreateFile(crashMarkerFilename, GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if(hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, minidumpPath, 2*wcslen(minidumpPath), &nBytes, nullptr);
      CloseHandle(hFile);
    }
#elif defined(XP_UNIX)
    int fd = sys_open(crashMarkerFilename,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0600);
    if (fd != -1) {
      unused << sys_write(fd, minidumpPath, my_strlen(minidumpPath));
      sys_close(fd);
    }
#endif
  }

  char oomAllocationSizeBuffer[32];
  int oomAllocationSizeBufferLen = 0;
  if (gOOMAllocationSize) {
    XP_STOA(gOOMAllocationSize, oomAllocationSizeBuffer, 10);
    oomAllocationSizeBufferLen = my_strlen(oomAllocationSizeBuffer);
  }

  // calculate time since last crash (if possible), and store
  // the time of this crash.
  time_t crashTime;
#ifdef XP_LINUX
  struct kernel_timeval tv;
  sys_gettimeofday(&tv, nullptr);
  crashTime = tv.tv_sec;
#else
  crashTime = time(nullptr);
#endif
  time_t timeSinceLastCrash = 0;
  // stringified versions of the above
  char crashTimeString[32];
  int crashTimeStringLen = 0;
  char timeSinceLastCrashString[32];
  int timeSinceLastCrashStringLen = 0;

  XP_TTOA(crashTime, crashTimeString, 10);
  crashTimeStringLen = my_strlen(crashTimeString);
  if (lastCrashTime != 0) {
    timeSinceLastCrash = crashTime - lastCrashTime;
    XP_TTOA(timeSinceLastCrash, timeSinceLastCrashString, 10);
    timeSinceLastCrashStringLen = my_strlen(timeSinceLastCrashString);
  }
  // write crash time to file
  if (lastCrashTimeFilename[0] != 0) {
#if defined(XP_WIN32)
    HANDLE hFile = CreateFile(lastCrashTimeFilename, GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if(hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, crashTimeString, crashTimeStringLen, &nBytes, nullptr);
      CloseHandle(hFile);
    }
#elif defined(XP_UNIX)
    int fd = sys_open(lastCrashTimeFilename,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0600);
    if (fd != -1) {
      unused << sys_write(fd, crashTimeString, crashTimeStringLen);
      sys_close(fd);
    }
#endif
  }

  // Write crash event file.

  // Minidump IDs are UUIDs (36) + NULL.
  static char id_ascii[37];
#ifdef XP_LINUX
  const char * index = strrchr(descriptor.path(), '/');
  MOZ_ASSERT(index);
  MOZ_ASSERT(strlen(index) == 1 + 36 + 4); // "/" + UUID + ".dmp"
  for (uint32_t i = 0; i < 36; i++) {
    id_ascii[i] = *(index + 1 + i);
  }
#else
  MOZ_ASSERT(XP_STRLEN(minidump_id) == 36);
  for (uint32_t i = 0; i < 36; i++) {
    id_ascii[i] = *((char *)(minidump_id + i));
  }
#endif

  if (eventsDirectory) {
    static XP_CHAR crashEventPath[XP_PATH_MAX];
    int size = XP_PATH_MAX;
    XP_CHAR* p;
    p = Concat(crashEventPath, eventsDirectory, &size);
    p = Concat(p, XP_PATH_SEPARATOR, &size);
#ifdef XP_LINUX
    p = Concat(p, id_ascii, &size);
#else
    p = Concat(p, minidump_id, &size);
#endif

#if defined(XP_WIN32)
    HANDLE hFile = CreateFile(crashEventPath, GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, kCrashMainID, sizeof(kCrashMainID) - 1, &nBytes,
                nullptr);
      WriteFile(hFile, crashTimeString, crashTimeStringLen, &nBytes, nullptr);
      WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      WriteFile(hFile, id_ascii, strlen(id_ascii), &nBytes, nullptr);
      CloseHandle(hFile);
    }
#elif defined(XP_UNIX)
    int fd = sys_open(crashEventPath,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0600);
    if (fd != -1) {
      unused << sys_write(fd, kCrashMainID, sizeof(kCrashMainID) - 1);
      unused << sys_write(fd, crashTimeString, crashTimeStringLen);
      unused << sys_write(fd, "\n", 1);
      unused << sys_write(fd, id_ascii, strlen(id_ascii));
      sys_close(fd);
    }
#endif
  }

#if defined(XP_WIN32)
  if (!crashReporterAPIData->IsEmpty()) {
    // write out API data
    HANDLE hFile = CreateFile(extraDataPath, GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if(hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, crashReporterAPIData->get(),
                crashReporterAPIData->Length(), &nBytes, nullptr);
      WriteFile(hFile, kCrashTimeParameter, kCrashTimeParameterLen,
                &nBytes, nullptr);
      WriteFile(hFile, crashTimeString, crashTimeStringLen, &nBytes, nullptr);
      WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      if (timeSinceLastCrash != 0) {
        WriteFile(hFile, kTimeSinceLastCrashParameter,
                  kTimeSinceLastCrashParameterLen, &nBytes, nullptr);
        WriteFile(hFile, timeSinceLastCrashString, timeSinceLastCrashStringLen,
                  &nBytes, nullptr);
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      }
      if (isGarbageCollecting) {
        WriteFile(hFile, kIsGarbageCollectingParameter, kIsGarbageCollectingParameterLen,
                  &nBytes, nullptr);
        WriteFile(hFile, isGarbageCollecting ? "1" : "0", 1, &nBytes, nullptr);
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      }

      char buffer[128];
      int bufferLen;

      if (eventloopNestingLevel > 0) {
        WriteFile(hFile, kEventLoopNestingLevelParameter, kEventLoopNestingLevelParameterLen,
                  &nBytes, nullptr);
        _ultoa(eventloopNestingLevel, buffer, 10);
        WriteFile(hFile, buffer, strlen(buffer), &nBytes, nullptr);
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      }

      if (gBreakpadReservedVM) {
        WriteFile(hFile, kBreakpadReserveAddressParameter, kBreakpadReserveAddressParameterLen, &nBytes, nullptr);
        _ui64toa(uintptr_t(gBreakpadReservedVM), buffer, 10);
        WriteFile(hFile, buffer, strlen(buffer), &nBytes, nullptr);
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);
        WriteFile(hFile, kBreakpadReserveSizeParameter, kBreakpadReserveSizeParameterLen, &nBytes, nullptr);
        _ui64toa(kReserveSize, buffer, 10);
        WriteFile(hFile, buffer, strlen(buffer), &nBytes, nullptr);
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      }

#ifdef HAS_DLL_BLOCKLIST
      DllBlocklist_WriteNotes(hFile);
#endif

      // Try to get some information about memory.
      MEMORYSTATUSEX statex;
      statex.dwLength = sizeof(statex);
      if (GlobalMemoryStatusEx(&statex)) {

#define WRITE_STATEX_FIELD(field, paramName, conversionFunc)     \
        WriteFile(hFile, k##paramName##Parameter,                \
                  k##paramName##ParameterLen, &nBytes, nullptr); \
        conversionFunc(statex.field, buffer, 10);                \
        bufferLen = strlen(buffer);                              \
        WriteFile(hFile, buffer, bufferLen, &nBytes, nullptr);   \
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);

        WRITE_STATEX_FIELD(dwMemoryLoad, SysMemory, ltoa);
        WRITE_STATEX_FIELD(ullTotalVirtual, TotalVirtualMemory, _ui64toa);
        WRITE_STATEX_FIELD(ullAvailVirtual, AvailableVirtualMemory, _ui64toa);
        WRITE_STATEX_FIELD(ullTotalPageFile, TotalPageFile, _ui64toa);
        WRITE_STATEX_FIELD(ullAvailPageFile, AvailablePageFile, _ui64toa);
        WRITE_STATEX_FIELD(ullTotalPhys, TotalPhysicalMemory, _ui64toa);
        WRITE_STATEX_FIELD(ullAvailPhys, AvailablePhysicalMemory, _ui64toa);

#undef WRITE_STATEX_FIELD
      }

      if (oomAllocationSizeBufferLen) {
        WriteFile(hFile, kOOMAllocationSizeParameter,
                  kOOMAllocationSizeParameterLen, &nBytes, nullptr);
        WriteFile(hFile, oomAllocationSizeBuffer, oomAllocationSizeBufferLen,
                  &nBytes, nullptr);
        WriteFile(hFile, "\n", 1, &nBytes, nullptr);
      }
      CloseHandle(hFile);
    }
  }

  if (!doReport) {
    return returnValue;
  }

  XP_CHAR cmdLine[CMDLINE_SIZE];
  size = CMDLINE_SIZE;
  p = Concat(cmdLine, L"\"", &size);
  p = Concat(p, crashReporterPath, &size);
  p = Concat(p, L"\" \"", &size);
  p = Concat(p, minidumpPath, &size);
  Concat(p, L"\"", &size);

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWNORMAL;
  ZeroMemory(&pi, sizeof(pi));

  if (CreateProcess(nullptr, (LPWSTR)cmdLine, nullptr, nullptr, FALSE, 0,
                    nullptr, nullptr, &si, &pi)) {
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
  }
  // we're not really in a position to do anything if the CreateProcess fails
  TerminateProcess(GetCurrentProcess(), 1);
#elif defined(XP_UNIX)
  if (!crashReporterAPIData->IsEmpty()) {
    // write out API data
    int fd = sys_open(extraDataPath,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0666);

    if (fd != -1) {
      // not much we can do in case of error
      unused << sys_write(fd, crashReporterAPIData->get(),
                                  crashReporterAPIData->Length());
      unused << sys_write(fd, kCrashTimeParameter, kCrashTimeParameterLen);
      unused << sys_write(fd, crashTimeString, crashTimeStringLen);
      unused << sys_write(fd, "\n", 1);
      if (timeSinceLastCrash != 0) {
        unused << sys_write(fd, kTimeSinceLastCrashParameter,
                        kTimeSinceLastCrashParameterLen);
        unused << sys_write(fd, timeSinceLastCrashString,
                        timeSinceLastCrashStringLen);
        unused << sys_write(fd, "\n", 1);
      }
      if (isGarbageCollecting) {
        unused << sys_write(fd, kIsGarbageCollectingParameter, kIsGarbageCollectingParameterLen);
        unused << sys_write(fd, isGarbageCollecting ? "1" : "0", 1);
        unused << sys_write(fd, "\n", 1);
      }
      if (eventloopNestingLevel > 0) {
        unused << sys_write(fd, kEventLoopNestingLevelParameter, kEventLoopNestingLevelParameterLen);
        char buffer[16];
        XP_TTOA(static_cast<time_t>(eventloopNestingLevel), buffer, 10);
        unused << sys_write(fd, buffer, my_strlen(buffer));
        unused << sys_write(fd, "\n", 1);
      }
      if (oomAllocationSizeBufferLen) {
        unused << sys_write(fd, kOOMAllocationSizeParameter,
                            kOOMAllocationSizeParameterLen);
        unused << sys_write(fd, oomAllocationSizeBuffer,
                            oomAllocationSizeBufferLen);
        unused << sys_write(fd, "\n", 1);
      }
      sys_close(fd);
    }
  }

  if (!doReport) {
    return returnValue;
  }

#ifdef XP_MACOSX
  char* const my_argv[] = {
    crashReporterPath,
    minidumpPath,
    nullptr
  };

  char **env = nullptr;
  char ***nsEnv = _NSGetEnviron();
  if (nsEnv)
    env = *nsEnv;
  int result = posix_spawnp(nullptr,
                            my_argv[0],
                            nullptr,
                            &spawnattr,
                            my_argv,
                            env);

  if (result != 0)
    return false;

#else // !XP_MACOSX
  pid_t pid = sys_fork();

  if (pid == -1)
    return false;
  else if (pid == 0) {
#if !defined(MOZ_WIDGET_ANDROID)
    // need to clobber this, as libcurl might load NSS,
    // and we want it to load the system NSS.
    unsetenv("LD_LIBRARY_PATH");
    unused << execl(crashReporterPath,
                    crashReporterPath, minidumpPath, (char*)0);
#else
    // Invoke the reportCrash activity using am
    if (androidUserSerial) {
      unused << execlp("/system/bin/am",
                       "/system/bin/am",
                       "start",
                       "--user", androidUserSerial,
                       "-a", "org.mozilla.gecko.reportCrash",
                       "-n", crashReporterPath,
                       "--es", "minidumpPath", minidumpPath,
                       (char*)0);
    } else {
      unused << execlp("/system/bin/am",
                       "/system/bin/am",
                       "start",
                       "-a", "org.mozilla.gecko.reportCrash",
                       "-n", crashReporterPath,
                       "--es", "minidumpPath", minidumpPath,
                       (char*)0);
    }
#endif
    _exit(1);
  }
#endif // XP_MACOSX
#endif // XP_UNIX

  return returnValue;
}

#ifdef XP_WIN
static void
ReserveBreakpadVM()
{
  if (!gBreakpadReservedVM) {
    gBreakpadReservedVM = VirtualAlloc(nullptr, kReserveSize, MEM_RESERVE,
                                       PAGE_NOACCESS);
  }
}

static void
FreeBreakpadVM()
{
  if (gBreakpadReservedVM) {
    VirtualFree(gBreakpadReservedVM, 0, MEM_RELEASE);
  }
}

/**
 * Filters out floating point exceptions which are handled by nsSigHandlers.cpp
 * and should not be handled as crashes.
 *
 * Also calls FreeBreakpadVM if appropriate.
 */
static bool FPEFilter(void* context, EXCEPTION_POINTERS* exinfo,
                      MDRawAssertionInfo* assertion)
{
  if (!exinfo) {
    mozilla::IOInterposer::Disable();
    FreeBreakpadVM();
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
      return false; // Don't write minidump, continue exception search
  }
  mozilla::IOInterposer::Disable();
  FreeBreakpadVM();
  return true;
}
#endif // XP_WIN

static bool ShouldReport()
{
  // this environment variable prevents us from launching
  // the crash reporter client
  const char *envvar = PR_GetEnv("MOZ_CRASHREPORTER_NO_REPORT");
  if (envvar && *envvar) {
    return false;
  }

  envvar = PR_GetEnv("MOZ_CRASHREPORTER_FULLDUMP");
  if (envvar && *envvar) {
    return false;
  }

  return true;
}

namespace {
  bool Filter(void* context) {
    mozilla::IOInterposer::Disable();
    return true;
  }
}


nsresult SetExceptionHandler(nsIFile* aXREDirectory,
                             bool force/*=false*/)
{
  if (gExceptionHandler)
    return NS_ERROR_ALREADY_INITIALIZED;

#if !defined(DEBUG) || defined(MOZ_WIDGET_GONK)
  // In non-debug builds, enable the crash reporter by default, and allow
  // disabling it with the MOZ_CRASHREPORTER_DISABLE environment variable.
  // Also enable it by default in debug gonk builds as it is difficult to
  // set environment on startup.
  const char *envvar = PR_GetEnv("MOZ_CRASHREPORTER_DISABLE");
  if (envvar && *envvar && !force)
    return NS_OK;
#else
  // In debug builds, disable the crash reporter by default, and allow to
  // enable it with the MOZ_CRASHREPORTER environment variable.
  const char *envvar = PR_GetEnv("MOZ_CRASHREPORTER");
  if ((!envvar || !*envvar) && !force)
    return NS_OK;
#endif

#if defined(MOZ_WIDGET_GONK)
  doReport = false;
  headlessClient = true;
#elif defined(XP_WIN)
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Desktop) {
    doReport = ShouldReport();
  } else {
    doReport = false;
    headlessClient = true;
  }
#else
  // this environment variable prevents us from launching
  // the crash reporter client
  doReport = ShouldReport();
#endif

  // allocate our strings
  crashReporterAPIData = new nsCString();
  NS_ENSURE_TRUE(crashReporterAPIData, NS_ERROR_OUT_OF_MEMORY);

  NS_ASSERTION(!crashReporterAPILock, "Shouldn't have a lock yet");
  crashReporterAPILock = new Mutex("crashReporterAPILock");
  NS_ASSERTION(!notesFieldLock, "Shouldn't have a lock yet");
  notesFieldLock = new Mutex("notesFieldLock");

  crashReporterAPIData_Hash =
    new nsDataHashtable<nsCStringHashKey,nsCString>();
  NS_ENSURE_TRUE(crashReporterAPIData_Hash, NS_ERROR_OUT_OF_MEMORY);

  notesField = new nsCString();
  NS_ENSURE_TRUE(notesField, NS_ERROR_OUT_OF_MEMORY);

  if (!headlessClient) {
    // locate crashreporter executable
    nsCOMPtr<nsIFile> exePath;
    nsresult rv = aXREDirectory->Clone(getter_AddRefs(exePath));
    NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_MACOSX)
    exePath->Append(NS_LITERAL_STRING("crashreporter.app"));
    exePath->Append(NS_LITERAL_STRING("Contents"));
    exePath->Append(NS_LITERAL_STRING("MacOS"));
#endif

    exePath->AppendNative(NS_LITERAL_CSTRING(CRASH_REPORTER_FILENAME));

#ifdef XP_WIN32
    nsString crashReporterPath_temp;

    exePath->GetPath(crashReporterPath_temp);
    crashReporterPath = reinterpret_cast<wchar_t*>(ToNewUnicode(crashReporterPath_temp));
#elif !defined(__ANDROID__)
    nsCString crashReporterPath_temp;

    exePath->GetNativePath(crashReporterPath_temp);
    crashReporterPath = ToNewCString(crashReporterPath_temp);
#else
    // On Android, we launch using the application package name
    // instead of a filename, so use ANDROID_PACKAGE_NAME to do that here.
    nsCString package(ANDROID_PACKAGE_NAME "/org.mozilla.gecko.CrashReporter");
    crashReporterPath = ToNewCString(package);
#endif
  }

  // get temp path to use for minidump path
#if defined(XP_WIN32)
  nsString tempPath;

  // first figure out buffer size
  int pathLen = GetTempPath(0, nullptr);
  if (pathLen == 0)
    return NS_ERROR_FAILURE;

  tempPath.SetLength(pathLen);
  GetTempPath(pathLen, (LPWSTR)tempPath.BeginWriting());
#elif defined(XP_MACOSX)
  nsCString tempPath;
  FSRef fsRef;
  OSErr err = FSFindFolder(kUserDomain, kTemporaryFolderType,
                           kCreateFolder, &fsRef);
  if (err != noErr)
    return NS_ERROR_FAILURE;

  char path[PATH_MAX];
  OSStatus status = FSRefMakePath(&fsRef, (UInt8*)path, PATH_MAX);
  if (status != noErr)
    return NS_ERROR_FAILURE;

  tempPath = path;

#elif defined(__ANDROID__)
  // GeckoAppShell or Gonk's init.rc sets this in the environment
  const char *tempenv = PR_GetEnv("TMPDIR");
  if (!tempenv)
    return NS_ERROR_FAILURE;
  nsCString tempPath(tempenv);
#elif defined(XP_UNIX)
  // we assume it's always /tmp on unix systems
  nsCString tempPath = NS_LITERAL_CSTRING("/tmp/");
#else
#error "Implement this for your platform"
#endif

#ifdef XP_MACOSX
  // Initialize spawn attributes, since this calls malloc.
  if (posix_spawnattr_init(&spawnattr) != 0) {
    return NS_ERROR_FAILURE;
  }

  // Set spawn attributes.
  size_t attr_count = ArrayLength(pref_cpu_types);
  size_t attr_ocount = 0;
  if (posix_spawnattr_setbinpref_np(&spawnattr,
                                    attr_count,
                                    pref_cpu_types,
                                    &attr_ocount) != 0 ||
      attr_ocount != attr_count) {
    posix_spawnattr_destroy(&spawnattr);
    return NS_ERROR_FAILURE;
  }
#endif

#ifdef XP_WIN32
  ReserveBreakpadVM();

  MINIDUMP_TYPE minidump_type = MiniDumpNormal;

  // Try to determine what version of dbghelp.dll we're using.
  // MinidumpWithFullMemoryInfo is only available in 6.1.x or newer.

  DWORD version_size = GetFileVersionInfoSizeW(L"dbghelp.dll", nullptr);
  if (version_size > 0) {
    std::vector<BYTE> buffer(version_size);
    if (GetFileVersionInfoW(L"dbghelp.dll",
                            0,
                            version_size,
                            &buffer[0])) {
      UINT len;
      VS_FIXEDFILEINFO* file_info;
      VerQueryValue(&buffer[0], L"\\", (void**)&file_info, &len);
      WORD major = HIWORD(file_info->dwFileVersionMS),
           minor = LOWORD(file_info->dwFileVersionMS),
           revision = HIWORD(file_info->dwFileVersionLS);
      if (major > 6 || (major == 6 && minor > 1) ||
          (major == 6 && minor == 1 && revision >= 7600)) {
        minidump_type = MiniDumpWithFullMemoryInfo;
      }
    }
  }

  const char* e = PR_GetEnv("MOZ_CRASHREPORTER_FULLDUMP");
  if (e && *e) {
    minidump_type = MiniDumpWithFullMemory;
  }
#endif // XP_WIN32

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
  NotePreviousUnhandledExceptionFilter();
#endif

  gExceptionHandler = new google_breakpad::
    ExceptionHandler(
#ifdef XP_LINUX
                     descriptor,
#else
                     tempPath.get(),
#endif

#ifdef XP_WIN
                     FPEFilter,
#else
                     Filter,
#endif
                     MinidumpCallback,
                     nullptr,
#ifdef XP_WIN32
                     google_breakpad::ExceptionHandler::HANDLER_ALL,
                     minidump_type,
                     (const wchar_t*) nullptr,
                     nullptr);
#else
                     true
#ifdef XP_MACOSX
                       , nullptr
#endif
#ifdef XP_LINUX
                       , -1
#endif
                      );
#endif // XP_WIN32

  if (!gExceptionHandler)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef XP_WIN
  gExceptionHandler->set_handle_debug_exceptions(true);
  
  // protect the crash reporter from being unloaded
  gBlockUnhandledExceptionFilter = true;
  gKernel32Intercept.Init("kernel32.dll");
  bool ok = gKernel32Intercept.AddHook("SetUnhandledExceptionFilter",
          reinterpret_cast<intptr_t>(patched_SetUnhandledExceptionFilter),
          (void**) &stub_SetUnhandledExceptionFilter);

#ifdef DEBUG
  if (!ok)
    printf_stderr ("SetUnhandledExceptionFilter hook failed; crash reporter is vulnerable.\n");
#endif
#endif

  // store application start time
  char timeString[32];
  time_t startupTime = time(nullptr);
  XP_TTOA(startupTime, timeString, 10);
  AnnotateCrashReport(NS_LITERAL_CSTRING("StartupTime"),
                      nsDependentCString(timeString));

#if defined(XP_MACOSX)
  // On OS X, many testers like to see the OS crash reporting dialog
  // since it offers immediate stack traces.  We allow them to set
  // a default to pass exceptions to the OS handler.
  Boolean keyExistsAndHasValidFormat = false;
  Boolean prefValue = ::CFPreferencesGetAppBooleanValue(CFSTR("OSCrashReporter"),
                                                        kCFPreferencesCurrentApplication,
                                                        &keyExistsAndHasValidFormat);
  if (keyExistsAndHasValidFormat)
    showOSCrashReporter = prefValue;
#endif

#if defined(MOZ_WIDGET_ANDROID)
  for (unsigned int i = 0; i < library_mappings.size(); i++) {
    u_int8_t guid[sizeof(MDGUID)];
    google_breakpad::FileID::ElfFileIdentifierFromMappedFile((void const *)library_mappings[i].start_address, guid);
    gExceptionHandler->AddMappingInfo(library_mappings[i].name,
                                      guid,
                                      library_mappings[i].start_address,
                                      library_mappings[i].length,
                                      library_mappings[i].file_offset);
  }
#endif

  mozalloc_set_oom_abort_handler(AnnotateOOMAllocationSize);

  return NS_OK;
}

bool GetEnabled()
{
  return gExceptionHandler != nullptr;
}

bool GetMinidumpPath(nsAString& aPath)
{
  if (!gExceptionHandler)
    return false;

#ifndef XP_LINUX
  aPath = CONVERT_XP_CHAR_TO_UTF16(gExceptionHandler->dump_path().c_str());
#else
  aPath = CONVERT_XP_CHAR_TO_UTF16(
      gExceptionHandler->minidump_descriptor().directory().c_str());
#endif
  return true;
}

nsresult SetMinidumpPath(const nsAString& aPath)
{
  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

#ifdef XP_WIN32
  gExceptionHandler->set_dump_path(char16ptr_t(aPath.BeginReading()));
#elif defined(XP_LINUX)
  gExceptionHandler->set_minidump_descriptor(
      MinidumpDescriptor(NS_ConvertUTF16toUTF8(aPath).BeginReading()));
#else
  gExceptionHandler->set_dump_path(NS_ConvertUTF16toUTF8(aPath).BeginReading());
#endif
  return NS_OK;
}

static nsresult
WriteDataToFile(nsIFile* aFile, const nsACString& data)
{
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

static nsresult
GetFileContents(nsIFile* aFile, nsACString& data)
{
  PRFileDesc* fd;
  nsresult rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_OK;
  int32_t filesize = PR_Available(fd);
  if (filesize <= 0) {
    rv = NS_ERROR_FILE_NOT_FOUND;
  }
  else {
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
static nsresult
GetOrInit(nsIFile* aDir, const nsACString& filename,
          nsACString& aContents, InitDataFunc aInitFunc)
{
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
    }
    else {
      // didn't pass in an init func
      rv = NS_ERROR_FAILURE;
    }
  }
  else {
    // just get the file's contents
    rv = GetFileContents(dataFile, aContents);
  }

  return rv;
}

// Init the "install time" data.  We're taking an easy way out here
// and just setting this to "the time when this version was first run".
static nsresult
InitInstallTime(nsACString& aInstallTime)
{
  time_t t = time(nullptr);
  char buf[16];
  sprintf(buf, "%ld", t);
  aInstallTime = buf;

  return NS_OK;
}

// Ensure a directory exists and create it if missing.
static nsresult
EnsureDirectoryExists(nsIFile* dir)
{
  nsresult rv = dir->Create(nsIFile::DIRECTORY_TYPE, 0700);

  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)) {
    return rv;
  }

  return NS_OK;
}

// Annotate the crash report with a Unique User ID and time
// since install.  Also do some prep work for recording
// time since last crash, which must be calculated at
// crash time.
// If any piece of data doesn't exist, initialize it first.
nsresult SetupExtraData(nsIFile* aAppDataDirectory,
                        const nsACString& aBuildID)
{
  nsCOMPtr<nsIFile> dataDirectory;
  nsresult rv = aAppDataDirectory->Clone(getter_AddRefs(dataDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataDirectory->AppendNative(NS_LITERAL_CSTRING("Crash Reports"));
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureDirectoryExists(dataDirectory);

#if defined(XP_WIN32)
  nsAutoString dataDirEnv(NS_LITERAL_STRING("MOZ_CRASHREPORTER_DATA_DIRECTORY="));

  nsAutoString dataDirectoryPath;
  rv = dataDirectory->GetPath(dataDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  dataDirEnv.Append(dataDirectoryPath);

  _wputenv(dataDirEnv.get());
#else
  // Save this path in the environment for the crash reporter application.
  nsAutoCString dataDirEnv("MOZ_CRASHREPORTER_DATA_DIRECTORY=");

  nsAutoCString dataDirectoryPath;
  rv = dataDirectory->GetNativePath(dataDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  dataDirEnv.Append(dataDirectoryPath);

  char* env = ToNewCString(dataDirEnv);
  NS_ENSURE_TRUE(env, NS_ERROR_OUT_OF_MEMORY);

  PR_SetEnv(env);
#endif

  nsAutoCString data;
  if(NS_SUCCEEDED(GetOrInit(dataDirectory,
                            NS_LITERAL_CSTRING("InstallTime") + aBuildID,
                            data, InitInstallTime)))
    AnnotateCrashReport(NS_LITERAL_CSTRING("InstallTime"), data);

  // this is a little different, since we can't init it with anything,
  // since it's stored at crash time, and we can't annotate the
  // crash report with the stored value, since we really want
  // (now - LastCrash), so we just get a value if it exists,
  // and store it in a time_t value.
  if(NS_SUCCEEDED(GetOrInit(dataDirectory, NS_LITERAL_CSTRING("LastCrash"),
                            data, nullptr))) {
    lastCrashTime = (time_t)atol(data.get());
  }

  // not really the best place to init this, but I have the path I need here
  nsCOMPtr<nsIFile> lastCrashFile;
  rv = dataDirectory->Clone(getter_AddRefs(lastCrashFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = lastCrashFile->AppendNative(NS_LITERAL_CSTRING("LastCrash"));
  NS_ENSURE_SUCCESS(rv, rv);
  memset(lastCrashTimeFilename, 0, sizeof(lastCrashTimeFilename));

#if defined(XP_WIN32)
  nsAutoString filename;
  rv = lastCrashFile->GetPath(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.Length() < XP_PATH_MAX)
    wcsncpy(lastCrashTimeFilename, filename.get(), filename.Length());
#else
  nsAutoCString filename;
  rv = lastCrashFile->GetNativePath(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.Length() < XP_PATH_MAX)
    strncpy(lastCrashTimeFilename, filename.get(), filename.Length());
#endif

  if (headlessClient) {
    nsCOMPtr<nsIFile> markerFile;
    rv = dataDirectory->Clone(getter_AddRefs(markerFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = markerFile->AppendNative(NS_LITERAL_CSTRING("LastCrashFilename"));
    NS_ENSURE_SUCCESS(rv, rv);
    memset(crashMarkerFilename, 0, sizeof(crashMarkerFilename));

#if defined(XP_WIN32)
    nsAutoString markerFilename;
    rv = markerFile->GetPath(markerFilename);
    NS_ENSURE_SUCCESS(rv, rv);

    if (markerFilename.Length() < XP_PATH_MAX)
      wcsncpy(crashMarkerFilename, markerFilename.get(),
              markerFilename.Length());
#else
    nsAutoCString markerFilename;
    rv = markerFile->GetNativePath(markerFilename);
    NS_ENSURE_SUCCESS(rv, rv);

    if (markerFilename.Length() < XP_PATH_MAX)
      strncpy(crashMarkerFilename, markerFilename.get(),
              markerFilename.Length());
#endif
  }

  return NS_OK;
}

static void OOPDeinit();

nsresult UnsetExceptionHandler()
{
  if (isSafeToDump) {
    MutexAutoLock lock(*dumpSafetyLock);
    isSafeToDump = false;
  }

#ifdef XP_WIN
  // allow SetUnhandledExceptionFilter
  gBlockUnhandledExceptionFilter = false;
#endif

  delete gExceptionHandler;

  // do this here in the unlikely case that we succeeded in allocating
  // our strings but failed to allocate gExceptionHandler.
  delete crashReporterAPIData_Hash;
  crashReporterAPIData_Hash = nullptr;

  delete crashReporterAPILock;
  crashReporterAPILock = nullptr;

  delete notesFieldLock;
  notesFieldLock = nullptr;

  delete crashReporterAPIData;
  crashReporterAPIData = nullptr;

  delete notesField;
  notesField = nullptr;

  delete lastRunCrashID;
  lastRunCrashID = nullptr;

  if (pendingDirectory) {
    NS_Free(pendingDirectory);
    pendingDirectory = nullptr;
  }

  if (crashReporterPath) {
    NS_Free(crashReporterPath);
    crashReporterPath = nullptr;
  }

  if (eventsDirectory) {
    NS_Free(eventsDirectory);
    eventsDirectory = nullptr;
  }

#ifdef XP_MACOSX
  posix_spawnattr_destroy(&spawnattr);
#endif

  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  gExceptionHandler = nullptr;

  OOPDeinit();

  delete dumpSafetyLock;
  dumpSafetyLock = nullptr;

  return NS_OK;
}

static void ReplaceChar(nsCString& str, const nsACString& character,
                        const nsACString& replacement)
{
  nsCString::const_iterator start, end;

  str.BeginReading(start);
  str.EndReading(end);

  while (FindInReadable(character, start, end)) {
    int32_t pos = end.size_backward();
    str.Replace(pos - 1, 1, replacement);

    str.BeginReading(start);
    start.advance(pos + replacement.Length() - 1);
    str.EndReading(end);
  }
}

static bool DoFindInReadable(const nsACString& str, const nsACString& value)
{
  nsACString::const_iterator start, end;
  str.BeginReading(start);
  str.EndReading(end);

  return FindInReadable(value, start, end);
}

static PLDHashOperator EnumerateEntries(const nsACString& key,
                                        nsCString entry,
                                        void* userData)
{
  crashReporterAPIData->Append(key + NS_LITERAL_CSTRING("=") + entry +
                               NS_LITERAL_CSTRING("\n"));
  return PL_DHASH_NEXT;
}

// This function is miscompiled with MSVC 2005/2008 when PGO is on.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
static nsresult
EscapeAnnotation(const nsACString& key, const nsACString& data, nsCString& escapedData)
{
  if (DoFindInReadable(key, NS_LITERAL_CSTRING("=")) ||
      DoFindInReadable(key, NS_LITERAL_CSTRING("\n")))
    return NS_ERROR_INVALID_ARG;

  if (DoFindInReadable(data, NS_LITERAL_CSTRING("\0")))
    return NS_ERROR_INVALID_ARG;

  escapedData = data;

  // escape backslashes
  ReplaceChar(escapedData, NS_LITERAL_CSTRING("\\"),
              NS_LITERAL_CSTRING("\\\\"));
  // escape newlines
  ReplaceChar(escapedData, NS_LITERAL_CSTRING("\n"),
              NS_LITERAL_CSTRING("\\n"));
  return NS_OK;
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

class DelayedNote
{
 public:
  DelayedNote(const nsACString& aKey, const nsACString& aData)
  : mKey(aKey), mData(aData), mType(Annotation) {}

  DelayedNote(const nsACString& aData)
  : mData(aData), mType(AppNote) {}

  void Run()
  {
    if (mType == Annotation) {
      AnnotateCrashReport(mKey, mData);
    } else {
      AppendAppNotesToCrashReport(mData);
    }
  }
  
 private:
  nsCString mKey;
  nsCString mData;
  enum AnnotationType { Annotation, AppNote } mType;
};

static void
EnqueueDelayedNote(DelayedNote* aNote)
{
  if (!gDelayedAnnotations) {
    gDelayedAnnotations = new nsTArray<nsAutoPtr<DelayedNote> >();
  }
  gDelayedAnnotations->AppendElement(aNote);
}

nsresult AnnotateCrashReport(const nsACString& key, const nsACString& data)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  nsCString escapedData;
  nsresult rv = EscapeAnnotation(key, data, escapedData);
  if (NS_FAILED(rv))
    return rv;

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    if (!NS_IsMainThread()) {
      NS_ERROR("Cannot call AnnotateCrashReport in child processes from non-main thread.");
      return NS_ERROR_FAILURE;
    }
    PCrashReporterChild* reporter = CrashReporterChild::GetCrashReporter();
    if (!reporter) {
      EnqueueDelayedNote(new DelayedNote(key, data));
      return NS_OK;
    }
    if (!reporter->SendAnnotateCrashReport(nsCString(key), escapedData))
      return NS_ERROR_FAILURE;
    return NS_OK;
  }

  MutexAutoLock lock(*crashReporterAPILock);

  crashReporterAPIData_Hash->Put(key, escapedData);

  // now rebuild the file contents
  crashReporterAPIData->Truncate(0);
  crashReporterAPIData_Hash->EnumerateRead(EnumerateEntries,
                                           crashReporterAPIData);

  return NS_OK;
}

nsresult SetGarbageCollecting(bool collecting)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  isGarbageCollecting = collecting;

  return NS_OK;
}

void SetEventloopNestingLevel(uint32_t level)
{
  eventloopNestingLevel = level;
}

nsresult AppendAppNotesToCrashReport(const nsACString& data)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  if (DoFindInReadable(data, NS_LITERAL_CSTRING("\0")))
    return NS_ERROR_INVALID_ARG;

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    if (!NS_IsMainThread()) {
      NS_ERROR("Cannot call AnnotateCrashReport in child processes from non-main thread.");
      return NS_ERROR_FAILURE;
    }
    PCrashReporterChild* reporter = CrashReporterChild::GetCrashReporter();
    if (!reporter) {
      EnqueueDelayedNote(new DelayedNote(data));
      return NS_OK;
    }

    // Since we don't go through AnnotateCrashReport in the parent process,
    // we must ensure that the data is escaped and valid before the parent
    // sees it.
    nsCString escapedData;
    nsresult rv = EscapeAnnotation(NS_LITERAL_CSTRING("Notes"), data, escapedData);
    if (NS_FAILED(rv))
      return rv;

    if (!reporter->SendAppendAppNotes(escapedData))
      return NS_ERROR_FAILURE;
    return NS_OK;
  }

  MutexAutoLock lock(*notesFieldLock);

  notesField->Append(data);
  return AnnotateCrashReport(NS_LITERAL_CSTRING("Notes"), *notesField);
}

// Returns true if found, false if not found.
bool GetAnnotation(const nsACString& key, nsACString& data)
{
  if (!gExceptionHandler)
    return false;

  nsAutoCString entry;
  if (!crashReporterAPIData_Hash->Get(key, &entry))
    return false;

  data = entry;
  return true;
}

nsresult RegisterAppMemory(void* ptr, size_t length)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

#if defined(XP_LINUX) || defined(XP_WIN32)
  gExceptionHandler->RegisterAppMemory(ptr, length);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult UnregisterAppMemory(void* ptr)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

#if defined(XP_LINUX) || defined(XP_WIN32)
  gExceptionHandler->UnregisterAppMemory(ptr);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

bool GetServerURL(nsACString& aServerURL)
{
  if (!gExceptionHandler)
    return false;

  return GetAnnotation(NS_LITERAL_CSTRING("ServerURL"), aServerURL);
}

nsresult SetServerURL(const nsACString& aServerURL)
{
  // store server URL with the API data
  // the client knows to handle this specially
  return AnnotateCrashReport(NS_LITERAL_CSTRING("ServerURL"),
                             aServerURL);
}

nsresult
SetRestartArgs(int argc, char** argv)
{
  if (!gExceptionHandler)
    return NS_OK;

  int i;
  nsAutoCString envVar;
  char *env;
  char *argv0 = getenv("MOZ_APP_LAUNCHER");
  for (i = 0; i < argc; i++) {
    envVar = "MOZ_CRASHREPORTER_RESTART_ARG_";
    envVar.AppendInt(i);
    envVar += "=";
    if (argv0 && i == 0) {
      // Is there a request to suppress default binary launcher?
      envVar += argv0;
    } else {
      envVar += argv[i];
    }

    // PR_SetEnv() wants the string to be available for the lifetime
    // of the app, so dup it here
    env = ToNewCString(envVar);
    if (!env)
      return NS_ERROR_OUT_OF_MEMORY;

    PR_SetEnv(env);
  }

  // make sure the arg list is terminated
  envVar = "MOZ_CRASHREPORTER_RESTART_ARG_";
  envVar.AppendInt(i);
  envVar += "=";

  // PR_SetEnv() wants the string to be available for the lifetime
  // of the app, so dup it here
  env = ToNewCString(envVar);
  if (!env)
    return NS_ERROR_OUT_OF_MEMORY;

  PR_SetEnv(env);

  // make sure we save the info in XUL_APP_FILE for the reporter
  const char *appfile = PR_GetEnv("XUL_APP_FILE");
  if (appfile && *appfile) {
    envVar = "MOZ_CRASHREPORTER_RESTART_XUL_APP_FILE=";
    envVar += appfile;
    env = ToNewCString(envVar);
    PR_SetEnv(env);
  }

  return NS_OK;
}

#ifdef XP_WIN32
nsresult WriteMinidumpForException(EXCEPTION_POINTERS* aExceptionInfo)
{
  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  return gExceptionHandler->WriteMinidumpForException(aExceptionInfo) ? NS_OK : NS_ERROR_FAILURE;
}
#endif

#ifdef XP_LINUX
bool WriteMinidumpForSigInfo(int signo, siginfo_t* info, void* uc)
{
  return gExceptionHandler->HandleSignal(signo, info, uc);
}
#endif

#ifdef XP_MACOSX
nsresult AppendObjCExceptionInfoToAppNotes(void *inException)
{
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
static nsresult PrefSubmitReports(bool* aSubmitReports, bool writePref)
{
  nsresult rv;
#if defined(XP_WIN32)
  /*
   * NOTE! This needs to stay in sync with the preference checking code
   *       in toolkit/crashreporter/client/crashreporter_win.cpp
   */
  nsCOMPtr<nsIXULAppInfo> appinfo =
    do_GetService("@mozilla.org/xre/app-info;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString appVendor, appName;
  rv = appinfo->GetVendor(appVendor);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = appinfo->GetName(appName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWindowsRegKey> regKey
    (do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString regPath;

  regPath.AppendLiteral("Software\\");

  // We need to ensure the registry keys are created so we can properly
  // write values to it
  
  // Create appVendor key
  if(!appVendor.IsEmpty()) {
    regPath.Append(appVendor);
    regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                   NS_ConvertUTF8toUTF16(regPath),
                   nsIWindowsRegKey::ACCESS_SET_VALUE);
    regPath.Append('\\');
  }

  // Create appName key
  regPath.Append(appName);
  regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                 NS_ConvertUTF8toUTF16(regPath),
                 nsIWindowsRegKey::ACCESS_SET_VALUE);
  regPath.Append('\\');

  // Create Crash Reporter key
  regPath.AppendLiteral("Crash Reporter");
  regKey->Create(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                 NS_ConvertUTF8toUTF16(regPath),
                 nsIWindowsRegKey::ACCESS_SET_VALUE);

  // If we're saving the pref value, just write it to ROOT_KEY_CURRENT_USER
  // and we're done.
  if (writePref) {
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                      NS_ConvertUTF8toUTF16(regPath),
                      nsIWindowsRegKey::ACCESS_SET_VALUE);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t value = *aSubmitReports ? 1 : 0;
    rv = regKey->WriteIntValue(NS_LITERAL_STRING("SubmitCrashReport"), value);
    regKey->Close();
    return rv;
  }

  // We're reading the pref value, so we need to first look under
  // ROOT_KEY_LOCAL_MACHINE to see if it's set there, and then fall back to
  // ROOT_KEY_CURRENT_USER. If it's not set in either place, the pref defaults
  // to "true".
  uint32_t value;
  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                    NS_ConvertUTF8toUTF16(regPath),
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_SUCCEEDED(rv)) {
    rv = regKey->ReadIntValue(NS_LITERAL_STRING("SubmitCrashReport"), &value);
    regKey->Close();
    if (NS_SUCCEEDED(rv)) {
      *aSubmitReports = !!value;
      return NS_OK;
    }
  }

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                    NS_ConvertUTF8toUTF16(regPath),
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) {
    *aSubmitReports = true;
    return NS_OK;
  }
  
  rv = regKey->ReadIntValue(NS_LITERAL_STRING("SubmitCrashReport"), &value);
  // default to true on failure
  if (NS_FAILED(rv)) {
    value = 1;
    rv = NS_OK;
  }
  regKey->Close();

  *aSubmitReports = !!value;
  return NS_OK;
#elif defined(XP_MACOSX)
  rv = NS_OK;
  if (writePref) {
    CFPropertyListRef cfValue = (CFPropertyListRef)(*aSubmitReports ? kCFBooleanTrue : kCFBooleanFalse);
    ::CFPreferencesSetAppValue(CFSTR("submitReport"),
                               cfValue,
                               reporterClientAppID);
    if (!::CFPreferencesAppSynchronize(reporterClientAppID))
      rv = NS_ERROR_FAILURE;
  }
  else {
    *aSubmitReports = true;
    Boolean keyExistsAndHasValidFormat = false;
    Boolean prefValue = ::CFPreferencesGetAppBooleanValue(CFSTR("submitReport"),
                                                          reporterClientAppID,
                                                          &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat)
      *aSubmitReports = !!prefValue;
  }
  return rv;
#elif defined(XP_UNIX)
  /*
   * NOTE! This needs to stay in sync with the preference checking code
   *       in toolkit/crashreporter/client/crashreporter_linux.cpp
   */
  nsCOMPtr<nsIFile> reporterINI;
  rv = NS_GetSpecialDirectory("UAppData", getter_AddRefs(reporterINI));
  NS_ENSURE_SUCCESS(rv, rv);
  reporterINI->AppendNative(NS_LITERAL_CSTRING("Crash Reports"));
  reporterINI->AppendNative(NS_LITERAL_CSTRING("crashreporter.ini"));

  bool exists;
  rv = reporterINI->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    if (!writePref) {
        // If reading the pref, default to true if .ini doesn't exist.
        *aSubmitReports = true;
        return NS_OK;
    }
    // Create the file so the INI processor can write to it.
    rv = reporterINI->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIINIParserFactory> iniFactory =
    do_GetService("@mozilla.org/xpcom/ini-processor-factory;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIINIParser> iniParser;
  rv = iniFactory->CreateINIParser(reporterINI,
                                   getter_AddRefs(iniParser));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're writing the pref, just set and we're done.
  if (writePref) {
    nsCOMPtr<nsIINIParserWriter> iniWriter = do_QueryInterface(iniParser);
    NS_ENSURE_TRUE(iniWriter, NS_ERROR_FAILURE);

    rv = iniWriter->SetString(NS_LITERAL_CSTRING("Crash Reporter"),
                              NS_LITERAL_CSTRING("SubmitReport"),
                              *aSubmitReports ?  NS_LITERAL_CSTRING("1") :
                                                 NS_LITERAL_CSTRING("0"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = iniWriter->WriteFile(nullptr, 0);
    return rv;
  }
  
  nsAutoCString submitReportValue;
  rv = iniParser->GetString(NS_LITERAL_CSTRING("Crash Reporter"),
                            NS_LITERAL_CSTRING("SubmitReport"),
                            submitReportValue);

  // Default to "true" if the pref can't be found.
  if (NS_FAILED(rv))
    *aSubmitReports = true;
  else if (submitReportValue.EqualsASCII("0"))
    *aSubmitReports = false;
  else
    *aSubmitReports = true;

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult GetSubmitReports(bool* aSubmitReports)
{
    return PrefSubmitReports(aSubmitReports, false);
}

nsresult SetSubmitReports(bool aSubmitReports)
{
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

static void
SetCrashEventsDir(nsIFile* aDir)
{
  nsCOMPtr<nsIFile> eventsDir = aDir;

  const char *env = PR_GetEnv("CRASHES_EVENTS_DIR");
  if (env && *env) {
    NS_NewNativeLocalFile(nsDependentCString(env),
                          false, getter_AddRefs(eventsDir));
    EnsureDirectoryExists(eventsDir);
  }

  if (eventsDirectory) {
    NS_Free(eventsDirectory);
  }

#ifdef XP_WIN
  nsString path;
  eventsDir->GetPath(path);
  eventsDirectory = reinterpret_cast<wchar_t*>(ToNewUnicode(path));
#else
  nsCString path;
  eventsDir->GetNativePath(path);
  eventsDirectory = ToNewCString(path);
#endif
}

void
SetProfileDirectory(nsIFile* aDir)
{
  nsCOMPtr<nsIFile> dir;
  aDir->Clone(getter_AddRefs(dir));

  dir->Append(NS_LITERAL_STRING("crashes"));
  EnsureDirectoryExists(dir);
  dir->Append(NS_LITERAL_STRING("events"));
  EnsureDirectoryExists(dir);
  SetCrashEventsDir(dir);
}

void
SetUserAppDataDirectory(nsIFile* aDir)
{
  nsCOMPtr<nsIFile> dir;
  aDir->Clone(getter_AddRefs(dir));

  dir->Append(NS_LITERAL_STRING("Crash Reports"));
  EnsureDirectoryExists(dir);
  dir->Append(NS_LITERAL_STRING("events"));
  EnsureDirectoryExists(dir);
  SetCrashEventsDir(dir);
}

void
UpdateCrashEventsDir()
{
  const char *env = PR_GetEnv("CRASHES_EVENTS_DIR");
  if (env && *env) {
    SetCrashEventsDir(nullptr);
  }

  nsCOMPtr<nsIFile> eventsDir;
  nsresult rv = NS_GetSpecialDirectory("ProfD", getter_AddRefs(eventsDir));
  if (NS_SUCCEEDED(rv)) {
    SetProfileDirectory(eventsDir);
    return;
  }

  rv = NS_GetSpecialDirectory("UAppData", getter_AddRefs(eventsDir));
  if (NS_SUCCEEDED(rv)) {
    SetUserAppDataDirectory(eventsDir);
    return;
  }

  NS_WARNING("Couldn't get the user appdata directory. Crash events may not be produced.");
}

bool GetCrashEventsDir(nsAString& aPath)
{
  if (!eventsDirectory) {
    return false;
  }

  aPath = CONVERT_XP_CHAR_TO_UTF16(eventsDirectory);
  return true;
}

static void
FindPendingDir()
{
  if (pendingDirectory)
    return;

  nsCOMPtr<nsIFile> pendingDir;
  nsresult rv = NS_GetSpecialDirectory("UAppData", getter_AddRefs(pendingDir));
  if (NS_FAILED(rv)) {
    NS_WARNING("Couldn't get the user appdata directory, crash dumps will go in an unusual location");
  }
  else {
    pendingDir->Append(NS_LITERAL_STRING("Crash Reports"));
    pendingDir->Append(NS_LITERAL_STRING("pending"));

#ifdef XP_WIN
    nsString path;
    pendingDir->GetPath(path);
    pendingDirectory = reinterpret_cast<wchar_t*>(ToNewUnicode(path));
#else
    nsCString path;
    pendingDir->GetNativePath(path);
    pendingDirectory = ToNewCString(path);
#endif
  }
}

// The "pending" dir is Crash Reports/pending, from which minidumps
// can be submitted. Because this method may be called off the main thread,
// we store the pending directory as a path.
static bool
GetPendingDir(nsIFile** dir)
{
  MOZ_ASSERT(OOPInitialized());
  if (!pendingDirectory) {
    return false;
  }

  nsCOMPtr<nsIFile> pending = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (!pending) {
    NS_WARNING("Can't set up pending directory during shutdown.");
    return false;
  }
#ifdef XP_WIN
  pending->InitWithPath(nsDependentString(pendingDirectory));
#else
  pending->InitWithNativePath(nsDependentCString(pendingDirectory));
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
static bool
GetMinidumpLimboDir(nsIFile** dir)
{
  if (ShouldReport()) {
    return GetPendingDir(dir);
  }
  else {
#ifndef XP_LINUX
    CreateFileFromPath(gExceptionHandler->dump_path(), dir);
#else
    CreateFileFromPath(gExceptionHandler->minidump_descriptor().directory(),
                       dir);
#endif
    return nullptr != *dir;
  }
}

bool
GetMinidumpForID(const nsAString& id, nsIFile** minidump)
{
  if (!GetMinidumpLimboDir(minidump))
    return false;
  (*minidump)->Append(id + NS_LITERAL_STRING(".dmp")); 
  return true;
}

bool
GetIDFromMinidump(nsIFile* minidump, nsAString& id)
{
  if (NS_SUCCEEDED(minidump->GetLeafName(id))) {
    id.Replace(id.Length() - 4, 4, NS_LITERAL_STRING(""));
    return true;
  }
  return false;
}

bool
GetExtraFileForID(const nsAString& id, nsIFile** extraFile)
{
  if (!GetMinidumpLimboDir(extraFile))
    return false;
  (*extraFile)->Append(id + NS_LITERAL_STRING(".extra"));
  return true;
}

bool
GetExtraFileForMinidump(nsIFile* minidump, nsIFile** extraFile)
{
  nsAutoString leafName;
  nsresult rv = minidump->GetLeafName(leafName);
  if (NS_FAILED(rv))
    return false;

  nsCOMPtr<nsIFile> extraF;
  rv = minidump->Clone(getter_AddRefs(extraF));
  if (NS_FAILED(rv))
    return false;

  leafName.Replace(leafName.Length() - 3, 3,
                   NS_LITERAL_STRING("extra"));
  rv = extraF->SetLeafName(leafName);
  if (NS_FAILED(rv))
    return false;

  *extraFile = nullptr;
  extraF.swap(*extraFile);
  return true;
}

bool
AppendExtraData(const nsAString& id, const AnnotationTable& data)
{
  nsCOMPtr<nsIFile> extraFile;
  if (!GetExtraFileForID(id, getter_AddRefs(extraFile)))
    return false;
  return AppendExtraData(extraFile, data);
}

//-----------------------------------------------------------------------------
// Helpers for AppendExtraData()
//
struct Blacklist {
  Blacklist() : mItems(nullptr), mLen(0) { }
  Blacklist(const char** items, int len) : mItems(items), mLen(len) { }

  bool Contains(const nsACString& key) const {
    for (int i = 0; i < mLen; ++i)
      if (key.EqualsASCII(mItems[i]))
        return true;
    return false;
  }

  const char** mItems;
  const int mLen;
};

struct EnumerateAnnotationsContext {
  const Blacklist& blacklist;
  PRFileDesc* fd;
};

static void
WriteAnnotation(PRFileDesc* fd, const nsACString& key, const nsACString& value)
{
  PR_Write(fd, key.BeginReading(), key.Length());
  PR_Write(fd, "=", 1);
  PR_Write(fd, value.BeginReading(), value.Length());
  PR_Write(fd, "\n", 1);
}

static PLDHashOperator
EnumerateAnnotations(const nsACString& key,
                     nsCString entry,
                     void* userData)
{
  EnumerateAnnotationsContext* ctx =
    static_cast<EnumerateAnnotationsContext*>(userData);
  const Blacklist& blacklist = ctx->blacklist;

  // skip entries in the blacklist
  if (blacklist.Contains(key))
      return PL_DHASH_NEXT;

  WriteAnnotation(ctx->fd, key, entry);

  return PL_DHASH_NEXT;
}

static bool
WriteExtraData(nsIFile* extraFile,
               const AnnotationTable& data,
               const Blacklist& blacklist,
               bool writeCrashTime=false,
               bool truncate=false)
{
  PRFileDesc* fd;
  int truncOrAppend = truncate ? PR_TRUNCATE : PR_APPEND;
  nsresult rv = 
    extraFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | truncOrAppend,
                                0600, &fd);
  if (NS_FAILED(rv))
    return false;

  EnumerateAnnotationsContext ctx = { blacklist, fd };
  data.EnumerateRead(EnumerateAnnotations, &ctx);

  if (writeCrashTime) {
    time_t crashTime = time(nullptr);
    char crashTimeString[32];
    XP_TTOA(crashTime, crashTimeString, 10);

    WriteAnnotation(fd,
                    nsDependentCString("CrashTime"),
                    nsDependentCString(crashTimeString));
  }

  PR_Close(fd);
  return true;
}

bool
AppendExtraData(nsIFile* extraFile, const AnnotationTable& data)
{
  return WriteExtraData(extraFile, data, Blacklist());
}


static bool
WriteExtraForMinidump(nsIFile* minidump,
                      const Blacklist& blacklist,
                      nsIFile** extraFile)
{
  nsCOMPtr<nsIFile> extra;
  if (!GetExtraFileForMinidump(minidump, getter_AddRefs(extra)))
    return false;

  if (!WriteExtraData(extra, *crashReporterAPIData_Hash,
                      blacklist,
                      true /*write crash time*/,
                      true /*truncate*/))
    return false;

  *extraFile = nullptr;
  extra.swap(*extraFile);

  return true;
}

// It really only makes sense to call this function when
// ShouldReport() is true.
static bool
MoveToPending(nsIFile* dumpFile, nsIFile* extraFile)
{
  nsCOMPtr<nsIFile> pendingDir;
  if (!GetPendingDir(getter_AddRefs(pendingDir)))
    return false;

  if (NS_FAILED(dumpFile->MoveTo(pendingDir, EmptyString()))) {
    return false;
  }

  if (extraFile && NS_FAILED(extraFile->MoveTo(pendingDir, EmptyString()))) {
    return false;
  }

  return true;
}

static void
OnChildProcessDumpRequested(void* aContext,
#ifdef XP_MACOSX
                            const ClientInfo& aClientInfo,
                            const xpstring& aFilePath
#else
                            const ClientInfo* aClientInfo,
                            const xpstring* aFilePath
#endif
                            )
{
  nsCOMPtr<nsIFile> minidump;
  nsCOMPtr<nsIFile> extraFile;

  // Hold the mutex until the current dump request is complete, to
  // prevent UnsetExceptionHandler() from pulling the rug out from
  // under us.
  MutexAutoLock lock(*dumpSafetyLock);
  if (!isSafeToDump)
    return;

  CreateFileFromPath(
#ifdef XP_MACOSX
                     aFilePath,
#else
                     *aFilePath,
#endif
                     getter_AddRefs(minidump));

  if (!WriteExtraForMinidump(minidump,
                             Blacklist(kSubprocessBlacklist,
                                       ArrayLength(kSubprocessBlacklist)),
                             getter_AddRefs(extraFile)))
    return;

  if (ShouldReport())
    MoveToPending(minidump, extraFile);

  {
    uint32_t pid =
#ifdef XP_MACOSX
      aClientInfo.pid();
#else
      aClientInfo->pid();
#endif

#ifdef MOZ_CRASHREPORTER_INJECTOR
    bool runCallback;
#endif
    {
      MutexAutoLock lock(*dumpMapLock);
      ChildProcessData* pd = pidToMinidump->PutEntry(pid);
      MOZ_ASSERT(!pd->minidump);
      pd->minidump = minidump;
      pd->sequence = ++crashSequence;
#ifdef MOZ_CRASHREPORTER_INJECTOR
      runCallback = nullptr != pd->callback;
#endif
    }
#ifdef MOZ_CRASHREPORTER_INJECTOR
    if (runCallback)
      NS_DispatchToMainThread(new ReportInjectedCrash(pid));
#endif
  }
}

static bool
OOPInitialized()
{
  return pidToMinidump != nullptr;
}

#ifdef XP_MACOSX
static bool ChildFilter(void *context) {
  mozilla::IOInterposer::Disable();
  return true;
}
#endif

void
OOPInit()
{
  if (OOPInitialized())
    return;

  MOZ_ASSERT(NS_IsMainThread());

  NS_ABORT_IF_FALSE(gExceptionHandler != nullptr,
                    "attempt to initialize OOP crash reporter before in-process crashreporter!");

#if defined(XP_WIN)
  childCrashNotifyPipe =
    PR_smprintf("\\\\.\\pipe\\gecko-crash-server-pipe.%i",
                static_cast<int>(::GetCurrentProcessId()));

  const std::wstring dumpPath = gExceptionHandler->dump_path();
  crashServer = new CrashGenerationServer(
    NS_ConvertASCIItoUTF16(childCrashNotifyPipe).get(),
    nullptr,                    // default security attributes
    nullptr, nullptr,           // we don't care about process connect here
    OnChildProcessDumpRequested, nullptr,
    nullptr, nullptr,           // we don't care about process exit here
    nullptr, nullptr,           // we don't care about upload request here
    true,                       // automatically generate dumps
    &dumpPath);

#elif defined(XP_LINUX)
  if (!CrashGenerationServer::CreateReportChannel(&serverSocketFd,
                                                  &clientSocketFd))
    NS_RUNTIMEABORT("can't create crash reporter socketpair()");

  const std::string dumpPath =
      gExceptionHandler->minidump_descriptor().directory();
  crashServer = new CrashGenerationServer(
    serverSocketFd,
    OnChildProcessDumpRequested, nullptr,
    nullptr, nullptr,           // we don't care about process exit here
    true,
    &dumpPath);

#elif defined(XP_MACOSX)
  childCrashNotifyPipe =
    PR_smprintf("gecko-crash-server-pipe.%i",
                static_cast<int>(getpid()));
  const std::string dumpPath = gExceptionHandler->dump_path();

  crashServer = new CrashGenerationServer(
    childCrashNotifyPipe,
    ChildFilter,
    nullptr,
    OnChildProcessDumpRequested, nullptr,
    nullptr, nullptr,
    true, // automatically generate dumps
    dumpPath);
#endif

  if (!crashServer->Start())
    NS_RUNTIMEABORT("can't start crash reporter server()");

  pidToMinidump = new ChildMinidumpMap();

  dumpMapLock = new Mutex("CrashReporter::dumpMapLock");

  FindPendingDir();
  UpdateCrashEventsDir();
}

static void
OOPDeinit()
{
  if (!OOPInitialized()) {
    NS_WARNING("OOPDeinit() without successful OOPInit()");
    return;
  }

#ifdef MOZ_CRASHREPORTER_INJECTOR
  if (sInjectorThread) {
    sInjectorThread->Shutdown();
    NS_RELEASE(sInjectorThread);
  }
#endif

  delete crashServer;
  crashServer = nullptr;

  delete dumpMapLock;
  dumpMapLock = nullptr;

  delete pidToMinidump;
  pidToMinidump = nullptr;

#if defined(XP_WIN)
  PR_Free(childCrashNotifyPipe);
  childCrashNotifyPipe = nullptr;
#endif
}

#if defined(XP_WIN) || defined(XP_MACOSX)
// Parent-side API for children
const char*
GetChildNotificationPipe()
{
  if (!GetEnabled())
    return kNullNotifyPipe;

  MOZ_ASSERT(OOPInitialized());

  return childCrashNotifyPipe;
}
#endif

#ifdef MOZ_CRASHREPORTER_INJECTOR
void
InjectCrashReporterIntoProcess(DWORD processID, InjectorCrashCallback* cb)
{
  if (!GetEnabled())
    return;

  if (!OOPInitialized())
    OOPInit();

  if (!sInjectorThread) {
    if (NS_FAILED(NS_NewThread(&sInjectorThread)))
      return;
  }

  {
    MutexAutoLock lock(*dumpMapLock);
    ChildProcessData* pd = pidToMinidump->PutEntry(processID);
    MOZ_ASSERT(!pd->minidump && !pd->callback);
    pd->callback = cb;
  }

  nsCOMPtr<nsIRunnable> r = new InjectCrashRunnable(processID);
  sInjectorThread->Dispatch(r, nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
ReportInjectedCrash::Run()
{
  // Crash reporting may have been disabled after this method was dispatched
  if (!OOPInitialized())
    return NS_OK;

  InjectorCrashCallback* cb;
  {
    MutexAutoLock lock(*dumpMapLock);
    ChildProcessData* pd = pidToMinidump->GetEntry(mPID);
    if (!pd || !pd->callback)
      return NS_OK;

    MOZ_ASSERT(pd->minidump);

    cb = pd->callback;
  }

  cb->OnCrash(mPID);
  return NS_OK;
}

void
UnregisterInjectorCallback(DWORD processID)
{
  if (!OOPInitialized())
    return;

  MutexAutoLock lock(*dumpMapLock);
  pidToMinidump->RemoveEntry(processID);
}

#endif // MOZ_CRASHREPORTER_INJECTOR

bool
CheckForLastRunCrash()
{
  if (lastRunCrashID)
    return true;

  // The exception handler callback leaves the filename of the
  // last minidump in a known file.
  nsCOMPtr<nsIFile> lastCrashFile;
  CreateFileFromPath(crashMarkerFilename,
                     getter_AddRefs(lastCrashFile));

  bool exists;
  if (NS_FAILED(lastCrashFile->Exists(&exists)) || !exists) {
    return false;
  }

  nsAutoCString lastMinidump_contents;
  if (NS_FAILED(GetFileContents(lastCrashFile, lastMinidump_contents))) {
    return false;
  }
  lastCrashFile->Remove(false);

#ifdef XP_WIN
  // Ugly but effective.
  nsDependentString lastMinidump(
      reinterpret_cast<const char16_t*>(lastMinidump_contents.get()));
#else
  nsAutoCString lastMinidump = lastMinidump_contents;
#endif
  nsCOMPtr<nsIFile> lastMinidumpFile;
  CreateFileFromPath(lastMinidump.get(),
                     getter_AddRefs(lastMinidumpFile));

  if (!lastMinidumpFile || NS_FAILED(lastMinidumpFile->Exists(&exists)) || !exists) {
    return false;
  }

  nsCOMPtr<nsIFile> lastExtraFile;
  if (!GetExtraFileForMinidump(lastMinidumpFile,
                               getter_AddRefs(lastExtraFile))) {
    return false;
  }

  FindPendingDir();

  // Move {dump,extra} to pending folder
  if (!MoveToPending(lastMinidumpFile, lastExtraFile)) {
    return false;
  }

  lastRunCrashID = new nsString();
  return GetIDFromMinidump(lastMinidumpFile, *lastRunCrashID);
}

bool
GetLastRunCrashID(nsAString& id)
{
  if (!lastRunCrashID_checked) {
    CheckForLastRunCrash();
    lastRunCrashID_checked = true;
  }

  if (!lastRunCrashID) {
    return false;
  }

  id = *lastRunCrashID;
  return true;
}

#if defined(XP_WIN)
// Child-side API
bool
SetRemoteExceptionHandler(const nsACString& crashPipe)
{
  // crash reporting is disabled
  if (crashPipe.Equals(kNullNotifyPipe))
    return true;

  NS_ABORT_IF_FALSE(!gExceptionHandler, "crash client already init'd");

  gExceptionHandler = new google_breakpad::
    ExceptionHandler(L"",
                     FPEFilter,
                     nullptr,    // no minidump callback
                     nullptr,    // no callback context
                     google_breakpad::ExceptionHandler::HANDLER_ALL,
                     MiniDumpNormal,
                     NS_ConvertASCIItoUTF16(crashPipe).get(),
                     nullptr);
#ifdef XP_WIN
  gExceptionHandler->set_handle_debug_exceptions(true);
#endif

  // we either do remote or nothing, no fallback to regular crash reporting
  return gExceptionHandler->IsOutOfProcess();
}

//--------------------------------------------------
#elif defined(XP_LINUX)

// Parent-side API for children
bool
CreateNotificationPipeForChild(int* childCrashFd, int* childCrashRemapFd)
{
  if (!GetEnabled()) {
    *childCrashFd = -1;
    *childCrashRemapFd = -1;
    return true;
  }

  MOZ_ASSERT(OOPInitialized());

  *childCrashFd = clientSocketFd;
  *childCrashRemapFd = kMagicChildCrashReportFd;

  return true;
}

// Child-side API
bool
SetRemoteExceptionHandler()
{
  NS_ABORT_IF_FALSE(!gExceptionHandler, "crash client already init'd");

#ifndef XP_LINUX
  xpstring path = "";
#else
  // MinidumpDescriptor requires a non-empty path.
  google_breakpad::MinidumpDescriptor path(".");
#endif
  gExceptionHandler = new google_breakpad::
    ExceptionHandler(path,
                     nullptr,    // no filter callback
                     nullptr,    // no minidump callback
                     nullptr,    // no callback context
                     true,       // install signal handlers
                     kMagicChildCrashReportFd);

  if (gDelayedAnnotations) {
    for (uint32_t i = 0; i < gDelayedAnnotations->Length(); i++) {
      gDelayedAnnotations->ElementAt(i)->Run();
    }
    delete gDelayedAnnotations;
  }

  // we either do remote or nothing, no fallback to regular crash reporting
  return gExceptionHandler->IsOutOfProcess();
}

//--------------------------------------------------
#elif defined(XP_MACOSX)
// Child-side API
bool
SetRemoteExceptionHandler(const nsACString& crashPipe)
{
  // crash reporting is disabled
  if (crashPipe.Equals(kNullNotifyPipe))
    return true;

  NS_ABORT_IF_FALSE(!gExceptionHandler, "crash client already init'd");

  gExceptionHandler = new google_breakpad::
    ExceptionHandler("",
                     Filter,
                     nullptr,    // no minidump callback
                     nullptr,    // no callback context
                     true,       // install signal handlers
                     crashPipe.BeginReading());

  // we either do remote or nothing, no fallback to regular crash reporting
  return gExceptionHandler->IsOutOfProcess();
}
#endif  // XP_WIN


bool
TakeMinidumpForChild(uint32_t childPid, nsIFile** dump, uint32_t* aSequence)
{
  if (!GetEnabled())
    return false;

  MutexAutoLock lock(*dumpMapLock);

  ChildProcessData* pd = pidToMinidump->GetEntry(childPid);
  if (!pd)
    return false;

  NS_IF_ADDREF(*dump = pd->minidump);
  if (aSequence) {
    *aSequence = pd->sequence;
  }
  
  pidToMinidump->RemoveEntry(childPid);

  return !!*dump;
}

//-----------------------------------------------------------------------------
// CreatePairedMinidumps() and helpers
//

void
RenameAdditionalHangMinidump(nsIFile* minidump, nsIFile* childMinidump,
                           const nsACString& name)
{
  nsCOMPtr<nsIFile> directory;
  childMinidump->GetParent(getter_AddRefs(directory));
  if (!directory)
    return;

  nsAutoCString leafName;
  childMinidump->GetNativeLeafName(leafName);

  // turn "<id>.dmp" into "<id>-<name>.dmp
  leafName.Insert(NS_LITERAL_CSTRING("-") + name, leafName.Length() - 4);

  minidump->MoveToNative(directory, leafName);
}

static bool
PairedDumpCallback(
#ifdef XP_LINUX
                   const MinidumpDescriptor& descriptor,
#else
                   const XP_CHAR* dump_path,
                   const XP_CHAR* minidump_id,
#endif
                   void* context,
#ifdef XP_WIN32
                   EXCEPTION_POINTERS* /*unused*/,
                   MDRawAssertionInfo* /*unused*/,
#endif
                   bool succeeded)
{
  nsCOMPtr<nsIFile>& minidump = *static_cast< nsCOMPtr<nsIFile>* >(context);

  xpstring dump;
#ifdef XP_LINUX
  dump = descriptor.path();
#else
  dump = dump_path;
  dump += XP_PATH_SEPARATOR;
  dump += minidump_id;
  dump += dumpFileExtension;
#endif

  CreateFileFromPath(dump, getter_AddRefs(minidump));
  return true;
}

static bool
PairedDumpCallbackExtra(
#ifdef XP_LINUX
                        const MinidumpDescriptor& descriptor,
#else
                        const XP_CHAR* dump_path,
                        const XP_CHAR* minidump_id,
#endif
                        void* context,
#ifdef XP_WIN32
                        EXCEPTION_POINTERS* /*unused*/,
                        MDRawAssertionInfo* /*unused*/,
#endif
                        bool succeeded)
{
  PairedDumpCallback(
#ifdef XP_LINUX
                     descriptor,
#else
                     dump_path, minidump_id,
#endif
                     context,
#ifdef XP_WIN32
                     nullptr, nullptr,
#endif
                     succeeded);

  nsCOMPtr<nsIFile>& minidump = *static_cast< nsCOMPtr<nsIFile>* >(context);

  nsCOMPtr<nsIFile> extra;
  return WriteExtraForMinidump(minidump, Blacklist(), getter_AddRefs(extra));
}

ThreadId
CurrentThreadId()
{
#if defined(XP_WIN)
  return ::GetCurrentThreadId();
#elif defined(XP_LINUX)
  return sys_gettid();
#elif defined(XP_MACOSX)
  // Just return an index, since Mach ports can't be directly serialized
  thread_act_port_array_t   threads_for_task;
  mach_msg_type_number_t    thread_count;

  if (task_threads(mach_task_self(), &threads_for_task, &thread_count))
    return -1;

  for (unsigned int i = 0; i < thread_count; ++i) {
    if (threads_for_task[i] == mach_thread_self())
      return i;
  }
  abort();
#else
#  error "Unsupported platform"
#endif
}

#ifdef XP_MACOSX
static mach_port_t
GetChildThread(ProcessHandle childPid, ThreadId childBlamedThread)
{
  mach_port_t childThread = MACH_PORT_NULL;
  thread_act_port_array_t   threads_for_task;
  mach_msg_type_number_t    thread_count;

  if (task_threads(childPid, &threads_for_task, &thread_count)
      == KERN_SUCCESS && childBlamedThread < thread_count) {
    childThread = threads_for_task[childBlamedThread];
  }

  return childThread;
}
#endif

bool
CreatePairedMinidumps(ProcessHandle childPid,
                      ThreadId childBlamedThread,
                      nsIFile** childDump)
{
  if (!GetEnabled())
    return false;

#ifdef XP_MACOSX
  mach_port_t childThread = GetChildThread(childPid, childBlamedThread);
#else
  ThreadId childThread = childBlamedThread;
#endif

  xpstring dump_path;
#ifndef XP_LINUX
  dump_path = gExceptionHandler->dump_path();
#else
  dump_path = gExceptionHandler->minidump_descriptor().directory();
#endif

  // dump the child
  nsCOMPtr<nsIFile> childMinidump;
  if (!google_breakpad::ExceptionHandler::WriteMinidumpForChild(
         childPid,
         childThread,
         dump_path,
         PairedDumpCallbackExtra,
         static_cast<void*>(&childMinidump)))
    return false;

  nsCOMPtr<nsIFile> childExtra;
  GetExtraFileForMinidump(childMinidump, getter_AddRefs(childExtra));

  // dump the parent
  nsCOMPtr<nsIFile> parentMinidump;
  if (!google_breakpad::ExceptionHandler::WriteMinidump(
         dump_path,
#ifdef XP_MACOSX
         true,                  // write exception stream
#endif
         PairedDumpCallback,
         static_cast<void*>(&parentMinidump))) {

    childMinidump->Remove(false);
    childExtra->Remove(false);

    return false;
  }

  // success
  RenameAdditionalHangMinidump(parentMinidump, childMinidump,
                               NS_LITERAL_CSTRING("browser"));

  if (ShouldReport()) {
    MoveToPending(childMinidump, childExtra);
    MoveToPending(parentMinidump, nullptr);
  }

  childMinidump.forget(childDump);

  return true;
}

bool
CreateAdditionalChildMinidump(ProcessHandle childPid,
                              ThreadId childBlamedThread,
                              nsIFile* parentMinidump,
                              const nsACString& name)
{
  if (!GetEnabled())
    return false;

#ifdef XP_MACOSX
  mach_port_t childThread = GetChildThread(childPid, childBlamedThread);
#else
  ThreadId childThread = childBlamedThread;
#endif

  xpstring dump_path;
#ifndef XP_LINUX
  dump_path = gExceptionHandler->dump_path();
#else
  dump_path = gExceptionHandler->minidump_descriptor().directory();
#endif

  // dump the child
  nsCOMPtr<nsIFile> childMinidump;
  if (!google_breakpad::ExceptionHandler::WriteMinidumpForChild(
         childPid,
         childThread,
         dump_path,
         PairedDumpCallback,
         static_cast<void*>(&childMinidump))) {
    return false;
  }

  RenameAdditionalHangMinidump(childMinidump, parentMinidump, name);

  return true;
}

bool
UnsetRemoteExceptionHandler()
{
  delete gExceptionHandler;
  gExceptionHandler = nullptr;
  return true;
}

#if defined(MOZ_WIDGET_ANDROID)
void AddLibraryMapping(const char* library_name,
                       uintptr_t   start_address,
                       size_t      mapping_length,
                       size_t      file_offset)
{
  if (!gExceptionHandler) {
    mapping_info info;
    info.name = library_name;
    info.start_address = start_address;
    info.length = mapping_length;
    info.file_offset = file_offset;
    library_mappings.push_back(info);
  }
  else {
    u_int8_t guid[sizeof(MDGUID)];
    google_breakpad::FileID::ElfFileIdentifierFromMappedFile((void const *)start_address, guid);
    gExceptionHandler->AddMappingInfo(library_name,
                                      guid,
                                      start_address,
                                      mapping_length,
                                      file_offset);
  }
}
#endif

} // namespace CrashReporter
