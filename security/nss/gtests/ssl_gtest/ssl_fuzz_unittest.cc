/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "ssl.h"
#include "sslimpl.h"
#include "tls_connect.h"

#include "gtest/gtest.h"

namespace nss_test {

#ifdef UNSAFE_FUZZER_MODE
#define FUZZ_F(c, f) TEST_F(c, Fuzz_##f)
#define FUZZ_P(c, f) TEST_P(c, Fuzz_##f)
#else
#define FUZZ_F(c, f) TEST_F(c, DISABLED_Fuzz_##f)
#define FUZZ_P(c, f) TEST_P(c, DISABLED_Fuzz_##f)
// RNG_ResetForFuzzing() isn't exported from the shared libraries, rather than
// fail to link to it, make it fail (we're not running it anyway).
#define RNG_ResetForFuzzing() SECFailure
#endif

const uint8_t kShortEmptyFinished[8] = {0};
const uint8_t kLongEmptyFinished[128] = {0};

class TlsFuzzTest : public ::testing::Test {};

// Record the application data stream.
class TlsApplicationDataRecorder : public TlsRecordFilter {
 public:
  TlsApplicationDataRecorder() : buffer_() {}

  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& input,
                                            DataBuffer* output) {
    if (header.content_type() == kTlsApplicationDataType) {
      buffer_.Append(input);
    }

    return KEEP;
  }

  const DataBuffer& buffer() const { return buffer_; }

 private:
  DataBuffer buffer_;
};

// Damages an SKE or CV signature.
class TlsSignatureDamager : public TlsHandshakeFilter {
 public:
  TlsSignatureDamager(uint8_t type) : type_(type) {}
  virtual PacketFilter::Action FilterHandshake(
      const TlsHandshakeFilter::HandshakeHeader& header,
      const DataBuffer& input, DataBuffer* output) {
    if (header.handshake_type() != type_) {
      return KEEP;
    }

    *output = input;

    // Modify the last byte of the signature.
    output->data()[output->len() - 1]++;
    return CHANGE;
  }

 private:
  uint8_t type_;
};

// Ensure that ssl_Time() returns a constant value.
FUZZ_F(TlsFuzzTest, SSL_Time_Constant) {
  PRUint32 now = ssl_Time();
  PR_Sleep(PR_SecondsToInterval(2));
  EXPECT_EQ(ssl_Time(), now);
}

// Check that due to the deterministic PRNG we derive
// the same master secret in two consecutive TLS sessions.
FUZZ_P(TlsConnectGeneric, DeterministicExporter) {
  const char kLabel[] = "label";
  std::vector<unsigned char> out1(32), out2(32);

  // Make sure we have RSA blinding params.
  Connect();

  Reset();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  DisableECDHEServerKeyReuse();

  // Reset the RNG state.
  EXPECT_EQ(SECSuccess, RNG_ResetForFuzzing());
  Connect();

  // Export a key derived from the MS and nonces.
  SECStatus rv =
      SSL_ExportKeyingMaterial(client_->ssl_fd(), kLabel, strlen(kLabel), false,
                               NULL, 0, out1.data(), out1.size());
  EXPECT_EQ(SECSuccess, rv);

  Reset();
  ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
  DisableECDHEServerKeyReuse();

  // Reset the RNG state.
  EXPECT_EQ(SECSuccess, RNG_ResetForFuzzing());
  Connect();

  // Export another key derived from the MS and nonces.
  rv = SSL_ExportKeyingMaterial(client_->ssl_fd(), kLabel, strlen(kLabel),
                                false, NULL, 0, out2.data(), out2.size());
  EXPECT_EQ(SECSuccess, rv);

  // The two exported keys should be the same.
  EXPECT_EQ(out1, out2);
}

// Check that due to the deterministic RNG two consecutive
// TLS sessions will have the exact same transcript.
FUZZ_P(TlsConnectGeneric, DeterministicTranscript) {
  // Make sure we have RSA blinding params.
  Connect();

  // Connect a few times and compare the transcripts byte-by-byte.
  DataBuffer last;
  for (size_t i = 0; i < 5; i++) {
    Reset();
    ConfigureSessionCache(RESUME_NONE, RESUME_NONE);
    DisableECDHEServerKeyReuse();

    DataBuffer buffer;
    client_->SetPacketFilter(new TlsConversationRecorder(buffer));
    server_->SetPacketFilter(new TlsConversationRecorder(buffer));

    // Reset the RNG state.
    EXPECT_EQ(SECSuccess, RNG_ResetForFuzzing());
    Connect();

    // Ensure the filters go away before |buffer| does.
    client_->DeletePacketFilter();
    server_->DeletePacketFilter();

    if (last.len() > 0) {
      EXPECT_EQ(last, buffer);
    }

    last = buffer;
  }
}

// Check that we can establish and use a connection
// with all supported TLS versions, STREAM and DGRAM.
// Check that records are NOT encrypted.
// Check that records don't have a MAC.
FUZZ_P(TlsConnectGeneric, ConnectSendReceive_NullCipher) {
  EnsureTlsSetup();

  // Set up app data filters.
  auto client_recorder = new TlsApplicationDataRecorder();
  client_->SetPacketFilter(client_recorder);
  auto server_recorder = new TlsApplicationDataRecorder();
  server_->SetPacketFilter(server_recorder);

  Connect();

  // Construct the plaintext.
  DataBuffer buf;
  buf.Allocate(50);
  for (size_t i = 0; i < buf.len(); ++i) {
    buf.data()[i] = i & 0xff;
  }

  // Send/Receive data.
  client_->SendBuffer(buf);
  server_->SendBuffer(buf);
  Receive(buf.len());

  // Check for plaintext on the wire.
  EXPECT_EQ(buf, client_recorder->buffer());
  EXPECT_EQ(buf, server_recorder->buffer());
}

// Check that an invalid Finished message doesn't abort the connection.
FUZZ_P(TlsConnectGeneric, BogusClientFinished) {
  EnsureTlsSetup();

  auto i1 = new TlsInspectorReplaceHandshakeMessage(
      kTlsHandshakeFinished,
      DataBuffer(kShortEmptyFinished, sizeof(kShortEmptyFinished)));
  client_->SetPacketFilter(i1);
  Connect();
  SendReceive();
}

// Check that an invalid Finished message doesn't abort the connection.
FUZZ_P(TlsConnectGeneric, BogusServerFinished) {
  EnsureTlsSetup();

  auto i1 = new TlsInspectorReplaceHandshakeMessage(
      kTlsHandshakeFinished,
      DataBuffer(kLongEmptyFinished, sizeof(kLongEmptyFinished)));
  server_->SetPacketFilter(i1);
  Connect();
  SendReceive();
}

// Check that an invalid server auth signature doesn't abort the connection.
FUZZ_P(TlsConnectGeneric, BogusServerAuthSignature) {
  EnsureTlsSetup();
  uint8_t msg_type = version_ == SSL_LIBRARY_VERSION_TLS_1_3
                         ? kTlsHandshakeCertificateVerify
                         : kTlsHandshakeServerKeyExchange;
  server_->SetPacketFilter(new TlsSignatureDamager(msg_type));
  Connect();
  SendReceive();
}

// Check that an invalid client auth signature doesn't abort the connection.
FUZZ_P(TlsConnectGeneric, BogusClientAuthSignature) {
  EnsureTlsSetup();
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  client_->SetPacketFilter(
      new TlsSignatureDamager(kTlsHandshakeCertificateVerify));
  Connect();
}
}
