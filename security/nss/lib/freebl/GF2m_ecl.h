/*
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
 * The Original Code is the elliptic curve math library for binary polynomial
 * field curves.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems, Inc. are Copyright (C) 2003
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *      Douglas Stebila <douglas@stebila.ca>
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
 */

#ifndef __gf2m_ecl_h_
#define __gf2m_ecl_h_
#ifdef NSS_ENABLE_ECC

#include "secmpi.h"

/* Checks if point P(px, py) is at infinity.  Uses affine coordinates. */
mp_err GF2m_ec_pt_is_inf_aff(const mp_int *px, const mp_int *py);

/* Sets P(px, py) to be the point at infinity.  Uses affine coordinates. */
mp_err GF2m_ec_pt_set_inf_aff(mp_int *px, mp_int *py);

/* Computes R = P + Q where R is (rx, ry), P is (px, py) and Q is (qx, qy). 
 * Uses affine coordinates.
 */
mp_err GF2m_ec_pt_add_aff(const mp_int *pp, const mp_int *a, 
    const mp_int *px, const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry);

/* Computes R = P - Q.  Uses affine coordinates. */
mp_err GF2m_ec_pt_sub_aff(const mp_int *pp, const mp_int *a, 
    const mp_int *px, const mp_int *py, const mp_int *qx, const mp_int *qy, 
    mp_int *rx, mp_int *ry);

/* Computes R = 2P.  Uses affine coordinates. */
mp_err GF2m_ec_pt_dbl_aff(const mp_int *pp, const mp_int *a, 
    const mp_int *px, const mp_int *py, mp_int *rx, mp_int *ry);

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the irreducible that 
 * determines the field GF2m.  Uses affine coordinates.
 */
mp_err GF2m_ec_pt_mul_aff(const mp_int *pp, const mp_int *a, const mp_int *b, 
    const mp_int *px, const mp_int *py, const mp_int *n, 
    mp_int *rx, mp_int *ry);

/* Computes R = nP where R is (rx, ry) and P is (px, py). The parameters
 * a, b and p are the elliptic curve coefficients and the irreducible that 
 * determines the field GF2m.  Uses Montgomery projective coordinates.
 */
mp_err GF2m_ec_pt_mul_mont(const mp_int *pp, const mp_int *a, 
    const mp_int *b, const mp_int *px, const mp_int *py, 
    const mp_int *n, mp_int *rx, mp_int *ry);

#define GF2m_ec_pt_is_inf(px, py)    GF2m_ec_pt_is_inf_aff((px), (py))
#define GF2m_ec_pt_add(p, a, px, py, qx, qy, rx, ry) \
        GF2m_ec_pt_add_aff((p), (a), (px), (py), (qx), (qy), (rx), (ry))

#define GF2m_ECL_MONTGOMERY
#ifdef GF2m_ECL_AFFINE
#define GF2m_ec_pt_mul(pp, a, b, px, py, n, rx, ry) \
	GF2m_ec_pt_mul_aff((pp), (a), (b), (px), (py), (n), (rx), (ry))
#elif defined(GF2m_ECL_MONTGOMERY)
#define GF2m_ec_pt_mul(pp, a, b, px, py, n, rx, ry) \
	GF2m_ec_pt_mul_mont((pp), (a), (b), (px), (py), (n), (rx), (ry))
#endif /* GF2m_ECL_AFFINE or GF2m_ECL_MONTGOMERY */

#endif /* NSS_ENABLE_ECC */
#endif /* __gf2m_ecl_h_ */
