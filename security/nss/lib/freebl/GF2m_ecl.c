/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the elliptic curve math library for binary polynomial field curves.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Douglas Stebila <douglas@stebila.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef NSS_ENABLE_ECC
/*
 * GF2m_ecl.c: Contains an implementation of elliptic curve math library
 * for curves over GF2m.
 *
 * XXX Can be moved to a separate subdirectory later.
 *
 */

#include "GF2m_ecl.h"
#include "mpi/mplogic.h"
#include "mpi/mp_gf2m.h"
#include <stdlib.h>

/* Checks if point P(px, py) is at infinity.  Uses affine coordinates. */
mp_err 
GF2m_ec_pt_is_inf_aff(const mp_int *px, const mp_int *py)
{

    if ((mp_cmp_z(px) == 0) && (mp_cmp_z(py) == 0)) {
        return MP_YES;
    } else {
        return MP_NO;
    }

}

/* Sets P(px, py) to be the point at infinity.  Uses affine coordinates. */
mp_err 
GF2m_ec_pt_set_inf_aff(mp_int *px, mp_int *py)
{
    mp_zero(px);
    mp_zero(py);
    return MP_OKAY;
}

/* Computes R = P + Q based on IEEE P1363 A.10.2.
 * Elliptic curve points P, Q, and R can all be identical.
 * Uses affine coordinates.
 */
mp_err 
GF2m_ec_pt_add_aff(const mp_int *pp, const mp_int *a, const mp_int *px, 
    const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int lambda, xtemp, ytemp;
    unsigned int *p;
    int p_size;

    p_size = mp_bpoly2arr(pp, p, 0) + 1;
    p = (unsigned int *) (malloc(sizeof(unsigned int) * p_size));
    if (p == NULL) goto cleanup;
    mp_bpoly2arr(pp, p, p_size);
    
    CHECK_MPI_OK( mp_init(&lambda) );
    CHECK_MPI_OK( mp_init(&xtemp) );
    CHECK_MPI_OK( mp_init(&ytemp) );
    /* if P = inf, then R = Q */
    if (GF2m_ec_pt_is_inf_aff(px, py) == 0) {
        CHECK_MPI_OK( mp_copy(qx, rx) );
        CHECK_MPI_OK( mp_copy(qy, ry) );
        err = MP_OKAY;
        goto cleanup;
    }
    /* if Q = inf, then R = P */
    if (GF2m_ec_pt_is_inf_aff(qx, qy) == 0) {
        CHECK_MPI_OK( mp_copy(px, rx) );
        CHECK_MPI_OK( mp_copy(py, ry) );
        err = MP_OKAY;
        goto cleanup;
    }
    /* if px != qx, then lambda = (py+qy) / (px+qx), 
     *                   xtemp = a + lambda^2 + lambda + px + qx
     */
    if (mp_cmp(px, qx) != 0) {
        CHECK_MPI_OK( mp_badd(py, qy, &ytemp) );
        CHECK_MPI_OK( mp_badd(px, qx, &xtemp) );
        CHECK_MPI_OK( mp_bdivmod(&ytemp, &xtemp, pp, p, &lambda) );
        CHECK_MPI_OK( mp_bsqrmod(&lambda, p, &xtemp) );
        CHECK_MPI_OK( mp_badd(&xtemp, &lambda, &xtemp) );
        CHECK_MPI_OK( mp_badd(&xtemp, a, &xtemp) );
        CHECK_MPI_OK( mp_badd(&xtemp, px, &xtemp) );
        CHECK_MPI_OK( mp_badd(&xtemp, qx, &xtemp) );
    } else {
        /* if py != qy or qx = 0, then R = inf */
        if (((mp_cmp(py, qy) != 0)) || (mp_cmp_z(qx) == 0)) {
            mp_zero(rx);
            mp_zero(ry);
            err = MP_OKAY;
            goto cleanup;
        }
        /* lambda = qx + qy / qx  */
        CHECK_MPI_OK( mp_bdivmod(qy, qx, pp, p, &lambda) );
        CHECK_MPI_OK( mp_badd(&lambda, qx, &lambda) );
        /* xtemp = a + lambda^2 + lambda */
        CHECK_MPI_OK( mp_bsqrmod(&lambda, p, &xtemp) );
        CHECK_MPI_OK( mp_badd(&xtemp, &lambda, &xtemp) );
        CHECK_MPI_OK( mp_badd(&xtemp, a, &xtemp) );
    }
    /* ry = (qx + xtemp) * lambda + xtemp + qy */
    CHECK_MPI_OK( mp_badd(qx, &xtemp, &ytemp) );
    CHECK_MPI_OK( mp_bmulmod(&ytemp, &lambda, p, &ytemp) );
    CHECK_MPI_OK( mp_badd(&ytemp, &xtemp, &ytemp) );
    CHECK_MPI_OK( mp_badd(&ytemp, qy, ry) );
    /* rx = xtemp */
    CHECK_MPI_OK( mp_copy(&xtemp, rx) );

cleanup:
    mp_clear(&lambda);
    mp_clear(&xtemp);
    mp_clear(&ytemp);
    free(p);
    return err;
}

