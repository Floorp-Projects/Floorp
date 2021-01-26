/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO: Add padding/maxNameLen tests after support is added in bug 1677181.

#include "secerr.h"
#include "ssl.h"

#include "gtest_utils.h"
#include "pk11pub.h"
#include "tls_agent.h"
#include "tls_connect.h"
#include "util.h"

namespace nss_test {

class TlsAgentEchTest : public TlsAgentTestClient13 {
 protected:
  void InstallEchConfig(const DataBuffer& echconfig, PRErrorCode err = 0) {
    SECStatus rv = SSL_SetClientEchConfigs(agent_->ssl_fd(), echconfig.data(),
                                           echconfig.len());
    if (err == 0) {
      ASSERT_EQ(SECSuccess, rv);
    } else {
      ASSERT_EQ(SECFailure, rv);
      ASSERT_EQ(err, PORT_GetError());
    }
  }
};

#ifdef NSS_ENABLE_DRAFT_HPKE
#include "cpputil.h"  // Unused function error if included without HPKE.

static std::string kPublicName("public.name");

static const std::vector<uint32_t> kDefaultSuites = {
    (static_cast<uint32_t>(HpkeKdfHkdfSha256) << 16) | HpkeAeadChaCha20Poly1305,
    (static_cast<uint32_t>(HpkeKdfHkdfSha256) << 16) | HpkeAeadAes128Gcm};
static const std::vector<uint32_t> kSuiteChaCha = {
    (static_cast<uint32_t>(HpkeKdfHkdfSha256) << 16) |
    HpkeAeadChaCha20Poly1305};
static const std::vector<uint32_t> kSuiteAes = {
    (static_cast<uint32_t>(HpkeKdfHkdfSha256) << 16) | HpkeAeadAes128Gcm};
std::vector<uint32_t> kBogusSuite = {0xfefefefe};
static const std::vector<uint32_t> kUnknownFirstSuite = {
    0xfefefefe,
    (static_cast<uint32_t>(HpkeKdfHkdfSha256) << 16) | HpkeAeadAes128Gcm};

class TlsConnectStreamTls13Ech : public TlsConnectTestBase {
 public:
  TlsConnectStreamTls13Ech()
      : TlsConnectTestBase(ssl_variant_stream, SSL_LIBRARY_VERSION_TLS_1_3) {}

  void ReplayChWithMalformedInner(const std::string& ch, uint8_t server_alert,
                                  uint32_t server_code, uint32_t client_code) {
    std::vector<uint8_t> ch_vec = hex_string_to_bytes(ch);
    DataBuffer ch_buf;
    ScopedSECKEYPublicKey pub;
    ScopedSECKEYPrivateKey priv;
    EnsureTlsSetup();
    ImportFixedEchKeypair(pub, priv);
    SetMutualEchConfigs(pub, priv);

    TlsAgentTestBase::MakeRecord(variant_, ssl_ct_handshake,
                                 SSL_LIBRARY_VERSION_TLS_1_3, ch_vec.data(),
                                 ch_vec.size(), &ch_buf, 0);
    StartConnect();
    client_->SendDirect(ch_buf);
    ExpectAlert(server_, server_alert);
    server_->Handshake();
    server_->CheckErrorCode(server_code);
    client_->ExpectReceiveAlert(server_alert, kTlsAlertFatal);
    client_->Handshake();
    client_->CheckErrorCode(client_code);
  }

  // Setup Client/Server with mismatched AEADs
  void SetupForEchRetry() {
    ScopedSECKEYPublicKey server_pub;
    ScopedSECKEYPrivateKey server_priv;
    ScopedSECKEYPublicKey client_pub;
    ScopedSECKEYPrivateKey client_priv;
    DataBuffer server_rec;
    DataBuffer client_rec;
    TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteChaCha,
                                          kPublicName, 100, server_rec,
                                          server_pub, server_priv);
    ASSERT_EQ(SECSuccess,
              SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                      server_priv.get(), server_rec.data(),
                                      server_rec.len()));

    TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                          kPublicName, 100, client_rec,
                                          client_pub, client_priv);
    ASSERT_EQ(SECSuccess,
              SSL_SetClientEchConfigs(client_->ssl_fd(), client_rec.data(),
                                      client_rec.len()));
  }

  // Parse a captured SNI extension and validate the contained name.
  void CheckSniExtension(const DataBuffer& data,
                         const std::string expected_name) {
    TlsParser parser(data.data(), data.len());
    uint32_t tmp;
    ASSERT_TRUE(parser.Read(&tmp, 2));
    ASSERT_EQ(parser.remaining(), tmp);
    ASSERT_TRUE(parser.Read(&tmp, 1));
    ASSERT_EQ(0U, tmp); /* sni_nametype_hostname */
    DataBuffer name;
    ASSERT_TRUE(parser.ReadVariable(&name, 2));
    ASSERT_EQ(0U, parser.remaining());
    // Manual comparison to silence coverity false-positives.
    ASSERT_EQ(name.len(), kPublicName.length());
    ASSERT_EQ(0,
              memcmp(kPublicName.c_str(), name.data(), kPublicName.length()));
  }

  void DoEchRetry(const ScopedSECKEYPublicKey& server_pub,
                  const ScopedSECKEYPrivateKey& server_priv,
                  const DataBuffer& server_rec) {
    StackSECItem retry_configs;
    ASSERT_EQ(SECSuccess,
              SSL_GetEchRetryConfigs(client_->ssl_fd(), &retry_configs));
    ASSERT_NE(0U, retry_configs.len);

    // Reset expectations for the TlsAgent dtor.
    server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
    Reset();
    EnsureTlsSetup();
    ASSERT_EQ(SECSuccess,
              SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                      server_priv.get(), server_rec.data(),
                                      server_rec.len()));
    ASSERT_EQ(SECSuccess,
              SSL_SetClientEchConfigs(client_->ssl_fd(), retry_configs.data,
                                      retry_configs.len));
    client_->ExpectEch();
    server_->ExpectEch();
    Connect();
  }

  void ImportFixedEchKeypair(ScopedSECKEYPublicKey& pub,
                             ScopedSECKEYPrivateKey& priv) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "No slot";
      return;
    }
    std::vector<uint8_t> pkcs8_r = hex_string_to_bytes(kFixedServerPubkey);
    SECItem pkcs8_r_item = {siBuffer, toUcharPtr(pkcs8_r.data()),
                            static_cast<unsigned int>(pkcs8_r.size())};

    SECKEYPrivateKey* tmp_priv = nullptr;
    ASSERT_EQ(SECSuccess, PK11_ImportDERPrivateKeyInfoAndReturnKey(
                              slot.get(), &pkcs8_r_item, nullptr, nullptr,
                              false, false, KU_ALL, &tmp_priv, nullptr));
    priv.reset(tmp_priv);
    SECKEYPublicKey* tmp_pub = SECKEY_ConvertToPublicKey(tmp_priv);
    pub.reset(tmp_pub);
    ASSERT_NE(nullptr, tmp_pub);
  }

 private:
  // Testing certan invalid CHInner configurations is tricky, particularly
  // since the CHOuter forms AAD and isn't available in filters. Instead of
  // generating these inputs on the fly, use a fixed server keypair so that
  // the input can be generated once (e.g. via a debugger) and replayed in
  // each invocation of the test.
  std::string kFixedServerPubkey =
      "3067020100301406072a8648ce3d020106092b06010401da470f01044c304a"
      "02010104205a8aa0d2476b28521588e0c704b14db82cdd4970d340d293a957"
      "6deaee9ec1c7a1230321008756e2580c07c1d2ffcb662f5fadc6d6ff13da85"
      "abd7adfecf984aaa102c1269";

  void SetMutualEchConfigs(ScopedSECKEYPublicKey& pub,
                           ScopedSECKEYPrivateKey& priv) {
    DataBuffer echconfig;
    TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                          kPublicName, 100, echconfig, pub,
                                          priv);
    ASSERT_EQ(SECSuccess,
              SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                      echconfig.data(), echconfig.len()));
    ASSERT_EQ(SECSuccess,
              SSL_SetClientEchConfigs(client_->ssl_fd(), echconfig.data(),
                                      echconfig.len()));
  }
};

