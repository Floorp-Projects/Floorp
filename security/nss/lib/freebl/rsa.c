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
 * $Id: rsa.c,v 1.4 2000/09/07 03:14:16 mcgreer%netscape.com Exp $
 */

#include "prerr.h"
#include "secerr.h"

#include "blapi.h"
#include "mpi.h"
#include "mpprime.h"
#include "secmpi.h"
#include "secitem.h"

/*
** RSA encryption/decryption. When encrypting/decrypting the output
** buffer must be at least the size of the public key modulus.
*/

/*
** Generate and return a new RSA public and private key.
**	Both keys are encoded in a single RSAPrivateKey structure.
**	"cx" is the random number generator context
**	"keySizeInBits" is the size of the key to be generated, in bits.
**	   512, 1024, etc.
**	"publicExponent" when not NULL is a pointer to some data that
**	   represents the public exponent to use. The data is a byte
**	   encoded integer, in "big endian" order.
*/
RSAPrivateKey *
RSA_NewKey(int keySizeInBits, SECItem *publicExponent)
{
    unsigned char *pb = NULL, *qb = NULL;
    unsigned int primeLen;
    unsigned long counter;
    mp_int p, q, n, e, d, phi;
    mp_int psub1, qsub1, tmp;
    mp_err   err = MP_OKAY;
    SECStatus rv = SECSuccess;
    RSAPrivateKey *key = NULL;
    PRArenaPool *arena = NULL;
    if (!publicExponent) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }
    /* length of primes p and q (in bytes) */
    primeLen = keySizeInBits / (2 * BITS_PER_BYTE);
    MP_DIGITS(&p) = 0;
    MP_DIGITS(&q) = 0;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&e) = 0;
    MP_DIGITS(&d) = 0;
    MP_DIGITS(&phi) = 0;
    MP_DIGITS(&psub1) = 0;
    MP_DIGITS(&qsub1) = 0;
    MP_DIGITS(&tmp) = 0;
    CHECK_MPI_OK( mp_init(&p) );
    CHECK_MPI_OK( mp_init(&q) );
    CHECK_MPI_OK( mp_init(&n) );
    CHECK_MPI_OK( mp_init(&e) );
    CHECK_MPI_OK( mp_init(&d) );
    CHECK_MPI_OK( mp_init(&phi) );
    CHECK_MPI_OK( mp_init(&psub1) );
    CHECK_MPI_OK( mp_init(&qsub1) );
    CHECK_MPI_OK( mp_init(&tmp) );
    /* 1. Allocate arena & key */
    arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE);
    if (!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }
    key = (RSAPrivateKey *)PORT_ArenaAlloc(arena, sizeof(RSAPrivateKey));
    if (!key) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_FreeArena(arena, PR_TRUE);
	return NULL;
    }
    key->arena = arena;
    /* 2.  Set the version number (PKCS1 v1.5 says it should be zero) */
    SECITEM_AllocItem(arena, &key->version, 1);
    key->version.data[0] = 0;
    /* 3.  Set the public exponent */
    SECITEM_CopyItem(arena, &key->publicExponent, publicExponent);
    SECITEM_TO_MPINT(*publicExponent, &e);
    /* 4.  Generate primes p and q */
    pb = PORT_Alloc(primeLen);
    qb = PORT_Alloc(primeLen);
