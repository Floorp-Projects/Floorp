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
#include "rsa8193.h"

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
  MakeTlsFilter<TlsInspectorReplaceHandshakeMessage>(
      client_, kTlsHandshakeClientKeyExchange,
      DataBuffer(kBogusClientKeyExchange, sizeof(kBogusClientKeyExchange)));
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

// Test that a PMS with a bogus version number is handled correctly.
// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectStaticRSABogusPMSVersionDetect) {
  EnableOnlyStaticRsaCiphers();
  MakeTlsFilter<TlsClientHelloVersionChanger>(client_, server_);
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

// Test that a PMS with a bogus version number is ignored when
// rollback detection is disabled. This is a positive control for
// ConnectStaticRSABogusPMSVersionDetect.
TEST_P(TlsConnectGenericPre13, ConnectStaticRSABogusPMSVersionIgnore) {
  EnableOnlyStaticRsaCiphers();
  MakeTlsFilter<TlsClientHelloVersionChanger>(client_, server_);
  server_->SetOption(SSL_ROLLBACK_DETECTION, PR_FALSE);
  Connect();
}

// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13, ConnectExtendedMasterSecretStaticRSABogusCKE) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  MakeTlsFilter<TlsInspectorReplaceHandshakeMessage>(
      client_, kTlsHandshakeClientKeyExchange,
      DataBuffer(kBogusClientKeyExchange, sizeof(kBogusClientKeyExchange)));
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

// This test is stream so we can catch the bad_record_mac alert.
TEST_P(TlsConnectStreamPre13,
       ConnectExtendedMasterSecretStaticRSABogusPMSVersionDetect) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  MakeTlsFilter<TlsClientHelloVersionChanger>(client_, server_);
  ConnectExpectAlert(server_, kTlsAlertBadRecordMac);
}

TEST_P(TlsConnectStreamPre13,
       ConnectExtendedMasterSecretStaticRSABogusPMSVersionIgnore) {
  EnableOnlyStaticRsaCiphers();
  EnableExtendedMasterSecret();
  MakeTlsFilter<TlsClientHelloVersionChanger>(client_, server_);
  server_->SetOption(SSL_ROLLBACK_DETECTION, PR_FALSE);
  Connect();
}

// Replace the server certificate with one that uses 8193-bit RSA.
class TooLargeRSACertFilter : public TlsHandshakeFilter {
 public:
  TooLargeRSACertFilter(const std::shared_ptr<TlsAgent> &server)
      : TlsHandshakeFilter(server, {kTlsHandshakeCertificate}) {}

 protected:
  virtual PacketFilter::Action FilterHandshake(const HandshakeHeader &header,
                                               const DataBuffer &input,
                                               DataBuffer *output) {
    const uint32_t cert_len = sizeof(rsa8193);
    const uint32_t outer_len = cert_len + 3;
    size_t offset = 0;
    offset = output->Write(offset, outer_len, 3);
    offset = output->Write(offset, cert_len, 3);
    offset = output->Write(offset, rsa8193, cert_len);

    return CHANGE;
  }
};

TEST_P(TlsConnectGenericPre13, TooLargeRSAKeyInCert) {
  EnableOnlyStaticRsaCiphers();
  MakeTlsFilter<TooLargeRSACertFilter>(server_);
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
  client_->CheckErrorCode(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectGeneric, ServerAuthBiggestRsa) {
  Reset(TlsAgent::kRsa8192);
  Connect();
  CheckKeys();
}

}  // namespace nss_test
