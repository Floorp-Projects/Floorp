/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PQG parameter generation/verification.  Based on FIPS 186-3.
 */
#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "blapi.h"
#include "secitem.h"
#include "mpi.h"
#include "mpprime.h"
#include "mplogic.h"
#include "secmpi.h"

#define MAX_ITERATIONS 1000 /* Maximum number of iterations of primegen */

typedef enum {
    FIPS186_1_TYPE,   /* Probablistic */
    FIPS186_3_TYPE,   /* Probablistic */
    FIPS186_3_ST_TYPE /* Shawe-Taylor provable */
} pqgGenType;

/*
 * These test iterations are quite a bit larger than we previously had.
 * This is because FIPS 186-3 is worried about the primes in PQG generation.
 * It may be possible to purposefully construct composites which more
 * iterations of Miller-Rabin than the for your normal randomly selected
 * numbers.There are 3 ways to counter this: 1) use one of the cool provably
 * prime algorithms (which would require a lot more work than DSA-2 deservers.
 * 2) add a Lucas primality test (which requires coding a Lucas primality test,
 * or 3) use a larger M-R test count. I chose the latter. It increases the time
 * that it takes to prove the selected prime, but it shouldn't increase the
 * overall time to run the algorithm (non-primes should still faile M-R
 * realively quickly). If you want to get that last bit of performance,
 * implement Lucas and adjust these two functions.  See FIPS 186-3 Appendix C
 * and F for more information.
 */
static int
prime_testcount_p(int L, int N)
{
    switch (L) {
        case 1024:
            return 40;
        case 2048:
            return 56;
        case 3072:
            return 64;
        default:
            break;
    }
    return 50; /* L = 512-960 */
}

/* The q numbers are different if you run M-R followd by Lucas. I created
 * a separate function so if someone wanted to add the Lucas check, they
 * could do so fairly easily */
static int
prime_testcount_q(int L, int N)
{
    return prime_testcount_p(L, N);
}

/*
 * generic function to make sure our input matches DSA2 requirements
 * this gives us one place to go if we need to bump the requirements in the
 * future.
 */
static SECStatus
pqg_validate_dsa2(unsigned int L, unsigned int N)
{

    switch (L) {
        case 1024:
            if (N != DSA1_Q_BITS) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            break;
        case 2048:
            if ((N != 224) && (N != 256)) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            break;
        case 3072:
            if (N != 256) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                return SECFailure;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
    }
    return SECSuccess;
}

static unsigned int
pqg_get_default_N(unsigned int L)
{
    unsigned int N = 0;
    switch (L) {
        case 1024:
            N = DSA1_Q_BITS;
            break;
        case 2048:
            N = 224;
            break;
        case 3072:
            N = 256;
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            break; /* N already set to zero */
    }
    return N;
}

/*
 * Select the lowest hash algorithm usable
 */
static HASH_HashType
getFirstHash(unsigned int L, unsigned int N)
{
    if (N < 224) {
        return HASH_AlgSHA1;
    }
    if (N < 256) {
        return HASH_AlgSHA224;
    }
    if (N < 384) {
        return HASH_AlgSHA256;
    }
    if (N < 512) {
        return HASH_AlgSHA384;
    }
    return HASH_AlgSHA512;
}

/*
 * find the next usable hash algorthim
 */
static HASH_HashType
getNextHash(HASH_HashType hashtype)
{
    switch (hashtype) {
        case HASH_AlgSHA1:
            hashtype = HASH_AlgSHA224;
            break;
        case HASH_AlgSHA224:
            hashtype = HASH_AlgSHA256;
            break;
        case HASH_AlgSHA256:
            hashtype = HASH_AlgSHA384;
            break;
        case HASH_AlgSHA384:
            hashtype = HASH_AlgSHA512;
            break;
        case HASH_AlgSHA512:
        default:
            hashtype = HASH_AlgTOTAL;
            break;
    }
    return hashtype;
}

static unsigned int
HASH_ResultLen(HASH_HashType type)
{
    const SECHashObject *hash_obj = HASH_GetRawHashObject(type);
    PORT_Assert(hash_obj != NULL);
    if (hash_obj == NULL) {
        /* type is always a valid HashType. Thus a null hash_obj must be a bug */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return 0;
    }
    PORT_Assert(hash_obj->length != 0);
    return hash_obj->length;
}

static SECStatus
HASH_HashBuf(HASH_HashType type, unsigned char *dest,
             const unsigned char *src, PRUint32 src_len)
{
    const SECHashObject *hash_obj = HASH_GetRawHashObject(type);
    void *hashcx = NULL;
    unsigned int dummy;

    if (hash_obj == NULL) {
        return SECFailure;
    }

    hashcx = hash_obj->create();
    if (hashcx == NULL) {
        return SECFailure;
    }
    hash_obj->begin(hashcx);
    hash_obj->update(hashcx, src, src_len);
    hash_obj->end(hashcx, dest, &dummy, hash_obj->length);
    hash_obj->destroy(hashcx, PR_TRUE);
    return SECSuccess;
}

unsigned int
PQG_GetLength(const SECItem *obj)
{
    unsigned int len = obj->len;

    if (obj->data == NULL) {
        return 0;
    }
    if (len > 1 && obj->data[0] == 0) {
        len--;
    }
    return len;
}

SECStatus
PQG_Check(const PQGParams *params)
{
    unsigned int L, N;
    SECStatus rv = SECSuccess;

    if (params == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    L = PQG_GetLength(&params->prime) * PR_BITS_PER_BYTE;
    N = PQG_GetLength(&params->subPrime) * PR_BITS_PER_BYTE;

    if (L < 1024) {
        int j;

        /* handle DSA1 pqg parameters with less thatn 1024 bits*/
        if (N != DSA1_Q_BITS) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        j = PQG_PBITS_TO_INDEX(L);
        if (j < 0) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            rv = SECFailure;
        }
    } else {
        /* handle DSA2 parameters (includes DSA1, 1024 bits) */
        rv = pqg_validate_dsa2(L, N);
    }
    return rv;
}

HASH_HashType
PQG_GetHashType(const PQGParams *params)
{
    unsigned int L, N;

    if (params == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return HASH_AlgNULL;
    }

    L = PQG_GetLength(&params->prime) * PR_BITS_PER_BYTE;
    N = PQG_GetLength(&params->subPrime) * PR_BITS_PER_BYTE;
    return getFirstHash(L, N);
}

/* Get a seed for generating P and Q.  If in testing mode, copy in the
** seed from FIPS 186-1 appendix 5.  Otherwise, obtain bytes from the
** global random number generator.
*/
static SECStatus
getPQseed(SECItem *seed, PLArenaPool *arena)
{
    SECStatus rv;

    if (!seed->data) {
        seed->data = (unsigned char *)PORT_ArenaZAlloc(arena, seed->len);
    }
    if (!seed->data) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    rv = RNG_GenerateGlobalRandomBytes(seed->data, seed->len);
    /*
     * NIST CMVP disallows a sequence of 20 bytes with the most
     * significant byte equal to 0.  Perhaps they interpret
     * "a sequence of at least 160 bits" as "a number >= 2^159".
     * So we always set the most significant bit to 1. (bug 334533)
     */
    seed->data[0] |= 0x80;
    return rv;
}

