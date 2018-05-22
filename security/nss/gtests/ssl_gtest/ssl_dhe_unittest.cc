/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <functional>
#include <memory>
#include <set>
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectGeneric, ConnectDhe) {
  EnableOnlyDheCiphers();
  Connect();
  CheckKeys(ssl_kea_dh, ssl_grp_ffdhe_2048, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
}

TEST_P(TlsConnectTls13, SharesForBothEcdheAndDhe) {
  EnsureTlsSetup();
  client_->ConfigNamedGroups(kAllDHEGroups);

  auto groups_capture =
      std::make_shared<TlsExtensionCapture>(client_, ssl_supported_groups_xtn);
  auto shares_capture =
      std::make_shared<TlsExtensionCapture>(client_, ssl_tls13_key_share_xtn);
  std::vector<std::shared_ptr<PacketFilter>> captures = {groups_capture,
                                                         shares_capture};
  client_->SetFilter(std::make_shared<ChainedPacketFilter>(captures));

  Connect();

  CheckKeys();

  bool ec, dh;
  auto track_group_type = [&ec, &dh](SSLNamedGroup group) {
    if ((group & 0xff00U) == 0x100U) {
      dh = true;
    } else {
      ec = true;
    }
  };
  CheckGroups(groups_capture->extension(), track_group_type);
  CheckShares(shares_capture->extension(), track_group_type);
  EXPECT_TRUE(ec) << "Should include an EC group and share";
  EXPECT_TRUE(dh) << "Should include an FFDHE group and share";
}

TEST_P(TlsConnectGeneric, ConnectFfdheClient) {
  EnableOnlyDheCiphers();
  client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  auto groups_capture =
      std::make_shared<TlsExtensionCapture>(client_, ssl_supported_groups_xtn);
  auto shares_capture =
      std::make_shared<TlsExtensionCapture>(client_, ssl_tls13_key_share_xtn);
  std::vector<std::shared_ptr<PacketFilter>> captures = {groups_capture,
                                                         shares_capture};
  client_->SetFilter(std::make_shared<ChainedPacketFilter>(captures));

  Connect();

  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
  auto is_ffdhe = [](SSLNamedGroup group) {
    // The group has to be in this range.
    EXPECT_LE(ssl_grp_ffdhe_2048, group);
    EXPECT_GE(ssl_grp_ffdhe_8192, group);
  };
  CheckGroups(groups_capture->extension(), is_ffdhe);
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
    CheckShares(shares_capture->extension(), is_ffdhe);
  } else {
    EXPECT_EQ(0U, shares_capture->extension().len());
  }
}

// Requiring the FFDHE extension on the server alone means that clients won't be
// able to connect using a DHE suite.  They should still connect in TLS 1.3,
// because the client automatically sends the supported groups extension.
TEST_P(TlsConnectGenericPre13, ConnectFfdheServer) {
  EnableOnlyDheCiphers();
  server_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    Connect();
    CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
  } else {
    ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
    client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
    server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  }
}

class TlsDheServerKeyExchangeDamager : public TlsHandshakeFilter {
 public:
  TlsDheServerKeyExchangeDamager(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeServerKeyExchange}) {}
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    // Damage the first octet of dh_p.  Anything other than the known prime will
    // be rejected as "weak" when we have SSL_REQUIRE_DH_NAMED_GROUPS enabled.
    *output = input;
    output->data()[3] ^= 73;
    return CHANGE;
  }
};

