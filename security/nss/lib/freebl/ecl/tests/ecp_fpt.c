/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecp_fp.h"
#include "mpprime.h"

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

/* Time k repetitions of operation op. */
#define M_TimeOperation(op, k)                                                                                       \
    {                                                                                                                \
        double dStart, dNow, dUserTime;                                                                              \
        struct rusage ru;                                                                                            \
        int i;                                                                                                       \
        getrusage(RUSAGE_SELF, &ru);                                                                                 \
        dStart = (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec * 0.000001;                                \
        for (i = 0; i < k; i++) {                                                                                    \
            {                                                                                                        \
                op;                                                                                                  \
            }                                                                                                        \
        };                                                                                                           \
        getrusage(RUSAGE_SELF, &ru);                                                                                 \
        dNow = (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec * 0.000001;                                  \
        dUserTime = dNow - dStart;                                                                                   \
        if (dUserTime)                                                                                               \
            printf("    %-45s\n      k: %6i, t: %6.2f sec, k/t: %6.2f ops/sec\n", #op, k, dUserTime, k / dUserTime); \
    }

/* Test curve using specific floating point field arithmetic. */
#define M_TestCurve(name_c, name)                                                       \
    {                                                                                   \
        printf("Testing %s using specific floating point implementation...\n", name_c); \
        ECGroup_free(ecgroup);                                                          \
        ecgroup = ECGroup_fromName(name);                                               \
        if (ecgroup == NULL) {                                                          \
            printf("  Warning: could not construct group.\n");                          \
            printf("%s failed.\n", name_c);                                             \
            res = MP_NO;                                                                \
            goto CLEANUP;                                                               \
        } else {                                                                        \
            MP_CHECKOK(testCurve(ecgroup));                                             \
            printf("%s passed.\n", name_c);                                             \
        }                                                                               \
    }

/* Outputs a floating point double (currently not used) */
void
d_output(const double *u, int len, char *name, const EC_group_fp *group)
{
    int i;

    printf("%s:  ", name);
    for (i = 0; i < len; i++) {
        printf("+ %.2f * 2^%i ", u[i] / ecfp_exp[i],
               group->doubleBitSize * i);
    }
    printf("\n");
}

/* Tests a point p in Jacobian coordinates, comparing against the
 * expected affine result (x, y). */
mp_err
testJacPoint(ecfp_jac_pt *p, mp_int *x, mp_int *y, ECGroup *ecgroup)
{
    char s[1000];
    mp_int rx, ry, rz;
    mp_err res = MP_OKAY;

    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&rz) = 0;

    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&rz));

    ecfp_fp2i(&rx, p->x, ecgroup);
    ecfp_fp2i(&ry, p->y, ecgroup);
    ecfp_fp2i(&rz, p->z, ecgroup);

    /* convert result R to affine coordinates */
    ec_GFp_pt_jac2aff(&rx, &ry, &rz, &rx, &ry, ecgroup);

    /* Compare to expected result */
    if ((mp_cmp(&rx, x) != 0) || (mp_cmp(&ry, y) != 0)) {
        printf("  Error: Jacobian Floating Point Incorrect.\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("floating point result\nrx    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry    %s\n", s);
        MP_CHECKOK(mp_toradix(x, s, 16));
        printf("integer result\nx   %s\n", s);
        MP_CHECKOK(mp_toradix(y, s, 16));
        printf("y   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

CLEANUP:
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&rz);

    return res;
}

/* Tests a point p in Chudnovsky Jacobian coordinates, comparing against
 * the expected affine result (x, y). */
mp_err
testChudPoint(ecfp_chud_pt *p, mp_int *x, mp_int *y, ECGroup *ecgroup)
{

    char s[1000];
    mp_int rx, ry, rz, rz2, rz3, test;
    mp_err res = MP_OKAY;

    /* Initialization */
    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&rz) = 0;
    MP_DIGITS(&rz2) = 0;
    MP_DIGITS(&rz3) = 0;
    MP_DIGITS(&test) = 0;

    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&rz));
    MP_CHECKOK(mp_init(&rz2));
    MP_CHECKOK(mp_init(&rz3));
    MP_CHECKOK(mp_init(&test));

    /* Convert to integers */
    ecfp_fp2i(&rx, p->x, ecgroup);
    ecfp_fp2i(&ry, p->y, ecgroup);
    ecfp_fp2i(&rz, p->z, ecgroup);
    ecfp_fp2i(&rz2, p->z2, ecgroup);
    ecfp_fp2i(&rz3, p->z3, ecgroup);

    /* Verify z2, z3 are valid */
    mp_sqrmod(&rz, &ecgroup->meth->irr, &test);
    if (mp_cmp(&test, &rz2) != 0) {
        printf("  Error: rzp2 not valid\n");
        res = MP_NO;
        goto CLEANUP;
    }
    mp_mulmod(&test, &rz, &ecgroup->meth->irr, &test);
    if (mp_cmp(&test, &rz3) != 0) {
        printf("  Error: rzp2 not valid\n");
        res = MP_NO;
        goto CLEANUP;
    }

    /* convert result R to affine coordinates */
    ec_GFp_pt_jac2aff(&rx, &ry, &rz, &rx, &ry, ecgroup);

    /* Compare against expected result */
    if ((mp_cmp(&rx, x) != 0) || (mp_cmp(&ry, y) != 0)) {
        printf("  Error: Chudnovsky Floating Point Incorrect.\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("floating point result\nrx    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry    %s\n", s);
        MP_CHECKOK(mp_toradix(x, s, 16));
        printf("integer result\nx   %s\n", s);
        MP_CHECKOK(mp_toradix(y, s, 16));
        printf("y   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

CLEANUP:
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&rz);
    mp_clear(&rz2);
    mp_clear(&rz3);
    mp_clear(&test);

    return res;
}

/* Tests a point p in Modified Jacobian coordinates, comparing against the
 * expected affine result (x, y). */
mp_err
testJmPoint(ecfp_jm_pt *r, mp_int *x, mp_int *y, ECGroup *ecgroup)
{

    char s[1000];
    mp_int rx, ry, rz, raz4, test;
    mp_err res = MP_OKAY;

    /* Initialization */
    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&rz) = 0;
    MP_DIGITS(&raz4) = 0;
    MP_DIGITS(&test) = 0;

    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&rz));
    MP_CHECKOK(mp_init(&raz4));
    MP_CHECKOK(mp_init(&test));

    /* Convert to integer */
    ecfp_fp2i(&rx, r->x, ecgroup);
    ecfp_fp2i(&ry, r->y, ecgroup);
    ecfp_fp2i(&rz, r->z, ecgroup);
    ecfp_fp2i(&raz4, r->az4, ecgroup);

    /* Verify raz4 = rz^4 * a */
    mp_sqrmod(&rz, &ecgroup->meth->irr, &test);
    mp_sqrmod(&test, &ecgroup->meth->irr, &test);
    mp_mulmod(&test, &ecgroup->curvea, &ecgroup->meth->irr, &test);
    if (mp_cmp(&test, &raz4) != 0) {
        printf("  Error: a*z^4 not valid\n");
        MP_CHECKOK(mp_toradix(&ecgroup->curvea, s, 16));
        printf("a    %s\n", s);
        MP_CHECKOK(mp_toradix(&rz, s, 16));
        printf("rz   %s\n", s);
        MP_CHECKOK(mp_toradix(&raz4, s, 16));
        printf("raz4    %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

    /* convert result R to affine coordinates */
    ec_GFp_pt_jac2aff(&rx, &ry, &rz, &rx, &ry, ecgroup);

    /* Compare against expected result */
    if ((mp_cmp(&rx, x) != 0) || (mp_cmp(&ry, y) != 0)) {
        printf("  Error: Modified Jacobian Floating Point Incorrect.\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("floating point result\nrx    %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry    %s\n", s);
        MP_CHECKOK(mp_toradix(x, s, 16));
        printf("integer result\nx   %s\n", s);
        MP_CHECKOK(mp_toradix(y, s, 16));
        printf("y   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }
CLEANUP:
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&rz);
    mp_clear(&raz4);
    mp_clear(&test);

    return res;
}

/* Tests point addition of Jacobian + Affine -> Jacobian */
mp_err
testPointAddJacAff(ECGroup *ecgroup)
{
    mp_err res;
    mp_int pz, rx2, ry2, rz2;
    ecfp_jac_pt p, r;
    ecfp_aff_pt q;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    /* Init */
    MP_DIGITS(&pz) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&rz2) = 0;
    MP_CHECKOK(mp_init(&pz));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&rz2));

    MP_CHECKOK(mp_set_int(&pz, 5));

    /* Set p */
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(p.z, &pz, ecgroup);
    /* Set q */
    ecfp_i2fp(q.x, &ecgroup->geny, ecgroup);
    ecfp_i2fp(q.y, &ecgroup->genx, ecgroup);

    /* Do calculations */
    group->pt_add_jac_aff(&p, &q, &r, group);

    /* Do calculation in integer to compare against */
    MP_CHECKOK(ec_GFp_pt_add_jac_aff(&ecgroup->genx, &ecgroup->geny, &pz, &ecgroup->geny,
                                     &ecgroup->genx, &rx2, &ry2, &rz2, ecgroup));
    /* convert result R to affine coordinates */
    ec_GFp_pt_jac2aff(&rx2, &ry2, &rz2, &rx2, &ry2, ecgroup);

    MP_CHECKOK(testJacPoint(&r, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Addition - Jacobian & Affine\n");
    else
        printf("TEST FAILED - Point Addition - Jacobian & Affine\n");

    mp_clear(&pz);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&rz2);

    return res;
}

/* Tests point addition in Jacobian coordinates */
mp_err
testPointAddJac(ECGroup *ecgroup)
{
    mp_err res;
    mp_int pz, qz, qx, qy, rx2, ry2, rz2;
    ecfp_jac_pt p, q, r;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    /* Init */
    MP_DIGITS(&pz) = 0;
    MP_DIGITS(&qx) = 0;
    MP_DIGITS(&qy) = 0;
    MP_DIGITS(&qz) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&rz2) = 0;
    MP_CHECKOK(mp_init(&pz));
    MP_CHECKOK(mp_init(&qx));
    MP_CHECKOK(mp_init(&qy));
    MP_CHECKOK(mp_init(&qz));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&rz2));

    MP_CHECKOK(mp_set_int(&pz, 5));
    MP_CHECKOK(mp_set_int(&qz, 105));

    /* Set p */
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(p.z, &pz, ecgroup);
    /* Set q */
    ecfp_i2fp(q.x, &ecgroup->geny, ecgroup);
    ecfp_i2fp(q.y, &ecgroup->genx, ecgroup);
    ecfp_i2fp(q.z, &qz, ecgroup);

    /* Do calculations */
    group->pt_add_jac(&p, &q, &r, group);

    /* Do calculation in integer to compare against */
    ec_GFp_pt_jac2aff(&ecgroup->geny, &ecgroup->genx, &qz, &qx, &qy,
                      ecgroup);
    MP_CHECKOK(ec_GFp_pt_add_jac_aff(&ecgroup->genx, &ecgroup->geny, &pz, &qx, &qy, &rx2, &ry2,
                                     &rz2, ecgroup));
    /* convert result R to affine coordinates */
    ec_GFp_pt_jac2aff(&rx2, &ry2, &rz2, &rx2, &ry2, ecgroup);

    MP_CHECKOK(testJacPoint(&r, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Addition - Jacobian\n");
    else
        printf("TEST FAILED - Point Addition - Jacobian\n");

    mp_clear(&pz);
    mp_clear(&qx);
    mp_clear(&qy);
    mp_clear(&qz);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&rz2);

    return res;
}

