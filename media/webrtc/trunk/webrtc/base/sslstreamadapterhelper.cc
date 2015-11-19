/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <vector>

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "webrtc/base/sslstreamadapterhelper.h"

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stream.h"

namespace rtc {

SSLStreamAdapterHelper::SSLStreamAdapterHelper(StreamInterface* stream)
    : SSLStreamAdapter(stream),
      state_(SSL_NONE),
      role_(SSL_CLIENT),
      ssl_error_code_(0),  // Not meaningful yet
      ssl_mode_(SSL_MODE_TLS) {
}

SSLStreamAdapterHelper::~SSLStreamAdapterHelper() = default;

void SSLStreamAdapterHelper::SetIdentity(SSLIdentity* identity) {
  ASSERT(identity_.get() == NULL);
  identity_.reset(identity);
}

void SSLStreamAdapterHelper::SetServerRole(SSLRole role) {
  role_ = role;
}

int SSLStreamAdapterHelper::StartSSLWithServer(const char* server_name) {
  ASSERT(server_name != NULL && server_name[0] != '\0');
  ssl_server_name_ = server_name;
  return StartSSL();
}

int SSLStreamAdapterHelper::StartSSLWithPeer() {
  ASSERT(ssl_server_name_.empty());
  // It is permitted to specify peer_certificate_ only later.
  return StartSSL();
}

void SSLStreamAdapterHelper::SetMode(SSLMode mode) {
  ASSERT(state_ == SSL_NONE);
  ssl_mode_ = mode;
}

StreamState SSLStreamAdapterHelper::GetState() const {
  switch (state_) {
    case SSL_WAIT:
    case SSL_CONNECTING:
      return SS_OPENING;
    case SSL_CONNECTED:
      return SS_OPEN;
    default:
      return SS_CLOSED;
  };
  // not reached
}

bool SSLStreamAdapterHelper::GetPeerCertificate(SSLCertificate** cert) const {
  if (!peer_certificate_)
    return false;

  *cert = peer_certificate_->GetReference();
  return true;
}

bool SSLStreamAdapterHelper::SetPeerCertificateDigest(
    const std::string &digest_alg,
    const unsigned char* digest_val,
    size_t digest_len) {
  ASSERT(peer_certificate_.get() == NULL);
  ASSERT(peer_certificate_digest_algorithm_.empty());
  ASSERT(ssl_server_name_.empty());
  size_t expected_len;

  if (!GetDigestLength(digest_alg, &expected_len)) {
    LOG(LS_WARNING) << "Unknown digest algorithm: " << digest_alg;
    return false;
  }
  if (expected_len != digest_len)
    return false;

  peer_certificate_digest_value_.SetData(digest_val, digest_len);
  peer_certificate_digest_algorithm_ = digest_alg;

  return true;
}

void SSLStreamAdapterHelper::Error(const char* context, int err, bool signal) {
  LOG(LS_WARNING) << "SSLStreamAdapterHelper::Error("
                  << context << ", " << err << "," << signal << ")";
  state_ = SSL_ERROR;
  ssl_error_code_ = err;
  Cleanup();
  if (signal)
    StreamAdapterInterface::OnEvent(stream(), SE_CLOSE, err);
}

void SSLStreamAdapterHelper::Close() {
  Cleanup();
  ASSERT(state_ == SSL_CLOSED || state_ == SSL_ERROR);
  StreamAdapterInterface::Close();
}

int SSLStreamAdapterHelper::StartSSL() {
  ASSERT(state_ == SSL_NONE);

  if (StreamAdapterInterface::GetState() != SS_OPEN) {
    state_ = SSL_WAIT;
    return 0;
  }

  state_ = SSL_CONNECTING;
  int err = BeginSSL();
  if (err) {
    Error("BeginSSL", err, false);
    return err;
  }

  return 0;
}

}  // namespace rtc

