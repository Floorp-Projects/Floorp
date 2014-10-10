/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
LaunchMacPostProcess(const char* aAppBundle)
{
  // Launch helper to perform post processing for the update; this is the Mac
  // analogue of LaunchWinPostProcess (PostUpdateWin).
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSString* iniPath = [NSString stringWithUTF8String:aAppBundle];
  iniPath =
    [iniPath stringByAppendingPathComponent:@"Contents/Resources/updater.ini"];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:iniPath]) {
    // the file does not exist; there is nothing to run
    [pool release];
    return;
  }

  int readResult;
  char values[2][MAX_TEXT_LEN];
  readResult = ReadStrings([iniPath UTF8String],
                           "ExeRelPath\0ExeArg\0",
                           2,
                           values,
                           "PostUpdateMac");
  if (readResult) {
    [pool release];
    return;
  }

  NSString *exeRelPath = [NSString stringWithUTF8String:values[0]];
  NSString *exeArg = [NSString stringWithUTF8String:values[1]];
  if (!exeArg || !exeRelPath) {
    [pool release];
    return;
  }

  NSString* exeFullPath = [NSString stringWithUTF8String:aAppBundle];
  exeFullPath = [exeFullPath stringByAppendingPathComponent:exeRelPath];

  char optVals[1][MAX_TEXT_LEN];
  readResult = ReadStrings([iniPath UTF8String],
                           "ExeAsync\0",
                           1,
                           optVals,
                           "PostUpdateMac");

  NSTask *task = [[NSTask alloc] init];
  [task setLaunchPath:exeFullPath];
  [task setArguments:[NSArray arrayWithObject:exeArg]];
  [task launch];
  if (!readResult) {
    NSString *exeAsync = [NSString stringWithUTF8String:optVals[0]];
    if ([exeAsync isEqualToString:@"false"]) {
      [task waitUntilExit];
    }
  }
  // ignore the return value of the task, there's nothing we can do with it
  [task release];

  [pool release];
}
