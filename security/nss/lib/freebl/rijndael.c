/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prinit.h"
#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "blapi.h"
#include "rijndael.h"

#include "cts.h"
#include "ctr.h"
#include "gcm.h"

#if USE_HW_AES
#include "intel-gcm.h"
#include "intel-aes.h"
#include "mpi.h"

static int has_intel_aes = 0;
static int has_intel_avx = 0;
static int has_intel_clmul = 0;
static PRBool use_hw_aes = PR_FALSE;
static PRBool use_hw_gcm = PR_FALSE;
#endif

/*
 * There are currently five ways to build this code, varying in performance
 * and code size.
 *
 * RIJNDAEL_INCLUDE_TABLES         Include all tables from rijndael32.tab
 * RIJNDAEL_GENERATE_TABLES        Generate tables on first 
 *                                 encryption/decryption, then store them;
 *                                 use the function gfm
 * RIJNDAEL_GENERATE_TABLES_MACRO  Same as above, but use macros to do
 *                                 the generation
 * RIJNDAEL_GENERATE_VALUES        Do not store tables, generate the table
 *                                 values "on-the-fly", using gfm
 * RIJNDAEL_GENERATE_VALUES_MACRO  Same as above, but use macros
 *
 * The default is RIJNDAEL_INCLUDE_TABLES.
 */

/*
 * When building RIJNDAEL_INCLUDE_TABLES, includes S**-1, Rcon, T[0..4], 
 *                                                 T**-1[0..4], IMXC[0..4]
 * When building anything else, includes S, S**-1, Rcon
 */
#include "rijndael32.tab"

#if defined(RIJNDAEL_INCLUDE_TABLES)
/*
 * RIJNDAEL_INCLUDE_TABLES
 */
#define T0(i)    _T0[i]
#define T1(i)    _T1[i]
#define T2(i)    _T2[i]
#define T3(i)    _T3[i]
#define TInv0(i) _TInv0[i]
#define TInv1(i) _TInv1[i]
#define TInv2(i) _TInv2[i]
#define TInv3(i) _TInv3[i]
#define IMXC0(b) _IMXC0[b]
#define IMXC1(b) _IMXC1[b]
#define IMXC2(b) _IMXC2[b]
#define IMXC3(b) _IMXC3[b]
/* The S-box can be recovered from the T-tables */
#ifdef IS_LITTLE_ENDIAN
#define SBOX(b)    ((PRUint8)_T3[b])
#else
#define SBOX(b)    ((PRUint8)_T1[b])
#endif
#define SINV(b) (_SInv[b])

#else /* not RIJNDAEL_INCLUDE_TABLES */

/*
 * Code for generating T-table values.
 */

#ifdef IS_LITTLE_ENDIAN
#define WORD4(b0, b1, b2, b3) \
    (((b3) << 24) | ((b2) << 16) | ((b1) << 8) | (b0))
#else
#define WORD4(b0, b1, b2, b3) \
    (((b0) << 24) | ((b1) << 16) | ((b2) << 8) | (b3))
#endif

/*
 * Define the S and S**-1 tables (both have been stored)
 */
#define SBOX(b)    (_S[b])
#define SINV(b) (_SInv[b])

/*
 * The function xtime, used for Galois field multiplication
 */
#define XTIME(a) \
    ((a & 0x80) ? ((a << 1) ^ 0x1b) : (a << 1))

/* Choose GFM method (macros or function) */
#if defined(RIJNDAEL_GENERATE_TABLES_MACRO) ||  \
    defined(RIJNDAEL_GENERATE_VALUES_MACRO)

/*
 * Galois field GF(2**8) multipliers, in macro form
 */
#define GFM01(a) \
    (a)                                 /* a * 01 = a, the identity */
#define GFM02(a) \
    (XTIME(a) & 0xff)                   /* a * 02 = xtime(a) */
#define GFM04(a) \
    (GFM02(GFM02(a)))                   /* a * 04 = xtime**2(a) */
#define GFM08(a) \
    (GFM02(GFM04(a)))                   /* a * 08 = xtime**3(a) */
#define GFM03(a) \
    (GFM01(a) ^ GFM02(a))               /* a * 03 = a * (01 + 02) */
#define GFM09(a) \
    (GFM01(a) ^ GFM08(a))               /* a * 09 = a * (01 + 08) */
