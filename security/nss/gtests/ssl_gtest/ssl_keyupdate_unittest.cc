/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_F(TlsConnectTest, KeyUpdateClient) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  SendReceive(50);
  SendReceive(60);
  CheckEpochs(4, 3);
}

TEST_F(TlsConnectStreamTls13, KeyUpdateTooEarly_Client) {
  StartConnect();
  auto filter = MakeTlsFilter<TlsEncryptedHandshakeMessageReplacer>(
      server_, kTlsHandshakeFinished, kTlsHandshakeKeyUpdate);
  filter->EnableDecryption();

  client_->Handshake();
  server_->Handshake();
  ExpectAlert(client_, kTlsAlertUnexpectedMessage);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_KEY_UPDATE);
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

TEST_F(TlsConnectStreamTls13, KeyUpdateTooEarly_Server) {
  StartConnect();
  auto filter = MakeTlsFilter<TlsEncryptedHandshakeMessageReplacer>(
      client_, kTlsHandshakeFinished, kTlsHandshakeKeyUpdate);
  filter->EnableDecryption();

  client_->Handshake();
  server_->Handshake();
  client_->Handshake();
  ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_KEY_UPDATE);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

TEST_F(TlsConnectTest, KeyUpdateClientRequestUpdate) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_TRUE));
  // SendReceive() only gives each peer one chance to read.  This isn't enough
  // when the read on one side generates another handshake message.  A second
  // read gives each peer an extra chance to consume the KeyUpdate.
  SendReceive(50);
  SendReceive(60);  // Cumulative count.
  CheckEpochs(4, 4);
}

TEST_F(TlsConnectTest, KeyUpdateServer) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));
  SendReceive(50);
  SendReceive(60);
  CheckEpochs(3, 4);
}

TEST_F(TlsConnectTest, KeyUpdateServerRequestUpdate) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  SendReceive(50);
  SendReceive(60);
  CheckEpochs(4, 4);
}

TEST_F(TlsConnectTest, KeyUpdateConsecutiveRequests) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  SendReceive(50);
  SendReceive(60);
  // The server should have updated twice, but the client should have declined
  // to respond to the second request from the server, since it doesn't send
  // anything in between those two requests.
  CheckEpochs(4, 5);
}

// Check that a local update can be immediately followed by a remotely triggered
// update even if there is no use of the keys.
TEST_F(TlsConnectTest, KeyUpdateLocalUpdateThenConsecutiveRequests) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  // This should trigger an update on the client.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // The client should update for the first request.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  // ...but not the second.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  SendReceive(50);
  SendReceive(60);
  // Both should have updated twice.
  CheckEpochs(5, 5);
}

TEST_F(TlsConnectTest, KeyUpdateMultiple) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  SendReceive(50);
  SendReceive(60);
  CheckEpochs(5, 6);
}

// Both ask the other for an update, and both should react.
TEST_F(TlsConnectTest, KeyUpdateBothRequest) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  Connect();
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  SendReceive(50);
  SendReceive(60);
  CheckEpochs(5, 5);
}

// If the sequence number exceeds the number of writes before an automatic
// update (currently 3/4 of the max records for the cipher suite), then the
// stack should send an update automatically (but not request one).
TEST_F(TlsConnectTest, KeyUpdateAutomaticOnWrite) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithCipherSuite(TLS_AES_128_GCM_SHA256);

  // Set this to one below the write threshold.
  uint64_t threshold = (0x5aULL << 28) * 3 / 4;
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), threshold));
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), threshold));

  // This should be OK.
  client_->SendData(10);
  server_->ReadBytes();

  // This should cause the client to update.
  client_->SendData(20);
  server_->ReadBytes();

  SendReceive(100);
  CheckEpochs(4, 3);
}

// If the sequence number exceeds a certain number of reads (currently 7/8 of
// the max records for the cipher suite), then the stack should send AND request
// an update automatically.  However, the sender (client) will be above its
// automatic update threshold, so the KeyUpdate - that it sends with the old
// cipher spec - will exceed the receiver (server) automatic update threshold.
// The receiver gets a packet with a sequence number over its automatic read
// update threshold.  Even though the sender has updated, the code that checks
// the sequence numbers at the receiver doesn't know this and it will request an
// update.  This causes two updates: one from the sender (without requesting a
// response) and one from the receiver (which does request a response).
TEST_F(TlsConnectTest, KeyUpdateAutomaticOnRead) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithCipherSuite(TLS_AES_128_GCM_SHA256);

  // Move to right at the read threshold.  Unlike the write test, we can't send
  // packets because that would cause the client to update, which would spoil
  // the test.
  uint64_t threshold = ((0x5aULL << 28) * 7 / 8) + 1;
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), threshold));
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), threshold));

  // This should cause the client to update, but not early enough to prevent the
  // server from updating also.
  client_->SendData(10);
  server_->ReadBytes();

  // Need two SendReceive() calls to ensure that the update that the server
  // requested is properly generated and consumed.
  SendReceive(70);
  SendReceive(80);
  CheckEpochs(5, 4);
}

// Filter to modify KeyUpdate message. Takes as an input which byte and what
// value to install.
class TLSKeyUpdateDamager : public TlsRecordFilter {
 public:
  TLSKeyUpdateDamager(const std::shared_ptr<TlsAgent>& a, size_t byte,
                      uint8_t val)
      : TlsRecordFilter(a), offset_(byte), value_(val) {}

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

    if (inner_content_type != ssl_ct_handshake) {
      return KEEP;
    }

    if (plaintext.data()[0] != ssl_hs_key_update) {
      return KEEP;
    }

    if (offset_ >= plaintext.len()) {
      ADD_FAILURE() << "TLSKeyUpdateDamager: the input (offset_) is out "
                       "of the range (the expected len is equal to "
                    << plaintext.len() << ")." << std::endl;
      return KEEP;
    }

