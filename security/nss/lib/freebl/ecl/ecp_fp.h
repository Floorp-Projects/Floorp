/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ecp_fp_h_
#define __ecp_fp_h_

#include "mpi.h"
#include "ecl.h"
#include "ecp.h"

#include <sys/types.h>
#include "mpi-priv.h"

#ifdef ECL_DEBUG
#include <assert.h>
#endif

/* Largest number of doubles to store one reduced number in floating
 * point. Used for memory allocation on the stack. */
#define ECFP_MAXDOUBLES 10

/* For debugging purposes */
#ifndef ECL_DEBUG
#define ECFP_ASSERT(x)
#else
#define ECFP_ASSERT(x) assert(x)
#endif

/* ECFP_Ti = 2^(i*24) Define as preprocessor constants so we can use in
 * multiple static constants */
#define ECFP_T0 1.0
#define ECFP_T1 16777216.0
#define ECFP_T2 281474976710656.0
#define ECFP_T3 4722366482869645213696.0
#define ECFP_T4 79228162514264337593543950336.0
#define ECFP_T5 1329227995784915872903807060280344576.0
#define ECFP_T6 22300745198530623141535718272648361505980416.0
#define ECFP_T7 374144419156711147060143317175368453031918731001856.0
#define ECFP_T8 6277101735386680763835789423207666416102355444464034512896.0
#define ECFP_T9 105312291668557186697918027683670432318895095400549111254310977536.0
#define ECFP_T10 1766847064778384329583297500742918515827483896875618958121606201292619776.0
#define ECFP_T11 29642774844752946028434172162224104410437116074403984394101141506025761187823616.0
#define ECFP_T12 497323236409786642155382248146820840100456150797347717440463976893159497012533375533056.0
#define ECFP_T13 8343699359066055009355553539724812947666814540455674882605631280555545803830627148527195652096.0
#define ECFP_T14 139984046386112763159840142535527767382602843577165595931249318810236991948760059086304843329475444736.0
#define ECFP_T15 2348542582773833227889480596789337027375682548908319870707290971532209025114608443463698998384768703031934976.0
#define ECFP_T16 39402006196394479212279040100143613805079739270465446667948293404245\
721771497210611414266254884915640806627990306816.0
#define ECFP_T17 66105596879024859895191530803277103982840468296428121928464879527440\
5791236311345825189210439715284847591212025023358304256.0
#define ECFP_T18 11090678776483259438313656736572334813745748301503266300681918322458\
485231222502492159897624416558312389564843845614287315896631296.0
#define ECFP_T19 18607071341967536398062689481932916079453218833595342343206149099024\
36577570298683715049089827234727835552055312041415509848580169253519\
36.0

#define ECFP_TWO160 1461501637330902918203684832716283019655932542976.0
#define ECFP_TWO192 6277101735386680763835789423207666416102355444464034512896.0
#define ECFP_TWO224 26959946667150639794667015087019630673637144422540572481103610249216.0

/* Multiplicative constants */
static const double ecfp_two32 = 4294967296.0;
static const double ecfp_two64 = 18446744073709551616.0;
static const double ecfp_twom16 = .0000152587890625;
static const double ecfp_twom128 =
    .00000000000000000000000000000000000000293873587705571876992184134305561419454666389193021880377187926569604314863681793212890625;
static const double ecfp_twom129 =
    .000000000000000000000000000000000000001469367938527859384960920671527807097273331945965109401885939632848021574318408966064453125;
static const double ecfp_twom160 =
    .0000000000000000000000000000000000000000000000006842277657836020854119773355907793609766904013068924666782559979930620520927053718196475529111921787261962890625;
static const double ecfp_twom192 =
    .000000000000000000000000000000000000000000000000000000000159309191113245227702888039776771180559110455519261878607388585338616290151305816094308987472018268594098344692611135542392730712890625;
static const double ecfp_twom224 =
    .00000000000000000000000000000000000000000000000000000000000000000003709206150687421385731735261547639513367564778757791002453039058917581340095629358997312082723208437536338919136001159027049567384892725385725498199462890625;