#define GFM0B(a) \
    (GFM01(a) ^ GFM02(a) ^ GFM08(a))    /* a * 0B = a * (01 + 02 + 08) */
#define GFM0D(a) \
    (GFM01(a) ^ GFM04(a) ^ GFM08(a))    /* a * 0D = a * (01 + 04 + 08) */
#define GFM0E(a) \
    (GFM02(a) ^ GFM04(a) ^ GFM08(a))    /* a * 0E = a * (02 + 04 + 08) */

#else  /* RIJNDAEL_GENERATE_TABLES or RIJNDAEL_GENERATE_VALUES */

/* GF_MULTIPLY
 *
 * multiply two bytes represented in GF(2**8), mod (x**4 + 1)
 */
PRUint8 gfm(PRUint8 a, PRUint8 b)
{
    PRUint8 res = 0;
    while (b > 0) {
	res = (b & 0x01) ? res ^ a : res;
	a = XTIME(a);
	b >>= 1;
    }
    return res;
}

#define GFM01(a) \
    (a)                                 /* a * 01 = a, the identity */
#define GFM02(a) \
    (XTIME(a) & 0xff)                   /* a * 02 = xtime(a) */
#define GFM03(a) \
    (gfm(a, 0x03))                      /* a * 03 */
#define GFM09(a) \
    (gfm(a, 0x09))                      /* a * 09 */
#define GFM0B(a) \
    (gfm(a, 0x0B))                      /* a * 0B */
#define GFM0D(a) \
    (gfm(a, 0x0D))                      /* a * 0D */
#define GFM0E(a) \
    (gfm(a, 0x0E))                      /* a * 0E */

#endif /* choosing GFM function */

/*
 * The T-tables
 */
#define G_T0(i) \
    ( WORD4( GFM02(SBOX(i)), GFM01(SBOX(i)), GFM01(SBOX(i)), GFM03(SBOX(i)) ) )
#define G_T1(i) \
    ( WORD4( GFM03(SBOX(i)), GFM02(SBOX(i)), GFM01(SBOX(i)), GFM01(SBOX(i)) ) )
#define G_T2(i) \
    ( WORD4( GFM01(SBOX(i)), GFM03(SBOX(i)), GFM02(SBOX(i)), GFM01(SBOX(i)) ) )
#define G_T3(i) \
    ( WORD4( GFM01(SBOX(i)), GFM01(SBOX(i)), GFM03(SBOX(i)), GFM02(SBOX(i)) ) )

/*
 * The inverse T-tables
 */
#define G_TInv0(i) \
    ( WORD4( GFM0E(SINV(i)), GFM09(SINV(i)), GFM0D(SINV(i)), GFM0B(SINV(i)) ) )
#define G_TInv1(i) \
    ( WORD4( GFM0B(SINV(i)), GFM0E(SINV(i)), GFM09(SINV(i)), GFM0D(SINV(i)) ) )
#define G_TInv2(i) \
    ( WORD4( GFM0D(SINV(i)), GFM0B(SINV(i)), GFM0E(SINV(i)), GFM09(SINV(i)) ) )
#define G_TInv3(i) \
    ( WORD4( GFM09(SINV(i)), GFM0D(SINV(i)), GFM0B(SINV(i)), GFM0E(SINV(i)) ) )

/*
 * The inverse mix column tables
 */
#define G_IMXC0(i) \
    ( WORD4( GFM0E(i), GFM09(i), GFM0D(i), GFM0B(i) ) )
#define G_IMXC1(i) \
    ( WORD4( GFM0B(i), GFM0E(i), GFM09(i), GFM0D(i) ) )
#define G_IMXC2(i) \
    ( WORD4( GFM0D(i), GFM0B(i), GFM0E(i), GFM09(i) ) )
#define G_IMXC3(i) \
    ( WORD4( GFM09(i), GFM0D(i), GFM0B(i), GFM0E(i) ) )

