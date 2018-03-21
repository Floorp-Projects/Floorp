/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

// This is internal, just to get TLS_1_3_DRAFT_VERSION.
#include "ssl3prot.h"

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsConnectTls13, HelloRetryRequestAbortsZeroRtt) {
  const char* k0RttData = "Such is life";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));

  SetupForZeroRtt();  // initial handshake as normal

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);
  ExpectResumption(RESUME_TICKET);

  // Send first ClientHello and send 0-RTT data
  auto capture_early_data =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_early_data_xtn);
  client_->Handshake();
  EXPECT_EQ(k0RttDataLen, PR_Write(client_->ssl_fd(), k0RttData,
                                   k0RttDataLen));  // 0-RTT write.
  EXPECT_TRUE(capture_early_data->captured());

  // Send the HelloRetryRequest
  auto hrr_capture = MakeTlsFilter<TlsHandshakeRecorder>(
      server_, kTlsHandshakeHelloRetryRequest);
  server_->Handshake();
  EXPECT_LT(0U, hrr_capture->buffer().len());

  // The server can't read
  std::vector<uint8_t> buf(k0RttDataLen);
  EXPECT_EQ(SECFailure, PR_Read(server_->ssl_fd(), buf.data(), k0RttDataLen));
  EXPECT_EQ(PR_WOULD_BLOCK_ERROR, PORT_GetError());

  // Make a new capture for the early data.
  capture_early_data =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_early_data_xtn);

  // Complete the handshake successfully
  Handshake();
  ExpectEarlyDataAccepted(false);  // The server should reject 0-RTT
  CheckConnected();
  SendReceive();
  EXPECT_FALSE(capture_early_data->captured());
}

// This filter only works for DTLS 1.3 where there is exactly one handshake
// packet. If the record is split into two packets, or there are multiple
// handshake packets, this will break.
class CorrectMessageSeqAfterHrrFilter : public TlsRecordFilter {
 public:
  CorrectMessageSeqAfterHrrFilter(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) {
    if (filtered_packets() > 0 || header.content_type() != content_handshake) {
      return KEEP;
    }

    DataBuffer buffer(record);
    TlsRecordHeader new_header(header.variant(), header.version(),
                               header.content_type(),
                               header.sequence_number() + 1);

    // Correct message_seq.
    buffer.Write(4, 1U, 2);

    *offset = new_header.Write(output, *offset, buffer);
    return CHANGE;
  }
};

TEST_P(TlsConnectTls13, SecondClientHelloRejectEarlyDataXtn) {
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};

  SetupForZeroRtt();
  ExpectResumption(RESUME_TICKET);

  client_->ConfigNamedGroups(groups);
  server_->ConfigNamedGroups(groups);
  client_->Set0RttEnabled(true);
  server_->Set0RttEnabled(true);

  // A new client that tries to resume with 0-RTT but doesn't send the
  // correct key share(s). The server will respond with an HRR.
  auto orig_client =
      std::make_shared<TlsAgent>(client_->name(), TlsAgent::CLIENT, variant_);
  client_.swap(orig_client);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->ConfigureSessionCache(RESUME_BOTH);
  client_->Set0RttEnabled(true);
  client_->StartConnect();

  // Swap in the new client.
  client_->SetPeer(server_);
  server_->SetPeer(client_);

  // Send the ClientHello.
  client_->Handshake();
  // Process the CH, send an HRR.
  server_->Handshake();

  // Swap the client we created manually with the one that successfully
  // received a PSK, and try to resume with 0-RTT. The client doesn't know
  // about the HRR so it will send the early_data xtn as well as 0-RTT data.
  client_.swap(orig_client);
  orig_client.reset();

  // Correct the DTLS message sequence number after an HRR.
  if (variant_ == ssl_variant_datagram) {
    MakeTlsFilter<CorrectMessageSeqAfterHrrFilter>(client_);
  }

  server_->SetPeer(client_);
  client_->Handshake();

  // Send 0-RTT data.
  const char* k0RttData = "ABCDEF";
  const PRInt32 k0RttDataLen = static_cast<PRInt32>(strlen(k0RttData));
  PRInt32 rv = PR_Write(client_->ssl_fd(), k0RttData, k0RttDataLen);
  EXPECT_EQ(k0RttDataLen, rv);

  ExpectAlert(server_, kTlsAlertUnsupportedExtension);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_EXTENSION_ALERT);
}

