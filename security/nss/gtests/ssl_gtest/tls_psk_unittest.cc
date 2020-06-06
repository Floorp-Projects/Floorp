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

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

class Tls13PskTest : public TlsConnectTestBase,
                     public ::testing::WithParamInterface<
                         std::tuple<SSLProtocolVariant, uint16_t>> {
 public:
  Tls13PskTest()
      : TlsConnectTestBase(std::get<0>(GetParam()),
                           SSL_LIBRARY_VERSION_TLS_1_3),
        suite_(std::get<1>(GetParam())) {}

  void SetUp() override {
    TlsConnectTestBase::SetUp();
    scoped_psk_.reset(GetPsk());
    ASSERT_TRUE(!!scoped_psk_);
  }

 private:
  PK11SymKey* GetPsk() {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE();
      return nullptr;
    }

    SECItem psk_item;
    psk_item.type = siBuffer;
    psk_item.len = sizeof(kPskDummyVal_);
    psk_item.data = const_cast<uint8_t*>(kPskDummyVal_);

    PK11SymKey* key =
        PK11_ImportSymKey(slot.get(), CKM_HKDF_KEY_GEN, PK11_OriginUnwrap,
                          CKA_DERIVE, &psk_item, NULL);
    if (!key) {
      ADD_FAILURE();
    }
    return key;
  }

 protected:
  ScopedPK11SymKey scoped_psk_;
  const uint16_t suite_;
  const uint8_t kPskDummyVal_[16] = {0x01, 0x02, 0x03, 0x04, 0x05,
                                     0x06, 0x07, 0x08, 0x09, 0x0a,
                                     0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  const std::string kPskDummyLabel_ = "NSS PSK GTEST label";
  const SSLHashType kPskHash_ = ssl_hash_sha384;
};

// TLS 1.3 PSK connection test.
TEST_P(Tls13PskTest, NormalExternal) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
  client_->RemovePsk(kPskDummyLabel_);
  server_->RemovePsk(kPskDummyLabel_);

  // Removing it again should fail.
  EXPECT_EQ(SECFailure, SSL_RemoveExternalPsk(client_->ssl_fd(),
                                              reinterpret_cast<const uint8_t*>(
                                                  kPskDummyLabel_.data()),
                                              kPskDummyLabel_.length()));
  EXPECT_EQ(SECFailure, SSL_RemoveExternalPsk(server_->ssl_fd(),
                                              reinterpret_cast<const uint8_t*>(
                                                  kPskDummyLabel_.data()),
                                              kPskDummyLabel_.length()));
}

TEST_P(Tls13PskTest, KeyTooLarge) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey scoped_psk(PK11_KeyGen(
      slot.get(), CKM_GENERIC_SECRET_KEY_GEN, nullptr, 128, nullptr));
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
}

// Attempt to use a PSK with the wrong PRF hash.
// "Clients MUST verify that...the server selected a cipher suite
// indicating a Hash associated with the PSK"
TEST_P(Tls13PskTest, ClientVerifyHashType) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  MakeTlsFilter<SelectedCipherSuiteReplacer>(server_,
                                             TLS_CHACHA20_POLY1305_SHA256);
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    ConnectExpectFail();
    EXPECT_EQ(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE, server_->error_code());
  } else {
    ConnectExpectFailOneSide(TlsAgent::CLIENT);
  }
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

// Different EPSKs (by label) on each endpoint. Expect cert auth.
TEST_P(Tls13PskTest, LabelMismatch) {
  client_->AddPsk(scoped_psk_, std::string("foo"), kPskHash_);
  server_->AddPsk(scoped_psk_, std::string("bar"), kPskHash_);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
}

SSLHelloRetryRequestAction RetryFirstHello(
    PRBool firstHello, const PRUint8* clientToken, unsigned int clientTokenLen,
    PRUint8* appToken, unsigned int* appTokenLen, unsigned int appTokenMax,
    void* arg) {
  auto* called = reinterpret_cast<size_t*>(arg);
  ++*called;
  EXPECT_EQ(0U, clientTokenLen);
  EXPECT_EQ(*called, firstHello ? 1U : 2U);
  return firstHello ? ssl_hello_retry_request : ssl_hello_retry_accept;
}

// Test resumption PSK with HRR.
TEST_P(Tls13PskTest, ResPskRetryStateless) {
  ConfigureSelfEncrypt();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();

  Reset();
  StartConnect();
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryFirstHello, &cb_called));
  ExpectResumption(RESUME_TICKET);
  Handshake();
  CheckConnected();
  EXPECT_EQ(2U, cb_called);
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
  SendReceive();
}

