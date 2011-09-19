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

#include "nsExceptionHandler.h"

#define ALL_VERSIONS   ((unsigned long long)-1LL)

// DLLs sometimes ship without a version number, particularly early
// releases. Blocking "version <= 0" has the effect of blocking unversioned
// DLLs (since the call to get version info fails), but not blocking
// any versioned instance.
#define UNVERSIONED    ((unsigned long long)0LL)

// Convert the 4 (decimal) components of a DLL version number into a
// single unsigned long long, as needed by the blocklist
#define MAKE_VERSION(a,b,c,d)\
  ((a##ULL << 48) + (b##ULL << 32) + (c##ULL << 16) + d##ULL)

struct DllBlockInfo {
  // The name of the DLL -- in LOWERCASE!  It will be compared to
  // a lowercase version of the DLL name only.
  const char *name;

  // If maxVersion is ALL_VERSIONS, we'll block all versions of this
  // dll.  Otherwise, we'll block all versions less than or equal to
  // the given version, as queried by GetFileVersionInfo and
  // VS_FIXEDFILEINFO's dwFileVersionMS and dwFileVersionLS fields.
  //
  // Note that the version is usually 4 components, which is A.B.C.D
  // encoded as 0x AAAA BBBB CCCC DDDD ULL (spaces added for clarity),
  // but it's not required to be of that format.
  unsigned long long maxVersion;
};

static DllBlockInfo sWindowsDllBlocklist[] = {
  // EXAMPLE:
  // { "uxtheme.dll", ALL_VERSIONS },
  // { "uxtheme.dll", 0x0000123400000000ULL },
  // The DLL name must be in lowercase!
  
  // NPFFAddon - Known malware
  { "npffaddon.dll", ALL_VERSIONS},

  // AVG 8 - Antivirus vendor AVG, old version, plugin already blocklisted
  {"avgrsstx.dll", MAKE_VERSION(8,5,0,401)},
  
  // calc.dll - Suspected malware
  {"calc.dll", MAKE_VERSION(1,0,0,1)},

  // hook.dll - Suspected malware
  {"hook.dll", ALL_VERSIONS},
  
  // GoogleDesktopNetwork3.dll - Extremely old, unversioned instances
  // of this DLL cause crashes
  {"googledesktopnetwork3.dll", UNVERSIONED},

  // rdolib.dll - Suspected malware
  {"rdolib.dll", MAKE_VERSION(6,0,88,4)},

  // fgjk4wvb.dll - Suspected malware
  {"fgjk4wvb.dll", MAKE_VERSION(8,8,8,8)},
  
  // radhslib.dll - Naomi internet filter - unmaintained since 2006
  {"radhslib.dll", UNVERSIONED},

  // Music download filter for vkontakte.ru - old instances
  // of this DLL cause crashes
  {"vksaver.dll", MAKE_VERSION(2,2,2,0)},

  // Topcrash in Firefox 4.0b1
  {"rlxf.dll", MAKE_VERSION(1,2,323,1)},

  // psicon.dll - Topcrashes in Thunderbird, and some crashes in Firefox
  // Adobe photoshop library, now redundant in later installations
  {"psicon.dll", ALL_VERSIONS},

  // Topcrash in Firefox 4 betas (bug 618899)
  {"accelerator.dll", MAKE_VERSION(3,2,1,6)},
  
  // leave these two in always for tests
  { "mozdllblockingtest.dll", ALL_VERSIONS },
  { "mozdllblockingtest_versioned.dll", 0x0000000400000000ULL },

  { NULL, 0 }
};

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

  if (!ok) {
    CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("DllBlockList Failed\n"));
  }
}
