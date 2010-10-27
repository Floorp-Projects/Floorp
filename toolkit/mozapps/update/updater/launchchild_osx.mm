/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <Cocoa/Cocoa.h>
#include <CoreServices/CoreServices.h>
#include <crt_externs.h>
#include <stdlib.h>
#include <stdio.h>
#include <spawn.h>
#include "readstrings.h"

// Prefer the currently running architecture (this is the same as the
// architecture that launched the updater) and fallback to CPU_TYPE_ANY if it
// is no longer available after the update.
static cpu_type_t pref_cpu_types[2] = {
#if defined(__i386__)
                                 CPU_TYPE_X86,
#elif defined(__x86_64__)
                                 CPU_TYPE_X86_64,
#elif defined(__ppc__)
                                 CPU_TYPE_POWERPC,
#endif
                                 CPU_TYPE_ANY };

void LaunchChild(int argc, char **argv)
{
  // Initialize spawn attributes.
  posix_spawnattr_t spawnattr;
  if (posix_spawnattr_init(&spawnattr) != 0) {
    printf("Failed to init posix spawn attribute.");
    return;
  }

  // Set spawn attributes.
  size_t attr_count = 2;
  size_t attr_ocount = 0;
  if (posix_spawnattr_setbinpref_np(&spawnattr, attr_count, pref_cpu_types, &attr_ocount) != 0 ||
      attr_ocount != attr_count) {
    printf("Failed to set binary preference on posix spawn attribute.");
    posix_spawnattr_destroy(&spawnattr);
    return;
  }

  // "posix_spawnp" uses null termination for arguments rather than a count.
  // Note that we are not duplicating the argument strings themselves.
  char** argv_copy = (char**)malloc((argc + 1) * sizeof(char*));
  if (!argv_copy) {
    printf("Failed to allocate memory for arguments.");
    posix_spawnattr_destroy(&spawnattr);
    return;
  }
  for (int i = 0; i < argc; i++) {
    argv_copy[i] = argv[i];
  }
  argv_copy[argc] = NULL;

  // Pass along our environment.
  char** envp = NULL;
  char*** cocoaEnvironment = _NSGetEnviron();
  if (cocoaEnvironment) {
    envp = *cocoaEnvironment;
  }

  int result = posix_spawnp(NULL, argv_copy[0], NULL, &spawnattr, argv_copy, envp);

  free(argv_copy);
  posix_spawnattr_destroy(&spawnattr);

  if (result != 0) {
    printf("Process spawn failed with code %d!", result);
  }
}

void
LaunchMacPostProcess(const char* aAppExe)
{
  // Launch helper to perform post processing for the update; this is the Mac
  // analogue of LaunchWinPostProcess (PostUpdateWin).
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // Find the app bundle containing the executable path given
  NSString *path = [NSString stringWithUTF8String:aAppExe];
  NSBundle *bundle;
  do {
    path = [path stringByDeletingLastPathComponent];
    bundle = [NSBundle bundleWithPath:path];
  } while ((!bundle || ![bundle bundleIdentifier]) && [path length] > 1);
  if (!bundle) {
    // No bundle found for the app being launched
    [pool release];
    return;
  }

  NSString *iniPath = [bundle pathForResource:@"updater" ofType:@"ini"];
  if (!iniPath) {
    // the file does not exist; there is nothing to run
    [pool release];
    return;
  }

  int readResult;
  char values[2][MAX_TEXT_LEN];
  readResult = ReadStrings([iniPath UTF8String],
                           "ExeArg\0ExeRelPath\0",
                           2,
                           values,
                           "PostUpdateMac");
  if (readResult) {
    [pool release];
    return;
  }

  NSString *exeArg = [NSString stringWithUTF8String:values[0]];
  NSString *exeRelPath = [NSString stringWithUTF8String:values[1]];
  if (!exeArg || !exeRelPath) {
    [pool release];
    return;
  }
  
  NSString *resourcePath = [bundle resourcePath];
  NSString *exeFullPath = [resourcePath stringByAppendingPathComponent:exeRelPath];

  NSTask *task = [[NSTask alloc] init];
  [task setLaunchPath:exeFullPath];
  [task setArguments:[NSArray arrayWithObject:exeArg]];
  [task launch];
  [task waitUntilExit];
  // ignore the return value of the task, there's nothing we can do with it
  [task release];

  [pool release];  
}