// Test external PSK with HRR.
TEST_P(Tls13PskTest, ExtPskRetryStateless) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryFirstHello, &cb_called));
  StartConnect();
  client_->Handshake();
  server_->Handshake();
  EXPECT_EQ(1U, cb_called);
  auto replacement = std::make_shared<TlsAgent>(
      server_->name(), TlsAgent::SERVER, server_->variant());
  server_ = replacement;
  server_->SetVersionRange(version_, version_);
  client_->SetPeer(server_);
  server_->SetPeer(client_);
  server_->AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  server_->ExpectPsk();
  server_->StartConnect();
  Handshake();
  CheckConnected();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
}

// Server not configured with PSK and sends a certificate instead of
// a selected_identity. Client should attempt certificate authentication.
TEST_P(Tls13PskTest, ClientOnly) {
  client_->AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
}

// Set a PSK, remove psk_key_exchange_modes.
TEST_P(Tls13PskTest, DropKexModes) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  StartConnect();
  MakeTlsFilter<TlsExtensionDropper>(client_,
                                     ssl_tls13_psk_key_exchange_modes_xtn);
  ConnectExpectAlert(server_, kTlsAlertMissingExtension);
  client_->CheckErrorCode(SSL_ERROR_MISSING_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_MISSING_PSK_KEY_EXCHANGE_MODES);
}

// "Clients MUST verify that...a server "key_share" extension is present
// if required by the ClientHello "psk_key_exchange_modes" extension."
// As we don't support PSK without DH, it is always required.
TEST_P(Tls13PskTest, DropRequiredKeyShare) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  StartConnect();
  MakeTlsFilter<TlsExtensionDropper>(server_, ssl_tls13_key_share_xtn);
  client_->ExpectSendAlert(kTlsAlertMissingExtension);
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    ConnectExpectFail();
  } else {
    ConnectExpectFailOneSide(TlsAgent::CLIENT);
  }
  client_->CheckErrorCode(SSL_ERROR_MISSING_KEY_SHARE);
}

// "Clients MUST verify that...the server's selected_identity is
// within the range supplied by the client". We send one OfferedPsk.
TEST_P(Tls13PskTest, InvalidSelectedIdentity) {
  static const uint8_t selected_identity[] = {0x00, 0x01};
  DataBuffer buf(selected_identity, sizeof(selected_identity));
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  StartConnect();
  MakeTlsFilter<TlsExtensionReplacer>(server_, ssl_tls13_pre_shared_key_xtn,
                                      buf);
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    ConnectExpectFail();
  } else {
    ConnectExpectFailOneSide(TlsAgent::CLIENT);
  }
  client_->CheckErrorCode(SSL_ERROR_MALFORMED_PRE_SHARED_KEY);
}

// Resume-eligible reconnect with an EPSK configured.
// Expect the EPSK to be used.
TEST_P(Tls13PskTest, PreferEpsk) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();

  Reset();
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  ExpectResumption(RESUME_NONE);
  StartConnect();
  Handshake();
  CheckConnected();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
}

// Enable resumption, but connect (initially) with an EPSK.
// Expect no session ticket.
TEST_P(Tls13PskTest, SuppressNewSessionTicket) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  auto nst_capture =
      MakeTlsFilter<TlsHandshakeRecorder>(server_, ssl_hs_new_session_ticket);
  nst_capture->EnableDecryption();
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
  EXPECT_EQ(SECFailure, SSL_SendSessionTicket(server_->ssl_fd(), nullptr, 0));
  EXPECT_EQ(0U, nst_capture->buffer().len());
  if (variant_ == ssl_variant_stream) {
    EXPECT_EQ(SSL_ERROR_FEATURE_DISABLED, PORT_GetError());
  } else {
    EXPECT_EQ(SSL_ERROR_FEATURE_NOT_SUPPORTED_FOR_VERSION, PORT_GetError());
  }

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  ExpectResumption(RESUME_NONE);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
}