/* Now choose the T-table indexing method */
#if defined(RIJNDAEL_GENERATE_VALUES)
/* generate values for the tables with a function*/
static PRUint32 gen_TInvXi(PRUint8 tx, PRUint8 i)
{
    PRUint8 si01, si02, si03, si04, si08, si09, si0B, si0D, si0E;
    si01 = SINV(i);
    si02 = XTIME(si01);
    si04 = XTIME(si02);
    si08 = XTIME(si04);
    si03 = si02 ^ si01;
    si09 = si08 ^ si01;
    si0B = si08 ^ si03;
    si0D = si09 ^ si04;
    si0E = si08 ^ si04 ^ si02;
    switch (tx) {
    case 0:
	return WORD4(si0E, si09, si0D, si0B);
    case 1:
	return WORD4(si0B, si0E, si09, si0D);
    case 2:
	return WORD4(si0D, si0B, si0E, si09);
    case 3:
	return WORD4(si09, si0D, si0B, si0E);
    }
    return -1;
}
#define T0(i)    G_T0(i)
#define T1(i)    G_T1(i)
#define T2(i)    G_T2(i)
#define T3(i)    G_T3(i)
#define TInv0(i) gen_TInvXi(0, i)
#define TInv1(i) gen_TInvXi(1, i)
#define TInv2(i) gen_TInvXi(2, i)
#define TInv3(i) gen_TInvXi(3, i)
#define IMXC0(b) G_IMXC0(b)
#define IMXC1(b) G_IMXC1(b)
#define IMXC2(b) G_IMXC2(b)
#define IMXC3(b) G_IMXC3(b)
#elif defined(RIJNDAEL_GENERATE_VALUES_MACRO)
/* generate values for the tables with macros */
#define T0(i)    G_T0(i)
#define T1(i)    G_T1(i)
#define T2(i)    G_T2(i)
#define T3(i)    G_T3(i)
#define TInv0(i) G_TInv0(i)
#define TInv1(i) G_TInv1(i)
#define TInv2(i) G_TInv2(i)
#define TInv3(i) G_TInv3(i)
#define IMXC0(b) G_IMXC0(b)
#define IMXC1(b) G_IMXC1(b)
#define IMXC2(b) G_IMXC2(b)
#define IMXC3(b) G_IMXC3(b)
#else  /* RIJNDAEL_GENERATE_TABLES or RIJNDAEL_GENERATE_TABLES_MACRO */
/* Generate T and T**-1 table values and store, then index */
/* The inverse mix column tables are still generated */
#define T0(i)    rijndaelTables->T0[i]
#define T1(i)    rijndaelTables->T1[i]
#define T2(i)    rijndaelTables->T2[i]
#define T3(i)    rijndaelTables->T3[i]
#define TInv0(i) rijndaelTables->TInv0[i]
#define TInv1(i) rijndaelTables->TInv1[i]
#define TInv2(i) rijndaelTables->TInv2[i]
#define TInv3(i) rijndaelTables->TInv3[i]
#define IMXC0(b) G_IMXC0(b)
#define IMXC1(b) G_IMXC1(b)
#define IMXC2(b) G_IMXC2(b)
#define IMXC3(b) G_IMXC3(b)
#endif /* choose T-table indexing method */

#endif /* not RIJNDAEL_INCLUDE_TABLES */

#if defined(RIJNDAEL_GENERATE_TABLES) ||  \
    defined(RIJNDAEL_GENERATE_TABLES_MACRO)

/* Code to generate and store the tables */

struct rijndael_tables_str {
    PRUint32 T0[256];
    PRUint32 T1[256];
    PRUint32 T2[256];
    PRUint32 T3[256];
    PRUint32 TInv0[256];
    PRUint32 TInv1[256];
    PRUint32 TInv2[256];
    PRUint32 TInv3[256];
};