/* Tests point addition in Chudnovsky Jacobian Coordinates */
mp_err
testPointAddChud(ECGroup *ecgroup)
{
    mp_err res;
    mp_int rx2, ry2, ix, iy, iz, test, pz, qx, qy, qz;
    ecfp_chud_pt p, q, r;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    MP_DIGITS(&qx) = 0;
    MP_DIGITS(&qy) = 0;
    MP_DIGITS(&qz) = 0;
    MP_DIGITS(&pz) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&ix) = 0;
    MP_DIGITS(&iy) = 0;
    MP_DIGITS(&iz) = 0;
    MP_DIGITS(&test) = 0;

    MP_CHECKOK(mp_init(&qx));
    MP_CHECKOK(mp_init(&qy));
    MP_CHECKOK(mp_init(&qz));
    MP_CHECKOK(mp_init(&pz));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&ix));
    MP_CHECKOK(mp_init(&iy));
    MP_CHECKOK(mp_init(&iz));
    MP_CHECKOK(mp_init(&test));

    /* Test Chudnovsky form addition */
    /* Set p */
    MP_CHECKOK(mp_set_int(&pz, 5));
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(p.z, &pz, ecgroup);
    mp_sqrmod(&pz, &ecgroup->meth->irr, &test);
    ecfp_i2fp(p.z2, &test, ecgroup);
    mp_mulmod(&test, &pz, &ecgroup->meth->irr, &test);
    ecfp_i2fp(p.z3, &test, ecgroup);

    /* Set q */
    MP_CHECKOK(mp_set_int(&qz, 105));
    ecfp_i2fp(q.x, &ecgroup->geny, ecgroup);
    ecfp_i2fp(q.y, &ecgroup->genx, ecgroup);
    ecfp_i2fp(q.z, &qz, ecgroup);
    mp_sqrmod(&qz, &ecgroup->meth->irr, &test);
    ecfp_i2fp(q.z2, &test, ecgroup);
    mp_mulmod(&test, &qz, &ecgroup->meth->irr, &test);
    ecfp_i2fp(q.z3, &test, ecgroup);

    group->pt_add_chud(&p, &q, &r, group);

    /* Calculate addition to compare against */
    ec_GFp_pt_jac2aff(&ecgroup->geny, &ecgroup->genx, &qz, &qx, &qy,
                      ecgroup);
    ec_GFp_pt_add_jac_aff(&ecgroup->genx, &ecgroup->geny, &pz, &qx, &qy,
                          &ix, &iy, &iz, ecgroup);
    ec_GFp_pt_jac2aff(&ix, &iy, &iz, &rx2, &ry2, ecgroup);

    MP_CHECKOK(testChudPoint(&r, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Addition - Chudnovsky Jacobian\n");
    else
        printf("TEST FAILED - Point Addition - Chudnovsky Jacobian\n");

    mp_clear(&qx);
    mp_clear(&qy);
    mp_clear(&qz);
    mp_clear(&pz);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&ix);
    mp_clear(&iy);
    mp_clear(&iz);
    mp_clear(&test);

    return res;
}

