/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

// This is an internal header, used to get TLS_1_3_DRAFT_VERSION.
#include "ssl3prot.h"

#include <memory>

#include "databuffer.h"
#include "tls_agent.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

static const uint8_t kD13 = TLS_1_3_DRAFT_VERSION;
// This is a 1-RTT ClientHello with ECDHE.
const static uint8_t kCannedTls13ClientHello[] = {
    0x01, 0x00, 0x00, 0xcf, 0x03, 0x03, 0x6c, 0xb3, 0x46, 0x81, 0xc8, 0x1a,
    0xf9, 0xd2, 0x05, 0x97, 0x48, 0x7c, 0xa8, 0x31, 0x03, 0x1c, 0x06, 0xa8,
    0x62, 0xb1, 0x90, 0xd6, 0x21, 0x44, 0x7f, 0xc1, 0x9b, 0x87, 0x3e, 0xad,
    0x91, 0x85, 0x00, 0x00, 0x06, 0x13, 0x01, 0x13, 0x03, 0x13, 0x02, 0x01,
    0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x09, 0x00, 0x00, 0x06,
    0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x0a, 0x00, 0x12, 0x00, 0x10, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x03, 0x01, 0x04, 0x00, 0x28, 0x00,
    0x47, 0x00, 0x45, 0x00, 0x17, 0x00, 0x41, 0x04, 0x86, 0x4a, 0xb9, 0xdc,
    0x6a, 0x38, 0xa7, 0xce, 0xe7, 0xc2, 0x4f, 0xa6, 0x28, 0xb9, 0xdc, 0x65,
    0xbf, 0x73, 0x47, 0x3c, 0x9c, 0x65, 0x8c, 0x47, 0x6d, 0x57, 0x22, 0x8a,
    0xc2, 0xb3, 0xc6, 0x80, 0x72, 0x86, 0x08, 0x86, 0x8f, 0x52, 0xc5, 0xcb,
    0xbf, 0x2a, 0xb5, 0x59, 0x64, 0xcc, 0x0c, 0x49, 0x95, 0x36, 0xe4, 0xd9,
    0x2f, 0xd4, 0x24, 0x66, 0x71, 0x6f, 0x5d, 0x70, 0xe2, 0xa0, 0xea, 0x26,
    0x00, 0x2b, 0x00, 0x03, 0x02, 0x7f, kD13, 0x00, 0x0d, 0x00, 0x20, 0x00,
    0x1e, 0x04, 0x03, 0x05, 0x03, 0x06, 0x03, 0x02, 0x03, 0x08, 0x04, 0x08,
    0x05, 0x08, 0x06, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01, 0x02, 0x01, 0x04,
    0x02, 0x05, 0x02, 0x06, 0x02, 0x02, 0x02};

const static uint8_t kCannedTls13ServerHello[] = {
    0x7f, kD13, 0x21, 0x12, 0xa7, 0xa7, 0x0d, 0x85, 0x8b, 0xb8, 0x0c, 0xbb,
    0xdc, 0xa6, 0xfd, 0x97, 0xfe, 0x31, 0x26, 0x49, 0x2d, 0xa8, 0x6c, 0x7b,
    0x65, 0x30, 0x71, 0x00, 0x31, 0x03, 0x2b, 0x94, 0xe2, 0x16, 0x13, 0x01,
    0x00, 0x4d, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x28, 0x00, 0x45, 0x00, 0x17,
    0x00, 0x41, 0x04, 0x10, 0x97, 0x3d, 0x7a, 0xcf, 0xa2, 0x34, 0xe3, 0x69,
    0xc4, 0xdd, 0x1e, 0xf2, 0xd6, 0xc0, 0x9a, 0x3e, 0xf5, 0x41, 0xf3, 0x03,
    0x23, 0x94, 0xd2, 0x31, 0x85, 0xb7, 0xae, 0x5d, 0xfa, 0xc6, 0x9a, 0xd0,
    0xa5, 0x44, 0xa3, 0x3a, 0xe0, 0xbb, 0x61, 0xaa, 0x0a, 0x6f, 0xe8, 0xaf,
    0xdf, 0x86, 0xd8, 0x48, 0x36, 0x9c, 0x19, 0x70, 0x55, 0x84, 0xb0, 0x1c,
    0x8d, 0xfc, 0x6e, 0xd8, 0x86, 0xc3, 0x95};
static const char *k0RttData = "ABCDEF";

TEST_P(TlsAgentTest, EarlyFinished) {
  DataBuffer buffer;
  MakeTrivialHandshakeRecord(kTlsHandshakeFinished, 0, &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_FINISHED);
}

TEST_P(TlsAgentTest, EarlyCertificateVerify) {
  DataBuffer buffer;
  MakeTrivialHandshakeRecord(kTlsHandshakeCertificateVerify, 0, &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY);
}