static void CheckCertVerifyPublicName(TlsAgent* agent) {
  agent->UpdatePreliminaryChannelInfo();
  EXPECT_NE(0U, (agent->pre_info().valuesSet & ssl_preinfo_ech));
  EXPECT_EQ(agent->GetEchExpected(), agent->pre_info().echAccepted);

  // Check that echPublicName is only exposed in the rejection
  // case, so that the application can use it for CertVerfiy.
  if (agent->GetEchExpected()) {
    EXPECT_EQ(nullptr, agent->pre_info().echPublicName);
  } else {
    EXPECT_NE(nullptr, agent->pre_info().echPublicName);
    if (agent->pre_info().echPublicName) {
      EXPECT_EQ(0,
                strcmp(kPublicName.c_str(), agent->pre_info().echPublicName));
    }
  }
}

static SECStatus AuthCompleteSuccess(TlsAgent* agent, PRBool, PRBool) {
  CheckCertVerifyPublicName(agent);
  return SECSuccess;
}

static SECStatus AuthCompleteFail(TlsAgent* agent, PRBool, PRBool) {
  CheckCertVerifyPublicName(agent);
  return SECFailure;
}

TEST_P(TlsAgentEchTest, EchConfigsSupportedYesNo) {
  if (variant_ == ssl_variant_datagram) {
    GTEST_SKIP();
  }

  // ECHConfig 2 cipher_suites are unsupported.
  const std::string mixed =
      "0086FE09003F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304"
      "444156E4E04D1BF0FFDA7783B6B457F75600200008000100030001000100640000FE0900"
      "3F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304444156E4E0"
      "4D1BF0FFDA7783B6B457F756002000080001FFFFFFFF000100640000";
  std::vector<uint8_t> config = hex_string_to_bytes(mixed);
  DataBuffer echconfig(config.data(), config.size());

  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(echconfig, 0);
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchConfigsSupportedNoYes) {
  if (variant_ == ssl_variant_datagram) {
    GTEST_SKIP();
  }

  // ECHConfig 1 cipher_suites are unsupported.
  const std::string mixed =
      "0086FE09003F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304"
      "444156E4E04D1BF0FFDA7783B6B457F756002000080001FFFFFFFF000100640000FE0900"
      "3F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304444156E4E0"
      "4D1BF0FFDA7783B6B457F75600200008000100030001000100640000";
  std::vector<uint8_t> config = hex_string_to_bytes(mixed);
  DataBuffer echconfig(config.data(), config.size());

  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(echconfig, 0);
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchConfigsSupportedNoNo) {
  if (variant_ == ssl_variant_datagram) {
    GTEST_SKIP();
  }

  // ECHConfig 1 and 2 cipher_suites are unsupported.
  const std::string unsupported =
      "0086FE09003F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304"
      "444156E4E04D1BF0FFDA7783B6B457F756002000080001FFFF0001FFFF00640000FE0900"
      "3F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304444156E4E0"
      "4D1BF0FFDA7783B6B457F75600200008FFFF0003FFFF000100640000";
  std::vector<uint8_t> config = hex_string_to_bytes(unsupported);
  DataBuffer echconfig(config.data(), config.size());

  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(echconfig, SEC_ERROR_INVALID_ARGS);
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, ShortEchConfig) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  echconfig.Truncate(echconfig.len() - 1);
  InstallEchConfig(echconfig, SEC_ERROR_BAD_DATA);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, LongEchConfig) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  echconfig.Write(echconfig.len(), 1, 1);  // Append one byte
  InstallEchConfig(echconfig, SEC_ERROR_BAD_DATA);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, UnsupportedEchConfigVersion) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  static const uint8_t bad_version[] = {0xff, 0xff};
  DataBuffer bad_ver_buf(bad_version, sizeof(bad_version));
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  echconfig.Splice(bad_ver_buf, 2, 2);
  InstallEchConfig(echconfig, SEC_ERROR_INVALID_ARGS);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, UnsupportedHpkeKem) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  // SSL_EncodeEchConfig encodes without validation.
  TlsConnectTestBase::GenerateEchConfig(static_cast<HpkeKemId>(0xff),
                                        kDefaultSuites, kPublicName, 100,
                                        echconfig, pub, priv);
  InstallEchConfig(echconfig, SEC_ERROR_INVALID_ARGS);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchRejectIgnoreAllUnknownSuites) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite,
                                        kPublicName, 100, echconfig, pub, priv);
  InstallEchConfig(echconfig, SEC_ERROR_INVALID_ARGS);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchConfigRejectEmptyPublicName) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite, "",
                                        100, echconfig, pub, priv);
  InstallEchConfig(echconfig, SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_FALSE(filter->captured());
}

TEST_F(TlsConnectStreamTls13, EchAcceptIgnoreSingleUnknownSuite) {
  EnsureTlsSetup();
  DataBuffer echconfig;
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256,
                                        kUnknownFirstSuite, kPublicName, 100,
                                        echconfig, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), echconfig.data(),
                                    echconfig.len()));
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

