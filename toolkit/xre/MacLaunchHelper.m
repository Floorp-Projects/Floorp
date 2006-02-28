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

#include "MacLaunchHelper.h"

#include <Cocoa/Cocoa.h>

#ifdef __ppc__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/machine.h>
#endif /* __ppc__ */

void LaunchChildMac(int aArgc, char** aArgv)
{
  int i;
  NSTask* child = [[NSTask alloc] init];
  NSMutableArray* args = [[NSMutableArray alloc] init];
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

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

  for (i = 1; i < aArgc; ++i) 
    [args addObject: [NSString stringWithCString: aArgv[i]]];
  
  [child setCurrentDirectoryPath:[[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent]];
  [child setLaunchPath:[[NSBundle mainBundle] executablePath]];
  [child setArguments:args];
  [child launch];
  [pool release];
}

