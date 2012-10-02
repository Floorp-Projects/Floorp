/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mpi.h"
#include "mplogic.h"
#include "ecl.h"
#include "ecp.h"
#include "ecl-priv.h"

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

/* Returns 2^e as an integer. This is meant to be used for small powers of 
 * two. */
int ec_twoTo(int e);

/* Number of bits of scalar to test */
#define BITSIZE 160

/* Time k repetitions of operation op. */
#define M_TimeOperation(op, k) { \
	double dStart, dNow, dUserTime; \
	struct rusage ru; \
	int i; \
	getrusage(RUSAGE_SELF, &ru); \
	dStart = (double)ru.ru_utime.tv_sec+(double)ru.ru_utime.tv_usec*0.000001; \
	for (i = 0; i < k; i++) { \
		{ op; } \
	}; \
	getrusage(RUSAGE_SELF, &ru); \
	dNow = (double)ru.ru_utime.tv_sec+(double)ru.ru_utime.tv_usec*0.000001; \
	dUserTime = dNow-dStart; \
	if (dUserTime) printf("    %-45s\n      k: %6i, t: %6.2f sec\n", #op, k, dUserTime); \
}

/* Tests wNAF computation. Non-adjacent-form is discussed in the paper: D. 
 * Hankerson, J. Hernandez and A. Menezes, "Software implementation of
 * elliptic curve cryptography over binary fields", Proc. CHES 2000. */

mp_err
main(void)
{
	signed char naf[BITSIZE + 1];
	ECGroup *group = NULL;
	mp_int k;
	mp_int *scalar;
	int i, count;
	int res;
	int w = 5;
	char s[1000];

	/* Get a 160 bit scalar to compute wNAF from */
	group = ECGroup_fromName(ECCurve_SECG_PRIME_160R1);
	scalar = &group->genx;

	/* Compute wNAF representation of scalar */
	ec_compute_wNAF(naf, BITSIZE, scalar, w);

	/* Verify correctness of representation */
	mp_init(&k);				/* init k to 0 */

	for (i = BITSIZE; i >= 0; i--) {
		mp_add(&k, &k, &k);
		/* digits in mp_???_d are unsigned */
		if (naf[i] >= 0) {
			mp_add_d(&k, naf[i], &k);
		} else {
			mp_sub_d(&k, -naf[i], &k);
		}
	}

	if (mp_cmp(&k, scalar) != 0) {
		printf("Error:  incorrect NAF value.\n");
		MP_CHECKOK(mp_toradix(&k, s, 16));
		printf("NAF value   %s\n", s);
		MP_CHECKOK(mp_toradix(scalar, s, 16));
		printf("original value   %s\n", s);
		goto CLEANUP;
	}

	/* Verify digits of representation are valid */
	for (i = 0; i <= BITSIZE; i++) {
		if (naf[i] % 2 == 0 && naf[i] != 0) {
			printf("Error:  Even non-zero digit found.\n");
			goto CLEANUP;
		}
		if (naf[i] < -(ec_twoTo(w - 1)) || naf[i] >= ec_twoTo(w - 1)) {
			printf("Error:  Magnitude of naf digit too large.\n");
			goto CLEANUP;
		}
	}

	/* Verify sparsity of representation */
	count = w - 1;
	for (i = 0; i <= BITSIZE; i++) {
		if (naf[i] != 0) {
			if (count < w - 1) {
				printf("Error:  Sparsity failed.\n");
				goto CLEANUP;
			}
			count = 0;
		} else
			count++;
	}

	/* Check timing */
	M_TimeOperation(ec_compute_wNAF(naf, BITSIZE, scalar, w), 10000);

	printf("Test passed.\n");
  CLEANUP:
	ECGroup_free(group);
	return MP_OKAY;
}
