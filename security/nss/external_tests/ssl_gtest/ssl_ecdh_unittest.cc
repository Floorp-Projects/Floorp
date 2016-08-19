/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include <functional>
#include <memory>
#include "secerr.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectGenericPre13, ConnectEcdh) {
  SetExpectedVersion(std::get<1>(GetParam()));
  Reset(TlsAgent::kServerEcdhEcdsa);
  DisableAllCiphers();
  EnableSomeEcdhCiphers();

  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdh_ecdsa);
}

TEST_P(TlsConnectGenericPre13, ConnectEcdhWithoutDisablingSuites) {
  SetExpectedVersion(std::get<1>(GetParam()));
  Reset(TlsAgent::kServerEcdhEcdsa);
  EnableSomeEcdhCiphers();

  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdh_ecdsa);
}

TEST_P(TlsConnectGeneric, ConnectEcdhe) {
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
}

// If we pick a 256-bit cipher suite and use a P-384 certificate, the server
// should choose P-384 for key exchange too.  Only valid for TLS >=1.2 because
// we don't have 256-bit ciphers before then.
// TODO: Re-enable for 1.3 when Bug 1286140 lands.
TEST_P(TlsConnectTls12, ConnectEcdheP384) {
  Reset(TlsAgent::kServerEcdsa384);
  ConnectWithCipherSuite(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa, 384);
}

TEST_P(TlsConnectGeneric, ConnectEcdheP384) {
  EnsureTlsSetup();
  client_->ConfigNamedGroup(ssl_grp_ec_secp256r1, false);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
}

// This enables only P-256 on the client and disables it on the server.
// This test will fail when we add other groups that identify as ECDHE.
TEST_P(TlsConnectGeneric, ConnectEcdheGroupMismatch) {
  EnsureTlsSetup();
  client_->ConfigNamedGroup(ssl_grp_ec_secp256r1, true);
  client_->ConfigNamedGroup(ssl_grp_ec_secp384r1, false);
  client_->ConfigNamedGroup(ssl_grp_ec_secp521r1, false);
  server_->ConfigNamedGroup(ssl_grp_ec_secp256r1, false);

  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
}

// Replace the point in the client key exchange message with an empty one
class ECCClientKEXFilter : public TlsHandshakeFilter {
 public:
  ECCClientKEXFilter() {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader &header,
                                               const DataBuffer &input,
                                               DataBuffer *output) {
    if (header.handshake_type() != kTlsHandshakeClientKeyExchange) {
      return KEEP;
    }

    // Replace the client key exchange message with an empty point
    output->Allocate(1);
    output->Write(0, 0U, 1);  // set point length 0
    return CHANGE;
  }
};

// Replace the point in the server key exchange message with an empty one
class ECCServerKEXFilter : public TlsHandshakeFilter {
 public:
  ECCServerKEXFilter() {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader &header,
                                               const DataBuffer &input,
                                               DataBuffer *output) {
    if (header.handshake_type() != kTlsHandshakeServerKeyExchange) {
      return KEEP;
    }

    // Replace the server key exchange message with an empty point
    output->Allocate(4);
    output->Write(0, 3U, 1);  // named curve
    uint32_t curve;
    EXPECT_TRUE(input.Read(1, 2, &curve));  // get curve id
    output->Write(1, curve, 2);             // write curve id
    output->Write(3, 0U, 1);                // point length 0
    return CHANGE;
  }
};

TEST_P(TlsConnectGenericPre13, ConnectECDHEmptyServerPoint) {
  // add packet filter
  server_->SetPacketFilter(new ECCServerKEXFilter());
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH);
}

TEST_P(TlsConnectGenericPre13, ConnectECDHEmptyClientPoint) {
  // add packet filter
  client_->SetPacketFilter(new ECCClientKEXFilter());
  ConnectExpectFail();
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_KEY_EXCH);
}

}  // namespace nspr_test
