// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "MachTransport.h"
#include "PeerIdentity.h"

namespace floorp::shim {

class SessionGate {
 public:
  SessionGate(NSString* appID, NSString* profileID, NSString* nonce, PeerVerifier verifier);
  bool Accept(const Message& message);
  bool active() const { return active_; }

 private:
  __strong NSString* appID_;
  __strong NSString* profileID_;
  __strong NSString* nonce_;
  PeerVerifier verifier_;
  uint64_t lastSequence_ = 0;
  bool active_ = false;
};

NSString* NewNonce();

}  // namespace floorp::shim
