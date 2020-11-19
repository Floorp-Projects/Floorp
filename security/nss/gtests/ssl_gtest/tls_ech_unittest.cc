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
  void InstallEchConfig(const DataBuffer& record, PRErrorCode err = 0) {
    SECStatus rv =
        SSL_SetClientEchConfigs(agent_->ssl_fd(), record.data(), record.len());
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

  void SetMutualEchConfigs(ScopedSECKEYPublicKey& pub,
                           ScopedSECKEYPrivateKey& priv) {
    DataBuffer record;
    TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                          kPublicName, 100, record, pub, priv);
    ASSERT_EQ(SECSuccess,
              SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                      record.data(), record.len()));
    ASSERT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                  record.data(), record.len()));
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
    return;
  }

  // ECHConfig 2 cipher_suites are unsupported.
  const std::string mixed =
      "0086FE08003F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304"
      "444156E4E04D1BF0FFDA7783B6B457F75600200008000100030001000100640000FE0800"
      "3F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304444156E4E0"
      "4D1BF0FFDA7783B6B457F756002000080001FFFFFFFF000100640000";
  std::vector<uint8_t> config = hex_string_to_bytes(mixed);
  DataBuffer record(config.data(), config.size());

  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(record, 0);
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchConfigsSupportedNoYes) {
  if (variant_ == ssl_variant_datagram) {
    return;
  }

  // ECHConfig 1 cipher_suites are unsupported.
  const std::string mixed =
      "0086FE08003F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304"
      "444156E4E04D1BF0FFDA7783B6B457F756002000080001FFFFFFFF000100640000FE0800"
      "3F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304444156E4E0"
      "4D1BF0FFDA7783B6B457F75600200008000100030001000100640000";
  std::vector<uint8_t> config = hex_string_to_bytes(mixed);
  DataBuffer record(config.data(), config.size());

  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(record, 0);
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(filter->captured());
}

TEST_P(TlsAgentEchTest, EchConfigsSupportedNoNo) {
  if (variant_ == ssl_variant_datagram) {
    return;
  }

  // ECHConfig 1 and 2 cipher_suites are unsupported.
  const std::string unsupported =
      "0086FE08003F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304"
      "444156E4E04D1BF0FFDA7783B6B457F756002000080001FFFF0001FFFF00640000FE0800"
      "3F000B7075626C69632E6E616D6500203BB6D46C201B820F1AE4AFD4DEC304444156E4E0"
      "4D1BF0FFDA7783B6B457F75600200008FFFF0003FFFF000100640000";
  std::vector<uint8_t> config = hex_string_to_bytes(unsupported);
  DataBuffer record(config.data(), config.size());

  EnsureInit();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  InstallEchConfig(record, SEC_ERROR_INVALID_ARGS);
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
  DataBuffer record;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  record.Truncate(record.len() - 1);
  InstallEchConfig(record, SEC_ERROR_BAD_DATA);
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
  DataBuffer record;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  record.Write(record.len(), 1, 1);  // Append one byte
  InstallEchConfig(record, SEC_ERROR_BAD_DATA);
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
  DataBuffer record;
  static const uint8_t bad_version[] = {0xff, 0xff};
  DataBuffer bad_ver_buf(bad_version, sizeof(bad_version));
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  record.Splice(bad_ver_buf, 2, 2);
  InstallEchConfig(record, SEC_ERROR_INVALID_ARGS);
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
  DataBuffer record;
  // SSL_EncodeEchConfig encodes without validation.
  TlsConnectTestBase::GenerateEchConfig(static_cast<HpkeKemId>(0xff),
                                        kDefaultSuites, kPublicName, 100,
                                        record, pub, priv);
  InstallEchConfig(record, SEC_ERROR_INVALID_ARGS);
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
  DataBuffer record;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kBogusSuite,
                                        kPublicName, 100, record, pub, priv);
  InstallEchConfig(record, SEC_ERROR_INVALID_ARGS);
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(agent_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  auto filter = MakeTlsFilter<TlsExtensionCapture>(
      agent_, ssl_tls13_encrypted_client_hello_xtn);
  agent_->Handshake();
  ASSERT_FALSE(filter->captured());
}

TEST_F(TlsConnectStreamTls13, EchAcceptIgnoreSingleUnknownSuite) {
  EnsureTlsSetup();
  DataBuffer record;
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256,
                                        kUnknownFirstSuite, kPublicName, 100,
                                        record, pub, priv);
  ASSERT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                record.data(), record.len()));
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    record.data(), record.len()));

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
  DataBuffer record;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  InstallEchConfig(record, 0);

  EXPECT_EQ(SECFailure,
            SSL_GetEchRetryConfigs(agent_->ssl_fd(), &retry_configs));
  EXPECT_EQ(SSL_ERROR_HANDSHAKE_NOT_COMPLETED, PORT_GetError());
}