    plaintext.data()[offset_] = value_;
    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintext, &ciphertext, &out_header);
    if (!ok) {
      ADD_FAILURE() << "Unable to protect the plaintext using  "
                    << protection_epoch << "epoch. " << std::endl;
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 protected:
  size_t offset_;
  uint8_t value_;
};

// The next tests check the behaviour in case of malformed KeyUpdate.
// The first test, TLSKeyUpdateWrongValueForUpdateRequested,
// modifies the 4th byte (KeyUpdate) to have the incorrect value.
// The last tests check the incorrect values of the length.

// RFC 8446: 4.  Handshake Protocol
//    struct {
//          HandshakeType msg_type;     handshake type
//          uint24 length;              remaining bytes in message
//          select (Handshake.msg_type) {
//              case key_update:            KeyUpdate; (4th byte)
//          };
//      } Handshake;

TEST_F(TlsConnectStreamTls13, TLSKeyUpdateWrongValueForUpdateRequested) {
  EnsureTlsSetup();
  // This test is setting the update_requested to be equal to 2
  // Whereas the allowed values are [0, 1].
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 4, 2);
  filter->EnableDecryption();
  filter->Disable();
  Connect();

  filter->Enable();
  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();

  ExpectAlert(server_, kTlsAlertDecodeError);
  client_->ExpectReceiveAlert(kTlsAlertDecodeError);

  server_->ExpectReadWriteError();
  client_->ExpectReadWriteError();
  server_->ReadBytes();
  client_->ReadBytes();

  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_KEY_UPDATE);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);

  // Even if the client has updated his writing key,
  client_->CheckEpochs(3, 4);
  // the server has not.
  server_->CheckEpochs(3, 3);
}

TEST_F(TlsConnectStreamTls13, TLSKeyUpdateWrongValueForLength_MessageTooLong) {
  EnsureTlsSetup();
  // the first byte of the length was replaced with 0xff.
  // The message now is too long.
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 1, 0xff);
  filter->EnableDecryption();
  filter->Disable();
  Connect();

  filter->Enable();
  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();

  ExpectAlert(server_, kTlsAlertDecodeError);
  client_->ExpectReceiveAlert(kTlsAlertDecodeError);

  server_->ExpectReadWriteError();
  client_->ExpectReadWriteError();
  server_->ReadBytes();
  client_->ReadBytes();

  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_HANDSHAKE);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);

  // Even if the client has updated his writing key,
  client_->CheckEpochs(3, 4);
  // the server has not.
  server_->CheckEpochs(3, 3);
}

TEST_F(TlsConnectStreamTls13, TLSKeyUpdateWrongValueForLength_MessageTooShort) {
  EnsureTlsSetup();
  // Changing the value of length of the KU message to be shorter than the
  // correct one.
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 0x3, 0x00);
  filter->EnableDecryption();
  filter->Disable();
  Connect();

  filter->Enable();
  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();

  ExpectAlert(server_, kTlsAlertDecodeError);
  client_->ExpectReceiveAlert(kTlsAlertCloseNotify);

  client_->SendData(10);
  server_->ReadBytes();
}

// DTLS1.3 tests

// The KeyUpdate in DTLS1.3 workflow (with the update_requested set):

// Client(P1) is asking for KeyUpdate
// Here the second parameter states whether the P1 requires update_requested
// (RFC9147, Section 8).
// EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(),
// PR_FALSE));

// The server (P2) receives the KeyUpdate request and processes it.
// server_->ReadBytes();

// P2 sends ACK.
// SSLInt_SendImmediateACK(server_->ssl_fd());

// P1 receives ACK and finished the KeyUpdate:
// client_->ReadBytes();

// This function sends and proceeds KeyUpdate explained above (assuming
// updateRequested == PR_FALSE) For the explantation of the updateRequested look
// at the test DTLSKeyUpdateClientUpdateRequestedSucceed.*/
static void SendAndProcessKU(const std::shared_ptr<TlsAgent>& sender,
                             const std::shared_ptr<TlsAgent>& receiver,
                             bool updateRequested) {
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(sender->ssl_fd(), updateRequested));
  receiver->ReadBytes();
  // It takes some time to send an ack message, so here we send it immediately
  SSLInt_SendImmediateACK(receiver->ssl_fd());
  sender->ReadBytes();
  if (updateRequested) {
    SSLInt_SendImmediateACK(sender->ssl_fd());
    receiver->ReadBytes();
  }
}

// This test checks that after the execution of KeyUpdate started by the client,
// the writing client/reading server key epoch was incremented.
// RFC 9147. Section 4.
// However, this value is set [...] of the connection epoch,
// which is an [...] counter incremented on every KeyUpdate.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientKUSucceed) {
  Connect();
  CheckEpochs(3, 3);
  //  Client starts KeyUpdate
  //  The updateRequested is not requested.
  SendAndProcessKU(client_, server_, PR_FALSE);
  //  The KeyUpdate is finished, and the client writing spec/the server reading
  //  spec is incremented.
  CheckEpochs(4, 3);
  //  Check that we can send/receive data after KeyUpdate.
  SendReceive(50);
}

// This test checks that only one KeyUpdate is possible at the same time.
// RFC 9147 Section 5.8.4
// In contrast, implementations MUST NOT send KeyUpdate, NewConnectionId, or
// RequestConnectionId messages if an earlier message of the same type has not
// yet been acknowledged.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientKUTwiceOnceIgnored) {
  Connect();
  CheckEpochs(3, 3);
  //  Client sends a key update message.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // The second key update message will be ignored as there is KeyUpdate in
  //  progress.
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  //  For the workflow see ssl_KeyUpdate_unittest.cc:SendAndProcessKU.
  server_->ReadBytes();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  //  As only one KeyUpdate was executed, the key epoch was incremented only
  //  once.
  CheckEpochs(4, 3);
  SendReceive(50);
}

