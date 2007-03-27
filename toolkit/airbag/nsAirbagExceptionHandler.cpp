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
 * The Original Code is Mozilla Airbag integration
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

#include "nsAirbagExceptionHandler.h"

#ifdef XP_WIN32
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "client/windows/handler/exception_handler.h"
#include <string.h>
#else
#error "Not yet implemented for this platform"
#endif // XP_WIN32

#ifndef HAVE_CPP_2BYTE_WCHAR_T
#error "This code expects a 2 byte wchar_t.  You should --disable-airbag."
#endif

#include <stdlib.h>
#include <prenv.h>
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsILocalFile.h"

namespace CrashReporter {

using std::wstring;

static const PRUnichar dumpFileExtension[] = {'.', 'd', 'm', 'p',
                                              '\"', '\0'}; // .dmp"
static const PRUnichar extraFileExtension[] = {'.', 'e', 'x', 't',
                                              'r', 'a', '\0'}; // .extra

// xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
static const PRInt32 kGUIDLength = 36;
// length of a GUID + .dmp" (yes, trailing double quote)
static const PRInt32 kMinidumpFilenameLength =
  kGUIDLength + sizeof(dumpFileExtension) / sizeof(dumpFileExtension[0]);

#ifdef XP_WIN32
NS_NAMED_LITERAL_STRING(crashReporterFilename, "crashreporter.exe");
#else
NS_NAMED_LITERAL_STRING(crashReporterFilename, "crashreporter");
#endif

static google_airbag::ExceptionHandler* gExceptionHandler = nsnull;

// for ease of replacing the dump path when someone
// calls SetMinidumpPath
static nsString crashReporterCmdLine_withoutDumpPath;
// this is set up so we don't have to do heap allocation in the handler
static PRUnichar* crashReporterCmdLine = nsnull;
// points at the end of the previous string
// so we can append the minidump filename
static PRUnichar* crashReporterCmdLineEnd = nsnull;
// space to hold a filename for the API data
static PRUnichar* crashReporterAPIDataFilename = nsnull;
static PRUnichar* crashReporterAPIDataFilenameEnd = nsnull;

// this holds additional data sent via the API
static nsCString crashReporterAPIData;

static void MinidumpCallback(const wstring &minidump_id,
                             void *context, bool succeeded)
{
  // append minidump filename to command line
  memcpy(crashReporterCmdLineEnd, minidump_id.c_str(),
         kGUIDLength * sizeof(PRUnichar));
  // this will copy the null terminator as well
  memcpy(crashReporterCmdLineEnd + kGUIDLength,
         dumpFileExtension, sizeof(dumpFileExtension));

  // append minidump filename to API data filename
  memcpy(crashReporterAPIDataFilenameEnd, minidump_id.c_str(),
         kGUIDLength * sizeof(PRUnichar));
  // this will copy the null terminator as well
  memcpy(crashReporterAPIDataFilenameEnd + kGUIDLength,
         extraFileExtension, sizeof(extraFileExtension));

#ifdef XP_WIN32
  if (!crashReporterAPIData.IsEmpty()) {
    // write out API data
    HANDLE hFile = CreateFile(crashReporterAPIDataFilename, GENERIC_WRITE, 0,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if(hFile != INVALID_HANDLE_VALUE) {
      DWORD nBytes;
      WriteFile(hFile, crashReporterAPIData.get(),
                crashReporterAPIData.Length(), &nBytes, NULL);
      CloseHandle(hFile);
    }
  }

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWNORMAL;
  ZeroMemory(&pi, sizeof(pi));

  if (CreateProcess(NULL, (LPWSTR)crashReporterCmdLine, NULL, NULL, FALSE, 0,
                    NULL, NULL, &si, &pi)) {
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
  }
  // we're not really in a position to do anything if the CreateProcess fails
  TerminateProcess(GetCurrentProcess(), 1);
#endif
}

static nsresult BuildCommandLine(const nsAString &tempPath)
{
  nsString crashReporterCmdLine_temp =
    crashReporterCmdLine_withoutDumpPath + NS_LITERAL_STRING(" \"") + tempPath;
  PRInt32 cmdLineLength = crashReporterCmdLine_temp.Length();

  // allocate extra space for minidump file name
  crashReporterCmdLine_temp.SetLength(cmdLineLength + kMinidumpFilenameLength
                                      + 1);
  crashReporterCmdLine = ToNewUnicode(crashReporterCmdLine_temp);
  crashReporterCmdLineEnd = crashReporterCmdLine + cmdLineLength;

  // build API data filename
  if(crashReporterAPIDataFilename != nsnull) {
    NS_Free(crashReporterAPIDataFilename);
    crashReporterAPIDataFilename = nsnull;
  }

  nsString apiDataFilename_temp(tempPath);
  PRInt32 filenameLength = apiDataFilename_temp.Length();
  apiDataFilename_temp.SetLength(filenameLength + kMinidumpFilenameLength + 1);
  crashReporterAPIDataFilename = ToNewUnicode(apiDataFilename_temp);
  crashReporterAPIDataFilenameEnd =
    crashReporterAPIDataFilename + filenameLength;

  return NS_OK;
}

static nsresult GetExecutablePath(nsString& exePath)
{
#ifdef XP_WIN32
  // sort of arbitrary, but MAX_PATH is kinda small
  exePath.SetLength(4096);
  if (!GetModuleFileName(NULL, (LPWSTR)exePath.BeginWriting(), 4096))
    return NS_ERROR_FAILURE;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

#ifdef XP_WIN32
  NS_NAMED_LITERAL_STRING(pathSep, "\\");
#else
  NS_NAMED_LITERAL_STRING(pathSep, "/");
#endif

  PRInt32 lastSlash = exePath.RFind(pathSep);
  if (lastSlash < 0)
    return NS_ERROR_FAILURE;

  exePath.Truncate(lastSlash + 1);

  return NS_OK;
}

nsresult SetExceptionHandler(nsILocalFile* aXREDirectory)
{
  nsresult rv;

  if (gExceptionHandler)
    return NS_ERROR_ALREADY_INITIALIZED;

  // check environment var to see if we're enabled.
  // we're off by default until we sort out the
  // rest of the infrastructure,
  // so it must exist and be set to a non-zero value.
  const char *airbagEnv = PR_GetEnv("MOZ_AIRBAG");
  if (airbagEnv == NULL || atoi(airbagEnv) == 0)
    return NS_ERROR_NOT_AVAILABLE;

  // locate crashreporter executable
  nsString exePath;

  if (aXREDirectory) {
    aXREDirectory->GetPath(exePath);
  }
  else {
    rv = GetExecutablePath(exePath);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // note that we enclose the exe filename in double quotes
  crashReporterCmdLine_withoutDumpPath = NS_LITERAL_STRING("\"") +
    exePath + crashReporterFilename + NS_LITERAL_STRING("\"");

  // get temp path to use for minidump path
  nsString tempPath;
#ifdef XP_WIN32
  // first figure out buffer size
  int pathLen = GetTempPath(0, NULL);
  if (pathLen == 0)
    return NS_ERROR_FAILURE;

  tempPath.SetLength(pathLen);
  GetTempPath(pathLen, (LPWSTR)tempPath.BeginWriting());
#endif

  rv = BuildCommandLine(tempPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // finally, set the exception handler
  gExceptionHandler = new google_airbag::ExceptionHandler(
                                            PromiseFlatString(tempPath).get(),
                                            MinidumpCallback,
                                            nsnull,
                                            true);

  if (!gExceptionHandler)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsresult SetMinidumpPath(const nsAString& aPath)
{
  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  if(crashReporterCmdLine != nsnull) {
    NS_Free(crashReporterCmdLine);
    crashReporterCmdLine = nsnull;
  }

#ifdef XP_WIN32
  NS_NAMED_LITERAL_STRING(pathSep, "\\");
#else
  NS_NAMED_LITERAL_STRING(pathSep, "/");
#endif

  nsresult rv;

  if(!StringEndsWith(aPath, pathSep)) {
    rv = BuildCommandLine(aPath + pathSep);
  }
  else {
    rv = BuildCommandLine(aPath);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  gExceptionHandler->set_dump_path(PromiseFlatString(aPath).get());
  return NS_OK;
}

nsresult UnsetExceptionHandler()
{
  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  delete gExceptionHandler;
  gExceptionHandler = nsnull;

  if(crashReporterCmdLine != nsnull) {
    NS_Free(crashReporterCmdLine);
    crashReporterCmdLine = nsnull;
  }

  if(crashReporterAPIDataFilename != nsnull) {
    NS_Free(crashReporterAPIDataFilename);
    crashReporterAPIDataFilename = nsnull;
  }

  crashReporterCmdLineEnd = nsnull;

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

nsresult AnnotateCrashReport(const nsACString &key, const nsACString &data)
{
  if (!gExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  if (DoFindInReadable(key, NS_LITERAL_CSTRING("=")) ||
      DoFindInReadable(key, NS_LITERAL_CSTRING("\n")))
    return NS_ERROR_INVALID_ARG;

  nsCString escapedData(data);
  
  // escape backslashes
  ReplaceChar(escapedData, NS_LITERAL_CSTRING("\\"),
              NS_LITERAL_CSTRING("\\\\"));
  // escape newlines
  ReplaceChar(escapedData, NS_LITERAL_CSTRING("\n"),
              NS_LITERAL_CSTRING("\\n"));

  crashReporterAPIData.Append(key + NS_LITERAL_CSTRING("=") + escapedData +
                              NS_LITERAL_CSTRING("\n"));
  return NS_OK;
}

} // namespace CrashReporter
