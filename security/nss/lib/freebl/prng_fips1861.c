/*
 *
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "prinit.h"
#include "blapi.h"
#include "nssilock.h"
#include "secitem.h"
#include "sha_fast.h"
#include "secrng.h"	/* for RNG_GetNoise() */

/*
 * The minimum amount of seed data required before the generator will
 * provide data.
 * Note that this is a measure of the number of bytes sent to
 * RNG_RandomUpdate, not the actual amount of entropy present in the
 * generator.  Naturally, it is impossible to know (at this level) just
 * how much entropy is present in the provided seed data.  A bare minimum
 * of entropy would be 20 bytes, so by requiring 1K this code is making
 * the tacit assumption that at least 1 byte of pure entropy is provided
 * with every 8 bytes supplied to RNG_RandomUpdate.  The reality of this
 * assumption is left up to the caller.
 */
#define MIN_SEED_COUNT 1024

/*
 * Steps taken from FIPS 186-1 Appendix 3.1
 */

/*
 * According to FIPS 186-1, 160 <= b <= 512
 * For our purposes, we will assume b == 160
 */
#define FIPS_B     160
#define BSIZE      FIPS_B / BITS_PER_BYTE

/*
 * Add two 160-bit numbers represented as arrays of 20 bytes.
 * The numbers are big-endian, MSB first, so addition is done
 * from the end of the buffer to the beginning.
 */
#define ADD_160BIT_PLUS_CARRY(dest, add1, add2, cy) \
    carry = cy; \
    for (i=BSIZE-1; i>=0; --i) { \
	carry += add1[i] + add2[i]; \
	dest[i] = (PRUint8)carry; \
	carry >>= 8; \
    }

#define ADD_160BIT_2(dest, add1, add2) \
	ADD_160BIT_PLUS_CARRY(dest, add1, add2, 0)


/*
 * FIPS requires result from Step 3c to be reduced mod q when generating
 * random numbers for DSA.
 * by definition q >= 2^159 + 1, thus xj < 2q
 * thus reducing mod q is simple subtraction when xj > q
 */
#define dsa_reduce_mod_q(xj, q) \
    PORT_Assert(q[0] >= 0x80); \
    if (memcmp(xj,q,BSIZE) > 0) { \
	carry = 0; \
	for (i=BSIZE-1; i>=0; --i) { \
	    carry += (signed int)xj[i] - (signed int)q[i]; \
	    xj[i] = (PRUint8)carry; \
	    carry >>= 8; \
	} \
    }

/*
 * Specialized SHA1-like function.  This function appends zeroes to a 
 * single input block and runs a single instance of the compression function, 
 * as specified in FIPS 186-1 appendix 3.3.
 */
void 
RNG_UpdateAndEnd_FIPS186_1(SHA1Context *ctx, 
                           unsigned char *input, unsigned int inputLen,
                           unsigned char *hashout, unsigned int *pDigestLen, 
                           unsigned int maxDigestLen);

/*
 * Global RNG context
 */ 
struct RNGContextStr {
    PRUint8   XKEY[BSIZE]; /* Seed for next SHA iteration */
    PRUint8   Xj[BSIZE];   /* Output from previous operation */
    PZLock   *lock;        /* Lock to serialize access to global rng */
    PRUint8   avail;       /* # bytes of output available, [0...20] */
    PRUint32  seedCount;   /* number of seed bytes given to generator */
    PRBool    isValid;     /* false if RNG reaches an invalid state */
};
typedef struct RNGContextStr RNGContext;
static RNGContext *globalrng = NULL;

/*
 * Free the global RNG context
 */
static void
freeRNGContext()
{
    PZ_DestroyLock(globalrng->lock);
    PORT_ZFree(globalrng, sizeof *globalrng);
    globalrng = NULL;
}

/*
 * Implementation of the algorithm in FIPS 186-1 appendix 3.1, heretofore
 * called alg3_1().  It is assumed a lock for the global rng context has
 * already been acquired.
 * Calling this function with XSEEDj == NULL is equivalent to saying there
 * is no optional user input, which is further equivalent to saying that
 * the optional user input is 0.
 */