// This test checks the same as the test DTLSKeyUpdateClientKeyUpdateSucceed,
// except that the server sends KeyUpdate.
TEST_F(TlsConnectDatagram13, DTLSKU_ServerKUSucceed) {
  Connect();
  CheckEpochs(3, 3);
  SendAndProcessKU(server_, client_, PR_FALSE);
  CheckEpochs(3, 4);
  SendReceive(50);
}

// This test checks the same as the test
// DTLSKeyUpdateClientKeyUpdateTwiceOnceIgnored, except that the server sends
// KeyUpdate.
TEST_F(TlsConnectDatagram13, DTLSKU_PreviousKUNotYetACKed) {
  Connect();
  CheckEpochs(3, 3);
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));
  // The second key update message will be ignored
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));

  client_->ReadBytes();
  SSLInt_SendImmediateACK(client_->ssl_fd());
  server_->ReadBytes();

  CheckEpochs(3, 4);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks that if we receive two KeyUpdates, one will be ignored
TEST_F(TlsConnectDatagram13, DTLSKU_TwiceReceivedOnceIgnored) {
  Connect();
  CheckEpochs(3, 3);
  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(server_);
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));

  // Here we check that there was no KeyUpdate happened
  client_->ReadBytes();
  SSLInt_SendImmediateACK(client_->ssl_fd());
  server_->ReadBytes();
  CheckEpochs(3, 3);

  DataBuffer d = filter->ReturnRecorded();
  // Sending the recorded KeyUpdate
  server_->SendDirect(d);
  // Sending the KeyUpdate again
  server_->SendDirect(d);

  client_->ReadBytes();
  SSLInt_SendImmediateACK(client_->ssl_fd());
  server_->ReadBytes();

  // We observe that only one KeyUpdate has happened
  CheckEpochs(3, 4);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// The KeyUpdate in DTLS1.3 workflow (with the update_requested set):

// Client(P1) is asking for KeyUpdate
// EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_TRUE));

// The server (P2) receives and processes the KeyUpdate request
// At the same time, P2 sends its own KeyUpdate request (due to update_requested
// was set)
// server_->ReadBytes();

// P1 receives the ACK and finalizes the KeyUpdate.
// SSLInt_SendImmediateACK(server_->ssl_fd());

// P1 receives the KeyUpdate request and processes it.
// client_->ReadBytes();

// P2 receives the ACK and finalizes the KeyUpdate.
// SSLInt_SendImmediateACK(client_->ssl_fd());
// server_->ReadBytes();

// This test checks that after the KeyUpdate (with update requested set)
// both client w/r and server w/r key epochs were incremented.
TEST_F(TlsConnectDatagram13, DTLSKU_UpdateRequestedSucceed) {
  Connect();
  CheckEpochs(3, 3);
  // Here the second parameter sets the update_requested to true.
  SendAndProcessKU(client_, server_, PR_TRUE);
  // As there were two KeyUpdates executed (one by a client, another one by a
  // server) Both of the keys were modified.
  CheckEpochs(4, 4);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks that after two KeyUpdates (with update requested set)
// the keys epochs were incremented twice.
TEST_F(TlsConnectDatagram13, DTLSKU_UpdateRequestedTwiceSucceed) {
  Connect();
  CheckEpochs(3, 3);
  SendAndProcessKU(client_, server_, PR_TRUE);
  // The KeyUpdate is finished, so both of the epochs got incremented.
  CheckEpochs(4, 4);
  SendAndProcessKU(client_, server_, PR_TRUE);
  // The second KeyUpdate is finished, so finally the epochs were incremented
  // twice.
  CheckEpochs(5, 5);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks the same as the test DTLSKeyUpdateUpdateRequestedSucceed,
// except that the server sends KeyUpdate.
TEST_F(TlsConnectDatagram13, DTLSKU_ServerUpdateRequestedSucceed) {
  Connect();
  CheckEpochs(3, 3);
  SendAndProcessKU(server_, client_, PR_TRUE);
  CheckEpochs(4, 4);
  SendReceive(50);
}

// This test checks that after two KeyUpdates (with update requested set)
// the keys epochs were incremented twice.
TEST_F(TlsConnectDatagram13, DTLSKU_ServerUpdateRequestedTwiceSucceed) {
  Connect();
  CheckEpochs(3, 3);
  SendAndProcessKU(server_, client_, PR_TRUE);
  // The KeyUpdate is finished, so both of the epochs got incremented.
  CheckEpochs(4, 4);

  // Server sends another KeyUpdate
  SendAndProcessKU(server_, client_, PR_TRUE);
  // The second KeyUpdate is finished, so finally the epochs were incremented
  // twice.
  CheckEpochs(5, 5);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks that both client and server can send the KeyUpdate in
// consequence.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientServerConseqSucceed) {
  Connect();
  CheckEpochs(3, 3);
  SendAndProcessKU(client_, server_, PR_FALSE);
  // As the server initiated KeyUpdate and did not request an update_request,
  // Only the server writing/client reading key epoch was incremented.
  CheckEpochs(4, 3);
  SendAndProcessKU(server_, client_, PR_FALSE);
  // Now the client initiated KeyUpdate and did not request an update_request,
  // so now both of epochs got incremented.
  CheckEpochs(4, 4);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks that both client and server can send the KeyUpdate in
// consequence. Compared to the DTLSKeyUpdateClientServerConseqSucceed TV, this
// time both parties set update_requested to be true.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientServerUpdateRequestedBothSucceed) {
  Connect();
  CheckEpochs(3, 3);
  SendAndProcessKU(client_, server_, PR_TRUE);
  SendAndProcessKU(server_, client_, PR_TRUE);
  // The second KeyUpdate (update_request = True) increments again the epochs
  // of both keys.
  CheckEpochs(5, 5);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks that if there is an ongoing KeyUpdate, the one started
// durint the KU is not going to be executed.
TEST_F(TlsConnectDatagram13, DTLSKU_KUInTheMiddleIsRejected) {
  Connect();
  CheckEpochs(3, 3);
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  client_->ReadBytes();
  SSLInt_SendImmediateACK(client_->ssl_fd());
  // Here a client starts KeyUpdate at the same time as the ongoing KeyUpdate
  // This KeyUpdate will not execute
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_TRUE));
  server_->ReadBytes();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  // As there was only one KeyUpdate executed, both keys got incremented only
  // once.
  CheckEpochs(4, 4);
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// DTLS1.3 KeyUpdate - Immediate Send Tests.

// The expected behaviour of the protocol:
// P1 starts initiates KeyUpdate
// P2 receives KeyUpdate
// And this moment, P2 will update the reading key to n
// But P2 will be accepting the keys from the previous epoch until a new message
// encrypted with the epoch n arrives.

// This test checks that when a client sent KeyUpdate, but the KeyUpdate message
// was not yet received, client can still send data.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientImmediateSend) {
  Connect();
  // Client has initiated KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // Server has not yet received it, client is trying to send some additional
  // data.
  CheckEpochs(3, 3);
  client_->SendData(10);
  // Server successfully receives it.
  WAIT_(server_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, server_->received_bytes());
  SendReceive(50);
}

// This test checks that when a client sent KeyUpdate, but the KeyUpdate message
// was not yet received, it can still receive data.
TEST_F(TlsConnectDatagram13, DTLSKU_ServerImmediateSend) {
  Connect();
  // Client has initiated KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // The server can successfully send data.
  CheckEpochs(3, 3);
  server_->SendData(10);
  WAIT_(client_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, client_->received_bytes());
  SendReceive(50);
}

// This test checks that when a client sent KeyUpdate,
// the server has not yet sent an ACK and the client has not yet ACKed
// KeyUpdate, both parties can exchange data.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientImmediateSendAfterServerRead) {
  Connect();
  // Client has initiated KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // Server receives KeyUpdate
  server_->ReadBytes();
  // Client can send data before the server sending ACK and client receiving
  // * ACK messages
  // Only server keys got updated.
  server_->CheckEpochs(4, 3);
  client_->CheckEpochs(3, 3);
  client_->SendData(10);
  WAIT_(server_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, server_->received_bytes());
  // Server can send data
  server_->SendData(10);
  WAIT_(client_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, client_->received_bytes());
  SendReceive(50);
}

