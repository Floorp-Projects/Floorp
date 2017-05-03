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
#include <SystemConfiguration/SystemConfiguration.h>
#include "readstrings.h"

class MacAutoreleasePool {
public:
  MacAutoreleasePool()
  {
    mPool = [[NSAutoreleasePool alloc] init];
  }
  ~MacAutoreleasePool()
  {
    [mPool release];
  }

private:
  NSAutoreleasePool* mPool;
};

void LaunchChild(int argc, const char** argv)
{
  MacAutoreleasePool pool;

  @try {
    NSString* launchPath = [NSString stringWithUTF8String:argv[0]];
    NSMutableArray* arguments = [NSMutableArray arrayWithCapacity:argc - 1];
    for (int i = 1; i < argc; i++) {
      [arguments addObject:[NSString stringWithUTF8String:argv[i]]];
    }
    [NSTask launchedTaskWithLaunchPath:launchPath
                             arguments:arguments];
  } @catch (NSException* e) {
    NSLog(@"%@: %@", e.name, e.reason);
  }
}

void
LaunchMacPostProcess(const char* aAppBundle)
{
  MacAutoreleasePool pool;

  // Launch helper to perform post processing for the update; this is the Mac
  // analogue of LaunchWinPostProcess (PostUpdateWin).
  NSString* iniPath = [NSString stringWithUTF8String:aAppBundle];
  iniPath =
    [iniPath stringByAppendingPathComponent:@"Contents/Resources/updater.ini"];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:iniPath]) {
    // the file does not exist; there is nothing to run
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
    return;
  }

  NSString *exeRelPath = [NSString stringWithUTF8String:values[0]];
  NSString *exeArg = [NSString stringWithUTF8String:values[1]];
  if (!exeArg || !exeRelPath) {
    return;
  }

  // The path must not traverse directories and it must be a relative path.
  if ([exeRelPath rangeOfString:@".."].location != NSNotFound ||
      [exeRelPath rangeOfString:@"./"].location != NSNotFound ||
      [exeRelPath rangeOfString:@"/"].location == 0) {
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
}

id ConnectToUpdateServer()
{
  MacAutoreleasePool pool;

  id updateServer = nil;
  BOOL isConnected = NO;
  int currTry = 0;
  const int numRetries = 10; // Number of IPC connection retries before
                             // giving up.
  while (!isConnected && currTry < numRetries) {
    @try {
      updateServer = (id)[NSConnection
        rootProxyForConnectionWithRegisteredName:
          @"org.mozilla.updater.server"
        host:nil
        usingNameServer:[NSSocketPortNameServer sharedInstance]];
      if (!updateServer ||
          ![updateServer respondsToSelector:@selector(abort)] ||
          ![updateServer respondsToSelector:@selector(getArguments)] ||
          ![updateServer respondsToSelector:@selector(shutdown)]) {
        NSLog(@"Server doesn't exist or doesn't provide correct selectors.");
        sleep(1); // Wait 1 second.
        currTry++;
      } else {
        isConnected = YES;
      }
    } @catch (NSException* e) {
      NSLog(@"Encountered exception, retrying: %@: %@", e.name, e.reason);
      sleep(1); // Wait 1 second.
      currTry++;
    }
  }
  if (!isConnected) {
    NSLog(@"Failed to connect to update server after several retries.");
    return nil;
  }
  return updateServer;
}

void CleanupElevatedMacUpdate(bool aFailureOccurred)
{
  MacAutoreleasePool pool;

  id updateServer = ConnectToUpdateServer();
  if (updateServer) {
    @try {
      if (aFailureOccurred) {
        [updateServer performSelector:@selector(abort)];
      } else {
        [updateServer performSelector:@selector(shutdown)];
      }
    } @catch (NSException* e) { }
  }

  NSFileManager* manager = [NSFileManager defaultManager];
  [manager removeItemAtPath:@"/Library/PrivilegedHelperTools/org.mozilla.updater"
                      error:nil];
  [manager removeItemAtPath:@"/Library/LaunchDaemons/org.mozilla.updater.plist"
                      error:nil];
  const char* launchctlArgs[] = {"/bin/launchctl",
                                 "remove",
                                 "org.mozilla.updater"};
  // The following call will terminate the current process due to the "remove"
  // argument in launchctlArgs.
  LaunchChild(3, launchctlArgs);
}

// Note: Caller is responsible for freeing argv.
bool ObtainUpdaterArguments(int* argc, char*** argv)
{
  MacAutoreleasePool pool;

  id updateServer = ConnectToUpdateServer();
  if (!updateServer) {
    // Let's try our best and clean up.
    CleanupElevatedMacUpdate(true);
    return false; // Won't actually get here due to CleanupElevatedMacUpdate.
  }

  @try {
    NSArray* updaterArguments =
      [updateServer performSelector:@selector(getArguments)];
    *argc = [updaterArguments count];
    char** tempArgv = (char**)malloc(sizeof(char*) * (*argc));
    for (int i = 0; i < *argc; i++) {
      int argLen = [[updaterArguments objectAtIndex:i] length] + 1;
      tempArgv[i] = (char*)malloc(argLen);
      strncpy(tempArgv[i], [[updaterArguments objectAtIndex:i] UTF8String],
              argLen);
    }
    *argv = tempArgv;
  } @catch (NSException* e) {
    // Let's try our best and clean up.
    CleanupElevatedMacUpdate(true);
    return false; // Won't actually get here due to CleanupElevatedMacUpdate.
  }
  return true;
}

