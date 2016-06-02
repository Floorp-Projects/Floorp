/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_connect.h"
extern "C" {
#include "libssl_internals.h"
}

#include <iostream>

#include "sslproto.h"
#include "gtest_utils.h"

extern std::string g_working_dir_path;

namespace nss_test {

static const std::string kTlsModesStreamArr[] = {"TLS"};
::testing::internal::ParamGenerator<std::string>
  TlsConnectTestBase::kTlsModesStream = ::testing::ValuesIn(kTlsModesStreamArr);
static const std::string kTlsModesDatagramArr[] = {"DTLS"};
::testing::internal::ParamGenerator<std::string>
TlsConnectTestBase::kTlsModesDatagram =
      ::testing::ValuesIn(kTlsModesDatagramArr);
static const std::string kTlsModesAllArr[] = {"TLS", "DTLS"};
::testing::internal::ParamGenerator<std::string>
  TlsConnectTestBase::kTlsModesAll = ::testing::ValuesIn(kTlsModesAllArr);

static const uint16_t kTlsV10Arr[] = {SSL_LIBRARY_VERSION_TLS_1_0};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV10 = ::testing::ValuesIn(kTlsV10Arr);
static const uint16_t kTlsV11Arr[] = {SSL_LIBRARY_VERSION_TLS_1_1};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV11 = ::testing::ValuesIn(kTlsV11Arr);
static const uint16_t kTlsV12Arr[] = {SSL_LIBRARY_VERSION_TLS_1_2};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV12 = ::testing::ValuesIn(kTlsV12Arr);
static const uint16_t kTlsV10V11Arr[] = {SSL_LIBRARY_VERSION_TLS_1_0,
                                         SSL_LIBRARY_VERSION_TLS_1_1};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV10V11 = ::testing::ValuesIn(kTlsV10V11Arr);
static const uint16_t kTlsV10ToV12Arr[] = {SSL_LIBRARY_VERSION_TLS_1_0,
                                           SSL_LIBRARY_VERSION_TLS_1_1,
                                           SSL_LIBRARY_VERSION_TLS_1_2};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV10ToV12 = ::testing::ValuesIn(kTlsV10ToV12Arr);
static const uint16_t kTlsV11V12Arr[] = {SSL_LIBRARY_VERSION_TLS_1_1,
                                         SSL_LIBRARY_VERSION_TLS_1_2};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV11V12 = ::testing::ValuesIn(kTlsV11V12Arr);

static const uint16_t kTlsV11PlusArr[] = {
#ifdef NSS_ENABLE_TLS_1_3
  SSL_LIBRARY_VERSION_TLS_1_3,
#endif
  SSL_LIBRARY_VERSION_TLS_1_2,
  SSL_LIBRARY_VERSION_TLS_1_1
};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV11Plus = ::testing::ValuesIn(kTlsV11PlusArr);
static const uint16_t kTlsV12PlusArr[] = {
#ifdef NSS_ENABLE_TLS_1_3
  SSL_LIBRARY_VERSION_TLS_1_3,
#endif
  SSL_LIBRARY_VERSION_TLS_1_2
};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV12Plus = ::testing::ValuesIn(kTlsV12PlusArr);
#ifdef NSS_ENABLE_TLS_1_3
static const uint16_t kTlsV13Arr[] = {
  SSL_LIBRARY_VERSION_TLS_1_3
};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV13 = ::testing::ValuesIn(kTlsV13Arr);
#endif
static const uint16_t kTlsVAllArr[] = {
#ifdef NSS_ENABLE_TLS_1_3
  SSL_LIBRARY_VERSION_TLS_1_3,
#endif
  SSL_LIBRARY_VERSION_TLS_1_2,
  SSL_LIBRARY_VERSION_TLS_1_1,
  SSL_LIBRARY_VERSION_TLS_1_0
};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsVAll = ::testing::ValuesIn(kTlsVAllArr);

static std::string VersionString(uint16_t version) {
  switch(version) {
  case 0:
    return "(no version)";
  case SSL_LIBRARY_VERSION_TLS_1_0:
    return "1.0";
  case SSL_LIBRARY_VERSION_TLS_1_1:
    return "1.1";
  case SSL_LIBRARY_VERSION_TLS_1_2:
    return "1.2";
  case SSL_LIBRARY_VERSION_TLS_1_3:
    return "1.3";
  default:
    std::cerr << "Invalid version: " << version << std::endl;
    EXPECT_TRUE(false);
    return "";
  }
}

TlsConnectTestBase::TlsConnectTestBase(Mode mode, uint16_t version)
      : mode_(mode),
        client_(new TlsAgent(TlsAgent::kClient, TlsAgent::CLIENT, mode_)),
        server_(new TlsAgent(TlsAgent::kServerRsa, TlsAgent::SERVER, mode_)),
        version_(version),
        expected_resumption_mode_(RESUME_NONE),
        session_ids_(),
        expect_extended_master_secret_(false) {
  std::string v;
  if (mode_ == DGRAM && version_ == SSL_LIBRARY_VERSION_TLS_1_1) {
    v = "1.0";
  } else {
    v = VersionString(version_);
  }
  std::cerr << "Version: " << mode_ << " " << v << std::endl;
}

TlsConnectTestBase::~TlsConnectTestBase() {
}

void TlsConnectTestBase::ClearStats() {
  // Clear statistics.
  SSL3Statistics* stats = SSL_GetStatistics();
  memset(stats, 0, sizeof(*stats));
}

void TlsConnectTestBase::ClearServerCache() {
  SSL_ShutdownServerSessionIDCache();
  SSLInt_ClearSessionTicketKey();
  SSL_ConfigServerSessionIDCache(1024, 0, 0, g_working_dir_path.c_str());
}

void TlsConnectTestBase::SetUp() {
  SSL_ConfigServerSessionIDCache(1024, 0, 0, g_working_dir_path.c_str());
  SSLInt_ClearSessionTicketKey();
  ClearStats();
  Init();
}

void TlsConnectTestBase::TearDown() {
  delete client_;
  delete server_;

  SSL_ClearSessionCache();
  SSLInt_ClearSessionTicketKey();
  SSL_ShutdownServerSessionIDCache();
}

void TlsConnectTestBase::Init() {
  EXPECT_TRUE(client_->Init());
  EXPECT_TRUE(server_->Init());

  client_->SetPeer(server_);
  server_->SetPeer(client_);

  if (version_) {
    client_->SetVersionRange(version_, version_);
    server_->SetVersionRange(version_, version_);
  }
}

void TlsConnectTestBase::Reset() {
  // Take a copy of the name because it's about to disappear.
  std::string name = server_->name();
  Reset(name);
}

void TlsConnectTestBase::Reset(const std::string& server_name) {
  delete client_;
  delete server_;

  client_ = new TlsAgent(TlsAgent::kClient, TlsAgent::CLIENT, mode_);
  server_ = new TlsAgent(server_name, TlsAgent::SERVER, mode_);

  Init();
}

void TlsConnectTestBase::ExpectResumption(SessionResumptionMode expected) {
  expected_resumption_mode_ = expected;
  if (expected != RESUME_NONE) {
    client_->ExpectResumption();
    server_->ExpectResumption();
  }
}

void TlsConnectTestBase::EnsureTlsSetup() {
  EXPECT_TRUE(client_->EnsureTlsSetup());
  EXPECT_TRUE(server_->EnsureTlsSetup());
}

void TlsConnectTestBase::Handshake() {
  EnsureTlsSetup();
  client_->SetServerKeyBits(server_->server_key_bits());
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT((client_->state() != TlsAgent::STATE_CONNECTING) &&
                   (server_->state() != TlsAgent::STATE_CONNECTING),
                   5000);
}

void TlsConnectTestBase::EnableExtendedMasterSecret() {
  client_->EnableExtendedMasterSecret();
  server_->EnableExtendedMasterSecret();
  ExpectExtendedMasterSecret(true);
}

void TlsConnectTestBase::Connect() {
  server_->StartConnect();
  client_->StartConnect();
  Handshake();
  CheckConnected();
}

void TlsConnectTestBase::ConnectWithCipherSuite(uint16_t cipher_suite)
{
  EnsureTlsSetup();
  client_->EnableSingleCipher(cipher_suite);

  Connect();
  SendReceive();

  // Check that we used the right cipher suite.
  uint16_t actual;
  EXPECT_TRUE(client_->cipher_suite(&actual));
  EXPECT_EQ(cipher_suite, actual);
  EXPECT_TRUE(server_->cipher_suite(&actual));
  EXPECT_EQ(cipher_suite, actual);
}


void TlsConnectTestBase::CheckConnected() {
  // Check the version is as expected
  EXPECT_EQ(client_->version(), server_->version());
  EXPECT_EQ(std::min(client_->max_version(),
                     server_->max_version()),
            client_->version());

  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());

