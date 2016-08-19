/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include <functional>
#include <memory>
#include <set>
#include "secerr.h"
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
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
}

// Track groups and make sure that there are no duplicates.
class CheckDuplicateGroup {
 public:
  void AddAndCheckGroup(uint16_t group) {
    EXPECT_EQ(groups_.end(), groups_.find(group))
        << "Group " << group << " should not be duplicated";
    groups_.insert(group);
  }

 private:
  std::set<uint16_t> groups_;
};

// Check the group of each of the supported groups
static void CheckGroups(const DataBuffer& groups,
                        std::function<void(uint16_t)> check_group) {
  CheckDuplicateGroup group_set;
  uint32_t tmp;
  EXPECT_TRUE(groups.Read(0, 2, &tmp));
  EXPECT_EQ(groups.len() - 2, static_cast<size_t>(tmp));
  for (size_t i = 2; i < groups.len(); i += 2) {
    EXPECT_TRUE(groups.Read(i, 2, &tmp));
    uint16_t group = static_cast<uint16_t>(tmp);
    group_set.AddAndCheckGroup(group);
    check_group(group);
  }
}

// Check the group of each of the shares
static void CheckShares(const DataBuffer& shares,
                        std::function<void(uint16_t)> check_group) {
  CheckDuplicateGroup group_set;
  uint32_t tmp;
  EXPECT_TRUE(shares.Read(0, 2, &tmp));
  EXPECT_EQ(shares.len() - 2, static_cast<size_t>(tmp));
  size_t i;
  for (i = 2; i < shares.len(); i += 4 + tmp) {
    ASSERT_TRUE(shares.Read(i, 2, &tmp));
    uint16_t group = static_cast<uint16_t>(tmp);
    group_set.AddAndCheckGroup(group);
    check_group(group);
    ASSERT_TRUE(shares.Read(i + 2, 2, &tmp));
  }
  EXPECT_EQ(shares.len(), i);
}

TEST_P(TlsConnectTls13, SharesForBothEcdheAndDhe) {
  EnsureTlsSetup();
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  client_->EnableCiphersByKeyExchange(ssl_kea_dh);

  auto groups_capture = new TlsExtensionCapture(ssl_supported_groups_xtn);
  auto shares_capture = new TlsExtensionCapture(ssl_tls13_key_share_xtn);
  std::vector<PacketFilter*> captures;
  captures.push_back(groups_capture);
  captures.push_back(shares_capture);
  client_->SetPacketFilter(new ChainedPacketFilter(captures));

  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);

  bool ec, dh;
  auto track_group_type = [&ec, &dh](uint16_t group) {
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

TEST_P(TlsConnectTls13, NoDheOnEcdheConnections) {
  EnsureTlsSetup();
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);

  auto groups_capture = new TlsExtensionCapture(ssl_supported_groups_xtn);
  auto shares_capture = new TlsExtensionCapture(ssl_tls13_key_share_xtn);
  std::vector<PacketFilter*> captures;
  captures.push_back(groups_capture);
  captures.push_back(shares_capture);
  client_->SetPacketFilter(new ChainedPacketFilter(captures));

  Connect();

  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  auto is_ecc = [](uint16_t group) { EXPECT_NE(0x100U, group & 0xff00U); };
  CheckGroups(groups_capture->extension(), is_ecc);
  CheckShares(shares_capture->extension(), is_ecc);
}

TEST_P(TlsConnectGeneric, ConnectFfdheClient) {
  EnableOnlyDheCiphers();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  auto groups_capture = new TlsExtensionCapture(ssl_supported_groups_xtn);
  auto shares_capture = new TlsExtensionCapture(ssl_tls13_key_share_xtn);
  std::vector<PacketFilter*> captures;
  captures.push_back(groups_capture);
  captures.push_back(shares_capture);
  client_->SetPacketFilter(new ChainedPacketFilter(captures));

  Connect();

  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
  auto is_ffdhe = [](uint16_t group) {
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
  EXPECT_EQ(SECSuccess, SSL_OptionSet(server_->ssl_fd(),
                                      SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    Connect();
    CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
  } else {
    ConnectExpectFail();
    client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
    server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  }
}

class TlsDheServerKeyExchangeDamager : public TlsHandshakeFilter {
 public:
  TlsDheServerKeyExchangeDamager() {}
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() != kTlsHandshakeServerKeyExchange) {
      return KEEP;
    }

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
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  server_->SetPacketFilter(new TlsDheServerKeyExchangeDamager());

  ConnectExpectFail();

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

  TlsDheSkeChangeY(ChangeYTo change) : change_Y_(change) {}

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
  TlsDheSkeChangeYServer(ChangeYTo change, bool modify)
      : TlsDheSkeChangeY(change), modify_(modify), p_() {}

  const DataBuffer& prime() const { return p_; }

 protected:
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) override {
    if (header.handshake_type() != kTlsHandshakeServerKeyExchange) {
      return KEEP;
    }

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
  TlsDheSkeChangeYClient(ChangeYTo change,
                         const TlsDheSkeChangeYServer* server_filter)
      : TlsDheSkeChangeY(change), server_filter_(server_filter) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) override {
    if (header.handshake_type() != kTlsHandshakeClientKeyExchange) {
      return KEEP;
    }

    ChangeY(input, output, 0, server_filter_->prime());
    return CHANGE;
  }

 private:
  const TlsDheSkeChangeYServer* server_filter_;
};

