/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef dtls_identity_h__
#define dtls_identity_h__

#include <string>

#include "m_cpp_utils.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "sslt.h"
#include "ScopedNSSTypes.h"

// All code in this module requires NSS to be live.
// Callers must initialize NSS and implement the nsNSSShutdownObject
// protocol.
namespace mozilla {

class DtlsIdentity final {
 public:
  // This constructor takes ownership of privkey and cert.
  DtlsIdentity(SECKEYPrivateKey *privkey,
               CERTCertificate *cert,
               SSLKEAType authType)
      : private_key_(privkey), cert_(cert), auth_type_(authType) {}

  // This is only for use in tests, or for external linkage.  It makes a (bad)
  // instance of this class.
  static RefPtr<DtlsIdentity> Generate();

  // These don't create copies or transfer ownership. If you want these to live
  // on, make a copy.
  const UniqueCERTCertificate& cert() const { return cert_; }
  SECKEYPrivateKey *privkey() const { return private_key_; }
  // Note: this uses SSLKEAType because that is what the libssl API requires.
  // This is a giant confusing mess, but libssl indexes certificates based on a
  // key exchange type, not authentication type (as you might have reasonably
  // expected).
  SSLKEAType auth_type() const { return auth_type_; }

  nsresult ComputeFingerprint(const std::string algorithm,
                              uint8_t *digest,
                              size_t size,
                              size_t *digest_length) const;
  static nsresult ComputeFingerprint(const UniqueCERTCertificate& cert,
                                     const std::string algorithm,
                                     uint8_t *digest,
                                     size_t size,
                                     size_t *digest_length);

  static const std::string DEFAULT_HASH_ALGORITHM;
  enum {
    HASH_ALGORITHM_MAX_LENGTH = 64
  };

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DtlsIdentity)

 private:
  ~DtlsIdentity() {}
  DISALLOW_COPY_ASSIGN(DtlsIdentity);

  ScopedSECKEYPrivateKey private_key_;
  UniqueCERTCertificate cert_;
  SSLKEAType auth_type_;
};
}  // close namespace
#endif
