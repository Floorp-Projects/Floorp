/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Breakpad integration
 *
 * The Initial Developer of the Original Code is
 * Ted Mielczarek <ted.mielczarek@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Josh Aas <josh@mozilla.com>
 *  Justin Dolske <dolske@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsExceptionHandler.h"

#if defined(XP_WIN32)
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "nsIWindowsRegKey.h"
#include "client/windows/crash_generation/crash_generation_server.h"
#include "client/windows/handler/exception_handler.h"
#include <DbgHelp.h>
#include <string.h>
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
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIINIParser.h"
#include "common/linux/linux_libc_support.h"
#include "common/linux/linux_syscall_support.h"
#include "client/linux/crash_generation/client_info.h"
#include "client/linux/crash_generation/crash_generation_server.h"
#include "client/linux/handler/exception_handler.h"
#include "client/linux/minidump_writer/linux_dumper.h"
#include "client/linux/minidump_writer/minidump_writer.h"
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

#include <stdlib.h>
#include <time.h>
#include <prenv.h>
#include <prio.h>
#include <prmem.h>
#include "mozilla/Mutex.h"
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsInterfaceHashtable.h"
#include "prprf.h"
#include "nsIXULAppInfo.h"
#include <map>
#include <vector>

#if defined(XP_MACOSX)
CFStringRef reporterClientAppID = CFSTR("org.mozilla.crashreporter");
#endif

#include "nsIUUIDGenerator.h"

