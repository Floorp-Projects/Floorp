/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ecp_h_
#define __ecp_h_

#include "ecl-priv.h"

/* Checks if point P(px, py) is at infinity.  Uses affine coordinates. */
mp_err ec_GFp_pt_is_inf_aff(const mp_int *px, const mp_int *py);

/* Sets P(px, py) to be the point at infinity.  Uses affine coordinates. */
mp_err ec_GFp_pt_set_inf_aff(mp_int *px, mp_int *py);

/* Computes R = P + Q where R is (rx, ry), P is (px, py) and Q is (qx,
 * qy). Uses affine coordinates. */
mp_err ec_GFp_pt_add_aff(const mp_int *px, const mp_int *py,
                         const mp_int *qx, const mp_int *qy, mp_int *rx,
                         mp_int *ry, const ECGroup *group);

/* Computes R = P - Q.  Uses affine coordinates. */
mp_err ec_GFp_pt_sub_aff(const mp_int *px, const mp_int *py,
                         const mp_int *qx, const mp_int *qy, mp_int *rx,
                         mp_int *ry, const ECGroup *group);

/* Computes R = 2P.  Uses affine coordinates. */
mp_err ec_GFp_pt_dbl_aff(const mp_int *px, const mp_int *py, mp_int *rx,
                         mp_int *ry, const ECGroup *group);

/* Validates a point on a GFp curve. */
mp_err ec_GFp_validate_point(const mp_int *px, const mp_int *py, const ECGroup *group);

#ifdef ECL_ENABLE_GFP_PT_MUL_AFF
/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that
 * determines the field GFp.  Uses affine coordinates. */
mp_err ec_GFp_pt_mul_aff(const mp_int *n, const mp_int *px,
                         const mp_int *py, mp_int *rx, mp_int *ry,
                         const ECGroup *group);
#endif

/* Converts a point P(px, py) from affine coordinates to Jacobian
 * projective coordinates R(rx, ry, rz). */
mp_err ec_GFp_pt_aff2jac(const mp_int *px, const mp_int *py, mp_int *rx,
                         mp_int *ry, mp_int *rz, const ECGroup *group);

/* Converts a point P(px, py, pz) from Jacobian projective coordinates to
 * affine coordinates R(rx, ry). */
mp_err ec_GFp_pt_jac2aff(const mp_int *px, const mp_int *py,
                         const mp_int *pz, mp_int *rx, mp_int *ry,
                         const ECGroup *group);

/* Checks if point P(px, py, pz) is at infinity.  Uses Jacobian
 * coordinates. */
mp_err ec_GFp_pt_is_inf_jac(const mp_int *px, const mp_int *py,
                            const mp_int *pz);

/* Sets P(px, py, pz) to be the point at infinity.  Uses Jacobian
 * coordinates. */
mp_err ec_GFp_pt_set_inf_jac(mp_int *px, mp_int *py, mp_int *pz);

/* Computes R = P + Q where R is (rx, ry, rz), P is (px, py, pz) and Q is
 * (qx, qy, qz).  Uses Jacobian coordinates. */
mp_err ec_GFp_pt_add_jac_aff(const mp_int *px, const mp_int *py,
                             const mp_int *pz, const mp_int *qx,
                             const mp_int *qy, mp_int *rx, mp_int *ry,
                             mp_int *rz, const ECGroup *group);

/* Computes R = 2P.  Uses Jacobian coordinates. */
mp_err ec_GFp_pt_dbl_jac(const mp_int *px, const mp_int *py,
                         const mp_int *pz, mp_int *rx, mp_int *ry,
                         mp_int *rz, const ECGroup *group);

#ifdef ECL_ENABLE_GFP_PT_MUL_JAC
/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that
 * determines the field GFp.  Uses Jacobian coordinates. */
mp_err ec_GFp_pt_mul_jac(const mp_int *n, const mp_int *px,
                         const mp_int *py, mp_int *rx, mp_int *ry,
                         const ECGroup *group);
#endif

/* Computes R(x, y) = k1 * G + k2 * P(x, y), where G is the generator
 * (base point) of the group of points on the elliptic curve. Allows k1 =
 * NULL or { k2, P } = NULL.  Implemented using mixed Jacobian-affine
 * coordinates. Input and output values are assumed to be NOT
 * field-encoded and are in affine form. */
mp_err
ec_GFp_pts_mul_jac(const mp_int *k1, const mp_int *k2, const mp_int *px,
                   const mp_int *py, mp_int *rx, mp_int *ry,
                   const ECGroup *group);

/* Computes R = nP where R is (rx, ry) and P is the base point. Elliptic
 * curve points P and R can be identical. Uses mixed Modified-Jacobian
 * co-ordinates for doubling and Chudnovsky Jacobian coordinates for
 * additions. Assumes input is already field-encoded using field_enc, and
 * returns output that is still field-encoded. Uses 5-bit window NAF
 * method (algorithm 11) for scalar-point multiplication from Brown,
 * Hankerson, Lopez, Menezes. Software Implementation of the NIST Elliptic
 * Curves Over Prime Fields. */
mp_err
ec_GFp_pt_mul_jm_wNAF(const mp_int *n, const mp_int *px, const mp_int *py,
                      mp_int *rx, mp_int *ry, const ECGroup *group);

#endif /* __ecp_h_ */
