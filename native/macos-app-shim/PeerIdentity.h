// SPDX-License-Identifier: MPL-2.0

#pragma once

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <functional>

namespace floorp::shim {

using PeerVerifier = std::function<bool(const audit_token_t&)>;
PeerVerifier SignedHostVerifier(pid_t expectedPID, NSString* expectedIdentifier,
                                NSString* designatedRequirement);

}  // namespace floorp::shim
