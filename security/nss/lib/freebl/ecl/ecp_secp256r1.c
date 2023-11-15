/* P-256 from HACL* */

#ifdef FREEBL_NO_DEPEND
#include "../stubs.h"
#endif

#include "ecl-priv.h"
#include "secitem.h"
#include "secerr.h"
#include "secmpi.h"
#include "../verified/Hacl_P256.h"

/*
 * Point Validation for P-256.
 */

SECStatus
ec_secp256r1_pt_validate(const SECItem *pt)
{
    SECStatus res = SECSuccess;
    if (!pt || !pt->data) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        res = SECFailure;
        return res;
    }

    if (pt->len != 65) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        res = SECFailure;
        return res;
    }

    if (pt->data[0] != EC_POINT_FORM_UNCOMPRESSED) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_EC_POINT_FORM);
        res = SECFailure;
        return res;
    }

    bool b = Hacl_P256_validate_public_key(pt->data + 1);

    if (!b) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        res = SECFailure;
    }
    return res;
}

/*
 * Scalar Validation for P-256.
 */

SECStatus
ec_secp256r1_scalar_validate(const SECItem *scalar)
{
    SECStatus res = SECSuccess;
    if (!scalar || !scalar->data) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        res = SECFailure;
        return res;
    }

    if (scalar->len != 32) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        res = SECFailure;
        return res;
    }

    bool b = Hacl_P256_validate_private_key(scalar->data);

    if (!b) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        res = SECFailure;
    }
    return res;
}

/*
 * Scalar multiplication for P-256.
 * If P == NULL, the base point is used.
 * Returns X = k*P
 */

SECStatus
ec_secp256r1_pt_mul(SECItem *X, SECItem *k, SECItem *P)
{
    SECStatus res = SECSuccess;
    if (!P) {
        uint8_t derived[64] = { 0 };

        if (!X || !k || !X->data || !k->data ||
            X->len < 65 || k->len != 32) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            res = SECFailure;
            return res;
        }

        bool b = Hacl_P256_dh_initiator(derived, k->data);

        if (!b) {
            PORT_SetError(SEC_ERROR_BAD_KEY);
            res = SECFailure;
            return res;
        }

        X->len = 65;
        X->data[0] = EC_POINT_FORM_UNCOMPRESSED;
        memcpy(X->data + 1, derived, 64);

    } else {
        uint8_t full_key[32] = { 0 };
        uint8_t *key;
        uint8_t derived[64] = { 0 };

        if (!X || !k || !P || !X->data || !k->data || !P->data ||
            X->len < 32 || P->len != 65 ||
            P->data[0] != EC_POINT_FORM_UNCOMPRESSED) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            res = SECFailure;
            return res;
        }

        /* We consider keys of up to size 32, or of size 33 with a single leading 0 */
        if (k->len < 32) {
            memcpy(full_key + 32 - k->len, k->data, k->len);
            key = full_key;
        } else if (k->len == 32) {
            key = k->data;
        } else if (k->len == 33 && k->data[0] == 0) {
            key = k->data + 1;
        } else {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            res = SECFailure;
            return res;
        }

        bool b = Hacl_P256_dh_responder(derived, P->data + 1, key);

        if (!b) {
            PORT_SetError(SEC_ERROR_BAD_KEY);
            res = SECFailure;
            return res;
        }

        X->len = 32;
        memcpy(X->data, derived, 32);
    }

    return res;
}

/*
 * ECDSA Signature for P-256
 */

SECStatus
ec_secp256r1_sign_digest(ECPrivateKey *key, SECItem *signature,
                         const SECItem *digest, const unsigned char *kb,
                         const unsigned int kblen)
{
    SECStatus res = SECSuccess;

    if (!key || !signature || !digest || !kb ||
        !key->privateValue.data ||
        !signature->data || !digest->data ||
        key->ecParams.name != ECCurve_NIST_P256) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        res = SECFailure;
        return res;
    }

    if (key->privateValue.len != 32 ||
        kblen == 0 ||
        digest->len == 0 ||
        signature->len < 64) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        res = SECFailure;
        return res;
    }

    uint8_t hash[32] = { 0 };
    if (digest->len < 32) {
        memcpy(hash + 32 - digest->len, digest->data, digest->len);
    } else {
        memcpy(hash, digest->data, 32);
    }

    uint8_t nonce[32] = { 0 };
    if (kblen < 32) {
        memcpy(nonce + 32 - kblen, kb, kblen);
    } else {
        memcpy(nonce, kb, 32);
    }

    bool b = Hacl_P256_ecdsa_sign_p256_without_hash(
        signature->data, 32, hash,
        key->privateValue.data, nonce);
    if (!b) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
        res = SECFailure;
        return res;
    }

    signature->len = 64;
    return res;
}

/*
 * ECDSA Signature Verification for P-256
 */

SECStatus
ec_secp256r1_verify_digest(ECPublicKey *key, const SECItem *signature,
                           const SECItem *digest)
{
    SECStatus res = SECSuccess;

    if (!key || !signature || !digest ||
        !key->publicValue.data ||
        !signature->data || !digest->data ||
        key->ecParams.name != ECCurve_NIST_P256) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        res = SECFailure;
        return res;
    }

    if (key->publicValue.len != 65 ||
        digest->len == 0 ||
        signature->len != 64) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        res = SECFailure;
        return res;
    }

    if (key->publicValue.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_EC_POINT_FORM);
        res = SECFailure;
        return res;
    }

    uint8_t hash[32] = { 0 };
    if (digest->len < 32) {
        memcpy(hash + 32 - digest->len, digest->data, digest->len);
    } else {
        memcpy(hash, digest->data, 32);
    }

    bool b = Hacl_P256_ecdsa_verif_without_hash(
        32, hash,
        key->publicValue.data + 1,
        signature->data, signature->data + 32);
    if (!b) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
        res = SECFailure;
        return res;
    }

    return res;
}