class KeyShareReplayer : public TlsExtensionFilter {
 public:
  KeyShareReplayer(const std::shared_ptr<TlsAgent>& a)
      : TlsExtensionFilter(a) {}

  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != ssl_tls13_key_share_xtn) {
      return KEEP;
    }

    if (!data_.len()) {
      data_ = input;
      return KEEP;
    }

    *output = data_;
    return CHANGE;
  }

 private:
  DataBuffer data_;
};

// This forces a HelloRetryRequest by disabling P-256 on the server.  However,
// the second ClientHello is modified so that it omits the requested share.  The
// server should reject this.
TEST_P(TlsConnectTls13, RetryWithSameKeyShare) {
  EnsureTlsSetup();
  MakeTlsFilter<KeyShareReplayer>(client_);
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_BAD_2ND_CLIENT_HELLO, server_->error_code());
  EXPECT_EQ(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, client_->error_code());
}

// Here we modify the second ClientHello so that the client retries with the
// same shares, even though the server wanted something else.
TEST_P(TlsConnectTls13, RetryWithTwoShares) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));
  MakeTlsFilter<KeyShareReplayer>(client_);

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
  EXPECT_EQ(SSL_ERROR_BAD_2ND_CLIENT_HELLO, server_->error_code());
  EXPECT_EQ(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, client_->error_code());
}

TEST_P(TlsConnectTls13, RetryCallbackAccept) {
  EnsureTlsSetup();

  auto accept_hello = [](PRBool firstHello, const PRUint8* clientToken,
                         unsigned int clientTokenLen, PRUint8* appToken,
                         unsigned int* appTokenLen, unsigned int appTokenMax,
                         void* arg) {
    auto* called = reinterpret_cast<bool*>(arg);
    *called = true;

    EXPECT_TRUE(firstHello);
    EXPECT_EQ(0U, clientTokenLen);
    return ssl_hello_retry_accept;
  };

  bool cb_run = false;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      accept_hello, &cb_run));
  Connect();
  EXPECT_TRUE(cb_run);
}

TEST_P(TlsConnectTls13, RetryCallbackAcceptGroupMismatch) {
  EnsureTlsSetup();

  auto accept_hello_twice = [](PRBool firstHello, const PRUint8* clientToken,
                               unsigned int clientTokenLen, PRUint8* appToken,
                               unsigned int* appTokenLen,
                               unsigned int appTokenMax, void* arg) {
    auto* called = reinterpret_cast<size_t*>(arg);
    ++*called;

    EXPECT_EQ(0U, clientTokenLen);
    return ssl_hello_retry_accept;
  };

  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_tls13_cookie_xtn);
  capture->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);

  size_t cb_run = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), accept_hello_twice, &cb_run));
  Connect();
  EXPECT_EQ(2U, cb_run);
  EXPECT_TRUE(capture->captured()) << "expected a cookie in HelloRetryRequest";
}

TEST_P(TlsConnectTls13, RetryCallbackFail) {
  EnsureTlsSetup();

  auto fail_hello = [](PRBool firstHello, const PRUint8* clientToken,
                       unsigned int clientTokenLen, PRUint8* appToken,
                       unsigned int* appTokenLen, unsigned int appTokenMax,
                       void* arg) {
    auto* called = reinterpret_cast<bool*>(arg);
    *called = true;

    EXPECT_TRUE(firstHello);
    EXPECT_EQ(0U, clientTokenLen);
    return ssl_hello_retry_fail;
  };

  bool cb_run = false;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      fail_hello, &cb_run));
  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
  server_->CheckErrorCode(SSL_ERROR_APPLICATION_ABORT);
  EXPECT_TRUE(cb_run);
}

// Asking for retry twice isn't allowed.
TEST_P(TlsConnectTls13, RetryCallbackRequestHrrTwice) {
  EnsureTlsSetup();

  auto bad_callback = [](PRBool firstHello, const PRUint8* clientToken,
                         unsigned int clientTokenLen, PRUint8* appToken,
                         unsigned int* appTokenLen, unsigned int appTokenMax,
                         void* arg) -> SSLHelloRetryRequestAction {
    return ssl_hello_retry_request;
  };
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      bad_callback, NULL));
  ConnectExpectAlert(server_, kTlsAlertInternalError);
  server_->CheckErrorCode(SSL_ERROR_APP_CALLBACK_ERROR);
}

