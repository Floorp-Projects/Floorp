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
 * The Original Code is the elliptic curve math library for prime field curves.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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

#include "ecp.h"
#include "mpi.h"
#include "mplogic.h"
#include "mpi-priv.h"
#include <stdlib.h>

/* Fast modular reduction for p224 = 2^224 - 2^96 + 1.  a can be r. Uses
 * algorithm 7 from Brown, Hankerson, Lopez, Menezes. Software
 * Implementation of the NIST Elliptic Curves over Prime Fields. */
mp_err
ec_GFp_nistp224_mod(const mp_int *a, mp_int *r, const GFMethod *meth)
{
	mp_err res = MP_OKAY;
	mp_size a_used = MP_USED(a);

	/* s is a statically-allocated mp_int of exactly the size we need */
	mp_int s;

#ifdef ECL_THIRTY_TWO_BIT
	mp_digit sa[8];
	mp_digit a13 = 0, a12 = 0, a11 = 0, a10, a9 = 0, a8, a7;

	MP_SIGN(&s) = MP_ZPOS;
	MP_ALLOC(&s) = 8;
	MP_USED(&s) = 7;
	MP_DIGITS(&s) = sa;
#else
	mp_digit sa[4];
	mp_digit a6 = 0, a5 = 0, a4 = 0, a3 = 0;

	MP_SIGN(&s) = MP_ZPOS;
	MP_ALLOC(&s) = 4;
	MP_USED(&s) = 4;
	MP_DIGITS(&s) = sa;
#endif

	/* reduction not needed if a is not larger than field size */
#ifdef ECL_THIRTY_TWO_BIT
	if (a_used < 8) {
#else
	if (a_used < 4) {
#endif
		return mp_copy(a, r);
	}
#ifdef ECL_THIRTY_TWO_BIT
	/* for polynomials larger than twice the field size, use regular
	 * reduction */
	if (a_used > 14) {
		MP_CHECKOK(mp_mod(a, &meth->irr, r));
	} else {
		/* copy out upper words of a */
		switch (a_used) {
		case 14:
			a13 = MP_DIGIT(a, 13);
		case 13:
			a12 = MP_DIGIT(a, 12);
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
		}
		/* set the lower words of r */
		if (a != r) {
			MP_CHECKOK(s_mp_pad(r, 8));
			MP_DIGIT(r, 6) = MP_DIGIT(a, 6);
			MP_DIGIT(r, 5) = MP_DIGIT(a, 5);
			MP_DIGIT(r, 4) = MP_DIGIT(a, 4);
			MP_DIGIT(r, 3) = MP_DIGIT(a, 3);
			MP_DIGIT(r, 2) = MP_DIGIT(a, 2);
			MP_DIGIT(r, 1) = MP_DIGIT(a, 1);
			MP_DIGIT(r, 0) = MP_DIGIT(a, 0);
		}
		MP_USED(r) = 7;
		switch (a_used) {
		case 14:
		case 13:
		case 12:
		case 11:
			sa[6] = a10;
		case 10:
			sa[5] = a9;
		case 9:
			sa[4] = a8;
		case 8:
			sa[3] = a7;
			sa[2] = sa[1] = sa[0] = 0;
			MP_USED(&s) = a_used - 4;
			if (MP_USED(&s) > 7)
				MP_USED(&s) = 7;
			MP_CHECKOK(mp_add(r, &s, r));
		}
		switch (a_used) {
		case 14:
			sa[5] = a13;
		case 13:
			sa[4] = a12;
		case 12:
			sa[3] = a11;
			sa[2] = sa[1] = sa[0] = 0;
			MP_USED(&s) = a_used - 8;
			MP_CHECKOK(mp_add(r, &s, r));
		}
		switch (a_used) {
		case 14:
			sa[6] = a13;
		case 13:
			sa[5] = a12;
		case 12:
			sa[4] = a11;
		case 11:
			sa[3] = a10;
		case 10:
			sa[2] = a9;
		case 9:
			sa[1] = a8;
		case 8:
			sa[0] = a7;
			MP_USED(&s) = a_used - 7;
			MP_CHECKOK(mp_sub(r, &s, r));
		}
		switch (a_used) {
		case 14:
			sa[2] = a13;
		case 13:
			sa[1] = a12;
		case 12:
			sa[0] = a11;
			MP_USED(&s) = a_used - 11;
			MP_CHECKOK(mp_sub(r, &s, r));
		}
		/* there might be 1 or 2 bits left to reduce; use regular
		 * reduction for this */
		MP_CHECKOK(mp_mod(r, &meth->irr, r));
	}
#else
	/* for polynomials larger than twice the field size, use regular
	 * reduction */
	if (a_used > 7) {
		MP_CHECKOK(mp_mod(a, &meth->irr, r));
	} else {
		/* copy out upper words of a */
		switch (a_used) {
		case 7:
			a6 = MP_DIGIT(a, 6);
		case 6:
			a5 = MP_DIGIT(a, 5);
		case 5:
			a4 = MP_DIGIT(a, 4);
		case 4:
			a3 = MP_DIGIT(a, 3) >> 32;
		}
		/* set the lower words of r */
		if (a != r) {
			MP_CHECKOK(s_mp_pad(r, 5));
			MP_DIGIT(r, 3) = MP_DIGIT(a, 3) & 0xFFFFFFFF;
			MP_DIGIT(r, 2) = MP_DIGIT(a, 2);
			MP_DIGIT(r, 1) = MP_DIGIT(a, 1);
			MP_DIGIT(r, 0) = MP_DIGIT(a, 0);
		} else {
			MP_DIGIT(r, 3) &= 0xFFFFFFFF;
		}
		MP_USED(r) = 4;
		switch (a_used) {
		case 7:
		case 6:
			sa[3] = a5 & 0xFFFFFFFF;
		case 5:
			sa[2] = a4;
		case 4:
			sa[1] = a3 << 32;
			sa[0] = 0;
			MP_USED(&s) = a_used - 2;
			if (MP_USED(&s) == 5)
				MP_USED(&s) = 4;
			MP_CHECKOK(mp_add(r, &s, r));
		}
		switch (a_used) {
		case 7:
			sa[2] = a6;
		case 6:
			sa[1] = (a5 >> 32) << 32;
			sa[0] = 0;
			MP_USED(&s) = a_used - 4;
			MP_CHECKOK(mp_add(r, &s, r));
		}
		sa[2] = sa[1] = sa[0] = 0;
		switch (a_used) {
		case 7:
			sa[3] = a6 >> 32;
			sa[2] = a6 << 32;
		case 6:
			sa[2] |= a5 >> 32;
			sa[1] = a5 << 32;
		case 5:
			sa[1] |= a4 >> 32;
			sa[0] = a4 << 32;
		case 4:
			sa[0] |= a3;
			MP_USED(&s) = a_used - 3;
			MP_CHECKOK(mp_sub(r, &s, r));
		}
		sa[0] = 0;
		switch (a_used) {
		case 7:
			sa[1] = a6 >> 32;
			sa[0] = a6 << 32;
		case 6:
			sa[0] |= a5 >> 32;
			MP_USED(&s) = a_used - 5;
			MP_CHECKOK(mp_sub(r, &s, r));
		}
		/* there might be 1 or 2 bits left to reduce; use regular
		 * reduction for this */
		MP_CHECKOK(mp_mod(r, &meth->irr, r));
	}
#endif

  CLEANUP:
	return res;
}

/* Compute the square of polynomial a, reduce modulo p224. Store the
 * result in r.  r could be a.  Uses optimized modular reduction for p224. 
 */
mp_err
ec_GFp_nistp224_sqr(const mp_int *a, mp_int *r, const GFMethod *meth)
{
	mp_err res = MP_OKAY;

	MP_CHECKOK(mp_sqr(a, r));
	MP_CHECKOK(ec_GFp_nistp224_mod(r, r, meth));
  CLEANUP:
	return res;
}

/* Compute the product of two polynomials a and b, reduce modulo p224.
 * Store the result in r.  r could be a or b; a could be b.  Uses
 * optimized modular reduction for p224. */
mp_err
ec_GFp_nistp224_mul(const mp_int *a, const mp_int *b, mp_int *r,
					const GFMethod *meth)
{
	mp_err res = MP_OKAY;

	MP_CHECKOK(mp_mul(a, b, r));
	MP_CHECKOK(ec_GFp_nistp224_mod(r, r, meth));
  CLEANUP:
	return res;
}

/* Wire in fast field arithmetic and precomputation of base point for
 * named curves. */
mp_err
ec_group_set_gfp224(ECGroup *group, ECCurveName name)
{
	if (name == ECCurve_NIST_P224) {
		group->meth->field_mod = &ec_GFp_nistp224_mod;
		group->meth->field_mul = &ec_GFp_nistp224_mul;
		group->meth->field_sqr = &ec_GFp_nistp224_sqr;
	}
	return MP_OKAY;
}
