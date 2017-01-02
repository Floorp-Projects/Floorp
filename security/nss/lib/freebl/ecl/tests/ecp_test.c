/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mpi.h"
#include "mplogic.h"
#include "mpprime.h"
#include "ecl.h"
#include "ecl-curve.h"
#include "ecp.h"
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
#define ECTEST_GENERIC_GFP(name_c, name)                             \
    printf("Testing %s using generic implementation...\n", name_c);  \
    params = EC_GetNamedCurveParams(name);                           \
    if (params == NULL) {                                            \
        printf("  Error: could not construct params.\n");            \
        res = MP_NO;                                                 \
        goto CLEANUP;                                                \
    }                                                                \
    ECGroup_free(group);                                             \
    group = ECGroup_fromHex(params);                                 \
    if (group == NULL) {                                             \
        printf("  Error: could not construct group.\n");             \
        res = MP_NO;                                                 \
        goto CLEANUP;                                                \
    }                                                                \
    MP_CHECKOK(ectest_curve_GFp(group, ectestPrint, ectestTime, 1)); \
    printf("... okay.\n");

/* Test curve using specific field arithmetic. */
#define ECTEST_NAMED_GFP(name_c, name)                                   \
    printf("Testing %s using specific implementation...\n", name_c);     \
    ECGroup_free(group);                                                 \
    group = ECGroup_fromName(name);                                      \
    if (group == NULL) {                                                 \
        printf("  Warning: could not construct group.\n");               \
        printf("... failed; continuing with remaining tests.\n");        \
    } else {                                                             \
        MP_CHECKOK(ectest_curve_GFp(group, ectestPrint, ectestTime, 0)); \
        printf("... okay.\n");                                           \
    }

