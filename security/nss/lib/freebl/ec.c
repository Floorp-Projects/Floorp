/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapi.h"
#include "prerr.h"
#include "secerr.h"
#include "secmpi.h"
#include "secitem.h"
#include "mplogic.h"
#include "ec.h"
#include "ecl.h"

static const ECMethod kMethods[] = {
    { ECCurve25519,
      ec_Curve25519_pt_mul,
      ec_Curve25519_pt_validate }
};

static const ECMethod *
ec_get_method_from_name(ECCurveName name)
{
    int i;
    for (i = 0; i < sizeof(kMethods) / sizeof(kMethods[0]); ++i) {
        if (kMethods[i].name == name) {
            return &kMethods[i];
        }
    }
    return NULL;
}

/*
 * Returns true if pointP is the point at infinity, false otherwise
 */
PRBool
ec_point_at_infinity(SECItem *pointP)
{
    unsigned int i;

    for (i = 1; i < pointP->len; i++) {
        if (pointP->data[i] != 0x00)
            return PR_FALSE;
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
        mp_tohex((mp_int *)k1, mpstr);
        printf("ec_points_mul: scalar k1: %s\n", mpstr);
        mp_todecimal((mp_int *)k1, mpstr);
        printf("ec_points_mul: scalar k1: %s (dec)\n", mpstr);
    }

    if (k2 != NULL) {
        mp_tohex((mp_int *)k2, mpstr);
        printf("ec_points_mul: scalar k2: %s\n", mpstr);
        mp_todecimal((mp_int *)k2, mpstr);
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
            PORT_SetError(SEC_ERROR_UNSUPPORTED_EC_POINT_FORM);
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
    CHECK_MPI_OK(mp_init(&Px));
    CHECK_MPI_OK(mp_init(&Py));
    CHECK_MPI_OK(mp_init(&Qx));
    CHECK_MPI_OK(mp_init(&Qy));
    CHECK_MPI_OK(mp_init(&Gx));
    CHECK_MPI_OK(mp_init(&Gy));
    CHECK_MPI_OK(mp_init(&order));
    CHECK_MPI_OK(mp_init(&irreducible));
    CHECK_MPI_OK(mp_init(&a));
    CHECK_MPI_OK(mp_init(&b));

    if ((k2 != NULL) && (pointP != NULL)) {
        /* Initialize Px and Py */
        CHECK_MPI_OK(mp_read_unsigned_octets(&Px, pointP->data + 1, (mp_size)len));
        CHECK_MPI_OK(mp_read_unsigned_octets(&Py, pointP->data + 1 + len, (mp_size)len));
    }

    /* construct from named params, if possible */
    if (params->name != ECCurve_noName) {
        group = ECGroup_fromName(params->name);
    }

    if (group == NULL)
        goto cleanup;

    if ((k2 != NULL) && (pointP != NULL)) {
        CHECK_MPI_OK(ECPoints_mul(group, k1, k2, &Px, &Py, &Qx, &Qy));
    } else {
        CHECK_MPI_OK(ECPoints_mul(group, k1, NULL, NULL, NULL, &Qx, &Qy));
    }

    /* Construct the SECItem representation of point Q */
    pointQ->data[0] = EC_POINT_FORM_UNCOMPRESSED;
    CHECK_MPI_OK(mp_to_fixlen_octets(&Qx, pointQ->data + 1,
                                     (mp_size)len));
    CHECK_MPI_OK(mp_to_fixlen_octets(&Qy, pointQ->data + 1 + len,
                                     (mp_size)len));

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

/* Generates a new EC key pair. The private key is a supplied
 * value and the public key is the result of performing a scalar
 * point multiplication of that value with the curve's base point.
 */
SECStatus
ec_NewKey(ECParams *ecParams, ECPrivateKey **privKey,
          const unsigned char *privKeyBytes, int privKeyLen)
{
    SECStatus rv = SECFailure;
    PLArenaPool *arena;
    ECPrivateKey *key;
    mp_int k;
    mp_err err = MP_OKAY;
    int len;

#if EC_DEBUG
    printf("ec_NewKey called\n");
#endif
    MP_DIGITS(&k) = 0;

    if (!ecParams || ecParams->name == ECCurve_noName ||
        !privKey || !privKeyBytes || privKeyLen <= 0) {
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
    if (ecParams->fieldID.type == ec_field_GFp ||
        ecParams->fieldID.type == ec_field_plain) {
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

    SECITEM_AllocItem(arena, &key->publicValue, EC_GetPointSize(ecParams));
    len = ecParams->order.len;
    SECITEM_AllocItem(arena, &key->privateValue, len);

    /* Copy private key */
    if (privKeyLen >= len) {
        memcpy(key->privateValue.data, privKeyBytes, len);
    } else {
        memset(key->privateValue.data, 0, (len - privKeyLen));
        memcpy(key->privateValue.data + (len - privKeyLen), privKeyBytes, privKeyLen);
    }

    /* Compute corresponding public key */

    /* Use curve specific code for point multiplication */
    if (ecParams->fieldID.type == ec_field_plain) {
        const ECMethod *method = ec_get_method_from_name(ecParams->name);
        if (method == NULL || method->mul == NULL) {
            /* unknown curve */
            rv = SECFailure;
            goto cleanup;
        }
        rv = method->mul(&key->publicValue, &key->privateValue, NULL);
        goto done;
    }

    CHECK_MPI_OK(mp_init(&k));
    CHECK_MPI_OK(mp_read_unsigned_octets(&k, key->privateValue.data,
                                         (mp_size)len));

    rv = ec_points_mul(ecParams, &k, NULL, NULL, &(key->publicValue));
    if (rv != SECSuccess) {
        goto cleanup;
    }

done:
    *privKey = key;

cleanup:
    mp_clear(&k);
    if (rv) {
        PORT_FreeArena(arena, PR_TRUE);
    }

#if EC_DEBUG
    printf("ec_NewKey returning %s\n",
           (rv == SECSuccess) ? "success" : "failure");
#endif

    return rv;
}

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
    rv = ec_NewKey(ecParams, privKey, seed, seedlen);
    return rv;
}

/* Generate a random private key using the algorithm A.4.1 of ANSI X9.62,
 * modified a la FIPS 186-2 Change Notice 1 to eliminate the bias in the
 * random number generator.
 *
 * Parameters
 * - order: a buffer that holds the curve's group order
 * - len: the length in octets of the order buffer
 *
 * Return Value
 * Returns a buffer of len octets that holds the private key. The caller
 * is responsible for freeing the buffer with PORT_ZFree.
 */
static unsigned char *
ec_GenerateRandomPrivateKey(const unsigned char *order, int len)
{
    SECStatus rv = SECSuccess;
    mp_err err;
    unsigned char *privKeyBytes = NULL;
    mp_int privKeyVal, order_1, one;

    MP_DIGITS(&privKeyVal) = 0;
    MP_DIGITS(&order_1) = 0;
    MP_DIGITS(&one) = 0;
    CHECK_MPI_OK(mp_init(&privKeyVal));
    CHECK_MPI_OK(mp_init(&order_1));
    CHECK_MPI_OK(mp_init(&one));

    /* Generates 2*len random bytes using the global random bit generator
     * (which implements Algorithm 1 of FIPS 186-2 Change Notice 1) then
     * reduces modulo the group order.
     */
    if ((privKeyBytes = PORT_Alloc(2 * len)) == NULL)
        goto cleanup;
    CHECK_SEC_OK(RNG_GenerateGlobalRandomBytes(privKeyBytes, 2 * len));
    CHECK_MPI_OK(mp_read_unsigned_octets(&privKeyVal, privKeyBytes, 2 * len));
    CHECK_MPI_OK(mp_read_unsigned_octets(&order_1, order, len));
    CHECK_MPI_OK(mp_set_int(&one, 1));
    CHECK_MPI_OK(mp_sub(&order_1, &one, &order_1));
    CHECK_MPI_OK(mp_mod(&privKeyVal, &order_1, &privKeyVal));
    CHECK_MPI_OK(mp_add(&privKeyVal, &one, &privKeyVal));
    CHECK_MPI_OK(mp_to_fixlen_octets(&privKeyVal, privKeyBytes, len));
    memset(privKeyBytes + len, 0, len);
cleanup:
    mp_clear(&privKeyVal);
    mp_clear(&order_1);
    mp_clear(&one);
    if (err < MP_OKAY) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }
    if (rv != SECSuccess && privKeyBytes) {
        PORT_ZFree(privKeyBytes, 2 * len);
        privKeyBytes = NULL;
    }
    return privKeyBytes;
}

/* Generates a new EC key pair. The private key is a random value and
 * the public key is the result of performing a scalar point multiplication
 * of that value with the curve's base point.
 */
SECStatus
EC_NewKey(ECParams *ecParams, ECPrivateKey **privKey)
{
    SECStatus rv = SECFailure;
    int len;
    unsigned char *privKeyBytes = NULL;

    if (!ecParams || ecParams->name == ECCurve_noName || !privKey) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    len = ecParams->order.len;
    privKeyBytes = ec_GenerateRandomPrivateKey(ecParams->order.data, len);
    if (privKeyBytes == NULL)
        goto cleanup;
    /* generate public key */
    CHECK_SEC_OK(ec_NewKey(ecParams, privKey, privKeyBytes, len));

cleanup:
    if (privKeyBytes) {
        PORT_ZFree(privKeyBytes, len);
    }
#if EC_DEBUG
    printf("EC_NewKey returning %s\n",
           (rv == SECSuccess) ? "success" : "failure");
#endif

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
    mp_int Px, Py;
    ECGroup *group = NULL;
    SECStatus rv = SECFailure;
    mp_err err = MP_OKAY;
    int len;

    if (!ecParams || ecParams->name == ECCurve_noName ||
        !publicValue || !publicValue->len) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Uses curve specific code for point validation. */
    if (ecParams->fieldID.type == ec_field_plain) {
        const ECMethod *method = ec_get_method_from_name(ecParams->name);
        if (method == NULL || method->validate == NULL) {
            /* unknown curve */
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        return method->validate(publicValue);
    }

    /* NOTE: We only support uncompressed points for now */
    len = (ecParams->fieldID.size + 7) >> 3;
    if (publicValue->data[0] != EC_POINT_FORM_UNCOMPRESSED) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_EC_POINT_FORM);
        return SECFailure;
    } else if (publicValue->len != (2 * len + 1)) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }

    MP_DIGITS(&Px) = 0;
    MP_DIGITS(&Py) = 0;
    CHECK_MPI_OK(mp_init(&Px));
    CHECK_MPI_OK(mp_init(&Py));

    /* Initialize Px and Py */
    CHECK_MPI_OK(mp_read_unsigned_octets(&Px, publicValue->data + 1, (mp_size)len));
    CHECK_MPI_OK(mp_read_unsigned_octets(&Py, publicValue->data + 1 + len, (mp_size)len));

    /* construct from named params */
    group = ECGroup_fromName(ecParams->name);
    if (group == NULL) {
        /*
         * ECGroup_fromName fails if ecParams->name is not a valid
         * ECCurveName value, or if we run out of memory, or perhaps
         * for other reasons.  Unfortunately if ecParams->name is a
         * valid ECCurveName value, we don't know what the right error
         * code should be because ECGroup_fromName doesn't return an
         * error code to the caller.  Set err to MP_UNDEF because
         * that's what ECGroup_fromName uses internally.
         */
        if ((ecParams->name <= ECCurve_noName) ||
            (ecParams->name >= ECCurve_pastLastCurve)) {
            err = MP_BADARG;
        } else {
            err = MP_UNDEF;
        }
        goto cleanup;
    }

    /* validate public point */
    if ((err = ECPoint_validate(group, &Px, &Py)) < MP_YES) {
        if (err == MP_NO) {
            PORT_SetError(SEC_ERROR_BAD_KEY);
            rv = SECFailure;
            err = MP_OKAY; /* don't change the error code */
        }
        goto cleanup;
    }

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
ECDH_Derive(SECItem *publicValue,
            ECParams *ecParams,
            SECItem *privateValue,
            PRBool withCofactor,
            SECItem *derivedSecret)
{
    SECStatus rv = SECFailure;
    unsigned int len = 0;
    SECItem pointQ = { siBuffer, NULL, 0 };
    mp_int k; /* to hold the private value */
    mp_int cofactor;
    mp_err err = MP_OKAY;
#if EC_DEBUG
    int i;
#endif

    if (!publicValue || !publicValue->len ||
        !ecParams || ecParams->name == ECCurve_noName ||
        !privateValue || !privateValue->len || !derivedSecret) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /*
     * Make sure the point is on the requested curve to avoid
     * certain small subgroup attacks.
     */
    if (EC_ValidatePublicKey(ecParams, publicValue) != SECSuccess) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }

    /* Perform curve specific multiplication using ECMethod */
    if (ecParams->fieldID.type == ec_field_plain) {
        const ECMethod *method;
        memset(derivedSecret, 0, sizeof(*derivedSecret));
        derivedSecret = SECITEM_AllocItem(NULL, derivedSecret, EC_GetPointSize(ecParams));
        if (derivedSecret == NULL) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            return SECFailure;
        }
        method = ec_get_method_from_name(ecParams->name);
        if (method == NULL || method->validate == NULL ||
            method->mul == NULL) {
            PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
            return SECFailure;
        }
        rv = method->mul(derivedSecret, privateValue, publicValue);
        if (rv != SECSuccess) {
            SECITEM_ZfreeItem(derivedSecret, PR_FALSE);
        }
        return rv;
    }

    /*
     * We fail if the public value is the point at infinity, since
     * this produces predictable results.
     */
    if (ec_point_at_infinity(publicValue)) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }

    MP_DIGITS(&k) = 0;
    memset(derivedSecret, 0, sizeof *derivedSecret);
    len = (ecParams->fieldID.size + 7) >> 3;
    pointQ.len = EC_GetPointSize(ecParams);
    if ((pointQ.data = PORT_Alloc(pointQ.len)) == NULL)
        goto cleanup;

    CHECK_MPI_OK(mp_init(&k));
    CHECK_MPI_OK(mp_read_unsigned_octets(&k, privateValue->data,
                                         (mp_size)privateValue->len));

    if (withCofactor && (ecParams->cofactor != 1)) {
        /* multiply k with the cofactor */
        MP_DIGITS(&cofactor) = 0;
        CHECK_MPI_OK(mp_init(&cofactor));
        mp_set(&cofactor, ecParams->cofactor);
        CHECK_MPI_OK(mp_mul(&k, &cofactor, &k));
    }

    /* Multiply our private key and peer's public point */
    if (ec_points_mul(ecParams, NULL, &k, publicValue, &pointQ) != SECSuccess) {
        goto cleanup;
    }
    if (ec_point_at_infinity(&pointQ)) {
        PORT_SetError(SEC_ERROR_BAD_KEY); /* XXX better error code? */
        goto cleanup;
    }

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

    if (err) {
        MP_TO_SEC_ERROR(err);
    }

    if (pointQ.data) {
        PORT_ZFree(pointQ.data, pointQ.len);
    }

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
    mp_int x1;
    mp_int d, k; /* private key, random integer */
    mp_int r, s; /* tuple (r, s) is the signature */
    mp_int t;    /* holding tmp values */
    mp_int n;
    mp_int ar; /* blinding value */
    mp_err err = MP_OKAY;
    ECParams *ecParams = NULL;
    SECItem kGpoint = { siBuffer, NULL, 0 };
    int flen = 0;   /* length in bytes of the field size */
    unsigned olen;  /* length in bytes of the base point order */
    unsigned obits; /* length in bits  of the base point order */
    unsigned char *t2 = NULL;

#if EC_DEBUG
    char mpstr[256];
#endif

    /* Initialize MPI integers. */
    /* must happen before the first potential call to cleanup */
    MP_DIGITS(&x1) = 0;
    MP_DIGITS(&d) = 0;
    MP_DIGITS(&k) = 0;
    MP_DIGITS(&r) = 0;
    MP_DIGITS(&s) = 0;
    MP_DIGITS(&n) = 0;
    MP_DIGITS(&t) = 0;
    MP_DIGITS(&ar) = 0;

    /* Check args */
    if (!key || !signature || !digest || !kb || (kblen < 0)) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto cleanup;
    }

    ecParams = &(key->ecParams);
    flen = (ecParams->fieldID.size + 7) >> 3;
    olen = ecParams->order.len;
    if (signature->data == NULL) {
        /* a call to get the signature length only */
        goto finish;
    }
    if (signature->len < 2 * olen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        goto cleanup;
    }

    CHECK_MPI_OK(mp_init(&x1));
    CHECK_MPI_OK(mp_init(&d));
    CHECK_MPI_OK(mp_init(&k));
    CHECK_MPI_OK(mp_init(&r));
    CHECK_MPI_OK(mp_init(&s));
    CHECK_MPI_OK(mp_init(&n));
    CHECK_MPI_OK(mp_init(&t));
    CHECK_MPI_OK(mp_init(&ar));

    SECITEM_TO_MPINT(ecParams->order, &n);
    SECITEM_TO_MPINT(key->privateValue, &d);

    CHECK_MPI_OK(mp_read_unsigned_octets(&k, kb, kblen));
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
    ** We do not want timing information to leak the length of k,
    ** so we compute k*G using an equivalent scalar of fixed
    ** bit-length.
    ** Fix based on patch for ECDSA timing attack in the paper
    ** by Billy Bob Brumley and Nicola Tuveri at
    **   http://eprint.iacr.org/2011/232
    **
    ** How do we convert k to a value of a fixed bit-length?
    ** k starts off as an integer satisfying 0 <= k < n.  Hence,
    ** n <= k+n < 2n, which means k+n has either the same number
    ** of bits as n or one more bit than n.  If k+n has the same
    ** number of bits as n, the second addition ensures that the
    ** final value has exactly one more bit than n.  Thus, we
    ** always end up with a value that exactly one more bit than n.
    */
    CHECK_MPI_OK(mp_add(&k, &n, &k));
    if (mpl_significant_bits(&k) <= mpl_significant_bits(&n)) {
        CHECK_MPI_OK(mp_add(&k, &n, &k));
    }

    /*
    ** ANSI X9.62, Section 5.3.2, Step 2
    **
    ** Compute kG
    */
    kGpoint.len = EC_GetPointSize(ecParams);
    kGpoint.data = PORT_Alloc(kGpoint.len);
    if ((kGpoint.data == NULL) ||
        (ec_points_mul(ecParams, &k, NULL, NULL, &kGpoint) != SECSuccess))
        goto cleanup;

    /*
    ** ANSI X9.62, Section 5.3.3, Step 1
    **
    ** Extract the x co-ordinate of kG into x1
    */
    CHECK_MPI_OK(mp_read_unsigned_octets(&x1, kGpoint.data + 1,
                                         (mp_size)flen));

    /*
    ** ANSI X9.62, Section 5.3.3, Step 2
    **
    ** r = x1 mod n  NOTE: n is the order of the curve
    */
    CHECK_MPI_OK(mp_mod(&x1, &n, &r));

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
    ** s = (k**-1 * (HASH(M) + d*r)) mod n
    */
    SECITEM_TO_MPINT(*digest, &s); /* s = HASH(M)     */

    /* In the definition of EC signing, digests are truncated
     * to the length of n in bits.
     * (see SEC 1 "Elliptic Curve Digit Signature Algorithm" section 4.1.*/
    CHECK_MPI_OK((obits = mpl_significant_bits(&n)));
    if (digest->len * 8 > obits) {
        mpl_rsh(&s, &s, digest->len * 8 - obits);
    }

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

    if ((t2 = PORT_Alloc(2 * ecParams->order.len)) == NULL) {
        rv = SECFailure;
        goto cleanup;
    }
    if (RNG_GenerateGlobalRandomBytes(t2, 2 * ecParams->order.len) != SECSuccess) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        rv = SECFailure;
        goto cleanup;
    }
    CHECK_MPI_OK(mp_read_unsigned_octets(&t, t2, 2 * ecParams->order.len)); /* t <-$ Zn */
    PORT_Memset(t2, 0, 2 * ecParams->order.len);
    if (RNG_GenerateGlobalRandomBytes(t2, 2 * ecParams->order.len) != SECSuccess) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        rv = SECFailure;
        goto cleanup;
    }
    CHECK_MPI_OK(mp_read_unsigned_octets(&ar, t2, 2 * ecParams->order.len)); /* ar <-$ Zn */

    /* Using mp_invmod on k directly would leak bits from k. */
    CHECK_MPI_OK(mp_mul(&k, &ar, &k));       /* k = k * ar */
    CHECK_MPI_OK(mp_mulmod(&k, &t, &n, &k)); /* k = k * t mod n */
    CHECK_MPI_OK(mp_invmod(&k, &n, &k));     /* k = k**-1 mod n */
    CHECK_MPI_OK(mp_mulmod(&k, &t, &n, &k)); /* k = k * t mod n */
    /* To avoid leaking secret bits here the addition is blinded. */
    CHECK_MPI_OK(mp_mul(&d, &ar, &t));        /* t = d * ar */
    CHECK_MPI_OK(mp_mulmod(&t, &r, &n, &d));  /* d = t * r mod n */
    CHECK_MPI_OK(mp_mulmod(&s, &ar, &n, &t)); /* t = s * ar mod n */
    CHECK_MPI_OK(mp_add(&t, &d, &s));         /* s = t + d */
    CHECK_MPI_OK(mp_mulmod(&s, &k, &n, &s));  /* s = s * k mod n */

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
    CHECK_MPI_OK(mp_to_fixlen_octets(&r, signature->data, olen));
    CHECK_MPI_OK(mp_to_fixlen_octets(&s, signature->data + olen, olen));