TEST_P(TlsAgentTestClient, CannedHello) {
  DataBuffer buffer;
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  DataBuffer server_hello;
  MakeHandshakeMessage(kTlsHandshakeServerHello, kCannedTls13ServerHello,
                       sizeof(kCannedTls13ServerHello), &server_hello);
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data(), server_hello.len(), &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
}

TEST_P(TlsAgentTestClient, EncryptedExtensionsInClear) {
  DataBuffer server_hello;
  MakeHandshakeMessage(kTlsHandshakeServerHello, kCannedTls13ServerHello,
                       sizeof(kCannedTls13ServerHello), &server_hello);
  DataBuffer encrypted_extensions;
  MakeHandshakeMessage(kTlsHandshakeEncryptedExtensions, nullptr, 0,
                       &encrypted_extensions, 1);
  server_hello.Append(encrypted_extensions);
  DataBuffer buffer;
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data(), server_hello.len(), &buffer);
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}

TEST_F(TlsAgentStreamTestClient, EncryptedExtensionsInClearTwoPieces) {
  DataBuffer server_hello;
  MakeHandshakeMessage(kTlsHandshakeServerHello, kCannedTls13ServerHello,
                       sizeof(kCannedTls13ServerHello), &server_hello);
  DataBuffer encrypted_extensions;
  MakeHandshakeMessage(kTlsHandshakeEncryptedExtensions, nullptr, 0,
                       &encrypted_extensions, 1);
  server_hello.Append(encrypted_extensions);
  DataBuffer buffer;
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data(), 20, &buffer);

  DataBuffer buffer2;
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data() + 20, server_hello.len() - 20, &buffer2);

  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
  ProcessMessage(buffer2, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}

TEST_F(TlsAgentDgramTestClient, EncryptedExtensionsInClearTwoPieces) {
  DataBuffer server_hello_frag1;
  MakeHandshakeMessageFragment(
      kTlsHandshakeServerHello, kCannedTls13ServerHello,
      sizeof(kCannedTls13ServerHello), &server_hello_frag1, 0, 0, 20);
  DataBuffer server_hello_frag2;
  MakeHandshakeMessageFragment(
      kTlsHandshakeServerHello, kCannedTls13ServerHello + 20,
      sizeof(kCannedTls13ServerHello), &server_hello_frag2, 0, 20,
      sizeof(kCannedTls13ServerHello) - 20);
  DataBuffer encrypted_extensions;
  MakeHandshakeMessage(kTlsHandshakeEncryptedExtensions, nullptr, 0,
                       &encrypted_extensions, 1);
  server_hello_frag2.Append(encrypted_extensions);
  DataBuffer buffer;
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello_frag1.data(), server_hello_frag1.len(), &buffer);

  DataBuffer buffer2;
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello_frag2.data(), server_hello_frag2.len(), &buffer2, 1);

  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
  ProcessMessage(buffer2, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}

TEST_F(TlsAgentStreamTestClient, Set0RttOptionThenWrite) {
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  agent_->StartConnect();
  agent_->Set0RttEnabled(true);
  auto filter =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeClientHello);
  agent_->SetPacketFilter(filter);
  PRInt32 rv = PR_Write(agent_->ssl_fd(), k0RttData, strlen(k0RttData));
  EXPECT_EQ(-1, rv);
  int32_t err = PORT_GetError();
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, err);
  EXPECT_LT(0UL, filter->buffer().len());
}

TEST_F(TlsAgentStreamTestClient, Set0RttOptionThenRead) {
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  agent_->StartConnect();
  agent_->Set0RttEnabled(true);
  DataBuffer buffer;
  MakeRecord(kTlsApplicationDataType, SSL_LIBRARY_VERSION_TLS_1_3,
             reinterpret_cast<const uint8_t *>(k0RttData), strlen(k0RttData),
             &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_APPLICATION_DATA);
}

// The server is allowing 0-RTT but the client doesn't offer it,
// so trial decryption isn't engaged and 0-RTT messages cause
// an error.
TEST_F(TlsAgentStreamTestServer, Set0RttOptionClientHelloThenRead) {
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  agent_->StartConnect();
  agent_->Set0RttEnabled(true);
  DataBuffer buffer;
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             kCannedTls13ClientHello, sizeof(kCannedTls13ClientHello), &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
  MakeRecord(kTlsApplicationDataType, SSL_LIBRARY_VERSION_TLS_1_3,
             reinterpret_cast<const uint8_t *>(k0RttData), strlen(k0RttData),
             &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR, SSL_ERROR_BAD_MAC_READ);
}

INSTANTIATE_TEST_CASE_P(
    AgentTests, TlsAgentTest,
    ::testing::Combine(TlsAgentTestBase::kTlsRolesAll,
                       TlsConnectTestBase::kTlsModesStream));
INSTANTIATE_TEST_CASE_P(ClientTests, TlsAgentTestClient,
                        TlsConnectTestBase::kTlsModesAll);
}  // namespace nss_test
