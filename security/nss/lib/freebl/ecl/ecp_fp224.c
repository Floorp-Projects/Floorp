/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecp_fp.h"
#include <stdlib.h>

#define ECFP_BSIZE 224
#define ECFP_NUMDOUBLES 10

#include "ecp_fpinc.c"

/* Performs a single step of reduction, just on the uppermost float
 * (assumes already tidied), and then retidies. Note, this does not
 * guarantee that the result will be less than p. */
void
ecfp224_singleReduce(double *r, const EC_group_fp * group)
{
	double q;

	ECFP_ASSERT(group->doubleBitSize == 24);
	ECFP_ASSERT(group->primeBitSize == 224);
	ECFP_ASSERT(group->numDoubles == 10);

	q = r[ECFP_NUMDOUBLES - 1] - ecfp_beta_224;
	q += group->bitSize_alpha;
	q -= group->bitSize_alpha;

	r[ECFP_NUMDOUBLES - 1] -= q;
	r[0] -= q * ecfp_twom224;
	r[4] += q * ecfp_twom128;

	ecfp_positiveTidy(r, group);
}

/* 
 * Performs imperfect reduction.  This might leave some negative terms,
 * and one more reduction might be required for the result to be between 0 
 * and p-1. x should be be an array of at least 20, and r at least 10 x
 * and r can be the same, but then the upper parts of r are not zeroed */
void
ecfp224_reduce(double *r, double *x, const EC_group_fp * group)
{

	double x10, x11, x12, x13, x14, q;

	ECFP_ASSERT(group->doubleBitSize == 24);
	ECFP_ASSERT(group->primeBitSize == 224);
	ECFP_ASSERT(group->numDoubles == 10);

	/* Tidy just the upper bits of x.  Don't need to tidy the lower ones
	 * yet. */
	ecfp_tidyUpper(x, group);

	x10 = x[10] + x[16] * ecfp_twom128;
	x11 = x[11] + x[17] * ecfp_twom128;
	x12 = x[12] + x[18] * ecfp_twom128;
	x13 = x[13] + x[19] * ecfp_twom128;

	/* Tidy up, or we won't have enough bits later to add it in */
	q = x10 + group->alpha[11];
	q -= group->alpha[11];
	x10 -= q;
	x11 = x11 + q;

	q = x11 + group->alpha[12];
	q -= group->alpha[12];
	x11 -= q;
	x12 = x12 + q;

	q = x12 + group->alpha[13];
	q -= group->alpha[13];
	x12 -= q;
	x13 = x13 + q;

	q = x13 + group->alpha[14];
	q -= group->alpha[14];
	x13 -= q;
	x14 = x[14] + q;

	r[9] = x[9] + x[15] * ecfp_twom128 - x[19] * ecfp_twom224;
	r[8] = x[8] + x14 * ecfp_twom128 - x[18] * ecfp_twom224;
	r[7] = x[7] + x13 * ecfp_twom128 - x[17] * ecfp_twom224;
	r[6] = x[6] + x12 * ecfp_twom128 - x[16] * ecfp_twom224;
	r[5] = x[5] + x11 * ecfp_twom128 - x[15] * ecfp_twom224;
	r[4] = x[4] + x10 * ecfp_twom128 - x14 * ecfp_twom224;
	r[3] = x[3] - x13 * ecfp_twom224;
	r[2] = x[2] - x12 * ecfp_twom224;
	r[1] = x[1] - x11 * ecfp_twom224;
	r[0] = x[0] - x10 * ecfp_twom224;

	/* 
	 * Tidy up just r[ECFP_NUMDOUBLES-2] so that the number of reductions
	 * is accurate plus or minus one.  (Rather than tidy all to make it
	 * totally accurate) */
	q = r[ECFP_NUMDOUBLES - 2] + group->alpha[ECFP_NUMDOUBLES - 1];
	q -= group->alpha[ECFP_NUMDOUBLES - 1];
	r[ECFP_NUMDOUBLES - 2] -= q;
	r[ECFP_NUMDOUBLES - 1] += q;

	/* Tidy up the excess bits on r[ECFP_NUMDOUBLES-1] using reduction */
	/* Use ecfp_beta so we get a positive res */
	q = r[ECFP_NUMDOUBLES - 1] - ecfp_beta_224;
	q += group->bitSize_alpha;
	q -= group->bitSize_alpha;

	r[ECFP_NUMDOUBLES - 1] -= q;
	r[0] -= q * ecfp_twom224;
	r[4] += q * ecfp_twom128;

	ecfp_tidyShort(r, group);
}

/* Sets group to use optimized calculations in this file */
mp_err
ec_group_set_nistp224_fp(ECGroup *group)
{

	EC_group_fp *fpg;

	/* Allocate memory for floating point group data */
	fpg = (EC_group_fp *) malloc(sizeof(EC_group_fp));
	if (fpg == NULL) {
		return MP_MEM;
	}

	fpg->numDoubles = ECFP_NUMDOUBLES;
	fpg->primeBitSize = ECFP_BSIZE;
	fpg->orderBitSize = 224;
	fpg->doubleBitSize = 24;
	fpg->numInts = (ECFP_BSIZE + ECL_BITS - 1) / ECL_BITS;
	fpg->aIsM3 = 1;
	fpg->ecfp_singleReduce = &ecfp224_singleReduce;
	fpg->ecfp_reduce = &ecfp224_reduce;
	fpg->ecfp_tidy = &ecfp_tidy;

	fpg->pt_add_jac_aff = &ecfp224_pt_add_jac_aff;
	fpg->pt_add_jac = &ecfp224_pt_add_jac;
	fpg->pt_add_jm_chud = &ecfp224_pt_add_jm_chud;
	fpg->pt_add_chud = &ecfp224_pt_add_chud;
	fpg->pt_dbl_jac = &ecfp224_pt_dbl_jac;
	fpg->pt_dbl_jm = &ecfp224_pt_dbl_jm;
	fpg->pt_dbl_aff2chud = &ecfp224_pt_dbl_aff2chud;
	fpg->precompute_chud = &ecfp224_precompute_chud;
	fpg->precompute_jac = &ecfp224_precompute_jac;

	group->point_mul = &ec_GFp_point_mul_wNAF_fp;
	group->points_mul = &ec_pts_mul_basic;
	group->extra1 = fpg;
	group->extra_free = &ec_GFp_extra_free_fp;

	ec_set_fp_precision(fpg);
	fpg->bitSize_alpha = ECFP_TWO224 * fpg->alpha[0];

	return MP_OKAY;
}
