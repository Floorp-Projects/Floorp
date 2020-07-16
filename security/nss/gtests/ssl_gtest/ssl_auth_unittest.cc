/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
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

TEST_P(TlsConnectGeneric, ServerAuthBigRsa) {
  Reset(TlsAgent::kRsa2048);
  Connect();
  CheckKeys();
}

TEST_P(TlsConnectGeneric, ServerAuthRsaChain) {
  Reset("rsa_chain");
  Connect();
  CheckKeys();
  size_t chain_length;
  EXPECT_TRUE(client_->GetPeerChainLength(&chain_length));
  EXPECT_EQ(2UL, chain_length);
}

TEST_P(TlsConnectTls12Plus, ServerAuthRsaPss) {
  static const SSLSignatureScheme kSignatureSchemePss[] = {
      ssl_sig_rsa_pss_pss_sha256};

  Reset(TlsAgent::kServerRsaPss);
  client_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  server_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_rsa_pss,
            ssl_sig_rsa_pss_pss_sha256);
}

// PSS doesn't work with TLS 1.0 or 1.1 because we can't signal it.
TEST_P(TlsConnectPre12, ServerAuthRsaPssFails) {
  static const SSLSignatureScheme kSignatureSchemePss[] = {
      ssl_sig_rsa_pss_pss_sha256};

  Reset(TlsAgent::kServerRsaPss);
  client_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  server_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// Check that a PSS certificate with no parameters works.
TEST_P(TlsConnectTls12Plus, ServerAuthRsaPssNoParameters) {
  static const SSLSignatureScheme kSignatureSchemePss[] = {
      ssl_sig_rsa_pss_pss_sha256};

  Reset("rsa_pss_noparam");
  client_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  server_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_rsa_pss,
            ssl_sig_rsa_pss_pss_sha256);
}

TEST_P(TlsConnectGeneric, ServerAuthRsaPssChain) {
  Reset("rsa_pss_chain");
  Connect();
  CheckKeys();
  size_t chain_length;
  EXPECT_TRUE(client_->GetPeerChainLength(&chain_length));
  EXPECT_EQ(2UL, chain_length);
}

TEST_P(TlsConnectGeneric, ServerAuthRsaCARsaPssChain) {
  Reset("rsa_ca_rsa_pss_chain");
  Connect();
  CheckKeys();
  size_t chain_length;
  EXPECT_TRUE(client_->GetPeerChainLength(&chain_length));
  EXPECT_EQ(2UL, chain_length);
}

TEST_P(TlsConnectGeneric, ServerAuthRejected) {
  EnsureTlsSetup();
  client_->SetAuthCertificateCallback(
      [](TlsAgent*, PRBool, PRBool) -> SECStatus { return SECFailure; });
  ConnectExpectAlert(client_, kTlsAlertBadCertificate);
  client_->CheckErrorCode(SSL_ERROR_BAD_CERTIFICATE);
  server_->CheckErrorCode(SSL_ERROR_BAD_CERT_ALERT);
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
}

struct AuthCompleteArgs : public PollTarget {
  AuthCompleteArgs(const std::shared_ptr<TlsAgent>& a, PRErrorCode c)
      : agent(a), code(c) {}

  std::shared_ptr<TlsAgent> agent;
  PRErrorCode code;
};

static void CallAuthComplete(PollTarget* target, Event event) {
  EXPECT_EQ(TIMER_EVENT, event);
  auto args = reinterpret_cast<AuthCompleteArgs*>(target);
  std::cerr << args->agent->role_str() << ": call SSL_AuthCertificateComplete "
            << (args->code ? PR_ErrorToName(args->code) : "no error")
            << std::endl;
  EXPECT_EQ(SECSuccess,
            SSL_AuthCertificateComplete(args->agent->ssl_fd(), args->code));
  args->agent->Handshake();  // Make the TlsAgent aware of the error.
  delete args;
}

// Install an AuthCertificateCallback that blocks when called.  Then
// SSL_AuthCertificateComplete is called on a very short timer.  This allows any
// processing that might follow the callback to complete.
static void SetDeferredAuthCertificateCallback(std::shared_ptr<TlsAgent> agent,
                                               PRErrorCode code) {
  auto args = new AuthCompleteArgs(agent, code);
  agent->SetAuthCertificateCallback(
      [args](TlsAgent*, PRBool, PRBool) -> SECStatus {
        // This can't be 0 or we race the message from the client to the server,
        // and tests assume that we lose that race.
        std::shared_ptr<Poller::Timer> timer_handle;
        Poller::Instance()->SetTimer(1U, args, CallAuthComplete, &timer_handle);
        return SECWouldBlock;
      });
}

TEST_P(TlsConnectTls13, ServerAuthRejectAsync) {
  SetDeferredAuthCertificateCallback(client_, SEC_ERROR_REVOKED_CERTIFICATE);
  ConnectExpectAlert(client_, kTlsAlertCertificateRevoked);
  // We only detect the error here when we attempt to handshake, so all the
  // client learns is that the handshake has already failed.
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_FAILED);
  server_->CheckErrorCode(SSL_ERROR_REVOKED_CERT_ALERT);
}

// In TLS 1.2 and earlier, this will result in the client sending its Finished
// before learning that the server certificate is bad.  That means that the
// server will believe that the handshake is complete.
TEST_P(TlsConnectGenericPre13, ServerAuthRejectAsync) {
  SetDeferredAuthCertificateCallback(client_, SEC_ERROR_EXPIRED_CERTIFICATE);
  client_->ExpectSendAlert(kTlsAlertCertificateExpired);
  server_->ExpectReceiveAlert(kTlsAlertCertificateExpired);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_FAILED);

  // The server might not receive the alert that the client sends, which would
  // cause the test to fail when it cleans up.  Reset expectations.
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

TEST_P(TlsConnectGeneric, ClientAuth) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();
}

class TlsCertificateRequestContextRecorder : public TlsHandshakeFilter {
 public:
  TlsCertificateRequestContextRecorder(const std::shared_ptr<TlsAgent>& a,
                                       uint8_t handshake_type)
      : TlsHandshakeFilter(a, {handshake_type}), buffer_(), filtered_(false) {
    EnableDecryption();
  }

  bool filtered() const { return filtered_; }
  const DataBuffer& buffer() const { return buffer_; }

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    assert(1 < input.len());
    size_t len = input.data()[0];
    assert(len + 1 < input.len());
    buffer_.Assign(input.data() + 1, len);
    filtered_ = true;
    return KEEP;
  }

 private:
  DataBuffer buffer_;
  bool filtered_;
};

// All stream only tests; DTLS isn't supported yet.

