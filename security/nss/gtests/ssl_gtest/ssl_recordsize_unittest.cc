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
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

// This class tracks the maximum size of record that was sent, both cleartext
// and plain.  It only tracks records that have an outer type of
// application_data.  In TLS 1.3, this includes handshake messages.
class TlsRecordMaximum : public TlsRecordFilter {
 public:
  TlsRecordMaximum(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a), max_ciphertext_(0), max_plaintext_(0) {}

  size_t max_ciphertext() const { return max_ciphertext_; }
  size_t max_plaintext() const { return max_plaintext_; }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    std::cerr << "max: " << record << std::endl;
    // Ignore unprotected packets.
    if (header.content_type() != ssl_ct_application_data) {
      return KEEP;
    }

    max_ciphertext_ = (std::max)(max_ciphertext_, record.len());
    return TlsRecordFilter::FilterRecord(header, record, offset, output);
  }

  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& data,
                                    DataBuffer* changed) override {
    max_plaintext_ = (std::max)(max_plaintext_, data.len());
    return KEEP;
  }

 private:
  size_t max_ciphertext_;
  size_t max_plaintext_;
};

void CheckRecordSizes(const std::shared_ptr<TlsAgent>& agent,
                      const std::shared_ptr<TlsRecordMaximum>& record_max,
                      size_t config) {
  uint16_t cipher_suite;
  ASSERT_TRUE(agent->cipher_suite(&cipher_suite));

  size_t expansion;
  size_t iv;
  switch (cipher_suite) {
    case TLS_AES_128_GCM_SHA256:
    case TLS_AES_256_GCM_SHA384:
    case TLS_CHACHA20_POLY1305_SHA256:
      expansion = 16;
      iv = 0;
      break;

    case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
      expansion = 16;
      iv = 8;
      break;

    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
      // Expansion is 20 for the MAC.  Maximum block padding is 16.  Maximum
      // padding is added when the input plus the MAC is an exact multiple of
      // the block size.
      expansion = 20 + 16 - ((config + 20) % 16);
      iv = 16;
      break;

    default:
      ADD_FAILURE() << "No expansion set for ciphersuite "
                    << agent->cipher_suite_name();
      return;
  }

  switch (agent->version()) {
    case SSL_LIBRARY_VERSION_TLS_1_3:
      EXPECT_EQ(0U, iv) << "No IV for TLS 1.3";
      // We only have decryption in TLS 1.3.
      EXPECT_EQ(config - 1, record_max->max_plaintext())
          << "bad plaintext length for " << agent->role_str();
      break;

    case SSL_LIBRARY_VERSION_TLS_1_2:
    case SSL_LIBRARY_VERSION_TLS_1_1:
      expansion += iv;
      break;

    case SSL_LIBRARY_VERSION_TLS_1_0:
      break;

    default:
      ADD_FAILURE() << "Unexpected version " << agent->version();
      return;
  }

  EXPECT_EQ(config + expansion, record_max->max_ciphertext())
      << "bad ciphertext length for " << agent->role_str();
}

TEST_P(TlsConnectGeneric, RecordSizeMaximum) {
  uint16_t max_record_size =
      (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) ? 16385 : 16384;
  size_t send_size = (version_ >= SSL_LIBRARY_VERSION_TLS_1_3)
                         ? max_record_size
                         : max_record_size + 1;

  EnsureTlsSetup();
  auto client_max = MakeTlsFilter<TlsRecordMaximum>(client_);
  auto server_max = MakeTlsFilter<TlsRecordMaximum>(server_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_max->EnableDecryption();
    server_max->EnableDecryption();
  }

  Connect();
  client_->SendData(send_size, send_size);
  server_->SendData(send_size, send_size);
  server_->ReadBytes(send_size);
  client_->ReadBytes(send_size);

  CheckRecordSizes(client_, client_max, max_record_size);
  CheckRecordSizes(server_, server_max, max_record_size);
}

TEST_P(TlsConnectGeneric, RecordSizeMinimumClient) {
  EnsureTlsSetup();
  auto server_max = MakeTlsFilter<TlsRecordMaximum>(server_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    server_max->EnableDecryption();
  }

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();
  SendReceive(127);  // Big enough for one record, allowing for 1+N splitting.

  CheckRecordSizes(server_, server_max, 64);
}

