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
#include <sys/types.h>
#include <sys/sysctl.h>
#include "readstrings.h"

#define ARCH_PATH "/usr/bin/arch"
#if defined(__x86_64__)
// Work around the fact that this constant is not available in the macOS SDK
#  define kCFBundleExecutableArchitectureARM64 0x0100000c
#endif

class MacAutoreleasePool {
 public:
  MacAutoreleasePool() { mPool = [[NSAutoreleasePool alloc] init]; }
  ~MacAutoreleasePool() { [mPool release]; }

 private:
  NSAutoreleasePool* mPool;
};

#if defined(__x86_64__)
/*
 * Returns true if the process is running under Rosetta translation. Returns
 * false if running natively or if an error was encountered. We use the
 * `sysctl.proc_translated` sysctl which is documented by Apple to be used
 * for this purpose.
 */
bool IsProcessRosettaTranslated() {
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1) {
    if (errno != ENOENT) {
      fprintf(stderr, "Failed to check for translation environment\n");
    }
    return false;
  }
  return (ret == 1);
}

// Returns true if the binary at |executablePath| can be executed natively
// on an arm64 Mac. Returns false otherwise or if an error occurred.
bool IsBinaryArmExecutable(const char* executablePath) {
  bool isArmExecutable = false;

  CFURLRef url = ::CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8*)executablePath, strlen(executablePath), false);
  if (!url) {
    return false;
  }

  CFArrayRef archs = ::CFBundleCopyExecutableArchitecturesForURL(url);
  if (!archs) {
    CFRelease(url);
    return false;
  }

  CFIndex archCount = ::CFArrayGetCount(archs);
  for (CFIndex i = 0; i < archCount; i++) {
    CFNumberRef currentArch = static_cast<CFNumberRef>(::CFArrayGetValueAtIndex(archs, i));
    int currentArchInt = 0;
    if (!::CFNumberGetValue(currentArch, kCFNumberIntType, &currentArchInt)) {
      continue;
    }
    if (currentArchInt == kCFBundleExecutableArchitectureARM64) {
      isArmExecutable = true;
      break;
    }
  }

  CFRelease(url);
  CFRelease(archs);

  return isArmExecutable;
}

// Returns true if the executable provided in |executablePath| should be
// launched with a preference for arm64. After updating from an x64 version
// running under Rosetta, if the update is to a universal binary with arm64
// support we want to switch to arm64 execution mode. Returns true if those
// conditions are met and the arch(1) utility at |archPath| is executable.
// It should be safe to always launch with arch and fallback to x64, but we
// limit its use to the only scenario it is necessary to minimize risk.
bool ShouldPreferArmLaunch(const char* archPath, const char* executablePath) {
  // If not running under Rosetta, we are not on arm64 hardware.
  if (!IsProcessRosettaTranslated()) {
    return false;
  }

  // Ensure the arch(1) utility is present and executable.
  NSFileManager* fileMgr = [NSFileManager defaultManager];
  NSString* archPathString = [NSString stringWithUTF8String:archPath];
  if (![fileMgr isExecutableFileAtPath:archPathString]) {
    return false;
  }

  // Ensure the binary can be run natively on arm64.
  return IsBinaryArmExecutable(executablePath);
}
#endif  // __x86_64__

void LaunchChild(int argc, const char** argv) {
  MacAutoreleasePool pool;

  @try {
    bool preferArmLaunch = false;

#if defined(__x86_64__)
    // When running under Rosetta, child processes inherit the architecture
    // preference of their parent and therefore universal binaries launched
    // by an emulated x64 process will launch in x64 mode. If we are running
    // under Rosetta, launch the child process with a preference for arm64 so
    // that we will switch to arm64 execution if we have just updated from
    // x64 to a universal build. This includes if we were already a universal
    // build and the user is intentionally running under Rosetta.
    preferArmLaunch = ShouldPreferArmLaunch(ARCH_PATH, argv[0]);
#endif  // __x86_64__

    NSString* launchPath;
    NSMutableArray* arguments;

    if (preferArmLaunch) {
      launchPath = [NSString stringWithUTF8String:ARCH_PATH];

      // Size the arguments array to include all the arguments
      // in |argv| plus two arguments to pass to the arch(1) utility.
      arguments = [NSMutableArray arrayWithCapacity:argc + 2];
      [arguments addObject:[NSString stringWithUTF8String:"-arm64"]];
      [arguments addObject:[NSString stringWithUTF8String:"-x86_64"]];

      // Add the first argument from |argv|. The rest are added below.
      [arguments addObject:[NSString stringWithUTF8String:argv[0]]];
    } else {
      launchPath = [NSString stringWithUTF8String:argv[0]];
      arguments = [NSMutableArray arrayWithCapacity:argc - 1];
    }

    for (int i = 1; i < argc; i++) {
      [arguments addObject:[NSString stringWithUTF8String:argv[i]]];
    }
    [NSTask launchedTaskWithLaunchPath:launchPath arguments:arguments];
  } @catch (NSException* e) {
    NSLog(@"%@: %@", e.name, e.reason);
  }
}

