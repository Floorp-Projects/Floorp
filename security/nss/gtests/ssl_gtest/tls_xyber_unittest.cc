/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsKeyExchangeTest13, Xyber768d00Supported) {
  EnsureKeyShareSetup();
  ConfigNamedGroups({ssl_grp_kem_xyber768d00});

  Connect();
  CheckKeys(ssl_kea_ecdh_hybrid, ssl_grp_kem_xyber768d00, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
}

TEST_P(TlsKeyExchangeTest, Tls12ClientXyber768d00NotSupported) {
  EnsureKeyShareSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh_hybrid);
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(
                            client_->ssl_fd(),
                            kECDHEGroups.size() + kEcdhHybridGroups.size()));

  Connect();
  std::vector<SSLNamedGroup> groups = GetGroupDetails(groups_capture_);
  for (auto group : groups) {
    EXPECT_NE(group, ssl_grp_kem_xyber768d00);
  }
}

TEST_P(TlsKeyExchangeTest13, Tls12ServerXyber768d00NotSupported) {
  if (variant_ == ssl_variant_datagram) {
    /* Bug 1874451 - reenable this test */
    return;
  }

  EnsureKeyShareSetup();

  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh_hybrid);
  client_->ConfigNamedGroups({ssl_grp_kem_xyber768d00, ssl_grp_ec_curve25519});
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  server_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  server_->EnableCiphersByKeyExchange(ssl_kea_ecdh_hybrid);
  server_->ConfigNamedGroups({ssl_grp_kem_xyber768d00, ssl_grp_ec_curve25519});

  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
}

TEST_P(TlsKeyExchangeTest13, Xyber768d00DisabledByPolicy) {
  EnsureKeyShareSetup();
  ConfigNamedGroups({ssl_grp_kem_xyber768d00, ssl_grp_ec_secp256r1});

  ASSERT_EQ(SECSuccess, NSS_SetAlgorithmPolicy(SEC_OID_XYBER768D00, 0,
                                               NSS_USE_ALG_IN_SSL_KX));
  ASSERT_EQ(SECSuccess, NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY,
                                               NSS_USE_POLICY_IN_SSL, 0));

  Connect();
  CheckKEXDetails({ssl_grp_ec_secp256r1}, {ssl_grp_ec_secp256r1});

  ASSERT_EQ(SECSuccess, NSS_SetAlgorithmPolicy(SEC_OID_XYBER768D00,
                                               NSS_USE_ALG_IN_SSL_KX, 0));
  ASSERT_EQ(SECSuccess, NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY,
                                               NSS_USE_POLICY_IN_SSL, 0));
}

class XyberShareDamager : public TlsExtensionFilter {
 public:
  typedef enum {
    downgrade,
    extend,
    truncate,
    zero_ecdh,
    modify_ecdh,
    modify_kyber,
  } damage_type;

  XyberShareDamager(const std::shared_ptr<TlsAgent>& a, damage_type damage)
      : TlsExtensionFilter(a), damage_(damage) {}

  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != ssl_tls13_key_share_xtn) {
      return KEEP;
    }

    // Find the Xyber768d00 share
    size_t offset = 0;
    if (agent()->role() == TlsAgent::CLIENT) {
      offset += 2;  // skip KeyShareClientHello length
    }

    uint32_t named_group;
    uint32_t named_group_len;
    input.Read(offset, 2, &named_group);
    input.Read(offset + 2, 2, &named_group_len);
    while (named_group != ssl_grp_kem_xyber768d00) {
      offset += 2 + 2 + named_group_len;
      input.Read(offset, 2, &named_group);
      input.Read(offset + 2, 2, &named_group_len);
    }
    EXPECT_EQ(named_group, ssl_grp_kem_xyber768d00);

    DataBuffer xyber_key_share(input.data() + offset, 2 + 2 + named_group_len);

    // Damage the Xyber768d00 share
    unsigned char* ecdh_component = xyber_key_share.data() + 2 + 2;
    unsigned char* kyber_component =
        xyber_key_share.data() + 2 + 2 + X25519_PUBLIC_KEY_BYTES;
    switch (damage_) {
      case XyberShareDamager::downgrade:
        // Downgrade a Xyber768d00 share to X25519
        xyber_key_share.Truncate(2 + 2 + X25519_PUBLIC_KEY_BYTES);
        xyber_key_share.Write(0, ssl_grp_ec_curve25519, 2);
        xyber_key_share.Write(2, X25519_PUBLIC_KEY_BYTES, 2);
        break;
      case XyberShareDamager::truncate:
        // Truncate a Xyber768d00 share after the X25519 component
        xyber_key_share.Truncate(2 + 2 + X25519_PUBLIC_KEY_BYTES);
        xyber_key_share.Write(2, X25519_PUBLIC_KEY_BYTES, 2);
        break;
      case XyberShareDamager::extend:
        // Append 4 bytes to a Xyber768d00 share
        uint32_t current_len;
        xyber_key_share.Read(2, 2, &current_len);
        xyber_key_share.Write(xyber_key_share.len(), current_len, 4);
        xyber_key_share.Write(2, current_len + 4, 2);
        break;
      case XyberShareDamager::zero_ecdh:
        // Replace an X25519 component with 0s
        memset(ecdh_component, 0, X25519_PUBLIC_KEY_BYTES);
        break;
      case XyberShareDamager::modify_ecdh:
        // Flip a bit in the X25519 component
        ecdh_component[0] ^= 0x01;
        break;
      case XyberShareDamager::modify_kyber:
        // Flip a bit in the Kyber component
        kyber_component[0] ^= 0x01;
        break;
    }

    *output = input;
    output->Splice(xyber_key_share, offset, 2 + 2 + named_group_len);

    // Fix the KeyShareClientHello length if necessary
    if (agent()->role() == TlsAgent::CLIENT &&
        xyber_key_share.len() != 2 + 2 + named_group_len) {
      output->Write(0, output->len() - 2, 2);
    }

    return CHANGE;
  }

 private:
  damage_type damage_;
};

