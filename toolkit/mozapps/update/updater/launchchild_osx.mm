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

#ifdef __ppc__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/machine.h>
#endif /* __ppc__ */

#include "readstrings.h"

void LaunchChild(int argc, char **argv)
{
  int i;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSTask *child = [[[NSTask alloc] init] autorelease];
  NSMutableArray *args = [[[NSMutableArray alloc] init] autorelease];

#ifdef __ppc__
  // It's possible that the app is a universal binary running under Rosetta
  // translation because the user forced it to.  Relaunching via NSTask would
  // launch the app natively, which the user apparently doesn't want.
  // In that case, try to preserve translation.

  // If the sysctl doesn't exist, it's because Rosetta doesn't exist,
  // so don't try to force translation.  In case of other errors, just assume
  // that the app is native.

  int isNative = 0;
  size_t sz = sizeof(isNative);

  if (sysctlbyname("sysctl.proc_native", &isNative, &sz, NULL, 0) == 0 &&
      !isNative) {
    // Running translated on ppc.

    cpu_type_t preferredCPU = CPU_TYPE_POWERPC;
    sysctlbyname("sysctl.proc_exec_affinity", NULL, NULL,
                 &preferredCPU, sizeof(preferredCPU));

    // Nothing can be done to handle failure, relaunch anyway.
  }
#endif /* __ppc__ */

  for (i = 1; i < argc; ++i)
    [args addObject: [NSString stringWithCString: argv[i]]];

  [child setLaunchPath: [NSString stringWithCString: argv[0]]];
  [child setArguments: args];
  [child launch];
  [pool release];
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

