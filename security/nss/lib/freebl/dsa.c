/*
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prerror.h"
#include "secerr.h"

#include "prtypes.h"
#include "prinit.h"
#include "blapi.h"
#include "nssilock.h"
#include "secitem.h"
#include "blapi.h"
#include "mpi.h"
#include "secmpi.h"
#include "pqg.h"

 /* XXX to be replaced by define in blapit.h */
#define NSS_FREEBL_DSA_DEFAULT_CHUNKSIZE 2048

/*
 * FIPS 186-2 requires result from random output to be reduced mod q when 
 * generating random numbers for DSA. 
 *
 * Input: w, 2*qLen bytes
 *        q, qLen bytes
 * Output: xj, qLen bytes
 */
static SECStatus
fips186Change_ReduceModQForDSA(const PRUint8 *w, const PRUint8 *q,
                               unsigned int qLen, PRUint8 * xj)
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
    CHECK_MPI_OK( mp_read_unsigned_octets(&W, w, 2*qLen) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&Q, q, qLen) );

    /*
     * Algorithm 1 of FIPS 186-2 Change Notice 1, Step 3.3
     *
     * xj = (w0 || w1) mod q
     */
    CHECK_MPI_OK( mp_mod(&W, &Q, &Xj) );
    CHECK_MPI_OK( mp_to_fixlen_octets(&Xj, xj, qLen) );
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
 * FIPS 186-2 requires result from random output to be reduced mod q when 
 * generating random numbers for DSA. 
 */
SECStatus
FIPS186Change_ReduceModQForDSA(const unsigned char *w,
                               const unsigned char *q,
                               unsigned char *xj) {
    return fips186Change_ReduceModQForDSA(w, q, DSA1_SUBPRIME_LEN, xj);
}

/*
 * The core of Algorithm 1 of FIPS 186-2 Change Notice 1.
 *
 * We no longer support FIPS 186-2 RNG. This function was exported
 * for power-up self tests and FIPS tests. Keep this stub, which fails,
 * to prevent crashes, but also to signal to test code that FIPS 186-2
 * RNG is no longer supported.
 */
SECStatus
FIPS186Change_GenerateX(PRUint8 *XKEY, const PRUint8 *XSEEDj,
                        PRUint8 *x_j)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

/*
 * Specialized RNG for DSA
 *
 * As per Algorithm 1 of FIPS 186-2 Change Notice 1, in step 3.3 the value
 * Xj should be reduced mod q, a 160-bit prime number.  Since this parameter
 * is only meaningful in the context of DSA, the above RNG functions
 * were implemented without it.  They are re-implemented below for use
 * with DSA.
 */

/*
** Generate some random bytes, using the global random number generator
** object.  In DSA mode, so there is a q.
*/
static SECStatus 
dsa_GenerateGlobalRandomBytes(const SECItem * qItem, PRUint8 * dest,
                              unsigned int * destLen, unsigned int maxDestLen)
{
    SECStatus rv;
    SECItem w;
    const PRUint8 * q = qItem->data;
    unsigned int qLen = qItem->len;

    if (*q == 0) {
        ++q;
        --qLen;
    }
    if (maxDestLen < qLen) {
        /* This condition can occur when DSA_SignDigest is passed a group
           with a subprime that is larger than DSA_MAX_SUBPRIME_LEN. */
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    w.data = NULL; /* otherwise SECITEM_AllocItem asserts */
    if (!SECITEM_AllocItem(NULL, &w, 2*qLen)) {
        return SECFailure;
    }
    *destLen = qLen;

    rv = RNG_GenerateGlobalRandomBytes(w.data, w.len);
    if (rv == SECSuccess) {
        rv = fips186Change_ReduceModQForDSA(w.data, q, qLen, dest);
    }

    SECITEM_FreeItem(&w, PR_FALSE);
    return rv;
}

static void translate_mpi_error(mp_err err)
{
    MP_TO_SEC_ERROR(err);
}

static SECStatus 
dsa_NewKeyExtended(const PQGParams *params, const SECItem * seed,
                   DSAPrivateKey **privKey)
{
    mp_int p, g;
    mp_int x, y;
    mp_err err;
    PLArenaPool *arena;
    DSAPrivateKey *key;
    /* Check args. */
    if (!params || !privKey || !seed || !seed->data) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* Initialize an arena for the DSA key. */
    arena = PORT_NewArena(NSS_FREEBL_DSA_DEFAULT_CHUNKSIZE);
    if (!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    key = (DSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(DSAPrivateKey));
    if (!key) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }
    key->params.arena = arena;
    /* Initialize MPI integers. */
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&g) = 0;
    MP_DIGITS(&x) = 0;
    MP_DIGITS(&y) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&g) );
    CHECK_MPI_OK( mp_init(&x) );
    CHECK_MPI_OK( mp_init(&y) );
    /* Copy over the PQG params */
    CHECK_MPI_OK( SECITEM_CopyItem(arena, &key->params.prime,
                                          &params->prime) );
    CHECK_MPI_OK( SECITEM_CopyItem(arena, &key->params.subPrime,
                                          &params->subPrime) );
    CHECK_MPI_OK( SECITEM_CopyItem(arena, &key->params.base, &params->base) );
    /* Convert stored p, g, and received x into MPI integers. */
    SECITEM_TO_MPINT(params->prime, &p);
    SECITEM_TO_MPINT(params->base,  &g);
    OCTETS_TO_MPINT(seed->data, &x, seed->len);
    /* Store x in private key */
    SECITEM_AllocItem(arena, &key->privateValue, seed->len);
    PORT_Memcpy(key->privateValue.data, seed->data, seed->len);
    /* Compute public key y = g**x mod p */
    CHECK_MPI_OK( mp_exptmod(&g, &x, &p, &y) );
    /* Store y in public key */
    MPINT_TO_SECITEM(&y, &key->publicValue, arena);
    *privKey = key;
    key = NULL;