/* Computes R = P - Q. 
 * Elliptic curve points P, Q, and R can all be identical.
 * Uses affine coordinates.
 */
mp_err 
GF2m_ec_pt_sub_aff(const mp_int *pp, const mp_int *a, const mp_int *px, 
    const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int nqy;
    MP_DIGITS(&nqy) = 0;
    CHECK_MPI_OK( mp_init(&nqy) );
    /* nqy = qx+qy */
    CHECK_MPI_OK( mp_badd(qx, qy, &nqy) );
    err = GF2m_ec_pt_add_aff(pp, a, px, py, qx, &nqy, rx, ry);
cleanup:
    mp_clear(&nqy);
    return err;
}

/* Computes R = 2P. 
 * Elliptic curve points P and R can be identical.
 * Uses affine coordinates.
 */
mp_err 
GF2m_ec_pt_dbl_aff(const mp_int *pp, const mp_int *a, const mp_int *px, 
    const mp_int *py, mp_int *rx, mp_int *ry)
{
    return GF2m_ec_pt_add_aff(pp, a, px, py, px, py, rx, ry);
}

/* Gets the i'th bit in the binary representation of a.
 * If i >= length(a), then return 0.
 * (The above behaviour differs from mpl_get_bit, which
 * causes an error if i >= length(a).)
 */
#define MP_GET_BIT(a, i) \
    ((i) >= mpl_significant_bits((a))) ? 0 : mpl_get_bit((a), (i))

/* Computes R = nP based on IEEE P1363 A.10.3.
 * Elliptic curve points P and R can be identical.
 * Uses affine coordinates.
 */
mp_err 
GF2m_ec_pt_mul_aff(const mp_int *pp, const mp_int *a, const mp_int *b, 
    const mp_int *px, const mp_int *py, const mp_int *n, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int k, k3, qx, qy, sx, sy;
    int b1, b3, i, l;
    unsigned int *p;
    int p_size;

    MP_DIGITS(&k) = 0;
    MP_DIGITS(&k3) = 0;
    MP_DIGITS(&qx) = 0;
    MP_DIGITS(&qy) = 0;
    MP_DIGITS(&sx) = 0;
    MP_DIGITS(&sy) = 0;
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_init(&k3) );
    CHECK_MPI_OK( mp_init(&qx) );
    CHECK_MPI_OK( mp_init(&qy) );
    CHECK_MPI_OK( mp_init(&sx) );
    CHECK_MPI_OK( mp_init(&sy) );
    
    p_size = mp_bpoly2arr(pp, p, 0) + 1;
    p = (unsigned int *) (malloc(sizeof(unsigned int) * p_size));
    if (p == NULL) goto cleanup;
    mp_bpoly2arr(pp, p, p_size);
    
    /* if n = 0 then r = inf */
    if (mp_cmp_z(n) == 0) {
        mp_zero(rx);
        mp_zero(ry);
        err = MP_OKAY;
        goto cleanup;
    }
    /* Q = P, k = n */
    CHECK_MPI_OK( mp_copy(px, &qx) );
    CHECK_MPI_OK( mp_copy(py, &qy) );
    CHECK_MPI_OK( mp_copy(n, &k) );
    /* if n < 0 then Q = -Q, k = -k */
    if (mp_cmp_z(n) < 0) {
        CHECK_MPI_OK( mp_badd(&qx, &qy, &qy) );
        CHECK_MPI_OK( mp_neg(&k, &k) );
    }