// Changing the prime in the server's key share results in an error.  This will
// invalidate the signature over the ServerKeyShare. That's ok, NSS won't check
// the signature until everything else has been checked.
TEST_P(TlsConnectGenericPre13, DamageServerKeyShare) {
  EnableOnlyDheCiphers();
  client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  MakeTlsFilter<TlsDheServerKeyExchangeDamager>(server_);

  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);

  client_->CheckErrorCode(SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

class TlsDheSkeChangeY : public TlsHandshakeFilter {
 public:
  enum ChangeYTo {
    kYZero,
    kYOne,
    kYPMinusOne,
    kYGreaterThanP,
    kYTooLarge,
    kYZeroPad
  };

  TlsDheSkeChangeY(const std::shared_ptr<TlsAgent>& a, uint8_t handshake_type,
                   ChangeYTo change)
      : TlsHandshakeFilter(a, {handshake_type}), change_Y_(change) {}

 protected:
  void ChangeY(const DataBuffer& input, DataBuffer* output, size_t offset,
               const DataBuffer& prime) {
    static const uint8_t kExtraZero = 0;
    static const uint8_t kTooLargeExtra = 1;

    uint32_t dh_Ys_len;
    EXPECT_TRUE(input.Read(offset, 2, &dh_Ys_len));
    EXPECT_LT(offset + dh_Ys_len, input.len());
    offset += 2;

    // This isn't generally true, but our code pads.
    EXPECT_EQ(prime.len(), dh_Ys_len)
        << "Length of dh_Ys must equal length of dh_p";

    *output = input;
    switch (change_Y_) {
      case kYZero:
        memset(output->data() + offset, 0, prime.len());
        break;

      case kYOne:
        memset(output->data() + offset, 0, prime.len() - 1);
        output->Write(offset + prime.len() - 1, 1U, 1);
        break;

      case kYPMinusOne:
        output->Write(offset, prime);
        EXPECT_TRUE(output->data()[offset + prime.len() - 1] & 0x01)
            << "P must at least be odd";
        --output->data()[offset + prime.len() - 1];
        break;

      case kYGreaterThanP:
        // Set the first 32 octets of Y to 0xff, except the first which we set
        // to p[0].  This will make Y > p.  That is, unless p is Mersenne, or
        // improbably large (but still the same bit length).  We currently only
        // use a fixed prime that isn't a problem for this code.
        EXPECT_LT(0, prime.data()[0]) << "dh_p should not be zero-padded";
        offset = output->Write(offset, prime.data()[0], 1);
        memset(output->data() + offset, 0xff, 31);
        break;

      case kYTooLarge:
        // Increase the dh_Ys length.
        output->Write(offset - 2, prime.len() + sizeof(kTooLargeExtra), 2);
        // Then insert the octet.
        output->Splice(&kTooLargeExtra, sizeof(kTooLargeExtra), offset);
        break;

      case kYZeroPad:
        output->Write(offset - 2, prime.len() + sizeof(kExtraZero), 2);
        output->Splice(&kExtraZero, sizeof(kExtraZero), offset);
        break;
    }
  }

 private:
  ChangeYTo change_Y_;
};

class TlsDheSkeChangeYServer : public TlsDheSkeChangeY {
 public:
  TlsDheSkeChangeYServer(const std::shared_ptr<TlsAgent>& a, ChangeYTo change,
                         bool modify)
      : TlsDheSkeChangeY(a, kTlsHandshakeServerKeyExchange, change),
        modify_(modify),
        p_() {}

  const DataBuffer& prime() const { return p_; }

 protected:
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) override {
    size_t offset = 2;
    // Read dh_p
    uint32_t dh_len = 0;
    EXPECT_TRUE(input.Read(0, 2, &dh_len));
    EXPECT_GT(input.len(), offset + dh_len);
    p_.Assign(input.data() + offset, dh_len);
    offset += dh_len;

    // Skip dh_g to find dh_Ys
    EXPECT_TRUE(input.Read(offset, 2, &dh_len));
    offset += 2 + dh_len;

    if (modify_) {
      ChangeY(input, output, offset, p_);
      return CHANGE;
    }
    return KEEP;
  }

 private:
  bool modify_;
  DataBuffer p_;
};

class TlsDheSkeChangeYClient : public TlsDheSkeChangeY {
 public:
  TlsDheSkeChangeYClient(
      const std::shared_ptr<TlsAgent>& a, ChangeYTo change,
      std::shared_ptr<const TlsDheSkeChangeYServer> server_filter)
      : TlsDheSkeChangeY(a, kTlsHandshakeClientKeyExchange, change),
        server_filter_(server_filter) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) override {
    ChangeY(input, output, 0, server_filter_->prime());
    return CHANGE;
  }

 private:
  std::shared_ptr<const TlsDheSkeChangeYServer> server_filter_;
};

/* This matrix includes: variant (stream/datagram), TLS version, what change to
 * make to dh_Ys, whether the client will be configured to require DH named
 * groups.  Test all combinations. */
typedef std::tuple<SSLProtocolVariant, uint16_t, TlsDheSkeChangeY::ChangeYTo,
                   bool>
    DamageDHYProfile;
class TlsDamageDHYTest
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<DamageDHYProfile> {
 public:
  TlsDamageDHYTest()
      : TlsConnectTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())) {}
};

