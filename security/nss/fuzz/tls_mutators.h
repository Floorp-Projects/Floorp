/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_mutators_h__
#define tls_mutators_h__

size_t TlsMutatorDropRecord(uint8_t *data, size_t size, size_t max_size,
                            unsigned int seed);
size_t TlsMutatorShuffleRecords(uint8_t *data, size_t size, size_t max_size,
                                unsigned int seed);
size_t TlsMutatorDuplicateRecord(uint8_t *data, size_t size, size_t max_size,
                                 unsigned int seed);
size_t TlsMutatorTruncateRecord(uint8_t *data, size_t size, size_t max_size,
                                unsigned int seed);
size_t TlsMutatorFragmentRecord(uint8_t *data, size_t size, size_t max_size,
                                unsigned int seed);

size_t TlsCrossOver(const uint8_t *data1, size_t size1, const uint8_t *data2,
                    size_t size2, uint8_t *out, size_t max_out_size,
                    unsigned int seed);

#endif  // tls_mutators_h__
