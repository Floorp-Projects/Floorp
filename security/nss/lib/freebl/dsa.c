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
/* $Id: dsa.c,v 1.18 2005/10/12 00:48:25 wtchang%redhat.com Exp $ */

#include "secerr.h"

#include "prtypes.h"
#include "prinit.h"
#include "blapi.h"
#include "nssilock.h"
#include "secitem.h"
#include "blapi.h"
#include "mpi.h"
#include "secmpi.h"

 /* XXX to be replaced by define in blapit.h */
#define NSS_FREEBL_DSA_DEFAULT_CHUNKSIZE 2048

/* DSA-specific random number function defined in prng_fips1861.c. */
extern SECStatus 
DSA_GenerateGlobalRandomBytes(void *dest, size_t len, const unsigned char *q);

static void translate_mpi_error(mp_err err)
{
    MP_TO_SEC_ERROR(err);
}

SECStatus 
dsa_NewKey(const PQGParams *params, DSAPrivateKey **privKey, 
           const unsigned char *xb)
{
    mp_int p, g;
    mp_int x, y;
    mp_err err;
    PRArenaPool *arena;
    DSAPrivateKey *key;
    /* Check args. */
    if (!params || !privKey) {
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
    OCTETS_TO_MPINT(xb, &x, DSA_SUBPRIME_LEN);
    /* Store x in private key */
    SECITEM_AllocItem(arena, &key->privateValue, DSA_SUBPRIME_LEN);
    memcpy(key->privateValue.data, xb, DSA_SUBPRIME_LEN);
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

/*
** Generate and return a new DSA public and private key pair,
**	both of which are encoded into a single DSAPrivateKey struct.
**	"params" is a pointer to the PQG parameters for the domain
**	Uses a random seed.
*/
SECStatus 
DSA_NewKey(const PQGParams *params, DSAPrivateKey **privKey)
{
    SECStatus rv;
    unsigned char seed[DSA_SUBPRIME_LEN];
    int retries = 10;
    int i;
    PRBool good;

    do {
	/* Generate seed bytes for x according to FIPS 186-1 appendix 3 */
	if (DSA_GenerateGlobalRandomBytes(seed, DSA_SUBPRIME_LEN,
					  params->subPrime.data))
	    return SECFailure;
	/* Disallow values of 0 and 1 for x. */
	good = PR_FALSE;
	for (i = 0; i < DSA_SUBPRIME_LEN-1; i++) {
	    if (seed[i] != 0) {
		good = PR_TRUE;
		break;
	    }
	}
	if (!good && seed[i] > 1) {
	    good = PR_TRUE;
	}
    } while (!good && --retries > 0);

    if (!good) {
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
	return SECFailure;
    }

    /* Generate a new DSA key using random seed. */
    rv = dsa_NewKey(params, privKey, seed);
    return rv;
}

/* For FIPS compliance testing. Seed must be exactly 20 bytes long */
SECStatus 
DSA_NewKeyFromSeed(const PQGParams *params, 
                   const unsigned char *seed,
                   DSAPrivateKey **privKey)
{
    SECStatus rv;
    rv = dsa_NewKey(params, privKey, seed);
    return rv;
}

static SECStatus 
dsa_SignDigest(DSAPrivateKey *key, SECItem *signature, const SECItem *digest,
               const unsigned char *kb)
{
    mp_int p, q, g;  /* PQG parameters */
    mp_int x, k;     /* private key & pseudo-random integer */
    mp_int r, s;     /* tuple (r, s) is signature) */
    mp_err err   = MP_OKAY;
    SECStatus rv = SECSuccess;

    /* FIPS-compliance dictates that digest is a SHA1 hash. */
    /* Check args. */
    if (!key || !signature || !digest ||
        (signature->len < DSA_SIGNATURE_LEN) ||
	(digest->len != SHA1_LENGTH)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* Initialize MPI integers. */
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&g) = 0;
    MP_DIGITS(&x) = 0;
    MP_DIGITS(&k) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&s) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&q) );
    CHECK_MPI_OK( mp_init(&g) );
    CHECK_MPI_OK( mp_init(&x) );
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_init(&r) );
    CHECK_MPI_OK( mp_init(&s) );
    /*
    ** Convert stored PQG and private key into MPI integers.
    */
    SECITEM_TO_MPINT(key->params.prime,    &p);
    SECITEM_TO_MPINT(key->params.subPrime, &q);
    SECITEM_TO_MPINT(key->params.base,     &g);
    SECITEM_TO_MPINT(key->privateValue,    &x);
    OCTETS_TO_MPINT(kb, &k, DSA_SUBPRIME_LEN);
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
    ** s = (k**-1 * (SHA1(M) + x*r)) mod q
    */
    SECITEM_TO_MPINT(*digest, &s);         /* s = SHA1(M)     */
    CHECK_MPI_OK( mp_invmod(&k, &q, &k) );      /* k = k**-1 mod q */
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
    err = mp_to_fixlen_octets(&r, signature->data, DSA_SUBPRIME_LEN);
    if (err < 0) goto cleanup; 
    err = mp_to_fixlen_octets(&s, signature->data + DSA_SUBPRIME_LEN, 
                                  DSA_SUBPRIME_LEN);
    if (err < 0) goto cleanup; 
    err = MP_OKAY;
    signature->len = DSA_SIGNATURE_LEN;
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&g);
    mp_clear(&x);
    mp_clear(&k);
    mp_clear(&r);
    mp_clear(&s);
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
    unsigned char kSeed[DSA_SUBPRIME_LEN];
    int       i;
    PRBool    good;

    PORT_SetError(0);
    do {
	rv = DSA_GenerateGlobalRandomBytes(kSeed, DSA_SUBPRIME_LEN, 
					   key->params.subPrime.data);
	if (rv != SECSuccess) 
	    break;
	/* Disallow a value of 0 for k. */
	good = PR_FALSE;
	for (i = 0; i < DSA_SUBPRIME_LEN; i++) {
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
    /* FIPS-compliance dictates that digest is a SHA1 hash. */
    mp_int p, q, g;      /* PQG parameters */
    mp_int r_, s_;       /* tuple (r', s') is received signature) */
    mp_int u1, u2, v, w; /* intermediate values used in verification */
    mp_int y;            /* public key */
    mp_err err;
    SECStatus verified = SECFailure;

    /* Check args. */
    if (!key || !signature || !digest ||
        (signature->len != DSA_SIGNATURE_LEN) ||
	(digest->len != SHA1_LENGTH)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
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
    OCTETS_TO_MPINT(signature->data, &r_, DSA_SUBPRIME_LEN);
    OCTETS_TO_MPINT(signature->data + DSA_SUBPRIME_LEN, &s_, DSA_SUBPRIME_LEN);
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
    ** u1 = ((SHA1(M')) * w) mod q
    */
    SECITEM_TO_MPINT(*digest, &u1);              /* u1 = SHA1(M')     */
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
