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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
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

#ifdef XRE_WANT_DLL_BLOCKLIST
#define XRE_SetupDllBlocklist SetupDllBlocklist
#else
#include "nsXULAppAPI.h"
#endif

#include "nsAutoPtr.h"

#include "prlog.h"

#include "nsWindowsDllInterceptor.h"

#define IN_WINDOWS_DLL_BLOCKLIST
#include "nsWindowsDllBlocklist.h"

#ifndef STATUS_DLL_NOT_FOUND
#define STATUS_DLL_NOT_FOUND ((DWORD)0xC0000135L)
#endif

// define this for very verbose dll load debug spew
#undef DEBUG_very_verbose

typedef NTSTATUS (NTAPI *LdrLoadDll_func) (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle);

static LdrLoadDll_func stub_LdrLoadDll = 0;

static NTSTATUS NTAPI
patched_LdrLoadDll (PWCHAR filePath, PULONG flags, PUNICODE_STRING moduleFileName, PHANDLE handle)
{
  // We have UCS2 (UTF16?), we want ASCII, but we also just want the filename portion
#define DLLNAME_MAX 128
  char dllName[DLLNAME_MAX+1];
  wchar_t *dll_part;
  DllBlockInfo *info;

  int len = moduleFileName->Length / 2;
  wchar_t *fname = moduleFileName->Buffer;

  // The filename isn't guaranteed to be null terminated, but in practice
  // it always will be; ensure that this is so, and bail if not.
  // This is done instead of the more robust approach because of bug 527122,
  // where lots of weird things were happening when we tried to make a copy.
  if (moduleFileName->MaximumLength < moduleFileName->Length+2 ||
      fname[len] != 0)
  {
#ifdef DEBUG
    printf_stderr("LdrLoadDll: non-null terminated string found!\n");
#endif
    goto continue_loading;
  }

  dll_part = wcsrchr(fname, L'\\');
  if (dll_part) {
    dll_part = dll_part + 1;
    len -= dll_part - fname;
  } else {
    dll_part = fname;
  }

#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: dll_part '%S' %d\n", dll_part, len);
#endif

  // if it's too long, then, we assume we won't want to block it,
  // since DLLNAME_MAX should be at least long enough to hold the longest
  // entry in our blocklist.
  if (len > DLLNAME_MAX) {
#ifdef DEBUG
    printf_stderr("LdrLoadDll: len too long! %d\n", len);
#endif
    goto continue_loading;
  }

  // copy over to our char byte buffer, lowercasing ASCII as we go
  for (int i = 0; i < len; i++) {
    wchar_t c = dll_part[i];

    if (c > 0x7f) {
      // welp, it's not ascii; if we need to add non-ascii things to
      // our blocklist, we'll have to remove this limitation.
      goto continue_loading;
    }

    // ensure that dll name is all lowercase
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';

    dllName[i] = (char) c;
  }

  dllName[len] = 0;

#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: dll name '%s'\n", dllName);
#endif

  // then compare to everything on the blocklist
  info = &sWindowsDllBlocklist[0];
  while (info->name) {
    if (strcmp(info->name, dllName) == 0)
      break;

    info++;
  }

  if (info->name) {
    bool load_ok = false;

#ifdef DEBUG_very_verbose
    printf_stderr("LdrLoadDll: info->name: '%s'\n", info->name);
#endif

    if (info->maxVersion != ALL_VERSIONS) {
      // figure out the length of the string that we need
      DWORD pathlen = SearchPathW(filePath, fname, L".dll", 0, NULL, NULL);
      if (pathlen == 0) {
        // uh, we couldn't find the DLL at all, so...
        printf_stderr("LdrLoadDll: Blocking load of '%s' (SearchPathW didn't find it?)\n", dllName);
        return STATUS_DLL_NOT_FOUND;
      }

      wchar_t *full_fname = (wchar_t*) malloc(sizeof(wchar_t)*(pathlen+1));
      if (!full_fname) {
        // couldn't allocate memory?
        return STATUS_DLL_NOT_FOUND;
      }

      // now actually grab it
      SearchPathW(filePath, fname, L".dll", pathlen+1, full_fname, NULL);

      DWORD zero;
      DWORD infoSize = GetFileVersionInfoSizeW(full_fname, &zero);

      // If we failed to get the version information, we block.

      if (infoSize != 0) {
        nsAutoArrayPtr<unsigned char> infoData(new unsigned char[infoSize]);
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
            load_ok = true;
        }
      }

      free(full_fname);
    }

    if (!load_ok) {
      printf_stderr("LdrLoadDll: Blocking load of '%s' -- see http://www.mozilla.com/en-US/blocklist/\n", dllName);
      return STATUS_DLL_NOT_FOUND;
    }
  }

continue_loading:
#ifdef DEBUG_very_verbose
  printf_stderr("LdrLoadDll: continuing load... ('%S')\n", moduleFileName->Buffer);
#endif

  NS_SetHasLoadedNewDLLs();

  return stub_LdrLoadDll(filePath, flags, moduleFileName, handle);
}

WindowsDllInterceptor NtDllIntercept;

void
XRE_SetupDllBlocklist()
{
  NtDllIntercept.Init("ntdll.dll");

  bool ok = NtDllIntercept.AddHook("LdrLoadDll", reinterpret_cast<intptr_t>(patched_LdrLoadDll), (void**) &stub_LdrLoadDll);

#ifdef DEBUG
  if (!ok)
    printf_stderr ("LdrLoadDll hook failed, no dll blocklisting active\n");
#endif
}
