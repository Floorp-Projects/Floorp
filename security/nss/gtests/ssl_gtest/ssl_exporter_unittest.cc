/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

static const char* kExporterLabel = "EXPORTER-duck";
static const uint8_t kExporterContext[] = {0x12, 0x34, 0x56};

static void ExportAndCompare(TlsAgent* client, TlsAgent* server, bool context) {
  static const size_t exporter_len = 10;
  uint8_t client_value[exporter_len] = {0};
  EXPECT_EQ(SECSuccess,
            SSL_ExportKeyingMaterial(
                client->ssl_fd(), kExporterLabel, strlen(kExporterLabel),
                context ? PR_TRUE : PR_FALSE, kExporterContext,
                sizeof(kExporterContext), client_value, sizeof(client_value)));
  uint8_t server_value[exporter_len] = {0xff};
  EXPECT_EQ(SECSuccess,
            SSL_ExportKeyingMaterial(
                server->ssl_fd(), kExporterLabel, strlen(kExporterLabel),
                context ? PR_TRUE : PR_FALSE, kExporterContext,
                sizeof(kExporterContext), server_value, sizeof(server_value)));
  EXPECT_EQ(0, memcmp(client_value, server_value, sizeof(client_value)));
}

TEST_P(TlsConnectGeneric, ExporterBasic) {
  EnsureTlsSetup();
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    server_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
  } else {
    server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
  }
  Connect();
  CheckKeys();
  ExportAndCompare(client_, server_, false);
}

TEST_P(TlsConnectGeneric, ExporterContext) {
  EnsureTlsSetup();
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    server_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
  } else {
    server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
  }
  Connect();
  CheckKeys();
  ExportAndCompare(client_, server_, true);
}

// Bug 1312976 - SHA-384 doesn't work in 1.2 right now.
TEST_P(TlsConnectTls13, ExporterSha384) {
  EnsureTlsSetup();
  client_->EnableSingleCipher(TLS_AES_256_GCM_SHA384);
  Connect();
  CheckKeys();
  ExportAndCompare(client_, server_, false);
}

TEST_P(TlsConnectTls13, ExporterContextEmptyIsSameAsNone) {
  EnsureTlsSetup();
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    server_->EnableSingleCipher(TLS_AES_128_GCM_SHA256);
  } else {
    server_->EnableSingleCipher(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
  }
  Connect();
  CheckKeys();
  ExportAndCompare(client_, server_, false);
}

// This has a weird signature so that it can be passed to the SNI callback.
int32_t RegularExporterShouldFail(TlsAgent* agent, const SECItem* srvNameArr,
                                  PRUint32 srvNameArrSize) {
  uint8_t val[10];
  EXPECT_EQ(SECFailure, SSL_ExportKeyingMaterial(
                            agent->ssl_fd(), kExporterLabel,
                            strlen(kExporterLabel), PR_TRUE, kExporterContext,
                            sizeof(kExporterContext), val, sizeof(val)))
      << "regular exporter should fail";
  return 0;
}

TEST_P(TlsConnectTls13, EarlyExporter) {
  SetupForZeroRtt();
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  client_->Handshake();  // Send ClientHello.
  uint8_t client_value[10] = {0};
  RegularExporterShouldFail(client_, nullptr, 0);
  EXPECT_EQ(SECSuccess,
            SSL_ExportEarlyKeyingMaterial(
                client_->ssl_fd(), kExporterLabel, strlen(kExporterLabel),
                kExporterContext, sizeof(kExporterContext), client_value,
                sizeof(client_value)));

  server_->SetSniCallback(RegularExporterShouldFail);
  server_->Handshake();  // Handle ClientHello.
  uint8_t server_value[10] = {0};
  EXPECT_EQ(SECSuccess,
            SSL_ExportEarlyKeyingMaterial(
                server_->ssl_fd(), kExporterLabel, strlen(kExporterLabel),
                kExporterContext, sizeof(kExporterContext), server_value,
                sizeof(server_value)));
  EXPECT_EQ(0, memcmp(client_value, server_value, sizeof(client_value)));

  Handshake();
  ExpectEarlyDataAccepted(true);
  CheckConnected();
  SendReceive();
}

}  // namespace nss_test
