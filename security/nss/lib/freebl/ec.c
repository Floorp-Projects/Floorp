/*
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
 * The Original Code is the Elliptic Curve Cryptography library.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems, Inc. are Copyright (C) 2003
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *	Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
 */

#include "blapi.h"
#include "prerr.h"
#include "secerr.h"
#include "secmpi.h"
#include "secitem.h"
#include "ec.h"
#include "GFp_ecl.h"

#ifdef NSS_ENABLE_ECC

/* 
 * Returns true if pointP is the point at infinity, false otherwise
 */
PRBool
ec_point_at_infinity(SECItem *pointP)
{
    int i;

    for (i = 1; i < pointP->len; i++) {
	if (pointP->data[i] != 0x00) return PR_FALSE;
    }

    return PR_TRUE;
}

/* 
 * Computes point addition R = P + Q for the curve whose 
 * parameters are encoded in params. Two or more of P, Q, 
 * R may point to the same memory location.
 */
SECStatus
ec_point_add(ECParams *params, SECItem *pointP,
             SECItem *pointQ, SECItem *pointR)
{
    mp_int Px, Py, Qx, Qy, Rx, Ry;
    mp_int prime, a;
    SECStatus rv = SECFailure;
    mp_err err = MP_OKAY;
    int len;

#if EC_DEBUG
    int i;

    printf("ec_point_add: params [len=%d]:", params->DEREncoding.len);
    for (i = 0; i < params->DEREncoding.len; i++) 
	    printf("%02x:", params->DEREncoding.data[i]);
    printf("\n");

    printf("ec_point_add: pointP [len=%d]:", pointP->len);
    for (i = 0; i < pointP->len; i++) 
	    printf("%02x:", pointP->data[i]);
    printf("\n");

    printf("ec_point_add: pointQ [len=%d]:", pointQ->len);
    for (i = 0; i < pointQ->len; i++) 
	    printf("%02x:", pointQ->data[i]);
    printf("\n");
#endif

    /* NOTE: We only support prime field curves for now */
    len = (params->fieldID.size + 7) >> 3;
    if ((pointP->data[0] != EC_POINT_FORM_UNCOMPRESSED) ||
	(pointP->len != (2 * len + 1)) ||
	(pointQ->data[0] != EC_POINT_FORM_UNCOMPRESSED) ||
	(pointQ->len != (2 * len + 1))) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    MP_DIGITS(&Px) = 0;
    MP_DIGITS(&Py) = 0;
    MP_DIGITS(&Qx) = 0;
    MP_DIGITS(&Qy) = 0;
    MP_DIGITS(&Rx) = 0;
    MP_DIGITS(&Ry) = 0;
    MP_DIGITS(&prime) = 0;
    MP_DIGITS(&a) = 0;
    CHECK_MPI_OK( mp_init(&Px) );
    CHECK_MPI_OK( mp_init(&Py) );
    CHECK_MPI_OK( mp_init(&Qx) );
    CHECK_MPI_OK( mp_init(&Qy) );
    CHECK_MPI_OK( mp_init(&Rx) );
    CHECK_MPI_OK( mp_init(&Ry) );
    CHECK_MPI_OK( mp_init(&prime) );
    CHECK_MPI_OK( mp_init(&a) );

    /* Initialize Px and Py */
    CHECK_MPI_OK( mp_read_unsigned_octets(&Px, pointP->data + 1, 
	                                  (mp_size) len) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&Py, pointP->data + 1 + len, 
	                                  (mp_size) len) );

    /* Initialize Qx and Qy */
    CHECK_MPI_OK( mp_read_unsigned_octets(&Qx, pointQ->data + 1, 
	                                  (mp_size) len) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&Qy, pointQ->data + 1 + len, 
	                                  (mp_size) len) );

    /* Set up the prime and curve coefficient */
    SECITEM_TO_MPINT( params->fieldID.u.prime, &prime );
    SECITEM_TO_MPINT( params->curve.a, &a );

    /* Compute R = P + Q */
    if (GFp_ec_pt_add(&prime, &a, &Px, &Py, &Qx, &Qy, 
	    &Rx, &Ry) != SECSuccess)
	    goto cleanup;

    /* Construct the SECItem representation of the result */
    pointR->data[0] = EC_POINT_FORM_UNCOMPRESSED;
    CHECK_MPI_OK( mp_to_fixlen_octets(&Rx, pointR->data + 1,
	                              (mp_size) len) );
    CHECK_MPI_OK( mp_to_fixlen_octets(&Ry, pointR->data + 1 + len,
	                              (mp_size) len) );
    rv = SECSuccess;

