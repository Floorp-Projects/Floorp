/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mpi.h"
#include "mplogic.h"
#include "mpprime.h"
#include "mp_gf2m.h"
#include "ecl.h"
#include "ecl-curve.h"
#include "ec2.h"
#include <stdio.h>
#include <strings.h>
#include <assert.h>

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

/* Time k repetitions of operation op. */
#define M_TimeOperation(op, k)                                                        \
    {                                                                                 \
        double dStart, dNow, dUserTime;                                               \
        struct rusage ru;                                                             \
        int i;                                                                        \
        getrusage(RUSAGE_SELF, &ru);                                                  \
        dStart = (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec * 0.000001; \
        for (i = 0; i < k; i++) {                                                     \
            {                                                                         \
                op;                                                                   \
            }                                                                         \
        };                                                                            \
        getrusage(RUSAGE_SELF, &ru);                                                  \
        dNow = (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec * 0.000001;   \
        dUserTime = dNow - dStart;                                                    \
        if (dUserTime)                                                                \
            printf("    %-45s k: %6i, t: %6.2f sec\n", #op, k, dUserTime);            \
    }

/* Test curve using generic field arithmetic. */
#define ECTEST_GENERIC_GF2M(name_c, name)                             \
    printf("Testing %s using generic implementation...\n", name_c);   \
    params = EC_GetNamedCurveParams(name);                            \
    if (params == NULL) {                                             \
        printf("  Error: could not construct params.\n");             \
        res = MP_NO;                                                  \
        goto CLEANUP;                                                 \
    }                                                                 \
    ECGroup_free(group);                                              \
    group = ECGroup_fromHex(params);                                  \
    if (group == NULL) {                                              \
        printf("  Error: could not construct group.\n");              \
        res = MP_NO;                                                  \
        goto CLEANUP;                                                 \
    }                                                                 \
    MP_CHECKOK(ectest_curve_GF2m(group, ectestPrint, ectestTime, 1)); \
    printf("... okay.\n");

/* Test curve using specific field arithmetic. */
#define ECTEST_NAMED_GF2M(name_c, name)                                   \
    printf("Testing %s using specific implementation...\n", name_c);      \
    ECGroup_free(group);                                                  \
    group = ECGroup_fromName(name);                                       \
    if (group == NULL) {                                                  \
        printf("  Warning: could not construct group.\n");                \
        printf("... failed; continuing with remaining tests.\n");         \
    } else {                                                              \
        MP_CHECKOK(ectest_curve_GF2m(group, ectestPrint, ectestTime, 0)); \
        printf("... okay.\n");                                            \
    }

/* Performs basic tests of elliptic curve cryptography over binary
 * polynomial fields. If tests fail, then it prints an error message,
 * aborts, and returns an error code. Otherwise, returns 0. */
int
ectest_curve_GF2m(ECGroup *group, int ectestPrint, int ectestTime,
                  int generic)
{

    mp_int one, order_1, gx, gy, rx, ry, n;
    int size;
    mp_err res;
    char s[1000];

    /* initialize values */
    MP_CHECKOK(mp_init(&one));
    MP_CHECKOK(mp_init(&order_1));
    MP_CHECKOK(mp_init(&gx));
    MP_CHECKOK(mp_init(&gy));
    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&n));

    MP_CHECKOK(mp_set_int(&one, 1));
    MP_CHECKOK(mp_sub(&group->order, &one, &order_1));

    /* encode base point */
    if (group->meth->field_dec) {
        MP_CHECKOK(group->meth->field_dec(&group->genx, &gx, group->meth));
        MP_CHECKOK(group->meth->field_dec(&group->geny, &gy, group->meth));
    } else {
        MP_CHECKOK(mp_copy(&group->genx, &gx));
        MP_CHECKOK(mp_copy(&group->geny, &gy));
    }

    if (ectestPrint) {
        /* output base point */
        printf("  base point P:\n");
        MP_CHECKOK(mp_toradix(&gx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&gy, s, 16));
        printf("    %s\n", s);
        if (group->meth->field_enc) {
            printf("  base point P (encoded):\n");
            MP_CHECKOK(mp_toradix(&group->genx, s, 16));
            printf("    %s\n", s);
            MP_CHECKOK(mp_toradix(&group->geny, s, 16));
            printf("    %s\n", s);
        }
    }

#ifdef ECL_ENABLE_GF2M_PT_MUL_AFF
    /* multiply base point by order - 1 and check for negative of base
	 * point */
    MP_CHECKOK(ec_GF2m_pt_mul_aff(&order_1, &group->genx, &group->geny, &rx, &ry, group));
    if (ectestPrint) {
        printf("  (order-1)*P (affine):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(group->meth->field_add(&ry, &rx, &ry, group->meth));
    if ((mp_cmp(&rx, &group->genx) != 0) || (mp_cmp(&ry, &group->geny) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }
#endif

    /* multiply base point by order - 1 and check for negative of base
	 * point */
    MP_CHECKOK(ec_GF2m_pt_mul_mont(&order_1, &group->genx, &group->geny, &rx, &ry, group));
    if (ectestPrint) {
        printf("  (order-1)*P (montgomery):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(group->meth->field_add(&ry, &rx, &ry, group->meth));
    if ((mp_cmp(&rx, &group->genx) != 0) || (mp_cmp(&ry, &group->geny) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }

#ifdef ECL_ENABLE_GF2M_PROJ
    /* multiply base point by order - 1 and check for negative of base
	 * point */
    MP_CHECKOK(ec_GF2m_pt_mul_proj(&order_1, &group->genx, &group->geny, &rx, &ry, group));
    if (ectestPrint) {
        printf("  (order-1)*P (projective):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(group->meth->field_add(&ry, &rx, &ry, group->meth));
    if ((mp_cmp(&rx, &group->genx) != 0) || (mp_cmp(&ry, &group->geny) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }
#endif

    /* multiply base point by order - 1 and check for negative of base
	 * point */
    MP_CHECKOK(ECPoint_mul(group, &order_1, NULL, NULL, &rx, &ry));
    if (ectestPrint) {
        printf("  (order-1)*P (ECPoint_mul):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(ec_GF2m_add(&ry, &rx, &ry, group->meth));
    if ((mp_cmp(&rx, &gx) != 0) || (mp_cmp(&ry, &gy) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }

    /* multiply base point by order - 1 and check for negative of base
	 * point */
    MP_CHECKOK(ECPoint_mul(group, &order_1, &gx, &gy, &rx, &ry));
    if (ectestPrint) {
        printf("  (order-1)*P (ECPoint_mul):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(ec_GF2m_add(&ry, &rx, &ry, group->meth));
    if ((mp_cmp(&rx, &gx) != 0) || (mp_cmp(&ry, &gy) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }

#ifdef ECL_ENABLE_GF2M_PT_MUL_AFF
    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ec_GF2m_pt_mul_aff(&group->order, &group->genx, &group->geny, &rx, &ry,
                                  group));
    if (ectestPrint) {
        printf("  (order)*P (affine):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GF2m_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: invalid result (expected point at infinity).\n");
        res = MP_NO;
        goto CLEANUP;
    }
#endif

    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ec_GF2m_pt_mul_mont(&group->order, &group->genx, &group->geny, &rx, &ry,
                                   group));
    if (ectestPrint) {
        printf("  (order)*P (montgomery):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GF2m_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: invalid result (expected point at infinity).\n");
        res = MP_NO;
        goto CLEANUP;
    }

#ifdef ECL_ENABLE_GF2M_PROJ
    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ec_GF2m_pt_mul_proj(&group->order, &group->genx, &group->geny, &rx, &ry,
                                   group));
    if (ectestPrint) {
        printf("  (order)*P (projective):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GF2m_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: invalid result (expected point at infinity).\n");
        res = MP_NO;
        goto CLEANUP;
    }
#endif

    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ECPoint_mul(group, &group->order, NULL, NULL, &rx, &ry));
    if (ectestPrint) {
        printf("  (order)*P (ECPoint_mul):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GF2m_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: invalid result (expected point at infinity).\n");
        res = MP_NO;
        goto CLEANUP;
    }

    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ECPoint_mul(group, &group->order, &gx, &gy, &rx, &ry));
    if (ectestPrint) {
        printf("  (order)*P (ECPoint_mul):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GF2m_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: invalid result (expected point at infinity).\n");
        res = MP_NO;
        goto CLEANUP;
    }

    /* check that (order-1)P + (order-1)P + P == (order-1)P */
    MP_CHECKOK(ECPoints_mul(group, &order_1, &order_1, &gx, &gy, &rx, &ry));
    MP_CHECKOK(ECPoints_mul(group, &one, &one, &rx, &ry, &rx, &ry));
    if (ectestPrint) {
        printf("  (order-1)*P + (order-1)*P + P == (order-1)*P (ECPoints_mul):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(ec_GF2m_add(&ry, &rx, &ry, group->meth));
    if ((mp_cmp(&rx, &gx) != 0) || (mp_cmp(&ry, &gy) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }

    /* test validate_point function */
    if (ECPoint_validate(group, &gx, &gy) != MP_YES) {
        printf("  Error: validate point on base point failed.\n");
        res = MP_NO;
        goto CLEANUP;
    }
    MP_CHECKOK(mp_add_d(&gy, 1, &ry));
    if (ECPoint_validate(group, &gx, &ry) != MP_NO) {
        printf("  Error: validate point on invalid point passed.\n");
        res = MP_NO;
        goto CLEANUP;
    }

    if (ectestTime) {
        /* compute random scalar */
        size = mpl_significant_bits(&group->meth->irr);
        if (size < MP_OKAY) {
            goto CLEANUP;
        }
        MP_CHECKOK(mpp_random_size(&n, (size + ECL_BITS - 1) / ECL_BITS));
        MP_CHECKOK(group->meth->field_mod(&n, &n, group->meth));
        /* timed test */
        if (generic) {
#ifdef ECL_ENABLE_GF2M_PT_MUL_AFF
            M_TimeOperation(MP_CHECKOK(ec_GF2m_pt_mul_aff(&n, &group->genx, &group->geny, &rx, &ry,
                                                          group)),
                            100);
#endif
            M_TimeOperation(MP_CHECKOK(ECPoint_mul(group, &n, NULL, NULL, &rx, &ry)),
                            100);
            M_TimeOperation(MP_CHECKOK(ECPoints_mul(group, &n, &n, &gx, &gy, &rx, &ry)), 100);
        } else {
            M_TimeOperation(MP_CHECKOK(ECPoint_mul(group, &n, NULL, NULL, &rx, &ry)),
                            100);
            M_TimeOperation(MP_CHECKOK(ECPoint_mul(group, &n, &gx, &gy, &rx, &ry)),
                            100);
            M_TimeOperation(MP_CHECKOK(ECPoints_mul(group, &n, &n, &gx, &gy, &rx, &ry)), 100);
        }
    }

CLEANUP:
    mp_clear(&one);
    mp_clear(&order_1);
    mp_clear(&gx);
    mp_clear(&gy);
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&n);
    if (res != MP_OKAY) {
        printf("  Error: exiting with error value %i\n", res);
    }
    return res;
}

/* Prints help information. */
void
printUsage()
{
    printf("Usage: ecp_test [--print] [--time]\n");
    printf("	--print     Print out results of each point arithmetic test.\n");
    printf("	--time      Benchmark point operations and print results.\n");
}

/* Performs tests of elliptic curve cryptography over binary polynomial
 * fields. If tests fail, then it prints an error message, aborts, and
 * returns an error code. Otherwise, returns 0. */
int
main(int argv, char **argc)
{

    int ectestTime = 0;
    int ectestPrint = 0;
    int i;
    ECGroup *group = NULL;
    ECCurveParams *params = NULL;
    mp_err res;

    /* read command-line arguments */
    for (i = 1; i < argv; i++) {
        if ((strcasecmp(argc[i], "time") == 0) || (strcasecmp(argc[i], "-time") == 0) || (strcasecmp(argc[i], "--time") == 0)) {
            ectestTime = 1;
        } else if ((strcasecmp(argc[i], "print") == 0) || (strcasecmp(argc[i], "-print") == 0) || (strcasecmp(argc[i], "--print") == 0)) {
            ectestPrint = 1;
        } else {
            printUsage();
            return 0;
        }
    }

    /* generic arithmetic tests */
    ECTEST_GENERIC_GF2M("SECT-131R1", ECCurve_SECG_CHAR2_131R1);

    /* specific arithmetic tests */
    ECTEST_NAMED_GF2M("NIST-K163", ECCurve_NIST_K163);
    ECTEST_NAMED_GF2M("NIST-B163", ECCurve_NIST_B163);
    ECTEST_NAMED_GF2M("NIST-K233", ECCurve_NIST_K233);
    ECTEST_NAMED_GF2M("NIST-B233", ECCurve_NIST_B233);
    ECTEST_NAMED_GF2M("NIST-K283", ECCurve_NIST_K283);
    ECTEST_NAMED_GF2M("NIST-B283", ECCurve_NIST_B283);
    ECTEST_NAMED_GF2M("NIST-K409", ECCurve_NIST_K409);
    ECTEST_NAMED_GF2M("NIST-B409", ECCurve_NIST_B409);
    ECTEST_NAMED_GF2M("NIST-K571", ECCurve_NIST_K571);
    ECTEST_NAMED_GF2M("NIST-B571", ECCurve_NIST_B571);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB163V1", ECCurve_X9_62_CHAR2_PNB163V1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB163V2", ECCurve_X9_62_CHAR2_PNB163V2);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB163V3", ECCurve_X9_62_CHAR2_PNB163V3);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB176V1", ECCurve_X9_62_CHAR2_PNB176V1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB191V1", ECCurve_X9_62_CHAR2_TNB191V1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB191V2", ECCurve_X9_62_CHAR2_TNB191V2);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB191V3", ECCurve_X9_62_CHAR2_TNB191V3);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB208W1", ECCurve_X9_62_CHAR2_PNB208W1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB239V1", ECCurve_X9_62_CHAR2_TNB239V1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB239V2", ECCurve_X9_62_CHAR2_TNB239V2);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB239V3", ECCurve_X9_62_CHAR2_TNB239V3);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB272W1", ECCurve_X9_62_CHAR2_PNB272W1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB304W1", ECCurve_X9_62_CHAR2_PNB304W1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB359V1", ECCurve_X9_62_CHAR2_TNB359V1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2PNB368W1", ECCurve_X9_62_CHAR2_PNB368W1);
    ECTEST_NAMED_GF2M("ANSI X9.62 C2TNB431R1", ECCurve_X9_62_CHAR2_TNB431R1);
    ECTEST_NAMED_GF2M("SECT-113R1", ECCurve_SECG_CHAR2_113R1);
    ECTEST_NAMED_GF2M("SECT-113R2", ECCurve_SECG_CHAR2_113R2);
    ECTEST_NAMED_GF2M("SECT-131R1", ECCurve_SECG_CHAR2_131R1);
    ECTEST_NAMED_GF2M("SECT-131R2", ECCurve_SECG_CHAR2_131R2);
    ECTEST_NAMED_GF2M("SECT-163K1", ECCurve_SECG_CHAR2_163K1);
    ECTEST_NAMED_GF2M("SECT-163R1", ECCurve_SECG_CHAR2_163R1);
    ECTEST_NAMED_GF2M("SECT-163R2", ECCurve_SECG_CHAR2_163R2);
    ECTEST_NAMED_GF2M("SECT-193R1", ECCurve_SECG_CHAR2_193R1);
    ECTEST_NAMED_GF2M("SECT-193R2", ECCurve_SECG_CHAR2_193R2);
    ECTEST_NAMED_GF2M("SECT-233K1", ECCurve_SECG_CHAR2_233K1);
    ECTEST_NAMED_GF2M("SECT-233R1", ECCurve_SECG_CHAR2_233R1);
    ECTEST_NAMED_GF2M("SECT-239K1", ECCurve_SECG_CHAR2_239K1);
    ECTEST_NAMED_GF2M("SECT-283K1", ECCurve_SECG_CHAR2_283K1);
    ECTEST_NAMED_GF2M("SECT-283R1", ECCurve_SECG_CHAR2_283R1);
    ECTEST_NAMED_GF2M("SECT-409K1", ECCurve_SECG_CHAR2_409K1);
    ECTEST_NAMED_GF2M("SECT-409R1", ECCurve_SECG_CHAR2_409R1);
    ECTEST_NAMED_GF2M("SECT-571K1", ECCurve_SECG_CHAR2_571K1);
    ECTEST_NAMED_GF2M("SECT-571R1", ECCurve_SECG_CHAR2_571R1);
    ECTEST_NAMED_GF2M("WTLS-1 (113)", ECCurve_WTLS_1);
    ECTEST_NAMED_GF2M("WTLS-3 (163)", ECCurve_WTLS_3);
    ECTEST_NAMED_GF2M("WTLS-4 (113)", ECCurve_WTLS_4);
    ECTEST_NAMED_GF2M("WTLS-5 (163)", ECCurve_WTLS_5);
    ECTEST_NAMED_GF2M("WTLS-10 (233)", ECCurve_WTLS_10);
    ECTEST_NAMED_GF2M("WTLS-11 (233)", ECCurve_WTLS_11);

CLEANUP:
    EC_FreeCurveParams(params);
    ECGroup_free(group);
    if (res != MP_OKAY) {
        printf("Error: exiting with error value %i\n", res);
    }
    return res;
}
