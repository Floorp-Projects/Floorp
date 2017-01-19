/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzerInternal.h"
#include "FuzzerRandom.h"
#include "asn1_mutators.h"
#include "shared.h"

extern const uint16_t DEFAULT_MAX_LENGTH = 3072U;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  CERTCertificate cert;
  QuickDERDecode(&cert, SEC_SignedCertificateTemplate, Data, Size);
  return 0;
}

ADD_CUSTOM_MUTATORS({&ASN1MutatorFlipConstructed, &ASN1MutatorChangeType})