// Accepting the CH and modifying the token isn't allowed.
TEST_P(TlsConnectTls13, RetryCallbackAcceptAndSetToken) {
  EnsureTlsSetup();

  auto bad_callback = [](PRBool firstHello, const PRUint8* clientToken,
                         unsigned int clientTokenLen, PRUint8* appToken,
                         unsigned int* appTokenLen, unsigned int appTokenMax,
                         void* arg) -> SSLHelloRetryRequestAction {
    *appTokenLen = 1;
    return ssl_hello_retry_accept;
  };
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      bad_callback, NULL));
  ConnectExpectAlert(server_, kTlsAlertInternalError);
  server_->CheckErrorCode(SSL_ERROR_APP_CALLBACK_ERROR);
}

// As above, but with reject.
TEST_P(TlsConnectTls13, RetryCallbackRejectAndSetToken) {
  EnsureTlsSetup();

  auto bad_callback = [](PRBool firstHello, const PRUint8* clientToken,
                         unsigned int clientTokenLen, PRUint8* appToken,
                         unsigned int* appTokenLen, unsigned int appTokenMax,
                         void* arg) -> SSLHelloRetryRequestAction {
    *appTokenLen = 1;
    return ssl_hello_retry_fail;
  };
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      bad_callback, NULL));
  ConnectExpectAlert(server_, kTlsAlertInternalError);
  server_->CheckErrorCode(SSL_ERROR_APP_CALLBACK_ERROR);
}

// This is a (pretend) buffer overflow.
TEST_P(TlsConnectTls13, RetryCallbackSetTooLargeToken) {
  EnsureTlsSetup();

  auto bad_callback = [](PRBool firstHello, const PRUint8* clientToken,
                         unsigned int clientTokenLen, PRUint8* appToken,
                         unsigned int* appTokenLen, unsigned int appTokenMax,
                         void* arg) -> SSLHelloRetryRequestAction {
    *appTokenLen = appTokenMax + 1;
    return ssl_hello_retry_accept;
  };
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      bad_callback, NULL));
  ConnectExpectAlert(server_, kTlsAlertInternalError);
  server_->CheckErrorCode(SSL_ERROR_APP_CALLBACK_ERROR);
}

SSLHelloRetryRequestAction RetryHello(PRBool firstHello,
                                      const PRUint8* clientToken,
                                      unsigned int clientTokenLen,
                                      PRUint8* appToken,
                                      unsigned int* appTokenLen,
                                      unsigned int appTokenMax, void* arg) {
  auto* called = reinterpret_cast<size_t*>(arg);
  ++*called;

  EXPECT_EQ(0U, clientTokenLen);
  return firstHello ? ssl_hello_retry_request : ssl_hello_retry_accept;
}

TEST_P(TlsConnectTls13, RetryCallbackRetry) {
  EnsureTlsSetup();

  auto capture_hrr = std::make_shared<TlsHandshakeRecorder>(
      server_, ssl_hs_hello_retry_request);
  auto capture_key_share =
      std::make_shared<TlsExtensionCapture>(server_, ssl_tls13_key_share_xtn);
  capture_key_share->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});
  std::vector<std::shared_ptr<PacketFilter>> chain = {capture_hrr,
                                                      capture_key_share};
  server_->SetFilter(std::make_shared<ChainedPacketFilter>(chain));

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      RetryHello, &cb_called));

  // Do the first message exchange.
  StartConnect();
  client_->Handshake();
  server_->Handshake();

  EXPECT_EQ(1U, cb_called) << "callback should be called once here";
  EXPECT_LT(0U, capture_hrr->buffer().len()) << "HelloRetryRequest expected";
  EXPECT_FALSE(capture_key_share->captured())
      << "no key_share extension expected";

  auto capture_cookie =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_cookie_xtn);

  Handshake();
  CheckConnected();
  EXPECT_EQ(2U, cb_called);
  EXPECT_TRUE(capture_cookie->captured()) << "should have a cookie";
}