#if EC_DEBUG
    printf("ec_point_add: pointR [len=%d]:", pointR->len);
    for (i = 0; i < pointR->len; i++) 
	    printf("%02x:", pointR->data[i]);
    printf("\n");
#endif

cleanup:
    mp_clear(&Px);
    mp_clear(&Py);
    mp_clear(&Qx);
    mp_clear(&Qy);
    mp_clear(&Rx);
    mp_clear(&Ry);
    mp_clear(&prime);
    mp_clear(&a);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }

    return rv;
}

/* 
 * Computes scalar point multiplication pointQ = k * pointP for
 * the curve whose parameters are encoded in params.
 */
SECStatus
ec_point_mul(ECParams *params, mp_int *k,
             SECItem *pointP, SECItem *pointQ)
{
    mp_int Px, Py, Qx, Qy;
    mp_int prime, a, b;
    SECStatus rv = SECFailure;
    mp_err err = MP_OKAY;
    int len;

#if EC_DEBUG
    int i;
    char mpstr[256];

    printf("ec_point_mul: params [len=%d]:", params->DEREncoding.len);
    for (i = 0; i < params->DEREncoding.len; i++) 
	    printf("%02x:", params->DEREncoding.data[i]);
    printf("\n");

    mp_tohex(k, mpstr);
    printf("ec_point_mul: scalar : %s\n", mpstr);
    mp_todecimal(k, mpstr);
    printf("ec_point_mul: scalar : %s (dec)\n", mpstr);

    printf("ec_point_mul: pointP [len=%d]:", pointP->len);
    for (i = 0; i < pointP->len; i++) 
	    printf("%02x:", pointP->data[i]);
    printf("\n");
#endif

    /* NOTE: We only support prime field curves for now */
    len = (params->fieldID.size + 7) >> 3;
    if ((params->fieldID.type != ec_field_GFp) ||
	(pointP->data[0] != EC_POINT_FORM_UNCOMPRESSED) ||
	(pointP->len != (2 * len + 1))) {
	    return SECFailure;
    };

    MP_DIGITS(&Px) = 0;
    MP_DIGITS(&Py) = 0;
    MP_DIGITS(&Qx) = 0;
    MP_DIGITS(&Qy) = 0;
    MP_DIGITS(&prime) = 0;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&b) = 0;
    CHECK_MPI_OK( mp_init(&Px) );
    CHECK_MPI_OK( mp_init(&Py) );
    CHECK_MPI_OK( mp_init(&Qx) );
    CHECK_MPI_OK( mp_init(&Qy) );
    CHECK_MPI_OK( mp_init(&prime) );
    CHECK_MPI_OK( mp_init(&a) );
    CHECK_MPI_OK( mp_init(&b) );


    /* Initialize Px and Py */
    CHECK_MPI_OK( mp_read_unsigned_octets(&Px, pointP->data + 1, 
	                                  (mp_size) len) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&Py, pointP->data + 1 + len, 
	                                  (mp_size) len) );

    /* Set up mp_ints containing the prime and curve coefficients */
    SECITEM_TO_MPINT( params->fieldID.u.prime, &prime );
    SECITEM_TO_MPINT( params->curve.a, &a );
    SECITEM_TO_MPINT( params->curve.b, &b );

    /* Compute Q = k * P */
    if (GFp_ec_pt_mul(&prime, &a, &b, &Px, &Py, k, 
	&Qx, &Qy) != SECSuccess) 
	    goto cleanup;

    /* Construct the SECItem representation of point Q */
    pointQ->data[0] = EC_POINT_FORM_UNCOMPRESSED;
    CHECK_MPI_OK( mp_to_fixlen_octets(&Qx, pointQ->data + 1,
	                              (mp_size) len) );
    CHECK_MPI_OK( mp_to_fixlen_octets(&Qy, pointQ->data + 1 + len,
	                              (mp_size) len) );
    rv = SECSuccess;

