/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"
#include "sslexp.h"

#include <memory>

#include "tls_connect.h"

namespace nss_test {

static void IncrementCounterArg(void *arg) {
  if (arg) {
    auto *called = reinterpret_cast<size_t *>(arg);
    ++*called;
  }
}

PRBool NoopExtensionWriter(PRFileDesc *fd, SSLHandshakeType message,
                           PRUint8 *data, unsigned int *len,
                           unsigned int maxLen, void *arg) {
  IncrementCounterArg(arg);
  return PR_FALSE;
}

PRBool EmptyExtensionWriter(PRFileDesc *fd, SSLHandshakeType message,
                            PRUint8 *data, unsigned int *len,
                            unsigned int maxLen, void *arg) {
  IncrementCounterArg(arg);
  return PR_TRUE;
}

SECStatus NoopExtensionHandler(PRFileDesc *fd, SSLHandshakeType message,
                               const PRUint8 *data, unsigned int len,
                               SSLAlertDescription *alert, void *arg) {
  return SECSuccess;
}

// All of the (current) set of supported extensions, plus a few extra.
static const uint16_t kManyExtensions[] = {
    ssl_server_name_xtn,
    ssl_cert_status_xtn,
    ssl_supported_groups_xtn,
    ssl_ec_point_formats_xtn,
    ssl_signature_algorithms_xtn,
    ssl_signature_algorithms_cert_xtn,
    ssl_use_srtp_xtn,
    ssl_app_layer_protocol_xtn,
    ssl_signed_cert_timestamp_xtn,
    ssl_padding_xtn,
    ssl_extended_master_secret_xtn,
    ssl_session_ticket_xtn,
    ssl_tls13_key_share_xtn,
    ssl_tls13_pre_shared_key_xtn,
    ssl_tls13_early_data_xtn,
    ssl_tls13_supported_versions_xtn,
    ssl_tls13_cookie_xtn,
    ssl_tls13_psk_key_exchange_modes_xtn,
    ssl_tls13_ticket_early_data_info_xtn,
    ssl_tls13_certificate_authorities_xtn,
    ssl_next_proto_nego_xtn,
    ssl_renegotiation_info_xtn,
    ssl_tls13_short_header_xtn,
    1,
    0xffff};
// The list here includes all extensions we expect to use (SSL_MAX_EXTENSIONS),
// plus the deprecated values (see sslt.h), and two extra dummy values.
PR_STATIC_ASSERT((SSL_MAX_EXTENSIONS + 5) == PR_ARRAY_SIZE(kManyExtensions));

void InstallManyWriters(std::shared_ptr<TlsAgent> agent,
                        SSLExtensionWriter writer, size_t *installed = nullptr,
                        size_t *called = nullptr) {
  for (size_t i = 0; i < PR_ARRAY_SIZE(kManyExtensions); ++i) {
    SSLExtensionSupport support = ssl_ext_none;
    SECStatus rv = SSL_GetExtensionSupport(kManyExtensions[i], &support);
    ASSERT_EQ(SECSuccess, rv) << "SSL_GetExtensionSupport cannot fail";

    rv = SSL_InstallExtensionHooks(agent->ssl_fd(), kManyExtensions[i], writer,
                                   called, NoopExtensionHandler, nullptr);
    if (support == ssl_ext_native_only) {
      EXPECT_EQ(SECFailure, rv);
      EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
    } else {
      if (installed) {
        ++*installed;
      }
      EXPECT_EQ(SECSuccess, rv);
    }
  }
}

TEST_F(TlsConnectStreamTls13, CustomExtensionAllNoopClient) {
  EnsureTlsSetup();
  size_t installed = 0;
  size_t called = 0;
  InstallManyWriters(client_, NoopExtensionWriter, &installed, &called);
  EXPECT_LT(0U, installed);
  Connect();
  EXPECT_EQ(installed, called);
}

TEST_F(TlsConnectStreamTls13, CustomExtensionAllNoopServer) {
  EnsureTlsSetup();
  size_t installed = 0;
  size_t called = 0;
  InstallManyWriters(server_, NoopExtensionWriter, &installed, &called);
  EXPECT_LT(0U, installed);
  Connect();
  // Extension writers are all called for each of ServerHello,
  // EncryptedExtensions, and Certificate.
  EXPECT_EQ(installed * 3, called);
}

TEST_F(TlsConnectStreamTls13, CustomExtensionEmptyWriterClient) {
  EnsureTlsSetup();
  InstallManyWriters(client_, EmptyExtensionWriter);
  InstallManyWriters(server_, EmptyExtensionWriter);
  Connect();
}

TEST_F(TlsConnectStreamTls13, CustomExtensionEmptyWriterServer) {
  EnsureTlsSetup();
  InstallManyWriters(server_, EmptyExtensionWriter);
  // Sending extensions that the client doesn't expect leads to extensions
  // appearing even if the client didn't send one, or in the wrong messages.
  client_->ExpectSendAlert(kTlsAlertUnsupportedExtension);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

// Install an writer to disable sending of a natively-supported extension.
TEST_F(TlsConnectStreamTls13, CustomExtensionWriterDisable) {
  EnsureTlsSetup();

  // This option enables sending the extension via the native support.
  SECStatus rv = SSL_OptionSet(client_->ssl_fd(),
                               SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);

  // This installs an override that doesn't do anything.  You have to specify
  // something; passing all nullptr values removes an existing handler.
  rv = SSL_InstallExtensionHooks(
      client_->ssl_fd(), ssl_signed_cert_timestamp_xtn, NoopExtensionWriter,
      nullptr, NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);
  auto capture =
      std::make_shared<TlsExtensionCapture>(ssl_signed_cert_timestamp_xtn);
  client_->SetPacketFilter(capture);

  Connect();
  // So nothing will be sent.
  EXPECT_FALSE(capture->captured());
}

// An extension that is unlikely to be parsed as valid.
static uint8_t kNonsenseExtension[] = {91, 82, 73, 64, 55, 46, 37, 28, 19};

static PRBool NonsenseExtensionWriter(PRFileDesc *fd, SSLHandshakeType message,
                                      PRUint8 *data, unsigned int *len,
                                      unsigned int maxLen, void *arg) {
  TlsAgent *agent = reinterpret_cast<TlsAgent *>(arg);
  EXPECT_NE(nullptr, agent);
  EXPECT_NE(nullptr, data);
  EXPECT_NE(nullptr, len);
  EXPECT_EQ(0U, *len);
  EXPECT_LT(0U, maxLen);
  EXPECT_EQ(agent->ssl_fd(), fd);

  if (message != ssl_hs_client_hello && message != ssl_hs_server_hello &&
      message != ssl_hs_encrypted_extensions) {
    return PR_FALSE;
  }

  *len = static_cast<unsigned int>(sizeof(kNonsenseExtension));
  EXPECT_GE(maxLen, *len);
  if (maxLen < *len) {
    return PR_FALSE;
  }
  PORT_Memcpy(data, kNonsenseExtension, *len);
  return PR_TRUE;
}

// Override the extension handler for an natively-supported and produce
// nonsense, which results in a handshake failure.
TEST_F(TlsConnectStreamTls13, CustomExtensionOverride) {
  EnsureTlsSetup();

  // This option enables sending the extension via the native support.
  SECStatus rv = SSL_OptionSet(client_->ssl_fd(),
                               SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);

  // This installs an override that sends nonsense.
  rv = SSL_InstallExtensionHooks(
      client_->ssl_fd(), ssl_signed_cert_timestamp_xtn, NonsenseExtensionWriter,
      client_.get(), NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Capture it to see what we got.
  auto capture =
      std::make_shared<TlsExtensionCapture>(ssl_signed_cert_timestamp_xtn);
  client_->SetPacketFilter(capture);

  ConnectExpectAlert(server_, kTlsAlertDecodeError);

  EXPECT_TRUE(capture->captured());
  EXPECT_EQ(DataBuffer(kNonsenseExtension, sizeof(kNonsenseExtension)),
            capture->extension());
}

static SECStatus NonsenseExtensionHandler(PRFileDesc *fd,
                                          SSLHandshakeType message,
                                          const PRUint8 *data, unsigned int len,
                                          SSLAlertDescription *alert,
                                          void *arg) {
  TlsAgent *agent = reinterpret_cast<TlsAgent *>(arg);
  EXPECT_EQ(agent->ssl_fd(), fd);
  if (agent->role() == TlsAgent::SERVER) {
    EXPECT_EQ(ssl_hs_client_hello, message);
  } else {
    EXPECT_TRUE(message == ssl_hs_server_hello ||
                message == ssl_hs_encrypted_extensions);
  }
  EXPECT_EQ(DataBuffer(kNonsenseExtension, sizeof(kNonsenseExtension)),
            DataBuffer(data, len));
  EXPECT_NE(nullptr, alert);
  return SECSuccess;
}

// Send nonsense in an extension from client to server.
TEST_F(TlsConnectStreamTls13, CustomExtensionClientToServer) {
  EnsureTlsSetup();

  // This installs an override that sends nonsense.
  const uint16_t extension_code = 0xffe5;
  SECStatus rv = SSL_InstallExtensionHooks(
      client_->ssl_fd(), extension_code, NonsenseExtensionWriter, client_.get(),
      NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Capture it to see what we got.
  auto capture = std::make_shared<TlsExtensionCapture>(extension_code);
  client_->SetPacketFilter(capture);

  // Handle it so that the handshake completes.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 NoopExtensionWriter, nullptr,
                                 NonsenseExtensionHandler, server_.get());
  EXPECT_EQ(SECSuccess, rv);

  Connect();

  EXPECT_TRUE(capture->captured());
  EXPECT_EQ(DataBuffer(kNonsenseExtension, sizeof(kNonsenseExtension)),
            capture->extension());
}

static PRBool NonsenseExtensionWriterSH(PRFileDesc *fd,
                                        SSLHandshakeType message, PRUint8 *data,
                                        unsigned int *len, unsigned int maxLen,
                                        void *arg) {
  if (message == ssl_hs_server_hello) {
    return NonsenseExtensionWriter(fd, message, data, len, maxLen, arg);
  }
  return PR_FALSE;
}

// Send nonsense in an extension from server to client, in ServerHello.
TEST_F(TlsConnectStreamTls13, CustomExtensionServerToClientSH) {
  EnsureTlsSetup();

  // This installs an override that sends nothing but expects nonsense.
  const uint16_t extension_code = 0xff5e;
  SECStatus rv = SSL_InstallExtensionHooks(
      client_->ssl_fd(), extension_code, EmptyExtensionWriter, nullptr,
      NonsenseExtensionHandler, client_.get());
  EXPECT_EQ(SECSuccess, rv);

  // Have the server send nonsense.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 NonsenseExtensionWriterSH, server_.get(),
                                 NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Capture the extension from the ServerHello only and check it.
  auto capture = std::make_shared<TlsExtensionCapture>(extension_code);
  capture->SetHandshakeTypes({kTlsHandshakeServerHello});
  server_->SetPacketFilter(capture);

  Connect();

  EXPECT_TRUE(capture->captured());
  EXPECT_EQ(DataBuffer(kNonsenseExtension, sizeof(kNonsenseExtension)),
            capture->extension());
}

static PRBool NonsenseExtensionWriterEE(PRFileDesc *fd,
                                        SSLHandshakeType message, PRUint8 *data,
                                        unsigned int *len, unsigned int maxLen,
                                        void *arg) {
  if (message == ssl_hs_encrypted_extensions) {
    return NonsenseExtensionWriter(fd, message, data, len, maxLen, arg);
  }
  return PR_FALSE;
}

// Send nonsense in an extension from server to client, in EncryptedExtensions.
TEST_F(TlsConnectStreamTls13, CustomExtensionServerToClientEE) {
  EnsureTlsSetup();

  // This installs an override that sends nothing but expects nonsense.
  const uint16_t extension_code = 0xff5e;
  SECStatus rv = SSL_InstallExtensionHooks(
      client_->ssl_fd(), extension_code, EmptyExtensionWriter, nullptr,
      NonsenseExtensionHandler, client_.get());
  EXPECT_EQ(SECSuccess, rv);

  // Have the server send nonsense.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 NonsenseExtensionWriterEE, server_.get(),
                                 NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Capture the extension from the EncryptedExtensions only and check it.
  auto capture = std::make_shared<TlsExtensionCapture>(extension_code);
  capture->SetHandshakeTypes({kTlsHandshakeEncryptedExtensions});
  server_->SetTlsRecordFilter(capture);

  Connect();

  EXPECT_TRUE(capture->captured());
  EXPECT_EQ(DataBuffer(kNonsenseExtension, sizeof(kNonsenseExtension)),
            capture->extension());
}

TEST_F(TlsConnectStreamTls13, CustomExtensionUnsolicitedServer) {
  EnsureTlsSetup();

  const uint16_t extension_code = 0xff5e;
  SECStatus rv = SSL_InstallExtensionHooks(
      server_->ssl_fd(), extension_code, NonsenseExtensionWriter, server_.get(),
      NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Capture it to see what we got.
  auto capture = std::make_shared<TlsExtensionCapture>(extension_code);
  server_->SetPacketFilter(capture);

  client_->ExpectSendAlert(kTlsAlertUnsupportedExtension);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();

  EXPECT_TRUE(capture->captured());
  EXPECT_EQ(DataBuffer(kNonsenseExtension, sizeof(kNonsenseExtension)),
            capture->extension());
}

SECStatus RejectExtensionHandler(PRFileDesc *fd, SSLHandshakeType message,
                                 const PRUint8 *data, unsigned int len,
                                 SSLAlertDescription *alert, void *arg) {
  return SECFailure;
}

TEST_F(TlsConnectStreamTls13, CustomExtensionServerReject) {
  EnsureTlsSetup();

  // This installs an override that sends nonsense.
  const uint16_t extension_code = 0xffe7;
  SECStatus rv = SSL_InstallExtensionHooks(client_->ssl_fd(), extension_code,
                                           EmptyExtensionWriter, nullptr,
                                           NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Reject the extension for no good reason.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 NoopExtensionWriter, nullptr,
                                 RejectExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  ConnectExpectAlert(server_, kTlsAlertHandshakeFailure);
}

// Send nonsense in an extension from client to server.
TEST_F(TlsConnectStreamTls13, CustomExtensionClientReject) {
  EnsureTlsSetup();

  // This installs an override that sends nothing but expects nonsense.
  const uint16_t extension_code = 0xff58;
  SECStatus rv = SSL_InstallExtensionHooks(client_->ssl_fd(), extension_code,
                                           EmptyExtensionWriter, nullptr,
                                           RejectExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Have the server send nonsense.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 EmptyExtensionWriter, nullptr,
                                 NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  client_->ExpectSendAlert(kTlsAlertHandshakeFailure);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

static const uint8_t kCustomAlert = 0xf6;

SECStatus AlertExtensionHandler(PRFileDesc *fd, SSLHandshakeType message,
                                const PRUint8 *data, unsigned int len,
                                SSLAlertDescription *alert, void *arg) {
  *alert = kCustomAlert;
  return SECFailure;
}

TEST_F(TlsConnectStreamTls13, CustomExtensionServerRejectAlert) {
  EnsureTlsSetup();

  // This installs an override that sends nonsense.
  const uint16_t extension_code = 0xffea;
  SECStatus rv = SSL_InstallExtensionHooks(client_->ssl_fd(), extension_code,
                                           EmptyExtensionWriter, nullptr,
                                           NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Reject the extension for no good reason.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 NoopExtensionWriter, nullptr,
                                 AlertExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  ConnectExpectAlert(server_, kCustomAlert);
}

// Send nonsense in an extension from client to server.
TEST_F(TlsConnectStreamTls13, CustomExtensionClientRejectAlert) {
  EnsureTlsSetup();

  // This installs an override that sends nothing but expects nonsense.
  const uint16_t extension_code = 0xff5a;
  SECStatus rv = SSL_InstallExtensionHooks(client_->ssl_fd(), extension_code,
                                           EmptyExtensionWriter, nullptr,
                                           AlertExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  // Have the server send nonsense.
  rv = SSL_InstallExtensionHooks(server_->ssl_fd(), extension_code,
                                 EmptyExtensionWriter, nullptr,
                                 NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);

  client_->ExpectSendAlert(kCustomAlert);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

// Configure a custom extension hook badly.
TEST_F(TlsConnectStreamTls13, CustomExtensionOnlyWriter) {
  EnsureTlsSetup();

  // This installs an override that sends nothing but expects nonsense.
  SECStatus rv =
      SSL_InstallExtensionHooks(client_->ssl_fd(), 0xff6c, EmptyExtensionWriter,
                                nullptr, nullptr, nullptr);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, CustomExtensionOnlyHandler) {
  EnsureTlsSetup();

  // This installs an override that sends nothing but expects nonsense.
  SECStatus rv =
      SSL_InstallExtensionHooks(client_->ssl_fd(), 0xff6d, nullptr, nullptr,
                                NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(TlsConnectStreamTls13, CustomExtensionOverrunBuffer) {
  EnsureTlsSetup();
  // This doesn't actually overrun the buffer, but it says that it does.
  auto overrun_writer = [](PRFileDesc *fd, SSLHandshakeType message,
                           PRUint8 *data, unsigned int *len,
                           unsigned int maxLen, void *arg) -> PRBool {
    *len = maxLen + 1;
    return PR_TRUE;
  };
  SECStatus rv =
      SSL_InstallExtensionHooks(client_->ssl_fd(), 0xff71, overrun_writer,
                                nullptr, NoopExtensionHandler, nullptr);
  EXPECT_EQ(SECSuccess, rv);
  client_->StartConnect();
  client_->Handshake();
  client_->CheckErrorCode(SEC_ERROR_APPLICATION_CALLBACK_ERROR);
}

}  // namespace "nss_test"