static size_t CountShares(const DataBuffer& key_share) {
  size_t count = 0;
  uint32_t len = 0;
  size_t offset = 2;

  EXPECT_TRUE(key_share.Read(0, 2, &len));
  EXPECT_EQ(key_share.len() - 2, len);
  while (offset < key_share.len()) {
    offset += 2;  // Skip KeyShareEntry.group
    EXPECT_TRUE(key_share.Read(offset, 2, &len));
    offset += 2 + len;  // Skip KeyShareEntry.key_exchange
    ++count;
  }
  return count;
}

TEST_P(TlsConnectTls13, RetryCallbackRetryWithAdditionalShares) {
  EnsureTlsSetup();
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  auto capture_server =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_tls13_key_share_xtn);
  capture_server->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      RetryHello, &cb_called));

  // Do the first message exchange.
  StartConnect();
  client_->Handshake();
  server_->Handshake();

  EXPECT_EQ(1U, cb_called) << "callback should be called once here";
  EXPECT_FALSE(capture_server->captured())
      << "no key_share extension expected from server";

  auto capture_client_2nd =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_key_share_xtn);

  Handshake();
  CheckConnected();
  EXPECT_EQ(2U, cb_called);
  EXPECT_TRUE(capture_client_2nd->captured()) << "client should send key_share";
  EXPECT_EQ(2U, CountShares(capture_client_2nd->extension()))
      << "client should still send two shares";
}

// The callback should be run even if we have another reason to send
// HelloRetryRequest.  In this case, the server sends HRR because the server
// wants a P-384 key share and the client didn't offer one.
TEST_P(TlsConnectTls13, RetryCallbackRetryWithGroupMismatch) {
  EnsureTlsSetup();

  auto capture_cookie =
      std::make_shared<TlsExtensionCapture>(server_, ssl_tls13_cookie_xtn);
  capture_cookie->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});
  auto capture_key_share =
      std::make_shared<TlsExtensionCapture>(server_, ssl_tls13_key_share_xtn);
  capture_key_share->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});
  server_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit{capture_cookie, capture_key_share}));

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      RetryHello, &cb_called));
  Connect();
  EXPECT_EQ(2U, cb_called);
  EXPECT_TRUE(capture_cookie->captured()) << "cookie expected";
  EXPECT_TRUE(capture_key_share->captured()) << "key_share expected";
}

static const uint8_t kApplicationToken[] = {0x92, 0x44, 0x00};

SSLHelloRetryRequestAction RetryHelloWithToken(
    PRBool firstHello, const PRUint8* clientToken, unsigned int clientTokenLen,
    PRUint8* appToken, unsigned int* appTokenLen, unsigned int appTokenMax,
    void* arg) {
  auto* called = reinterpret_cast<size_t*>(arg);
  ++*called;

  if (firstHello) {
    memcpy(appToken, kApplicationToken, sizeof(kApplicationToken));
    *appTokenLen = sizeof(kApplicationToken);
    return ssl_hello_retry_request;
  }

  EXPECT_EQ(DataBuffer(kApplicationToken, sizeof(kApplicationToken)),
            DataBuffer(clientToken, static_cast<size_t>(clientTokenLen)));
  return ssl_hello_retry_accept;
}

TEST_P(TlsConnectTls13, RetryCallbackRetryWithToken) {
  EnsureTlsSetup();

  auto capture_key_share =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_tls13_key_share_xtn);
  capture_key_share->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess,
            SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                          RetryHelloWithToken, &cb_called));
  Connect();
  EXPECT_EQ(2U, cb_called);
  EXPECT_FALSE(capture_key_share->captured()) << "no key share expected";
}

TEST_P(TlsConnectTls13, RetryCallbackRetryWithTokenAndGroupMismatch) {
  EnsureTlsSetup();

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);

  auto capture_key_share =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_tls13_key_share_xtn);
  capture_key_share->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess,
            SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                          RetryHelloWithToken, &cb_called));
  Connect();
  EXPECT_EQ(2U, cb_called);
  EXPECT_TRUE(capture_key_share->captured()) << "key share expected";
}