#if EC_DEBUG
    printf("ec_point_mul: pointQ [len=%d]:", pointQ->len);
    for (i = 0; i < pointQ->len; i++) 
	    printf("%02x:", pointQ->data[i]);
    printf("\n");
#endif

cleanup:
    mp_clear(&Px);
    mp_clear(&Py);
    mp_clear(&Qx);
    mp_clear(&Qy);
    mp_clear(&prime);
    mp_clear(&a);
    mp_clear(&b);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }

    return rv;
}

static unsigned char bitmask[] = {
	0xff, 0x7f, 0x3f, 0x1f,
	0x0f, 0x07, 0x03, 0x01
};
#endif /* NSS_ENABLE_ECC */

/* Generates a new EC key pair. The private key is a supplied
 * random value (in seed) and the public key is the result of 
 * performing a scalar point multiplication of that value with 
 * the curve's base point.
 */
SECStatus 
EC_NewKeyFromSeed(ECParams *ecParams, ECPrivateKey **privKey, 
    const unsigned char *seed, int seedlen)
{
    SECStatus rv = SECFailure;
#ifdef NSS_ENABLE_ECC
    PRArenaPool *arena;
    ECPrivateKey *key;
    mp_int k;
    mp_err err = MP_OKAY;
    int len;

#if EC_DEBUG
    printf("EC_NewKeyFromSeed called\n");
#endif

    if (!ecParams || !privKey || !seed || (seedlen < 0)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* Initialize an arena for the EC key. */
    if (!(arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE)))
	return SECFailure;

    key = (ECPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(ECPrivateKey));
    if (!key) {
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }


    /* Copy all of the fields from the ECParams argument to the
     * ECParams structure within the private key.
     */
    key->ecParams.arena = arena;
    key->ecParams.type = ecParams->type;
    key->ecParams.fieldID.size = ecParams->fieldID.size;
    key->ecParams.fieldID.type = ecParams->fieldID.type;
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.fieldID.u.prime,
	&ecParams->fieldID.u.prime));
    key->ecParams.fieldID.k1 = ecParams->fieldID.k1;
    key->ecParams.fieldID.k2 = ecParams->fieldID.k2;
    key->ecParams.fieldID.k3 = ecParams->fieldID.k3;
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.curve.a,
	&ecParams->curve.a));
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.curve.b,
	&ecParams->curve.b));
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.curve.seed,
	&ecParams->curve.seed));
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.base,
	&ecParams->base));
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.order,
	&ecParams->order));
    key->ecParams.cofactor = ecParams->cofactor;
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.DEREncoding,
	&ecParams->DEREncoding));

    len = (ecParams->fieldID.size + 7) >> 3;  
    SECITEM_AllocItem(arena, &key->privateValue, len);
    SECITEM_AllocItem(arena, &key->publicValue, 2*len + 1);

    /* Copy private key */
    if (seedlen >= len) {
	memcpy(key->privateValue.data, seed, len);
    } else {
	memset(key->privateValue.data, 0, (len - seedlen));
	memcpy(key->privateValue.data + (len - seedlen), seed, seedlen);
    }

    /* Compute corresponding public key */
    MP_DIGITS(&k) = 0;
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&k, key->privateValue.data, 
	(mp_size) len) );

    rv = ec_point_mul(ecParams, &k, &(ecParams->base), &(key->publicValue));
    if (rv != SECSuccess) goto cleanup;
    *privKey = key;

