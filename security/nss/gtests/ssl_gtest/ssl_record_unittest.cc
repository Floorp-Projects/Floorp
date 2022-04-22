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

/* This filter modifies any application data record, changes its inner content
 * type to the specified one (default handshake) and optionally add padding,
 * to create records/fragments invalid in TLS 1.3.
 *
 * Implementations MUST NOT send Handshake and Alert records that have a
 * zero-length TLSInnerPlaintext.content; if such a message is received,
 * the receiving implementation MUST terminate the connection with an
 * "unexpected_message" alert [RFC8446, Section 5.4].
 *
 * !!! TLS 1.3 ONLY !!! */
class ZeroLengthInnerPlaintextCreatorFilter : public TlsRecordFilter {
 public:
  ZeroLengthInnerPlaintextCreatorFilter(
      const std::shared_ptr<TlsAgent>& a,
      SSLContentType contentType = ssl_ct_handshake, size_t padding = 0)
      : TlsRecordFilter(a), contentType_(contentType), padding_(padding) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    if (!header.is_protected()) {
      return KEEP;
    }

    uint16_t protection_epoch;
    uint8_t inner_content_type;
    DataBuffer plaintext;
    TlsRecordHeader out_header;
    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    if (decrypting() && inner_content_type != ssl_ct_application_data) {
      return KEEP;
    }

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, contentType_,
                      DataBuffer(0), &ciphertext, &out_header, padding_);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }

    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  SSLContentType contentType_;
  size_t padding_;
};

/* Zero-length InnerPlaintext test class
 *
 * Parameter = Tuple of:
 * - TLS variant (datagram/stream)
 * - Content type to be set in zero-length inner plaintext record
 * - Padding of record plaintext
 */
class ZeroLengthInnerPlaintextSetupTls13
    : public TlsConnectTestBase,
      public testing::WithParamInterface<
          std::tuple<SSLProtocolVariant, SSLContentType, size_t>> {
 public:
  ZeroLengthInnerPlaintextSetupTls13()
      : TlsConnectTestBase(std::get<0>(GetParam()),
                           SSL_LIBRARY_VERSION_TLS_1_3),
        contentType_(std::get<1>(GetParam())),
        padding_(std::get<2>(GetParam())){};

 protected:
  SSLContentType contentType_;
  size_t padding_;
};

/* Test correct rejection of TLS 1.3 encrypted handshake/alert records with
 * zero-length inner plaintext content length with and without padding. */
TEST_P(ZeroLengthInnerPlaintextSetupTls13, ZeroLengthInnerPlaintextRun) {
  EnsureTlsSetup();

  // Filter modifies record to be zero-length
  auto filter = MakeTlsFilter<ZeroLengthInnerPlaintextCreatorFilter>(
      client_, contentType_, padding_);
  filter->EnableDecryption();
  filter->Disable();

  Connect();

  filter->Enable();

  // Record will be overwritten
  client_->SendData(0xf);

  // Receive corrupt record
  if (variant_ == ssl_variant_stream) {
    server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
    // 22B = 16B MAC + 1B innerContentType + 5B Header
    server_->ReadBytes(22);
    // Process alert at peer
    client_->ExpectReceiveAlert(kTlsAlertUnexpectedMessage);
    client_->Handshake();
  } else { /* DTLS */
    size_t received = server_->received_bytes();
    // 22B = 16B MAC + 1B innerContentType + 5B Header
    server_->ReadBytes(22);
    // Check that no bytes were received => packet was dropped
    ASSERT_EQ(received, server_->received_bytes());
    // Check that we are still connected / not in error state
    EXPECT_EQ(TlsAgent::STATE_CONNECTED, client_->state());
    EXPECT_EQ(TlsAgent::STATE_CONNECTED, server_->state());
  }
}

// Test for TLS and DTLS
const SSLProtocolVariant kZeroLengthInnerPlaintextVariants[] = {
    ssl_variant_stream, ssl_variant_datagram};
// Test for handshake and alert fragments
const SSLContentType kZeroLengthInnerPlaintextContentTypes[] = {
    ssl_ct_handshake, ssl_ct_alert};
// Test with 0,1 and 100 octets of padding
const size_t kZeroLengthInnerPlaintextPadding[] = {0, 1, 100};

INSTANTIATE_TEST_SUITE_P(
    ZeroLengthInnerPlaintextTest, ZeroLengthInnerPlaintextSetupTls13,
    testing::Combine(testing::ValuesIn(kZeroLengthInnerPlaintextVariants),
                     testing::ValuesIn(kZeroLengthInnerPlaintextContentTypes),
                     testing::ValuesIn(kZeroLengthInnerPlaintextPadding)),
    [](const testing::TestParamInfo<
        ZeroLengthInnerPlaintextSetupTls13::ParamType>& inf) {
      return std::string(std::get<0>(inf.param) == ssl_variant_stream
                             ? "Tls"
                             : "Dtls") +
             "ZeroLengthInnerPlaintext" +
             (std::get<1>(inf.param) == ssl_ct_handshake ? "Handshake"
                                                         : "Alert") +
             (std::get<2>(inf.param)
                  ? "Padding" + std::to_string(std::get<2>(inf.param)) + "B"
                  : "") +
             "Test";
    });

/* Zero-length record test class
 *
 * Parameter = Tuple of:
 * - TLS variant (datagram/stream)
 * - Content type to be set in zero-length record
 */