using google_breakpad::CrashGenerationServer;
using google_breakpad::ClientInfo;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace CrashReporter {

#ifdef XP_WIN32
typedef wchar_t XP_CHAR;
typedef std::wstring xpstring;
#define CONVERT_UTF16_TO_XP_CHAR(x) x
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
#else
typedef char XP_CHAR;
typedef std::string xpstring;
#define CONVERT_UTF16_TO_XP_CHAR(x) NS_ConvertUTF16toUTF8(x)
#define CONVERT_XP_CHAR_TO_UTF16(x) NS_ConvertUTF8toUTF16(x)
#define CRASH_REPORTER_FILENAME "crashreporter"
#define PATH_SEPARATOR "/"
#define XP_PATH_SEPARATOR "/"
#define XP_PATH_MAX PATH_MAX
#ifdef XP_LINUX
#define XP_STRLEN(x) my_strlen(x)
#define XP_TTOA(time, buffer, base) my_timetostring(time, buffer, sizeof(buffer))
#else
#define XP_STRLEN(x) strlen(x)
#define XP_TTOA(time, buffer, base) sprintf(buffer, "%ld", time)
#define my_strlen strlen
#define sys_close close
#define sys_fork fork
#define sys_open open
#define sys_write write
#endif
#endif // XP_WIN32

static const XP_CHAR dumpFileExtension[] = {'.', 'd', 'm', 'p',
                                            '\0'}; // .dmp
static const XP_CHAR extraFileExtension[] = {'.', 'e', 'x', 't',
                                             'r', 'a', '\0'}; // .extra

static google_breakpad::ExceptionHandler* gExceptionHandler = nsnull;

static XP_CHAR* crashReporterPath;

// if this is false, we don't launch the crash reporter
static bool doReport = true;

// if this is true, we pass the exception on to the OS crash reporter
static bool showOSCrashReporter = false;

// The time of the last recorded crash, as a time_t value.
static time_t lastCrashTime = 0;
// The pathname of a file to store the crash time in
static XP_CHAR lastCrashTimeFilename[XP_PATH_MAX] = {0};

// these are just here for readability
static const char kCrashTimeParameter[] = "CrashTime=";
static const int kCrashTimeParameterLen = sizeof(kCrashTimeParameter)-1;

static const char kTimeSinceLastCrashParameter[] = "SecondsSinceLastCrash=";
static const int kTimeSinceLastCrashParameterLen =
                                     sizeof(kTimeSinceLastCrashParameter)-1;

static const char kSysMemoryParameter[] = "SystemMemoryUsePercentage=";
static const int kSysMemoryParameterLen = sizeof(kSysMemoryParameter)-1;

static const char kTotalVirtualMemoryParameter[] = "TotalVirtualMemory=";
static const int kTotalVirtualMemoryParameterLen =
  sizeof(kTotalVirtualMemoryParameter)-1;

static const char kAvailableVirtualMemoryParameter[] = "AvailableVirtualMemory=";
static const int kAvailableVirtualMemoryParameterLen =
  sizeof(kAvailableVirtualMemoryParameter)-1;

// this holds additional data sent via the API
static AnnotationTable* crashReporterAPIData_Hash;
static nsCString* crashReporterAPIData = nsnull;
static nsCString* notesField = nsnull;

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
typedef nsInterfaceHashtable<nsUint32HashKey, nsILocalFile> ChildMinidumpMap;
static ChildMinidumpMap* pidToMinidump;

// Crashreporter annotations that we don't send along in subprocess
// reports
static const char* kSubprocessBlacklist[] = {
  "FramePoisonBase",
  "FramePoisonSize",
  "StartupTime",
  "URL"
};


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

#if defined(__ANDROID__)
// Android builds use a custom library loader,
// so the embedding will provide a list of shared
// libraries that are mapped into anonymous mappings.
typedef struct {
  std::string name;
  std::string debug_id;
  uintptr_t   start_address;
  size_t      length;
  size_t      file_offset;
} mapping_info;
static std::vector<mapping_info> library_mappings;
typedef std::map<PRUint32,google_breakpad::MappingList> MappingMap;
static MappingMap child_library_mappings;

void FileIDToGUID(const char* file_id, u_int8_t guid[sizeof(MDGUID)])
{
  for (int i = 0; i < sizeof(MDGUID); i++) {
    int c;
    sscanf(file_id, "%02X", &c);
    guid[i] = (u_int8_t)(c & 0xFF);
    file_id += 2;
  }
  // GUIDs are stored in network byte order.
  uint32_t* data1 = reinterpret_cast<uint32_t*>(guid);
  *data1 = htonl(*data1);
  uint16_t* data2 = reinterpret_cast<uint16_t*>(guid + 4);
  *data2 = htons(*data2);
  uint16_t* data3 = reinterpret_cast<uint16_t*>(guid + 6);
  *data3 = htons(*data3);
}
#endif

#ifdef XP_LINUX
inline void
my_timetostring(time_t t, char* buffer, size_t buffer_length)
{
  my_memset(buffer, 0, buffer_length);
  my_itos(buffer, t, my_int_len(t));
}
#endif

#ifdef XP_WIN
static void
CreateFileFromPath(const xpstring& path, nsILocalFile** file)
{
  NS_NewLocalFile(nsDependentString(path.c_str()), PR_FALSE, file);
}
#else
static void
CreateFileFromPath(const xpstring& path, nsILocalFile** file)
{
  NS_NewNativeLocalFile(nsDependentCString(path.c_str()), PR_FALSE, file);
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

bool MinidumpCallback(const XP_CHAR* dump_path,
                      const XP_CHAR* minidump_id,
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
  XP_CHAR* p = Concat(minidumpPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
  Concat(p, dumpFileExtension, &size);

  static XP_CHAR extraDataPath[XP_PATH_MAX];
  size = XP_PATH_MAX;
  p = Concat(extraDataPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
  Concat(p, extraFileExtension, &size);

  // calculate time since last crash (if possible), and store
  // the time of this crash.
  time_t crashTime;
#ifdef XP_LINUX
  struct kernel_timeval tv;
  sys_gettimeofday(&tv, NULL);
  crashTime = tv.tv_sec;
#else
  crashTime = time(NULL);
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
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if(hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, crashTimeString, crashTimeStringLen, &nBytes, NULL);
      CloseHandle(hFile);
    }
#elif defined(XP_UNIX)
    int fd = sys_open(lastCrashTimeFilename,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0600);
    if (fd != -1) {
      ssize_t ignored = sys_write(fd, crashTimeString, crashTimeStringLen);
      (void)ignored;
      sys_close(fd);
    }
#endif
  }

#if defined(XP_WIN32)
  XP_CHAR cmdLine[CMDLINE_SIZE];
  size = CMDLINE_SIZE;
  p = Concat(cmdLine, L"\"", &size);
  p = Concat(p, crashReporterPath, &size);
  p = Concat(p, L"\" \"", &size);
  p = Concat(p, minidumpPath, &size);
  Concat(p, L"\"", &size);

  if (!crashReporterAPIData->IsEmpty()) {
    // write out API data
    HANDLE hFile = CreateFile(extraDataPath, GENERIC_WRITE, 0,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if(hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, crashReporterAPIData->get(),
                crashReporterAPIData->Length(), &nBytes, NULL);
      WriteFile(hFile, kCrashTimeParameter, kCrashTimeParameterLen,
                &nBytes, NULL);
      WriteFile(hFile, crashTimeString, crashTimeStringLen, &nBytes, NULL);
      WriteFile(hFile, "\n", 1, &nBytes, NULL);
      if (timeSinceLastCrash != 0) {
        WriteFile(hFile, kTimeSinceLastCrashParameter,
                  kTimeSinceLastCrashParameterLen, &nBytes, NULL);
        WriteFile(hFile, timeSinceLastCrashString, timeSinceLastCrashStringLen,
                  &nBytes, NULL);
        WriteFile(hFile, "\n", 1, &nBytes, NULL);
      }
      // Try to get some information about memory.
      MEMORYSTATUSEX statex;
      statex.dwLength = sizeof(statex);
      if (GlobalMemoryStatusEx(&statex)) {
        char buffer[128];
        int bufferLen;
        WriteFile(hFile, kSysMemoryParameter,
                  kSysMemoryParameterLen, &nBytes, NULL);
        ltoa(statex.dwMemoryLoad, buffer, 10);
        bufferLen = strlen(buffer);
        WriteFile(hFile, buffer, bufferLen,
                  &nBytes, NULL);
        WriteFile(hFile, "\n", 1, &nBytes, NULL);
        WriteFile(hFile, kTotalVirtualMemoryParameter,
                  kTotalVirtualMemoryParameterLen, &nBytes, NULL);
        _ui64toa(statex.ullTotalVirtual, buffer, 10);
        bufferLen = strlen(buffer);
        WriteFile(hFile, buffer, bufferLen,
                  &nBytes, NULL);
        WriteFile(hFile, "\n", 1, &nBytes, NULL);
        WriteFile(hFile, kAvailableVirtualMemoryParameter,
                  kAvailableVirtualMemoryParameterLen, &nBytes, NULL);
        _ui64toa(statex.ullAvailVirtual, buffer, 10);
        bufferLen = strlen(buffer);
        WriteFile(hFile, buffer, bufferLen,
                  &nBytes, NULL);
        WriteFile(hFile, "\n", 1, &nBytes, NULL);
      }
      CloseHandle(hFile);
    }
  }

  if (!doReport) {
    return returnValue;
  }

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWNORMAL;
  ZeroMemory(&pi, sizeof(pi));

  if (CreateProcess(NULL, (LPWSTR)cmdLine, NULL, NULL, FALSE, 0,
                    NULL, NULL, &si, &pi)) {
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
      ssize_t ignored = sys_write(fd, crashReporterAPIData->get(),
                                  crashReporterAPIData->Length());
      ignored = sys_write(fd, kCrashTimeParameter, kCrashTimeParameterLen);
      ignored = sys_write(fd, crashTimeString, crashTimeStringLen);
      ignored = sys_write(fd, "\n", 1);
      if (timeSinceLastCrash != 0) {
        ignored = sys_write(fd, kTimeSinceLastCrashParameter,
                        kTimeSinceLastCrashParameterLen);
        ignored = sys_write(fd, timeSinceLastCrashString,
                        timeSinceLastCrashStringLen);
        ignored = sys_write(fd, "\n", 1);
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
    NULL
  };

  char **env = NULL;
  char ***nsEnv = _NSGetEnviron();
  if (nsEnv)
    env = *nsEnv;
  int result = posix_spawnp(NULL,
                            my_argv[0],
                            NULL,
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
#if !defined(__ANDROID__)
    // need to clobber this, as libcurl might load NSS,
    // and we want it to load the system NSS.
    unsetenv("LD_LIBRARY_PATH");
    (void) execl(crashReporterPath,
                 crashReporterPath, minidumpPath, (char*)0);
#else
    // Invoke the reportCrash activity using am
    (void) execlp("/system/bin/am",
                 "/system/bin/am",
                 "start",
                 "-a", "org.mozilla.gecko.reportCrash",
                 "-n", crashReporterPath,
                 "--es", "minidumpPath", minidumpPath,
                 (char*)0);
#endif
    _exit(1);
  }
#endif // XP_MACOSX
#endif // XP_UNIX

 return returnValue;
}

#ifdef XP_WIN
/**
 * Filters out floating point exceptions which are handled by nsSigHandlers.cpp
 * and should not be handled as crashes.
 */
static bool FPEFilter(void* context, EXCEPTION_POINTERS* exinfo,
                      MDRawAssertionInfo* assertion)
{
  if (!exinfo)
    return true;

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
  return true;
}
#endif // XP_WIN

static bool ShouldReport()
{
  // this environment variable prevents us from launching
  // the crash reporter client
  const char *envvar = PR_GetEnv("MOZ_CRASHREPORTER_NO_REPORT");
  return !(envvar && *envvar);
}

nsresult SetExceptionHandler(nsILocalFile* aXREDirectory,
                             bool force/*=false*/)
{
  nsresult rv;

  if (gExceptionHandler)
    return NS_ERROR_ALREADY_INITIALIZED;

  const char *envvar = PR_GetEnv("MOZ_CRASHREPORTER_DISABLE");
  if (envvar && *envvar && !force)
    return NS_OK;

  // this environment variable prevents us from launching
  // the crash reporter client
  doReport = ShouldReport();

  // allocate our strings
  crashReporterAPIData = new nsCString();
  NS_ENSURE_TRUE(crashReporterAPIData, NS_ERROR_OUT_OF_MEMORY);

  crashReporterAPIData_Hash =
    new nsDataHashtable<nsCStringHashKey,nsCString>();
  NS_ENSURE_TRUE(crashReporterAPIData_Hash, NS_ERROR_OUT_OF_MEMORY);

  rv = crashReporterAPIData_Hash->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  notesField = new nsCString();
  NS_ENSURE_TRUE(notesField, NS_ERROR_OUT_OF_MEMORY);

  // locate crashreporter executable
  nsCOMPtr<nsIFile> exePath;
  rv = aXREDirectory->Clone(getter_AddRefs(exePath));
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

  crashReporterPath = ToNewUnicode(crashReporterPath_temp);
#elif !defined(__ANDROID__)
  nsCString crashReporterPath_temp;
  exePath->GetNativePath(crashReporterPath_temp);

  crashReporterPath = ToNewCString(crashReporterPath_temp);
#else
  // On Android, we launch using the application package name
  // instead of a filename, so use ANDROID_PACKAGE_NAME to do that here.
  //TODO: don't hardcode org.mozilla here, so other vendors can
  // ship XUL apps with different package names on Android?
  nsCString package(ANDROID_PACKAGE_NAME "/.CrashReporter");
  crashReporterPath = ToNewCString(package);
#endif

  // get temp path to use for minidump path
#if defined(XP_WIN32)
  nsString tempPath;

  // first figure out buffer size
  int pathLen = GetTempPath(0, NULL);
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
  // GeckoAppShell sets this in the environment
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
  size_t attr_count = NS_ARRAY_LENGTH(pref_cpu_types);
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
  MINIDUMP_TYPE minidump_type = MiniDumpNormal;

#if MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
  // Try to determine what version of dbghelp.dll we're using.
  // MinidumpWithFullMemoryInfo is only available in 6.1.x or newer.

  DWORD version_size = GetFileVersionInfoSizeW(L"dbghelp.dll", NULL);
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
#endif // MOZ_WINSDK_TARGETVER >= MOZ_NTDDI_LONGHORN
#endif // XP_WIN32

  // now set the exception handler
  gExceptionHandler = new google_breakpad::
    ExceptionHandler(tempPath.get(),
#ifdef XP_WIN
                     FPEFilter,
#else
                     nsnull,
#endif
                     MinidumpCallback,
                     nsnull,
#if defined(XP_WIN32)
                     google_breakpad::ExceptionHandler::HANDLER_ALL,
                     minidump_type,
                     NULL,
                     NULL);
#else
                     true
#if defined(XP_MACOSX)
                       , NULL
#endif
                      );
#endif // XP_WIN32

  if (!gExceptionHandler)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef XP_WIN
  gExceptionHandler->set_handle_debug_exceptions(true);
#endif

  // store application start time
  char timeString[32];
  time_t startupTime = time(NULL);
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

#if defined(__ANDROID__)
  for (unsigned int i = 0; i < library_mappings.size(); i++) {
    u_int8_t guid[sizeof(MDGUID)];
    FileIDToGUID(library_mappings[i].debug_id.c_str(), guid);
    gExceptionHandler->AddMappingInfo(library_mappings[i].name,
                                      guid,
                                      library_mappings[i].start_address,
                                      library_mappings[i].length,
                                      library_mappings[i].file_offset);
  }
#endif

  return NS_OK;
}

bool GetEnabled()
{
  return gExceptionHandler != nsnull && !gExceptionHandler->IsOutOfProcess();
}

bool GetMinidumpPath(nsAString& aPath)
{
  if (!gExceptionHandler)
    return false;

  aPath = CONVERT_XP_CHAR_TO_UTF16(gExceptionHandler->dump_path().c_str());
  return true;
}

nsresult SetMinidumpPath(const nsAString& aPath)
{
  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  gExceptionHandler->set_dump_path(CONVERT_UTF16_TO_XP_CHAR(aPath).BeginReading());

  return NS_OK;
}

static nsresult
WriteDataToFile(nsIFile* aFile, const nsACString& data)
{
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(aFile);
  NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);

  PRFileDesc* fd;
  nsresult rv = localFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, 00600,
                                            &fd);
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
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(aFile);
  NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);

  PRFileDesc* fd;
  nsresult rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_OK;
  PRInt32 filesize = PR_Available(fd);
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
  PRBool exists;

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
  time_t t = time(NULL);
  char buf[16];
  sprintf(buf, "%ld", t);
  aInstallTime = buf;

  return NS_OK;
}

