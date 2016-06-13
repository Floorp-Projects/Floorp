/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include <memory>
#include <functional>

#include "scoped_ptrs.h"
#include "tls_parser.h"
#include "tls_filter.h"
#include "tls_connect.h"
#include "gtest_utils.h"

namespace nss_test {

TEST_P(TlsConnectGeneric, ConnectDhe) {
  EnableOnlyDheCiphers();
  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
}

TEST_P(TlsConnectGeneric, ConnectFfdheClient) {
  EnableOnlyDheCiphers();
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(client_->ssl_fd(),
                          SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  auto clientCapture = new TlsExtensionCapture(ssl_supported_groups_xtn);
  client_->SetPacketFilter(clientCapture);

  Connect();

  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);

  // Extension value: length + FFDHE 2048 group identifier.
  const uint8_t val[] = { 0x00, 0x02, 0x01, 0x00 };
  DataBuffer expected_groups(val, sizeof(val));
  EXPECT_EQ(expected_groups, clientCapture->extension());
}

// Requiring the FFDHE extension on the server alone means that clients won't be
// able to connect using a DHE suite.  They should still connect in TLS 1.3,
// because the client automatically sends the supported groups extension.
TEST_P(TlsConnectGenericPre13, ConnectFfdheServer) {
  EnableOnlyDheCiphers();
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(server_->ssl_fd(),
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
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(client_->ssl_fd(),
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
  void ChangeY(const DataBuffer& input, DataBuffer* output,
               size_t offset, const DataBuffer& prime) {
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

  const DataBuffer& prime() const {
    return p_;
  }

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
                         const TlsDheSkeChangeYServer *server_filter)
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
  const TlsDheSkeChangeYServer *server_filter_;
};

/* This matrix includes: mode (stream/datagram), TLS version, what change to
 * make to dh_Ys, whether the client will be configured to require DH named
 * groups.  Test all combinations. */
typedef std::tuple<std::string, uint16_t,
                   TlsDheSkeChangeY::ChangeYTo, bool> DamageDHYProfile;
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
    EXPECT_EQ(SECSuccess,
              SSL_OptionSet(client_->ssl_fd(),
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
    EXPECT_EQ(SECSuccess,
              SSL_OptionSet(client_->ssl_fd(),
                            SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  }
  // The filter on the server is required to capture the prime.
  TlsDheSkeChangeYServer* server_filter
      = new TlsDheSkeChangeYServer(TlsDheSkeChangeY::kYZero, false);
  server_->SetPacketFilter(server_filter);

  // The client filter does the damage.
  TlsDheSkeChangeY::ChangeYTo change = std::get<2>(GetParam());
  client_->SetPacketFilter(
      new TlsDheSkeChangeYClient(change, server_filter));

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
  TlsDheSkeChangeY::kYZero,
  TlsDheSkeChangeY::kYOne,
  TlsDheSkeChangeY::kYPMinusOne,
  TlsDheSkeChangeY::kYGreaterThanP,
  TlsDheSkeChangeY::kYTooLarge,
  TlsDheSkeChangeY::kYZeroPad
};
static ::testing::internal::ParamGenerator<TlsDheSkeChangeY::ChangeYTo>
  kAllY = ::testing::ValuesIn(kAllYArr);
static const bool kTrueFalseArr[] = { true, false };
static ::testing::internal::ParamGenerator<bool>
  kTrueFalse = ::testing::ValuesIn(kTrueFalseArr);

INSTANTIATE_TEST_CASE_P(DamageYStream, TlsDamageDHYTest,
                        ::testing::Combine(
                          TlsConnectTestBase::kTlsModesStream,
                          TlsConnectTestBase::kTlsV10ToV12,
                          kAllY, kTrueFalse));
INSTANTIATE_TEST_CASE_P(DamageYDatagram, TlsDamageDHYTest,
                        ::testing::Combine(
                             TlsConnectTestBase::kTlsModesDatagram,
                             TlsConnectTestBase::kTlsV11V12,
                             kAllY, kTrueFalse));

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
    output->Write(0, dh_len + sizeof(kZeroPad), 2); // increment the length
    output->Splice(&kZeroPad, sizeof(kZeroPad), 2); // insert a zero

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
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(client_->ssl_fd(),
                          SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));
  EXPECT_EQ(SECSuccess,
            SSL_EnableWeakDHEPrimeGroup(server_->ssl_fd(), PR_TRUE));

  Connect();
}

#ifdef NSS_ENABLE_TLS_1_3

// In the absence of HelloRetryRequest, enabling only the 3072-bit group causes
// the TLS 1.3 handshake to fail because the client will only add the 2048-bit
// group to its ClientHello.
TEST_P(TlsConnectTls13, DisableFfdhe2048) {
  EnableOnlyDheCiphers();
  static const SSLDHEGroupType groups[] = { ssl_ff_dhe_3072_group };
  EXPECT_EQ(SECSuccess,
            SSL_DHEGroupPrefSet(client_->ssl_fd(), groups,
                                PR_ARRAY_SIZE(groups)));
  EXPECT_EQ(SECSuccess,
            SSL_DHEGroupPrefSet(server_->ssl_fd(), groups,
                                PR_ARRAY_SIZE(groups)));
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(server_->ssl_fd(),
                          SSL_REQUIRE_DH_NAMED_GROUPS, PR_TRUE));

  ConnectExpectFail();

  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectTls13, ResumeFfdhe) {
  EnableOnlyDheCiphers();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive(); // Need to read so that we absorb the session ticket.
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableOnlyDheCiphers();
  TlsExtensionCapture *clientCapture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  client_->SetPacketFilter(clientCapture);
  TlsExtensionCapture *serverCapture =
      new TlsExtensionCapture(kTlsExtensionPreSharedKey);
  server_->SetPacketFilter(serverCapture);
  ExpectResumption(RESUME_TICKET);
  Connect();
  CheckKeys(ssl_kea_dh, ssl_auth_rsa_sign);
  ASSERT_LT(0UL, clientCapture->extension().len());
  ASSERT_LT(0UL, serverCapture->extension().len());
}

#endif // NSS_ENABLE_TLS_1_3

} // namespace nss_test