TEST_F(TlsConnectStreamTls13, PostHandshakeAuth) {
  EnsureTlsSetup();
  auto capture_cert_req = MakeTlsFilter<TlsCertificateRequestContextRecorder>(
      server_, kTlsHandshakeCertificateRequest);
  auto capture_certificate =
      MakeTlsFilter<TlsCertificateRequestContextRecorder>(
          client_, kTlsHandshakeCertificate);
  client_->SetupClientAuth();
  client_->SetOption(SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE);
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  EXPECT_EQ(0U, called);
  EXPECT_FALSE(capture_cert_req->filtered());
  EXPECT_FALSE(capture_certificate->filtered());
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  // Need to do a round-trip so that the post-handshake message is
  // handled on both client and server.
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);
  EXPECT_EQ(1U, called);
  EXPECT_TRUE(capture_cert_req->filtered());
  EXPECT_TRUE(capture_certificate->filtered());
  // Check if a non-empty request context is generated and it is
  // properly sent back.
  EXPECT_LT(0U, capture_cert_req->buffer().len());
  EXPECT_EQ(capture_cert_req->buffer().len(),
            capture_certificate->buffer().len());
  EXPECT_EQ(0, memcmp(capture_cert_req->buffer().data(),
                      capture_certificate->buffer().data(),
                      capture_cert_req->buffer().len()));
  ScopedCERTCertificate cert1(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert1.get());
  ScopedCERTCertificate cert2(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert2.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthAfterResumption) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();

  SendReceive();  // Need to read so that we absorb the session tickets.
  CheckKeys();

  // Resume the connection.
  Reset();

  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ExpectResumption(RESUME_TICKET);

  client_->SetupClientAuth();
  client_->SetOption(SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE);
  Connect();
  SendReceive();

  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);
  EXPECT_EQ(1U, called);

  ScopedCERTCertificate cert1(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert1.get());
  ScopedCERTCertificate cert2(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert2.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
}

static SECStatus GetClientAuthDataHook(void* self, PRFileDesc* fd,
                                       CERTDistNames* caNames,
                                       CERTCertificate** clientCert,
                                       SECKEYPrivateKey** clientKey) {
  ScopedCERTCertificate cert;
  ScopedSECKEYPrivateKey priv;
  // use a different certificate than TlsAgent::kClient
  if (!TlsAgent::LoadCertificate(TlsAgent::kRsa2048, &cert, &priv)) {
    return SECFailure;
  }

  *clientCert = cert.release();
  *clientKey = priv.release();
  return SECSuccess;
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthMultiple) {
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  EXPECT_EQ(0U, called);
  EXPECT_EQ(nullptr, SSL_PeerCertificate(server_->ssl_fd()));
  // Send 1st CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);
  EXPECT_EQ(1U, called);
  ScopedCERTCertificate cert1(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert1.get());
  ScopedCERTCertificate cert2(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert2.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
  // Send 2nd CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_GetClientAuthDataHook(
                            client_->ssl_fd(), GetClientAuthDataHook, nullptr));
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);
  EXPECT_EQ(2U, called);
  ScopedCERTCertificate cert3(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert3.get());
  ScopedCERTCertificate cert4(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert4.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert3->derCert, &cert4->derCert));
  EXPECT_FALSE(SECITEM_ItemsAreEqual(&cert3->derCert, &cert1->derCert));
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthConcurrent) {
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  Connect();
  // Send 1st CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  // Send 2nd CertificateRequest.
  EXPECT_EQ(SECFailure, SSL_SendCertificateRequest(server_->ssl_fd()));
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthBeforeKeyUpdate) {
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  Connect();
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  // Send KeyUpdate.
  EXPECT_EQ(SECFailure, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthDuringClientKeyUpdate) {
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  Connect();
  CheckEpochs(3, 3);
  // Send CertificateRequest from server.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  // Send KeyUpdate from client.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_TRUE));
  server_->SendData(50);   // server sends CertificateRequest
  client_->SendData(50);   // client sends KeyUpdate
  server_->ReadBytes(50);  // server receives KeyUpdate and defers response
  CheckEpochs(4, 3);
  client_->ReadBytes(50);  // client receives CertificateRequest
  client_->SendData(
      50);  // client sends Certificate, CertificateVerify, Finished
  server_->ReadBytes(
      50);  // server receives Certificate, CertificateVerify, Finished
  client_->CheckEpochs(3, 4);
  server_->CheckEpochs(4, 4);
  server_->SendData(50);   // server sends KeyUpdate
  client_->ReadBytes(50);  // client receives KeyUpdate
  client_->CheckEpochs(4, 4);
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthMissingExtension) {
  client_->SetupClientAuth();
  Connect();
  // Send CertificateRequest, should fail due to missing
  // post_handshake_auth extension.
  EXPECT_EQ(SECFailure, SSL_SendCertificateRequest(server_->ssl_fd()));
  EXPECT_EQ(SSL_ERROR_MISSING_POST_HANDSHAKE_AUTH_EXTENSION, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthAfterClientAuth) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  EXPECT_EQ(1U, called);
  ScopedCERTCertificate cert1(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert1.get());
  ScopedCERTCertificate cert2(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert2.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_GetClientAuthDataHook(
                            client_->ssl_fd(), GetClientAuthDataHook, nullptr));
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);
  EXPECT_EQ(2U, called);
  ScopedCERTCertificate cert3(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert3.get());
  ScopedCERTCertificate cert4(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert4.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert3->derCert, &cert4->derCert));
  EXPECT_FALSE(SECITEM_ItemsAreEqual(&cert3->derCert, &cert1->derCert));
}

// Damages the request context in a CertificateRequest message.
// We don't modify a Certificate message instead, so that the client
// can compute CertificateVerify correctly.
class TlsDamageCertificateRequestContextFilter : public TlsHandshakeFilter {
 public:
  TlsDamageCertificateRequestContextFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeCertificateRequest}) {
    EnableDecryption();
  }

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    *output = input;
    assert(1 < output->len());
    // The request context has a 1 octet length.
    output->data()[1] ^= 73;
    return CHANGE;
  }
};

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthContextMismatch) {
  EnsureTlsSetup();
  MakeTlsFilter<TlsDamageCertificateRequestContextFilter>(server_);
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  Connect();
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ExpectSendAlert(kTlsAlertIllegalParameter);
  server_->ReadBytes(50);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CERTIFICATE, PORT_GetError());
  server_->ExpectReadWriteError();
  server_->SendData(50);
  client_->ExpectReceiveAlert(kTlsAlertIllegalParameter);
  client_->ReadBytes(50);
  EXPECT_EQ(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, PORT_GetError());
}

// Replaces signature in a CertificateVerify message.
class TlsDamageSignatureFilter : public TlsHandshakeFilter {
 public:
  TlsDamageSignatureFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeCertificateVerify}) {
    EnableDecryption();
  }

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    *output = input;
    assert(2 < output->len());
    // The signature follows a 2-octet signature scheme.
    output->data()[2] ^= 73;
    return CHANGE;
  }
};

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthBadSignature) {
  EnsureTlsSetup();
  MakeTlsFilter<TlsDamageSignatureFilter>(client_);
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  Connect();
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ExpectSendAlert(kTlsAlertDecodeError);
  server_->ReadBytes(50);
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_CERT_VERIFY, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, PostHandshakeAuthDecline) {
  EnsureTlsSetup();
  auto capture_cert_req = MakeTlsFilter<TlsCertificateRequestContextRecorder>(
      server_, kTlsHandshakeCertificateRequest);
  auto capture_certificate =
      MakeTlsFilter<TlsCertificateRequestContextRecorder>(
          client_, kTlsHandshakeCertificate);
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(server_->ssl_fd(), SSL_REQUIRE_CERTIFICATE,
                          SSL_REQUIRE_ALWAYS));
  // Client to decline the certificate request.
  EXPECT_EQ(SECSuccess,
            SSL_GetClientAuthDataHook(
                client_->ssl_fd(),
                [](void*, PRFileDesc*, CERTDistNames*, CERTCertificate**,
                   SECKEYPrivateKey**) -> SECStatus { return SECFailure; },
                nullptr));
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  EXPECT_EQ(0U, called);
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);   // send Certificate Request
  client_->ReadBytes(50);  // read Certificate Request
  client_->SendData(50);   // send empty Certificate+Finished
  server_->ExpectSendAlert(kTlsAlertCertificateRequired);
  server_->ReadBytes(50);  // read empty Certificate+Finished
  server_->ExpectReadWriteError();
  server_->SendData(50);  // send alert
  // AuthCertificateCallback is not called, because the client sends
  // an empty certificate_list.
  EXPECT_EQ(0U, called);
  EXPECT_TRUE(capture_cert_req->filtered());
  EXPECT_TRUE(capture_certificate->filtered());
  // Check if a non-empty request context is generated and it is
  // properly sent back.
  EXPECT_LT(0U, capture_cert_req->buffer().len());
  EXPECT_EQ(capture_cert_req->buffer().len(),
            capture_certificate->buffer().len());
  EXPECT_EQ(0, memcmp(capture_cert_req->buffer().data(),
                      capture_certificate->buffer().data(),
                      capture_cert_req->buffer().len()));
}