  uint16_t cipher_suite1, cipher_suite2;
  bool ret = client_->cipher_suite(&cipher_suite1);
  EXPECT_TRUE(ret);
  ret = server_->cipher_suite(&cipher_suite2);
  EXPECT_TRUE(ret);
  EXPECT_EQ(cipher_suite1, cipher_suite2);

  std::cerr << "Connected with version " << client_->version()
            << " cipher suite " << client_->cipher_suite_name()
            << std::endl;

  if (client_->version() < SSL_LIBRARY_VERSION_TLS_1_3) {
    // Check and store session ids.
    std::vector<uint8_t> sid_c1 = client_->session_id();
    EXPECT_EQ(32U, sid_c1.size());
    std::vector<uint8_t> sid_s1 = server_->session_id();
    EXPECT_EQ(32U, sid_s1.size());
    EXPECT_EQ(sid_c1, sid_s1);
    session_ids_.push_back(sid_c1);
  }

  CheckExtendedMasterSecret();

  CheckResumption(expected_resumption_mode_);
}

void TlsConnectTestBase::CheckKeys(SSLKEAType keaType,
                                   SSLAuthType authType) const {
  client_->CheckKEAType(keaType);
  server_->CheckKEAType(keaType);
  client_->CheckAuthType(authType);
  server_->CheckAuthType(authType);
}