static struct rijndael_tables_str *rijndaelTables = NULL;
static PRCallOnceType coRTInit = { 0, 0, 0 };
static PRStatus 
init_rijndael_tables(void)
{
    PRUint32 i;
    PRUint8 si01, si02, si03, si04, si08, si09, si0B, si0D, si0E;
    struct rijndael_tables_str *rts;
    rts = (struct rijndael_tables_str *)
                   PORT_Alloc(sizeof(struct rijndael_tables_str));
    if (!rts) return PR_FAILURE;
    for (i=0; i<256; i++) {
	/* The forward values */
	si01 = SBOX(i);
	si02 = XTIME(si01);
	si03 = si02 ^ si01;
	rts->T0[i] = WORD4(si02, si01, si01, si03);
	rts->T1[i] = WORD4(si03, si02, si01, si01);
	rts->T2[i] = WORD4(si01, si03, si02, si01);
	rts->T3[i] = WORD4(si01, si01, si03, si02);
	/* The inverse values */
	si01 = SINV(i);
	si02 = XTIME(si01);
	si04 = XTIME(si02);
	si08 = XTIME(si04);
	si03 = si02 ^ si01;
	si09 = si08 ^ si01;
	si0B = si08 ^ si03;
	si0D = si09 ^ si04;
	si0E = si08 ^ si04 ^ si02;
	rts->TInv0[i] = WORD4(si0E, si09, si0D, si0B);
	rts->TInv1[i] = WORD4(si0B, si0E, si09, si0D);
	rts->TInv2[i] = WORD4(si0D, si0B, si0E, si09);
	rts->TInv3[i] = WORD4(si09, si0D, si0B, si0E);
    }
    /* wait until all the values are in to set */
    rijndaelTables = rts;
    return PR_SUCCESS;
}

#endif /* code to generate tables */

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
rijndael_key_expansion7(AESContext *cx, const unsigned char *key, unsigned int Nk)
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
rijndael_key_expansion(AESContext *cx, const unsigned char *key, unsigned int Nk)
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
rijndael_invkey_expansion(AESContext *cx, const unsigned char *key, unsigned int Nk)
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

typedef union {
    PRUint32 w[4];
    PRUint8  b[16];
} rijndael_state;

#define COLUMN_0(state) state.w[0]
#define COLUMN_1(state) state.w[1]
#define COLUMN_2(state) state.w[2]
#define COLUMN_3(state) state.w[3]

#define STATE_BYTE(i) state.b[i]

static SECStatus 
rijndael_encryptBlock128(AESContext *cx, 
                         unsigned char *output,
                         const unsigned char *input)
{
    unsigned int r;
    PRUint32 *roundkeyw;
    rijndael_state state;
    PRUint32 C0, C1, C2, C3;
#if defined(NSS_X86_OR_X64)
#define pIn input
#define pOut output
#else
    unsigned char *pIn, *pOut;
    PRUint32 inBuf[4], outBuf[4];

    if ((ptrdiff_t)input & 0x3) {
	memcpy(inBuf, input, sizeof inBuf);
	pIn = (unsigned char *)inBuf;
    } else {
	pIn = (unsigned char *)input;
    }
    if ((ptrdiff_t)output & 0x3) {
	pOut = (unsigned char *)outBuf;
    } else {
	pOut = (unsigned char *)output;
    }
#endif
    roundkeyw = cx->expandedKey;
    /* Step 1: Add Round Key 0 to initial state */
    COLUMN_0(state) = *((PRUint32 *)(pIn     )) ^ *roundkeyw++;
    COLUMN_1(state) = *((PRUint32 *)(pIn + 4 )) ^ *roundkeyw++;
    COLUMN_2(state) = *((PRUint32 *)(pIn + 8 )) ^ *roundkeyw++;
    COLUMN_3(state) = *((PRUint32 *)(pIn + 12)) ^ *roundkeyw++;
    /* Step 2: Loop over rounds [1..NR-1] */
    for (r=1; r<cx->Nr; ++r) {
        /* Do ShiftRow, ByteSub, and MixColumn all at once */
	C0 = T0(STATE_BYTE(0))  ^
	     T1(STATE_BYTE(5))  ^
	     T2(STATE_BYTE(10)) ^
	     T3(STATE_BYTE(15));
	C1 = T0(STATE_BYTE(4))  ^
	     T1(STATE_BYTE(9))  ^
	     T2(STATE_BYTE(14)) ^
	     T3(STATE_BYTE(3));
	C2 = T0(STATE_BYTE(8))  ^
	     T1(STATE_BYTE(13)) ^
	     T2(STATE_BYTE(2))  ^
	     T3(STATE_BYTE(7));
	C3 = T0(STATE_BYTE(12)) ^
	     T1(STATE_BYTE(1))  ^
	     T2(STATE_BYTE(6))  ^
	     T3(STATE_BYTE(11));
	/* Round key addition */
	COLUMN_0(state) = C0 ^ *roundkeyw++;
	COLUMN_1(state) = C1 ^ *roundkeyw++;
	COLUMN_2(state) = C2 ^ *roundkeyw++;
	COLUMN_3(state) = C3 ^ *roundkeyw++;
    }
    /* Step 3: Do the last round */
    /* Final round does not employ MixColumn */
    C0 = ((BYTE0WORD(T2(STATE_BYTE(0))))   |
          (BYTE1WORD(T3(STATE_BYTE(5))))   |
          (BYTE2WORD(T0(STATE_BYTE(10))))  |
          (BYTE3WORD(T1(STATE_BYTE(15)))))  ^
          *roundkeyw++;
    C1 = ((BYTE0WORD(T2(STATE_BYTE(4))))   |
          (BYTE1WORD(T3(STATE_BYTE(9))))   |
          (BYTE2WORD(T0(STATE_BYTE(14))))  |
          (BYTE3WORD(T1(STATE_BYTE(3)))))   ^
          *roundkeyw++;
    C2 = ((BYTE0WORD(T2(STATE_BYTE(8))))   |
          (BYTE1WORD(T3(STATE_BYTE(13))))  |
          (BYTE2WORD(T0(STATE_BYTE(2))))   |
          (BYTE3WORD(T1(STATE_BYTE(7)))))   ^
          *roundkeyw++;
    C3 = ((BYTE0WORD(T2(STATE_BYTE(12))))  |
          (BYTE1WORD(T3(STATE_BYTE(1))))   |
          (BYTE2WORD(T0(STATE_BYTE(6))))   |
          (BYTE3WORD(T1(STATE_BYTE(11)))))  ^
          *roundkeyw++;
    *((PRUint32 *) pOut     )  = C0;
    *((PRUint32 *)(pOut + 4))  = C1;
    *((PRUint32 *)(pOut + 8))  = C2;
    *((PRUint32 *)(pOut + 12)) = C3;
#if defined(NSS_X86_OR_X64)
#undef pIn
#undef pOut
#else
    if ((ptrdiff_t)output & 0x3) {
	memcpy(output, outBuf, sizeof outBuf);
    }
#endif
    return SECSuccess;
}

