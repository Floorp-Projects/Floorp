/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: rijndael.c,v 1.4 2001/06/20 03:17:00 nelsonb%netscape.com Exp $
 */

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "blapi.h"
#include "rijndael.h"

#ifndef RIJNDAEL_NO_TABLES
/* includes S**-1, Rcon, T0, T1, T2, T3, T0**-1, T1**-1, T2**-1, T3**-1 */
#include "rijndael32.tab"
#endif

#ifdef IS_LITTLE_ENDIAN
#define SBOX(b)    ((PRUint8)_T3[b])
#else
#define SBOX(b)    ((PRUint8)_T1[b])
#endif
#define SBOXINV(b) (_SInv[b])

#ifndef RIJNDAEL_NO_TABLES
/* index the tables directly */
#define T0(i)    _T0[i]
#define T1(i)    _T1[i]
#define T2(i)    _T2[i]
#define T3(i)    _T3[i]
#define TInv0(i) _TInv0[i]
#define TInv1(i) _TInv1[i]
#define TInv2(i) _TInv2[i]
#define TInv3(i) _TInv3[i]
#define ME(i)    _ME[i]
#define M9(i)    _M9[i]
#define MB(i)    _MB[i]
#define MD(i)    _MD[i]
#define IMXC0(b) _IMXC0[b]
#define IMXC1(b) _IMXC1[b]
#define IMXC2(b) _IMXC2[b]
#define IMXC3(b) _IMXC3[b]
#else
/* generate the tables on the fly */
/* XXX not finished, just here for fun */
#define XTIME(a) \
    ((a & 0x80) ? ((a << 1) ^ 0x1b) : (a << 1))
#ifdef IS_LITTLE_ENDIAN
#define WORD4(b0, b1, b2, b3) \
    (((b3) << 24) | ((b2) << 16) | ((b1) << 8) | (b0))
#else
#define WORD4(b0, b1, b2, b3) \
    (((b0) << 24) | ((b1) << 16) | ((b2) << 8) | (b3))
#endif
#define T0(i) \
    (WORD4( XTIME(SBOX(i)), SBOX(i), SBOX(i), XTIME(SBOX(i)) ^ SBOX(i) ))
#define T1(i) \
    (WORD4( XTIME(SBOX(i)) ^ SBOX(i), XTIME(SBOX(i)), SBOX(i), SBOX(i) ))
#define T2(i) \
    (WORD4( SBOX(i), XTIME(SBOX(i)) ^ SBOX(i), XTIME(SBOX(i)), SBOX(i) ))
#define T3(i) \
    (WORD4( SBOX(i), SBOX(i), XTIME(SBOX(i)) ^ SBOX(i), XTIME(SBOX(i)) ))
#endif

/**************************************************************************
 *
 * Stuff related to the Rijndael key schedule
 *
 *************************************************************************/

#define SUBBYTE(w) \
    ((SBOX((w >> 24) & 0xff) << 24) | \
     (SBOX((w >> 16) & 0xff) << 16) | \
     (SBOX((w >>  8) & 0xff) <<  8) | \
     (SBOX((w      ) & 0xff)         ))

#ifdef IS_LITTLE_ENDIAN
#define ROTBYTE(b) \
    ((b >> 8) | (b << 24))
#else
#define ROTBYTE(b) \
    ((b << 8) | (b >> 24))
#endif

/* rijndael_key_expansion7
 *
 * Generate the expanded key from the key input by the user.
 * XXX
 * Nk == 7 (224 key bits) is a weird case.  Since Nk > 6, an added SubByte
 * transformation is done periodically.  The period is every 4 bytes, and
 * since 7%4 != 0 this happens at different times for each key word (unlike
 * Nk == 8 where it happens twice in every key word, in the same positions).
 * For now, I'm implementing this case "dumbly", w/o any unrolling.
 */