cleanup:
    mp_clear(&p);
    mp_clear(&g);
    mp_clear(&x);
    mp_clear(&y);
    if (key)
	PORT_FreeArena(key->params.arena, PR_TRUE);
    if (err) {
	translate_mpi_error(err);
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
DSA_NewRandom(PLArenaPool * arena, const SECItem * q, SECItem * seed)
{
    int retries = 10;
    unsigned int i;
    PRBool good;

    if (q == NULL || q->data == NULL || q->len == 0 ||
        (q->data[0] == 0 && q->len == 1)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (!SECITEM_AllocItem(arena, seed, q->len)) {
        return SECFailure;
    }

    do {
	/* Generate seed bytes for x according to FIPS 186-1 appendix 3 */
        if (dsa_GenerateGlobalRandomBytes(q, seed->data, &seed->len,
                                          seed->len)) {
            goto loser;
        }
	/* Disallow values of 0 and 1 for x. */
	good = PR_FALSE;
	for (i = 0; i < seed->len-1; i++) {
	    if (seed->data[i] != 0) {
		good = PR_TRUE;
		break;
	    }
	}
	if (!good && seed->data[i] > 1) {
	    good = PR_TRUE;
	}
    } while (!good && --retries > 0);

    if (!good) {
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
loser:	if (arena != NULL) {
            SECITEM_FreeItem(seed, PR_FALSE);
        }
	return SECFailure;
    }

    return SECSuccess;
}

/*
** Generate and return a new DSA public and private key pair,
**	both of which are encoded into a single DSAPrivateKey struct.
**	"params" is a pointer to the PQG parameters for the domain
**	Uses a random seed.
*/
SECStatus 
DSA_NewKey(const PQGParams *params, DSAPrivateKey **privKey)
{
    SECItem seed;
    SECStatus rv;

    rv = PQG_Check(params);
    if (rv != SECSuccess) {
	return rv;
    }
    seed.data = NULL;

    rv = DSA_NewRandom(NULL, &params->subPrime, &seed);
    if (rv == SECSuccess) {
        if (seed.len != PQG_GetLength(&params->subPrime)) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            rv = SECFailure;
        } else {
            rv = dsa_NewKeyExtended(params, &seed, privKey);
        }
    }
    SECITEM_FreeItem(&seed, PR_FALSE);
    return rv;
}

/* For FIPS compliance testing. Seed must be exactly the size of subPrime  */
SECStatus 
DSA_NewKeyFromSeed(const PQGParams *params, 
                   const unsigned char *seed,
                   DSAPrivateKey **privKey)
{
    SECItem seedItem;
    seedItem.data = (unsigned char*) seed;
    seedItem.len = PQG_GetLength(&params->subPrime);
    return dsa_NewKeyExtended(params, &seedItem, privKey);
}

static SECStatus 
dsa_SignDigest(DSAPrivateKey *key, SECItem *signature, const SECItem *digest,
               const unsigned char *kb)
{
    mp_int p, q, g;  /* PQG parameters */
    mp_int x, k;     /* private key & pseudo-random integer */
    mp_int r, s;     /* tuple (r, s) is signature) */
    mp_int t;        /* holding tmp values */
    mp_err err   = MP_OKAY;
    SECStatus rv = SECSuccess;
    unsigned int dsa_subprime_len, dsa_signature_len, offset;
    SECItem localDigest;
    unsigned char localDigestData[DSA_MAX_SUBPRIME_LEN];
    SECItem t2 = { siBuffer, NULL, 0 };

    /* FIPS-compliance dictates that digest is a SHA hash. */
    /* Check args. */
    if (!key || !signature || !digest) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    dsa_subprime_len = PQG_GetLength(&key->params.subPrime);
    dsa_signature_len = dsa_subprime_len*2;
    if ((signature->len < dsa_signature_len) ||
	(digest->len > HASH_LENGTH_MAX)  ||
	(digest->len < SHA1_LENGTH)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* DSA accepts digests not equal to dsa_subprime_len, if the 
     * digests are greater, then they are truncated to the size of 
     * dsa_subprime_len, using the left most bits. If they are less
     * then they are padded on the left.*/
    PORT_Memset(localDigestData, 0, dsa_subprime_len);
    offset = (digest->len < dsa_subprime_len) ? 
			(dsa_subprime_len - digest->len) : 0;
    PORT_Memcpy(localDigestData+offset, digest->data, 
		dsa_subprime_len - offset);
    localDigest.data = localDigestData;
    localDigest.len = dsa_subprime_len;

    /* Initialize MPI integers. */
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&g) = 0;
    MP_DIGITS(&x) = 0;
    MP_DIGITS(&k) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&s) = 0;
    MP_DIGITS(&t) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&q) );
    CHECK_MPI_OK( mp_init(&g) );
    CHECK_MPI_OK( mp_init(&x) );
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_init(&r) );
    CHECK_MPI_OK( mp_init(&s) );
    CHECK_MPI_OK( mp_init(&t) );
    /*
    ** Convert stored PQG and private key into MPI integers.
    */
    SECITEM_TO_MPINT(key->params.prime,    &p);
    SECITEM_TO_MPINT(key->params.subPrime, &q);
    SECITEM_TO_MPINT(key->params.base,     &g);
    SECITEM_TO_MPINT(key->privateValue,    &x);
    OCTETS_TO_MPINT(kb, &k, dsa_subprime_len);
    /*
    ** FIPS 186-1, Section 5, Step 1
    **
    ** r = (g**k mod p) mod q
    */
    CHECK_MPI_OK( mp_exptmod(&g, &k, &p, &r) ); /* r = g**k mod p */
    CHECK_MPI_OK(     mp_mod(&r, &q, &r) );     /* r = r mod q    */
    /*                                  
    ** FIPS 186-1, Section 5, Step 2
    **
    ** s = (k**-1 * (HASH(M) + x*r)) mod q
    */
    if (DSA_NewRandom(NULL, &key->params.subPrime, &t2) != SECSuccess) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        rv = SECFailure;
        goto cleanup;
    }
    SECITEM_TO_MPINT(t2, &t); /* t <-$ Zq */
    CHECK_MPI_OK( mp_mulmod(&k, &t, &q, &k) );  /* k = k * t mod q */
    CHECK_MPI_OK( mp_invmod(&k, &q, &k) );      /* k = k**-1 mod q */
    CHECK_MPI_OK( mp_mulmod(&k, &t, &q, &k) );  /* k = k * t mod q */
    SECITEM_TO_MPINT(localDigest, &s);          /* s = HASH(M)     */
    CHECK_MPI_OK( mp_mulmod(&x, &r, &q, &x) );  /* x = x * r mod q */
    CHECK_MPI_OK( mp_addmod(&s, &x, &q, &s) );  /* s = s + x mod q */
    CHECK_MPI_OK( mp_mulmod(&s, &k, &q, &s) );  /* s = s * k mod q */
    /*
    ** verify r != 0 and s != 0
    ** mentioned as optional in FIPS 186-1.
    */
    if (mp_cmp_z(&r) == 0 || mp_cmp_z(&s) == 0) {
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
	rv = SECFailure;
	goto cleanup;
    }
    /*
    ** Step 4
    **
    ** Signature is tuple (r, s)
    */
    err = mp_to_fixlen_octets(&r, signature->data, dsa_subprime_len);
    if (err < 0) goto cleanup; 
    err = mp_to_fixlen_octets(&s, signature->data + dsa_subprime_len, 
                                  dsa_subprime_len);
    if (err < 0) goto cleanup; 
    err = MP_OKAY;
    signature->len = dsa_signature_len;
