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
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "blapi.h"
#include "secitem.h"
#include "mpi.h"
#include "mpprime.h"
#include "mplogic.h"

#define MAX_ITERATIONS 5  /* Maximum number of iterations of primegen */
#define NUMITER        40 /* Number iterations for primality tests    */

 /* XXX to be replaced by define in blapit.h */
#define BITS_IN_Q 160
#define NSS_FREEBL_DSA_DEFAULT_CHUNKSIZE 2048

/* XXX the following defines are common to all NSS code using MPI, but
   not part of MPI itself.  They should go in a private header in
   lib/freebl.
   */
#define CHECK_SEC_OK(func) if (SECSuccess != (rv = func)) goto cleanup

#define CHECK_MPI_OK(func) if (MP_OKAY > (err = func)) goto cleanup

#define OCTETS_TO_MPINT(oc, mp, len) \
    CHECK_MPI_OK(mp_read_unsigned_octets((mp), oc, len))

#define SECITEM_TO_MPINT(it, mp) \
    CHECK_MPI_OK(mp_read_unsigned_octets((mp), (it).data, (it).len))

#define MPINT_TO_SECITEM(mp, it, arena)                         \
    SECITEM_AllocItem(arena, (it), mp_unsigned_octet_size(mp)); \
    err = mp_to_unsigned_octets(mp, (it)->data, (it)->len);     \
    if (err < 0) goto cleanup; else err = MP_OKAY;

#define MP_TO_SEC_ERROR(err)                                          \
    switch (err) {                                                    \
    case MP_MEM:    PORT_SetError(SEC_ERROR_NO_MEMORY);       break;  \
    case MP_RANGE:  PORT_SetError(SEC_ERROR_BAD_DATA);        break;  \
    case MP_BADARG: PORT_SetError(SEC_ERROR_INVALID_ARGS);    break;  \
    default:        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE); break;  \
    }

/* For FIPS-compliance testing.
** The following array holds the seed defined in FIPS 186-1 appendix 5.
** This seed is used to generate P and Q according to appendix 2; use of
** this seed will exactly generate the PQG specified in appendix 2.
*/
#ifdef FIPS_186_1_A5_TEST
static const unsigned char fips_186_1_a5_pqseed[] = {
    0xd5, 0x01, 0x4e, 0x4b, 0x60, 0xef, 0x2b, 0xa8,
    0xb6, 0x21, 0x1b, 0x40, 0x62, 0xba, 0x32, 0x24,
    0xe0, 0x42, 0x7d, 0xd3
};
#endif

/* Get a seed for generating P and Q.  If in testing mode, copy in the
** seed from FIPS 186-1 appendix 5.  Otherwise, obtain bytes from the
** global random number generator.
*/
static SECStatus
getPQseed(SECItem *seed)
{
    if (seed->data) {
        PORT_Free(seed->data);
        seed->data = NULL;
    }
    seed->data = (unsigned char*)PORT_ZAlloc(seed->len);
    if (!seed->data) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
#ifdef FIPS_186_1_A5_TEST
    memcpy(seed->data, fips_186_1_a5_pqseed, seed->len);
#else
    return RNG_GenerateGlobalRandomBytes(seed->data, seed->len);
#endif
    return SECSuccess;
}

