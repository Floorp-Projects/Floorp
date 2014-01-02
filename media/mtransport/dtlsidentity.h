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
#include "ScopedNSSTypes.h"

// All code in this module requires NSS to be live.
// Callers must initialize NSS and implement the nsNSSShutdownObject
// protocol.
namespace mozilla {

class DtlsIdentity {
 public:
  ~DtlsIdentity();

  // Generate an identity with a random name.
  static TemporaryRef<DtlsIdentity> Generate();

  // Note: the following two functions just provide access. They
  // do not transfer ownership. If you want a pointer that lasts
  // past the lifetime of the DtlsIdentity, you must make
  // a copy yourself.
  CERTCertificate *cert() { return cert_; }
  SECKEYPrivateKey *privkey() { return privkey_; }

  std::string GetFormattedFingerprint(const std::string &algorithm = DEFAULT_HASH_ALGORITHM);

  nsresult ComputeFingerprint(const std::string algorithm,
                              unsigned char *digest,
                              std::size_t size,
                              std::size_t *digest_length);

  static nsresult ComputeFingerprint(const CERTCertificate *cert,
                                     const std::string algorithm,
                                     unsigned char *digest,
                                     std::size_t size,
                                     std::size_t *digest_length);

  static nsresult ParseFingerprint(const std::string fp,
                                   unsigned char *digest,
                                   size_t size, size_t *length);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DtlsIdentity)

 private:
  DtlsIdentity(SECKEYPrivateKey *privkey, CERTCertificate *cert)
      : privkey_(privkey), cert_(cert) {}
  DISALLOW_COPY_ASSIGN(DtlsIdentity);

  static const std::string DEFAULT_HASH_ALGORITHM;
  static const size_t HASH_ALGORITHM_MAX_LENGTH;

  std::string FormatFingerprint(const unsigned char *digest,
                                std::size_t size);

  ScopedSECKEYPrivateKey privkey_;
  CERTCertificate *cert_;  // TODO: Using a smart pointer here causes link
                           // errors.
};
}  // close namespace
#endif
