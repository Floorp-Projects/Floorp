/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Diffie-Hellman parameter generation, key generation, and secret derivation.
 * KEA secret generation and verification.
 *
 * $Id: dh.c,v 1.12 2012/06/14 18:55:10 wtc%google.com Exp $
 */
#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prerr.h"
#include "secerr.h"

#include "blapi.h"
#include "secitem.h"
#include "mpi.h"
#include "mpprime.h"
#include "secmpi.h"

#define KEA_DERIVED_SECRET_LEN 128

/* Lengths are in bytes. */
static unsigned int
dh_GetSecretKeyLen(unsigned int primeLen)
{
    /* Based on Table 2 in NIST SP 800-57. */
    if (primeLen >= 1920) { /* 15360 bits */
        return 64;  /* 512 bits */
    }
    if (primeLen >= 960) { /* 7680 bits */
        return 48;  /* 384 bits */
    }
    if (primeLen >= 384) { /* 3072 bits */
        return 32;  /* 256 bits */
    }
    if (primeLen >= 256) { /* 2048 bits */
        return 28;  /* 224 bits */
    }
    return 20;  /* 160 bits */
}

SECStatus 
DH_GenParam(int primeLen, DHParams **params)
{
    PRArenaPool *arena;
    DHParams *dhparams;
    unsigned char *pb = NULL;
    unsigned char *ab = NULL;
    unsigned long counter = 0;
    mp_int p, q, a, h, psub1, test;
    mp_err err = MP_OKAY;
    SECStatus rv = SECSuccess;
    if (!params || primeLen < 0) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
    if (!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    dhparams = (DHParams *)PORT_ArenaZAlloc(arena, sizeof(DHParams));
    if (!dhparams) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }
    dhparams->arena = arena;
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&h) = 0;
    MP_DIGITS(&psub1) = 0;
    MP_DIGITS(&test) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&q) );
    CHECK_MPI_OK( mp_init(&a) );
    CHECK_MPI_OK( mp_init(&h) );
    CHECK_MPI_OK( mp_init(&psub1) );
    CHECK_MPI_OK( mp_init(&test) );
    /* generate prime with MPI, uses Miller-Rabin to generate strong prime. */
    pb = PORT_Alloc(primeLen);
    CHECK_SEC_OK( RNG_GenerateGlobalRandomBytes(pb, primeLen) );
    pb[0]          |= 0x80; /* set high-order bit */
    pb[primeLen-1] |= 0x01; /* set low-order bit  */
    CHECK_MPI_OK( mp_read_unsigned_octets(&p, pb, primeLen) );
    CHECK_MPI_OK( mpp_make_prime(&p, primeLen * 8, PR_TRUE, &counter) );
    /* construct Sophie-Germain prime q = (p-1)/2. */
    CHECK_MPI_OK( mp_sub_d(&p, 1, &psub1) );
    CHECK_MPI_OK( mp_div_2(&psub1, &q)    );
    /* construct a generator from the prime. */
    ab = PORT_Alloc(primeLen);
    /* generate a candidate number a in p's field */
    CHECK_SEC_OK( RNG_GenerateGlobalRandomBytes(ab, primeLen) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&a, ab, primeLen) );
    /* force a < p (note that quot(a/p) <= 1) */
    if ( mp_cmp(&a, &p) > 0 )
	CHECK_MPI_OK( mp_sub(&a, &p, &a) );
    do {
	/* check that a is in the range [2..p-1] */
	if ( mp_cmp_d(&a, 2) < 0 || mp_cmp(&a, &psub1) >= 0) {
	    /* a is outside of the allowed range.  Set a=3 and keep going. */
            mp_set(&a, 3);
	}
	/* if a**q mod p != 1 then a is a generator */
	CHECK_MPI_OK( mp_exptmod(&a, &q, &p, &test) );
	if ( mp_cmp_d(&test, 1) != 0 )
	    break;
	/* increment the candidate and try again. */
	CHECK_MPI_OK( mp_add_d(&a, 1, &a) );
    } while (PR_TRUE);
    MPINT_TO_SECITEM(&p, &dhparams->prime, arena);
    MPINT_TO_SECITEM(&a, &dhparams->base, arena);
    *params = dhparams;
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&a);
    mp_clear(&h);
    mp_clear(&psub1);
    mp_clear(&test);
    if (pb) PORT_ZFree(pb, primeLen);
    if (ab) PORT_ZFree(ab, primeLen);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    if (rv)
	PORT_FreeArena(arena, PR_TRUE);
    return rv;
}