/* Tests point addition in Modified Jacobian + Chudnovsky Jacobian ->
 * Modified Jacobian coordinates. */
mp_err
testPointAddJmChud(ECGroup *ecgroup)
{
    mp_err res;
    mp_int rx2, ry2, ix, iy, iz, test, pz, paz4, qx, qy, qz;
    ecfp_chud_pt q;
    ecfp_jm_pt p, r;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    MP_DIGITS(&qx) = 0;
    MP_DIGITS(&qy) = 0;
    MP_DIGITS(&qz) = 0;
    MP_DIGITS(&pz) = 0;
    MP_DIGITS(&paz4) = 0;
    MP_DIGITS(&iz) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&ix) = 0;
    MP_DIGITS(&iy) = 0;
    MP_DIGITS(&iz) = 0;
    MP_DIGITS(&test) = 0;

    MP_CHECKOK(mp_init(&qx));
    MP_CHECKOK(mp_init(&qy));
    MP_CHECKOK(mp_init(&qz));
    MP_CHECKOK(mp_init(&pz));
    MP_CHECKOK(mp_init(&paz4));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&ix));
    MP_CHECKOK(mp_init(&iy));
    MP_CHECKOK(mp_init(&iz));
    MP_CHECKOK(mp_init(&test));

    /* Test Modified Jacobian form addition */
    /* Set p */
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(group->curvea, &ecgroup->curvea, ecgroup);
    /* paz4 = az^4 */
    MP_CHECKOK(mp_set_int(&pz, 5));
    mp_sqrmod(&pz, &ecgroup->meth->irr, &paz4);
    mp_sqrmod(&paz4, &ecgroup->meth->irr, &paz4);
    mp_mulmod(&paz4, &ecgroup->curvea, &ecgroup->meth->irr, &paz4);
    ecfp_i2fp(p.z, &pz, ecgroup);
    ecfp_i2fp(p.az4, &paz4, ecgroup);

    /* Set q */
    MP_CHECKOK(mp_set_int(&qz, 105));
    ecfp_i2fp(q.x, &ecgroup->geny, ecgroup);
    ecfp_i2fp(q.y, &ecgroup->genx, ecgroup);
    ecfp_i2fp(q.z, &qz, ecgroup);
    mp_sqrmod(&qz, &ecgroup->meth->irr, &test);
    ecfp_i2fp(q.z2, &test, ecgroup);
    mp_mulmod(&test, &qz, &ecgroup->meth->irr, &test);
    ecfp_i2fp(q.z3, &test, ecgroup);

    /* Do calculation */
    group->pt_add_jm_chud(&p, &q, &r, group);

    /* Calculate addition to compare against */
    ec_GFp_pt_jac2aff(&ecgroup->geny, &ecgroup->genx, &qz, &qx, &qy,
                      ecgroup);
    ec_GFp_pt_add_jac_aff(&ecgroup->genx, &ecgroup->geny, &pz, &qx, &qy,
                          &ix, &iy, &iz, ecgroup);
    ec_GFp_pt_jac2aff(&ix, &iy, &iz, &rx2, &ry2, ecgroup);

    MP_CHECKOK(testJmPoint(&r, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Addition - Modified & Chudnovsky Jacobian\n");
    else
        printf("TEST FAILED - Point Addition - Modified & Chudnovsky Jacobian\n");

    mp_clear(&qx);
    mp_clear(&qy);
    mp_clear(&qz);
    mp_clear(&pz);
    mp_clear(&paz4);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&ix);
    mp_clear(&iy);
    mp_clear(&iz);
    mp_clear(&test);

    return res;
}