/* ecfp_exp[i] = 2^(i*ECFP_DSIZE) */
static const double ecfp_exp[2 * ECFP_MAXDOUBLES] = {
    ECFP_T0, ECFP_T1, ECFP_T2, ECFP_T3, ECFP_T4, ECFP_T5,
    ECFP_T6, ECFP_T7, ECFP_T8, ECFP_T9, ECFP_T10, ECFP_T11,
    ECFP_T12, ECFP_T13, ECFP_T14, ECFP_T15, ECFP_T16, ECFP_T17, ECFP_T18,
    ECFP_T19
};

/* 1.1 * 2^52 Uses 2^52 to truncate, the .1 is an extra 2^51 to protect
 * the 2^52 bit, so that adding alphas to a negative number won't borrow
 * and empty the important 2^52 bit */
#define ECFP_ALPHABASE_53 6755399441055744.0
/* Special case: On some platforms, notably x86 Linux, there is an
 * extended-precision floating point representation with 64-bits of
 * precision in the mantissa.  These extra bits of precision require a
 * larger value of alpha to truncate, i.e. 1.1 * 2^63. */
#define ECFP_ALPHABASE_64 13835058055282163712.0

/*
 * ecfp_alpha[i] = 1.5 * 2^(52 + i*ECFP_DSIZE) we add and subtract alpha
 * to truncate floating point numbers to a certain number of bits for
 * tidying */
static const double ecfp_alpha_53[2 * ECFP_MAXDOUBLES] = {
    ECFP_ALPHABASE_53 * ECFP_T0,
    ECFP_ALPHABASE_53 *ECFP_T1,
    ECFP_ALPHABASE_53 *ECFP_T2,
    ECFP_ALPHABASE_53 *ECFP_T3,
    ECFP_ALPHABASE_53 *ECFP_T4,
    ECFP_ALPHABASE_53 *ECFP_T5,
    ECFP_ALPHABASE_53 *ECFP_T6,
    ECFP_ALPHABASE_53 *ECFP_T7,
    ECFP_ALPHABASE_53 *ECFP_T8,
    ECFP_ALPHABASE_53 *ECFP_T9,
    ECFP_ALPHABASE_53 *ECFP_T10,
    ECFP_ALPHABASE_53 *ECFP_T11,
    ECFP_ALPHABASE_53 *ECFP_T12,
    ECFP_ALPHABASE_53 *ECFP_T13,
    ECFP_ALPHABASE_53 *ECFP_T14,
    ECFP_ALPHABASE_53 *ECFP_T15,
    ECFP_ALPHABASE_53 *ECFP_T16,
    ECFP_ALPHABASE_53 *ECFP_T17,
    ECFP_ALPHABASE_53 *ECFP_T18,
    ECFP_ALPHABASE_53 *ECFP_T19
};

/*
 * ecfp_alpha[i] = 1.5 * 2^(63 + i*ECFP_DSIZE) we add and subtract alpha
 * to truncate floating point numbers to a certain number of bits for
 * tidying */
static const double ecfp_alpha_64[2 * ECFP_MAXDOUBLES] = {
    ECFP_ALPHABASE_64 * ECFP_T0,
    ECFP_ALPHABASE_64 *ECFP_T1,
    ECFP_ALPHABASE_64 *ECFP_T2,
    ECFP_ALPHABASE_64 *ECFP_T3,
    ECFP_ALPHABASE_64 *ECFP_T4,
    ECFP_ALPHABASE_64 *ECFP_T5,
    ECFP_ALPHABASE_64 *ECFP_T6,
    ECFP_ALPHABASE_64 *ECFP_T7,
    ECFP_ALPHABASE_64 *ECFP_T8,
    ECFP_ALPHABASE_64 *ECFP_T9,
    ECFP_ALPHABASE_64 *ECFP_T10,
    ECFP_ALPHABASE_64 *ECFP_T11,
    ECFP_ALPHABASE_64 *ECFP_T12,
    ECFP_ALPHABASE_64 *ECFP_T13,
    ECFP_ALPHABASE_64 *ECFP_T14,
    ECFP_ALPHABASE_64 *ECFP_T15,
    ECFP_ALPHABASE_64 *ECFP_T16,
    ECFP_ALPHABASE_64 *ECFP_T17,
    ECFP_ALPHABASE_64 *ECFP_T18,
    ECFP_ALPHABASE_64 *ECFP_T19
};