#ifdef EC_DEBUG /* basic double and add method */
    l = mpl_significant_bits(&k) - 1;
    mp_zero(&sx);
    mp_zero(&sy);
    for (i = l; i >= 0; i--) {
        /* if k_i = 1, then S = S + Q */
        if (mpl_get_bit(&k, i) != 0) {
            CHECK_MPI_OK( GF2m_ec_pt_add_aff(pp, a, &sx, &sy, &qx, &qy, &sx, &sy) );
        }
        if (i > 0) {
            /* S = 2S */
            CHECK_MPI_OK( GF2m_ec_pt_dbl_aff(pp, a, &sx, &sy, &sx, &sy) );
        }
    }
#else /* double and add/subtract method from standard */
    /* k3 = 3 * k */
    mp_set(&k3, 0x3);
    CHECK_MPI_OK( mp_mul(&k, &k3, &k3) );
    /* S = Q */
    CHECK_MPI_OK( mp_copy(&qx, &sx) );
    CHECK_MPI_OK( mp_copy(&qy, &sy) );
    /* l = index of high order bit in binary representation of 3*k */
    l = mpl_significant_bits(&k3) - 1;
    /* for i = l-1 downto 1 */
    for (i = l - 1; i >= 1; i--) {
        /* S = 2S */
        CHECK_MPI_OK( GF2m_ec_pt_dbl_aff(pp, a, &sx, &sy, &sx, &sy) );
        b3 = MP_GET_BIT(&k3, i);
        b1 = MP_GET_BIT(&k, i);
        /* if k3_i = 1 and k_i = 0, then S = S + Q */
        if ((b3 == 1) && (b1 == 0)) {
            CHECK_MPI_OK( GF2m_ec_pt_add_aff(pp, a, &sx, &sy, &qx, &qy, &sx, &sy) );
        /* if k3_i = 0 and k_i = 1, then S = S - Q */
        } else if ((b3 == 0) && (b1 == 1)) {
            CHECK_MPI_OK( GF2m_ec_pt_sub_aff(pp, a, &sx, &sy, &qx, &qy, &sx, &sy) );
        }
    }
#endif
    /* output S */
    CHECK_MPI_OK( mp_copy(&sx, rx) );
    CHECK_MPI_OK( mp_copy(&sy, ry) );

cleanup:
    mp_clear(&k);
    mp_clear(&k3);
    mp_clear(&qx);
    mp_clear(&qy);
    mp_clear(&sx);
    mp_clear(&sy);
    free(p);
    return err;
}

/* Compute the x-coordinate x/z for the point 2*(x/z) in Montgomery projective 
 * coordinates.
 * Uses algorithm Mdouble in appendix of 
 *     Lopez, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 * modified to not require precomputation of c=b^{2^{m-1}}.
 */
static mp_err 
gf2m_Mdouble(const mp_int *pp, const unsigned int p[], const mp_int *a, 
    const mp_int *b, mp_int *x, mp_int *z) 
{
    mp_err err = MP_OKAY;
    mp_int t1;

    MP_DIGITS(&t1) = 0;
    CHECK_MPI_OK( mp_init(&t1) );
    
    CHECK_MPI_OK( mp_bsqrmod(x, p, x) );
    CHECK_MPI_OK( mp_bsqrmod(z, p, &t1) );
    CHECK_MPI_OK( mp_bmulmod(x, &t1, p, z) );
    CHECK_MPI_OK( mp_bsqrmod(x, p, x) );
    CHECK_MPI_OK( mp_bsqrmod(&t1, p, &t1) );
    CHECK_MPI_OK( mp_bmulmod(b, &t1, p, &t1) );
    CHECK_MPI_OK( mp_badd(x, &t1, x) );

cleanup:
    mp_clear(&t1);
    return err;
}

/* Compute the x-coordinate x1/z1 for the point (x1/z1)+(x2/x2) in Montgomery 
 * projective coordinates.
 * Uses algorithm Madd in appendix of 
 *     Lopex, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 */
