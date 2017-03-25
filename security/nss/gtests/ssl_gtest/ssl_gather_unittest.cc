/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

class GatherV2ClientHelloTest : public TlsConnectTestBase {
 public:
  GatherV2ClientHelloTest() : TlsConnectTestBase(STREAM, 0) {}

  void ConnectExpectMalformedClientHello(const DataBuffer &data) {
    EnsureTlsSetup();
    server_->ExpectSendAlert(kTlsAlertIllegalParameter);
    client_->SendDirect(data);
    server_->StartConnect();
    server_->Handshake();
    ASSERT_TRUE_WAIT(
        (server_->error_code() == SSL_ERROR_RX_MALFORMED_CLIENT_HELLO), 2000);
  }
};

// Gather a 5-byte v3 record, with a zero fragment length. The empty handshake
// message should be ignored, and the connection will succeed afterwards.
TEST_F(TlsConnectTest, GatherEmptyV3Record) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x16, 1);    // handshake
  idx = buffer.Write(idx, 0x0301, 2);  // record_version
  (void)buffer.Write(idx, 0U, 2);      // length=0

  EnsureTlsSetup();
  client_->SendDirect(buffer);
  Connect();
}

// Gather a 5-byte v3 record, with a fragment length exceeding the maximum.
TEST_F(TlsConnectTest, GatherExcessiveV3Record) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x16, 1);                            // handshake
  idx = buffer.Write(idx, 0x0301, 2);                          // record_version
  (void)buffer.Write(idx, MAX_FRAGMENT_LENGTH + 2048 + 1, 2);  // length=max+1

  EnsureTlsSetup();
  server_->ExpectSendAlert(kTlsAlertRecordOverflow);
  client_->SendDirect(buffer);
  server_->StartConnect();
  server_->Handshake();
  ASSERT_TRUE_WAIT((server_->error_code() == SSL_ERROR_RX_RECORD_TOO_LONG),
                   2000);
}

// Gather a 3-byte v2 header, with a fragment length of 2.
TEST_F(GatherV2ClientHelloTest, GatherV2RecordLongHeader) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x0002, 2);  // length=2 (long header)
  idx = buffer.Write(idx, 0U, 1);      // padding=0
  (void)buffer.Write(idx, 0U, 2);      // data

  ConnectExpectMalformedClientHello(buffer);
}

// Gather a 3-byte v2 header, with a fragment length of 1.
TEST_F(GatherV2ClientHelloTest, GatherV2RecordLongHeader2) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x0001, 2);  // length=1 (long header)
  idx = buffer.Write(idx, 0U, 1);      // padding=0
  idx = buffer.Write(idx, 0U, 1);      // data
  (void)buffer.Write(idx, 0U, 1);      // surplus (need 5 bytes total)

  ConnectExpectMalformedClientHello(buffer);
}

// Gather a 3-byte v2 header, with a zero fragment length.
TEST_F(GatherV2ClientHelloTest, GatherEmptyV2RecordLongHeader) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0U, 2);  // length=0 (long header)
  idx = buffer.Write(idx, 0U, 1);  // padding=0
  (void)buffer.Write(idx, 0U, 2);  // surplus (need 5 bytes total)

  ConnectExpectMalformedClientHello(buffer);
}

// Gather a 2-byte v2 header, with a fragment length of 3.
TEST_F(GatherV2ClientHelloTest, GatherV2RecordShortHeader) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x8003, 2);  // length=3 (short header)
  (void)buffer.Write(idx, 0U, 3);      // data

  ConnectExpectMalformedClientHello(buffer);
}

// Gather a 2-byte v2 header, with a fragment length of 2.
TEST_F(GatherV2ClientHelloTest, GatherEmptyV2RecordShortHeader2) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x8002, 2);  // length=2 (short header)
  idx = buffer.Write(idx, 0U, 2);      // data
  (void)buffer.Write(idx, 0U, 1);      // surplus (need 5 bytes total)

  ConnectExpectMalformedClientHello(buffer);
}

// Gather a 2-byte v2 header, with a fragment length of 1.
TEST_F(GatherV2ClientHelloTest, GatherEmptyV2RecordShortHeader3) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x8001, 2);  // length=1 (short header)
  idx = buffer.Write(idx, 0U, 1);      // data
  (void)buffer.Write(idx, 0U, 2);      // surplus (need 5 bytes total)

  ConnectExpectMalformedClientHello(buffer);
}

// Gather a 2-byte v2 header, with a zero fragment length.
TEST_F(GatherV2ClientHelloTest, GatherEmptyV2RecordShortHeader) {
  DataBuffer buffer;

  size_t idx = 0;
  idx = buffer.Write(idx, 0x8000, 2);  // length=0 (short header)
  (void)buffer.Write(idx, 0U, 3);      // surplus (need 5 bytes total)

  ConnectExpectMalformedClientHello(buffer);
}

}  // namespace nss_test
