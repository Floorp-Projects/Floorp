/*
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
 * The Original Code is the elliptic curve math library for prime 
 * field curves.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems, Inc. are Copyright (C) 2003
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *	Sheueling Chang Shantz <sheueling.chang@sun.com> and
 *	Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
 *
 *	Bodo Moeller <moeller@cdc.informatik.tu-darmstadt.de>, 
 *	Nils Larsch <nla@trustcenter.de>, and 
 *	Lenka Fibikova <fibikova@exp-math.uni-essen.de>, the OpenSSL Project.
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
 */
#ifdef NSS_ENABLE_ECC
/*
 * GFp_ecl.c: Contains an implementation of elliptic curve math library
 * for curves over GFp.
 *
 * XXX Can be moved to a separate subdirectory later.
 *
 */

#include "GFp_ecl.h"
#include "mpi/mplogic.h"

/* Checks if point P(px, py) is at infinity.  Uses affine coordinates. */
mp_err
GFp_ec_pt_is_inf_aff(const mp_int *px, const mp_int *py)
{

    if ((mp_cmp_z(px) == 0) && (mp_cmp_z(py) == 0)) {
        return MP_YES;
    } else {
        return MP_NO;
    }

}

/* Sets P(px, py) to be the point at infinity.  Uses affine coordinates. */
mp_err
GFp_ec_pt_set_inf_aff(mp_int *px, mp_int *py)
{
    mp_zero(px);
    mp_zero(py);
    return MP_OKAY;
}

/* Computes R = P + Q based on IEEE P1363 A.10.1.
 * Elliptic curve points P, Q, and R can all be identical.
 * Uses affine coordinates.
 */
mp_err
GFp_ec_pt_add_aff(const mp_int *p, const mp_int *a, const mp_int *px,
    const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int lambda, temp, xtemp, ytemp;

    CHECK_MPI_OK( mp_init(&lambda) );
    CHECK_MPI_OK( mp_init(&temp) );
    CHECK_MPI_OK( mp_init(&xtemp) );
    CHECK_MPI_OK( mp_init(&ytemp) );
    /* if P = inf, then R = Q */
    if (GFp_ec_pt_is_inf_aff(px, py) == 0) {
        CHECK_MPI_OK( mp_copy(qx, rx) );
        CHECK_MPI_OK( mp_copy(qy, ry) );
        err = MP_OKAY;
        goto cleanup;
    }
    /* if Q = inf, then R = P */
    if (GFp_ec_pt_is_inf_aff(qx, qy) == 0) {
        CHECK_MPI_OK( mp_copy(px, rx) );
        CHECK_MPI_OK( mp_copy(py, ry) );
        err = MP_OKAY;
        goto cleanup;
    }
    /* if px != qx, then lambda = (py-qy) / (px-qx) */
    if (mp_cmp(px, qx) != 0) {
        CHECK_MPI_OK( mp_submod(py, qy, p, &ytemp) );
        CHECK_MPI_OK( mp_submod(px, qx, p, &xtemp) );
        CHECK_MPI_OK( mp_invmod(&xtemp, p, &xtemp) );
        CHECK_MPI_OK( mp_mulmod(&ytemp, &xtemp, p, &lambda) );
    } else {
        /* if py != qy or qy = 0, then R = inf */
        if (((mp_cmp(py, qy) != 0)) || (mp_cmp_z(qy) == 0)) {
            mp_zero(rx);
            mp_zero(ry);
            err = MP_OKAY;
            goto cleanup;
        }
        /* lambda = (3qx^2+a) / (2qy) */
        CHECK_MPI_OK( mp_sqrmod(qx, p, &xtemp) );
        mp_set(&temp, 0x3);
        CHECK_MPI_OK( mp_mulmod(&xtemp, &temp, p, &xtemp) );
        CHECK_MPI_OK( mp_addmod(&xtemp, a, p, &xtemp) );
        mp_set(&temp, 0x2);
        CHECK_MPI_OK( mp_mulmod(qy, &temp, p, &ytemp) );
        CHECK_MPI_OK( mp_invmod(&ytemp, p, &ytemp) );
        CHECK_MPI_OK( mp_mulmod(&xtemp, &ytemp, p, &lambda) );
    }
    /* rx = lambda^2 - px - qx */
    CHECK_MPI_OK( mp_sqrmod(&lambda, p, &xtemp) );
    CHECK_MPI_OK( mp_submod(&xtemp, px, p, &xtemp) );
    CHECK_MPI_OK( mp_submod(&xtemp, qx, p, &xtemp) );
    /* ry = (x1-x2) * lambda - y1 */
    CHECK_MPI_OK( mp_submod(qx, &xtemp, p, &ytemp) );
    CHECK_MPI_OK( mp_mulmod(&ytemp, &lambda, p, &ytemp) );
    CHECK_MPI_OK( mp_submod(&ytemp, qy, p, &ytemp) );
    CHECK_MPI_OK( mp_copy(&xtemp, rx) );
    CHECK_MPI_OK( mp_copy(&ytemp, ry) );

cleanup:
    mp_clear(&lambda);
    mp_clear(&temp);
    mp_clear(&xtemp);
    mp_clear(&ytemp);
    return err;
}