void LaunchMacPostProcess(const char* aAppBundle) {
  MacAutoreleasePool pool;

  // Launch helper to perform post processing for the update; this is the Mac
  // analogue of LaunchWinPostProcess (PostUpdateWin).
  NSString* iniPath = [NSString stringWithUTF8String:aAppBundle];
  iniPath = [iniPath stringByAppendingPathComponent:@"Contents/Resources/updater.ini"];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:iniPath]) {
    // the file does not exist; there is nothing to run
    return;
  }

  int readResult;
  mozilla::UniquePtr<char[]> values[2];
  readResult =
      ReadStrings([iniPath UTF8String], "ExeRelPath\0ExeArg\0", 2, values, "PostUpdateMac");
  if (readResult) {
    return;
  }

  NSString* exeRelPath = [NSString stringWithUTF8String:values[0].get()];
  NSString* exeArg = [NSString stringWithUTF8String:values[1].get()];
  if (!exeArg || !exeRelPath) {
    return;
  }

  // The path must not traverse directories and it must be a relative path.
  if ([exeRelPath isEqualToString:@".."] || [exeRelPath hasPrefix:@"/"] ||
      [exeRelPath hasPrefix:@"../"] || [exeRelPath hasSuffix:@"/.."] ||
      [exeRelPath containsString:@"/../"]) {
    return;
  }

  NSString* exeFullPath = [NSString stringWithUTF8String:aAppBundle];
  exeFullPath = [exeFullPath stringByAppendingPathComponent:exeRelPath];

  mozilla::UniquePtr<char[]> optVal;
  readResult = ReadStrings([iniPath UTF8String], "ExeAsync\0", 1, &optVal, "PostUpdateMac");

  NSTask* task = [[NSTask alloc] init];
  [task setLaunchPath:exeFullPath];
  [task setArguments:[NSArray arrayWithObject:exeArg]];
  [task launch];
  if (!readResult) {
    NSString* exeAsync = [NSString stringWithUTF8String:optVal.get()];
    if ([exeAsync isEqualToString:@"false"]) {
      [task waitUntilExit];
    }
  }
  // ignore the return value of the task, there's nothing we can do with it
  [task release];
}

id ConnectToUpdateServer() {
  MacAutoreleasePool pool;

  id updateServer = nil;
  BOOL isConnected = NO;
  int currTry = 0;
  const int numRetries = 10;  // Number of IPC connection retries before
                              // giving up.
  while (!isConnected && currTry < numRetries) {
    @try {
      updateServer = (id)[NSConnection
          rootProxyForConnectionWithRegisteredName:@"org.mozilla.updater.server"
                                              host:nil
                                   usingNameServer:[NSSocketPortNameServer sharedInstance]];
      if (!updateServer || ![updateServer respondsToSelector:@selector(abort)] ||
          ![updateServer respondsToSelector:@selector(getArguments)] ||
          ![updateServer respondsToSelector:@selector(shutdown)]) {
        NSLog(@"Server doesn't exist or doesn't provide correct selectors.");
        sleep(1);  // Wait 1 second.
        currTry++;
      } else {
        isConnected = YES;
      }
    } @catch (NSException* e) {
      NSLog(@"Encountered exception, retrying: %@: %@", e.name, e.reason);
      sleep(1);  // Wait 1 second.
      currTry++;
    }
  }
  if (!isConnected) {
    NSLog(@"Failed to connect to update server after several retries.");
    return nil;
  }
  return updateServer;
}

