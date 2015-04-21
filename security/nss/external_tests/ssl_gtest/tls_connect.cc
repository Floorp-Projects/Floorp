/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_connect.h"

#include <iostream>

#include "sslproto.h"
#include "gtest_utils.h"

extern std::string g_working_dir_path;

namespace nss_test {

static const std::string kTlsModesStreamArr[] = {"TLS"};
::testing::internal::ParamGenerator<std::string>
  TlsConnectTestBase::kTlsModesStream = ::testing::ValuesIn(kTlsModesStreamArr);
static const std::string kTlsModesAllArr[] = {"TLS", "DTLS"};
::testing::internal::ParamGenerator<std::string>
  TlsConnectTestBase::kTlsModesAll = ::testing::ValuesIn(kTlsModesAllArr);
static const uint16_t kTlsV10Arr[] = {SSL_LIBRARY_VERSION_TLS_1_0};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV10 = ::testing::ValuesIn(kTlsV10Arr);
static const uint16_t kTlsV11V12Arr[] = {SSL_LIBRARY_VERSION_TLS_1_1,
                                         SSL_LIBRARY_VERSION_TLS_1_2};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV11V12 = ::testing::ValuesIn(kTlsV11V12Arr);
// TODO: add TLS 1.3
static const uint16_t kTlsV12PlusArr[] = {SSL_LIBRARY_VERSION_TLS_1_2};
::testing::internal::ParamGenerator<uint16_t>
  TlsConnectTestBase::kTlsV12Plus = ::testing::ValuesIn(kTlsV12PlusArr);

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
  default:
    std::cerr << "Invalid version: " << version << std::endl;
    EXPECT_TRUE(false);
    return "";
  }
}

TlsConnectTestBase::TlsConnectTestBase(Mode mode, uint16_t version)
      : mode_(mode),
        client_(new TlsAgent("client", TlsAgent::CLIENT, mode_, ssl_kea_rsa)),
        server_(new TlsAgent("server", TlsAgent::SERVER, mode_, ssl_kea_rsa)),
        version_(version),
        session_ids_() {
  std::cerr << "Version: " << mode_ << " " << VersionString(version_) << std::endl;
}

TlsConnectTestBase::~TlsConnectTestBase() {
  delete client_;
  delete server_;
}

void TlsConnectTestBase::SetUp() {
  // Configure a fresh session cache.
  SSL_ConfigServerSessionIDCache(1024, 0, 0, g_working_dir_path.c_str());

  // Clear statistics.
  SSL3Statistics* stats = SSL_GetStatistics();
  memset(stats, 0, sizeof(*stats));

  Init();
}

void TlsConnectTestBase::TearDown() {
  client_ = nullptr;
  server_ = nullptr;

  SSL_ClearSessionCache();
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

void TlsConnectTestBase::Reset(const std::string& server_name, SSLKEAType kea) {
  delete client_;
  delete server_;

  client_ = new TlsAgent("client", TlsAgent::CLIENT, mode_, kea);
  server_ = new TlsAgent(server_name, TlsAgent::SERVER, mode_, kea);

  Init();
}

void TlsConnectTestBase::ResetRsa() {
  Reset("server", ssl_kea_rsa);
}

void TlsConnectTestBase::ResetEcdsa() {
  Reset("ecdsa", ssl_kea_ecdh);
  EnableSomeEcdheCiphers();
}

void TlsConnectTestBase::EnsureTlsSetup() {
  EXPECT_TRUE(client_->EnsureTlsSetup());
  EXPECT_TRUE(server_->EnsureTlsSetup());
}

void TlsConnectTestBase::Handshake() {
  server_->StartConnect();
  client_->StartConnect();
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT((client_->state() != TlsAgent::CONNECTING) &&
                   (server_->state() != TlsAgent::CONNECTING),
                   5000);

}

void TlsConnectTestBase::Connect() {
  Handshake();

  // Check the version is as expected
  EXPECT_EQ(client_->version(), server_->version());
  EXPECT_EQ(std::min(client_->max_version(),
                     server_->max_version()),
            client_->version());

  EXPECT_EQ(TlsAgent::CONNECTED, client_->state());
  EXPECT_EQ(TlsAgent::CONNECTED, server_->state());

  int16_t cipher_suite1, cipher_suite2;
  bool ret = client_->cipher_suite(&cipher_suite1);
  EXPECT_TRUE(ret);
  ret = server_->cipher_suite(&cipher_suite2);
  EXPECT_TRUE(ret);
  EXPECT_EQ(cipher_suite1, cipher_suite2);

  std::cerr << "Connected with version " << client_->version()
            << " cipher suite " << client_->cipher_suite_name()
            << std::endl;

  // Check and store session ids.
  std::vector<uint8_t> sid_c1 = client_->session_id();
  EXPECT_EQ(32U, sid_c1.size());
  std::vector<uint8_t> sid_s1 = server_->session_id();
  EXPECT_EQ(32U, sid_s1.size());
  EXPECT_EQ(sid_c1, sid_s1);
  session_ids_.push_back(sid_c1);
}

void TlsConnectTestBase::ConnectExpectFail() {
  Handshake();

  ASSERT_EQ(TlsAgent::ERROR, client_->state());
  ASSERT_EQ(TlsAgent::ERROR, server_->state());
}

void TlsConnectTestBase::EnableSomeEcdheCiphers() {
  client_->EnableSomeEcdheCiphers();
  server_->EnableSomeEcdheCiphers();
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

  if (resume_ct) {
    // Check that the last two session ids match.
    EXPECT_GE(2U, session_ids_.size());
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

void TlsConnectTestBase::CheckSrtp() {
  client_->CheckSrtp();
  server_->CheckSrtp();
}

TlsConnectGeneric::TlsConnectGeneric()
  : TlsConnectTestBase(TlsConnectTestBase::ToMode(std::get<0>(GetParam())),
                       std::get<1>(GetParam())) {}

} // namespace nss_test
