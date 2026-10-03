// SPDX-License-Identifier: MPL-2.0

#include "ColdLaunch.h"
#include "Protocol.h"

#import <Cocoa/Cocoa.h>
#import <Security/Security.h>
#include <spawn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>

extern char** environ;

namespace floorp::shim {

int LaunchVerifiedHost(NSDictionary* sealedInfo) {
  SecCodeRef self = nullptr;
  if (SecCodeCopySelf(kSecCSDefaultFlags, &self) != errSecSuccess) return 1;
  bool selfValid = SecCodeCheckValidity(self, kSecCSStrictValidate, nullptr) == errSecSuccess;
  CFRelease(self);
  if (!selfValid) return 1;
  NSString* hostPath = ReadString(sealedInfo, @"FloorpAppShimHostPath", 4096);
  NSString* profilePath = ReadString(sealedInfo, @"FloorpAppShimProfilePath", 4096);
  NSString* hostIdentifier = ReadString(sealedInfo, @"FloorpAppShimHostBundleIdentifier", 256);
  NSString* requirementText = ReadString(sealedInfo, @"FloorpAppShimHostCodeRequirement", 8192);
  NSString* appID = ReadString(sealedInfo, @"FloorpAppShimAppId", 128);
  BOOL isDirectory = NO;
  if (!hostPath.isAbsolutePath || !profilePath.isAbsolutePath || !hostIdentifier.length ||
      ![hostPath.pathExtension isEqual:@"app"] || !requirementText.length || !ValidIdentity(appID) ||
      ![NSFileManager.defaultManager fileExistsAtPath:profilePath isDirectory:&isDirectory] || !isDirectory) {
    fprintf(stderr, "App Shim cannot locate its existing browser profile or sealed host identity.\n");
    return 1;
  }
  NSURL* hostURL = [NSURL fileURLWithPath:hostPath isDirectory:YES];
  SecStaticCodeRef host = nullptr;
  SecRequirementRef requirement = nullptr;
  bool hostValid = SecStaticCodeCreateWithPath((__bridge CFURLRef)hostURL, kSecCSDefaultFlags, &host) == errSecSuccess &&
      SecRequirementCreateWithString((__bridge CFStringRef)requirementText, kSecCSDefaultFlags, &requirement) == errSecSuccess &&
      SecStaticCodeCheckValidity(host, kSecCSStrictValidate | kSecCSCheckAllArchitectures, requirement) == errSecSuccess;
  CFDictionaryRef signing = nullptr;
  if (hostValid) {
    hostValid = SecCodeCopySigningInformation(host, kSecCSSigningInformation, &signing) == errSecSuccess;
    if (hostValid) {
      NSDictionary* info = (__bridge NSDictionary*)signing;
      hostValid = [info[(__bridge NSString*)kSecCodeInfoIdentifier] isEqual:hostIdentifier];
    }
  }
  if (signing) CFRelease(signing);
  if (requirement) CFRelease(requirement);
  if (host) CFRelease(host);
  if (!hostValid) {
    fprintf(stderr, "App Shim refused to launch a browser that does not match its signed host identity.\n");
    return 1;
  }
  NSString* executable = [NSBundle bundleWithURL:hostURL].executablePath;
  NSString* canonicalRoot = hostPath.stringByResolvingSymlinksInPath;
  NSString* expectedPrefix = [canonicalRoot stringByAppendingString:@"/Contents/MacOS/"];
  NSString* canonicalExecutable = executable.stringByResolvingSymlinksInPath;
  struct stat status;
  if (!executable.isAbsolutePath || ![canonicalExecutable hasPrefix:expectedPrefix] ||
      lstat(executable.fileSystemRepresentation, &status) != 0 || !S_ISREG(status.st_mode) ||
      access(executable.fileSystemRepresentation, X_OK) != 0) {
    fprintf(stderr, "App Shim refused an executable outside its verified browser bundle.\n");
    return 1;
  }
  std::vector<std::string> arguments = {executable.fileSystemRepresentation,
      "--profile", profilePath.fileSystemRepresentation, "--start-ssb", appID.UTF8String};
  std::vector<char*> argv;
  for (std::string& argument : arguments) argv.push_back(argument.data());
  argv.push_back(nullptr);
  std::vector<char*> environment;
  // Gecko consults restart/reset profile variables before --profile. A Finder
  // launch must use the profile sealed into this app, regardless of its parent.
  const char* excluded[] = {"MOZ_NO_REMOTE=", "MOZ_NEW_INSTANCE=",
      "XRE_PROFILE_PATH=", "XRE_PROFILE_LOCAL_PATH=",
      "SELECTABLE_PROFILE_RESET_PATH=", "SELECTABLE_PROFILE_RESET_STORE_ID="};
  for (char** entry = environ; entry && *entry; ++entry) {
    bool omit = false;
    for (const char* prefix : excluded) {
      if (!std::strncmp(*entry, prefix, std::strlen(prefix))) {
        omit = true;
        break;
      }
    }
    if (!omit) environment.push_back(*entry);
  }
  environment.push_back(nullptr);
  pid_t child = 0;
  int result = posix_spawn(&child, executable.fileSystemRepresentation, nullptr, nullptr,
                           argv.data(), environment.data());
  if (result) fprintf(stderr, "App Shim could not start its verified browser host: %d.\n", result);
  return result ? 1 : 0;
}

}  // namespace floorp::shim