static SECStatus
rijndael_key_expansion7(AESContext *cx, unsigned char *key, unsigned int Nk)
{
    unsigned int i;
    PRUint32 *W;
    PRUint32 *pW;
    PRUint32 tmp;
    W = cx->expandedKey;
    /* 1.  the first Nk words contain the cipher key */
    memcpy(W, key, Nk * 4);
    i = Nk;
    /* 2.  loop until full expanded key is obtained */
    pW = W + i - 1;
    for (; i < cx->Nb * (cx->Nr + 1); ++i) {
	tmp = *pW++;
	if (i % Nk == 0)
	    tmp = SUBBYTE(ROTBYTE(tmp)) ^ Rcon[i / Nk - 1];
	else if (i % Nk == 4)
	    tmp = SUBBYTE(tmp);
	*pW = W[i - Nk] ^ tmp;
    }
    return SECSuccess;
}

/* rijndael_key_expansion
 *
 * Generate the expanded key from the key input by the user.
 */
static SECStatus
rijndael_key_expansion(AESContext *cx, unsigned char *key, unsigned int Nk)
{
    unsigned int i;
    PRUint32 *W;
    PRUint32 *pW;
    PRUint32 tmp;
    unsigned int round_key_words = cx->Nb * (cx->Nr + 1);
    if (Nk == 7)
	return rijndael_key_expansion7(cx, key, Nk);
    W = cx->expandedKey;
    /* The first Nk words contain the input cipher key */
    memcpy(W, key, Nk * 4);
    i = Nk;
    pW = W + i - 1;
    /* Loop over all sets of Nk words, except the last */
    while (i < round_key_words - Nk) {
	tmp = *pW++;
	tmp = SUBBYTE(ROTBYTE(tmp)) ^ Rcon[i / Nk - 1];
	*pW = W[i++ - Nk] ^ tmp;
	tmp = *pW++; *pW = W[i++ - Nk] ^ tmp;
	tmp = *pW++; *pW = W[i++ - Nk] ^ tmp;
	tmp = *pW++; *pW = W[i++ - Nk] ^ tmp;
	if (Nk == 4)
	    continue;
	switch (Nk) {
	case 8: tmp = *pW++; tmp = SUBBYTE(tmp); *pW = W[i++ - Nk] ^ tmp;
	case 7: tmp = *pW++; *pW = W[i++ - Nk] ^ tmp;
	case 6: tmp = *pW++; *pW = W[i++ - Nk] ^ tmp;
	case 5: tmp = *pW++; *pW = W[i++ - Nk] ^ tmp;
	}
    }
    /* Generate the last word */
    tmp = *pW++;
    tmp = SUBBYTE(ROTBYTE(tmp)) ^ Rcon[i / Nk - 1];
    *pW = W[i++ - Nk] ^ tmp;
    /* There may be overflow here, if Nk % (Nb * (Nr + 1)) > 0.  However,
     * since the above loop generated all but the last Nk key words, there
     * is no more need for the SubByte transformation.
     */
    if (Nk < 8) {
	for (; i < round_key_words; ++i) {
	    tmp = *pW++; 
	    *pW = W[i - Nk] ^ tmp;
	}
    } else {
	/* except in the case when Nk == 8.  Then one more SubByte may have
	 * to be performed, at i % Nk == 4.
	 */
	for (; i < round_key_words; ++i) {
	    tmp = *pW++;
	    if (i % Nk == 4)
		tmp = SUBBYTE(tmp);
	    *pW = W[i - Nk] ^ tmp;
	}
    }
    return SECSuccess;
}

/* rijndael_invkey_expansion
 *
 * Generate the expanded key for the inverse cipher from the key input by 
 * the user.
 */
