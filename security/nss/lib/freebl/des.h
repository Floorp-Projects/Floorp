/*
 *  des.h
 *
 *  header file for DES-150 library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _DES_H_
#define _DES_H_ 1

#include "blapi.h"

typedef unsigned char BYTE;
typedef unsigned int  HALF;

#define HALFPTR(x) ((HALF *)(x))
#define SHORTPTR(x) ((unsigned short *)(x))
#define BYTEPTR(x) ((BYTE *)(x))

typedef enum {
    DES_ENCRYPT = 0x5555,
    DES_DECRYPT = 0xAAAA
} DESDirection;

typedef void DESFunc(struct DESContextStr *cx, BYTE *out, const BYTE *in, 
                     unsigned int len);

struct DESContextStr {
    /* key schedule, 16 internal keys, each with 8 6-bit parts */
    HALF ks0 [32];
    HALF ks1 [32];
    HALF ks2 [32];
    HALF iv  [2];
    DESDirection direction;
    DESFunc  *worker;
};

void DES_MakeSchedule( HALF * ks, const BYTE * key,   DESDirection direction);
void DES_Do1Block(     HALF * ks, const BYTE * inbuf, BYTE * outbuf);

#endif