/* Tests point doubling in Modified Jacobian coordinates */
mp_err
testPointDoubleJm(ECGroup *ecgroup)
{
    mp_err res;
    mp_int pz, paz4, rx2, ry2, rz2, raz4;
    ecfp_jm_pt p, r;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    MP_DIGITS(&pz) = 0;
    MP_DIGITS(&paz4) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&rz2) = 0;
    MP_DIGITS(&raz4) = 0;

    MP_CHECKOK(mp_init(&pz));
    MP_CHECKOK(mp_init(&paz4));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&rz2));
    MP_CHECKOK(mp_init(&raz4));

    /* Set p */
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(group->curvea, &ecgroup->curvea, ecgroup);

    /* paz4 = az^4 */
    MP_CHECKOK(mp_set_int(&pz, 5));
    mp_sqrmod(&pz, &ecgroup->meth->irr, &paz4);
    mp_sqrmod(&paz4, &ecgroup->meth->irr, &paz4);
    mp_mulmod(&paz4, &ecgroup->curvea, &ecgroup->meth->irr, &paz4);

    ecfp_i2fp(p.z, &pz, ecgroup);
    ecfp_i2fp(p.az4, &paz4, ecgroup);

    group->pt_dbl_jm(&p, &r, group);

    M_TimeOperation(group->pt_dbl_jm(&p, &r, group), 100000);

    /* Calculate doubling to compare against */
    ec_GFp_pt_dbl_jac(&ecgroup->genx, &ecgroup->geny, &pz, &rx2, &ry2,
                      &rz2, ecgroup);
    ec_GFp_pt_jac2aff(&rx2, &ry2, &rz2, &rx2, &ry2, ecgroup);

    /* Do comparison and check az^4 */
    MP_CHECKOK(testJmPoint(&r, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Doubling - Modified Jacobian\n");
    else
        printf("TEST FAILED - Point Doubling - Modified Jacobian\n");
    mp_clear(&pz);
    mp_clear(&paz4);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&rz2);
    mp_clear(&raz4);

    return res;
}

