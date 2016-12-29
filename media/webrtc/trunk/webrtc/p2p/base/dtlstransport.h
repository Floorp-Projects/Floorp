/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_DTLSTRANSPORT_H_
#define WEBRTC_P2P_BASE_DTLSTRANSPORT_H_

#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/transport.h"

namespace rtc {
class SSLIdentity;
}

namespace cricket {

class PortAllocator;

// Base should be a descendant of cricket::Transport and have a constructor
// that takes a transport name and PortAllocator.
//
// Everything in this class should be called on the worker thread.
template<class Base>
class DtlsTransport : public Base {
 public:
  DtlsTransport(const std::string& name,
                PortAllocator* allocator,
                const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
      : Base(name, allocator),
        certificate_(certificate),
        secure_role_(rtc::SSL_CLIENT),
        ssl_max_version_(rtc::SSL_PROTOCOL_DTLS_12) {}

  ~DtlsTransport() {
    Base::DestroyAllChannels();
  }

  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override {
    certificate_ = certificate;
  }
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override {
    if (!certificate_)
      return false;

    *certificate = certificate_;
    return true;
  }

  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) override {
    ssl_max_version_ = version;
    return true;
  }

  bool ApplyLocalTransportDescription(TransportChannelImpl* channel,
                                      std::string* error_desc) override {
    rtc::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();

    if (local_fp) {
      // Sanity check local fingerprint.
      if (certificate_) {
        rtc::scoped_ptr<rtc::SSLFingerprint> local_fp_tmp(
            rtc::SSLFingerprint::Create(local_fp->algorithm,
                                        certificate_->identity()));
        ASSERT(local_fp_tmp.get() != NULL);
        if (!(*local_fp_tmp == *local_fp)) {
          std::ostringstream desc;
          desc << "Local fingerprint does not match identity. Expected: ";
          desc << local_fp_tmp->ToString();
          desc << " Got: " << local_fp->ToString();
          return BadTransportDescription(desc.str(), error_desc);
        }
      } else {
        return BadTransportDescription(
            "Local fingerprint provided but no identity available.",
            error_desc);
      }
    } else {
      certificate_ = nullptr;
    }

    if (!channel->SetLocalCertificate(certificate_)) {
      return BadTransportDescription("Failed to set local identity.",
                                     error_desc);
    }

    // Apply the description in the base class.
    return Base::ApplyLocalTransportDescription(channel, error_desc);
  }

