/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_SSLSTREAMADAPTERHELPER_H_
#define WEBRTC_BASE_SSLSTREAMADAPTERHELPER_H_

#include <string>
#include <vector>

#include "webrtc/base/buffer.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/sslstreamadapter.h"

namespace rtc {

// SSLStreamAdapterHelper : A stream adapter which implements much
// of the logic that is common between the known implementations
// (NSS and OpenSSL)
class SSLStreamAdapterHelper : public SSLStreamAdapter {
 public:
  explicit SSLStreamAdapterHelper(StreamInterface* stream)
      : SSLStreamAdapter(stream),
        state_(SSL_NONE),
        role_(SSL_CLIENT),
        ssl_error_code_(0),  // Not meaningful yet
        ssl_mode_(SSL_MODE_TLS) {}


  // Overrides of SSLStreamAdapter
  virtual void SetIdentity(SSLIdentity* identity);
  virtual void SetServerRole(SSLRole role = SSL_SERVER);
  virtual void SetMode(SSLMode mode);

  virtual int StartSSLWithServer(const char* server_name);
  virtual int StartSSLWithPeer();

  virtual bool SetPeerCertificateDigest(const std::string& digest_alg,
                                        const unsigned char* digest_val,
                                        size_t digest_len);
  virtual bool GetPeerCertificate(SSLCertificate** cert) const;
  virtual StreamState GetState() const;
  virtual void Close();

 protected:
  // Internal helper methods
  // The following method returns 0 on success and a negative
  // error code on failure. The error code may be either -1 or
  // from the impl on some other error cases, so it can't really be
  // interpreted unfortunately.

  // Perform SSL negotiation steps.
  int ContinueSSL();

  // Error handler helper. signal is given as true for errors in
  // asynchronous contexts (when an error code was not returned
  // through some other method), and in that case an SE_CLOSE event is
  // raised on the stream with the specified error.
  // A 0 error means a graceful close, otherwise there is not really enough
  // context to interpret the error code.
  virtual void Error(const char* context, int err, bool signal);

  // Must be implemented by descendents
  virtual int BeginSSL() = 0;
  virtual void Cleanup() = 0;
  virtual bool GetDigestLength(const std::string& algorithm,
                               size_t* length) = 0;

  enum SSLState {
    // Before calling one of the StartSSL methods, data flows
    // in clear text.
    SSL_NONE,
    SSL_WAIT,  // waiting for the stream to open to start SSL negotiation
    SSL_CONNECTING,  // SSL negotiation in progress
    SSL_CONNECTED,  // SSL stream successfully established
    SSL_ERROR,  // some SSL error occurred, stream is closed
    SSL_CLOSED  // Clean close
  };

  // MSG_MAX is the maximum generic stream message number.
  enum { MSG_DTLS_TIMEOUT = MSG_MAX + 1 };

  SSLState state_;
  SSLRole role_;
  int ssl_error_code_;  // valid when state_ == SSL_ERROR

  // Our key and certificate, mostly useful in peer-to-peer mode.
  scoped_ptr<SSLIdentity> identity_;
  // in traditional mode, the server name that the server's certificate
  // must specify. Empty in peer-to-peer mode.
  std::string ssl_server_name_;
  // The peer's certificate. Only used for GetPeerCertificate.
  scoped_ptr<SSLCertificate> peer_certificate_;

  // The digest of the certificate that the peer must present.
  Buffer peer_certificate_digest_value_;
  std::string peer_certificate_digest_algorithm_;

  // Do DTLS or not
  SSLMode ssl_mode_;

 private:
  // Go from state SSL_NONE to either SSL_CONNECTING or SSL_WAIT,
  // depending on whether the underlying stream is already open or
  // not. Returns 0 on success and a negative value on error.
  int StartSSL();
};

}  // namespace rtc

#endif  // WEBRTC_BASE_SSLSTREAMADAPTERHELPER_H_