cleanup:
    PORT_Memset(localDigestData, 0, DSA_MAX_SUBPRIME_LEN);
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&g);
    mp_clear(&x);
    mp_clear(&k);
    mp_clear(&r);
    mp_clear(&s);
    mp_clear(&t);
    SECITEM_FreeItem(&t2, PR_FALSE);
    if (err) {
	translate_mpi_error(err);
	rv = SECFailure;
    }
    return rv;
}

/* signature is caller-supplied buffer of at least 40 bytes.
** On input,  signature->len == size of buffer to hold signature.
**            digest->len    == size of digest.
** On output, signature->len == size of signature in buffer.
** Uses a random seed.
*/
SECStatus 
DSA_SignDigest(DSAPrivateKey *key, SECItem *signature, const SECItem *digest)
{
    SECStatus rv;
    int       retries = 10;
    unsigned char kSeed[DSA_MAX_SUBPRIME_LEN];
    unsigned int kSeedLen = 0;
    unsigned int i;
    unsigned int dsa_subprime_len = PQG_GetLength(&key->params.subPrime);
    PRBool    good;

    PORT_SetError(0);
    do {
	rv = dsa_GenerateGlobalRandomBytes(&key->params.subPrime,
                                           kSeed, &kSeedLen, sizeof kSeed);
	if (rv != SECSuccess) 
	    break;
        if (kSeedLen != dsa_subprime_len) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            rv = SECFailure;
            break;
        }
	/* Disallow a value of 0 for k. */
	good = PR_FALSE;
	for (i = 0; i < kSeedLen; i++) {
	    if (kSeed[i] != 0) {
		good = PR_TRUE;
		break;
	    }
	}
	if (!good) {
	    PORT_SetError(SEC_ERROR_NEED_RANDOM);
	    rv = SECFailure;
	    continue;
	}
	rv = dsa_SignDigest(key, signature, digest, kSeed);
    } while (rv != SECSuccess && PORT_GetError() == SEC_ERROR_NEED_RANDOM &&
	     --retries > 0);
    return rv;
}

