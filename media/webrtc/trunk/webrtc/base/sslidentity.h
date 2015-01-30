/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.

#ifndef WEBRTC_BASE_SSLIDENTITY_H_
#define WEBRTC_BASE_SSLIDENTITY_H_

#include <algorithm>
#include <string>
#include <vector>

#include "webrtc/base/buffer.h"
#include "webrtc/base/messagedigest.h"

namespace rtc {

// Forward declaration due to circular dependency with SSLCertificate.
class SSLCertChain;

// Abstract interface overridden by SSL library specific
// implementations.

// A somewhat opaque type used to encapsulate a certificate.
// Wraps the SSL library's notion of a certificate, with reference counting.
// The SSLCertificate object is pretty much immutable once created.
// (The OpenSSL implementation only does reference counting and
// possibly caching of intermediate results.)
class SSLCertificate {
 public:
  // Parses and build a certificate from a PEM encoded string.
  // Returns NULL on failure.
  // The length of the string representation of the certificate is
  // stored in *pem_length if it is non-NULL, and only if
  // parsing was successful.
  // Caller is responsible for freeing the returned object.
  static SSLCertificate* FromPEMString(const std::string& pem_string);
  virtual ~SSLCertificate() {}

  // Returns a new SSLCertificate object instance wrapping the same
  // underlying certificate, including its chain if present.
  // Caller is responsible for freeing the returned object.
  virtual SSLCertificate* GetReference() const = 0;

  // Provides the cert chain, or returns false.  The caller owns the chain.
  // The chain includes a copy of each certificate, excluding the leaf.
  virtual bool GetChain(SSLCertChain** chain) const = 0;

  // Returns a PEM encoded string representation of the certificate.
  virtual std::string ToPEMString() const = 0;

  // Provides a DER encoded binary representation of the certificate.
  virtual void ToDER(Buffer* der_buffer) const = 0;

  // Gets the name of the digest algorithm that was used to compute this
  // certificate's signature.
  virtual bool GetSignatureDigestAlgorithm(std::string* algorithm) const = 0;

  // Compute the digest of the certificate given algorithm
  virtual bool ComputeDigest(const std::string& algorithm,
                             unsigned char* digest,
                             size_t size,
                             size_t* length) const = 0;
};

// SSLCertChain is a simple wrapper for a vector of SSLCertificates. It serves
// primarily to ensure proper memory management (especially deletion) of the
// SSLCertificate pointers.
class SSLCertChain {
 public:
  // These constructors copy the provided SSLCertificate(s), so the caller
  // retains ownership.
  explicit SSLCertChain(const std::vector<SSLCertificate*>& certs) {
    ASSERT(!certs.empty());
    certs_.resize(certs.size());
    std::transform(certs.begin(), certs.end(), certs_.begin(), DupCert);
  }
  explicit SSLCertChain(const SSLCertificate* cert) {
    certs_.push_back(cert->GetReference());
  }

  ~SSLCertChain() {
    std::for_each(certs_.begin(), certs_.end(), DeleteCert);
  }

  // Vector access methods.
  size_t GetSize() const { return certs_.size(); }

  // Returns a temporary reference, only valid until the chain is destroyed.
  const SSLCertificate& Get(size_t pos) const { return *(certs_[pos]); }

  // Returns a new SSLCertChain object instance wrapping the same underlying
  // certificate chain.  Caller is responsible for freeing the returned object.
  SSLCertChain* Copy() const {
    return new SSLCertChain(certs_);
  }

 private:
  // Helper function for duplicating a vector of certificates.
  static SSLCertificate* DupCert(const SSLCertificate* cert) {
    return cert->GetReference();
  }

  // Helper function for deleting a vector of certificates.
  static void DeleteCert(SSLCertificate* cert) { delete cert; }

  std::vector<SSLCertificate*> certs_;

  DISALLOW_COPY_AND_ASSIGN(SSLCertChain);
};

// Parameters for generating an identity for testing. If common_name is
// non-empty, it will be used for the certificate's subject and issuer name,
// otherwise a random string will be used. |not_before| and |not_after| are
// offsets to the current time in number of seconds.
struct SSLIdentityParams {
  std::string common_name;
  int not_before;  // in seconds.
  int not_after;  // in seconds.
};

// Our identity in an SSL negotiation: a keypair and certificate (both
// with the same public key).
// This too is pretty much immutable once created.
class SSLIdentity {
 public:
  // Generates an identity (keypair and self-signed certificate). If
  // common_name is non-empty, it will be used for the certificate's
  // subject and issuer name, otherwise a random string will be used.
  // Returns NULL on failure.
  // Caller is responsible for freeing the returned object.
  static SSLIdentity* Generate(const std::string& common_name);

  // Generates an identity with the specified validity period.
  static SSLIdentity* GenerateForTest(const SSLIdentityParams& params);

  // Construct an identity from a private key and a certificate.
  static SSLIdentity* FromPEMStrings(const std::string& private_key,
                                     const std::string& certificate);

  virtual ~SSLIdentity() {}

  // Returns a new SSLIdentity object instance wrapping the same
  // identity information.
  // Caller is responsible for freeing the returned object.
  virtual SSLIdentity* GetReference() const = 0;

  // Returns a temporary reference to the certificate.
  virtual const SSLCertificate& certificate() const = 0;

  // Helpers for parsing converting between PEM and DER format.
  static bool PemToDer(const std::string& pem_type,
                       const std::string& pem_string,
                       std::string* der);
  static std::string DerToPem(const std::string& pem_type,
                              const unsigned char* data,
                              size_t length);
};

extern const char kPemTypeCertificate[];
extern const char kPemTypeRsaPrivateKey[];

}  // namespace rtc

#endif  // WEBRTC_BASE_SSLIDENTITY_H_
