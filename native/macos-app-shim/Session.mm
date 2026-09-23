// SPDX-License-Identifier: MPL-2.0

#include "Session.h"

#import <Security/Security.h>
#include <utility>

namespace floorp::shim {

SessionGate::SessionGate(NSString* appID, NSString* profileID, NSString* nonce,
                         PeerVerifier verifier)
    : appID_([appID copy]), profileID_([profileID copy]), nonce_([nonce copy]),
      verifier_(std::move(verifier)) {}

bool SessionGate::Accept(const Message& message) {
  if (!verifier_ || !verifier_(message.auditToken) || message.header.sequence <= lastSequence_) return false;
  auto type = static_cast<MessageType>(message.header.type);
  if (!active_) {
    if (type != MessageType::HelloAccepted || message.attachment ||
        ![message.payload[@"appId"] isEqual:appID_] ||
        ![message.payload[@"profileId"] isEqual:profileID_] ||
        ![message.payload[@"nonce"] isEqual:nonce_] ||
        ![message.payload[@"accepted"] isEqual:@YES]) return false;
    active_ = true;
  } else if (type == MessageType::Hello || type == MessageType::HelloAccepted ||
             message.header.type >= static_cast<uint32_t>(MessageType::Input)) {
    return false;
  }
  lastSequence_ = message.header.sequence;
  return true;
}

NSString* NewNonce() {
  unsigned char bytes[32];
  if (SecRandomCopyBytes(kSecRandomDefault, sizeof(bytes), bytes) != errSecSuccess) return nil;
  NSMutableString* nonce = [NSMutableString stringWithCapacity:64];
  for (auto byte : bytes) [nonce appendFormat:@"%02x", byte];
  return nonce;
}

}  // namespace floorp::shim
