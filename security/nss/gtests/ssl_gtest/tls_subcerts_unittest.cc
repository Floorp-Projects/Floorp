/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctime>

#include "prtime.h"
#include "secerr.h"
#include "ssl.h"

#include "gtest_utils.h"
#include "tls_agent.h"
#include "tls_connect.h"

namespace nss_test {

const std::string kDelegatorId = TlsAgent::kDelegatorEcdsa256;
const std::string kDCId = TlsAgent::kServerEcdsa256;
const SSLSignatureScheme kDCScheme = ssl_sig_ecdsa_secp256r1_sha256;
const PRUint32 kDCValidFor = 60 * 60 * 24 * 7 /* 1 week (seconds */;

static void CheckPeerDelegCred(const std::shared_ptr<TlsAgent>& client,
                               bool expected, PRUint32 key_bits = 0) {
  EXPECT_EQ(expected, client->info().peerDelegCred);
  if (expected) {
    EXPECT_EQ(key_bits, client->info().authKeyBits);
  }
}

// Attempt to configure a DC when either the DC or DC private key is missing.
TEST_P(TlsConnectTls13, DCNotConfigured) {
  // Load and delegate the credential.
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EXPECT_TRUE(TlsAgent::LoadKeyPairFromCert(kDCId, &pub, &priv));

  StackSECItem dc;
  TlsAgent::DelegateCredential(kDelegatorId, pub, kDCScheme, kDCValidFor, now(),
                               &dc);

  // Attempt to install the certificate and DC with a missing DC private key.
  EnsureTlsSetup();
  SSLExtraServerCertData extra_data_missing_dc_priv_key = {
      ssl_auth_null, nullptr, nullptr, nullptr, &dc, nullptr};
  EXPECT_FALSE(server_->ConfigServerCert(kDelegatorId, true,
                                         &extra_data_missing_dc_priv_key));

  // Attempt to install the certificate and with only the DC private key.
  EnsureTlsSetup();
  SSLExtraServerCertData extra_data_missing_dc = {
      ssl_auth_null, nullptr, nullptr, nullptr, nullptr, priv.get()};
  EXPECT_FALSE(
      server_->ConfigServerCert(kDelegatorId, true, &extra_data_missing_dc));
}

// Connected with ECDSA-P256.
TEST_P(TlsConnectTls13, DCConnectEcdsaP256) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(TlsAgent::kServerEcdsa256,
                                  ssl_sig_ecdsa_secp256r1_sha256, kDCValidFor,
                                  now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, true, 256);
  EXPECT_EQ(ssl_sig_ecdsa_secp256r1_sha256, client_->info().signatureScheme);
}

// Connected with ECDSA-P521.
TEST_P(TlsConnectTls13, DCConnectEcdsaP521) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(TlsAgent::kServerEcdsa521,
                                  ssl_sig_ecdsa_secp521r1_sha512, kDCValidFor,
                                  now());
  client_->EnableDelegatedCredentials();

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, true, 521);
  EXPECT_EQ(ssl_sig_ecdsa_secp521r1_sha512, client_->info().signatureScheme);
}

// Connected with RSA-PSS, using an RSAE SPKI.
TEST_P(TlsConnectTls13, DCConnectRsaPssRsae) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(
      TlsAgent::kServerRsaPss, ssl_sig_rsa_pss_rsae_sha256, kDCValidFor, now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, true, 1024);
  EXPECT_EQ(ssl_sig_rsa_pss_rsae_sha256, client_->info().signatureScheme);
}

// Connected with RSA-PSS, using a PSS SPKI.
TEST_P(TlsConnectTls13, DCConnectRsaPssPss) {
  Reset(kDelegatorId);

  // Need to enable PSS-PSS, which is not on by default.
  static const SSLSignatureScheme kSchemes[] = {ssl_sig_ecdsa_secp256r1_sha256,
                                                ssl_sig_rsa_pss_pss_sha256};
  client_->SetSignatureSchemes(kSchemes, PR_ARRAY_SIZE(kSchemes));
  server_->SetSignatureSchemes(kSchemes, PR_ARRAY_SIZE(kSchemes));

  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(
      TlsAgent::kServerRsaPss, ssl_sig_rsa_pss_pss_sha256, kDCValidFor, now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, true, 1024);
  EXPECT_EQ(ssl_sig_rsa_pss_pss_sha256, client_->info().signatureScheme);
}

