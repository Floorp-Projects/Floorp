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
#include "scoped_ptrs.h"
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
  Reset(TlsAgent::kServerRsaChain);
  Connect();
  CheckKeys();
  size_t chain_length;
  EXPECT_TRUE(client_->GetPeerChainLength(&chain_length));
  EXPECT_EQ(2UL, chain_length);
}

TEST_P(TlsConnectGeneric, ClientAuth) {
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();
}

// In TLS 1.3, the client sends its cert rejection on the
// second flight, and since it has already received the
// server's Finished, it transitions to complete and
// then gets an alert from the server. The test harness
// doesn't handle this right yet.
TEST_P(TlsConnectStream, DISABLED_ClientAuthRequiredRejected) {
  server_->RequestClientAuth(true);
  ConnectExpectFail();
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
static void CheckSigScheme(TlsInspectorRecordHandshakeMessage* capture,
                           size_t offset, TlsAgent* peer,
                           uint16_t expected_scheme, size_t expected_size) {
  EXPECT_LT(offset + 2U, capture->buffer().len());

  uint32_t scheme = 0;
  capture->buffer().Read(offset, 2, &scheme);
  EXPECT_EQ(expected_scheme, static_cast<uint16_t>(scheme));

  ScopedCERTCertificate remote_cert(SSL_PeerCertificate(peer->ssl_fd()));
  ScopedSECKEYPublicKey remote_key(CERT_ExtractPublicKey(remote_cert.get()));
  EXPECT_EQ(expected_size, SECKEY_PublicKeyStrengthInBits(remote_key.get()));
}

// The server should prefer SHA-256 by default, even for the small key size used
// in the default certificate.
TEST_P(TlsConnectTls12, ServerAuthCheckSigAlg) {
  EnsureTlsSetup();
  auto capture_ske =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetPacketFilter(capture_ske);
  Connect();
  CheckKeys();

  const DataBuffer& buffer = capture_ske->buffer();
  EXPECT_LT(3U, buffer.len());
  EXPECT_EQ(3U, buffer.data()[0]) << "curve_type == named_curve";
  uint32_t tmp;
  EXPECT_TRUE(buffer.Read(1, 2, &tmp)) << "read NamedCurve";
  EXPECT_EQ(ssl_grp_ec_curve25519, tmp);
  EXPECT_TRUE(buffer.Read(3, 1, &tmp)) << " read ECPoint";
  CheckSigScheme(capture_ske, 4 + tmp, client_, ssl_sig_rsa_pss_sha256, 1024);
}

TEST_P(TlsConnectTls12, ClientAuthCheckSigAlg) {
  EnsureTlsSetup();
  auto capture_cert_verify =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeCertificateVerify);
  client_->SetPacketFilter(capture_cert_verify);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();

  CheckSigScheme(capture_cert_verify, 0, server_, ssl_sig_rsa_pkcs1_sha1, 1024);
}

TEST_P(TlsConnectTls12, ClientAuthBigRsaCheckSigAlg) {
  Reset(TlsAgent::kServerRsa, TlsAgent::kRsa2048);
  auto capture_cert_verify =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeCertificateVerify);
  client_->SetPacketFilter(capture_cert_verify);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  Connect();
  CheckKeys();
  CheckSigScheme(capture_cert_verify, 0, server_, ssl_sig_rsa_pss_sha256, 2048);
}

static const SSLSignatureScheme SignatureSchemeEcdsaSha384[] = {
    ssl_sig_ecdsa_secp384r1_sha384};
static const SSLSignatureScheme SignatureSchemeEcdsaSha256[] = {
    ssl_sig_ecdsa_secp256r1_sha256};
static const SSLSignatureScheme SignatureSchemeRsaSha384[] = {
    ssl_sig_rsa_pkcs1_sha384};
static const SSLSignatureScheme SignatureSchemeRsaSha256[] = {
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
  client_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
  server_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
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
  server_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
  Connect();
  CheckKeys(ssl_kea_ecdh, NamedGroupForEcdsa384(version_), ssl_auth_ecdsa,
            ssl_sig_ecdsa_secp384r1_sha384);
}

