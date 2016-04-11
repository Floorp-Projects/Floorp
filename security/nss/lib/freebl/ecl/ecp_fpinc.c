/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source file is meant to be included by other source files
 * (ecp_fp###.c, where ### is one of 160, 192, 224) and should not
 * constitute an independent compilation unit. It requires the following
 * preprocessor definitions be made: ECFP_BSIZE - the number of bits in
 * the field's prime 
 * ECFP_NUMDOUBLES - the number of doubles to store one
 * multi-precision integer in floating point 

/* Adds a prefix to a given token to give a unique token name. Prefixes
 * with "ecfp" + ECFP_BSIZE + "_". e.g. if ECFP_BSIZE = 160, then
 * PREFIX(hello) = ecfp160_hello This optimization allows static function
 * linking and compiler loop unrolling without code duplication. */
#ifndef PREFIX
#define PREFIX(b) PREFIX1(ECFP_BSIZE, b)
#define PREFIX1(bsize, b) PREFIX2(bsize, b)
#define PREFIX2(bsize, b) ecfp ## bsize ## _ ## b
#endif

/* Returns true iff every double in d is 0. (If d == 0 and it is tidied,
 * this will be true.) */
mp_err PREFIX(isZero) (const double *d) {
	int i;

	for (i = 0; i < ECFP_NUMDOUBLES; i++) {
		if (d[i] != 0)
			return MP_NO;
	}
	return MP_YES;
}

/* Sets the multi-precision floating point number at t = 0 */
void PREFIX(zero) (double *t) {
	int i;

	for (i = 0; i < ECFP_NUMDOUBLES; i++) {
		t[i] = 0;
	}
}

/* Sets the multi-precision floating point number at t = 1 */
void PREFIX(one) (double *t) {
	int i;

	t[0] = 1;
	for (i = 1; i < ECFP_NUMDOUBLES; i++) {
		t[i] = 0;
	}
}

/* Checks if point P(x, y, z) is at infinity. Uses Jacobian coordinates. */
mp_err PREFIX(pt_is_inf_jac) (const ecfp_jac_pt * p) {
	return PREFIX(isZero) (p->z);
}

/* Sets the Jacobian point P to be at infinity. */
void PREFIX(set_pt_inf_jac) (ecfp_jac_pt * p) {
	PREFIX(zero) (p->z);
}

/* Checks if point P(x, y) is at infinity. Uses Affine coordinates. */
mp_err PREFIX(pt_is_inf_aff) (const ecfp_aff_pt * p) {
	if (PREFIX(isZero) (p->x) == MP_YES && PREFIX(isZero) (p->y) == MP_YES)
		return MP_YES;
	return MP_NO;
}

/* Sets the affine point P to be at infinity. */
void PREFIX(set_pt_inf_aff) (ecfp_aff_pt * p) {
	PREFIX(zero) (p->x);
	PREFIX(zero) (p->y);
}

/* Checks if point P(x, y, z, a*z^4) is at infinity. Uses Modified
 * Jacobian coordinates. */
mp_err PREFIX(pt_is_inf_jm) (const ecfp_jm_pt * p) {
	return PREFIX(isZero) (p->z);
}

/* Sets the Modified Jacobian point P to be at infinity. */
void PREFIX(set_pt_inf_jm) (ecfp_jm_pt * p) {
	PREFIX(zero) (p->z);
}

/* Checks if point P(x, y, z, z^2, z^3) is at infinity. Uses Chudnovsky
 * Jacobian coordinates */
mp_err PREFIX(pt_is_inf_chud) (const ecfp_chud_pt * p) {
	return PREFIX(isZero) (p->z);
}

/* Sets the Chudnovsky Jacobian point P to be at infinity. */
void PREFIX(set_pt_inf_chud) (ecfp_chud_pt * p) {
	PREFIX(zero) (p->z);
}

/* Copies a multi-precision floating point number, Setting dest = src */
void PREFIX(copy) (double *dest, const double *src) {
	int i;

	for (i = 0; i < ECFP_NUMDOUBLES; i++) {
		dest[i] = src[i];
	}
}

/* Sets dest = -src */
void PREFIX(negLong) (double *dest, const double *src) {
	int i;

	for (i = 0; i < 2 * ECFP_NUMDOUBLES; i++) {
		dest[i] = -src[i];
	}
}

/* Sets r = -p p = (x, y, z, z2, z3) r = (x, -y, z, z2, z3) Uses
 * Chudnovsky Jacobian coordinates. */
/* TODO reverse order */
void PREFIX(pt_neg_chud) (const ecfp_chud_pt * p, ecfp_chud_pt * r) {
	int i;

	PREFIX(copy) (r->x, p->x);
	PREFIX(copy) (r->z, p->z);
	PREFIX(copy) (r->z2, p->z2);
	PREFIX(copy) (r->z3, p->z3);
	for (i = 0; i < ECFP_NUMDOUBLES; i++) {
		r->y[i] = -p->y[i];
	}
}

/* Computes r = x + y. Does not tidy or reduce. Any combinations of r, x,
 * y can point to the same data. Componentwise adds first ECFP_NUMDOUBLES
 * doubles of x and y and stores the result in r. */
void PREFIX(addShort) (double *r, const double *x, const double *y) {
	int i;

	for (i = 0; i < ECFP_NUMDOUBLES; i++) {
		*r++ = *x++ + *y++;
	}
}

/* Computes r = x + y. Does not tidy or reduce. Any combinations of r, x,
 * y can point to the same data. Componentwise adds first
 * 2*ECFP_NUMDOUBLES doubles of x and y and stores the result in r. */
void PREFIX(addLong) (double *r, const double *x, const double *y) {
	int i;

	for (i = 0; i < 2 * ECFP_NUMDOUBLES; i++) {
		*r++ = *x++ + *y++;
	}
}

/* Computes r = x - y. Does not tidy or reduce. Any combinations of r, x,
 * y can point to the same data. Componentwise subtracts first
 * ECFP_NUMDOUBLES doubles of x and y and stores the result in r. */
void PREFIX(subtractShort) (double *r, const double *x, const double *y) {
	int i;

	for (i = 0; i < ECFP_NUMDOUBLES; i++) {
		*r++ = *x++ - *y++;
	}
}

/* Computes r = x - y. Does not tidy or reduce. Any combinations of r, x,
 * y can point to the same data. Componentwise subtracts first
 * 2*ECFP_NUMDOUBLES doubles of x and y and stores the result in r. */
void PREFIX(subtractLong) (double *r, const double *x, const double *y) {
	int i;

	for (i = 0; i < 2 * ECFP_NUMDOUBLES; i++) {
		*r++ = *x++ - *y++;
	}
}

/* Computes r = x*y.  Both x and y should be tidied and reduced,
 * r must be different (point to different memory) than x and y.
 * Does not tidy or reduce. */
void PREFIX(multiply)(double *r, const double *x, const double *y) {
	int i, j;

	for(j=0;j<ECFP_NUMDOUBLES-1;j++) {
		r[j] = x[0] * y[j];
		r[j+(ECFP_NUMDOUBLES-1)] = x[ECFP_NUMDOUBLES-1] * y[j];
	}
	r[ECFP_NUMDOUBLES-1] = x[0] * y[ECFP_NUMDOUBLES-1];
	r[ECFP_NUMDOUBLES-1] += x[ECFP_NUMDOUBLES-1] * y[0];
	r[2*ECFP_NUMDOUBLES-2] = x[ECFP_NUMDOUBLES-1] * y[ECFP_NUMDOUBLES-1];
	r[2*ECFP_NUMDOUBLES-1] = 0;
	
	for(i=1;i<ECFP_NUMDOUBLES-1;i++) {
		for(j=0;j<ECFP_NUMDOUBLES;j++) {
			r[i+j] += (x[i] * y[j]);
		}
	}
}

/* Computes the square of x and stores the result in r.  x should be
 * tidied & reduced, r will be neither tidied nor reduced. 
 * r should point to different memory than x */
void PREFIX(square) (double *r, const double *x) {
	PREFIX(multiply) (r, x, x);
}

/* Perform a point doubling in Jacobian coordinates. Input and output
 * should be multi-precision floating point integers. */
void PREFIX(pt_dbl_jac) (const ecfp_jac_pt * dp, ecfp_jac_pt * dr,
						 const EC_group_fp * group) {
	double t0[2 * ECFP_NUMDOUBLES], t1[2 * ECFP_NUMDOUBLES],
		M[2 * ECFP_NUMDOUBLES], S[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity */
	if (PREFIX(pt_is_inf_jac) (dp) == MP_YES) {
		/* Set r = pt at infinity */
		PREFIX(set_pt_inf_jac) (dr);
		goto CLEANUP;
	}

	/* Perform typical point doubling operations */

	/* TODO? is it worthwhile to do optimizations for when pz = 1? */

	if (group->aIsM3) {
		/* When a = -3, M = 3(px - pz^2)(px + pz^2) */
		PREFIX(square) (t1, dp->z);
		group->ecfp_reduce(t1, t1, group);	/* 2^23 since the negative
											 * rounding buys another bit */
		PREFIX(addShort) (t0, dp->x, t1);	/* 2*2^23 */
		PREFIX(subtractShort) (t1, dp->x, t1);	/* 2 * 2^23 */
		PREFIX(multiply) (M, t0, t1);	/* 40 * 2^46 */
		PREFIX(addLong) (t0, M, M);	/* 80 * 2^46 */
		PREFIX(addLong) (M, t0, M);	/* 120 * 2^46 < 2^53 */
		group->ecfp_reduce(M, M, group);
	} else {
		/* Generic case */
		/* M = 3 (px^2) + a*(pz^4) */
		PREFIX(square) (t0, dp->x);
		PREFIX(addLong) (M, t0, t0);
		PREFIX(addLong) (t0, t0, M);	/* t0 = 3(px^2) */
		PREFIX(square) (M, dp->z);
		group->ecfp_reduce(M, M, group);
		PREFIX(square) (t1, M);
		group->ecfp_reduce(t1, t1, group);
		PREFIX(multiply) (M, t1, group->curvea);	/* M = a(pz^4) */
		PREFIX(addLong) (M, M, t0);
		group->ecfp_reduce(M, M, group);
	}

	/* rz = 2 * py * pz */
	PREFIX(multiply) (t1, dp->y, dp->z);
	PREFIX(addLong) (t1, t1, t1);
	group->ecfp_reduce(dr->z, t1, group);

	/* t0 = 2y^2 */
	PREFIX(square) (t0, dp->y);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(addShort) (t0, t0, t0);

	/* S = 4 * px * py^2 = 2 * px * t0 */
	PREFIX(multiply) (S, dp->x, t0);
	PREFIX(addLong) (S, S, S);
	group->ecfp_reduce(S, S, group);

	/* rx = M^2 - 2 * S */
	PREFIX(square) (t1, M);
	PREFIX(subtractShort) (t1, t1, S);
	PREFIX(subtractShort) (t1, t1, S);
	group->ecfp_reduce(dr->x, t1, group);

	/* ry = M * (S - rx) - 8 * py^4 */
	PREFIX(square) (t1, t0);	/* t1 = 4y^4 */
	PREFIX(subtractShort) (S, S, dr->x);
	PREFIX(multiply) (t0, M, S);
	PREFIX(subtractLong) (t0, t0, t1);
	PREFIX(subtractLong) (t0, t0, t1);
	group->ecfp_reduce(dr->y, t0, group);

  CLEANUP:
	return;
}

/* Perform a point addition using coordinate system Jacobian + Affine ->
 * Jacobian. Input and output should be multi-precision floating point
 * integers. */
void PREFIX(pt_add_jac_aff) (const ecfp_jac_pt * p, const ecfp_aff_pt * q,
							 ecfp_jac_pt * r, const EC_group_fp * group) {
	/* Temporary storage */
	double A[2 * ECFP_NUMDOUBLES], B[2 * ECFP_NUMDOUBLES],
		C[2 * ECFP_NUMDOUBLES], C2[2 * ECFP_NUMDOUBLES],
		D[2 * ECFP_NUMDOUBLES], C3[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity for p or q */
	if (PREFIX(pt_is_inf_aff) (q) == MP_YES) {
		PREFIX(copy) (r->x, p->x);
		PREFIX(copy) (r->y, p->y);
		PREFIX(copy) (r->z, p->z);
		goto CLEANUP;
	} else if (PREFIX(pt_is_inf_jac) (p) == MP_YES) {
		PREFIX(copy) (r->x, q->x);
		PREFIX(copy) (r->y, q->y);
		/* Since the affine point is not infinity, we can set r->z = 1 */
		PREFIX(one) (r->z);
		goto CLEANUP;
	}

	/* Calculates c = qx * pz^2 - px d = (qy * b - py) rx = d^2 - c^3 + 2
	 * (px * c^2) ry = d * (c-rx) - py*c^3 rz = c * pz */

	/* A = pz^2, B = pz^3 */
	PREFIX(square) (A, p->z);
	group->ecfp_reduce(A, A, group);
	PREFIX(multiply) (B, A, p->z);
	group->ecfp_reduce(B, B, group);

	/* C = qx * A - px */
	PREFIX(multiply) (C, q->x, A);
	PREFIX(subtractShort) (C, C, p->x);
	group->ecfp_reduce(C, C, group);

	/* D = qy * B - py */
	PREFIX(multiply) (D, q->y, B);
	PREFIX(subtractShort) (D, D, p->y);
	group->ecfp_reduce(D, D, group);

	/* C2 = C^2, C3 = C^3 */
	PREFIX(square) (C2, C);
	group->ecfp_reduce(C2, C2, group);
	PREFIX(multiply) (C3, C2, C);
	group->ecfp_reduce(C3, C3, group);

	/* rz = A = pz * C */
	PREFIX(multiply) (A, p->z, C);
	group->ecfp_reduce(r->z, A, group);

	/* C = px * C^2, untidied, unreduced */
	PREFIX(multiply) (C, p->x, C2);

	/* A = D^2, untidied, unreduced */
	PREFIX(square) (A, D);

	/* rx = B = A - C3 - C - C = D^2 - (C^3 + 2 * (px * C^2) */
	PREFIX(subtractShort) (A, A, C3);
	PREFIX(subtractLong) (A, A, C);
	PREFIX(subtractLong) (A, A, C);
	group->ecfp_reduce(r->x, A, group);

	/* B = py * C3, untidied, unreduced */
	PREFIX(multiply) (B, p->y, C3);

	/* C = px * C^2 - rx */
	PREFIX(subtractShort) (C, C, r->x);
	group->ecfp_reduce(C, C, group);

	/* ry = A = D * C - py * C^3 */
	PREFIX(multiply) (A, D, C);
	PREFIX(subtractLong) (A, A, B);
	group->ecfp_reduce(r->y, A, group);

  CLEANUP:
	return;
}

/* Perform a point addition using Jacobian coordinate system. Input and
 * output should be multi-precision floating point integers. */
void PREFIX(pt_add_jac) (const ecfp_jac_pt * p, const ecfp_jac_pt * q,
						 ecfp_jac_pt * r, const EC_group_fp * group) {

	/* Temporary Storage */
	double t0[2 * ECFP_NUMDOUBLES], t1[2 * ECFP_NUMDOUBLES],
		U[2 * ECFP_NUMDOUBLES], R[2 * ECFP_NUMDOUBLES],
		S[2 * ECFP_NUMDOUBLES], H[2 * ECFP_NUMDOUBLES],
		H3[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity for p, if so set r = q */
	if (PREFIX(pt_is_inf_jac) (p) == MP_YES) {
		PREFIX(copy) (r->x, q->x);
		PREFIX(copy) (r->y, q->y);
		PREFIX(copy) (r->z, q->z);
		goto CLEANUP;
	}

	/* Check for point at infinity for p, if so set r = q */
	if (PREFIX(pt_is_inf_jac) (q) == MP_YES) {
		PREFIX(copy) (r->x, p->x);
		PREFIX(copy) (r->y, p->y);
		PREFIX(copy) (r->z, p->z);
		goto CLEANUP;
	}

	/* U = px * qz^2 , S = py * qz^3 */
	PREFIX(square) (t0, q->z);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (U, p->x, t0);
	group->ecfp_reduce(U, U, group);
	PREFIX(multiply) (t1, t0, q->z);
	group->ecfp_reduce(t1, t1, group);
	PREFIX(multiply) (t0, p->y, t1);
	group->ecfp_reduce(S, t0, group);

	/* H = qx*(pz)^2 - U , R = (qy * pz^3 - S) */
	PREFIX(square) (t0, p->z);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (H, q->x, t0);
	PREFIX(subtractShort) (H, H, U);
	group->ecfp_reduce(H, H, group);
	PREFIX(multiply) (t1, t0, p->z);	/* t1 = pz^3 */
	group->ecfp_reduce(t1, t1, group);
	PREFIX(multiply) (t0, t1, q->y);	/* t0 = qy * pz^3 */
	PREFIX(subtractShort) (t0, t0, S);
	group->ecfp_reduce(R, t0, group);

	/* U = U*H^2, H3 = H^3 */
	PREFIX(square) (t0, H);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (t1, U, t0);
	group->ecfp_reduce(U, t1, group);
	PREFIX(multiply) (H3, t0, H);
	group->ecfp_reduce(H3, H3, group);

	/* rz = pz * qz * H */
	PREFIX(multiply) (t0, q->z, H);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (t1, t0, p->z);
	group->ecfp_reduce(r->z, t1, group);

	/* rx = R^2 - H^3 - 2 * U */
	PREFIX(square) (t0, R);
	PREFIX(subtractShort) (t0, t0, H3);
	PREFIX(subtractShort) (t0, t0, U);
	PREFIX(subtractShort) (t0, t0, U);
	group->ecfp_reduce(r->x, t0, group);

	/* ry = R(U - rx) - S*H3 */
	PREFIX(subtractShort) (t1, U, r->x);
	PREFIX(multiply) (t0, t1, R);
	PREFIX(multiply) (t1, S, H3);
	PREFIX(subtractLong) (t1, t0, t1);
	group->ecfp_reduce(r->y, t1, group);

  CLEANUP:
	return;
}

/* Perform a point doubling in Modified Jacobian coordinates. Input and
 * output should be multi-precision floating point integers. */
void PREFIX(pt_dbl_jm) (const ecfp_jm_pt * p, ecfp_jm_pt * r,
						const EC_group_fp * group) {

	/* Temporary storage */
	double t0[2 * ECFP_NUMDOUBLES], t1[2 * ECFP_NUMDOUBLES],
		M[2 * ECFP_NUMDOUBLES], S[2 * ECFP_NUMDOUBLES],
		U[2 * ECFP_NUMDOUBLES], T[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity */
	if (PREFIX(pt_is_inf_jm) (p) == MP_YES) {
		/* Set r = pt at infinity by setting rz = 0 */
		PREFIX(set_pt_inf_jm) (r);
		goto CLEANUP;
	}

	/* M = 3 (px^2) + a*(pz^4) */
	PREFIX(square) (t0, p->x);
	PREFIX(addLong) (M, t0, t0);
	PREFIX(addLong) (t0, t0, M);	/* t0 = 3(px^2) */
	PREFIX(addShort) (t0, t0, p->az4);
	group->ecfp_reduce(M, t0, group);

	/* rz = 2 * py * pz */
	PREFIX(multiply) (t1, p->y, p->z);
	PREFIX(addLong) (t1, t1, t1);
	group->ecfp_reduce(r->z, t1, group);

	/* t0 = 2y^2, U = 8y^4 */
	PREFIX(square) (t0, p->y);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(addShort) (t0, t0, t0);
	PREFIX(square) (U, t0);
	group->ecfp_reduce(U, U, group);
	PREFIX(addShort) (U, U, U);

	/* S = 4 * px * py^2 = 2 * px * t0 */
	PREFIX(multiply) (S, p->x, t0);
	group->ecfp_reduce(S, S, group);
	PREFIX(addShort) (S, S, S);

	/* rx = M^2 - 2S */
	PREFIX(square) (T, M);
	PREFIX(subtractShort) (T, T, S);
	PREFIX(subtractShort) (T, T, S);
	group->ecfp_reduce(r->x, T, group);

	/* ry = M * (S - rx) - U */
	PREFIX(subtractShort) (S, S, r->x);
	PREFIX(multiply) (t0, M, S);
	PREFIX(subtractShort) (t0, t0, U);
	group->ecfp_reduce(r->y, t0, group);

	/* ra*z^4 = 2*U*(apz4) */
	PREFIX(multiply) (t1, U, p->az4);
	PREFIX(addLong) (t1, t1, t1);
	group->ecfp_reduce(r->az4, t1, group);

  CLEANUP:
	return;
}

/* Perform a point doubling using coordinates Affine -> Chudnovsky
 * Jacobian. Input and output should be multi-precision floating point
 * integers. */
void PREFIX(pt_dbl_aff2chud) (const ecfp_aff_pt * p, ecfp_chud_pt * r,
							  const EC_group_fp * group) {
	double t0[2 * ECFP_NUMDOUBLES], t1[2 * ECFP_NUMDOUBLES],
		M[2 * ECFP_NUMDOUBLES], twoY2[2 * ECFP_NUMDOUBLES],
		S[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity for p, if so set r = O */
	if (PREFIX(pt_is_inf_aff) (p) == MP_YES) {
		PREFIX(set_pt_inf_chud) (r);
		goto CLEANUP;
	}

	/* M = 3(px)^2 + a */
	PREFIX(square) (t0, p->x);
	PREFIX(addLong) (t1, t0, t0);
	PREFIX(addLong) (t1, t1, t0);
	PREFIX(addShort) (t1, t1, group->curvea);
	group->ecfp_reduce(M, t1, group);

	/* twoY2 = 2*(py)^2, S = 4(px)(py)^2 */
	PREFIX(square) (twoY2, p->y);
	PREFIX(addLong) (twoY2, twoY2, twoY2);
	group->ecfp_reduce(twoY2, twoY2, group);
	PREFIX(multiply) (S, p->x, twoY2);
	PREFIX(addLong) (S, S, S);
	group->ecfp_reduce(S, S, group);

	/* rx = M^2 - 2S */
	PREFIX(square) (t0, M);
	PREFIX(subtractShort) (t0, t0, S);
	PREFIX(subtractShort) (t0, t0, S);
	group->ecfp_reduce(r->x, t0, group);

	/* ry = M(S-rx) - 8y^4 */
	PREFIX(subtractShort) (t0, S, r->x);
	PREFIX(multiply) (t1, t0, M);
	PREFIX(square) (t0, twoY2);
	PREFIX(subtractLong) (t1, t1, t0);
	PREFIX(subtractLong) (t1, t1, t0);
	group->ecfp_reduce(r->y, t1, group);

	/* rz = 2py */
	PREFIX(addShort) (r->z, p->y, p->y);

	/* rz2 = rz^2 */
	PREFIX(square) (t0, r->z);
	group->ecfp_reduce(r->z2, t0, group);

	/* rz3 = rz^3 */
	PREFIX(multiply) (t0, r->z, r->z2);
	group->ecfp_reduce(r->z3, t0, group);

  CLEANUP:
	return;
}

/* Perform a point addition using coordinates: Modified Jacobian +
 * Chudnovsky Jacobian -> Modified Jacobian. Input and output should be
 * multi-precision floating point integers. */
void PREFIX(pt_add_jm_chud) (ecfp_jm_pt * p, ecfp_chud_pt * q,
							 ecfp_jm_pt * r, const EC_group_fp * group) {

	double t0[2 * ECFP_NUMDOUBLES], t1[2 * ECFP_NUMDOUBLES],
		U[2 * ECFP_NUMDOUBLES], R[2 * ECFP_NUMDOUBLES],
		S[2 * ECFP_NUMDOUBLES], H[2 * ECFP_NUMDOUBLES],
		H3[2 * ECFP_NUMDOUBLES], pz2[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity for p, if so set r = q need to convert
	 * from Chudnovsky form to Modified Jacobian form */
	if (PREFIX(pt_is_inf_jm) (p) == MP_YES) {
		PREFIX(copy) (r->x, q->x);
		PREFIX(copy) (r->y, q->y);
		PREFIX(copy) (r->z, q->z);
		PREFIX(square) (t0, q->z2);
		group->ecfp_reduce(t0, t0, group);
		PREFIX(multiply) (t1, t0, group->curvea);
		group->ecfp_reduce(r->az4, t1, group);
		goto CLEANUP;
	}
	/* Check for point at infinity for q, if so set r = p */
	if (PREFIX(pt_is_inf_chud) (q) == MP_YES) {
		PREFIX(copy) (r->x, p->x);
		PREFIX(copy) (r->y, p->y);
		PREFIX(copy) (r->z, p->z);
		PREFIX(copy) (r->az4, p->az4);
		goto CLEANUP;
	}

	/* U = px * qz^2 */
	PREFIX(multiply) (U, p->x, q->z2);
	group->ecfp_reduce(U, U, group);

	/* H = qx*(pz)^2 - U */
	PREFIX(square) (t0, p->z);
	group->ecfp_reduce(pz2, t0, group);
	PREFIX(multiply) (H, pz2, q->x);
	group->ecfp_reduce(H, H, group);
	PREFIX(subtractShort) (H, H, U);

	/* U = U*H^2, H3 = H^3 */
	PREFIX(square) (t0, H);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (t1, U, t0);
	group->ecfp_reduce(U, t1, group);
	PREFIX(multiply) (H3, t0, H);
	group->ecfp_reduce(H3, H3, group);

	/* S = py * qz^3 */
	PREFIX(multiply) (S, p->y, q->z3);
	group->ecfp_reduce(S, S, group);

	/* R = (qy * z1^3 - s) */
	PREFIX(multiply) (t0, pz2, p->z);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (R, t0, q->y);
	PREFIX(subtractShort) (R, R, S);
	group->ecfp_reduce(R, R, group);

	/* rz = pz * qz * H */
	PREFIX(multiply) (t1, q->z, H);
	group->ecfp_reduce(t1, t1, group);
	PREFIX(multiply) (t0, p->z, t1);
	group->ecfp_reduce(r->z, t0, group);

	/* rx = R^2 - H^3 - 2 * U */
	PREFIX(square) (t0, R);
	PREFIX(subtractShort) (t0, t0, H3);
	PREFIX(subtractShort) (t0, t0, U);
	PREFIX(subtractShort) (t0, t0, U);
	group->ecfp_reduce(r->x, t0, group);

	/* ry = R(U - rx) - S*H3 */
	PREFIX(subtractShort) (t1, U, r->x);
	PREFIX(multiply) (t0, t1, R);
	PREFIX(multiply) (t1, S, H3);
	PREFIX(subtractLong) (t1, t0, t1);
	group->ecfp_reduce(r->y, t1, group);

	if (group->aIsM3) {			/* a == -3 */
		/* a(rz^4) = -3 * ((rz^2)^2) */
		PREFIX(square) (t0, r->z);
		group->ecfp_reduce(t0, t0, group);
		PREFIX(square) (t1, t0);
		PREFIX(addLong) (t0, t1, t1);
		PREFIX(addLong) (t0, t0, t1);
		PREFIX(negLong) (t0, t0);
		group->ecfp_reduce(r->az4, t0, group);
	} else {					/* Generic case */
		/* a(rz^4) = a * ((rz^2)^2) */
		PREFIX(square) (t0, r->z);
		group->ecfp_reduce(t0, t0, group);
		PREFIX(square) (t1, t0);
		group->ecfp_reduce(t1, t1, group);
		PREFIX(multiply) (t0, group->curvea, t1);
		group->ecfp_reduce(r->az4, t0, group);
	}
  CLEANUP:
	return;
}

/* Perform a point addition using Chudnovsky Jacobian coordinates. Input
 * and output should be multi-precision floating point integers. */
void PREFIX(pt_add_chud) (const ecfp_chud_pt * p, const ecfp_chud_pt * q,
						  ecfp_chud_pt * r, const EC_group_fp * group) {

	/* Temporary Storage */
	double t0[2 * ECFP_NUMDOUBLES], t1[2 * ECFP_NUMDOUBLES],
		U[2 * ECFP_NUMDOUBLES], R[2 * ECFP_NUMDOUBLES],
		S[2 * ECFP_NUMDOUBLES], H[2 * ECFP_NUMDOUBLES],
		H3[2 * ECFP_NUMDOUBLES];

	/* Check for point at infinity for p, if so set r = q */
	if (PREFIX(pt_is_inf_chud) (p) == MP_YES) {
		PREFIX(copy) (r->x, q->x);
		PREFIX(copy) (r->y, q->y);
		PREFIX(copy) (r->z, q->z);
		PREFIX(copy) (r->z2, q->z2);
		PREFIX(copy) (r->z3, q->z3);
		goto CLEANUP;
	}

	/* Check for point at infinity for p, if so set r = q */
	if (PREFIX(pt_is_inf_chud) (q) == MP_YES) {
		PREFIX(copy) (r->x, p->x);
		PREFIX(copy) (r->y, p->y);
		PREFIX(copy) (r->z, p->z);
		PREFIX(copy) (r->z2, p->z2);
		PREFIX(copy) (r->z3, p->z3);
		goto CLEANUP;
	}

	/* U = px * qz^2 */
	PREFIX(multiply) (U, p->x, q->z2);
	group->ecfp_reduce(U, U, group);

	/* H = qx*(pz)^2 - U */
	PREFIX(multiply) (H, q->x, p->z2);
	PREFIX(subtractShort) (H, H, U);
	group->ecfp_reduce(H, H, group);

	/* U = U*H^2, H3 = H^3 */
	PREFIX(square) (t0, H);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (t1, U, t0);
	group->ecfp_reduce(U, t1, group);
	PREFIX(multiply) (H3, t0, H);
	group->ecfp_reduce(H3, H3, group);

	/* S = py * qz^3 */
	PREFIX(multiply) (S, p->y, q->z3);
	group->ecfp_reduce(S, S, group);

	/* rz = pz * qz * H */
	PREFIX(multiply) (t0, q->z, H);
	group->ecfp_reduce(t0, t0, group);
	PREFIX(multiply) (t1, t0, p->z);
	group->ecfp_reduce(r->z, t1, group);

	/* R = (qy * z1^3 - s) */
	PREFIX(multiply) (t0, q->y, p->z3);
	PREFIX(subtractShort) (t0, t0, S);
	group->ecfp_reduce(R, t0, group);

	/* rx = R^2 - H^3 - 2 * U */
	PREFIX(square) (t0, R);
	PREFIX(subtractShort) (t0, t0, H3);
	PREFIX(subtractShort) (t0, t0, U);
	PREFIX(subtractShort) (t0, t0, U);
	group->ecfp_reduce(r->x, t0, group);

	/* ry = R(U - rx) - S*H3 */
	PREFIX(subtractShort) (t1, U, r->x);
	PREFIX(multiply) (t0, t1, R);
	PREFIX(multiply) (t1, S, H3);
	PREFIX(subtractLong) (t1, t0, t1);
	group->ecfp_reduce(r->y, t1, group);

	/* rz2 = rz^2 */
	PREFIX(square) (t0, r->z);
	group->ecfp_reduce(r->z2, t0, group);

	/* rz3 = rz^3 */
	PREFIX(multiply) (t0, r->z, r->z2);
	group->ecfp_reduce(r->z3, t0, group);

  CLEANUP:
	return;
}

/* Expects out to be an array of size 16 of Chudnovsky Jacobian points.
 * Fills in Chudnovsky Jacobian form (x, y, z, z^2, z^3), for -15P, -13P,
 * -11P, -9P, -7P, -5P, -3P, -P, P, 3P, 5P, 7P, 9P, 11P, 13P, 15P */
void PREFIX(precompute_chud) (ecfp_chud_pt * out, const ecfp_aff_pt * p,
							  const EC_group_fp * group) {

	ecfp_chud_pt p2;

	/* Set out[8] = P */
	PREFIX(copy) (out[8].x, p->x);
	PREFIX(copy) (out[8].y, p->y);
	PREFIX(one) (out[8].z);
	PREFIX(one) (out[8].z2);
	PREFIX(one) (out[8].z3);

	/* Set p2 = 2P */
	PREFIX(pt_dbl_aff2chud) (p, &p2, group);

	/* Set 3P, 5P, ..., 15P */
	PREFIX(pt_add_chud) (&out[8], &p2, &out[9], group);
	PREFIX(pt_add_chud) (&out[9], &p2, &out[10], group);
	PREFIX(pt_add_chud) (&out[10], &p2, &out[11], group);
	PREFIX(pt_add_chud) (&out[11], &p2, &out[12], group);
	PREFIX(pt_add_chud) (&out[12], &p2, &out[13], group);
	PREFIX(pt_add_chud) (&out[13], &p2, &out[14], group);
	PREFIX(pt_add_chud) (&out[14], &p2, &out[15], group);

	/* Set -15P, -13P, ..., -P */
	PREFIX(pt_neg_chud) (&out[8], &out[7]);
	PREFIX(pt_neg_chud) (&out[9], &out[6]);
	PREFIX(pt_neg_chud) (&out[10], &out[5]);
	PREFIX(pt_neg_chud) (&out[11], &out[4]);
	PREFIX(pt_neg_chud) (&out[12], &out[3]);
	PREFIX(pt_neg_chud) (&out[13], &out[2]);
	PREFIX(pt_neg_chud) (&out[14], &out[1]);
	PREFIX(pt_neg_chud) (&out[15], &out[0]);
}

/* Expects out to be an array of size 16 of Jacobian points. Fills in
 * Jacobian form (x, y, z), for O, P, 2P, ... 15P */
void PREFIX(precompute_jac) (ecfp_jac_pt * precomp, const ecfp_aff_pt * p,
							 const EC_group_fp * group) {
	int i;

	/* fill precomputation table */
	/* set precomp[0] */
	PREFIX(set_pt_inf_jac) (&precomp[0]);
	/* set precomp[1] */
	PREFIX(copy) (precomp[1].x, p->x);
	PREFIX(copy) (precomp[1].y, p->y);
	if (PREFIX(pt_is_inf_aff) (p) == MP_YES) {
		PREFIX(zero) (precomp[1].z);
	} else {
		PREFIX(one) (precomp[1].z);
	}
	/* set precomp[2] */
	group->pt_dbl_jac(&precomp[1], &precomp[2], group);

	/* set rest of precomp */
	for (i = 3; i < 16; i++) {
		group->pt_add_jac_aff(&precomp[i - 1], p, &precomp[i], group);
	}
}
