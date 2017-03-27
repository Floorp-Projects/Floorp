/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <functional>
#include <memory>
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

const uint8_t kBogusClientKeyExchange[] = {
    0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

TEST_P(TlsConnectGenericPre13, ConnectStaticRSA) {
  EnableOnlyStaticRsaCiphers();
  Connect();
  CheckKeys(ssl_kea_rsa, ssl_grp_none, ssl_auth_rsa_decrypt, ssl_sig_none);
}

// Test that a totally bogus EPMS is handled correctly.
// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectStaticRSABogusCKE) {
  EnableOnlyStaticRsaCiphers();
  auto i1 = std::make_shared<TlsInspectorReplaceHandshakeMessage>(
      kTlsHandshakeClientKeyExchange,
      DataBuffer(kBogusClientKeyExchange, sizeof(kBogusClientKeyExchange)));
  client_->SetPacketFilter(i1);
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

// Test that a PMS with a bogus version number is handled correctly.
// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectStaticRSABogusPMSVersionDetect) {
  EnableOnlyStaticRsaCiphers();
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionChanger>(server_));
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

// Test that a PMS with a bogus version number is ignored when
// rollback detection is disabled. This is a positive control for
// ConnectStaticRSABogusPMSVersionDetect.
TEST_P(TlsConnectGenericPre13, ConnectStaticRSABogusPMSVersionIgnore) {
  EnableOnlyStaticRsaCiphers();
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionChanger>(server_));
  server_->DisableRollbackDetection();
  Connect();
}

// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectExtendedMasterSecretStaticRSABogusCKE) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  auto inspect = std::make_shared<TlsInspectorReplaceHandshakeMessage>(
      kTlsHandshakeClientKeyExchange,
      DataBuffer(kBogusClientKeyExchange, sizeof(kBogusClientKeyExchange)));
  client_->SetPacketFilter(inspect);
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13,
       ConnectExtendedMasterSecretStaticRSABogusPMSVersionDetect) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionChanger>(server_));
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

TEST_P(TlsConnectStreamPre13,
       ConnectExtendedMasterSecretStaticRSABogusPMSVersionIgnore) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  client_->SetPacketFilter(
      std::make_shared<TlsInspectorClientHelloVersionChanger>(server_));
  server_->DisableRollbackDetection();
  Connect();
}

}  // namespace nspr_test