/* 0.011111111111111111111111 (binary) = 0.5 - 2^25 (24 ones) */
#define ECFP_BETABASE 0.4999999701976776123046875

/*
 * We subtract beta prior to using alpha to simulate rounding down. We
 * make this close to 0.5 to round almost everything down, but exactly 0.5
 * would cause some incorrect rounding. */
static const double ecfp_beta[2 * ECFP_MAXDOUBLES] = {
    ECFP_BETABASE * ECFP_T0,
    ECFP_BETABASE *ECFP_T1,
    ECFP_BETABASE *ECFP_T2,
    ECFP_BETABASE *ECFP_T3,
    ECFP_BETABASE *ECFP_T4,
    ECFP_BETABASE *ECFP_T5,
    ECFP_BETABASE *ECFP_T6,
    ECFP_BETABASE *ECFP_T7,
    ECFP_BETABASE *ECFP_T8,
    ECFP_BETABASE *ECFP_T9,
    ECFP_BETABASE *ECFP_T10,
    ECFP_BETABASE *ECFP_T11,
    ECFP_BETABASE *ECFP_T12,
    ECFP_BETABASE *ECFP_T13,
    ECFP_BETABASE *ECFP_T14,
    ECFP_BETABASE *ECFP_T15,
    ECFP_BETABASE *ECFP_T16,
    ECFP_BETABASE *ECFP_T17,
    ECFP_BETABASE *ECFP_T18,
    ECFP_BETABASE *ECFP_T19
};

static const double ecfp_beta_160 = ECFP_BETABASE * ECFP_TWO160;
static const double ecfp_beta_192 = ECFP_BETABASE * ECFP_TWO192;
static const double ecfp_beta_224 = ECFP_BETABASE * ECFP_TWO224;

/* Affine EC Point. This is the basic representation (x, y) of an elliptic
 * curve point. */
typedef struct {
    double x[ECFP_MAXDOUBLES];
    double y[ECFP_MAXDOUBLES];
} ecfp_aff_pt;

/* Jacobian EC Point. This coordinate system uses X = x/z^2, Y = y/z^3,
 * which enables calculations with fewer inversions than affine
 * coordinates. */
typedef struct {
    double x[ECFP_MAXDOUBLES];
    double y[ECFP_MAXDOUBLES];
    double z[ECFP_MAXDOUBLES];
} ecfp_jac_pt;

/* Chudnovsky Jacobian EC Point. This coordinate system is the same as
 * Jacobian, except it keeps z^2, z^3 for faster additions. */
typedef struct {
    double x[ECFP_MAXDOUBLES];
    double y[ECFP_MAXDOUBLES];
    double z[ECFP_MAXDOUBLES];
    double z2[ECFP_MAXDOUBLES];
    double z3[ECFP_MAXDOUBLES];
} ecfp_chud_pt;

/* Modified Jacobian EC Point. This coordinate system is the same as
 * Jacobian, except it keeps a*z^4 for faster doublings. */
typedef struct {
    double x[ECFP_MAXDOUBLES];
    double y[ECFP_MAXDOUBLES];
    double z[ECFP_MAXDOUBLES];
    double az4[ECFP_MAXDOUBLES];
} ecfp_jm_pt;

struct EC_group_fp_str;