SSLHelloRetryRequestAction CheckTicketToken(
    PRBool firstHello, const PRUint8* clientToken, unsigned int clientTokenLen,
    PRUint8* appToken, unsigned int* appTokenLen, unsigned int appTokenMax,
    void* arg) {
  auto* called = reinterpret_cast<bool*>(arg);
  *called = true;

  EXPECT_TRUE(firstHello);
  EXPECT_EQ(DataBuffer(kApplicationToken, sizeof(kApplicationToken)),
            DataBuffer(clientToken, static_cast<size_t>(clientTokenLen)));
  return ssl_hello_retry_accept;
}

// Stream because SSL_SendSessionTicket only supports that.
TEST_F(TlsConnectStreamTls13, RetryCallbackWithSessionTicketToken) {
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  Connect();
  EXPECT_EQ(SECSuccess,
            SSL_SendSessionTicket(server_->ssl_fd(), kApplicationToken,
                                  sizeof(kApplicationToken)));
  SendReceive();

  Reset();
  ConfigureSessionCache(RESUME_BOTH, RESUME_TICKET);
  ExpectResumption(RESUME_TICKET);

  bool cb_run = false;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(
                            server_->ssl_fd(), CheckTicketToken, &cb_run));
  Connect();
  EXPECT_TRUE(cb_run);
}

void TriggerHelloRetryRequest(std::shared_ptr<TlsAgent>& client,
                              std::shared_ptr<TlsAgent>& server) {
  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server->ssl_fd(),
                                                      RetryHello, &cb_called));

  // Start the handshake.
  client->StartConnect();
  server->StartConnect();
  client->Handshake();
  server->Handshake();
  EXPECT_EQ(1U, cb_called);
  // Stop the callback from being called in future handshakes.
  EXPECT_EQ(SECSuccess,
            SSL_HelloRetryRequestCallback(server->ssl_fd(), nullptr, nullptr));
}

TEST_P(TlsConnectTls13, VersionNumbersAfterRetry) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();
  auto r = MakeTlsFilter<TlsRecordRecorder>(client_);
  TriggerHelloRetryRequest(client_, server_);
  Handshake();
  ASSERT_GT(r->count(), 1UL);
  auto ch1 = r->record(0);
  if (ch1.header.is_dtls()) {
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, ch1.header.version());
  } else {
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, ch1.header.version());
  }
  auto ch2 = r->record(1);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, ch2.header.version());

  CheckConnected();
}

TEST_P(TlsConnectTls13, RetryStateless) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  Handshake();
  CheckConnected();
  SendReceive();
}

TEST_P(TlsConnectTls13, RetryStatefulDropCookie) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TriggerHelloRetryRequest(client_, server_);
  MakeTlsFilter<TlsExtensionDropper>(client_, ssl_tls13_cookie_xtn);

  ExpectAlert(server_, kTlsAlertMissingExtension);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_MISSING_EXTENSION_ALERT);
  server_->CheckErrorCode(SSL_ERROR_MISSING_COOKIE_EXTENSION);
}

// Stream only because DTLS drops bad packets.
TEST_F(TlsConnectStreamTls13, RetryStatelessDamageFirstClientHello) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  auto damage_ch =
      MakeTlsFilter<TlsExtensionInjector>(client_, 0xfff3, DataBuffer());

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  // Key exchange fails when the handshake continues because client and server
  // disagree about the transcript.
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
}

TEST_F(TlsConnectStreamTls13, RetryStatelessDamageSecondClientHello) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  auto damage_ch =
      MakeTlsFilter<TlsExtensionInjector>(client_, 0xfff3, DataBuffer());

  // Key exchange fails when the handshake continues because client and server
  // disagree about the transcript.
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
  client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
}

// Read the cipher suite from the HRR and disable it on the identified agent.
static void DisableSuiteFromHrr(
    std::shared_ptr<TlsAgent>& agent,
    std::shared_ptr<TlsHandshakeRecorder>& capture_hrr) {
  uint32_t tmp;
  size_t offset = 2 + 32;  // skip version + server_random
  ASSERT_TRUE(
      capture_hrr->buffer().Read(offset, 1, &tmp));  // session_id length
  EXPECT_EQ(0U, tmp);
  offset += 1 + tmp;
  ASSERT_TRUE(capture_hrr->buffer().Read(offset, 2, &tmp));  // suite
  EXPECT_EQ(
      SECSuccess,
      SSL_CipherPrefSet(agent->ssl_fd(), static_cast<uint16_t>(tmp), PR_FALSE));
}