// Check if post-handshake auth still works when session tickets are enabled:
// https://bugzilla.mozilla.org/show_bug.cgi?id=1553443
TEST_F(TlsConnectStreamTls13, PostHandshakeAuthWithSessionTicketsEnabled) {
  EnsureTlsSetup();
  client_->SetupClientAuth();
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_OptionSet(client_->ssl_fd(),
                                      SSL_ENABLE_SESSION_TICKETS, PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_OptionSet(server_->ssl_fd(),
                                      SSL_ENABLE_SESSION_TICKETS, PR_TRUE));
  size_t called = 0;
  server_->SetAuthCertificateCallback(
      [&called](TlsAgent*, PRBool, PRBool) -> SECStatus {
        called++;
        return SECSuccess;
      });
  Connect();
  EXPECT_EQ(0U, called);
  // Send CertificateRequest.
  EXPECT_EQ(SECSuccess, SSL_GetClientAuthDataHook(
                            client_->ssl_fd(), GetClientAuthDataHook, nullptr));
  EXPECT_EQ(SECSuccess, SSL_SendCertificateRequest(server_->ssl_fd()))
      << "Unexpected error: " << PORT_ErrorToName(PORT_GetError());
  server_->SendData(50);
  client_->ReadBytes(50);
  client_->SendData(50);
  server_->ReadBytes(50);
  EXPECT_EQ(1U, called);
  ScopedCERTCertificate cert1(SSL_PeerCertificate(server_->ssl_fd()));
  ASSERT_NE(nullptr, cert1.get());
  ScopedCERTCertificate cert2(SSL_LocalCertificate(client_->ssl_fd()));
  ASSERT_NE(nullptr, cert2.get());
  EXPECT_TRUE(SECITEM_ItemsAreEqual(&cert1->derCert, &cert2->derCert));
}

TEST_P(TlsConnectGenericPre13, ClientAuthRequiredRejected) {
  server_->RequestClientAuth(true);
  ConnectExpectAlert(server_, kTlsAlertBadCertificate);
  client_->CheckErrorCode(SSL_ERROR_BAD_CERT_ALERT);
  server_->CheckErrorCode(SSL_ERROR_NO_CERTIFICATE);
}

// In TLS 1.3, the client will claim that the connection is done and then
// receive the alert afterwards.  So drive the handshake manually.
TEST_P(TlsConnectTls13, ClientAuthRequiredRejected) {
  server_->RequestClientAuth(true);
  StartConnect();
  client_->Handshake();  // CH
  server_->Handshake();  // SH.. (no resumption)
  client_->Handshake();  // Next message
  ASSERT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  ExpectAlert(server_, kTlsAlertCertificateRequired);
  server_->Handshake();  // Alert
  server_->CheckErrorCode(SSL_ERROR_NO_CERTIFICATE);
  client_->Handshake();  // Receive Alert
  client_->CheckErrorCode(SSL_ERROR_RX_CERTIFICATE_REQUIRED_ALERT);
}

TEST_P(TlsConnectGeneric, ClientAuthRequestedRejected) {
  server_->RequestClientAuth(false);
  Connect();
  CheckKeys();
}

TEST_P(TlsConnectGeneric, ClientAuthEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_auth_ecdsa);
}

TEST_P(TlsConnectGeneric, ClientAuthBigRsa) {
  Reset(TlsAgent::kServerRsa, TlsAgent::kRsa2048);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();
}

// Offset is the position in the captured buffer where the signature sits.
static void CheckSigScheme(std::shared_ptr<TlsHandshakeRecorder>& capture,
                           size_t offset, std::shared_ptr<TlsAgent>& peer,
                           uint16_t expected_scheme, size_t expected_size) {
  EXPECT_LT(offset + 2U, capture->buffer().len());

  uint32_t scheme = 0;
  capture->buffer().Read(offset, 2, &scheme);
  EXPECT_EQ(expected_scheme, static_cast<uint16_t>(scheme));

  ScopedCERTCertificate remote_cert(SSL_PeerCertificate(peer->ssl_fd()));
  ASSERT_NE(nullptr, remote_cert.get());
  ScopedSECKEYPublicKey remote_key(CERT_ExtractPublicKey(remote_cert.get()));
  ASSERT_NE(nullptr, remote_key.get());
  EXPECT_EQ(expected_size, SECKEY_PublicKeyStrengthInBits(remote_key.get()));
}

// The server should prefer SHA-256 by default, even for the small key size used
// in the default certificate.
TEST_P(TlsConnectTls12, ServerAuthCheckSigAlg) {
  EnsureTlsSetup();
  auto capture_ske = MakeTlsFilter<TlsHandshakeRecorder>(
      server_, kTlsHandshakeServerKeyExchange);
  Connect();
  CheckKeys();

  const DataBuffer& buffer = capture_ske->buffer();
  EXPECT_LT(3U, buffer.len());
  EXPECT_EQ(3U, buffer.data()[0]) << "curve_type == named_curve";
  uint32_t tmp;
  EXPECT_TRUE(buffer.Read(1, 2, &tmp)) << "read NamedCurve";
  EXPECT_EQ(ssl_grp_ec_curve25519, tmp);
  EXPECT_TRUE(buffer.Read(3, 1, &tmp)) << " read ECPoint";
  CheckSigScheme(capture_ske, 4 + tmp, client_, ssl_sig_rsa_pss_rsae_sha256,
                 1024);
}

TEST_P(TlsConnectTls12, ClientAuthCheckSigAlg) {
  EnsureTlsSetup();
  auto capture_cert_verify = MakeTlsFilter<TlsHandshakeRecorder>(
      client_, kTlsHandshakeCertificateVerify);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();

  CheckSigScheme(capture_cert_verify, 0, server_, ssl_sig_rsa_pkcs1_sha1, 1024);
}

