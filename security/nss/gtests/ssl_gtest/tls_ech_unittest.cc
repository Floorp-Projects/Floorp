/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"

#include "gtest_utils.h"
#include "pk11pub.h"
#include "tls_agent.h"
#include "tls_connect.h"
#include "util.h"
#include "tls13ech.h"

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

#include "cpputil.h"  // Unused function error if included without HPKE.

static std::string kPublicName("public.name");

static const std::vector<HpkeSymmetricSuite> kDefaultSuites = {
    {HpkeKdfHkdfSha256, HpkeAeadChaCha20Poly1305},
    {HpkeKdfHkdfSha256, HpkeAeadAes128Gcm}};
static const std::vector<HpkeSymmetricSuite> kSuiteChaCha = {
    {HpkeKdfHkdfSha256, HpkeAeadChaCha20Poly1305}};
static const std::vector<HpkeSymmetricSuite> kSuiteAes = {
    {HpkeKdfHkdfSha256, HpkeAeadAes128Gcm}};
std::vector<HpkeSymmetricSuite> kBogusSuite = {
    {static_cast<HpkeKdfId>(0xfefe), static_cast<HpkeAeadId>(0xfefe)}};
static const std::vector<HpkeSymmetricSuite> kUnknownFirstSuite = {
    {static_cast<HpkeKdfId>(0xfefe), static_cast<HpkeAeadId>(0xfefe)},
    {HpkeKdfHkdfSha256, HpkeAeadAes128Gcm}};

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
    std::vector<uint8_t> pkcs8_r = hex_string_to_bytes(kFixedServerKey);
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

  void ValidatePublicNames(const std::vector<std::string>& names,
                           SECStatus expected) {
    static const std::vector<HpkeSymmetricSuite> kSuites = {
        {HpkeKdfHkdfSha256, HpkeAeadAes128Gcm}};

    ScopedSECItem ecParams = MakeEcKeyParams(ssl_grp_ec_curve25519);
    ScopedSECKEYPublicKey pub;
    ScopedSECKEYPrivateKey priv;
    SECKEYPublicKey* pub_p = nullptr;
    SECKEYPrivateKey* priv_p =
        SECKEY_CreateECPrivateKey(ecParams.get(), &pub_p, nullptr);
    pub.reset(pub_p);
    priv.reset(priv_p);
    ASSERT_TRUE(!!pub);
    ASSERT_TRUE(!!priv);

    EnsureTlsSetup();

    DataBuffer cfg;
    SECStatus rv;
    for (auto name : names) {
      if (g_ssl_gtest_verbose) {
        std::cout << ((expected == SECFailure) ? "in" : "")
                  << "valid public_name: " << name << std::endl;
      }
      GenerateEchConfig(HpkeDhKemX25519Sha256, kSuites, name, 100, cfg, pub,
                        priv);

      rv = SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                   cfg.data(), cfg.len());
      EXPECT_EQ(expected, rv);

      rv = SSL_SetClientEchConfigs(client_->ssl_fd(), cfg.data(), cfg.len());
      EXPECT_EQ(expected, rv);
    }
  }

 private:
  // Testing certan invalid CHInner configurations is tricky, particularly
  // since the CHOuter forms AAD and isn't available in filters. Instead of
  // generating these inputs on the fly, use a fixed server keypair so that
  // the input can be generated once (e.g. via a debugger) and replayed in
  // each invocation of the test.
  std::string kFixedServerKey =
      "3067020100301406072a8648ce3d020106092b06010401da470f01044c304a"
      "02010104205a8aa0d2476b28521588e0c704b14db82cdd4970d340d293a957"
      "6deaee9ec1c7a1230321008756e2580c07c1d2ffcb662f5fadc6d6ff13da85"
      "abd7adfecf984aaa102c1269";
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

static SECStatus AuthCompleteFail(TlsAgent* agent, PRBool, PRBool) {
  CheckCertVerifyPublicName(agent);
  return SECFailure;
}

// Given two EchConfigList structures, e.g. from GenerateEchConfig, construct
// a single list containing all entries.
static DataBuffer MakeEchConfigList(DataBuffer config1, DataBuffer config2) {
  DataBuffer sizedConfigListBuffer;

  sizedConfigListBuffer.Write(2, config1.data() + 2, config1.len() - 2);
  sizedConfigListBuffer.Write(sizedConfigListBuffer.len(), config2.data() + 2,
                              config2.len() - 2);
  sizedConfigListBuffer.Write(0, sizedConfigListBuffer.len() - 2, 2);

  PR_ASSERT(sizedConfigListBuffer.len() == config1.len() + config2.len() - 2);
  return sizedConfigListBuffer;
}