static SECStatus
rijndael_invkey_expansion(AESContext *cx, unsigned char *key, unsigned int Nk)
{
    unsigned int r;
    PRUint32 *roundkeyw;
    PRUint8 *b;
    int Nb = cx->Nb;
    /* begins like usual key expansion ... */
    if (rijndael_key_expansion(cx, key, Nk) != SECSuccess)
	return SECFailure;
    /* ... but has the additional step of InvMixColumn,
     * excepting the first and last round keys.
     */
    roundkeyw = cx->expandedKey + cx->Nb;
    for (r=1; r<cx->Nr; ++r) {
	/* each key word, roundkeyw, represents a column in the key
	 * matrix.  Each column is multiplied by the InvMixColumn matrix.
	 *   [ 0E 0B 0D 09 ]   [ b0 ]
	 *   [ 09 0E 0B 0D ] * [ b1 ]
	 *   [ 0D 09 0E 0B ]   [ b2 ]
	 *   [ 0B 0D 09 0E ]   [ b3 ]
	 */
	b = (PRUint8 *)roundkeyw;
	*roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ IMXC2(b[2]) ^ IMXC3(b[3]);
	b = (PRUint8 *)roundkeyw;
	*roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ IMXC2(b[2]) ^ IMXC3(b[3]);
	b = (PRUint8 *)roundkeyw;
	*roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ IMXC2(b[2]) ^ IMXC3(b[3]);
	b = (PRUint8 *)roundkeyw;
	*roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ IMXC2(b[2]) ^ IMXC3(b[3]);
	if (Nb <= 4)
	    continue;
	switch (Nb) {
	case 8: b = (PRUint8 *)roundkeyw;
	        *roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ 
	                       IMXC2(b[2]) ^ IMXC3(b[3]);
	case 7: b = (PRUint8 *)roundkeyw;
	        *roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ 
	                       IMXC2(b[2]) ^ IMXC3(b[3]);
	case 6: b = (PRUint8 *)roundkeyw;
	        *roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ 
	                       IMXC2(b[2]) ^ IMXC3(b[3]);
	case 5: b = (PRUint8 *)roundkeyw;
	        *roundkeyw++ = IMXC0(b[0]) ^ IMXC1(b[1]) ^ 
	                       IMXC2(b[2]) ^ IMXC3(b[3]);
	}
    }
    return SECSuccess;
}
/**************************************************************************
 *
 * Stuff related to Rijndael encryption/decryption, optimized for
 * a 128-bit blocksize.
 *
 *************************************************************************/

#ifdef IS_LITTLE_ENDIAN
#define BYTE0WORD(w) ((w) & 0x000000ff)
#define BYTE1WORD(w) ((w) & 0x0000ff00)
#define BYTE2WORD(w) ((w) & 0x00ff0000)
#define BYTE3WORD(w) ((w) & 0xff000000)
#else
#define BYTE0WORD(w) ((w) & 0xff000000)
#define BYTE1WORD(w) ((w) & 0x00ff0000)
#define BYTE2WORD(w) ((w) & 0x0000ff00)
#define BYTE3WORD(w) ((w) & 0x000000ff)
#endif

#define COLUMN_0(array) *((PRUint32 *)(array     ))
#define COLUMN_1(array) *((PRUint32 *)(array +  4))
#define COLUMN_2(array) *((PRUint32 *)(array +  8))
#define COLUMN_3(array) *((PRUint32 *)(array + 12))
#define COLUMN_4(array) *((PRUint32 *)(array + 16))
#define COLUMN_5(array) *((PRUint32 *)(array + 20))
#define COLUMN_6(array) *((PRUint32 *)(array + 24))
#define COLUMN_7(array) *((PRUint32 *)(array + 28))

#define STATE_BYTE(i) clone[i]

