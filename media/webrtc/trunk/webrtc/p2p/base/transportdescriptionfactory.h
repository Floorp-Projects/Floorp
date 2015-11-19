/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TRANSPORTDESCRIPTIONFACTORY_H_
#define WEBRTC_P2P_BASE_TRANSPORTDESCRIPTIONFACTORY_H_

#include "webrtc/p2p/base/transportdescription.h"

namespace rtc {
class SSLIdentity;
}

namespace cricket {

struct TransportOptions {
  TransportOptions() : ice_restart(false), prefer_passive_role(false) {}
  bool ice_restart;
  bool prefer_passive_role;
};

// Creates transport descriptions according to the supplied configuration.
// When creating answers, performs the appropriate negotiation
// of the various fields to determine the proper result.
class TransportDescriptionFactory {
 public:
  // Default ctor; use methods below to set configuration.
  TransportDescriptionFactory();
  SecurePolicy secure() const { return secure_; }
  // The identity to use when setting up DTLS.
  rtc::SSLIdentity* identity() const { return identity_; }

  // Specifies the transport protocol to be use.
  void set_protocol(TransportProtocol protocol) { protocol_ = protocol; }
  // Specifies the transport security policy to use.
  void set_secure(SecurePolicy s) { secure_ = s; }
  // Specifies the identity to use (only used when secure is not SEC_DISABLED).
  void set_identity(rtc::SSLIdentity* identity) { identity_ = identity; }

  // Creates a transport description suitable for use in an offer.
  TransportDescription* CreateOffer(const TransportOptions& options,
      const TransportDescription* current_description) const;
  // Create a transport description that is a response to an offer.
  TransportDescription* CreateAnswer(
      const TransportDescription* offer,
      const TransportOptions& options,
      const TransportDescription* current_description) const;

 private:
  bool SetSecurityInfo(TransportDescription* description,
                       ConnectionRole role) const;

  TransportProtocol protocol_;
  SecurePolicy secure_;
  rtc::SSLIdentity* identity_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORTDESCRIPTIONFACTORY_H_
