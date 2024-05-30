/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ecl_priv_h_
#define __ecl_priv_h_

#include "ecl.h"

SECStatus ec_Curve25519_mul(PRUint8 *q, const PRUint8 *s, const PRUint8 *p);

#endif /* __ecl_priv_h_ */