/* Generate a candidate h value.  If in testing mode, use the h value
** specified in FIPS 186-1 appendix 5, h = 2.  Otherwise, obtain bytes
** from the global random number generator.
*/
static SECStatus
generate_h_candidate(SECItem *hit, mp_int *H)
{
    SECStatus rv = SECSuccess;
    mp_err err = MP_OKAY;
#ifdef FIPS_186_1_A5_TEST
    memset(hit->data, 0, hit->len);
    hit->data[hit->len - 1] = 0x02;
#else
    rv = RNG_GenerateGlobalRandomBytes(hit->data, hit->len);
#endif
    if (rv)
        return SECFailure;
    err = mp_read_unsigned_octets(H, hit->data, hit->len);
    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
addToSeed(const SECItem *seed,
          unsigned long addend,
          int seedlen, /* g in 186-1 */
          SECItem *seedout)
{
    mp_int s, sum, modulus, tmp;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    MP_DIGITS(&s) = 0;
    MP_DIGITS(&sum) = 0;
    MP_DIGITS(&modulus) = 0;
    MP_DIGITS(&tmp) = 0;
    CHECK_MPI_OK(mp_init(&s));
    CHECK_MPI_OK(mp_init(&sum));
    CHECK_MPI_OK(mp_init(&modulus));
    SECITEM_TO_MPINT(*seed, &s); /* s = seed */
    /* seed += addend */
    if (addend < MP_DIGIT_MAX) {
        CHECK_MPI_OK(mp_add_d(&s, (mp_digit)addend, &s));
    } else {
        CHECK_MPI_OK(mp_init(&tmp));
        CHECK_MPI_OK(mp_set_ulong(&tmp, addend));
        CHECK_MPI_OK(mp_add(&s, &tmp, &s));
    }
    /*sum = s mod 2**seedlen */
    CHECK_MPI_OK(mp_div_2d(&s, (mp_digit)seedlen, NULL, &sum));
    if (seedout->data != NULL) {
        SECITEM_ZfreeItem(seedout, PR_FALSE);
    }
    MPINT_TO_SECITEM(&sum, seedout, NULL);
cleanup:
    mp_clear(&s);
    mp_clear(&sum);
    mp_clear(&modulus);
    mp_clear(&tmp);
    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return rv;
}

/* Compute Hash[(SEED + addend) mod 2**g]
** Result is placed in shaOutBuf.
** This computation is used in steps 2 and 7 of FIPS 186 Appendix 2.2  and
** step 11.2 of FIPS 186-3 Appendix A.1.1.2 .
*/
static SECStatus
addToSeedThenHash(HASH_HashType hashtype,
                  const SECItem *seed,
                  unsigned long addend,
                  int seedlen, /* g in 186-1 */
                  unsigned char *hashOutBuf)
{
    SECItem str = { 0, 0, 0 };
    SECStatus rv;
    rv = addToSeed(seed, addend, seedlen, &str);
    if (rv != SECSuccess) {
        return rv;
    }
    rv = HASH_HashBuf(hashtype, hashOutBuf, str.data, str.len); /* hash result */
    if (str.data)
        SECITEM_ZfreeItem(&str, PR_FALSE);
    return rv;
}

/*
**  Perform steps 2 and 3 of FIPS 186-1, appendix 2.2.
**  Generate Q from seed.
*/
static SECStatus
makeQfromSeed(
    unsigned int g,      /* input.  Length of seed in bits. */
    const SECItem *seed, /* input.  */
    mp_int *Q)           /* output. */
{
    unsigned char sha1[SHA1_LENGTH];
    unsigned char sha2[SHA1_LENGTH];
    unsigned char U[SHA1_LENGTH];
    SECStatus rv = SECSuccess;
    mp_err err = MP_OKAY;
    int i;
    /* ******************************************************************
    ** Step 2.
    ** "Compute U = SHA[SEED] XOR SHA[(SEED+1) mod 2**g]."
    **/
    CHECK_SEC_OK(SHA1_HashBuf(sha1, seed->data, seed->len));
    CHECK_SEC_OK(addToSeedThenHash(HASH_AlgSHA1, seed, 1, g, sha2));
    for (i = 0; i < SHA1_LENGTH; ++i)
        U[i] = sha1[i] ^ sha2[i];
    /* ******************************************************************
    ** Step 3.
    ** "Form Q from U by setting the most signficant bit (the 2**159 bit)
    **  and the least signficant bit to 1.  In terms of boolean operations,
    **  Q = U OR 2**159 OR 1.  Note that 2**159 < Q < 2**160."
    */
    U[0] |= 0x80; /* U is MSB first */
    U[SHA1_LENGTH - 1] |= 0x01;
    err = mp_read_unsigned_octets(Q, U, SHA1_LENGTH);
cleanup:
    memset(U, 0, SHA1_LENGTH);
    memset(sha1, 0, SHA1_LENGTH);
    memset(sha2, 0, SHA1_LENGTH);
    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return rv;
}

/*
**  Perform steps 6 and 7 of FIPS 186-3, appendix A.1.1.2.
**  Generate Q from seed.
*/
static SECStatus
makeQ2fromSeed(
    HASH_HashType hashtype, /* selected Hashing algorithm */
    unsigned int N,         /* input.  Length of q in bits. */
    const SECItem *seed,    /* input.  */
    mp_int *Q)              /* output. */
{
    unsigned char U[HASH_LENGTH_MAX];
    SECStatus rv = SECSuccess;
    mp_err err = MP_OKAY;
    int N_bytes = N / PR_BITS_PER_BYTE; /* length of N in bytes rather than bits */
    int hashLen = HASH_ResultLen(hashtype);
    int offset = 0;

    /* ******************************************************************
    ** Step 6.
    ** "Compute U = hash[SEED] mod 2**N-1]."
    **/
    CHECK_SEC_OK(HASH_HashBuf(hashtype, U, seed->data, seed->len));
    /* mod 2**N . Step 7 will explicitly set the top bit to 1, so no need
     * to handle mod 2**N-1 */
    if (hashLen > N_bytes) {
        offset = hashLen - N_bytes;
    }
    /* ******************************************************************
    ** Step 7.
    ** computed_q = 2**(N-1) + U + 1 - (U mod 2)
    **
    ** This is the same as:
    ** computed_q = 2**(N-1) | U | 1;
    */
    U[offset] |= 0x80; /* U is MSB first */
    U[hashLen - 1] |= 0x01;
    err = mp_read_unsigned_octets(Q, &U[offset], N_bytes);
cleanup:
    memset(U, 0, HASH_LENGTH_MAX);
    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return rv;
}

/*
**  Perform steps from  FIPS 186-3, Appendix A.1.2.1 and Appendix C.6
**
**  This generates a provable prime from two smaller prime. The resulting
**  prime p will have q0 as a multiple of p-1. q0 can be 1.
**
** This implments steps 4 thorough 22 of FIPS 186-3 A.1.2.1 and
**                steps 16 through 34 of FIPS 186-2 C.6
*/
static SECStatus
makePrimefromPrimesShaweTaylor(
    HASH_HashType hashtype,          /* selected Hashing algorithm */
    unsigned int length,             /* input. Length of prime in bits. */
    unsigned int seedlen,            /* input seed length in bits */
    mp_int *c0,                      /* seed prime */
    mp_int *q,                       /* sub prime, can be 1 */
    mp_int *prime,                   /* output.  */
    SECItem *prime_seed,             /* input/output.  */
    unsigned int *prime_gen_counter) /* input/output.  */
{
    mp_int c;
    mp_int c0_2;
    mp_int t;
    mp_int a;
    mp_int z;
    mp_int two_length_minus_1;
    SECStatus rv = SECFailure;
    int hashlen = HASH_ResultLen(hashtype);
    int outlen = hashlen * PR_BITS_PER_BYTE;
    int offset;
    unsigned char bit, mask;
    /* x needs to hold roundup(L/outlen)*outlen.
     * This can be no larger than L+outlen-1, So we set it's size to
     * our max L + max outlen and know we are safe */
    unsigned char x[DSA_MAX_P_BITS / 8 + HASH_LENGTH_MAX];
    mp_err err = MP_OKAY;
    int i;
    int iterations;
    int old_counter;

    MP_DIGITS(&c) = 0;
    MP_DIGITS(&c0_2) = 0;
    MP_DIGITS(&t) = 0;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&z) = 0;
    MP_DIGITS(&two_length_minus_1) = 0;
    CHECK_MPI_OK(mp_init(&c));
    CHECK_MPI_OK(mp_init(&c0_2));
    CHECK_MPI_OK(mp_init(&t));
    CHECK_MPI_OK(mp_init(&a));
    CHECK_MPI_OK(mp_init(&z));
    CHECK_MPI_OK(mp_init(&two_length_minus_1));

    /*
    ** There is a slight mapping of variable names depending on which
    ** FIPS 186 steps are being carried out. The mapping is as follows:
    **  variable          A.1.2.1           C.6
    **    c0                p0               c0
    **    q                 q                1
    **    c                 p                c
    **    c0_2            2*p0*q            2*c0
    **    length            L               length
    **    prime_seed       pseed            prime_seed
    **  prime_gen_counter pgen_counter     prime_gen_counter
    **
    ** Also note: or iterations variable is actually iterations+1, since
    ** iterations+1 works better in C.
    */

    /* Step 4/16 iterations = ceiling(length/outlen)-1 */
    iterations = (length + outlen - 1) / outlen; /* NOTE: iterations +1 */
    /* Step 5/17 old_counter = prime_gen_counter */
    old_counter = *prime_gen_counter;
    /*
    ** Comment: Generate a pseudorandom integer x in the interval
    ** [2**(length-1), 2**length].
    **
    ** Step 6/18 x = 0
    */
    PORT_Memset(x, 0, sizeof(x));
    /*
    ** Step 7/19 for i = 0 to iterations do
    **  x = x + (HASH(prime_seed + i) * 2^(i*outlen))
    */
    for (i = 0; i < iterations; i++) {
        /* is bigger than prime_seed should get to */
        CHECK_SEC_OK(addToSeedThenHash(hashtype, prime_seed, i,
                                       seedlen, &x[(iterations - i - 1) * hashlen]));
    }
    /* Step 8/20 prime_seed = prime_seed + iterations + 1 */
    CHECK_SEC_OK(addToSeed(prime_seed, iterations, seedlen, prime_seed));
    /*
    ** Step 9/21 x = 2 ** (length-1) + x mod 2 ** (length-1)
    **
    **   This step mathematically sets the high bit and clears out
    **  all the other bits higher than length. 'x' is stored
    **  in the x array, MSB first. The above formula gives us an 'x'
    **  which is length bytes long and has the high bit set. We also know
    **  that length <= iterations*outlen since
    **  iterations=ceiling(length/outlen). First we find the offset in
    **  bytes into the array where the high bit is.
    */
    offset = (outlen * iterations - length) / PR_BITS_PER_BYTE;
    /* now we want to set the 'high bit', since length may not be a
     * multiple of 8,*/
    bit = 1 << ((length - 1) & 0x7); /* select the proper bit in the byte */
    /* we need to zero out the rest of the bits in the byte above */
    mask = (bit - 1);
    /* now we set it */
    x[offset] = (mask & x[offset]) | bit;
    /*
    ** Comment: Generate a candidate prime c in the interval
    ** [2**(length-1), 2**length].
    **
    ** Step 10 t = ceiling(x/(2q(p0)))
    ** Step 22 t = ceiling(x/(2(c0)))
    */
    CHECK_MPI_OK(mp_read_unsigned_octets(&t, &x[offset],
                                         hashlen * iterations - offset)); /* t = x */
    CHECK_MPI_OK(mp_mul(c0, q, &c0_2));                                   /* c0_2 is now c0*q */
    CHECK_MPI_OK(mp_add(&c0_2, &c0_2, &c0_2));                            /* c0_2 is now 2*q*c0 */
    CHECK_MPI_OK(mp_add(&t, &c0_2, &t));                                  /* t = x+2*q*c0 */
    CHECK_MPI_OK(mp_sub_d(&t, (mp_digit)1, &t));                          /* t = x+2*q*c0 -1 */
    /* t = floor((x+2qc0-1)/2qc0) = ceil(x/2qc0) */
    CHECK_MPI_OK(mp_div(&t, &c0_2, &t, NULL));
    /*
    ** step 11: if (2tqp0 +1 > 2**length), then t = ceiling(2**(length-1)/2qp0)
    ** step 12: t = 2tqp0 +1.
    **
    ** step 23: if (2tc0 +1 > 2**length), then t = ceiling(2**(length-1)/2c0)
    ** step 24: t = 2tc0 +1.
    */
    CHECK_MPI_OK(mp_2expt(&two_length_minus_1, length - 1));
step_23:
    CHECK_MPI_OK(mp_mul(&t, &c0_2, &c));                /* c = t*2qc0 */
    CHECK_MPI_OK(mp_add_d(&c, (mp_digit)1, &c));        /* c= 2tqc0 + 1*/
    if (mpl_significant_bits(&c) > length) {            /* if c > 2**length */
        CHECK_MPI_OK(mp_sub_d(&c0_2, (mp_digit)1, &t)); /* t = 2qc0-1 */
        /* t = 2**(length-1) + 2qc0 -1 */
        CHECK_MPI_OK(mp_add(&two_length_minus_1, &t, &t));
        /* t = floor((2**(length-1)+2qc0 -1)/2qco)
         *   = ceil(2**(length-2)/2qc0) */
        CHECK_MPI_OK(mp_div(&t, &c0_2, &t, NULL));
        CHECK_MPI_OK(mp_mul(&t, &c0_2, &c));
        CHECK_MPI_OK(mp_add_d(&c, (mp_digit)1, &c)); /* c= 2tqc0 + 1*/
    }
    /* Step 13/25 prime_gen_counter = prime_gen_counter + 1*/
    (*prime_gen_counter)++;
    /*
    ** Comment: Test the candidate prime c for primality; first pick an
    ** integer a between 2 and c-2.
    **
    ** Step 14/26 a=0
    */
    PORT_Memset(x, 0, sizeof(x)); /* use x for a */
    /*
    ** Step 15/27 for i = 0 to iterations do
    **  a = a + (HASH(prime_seed + i) * 2^(i*outlen))
    **
    ** NOTE: we reuse the x array for 'a' initially.
    */
    for (i = 0; i < iterations; i++) {
        CHECK_SEC_OK(addToSeedThenHash(hashtype, prime_seed, i,
                                       seedlen, &x[(iterations - i - 1) * hashlen]));
    }
    /* Step 16/28 prime_seed = prime_seed + iterations + 1 */
    CHECK_SEC_OK(addToSeed(prime_seed, iterations, seedlen, prime_seed));
    /* Step 17/29 a = 2 + (a mod (c-3)). */
    CHECK_MPI_OK(mp_read_unsigned_octets(&a, x, iterations * hashlen));
    CHECK_MPI_OK(mp_sub_d(&c, (mp_digit)3, &z)); /* z = c -3 */
    CHECK_MPI_OK(mp_mod(&a, &z, &a));            /* a = a mod c -3 */
    CHECK_MPI_OK(mp_add_d(&a, (mp_digit)2, &a)); /* a = 2 + a mod c -3 */
    /*
    ** Step 18 z = a**(2tq) mod p.
    ** Step 30 z = a**(2t) mod c.
    */
    CHECK_MPI_OK(mp_mul(&t, q, &z));          /* z = tq */
    CHECK_MPI_OK(mp_add(&z, &z, &z));         /* z = 2tq */
    CHECK_MPI_OK(mp_exptmod(&a, &z, &c, &z)); /* z = a**(2tq) mod c */
    /*
    ** Step 19 if (( 1 == GCD(z-1,p)) and ( 1 == z**p0 mod p )), then
    ** Step 31 if (( 1 == GCD(z-1,c)) and ( 1 == z**c0 mod c )), then
    */
    CHECK_MPI_OK(mp_sub_d(&z, (mp_digit)1, &a));
    CHECK_MPI_OK(mp_gcd(&a, &c, &a));
    if (mp_cmp_d(&a, (mp_digit)1) == 0) {
        CHECK_MPI_OK(mp_exptmod(&z, c0, &c, &a));
        if (mp_cmp_d(&a, (mp_digit)1) == 0) {
            /* Step 31.1 prime = c */
            CHECK_MPI_OK(mp_copy(&c, prime));
            /*
        ** Step 31.2 return Success, prime, prime_seed,
        **    prime_gen_counter
        */
            rv = SECSuccess;
            goto cleanup;
        }
    }
    /*
    ** Step 20/32 If (prime_gen_counter > 4 * length + old_counter then
    **   return (FAILURE, 0, 0, 0).
    ** NOTE: the test is reversed, so we fall through on failure to the
    ** cleanup routine
    */
    if (*prime_gen_counter < (4 * length + old_counter)) {
        /* Step 21/33 t = t + 1 */
        CHECK_MPI_OK(mp_add_d(&t, (mp_digit)1, &t));
        /* Step 22/34 Go to step 23/11 */
        goto step_23;
    }

    /* if (prime_gencont > (4*length + old_counter), fall through to failure */
    rv = SECFailure; /* really is already set, but paranoia is good */

cleanup:
    mp_clear(&c);
    mp_clear(&c0_2);
    mp_clear(&t);
    mp_clear(&a);
    mp_clear(&z);
    mp_clear(&two_length_minus_1);
    PORT_Memset(x, 0, sizeof(x));
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    if (rv == SECFailure) {
        mp_zero(prime);
        if (prime_seed->data) {
            SECITEM_FreeItem(prime_seed, PR_FALSE);
        }
        *prime_gen_counter = 0;
    }
    return rv;
}