static SECStatus 
rijndael_decryptBlock128(AESContext *cx, 
                         unsigned char *output,
                         const unsigned char *input)
{
    int r;
    PRUint32 *roundkeyw;
    rijndael_state state;
    PRUint32 C0, C1, C2, C3;
#if defined(NSS_X86_OR_X64)
#define pIn input
#define pOut output
#else
    unsigned char *pIn, *pOut;
    PRUint32 inBuf[4], outBuf[4];

    if ((ptrdiff_t)input & 0x3) {
	memcpy(inBuf, input, sizeof inBuf);
	pIn = (unsigned char *)inBuf;
    } else {
	pIn = (unsigned char *)input;
    }
    if ((ptrdiff_t)output & 0x3) {
	pOut = (unsigned char *)outBuf;
    } else {
	pOut = (unsigned char *)output;
    }
#endif
    roundkeyw = cx->expandedKey + cx->Nb * cx->Nr + 3;
    /* reverse the final key addition */
    COLUMN_3(state) = *((PRUint32 *)(pIn + 12)) ^ *roundkeyw--;
    COLUMN_2(state) = *((PRUint32 *)(pIn +  8)) ^ *roundkeyw--;
    COLUMN_1(state) = *((PRUint32 *)(pIn +  4)) ^ *roundkeyw--;
    COLUMN_0(state) = *((PRUint32 *)(pIn     )) ^ *roundkeyw--;
    /* Loop over rounds in reverse [NR..1] */
    for (r=cx->Nr; r>1; --r) {
	/* Invert the (InvByteSub*InvMixColumn)(InvShiftRow(state)) */
	C0 = TInv0(STATE_BYTE(0))  ^
	     TInv1(STATE_BYTE(13)) ^
	     TInv2(STATE_BYTE(10)) ^
	     TInv3(STATE_BYTE(7));
	C1 = TInv0(STATE_BYTE(4))  ^
	     TInv1(STATE_BYTE(1))  ^
	     TInv2(STATE_BYTE(14)) ^
	     TInv3(STATE_BYTE(11));
	C2 = TInv0(STATE_BYTE(8))  ^
	     TInv1(STATE_BYTE(5))  ^
	     TInv2(STATE_BYTE(2))  ^
	     TInv3(STATE_BYTE(15));
	C3 = TInv0(STATE_BYTE(12)) ^
	     TInv1(STATE_BYTE(9))  ^
	     TInv2(STATE_BYTE(6))  ^
	     TInv3(STATE_BYTE(3));
	/* Invert the key addition step */
	COLUMN_3(state) = C3 ^ *roundkeyw--;
	COLUMN_2(state) = C2 ^ *roundkeyw--;
	COLUMN_1(state) = C1 ^ *roundkeyw--;
	COLUMN_0(state) = C0 ^ *roundkeyw--;
    }
    /* inverse sub */
    pOut[ 0] = SINV(STATE_BYTE( 0));
    pOut[ 1] = SINV(STATE_BYTE(13));
    pOut[ 2] = SINV(STATE_BYTE(10));
    pOut[ 3] = SINV(STATE_BYTE( 7));
    pOut[ 4] = SINV(STATE_BYTE( 4));
    pOut[ 5] = SINV(STATE_BYTE( 1));
    pOut[ 6] = SINV(STATE_BYTE(14));
    pOut[ 7] = SINV(STATE_BYTE(11));
    pOut[ 8] = SINV(STATE_BYTE( 8));
    pOut[ 9] = SINV(STATE_BYTE( 5));
    pOut[10] = SINV(STATE_BYTE( 2));
    pOut[11] = SINV(STATE_BYTE(15));
    pOut[12] = SINV(STATE_BYTE(12));
    pOut[13] = SINV(STATE_BYTE( 9));
    pOut[14] = SINV(STATE_BYTE( 6));
    pOut[15] = SINV(STATE_BYTE( 3));
    /* final key addition */
    *((PRUint32 *)(pOut + 12)) ^= *roundkeyw--;
    *((PRUint32 *)(pOut +  8)) ^= *roundkeyw--;
    *((PRUint32 *)(pOut +  4)) ^= *roundkeyw--;
    *((PRUint32 *) pOut      ) ^= *roundkeyw--;
#if defined(NSS_X86_OR_X64)
#undef pIn
#undef pOut
#else
    if ((ptrdiff_t)output & 0x3) {
	memcpy(output, outBuf, sizeof outBuf);
    }
#endif
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
    return SECFailure;
#ifdef rijndael_large_blocks_fixed
    unsigned int j, r, Nb;
    unsigned int c2=0, c3=0;
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
#endif
}