TEST_P(TlsAgentEchTest, NoSniSoNoEch) {
  EnsureInit();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer record;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  SSL_SetURL(agent_->ssl_fd(), "");
  InstallEchConfig(record, 0);
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
  DataBuffer record;
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
  DataBuffer record;
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);

  static const uint8_t duped_xtn[] = {0x00, 0x08, 0x00, 0x01, 0x00,
                                      0x00, 0x00, 0x01, 0x00, 0x00};
  DataBuffer buf(duped_xtn, sizeof(duped_xtn));
  record.Truncate(record.len() - 2);
  record.Append(buf);
  uint32_t len;
  ASSERT_TRUE(record.Read(0, 2, &len));
  len += buf.len() - 2;
  DataBuffer new_len;
  ASSERT_TRUE(new_len.Write(0, len, 2));
  record.Splice(new_len, 0, 2);
  new_len.Truncate(0);

  ASSERT_TRUE(record.Read(4, 2, &len));
  len += buf.len() - 2;
  ASSERT_TRUE(new_len.Write(0, len, 2));
  record.Splice(new_len, 4, 2);

  InstallEchConfig(record, SEC_ERROR_EXTENSION_VALUE_INVALID);
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
      "01000170030374d616d97efe591bf9bee4496bcc1118145b4dd02f7d1ff979fd0cf61749"
      "a91e0000061301130313020100014100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00204f346f86351b077492c83564c909d1aaab4f6f3ee2566af0e90a4684c793805d002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0800b30001000320a10698ccbd4bd86df91f617e58dd2ca96b8b"
      "a5f058dd5c5ab1ca9750ef9d28c70020924764b36fe5d4a985f9857ceb75edb10b5f4b5b"
      "f9d59290db70743e3c582163006acea5d7785cc506ecf5c859a9cad18f2b1df1a32231fe"
      "0330471ee0e88ece9047e6491a381bfabed58f7fc542f0ba78eb55030bcfe1d400f67275"
      "eac8619d1e4237e9d6176dd4eb54f3f25865686756f313a4ba47901c83e5ad5413609d39"
      "816346b940115fd68e534609";
  ReplayChWithMalformedInner(ch, kTlsAlertIllegalParameter,
                             SSL_ERROR_RX_MALFORMED_ECH_EXTENSION,
                             SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Drop supported_versions from CHInner, make sure we don't negotiate 1.2+ECH.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12Inner) {
  std::string ch =
      "0100017003034dd5bf4c12835e9be21f983953720e3595b3a8eeb4a44467678caceb7727"
      "3be90000061301130313020100014100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "0020af7b976cdf69ffcd494ca5a93ae3ecde692b09be518ee033aad908c45b82c368002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c0002400100150003000000fe0800ac0001000320a10698ccbd4bd86df91f61"
      "7e58dd2ca96b8ba5f058dd5c5ab1ca9750ef9d28c70020f5ece4c187b76f7e3d467c7506"
      "215e73c27c918cd863c0e80d76a7987ec274320063e037492868eff5296a22dc50885e9d"
      "f6964a5e26546f1bada043f8834988dfea5394b4c45a4d0b3afc52142d33f94161135a63"
      "ed3c1b63f60d8133fb1cff17e1f9ced6c871984e412ed8ddb0f487c4d09d7aea80488004"
      "c45a17cd3b5cdca316155fdb";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Use CHInner supported_versions to negotiate 1.2.
TEST_F(TlsConnectStreamTls13Ech, EchVersion12InnerSupportedVersions) {
  std::string ch =
      "010001700303845c298db4017d2ed2584284b90e4ecba57a63663560c57aa0b1ac51203d"
      "c8560000061301130313020100014100000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00203356719e88b539645438f645916aeeffe93c38803a59d6997938aa98eefbcf64002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c00024001fe0800b30001000320a10698ccbd4bd86df91f617e58dd2ca96b8b"
      "a5f058dd5c5ab1ca9750ef9d28c700208412c945c53624bcace5eda0dc1ad300a1620e86"
      "5a0f4a27755a3477b115b65b006abf1dfd77ddc1b80c5976732174a5fe7ebcf9ff1a548b"
      "097daa12a37f3e32a613a0798544ba1d96239431bc807ddd9055ac3fb3e32b2eb42cec30"
      "e915357418a953027d73020fd739287414205349eeff376dd464750ca70a965141a88800"
      "6a043fe1d6d882d9a2c2f6f3";
  ReplayChWithMalformedInner(ch, kTlsAlertProtocolVersion,
                             SSL_ERROR_UNSUPPORTED_VERSION,
                             SSL_ERROR_PROTOCOL_VERSION_ALERT);
}

// Replay a CH for which the ECH Inner lacks the required
// empty ECH extension.
TEST_F(TlsConnectStreamTls13Ech, EchInnerMissingEmptyEch) {
  std::string ch =
      "0100017103032bf866cbd6d4abdec8ce23107eaef9af51b644043953e3b70f2f28f1898e"
      "87880000061301130313020100014200000010000e00000b7075626c69632e6e616d65ff"
      "01000100000a00140012001d00170018001901000101010201030104003300260024001d"
      "00208f614d3017575332ca009a42d33bcaf876b4ba6d44b052e8019c31f6f1559e41002b"
      "0003020304000d0018001604030503060302030804080508060401050106010201002d00"
      "020101001c000240010015000100fe0800af0001000320a10698ccbd4bd86df91f617e58"
      "dd2ca96b8ba5f058dd5c5ab1ca9750ef9d28c70020da1d5d9f183a5d5e49892e38eaae5e"
      "9e3e6c5d404a5fdb672ca37f9cebabd57400660ea1d61917cc1049aab22506078ccecfc4"
      "16a364a1beaa8915b250bb86ac2c725698c3c641830c4aa4e8b7f50152b5732b29b1ac43"
      "45c97fc018855fd68e5600d0ef188e905b69997c3711b0ec0114a857177df728c7b84f52"
      "2923f932838f7f15bb22644fd4";
  ReplayChWithMalformedInner(ch, kTlsAlertDecodeError,
                             SSL_ERROR_MISSING_ECH_EXTENSION,
                             SSL_ERROR_DECODE_ERROR_ALERT);
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
  DataBuffer record;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    record.data(), record.len()));
  ASSERT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                record.data(), record.len()));
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
                                    record.data(), record.len()));
  client_->ExpectEch();
  server_->ExpectEch();
  client_->SetAuthCertificateCallback(AuthCompleteSuccess);
  Handshake();
  EXPECT_EQ(1U, cb_called);
  CheckConnected();
  SendReceive();
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
  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  EXPECT_EQ(1U, cb_called);
}