TEST_P(TlsAgentEchTest, EchConfigsSupportedYesNo) {
  if (variant_ == ssl_variant_datagram) {
    GTEST_SKIP();
  }
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  // ECHConfig 2 cipher_suites are unsupported.
  DataBuffer config1;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, config1, pub, priv);
  DataBuffer config2;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite,
                                        kPublicName, 100, config2, pub, priv);
  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE

  DataBuffer sizedConfigListBuffer = MakeEchConfigList(config1, config2);
  InstallEchConfig(sizedConfigListBuffer, 0);
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
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer config2;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, config2, pub, priv);
  DataBuffer config1;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite,
                                        kPublicName, 100, config1, pub, priv);
  // ECHConfig 1 cipher_suites are unsupported.
  DataBuffer sizedConfigListBuffer = MakeEchConfigList(config1, config2);
  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(sizedConfigListBuffer, 0);
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

  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer config2;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite,
                                        kPublicName, 100, config2, pub, priv);
  DataBuffer config1;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite,
                                        kPublicName, 100, config1, pub, priv);
  // ECHConfig 1 and 2 cipher_suites are unsupported.
  DataBuffer sizedConfigListBuffer = MakeEchConfigList(config1, config2);
  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(sizedConfigListBuffer, SEC_ERROR_INVALID_ARGS);
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
  // SSL_EncodeEchConfigId encodes without validation.
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

  // EncodeEchConfigId
  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfigId(0, nullptr, 1, static_cast<HpkeKemId>(1),
                                  reinterpret_cast<SECKEYPublicKey*>(1),
                                  reinterpret_cast<HpkeSymmetricSuite*>(1), 1,
                                  reinterpret_cast<uint8_t*>(1),
                                  reinterpret_cast<unsigned int*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfigId(0, "name", 1, static_cast<HpkeKemId>(1),
                                  reinterpret_cast<SECKEYPublicKey*>(1),
                                  nullptr, 1, reinterpret_cast<uint8_t*>(1),
                                  reinterpret_cast<unsigned int*>(1), 1));
  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfigId(0, "name", 1, static_cast<HpkeKemId>(1),
                                  reinterpret_cast<SECKEYPublicKey*>(1),
                                  reinterpret_cast<HpkeSymmetricSuite*>(1), 0,
                                  reinterpret_cast<uint8_t*>(1),
                                  reinterpret_cast<unsigned int*>(1), 1));

  EXPECT_EQ(SECFailure, SSL_EncodeEchConfigId(
                            0, "name", 1, static_cast<HpkeKemId>(1), nullptr,
                            reinterpret_cast<HpkeSymmetricSuite*>(1), 1,
                            reinterpret_cast<uint8_t*>(1),
                            reinterpret_cast<unsigned int*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfigId(0, nullptr, 0, static_cast<HpkeKemId>(1),
                                  reinterpret_cast<SECKEYPublicKey*>(1),
                                  reinterpret_cast<HpkeSymmetricSuite*>(1), 1,
                                  reinterpret_cast<uint8_t*>(1),
                                  reinterpret_cast<unsigned int*>(1), 1));

  EXPECT_EQ(SECFailure, SSL_EncodeEchConfigId(
                            0, "name", 1, static_cast<HpkeKemId>(1),
                            reinterpret_cast<SECKEYPublicKey*>(1),
                            reinterpret_cast<HpkeSymmetricSuite*>(1), 1,
                            nullptr, reinterpret_cast<unsigned int*>(1), 1));

  EXPECT_EQ(SECFailure,
            SSL_EncodeEchConfigId(0, "name", 1, static_cast<HpkeKemId>(1),
                                  reinterpret_cast<SECKEYPublicKey*>(1),
                                  reinterpret_cast<HpkeSymmetricSuite*>(1), 1,
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

TEST_F(TlsConnectStreamTls13Ech, EchFixedConfig) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EnsureTlsSetup();
  ImportFixedEchKeypair(pub, priv);
  SetMutualEchConfigs(pub, priv);

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

// The next set of tests all use a fixed server key and a pre-built ClientHello.
// This ClientHelo can be constructed using the above EchFixedConfig test,
// modifying tls13_ConstructInnerExtensionsFromOuter as indicated.  For this
// small number of tests, these fixed values are easier to construct than
// constructing ClientHello in the test that can be successfully decrypted.

// Test an encoded ClientHelloInner containing an extra extensionType
// in outer_extensions, for which there is no corresponding (uncompressed)
// extension in ClientHelloOuter.
TEST_F(TlsConnectStreamTls13Ech, EchOuterExtensionsReferencesMissing) {
  // Construct this by prepending 0xabcd to ssl_tls13_outer_extensions_xtn.
  std::string ch =
      "010001fc030390901d039ca83262d9115a5f98f43ddb2553241a8de5c46d9f118c4c29c2"
      "64bc000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00206df5f908d1c02320e246694c765d5ec1c0f7d7aef2b1b00b17c36331623d332d002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00209a4f67b0744d1fba23aa4bacfadb2a"
      "c706562dae04d80a83ae668a6f2dd6ef2700cfab1671182341df246d66c3aca873e8c714"
      "bc2b1c3b576653609533c486df0bdcf63ab4e4e7d0b67fadf4e3504eec96f72e6778b15d"
      "69c9a9594a041348a7130f67a1a7cac796a0e6d6fca505438355278a9a8fd55e44218441"
      "9927a1e084ac7d7adeb2f0c19faafba430876bf0cdf4d195b2d06428b3de13120f65748a"
      "468f8997a2c3bf1dd7f3996a0f2c70dea6c88149df182b3c3b78a8da8bb709a9ed9d77c6"
      "5dc09accdfeb66c90db26b99a35052a8cbaf7bb9307a1e17d90a7aa9f768f5f446559d08"
      "69bccc83eda9d2b347a00015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, EchOuterExtensionsInsideInner) {
  // Construct this by appending ssl_tls13_outer_extensions_xtn to the
  // references in
  // ssl_tls13_outer_extensions_xtn.
  std::string ch =
      "010001fc03035e2268bc7133079cd33eb088253393e561d80c5ee6f9a238aff022e1e10d"
      "4c82000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00200e071fd982854d50236ed0e4e7981460840f03d03fd84b44c409fe486203b252002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d002099a032502ea4fd3c85b858ae1c59df"
      "6a374f3698ed6bca188cf75c432c78cf5a00cf28dde32de7ade40abb16d550c1eec3dad4"
      "a03c85efb95ec605837deae92a419285116e5cb8223ea53cff2b605e66f28e96d37e9b4e"
      "3035fb1cfa125fa053d6770091b5731c9fb03e872a82991dfdd24ad8399fcc76db7fadba"
      "029e064beb02c1282684a93e777bcefbca3dd143dfc225d2e65c80dbf3819ebda288e32c"
      "3a1f8a27bb3aa9480dee2a4307073da3e15ee03dba386223d9399ad796af80c646f85406"
      "282c34fd9406d25752087f08140e1be834e8a149f0bebfc2b3db16ccba83c37051e2e75d"
      "e8a4e999ef385c74c96d0015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, EchOuterExtensionsDuplicateReference) {
  // Construct this by altering tls13_ConstructInnerExtensionsFromOuter to have
  // each extension inserted in the reference list twice and running the
  // EchFixedConfig test.
  std::string ch =
      "010001fc0303d8717df80286fcd8b4242ed846995c6473e290678231046bb1bfc7848460"
      "b122000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00206f21d5fdf7bf81943939a03656c1195ad347cec453bf7a16d0773ffef481d22f002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d011900000100034d002027eb9b641ba8ffc3a4028d00d1f5bd"
      "e190736b1ea5a79513dee0a551cc6fe55200efc2ed6bf501f100896eb91221ce512c20c3"
      "c5c110e7be6a5d340854ff5ac0175312631b021fd5a5c9841549989f415c4041a4b384b1"
      "dba1d6b4182cc48904f993a15eab6bf7787b267ca65acef51c019508e0c9b382086a71d8"
      "517cf19644d66d396efc066a4d37916d67b0e5fe08d52dd94d068dd85b9a245aaffac4ff"
      "66d9a5221fd5805473bb7584eb7f218357c00aff890d2f2edf1c092c648c888b5cba1ca6"
      "26817fda7765fcedfbc418b90b1841d878ed443593cafb61fa8fb708c53977615b45f545"
      "2a8236cab3ec121cdc91a2de6a79437cae9d09e781339fddcac005ce62fd65d50e33faa2"
      "2366955a0374001500220000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, EchOuterExtensionsOutOfOrder) {
  // Construct this by altering tls13_ConstructInnerExtensionsFromOuter to leave
  // a gap at the start and insert a 'late' extension there.
  std::string ch =
      "010001fc0303fabff6caf4d8b1eb1db5945c96badefec4b33188b97121e6a206e82b74bd"
      "a855000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00208fe970fc0c908f0c51734f18467e640df1d45a6ace2948b5c4bf73ee52ab3160002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00203339239f8925c3f9b89f4ced17c3b3"
      "1c649299d7e10b3cdbc115de2a57d90d2200cf006e62866516380e8a16763bee5b2a75a8"
      "74e8698c459f474d0e952c2fd3300bef1decd6f259b8ac2912684ef69b7a7be2520fbf15"
      "5e0c3f88998789976ca1fbcaa40616fc513e3353540db091da76ca98007532974550d3da"
      "aaddb799baf60adbc5800df30e187251427fe9de707d18a270352ee44f6eb37f0d8c72a1"
      "5f9ffb5dd4bbb6045473c8d99b7a5c2c8cc59027f346cbe6ef240d5cf1919f58a998d427"
      "0f8c882d03d22ec4df4079e15a639452ea4c24023f6bcad89566ce6a32b1dad6ddf6b436"
      "3e6759bd48bed1b30a840015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Drop supported_versions from CHInner, make sure we don't negotiate 1.2+ECH.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12Inner) {
  // Construct this by removing ssl_tls13_supported_versions_xtn entirely.
  std::string ch =
      "010001fc030338e9ebcde2b87ef779c4d9a9b9870aef3978130b254fbf168a92644c97c1"
      "c5cb000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "002081b3ea444fd631f9264e01276bcc1a6771aed3b5a8a396446467d1c820e52b25002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00205864042b43f4d4d544558fbcba410f"
      "ebfb78ddfc5528672a7f7d9e70abc3eb6300cf6ff3271da628139bddc4a58ee92db26170"
      "7310dee54d88c8a96a8d998b8608d5f10260b7e201e5dc8cafa13917a3fdfdf399082959"
      "8adf3c291decf640f696e64c4e22bafb81565587c50dd829ccad68bd00babeaba7d8a7a5"
      "400ad3200dbae674c549953ca6d3298ed751a9bc215a33be444fe908bf1c6f374cc139f9"
      "98339f58b8fd3510a670e4102e3f7de21586ebd70c3fb1df8bb6b9e5dbc0db147dbac6d0"
      "72dfc6cdf17ecee5c019c311b37ef9f5ceabb7edbdf87a4a04041c4d8b512a16517c5380"
      "e8d4f6e3b2412b4a6c030015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Use CHInner supported_versions to negotiate 1.2.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12InnerSupportedVersions) {
  // Construct this by changing ssl_tls13_supported_versions_xtn to write
  // TLS 1.2 instead of TLS 1.3.
  std::string ch =
      "010001fc0303f7146bdc88c399feb49c62b796db2f8b1330e25292a889edf7c65231d0be"
      "b95f000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020d31f8eb204efba49dbdbf40bb046b1e0b90fa3f034260d60f351d4b15e614e7f002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d0020eaa25e92721e65fd405577bf2fd322"
      "857e60f8766a595929fc404c9a01ef441200cf04992c693fbc8eac87726b336a11abc411"
      "541ceff50d533d4cf4d6e1078479acb5446675b652f22d6db04daf0c3640ec2429ba4f51"
      "99c00daa43e9a7d85bd6733041feeca0b38ee6ca07042c7e67d40cd3e236499f3f9d92ab"
      "e4642e483c75d77c247b0228bc773c09551d15845c35663afd1805c5b3adb136ffa6d94f"
      "b7cbfe93d5d33c894b2a6437ad9a2278d5863ed20db652a6084c9e95a8dfaf821d0b474a"
      "7efc2839f110edb4a73376ecab629b26b1eea63304899c49a07157fbbee67c786686cb04"
      "a53666a74e1e003aefc70015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Replay a CH for which CHInner lacks the required ech xtn of inner type
TEST_F(TlsConnectStreamTls13Ech, EchInnerMissing) {
  // Construct by omitting the ech inner extension
  std::string ch =
      "010001fc0303fa9cd9cf5b77bb4083f69a1d169d44b356faea0d6a0aee6d50412de6fef7"
      "8d22000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020c329f1dde4d51b50f68c21053b545290b250af527b2832d3acf2c6af9b8b8d5c002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00207e2a0397b7d2776ae468057d630243"
      "b01388cf80680b074323adf4091aba7b4c00cff4b649fb5b3a0719c1e085c7006a95eaad"
      "32375b717a42d009c075e6246342fdc1e847c528495f90378ff5b4912da5190f7e8bfa1c"
      "c9744b50e9e469cd7cd12bcb5f6534b7d617459d2efa4d796ad244567c49f1d22feb08a5"
      "8e8ebdce059c28883dd69ca401e189f3ef438c3f0bf3d377e6727a1f6abf3a8a8cc149ee"
      "60a1aa5ba4a50e99d2519216762558e9613a238bd630b5822f549575d9402f8da066aaef"
      "2e0e6a7a04583b041925e0ef4575107c4436f9af26e561c0ab733cd88bee6a20e6414128"
      "ea0ba1c73612bb62c1e90015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_MISSING_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, EchInnerWrongSize) {
  // Construct by including ech inner with wrong size
  std::string ch =
      "010001fc03035f8410dab9e49b0833d13390f3fe0b3c6321d842961c9cc46b59a0b5b8e1"
      "4e0b000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020526a56087d685e574accb0e87d6781bc553612479e56460fe6a497fa1cd74e2e002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00200d096bf6ac0c3bcb79d70677da0e0d"
      "249b40bc5ba6b8727654619fe6567d0b0700cfd13e136d2d041e3cd993b252386d97e98d"
      "c972d29d28e0281a210fa56156b95e4371a6610a0b3e65f1b842875fb456de9b9c0e03f8"
      "aa4d1055057ac3e20e5fa45b837ccbb06ef3856c71f1f63e91b60bfb5f3415f26e9a0d3c"
      "4d404d5d5aaa6dca8d57cf2e6b4aaf399fa7271b0c1eedbfdd85fbc9711b0446eb9c9535"
      "a74f3e5a71e2e22dc8d89980f96233ec9b80fbe4f295ff7903bade407fc544c8d76df4fb"
      "ce4b8d79cea0ff7e0b0736ecbeaf5a146a4f81a930e788ae144cf2219e90dc3594165a7e"
      "2a0b64f6189a87a348840015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertDecodeError,
                             SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION,
                             SSL_ERROR_DECODE_ERROR_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, InnerWithEchAndEchIsInner) {
  // Construct by appending an empty ssl_tls13_encrypted_client_hello_xtn of
  // type outer to
  // CHInner.
  std::string ch =
      "010001fc0303527df5a8dbcf390c184c5274295283fdba78d05784170d8f3cb8c7d84747"
      "afb5000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "002099461dcfcdc7804a0f34bf3ca49ac39776a7ef4d8edd30fab3599ff59b09f826002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00201da1341e8ba21ff90e025d2438d4e5"
      "b4e8b376befc57cf8c9afb484e6f051b2f00cff747491b810705e5cc8d8a1302468000d9"
      "8660d659d8382a6fc23ca1a582def728eabb363771328035565048213b1d725b20f757be"
      "63d6956cd861aa9d33adcc913de2443695f70e130af96fd2b078dd662478a29bd17a4479"
      "715c949b5fc118456d0243c9d1819cecd0f5fbd1c78dadd6fcd09abe41ca97a00c97efb3"
      "894c9d4bab60dcd150b55608f6260723a08e112e39e6a43f645f85a08085054f27f269bc"
      "1acb9ff5007b04eaef3414767666472e4e24c2a2953f5dc68aeb5207d556f1b872a810b6"
      "686cf83a09db8b474df70015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_UNEXPECTED_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13, EchWithInnerExtNotSplit) {
  static uint8_t type_val[1] = {1};
  DataBuffer type_buffer(type_val, sizeof(type_val));

  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE));
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello,
                                      ssl_tls13_encrypted_client_hello_xtn,
                                      type_buffer);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
}

/* Parameters
 * Length of SNI for first connection
 * Length of SNI for second connection
 * Use GREASE for first connection?
 * Use GREASE for second connection?
 * For both connections, SNI length to pad to.
 */
class EchCHPaddingTest : public TlsConnectStreamTls13,
                         public testing::WithParamInterface<
                             std::tuple<int, int, bool, bool, int>> {};

TEST_P(EchCHPaddingTest, EchChPaddingEqual) {
  auto parameters = GetParam();
  std::string name_str1 = std::string(std::get<0>(parameters), 'a');
  std::string name_str2 = std::string(std::get<1>(parameters), 'a');
  const char* name1 = name_str1.c_str();
  const char* name2 = name_str2.c_str();
  bool grease_mode1 = std::get<2>(parameters);
  bool grease_mode2 = std::get<3>(parameters);
  uint8_t max_name_len = std::get<4>(parameters);

  // Connection 1
  EnsureTlsSetup();
  SSL_SetURL(client_->ssl_fd(), name1);
  if (grease_mode1) {
    EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
    EXPECT_EQ(SECSuccess,
              SSL_SetTls13GreaseEchSize(client_->ssl_fd(), max_name_len));
    client_->ExpectEch(false);
    server_->ExpectEch(false);
  } else {
    SetupEch(client_, server_, HpkeDhKemX25519Sha256, true, true, true,
             max_name_len);
  }
  auto filter1 = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  Connect();
  size_t echXtnLen1 = filter1->extension().len();

  Reset();

  // Connection 2
  EnsureTlsSetup();
  SSL_SetURL(client_->ssl_fd(), name2);
  if (grease_mode2) {
    EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
    EXPECT_EQ(SECSuccess,
              SSL_SetTls13GreaseEchSize(client_->ssl_fd(), max_name_len));
    client_->ExpectEch(false);
    server_->ExpectEch(false);
  } else {
    SetupEch(client_, server_, HpkeDhKemX25519Sha256, true, true, true,
             max_name_len);
  }
  auto filter2 = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  Connect();
  size_t echXtnLen2 = filter2->extension().len();

  // We always expect an ECH extension.
  ASSERT_TRUE(echXtnLen2 > 0 && echXtnLen1 > 0);
  // We expect the ECH extension to round to the same multiple of 32.
  // Note: It will not be 0 % 32 because we pad the Payload, but have a number
  // of extra bytes from the rest of the ECH extension (e.g. ciphersuite)
  ASSERT_EQ(echXtnLen1 % 32, echXtnLen2 % 32);
  // Both connections should have the same size after padding.
  if (name_str1.size() <= max_name_len && name_str2.size() <= max_name_len) {
    ASSERT_EQ(echXtnLen1, echXtnLen2);
  }
}

#define ECH_PADDING_TEST_INSTANTIATE(name, values)                           \
  INSTANTIATE_TEST_SUITE_P(name, EchCHPaddingTest,                           \
                           testing::Combine(values, values, testing::Bool(), \
                                            testing::Bool(), values))

const int kExtremalSNILengths[] = {1, 128, 255};
const int kNormalSNILengths[] = {17, 24, 100};
const int kLongSNILengths[] = {90, 167, 214};

/* Each invocation with N lengths, results in 4N^3 test cases, so we test
 * 3 lots of (4*3^3) rather than all permutations. */
ECH_PADDING_TEST_INSTANTIATE(extremal, testing::ValuesIn(kExtremalSNILengths));
ECH_PADDING_TEST_INSTANTIATE(normal, testing::ValuesIn(kNormalSNILengths));
ECH_PADDING_TEST_INSTANTIATE(lengthy, testing::ValuesIn(kLongSNILengths));

// Check the server rejects ClientHellos with bad padding
TEST_F(TlsConnectStreamTls13Ech, EchChPaddingChecked) {
  // Generate this string by changing the padding in
  // tls13_GenPaddingClientHelloInner
  std::string ch =
      "010001fc03037473367a6eb6773391081b403908fc0c0026aac706889c59ca694d0c1188"
      "c4b3000006130113031302010001cd00000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020f7d8ad5fea0165e115e984e11c43f1d8f255bd8f772b893432d8d7721e91785a002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0d00f900000100034d00207e0ad8e83f8a9c89e1ae4fd65b8091"
      "01e496bbb5f29ce20b299ce58937e2563300cff471a787585e15ae5aff5e4fee7ec988ba"
      "72f8a95db41e793568b0301d553251f0826dc0c3ff658e4e029ef840ae86fa80af4b11b5"
      "3a33fab99887bf8df18bc87abbb1f578f7964848d91a2023cbe7609fcc31bd721865009c"
      "ad68c09e438d677f7c56af76e62c168bdb373bb88962471dacc4ddf654e435cd903f6555"
      "4c9a93ffd2541cd7bce520e7215d15495184b781ca8c138cedd573fbdef1d40e5de82c33"
      "5c9c43370102ecb0b66dd27efc719a9a54589b6e6b599b1b0146e121eae0ab5b2070c12f"
      "4f4f2b099808294a459f0015004200000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_F(TlsConnectStreamTls13Ech, EchConfigList) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EnsureTlsSetup();

  DataBuffer config1;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, config1, pub, priv);
  DataBuffer config2;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, config2, pub, priv);
  DataBuffer configList = MakeEchConfigList(config1, config2);
  SECStatus rv =
      SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                              configList.data(), configList.len());
  printf("%u", rv);
  ASSERT_EQ(rv, SECSuccess);
}

