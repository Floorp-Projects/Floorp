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

#include "client/windows/handler/exception_handler.h"
#include <string.h>
#elif defined(XP_MACOSX)
#include "client/mac/handler/exception_handler.h"
#include <string>
#include <Carbon/Carbon.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "mac_utils.h"
#elif defined(XP_LINUX)
#include "client/linux/handler/exception_handler.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#else
#error "Not yet implemented for this platform"
#endif // defined(XP_WIN32)

#ifndef HAVE_CPP_2BYTE_WCHAR_T
#error "This code expects a 2 byte wchar_t.  You should --disable-crashreporter."
#endif

#include <stdlib.h>
#include <time.h>
#include <prenv.h>
#include <prio.h>
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsDataHashtable.h"

namespace CrashReporter {

#ifdef XP_WIN32
typedef wchar_t XP_CHAR;
#define CONVERT_UTF16_TO_XP_CHAR(x) x
#define XP_STRLEN(x) wcslen(x)
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
#define CONVERT_UTF16_TO_XP_CHAR(x) NS_ConvertUTF16toUTF8(x)
#define XP_STRLEN(x) strlen(x)
#define CRASH_REPORTER_FILENAME "crashreporter"
#define PATH_SEPARATOR "/"
#define XP_PATH_SEPARATOR "/"
#define XP_PATH_MAX PATH_MAX
#define XP_TTOA(time, buffer, base) sprintf(buffer, "%ld", time)
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

// this holds additional data sent via the API
static nsDataHashtable<nsCStringHashKey,nsCString>* crashReporterAPIData_Hash;
static nsCString* crashReporterAPIData = nsnull;

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

