/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ec2.h"
#include "mplogic.h"
#include "mp_gf2m.h"
#include <stdlib.h>
#ifdef ECL_DEBUG
#include <assert.h>
#endif

/* by default, these routines are unused and thus don't need to be compiled */
#ifdef ECL_ENABLE_GF2M_PROJ
/* Converts a point P(px, py) from affine coordinates to projective
 * coordinates R(rx, ry, rz). Assumes input is already field-encoded using 
 * field_enc, and returns output that is still field-encoded. */
mp_err
ec_GF2m_pt_aff2proj(const mp_int *px, const mp_int *py, mp_int *rx,
					mp_int *ry, mp_int *rz, const ECGroup *group)
{
	mp_err res = MP_OKAY;

	MP_CHECKOK(mp_copy(px, rx));
	MP_CHECKOK(mp_copy(py, ry));
	MP_CHECKOK(mp_set_int(rz, 1));
	if (group->meth->field_enc) {
		MP_CHECKOK(group->meth->field_enc(rz, rz, group->meth));
	}
  CLEANUP:
	return res;
}

/* Converts a point P(px, py, pz) from projective coordinates to affine
 * coordinates R(rx, ry).  P and R can share x and y coordinates. Assumes
 * input is already field-encoded using field_enc, and returns output that
 * is still field-encoded. */
mp_err
ec_GF2m_pt_proj2aff(const mp_int *px, const mp_int *py, const mp_int *pz,
					mp_int *rx, mp_int *ry, const ECGroup *group)
{
	mp_err res = MP_OKAY;
	mp_int z1, z2;

	MP_DIGITS(&z1) = 0;
	MP_DIGITS(&z2) = 0;
	MP_CHECKOK(mp_init(&z1));
	MP_CHECKOK(mp_init(&z2));

	/* if point at infinity, then set point at infinity and exit */
	if (ec_GF2m_pt_is_inf_proj(px, py, pz) == MP_YES) {
		MP_CHECKOK(ec_GF2m_pt_set_inf_aff(rx, ry));
		goto CLEANUP;
	}

	/* transform (px, py, pz) into (px / pz, py / pz^2) */
	if (mp_cmp_d(pz, 1) == 0) {
		MP_CHECKOK(mp_copy(px, rx));
		MP_CHECKOK(mp_copy(py, ry));
	} else {
		MP_CHECKOK(group->meth->field_div(NULL, pz, &z1, group->meth));
		MP_CHECKOK(group->meth->field_sqr(&z1, &z2, group->meth));
		MP_CHECKOK(group->meth->field_mul(px, &z1, rx, group->meth));
		MP_CHECKOK(group->meth->field_mul(py, &z2, ry, group->meth));
	}

  CLEANUP:
	mp_clear(&z1);
	mp_clear(&z2);
	return res;
}

/* Checks if point P(px, py, pz) is at infinity. Uses projective
 * coordinates. */
mp_err
ec_GF2m_pt_is_inf_proj(const mp_int *px, const mp_int *py,
					   const mp_int *pz)
{
	return mp_cmp_z(pz);
}

/* Sets P(px, py, pz) to be the point at infinity.  Uses projective
 * coordinates. */
mp_err
ec_GF2m_pt_set_inf_proj(mp_int *px, mp_int *py, mp_int *pz)
{
	mp_zero(pz);
	return MP_OKAY;
}

/* Computes R = P + Q where R is (rx, ry, rz), P is (px, py, pz) and Q is
 * (qx, qy, 1).  Elliptic curve points P, Q, and R can all be identical.
 * Uses mixed projective-affine coordinates. Assumes input is already
 * field-encoded using field_enc, and returns output that is still
 * field-encoded. Uses equation (3) from Hankerson, Hernandez, Menezes.
 * Software Implementation of Elliptic Curve Cryptography Over Binary
 * Fields. */
