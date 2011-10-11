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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@mozilla.org>
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

#include "mozilla/Util.h"

#include "prtypes.h"
#include "MacLaunchHelper.h"

#include "nsMemory.h"
#include "nsAutoPtr.h"
#include "nsIAppStartup.h"

#include <stdio.h>
#include <spawn.h>
#include <crt_externs.h>

using namespace mozilla;

namespace {
cpu_type_t pref_cpu_types[2] = {
#if defined(__i386__)
                                 CPU_TYPE_X86,
#elif defined(__x86_64__)
                                 CPU_TYPE_X86_64,
#elif defined(__ppc__)
                                 CPU_TYPE_POWERPC,
#endif
                                 CPU_TYPE_ANY };

cpu_type_t cpu_i386_types[2] = {
                                 CPU_TYPE_X86,
                                 CPU_TYPE_ANY };

cpu_type_t cpu_x64_86_types[2] = {
                                 CPU_TYPE_X86_64,
                                 CPU_TYPE_ANY };
}

void LaunchChildMac(int aArgc, char** aArgv, PRUint32 aRestartType)
{
  // "posix_spawnp" uses null termination for arguments rather than a count.
  // Note that we are not duplicating the argument strings themselves.
  nsAutoArrayPtr<char*> argv_copy(new char*[aArgc + 1]);
  for (int i = 0; i < aArgc; i++) {
    argv_copy[i] = aArgv[i];
  }
  argv_copy[aArgc] = NULL;

  // Initialize spawn attributes.
  posix_spawnattr_t spawnattr;
  if (posix_spawnattr_init(&spawnattr) != 0) {
    printf("Failed to init posix spawn attribute.");
    return;
  }

  cpu_type_t *wanted_type = pref_cpu_types;
  size_t attr_count = ArrayLength(pref_cpu_types);

  if (aRestartType & nsIAppStartup::eRestarti386) {
    wanted_type = cpu_i386_types;
    attr_count = ArrayLength(cpu_i386_types);
  } else if (aRestartType & nsIAppStartup::eRestartx86_64) {
    wanted_type = cpu_x64_86_types;
    attr_count = ArrayLength(cpu_x64_86_types);
  }

  // Set spawn attributes.
  size_t attr_ocount = 0;
  if (posix_spawnattr_setbinpref_np(&spawnattr, attr_count, wanted_type, &attr_ocount) != 0 ||
      attr_ocount != attr_count) {
    printf("Failed to set binary preference on posix spawn attribute.");
    posix_spawnattr_destroy(&spawnattr);
    return;
  }

  // Pass along our environment.
  char** envp = NULL;
  char*** cocoaEnvironment = _NSGetEnviron();
  if (cocoaEnvironment) {
    envp = *cocoaEnvironment;
  }

  int result = posix_spawnp(NULL, argv_copy[0], NULL, &spawnattr, argv_copy, envp);

  posix_spawnattr_destroy(&spawnattr);

  if (result != 0) {
    printf("Process spawn failed with code %d!", result);
  }
}