TEST_P(TlsAgentEchTest, ApiInvalidArgs) {
  EnsureInit();
  // SetClient
  EXPECT_EQ(SECFailure, SSL_SetClientEchConfigs(agent_->ssl_fd(), nullptr, 1));

  EXPECT_EQ(SECFailure,
            SSL_SetClientEchConfigs(agent_->ssl_fd(),
                                    reinterpret_cast<const uint8_t*>(1), 0));

  // SetServer
  EXPECT_EQ(SECFailure,
            SSL_SetServerEchConfigs(agent_->ssl_fd(), nullptr,
                                    reinterpret_cast<SECKEYPrivateKey*>(1),
                                    reinterpret_cast<const uint8_t*>(1), 1));
  EXPECT_EQ(SECFailure,
            SSL_SetServerEchConfigs(
                agent_->ssl_fd(), reinterpret_cast<SECKEYPublicKey*>(1),
                nullptr, reinterpret_cast<const uint8_t*>(1), 1));
  EXPECT_EQ(SECFailure,
            SSL_SetServerEchConfigs(
                agent_->ssl_fd(), reinterpret_cast<SECKEYPublicKey*>(1),
                reinterpret_cast<SECKEYPrivateKey*>(1), nullptr, 1));
  EXPECT_EQ(SECFailure,
            SSL_SetServerEchConfigs(agent_->ssl_fd(),
                                    reinterpret_cast<SECKEYPublicKey*>(1),
                                    reinterpret_cast<SECKEYPrivateKey*>(1),
                                    reinterpret_cast<const uint8_t*>(1), 0));

  // GetRetries
  EXPECT_EQ(SECFailure, SSL_GetEchRetryConfigs(agent_->ssl_fd(), nullptr));

  // EncodeEchConfig
  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig(nullptr, reinterpret_cast<uint32_t*>(1), 1,
                                static_cast<HpkeKemId>(1),
                                reinterpret_cast<SECKEYPublicKey*>(1), 1,
                                reinterpret_cast<uint8_t*>(1),
                                reinterpret_cast<uint32_t*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig("name", nullptr, 1, static_cast<HpkeKemId>(1),
                                reinterpret_cast<SECKEYPublicKey*>(1), 1,
                                reinterpret_cast<uint8_t*>(1),
                                reinterpret_cast<uint32_t*>(1), 1));
  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig("name", reinterpret_cast<uint32_t*>(1), 0,
                                static_cast<HpkeKemId>(1),
                                reinterpret_cast<SECKEYPublicKey*>(1), 1,
                                reinterpret_cast<uint8_t*>(1),
                                reinterpret_cast<uint32_t*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig("name", reinterpret_cast<uint32_t*>(1), 1,
                                static_cast<HpkeKemId>(1), nullptr, 1,
                                reinterpret_cast<uint8_t*>(1),
                                reinterpret_cast<uint32_t*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig(nullptr, reinterpret_cast<uint32_t*>(1), 1,
                                static_cast<HpkeKemId>(1),
                                reinterpret_cast<SECKEYPublicKey*>(1), 0,
                                reinterpret_cast<uint8_t*>(1),
                                reinterpret_cast<uint32_t*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig("name", reinterpret_cast<uint32_t*>(1), 1,
                                static_cast<HpkeKemId>(1),
                                reinterpret_cast<SECKEYPublicKey*>(1), 1,
                                nullptr, reinterpret_cast<uint32_t*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfig("name", reinterpret_cast<uint32_t*>(1), 1,
                                static_cast<HpkeKemId>(1),
                                reinterpret_cast<SECKEYPublicKey*>(1), 1,
                                reinterpret_cast<uint8_t*>(1), nullptr, 1));
}

TEST_P(TlsAgentEchTest, NoEarlyRetryConfigs) {
  EnsureInit();
  StackSECItem retry_configs;
  EXPECT_EQ(SECFailure,
            SSL_GetEchRetryConfigs(agent_->ssl_fd(), &retry_configs));
  EXPECT_EQ(SSL_ERROR_HANDSHAKE_NOT_COMPLETED, PORT_GetError());

  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  InstallEchConfig(echconfig, 0);

  EXPECT_EQ(SECFailure,
            SSL_GetEchRetryConfigs(agent_->ssl_fd(), &retry_configs));
  EXPECT_EQ(SSL_ERROR_HANDSHAKE_NOT_COMPLETED, PORT_GetError());
}

TEST_P(TlsAgentEchTest, NoSniSoNoEch) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  SSL_SetURL(agent_->ssl_fd(), "");
  InstallEchConfig(echconfig, 0);
  SSL_SetURL(agent_->ssl_fd(), "");
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, NoEchConfigSoNoEch) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_FALSE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchConfigDuplicateExtensions) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);

  static const uint8_t duped_xtn[] = {0x00, 0x08, 0x00, 0x01, 0x00,
                                      0x00, 0x00, 0x01, 0x00, 0x00};
  DataBuffer buf(duped_xtn, sizeof(duped_xtn));
  echconfig.Truncate(echconfig.len() - 2);
  echconfig.Append(buf);
  uint32_t len;
  ASSERT_TRUE(echconfig.Read(0, 2, &len));
  len += buf.len() - 2;
  DataBuffer new_len;
  ASSERT_TRUE(new_len.Write(0, len, 2));
  echconfig.Splice(new_len, 0, 2);
  new_len.Truncate(0);

  ASSERT_TRUE(echconfig.Read(4, 2, &len));
  len += buf.len() - 2;
  ASSERT_TRUE(new_len.Write(0, len, 2));
  echconfig.Splice(new_len, 4, 2);

  InstallEchConfig(echconfig, SEC_ERROR_EXTENSION_VALUE_INVALID);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_FALSE(filter->captured());
}

