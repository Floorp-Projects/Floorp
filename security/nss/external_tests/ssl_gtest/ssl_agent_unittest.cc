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

void MakeTrivialHandshakeMessage(uint8_t hs_type, size_t hs_len,
                                 DataBuffer* out) {
  size_t total_len = 5 + 4 + hs_len;

  out->Allocate(total_len);

  size_t index = 0;
  out->Write(index, kTlsHandshakeType, 1); ++index; // Content Type
  out->Write(index, 3, 1); ++index; // Version high
  out->Write(index, 1, 1); ++index; // Version low
  out->Write(index, 4 + hs_len, 2); index += 2; // Length

  out->Write(index, hs_type, 1); ++index; // Handshake record type.
  out->Write(index, hs_len, 3); index += 3; // Handshake length
  for (; index < total_len; ++index) {
    out->Write(index, 1, 1);
  }
}

TEST_P(TlsAgentTest, EarlyFinished) {
  DataBuffer buffer;
  MakeTrivialHandshakeMessage(kTlsHandshakeFinished, 0, &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_FINISHED);
}

TEST_P(TlsAgentTest, EarlyCertificateVerify) {
  DataBuffer buffer;
  MakeTrivialHandshakeMessage(kTlsHandshakeCertificateVerify, 0, &buffer);
  ProcessMessage(buffer, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_CERT_VERIFY);
}

INSTANTIATE_TEST_CASE_P(AgentTests, TlsAgentTest,
                        ::testing::Combine(
                             TlsAgentTestBase::kTlsRolesAll,
                             TlsConnectTestBase::kTlsModesStream));

}  // namespace nss_test