cleanup:
    mp_clear(&k);
    if (rv)
	PORT_FreeArena(arena, PR_TRUE);

#if EC_DEBUG
    printf("EC_NewKeyFromSeed returning %s\n", 
	(rv == SECSuccess) ? "success" : "failure");
#endif
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
#endif /* NSS_ENABLE_ECC */

    return rv;

}

/* Generates a new EC key pair. The private key is a random value and
 * the public key is the result of performing a scalar point multiplication
 * of that value with the curve's base point.
 */
SECStatus 
EC_NewKey(ECParams *ecParams, ECPrivateKey **privKey)
{
    SECStatus rv = SECFailure;
#ifdef NSS_ENABLE_ECC
    int len;
    unsigned char *seed;

    if (!ecParams || !privKey) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* Generate random private key */
    len = (ecParams->fieldID.size + 7) >> 3;
    if ((seed = PORT_Alloc(len)) == NULL) goto cleanup;
    if (RNG_GenerateGlobalRandomBytes(seed, len) != SECSuccess) goto cleanup;

    /* Fit private key to the field size */
    seed[0] &= bitmask[len * 8 - ecParams->fieldID.size];
    rv = EC_NewKeyFromSeed(ecParams, privKey, seed, len);

cleanup:
    if (!seed) {
	PORT_ZFree(seed, len);
    }
#if EC_DEBUG
    printf("EC_NewKey returning %s\n", 
	(rv == SECSuccess) ? "success" : "failure");
#endif
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
#endif /* NSS_ENABLE_ECC */
    
    return rv;
}

/* Validates an EC public key as described in Section 5.2.2 of
 * X9.63. The ECDH primitive when used without the cofactor does
 * not address small subgroup attacks, which may occur when the
 * public key is not valid. These attacks can be prevented by 
 * validating the public key before using ECDH.
 */
SECStatus 
EC_ValidatePublicKey(ECParams *ecParams, SECItem *publicValue)
{
#ifdef NSS_ENABLE_ECC
    if (!ecParams || !publicValue) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* XXX Add actual checks here. */
    return SECSuccess;
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
    return SECFailure;
#endif /* NSS_ENABLE_ECC */
}

/* 
** Performs an ECDH key derivation by computing the scalar point
** multiplication of privateValue and publicValue (with or without the
** cofactor) and returns the x-coordinate of the resulting elliptic
** curve point in derived secret.  If successful, derivedSecret->data
** is set to the address of the newly allocated buffer containing the
** derived secret, and derivedSecret->len is the size of the secret
** produced. It is the caller's responsibility to free the allocated
** buffer containing the derived secret.
*/
SECStatus 
ECDH_Derive(SECItem  *publicValue, 
            ECParams *ecParams,
            SECItem  *privateValue,
            PRBool    withCofactor,
            SECItem  *derivedSecret)
{
    SECStatus rv = SECFailure;
#ifdef NSS_ENABLE_ECC
    unsigned int len = 0;
    SECItem pointQ = {siBuffer, NULL, 0};
    mp_int k; /* to hold the private value */
    mp_int cofactor;
    mp_err err = MP_OKAY;
#if EC_DEBUG
    int i;
#endif

    if (!publicValue || !ecParams || !privateValue || 
	!derivedSecret) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    memset(derivedSecret, 0, sizeof *derivedSecret);
    len = (ecParams->fieldID.size + 7) >> 3;  
    pointQ.len = 2*len + 1;
    if ((pointQ.data = PORT_Alloc(2*len + 1)) == NULL) goto cleanup;

    MP_DIGITS(&k) = 0;
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&k, privateValue->data, 
	                                  (mp_size) privateValue->len) );

    if (withCofactor && (ecParams->cofactor != 1)) {
	    /* multiply k with the cofactor */
	    MP_DIGITS(&cofactor) = 0;
	    CHECK_MPI_OK( mp_init(&cofactor) );
	    mp_set(&cofactor, ecParams->cofactor);
	    CHECK_MPI_OK( mp_mul(&k, &cofactor, &k) );
    }

    /* Multiply our private key and peer's public point */
    if ((ec_point_mul(ecParams, &k, publicValue, &pointQ) != SECSuccess) ||
	ec_point_at_infinity(&pointQ))
	goto cleanup;

    /* Allocate memory for the derived secret and copy
     * the x co-ordinate of pointQ into it.
     */
    SECITEM_AllocItem(NULL, derivedSecret, len);
    memcpy(derivedSecret->data, pointQ.data + 1, len);

    rv = SECSuccess;