SECStatus 
rijndael_decryptBlock(AESContext *cx, 
                      unsigned char *output,
                      const unsigned char *input)
{
    return SECFailure;
#ifdef rijndael_large_blocks_fixed
    int j, r, Nb;
    int c2=0, c3=0;
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
	output[j] = SINV(clone[j]);
    }
    /* final key addition */
    for (j=4*Nb; j>=0; j-=4) {
	COLUMN(output, j) ^= *roundkeyw--;
    }
    return SECSuccess;
#endif
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
                    unsigned int blocksize)
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
                    unsigned int blocksize)
{
    unsigned int j;
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
                    unsigned int blocksize)
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
                    unsigned int blocksize)
{
    SECStatus rv;
    AESBlockFunc *decryptor;
    const unsigned char *in;
    unsigned char *out;
    unsigned int j;
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
	    out[j] ^= in[(int)(j - blocksize)];
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

AESContext * AES_AllocateContext(void)
{
    return PORT_ZNew(AESContext);
}


#if USE_HW_AES
/*
 * Adapted from the example code in "How to detect New Instruction support in
 * the 4th generation Intel Core processor family" by Max Locktyukhin.
 */
static PRBool
check_xcr0_ymm()
{
    PRUint32 xcr0;
#if defined(_MSC_VER)
    xcr0 = (PRUint32)_xgetbv(0);  /* Requires VS2010 SP1 or later. */
#else
    __asm__ ("xgetbv" : "=a" (xcr0) : "c" (0) : "%edx");
#endif
    /* Check if xmm and ymm state are enabled in XCR0. */
    return (xcr0 & 6) == 6;
}
#endif

/*
** Initialize a new AES context suitable for AES encryption/decryption in
** the ECB or CBC mode.
** 	"mode" the mode of operation, which must be NSS_AES or NSS_AES_CBC
*/
static SECStatus   
aes_InitContext(AESContext *cx, const unsigned char *key, unsigned int keysize, 
	        const unsigned char *iv, int mode, unsigned int encrypt,
	        unsigned int blocksize)
{
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
	return SECFailure;
    }
    if (mode != NSS_AES && mode != NSS_AES_CBC) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (mode == NSS_AES_CBC && iv == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (!cx) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
    	return SECFailure;
    }
#if USE_HW_AES
    if (has_intel_aes == 0) {
	unsigned long eax, ebx, ecx, edx;
	char *disable_hw_aes = getenv("NSS_DISABLE_HW_AES");

	if (disable_hw_aes == NULL) {
	    freebl_cpuid(1, &eax, &ebx, &ecx, &edx);
	    has_intel_aes = (ecx & (1 << 25)) != 0 ? 1 : -1;
	    has_intel_clmul = (ecx & (1 << 1)) != 0 ? 1 : -1;
	    if ((ecx & (1 << 27)) != 0 && (ecx & (1 << 28)) != 0 &&
		check_xcr0_ymm()) {
		has_intel_avx = 1;
	    } else {
		has_intel_avx = -1;
	    }
	} else {
	    has_intel_aes = -1;
	    has_intel_avx = -1;
	    has_intel_clmul = -1;
	}
    }
    use_hw_aes = (PRBool)
		(has_intel_aes > 0 && (keysize % 8) == 0 && blocksize == 16);
    use_hw_gcm = (PRBool)
		(use_hw_aes && has_intel_avx>0 && has_intel_clmul>0);
#endif
    /* Nb = (block size in bits) / 32 */
    cx->Nb = blocksize / 4;
    /* Nk = (key size in bits) / 32 */
    Nk = keysize / 4;
    /* Obtain number of rounds from "table" */
    cx->Nr = RIJNDAEL_NUM_ROUNDS(Nk, cx->Nb);
    /* copy in the iv, if neccessary */
    if (mode == NSS_AES_CBC) {
	memcpy(cx->iv, iv, blocksize);
#if USE_HW_AES
	if (use_hw_aes) {
	    cx->worker = (freeblCipherFunc)
				intel_aes_cbc_worker(encrypt, keysize);
	} else
#endif
	    cx->worker = (freeblCipherFunc) (encrypt
			  ? &rijndael_encryptCBC : &rijndael_decryptCBC);
    } else {
#if  USE_HW_AES
	if (use_hw_aes) {
	    cx->worker = (freeblCipherFunc) 
				intel_aes_ecb_worker(encrypt, keysize);
	} else
#endif
	    cx->worker = (freeblCipherFunc) (encrypt
			  ? &rijndael_encryptECB : &rijndael_decryptECB);
    }
    PORT_Assert((cx->Nb * (cx->Nr + 1)) <= RIJNDAEL_MAX_EXP_KEY_SIZE);
    if ((cx->Nb * (cx->Nr + 1)) > RIJNDAEL_MAX_EXP_KEY_SIZE) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	goto cleanup;
    }
