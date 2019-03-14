/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctime>

#include "secerr.h"
#include "ssl.h"

#include "gtest_utils.h"
#include "tls_agent.h"
#include "tls_connect.h"

namespace nss_test {

static const char* kDummySni("dummy.invalid");

std::vector<uint16_t> kDefaultSuites = {TLS_AES_256_GCM_SHA384,
                                        TLS_AES_128_GCM_SHA256};
std::vector<uint16_t> kChaChaSuite = {TLS_CHACHA20_POLY1305_SHA256};
std::vector<uint16_t> kBogusSuites = {0};
std::vector<uint16_t> kTls12Suites = {
    TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256};

static void NamedGroup2ECParams(SSLNamedGroup group, SECItem* params) {
  auto groupDef = ssl_LookupNamedGroup(group);
  ASSERT_NE(nullptr, groupDef);

  auto oidData = SECOID_FindOIDByTag(groupDef->oidTag);
  ASSERT_NE(nullptr, oidData);
  ASSERT_NE(nullptr,
            SECITEM_AllocItem(nullptr, params, (2 + oidData->oid.len)));

  /*
   * params->data needs to contain the ASN encoding of an object ID (OID)
   * representing the named curve. The actual OID is in
   * oidData->oid.data so we simply prepend 0x06 and OID length
   */
  params->data[0] = SEC_ASN1_OBJECT_ID;
  params->data[1] = oidData->oid.len;
  memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);
}

/* Checksum is a 4-byte array. */
static void UpdateEsniKeysChecksum(DataBuffer* buf) {
  SECStatus rv;
  PRUint8 sha256[32];

  /* Stomp the checksum. */
  PORT_Memset(buf->data() + 2, 0, 4);

  rv = PK11_HashBuf(ssl3_HashTypeToOID(ssl_hash_sha256), sha256, buf->data(),
                    buf->len());
  ASSERT_EQ(SECSuccess, rv);
  buf->Write(2, sha256, 4);
}

static void GenerateEsniKey(time_t windowStart, SSLNamedGroup group,
                            std::vector<uint16_t>& cipher_suites,
                            DataBuffer* record,
                            ScopedSECKEYPublicKey* pubKey = nullptr,
                            ScopedSECKEYPrivateKey* privKey = nullptr) {
  SECKEYECParams ecParams = {siBuffer, NULL, 0};
  NamedGroup2ECParams(group, &ecParams);

  SECKEYPublicKey* pub = nullptr;
  SECKEYPrivateKey* priv = SECKEY_CreateECPrivateKey(&ecParams, &pub, nullptr);
  ASSERT_NE(nullptr, priv);
  SECITEM_FreeItem(&ecParams, PR_FALSE);
  PRUint8 encoded[1024];
  unsigned int encoded_len;

  SECStatus rv = SSL_EncodeESNIKeys(
      &cipher_suites[0], cipher_suites.size(), group, pub, 100, windowStart,
      windowStart + 10, encoded, &encoded_len, sizeof(encoded));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_GT(encoded_len, 0U);

  if (pubKey) {
    pubKey->reset(pub);
  } else {
    SECKEY_DestroyPublicKey(pub);
  }
  if (privKey) {
    privKey->reset(priv);
  } else {
    SECKEY_DestroyPrivateKey(priv);
  }
  record->Truncate(0);
  record->Write(0, encoded, encoded_len);
}

static void SetupEsni(const std::shared_ptr<TlsAgent>& client,
                      const std::shared_ptr<TlsAgent>& server,
                      SSLNamedGroup group = ssl_grp_ec_curve25519) {
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer record;

  GenerateEsniKey(time(nullptr), ssl_grp_ec_curve25519, kDefaultSuites, &record,
                  &pub, &priv);
  SECStatus rv = SSL_SetESNIKeyPair(server->ssl_fd(), priv.get(), record.data(),
                                    record.len());
  ASSERT_EQ(SECSuccess, rv);

  rv = SSL_EnableESNI(client->ssl_fd(), record.data(), record.len(), kDummySni);
  ASSERT_EQ(SECSuccess, rv);
}

