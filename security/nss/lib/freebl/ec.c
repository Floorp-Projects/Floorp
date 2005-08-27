/*
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
 * The Original Code is the Elliptic Curve Cryptography library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Vipul Gupta <vipul.gupta@sun.com> and
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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

#include "blapi.h"
#include "prerr.h"
#include "secerr.h"
#include "secmpi.h"
#include "secitem.h"
#include "ec.h"
#include "ecl.h"

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
 * Computes scalar point multiplication pointQ = k1 * G + k2 * pointP for
 * the curve whose parameters are encoded in params with base point G.
 */
SECStatus 
ec_points_mul(const ECParams *params, const mp_int *k1, const mp_int *k2,
             const SECItem *pointP, SECItem *pointQ)
{
    mp_int Px, Py, Qx, Qy;
    mp_int Gx, Gy, order, irreducible, a, b;
#if 0 /* currently don't support non-named curves */
    unsigned int irr_arr[5];
#endif
    ECGroup *group = NULL;
    SECStatus rv = SECFailure;
    mp_err err = MP_OKAY;
    int len;

#if EC_DEBUG
    int i;
    char mpstr[256];

    printf("ec_points_mul: params [len=%d]:", params->DEREncoding.len);
    for (i = 0; i < params->DEREncoding.len; i++) 
	    printf("%02x:", params->DEREncoding.data[i]);
    printf("\n");

	if (k1 != NULL) {
		mp_tohex(k1, mpstr);
		printf("ec_points_mul: scalar k1: %s\n", mpstr);
		mp_todecimal(k1, mpstr);
		printf("ec_points_mul: scalar k1: %s (dec)\n", mpstr);
	}

	if (k2 != NULL) {
		mp_tohex(k2, mpstr);
		printf("ec_points_mul: scalar k2: %s\n", mpstr);
		mp_todecimal(k2, mpstr);
		printf("ec_points_mul: scalar k2: %s (dec)\n", mpstr);
	}

	if (pointP != NULL) {
		printf("ec_points_mul: pointP [len=%d]:", pointP->len);
		for (i = 0; i < pointP->len; i++) 
			printf("%02x:", pointP->data[i]);
		printf("\n");
	}
#endif

	/* NOTE: We only support uncompressed points for now */
	len = (params->fieldID.size + 7) >> 3;
	if (pointP != NULL) {
		if ((pointP->data[0] != EC_POINT_FORM_UNCOMPRESSED) ||
			(pointP->len != (2 * len + 1))) {
			return SECFailure;
		};
	}

	MP_DIGITS(&Px) = 0;
	MP_DIGITS(&Py) = 0;
	MP_DIGITS(&Qx) = 0;
	MP_DIGITS(&Qy) = 0;
	MP_DIGITS(&Gx) = 0;
	MP_DIGITS(&Gy) = 0;
	MP_DIGITS(&order) = 0;
	MP_DIGITS(&irreducible) = 0;
	MP_DIGITS(&a) = 0;
	MP_DIGITS(&b) = 0;
	CHECK_MPI_OK( mp_init(&Px) );
	CHECK_MPI_OK( mp_init(&Py) );
	CHECK_MPI_OK( mp_init(&Qx) );
	CHECK_MPI_OK( mp_init(&Qy) );
	CHECK_MPI_OK( mp_init(&Gx) );
	CHECK_MPI_OK( mp_init(&Gy) );
	CHECK_MPI_OK( mp_init(&order) );
	CHECK_MPI_OK( mp_init(&irreducible) );
	CHECK_MPI_OK( mp_init(&a) );
	CHECK_MPI_OK( mp_init(&b) );

	if ((k2 != NULL) && (pointP != NULL)) {
		/* Initialize Px and Py */
		CHECK_MPI_OK( mp_read_unsigned_octets(&Px, pointP->data + 1, (mp_size) len) );
		CHECK_MPI_OK( mp_read_unsigned_octets(&Py, pointP->data + 1 + len, (mp_size) len) );
	}

	/* construct from named params, if possible */
	if (params->name != ECCurve_noName) {
		group = ECGroup_fromName(params->name);
	}

#if 0 /* currently don't support non-named curves */
	if (group == NULL) {
		/* Set up mp_ints containing the curve coefficients */
		CHECK_MPI_OK( mp_read_unsigned_octets(&Gx, params->base.data + 1, 
										  (mp_size) len) );
		CHECK_MPI_OK( mp_read_unsigned_octets(&Gy, params->base.data + 1 + len, 
										  (mp_size) len) );
		SECITEM_TO_MPINT( params->order, &order );
		SECITEM_TO_MPINT( params->curve.a, &a );
		SECITEM_TO_MPINT( params->curve.b, &b );
		if (params->fieldID.type == ec_field_GFp) {
			SECITEM_TO_MPINT( params->fieldID.u.prime, &irreducible );
			group = ECGroup_consGFp(&irreducible, &a, &b, &Gx, &Gy, &order, params->cofactor);
		} else {
			SECITEM_TO_MPINT( params->fieldID.u.poly, &irreducible );
			irr_arr[0] = params->fieldID.size;
			irr_arr[1] = params->fieldID.k1;
			irr_arr[2] = params->fieldID.k2;
			irr_arr[3] = params->fieldID.k3;
			irr_arr[4] = 0;
			group = ECGroup_consGF2m(&irreducible, irr_arr, &a, &b, &Gx, &Gy, &order, params->cofactor);
		}
	}
#endif
	if (group == NULL)
		goto cleanup;

	if ((k2 != NULL) && (pointP != NULL)) {
		CHECK_MPI_OK( ECPoints_mul(group, k1, k2, &Px, &Py, &Qx, &Qy) );
	} else {
		CHECK_MPI_OK( ECPoints_mul(group, k1, NULL, NULL, NULL, &Qx, &Qy) );
    }

    /* Construct the SECItem representation of point Q */
    pointQ->data[0] = EC_POINT_FORM_UNCOMPRESSED;
    CHECK_MPI_OK( mp_to_fixlen_octets(&Qx, pointQ->data + 1,
	                              (mp_size) len) );
    CHECK_MPI_OK( mp_to_fixlen_octets(&Qy, pointQ->data + 1 + len,
	                              (mp_size) len) );

    rv = SECSuccess;

#if EC_DEBUG
    printf("ec_points_mul: pointQ [len=%d]:", pointQ->len);
    for (i = 0; i < pointQ->len; i++) 
	    printf("%02x:", pointQ->data[i]);
    printf("\n");
#endif

cleanup:
    ECGroup_free(group);
    mp_clear(&Px);
    mp_clear(&Py);
    mp_clear(&Qx);
    mp_clear(&Qy);
    mp_clear(&Gx);
    mp_clear(&Gy);
    mp_clear(&order);
    mp_clear(&irreducible);
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

    /* Set the version number (SEC 1 section C.4 says it should be 1) */
    SECITEM_AllocItem(arena, &key->version, 1);
    key->version.data[0] = 1;

    /* Copy all of the fields from the ECParams argument to the
     * ECParams structure within the private key.
     */
    key->ecParams.arena = arena;
    key->ecParams.type = ecParams->type;
    key->ecParams.fieldID.size = ecParams->fieldID.size;
    key->ecParams.fieldID.type = ecParams->fieldID.type;
    if (ecParams->fieldID.type == ec_field_GFp) {
	CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.fieldID.u.prime,
	    &ecParams->fieldID.u.prime));
    } else {
	CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.fieldID.u.poly,
	    &ecParams->fieldID.u.poly));
    }
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
    key->ecParams.name = ecParams->name;
    CHECK_SEC_OK(SECITEM_CopyItem(arena, &key->ecParams.curveOID,
	&ecParams->curveOID));

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

    rv = ec_points_mul(ecParams, &k, NULL, NULL, &(key->publicValue));
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
 * X9.62. The ECDH primitive when used without the cofactor does
 * not address small subgroup attacks, which may occur when the
 * public key is not valid. These attacks can be prevented by 
 * validating the public key before using ECDH.
 */
