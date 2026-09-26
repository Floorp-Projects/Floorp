// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "MachTransport.h"
#include "PeerIdentity.h"

namespace floorp::shim {

struct AppIdentity {
  __strong NSString* appID = nil;
  __strong NSString* profileID = nil;
  __strong NSString* displayName = nil;
  __strong NSString* bundleIdentifier = nil;
  __strong NSString* launchToken = nil;
};

int RunApplication(AppIdentity identity, Port host, pid_t hostPID, PeerVerifier verifier);

}  // namespace floorp::shim
