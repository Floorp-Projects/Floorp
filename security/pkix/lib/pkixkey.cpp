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

#include "pkix/pkix.h"

#include <limits>
#include <stdint.h>

#include "cert.h"
#include "cryptohi.h"
#include "prerror.h"
#include "secerr.h"

namespace mozilla { namespace pkix {

SECStatus
VerifySignedData(const CERTSignedData* sd, const SECItem& subjectPublicKeyInfo,
                 void* pkcs11PinArg)
{
  if (!sd || !sd->data.data || !sd->signatureAlgorithm.algorithm.data ||
      !sd->signature.data) {
    PR_NOT_REACHED("invalid args to VerifySignedData");
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  // See bug 921585.
  if (sd->data.len > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  // convert sig->len from bit counts to byte count.
  SECItem sig = sd->signature;
  DER_ConvertBitString(&sig);

  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(&subjectPublicKeyInfo));
  if (!spki) {
    return SECFailure;
  }
  ScopedPtr<SECKEYPublicKey, SECKEY_DestroyPublicKey>
    pubKey(SECKEY_ExtractPublicKey(spki.get()));
  if (!pubKey) {
    return SECFailure;
  }

  SECOidTag hashAlg;
  if (VFY_VerifyDataWithAlgorithmID(sd->data.data, static_cast<int>(sd->data.len),
                                    pubKey.get(), &sig, &sd->signatureAlgorithm,
                                    &hashAlg, pkcs11PinArg) != SECSuccess) {
    return SECFailure;
  }

  // TODO: Ideally, we would do this check before we call
  // VFY_VerifyDataWithAlgorithmID. But, VFY_VerifyDataWithAlgorithmID gives us
  // the hash algorithm so it is more convenient to do things in this order.
  uint32_t policy;
  if (NSS_GetAlgorithmPolicy(hashAlg, &policy) != SECSuccess) {
    return SECFailure;
  }

  // XXX: I'm not sure why there isn't NSS_USE_ALG_IN_SSL_SIGNATURE, but there
  // isn't. Since we don't know the context in which we're being called, be as
  // strict as we can be given the NSS API that is available.
  static const uint32_t requiredPolicy = NSS_USE_ALG_IN_CERT_SIGNATURE |
                                         NSS_USE_ALG_IN_CMS_SIGNATURE;
  if ((policy & requiredPolicy) != requiredPolicy) {
    PR_SetError(SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED, 0);
    return SECFailure;
  }

  return SECSuccess;
}

} } // namespace mozilla::pkix