SECStatus 
DH_NewKey(DHParams *params, DHPrivateKey **privKey)
{
    PRArenaPool *arena;
    DHPrivateKey *key;
    mp_int g, xa, p, Ya;
    mp_err   err = MP_OKAY;
    SECStatus rv = SECSuccess;
    if (!params || !privKey) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
    if (!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    key = (DHPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(DHPrivateKey));
    if (!key) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }
    key->arena = arena;
    MP_DIGITS(&g)  = 0;
    MP_DIGITS(&xa) = 0;
    MP_DIGITS(&p)  = 0;
    MP_DIGITS(&Ya) = 0;
    CHECK_MPI_OK( mp_init(&g)  );
    CHECK_MPI_OK( mp_init(&xa) );
    CHECK_MPI_OK( mp_init(&p)  );
    CHECK_MPI_OK( mp_init(&Ya) );
    /* Set private key's p */
    CHECK_SEC_OK( SECITEM_CopyItem(arena, &key->prime, &params->prime) );
    SECITEM_TO_MPINT(key->prime, &p);
    /* Set private key's g */
    CHECK_SEC_OK( SECITEM_CopyItem(arena, &key->base, &params->base) );
    SECITEM_TO_MPINT(key->base, &g);
    /* Generate private key xa */
    SECITEM_AllocItem(arena, &key->privateValue,
                      dh_GetSecretKeyLen(params->prime.len));
    RNG_GenerateGlobalRandomBytes(key->privateValue.data, 
                                  key->privateValue.len);
    SECITEM_TO_MPINT( key->privateValue, &xa );
    /* xa < p */
    CHECK_MPI_OK( mp_mod(&xa, &p, &xa) );
    /* Compute public key Ya = g ** xa mod p */
    CHECK_MPI_OK( mp_exptmod(&g, &xa, &p, &Ya) );
    MPINT_TO_SECITEM(&Ya, &key->publicValue, key->arena);
    *privKey = key;
cleanup:
    mp_clear(&g);
    mp_clear(&xa);
    mp_clear(&p);
    mp_clear(&Ya);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    if (rv)
	PORT_FreeArena(arena, PR_TRUE);
    return rv;
}