static SECStatus 
rijndael_encryptBlock128(AESContext *cx, 
                         unsigned char *output,
                         const unsigned char *input)
{
    unsigned int r;
    PRUint32 *roundkeyw;
    PRUint8 clone[RIJNDAEL_MAX_STATE_SIZE];

    roundkeyw = cx->expandedKey;
    /* Step 1: Add Round Key 0 to initial state */
    COLUMN_0(clone) = COLUMN_0(input) ^ *roundkeyw++;
    COLUMN_1(clone) = COLUMN_1(input) ^ *roundkeyw++;
    COLUMN_2(clone) = COLUMN_2(input) ^ *roundkeyw++;
    COLUMN_3(clone) = COLUMN_3(input) ^ *roundkeyw++;
    /* Step 2: Loop over rounds [1..NR-1] */
    for (r=1; r<cx->Nr; ++r) {
        /* Do ShiftRow, ByteSub, and MixColumn all at once */
	COLUMN_0(output) = T0(STATE_BYTE(0))  ^
	                   T1(STATE_BYTE(5))  ^
	                   T2(STATE_BYTE(10)) ^
	                   T3(STATE_BYTE(15));
	COLUMN_1(output) = T0(STATE_BYTE(4))  ^
	                   T1(STATE_BYTE(9))  ^
	                   T2(STATE_BYTE(14)) ^
	                   T3(STATE_BYTE(3));
	COLUMN_2(output) = T0(STATE_BYTE(8))  ^
	                   T1(STATE_BYTE(13)) ^
	                   T2(STATE_BYTE(2))  ^
	                   T3(STATE_BYTE(7));
	COLUMN_3(output) = T0(STATE_BYTE(12)) ^
	                   T1(STATE_BYTE(1))  ^
	                   T2(STATE_BYTE(6))  ^
	                   T3(STATE_BYTE(11));
	/* Round key addition */
	COLUMN_0(clone) = COLUMN_0(output) ^ *roundkeyw++;
	COLUMN_1(clone) = COLUMN_1(output) ^ *roundkeyw++;
	COLUMN_2(clone) = COLUMN_2(output) ^ *roundkeyw++;
	COLUMN_3(clone) = COLUMN_3(output) ^ *roundkeyw++;
    }
    /* Step 3: Do the last round */
    /* Final round does not employ MixColumn */
    COLUMN_0(output) = ((BYTE0WORD(T2(STATE_BYTE(0))))   |
                        (BYTE1WORD(T3(STATE_BYTE(5))))   |
                        (BYTE2WORD(T0(STATE_BYTE(10))))  |
                        (BYTE3WORD(T1(STATE_BYTE(15)))))  ^
	                *roundkeyw++;
    COLUMN_1(output) = ((BYTE0WORD(T2(STATE_BYTE(4))))   |
                        (BYTE1WORD(T3(STATE_BYTE(9))))   |
                        (BYTE2WORD(T0(STATE_BYTE(14))))  |
                        (BYTE3WORD(T1(STATE_BYTE(3)))))   ^
	                *roundkeyw++;
    COLUMN_2(output) = ((BYTE0WORD(T2(STATE_BYTE(8))))   |
                        (BYTE1WORD(T3(STATE_BYTE(13))))  |
                        (BYTE2WORD(T0(STATE_BYTE(2))))   |
                        (BYTE3WORD(T1(STATE_BYTE(7)))))   ^
	                *roundkeyw++;
    COLUMN_3(output) = ((BYTE0WORD(T2(STATE_BYTE(12))))  |
                        (BYTE1WORD(T3(STATE_BYTE(1))))   |
                        (BYTE2WORD(T0(STATE_BYTE(6))))   |
                        (BYTE3WORD(T1(STATE_BYTE(11)))))  ^
	                *roundkeyw++;
    return SECSuccess;
}