TEST_P(TlsConnectTls12, ClientAuthBigRsaCheckSigAlg) {
  Reset(TlsAgent::kServerRsa, TlsAgent::kRsa2048);
  auto capture_cert_verify = MakeTlsFilter<TlsHandshakeRecorder>(
      client_, kTlsHandshakeCertificateVerify);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();
  CheckSigScheme(capture_cert_verify, 0, server_, ssl_sig_rsa_pss_rsae_sha256,
                 2048);
}

// Replaces the signature scheme in a CertificateVerify message.
class TlsReplaceSignatureSchemeFilter : public TlsHandshakeFilter {
 public:
  TlsReplaceSignatureSchemeFilter(const std::shared_ptr<TlsAgent>& a,
                                  SSLSignatureScheme scheme)
      : TlsHandshakeFilter(a, {kTlsHandshakeCertificateVerify}),
        scheme_(scheme) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    *output = input;
    output->Write(0, scheme_, 2);
    return CHANGE;
  }

 private:
  SSLSignatureScheme scheme_;
};

// Check if CertificateVerify signed with rsa_pss_rsae_* is properly
// rejected when the certificate is RSA-PSS.
//
// This only works under TLS 1.2, because PSS doesn't work with TLS
// 1.0 or TLS 1.1 and the TLS 1.3 1-RTT handshake is partially
// successful at the client side.
TEST_P(TlsConnectTls12, ClientAuthInconsistentRsaeSignatureScheme) {
  static const SSLSignatureScheme kSignatureSchemePss[] = {
      ssl_sig_rsa_pss_pss_sha256, ssl_sig_rsa_pss_rsae_sha256};

  Reset(TlsAgent::kServerRsa, "rsa_pss");
  client_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  server_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  EnsureTlsSetup();

  MakeTlsFilter<TlsReplaceSignatureSchemeFilter>(client_,
                                                 ssl_sig_rsa_pss_rsae_sha256);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
}

// Check if CertificateVerify signed with rsa_pss_pss_* is properly
// rejected when the certificate is RSA.
//
// This only works under TLS 1.2, because PSS doesn't work with TLS
// 1.0 or TLS 1.1 and the TLS 1.3 1-RTT handshake is partially
// successful at the client side.
TEST_P(TlsConnectTls12, ClientAuthInconsistentPssSignatureScheme) {
  static const SSLSignatureScheme kSignatureSchemePss[] = {
      ssl_sig_rsa_pss_rsae_sha256, ssl_sig_rsa_pss_pss_sha256};

  Reset(TlsAgent::kServerRsa, "rsa");
  client_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  server_->SetSignatureSchemes(kSignatureSchemePss,
                               PR_ARRAY_SIZE(kSignatureSchemePss));
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  EnsureTlsSetup();

  MakeTlsFilter<TlsReplaceSignatureSchemeFilter>(client_,
                                                 ssl_sig_rsa_pss_pss_sha256);

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
}

TEST_P(TlsConnectTls13, ClientAuthPkcs1SignatureScheme) {
  static const SSLSignatureScheme kSignatureScheme[] = {
      ssl_sig_rsa_pkcs1_sha256, ssl_sig_rsa_pss_rsae_sha256};

  Reset(TlsAgent::kServerRsa, "rsa");
  client_->SetSignatureSchemes(kSignatureScheme,
                               PR_ARRAY_SIZE(kSignatureScheme));
  server_->SetSignatureSchemes(kSignatureScheme,
                               PR_ARRAY_SIZE(kSignatureScheme));
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  auto capture_cert_verify = MakeTlsFilter<TlsHandshakeRecorder>(
      client_, kTlsHandshakeCertificateVerify);
  capture_cert_verify->EnableDecryption();

  Connect();
  CheckSigScheme(capture_cert_verify, 0, server_, ssl_sig_rsa_pss_rsae_sha256,
                 1024);
}

// Client should refuse to connect without a usable signature scheme.
TEST_P(TlsConnectTls13, ClientAuthPkcs1SignatureSchemeOnly) {
  static const SSLSignatureScheme kSignatureScheme[] = {
      ssl_sig_rsa_pkcs1_sha256};

  Reset(TlsAgent::kServerRsa, "rsa");
  client_->SetSignatureSchemes(kSignatureScheme,
                               PR_ARRAY_SIZE(kSignatureScheme));
  client_->SetupClientAuth();
  client_->StartConnect();
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  client_->CheckErrorCode(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
}

// Though the client has a usable signature scheme, when a certificate is
// requested, it can't produce one.
TEST_P(TlsConnectTls13, ClientAuthPkcs1AndEcdsaScheme) {
  static const SSLSignatureScheme kSignatureScheme[] = {
      ssl_sig_rsa_pkcs1_sha256, ssl_sig_ecdsa_secp256r1_sha256};

  Reset(TlsAgent::kServerRsa, "rsa");
  client_->SetSignatureSchemes(kSignatureScheme,
                               PR_ARRAY_SIZE(kSignatureScheme));
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

class TlsZeroCertificateRequestSigAlgsFilter : public TlsHandshakeFilter {
 public:
  TlsZeroCertificateRequestSigAlgsFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {kTlsHandshakeCertificateRequest}) {}
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    TlsParser parser(input);
    std::cerr << "Zeroing CertReq.supported_signature_algorithms" << std::endl;

    DataBuffer cert_types;
    if (!parser.ReadVariable(&cert_types, 1)) {
      ADD_FAILURE();
      return KEEP;
    }

    if (!parser.SkipVariable(2)) {
      ADD_FAILURE();
      return KEEP;
    }

    DataBuffer cas;
    if (!parser.ReadVariable(&cas, 2)) {
      ADD_FAILURE();
      return KEEP;
    }

    size_t idx = 0;

    // Write certificate types.
    idx = output->Write(idx, cert_types.len(), 1);
    idx = output->Write(idx, cert_types);

    // Write zero signature algorithms.
    idx = output->Write(idx, 0U, 2);

    // Write certificate authorities.
    idx = output->Write(idx, cas.len(), 2);
    idx = output->Write(idx, cas);

    return CHANGE;
  }
};

// Check that we send an alert when the server doesn't provide any
// supported_signature_algorithms in the CertificateRequest message.
TEST_P(TlsConnectTls12, ClientAuthNoSigAlgs) {
  EnsureTlsSetup();
  MakeTlsFilter<TlsZeroCertificateRequestSigAlgsFilter>(server_);
  auto capture_cert_verify = MakeTlsFilter<TlsHandshakeRecorder>(
      client_, kTlsHandshakeCertificateVerify);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);

  ConnectExpectAlert(client_, kTlsAlertHandshakeFailure);

  server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_FAILURE_ALERT);
  client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
}

static const SSLSignatureScheme kSignatureSchemeEcdsaSha384[] = {
    ssl_sig_ecdsa_secp384r1_sha384};
static const SSLSignatureScheme kSignatureSchemeEcdsaSha256[] = {
    ssl_sig_ecdsa_secp256r1_sha256};
static const SSLSignatureScheme kSignatureSchemeRsaSha384[] = {
    ssl_sig_rsa_pkcs1_sha384};
static const SSLSignatureScheme kSignatureSchemeRsaSha256[] = {
    ssl_sig_rsa_pkcs1_sha256};