/* Performs basic tests of elliptic curve cryptography over prime fields.
 * If tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
int
ectest_curve_GFp(ECGroup *group, int ectestPrint, int ectestTime,
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

#ifdef ECL_ENABLE_GFP_PT_MUL_AFF
    /* multiply base point by order - 1 and check for negative of base
     * point */
    MP_CHECKOK(ec_GFp_pt_mul_aff(&order_1, &group->genx, &group->geny, &rx, &ry, group));
    if (ectestPrint) {
        printf("  (order-1)*P (affine):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(group->meth->field_neg(&ry, &ry, group->meth));
    if ((mp_cmp(&rx, &group->genx) != 0) || (mp_cmp(&ry, &group->geny) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }
#endif

#ifdef ECL_ENABLE_GFP_PT_MUL_AFF
    /* multiply base point by order - 1 and check for negative of base
     * point */
    MP_CHECKOK(ec_GFp_pt_mul_jac(&order_1, &group->genx, &group->geny, &rx, &ry, group));
    if (ectestPrint) {
        printf("  (order-1)*P (jacobian):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    MP_CHECKOK(group->meth->field_neg(&ry, &ry, group->meth));
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
    MP_CHECKOK(mp_submod(&group->meth->irr, &ry, &group->meth->irr, &ry));
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
    MP_CHECKOK(mp_submod(&group->meth->irr, &ry, &group->meth->irr, &ry));
    if ((mp_cmp(&rx, &gx) != 0) || (mp_cmp(&ry, &gy) != 0)) {
        printf("  Error: invalid result (expected (- base point)).\n");
        res = MP_NO;
        goto CLEANUP;
    }

#ifdef ECL_ENABLE_GFP_PT_MUL_AFF
    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ec_GFp_pt_mul_aff(&group->order, &group->genx, &group->geny, &rx, &ry,
                                 group));
    if (ectestPrint) {
        printf("  (order)*P (affine):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: invalid result (expected point at infinity).\n");
        res = MP_NO;
        goto CLEANUP;
    }
#endif

#ifdef ECL_ENABLE_GFP_PT_MUL_JAC
    /* multiply base point by order and check for point at infinity */
    MP_CHECKOK(ec_GFp_pt_mul_jac(&group->order, &group->genx, &group->geny, &rx, &ry,
                                 group));
    if (ectestPrint) {
        printf("  (order)*P (jacobian):\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("    %s\n", s);
    }
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
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
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
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
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
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
    MP_CHECKOK(mp_submod(&group->meth->irr, &ry, &group->meth->irr, &ry));
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
#ifdef ECL_ENABLE_GFP_PT_MUL_AFF
            M_TimeOperation(MP_CHECKOK(ec_GFp_pt_mul_aff(&n, &group->genx, &group->geny, &rx, &ry,
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
    printf("    --print     Print out results of each point arithmetic test.\n");
    printf("    --time      Benchmark point operations and print results.\n");
}

/* Performs tests of elliptic curve cryptography over prime fields If
 * tests fail, then it prints an error message, aborts, and returns an
 * error code. Otherwise, returns 0. */
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
    ECTEST_GENERIC_GFP("SECP-160R1", ECCurve_SECG_PRIME_160R1);

    /* specific arithmetic tests */
    ECTEST_NAMED_GFP("NIST-P192", ECCurve_NIST_P192);
    ECTEST_NAMED_GFP("NIST-P224", ECCurve_NIST_P224);
    ECTEST_NAMED_GFP("NIST-P256", ECCurve_NIST_P256);
    ECTEST_NAMED_GFP("NIST-P384", ECCurve_NIST_P384);
    ECTEST_NAMED_GFP("NIST-P521", ECCurve_NIST_P521);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME192v1", ECCurve_X9_62_PRIME_192V1);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME192v2", ECCurve_X9_62_PRIME_192V2);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME192v3", ECCurve_X9_62_PRIME_192V3);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME239v1", ECCurve_X9_62_PRIME_239V1);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME239v2", ECCurve_X9_62_PRIME_239V2);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME239v3", ECCurve_X9_62_PRIME_239V3);
    ECTEST_NAMED_GFP("ANSI X9.62 PRIME256v1", ECCurve_X9_62_PRIME_256V1);
    ECTEST_NAMED_GFP("SECP-112R1", ECCurve_SECG_PRIME_112R1);
    ECTEST_NAMED_GFP("SECP-112R2", ECCurve_SECG_PRIME_112R2);
    ECTEST_NAMED_GFP("SECP-128R1", ECCurve_SECG_PRIME_128R1);
    ECTEST_NAMED_GFP("SECP-128R2", ECCurve_SECG_PRIME_128R2);
    ECTEST_NAMED_GFP("SECP-160K1", ECCurve_SECG_PRIME_160K1);
    ECTEST_NAMED_GFP("SECP-160R1", ECCurve_SECG_PRIME_160R1);
    ECTEST_NAMED_GFP("SECP-160R2", ECCurve_SECG_PRIME_160R2);
    ECTEST_NAMED_GFP("SECP-192K1", ECCurve_SECG_PRIME_192K1);
    ECTEST_NAMED_GFP("SECP-192R1", ECCurve_SECG_PRIME_192R1);
    ECTEST_NAMED_GFP("SECP-224K1", ECCurve_SECG_PRIME_224K1);
    ECTEST_NAMED_GFP("SECP-224R1", ECCurve_SECG_PRIME_224R1);
    ECTEST_NAMED_GFP("SECP-256K1", ECCurve_SECG_PRIME_256K1);
    ECTEST_NAMED_GFP("SECP-256R1", ECCurve_SECG_PRIME_256R1);
    ECTEST_NAMED_GFP("SECP-384R1", ECCurve_SECG_PRIME_384R1);
    ECTEST_NAMED_GFP("SECP-521R1", ECCurve_SECG_PRIME_521R1);
    ECTEST_NAMED_GFP("WTLS-6 (112)", ECCurve_WTLS_6);
    ECTEST_NAMED_GFP("WTLS-7 (160)", ECCurve_WTLS_7);
    ECTEST_NAMED_GFP("WTLS-8 (112)", ECCurve_WTLS_8);
    ECTEST_NAMED_GFP("WTLS-9 (160)", ECCurve_WTLS_9);
    ECTEST_NAMED_GFP("WTLS-12 (224)", ECCurve_WTLS_12);

CLEANUP:
    EC_FreeCurveParams(params);
    ECGroup_free(group);
    if (res != MP_OKAY) {
        printf("Error: exiting with error value %i\n", res);
    }
    return res;
}