// This test checks that when a client sent KeyUpdate, but has not yet ACKed it,
// both parties can exchange data.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientImmediateSendAfterServerReadAndACK) {
  Connect();
  CheckEpochs(3, 3);
  // Client has initiated KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // Server receives KeyUpdate
  server_->ReadBytes();
  // Server sends ACK
  SSLInt_SendImmediateACK(server_->ssl_fd());
  // Client can send data before he has received KeyUpdate
  // Only server keys got updated.
  server_->CheckEpochs(4, 3);
  client_->CheckEpochs(3, 3);
  client_->SendData(10);
  WAIT_(server_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, server_->received_bytes());
  // Server can send data
  server_->SendData(10);
  WAIT_(client_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, client_->received_bytes());
  SendReceive(50);
}

// This test checks that the client writing epoch is updated only
// when the client has received the ACK.
// RFC 9147. Section 8
// As with other handshake messages with no built-in response, KeyUpdates MUST
// be acknowledged.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientWritingEpochUpdatedAfterReceivedACK) {
  Connect();
  // Previous epoch
  CheckEpochs(3, 3);
  // Client sends a KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // Server updates his reading key
  server_->ReadBytes();
  // Now the server has a reading key = 4
  server_->CheckEpochs(4, 3);
  // But the client has a writing key = 3
  client_->CheckEpochs(3, 3);

  // Client sends a data, but using the old (3) keys
  client_->SendData(10);
  WAIT_(server_->received_bytes() == 10, 2000);
  ASSERT_EQ((size_t)10, server_->received_bytes());

  server_->CheckEpochs(4, 3);
  client_->CheckEpochs(3, 3);

  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();

  CheckEpochs(4, 3);
  SendReceive(50);
}

// DTLS1.3 KeyUpdate - Testing the border conditions
// (i.e. the cases where we reached the highest epoch).

// This test checks that the maximum epoch will not be exceeded on KeyUpdate.
// RFC 9147. Section 8.
// In order to provide an extra margin of security,
// sending implementations MUST NOT allow the epoch to exceed 2^48-1.

// Here we use the maximum as 2^16,
// See bug https://bugzilla.mozilla.org/show_bug.cgi?id=1809872
// When the bug is solved, the constant is to be replaced with 2^48 as
// required by RFC.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientMaxEpochReached) {
  Connect();
  CheckEpochs(3, 3);
  PRUint64 max_epoch_type = (0x1ULL << 16) - 1;

  // We assign the maximum possible epochs
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteEpochNum(client_->ssl_fd(), max_epoch_type));
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceReadEpochNum(server_->ssl_fd(), max_epoch_type));
  CheckEpochs(max_epoch_type, 3);
  // Upon trying to execute KeyUpdate, we return a SECFailure.
  EXPECT_EQ(SECFailure, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  SendReceive(50);
}

