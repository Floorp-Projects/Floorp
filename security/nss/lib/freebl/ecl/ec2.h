/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ec2_h_
#define __ec2_h_

#include "ecl-priv.h"

/* Checks if point P(px, py) is at infinity.  Uses affine coordinates. */
mp_err ec_GF2m_pt_is_inf_aff(const mp_int *px, const mp_int *py);

/* Sets P(px, py) to be the point at infinity.  Uses affine coordinates. */
mp_err ec_GF2m_pt_set_inf_aff(mp_int *px, mp_int *py);

/* Computes R = P + Q where R is (rx, ry), P is (px, py) and Q is (qx,
 * qy). Uses affine coordinates. */
mp_err ec_GF2m_pt_add_aff(const mp_int *px, const mp_int *py,
                          const mp_int *qx, const mp_int *qy, mp_int *rx,
                          mp_int *ry, const ECGroup *group);

/* Computes R = P - Q.  Uses affine coordinates. */
mp_err ec_GF2m_pt_sub_aff(const mp_int *px, const mp_int *py,
                          const mp_int *qx, const mp_int *qy, mp_int *rx,
                          mp_int *ry, const ECGroup *group);

/* Computes R = 2P.  Uses affine coordinates. */
mp_err ec_GF2m_pt_dbl_aff(const mp_int *px, const mp_int *py, mp_int *rx,
                          mp_int *ry, const ECGroup *group);

/* Validates a point on a GF2m curve. */
mp_err ec_GF2m_validate_point(const mp_int *px, const mp_int *py, const ECGroup *group);

/* by default, this routine is unused and thus doesn't need to be compiled */
#ifdef ECL_ENABLE_GF2M_PT_MUL_AFF
/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the irreducible that
 * determines the field GF2m.  Uses affine coordinates. */
mp_err ec_GF2m_pt_mul_aff(const mp_int *n, const mp_int *px,
                          const mp_int *py, mp_int *rx, mp_int *ry,
                          const ECGroup *group);
#endif

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the irreducible that
 * determines the field GF2m.  Uses Montgomery projective coordinates. */
mp_err ec_GF2m_pt_mul_mont(const mp_int *n, const mp_int *px,
                           const mp_int *py, mp_int *rx, mp_int *ry,
                           const ECGroup *group);

#ifdef ECL_ENABLE_GF2M_PROJ
/* Converts a point P(px, py) from affine coordinates to projective
 * coordinates R(rx, ry, rz). */
mp_err ec_GF2m_pt_aff2proj(const mp_int *px, const mp_int *py, mp_int *rx,
                           mp_int *ry, mp_int *rz, const ECGroup *group);

/* Converts a point P(px, py, pz) from projective coordinates to affine
 * coordinates R(rx, ry). */
mp_err ec_GF2m_pt_proj2aff(const mp_int *px, const mp_int *py,
                           const mp_int *pz, mp_int *rx, mp_int *ry,
                           const ECGroup *group);

/* Checks if point P(px, py, pz) is at infinity.  Uses projective
 * coordinates. */
mp_err ec_GF2m_pt_is_inf_proj(const mp_int *px, const mp_int *py,
                              const mp_int *pz);

/* Sets P(px, py, pz) to be the point at infinity.  Uses projective
 * coordinates. */
mp_err ec_GF2m_pt_set_inf_proj(mp_int *px, mp_int *py, mp_int *pz);

/* Computes R = P + Q where R is (rx, ry, rz), P is (px, py, pz) and Q is
 * (qx, qy, qz).  Uses projective coordinates. */
mp_err ec_GF2m_pt_add_proj(const mp_int *px, const mp_int *py,
                           const mp_int *pz, const mp_int *qx,
                           const mp_int *qy, mp_int *rx, mp_int *ry,
                           mp_int *rz, const ECGroup *group);

/* Computes R = 2P.  Uses projective coordinates. */
mp_err ec_GF2m_pt_dbl_proj(const mp_int *px, const mp_int *py,
                           const mp_int *pz, mp_int *rx, mp_int *ry,
                           mp_int *rz, const ECGroup *group);

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that
 * determines the field GF2m.  Uses projective coordinates. */
mp_err ec_GF2m_pt_mul_proj(const mp_int *n, const mp_int *px,
                           const mp_int *py, mp_int *rx, mp_int *ry,
                           const ECGroup *group);
#endif

#endif /* __ec2_h_ */