finish:
    signature->len = 2 * olen;

    rv = SECSuccess;
    err = MP_OKAY;
cleanup:
    mp_clear(&x1);
    mp_clear(&d);
    mp_clear(&k);
    mp_clear(&r);
    mp_clear(&s);
    mp_clear(&n);
    mp_clear(&t);
    mp_clear(&ar);

    if (t2) {
        PORT_Free(t2);
    }

    if (kGpoint.data) {
        PORT_ZFree(kGpoint.data, kGpoint.len);
    }

    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }

#if EC_DEBUG
    printf("ECDSA signing with seed %s\n",
           (rv == SECSuccess) ? "succeeded" : "failed");
#endif

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
    int len;
    unsigned char *kBytes = NULL;

    if (!key) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* Generate random value k */
    len = key->ecParams.order.len;
    kBytes = ec_GenerateRandomPrivateKey(key->ecParams.order.data, len);
    if (kBytes == NULL)
        goto cleanup;

    /* Generate ECDSA signature with the specified k value */
    rv = ECDSA_SignDigestWithSeed(key, signature, digest, kBytes, len);

cleanup:
    if (kBytes) {
        PORT_ZFree(kBytes, len);
    }

#if EC_DEBUG
    printf("ECDSA signing %s\n",
           (rv == SECSuccess) ? "succeeded" : "failed");