class ZeroLengthRecordSetup
    : public TlsConnectTestBase,
      public testing::WithParamInterface<
          std::tuple<SSLProtocolVariant, SSLContentType>> {
 public:
  ZeroLengthRecordSetup()
      : TlsConnectTestBase(std::get<0>(GetParam()), 0),
        variant_(std::get<0>(GetParam())),
        contentType_(std::get<1>(GetParam())){};

  void createZeroLengthRecord(DataBuffer& buffer, unsigned epoch = 0,
                              unsigned seqn = 0) {
    size_t idx = 0;
    // Set header content type
    idx = buffer.Write(idx, contentType_, 1);
    // The record version is not checked during record layer handling
    idx = buffer.Write(idx, 0xDEAD, 2);
    // DTLS (version always < TLS 1.3)
    if (variant_ == ssl_variant_datagram) {
      // Set epoch (Should be 0 before handshake)
      idx = buffer.Write(idx, 0U, 2);
      // Set 6B sequence number (0 if send as first message)
      idx = buffer.Write(idx, 0U, 2);
      idx = buffer.Write(idx, 0U, 4);
    }
    // Set fragment to be of zero-length
    (void)buffer.Write(idx, 0U, 2);
  }

 protected:
  SSLProtocolVariant variant_;
  SSLContentType contentType_;
};

/* Test handling of zero-length (ciphertext/fragment) records before handshake.
 *
 * This is only tested before the first handshake, since after it all of these
 * messages are expected to be encrypted which is impossible for a content
 * length of zero, always leading to a bad record mac. For TLS 1.3 only
 * records of application data content type is legal after the handshake.
 *
 * Handshake records of length zero will be ignored in the record layer since
 * the RFC does only specify that such records MUST NOT be sent but it does not
 * state that an alert should be sent or the connection be terminated
 * [RFC8446, Section 5.1].
 *
 * Even though only handshake messages are handled (ignored) in the record
 * layer handling, this test covers zero-length records of all content types
 * for complete coverage of cases.
 *
 * !!! Expected TLS (Stream) behavior !!!
 * - Handshake records of zero length are ignored.
 * - Alert and ChangeCipherSpec records of zero-length lead to illegal
 * parameter alerts due to the malformed record content.
 * - ApplicationData before the handshake leads to an unexpected message alert.
 *
 * !!! Expected DTLS (Datagram) behavior !!!
 * - Handshake message of zero length are ignored.
 * - Alert messages lead to an illegal parameter alert due to malformed record
 * content.
 * - ChangeCipherSpec records before the first handshake are not expected and
 * ignored (see ssl3con.c, line 3276).
 * - ApplicationData before the handshake is ignored since it could be a packet
 * received in incorrect order (see ssl3con.c, line 13353).
 */
TEST_P(ZeroLengthRecordSetup, ZeroLengthRecordRun) {
  DataBuffer buffer;
  createZeroLengthRecord(buffer);

  EnsureTlsSetup();
  // Send zero-length record
  client_->SendDirect(buffer);
  // This must be set, otherwise handshake completness assertions might fail
  server_->StartConnect();

  // DTLS ignores all cases but malformed alert
  if (contentType_ != ssl_ct_alert && variant_ == ssl_variant_datagram) {
    contentType_ = ssl_ct_handshake;
  }

  SSLAlertDescription alert;
  switch (contentType_) {
    case ssl_ct_alert:
    case ssl_ct_change_cipher_spec:
      alert = illegal_parameter;
      break;
    case ssl_ct_application_data:
      alert = unexpected_message;
      break;
    default:  // ssl_ct_handshake
      // Receive zero-length record
      server_->Handshake();
      // Connect successfully since empty record was ignored
      Connect();
      return;
  }

  // Assert alert is send for TLS and DTLS alert records
  server_->ExpectSendAlert(alert);
  server_->Handshake();

  // Consume alert at peer, expect alert for TLS and DTLS alert records
  client_->StartConnect();
  client_->ExpectReceiveAlert(alert);
  client_->Handshake();
}

// Test for TLS and DTLS
const SSLProtocolVariant kZeroLengthRecordVariants[] = {ssl_variant_datagram,
                                                        ssl_variant_stream};

// Test for handshake, alert, change_cipher_spec and application data fragments
const SSLContentType kZeroLengthRecordContentTypes[] = {
    ssl_ct_handshake, ssl_ct_alert, ssl_ct_change_cipher_spec,
    ssl_ct_application_data};

INSTANTIATE_TEST_SUITE_P(
    ZeroLengthRecordTest, ZeroLengthRecordSetup,
    testing::Combine(testing::ValuesIn(kZeroLengthRecordVariants),
                     testing::ValuesIn(kZeroLengthRecordContentTypes)),
    [](const testing::TestParamInfo<ZeroLengthRecordSetup::ParamType>& inf) {
      std::string variant =
          (std::get<0>(inf.param) == ssl_variant_stream) ? "Tls" : "Dtls";
      std::string contentType;
      switch (std::get<1>(inf.param)) {
        case ssl_ct_handshake:
          contentType = "Handshake";
          break;
        case ssl_ct_alert:
          contentType = "Alert";
          break;
        case ssl_ct_application_data:
          contentType = "ApplicationData";
          break;
        case ssl_ct_change_cipher_spec:
          contentType = "ChangeCipherSpec";
          break;
        default:
          contentType = "InvalidParameter";
      }
      return variant + "ZeroLength" + contentType + "Test";
    });

}  // namespace nss_test