retry:
    CHECK_SEC_OK( RNG_GenerateGlobalRandomBytes(pb, primeLen) );
    CHECK_SEC_OK( RNG_GenerateGlobalRandomBytes(qb, primeLen) );
    pb[0]          |= 0x80; /* set high-order bit */
    pb[primeLen-1] |= 0x01; /* set low-order bit  */
    qb[0]          |= 0x80; /* set high-order bit */
    qb[primeLen-1] |= 0x01; /* set low-order bit  */
    CHECK_MPI_OK( mp_read_unsigned_octets(&p, pb, primeLen) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&q, qb, primeLen) );
    CHECK_MPI_OK( mpp_make_prime(&p, primeLen * 8, PR_FALSE, &counter) );
    CHECK_MPI_OK( mpp_make_prime(&q, primeLen * 8, PR_FALSE, &counter) );
    MPINT_TO_SECITEM(&p, &key->prime1, arena);
    MPINT_TO_SECITEM(&q, &key->prime2, arena);
    /* 5.  Compute n = p*q */
    CHECK_MPI_OK( mp_mul(&p, &q, &n) );
    MPINT_TO_SECITEM(&n, &key->modulus, arena);
    /* 6.  Compute phi = (p-1)*(q-1) */
    CHECK_MPI_OK( mp_mul(&psub1, &qsub1, &phi) );
    /* 7.  Compute d = e**-1 mod(phi) using extended Euclidean algorithm */
    CHECK_MPI_OK( mp_xgcd(&e, &phi, &tmp, &d, NULL) );
    /*     Verify that phi(n) and e have no common divisors */
    if (mp_cmp_d(&tmp, 1) != 0)
	goto retry;
    MPINT_TO_SECITEM(&d, &key->privateExponent, arena);
    /* 8.  Compute exponent1 = d mod (p-1) */
    CHECK_MPI_OK( mp_sub_d(&p, 1, &psub1) );
    CHECK_MPI_OK( mp_mod(&d, &psub1, &tmp) );
    MPINT_TO_SECITEM(&tmp, &key->exponent1, arena);
    /* 9.  Compute exponent2 = d mod (q-1) */
    CHECK_MPI_OK( mp_sub_d(&q, 1, &qsub1) );
    CHECK_MPI_OK( mp_mod(&d, &qsub1, &tmp) );
    MPINT_TO_SECITEM(&tmp, &key->exponent2, arena);
    /*10.  Compute coefficient = q**-1 mod p */
    CHECK_MPI_OK( mp_invmod(&q, &p, &tmp) );
    MPINT_TO_SECITEM(&tmp, &key->coefficient, arena);
cleanup:
    mp_clear(&p);
    mp_clear(&q);
    mp_clear(&n);
    mp_clear(&e);
    mp_clear(&d);
    mp_clear(&phi);
    mp_clear(&psub1);
    mp_clear(&qsub1);
    mp_clear(&tmp);
    if (pb)
	PORT_ZFree(pb, primeLen);
    if (qb)
	PORT_ZFree(qb, primeLen);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    if (rv && arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }
    return key;
}

static unsigned int
rsa_modulusLen(SECItem *modulus)
{
	unsigned char byteZero = modulus->data[0];
	unsigned int modLen = modulus->len - !byteZero;
	return modLen;
}

/*
** Perform a raw public-key operation 
**	Length of input and output buffers are equal to key's modulus len.
*/
SECStatus 
RSA_PublicKeyOp(RSAPublicKey  *key, 
                unsigned char *output, 
                unsigned char *input)
{
    unsigned int modLen;
    mp_int n, e, m, c;
    mp_err err   = MP_OKAY;
    SECStatus rv = SECSuccess;
    if (!key || !output || !input) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&e) = 0;
    MP_DIGITS(&m) = 0;
    MP_DIGITS(&c) = 0;
    CHECK_MPI_OK( mp_init(&n) );
    CHECK_MPI_OK( mp_init(&e) );
    CHECK_MPI_OK( mp_init(&m) );
    CHECK_MPI_OK( mp_init(&c) );
    modLen = rsa_modulusLen(&key->modulus);
    /* 1.  Obtain public key (n, e) */
    SECITEM_TO_MPINT(key->modulus, &n);
#ifdef USE_MPI_EXPT_D
    /* XXX convert exponent to mp_digit */
#else
    SECITEM_TO_MPINT(key->publicExponent, &e);
#endif
    /* 2.  Represent message as integer in range [0..n-1] */
    CHECK_MPI_OK( mp_read_unsigned_octets(&m, input, modLen) );
    /* 3.  Compute c = m**e mod n */
#ifdef USE_MPI_EXPT_D
    /* XXX see which is faster */
    CHECK_MPI_OK( mp_exptmod_d(&m, exp, &n, &c) );
#else
    CHECK_MPI_OK( mp_exptmod(&m, &e, &n, &c) );
#endif
    /* 4.  result c is ciphertext */
    err = mp_to_unsigned_octets(&c, output, modLen);
    if (err >= 0) err = MP_OKAY;
cleanup:
    mp_clear(&n);
    mp_clear(&e);
    mp_clear(&m);
    mp_clear(&c);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    return rv;
}

/*
** Perform a raw private-key operation 
**	Length of input and output buffers are equal to key's modulus len.
*/
SECStatus 
RSA_PrivateKeyOp(RSAPrivateKey *key, 
                 unsigned char *output, 
                 unsigned char *input)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}
