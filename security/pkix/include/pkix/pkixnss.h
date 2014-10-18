/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__pkixnss_h
#define mozilla_pkix__pkixnss_h

#include "pkixtypes.h"
#include "prerror.h"
#include "seccomon.h"

namespace mozilla { namespace pkix {

// Verify the given signed data using the given public key.
Result VerifySignedData(const SignedDataWithSignature& sd,
                        Input subjectPublicKeyInfo,
                        unsigned int minimumNonECCBits,
                        void* pkcs11PinArg);

// Computes the SHA-1 hash of the data in the current item.
//
// item contains the data to hash.
// digestBuf must point to a buffer to where the SHA-1 hash will be written.
// digestBufLen must be 20 (the length of a SHA-1 hash,
//              TrustDomain::DIGEST_LENGTH).
//
// TODO(bug 966856): Add SHA-2 support
// TODO: Taking the output buffer as (uint8_t*, size_t) is counter to our
// other, extensive, memory safety efforts in mozilla::pkix, and we should find
// a way to provide a more-obviously-safe interface.
Result DigestBuf(Input item, /*out*/ uint8_t* digestBuf,
                 size_t digestBufLen);

// Checks, for RSA keys and DSA keys, that the modulus is at least the given
// number of bits.
Result CheckPublicKey(Input subjectPublicKeyInfo,
                      unsigned int minimumNonECCBits);

Result MapPRErrorCodeToResult(PRErrorCode errorCode);
PRErrorCode MapResultToPRErrorCode(Result result);

// The error codes within each module must fit in 16 bits. We want these
// errors to fit in the same module as the NSS errors but not overlap with
// any of them. Converting an NSS SEC, NSS SSL, or PSM error to an NS error
// involves negating the value of the error and then synthesizing an error
// in the NS_ERROR_MODULE_SECURITY module. Hence, PSM errors will start at
// a negative value that both doesn't overlap with the current value
// ranges for NSS errors and that will fit in 16 bits when negated.
static const PRErrorCode ERROR_BASE = -0x4000;
static const PRErrorCode ERROR_LIMIT = ERROR_BASE + 1000;

enum ErrorCode {
  MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE = ERROR_BASE + 0,
  MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY = ERROR_BASE + 1,
  MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE = ERROR_BASE + 2,
  MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA = ERROR_BASE + 3,
};

void RegisterErrorTable();

inline SECItem UnsafeMapInputToSECItem(Input input)
{
  SECItem result = {
    siBuffer,
    const_cast<uint8_t*>(input.UnsafeGetData()),
    input.GetLength()
  };
  return result;
}

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixnss_h