static SECStatus 
rijndael_decryptBlock128(AESContext *cx, 
                         unsigned char *output,
                         const unsigned char *input)
{
    int r;
    PRUint32 *roundkeyw;
    PRUint8 clone[RIJNDAEL_MAX_STATE_SIZE];

    roundkeyw = cx->expandedKey + cx->Nb * cx->Nr + 3;
    /* reverse the final key addition */
    COLUMN_3(clone) = COLUMN_3(input) ^ *roundkeyw--;
    COLUMN_2(clone) = COLUMN_2(input) ^ *roundkeyw--;
    COLUMN_1(clone) = COLUMN_1(input) ^ *roundkeyw--;
    COLUMN_0(clone) = COLUMN_0(input) ^ *roundkeyw--;
    /* Loop over rounds in reverse [NR..1] */
    for (r=cx->Nr; r>1; --r) {
	/* Invert the (InvByteSub*InvMixColumn)(InvShiftRow(state)) */
	COLUMN_0(output) = TInv0(STATE_BYTE(0))  ^
	                   TInv1(STATE_BYTE(13)) ^
	                   TInv2(STATE_BYTE(10)) ^
	                   TInv3(STATE_BYTE(7));
	COLUMN_1(output) = TInv0(STATE_BYTE(4))  ^
	                   TInv1(STATE_BYTE(1))  ^
	                   TInv2(STATE_BYTE(14)) ^
	                   TInv3(STATE_BYTE(11));
	COLUMN_2(output) = TInv0(STATE_BYTE(8))  ^
	                   TInv1(STATE_BYTE(5))  ^
	                   TInv2(STATE_BYTE(2))  ^
	                   TInv3(STATE_BYTE(15));
	COLUMN_3(output) = TInv0(STATE_BYTE(12)) ^
	                   TInv1(STATE_BYTE(9))  ^
	                   TInv2(STATE_BYTE(6))  ^
	                   TInv3(STATE_BYTE(3));
	/* Invert the key addition step */
	COLUMN_3(clone) = COLUMN_3(output) ^ *roundkeyw--;
	COLUMN_2(clone) = COLUMN_2(output) ^ *roundkeyw--;
	COLUMN_1(clone) = COLUMN_1(output) ^ *roundkeyw--;
	COLUMN_0(clone) = COLUMN_0(output) ^ *roundkeyw--;
    }
    /* inverse sub */
    output[ 0] = SBOXINV(clone[ 0]);
    output[ 1] = SBOXINV(clone[13]);
    output[ 2] = SBOXINV(clone[10]);
    output[ 3] = SBOXINV(clone[ 7]);
    output[ 4] = SBOXINV(clone[ 4]);
    output[ 5] = SBOXINV(clone[ 1]);
    output[ 6] = SBOXINV(clone[14]);
    output[ 7] = SBOXINV(clone[11]);
    output[ 8] = SBOXINV(clone[ 8]);
    output[ 9] = SBOXINV(clone[ 5]);
    output[10] = SBOXINV(clone[ 2]);
    output[11] = SBOXINV(clone[15]);
    output[12] = SBOXINV(clone[12]);
    output[13] = SBOXINV(clone[ 9]);
    output[14] = SBOXINV(clone[ 6]);
    output[15] = SBOXINV(clone[ 3]);
    /* final key addition */
    COLUMN_3(output) ^= *roundkeyw--;
    COLUMN_2(output) ^= *roundkeyw--;
    COLUMN_1(output) ^= *roundkeyw--;
    COLUMN_0(output) ^= *roundkeyw--;
    return SECSuccess;
}

/**************************************************************************
 *
 * Stuff related to general Rijndael encryption/decryption, for blocksizes
 * greater than 128 bits.
 *
 * XXX This code is currently untested!  So far, AES specs have only been
 *     released for 128 bit blocksizes.  This will be tested, but for now
 *     only the code above has been tested using known values.
 *
 *************************************************************************/

#define COLUMN(array, j) *((PRUint32 *)(array + j))