// This test checks the compliance with the RFC 9147 stating the behaviour
// reaching the max epoch: RFC 9147 Section 8. If a sending implementation
// receives a KeyUpdate with request_update set to "update_requested", it MUST
// NOT send its own KeyUpdate if that would cause it to exceed these limits and
// SHOULD instead ignore the "update_requested" flag.
TEST_F(TlsConnectDatagram13, DTLSKU_ClientMaxEpochReachedUpdateRequested) {
  Connect();
  CheckEpochs(3, 3);

  PRUint64 max_epoch_type = (0x1ULL << 16) - 1;

  // We assign the maximum possible epochs - 1.
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteEpochNum(client_->ssl_fd(), max_epoch_type));
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceReadEpochNum(server_->ssl_fd(), max_epoch_type));

  CheckEpochs(max_epoch_type, 3);
  // Once we call KeyUpdate with update requested
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(server_->ssl_fd(), PR_TRUE));
  client_->ReadBytes();
  SSLInt_SendImmediateACK(client_->ssl_fd());
  server_->ReadBytes();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  // Only one key (that has not reached the maximum epoch) was updated.
  CheckEpochs(max_epoch_type, 4);
  SendReceive(50);
}

// DTLS1.3 KeyUpdate - Automatic update tests

// RFC 9147 Section 4.5.3.
// Implementations SHOULD NOT protect more records than allowed by the limit
// specified for the negotiated AEAD.
// Implementations SHOULD initiate a key update before reaching this limit.

// These two tests check that the KeyUpdate is automatically called upon
// reaching the reading/writing limit.
TEST_F(TlsConnectDatagram13, DTLSKU_AutomaticOnWrite) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithCipherSuite(TLS_AES_128_GCM_SHA256);
  CheckEpochs(3, 3);

  // Set this to one below the write threshold.
  uint64_t threshold = 0x438000000;
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), threshold));
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), threshold));

  // This should be OK.
  client_->SendData(10);
  server_->ReadBytes();

  // This should cause the client to update.
  client_->SendData(15);
  server_->ReadBytes();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();

  // The client key epoch was incremented.
  CheckEpochs(4, 3);
  // Checking that we still can send/receive data.
  SendReceive(100);
}

TEST_F(TlsConnectDatagram13, DTLSKU_AutomaticOnRead) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithCipherSuite(TLS_AES_128_GCM_SHA256);
  CheckEpochs(3, 3);

  // Set this to one below the read threshold.
  uint64_t threshold = 0x4ec000000 - 1;
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), threshold));
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), threshold));

  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(client_);
  client_->SendData(10);
  DataBuffer d = filter->ReturnRecorded();

  client_->SendDirect(d);
  // This message will cause the server to start KeyUpdate with updateRequested
  // = 1.
  server_->ReadBytes();

  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  SSLInt_SendImmediateACK(client_->ssl_fd());
  server_->ReadBytes();

  // Both keys got updated.
  CheckEpochs(4, 4);
  // Checking that we still can send/receive data.
  SendReceive(100);
}

// The test describes the situation when there was a request
// to execute an automatic KU, but the server has not responded.
TEST_F(TlsConnectDatagram13, DTLSKU_CanSendBeforeThreshold) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithCipherSuite(TLS_AES_128_GCM_SHA256);
  CheckEpochs(3, 3);

  uint64_t threshold = 0x5a0000000 - 2;
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), threshold));
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), threshold));

  size_t received_bytes = server_->received_bytes();
  // We still can send a message
  client_->SendData(15);

  // We can not send a message anymore
  client_->ExpectReadWriteError();
  client_->SendData(105);

  server_->ReadBytes();
  // And it was not received.
  ASSERT_EQ((size_t)received_bytes + 15, server_->received_bytes());
}

TEST_F(TlsConnectDatagram13, DTLSKU_DiscardAfterThreshold) {
  ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  ConnectWithCipherSuite(TLS_AES_128_GCM_SHA256);
  CheckEpochs(3, 3);

  uint64_t threshold = 0x5a0000000 - 3;
  EXPECT_EQ(SECSuccess,
            SSLInt_AdvanceWriteSeqNum(client_->ssl_fd(), threshold));
  EXPECT_EQ(SECSuccess, SSLInt_AdvanceReadSeqNum(server_->ssl_fd(), threshold));

  size_t received_bytes = server_->received_bytes();

  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(client_);
  client_->SendData(30);
  DataBuffer d = filter->ReturnRecorded();

  client_->SendDirect(d);
  client_->SendDirect(d);

  server_->ReadBytes();
  // Only one message was received.
  ASSERT_EQ((size_t)received_bytes + 30, server_->received_bytes());
}

// DTLS1.3 KeyUpdate - Managing previous epoch messages
// RFC 9147 Section 8.
// Due to the possibility of an ACK message for a KeyUpdate being lost
// and thereby preventing the sender of the KeyUpdate from updating its
// keying material, receivers MUST retain the pre-update keying material
// until receipt and successful decryption of a message using the new
// keys.

