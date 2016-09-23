/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <functional>
#include <memory>
#include "secerr.h"
#include "ssl.h"
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
// should choose P-384 for key exchange too.  Only valid for TLS == 1.2 because
// we don't have 256-bit ciphers before then and 1.3 doesn't try to couple
// DHE size to symmetric size.
TEST_P(TlsConnectTls12, ConnectEcdheP384) {
  Reset(TlsAgent::kServerEcdsa384);
  ConnectWithCipherSuite(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa, 384);
}

TEST_P(TlsConnectGeneric, ConnectEcdheP384Client) {
  EnsureTlsSetup();
  const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                             ssl_grp_ffdhe_2048};
  client_->ConfigNamedGroups(groups);
  server_->ConfigNamedGroups(groups);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
}

// This causes a HelloRetryRequest in TLS 1.3.  Earlier versions don't care.
TEST_P(TlsConnectGeneric, ConnectEcdheP384Server) {
  EnsureTlsSetup();
  auto hrr_capture =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeHelloRetryRequest);
  server_->SetPacketFilter(hrr_capture);
  const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
  EXPECT_EQ(version_ == SSL_LIBRARY_VERSION_TLS_1_3,
            hrr_capture->buffer().len() != 0);
}

// This enables only P-256 on the client and disables it on the server.
// This test will fail when we add other groups that identify as ECDHE.
TEST_P(TlsConnectGeneric, ConnectEcdheGroupMismatch) {
  EnsureTlsSetup();
  const std::vector<SSLNamedGroup> clientGroups = {ssl_grp_ec_secp256r1,
                                                   ssl_grp_ffdhe_2048};
  const std::vector<SSLNamedGroup> serverGroups = {ssl_grp_ffdhe_2048};
  client_->ConfigNamedGroups(clientGroups);
  server_->ConfigNamedGroups(serverGroups);

  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
}

TEST_P(TlsKeyExchangeTest, P384Priority) {
  // P256, P384 and P521 are enabled. Both prefer P384.
  const std::vector<SSLNamedGroup> groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1};
  EnsureKeyShareSetup();
  ConfigNamedGroups(groups);
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);

  std::vector<SSLNamedGroup> shares = {ssl_grp_ec_secp384r1};
  CheckKEXDetails(groups, shares);
}

TEST_P(TlsKeyExchangeTest, DuplicateGroupConfig) {
  const std::vector<SSLNamedGroup> groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_secp384r1, ssl_grp_ec_secp384r1,
      ssl_grp_ec_secp256r1, ssl_grp_ec_secp256r1};
  EnsureKeyShareSetup();
  ConfigNamedGroups(groups);
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);

  std::vector<SSLNamedGroup> shares = {ssl_grp_ec_secp384r1};
  std::vector<SSLNamedGroup> expectedGroups = {ssl_grp_ec_secp384r1,
                                               ssl_grp_ec_secp256r1};
  CheckKEXDetails(expectedGroups, shares);
}

TEST_P(TlsKeyExchangeTest, P384PriorityDHEnabled) {
  // P256, P384,  P521, and FFDHE2048 are enabled. Both prefer P384.
  const std::vector<SSLNamedGroup> groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ffdhe_2048, ssl_grp_ec_secp256r1,
      ssl_grp_ec_secp521r1};
  EnsureKeyShareSetup();
  ConfigNamedGroups(groups);
  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    std::vector<SSLNamedGroup> shares = {ssl_grp_ec_secp384r1};
    CheckKEXDetails(groups, shares);
  } else {
    std::vector<SSLNamedGroup> oldtlsgroups = {
        ssl_grp_ec_secp384r1, ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1};
    CheckKEXDetails(oldtlsgroups, std::vector<SSLNamedGroup>());
  }
}

TEST_P(TlsConnectGenericPre13, P384PriorityOnServer) {
  EnsureTlsSetup();
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);

  // The server prefers P384. It has to win.
  const std::vector<SSLNamedGroup> serverGroups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(serverGroups);

  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
}

TEST_P(TlsConnectGenericPre13, P384PriorityFromModelSocket) {
#ifdef NSS_ECC_MORE_THAN_SUITE_B
  // We can't run this test with a model socket and more than suite B.
  return;
#endif
  EnsureModelSockets();

  /* Both prefer P384, set on the model socket. */
  const std::vector<SSLNamedGroup> groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1,
      ssl_grp_ffdhe_2048};
  client_model_->ConfigNamedGroups(groups);
  server_model_->ConfigNamedGroups(groups);

  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
}

// If we only have a lame group, we fall back to static RSA.
TEST_P(TlsConnectGenericPre13, UseLameGroup) {
  const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp192r1};
  client_->ConfigNamedGroups(groups);
  server_->ConfigNamedGroups(groups);
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_auth_rsa_decrypt);
}

// In TLS 1.3, we can't generate the ClientHello.
TEST_P(TlsConnectTls13, UseLameGroup) {
  const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_sect283k1};
  client_->ConfigNamedGroups(groups);
  server_->ConfigNamedGroups(groups);
  client_->StartConnect();
  client_->Handshake();
#ifndef NSS_ECC_MORE_THAN_SUITE_B  // TODO: remove this guard
  client_->CheckErrorCode(SSL_ERROR_NO_CIPHERS_SUPPORTED);
#endif
}

TEST_P(TlsConnectStreamPre13, ConfiguredGroupsRenegotiate) {
  EnsureTlsSetup();
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);

  const std::vector<SSLNamedGroup> serverGroups = {ssl_grp_ec_secp256r1,
                                                   ssl_grp_ec_secp384r1};
  const std::vector<SSLNamedGroup> clientGroups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(clientGroups);
  client_->ConfigNamedGroups(serverGroups);

  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
  CheckConnected();

  // The renegotiation has to use the same preferences as the original session.
  server_->PrepareForRenegotiate();
  client_->StartRenegotiate();
  Handshake();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 384);
}

TEST_P(TlsKeyExchangeTest, Curve25519) {
  Reset(TlsAgent::kServerEcdsa256);
  const std::vector<SSLNamedGroup> groups = {
      ssl_grp_ec_curve25519, ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1};
  EnsureKeyShareSetup();
  ConfigNamedGroups(groups);
  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa, 255);
  const std::vector<SSLNamedGroup> shares = {ssl_grp_ec_curve25519};
  CheckKEXDetails(groups, shares);
}

TEST_P(TlsConnectGeneric, P256andCurve25519OnlyServer) {
  EnsureTlsSetup();
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);

  // the client sends a P256 key share while the server prefers 25519
  const std::vector<SSLNamedGroup> server_groups = {ssl_grp_ec_curve25519,
                                                    ssl_grp_ec_secp256r1};
  const std::vector<SSLNamedGroup> client_groups = {ssl_grp_ec_secp256r1,
                                                    ssl_grp_ec_curve25519};
  client_->ConfigNamedGroups(client_groups);
  server_->ConfigNamedGroups(server_groups);

  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign, 255);
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

INSTANTIATE_TEST_CASE_P(KeyExchangeTest, TlsKeyExchangeTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                                           TlsConnectTestBase::kTlsV11Plus));

}  // namespace nss_test