// Test an encoded ClientHelloInner containing an extra extensionType
// in outer_extensions, for which there is no corresponding (uncompressed)
// extension in ClientHelloOuter.
TEST_F(TlsConnectStreamTls13Ech, EchOuterExtensionsReferencesMissing) {
  std::string ch =
      "010001580303dfff91b5e1ba00f29d2338419b3abf125ee1051a942ae25163bbf609a1ea"
      "11920000061301130313020100012900000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020d94c1590c261e9ea8ae55bc9581f397cc598115f8b70aec1b0236f4c8c555537002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe09009b0001000308fde4163c5c6e8bb6002067a895efa2721c88"
      "63ecfa1bea1e520ae6f6cf938e3e37802688f7a83a871a04006aa693f053f87db87cf82a"
      "7caa20670d79b92ccda97893fdf99352fc766fb3dd5570948311dddb6d41214234fae585"
      "e354a048c072b3fb00a0a64e8e089e4a90152ee91a2c5b947c99d3dcebfb6334453b023d"
      "4d725010996a290a0552e4b238ec91c21440adc0d51a4435";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Drop supported_versions from CHInner, make sure we don't negotiate 1.2+ECH.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12Inner) {
  std::string ch =
      "0100015103038fbe6f75b0123116fa5c4eccf0cf26c17ab1ded5529307e419c036ac7e9c"
      "e8e30000061301130313020100012200000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "002078d644583b4f056bec4d8ae9bddd383aed6eb7cdb3294f88b0e37a4f26a02549002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0900940001000308fde4163c5c6e8bb600208958e66d1d4bbd46"
      "4792f392e119dbce91ee3e65067899b45c83855dae61e67a00637df038e7b35483786707"
      "dd1b25be5cd3dd07f1ca4b33a3595ddb959e5c0da3d2f0b3314417614968691700c05232"
      "07c729b34f3b5de62728b3cb6b45b00e6f94b204a9504d0e7e24c66f42aacc73591c86ef"
      "571e61cebd6ba671081150a2dae89e7493";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Use CHInner supported_versions to negotiate 1.2.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12InnerSupportedVersions) {
  std::string ch =
      "01000158030378a601a3f12229e53e0b8d92c3599bf1782e8261d2ecaec9bbe595d4c901"
      "98770000061301130313020100012900000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00201c8017d6970f3a92ac1c9919c3a26788052f84599fb0c3cb7bd381304148724e002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe09009b0001000308fde4163c5c6e8bb60020f7347d34f125e866"
      "76b1cdc43455c6c00918a3c8a961335e1b9aa864da2b5313006a21e6ad81533e90cea24e"
      "c2c3656f6b53114b4c63bf89462696f1c8ad4e1193d87062a5537edbe83c9b35c41e9763"
      "1d2333270854758ee02548afb7f2264f904474465415a5085024487f22b017208e250ca4"
      "7902d61d98fbd1cb8afc0a14dcd70a68343cf67c258758d9";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Replay a CH for which CHInner lacks the required ech_is_inner extension.
TEST_F(TlsConnectStreamTls13Ech, EchInnerMissingEmptyEch) {
  std::string ch =
      "010001540303033b3284790ada882445bfb38b8af3509659033c931e6ae97febbaa62b19"
      "b4ac0000061301130313020100012500000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00209d1ed410ccb05ce9e424f52b1be3599bcc1efb0913ae14a24d9a69cbfbc39744002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0900970001000308fde4163c5c6e8bb600206321bdc543a23d47"
      "7a7104ba69177cb722927c6c485117df4a077b8e82167f0b0066103d9aac7e5fc4ef990b"
      "2ce38593589f7f6ba043847d7db6c9136adb811f63b956d56e6ca8cbe6864e3fc43a3bc5"
      "94a332d4d63833e411c89ef14af63b5cd18c7adee99ffd1ad3112449ea18d6650bbaca66"
      "528f7e4146fafbf338c27cf89b145a55022b26a3";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_MISSING_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Replay a CH for which CHInner contains both an ECH and ech_is_inner
// extension.
TEST_F(TlsConnectStreamTls13Ech, InnerWithEchAndEchIsInner) {
  std::string ch =
      "0100015c030383fb49c98b62bcdf04cbbae418dd684f8f9512f40fca6861ba40555269a9"
      "789f0000061301130313020100012d00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00201e3d35a6755b7dddf7e481359429e9677baaa8dd99569c2bf0b0f7ea56e68b12002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe09009f0001000308fde4163c5c6e8bb6002090110b89c1ba6618"
      "942ea7aae8c472c22e97f10bef7dd490bee50cc108082b48006eed016fa2b3e3419cf5ef"
      "9b41ab9ecffa84a4b60e2f4cc710cf31c739d1f6f88b48207aaf7ccabdd744a25a8f2a38"
      "029d1b133e9d990681cf08c07a255d9242b3a002bc0865935cbb609b2b1996fab0626cb0"
      "2ece6544bbde0d3218333ffd95c383a41854b76b1a254bb346a2702b";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_CLIENT_HELLO,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13, OuterWithEchAndEchIsInner) {
  static uint8_t empty_buf[1] = {0};
  DataBuffer empty(empty_buf, 0);

  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello,
                                      ssl_tls13_ech_is_inner_xtn, empty);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
}

// Apply two ECHConfigs on the server. They are identical with the exception
// of the public key: the first ECHConfig contains a public key for which we
// lack the private value. Use an SSLInt function to zero all the config_ids
// (client and server), then confirm that trial decryption works.
TEST_F(TlsConnectStreamTls13Ech, EchConfigsTrialDecrypt) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EnsureTlsSetup();
  ImportFixedEchKeypair(pub, priv);

  const std::string two_configs_str =
      "007EFE09003B000B7075626C69632E6E616D650020111111111111111111111111111111"
      "1111111111111111111111111111111111002000040001000100640000fe09003B000B70"
      "75626C69632E6E616D6500208756E2580C07C1D2FFCB662F5FADC6D6FF13DA85ABD7ADFE"
      "CF984AAA102C1269002000040001000100640000";
  const std::string second_config_str =
      "003FFE09003B000B7075626C69632E6E616D6500208756E2580C07C1D2FFCB662F5FADC6"
      "D6FF13DA85ABD7ADFECF984AAA102C1269002000040001000100640000";
  std::vector<uint8_t> two_configs = hex_string_to_bytes(two_configs_str);
  std::vector<uint8_t> second_config = hex_string_to_bytes(second_config_str);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    two_configs.data(), two_configs.size()));
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), second_config.data(),
                                    second_config.size()));

  ASSERT_EQ(SECSuccess, SSLInt_ZeroEchConfigIds(client_->ssl_fd()));
  ASSERT_EQ(SECSuccess, SSLInt_ZeroEchConfigIds(server_->ssl_fd()));
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

