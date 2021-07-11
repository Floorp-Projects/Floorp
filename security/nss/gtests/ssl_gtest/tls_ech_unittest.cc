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
      "0088FE0A004000002000203BB6D46C201B820F1AE4AFD4DEC304444156E4E04D1BF0FFDA"
      "7783B6B457F756000800010003000100010064000B7075626C69632E6E616D650000FE0A"
      "004000002000203BB6D46C201B820F1AE4AFD4DEC304444156E4E04D1BF0FFDA7783B6B4"
      "57F75600080001FFFFFFFF00010064000B7075626C69632E6E616D650000";
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
      "0088FE0A004000002000203BB6D46C201B820F1AE4AFD4DEC304444156E4E04D1BF0FFDA"
      "7783B6B457F75600080001FFFFFFFF00010064000B7075626C69632E6E616D650000FE0A"
      "004000002000203BB6D46C201B820F1AE4AFD4DEC304444156E4E04D1BF0FFDA7783B6B4"
      "57F756000800010003000100010064000B7075626C69632E6E616D650000";
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
      "0088FE0A004000002000203BB6D46C201B820F1AE4AFD4DEC304444156E4E04D1BF0FFDA"
      "7783B6B457F75600080001FFFF0001FFFF0064000B7075626C69632E6E616D650000FE0A"
      "004000002000203BB6D46C201B820F1AE4AFD4DEC304444156E4E04D1BF0FFDA7783B6B4"
      "57F7560008FFFF0003FFFF00010064000B7075626C69632E6E616D650000";
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
      "01000200030341a6813ccf3eefc2deb9c78f7627715ae343f5236e7224f454c723c93e0b"
      "d875000006130113031302010001d100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00200573a70286658ad4bc166d8f5f237f035714ba5ae4e838c077677ccb6d619813002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001001500aa0000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000fe0a0095000100034d00209c68779a77e4bac0e9f39c2974b1900de044ae1510bf"
      "d34fb5120a2a9d039607006c76c4571099733157eb8614ef2ad6049372e9fdf740f8ad4f"
      "d24723702c9104a38ecc366eea78b0285422b3f119fc057e2282433a74d8c56b2135c785"
      "bd5d01f89b2dbb42aa9a609eb1c6dd89252fa04cf8fbc4097e9c85da1e3eeebc188bbe16"
      "db1166f6df1a0c7c6e0dce71";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Drop supported_versions from CHInner, make sure we don't negotiate 1.2+ECH.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12Inner) {
  // Construct this by removing ssl_tls13_supported_versions_xtn entirely.
  std::string ch =
      "010002000303baf30ea25e5056b659a4d55233922c4ee261a04e6d84c8200713edca2f55"
      "d434000006130113031302010001d100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "002081908a3cf3ed9ebf6d6b1f7082d77bb3bf8ff309c3c1255421720c4172548762002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001001500b30000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000fe0a008c000100034d0020305bc263a664387b90a6975b2a"
      "3aa1e4358e80a8ca0841237035d2475628582d006352a2b49912a61543dfa045c1429582"
      "540c8c7019968867fde698eb37667f9aa9c23d02757492a4580fb027bbe4ba7615eea118"
      "ad3bf7f02a88f8372cfa01888e7be0c55616f846e902bbdfc7edf56994d6398f5a965d9e"
      "c4b1bc7afc580b28b0ac91d8";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Use CHInner supported_versions to negotiate 1.2.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12InnerSupportedVersions) {
  // Construct this by changing ssl_tls13_supported_versions_xtn to write
  // TLS 1.2 instead of TLS 1.3.
  std::string ch =
      "0100020003036c4a7f6f6b5479a5c1f769c7b04c082ba40b514522d193df855df8bea933"
      "b565000006130113031302010001d100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020ee721b8fe89260f8987d0d21b628db136c6155793fa63f4f546b244ee5357761002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001001500ac0000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000fe0a0093000100034d00205de27fe39481bfb370ee8441f12e28296bc5c8fe"
      "b6a6198ddc6ab03b2d024638006a9e9f57c3f39c0ad1c3427549a77f301d01d718e09da4"
      "5497df178c95fd598bf0c9098d68dfba80a05eeeabc84dc0bb3225cee4a74688d520c632"
      "73612f98be847dea4f040a8d9b2b92bb4a44273d0cafafbfe1ee4ed69448bc243b4359c6"
      "e1eb3971e125fbfb016245fa";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Replay a CH for which CHInner lacks the required ech_is_inner extension.
TEST_F(TlsConnectStreamTls13Ech, EchInnerMissing) {
  // Construct by omitting ssl_tls13_ech_is_inner_xtn.
  std::string ch =
      "010002000303912d293136b843248ffeecdde6ef0d5bc5d0adb4d356b985c2fcec8fe2b0"
      "8f5d000006130113031302010001d100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00209222e6b0c672fd1e564fbca5889155e9126e3b006a8ff77ff61898dd56a08429002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001001500b00000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000fe0a008f000100034d0020e1bc83c066a251621c4b055779789a2c"
      "6ac4b9a3850366b2ea0d32a8e041181c0066a4e9cc6912b8bc6c1b54a2c6c40428ab01a3"
      "0e4621ec65320df2beff846a606618429c108fdfe24a6fad5053f53fb36082bf7d80d6f4"
      "73131a3d6c04e182595606070ac4e0be078ada56456c02d37a2fee7cb17accd273cbd810"
      "30ee0dbe139e51ba1d2a471f";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_MISSING_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Replay a CH for which CHInner contains both an ECH and ech_is_inner
// extension.
TEST_F(TlsConnectStreamTls13Ech, InnerWithEchAndEchIsInner) {
  // Construct by appending an empty ssl_tls13_encrypted_client_hello_xtn to
  // CHInner.
  std::string ch =
      "010002000303b690bc4090ecfd7ad167de639b1d1ea7682588ffefae1164179d370f6cd3"
      "0864000006130113031302010001d100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00200c3c15b0e9884d5f593634a168b70a62ae18c8d69a68f8e29c826fbabcd99b22002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001001500a80000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "00fe0a0097000100034d0020d46cc9042eff6efee046a5ff653d1b6a60cfd5786afef5ce"
      "43300bc515ef5f09006ea6bf626854596df74b2d8f81a479a6d2fef13295a81e0571008a"
      "12fc92f82170fdb899cd22ebadc33a3147c2801619f7621ffe8684c96918443e3fbe9b17"
      "34fbf678ba0b2abad7ab6b018bccc1034b9537a5d399fdb9a5ccb92360bde4a94a2f2509"
      "0e7313dd9254eae3603e1fee";
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
      "0080FE0A003C000020002011111111111111111111111111111111111111111111111111"
      "111111111111110004000100010064000B7075626C69632E6E616D650000FE0A003C0000"
      "2000208756E2580C07C1D2FFCB662F5FADC6D6FF13DA85ABD7ADFECF984AAA102C126900"
      "04000100010064000B7075626C69632E6E616D650000";
  const std::string second_config_str =
      "0040FE0A003C00002000208756E2580C07C1D2FFCB662F5FADC6D6FF13DA85ABD7ADFECF"
      "984AAA102C12690004000100010064000B7075626C69632E6E616D650000";
  std::vector<uint8_t> two_configs = hex_string_to_bytes(two_configs_str);
  std::vector<uint8_t> second_config = hex_string_to_bytes(second_config_str);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    two_configs.data(), two_configs.size()));
  ASSERT_EQ(SECSuccess,
            SSL_SetClientEchConfigs(client_->ssl_fd(), second_config.data(),
                                    second_config.size()));

  client_->ExpectEch();
  server_->ExpectEch();
  Connect();
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

INSTANTIATE_TEST_SUITE_P(EchAgentTest, TlsAgentEchTest,
                         ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                            TlsConnectTestBase::kTlsV13));

}  // namespace nss_test