#if EC_DEBUG
    printf("derived_secret:\n");
    for (i = 0; i < derivedSecret->len; i++) 
	printf("%02x:", derivedSecret->data[i]);
    printf("\n");
#endif

cleanup:
    mp_clear(&k);

    if (pointQ.data) {
	PORT_ZFree(pointQ.data, 2*len + 1);
    }
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
#endif /* NSS_ENABLE_ECC */

    return rv;
}

/* Computes the ECDSA signature (a concatenation of two values r and s)
 * on the digest using the given key and the random value kb (used in
 * computing s).
 */
SECStatus 
ECDSA_SignDigestWithSeed(ECPrivateKey *key, SECItem *signature, 
    const SECItem *digest, const unsigned char *kb, const int kblen)
{
    SECStatus rv = SECFailure;
#ifdef NSS_ENABLE_ECC
    mp_int x1;
    mp_int d, k;     /* private key, random integer */
    mp_int r, s;     /* tuple (r, s) is the signature */
    mp_int n;
    mp_err err = MP_OKAY;
    ECParams *ecParams = NULL;
    SECItem kGpoint = { siBuffer, NULL, 0};
    int len = 0;

#if EC_DEBUG
    char mpstr[256];
#endif

    /* Check args */
    if (!key || !signature || !digest || !kb || (kblen < 0) ||
	(digest->len != SHA1_LENGTH)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto cleanup;
    }

    ecParams = &(key->ecParams);
    len = (ecParams->fieldID.size + 7) >> 3;  
    if (signature->len < 2*len) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto cleanup;
    }

    /* Initialize MPI integers. */
    MP_DIGITS(&x1) = 0;
    MP_DIGITS(&d) = 0;
    MP_DIGITS(&k) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&s) = 0;
    MP_DIGITS(&n) = 0;
    CHECK_MPI_OK( mp_init(&x1) );
    CHECK_MPI_OK( mp_init(&d) );
    CHECK_MPI_OK( mp_init(&k) );
    CHECK_MPI_OK( mp_init(&r) );
    CHECK_MPI_OK( mp_init(&s) );
    CHECK_MPI_OK( mp_init(&n) );

    SECITEM_TO_MPINT( ecParams->order, &n );
    SECITEM_TO_MPINT( key->privateValue, &d );
    CHECK_MPI_OK( mp_read_unsigned_octets(&k, kb, kblen) );
    /* Make sure k is in the interval [1, n-1] */
    if ((mp_cmp_z(&k) <= 0) || (mp_cmp(&k, &n) >= 0)) {
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
	goto cleanup;
    }

    /* 
    ** ANSI X9.62, Section 5.3.2, Step 2
    **
    ** Compute kG
    */
    kGpoint.len = 2*len + 1;
    kGpoint.data = PORT_Alloc(2*len + 1);
    if ((kGpoint.data == NULL) ||
	(ec_point_mul(ecParams, &k, &(ecParams->base), &kGpoint)
	    != SECSuccess))
	goto cleanup;

    /* 
    ** ANSI X9.62, Section 5.3.3, Step 1
    **
    ** Extract the x co-ordinate of kG into x1
    */
    CHECK_MPI_OK( mp_read_unsigned_octets(&x1, kGpoint.data + 1, 
	                                  (mp_size) len) );

    /* 
    ** ANSI X9.62, Section 5.3.3, Step 2
    **
    ** r = x1 mod n  NOTE: n is the order of the curve
    */
    CHECK_MPI_OK( mp_mod(&x1, &n, &r) );

    /*
    ** ANSI X9.62, Section 5.3.3, Step 3
    **
    ** verify r != 0 
    */
    if (mp_cmp_z(&r) == 0) {
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
	goto cleanup;
    }

    /*                                  
    ** ANSI X9.62, Section 5.3.3, Step 4
    **
    ** s = (k**-1 * (SHA1(M) + d*r)) mod n 
    */
    SECITEM_TO_MPINT(*digest, &s);        /* s = SHA1(M)     */