TEST_F(TlsConnectStreamTls13Ech, EchConfigsTrialDecrypt) {
  // Apply two ECHConfigs on the server. They are identical with the exception
  // of the public key: the first ECHConfig contains a public key for which we
  // lack the private value. Use an SSLInt function to zero all the config_ids
  // (client and server), then confirm that trial decryption works.
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  EnsureTlsSetup();
  ImportFixedEchKeypair(pub, priv);
  ScopedSECKEYPublicKey pub2;
  ScopedSECKEYPrivateKey priv2;
  DataBuffer config2;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, config2, pub, priv);
  DataBuffer config1;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteAes,
                                        kPublicName, 100, config1, pub2, priv2);
  // Zero the config id for both, only public key differs.
  config2.Write(7, (uint32_t)0, 1);
  config1.Write(7, (uint32_t)0, 1);
  // Server only knows private key for conf2
  DataBuffer configList = MakeEchConfigList(config1, config2);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    configList.data(), configList.len()));
  ASSERT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                config2.data(), config2.len()));
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

TEST_F(TlsConnectStreamTls13Ech, EchAcceptBasic) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  auto c_filter_sni =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  Connect();
  ASSERT_TRUE(c_filter_sni->captured());
  CheckSniExtension(c_filter_sni->extension(), kPublicName);
}