static void CheckSniExtension(const DataBuffer& data) {
  TlsParser parser(data.data(), data.len());
  uint32_t tmp;
  ASSERT_TRUE(parser.Read(&tmp, 2));
  ASSERT_EQ(parser.remaining(), tmp);
  ASSERT_TRUE(parser.Read(&tmp, 1));
  ASSERT_EQ(0U, tmp); /* sni_nametype_hostname */
  DataBuffer name;
  ASSERT_TRUE(parser.ReadVariable(&name, 2));
  ASSERT_EQ(0U, parser.remaining());
  DataBuffer expected(reinterpret_cast<const uint8_t*>(kDummySni),
                      strlen(kDummySni));
  ASSERT_EQ(expected, name);
}

static void ClientInstallEsni(std::shared_ptr<TlsAgent>& agent,
                              const DataBuffer& record, PRErrorCode err = 0) {
  SECStatus rv =
      SSL_EnableESNI(agent->ssl_fd(), record.data(), record.len(), kDummySni);
  if (err == 0) {
    ASSERT_EQ(SECSuccess, rv);
  } else {
    ASSERT_EQ(SECFailure, rv);
    ASSERT_EQ(err, PORT_GetError());
  }
}

TEST_P(TlsAgentTestClient13, EsniInstall) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  ClientInstallEsni(agent_, record);
}

// The next set of tests fail at setup time.
TEST_P(TlsAgentTestClient13, EsniInvalidHash) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  record.data()[2]++;
  ClientInstallEsni(agent_, record, SSL_ERROR_RX_MALFORMED_ESNI_KEYS);
}

TEST_P(TlsAgentTestClient13, EsniInvalidVersion) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  record.Write(0, 0xffff, 2);
  ClientInstallEsni(agent_, record, SSL_ERROR_UNSUPPORTED_VERSION);
}

TEST_P(TlsAgentTestClient13, EsniShort) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  record.Truncate(record.len() - 1);
  UpdateEsniKeysChecksum(&record);
  ClientInstallEsni(agent_, record, SSL_ERROR_RX_MALFORMED_ESNI_KEYS);
}

TEST_P(TlsAgentTestClient13, EsniLong) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  record.Write(record.len(), 1, 1);
  UpdateEsniKeysChecksum(&record);
  ClientInstallEsni(agent_, record, SSL_ERROR_RX_MALFORMED_ESNI_KEYS);
}

TEST_P(TlsAgentTestClient13, EsniExtensionMismatch) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  record.Write(record.len() - 1, 1, 1);
  UpdateEsniKeysChecksum(&record);
  ClientInstallEsni(agent_, record, SSL_ERROR_RX_MALFORMED_ESNI_KEYS);
}

// The following tests fail by ignoring the Esni block.
TEST_P(TlsAgentTestClient13, EsniUnknownGroup) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  record.Write(8, 0xffff, 2);  // Fake group
  UpdateEsniKeysChecksum(&record);
  ClientInstallEsni(agent_, record, 0);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(agent_, ssl_tls13_encrypted_sni_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(!filter->captured());
}

TEST_P(TlsAgentTestClient13, EsniUnknownCS) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kBogusSuites, &record);
  ClientInstallEsni(agent_, record, 0);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(agent_, ssl_tls13_encrypted_sni_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(!filter->captured());
}

TEST_P(TlsAgentTestClient13, EsniInvalidCS) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kTls12Suites, &record);
  UpdateEsniKeysChecksum(&record);
  ClientInstallEsni(agent_, record, 0);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(agent_, ssl_tls13_encrypted_sni_xtn);
  agent_->Handshake();
  ASSERT_EQ(TlsAgent::STATE_CONNECTING, agent_->state());
  ASSERT_TRUE(!filter->captured());
}

TEST_P(TlsAgentTestClient13, EsniNotReady) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0) + 1000, ssl_grp_ec_curve25519, kDefaultSuites,
                  &record);
  ClientInstallEsni(agent_, record, 0);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(agent_, ssl_tls13_encrypted_sni_xtn);
  agent_->Handshake();
  ASSERT_TRUE(!filter->captured());
}

TEST_P(TlsAgentTestClient13, EsniExpired) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0) - 1000, ssl_grp_ec_curve25519, kDefaultSuites,
                  &record);
  ClientInstallEsni(agent_, record, 0);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(agent_, ssl_tls13_encrypted_sni_xtn);
  agent_->Handshake();
  ASSERT_TRUE(!filter->captured());
}