/*
**  Perform steps from  FIPS 186-3, Appendix C.6
**
**  This generates a provable prime from a seed
*/
static SECStatus
makePrimefromSeedShaweTaylor(
    HASH_HashType hashtype,          /* selected Hashing algorithm */
    unsigned int length,             /* input.  Length of prime in bits. */
    const SECItem *input_seed,       /* input.  */
    mp_int *prime,                   /* output.  */
    SECItem *prime_seed,             /* output.  */
    unsigned int *prime_gen_counter) /* output.  */
{
    mp_int c;
    mp_int c0;
    mp_int one;
    SECStatus rv = SECFailure;
    int hashlen = HASH_ResultLen(hashtype);
    int outlen = hashlen * PR_BITS_PER_BYTE;
    int offset;
    int seedlen = input_seed->len * 8; /*seedlen is in bits */
    unsigned char bit, mask;
    unsigned char x[HASH_LENGTH_MAX * 2];
    mp_digit dummy;
    mp_err err = MP_OKAY;
    int i;

    MP_DIGITS(&c) = 0;
    MP_DIGITS(&c0) = 0;
    MP_DIGITS(&one) = 0;
    CHECK_MPI_OK(mp_init(&c));
    CHECK_MPI_OK(mp_init(&c0));
    CHECK_MPI_OK(mp_init(&one));

    /* Step 1. if length < 2 then return (FAILURE, 0, 0, 0) */
    if (length < 2) {
        rv = SECFailure;
        goto cleanup;
    }
    /* Step 2. if length >= 33 then goto step 14 */
    if (length >= 33) {
        mp_zero(&one);
        CHECK_MPI_OK(mp_add_d(&one, (mp_digit)1, &one));

        /* Step 14 (status, c0, prime_seed, prime_gen_counter) =
    ** (ST_Random_Prime((ceil(length/2)+1, input_seed)
    */
        rv = makePrimefromSeedShaweTaylor(hashtype, (length + 1) / 2 + 1,
                                          input_seed, &c0, prime_seed, prime_gen_counter);
        /* Step 15 if FAILURE is returned, return (FAILURE, 0, 0, 0). */
        if (rv != SECSuccess) {
            goto cleanup;
        }
        /* Steps 16-34 */
        rv = makePrimefromPrimesShaweTaylor(hashtype, length, seedlen, &c0, &one,
                                            prime, prime_seed, prime_gen_counter);
        goto cleanup; /* we're done, one way or the other */
    }
    /* Step 3 prime_seed = input_seed */
    CHECK_SEC_OK(SECITEM_CopyItem(NULL, prime_seed, input_seed));
    /* Step 4 prime_gen_count = 0 */
    *prime_gen_counter = 0;

step_5:
    /* Step 5 c = Hash(prime_seed) xor Hash(prime_seed+1). */
    CHECK_SEC_OK(HASH_HashBuf(hashtype, x, prime_seed->data, prime_seed->len));
    CHECK_SEC_OK(addToSeedThenHash(hashtype, prime_seed, 1, seedlen, &x[hashlen]));
    for (i = 0; i < hashlen; i++) {
        x[i] = x[i] ^ x[i + hashlen];
    }
    /* Step 6 c = 2**length-1 + c mod 2**length-1 */
    /*   This step mathematically sets the high bit and clears out
    **  all the other bits higher than length. Right now c is stored
    **  in the x array, MSB first. The above formula gives us a c which
    **  is length bytes long and has the high bit set. We also know that
    **  length < outlen since the smallest outlen is 160 bits and the largest
    **  length at this point is 32 bits. So first we find the offset in bytes
    **  into the array where the high bit is.
    */
    offset = (outlen - length) / PR_BITS_PER_BYTE;
    /* now we want to set the 'high bit'. We have to calculate this since
     * length may not be a multiple of 8.*/
    bit = 1 << ((length - 1) & 0x7); /* select the proper bit in the byte */
    /* we need to zero out the rest of the bits  in the byte above */
    mask = (bit - 1);
    /* now we set it */
    x[offset] = (mask & x[offset]) | bit;
    /* Step 7 c = c*floor(c/2) + 1 */
    /* set the low bit. much easier to find (the end of the array) */
    x[hashlen - 1] |= 1;
    /* now that we've set our bits, we can create our candidate "c" */
    CHECK_MPI_OK(mp_read_unsigned_octets(&c, &x[offset], hashlen - offset));
    /* Step 8 prime_gen_counter = prime_gen_counter + 1 */
    (*prime_gen_counter)++;
    /* Step 9 prime_seed = prime_seed + 2 */
    CHECK_SEC_OK(addToSeed(prime_seed, 2, seedlen, prime_seed));
    /* Step 10 Perform deterministic primality test on c. For example, since
    ** c is small, it's primality can be tested by trial division, See
    ** See Appendic C.7.
    **
    ** We in fact test with trial division. mpi has a built int trial divider
    ** that divides all divisors up to 2^16.
    */
    if (prime_tab[prime_tab_size - 1] < 0xFFF1) {
        /* we aren't testing all the primes between 0 and 2^16, we really
     * can't use this construction. Just fail. */
        rv = SECFailure;
        goto cleanup;
    }
    dummy = prime_tab_size;
    err = mpp_divis_primes(&c, &dummy);
    /* Step 11 if c is prime then */
    if (err == MP_NO) {
        /* Step 11.1 prime = c */
        CHECK_MPI_OK(mp_copy(&c, prime));
        /* Step 11.2 return SUCCESS prime, prime_seed, prime_gen_counter */
        err = MP_OKAY;
        rv = SECSuccess;
        goto cleanup;
    } else if (err != MP_YES) {
        goto cleanup; /* function failed, bail out */
    } else {
        /* reset mp_err */
        err = MP_OKAY;
    }
    /*
    ** Step 12 if (prime_gen_counter > (4*len))
    ** then return (FAILURE, 0, 0, 0))
    ** Step 13 goto step 5
    */
    if (*prime_gen_counter <= (4 * length)) {
        goto step_5;
    }
    /* if (prime_gencont > 4*length), fall through to failure */
    rv = SECFailure; /* really is already set, but paranoia is good */

cleanup:
    mp_clear(&c);
    mp_clear(&c0);
    mp_clear(&one);
    PORT_Memset(x, 0, sizeof(x));
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    if (rv == SECFailure) {
        mp_zero(prime);
        if (prime_seed->data) {
            SECITEM_FreeItem(prime_seed, PR_FALSE);
        }
        *prime_gen_counter = 0;
    }
    return rv;
}