void CleanupElevatedMacUpdate(bool aFailureOccurred) {
  MacAutoreleasePool pool;

  id updateServer = ConnectToUpdateServer();
  if (updateServer) {
    @try {
      if (aFailureOccurred) {
        [updateServer performSelector:@selector(abort)];
      } else {
        [updateServer performSelector:@selector(shutdown)];
      }
    } @catch (NSException* e) {
    }
  }

  NSFileManager* manager = [NSFileManager defaultManager];
  [manager removeItemAtPath:@"/Library/PrivilegedHelperTools/org.mozilla.updater" error:nil];
  [manager removeItemAtPath:@"/Library/LaunchDaemons/org.mozilla.updater.plist" error:nil];
  const char* launchctlArgs[] = {"/bin/launchctl", "remove", "org.mozilla.updater"};
  // The following call will terminate the current process due to the "remove"
  // argument in launchctlArgs.
  LaunchChild(3, launchctlArgs);
}

// Note: Caller is responsible for freeing argv.
bool ObtainUpdaterArguments(int* argc, char*** argv) {
  MacAutoreleasePool pool;

  id updateServer = ConnectToUpdateServer();
  if (!updateServer) {
    // Let's try our best and clean up.
    CleanupElevatedMacUpdate(true);
    return false;  // Won't actually get here due to CleanupElevatedMacUpdate.
  }

  @try {
    NSArray* updaterArguments = [updateServer performSelector:@selector(getArguments)];
    *argc = [updaterArguments count];
    char** tempArgv = (char**)malloc(sizeof(char*) * (*argc));
    for (int i = 0; i < *argc; i++) {
      int argLen = [[updaterArguments objectAtIndex:i] length] + 1;
      tempArgv[i] = (char*)malloc(argLen);
      strncpy(tempArgv[i], [[updaterArguments objectAtIndex:i] UTF8String], argLen);
    }
    *argv = tempArgv;
  } @catch (NSException* e) {
    // Let's try our best and clean up.
    CleanupElevatedMacUpdate(true);
    return false;  // Won't actually get here due to CleanupElevatedMacUpdate.
  }
  return true;
}

/**
 * The ElevatedUpdateServer is launched from a non-elevated updater process.
 * It allows an elevated updater process (usually a privileged helper tool) to
 * connect to it and receive all the necessary arguments to complete a
 * successful update.
 */
