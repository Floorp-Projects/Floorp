/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "ssl.h"
#include "sslimpl.h"

#include "databuffer.h"
#include "gtest_utils.h"
#include "tls_connect.h"
#include "tls_filter.h"

namespace nss_test {

const static size_t kMacSize = 20;

class TlsPaddingTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, bool>> {
 public:
  TlsPaddingTest() : plaintext_len_(std::get<0>(GetParam())) {
    size_t extra =
        (plaintext_len_ + 1) % 16;  // Bytes past a block (1 == pad len)
    // Minimal padding.
    pad_len_ = extra ? 16 - extra : 0;
    if (std::get<1>(GetParam())) {
      // Maximal padding.
      pad_len_ += 240;
    }
    MakePaddedPlaintext();
  }

  // Makes a plaintext record with correct padding.
  void MakePaddedPlaintext() {
    EXPECT_EQ(0UL, (plaintext_len_ + pad_len_ + 1) % 16);
    size_t i = 0;
    plaintext_.Allocate(plaintext_len_ + pad_len_ + 1);
    for (; i < plaintext_len_; ++i) {
      plaintext_.Write(i, 'A', 1);
    }

    for (; i < plaintext_len_ + pad_len_ + 1; ++i) {
      plaintext_.Write(i, pad_len_, 1);
    }
  }

  void Unpad(bool expect_success) {
    std::cerr << "Content length=" << plaintext_len_
              << " padding length=" << pad_len_
              << " total length=" << plaintext_.len() << std::endl;
    std::cerr << "Plaintext: " << plaintext_ << std::endl;
    sslBuffer s;
    s.buf = const_cast<unsigned char*>(
        static_cast<const unsigned char*>(plaintext_.data()));
    s.len = plaintext_.len();
    SECStatus rv = ssl_RemoveTLSCBCPadding(&s, kMacSize);
    if (expect_success) {
      EXPECT_EQ(SECSuccess, rv);
      EXPECT_EQ(plaintext_len_, static_cast<size_t>(s.len));
    } else {
      EXPECT_EQ(SECFailure, rv);
    }
  }

 protected:
  size_t plaintext_len_;
  size_t pad_len_;
  DataBuffer plaintext_;
};

TEST_P(TlsPaddingTest, Correct) {
  if (plaintext_len_ >= kMacSize) {
    Unpad(true);
  } else {
    Unpad(false);
  }
}

TEST_P(TlsPaddingTest, PadTooLong) {
  if (plaintext_.len() < 255) {
    plaintext_.Write(plaintext_.len() - 1, plaintext_.len(), 1);
    Unpad(false);
  }
}

TEST_P(TlsPaddingTest, FirstByteOfPadWrong) {
  if (pad_len_) {
    plaintext_.Write(plaintext_len_, plaintext_.data()[plaintext_len_] + 1, 1);
    Unpad(false);
  }
}

TEST_P(TlsPaddingTest, LastByteOfPadWrong) {
  if (pad_len_) {
    plaintext_.Write(plaintext_.len() - 2,
                     plaintext_.data()[plaintext_.len() - 1] + 1, 1);
    Unpad(false);
  }
}

class RecordReplacer : public TlsRecordFilter {
 public:
  RecordReplacer(const std::shared_ptr<TlsAgent>& a, size_t size)
      : TlsRecordFilter(a), size_(size) {
    Disable();
  }

  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& data,
                                    DataBuffer* changed) override {
    EXPECT_EQ(ssl_ct_application_data, header.content_type());
    changed->Allocate(size_);

    for (size_t i = 0; i < size_; ++i) {
      changed->data()[i] = i & 0xff;
    }

    Disable();
    return CHANGE;
  }

 private:
  size_t size_;
};

TEST_P(TlsConnectStream, BadRecordMac) {
  EnsureTlsSetup();
  Connect();
  client_->SetFilter(std::make_shared<TlsRecordLastByteDamager>(client_));
  ExpectAlert(server_, kTlsAlertBadRecordMac);
  client_->SendData(10);

  // Read from the client, get error.
  uint8_t buf[10];
  PRInt32 rv = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_GT(0, rv);
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, PORT_GetError());

  // Read the server alert.
  rv = PR_Read(client_->ssl_fd(), buf, sizeof(buf));
  EXPECT_GT(0, rv);
  EXPECT_EQ(SSL_ERROR_BAD_MAC_ALERT, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, LargeRecord) {
  EnsureTlsSetup();

  const size_t record_limit = 16384;
  auto replacer = MakeTlsFilter<RecordReplacer>(client_, record_limit);
  replacer->EnableDecryption();
  Connect();

  replacer->Enable();
  client_->SendData(10);
  WAIT_(server_->received_bytes() == record_limit, 2000);
  ASSERT_EQ(record_limit, server_->received_bytes());
}

TEST_F(TlsConnectStreamTls13, TooLargeRecord) {
  EnsureTlsSetup();

  const size_t record_limit = 16384;
  auto replacer = MakeTlsFilter<RecordReplacer>(client_, record_limit + 1);
  replacer->EnableDecryption();
  Connect();

  replacer->Enable();
  ExpectAlert(server_, kTlsAlertRecordOverflow);
  client_->SendData(10);  // This is expanded.

  uint8_t buf[record_limit + 2];
  PRInt32 rv = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_GT(0, rv);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, PORT_GetError());

  // Read the server alert.
  rv = PR_Read(client_->ssl_fd(), buf, sizeof(buf));
  EXPECT_GT(0, rv);
  EXPECT_EQ(SSL_ERROR_RECORD_OVERFLOW_ALERT, PORT_GetError());
}