/*
 * Find a Q and algorithm from Seed.
 */
static SECStatus
findQfromSeed(
    unsigned int L,             /* input.  Length of p in bits. */
    unsigned int N,             /* input.  Length of q in bits. */
    unsigned int g,             /* input.  Length of seed in bits. */
    const SECItem *seed,        /* input.  */
    mp_int *Q,                  /* input. */
    mp_int *Q_,                 /* output. */
    unsigned int *qseed_len,    /* output */
    HASH_HashType *hashtypePtr, /* output. Hash uses */
    pqgGenType *typePtr,        /* output. Generation Type used */
    unsigned int *qgen_counter) /* output. q_counter */
{
    HASH_HashType hashtype;
    SECItem firstseed = { 0, 0, 0 };
    SECItem qseed = { 0, 0, 0 };
    SECStatus rv;

    *qseed_len = 0; /* only set if FIPS186_3_ST_TYPE */

    /* handle legacy small DSA first can only be FIPS186_1_TYPE */
    if (L < 1024) {
        rv = makeQfromSeed(g, seed, Q_);
        if ((rv == SECSuccess) && (mp_cmp(Q, Q_) == 0)) {
            *hashtypePtr = HASH_AlgSHA1;
            *typePtr = FIPS186_1_TYPE;
            return SECSuccess;
        }
        return SECFailure;
    }
    /* 1024 could use FIPS186_1 or FIPS186_3 algorithms, we need to try
     * them both */
    if (L == 1024) {
        rv = makeQfromSeed(g, seed, Q_);
        if (rv == SECSuccess) {
            if (mp_cmp(Q, Q_) == 0) {
                *hashtypePtr = HASH_AlgSHA1;
                *typePtr = FIPS186_1_TYPE;
                return SECSuccess;
            }
        }
        /* fall through for FIPS186_3 types */
    }
    /* at this point we know we aren't using FIPS186_1, start trying FIPS186_3
     * with appropriate hash types */
    for (hashtype = getFirstHash(L, N); hashtype != HASH_AlgTOTAL;
         hashtype = getNextHash(hashtype)) {
        rv = makeQ2fromSeed(hashtype, N, seed, Q_);
        if (rv != SECSuccess) {
            continue;
        }
        if (mp_cmp(Q, Q_) == 0) {
            *hashtypePtr = hashtype;
            *typePtr = FIPS186_3_TYPE;
            return SECSuccess;
        }
    }
    /*
     * OK finally try FIPS186_3 Shawe-Taylor
     */
    firstseed = *seed;
    firstseed.len = seed->len / 3;
    for (hashtype = getFirstHash(L, N); hashtype != HASH_AlgTOTAL;
         hashtype = getNextHash(hashtype)) {
        unsigned int count;

        rv = makePrimefromSeedShaweTaylor(hashtype, N, &firstseed, Q_,
                                          &qseed, &count);
        if (rv != SECSuccess) {
            continue;
        }
        if (mp_cmp(Q, Q_) == 0) {
            /* check qseed as well... */
            int offset = seed->len - qseed.len;
            if ((offset < 0) ||
                (PORT_Memcmp(&seed->data[offset], qseed.data, qseed.len) != 0)) {
                /* we found q, but the seeds don't match. This isn't an
         * accident, someone has been tweeking with the seeds, just
         * fail a this point. */
                SECITEM_FreeItem(&qseed, PR_FALSE);
                return SECFailure;
            }
            *qseed_len = qseed.len;
            *hashtypePtr = hashtype;
            *typePtr = FIPS186_3_ST_TYPE;
            *qgen_counter = count;
            SECITEM_FreeItem(&qseed, PR_FALSE);
            return SECSuccess;
        }
        SECITEM_FreeItem(&qseed, PR_FALSE);
    }
    /* no hash algorithms found which match seed to Q, fail */
    return SECFailure;
}