#ifdef USE_HW_AES
    if (use_hw_aes) {
	intel_aes_init(encrypt, keysize);
    } else
#endif
    {

#if defined(RIJNDAEL_GENERATE_TABLES) ||  \
	defined(RIJNDAEL_GENERATE_TABLES_MACRO)
	if (rijndaelTables == NULL) {
	    if (PR_CallOnce(&coRTInit, init_rijndael_tables)
	      != PR_SUCCESS) {
		return SecFailure;
	    }
	}
#endif
	/* Generate expanded key */
	if (encrypt) {
	    if (rijndael_key_expansion(cx, key, Nk) != SECSuccess)
		goto cleanup;
	} else {
	    if (rijndael_invkey_expansion(cx, key, Nk) != SECSuccess)
		goto cleanup;
	}
    }
    cx->worker_cx = cx;
    cx->destroy = NULL;
    cx->isBlock = PR_TRUE;
    return SECSuccess;
cleanup:
    return SECFailure;
}

SECStatus   
AES_InitContext(AESContext *cx, const unsigned char *key, unsigned int keysize, 
	        const unsigned char *iv, int mode, unsigned int encrypt,
	        unsigned int blocksize)
{
    int basemode = mode;
    PRBool baseencrypt = encrypt;
    SECStatus rv;

    switch (mode) {
    case NSS_AES_CTS:
	basemode = NSS_AES_CBC;
	break;
    case NSS_AES_GCM:
    case NSS_AES_CTR:
	basemode = NSS_AES;
	baseencrypt = PR_TRUE;
	break;
    }
    /* make sure enough is initializes so we can safely call Destroy */
    cx->worker_cx = NULL;
    cx->destroy = NULL;
    rv = aes_InitContext(cx, key, keysize, iv, basemode, 
					baseencrypt, blocksize);
    if (rv != SECSuccess) {
	AES_DestroyContext(cx, PR_FALSE);
	return rv;
    }

    /* finally, set up any mode specific contexts */
    switch (mode) {
    case NSS_AES_CTS:
	cx->worker_cx = CTS_CreateContext(cx, cx->worker, iv, blocksize);
	cx->worker = (freeblCipherFunc) 
			(encrypt ?  CTS_EncryptUpdate : CTS_DecryptUpdate);
	cx->destroy = (freeblDestroyFunc) CTS_DestroyContext;
	cx->isBlock = PR_FALSE;
	break;
    case NSS_AES_GCM:
#if USE_HW_AES
	if(use_hw_gcm) {
        	cx->worker_cx = intel_AES_GCM_CreateContext(cx, cx->worker, iv, blocksize);
		cx->worker = (freeblCipherFunc)
			(encrypt ? intel_AES_GCM_EncryptUpdate : intel_AES_GCM_DecryptUpdate);
		cx->destroy = (freeblDestroyFunc) intel_AES_GCM_DestroyContext;
		cx->isBlock = PR_FALSE;
    	} else
#endif
	{
	cx->worker_cx = GCM_CreateContext(cx, cx->worker, iv, blocksize);
	cx->worker = (freeblCipherFunc)
			(encrypt ? GCM_EncryptUpdate : GCM_DecryptUpdate);
	cx->destroy = (freeblDestroyFunc) GCM_DestroyContext;
	cx->isBlock = PR_FALSE;
	}
	break;
    case NSS_AES_CTR:
	cx->worker_cx = CTR_CreateContext(cx, cx->worker, iv, blocksize);
	cx->worker = (freeblCipherFunc) CTR_Update ;
	cx->destroy = (freeblDestroyFunc) CTR_DestroyContext;
	cx->isBlock = PR_FALSE;
	break;
    default:
	/* everything has already been set up by aes_InitContext, just
	 * return */
	return SECSuccess;
    }
    /* check to see if we succeeded in getting the worker context */
    if (cx->worker_cx == NULL) {
	/* no, just destroy the existing context */
	cx->destroy = NULL; /* paranoia, though you can see a dozen lines */
			    /* below that this isn't necessary */
	AES_DestroyContext(cx, PR_FALSE);
	return SECFailure;
    }
    return SECSuccess;
}