TEST_P(TlsConnectTls12Plus, SignatureSchemeCurveMismatch) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
  ConnectExpectFail();
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

TEST_P(TlsConnectTls12Plus, SignatureSchemeBadConfig) {
  Reset(TlsAgent::kServerEcdsa256);  // P-256 cert can't be used.
  server_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
  ConnectExpectFail();
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
}

// Where there is no overlap on signature schemes, we still connect successfully
// if we aren't going to use a signature.
TEST_P(TlsConnectGenericPre13, SignatureAlgorithmNoOverlapStaticRsa) {
  client_->SetSignatureSchemes(SignatureSchemeRsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeRsaSha384));
  server_->SetSignatureSchemes(SignatureSchemeRsaSha256,
                               PR_ARRAY_SIZE(SignatureSchemeRsaSha256));
  EnableOnlyStaticRsaCiphers();
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_auth_rsa_decrypt);
}

TEST_P(TlsConnectTls12Plus, SignatureAlgorithmNoOverlapEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
  server_->SetSignatureSchemes(SignatureSchemeEcdsaSha256,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha256));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_SIGNATURE_ALGORITHM);
}

// Pre 1.2, a mismatch on signature algorithms shouldn't affect anything.
TEST_P(TlsConnectPre12, SignatureAlgorithmNoOverlapEcdsa) {
  Reset(TlsAgent::kServerEcdsa256);
  client_->SetSignatureSchemes(SignatureSchemeEcdsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha384));
  server_->SetSignatureSchemes(SignatureSchemeEcdsaSha256,
                               PR_ARRAY_SIZE(SignatureSchemeEcdsaSha256));
  Connect();
}

// The signature_algorithms extension is mandatory in TLS 1.3.
TEST_P(TlsConnectTls13, SignatureAlgorithmDrop) {
  client_->SetPacketFilter(
      new TlsExtensionDropper(ssl_signature_algorithms_xtn));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_MISSING_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_MISSING_SIGNATURE_ALGORITHMS_EXTENSION);
}

// TLS 1.2 has trouble detecting this sort of modification: it uses SHA1 and
// only fails when the Finished is checked.
TEST_P(TlsConnectTls12, SignatureAlgorithmDrop) {
  client_->SetPacketFilter(
      new TlsExtensionDropper(ssl_signature_algorithms_xtn));
  ConnectExpectFail();
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
}

TEST_P(TlsConnectTls12Plus, RequestClientAuthWithSha384) {
  server_->SetSignatureSchemes(SignatureSchemeRsaSha384,
                               PR_ARRAY_SIZE(SignatureSchemeRsaSha384));
  server_->RequestClientAuth(false);
  Connect();
}

class BeforeFinished : public TlsRecordFilter {
 private:
  enum HandshakeState { BEFORE_CCS, AFTER_CCS, DONE };

 public:
  BeforeFinished(TlsAgent* client, TlsAgent* server, VoidFunction before_ccs,
                 VoidFunction before_finished)
      : client_(client),
        server_(server),
        before_ccs_(before_ccs),
        before_finished_(before_finished),
        state_(BEFORE_CCS) {}