mp_err
ec_GF2m_pt_add_proj(const mp_int *px, const mp_int *py, const mp_int *pz,
					const mp_int *qx, const mp_int *qy, mp_int *rx,
					mp_int *ry, mp_int *rz, const ECGroup *group)
{
	mp_err res = MP_OKAY;
	mp_int A, B, C, D, E, F, G;

	/* If either P or Q is the point at infinity, then return the other
	 * point */
	if (ec_GF2m_pt_is_inf_proj(px, py, pz) == MP_YES) {
		return ec_GF2m_pt_aff2proj(qx, qy, rx, ry, rz, group);
	}
	if (ec_GF2m_pt_is_inf_aff(qx, qy) == MP_YES) {
		MP_CHECKOK(mp_copy(px, rx));
		MP_CHECKOK(mp_copy(py, ry));
		return mp_copy(pz, rz);
	}

	MP_DIGITS(&A) = 0;
	MP_DIGITS(&B) = 0;
	MP_DIGITS(&C) = 0;
	MP_DIGITS(&D) = 0;
	MP_DIGITS(&E) = 0;
	MP_DIGITS(&F) = 0;
	MP_DIGITS(&G) = 0;
	MP_CHECKOK(mp_init(&A));
	MP_CHECKOK(mp_init(&B));
	MP_CHECKOK(mp_init(&C));
	MP_CHECKOK(mp_init(&D));
	MP_CHECKOK(mp_init(&E));
	MP_CHECKOK(mp_init(&F));
	MP_CHECKOK(mp_init(&G));

	/* D = pz^2 */
	MP_CHECKOK(group->meth->field_sqr(pz, &D, group->meth));

	/* A = qy * pz^2 + py */
	MP_CHECKOK(group->meth->field_mul(qy, &D, &A, group->meth));
	MP_CHECKOK(group->meth->field_add(&A, py, &A, group->meth));

	/* B = qx * pz + px */
	MP_CHECKOK(group->meth->field_mul(qx, pz, &B, group->meth));
	MP_CHECKOK(group->meth->field_add(&B, px, &B, group->meth));

	/* C = pz * B */
	MP_CHECKOK(group->meth->field_mul(pz, &B, &C, group->meth));

	/* D = B^2 * (C + a * pz^2) (using E as a temporary variable) */
	MP_CHECKOK(group->meth->
			   field_mul(&group->curvea, &D, &D, group->meth));
	MP_CHECKOK(group->meth->field_add(&C, &D, &D, group->meth));
	MP_CHECKOK(group->meth->field_sqr(&B, &E, group->meth));
	MP_CHECKOK(group->meth->field_mul(&E, &D, &D, group->meth));

	/* rz = C^2 */
	MP_CHECKOK(group->meth->field_sqr(&C, rz, group->meth));

	/* E = A * C */
	MP_CHECKOK(group->meth->field_mul(&A, &C, &E, group->meth));

	/* rx = A^2 + D + E */
	MP_CHECKOK(group->meth->field_sqr(&A, rx, group->meth));
	MP_CHECKOK(group->meth->field_add(rx, &D, rx, group->meth));
	MP_CHECKOK(group->meth->field_add(rx, &E, rx, group->meth));

	/* F = rx + qx * rz */
	MP_CHECKOK(group->meth->field_mul(qx, rz, &F, group->meth));
	MP_CHECKOK(group->meth->field_add(rx, &F, &F, group->meth));

	/* G = rx + qy * rz */
	MP_CHECKOK(group->meth->field_mul(qy, rz, &G, group->meth));
	MP_CHECKOK(group->meth->field_add(rx, &G, &G, group->meth));

	/* ry = E * F + rz * G (using G as a temporary variable) */
	MP_CHECKOK(group->meth->field_mul(rz, &G, &G, group->meth));
	MP_CHECKOK(group->meth->field_mul(&E, &F, ry, group->meth));
	MP_CHECKOK(group->meth->field_add(ry, &G, ry, group->meth));

  CLEANUP:
	mp_clear(&A);
	mp_clear(&B);
	mp_clear(&C);
	mp_clear(&D);
	mp_clear(&E);
	mp_clear(&F);
	mp_clear(&G);
	return res;
}

/* Computes R = 2P.  Elliptic curve points P and R can be identical.  Uses 
 * projective coordinates.
 *
 * Assumes input is already field-encoded using field_enc, and returns 
 * output that is still field-encoded.
 *
 * Uses equation (3) from Hankerson, Hernandez, Menezes. Software 
 * Implementation of Elliptic Curve Cryptography Over Binary Fields.
 */
mp_err
ec_GF2m_pt_dbl_proj(const mp_int *px, const mp_int *py, const mp_int *pz,
					mp_int *rx, mp_int *ry, mp_int *rz,
					const ECGroup *group)
{
	mp_err res = MP_OKAY;
	mp_int t0, t1;

	if (ec_GF2m_pt_is_inf_proj(px, py, pz) == MP_YES) {
		return ec_GF2m_pt_set_inf_proj(rx, ry, rz);
	}

	MP_DIGITS(&t0) = 0;
	MP_DIGITS(&t1) = 0;
	MP_CHECKOK(mp_init(&t0));
	MP_CHECKOK(mp_init(&t1));

	/* t0 = px^2 */
	/* t1 = pz^2 */
	MP_CHECKOK(group->meth->field_sqr(px, &t0, group->meth));
	MP_CHECKOK(group->meth->field_sqr(pz, &t1, group->meth));

	/* rz = px^2 * pz^2 */
	MP_CHECKOK(group->meth->field_mul(&t0, &t1, rz, group->meth));

	/* t0 = px^4 */
	/* t1 = b * pz^4 */
	MP_CHECKOK(group->meth->field_sqr(&t0, &t0, group->meth));
	MP_CHECKOK(group->meth->field_sqr(&t1, &t1, group->meth));
	MP_CHECKOK(group->meth->
			   field_mul(&group->curveb, &t1, &t1, group->meth));

	/* rx = px^4 + b * pz^4 */
	MP_CHECKOK(group->meth->field_add(&t0, &t1, rx, group->meth));

	/* ry = b * pz^4 * rz + rx * (a * rz + py^2 + b * pz^4) */
	MP_CHECKOK(group->meth->field_sqr(py, ry, group->meth));
	MP_CHECKOK(group->meth->field_add(ry, &t1, ry, group->meth));
	/* t0 = a * rz */
	MP_CHECKOK(group->meth->
			   field_mul(&group->curvea, rz, &t0, group->meth));
	MP_CHECKOK(group->meth->field_add(&t0, ry, ry, group->meth));
	MP_CHECKOK(group->meth->field_mul(rx, ry, ry, group->meth));
	/* t1 = b * pz^4 * rz */
	MP_CHECKOK(group->meth->field_mul(&t1, rz, &t1, group->meth));
	MP_CHECKOK(group->meth->field_add(&t1, ry, ry, group->meth));

  CLEANUP:
	mp_clear(&t0);
	mp_clear(&t1);
	return res;
}

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that
 * determines the field GF2m.  Elliptic curve points P and R can be
 * identical.  Uses mixed projective-affine coordinates. Assumes input is
 * already field-encoded using field_enc, and returns output that is still
 * field-encoded. Uses 4-bit window method. */
