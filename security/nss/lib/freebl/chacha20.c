/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Adopted from the public domain code in NaCl by djb. */

#include <string.h>
#include <stdio.h>

#include "chacha20.h"
#include "verified/Hacl_Chacha20.h"

void
ChaCha20XOR(unsigned char *out, const unsigned char *in, unsigned int inLen,
            const unsigned char key[32], const unsigned char nonce[12],
            uint32_t counter)
{
    Hacl_Chacha20_chacha20(out, (uint8_t *)in, inLen, (uint8_t *)key, (uint8_t *)nonce, counter);
}