static SSLNamedGroup NamedGroupForEcdsa384(uint16_t version) {
  // NSS tries to match the group size to the symmetric cipher. In TLS 1.1 and
  // 1.0, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA is the highest priority suite, so
  // we use P-384. With TLS 1.2 on we pick AES-128 GCM so use x25519.
  if (version <= SSL_LIBRARY_VERSION_TLS_1_1) {
    return ssl_grp_ec_secp384r1;
  }
  return ssl_grp_ec_curve25519;
}

// When signature algorithms match up, this should connect successfully; even
// for TLS 1.1 and 1.0, where they should be ignored.
TEST_P(TlsConnectGeneric, SignatureAlgorithmServerAuth) {
  Reset(TlsAgent::kServerEcdsa384);
  client_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  server_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  Connect();
  CheckKeys(ssl_kea_ecdh, NamedGroupForEcdsa384(version_), ssl_auth_ecdsa,
            ssl_sig_ecdsa_secp384r1_sha384);
}

// Here the client picks a single option, which should work in all versions.
// Defaults on the server include the first option.
TEST_P(TlsConnectGeneric, SignatureAlgorithmClientOnly) {
  const SSLSignatureAndHashAlg clientAlgorithms[] = {
      {ssl_hash_sha384, ssl_sign_ecdsa},
      {ssl_hash_sha384, ssl_sign_rsa},  // supported but unusable
      {ssl_hash_md5, ssl_sign_ecdsa}    // unsupported and ignored
  };
  Reset(TlsAgent::kServerEcdsa384);
  EnsureTlsSetup();
  // Use the old API for this function.
  EXPECT_EQ(SECSuccess,
            SSL_SignaturePrefSet(client_->ssl_fd(), clientAlgorithms,
                                 PR_ARRAY_SIZE(clientAlgorithms)));
  Connect();
  CheckKeys(ssl_kea_ecdh, NamedGroupForEcdsa384(version_), ssl_auth_ecdsa,
            ssl_sig_ecdsa_secp384r1_sha384);
}

// Here the server picks a single option, which should work in all versions.
// Defaults on the client include the provided option.
TEST_P(TlsConnectGeneric, SignatureAlgorithmServerOnly) {
  Reset(TlsAgent::kServerEcdsa384);
  server_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  Connect();
  CheckKeys(ssl_kea_ecdh, NamedGroupForEcdsa384(version_), ssl_auth_ecdsa,
            ssl_sig_ecdsa_secp384r1_sha384);
}

// In TLS 1.2, curve and hash aren't bound together.
TEST_P(TlsConnectTls12, SignatureSchemeCurveMismatch) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  Connect();
}

// In TLS 1.3, curve and hash are coupled.
TEST_P(TlsConnectTls13, SignatureSchemeCurveMismatch) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// Configuring a P-256 cert with only SHA-384 signatures is OK in TLS 1.2.
TEST_P(TlsConnectTls12, SignatureSchemeBadConfig) {
  Reset(TlsAgent::kServerEcdsa256);  // P-256 cert can't be used.
  server_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  Connect();
}

// A P-256 certificate in TLS 1.3 needs a SHA-256 signature scheme.
TEST_P(TlsConnectTls13, SignatureSchemeBadConfig) {
  Reset(TlsAgent::kServerEcdsa256);  // P-256 cert can't be used.
  server_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// Where there is no overlap on signature schemes, we still connect successfully
// if we aren't going to use a signature.
TEST_P(TlsConnectGenericPre13, SignatureAlgorithmNoOverlapStaticRsa) {
  client_->SetSignatureSchemes(kSignatureSchemeRsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeRsaSha384));
  server_->SetSignatureSchemes(kSignatureSchemeRsaSha256,
                               PR_ARRAY_SIZE(kSignatureSchemeRsaSha256));
  EnableOnlyStaticRsaCiphers();
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_auth_rsa_decrypt);
}

TEST_P(TlsConnectTls12Plus, SignatureAlgorithmNoOverlapEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  server_->SetSignatureSchemes(kSignatureSchemeEcdsaSha256,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha256));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
}

// Pre 1.2, a mismatch on signature algorithms shouldn't affect anything.
TEST_P(TlsConnectPre12, SignatureAlgorithmNoOverlapEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(kSignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha384));
  server_->SetSignatureSchemes(kSignatureSchemeEcdsaSha256,
                               PR_ARRAY_SIZE(kSignatureSchemeEcdsaSha256));
  Connect();
}

// The signature_algorithms extension is mandatory in TLS 1.3.
TEST_P(TlsConnectTls13, SignatureAlgorithmDrop) {
  MakeTlsFilter<TlsExtensionDropper>(client_, ssl_signature_algorithms_xtn);
  ConnectExpectAlert(server_, kTlsAlertMissingExtension);
  client_->CheckErrorCode(SSL_ERROR_MISSING_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_MISSING_SIGNATURE_ALGORITHMS_EXTENSION);
}

// TLS 1.2 has trouble detecting this sort of modification: it uses SHA1 and
// only fails when the Finished is checked.
TEST_P(TlsConnectTls12, SignatureAlgorithmDrop) {
  MakeTlsFilter<TlsExtensionDropper>(client_, ssl_signature_algorithms_xtn);
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

TEST_P(TlsConnectTls13, UnsupportedSignatureSchemeAlert) {
  EnsureTlsSetup();
  auto filter =
      MakeTlsFilter<TlsReplaceSignatureSchemeFilter>(server_, ssl_sig_none);
  filter->EnableDecryption();

  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CERT_VERIFY);
}

TEST_P(TlsConnectTls13, InconsistentSignatureSchemeAlert) {
  EnsureTlsSetup();

  // This won't work because we use an RSA cert by default.
  auto filter = MakeTlsFilter<TlsReplaceSignatureSchemeFilter>(
      server_, ssl_sig_ecdsa_secp256r1_sha256);
  filter->EnableDecryption();

  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  client_->CheckErrorCode(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM);
}

TEST_P(TlsConnectTls12, RequestClientAuthWithSha384) {
  server_->SetSignatureSchemes(kSignatureSchemeRsaSha384,
                               PR_ARRAY_SIZE(kSignatureSchemeRsaSha384));
  server_->RequestClientAuth(false);
  Connect();
}

class BeforeFinished : public TlsRecordFilter {
 private:
  enum HandshakeState { BEFORE_CCS, AFTER_CCS, DONE };

 public:
  BeforeFinished(const std::shared_ptr<TlsAgent>& server,
                 const std::shared_ptr<TlsAgent>& client,
                 VoidFunction before_ccs, VoidFunction before_finished)
      : TlsRecordFilter(server),
        client_(client),
        before_ccs_(before_ccs),
        before_finished_(before_finished),
        state_(BEFORE_CCS) {}

 protected:
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& body,
                                            DataBuffer* out) {
    switch (state_) {
      case BEFORE_CCS:
        // Awaken when we see the CCS.
        if (header.content_type() == ssl_ct_change_cipher_spec) {
          before_ccs_();

          // Write the CCS out as a separate write, so that we can make
          // progress. Ordinarily, libssl sends the CCS and Finished together,
          // but that means that they both get processed together.
          DataBuffer ccs;
          header.Write(&ccs, 0, body);
          agent()->SendDirect(ccs);
          client_.lock()->Handshake();
          state_ = AFTER_CCS;
          // Request that the original record be dropped by the filter.
          return DROP;
        }
        break;

      case AFTER_CCS:
        EXPECT_EQ(ssl_ct_handshake, header.content_type());
        // This could check that data contains a Finished message, but it's
        // encrypted, so that's too much extra work.

        before_finished_();
        state_ = DONE;
        break;

      case DONE:
        break;
    }
    return KEEP;
  }

 private:
  std::weak_ptr<TlsAgent> client_;
  VoidFunction before_ccs_;
  VoidFunction before_finished_;
  HandshakeState state_;
};