/* For FIPS compliance testing. Seed must be exactly 20 bytes. */
SECStatus 
DSA_SignDigestWithSeed(DSAPrivateKey * key,
                       SECItem *       signature,
                       const SECItem * digest,
                       const unsigned char * seed)
{
    SECStatus rv;
    rv = dsa_SignDigest(key, signature, digest, seed);
    return rv;
}

/* signature is caller-supplied buffer of at least 20 bytes.
** On input,  signature->len == size of buffer to hold signature.
**            digest->len    == size of digest.
*/
SECStatus 
DSA_VerifyDigest(DSAPublicKey *key, const SECItem *signature, 
                 const SECItem *digest)
{
    /* FIPS-compliance dictates that digest is a SHA hash. */
    mp_int p, q, g;      /* PQG parameters */
    mp_int r_, s_;       /* tuple (r', s') is received signature) */
    mp_int u1, u2, v, w; /* intermediate values used in verification */
    mp_int y;            /* public key */
    mp_err err;
    unsigned int dsa_subprime_len, dsa_signature_len, offset;
    SECItem localDigest;
    unsigned char localDigestData[DSA_MAX_SUBPRIME_LEN];
    SECStatus verified = SECFailure;

    /* Check args. */
    if (!key || !signature || !digest ) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    dsa_subprime_len = PQG_GetLength(&key->params.subPrime);
    dsa_signature_len = dsa_subprime_len*2;
    if ((signature->len != dsa_signature_len) ||
	(digest->len > HASH_LENGTH_MAX)  ||
	(digest->len < SHA1_LENGTH)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* DSA accepts digests not equal to dsa_subprime_len, if the 
     * digests are greater, than they are truncated to the size of 
     * dsa_subprime_len, using the left most bits. If they are less
     * then they are padded on the left.*/
    PORT_Memset(localDigestData, 0, dsa_subprime_len);
    offset = (digest->len < dsa_subprime_len) ? 
			(dsa_subprime_len - digest->len) : 0;
    PORT_Memcpy(localDigestData+offset, digest->data, 
		dsa_subprime_len - offset);
    localDigest.data = localDigestData;
    localDigest.len = dsa_subprime_len;

    /* Initialize MPI integers. */
    MP_DIGITS(&p)  = 0;
    MP_DIGITS(&q)  = 0;
    MP_DIGITS(&g)  = 0;
    MP_DIGITS(&y)  = 0;
    MP_DIGITS(&r_) = 0;
    MP_DIGITS(&s_) = 0;
    MP_DIGITS(&u1) = 0;
    MP_DIGITS(&u2) = 0;
    MP_DIGITS(&v)  = 0;
    MP_DIGITS(&w)  = 0;
    CHECK_MPI_OK( mp_init(&p)  );
    CHECK_MPI_OK( mp_init(&q)  );
    CHECK_MPI_OK( mp_init(&g)  );
    CHECK_MPI_OK( mp_init(&y)  );
    CHECK_MPI_OK( mp_init(&r_) );
    CHECK_MPI_OK( mp_init(&s_) );
    CHECK_MPI_OK( mp_init(&u1) );
    CHECK_MPI_OK( mp_init(&u2) );
    CHECK_MPI_OK( mp_init(&v)  );
    CHECK_MPI_OK( mp_init(&w)  );
    /*
    ** Convert stored PQG and public key into MPI integers.
    */
    SECITEM_TO_MPINT(key->params.prime,    &p);
    SECITEM_TO_MPINT(key->params.subPrime, &q);
    SECITEM_TO_MPINT(key->params.base,     &g);
    SECITEM_TO_MPINT(key->publicValue,     &y);
    /*
    ** Convert received signature (r', s') into MPI integers.
    */
    OCTETS_TO_MPINT(signature->data, &r_, dsa_subprime_len);
    OCTETS_TO_MPINT(signature->data + dsa_subprime_len, &s_, dsa_subprime_len);
    /*
    ** Verify that 0 < r' < q and 0 < s' < q
    */
    if (mp_cmp_z(&r_) <= 0 || mp_cmp_z(&s_) <= 0 ||
        mp_cmp(&r_, &q) >= 0 || mp_cmp(&s_, &q) >= 0) {
	/* err is zero here. */
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	goto cleanup; /* will return verified == SECFailure */
    }
    /*
    ** FIPS 186-1, Section 6, Step 1
    **
    ** w = (s')**-1 mod q
    */
    CHECK_MPI_OK( mp_invmod(&s_, &q, &w) );      /* w = (s')**-1 mod q */
    /*
    ** FIPS 186-1, Section 6, Step 2
    **
    ** u1 = ((Hash(M')) * w) mod q
    */
    SECITEM_TO_MPINT(localDigest, &u1);              /* u1 = HASH(M')     */
    CHECK_MPI_OK( mp_mulmod(&u1, &w, &q, &u1) ); /* u1 = u1 * w mod q */
    /*
    ** FIPS 186-1, Section 6, Step 3
    **
    ** u2 = ((r') * w) mod q
    */
    CHECK_MPI_OK( mp_mulmod(&r_, &w, &q, &u2) );
    /*
    ** FIPS 186-1, Section 6, Step 4
    **
    ** v = ((g**u1 * y**u2) mod p) mod q
    */
    CHECK_MPI_OK( mp_exptmod(&g, &u1, &p, &g) ); /* g = g**u1 mod p */
    CHECK_MPI_OK( mp_exptmod(&y, &u2, &p, &y) ); /* y = y**u2 mod p */
    CHECK_MPI_OK(  mp_mulmod(&g, &y, &p, &v)  ); /* v = g * y mod p */
    CHECK_MPI_OK(     mp_mod(&v, &q, &v)      ); /* v = v mod q     */
    /*
    ** Verification:  v == r'
    */
    if (mp_cmp(&v, &r_)) {
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	verified = SECFailure; /* Signature failed to verify. */
    } else {
	verified = SECSuccess; /* Signature verified. */
    }
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&g);
    mp_clear(&y);
    mp_clear(&r_);
    mp_clear(&s_);
    mp_clear(&u1);
    mp_clear(&u2);
    mp_clear(&v);
    mp_clear(&w);
    if (err) {
	translate_mpi_error(err);
    }
    return verified;
}