/* Tests point doubling in Chudnovsky Jacobian coordinates */
mp_err
testPointDoubleChud(ECGroup *ecgroup)
{
    mp_err res;
    mp_int px, py, pz, rx2, ry2, rz2;
    ecfp_aff_pt p;
    ecfp_chud_pt p2;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&rz2) = 0;
    MP_DIGITS(&px) = 0;
    MP_DIGITS(&py) = 0;
    MP_DIGITS(&pz) = 0;

    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&rz2));
    MP_CHECKOK(mp_init(&px));
    MP_CHECKOK(mp_init(&py));
    MP_CHECKOK(mp_init(&pz));

    /* Set p2 = 2P */
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(group->curvea, &ecgroup->curvea, ecgroup);

    group->pt_dbl_aff2chud(&p, &p2, group);

    /* Calculate doubling to compare against */
    MP_CHECKOK(mp_set_int(&pz, 1));
    ec_GFp_pt_dbl_jac(&ecgroup->genx, &ecgroup->geny, &pz, &rx2, &ry2,
                      &rz2, ecgroup);
    ec_GFp_pt_jac2aff(&rx2, &ry2, &rz2, &rx2, &ry2, ecgroup);

    /* Do comparison and check az^4 */
    MP_CHECKOK(testChudPoint(&p2, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Doubling - Chudnovsky Jacobian\n");
    else
        printf("TEST FAILED - Point Doubling - Chudnovsky Jacobian\n");

    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&rz2);
    mp_clear(&px);
    mp_clear(&py);
    mp_clear(&pz);

    return res;
}

