/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "MacLaunchHelper.h"

#include <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>
#include <sys/utsname.h>

@interface TaskMonitor : NSObject
-(void)prebindFinished:(NSNotification *)aNotification;
@end

@implementation TaskMonitor
-(void)prebindFinished:(NSNotification *)aNotification
{
  /* Delete the task and the TaskMonitor */
  [[aNotification object] release];
  [self release];
}
@end

void
UpdatePrebinding()
{
#ifdef _BUILD_STATIC_BIN
  struct utsname u;
  uname(&u);

  // We run the redo-prebinding script in these cases:
  // 10.1.x (5.x): No auto-update of prebinding exists
  // On 10.3.x, prebinding fails to update automatically but this script
  // doesn't work either.  It doesn't matter though, because in 10.3.4 and
  // higher, the loader is improved so that prebinding is unnecessary.
  if (u.release[0] != '5')
    return;

  if (!_dyld_launched_prebound()) {
    NSTask *task;
    NSArray *args;
    TaskMonitor *monitor;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"Not prebound, launching update script");
    task = [[NSTask alloc] init];
    args = [NSArray arrayWithObject: @"redo-prebinding.sh"];

    [task setCurrentDirectoryPath:[[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent]];
    [task setLaunchPath:@"/bin/sh"];
    [task setArguments:args];

    monitor = [[TaskMonitor alloc] init];

    [[NSNotificationCenter defaultCenter] addObserver:monitor
     selector:@selector(prebindFinished:)
     name:NSTaskDidTerminateNotification
     object:nil];

    [task launch];
    [pool release];
  }
#endif
}

void LaunchChildMac(int aArgc, char** aArgv)
{
  int i;
  NSTask* child = [[NSTask alloc] init];
  NSMutableArray* args = [[NSMutableArray alloc] init];
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  for (i = 1; i < aArgc; ++i) 
    [args addObject: [NSString stringWithCString: aArgv[i]]];
  
  [child setCurrentDirectoryPath:[[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent]];
  [child setLaunchPath:[[NSBundle mainBundle] executablePath]];
  [child setArguments:args];
  [child launch];
  [pool release];
}

