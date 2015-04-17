/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_agent.h"

#include "pk11func.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "keyhi.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

namespace nss_test {

const char* TlsAgent::states[] = {"INIT", "CONNECTING", "CONNECTED", "ERROR"};

bool TlsAgent::EnsureTlsSetup() {
  // Don't set up twice
  if (ssl_fd_) return true;

  if (adapter_->mode() == STREAM) {
    ssl_fd_ = SSL_ImportFD(nullptr, pr_fd_);
  } else {
    ssl_fd_ = DTLS_ImportFD(nullptr, pr_fd_);
  }

  EXPECT_NE(nullptr, ssl_fd_);
  if (!ssl_fd_) return false;
  pr_fd_ = nullptr;

  if (role_ == SERVER) {
    CERTCertificate* cert = PK11_FindCertFromNickname(name_.c_str(), nullptr);
    EXPECT_NE(nullptr, cert);
    if (!cert) return false;

    SECKEYPrivateKey* priv = PK11_FindKeyByAnyCert(cert, nullptr);
    EXPECT_NE(nullptr, priv);
    if (!priv) return false;  // Leak cert.

    SECStatus rv = SSL_ConfigSecureServer(ssl_fd_, cert, priv, kea_);
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) return false;  // Leak cert and key.

    SECKEY_DestroyPrivateKey(priv);
    CERT_DestroyCertificate(cert);

    rv = SSL_SNISocketConfigHook(ssl_fd_, SniHook,
                                 reinterpret_cast<void*>(this));
    EXPECT_EQ(SECSuccess, rv);  // don't abort, just fail
  } else {
    SECStatus rv = SSL_SetURL(ssl_fd_, "server");
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) return false;
  }

  SECStatus rv = SSL_VersionRangeSet(ssl_fd_, &vrange_);
  EXPECT_EQ(SECSuccess, rv);
  if (rv != SECSuccess) return false;

  rv = SSL_AuthCertificateHook(ssl_fd_, AuthCertificateHook,
                               reinterpret_cast<void*>(this));
  EXPECT_EQ(SECSuccess, rv);
  if (rv != SECSuccess) return false;

  return true;
}

void TlsAgent::StartConnect() {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv;
  rv = SSL_ResetHandshake(ssl_fd_, role_ == SERVER ? PR_TRUE : PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
  SetState(CONNECTING);
}

void TlsAgent::EnableSomeEcdheCiphers() {
  EXPECT_TRUE(EnsureTlsSetup());

  const uint32_t EcdheCiphers[] = {TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
                                   TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
                                   TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                   TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA};

  for (size_t i = 0; i < PR_ARRAY_SIZE(EcdheCiphers); ++i) {
    SECStatus rv = SSL_CipherPrefSet(ssl_fd_, EcdheCiphers[i], PR_TRUE);
    EXPECT_EQ(SECSuccess, rv);
  }
}

void TlsAgent::SetSessionTicketsEnabled(bool en) {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_ENABLE_SESSION_TICKETS,
                               en ? PR_TRUE : PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
}

void TlsAgent::SetSessionCacheEnabled(bool en) {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_NO_CACHE,
                               en ? PR_FALSE : PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);
}

void TlsAgent::SetVersionRange(uint16_t minver, uint16_t maxver) {
   vrange_.min = minver;
   vrange_.max = maxver;

   if (ssl_fd_) {
     SECStatus rv = SSL_VersionRangeSet(ssl_fd_, &vrange_);
     EXPECT_EQ(SECSuccess, rv);
   }
}

void TlsAgent::CheckKEAType(SSLKEAType type) const {
  EXPECT_EQ(CONNECTED, state_);
  EXPECT_EQ(type, csinfo_.keaType);
}

void TlsAgent::CheckAuthType(SSLAuthType type) const {
  EXPECT_EQ(CONNECTED, state_);
  EXPECT_EQ(type, csinfo_.authAlgorithm);
}

void TlsAgent::CheckVersion(uint16_t version) const {
  EXPECT_EQ(CONNECTED, state_);
  EXPECT_EQ(version, info_.protocolVersion);
}

void TlsAgent::EnableAlpn(const uint8_t* val, size_t len) {
  EXPECT_TRUE(EnsureTlsSetup());

  EXPECT_EQ(SECSuccess, SSL_OptionSet(ssl_fd_, SSL_ENABLE_ALPN, PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_SetNextProtoNego(ssl_fd_, val, len));
}

void TlsAgent::CheckAlpn(SSLNextProtoState expected_state,
                         const std::string& expected) {
  SSLNextProtoState state;
  char chosen[10];
  unsigned int chosen_len;
  SECStatus rv = SSL_GetNextProto(ssl_fd_, &state,
                                  reinterpret_cast<unsigned char*>(chosen),
                                  &chosen_len, sizeof(chosen));
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected_state, state);
  EXPECT_EQ(expected, std::string(chosen, chosen_len));
}

void TlsAgent::EnableSrtp() {
  EXPECT_TRUE(EnsureTlsSetup());
  const uint16_t ciphers[] = {
    SRTP_AES128_CM_HMAC_SHA1_80, SRTP_AES128_CM_HMAC_SHA1_32
  };
  EXPECT_EQ(SECSuccess, SSL_SetSRTPCiphers(ssl_fd_, ciphers,
                                           PR_ARRAY_SIZE(ciphers)));

}

void TlsAgent::CheckSrtp() {
  uint16_t actual;
  EXPECT_EQ(SECSuccess, SSL_GetSRTPCipher(ssl_fd_, &actual));
  EXPECT_EQ(SRTP_AES128_CM_HMAC_SHA1_80, actual);
}

void TlsAgent::CheckErrorCode(int32_t expected) const {
  EXPECT_EQ(ERROR, state_);
  EXPECT_EQ(expected, error_code_);
}

void TlsAgent::Handshake() {
  SECStatus rv = SSL_ForceHandshake(ssl_fd_);
  if (rv == SECSuccess) {
    LOG("Handshake success");
    SECStatus rv = SSL_GetChannelInfo(ssl_fd_, &info_, sizeof(info_));
    EXPECT_EQ(SECSuccess, rv);

    rv = SSL_GetCipherSuiteInfo(info_.cipherSuite, &csinfo_, sizeof(csinfo_));
    EXPECT_EQ(SECSuccess, rv);

    SetState(CONNECTED);
    return;
  }

  int32_t err = PR_GetError();
  switch (err) {
    case PR_WOULD_BLOCK_ERROR:
      LOG("Would have blocked");
      // TODO(ekr@rtfm.com): set DTLS timeouts
      Poller::Instance()->Wait(READABLE_EVENT, adapter_, this,
                               &TlsAgent::ReadableCallback);
      return;
      break;

      // TODO(ekr@rtfm.com): needs special case for DTLS
    case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
    default:
      LOG("Handshake failed with error " << err);
      error_code_ = err;
      SetState(ERROR);
      return;
  }
}

void TlsAgent::ConfigureSessionCache(SessionResumptionMode mode) {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_,
                               SSL_NO_CACHE,
                               mode & RESUME_SESSIONID ?
                               PR_FALSE : PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);

  rv = SSL_OptionSet(ssl_fd_,
                     SSL_ENABLE_SESSION_TICKETS,
                     mode & RESUME_TICKET ?
                     PR_TRUE : PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
}


} // namespace nss_test