@interface ElevatedUpdateServer : NSObject {
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

- (id)initWithArgs:(NSArray*)args {
  self = [super init];
  if (!self) {
    return nil;
  }
  mUpdaterArguments = args;
  mShouldKeepRunning = YES;
  mAborted = NO;
  return self;
}

- (BOOL)runServer {
  NSPort* serverPort = [NSSocketPort port];
  NSConnection* server = [NSConnection connectionWithReceivePort:serverPort sendPort:serverPort];
  [server setRootObject:self];
  if ([server registerName:@"org.mozilla.updater.server"
            withNameServer:[NSSocketPortNameServer sharedInstance]] == NO) {
    NSLog(@"Unable to register as DirectoryServer.");
    NSLog(@"Is another copy running?");
    return NO;
  }

  while ([self shouldKeepRunning] && [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                                              beforeDate:[NSDate distantFuture]])
    ;
  return ![self wasAborted];
}

- (NSArray*)getArguments {
  return mUpdaterArguments;
}

- (void)abort {
  mAborted = YES;
  [self shutdown];
}

- (BOOL)wasAborted {
  return mAborted;
}

- (void)shutdown {
  mShouldKeepRunning = NO;
}

- (BOOL)shouldKeepRunning {
  return mShouldKeepRunning;
}

@end

bool ServeElevatedUpdate(int argc, const char** argv) {
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

bool IsOwnedByGroupAdmin(const char* aAppBundle) {
  MacAutoreleasePool pool;

  NSString* appDir = [NSString stringWithUTF8String:aAppBundle];
  NSFileManager* fileManager = [NSFileManager defaultManager];

  NSDictionary* attributes = [fileManager attributesOfItemAtPath:appDir error:nil];
  bool isOwnedByAdmin = false;
  if (attributes && [[attributes valueForKey:NSFileGroupOwnerAccountID] intValue] == 80) {
    isOwnedByAdmin = true;
  }
  return isOwnedByAdmin;
}

void SetGroupOwnershipAndPermissions(const char* aAppBundle) {
  MacAutoreleasePool pool;

  NSString* appDir = [NSString stringWithUTF8String:aAppBundle];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  NSArray* paths = [fileManager subpathsOfDirectoryAtPath:appDir error:&error];
  if (error) {
    return;
  }

  // Set group ownership of Firefox.app to 80 ("admin") and permissions to
  // 0775.
  if (![fileManager setAttributes:@{
        NSFileGroupOwnerAccountID : @(80),
        NSFilePosixPermissions : @(0775)
      }
                     ofItemAtPath:appDir
                            error:&error] ||
      error) {
    return;
  }

  NSArray* permKeys =
      [NSArray arrayWithObjects:NSFileGroupOwnerAccountID, NSFilePosixPermissions, nil];
  // For all descendants of Firefox.app, set group ownership to 80 ("admin") and
  // ensure write permission for the group.
  for (NSString* currPath in paths) {
    NSString* child = [appDir stringByAppendingPathComponent:currPath];
    NSDictionary* oldAttributes = [fileManager attributesOfItemAtPath:child error:&error];
    if (error) {
      return;
    }
    // Skip symlinks, since they could be pointing to files outside of the .app
    // bundle.
    if ([oldAttributes fileType] == NSFileTypeSymbolicLink) {
      continue;
    }
    NSNumber* oldPerms = (NSNumber*)[oldAttributes valueForKey:NSFilePosixPermissions];
    NSArray* permObjects = [NSArray
        arrayWithObjects:[NSNumber numberWithUnsignedLong:80],
                         [NSNumber numberWithUnsignedLong:[oldPerms shortValue] | 020], nil];
    NSDictionary* attributes = [NSDictionary dictionaryWithObjects:permObjects forKeys:permKeys];
    if (![fileManager setAttributes:attributes ofItemAtPath:child error:&error] || error) {
      return;
    }
  }
}

#if !defined(MAC_OS_X_VERSION_10_13) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13
@interface NSTask (NSTask10_13)
@property(copy) NSURL* executableURL NS_AVAILABLE_MAC(10_13);
@property(copy) NSArray<NSString*>* arguments;
- (BOOL)launchAndReturnError:(NSError**)error NS_AVAILABLE_MAC(10_13);
@end
#endif

/**
 * Helper to launch macOS tasks via NSTask.
 */
static void LaunchTask(NSString* aPath, NSArray* aArguments) {
  if (@available(macOS 10.13, *)) {
    NSTask* task = [[NSTask alloc] init];
    [task setExecutableURL:[NSURL fileURLWithPath:aPath]];
    if (aArguments) {
      [task setArguments:aArguments];
    }
    [task launchAndReturnError:nil];
    [task release];
  } else {
    NSArray* arguments = aArguments;
    if (!arguments) {
      arguments = @[];
    }
    [NSTask launchedTaskWithLaunchPath:aPath arguments:arguments];
  }
}

static void RegisterAppWithLaunchServices(NSString* aBundlePath) {
  NSArray* arguments = @[ @"-f", aBundlePath ];
  LaunchTask(@"/System/Library/Frameworks/CoreServices.framework/Frameworks/"
             @"LaunchServices.framework/Support/lsregister",
             arguments);
}

static void StripQuarantineBit(NSString* aBundlePath) {
  NSArray* arguments = @[ @"-d", @"com.apple.quarantine", aBundlePath ];
  LaunchTask(@"/usr/bin/xattr", arguments);
}

bool PerformInstallationFromDMG(int argc, char** argv) {
  MacAutoreleasePool pool;
  if (argc < 4) {
    return false;
  }
  NSString* bundlePath = [NSString stringWithUTF8String:argv[2]];
  NSString* destPath = [NSString stringWithUTF8String:argv[3]];
  if ([[NSFileManager defaultManager] copyItemAtPath:bundlePath toPath:destPath error:nil]) {
    RegisterAppWithLaunchServices(destPath);
    StripQuarantineBit(destPath);
    return true;
  }
  return false;
}