/* This matrix includes: mode (stream/datagram), TLS version, what change to
 * make to dh_Ys, whether the client will be configured to require DH named
 * groups.  Test all combinations. */
typedef std::tuple<std::string, uint16_t, TlsDheSkeChangeY::ChangeYTo, bool>
    DamageDHYProfile;
class TlsDamageDHYTest
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<DamageDHYProfile> {
 public:
  TlsDamageDHYTest()
      : TlsConnectTestBase(TlsConnectTestBase::ToMode(std::get<0>(GetParam())),
                           std::get<1>(GetParam())) {}
};

TEST_P(TlsDamageDHYTest, DamageServerY) {
  EnableOnlyDheCiphers();
  if (std::get<3>(GetParam())) {
    EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                        SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  }
  TlsDheSkeChangeY::ChangeYTo change = std::get<2>(GetParam());
  server_->SetPacketFilter(new TlsDheSkeChangeYServer(change, true));

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
    EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                        SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  }
  // The filter on the server is required to capture the prime.
  TlsDheSkeChangeYServer* server_filter =
      new TlsDheSkeChangeYServer(TlsDheSkeChangeY::kYZero, false);
  server_->SetPacketFilter(server_filter);

  // The client filter does the damage.
  TlsDheSkeChangeY::ChangeYTo change = std::get<2>(GetParam());
  client_->SetPacketFilter(new TlsDheSkeChangeYClient(change, server_filter));

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

INSTANTIATE_TEST_CASE_P(DamageYStream, TlsDamageDHYTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsModesStream,
                                           TlsConnectTestBase::kTlsV10ToV12,
                                           kAllY, kTrueFalse));
INSTANTIATE_TEST_CASE_P(
    DamageYDatagram, TlsDamageDHYTest,
    ::testing::Combine(TlsConnectTestBase::kTlsModesDatagram,
                       TlsConnectTestBase::kTlsV11V12, kAllY, kTrueFalse));

class TlsDheSkeMakePEven : public TlsHandshakeFilter {
 public:
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() != kTlsHandshakeServerKeyExchange) {
      return KEEP;
    }

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
  server_->SetPacketFilter(new TlsDheSkeMakePEven());

  ConnectExpectFail();

  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_DHE_KEY_SHARE);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

class TlsDheSkeZeroPadP : public TlsHandshakeFilter {
 public:
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() != kTlsHandshakeServerKeyExchange) {
      return KEEP;
    }

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
  server_->SetPacketFilter(new TlsDheSkeZeroPadP());

  ConnectExpectFail();

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
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  EXPECT_EQ(SECSuccess,
            SSL_EnableWeakDHEPrimeGroup(server_->ssl_fd(), PR_TRUE));

  Connect();
}

TEST_P(TlsConnectGeneric, Ffdhe3072) {
  EnableOnlyDheCiphers();
  client_->ConfigNamedGroup(ssl_grp_ffdhe_2048, false);

  Connect();
}

TEST_P(TlsConnectGenericPre13, PreferredFfdhe) {
  EnableOnlyDheCiphers();
  static const SSLDHEGroupType groups[] = {ssl_ff_dhe_3072_group,
                                           ssl_ff_dhe_2048_group};
  EXPECT_EQ(SECSuccess, SSL_DHEGroupPrefSet(server_->ssl_fd(), groups,
                                            PR_ARRAY_SIZE(groups)));

  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign, 3072);
}

TEST_P(TlsConnectTls13, ResumeFfdhe) {
  EnableOnlyDheCiphers();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableOnlyDheCiphers();
  TlsExtensionCapture* clientCapture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetPacketFilter(clientCapture);
  TlsExtensionCapture* serverCapture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  server_->SetPacketFilter(serverCapture);
  ExpectResumption(RESUME_TICKET);
  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
  ASSERT_LT(0UL, clientCapture->extension().len());
  ASSERT_LT(0UL, serverCapture->extension().len());
}

}  // namespace nss_test