/* AES_CreateContext
 *
 * create a new context for Rijndael operations
 */
AESContext *
AES_CreateContext(const unsigned char *key, const unsigned char *iv, 
                  int mode, int encrypt,
                  unsigned int keysize, unsigned int blocksize)
{
    AESContext *cx = AES_AllocateContext();
    if (cx) {
	SECStatus rv = AES_InitContext(cx, key, keysize, iv, mode, encrypt,
				       blocksize);
	if (rv != SECSuccess) {
	    AES_DestroyContext(cx, PR_TRUE);
	    cx = NULL;
	}
    }
    return cx;
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
    if (cx->worker_cx && cx->destroy) {
	(*cx->destroy)(cx->worker_cx, PR_TRUE);
	cx->worker_cx = NULL;
	cx->destroy = NULL;
    }
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
    if (cx == NULL || output == NULL || (input == NULL && inputLen != 0)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    blocksize = 4 * cx->Nb;
    if (cx->isBlock && (inputLen % blocksize != 0)) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return SECFailure;
    }
    if (maxOutputLen < inputLen) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    *outputLen = inputLen;
    return (*cx->worker)(cx->worker_cx, output, outputLen, maxOutputLen,	
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
    if (cx == NULL || output == NULL || (input == NULL && inputLen != 0)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    blocksize = 4 * cx->Nb;
    if (cx->isBlock && (inputLen % blocksize != 0)) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return SECFailure;
    }
    if (maxOutputLen < inputLen) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    *outputLen = inputLen;
    return (*cx->worker)(cx->worker_cx, output, outputLen, maxOutputLen,	
                             input, inputLen, blocksize);
}
