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

SECStatus BuildCertChain(TrustDomain& trustDomain,
                         CERTCertificate* cert,
                         PRTime time,
            /*optional*/ KeyUsages requiredKeyUsagesIfPresent,
                 /*out*/ ScopedCERTCertList& results);

// Verify the given signed data using the public key of the given certificate.
// (EC)DSA parameter inheritance is not supported.
SECStatus VerifySignedData(const CERTSignedData* sd,
                           const CERTCertificate* cert,
                           void* pkcs11PinArg);

} } // namespace insanity::pkix

#endif // insanity_pkix__pkix_h