/* Generate a candidate h value.  If in testing mode, use the h value
** specified in FIPS 186-1 appendix 5, h = 2.  Otherwise, obtain bytes
** from the global random number generator.
*/
static SECStatus
generate_h_candidate(SECItem *hit, mp_int *H)
{
    SECStatus rv = SECSuccess;
    mp_err   err = MP_OKAY;
#ifdef FIPS_186_1_A5_TEST
    memset(hit->data, 0, hit->len);
    hit->data[hit->len-1] = 0x02;
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

/* Compute SHA[(SEED + addend) mod 2**g]
** Result is placed in shaOutBuf.
** This computation is used in steps 2 and 7 of FIPS 186 Appendix 2.2 .
*/
static SECStatus
addToSeedThenSHA(const SECItem * seed,
                 unsigned long   addend,
                 int             g,
                 unsigned char * shaOutBuf)
{
    SECItem str = { 0, 0, 0 };
    mp_int s, sum, modulus, tmp;
    mp_err    err = MP_OKAY;
    SECStatus rv  = SECSuccess;
    MP_DIGITS(&s)       = 0;
    MP_DIGITS(&sum)     = 0;
    MP_DIGITS(&modulus) = 0;
    MP_DIGITS(&tmp)     = 0;
    CHECK_MPI_OK( mp_init(&s) );
    CHECK_MPI_OK( mp_init(&sum) );
    CHECK_MPI_OK( mp_init(&modulus) );
    SECITEM_TO_MPINT(*seed, &s); /* s = seed */
    /* seed += addend */
    if (addend < MP_DIGIT_MAX) {
	CHECK_MPI_OK( mp_add_d(&s, (mp_digit)addend, &s) );
    } else {
	CHECK_MPI_OK( mp_init(&tmp) );
	CHECK_MPI_OK( mp_set_int(&tmp, addend) ); /* XXX needs to be ulong! */
	CHECK_MPI_OK( mp_add(&s, &tmp, &s) );
    }
    CHECK_MPI_OK( mp_div_2d(&s, (mp_digit)g, NULL, &sum) );/*sum = s mod 2**g */
    MPINT_TO_SECITEM(&sum, &str, NULL);
    rv = SHA1_HashBuf(shaOutBuf, str.data, str.len); /* SHA1 hash result */
cleanup:
    mp_clear(&s);
    mp_clear(&sum);
    mp_clear(&modulus);
    mp_clear(&tmp);
    if (str.data)
	SECITEM_ZfreeItem(&str, PR_FALSE);
    if (err) {
	MP_TO_SEC_ERROR(err);
	return SECFailure;
    }
    return rv;
}

/*
**  Perform steps 2 and 3 of FIPS 186, appendix 2.2.
**  Generate Q from seed.
*/
static SECStatus
makeQfromSeed(
      unsigned int  g,          /* input.  Length of seed in bits. */
const SECItem   *   seed,       /* input.  */
      mp_int    *   Q)          /* output. */
{
    unsigned char sha1[SHA1_LENGTH];
    unsigned char sha2[SHA1_LENGTH];
    unsigned char U[SHA1_LENGTH];
    SECStatus rv  = SECSuccess;
    mp_err    err = MP_OKAY;
    int i;
    /* ******************************************************************
    ** Step 2.
    ** "Compute U = SHA[SEED] XOR SHA[(SEED+1) mod 2**g]."
    **/
    CHECK_SEC_OK( SHA1_HashBuf(sha1, seed->data, seed->len) );
    CHECK_SEC_OK( addToSeedThenSHA(seed, 1, g, sha2) );
    for (i=0; i<SHA1_LENGTH; ++i) U[i] = sha1[i] ^ sha2[i];
    /* ******************************************************************
    ** Step 3.
    ** "Form Q from U by setting the most signficant bit (the 2**159 bit)
    **  and the least signficant bit to 1.  In terms of boolean operations,
    **  Q = U OR 2**159 OR 1.  Note that 2**159 < Q < 2**160."
    */
    U[0]             |= 0x80;  /* U is MSB first */
    U[SHA1_LENGTH-1] |= 0x01;
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

/*  Perform steps 7, 8 and 9 of FIPS 186, appendix 2.2.
**  Generate P from Q, seed, L, and offset.
*/
static SECStatus
makePfromQandSeed(
      unsigned int  L,          /* Length of P in bits.  Per FIPS 186. */
      unsigned int  offset,     /* Per FIPS 186, appendix 2.2. */
      unsigned int  g,          /* input.  Length of seed in bits. */
const SECItem   *   seed,       /* input.  */
const mp_int    *   Q,          /* input.  */
      mp_int    *   P)          /* output. */
{
    unsigned int  k;            /* Per FIPS 186, appendix 2.2. */
    unsigned int  n;            /* Per FIPS 186, appendix 2.2. */
    mp_digit      b;            /* Per FIPS 186, appendix 2.2. */
    unsigned char V_k[SHA1_LENGTH];
    mp_int        W, X, c, twoQ, V_n, tmp, shift;
    mp_err    err = MP_OKAY;
    SECStatus rv  = SECSuccess;
    /* Initialize bignums */
    MP_DIGITS(&W)     = 0;
    MP_DIGITS(&X)     = 0;
    MP_DIGITS(&c)     = 0;
    MP_DIGITS(&twoQ)  = 0;
    MP_DIGITS(&V_n)   = 0;
    MP_DIGITS(&tmp)   = 0;
    MP_DIGITS(&shift) = 0;
    CHECK_MPI_OK( mp_init(&W)    );
    CHECK_MPI_OK( mp_init(&X)    );
    CHECK_MPI_OK( mp_init(&c)    );
    CHECK_MPI_OK( mp_init(&twoQ) );
    CHECK_MPI_OK( mp_init(&tmp)  );
    CHECK_MPI_OK( mp_init(&V_n)  );
    CHECK_MPI_OK( mp_init(&shift));
    /* L - 1 = n*160 + b */
    n = (L - 1) / BITS_IN_Q;
    b = (L - 1) % BITS_IN_Q;
    /* ******************************************************************
    ** Step 7.
    **  "for k = 0 ... n let
    **           V_k = SHA[(SEED + offset + k) mod 2**g]."
    **
    ** Step 8.
    **  "Let W be the integer 
    **    W = V_0 + (V_1 * 2**160) + ... + (V_n-1 * 2**((n-1)*160)) 
    **         + ((V_n mod 2**b) * 2**(n*160))
    */
    for (k=0; k<n; ++k) { /* Do the first n-1 terms of V_k */
	/* Do step 7 for iteration k.
	** V_k = SHA[(seed + offset + k) mod 2**g]
	*/
	CHECK_SEC_OK( addToSeedThenSHA(seed, offset + k, g, V_k) );
	/* Do step 8 for iteration k.
	** W += V_k * 2**(k*160)
	*/
	OCTETS_TO_MPINT(V_k, &tmp, SHA1_LENGTH);           /* get bignum V_k */
	CHECK_MPI_OK( mp_2expt(&shift, (mp_digit)k*160) ); /* 2**(k*160)     */
	CHECK_MPI_OK( mp_mul(&tmp, &shift, &tmp) );        /* V_k << shift   */
	CHECK_MPI_OK( mp_add(&W, &tmp, &W) );              /* W += tmp       */
	mp_zero(&tmp);   /* XXX needed? */
	mp_zero(&shift); /* XXX needed? */
    }
    /* Step 8, continued.
    **   [W += ((V_n mod 2**b) * 2**(n*160))] 
    */
    CHECK_SEC_OK( addToSeedThenSHA(seed, offset + n, g, V_k) );
    OCTETS_TO_MPINT(V_k, &V_n, SHA1_LENGTH);           /* get bignum V_n     */
    CHECK_MPI_OK( mp_div_2d(&V_n, b, NULL, &tmp) );    /* tmp = V_n mod 2**b */
    CHECK_MPI_OK( mp_2expt(&shift, (mp_digit)n*160) ); /* 2**(n*160)         */
    CHECK_MPI_OK( mp_mul(&tmp, &shift, &tmp) );        /* tmp << shift       */
    CHECK_MPI_OK( mp_add(&W, &tmp, &W) );              /* W += tmp           */
    /* Step 8, continued.
    ** "and let X = W + 2**(L-1).
    **  Note that 0 <= W < 2**(L-1) and hence 2**(L-1) <= X < 2**L."
    */
    CHECK_MPI_OK( mpl_set_bit(&X, (mp_size)(L-1), 1) );    /* X = 2**(L-1) */
    CHECK_MPI_OK( mp_add(&X, &W, &X) );                    /* X += W       */
    /*************************************************************
    ** Step 9.
    ** "Let c = X mod 2q  and set p = X - (c - 1).
    **  Note that p is congruent to 1 mod 2q."
    */
    CHECK_MPI_OK( mp_mul_2(Q, &twoQ) );                    /* 2q           */
    CHECK_MPI_OK( mp_mod(&X, &twoQ, &c) );                 /* c = X mod 2q */
    CHECK_MPI_OK( mp_sub_d(&c, 1, &c) );                   /* c -= 1       */
    CHECK_MPI_OK( mp_sub(&X, &c, P) );                     /* P = X - c    */
cleanup:
    mp_clear(&W);
    mp_clear(&X);
    mp_clear(&c);
    mp_clear(&twoQ);
    mp_clear(&V_n);
    mp_clear(&tmp);
    mp_clear(&shift);
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
makeGfromH(const mp_int *P,     /* input.  */
           const mp_int *Q,     /* input.  */
                 mp_int *H,     /* input and output. */
                 mp_int *G,     /* output. */
                 PRBool *passed)
{
    mp_int exp, pm1;
    mp_err err = MP_OKAY;
    *passed = PR_FALSE;
    MP_DIGITS(&exp) = 0;
    MP_DIGITS(&pm1) = 0;
    CHECK_MPI_OK( mp_init(&exp) );
    CHECK_MPI_OK( mp_init(&pm1) );
    CHECK_MPI_OK( mp_sub_d(P, 1, &pm1) );        /* P - 1            */
    CHECK_MPI_OK( mp_mod(H, &pm1, H) );          /* H = H mod (P-1)  */
    /* Check for H = to 0 or 1.  Regen H if so.  (Regen means return error). */
    if (mp_cmp_z(H) == 0 || mp_cmp_d(H, 1) == 0)
	goto cleanup;
    /* Compute G, according to the equation  G = (H ** ((P-1)/Q)) mod P */
    CHECK_MPI_OK( mp_div(&pm1, Q, &exp, NULL) );  /* exp = (P-1)/Q      */
    CHECK_MPI_OK( mp_exptmod(H, &exp, P, G) );    /* G = H ** exp mod P */
    /* Check for G == 0 or G == 1, return error if so. */
    if (mp_cmp_z(G) == 0 || mp_cmp_d(G, 1) == 0)
	goto cleanup;
    *passed = PR_TRUE;
cleanup:
    mp_clear(&exp);
    mp_clear(&pm1);
    if (err) {
	MP_TO_SEC_ERROR(err);
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
PQG_ParamGen(unsigned int j, PQGParams **pParams, PQGVerify **pVfy)
{
    unsigned int L;            /* Length of P in bits.  Per FIPS 186. */
    unsigned int seedBytes;

    if (j > 8 || !pParams || !pVfy) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    L = 512 + (j * 64);         /* bits in P */
    seedBytes = L/8;
    return PQG_ParamGenSeedLen(j, seedBytes, pParams, pVfy);
}

/* This code uses labels and gotos, so that it can follow the numbered
** steps in the algorithms from FIPS 186 appendix 2.2 very closely,
** and so that the correctness of this code can be easily verified.
** So, please forgive the ugly c code.
**/
SECStatus
PQG_ParamGenSeedLen(unsigned int j, unsigned int seedBytes,
                    PQGParams **pParams, PQGVerify **pVfy)
{
    unsigned int  L;        /* Length of P in bits.  Per FIPS 186. */
    unsigned int  n;        /* Per FIPS 186, appendix 2.2. */
    unsigned int  b;        /* Per FIPS 186, appendix 2.2. */
    unsigned int  g;        /* Per FIPS 186, appendix 2.2. */
    unsigned int  counter;  /* Per FIPS 186, appendix 2.2. */
    unsigned int  offset;   /* Per FIPS 186, appendix 2.2. */
    SECItem      *seed;     /* Per FIPS 186, appendix 2.2. */
    PRArenaPool  *arena  = NULL;
    PQGParams    *params = NULL;
    PQGVerify    *verify = NULL;
    PRBool passed;
    SECItem hit = { 0, 0, 0 };
    mp_int P, Q, G, H, l;
    mp_err    err = MP_OKAY;
    SECStatus rv  = SECFailure;
    int iterations = 0;
    if (j > 8 || !pParams || !pVfy) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* Initialize an arena for the params. */
    arena = PORT_NewArena(NSS_FREEBL_DSA_DEFAULT_CHUNKSIZE);
    if (!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    params = (PQGParams *)PORT_ArenaAlloc(arena, sizeof(PQGParams));
    if (!params) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }
    params->arena = arena;
    /* Initialize an arena for the verify. */
    arena = PORT_NewArena(NSS_FREEBL_DSA_DEFAULT_CHUNKSIZE);
    if (!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    verify = (PQGVerify *)PORT_ArenaAlloc(arena, sizeof(PQGVerify));
    if (!verify) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_FreeArena(arena, PR_TRUE);
	PORT_FreeArena(params->arena, PR_TRUE);
	return SECFailure;
    }
    verify->arena = arena;
    seed = &verify->seed;
    arena = NULL;
    /* Initialize bignums */
    MP_DIGITS(&P) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&G) = 0;
    MP_DIGITS(&H) = 0;
    MP_DIGITS(&l) = 0;
    CHECK_MPI_OK( mp_init(&P) );
    CHECK_MPI_OK( mp_init(&Q) );
    CHECK_MPI_OK( mp_init(&G) );
    CHECK_MPI_OK( mp_init(&H) );
    CHECK_MPI_OK( mp_init(&l) );
    /* Compute lengths. */
    L = 512 + (j * 64);               /* bits in P */
    n = (L - 1) / BITS_IN_Q;          /* BITS_IN_Q is 160 */  
    b = (L - 1) % BITS_IN_Q;
    g = seedBytes * BITS_PER_BYTE;    /* bits in seed, NOT G of PQG. */
step_1:
    /* ******************************************************************
    ** Step 1.
    ** "Choose an abitrary sequence of at least 160 bits and call it SEED.
    **  Let g be the length of SEED in bits."
    */
    if (++iterations > MAX_ITERATIONS) {        /* give up after a while */
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        goto cleanup;
    }
    seed->len = seedBytes;
    CHECK_SEC_OK( getPQseed(seed) );
    /* ******************************************************************
    ** Step 2.
    ** "Compute U = SHA[SEED] XOR SHA[(SEED+1) mod 2**g]."
    **
    ** Step 3.
    ** "Form Q from U by setting the most signficant bit (the 2**159 bit) 
    **  and the least signficant bit to 1.  In terms of boolean operations,
    **  Q = U OR 2**159 OR 1.  Note that 2**159 < Q < 2**160."
    */
    CHECK_SEC_OK( makeQfromSeed(g, seed, &Q) );
    /* ******************************************************************
    ** Step 4.
    ** "Use a robust primality testing algorithm to test whether q is prime."
    **
    ** Appendix 2.1 states that a Rabin test with at least 50 iterations
    ** "will give an acceptable probability of error."
    */
    /*CHECK_SEC_OK( prm_RabinTest(&Q, &passed) );*/
    err = mpp_pprime(&Q, 40);
    passed = (err == MP_YES) ? SECSuccess : SECFailure;
    /* ******************************************************************
    ** Step 5. "If q is not prime, goto step 1."
    */
    if (passed != SECSuccess)
        goto step_1;
    /* ******************************************************************
    ** Step 6. "Let counter = 0 and offset = 2."
    */
    counter = 0;
    offset  = 2;
step_7:
    /* ******************************************************************
    ** Step 7.
    ** "for k = 0 ... n let
    **          V_k = SHA[(SEED + offset + k) mod 2**g]."
    **
    ** Step 8.
    ** "Let W be the sum of  (V_k * 2**(k*160)) for k = 0 ... n
    **  and let X = W + 2**(L-1).
    **  Note that 0 <= W < 2**(L-1) and hence 2**(L-1) <= X < 2**L."
    **
    ** Step 9.
    ** "Let c = X mod 2q  and set p = X - (c - 1).
    **  Note that p is congruent to 1 mod 2q."
    */
    CHECK_SEC_OK( makePfromQandSeed(L, offset, g, seed, &Q, &P) );
    /*************************************************************
    ** Step 10.
    ** "if p < 2**(L-1), then goto step 13."
    */
    CHECK_MPI_OK( mpl_set_bit(&l, (mp_size)(L-1), 1) ); /* l = 2**(L-1) */
    if (mp_cmp(&P, &l) < 0)
        goto step_13;
    /************************************************************
    ** Step 11.
    ** "Perform a robust primality test on p."
    */
    /*CHECK_SEC_OK( prm_RabinTest(&P, &passed) );*/
    err = mpp_pprime(&P, 40);
    passed = (err == MP_YES) ? SECSuccess : SECFailure;
    /* ******************************************************************
    ** Step 12. "If p passes the test performed in step 11, go to step 15."
    */
    if (passed == SECSuccess)
        goto step_15;
step_13:
    /* ******************************************************************
    ** Step 13.  "Let counter = counter + 1 and offset = offset + n + 1."
    */
    counter++;
    offset += n + 1;
    /* ******************************************************************
    ** Step 14.  "If counter >= 4096 goto step 1, otherwise go to step 7."
    */
    if (counter >= 4096)
        goto step_1;
    goto step_7;
step_15:
    /* ******************************************************************
    ** Step 15.  
    ** "Save the value of SEED and the value of counter for use
    **  in certifying the proper generation of p and q."
    */
    /* Generate h. */
    SECITEM_AllocItem(NULL, &hit, seedBytes); /* h is no longer than p */
    if (!hit.data) goto cleanup;
    do {
	/* loop generate h until 1<h<p-1 and (h**[(p-1)/q])mod p > 1 */
	CHECK_SEC_OK( generate_h_candidate(&hit, &H) );
        CHECK_SEC_OK( makeGfromH(&P, &Q, &H, &G, &passed) );
    } while (passed != PR_TRUE);
    /* All generation is done.  Now, save the PQG params.  */
    MPINT_TO_SECITEM(&P, &params->prime,    params->arena);
    MPINT_TO_SECITEM(&Q, &params->subPrime, params->arena);
    MPINT_TO_SECITEM(&G, &params->base,     params->arena);
    MPINT_TO_SECITEM(&H, &verify->h,        verify->arena);
    verify->counter = counter;
    *pParams = params;
    *pVfy = verify;
cleanup:
    mp_clear(&P);
    mp_clear(&Q);
    mp_clear(&G);
    mp_clear(&H);
    mp_clear(&l);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    if (rv) {
	PORT_FreeArena(params->arena, PR_TRUE);
	PORT_FreeArena(verify->arena, PR_TRUE);
    }
    return rv;
}

SECStatus   
PQG_VerifyParams(const PQGParams *params, 
                 const PQGVerify *vfy, SECStatus *result)
{
    SECStatus rv = SECSuccess;
    SECStatus passed;
    unsigned int g, n, L, offset;
    mp_int P, Q, G, P_, Q_, G_, r, h;
    mp_err err = MP_OKAY;
    int j;
#define CHECKPARAM(cond)      \
    if (!(cond)) {            \
	rv = *result = SECFailure; \
	goto cleanup;         \
    }
    if (!params || !vfy || !result) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    MP_DIGITS(&P) = 0;
    MP_DIGITS(&Q) = 0;
    MP_DIGITS(&G) = 0;
    MP_DIGITS(&P_) = 0;
    MP_DIGITS(&Q_) = 0;
    MP_DIGITS(&G_) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&h) = 0;
    CHECK_MPI_OK( mp_init(&P) );
    CHECK_MPI_OK( mp_init(&Q) );
    CHECK_MPI_OK( mp_init(&G) );
    CHECK_MPI_OK( mp_init(&P_) );
    CHECK_MPI_OK( mp_init(&Q_) );
    CHECK_MPI_OK( mp_init(&G_) );
    CHECK_MPI_OK( mp_init(&r) );
    CHECK_MPI_OK( mp_init(&h) );
    *result = SECSuccess;
    SECITEM_TO_MPINT(params->prime,    &P);
    SECITEM_TO_MPINT(params->subPrime, &Q);
    SECITEM_TO_MPINT(params->base,     &G);
    /* 1.  Q is 160 bits long. */
    CHECKPARAM( params->subPrime.len == DSA_SUBPRIME_LEN &&
                params->subPrime.data[0] & 0x80 );
    /* 2.  P is one of the 9 valid lengths. */
    L = params->prime.len * 8;
    CHECKPARAM( params->prime.data[0] & 0x80 );
    for (j=0; j<9; j++)
	if (L == PQG_INDEX_TO_PBITS(j)) break;
    CHECKPARAM( j < 9 );
    /* 3.  G < P */
    CHECKPARAM( mp_cmp(&G, &P) < 0 );
    /* 4.  P % Q == 1 */
    rv = mp_mod(&P, &Q, &r);
    CHECKPARAM( mp_cmp_d(&r, 1) == 0 );
    /* 5.  Q is prime */
    CHECKPARAM( mpp_pprime(&Q, NUMITER) == MP_YES ); /* XXX rabin test */
    /* 6.  P is prime */
    CHECKPARAM( mpp_pprime(&P, NUMITER) == MP_YES ); /* XXX rabin test */
    /* Steps 7-12 are done only if the optional PQGVerify is supplied. */
    if (!vfy) goto cleanup;
    /* 7.  counter < 4096 */
    CHECKPARAM( vfy->counter < 4096 );
    /* 8.  g >= 160 and g < 2048   (g is length of seed in bits) */
    g = vfy->seed.len * 8;
    CHECKPARAM( g >= 160 && g < 4096 );
    /* 9.  Q generated from SEED matches Q in PQGParams. */
    CHECK_SEC_OK( makeQfromSeed(g, &vfy->seed, &Q_) );
    CHECKPARAM( mp_cmp(&Q, &Q_) == 0 );
    /* 10. P generated from (L, counter, g, SEED, Q) matches P in PQGParams. */
    n = (L - 1) / BITS_IN_Q;
    offset = vfy->counter * (n + 1) + 2;
    CHECK_SEC_OK( makePfromQandSeed(L, offset, g, &vfy->seed, &Q, &P_) );
    CHECKPARAM( mp_cmp(&P, &P_) == 0 );
    /* Next two are optional: if h == 0 ignore */
    if (vfy->h.len == 0) return rv;
    /* 11. 1 < h < P-1 */
    SECITEM_TO_MPINT(vfy->h, &h);
    CHECK_MPI_OK( mpl_set_bit(&P, 0, 0) ); /* P is prime, p-1 == zero 1st bit */
    CHECKPARAM( mp_cmp_d(&h, 1) > 0 && mp_cmp(&h, &P) );
    CHECK_MPI_OK( mpl_set_bit(&P, 0, 1) ); /* set it back */
    /* 12. G generated from h matches G in PQGParams. */
    CHECK_SEC_OK( makeGfromH(&P, &Q, &h, &G_, &passed) );
    CHECKPARAM( passed && mp_cmp(&G, &G_) == 0 );
cleanup:
    mp_clear(&P);
    mp_clear(&Q);
    mp_clear(&G);
    mp_clear(&P_);
    mp_clear(&Q_);
    mp_clear(&G_);
    mp_clear(&r);
    mp_clear(&h);
    if (err) {
	MP_TO_SEC_ERROR(err);
    }
    return rv;
}