TEST_F(TlsConnectStreamTls13, EchAcceptWithResume) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
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
  ASSERT_TRUE(filter->captured());
}

TEST_F(TlsConnectStreamTls13, EchAcceptWithExternalPsk) {
  static const std::string kPskId = "testing123";
  EnsureTlsSetup();
  SetupEch(client_, server_);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(!!slot);
  ScopedPK11SymKey key(
      PK11_KeyGen(slot.get(), CKM_HKDF_KEY_GEN, nullptr, 16, nullptr));
  ASSERT_TRUE(!!key);
  AddPsk(key, kPskId, ssl_hash_sha256);

  // Not permitted in outer.
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_pre_shared_key_xtn);
  StartConnect();
  Handshake();
  CheckConnected();
  SendReceive();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_psk, ssl_sig_none);
  // The PSK extension is present in CHOuter.
  ASSERT_TRUE(filter->captured());

  // But the PSK in CHOuter is completely different.
  // (Failure/collision chance means kPskId needs to be longish.)
  uint32_t v = 0;
  ASSERT_TRUE(filter->extension().Read(0, 2, &v));
  ASSERT_EQ(v, kPskId.size() + 2 + 4) << "check size of identities";
  ASSERT_TRUE(filter->extension().Read(2, 2, &v));
  ASSERT_EQ(v, kPskId.size()) << "check size of identity";
  bool different = false;
  for (size_t i = 0; i < kPskId.size(); ++i) {
    ASSERT_TRUE(filter->extension().Read(i + 4, 1, &v));
    different |= v != static_cast<uint8_t>(kPskId[i]);
  }
  ASSERT_TRUE(different);
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

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  auto server_hrr_ech_xtn = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
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
  Handshake();
  ASSERT_TRUE(server_hrr_ech_xtn->captured());
  EXPECT_EQ(1U, cb_called);
  CheckConnected();
  SendReceive();
}