TEST_P(TlsConnectTls13, RetryStatelessDisableSuiteClient) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  auto capture_hrr =
      MakeTlsFilter<TlsHandshakeRecorder>(server_, ssl_hs_hello_retry_request);

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  DisableSuiteFromHrr(client_, capture_hrr);

  // The client thinks that the HelloRetryRequest is bad, even though its
  // because it changed its mind about the cipher suite.
  ExpectAlert(client_, kTlsAlertIllegalParameter);
  Handshake();
  client_->CheckErrorCode(SSL_ERROR_NO_CYPHER_OVERLAP);
  server_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectTls13, RetryStatelessDisableSuiteServer) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  auto capture_hrr =
      MakeTlsFilter<TlsHandshakeRecorder>(server_, ssl_hs_hello_retry_request);

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  DisableSuiteFromHrr(server_, capture_hrr);

  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectTls13, RetryStatelessDisableGroupClient) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  client_->ConfigNamedGroups(groups);

  // We're into undefined behavior on the client side, but - at the point this
  // test was written - the client here doesn't amend its key shares because the
  // server doesn't ask it to.  The server notices that the key share (x25519)
  // doesn't match the negotiated group (P-384) and objects.
  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectTls13, RetryStatelessDisableGroupServer) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TriggerHelloRetryRequest(client_, server_);
  MakeNewServer();

  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);

  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

TEST_P(TlsConnectTls13, RetryStatelessBadCookie) {
  ConfigureSelfEncrypt();
  EnsureTlsSetup();

  TriggerHelloRetryRequest(client_, server_);

  // Now replace the self-encrypt MAC key with a garbage key.
  static const uint8_t bad_hmac_key[32] = {0};
  SECItem key_item = {siBuffer, const_cast<uint8_t*>(bad_hmac_key),
                      sizeof(bad_hmac_key)};
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  PK11SymKey* hmac_key =
      PK11_ImportSymKey(slot.get(), CKM_SHA256_HMAC, PK11_OriginUnwrap,
                        CKA_SIGN, &key_item, nullptr);
  ASSERT_NE(nullptr, hmac_key);
  SSLInt_SetSelfEncryptMacKey(hmac_key);  // Passes ownership.

  MakeNewServer();

  ExpectAlert(server_, kTlsAlertIllegalParameter);
  Handshake();
  server_->CheckErrorCode(SSL_ERROR_BAD_2ND_CLIENT_HELLO);
  client_->CheckErrorCode(SSL_ERROR_ILLEGAL_PARAMETER_ALERT);
}

// Stream because the server doesn't consume the alert and terminate.
TEST_F(TlsConnectStreamTls13, RetryWithDifferentCipherSuite) {
  EnsureTlsSetup();
  // Force a HelloRetryRequest.
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);
  // Then switch out the default suite (TLS_AES_128_GCM_SHA256).
  MakeTlsFilter<SelectedCipherSuiteReplacer>(server_,
                                             TLS_CHACHA20_POLY1305_SHA256);

  client_->ExpectSendAlert(kTlsAlertIllegalParameter);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
  EXPECT_EQ(SSL_ERROR_BAD_MAC_READ, server_->error_code());
}

// This tests that the second attempt at sending a ClientHello (after receiving
// a HelloRetryRequest) is correctly retransmitted.
TEST_F(TlsConnectDatagram13, DropClientSecondFlightWithHelloRetry) {
  static const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1,
                                                    ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(groups);
  server_->SetFilter(std::make_shared<SelectiveDropFilter>(0x2));
  Connect();
}

class TlsKeyExchange13 : public TlsKeyExchangeTest {};

// This should work, with an HRR, because the server prefers x25519 and the
// client generates a share for P-384 on the initial ClientHello.
TEST_P(TlsKeyExchange13, ConnectEcdhePreferenceMismatchHrr) {
  EnsureKeyShareSetup();
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_curve25519};
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_curve25519, ssl_grp_ec_secp384r1};
  client_->ConfigNamedGroups(client_groups);
  server_->ConfigNamedGroups(server_groups);
  Connect();
  CheckKeys();
  static const std::vector<SSLNamedGroup> expectedShares = {
      ssl_grp_ec_secp384r1};
  CheckKEXDetails(client_groups, expectedShares, ssl_grp_ec_curve25519);
}