class ShortHeaderChecker : public PacketFilter {
 public:
  PacketFilter::Action Filter(const DataBuffer& input, DataBuffer* output) {
    // The first octet should be 0b001000xx.
    EXPECT_EQ(kCtDtlsCiphertext, (input.data()[0] & ~0x3));
    return KEEP;
  }
};

TEST_F(TlsConnectDatagram13, AeadLimit) {
  Connect();
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceDtls13DecryptFailures(server_->ssl_fd(),
                                                            (1ULL << 36) - 2));
  SendReceive(50);

  // Expect this to increment the counter. We should still be able to talk.
  client_->SetFilter(std::make_shared<TlsRecordLastByteDamager>(client_));
  client_->SendData(10);
  server_->ReadBytes(10);
  client_->ClearFilter();
  client_->ResetSentBytes(50);
  SendReceive(60);

  // Expect alert when the limit is hit.
  client_->SetFilter(std::make_shared<TlsRecordLastByteDamager>(client_));
  client_->SendData(10);
  ExpectAlert(server_, kTlsAlertBadRecordMac);

  // Check the error on both endpoints.
  uint8_t buf[10];
  PRInt32 rv = PR_Read(server_->ssl_fd(), buf, sizeof(buf));
  EXPECT_EQ(-1, rv);
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, PORT_GetError());

  rv = PR_Read(client_->ssl_fd(), buf, sizeof(buf));
  EXPECT_EQ(-1, rv);
  EXPECT_EQ(SSL_ERROR_BAD_MAC_ALERT, PORT_GetError());
}

TEST_F(TlsConnectDatagram13, ShortHeadersClient) {
  Connect();
  client_->SetOption(SSL_ENABLE_DTLS_SHORT_HEADER, PR_TRUE);
  client_->SetFilter(std::make_shared<ShortHeaderChecker>());
  SendReceive();
}

TEST_F(TlsConnectDatagram13, ShortHeadersServer) {
  Connect();
  server_->SetOption(SSL_ENABLE_DTLS_SHORT_HEADER, PR_TRUE);
  server_->SetFilter(std::make_shared<ShortHeaderChecker>());
  SendReceive();
}

// Send a DTLSCiphertext header with a 2B sequence number, and no length.
TEST_F(TlsConnectDatagram13, DtlsAlternateShortHeader) {
  StartConnect();
  TlsSendCipherSpecCapturer capturer(client_);
  Connect();
  SendReceive(50);

  uint8_t buf[] = {0x32, 0x33, 0x34};
  auto spec = capturer.spec(1);
  ASSERT_NE(nullptr, spec.get());
  ASSERT_EQ(3, spec->epoch());

  uint8_t dtls13_ct = kCtDtlsCiphertext | kCtDtlsCiphertext16bSeqno;
  TlsRecordHeader header(variant_, SSL_LIBRARY_VERSION_TLS_1_3, dtls13_ct,
                         0x0003000000000001);
  TlsRecordHeader out_header(header);
  DataBuffer msg(buf, sizeof(buf));
  msg.Write(msg.len(), ssl_ct_application_data, 1);
  DataBuffer ciphertext;
  EXPECT_TRUE(spec->Protect(header, msg, &ciphertext, &out_header));

  DataBuffer record;
  auto rv = out_header.Write(&record, 0, ciphertext);
  EXPECT_EQ(out_header.header_length() + ciphertext.len(), rv);
  client_->SendDirect(record);

  server_->ReadBytes(3);
}

TEST_F(TlsConnectStreamTls13, UnencryptedFinishedMessage) {
  StartConnect();
  client_->Handshake();  // Send ClientHello
  server_->Handshake();  // Send first server flight

  // Record and drop the first record, which is the Finished.
  auto recorder = std::make_shared<TlsRecordRecorder>(client_);
  recorder->EnableDecryption();
  auto dropper = std::make_shared<SelectiveDropFilter>(1);
  client_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit({recorder, dropper})));
  client_->Handshake();  // Save and drop CFIN.
  EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());

  ASSERT_EQ(1U, recorder->count());
  auto& finished = recorder->record(0);

  DataBuffer d;
  size_t offset = d.Write(0, ssl_ct_handshake, 1);
  offset = d.Write(offset, SSL_LIBRARY_VERSION_TLS_1_2, 2);
  offset = d.Write(offset, finished.buffer.len(), 2);
  d.Append(finished.buffer);
  client_->SendDirect(d);

  // Now process the message.
  ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  // The server should generate an alert.
  server_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, server_->state());
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_RECORD_TYPE);
  // Have the client consume the alert.
  client_->Handshake();
  EXPECT_EQ(TlsAgent::STATE_ERROR, client_->state());
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

const static size_t kContentSizesArr[] = {
    1, kMacSize - 1, kMacSize, 30, 31, 32, 36, 256, 257, 287, 288};

auto kContentSizes = ::testing::ValuesIn(kContentSizesArr);
const static bool kTrueFalseArr[] = {true, false};
auto kTrueFalse = ::testing::ValuesIn(kTrueFalseArr);

INSTANTIATE_TEST_SUITE_P(TlsPadding, TlsPaddingTest,
                         ::testing::Combine(kContentSizes, kTrueFalse));
}  // namespace nss_test
