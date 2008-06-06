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
 * The Original Code is the elliptic curve math library.
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

/* Although this is not an exported header file, code which uses elliptic
 * curve point operations will need to include it. */

#ifndef __ecl_h_
#define __ecl_h_

#include "ecl-exp.h"
#include "mpi.h"

struct ECGroupStr;
typedef struct ECGroupStr ECGroup;

/* Construct ECGroup from hexadecimal representations of parameters. */
ECGroup *ECGroup_fromHex(const ECCurveParams * params);

/* Construct ECGroup from named parameters. */
ECGroup *ECGroup_fromName(const ECCurveName name);

/* Free an allocated ECGroup. */
void ECGroup_free(ECGroup *group);

/* Construct ECCurveParams from an ECCurveName */
ECCurveParams *EC_GetNamedCurveParams(const ECCurveName name);

/* Duplicates an ECCurveParams */
ECCurveParams *ECCurveParams_dup(const ECCurveParams * params);

/* Free an allocated ECCurveParams */
void EC_FreeCurveParams(ECCurveParams * params);

/* Elliptic curve scalar-point multiplication. Computes Q(x, y) = k * P(x, 
 * y).  If x, y = NULL, then P is assumed to be the generator (base point) 
 * of the group of points on the elliptic curve. Input and output values
 * are assumed to be NOT field-encoded. */
mp_err ECPoint_mul(const ECGroup *group, const mp_int *k, const mp_int *px,
				   const mp_int *py, mp_int *qx, mp_int *qy);

/* Elliptic curve scalar-point multiplication. Computes Q(x, y) = k1 * G + 
 * k2 * P(x, y), where G is the generator (base point) of the group of
 * points on the elliptic curve. Input and output values are assumed to
 * be NOT field-encoded. */
mp_err ECPoints_mul(const ECGroup *group, const mp_int *k1,
					const mp_int *k2, const mp_int *px, const mp_int *py,
					mp_int *qx, mp_int *qy);

/* Validates an EC public key as described in Section 5.2.2 of X9.62.
 * Returns MP_YES if the public key is valid, MP_NO if the public key
 * is invalid, or an error code if the validation could not be
 * performed. */
mp_err ECPoint_validate(const ECGroup *group, const mp_int *px, const 
					mp_int *py);

#endif							/* __ecl_h_ */
