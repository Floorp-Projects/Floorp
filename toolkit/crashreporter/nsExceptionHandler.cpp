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
#endif

#include <stdlib.h>
#include <prenv.h>
#include "nsDebug.h"

static google_airbag::ExceptionHandler* gAirbagExceptionHandler = nsnull;

#ifdef XP_WIN32
static TCHAR crashReporterExe[MAX_PATH] = L"\0";
static TCHAR minidumpPath[MAX_PATH] = L"\0";
#endif

void AirbagMinidumpCallback(const std::wstring &minidump_id,
                            void *context, bool succeeded)
{
#ifdef XP_WIN32
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  TCHAR cmdLine[2*MAX_PATH];

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWNORMAL;
  ZeroMemory(&pi, sizeof(pi));

  wcscat(minidumpPath, minidump_id.c_str());
  wcscat(minidumpPath, L".dmp");
  wsprintf(cmdLine, L"\"%s\" \"%s\"", crashReporterExe, minidumpPath);
  
  if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
  }
  // we're not really in a position to do anything if the CreateProcess fails
  TerminateProcess(GetCurrentProcess(), 1);
#endif
}

nsresult SetAirbagExceptionHandler()
{
  if (gAirbagExceptionHandler)
    return NS_ERROR_ALREADY_INITIALIZED;

  // check environment var to see if we're enabled.
  // we're off by default until we sort out the
  // rest of the infrastructure,
  // so it must exist and be set to a non-zero value.
  const char *airbagEnv = PR_GetEnv("MOZ_AIRBAG");
  if (airbagEnv == NULL || atoi(airbagEnv) == 0)
    return NS_OK;

#ifdef XP_WIN32
  //TODO: check registry to see if crash reporting is enabled
  if (!GetTempPath(MAX_PATH, minidumpPath))
    return NS_ERROR_FAILURE;

  std::wstring tempStr(minidumpPath);
  gAirbagExceptionHandler = new google_airbag::ExceptionHandler(tempStr,
                                                 AirbagMinidumpCallback,
                                                 nsnull,
                                                 true);
  if (GetModuleFileName(NULL, crashReporterExe, MAX_PATH)) {
    // get crashreporter exe
    LPTSTR s = wcsrchr(crashReporterExe, '\\');
    if (s) {
      s++;
      wcscpy(s, L"crashreporter.exe");
    }
  }
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

  if (!gAirbagExceptionHandler)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsresult SetAirbagMinidumpPath(const nsAString* aPath)
{
  NS_ENSURE_ARG_POINTER(aPath);

  if (!gAirbagExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  std::wstring path;
#ifdef XP_WIN32
  wcsncpy(minidumpPath, PromiseFlatString(*aPath).get(), MAX_PATH);
  path = std::wstring(minidumpPath);
  int l = wcslen(minidumpPath);
  if (minidumpPath[l-1] != '\\' && l < MAX_PATH - 1) {
    minidumpPath[l] = '\\';
    minidumpPath[l+1] = '\0';
  }
#endif
  gAirbagExceptionHandler->set_dump_path(path);
  return NS_OK;
}

nsresult UnsetAirbagExceptionHandler()
{
  if (!gAirbagExceptionHandler)
    return NS_ERROR_NOT_INITIALIZED;

  delete gAirbagExceptionHandler;
  gAirbagExceptionHandler = nsnull;

  return NS_OK;
}