TEST_P(TlsDamageDHYTest, DamageServerY) {
  EnableOnlyDheCiphers();
  if (std::get<3>(GetParam())) {
    client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  }
  TlsDheSkeChangeY::ChangeYTo change = std::get<2>(GetParam());
  MakeTlsFilter<TlsDheSkeChangeYServer>(server_, change, true);

  if (change == TlsDheSkeChangeY::kYZeroPad) {
    ExpectAlert(client_, kTlsAlertDecryptError);
  } else {
    ExpectAlert(client_, kTlsAlertIllegalParameter);
  }
  ConnectExpectFail();
  if (change == TlsDheSkeChangeY::kYZeroPad) {
    // Zero padding Y only manifests in a signature failure.
    // In TLS 1.0 and 1.1, the client reports a device error.
    if (version_ < SSL_LIBRARY_VERSION_TLS_1_2) {
      client_->CheckErrorCode(SEC_ERROR_PKCS11_DEVICE_ERROR);
    } else {
      client_->CheckErrorCode(SEC_ERROR_BAD_SIGNATURE);
    }
    server_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  } else {
    client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE);
    server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  }
}

TEST_P(TlsDamageDHYTest, DamageClientY) {
  EnableOnlyDheCiphers();
  if (std::get<3>(GetParam())) {
    client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  }
  // The filter on the server is required to capture the prime.
  auto server_filter = MakeTlsFilter<TlsDheSkeChangeYServer>(
      server_, TlsDheSkeChangeY::kYZero, false);

  // The client filter does the damage.
  TlsDheSkeChangeY::ChangeYTo change = std::get<2>(GetParam());
  MakeTlsFilter<TlsDheSkeChangeYClient>(client_, change, server_filter);

  if (change == TlsDheSkeChangeY::kYZeroPad) {
    ExpectAlert(server_, kTlsAlertDecryptError);
  } else {
    ExpectAlert(server_, kTlsAlertHandshakeFailure);
  }
  ConnectExpectFail();
  if (change == TlsDheSkeChangeY::kYZeroPad) {
    // Zero padding Y only manifests in a finished error.
    client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
    server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  } else {
    client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_FAILURE_ALERT);
    server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE);
  }
}

static const TlsDheSkeChangeY::ChangeYTo kAllYArr[] = {
    TlsDheSkeChangeY::kYZero,      TlsDheSkeChangeY::kYOne,
    TlsDheSkeChangeY::kYPMinusOne, TlsDheSkeChangeY::kYGreaterThanP,
    TlsDheSkeChangeY::kYTooLarge,  TlsDheSkeChangeY::kYZeroPad};
static ::testing::internal::ParamGenerator<TlsDheSkeChangeY::ChangeYTo> kAllY =
    ::testing::ValuesIn(kAllYArr);
static const bool kTrueFalseArr[] = {true, false};
static ::testing::internal::ParamGenerator<bool> kTrueFalse =
    ::testing::ValuesIn(kTrueFalseArr);

INSTANTIATE_TEST_CASE_P(
    DamageYStream, TlsDamageDHYTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsV10ToV12, kAllY, kTrueFalse));
INSTANTIATE_TEST_CASE_P(
    DamageYDatagram, TlsDamageDHYTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsDatagram,
                       TlsConnectTestBase::kTlsV11V12, kAllY, kTrueFalse));

class TlsDheSkeMakePEven : public TlsHandshakeFilter {
 public:
  TlsDheSkeMakePEven(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeServerKeyExchange}) {}

  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    // Find the end of dh_p
    uint32_t dh_len = 0;
    EXPECT_TRUE(input.Read(0, 2, &dh_len));
    EXPECT_GT(input.len(), 2 + dh_len) << "enough space for dh_p";
    size_t offset = 2 + dh_len - 1;
    EXPECT_TRUE((input.data()[offset] & 0x01) == 0x01) << "p should be odd";

    *output = input;
    output->data()[offset] &= 0xfe;

    return CHANGE;
  }
};

// Even without requiring named groups, an even value for p is bad news.
TEST_P(TlsConnectGenericPre13, MakeDhePEven) {
  EnableOnlyDheCiphers();
  MakeTlsFilter<TlsDheSkeMakePEven>(server_);

  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

class TlsDheSkeZeroPadP : public TlsHandshakeFilter {
 public:
  TlsDheSkeZeroPadP(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeServerKeyExchange}) {}

  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    *output = input;
    uint32_t dh_len = 0;
    EXPECT_TRUE(input.Read(0, 2, &dh_len));
    static const uint8_t kZeroPad = 0;
    output->Write(0, dh_len + sizeof(kZeroPad), 2);  // increment the length
    output->Splice(&kZeroPad, sizeof(kZeroPad), 2);  // insert a zero

    return CHANGE;
  }
};

