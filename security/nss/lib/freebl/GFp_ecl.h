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

#ifndef __gfp_ecl_h_
#define __gfp_ecl_h_
#ifdef NSS_ENABLE_ECC

#include "secmpi.h"

/* Checks if point P(px, py) is at infinity.  Uses affine coordinates. */
extern mp_err GFp_ec_pt_is_inf_aff(const mp_int *px, const mp_int *py);

/* Sets P(px, py) to be the point at infinity.  Uses affine coordinates. */
extern mp_err GFp_ec_pt_set_inf_aff(mp_int *px, mp_int *py);

/* Computes R = P + Q where R is (rx, ry), P is (px, py) and Q is (qx, qy). 
 * Uses affine coordinates.
 */
extern mp_err GFp_ec_pt_add_aff(const mp_int *p, const mp_int *a, 
    const mp_int *px, const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry);

/* Computes R = P - Q.  Uses affine coordinates. */
extern mp_err GFp_ec_pt_sub_aff(const mp_int *p, const mp_int *a, 
    const mp_int *px, const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry);

/* Computes R = 2P.  Uses affine coordinates. */
extern mp_err GFp_ec_pt_dbl_aff(const mp_int *p, const mp_int *a, 
    const mp_int *px, const mp_int *py, mp_int *rx, mp_int *ry);

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that 
 * determines the field GFp.  Uses affine coordinates.
 */
extern mp_err GFp_ec_pt_mul_aff(const mp_int *p, const mp_int *a, 
    const mp_int *b, const mp_int *px, const mp_int *py, const mp_int *n, 
    mp_int *rx, mp_int *ry);

/* Converts a point P(px, py, pz) from Jacobian projective coordinates to
 * affine coordinates R(rx, ry).
 */
extern mp_err GFp_ec_pt_jac2aff(const mp_int *px, const mp_int *py, 
    const mp_int *pz, const mp_int *p, mp_int *rx, mp_int *ry);

/* Checks if point P(px, py, pz) is at infinity.  Uses Jacobian 
 * coordinates. 
 */
extern mp_err GFp_ec_pt_is_inf_jac(const mp_int *px, const mp_int *py, 
	const mp_int *pz);

/* Sets P(px, py, pz) to be the point at infinity.  Uses Jacobian 
 * coordinates. 
 */
extern mp_err GFp_ec_pt_set_inf_jac(mp_int *px, mp_int *py, mp_int *pz);

/* Computes R = P + Q where R is (rx, ry, rz), P is (px, py, pz) and 
 * Q is (qx, qy, qz).  Uses Jacobian coordinates.
 */
extern mp_err GFp_ec_pt_add_jac(const mp_int *p, const mp_int *a, 
    const mp_int *px, const mp_int *py, const mp_int *pz, 
    const mp_int *qx, const mp_int *qy, const mp_int *qz, 
    mp_int *rx, mp_int *ry, mp_int *rz);

/* Computes R = 2P.  Uses Jacobian coordinates. */
extern mp_err GFp_ec_pt_dbl_jac(const mp_int *p, const mp_int *a, 
    const mp_int *px, const mp_int *py, const mp_int *pz, 
    mp_int *rx, mp_int *ry, mp_int *rz);

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the prime that 
 * determines the field GFp.  Uses Jacobian coordinates.
 */
mp_err GFp_ec_pt_mul_jac(const mp_int *p, const mp_int *a, const mp_int *b, 
    const mp_int *px, const mp_int *py, const mp_int *n, 
    mp_int *rx, mp_int *ry);

#define GFp_ec_pt_is_inf(px, py)    GFp_ec_pt_is_inf_aff((px), (py))
#define GFp_ec_pt_add(p, a, px, py, qx, qy, rx, ry) \
        GFp_ec_pt_add_aff((p), (a), (px), (py), (qx), (qy), (rx), (ry))

#define GFp_ECL_JACOBIAN
#ifdef GFp_ECL_AFFINE
#define GFp_ec_pt_mul(p, a, b, px, py, n, rx, ry) \
	GFp_ec_pt_mul_aff((p), (a), (b), (px), (py), (n), (rx), (ry))
#elif defined(GFp_ECL_JACOBIAN)
#define GFp_ec_pt_mul(p, a, b, px, py, n, rx, ry) \
	GFp_ec_pt_mul_jac((p), (a), (b), (px), (py), (n), (rx), (ry))
#endif /* GFp_ECL_AFFINE or GFp_ECL_JACOBIAN*/

#endif /* NSS_ENABLE_ECC */
#endif /* __gfp_ecl_h_ */
