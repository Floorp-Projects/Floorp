/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __MPARRAY_H__
#define __MPARRAY_H__

#include <mpi.h>
#include <mprio.h>

struct mparray
{
  int len;
  mp_int* data;
};

typedef struct mparray* MPArray;
typedef const struct mparray* const_MPArray;

/*
 * Initialize an array of `mp_int`s of the given length.
 */
MPArray MPArray_new(int len);
void MPArray_clear(MPArray arr);

/*
 * Copies secret sharing of data from src into arrays
 * arrA and arrB. The lengths of the three input arrays
 * must be identical.
 */
SECStatus MPArray_set_share(MPArray arrA, MPArray arrB, const_MPArray src,
                            const_PrioConfig cfg);

/*
 * Initializes array with 0/1 values specified in boolean array `data_in`
 */
MPArray MPArray_new_bool(int len, const bool* data_in);

/*
 * Expands or shrinks the MPArray to the desired size. If shrinking,
 * will clear the values on the end of array.
 */
SECStatus MPArray_resize(MPArray arr, int newlen);

/*
 * Initializes dst and creates a duplicate of the array in src.
 */
MPArray MPArray_dup(const_MPArray src);

/*
 * Copies array from src to dst. Arrays must have the same length.
 */
SECStatus MPArray_copy(MPArray dst, const_MPArray src);

/* For each index i into the array, set:
 *    dst[i] = dst[i] + to_add[i]   (modulo mod)
 */
SECStatus MPArray_addmod(MPArray dst, const_MPArray to_add, const mp_int* mod);

/*
 * Return true iff the two arrays are equal in length
 * and contents. This comparison is NOT constant time.
 */
bool MPArray_areEqual(const_MPArray arr1, const_MPArray arr2);

#endif /* __MPARRAY_H__ */