 protected:
  virtual PacketFilter::Action FilterRecord(const RecordHeader& header,
                                            const DataBuffer& body,
                                            DataBuffer* out) {
    switch (state_) {
      case BEFORE_CCS:
        // Awaken when we see the CCS.
        if (header.content_type() == kTlsChangeCipherSpecType) {
          before_ccs_();

          // Write the CCS out as a separate write, so that we can make
          // progress. Ordinarily, libssl sends the CCS and Finished together,
          // but that means that they both get processed together.
          DataBuffer ccs;
          header.Write(&ccs, 0, body);
          server_->SendDirect(ccs);
          client_->Handshake();
          state_ = AFTER_CCS;
          // Request that the original record be dropped by the filter.
          return DROP;
        }
        break;

      case AFTER_CCS:
        EXPECT_EQ(kTlsHandshakeType, header.content_type());
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
  TlsAgent* client_;
  TlsAgent* server_;
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
  BeforeFinished13(TlsAgent* client, TlsAgent* server,
                   VoidFunction before_finished)
      : client_(client),
        server_(server),
        before_finished_(before_finished),
        records_(0) {}

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output) {
    switch (++records_) {
      case 1:
        // Packet 1 is the server's entire first flight.  Drop it.
        EXPECT_EQ(SECSuccess,
                  SSLInt_SetMTU(server_->ssl_fd(), input.len() - 1));
        return DROP;

      // Packet 2 is the first part of the server's retransmitted first
      // flight.  Keep that.

      case 3:
        // Packet 3 is the second part of the server's retransmitted first
        // flight.  Before passing that on, make sure that the client processes
        // packet 2, then call the before_finished_() callback.
        client_->Handshake();
        before_finished_();
        break;

      default:
        break;
    }
    return KEEP;
  }

 private:
  TlsAgent* client_;
  TlsAgent* server_;
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
  server_->SetPacketFilter(new BeforeFinished13(client_, server_, [this]() {
    EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  }));
  Connect();
}

static void TriggerAuthComplete(PollTarget* target, Event event) {
  std::cerr << "client: call SSL_AuthCertificateComplete" << std::endl;
  EXPECT_EQ(TIMER_EVENT, event);
  TlsAgent* client = static_cast<TlsAgent*>(target);
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client->ssl_fd(), 0));
}

// This test uses a simple AuthCertificateCallback.  Due to the way that the
// entire server flight is processed, the call to SSL_AuthCertificateComplete
// will trigger after the Finished message is processed.
TEST_F(TlsConnectDatagram13, AuthCompleteAfterFinished) {
  client_->SetAuthCertificateCallback(
      [this](TlsAgent*, PRBool, PRBool) -> SECStatus {
        Poller::Timer* timer_handle;
        // This is really just to unroll the stack.
        Poller::Instance()->SetTimer(1U, client_, TriggerAuthComplete,
                                     &timer_handle);
        return SECWouldBlock;
      });
  Connect();
}

TEST_P(TlsConnectGenericPre13, ClientWriteBetweenCCSAndFinishedWithFalseStart) {
  client_->EnableFalseStart();
  server_->SetPacketFilter(new BeforeFinished(
      client_, server_,
      [this]() { EXPECT_TRUE(client_->can_falsestart_hook_called()); },
      [this]() {
        // Write something, which used to fail: bug 1235366.
        client_->SendData(10);
      }));

  Connect();
  server_->SendData(10);
  Receive(10);
}

TEST_P(TlsConnectGenericPre13, AuthCompleteBeforeFinishedWithFalseStart) {
  client_->EnableFalseStart();
  client_->SetAuthCertificateCallback(AuthCompleteBlock);
  server_->SetPacketFilter(new BeforeFinished(
      client_, server_,
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
      }));

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

  server_->StartConnect();
  client_->StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send ServerHello
  client_->Handshake();  // Send ClientKeyExchange and Finished
  server_->Handshake();  // Send Finished
  // The server should now report that it is connected
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());

  // The client should send nothing from here on.
  client_->SetPacketFilter(new EnforceNoActivity());
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());

  // This should allow the handshake to complete now.
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  client_->Handshake();  // Transition to connected
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());

  // Remove this before closing or the close_notify alert will trigger it.
  client_->SetPacketFilter(nullptr);
}

// TLS 1.3 handles a delayed AuthComplete callback differently since the
// shape of the handshake is different.
TEST_P(TlsConnectTls13, AuthCompleteDelayed) {
  client_->SetAuthCertificateCallback(AuthCompleteBlock);

  server_->StartConnect();
  client_->StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send ServerHello
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, server_->state());

  // The client will send nothing until AuthCertificateComplete is called.
  client_->SetPacketFilter(new EnforceNoActivity());
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_CONNECTING, client_->state());

  // This should allow the handshake to complete now.
  client_->SetPacketFilter(nullptr);
  EXPECT_EQ(SECSuccess, SSL_AuthCertificateComplete(client_->ssl_fd(), 0));
  client_->Handshake();  // Send Finished
  server_->Handshake();  // Transition to connected and send NewSessionTicket
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
}

static const SSLExtraServerCertData ServerCertDataRsaPkcs1Decrypt = {
    ssl_auth_rsa_decrypt, nullptr, nullptr, nullptr};
