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
 *      Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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

#include "ecp.h"
#include "mpi.h"
#include "mplogic.h"
#include "mpi-priv.h"
#include <stdlib.h>

/* Fast modular reduction for p192 = 2^192 - 2^64 - 1.  a can be r. Uses
 * algorithm 7 from Brown, Hankerson, Lopez, Menezes. Software
 * Implementation of the NIST Elliptic Curves over Prime Fields. */
mp_err
ec_GFp_nistp192_mod(const mp_int *a, mp_int *r, const GFMethod *meth)
{
	mp_err res = MP_OKAY;
	mp_size a_used = MP_USED(a);

	/* s is a statically-allocated mp_int of exactly the size we need */
	mp_int s;

#ifdef ECL_THIRTY_TWO_BIT
	mp_digit sa[6];
	mp_digit a11 = 0, a10, a9 = 0, a8, a7 = 0, a6;

	MP_SIGN(&s) = MP_ZPOS;
	MP_ALLOC(&s) = 6;
	MP_USED(&s) = 6;
	MP_DIGITS(&s) = sa;
#else
	mp_digit sa[3];
	mp_digit a5 = 0, a4 = 0, a3 = 0;

	MP_SIGN(&s) = MP_ZPOS;
	MP_ALLOC(&s) = 3;
	MP_USED(&s) = 3;
	MP_DIGITS(&s) = sa;
#endif

	/* reduction not needed if a is not larger than field size */
#ifdef ECL_THIRTY_TWO_BIT
	if (a_used < 6) {
#else
	if (a_used < 3) {
#endif
		return mp_copy(a, r);
	}
#ifdef ECL_THIRTY_TWO_BIT
	/* for polynomials larger than twice the field size, use regular
	 * reduction */
	if (a_used > 12) {
		MP_CHECKOK(mp_mod(a, &meth->irr, r));
	} else {
		/* copy out upper words of a */
		switch (a_used) {
		case 12:
			a11 = MP_DIGIT(a, 11);
		case 11:
			a10 = MP_DIGIT(a, 10);
		case 10:
			a9 = MP_DIGIT(a, 9);
		case 9:
			a8 = MP_DIGIT(a, 8);
		case 8:
			a7 = MP_DIGIT(a, 7);
		case 7:
			a6 = MP_DIGIT(a, 6);
		}
		/* set the lower words of r */
		if (a != r) {
			MP_CHECKOK(s_mp_pad(r, 7));
			MP_DIGIT(r, 5) = MP_DIGIT(a, 5);
			MP_DIGIT(r, 4) = MP_DIGIT(a, 4);
			MP_DIGIT(r, 3) = MP_DIGIT(a, 3);
			MP_DIGIT(r, 2) = MP_DIGIT(a, 2);
			MP_DIGIT(r, 1) = MP_DIGIT(a, 1);
			MP_DIGIT(r, 0) = MP_DIGIT(a, 0);
		}
		MP_USED(r) = 6;
		/* compute r = s1 + s2 + s3 + s4, where s1 = (a2,a1,a0), s2 =
		 * (0,a3,a3), s3 = (a4,a4,0), and s4 = (a5,a5,a5), for
		 * sixty-four-bit words */
		switch (a_used) {
		case 12:
		case 11:
			sa[5] = sa[3] = sa[1] = a11;
			sa[4] = sa[2] = sa[0] = a10;
			MP_CHECKOK(mp_add(r, &s, r));
		case 10:
		case 9:
			sa[5] = sa[3] = a9;
			sa[4] = sa[2] = a8;
			sa[1] = sa[0] = 0;
			MP_CHECKOK(mp_add(r, &s, r));
		case 8:
		case 7:
			sa[5] = sa[4] = 0;
			sa[3] = sa[1] = a7;
			sa[2] = sa[0] = a6;
			MP_CHECKOK(mp_add(r, &s, r));
		}
		/* there might be 1 or 2 bits left to reduce; use regular
		 * reduction for this */
		MP_CHECKOK(mp_mod(r, &meth->irr, r));
	}
#else
	/* for polynomials larger than twice the field size, use regular
	 * reduction */
	if (a_used > 6) {
		MP_CHECKOK(mp_mod(a, &meth->irr, r));
	} else {
		/* copy out upper words of a */
		switch (a_used) {
		case 6:
			a5 = MP_DIGIT(a, 5);
		case 5:
			a4 = MP_DIGIT(a, 4);
		case 4:
			a3 = MP_DIGIT(a, 3);
		}
		/* set the lower words of r */
		if (a != r) {
			MP_CHECKOK(s_mp_pad(r, 4));
			MP_DIGIT(r, 2) = MP_DIGIT(a, 2);
			MP_DIGIT(r, 1) = MP_DIGIT(a, 1);
			MP_DIGIT(r, 0) = MP_DIGIT(a, 0);
		}
		MP_USED(r) = 3;
		/* compute r = s1 + s2 + s3 + s4, where s1 = (a2,a1,a0), s2 =
		 * (0,a3,a3), s3 = (a4,a4,0), and s4 = (a5,a5,a5) */
		switch (a_used) {
		case 6:
			sa[2] = sa[1] = sa[0] = a5;
			MP_CHECKOK(mp_add(r, &s, r));
		case 5:
			sa[2] = sa[1] = a4;
			sa[0] = 0;
			MP_CHECKOK(mp_add(r, &s, r));
		case 4:
			sa[2] = 0;
			sa[1] = sa[0] = a3;
			MP_CHECKOK(mp_add(r, &s, r));
		}
		/* there might be 1 or 2 bits left to reduce; use regular
		 * reduction for this */
		MP_CHECKOK(mp_mod(r, &meth->irr, r));
	}
#endif

  CLEANUP:
	return res;
}

/* Compute the square of polynomial a, reduce modulo p192. Store the
 * result in r.  r could be a.  Uses optimized modular reduction for p192. 
 */
mp_err
ec_GFp_nistp192_sqr(const mp_int *a, mp_int *r, const GFMethod *meth)
{
	mp_err res = MP_OKAY;

	MP_CHECKOK(mp_sqr(a, r));
	MP_CHECKOK(ec_GFp_nistp192_mod(r, r, meth));
  CLEANUP:
	return res;
}

/* Compute the product of two polynomials a and b, reduce modulo p192.
 * Store the result in r.  r could be a or b; a could be b.  Uses
 * optimized modular reduction for p192. */
mp_err
ec_GFp_nistp192_mul(const mp_int *a, const mp_int *b, mp_int *r,
					const GFMethod *meth)
{
	mp_err res = MP_OKAY;

	MP_CHECKOK(mp_mul(a, b, r));
	MP_CHECKOK(ec_GFp_nistp192_mod(r, r, meth));
  CLEANUP:
	return res;
}

/* Wire in fast field arithmetic and precomputation of base point for
 * named curves. */
mp_err
ec_group_set_gfp192(ECGroup *group, ECCurveName name)
{
	if (name == ECCurve_NIST_P192) {
		group->meth->field_mod = &ec_GFp_nistp192_mod;
		group->meth->field_mul = &ec_GFp_nistp192_mul;
		group->meth->field_sqr = &ec_GFp_nistp192_sqr;
	}
	return MP_OKAY;
}