TEST_F(TlsConnectStreamTls13, EchHrrChangeCh2OfferingNY) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), RetryEchHello, &cb_called));

  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_FALSE));  // Don't GREASE
  // Start the handshake.
  client_->StartConnect();
  server_->StartConnect();
  client_->Handshake();
  server_->Handshake();
  MakeNewServer();
  EXPECT_EQ(SECSuccess, SSL_EnableTls13GreaseEch(client_->ssl_fd(),
                                                 PR_TRUE));  // Send GREASE
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
  DataBuffer record;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  ASSERT_EQ(SECSuccess,
            SSL_SetServerEchConfigs(server_->ssl_fd(), pub.get(), priv.get(),
                                    record.data(), record.len()));
  ASSERT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                record.data(), record.len()));
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
                                    record.data(), record.len()));
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
  DataBuffer record;
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

// Reject ECH on CH1 and (HRR) CH2. PSKs are no longer allowed
// in CHOuter, but can still make sure the handshake succeeds.
// (prompting ech_required at the completion).
TEST_F(TlsConnectStreamTls13, EchRejectWithHrrAndPsk) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer record;
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kDefaultSuites,
                                        kPublicName, 100, record, pub, priv);
  ASSERT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                record.data(), record.len()));

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
  DataBuffer record;
  DataBuffer crit_rec;
  DataBuffer len_buf;
  uint64_t tmp;

  static const uint8_t crit_extensions[] = {0x00, 0x04, 0xff, 0xff, 0x00, 0x00};
  static const uint8_t extensions[] = {0x00, 0x04, 0x7f, 0xff, 0x00, 0x00};
  DataBuffer crit_exts(crit_extensions, sizeof(crit_extensions));
  DataBuffer non_crit_exts(extensions, sizeof(extensions));

  TlsConnectTestBase::GenerateEchConfig(HpkeDhKemX25519Sha256, kSuiteChaCha,
                                        kPublicName, 100, record, pub, priv);
  record.Truncate(record.len() - 2);  // Eat the empty extensions.
  crit_rec.Assign(record);
  ASSERT_TRUE(crit_rec.Read(0, 2, &tmp));
  len_buf.Write(0, tmp + crit_exts.len() - 2, 2);  // two bytes of length
  crit_rec.Splice(len_buf, 0, 2);
  len_buf.Truncate(0);

  ASSERT_TRUE(crit_rec.Read(4, 2, &tmp));
  len_buf.Write(0, tmp + crit_exts.len() - 2, 2);  // two bytes of length
  crit_rec.Append(crit_exts);
  crit_rec.Splice(len_buf, 4, 2);
  len_buf.Truncate(0);

  ASSERT_TRUE(record.Read(0, 2, &tmp));
  len_buf.Write(0, tmp + non_crit_exts.len() - 2, 2);
  record.Append(non_crit_exts);
  record.Splice(len_buf, 0, 2);
  ASSERT_TRUE(record.Read(4, 2, &tmp));
  len_buf.Write(0, tmp + non_crit_exts.len() - 2, 2);
  record.Splice(len_buf, 4, 2);

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
  EXPECT_EQ(SECSuccess, SSL_SetClientEchConfigs(client_->ssl_fd(),
                                                record.data(), record.len()));
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

INSTANTIATE_TEST_CASE_P(EchAgentTest, TlsAgentEchTest,
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

INSTANTIATE_TEST_CASE_P(EchAgentTest, TlsAgentEchTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                           TlsConnectTestBase::kTlsV13));

#endif  // NSS_ENABLE_DRAFT_HPKE
}  // namespace nss_test