// This test checks that message encrypted with the key n-1 will be accepted
// after KeyUpdate is executed, but before the message n has arrived.
TEST_F(TlsConnectDatagram13, DTLSKU_PreviousEpochIsAcceptedBeforeNew) {
  size_t len = 10;

  Connect();
  // Client starts KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // Server receives KeyUpdate and sends ACK
  server_->ReadBytes();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  // Client has not yet received the ACK, so the writing key epoch has not
  // changed
  client_->CheckEpochs(3, 3);
  server_->CheckEpochs(4, 3);

  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(client_);

  // Here the message previousEpochMessageBuffer contains a message
  // encrypted with the client 3rd epoch key, m1 = enc(message, key_3)
  client_->SendData(len);
  DataBuffer d = filter->ReturnRecorded();

  // Client has received the ACK
  client_->ReadBytes();
  // Now he updates the writing Key to 4
  client_->CheckEpochs(3, 4);
  server_->CheckEpochs(4, 3);

  // And now we resend the message m1 and successfully receive it
  client_->SendDirect(d);
  WAIT_(server_->received_bytes() == len, 2000);
  ASSERT_EQ(len, server_->received_bytes());
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// This test checks that message encrypted with the key n-2 will not be accepted
// after KeyUpdate is executed, but before the message n has arrived.
TEST_F(TlsConnectDatagram13, DTLSKU_2EpochsAgoIsRejected) {
  size_t len = 10;

  Connect();
  CheckEpochs(3, 3);
  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(client_);
  client_->SendData(len);
  DataBuffer d = filter->ReturnRecorded();
  client_->ResetSentBytes();

  SendAndProcessKU(client_, server_, PR_FALSE);
  SendAndProcessKU(client_, server_, PR_FALSE);

  // Executing 2 KeyUpdates, so the client writing key is equal to 5 now
  CheckEpochs(5, 3);
  // And now we resend the message m1 encrypted with the key n-2 (3)
  client_->SendDirect(d);
  server_->ReadBytes();
  // Server has still received just legal_message_len of bytes (not the
  // previousEpochLen + legal_message_len)
  ASSERT_EQ((size_t)0, server_->received_bytes());
  // Checking that we still can send/receive data.
  SendReceive(60);
}

// This test checks that that message encrypted with the key n-1 will be
// rejected after KeyUpdate is executed, and after the message n has arrived.
TEST_F(TlsConnectDatagram13, DTLSKU_PreviousEpochIsAcceptedAfterNew) {
  size_t len = 30;
  size_t legal_message_len = 20;

  Connect();
  // Client starts KeyUpdate
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  // Server receives KeyUpdate and sends ACK
  server_->ReadBytes();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  // Client has not yet received the ACK, so the writing key epoch has not
  // changed
  client_->CheckEpochs(3, 3);
  server_->CheckEpochs(4, 3);

  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(client_);

  // Here the message previousEpochMessageBuffer contains a message
  // encrypted with the client 3rd epoch key, m1 = enc(message, key_3)
  client_->SendData(len);
  DataBuffer d = filter->ReturnRecorded();
  client_->ResetSentBytes();

  // Client has received the ACK
  client_->ReadBytes();
  client_->CheckEpochs(3, 4);
  server_->CheckEpochs(4, 3);

  // At this moment, a client will send a message with the new key
  SendReceive(legal_message_len);
  // As soon as it's received, the server will forbid the messaged from the
  // previous epochs
  server_->ReadBytes();

  // If a message from the previous epoch arrives to the server (m1, the key_3
  // was used to encrypt it)
  client_->SendDirect(d);
  // it will be silently dropped
  server_->ReadBytes();
  //  Server has still received just legal_message_len of bytes (not the
  // previousEpochLen + legal_message_len)
  ASSERT_EQ((size_t)legal_message_len, server_->received_bytes());
  // Checking that we still can send/receive data.
  SendReceive(50);
}

// DTLS Epoch reconstruction test
// RFC 9147 Section 8. 4.2.2. Reconstructing the Sequence Number and Epoch

// This test checks that the epoch reconstruction is correct.
// The function under testing is dtlscon.c::dtls_ReadEpoch.
// We only consider the case when dtls_IsDtls13Ciphertext is true.

typedef struct sslKeyUpdateReadEpochTVStr {
  // The current epoch
  DTLSEpoch epoch;
  // Only two-bit epoch here
  PRUint8 header;
  DTLSEpoch expected_reconstructed_epoch;
} sslKeyUpdateReadEpochTV_t;

static const sslKeyUpdateReadEpochTV_t sslKeyUpdateReadEpochTV[26] = {
    {0x1, 0x1, 0x1},

    {0x2, 0x1, 0x1},
    {0x2, 0x2, 0x2},

    {0x3, 0x3, 0x3},
    {0x3, 0x2, 0x2},
    {0x3, 0x1, 0x1},

    {0x4, 0x0, 0x4},  // the difference (diff) between the reconstructed and
                      // the current epoch is equal to 0
    {0x4, 0x1, 0x1},  // diff == 3
    {0x4, 0x2, 0x2},  // diff == 2
    {0x4, 0x3, 0x3},  // diff == 1

    {0x5, 0x0, 0x4},  // diff == 1
    {0x5, 0x1, 0x5},  // diff == 0
    {0x5, 0x2, 0x2},  // diff == 3
    {0x5, 0x3, 0x3},  // diff == 2

    {0x6, 0x0, 0x4},
    {0x6, 0x1, 0x5},
    {0x6, 0x2, 0x6},
    {0x6, 0x3, 0x3},

    {0x7, 0x0, 0x4},
    {0x7, 0x1, 0x5},
    {0x7, 0x2, 0x6},
    {0x7, 0x3, 0x7},

    {0x8, 0x0, 0x8},
    {0x8, 0x1, 0x5},
    {0x8, 0x2, 0x6},
    {0x8, 0x3, 0x7},

    // Starting from here the pattern (starting from 4) repeats:
    // if a current epoch is equal to n,
    // the difference will behave as for n % 4 + 4.
    // For example, if the current epoch is equal to 9, then
    // the difference between the reconstructed epoch and the current one
    // will be the same as for the 5th epoch.
};

TEST_F(TlsConnectDatagram13, DTLS_EpochReconstruction) {
  PRUint8 header[5] = {0};
  header[0] = 0x20;
  DTLSEpoch epoch;

  for (size_t i = 0; i < 26; i++) {
    epoch = sslKeyUpdateReadEpochTV[i].epoch;
    header[0] = (header[0] & 0xfc) | (sslKeyUpdateReadEpochTV[i].header & 0x3);
    // ReadEpoch (dtlscon.c#1339) uses only spec->version and spec->epoch.
    ASSERT_EQ(sslKeyUpdateReadEpochTV[i].expected_reconstructed_epoch,
              dtls_ReadEpoch(SSL_LIBRARY_VERSION_TLS_1_3, epoch, header));
  }
}

// RFC 9147. A.2. Handshake Protocol
// struct {
//  HandshakeType msg_type;    -- handshake type
//  uint24 length;             -- bytes in message
//  uint16 message_seq;        -- DTLS-required field
//  uint24 fragment_offset;    -- DTLS-required field
//  uint24 fragment_length;    -- DTLS-required field
//  select (msg_type) {
//  ...
//  case key_update:            KeyUpdate;
//  } body;
// } Handshake;
//
// enum {
// update_not_requested(0), update_requested(1), (255)
// } KeyUpdateRequest;

// The next tests send malformed KeyUpdate messages.
// A remainder: TLSKeyUpdateDamager filter takes as an input an agent,
// a byte index and a value that the existing value of the byte with the byte
// index will be replaced with. The filter catchs only the KeyUpdate messages,
// keeping unchanged all the rest.

// The first test, DTLSKeyUpdateDamagerFilterTestingNoModification,
// checks the correctness of the filter itself. It replaces the value of 12th
// byte with 0: The 12th byte is used to specify KeyUpdateRequest. Thus, the
// modification done in the test will still result in the correct KeyUpdate
// request.

// The test DTLSKU_WrongValueForUpdateRequested is modifying
// KeyUpdateRequest byte to have an not-allowed value.

// The test DTLSKeyUpdateDamagedLength modifies the 3rd byte (one of the length
// bytes).

// The test DTLSKeyUpdateDamagedLengthLongMessage changes the length of the
// message as well.

// The test DTLSKeyUpdateDamagedFragmentLength modifies the 10th byte (one of
// the fragment_length bytes)

TEST_F(TlsConnectDatagram13, DTLSKU_WrongValueForUpdateRequested) {
  EnsureTlsSetup();
  // Filter replacing the update_requested with an unexpected value.
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 12, 2);
  filter->EnableDecryption();
  filter->Disable();
  Connect();
  filter->Enable();
  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();

  ExpectAlert(server_, kTlsAlertDecodeError);
  client_->ExpectReceiveAlert(kTlsAlertDecodeError);

  server_->ExpectReadWriteError();
  client_->ExpectReadWriteError();

  server_->ReadBytes();
  client_->ReadBytes();

  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_KEY_UPDATE);
  client_->CheckErrorCode(SSL_ERROR_DECODE_ERROR_ALERT);

  // No KeyUpdate happened.
  CheckEpochs(3, 3);
}