// An empty config_id should prompt an alert. We don't support
// Optional Configuration Identifiers.
TEST_F(TlsConnectStreamTls13, EchRejectEmptyConfigId) {
  static const uint8_t junk[16] = {0};
  DataBuffer junk_buf(junk, sizeof(junk));
  DataBuffer ech_xtn;
  ech_xtn.Write(ech_xtn.len(), HpkeKdfHkdfSha256, 2);
  ech_xtn.Write(ech_xtn.len(), HpkeAeadAes128Gcm, 2);
  ech_xtn.Write(ech_xtn.len(), 0U, 1);              // empty config_id
  ech_xtn.Write(ech_xtn.len(), junk_buf.len(), 2);  // enc
  ech_xtn.Append(junk_buf);
  ech_xtn.Write(ech_xtn.len(), junk_buf.len(), 2);  // payload
  ech_xtn.Append(junk_buf);

  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello,
                                      ssl_tls13_encrypted_client_hello_xtn,
                                      ech_xtn);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
}

TEST_F(TlsConnectStreamTls13Ech, EchAcceptBasic) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);

  auto c_filter_sni =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);

  Connect();
  ASSERT_TRUE(c_filter_sni->captured());
  CheckSniExtension(c_filter_sni->extension(), kPublicName);
}

TEST_F(TlsConnectStreamTls13, EchAcceptWithResume) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();  // Need to read so that we absorb the session ticket.
  CheckKeys();

  Reset();
  EnsureTlsSetup();
  SetupEch(client_, server_);
  ExpectResumption(RESUME_TICKET);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_pre_shared_key_xtn);
  StartConnect();
  Handshake();
  CheckConnected();
  // Make sure that the PSK extension is only in CHInner.
  ASSERT_FALSE(filter->captured());
}

TEST_F(TlsConnectStreamTls13, EchAcceptWithExternalPsk) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey key(
      PK11_KeyGen(slot.get(), CKM_HKDF_KEY_GEN, nullptr, 16, nullptr));
  ASSERT_TRUE(!!key);
  AddPsk(key, std::string("foo"), ssl_hash_sha256);

  // Not permitted in outer.
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_pre_shared_key_xtn);
  StartConnect();
  Handshake();
  CheckConnected();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
  // Make sure that the PSK extension is only in CHInner.
  ASSERT_FALSE(filter->captured());
}

// If an earlier version is negotiated, False Start must be disabled.
TEST_F(TlsConnectStreamTls13, EchDowngradeNoFalseStart) {
  EnsureTlsSetup();
  SetupEch(client_, server_, HpkeDhKemX25519Sha256, false, true, false);
  MakeTlsFilter<TlsExtensionDropper>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn);
  client_->EnableFalseStart();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  StartConnect();
  client_->Handshake();
  server_->Handshake();
  client_->Handshake();
  EXPECT_FALSE(client_->can_falsestart_hook_called());

  // Make sure the write is blocked.
  client_->ExpectReadWriteError();
  client_->SendData(10);
}

SSLHelloRetryRequestAction RetryEchHello(PRBool firstHello,
                                         const PRUint8* clientToken,
                                         unsigned int clientTokenLen,
                                         PRUint8* appToken,
                                         unsigned int* appTokenLen,
                                         unsigned int appTokenMax, void* arg) {
  auto* called = reinterpret_cast<size_t*>(arg);
  ++*called;

  EXPECT_EQ(0U, clientTokenLen);
  return firstHello ? ssl_hello_retry_request : ssl_hello_retry_accept;
}

// Generate HRR on CH1 Inner
TEST_F(TlsConnectStreamTls13, EchAcceptWithHrr) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), echconfig.data(),
                                    echconfig.len()));
  client_->ExpectEch();
  server_->ExpectEch();
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  client_->ExpectEch();
  server_->ExpectEch();
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  Handshake();
  EXPECT_EQ(1U, cb_called);
  CheckConnected();
  SendReceive();
}

// Send GREASE ECH in CH1. CH2 must send exactly the same GREASE ECH contents.
TEST_F(TlsConnectStreamTls13, GreaseEchHrrMatches) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_TRUE));  // GREASE
  auto capture = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);

  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();  // Send CH1
  EXPECT_TRUE(capture->captured());
  DataBuffer ch1_grease = capture->extension();

  server_->Handshake();
  MakeNewServer();
  capture = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);

  EXPECT_FALSE(capture->captured());
  client_->Handshake();  // Send CH2
  EXPECT_TRUE(capture->captured());
  EXPECT_EQ(ch1_grease, capture->extension());

  EXPECT_EQ(1U, cb_called);
  server_->StartConnect();
  Handshake();
  CheckConnected();
}

// Fail to decrypt CH2. Unlike CH1, this generates an alert.
TEST_F(TlsConnectStreamTls13, EchFailDecryptCH2) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  EXPECT_EQ(1U, cb_called);
  // Stop the callback from being called in future handshakes.
  EXPECT_EQ(SECSuccess,
            SSL_HelloRetryRequestCallback(server_->ssl_fd(), nullptr, nullptr));

  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 80);
  ExpectAlert(server_, kTlsAlertDecryptError);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
}

// Change the ECH advertisement between CH1 and CH2. Use GREASE for simplicity.
TEST_F(TlsConnectStreamTls13, EchHrrChangeCh2OfferingYN) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  // Start the handshake, send GREASE ECH.
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_TRUE));  // GREASE
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  ExpectAlert(server_, kTlsAlertMissingExtension);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_MISSING_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  EXPECT_EQ(1U, cb_called);
}

TEST_F(TlsConnectStreamTls13, EchHrrChangeCh2OfferingNY) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  SetupEch(client_, server_);
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  MakeTlsFilter<TlsExtensionDropper>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  client_->ClearFilter();  // Let the second ECH offering through.
  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  EXPECT_EQ(1U, cb_called);
}

// Change the ECHCipherSuite between CH1 and CH2. Expect alert.
TEST_F(TlsConnectStreamTls13, EchHrrChangeCipherSuite) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  SetupEch(client_, server_);

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));
  // Start the handshake and trigger HRR.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();

  // Damage the first byte of the ciphersuite (offset 0)
  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 0);

  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  EXPECT_EQ(1U, cb_called);
}

// Configure an external PSK. Generate an HRR off CH1Inner (which contains
// the PSK extension). Use the same PSK in CH2 and connect.
TEST_F(TlsConnectStreamTls13, EchAcceptWithHrrAndPsk) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), echconfig.data(),
                                    echconfig.len()));
  client_->ExpectEch();
  server_->ExpectEch();

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  static const uint8_t key_buf[16] = {0};
  SECItem key_item = {siBuffer, const_cast<uint8_t*>(&key_buf[0]),
                      sizeof(key_buf)};
  const char* label = "foo";
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), CKM_HKDF_KEY_GEN,
                                         PK11_OriginUnwrap, CKA_DERIVE,
                                         &key_item, nullptr));
  ASSERT_TRUE(!!key);
  AddPsk(key, std::string(label), ssl_hash_sha256);

  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  client_->ExpectEch();
  server_->ExpectEch();
  EXPECT_EQ(SECSuccess,
            SSL_AddExternalPsk0Rtt(server_->ssl_fd(), key.get(),
                                   reinterpret_cast<const uint8_t*>(label),
                                   strlen(label), ssl_hash_sha256, 0, 1000));
  server_->ExpectPsk();
  Handshake();
  EXPECT_EQ(1U, cb_called);
  CheckConnected();
  SendReceive();
}