#if EC_DEBUG
    mp_todecimal(&n, mpstr);
    printf("n : %s (dec)\n", mpstr);
    mp_todecimal(&d, mpstr);
    printf("d : %s (dec)\n", mpstr);
    mp_tohex(&x1, mpstr);
    printf("x1: %s\n", mpstr);
    mp_todecimal(&s, mpstr);
    printf("digest: %s (decimal)\n", mpstr);
    mp_todecimal(&r, mpstr);
    printf("r : %s (dec)\n", mpstr);
#endif

    CHECK_MPI_OK( mp_invmod(&k, &n, &k) );      /* k = k**-1 mod n */
    CHECK_MPI_OK( mp_mulmod(&d, &r, &n, &d) );  /* d = d * r mod n */
    CHECK_MPI_OK( mp_addmod(&s, &d, &n, &s) );  /* s = s + d mod n */
    CHECK_MPI_OK( mp_mulmod(&s, &k, &n, &s) );  /* s = s * k mod n */

#if EC_DEBUG
    mp_todecimal(&s, mpstr);
    printf("s : %s (dec)\n", mpstr);
#endif

    /*
    ** ANSI X9.62, Section 5.3.3, Step 5
    **
    ** verify s != 0
    */
    if (mp_cmp_z(&s) == 0) {
	PORT_SetError(SEC_ERROR_NEED_RANDOM);
	goto cleanup;
    }

   /*
    **
    ** Signature is tuple (r, s)
    */
    CHECK_MPI_OK( mp_to_fixlen_octets(&r, signature->data, len) );
    CHECK_MPI_OK( mp_to_fixlen_octets(&s, signature->data + len, len) );
    signature->len = 2*len;

    rv = SECSuccess;
    err = MP_OKAY;
cleanup:
    mp_clear(&x1);
    mp_clear(&d);
    mp_clear(&k);
    mp_clear(&r);
    mp_clear(&s);
    mp_clear(&n);

    if (kGpoint.data) {
	PORT_ZFree(kGpoint.data, 2*len + 1);
    }

    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }

#if EC_DEBUG
    printf("ECDSA signing with seed %s\n",
	(rv == SECSuccess) ? "succeeded" : "failed");
#endif
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
#endif /* NSS_ENABLE_ECC */

   return rv;
}

/*
** Computes the ECDSA signature on the digest using the given key 
** and a random seed.
*/
SECStatus 
ECDSA_SignDigest(ECPrivateKey *key, SECItem *signature, const SECItem *digest)
{
    SECStatus rv = SECFailure;
#ifdef NSS_ENABLE_ECC
    int prerr = 0;
    int n = (key->ecParams.fieldID.size + 7) >> 3;
    unsigned char mask = bitmask[n * 8 - key->ecParams.fieldID.size];
    unsigned char *kseed = NULL;

    /* Generate random seed of appropriate size as dictated 
     * by field size.
     */
    if ((kseed = PORT_Alloc(n)) == NULL) return SECFailure;

    do {
        if (RNG_GenerateGlobalRandomBytes(kseed, n) != SECSuccess) 
	    goto cleanup;
	*kseed &= mask;
	rv = ECDSA_SignDigestWithSeed(key, signature, digest, kseed, n);
	if (rv) prerr = PORT_GetError();
    } while ((rv != SECSuccess) && (prerr == SEC_ERROR_NEED_RANDOM));

cleanup:    
    if (kseed) PORT_ZFree(kseed, n);

#if EC_DEBUG
    printf("ECDSA signing %s\n",
	(rv == SECSuccess) ? "succeeded" : "failed");
#endif
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
#endif /* NSS_ENABLE_ECC */

    return rv;
}