mp_err
ec_GF2m_pt_mul_proj(const mp_int *n, const mp_int *px, const mp_int *py,
					mp_int *rx, mp_int *ry, const ECGroup *group)
{
	mp_err res = MP_OKAY;
	mp_int precomp[16][2], rz;
	mp_digit precomp_arr[ECL_MAX_FIELD_SIZE_DIGITS * 16 * 2], *t;
	int i, ni, d;

	ARGCHK(group != NULL, MP_BADARG);
	ARGCHK((n != NULL) && (px != NULL) && (py != NULL), MP_BADARG);

	/* initialize precomputation table */
	t = precomp_arr;
	for (i = 0; i < 16; i++) {
		/* x co-ord */
		MP_SIGN(&precomp[i][0]) = MP_ZPOS;
		MP_ALLOC(&precomp[i][0]) = ECL_MAX_FIELD_SIZE_DIGITS;
		MP_USED(&precomp[i][0]) = 1;
		*t = 0;
		MP_DIGITS(&precomp[i][0]) = t;
		t += ECL_MAX_FIELD_SIZE_DIGITS;
		/* y co-ord */
		MP_SIGN(&precomp[i][1]) = MP_ZPOS;
		MP_ALLOC(&precomp[i][1]) = ECL_MAX_FIELD_SIZE_DIGITS;
		MP_USED(&precomp[i][1]) = 1;
		*t = 0;
		MP_DIGITS(&precomp[i][1]) = t;
		t += ECL_MAX_FIELD_SIZE_DIGITS;
	}

	/* fill precomputation table */
	mp_zero(&precomp[0][0]);
	mp_zero(&precomp[0][1]);
	MP_CHECKOK(mp_copy(px, &precomp[1][0]));
	MP_CHECKOK(mp_copy(py, &precomp[1][1]));
	for (i = 2; i < 16; i++) {
		MP_CHECKOK(group->
				   point_add(&precomp[1][0], &precomp[1][1],
							 &precomp[i - 1][0], &precomp[i - 1][1],
							 &precomp[i][0], &precomp[i][1], group));
	}

	d = (mpl_significant_bits(n) + 3) / 4;

	/* R = inf */
	MP_DIGITS(&rz) = 0;
	MP_CHECKOK(mp_init(&rz));
	MP_CHECKOK(ec_GF2m_pt_set_inf_proj(rx, ry, &rz));

	for (i = d - 1; i >= 0; i--) {
		/* compute window ni */
		ni = MP_GET_BIT(n, 4 * i + 3);
		ni <<= 1;
		ni |= MP_GET_BIT(n, 4 * i + 2);
		ni <<= 1;
		ni |= MP_GET_BIT(n, 4 * i + 1);
		ni <<= 1;
		ni |= MP_GET_BIT(n, 4 * i);
		/* R = 2^4 * R */
		MP_CHECKOK(ec_GF2m_pt_dbl_proj(rx, ry, &rz, rx, ry, &rz, group));
		MP_CHECKOK(ec_GF2m_pt_dbl_proj(rx, ry, &rz, rx, ry, &rz, group));
		MP_CHECKOK(ec_GF2m_pt_dbl_proj(rx, ry, &rz, rx, ry, &rz, group));
		MP_CHECKOK(ec_GF2m_pt_dbl_proj(rx, ry, &rz, rx, ry, &rz, group));
		/* R = R + (ni * P) */
		MP_CHECKOK(ec_GF2m_pt_add_proj
				   (rx, ry, &rz, &precomp[ni][0], &precomp[ni][1], rx, ry,
					&rz, group));
	}

	/* convert result S to affine coordinates */
	MP_CHECKOK(ec_GF2m_pt_proj2aff(rx, ry, &rz, rx, ry, group));

  CLEANUP:
	mp_clear(&rz);
	return res;
}
#endif