SECStatus 
rijndael_encryptBlock(AESContext *cx, 
                      unsigned char *output,
                      const unsigned char *input)
{
    unsigned int j, r, Nb;
    unsigned int c2, c3;
    PRUint32 *roundkeyw;
    PRUint8 clone[RIJNDAEL_MAX_STATE_SIZE];
    Nb = cx->Nb;
    roundkeyw = cx->expandedKey;
    /* Step 1: Add Round Key 0 to initial state */
    for (j=0; j<4*Nb; j+=4) {
	COLUMN(clone, j) = COLUMN(input, j) ^ *roundkeyw++;
    }
    /* Step 2: Loop over rounds [1..NR-1] */
    for (r=1; r<cx->Nr; ++r) {
	for (j=0; j<Nb; ++j) {
	    COLUMN(output, j) = T0(STATE_BYTE(4*  j          )) ^
	                        T1(STATE_BYTE(4*((j+ 1)%Nb)+1)) ^
	                        T2(STATE_BYTE(4*((j+c2)%Nb)+2)) ^
	                        T3(STATE_BYTE(4*((j+c3)%Nb)+3));
	}
	for (j=0; j<4*Nb; j+=4) {
	    COLUMN(clone, j) = COLUMN(output, j) ^ *roundkeyw++;
	}
    }
    /* Step 3: Do the last round */
    /* Final round does not employ MixColumn */
    for (j=0; j<Nb; ++j) {
	COLUMN(output, j) = ((BYTE0WORD(T2(STATE_BYTE(4* j         ))))  |
                             (BYTE1WORD(T3(STATE_BYTE(4*(j+ 1)%Nb)+1)))  |
                             (BYTE2WORD(T0(STATE_BYTE(4*(j+c2)%Nb)+2)))  |
                             (BYTE3WORD(T1(STATE_BYTE(4*(j+c3)%Nb)+3)))) ^
	                     *roundkeyw++;
    }
    return SECSuccess;
}

SECStatus 
rijndael_decryptBlock(AESContext *cx, 
                      unsigned char *output,
                      const unsigned char *input)
{
    int j, r, Nb;
    int c2, c3;
    PRUint32 *roundkeyw;
    PRUint8 clone[RIJNDAEL_MAX_STATE_SIZE];
    Nb = cx->Nb;
    roundkeyw = cx->expandedKey + cx->Nb * cx->Nr + 3;
    /* reverse key addition */
    for (j=4*Nb; j>=0; j-=4) {
	COLUMN(clone, j) = COLUMN(input, j) ^ *roundkeyw--;
    }
    /* Loop over rounds in reverse [NR..1] */
    for (r=cx->Nr; r>1; --r) {
	/* Invert the (InvByteSub*InvMixColumn)(InvShiftRow(state)) */
	for (j=0; j<Nb; ++j) {
	    COLUMN(output, 4*j) = TInv0(STATE_BYTE(4* j            )) ^
	                          TInv1(STATE_BYTE(4*(j+Nb- 1)%Nb)+1) ^
	                          TInv2(STATE_BYTE(4*(j+Nb-c2)%Nb)+2) ^
	                          TInv3(STATE_BYTE(4*(j+Nb-c3)%Nb)+3);
	}
	/* Invert the key addition step */
	for (j=4*Nb; j>=0; j-=4) {
	    COLUMN(clone, j) = COLUMN(output, j) ^ *roundkeyw--;
	}
    }
    /* inverse sub */
    for (j=0; j<4*Nb; ++j) {
	output[j] = SBOXINV(clone[j]);
    }
    /* final key addition */
    for (j=4*Nb; j>=0; j-=4) {
	COLUMN(output, j) ^= *roundkeyw--;
    }
    return SECSuccess;
}

/**************************************************************************
 *
 *  Rijndael modes of operation (ECB and CBC)
 *
 *************************************************************************/

static SECStatus 
rijndael_encryptECB(AESContext *cx, unsigned char *output,
                    unsigned int *outputLen, unsigned int maxOutputLen,
                    const unsigned char *input, unsigned int inputLen, 
                    int blocksize)
{
    SECStatus rv;
    AESBlockFunc *encryptor;
    encryptor = (blocksize == RIJNDAEL_MIN_BLOCKSIZE) 
				  ? &rijndael_encryptBlock128 
				  : &rijndael_encryptBlock;
    while (inputLen > 0) {
        rv = (*encryptor)(cx, output, input);
	if (rv != SECSuccess)
	    return rv;
	output += blocksize;
	input += blocksize;
	inputLen -= blocksize;
    }
    return SECSuccess;
}

