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
/* $Id: prng_fips1861.c,v 1.21 2005/09/30 22:01:46 wtchang%redhat.com Exp $ */

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "prinit.h"
#include "blapi.h"
#include "nssilock.h"
#include "secitem.h"
#include "sha_fast.h"
#include "secrng.h"	/* for RNG_GetNoise() */
#include "secmpi.h"

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
 * Steps taken from Algorithm 1 of FIPS 186-2 Change Notice 1
 */

/*
 * According to FIPS 186-2, 160 <= b <= 512.
 * For our purposes, we will assume b == 160,
 * 256, or 512 (the output size of SHA-1,
 * SHA-256, or SHA-512).
 */
#define FIPS_B     256
#define B_HASH_BUF SHA256_HashBuf
#define BSIZE      (FIPS_B / PR_BITS_PER_BYTE)

/* Output size of the G function */
#define FIPS_G     160
#define GSIZE      (FIPS_G / PR_BITS_PER_BYTE)

/*
 * Add two b-bit numbers represented as arrays of BSIZE bytes.
 * The numbers are big-endian, MSB first, so addition is done
 * from the end of the buffer to the beginning.
 */
#define ADD_B_BIT_PLUS_CARRY(dest, add1, add2, cy) \
    carry = cy; \
    for (k=BSIZE-1; k>=0; --k) { \
	carry += add1[k] + add2[k]; \
	dest[k] = (PRUint8)carry; \
	carry >>= 8; \
    }

#define ADD_B_BIT_2(dest, add1, add2) \
	ADD_B_BIT_PLUS_CARRY(dest, add1, add2, 0)


/*
 * FIPS requires result from Step 3.3 to be reduced mod q when generating
 * random numbers for DSA.
 *
 * Input: w, 2*GSIZE bytes
 *        q, DSA_SUBPRIME_LEN bytes
 * Output: xj, DSA_SUBPRIME_LEN bytes
 */
static SECStatus
dsa_reduce_mod_q(const unsigned char *w, const unsigned char *q,
    unsigned char *xj)
{
    mp_int W, Q, Xj;
    mp_err err;
    SECStatus rv = SECSuccess;

    /* Initialize MPI integers. */
    MP_DIGITS(&W) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&Xj) = 0;
    CHECK_MPI_OK( mp_init(&W) );
    CHECK_MPI_OK( mp_init(&Q) );
    CHECK_MPI_OK( mp_init(&Xj) );
    /*
     * Convert input arguments into MPI integers.
     */
    CHECK_MPI_OK( mp_read_unsigned_octets(&W, w, 2*GSIZE) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&Q, q, DSA_SUBPRIME_LEN) );
    /*
     * Algorithm 1 of FIPS 186-2 Change Notice 1, Step 3.3
     *
     * xj = (w0 || w1) mod q
     */
    CHECK_MPI_OK( mp_mod(&W, &Q, &Xj) );
    CHECK_MPI_OK( mp_to_fixlen_octets(&Xj, xj, DSA_SUBPRIME_LEN) );
cleanup:
    mp_clear(&W);
    mp_clear(&Q);
    mp_clear(&Xj);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    return rv;
}

/*
 * Specialized SHA1-like function.  This function appends zeroes to a 
 * single input block and runs a single instance of the compression function, 
 * as specified in FIPS 186-2 appendix 3.3.
 */
static void 
RNG_UpdateAndEnd_FIPS186_2(SHA1Context *ctx, 
                           unsigned char *input, unsigned int inputLen,
                           unsigned char *hashout, unsigned int *pDigestLen, 
                           unsigned int maxDigestLen);

/*
 * Global RNG context
 */ 