// Generate an HRR on CHOuter. Reject ECH on the second CH.
TEST_F(TlsConnectStreamTls13Ech, EchRejectWithHrr) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  SetupForEchRetry();

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);

  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  ExpectAlert(client_, kTlsAlertEchRequired);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  EXPECT_EQ(1U, cb_called);
}

// Reject ECH on CH1 and CH2. PSKs are no longer allowed
// in CHOuter, but we can still make sure the handshake succeeds.
// This prompts an ech_required alert when the handshake completes.
TEST_F(TlsConnectStreamTls13, EchRejectWithHrrAndPsk) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), echconfig.data(),
                                    echconfig.len()));

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  // Add a PSK to both endpoints.
  static const uint8_t key_buf[16] = {0};
  SECItem key_item = {siBuffer, const_cast<uint8_t*>(&key_buf[0]),
                      sizeof(key_buf)};
  const char* label = "foo";
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), CKM_HKDF_KEY_GEN,
                                         PK11_OriginUnwrap, CKA_DERIVE,
                                         &key_item, nullptr));
  ASSERT_TRUE(!!key);
  AddPsk(key, std::string(label), ssl_hash_sha256);
  client_->ExpectPsk(ssl_psk_none);

  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  EXPECT_EQ(SECSuccess,
            SSL_AddExternalPsk0Rtt(server_->ssl_fd(), key.get(),
                                   reinterpret_cast<const uint8_t*>(label),
                                   strlen(label), ssl_hash_sha256, 0, 1000));
  // Don't call ExpectPsk
  ExpectAlert(client_, kTlsAlertEchRequired);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  EXPECT_EQ(1U, cb_called);
}

// ECH (both connections), resumption rejected.
TEST_F(TlsConnectStreamTls13, EchRejectResume) {
  EnsureTlsSetup();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  SetupEch(client_, server_);
  Connect();
  SendReceive();

  Reset();
  ClearServerCache();  // Invalidate the ticket
  ConfigureSessionCache(RESUME_BOTH, RESUME_NONE);
  ExpectResumption(RESUME_NONE);
  SetupEch(client_, server_);
  Connect();
  SendReceive();
}

// ECH (both connections) + 0-RTT
TEST_F(TlsConnectStreamTls13, EchZeroRttBoth) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  SetupEch(client_, server_);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

// ECH (first connection only) + 0-RTT
TEST_F(TlsConnectStreamTls13, EchZeroRttFirst) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

// ECH (second connection only) + 0-RTT
TEST_F(TlsConnectStreamTls13, EchZeroRttSecond) {
  EnsureTlsSetup();
  SetupForZeroRtt();  // Get a ticket
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  SetupEch(client_, server_);
  ExpectResumption(RESUME_TICKET);
  ZeroRttSendReceive(true, true);
  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

// ECH (first connection only, reject on second) + 0-RTT
TEST_F(TlsConnectStreamTls13, EchZeroRttRejectSecond) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  // Setup ECH only on the client.
  SetupEch(client_, server_, HpkeDhKemX25519Sha256, false, true, false);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);

  ExpectResumption(RESUME_NONE);
  ExpectAlert(client_, kTlsAlertEchRequired);
  ZeroRttSendReceive(true, false);
  server_->Handshake();
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);

  ExpectEarlyDataAccepted(false);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  // Reset expectations for the TlsAgent dtor.
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

// Test a critical extension in ECHConfig
TEST_F(TlsConnectStreamTls13, EchRejectUnknownCriticalExtension) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  DataBuffer crit_rec;
  DataBuffer len_buf;
  uint64_t tmp;

  static const uint8_t crit_extensions[] = {0x00, 0x04, 0xff, 0xff, 0x00, 0x00};
  static const uint8_t extensions[] = {0x00, 0x04, 0x7f, 0xff, 0x00, 0x00};
  DataBuffer crit_exts(crit_extensions, sizeof(crit_extensions));
  DataBuffer non_crit_exts(extensions, sizeof(extensions));

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteChaCha,
                                        kPublicName, 100, echconfig, pub, priv);
  echconfig.Truncate(echconfig.len() - 2);  // Eat the empty extensions.
  crit_rec.Assign(echconfig);
  ASSERT_TRUE(crit_rec.Read(0, 2, &tmp));
  len_buf.Write(0, tmp + crit_exts.len() - 2, 2);  // two bytes of length
  crit_rec.Splice(len_buf, 0, 2);
  len_buf.Truncate(0);

  ASSERT_TRUE(crit_rec.Read(4, 2, &tmp));
  len_buf.Write(0, tmp + crit_exts.len() - 2, 2);  // two bytes of length
  crit_rec.Append(crit_exts);
  crit_rec.Splice(len_buf, 4, 2);
  len_buf.Truncate(0);

  ASSERT_TRUE(echconfig.Read(0, 2, &tmp));
  len_buf.Write(0, tmp + non_crit_exts.len() - 2, 2);
  echconfig.Append(non_crit_exts);
  echconfig.Splice(len_buf, 0, 2);
  ASSERT_TRUE(echconfig.Read(4, 2, &tmp));
  len_buf.Write(0, tmp + non_crit_exts.len() - 2, 2);
  echconfig.Splice(len_buf, 4, 2);

  EXPECT_EQ(SECFailure,
            SSL_SetClientEchConfigs(client_->ssl_fd(), crit_rec.data(),
                                    crit_rec.len()));
  EXPECT_EQ(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION, PORT_GetError());
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  StartConnect();
  client_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  ASSERT_FALSE(filter->captured());

  // Now try a variant with non-critical extensions, it should work.
  Reset();
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), echconfig.data(),
                                    echconfig.len()));
  filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  StartConnect();
  client_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, client_->state());
  ASSERT_TRUE(filter->captured());
}