TEST_P(Tls13PskTest, BadConfigValues) {
  EXPECT_TRUE(client_->EnsureTlsSetup());
  std::vector<uint8_t> label{'L', 'A', 'B', 'E', 'L'};
  EXPECT_EQ(SECFailure,
            SSL_AddExternalPsk(client_->ssl_fd(), nullptr, label.data(),
                               label.size(), kPskHash_));
  EXPECT_EQ(SECFailure, SSL_AddExternalPsk(client_->ssl_fd(), scoped_psk_.get(),
                                           nullptr, label.size(), kPskHash_));

  EXPECT_EQ(SECFailure, SSL_AddExternalPsk(client_->ssl_fd(), scoped_psk_.get(),
                                           label.data(), 0, kPskHash_));
  EXPECT_EQ(SECSuccess,
            SSL_AddExternalPsk(client_->ssl_fd(), scoped_psk_.get(),
                               label.data(), label.size(), ssl_hash_sha256));

  EXPECT_EQ(SECFailure,
            SSL_RemoveExternalPsk(client_->ssl_fd(), nullptr, label.size()));

  EXPECT_EQ(SECFailure,
            SSL_RemoveExternalPsk(client_->ssl_fd(), label.data(), 0));

  EXPECT_EQ(SECSuccess, SSL_RemoveExternalPsk(client_->ssl_fd(), label.data(),
                                              label.size()));
}

// If the server has an EPSK configured with a ciphersuite not supported
// by the client, it should use certificate authentication.
TEST_P(Tls13PskTest, FallbackUnsupportedCiphersuite) {
  client_->AddPsk(scoped_psk_, kPskDummyLabel_, ssl_hash_sha256,
                  TLS_AES_128_GCM_SHA256);
  server_->AddPsk(scoped_psk_, kPskDummyLabel_, ssl_hash_sha256,
                  TLS_CHACHA20_POLY1305_SHA256);

  client_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_auth_rsa_sign);
}

// That fallback should not occur if there is no cipher overlap.
TEST_P(Tls13PskTest, ExplicitSuiteNoOverlap) {
  client_->AddPsk(scoped_psk_, kPskDummyLabel_, ssl_hash_sha256,
                  TLS_AES_128_GCM_SHA256);
  server_->AddPsk(scoped_psk_, kPskDummyLabel_, ssl_hash_sha256,
                  TLS_CHACHA20_POLY1305_SHA256);

  client_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
  server_->EnableSingleCipher(TLS_CHACHA20_POLY1305_SHA256);
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(Tls13PskTest, SuppressHandshakeCertReq) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  server_->SetOption(SSL_REQUEST_CERTIFICATE, PR_TRUE);
  server_->SetOption(SSL_REQUIRE_CERTIFICATE, PR_TRUE);
  const std::set<uint8_t> hs_types = {ssl_hs_certificate,
                                      ssl_hs_certificate_request};
  auto cr_cert_capture = MakeTlsFilter<TlsHandshakeRecorder>(server_, hs_types);
  cr_cert_capture->EnableDecryption();

  Connect();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
  EXPECT_EQ(0U, cr_cert_capture->buffer().len());
}

TEST_P(Tls13PskTest, DisallowClientConfigWithoutServerCert) {
  AddPsk(scoped_psk_, kPskDummyLabel_, kPskHash_);
  server_->SetOption(SSL_REQUEST_CERTIFICATE, PR_TRUE);
  server_->SetOption(SSL_REQUIRE_CERTIFICATE, PR_TRUE);
  const std::set<uint8_t> hs_types = {ssl_hs_certificate,
                                      ssl_hs_certificate_request};
  auto cr_cert_capture = MakeTlsFilter<TlsHandshakeRecorder>(server_, hs_types);
  cr_cert_capture->EnableDecryption();

  EXPECT_EQ(SECSuccess, SSLInt_RemoveServerCertificates(server_->ssl_fd()));

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CERTIFICATE);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  EXPECT_EQ(0U, cr_cert_capture->buffer().len());
}

TEST_F(TlsConnectStreamTls13, ClientRejectHandshakeCertReq) {
  // Stream only, as the filter doesn't support DTLS 1.3 yet.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey scoped_psk(PK11_KeyGen(
      slot.get(), CKM_GENERIC_SECRET_KEY_GEN, nullptr, 32, nullptr));
  AddPsk(scoped_psk, std::string("foo"), ssl_hash_sha256);
  // Inject a CR after EE. This would be legal if not for ssl_auth_psk.
  auto filter = MakeTlsFilter<TlsEncryptedHandshakeMessageReplacer>(
      server_, kTlsHandshakeFinished, kTlsHandshakeCertificateRequest);
  filter->EnableDecryption();

  ExpectAlert(client_, kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST);
  server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