static SECStatus 
rijndael_encryptCBC(AESContext *cx, unsigned char *output,
                    unsigned int *outputLen, unsigned int maxOutputLen,
                    const unsigned char *input, unsigned int inputLen, 
                    int blocksize)
{
    int j;
    SECStatus rv;
    AESBlockFunc *encryptor;
    unsigned char *lastblock;
    unsigned char inblock[RIJNDAEL_MAX_STATE_SIZE * 8];

    if (!inputLen)
	return SECSuccess;
    lastblock = cx->iv;
    encryptor = (blocksize == RIJNDAEL_MIN_BLOCKSIZE) 
				  ? &rijndael_encryptBlock128 
				  : &rijndael_encryptBlock;
    while (inputLen > 0) {
	/* XOR with the last block (IV if first block) */
	for (j=0; j<blocksize; ++j)
	    inblock[j] = input[j] ^ lastblock[j];
	/* encrypt */
        rv = (*encryptor)(cx, output, inblock);
	if (rv != SECSuccess)
	    return rv;
	/* move to the next block */
	lastblock = output;
	output += blocksize;
	input += blocksize;
	inputLen -= blocksize;
    }
    memcpy(cx->iv, lastblock, blocksize);
    return SECSuccess;
}

static SECStatus 
rijndael_decryptECB(AESContext *cx, unsigned char *output,
                    unsigned int *outputLen, unsigned int maxOutputLen,
                    const unsigned char *input, unsigned int inputLen, 
                    int blocksize)
{
    SECStatus rv;
    AESBlockFunc *decryptor;
    decryptor = (blocksize == RIJNDAEL_MIN_BLOCKSIZE) 
				  ? &rijndael_decryptBlock128 
				  : &rijndael_decryptBlock;
    while (inputLen > 0) {
        rv = (*decryptor)(cx, output, input);
	if (rv != SECSuccess)
	    return rv;
	output += blocksize;
	input += blocksize;
	inputLen -= blocksize;
    }
    return SECSuccess;
}

static SECStatus 
rijndael_decryptCBC(AESContext *cx, unsigned char *output,
                    unsigned int *outputLen, unsigned int maxOutputLen,
                    const unsigned char *input, unsigned int inputLen, 
                    int blocksize)
{
    SECStatus rv;
    AESBlockFunc *decryptor;
    const unsigned char *in;
    unsigned char *out;
    int j;
    unsigned char newIV[RIJNDAEL_MAX_BLOCKSIZE];

    if (!inputLen) 
	return SECSuccess;
    PORT_Assert(output - input >= 0 || input - output >= (int)inputLen );
    decryptor = (blocksize == RIJNDAEL_MIN_BLOCKSIZE) 
                                  ? &rijndael_decryptBlock128 
				  : &rijndael_decryptBlock;
    in  = input  + (inputLen - blocksize);
    memcpy(newIV, in, blocksize);
    out = output + (inputLen - blocksize);
    while (inputLen > blocksize) {
        rv = (*decryptor)(cx, out, in);
	if (rv != SECSuccess)
	    return rv;
	for (j=0; j<blocksize; ++j)
	    out[j] ^= in[j - blocksize];
	out -= blocksize;
	in -= blocksize;
	inputLen -= blocksize;
    }
    if (in == input) {
        rv = (*decryptor)(cx, out, in);
	if (rv != SECSuccess)
	    return rv;
	for (j=0; j<blocksize; ++j)
	    out[j] ^= cx->iv[j];
    }
    memcpy(cx->iv, newIV, blocksize);
    return SECSuccess;
}

/************************************************************************
 *
 * BLAPI Interface functions
 *
 * The following functions implement the encryption routines defined in
 * BLAPI for the AES cipher, Rijndael.
 *
 ***********************************************************************/

/* AES_CreateContext
 *
 * create a new context for Rijndael operations
 */
