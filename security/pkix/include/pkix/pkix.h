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

#ifndef mozilla_pkix__pkix_h
#define mozilla_pkix__pkix_h

#include "pkixtypes.h"

namespace mozilla { namespace pkix {

// ----------------------------------------------------------------------------
// LIMITED SUPPORT FOR CERTIFICATE POLICIES
//
// If SEC_OID_X509_ANY_POLICY is passed as the value of the requiredPolicy
// parameter then all policy validation will be skipped. Otherwise, path
// building and validation will be done for the given policy.
//
// In RFC 5280 terms:
//
//    * user-initial-policy-set = { requiredPolicy }.
//    * initial-explicit-policy = true
//    * initial-any-policy-inhibit = false
//
// We allow intermediate cerificates to use this extension but since
// we do not process the inhibit anyPolicy extesion we will fail if this
// extension is present. TODO(bug 989051)
// Because we force explicit policy and because we prohibit policy mapping, we
// do not bother processing the policy mapping, or policy constraint.
//
// ----------------------------------------------------------------------------
// ERROR RANKING
//
// BuildCertChain prioritizes certain checks ahead of others so that when a
// certificate chain has multiple errors, the "most serious" error is
// returned. In practice, this ranking of seriousness is tied directly to how
// Firefox's certificate error override mechanism.
//
// The ranking is:
//
//    1. Active distrust (Result::ERROR_UNTRUSTED_CERT).
//    2. Problems with issuer-independent properties for CA certificates.
//    3. Unknown issuer (Result::ERROR_UNKNOWN_ISSUER).
//    4. Problems with issuer-independent properties for EE certificates.
//    5. Revocation.
//
// In particular, if BuildCertChain returns Result::ERROR_UNKNOWN_ISSUER then
// the caller can call CERT_CheckCertValidTimes to determine if the certificate
// is ALSO expired.
//
// It would be better if revocation were prioritized above expiration and
// unknown issuer. However, it is impossible to do revocation checking without
// knowing the issuer, since the issuer information is needed to validate the
// revocation information. Also, generally revocation checking only works
// during the validity period of the certificate.
//
// In general, when path building fails, BuildCertChain will return
// Result::ERROR_UNKNOWN_ISSUER. However, if all attempted paths resulted in
// the same error (which is trivially true when there is only one potential
// path), more specific errors will be returned.
//
// ----------------------------------------------------------------------------
// Meaning of specific error codes
//
// Result::ERROR_UNTRUSTED_CERT means that the end-entity certificate was
//                              actively distrusted.
// Result::SEC_ERROR_UNTRUSTED_ISSUER means that path building failed because
//                                    of active distrust.
// TODO(bug 968451): Document more of these.

Result BuildCertChain(TrustDomain& trustDomain, Input cert,
                      Time time, EndEntityOrCA endEntityOrCA,
                      KeyUsage requiredKeyUsageIfPresent,
                      KeyPurposeId requiredEKUIfPresent,
                      const CertPolicyId& requiredPolicy,
                      /*optional*/ const Input* stapledOCSPResponse);

static const size_t OCSP_REQUEST_MAX_LENGTH = 127;
Result CreateEncodedOCSPRequest(TrustDomain& trustDomain,
                                const struct CertID& certID,
                                /*out*/ uint8_t (&out)[OCSP_REQUEST_MAX_LENGTH],
                                /*out*/ size_t& outLen);

// The out parameter expired will be true if the response has expired. If the
// response also indicates a revoked or unknown certificate, that error
// will be returned. Otherwise, REsult::ERROR_OCSP_OLD_RESPONSE will be
// returned for an expired response.
//
// The optional parameter thisUpdate will be the thisUpdate value of
// the encoded response if it is considered trustworthy. Only
// good, unknown, or revoked responses that verify correctly are considered
// trustworthy. If the response is not trustworthy, thisUpdate will be 0.
// Similarly, the optional parameter validThrough will be the time through
// which the encoded response is considered trustworthy (that is, if a response had a
// thisUpdate time of validThrough, it would be considered trustworthy).
Result VerifyEncodedOCSPResponse(TrustDomain& trustDomain,
                                 const CertID& certID, Time time,
                                 uint16_t maxLifetimeInDays,
                                 Input encodedResponse,
                       /* out */ bool& expired,
              /* optional out */ Time* thisUpdate = nullptr,
              /* optional out */ Time* validThrough = nullptr);

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkix_h
