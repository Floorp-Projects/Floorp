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

#ifndef IN_WINDOWS_DLL_BLOCKLIST
#error This file should only be included by nsWindowsDllBlocklist.cpp
#endif

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
  {"vksaver.dll", MAKE_VERSION(1,0,0,1)},

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