// Generate a weak key.  We can't do this in the fixture because certutil
// won't sign with such a tiny key.  That's OK, because this is fast(ish).
static void GenerateWeakRsaKey(ScopedSECKEYPrivateKey& priv,
                               ScopedSECKEYPublicKey& pub) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);
  PK11RSAGenParams rsaparams;
  // The absolute minimum size of RSA key that we can use with SHA-256 is
  // 256bit (hash) + 256bit (salt) + 8 (start byte) + 8 (end byte) = 528.
  rsaparams.keySizeInBits = 528;
  rsaparams.pe = 65537;

  // Bug 1012786: PK11_GenerateKeyPair can fail if there is insufficient
  // entropy to generate a random key. We can fake some.
  for (int retry = 0; retry < 10; ++retry) {
    SECKEYPublicKey* p_pub = nullptr;
    priv.reset(PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                    &rsaparams, &p_pub, false, false, nullptr));
    pub.reset(p_pub);
    if (priv) {
      return;
    }

    ASSERT_FALSE(pub);
    if (PORT_GetError() != SEC_ERROR_PKCS11_FUNCTION_FAILED) {
      break;
    }

    // https://xkcd.com/221/
    static const uint8_t FRESH_ENTROPY[16] = {4};
    ASSERT_EQ(
        SECSuccess,
        PK11_RandomUpdate(
            const_cast<void*>(reinterpret_cast<const void*>(FRESH_ENTROPY)),
            sizeof(FRESH_ENTROPY)));
    break;
  }
  ADD_FAILURE() << "Unable to generate an RSA key: "
                << PORT_ErrorToName(PORT_GetError());
}

// Fail to connect with a weak RSA key.
TEST_P(TlsConnectTls13, DCWeakKey) {
  Reset(kDelegatorId);
  EnsureTlsSetup();

  ScopedSECKEYPrivateKey dc_priv;
  ScopedSECKEYPublicKey dc_pub;
  GenerateWeakRsaKey(dc_priv, dc_pub);
  ASSERT_TRUE(dc_priv);

  // Construct a DC.
  StackSECItem dc;
  TlsAgent::DelegateCredential(kDelegatorId, dc_pub,
                               ssl_sig_rsa_pss_rsae_sha256, kDCValidFor, now(),
                               &dc);

  // Configure the DC on the server.
  SSLExtraServerCertData extra_data = {ssl_auth_null, nullptr, nullptr,
                                       nullptr,       &dc,     dc_priv.get()};
  EXPECT_TRUE(server_->ConfigServerCert(kDelegatorId, true, &extra_data));

  client_->EnableDelegatedCredentials();

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  ConnectExpectAlert(client_, kTlsAlertInsufficientSecurity);
}

class ReplaceDCSigScheme : public TlsHandshakeFilter {
 public:
  ReplaceDCSigScheme(const std::shared_ptr<TlsAgent>& a)
      : TlsHandshakeFilter(a, {ssl_hs_certificate_verify}) {}

 protected:
  PacketFilter::Action FilterHandshake(const HandshakeHeader& header,
                                       const DataBuffer& input,
                                       DataBuffer* output) override {
    *output = input;
    output->Write(0, ssl_sig_ecdsa_secp384r1_sha384, 2);
    return CHANGE;
  }
};

// Aborted because of incorrect DC signature algorithm indication.
TEST_P(TlsConnectTls13, DCAbortBadExpectedCertVerifyAlg) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(TlsAgent::kServerEcdsa256,
                                  ssl_sig_ecdsa_secp256r1_sha256, kDCValidFor,
                                  now());
  auto filter = MakeTlsFilter<ReplaceDCSigScheme>(server_);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_DC_CERT_VERIFY_ALG_MISMATCH);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Aborted because of invalid DC signature.
TEST_P(TlsConnectTls13, DCAbortBadSignature) {
  Reset(kDelegatorId);
  EnsureTlsSetup();
  client_->EnableDelegatedCredentials();

  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EXPECT_TRUE(TlsAgent::LoadKeyPairFromCert(kDCId, &pub, &priv));

  StackSECItem dc;
  TlsAgent::DelegateCredential(kDelegatorId, pub, kDCScheme, kDCValidFor, now(),
                               &dc);
  ASSERT_TRUE(dc.data != nullptr);

  // Flip the first bit of the DC so that the signature is invalid.
  dc.data[0] ^= 0x01;

  SSLExtraServerCertData extra_data = {ssl_auth_null, nullptr, nullptr,
                                       nullptr,       &dc,     priv.get()};
  EXPECT_TRUE(server_->ConfigServerCert(kDelegatorId, true, &extra_data));

  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_DC_BAD_SIGNATURE);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Aborted because of expired DC.
TEST_P(TlsConnectTls13, DCAbortExpired) {
  Reset(kDelegatorId);
  server_->AddDelegatedCredential(kDCId, kDCScheme, kDCValidFor, now());
  client_->EnableDelegatedCredentials();
  // When the client checks the time, it will be at least one second after the
  // DC expired.
  AdvanceTime((static_cast<PRTime>(kDCValidFor) + 1) * PR_USEC_PER_SEC);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_DC_EXPIRED);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Aborted because of invalid key usage.
TEST_P(TlsConnectTls13, DCAbortBadKeyUsage) {
  // The sever does not have the delegationUsage extension.
  Reset(TlsAgent::kServerEcdsa256);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(kDCId, kDCScheme, kDCValidFor, now());
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
}