SECStatus 
EC_ValidatePublicKey(ECParams *ecParams, SECItem *publicValue)
{
#ifdef NSS_ENABLE_ECC
    mp_int Px, Py;
    ECGroup *group = NULL;
    SECStatus rv = SECFailure;
    mp_err err = MP_OKAY;
    int len;

    if (!ecParams || !publicValue) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
	
    /* NOTE: We only support uncompressed points for now */
    len = (ecParams->fieldID.size + 7) >> 3;
    if (publicValue->data[0] != EC_POINT_FORM_UNCOMPRESSED) {
	PORT_SetError(SEC_ERROR_UNSUPPORTED_EC_POINT_FORM); 
	return SECFailure;
    } else if (publicValue->len != (2 * len + 1)) {
	PORT_SetError(SEC_ERROR_INPUT_LEN); 
	return SECFailure;
    };

    MP_DIGITS(&Px) = 0;
    MP_DIGITS(&Py) = 0;
    CHECK_MPI_OK( mp_init(&Px) );
    CHECK_MPI_OK( mp_init(&Py) );

    /* Initialize Px and Py */
    CHECK_MPI_OK( mp_read_unsigned_octets(&Px, publicValue->data + 1, (mp_size) len) );
    CHECK_MPI_OK( mp_read_unsigned_octets(&Py, publicValue->data + 1 + len, (mp_size) len) );

    /* construct from named params */
    group = ECGroup_fromName(ecParams->name);
    if (group == NULL)
	goto cleanup;

    /* validate public point */
    CHECK_MPI_OK( ECPoint_validate(group, &Px, &Py) );

    rv = SECSuccess;

cleanup:
    ECGroup_free(group);
    mp_clear(&Px);
    mp_clear(&Py);
    if (err) {
	MP_TO_SEC_ERROR(err);
	rv = SECFailure;
    }
    return rv;
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
    if ((ec_points_mul(ecParams, NULL, &k, publicValue, &pointQ) != SECSuccess) ||
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
#if EC_DEBUG
        printf("k is outside [1, n-1]\n");
        mp_tohex(&k, mpstr);
	printf("k : %s \n", mpstr);
        mp_tohex(&n, mpstr);
	printf("n : %s \n", mpstr);
#endif
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
	(ec_points_mul(ecParams, &k, NULL, NULL, &kGpoint)
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
    mp_tohex(&r, mpstr);
    printf("r : %s\n", mpstr);
#endif

    CHECK_MPI_OK( mp_invmod(&k, &n, &k) );      /* k = k**-1 mod n */
    CHECK_MPI_OK( mp_mulmod(&d, &r, &n, &d) );  /* d = d * r mod n */
    CHECK_MPI_OK( mp_addmod(&s, &d, &n, &s) );  /* s = s + d mod n */
    CHECK_MPI_OK( mp_mulmod(&s, &k, &n, &s) );  /* s = s * k mod n */

#if EC_DEBUG
    mp_todecimal(&s, mpstr);
    printf("s : %s (dec)\n", mpstr);
    mp_tohex(&s, mpstr);
    printf("s : %s\n", mpstr);
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
    int n = key->ecParams.order.len;
    unsigned char *kseed = NULL;
    unsigned char *mask;
    int i;

    /* Generate random seed of appropriate size as dictated 
     * by field size.
     */
    if ((kseed = PORT_Alloc(n)) == NULL) return SECFailure;

    do {
        if (RNG_GenerateGlobalRandomBytes(kseed, n) != SECSuccess) 
	    goto cleanup;
	/* make sure that kseed is smaller than the curve order */
	mask = key->ecParams.order.data;
	for (i = 0; (i < n) && (*mask == 0x00); i++, mask++) {
#if EC_DEBUG
	  printf("replacing byte %02x in position %d [n=%d] with zero\n", 
		 *(kseed + i), i, n);
#endif
	  *(kseed + i) = 0x00;
	}

	if (i == n) {
	    rv = SECFailure;
	    prerr = SEC_ERROR_NEED_RANDOM;
	} else {
#if EC_DEBUG
	    printf("replacing byte %02x in position %d [n=%d] with %d\n", 
		   *(kseed + i), i, n, (*mask - 1));
#endif
	    if (*(kseed + i) >= *mask) 
	        *(kseed + i) = *mask - 1;
	    rv = ECDSA_SignDigestWithSeed(key, signature, digest, kseed, n);
	    if (rv) prerr = PORT_GetError();
	}
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
	if ((ec_points_mul(ecParams, &u1, &u2, &key->publicValue, &pointC) == SECFailure) ||
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

#if EC_DEBUG
    mp_todecimal(&r_, mpstr);
    printf("r_: %s (dec)\n", mpstr);
    mp_todecimal(&v, mpstr);
    printf("v : %s (dec)\n", mpstr);
#endif

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

