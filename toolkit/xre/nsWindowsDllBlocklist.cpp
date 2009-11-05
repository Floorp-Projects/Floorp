/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corp
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
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

#include <windows.h>
#include <winternl.h>

#include <stdio.h>

#include "nsAutoPtr.h"

#include "prlog.h"

#include "nsWindowsDllInterceptor.h"

#define IN_WINDOWS_DLL_BLOCKLIST
#include "nsWindowsDllBlocklist.h"

#ifndef STATUS_DLL_NOT_FOUND
#define STATUS_DLL_NOT_FOUND ((DWORD)0xC0000135L)
#endif

typedef NTSTATUS (NTAPI *LdrLoadDll_func) (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle);

static LdrLoadDll_func stub_LdrLoadDll = 0;

static NTSTATUS NTAPI
patched_LdrLoadDll (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle)
{
  // We have UCS2 (UTF16?), we want ASCII, but we also just want the filename portion
#define DLLNAME_MAX 128
  char dllName[DLLNAME_MAX+1];

  // Dirty secret about this UNICODE_STRING: it's not guaranteed to be
  // null-terminated, and Length is supposed to contain the number of
  // characters.  But in the UNICODE_STRING passed to this function,
  // that doesn't seem to be true -- Length is often much bigger than
  // the actual valid characters of the string, which seems to always
  // be null terminated.  So, we take the minimum of len or the length
  // to the first null byte, if any, but we still can't assume the null
  // termination.
  int len = moduleFileName->Length;
  wchar_t *fn_buf = moduleFileName->Buffer;

  int count = 0;
  while (count < len && fn_buf[count] != 0)
    count++;

  len = count;

  // copy it into fname, which will then be guaranteed null-terminated
  nsAutoArrayPtr<wchar_t> fname = new wchar_t[len+1];
  wcsncpy(fname, moduleFileName->Buffer, len);
  fname[len] = 0; // *ncpy considered harmful

  wchar_t *dll_part = wcsrchr(fname, L'\\');
  if (dll_part) {
    dll_part = dll_part + 1;
    len = (fname+len) - dll_part;
  } else {
    dll_part = fname;
  }

  // if it's too long, then, we assume we won't want to block it,
  // since DLLNAME_MAX should be at least long enough to hold the longest
  // entry in our blocklist.
  if (len > DLLNAME_MAX)
    goto continue_loading;

  // copy over to our char byte buffer, lowercasing ASCII as we go
  for (int i = 0; i < len; i++) {
    wchar_t c = dll_part[i];
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';

    if (c > 0x7f) {
      // welp, it's not ascii; if we need to add non-ascii things to
      // our blocklist, we'll have to remove this limitation.
      goto continue_loading;
    }

    dllName[i] = (char) c;
  }

  dllName[len] = 0;

  // then compare to everything on the blocklist
  DllBlockInfo *info = &sWindowsDllBlocklist[0];
  while (info->name) {
    if (strcmp(info->name, dllName) == 0)
      break;

    info++;
  }

  if (info->name) {
    BOOL load_ok = FALSE;

    if (info->maxVersion != ALL_VERSIONS) {
      // figure out the length of the string that we need
      DWORD pathlen = SearchPathW(filePath, fname, L".dll", 0, NULL, NULL);
      if (pathlen == 0) {
        // uh, we couldn't find the DLL at all, so...
        return STATUS_DLL_NOT_FOUND;
      }

      nsAutoArrayPtr<wchar_t> full_fname = new wchar_t[pathlen+1];

      // now actually grab it
      SearchPathW(filePath, fname, L".dll", pathlen+1, full_fname, NULL);

      DWORD zero;
      DWORD infoSize = GetFileVersionInfoSizeW(full_fname, &zero);

      // If we failed to get the version information, we block.

      if (infoSize != 0) {
        nsAutoArrayPtr<unsigned char> infoData = new unsigned char[infoSize];
        VS_FIXEDFILEINFO *vInfo;
        UINT vInfoLen;

        if (GetFileVersionInfoW(full_fname, 0, infoSize, infoData) &&
            VerQueryValueW(infoData, L"\\", (LPVOID*) &vInfo, &vInfoLen))
        {
          unsigned long long fVersion =
            ((unsigned long long)vInfo->dwFileVersionMS) << 32 |
            ((unsigned long long)vInfo->dwFileVersionLS);

          // finally do the version check, and if it's greater than our block
          // version, keep loading
          if (fVersion > info->maxVersion)
            goto continue_loading;
        }
      }
    }

    PR_LogPrint("LdrLoadDll: Blocking load of '%s'", dllName);
    return STATUS_DLL_NOT_FOUND;
  }

continue_loading:
  return stub_LdrLoadDll(filePath, flags, moduleFileName, handle);
}

WindowsDllInterceptor NtDllIntercept;

void
SetupDllBlocklist()
{
  NtDllIntercept.Init("ntdll.dll");

  bool ok = NtDllIntercept.AddHook("LdrLoadDll", patched_LdrLoadDll, (void**) &stub_LdrLoadDll);

  if (!ok)
    PR_LogPrint ("LdrLoadDll hook failed, no dll blocklisting active");
}