TEST_P(TlsConnectGeneric, RecordSizeMinimumServer) {
  EnsureTlsSetup();
  auto client_max = MakeTlsFilter<TlsRecordMaximum>(client_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_max->EnableDecryption();
  }

  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();
  SendReceive(127);

  CheckRecordSizes(client_, client_max, 64);
}

TEST_P(TlsConnectGeneric, RecordSizeAsymmetric) {
  EnsureTlsSetup();
  auto client_max = MakeTlsFilter<TlsRecordMaximum>(client_);
  auto server_max = MakeTlsFilter<TlsRecordMaximum>(server_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_max->EnableDecryption();
    server_max->EnableDecryption();
  }

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 100);
  Connect();
  SendReceive(127);

  CheckRecordSizes(client_, client_max, 100);
  CheckRecordSizes(server_, server_max, 64);
}

// This just modifies the encrypted payload so to include a few extra zeros.
class TlsRecordExpander : public TlsRecordFilter {
 public:
  TlsRecordExpander(const std::shared_ptr<TlsAgent>& a, size_t expansion)
      : TlsRecordFilter(a), expansion_(expansion) {}

 protected:
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& data,
                                            DataBuffer* changed) {
    if (header.content_type() != ssl_ct_application_data) {
      return KEEP;
    }
    changed->Allocate(data.len() + expansion_);
    changed->Write(0, data.data(), data.len());
    return CHANGE;
  }

 private:
  size_t expansion_;
};

// Tweak the plaintext of server records so that they exceed the client's limit.
TEST_P(TlsConnectTls13, RecordSizePlaintextExceed) {
  EnsureTlsSetup();
  auto server_expand = MakeTlsFilter<TlsRecordExpander>(server_, 1);
  server_expand->EnableDecryption();

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();

  server_->SendData(100);

  client_->ExpectReadWriteError();
  ExpectAlert(client_, kTlsAlertRecordOverflow);
  client_->ReadBytes(100);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, client_->error_code());

  // Consume the alert at the server.
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RECORD_OVERFLOW_ALERT);
}

// Tweak the ciphertext of server records so that they greatly exceed the limit.
// This requires a much larger expansion than for plaintext to trigger the
// guard, which runs before decryption (current allowance is 320 octets,
// see MAX_EXPANSION in ssl3con.c).
TEST_P(TlsConnectTls13, RecordSizeCiphertextExceed) {
  EnsureTlsSetup();

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();

  auto server_expand = MakeTlsFilter<TlsRecordExpander>(server_, 336);
  server_->SendData(100);

  client_->ExpectReadWriteError();
  ExpectAlert(client_, kTlsAlertRecordOverflow);
  client_->ReadBytes(100);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, client_->error_code());

  // Consume the alert at the server.
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RECORD_OVERFLOW_ALERT);
}

// This indiscriminately adds padding to application data records.
class TlsRecordPadder : public TlsRecordFilter {
 public:
  TlsRecordPadder(const std::shared_ptr<TlsAgent>& a, size_t padding)
      : TlsRecordFilter(a), padding_(padding) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    if (header.content_type() != ssl_ct_application_data) {
      return KEEP;
    }

    uint16_t protection_epoch;
    uint8_t inner_content_type;
    DataBuffer plaintext;
    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext)) {
      return KEEP;
    }

    if (inner_content_type != ssl_ct_application_data) {
      return KEEP;
    }

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), header, inner_content_type,
                      plaintext, &ciphertext, padding_);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }
    *offset = header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  size_t padding_;
};

TEST_P(TlsConnectTls13, RecordSizeExceedPad) {
  EnsureTlsSetup();
  auto server_max = std::make_shared<TlsRecordMaximum>(server_);
  auto server_expand = std::make_shared<TlsRecordPadder>(server_, 1);
  server_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit({server_max, server_expand})));
  server_expand->EnableDecryption();

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();

  server_->SendData(100);

  client_->ExpectReadWriteError();
  ExpectAlert(client_, kTlsAlertRecordOverflow);
  client_->ReadBytes(100);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, client_->error_code());

  // Consume the alert at the server.
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RECORD_OVERFLOW_ALERT);
}

