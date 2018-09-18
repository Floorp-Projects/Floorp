/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __RAND_H__
#define __RAND_H__

#include <mpi.h>
#include <seccomon.h>
#include <stdlib.h>

/*
 * Typedef for function pointer. A function pointer of type RandBytesFunc
 * points to a function that fills the buffer `out` of with `len` random bytes.
 */
typedef SECStatus (*RandBytesFunc)(void* user_data, unsigned char* out,
                                   size_t len);

/*
 * Initialize or cleanup the global random number generator
 * state that NSS uses.
 */
SECStatus rand_init(void);
void rand_clear(void);

/*
 * Generate the specified number of random bytes using the
 * NSS random number generator.
 */
SECStatus rand_bytes(unsigned char* out, size_t n_bytes);

/*
 * Generate a random number x such that
 *    0 <= x < max
 * using the NSS random number generator.
 */
SECStatus rand_int(mp_int* out, const mp_int* max);

/*
 * Generate a random number x such that
 *    0 <= x < max
 * using the specified randomness generator.
 *
 * The pointer user_data is passed to RandBytesFung `rng` as a first
 * argument.
 */
SECStatus rand_int_rng(mp_int* out, const mp_int* max, RandBytesFunc rng,
                       void* user_data);

#endif /* __RAND_H__ */