TEST_P(TlsAgentTestClient13, NoSniSoNoEsni) {
  EnsureInit();
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  SSL_SetURL(agent_->ssl_fd(), "");
  ClientInstallEsni(agent_, record, 0);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(agent_, ssl_tls13_encrypted_sni_xtn);
  agent_->Handshake();
  ASSERT_TRUE(!filter->captured());
}

static int32_t SniCallback(TlsAgent* agent, const SECItem* srvNameAddr,
                           PRUint32 srvNameArrSize) {
  EXPECT_EQ(1U, srvNameArrSize);
  SECItem expected = {
      siBuffer, reinterpret_cast<unsigned char*>(const_cast<char*>("server")),
      6};
  EXPECT_TRUE(!SECITEM_CompareItem(&expected, &srvNameAddr[0]));
  return SECSuccess;
}

TEST_P(TlsConnectTls13, ConnectEsni) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  auto cFilterSni =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  auto cFilterEsni =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_tls13_encrypted_sni_xtn);
  client_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit({cFilterSni, cFilterEsni})));
  auto sfilter =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_server_name_xtn);
  sfilter->EnableDecryption();
  server_->SetSniCallback(SniCallback);
  Connect();
  CheckSniExtension(cFilterSni->extension());
  ASSERT_TRUE(cFilterEsni->captured());
  // Check that our most preferred suite got chosen.
  uint32_t suite;
  ASSERT_TRUE(cFilterEsni->extension().Read(0, 2, &suite));
  ASSERT_EQ(TLS_AES_128_GCM_SHA256, static_cast<PRUint16>(suite));
  ASSERT_TRUE(!sfilter->captured());
}

TEST_P(TlsConnectTls13, ConnectEsniHrr) {
  EnsureTlsSetup();
  const std::vector<SSLNamedGroup> groups = {ssl_grp_ec_secp384r1};
  server_->ConfigNamedGroups(groups);
  SetupEsni(client_, server_);
  auto hrr_capture = MakeTlsFilter<TlsHandshakeRecorder>(
      server_, kTlsHandshakeHelloRetryRequest);
  auto filter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  auto filter2 =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn, true);
  client_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit({filter, filter2})));
  server_->SetSniCallback(SniCallback);
  Connect();
  CheckSniExtension(filter->extension());
  CheckSniExtension(filter2->extension());
  EXPECT_NE(0UL, hrr_capture->buffer().len());
}

TEST_P(TlsConnectTls13, ConnectEsniNoDummy) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer record;

  GenerateEsniKey(time(nullptr), ssl_grp_ec_curve25519, kDefaultSuites, &record,
                  &pub, &priv);
  SECStatus rv = SSL_SetESNIKeyPair(server_->ssl_fd(), priv.get(),
                                    record.data(), record.len());
  ASSERT_EQ(SECSuccess, rv);
  rv = SSL_EnableESNI(client_->ssl_fd(), record.data(), record.len(), "");
  ASSERT_EQ(SECSuccess, rv);

  auto cfilter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  auto sfilter =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_server_name_xtn);
  server_->SetSniCallback(SniCallback);
  Connect();
  ASSERT_TRUE(!cfilter->captured());
  ASSERT_TRUE(!sfilter->captured());
}

TEST_P(TlsConnectTls13, ConnectEsniNullDummy) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer record;

  GenerateEsniKey(time(nullptr), ssl_grp_ec_curve25519, kDefaultSuites, &record,
                  &pub, &priv);
  SECStatus rv = SSL_SetESNIKeyPair(server_->ssl_fd(), priv.get(),
                                    record.data(), record.len());
  ASSERT_EQ(SECSuccess, rv);
  rv = SSL_EnableESNI(client_->ssl_fd(), record.data(), record.len(), nullptr);
  ASSERT_EQ(SECSuccess, rv);

  auto cfilter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  auto sfilter =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_server_name_xtn);
  server_->SetSniCallback(SniCallback);
  Connect();
  ASSERT_TRUE(!cfilter->captured());
  ASSERT_TRUE(!sfilter->captured());
}