static const SSLExtraServerCertData ServerCertDataRsaPkcs1Sign = {
    ssl_auth_rsa_sign, nullptr, nullptr, nullptr};
static const SSLExtraServerCertData ServerCertDataRsaPss = {
    ssl_auth_rsa_pss, nullptr, nullptr, nullptr};

// Test RSA cert with usage=[signature, encipherment].
TEST_F(TlsAgentStreamTestServer, ConfigureCertRsaPkcs1SignAndKEX) {
  Reset(TlsAgent::kServerRsa);

  PRFileDesc* ssl_fd = agent_->ssl_fd();
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_decrypt));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_sign));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_pss));

  // Configuring for only rsa_sign, rsa_pss, or rsa_decrypt should work.
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsa, false,
                                       &ServerCertDataRsaPkcs1Decrypt));
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsa, false,
                                       &ServerCertDataRsaPkcs1Sign));
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsa, false,
                                       &ServerCertDataRsaPss));
}

// Test RSA cert with usage=[signature].
TEST_F(TlsAgentStreamTestServer, ConfigureCertRsaPkcs1Sign) {
  Reset(TlsAgent::kServerRsaSign);

  PRFileDesc* ssl_fd = agent_->ssl_fd();
  EXPECT_FALSE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_decrypt));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_sign));
  EXPECT_TRUE(SSLInt_HasCertWithAuthType(ssl_fd, ssl_auth_rsa_pss));

  // Configuring for only rsa_decrypt should fail.
  EXPECT_FALSE(agent_->ConfigServerCert(TlsAgent::kServerRsaSign, false,
                                        &ServerCertDataRsaPkcs1Decrypt));

  // Configuring for only rsa_sign or rsa_pss should work.
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsaSign, false,
                                       &ServerCertDataRsaPkcs1Sign));
  EXPECT_TRUE(agent_->ConfigServerCert(TlsAgent::kServerRsaSign, false,
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

// mode, version, certificate, auth type, signature scheme
typedef std::tuple<std::string, uint16_t, std::string, SSLAuthType,
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
  void TestSignatureSchemeConfig(TlsAgent* configPeer) {
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
  TlsExtensionCapture* capture =
      new TlsExtensionCapture(ssl_signature_algorithms_xtn);
  client_->SetPacketFilter(capture);
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
        TlsConnectTestBase::kTlsModesAll, TlsConnectTestBase::kTlsV12Plus,
        ::testing::Values(TlsAgent::kServerRsaSign),
        ::testing::Values(ssl_auth_rsa_sign),
        ::testing::Values(ssl_sig_rsa_pkcs1_sha256, ssl_sig_rsa_pkcs1_sha384,
                          ssl_sig_rsa_pkcs1_sha512, ssl_sig_rsa_pss_sha256,
                          ssl_sig_rsa_pss_sha384)));
// PSS with SHA-512 needs a bigger key to work.
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeBigRsa, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kRsa2048),
                       ::testing::Values(ssl_auth_rsa_sign),
                       ::testing::Values(ssl_sig_rsa_pss_sha512)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeRsaSha1, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                       TlsConnectTestBase::kTlsV12,
                       ::testing::Values(TlsAgent::kServerRsa),
                       ::testing::Values(ssl_auth_rsa_sign),
                       ::testing::Values(ssl_sig_rsa_pkcs1_sha1)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaP256, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kServerEcdsa256),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_secp256r1_sha256)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaP384, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kServerEcdsa384),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_secp384r1_sha384)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaP521, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                       TlsConnectTestBase::kTlsV12Plus,
                       ::testing::Values(TlsAgent::kServerEcdsa521),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_secp521r1_sha512)));
INSTANTIATE_TEST_CASE_P(
    SignatureSchemeEcdsaSha1, TlsSignatureSchemeConfiguration,
    ::testing::Combine(TlsConnectTestBase::kTlsModesAll,
                       TlsConnectTestBase::kTlsV12,
                       ::testing::Values(TlsAgent::kServerEcdsa256,
                                         TlsAgent::kServerEcdsa384),
                       ::testing::Values(ssl_auth_ecdsa),
                       ::testing::Values(ssl_sig_ecdsa_sha1)));
}