  bool NegotiateTransportDescription(ContentAction local_role,
                                     std::string* error_desc) override {
    if (!Base::local_description() || !Base::remote_description()) {
      const std::string msg = "Local and Remote description must be set before "
                              "transport descriptions are negotiated";
      return BadTransportDescription(msg, error_desc);
    }

    rtc::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();
    rtc::SSLFingerprint* remote_fp =
        Base::remote_description()->identity_fingerprint.get();

    if (remote_fp && local_fp) {
      remote_fingerprint_.reset(new rtc::SSLFingerprint(*remote_fp));

      // From RFC 4145, section-4.1, The following are the values that the
      // 'setup' attribute can take in an offer/answer exchange:
      //       Offer      Answer
      //      ________________
      //      active     passive / holdconn
      //      passive    active / holdconn
      //      actpass    active / passive / holdconn
      //      holdconn   holdconn
      //
      // Set the role that is most conformant with RFC 5763, Section 5, bullet 1
      // The endpoint MUST use the setup attribute defined in [RFC4145].
      // The endpoint that is the offerer MUST use the setup attribute
      // value of setup:actpass and be prepared to receive a client_hello
      // before it receives the answer.  The answerer MUST use either a
      // setup attribute value of setup:active or setup:passive.  Note that
      // if the answerer uses setup:passive, then the DTLS handshake will
      // not begin until the answerer is received, which adds additional
      // latency. setup:active allows the answer and the DTLS handshake to
      // occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
      // party is active MUST initiate a DTLS handshake by sending a
      // ClientHello over each flow (host/port quartet).
      // IOW - actpass and passive modes should be treated as server and
      // active as client.
      ConnectionRole local_connection_role =
          Base::local_description()->connection_role;
      ConnectionRole remote_connection_role =
          Base::remote_description()->connection_role;

      bool is_remote_server = false;
      if (local_role == CA_OFFER) {
        if (local_connection_role != CONNECTIONROLE_ACTPASS) {
          return BadTransportDescription(
              "Offerer must use actpass value for setup attribute.",
              error_desc);
        }

        if (remote_connection_role == CONNECTIONROLE_ACTIVE ||
            remote_connection_role == CONNECTIONROLE_PASSIVE ||
            remote_connection_role == CONNECTIONROLE_NONE) {
          is_remote_server = (remote_connection_role == CONNECTIONROLE_PASSIVE);
        } else {
          const std::string msg =
              "Answerer must use either active or passive value "
              "for setup attribute.";
          return BadTransportDescription(msg, error_desc);
        }
        // If remote is NONE or ACTIVE it will act as client.
      } else {
        if (remote_connection_role != CONNECTIONROLE_ACTPASS &&
            remote_connection_role != CONNECTIONROLE_NONE) {
          return BadTransportDescription(
              "Offerer must use actpass value for setup attribute.",
              error_desc);
        }

        if (local_connection_role == CONNECTIONROLE_ACTIVE ||
            local_connection_role == CONNECTIONROLE_PASSIVE) {
          is_remote_server = (local_connection_role == CONNECTIONROLE_ACTIVE);
        } else {
          const std::string msg =
              "Answerer must use either active or passive value "
              "for setup attribute.";
          return BadTransportDescription(msg, error_desc);
        }

        // If local is passive, local will act as server.
      }

      secure_role_ = is_remote_server ? rtc::SSL_CLIENT :
                                        rtc::SSL_SERVER;

    } else if (local_fp && (local_role == CA_ANSWER)) {
      return BadTransportDescription(
          "Local fingerprint supplied when caller didn't offer DTLS.",
          error_desc);
    } else {
      // We are not doing DTLS
      remote_fingerprint_.reset(new rtc::SSLFingerprint(
          "", NULL, 0));
    }

    // Now run the negotiation for the base class.
    return Base::NegotiateTransportDescription(local_role, error_desc);
  }

  DtlsTransportChannelWrapper* CreateTransportChannel(int component) override {
    DtlsTransportChannelWrapper* channel = new DtlsTransportChannelWrapper(
        this, Base::CreateTransportChannel(component));
    channel->SetSslMaxProtocolVersion(ssl_max_version_);
    return channel;
  }

  void DestroyTransportChannel(TransportChannelImpl* channel) override {
    // Kind of ugly, but this lets us do the exact inverse of the create.
    DtlsTransportChannelWrapper* dtls_channel =
        static_cast<DtlsTransportChannelWrapper*>(channel);
    TransportChannelImpl* base_channel = dtls_channel->channel();
    delete dtls_channel;
    Base::DestroyTransportChannel(base_channel);
  }

  bool GetSslRole(rtc::SSLRole* ssl_role) const override {
    ASSERT(ssl_role != NULL);
    *ssl_role = secure_role_;
    return true;
  }

 private:
  bool ApplyNegotiatedTransportDescription(TransportChannelImpl* channel,
                                           std::string* error_desc) override {
    // Set ssl role. Role must be set before fingerprint is applied, which
    // initiates DTLS setup.
    if (!channel->SetSslRole(secure_role_)) {
      return BadTransportDescription("Failed to set ssl role for the channel.",
                                     error_desc);
    }
    // Apply remote fingerprint.
    if (!channel->SetRemoteFingerprint(remote_fingerprint_->algorithm,
                                       reinterpret_cast<const uint8_t*>(
                                           remote_fingerprint_->digest.data()),
                                       remote_fingerprint_->digest.size())) {
      return BadTransportDescription("Failed to apply remote fingerprint.",
                                     error_desc);
    }
    return Base::ApplyNegotiatedTransportDescription(channel, error_desc);
  }

  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::SSLRole secure_role_;
  rtc::SSLProtocolVersion ssl_max_version_;
  rtc::scoped_ptr<rtc::SSLFingerprint> remote_fingerprint_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_DTLSTRANSPORT_H_
