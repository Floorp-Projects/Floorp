/*-
 * MIT License
 * -
 * Copyright (c) 2020 Luis Rivera-Zamarripa, Jesús-Javier Chi-Domínguez, Billy Bob Brumley
 * -
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * -
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * -
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#undef RADIX
#include "ecp.h"
#include "ecp_secp384r1.h"
#include "mpi-priv.h"
#include "mplogic.h"

/*-
 * reverse bytes -- total hack
 */
#define MP_BE2LE(a)            \
    do {                       \
        unsigned char z_bswap; \
        z_bswap = a[0];        \
        a[0] = a[47];          \
        a[47] = z_bswap;       \
        z_bswap = a[1];        \
        a[1] = a[46];          \
        a[46] = z_bswap;       \
        z_bswap = a[2];        \
        a[2] = a[45];          \
        a[45] = z_bswap;       \
        z_bswap = a[3];        \
        a[3] = a[44];          \
        a[44] = z_bswap;       \
        z_bswap = a[4];        \
        a[4] = a[43];          \
        a[43] = z_bswap;       \
        z_bswap = a[5];        \
        a[5] = a[42];          \
        a[42] = z_bswap;       \
        z_bswap = a[6];        \
        a[6] = a[41];          \
        a[41] = z_bswap;       \
        z_bswap = a[7];        \
        a[7] = a[40];          \
        a[40] = z_bswap;       \
        z_bswap = a[8];        \
        a[8] = a[39];          \
        a[39] = z_bswap;       \
        z_bswap = a[9];        \
        a[9] = a[38];          \
        a[38] = z_bswap;       \
        z_bswap = a[10];       \
        a[10] = a[37];         \
        a[37] = z_bswap;       \
        z_bswap = a[11];       \
        a[11] = a[36];         \
        a[36] = z_bswap;       \
        z_bswap = a[12];       \
        a[12] = a[35];         \
        a[35] = z_bswap;       \
        z_bswap = a[13];       \
        a[13] = a[34];         \
        a[34] = z_bswap;       \
        z_bswap = a[14];       \
        a[14] = a[33];         \
        a[33] = z_bswap;       \
        z_bswap = a[15];       \
        a[15] = a[32];         \
        a[32] = z_bswap;       \
        z_bswap = a[16];       \
        a[16] = a[31];         \
        a[31] = z_bswap;       \
        z_bswap = a[17];       \
        a[17] = a[30];         \
        a[30] = z_bswap;       \
        z_bswap = a[18];       \
        a[18] = a[29];         \
        a[29] = z_bswap;       \
        z_bswap = a[19];       \
        a[19] = a[28];         \
        a[28] = z_bswap;       \
        z_bswap = a[20];       \
        a[20] = a[27];         \
        a[27] = z_bswap;       \
        z_bswap = a[21];       \
        a[21] = a[26];         \
        a[26] = z_bswap;       \
        z_bswap = a[22];       \
        a[22] = a[25];         \
        a[25] = z_bswap;       \
        z_bswap = a[23];       \
        a[23] = a[24];         \
        a[24] = z_bswap;       \
    } while (0)

static mp_err
point_mul_g_secp384r1_wrap(const mp_int *n, mp_int *out_x,
                           mp_int *out_y, const ECGroup *group)
{
    unsigned char b_x[48];
    unsigned char b_y[48];
    unsigned char b_n[48];
    mp_err res;

    ARGCHK(n != NULL && out_x != NULL && out_y != NULL, MP_BADARG);

    /* fail on out of range scalars */
    if (mpl_significant_bits(n) > 384 || mp_cmp_z(n) != MP_GT)
        return MP_RANGE;

    MP_CHECKOK(mp_to_fixlen_octets(n, b_n, 48));
    MP_BE2LE(b_n);
    point_mul_g_secp384r1(b_x, b_y, b_n);
    MP_BE2LE(b_x);
    MP_BE2LE(b_y);
    MP_CHECKOK(mp_read_unsigned_octets(out_x, b_x, 48));
    MP_CHECKOK(mp_read_unsigned_octets(out_y, b_y, 48));

CLEANUP:
    return res;
}