/* Test point doubling in Jacobian coordinates */
mp_err
testPointDoubleJac(ECGroup *ecgroup)
{
    mp_err res;
    mp_int pz, rx, ry, rz, rx2, ry2, rz2;
    ecfp_jac_pt p, p2;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    MP_DIGITS(&pz) = 0;
    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&rz) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&rz2) = 0;

    MP_CHECKOK(mp_init(&pz));
    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&rz));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&rz2));

    MP_CHECKOK(mp_set_int(&pz, 5));

    /* Set p2 = 2P */
    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(p.z, &pz, ecgroup);
    ecfp_i2fp(group->curvea, &ecgroup->curvea, ecgroup);

    group->pt_dbl_jac(&p, &p2, group);
    M_TimeOperation(group->pt_dbl_jac(&p, &p2, group), 100000);

    /* Calculate doubling to compare against */
    ec_GFp_pt_dbl_jac(&ecgroup->genx, &ecgroup->geny, &pz, &rx2, &ry2,
                      &rz2, ecgroup);
    ec_GFp_pt_jac2aff(&rx2, &ry2, &rz2, &rx2, &ry2, ecgroup);

    /* Do comparison */
    MP_CHECKOK(testJacPoint(&p2, &rx2, &ry2, ecgroup));

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Doubling - Jacobian\n");
    else
        printf("TEST FAILED - Point Doubling - Jacobian\n");

    mp_clear(&pz);
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&rz);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&rz2);

    return res;
}

