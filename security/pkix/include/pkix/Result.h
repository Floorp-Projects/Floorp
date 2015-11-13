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

#ifndef mozilla_pkix_Result_h
#define mozilla_pkix_Result_h

#include <cassert>

namespace mozilla { namespace pkix {

static const unsigned int FATAL_ERROR_FLAG = 0x800;

// ----------------------------------------------------------------------------
// SELECTED ERROR CODE EXPLANATIONS
//
// Result::ERROR_UNTRUSTED_CERT
//         means that the end-entity certificate was actively distrusted.
// Result::ERROR_UNTRUSTED_ISSUER
//         means that path building failed because of active distrust.
// Result::ERROR_INVALID_DER_TIME
//         means the DER-encoded time was unexpected, such as being before the
//         UNIX epoch (allowed by X500, but not valid here).
// Result::ERROR_EXPIRED_CERTIFICATE
//         means the end entity certificate expired.
// Result::ERROR_EXPIRED_ISSUER_CERTIFICATE
//         means the CA certificate expired.
// Result::ERROR_UNKNOWN_ISSUER
//         means that the CA could not be found in the root store.
// Result::ERROR_POLICY_VALIDATION_FAILED
//         means that an encoded policy could not be applied or wasn't present
//         when expected. Usually this is in the context of Extended Validation.
// Result::ERROR_BAD_CERT_DOMAIN
//         means that the certificate's name couldn't be matched to the
//         reference identifier.
// Result::ERROR_CERT_NOT_IN_NAME_SPACE
//         typically means the certificate violates name constraints applied
//         by the issuer.
// Result::ERROR_BAD_DER
//         means the input was improperly encoded.
// Result::ERROR_UNKNOWN_ERROR
//         means that an external library (NSS) provided an error we didn't
//         anticipate. See the map below in Result.h to add new ones.
// Result::FATAL_ERROR_LIBRARY_FAILURE
//         is an unexpected fatal error indicating a library had an unexpected
//         failure, and we can't proceed.
// Result::FATAL_ERROR_INVALID_ARGS
//         means that we violated our own expectations on inputs and there's a
//         bug somewhere.
// Result::FATAL_ERROR_INVALID_STATE
//         means that we violated our own expectations on state and there's a
//         bug somewhere.
// Result::FATAL_ERROR_NO_MEMORY
//         means a memory allocation failed, prohibiting validation.
// ----------------------------------------------------------------------------

// The first argument to MOZILLA_PKIX_MAP() is used for building the mapping
// from error code to error name in MapResultToName.
//
// The second argument is for defining the value for the enum literal in the
// Result enum class.
//
// The third argument to MOZILLA_PKIX_MAP() is used, along with the first
// argument, for maintaining the mapping of mozilla::pkix error codes to
// NSS/NSPR error codes in pkixnss.cpp.
#define MOZILLA_PKIX_MAP_LIST \
    MOZILLA_PKIX_MAP(Success, 0, 0) \
    MOZILLA_PKIX_MAP(ERROR_BAD_DER, 1, \
                     SEC_ERROR_BAD_DER) \
    MOZILLA_PKIX_MAP(ERROR_CA_CERT_INVALID, 2, \
                     SEC_ERROR_CA_CERT_INVALID) \
    MOZILLA_PKIX_MAP(ERROR_BAD_SIGNATURE, 3, \
                     SEC_ERROR_BAD_SIGNATURE) \
    MOZILLA_PKIX_MAP(ERROR_CERT_BAD_ACCESS_LOCATION, 4, \
                     SEC_ERROR_CERT_BAD_ACCESS_LOCATION) \
    MOZILLA_PKIX_MAP(ERROR_CERT_NOT_IN_NAME_SPACE, 5, \
                     SEC_ERROR_CERT_NOT_IN_NAME_SPACE) \
    MOZILLA_PKIX_MAP(ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED, 6, \
                     SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED) \
    MOZILLA_PKIX_MAP(ERROR_CONNECT_REFUSED, 7, \
                     PR_CONNECT_REFUSED_ERROR) \
    MOZILLA_PKIX_MAP(ERROR_EXPIRED_CERTIFICATE, 8, \
                     SEC_ERROR_EXPIRED_CERTIFICATE) \
    MOZILLA_PKIX_MAP(ERROR_EXTENSION_VALUE_INVALID, 9, \
                     SEC_ERROR_EXTENSION_VALUE_INVALID) \
    MOZILLA_PKIX_MAP(ERROR_INADEQUATE_CERT_TYPE, 10, \
                     SEC_ERROR_INADEQUATE_CERT_TYPE) \
    MOZILLA_PKIX_MAP(ERROR_INADEQUATE_KEY_USAGE, 11, \
                     SEC_ERROR_INADEQUATE_KEY_USAGE) \
    MOZILLA_PKIX_MAP(ERROR_INVALID_ALGORITHM, 12, \
                     SEC_ERROR_INVALID_ALGORITHM) \
    MOZILLA_PKIX_MAP(ERROR_INVALID_DER_TIME, 13, \
                     SEC_ERROR_INVALID_TIME) \
    MOZILLA_PKIX_MAP(ERROR_KEY_PINNING_FAILURE, 14, \
                     MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE) \
    MOZILLA_PKIX_MAP(ERROR_PATH_LEN_CONSTRAINT_INVALID, 15, \
                     SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID) \
    MOZILLA_PKIX_MAP(ERROR_POLICY_VALIDATION_FAILED, 16, \
                     SEC_ERROR_POLICY_VALIDATION_FAILED) \
    MOZILLA_PKIX_MAP(ERROR_REVOKED_CERTIFICATE, 17, \
                     SEC_ERROR_REVOKED_CERTIFICATE) \
    MOZILLA_PKIX_MAP(ERROR_UNKNOWN_CRITICAL_EXTENSION, 18, \
                     SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION) \
    MOZILLA_PKIX_MAP(ERROR_UNKNOWN_ERROR, 19, \
                     PR_UNKNOWN_ERROR) \
    MOZILLA_PKIX_MAP(ERROR_UNKNOWN_ISSUER, 20, \
                     SEC_ERROR_UNKNOWN_ISSUER) \
    MOZILLA_PKIX_MAP(ERROR_UNTRUSTED_CERT, 21, \
                     SEC_ERROR_UNTRUSTED_CERT) \
    MOZILLA_PKIX_MAP(ERROR_UNTRUSTED_ISSUER, 22, \
                     SEC_ERROR_UNTRUSTED_ISSUER) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_BAD_SIGNATURE, 23, \
                     SEC_ERROR_OCSP_BAD_SIGNATURE) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_INVALID_SIGNING_CERT, 24, \
                     SEC_ERROR_OCSP_INVALID_SIGNING_CERT) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_MALFORMED_REQUEST, 25, \
                     SEC_ERROR_OCSP_MALFORMED_REQUEST) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_MALFORMED_RESPONSE, 26, \
                     SEC_ERROR_OCSP_MALFORMED_RESPONSE) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_OLD_RESPONSE, 27, \
                     SEC_ERROR_OCSP_OLD_RESPONSE) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_REQUEST_NEEDS_SIG, 28, \
                     SEC_ERROR_OCSP_REQUEST_NEEDS_SIG) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_RESPONDER_CERT_INVALID, 29, \
                     SEC_ERROR_OCSP_RESPONDER_CERT_INVALID) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_SERVER_ERROR, 30, \
                     SEC_ERROR_OCSP_SERVER_ERROR) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_TRY_SERVER_LATER, 31, \
                     SEC_ERROR_OCSP_TRY_SERVER_LATER) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_UNAUTHORIZED_REQUEST, 32, \
                     SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_UNKNOWN_RESPONSE_STATUS, 33, \
                     SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_UNKNOWN_CERT, 34, \
                     SEC_ERROR_OCSP_UNKNOWN_CERT) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_FUTURE_RESPONSE, 35, \
                     SEC_ERROR_OCSP_FUTURE_RESPONSE) \
    MOZILLA_PKIX_MAP(ERROR_INVALID_KEY, 36, \
                     SEC_ERROR_INVALID_KEY) \
    MOZILLA_PKIX_MAP(ERROR_UNSUPPORTED_KEYALG, 37, \
                     SEC_ERROR_UNSUPPORTED_KEYALG) \
    MOZILLA_PKIX_MAP(ERROR_EXPIRED_ISSUER_CERTIFICATE, 38, \
                     SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE) \
    MOZILLA_PKIX_MAP(ERROR_CA_CERT_USED_AS_END_ENTITY, 39, \
                     MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY) \
    MOZILLA_PKIX_MAP(ERROR_INADEQUATE_KEY_SIZE, 40, \
                     MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE) \
    MOZILLA_PKIX_MAP(ERROR_V1_CERT_USED_AS_CA, 41, \
                     MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA) \
    MOZILLA_PKIX_MAP(ERROR_BAD_CERT_DOMAIN, 42, \
                     SSL_ERROR_BAD_CERT_DOMAIN) \
    MOZILLA_PKIX_MAP(ERROR_NO_RFC822NAME_MATCH, 43, \
                     MOZILLA_PKIX_ERROR_NO_RFC822NAME_MATCH) \
    MOZILLA_PKIX_MAP(ERROR_UNSUPPORTED_ELLIPTIC_CURVE, 44, \
                     SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE) \
    MOZILLA_PKIX_MAP(ERROR_NOT_YET_VALID_CERTIFICATE, 45, \
                     MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE) \
    MOZILLA_PKIX_MAP(ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE, 46, \
                     MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE) \
    MOZILLA_PKIX_MAP(ERROR_UNSUPPORTED_EC_POINT_FORM, 47, \
                     SEC_ERROR_UNSUPPORTED_EC_POINT_FORM) \
    MOZILLA_PKIX_MAP(ERROR_SIGNATURE_ALGORITHM_MISMATCH, 48, \
                     MOZILLA_PKIX_ERROR_SIGNATURE_ALGORITHM_MISMATCH) \
    MOZILLA_PKIX_MAP(ERROR_OCSP_RESPONSE_FOR_CERT_MISSING, 49, \
                     MOZILLA_PKIX_ERROR_OCSP_RESPONSE_FOR_CERT_MISSING) \
    MOZILLA_PKIX_MAP(ERROR_VALIDITY_TOO_LONG, 50, \
                     MOZILLA_PKIX_ERROR_VALIDITY_TOO_LONG) \
    MOZILLA_PKIX_MAP(ERROR_REQUIRED_TLS_FEATURE_MISSING, 51, \
                     MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING) \
    MOZILLA_PKIX_MAP(FATAL_ERROR_INVALID_ARGS, FATAL_ERROR_FLAG | 1, \
                     SEC_ERROR_INVALID_ARGS) \
    MOZILLA_PKIX_MAP(FATAL_ERROR_INVALID_STATE, FATAL_ERROR_FLAG | 2, \
                     PR_INVALID_STATE_ERROR) \
    MOZILLA_PKIX_MAP(FATAL_ERROR_LIBRARY_FAILURE, FATAL_ERROR_FLAG | 3, \
                     SEC_ERROR_LIBRARY_FAILURE) \
    MOZILLA_PKIX_MAP(FATAL_ERROR_NO_MEMORY, FATAL_ERROR_FLAG | 4, \
                     SEC_ERROR_NO_MEMORY) \
    /* nothing here */

enum class Result
{
#define MOZILLA_PKIX_MAP(name, value, nss_name) name = value,
  MOZILLA_PKIX_MAP_LIST
#undef MOZILLA_PKIX_MAP
};

// Returns the stringified name of the given result, e.g. "Result::Success",
// or nullptr if result is unknown (invalid).
const char* MapResultToName(Result result);

// We write many comparisons as (x != Success), and this shortened name makes
// those comparisons clearer, especially because the shortened name often
// results in less line wrapping.
static const Result Success = Result::Success;

inline bool
IsFatalError(Result rv)
{
  return (static_cast<unsigned int>(rv) & FATAL_ERROR_FLAG) != 0;
}

inline Result
NotReached(const char* /*explanation*/, Result result)
{
  assert(false);
  return result;
}

} } // namespace mozilla::pkix

#endif // mozilla_pkix_Result_h