// Secure disable without ECH
TEST_F(TlsConnectStreamTls13, EchRejectAuthCertSuccessNoRetries) {
  EnsureTlsSetup();
  SetupEch(client_, server_, HpkeDhKemX25519Sha256, false, true, false);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  ExpectAlert(client_, kTlsAlertEchRequired);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  // Reset expectations for the TlsAgent dtor.
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

// When authenticating to the public name, the client MUST NOT
// send a certificate in response to a certificate request.
TEST_F(TlsConnectStreamTls13, EchRejectSuppressClientCert) {
  EnsureTlsSetup();
  SetupEch(client_, server_, HpkeDhKemX25519Sha256, false, true, false);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  auto cert_capture =
      MakeTlsFilter<TlsHandshakeRecorder>(client_, kTlsHandshakeCertificate);
  cert_capture->EnableDecryption();

  StartConnect();
  client_->ExpectSendAlert(kTlsAlertEchRequired);
  server_->ExpectSendAlert(kTlsAlertCertificateRequired);
  ConnectExpectFail();

  static const uint8_t empty_cert[4] = {0};
  EXPECT_EQ(DataBuffer(empty_cert, sizeof(empty_cert)), cert_capture->buffer());
}

// Secure disable with incompatible ECHConfig
TEST_F(TlsConnectStreamTls13, EchRejectAuthCertSuccessIncompatibleRetries) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey server_pub;
  ScopedSECKEYPrivateKey server_priv;
  ScopedSECKEYPublicKey client_pub;
  ScopedSECKEYPrivateKey client_priv;
  DataBuffer server_rec;
  DataBuffer client_rec;

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteChaCha,
                                        kPublicName, 100, server_rec,
                                        server_pub, server_priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                    server_priv.get(), server_rec.data(),
                                    server_rec.len()));

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, client_rec,
                                        client_pub, client_priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), client_rec.data(),
                                    client_rec.len()));

  // Change the first ECHConfig version to one we don't understand.
  server_rec.Write(2, 0xfefe, 2);
  // Skip the ECHConfigs length, the server sender will re-encode.
  ASSERT_EQ(SECSuccess, SSLInt_SetRawEchConfigForRetry(server_->ssl_fd(),
                                                       &server_rec.data()[2],
                                                       server_rec.len() - 2));

  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  ExpectAlert(client_, kTlsAlertEchRequired);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  // Reset expectations for the TlsAgent dtor.
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

// Check that an otherwise-accepted ECH fails expectedly
// with a bad certificate.
TEST_F(TlsConnectStreamTls13, EchRejectAuthCertFail) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetAuthCertificateCallback(AuthCompleteFail);
  ConnectExpectAlert(client_, kTlsAlertBadCertificate);
  client_->CheckErrorCode(SSL_ERROR_BAD_CERTIFICATE);
  server_->CheckErrorCode(SSL_ERROR_BAD_CERT_ALERT);
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
}

TEST_F(TlsConnectStreamTls13Ech, EchShortClientEncryptedCH) {
  EnsureTlsSetup();
  SetupForEchRetry();
  auto filter = MakeTlsFilter<TlsExtensionResizer>(
      client_, ssl_tls13_encrypted_client_hello_xtn, 1);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
}

TEST_F(TlsConnectStreamTls13Ech, EchLongClientEncryptedCH) {
  EnsureTlsSetup();
  SetupForEchRetry();
  auto filter = MakeTlsFilter<TlsExtensionResizer>(
      client_, ssl_tls13_encrypted_client_hello_xtn, 1000);
  ConnectExpectAlert(server_, kTlsAlertDecodeError);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
}

TEST_F(TlsConnectStreamTls13Ech, EchShortServerEncryptedCH) {
  EnsureTlsSetup();
  SetupForEchRetry();
  auto filter = MakeTlsFilter<TlsExtensionResizer>(
      server_, ssl_tls13_encrypted_client_hello_xtn, 1);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, kTlsAlertDecodeError);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
  server_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, EchLongServerEncryptedCH) {
  EnsureTlsSetup();
  SetupForEchRetry();
  auto filter = MakeTlsFilter<TlsExtensionResizer>(
      server_, ssl_tls13_encrypted_client_hello_xtn, 1000);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, kTlsAlertDecodeError);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
  server_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
}

// Check that if authCertificate fails, retry_configs
// are not available to the application.
TEST_F(TlsConnectStreamTls13Ech, EchInsecureFallbackNoRetries) {
  EnsureTlsSetup();
  StackSECItem retry_configs;
  SetupForEchRetry();

  // Use the filter to make sure retry_configs are sent.
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  filter->EnableDecryption();

  client_->SetAuthCertificateCallback(AuthCompleteFail);
  ConnectExpectAlert(client_, kTlsAlertBadCertificate);
  client_->CheckErrorCode(SSL_ERROR_BAD_CERTIFICATE);
  server_->CheckErrorCode(SSL_ERROR_BAD_CERT_ALERT);
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  EXPECT_EQ(SECFailure,
            SSL_GetEchRetryConfigs(client_->ssl_fd(), &retry_configs));
  EXPECT_EQ(SSL_ERROR_HANDSHAKE_NOT_COMPLETED, PORT_GetError());
  ASSERT_EQ(0U, retry_configs.len);
  EXPECT_TRUE(filter->captured());
}

// Test that mismatched ECHConfigContents triggers a retry.
TEST_F(TlsConnectStreamTls13Ech, EchMismatchHpkeCiphersRetry) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey server_pub;
  ScopedSECKEYPrivateKey server_priv;
  ScopedSECKEYPublicKey client_pub;
  ScopedSECKEYPrivateKey client_priv;
  DataBuffer server_rec;
  DataBuffer client_rec;

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteChaCha,
                                        kPublicName, 100, server_rec,
                                        server_pub, server_priv);
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, client_rec,
                                        client_pub, client_priv);

  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                    server_priv.get(), server_rec.data(),
                                    server_rec.len()));
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), client_rec.data(),
                                    client_rec.len()));

  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  ExpectAlert(client_, kTlsAlertEchRequired);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITH_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  DoEchRetry(server_pub, server_priv, server_rec);
}

// Test that mismatched ECH server keypair triggers a retry.
TEST_F(TlsConnectStreamTls13Ech, EchMismatchKeysRetry) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey server_pub;
  ScopedSECKEYPrivateKey server_priv;
  ScopedSECKEYPublicKey client_pub;
  ScopedSECKEYPrivateKey client_priv;
  DataBuffer server_rec;
  DataBuffer client_rec;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, server_rec,
                                        server_pub, server_priv);
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, client_rec,
                                        client_pub, client_priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                    server_priv.get(), server_rec.data(),
                                    server_rec.len()));
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), client_rec.data(),
                                    client_rec.len()));

  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  client_->ExpectSendAlert(kTlsAlertEchRequired);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITH_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  DoEchRetry(server_pub, server_priv, server_rec);
}