struct RNGContextStr {
    PRUint8   XKEY[BSIZE]; /* Seed for next SHA iteration */
    PRUint8   Xj[2*GSIZE]; /* Output from previous operation. */
    PZLock   *lock;        /* Lock to serialize access to global rng */
    PRUint8   avail;       /* # bytes of output available, [0...2*GSIZE] */
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
 * Implementation of Algorithm 1 of FIPS 186-2 Change Notice 1,
 * hereinafter called alg_cn_1().  It is assumed a lock for the global
 * rng context has already been acquired.
 * Calling this function with XSEEDj == NULL is equivalent to saying there
 * is no optional user input, which is further equivalent to saying that
 * the optional user input is 0.
 */
static SECStatus
alg_fips186_2_cn_1(RNGContext *rng, const unsigned char *XSEEDj)
{
    /* SHA1 context for G(t, XVAL) function */
    SHA1Context sha1cx;
    /* input to hash function */
    PRUint8 XVAL[BSIZE];
    /* store a copy of the output to compare with the previous output */
    PRUint8 x_j[2*GSIZE];
    /* used by ADD_B_BIT macros */
    int k, carry;
    /* store the output of G(t, XVAL) in the rightmost GSIZE bytes */
    PRUint8 w_i[BSIZE];
    int i;
    unsigned int len;
    if (!rng->isValid) {
	/* RNG has alread entered an invalid state. */
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
#if GSIZE < BSIZE
    /* zero the leftmost bytes so we can pass it to ADD_B_BIT_PLUS_CARRY */
    memset(w_i, 0, BSIZE - GSIZE);
#endif
    /* 
     * <Step 2> Initialize t, taken care of in SHA-1 (same initial values)
     *
     * <Step 3.1> XSEEDj is optional user input
     */ 
    for (i = 0; i < 2; i++) {
	/* 
	 * <Step 3.2a> XVAL = (XKEY + XSEEDj) mod 2^b
	 *     :always reduced mod 2^b, since storing as b-bit value
	 */
	if (XSEEDj) {
	    /* XSEEDj > 0 */
	    ADD_B_BIT_2(XVAL, rng->XKEY, XSEEDj);
	} else {
	    /* XSEEDj == 0 */
	    memcpy(XVAL, rng->XKEY, BSIZE);
	}
	/* 
	 * <Step 3.2b> Wi = G(t, XVAL)
	 *     :FIPS 186-2 specifies a different padding than the SHA1 180-1
	 *     :specification, this function is implemented in
	 *     :RNG_UpdateAndEnd_FIPS186_2 below.
	 */ 
	SHA1_Begin(&sha1cx);
	RNG_UpdateAndEnd_FIPS186_2(&sha1cx, XVAL, BSIZE,
				   &w_i[BSIZE - GSIZE], &len, GSIZE);
	/* 
	 * <Step 3.2c> XKEY = (1 + XKEY + Wi) mod 2^b
	 *     :always reduced mod 2^b, since storing as 160-bit value 
	 */
	ADD_B_BIT_PLUS_CARRY(rng->XKEY, rng->XKEY, w_i, 1);
	/*
	 * <Step 3.3> Xj = (W0 || W1)
	 */
	memcpy(&x_j[i*GSIZE], &w_i[BSIZE - GSIZE], GSIZE);
    }
    /*     [FIPS 140-2] verify output does not match previous output */
    if (memcmp(x_j, rng->Xj, 2*GSIZE) == 0) {
	/* failed FIPS 140-2 continuous RNG test.  RNG now invalid. */
	rng->isValid = PR_FALSE;
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    /* Xj is the output */
    memcpy(rng->Xj, x_j, 2*GSIZE);
    /* Always have a full buffer after executing alg_cn_1() */
    rng->avail = 2*GSIZE;
    /* housekeeping */
    memset(&w_i[BSIZE - GSIZE], 0, GSIZE);
    memset(x_j, 0, 2*GSIZE);
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
	    PORT_Free(globalrng);
	    globalrng = NULL;
	    PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
	    return PR_FAILURE;
	}
	/* the RNG is in a valid state */
	globalrng->isValid = PR_TRUE;
	/* Try to get some seed data for the RNG */
	numBytes = RNG_GetNoise(bytes, sizeof bytes);
	RNG_RandomUpdate(bytes, numBytes);
    }
    return PR_SUCCESS;
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
static SECStatus 
prng_RandomUpdate(RNGContext *rng, const void *data, size_t bytes)
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
	rv = B_HASH_BUF(inputhash, data, bytes);
    if (rv != SECSuccess) {
	/* B_HASH_BUF set error */
	return SECFailure;
    }
    /* --- LOCKED --- */
    PZ_Lock(rng->lock);
    /*
     * Random information is initially supplied by a call to
     * RNG_SystemInfoForRNG().  That function collects entropy from
     * the system and calls RNG_RandomUpdate() to seed the generator.
     * Algorithm 1 of FIPS 186-2 Change Notice 1, step 1 specifies that
     * a secret value for the seed-key must be chosen before the
     * generator can begin.  The size of XKEY is b bits, so fill it
     * with the first b bits sent to RNG_RandomUpdate().
     */
    if (rng->seedCount == 0) {
	/* This is the first call to RandomUpdate().  Use a hash
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
	rv = alg_fips186_2_cn_1(rng, NULL);
	/* As per FIPS 140-2 continuous RNG test requirement, the first
	 * iteration of output is discarded.  So here there is really
	 * no output available.  This forces another execution of alg_cn_1()
	 * before any bytes can be extracted from the generator.
	 */
	rng->avail = 0;
    } else {
	/* Execute the algorithm from FIPS 186-2 Change Notice 1 */
	rv = alg_fips186_2_cn_1(rng, inputhash);
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
** material.
*/
SECStatus 
RNG_RandomUpdate(const void *data, size_t bytes)
{
    return prng_RandomUpdate(globalrng, data, bytes);
}

/*
** Generate some random bytes, using the global random number generator
** object.
*/
SECStatus 
prng_GenerateGlobalRandomBytes(RNGContext *rng,
                               void *dest, size_t len)
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
     * else call alg_cn_1() with 0's to generate more random data.
     */
    while (len > 0 && rv == SECSuccess) {
	if (rng->avail == 0) {
	    /* All available bytes are used, so generate more. */
	    rv = alg_fips186_2_cn_1(rng, NULL);
	}
	/* number of bytes to obtain on this iteration (max of 40) */
	num = PR_MIN(rng->avail, len);
	/*
	 * if avail < 2*GSIZE, the first 2*GSIZE - avail bytes have
	 * already been used.
	 */
	if (num) {
	    memcpy(output, rng->Xj + (2*GSIZE - rng->avail), num);
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
** object.
*/
SECStatus 
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
    return prng_GenerateGlobalRandomBytes(globalrng, dest, len);
}

void
RNG_RNGShutdown(void)
{
    /* check for a valid global RNG context */
    PORT_Assert(globalrng != NULL);
    if (globalrng == NULL) {
	/* Should set a "not initialized" error code. */
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
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
 *       The PRNG specified in FIPS 186-2 3.3 uses a function, G,
 *       which has the same initialization and compression functions
 *       as SHA1 180-1, but uses different padding.  FIPS 186-2 3.3 
 *       specifies that the message be padded with 0's until the size
 *       reaches 512 bits.
 */
static void 
RNG_UpdateAndEnd_FIPS186_2(SHA1Context *ctx, 
                           unsigned char *input, unsigned int inputLen,
                           unsigned char *hashout, unsigned int *pDigestLen, 
                           unsigned int maxDigestLen)
{
#if defined(SHA_NEED_TMP_VARIABLE)
    register PRUint32 tmp;
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
    SHA_STORE_RESULT;
    *pDigestLen = SHA1_LENGTH;
}

/*
 * Specialized RNG for DSA
 *
 * As per Algorithm 1 of FIPS 186-2 Change Notice 1, in step 3.3 the value
 * Xj should be reduced mod q, a 160-bit prime number.  Since this parameter
 * is only meaningful in the context of DSA, the above RNG functions
 * were implemented without it.  They are re-implemented below for use
 * with DSA.
 *
 */

/*
** Generate some random bytes, using the global random number generator
** object.  In DSA mode, so there is a q.
*/
SECStatus 
DSA_GenerateGlobalRandomBytes(void *dest, size_t len, const unsigned char *q)
{
    SECStatus rv;
    unsigned char w[2*GSIZE];

    PORT_Assert(q && len == DSA_SUBPRIME_LEN);
    if (len != DSA_SUBPRIME_LEN) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    if (*q == 0) {
        ++q;
    }
    rv = prng_GenerateGlobalRandomBytes(globalrng, w, 2*GSIZE);
    if (rv != SECSuccess) {
	return rv;
    }
    dsa_reduce_mod_q(w, q, (unsigned char *)dest);
    return rv;
}