// Zero padding only causes signature failure.
TEST_P(TlsConnectGenericPre13, PadDheP) {
  EnableOnlyDheCiphers();
  MakeTlsFilter<TlsDheSkeZeroPadP>(server_);

  ConnectExpectAlert(client_, kTlsAlertDecryptError);

  // In TLS 1.0 and 1.1, the client reports a device error.
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_2) {
    client_->CheckErrorCode(SEC_ERROR_PKCS11_DEVICE_ERROR);
  } else {
    client_->CheckErrorCode(SEC_ERROR_BAD_SIGNATURE);
  }
  server_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
}

// The server should not pick the weak DH group if the client includes FFDHE
// named groups in the supported_groups extension. The server then picks a
// commonly-supported named DH group and this connects.
//
// Note: This test case can take ages to generate the weak DH key.
TEST_P(TlsConnectGenericPre13, WeakDHGroup) {
  EnableOnlyDheCiphers();
  client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  EXPECT_EQ(SECSuccess,
            SSL_EnableWeakDHEPrimeGroup(server_->ssl_fd(), PR_TRUE));

  Connect();
}

TEST_P(TlsConnectGeneric, Ffdhe3072) {
  EnableOnlyDheCiphers();
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ffdhe_3072};
  client_->ConfigNamedGroups(groups);

  Connect();
}

// Even though the client doesn't have DHE groups enabled the server assumes it
// does. Because the client doesn't require named groups it accepts FF3072 as
// custom group.
TEST_P(TlsConnectGenericPre13, NamedGroupMismatchPre13) {
  EnableOnlyDheCiphers();
  static const std::vector<SSLNamedGroup> server_groups = {ssl_grp_ffdhe_3072};
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp256r1};
  server_->ConfigNamedGroups(server_groups);
  client_->ConfigNamedGroups(client_groups);

  Connect();
  CheckKeys(ssl_kea_dh, ssl_grp_ffdhe_custom, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
}

// Same test but for TLS 1.3. This has to fail.
TEST_P(TlsConnectTls13, NamedGroupMismatch13) {
  EnableOnlyDheCiphers();
  static const std::vector<SSLNamedGroup> server_groups = {ssl_grp_ffdhe_3072};
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp256r1};
  server_->ConfigNamedGroups(server_groups);
  client_->ConfigNamedGroups(client_groups);

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// Replace the key share in the server key exchange message with one that's
// larger than 8192 bits.
class TooLongDHEServerKEXFilter : public TlsHandshakeFilter {
 public:
  TooLongDHEServerKEXFilter(const std::shared_ptr<TlsAgent>& server)
      : TlsHandshakeFilter(server, {kTlsHandshakeServerKeyExchange}) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    // Replace the server key exchange message very large DH shares that are
    // not supported by NSS.
    const uint32_t share_len = 0x401;
    const uint8_t zero_share[share_len] = {0x80};
    size_t offset = 0;
    // Write dh_p.
    offset = output->Write(offset, share_len, 2);
    offset = output->Write(offset, zero_share, share_len);
    // Write dh_g.
    offset = output->Write(offset, share_len, 2);
    offset = output->Write(offset, zero_share, share_len);
    // Write dh_Y.
    offset = output->Write(offset, share_len, 2);
    offset = output->Write(offset, zero_share, share_len);

    return CHANGE;
  }
};

TEST_P(TlsConnectGenericPre13, TooBigDHGroup) {
  EnableOnlyDheCiphers();
  MakeTlsFilter<TooLongDHEServerKEXFilter>(server_);
  client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_FALSE);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  client_->CheckErrorCode(SSL_ERROR_DH_KEY_TOO_LONG);
}

// Even though the client doesn't have DHE groups enabled the server assumes it
// does. The client requires named groups and thus does not accept FF3072 as
// custom group in contrast to the previous test.
TEST_P(TlsConnectGenericPre13, RequireNamedGroupsMismatchPre13) {
  EnableOnlyDheCiphers();
  client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  static const std::vector<SSLNamedGroup> server_groups = {ssl_grp_ffdhe_3072};
  static const std::vector<SSLNamedGroup> client_groups = {ssl_grp_ec_secp256r1,
                                                           ssl_grp_ffdhe_2048};
  server_->ConfigNamedGroups(server_groups);
  client_->ConfigNamedGroups(client_groups);

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectGenericPre13, PreferredFfdhe) {
  EnableOnlyDheCiphers();
  static const SSLDHEGroupType groups[] = {ssl_ff_dhe_3072_group,
                                           ssl_ff_dhe_2048_group};
  EXPECT_EQ(SECSuccess, SSL_DHEGroupPrefSet(server_->ssl_fd(), groups,
                                            PR_ARRAY_SIZE(groups)));

  Connect();
  client_->CheckKEA(ssl_kea_dh, ssl_grp_ffdhe_3072, 3072);
  server_->CheckKEA(ssl_kea_dh, ssl_grp_ffdhe_3072, 3072);
  client_->CheckAuthType(ssl_auth_rsa_sign, ssl_sig_rsa_pss_rsae_sha256);
  server_->CheckAuthType(ssl_auth_rsa_sign, ssl_sig_rsa_pss_rsae_sha256);
}