/*
** Checks the signature on the given digest using the key provided.
*/
SECStatus 
ECDSA_VerifyDigest(ECPublicKey *key, const SECItem *signature, 
                 const SECItem *digest)
{
    SECStatus rv = SECFailure;
#ifdef NSS_ENABLE_ECC
    mp_int r_, s_;           /* tuple (r', s') is received signature) */
    mp_int c, u1, u2, v;     /* intermediate values used in verification */
    mp_int x1, y1;
    mp_int x2, y2;
    mp_int n;
    mp_err err = MP_OKAY;
    PRArenaPool *arena = NULL;
    ECParams *ecParams = NULL;
    SECItem pointA = { siBuffer, NULL, 0 };
    SECItem pointB = { siBuffer, NULL, 0 };
    SECItem pointC = { siBuffer, NULL, 0 };
    int len;

#if EC_DEBUG
    char mpstr[256];
    printf("ECDSA verification called\n");
#endif

    /* Check args */
    if (!key || !signature || !digest ||
	(digest->len != SHA1_LENGTH)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto cleanup;
    }

    ecParams = &(key->ecParams);
    len = (ecParams->fieldID.size + 7) >> 3;  
    printf("len is %d\n", len);
    if (signature->len < 2*len) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto cleanup;
    }

    /* Initialize an arena for pointA, pointB and pointC */
    if ((arena = PORT_NewArena(NSS_FREEBL_DEFAULT_CHUNKSIZE)) == NULL)
	goto cleanup;

    SECITEM_AllocItem(arena, &pointA, 2*len + 1);
    SECITEM_AllocItem(arena, &pointB, 2*len + 1);
    SECITEM_AllocItem(arena, &pointC, 2*len + 1);
    if (pointA.data == NULL || pointB.data == NULL || pointC.data == NULL)
	goto cleanup;

    /* Initialize MPI integers. */
    MP_DIGITS(&r_) = 0;
    MP_DIGITS(&s_) = 0;
    MP_DIGITS(&c) = 0;
    MP_DIGITS(&u1) = 0;
    MP_DIGITS(&u2) = 0;
    MP_DIGITS(&x1) = 0;
    MP_DIGITS(&y1) = 0;
    MP_DIGITS(&x2) = 0;
    MP_DIGITS(&y2) = 0;
    MP_DIGITS(&v)  = 0;
    MP_DIGITS(&n)  = 0;
    CHECK_MPI_OK( mp_init(&r_) );
    CHECK_MPI_OK( mp_init(&s_) );
    CHECK_MPI_OK( mp_init(&c)  );
    CHECK_MPI_OK( mp_init(&u1) );
    CHECK_MPI_OK( mp_init(&u2) );
    CHECK_MPI_OK( mp_init(&x1)  );
    CHECK_MPI_OK( mp_init(&y1)  );
    CHECK_MPI_OK( mp_init(&x2)  );
    CHECK_MPI_OK( mp_init(&y2)  );
    CHECK_MPI_OK( mp_init(&v)  );
    CHECK_MPI_OK( mp_init(&n)  );

    /*
    ** Convert received signature (r', s') into MPI integers.
    */
    CHECK_MPI_OK( mp_read_unsigned_octets(&r_, signature->data, len) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&s_, signature->data + len, len) );
                                          
    /* 
    ** ANSI X9.62, Section 5.4.2, Steps 1 and 2
    **
    ** Verify that 0 < r' < n and 0 < s' < n
    */
    SECITEM_TO_MPINT(ecParams->order, &n);
    if (mp_cmp_z(&r_) <= 0 || mp_cmp_z(&s_) <= 0 ||
        mp_cmp(&r_, &n) >= 0 || mp_cmp(&s_, &n) >= 0)
	goto cleanup; /* will return rv == SECFailure */

    /*
    ** ANSI X9.62, Section 5.4.2, Step 3
    **
    ** c = (s')**-1 mod n
    */
    CHECK_MPI_OK( mp_invmod(&s_, &n, &c) );      /* c = (s')**-1 mod n */

    /*
    ** ANSI X9.62, Section 5.4.2, Step 4
    **
    ** u1 = ((SHA1(M')) * c) mod n
    */
    SECITEM_TO_MPINT(*digest, &u1);         /* u1 = SHA1(M')     */