TEST_F(TlsConnectStreamTls13Ech, EchGreaseSize) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));

  auto greased_ext = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  Connect();
  ASSERT_TRUE(greased_ext->captured());

  Reset();
  EnsureTlsSetup();

  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  ImportFixedEchKeypair(pub, priv);
  SetMutualEchConfigs(pub, priv);

  auto real_ext = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();

  ASSERT_TRUE(real_ext->captured());
  ASSERT_EQ(real_ext->extension().len(), greased_ext->extension().len());
}

TEST_F(TlsConnectStreamTls13Ech, EchGreaseClientDisable) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  EnsureTlsSetup();
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, echconfig, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE));

  auto c_filter_esni = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);

  Connect();
  ASSERT_TRUE(!c_filter_esni->captured());
}

TEST_F(TlsConnectStreamTls13Ech, EchHrrGreaseServerDisable) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_FALSE));
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  auto server_hrr_ech_xtn = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_FALSE));
  Handshake();
  ASSERT_TRUE(!server_hrr_ech_xtn->captured());
  EXPECT_EQ(1U, cb_called);
  CheckConnected();
  SendReceive();
}

TEST_F(TlsConnectStreamTls13Ech, EchGreaseSizePsk) {
  // Original connection without ECH
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  Connect();
  SendReceive();

  // Resumption with only GREASE
  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  ExpectResumption(RESUME_TICKET);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));

  auto greased_ext = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  Connect();
  SendReceive();
  ASSERT_TRUE(greased_ext->captured());

  // Finally, resume with ECH enabled
  // ECH state does not determine whether resumption succeeds
  // or is attempted, so this should work fine.
  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_BOTH);
  ExpectResumption(RESUME_TICKET, 2);

  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  ImportFixedEchKeypair(pub, priv);
  SetMutualEchConfigs(pub, priv);

  auto real_ext = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
  ASSERT_TRUE(real_ext->captured());

  ASSERT_EQ(real_ext->extension().len(), greased_ext->extension().len());
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