// Check that the client validates any server response to GREASE ECH
TEST_F(TlsConnectStreamTls13, EchValidateGreaseResponse) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey server_pub;
  ScopedSECKEYPrivateKey server_priv;
  DataBuffer server_rec;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, server_rec,
                                        server_pub, server_priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                    server_priv.get(), server_rec.data(),
                                    server_rec.len()));

  // Damage the length and expect an alert.
  auto filter = MakeTlsFilter<TlsExtensionDamager>(
      server_, ssl_tls13_encrypted_client_hello_xtn, 0);
  filter->EnableDecryption();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_TRUE));  // GREASE
  ConnectExpectAlert(client_, kTlsAlertDecodeError);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_CONFIG);
  server_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);

  // If the retry_config contains an unknown version, it should be ignored.
  Reset();
  EnsureTlsSetup();
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                    server_priv.get(), server_rec.data(),
                                    server_rec.len()));
  server_rec.Write(2, 0xfefe, 2);
  // Skip the ECHConfigs length, the server sender will re-encode.
  ASSERT_EQ(SECSuccess, SSLInt_SetRawEchConfigForRetry(server_->ssl_fd(),
                                                       &server_rec.data()[2],
                                                       server_rec.len() - 2));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_TRUE));  // GREASE
  Connect();

  // Lastly, if we DO support the retry_config, GREASE ECH should ignore it.
  Reset();
  EnsureTlsSetup();
  server_rec.Write(2, ssl_tls13_encrypted_client_hello_xtn, 2);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), server_pub.get(),
                                    server_priv.get(), server_rec.data(),
                                    server_rec.len()));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_TRUE));  // GREASE
  Connect();
}

// Test a tampered CHInner (decrypt failure).
// Expect negotiation on outer, which fails due to the tampered transcript.
TEST_F(TlsConnectStreamTls13, EchBadCiphertext) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  /* Target the payload:
     struct {
            ECHCipherSuite suite;      // 4B
            opaque config_id<0..255>;  // 32B
            opaque enc<1..2^16-1>;     // 32B for X25519
            opaque payload<1..2^16-1>;
        } ClientEncryptedCH;
  */
  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 80);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

// Test a tampered CHOuter (decrypt failure on AAD).
// Expect negotiation on outer, which fails due to the tampered transcript.
TEST_F(TlsConnectStreamTls13, EchOuterBinding) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);

  static const uint8_t supported_vers_13[] = {0x02, 0x03, 0x04};
  DataBuffer buf(supported_vers_13, sizeof(supported_vers_13));
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_tls13_supported_versions_xtn,
                                      buf);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

// Test a bad (unknown) ECHCipherSuite.
// Expect negotiation on outer, which fails due to the tampered transcript.
TEST_F(TlsConnectStreamTls13, EchBadCiphersuite) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  /* Make KDF unknown */
  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 0);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();

  Reset();
  EnsureTlsSetup();
  SetupEch(client_, server_);
  /* Make AEAD unknown */
  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 3);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

// Connect to a 1.2 server, it should ignore ECH.
TEST_F(TlsConnectStreamTls13, EchToTls12Server) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  client_->ExpectEch(false);
  server_->ExpectEch(false);
  Connect();
}

TEST_F(TlsConnectStreamTls13, NoEchFromTls12Client) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  SetExpectedVersion(SSL_LIBRARY_VERSION_TLS_1_2);
  Connect();
  ASSERT_FALSE(filter->captured());
}

TEST_F(TlsConnectStreamTls13, EchOuterWith12Max) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);

  static const uint8_t supported_vers_12[] = {0x02, 0x03, 0x03};
  DataBuffer buf(supported_vers_12, sizeof(supported_vers_12));

  StartConnect();
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_tls13_supported_versions_xtn,
                                      buf);

  // Server should ignore the extension if 1.2 is negotiated.
  // Here the CHInner is not modified, so if Accepted we'd connect.
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
  client_->CheckErrorCode(SSL_ERROR_DECRYPT_ERROR_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE);
  ASSERT_FALSE(filter->captured());
}

TEST_F(TlsConnectStreamTls13, EchOuterExtensionsInCHOuter) {
  EnsureTlsSetup();
  uint8_t outer[2] = {0};
  DataBuffer outer_buf(outer, sizeof(outer));
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello,
                                      ssl_tls13_outer_extensions_xtn,
                                      outer_buf);

  ConnectExpectAlert(server_, kTlsAlertUnsupportedExtension);
  client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

// At draft-09: If a CH containing the ech_is_inner extension is received, the
// server acts as backend server in split-mode by responding with the ECH
// acceptance signal. The signal value itself depends on the handshake secret,
// which we've broken by appending ech_is_inner. For now, just check that the
// server negotiates ech_is_inner (which is what triggers sending the signal).
TEST_F(TlsConnectStreamTls13, EchBackendAcceptance) {
  DataBuffer ch_buf;
  static uint8_t empty_buf[1] = {0};
  DataBuffer empty(empty_buf, 0);

  EnsureTlsSetup();
  StartConnect();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE));
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello,
                                      ssl_tls13_ech_is_inner_xtn, empty);

  EXPECT_EQ(SECSuccess, SSL_EnableTls13BackendEch(server_->ssl_fd(), PR_TRUE));
  client_->Handshake();
  server_->Handshake();

  ExpectAlert(client_, kTlsAlertBadRecordMac);
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  EXPECT_EQ(PR_TRUE, SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                                ssl_tls13_ech_is_inner_xtn));
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

INSTANTIATE_TEST_SUITE_P(EchAgentTest, TlsAgentEchTest,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV13));
#else

TEST_P(TlsAgentEchTest, NoEchWithoutHpke) {
  EnsureInit();
  uint8_t non_null[1];
  SECKEYPublicKey pub;
  SECKEYPrivateKey priv;
  ASSERT_EQ(SECFailure, SSL_SetClientEchConfigs(agent_->ssl_fd(), non_null,
                                                sizeof(non_null)));
  ASSERT_EQ(SSL_ERROR_FEATURE_DISABLED, PORT_GetError());

  ASSERT_EQ(SECFailure, SSL_SetServerEchConfigs(agent_->ssl_fd(), &pub, &priv,
                                                non_null, sizeof(non_null)));
  ASSERT_EQ(SSL_ERROR_FEATURE_DISABLED, PORT_GetError());
}

INSTANTIATE_TEST_SUITE_P(EchAgentTest, TlsAgentEchTest,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV13));

#endif  // NSS_ENABLE_DRAFT_HPKE
}  // namespace nss_test