TEST_F(TlsConnectDatagram13, DTLSKU_DamagedLength) {
  EnsureTlsSetup();
  // Filter replacing the length value with 0.
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 3, 0);
  filter->EnableDecryption();
  filter->Disable();
  Connect();
  filter->Enable();

  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  // No KeyUpdate happened.
  CheckEpochs(3, 3);
  SendReceive(50);
}

TEST_F(TlsConnectDatagram13, DTLSKU_DamagedLengthTooLong) {
  EnsureTlsSetup();
  // Filter replacing the second byte of length with one
  // The message length is increased by 2 ^ 8
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 2, 2);
  filter->EnableDecryption();
  filter->Disable();
  Connect();
  filter->Enable();

  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  // No KeyUpdate happened.
  CheckEpochs(3, 3);
  SendReceive(50);
}

TEST_F(TlsConnectDatagram13, DTLSKU_DamagedFragmentLength) {
  EnsureTlsSetup();
  // Filter replacing the fragment length with 1.
  auto filter = MakeTlsFilter<TLSKeyUpdateDamager>(client_, 10, 1);
  filter->EnableDecryption();
  filter->Disable();
  Connect();
  filter->Enable();

  SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE);
  filter->Disable();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  client_->ReadBytes();
  // No KeyUpdate happened.
  CheckEpochs(3, 3);
  SendReceive(50);
}

// This filter is used in order to modify an ACK message.
// As it's possible that one record contains several ACKs,
// we fault all of them.

class TLSACKDamager : public TlsRecordFilter {
 public:
  TLSACKDamager(const std::shared_ptr<TlsAgent>& a, size_t byte, uint8_t val)
      : TlsRecordFilter(a), offset_(byte), value_(val) {}

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

    if (plaintext.data() == NULL || plaintext.len() == 0) {
      return KEEP;
    }

    if (decrypting() && inner_content_type != ssl_ct_ack) {
      return KEEP;
    }

    // We compute the number of ACKS in the message
    // As we keep processing the ACK even if one message is incorrent,
    // we fault all the found ACKs.

    uint8_t ack_message_header_len = 2;
    uint8_t ack_message_len_one_ACK = 16;
    uint64_t acks = plaintext.len() - ack_message_header_len;
    EXPECT_EQ((uint64_t)0, acks % ack_message_len_one_ACK);
    acks = acks / ack_message_len_one_ACK;

    if (plaintext.len() <= ack_message_header_len + offset_ +
                               (acks - 1) * ack_message_len_one_ACK) {
      return KEEP;
    }

    for (size_t i = 0; i < acks; i++) {
      // Here we replace the offset_-th byte after the header
      // i.e. headerAck + ACK(0) + ACK(1) <-- the offset_-th byte
      // of ACK(0), ACK(1), etc
      plaintext.data()[ack_message_header_len + offset_ +
                       i * ack_message_len_one_ACK] = value_;
    }

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintext, &ciphertext, &out_header);
    if (!ok) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 protected:
  size_t offset_;
  uint8_t value_;
};

// The next two tests are modifying the ACK message:

// First, we call KeyUpdate on the client side. The server successfully
// processes it, and it's sending an ACK message. At this moment, the filter
// modifies the content of the ACK message by changing the seqNum or epoch and
// sends it back to the client.
//
// struct {
//  uint64 epoch;
//  uint64 sequence_number;
// } RecordNumber;

// struct {
//  RecordNumber record_numbers<0..2^16-1>;
// } ACK;

