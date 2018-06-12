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
    EXPECT_EQ(kTlsApplicationDataType, header.content_type());
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
    // The first octet should be 0b001xxxxx.
    EXPECT_EQ(1, input.data()[0] >> 5);
    return KEEP;
  }
};

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

const static size_t kContentSizesArr[] = {
    1, kMacSize - 1, kMacSize, 30, 31, 32, 36, 256, 257, 287, 288};

auto kContentSizes = ::testing::ValuesIn(kContentSizesArr);
const static bool kTrueFalseArr[] = {true, false};
auto kTrueFalse = ::testing::ValuesIn(kTrueFalseArr);

INSTANTIATE_TEST_CASE_P(TlsPadding, TlsPaddingTest,
                        ::testing::Combine(kContentSizes, kTrueFalse));
}  // namespace nss_test