TEST_F(TlsConnectStreamTls13Ech, EchRejectMisizedEchXtn) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_TRUE));
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));
  auto server_hrr_ext_xtn_fake = MakeTlsFilter<TlsExtensionResizer>(
      server_, ssl_tls13_encrypted_client_hello_xtn, 34);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  // Process the hello retry.
  server_->ExpectReceiveAlert(kTlsAlertDecodeError, kTlsAlertFatal);
  client_->ExpectSendAlert(kTlsAlertDecodeError);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ECH_EXTENSION);
  server_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);
  EXPECT_EQ(1U, cb_called);
}

TEST_F(TlsConnectStreamTls13Ech, EchRejectDroppedEchXtn) {
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
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));
  auto server_hrr_ext_xtn_fake = MakeTlsFilter<TlsExtensionDropper>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  // Process the hello retry.
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  EXPECT_EQ(1U, cb_called);
}

// Generate an HRR on CHInner. Mangle the Hrr Xtn causing client to reject ECH
// which then causes a MAC mismatch.
TEST_F(TlsConnectStreamTls13Ech, EchRejectMangledHrrXtn) {
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

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));
  auto server_hrr_ech_xtn = MakeTlsFilter<TlsExtensionDamager>(
      server_, ssl_tls13_encrypted_client_hello_xtn, 4);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  EXPECT_EQ(1U, cb_called);
}

// First capture an ECH CH Xtn.
// Start new connection, inject ECH CH Xtn.
// Server will respond with ECH HRR Xtn.
// Check Client correctly panics.
TEST_F(TlsConnectStreamTls13Ech, EchClientRejectSpuriousHrrXtn) {
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
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  auto client_ech_xtn_capture = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  Connect();
  ASSERT_TRUE(client_ech_xtn_capture->captured());

  // Now configure client without ECH. Server with ECH.
  Reset();
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE));
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    echconfig.data(), echconfig.len()));
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  // Inject CH ECH Xtn into CH.
  DataBuffer buff = DataBuffer(client_ech_xtn_capture->extension());
  auto client_ech_xtn = MakeTlsFilter<TlsExtensionAppender>(
      client_, kTlsHandshakeClientHello, ssl_tls13_encrypted_client_hello_xtn,
      buff);

  // Connect and check we see the HRR extension and alert.
  auto server_hrr_ech_xtn = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  server_hrr_ech_xtn->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});

  ConnectExpectAlert(client_, kTlsAlertUnsupportedExtension);

  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
  server_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT);
  ASSERT_TRUE(server_hrr_ech_xtn->captured());
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

  // Damage the first byte of the ciphersuite (offset 1)
  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 1);

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
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_TRUE));
  auto server_hrr_ech_xtn = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_TRUE));
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  ExpectAlert(client_, kTlsAlertEchRequired);
  Handshake();
  ASSERT_TRUE(server_hrr_ech_xtn->captured());
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->Handshake();
  EXPECT_EQ(1U, cb_called);
}

// Server can't change its mind on ECH after HRR. We change the confirmation
// value and the server panics accordingly.
TEST_F(TlsConnectStreamTls13Ech, EchHrrServerYN) {
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

  auto server_hrr_ech_xtn = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
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
  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  auto server_random_damager = MakeTlsFilter<ServerHelloRandomChanger>(server_);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_SERVER_HELLO);
  ASSERT_TRUE(server_hrr_ech_xtn->captured());
  EXPECT_EQ(1U, cb_called);
}

// Client sends GREASE'd ECH Xtn, server reponds with HRR in GREASE mode
// Check HRR responses are present and differ.
TEST_F(TlsConnectStreamTls13Ech, EchHrrServerGreaseChanges) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer echconfig;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_TRUE));
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  auto server_hrr_ech_xtn_1 = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  ASSERT_TRUE(server_hrr_ech_xtn_1->captured());
  EXPECT_EQ(1U, cb_called);

  /* Run the connection again */
  Reset();
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(server_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_TRUE));
  cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  auto server_hrr_ech_xtn_2 = MakeTlsFilter<TlsExtensionCapture>(
      server_, ssl_tls13_encrypted_client_hello_xtn);
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  ASSERT_TRUE(server_hrr_ech_xtn_2->captured());
  EXPECT_EQ(1U, cb_called);

  ASSERT_TRUE(server_hrr_ech_xtn_1->extension().len() ==
              server_hrr_ech_xtn_2->extension().len());
  ASSERT_TRUE(memcmp(server_hrr_ech_xtn_1->extension().data(),
                     server_hrr_ech_xtn_2->extension().data(),
                     server_hrr_ech_xtn_1->extension().len()));
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

  /* Expect that retry configs containing unsupported mandatory extensions can
   * not be set and lead to SEC_ERROR_INVALID_ARGS. */
  EXPECT_EQ(SECFailure,
            SSL_SetClientEchConfigs(client_->ssl_fd(), crit_rec.data(),
                                    crit_rec.len()));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
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

// Altering the CH after the Ech Xtn should also cause a failure.
TEST_F(TlsConnectStreamTls13, EchOuterBindingAfterXtn) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);

  static const uint8_t supported_vers_13[] = {0x02, 0x03, 0x04};
  DataBuffer buf(supported_vers_13, sizeof(supported_vers_13));
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello, 5044,
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
                                     ssl_tls13_encrypted_client_hello_xtn, 1);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();

  Reset();
  EnsureTlsSetup();
  SetupEch(client_, server_);
  /* Make AEAD unknown */
  MakeTlsFilter<TlsExtensionDamager>(client_,
                                     ssl_tls13_encrypted_client_hello_xtn, 4);
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

/* ECH (configured) client connects to a 1.2 server, this MUST lead to an
 * 'ech_required' alert being sent by the client when handling the handshake
 * finished messages [draft-ietf-tls-esni-14, Section 6.1.6]. */