SECStatus 
DH_Derive(SECItem *publicValue, 
          SECItem *prime, 
          SECItem *privateValue, 
          SECItem *derivedSecret, 
          unsigned int outBytes)
{
    mp_int p, Xa, Yb, ZZ;
    mp_err err = MP_OKAY;
    int len = 0;
    unsigned int nb;
    unsigned char *secret = NULL;
    if (!publicValue || !prime || !privateValue || !derivedSecret) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    memset(derivedSecret, 0, sizeof *derivedSecret);
    MP_DIGITS(&p)  = 0;
    MP_DIGITS(&Xa) = 0;
    MP_DIGITS(&Yb) = 0;
    MP_DIGITS(&ZZ) = 0;
    CHECK_MPI_OK( mp_init(&p)  );
    CHECK_MPI_OK( mp_init(&Xa) );
    CHECK_MPI_OK( mp_init(&Yb) );
    CHECK_MPI_OK( mp_init(&ZZ) );
    SECITEM_TO_MPINT(*publicValue,  &Yb);
    SECITEM_TO_MPINT(*privateValue, &Xa);
    SECITEM_TO_MPINT(*prime,        &p);
    /* ZZ = (Yb)**Xa mod p */
    CHECK_MPI_OK( mp_exptmod(&Yb, &Xa, &p, &ZZ) );
    /* number of bytes in the derived secret */
    len = mp_unsigned_octet_size(&ZZ);
    if (len <= 0) {
        err = MP_BADARG;
        goto cleanup;
    }
    /* allocate a buffer which can hold the entire derived secret. */
    secret = PORT_Alloc(len);
    /* grab the derived secret */
    err = mp_to_unsigned_octets(&ZZ, secret, len);
    if (err >= 0) err = MP_OKAY;
    /* 
    ** if outBytes is 0 take all of the bytes from the derived secret.
    ** if outBytes is not 0 take exactly outBytes from the derived secret, zero
    ** pad at the beginning if necessary, and truncate beginning bytes 
    ** if necessary.
    */
    if (outBytes > 0)
	nb = outBytes;
    else
	nb = len;
    SECITEM_AllocItem(NULL, derivedSecret, nb);
    if (len < nb) {
	unsigned int offset = nb - len;
	memset(derivedSecret->data, 0, offset);
	memcpy(derivedSecret->data + offset, secret, len);
    } else {
	memcpy(derivedSecret->data, secret + len - nb, nb);
    }
cleanup:
    mp_clear(&p);
    mp_clear(&Xa);
    mp_clear(&Yb);
    mp_clear(&ZZ);
    if (secret) {
	/* free the buffer allocated for the full secret. */
	PORT_ZFree(secret, len);
    }
    if (err) {
	MP_TO_SEC_ERROR(err);
	if (derivedSecret->data) 
	    PORT_ZFree(derivedSecret->data, derivedSecret->len);
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus 
KEA_Derive(SECItem *prime, 
           SECItem *public1, 
           SECItem *public2, 
           SECItem *private1, 
           SECItem *private2,
           SECItem *derivedSecret)
{
    mp_int p, Y, R, r, x, t, u, w;
    mp_err err;
    unsigned char *secret = NULL;
    unsigned int len = 0, offset;
    if (!prime || !public1 || !public2 || !private1 || !private2 ||
        !derivedSecret) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    memset(derivedSecret, 0, sizeof *derivedSecret);
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&Y) = 0;
    MP_DIGITS(&R) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&x) = 0;
    MP_DIGITS(&t) = 0;
    MP_DIGITS(&u) = 0;
    MP_DIGITS(&w) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&Y) );
    CHECK_MPI_OK( mp_init(&R) );
    CHECK_MPI_OK( mp_init(&r) );
    CHECK_MPI_OK( mp_init(&x) );
    CHECK_MPI_OK( mp_init(&t) );
    CHECK_MPI_OK( mp_init(&u) );
    CHECK_MPI_OK( mp_init(&w) );
    SECITEM_TO_MPINT(*prime,    &p);
    SECITEM_TO_MPINT(*public1,  &Y);
    SECITEM_TO_MPINT(*public2,  &R);
    SECITEM_TO_MPINT(*private1, &r);
    SECITEM_TO_MPINT(*private2, &x);
    /* t = DH(Y, r, p) = Y ** r mod p */
    CHECK_MPI_OK( mp_exptmod(&Y, &r, &p, &t) );
    /* u = DH(R, x, p) = R ** x mod p */
    CHECK_MPI_OK( mp_exptmod(&R, &x, &p, &u) );
    /* w = (t + u) mod p */
    CHECK_MPI_OK( mp_addmod(&t, &u, &p, &w) );
    /* allocate a buffer for the full derived secret */
    len = mp_unsigned_octet_size(&w);
    secret = PORT_Alloc(len);
    /* grab the secret */
    err = mp_to_unsigned_octets(&w, secret, len);
    if (err > 0) err = MP_OKAY;
    /* allocate output buffer */
    SECITEM_AllocItem(NULL, derivedSecret, KEA_DERIVED_SECRET_LEN);
    memset(derivedSecret->data, 0, derivedSecret->len);
    /* copy in the 128 lsb of the secret */
    if (len >= KEA_DERIVED_SECRET_LEN) {
	memcpy(derivedSecret->data, secret + (len - KEA_DERIVED_SECRET_LEN),
	       KEA_DERIVED_SECRET_LEN);
    } else {
	offset = KEA_DERIVED_SECRET_LEN - len;
	memcpy(derivedSecret->data + offset, secret, len);
    }
cleanup:
    mp_clear(&p);
    mp_clear(&Y);
    mp_clear(&R);
    mp_clear(&r);
    mp_clear(&x);
    mp_clear(&t);
    mp_clear(&u);
    mp_clear(&w);
    if (secret)
	PORT_ZFree(secret, len);
    if (err) {
	MP_TO_SEC_ERROR(err);
	return SECFailure;
    }
    return SECSuccess;
}

PRBool 
KEA_Verify(SECItem *Y, SECItem *prime, SECItem *subPrime)
{
    mp_int p, q, y, r;
    mp_err err;
    int cmp = 1;  /* default is false */
    if (!Y || !prime || !subPrime) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&y) = 0;
    MP_DIGITS(&r) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&q) );
    CHECK_MPI_OK( mp_init(&y) );
    CHECK_MPI_OK( mp_init(&r) );
    SECITEM_TO_MPINT(*prime,    &p);
    SECITEM_TO_MPINT(*subPrime, &q);
    SECITEM_TO_MPINT(*Y,        &y);
    /* compute r = y**q mod p */
    CHECK_MPI_OK( mp_exptmod(&y, &q, &p, &r) );
    /* compare to 1 */
    cmp = mp_cmp_d(&r, 1);
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&y);
    mp_clear(&r);
    if (err) {
	MP_TO_SEC_ERROR(err);
	return PR_FALSE;
    }
    return (cmp == 0) ? PR_TRUE : PR_FALSE;
}