TEST_F(TlsConnectStreamTls13, RejectPha) {
  // Stream only, as the filter doesn't support DTLS 1.3 yet.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey scoped_psk(PK11_KeyGen(
      slot.get(), CKM_GENERIC_SECRET_KEY_GEN, nullptr, 32, nullptr));
  AddPsk(scoped_psk, std::string("foo"), ssl_hash_sha256);
  server_->SetOption(SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE);
  auto kuToCr = MakeTlsFilter<TlsEncryptedHandshakeMessageReplacer>(
      server_, kTlsHandshakeKeyUpdate, kTlsHandshakeCertificateRequest);
  kuToCr->EnableDecryption();
  Connect();

  // Make sure the direct path is blocked.
  EXPECT_EQ(SECFailure, SSL_SendCertificateRequest(server_->ssl_fd()));
  EXPECT_EQ(SSL_ERROR_FEATURE_DISABLED, PORT_GetError());

  // Inject a PHA CR. Since this is not allowed, send KeyUpdate
  // and change the message type.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  ExpectAlert(client_, kTlsAlertUnexpectedMessage);
  client_->Handshake();  // Eat the CR.
  server_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_CERT_REQUEST);
  server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

class Tls13PskTestWithCiphers : public Tls13PskTest {};

TEST_P(Tls13PskTestWithCiphers, 0RttCiphers) {
  RolloverAntiReplay();
  AddPsk(scoped_psk_, kPskDummyLabel_, tls13_GetHashForCipherSuite(suite_),
         suite_);
  StartConnect();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
}

TEST_P(Tls13PskTestWithCiphers, 0RttMaxEarlyData) {
  EnsureTlsSetup();
  RolloverAntiReplay();
  const char* big_message = "0123456789abcdef";
  const size_t short_size = strlen(big_message) - 1;
  const PRInt32 short_length = static_cast<PRInt32>(short_size);

  // Set up the PSK
  EXPECT_EQ(SECSuccess,
            SSL_AddExternalPsk0Rtt(
                client_->ssl_fd(), scoped_psk_.get(),
                reinterpret_cast<const uint8_t*>(kPskDummyLabel_.data()),
                kPskDummyLabel_.length(), tls13_GetHashForCipherSuite(suite_),
                suite_, short_length));
  EXPECT_EQ(SECSuccess,
            SSL_AddExternalPsk0Rtt(
                server_->ssl_fd(), scoped_psk_.get(),
                reinterpret_cast<const uint8_t*>(kPskDummyLabel_.data()),
                kPskDummyLabel_.length(), tls13_GetHashForCipherSuite(suite_),
                suite_, short_length));
  client_->ExpectPsk();
  server_->ExpectPsk();
  client_->expected_cipher_suite(suite_);
  server_->expected_cipher_suite(suite_);
  StartConnect();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  client_->Handshake();
  CheckEarlyDataLimit(client_, short_size);

  PRInt32 sent;
  // Writing more than the limit will succeed in TLS, but fail in DTLS.
  if (variant_ == ssl_variant_stream) {
    sent = PR_Write(client_->ssl_fd(), big_message,
                    static_cast<PRInt32>(strlen(big_message)));
  } else {
    sent = PR_Write(client_->ssl_fd(), big_message,
                    static_cast<PRInt32>(strlen(big_message)));
    EXPECT_GE(0, sent);
    EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

    // Try an exact-sized write now.
    sent = PR_Write(client_->ssl_fd(), big_message, short_length);
  }
  EXPECT_EQ(short_length, sent);

  // Even a single octet write should now fail.
  sent = PR_Write(client_->ssl_fd(), big_message, 1);
  EXPECT_GE(0, sent);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  // Process the ClientHello and read 0-RTT.
  server_->Handshake();
  CheckEarlyDataLimit(server_, short_size);

  std::vector<uint8_t> buf(short_size + 1);
  PRInt32 read = PR_Read(server_->ssl_fd(), buf.data(), buf.capacity());
  EXPECT_EQ(short_length, read);
  EXPECT_EQ(0, memcmp(big_message, buf.data(), short_size));

  // Second read fails.
  read = PR_Read(server_->ssl_fd(), buf.data(), buf.capacity());
  EXPECT_EQ(SECFailure, read);
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

static const uint16_t k0RttCipherDefs[] = {TLS_CHACHA20_POLY1305_SHA256,
                                           TLS_AES_128_GCM_SHA256,
                                           TLS_AES_256_GCM_SHA384};

static const uint16_t kDefaultSuite[] = {TLS_CHACHA20_POLY1305_SHA256};

INSTANTIATE_TEST_CASE_P(Tls13PskTest, Tls13PskTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                           ::testing::ValuesIn(kDefaultSuite)));

INSTANTIATE_TEST_CASE_P(
    Tls13PskTestWithCiphers, Tls13PskTestWithCiphers,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       ::testing::ValuesIn(k0RttCipherDefs)));

}  // namespace nss_test
