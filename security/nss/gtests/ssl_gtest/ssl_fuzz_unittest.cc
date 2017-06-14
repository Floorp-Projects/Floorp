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
  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));
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
  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));
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
    client_->SetPacketFilter(std::make_shared<TlsConversationRecorder>(buffer));
    server_->SetPacketFilter(std::make_shared<TlsConversationRecorder>(buffer));

    // Reset the RNG state.
    EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));
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
  auto client_recorder = std::make_shared<TlsApplicationDataRecorder>();
  client_->SetPacketFilter(client_recorder);
  auto server_recorder = std::make_shared<TlsApplicationDataRecorder>();
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

  auto i1 = std::make_shared<TlsInspectorReplaceHandshakeMessage>(
      kTlsHandshakeFinished,
      DataBuffer(kShortEmptyFinished, sizeof(kShortEmptyFinished)));
  client_->SetPacketFilter(i1);
  Connect();
  SendReceive();
}

// Check that an invalid Finished message doesn't abort the connection.
FUZZ_P(TlsConnectGeneric, BogusServerFinished) {
  EnsureTlsSetup();

  auto i1 = std::make_shared<TlsInspectorReplaceHandshakeMessage>(
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
  server_->SetPacketFilter(std::make_shared<TlsLastByteDamager>(msg_type));
  Connect();
  SendReceive();
}

// Check that an invalid client auth signature doesn't abort the connection.
FUZZ_P(TlsConnectGeneric, BogusClientAuthSignature) {
  EnsureTlsSetup();
  client_->SetupClientAuth();
  server_->RequestClientAuth(true);
  client_->SetPacketFilter(
      std::make_shared<TlsLastByteDamager>(kTlsHandshakeCertificateVerify));
  Connect();
}

// Check that session ticket resumption works.
FUZZ_P(TlsConnectGeneric, SessionTicketResumption) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);
  Connect();
  SendReceive();
}

class TlsSessionTicketMacDamager : public TlsExtensionFilter {
 public:
  TlsSessionTicketMacDamager() {}
  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != ssl_session_ticket_xtn &&
        extension_type != ssl_tls13_pre_shared_key_xtn) {
      return KEEP;
    }

    *output = input;

    // Handle everything before TLS 1.3.
    if (extension_type == ssl_session_ticket_xtn) {
      // Modify the last byte of the MAC.
      output->data()[output->len() - 1] ^= 0xff;
    }

    // Handle TLS 1.3.
    if (extension_type == ssl_tls13_pre_shared_key_xtn) {
      TlsParser parser(input);

      uint32_t ids_len;
      EXPECT_TRUE(parser.Read(&ids_len, 2) && ids_len > 0);

      uint32_t ticket_len;
      EXPECT_TRUE(parser.Read(&ticket_len, 2) && ticket_len > 0);

      // Modify the last byte of the MAC.
      output->data()[2 + 2 + ticket_len - 1] ^= 0xff;
    }

    return CHANGE;
  }
};

// Check that session ticket resumption works with a bad MAC.
FUZZ_P(TlsConnectGeneric, SessionTicketResumptionBadMac) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);

  client_->SetPacketFilter(std::make_shared<TlsSessionTicketMacDamager>());
  Connect();
  SendReceive();
}

// Check that session tickets are not encrypted.
FUZZ_P(TlsConnectGeneric, UnencryptedSessionTickets) {
  ConfigureSessionCache(RESUME_TICKET, RESUME_TICKET);

  auto i1 = std::make_shared<TlsInspectorRecordHandshakeMessage>(
      kTlsHandshakeNewSessionTicket);
  server_->SetPacketFilter(i1);
  Connect();

  size_t offset = 4; /* lifetime */
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
    offset += 1 + 1 + /* ke_modes */
              1 + 1;  /* auth_modes */
  }
  offset += 2 + /* ticket length */
            2;  /* TLS_EX_SESS_TICKET_VERSION */
  // Check the protocol version number.
  uint32_t tls_version = 0;
  EXPECT_TRUE(i1->buffer().Read(offset, sizeof(version_), &tls_version));
  EXPECT_EQ(version_, static_cast<decltype(version_)>(tls_version));

  // Check the cipher suite.
  uint32_t suite = 0;
  EXPECT_TRUE(i1->buffer().Read(offset + sizeof(version_), 2, &suite));
  client_->CheckCipherSuite(static_cast<uint16_t>(suite));
}
}