void TlsConnectTestBase::ConnectExpectFail() {
  server_->StartConnect();
  client_->StartConnect();
  Handshake();
  ASSERT_EQ(TlsAgent::STATE_ERROR, client_->state());
  ASSERT_EQ(TlsAgent::STATE_ERROR, server_->state());
}

void TlsConnectTestBase::SetExpectedVersion(uint16_t version) {
  client_->SetExpectedVersion(version);
  server_->SetExpectedVersion(version);
}

void TlsConnectTestBase::DisableDheCiphers() {
  client_->DisableCiphersByKeyExchange(ssl_kea_dh);
  server_->DisableCiphersByKeyExchange(ssl_kea_dh);
}

void TlsConnectTestBase::DisableEcdheCiphers() {
  client_->DisableCiphersByKeyExchange(ssl_kea_ecdh);
  server_->DisableCiphersByKeyExchange(ssl_kea_ecdh);
}

void TlsConnectTestBase::DisableDheAndEcdheCiphers() {
  DisableDheCiphers();
  DisableEcdheCiphers();
}

void TlsConnectTestBase::EnableSomeEcdhCiphers() {
  client_->EnableCiphersByAuthType(ssl_auth_ecdh_rsa);
  client_->EnableCiphersByAuthType(ssl_auth_ecdh_ecdsa);
  server_->EnableCiphersByAuthType(ssl_auth_ecdh_rsa);
  server_->EnableCiphersByAuthType(ssl_auth_ecdh_ecdsa);
}

void TlsConnectTestBase::ConfigureSessionCache(SessionResumptionMode client,
                                               SessionResumptionMode server) {
  client_->ConfigureSessionCache(client);
  server_->ConfigureSessionCache(server);
}

void TlsConnectTestBase::CheckResumption(SessionResumptionMode expected) {
  EXPECT_NE(RESUME_BOTH, expected);

  int resume_ct = expected ? 1 : 0;
  int stateless_ct = (expected & RESUME_TICKET) ? 1 : 0;

  SSL3Statistics* stats = SSL_GetStatistics();
  EXPECT_EQ(resume_ct, stats->hch_sid_cache_hits);
  EXPECT_EQ(resume_ct, stats->hsh_sid_cache_hits);

  EXPECT_EQ(stateless_ct, stats->hch_sid_stateless_resumes);
  EXPECT_EQ(stateless_ct, stats->hsh_sid_stateless_resumes);

  if (resume_ct &&
      client_->version() < SSL_LIBRARY_VERSION_TLS_1_3) {
    // Check that the last two session ids match.
    // TLS 1.3 doesn't do session id-based resumption. It's all
    // tickets.
    EXPECT_EQ(2U, session_ids_.size());
    EXPECT_EQ(session_ids_[session_ids_.size()-1],
              session_ids_[session_ids_.size()-2]);
  }
}

void TlsConnectTestBase::EnableAlpn() {
  // A simple value of "a", "b".  Note that the preferred value of "a" is placed
  // at the end, because the NSS API follows the now defunct NPN specification,
  // which places the preferred (and default) entry at the end of the list.
  // NSS will move this final entry to the front when used with ALPN.
  static const uint8_t val[] = { 0x01, 0x62, 0x01, 0x61 };
  client_->EnableAlpn(val, sizeof(val));
  server_->EnableAlpn(val, sizeof(val));
}

void TlsConnectTestBase::EnableSrtp() {
  client_->EnableSrtp();
  server_->EnableSrtp();
}

void TlsConnectTestBase::CheckSrtp() const {
  client_->CheckSrtp();
  server_->CheckSrtp();
}

void TlsConnectTestBase::SendReceive() {
  client_->SendData(50);
  server_->SendData(50);
  Receive(50);
}

void TlsConnectTestBase::Receive(size_t amount) {
  WAIT_(client_->received_bytes() == amount &&
        server_->received_bytes() == amount, 2000);
  ASSERT_EQ(amount, client_->received_bytes());
  ASSERT_EQ(amount, server_->received_bytes());
}


void TlsConnectTestBase::ExpectExtendedMasterSecret(bool expected) {
  expect_extended_master_secret_ = expected;
}

void TlsConnectTestBase::CheckExtendedMasterSecret() {
  client_->CheckExtendedMasterSecret(expect_extended_master_secret_);
  server_->CheckExtendedMasterSecret(expect_extended_master_secret_);
}

TlsConnectGeneric::TlsConnectGeneric()
  : TlsConnectTestBase(TlsConnectTestBase::ToMode(std::get<0>(GetParam())),
                       std::get<1>(GetParam())) {}

TlsConnectPre12::TlsConnectPre12()
  : TlsConnectTestBase(TlsConnectTestBase::ToMode(std::get<0>(GetParam())),
                       std::get<1>(GetParam())) {}

TlsConnectTls12::TlsConnectTls12()
  : TlsConnectTestBase(TlsConnectTestBase::ToMode(GetParam()),
                       SSL_LIBRARY_VERSION_TLS_1_2) {}

} // namespace nss_test