#if EC_DEBUG
    mp_todecimal(&r_, mpstr);
    printf("r_: %s (dec)\n", mpstr);
    mp_todecimal(&s_, mpstr);
    printf("s_: %s (dec)\n", mpstr);
    mp_todecimal(&c, mpstr);
    printf("c : %s (dec)\n", mpstr);
    mp_todecimal(&u1, mpstr);
    printf("digest: %s (dec)\n", mpstr);
#endif

    CHECK_MPI_OK( mp_mulmod(&u1, &c, &n, &u1) );  /* u1 = u1 * c mod n */

    /*
    ** ANSI X9.62, Section 5.4.2, Step 4
    **
    ** u2 = ((r') * c) mod n
    */
    CHECK_MPI_OK( mp_mulmod(&r_, &c, &n, &u2) );

    /*
    ** ANSI X9.62, Section 5.4.3, Step 1
    **
    ** Compute u1*G + u2*Q
    ** Here, A = u1.G     B = u2.Q    and   C = A + B
    ** If the result, C, is the point at infinity, reject the signature
    */
    if ((ec_point_mul(ecParams, &u1, &ecParams->base, &pointA) 
	    == SECFailure) ||
	(ec_point_mul(ecParams, &u2, &key->publicValue, &pointB) 
	    == SECFailure) ||
	(ec_point_add(ecParams, &pointA, &pointB, &pointC) == SECFailure) ||
	ec_point_at_infinity(&pointC)) {
	    rv = SECFailure;
	    goto cleanup;
    }

    CHECK_MPI_OK( mp_read_unsigned_octets(&x1, pointC.data + 1, len) );

    /*
    ** ANSI X9.62, Section 5.4.4, Step 2
    **
    ** v = x1 mod n
    */
    CHECK_MPI_OK( mp_mod(&x1, &n, &v) );

    /*
    ** ANSI X9.62, Section 5.4.4, Step 3
    **
    ** Verification:  v == r'
    */
    if (mp_cmp(&v, &r_)) {
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	rv = SECFailure; /* Signature failed to verify. */
    } else {
	rv = SECSuccess; /* Signature verified. */
    }

#if EC_DEBUG
    mp_todecimal(&u1, mpstr);
    printf("u1: %s (dec)\n", mpstr);
    mp_todecimal(&u2, mpstr);
    printf("u2: %s (dec)\n", mpstr);
    mp_tohex(&x1, mpstr);
    printf("x1: %s\n", mpstr);
    mp_todecimal(&v, mpstr);
    printf("v : %s (dec)\n", mpstr);
#endif

cleanup:
    mp_clear(&r_);
    mp_clear(&s_);
    mp_clear(&c);
    mp_clear(&u1);
    mp_clear(&u2);
    mp_clear(&x1);
    mp_clear(&y1);
    mp_clear(&x2);
    mp_clear(&y2);
    mp_clear(&v);
    mp_clear(&n);

    if (arena) PORT_FreeArena(arena, PR_TRUE);	
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }

#if EC_DEBUG
    printf("ECDSA verification %s\n",
	(rv == SECSuccess) ? "succeeded" : "failed");
#endif
#else
    PORT_SetError(SEC_ERROR_UNSUPPORTED_KEYALG);
#endif /* NSS_ENABLE_ECC */

    return rv;
}