TEST_F(TlsConnectDatagram13, DTLSKU_ModifACKEpoch) {
  EnsureTlsSetup();
  uint8_t byte = 3;
  uint8_t v = 1;
  // The filter will replace value-th byte of each ACK with one
  // The epoch will be more than v * 2 ^ ((byte - 1) * 8).
  // HandleACK function allows the epochs such that (epoch > RECORD_EPOCH_MAX)
  // where RECORD_EPOCH_MAX == ((0x1ULL << 16) - 1)
  auto filter = MakeTlsFilter<TLSACKDamager>(server_, byte, v);
  filter->EnableDecryption();
  filter->Disable();
  Connect();
  CheckEpochs(3, 3);
  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  server_->ReadBytes();

  filter->Enable();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  filter->Disable();

  client_->ReadBytes();
  server_->CheckEpochs(4, 3);
  // The client has not received the ACK, so it will not update the key.
  client_->CheckEpochs(3, 3);

  // The communication still continues.
  SendReceive(50);
}

TEST_F(TlsConnectDatagram13, DTLSKU_ModifACKSeqNum) {
  EnsureTlsSetup();
  uint8_t byte = 7;
  uint8_t v = 1;
  // The filter will replace value byte  of each ACK with one
  // The seqNum will be more than v * 2 ^ ((byte - 1) * 8).
  // HandleACK function allows the epochs such that (seq > RECORD_SEQ_MAX)
  // where RECORD_SEQ_MAX == ((0x1ULL << 48) - 1)

  // here byte + 8 means that we modify not epoch, but sequenceNum
  auto filter = MakeTlsFilter<TLSACKDamager>(server_, byte + 8, v);
  filter->EnableDecryption();
  filter->Disable();
  Connect();

  EXPECT_EQ(SECSuccess, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
  server_->ReadBytes();

  filter->Enable();
  SSLInt_SendImmediateACK(server_->ssl_fd());
  filter->Disable();

  client_->ReadBytes();

  client_->ReadBytes();
  server_->CheckEpochs(4, 3);
  // The client has not received the ACK, so it will not update the key.
  client_->CheckEpochs(3, 3);

  // The communication still continues.
  SendReceive(50);
}

TEST_F(TlsConnectDatagram13, DTLSKU_TooEarly_ClientCannotSendKeyUpdate) {
  StartConnect();
  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(server_);
  filter->EnableDecryption();

  client_->Handshake();
  server_->Handshake();

  EXPECT_EQ(SECFailure, SSL_KeyUpdate(client_->ssl_fd(), PR_FALSE));
}

TEST_F(TlsConnectDatagram13, DTLSKeyUpdateTooEarly_ServerCannotSendKeyUpdate) {
  StartConnect();
  auto filter = MakeTlsFilter<TLSRecordSaveAndDropNext>(server_);
  filter->EnableDecryption();

  client_->Handshake();
  server_->Handshake();

  EXPECT_EQ(SECFailure, SSL_KeyUpdate(server_->ssl_fd(), PR_FALSE));
}

class DTlsEncryptedHandshakeHeaderReplacer : public TlsRecordFilter {
 public:
  DTlsEncryptedHandshakeHeaderReplacer(const std::shared_ptr<TlsAgent>& a,
                                       uint8_t old_ct, uint8_t new_ct)
      : TlsRecordFilter(a),
        old_ct_(old_ct),
        new_ct_(new_ct),
        replaced_(false) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    if (replaced_) return KEEP;

    uint8_t inner_content_type;
    DataBuffer plaintext;
    uint16_t protection_epoch = 0;
    TlsRecordHeader out_header(header);

    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    auto& protection_spec = spec(protection_epoch);
    uint32_t msg_type = 256;  // Not a real message
    if (!plaintext.Read(0, 1, &msg_type) || msg_type == old_ct_) {
      replaced_ = true;
      plaintext.Write(0, new_ct_, 1);
    }

    uint64_t seq_num = protection_spec.next_out_seqno();
    if (out_header.is_dtls()) {
      seq_num |= out_header.sequence_number() & (0xffffULL << 48);
    }
    out_header.sequence_number(seq_num);

    DataBuffer ciphertext;
    bool rv = Protect(protection_spec, out_header, inner_content_type,
                      plaintext, &ciphertext, &out_header);
    if (!rv) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  uint8_t old_ct_;
  uint8_t new_ct_;
  bool replaced_;
};

// The next tests check the behaviour of KU before the handshake is finished.
TEST_F(TlsConnectDatagram13, DTLSKU_TooEarly_Client) {
  StartConnect();
  // This filter takes the record and if it finds kTlsHandshakeFinished
  // it replaces it with kTlsHandshakeKeyUpdate
  // Then, the KeyUpdate will be started when the handshake is not yet finished
  // This handshake will be cancelled.
  auto filter = MakeTlsFilter<DTlsEncryptedHandshakeHeaderReplacer>(
      server_, kTlsHandshakeFinished, kTlsHandshakeKeyUpdate);
  filter->EnableDecryption();

  client_->Handshake();
  server_->Handshake();
  ExpectAlert(client_, kTlsAlertUnexpectedMessage);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_KEY_UPDATE);
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

TEST_F(TlsConnectDatagram13, DTLSKU_TooEarly_Server) {
  StartConnect();
  // This filter takes the record and if it finds kTlsHandshakeFinished
  // it replaces it with kTlsHandshakeKeyUpdate
  auto filter = MakeTlsFilter<DTlsEncryptedHandshakeHeaderReplacer>(
      client_, kTlsHandshakeFinished, kTlsHandshakeKeyUpdate);
  filter->EnableDecryption();

  client_->Handshake();
  server_->Handshake();
  client_->Handshake();
  ExpectAlert(server_, kTlsAlertUnexpectedMessage);
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RX_UNEXPECTED_KEY_UPDATE);
  client_->Handshake();
  client_->CheckErrorCode(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
}

}  // namespace nss_test