/* Tell the client that it supports AES but the server that it supports ChaCha
 */
TEST_P(TlsConnectTls13, ConnectEsniCSMismatch) {
  EnsureTlsSetup();
  ScopedSECKEYPublicKey pub;
  ScopedSECKEYPrivateKey priv;
  DataBuffer record;

  GenerateEsniKey(time(nullptr), ssl_grp_ec_curve25519, kDefaultSuites, &record,
                  &pub, &priv);
  PRUint8 encoded[1024];
  unsigned int encoded_len;

  SECStatus rv = SSL_EncodeESNIKeys(
      &kChaChaSuite[0], kChaChaSuite.size(), ssl_grp_ec_curve25519, pub.get(),
      100, time(0), time(0) + 10, encoded, &encoded_len, sizeof(encoded));
  rv = SSL_SetESNIKeyPair(server_->ssl_fd(), priv.get(), encoded, encoded_len);
  ASSERT_EQ(SECSuccess, rv);
  rv = SSL_EnableESNI(client_->ssl_fd(), record.data(), record.len(), "");
  ASSERT_EQ(SECSuccess, rv);
  ConnectExpectAlert(server_, illegal_parameter);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_P(TlsConnectTls13, ConnectEsniP256) {
  EnsureTlsSetup();
  SetupEsni(client_, server_, ssl_grp_ec_secp256r1);
  auto cfilter =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_server_name_xtn);
  auto sfilter =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_server_name_xtn);
  server_->SetSniCallback(SniCallback);
  Connect();
  CheckSniExtension(cfilter->extension());
  ASSERT_TRUE(!sfilter->captured());
}

TEST_P(TlsConnectTls13, ConnectMismatchedEsniKeys) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  // Now install a new set of keys on the client, so we have a mismatch.
  DataBuffer record;
  GenerateEsniKey(time(0), ssl_grp_ec_curve25519, kDefaultSuites, &record);
  ClientInstallEsni(client_, record, 0);
  ConnectExpectAlert(server_, illegal_parameter);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_P(TlsConnectTls13, ConnectDamagedEsniExtensionCH) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  auto filter = MakeTlsFilter<TlsExtensionDamager>(
      client_, ssl_tls13_encrypted_sni_xtn, 50);  // in the ciphertext
  ConnectExpectAlert(server_, illegal_parameter);
  server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_CLIENT_HELLO);
}

TEST_P(TlsConnectTls13, ConnectRemoveEsniExtensionEE) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  auto filter =
      MakeTlsFilter<TlsExtensionDropper>(server_, ssl_tls13_encrypted_sni_xtn);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, missing_extension);
  client_->CheckErrorCode(SSL_ERROR_MISSING_ESNI_EXTENSION);
}

TEST_P(TlsConnectTls13, ConnectShortEsniExtensionEE) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  DataBuffer shortNonce;
  auto filter = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_tls13_encrypted_sni_xtn, shortNonce);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, illegal_parameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION);
}

TEST_P(TlsConnectTls13, ConnectBogusEsniExtensionEE) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  const uint8_t bogusNonceBuf[16] = {0};
  DataBuffer bogusNonce(bogusNonceBuf, sizeof(bogusNonceBuf));
  auto filter = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_tls13_encrypted_sni_xtn, bogusNonce);
  filter->EnableDecryption();
  ConnectExpectAlert(client_, illegal_parameter);
  client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_ESNI_EXTENSION);
}

// ESNI is a commitment to doing TLS 1.3 or above.
// The TLS 1.2 server ignores ESNI and processes the dummy SNI.
// The client then aborts when it sees the server did TLS 1.2.
TEST_P(TlsConnectTls13, EsniButTLS12Server) {
  EnsureTlsSetup();
  SetupEsni(client_, server_);
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  ConnectExpectAlert(client_, kTlsAlertProtocolVersion);
  client_->CheckErrorCode(SSL_ERROR_UNSUPPORTED_VERSION);
  server_->CheckErrorCode(SSL_ERROR_PROTOCOL_VERSION_ALERT);
  ASSERT_FALSE(SSLInt_ExtensionNegotiated(server_->ssl_fd(),
                                          ssl_tls13_encrypted_sni_xtn));
}
}
