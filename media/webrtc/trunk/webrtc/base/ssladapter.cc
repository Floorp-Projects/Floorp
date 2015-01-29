/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "webrtc/base/ssladapter.h"

#include "webrtc/base/sslconfig.h"

#if SSL_USE_SCHANNEL

#include "schanneladapter.h"

#elif SSL_USE_OPENSSL  // && !SSL_USE_SCHANNEL

#include "openssladapter.h"

#elif SSL_USE_NSS     // && !SSL_USE_CHANNEL && !SSL_USE_OPENSSL

#include "nssstreamadapter.h"

#endif  // SSL_USE_OPENSSL && !SSL_USE_SCHANNEL && !SSL_USE_NSS

///////////////////////////////////////////////////////////////////////////////

namespace rtc {

SSLAdapter*
SSLAdapter::Create(AsyncSocket* socket) {
#if SSL_USE_SCHANNEL
  return new SChannelAdapter(socket);
#elif SSL_USE_OPENSSL  // && !SSL_USE_SCHANNEL
  return new OpenSSLAdapter(socket);
#else  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL
  delete socket;
  return NULL;
#endif  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL
}

///////////////////////////////////////////////////////////////////////////////

#if SSL_USE_OPENSSL

bool InitializeSSL(VerificationCallback callback) {
  return OpenSSLAdapter::InitializeSSL(callback);
}

bool InitializeSSLThread() {
  return OpenSSLAdapter::InitializeSSLThread();
}

bool CleanupSSL() {
  return OpenSSLAdapter::CleanupSSL();
}

#elif SSL_USE_NSS  // !SSL_USE_OPENSSL

bool InitializeSSL(VerificationCallback callback) {
  return NSSContext::InitializeSSL(callback);
}

bool InitializeSSLThread() {
  return NSSContext::InitializeSSLThread();
}

bool CleanupSSL() {
  return NSSContext::CleanupSSL();
}

#else  // !SSL_USE_OPENSSL && !SSL_USE_NSS

bool InitializeSSL(VerificationCallback callback) {
  return true;
}

bool InitializeSSLThread() {
  return true;
}

bool CleanupSSL() {
  return true;
}

#endif  // !SSL_USE_OPENSSL && !SSL_USE_NSS

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc
