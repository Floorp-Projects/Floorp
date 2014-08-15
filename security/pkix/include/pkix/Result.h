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

#ifndef mozilla_pkix__Result_h
#define mozilla_pkix__Result_h

#include <cassert>

#include "pkix/enumclass.h"

namespace mozilla { namespace pkix {

static const unsigned int FATAL_ERROR_FLAG = 0x800;

MOZILLA_PKIX_ENUM_CLASS Result
{
  Success = 0,

  ERROR_BAD_DER = 1,
  ERROR_CA_CERT_INVALID = 2,
  ERROR_BAD_SIGNATURE = 3,
  ERROR_CERT_BAD_ACCESS_LOCATION = 4,
  ERROR_CERT_NOT_IN_NAME_SPACE = 5,
  ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED = 6,
  ERROR_CONNECT_REFUSED = 7,
  ERROR_EXPIRED_CERTIFICATE = 8,
  ERROR_EXTENSION_VALUE_INVALID = 9,
  ERROR_INADEQUATE_CERT_TYPE = 10,
  ERROR_INADEQUATE_KEY_USAGE = 11,
  ERROR_INVALID_ALGORITHM = 12,
  ERROR_INVALID_TIME = 13,
  ERROR_KEY_PINNING_FAILURE = 14,
  ERROR_PATH_LEN_CONSTRAINT_INVALID = 15,
  ERROR_POLICY_VALIDATION_FAILED = 16,
  ERROR_REVOKED_CERTIFICATE = 17,
  ERROR_UNKNOWN_CRITICAL_EXTENSION = 18,
  ERROR_UNKNOWN_ISSUER = 19,
  ERROR_UNTRUSTED_CERT = 20,
  ERROR_UNTRUSTED_ISSUER = 21,

  ERROR_OCSP_BAD_SIGNATURE = 22,
  ERROR_OCSP_INVALID_SIGNING_CERT = 23,
  ERROR_OCSP_MALFORMED_REQUEST = 24,
  ERROR_OCSP_MALFORMED_RESPONSE = 25,
  ERROR_OCSP_OLD_RESPONSE = 26,
  ERROR_OCSP_REQUEST_NEEDS_SIG = 27,
  ERROR_OCSP_RESPONDER_CERT_INVALID = 28,
  ERROR_OCSP_SERVER_ERROR = 29,
  ERROR_OCSP_TRY_SERVER_LATER = 30,
  ERROR_OCSP_UNAUTHORIZED_REQUEST = 31,
  ERROR_OCSP_UNKNOWN_RESPONSE_STATUS = 32,
  ERROR_OCSP_UNKNOWN_CERT = 33,
  ERROR_OCSP_FUTURE_RESPONSE = 34,

  ERROR_UNKNOWN_ERROR = 35,
  ERROR_INVALID_KEY = 36,
  ERROR_UNSUPPORTED_KEYALG = 37,
  ERROR_EXPIRED_ISSUER_CERTIFICATE = 38,
  ERROR_CA_CERT_USED_AS_END_ENTITY = 39,
  ERROR_INADEQUATE_KEY_SIZE = 40,

  // Keep this in sync with MAP_LIST in pkixnss.cpp

  FATAL_ERROR_INVALID_ARGS = FATAL_ERROR_FLAG | 1,
  FATAL_ERROR_INVALID_STATE = FATAL_ERROR_FLAG | 2,
  FATAL_ERROR_LIBRARY_FAILURE = FATAL_ERROR_FLAG | 3,
  FATAL_ERROR_NO_MEMORY = FATAL_ERROR_FLAG | 4,

  // Keep this in sync with MAP_LIST in pkixnss.cpp
};

// We write many comparisons as (x != Success), and this shortened name makes
// those comparisons clearer, especially because the shortened name often
// results in less line wrapping.
//
// Visual Studio before VS2013 does not support "enum class," so
// Result::Success will already be visible in this scope, and compilation will
// fail if we try to define a variable with that name here.
#if !defined(_MSC_VER) || (_MSC_VER >= 1700)
static const Result Success = Result::Success;
#endif

inline bool
IsFatalError(Result rv)
{
  return static_cast<unsigned int>(rv) & FATAL_ERROR_FLAG;
}

inline Result
NotReached(const char* /*explanation*/, Result result)
{
  assert(false);
  return result;
}

} } // namespace mozilla::pkix

#endif // mozilla_pkix__Result_h
