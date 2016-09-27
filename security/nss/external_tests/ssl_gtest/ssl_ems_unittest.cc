/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecret) {
  EnableExtendedMasterSecret();
  Connect();
  Reset();
  ExpectResumption(RESUME_SESSIONID);
  EnableExtendedMasterSecret();
  Connect();
}

TEST_P(TlsConnectTls12, ConnectExtendedMasterSecretSha384) {
  EnableExtendedMasterSecret();
  server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
  ConnectWithCipherSuite(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretStaticRSA) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretECDHE) {
  EnableExtendedMasterSecret();
  Connect();

  Reset();
  EnableExtendedMasterSecret();
  ExpectResumption(RESUME_SESSIONID);
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretTicket) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  EnableExtendedMasterSecret();
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);

  EnableExtendedMasterSecret();
  ExpectResumption(RESUME_TICKET);
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretClientOnly) {
  client_->EnableExtendedMasterSecret();
  ExpectExtendedMasterSecret(false);
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretServerOnly) {
  server_->EnableExtendedMasterSecret();
  ExpectExtendedMasterSecret(false);
  Connect();
}

TEST_P(TlsConnectGenericPre13, ConnectExtendedMasterSecretResumeWithout) {
  EnableExtendedMasterSecret();
  Connect();

  Reset();
  server_->EnableExtendedMasterSecret();
  auto alert_recorder = new TlsAlertRecorder();
  server_->SetPacketFilter(alert_recorder);
  ConnectExpectFail();
  EXPECT_EQ(kTlsAlertFatal, alert_recorder->level());
  EXPECT_EQ(kTlsAlertHandshakeFailure, alert_recorder->description());
}

TEST_P(TlsConnectGenericPre13, ConnectNormalResumeWithExtendedMasterSecret) {
  ConfigureSessionCache(RESUME_SESSIONID, RESUME_SESSIONID);
  ExpectExtendedMasterSecret(false);
  Connect();

  Reset();
  EnableExtendedMasterSecret();
  ExpectResumption(RESUME_NONE);
  Connect();
}

}  // namespace nss_test
