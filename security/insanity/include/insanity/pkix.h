/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
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

#ifndef insanity_pkix__pkix_h
#define insanity_pkix__pkix_h

#include "pkixtypes.h"
#include "prtime.h"

namespace insanity { namespace pkix {

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
//    * initial-any-policy-inhibit = true
//
// Because we force explicit policy and because we prohibit policy mapping, we
// do not bother processing the policy mapping, policy constraint, or inhibit
// anyPolicy extensions.
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
//    1. Active distrust (SEC_ERROR_UNTRUSTED_CERT).
//    2. Problems with issuer-independent properties other than
//       notBefore/notAfter.
//    3. For CA certificates: Expiration.
//    4. Unknown issuer (SEC_ERROR_UNKNOWN_ISSUER).
//    5. For end-entity certificates: Expiration.
//    6. Revocation.
//
// In particular, if BuildCertChain returns SEC_ERROR_UNKNOWN_ISSUER then the
// caller can call CERT_CheckCertValidTimes to determine if the certificate is
// ALSO expired.
//
// It would be better if revocation were prioritized above expiration and
// unknown issuer. However, it is impossible to do revocation checking without
// knowing the issuer, since the issuer information is needed to validate the
// revocation information. Also, generally revocation checking only works
// during the validity period of the certificate.
//
// In general, when path building fails, BuildCertChain will return
// SEC_ERROR_UNKNOWN_ISSUER. However, if all attempted paths resulted in the
// same error (which is trivially true when there is only one potential path),
// more specific errors will be returned.
//
// ----------------------------------------------------------------------------
// Meaning of specific error codes
//
// SEC_ERROR_UNTRUSTED_CERT means that the end-entity certificate was actively
//                          distrusted.
// SEC_ERROR_UNTRUSTED_ISSUER means that path building failed because of active
//                            distrust.
// TODO(bug 968451): Document more of these.

SECStatus BuildCertChain(TrustDomain& trustDomain,
                         CERTCertificate* cert,
                         PRTime time,
                         EndEntityOrCA endEntityOrCA,
            /*optional*/ KeyUsages requiredKeyUsagesIfPresent,
            /*optional*/ SECOidTag requiredEKUIfPresent,
            /*optional*/ SECOidTag requiredPolicy,
            /*optional*/ const SECItem* stapledOCSPResponse,
                 /*out*/ ScopedCERTCertList& results);

// Verify the given signed data using the public key of the given certificate.
// (EC)DSA parameter inheritance is not supported.
SECStatus VerifySignedData(const CERTSignedData* sd,
                           const CERTCertificate* cert,
                           void* pkcs11PinArg);

// The return value, if non-null, is owned by the arena and MUST NOT be freed.
SECItem* CreateEncodedOCSPRequest(PLArenaPool* arena,
                                  const CERTCertificate* cert,
                                  const CERTCertificate* issuerCert);

SECStatus VerifyEncodedOCSPResponse(TrustDomain& trustDomain,
                                    const CERTCertificate* cert,
                                    CERTCertificate* issuerCert,
                                    PRTime time,
                                    const SECItem* encodedResponse);

} } // namespace insanity::pkix

#endif // insanity_pkix__pkix_h
