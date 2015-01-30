/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_OPENSSLIDENTITY_H_
#define WEBRTC_BASE_OPENSSLIDENTITY_H_

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <string>

#include "webrtc/base/common.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sslidentity.h"

typedef struct ssl_ctx_st SSL_CTX;

namespace rtc {

// OpenSSLKeyPair encapsulates an OpenSSL EVP_PKEY* keypair object,
// which is reference counted inside the OpenSSL library.
class OpenSSLKeyPair {
 public:
  explicit OpenSSLKeyPair(EVP_PKEY* pkey) : pkey_(pkey) {
    ASSERT(pkey_ != NULL);
  }

  static OpenSSLKeyPair* Generate();

  virtual ~OpenSSLKeyPair();

  virtual OpenSSLKeyPair* GetReference() {
    AddReference();
    return new OpenSSLKeyPair(pkey_);
  }

  EVP_PKEY* pkey() const { return pkey_; }

 private:
  void AddReference();

  EVP_PKEY* pkey_;

  DISALLOW_EVIL_CONSTRUCTORS(OpenSSLKeyPair);
};

// OpenSSLCertificate encapsulates an OpenSSL X509* certificate object,
// which is also reference counted inside the OpenSSL library.
class OpenSSLCertificate : public SSLCertificate {
 public:
  // Caller retains ownership of the X509 object.
  explicit OpenSSLCertificate(X509* x509) : x509_(x509) {
    AddReference();
  }

  static OpenSSLCertificate* Generate(OpenSSLKeyPair* key_pair,
                                      const SSLIdentityParams& params);
  static OpenSSLCertificate* FromPEMString(const std::string& pem_string);

  virtual ~OpenSSLCertificate();

  virtual OpenSSLCertificate* GetReference() const {
    return new OpenSSLCertificate(x509_);
  }

  X509* x509() const { return x509_; }

  virtual std::string ToPEMString() const;

  virtual void ToDER(Buffer* der_buffer) const;

  // Compute the digest of the certificate given algorithm
  virtual bool ComputeDigest(const std::string& algorithm,
                             unsigned char* digest,
                             size_t size,
                             size_t* length) const;

  // Compute the digest of a certificate as an X509 *
  static bool ComputeDigest(const X509* x509,
                            const std::string& algorithm,
                            unsigned char* digest,
                            size_t size,
                            size_t* length);

  virtual bool GetSignatureDigestAlgorithm(std::string* algorithm) const;

  virtual bool GetChain(SSLCertChain** chain) const {
    // Chains are not yet supported when using OpenSSL.
    // OpenSSLStreamAdapter::SSLVerifyCallback currently requires the remote
    // certificate to be self-signed.
    return false;
  }

 private:
  void AddReference() const;

  X509* x509_;

  DISALLOW_EVIL_CONSTRUCTORS(OpenSSLCertificate);
};

// Holds a keypair and certificate together, and a method to generate
// them consistently.
class OpenSSLIdentity : public SSLIdentity {
 public:
  static OpenSSLIdentity* Generate(const std::string& common_name);
  static OpenSSLIdentity* GenerateForTest(const SSLIdentityParams& params);
  static SSLIdentity* FromPEMStrings(const std::string& private_key,
                                     const std::string& certificate);
  virtual ~OpenSSLIdentity() { }

  virtual const OpenSSLCertificate& certificate() const {
    return *certificate_;
  }

  virtual OpenSSLIdentity* GetReference() const {
    return new OpenSSLIdentity(key_pair_->GetReference(),
                               certificate_->GetReference());
  }

  // Configure an SSL context object to use our key and certificate.
  bool ConfigureIdentity(SSL_CTX* ctx);

 private:
  OpenSSLIdentity(OpenSSLKeyPair* key_pair,
                  OpenSSLCertificate* certificate)
      : key_pair_(key_pair), certificate_(certificate) {
    ASSERT(key_pair != NULL);
    ASSERT(certificate != NULL);
  }

  static OpenSSLIdentity* GenerateInternal(const SSLIdentityParams& params);

  scoped_ptr<OpenSSLKeyPair> key_pair_;
  scoped_ptr<OpenSSLCertificate> certificate_;

  DISALLOW_EVIL_CONSTRUCTORS(OpenSSLIdentity);
};


}  // namespace rtc

#endif  // WEBRTC_BASE_OPENSSLIDENTITY_H_