// Running code after the client has started processing the encrypted part of
// the server's first flight, but before the Finished is processed is very hard
// in TLS 1.3.  These encrypted messages are sent in a single encrypted blob.
// The following test uses DTLS to make it possible to force the client to
// process the handshake in pieces.
//
// The first encrypted message from the server is dropped, and the MTU is
// reduced to just below the original message size so that the server sends two
// messages.  The Finished message is then processed separately.
class BeforeFinished13 : public PacketFilter {
 private:
  enum HandshakeState {
    INIT,
    BEFORE_FIRST_FRAGMENT,
    BEFORE_SECOND_FRAGMENT,
    DONE
  };

 public:
  BeforeFinished13(const std::shared_ptr<TlsAgent>& server,
                   const std::shared_ptr<TlsAgent>& client,
                   VoidFunction before_finished)
      : server_(server),
        client_(client),
        before_finished_(before_finished),
        records_(0) {}

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output) {
    switch (++records_) {
      case 1:
        // Packet 1 is the server's entire first flight.  Drop it.
        EXPECT_EQ(SECSuccess,
                  SSLInt_SetMTU(server_.lock()->ssl_fd(), input.len() - 1));
        return DROP;

      // Packet 2 is the first part of the server's retransmitted first
      // flight.  Keep that.

      case 3:
        // Packet 3 is the second part of the server's retransmitted first
        // flight.  Before passing that on, make sure that the client processes
        // packet 2, then call the before_finished_() callback.
        client_.lock()->Handshake();
        before_finished_();
        break;

      default:
        break;
    }
    return KEEP;
  }

 private:
  std::weak_ptr<TlsAgent> server_;
  std::weak_ptr<TlsAgent> client_;
  VoidFunction before_finished_;
  size_t records_;
};

static SECStatus AuthCompleteBlock(TlsAgent*, PRBool, PRBool) {
  return SECWouldBlock;
}

// This test uses an AuthCertificateCallback that blocks.  A filter is used to
// split the server's first flight into two pieces.  Before the second piece is
// processed by the client, SSL_AuthCertificateComplete() is called.
TEST_F(TlsConnectDatagram13, AuthCompleteBeforeFinished) {
  client_->SetAuthCertificateCallback(AuthCompleteBlock);
  MakeTlsFilter<BeforeFinished13>(server_, client_, [this]() {
    EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  });
  Connect();
}

// This test uses a simple AuthCertificateCallback.  Due to the way that the
// entire server flight is processed, the call to SSL_AuthCertificateComplete
// will trigger after the Finished message is processed.
TEST_F(TlsConnectDatagram13, AuthCompleteAfterFinished) {
  SetDeferredAuthCertificateCallback(client_, 0);  // 0 = success.
  Connect();
}

TEST_P(TlsConnectGenericPre13, ClientWriteBetweenCCSAndFinishedWithFalseStart) {
  client_->EnableFalseStart();
  MakeTlsFilter<BeforeFinished>(
      server_, client_,
      [this]() { EXPECT_TRUE(client_->can_falsestart_hook_called()); },
      [this]() {
        // Write something, which used to fail: bug 1235366.
        client_->SendData(10);
      });

  Connect();
  server_->SendData(10);
  Receive(10);
}

TEST_P(TlsConnectGenericPre13, AuthCompleteBeforeFinishedWithFalseStart) {
  client_->EnableFalseStart();
  client_->SetAuthCertificateCallback(AuthCompleteBlock);
  MakeTlsFilter<BeforeFinished>(
      server_, client_,
      []() {
        // Do nothing before CCS
      },
      [this]() {
        EXPECT_FALSE(client_->can_falsestart_hook_called());
        // AuthComplete before Finished still enables false start.
        EXPECT_EQ(SECSuccess,
                  SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
        EXPECT_TRUE(client_->can_falsestart_hook_called());
        client_->SendData(10);
      });

  Connect();
  server_->SendData(10);
  Receive(10);
}

class EnforceNoActivity : public PacketFilter {
 protected:
  PacketFilter::Action Filter(const DataBuffer& input,
                              DataBuffer* output) override {
    std::cerr << "Unexpected packet: " << input << std::endl;
    EXPECT_TRUE(false) << "should not send anything";
    return KEEP;
  }
};

// In this test, we want to make sure that the server completes its handshake,
// but the client does not.  Because the AuthCertificate callback blocks and we
// never call SSL_AuthCertificateComplete(), the client should never report that
// it has completed the handshake.  Manually call Handshake(), alternating sides
// between client and server, until the desired state is reached.
TEST_P(TlsConnectGenericPre13, AuthCompleteDelayed) {
  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send ServerHello
  client_->Handshake();  // Send ClientKeyExchange and Finished
  server_->Handshake();  // Send Finished
  // The server should now report that it is connected
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());

  // The client should send nothing from here on.
  client_->SetFilter(std::make_shared<EnforceNoActivity>());
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());

  // This should allow the handshake to complete now.
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  client_->Handshake();  // Transition to connected
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());

  // Remove filter before closing or the close_notify alert will trigger it.
  client_->ClearFilter();
}

TEST_P(TlsConnectGenericPre13, AuthCompleteFailDelayed) {
  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send ServerHello
  client_->Handshake();  // Send ClientKeyExchange and Finished
  server_->Handshake();  // Send Finished
  // The server should now report that it is connected
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());

  // The client should send nothing from here on.
  client_->SetFilter(std::make_shared<EnforceNoActivity>());
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());

  // Report failure.
  client_->ClearFilter();
  client_->ExpectSendAlert(kTlsAlertBadCertificate);
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(),
                                                    SSL_ERROR_BAD_CERTIFICATE));
  client_->Handshake();  // Fail
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
}

// TLS 1.3 handles a delayed AuthComplete callback differently since the
// shape of the handshake is different.
TEST_P(TlsConnectTls13, AuthCompleteDelayed) {
  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send ServerHello
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());

  // The client will send nothing until AuthCertificateComplete is called.
  client_->SetFilter(std::make_shared<EnforceNoActivity>());
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());

  // This should allow the handshake to complete now.
  client_->ClearFilter();
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  client_->Handshake();  // Send Finished
  server_->Handshake();  // Transition to connected and send NewSessionTicket
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
}

TEST_P(TlsConnectTls13, AuthCompleteFailDelayed) {
  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send ServerHello
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());

  // The client will send nothing until AuthCertificateComplete is called.
  client_->SetFilter(std::make_shared<EnforceNoActivity>());
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());

  // Report failure.
  client_->ClearFilter();
  ExpectAlert(client_, kTlsAlertBadCertificate);
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(),
                                                    SSL_ERROR_BAD_CERTIFICATE));
  client_->Handshake();  // This should now fail.
  server_->Handshake();  // Get the error.
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  EXPECT_EQ(TlsAgent::STATE_ERROR, server_->state());
}

static SECStatus AuthCompleteFail(TlsAgent*, PRBool, PRBool) {
  PORT_SetError(SSL_ERROR_BAD_CERTIFICATE);
  return SECFailure;
}

TEST_P(TlsConnectGeneric, AuthFailImmediate) {
  client_->SetAuthCertificateCallback(AuthCompleteFail);

  StartConnect();
  ConnectExpectAlert(client_, kTlsAlertBadCertificate);
  client_->CheckErrorCode(SSL_ERROR_BAD_CERTIFICATE);
}

static const SSLExtraServerCertData ServerCertDataRsaPkcs1Decrypt = {
    ssl_auth_rsa_decrypt, nullptr, nullptr, nullptr, nullptr, nullptr};
static const SSLExtraServerCertData ServerCertDataRsaPkcs1Sign = {
    ssl_auth_rsa_sign, nullptr, nullptr, nullptr, nullptr, nullptr};
static const SSLExtraServerCertData ServerCertDataRsaPss = {
    ssl_auth_rsa_pss, nullptr, nullptr, nullptr, nullptr, nullptr};

// Test RSA cert with usage=[signature, encipherment].
TEST_F(TlsAgentStreamTestServer, ConfigureCertRsaPkcs1SignAndKEX) {
  Reset(TlsAgent::kServerRsa);

  PRFileDesc* ssl_fd = agent_->ssl_fd();
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_decrypt));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_sign));
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_pss));

  // Configuring for only rsa_sign or rsa_decrypt should work.
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsa, false,
                                       &ServerCertDataRsaPkcs1Decrypt));
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsa, false,
                                       &ServerCertDataRsaPkcs1Sign));
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsa, false,
                                        &ServerCertDataRsaPss));
}

// Test RSA cert with usage=[signature].
TEST_F(TlsAgentStreamTestServer, ConfigureCertRsaPkcs1Sign) {
  Reset(TlsAgent::kServerRsaSign);

  PRFileDesc* ssl_fd = agent_->ssl_fd();
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_decrypt));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_sign));
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_pss));

  // Configuring for only rsa_decrypt should fail.
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaSign, false,
                                        &ServerCertDataRsaPkcs1Decrypt));

  // Configuring for only rsa_sign should work.
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsaSign, false,
                                       &ServerCertDataRsaPkcs1Sign));
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaSign, false,
                                        &ServerCertDataRsaPss));
}

// Test RSA cert with usage=[encipherment].
TEST_F(TlsAgentStreamTestServer, ConfigureCertRsaPkcs1KEX) {
  Reset(TlsAgent::kServerRsaDecrypt);

  PRFileDesc* ssl_fd = agent_->ssl_fd();
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_decrypt));
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_sign));
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_pss));

  // Configuring for only rsa_sign or rsa_pss should fail.
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaDecrypt, false,
                                        &ServerCertDataRsaPkcs1Sign));
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaDecrypt, false,
                                        &ServerCertDataRsaPss));

  // Configuring for only rsa_decrypt should work.
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsaDecrypt, false,
                                       &ServerCertDataRsaPkcs1Decrypt));
}

// Test configuring an RSA-PSS cert.
TEST_F(TlsAgentStreamTestServer, ConfigureCertRsaPss) {
  Reset(TlsAgent::kServerRsaPss);

  PRFileDesc* ssl_fd = agent_->ssl_fd();
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_decrypt));
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_sign));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_pss));

  // Configuring for only rsa_sign or rsa_decrypt should fail.
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaPss, false,
                                        &ServerCertDataRsaPkcs1Sign));
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaPss, false,
                                        &ServerCertDataRsaPkcs1Decrypt));

  // Configuring for only rsa_pss should work.
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsaPss, false,
                                       &ServerCertDataRsaPss));
}

// A server should refuse to even start a handshake with
// misconfigured certificate and signature scheme.
TEST_P(TlsConnectTls12Plus, MisconfiguredCertScheme) {
  Reset(TlsAgent::kServerDsa);
  static const SSLSignatureScheme kScheme[] = {ssl_sig_ecdsa_secp256r1_sha256};
  server_->SetSignatureSchemes(kScheme, PR_ARRAY_SIZE(kScheme));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
    // TLS 1.2 disables cipher suites, which leads to a different error.
    server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  } else {
    server_->CheckErrorCode(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
  }
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// In TLS 1.2, disabling an EC group causes ECDSA to be invalid.
TEST_P(TlsConnectTls12, Tls12CertDisabledGroup) {
  Reset(TlsAgent::kServerEcdsa256);
  static const std::vector<SSLNamedGroup> k25519 = {ssl_grp_ec_curve25519};
  server_->ConfigNamedGroups(k25519);
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// In TLS 1.3, ECDSA configuration only depends on the signature scheme.
TEST_P(TlsConnectTls13, Tls13CertDisabledGroup) {
  Reset(TlsAgent::kServerEcdsa256);
  static const std::vector<SSLNamedGroup> k25519 = {ssl_grp_ec_curve25519};
  server_->ConfigNamedGroups(k25519);
  Connect();
}

// A client should refuse to even start a handshake with only DSA.
TEST_P(TlsConnectTls13, Tls13DsaOnlyClient) {
  static const SSLSignatureScheme kDsa[] = {ssl_sig_dsa_sha256};
  client_->SetSignatureSchemes(kDsa, PR_ARRAY_SIZE(kDsa));
  client_->StartConnect();
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  client_->CheckErrorCode(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
}

TEST_P(TlsConnectTls13, Tls13DsaOnlyServer) {
  Reset(TlsAgent::kServerDsa);
  static const SSLSignatureScheme kDsa[] = {ssl_sig_dsa_sha256};
  server_->SetSignatureSchemes(kDsa, PR_ARRAY_SIZE(kDsa));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectTls13, Tls13Pkcs1OnlyClient) {
  static const SSLSignatureScheme kPkcs1[] = {ssl_sig_rsa_pkcs1_sha256};
  client_->SetSignatureSchemes(kPkcs1, PR_ARRAY_SIZE(kPkcs1));
  client_->StartConnect();
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  client_->CheckErrorCode(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
}

TEST_P(TlsConnectTls13, Tls13Pkcs1OnlyServer) {
  static const SSLSignatureScheme kPkcs1[] = {ssl_sig_rsa_pkcs1_sha256};
  server_->SetSignatureSchemes(kPkcs1, PR_ARRAY_SIZE(kPkcs1));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_NO_SUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectTls13, Tls13DsaIsNotAdvertisedClient) {
  EnsureTlsSetup();
  static const SSLSignatureScheme kSchemes[] = {ssl_sig_dsa_sha256,
                                                ssl_sig_rsa_pss_rsae_sha256};
  client_->SetSignatureSchemes(kSchemes, PR_ARRAY_SIZE(kSchemes));
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_signature_algorithms_xtn);
  Connect();
  // We should only have the one signature algorithm advertised.
  static const uint8_t kExpectedExt[] = {0, 2, ssl_sig_rsa_pss_rsae_sha256 >> 8,
                                         ssl_sig_rsa_pss_rsae_sha256 & 0xff};
  ASSERT_EQ(DataBuffer(kExpectedExt, sizeof(kExpectedExt)),
            capture->extension());
}

TEST_P(TlsConnectTls13, Tls13DsaIsNotAdvertisedServer) {
  EnsureTlsSetup();
  static const SSLSignatureScheme kSchemes[] = {ssl_sig_dsa_sha256,
                                                ssl_sig_rsa_pss_rsae_sha256};
  server_->SetSignatureSchemes(kSchemes, PR_ARRAY_SIZE(kSchemes));
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_signature_algorithms_xtn, true);
  capture->SetHandshakeTypes({kTlsHandshakeCertificateRequest});
  capture->EnableDecryption();
  server_->RequestClientAuth(false);  // So we get a CertificateRequest.
  Connect();
  // We should only have the one signature algorithm advertised.
  static const uint8_t kExpectedExt[] = {0, 2, ssl_sig_rsa_pss_rsae_sha256 >> 8,
                                         ssl_sig_rsa_pss_rsae_sha256 & 0xff};
  ASSERT_EQ(DataBuffer(kExpectedExt, sizeof(kExpectedExt)),
            capture->extension());
}

TEST_P(TlsConnectTls13, Tls13RsaPkcs1IsAdvertisedClient) {
  EnsureTlsSetup();
  static const SSLSignatureScheme kSchemes[] = {ssl_sig_rsa_pkcs1_sha256,
                                                ssl_sig_rsa_pss_rsae_sha256};
  client_->SetSignatureSchemes(kSchemes, PR_ARRAY_SIZE(kSchemes));
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_signature_algorithms_xtn);
  Connect();
  // We should only have the one signature algorithm advertised.
  static const uint8_t kExpectedExt[] = {0,
                                         4,
                                         ssl_sig_rsa_pss_rsae_sha256 >> 8,
                                         ssl_sig_rsa_pss_rsae_sha256 & 0xff,
                                         ssl_sig_rsa_pkcs1_sha256 >> 8,
                                         ssl_sig_rsa_pkcs1_sha256 & 0xff};
  ASSERT_EQ(DataBuffer(kExpectedExt, sizeof(kExpectedExt)),
            capture->extension());
}

TEST_P(TlsConnectTls13, Tls13RsaPkcs1IsAdvertisedServer) {
  EnsureTlsSetup();
  static const SSLSignatureScheme kSchemes[] = {ssl_sig_rsa_pkcs1_sha256,
                                                ssl_sig_rsa_pss_rsae_sha256};
  server_->SetSignatureSchemes(kSchemes, PR_ARRAY_SIZE(kSchemes));
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_signature_algorithms_xtn, true);
  capture->SetHandshakeTypes({kTlsHandshakeCertificateRequest});
  capture->EnableDecryption();
  server_->RequestClientAuth(false);  // So we get a CertificateRequest.
  Connect();
  // We should only have the one signature algorithm advertised.
  static const uint8_t kExpectedExt[] = {0,
                                         4,
                                         ssl_sig_rsa_pss_rsae_sha256 >> 8,
                                         ssl_sig_rsa_pss_rsae_sha256 & 0xff,
                                         ssl_sig_rsa_pkcs1_sha256 >> 8,
                                         ssl_sig_rsa_pkcs1_sha256 & 0xff};
  ASSERT_EQ(DataBuffer(kExpectedExt, sizeof(kExpectedExt)),
            capture->extension());
}