// Connected without DC because of no client indication.
TEST_P(TlsConnectTls13, DCConnectNoClientSupport) {
  Reset(kDelegatorId);
  server_->AddDelegatedCredential(kDCId, kDCScheme, kDCValidFor, now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_FALSE(cfilter->captured());
  CheckPeerDelegCred(client_, false);
}

// Connected without DC because of no server DC.
TEST_P(TlsConnectTls13, DCConnectNoServerSupport) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, false);
}

// Connected without DC because client doesn't support TLS 1.3.
TEST_P(TlsConnectTls13, DCConnectClientNoTls13) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(kDCId, kDCScheme, kDCValidFor, now());

  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  // Should fallback to TLS 1.2 and not negotiate a DC.
  EXPECT_FALSE(cfilter->captured());
  CheckPeerDelegCred(client_, false);
}

// Connected without DC because server doesn't support TLS 1.3.
TEST_P(TlsConnectTls13, DCConnectServerNoTls13) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(kDCId, kDCScheme, kDCValidFor, now());

  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  // Should fallback to TLS 1.2 and not negotiate a DC. The client will still
  // send the indication because it supports 1.3.
  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, false);
}

// Connected without DC because client doesn't support the signature scheme.
TEST_P(TlsConnectTls13, DCConnectExpectedCertVerifyAlgNotSupported) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  static const SSLSignatureScheme kClientSchemes[] = {
      ssl_sig_ecdsa_secp256r1_sha256,
  };
  client_->SetSignatureSchemes(kClientSchemes, PR_ARRAY_SIZE(kClientSchemes));

  server_->AddDelegatedCredential(TlsAgent::kServerEcdsa521,
                                  ssl_sig_ecdsa_secp521r1_sha512, kDCValidFor,
                                  now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  // Client sends indication, but the server doesn't send a DC.
  EXPECT_TRUE(cfilter->captured());
  CheckPeerDelegCred(client_, false);
}

class DCDelegation : public ::testing::Test {};

TEST_F(DCDelegation, DCDelegations) {
  PRTime now = PR_Now();
  ScopedCERTCertificate cert;
  ScopedSECKEYPrivateKey priv;
  ASSERT_TRUE(TlsAgent::LoadCertificate(kDelegatorId, &cert, &priv));

  ScopedSECKEYPublicKey pub_rsa;
  ScopedSECKEYPrivateKey priv_rsa;
  ASSERT_TRUE(
      TlsAgent::LoadKeyPairFromCert(TlsAgent::kServerRsa, &pub_rsa, &priv_rsa));

  StackSECItem dc;
  EXPECT_EQ(SECFailure,
            SSL_DelegateCredential(cert.get(), priv.get(), pub_rsa.get(),
                                   ssl_sig_ecdsa_secp256r1_sha256, kDCValidFor,
                                   now, &dc));
  EXPECT_EQ(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM, PORT_GetError());

  // Using different PSS hashes should be OK.
  EXPECT_EQ(SECSuccess,
            SSL_DelegateCredential(cert.get(), priv.get(), pub_rsa.get(),
                                   ssl_sig_rsa_pss_rsae_sha256, kDCValidFor,
                                   now, &dc));
  // Make sure to reset |dc| after each success.
  dc.Reset();
  EXPECT_EQ(SECSuccess, SSL_DelegateCredential(
                            cert.get(), priv.get(), pub_rsa.get(),
                            ssl_sig_rsa_pss_pss_sha256, kDCValidFor, now, &dc));
  dc.Reset();
  EXPECT_EQ(SECSuccess, SSL_DelegateCredential(
                            cert.get(), priv.get(), pub_rsa.get(),
                            ssl_sig_rsa_pss_pss_sha384, kDCValidFor, now, &dc));
  dc.Reset();

  ScopedSECKEYPublicKey pub_ecdsa;
  ScopedSECKEYPrivateKey priv_ecdsa;
  ASSERT_TRUE(TlsAgent::LoadKeyPairFromCert(TlsAgent::kServerEcdsa256,
                                            &pub_ecdsa, &priv_ecdsa));

  EXPECT_EQ(SECFailure,
            SSL_DelegateCredential(cert.get(), priv.get(), pub_ecdsa.get(),
                                   ssl_sig_rsa_pss_rsae_sha256, kDCValidFor,
                                   now, &dc));
  EXPECT_EQ(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM, PORT_GetError());
  EXPECT_EQ(SECFailure, SSL_DelegateCredential(
                            cert.get(), priv.get(), pub_ecdsa.get(),
                            ssl_sig_rsa_pss_pss_sha256, kDCValidFor, now, &dc));
  EXPECT_EQ(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM, PORT_GetError());
  EXPECT_EQ(SECFailure,
            SSL_DelegateCredential(cert.get(), priv.get(), pub_ecdsa.get(),
                                   ssl_sig_ecdsa_secp384r1_sha384, kDCValidFor,
                                   now, &dc));
  EXPECT_EQ(SSL_ERROR_INCORRECT_SIGNATURE_ALGORITHM, PORT_GetError());
}

}  // namespace nss_test