/*
**  Perform steps 7, 8 and 9 of FIPS 186, appendix 2.2.
**  which are the same as steps 11.1-11.5 of FIPS 186-2, App A.1.1.2
**  Generate P from Q, seed, L, and offset.
*/
static SECStatus
makePfromQandSeed(
    HASH_HashType hashtype, /* selected Hashing algorithm */
    unsigned int L,         /* Length of P in bits.  Per FIPS 186. */
    unsigned int N,         /* Length of Q in bits.  Per FIPS 186. */
    unsigned int offset,    /* Per FIPS 186, App 2.2. & 186-3 App A.1.1.2 */
    unsigned int seedlen,   /* input. Length of seed in bits. (g in 186-1)*/
    const SECItem *seed,    /* input.  */
    const mp_int *Q,        /* input.  */
    mp_int *P)              /* output. */
{
    unsigned int j;       /* Per FIPS 186-3 App. A.1.1.2  (k in 186-1)*/
    unsigned int n;       /* Per FIPS 186, appendix 2.2. */
    mp_digit b;           /* Per FIPS 186, appendix 2.2. */
    unsigned int outlen;  /* Per FIPS 186-3 App. A.1.1.2 */
    unsigned int hashlen; /* outlen in bytes */
    unsigned char V_j[HASH_LENGTH_MAX];
    mp_int W, X, c, twoQ, V_n, tmp;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    /* Initialize bignums */
    MP_DIGITS(&W) = 0;
    MP_DIGITS(&X) = 0;
    MP_DIGITS(&c) = 0;
    MP_DIGITS(&twoQ) = 0;
    MP_DIGITS(&V_n) = 0;
    MP_DIGITS(&tmp) = 0;
    CHECK_MPI_OK(mp_init(&W));
    CHECK_MPI_OK(mp_init(&X));
    CHECK_MPI_OK(mp_init(&c));
    CHECK_MPI_OK(mp_init(&twoQ));
    CHECK_MPI_OK(mp_init(&tmp));
    CHECK_MPI_OK(mp_init(&V_n));

    hashlen = HASH_ResultLen(hashtype);
    outlen = hashlen * PR_BITS_PER_BYTE;

    /* L - 1 = n*outlen + b */
    n = (L - 1) / outlen;
    b = (L - 1) % outlen;

    /* ******************************************************************
    ** Step 11.1 (Step 7 in 186-1)
    **  "for j = 0 ... n let
    **           V_j = SHA[(SEED + offset + j) mod 2**seedlen]."
    **
    ** Step 11.2 (Step 8 in 186-1)
    **   "W = V_0 + (V_1 * 2**outlen) + ... + (V_n-1 * 2**((n-1)*outlen))
    **         + ((V_n mod 2**b) * 2**(n*outlen))
    */
    for (j = 0; j < n; ++j) { /* Do the first n terms of V_j */
        /* Do step 11.1 for iteration j.
    ** V_j = HASH[(seed + offset + j) mod 2**g]
    */
        CHECK_SEC_OK(addToSeedThenHash(hashtype, seed, offset + j, seedlen, V_j));
        /* Do step 11.2 for iteration j.
    ** W += V_j * 2**(j*outlen)
    */
        OCTETS_TO_MPINT(V_j, &tmp, hashlen);           /* get bignum V_j     */
        CHECK_MPI_OK(mpl_lsh(&tmp, &tmp, j * outlen)); /* tmp=V_j << j*outlen */
        CHECK_MPI_OK(mp_add(&W, &tmp, &W));            /* W += tmp           */
    }
    /* Step 11.2, continued.
    **   [W += ((V_n mod 2**b) * 2**(n*outlen))]
    */
    CHECK_SEC_OK(addToSeedThenHash(hashtype, seed, offset + n, seedlen, V_j));
    OCTETS_TO_MPINT(V_j, &V_n, hashlen);           /* get bignum V_n     */
    CHECK_MPI_OK(mp_div_2d(&V_n, b, NULL, &tmp));  /* tmp = V_n mod 2**b */
    CHECK_MPI_OK(mpl_lsh(&tmp, &tmp, n * outlen)); /* tmp = tmp << n*outlen */
    CHECK_MPI_OK(mp_add(&W, &tmp, &W));            /* W += tmp           */
    /* Step 11.3, (Step 8 in 186-1)
    ** "X = W + 2**(L-1).
    **  Note that 0 <= W < 2**(L-1) and hence 2**(L-1) <= X < 2**L."
    */
    CHECK_MPI_OK(mpl_set_bit(&X, (mp_size)(L - 1), 1)); /* X = 2**(L-1) */
    CHECK_MPI_OK(mp_add(&X, &W, &X));                   /* X += W       */
    /*************************************************************
    ** Step 11.4. (Step 9 in 186-1)
    ** "c = X mod 2q"
    */
    CHECK_MPI_OK(mp_mul_2(Q, &twoQ));    /* 2q           */
    CHECK_MPI_OK(mp_mod(&X, &twoQ, &c)); /* c = X mod 2q */
    /*************************************************************
    ** Step 11.5. (Step 9 in 186-1)
    ** "p = X - (c - 1).
    **  Note that p is congruent to 1 mod 2q."
    */
    CHECK_MPI_OK(mp_sub_d(&c, 1, &c)); /* c -= 1       */
    CHECK_MPI_OK(mp_sub(&X, &c, P));   /* P = X - c    */
cleanup:
    mp_clear(&W);
    mp_clear(&X);
    mp_clear(&c);
    mp_clear(&twoQ);
    mp_clear(&V_n);
    mp_clear(&tmp);
    if (err) {
        MP_TO_SEC_ERROR(err);
        return SECFailure;
    }
    return rv;
}

/*
** Generate G from h, P, and Q.
*/
static SECStatus
makeGfromH(const mp_int *P, /* input.  */
           const mp_int *Q, /* input.  */
           mp_int *H,       /* input and output. */
           mp_int *G,       /* output. */
           PRBool *passed)
{
    mp_int exp, pm1;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    *passed = PR_FALSE;
    MP_DIGITS(&exp) = 0;
    MP_DIGITS(&pm1) = 0;
    CHECK_MPI_OK(mp_init(&exp));
    CHECK_MPI_OK(mp_init(&pm1));
    CHECK_MPI_OK(mp_sub_d(P, 1, &pm1));   /* P - 1            */
    if (mp_cmp(H, &pm1) >= 0)             /* H >= P-1         */
        CHECK_MPI_OK(mp_sub(H, &pm1, H)); /* H = H mod (P-1)  */
    /* Let b = 2**n (smallest power of 2 greater than P).
    ** Since P-1 >= b/2, and H < b, quotient(H/(P-1)) = 0 or 1
    ** so the above operation safely computes H mod (P-1)
    */
    /* Check for H = to 0 or 1.  Regen H if so.  (Regen means return error). */
    if (mp_cmp_d(H, 1) <= 0) {
        rv = SECFailure;
        goto cleanup;
    }
    /* Compute G, according to the equation  G = (H ** ((P-1)/Q)) mod P */
    CHECK_MPI_OK(mp_div(&pm1, Q, &exp, NULL)); /* exp = (P-1)/Q      */
    CHECK_MPI_OK(mp_exptmod(H, &exp, P, G));   /* G = H ** exp mod P */
    /* Check for G == 0 or G == 1, return error if so. */
    if (mp_cmp_d(G, 1) <= 0) {
        rv = SECFailure;
        goto cleanup;
    }
    *passed = PR_TRUE;
cleanup:
    mp_clear(&exp);
    mp_clear(&pm1);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/*
** Generate G from seed, index, P, and Q.
*/
static SECStatus
makeGfromIndex(HASH_HashType hashtype,
               const mp_int *P,     /* input.  */
               const mp_int *Q,     /* input.  */
               const SECItem *seed, /* input. */
               unsigned char index, /* input. */
               mp_int *G)           /* input/output */
{
    mp_int e, pm1, W;
    unsigned int count;
    unsigned char data[HASH_LENGTH_MAX];
    unsigned int len;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    const SECHashObject *hashobj = NULL;
    void *hashcx = NULL;

    MP_DIGITS(&e) = 0;
    MP_DIGITS(&pm1) = 0;
    MP_DIGITS(&W) = 0;
    CHECK_MPI_OK(mp_init(&e));
    CHECK_MPI_OK(mp_init(&pm1));
    CHECK_MPI_OK(mp_init(&W));

    /* initialize our hash stuff */
    hashobj = HASH_GetRawHashObject(hashtype);
    if (hashobj == NULL) {
        /* shouldn't happen */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        rv = SECFailure;
        goto cleanup;
    }
    hashcx = hashobj->create();
    if (hashcx == NULL) {
        rv = SECFailure;
        goto cleanup;
    }

    CHECK_MPI_OK(mp_sub_d(P, 1, &pm1)); /* P - 1            */
    /* Step 3 e = (p-1)/q */
    CHECK_MPI_OK(mp_div(&pm1, Q, &e, NULL)); /* e = (P-1)/Q      */
/* Steps 4, 5, and 6 */
/* count is a 16 bit value in the spec. We actually represent count
     * as more than 16 bits so we can easily detect the 16 bit overflow */
#define MAX_COUNT 0x10000
    for (count = 1; count < MAX_COUNT; count++) {
        /* step 7
         * U = domain_param_seed || "ggen" || index || count
             * step 8
         * W = HASH(U)
         */
        hashobj->begin(hashcx);
        hashobj->update(hashcx, seed->data, seed->len);
        hashobj->update(hashcx, (unsigned char *)"ggen", 4);
        hashobj->update(hashcx, &index, 1);
        data[0] = (count >> 8) & 0xff;
        data[1] = count & 0xff;
        hashobj->update(hashcx, data, 2);
        hashobj->end(hashcx, data, &len, sizeof(data));
        OCTETS_TO_MPINT(data, &W, len);
        /* step 9. g = W**e mod p */
        CHECK_MPI_OK(mp_exptmod(&W, &e, P, G));
        /* step 10. if (g < 2) then goto step 5 */
        /* NOTE: this weird construct is to keep the flow according to the spec.
     * the continue puts us back to step 5 of the for loop */
        if (mp_cmp_d(G, 2) < 0) {
            continue;
        }
        break; /* step 11 follows step 10 if the test condition is false */
    }
    if (count >= MAX_COUNT) {
        rv = SECFailure; /* last part of step 6 */
    }
/* step 11.
     * return valid G */
cleanup:
    PORT_Memset(data, 0, sizeof(data));
    if (hashcx) {
        hashobj->destroy(hashcx, PR_TRUE);
    }
    mp_clear(&e);
    mp_clear(&pm1);
    mp_clear(&W);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/* This code uses labels and gotos, so that it can follow the numbered
** steps in the algorithms from FIPS 186-3 appendix A.1.1.2 very closely,
** and so that the correctness of this code can be easily verified.
** So, please forgive the ugly c code.
**/
static SECStatus
pqg_ParamGen(unsigned int L, unsigned int N, pqgGenType type,
             unsigned int seedBytes, PQGParams **pParams, PQGVerify **pVfy)
{
    unsigned int n;       /* Per FIPS 186, app 2.2. 186-3 app A.1.1.2 */
    unsigned int seedlen; /* Per FIPS 186-3 app A.1.1.2  (was 'g' 186-1)*/
    unsigned int counter; /* Per FIPS 186, app 2.2. 186-3 app A.1.1.2 */
    unsigned int offset;  /* Per FIPS 186, app 2.2. 186-3 app A.1.1.2 */
    unsigned int outlen;  /* Per FIPS 186-3, appendix A.1.1.2. */
    unsigned int maxCount;
    HASH_HashType hashtype;
    SECItem *seed; /* Per FIPS 186, app 2.2. 186-3 app A.1.1.2 */
    PLArenaPool *arena = NULL;
    PQGParams *params = NULL;
    PQGVerify *verify = NULL;
    PRBool passed;
    SECItem hit = { 0, 0, 0 };
    SECItem firstseed = { 0, 0, 0 };
    SECItem qseed = { 0, 0, 0 };
    SECItem pseed = { 0, 0, 0 };
    mp_int P, Q, G, H, l, p0;
    mp_err err = MP_OKAY;
    SECStatus rv = SECFailure;
    int iterations = 0;

    /* Step 1. L and N already checked by caller*/
    /* Step 2. if (seedlen < N) return INVALID; */
    if (seedBytes < N / PR_BITS_PER_BYTE || !pParams || !pVfy) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Initialize bignums */
    MP_DIGITS(&P) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&G) = 0;
    MP_DIGITS(&H) = 0;
    MP_DIGITS(&l) = 0;
    MP_DIGITS(&p0) = 0;
    CHECK_MPI_OK(mp_init(&P));
    CHECK_MPI_OK(mp_init(&Q));
    CHECK_MPI_OK(mp_init(&G));
    CHECK_MPI_OK(mp_init(&H));
    CHECK_MPI_OK(mp_init(&l));
    CHECK_MPI_OK(mp_init(&p0));

    /* parameters have been passed in, only generate G */
    if (*pParams != NULL) {
        /* we only support G index generation if generating separate from PQ */
        if ((*pVfy == NULL) || (type == FIPS186_1_TYPE) ||
            ((*pVfy)->h.len != 1) || ((*pVfy)->h.data == NULL) ||
            ((*pVfy)->seed.data == NULL) || ((*pVfy)->seed.len == 0)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        params = *pParams;
        verify = *pVfy;

        /* fill in P Q,  */
        SECITEM_TO_MPINT((*pParams)->prime, &P);
        SECITEM_TO_MPINT((*pParams)->subPrime, &Q);
        hashtype = getFirstHash(L, N);
        CHECK_SEC_OK(makeGfromIndex(hashtype, &P, &Q, &(*pVfy)->seed,
                                    (*pVfy)->h.data[0], &G));
        MPINT_TO_SECITEM(&G, &(*pParams)->base, (*pParams)->arena);
        goto cleanup;
    }
    /* Initialize an arena for the params. */
    arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
    if (!arena) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }
    params = (PQGParams *)PORT_ArenaZAlloc(arena, sizeof(PQGParams));
    if (!params) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        PORT_FreeArena(arena, PR_TRUE);
        return SECFailure;
    }
    params->arena = arena;
    /* Initialize an arena for the verify. */
    arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
    if (!arena) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        PORT_FreeArena(params->arena, PR_TRUE);
        return SECFailure;
    }
    verify = (PQGVerify *)PORT_ArenaZAlloc(arena, sizeof(PQGVerify));
    if (!verify) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        PORT_FreeArena(arena, PR_TRUE);
        PORT_FreeArena(params->arena, PR_TRUE);
        return SECFailure;
    }
    verify->arena = arena;
    seed = &verify->seed;
    arena = NULL;

    /* Select Hash and Compute lengths. */
    /* getFirstHash gives us the smallest acceptable hash for this key
     * strength */
    hashtype = getFirstHash(L, N);
    outlen = HASH_ResultLen(hashtype) * PR_BITS_PER_BYTE;

    /* Step 3: n = Ceil(L/outlen)-1; (same as n = Floor((L-1)/outlen)) */
    n = (L - 1) / outlen;
    /* Step 4: (skipped since we don't use b): b = L -1 - (n*outlen); */
    seedlen = seedBytes * PR_BITS_PER_BYTE; /* bits in seed */