// This should work, but not use HRR because the key share for x25519 was
// pre-generated by the client.
TEST_P(TlsKeyExchange13, ConnectEcdhePreferenceMismatchHrrExtraShares) {
  EnsureKeyShareSetup();
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_curve25519};
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_curve25519, ssl_grp_ec_secp384r1};
  client_->ConfigNamedGroups(client_groups);
  server_->ConfigNamedGroups(server_groups);
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  Connect();
  CheckKeys();
  CheckKEXDetails(client_groups, client_groups);
}

// The callback should be run even if we have another reason to send
// HelloRetryRequest.  In this case, the server sends HRR because the server
// wants an X25519 key share and the client didn't offer one.
TEST_P(TlsKeyExchange13,
       RetryCallbackRetryWithGroupMismatchAndAdditionalShares) {
  EnsureKeyShareSetup();

  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp256r1, ssl_grp_ec_secp384r1, ssl_grp_ec_curve25519};
  client_->ConfigNamedGroups(client_groups);
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_curve25519};
  server_->ConfigNamedGroups(server_groups);
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  auto capture_server =
      std::make_shared<TlsExtensionCapture>(server_, ssl_tls13_key_share_xtn);
  capture_server->SetHandshakeTypes({kTlsHandshakeHelloRetryRequest});
  server_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit{capture_hrr_, capture_server}));

  size_t cb_called = 0;
  EXPECT_EQ(SECSuccess, SSL_HelloRetryRequestCallback(server_->ssl_fd(),
                                                      RetryHello, &cb_called));

  // Do the first message exchange.
  StartConnect();
  client_->Handshake();
  server_->Handshake();

  EXPECT_EQ(1U, cb_called) << "callback should be called once here";
  EXPECT_TRUE(capture_server->captured()) << "key_share extension expected";

  uint32_t server_group = 0;
  EXPECT_TRUE(capture_server->extension().Read(0, 2, &server_group));
  EXPECT_EQ(ssl_grp_ec_curve25519, static_cast<SSLNamedGroup>(server_group));

  Handshake();
  CheckConnected();
  EXPECT_EQ(2U, cb_called);
  EXPECT_TRUE(shares_capture2_->captured()) << "client should send shares";

  CheckKeys();
  static const std::vector<SSLNamedGroup> client_shares(
      client_groups.begin(), client_groups.begin() + 2);
  CheckKEXDetails(client_groups, client_shares, server_groups[0]);
}

TEST_F(TlsConnectTest, Select12AfterHelloRetryRequest) {
  EnsureTlsSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  static const std::vector<SSLNamedGroup> client_groups = {
      ssl_grp_ec_secp256r1, ssl_grp_ec_secp521r1};
  client_->ConfigNamedGroups(client_groups);
  static const std::vector<SSLNamedGroup> server_groups = {
      ssl_grp_ec_secp384r1, ssl_grp_ec_secp521r1};
  server_->ConfigNamedGroups(server_groups);
  StartConnect();

  client_->Handshake();
  server_->Handshake();

  // Here we replace the TLS server with one that does TLS 1.2 only.
  // This will happily send the client a TLS 1.2 ServerHello.
  server_.reset(new TlsAgent(server_->name(), TlsAgent::SERVER, variant_));
  client_->SetPeer(server_);
  server_->SetPeer(client_);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->StartConnect();
  ExpectAlert(client_, kTlsAlertIllegalParameter);
  Handshake();
  EXPECT_EQ(SSL_ERROR_ILLEGAL_PARAMETER_ALERT, server_->error_code());
  EXPECT_EQ(SSL_ERROR_RX_MALFORMED_SERVER_HELLO, client_->error_code());
}

class HelloRetryRequestAgentTest : public TlsAgentTestClient {
 protected:
  void SetUp() override {
    TlsAgentTestClient::SetUp();
    EnsureInit();
    agent_->StartConnect();
  }