TEST_P(TlsConnectGeneric, RecordSizeBadValues) {
  EnsureTlsSetup();
  EXPECT_EQ(SECFailure,
            SSL_OptionSet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, 63));
  EXPECT_EQ(SECFailure,
            SSL_OptionSet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, -1));
  EXPECT_EQ(SECFailure,
            SSL_OptionSet(server_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, 16386));
  Connect();
}

TEST_P(TlsConnectGeneric, RecordSizeGetValues) {
  EnsureTlsSetup();
  int v;
  EXPECT_EQ(SECSuccess,
            SSL_OptionGet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(16385, v);
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 300);
  EXPECT_EQ(SECSuccess,
            SSL_OptionGet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(300, v);
  Connect();
}

// The value of the extension is capped by the maximum version of the client.
TEST_P(TlsConnectGeneric, RecordSizeCapExtensionClient) {
  EnsureTlsSetup();
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 16385);
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_record_size_limit_xtn);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    capture->EnableDecryption();
  }
  Connect();

  uint64_t val = 0;
  EXPECT_TRUE(capture->extension().Read(0, 2, &val));
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
    EXPECT_EQ(16384U, val) << "Extension should be capped";
  } else {
    EXPECT_EQ(16385U, val);
  }
}

// The value of the extension is capped by the maximum version of the server.
TEST_P(TlsConnectGeneric, RecordSizeCapExtensionServer) {
  EnsureTlsSetup();
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 16385);
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_record_size_limit_xtn);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    capture->EnableDecryption();
  }
  Connect();

  uint64_t val = 0;
  EXPECT_TRUE(capture->extension().Read(0, 2, &val));
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
    EXPECT_EQ(16384U, val) << "Extension should be capped";
  } else {
    EXPECT_EQ(16385U, val);
  }
}

// Damage the client extension and the handshake fails, but the server
// doesn't generate a validation error.
TEST_P(TlsConnectGenericPre13, RecordSizeClientExtensionInvalid) {
  EnsureTlsSetup();
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0xf4, 0x1f};
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_record_size_limit_xtn,
                                      DataBuffer(v, sizeof(v)));
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
}

// Special handling for TLS 1.3, where the alert isn't read.
TEST_F(TlsConnectStreamTls13, RecordSizeClientExtensionInvalid) {
  EnsureTlsSetup();
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0xf4, 0x1f};
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_record_size_limit_xtn,
                                      DataBuffer(v, sizeof(v)));
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

TEST_P(TlsConnectGeneric, RecordSizeServerExtensionInvalid) {
  EnsureTlsSetup();
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0xf4, 0x1f};
  auto replace = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_record_size_limit_xtn, DataBuffer(v, sizeof(v)));
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    replace->EnableDecryption();
  }
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
}

TEST_P(TlsConnectGeneric, RecordSizeServerExtensionExtra) {
  EnsureTlsSetup();
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0x01, 0x00, 0x00};
  auto replace = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_record_size_limit_xtn, DataBuffer(v, sizeof(v)));
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    replace->EnableDecryption();
  }
  ConnectExpectAlert(client_, kTlsAlertDecodeError);
}

class RecordSizeDefaultsTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_EQ(SECSuccess,
              SSL_OptionGetDefault(SSL_RECORD_SIZE_LIMIT, &default_));
  }
  void TearDown() {
    // Make sure to restore the default value at the end.
    EXPECT_EQ(SECSuccess,
              SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, default_));
  }

 private:
  PRIntn default_ = 0;
};

TEST_F(RecordSizeDefaultsTest, RecordSizeBadValues) {
  EXPECT_EQ(SECFailure, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, 63));
  EXPECT_EQ(SECFailure, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, -1));
  EXPECT_EQ(SECFailure, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, 16386));
}

TEST_F(RecordSizeDefaultsTest, RecordSizeGetValue) {
  int v;
  EXPECT_EQ(SECSuccess, SSL_OptionGetDefault(SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(16385, v);
  EXPECT_EQ(SECSuccess, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, 3000));
  EXPECT_EQ(SECSuccess, SSL_OptionGetDefault(SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(3000, v);
}

}  // namespace nss_test