static mp_err
point_mul_secp384r1_wrap(const mp_int *n, const mp_int *in_x,
                         const mp_int *in_y, mp_int *out_x,
                         mp_int *out_y, const ECGroup *group)
{
    unsigned char b_x[48];
    unsigned char b_y[48];
    unsigned char b_n[48];
    mp_err res;

    ARGCHK(n != NULL && in_x != NULL && in_y != NULL && out_x != NULL &&
               out_y != NULL,
           MP_BADARG);

    /* fail on out of range scalars */
    if (mpl_significant_bits(n) > 384 || mp_cmp_z(n) != MP_GT)
        return MP_RANGE;

    MP_CHECKOK(mp_to_fixlen_octets(n, b_n, 48));
    MP_CHECKOK(mp_to_fixlen_octets(in_x, b_x, 48));
    MP_CHECKOK(mp_to_fixlen_octets(in_y, b_y, 48));
    MP_BE2LE(b_x);
    MP_BE2LE(b_y);
    MP_BE2LE(b_n);
    point_mul_secp384r1(b_x, b_y, b_n, b_x, b_y);
    MP_BE2LE(b_x);
    MP_BE2LE(b_y);
    MP_CHECKOK(mp_read_unsigned_octets(out_x, b_x, 48));
    MP_CHECKOK(mp_read_unsigned_octets(out_y, b_y, 48));

CLEANUP:
    return res;
}

static mp_err
point_mul_two_secp384r1_wrap(const mp_int *n1, const mp_int *n2,
                             const mp_int *in_x,
                             const mp_int *in_y, mp_int *out_x,
                             mp_int *out_y,
                             const ECGroup *group)
{
    unsigned char b_x[48];
    unsigned char b_y[48];
    unsigned char b_n1[48];
    unsigned char b_n2[48];
    mp_err res;

    /* If n2 == NULL or 0, this is just a base-point multiplication. */
    if (n2 == NULL || mp_cmp_z(n2) == MP_EQ)
        return point_mul_g_secp384r1_wrap(n1, out_x, out_y, group);

    /* If n1 == NULL or 0, this is just an arbitary-point multiplication. */
    if (n1 == NULL || mp_cmp_z(n1) == MP_EQ)
        return point_mul_secp384r1_wrap(n2, in_x, in_y, out_x, out_y, group);

    ARGCHK(in_x != NULL && in_y != NULL && out_x != NULL && out_y != NULL,
           MP_BADARG);

    /* fail on out of range scalars */
    if (mpl_significant_bits(n1) > 384 || mp_cmp_z(n1) != MP_GT ||
        mpl_significant_bits(n2) > 384 || mp_cmp_z(n2) != MP_GT)
        return MP_RANGE;

    MP_CHECKOK(mp_to_fixlen_octets(n1, b_n1, 48));
    MP_CHECKOK(mp_to_fixlen_octets(n2, b_n2, 48));
    MP_CHECKOK(mp_to_fixlen_octets(in_x, b_x, 48));
    MP_CHECKOK(mp_to_fixlen_octets(in_y, b_y, 48));
    MP_BE2LE(b_x);
    MP_BE2LE(b_y);
    MP_BE2LE(b_n1);
    MP_BE2LE(b_n2);
    point_mul_two_secp384r1(b_x, b_y, b_n1, b_n2, b_x, b_y);
    MP_BE2LE(b_x);
    MP_BE2LE(b_y);
    MP_CHECKOK(mp_read_unsigned_octets(out_x, b_x, 48));
    MP_CHECKOK(mp_read_unsigned_octets(out_y, b_y, 48));

CLEANUP:
    return res;
}

mp_err
ec_group_set_secp384r1(ECGroup *group, ECCurveName name)
{
    if (name == ECCurve_NIST_P384) {
        group->base_point_mul = &point_mul_g_secp384r1_wrap;
        group->point_mul = &point_mul_secp384r1_wrap;
        group->points_mul = &point_mul_two_secp384r1_wrap;
    }
    return MP_OKAY;
}