  void MakeCannedHrr(const uint8_t* body, size_t len, DataBuffer* hrr_record,
                     uint32_t seq_num = 0) const {
    DataBuffer hrr_data;
    const uint8_t ssl_hello_retry_random[] = {
        0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C,
        0x02, 0x1E, 0x65, 0xB8, 0x91, 0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB,
        0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C};

    hrr_data.Allocate(len + 6);
    size_t i = 0;
    i = hrr_data.Write(i, variant_ == ssl_variant_datagram
                              ? SSL_LIBRARY_VERSION_DTLS_1_2_WIRE
                              : SSL_LIBRARY_VERSION_TLS_1_2,
                       2);
    i = hrr_data.Write(i, ssl_hello_retry_random,
                       sizeof(ssl_hello_retry_random));
    i = hrr_data.Write(i, static_cast<uint32_t>(0), 1);  // session_id
    i = hrr_data.Write(i, TLS_AES_128_GCM_SHA256, 2);
    i = hrr_data.Write(i, ssl_compression_null, 1);
    // Add extensions.  First a length, which includes the supported version.
    i = hrr_data.Write(i, static_cast<uint32_t>(len) + 6, 2);
    // Now the supported version.
    i = hrr_data.Write(i, ssl_tls13_supported_versions_xtn, 2);
    i = hrr_data.Write(i, 2, 2);
    i = hrr_data.Write(i, 0x7f00 | TLS_1_3_DRAFT_VERSION, 2);
    if (len) {
      hrr_data.Write(i, body, len);
    }
    DataBuffer hrr;
    MakeHandshakeMessage(kTlsHandshakeServerHello, hrr_data.data(),
                         hrr_data.len(), &hrr, seq_num);
    MakeRecord(kTlsHandshakeType, SSL_LIBRARY_VERSION_TLS_1_3, hrr.data(),
               hrr.len(), hrr_record, seq_num);
  }

  void MakeGroupHrr(SSLNamedGroup group, DataBuffer* hrr_record,
                    uint32_t seq_num = 0) const {
    const uint8_t group_hrr[] = {
        static_cast<uint8_t>(ssl_tls13_key_share_xtn >> 8),
        static_cast<uint8_t>(ssl_tls13_key_share_xtn),
        0,
        2,  // length of key share extension
        static_cast<uint8_t>(group >> 8),
        static_cast<uint8_t>(group)};
    MakeCannedHrr(group_hrr, sizeof(group_hrr), hrr_record, seq_num);
  }
};

// Send two HelloRetryRequest messages in response to the ClientHello.  The are
// constructed to appear legitimate by asking for a new share in each, so that
// the client has to count to work out that the server is being unreasonable.
TEST_P(HelloRetryRequestAgentTest, SendSecondHelloRetryRequest) {
  DataBuffer hrr;
  MakeGroupHrr(ssl_grp_ec_secp384r1, &hrr, 0);
  ProcessMessage(hrr, TlsAgent::STATE_CONNECTING);
  MakeGroupHrr(ssl_grp_ec_secp521r1, &hrr, 1);
  ExpectAlert(kTlsAlertUnexpectedMessage);
  ProcessMessage(hrr, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_UNEXPECTED_HELLO_RETRY_REQUEST);
}

// Here the client receives a HelloRetryRequest with a group that they already
// provided a share for.
TEST_P(HelloRetryRequestAgentTest, HandleBogusHelloRetryRequest) {
  DataBuffer hrr;
  MakeGroupHrr(ssl_grp_ec_curve25519, &hrr);
  ExpectAlert(kTlsAlertIllegalParameter);
  ProcessMessage(hrr, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
}

TEST_P(HelloRetryRequestAgentTest, HandleNoopHelloRetryRequest) {
  DataBuffer hrr;
  MakeCannedHrr(nullptr, 0U, &hrr);
  ExpectAlert(kTlsAlertDecodeError);
  ProcessMessage(hrr, TlsAgent::STATE_ERROR,
                 SSL_ERROR_RX_MALFORMED_HELLO_RETRY_REQUEST);
}

INSTANTIATE_TEST_CASE_P(HelloRetryRequestAgentTests, HelloRetryRequestAgentTest,
                        ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                           TlsConnectTestBase::kTlsV13));
#ifndef NSS_DISABLE_TLS_1_3
INSTANTIATE_TEST_CASE_P(HelloRetryRequestKeyExchangeTests, TlsKeyExchange13,
                        ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                           TlsConnectTestBase::kTlsV13));
#endif

}  // namespace nss_test