// variant, version, certificate, auth type, signature scheme
typedef std::tuple<SSLProtocolVariant, uint16_t, std::string, SSLAuthType,
                   SSLSignatureScheme>
    SignatureSchemeProfile;

class TlsSignatureSchemeConfiguration
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<SignatureSchemeProfile> {
 public:
  TlsSignatureSchemeConfiguration()
      : TlsConnectTestBase(std::get<0>(GetParam()), std::get<1>(GetParam())),
        certificate_(std::get<2>(GetParam())),
        auth_type_(std::get<3>(GetParam())),
        signature_scheme_(std::get<4>(GetParam())) {}

 protected:
  void TestSignatureSchemeConfig(std::shared_ptr<TlsAgent>& configPeer) {
    EnsureTlsSetup();
    configPeer->SetSignatureSchemes(&signature_scheme_, 1);
    Connect();
    CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, auth_type_,
              signature_scheme_);
  }

  std::string certificate_;
  SSLAuthType auth_type_;
  SSLSignatureScheme signature_scheme_;
};

TEST_P(TlsSignatureSchemeConfiguration, SignatureSchemeConfigServer) {
  Reset(certificate_);
  TestSignatureSchemeConfig(server_);
}

TEST_P(TlsSignatureSchemeConfiguration, SignatureSchemeConfigClient) {
  Reset(certificate_);
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_signature_algorithms_xtn);
  TestSignatureSchemeConfig(client_);

  const DataBuffer& ext = capture->extension();
  ASSERT_EQ(2U + 2U, ext.len());
  uint32_t v = 0;
  ASSERT_TRUE(ext.Read(0, 2, &v));
  EXPECT_EQ(2U, v);
  ASSERT_TRUE(ext.Read(2, 2, &v));
  EXPECT_EQ(signature_scheme_, static_cast<SSLSignatureScheme>(v));
}

TEST_P(TlsSignatureSchemeConfiguration, SignatureSchemeConfigBoth) {
  Reset(certificate_);
  EnsureTlsSetup();
  client_->SetSignatureSchemes(&signature_scheme_, 1);
  server_->SetSignatureSchemes(&signature_scheme_, 1);
  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, auth_type_, signature_scheme_);
}

INSTANTIATE_TEST_CASE_P(
    SignatureSchemeRsa, TlsSignatureSchemeConfiguration,
    ::testing::Combine(
        TlsConnectTestBase::kTlsVariantsAll, TlsConnectTestBase::kTlsV12,
        ::testing::Values(TlsAgent::kServerRsaSign),
        ::testing::Values(ssl_auth_rsa_sign),
        ::testing::Values(ssl_sig_rsa_pkcs1_sha256, ssl_sig_rsa_pkcs1_sha384,
                          ssl_sig_rsa_pkcs1_sha512, ssl_sig_rsa_pss_rsae_sha256,
                          ssl_sig_rsa_pss_rsae_sha384)));
// RSASSA-PKCS1-v1_5 is not allowed to be used in TLS 1.3
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeRsaTls13, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV13,
                       ::testing::Values(TlsAgent::kServerRsaSign),
                       ::testing::Values(ssl_auth_rsa_sign),
                       ::testing::Values(ssl_sig_rsa_pss_rsae_sha256,
                                         ssl_sig_rsa_pss_rsae_sha384)));
// PSS with SHA-512 needs a bigger key to work.
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeBigRsa, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kRsa2048),
                       ::testing::Values(ssl_auth_rsa_sign),
                       ::testing::Values(ssl_sig_rsa_pss_rsae_sha512)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeRsaSha1, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV12,
                       ::testing::Values(TlsAgent::kServerRsa),
                       ::testing::Values(ssl_auth_rsa_sign),
                       ::testing::Values(ssl_sig_rsa_pkcs1_sha1)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaP256, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kServerEcdsa256),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_secp256r1_sha256)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaP384, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kServerEcdsa384),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_secp384r1_sha384)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaP521, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kServerEcdsa521),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_secp521r1_sha512)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaSha1, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                       TlsConnectTestBase::kTlsV12,
                       ::testing::Values(TlsAgent::kServerEcdsa256,
                                         TlsAgent::kServerEcdsa384),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_sha1)));
}  // namespace nss_test
