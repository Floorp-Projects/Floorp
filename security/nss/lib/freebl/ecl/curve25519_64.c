/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ecl-priv.h"
#include "../verified/Hacl_Curve25519_51.h"

SECStatus
ec_Curve25519_mul(uint8_t *mypublic, const uint8_t *secret, const uint8_t *basepoint)
{
    // Note: this cast is safe because HaCl* state has a post-condition that only "mypublic" changed.
    Hacl_Curve25519_51_ecdh(mypublic, (uint8_t *)secret, (uint8_t *)basepoint);
    return 0;
}
