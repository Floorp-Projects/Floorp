/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef NSS_ALLOW_SSLKEYLOGFILE

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

static const std::string keylog_file_path = "keylog.txt";
static const std::string keylog_env = "SSLKEYLOGFILE=" + keylog_file_path;

class KeyLogFileTest : public TlsConnectGeneric {
 public:
  void SetUp() override {
    TlsConnectGeneric::SetUp();
    // Remove previous results (if any).
    (void)remove(keylog_file_path.c_str());
    PR_SetEnv(keylog_env.c_str());
  }

  void CheckKeyLog() {
    std::ifstream f(keylog_file_path);
    std::map<std::string, size_t> labels;
    std::set<std::string> client_randoms;
    for (std::string line; std::getline(f, line);) {
      if (line[0] == '#') {
        continue;
      }

      std::istringstream iss(line);
      std::string label, client_random, secret;
      iss >> label >> client_random >> secret;

      ASSERT_EQ(64U, client_random.size());
      client_randoms.insert(client_random);
      labels[label]++;
    }

    if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
      ASSERT_EQ(1U, client_randoms.size());
    } else {
      /* two handshakes for 0-RTT */
      ASSERT_EQ(2U, client_randoms.size());
    }

    // Every entry occurs twice (one log from server, one from client).
    if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
      ASSERT_EQ(2U, labels["CLIENT_RANDOM"]);
    } else {
      ASSERT_EQ(2U, labels["CLIENT_EARLY_TRAFFIC_SECRET"]);
      ASSERT_EQ(2U, labels["EARLY_EXPORTER_SECRET"]);
      ASSERT_EQ(4U, labels["CLIENT_HANDSHAKE_TRAFFIC_SECRET"]);
      ASSERT_EQ(4U, labels["SERVER_HANDSHAKE_TRAFFIC_SECRET"]);
      ASSERT_EQ(4U, labels["CLIENT_TRAFFIC_SECRET_0"]);
      ASSERT_EQ(4U, labels["SERVER_TRAFFIC_SECRET_0"]);
      ASSERT_EQ(4U, labels["EXPORTER_SECRET"]);
    }
  }

  void ConnectAndCheck() {
    // This is a child process, ensure that error messages immediately
    // propagate or else it will not be visible.
    ::testing::GTEST_FLAG(throw_on_failure) = true;

    if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
      SetupForZeroRtt();
      client_->Set0RttEnabled(true);
      server_->Set0RttEnabled(true);
      ExpectResumption(RESUME_TICKET);
      ZeroRttSendReceive(true, true);
      Handshake();
      ExpectEarlyDataAccepted(true);
      CheckConnected();
      SendReceive();
    } else {
      Connect();
    }
    CheckKeyLog();
    _exit(0);
  }
};

// Tests are run in a separate process to ensure that NSS is not initialized yet
// and can process the SSLKEYLOGFILE environment variable.

TEST_P(KeyLogFileTest, KeyLogFile) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";

  ASSERT_EXIT(ConnectAndCheck(), ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_CASE_P(
    KeyLogFileDTLS12, KeyLogFileTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsDatagram,
                       TlsConnectTestBase::kTlsV11V12));
INSTANTIATE_TEST_CASE_P(
    KeyLogFileTLS12, KeyLogFileTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsV10ToV12));
#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(
    KeyLogFileTLS13, KeyLogFileTest,
    ::testing::Combine(TlsConnectTestBase::kTlsVariantsStream,
                       TlsConnectTestBase::kTlsV13));
#endif

}  // namespace nss_test

#endif  // NSS_ALLOW_SSLKEYLOGFILE
