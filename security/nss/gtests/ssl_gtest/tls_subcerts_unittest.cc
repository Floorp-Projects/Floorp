/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctime>

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
  server_->AddDelegatedCredential(kDCId, kDCScheme, kDCValidFor, now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  client_->CheckPeerDelegCred(true);
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
  client_->CheckPeerDelegCred(true);
}

// Connected with RSA-PSS.
TEST_P(TlsConnectTls13, DCConnectRsaPss) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(
      TlsAgent::kServerRsaPss, ssl_sig_rsa_pss_rsae_sha256, kDCValidFor, now());

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  client_->CheckPeerDelegCred(true);
}

// Aborted because of incorrect DC signature algorithm indication.
TEST_P(TlsConnectTls13, DCAbortBadExpectedCertVerifyAlg) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();
  server_->AddDelegatedCredential(TlsAgent::kServerEcdsa256,
                                  ssl_sig_ecdsa_secp521r1_sha512, kDCValidFor,
                                  now());
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
}

// Aborted because of invalid DC signature.
TEST_P(TlsConnectTls13, DCABortBadSignature) {
  Reset(kDelegatorId);
  EnsureTlsSetup();
  client_->EnableDelegatedCredentials();

  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EXPECT_TRUE(TlsAgent::LoadKeyPairFromCert(kDCId, &pub, &priv));

  StackSECItem dc;
  TlsAgent::DelegateCredential(kDelegatorId, pub, kDCScheme, kDCValidFor, now(),
                               &dc);

  // Flip the first bit of the DC so that the signature is invalid.
  dc.data[0] ^= 0x01;

  SSLExtraServerCertData extra_data = {ssl_auth_null, nullptr, nullptr,
                                       nullptr,       &dc,     priv.get()};
  EXPECT_TRUE(server_->ConfigServerCert(kDelegatorId, true, &extra_data));

  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
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
  client_->CheckPeerDelegCred(false);
}

// Connected without DC because of no server DC.
TEST_P(TlsConnectTls13, DCConnectNoServerSupport) {
  Reset(kDelegatorId);
  client_->EnableDelegatedCredentials();

  auto cfilter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_delegated_credentials_xtn);
  Connect();

  EXPECT_TRUE(cfilter->captured());
  client_->CheckPeerDelegCred(false);
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
  client_->CheckPeerDelegCred(false);
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
  client_->CheckPeerDelegCred(false);
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
  client_->CheckPeerDelegCred(false);
}

}  // namespace nss_test