TEST_P(TlsConnectGenericPre13, MismatchDHE) {
  EnableOnlyDheCiphers();
  client_->SetOption(SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE);
  static const SSLDHEGroupType serverGroups[] = {ssl_ff_dhe_3072_group};
  EXPECT_EQ(SECSuccess, SSL_DHEGroupPrefSet(server_->ssl_fd(), serverGroups,
                                            PR_ARRAY_SIZE(serverGroups)));
  static const SSLDHEGroupType clientGroups[] = {ssl_ff_dhe_2048_group};
  EXPECT_EQ(SECSuccess, SSL_DHEGroupPrefSet(client_->ssl_fd(), clientGroups,
                                            PR_ARRAY_SIZE(clientGroups)));

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectTls13, ResumeFfdhe) {
  EnableOnlyDheCiphers();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_dh, ssl_grp_ffdhe_2048, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableOnlyDheCiphers();
  auto clientCapture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_pre_shared_key_xtn);
  auto serverCapture =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_tls13_pre_shared_key_xtn);
  ExpectResumption(RESUME_TICKET);
  Connect();
  CheckKeys(ssl_kea_dh, ssl_grp_ffdhe_2048, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
  ASSERT_LT(0UL, clientCapture->extension().len());
  ASSERT_LT(0UL, serverCapture->extension().len());
}

class TlsDheSkeChangeSignature : public TlsHandshakeFilter {
 public:
  TlsDheSkeChangeSignature(const std::shared_ptr<TlsAgent>& a, uint16_t version,
                           const uint8_t* data, size_t len)
      : TlsHandshakeFilter(a, {kTlsHandshakeServerKeyExchange}),
        version_(version),
        data_(data),
        len_(len) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    TlsParser parser(input);
    EXPECT_TRUE(parser.SkipVariable(2));  // dh_p
    EXPECT_TRUE(parser.SkipVariable(2));  // dh_g
    EXPECT_TRUE(parser.SkipVariable(2));  // dh_Ys

    // Copy DH params to output.
    size_t offset = output->Write(0, input.data(), parser.consumed());

    if (version_ == SSL_LIBRARY_VERSION_TLS_1_2) {
      // Write signature algorithm.
      offset = output->Write(offset, ssl_sig_dsa_sha256, 2);
    }

    // Write new signature.
    offset = output->Write(offset, len_, 2);
    offset = output->Write(offset, data_, len_);

    return CHANGE;
  }

 private:
  uint16_t version_;
  const uint8_t* data_;
  size_t len_;
};

TEST_P(TlsConnectGenericPre13, InvalidDERSignatureFfdhe) {
  const uint8_t kBogusDheSignature[] = {
      0x30, 0x69, 0x3c, 0x02, 0x1c, 0x7d, 0x0b, 0x2f, 0x64, 0x00, 0x27,
      0xae, 0xcf, 0x1e, 0x28, 0x08, 0x6a, 0x7f, 0xb1, 0xbd, 0x78, 0xb5,
      0x3b, 0x8c, 0x8f, 0x59, 0xed, 0x8f, 0xee, 0x78, 0xeb, 0x2c, 0xe9,
      0x02, 0x1c, 0x6d, 0x7f, 0x3c, 0x0f, 0xf4, 0x44, 0x35, 0x0b, 0xb2,
      0x6d, 0xdc, 0xb8, 0x21, 0x87, 0xdd, 0x0d, 0xb9, 0x46, 0x09, 0x3e,
      0xef, 0x81, 0x5b, 0x37, 0x09, 0x39, 0xeb};

  Reset(TlsAgent::kServerDsa);

  const std::vector<SSLNamedGroup> client_groups = {ssl_grp_ffdhe_2048};
  client_->ConfigNamedGroups(client_groups);

  MakeTlsFilter<TlsDheSkeChangeSignature>(server_, version_, kBogusDheSignature,
                                          sizeof(kBogusDheSignature));

  ConnectExpectAlert(client_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

}  // namespace nss_test