TEST_F(TlsConnectStreamTls13, EchToTls12Server) {
  EnsureTlsSetup();
  SetupEch(client_, server_);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  client_->ExpectEch(false);
  server_->ExpectEch(false);

  client_->ExpectSendAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  server_->ExpectReceiveAlert(kTlsAlertEchRequired, kTlsAlertFatal);
  ConnectExpectFailOneSide(TlsAgent::CLIENT);
  client_->CheckErrorCode(SSL_ERROR_ECH_RETRY_WITHOUT_ECH);

  /* Reset expectations for the TlsAgent deconstructor. */
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
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

  // The server will set the downgrade sentinel. The client needs
  // to ignore it for this test.
  client_->SetOption(SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_FALSE);

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

  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

static SECStatus NoopExtensionHandler(PRFileDesc* fd, SSLHandshakeType message,
                                      const PRUint8* data, unsigned int len,
                                      SSLAlertDescription* alert, void* arg) {
  return SECSuccess;
}

static PRBool EmptyExtensionWriter(PRFileDesc* fd, SSLHandshakeType message,
                                   PRUint8* data, unsigned int* len,
                                   unsigned int maxLen, void* arg) {
  return true;
}

static PRBool LargeExtensionWriter(PRFileDesc* fd, SSLHandshakeType message,
                                   PRUint8* data, unsigned int* len,
                                   unsigned int maxLen, void* arg) {
  unsigned int length = 1024;
  PR_ASSERT(length <= maxLen);
  memset(data, 0, length);
  *len = length;
  return true;
}

static PRBool OuterOnlyExtensionWriter(PRFileDesc* fd, SSLHandshakeType message,
                                       PRUint8* data, unsigned int* len,
                                       unsigned int maxLen, void* arg) {
  if (message == ssl_hs_ech_outer_client_hello) {
    return LargeExtensionWriter(fd, message, data, len, maxLen, arg);
  }
  return false;
}

static PRBool InnerOnlyExtensionWriter(PRFileDesc* fd, SSLHandshakeType message,
                                       PRUint8* data, unsigned int* len,
                                       unsigned int maxLen, void* arg) {
  if (message == ssl_hs_client_hello) {
    return LargeExtensionWriter(fd, message, data, len, maxLen, arg);
  }
  return false;
}

static PRBool InnerOuterDiffExtensionWriter(PRFileDesc* fd,
                                            SSLHandshakeType message,
                                            PRUint8* data, unsigned int* len,
                                            unsigned int maxLen, void* arg) {
  unsigned int length = 1024;
  PR_ASSERT(length <= maxLen);
  memset(data, (message == ssl_hs_client_hello) ? 1 : 0, length);
  *len = length;
  return true;
}

TEST_F(TlsConnectStreamTls13Ech, EchCustomExtensionWriter) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, EmptyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

TEST_F(TlsConnectStreamTls13Ech, EchCustomExtensionWriterOuterOnly) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, OuterOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

TEST_F(TlsConnectStreamTls13Ech, EchCustomExtensionWriterInnerOnly) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, InnerOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

// Write different values to inner and outer CH.
TEST_F(TlsConnectStreamTls13Ech, EchCustomExtensionWriterDifferent) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess,
            SSL_InstallExtensionHooks(client_->ssl_fd(), 62028,
                                      InnerOuterDiffExtensionWriter, nullptr,
                                      NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
  ASSERT_TRUE(filter->extension().len() > 1024);
}

// Test that basic compression works
TEST_F(TlsConnectStreamTls13Ech, EchCustomExtensionWriterCompressionBasic) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  // This will be compressed.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
  size_t echXtnLen = filter->extension().len();
  ASSERT_TRUE(echXtnLen > 0 && echXtnLen < 1024);
}

// Test that compression works when things change.
TEST_F(TlsConnectStreamTls13Ech,
       EchCustomExtensionWriterCompressSomeDifferent) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  // This will be compressed.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  // This can't be.
  EXPECT_EQ(SECSuccess,
            SSL_InstallExtensionHooks(client_->ssl_fd(), 62029,
                                      InnerOuterDiffExtensionWriter, nullptr,
                                      NoopExtensionHandler, nullptr));
  // This will be compressed.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62030, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
  auto echXtnLen = filter->extension().len();
  /* Exactly one custom xtn plus change */
  ASSERT_TRUE(echXtnLen > 1024 && echXtnLen < 2048);
}

// An outer-only extension stops compression.
TEST_F(TlsConnectStreamTls13Ech,
       EchCustomExtensionWriterCompressSomeOuterOnly) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  // This will be compressed.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  // This can't be as it appears in the outer only.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62029, OuterOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  // This will be compressed
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62030, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
  size_t echXtnLen = filter->extension().len();
  ASSERT_TRUE(echXtnLen > 0 && echXtnLen < 1024);
}

// An inner only extension does not stop compression.
TEST_F(TlsConnectStreamTls13Ech, EchCustomExtensionWriterCompressAllInnerOnly) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  // This will be compressed.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  // This can't be as it appears in the inner only.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62029, InnerOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  // This will be compressed.
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62030, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      client_, ssl_tls13_encrypted_client_hello_xtn);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
  size_t echXtnLen = filter->extension().len();
  ASSERT_TRUE(echXtnLen > 1024 && echXtnLen < 2048);
}

TEST_F(TlsConnectStreamTls13Ech, EchAcceptCustomXtn) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));

  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            server_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(server_, 62028);
  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

// Test that we reject Outer Xtn in SH if accepting ECH Inner
TEST_F(TlsConnectStreamTls13Ech, EchRejectOuterXtnOnInner) {
  EnsureTlsSetup();
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, OuterOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));

  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));

  // Put the same extension on the Server Hello
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            server_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(server_, 62028);
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  client_->ExpectSendAlert(kTlsAlertUnsupportedExtension);
  // The server will be expecting an alert encrypted under a different key.
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  ASSERT_TRUE(filter->captured());
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
}

// Test that we reject Inner Xtn in SH if accepting ECH Outer
TEST_F(TlsConnectStreamTls13Ech, EchRejectInnerXtnOnOuter) {
  EnsureTlsSetup();

  // Setup ECH only on the client
  SetupEch(client_, server_, HpkeDhKemX25519Sha256, false, true, false);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, InnerOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));

  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));

  // Put the same extension on the Server Hello
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            server_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(server_, 62028);
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  client_->ExpectSendAlert(kTlsAlertUnsupportedExtension);
  // The server will be expecting an alert encrypted under a different key.
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  ASSERT_TRUE(filter->captured());
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
}

