/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecl-priv.h"

/* Returns 2^e as an integer. This is meant to be used for small powers of
 * two. */
int
ec_twoTo(int e)
{
    int a = 1;
    int i;

    for (i = 0; i < e; i++) {
        a *= 2;
    }
    return a;
}

/* Computes the windowed non-adjacent-form (NAF) of a scalar. Out should
 * be an array of signed char's to output to, bitsize should be the number
 * of bits of out, in is the original scalar, and w is the window size.
 * NAF is discussed in the paper: D. Hankerson, J. Hernandez and A.
 * Menezes, "Software implementation of elliptic curve cryptography over
 * binary fields", Proc. CHES 2000. */
mp_err
ec_compute_wNAF(signed char *out, int bitsize, const mp_int *in, int w)
{
    mp_int k;
    mp_err res = MP_OKAY;
    int i, twowm1, mask;

    twowm1 = ec_twoTo(w - 1);
    mask = 2 * twowm1 - 1;

    MP_DIGITS(&k) = 0;
    MP_CHECKOK(mp_init_copy(&k, in));

    i = 0;
    /* Compute wNAF form */
    while (mp_cmp_z(&k) > 0) {
        if (mp_isodd(&k)) {
            out[i] = MP_DIGIT(&k, 0) & mask;
            if (out[i] >= twowm1)
                out[i] -= 2 * twowm1;

            /* Subtract off out[i].  Note mp_sub_d only works with
             * unsigned digits */
            if (out[i] >= 0) {
                MP_CHECKOK(mp_sub_d(&k, out[i], &k));
            } else {
                MP_CHECKOK(mp_add_d(&k, -(out[i]), &k));
            }
        } else {
            out[i] = 0;
        }
        MP_CHECKOK(mp_div_2(&k, &k));
        i++;
    }
    /* Zero out the remaining elements of the out array. */
    for (; i < bitsize + 1; i++) {
        out[i] = 0;
    }
CLEANUP:
    mp_clear(&k);
    return res;
}