  XP_CHAR minidumpPath[XP_PATH_MAX];
  int size = XP_PATH_MAX;
  XP_CHAR* p = Concat(minidumpPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
  Concat(p, dumpFileExtension, &size);

  XP_CHAR extraDataPath[XP_PATH_MAX];
  size = XP_PATH_MAX;
  p = Concat(extraDataPath, dump_path, &size);
  p = Concat(p, XP_PATH_SEPARATOR, &size);
  p = Concat(p, minidump_id, &size);
  Concat(p, extraFileExtension, &size);

  // calculate time since last crash (if possible), and store
  // the time of this crash.
  time_t crashTime = time(NULL);
  time_t timeSinceLastCrash = 0;
  // stringified versions of the above
  char crashTimeString[32];
  int crashTimeStringLen = 0;
  char timeSinceLastCrashString[32];
  int timeSinceLastCrashStringLen = 0;

  XP_TTOA(crashTime, crashTimeString, 10);
  crashTimeStringLen = strlen(crashTimeString);
  if (lastCrashTime != 0) {
    timeSinceLastCrash = crashTime - lastCrashTime;
    XP_TTOA(timeSinceLastCrash, timeSinceLastCrashString, 10);
    timeSinceLastCrashStringLen = strlen(timeSinceLastCrashString);
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
    int fd = open(lastCrashTimeFilename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0600);
    if (fd != -1) {
      write(fd, crashTimeString, crashTimeStringLen);
      close(fd);
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
    int fd = open(extraDataPath,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0666);

    if (fd != -1) {
      // not much we can do in case of error
      write(fd, crashReporterAPIData->get(), crashReporterAPIData->Length());
      write(fd, kCrashTimeParameter, kCrashTimeParameterLen);
      write(fd, crashTimeString, crashTimeStringLen);
      write(fd, "\n", 1);
      if (timeSinceLastCrash != 0) {
        write(fd, kTimeSinceLastCrashParameter,kTimeSinceLastCrashParameterLen);
        write(fd, timeSinceLastCrashString, timeSinceLastCrashStringLen);
        write(fd, "\n", 1);
      }
      close (fd);
    }
  }

  if (!doReport) {
    return returnValue;
  }

  pid_t pid = fork();

  if (pid == -1)
    return false;
  else if (pid == 0) {
    (void) execl(crashReporterPath,
                 crashReporterPath, minidumpPath, (char*)0);
    _exit(1);
  }
#endif

 return returnValue;
}

nsresult SetExceptionHandler(nsILocalFile* aXREDirectory,
                             const char* aServerURL)
{
  nsresult rv;

  if (gExceptionHandler)
    return NS_ERROR_ALREADY_INITIALIZED;

  const char *envvar = PR_GetEnv("MOZ_CRASHREPORTER_DISABLE");
  if (envvar && *envvar)
    return NS_OK;

  // this environment variable prevents us from launching
  // the crash reporter client
  envvar = PR_GetEnv("MOZ_CRASHREPORTER_NO_REPORT");
  if (envvar && *envvar)
    doReport = false;

  // allocate our strings
  crashReporterAPIData = new nsCString();
  NS_ENSURE_TRUE(crashReporterAPIData, NS_ERROR_OUT_OF_MEMORY);

  crashReporterAPIData_Hash =
    new nsDataHashtable<nsCStringHashKey,nsCString>();
  NS_ENSURE_TRUE(crashReporterAPIData_Hash, NS_ERROR_OUT_OF_MEMORY);

  rv = crashReporterAPIData_Hash->Init();
  NS_ENSURE_SUCCESS(rv, rv);

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
#else
  nsCString crashReporterPath_temp;
  exePath->GetNativePath(crashReporterPath_temp);

  crashReporterPath = ToNewCString(crashReporterPath_temp);
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

#elif defined(XP_UNIX)
  // we assume it's always /tmp on unix systems
  nsCString tempPath = NS_LITERAL_CSTRING("/tmp/");
#else
#error "Implement this for your platform"
#endif

  // now set the exception handler
  gExceptionHandler = new google_breakpad::
    ExceptionHandler(tempPath.get(),
                     nsnull,
                     MinidumpCallback,
                     nsnull,
#if defined(XP_WIN32)
                     google_breakpad::ExceptionHandler::HANDLER_ALL);
#else
                     true);
#endif

  if (!gExceptionHandler)
    return NS_ERROR_OUT_OF_MEMORY;

  // store server URL with the API data
  if (aServerURL)
    AnnotateCrashReport(NS_LITERAL_CSTRING("ServerURL"),
                        nsDependentCString(aServerURL));

#if defined(XP_MACOSX)
  // On OS X, many testers like to see the OS crash reporting dialog
  // since it offers immediate stack traces.  We allow them to set
  // a default to pass exceptions to the OS handler.
  showOSCrashReporter = PassToOSCrashReporter();
#endif

  return NS_OK;
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

// Generate a unique user ID.  We're using a GUID form,
// but not jumping through hoops to make it cryptographically
// secure.  We just want it to distinguish unique users.
static nsresult
InitUserID(nsACString& aUserID)
{
  nsID id;

  // copied shamelessly from nsUUIDGenerator.cpp
#if defined(XP_WIN)
  HRESULT hr = CoCreateGuid((GUID*)&id);
  if (NS_FAILED(hr))
    return NS_ERROR_FAILURE;
#elif defined(XP_MACOSX)
  CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
  if (!uuid)
    return NS_ERROR_FAILURE;

  CFUUIDBytes bytes = CFUUIDGetUUIDBytes(uuid);
  memcpy(&id, &bytes, sizeof(nsID));

  CFRelease(uuid);
#else
  // UNIX or some such thing
  id.m0 = random();
  id.m1 = random();
  id.m2 = random();
  *reinterpret_cast<PRUint32*>(&id.m3[0]) = random();
  *reinterpret_cast<PRUint32*>(&id.m3[4]) = random();
#endif

  nsCAutoString id_str(id.ToString());
  aUserID = Substring(id_str, 1, id_str.Length()-2);

  return NS_OK;
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

  // Save this path in the environment for the crash reporter application.
  nsCAutoString dataDirEnv("MOZ_CRASHREPORTER_DATA_DIRECTORY=");

#if defined(XP_WIN32)
  nsAutoString dataDirectoryPath;
  rv = dataDirectory->GetPath(dataDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendUTF16toUTF8(dataDirectoryPath, dataDirEnv);
#else
  nsCAutoString dataDirectoryPath;
  rv = dataDirectory->GetNativePath(dataDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  dataDirEnv.Append(dataDirectoryPath);
#endif

  char* env = ToNewCString(dataDirEnv);
  NS_ENSURE_TRUE(env, NS_ERROR_OUT_OF_MEMORY);

  PR_SetEnv(env);

  nsCAutoString data;
  if(NS_SUCCEEDED(GetOrInit(dataDirectory, NS_LITERAL_CSTRING("UserID"),
                            data, InitUserID)))
    AnnotateCrashReport(NS_LITERAL_CSTRING("UserID"), data);

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

nsresult UnsetExceptionHandler()
{
  // do this here in the unlikely case that we succeeded in allocating
  // our strings but failed to allocate gExceptionHandler.
  if (crashReporterAPIData_Hash) {
    delete crashReporterAPIData_Hash;
    crashReporterAPIData_Hash = nsnull;
  }
  if (crashReporterPath) {
    NS_Free(crashReporterPath);
    crashReporterPath = nsnull;
  }

  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  delete gExceptionHandler;
  gExceptionHandler = nsnull;

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

static PLDHashOperator PR_CALLBACK EnumerateEntries(const nsACString& key,
                                                    nsCString entry,
                                                    void* userData)
{
  crashReporterAPIData->Append(key + NS_LITERAL_CSTRING("=") + entry +
                               NS_LITERAL_CSTRING("\n"));
  return PL_DHASH_NEXT;
}

nsresult AnnotateCrashReport(const nsACString &key, const nsACString &data)
{
  if (!gExceptionHandler)
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

nsresult
SetRestartArgs(int argc, char **argv)
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
    envVar += argv[i];

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
} // namespace CrashReporter
