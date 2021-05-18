/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_pkix_pkixc_h
#define mozilla_pkix_pkixc_h

#include "prerror.h"
#include "stdint.h"

// VerifyCertificateChain will attempt to build a verified certificate chain
// starting from the 0th certificate in the given array to the indicated trust
// anchor. It returns true on success and false otherwise. No particular key
// usage is required, and no particular policy is required. The code signing
// extended key usage is required.  No revocation checking is performed. RSA
// keys must be at least 2048 bits long, and EC keys must be from one of the
// curves secp256r1, secp384r1, or secp521r1. Only SHA256, SHA384, and SHA512
// are acceptable digest algorithms. When doing name checking, the subject
// common name field is ignored.
// certificate is an array of pointers to certificates.
// certificateLengths is an array of the lengths of each certificate.
// numCertificates indicates how many certificates are in certificates.
// secondsSinceEpoch indicates the time at which the certificate chain must be
// valid, in seconds since the epoch.
// rootSHA256Hash identifies a trust anchor by the SHA256 hash of its contents.
// It must be an array of 32 bytes.
// hostname is a doman name for which the end-entity certificate must be valid.
// error will be set if and only if the return value is false. Its value may
// indicate why verification failed.

#ifdef __cplusplus
extern "C" {
#endif
bool VerifyCodeSigningCertificateChain(const uint8_t** certificates,
                                       const uint16_t* certificateLengths,
                                       size_t numCertificates,
                                       uint64_t secondsSinceEpoch,
                                       const uint8_t* rootSHA256Hash,
                                       const uint8_t* hostname,
                                       size_t hostnameLength,
                                       /* out */ PRErrorCode* error);
#ifdef __cplusplus
}
#endif

#endif  // mozilla_pkix_pkixc_h
