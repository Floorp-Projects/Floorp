// SPDX-License-Identifier: MPL-2.0

#include "ShimApplication.h"
#include "ColdLaunch.h"

#include <cstdlib>
#include <cstring>

int main(int argc, char* argv[]) {
  @autoreleasepool {
    if (argc == 1) return floorp::shim::LaunchVerifiedHost(NSBundle.mainBundle.infoDictionary);
    if (argc != 7 || std::strcmp(argv[1], "--host-service") || std::strcmp(argv[3], "--host-pid") ||
        std::strcmp(argv[5], "--launch-token")) {
      fprintf(stderr, "App Shim requires --host-service SERVICE --host-pid PID --launch-token TOKEN.\n");
      return 2;
    }
    char* end = nullptr;
    long parsedPID = std::strtol(argv[4], &end, 10);
    if (!end || *end || parsedPID <= 0 || parsedPID > INT32_MAX) return 2;
    NSDictionary* info = NSBundle.mainBundle.infoDictionary;
    using namespace floorp::shim;
    NSString* hostIdentifier = ReadString(info, @"FloorpAppShimHostBundleIdentifier", 256);
    NSString* requirement = ReadString(info, @"FloorpAppShimHostCodeRequirement", 8192);
    PeerVerifier verifier = SignedHostVerifier(static_cast<pid_t>(parsedPID), hostIdentifier, requirement);
    if (!verifier) {
      fprintf(stderr, "App Shim refused startup: signed bundle and pinned host identity are required.\n");
      return 1;
    }
    AppIdentity identity;
    identity.appID = ReadString(info, @"FloorpAppShimAppId", 128);
    identity.profileID = ReadString(info, @"FloorpAppShimProfileId", 128);
    identity.displayName = ReadString(info, @"CFBundleDisplayName", 256);
    identity.bundleIdentifier = NSBundle.mainBundle.bundleIdentifier;
    identity.launchToken = [NSString stringWithUTF8String:argv[6]];
    if (identity.launchToken.length != 64 || [identity.launchToken rangeOfCharacterFromSet:
        [NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"].invertedSet].location != NSNotFound) return 2;
    Port host = Port::Lookup([NSString stringWithUTF8String:argv[2]]);
    if (!host) {
      fprintf(stderr, "App Shim could not connect to its registered browser host.\n");
      return 1;
    }
    return RunApplication(std::move(identity), std::move(host), static_cast<pid_t>(parsedPID), std::move(verifier));
  }
}