// Test that we reject an Inner Xtn in SH, if accepting Ech Inner and
// we didn't advertise it on SH Outer.
TEST_F(TlsConnectStreamTls13Ech, EchRejectInnerXtnNotOnOuter) {
  EnsureTlsSetup();

  // Setup ECH only on the client
  SetupEch(client_, server_);

  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            client_->ssl_fd(), 62028, InnerOnlyExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));

  EXPECT_EQ(SECSuccess,
            SSL_CallExtensionWriterOnEchInner(client_->ssl_fd(), true));

  // Put the same extension on the Server Hello
  EXPECT_EQ(SECSuccess, SSL_InstallExtensionHooks(
                            server_->ssl_fd(), 62028, LargeExtensionWriter,
                            nullptr, NoopExtensionHandler, nullptr));
  auto filter = MakeTlsFilter<TlsExtensionCapture>(server_, 62028);
  client_->ExpectEch(false);
  server_->ExpectEch(false);
  client_->ExpectSendAlert(kTlsAlertUnsupportedExtension);
  // The server will be expecting an alert encrypted under a different key.
  server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
  ConnectExpectFail();
  ASSERT_TRUE(filter->captured());
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_EXTENSION);
}

// At draft-09: If a CH containing the ech_is_inner extension is received, the
// server acts as backend server in split-mode by responding with the ECH
// acceptance signal. The signal value itself depends on the handshake secret,
// which we've broken by appending ech_is_inner. For now, just check that the
// server negotiates ech_is_inner (which is what triggers sending the signal).
TEST_F(TlsConnectStreamTls13, EchBackendAcceptance) {
  DataBuffer ch_buf;
  static uint8_t inner_value[1] = {1};
  DataBuffer inner_buffer(inner_value, sizeof(inner_value));

  EnsureTlsSetup();
  StartConnect();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE));
  MakeTlsFilter<TlsExtensionAppender>(client_, kTlsHandshakeClientHello,
                                      ssl_tls13_encrypted_client_hello_xtn,
                                      inner_buffer);

  EXPECT_EQ(SECSuccess, SSL_EnableTls13BackendEch(server_->ssl_fd(), PR_TRUE));
  client_->Handshake();
  server_->Handshake();

  ExpectAlert(client_, kTlsAlertBadRecordMac);
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  EXPECT_EQ(PR_TRUE,
            SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                       ssl_tls13_encrypted_client_hello_xtn));
  server_->ExpectReceiveAlert(kTlsAlertCloseNotify, kTlsAlertWarning);
}

// A public_name that includes an IP address has to be rejected.
TEST_F(TlsConnectStreamTls13Ech, EchPublicNameIp) {
  static const std::vector<std::string> kIps = {
      "0.0.0.0",
      "1.1.1.1",
      "255.255.255.255",
      "255.255.65535",
      "255.16777215",
      "4294967295",
      "0377.0377.0377.0377",
      "0377.0377.0177777",
      "0377.077777777",
      "037777777777",
      "00377.00377.00377.00377",
      "00377.00377.00177777",
      "00377.0077777777",
      "0037777777777",
      "0xff.0xff.0xff.0xff",
      "0xff.0xff.0xffff",
      "0xff.0xffffff",
      "0xffffffff",
      "0XFF.0XFF.0XFF.0XFF",
      "0XFF.0XFF.0XFFFF",
      "0XFF.0XFFFFFF",
      "0XFFFFFFFF",
      "0x0ff.0x0ff.0x0ff.0x0ff",
      "0x0ff.0x0ff.0x0ffff",
      "0x0ff.0x0ffffff",
      "0x0ffffffff",
      "00000000000000000000000000000000000000000",
      "00000000000000000000000000000000000000001",
      "127.0.0.1",
      "127.0.1",
      "127.1",
      "2130706433",
      "017700000001",
  };
  ValidatePublicNames(kIps, SECFailure);
}

// These are nearly IP addresses.
TEST_F(TlsConnectStreamTls13Ech, EchPublicNameNotIp) {
  static const std::vector<std::string> kNotIps = {
      "0.0.0.0.0",
      "1.2.3.4.5",
      "999999999999999999999999999999999",
      "07777777777777777777777777777777777777777",
      "111111111100000000001111111111000000000011111111110000000000123",
      "256.255.255.255",
      "255.256.255.255",
      "255.255.256.255",
      "255.255.255.256",
      "255.255.65536",
      "255.16777216",
      "4294967296",
      "0400.0377.0377.0377",
      "0377.0400.0377.0377",
      "0377.0377.0400.0377",
      "0377.0377.0377.0400",
      "0377.0377.0200000",
      "0377.0100000000",
      "040000000000",
      "0x100.0xff.0xff.0xff",
      "0xff.0x100.0xff.0xff",
      "0xff.0xff.0x100.0xff",
      "0xff.0xff.0xff.0x100",
      "0xff.0xff.0x10000",
      "0xff.0x1000000",
      "0x100000000",
      "08",
      "09",
      "a",
      "0xg",
      "0XG",
      "0x",
      "0x.1.2.3",
      "test-name",
      "test-name.test",
      "TEST-NAME",
      "under_score",
      "_under_score",
      "under_score_",
  };
  ValidatePublicNames(kNotIps, SECSuccess);
}

TEST_F(TlsConnectStreamTls13Ech, EchPublicNameNotLdh) {
  static const std::vector<std::string> kNotLdh = {
      ".",
      "name.",
      ".name",
      "test..name",
      "1111111111000000000011111111110000000000111111111100000000001234",
      "-name",
      "name-",
      "test-.name",
      "!",
      u8"\u2077",
  };
  ValidatePublicNames(kNotLdh, SECFailure);
}

TEST_F(TlsConnectStreamTls13, EchClientHelloExtensionPermutation) {
  EnsureTlsSetup();
  ASSERT_TRUE(SSL_OptionSet(client_->ssl_fd(),
                            SSL_ENABLE_CH_EXTENSION_PERMUTATION,
                            PR_TRUE) == SECSuccess);
  SetupEch(client_, server_);

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
}

TEST_F(TlsConnectStreamTls13, EchGreaseClientHelloExtensionPermutation) {
  EnsureTlsSetup();
  ASSERT_TRUE(SSL_OptionSet(client_->ssl_fd(),
                            SSL_ENABLE_CH_EXTENSION_PERMUTATION,
                            PR_TRUE) == SECSuccess);
  ASSERT_TRUE(SSL_EnableTls13GreaseEch(client_->ssl_fd(), PR_FALSE) ==
              SECSuccess);
  Connect();
}

INSTANTIATE_TEST_SUITE_P(EchAgentTest, TlsAgentEchTest,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV13));

}  // namespace nss_test
