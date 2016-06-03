/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include <memory>

#include "databuffer.h"
#include "tls_agent.h"
#include "tls_connect.h"
#include "tls_parser.h"

namespace nss_test {

#ifdef NSS_ENABLE_TLS_1_3
const static uint8_t kCannedServerTls13ServerHello[] = {
  0x03, 0x04, 0x54, 0x1a, 0xfc, 0x6c, 0x8e, 0xd7,
  0x08, 0x96, 0x8f, 0x9c, 0xc8, 0x0a, 0xf6, 0x50,
  0x20, 0x15, 0xa9, 0x75, 0x52, 0xc5, 0x9c, 0x74,
  0xf6, 0xc9, 0xea, 0x3b, 0x77, 0x5a, 0x83, 0xaa,
  0x66, 0xdc, 0xc0, 0x2f, 0x00, 0x4a, 0x00, 0x28,
  0x00, 0x46, 0x00, 0x17, 0x00, 0x42, 0x41, 0x04,
  0xbd, 0xbc, 0x15, 0x6f, 0x43, 0xc7, 0x67, 0x7c,
  0x08, 0x78, 0x08, 0x4d, 0x85, 0x4f, 0xf9, 0x45,
  0x10, 0x4a, 0x04, 0xf4, 0x07, 0xc0, 0x4c, 0x26,
  0x51, 0xa0, 0x38, 0x49, 0x98, 0xf7, 0x79, 0x0f,
  0xb4, 0x26, 0xe6, 0xac, 0x4b, 0x5a, 0x2a, 0x04,
  0x80, 0x14, 0x03, 0xd0, 0x31, 0xf1, 0x70, 0xe2,
  0x72, 0x1d, 0x5e, 0xfb, 0x74, 0x80, 0xc8, 0x9e,
  0x08, 0x39, 0xd6, 0xa5, 0x1b, 0xcf, 0x54, 0x9f
};
#endif

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

#ifdef NSS_ENABLE_TLS_1_3
TEST_P(TlsAgentTestClient, CannedServerHello) {
  DataBuffer buffer;
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  DataBuffer server_hello_inner(kCannedServerTls13ServerHello,
                                sizeof(kCannedServerTls13ServerHello));
  uint16_t wire_version = mode_ == STREAM ?
      SSL_LIBRARY_VERSION_TLS_1_3:
      TlsVersionToDtlsVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  server_hello_inner.Write(0, wire_version, 2);
  DataBuffer server_hello;
  MakeHandshakeMessage(kTlsHandshakeServerHello,
                       server_hello_inner.data(),
                       server_hello_inner.len(),
                       &server_hello);
  MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data(), server_hello.len(), &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
}

TEST_P(TlsAgentTestClient, EncryptedExtensionsInClear) {
  DataBuffer buffer;
  DataBuffer server_hello_inner(kCannedServerTls13ServerHello,
                                sizeof(kCannedServerTls13ServerHello));
  server_hello_inner.Write(0,
                           mode_ == STREAM ?
                           SSL_LIBRARY_VERSION_TLS_1_3:
                           TlsVersionToDtlsVersion(
                               SSL_LIBRARY_VERSION_TLS_1_3),
                           2);
  DataBuffer server_hello;
  MakeHandshakeMessage(kTlsHandshakeServerHello,
                       server_hello_inner.data(),
                       server_hello_inner.len(),
                       &server_hello);
  DataBuffer encrypted_extensions;
  MakeHandshakeMessage(kTlsHandshakeEncryptedExtensions, nullptr, 0,
                       &encrypted_extensions, 1);
  server_hello.Append(encrypted_extensions);
  MakeRecord(kTlsHandshakeType,
             SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data(),
             server_hello.len(), &buffer);
  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}

TEST_F(TlsAgentStreamTestClient, EncryptedExtensionsInClearTwoPieces) {
  DataBuffer buffer;
  DataBuffer buffer2;
  DataBuffer server_hello_inner(kCannedServerTls13ServerHello,
                                sizeof(kCannedServerTls13ServerHello));
  server_hello_inner.Write(0, SSL_LIBRARY_VERSION_TLS_1_3, 2);
  DataBuffer server_hello;
  MakeHandshakeMessage(kTlsHandshakeServerHello,
                       server_hello_inner.data(),
                       server_hello_inner.len(),
                       &server_hello);
  DataBuffer encrypted_extensions;
  MakeHandshakeMessage(kTlsHandshakeEncryptedExtensions, nullptr, 0,
                       &encrypted_extensions, 1);
  server_hello.Append(encrypted_extensions);
  MakeRecord(kTlsHandshakeType,
             SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data(), 20,
             &buffer);

  MakeRecord(kTlsHandshakeType,
             SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello.data() + 20,
             server_hello.len() - 20, &buffer2);

  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
  ProcessMessage(buffer2, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}


TEST_F(TlsAgentDgramTestClient, EncryptedExtensionsInClearTwoPieces) {
  DataBuffer buffer;
  DataBuffer buffer2;
  DataBuffer server_hello_inner(kCannedServerTls13ServerHello,
                                sizeof(kCannedServerTls13ServerHello));
  server_hello_inner.Write(
      0, TlsVersionToDtlsVersion(SSL_LIBRARY_VERSION_TLS_1_3), 2);
  DataBuffer server_hello_frag1;
  MakeHandshakeMessageFragment(kTlsHandshakeServerHello,
                       server_hello_inner.data(),
                       server_hello_inner.len(),
                       &server_hello_frag1, 0,
                       0, 20);
  DataBuffer server_hello_frag2;
  MakeHandshakeMessageFragment(kTlsHandshakeServerHello,
                       server_hello_inner.data() + 20,
                       server_hello_inner.len(), &server_hello_frag2, 0,
                       20, server_hello_inner.len() - 20);
  DataBuffer encrypted_extensions;
  MakeHandshakeMessage(kTlsHandshakeEncryptedExtensions, nullptr, 0,
                       &encrypted_extensions, 1);
  server_hello_frag2.Append(encrypted_extensions);
  MakeRecord(kTlsHandshakeType,
             SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello_frag1.data(), server_hello_frag1.len(),
             &buffer);

  MakeRecord(kTlsHandshakeType,
             SSL_LIBRARY_VERSION_TLS_1_3,
             server_hello_frag2.data(), server_hello_frag2.len(),
             &buffer2, 1);

  EnsureInit();
  agent_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_3,
                          SSL_LIBRARY_VERSION_TLS_1_3);
  ProcessMessage(buffer, TlsAgent::STATE_CONNECTING);
  ProcessMessage(buffer2, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HANDSHAKE);
}
#endif

INSTANTIATE_TEST_CASE_P(AgentTests, TlsAgentTest,
                        ::testing::Combine(
                             TlsAgentTestBase::kTlsRolesAll,
                             TlsConnectTestBase::kTlsModesStream));
#ifdef NSS_ENABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(ClientTests, TlsAgentTestClient,
                        TlsConnectTestBase::kTlsModesAll);
#endif
} // namespace nss_test
