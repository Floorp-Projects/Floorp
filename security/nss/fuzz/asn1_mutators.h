/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef asn1_mutators_h__
#define asn1_mutators_h__

#include <stdint.h>
#include <cstddef>

size_t ASN1MutatorFlipConstructed(uint8_t *Data, size_t Size, size_t MaxSize,
                                  unsigned int Seed);
size_t ASN1MutatorChangeType(uint8_t *Data, size_t Size, size_t MaxSize,
                             unsigned int Seed);

#endif  // asn1_mutators_h__
