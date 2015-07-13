/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
} // namespace

void LaunchChildMac(int aArgc, char** aArgv,
                    uint32_t aRestartType, pid_t *pid)
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

  int result = posix_spawnp(pid, argv_copy[0], NULL, &spawnattr, argv_copy, envp);

  posix_spawnattr_destroy(&spawnattr);

  if (result != 0) {
    printf("Process spawn failed with code %d!", result);
  }
}