/**
 * The ElevatedUpdateServer is launched from a non-elevated updater process.
 * It allows an elevated updater process (usually a privileged helper tool) to
 * connect to it and receive all the necessary arguments to complete a
 * successful update.
 */
@interface ElevatedUpdateServer : NSObject
{
  NSArray* mUpdaterArguments;
  BOOL mShouldKeepRunning;
  BOOL mAborted;
}
- (id)initWithArgs:(NSArray*)args;
- (BOOL)runServer;
- (NSArray*)getArguments;
- (void)abort;
- (BOOL)wasAborted;
- (void)shutdown;
- (BOOL)shouldKeepRunning;
@end

@implementation ElevatedUpdateServer

- (id)initWithArgs:(NSArray*)args
{
  self = [super init];
  if (!self) {
    return nil;
  }
  mUpdaterArguments = args;
  mShouldKeepRunning = YES;
  mAborted = NO;
  return self;
}

- (BOOL)runServer
{
  NSPort* serverPort = [NSSocketPort port];
  NSConnection* server = [NSConnection connectionWithReceivePort:serverPort
                                                        sendPort:serverPort];
  [server setRootObject:self];
  if ([server registerName:@"org.mozilla.updater.server"
            withNameServer:[NSSocketPortNameServer sharedInstance]] == NO) {
    NSLog(@"Unable to register as DirectoryServer.");
    NSLog(@"Is another copy running?");
    return NO;
  }

  while ([self shouldKeepRunning] &&
         [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                  beforeDate:[NSDate distantFuture]]);
  return ![self wasAborted];
}

- (NSArray*)getArguments
{
  return mUpdaterArguments;
}

- (void)abort
{
  mAborted = YES;
  [self shutdown];
}

- (BOOL)wasAborted
{
  return mAborted;
}

- (void)shutdown
{
  mShouldKeepRunning = NO;
}

- (BOOL)shouldKeepRunning
{
  return mShouldKeepRunning;
}

@end

bool ServeElevatedUpdate(int argc, const char** argv)
{
  MacAutoreleasePool pool;

  NSMutableArray* updaterArguments = [NSMutableArray arrayWithCapacity:argc];
  for (int i = 0; i < argc; i++) {
    [updaterArguments addObject:[NSString stringWithUTF8String:argv[i]]];
  }

  ElevatedUpdateServer* updater =
    [[ElevatedUpdateServer alloc] initWithArgs:[updaterArguments copy]];
  bool didSucceed = [updater runServer];

  [updater release];
  return didSucceed;
}

bool IsOwnedByGroupAdmin(const char* aAppBundle)
{
  MacAutoreleasePool pool;

  NSString* appDir = [NSString stringWithUTF8String:aAppBundle];
  NSFileManager* fileManager = [NSFileManager defaultManager];

  NSDictionary* attributes = [fileManager attributesOfItemAtPath:appDir
                                                           error:nil];
  bool isOwnedByAdmin = false;
  if (attributes &&
      [[attributes valueForKey:NSFileGroupOwnerAccountID] intValue] == 80) {
    isOwnedByAdmin = true;
  }
  return isOwnedByAdmin;
}

void SetGroupOwnershipAndPermissions(const char* aAppBundle)
{
  MacAutoreleasePool pool;

  NSString* appDir = [NSString stringWithUTF8String:aAppBundle];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  NSArray* paths =
    [fileManager subpathsOfDirectoryAtPath:appDir
                                     error:&error];
  if (error) {
    return;
  }

  // Set group ownership of Firefox.app to 80 ("admin") and permissions to
  // 0775.
  if (![fileManager setAttributes:@{ NSFileGroupOwnerAccountID: @(80),
                                     NSFilePosixPermissions: @(0775) }
                     ofItemAtPath:appDir
                            error:&error] || error) {
    return;
  }

  NSArray* permKeys = [NSArray arrayWithObjects:NSFileGroupOwnerAccountID,
                                                NSFilePosixPermissions,
                                                nil];
  // For all descendants of Firefox.app, set group ownership to 80 ("admin") and
  // ensure write permission for the group.
  for (NSString* currPath in paths) {
    NSString* child = [appDir stringByAppendingPathComponent:currPath];
    NSDictionary* oldAttributes =
      [fileManager attributesOfItemAtPath:child
                                    error:&error];
    if (error) {
      return;
    }
    // Skip symlinks, since they could be pointing to files outside of the .app
    // bundle.
    if ([oldAttributes fileType] == NSFileTypeSymbolicLink) {
      continue;
    }
    NSNumber* oldPerms =
      (NSNumber*)[oldAttributes valueForKey:NSFilePosixPermissions];
    NSArray* permObjects =
      [NSArray arrayWithObjects:
        [NSNumber numberWithUnsignedLong:80],
        [NSNumber numberWithUnsignedLong:[oldPerms shortValue] | 020],
        nil];
    NSDictionary* attributes = [NSDictionary dictionaryWithObjects:permObjects
                                                           forKeys:permKeys];
    if (![fileManager setAttributes:attributes
                       ofItemAtPath:child
                              error:&error] || error) {
      return;
    }
  }
}