/* Tests a point multiplication (various algorithms) */
mp_err
testPointMul(ECGroup *ecgroup)
{
    mp_err res;
    char s[1000];
    mp_int rx, ry, order_1;

    /* Init */
    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&order_1) = 0;

    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&order_1));

    MP_CHECKOK(mp_set_int(&order_1, 1));
    MP_CHECKOK(mp_sub(&ecgroup->order, &order_1, &order_1));

    /* Test Algorithm 1: Jacobian-Affine Double & Add */
    ec_GFp_pt_mul_jac_fp(&order_1, &ecgroup->genx, &ecgroup->geny, &rx,
                         &ry, ecgroup);
    MP_CHECKOK(ecgroup->meth->field_neg(&ry, &ry, ecgroup->meth));
    if ((mp_cmp(&rx, &ecgroup->genx) != 0) || (mp_cmp(&ry, &ecgroup->geny) != 0)) {
        printf("  Error: ec_GFp_pt_mul_jac_fp invalid result (expected (- base point)).\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("rx   %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

    ec_GFp_pt_mul_jac_fp(&ecgroup->order, &ecgroup->genx, &ecgroup->geny,
                         &rx, &ry, ecgroup);
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: ec_GFp_pt_mul_jac_fp invalid result (expected point at infinity.\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("rx   %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

    /* Test Algorithm 2: 4-bit Window in Jacobian */
    ec_GFp_point_mul_jac_4w_fp(&order_1, &ecgroup->genx, &ecgroup->geny,
                               &rx, &ry, ecgroup);
    MP_CHECKOK(ecgroup->meth->field_neg(&ry, &ry, ecgroup->meth));
    if ((mp_cmp(&rx, &ecgroup->genx) != 0) || (mp_cmp(&ry, &ecgroup->geny) != 0)) {
        printf("  Error: ec_GFp_point_mul_jac_4w_fp invalid result (expected (- base point)).\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("rx   %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

    ec_GFp_point_mul_jac_4w_fp(&ecgroup->order, &ecgroup->genx,
                               &ecgroup->geny, &rx, &ry, ecgroup);
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: ec_GFp_point_mul_jac_4w_fp invalid result (expected point at infinity.\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("rx   %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

    /* Test Algorithm 3: wNAF with modified Jacobian coordinates */
    ec_GFp_point_mul_wNAF_fp(&order_1, &ecgroup->genx, &ecgroup->geny, &rx,
                             &ry, ecgroup);
    MP_CHECKOK(ecgroup->meth->field_neg(&ry, &ry, ecgroup->meth));
    if ((mp_cmp(&rx, &ecgroup->genx) != 0) || (mp_cmp(&ry, &ecgroup->geny) != 0)) {
        printf("  Error: ec_GFp_pt_mul_wNAF_fp invalid result (expected (- base point)).\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("rx   %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

    ec_GFp_point_mul_wNAF_fp(&ecgroup->order, &ecgroup->genx,
                             &ecgroup->geny, &rx, &ry, ecgroup);
    if (ec_GFp_pt_is_inf_aff(&rx, &ry) != MP_YES) {
        printf("  Error: ec_GFp_pt_mul_wNAF_fp invalid result (expected point at infinity.\n");
        MP_CHECKOK(mp_toradix(&rx, s, 16));
        printf("rx   %s\n", s);
        MP_CHECKOK(mp_toradix(&ry, s, 16));
        printf("ry   %s\n", s);
        res = MP_NO;
        goto CLEANUP;
    }

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Multiplication\n");
    else
        printf("TEST FAILED - Point Multiplication\n");
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&order_1);

    return res;
}

/* Tests point multiplication with a random scalar repeatedly, comparing
 * for consistency within different algorithms. */
mp_err
testPointMulRandom(ECGroup *ecgroup)
{
    mp_err res;
    mp_int rx, ry, rx2, ry2, n;
    int i, size;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;

    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&rx2) = 0;
    MP_DIGITS(&ry2) = 0;
    MP_DIGITS(&n) = 0;

    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&rx2));
    MP_CHECKOK(mp_init(&ry2));
    MP_CHECKOK(mp_init(&n));

    for (i = 0; i < 100; i++) {
        /* compute random scalar */
        size = mpl_significant_bits(&ecgroup->meth->irr);
        if (size < MP_OKAY) {
            res = MP_NO;
            goto CLEANUP;
        }
        MP_CHECKOK(mpp_random_size(&n, group->orderBitSize));
        MP_CHECKOK(mp_mod(&n, &ecgroup->order, &n));

        ec_GFp_pt_mul_jac(&n, &ecgroup->genx, &ecgroup->geny, &rx, &ry,
                          ecgroup);
        ec_GFp_pt_mul_jac_fp(&n, &ecgroup->genx, &ecgroup->geny, &rx2,
                             &ry2, ecgroup);

        if ((mp_cmp(&rx, &rx2) != 0) || (mp_cmp(&ry, &ry2) != 0)) {
            printf("  Error: different results for Point Multiplication - Double & Add.\n");
            res = MP_NO;
            goto CLEANUP;
        }

        ec_GFp_point_mul_wNAF_fp(&n, &ecgroup->genx, &ecgroup->geny, &rx,
                                 &ry, ecgroup);
        if ((mp_cmp(&rx, &rx2) != 0) || (mp_cmp(&ry, &ry2) != 0)) {
            printf("  Error: different results for Point Multiplication - wNAF.\n");
            res = MP_NO;
            goto CLEANUP;
        }

        ec_GFp_point_mul_jac_4w_fp(&n, &ecgroup->genx, &ecgroup->geny, &rx,
                                   &ry, ecgroup);
        if ((mp_cmp(&rx, &rx2) != 0) || (mp_cmp(&ry, &ry2) != 0)) {
            printf("  Error: different results for Point Multiplication - 4 bit window.\n");
            res = MP_NO;
            goto CLEANUP;
        }
    }

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Random Multiplication\n");
    else
        printf("TEST FAILED - Point Random Multiplication\n");
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&rx2);
    mp_clear(&ry2);
    mp_clear(&n);

    return res;
}

/* Tests the time required for a point multiplication */
mp_err
testPointMulTime(ECGroup *ecgroup)
{
    mp_err res = MP_OKAY;
    mp_int rx, ry, n;
    int size;

    MP_DIGITS(&rx) = 0;
    MP_DIGITS(&ry) = 0;
    MP_DIGITS(&n) = 0;

    MP_CHECKOK(mp_init(&rx));
    MP_CHECKOK(mp_init(&ry));
    MP_CHECKOK(mp_init(&n));

    /* compute random scalar */
    size = mpl_significant_bits(&ecgroup->meth->irr);
    if (size < MP_OKAY) {
        res = MP_NO;
        goto CLEANUP;
    }

    MP_CHECKOK(mpp_random_size(&n, (size + ECL_BITS - 1) / ECL_BITS));
    MP_CHECKOK(ecgroup->meth->field_mod(&n, &n, ecgroup->meth));

    M_TimeOperation(ec_GFp_pt_mul_jac_fp(&n, &ecgroup->genx, &ecgroup->geny, &rx, &ry,
                                         ecgroup),
                    1000);

    M_TimeOperation(ec_GFp_point_mul_jac_4w_fp(&n, &ecgroup->genx, &ecgroup->geny, &rx, &ry,
                                               ecgroup),
                    1000);

    M_TimeOperation(ec_GFp_point_mul_wNAF_fp(&n, &ecgroup->genx, &ecgroup->geny, &rx, &ry,
                                             ecgroup),
                    1000);

    M_TimeOperation(ec_GFp_pt_mul_jac(&n, &ecgroup->genx, &ecgroup->geny, &rx, &ry,
                                      ecgroup),
                    100);

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Point Multiplication Timing\n");
    else
        printf("TEST FAILED - Point Multiplication Timing\n");
    mp_clear(&rx);
    mp_clear(&ry);
    mp_clear(&n);

    return res;
}

/* Tests pre computation of Chudnovsky Jacobian points used in wNAF form */
mp_err
testPreCompute(ECGroup *ecgroup)
{
    ecfp_chud_pt precomp[16];
    ecfp_aff_pt p;
    EC_group_fp *group = (EC_group_fp *)ecgroup->extra1;
    int i;
    mp_err res;

    mp_int x, y, ny, x2, y2;

    MP_DIGITS(&x) = 0;
    MP_DIGITS(&y) = 0;
    MP_DIGITS(&ny) = 0;
    MP_DIGITS(&x2) = 0;
    MP_DIGITS(&y2) = 0;

    MP_CHECKOK(mp_init(&x));
    MP_CHECKOK(mp_init(&y));
    MP_CHECKOK(mp_init(&ny));
    MP_CHECKOK(mp_init(&x2));
    MP_CHECKOK(mp_init(&y2));

    ecfp_i2fp(p.x, &ecgroup->genx, ecgroup);
    ecfp_i2fp(p.y, &ecgroup->geny, ecgroup);
    ecfp_i2fp(group->curvea, &(ecgroup->curvea), ecgroup);

    /* Perform precomputation */
    group->precompute_chud(precomp, &p, group);

    M_TimeOperation(group->precompute_chud(precomp, &p, group), 10000);

    /* Calculate addition to compare against */
    MP_CHECKOK(mp_copy(&ecgroup->genx, &x));
    MP_CHECKOK(mp_copy(&ecgroup->geny, &y));
    MP_CHECKOK(ecgroup->meth->field_neg(&y, &ny, ecgroup->meth));

    ec_GFp_pt_dbl_aff(&x, &y, &x2, &y2, ecgroup);

    for (i = 0; i < 8; i++) {
        MP_CHECKOK(testChudPoint(&precomp[8 + i], &x, &y, ecgroup));
        MP_CHECKOK(testChudPoint(&precomp[7 - i], &x, &ny, ecgroup));
        ec_GFp_pt_add_aff(&x, &y, &x2, &y2, &x, &y, ecgroup);
        MP_CHECKOK(ecgroup->meth->field_neg(&y, &ny, ecgroup->meth));
    }

CLEANUP:
    if (res == MP_OKAY)
        printf("  Test Passed - Precomputation\n");
    else
        printf("TEST FAILED - Precomputation\n");

    mp_clear(&x);
    mp_clear(&y);
    mp_clear(&ny);
    mp_clear(&x2);
    mp_clear(&y2);
    return res;
}

/* Given a curve using floating point arithmetic, test it. This method
 * specifies which of the above tests to run. */
mp_err
testCurve(ECGroup *ecgroup)
{
    int res = MP_OKAY;

    MP_CHECKOK(testPointAddJacAff(ecgroup));
    MP_CHECKOK(testPointAddJac(ecgroup));
    MP_CHECKOK(testPointAddChud(ecgroup));
    MP_CHECKOK(testPointAddJmChud(ecgroup));
    MP_CHECKOK(testPointDoubleJac(ecgroup));
    MP_CHECKOK(testPointDoubleChud(ecgroup));
    MP_CHECKOK(testPointDoubleJm(ecgroup));
    MP_CHECKOK(testPreCompute(ecgroup));
    MP_CHECKOK(testPointMul(ecgroup));
    MP_CHECKOK(testPointMulRandom(ecgroup));
    MP_CHECKOK(testPointMulTime(ecgroup));
CLEANUP:
    return res;
}

/* Tests a number of curves optimized using floating point arithmetic */
int
main(void)
{
    mp_err res = MP_OKAY;
    ECGroup *ecgroup = NULL;

    /* specific arithmetic tests */
    M_TestCurve("SECG-160R1", ECCurve_SECG_PRIME_160R1);
    M_TestCurve("SECG-192R1", ECCurve_SECG_PRIME_192R1);
    M_TestCurve("SEGC-224R1", ECCurve_SECG_PRIME_224R1);

CLEANUP:
    ECGroup_free(ecgroup);
    if (res != MP_OKAY) {
        printf("Error: exiting with error value %i\n", res);
    }
    return res;
}