class TlsXyberDamageTest
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<XyberShareDamager::damage_type> {
 public:
  TlsXyberDamageTest()
      : TlsConnectTestBase(ssl_variant_stream, SSL_LIBRARY_VERSION_TLS_1_3) {}

 protected:
  void Damage(const std::shared_ptr<TlsAgent>& agent) {
    EnsureTlsSetup();
    client_->ConfigNamedGroups(
        {ssl_grp_ec_curve25519, ssl_grp_kem_xyber768d00});
    server_->ConfigNamedGroups(
        {ssl_grp_kem_xyber768d00, ssl_grp_ec_curve25519});
    EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));
    MakeTlsFilter<XyberShareDamager>(agent, GetParam());
  }
};

TEST_P(TlsXyberDamageTest, DamageClientShare) {
  Damage(client_);

  switch (GetParam()) {
    case XyberShareDamager::extend:
    case XyberShareDamager::truncate:
      ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
      server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_HYBRID_KEY_SHARE);
      break;
    case XyberShareDamager::zero_ecdh:
      ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
      server_->CheckErrorCode(SEC_ERROR_INVALID_KEY);
      break;
    case XyberShareDamager::downgrade:
    case XyberShareDamager::modify_ecdh:
    case XyberShareDamager::modify_kyber:
      client_->ExpectSendAlert(kTlsAlertBadRecordMac);
      server_->ExpectSendAlert(kTlsAlertBadRecordMac);
      ConnectExpectFail();
      client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      break;
  }
}

TEST_P(TlsXyberDamageTest, DamageServerShare) {
  Damage(server_);

  switch (GetParam()) {
    case XyberShareDamager::extend:
    case XyberShareDamager::truncate:
      client_->ExpectSendAlert(kTlsAlertIllegalParameter);
      server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
      ConnectExpectFail();
      client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_HYBRID_KEY_SHARE);
      break;
    case XyberShareDamager::zero_ecdh:
      client_->ExpectSendAlert(kTlsAlertIllegalParameter);
      server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
      ConnectExpectFail();
      client_->CheckErrorCode(SEC_ERROR_INVALID_KEY);
      break;
    case XyberShareDamager::downgrade:
    case XyberShareDamager::modify_ecdh:
    case XyberShareDamager::modify_kyber:
      client_->ExpectSendAlert(kTlsAlertBadRecordMac);
      server_->ExpectSendAlert(kTlsAlertBadRecordMac);
      ConnectExpectFail();
      client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(TlsXyberDamageTest, TlsXyberDamageTest,
                         ::testing::Values(XyberShareDamager::downgrade,
                                           XyberShareDamager::extend,
                                           XyberShareDamager::truncate,
                                           XyberShareDamager::zero_ecdh,
                                           XyberShareDamager::modify_ecdh,
                                           XyberShareDamager::modify_kyber));

}  // namespace nss_test