step_5:
    /* ******************************************************************
    ** Step 5. (Step 1 in 186-1)
    ** "Choose an abitrary sequence of at least N bits and call it SEED.
    **  Let g be the length of SEED in bits."
    */
    if (++iterations > MAX_ITERATIONS) { /* give up after a while */
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        goto cleanup;
    }
    seed->len = seedBytes;
    CHECK_SEC_OK(getPQseed(seed, verify->arena));
    /* ******************************************************************
    ** Step 6. (Step 2 in 186-1)
    **
    ** "Compute U = SHA[SEED] XOR SHA[(SEED+1) mod 2**g].  (186-1)"
    ** "Compute U = HASH[SEED] 2**(N-1).  (186-3)"
    **
    ** Step 7. (Step 3 in 186-1)
    ** "Form Q from U by setting the most signficant bit (the 2**159 bit)
    **  and the least signficant bit to 1.  In terms of boolean operations,
    **  Q = U OR 2**159 OR 1.  Note that 2**159 < Q < 2**160. (186-1)"
    **
    ** "q = 2**(N-1) + U + 1 - (U mod 2) (186-3)
    **
    ** Note: Both formulations are the same for U < 2**(N-1) and N=160
    **
    ** If using Shawe-Taylor, We do the entire A.1.2.1.2 setps in the block
    ** FIPS186_3_ST_TYPE.
    */
    if (type == FIPS186_1_TYPE) {
        CHECK_SEC_OK(makeQfromSeed(seedlen, seed, &Q));
    } else if (type == FIPS186_3_TYPE) {
        CHECK_SEC_OK(makeQ2fromSeed(hashtype, N, seed, &Q));
    } else {
        /* FIPS186_3_ST_TYPE */
        unsigned int qgen_counter, pgen_counter;

        /* Step 1 (L,N) already checked for acceptability */

        firstseed = *seed;
        qgen_counter = 0;
        /* Step 2. Use N and firstseed to  generate random prime q
     * using Apendix C.6 */
        CHECK_SEC_OK(makePrimefromSeedShaweTaylor(hashtype, N, &firstseed, &Q,
                                                  &qseed, &qgen_counter));
        /* Step 3. Use floor(L/2+1) and qseed to generate random prime p0
     * using Appendix C.6 */
        pgen_counter = 0;
        CHECK_SEC_OK(makePrimefromSeedShaweTaylor(hashtype, (L + 1) / 2 + 1,
                                                  &qseed, &p0, &pseed, &pgen_counter));
        /* Steps 4-22 FIPS 186-3 appendix A.1.2.1.2 */
        CHECK_SEC_OK(makePrimefromPrimesShaweTaylor(hashtype, L, seedBytes * 8,
                                                    &p0, &Q, &P, &pseed, &pgen_counter));

        /* combine all the seeds */
        if ((qseed.len > firstseed.len) || (pseed.len > firstseed.len)) {
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE); /* shouldn't happen */
            goto cleanup;
        }
        /* If the seed overflows, then pseed and qseed may have leading zeros which the mpl code clamps.
         * we want to make sure those are added back in so the individual seed lengths are predictable from
         * the overall seed length */
        seed->len = firstseed.len * 3;
        seed->data = PORT_ArenaZAlloc(verify->arena, seed->len);
        if (seed->data == NULL) {
            goto cleanup;
        }
        PORT_Memcpy(seed->data, firstseed.data, firstseed.len);
        PORT_Memcpy(seed->data + 2 * firstseed.len - pseed.len, pseed.data, pseed.len);
        PORT_Memcpy(seed->data + 3 * firstseed.len - qseed.len, qseed.data, qseed.len);
        counter = (qgen_counter << 16) | pgen_counter;

        /* we've generated both P and Q now, skip to generating G */
        goto generate_G;
    }
    /* ******************************************************************
    ** Step 8. (Step 4 in 186-1)
    ** "Use a robust primality testing algorithm to test whether q is prime."
    **
    ** Appendix 2.1 states that a Rabin test with at least 50 iterations
    ** "will give an acceptable probability of error."
    */
    /*CHECK_SEC_OK( prm_RabinTest(&Q, &passed) );*/
    err = mpp_pprime(&Q, prime_testcount_q(L, N));
    passed = (err == MP_YES) ? SECSuccess : SECFailure;
    /* ******************************************************************
    ** Step 9. (Step 5 in 186-1) "If q is not prime, goto step 5 (1 in 186-1)."
    */
    if (passed != SECSuccess)
        goto step_5;
    /* ******************************************************************
    ** Step 10.
    **      offset = 1;
    **(     Step 6b 186-1)"Let counter = 0 and offset = 2."
    */
    offset = (type == FIPS186_1_TYPE) ? 2 : 1;
    /*
    ** Step 11. (Step 6a,13a,14 in 186-1)
    **  For counter - 0 to (4L-1) do
    **
    */
    maxCount = L >= 1024 ? (4 * L - 1) : 4095;
    for (counter = 0; counter <= maxCount; counter++) {
        /* ******************************************************************
    ** Step 11.1  (Step 7 in 186-1)
    ** "for j = 0 ... n let
    **          V_j = HASH[(SEED + offset + j) mod 2**seedlen]."
    **
    ** Step 11.2 (Step 8 in 186-1)
    ** "W = V_0 + V_1*2**outlen+...+ V_n-1 * 2**((n-1)*outlen) +
    **                               ((Vn* mod 2**b)*2**(n*outlen))"
    ** Step 11.3 (Step 8 in 186-1)
    ** "X = W + 2**(L-1)
    **  Note that 0 <= W < 2**(L-1) and hence 2**(L-1) <= X < 2**L."
    **
    ** Step 11.4 (Step 9 in 186-1).
    ** "c = X mod 2q"
    **
    ** Step 11.5 (Step 9 in 186-1).
    ** " p = X - (c - 1).
    **  Note that p is congruent to 1 mod 2q."
    */
        CHECK_SEC_OK(makePfromQandSeed(hashtype, L, N, offset, seedlen,
                                       seed, &Q, &P));
        /*************************************************************
    ** Step 11.6. (Step 10 in 186-1)
    ** "if p < 2**(L-1), then goto step 11.9. (step 13 in 186-1)"
    */
        CHECK_MPI_OK(mpl_set_bit(&l, (mp_size)(L - 1), 1)); /* l = 2**(L-1) */
        if (mp_cmp(&P, &l) < 0)
            goto step_11_9;
        /************************************************************
    ** Step 11.7 (step 11 in 186-1)
    ** "Perform a robust primality test on p."
    */
        /*CHECK_SEC_OK( prm_RabinTest(&P, &passed) );*/
        err = mpp_pprime(&P, prime_testcount_p(L, N));
        passed = (err == MP_YES) ? SECSuccess : SECFailure;
        /* ******************************************************************
    ** Step 11.8. "If p is determined to be primed return VALID
        ** values of p, q, seed and counter."
    */
        if (passed == SECSuccess)
            break;
    step_11_9:
        /* ******************************************************************
    ** Step 11.9.  "offset = offset + n + 1."
    */
        offset += n + 1;
    }
    /* ******************************************************************
    ** Step 12.  "goto step 5."
    **
    ** NOTE: if counter <= maxCount, then we exited the loop at Step 11.8
    ** and now need to return p,q, seed, and counter.
    */
    if (counter > maxCount)
        goto step_5;

