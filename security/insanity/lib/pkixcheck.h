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

#ifndef insanity__pkixcheck_h
#define insanity__pkixcheck_h

#include "pkixutil.h"
#include "certt.h"

namespace insanity { namespace pkix {

Result CheckIssuerIndependentProperties(
          TrustDomain& trustDomain,
          BackCert& cert,
          PRTime time,
          EndEntityOrCA endEntityOrCA,
          KeyUsages requiredKeyUsagesIfPresent,
          SECOidTag requiredEKUIfPresent,
          SECOidTag requiredPolicy,
          unsigned int subCACount,
          /*optional out*/ TrustDomain::TrustLevel* trustLevel = nullptr);

Result CheckNameConstraints(BackCert& cert);

} } // namespace insanity::pkix

#endif // insanity__pkixcheck_h