/* Computes R = P - Q. 
 * Elliptic curve points P, Q, and R can all be identical.
 * Uses affine coordinates.
 */
mp_err
GFp_ec_pt_sub_aff(const mp_int *p, const mp_int *a, const mp_int *px, 
    const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int nqy;
    MP_DIGITS(&nqy) = 0;
    CHECK_MPI_OK( mp_init(&nqy) );
    /* nqy = -qy */
    CHECK_MPI_OK( mp_neg(qy, &nqy) );
    err = GFp_ec_pt_add_aff(p, a, px, py, qx, &nqy, rx, ry);
cleanup:
    mp_clear(&nqy);
    return err;
}

/* Computes R = 2P. 
 * Elliptic curve points P and R can be identical.
 * Uses affine coordinates.
 */
mp_err
GFp_ec_pt_dbl_aff(const mp_int *p, const mp_int *a, const mp_int *px, 
    const mp_int *py, mp_int *rx, mp_int *ry)
{
    return GFp_ec_pt_add_aff(p, a, px, py, px, py, rx, ry);
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
GFp_ec_pt_mul_aff(const mp_int *p, const mp_int *a, const mp_int *b,
    const mp_int *px, const mp_int *py, const mp_int *n, mp_int *rx,
    mp_int *ry) 
{
    mp_err err = MP_OKAY;
    mp_int k, k3, qx, qy, sx, sy;
    int b1, b3, i, l;

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
    /* if n < 0 Q = -Q, k = -k */
    if (mp_cmp_z(n) < 0) {
        CHECK_MPI_OK( mp_neg(&qy, &qy) );
        CHECK_MPI_OK( mp_mod(&qy, p, &qy) );
        CHECK_MPI_OK( mp_neg(&k, &k) );
        CHECK_MPI_OK( mp_mod(&k, p, &k) );
    }
#ifdef EC_DEBUG /* basic double and add method */
    l = mpl_significant_bits(&k) - 1;
    mp_zero(&sx);
    mp_zero(&sy);
    for (i = l; i >= 0; i--) {
        /* if k_i = 1, then S = S + Q */
        if (mpl_get_bit(&k, i) != 0) {
            CHECK_MPI_OK( GFp_ec_pt_add_aff(p, a, &sx, &sy, 
		&qx, &qy, &sx, &sy) );
        }
        if (i > 0) {
            /* S = 2S */
            CHECK_MPI_OK( GFp_ec_pt_dbl_aff(p, a, &sx, &sy, &sx, &sy) );
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
        CHECK_MPI_OK( GFp_ec_pt_dbl_aff(p, a, &sx, &sy, &sx, &sy) );
		b3 = MP_GET_BIT(&k3, i);
		b1 = MP_GET_BIT(&k, i);
        /* if k3_i = 1 and k_i = 0, then S = S + Q */
        if ((b3 == 1) && (b1 == 0)) {
            CHECK_MPI_OK( GFp_ec_pt_add_aff(p, a, &sx, &sy, 
		&qx, &qy, &sx, &sy) );
        /* if k3_i = 0 and k_i = 1, then S = S - Q */
        } else if ((b3 == 0) && (b1 == 1)) {
            CHECK_MPI_OK( GFp_ec_pt_sub_aff(p, a, &sx, &sy, 
		&qx, &qy, &sx, &sy) );
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
    return err;
}

/* Converts a point P(px, py, pz) from Jacobian projective coordinates to
 * affine coordinates R(rx, ry).  P and R can share x and y coordinates.
 */
mp_err
GFp_ec_pt_jac2aff(const mp_int *px, const mp_int *py, const mp_int *pz,
    const mp_int *p, mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int z1, z2, z3;
    MP_DIGITS(&z1) = 0;
    MP_DIGITS(&z2) = 0;
    MP_DIGITS(&z3) = 0;
    CHECK_MPI_OK( mp_init(&z1) );
    CHECK_MPI_OK( mp_init(&z2) );
    CHECK_MPI_OK( mp_init(&z3) );

    /* if point at infinity, then set point at infinity and exit */
    if (GFp_ec_pt_is_inf_jac(px, py, pz) == MP_YES) {
        CHECK_MPI_OK( GFp_ec_pt_set_inf_aff(rx, ry) );
	goto cleanup;
    }

    /* transform (px, py, pz) into (px / pz^2, py / pz^3) */
    if (mp_cmp_d(pz, 1) == 0) {
        CHECK_MPI_OK( mp_copy(px, rx) );
	CHECK_MPI_OK( mp_copy(py, ry) );
    } else {
        CHECK_MPI_OK( mp_invmod(pz, p, &z1) );
	CHECK_MPI_OK( mp_sqrmod(&z1, p, &z2) );
	CHECK_MPI_OK( mp_mulmod(&z1, &z2, p, &z3) );
	CHECK_MPI_OK( mp_mulmod(px, &z2, p, rx) );
	CHECK_MPI_OK( mp_mulmod(py, &z3, p, ry) );
    }
	
cleanup:
    mp_clear(&z1);
    mp_clear(&z2);
    mp_clear(&z3);
	return err;
}

/* Checks if point P(px, py, pz) is at infinity.  
 * Uses Jacobian coordinates.
 */
mp_err
GFp_ec_pt_is_inf_jac(const mp_int *px, const mp_int *py, const mp_int *pz) 
{
    return mp_cmp_z(pz);
}

/* Sets P(px, py, pz) to be the point at infinity.  Uses Jacobian 
 * coordinates. 
 */
mp_err
GFp_ec_pt_set_inf_jac(mp_int *px, mp_int *py, mp_int *pz) 
{
    mp_zero(pz);
    return MP_OKAY;
}

/* Computes R = P + Q where R is (rx, ry, rz), P is (px, py, pz) and 
 * Q is (qx, qy, qz).  Elliptic curve points P, Q, and R can all be 
 * identical. Uses Jacobian coordinates.
 *
 * This routine implements Point Addition in the Jacobian Projective 
 * space as described in the paper "Efficient elliptic curve exponentiation 
 * using mixed coordinates", by H. Cohen, A Miyaji, T. Ono.
 */
mp_err
GFp_ec_pt_add_jac(const mp_int *p, const mp_int *a, const mp_int *px,
    const mp_int *py, const mp_int *pz, const mp_int *qx, 
    const mp_int *qy, const mp_int *qz, mp_int *rx, mp_int *ry, mp_int *rz)
{
    mp_err err = MP_OKAY;
    mp_int n0, u1, u2, s1, s2, H, G;
    MP_DIGITS(&n0) = 0;
    MP_DIGITS(&u1) = 0;
    MP_DIGITS(&u2) = 0;
    MP_DIGITS(&s1) = 0;
    MP_DIGITS(&s2) = 0;
    MP_DIGITS(&H) = 0;
    MP_DIGITS(&G) = 0;
    CHECK_MPI_OK( mp_init(&n0) );
    CHECK_MPI_OK( mp_init(&u1) );
    CHECK_MPI_OK( mp_init(&u2) );
    CHECK_MPI_OK( mp_init(&s1) );
    CHECK_MPI_OK( mp_init(&s2) );
    CHECK_MPI_OK( mp_init(&H) );
    CHECK_MPI_OK( mp_init(&G) );

    /* Use point double if pointers are equal. */
    if ((px == qx) && (py == qy) && (pz == qz)) {
        err = GFp_ec_pt_dbl_jac(p, a, px, py, pz, rx, ry, rz);
	goto cleanup;
    }
	
    /* If either P or Q is the point at infinity, then return 
     * the other point 
     */
    if (GFp_ec_pt_is_inf_jac(px, py, pz) == MP_YES) {
        CHECK_MPI_OK( mp_copy(qx, rx) );
	CHECK_MPI_OK( mp_copy(qy, ry) );
	CHECK_MPI_OK( mp_copy(qz, rz) );
	goto cleanup;
    }
    if (GFp_ec_pt_is_inf_jac(qx, qy, qz) == MP_YES) {
        CHECK_MPI_OK( mp_copy(px, rx) );
	CHECK_MPI_OK( mp_copy(py, ry) );
	CHECK_MPI_OK( mp_copy(pz, rz) );
	goto cleanup;
    }
	
    /* Compute u1 = px * qz^2, s1 = py * qz^3 */
    if (mp_cmp_d(qz, 1) == 0) {
        CHECK_MPI_OK( mp_copy(px, &u1) );
	CHECK_MPI_OK( mp_copy(py, &s1) );
    } else {
        CHECK_MPI_OK( mp_sqrmod(qz, p, &n0) );
	CHECK_MPI_OK( mp_mulmod(px, &n0, p, &u1) );
	CHECK_MPI_OK( mp_mulmod(&n0, qz, p, &n0) );
	CHECK_MPI_OK( mp_mulmod(py, &n0, p, &s1) );
    }

    /* Compute u2 = qx * pz^2, s2 = qy * pz^3 */
    if (mp_cmp_d(pz, 1) == 0) {
        CHECK_MPI_OK( mp_copy(qx, &u2) );
	CHECK_MPI_OK( mp_copy(qy, &s2) );
    } else {
        CHECK_MPI_OK( mp_sqrmod(pz, p, &n0) );
	CHECK_MPI_OK( mp_mulmod(qx, &n0, p, &u2) );
	CHECK_MPI_OK( mp_mulmod(&n0, pz, p, &n0) );
	CHECK_MPI_OK( mp_mulmod(qy, &n0, p, &s2) );
    }

    /* Compute H = u2 - u1 ; G = s2 - s1 */
    CHECK_MPI_OK( mp_submod(&u2, &u1, p, &H) );
    CHECK_MPI_OK( mp_submod(&s2, &s1, p, &G) );

    if (mp_cmp_z(&H) == 0) {
        if (mp_cmp_z(&G) == 0) {
	    /* P = Q; double */
	    err = GFp_ec_pt_dbl_jac(p, a, px, py, pz, 
				    rx, ry, rz);
	    goto cleanup;
	} else {
	    /* P = -Q; return point at infinity */
	    CHECK_MPI_OK( GFp_ec_pt_set_inf_jac(rx, ry, rz) );
	    goto cleanup;
	}
    }

    /* rz = pz * qz * H */
    if (mp_cmp_d(pz, 1) == 0) {
        if (mp_cmp_d(qz, 1) == 0) {
	    /* if pz == qz == 1, then rz = H */
	    CHECK_MPI_OK( mp_copy(&H, rz) );
	} else {
	    CHECK_MPI_OK( mp_mulmod(qz, &H, p, rz) );
	}
    } else {
        if (mp_cmp_d(qz, 1) == 0) {
	    CHECK_MPI_OK( mp_mulmod(pz, &H, p, rz) );
	} else {
	    CHECK_MPI_OK( mp_mulmod(pz, qz, p, &n0) );
	    CHECK_MPI_OK( mp_mulmod(&n0, &H, p, rz) );
	}
    }
	
    /* rx = G^2 - H^3 - 2 * u1 * H^2 */
    CHECK_MPI_OK( mp_sqrmod(&G, p, rx) );
    CHECK_MPI_OK( mp_sqrmod(&H, p, &n0) );
    CHECK_MPI_OK( mp_mulmod(&n0, &u1, p, &u1) );
    CHECK_MPI_OK( mp_addmod(&u1, &u1, p, &u2) );
    CHECK_MPI_OK( mp_mulmod(&H, &n0, p, &H) );
    CHECK_MPI_OK( mp_submod(rx, &H, p, rx) );
    CHECK_MPI_OK( mp_submod(rx, &u2, p, rx) );

    /* ry = - s1 * H^3 + G * (u1 * H^2 - rx) */
    /* (formula based on values of variables before block above) */
    CHECK_MPI_OK( mp_submod(&u1, rx, p, &u1) );
    CHECK_MPI_OK( mp_mulmod(&G, &u1, p, ry) );
    CHECK_MPI_OK( mp_mulmod(&s1, &H, p, &s1) );
    CHECK_MPI_OK( mp_submod(ry, &s1, p, ry) );

cleanup:
    mp_clear(&n0);
    mp_clear(&u1);
    mp_clear(&u2);
    mp_clear(&s1);
    mp_clear(&s2);
    mp_clear(&H);
    mp_clear(&G);
	return err;
}

/* Computes R = 2P.  Elliptic curve points P and R can be identical.  Uses 
 * Jacobian coordinates.
 *
 * This routine implements Point Doubling in the Jacobian Projective 
 * space as described in the paper "Efficient elliptic curve exponentiation 
 * using mixed coordinates", by H. Cohen, A Miyaji, T. Ono.
 */
mp_err
GFp_ec_pt_dbl_jac(const mp_int *p, const mp_int *a, const mp_int *px, 
    const mp_int *py, const mp_int *pz, mp_int *rx, mp_int *ry, mp_int *rz)
{
    mp_err err = MP_OKAY;
    mp_int t0, t1, M, S;
    MP_DIGITS(&t0) = 0;
    MP_DIGITS(&t1) = 0;
    MP_DIGITS(&M) = 0;
    MP_DIGITS(&S) = 0;
    CHECK_MPI_OK( mp_init(&t0) );
    CHECK_MPI_OK( mp_init(&t1) );
    CHECK_MPI_OK( mp_init(&M) );
    CHECK_MPI_OK( mp_init(&S) );

    if (GFp_ec_pt_is_inf_jac(px, py, pz) == MP_YES) {
        CHECK_MPI_OK( GFp_ec_pt_set_inf_jac(rx, ry, rz) );
	goto cleanup;
    }

    if (mp_cmp_d(pz, 1) == 0) {
        /* M = 3 * px^2 + a */
        CHECK_MPI_OK( mp_sqrmod(px, p, &t0) );
	CHECK_MPI_OK( mp_addmod(&t0, &t0, p, &M) );
	CHECK_MPI_OK( mp_addmod(&t0, &M, p, &t0) );
	CHECK_MPI_OK( mp_addmod(&t0, a, p, &M) );
    } else if (mp_cmp_int(a, -3) == 0) {
        /* M = 3 * (px + pz^2) * (px - pz) */
        CHECK_MPI_OK( mp_sqrmod(pz, p, &M) );
	CHECK_MPI_OK( mp_addmod(px, &M, p, &t0) );
	CHECK_MPI_OK( mp_submod(px, &M, p, &t1) );
	CHECK_MPI_OK( mp_mulmod(&t0, &t1, p, &M) );
	CHECK_MPI_OK( mp_addmod(&M, &M, p, &t0) );
	CHECK_MPI_OK( mp_addmod(&t0, &M, p, &M) );
    } else {
        CHECK_MPI_OK( mp_sqrmod(px, p, &t0) );
	CHECK_MPI_OK( mp_addmod(&t0, &t0, p, &M) );
	CHECK_MPI_OK( mp_addmod(&t0, &M, p, &t0) );
	CHECK_MPI_OK( mp_sqrmod(pz, p, &M) );
	CHECK_MPI_OK( mp_sqrmod(&M, p, &M) );
	CHECK_MPI_OK( mp_mulmod(&M, a, p, &M) );
	CHECK_MPI_OK( mp_addmod(&M, &t0, p, &M) );
    }

    /* rz = 2 * py * pz */
    if (mp_cmp_d(pz, 1) == 0) {
        CHECK_MPI_OK( mp_addmod(py, py, p, rz) );
	CHECK_MPI_OK( mp_sqrmod(rz, p, &t0) );
    } else {
        CHECK_MPI_OK( mp_addmod(py, py, p, &t0) );
	CHECK_MPI_OK( mp_mulmod(&t0, pz, p, rz) );
	CHECK_MPI_OK( mp_sqrmod(&t0, p, &t0) );
    }

    /* S = 4 * px * py^2 = pz * (2 * py)^2 */
    CHECK_MPI_OK( mp_mulmod(px, &t0, p, &S) );

    /* rx = M^2 - 2 * S */
    CHECK_MPI_OK( mp_addmod(&S, &S, p, &t1) );
    CHECK_MPI_OK( mp_sqrmod(&M, p, rx) );
    CHECK_MPI_OK( mp_submod(rx, &t1, p, rx) );

    /* ry = M * (S - rx) - 8 * py^4 */
    CHECK_MPI_OK( mp_sqrmod(&t0, p, &t1) );
    if (mp_isodd(&t1)) {
        CHECK_MPI_OK( mp_add(&t1, p, &t1) );
    }
    CHECK_MPI_OK( mp_div_2(&t1, &t1) );
    CHECK_MPI_OK( mp_submod(&S, rx, p, &S) );
    CHECK_MPI_OK( mp_mulmod(&M, &S, p, &M) );
    CHECK_MPI_OK( mp_submod(&M, &t1, p, ry) );
	
cleanup:
    mp_clear(&t0);
    mp_clear(&t1);
    mp_clear(&M);
    mp_clear(&S);
    return err;
}

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that 
 * determines the field GFp.  Elliptic curve points P and R can be 
 * identical.  Uses Jacobian coordinates.
 */
mp_err
GFp_ec_pt_mul_jac(const mp_int *p, const mp_int *a, const mp_int *b, 
    const mp_int *px, const mp_int *py, const mp_int *n, 
    mp_int *rx, mp_int *ry)
{
    mp_err err = MP_OKAY;
    mp_int k, qx, qy, qz, sx, sy, sz;
    int i, l;

    MP_DIGITS(&k) = 0;
    MP_DIGITS(&qx) = 0;
    MP_DIGITS(&qy) = 0;
    MP_DIGITS(&qz) = 0;
    MP_DIGITS(&sx) = 0;
    MP_DIGITS(&sy) = 0;
    MP_DIGITS(&sz) = 0;
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_init(&qx) );
    CHECK_MPI_OK( mp_init(&qy) );
    CHECK_MPI_OK( mp_init(&qz) );
    CHECK_MPI_OK( mp_init(&sx) );
    CHECK_MPI_OK( mp_init(&sy) );
    CHECK_MPI_OK( mp_init(&sz) );

    /* if n = 0 then r = inf */
    if (mp_cmp_z(n) == 0) {
        mp_zero(rx);
        mp_zero(ry);
        err = MP_OKAY;
        goto cleanup;
	/* if n < 0 then out of range error */
    } else if (mp_cmp_z(n) < 0) {
        err = MP_RANGE;
	goto cleanup;
    }
    /* Q = P, k = n */
    CHECK_MPI_OK( mp_copy(px, &qx) );
    CHECK_MPI_OK( mp_copy(py, &qy) );
    CHECK_MPI_OK( mp_set_int(&qz, 1) );
    CHECK_MPI_OK( mp_copy(n, &k) );

    /* double and add method */
    l = mpl_significant_bits(&k) - 1;
    mp_zero(&sx);
    mp_zero(&sy);
    mp_zero(&sz);
    for (i = l; i >= 0; i--) {
        /* if k_i = 1, then S = S + Q */
        if (MP_GET_BIT(&k, i) != 0) {
            CHECK_MPI_OK( GFp_ec_pt_add_jac(p, a, &sx, &sy, &sz, 
					    &qx, &qy, &qz, &sx, &sy, &sz) );
        }
        if (i > 0) {
            /* S = 2S */
            CHECK_MPI_OK( GFp_ec_pt_dbl_jac(p, a, &sx, &sy, &sz, 
					    &sx, &sy, &sz) );
        }
    }

    /* convert result S to affine coordinates */
    CHECK_MPI_OK( GFp_ec_pt_jac2aff(&sx, &sy, &sz, p, rx, ry) );

cleanup:
    mp_clear(&k);
    mp_clear(&qx);
    mp_clear(&qy);
    mp_clear(&qz);
    mp_clear(&sx);
    mp_clear(&sy);
    mp_clear(&sz);
    return err;
}
#endif /* NSS_ENABLE_ECC */