generate_G:
    /* ******************************************************************
    ** returning p, q, seed and counter
    */
    if (type == FIPS186_1_TYPE) {
        /* Generate g, This is called the "Unverifiable Generation of g
     * in FIPA186-3 Appedix A.2.1. For compatibility we maintain
     * this version of the code */
        SECITEM_AllocItem(NULL, &hit, L / 8); /* h is no longer than p */
        if (!hit.data)
            goto cleanup;
        do {
            /* loop generate h until 1<h<p-1 and (h**[(p-1)/q])mod p > 1 */
            CHECK_SEC_OK(generate_h_candidate(&hit, &H));
            CHECK_SEC_OK(makeGfromH(&P, &Q, &H, &G, &passed));
        } while (passed != PR_TRUE);
        MPINT_TO_SECITEM(&H, &verify->h, verify->arena);
    } else {
        unsigned char index = 1; /* default to 1 */
        verify->h.data = (unsigned char *)PORT_ArenaZAlloc(verify->arena, 1);
        if (verify->h.data == NULL) {
            goto cleanup;
        }
        verify->h.len = 1;
        verify->h.data[0] = index;
        /* Generate g, using the FIPS 186-3 Appendix A.23 */
        CHECK_SEC_OK(makeGfromIndex(hashtype, &P, &Q, seed, index, &G));
    }
    /* All generation is done.  Now, save the PQG params.  */
    MPINT_TO_SECITEM(&P, &params->prime, params->arena);
    MPINT_TO_SECITEM(&Q, &params->subPrime, params->arena);
    MPINT_TO_SECITEM(&G, &params->base, params->arena);
    verify->counter = counter;
    *pParams = params;
    *pVfy = verify;
cleanup:
    if (pseed.data) {
        PORT_Free(pseed.data);
    }
    if (qseed.data) {
        PORT_Free(qseed.data);
    }
    mp_clear(&P);
    mp_clear(&Q);
    mp_clear(&G);
    mp_clear(&H);
    mp_clear(&l);
    mp_clear(&p0);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    if (rv) {
        if (params) {
            PORT_FreeArena(params->arena, PR_TRUE);
        }
        if (verify) {
            PORT_FreeArena(verify->arena, PR_TRUE);
        }
    }
    if (hit.data) {
        SECITEM_FreeItem(&hit, PR_FALSE);
    }
    return rv;
}

SECStatus
PQG_ParamGen(unsigned int j, PQGParams **pParams, PQGVerify **pVfy)
{
    unsigned int L; /* Length of P in bits.  Per FIPS 186. */
    unsigned int seedBytes;

    if (j > 8 || !pParams || !pVfy) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    L = 512 + (j * 64); /* bits in P */
    seedBytes = L / 8;
    return pqg_ParamGen(L, DSA1_Q_BITS, FIPS186_1_TYPE, seedBytes,
                        pParams, pVfy);
}

SECStatus
PQG_ParamGenSeedLen(unsigned int j, unsigned int seedBytes,
                    PQGParams **pParams, PQGVerify **pVfy)
{
    unsigned int L; /* Length of P in bits.  Per FIPS 186. */

    if (j > 8 || !pParams || !pVfy) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    L = 512 + (j * 64); /* bits in P */
    return pqg_ParamGen(L, DSA1_Q_BITS, FIPS186_1_TYPE, seedBytes,
                        pParams, pVfy);
}

SECStatus
PQG_ParamGenV2(unsigned int L, unsigned int N, unsigned int seedBytes,
               PQGParams **pParams, PQGVerify **pVfy)
{
    if (N == 0) {
        N = pqg_get_default_N(L);
    }
    if (seedBytes == 0) {
        /* seedBytes == L/8 for probable primes, N/8 for Shawe-Taylor Primes */
        seedBytes = N / 8;
    }
    if (pqg_validate_dsa2(L, N) != SECSuccess) {
        /* error code already set */
        return SECFailure;
    }
    return pqg_ParamGen(L, N, FIPS186_3_ST_TYPE, seedBytes, pParams, pVfy);
}

/*
 * verify can use vfy structures returned from either FIPS186-1 or
 * FIPS186-2, and can handle differences in selected Hash functions to
 * generate the parameters.
 */