static mp_err 
gf2m_Madd(const mp_int *pp, const unsigned int p[], const mp_int *a, 
    const mp_int *b, const mp_int *x, mp_int *x1, mp_int *z1, mp_int *x2,
    mp_int *z2)
{
    mp_err err = MP_OKAY;
    mp_int t1, t2;

    MP_DIGITS(&t1) = 0;
    MP_DIGITS(&t2) = 0;
    CHECK_MPI_OK( mp_init(&t1) );
    CHECK_MPI_OK( mp_init(&t2) );
    
    CHECK_MPI_OK( mp_copy(x, &t1) );
    CHECK_MPI_OK( mp_bmulmod(x1, z2, p, x1) );
    CHECK_MPI_OK( mp_bmulmod(z1, x2, p, z1) );
    CHECK_MPI_OK( mp_bmulmod(x1, z1, p, &t2) );
    CHECK_MPI_OK( mp_badd(z1, x1, z1) );
    CHECK_MPI_OK( mp_bsqrmod(z1, p, z1) );
    CHECK_MPI_OK( mp_bmulmod(z1, &t1, p, x1) );
    CHECK_MPI_OK( mp_badd(x1, &t2, x1) );
    
cleanup:
    mp_clear(&t1);
    mp_clear(&t2);
    return err;
}

/* Compute the x, y affine coordinates from the point (x1, z1) (x2, z2) 
 * using Montgomery point multiplication algorithm Mxy() in appendix of 
 *     Lopex, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 * Returns:
 *     0 on error
 *     1 if return value should be the point at infinity
 *     2 otherwise
 */
static int
gf2m_Mxy(const mp_int *pp, const unsigned int p[], const mp_int *a, 
    const mp_int *b, const mp_int *x, const mp_int *y, mp_int *x1, mp_int *z1, 
    mp_int *x2, mp_int *z2)
{
    mp_err err = MP_OKAY;
    int ret;
    mp_int t3, t4, t5;

    MP_DIGITS(&t3) = 0;
    MP_DIGITS(&t4) = 0;
    MP_DIGITS(&t5) = 0;
    CHECK_MPI_OK( mp_init(&t3) );
    CHECK_MPI_OK( mp_init(&t4) );
    CHECK_MPI_OK( mp_init(&t5) );
    
    if (mp_cmp_z(z1) == 0) {
        mp_zero(x2);
        mp_zero(z2);
        ret = 1;
        goto cleanup;
    }
    
    if (mp_cmp_z(z2) == 0) {
        CHECK_MPI_OK( mp_copy(x, x2) );
        CHECK_MPI_OK( mp_badd(x, y, z2) );
        ret = 2;
        goto cleanup;
    }
        
    mp_set(&t5, 0x1);

    CHECK_MPI_OK( mp_bmulmod(z1, z2, p, &t3) );

    CHECK_MPI_OK( mp_bmulmod(z1, x, p, z1) );
    CHECK_MPI_OK( mp_badd(z1, x1, z1) );
    CHECK_MPI_OK( mp_bmulmod(z2, x, p, z2) );
    CHECK_MPI_OK( mp_bmulmod(z2, x1, p, x1) );
    CHECK_MPI_OK( mp_badd(z2, x2, z2) );

    CHECK_MPI_OK( mp_bmulmod(z2, z1, p, z2) );
    CHECK_MPI_OK( mp_bsqrmod(x, p, &t4) );
    CHECK_MPI_OK( mp_badd(&t4, y, &t4) );
    CHECK_MPI_OK( mp_bmulmod(&t4, &t3, p, &t4) );
    CHECK_MPI_OK( mp_badd(&t4, z2, &t4) );

    CHECK_MPI_OK( mp_bmulmod(&t3, x, p, &t3) );
    CHECK_MPI_OK( mp_bdivmod(&t5, &t3, pp, p, &t3) );
    CHECK_MPI_OK( mp_bmulmod(&t3, &t4, p, &t4) );
    CHECK_MPI_OK( mp_bmulmod(x1, &t3, p, x2) );
    CHECK_MPI_OK( mp_badd(x2, x, z2) );

    CHECK_MPI_OK( mp_bmulmod(z2, &t4, p, z2) );
    CHECK_MPI_OK( mp_badd(z2, y, z2) );

    ret = 2;
    
cleanup:
    mp_clear(&t3);
    mp_clear(&t4);
    mp_clear(&t5);
    if (err == MP_OKAY) {
        return ret;
    } else {
        return 0;
    }
}

