/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacLaunchHelper.h"

#include "MacAutoreleasePool.h"
#include "mozilla/UniquePtr.h"
#include "nsMemory.h"

#include <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <ServiceManagement/ServiceManagement.h>
#include <Security/Authorization.h>
#include <spawn.h>
#include <stdio.h>

using namespace mozilla;

void LaunchChildMac(int aArgc, char** aArgv, pid_t* aPid) {
  MacAutoreleasePool pool;

  @try {
    NSString* launchPath = [NSString stringWithUTF8String:aArgv[0]];
    NSMutableArray* arguments = [NSMutableArray arrayWithCapacity:aArgc - 1];
    for (int i = 1; i < aArgc; i++) {
      [arguments addObject:[NSString stringWithUTF8String:aArgv[i]]];
    }
    NSTask* child = [NSTask launchedTaskWithLaunchPath:launchPath arguments:arguments];
    if (aPid) {
      *aPid = [child processIdentifier];
      // We used to use waitpid to wait for the process to terminate. This is
      // incompatible with NSTask and we wait for the process to exit here
      // instead.
      [child waitUntilExit];
    }
  } @catch (NSException* e) {
    NSLog(@"%@: %@", e.name, e.reason);
  }
}

BOOL InstallPrivilegedHelper() {
  AuthorizationRef authRef = NULL;
  OSStatus status = AuthorizationCreate(
      NULL, kAuthorizationEmptyEnvironment,
      kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed, &authRef);
  if (status != errAuthorizationSuccess) {
    // AuthorizationCreate really shouldn't fail.
    NSLog(@"AuthorizationCreate failed! NSOSStatusErrorDomain / %d", (int)status);
    return NO;
  }

  BOOL result = NO;
  AuthorizationItem authItem = {kSMRightBlessPrivilegedHelper, 0, NULL, 0};
  AuthorizationRights authRights = {1, &authItem};
  AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed |
                             kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;

  // Obtain the right to install our privileged helper tool.
  status =
      AuthorizationCopyRights(authRef, &authRights, kAuthorizationEmptyEnvironment, flags, NULL);
  if (status != errAuthorizationSuccess) {
    NSLog(@"AuthorizationCopyRights failed! NSOSStatusErrorDomain / %d", (int)status);
  } else {
    CFErrorRef cfError;
    // This does all the work of verifying the helper tool against the
    // application and vice-versa. Once verification has passed, the embedded
    // launchd.plist is extracted and placed in /Library/LaunchDaemons and then
    // loaded. The executable is placed in /Library/PrivilegedHelperTools.
    result = (BOOL)SMJobBless(kSMDomainSystemLaunchd, (CFStringRef) @"org.mozilla.updater", authRef,
                              &cfError);
    if (!result) {
      NSLog(@"Unable to install helper!");
      CFRelease(cfError);
    }
  }

  return result;
}

void AbortElevatedUpdate() {
  mozilla::MacAutoreleasePool pool;

  id updateServer = nil;
  int currTry = 0;
  const int numRetries = 10;  // Number of IPC connection retries before
                              // giving up.
  while (currTry < numRetries) {
    @try {
      updateServer = (id)[NSConnection
          rootProxyForConnectionWithRegisteredName:@"org.mozilla.updater.server"
                                              host:nil
                                   usingNameServer:[NSSocketPortNameServer sharedInstance]];
      if (updateServer && [updateServer respondsToSelector:@selector(abort)]) {
        [updateServer performSelector:@selector(abort)];
        return;
      }
      NSLog(@"Server doesn't exist or doesn't provide correct selectors.");
      sleep(1);  // Wait 1 second.
      currTry++;
    } @catch (NSException* e) {
      NSLog(@"Encountered exception, retrying: %@: %@", e.name, e.reason);
      sleep(1);  // Wait 1 second.
      currTry++;
    }
  }
  NSLog(@"Unable to clean up updater.");
}

bool LaunchElevatedUpdate(int aArgc, char** aArgv, pid_t* aPid) {
  LaunchChildMac(aArgc, aArgv, aPid);
  bool didSucceed = InstallPrivilegedHelper();
  if (!didSucceed) {
    AbortElevatedUpdate();
  }
  return didSucceed;
}
