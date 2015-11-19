/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/transportdescriptionfactory.h"

#include "webrtc/p2p/base/transportdescription.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagedigest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sslfingerprint.h"

namespace cricket {

static TransportProtocol kDefaultProtocol = ICEPROTO_RFC5245;

TransportDescriptionFactory::TransportDescriptionFactory()
    : protocol_(kDefaultProtocol),
      secure_(SEC_DISABLED),
      identity_(NULL) {
}

TransportDescription* TransportDescriptionFactory::CreateOffer(
    const TransportOptions& options,
    const TransportDescription* current_description) const {
  rtc::scoped_ptr<TransportDescription> desc(new TransportDescription());

  // Set the transport type depending on the selected protocol.
  if (protocol_ == ICEPROTO_RFC5245) {
    desc->transport_type = NS_JINGLE_ICE_UDP;
  } else if (protocol_ == ICEPROTO_HYBRID) {
    desc->transport_type = NS_JINGLE_ICE_UDP;
    desc->AddOption(ICE_OPTION_GICE);
  } else if (protocol_ == ICEPROTO_GOOGLE) {
    desc->transport_type = NS_GINGLE_P2P;
  }

  // Generate the ICE credentials if we don't already have them.
  if (!current_description || options.ice_restart) {
    desc->ice_ufrag = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    desc->ice_pwd = rtc::CreateRandomString(ICE_PWD_LENGTH);
  } else {
    desc->ice_ufrag = current_description->ice_ufrag;
    desc->ice_pwd = current_description->ice_pwd;
  }

  // If we are trying to establish a secure transport, add a fingerprint.
  if (secure_ == SEC_ENABLED || secure_ == SEC_REQUIRED) {
    // Fail if we can't create the fingerprint.
    // If we are the initiator set role to "actpass".
    if (!SetSecurityInfo(desc.get(), CONNECTIONROLE_ACTPASS)) {
      return NULL;
    }
  }

  return desc.release();
}

TransportDescription* TransportDescriptionFactory::CreateAnswer(
    const TransportDescription* offer,
    const TransportOptions& options,
    const TransportDescription* current_description) const {
  // A NULL offer is treated as a GICE transport description.
  // TODO(juberti): Figure out why we get NULL offers, and fix this upstream.
  rtc::scoped_ptr<TransportDescription> desc(new TransportDescription());

  // Figure out which ICE variant to negotiate; prefer RFC 5245 ICE, but fall
  // back to G-ICE if needed. Note that we never create a hybrid answer, since
  // we know what the other side can support already.
  if (offer && offer->transport_type == NS_JINGLE_ICE_UDP &&
      (protocol_ == ICEPROTO_RFC5245 || protocol_ == ICEPROTO_HYBRID)) {
    // Offer is ICE or hybrid, we support ICE or hybrid: use ICE.
    desc->transport_type = NS_JINGLE_ICE_UDP;
  } else if (offer && offer->transport_type == NS_JINGLE_ICE_UDP &&
             offer->HasOption(ICE_OPTION_GICE) &&
             protocol_ == ICEPROTO_GOOGLE) {
    desc->transport_type = NS_GINGLE_P2P;
    // Offer is hybrid, we support GICE: use GICE.
  } else if ((!offer || offer->transport_type == NS_GINGLE_P2P) &&
             (protocol_ == ICEPROTO_HYBRID || protocol_ == ICEPROTO_GOOGLE)) {
    // Offer is GICE, we support hybrid or GICE: use GICE.
    desc->transport_type = NS_GINGLE_P2P;
  } else {
    // Mismatch.
    LOG(LS_WARNING) << "Failed to create TransportDescription answer "
                       "because of incompatible transport types";
    return NULL;
  }

  // Generate the ICE credentials if we don't already have them or ice is
  // being restarted.
  if (!current_description || options.ice_restart) {
    desc->ice_ufrag = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    desc->ice_pwd = rtc::CreateRandomString(ICE_PWD_LENGTH);
  } else {
    desc->ice_ufrag = current_description->ice_ufrag;
    desc->ice_pwd = current_description->ice_pwd;
  }

  // Negotiate security params.
  if (offer && offer->identity_fingerprint.get()) {
    // The offer supports DTLS, so answer with DTLS, as long as we support it.
    if (secure_ == SEC_ENABLED || secure_ == SEC_REQUIRED) {
      // Fail if we can't create the fingerprint.
      // Setting DTLS role to active.
      ConnectionRole role = (options.prefer_passive_role) ?
          CONNECTIONROLE_PASSIVE : CONNECTIONROLE_ACTIVE;

      if (!SetSecurityInfo(desc.get(), role)) {
        return NULL;
      }
    }
  } else if (secure_ == SEC_REQUIRED) {
    // We require DTLS, but the other side didn't offer it. Fail.
    LOG(LS_WARNING) << "Failed to create TransportDescription answer "
                       "because of incompatible security settings";
    return NULL;
  }

  return desc.release();
}

bool TransportDescriptionFactory::SetSecurityInfo(
    TransportDescription* desc, ConnectionRole role) const {
  if (!identity_) {
    LOG(LS_ERROR) << "Cannot create identity digest with no identity";
    return false;
  }

  // This digest algorithm is used to produce the a=fingerprint lines in SDP.
  // RFC 4572 Section 5 requires that those lines use the same hash function as
  // the certificate's signature.
  std::string digest_alg;
  if (!identity_->certificate().GetSignatureDigestAlgorithm(&digest_alg)) {
    LOG(LS_ERROR) << "Failed to retrieve the certificate's digest algorithm";
    return false;
  }

  desc->identity_fingerprint.reset(
      rtc::SSLFingerprint::Create(digest_alg, identity_));
  if (!desc->identity_fingerprint.get()) {
    LOG(LS_ERROR) << "Failed to create identity fingerprint, alg="
                  << digest_alg;
    return false;
  }

  // Assign security role.
  desc->connection_role = role;
  return true;
}

}  // namespace cricket