/* Computes R = nP based on algorithm 2P of
 *     Lopex, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 * Elliptic curve points P and R can be identical.
 * Uses Montgomery projective coordinates.
 */
mp_err 
GF2m_ec_pt_mul_mont(const mp_int *pp, const mp_int *a, const mp_int *b, 
    const mp_int *px, const mp_int *py, const mp_int *n, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int x1, x2, z1, z2;
    int i, j;
    mp_digit top_bit, mask;
    unsigned int *p;
    int p_size;

    MP_DIGITS(&x1) = 0;
    MP_DIGITS(&x2) = 0;
    MP_DIGITS(&z1) = 0;
    MP_DIGITS(&z2) = 0;
    CHECK_MPI_OK( mp_init(&x1) );
    CHECK_MPI_OK( mp_init(&x2) );
    CHECK_MPI_OK( mp_init(&z1) );
    CHECK_MPI_OK( mp_init(&z2) );

    p_size = mp_bpoly2arr(pp, p, 0) + 1;
    p = (unsigned int *) (malloc(sizeof(unsigned int) * p_size));
    if (p == NULL) goto cleanup;
    mp_bpoly2arr(pp, p, p_size);
    
	/* if result should be point at infinity */
    if ((mp_cmp_z(n) == 0) || (GF2m_ec_pt_is_inf_aff(px, py) == MP_YES)) {
        CHECK_MPI_OK( GF2m_ec_pt_set_inf_aff(rx, ry) );
        goto cleanup;
    }

    CHECK_MPI_OK( mp_copy(rx, &x2) );           /* x2 = rx */
    CHECK_MPI_OK( mp_copy(ry, &z2) );           /* z2 = ry */

    CHECK_MPI_OK( mp_copy(px, &x1) );           /* x1 = px */
    mp_set(&z1, 0x1);                           /* z1 = 1 */
    CHECK_MPI_OK( mp_bsqrmod(&x1, p, &z2) );    /* z2 = x1^2 = x2^2 */
    CHECK_MPI_OK( mp_bsqrmod(&z2, p, &x2) );
    CHECK_MPI_OK( mp_badd(&x2, b, &x2) );       /* x2 = px^4 + b */
    
    /* find top-most bit and go one past it */
    i = MP_USED(n) - 1;
    j = MP_DIGIT_BIT - 1;
    top_bit = 1;
    top_bit <<= MP_DIGIT_BIT - 1;
    mask = top_bit;
    while (!(MP_DIGITS(n)[i] & mask)) {
        mask >>= 1;
        j--;
    }
    mask >>= 1; j--;
    
    /* if top most bit was at word break, go to next word */
    if (!mask) {
	i--; 
        j = MP_DIGIT_BIT - 1;
	mask = top_bit;
    }

    for (; i >= 0; i--) {
	for (; j >= 0; j--) {
	    if (MP_DIGITS(n)[i] & mask) {
		CHECK_MPI_OK( gf2m_Madd(pp, p, a, b, px, &x1, &z1, &x2, &z2) );
                CHECK_MPI_OK( gf2m_Mdouble(pp, p, a, b, &x2, &z2) );
            } else {
                CHECK_MPI_OK( gf2m_Madd(pp, p, a, b, px, &x2, &z2, &x1, &z1) );
                CHECK_MPI_OK( gf2m_Mdouble(pp, p, a, b, &x1, &z1) );
            }
	    mask >>= 1;
	}
	j = MP_DIGIT_BIT - 1;
	mask = top_bit;
    }

    /* convert out of "projective" coordinates */
    i = gf2m_Mxy(pp, p, a, b, px, py, &x1, &z1, &x2, &z2);
    if (i == 0) {
	err = MP_BADARG;
        goto cleanup;
    } else if (i == 1) {
	CHECK_MPI_OK( GF2m_ec_pt_set_inf_aff(rx, ry) );
    } else {
        CHECK_MPI_OK( mp_copy(&x2, rx) );
        CHECK_MPI_OK( mp_copy(&z2, ry) );
    }

cleanup:
    mp_clear(&x1);
    mp_clear(&x2);
    mp_clear(&z1);
    mp_clear(&z2);
    free(p);
    return err;
}

#endif /* NSS_ENABLE_ECC */