#endif

    return rv;
}

/*
** Checks the signature on the given digest using the key provided.
**
** The key argument must represent a valid EC public key (a point on
** the relevant curve).  If it is not a valid point, then the behavior
** of this function is undefined.  In cases where a public key might
** not be valid, use EC_ValidatePublicKey to check.
*/
SECStatus
ECDSA_VerifyDigest(ECPublicKey *key, const SECItem *signature,
                   const SECItem *digest)
{
    SECStatus rv = SECFailure;
    mp_int r_, s_;       /* tuple (r', s') is received signature) */
    mp_int c, u1, u2, v; /* intermediate values used in verification */
    mp_int x1;
    mp_int n;
    mp_err err = MP_OKAY;
    ECParams *ecParams = NULL;
    SECItem pointC = { siBuffer, NULL, 0 };
    int slen;       /* length in bytes of a half signature (r or s) */
    int flen;       /* length in bytes of the field size */
    unsigned olen;  /* length in bytes of the base point order */
    unsigned obits; /* length in bits  of the base point order */

#if EC_DEBUG
    char mpstr[256];
    printf("ECDSA verification called\n");
#endif

    /* Initialize MPI integers. */
    /* must happen before the first potential call to cleanup */
    MP_DIGITS(&r_) = 0;
    MP_DIGITS(&s_) = 0;
    MP_DIGITS(&c) = 0;
    MP_DIGITS(&u1) = 0;
    MP_DIGITS(&u2) = 0;
    MP_DIGITS(&x1) = 0;
    MP_DIGITS(&v) = 0;
    MP_DIGITS(&n) = 0;

    /* Check args */
    if (!key || !signature || !digest) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto cleanup;
    }

    ecParams = &(key->ecParams);
    flen = (ecParams->fieldID.size + 7) >> 3;
    olen = ecParams->order.len;
    if (signature->len == 0 || signature->len % 2 != 0 ||
        signature->len > 2 * olen) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        goto cleanup;
    }
    slen = signature->len / 2;

    /*
     * The incoming point has been verified in sftk_handlePublicKeyObject.
     */

    SECITEM_AllocItem(NULL, &pointC, EC_GetPointSize(ecParams));
    if (pointC.data == NULL) {
        goto cleanup;
    }

    CHECK_MPI_OK(mp_init(&r_));
    CHECK_MPI_OK(mp_init(&s_));
    CHECK_MPI_OK(mp_init(&c));
    CHECK_MPI_OK(mp_init(&u1));
    CHECK_MPI_OK(mp_init(&u2));
    CHECK_MPI_OK(mp_init(&x1));
    CHECK_MPI_OK(mp_init(&v));
    CHECK_MPI_OK(mp_init(&n));

    /*
    ** Convert received signature (r', s') into MPI integers.
    */
    CHECK_MPI_OK(mp_read_unsigned_octets(&r_, signature->data, slen));
    CHECK_MPI_OK(mp_read_unsigned_octets(&s_, signature->data + slen, slen));

    /*
    ** ANSI X9.62, Section 5.4.2, Steps 1 and 2
    **
    ** Verify that 0 < r' < n and 0 < s' < n
    */
    SECITEM_TO_MPINT(ecParams->order, &n);
    if (mp_cmp_z(&r_) <= 0 || mp_cmp_z(&s_) <= 0 ||
        mp_cmp(&r_, &n) >= 0 || mp_cmp(&s_, &n) >= 0) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        goto cleanup; /* will return rv == SECFailure */
    }

    /*
    ** ANSI X9.62, Section 5.4.2, Step 3
    **
    ** c = (s')**-1 mod n
    */
    CHECK_MPI_OK(mp_invmod(&s_, &n, &c)); /* c = (s')**-1 mod n */

    /*
    ** ANSI X9.62, Section 5.4.2, Step 4
    **
    ** u1 = ((HASH(M')) * c) mod n
    */
    SECITEM_TO_MPINT(*digest, &u1); /* u1 = HASH(M)     */

    /* In the definition of EC signing, digests are truncated
     * to the length of n in bits.
     * (see SEC 1 "Elliptic Curve Digit Signature Algorithm" section 4.1.*/
    CHECK_MPI_OK((obits = mpl_significant_bits(&n)));
    if (digest->len * 8 > obits) { /* u1 = HASH(M')     */
        mpl_rsh(&u1, &u1, digest->len * 8 - obits);
    }

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

    CHECK_MPI_OK(mp_mulmod(&u1, &c, &n, &u1)); /* u1 = u1 * c mod n */

    /*
    ** ANSI X9.62, Section 5.4.2, Step 4
    **
    ** u2 = ((r') * c) mod n
    */
    CHECK_MPI_OK(mp_mulmod(&r_, &c, &n, &u2));

    /*
    ** ANSI X9.62, Section 5.4.3, Step 1
    **
    ** Compute u1*G + u2*Q
    ** Here, A = u1.G     B = u2.Q    and   C = A + B
    ** If the result, C, is the point at infinity, reject the signature
    */
    if (ec_points_mul(ecParams, &u1, &u2, &key->publicValue, &pointC) != SECSuccess) {
        rv = SECFailure;
        goto cleanup;
    }
    if (ec_point_at_infinity(&pointC)) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        rv = SECFailure;
        goto cleanup;
    }

    CHECK_MPI_OK(mp_read_unsigned_octets(&x1, pointC.data + 1, flen));

    /*
    ** ANSI X9.62, Section 5.4.4, Step 2
    **
    ** v = x1 mod n
    */
    CHECK_MPI_OK(mp_mod(&x1, &n, &v));

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
    mp_clear(&v);
    mp_clear(&n);

    if (pointC.data)
        SECITEM_ZfreeItem(&pointC, PR_FALSE);
    if (err) {
        MP_TO_SEC_ERROR(err);
        rv = SECFailure;
    }

#if EC_DEBUG
    printf("ECDSA verification %s\n",
           (rv == SECSuccess) ? "succeeded" : "failed");
#endif

    return rv;
}
