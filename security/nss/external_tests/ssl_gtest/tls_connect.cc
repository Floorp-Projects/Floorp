/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_connect.h"

#include <iostream>

#include "gtest_utils.h"

extern std::string g_working_dir_path;

namespace nss_test {

TlsConnectTestBase::TlsConnectTestBase(Mode mode)
      : mode_(mode),
        client_(new TlsAgent("client", TlsAgent::CLIENT, mode_)),
        server_(new TlsAgent("server", TlsAgent::SERVER, mode_)) {}

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
  ASSERT_TRUE(client_->Init());
  ASSERT_TRUE(server_->Init());

  client_->SetPeer(server_);
  server_->SetPeer(client_);
}

void TlsConnectTestBase::Reset() {
  delete client_;
  delete server_;

  client_ = new TlsAgent("client", TlsAgent::CLIENT, mode_);
  server_ = new TlsAgent("server", TlsAgent::SERVER, mode_);

  Init();
}

void TlsConnectTestBase::EnsureTlsSetup() {
  ASSERT_TRUE(client_->EnsureTlsSetup());
  ASSERT_TRUE(server_->EnsureTlsSetup());
}

void TlsConnectTestBase::Handshake() {
  server_->StartConnect();
  client_->StartConnect();
  client_->Handshake();
  server_->Handshake();

  ASSERT_TRUE_WAIT(client_->state() != TlsAgent::CONNECTING &&
                   server_->state() != TlsAgent::CONNECTING,
                   5000);
}

void TlsConnectTestBase::Connect() {
  Handshake();

  ASSERT_EQ(TlsAgent::CONNECTED, client_->state());
  ASSERT_EQ(TlsAgent::CONNECTED, server_->state());

  int16_t cipher_suite1, cipher_suite2;
  bool ret = client_->cipher_suite(&cipher_suite1);
  ASSERT_TRUE(ret);
  ret = server_->cipher_suite(&cipher_suite2);
  ASSERT_TRUE(ret);
  ASSERT_EQ(cipher_suite1, cipher_suite2);

  std::cerr << "Connected with cipher suite " << client_->cipher_suite_name()
            << std::endl;

  // Check and store session ids.
  std::vector<uint8_t> sid_c1 = client_->session_id();
  ASSERT_EQ(32, sid_c1.size());
  std::vector<uint8_t> sid_s1 = server_->session_id();
  ASSERT_EQ(32, sid_s1.size());
  ASSERT_EQ(sid_c1, sid_s1);
  session_ids_.push_back(sid_c1);
}

void TlsConnectTestBase::ConnectExpectFail() {
  Handshake();

  ASSERT_EQ(TlsAgent::ERROR, client_->state());
  ASSERT_EQ(TlsAgent::ERROR, server_->state());
}

void TlsConnectTestBase::EnableSomeECDHECiphers() {
  client_->EnableSomeECDHECiphers();
  server_->EnableSomeECDHECiphers();
}


void TlsConnectTestBase::ConfigureSessionCache(SessionResumptionMode client,
                                               SessionResumptionMode server) {
  client_->ConfigureSessionCache(client);
  server_->ConfigureSessionCache(server);
}

void TlsConnectTestBase::CheckResumption(SessionResumptionMode expected) {
  ASSERT_NE(RESUME_BOTH, expected);

  int resume_ct = expected ? 1 : 0;
  int stateless_ct = (expected & RESUME_TICKET) ? 1 : 0;

  SSL3Statistics* stats = SSL_GetStatistics();
  ASSERT_EQ(resume_ct, stats->hch_sid_cache_hits);
  ASSERT_EQ(resume_ct, stats->hsh_sid_cache_hits);

  ASSERT_EQ(stateless_ct, stats->hch_sid_stateless_resumes);
  ASSERT_EQ(stateless_ct, stats->hsh_sid_stateless_resumes);

  if (resume_ct) {
    // Check that the last two session ids match.
    ASSERT_GE(2, session_ids_.size());
    ASSERT_EQ(session_ids_[session_ids_.size()-1],
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
    : TlsConnectTestBase((GetParam() == "TLS") ? STREAM : DGRAM) {
  std::cerr << "Variant: " << GetParam() << std::endl;
}

} // namespace nss_test