typedef struct EC_group_fp_str EC_group_fp;
struct EC_group_fp_str {
    int fpPrecision; /* Set to number of bits in mantissa, 53
                                 * or 64 */
    int numDoubles;
    int primeBitSize;
    int orderBitSize;
    int doubleBitSize;
    int numInts;
    int aIsM3; /* True if curvea == -3 (mod p), then we
                                 * can optimize doubling */
    double curvea[ECFP_MAXDOUBLES];
    /* Used to truncate a double to the number of bits in the curve */
    double bitSize_alpha;
    /* Pointer to either ecfp_alpha_53 or ecfp_alpha_64 */
    const double *alpha;

    void (*ecfp_singleReduce)(double *r, const EC_group_fp *group);
    void (*ecfp_reduce)(double *r, double *x, const EC_group_fp *group);
    /* Performs a "tidy" operation, which performs carrying, moving excess
     * bits from one double to the next double, so that the precision of
     * the doubles is reduced to the regular precision ECFP_DSIZE. This
     * might result in some float digits being negative. */
    void (*ecfp_tidy)(double *t, const double *alpha,
                      const EC_group_fp *group);
    /* Perform a point addition using coordinate system Jacobian + Affine
     * -> Jacobian. Input and output should be multi-precision floating
     * point integers. */
    void (*pt_add_jac_aff)(const ecfp_jac_pt *p, const ecfp_aff_pt *q,
                           ecfp_jac_pt *r, const EC_group_fp *group);
    /* Perform a point doubling in Jacobian coordinates. Input and output
     * should be multi-precision floating point integers. */
    void (*pt_dbl_jac)(const ecfp_jac_pt *dp, ecfp_jac_pt *dr,
                       const EC_group_fp *group);
    /* Perform a point addition using Jacobian coordinate system. Input
     * and output should be multi-precision floating point integers. */
    void (*pt_add_jac)(const ecfp_jac_pt *p, const ecfp_jac_pt *q,
                       ecfp_jac_pt *r, const EC_group_fp *group);
    /* Perform a point doubling in Modified Jacobian coordinates. Input
     * and output should be multi-precision floating point integers. */
    void (*pt_dbl_jm)(const ecfp_jm_pt *p, ecfp_jm_pt *r,
                      const EC_group_fp *group);
    /* Perform a point doubling using coordinates Affine -> Chudnovsky
     * Jacobian. Input and output should be multi-precision floating point
     * integers. */
    void (*pt_dbl_aff2chud)(const ecfp_aff_pt *p, ecfp_chud_pt *r,
                            const EC_group_fp *group);
    /* Perform a point addition using coordinates: Modified Jacobian +
     * Chudnovsky Jacobian -> Modified Jacobian. Input and output should
     * be multi-precision floating point integers. */
    void (*pt_add_jm_chud)(ecfp_jm_pt *p, ecfp_chud_pt *q,
                           ecfp_jm_pt *r, const EC_group_fp *group);
    /* Perform a point addition using Chudnovsky Jacobian coordinates.
     * Input and output should be multi-precision floating point integers.
     */
    void (*pt_add_chud)(const ecfp_chud_pt *p, const ecfp_chud_pt *q,
                        ecfp_chud_pt *r, const EC_group_fp *group);
    /* Expects out to be an array of size 16 of Chudnovsky Jacobian
     * points. Fills in Chudnovsky Jacobian form (x, y, z, z^2, z^3), for
     * -15P, -13P, -11P, -9P, -7P, -5P, -3P, -P, P, 3P, 5P, 7P, 9P, 11P,
     * 13P, 15P */
    void (*precompute_chud)(ecfp_chud_pt *out, const ecfp_aff_pt *p,
                            const EC_group_fp *group);
    /* Expects out to be an array of size 16 of Jacobian points. Fills in
     * Chudnovsky Jacobian form (x, y, z), for O, P, 2P, ... 15P */
    void (*precompute_jac)(ecfp_jac_pt *out, const ecfp_aff_pt *p,
                           const EC_group_fp *group);
};

/* Computes r = x*y.
 * r must be different (point to different memory) than x and y.
 * Does not tidy or reduce. */