static SECStatus
alg_fips186_1_x3_1(RNGContext *rng,
                   const unsigned char *XSEEDj, unsigned char *q)
{
    /* SHA1 context for G(t, XVAL) function */
    SHA1Context sha1cx;
    /* input to hash function */
    PRUint8 XVAL[BSIZE];
    /* store a copy of the output to compare with the previous output */
    PRUint8 x_j[BSIZE];
    /* used by ADD_160BIT macros */
    int i, carry;
    unsigned int len;
    if (!rng->isValid) {
	/* RNG has alread entered an invalid state. */
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* 
     * <Step 2> Initialize t, taken care of in SHA-1 (same initial values) 
     */
    SHA1_Begin(&sha1cx);
    /* 
     * <Step 3a> XSEEDj is optional user input
     * 
     * <Step 3b> XVAL = (XKEY + XSEEDj) mod 2^b
     *     :always reduced mod 2^b, since storing as 160-bit value
     */
    if (XSEEDj) {
	/* XSEEDj > 0 */
	ADD_160BIT_2(XVAL, rng->XKEY, XSEEDj);
    } else {
	/* XSEEDj == 0 */
	memcpy(XVAL, rng->XKEY, BSIZE);
    }
    /* 
     * <Step 3c> Xj = G(t, XVAL) mod q
     *     :In this code, (mod q) is only understood for DSA ops, 
     *     :not general RNG (what would q be in non-DSA uses?).
     *     :If a q is specified, use it.
     *     :FIPS 186-1 specifies a different padding than the SHA1 180-1
     *     :specification, this function is implemented below.
     */ 
    RNG_UpdateAndEnd_FIPS186_1(&sha1cx, XVAL, BSIZE, x_j, &len, BSIZE);
    if (q != NULL) {
	dsa_reduce_mod_q(x_j, q);
    }
    /*     [FIPS 140-1] verify output does not match previous output */
    if (memcmp(x_j, rng->Xj, BSIZE) == 0) {
	/* failed FIPS 140-1 continuous RNG condition.  RNG now invalid. */
	rng->isValid = PR_FALSE;
	return SECFailure;
    }
    /* Xj is the output */
    memcpy(rng->Xj, x_j, BSIZE);
    /* 
     * <Step 3d> XKEY = (1 + XKEY + Xj) mod 2^b
     *     :always reduced mod 2^b, since storing as 160-bit value 
     */
    ADD_160BIT_PLUS_CARRY(rng->XKEY, rng->XKEY, x_j, 1);
    /* Always have a full buffer after executing alg3_1() */
    rng->avail = BSIZE;
    /* housekeeping */
    memset(x_j, 0, BSIZE);
    memset(XVAL, 0, BSIZE);
    return SECSuccess;
}

/* Use NSPR to prevent RNG_RNGInit from being called from separate
 * threads, creating a race condition.
 */
static PRCallOnceType coRNGInit = { 0, 0, 0 };
static PRStatus rng_init(void)
{
    unsigned char bytes[120];
    unsigned int numBytes;
    if (globalrng == NULL) {
	/* create a new global RNG context */
	globalrng = (RNGContext *)PORT_ZAlloc(sizeof(RNGContext));
	if (globalrng == NULL) {
	    PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
	    return PR_FAILURE;
	}
	/* create a lock for it */
	globalrng->lock = PZ_NewLock(nssILockOther);
	if (globalrng->lock == NULL) {
	    PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
	    return PR_FAILURE;
	}
	/* the RNG is in a valid state */
	globalrng->isValid = PR_TRUE;
	/* Try to get some seed data for the RNG */
	numBytes = RNG_GetNoise(bytes, sizeof bytes);
	RNG_RandomUpdate(bytes, numBytes);
    }
    return (globalrng != NULL) ? PR_SUCCESS : PR_FAILURE;
}

/*
 * Initialize the global RNG context and give it some seed input taken
 * from the system.  This function is thread-safe and will only allow
 * the global context to be initialized once.  The seed input is likely
 * small, so it is imperative that RNG_RandomUpdate() be called with
 * additional seed data before the generator is used.  A good way to
 * provide the generator with additional entropy is to call
 * RNG_SystemInfoForRNG().  Note that NSS_Init() does exactly that.
 */
SECStatus 
RNG_RNGInit(void)
{
    /* Allow only one call to initialize the context */
    PR_CallOnce(&coRNGInit, rng_init);
    /* Make sure there is a context */
    return (globalrng != NULL) ? PR_SUCCESS : PR_FAILURE;
}

/*
** Update the global random number generator with more seeding
** material
*/
SECStatus 
prng_RandomUpdate(RNGContext *rng, const void *data, size_t bytes, 
	unsigned char *q)
{
    SECStatus rv = SECSuccess;
    unsigned char inputhash[BSIZE];
    /* check for a valid global RNG context */
    PORT_Assert(rng != NULL);
    if (rng == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* RNG_SystemInfoForRNG() sometimes does this, not really an error */
    if (bytes == 0)
	return SECSuccess;
    /* If received 20 bytes of input, use it, else hash the input before 
     * locking.
     */
    if (bytes == BSIZE)
	memcpy(inputhash, data, BSIZE);
    else
	rv = SHA1_HashBuf(inputhash, data, bytes);
    if (rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* --- LOCKED --- */
    PZ_Lock(rng->lock);
    /*
     * Random information is initially supplied by a call to
     * RNG_SystemInfoForRNG().  That function collects entropy from
     * the system and calls RNG_RandomUpdate() to seed the generator.
     * FIPS 186-1 3.1 step 1 specifies that a secret value for the
     * seed-key must be chosen before the generator can begin.  The
     * size of XKEY is b-bytes, so fill it with the first b-bytes
     * sent to RNG_RandomUpdate().
     */
    if (rng->seedCount == 0) {
	/* This is the first call to RandomUpdate().  Use a SHA1 hash
	 * of the input to set the seed, XKEY.
	 *
	 * <Step 1> copy seed bytes into context's XKEY
	 */
	memcpy(rng->XKEY, inputhash, BSIZE);
	/* 
	 * Now continue with algorithm.  Since the input was used to
	 * initialize XKEY, the "optional user input" at this stage
	 * will be a pad of zeros, XSEEDj = 0.
	 */
	rv = alg_fips186_1_x3_1(rng, NULL, q);
	/* As per FIPS 140-1 continuous RNG requirement, the first
	 * iteration of output is discarded.  So here there is really
	 * no output available.  This forces another execution of alg3_1()
	 * before any bytes can be extracted from the generator.
	 */
	rng->avail = 0;
    } else {
	/* Execute the algorithm from FIPS 186-1 appendix 3.1 */
	rv = alg_fips186_1_x3_1(rng, inputhash, q);
    }
    /* If got this far, have added bytes of seed data. */
    rng->seedCount += bytes;
    PZ_Unlock(rng->lock);
    /* --- UNLOCKED --- */
    /* housekeeping */
    memset(inputhash, 0, BSIZE);
    return rv;
}

/*
** Update the global random number generator with more seeding
** material.  Not DSA, so no q.
*/
SECStatus 
RNG_RandomUpdate(const void *data, size_t bytes)
{
    return prng_RandomUpdate(globalrng, data, bytes, NULL);
}

/*
** Generate some random bytes, using the global random number generator
** object.
*/
SECStatus 
prng_GenerateGlobalRandomBytes(RNGContext *rng,
                               void *dest, size_t len, unsigned char *q)
{
    PRUint8 num;
    SECStatus rv = SECSuccess;
    unsigned char *output = dest;
    /* check for a valid global RNG context */
    PORT_Assert(rng != NULL);
    if (rng == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* --- LOCKED --- */
    PZ_Lock(rng->lock);
    /* Check the amount of seed data in the generator.  If not enough,
     * don't produce any data.
     */
    if (rng->seedCount < MIN_SEED_COUNT) {
	PZ_Unlock(rng->lock);
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
	return SECFailure;
    }
    /*
     * If there are enough bytes of random data, send back Xj, 
     * else call alg3_1() with 0's to generate more random data.
     */
    while (len > 0 && rv == SECSuccess) {
	if (rng->avail == 0) {
	    /* All available bytes are used, so generate more. */
	    rv = alg_fips186_1_x3_1(rng, NULL, q);
	}
	/* number of bytes to obtain on this iteration (max of 20) */
	num = PR_MIN(rng->avail, len);
	/* if avail < BSIZE, the first avail bytes have already been used. */
	if (num) {
	    memcpy(output, rng->Xj + (BSIZE - rng->avail), num);
	    rng->avail -= num;
	    len -= num;
	    output += num;
	}
    }
    PZ_Unlock(rng->lock);
    /* --- UNLOCKED --- */
    return rv;
}

/*
** Generate some random bytes, using the global random number generator
** object.  Not DSA, so no q.
*/
SECStatus 
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
    return prng_GenerateGlobalRandomBytes(globalrng, dest, len, NULL);
}

void
RNG_RNGShutdown(void)
{
    /* check for a valid global RNG context */
    PORT_Assert(globalrng != NULL);
    if (globalrng == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return;
    }
    /* clear */
    freeRNGContext();
    /* zero the callonce struct to allow a new call to RNG_RNGInit() */
    memset(&coRNGInit, 0, sizeof coRNGInit);
}

/*
 *  SHA: Generate hash value from context
 *       Specialized function for PRNG
 *       The PRNG specified in FIPS 186-1 3.3 uses a function, G,
 *       which has the same initialization and compression functions
 *       as SHA1 180-1, but uses different padding.  FIPS 186-1 3.3 
 *       specifies that the message be padded with 0's until the size
 *       reaches 512 bits.
 */
void 
RNG_UpdateAndEnd_FIPS186_1(SHA1Context *ctx, 
                           unsigned char *input, unsigned int inputLen,
                           unsigned char *hashout, unsigned int *pDigestLen, 
                           unsigned int maxDigestLen)
{
#if defined(IS_LITTLE_ENDIAN)
    register PRUint32 A;
#endif
    static const unsigned char bulk_pad0[64] = { 0,0,0,0,0,0,0,0,0,0,
               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  };

    PORT_Assert(maxDigestLen >= SHA1_LENGTH);
    PORT_Assert(inputLen <= SHA1_INPUT_LEN);

    /*
     *  Add the input
     */
    SHA1_Update(ctx, input, inputLen);
    /*
     *  Pad with zeroes
     *  This will fill the input block and cause the compression function
     *  to be called.
     */
    SHA1_Update(ctx, bulk_pad0, SHA1_INPUT_LEN - inputLen);

    /*
     *  Output hash
     */
#if defined(IS_LITTLE_ENDIAN)
    SHA_BYTESWAP(ctx->H[0]);
    SHA_BYTESWAP(ctx->H[1]);
    SHA_BYTESWAP(ctx->H[2]);
    SHA_BYTESWAP(ctx->H[3]);
    SHA_BYTESWAP(ctx->H[4]);
#endif
    memcpy(hashout, ctx->H, SHA1_LENGTH);
    *pDigestLen = SHA1_LENGTH;

    /*
     *  Re-initialize the context (also zeroizes contents)
     */
    SHA1_Begin(ctx);
}

/*
 * Specialized RNG for DSA
 *
 * As per FIPS 186-1 appendix 3.1, in step 5 the value Xj should
 * be reduced mod q, a 160-bit prime number.   Since this parameter is
 * only meaningful in the context of DSA, the above RNG functions
 * were implemented without it.  They are re-implemented below for use
 * with DSA.
 *
 */

/*
** Update the global random number generator with more seeding
** material.  DSA needs a q parameter.
*/
SECStatus 
DSA_RandomUpdate(void *data, size_t bytes, unsigned char *q)
{
    if( q && (*q == 0) ) {
        ++q;
    }
    return prng_RandomUpdate(globalrng, data, bytes, q);
}

/*
** Generate some random bytes, using the global random number generator
** object.  In DSA mode, so there is a q.
*/
SECStatus 
DSA_GenerateGlobalRandomBytes(void *dest, size_t len, unsigned char *q)
{
    if( q && (*q == 0) ) {
        ++q;
    }
    return prng_GenerateGlobalRandomBytes(globalrng, dest, len, q);
}