// Annotate the crash report with a Unique User ID and time
// since install.  Also do some prep work for recording
// time since last crash, which must be calculated at
// crash time.
// If any piece of data doesn't exist, initialize it first.
nsresult SetupExtraData(nsILocalFile* aAppDataDirectory,
                        const nsACString& aBuildID)
{
  nsCOMPtr<nsIFile> dataDirectory;
  nsresult rv = aAppDataDirectory->Clone(getter_AddRefs(dataDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataDirectory->AppendNative(NS_LITERAL_CSTRING("Crash Reports"));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = dataDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    rv = dataDirectory->Create(nsIFile::DIRECTORY_TYPE, 0700);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#if defined(XP_WIN32)
  nsAutoString dataDirEnv(NS_LITERAL_STRING("MOZ_CRASHREPORTER_DATA_DIRECTORY="));

  nsAutoString dataDirectoryPath;
  rv = dataDirectory->GetPath(dataDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  dataDirEnv.Append(dataDirectoryPath);

  _wputenv(dataDirEnv.get());
#else
  // Save this path in the environment for the crash reporter application.
  nsCAutoString dataDirEnv("MOZ_CRASHREPORTER_DATA_DIRECTORY=");

  nsCAutoString dataDirectoryPath;
  rv = dataDirectory->GetNativePath(dataDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  dataDirEnv.Append(dataDirectoryPath);

  char* env = ToNewCString(dataDirEnv);
  NS_ENSURE_TRUE(env, NS_ERROR_OUT_OF_MEMORY);

  PR_SetEnv(env);
#endif

  nsCAutoString data;
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
                            data, NULL))) {
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
  nsCAutoString filename;
  rv = lastCrashFile->GetNativePath(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.Length() < XP_PATH_MAX)
    strncpy(lastCrashTimeFilename, filename.get(), filename.Length());
#endif

  return NS_OK;
}

static void OOPDeinit();

nsresult UnsetExceptionHandler()
{
  delete gExceptionHandler;

  // do this here in the unlikely case that we succeeded in allocating
  // our strings but failed to allocate gExceptionHandler.
  if (crashReporterAPIData_Hash) {
    delete crashReporterAPIData_Hash;
    crashReporterAPIData_Hash = nsnull;
  }

  if (crashReporterAPIData) {
    delete crashReporterAPIData;
    crashReporterAPIData = nsnull;
  }

  if (notesField) {
    delete notesField;
    notesField = nsnull;
  }

  if (crashReporterPath) {
    NS_Free(crashReporterPath);
    crashReporterPath = nsnull;
  }

#ifdef XP_MACOSX
  posix_spawnattr_destroy(&spawnattr);
#endif

  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  gExceptionHandler = nsnull;

  OOPDeinit();

  return NS_OK;
}

static void ReplaceChar(nsCString& str, const nsACString& character,
                        const nsACString& replacement)
{
  nsCString::const_iterator start, end;

  str.BeginReading(start);
  str.EndReading(end);

  while (FindInReadable(character, start, end)) {
    PRInt32 pos = end.size_backward();
    str.Replace(pos - 1, 1, replacement);

    str.BeginReading(start);
    start.advance(pos + replacement.Length() - 1);
    str.EndReading(end);
  }
}

static PRBool DoFindInReadable(const nsACString& str, const nsACString& value)
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

nsresult AnnotateCrashReport(const nsACString& key, const nsACString& data)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  if (DoFindInReadable(key, NS_LITERAL_CSTRING("=")) ||
      DoFindInReadable(key, NS_LITERAL_CSTRING("\n")))
    return NS_ERROR_INVALID_ARG;

  if (DoFindInReadable(data, NS_LITERAL_CSTRING("\0")))
    return NS_ERROR_INVALID_ARG;

  nsCString escapedData(data);

  // escape backslashes
  ReplaceChar(escapedData, NS_LITERAL_CSTRING("\\"),
              NS_LITERAL_CSTRING("\\\\"));
  // escape newlines
  ReplaceChar(escapedData, NS_LITERAL_CSTRING("\n"),
              NS_LITERAL_CSTRING("\\n"));

  nsresult rv = crashReporterAPIData_Hash->Put(key, escapedData);
  NS_ENSURE_SUCCESS(rv, rv);

  // now rebuild the file contents
  crashReporterAPIData->Truncate(0);
  crashReporterAPIData_Hash->EnumerateRead(EnumerateEntries,
                                           crashReporterAPIData);

  return NS_OK;
}

nsresult AppendAppNotesToCrashReport(const nsACString& data)
{
  if (!GetEnabled())
    return NS_ERROR_NOT_INITIALIZED;

  if (DoFindInReadable(data, NS_LITERAL_CSTRING("\0")))
    return NS_ERROR_INVALID_ARG;

  notesField->Append(data);
  return AnnotateCrashReport(NS_LITERAL_CSTRING("Notes"), *notesField);
}

// Returns true if found, false if not found.
bool GetAnnotation(const nsACString& key, nsACString& data)
{
  if (!gExceptionHandler)
    return false;

  nsCAutoString entry;
  if (!crashReporterAPIData_Hash->Get(key, &entry))
    return false;

  data = entry;
  return true;
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
  nsCAutoString envVar;
  char *env;
  for (i = 0; i < argc; i++) {
    envVar = "MOZ_CRASHREPORTER_RESTART_ARG_";
    envVar.AppendInt(i);
    envVar += "=";
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    // we'd like to run the script around the binary
    // instead of the binary itself, so remove the -bin
    // if it exists on the first argument
    int arg_len = 0;
    if (i == 0 &&
        (arg_len = strlen(argv[i])) > 4 &&
        strcmp(argv[i] + arg_len - 4, "-bin") == 0) {
      envVar.Append(argv[i], arg_len - 4);
    } else
#endif
    {
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

#ifdef XP_MACOSX
nsresult AppendObjCExceptionInfoToAppNotes(void *inException)
{
  nsCAutoString excString;
  GetObjCExceptionInfo(inException, excString);
  AppendAppNotesToCrashReport(excString);
  return NS_OK;
}
#endif

/*
 * Combined code to get/set the crash reporter submission pref on
 * different platforms.
 */
static nsresult PrefSubmitReports(PRBool* aSubmitReports, bool writePref)
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

  nsCAutoString appVendor, appName;
  rv = appinfo->GetVendor(appVendor);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = appinfo->GetName(appName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWindowsRegKey> regKey
    (do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString regPath;

  regPath.AppendLiteral("Software\\");
  if(!appVendor.IsEmpty()) {
    regPath.Append(appVendor);
    regPath.AppendLiteral("\\");
  }
  regPath.Append(appName);
  regPath.AppendLiteral("\\Crash Reporter");

  // If we're saving the pref value, just write it to ROOT_KEY_CURRENT_USER
  // and we're done.
  if (writePref) {
    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                      NS_ConvertUTF8toUTF16(regPath),
                      nsIWindowsRegKey::ACCESS_SET_VALUE);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 value = *aSubmitReports ? 1 : 0;
    rv = regKey->WriteIntValue(NS_LITERAL_STRING("SubmitCrashReport"), value);
    regKey->Close();
    return rv;
  }

  // We're reading the pref value, so we need to first look under
  // ROOT_KEY_LOCAL_MACHINE to see if it's set there, and then fall back to
  // ROOT_KEY_CURRENT_USER. If it's not set in either place, the pref defaults
  // to "true".
  PRUint32 value;
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
    *aSubmitReports = PR_TRUE;
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
    *aSubmitReports = PR_TRUE;
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

  PRBool exists;
  rv = reporterINI->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    if (!writePref) {
        // If reading the pref, default to true if .ini doesn't exist.
        *aSubmitReports = PR_TRUE;
        return NS_OK;
    }
    // Create the file so the INI processor can write to it.
    rv = reporterINI->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIINIParserFactory> iniFactory =
    do_GetService("@mozilla.org/xpcom/ini-processor-factory;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(reporterINI);
  NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);
  nsCOMPtr<nsIINIParser> iniParser;
  rv = iniFactory->CreateINIParser(localFile,
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
    rv = iniWriter->WriteFile(NULL);
    return rv;
  }
  
  nsCAutoString submitReportValue;
  rv = iniParser->GetString(NS_LITERAL_CSTRING("Crash Reporter"),
                            NS_LITERAL_CSTRING("SubmitReport"),
                            submitReportValue);

  // Default to "true" if the pref can't be found.
  if (NS_FAILED(rv))
    *aSubmitReports = PR_TRUE;
  else if (submitReportValue.EqualsASCII("0"))
    *aSubmitReports = PR_FALSE;
  else
    *aSubmitReports = PR_TRUE;

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult GetSubmitReports(PRBool* aSubmitReports)
{
    return PrefSubmitReports(aSubmitReports, false);
}

nsresult SetSubmitReports(PRBool aSubmitReports)
{
    return PrefSubmitReports(&aSubmitReports, true);
}

// The "pending" dir is Crash Reports/pending, from which minidumps
// can be submitted
static bool
GetPendingDir(nsILocalFile** dir)
{
  nsCOMPtr<nsIProperties> dirSvc =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return false;
  nsCOMPtr<nsILocalFile> pendingDir;
  if (NS_FAILED(dirSvc->Get("UAppData",
                            NS_GET_IID(nsILocalFile),
                            getter_AddRefs(pendingDir))) ||
      NS_FAILED(pendingDir->Append(NS_LITERAL_STRING("Crash Reports"))) ||
      NS_FAILED(pendingDir->Append(NS_LITERAL_STRING("pending"))))
    return false;
  *dir = NULL;
  pendingDir.swap(*dir);
  return true;
}

// The "limbo" dir is where minidumps go to wait for something else to
// use them.  If we're |ShouldReport()|, then the "something else" is
// a minidump submitter, and they're coming from the 
// Crash Reports/pending/ dir.  Otherwise, we don't know what the
// "somthing else" is, but the minidumps stay in [profile]/minidumps/
// limbo.
static bool
GetMinidumpLimboDir(nsILocalFile** dir)
{
  if (ShouldReport()) {
    return GetPendingDir(dir);
  }
  else {
    CreateFileFromPath(gExceptionHandler->dump_path(), dir);
    return NULL != *dir;
  }
}

bool
GetMinidumpForID(const nsAString& id, nsILocalFile** minidump)
{
  if (!GetMinidumpLimboDir(minidump))
    return false;
  (*minidump)->Append(id + NS_LITERAL_STRING(".dmp")); 
  return true;
}

bool
GetIDFromMinidump(nsILocalFile* minidump, nsAString& id)
{
  if (NS_SUCCEEDED(minidump->GetLeafName(id))) {
    id.Replace(id.Length() - 4, 4, NS_LITERAL_STRING(""));
    return true;
  }
  return false;
}

bool
GetExtraFileForID(const nsAString& id, nsILocalFile** extraFile)
{
  if (!GetMinidumpLimboDir(extraFile))
    return false;
  (*extraFile)->Append(id + NS_LITERAL_STRING(".extra"));
  return true;
}

bool
GetExtraFileForMinidump(nsILocalFile* minidump, nsILocalFile** extraFile)
{
  nsAutoString leafName;
  nsresult rv = minidump->GetLeafName(leafName);
  if (NS_FAILED(rv))
    return false;

  nsCOMPtr<nsIFile> extraF;
  rv = minidump->Clone(getter_AddRefs(extraF));
  if (NS_FAILED(rv))
    return false;

  nsCOMPtr<nsILocalFile> extra = do_QueryInterface(extraF);
  if (!extra)
    return false;

  leafName.Replace(leafName.Length() - 3, 3,
                   NS_LITERAL_STRING("extra"));
  rv = extra->SetLeafName(leafName);
  if (NS_FAILED(rv))
    return false;

  *extraFile = NULL;
  extra.swap(*extraFile);
  return true;
}

bool
AppendExtraData(const nsAString& id, const AnnotationTable& data)
{
  nsCOMPtr<nsILocalFile> extraFile;
  if (!GetExtraFileForID(id, getter_AddRefs(extraFile)))
    return false;
  return AppendExtraData(extraFile, data);
}

//-----------------------------------------------------------------------------
// Helpers for AppendExtraData()
//
struct Blacklist {
  Blacklist() : mItems(NULL), mLen(0) { }
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
WriteExtraData(nsILocalFile* extraFile,
               const AnnotationTable& data,
               const Blacklist& blacklist,
               bool writeCrashTime=false,
               bool truncate=false)
{
  PRFileDesc* fd;
  PRIntn truncOrAppend = truncate ? PR_TRUNCATE : PR_APPEND;
  nsresult rv = 
    extraFile->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | truncOrAppend,
                                0600, &fd);
  if (NS_FAILED(rv))
    return false;

  EnumerateAnnotationsContext ctx = { blacklist, fd };
  data.EnumerateRead(EnumerateAnnotations, &ctx);

  if (writeCrashTime) {
    time_t crashTime = time(NULL);
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
AppendExtraData(nsILocalFile* extraFile, const AnnotationTable& data)
{
  return WriteExtraData(extraFile, data, Blacklist());
}


static bool
WriteExtraForMinidump(nsILocalFile* minidump,
                      const Blacklist& blacklist,
                      nsILocalFile** extraFile)
{
  nsCOMPtr<nsILocalFile> extra;
  if (!GetExtraFileForMinidump(minidump, getter_AddRefs(extra)))
    return false;

  if (!WriteExtraData(extra, *crashReporterAPIData_Hash,
                      blacklist,
                      true /*write crash time*/,
                      true /*truncate*/))
    return false;

  *extraFile = NULL;
  extra.swap(*extraFile);

  return true;
}

// It really only makes sense to call this function when
// ShouldReport() is true.
static bool
MoveToPending(nsIFile* dumpFile, nsIFile* extraFile)
{
  nsCOMPtr<nsILocalFile> pendingDir;
  if (!GetPendingDir(getter_AddRefs(pendingDir)))
    return false;

  return NS_SUCCEEDED(dumpFile->MoveTo(pendingDir, EmptyString())) &&
    NS_SUCCEEDED(extraFile->MoveTo(pendingDir, EmptyString()));
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
  nsCOMPtr<nsILocalFile> minidump;
  nsCOMPtr<nsILocalFile> extraFile;

  CreateFileFromPath(
#ifdef XP_MACOSX
                     aFilePath,
#else
                     *aFilePath,
#endif
                     getter_AddRefs(minidump));

#if defined(__ANDROID__)
  // Do dump generation here since the CrashGenerationServer doesn't
  // have access to the library mappings.
  MappingMap::const_iterator iter = 
    child_library_mappings.find(aClientInfo->pid_);
  if (iter == child_library_mappings.end()) {
    NS_WARNING("No library mappings found for child, can't write minidump!");
    return;
  }

  if (!google_breakpad::WriteMinidump(aFilePath->c_str(),
                                      aClientInfo->pid_,
                                      aClientInfo->crash_context,
                                      aClientInfo->crash_context_size,
                                      iter->second))
    return;
#endif

  if (!WriteExtraForMinidump(minidump,
                             Blacklist(kSubprocessBlacklist,
                                       NS_ARRAY_LENGTH(kSubprocessBlacklist)),
                             getter_AddRefs(extraFile)))
    return;

  if (ShouldReport())
    MoveToPending(minidump, extraFile);

  {
    PRUint32 pid =
#ifdef XP_MACOSX
      aClientInfo.pid();
#else
      aClientInfo->pid();
#endif

    MutexAutoLock lock(*dumpMapLock);
    pidToMinidump->Put(pid, minidump);
  }
}

static bool
OOPInitialized()
{
  return pidToMinidump != NULL;
}

static void
OOPInit()
{
  NS_ABORT_IF_FALSE(!OOPInitialized(),
                    "OOP crash reporter initialized more than once!");
  NS_ABORT_IF_FALSE(gExceptionHandler != NULL,
                    "attempt to initialize OOP crash reporter before in-process crashreporter!");

#if defined(XP_WIN)
  childCrashNotifyPipe =
    PR_smprintf("\\\\.\\pipe\\gecko-crash-server-pipe.%i",
                static_cast<int>(::GetCurrentProcessId()));

  const std::wstring dumpPath = gExceptionHandler->dump_path();
  crashServer = new CrashGenerationServer(
    NS_ConvertASCIItoUTF16(childCrashNotifyPipe).get(),
    NULL,                       // default security attributes
    NULL, NULL,                 // we don't care about process connect here
    OnChildProcessDumpRequested, NULL,
    NULL, NULL,                 // we don't care about process exit here
    true,                       // automatically generate dumps
    &dumpPath);

#elif defined(XP_LINUX)
  if (!CrashGenerationServer::CreateReportChannel(&serverSocketFd,
                                                  &clientSocketFd))
    NS_RUNTIMEABORT("can't create crash reporter socketpair()");

  const std::string dumpPath = gExceptionHandler->dump_path();
  bool generateDumps = true;
#if defined(__ANDROID__)
  // On Android, the callback will do dump generation, since it needs
  // to pass the library mappings.
  generateDumps = false;
#endif
  crashServer = new CrashGenerationServer(
    serverSocketFd,
    OnChildProcessDumpRequested, NULL,
    NULL, NULL,                 // we don't care about process exit here
    generateDumps,
    &dumpPath);

#elif defined(XP_MACOSX)
  childCrashNotifyPipe =
    PR_smprintf("gecko-crash-server-pipe.%i",
                static_cast<int>(getpid()));
  const std::string dumpPath = gExceptionHandler->dump_path();

  crashServer = new CrashGenerationServer(
    childCrashNotifyPipe,
    OnChildProcessDumpRequested, NULL,
    NULL, NULL,
    true, // automatically generate dumps
    dumpPath);
#endif

  if (!crashServer->Start())
    NS_RUNTIMEABORT("can't start crash reporter server()");

  pidToMinidump = new ChildMinidumpMap();
  pidToMinidump->Init();

  dumpMapLock = new Mutex("CrashReporter::dumpMapLock");
}

static void
OOPDeinit()
{
  if (!OOPInitialized()) {
    NS_WARNING("OOPDeinit() without successful OOPInit()");
    return;
  }

  delete crashServer;
  crashServer = NULL;

  delete dumpMapLock;
  dumpMapLock = NULL;

  delete pidToMinidump;
  pidToMinidump = NULL;

#if defined(XP_WIN)
  PR_Free(childCrashNotifyPipe);
  childCrashNotifyPipe = NULL;
#endif
}

#if defined(XP_WIN) || defined(XP_MACOSX)
// Parent-side API for children
const char*
GetChildNotificationPipe()
{
  if (!GetEnabled())
    return kNullNotifyPipe;

  if (!OOPInitialized())
    OOPInit();

  return childCrashNotifyPipe;
}
#endif

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
                     NULL,    // no filter callback
                     NULL,    // no minidump callback
                     NULL,    // no callback context
                     google_breakpad::ExceptionHandler::HANDLER_ALL,
                     MiniDumpNormal,
                     NS_ConvertASCIItoUTF16(crashPipe).BeginReading(),
                     NULL);
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

  if (!OOPInitialized())
    OOPInit();

  *childCrashFd = clientSocketFd;
  *childCrashRemapFd = kMagicChildCrashReportFd;

  return true;
}

// Child-side API
bool
SetRemoteExceptionHandler()
{
  NS_ABORT_IF_FALSE(!gExceptionHandler, "crash client already init'd");

  gExceptionHandler = new google_breakpad::
    ExceptionHandler("",
                     NULL,    // no filter callback
                     NULL,    // no minidump callback
                     NULL,    // no callback context
                     true,    // install signal handlers
                     kMagicChildCrashReportFd);

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
                     NULL,    // no filter callback
                     NULL,    // no minidump callback
                     NULL,    // no callback context
                     true,    // install signal handlers
                     crashPipe.BeginReading());

  // we either do remote or nothing, no fallback to regular crash reporting
  return gExceptionHandler->IsOutOfProcess();
}
#endif  // XP_WIN


bool
TakeMinidumpForChild(PRUint32 childPid, nsILocalFile** dump)
{
  if (!GetEnabled())
    return false;

  MutexAutoLock lock(*dumpMapLock);

  nsCOMPtr<nsILocalFile> d;
  bool found = pidToMinidump->Get(childPid, getter_AddRefs(d));
  if (found)
    pidToMinidump->Remove(childPid);

  *dump = NULL;
  d.swap(*dump);

  return found;
}

//-----------------------------------------------------------------------------
// CreatePairedMinidumps() and helpers
//
struct PairedDumpContext {
  nsCOMPtr<nsILocalFile>* minidump;
  nsCOMPtr<nsILocalFile>* extra;
  const Blacklist& blacklist;
};

static bool
PairedDumpCallback(const XP_CHAR* dump_path,
                   const XP_CHAR* minidump_id,
                   void* context,
#ifdef XP_WIN32
                   EXCEPTION_POINTERS* /*unused*/,
                   MDRawAssertionInfo* /*unused*/,
#endif
                   bool succeeded)
{
  PairedDumpContext* ctx = static_cast<PairedDumpContext*>(context);
  nsCOMPtr<nsILocalFile>& minidump = *ctx->minidump;
  nsCOMPtr<nsILocalFile>& extra = *ctx->extra;
  const Blacklist& blacklist = ctx->blacklist;

  xpstring dump(dump_path);
  dump += XP_PATH_SEPARATOR;
  dump += minidump_id;
  dump += dumpFileExtension;

  CreateFileFromPath(dump, getter_AddRefs(minidump));
  return WriteExtraForMinidump(minidump, blacklist, getter_AddRefs(extra));
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

bool
CreatePairedMinidumps(ProcessHandle childPid,
                      ThreadId childBlamedThread,
                      nsAString* pairGUID,
                      nsILocalFile** childDump,
                      nsILocalFile** parentDump)
{
  if (!GetEnabled())
    return false;

  // create the UUID for the hang dump as a pair
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, false);  

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, false);
  
  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);
  CopyASCIItoUTF16(chars, *pairGUID);

  // trim off braces
  pairGUID->Cut(0, 1);
  pairGUID->Cut(pairGUID->Length()-1, 1);

#ifdef XP_MACOSX
  mach_port_t childThread = MACH_PORT_NULL;
  thread_act_port_array_t   threads_for_task;
  mach_msg_type_number_t    thread_count;

  if (task_threads(childPid, &threads_for_task, &thread_count)
      == KERN_SUCCESS && childBlamedThread < thread_count) {
    childThread = threads_for_task[childBlamedThread];
  }
#else
  ThreadId childThread = childBlamedThread;
#endif

  // dump the child
  nsCOMPtr<nsILocalFile> childMinidump;
  nsCOMPtr<nsILocalFile> childExtra;
  Blacklist childBlacklist(kSubprocessBlacklist,
                           NS_ARRAY_LENGTH(kSubprocessBlacklist));
  PairedDumpContext childCtx =
    { &childMinidump, &childExtra, childBlacklist };
  if (!google_breakpad::ExceptionHandler::WriteMinidumpForChild(
         childPid,
         childThread,
         gExceptionHandler->dump_path(),
         PairedDumpCallback,
         &childCtx))
    return false;

  // dump the parent
  nsCOMPtr<nsILocalFile> parentMinidump;
  nsCOMPtr<nsILocalFile> parentExtra;
  // nothing's blacklisted for this process
  Blacklist parentBlacklist;
  PairedDumpContext parentCtx =
    { &parentMinidump, &parentExtra, parentBlacklist };
  if (!google_breakpad::ExceptionHandler::WriteMinidump(
         gExceptionHandler->dump_path(),
         true,                  // write exception stream
         PairedDumpCallback,
         &parentCtx))
    return false;

  // success
  if (ShouldReport()) {
    MoveToPending(childMinidump, childExtra);
    MoveToPending(parentMinidump, parentExtra);
  }

  *childDump = NULL;
  *parentDump = NULL;
  childMinidump.swap(*childDump);
  parentMinidump.swap(*parentDump);

  return true;
}

bool
UnsetRemoteExceptionHandler()
{
  delete gExceptionHandler;
  gExceptionHandler = NULL;
  return true;
}

#if defined(__ANDROID__)
void AddLibraryMapping(const char* library_name,
                       const char* file_id,
                       uintptr_t   start_address,
                       size_t      mapping_length,
                       size_t      file_offset)
{
  if (!gExceptionHandler) {
    mapping_info info;
    info.name = library_name;
    info.debug_id = file_id;
    info.start_address = start_address;
    info.length = mapping_length;
    info.file_offset = file_offset;
    library_mappings.push_back(info);
  }
  else {
    u_int8_t guid[sizeof(MDGUID)];
    FileIDToGUID(file_id, guid);
    gExceptionHandler->AddMappingInfo(library_name,
                                      guid,
                                      start_address,
                                      mapping_length,
                                      file_offset);
  }
}

void AddLibraryMappingForChild(PRUint32    childPid,
                               const char* library_name,
                               const char* file_id,
                               uintptr_t   start_address,
                               size_t      mapping_length,
                               size_t      file_offset)
{
  if (child_library_mappings.find(childPid) == child_library_mappings.end())
    child_library_mappings[childPid] = google_breakpad::MappingList();
  google_breakpad::MappingInfo info;
  info.start_addr = start_address;
  info.size = mapping_length;
  info.offset = file_offset;
  strcpy(info.name, library_name);
 
  std::pair<google_breakpad::MappingInfo, u_int8_t[sizeof(MDGUID)]> mapping;
  mapping.first = info;
  u_int8_t guid[sizeof(MDGUID)];
  FileIDToGUID(file_id, guid);
  memcpy(mapping.second, guid, sizeof(MDGUID));
  child_library_mappings[childPid].push_back(mapping);
}

void RemoveLibraryMappingsForChild(PRUint32 childPid)
{
  MappingMap::iterator iter = child_library_mappings.find(childPid);
  if (iter != child_library_mappings.end())
    child_library_mappings.erase(iter);
}
#endif

} // namespace CrashReporter