SECStatus
PQG_VerifyParams(const PQGParams *params,
                 const PQGVerify *vfy, SECStatus *result)
{
    SECStatus rv = SECSuccess;
    unsigned int g, n, L, N, offset, outlen;
    mp_int p0, P, Q, G, P_, Q_, G_, r, h;
    mp_err err = MP_OKAY;
    int j;
    unsigned int counter_max = 0; /* handle legacy L < 1024 */
    unsigned int qseed_len;
    unsigned int qgen_counter_ = 0;
    SECItem pseed_ = { 0, 0, 0 };
    HASH_HashType hashtype;
    pqgGenType type;

#define CHECKPARAM(cond)      \
    if (!(cond)) {            \
        *result = SECFailure; \
        goto cleanup;         \
    }
    if (!params || !vfy || !result) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* always need at least p, q, and seed for any meaningful check */
    if ((params->prime.len == 0) || (params->subPrime.len == 0) ||
        (vfy->seed.len == 0)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    /* we want to either check PQ or G or both. If we don't have G, make
     * sure we have count so we can check P. */
    if ((params->base.len == 0) && (vfy->counter == -1)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    MP_DIGITS(&p0) = 0;
    MP_DIGITS(&P) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&G) = 0;
    MP_DIGITS(&P_) = 0;
    MP_DIGITS(&Q_) = 0;
    MP_DIGITS(&G_) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&h) = 0;
    CHECK_MPI_OK(mp_init(&p0));
    CHECK_MPI_OK(mp_init(&P));
    CHECK_MPI_OK(mp_init(&Q));
    CHECK_MPI_OK(mp_init(&G));
    CHECK_MPI_OK(mp_init(&P_));
    CHECK_MPI_OK(mp_init(&Q_));
    CHECK_MPI_OK(mp_init(&G_));
    CHECK_MPI_OK(mp_init(&r));
    CHECK_MPI_OK(mp_init(&h));
    *result = SECSuccess;
    SECITEM_TO_MPINT(params->prime, &P);
    SECITEM_TO_MPINT(params->subPrime, &Q);
    /* if G isn't specified, just check P and Q */
    if (params->base.len != 0) {
        SECITEM_TO_MPINT(params->base, &G);
    }
    /* 1.  Check (L,N) pair */
    N = mpl_significant_bits(&Q);
    L = mpl_significant_bits(&P);
    if (L < 1024) {
        /* handle DSA1 pqg parameters with less thatn 1024 bits*/
        CHECKPARAM(N == DSA1_Q_BITS);
        j = PQG_PBITS_TO_INDEX(L);
        CHECKPARAM(j >= 0 && j <= 8);
        counter_max = 4096;
    } else {
        /* handle DSA2 parameters (includes DSA1, 1024 bits) */
        CHECKPARAM(pqg_validate_dsa2(L, N) == SECSuccess);
        counter_max = 4 * L;
    }
    /* 3.  G < P */
    if (params->base.len != 0) {
        CHECKPARAM(mp_cmp(&G, &P) < 0);
    }
    /* 4.  P % Q == 1 */
    CHECK_MPI_OK(mp_mod(&P, &Q, &r));
    CHECKPARAM(mp_cmp_d(&r, 1) == 0);
    /* 5.  Q is prime */
    CHECKPARAM(mpp_pprime(&Q, prime_testcount_q(L, N)) == MP_YES);
    /* 6.  P is prime */
    CHECKPARAM(mpp_pprime(&P, prime_testcount_p(L, N)) == MP_YES);
    /* Steps 7-12 are done only if the optional PQGVerify is supplied. */
    /* continue processing P */
    /* 7.  counter < 4*L */
    /* 8.  g >= N and g < 2*L   (g is length of seed in bits) */
    /* step 7 and 8 are delayed until we determine which type of generation
     * was used */
    /* 9.  Q generated from SEED matches Q in PQGParams. */
    /* This function checks all possible hash and generation types to
     * find a Q_ which matches Q. */
    g = vfy->seed.len * 8;
    CHECKPARAM(findQfromSeed(L, N, g, &vfy->seed, &Q, &Q_, &qseed_len,
                             &hashtype, &type, &qgen_counter_) == SECSuccess);
    CHECKPARAM(mp_cmp(&Q, &Q_) == 0);
    /* now we can do steps 7  & 8*/
    if ((type == FIPS186_1_TYPE) || (type == FIPS186_3_TYPE)) {
        CHECKPARAM((vfy->counter == -1) || (vfy->counter < counter_max));
        CHECKPARAM(g >= N && g < counter_max / 2);
    }
    if (type == FIPS186_3_ST_TYPE) {
        SECItem qseed = { 0, 0, 0 };
        SECItem pseed = { 0, 0, 0 };
        unsigned int first_seed_len;
        unsigned int pgen_counter_ = 0;
        unsigned int qgen_counter = (vfy->counter >> 16) & 0xffff;
        unsigned int pgen_counter = (vfy->counter) & 0xffff;

        /* extract pseed and qseed from domain_parameter_seed, which is
         * first_seed || pseed || qseed. qseed is first_seed + small_integer
         * mod the length of first_seed. pseed is qseed + small_integer mod
         * the length of first_seed. This means most of the time
         * first_seed.len == qseed.len == pseed.len. Rarely qseed.len and/or
         * pseed.len will be smaller because mpi clamps them. pqgGen
         * automatically adds the zero pad back though, so we can depend
         * domain_parameter_seed.len to be a multiple of three. We only have
         * to deal with the fact that the returned seeds from our functions
         * could be shorter.
         *   first_seed.len = domain_parameter_seed.len/3
         * We can now find the offsets;
         * first_seed.data = domain_parameter_seed.data + 0
         * pseed.data = domain_parameter_seed.data + first_seed.len
         * qseed.data = domain_parameter_seed.data
         *         + domain_paramter_seed.len - qseed.len
         * We deal with pseed possibly having zero pad in the pseed check later.
         */
        first_seed_len = vfy->seed.len / 3;
        CHECKPARAM(qseed_len < vfy->seed.len);
        CHECKPARAM(first_seed_len * 8 > N - 1);
        CHECKPARAM(first_seed_len * 8 < counter_max / 2);
        CHECKPARAM(first_seed_len >= qseed_len);
        qseed.len = qseed_len;
        qseed.data = vfy->seed.data + vfy->seed.len - qseed.len;
        pseed.len = first_seed_len;
        pseed.data = vfy->seed.data + first_seed_len;

        /*
         * now complete FIPS 186-3 A.1.2.1.2. Step 1 was completed
         * above in our initial checks, Step 2 was completed by
         * findQfromSeed */

        /* Step 3 (status, c0, prime_seed, prime_gen_counter) =
        ** (ST_Random_Prime((ceil(length/2)+1, input_seed)
        */
        CHECK_SEC_OK(makePrimefromSeedShaweTaylor(hashtype, (L + 1) / 2 + 1,
                                                  &qseed, &p0, &pseed_, &pgen_counter_));
        /* Steps 4-22 FIPS 186-3 appendix A.1.2.1.2 */
        CHECK_SEC_OK(makePrimefromPrimesShaweTaylor(hashtype, L, first_seed_len * 8,
                                                    &p0, &Q_, &P_, &pseed_, &pgen_counter_));
        CHECKPARAM(mp_cmp(&P, &P_) == 0);
        /* make sure pseed wasn't tampered with (since it is part of
         * calculating G) */
        if (pseed.len > pseed_.len) {
            /* handle the case of zero pad for pseed */
            int extra = pseed.len - pseed_.len;
            int i;
            for (i = 0; i < extra; i++) {
                if (pseed.data[i] != 0) {
                    *result = SECFailure;
                    goto cleanup;
                }
            }
            pseed.data += extra;
            pseed.len -= extra;
            /* the rest is handled in the normal compare below */
        }
        CHECKPARAM(SECITEM_CompareItem(&pseed, &pseed_) == SECEqual);
        if (vfy->counter != -1) {
            CHECKPARAM(pgen_counter < counter_max);
            CHECKPARAM(qgen_counter < counter_max);
            CHECKPARAM((pgen_counter_ == pgen_counter));
            CHECKPARAM((qgen_counter_ == qgen_counter));
        }
    } else if (vfy->counter == -1) {
        /* If counter is set to -1, we are really only verifying G, skip
         * the remainder of the checks for P */
        CHECKPARAM(type != FIPS186_1_TYPE); /* we only do this for DSA2 */
    } else {
        /* 10. P generated from (L, counter, g, SEED, Q) matches P
         * in PQGParams. */
        outlen = HASH_ResultLen(hashtype) * PR_BITS_PER_BYTE;
        n = (L - 1) / outlen;
        offset = vfy->counter * (n + 1) + ((type == FIPS186_1_TYPE) ? 2 : 1);
        CHECK_SEC_OK(makePfromQandSeed(hashtype, L, N, offset, g, &vfy->seed,
                                       &Q, &P_));
        CHECKPARAM(mp_cmp(&P, &P_) == 0);
    }

    /* now check G, skip if don't have a g */
    if (params->base.len == 0)
        goto cleanup;

    /* first Always check that G is OK  FIPS186-3 A.2.2  & A.2.4*/
    /* 1. 2 < G < P-1 */
    /* P is prime, p-1 == zero 1st bit */
    CHECK_MPI_OK(mpl_set_bit(&P, 0, 0));
    CHECKPARAM(mp_cmp_d(&G, 2) > 0 && mp_cmp(&G, &P) < 0);
    CHECK_MPI_OK(mpl_set_bit(&P, 0, 1)); /* set it back */
    /* 2. verify g**q mod p == 1 */
    CHECK_MPI_OK(mp_exptmod(&G, &Q, &P, &h)); /* h = G ** Q mod P */
    CHECKPARAM(mp_cmp_d(&h, 1) == 0);

    /* no h, the above is the best we can do */
    if (vfy->h.len == 0) {
        if (type != FIPS186_1_TYPE) {
            *result = SECWouldBlock;
        }
        goto cleanup;
    }

    /*
     * If h is one byte and FIPS186-3 was used to generate Q (we've verified
     * Q was generated from seed already, then we assume that FIPS 186-3
     * appendix A.2.3 was used to generate G. Otherwise we assume A.2.1 was
     * used to generate G.
     */
    if ((vfy->h.len == 1) && (type != FIPS186_1_TYPE)) {
        /* A.2.3 */
        CHECK_SEC_OK(makeGfromIndex(hashtype, &P, &Q, &vfy->seed,
                                    vfy->h.data[0], &G_));
        CHECKPARAM(mp_cmp(&G, &G_) == 0);
    } else {
        int passed;
        /* A.2.1 */
        SECITEM_TO_MPINT(vfy->h, &h);
        /* 11. 1 < h < P-1 */
        /* P is prime, p-1 == zero 1st bit */
        CHECK_MPI_OK(mpl_set_bit(&P, 0, 0));
        CHECKPARAM(mp_cmp_d(&G, 2) > 0 && mp_cmp(&G, &P));
        CHECK_MPI_OK(mpl_set_bit(&P, 0, 1)); /* set it back */
                                             /* 12. G generated from h matches G in PQGParams. */
        CHECK_SEC_OK(makeGfromH(&P, &Q, &h, &G_, &passed));
        CHECKPARAM(passed && mp_cmp(&G, &G_) == 0);
    }
cleanup:
    mp_clear(&p0);
    mp_clear(&P);
    mp_clear(&Q);
    mp_clear(&G);
    mp_clear(&P_);
    mp_clear(&Q_);
    mp_clear(&G_);
    mp_clear(&r);
    mp_clear(&h);
    if (pseed_.data) {
        SECITEM_FreeItem(&pseed_, PR_FALSE);
    }
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    return rv;
}

/**************************************************************************
 *  Free the PQGParams struct and the things it points to.                *
 **************************************************************************/
void
PQG_DestroyParams(PQGParams *params)
{
    if (params == NULL)
        return;
    if (params->arena != NULL) {
        PORT_FreeArena(params->arena, PR_FALSE); /* don't zero it */
    } else {
        SECITEM_FreeItem(&params->prime, PR_FALSE);    /* don't free prime */
        SECITEM_FreeItem(&params->subPrime, PR_FALSE); /* don't free subPrime */
        SECITEM_FreeItem(&params->base, PR_FALSE);     /* don't free base */
        PORT_Free(params);
    }
}

/**************************************************************************
 *  Free the PQGVerify struct and the things it points to.                *
 **************************************************************************/

void
PQG_DestroyVerify(PQGVerify *vfy)
{
    if (vfy == NULL)
        return;
    if (vfy->arena != NULL) {
        PORT_FreeArena(vfy->arena, PR_FALSE); /* don't zero it */
    } else {
        SECITEM_FreeItem(&vfy->seed, PR_FALSE); /* don't free seed */
        SECITEM_FreeItem(&vfy->h, PR_FALSE);    /* don't free h */
        PORT_Free(vfy);
    }
}