AESContext *
AES_CreateContext(unsigned char *key, unsigned char *iv, int mode, int encrypt,
                  unsigned int keysize, unsigned int blocksize)
{
    AESContext *cx;
    unsigned int Nk;
    /* According to Rijndael AES Proposal, section 12.1, block and key
     * lengths between 128 and 256 bits are supported, as long as the
     * length in bytes is divisible by 4.
     */
    if (key == NULL || 
        keysize < RIJNDAEL_MIN_BLOCKSIZE   || 
	keysize > RIJNDAEL_MAX_BLOCKSIZE   || 
	keysize % 4 != 0 ||
        blocksize < RIJNDAEL_MIN_BLOCKSIZE || 
	blocksize > RIJNDAEL_MAX_BLOCKSIZE || 
	blocksize % 4 != 0) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    if (mode != NSS_AES && mode != NSS_AES_CBC) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    if (mode == NSS_AES_CBC && iv == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    cx = PORT_ZNew(AESContext);
    if (!cx) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    	return NULL;
    }
    /* Nb = (block size in bits) / 32 */
    cx->Nb = blocksize / 4;
    /* Nk = (key size in bits) / 32 */
    Nk = keysize / 4;
    /* Obtain number of rounds from "table" */
    cx->Nr = RIJNDAEL_NUM_ROUNDS(Nk, cx->Nb);
    /* copy in the iv, if neccessary */
    if (mode == NSS_AES_CBC) {
	memcpy(cx->iv, iv, blocksize);
	cx->worker = (encrypt) ? &rijndael_encryptCBC : &rijndael_decryptCBC;
    } else {
	cx->worker = (encrypt) ? &rijndael_encryptECB : &rijndael_decryptECB;
    }
    /* Allocate memory for the expanded key */
    cx->expandedKey = PORT_ZNewArray(PRUint32, cx->Nb * (cx->Nr + 1));
    if (!cx->expandedKey) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto cleanup;
    }
    /* Generate expanded key */
    if (encrypt) {
	if (rijndael_key_expansion(cx, key, Nk) != SECSuccess)
	    goto cleanup;
    } else {
	if (rijndael_invkey_expansion(cx, key, Nk) != SECSuccess)
	    goto cleanup;
    }
    return cx;
cleanup:
    if (cx->expandedKey)
	PORT_ZFree(cx->expandedKey, cx->Nb * (cx->Nr + 1));
    PORT_ZFree(cx, sizeof *cx);
    return NULL;
}

/*
 * AES_DestroyContext
 * 
 * Zero an AES cipher context.  If freeit is true, also free the pointer
 * to the context.
 */
void 
AES_DestroyContext(AESContext *cx, PRBool freeit)
{
    PORT_ZFree(cx->expandedKey, cx->Nb * (cx->Nr + 1));
    memset(cx, 0, sizeof *cx);
    if (freeit)
	PORT_Free(cx);
}

/*
 * AES_Encrypt
 *
 * Encrypt an arbitrary-length buffer.  The output buffer must already be
 * allocated to at least inputLen.
 */
SECStatus 
AES_Encrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    int blocksize;
    /* Check args */
    if (cx == NULL || output == NULL || input == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    blocksize = 4 * cx->Nb;
    if (inputLen % blocksize != 0) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return SECFailure;
    }
    if (maxOutputLen < inputLen) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    *outputLen = inputLen;
    return (*cx->worker)(cx, output, outputLen, maxOutputLen,	
                             input, inputLen, blocksize);
}

/*
 * AES_Decrypt
 *
 * Decrypt and arbitrary-length buffer.  The output buffer must already be
 * allocated to at least inputLen.
 */
SECStatus 
AES_Decrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    int blocksize;
    /* Check args */
    if (cx == NULL || output == NULL || input == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    blocksize = 4 * cx->Nb;
    if (inputLen % blocksize != 0) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return SECFailure;
    }
    if (maxOutputLen < inputLen) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    *outputLen = inputLen;
    return (*cx->worker)(cx, output, outputLen, maxOutputLen,	
                             input, inputLen, blocksize);
}
