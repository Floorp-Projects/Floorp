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

#include "blapi.h"
#include "secitem.h"
#include "mpi.h"
#include "mpprime.h"
#include "secmpi.h"

#define DH_SECRET_KEY_LEN      20
#define KEA_DERIVED_SECRET_LEN 128

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
    dhparams = (DHParams *)PORT_ArenaAlloc(arena, sizeof(DHParams));
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
    do {
	/* generate a candidate number a in p's field */
	CHECK_SEC_OK( RNG_GenerateGlobalRandomBytes(ab, primeLen) );
	CHECK_MPI_OK( mp_read_unsigned_octets(&a, ab, primeLen) );
	/* force a < P-1 */
	CHECK_MPI_OK( mp_mod(&a, &psub1, &a) );
	/* check that a is in the range [2..P-1] */
	if ( mp_cmp_d(&a, 2) < 0) continue;
	/* check that a**q mod p != 1 */
	CHECK_MPI_OK( mp_exptmod(&a, &q, &p, &test) );
    } while ( mp_cmp_d(&test, 1) == 0 );
    /* a is a generator, finish up */
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
    key = (DHPrivateKey *)PORT_ArenaAlloc(arena, sizeof(DHPrivateKey));
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
    /* Set private key's g */
    CHECK_SEC_OK( SECITEM_CopyItem(arena, &key->base, &params->base) );
    /* Generate private key xa */
    SECITEM_AllocItem(arena, &key->privateValue, DH_SECRET_KEY_LEN);
    RNG_GenerateGlobalRandomBytes(key->privateValue.data, 
                                  key->privateValue.len);
    SECITEM_TO_MPINT( key->privateValue, &xa );
    /* xa < p */
    CHECK_MPI_OK( mp_mod(&xa, &p, &xa) );
    /* Compute public key Ya = g ** xa mod p */
    CHECK_MPI_OK( mp_exptmod(&g, &xa, &p, &Ya) );
    MPINT_TO_SECITEM(&g, &key->publicValue, key->arena);
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
          unsigned int maxOutBytes)
{
    mp_int p, Xa, Yb, ZZ;
    mp_err err = MP_OKAY;
    unsigned int len;
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
    len = mp_unsigned_octet_size(&ZZ);
    /* Take minimum of bytes requested and bytes in shared secret,
    ** if maxOutBytes is 0 take all of the bytes from the prime.
    */
    if (maxOutBytes > 0)
	len = PR_MIN(len, maxOutBytes);
    /* It is the caller's responsibility to free this buffer. */
    SECITEM_AllocItem(NULL, derivedSecret, len);
    err = mp_to_unsigned_octets(&ZZ, derivedSecret->data, len);
    if (err >= 0) err = MP_OKAY;
cleanup:
    mp_clear(&p);
    mp_clear(&Xa);
    mp_clear(&Yb);
    mp_clear(&ZZ);
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
    if (!prime || !public1 || !public2 || !private1 || !private2 ||
        !derivedSecret) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
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
    /* allocate output buffer and return */
    SECITEM_AllocItem(NULL, derivedSecret, KEA_DERIVED_SECRET_LEN);
    err = mp_to_fixlen_octets(&w, derivedSecret->data, KEA_DERIVED_SECRET_LEN);
    if (err > 0) err = MP_OKAY;
cleanup:
    mp_clear(&p);
    mp_clear(&Y);
    mp_clear(&R);
    mp_clear(&r);
    mp_clear(&x);
    mp_clear(&t);
    mp_clear(&u);
    mp_clear(&w);
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
