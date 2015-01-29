/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_OPENSSLADAPTER_H__
#define WEBRTC_BASE_OPENSSLADAPTER_H__

#include <string>
#include "webrtc/base/ssladapter.h"

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_store_ctx_st X509_STORE_CTX;

namespace rtc {

///////////////////////////////////////////////////////////////////////////////

class OpenSSLAdapter : public SSLAdapter {
public:
  static bool InitializeSSL(VerificationCallback callback);
  static bool InitializeSSLThread();
  static bool CleanupSSL();

  OpenSSLAdapter(AsyncSocket* socket);
  virtual ~OpenSSLAdapter();

  virtual int StartSSL(const char* hostname, bool restartable);
  virtual int Send(const void* pv, size_t cb);
  virtual int Recv(void* pv, size_t cb);
  virtual int Close();

  // Note that the socket returns ST_CONNECTING while SSL is being negotiated.
  virtual ConnState GetState() const;

protected:
  virtual void OnConnectEvent(AsyncSocket* socket);
  virtual void OnReadEvent(AsyncSocket* socket);
  virtual void OnWriteEvent(AsyncSocket* socket);
  virtual void OnCloseEvent(AsyncSocket* socket, int err);

private:
  enum SSLState {
    SSL_NONE, SSL_WAIT, SSL_CONNECTING, SSL_CONNECTED, SSL_ERROR
  };

  int BeginSSL();
  int ContinueSSL();
  void Error(const char* context, int err, bool signal = true);
  void Cleanup();

  static bool VerifyServerName(SSL* ssl, const char* host,
                               bool ignore_bad_cert);
  bool SSLPostConnectionCheck(SSL* ssl, const char* host);
#if _DEBUG
  static void SSLInfoCallback(const SSL* s, int where, int ret);
#endif  // !_DEBUG
  static int SSLVerifyCallback(int ok, X509_STORE_CTX* store);
  static VerificationCallback custom_verify_callback_;
  friend class OpenSSLStreamAdapter;  // for custom_verify_callback_;

  static bool ConfigureTrustedRootCertificates(SSL_CTX* ctx);
  static SSL_CTX* SetupSSLContext();

  SSLState state_;
  bool ssl_read_needs_write_;
  bool ssl_write_needs_read_;
  // If true, socket will retain SSL configuration after Close.
  bool restartable_;

  SSL* ssl_;
  SSL_CTX* ssl_ctx_;
  std::string ssl_host_name_;

  bool custom_verification_succeeded_;
};

/////////////////////////////////////////////////////////////////////////////

} // namespace rtc

#endif // WEBRTC_BASE_OPENSSLADAPTER_H__