void ecfp_multiply(double *r, const double *x, const double *y);

/* Performs a "tidy" operation, which performs carrying, moving excess
 * bits from one double to the next double, so that the precision of the
 * doubles is reduced to the regular precision group->doubleBitSize. This
 * might result in some float digits being negative. */
void ecfp_tidy(double *t, const double *alpha, const EC_group_fp *group);

/* Performs tidying on only the upper float digits of a multi-precision
 * floating point integer, i.e. the digits beyond the regular length which
 * are removed in the reduction step. */
void ecfp_tidyUpper(double *t, const EC_group_fp *group);

/* Performs tidying on a short multi-precision floating point integer (the
 * lower group->numDoubles floats). */
void ecfp_tidyShort(double *t, const EC_group_fp *group);

/* Performs a more mathematically precise "tidying" so that each term is
 * positive.  This is slower than the regular tidying, and is used for
 * conversion from floating point to integer. */
void ecfp_positiveTidy(double *t, const EC_group_fp *group);

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that
 * determines the field GFp.  Elliptic curve points P and R can be
 * identical.  Uses mixed Jacobian-affine coordinates. Uses 4-bit window
 * method. */
mp_err
ec_GFp_point_mul_jac_4w_fp(const mp_int *n, const mp_int *px,
                           const mp_int *py, mp_int *rx, mp_int *ry,
                           const ECGroup *ecgroup);

/* Computes R = nP where R is (rx, ry) and P is the base point. The
 * parameters a, b and p are the elliptic curve coefficients and the prime
 * that determines the field GFp.  Elliptic curve points P and R can be
 * identical.  Uses mixed Jacobian-affine coordinates (Jacobian
 * coordinates for doubles and affine coordinates for additions; based on
 * recommendation from Brown et al.). Uses window NAF method (algorithm
 * 11) for scalar-point multiplication from Brown, Hankerson, Lopez,
 * Menezes. Software Implementation of the NIST Elliptic Curves Over Prime
 * Fields. */
mp_err ec_GFp_point_mul_wNAF_fp(const mp_int *n, const mp_int *px,
                                const mp_int *py, mp_int *rx, mp_int *ry,
                                const ECGroup *ecgroup);

/* Uses mixed Jacobian-affine coordinates to perform a point
 * multiplication: R = n * P, n scalar. Uses mixed Jacobian-affine
 * coordinates (Jacobian coordinates for doubles and affine coordinates
 * for additions; based on recommendation from Brown et al.). Not very
 * time efficient but quite space efficient, no precomputation needed.
 * group contains the elliptic curve coefficients and the prime that
 * determines the field GFp.  Elliptic curve points P and R can be
 * identical. Performs calculations in floating point number format, since
 * this is faster than the integer operations on the ULTRASPARC III.
 * Uses left-to-right binary method (double & add) (algorithm 9) for
 * scalar-point multiplication from Brown, Hankerson, Lopez, Menezes.
 * Software Implementation of the NIST Elliptic Curves Over Prime Fields. */
mp_err
ec_GFp_pt_mul_jac_fp(const mp_int *n, const mp_int *px, const mp_int *py,
                     mp_int *rx, mp_int *ry, const ECGroup *ecgroup);

/* Cleans up extra memory allocated in ECGroup for this implementation. */
void ec_GFp_extra_free_fp(ECGroup *group);

/* Converts from a floating point representation into an mp_int. Expects
 * that d is already reduced. */
void
ecfp_fp2i(mp_int *mpout, double *d, const ECGroup *ecgroup);

/* Converts from an mpint into a floating point representation. */
void
ecfp_i2fp(double *out, const mp_int *x, const ECGroup *ecgroup);

/* Tests what precision floating point arithmetic is set to. This should
 * be either a 53-bit mantissa (IEEE standard) or a 64-bit mantissa
 * (extended precision on x86) and sets it into the EC_group_fp. Returns
 * either 53 or 64 accordingly. */
int ec_set_fp_precision(EC_group_fp *group);

#endif
