/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements PKCS 11 on top of our existing security modules
 *
 * For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *   This implementation has two slots:
 *      slot 1 is our generic crypto support. It does not require login.
 *   It supports Public Key ops, and all they bulk ciphers and hashes.
 *   It can also support Private Key ops for imported Private keys. It does
 *   not have any token storage.
 *      slot 2 is our private key support. It requires a login before use. It
 *   can store Private Keys and Certs as token objects. Currently only private
 *   keys and their associated Certificates are saved on the token.
 *
 *   In this implementation, session objects are only visible to the session
 *   that created or generated them.
 */
#include "seccomon.h"
#include "secitem.h"
#include "secport.h"
#include "blapi.h"
#include "pkcs11.h"
#include "pkcs11i.h"
#include "pkcs1sig.h"
#include "lowkeyi.h"
#include "secder.h"
#include "secdig.h"
#include "lowpbe.h" /* We do PBE below */
#include "pkcs11t.h"
#include "secoid.h"
#include "cmac.h"
#include "alghmac.h"
#include "softoken.h"
#include "secasn1.h"
#include "secerr.h"

#include "prprf.h"
#include "prenv.h"

#define __PASTE(x, y) x##y
#define BAD_PARAM_CAST(pMech, typeSize) (!pMech->pParameter || pMech->ulParameterLen < typeSize)
/*
 * we renamed all our internal functions, get the correct
 * definitions for them...
 */
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_EXTERN extern
#define CK_PKCS11_FUNCTION_INFO(func) \
    CK_RV __PASTE(NS, func)
#define CK_NEED_ARG_LIST 1

#include "pkcs11f.h"

typedef struct {
    PRUint8 client_version[2];
    PRUint8 random[46];
} SSL3RSAPreMasterSecret;

static void
sftk_Null(void *data, PRBool freeit)
{
    return;
}

#ifdef EC_DEBUG
#define SEC_PRINT(str1, str2, num, sitem)             \
    printf("pkcs11c.c:%s:%s (keytype=%d) [len=%d]\n", \
           str1, str2, num, sitem->len);              \
    for (i = 0; i < sitem->len; i++) {                \
        printf("%02x:", sitem->data[i]);              \
    }                                                 \
    printf("\n")
#else
#undef EC_DEBUG
#define SEC_PRINT(a, b, c, d)
#endif

/*
 * free routines.... Free local type  allocated data, and convert
 * other free routines to the destroy signature.
 */
static void
sftk_FreePrivKey(NSSLOWKEYPrivateKey *key, PRBool freeit)
{
    nsslowkey_DestroyPrivateKey(key);
}

static void
sftk_Space(void *data, PRBool freeit)
{
    PORT_Free(data);
}

/*
 * map all the SEC_ERROR_xxx error codes that may be returned by freebl
 * functions to CKR_xxx.  Most of the mapping is done in 
 * sftk_mapCryptError (now in pkcs11u.c). The next two functions adjust
 * that mapping based for different contexts (Decrypt or Verify).
 */

/* used by Decrypt and UnwrapKey (indirectly) */
static CK_RV
sftk_MapDecryptError(int error)
{
    switch (error) {
        case SEC_ERROR_BAD_DATA:
            return CKR_ENCRYPTED_DATA_INVALID;
        default:
            return sftk_MapCryptError(error);
    }
}

/*
 * return CKR_SIGNATURE_INVALID instead of CKR_DEVICE_ERROR by default for
 * backward compatibilty.
 */
static CK_RV
sftk_MapVerifyError(int error)
{
    CK_RV crv = sftk_MapCryptError(error);
    if (crv == CKR_DEVICE_ERROR)
        crv = CKR_SIGNATURE_INVALID;
    return crv;
}

/*
 * turn a CDMF key into a des key. CDMF is an old IBM scheme to export DES by
 * Deprecating a full des key to 40 bit key strenth.
 */
static CK_RV
sftk_cdmf2des(unsigned char *cdmfkey, unsigned char *deskey)
{
    unsigned char key1[8] = { 0xc4, 0x08, 0xb0, 0x54, 0x0b, 0xa1, 0xe0, 0xae };
    unsigned char key2[8] = { 0xef, 0x2c, 0x04, 0x1c, 0xe6, 0x38, 0x2f, 0xe6 };
    unsigned char enc_src[8];
    unsigned char enc_dest[8];
    unsigned int leng, i;
    DESContext *descx;
    SECStatus rv;

    /* zero the parity bits */
    for (i = 0; i < 8; i++) {
        enc_src[i] = cdmfkey[i] & 0xfe;
    }

    /* encrypt with key 1 */
    descx = DES_CreateContext(key1, NULL, NSS_DES, PR_TRUE);
    if (descx == NULL)
        return CKR_HOST_MEMORY;
    rv = DES_Encrypt(descx, enc_dest, &leng, 8, enc_src, 8);
    DES_DestroyContext(descx, PR_TRUE);
    if (rv != SECSuccess)
        return sftk_MapCryptError(PORT_GetError());

    /* xor source with des, zero the parity bits and deprecate the key*/
    for (i = 0; i < 8; i++) {
        if (i & 1) {
            enc_src[i] = (enc_src[i] ^ enc_dest[i]) & 0xfe;
        } else {
            enc_src[i] = (enc_src[i] ^ enc_dest[i]) & 0x0e;
        }
    }

    /* encrypt with key 2 */
    descx = DES_CreateContext(key2, NULL, NSS_DES, PR_TRUE);
    if (descx == NULL)
        return CKR_HOST_MEMORY;
    rv = DES_Encrypt(descx, deskey, &leng, 8, enc_src, 8);
    DES_DestroyContext(descx, PR_TRUE);
    if (rv != SECSuccess)
        return sftk_MapCryptError(PORT_GetError());

    /* set the corret parity on our new des key */
    sftk_FormatDESKey(deskey, 8);
    return CKR_OK;
}

/* NSC_DestroyObject destroys an object. */
CK_RV
NSC_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SFTKSession *session;
    SFTKObject *object;
    SFTKFreeStatus status;

    CHECK_FORK();

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * This whole block just makes sure we really can destroy the
     * requested object.
     */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    object = sftk_ObjectFromHandle(hObject, session);
    if (object == NULL) {
        sftk_FreeSession(session);
        return CKR_OBJECT_HANDLE_INVALID;
    }

    /* don't destroy a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
        (sftk_isTrue(object, CKA_PRIVATE))) {
        sftk_FreeSession(session);
        sftk_FreeObject(object);
        return CKR_USER_NOT_LOGGED_IN;
    }

    /* don't destroy a token object if we aren't in a rw session */

    if (((session->info.flags & CKF_RW_SESSION) == 0) &&
        (sftk_isTrue(object, CKA_TOKEN))) {
        sftk_FreeSession(session);
        sftk_FreeObject(object);
        return CKR_SESSION_READ_ONLY;
    }

    sftk_DeleteObject(session, object);

    sftk_FreeSession(session);

    /*
     * get some indication if the object is destroyed. Note: this is not
     * 100%. Someone may have an object reference outstanding (though that
     * should not be the case by here. Also note that the object is "half"
     * destroyed. Our internal representation is destroyed, but it may still
     * be in the data base.
     */
    status = sftk_FreeObject(object);

    return (status != SFTK_DestroyFailure) ? CKR_OK : CKR_DEVICE_ERROR;
}

/*
 ************** Crypto Functions:     Utilities ************************
 */
/*
 * Utility function for converting PSS/OAEP parameter types into
 * HASH_HashTypes. Note: Only SHA family functions are defined in RFC 3447.
 */
static HASH_HashType
GetHashTypeFromMechanism(CK_MECHANISM_TYPE mech)
{
    switch (mech) {
        case CKM_SHA_1:
        case CKG_MGF1_SHA1:
            return HASH_AlgSHA1;
        case CKM_SHA224:
        case CKG_MGF1_SHA224:
            return HASH_AlgSHA224;
        case CKM_SHA256:
        case CKG_MGF1_SHA256:
            return HASH_AlgSHA256;
        case CKM_SHA384:
        case CKG_MGF1_SHA384:
            return HASH_AlgSHA384;
        case CKM_SHA512:
        case CKG_MGF1_SHA512:
            return HASH_AlgSHA512;
        default:
            return HASH_AlgNULL;
    }
}

/*
 * Returns true if "params" contains a valid set of PSS parameters
 */
static PRBool
sftk_ValidatePssParams(const CK_RSA_PKCS_PSS_PARAMS *params)
{
    if (!params) {
        return PR_FALSE;
    }
    if (GetHashTypeFromMechanism(params->hashAlg) == HASH_AlgNULL ||
        GetHashTypeFromMechanism(params->mgf) == HASH_AlgNULL) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

/*
 * Returns true if "params" contains a valid set of OAEP parameters
 */
static PRBool
sftk_ValidateOaepParams(const CK_RSA_PKCS_OAEP_PARAMS *params)
{
    if (!params) {
        return PR_FALSE;
    }
    /* The requirements of ulSourceLen/pSourceData come from PKCS #11, which
     * state:
     *   If the parameter is empty, pSourceData must be NULL and
     *   ulSourceDataLen must be zero.
     */
    if (params->source != CKZ_DATA_SPECIFIED ||
        (GetHashTypeFromMechanism(params->hashAlg) == HASH_AlgNULL) ||
        (GetHashTypeFromMechanism(params->mgf) == HASH_AlgNULL) ||
        (params->ulSourceDataLen == 0 && params->pSourceData != NULL) ||
        (params->ulSourceDataLen != 0 && params->pSourceData == NULL)) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

/*
 * return a context based on the SFTKContext type.
 */
SFTKSessionContext *
sftk_ReturnContextByType(SFTKSession *session, SFTKContextType type)
{
    switch (type) {
        case SFTK_ENCRYPT:
        case SFTK_DECRYPT:
            return session->enc_context;
        case SFTK_HASH:
            return session->hash_context;
        case SFTK_SIGN:
        case SFTK_SIGN_RECOVER:
        case SFTK_VERIFY:
        case SFTK_VERIFY_RECOVER:
            return session->hash_context;
    }
    return NULL;
}

/*
 * change a context based on the SFTKContext type.
 */
void
sftk_SetContextByType(SFTKSession *session, SFTKContextType type,
                      SFTKSessionContext *context)
{
    switch (type) {
        case SFTK_ENCRYPT:
        case SFTK_DECRYPT:
            session->enc_context = context;
            break;
        case SFTK_HASH:
            session->hash_context = context;
            break;
        case SFTK_SIGN:
        case SFTK_SIGN_RECOVER:
        case SFTK_VERIFY:
        case SFTK_VERIFY_RECOVER:
            session->hash_context = context;
            break;
    }
    return;
}

/*
 * code to grab the context. Needed by every C_XXXUpdate, C_XXXFinal,
 * and C_XXX function. The function takes a session handle, the context type,
 * and wether or not the session needs to be multipart. It returns the context,
 * and optionally returns the session pointer (if sessionPtr != NULL) if session
 * pointer is returned, the caller is responsible for freeing it.
 */
static CK_RV
sftk_GetContext(CK_SESSION_HANDLE handle, SFTKSessionContext **contextPtr,
                SFTKContextType type, PRBool needMulti, SFTKSession **sessionPtr)
{
    SFTKSession *session;
    SFTKSessionContext *context;

    session = sftk_SessionFromHandle(handle);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    context = sftk_ReturnContextByType(session, type);
    /* make sure the context is valid */
    if ((context == NULL) || (context->type != type) || (needMulti && !(context->multi))) {
        sftk_FreeSession(session);
        return CKR_OPERATION_NOT_INITIALIZED;
    }
    *contextPtr = context;
    if (sessionPtr != NULL) {
        *sessionPtr = session;
    } else {
        sftk_FreeSession(session);
    }
    return CKR_OK;
}

/** Terminate operation (in the PKCS#11 spec sense).
 *  Intuitive name for FreeContext/SetNullContext pair.
 */
static void
sftk_TerminateOp(SFTKSession *session, SFTKContextType ctype,
                 SFTKSessionContext *context)
{
    sftk_FreeContext(context);
    sftk_SetContextByType(session, ctype, NULL);
}

/*
 ************** Crypto Functions:     Encrypt ************************
 */

/*
 * All the NSC_InitXXX functions have a set of common checks and processing they
 * all need to do at the beginning. This is done here.
 */
static CK_RV
sftk_InitGeneric(SFTKSession *session, SFTKSessionContext **contextPtr,
                 SFTKContextType ctype, SFTKObject **keyPtr,
                 CK_OBJECT_HANDLE hKey, CK_KEY_TYPE *keyTypePtr,
                 CK_OBJECT_CLASS pubKeyType, CK_ATTRIBUTE_TYPE operation)
{
    SFTKObject *key = NULL;
    SFTKAttribute *att;
    SFTKSessionContext *context;

    /* We can only init if there is not current context active */
    if (sftk_ReturnContextByType(session, ctype) != NULL) {
        return CKR_OPERATION_ACTIVE;
    }

    /* find the key */
    if (keyPtr) {
        key = sftk_ObjectFromHandle(hKey, session);
        if (key == NULL) {
            return CKR_KEY_HANDLE_INVALID;
        }

        /* make sure it's a valid  key for this operation */
        if (((key->objclass != CKO_SECRET_KEY) && (key->objclass != pubKeyType)) || !sftk_isTrue(key, operation)) {
            sftk_FreeObject(key);
            return CKR_KEY_TYPE_INCONSISTENT;
        }
        /* get the key type */
        att = sftk_FindAttribute(key, CKA_KEY_TYPE);
        if (att == NULL) {
            sftk_FreeObject(key);
            return CKR_KEY_TYPE_INCONSISTENT;
        }
        PORT_Assert(att->attrib.ulValueLen == sizeof(CK_KEY_TYPE));
        if (att->attrib.ulValueLen != sizeof(CK_KEY_TYPE)) {
            sftk_FreeAttribute(att);
            sftk_FreeObject(key);
            return CKR_ATTRIBUTE_VALUE_INVALID;
        }
        PORT_Memcpy(keyTypePtr, att->attrib.pValue, sizeof(CK_KEY_TYPE));
        sftk_FreeAttribute(att);
        *keyPtr = key;
    }

    /* allocate the context structure */
    context = (SFTKSessionContext *)PORT_Alloc(sizeof(SFTKSessionContext));
    if (context == NULL) {
        if (key)
            sftk_FreeObject(key);
        return CKR_HOST_MEMORY;
    }
    context->type = ctype;
    context->multi = PR_TRUE;
    context->rsa = PR_FALSE;
    context->cipherInfo = NULL;
    context->hashInfo = NULL;
    context->doPad = PR_FALSE;
    context->padDataLength = 0;
    context->key = key;
    context->blockSize = 0;
    context->maxLen = 0;

    *contextPtr = context;
    return CKR_OK;
}

static int
sftk_aes_mode(CK_MECHANISM_TYPE mechanism)
{
    switch (mechanism) {
        case CKM_AES_CBC_PAD:
        case CKM_AES_CBC:
            return NSS_AES_CBC;
        case CKM_AES_ECB:
            return NSS_AES;
        case CKM_AES_CTS:
            return NSS_AES_CTS;
        case CKM_AES_CTR:
            return NSS_AES_CTR;
        case CKM_AES_GCM:
            return NSS_AES_GCM;
    }
    return -1;
}

static SECStatus
sftk_RSAEncryptRaw(NSSLOWKEYPublicKey *key, unsigned char *output,
                   unsigned int *outputLen, unsigned int maxLen,
                   const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = RSA_EncryptRaw(&key->u.rsa, output, outputLen, maxLen, input,
                        inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }

    return rv;
}

static SECStatus
sftk_RSADecryptRaw(NSSLOWKEYPrivateKey *key, unsigned char *output,
                   unsigned int *outputLen, unsigned int maxLen,
                   const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = RSA_DecryptRaw(&key->u.rsa, output, outputLen, maxLen, input,
                        inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }

    return rv;
}

static SECStatus
sftk_RSAEncrypt(NSSLOWKEYPublicKey *key, unsigned char *output,
                unsigned int *outputLen, unsigned int maxLen,
                const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = RSA_EncryptBlock(&key->u.rsa, output, outputLen, maxLen, input,
                          inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }

    return rv;
}

static SECStatus
sftk_RSADecrypt(NSSLOWKEYPrivateKey *key, unsigned char *output,
                unsigned int *outputLen, unsigned int maxLen,
                const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = RSA_DecryptBlock(&key->u.rsa, output, outputLen, maxLen, input,
                          inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }

    return rv;
}

static SECStatus
sftk_RSAEncryptOAEP(SFTKOAEPEncryptInfo *info, unsigned char *output,
                    unsigned int *outputLen, unsigned int maxLen,
                    const unsigned char *input, unsigned int inputLen)
{
    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;

    PORT_Assert(info->key->keyType == NSSLOWKEYRSAKey);
    if (info->key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(info->params->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(info->params->mgf);

    return RSA_EncryptOAEP(&info->key->u.rsa, hashAlg, maskHashAlg,
                           (const unsigned char *)info->params->pSourceData,
                           info->params->ulSourceDataLen, NULL, 0,
                           output, outputLen, maxLen, input, inputLen);
}

static SECStatus
sftk_RSADecryptOAEP(SFTKOAEPDecryptInfo *info, unsigned char *output,
                    unsigned int *outputLen, unsigned int maxLen,
                    const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;
    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;

    PORT_Assert(info->key->keyType == NSSLOWKEYRSAKey);
    if (info->key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(info->params->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(info->params->mgf);

    rv = RSA_DecryptOAEP(&info->key->u.rsa, hashAlg, maskHashAlg,
                         (const unsigned char *)info->params->pSourceData,
                         info->params->ulSourceDataLen,
                         output, outputLen, maxLen, input, inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }
    return rv;
}

static SFTKChaCha20Poly1305Info *
sftk_ChaCha20Poly1305_CreateContext(const unsigned char *key,
                                    unsigned int keyLen,
                                    const CK_NSS_AEAD_PARAMS *params)
{
    SFTKChaCha20Poly1305Info *ctx;

    if (params->ulNonceLen != sizeof(ctx->nonce)) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return NULL;
    }

    ctx = PORT_New(SFTKChaCha20Poly1305Info);
    if (ctx == NULL) {
        return NULL;
    }

    if (ChaCha20Poly1305_InitContext(&ctx->freeblCtx, key, keyLen,
                                     params->ulTagLen) != SECSuccess) {
        PORT_Free(ctx);
        return NULL;
    }

    PORT_Memcpy(ctx->nonce, params->pNonce, sizeof(ctx->nonce));

    /* AAD data and length must both be null, or both non-null. */
    PORT_Assert((params->pAAD == NULL) == (params->ulAADLen == 0));

    if (params->ulAADLen > sizeof(ctx->ad)) {
        /* Need to allocate an overflow buffer for the additional data. */
        ctx->adOverflow = (unsigned char *)PORT_Alloc(params->ulAADLen);
        if (!ctx->adOverflow) {
            PORT_Free(ctx);
            return NULL;
        }
        PORT_Memcpy(ctx->adOverflow, params->pAAD, params->ulAADLen);
    } else {
        ctx->adOverflow = NULL;
        if (params->pAAD) {
            PORT_Memcpy(ctx->ad, params->pAAD, params->ulAADLen);
        }
    }
    ctx->adLen = params->ulAADLen;

    return ctx;
}

static void
sftk_ChaCha20Poly1305_DestroyContext(SFTKChaCha20Poly1305Info *ctx,
                                     PRBool freeit)
{
    ChaCha20Poly1305_DestroyContext(&ctx->freeblCtx, PR_FALSE);
    if (ctx->adOverflow != NULL) {
        PORT_Free(ctx->adOverflow);
        ctx->adOverflow = NULL;
    }
    ctx->adLen = 0;
    if (freeit) {
        PORT_Free(ctx);
    }
}

static SECStatus
sftk_ChaCha20Poly1305_Encrypt(const SFTKChaCha20Poly1305Info *ctx,
                              unsigned char *output, unsigned int *outputLen,
                              unsigned int maxOutputLen,
                              const unsigned char *input, unsigned int inputLen)
{
    const unsigned char *ad = ctx->adOverflow;

    if (ad == NULL) {
        ad = ctx->ad;
    }

    return ChaCha20Poly1305_Seal(&ctx->freeblCtx, output, outputLen,
                                 maxOutputLen, input, inputLen, ctx->nonce,
                                 sizeof(ctx->nonce), ad, ctx->adLen);
}

static SECStatus
sftk_ChaCha20Poly1305_Decrypt(const SFTKChaCha20Poly1305Info *ctx,
                              unsigned char *output, unsigned int *outputLen,
                              unsigned int maxOutputLen,
                              const unsigned char *input, unsigned int inputLen)
{
    const unsigned char *ad = ctx->adOverflow;

    if (ad == NULL) {
        ad = ctx->ad;
    }

    return ChaCha20Poly1305_Open(&ctx->freeblCtx, output, outputLen,
                                 maxOutputLen, input, inputLen, ctx->nonce,
                                 sizeof(ctx->nonce), ad, ctx->adLen);
}

static SECStatus
sftk_ChaCha20Ctr(const SFTKChaCha20CtrInfo *ctx,
                 unsigned char *output, unsigned int *outputLen,
                 unsigned int maxOutputLen,
                 const unsigned char *input, unsigned int inputLen)
{
    if (maxOutputLen < inputLen) {
        PORT_SetError(SEC_ERROR_OUTPUT_LEN);
        return SECFailure;
    }
    ChaCha20_Xor(output, input, inputLen, ctx->key,
                 ctx->nonce, ctx->counter);
    *outputLen = inputLen;
    return SECSuccess;
}

static void
sftk_ChaCha20Ctr_DestroyContext(SFTKChaCha20CtrInfo *ctx,
                                PRBool freeit)
{
    memset(ctx, 0, sizeof(*ctx));
    if (freeit) {
        PORT_Free(ctx);
    }
}

/** NSC_CryptInit initializes an encryption/Decryption operation.
 *
 * Always called by NSC_EncryptInit, NSC_DecryptInit, NSC_WrapKey,NSC_UnwrapKey.
 * Called by NSC_SignInit, NSC_VerifyInit (via sftk_InitCBCMac) only for block
 *  ciphers MAC'ing.
 */
static CK_RV
sftk_CryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
               CK_OBJECT_HANDLE hKey,
               CK_ATTRIBUTE_TYPE mechUsage, CK_ATTRIBUTE_TYPE keyUsage,
               SFTKContextType contextType, PRBool isEncrypt)
{
    SFTKSession *session;
    SFTKObject *key;
    SFTKSessionContext *context;
    SFTKAttribute *att;
    CK_RC2_CBC_PARAMS *rc2_param;
#if NSS_SOFTOKEN_DOES_RC5
    CK_RC5_CBC_PARAMS *rc5_param;
    SECItem rc5Key;
#endif
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    unsigned effectiveKeyLength;
    unsigned char newdeskey[24];
    PRBool useNewKey = PR_FALSE;
    int t;

    if (!pMechanism) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    crv = sftk_MechAllowsOperation(pMechanism->mechanism, mechUsage);
    if (crv != CKR_OK)
        return crv;

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;

    crv = sftk_InitGeneric(session, &context, contextType, &key, hKey, &key_type,
                           isEncrypt ? CKO_PUBLIC_KEY : CKO_PRIVATE_KEY, keyUsage);

    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        return crv;
    }

    context->doPad = PR_FALSE;
    switch (pMechanism->mechanism) {
        case CKM_RSA_PKCS:
        case CKM_RSA_X_509:
            if (key_type != CKK_RSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            context->multi = PR_FALSE;
            context->rsa = PR_TRUE;
            if (isEncrypt) {
                NSSLOWKEYPublicKey *pubKey = sftk_GetPubKey(key, CKK_RSA, &crv);
                if (pubKey == NULL) {
                    crv = CKR_KEY_HANDLE_INVALID;
                    break;
                }
                context->maxLen = nsslowkey_PublicModulusLen(pubKey);
                context->cipherInfo = (void *)pubKey;
                context->update = (SFTKCipher)(pMechanism->mechanism == CKM_RSA_X_509
                                                   ? sftk_RSAEncryptRaw
                                                   : sftk_RSAEncrypt);
            } else {
                NSSLOWKEYPrivateKey *privKey = sftk_GetPrivKey(key, CKK_RSA, &crv);
                if (privKey == NULL) {
                    crv = CKR_KEY_HANDLE_INVALID;
                    break;
                }
                context->maxLen = nsslowkey_PrivateModulusLen(privKey);
                context->cipherInfo = (void *)privKey;
                context->update = (SFTKCipher)(pMechanism->mechanism == CKM_RSA_X_509
                                                   ? sftk_RSADecryptRaw
                                                   : sftk_RSADecrypt);
            }
            context->destroy = sftk_Null;
            break;
        case CKM_RSA_PKCS_OAEP:
            if (key_type != CKK_RSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            if (pMechanism->ulParameterLen != sizeof(CK_RSA_PKCS_OAEP_PARAMS) ||
                !sftk_ValidateOaepParams((CK_RSA_PKCS_OAEP_PARAMS *)pMechanism->pParameter)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            context->multi = PR_FALSE;
            context->rsa = PR_TRUE;
            if (isEncrypt) {
                SFTKOAEPEncryptInfo *info = PORT_New(SFTKOAEPEncryptInfo);
                if (info == NULL) {
                    crv = CKR_HOST_MEMORY;
                    break;
                }
                info->params = pMechanism->pParameter;
                info->key = sftk_GetPubKey(key, CKK_RSA, &crv);
                if (info->key == NULL) {
                    PORT_Free(info);
                    crv = CKR_KEY_HANDLE_INVALID;
                    break;
                }
                context->update = (SFTKCipher)sftk_RSAEncryptOAEP;
                context->maxLen = nsslowkey_PublicModulusLen(info->key);
                context->cipherInfo = info;
            } else {
                SFTKOAEPDecryptInfo *info = PORT_New(SFTKOAEPDecryptInfo);
                if (info == NULL) {
                    crv = CKR_HOST_MEMORY;
                    break;
                }
                info->params = pMechanism->pParameter;
                info->key = sftk_GetPrivKey(key, CKK_RSA, &crv);
                if (info->key == NULL) {
                    PORT_Free(info);
                    crv = CKR_KEY_HANDLE_INVALID;
                    break;
                }
                context->update = (SFTKCipher)sftk_RSADecryptOAEP;
                context->maxLen = nsslowkey_PrivateModulusLen(info->key);
                context->cipherInfo = info;
            }
            context->destroy = (SFTKDestroy)sftk_Space;
            break;
        case CKM_RC2_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_RC2_ECB:
        case CKM_RC2_CBC:
            context->blockSize = 8;
            if (key_type != CKK_RC2) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }

            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_RC2_CBC_PARAMS))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            rc2_param = (CK_RC2_CBC_PARAMS *)pMechanism->pParameter;
            effectiveKeyLength = (rc2_param->ulEffectiveBits + 7) / 8;
            context->cipherInfo =
                RC2_CreateContext((unsigned char *)att->attrib.pValue,
                                  att->attrib.ulValueLen, rc2_param->iv,
                                  pMechanism->mechanism == CKM_RC2_ECB ? NSS_RC2 : NSS_RC2_CBC, effectiveKeyLength);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? RC2_Encrypt : RC2_Decrypt);
            context->destroy = (SFTKDestroy)RC2_DestroyContext;
            break;
#if NSS_SOFTOKEN_DOES_RC5
        case CKM_RC5_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_RC5_ECB:
        case CKM_RC5_CBC:
            if (key_type != CKK_RC5) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }

            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_RC5_CBC_PARAMS))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            rc5_param = (CK_RC5_CBC_PARAMS *)pMechanism->pParameter;
            context->blockSize = rc5_param->ulWordsize * 2;
            rc5Key.data = (unsigned char *)att->attrib.pValue;
            rc5Key.len = att->attrib.ulValueLen;
            context->cipherInfo = RC5_CreateContext(&rc5Key, rc5_param->ulRounds,
                                                    rc5_param->ulWordsize, rc5_param->pIv,
                                                    pMechanism->mechanism == CKM_RC5_ECB ? NSS_RC5 : NSS_RC5_CBC);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? RC5_Encrypt : RC5_Decrypt);
            context->destroy = (SFTKDestroy)RC5_DestroyContext;
            break;
#endif
        case CKM_RC4:
            if (key_type != CKK_RC4) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            context->cipherInfo =
                RC4_CreateContext((unsigned char *)att->attrib.pValue,
                                  att->attrib.ulValueLen);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY; /* WRONG !!! */
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? RC4_Encrypt : RC4_Decrypt);
            context->destroy = (SFTKDestroy)RC4_DestroyContext;
            break;
        case CKM_CDMF_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_CDMF_ECB:
        case CKM_CDMF_CBC:
            if (key_type != CKK_CDMF) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            t = (pMechanism->mechanism == CKM_CDMF_ECB) ? NSS_DES : NSS_DES_CBC;
            goto finish_des;
        case CKM_DES_ECB:
            if (key_type != CKK_DES) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            t = NSS_DES;
            goto finish_des;
        case CKM_DES_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_DES_CBC:
            if (key_type != CKK_DES) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            t = NSS_DES_CBC;
            goto finish_des;
        case CKM_DES3_ECB:
            if ((key_type != CKK_DES2) && (key_type != CKK_DES3)) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            t = NSS_DES_EDE3;
            goto finish_des;
        case CKM_DES3_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_DES3_CBC:
            if ((key_type != CKK_DES2) && (key_type != CKK_DES3)) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            t = NSS_DES_EDE3_CBC;
        finish_des:
            context->blockSize = 8;
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            if (key_type == CKK_DES2 &&
                (t == NSS_DES_EDE3_CBC || t == NSS_DES_EDE3)) {
                /* extend DES2 key to DES3 key. */
                memcpy(newdeskey, att->attrib.pValue, 16);
                memcpy(newdeskey + 16, newdeskey, 8);
                useNewKey = PR_TRUE;
            } else if (key_type == CKK_CDMF) {
                crv = sftk_cdmf2des((unsigned char *)att->attrib.pValue, newdeskey);
                if (crv != CKR_OK) {
                    sftk_FreeAttribute(att);
                    break;
                }
                useNewKey = PR_TRUE;
            }
            context->cipherInfo = DES_CreateContext(
                useNewKey ? newdeskey : (unsigned char *)att->attrib.pValue,
                (unsigned char *)pMechanism->pParameter, t, isEncrypt);
            if (useNewKey)
                memset(newdeskey, 0, sizeof newdeskey);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? DES_Encrypt : DES_Decrypt);
            context->destroy = (SFTKDestroy)DES_DestroyContext;
            break;
        case CKM_SEED_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_SEED_CBC:
            if (!pMechanism->pParameter ||
                pMechanism->ulParameterLen != 16) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
        /* fall thru */
        case CKM_SEED_ECB:
            context->blockSize = 16;
            if (key_type != CKK_SEED) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            context->cipherInfo = SEED_CreateContext(
                (unsigned char *)att->attrib.pValue,
                (unsigned char *)pMechanism->pParameter,
                pMechanism->mechanism == CKM_SEED_ECB ? NSS_SEED : NSS_SEED_CBC,
                isEncrypt);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? SEED_Encrypt : SEED_Decrypt);
            context->destroy = (SFTKDestroy)SEED_DestroyContext;
            break;

        case CKM_CAMELLIA_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_CAMELLIA_CBC:
            if (!pMechanism->pParameter ||
                pMechanism->ulParameterLen != 16) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
        /* fall thru */
        case CKM_CAMELLIA_ECB:
            context->blockSize = 16;
            if (key_type != CKK_CAMELLIA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            context->cipherInfo = Camellia_CreateContext(
                (unsigned char *)att->attrib.pValue,
                (unsigned char *)pMechanism->pParameter,
                pMechanism->mechanism ==
                        CKM_CAMELLIA_ECB
                    ? NSS_CAMELLIA
                    : NSS_CAMELLIA_CBC,
                isEncrypt, att->attrib.ulValueLen);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? Camellia_Encrypt : Camellia_Decrypt);
            context->destroy = (SFTKDestroy)Camellia_DestroyContext;
            break;

        case CKM_AES_CBC_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_AES_ECB:
        case CKM_AES_CBC:
            context->blockSize = 16;
        case CKM_AES_CTS:
        case CKM_AES_CTR:
        case CKM_AES_GCM:
            if ((pMechanism->mechanism == CKM_AES_GCM && BAD_PARAM_CAST(pMechanism, sizeof(CK_GCM_PARAMS))) ||
                (pMechanism->mechanism == CKM_AES_CTR && BAD_PARAM_CAST(pMechanism, sizeof(CK_AES_CTR_PARAMS))) ||
                ((pMechanism->mechanism == CKM_AES_CBC || pMechanism->mechanism == CKM_AES_CTS) && BAD_PARAM_CAST(pMechanism, AES_BLOCK_SIZE))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }

            if (pMechanism->mechanism == CKM_AES_GCM) {
                context->multi = PR_FALSE;
            }
            if (key_type != CKK_AES) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            context->cipherInfo = AES_CreateContext(
                (unsigned char *)att->attrib.pValue,
                (unsigned char *)pMechanism->pParameter,
                sftk_aes_mode(pMechanism->mechanism),
                isEncrypt, att->attrib.ulValueLen, 16);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? AES_Encrypt : AES_Decrypt);
            context->destroy = (SFTKDestroy)AES_DestroyContext;
            break;

        case CKM_NSS_CHACHA20_POLY1305:
            if (pMechanism->ulParameterLen != sizeof(CK_NSS_AEAD_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            context->multi = PR_FALSE;
            if (key_type != CKK_NSS_CHACHA20) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            context->cipherInfo = sftk_ChaCha20Poly1305_CreateContext(
                (unsigned char *)att->attrib.pValue, att->attrib.ulValueLen,
                (CK_NSS_AEAD_PARAMS *)pMechanism->pParameter);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? sftk_ChaCha20Poly1305_Encrypt : sftk_ChaCha20Poly1305_Decrypt);
            context->destroy = (SFTKDestroy)sftk_ChaCha20Poly1305_DestroyContext;
            break;

        case CKM_NSS_CHACHA20_CTR:
            if (key_type != CKK_NSS_CHACHA20) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            if (pMechanism->pParameter == NULL || pMechanism->ulParameterLen != 16) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }

            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            SFTKChaCha20CtrInfo *ctx = PORT_ZNew(SFTKChaCha20CtrInfo);
            if (!ctx) {
                sftk_FreeAttribute(att);
                crv = CKR_HOST_MEMORY;
                break;
            }
            if (att->attrib.ulValueLen != sizeof(ctx->key)) {
                sftk_FreeAttribute(att);
                PORT_Free(ctx);
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            memcpy(ctx->key, att->attrib.pValue, att->attrib.ulValueLen);
            sftk_FreeAttribute(att);

            /* The counter is little endian. */
            PRUint8 *param = pMechanism->pParameter;
            int i = 0;
            for (; i < 4; ++i) {
                ctx->counter |= param[i] << (i * 8);
            }
            memcpy(ctx->nonce, param + 4, 12);
            context->cipherInfo = ctx;
            context->update = (SFTKCipher)sftk_ChaCha20Ctr;
            context->destroy = (SFTKDestroy)sftk_ChaCha20Ctr_DestroyContext;
            break;

        case CKM_NSS_AES_KEY_WRAP_PAD:
            context->doPad = PR_TRUE;
        /* fall thru */
        case CKM_NSS_AES_KEY_WRAP:
            context->multi = PR_FALSE;
            context->blockSize = 8;
            if (key_type != CKK_AES) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att = sftk_FindAttribute(key, CKA_VALUE);
            if (att == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            context->cipherInfo = AESKeyWrap_CreateContext(
                (unsigned char *)att->attrib.pValue,
                (unsigned char *)pMechanism->pParameter,
                isEncrypt, att->attrib.ulValueLen);
            sftk_FreeAttribute(att);
            if (context->cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->update = (SFTKCipher)(isEncrypt ? AESKeyWrap_Encrypt
                                                     : AESKeyWrap_Decrypt);
            context->destroy = (SFTKDestroy)AESKeyWrap_DestroyContext;
            break;

        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    if (crv != CKR_OK) {
        sftk_FreeContext(context);
        sftk_FreeSession(session);
        return crv;
    }
    sftk_SetContextByType(session, contextType, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_EncryptInit initializes an encryption operation. */
CK_RV
NSC_EncryptInit(CK_SESSION_HANDLE hSession,
                CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    CHECK_FORK();
    return sftk_CryptInit(hSession, pMechanism, hKey, CKA_ENCRYPT, CKA_ENCRYPT,
                          SFTK_ENCRYPT, PR_TRUE);
}

/* NSC_EncryptUpdate continues a multiple-part encryption operation. */
CK_RV
NSC_EncryptUpdate(CK_SESSION_HANDLE hSession,
                  CK_BYTE_PTR pPart, CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart,
                  CK_ULONG_PTR pulEncryptedPartLen)
{
    SFTKSessionContext *context;
    unsigned int outlen, i;
    unsigned int padoutlen = 0;
    unsigned int maxout = *pulEncryptedPartLen;
    CK_RV crv;
    SECStatus rv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_ENCRYPT, PR_TRUE, NULL);
    if (crv != CKR_OK)
        return crv;

    if (!pEncryptedPart) {
        if (context->doPad) {
            CK_ULONG totalDataAvailable = ulPartLen + context->padDataLength;
            CK_ULONG blocksToSend = totalDataAvailable / context->blockSize;

            *pulEncryptedPartLen = blocksToSend * context->blockSize;
            return CKR_OK;
        }
        *pulEncryptedPartLen = ulPartLen;
        return CKR_OK;
    }

    /* do padding */
    if (context->doPad) {
        /* deal with previous buffered data */
        if (context->padDataLength != 0) {
            /* fill in the padded to a full block size */
            for (i = context->padDataLength;
                 (ulPartLen != 0) && i < context->blockSize; i++) {
                context->padBuf[i] = *pPart++;
                ulPartLen--;
                context->padDataLength++;
            }

            /* not enough data to encrypt yet? then return */
            if (context->padDataLength != context->blockSize) {
                *pulEncryptedPartLen = 0;
                return CKR_OK;
            }
            /* encrypt the current padded data */
            rv = (*context->update)(context->cipherInfo, pEncryptedPart,
                                    &padoutlen, context->blockSize, context->padBuf,
                                    context->blockSize);
            if (rv != SECSuccess) {
                return sftk_MapCryptError(PORT_GetError());
            }
            pEncryptedPart += padoutlen;
            maxout -= padoutlen;
        }
        /* save the residual */
        context->padDataLength = ulPartLen % context->blockSize;
        if (context->padDataLength) {
            PORT_Memcpy(context->padBuf,
                        &pPart[ulPartLen - context->padDataLength],
                        context->padDataLength);
            ulPartLen -= context->padDataLength;
        }
        /* if we've exhausted our new buffer, we're done */
        if (ulPartLen == 0) {
            *pulEncryptedPartLen = padoutlen;
            return CKR_OK;
        }
    }

    /* do it: NOTE: this assumes buf size in is >= buf size out! */
    rv = (*context->update)(context->cipherInfo, pEncryptedPart,
                            &outlen, maxout, pPart, ulPartLen);
    if (rv != SECSuccess) {
        return sftk_MapCryptError(PORT_GetError());
    }
    *pulEncryptedPartLen = (CK_ULONG)(outlen + padoutlen);
    return CKR_OK;
}

/* NSC_EncryptFinal finishes a multiple-part encryption operation. */
CK_RV
NSC_EncryptFinal(CK_SESSION_HANDLE hSession,
                 CK_BYTE_PTR pLastEncryptedPart, CK_ULONG_PTR pulLastEncryptedPartLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int outlen, i;
    unsigned int maxout = *pulLastEncryptedPartLen;
    CK_RV crv;
    SECStatus rv = SECSuccess;
    PRBool contextFinished = PR_TRUE;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_ENCRYPT, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    *pulLastEncryptedPartLen = 0;
    if (!pLastEncryptedPart) {
        /* caller is checking the amount of remaining data */
        if (context->blockSize > 0 && context->doPad) {
            *pulLastEncryptedPartLen = context->blockSize;
            contextFinished = PR_FALSE; /* still have padding to go */
        }
        goto finish;
    }

    /* do padding */
    if (context->doPad) {
        unsigned char padbyte = (unsigned char)(context->blockSize - context->padDataLength);
        /* fill out rest of pad buffer with pad magic*/
        for (i = context->padDataLength; i < context->blockSize; i++) {
            context->padBuf[i] = padbyte;
        }
        rv = (*context->update)(context->cipherInfo, pLastEncryptedPart,
                                &outlen, maxout, context->padBuf, context->blockSize);
        if (rv == SECSuccess)
            *pulLastEncryptedPartLen = (CK_ULONG)outlen;
    }

finish:
    if (contextFinished)
        sftk_TerminateOp(session, SFTK_ENCRYPT, context);
    sftk_FreeSession(session);
    return (rv == SECSuccess) ? CKR_OK : sftk_MapCryptError(PORT_GetError());
}

/* NSC_Encrypt encrypts single-part data. */
CK_RV
NSC_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
            CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData,
            CK_ULONG_PTR pulEncryptedDataLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulEncryptedDataLen;
    CK_RV crv;
    CK_RV crv2;
    SECStatus rv = SECSuccess;
    SECItem pText;

    pText.type = siBuffer;
    pText.data = pData;
    pText.len = ulDataLen;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_ENCRYPT, PR_FALSE, &session);
    if (crv != CKR_OK)
        return crv;

    if (!pEncryptedData) {
        outlen = context->rsa ? context->maxLen : ulDataLen + 2 * context->blockSize;
        goto done;
    }

    if (context->doPad) {
        if (context->multi) {
            CK_ULONG updateLen = maxoutlen;
            CK_ULONG finalLen;
            /* padding is fairly complicated, have the update and final
             * code deal with it */
            sftk_FreeSession(session);
            crv = NSC_EncryptUpdate(hSession, pData, ulDataLen, pEncryptedData,
                                    &updateLen);
            if (crv != CKR_OK) {
                updateLen = 0;
            }
            maxoutlen -= updateLen;
            pEncryptedData += updateLen;
            finalLen = maxoutlen;
            crv2 = NSC_EncryptFinal(hSession, pEncryptedData, &finalLen);
            if (crv == CKR_OK && crv2 == CKR_OK) {
                *pulEncryptedDataLen = updateLen + finalLen;
            }
            return crv == CKR_OK ? crv2 : crv;
        }
        /* doPad without multi means that padding must be done on the first
        ** and only update.  There will be no final.
        */
        PORT_Assert(context->blockSize > 1);
        if (context->blockSize > 1) {
            CK_ULONG remainder = ulDataLen % context->blockSize;
            CK_ULONG padding = context->blockSize - remainder;
            pText.len += padding;
            pText.data = PORT_ZAlloc(pText.len);
            if (pText.data) {
                memcpy(pText.data, pData, ulDataLen);
                memset(pText.data + ulDataLen, padding, padding);
            } else {
                crv = CKR_HOST_MEMORY;
                goto fail;
            }
        }
    }

    /* do it: NOTE: this assumes buf size is big enough. */
    rv = (*context->update)(context->cipherInfo, pEncryptedData,
                            &outlen, maxoutlen, pText.data, pText.len);
    crv = (rv == SECSuccess) ? CKR_OK : sftk_MapCryptError(PORT_GetError());
    if (pText.data != pData)
        PORT_ZFree(pText.data, pText.len);
fail:
    sftk_TerminateOp(session, SFTK_ENCRYPT, context);
done:
    sftk_FreeSession(session);
    if (crv == CKR_OK) {
        *pulEncryptedDataLen = (CK_ULONG)outlen;
    }
    return crv;
}

/*
 ************** Crypto Functions:     Decrypt ************************
 */

/* NSC_DecryptInit initializes a decryption operation. */
CK_RV
NSC_DecryptInit(CK_SESSION_HANDLE hSession,
                CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    CHECK_FORK();
    return sftk_CryptInit(hSession, pMechanism, hKey, CKA_DECRYPT, CKA_DECRYPT,
                          SFTK_DECRYPT, PR_FALSE);
}

/* NSC_DecryptUpdate continues a multiple-part decryption operation. */
CK_RV
NSC_DecryptUpdate(CK_SESSION_HANDLE hSession,
                  CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen,
                  CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
    SFTKSessionContext *context;
    unsigned int padoutlen = 0;
    unsigned int outlen;
    unsigned int maxout = *pulPartLen;
    CK_RV crv;
    SECStatus rv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_DECRYPT, PR_TRUE, NULL);
    if (crv != CKR_OK)
        return crv;

    /* this can only happen on an NSS programming error */
    PORT_Assert((context->padDataLength == 0) || context->padDataLength == context->blockSize);

    if (context->doPad) {
        /* Check the data length for block ciphers. If we are padding,
         * then we must be using a block cipher. In the non-padding case
         * the error will be returned by the underlying decryption
         * function when we do the actual decrypt. We need to do the
         * check here to avoid returning a negative length to the caller
         * or reading before the beginning of the pEncryptedPart buffer.
         */
        if ((ulEncryptedPartLen == 0) ||
            (ulEncryptedPartLen % context->blockSize) != 0) {
            return CKR_ENCRYPTED_DATA_LEN_RANGE;
        }
    }

    if (!pPart) {
        if (context->doPad) {
            *pulPartLen =
                ulEncryptedPartLen + context->padDataLength - context->blockSize;
            return CKR_OK;
        }
        /* for stream ciphers there is are no constraints on ulEncryptedPartLen.
         * for block ciphers, it must be a multiple of blockSize. The error is
         * detected when this function is called again do decrypt the output.
         */
        *pulPartLen = ulEncryptedPartLen;
        return CKR_OK;
    }

    if (context->doPad) {
        /* first decrypt our saved buffer */
        if (context->padDataLength != 0) {
            rv = (*context->update)(context->cipherInfo, pPart, &padoutlen,
                                    maxout, context->padBuf, context->blockSize);
            if (rv != SECSuccess)
                return sftk_MapDecryptError(PORT_GetError());
            pPart += padoutlen;
            maxout -= padoutlen;
        }
        /* now save the final block for the next decrypt or the final */
        PORT_Memcpy(context->padBuf, &pEncryptedPart[ulEncryptedPartLen - context->blockSize],
                    context->blockSize);
        context->padDataLength = context->blockSize;
        ulEncryptedPartLen -= context->padDataLength;
    }

    /* do it: NOTE: this assumes buf size in is >= buf size out! */
    rv = (*context->update)(context->cipherInfo, pPart, &outlen,
                            maxout, pEncryptedPart, ulEncryptedPartLen);
    if (rv != SECSuccess) {
        return sftk_MapDecryptError(PORT_GetError());
    }
    *pulPartLen = (CK_ULONG)(outlen + padoutlen);
    return CKR_OK;
}

/* NSC_DecryptFinal finishes a multiple-part decryption operation. */
CK_RV
NSC_DecryptFinal(CK_SESSION_HANDLE hSession,
                 CK_BYTE_PTR pLastPart, CK_ULONG_PTR pulLastPartLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int outlen;
    unsigned int maxout = *pulLastPartLen;
    CK_RV crv;
    SECStatus rv = SECSuccess;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_DECRYPT, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    *pulLastPartLen = 0;
    if (!pLastPart) {
        /* caller is checking the amount of remaining data */
        if (context->padDataLength > 0) {
            *pulLastPartLen = context->padDataLength;
        }
        goto finish;
    }

    if (context->doPad) {
        /* decrypt our saved buffer */
        if (context->padDataLength != 0) {
            /* this assumes that pLastPart is big enough to hold the *whole*
             * buffer!!! */
            rv = (*context->update)(context->cipherInfo, pLastPart, &outlen,
                                    maxout, context->padBuf, context->blockSize);
            if (rv != SECSuccess) {
                crv = sftk_MapDecryptError(PORT_GetError());
            } else {
                unsigned int padSize =
                    (unsigned int)pLastPart[context->blockSize - 1];
                if ((padSize > context->blockSize) || (padSize == 0)) {
                    crv = CKR_ENCRYPTED_DATA_INVALID;
                } else {
                    unsigned int i;
                    unsigned int badPadding = 0; /* used as a boolean */
                    for (i = 0; i < padSize; i++) {
                        badPadding |=
                            (unsigned int)pLastPart[context->blockSize - 1 - i] ^
                            padSize;
                    }
                    if (badPadding) {
                        crv = CKR_ENCRYPTED_DATA_INVALID;
                    } else {
                        *pulLastPartLen = outlen - padSize;
                    }
                }
            }
        }
    }

    sftk_TerminateOp(session, SFTK_DECRYPT, context);
finish:
    sftk_FreeSession(session);
    return crv;
}

/* NSC_Decrypt decrypts encrypted data in a single part. */
CK_RV
NSC_Decrypt(CK_SESSION_HANDLE hSession,
            CK_BYTE_PTR pEncryptedData, CK_ULONG ulEncryptedDataLen, CK_BYTE_PTR pData,
            CK_ULONG_PTR pulDataLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulDataLen;
    CK_RV crv;
    CK_RV crv2;
    SECStatus rv = SECSuccess;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_DECRYPT, PR_FALSE, &session);
    if (crv != CKR_OK)
        return crv;

    if (!pData) {
        outlen = ulEncryptedDataLen + context->blockSize;
        goto done;
    }

    if (context->doPad && context->multi) {
        CK_ULONG updateLen = maxoutlen;
        CK_ULONG finalLen;
        /* padding is fairly complicated, have the update and final
         * code deal with it */
        sftk_FreeSession(session);
        crv = NSC_DecryptUpdate(hSession, pEncryptedData, ulEncryptedDataLen,
                                pData, &updateLen);
        if (crv == CKR_OK) {
            maxoutlen -= updateLen;
            pData += updateLen;
        }
        finalLen = maxoutlen;
        crv2 = NSC_DecryptFinal(hSession, pData, &finalLen);
        if (crv == CKR_OK && crv2 == CKR_OK) {
            *pulDataLen = updateLen + finalLen;
        }
        return crv == CKR_OK ? crv2 : crv;
    }

    rv = (*context->update)(context->cipherInfo, pData, &outlen, maxoutlen,
                            pEncryptedData, ulEncryptedDataLen);
    /* XXX need to do MUCH better error mapping than this. */
    crv = (rv == SECSuccess) ? CKR_OK : sftk_MapDecryptError(PORT_GetError());
    if (rv == SECSuccess && context->doPad) {
        unsigned int padding = pData[outlen - 1];
        if (padding > context->blockSize || !padding) {
            crv = CKR_ENCRYPTED_DATA_INVALID;
        } else {
            unsigned int i;
            unsigned int badPadding = 0; /* used as a boolean */
            for (i = 0; i < padding; i++) {
                badPadding |= (unsigned int)pData[outlen - 1 - i] ^ padding;
            }
            if (badPadding) {
                crv = CKR_ENCRYPTED_DATA_INVALID;
            } else {
                outlen -= padding;
            }
        }
    }
    sftk_TerminateOp(session, SFTK_DECRYPT, context);
done:
    sftk_FreeSession(session);
    if (crv == CKR_OK) {
        *pulDataLen = (CK_ULONG)outlen;
    }
    return crv;
}

/*
 ************** Crypto Functions:     Digest (HASH)  ************************
 */

/* NSC_DigestInit initializes a message-digesting operation. */
CK_RV
NSC_DigestInit(CK_SESSION_HANDLE hSession,
               CK_MECHANISM_PTR pMechanism)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    CK_RV crv = CKR_OK;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    crv = sftk_InitGeneric(session, &context, SFTK_HASH, NULL, 0, NULL, 0, 0);
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        return crv;
    }

#define INIT_MECH(mech, mmm)                                   \
    case mech: {                                               \
        mmm##Context *mmm##_ctx = mmm##_NewContext();          \
        context->cipherInfo = (void *)mmm##_ctx;               \
        context->cipherInfoLen = mmm##_FlattenSize(mmm##_ctx); \
        context->currentMech = mech;                           \
        context->hashUpdate = (SFTKHash)mmm##_Update;          \
        context->end = (SFTKEnd)mmm##_End;                     \
        context->destroy = (SFTKDestroy)mmm##_DestroyContext;  \
        context->maxLen = mmm##_LENGTH;                        \
        if (mmm##_ctx)                                         \
            mmm##_Begin(mmm##_ctx);                            \
        else                                                   \
            crv = CKR_HOST_MEMORY;                             \
        break;                                                 \
    }

    switch (pMechanism->mechanism) {
        INIT_MECH(CKM_MD2, MD2)
        INIT_MECH(CKM_MD5, MD5)
        INIT_MECH(CKM_SHA_1, SHA1)
        INIT_MECH(CKM_SHA224, SHA224)
        INIT_MECH(CKM_SHA256, SHA256)
        INIT_MECH(CKM_SHA384, SHA384)
        INIT_MECH(CKM_SHA512, SHA512)

        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    if (crv != CKR_OK) {
        sftk_FreeContext(context);
        sftk_FreeSession(session);
        return crv;
    }
    sftk_SetContextByType(session, SFTK_HASH, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_Digest digests data in a single part. */
CK_RV
NSC_Digest(CK_SESSION_HANDLE hSession,
           CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pDigest,
           CK_ULONG_PTR pulDigestLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int digestLen;
    unsigned int maxout = *pulDigestLen;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_HASH, PR_FALSE, &session);
    if (crv != CKR_OK)
        return crv;

    if (pDigest == NULL) {
        *pulDigestLen = context->maxLen;
        goto finish;
    }

    /* do it: */
    (*context->hashUpdate)(context->cipherInfo, pData, ulDataLen);
    /*  NOTE: this assumes buf size is bigenough for the algorithm */
    (*context->end)(context->cipherInfo, pDigest, &digestLen, maxout);
    *pulDigestLen = digestLen;

    sftk_TerminateOp(session, SFTK_HASH, context);
finish:
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_DigestUpdate continues a multiple-part message-digesting operation. */
CK_RV
NSC_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                 CK_ULONG ulPartLen)
{
    SFTKSessionContext *context;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_HASH, PR_TRUE, NULL);
    if (crv != CKR_OK)
        return crv;
    /* do it: */
    (*context->hashUpdate)(context->cipherInfo, pPart, ulPartLen);
    return CKR_OK;
}

/* NSC_DigestFinal finishes a multiple-part message-digesting operation. */
CK_RV
NSC_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
                CK_ULONG_PTR pulDigestLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int maxout = *pulDigestLen;
    unsigned int digestLen;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_HASH, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    if (pDigest != NULL) {
        (*context->end)(context->cipherInfo, pDigest, &digestLen, maxout);
        *pulDigestLen = digestLen;
        sftk_TerminateOp(session, SFTK_HASH, context);
    } else {
        *pulDigestLen = context->maxLen;
    }

    sftk_FreeSession(session);
    return CKR_OK;
}

/*
 * these helper functions are used by Generic Macing and Signing functions
 * that use hashes as part of their operations.
 */
#define DOSUB(mmm)                                                \
    static CK_RV                                                  \
        sftk_doSub##mmm(SFTKSessionContext *context)              \
    {                                                             \
        mmm##Context *mmm##_ctx = mmm##_NewContext();             \
        context->hashInfo = (void *)mmm##_ctx;                    \
        context->hashUpdate = (SFTKHash)mmm##_Update;             \
        context->end = (SFTKEnd)mmm##_End;                        \
        context->hashdestroy = (SFTKDestroy)mmm##_DestroyContext; \
        if (!context->hashInfo) {                                 \
            return CKR_HOST_MEMORY;                               \
        }                                                         \
        mmm##_Begin(mmm##_ctx);                                   \
        return CKR_OK;                                            \
    }

DOSUB(MD2)
DOSUB(MD5)
DOSUB(SHA1)
DOSUB(SHA224)
DOSUB(SHA256)
DOSUB(SHA384)
DOSUB(SHA512)

static SECStatus
sftk_SignCopy(
    CK_ULONG *copyLen,
    void *out, unsigned int *outLength,
    unsigned int maxLength,
    const unsigned char *hashResult,
    unsigned int hashResultLength)
{
    unsigned int toCopy = *copyLen;
    if (toCopy > maxLength) {
        toCopy = maxLength;
    }
    if (toCopy > hashResultLength) {
        toCopy = hashResultLength;
    }
    memcpy(out, hashResult, toCopy);
    if (outLength) {
        *outLength = toCopy;
    }
    return SECSuccess;
}

/* Verify is just a compare for HMAC */
static SECStatus
sftk_HMACCmp(CK_ULONG *copyLen, unsigned char *sig, unsigned int sigLen,
             unsigned char *hash, unsigned int hashLen)
{
    return (NSS_SecureMemcmp(sig, hash, *copyLen) == 0) ? SECSuccess : SECFailure;
}

/*
 * common HMAC initalization routine
 */
static CK_RV
sftk_doHMACInit(SFTKSessionContext *context, HASH_HashType hash,
                SFTKObject *key, CK_ULONG mac_size)
{
    SFTKAttribute *keyval;
    HMACContext *HMACcontext;
    CK_ULONG *intpointer;
    const SECHashObject *hashObj = HASH_GetRawHashObject(hash);
    PRBool isFIPS = (key->slot->slotID == FIPS_SLOT_ID);

    /* required by FIPS 198 Section 4 */
    if (isFIPS && (mac_size < 4 || mac_size < hashObj->length / 2)) {
        return CKR_BUFFER_TOO_SMALL;
    }

    keyval = sftk_FindAttribute(key, CKA_VALUE);
    if (keyval == NULL)
        return CKR_KEY_SIZE_RANGE;

    HMACcontext = HMAC_Create(hashObj,
                              (const unsigned char *)keyval->attrib.pValue,
                              keyval->attrib.ulValueLen, isFIPS);
    context->hashInfo = HMACcontext;
    context->multi = PR_TRUE;
    sftk_FreeAttribute(keyval);
    if (context->hashInfo == NULL) {
        if (PORT_GetError() == SEC_ERROR_INVALID_ARGS) {
            return CKR_KEY_SIZE_RANGE;
        }
        return CKR_HOST_MEMORY;
    }
    context->hashUpdate = (SFTKHash)HMAC_Update;
    context->end = (SFTKEnd)HMAC_Finish;

    context->hashdestroy = (SFTKDestroy)HMAC_Destroy;
    intpointer = PORT_New(CK_ULONG);
    if (intpointer == NULL) {
        return CKR_HOST_MEMORY;
    }
    *intpointer = mac_size;
    context->cipherInfo = intpointer;
    context->destroy = (SFTKDestroy)sftk_Space;
    context->update = (SFTKCipher)sftk_SignCopy;
    context->verify = (SFTKVerify)sftk_HMACCmp;
    context->maxLen = hashObj->length;
    HMAC_Begin(HMACcontext);
    return CKR_OK;
}

/*
 * common CMAC initialization routine
 */
static CK_RV
sftk_doCMACInit(SFTKSessionContext *session, CMACCipher type,
                SFTKObject *key, CK_ULONG mac_size)
{
    SFTKAttribute *keyval;
    CMACContext *cmacContext;
    CK_ULONG *intpointer;

    /* Unlike HMAC, CMAC doesn't need to check key sizes as the underlying
     * block cipher does this for us: block ciphers support only a single
     * key size per variant.
     *
     * To introduce support for a CMAC based on a new block cipher, first add
     * support for the relevant block cipher to CMAC in the freebl layer. Then
     * update the switch statement at the end of this function. Also remember
     * to update the switch statement in NSC_SignInit with the PKCS#11
     * mechanism constants.
     */

    keyval = sftk_FindAttribute(key, CKA_VALUE);
    if (keyval == NULL) {
        return CKR_KEY_SIZE_RANGE;
    }

    /* Create the underlying CMACContext and associate it with the
     * SFTKSessionContext's hashInfo field */
    cmacContext = CMAC_Create(type,
                              (const unsigned char *)keyval->attrib.pValue,
                              keyval->attrib.ulValueLen);
    sftk_FreeAttribute(keyval);

    if (cmacContext == NULL) {
        if (PORT_GetError() == SEC_ERROR_INVALID_ARGS) {
            return CKR_KEY_SIZE_RANGE;
        }

        return CKR_HOST_MEMORY;
    }
    session->hashInfo = cmacContext;

    /* MACs all behave roughly the same. However, CMAC can fail because
     * the underlying cipher can fail. In practice, this shouldn't occur
     * because we're not using any chaining modes, letting us safely ignore
     * the return value. */
    session->multi = PR_TRUE;
    session->hashUpdate = (SFTKHash)CMAC_Update;
    session->end = (SFTKEnd)CMAC_Finish;
    session->hashdestroy = (SFTKDestroy)CMAC_Destroy;

    intpointer = PORT_New(CK_ULONG);
    if (intpointer == NULL) {
        return CKR_HOST_MEMORY;
    }
    *intpointer = mac_size;
    session->cipherInfo = intpointer;

    /* Since we're only "hashing", copy the result from session->end to the
     * caller using sftk_SignCopy. */
    session->update = (SFTKCipher)sftk_SignCopy;
    session->verify = (SFTKVerify)sftk_HMACCmp;
    session->destroy = (SFTKDestroy)sftk_Space;

    /* Will need to be updated for additional block ciphers in the future. */
    switch (type) {
        case CMAC_AES:
            session->maxLen = AES_BLOCK_SIZE;
            break;
        default:
            PORT_Assert(0);
            return CKR_KEY_SIZE_RANGE;
    }

    return CKR_OK;
}

/*
 *  SSL Macing support. SSL Macs are inited, then update with the base
 * hashing algorithm, then finalized in sign and verify
 */

/*
 * FROM SSL:
 * 60 bytes is 3 times the maximum length MAC size that is supported.
 * We probably should have one copy of this table. We still need this table
 * in ssl to 'sign' the handshake hashes.
 */
static unsigned char ssl_pad_1[60] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36
};
static unsigned char ssl_pad_2[60] = {
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c
};

static SECStatus
sftk_SSLMACSign(SFTKSSLMACInfo *info, unsigned char *sig, unsigned int *sigLen,
                unsigned int maxLen, unsigned char *hash, unsigned int hashLen)
{
    unsigned char tmpBuf[SFTK_MAX_MAC_LENGTH];
    unsigned int out;

    info->begin(info->hashContext);
    info->update(info->hashContext, info->key, info->keySize);
    info->update(info->hashContext, ssl_pad_2, info->padSize);
    info->update(info->hashContext, hash, hashLen);
    info->end(info->hashContext, tmpBuf, &out, SFTK_MAX_MAC_LENGTH);
    PORT_Memcpy(sig, tmpBuf, info->macSize);
    *sigLen = info->macSize;
    return SECSuccess;
}

static SECStatus
sftk_SSLMACVerify(SFTKSSLMACInfo *info, unsigned char *sig, unsigned int sigLen,
                  unsigned char *hash, unsigned int hashLen)
{
    unsigned char tmpBuf[SFTK_MAX_MAC_LENGTH];
    unsigned int out;

    info->begin(info->hashContext);
    info->update(info->hashContext, info->key, info->keySize);
    info->update(info->hashContext, ssl_pad_2, info->padSize);
    info->update(info->hashContext, hash, hashLen);
    info->end(info->hashContext, tmpBuf, &out, SFTK_MAX_MAC_LENGTH);
    return (NSS_SecureMemcmp(sig, tmpBuf, info->macSize) == 0) ? SECSuccess : SECFailure;
}

/*
 * common HMAC initalization routine
 */
static CK_RV
sftk_doSSLMACInit(SFTKSessionContext *context, SECOidTag oid,
                  SFTKObject *key, CK_ULONG mac_size)
{
    SFTKAttribute *keyval;
    SFTKBegin begin;
    int padSize;
    SFTKSSLMACInfo *sslmacinfo;
    CK_RV crv = CKR_MECHANISM_INVALID;

    if (oid == SEC_OID_SHA1) {
        crv = sftk_doSubSHA1(context);
        if (crv != CKR_OK)
            return crv;
        begin = (SFTKBegin)SHA1_Begin;
        padSize = 40;
    } else {
        crv = sftk_doSubMD5(context);
        if (crv != CKR_OK)
            return crv;
        begin = (SFTKBegin)MD5_Begin;
        padSize = 48;
    }
    context->multi = PR_TRUE;

    keyval = sftk_FindAttribute(key, CKA_VALUE);
    if (keyval == NULL)
        return CKR_KEY_SIZE_RANGE;

    context->hashUpdate(context->hashInfo, keyval->attrib.pValue,
                        keyval->attrib.ulValueLen);
    context->hashUpdate(context->hashInfo, ssl_pad_1, padSize);
    sslmacinfo = (SFTKSSLMACInfo *)PORT_Alloc(sizeof(SFTKSSLMACInfo));
    if (sslmacinfo == NULL) {
        sftk_FreeAttribute(keyval);
        return CKR_HOST_MEMORY;
    }
    sslmacinfo->macSize = mac_size;
    sslmacinfo->hashContext = context->hashInfo;
    PORT_Memcpy(sslmacinfo->key, keyval->attrib.pValue,
                keyval->attrib.ulValueLen);
    sslmacinfo->keySize = keyval->attrib.ulValueLen;
    sslmacinfo->begin = begin;
    sslmacinfo->end = context->end;
    sslmacinfo->update = context->hashUpdate;
    sslmacinfo->padSize = padSize;
    sftk_FreeAttribute(keyval);
    context->cipherInfo = (void *)sslmacinfo;
    context->destroy = (SFTKDestroy)sftk_Space;
    context->update = (SFTKCipher)sftk_SSLMACSign;
    context->verify = (SFTKVerify)sftk_SSLMACVerify;
    context->maxLen = mac_size;
    return CKR_OK;
}

/*
 ************** Crypto Functions:     Sign  ************************
 */

/**
 * Check if We're using CBCMacing and initialize the session context if we are.
 *  @param contextType SFTK_SIGN or SFTK_VERIFY
 *  @param keyUsage    check whether key allows this usage
 */
static CK_RV
sftk_InitCBCMac(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                CK_OBJECT_HANDLE hKey, CK_ATTRIBUTE_TYPE keyUsage,
                SFTKContextType contextType)

{
    CK_MECHANISM cbc_mechanism;
    CK_ULONG mac_bytes = SFTK_INVALID_MAC_SIZE;
    CK_RC2_CBC_PARAMS rc2_params;
#if NSS_SOFTOKEN_DOES_RC5
    CK_RC5_CBC_PARAMS rc5_params;
    CK_RC5_MAC_GENERAL_PARAMS *rc5_mac;
#endif
    unsigned char ivBlock[SFTK_MAX_BLOCK_SIZE];
    unsigned char k2[SFTK_MAX_BLOCK_SIZE];
    unsigned char k3[SFTK_MAX_BLOCK_SIZE];
    SFTKSessionContext *context;
    CK_RV crv;
    unsigned int blockSize;
    PRBool isXCBC = PR_FALSE;

    if (!pMechanism) {
        return CKR_MECHANISM_PARAM_INVALID;
    }

    switch (pMechanism->mechanism) {
        case CKM_RC2_MAC_GENERAL:
            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_RC2_MAC_GENERAL_PARAMS))) {
                return CKR_MECHANISM_PARAM_INVALID;
            }
            mac_bytes =
                ((CK_RC2_MAC_GENERAL_PARAMS *)pMechanism->pParameter)->ulMacLength;
        /* fall through */
        case CKM_RC2_MAC:
            /* this works because ulEffectiveBits is in the same place in both the
             * CK_RC2_MAC_GENERAL_PARAMS and CK_RC2_CBC_PARAMS */
            rc2_params.ulEffectiveBits = ((CK_RC2_MAC_GENERAL_PARAMS *)
                                              pMechanism->pParameter)
                                             ->ulEffectiveBits;
            PORT_Memset(rc2_params.iv, 0, sizeof(rc2_params.iv));
            cbc_mechanism.mechanism = CKM_RC2_CBC;
            cbc_mechanism.pParameter = &rc2_params;
            cbc_mechanism.ulParameterLen = sizeof(rc2_params);
            blockSize = 8;
            break;
#if NSS_SOFTOKEN_DOES_RC5
        case CKM_RC5_MAC_GENERAL:
            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_RC5_MAC_GENERAL_PARAMS))) {
                return CKR_MECHANISM_PARAM_INVALID;
            }
            mac_bytes =
                ((CK_RC5_MAC_GENERAL_PARAMS *)pMechanism->pParameter)->ulMacLength;
        /* fall through */
        case CKM_RC5_MAC:
            /* this works because ulEffectiveBits is in the same place in both the
             * CK_RC5_MAC_GENERAL_PARAMS and CK_RC5_CBC_PARAMS */
            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_RC5_MAC_GENERAL_PARAMS))) {
                return CKR_MECHANISM_PARAM_INVALID;
            }
            rc5_mac = (CK_RC5_MAC_GENERAL_PARAMS *)pMechanism->pParameter;
            rc5_params.ulWordsize = rc5_mac->ulWordsize;
            rc5_params.ulRounds = rc5_mac->ulRounds;
            rc5_params.pIv = ivBlock;
            if ((blockSize = rc5_mac->ulWordsize * 2) > SFTK_MAX_BLOCK_SIZE)
                return CKR_MECHANISM_PARAM_INVALID;
            rc5_params.ulIvLen = blockSize;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_RC5_CBC;
            cbc_mechanism.pParameter = &rc5_params;
            cbc_mechanism.ulParameterLen = sizeof(rc5_params);
            break;
#endif
        /* add cast and idea later */
        case CKM_DES_MAC_GENERAL:
            mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
        /* fall through */
        case CKM_DES_MAC:
            blockSize = 8;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_DES_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            break;
        case CKM_DES3_MAC_GENERAL:
            mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
        /* fall through */
        case CKM_DES3_MAC:
            blockSize = 8;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_DES3_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            break;
        case CKM_CDMF_MAC_GENERAL:
            mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
        /* fall through */
        case CKM_CDMF_MAC:
            blockSize = 8;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_CDMF_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            break;
        case CKM_SEED_MAC_GENERAL:
            mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
        /* fall through */
        case CKM_SEED_MAC:
            blockSize = 16;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_SEED_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            break;
        case CKM_CAMELLIA_MAC_GENERAL:
            mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
        /* fall through */
        case CKM_CAMELLIA_MAC:
            blockSize = 16;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_CAMELLIA_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            break;
        case CKM_AES_MAC_GENERAL:
            mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
        /* fall through */
        case CKM_AES_MAC:
            blockSize = 16;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_AES_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            break;
        case CKM_AES_XCBC_MAC_96:
        case CKM_AES_XCBC_MAC:
            /* The only difference between CKM_AES_XCBC_MAC
             * and CKM_AES_XCBC_MAC_96 is the size of the returned mac. */
            mac_bytes = pMechanism->mechanism == CKM_AES_XCBC_MAC_96 ? 12 : 16;
            blockSize = 16;
            PORT_Memset(ivBlock, 0, blockSize);
            cbc_mechanism.mechanism = CKM_AES_CBC;
            cbc_mechanism.pParameter = &ivBlock;
            cbc_mechanism.ulParameterLen = blockSize;
            /* is XCBC requires extra processing at the end of the operation */
            isXCBC = PR_TRUE;
            /* The input key is used to generate k1, k2, and k3. k2 and k3
             * are used at the end in the pad step. k1 replaces the input
             * key in the aes cbc mac */
            crv = sftk_aes_xcbc_new_keys(hSession, hKey, &hKey, k2, k3);
            if (crv != CKR_OK) {
                return crv;
            }
            break;
        default:
            return CKR_FUNCTION_NOT_SUPPORTED;
    }

    /* if MAC size is externally supplied, it should be checked.
     */
    if (mac_bytes == SFTK_INVALID_MAC_SIZE)
        mac_bytes = blockSize >> 1;
    else {
        if (mac_bytes > blockSize) {
            crv = CKR_MECHANISM_PARAM_INVALID;
            goto fail;
        }
    }

    crv = sftk_CryptInit(hSession, &cbc_mechanism, hKey,
                         CKA_ENCRYPT, /* CBC mech is able to ENCRYPT, not SIGN/VERIFY */
                         keyUsage, contextType, PR_TRUE);
    if (crv != CKR_OK)
        goto fail;
    crv = sftk_GetContext(hSession, &context, contextType, PR_TRUE, NULL);

    /* this shouldn't happen! */
    PORT_Assert(crv == CKR_OK);
    if (crv != CKR_OK)
        goto fail;
    context->blockSize = blockSize;
    context->macSize = mac_bytes;
    context->isXCBC = isXCBC;
    if (isXCBC) {
        /* save the xcbc specific parameters */
        PORT_Memcpy(context->k2, k2, blockSize);
        PORT_Memcpy(context->k3, k3, blockSize);
        PORT_Memset(k2, 0, blockSize);
        PORT_Memset(k3, 0, blockSize);
        /* get rid of the temp key now that the context has been created */
        NSC_DestroyObject(hSession, hKey);
    }
    return CKR_OK;
fail:
    if (isXCBC) {
        PORT_Memset(k2, 0, blockSize);
        PORT_Memset(k3, 0, blockSize);
        NSC_DestroyObject(hSession, hKey); /* get rid of our temp key */
    }
    return crv;
}

/*
 * encode RSA PKCS #1 Signature data before signing...
 */
static SECStatus
sftk_RSAHashSign(SFTKHashSignInfo *info, unsigned char *sig,
                 unsigned int *sigLen, unsigned int maxLen,
                 const unsigned char *hash, unsigned int hashLen)
{
    PORT_Assert(info->key->keyType == NSSLOWKEYRSAKey);
    if (info->key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    return RSA_HashSign(info->hashOid, info->key, sig, sigLen, maxLen,
                        hash, hashLen);
}

/* XXX Old template; want to expunge it eventually. */
static DERTemplate SECAlgorithmIDTemplate[] = {
    { DER_SEQUENCE,
      0, NULL, sizeof(SECAlgorithmID) },
    { DER_OBJECT_ID,
      offsetof(SECAlgorithmID, algorithm) },
    { DER_OPTIONAL | DER_ANY,
      offsetof(SECAlgorithmID, parameters) },
    { 0 }
};

/*
 * XXX OLD Template.  Once all uses have been switched over to new one,
 * remove this.
 */
static DERTemplate SGNDigestInfoTemplate[] = {
    { DER_SEQUENCE,
      0, NULL, sizeof(SGNDigestInfo) },
    { DER_INLINE,
      offsetof(SGNDigestInfo, digestAlgorithm),
      SECAlgorithmIDTemplate },
    { DER_OCTET_STRING,
      offsetof(SGNDigestInfo, digest) },
    { 0 }
};

/*
 * encode RSA PKCS #1 Signature data before signing...
 */
SECStatus
RSA_HashSign(SECOidTag hashOid, NSSLOWKEYPrivateKey *key,
             unsigned char *sig, unsigned int *sigLen, unsigned int maxLen,
             const unsigned char *hash, unsigned int hashLen)
{
    SECStatus rv = SECFailure;
    SECItem digder;
    PLArenaPool *arena = NULL;
    SGNDigestInfo *di = NULL;

    digder.data = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        goto loser;
    }

    /* Construct digest info */
    di = SGN_CreateDigestInfo(hashOid, hash, hashLen);
    if (!di) {
        goto loser;
    }

    /* Der encode the digest as a DigestInfo */
    rv = DER_Encode(arena, &digder, SGNDigestInfoTemplate, di);
    if (rv != SECSuccess) {
        goto loser;
    }

    /*
    ** Encrypt signature after constructing appropriate PKCS#1 signature
    ** block
    */
    rv = RSA_Sign(&key->u.rsa, sig, sigLen, maxLen, digder.data,
                  digder.len);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }

loser:
    SGN_DestroyDigestInfo(di);
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}

static SECStatus
sftk_RSASign(NSSLOWKEYPrivateKey *key, unsigned char *output,
             unsigned int *outputLen, unsigned int maxOutputLen,
             const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = RSA_Sign(&key->u.rsa, output, outputLen, maxOutputLen, input,
                  inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }
    return rv;
}

static SECStatus
sftk_RSASignRaw(NSSLOWKEYPrivateKey *key, unsigned char *output,
                unsigned int *outputLen, unsigned int maxOutputLen,
                const unsigned char *input, unsigned int inputLen)
{
    SECStatus rv = SECFailure;

    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    rv = RSA_SignRaw(&key->u.rsa, output, outputLen, maxOutputLen, input,
                     inputLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }
    return rv;
}

static SECStatus
sftk_RSASignPSS(SFTKHashSignInfo *info, unsigned char *sig,
                unsigned int *sigLen, unsigned int maxLen,
                const unsigned char *hash, unsigned int hashLen)
{
    SECStatus rv = SECFailure;
    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;
    CK_RSA_PKCS_PSS_PARAMS *params = (CK_RSA_PKCS_PSS_PARAMS *)info->params;

    PORT_Assert(info->key->keyType == NSSLOWKEYRSAKey);
    if (info->key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(params->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(params->mgf);

    rv = RSA_SignPSS(&info->key->u.rsa, hashAlg, maskHashAlg, NULL,
                     params->sLen, sig, sigLen, maxLen, hash, hashLen);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }
    return rv;
}

static SECStatus
nsc_DSA_Verify_Stub(void *ctx, void *sigBuf, unsigned int sigLen,
                    void *dataBuf, unsigned int dataLen)
{
    SECItem signature, digest;
    NSSLOWKEYPublicKey *key = (NSSLOWKEYPublicKey *)ctx;

    signature.data = (unsigned char *)sigBuf;
    signature.len = sigLen;
    digest.data = (unsigned char *)dataBuf;
    digest.len = dataLen;
    return DSA_VerifyDigest(&(key->u.dsa), &signature, &digest);
}

static SECStatus
nsc_DSA_Sign_Stub(void *ctx, void *sigBuf,
                  unsigned int *sigLen, unsigned int maxSigLen,
                  void *dataBuf, unsigned int dataLen)
{
    SECItem signature, digest;
    SECStatus rv;
    NSSLOWKEYPrivateKey *key = (NSSLOWKEYPrivateKey *)ctx;

    signature.data = (unsigned char *)sigBuf;
    signature.len = maxSigLen;
    digest.data = (unsigned char *)dataBuf;
    digest.len = dataLen;
    rv = DSA_SignDigest(&(key->u.dsa), &signature, &digest);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }
    *sigLen = signature.len;
    return rv;
}

static SECStatus
nsc_ECDSAVerifyStub(void *ctx, void *sigBuf, unsigned int sigLen,
                    void *dataBuf, unsigned int dataLen)
{
    SECItem signature, digest;
    NSSLOWKEYPublicKey *key = (NSSLOWKEYPublicKey *)ctx;

    signature.data = (unsigned char *)sigBuf;
    signature.len = sigLen;
    digest.data = (unsigned char *)dataBuf;
    digest.len = dataLen;
    return ECDSA_VerifyDigest(&(key->u.ec), &signature, &digest);
}

static SECStatus
nsc_ECDSASignStub(void *ctx, void *sigBuf,
                  unsigned int *sigLen, unsigned int maxSigLen,
                  void *dataBuf, unsigned int dataLen)
{
    SECItem signature, digest;
    SECStatus rv;
    NSSLOWKEYPrivateKey *key = (NSSLOWKEYPrivateKey *)ctx;

    signature.data = (unsigned char *)sigBuf;
    signature.len = maxSigLen;
    digest.data = (unsigned char *)dataBuf;
    digest.len = dataLen;
    rv = ECDSA_SignDigest(&(key->u.ec), &signature, &digest);
    if (rv != SECSuccess && PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
        sftk_fatalError = PR_TRUE;
    }
    *sigLen = signature.len;
    return rv;
}

/* NSC_SignInit setups up the signing operations. There are three basic
 * types of signing:
 *      (1) the tradition single part, where "Raw RSA" or "Raw DSA" is applied
 *  to data in a single Sign operation (which often looks a lot like an
 *  encrypt, with data coming in and data going out).
 *      (2) Hash based signing, where we continually hash the data, then apply
 *  some sort of signature to the end.
 *      (3) Block Encryption CBC MAC's, where the Data is encrypted with a key,
 *  and only the final block is part of the mac.
 *
 *  For case number 3, we initialize a context much like the Encryption Context
 *  (in fact we share code). We detect case 3 in C_SignUpdate, C_Sign, and
 *  C_Final by the following method... if it's not multi-part, and it's doesn't
 *  have a hash context, it must be a block Encryption CBC MAC.
 *
 *  For case number 2, we initialize a hash structure, as well as make it
 *  multi-part. Updates are simple calls to the hash update function. Final
 *  calls the hashend, then passes the result to the 'update' function (which
 *  operates as a final signature function). In some hash based MAC'ing (as
 *  opposed to hash base signatures), the update function is can be simply a
 *  copy (as is the case with HMAC).
 */
CK_RV
NSC_SignInit(CK_SESSION_HANDLE hSession,
             CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTKSession *session;
    SFTKObject *key;
    SFTKSessionContext *context;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    NSSLOWKEYPrivateKey *privKey;
    SFTKHashSignInfo *info = NULL;

    CHECK_FORK();

    /* Block Cipher MACing Algorithms use a different Context init method..*/
    crv = sftk_InitCBCMac(hSession, pMechanism, hKey, CKA_SIGN, SFTK_SIGN);
    if (crv != CKR_FUNCTION_NOT_SUPPORTED)
        return crv;

    /* we're not using a block cipher mac */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    crv = sftk_InitGeneric(session, &context, SFTK_SIGN, &key, hKey, &key_type,
                           CKO_PRIVATE_KEY, CKA_SIGN);
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        return crv;
    }

    context->multi = PR_FALSE;

#define INIT_RSA_SIGN_MECH(mmm)                         \
    case CKM_##mmm##_RSA_PKCS:                          \
        context->multi = PR_TRUE;                       \
        crv = sftk_doSub##mmm(context);                 \
        if (crv != CKR_OK)                              \
            break;                                      \
        context->update = (SFTKCipher)sftk_RSAHashSign; \
        info = PORT_New(SFTKHashSignInfo);              \
        if (info == NULL) {                             \
            crv = CKR_HOST_MEMORY;                      \
            break;                                      \
        }                                               \
        info->hashOid = SEC_OID_##mmm;                  \
        goto finish_rsa;

    switch (pMechanism->mechanism) {
        INIT_RSA_SIGN_MECH(MD5)
        INIT_RSA_SIGN_MECH(MD2)
        INIT_RSA_SIGN_MECH(SHA1)
        INIT_RSA_SIGN_MECH(SHA224)
        INIT_RSA_SIGN_MECH(SHA256)
        INIT_RSA_SIGN_MECH(SHA384)
        INIT_RSA_SIGN_MECH(SHA512)

        case CKM_RSA_PKCS:
            context->update = (SFTKCipher)sftk_RSASign;
            goto finish_rsa;
        case CKM_RSA_X_509:
            context->update = (SFTKCipher)sftk_RSASignRaw;
        finish_rsa:
            if (key_type != CKK_RSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            context->rsa = PR_TRUE;
            privKey = sftk_GetPrivKey(key, CKK_RSA, &crv);
            if (privKey == NULL) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            /* OK, info is allocated only if we're doing hash and sign mechanism.
             * It's necessary to be able to set the correct OID in the final
             * signature.
             */
            if (info) {
                info->key = privKey;
                context->cipherInfo = info;
                context->destroy = (SFTKDestroy)sftk_Space;
            } else {
                context->cipherInfo = privKey;
                context->destroy = (SFTKDestroy)sftk_Null;
            }
            context->maxLen = nsslowkey_PrivateModulusLen(privKey);
            break;
        case CKM_RSA_PKCS_PSS:
            if (key_type != CKK_RSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            context->rsa = PR_TRUE;
            if (pMechanism->ulParameterLen != sizeof(CK_RSA_PKCS_PSS_PARAMS) ||
                !sftk_ValidatePssParams((const CK_RSA_PKCS_PSS_PARAMS *)pMechanism->pParameter)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            info = PORT_New(SFTKHashSignInfo);
            if (info == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            info->params = pMechanism->pParameter;
            info->key = sftk_GetPrivKey(key, CKK_RSA, &crv);
            if (info->key == NULL) {
                PORT_Free(info);
                break;
            }
            context->cipherInfo = info;
            context->destroy = (SFTKDestroy)sftk_Space;
            context->update = (SFTKCipher)sftk_RSASignPSS;
            context->maxLen = nsslowkey_PrivateModulusLen(info->key);
            break;

        case CKM_DSA_SHA1:
            context->multi = PR_TRUE;
            crv = sftk_doSubSHA1(context);
            if (crv != CKR_OK)
                break;
        /* fall through */
        case CKM_DSA:
            if (key_type != CKK_DSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            privKey = sftk_GetPrivKey(key, CKK_DSA, &crv);
            if (privKey == NULL) {
                break;
            }
            context->cipherInfo = privKey;
            context->update = (SFTKCipher)nsc_DSA_Sign_Stub;
            context->destroy = (privKey == key->objectInfo) ? (SFTKDestroy)sftk_Null : (SFTKDestroy)sftk_FreePrivKey;
            context->maxLen = DSA_MAX_SIGNATURE_LEN;

            break;

        case CKM_ECDSA_SHA1:
            context->multi = PR_TRUE;
            crv = sftk_doSubSHA1(context);
            if (crv != CKR_OK)
                break;
        /* fall through */
        case CKM_ECDSA:
            if (key_type != CKK_EC) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            privKey = sftk_GetPrivKey(key, CKK_EC, &crv);
            if (privKey == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->cipherInfo = privKey;
            context->update = (SFTKCipher)nsc_ECDSASignStub;
            context->destroy = (privKey == key->objectInfo) ? (SFTKDestroy)sftk_Null : (SFTKDestroy)sftk_FreePrivKey;
            context->maxLen = MAX_ECKEY_LEN * 2;

            break;

#define INIT_HMAC_MECH(mmm)                                               \
    case CKM_##mmm##_HMAC_GENERAL:                                        \
        PORT_Assert(pMechanism->pParameter);                              \
        if (!pMechanism->pParameter) {                                    \
            crv = CKR_MECHANISM_PARAM_INVALID;                            \
            break;                                                        \
        }                                                                 \
        crv = sftk_doHMACInit(context, HASH_Alg##mmm, key,                \
                              *(CK_ULONG *)pMechanism->pParameter);       \
        break;                                                            \
    case CKM_##mmm##_HMAC:                                                \
        crv = sftk_doHMACInit(context, HASH_Alg##mmm, key, mmm##_LENGTH); \
        break;

            INIT_HMAC_MECH(MD2)
            INIT_HMAC_MECH(MD5)
            INIT_HMAC_MECH(SHA224)
            INIT_HMAC_MECH(SHA256)
            INIT_HMAC_MECH(SHA384)
            INIT_HMAC_MECH(SHA512)

        case CKM_SHA_1_HMAC_GENERAL:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter || pMechanism->ulParameterLen != sizeof(CK_MAC_GENERAL_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doHMACInit(context, HASH_AlgSHA1, key,
                                  *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_SHA_1_HMAC:
            crv = sftk_doHMACInit(context, HASH_AlgSHA1, key, SHA1_LENGTH);
            break;
        case CKM_AES_CMAC_GENERAL:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter || pMechanism->ulParameterLen != sizeof(CK_MAC_GENERAL_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doCMACInit(context, CMAC_AES, key, *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_AES_CMAC:
            crv = sftk_doCMACInit(context, CMAC_AES, key, AES_BLOCK_SIZE);
            break;
        case CKM_SSL3_MD5_MAC:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doSSLMACInit(context, SEC_OID_MD5, key,
                                    *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_SSL3_SHA1_MAC:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doSSLMACInit(context, SEC_OID_SHA1, key,
                                    *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_TLS_PRF_GENERAL:
            crv = sftk_TLSPRFInit(context, key, key_type, HASH_AlgNULL, 0);
            break;
        case CKM_TLS_MAC: {
            CK_TLS_MAC_PARAMS *tls12_mac_params;
            HASH_HashType tlsPrfHash;
            const char *label;

            if (pMechanism->ulParameterLen != sizeof(CK_TLS_MAC_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            tls12_mac_params = (CK_TLS_MAC_PARAMS *)pMechanism->pParameter;
            if (tls12_mac_params->prfMechanism == CKM_TLS_PRF) {
                /* The TLS 1.0 and 1.1 PRF */
                tlsPrfHash = HASH_AlgNULL;
                if (tls12_mac_params->ulMacLength != 12) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
            } else {
                /* The hash function for the TLS 1.2 PRF */
                tlsPrfHash =
                    GetHashTypeFromMechanism(tls12_mac_params->prfMechanism);
                if (tlsPrfHash == HASH_AlgNULL ||
                    tls12_mac_params->ulMacLength < 12) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
            }
            if (tls12_mac_params->ulServerOrClient == 1) {
                label = "server finished";
            } else if (tls12_mac_params->ulServerOrClient == 2) {
                label = "client finished";
            } else {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_TLSPRFInit(context, key, key_type, tlsPrfHash,
                                  tls12_mac_params->ulMacLength);
            if (crv == CKR_OK) {
                context->hashUpdate(context->hashInfo, label, 15);
            }
            break;
        }
        case CKM_NSS_TLS_PRF_GENERAL_SHA256:
            crv = sftk_TLSPRFInit(context, key, key_type, HASH_AlgSHA256, 0);
            break;

        case CKM_NSS_HMAC_CONSTANT_TIME: {
            sftk_MACConstantTimeCtx *ctx =
                sftk_HMACConstantTime_New(pMechanism, key);
            CK_ULONG *intpointer;

            if (ctx == NULL) {
                crv = CKR_ARGUMENTS_BAD;
                break;
            }
            intpointer = PORT_New(CK_ULONG);
            if (intpointer == NULL) {
                PORT_Free(ctx);
                crv = CKR_HOST_MEMORY;
                break;
            }
            *intpointer = ctx->hash->length;

            context->cipherInfo = intpointer;
            context->hashInfo = ctx;
            context->currentMech = pMechanism->mechanism;
            context->hashUpdate = sftk_HMACConstantTime_Update;
            context->hashdestroy = sftk_MACConstantTime_DestroyContext;
            context->end = sftk_MACConstantTime_EndHash;
            context->update = (SFTKCipher)sftk_SignCopy;
            context->destroy = sftk_Space;
            context->maxLen = 64;
            context->multi = PR_TRUE;
            break;
        }

        case CKM_NSS_SSL3_MAC_CONSTANT_TIME: {
            sftk_MACConstantTimeCtx *ctx =
                sftk_SSLv3MACConstantTime_New(pMechanism, key);
            CK_ULONG *intpointer;

            if (ctx == NULL) {
                crv = CKR_ARGUMENTS_BAD;
                break;
            }
            intpointer = PORT_New(CK_ULONG);
            if (intpointer == NULL) {
                PORT_Free(ctx);
                crv = CKR_HOST_MEMORY;
                break;
            }
            *intpointer = ctx->hash->length;

            context->cipherInfo = intpointer;
            context->hashInfo = ctx;
            context->currentMech = pMechanism->mechanism;
            context->hashUpdate = sftk_SSLv3MACConstantTime_Update;
            context->hashdestroy = sftk_MACConstantTime_DestroyContext;
            context->end = sftk_MACConstantTime_EndHash;
            context->update = (SFTKCipher)sftk_SignCopy;
            context->destroy = sftk_Space;
            context->maxLen = 64;
            context->multi = PR_TRUE;
            break;
        }

        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    if (crv != CKR_OK) {
        if (info)
            PORT_Free(info);
        sftk_FreeContext(context);
        sftk_FreeSession(session);
        return crv;
    }
    sftk_SetContextByType(session, SFTK_SIGN, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/** MAC one block of data by block cipher
 */
static CK_RV
sftk_MACBlock(SFTKSessionContext *ctx, void *blk)
{
    unsigned int outlen;
    return (SECSuccess == (ctx->update)(ctx->cipherInfo, ctx->macBuf, &outlen,
                                        SFTK_MAX_BLOCK_SIZE, blk, ctx->blockSize))
               ? CKR_OK
               : sftk_MapCryptError(PORT_GetError());
}

/** MAC last (incomplete) block of data by block cipher
 *
 *  Call once, then terminate MACing operation.
 */
static CK_RV
sftk_MACFinal(SFTKSessionContext *ctx)
{
    unsigned int padLen = ctx->padDataLength;
    /* pad and proceed the residual */
    if (ctx->isXCBC) {
        CK_RV crv = sftk_xcbc_mac_pad(ctx->padBuf, padLen, ctx->blockSize,
                                      ctx->k2, ctx->k3);
        if (crv != CKR_OK)
            return crv;
        return sftk_MACBlock(ctx, ctx->padBuf);
    }
    if (padLen) {
        /* shd clr ctx->padLen to make sftk_MACFinal idempotent */
        PORT_Memset(ctx->padBuf + padLen, 0, ctx->blockSize - padLen);
        return sftk_MACBlock(ctx, ctx->padBuf);
    } else
        return CKR_OK;
}

/** The common implementation for {Sign,Verify}Update. (S/V only vary in their
 * setup and final operations).
 *
 * A call which results in an error terminates the operation [PKCS#11,v2.11]
 */
static CK_RV
sftk_MACUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
               CK_ULONG ulPartLen, SFTKContextType type)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    CK_RV crv;

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, type, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    if (context->hashInfo) {
        (*context->hashUpdate)(context->hashInfo, pPart, ulPartLen);
    } else {
        /* must be block cipher MACing */

        unsigned int blkSize = context->blockSize;
        unsigned char *residual = /* free room in context->padBuf */
            context->padBuf + context->padDataLength;
        unsigned int minInput = /* min input for MACing at least one block */
            blkSize - context->padDataLength;

        /* not enough data even for one block */
        if (ulPartLen <= minInput) {
            PORT_Memcpy(residual, pPart, ulPartLen);
            context->padDataLength += ulPartLen;
            goto cleanup;
        }
        /* MACing residual */
        if (context->padDataLength) {
            PORT_Memcpy(residual, pPart, minInput);
            ulPartLen -= minInput;
            pPart += minInput;
            if (CKR_OK != (crv = sftk_MACBlock(context, context->padBuf)))
                goto terminate;
        }
        /* MACing full blocks */
        while (ulPartLen > blkSize) {
            if (CKR_OK != (crv = sftk_MACBlock(context, pPart)))
                goto terminate;
            ulPartLen -= blkSize;
            pPart += blkSize;
        }
        /* save the residual */
        if ((context->padDataLength = ulPartLen))
            PORT_Memcpy(context->padBuf, pPart, ulPartLen);
    } /* blk cipher MACing */

    goto cleanup;

terminate:
    sftk_TerminateOp(session, type, context);
cleanup:
    sftk_FreeSession(session);
    return crv;
}

/* NSC_SignUpdate continues a multiple-part signature operation,
 * where the signature is (will be) an appendix to the data,
 * and plaintext cannot be recovered from the signature
 *
 * A call which results in an error terminates the operation [PKCS#11,v2.11]
 */
CK_RV
NSC_SignUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
               CK_ULONG ulPartLen)
{
    CHECK_FORK();
    return sftk_MACUpdate(hSession, pPart, ulPartLen, SFTK_SIGN);
}

/* NSC_SignFinal finishes a multiple-part signature operation,
 * returning the signature. */
CK_RV
NSC_SignFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature,
              CK_ULONG_PTR pulSignatureLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulSignatureLen;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_SIGN, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    if (context->hashInfo) {
        unsigned int digestLen;
        unsigned char tmpbuf[SFTK_MAX_MAC_LENGTH];

        if (!pSignature) {
            outlen = context->maxLen;
            goto finish;
        }
        (*context->end)(context->hashInfo, tmpbuf, &digestLen, sizeof(tmpbuf));
        if (SECSuccess != (context->update)(context->cipherInfo, pSignature,
                                            &outlen, maxoutlen, tmpbuf, digestLen))
            crv = sftk_MapCryptError(PORT_GetError());
        /* CKR_BUFFER_TOO_SMALL here isn't continuable, let operation terminate.
         * Keeping "too small" CK_RV intact is a standard violation, but allows
         * application read EXACT signature length */
    } else {
        /* must be block cipher MACing */
        outlen = context->macSize;
        /* null or "too small" buf doesn't terminate operation [PKCS#11,v2.11]*/
        if (!pSignature || maxoutlen < outlen) {
            if (pSignature)
                crv = CKR_BUFFER_TOO_SMALL;
            goto finish;
        }
        if (CKR_OK == (crv = sftk_MACFinal(context)))
            PORT_Memcpy(pSignature, context->macBuf, outlen);
    }

    sftk_TerminateOp(session, SFTK_SIGN, context);
finish:
    *pulSignatureLen = outlen;
    sftk_FreeSession(session);
    return crv;
}

/* NSC_Sign signs (encrypts with private key) data in a single part,
 * where the signature is (will be) an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
NSC_Sign(CK_SESSION_HANDLE hSession,
         CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
         CK_ULONG_PTR pulSignatureLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_SIGN, PR_FALSE, &session);
    if (crv != CKR_OK)
        return crv;

    if (!pSignature) {
        /* see also how C_SignUpdate implements this */
        *pulSignatureLen = (!context->multi || context->hashInfo)
                               ? context->maxLen
                               : context->macSize; /* must be block cipher MACing */
        goto finish;
    }

    /* multi part Signing are completely implemented by SignUpdate and
     * sign Final */
    if (context->multi) {
        /* SignFinal can't follow failed SignUpdate */
        if (CKR_OK == (crv = NSC_SignUpdate(hSession, pData, ulDataLen)))
            crv = NSC_SignFinal(hSession, pSignature, pulSignatureLen);
    } else {
        /* single-part PKC signature (e.g. CKM_ECDSA) */
        unsigned int outlen;
        unsigned int maxoutlen = *pulSignatureLen;
        if (SECSuccess != (*context->update)(context->cipherInfo, pSignature,
                                             &outlen, maxoutlen, pData, ulDataLen))
            crv = sftk_MapCryptError(PORT_GetError());
        *pulSignatureLen = (CK_ULONG)outlen;
        /*  "too small" here is certainly continuable */
        if (crv != CKR_BUFFER_TOO_SMALL)
            sftk_TerminateOp(session, SFTK_SIGN, context);
    } /* single-part */

finish:
    sftk_FreeSession(session);
    return crv;
}

/*
 ************** Crypto Functions:     Sign Recover  ************************
 */
/* NSC_SignRecoverInit initializes a signature operation,
 * where the (digest) data can be recovered from the signature.
 * E.g. encryption with the user's private key */
CK_RV
NSC_SignRecoverInit(CK_SESSION_HANDLE hSession,
                    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    CHECK_FORK();

    switch (pMechanism->mechanism) {
        case CKM_RSA_PKCS:
        case CKM_RSA_X_509:
            return NSC_SignInit(hSession, pMechanism, hKey);
        default:
            break;
    }
    return CKR_MECHANISM_INVALID;
}

/* NSC_SignRecover signs data in a single operation
 * where the (digest) data can be recovered from the signature.
 * E.g. encryption with the user's private key */
CK_RV
NSC_SignRecover(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
    CHECK_FORK();

    return NSC_Sign(hSession, pData, ulDataLen, pSignature, pulSignatureLen);
}

/*
 ************** Crypto Functions:     verify  ************************
 */

/* Handle RSA Signature formatting */
static SECStatus
sftk_hashCheckSign(SFTKHashVerifyInfo *info, const unsigned char *sig,
                   unsigned int sigLen, const unsigned char *digest,
                   unsigned int digestLen)
{
    PORT_Assert(info->key->keyType == NSSLOWKEYRSAKey);
    if (info->key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    return RSA_HashCheckSign(info->hashOid, info->key, sig, sigLen, digest,
                             digestLen);
}

SECStatus
RSA_HashCheckSign(SECOidTag digestOid, NSSLOWKEYPublicKey *key,
                  const unsigned char *sig, unsigned int sigLen,
                  const unsigned char *digestData, unsigned int digestLen)
{
    unsigned char *pkcs1DigestInfoData;
    SECItem pkcs1DigestInfo;
    SECItem digest;
    unsigned int bufferSize;
    SECStatus rv;

    /* pkcs1DigestInfo.data must be less than key->u.rsa.modulus.len */
    bufferSize = key->u.rsa.modulus.len;
    pkcs1DigestInfoData = PORT_ZAlloc(bufferSize);
    if (!pkcs1DigestInfoData) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    pkcs1DigestInfo.data = pkcs1DigestInfoData;
    pkcs1DigestInfo.len = bufferSize;

    /* decrypt the block */
    rv = RSA_CheckSignRecover(&key->u.rsa, pkcs1DigestInfo.data,
                              &pkcs1DigestInfo.len, pkcs1DigestInfo.len,
                              sig, sigLen);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
    } else {
        digest.data = (PRUint8 *)digestData;
        digest.len = digestLen;
        rv = _SGN_VerifyPKCS1DigestInfo(
            digestOid, &digest, &pkcs1DigestInfo,
            PR_FALSE /*XXX: unsafeAllowMissingParameters*/);
    }

    PORT_Free(pkcs1DigestInfoData);
    return rv;
}

static SECStatus
sftk_RSACheckSign(NSSLOWKEYPublicKey *key, const unsigned char *sig,
                  unsigned int sigLen, const unsigned char *digest,
                  unsigned int digestLen)
{
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    return RSA_CheckSign(&key->u.rsa, sig, sigLen, digest, digestLen);
}

static SECStatus
sftk_RSACheckSignRaw(NSSLOWKEYPublicKey *key, const unsigned char *sig,
                     unsigned int sigLen, const unsigned char *digest,
                     unsigned int digestLen)
{
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    return RSA_CheckSignRaw(&key->u.rsa, sig, sigLen, digest, digestLen);
}

static SECStatus
sftk_RSACheckSignPSS(SFTKHashVerifyInfo *info, const unsigned char *sig,
                     unsigned int sigLen, const unsigned char *digest,
                     unsigned int digestLen)
{
    HASH_HashType hashAlg;
    HASH_HashType maskHashAlg;
    CK_RSA_PKCS_PSS_PARAMS *params = (CK_RSA_PKCS_PSS_PARAMS *)info->params;

    PORT_Assert(info->key->keyType == NSSLOWKEYRSAKey);
    if (info->key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    hashAlg = GetHashTypeFromMechanism(params->hashAlg);
    maskHashAlg = GetHashTypeFromMechanism(params->mgf);

    return RSA_CheckSignPSS(&info->key->u.rsa, hashAlg, maskHashAlg,
                            params->sLen, sig, sigLen, digest, digestLen);
}

/* NSC_VerifyInit initializes a verification operation,
 * where the signature is an appendix to the data,
 * and plaintext cannot be recovered from the signature (e.g. DSA) */
CK_RV
NSC_VerifyInit(CK_SESSION_HANDLE hSession,
               CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTKSession *session;
    SFTKObject *key;
    SFTKSessionContext *context;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    NSSLOWKEYPublicKey *pubKey;
    SFTKHashVerifyInfo *info = NULL;

    CHECK_FORK();

    /* Block Cipher MACing Algorithms use a different Context init method..*/
    crv = sftk_InitCBCMac(hSession, pMechanism, hKey, CKA_VERIFY, SFTK_VERIFY);
    if (crv != CKR_FUNCTION_NOT_SUPPORTED)
        return crv;

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    crv = sftk_InitGeneric(session, &context, SFTK_VERIFY, &key, hKey, &key_type,
                           CKO_PUBLIC_KEY, CKA_VERIFY);
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        return crv;
    }

    context->multi = PR_FALSE;

#define INIT_RSA_VFY_MECH(mmm)                            \
    case CKM_##mmm##_RSA_PKCS:                            \
        context->multi = PR_TRUE;                         \
        crv = sftk_doSub##mmm(context);                   \
        if (crv != CKR_OK)                                \
            break;                                        \
        context->verify = (SFTKVerify)sftk_hashCheckSign; \
        info = PORT_New(SFTKHashVerifyInfo);              \
        if (info == NULL) {                               \
            crv = CKR_HOST_MEMORY;                        \
            break;                                        \
        }                                                 \
        info->hashOid = SEC_OID_##mmm;                    \
        goto finish_rsa;

    switch (pMechanism->mechanism) {
        INIT_RSA_VFY_MECH(MD5)
        INIT_RSA_VFY_MECH(MD2)
        INIT_RSA_VFY_MECH(SHA1)
        INIT_RSA_VFY_MECH(SHA224)
        INIT_RSA_VFY_MECH(SHA256)
        INIT_RSA_VFY_MECH(SHA384)
        INIT_RSA_VFY_MECH(SHA512)

        case CKM_RSA_PKCS:
            context->verify = (SFTKVerify)sftk_RSACheckSign;
            goto finish_rsa;
        case CKM_RSA_X_509:
            context->verify = (SFTKVerify)sftk_RSACheckSignRaw;
        finish_rsa:
            if (key_type != CKK_RSA) {
                if (info)
                    PORT_Free(info);
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            context->rsa = PR_TRUE;
            pubKey = sftk_GetPubKey(key, CKK_RSA, &crv);
            if (pubKey == NULL) {
                if (info)
                    PORT_Free(info);
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            if (info) {
                info->key = pubKey;
                context->cipherInfo = info;
                context->destroy = sftk_Space;
            } else {
                context->cipherInfo = pubKey;
                context->destroy = sftk_Null;
            }
            break;
        case CKM_RSA_PKCS_PSS:
            if (key_type != CKK_RSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            context->rsa = PR_TRUE;
            if (pMechanism->ulParameterLen != sizeof(CK_RSA_PKCS_PSS_PARAMS) ||
                !sftk_ValidatePssParams((const CK_RSA_PKCS_PSS_PARAMS *)pMechanism->pParameter)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            info = PORT_New(SFTKHashVerifyInfo);
            if (info == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            info->params = pMechanism->pParameter;
            info->key = sftk_GetPubKey(key, CKK_RSA, &crv);
            if (info->key == NULL) {
                PORT_Free(info);
                break;
            }
            context->cipherInfo = info;
            context->destroy = (SFTKDestroy)sftk_Space;
            context->verify = (SFTKVerify)sftk_RSACheckSignPSS;
            break;
        case CKM_DSA_SHA1:
            context->multi = PR_TRUE;
            crv = sftk_doSubSHA1(context);
            if (crv != CKR_OK)
                break;
        /* fall through */
        case CKM_DSA:
            if (key_type != CKK_DSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            pubKey = sftk_GetPubKey(key, CKK_DSA, &crv);
            if (pubKey == NULL) {
                break;
            }
            context->cipherInfo = pubKey;
            context->verify = (SFTKVerify)nsc_DSA_Verify_Stub;
            context->destroy = sftk_Null;
            break;
        case CKM_ECDSA_SHA1:
            context->multi = PR_TRUE;
            crv = sftk_doSubSHA1(context);
            if (crv != CKR_OK)
                break;
        /* fall through */
        case CKM_ECDSA:
            if (key_type != CKK_EC) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            pubKey = sftk_GetPubKey(key, CKK_EC, &crv);
            if (pubKey == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            context->cipherInfo = pubKey;
            context->verify = (SFTKVerify)nsc_ECDSAVerifyStub;
            context->destroy = sftk_Null;
            break;

            INIT_HMAC_MECH(MD2)
            INIT_HMAC_MECH(MD5)
            INIT_HMAC_MECH(SHA224)
            INIT_HMAC_MECH(SHA256)
            INIT_HMAC_MECH(SHA384)
            INIT_HMAC_MECH(SHA512)

        case CKM_SHA_1_HMAC_GENERAL:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doHMACInit(context, HASH_AlgSHA1, key,
                                  *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_SHA_1_HMAC:
            crv = sftk_doHMACInit(context, HASH_AlgSHA1, key, SHA1_LENGTH);
            break;

        case CKM_SSL3_MD5_MAC:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doSSLMACInit(context, SEC_OID_MD5, key,
                                    *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_SSL3_SHA1_MAC:
            PORT_Assert(pMechanism->pParameter);
            if (!pMechanism->pParameter) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_doSSLMACInit(context, SEC_OID_SHA1, key,
                                    *(CK_ULONG *)pMechanism->pParameter);
            break;
        case CKM_TLS_PRF_GENERAL:
            crv = sftk_TLSPRFInit(context, key, key_type, HASH_AlgNULL, 0);
            break;
        case CKM_NSS_TLS_PRF_GENERAL_SHA256:
            crv = sftk_TLSPRFInit(context, key, key_type, HASH_AlgSHA256, 0);
            break;

        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    if (crv != CKR_OK) {
        if (info)
            PORT_Free(info);
        sftk_FreeContext(context);
        sftk_FreeSession(session);
        return crv;
    }
    sftk_SetContextByType(session, SFTK_VERIFY, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_Verify verifies a signature in a single-part operation,
 * where the signature is an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
NSC_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
           CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    CK_RV crv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_VERIFY, PR_FALSE, &session);
    if (crv != CKR_OK)
        return crv;

    /* multi part Verifying are completely implemented by VerifyUpdate and
     * VerifyFinal */
    if (context->multi) {
        /* VerifyFinal can't follow failed VerifyUpdate */
        if (CKR_OK == (crv = NSC_VerifyUpdate(hSession, pData, ulDataLen)))
            crv = NSC_VerifyFinal(hSession, pSignature, ulSignatureLen);
    } else {
        if (SECSuccess != (*context->verify)(context->cipherInfo, pSignature,
                                             ulSignatureLen, pData, ulDataLen))
            crv = sftk_MapCryptError(PORT_GetError());

        sftk_TerminateOp(session, SFTK_VERIFY, context);
    }
    sftk_FreeSession(session);
    return crv;
}

/* NSC_VerifyUpdate continues a multiple-part verification operation,
 * where the signature is an appendix to the data,
 * and plaintext cannot be recovered from the signature
 *
 * A call which results in an error terminates the operation [PKCS#11,v2.11]
 */
CK_RV
NSC_VerifyUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                 CK_ULONG ulPartLen)
{
    CHECK_FORK();
    return sftk_MACUpdate(hSession, pPart, ulPartLen, SFTK_VERIFY);
}

/* NSC_VerifyFinal finishes a multiple-part verification operation,
 * checking the signature. */
CK_RV
NSC_VerifyFinal(CK_SESSION_HANDLE hSession,
                CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    CK_RV crv;

    CHECK_FORK();

    if (!pSignature)
        return CKR_ARGUMENTS_BAD;

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_VERIFY, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    if (context->hashInfo) {
        unsigned int digestLen;
        unsigned char tmpbuf[SFTK_MAX_MAC_LENGTH];

        (*context->end)(context->hashInfo, tmpbuf, &digestLen, sizeof(tmpbuf));
        if (SECSuccess != (context->verify)(context->cipherInfo, pSignature,
                                            ulSignatureLen, tmpbuf, digestLen))
            crv = sftk_MapCryptError(PORT_GetError());
    } else if (ulSignatureLen != context->macSize) {
        /* must be block cipher MACing */
        crv = CKR_SIGNATURE_LEN_RANGE;
    } else if (CKR_OK == (crv = sftk_MACFinal(context))) {
        if (NSS_SecureMemcmp(pSignature, context->macBuf, ulSignatureLen))
            crv = CKR_SIGNATURE_INVALID;
    }

    sftk_TerminateOp(session, SFTK_VERIFY, context);
    sftk_FreeSession(session);
    return crv;
}

/*
 ************** Crypto Functions:     Verify  Recover ************************
 */
static SECStatus
sftk_RSACheckSignRecover(NSSLOWKEYPublicKey *key, unsigned char *data,
                         unsigned int *dataLen, unsigned int maxDataLen,
                         const unsigned char *sig, unsigned int sigLen)
{
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    return RSA_CheckSignRecover(&key->u.rsa, data, dataLen, maxDataLen,
                                sig, sigLen);
}

static SECStatus
sftk_RSACheckSignRecoverRaw(NSSLOWKEYPublicKey *key, unsigned char *data,
                            unsigned int *dataLen, unsigned int maxDataLen,
                            const unsigned char *sig, unsigned int sigLen)
{
    PORT_Assert(key->keyType == NSSLOWKEYRSAKey);
    if (key->keyType != NSSLOWKEYRSAKey) {
        PORT_SetError(SEC_ERROR_INVALID_KEY);
        return SECFailure;
    }

    return RSA_CheckSignRecoverRaw(&key->u.rsa, data, dataLen, maxDataLen,
                                   sig, sigLen);
}

/* NSC_VerifyRecoverInit initializes a signature verification operation,
 * where the data is recovered from the signature.
 * E.g. Decryption with the user's public key */
CK_RV
NSC_VerifyRecoverInit(CK_SESSION_HANDLE hSession,
                      CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTKSession *session;
    SFTKObject *key;
    SFTKSessionContext *context;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    NSSLOWKEYPublicKey *pubKey;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    crv = sftk_InitGeneric(session, &context, SFTK_VERIFY_RECOVER,
                           &key, hKey, &key_type, CKO_PUBLIC_KEY, CKA_VERIFY_RECOVER);
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        return crv;
    }

    context->multi = PR_TRUE;

    switch (pMechanism->mechanism) {
        case CKM_RSA_PKCS:
        case CKM_RSA_X_509:
            if (key_type != CKK_RSA) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            context->multi = PR_FALSE;
            context->rsa = PR_TRUE;
            pubKey = sftk_GetPubKey(key, CKK_RSA, &crv);
            if (pubKey == NULL) {
                break;
            }
            context->cipherInfo = pubKey;
            context->update = (SFTKCipher)(pMechanism->mechanism == CKM_RSA_X_509
                                               ? sftk_RSACheckSignRecoverRaw
                                               : sftk_RSACheckSignRecover);
            context->destroy = sftk_Null;
            break;
        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    if (crv != CKR_OK) {
        PORT_Free(context);
        sftk_FreeSession(session);
        return crv;
    }
    sftk_SetContextByType(session, SFTK_VERIFY_RECOVER, context);
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_VerifyRecover verifies a signature in a single-part operation,
 * where the data is recovered from the signature.
 * E.g. Decryption with the user's public key */
CK_RV
NSC_VerifyRecover(CK_SESSION_HANDLE hSession,
                  CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen,
                  CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{
    SFTKSession *session;
    SFTKSessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulDataLen;
    CK_RV crv;
    SECStatus rv;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_VERIFY_RECOVER,
                          PR_FALSE, &session);
    if (crv != CKR_OK)
        return crv;
    if (pData == NULL) {
        /* to return the actual size, we need  to do the decrypt, just return
         * the max size, which is the size of the input signature. */
        *pulDataLen = ulSignatureLen;
        rv = SECSuccess;
        goto finish;
    }

    rv = (*context->update)(context->cipherInfo, pData, &outlen, maxoutlen,
                            pSignature, ulSignatureLen);
    *pulDataLen = (CK_ULONG)outlen;

    sftk_TerminateOp(session, SFTK_VERIFY_RECOVER, context);
finish:
    sftk_FreeSession(session);
    return (rv == SECSuccess) ? CKR_OK : sftk_MapVerifyError(PORT_GetError());
}

/*
 **************************** Random Functions:  ************************
 */

/* NSC_SeedRandom mixes additional seed material into the token's random number
 * generator. */
CK_RV
NSC_SeedRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSeed,
               CK_ULONG ulSeedLen)
{
    SECStatus rv;

    CHECK_FORK();

    rv = RNG_RandomUpdate(pSeed, ulSeedLen);
    return (rv == SECSuccess) ? CKR_OK : sftk_MapCryptError(PORT_GetError());
}

/* NSC_GenerateRandom generates random data. */
CK_RV
NSC_GenerateRandom(CK_SESSION_HANDLE hSession,
                   CK_BYTE_PTR pRandomData, CK_ULONG ulRandomLen)
{
    SECStatus rv;

    CHECK_FORK();

    rv = RNG_GenerateGlobalRandomBytes(pRandomData, ulRandomLen);
    /*
     * This may fail with SEC_ERROR_NEED_RANDOM, which means the RNG isn't
     * seeded with enough entropy.
     */
    return (rv == SECSuccess) ? CKR_OK : sftk_MapCryptError(PORT_GetError());
}

/*
 **************************** Key Functions:  ************************
 */

/*
 * generate a password based encryption key. This code uses
 * PKCS5 to do the work.
 */
static CK_RV
nsc_pbe_key_gen(NSSPKCS5PBEParameter *pkcs5_pbe, CK_MECHANISM_PTR pMechanism,
                void *buf, CK_ULONG *key_length, PRBool faulty3DES)
{
    SECItem *pbe_key = NULL, iv, pwitem;
    CK_PBE_PARAMS *pbe_params = NULL;
    CK_PKCS5_PBKD2_PARAMS *pbkd2_params = NULL;

    *key_length = 0;
    iv.data = NULL;
    iv.len = 0;

    if (pMechanism->mechanism == CKM_PKCS5_PBKD2) {
        if (BAD_PARAM_CAST(pMechanism, sizeof(CK_PKCS5_PBKD2_PARAMS))) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
        pbkd2_params = (CK_PKCS5_PBKD2_PARAMS *)pMechanism->pParameter;
        pwitem.data = (unsigned char *)pbkd2_params->pPassword;
        /* was this a typo in the PKCS #11 spec? */
        pwitem.len = *pbkd2_params->ulPasswordLen;
    } else {
        if (BAD_PARAM_CAST(pMechanism, sizeof(CK_PBE_PARAMS))) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
        pbe_params = (CK_PBE_PARAMS *)pMechanism->pParameter;
        pwitem.data = (unsigned char *)pbe_params->pPassword;
        pwitem.len = pbe_params->ulPasswordLen;
    }
    pbe_key = nsspkcs5_ComputeKeyAndIV(pkcs5_pbe, &pwitem, &iv, faulty3DES);
    if (pbe_key == NULL) {
        return CKR_HOST_MEMORY;
    }

    PORT_Memcpy(buf, pbe_key->data, pbe_key->len);
    *key_length = pbe_key->len;
    SECITEM_ZfreeItem(pbe_key, PR_TRUE);
    pbe_key = NULL;

    if (iv.data) {
        if (pbe_params && pbe_params->pInitVector != NULL) {
            PORT_Memcpy(pbe_params->pInitVector, iv.data, iv.len);
        }
        PORT_Free(iv.data);
    }

    return CKR_OK;
}

/*
 * this is coded for "full" support. These selections will be limitted to
 * the official subset by freebl.
 */
static unsigned int
sftk_GetSubPrimeFromPrime(unsigned int primeBits)
{
    if (primeBits <= 1024) {
        return 160;
    } else if (primeBits <= 2048) {
        return 224;
    } else if (primeBits <= 3072) {
        return 256;
    } else if (primeBits <= 7680) {
        return 384;
    } else {
        return 512;
    }
}

static CK_RV
nsc_parameter_gen(CK_KEY_TYPE key_type, SFTKObject *key)
{
    SFTKAttribute *attribute;
    CK_ULONG counter;
    unsigned int seedBits = 0;
    unsigned int subprimeBits = 0;
    unsigned int primeBits;
    unsigned int j = 8; /* default to 1024 bits */
    CK_RV crv = CKR_OK;
    PQGParams *params = NULL;
    PQGVerify *vfy = NULL;
    SECStatus rv;

    attribute = sftk_FindAttribute(key, CKA_PRIME_BITS);
    if (attribute == NULL) {
        attribute = sftk_FindAttribute(key, CKA_PRIME);
        if (attribute == NULL) {
            return CKR_TEMPLATE_INCOMPLETE;
        } else {
            primeBits = attribute->attrib.ulValueLen;
            sftk_FreeAttribute(attribute);
        }
    } else {
        primeBits = (unsigned int)*(CK_ULONG *)attribute->attrib.pValue;
        sftk_FreeAttribute(attribute);
    }
    if (primeBits < 1024) {
        j = PQG_PBITS_TO_INDEX(primeBits);
        if (j == (unsigned int)-1) {
            return CKR_ATTRIBUTE_VALUE_INVALID;
        }
    }

    attribute = sftk_FindAttribute(key, CKA_NSS_PQG_SEED_BITS);
    if (attribute != NULL) {
        seedBits = (unsigned int)*(CK_ULONG *)attribute->attrib.pValue;
        sftk_FreeAttribute(attribute);
    }

    attribute = sftk_FindAttribute(key, CKA_SUBPRIME_BITS);
    if (attribute != NULL) {
        subprimeBits = (unsigned int)*(CK_ULONG *)attribute->attrib.pValue;
        sftk_FreeAttribute(attribute);
    }

    /* if P and Q are supplied, we want to generate a new G */
    attribute = sftk_FindAttribute(key, CKA_PRIME);
    if (attribute != NULL) {
        PLArenaPool *arena;

        sftk_FreeAttribute(attribute);
        arena = PORT_NewArena(1024);
        if (arena == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        params = PORT_ArenaAlloc(arena, sizeof(*params));
        if (params == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        params->arena = arena;
        crv = sftk_Attribute2SSecItem(arena, &params->prime, key, CKA_PRIME);
        if (crv != CKR_OK) {
            goto loser;
        }
        crv = sftk_Attribute2SSecItem(arena, &params->subPrime,
                                      key, CKA_SUBPRIME);
        if (crv != CKR_OK) {
            goto loser;
        }

        arena = PORT_NewArena(1024);
        if (arena == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        vfy = PORT_ArenaAlloc(arena, sizeof(*vfy));
        if (vfy == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        vfy->arena = arena;
        crv = sftk_Attribute2SSecItem(arena, &vfy->seed, key, CKA_NSS_PQG_SEED);
        if (crv != CKR_OK) {
            goto loser;
        }
        crv = sftk_Attribute2SSecItem(arena, &vfy->h, key, CKA_NSS_PQG_H);
        if (crv != CKR_OK) {
            goto loser;
        }
        sftk_DeleteAttributeType(key, CKA_PRIME);
        sftk_DeleteAttributeType(key, CKA_SUBPRIME);
        sftk_DeleteAttributeType(key, CKA_NSS_PQG_SEED);
        sftk_DeleteAttributeType(key, CKA_NSS_PQG_H);
    }

    sftk_DeleteAttributeType(key, CKA_PRIME_BITS);
    sftk_DeleteAttributeType(key, CKA_SUBPRIME_BITS);
    sftk_DeleteAttributeType(key, CKA_NSS_PQG_SEED_BITS);

    /* use the old PQG interface if we have old input data */
    if ((primeBits < 1024) || ((primeBits == 1024) && (subprimeBits == 0))) {
        if (seedBits == 0) {
            rv = PQG_ParamGen(j, &params, &vfy);
        } else {
            rv = PQG_ParamGenSeedLen(j, seedBits / 8, &params, &vfy);
        }
    } else {
        if (subprimeBits == 0) {
            subprimeBits = sftk_GetSubPrimeFromPrime(primeBits);
        }
        if (seedBits == 0) {
            seedBits = primeBits;
        }
        rv = PQG_ParamGenV2(primeBits, subprimeBits, seedBits / 8, &params, &vfy);
    }

    if (rv != SECSuccess) {
        if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
            sftk_fatalError = PR_TRUE;
        }
        return sftk_MapCryptError(PORT_GetError());
    }
    crv = sftk_AddAttributeType(key, CKA_PRIME,
                                params->prime.data, params->prime.len);
    if (crv != CKR_OK)
        goto loser;
    crv = sftk_AddAttributeType(key, CKA_SUBPRIME,
                                params->subPrime.data, params->subPrime.len);
    if (crv != CKR_OK)
        goto loser;
    crv = sftk_AddAttributeType(key, CKA_BASE,
                                params->base.data, params->base.len);
    if (crv != CKR_OK)
        goto loser;
    counter = vfy->counter;
    crv = sftk_AddAttributeType(key, CKA_NSS_PQG_COUNTER,
                                &counter, sizeof(counter));
    crv = sftk_AddAttributeType(key, CKA_NSS_PQG_SEED,
                                vfy->seed.data, vfy->seed.len);
    if (crv != CKR_OK)
        goto loser;
    crv = sftk_AddAttributeType(key, CKA_NSS_PQG_H,
                                vfy->h.data, vfy->h.len);
    if (crv != CKR_OK)
        goto loser;

loser:
    if (params) {
        PQG_DestroyParams(params);
    }

    if (vfy) {
        PQG_DestroyVerify(vfy);
    }
    return crv;
}

static CK_RV
nsc_SetupBulkKeyGen(CK_MECHANISM_TYPE mechanism, CK_KEY_TYPE *key_type,
                    CK_ULONG *key_length)
{
    CK_RV crv = CKR_OK;

    switch (mechanism) {
        case CKM_RC2_KEY_GEN:
            *key_type = CKK_RC2;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
#if NSS_SOFTOKEN_DOES_RC5
        case CKM_RC5_KEY_GEN:
            *key_type = CKK_RC5;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
#endif
        case CKM_RC4_KEY_GEN:
            *key_type = CKK_RC4;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
        case CKM_GENERIC_SECRET_KEY_GEN:
            *key_type = CKK_GENERIC_SECRET;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
        case CKM_CDMF_KEY_GEN:
            *key_type = CKK_CDMF;
            *key_length = 8;
            break;
        case CKM_DES_KEY_GEN:
            *key_type = CKK_DES;
            *key_length = 8;
            break;
        case CKM_DES2_KEY_GEN:
            *key_type = CKK_DES2;
            *key_length = 16;
            break;
        case CKM_DES3_KEY_GEN:
            *key_type = CKK_DES3;
            *key_length = 24;
            break;
        case CKM_SEED_KEY_GEN:
            *key_type = CKK_SEED;
            *key_length = 16;
            break;
        case CKM_CAMELLIA_KEY_GEN:
            *key_type = CKK_CAMELLIA;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
        case CKM_AES_KEY_GEN:
            *key_type = CKK_AES;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
        case CKM_NSS_CHACHA20_KEY_GEN:
            *key_type = CKK_NSS_CHACHA20;
            if (*key_length == 0)
                crv = CKR_TEMPLATE_INCOMPLETE;
            break;
        default:
            PORT_Assert(0);
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    return crv;
}

CK_RV
nsc_SetupHMACKeyGen(CK_MECHANISM_PTR pMechanism, NSSPKCS5PBEParameter **pbe)
{
    SECItem salt;
    CK_PBE_PARAMS *pbe_params = NULL;
    NSSPKCS5PBEParameter *params;
    PLArenaPool *arena = NULL;
    SECStatus rv;

    *pbe = NULL;

    arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
    if (arena == NULL) {
        return CKR_HOST_MEMORY;
    }

    params = (NSSPKCS5PBEParameter *)PORT_ArenaZAlloc(arena,
                                                      sizeof(NSSPKCS5PBEParameter));
    if (params == NULL) {
        PORT_FreeArena(arena, PR_TRUE);
        return CKR_HOST_MEMORY;
    }
    if (BAD_PARAM_CAST(pMechanism, sizeof(CK_PBE_PARAMS))) {
        PORT_FreeArena(arena, PR_TRUE);
        return CKR_MECHANISM_PARAM_INVALID;
    }

    params->poolp = arena;
    params->ivLen = 0;
    params->pbeType = NSSPKCS5_PKCS12_V2;
    params->hashType = HASH_AlgSHA1;
    params->encAlg = SEC_OID_SHA1; /* any invalid value */
    params->is2KeyDES = PR_FALSE;
    params->keyID = pbeBitGenIntegrityKey;
    pbe_params = (CK_PBE_PARAMS *)pMechanism->pParameter;
    params->iter = pbe_params->ulIteration;

    salt.data = (unsigned char *)pbe_params->pSalt;
    salt.len = (unsigned int)pbe_params->ulSaltLen;
    salt.type = siBuffer;
    rv = SECITEM_CopyItem(arena, &params->salt, &salt);
    if (rv != SECSuccess) {
        PORT_FreeArena(arena, PR_TRUE);
        return CKR_HOST_MEMORY;
    }
    switch (pMechanism->mechanism) {
        case CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN:
        case CKM_PBA_SHA1_WITH_SHA1_HMAC:
            params->hashType = HASH_AlgSHA1;
            params->keyLen = 20;
            break;
        case CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN:
            params->hashType = HASH_AlgMD5;
            params->keyLen = 16;
            break;
        case CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN:
            params->hashType = HASH_AlgMD2;
            params->keyLen = 16;
            break;
        case CKM_NSS_PKCS12_PBE_SHA224_HMAC_KEY_GEN:
            params->hashType = HASH_AlgSHA224;
            params->keyLen = 28;
            break;
        case CKM_NSS_PKCS12_PBE_SHA256_HMAC_KEY_GEN:
            params->hashType = HASH_AlgSHA256;
            params->keyLen = 32;
            break;
        case CKM_NSS_PKCS12_PBE_SHA384_HMAC_KEY_GEN:
            params->hashType = HASH_AlgSHA384;
            params->keyLen = 48;
            break;
        case CKM_NSS_PKCS12_PBE_SHA512_HMAC_KEY_GEN:
            params->hashType = HASH_AlgSHA512;
            params->keyLen = 64;
            break;
        default:
            PORT_FreeArena(arena, PR_TRUE);
            return CKR_MECHANISM_INVALID;
    }
    *pbe = params;
    return CKR_OK;
}

/* maybe this should be table driven? */
static CK_RV
nsc_SetupPBEKeyGen(CK_MECHANISM_PTR pMechanism, NSSPKCS5PBEParameter **pbe,
                   CK_KEY_TYPE *key_type, CK_ULONG *key_length)
{
    CK_RV crv = CKR_OK;
    SECOidData *oid;
    CK_PBE_PARAMS *pbe_params = NULL;
    NSSPKCS5PBEParameter *params = NULL;
    HASH_HashType hashType = HASH_AlgSHA1;
    CK_PKCS5_PBKD2_PARAMS *pbkd2_params = NULL;
    SECItem salt;
    CK_ULONG iteration = 0;

    *pbe = NULL;

    oid = SECOID_FindOIDByMechanism(pMechanism->mechanism);
    if (oid == NULL) {
        return CKR_MECHANISM_INVALID;
    }

    if (pMechanism->mechanism == CKM_PKCS5_PBKD2) {
        if (BAD_PARAM_CAST(pMechanism, sizeof(CK_PKCS5_PBKD2_PARAMS))) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
        pbkd2_params = (CK_PKCS5_PBKD2_PARAMS *)pMechanism->pParameter;
        switch (pbkd2_params->prf) {
            case CKP_PKCS5_PBKD2_HMAC_SHA1:
                hashType = HASH_AlgSHA1;
                break;
            case CKP_PKCS5_PBKD2_HMAC_SHA224:
                hashType = HASH_AlgSHA224;
                break;
            case CKP_PKCS5_PBKD2_HMAC_SHA256:
                hashType = HASH_AlgSHA256;
                break;
            case CKP_PKCS5_PBKD2_HMAC_SHA384:
                hashType = HASH_AlgSHA384;
                break;
            case CKP_PKCS5_PBKD2_HMAC_SHA512:
                hashType = HASH_AlgSHA512;
                break;
            default:
                return CKR_MECHANISM_PARAM_INVALID;
        }
        if (pbkd2_params->saltSource != CKZ_SALT_SPECIFIED) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
        salt.data = (unsigned char *)pbkd2_params->pSaltSourceData;
        salt.len = (unsigned int)pbkd2_params->ulSaltSourceDataLen;
        iteration = pbkd2_params->iterations;
    } else {
        if (BAD_PARAM_CAST(pMechanism, sizeof(CK_PBE_PARAMS))) {
            return CKR_MECHANISM_PARAM_INVALID;
        }
        pbe_params = (CK_PBE_PARAMS *)pMechanism->pParameter;
        salt.data = (unsigned char *)pbe_params->pSalt;
        salt.len = (unsigned int)pbe_params->ulSaltLen;
        iteration = pbe_params->ulIteration;
    }
    params = nsspkcs5_NewParam(oid->offset, hashType, &salt, iteration);
    if (params == NULL) {
        return CKR_MECHANISM_INVALID;
    }

    switch (params->encAlg) {
        case SEC_OID_DES_CBC:
            *key_type = CKK_DES;
            *key_length = params->keyLen;
            break;
        case SEC_OID_DES_EDE3_CBC:
            *key_type = params->is2KeyDES ? CKK_DES2 : CKK_DES3;
            *key_length = params->keyLen;
            break;
        case SEC_OID_RC2_CBC:
            *key_type = CKK_RC2;
            *key_length = params->keyLen;
            break;
        case SEC_OID_RC4:
            *key_type = CKK_RC4;
            *key_length = params->keyLen;
            break;
        case SEC_OID_PKCS5_PBKDF2:
            /* key type must already be set */
            if (*key_type == CKK_INVALID_KEY_TYPE) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                break;
            }
            /* PBKDF2 needs to calculate the key length from the other parameters
             */
            if (*key_length == 0) {
                *key_length = sftk_MapKeySize(*key_type);
            }
            if (*key_length == 0) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                break;
            }
            params->keyLen = *key_length;
            break;
        default:
            crv = CKR_MECHANISM_INVALID;
            nsspkcs5_DestroyPBEParameter(params);
            break;
    }
    if (crv == CKR_OK) {
        *pbe = params;
    }
    return crv;
}

/* NSC_GenerateKey generates a secret key, creating a new key object. */
CK_RV
NSC_GenerateKey(CK_SESSION_HANDLE hSession,
                CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
                CK_OBJECT_HANDLE_PTR phKey)
{
    SFTKObject *key;
    SFTKSession *session;
    PRBool checkWeak = PR_FALSE;
    CK_ULONG key_length = 0;
    CK_KEY_TYPE key_type = CKK_INVALID_KEY_TYPE;
    CK_OBJECT_CLASS objclass = CKO_SECRET_KEY;
    CK_RV crv = CKR_OK;
    CK_BBOOL cktrue = CK_TRUE;
    int i;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    unsigned char buf[MAX_KEY_LEN];
    enum { nsc_pbe,
           nsc_ssl,
           nsc_bulk,
           nsc_param,
           nsc_jpake } key_gen_type;
    NSSPKCS5PBEParameter *pbe_param;
    SSL3RSAPreMasterSecret *rsa_pms;
    CK_VERSION *version;
    /* in very old versions of NSS, there were implementation errors with key
     * generation methods.  We want to beable to read these, but not
     * produce them any more.  The affected algorithm was 3DES.
     */
    PRBool faultyPBE3DES = PR_FALSE;
    HASH_HashType hashType = HASH_AlgNULL;

    CHECK_FORK();

    if (!slot) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * now lets create an object to hang the attributes off of
     */
    key = sftk_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
        return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i = 0; i < (int)ulCount; i++) {
        if (pTemplate[i].type == CKA_VALUE_LEN) {
            key_length = *(CK_ULONG *)pTemplate[i].pValue;
            continue;
        }
        /* some algorithms need keytype specified */
        if (pTemplate[i].type == CKA_KEY_TYPE) {
            key_type = *(CK_ULONG *)pTemplate[i].pValue;
            continue;
        }

        crv = sftk_AddAttributeType(key, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK)
            break;
    }
    if (crv != CKR_OK) {
        goto loser;
    }

    /* make sure we don't have any class, key_type, or value fields */
    sftk_DeleteAttributeType(key, CKA_CLASS);
    sftk_DeleteAttributeType(key, CKA_KEY_TYPE);
    sftk_DeleteAttributeType(key, CKA_VALUE);

    /* Now Set up the parameters to generate the key (based on mechanism) */
    key_gen_type = nsc_bulk; /* bulk key by default */
    switch (pMechanism->mechanism) {
        case CKM_CDMF_KEY_GEN:
        case CKM_DES_KEY_GEN:
        case CKM_DES2_KEY_GEN:
        case CKM_DES3_KEY_GEN:
            checkWeak = PR_TRUE;
        /* fall through */
        case CKM_RC2_KEY_GEN:
        case CKM_RC4_KEY_GEN:
        case CKM_GENERIC_SECRET_KEY_GEN:
        case CKM_SEED_KEY_GEN:
        case CKM_CAMELLIA_KEY_GEN:
        case CKM_AES_KEY_GEN:
        case CKM_NSS_CHACHA20_KEY_GEN:
#if NSS_SOFTOKEN_DOES_RC5
        case CKM_RC5_KEY_GEN:
#endif
            crv = nsc_SetupBulkKeyGen(pMechanism->mechanism, &key_type, &key_length);
            break;
        case CKM_SSL3_PRE_MASTER_KEY_GEN:
            key_type = CKK_GENERIC_SECRET;
            key_length = 48;
            key_gen_type = nsc_ssl;
            break;
        case CKM_PBA_SHA1_WITH_SHA1_HMAC:
        case CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN:
        case CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN:
        case CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA224_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA256_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA384_HMAC_KEY_GEN:
        case CKM_NSS_PKCS12_PBE_SHA512_HMAC_KEY_GEN:
            key_gen_type = nsc_pbe;
            key_type = CKK_GENERIC_SECRET;
            crv = nsc_SetupHMACKeyGen(pMechanism, &pbe_param);
            break;
        case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
            faultyPBE3DES = PR_TRUE;
        /* fall through */
        case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
        case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
        case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
        case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
        case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
        case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
        case CKM_PBE_SHA1_DES3_EDE_CBC:
        case CKM_PBE_SHA1_DES2_EDE_CBC:
        case CKM_PBE_SHA1_RC2_128_CBC:
        case CKM_PBE_SHA1_RC2_40_CBC:
        case CKM_PBE_SHA1_RC4_128:
        case CKM_PBE_SHA1_RC4_40:
        case CKM_PBE_MD5_DES_CBC:
        case CKM_PBE_MD2_DES_CBC:
        case CKM_PKCS5_PBKD2:
            key_gen_type = nsc_pbe;
            crv = nsc_SetupPBEKeyGen(pMechanism, &pbe_param, &key_type, &key_length);
            break;
        case CKM_DSA_PARAMETER_GEN:
            key_gen_type = nsc_param;
            key_type = CKK_DSA;
            objclass = CKO_KG_PARAMETERS;
            crv = CKR_OK;
            break;
        case CKM_NSS_JPAKE_ROUND1_SHA1:
            hashType = HASH_AlgSHA1;
            goto jpake1;
        case CKM_NSS_JPAKE_ROUND1_SHA256:
            hashType = HASH_AlgSHA256;
            goto jpake1;
        case CKM_NSS_JPAKE_ROUND1_SHA384:
            hashType = HASH_AlgSHA384;
            goto jpake1;
        case CKM_NSS_JPAKE_ROUND1_SHA512:
            hashType = HASH_AlgSHA512;
            goto jpake1;
        jpake1:
            key_gen_type = nsc_jpake;
            key_type = CKK_NSS_JPAKE_ROUND1;
            objclass = CKO_PRIVATE_KEY;
            if (pMechanism->pParameter == NULL ||
                pMechanism->ulParameterLen != sizeof(CK_NSS_JPAKERound1Params)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            if (sftk_isTrue(key, CKA_TOKEN)) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            crv = CKR_OK;
            break;
        default:
            crv = CKR_MECHANISM_INVALID;
            break;
    }

    /* make sure we aren't going to overflow the buffer */
    if (sizeof(buf) < key_length) {
        /* someone is getting pretty optimistic about how big their key can
         * be... */
        crv = CKR_TEMPLATE_INCONSISTENT;
    }

    if (crv != CKR_OK) {
        goto loser;
    }

    /* if there was no error,
     * key_type *MUST* be set in the switch statement above */
    PORT_Assert(key_type != CKK_INVALID_KEY_TYPE);

    /*
     * now to the actual key gen.
     */
    switch (key_gen_type) {
        case nsc_pbe:
            crv = nsc_pbe_key_gen(pbe_param, pMechanism, buf, &key_length,
                                  faultyPBE3DES);
            nsspkcs5_DestroyPBEParameter(pbe_param);
            break;
        case nsc_ssl:
            rsa_pms = (SSL3RSAPreMasterSecret *)buf;
            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_VERSION))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                goto loser;
            }
            version = (CK_VERSION *)pMechanism->pParameter;
            rsa_pms->client_version[0] = version->major;
            rsa_pms->client_version[1] = version->minor;
            crv =
                NSC_GenerateRandom(0, &rsa_pms->random[0], sizeof(rsa_pms->random));
            break;
        case nsc_bulk:
            /* get the key, check for weak keys and repeat if found */
            do {
                crv = NSC_GenerateRandom(0, buf, key_length);
            } while (crv == CKR_OK && checkWeak && sftk_IsWeakKey(buf, key_type));
            break;
        case nsc_param:
            /* generate parameters */
            *buf = 0;
            crv = nsc_parameter_gen(key_type, key);
            break;
        case nsc_jpake:
            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_NSS_JPAKERound1Params))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                goto loser;
            }
            crv = jpake_Round1(hashType,
                               (CK_NSS_JPAKERound1Params *)pMechanism->pParameter,
                               key);
            break;
    }

    if (crv != CKR_OK) {
        goto loser;
    }

    /* Add the class, key_type, and value */
    crv = sftk_AddAttributeType(key, CKA_CLASS, &objclass, sizeof(CK_OBJECT_CLASS));
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = sftk_AddAttributeType(key, CKA_KEY_TYPE, &key_type, sizeof(CK_KEY_TYPE));
    if (crv != CKR_OK) {
        goto loser;
    }
    if (key_length != 0) {
        crv = sftk_AddAttributeType(key, CKA_VALUE, buf, key_length);
        if (crv != CKR_OK) {
            goto loser;
        }
    }

    /* get the session */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        crv = CKR_SESSION_HANDLE_INVALID;
        goto loser;
    }

    /*
     * handle the base object stuff
     */
    crv = sftk_handleObject(key, session);
    sftk_FreeSession(session);
    if (crv == CKR_OK && sftk_isTrue(key, CKA_SENSITIVE)) {
        crv = sftk_forceAttribute(key, CKA_ALWAYS_SENSITIVE, &cktrue, sizeof(CK_BBOOL));
    }
    if (crv == CKR_OK && !sftk_isTrue(key, CKA_EXTRACTABLE)) {
        crv = sftk_forceAttribute(key, CKA_NEVER_EXTRACTABLE, &cktrue, sizeof(CK_BBOOL));
    }
    if (crv == CKR_OK) {
        *phKey = key->handle;
    }
loser:
    sftk_FreeObject(key);
    return crv;
}

#define PAIRWISE_DIGEST_LENGTH SHA1_LENGTH /* 160-bits */
#define PAIRWISE_MESSAGE_LENGTH 20         /* 160-bits */

/*
 * FIPS 140-2 pairwise consistency check utilized to validate key pair.
 *
 * This function returns
 *   CKR_OK               if pairwise consistency check passed
 *   CKR_GENERAL_ERROR    if pairwise consistency check failed
 *   other error codes    if paiswise consistency check could not be
 *                        performed, for example, CKR_HOST_MEMORY.
 */
static CK_RV
sftk_PairwiseConsistencyCheck(CK_SESSION_HANDLE hSession,
                              SFTKObject *publicKey, SFTKObject *privateKey, CK_KEY_TYPE keyType)
{
    /*
     *                      Key type    Mechanism type
     *                      --------------------------------
     * For encrypt/decrypt: CKK_RSA  => CKM_RSA_PKCS
     *                      others   => CKM_INVALID_MECHANISM
     *
     * For sign/verify:     CKK_RSA  => CKM_RSA_PKCS
     *                      CKK_DSA  => CKM_DSA
     *                      CKK_EC   => CKM_ECDSA
     *                      others   => CKM_INVALID_MECHANISM
     *
     * None of these mechanisms has a parameter.
     */
    CK_MECHANISM mech = { 0, NULL, 0 };

    CK_ULONG modulusLen = 0;
    CK_ULONG subPrimeLen = 0;
    PRBool isEncryptable = PR_FALSE;
    PRBool canSignVerify = PR_FALSE;
    PRBool isDerivable = PR_FALSE;
    CK_RV crv;

    /* Variables used for Encrypt/Decrypt functions. */
    unsigned char *known_message = (unsigned char *)"Known Crypto Message";
    unsigned char plaintext[PAIRWISE_MESSAGE_LENGTH];
    CK_ULONG bytes_decrypted;
    unsigned char *ciphertext;
    unsigned char *text_compared;
    CK_ULONG bytes_encrypted;
    CK_ULONG bytes_compared;
    CK_ULONG pairwise_digest_length = PAIRWISE_DIGEST_LENGTH;

    /* Variables used for Signature/Verification functions. */
    /* Must be at least 256 bits for DSA2 digest */
    unsigned char *known_digest = (unsigned char *)"Mozilla Rules the World through NSS!";
    unsigned char *signature;
    CK_ULONG signature_length;

    if (keyType == CKK_RSA) {
        SFTKAttribute *attribute;

        /* Get modulus length of private key. */
        attribute = sftk_FindAttribute(privateKey, CKA_MODULUS);
        if (attribute == NULL) {
            return CKR_DEVICE_ERROR;
        }
        modulusLen = attribute->attrib.ulValueLen;
        if (*(unsigned char *)attribute->attrib.pValue == 0) {
            modulusLen--;
        }
        sftk_FreeAttribute(attribute);
    } else if (keyType == CKK_DSA) {
        SFTKAttribute *attribute;

        /* Get subprime length of private key. */
        attribute = sftk_FindAttribute(privateKey, CKA_SUBPRIME);
        if (attribute == NULL) {
            return CKR_DEVICE_ERROR;
        }
        subPrimeLen = attribute->attrib.ulValueLen;
        if (subPrimeLen > 1 && *(unsigned char *)attribute->attrib.pValue == 0) {
            subPrimeLen--;
        }
        sftk_FreeAttribute(attribute);
    }

    /**************************************************/
    /* Pairwise Consistency Check of Encrypt/Decrypt. */
    /**************************************************/

    isEncryptable = sftk_isTrue(privateKey, CKA_DECRYPT);

    /*
     * If the decryption attribute is set, attempt to encrypt
     * with the public key and decrypt with the private key.
     */
    if (isEncryptable) {
        if (keyType != CKK_RSA) {
            return CKR_DEVICE_ERROR;
        }
        bytes_encrypted = modulusLen;
        mech.mechanism = CKM_RSA_PKCS;

        /* Allocate space for ciphertext. */
        ciphertext = (unsigned char *)PORT_ZAlloc(bytes_encrypted);
        if (ciphertext == NULL) {
            return CKR_HOST_MEMORY;
        }

        /* Prepare for encryption using the public key. */
        crv = NSC_EncryptInit(hSession, &mech, publicKey->handle);
        if (crv != CKR_OK) {
            PORT_Free(ciphertext);
            return crv;
        }

        /* Encrypt using the public key. */
        crv = NSC_Encrypt(hSession,
                          known_message,
                          PAIRWISE_MESSAGE_LENGTH,
                          ciphertext,
                          &bytes_encrypted);
        if (crv != CKR_OK) {
            PORT_Free(ciphertext);
            return crv;
        }

        /* Always use the smaller of these two values . . . */
        bytes_compared = PR_MIN(bytes_encrypted, PAIRWISE_MESSAGE_LENGTH);

        /*
         * If there was a failure, the plaintext
         * goes at the end, therefore . . .
         */
        text_compared = ciphertext + bytes_encrypted - bytes_compared;

        /*
         * Check to ensure that ciphertext does
         * NOT EQUAL known input message text
         * per FIPS PUB 140-2 directive.
         */
        if (PORT_Memcmp(text_compared, known_message,
                        bytes_compared) == 0) {
            /* Set error to Invalid PRIVATE Key. */
            PORT_SetError(SEC_ERROR_INVALID_KEY);
            PORT_Free(ciphertext);
            return CKR_GENERAL_ERROR;
        }

        /* Prepare for decryption using the private key. */
        crv = NSC_DecryptInit(hSession, &mech, privateKey->handle);
        if (crv != CKR_OK) {
            PORT_Free(ciphertext);
            return crv;
        }

        memset(plaintext, 0, PAIRWISE_MESSAGE_LENGTH);

        /*
         * Initialize bytes decrypted to be the
         * expected PAIRWISE_MESSAGE_LENGTH.
         */
        bytes_decrypted = PAIRWISE_MESSAGE_LENGTH;

        /*
         * Decrypt using the private key.
         * NOTE:  No need to reset the
         *        value of bytes_encrypted.
         */
        crv = NSC_Decrypt(hSession,
                          ciphertext,
                          bytes_encrypted,
                          plaintext,
                          &bytes_decrypted);

        /* Finished with ciphertext; free it. */
        PORT_Free(ciphertext);

        if (crv != CKR_OK) {
            return crv;
        }

        /*
         * Check to ensure that the output plaintext
         * does EQUAL known input message text.
         */
        if ((bytes_decrypted != PAIRWISE_MESSAGE_LENGTH) ||
            (PORT_Memcmp(plaintext, known_message,
                         PAIRWISE_MESSAGE_LENGTH) != 0)) {
            /* Set error to Bad PUBLIC Key. */
            PORT_SetError(SEC_ERROR_BAD_KEY);
            return CKR_GENERAL_ERROR;
        }
    }

    /**********************************************/
    /* Pairwise Consistency Check of Sign/Verify. */
    /**********************************************/

    canSignVerify = sftk_isTrue(privateKey, CKA_SIGN);
    /* Unfortunately CKA_SIGN is always true in lg dbs. We have to check the
     * actual curve to determine if we can do sign/verify. */
    if (canSignVerify && keyType == CKK_EC) {
        NSSLOWKEYPrivateKey *privKey = sftk_GetPrivKey(privateKey, CKK_EC, &crv);
        if (privKey && privKey->u.ec.ecParams.name == ECCurve25519) {
            canSignVerify = PR_FALSE;
        }
    }

    if (canSignVerify) {
        /* Determine length of signature. */
        switch (keyType) {
            case CKK_RSA:
                signature_length = modulusLen;
                mech.mechanism = CKM_RSA_PKCS;
                break;
            case CKK_DSA:
                signature_length = DSA_MAX_SIGNATURE_LEN;
                pairwise_digest_length = subPrimeLen;
                mech.mechanism = CKM_DSA;
                break;
            case CKK_EC:
                signature_length = MAX_ECKEY_LEN * 2;
                mech.mechanism = CKM_ECDSA;
                break;
            default:
                return CKR_DEVICE_ERROR;
        }

        /* Allocate space for signature data. */
        signature = (unsigned char *)PORT_ZAlloc(signature_length);
        if (signature == NULL) {
            return CKR_HOST_MEMORY;
        }

        /* Sign the known hash using the private key. */
        crv = NSC_SignInit(hSession, &mech, privateKey->handle);
        if (crv != CKR_OK) {
            PORT_Free(signature);
            return crv;
        }

        crv = NSC_Sign(hSession,
                       known_digest,
                       pairwise_digest_length,
                       signature,
                       &signature_length);
        if (crv != CKR_OK) {
            PORT_Free(signature);
            return crv;
        }

        /* detect trivial signing transforms */
        if ((signature_length >= pairwise_digest_length) &&
            (PORT_Memcmp(known_digest, signature + (signature_length - pairwise_digest_length), pairwise_digest_length) == 0)) {
            PORT_Free(signature);
            return CKR_DEVICE_ERROR;
        }

        /* Verify the known hash using the public key. */
        crv = NSC_VerifyInit(hSession, &mech, publicKey->handle);
        if (crv != CKR_OK) {
            PORT_Free(signature);
            return crv;
        }

        crv = NSC_Verify(hSession,
                         known_digest,
                         pairwise_digest_length,
                         signature,
                         signature_length);

        /* Free signature data. */
        PORT_Free(signature);

        if ((crv == CKR_SIGNATURE_LEN_RANGE) ||
            (crv == CKR_SIGNATURE_INVALID)) {
            return CKR_GENERAL_ERROR;
        }
        if (crv != CKR_OK) {
            return crv;
        }
    }

    /**********************************************/
    /* Pairwise Consistency Check for Derivation  */
    /**********************************************/

    isDerivable = sftk_isTrue(privateKey, CKA_DERIVE);

    if (isDerivable) {
        /*
         * We are not doing consistency check for Diffie-Hellman Key -
         * otherwise it would be here
         * This is also true for Elliptic Curve Diffie-Hellman keys
         * NOTE: EC keys are currently subjected to pairwise
         * consistency check for signing/verification.
         */
        /*
         * FIPS 140-2 had the following pairwise consistency test for
         * public and private keys used for key agreement:
         *   If the keys are used to perform key agreement, then the
         *   cryptographic module shall create a second, compatible
         *   key pair.  The cryptographic module shall perform both
         *   sides of the key agreement algorithm and shall compare
         *   the resulting shared values.  If the shared values are
         *   not equal, the test shall fail.
         * This test was removed in Change Notice 3.
         */
    }

    return CKR_OK;
}

/* NSC_GenerateKeyPair generates a public-key/private-key pair,
 * creating new key objects. */
CK_RV
NSC_GenerateKeyPair(CK_SESSION_HANDLE hSession,
                    CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                    CK_ULONG ulPublicKeyAttributeCount, CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                    CK_ULONG ulPrivateKeyAttributeCount, CK_OBJECT_HANDLE_PTR phPublicKey,
                    CK_OBJECT_HANDLE_PTR phPrivateKey)
{
    SFTKObject *publicKey, *privateKey;
    SFTKSession *session;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    CK_BBOOL cktrue = CK_TRUE;
    SECStatus rv;
    CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
    CK_OBJECT_CLASS privClass = CKO_PRIVATE_KEY;
    int i;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    unsigned int bitSize;

    /* RSA */
    int public_modulus_bits = 0;
    SECItem pubExp;
    RSAPrivateKey *rsaPriv;

    /* DSA */
    PQGParams pqgParam;
    DHParams dhParam;
    DSAPrivateKey *dsaPriv;

    /* Diffie Hellman */
    DHPrivateKey *dhPriv;

    /* Elliptic Curve Cryptography */
    SECItem ecEncodedParams; /* DER Encoded parameters */
    ECPrivateKey *ecPriv;
    ECParams *ecParams;

    CHECK_FORK();

    if (!slot) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * now lets create an object to hang the attributes off of
     */
    publicKey = sftk_NewObject(slot); /* fill in the handle later */
    if (publicKey == NULL) {
        return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the publicKey
     */
    for (i = 0; i < (int)ulPublicKeyAttributeCount; i++) {
        if (pPublicKeyTemplate[i].type == CKA_MODULUS_BITS) {
            public_modulus_bits = *(CK_ULONG *)pPublicKeyTemplate[i].pValue;
            continue;
        }

        crv = sftk_AddAttributeType(publicKey,
                                    sftk_attr_expand(&pPublicKeyTemplate[i]));
        if (crv != CKR_OK)
            break;
    }

    if (crv != CKR_OK) {
        sftk_FreeObject(publicKey);
        return CKR_HOST_MEMORY;
    }

    privateKey = sftk_NewObject(slot); /* fill in the handle later */
    if (privateKey == NULL) {
        sftk_FreeObject(publicKey);
        return CKR_HOST_MEMORY;
    }
    /*
     * now load the private key template
     */
    for (i = 0; i < (int)ulPrivateKeyAttributeCount; i++) {
        if (pPrivateKeyTemplate[i].type == CKA_VALUE_BITS) {
            continue;
        }

        crv = sftk_AddAttributeType(privateKey,
                                    sftk_attr_expand(&pPrivateKeyTemplate[i]));
        if (crv != CKR_OK)
            break;
    }

    if (crv != CKR_OK) {
        sftk_FreeObject(publicKey);
        sftk_FreeObject(privateKey);
        return CKR_HOST_MEMORY;
    }
    sftk_DeleteAttributeType(privateKey, CKA_CLASS);
    sftk_DeleteAttributeType(privateKey, CKA_KEY_TYPE);
    sftk_DeleteAttributeType(privateKey, CKA_VALUE);
    sftk_DeleteAttributeType(publicKey, CKA_CLASS);
    sftk_DeleteAttributeType(publicKey, CKA_KEY_TYPE);
    sftk_DeleteAttributeType(publicKey, CKA_VALUE);

    /* Now Set up the parameters to generate the key (based on mechanism) */
    switch (pMechanism->mechanism) {
        case CKM_RSA_PKCS_KEY_PAIR_GEN:
            /* format the keys */
            sftk_DeleteAttributeType(publicKey, CKA_MODULUS);
            sftk_DeleteAttributeType(privateKey, CKA_NETSCAPE_DB);
            sftk_DeleteAttributeType(privateKey, CKA_MODULUS);
            sftk_DeleteAttributeType(privateKey, CKA_PRIVATE_EXPONENT);
            sftk_DeleteAttributeType(privateKey, CKA_PUBLIC_EXPONENT);
            sftk_DeleteAttributeType(privateKey, CKA_PRIME_1);
            sftk_DeleteAttributeType(privateKey, CKA_PRIME_2);
            sftk_DeleteAttributeType(privateKey, CKA_EXPONENT_1);
            sftk_DeleteAttributeType(privateKey, CKA_EXPONENT_2);
            sftk_DeleteAttributeType(privateKey, CKA_COEFFICIENT);
            key_type = CKK_RSA;
            if (public_modulus_bits == 0) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                break;
            }
            if (public_modulus_bits < RSA_MIN_MODULUS_BITS) {
                crv = CKR_ATTRIBUTE_VALUE_INVALID;
                break;
            }
            if (public_modulus_bits % 2 != 0) {
                crv = CKR_ATTRIBUTE_VALUE_INVALID;
                break;
            }

            /* extract the exponent */
            crv = sftk_Attribute2SSecItem(NULL, &pubExp, publicKey, CKA_PUBLIC_EXPONENT);
            if (crv != CKR_OK)
                break;
            bitSize = sftk_GetLengthInBits(pubExp.data, pubExp.len);
            if (bitSize < 2) {
                crv = CKR_ATTRIBUTE_VALUE_INVALID;
                PORT_Free(pubExp.data);
                break;
            }
            crv = sftk_AddAttributeType(privateKey, CKA_PUBLIC_EXPONENT,
                                        sftk_item_expand(&pubExp));
            if (crv != CKR_OK) {
                PORT_Free(pubExp.data);
                break;
            }

            rsaPriv = RSA_NewKey(public_modulus_bits, &pubExp);
            PORT_Free(pubExp.data);
            if (rsaPriv == NULL) {
                if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
                    sftk_fatalError = PR_TRUE;
                }
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }
            /* now fill in the RSA dependent paramenters in the public key */
            crv = sftk_AddAttributeType(publicKey, CKA_MODULUS,
                                        sftk_item_expand(&rsaPriv->modulus));
            if (crv != CKR_OK)
                goto kpg_done;
            /* now fill in the RSA dependent paramenters in the private key */
            crv = sftk_AddAttributeType(privateKey, CKA_NETSCAPE_DB,
                                        sftk_item_expand(&rsaPriv->modulus));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_MODULUS,
                                        sftk_item_expand(&rsaPriv->modulus));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_PRIVATE_EXPONENT,
                                        sftk_item_expand(&rsaPriv->privateExponent));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_PRIME_1,
                                        sftk_item_expand(&rsaPriv->prime1));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_PRIME_2,
                                        sftk_item_expand(&rsaPriv->prime2));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_EXPONENT_1,
                                        sftk_item_expand(&rsaPriv->exponent1));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_EXPONENT_2,
                                        sftk_item_expand(&rsaPriv->exponent2));
            if (crv != CKR_OK)
                goto kpg_done;
            crv = sftk_AddAttributeType(privateKey, CKA_COEFFICIENT,
                                        sftk_item_expand(&rsaPriv->coefficient));
        kpg_done:
            /* Should zeroize the contents first, since this func doesn't. */
            PORT_FreeArena(rsaPriv->arena, PR_TRUE);
            break;
        case CKM_DSA_KEY_PAIR_GEN:
            sftk_DeleteAttributeType(publicKey, CKA_VALUE);
            sftk_DeleteAttributeType(privateKey, CKA_NETSCAPE_DB);
            sftk_DeleteAttributeType(privateKey, CKA_PRIME);
            sftk_DeleteAttributeType(privateKey, CKA_SUBPRIME);
            sftk_DeleteAttributeType(privateKey, CKA_BASE);
            key_type = CKK_DSA;

            /* extract the necessary parameters and copy them to the private key */
            crv = sftk_Attribute2SSecItem(NULL, &pqgParam.prime, publicKey, CKA_PRIME);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(NULL, &pqgParam.subPrime, publicKey,
                                          CKA_SUBPRIME);
            if (crv != CKR_OK) {
                PORT_Free(pqgParam.prime.data);
                break;
            }
            crv = sftk_Attribute2SSecItem(NULL, &pqgParam.base, publicKey, CKA_BASE);
            if (crv != CKR_OK) {
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                break;
            }
            crv = sftk_AddAttributeType(privateKey, CKA_PRIME,
                                        sftk_item_expand(&pqgParam.prime));
            if (crv != CKR_OK) {
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                PORT_Free(pqgParam.base.data);
                break;
            }
            crv = sftk_AddAttributeType(privateKey, CKA_SUBPRIME,
                                        sftk_item_expand(&pqgParam.subPrime));
            if (crv != CKR_OK) {
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                PORT_Free(pqgParam.base.data);
                break;
            }
            crv = sftk_AddAttributeType(privateKey, CKA_BASE,
                                        sftk_item_expand(&pqgParam.base));
            if (crv != CKR_OK) {
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                PORT_Free(pqgParam.base.data);
                break;
            }

            /*
             * these are checked by DSA_NewKey
             */
            bitSize = sftk_GetLengthInBits(pqgParam.subPrime.data,
                                           pqgParam.subPrime.len);
            if ((bitSize < DSA_MIN_Q_BITS) || (bitSize > DSA_MAX_Q_BITS)) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                PORT_Free(pqgParam.base.data);
                break;
            }
            bitSize = sftk_GetLengthInBits(pqgParam.prime.data, pqgParam.prime.len);
            if ((bitSize < DSA_MIN_P_BITS) || (bitSize > DSA_MAX_P_BITS)) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                PORT_Free(pqgParam.base.data);
                break;
            }
            bitSize = sftk_GetLengthInBits(pqgParam.base.data, pqgParam.base.len);
            if ((bitSize < 2) || (bitSize > DSA_MAX_P_BITS)) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                PORT_Free(pqgParam.prime.data);
                PORT_Free(pqgParam.subPrime.data);
                PORT_Free(pqgParam.base.data);
                break;
            }

            /* Generate the key */
            rv = DSA_NewKey(&pqgParam, &dsaPriv);

            PORT_Free(pqgParam.prime.data);
            PORT_Free(pqgParam.subPrime.data);
            PORT_Free(pqgParam.base.data);

            if (rv != SECSuccess) {
                if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
                    sftk_fatalError = PR_TRUE;
                }
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }

            /* store the generated key into the attributes */
            crv = sftk_AddAttributeType(publicKey, CKA_VALUE,
                                        sftk_item_expand(&dsaPriv->publicValue));
            if (crv != CKR_OK)
                goto dsagn_done;

            /* now fill in the RSA dependent paramenters in the private key */
            crv = sftk_AddAttributeType(privateKey, CKA_NETSCAPE_DB,
                                        sftk_item_expand(&dsaPriv->publicValue));
            if (crv != CKR_OK)
                goto dsagn_done;
            crv = sftk_AddAttributeType(privateKey, CKA_VALUE,
                                        sftk_item_expand(&dsaPriv->privateValue));

        dsagn_done:
            /* should zeroize, since this function doesn't. */
            PORT_FreeArena(dsaPriv->params.arena, PR_TRUE);
            break;

        case CKM_DH_PKCS_KEY_PAIR_GEN:
            sftk_DeleteAttributeType(privateKey, CKA_PRIME);
            sftk_DeleteAttributeType(privateKey, CKA_BASE);
            sftk_DeleteAttributeType(privateKey, CKA_VALUE);
            sftk_DeleteAttributeType(privateKey, CKA_NETSCAPE_DB);
            key_type = CKK_DH;

            /* extract the necessary parameters and copy them to private keys */
            crv = sftk_Attribute2SSecItem(NULL, &dhParam.prime, publicKey,
                                          CKA_PRIME);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(NULL, &dhParam.base, publicKey, CKA_BASE);
            if (crv != CKR_OK) {
                PORT_Free(dhParam.prime.data);
                break;
            }
            crv = sftk_AddAttributeType(privateKey, CKA_PRIME,
                                        sftk_item_expand(&dhParam.prime));
            if (crv != CKR_OK) {
                PORT_Free(dhParam.prime.data);
                PORT_Free(dhParam.base.data);
                break;
            }
            crv = sftk_AddAttributeType(privateKey, CKA_BASE,
                                        sftk_item_expand(&dhParam.base));
            if (crv != CKR_OK) {
                PORT_Free(dhParam.prime.data);
                PORT_Free(dhParam.base.data);
                break;
            }
            bitSize = sftk_GetLengthInBits(dhParam.prime.data, dhParam.prime.len);
            if ((bitSize < DH_MIN_P_BITS) || (bitSize > DH_MAX_P_BITS)) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                PORT_Free(dhParam.prime.data);
                PORT_Free(dhParam.base.data);
                break;
            }
            bitSize = sftk_GetLengthInBits(dhParam.base.data, dhParam.base.len);
            if ((bitSize < 1) || (bitSize > DH_MAX_P_BITS)) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                PORT_Free(dhParam.prime.data);
                PORT_Free(dhParam.base.data);
                break;
            }

            rv = DH_NewKey(&dhParam, &dhPriv);
            PORT_Free(dhParam.prime.data);
            PORT_Free(dhParam.base.data);
            if (rv != SECSuccess) {
                if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
                    sftk_fatalError = PR_TRUE;
                }
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }

            crv = sftk_AddAttributeType(publicKey, CKA_VALUE,
                                        sftk_item_expand(&dhPriv->publicValue));
            if (crv != CKR_OK)
                goto dhgn_done;

            crv = sftk_AddAttributeType(privateKey, CKA_NETSCAPE_DB,
                                        sftk_item_expand(&dhPriv->publicValue));
            if (crv != CKR_OK)
                goto dhgn_done;

            crv = sftk_AddAttributeType(privateKey, CKA_VALUE,
                                        sftk_item_expand(&dhPriv->privateValue));

        dhgn_done:
            /* should zeroize, since this function doesn't. */
            PORT_FreeArena(dhPriv->arena, PR_TRUE);
            break;

        case CKM_EC_KEY_PAIR_GEN:
            sftk_DeleteAttributeType(privateKey, CKA_EC_PARAMS);
            sftk_DeleteAttributeType(privateKey, CKA_VALUE);
            sftk_DeleteAttributeType(privateKey, CKA_NETSCAPE_DB);
            key_type = CKK_EC;

            /* extract the necessary parameters and copy them to private keys */
            crv = sftk_Attribute2SSecItem(NULL, &ecEncodedParams, publicKey,
                                          CKA_EC_PARAMS);
            if (crv != CKR_OK)
                break;

            crv = sftk_AddAttributeType(privateKey, CKA_EC_PARAMS,
                                        sftk_item_expand(&ecEncodedParams));
            if (crv != CKR_OK) {
                PORT_Free(ecEncodedParams.data);
                break;
            }

            /* Decode ec params before calling EC_NewKey */
            rv = EC_DecodeParams(&ecEncodedParams, &ecParams);
            PORT_Free(ecEncodedParams.data);
            if (rv != SECSuccess) {
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }
            rv = EC_NewKey(ecParams, &ecPriv);
            if (rv != SECSuccess) {
                if (PORT_GetError() == SEC_ERROR_LIBRARY_FAILURE) {
                    sftk_fatalError = PR_TRUE;
                }
                PORT_FreeArena(ecParams->arena, PR_TRUE);
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }

            if (PR_GetEnvSecure("NSS_USE_DECODED_CKA_EC_POINT") ||
                ecParams->fieldID.type == ec_field_plain) {
                PORT_FreeArena(ecParams->arena, PR_TRUE);
                crv = sftk_AddAttributeType(publicKey, CKA_EC_POINT,
                                            sftk_item_expand(&ecPriv->publicValue));
            } else {
                PORT_FreeArena(ecParams->arena, PR_TRUE);
                SECItem *pubValue = SEC_ASN1EncodeItem(NULL, NULL,
                                                       &ecPriv->publicValue,
                                                       SEC_ASN1_GET(SEC_OctetStringTemplate));
                if (!pubValue) {
                    crv = CKR_ARGUMENTS_BAD;
                    goto ecgn_done;
                }
                crv = sftk_AddAttributeType(publicKey, CKA_EC_POINT,
                                            sftk_item_expand(pubValue));
                SECITEM_FreeItem(pubValue, PR_TRUE);
            }
            if (crv != CKR_OK)
                goto ecgn_done;

            crv = sftk_AddAttributeType(privateKey, CKA_VALUE,
                                        sftk_item_expand(&ecPriv->privateValue));
            if (crv != CKR_OK)
                goto ecgn_done;

            crv = sftk_AddAttributeType(privateKey, CKA_NETSCAPE_DB,
                                        sftk_item_expand(&ecPriv->publicValue));
        ecgn_done:
            /* should zeroize, since this function doesn't. */
            PORT_FreeArena(ecPriv->ecParams.arena, PR_TRUE);
            break;

        default:
            crv = CKR_MECHANISM_INVALID;
    }

    if (crv != CKR_OK) {
        sftk_FreeObject(privateKey);
        sftk_FreeObject(publicKey);
        return crv;
    }

    /* Add the class, key_type The loop lets us check errors blow out
     *  on errors and clean up at the bottom */
    session = NULL; /* make pedtantic happy... session cannot leave the*/
                    /* loop below NULL unless an error is set... */
    do {
        crv = sftk_AddAttributeType(privateKey, CKA_CLASS, &privClass,
                                    sizeof(CK_OBJECT_CLASS));
        if (crv != CKR_OK)
            break;
        crv = sftk_AddAttributeType(publicKey, CKA_CLASS, &pubClass,
                                    sizeof(CK_OBJECT_CLASS));
        if (crv != CKR_OK)
            break;
        crv = sftk_AddAttributeType(privateKey, CKA_KEY_TYPE, &key_type,
                                    sizeof(CK_KEY_TYPE));
        if (crv != CKR_OK)
            break;
        crv = sftk_AddAttributeType(publicKey, CKA_KEY_TYPE, &key_type,
                                    sizeof(CK_KEY_TYPE));
        if (crv != CKR_OK)
            break;
        session = sftk_SessionFromHandle(hSession);
        if (session == NULL)
            crv = CKR_SESSION_HANDLE_INVALID;
    } while (0);

    if (crv != CKR_OK) {
        sftk_FreeObject(privateKey);
        sftk_FreeObject(publicKey);
        return crv;
    }

    /*
     * handle the base object cleanup for the public Key
     */
    crv = sftk_handleObject(privateKey, session);
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        sftk_FreeObject(privateKey);
        sftk_FreeObject(publicKey);
        return crv;
    }

    /*
     * handle the base object cleanup for the private Key
     * If we have any problems, we destroy the public Key we've
     * created and linked.
     */
    crv = sftk_handleObject(publicKey, session);
    sftk_FreeSession(session);
    if (crv != CKR_OK) {
        sftk_FreeObject(publicKey);
        NSC_DestroyObject(hSession, privateKey->handle);
        sftk_FreeObject(privateKey);
        return crv;
    }
    if (sftk_isTrue(privateKey, CKA_SENSITIVE)) {
        crv = sftk_forceAttribute(privateKey, CKA_ALWAYS_SENSITIVE,
                                  &cktrue, sizeof(CK_BBOOL));
    }
    if (crv == CKR_OK && sftk_isTrue(publicKey, CKA_SENSITIVE)) {
        crv = sftk_forceAttribute(publicKey, CKA_ALWAYS_SENSITIVE,
                                  &cktrue, sizeof(CK_BBOOL));
    }
    if (crv == CKR_OK && !sftk_isTrue(privateKey, CKA_EXTRACTABLE)) {
        crv = sftk_forceAttribute(privateKey, CKA_NEVER_EXTRACTABLE,
                                  &cktrue, sizeof(CK_BBOOL));
    }
    if (crv == CKR_OK && !sftk_isTrue(publicKey, CKA_EXTRACTABLE)) {
        crv = sftk_forceAttribute(publicKey, CKA_NEVER_EXTRACTABLE,
                                  &cktrue, sizeof(CK_BBOOL));
    }

    if (crv == CKR_OK) {
        /* Perform FIPS 140-2 pairwise consistency check. */
        crv = sftk_PairwiseConsistencyCheck(hSession,
                                            publicKey, privateKey, key_type);
        if (crv != CKR_OK) {
            if (sftk_audit_enabled) {
                char msg[128];
                PR_snprintf(msg, sizeof msg,
                            "C_GenerateKeyPair(hSession=0x%08lX, "
                            "pMechanism->mechanism=0x%08lX)=0x%08lX "
                            "self-test: pair-wise consistency test failed",
                            (PRUint32)hSession, (PRUint32)pMechanism->mechanism,
                            (PRUint32)crv);
                sftk_LogAuditMessage(NSS_AUDIT_ERROR, NSS_AUDIT_SELF_TEST, msg);
            }
        }
    }

    if (crv != CKR_OK) {
        NSC_DestroyObject(hSession, publicKey->handle);
        sftk_FreeObject(publicKey);
        NSC_DestroyObject(hSession, privateKey->handle);
        sftk_FreeObject(privateKey);
        return crv;
    }

    *phPrivateKey = privateKey->handle;
    *phPublicKey = publicKey->handle;
    sftk_FreeObject(publicKey);
    sftk_FreeObject(privateKey);

    return CKR_OK;
}

static SECItem *
sftk_PackagePrivateKey(SFTKObject *key, CK_RV *crvp)
{
    NSSLOWKEYPrivateKey *lk = NULL;
    NSSLOWKEYPrivateKeyInfo *pki = NULL;
    SFTKAttribute *attribute = NULL;
    PLArenaPool *arena = NULL;
    SECOidTag algorithm = SEC_OID_UNKNOWN;
    void *dummy, *param = NULL;
    SECStatus rv = SECSuccess;
    SECItem *encodedKey = NULL;
#ifdef EC_DEBUG
    SECItem *fordebug;
#endif
    int savelen;

    if (!key) {
        *crvp = CKR_KEY_HANDLE_INVALID; /* really can't happen */
        return NULL;
    }

    attribute = sftk_FindAttribute(key, CKA_KEY_TYPE);
    if (!attribute) {
        *crvp = CKR_KEY_TYPE_INCONSISTENT;
        return NULL;
    }

    lk = sftk_GetPrivKey(key, *(CK_KEY_TYPE *)attribute->attrib.pValue, crvp);
    sftk_FreeAttribute(attribute);
    if (!lk) {
        return NULL;
    }

    arena = PORT_NewArena(2048); /* XXX different size? */
    if (!arena) {
        *crvp = CKR_HOST_MEMORY;
        rv = SECFailure;
        goto loser;
    }

    pki = (NSSLOWKEYPrivateKeyInfo *)PORT_ArenaZAlloc(arena,
                                                      sizeof(NSSLOWKEYPrivateKeyInfo));
    if (!pki) {
        *crvp = CKR_HOST_MEMORY;
        rv = SECFailure;
        goto loser;
    }
    pki->arena = arena;

    param = NULL;
    switch (lk->keyType) {
        case NSSLOWKEYRSAKey:
            prepare_low_rsa_priv_key_for_asn1(lk);
            dummy = SEC_ASN1EncodeItem(arena, &pki->privateKey, lk,
                                       nsslowkey_RSAPrivateKeyTemplate);

            /* determine RSA key type from the CKA_PUBLIC_KEY_INFO if present */
            attribute = sftk_FindAttribute(key, CKA_PUBLIC_KEY_INFO);
            if (attribute) {
                NSSLOWKEYSubjectPublicKeyInfo *publicKeyInfo;
                SECItem spki;

                spki.data = attribute->attrib.pValue;
                spki.len = attribute->attrib.ulValueLen;

                publicKeyInfo = PORT_ArenaZAlloc(arena,
                                                 sizeof(NSSLOWKEYSubjectPublicKeyInfo));
                if (!publicKeyInfo) {
                    sftk_FreeAttribute(attribute);
                    *crvp = CKR_HOST_MEMORY;
                    rv = SECFailure;
                    goto loser;
                }
                rv = SEC_QuickDERDecodeItem(arena, publicKeyInfo,
                                            nsslowkey_SubjectPublicKeyInfoTemplate,
                                            &spki);
                if (rv != SECSuccess) {
                    sftk_FreeAttribute(attribute);
                    *crvp = CKR_KEY_TYPE_INCONSISTENT;
                    goto loser;
                }
                algorithm = SECOID_GetAlgorithmTag(&publicKeyInfo->algorithm);
                if (algorithm != SEC_OID_PKCS1_RSA_ENCRYPTION &&
                    algorithm != SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {
                    sftk_FreeAttribute(attribute);
                    rv = SECFailure;
                    *crvp = CKR_KEY_TYPE_INCONSISTENT;
                    goto loser;
                }
                param = SECITEM_DupItem(&publicKeyInfo->algorithm.parameters);
                if (!param) {
                    sftk_FreeAttribute(attribute);
                    rv = SECFailure;
                    *crvp = CKR_HOST_MEMORY;
                    goto loser;
                }
                sftk_FreeAttribute(attribute);
            } else {
                /* default to PKCS #1 */
                algorithm = SEC_OID_PKCS1_RSA_ENCRYPTION;
            }
            break;
        case NSSLOWKEYDSAKey:
            prepare_low_dsa_priv_key_export_for_asn1(lk);
            dummy = SEC_ASN1EncodeItem(arena, &pki->privateKey, lk,
                                       nsslowkey_DSAPrivateKeyExportTemplate);
            prepare_low_pqg_params_for_asn1(&lk->u.dsa.params);
            param = SEC_ASN1EncodeItem(NULL, NULL, &(lk->u.dsa.params),
                                       nsslowkey_PQGParamsTemplate);
            algorithm = SEC_OID_ANSIX9_DSA_SIGNATURE;
            break;
        case NSSLOWKEYECKey:
            prepare_low_ec_priv_key_for_asn1(lk);
            /* Public value is encoded as a bit string so adjust length
             * to be in bits before ASN encoding and readjust
             * immediately after.
             *
             * Since the SECG specification recommends not including the
             * parameters as part of ECPrivateKey, we zero out the curveOID
             * length before encoding and restore it later.
             */
            lk->u.ec.publicValue.len <<= 3;
            savelen = lk->u.ec.ecParams.curveOID.len;
            lk->u.ec.ecParams.curveOID.len = 0;
            dummy = SEC_ASN1EncodeItem(arena, &pki->privateKey, lk,
                                       nsslowkey_ECPrivateKeyTemplate);
            lk->u.ec.ecParams.curveOID.len = savelen;
            lk->u.ec.publicValue.len >>= 3;

#ifdef EC_DEBUG
            fordebug = &pki->privateKey;
            SEC_PRINT("sftk_PackagePrivateKey()", "PrivateKey", lk->keyType,
                      fordebug);
#endif

            param = SECITEM_DupItem(&lk->u.ec.ecParams.DEREncoding);

            algorithm = SEC_OID_ANSIX962_EC_PUBLIC_KEY;
            break;
        case NSSLOWKEYDHKey:
        default:
            dummy = NULL;
            break;
    }

    if (!dummy || ((lk->keyType == NSSLOWKEYDSAKey) && !param)) {
        *crvp = CKR_DEVICE_ERROR; /* should map NSS SECError */
        rv = SECFailure;
        goto loser;
    }

    rv = SECOID_SetAlgorithmID(arena, &pki->algorithm, algorithm,
                               (SECItem *)param);
    if (rv != SECSuccess) {
        *crvp = CKR_DEVICE_ERROR; /* should map NSS SECError */
        rv = SECFailure;
        goto loser;
    }

    dummy = SEC_ASN1EncodeInteger(arena, &pki->version,
                                  NSSLOWKEY_PRIVATE_KEY_INFO_VERSION);
    if (!dummy) {
        *crvp = CKR_DEVICE_ERROR; /* should map NSS SECError */
        rv = SECFailure;
        goto loser;
    }

    encodedKey = SEC_ASN1EncodeItem(NULL, NULL, pki,
                                    nsslowkey_PrivateKeyInfoTemplate);
    *crvp = encodedKey ? CKR_OK : CKR_DEVICE_ERROR;

#ifdef EC_DEBUG
    fordebug = encodedKey;
    SEC_PRINT("sftk_PackagePrivateKey()", "PrivateKeyInfo", lk->keyType,
              fordebug);
#endif
loser:
    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }

    if (lk && (lk != key->objectInfo)) {
        nsslowkey_DestroyPrivateKey(lk);
    }

    if (param) {
        SECITEM_ZfreeItem((SECItem *)param, PR_TRUE);
    }

    if (rv != SECSuccess) {
        return NULL;
    }

    return encodedKey;
}

/* it doesn't matter yet, since we colapse error conditions in the
 * level above, but we really should map those few key error differences */
static CK_RV
sftk_mapWrap(CK_RV crv)
{
    switch (crv) {
        case CKR_ENCRYPTED_DATA_INVALID:
            crv = CKR_WRAPPED_KEY_INVALID;
            break;
    }
    return crv;
}

/* NSC_WrapKey wraps (i.e., encrypts) a key. */
CK_RV
NSC_WrapKey(CK_SESSION_HANDLE hSession,
            CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hWrappingKey,
            CK_OBJECT_HANDLE hKey, CK_BYTE_PTR pWrappedKey,
            CK_ULONG_PTR pulWrappedKeyLen)
{
    SFTKSession *session;
    SFTKAttribute *attribute;
    SFTKObject *key;
    CK_RV crv;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    key = sftk_ObjectFromHandle(hKey, session);
    if (key == NULL) {
        sftk_FreeSession(session);
        return CKR_KEY_HANDLE_INVALID;
    }

    switch (key->objclass) {
        case CKO_SECRET_KEY: {
            SFTKSessionContext *context = NULL;
            SECItem pText;

            attribute = sftk_FindAttribute(key, CKA_VALUE);

            if (attribute == NULL) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            crv = sftk_CryptInit(hSession, pMechanism, hWrappingKey,
                                 CKA_WRAP, CKA_WRAP, SFTK_ENCRYPT, PR_TRUE);
            if (crv != CKR_OK) {
                sftk_FreeAttribute(attribute);
                break;
            }

            pText.type = siBuffer;
            pText.data = (unsigned char *)attribute->attrib.pValue;
            pText.len = attribute->attrib.ulValueLen;

            /* Find out if this is a block cipher. */
            crv = sftk_GetContext(hSession, &context, SFTK_ENCRYPT, PR_FALSE, NULL);
            if (crv != CKR_OK || !context)
                break;
            if (context->blockSize > 1) {
                unsigned int remainder = pText.len % context->blockSize;
                if (!context->doPad && remainder) {
                    /* When wrapping secret keys with unpadded block ciphers,
                    ** the keys are zero padded, if necessary, to fill out
                    ** a full block.
                    */
                    pText.len += context->blockSize - remainder;
                    pText.data = PORT_ZAlloc(pText.len);
                    if (pText.data)
                        memcpy(pText.data, attribute->attrib.pValue,
                               attribute->attrib.ulValueLen);
                    else {
                        crv = CKR_HOST_MEMORY;
                        break;
                    }
                }
            }

            crv = NSC_Encrypt(hSession, (CK_BYTE_PTR)pText.data,
                              pText.len, pWrappedKey, pulWrappedKeyLen);
            /* always force a finalize, both on errors and when
             * we are just getting the size */
            if (crv != CKR_OK || pWrappedKey == NULL) {
                CK_RV lcrv;
                lcrv = sftk_GetContext(hSession, &context,
                                       SFTK_ENCRYPT, PR_FALSE, NULL);
                sftk_SetContextByType(session, SFTK_ENCRYPT, NULL);
                if (lcrv == CKR_OK && context) {
                    sftk_FreeContext(context);
                }
            }

            if (pText.data != (unsigned char *)attribute->attrib.pValue)
                PORT_ZFree(pText.data, pText.len);
            sftk_FreeAttribute(attribute);
            break;
        }

        case CKO_PRIVATE_KEY: {
            SECItem *bpki = sftk_PackagePrivateKey(key, &crv);
            SFTKSessionContext *context = NULL;

            if (!bpki) {
                break;
            }

            crv = sftk_CryptInit(hSession, pMechanism, hWrappingKey,
                                 CKA_WRAP, CKA_WRAP, SFTK_ENCRYPT, PR_TRUE);
            if (crv != CKR_OK) {
                SECITEM_ZfreeItem(bpki, PR_TRUE);
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }

            crv = NSC_Encrypt(hSession, bpki->data, bpki->len,
                              pWrappedKey, pulWrappedKeyLen);
            /* always force a finalize */
            if (crv != CKR_OK || pWrappedKey == NULL) {
                CK_RV lcrv;
                lcrv = sftk_GetContext(hSession, &context,
                                       SFTK_ENCRYPT, PR_FALSE, NULL);
                sftk_SetContextByType(session, SFTK_ENCRYPT, NULL);
                if (lcrv == CKR_OK && context) {
                    sftk_FreeContext(context);
                }
            }
            SECITEM_ZfreeItem(bpki, PR_TRUE);
            break;
        }

        default:
            crv = CKR_KEY_TYPE_INCONSISTENT;
            break;
    }
    sftk_FreeObject(key);
    sftk_FreeSession(session);
    return sftk_mapWrap(crv);
}

/*
 * import a pprivate key info into the desired slot
 */
static SECStatus
sftk_unwrapPrivateKey(SFTKObject *key, SECItem *bpki)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_KEY_TYPE keyType = CKK_RSA;
    SECStatus rv = SECFailure;
    const SEC_ASN1Template *keyTemplate, *paramTemplate;
    void *paramDest = NULL;
    PLArenaPool *arena;
    NSSLOWKEYPrivateKey *lpk = NULL;
    NSSLOWKEYPrivateKeyInfo *pki = NULL;
    CK_RV crv = CKR_KEY_TYPE_INCONSISTENT;

    arena = PORT_NewArena(2048);
    if (!arena) {
        return SECFailure;
    }

    pki = (NSSLOWKEYPrivateKeyInfo *)PORT_ArenaZAlloc(arena,
                                                      sizeof(NSSLOWKEYPrivateKeyInfo));
    if (!pki) {
        PORT_FreeArena(arena, PR_FALSE);
        return SECFailure;
    }

    if (SEC_ASN1DecodeItem(arena, pki, nsslowkey_PrivateKeyInfoTemplate, bpki) != SECSuccess) {
        PORT_FreeArena(arena, PR_TRUE);
        return SECFailure;
    }

    lpk = (NSSLOWKEYPrivateKey *)PORT_ArenaZAlloc(arena,
                                                  sizeof(NSSLOWKEYPrivateKey));
    if (lpk == NULL) {
        goto loser;
    }
    lpk->arena = arena;

    switch (SECOID_GetAlgorithmTag(&pki->algorithm)) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
        case SEC_OID_PKCS1_RSA_PSS_SIGNATURE:
            keyTemplate = nsslowkey_RSAPrivateKeyTemplate;
            paramTemplate = NULL;
            paramDest = NULL;
            lpk->keyType = NSSLOWKEYRSAKey;
            prepare_low_rsa_priv_key_for_asn1(lpk);
            break;
        case SEC_OID_ANSIX9_DSA_SIGNATURE:
            keyTemplate = nsslowkey_DSAPrivateKeyExportTemplate;
            paramTemplate = nsslowkey_PQGParamsTemplate;
            paramDest = &(lpk->u.dsa.params);
            lpk->keyType = NSSLOWKEYDSAKey;
            prepare_low_dsa_priv_key_export_for_asn1(lpk);
            prepare_low_pqg_params_for_asn1(&lpk->u.dsa.params);
            break;
        /* case NSSLOWKEYDHKey: */
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            keyTemplate = nsslowkey_ECPrivateKeyTemplate;
            paramTemplate = NULL;
            paramDest = &(lpk->u.ec.ecParams.DEREncoding);
            lpk->keyType = NSSLOWKEYECKey;
            prepare_low_ec_priv_key_for_asn1(lpk);
            prepare_low_ecparams_for_asn1(&lpk->u.ec.ecParams);
            break;
        default:
            keyTemplate = NULL;
            paramTemplate = NULL;
            paramDest = NULL;
            break;
    }

    if (!keyTemplate) {
        goto loser;
    }

    /* decode the private key and any algorithm parameters */
    rv = SEC_QuickDERDecodeItem(arena, lpk, keyTemplate, &pki->privateKey);

    if (lpk->keyType == NSSLOWKEYECKey) {
        /* convert length in bits to length in bytes */
        lpk->u.ec.publicValue.len >>= 3;
        rv = SECITEM_CopyItem(arena,
                              &(lpk->u.ec.ecParams.DEREncoding),
                              &(pki->algorithm.parameters));
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    if (rv != SECSuccess) {
        goto loser;
    }
    if (paramDest && paramTemplate) {
        rv = SEC_QuickDERDecodeItem(arena, paramDest, paramTemplate,
                                    &(pki->algorithm.parameters));
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    rv = SECFailure;

    switch (lpk->keyType) {
        case NSSLOWKEYRSAKey:
            keyType = CKK_RSA;
            if (sftk_hasAttribute(key, CKA_NETSCAPE_DB)) {
                sftk_DeleteAttributeType(key, CKA_NETSCAPE_DB);
            }
            crv = sftk_AddAttributeType(key, CKA_KEY_TYPE, &keyType,
                                        sizeof(keyType));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_UNWRAP, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_DECRYPT, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SIGN, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SIGN_RECOVER, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_MODULUS,
                                        sftk_item_expand(&lpk->u.rsa.modulus));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_PUBLIC_EXPONENT,
                                        sftk_item_expand(&lpk->u.rsa.publicExponent));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_PRIVATE_EXPONENT,
                                        sftk_item_expand(&lpk->u.rsa.privateExponent));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_PRIME_1,
                                        sftk_item_expand(&lpk->u.rsa.prime1));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_PRIME_2,
                                        sftk_item_expand(&lpk->u.rsa.prime2));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_EXPONENT_1,
                                        sftk_item_expand(&lpk->u.rsa.exponent1));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_EXPONENT_2,
                                        sftk_item_expand(&lpk->u.rsa.exponent2));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_COEFFICIENT,
                                        sftk_item_expand(&lpk->u.rsa.coefficient));
            break;
        case NSSLOWKEYDSAKey:
            keyType = CKK_DSA;
            crv = (sftk_hasAttribute(key, CKA_NETSCAPE_DB)) ? CKR_OK : CKR_KEY_TYPE_INCONSISTENT;
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_KEY_TYPE, &keyType,
                                        sizeof(keyType));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SIGN, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SIGN_RECOVER, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_PRIME,
                                        sftk_item_expand(&lpk->u.dsa.params.prime));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SUBPRIME,
                                        sftk_item_expand(&lpk->u.dsa.params.subPrime));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_BASE,
                                        sftk_item_expand(&lpk->u.dsa.params.base));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_VALUE,
                                        sftk_item_expand(&lpk->u.dsa.privateValue));
            if (crv != CKR_OK)
                break;
            break;
#ifdef notdef
        case NSSLOWKEYDHKey:
            template = dhTemplate;
            templateCount = sizeof(dhTemplate) / sizeof(CK_ATTRIBUTE);
            keyType = CKK_DH;
            break;
#endif
        /* what about fortezza??? */
        case NSSLOWKEYECKey:
            keyType = CKK_EC;
            crv = (sftk_hasAttribute(key, CKA_NETSCAPE_DB)) ? CKR_OK : CKR_KEY_TYPE_INCONSISTENT;
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_KEY_TYPE, &keyType,
                                        sizeof(keyType));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SIGN, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_SIGN_RECOVER, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_DERIVE, &cktrue,
                                        sizeof(CK_BBOOL));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_EC_PARAMS,
                                        sftk_item_expand(&lpk->u.ec.ecParams.DEREncoding));
            if (crv != CKR_OK)
                break;
            crv = sftk_AddAttributeType(key, CKA_VALUE,
                                        sftk_item_expand(&lpk->u.ec.privateValue));
            if (crv != CKR_OK)
                break;
            /* XXX Do we need to decode the EC Params here ?? */
            break;
        default:
            crv = CKR_KEY_TYPE_INCONSISTENT;
            break;
    }

    if (crv != CKR_OK) {
        goto loser;
    }

    /* For RSA-PSS, record the original algorithm parameters so
     * they can be encrypted altoghether when wrapping */
    if (SECOID_GetAlgorithmTag(&pki->algorithm) == SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {
        NSSLOWKEYSubjectPublicKeyInfo spki;
        NSSLOWKEYPublicKey pubk;
        SECItem *publicKeyInfo;

        memset(&spki, 0, sizeof(NSSLOWKEYSubjectPublicKeyInfo));
        rv = SECOID_CopyAlgorithmID(arena, &spki.algorithm, &pki->algorithm);
        if (rv != SECSuccess) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }

        prepare_low_rsa_pub_key_for_asn1(&pubk);

        rv = SECITEM_CopyItem(arena, &pubk.u.rsa.modulus, &lpk->u.rsa.modulus);
        if (rv != SECSuccess) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        rv = SECITEM_CopyItem(arena, &pubk.u.rsa.publicExponent, &lpk->u.rsa.publicExponent);
        if (rv != SECSuccess) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }

        if (SEC_ASN1EncodeItem(arena, &spki.subjectPublicKey,
                               &pubk, nsslowkey_RSAPublicKeyTemplate) == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }

        publicKeyInfo = SEC_ASN1EncodeItem(arena, NULL,
                                           &spki, nsslowkey_SubjectPublicKeyInfoTemplate);
        if (!publicKeyInfo) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        crv = sftk_AddAttributeType(key, CKA_PUBLIC_KEY_INFO,
                                    sftk_item_expand(publicKeyInfo));
    }

loser:
    if (lpk) {
        nsslowkey_DestroyPrivateKey(lpk);
    }

    if (crv != CKR_OK) {
        return SECFailure;
    }

    return SECSuccess;
}

/* NSC_UnwrapKey unwraps (decrypts) a wrapped key, creating a new key object. */
CK_RV
NSC_UnwrapKey(CK_SESSION_HANDLE hSession,
              CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hUnwrappingKey,
              CK_BYTE_PTR pWrappedKey, CK_ULONG ulWrappedKeyLen,
              CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
              CK_OBJECT_HANDLE_PTR phKey)
{
    SFTKObject *key = NULL;
    SFTKSession *session;
    CK_ULONG key_length = 0;
    unsigned char *buf = NULL;
    CK_RV crv = CKR_OK;
    int i;
    CK_ULONG bsize = ulWrappedKeyLen;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SECItem bpki;
    CK_OBJECT_CLASS target_type = CKO_SECRET_KEY;

    CHECK_FORK();

    if (!slot) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * now lets create an object to hang the attributes off of
     */
    key = sftk_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
        return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i = 0; i < (int)ulAttributeCount; i++) {
        if (pTemplate[i].type == CKA_VALUE_LEN) {
            key_length = *(CK_ULONG *)pTemplate[i].pValue;
            continue;
        }
        if (pTemplate[i].type == CKA_CLASS) {
            target_type = *(CK_OBJECT_CLASS *)pTemplate[i].pValue;
        }
        crv = sftk_AddAttributeType(key, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK)
            break;
    }
    if (crv != CKR_OK) {
        sftk_FreeObject(key);
        return crv;
    }

    crv = sftk_CryptInit(hSession, pMechanism, hUnwrappingKey, CKA_UNWRAP,
                         CKA_UNWRAP, SFTK_DECRYPT, PR_FALSE);
    if (crv != CKR_OK) {
        sftk_FreeObject(key);
        return sftk_mapWrap(crv);
    }

    /* allocate the buffer to decrypt into
     * this assumes the unwrapped key is never larger than the
     * wrapped key. For all the mechanisms we support this is true */
    buf = (unsigned char *)PORT_Alloc(ulWrappedKeyLen);
    bsize = ulWrappedKeyLen;

    crv = NSC_Decrypt(hSession, pWrappedKey, ulWrappedKeyLen, buf, &bsize);
    if (crv != CKR_OK) {
        sftk_FreeObject(key);
        PORT_Free(buf);
        return sftk_mapWrap(crv);
    }

    switch (target_type) {
        case CKO_SECRET_KEY:
            if (!sftk_hasAttribute(key, CKA_KEY_TYPE)) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                break;
            }

            if (key_length == 0 || key_length > bsize) {
                key_length = bsize;
            }
            if (key_length > MAX_KEY_LEN) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }

            /* add the value */
            crv = sftk_AddAttributeType(key, CKA_VALUE, buf, key_length);
            break;
        case CKO_PRIVATE_KEY:
            bpki.data = (unsigned char *)buf;
            bpki.len = bsize;
            crv = CKR_OK;
            if (sftk_unwrapPrivateKey(key, &bpki) != SECSuccess) {
                crv = CKR_TEMPLATE_INCOMPLETE;
            }
            break;
        default:
            crv = CKR_TEMPLATE_INCONSISTENT;
            break;
    }

    PORT_ZFree(buf, bsize);
    if (crv != CKR_OK) {
        sftk_FreeObject(key);
        return crv;
    }

    /* get the session */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        sftk_FreeObject(key);
        return CKR_SESSION_HANDLE_INVALID;
    }

    /*
     * handle the base object stuff
     */
    crv = sftk_handleObject(key, session);
    *phKey = key->handle;
    sftk_FreeSession(session);
    sftk_FreeObject(key);

    return crv;
}

/*
 * The SSL key gen mechanism create's lots of keys. This function handles the
 * details of each of these key creation.
 */
static CK_RV
sftk_buildSSLKey(CK_SESSION_HANDLE hSession, SFTKObject *baseKey,
                 PRBool isMacKey, unsigned char *keyBlock, unsigned int keySize,
                 CK_OBJECT_HANDLE *keyHandle)
{
    SFTKObject *key;
    SFTKSession *session;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_RV crv = CKR_HOST_MEMORY;

    /*
     * now lets create an object to hang the attributes off of
     */
    *keyHandle = CK_INVALID_HANDLE;
    key = sftk_NewObject(baseKey->slot);
    if (key == NULL)
        return CKR_HOST_MEMORY;
    sftk_narrowToSessionObject(key)->wasDerived = PR_TRUE;

    crv = sftk_CopyObject(key, baseKey);
    if (crv != CKR_OK)
        goto loser;
    if (isMacKey) {
        crv = sftk_forceAttribute(key, CKA_KEY_TYPE, &keyType, sizeof(keyType));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_DERIVE, &cktrue, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_ENCRYPT, &ckfalse, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_DECRYPT, &ckfalse, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_SIGN, &cktrue, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_WRAP, &ckfalse, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
        crv = sftk_forceAttribute(key, CKA_UNWRAP, &ckfalse, sizeof(CK_BBOOL));
        if (crv != CKR_OK)
            goto loser;
    }
    crv = sftk_forceAttribute(key, CKA_VALUE, keyBlock, keySize);
    if (crv != CKR_OK)
        goto loser;

    /* get the session */
    crv = CKR_HOST_MEMORY;
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        goto loser;
    }

    crv = sftk_handleObject(key, session);
    sftk_FreeSession(session);
    *keyHandle = key->handle;
loser:
    if (key)
        sftk_FreeObject(key);
    return crv;
}

/*
 * if there is an error, we need to free the keys we already created in SSL
 * This is the routine that will do it..
 */
static void
sftk_freeSSLKeys(CK_SESSION_HANDLE session,
                 CK_SSL3_KEY_MAT_OUT *returnedMaterial)
{
    if (returnedMaterial->hClientMacSecret != CK_INVALID_HANDLE) {
        NSC_DestroyObject(session, returnedMaterial->hClientMacSecret);
    }
    if (returnedMaterial->hServerMacSecret != CK_INVALID_HANDLE) {
        NSC_DestroyObject(session, returnedMaterial->hServerMacSecret);
    }
    if (returnedMaterial->hClientKey != CK_INVALID_HANDLE) {
        NSC_DestroyObject(session, returnedMaterial->hClientKey);
    }
    if (returnedMaterial->hServerKey != CK_INVALID_HANDLE) {
        NSC_DestroyObject(session, returnedMaterial->hServerKey);
    }
}

/*
 * when deriving from sensitive and extractable keys, we need to preserve some
 * of the semantics in the derived key. This helper routine maintains these
 * semantics.
 */
static CK_RV
sftk_DeriveSensitiveCheck(SFTKObject *baseKey, SFTKObject *destKey)
{
    PRBool hasSensitive;
    PRBool sensitive = PR_FALSE;
    PRBool hasExtractable;
    PRBool extractable = PR_TRUE;
    CK_RV crv = CKR_OK;
    SFTKAttribute *att;

    hasSensitive = PR_FALSE;
    att = sftk_FindAttribute(destKey, CKA_SENSITIVE);
    if (att) {
        hasSensitive = PR_TRUE;
        sensitive = (PRBool) * (CK_BBOOL *)att->attrib.pValue;
        sftk_FreeAttribute(att);
    }

    hasExtractable = PR_FALSE;
    att = sftk_FindAttribute(destKey, CKA_EXTRACTABLE);
    if (att) {
        hasExtractable = PR_TRUE;
        extractable = (PRBool) * (CK_BBOOL *)att->attrib.pValue;
        sftk_FreeAttribute(att);
    }

    /* don't make a key more accessible */
    if (sftk_isTrue(baseKey, CKA_SENSITIVE) && hasSensitive &&
        (sensitive == PR_FALSE)) {
        return CKR_KEY_FUNCTION_NOT_PERMITTED;
    }
    if (!sftk_isTrue(baseKey, CKA_EXTRACTABLE) && hasExtractable &&
        (extractable == PR_TRUE)) {
        return CKR_KEY_FUNCTION_NOT_PERMITTED;
    }

    /* inherit parent's sensitivity */
    if (!hasSensitive) {
        att = sftk_FindAttribute(baseKey, CKA_SENSITIVE);
        if (att == NULL)
            return CKR_KEY_TYPE_INCONSISTENT;
        crv = sftk_defaultAttribute(destKey, sftk_attr_expand(&att->attrib));
        sftk_FreeAttribute(att);
        if (crv != CKR_OK)
            return crv;
    }
    if (!hasExtractable) {
        att = sftk_FindAttribute(baseKey, CKA_EXTRACTABLE);
        if (att == NULL)
            return CKR_KEY_TYPE_INCONSISTENT;
        crv = sftk_defaultAttribute(destKey, sftk_attr_expand(&att->attrib));
        sftk_FreeAttribute(att);
        if (crv != CKR_OK)
            return crv;
    }

    /* we should inherit the parent's always extractable/ never sensitive info,
     * but handleObject always forces this attributes, so we would need to do
     * something special. */
    return CKR_OK;
}

/*
 * make known fixed PKCS #11 key types to their sizes in bytes
 */
unsigned long
sftk_MapKeySize(CK_KEY_TYPE keyType)
{
    switch (keyType) {
        case CKK_CDMF:
            return 8;
        case CKK_DES:
            return 8;
        case CKK_DES2:
            return 16;
        case CKK_DES3:
            return 24;
        /* IDEA and CAST need to be added */
        default:
            break;
    }
    return 0;
}

/* Inputs:
 *  key_len: Length of derived key to be generated.
 *  SharedSecret: a shared secret that is the output of a key agreement primitive.
 *  SharedInfo: (Optional) some data shared by the entities computing the secret key.
 *  SharedInfoLen: the length in octets of SharedInfo
 *  Hash: The hash function to be used in the KDF
 *  HashLen: the length in octets of the output of Hash
 * Output:
 *  key: Pointer to a buffer containing derived key, if return value is SECSuccess.
 */
static CK_RV
sftk_compute_ANSI_X9_63_kdf(CK_BYTE **key, CK_ULONG key_len, SECItem *SharedSecret,
                            CK_BYTE_PTR SharedInfo, CK_ULONG SharedInfoLen,
                            SECStatus Hash(unsigned char *, const unsigned char *, PRUint32),
                            CK_ULONG HashLen)
{
    unsigned char *buffer = NULL, *output_buffer = NULL;
    PRUint32 buffer_len, max_counter, i;
    SECStatus rv;
    CK_RV crv;

    /* Check that key_len isn't too long.  The maximum key length could be
     * greatly increased if the code below did not limit the 4-byte counter
     * to a maximum value of 255. */
    if (key_len > 254 * HashLen)
        return CKR_ARGUMENTS_BAD;

    if (SharedInfo == NULL)
        SharedInfoLen = 0;

    buffer_len = SharedSecret->len + 4 + SharedInfoLen;
    buffer = (CK_BYTE *)PORT_Alloc(buffer_len);
    if (buffer == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }

    max_counter = key_len / HashLen;
    if (key_len > max_counter * HashLen)
        max_counter++;

    output_buffer = (CK_BYTE *)PORT_Alloc(max_counter * HashLen);
    if (output_buffer == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }

    /* Populate buffer with SharedSecret || Counter || [SharedInfo]
     * where Counter is 0x00000001 */
    PORT_Memcpy(buffer, SharedSecret->data, SharedSecret->len);
    buffer[SharedSecret->len] = 0;
    buffer[SharedSecret->len + 1] = 0;
    buffer[SharedSecret->len + 2] = 0;
    buffer[SharedSecret->len + 3] = 1;
    if (SharedInfo) {
        PORT_Memcpy(&buffer[SharedSecret->len + 4], SharedInfo, SharedInfoLen);
    }

    for (i = 0; i < max_counter; i++) {
        rv = Hash(&output_buffer[i * HashLen], buffer, buffer_len);
        if (rv != SECSuccess) {
            /* 'Hash' should not fail. */
            crv = CKR_FUNCTION_FAILED;
            goto loser;
        }

        /* Increment counter (assumes max_counter < 255) */
        buffer[SharedSecret->len + 3]++;
    }

    PORT_ZFree(buffer, buffer_len);
    if (key_len < max_counter * HashLen) {
        PORT_Memset(output_buffer + key_len, 0, max_counter * HashLen - key_len);
    }
    *key = output_buffer;

    return CKR_OK;

loser:
    if (buffer) {
        PORT_ZFree(buffer, buffer_len);
    }
    if (output_buffer) {
        PORT_ZFree(output_buffer, max_counter * HashLen);
    }
    return crv;
}

static CK_RV
sftk_ANSI_X9_63_kdf(CK_BYTE **key, CK_ULONG key_len,
                    SECItem *SharedSecret,
                    CK_BYTE_PTR SharedInfo, CK_ULONG SharedInfoLen,
                    CK_EC_KDF_TYPE kdf)
{
    if (kdf == CKD_SHA1_KDF)
        return sftk_compute_ANSI_X9_63_kdf(key, key_len, SharedSecret, SharedInfo,
                                           SharedInfoLen, SHA1_HashBuf, SHA1_LENGTH);
    else if (kdf == CKD_SHA224_KDF)
        return sftk_compute_ANSI_X9_63_kdf(key, key_len, SharedSecret, SharedInfo,
                                           SharedInfoLen, SHA224_HashBuf, SHA224_LENGTH);
    else if (kdf == CKD_SHA256_KDF)
        return sftk_compute_ANSI_X9_63_kdf(key, key_len, SharedSecret, SharedInfo,
                                           SharedInfoLen, SHA256_HashBuf, SHA256_LENGTH);
    else if (kdf == CKD_SHA384_KDF)
        return sftk_compute_ANSI_X9_63_kdf(key, key_len, SharedSecret, SharedInfo,
                                           SharedInfoLen, SHA384_HashBuf, SHA384_LENGTH);
    else if (kdf == CKD_SHA512_KDF)
        return sftk_compute_ANSI_X9_63_kdf(key, key_len, SharedSecret, SharedInfo,
                                           SharedInfoLen, SHA512_HashBuf, SHA512_LENGTH);
    else
        return CKR_MECHANISM_INVALID;
}

/*
 *  Handle the derive from a block encryption cipher
 */
CK_RV
sftk_DeriveEncrypt(SFTKCipher encrypt, void *cipherInfo,
                   int blockSize, SFTKObject *key, CK_ULONG keySize,
                   unsigned char *data, CK_ULONG len)
{
    /* large enough for a 512-bit key */
    unsigned char tmpdata[SFTK_MAX_DERIVE_KEY_SIZE];
    SECStatus rv;
    unsigned int outLen;
    CK_RV crv;

    if ((len % blockSize) != 0) {
        return CKR_MECHANISM_PARAM_INVALID;
    }
    if (len > SFTK_MAX_DERIVE_KEY_SIZE) {
        return CKR_MECHANISM_PARAM_INVALID;
    }
    if (keySize && (len < keySize)) {
        return CKR_MECHANISM_PARAM_INVALID;
    }
    if (keySize == 0) {
        keySize = len;
    }

    rv = (*encrypt)(cipherInfo, &tmpdata, &outLen, len, data, len);
    if (rv != SECSuccess) {
        crv = sftk_MapCryptError(PORT_GetError());
        return crv;
    }

    crv = sftk_forceAttribute(key, CKA_VALUE, tmpdata, keySize);
    return crv;
}

/*
 * SSL Key generation given pre master secret
 */
#define NUM_MIXERS 9
static const char *const mixers[NUM_MIXERS] = {
    "A",
    "BB",
    "CCC",
    "DDDD",
    "EEEEE",
    "FFFFFF",
    "GGGGGGG",
    "HHHHHHHH",
    "IIIIIIIII"
};
#define SSL3_PMS_LENGTH 48
#define SSL3_MASTER_SECRET_LENGTH 48
#define SSL3_RANDOM_LENGTH 32

/* NSC_DeriveKey derives a key from a base key, creating a new key object. */
CK_RV
NSC_DeriveKey(CK_SESSION_HANDLE hSession,
              CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hBaseKey,
              CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
              CK_OBJECT_HANDLE_PTR phKey)
{
    SFTKSession *session;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SFTKObject *key;
    SFTKObject *sourceKey;
    SFTKAttribute *att = NULL;
    SFTKAttribute *att2 = NULL;
    unsigned char *buf;
    SHA1Context *sha;
    MD5Context *md5;
    MD2Context *md2;
    CK_ULONG macSize;
    CK_ULONG tmpKeySize;
    CK_ULONG IVSize;
    CK_ULONG keySize = 0;
    CK_RV crv = CKR_OK;
    CK_BBOOL cktrue = CK_TRUE;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_OBJECT_CLASS classType = CKO_SECRET_KEY;
    CK_KEY_DERIVATION_STRING_DATA *stringPtr;
    PRBool isTLS = PR_FALSE;
    PRBool isDH = PR_FALSE;
    HASH_HashType tlsPrfHash = HASH_AlgNULL;
    SECStatus rv;
    int i;
    unsigned int outLen;
    unsigned char sha_out[SHA1_LENGTH];
    unsigned char key_block[NUM_MIXERS * SFTK_MAX_MAC_LENGTH];
    PRBool isFIPS;
    HASH_HashType hashType;
    PRBool extractValue = PR_TRUE;

    CHECK_FORK();

    if (!slot) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    if (!pMechanism) {
        return CKR_MECHANISM_PARAM_INVALID;
    }
    CK_MECHANISM_TYPE mechanism = pMechanism->mechanism;

    /*
     * now lets create an object to hang the attributes off of
     */
    if (phKey)
        *phKey = CK_INVALID_HANDLE;

    key = sftk_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
        return CKR_HOST_MEMORY;
    }
    isFIPS = (slot->slotID == FIPS_SLOT_ID);

    /*
     * load the template values into the object
     */
    for (i = 0; i < (int)ulAttributeCount; i++) {
        crv = sftk_AddAttributeType(key, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK)
            break;

        if (pTemplate[i].type == CKA_KEY_TYPE) {
            keyType = *(CK_KEY_TYPE *)pTemplate[i].pValue;
        }
        if (pTemplate[i].type == CKA_VALUE_LEN) {
            keySize = *(CK_ULONG *)pTemplate[i].pValue;
        }
    }
    if (crv != CKR_OK) {
        sftk_FreeObject(key);
        return crv;
    }

    if (keySize == 0) {
        keySize = sftk_MapKeySize(keyType);
    }

    switch (mechanism) {
        case CKM_NSS_JPAKE_ROUND2_SHA1:   /* fall through */
        case CKM_NSS_JPAKE_ROUND2_SHA256: /* fall through */
        case CKM_NSS_JPAKE_ROUND2_SHA384: /* fall through */
        case CKM_NSS_JPAKE_ROUND2_SHA512:
            extractValue = PR_FALSE;
            classType = CKO_PRIVATE_KEY;
            break;
        case CKM_NSS_PUB_FROM_PRIV:
            extractValue = PR_FALSE;
            classType = CKO_PUBLIC_KEY;
            break;
        case CKM_NSS_JPAKE_FINAL_SHA1:   /* fall through */
        case CKM_NSS_JPAKE_FINAL_SHA256: /* fall through */
        case CKM_NSS_JPAKE_FINAL_SHA384: /* fall through */
        case CKM_NSS_JPAKE_FINAL_SHA512:
            extractValue = PR_FALSE;
        /* fall through */
        default:
            classType = CKO_SECRET_KEY;
    }

    crv = sftk_forceAttribute(key, CKA_CLASS, &classType, sizeof(classType));
    if (crv != CKR_OK) {
        sftk_FreeObject(key);
        return crv;
    }

    /* look up the base key we're deriving with */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        sftk_FreeObject(key);
        return CKR_SESSION_HANDLE_INVALID;
    }

    sourceKey = sftk_ObjectFromHandle(hBaseKey, session);
    sftk_FreeSession(session);
    if (sourceKey == NULL) {
        sftk_FreeObject(key);
        return CKR_KEY_HANDLE_INVALID;
    }

    if (extractValue) {
        /* get the value of the base key */
        att = sftk_FindAttribute(sourceKey, CKA_VALUE);
        if (att == NULL) {
            sftk_FreeObject(key);
            sftk_FreeObject(sourceKey);
            return CKR_KEY_HANDLE_INVALID;
        }
    }

    switch (mechanism) {
        /* get a public key from a private key. nsslowkey_ConvertToPublickey()
         * will generate the public portion if it doesn't already exist. */
        case CKM_NSS_PUB_FROM_PRIV: {
            NSSLOWKEYPrivateKey *privKey;
            NSSLOWKEYPublicKey *pubKey;
            int error;

            crv = sftk_GetULongAttribute(sourceKey, CKA_KEY_TYPE, &keyType);
            if (crv != CKR_OK) {
                break;
            }

            /* privKey is stored in sourceKey and will be destroyed when
             * the sourceKey is freed. */
            privKey = sftk_GetPrivKey(sourceKey, keyType, &crv);
            if (privKey == NULL) {
                break;
            }
            pubKey = nsslowkey_ConvertToPublicKey(privKey);
            if (pubKey == NULL) {
                error = PORT_GetError();
                crv = sftk_MapCryptError(error);
                break;
            }
            crv = sftk_PutPubKey(key, sourceKey, keyType, pubKey);
            nsslowkey_DestroyPublicKey(pubKey);
            break;
        }
        case CKM_NSS_IKE_PRF_DERIVE:
            if (pMechanism->ulParameterLen !=
                sizeof(CK_NSS_IKE_PRF_DERIVE_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_ike_prf(hSession, att,
                               (CK_NSS_IKE_PRF_DERIVE_PARAMS *)pMechanism->pParameter, key);
            break;
        case CKM_NSS_IKE1_PRF_DERIVE:
            if (pMechanism->ulParameterLen !=
                sizeof(CK_NSS_IKE1_PRF_DERIVE_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_ike1_prf(hSession, att,
                                (CK_NSS_IKE1_PRF_DERIVE_PARAMS *)pMechanism->pParameter,
                                key, keySize);
            break;
        case CKM_NSS_IKE1_APP_B_PRF_DERIVE:
            if (pMechanism->ulParameterLen !=
                sizeof(CK_MECHANISM_TYPE)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_ike1_appendix_b_prf(hSession, att,
                                           (CK_MECHANISM_TYPE *)pMechanism->pParameter,
                                           key, keySize);
            break;
        case CKM_NSS_IKE_PRF_PLUS_DERIVE:
            if (pMechanism->ulParameterLen !=
                sizeof(CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            crv = sftk_ike_prf_plus(hSession, att,
                                    (CK_NSS_IKE_PRF_PLUS_DERIVE_PARAMS *)pMechanism->pParameter,
                                    key, keySize);
            break;
        /*
         * generate the master secret
         */
        case CKM_TLS12_MASTER_KEY_DERIVE:
        case CKM_TLS12_MASTER_KEY_DERIVE_DH:
        case CKM_NSS_TLS_MASTER_KEY_DERIVE_SHA256:
        case CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256:
        case CKM_TLS_MASTER_KEY_DERIVE:
        case CKM_TLS_MASTER_KEY_DERIVE_DH:
        case CKM_SSL3_MASTER_KEY_DERIVE:
        case CKM_SSL3_MASTER_KEY_DERIVE_DH: {
            CK_SSL3_MASTER_KEY_DERIVE_PARAMS *ssl3_master;
            SSL3RSAPreMasterSecret *rsa_pms;
            unsigned char crsrdata[SSL3_RANDOM_LENGTH * 2];

            if ((mechanism == CKM_TLS12_MASTER_KEY_DERIVE) ||
                (mechanism == CKM_TLS12_MASTER_KEY_DERIVE_DH)) {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_TLS12_MASTER_KEY_DERIVE_PARAMS))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                CK_TLS12_MASTER_KEY_DERIVE_PARAMS *tls12_master =
                    (CK_TLS12_MASTER_KEY_DERIVE_PARAMS *)pMechanism->pParameter;
                tlsPrfHash = GetHashTypeFromMechanism(tls12_master->prfHashMechanism);
                if (tlsPrfHash == HASH_AlgNULL) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
            } else if ((mechanism == CKM_NSS_TLS_MASTER_KEY_DERIVE_SHA256) ||
                       (mechanism == CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256)) {
                tlsPrfHash = HASH_AlgSHA256;
            }

            if ((mechanism != CKM_SSL3_MASTER_KEY_DERIVE) &&
                (mechanism != CKM_SSL3_MASTER_KEY_DERIVE_DH)) {
                isTLS = PR_TRUE;
            }
            if ((mechanism == CKM_SSL3_MASTER_KEY_DERIVE_DH) ||
                (mechanism == CKM_TLS_MASTER_KEY_DERIVE_DH) ||
                (mechanism == CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256) ||
                (mechanism == CKM_TLS12_MASTER_KEY_DERIVE_DH)) {
                isDH = PR_TRUE;
            }

            /* first do the consistency checks */
            if (!isDH && (att->attrib.ulValueLen != SSL3_PMS_LENGTH)) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att2 = sftk_FindAttribute(sourceKey, CKA_KEY_TYPE);
            if ((att2 == NULL) || (*(CK_KEY_TYPE *)att2->attrib.pValue !=
                                   CKK_GENERIC_SECRET)) {
                if (att2)
                    sftk_FreeAttribute(att2);
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }
            sftk_FreeAttribute(att2);
            if (keyType != CKK_GENERIC_SECRET) {
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }
            if ((keySize != 0) && (keySize != SSL3_MASTER_SECRET_LENGTH)) {
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }

            /* finally do the key gen */
            ssl3_master = (CK_SSL3_MASTER_KEY_DERIVE_PARAMS *)
                              pMechanism->pParameter;

            PORT_Memcpy(crsrdata,
                        ssl3_master->RandomInfo.pClientRandom, SSL3_RANDOM_LENGTH);
            PORT_Memcpy(crsrdata + SSL3_RANDOM_LENGTH,
                        ssl3_master->RandomInfo.pServerRandom, SSL3_RANDOM_LENGTH);

            if (ssl3_master->pVersion) {
                SFTKSessionObject *sessKey = sftk_narrowToSessionObject(key);
                rsa_pms = (SSL3RSAPreMasterSecret *)att->attrib.pValue;
                /* don't leak more key material then necessary for SSL to work */
                if ((sessKey == NULL) || sessKey->wasDerived) {
                    ssl3_master->pVersion->major = 0xff;
                    ssl3_master->pVersion->minor = 0xff;
                } else {
                    ssl3_master->pVersion->major = rsa_pms->client_version[0];
                    ssl3_master->pVersion->minor = rsa_pms->client_version[1];
                }
            }
            if (ssl3_master->RandomInfo.ulClientRandomLen != SSL3_RANDOM_LENGTH) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            if (ssl3_master->RandomInfo.ulServerRandomLen != SSL3_RANDOM_LENGTH) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }

            if (isTLS) {
                SECStatus status;
                SECItem crsr = { siBuffer, NULL, 0 };
                SECItem master = { siBuffer, NULL, 0 };
                SECItem pms = { siBuffer, NULL, 0 };

                crsr.data = crsrdata;
                crsr.len = sizeof crsrdata;
                master.data = key_block;
                master.len = SSL3_MASTER_SECRET_LENGTH;
                pms.data = (unsigned char *)att->attrib.pValue;
                pms.len = att->attrib.ulValueLen;

                if (tlsPrfHash != HASH_AlgNULL) {
                    status = TLS_P_hash(tlsPrfHash, &pms, "master secret",
                                        &crsr, &master, isFIPS);
                } else {
                    status = TLS_PRF(&pms, "master secret", &crsr, &master, isFIPS);
                }
                if (status != SECSuccess) {
                    crv = CKR_FUNCTION_FAILED;
                    break;
                }
            } else {
                /* now allocate the hash contexts */
                md5 = MD5_NewContext();
                if (md5 == NULL) {
                    crv = CKR_HOST_MEMORY;
                    break;
                }
                sha = SHA1_NewContext();
                if (sha == NULL) {
                    PORT_Free(md5);
                    crv = CKR_HOST_MEMORY;
                    break;
                }
                for (i = 0; i < 3; i++) {
                    SHA1_Begin(sha);
                    SHA1_Update(sha, (unsigned char *)mixers[i], strlen(mixers[i]));
                    SHA1_Update(sha, (const unsigned char *)att->attrib.pValue,
                                att->attrib.ulValueLen);
                    SHA1_Update(sha, crsrdata, sizeof crsrdata);
                    SHA1_End(sha, sha_out, &outLen, SHA1_LENGTH);
                    PORT_Assert(outLen == SHA1_LENGTH);

                    MD5_Begin(md5);
                    MD5_Update(md5, (const unsigned char *)att->attrib.pValue,
                               att->attrib.ulValueLen);
                    MD5_Update(md5, sha_out, outLen);
                    MD5_End(md5, &key_block[i * MD5_LENGTH], &outLen, MD5_LENGTH);
                    PORT_Assert(outLen == MD5_LENGTH);
                }
                PORT_Free(md5);
                PORT_Free(sha);
            }

            /* store the results */
            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, SSL3_MASTER_SECRET_LENGTH);
            if (crv != CKR_OK)
                break;
            keyType = CKK_GENERIC_SECRET;
            crv = sftk_forceAttribute(key, CKA_KEY_TYPE, &keyType, sizeof(keyType));
            if (isTLS) {
                /* TLS's master secret is used to "sign" finished msgs with PRF. */
                /* XXX This seems like a hack.   But SFTK_Derive only accepts
                 * one "operation" argument. */
                crv = sftk_forceAttribute(key, CKA_SIGN, &cktrue, sizeof(CK_BBOOL));
                if (crv != CKR_OK)
                    break;
                crv = sftk_forceAttribute(key, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));
                if (crv != CKR_OK)
                    break;
                /* While we're here, we might as well force this, too. */
                crv = sftk_forceAttribute(key, CKA_DERIVE, &cktrue, sizeof(CK_BBOOL));
                if (crv != CKR_OK)
                    break;
            }
            break;
        }

        /* Extended master key derivation [draft-ietf-tls-session-hash] */
        case CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE:
        case CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_DH: {
            CK_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_PARAMS *ems_params;
            SSL3RSAPreMasterSecret *rsa_pms;
            SECStatus status;
            SECItem pms = { siBuffer, NULL, 0 };
            SECItem seed = { siBuffer, NULL, 0 };
            SECItem master = { siBuffer, NULL, 0 };

            ems_params = (CK_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_PARAMS *)
                             pMechanism->pParameter;

            /* First do the consistency checks */
            if ((mechanism == CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE) &&
                (att->attrib.ulValueLen != SSL3_PMS_LENGTH)) {
                crv = CKR_KEY_TYPE_INCONSISTENT;
                break;
            }
            att2 = sftk_FindAttribute(sourceKey, CKA_KEY_TYPE);
            if ((att2 == NULL) ||
                (*(CK_KEY_TYPE *)att2->attrib.pValue != CKK_GENERIC_SECRET)) {
                if (att2)
                    sftk_FreeAttribute(att2);
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }
            sftk_FreeAttribute(att2);
            if (keyType != CKK_GENERIC_SECRET) {
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }
            if ((keySize != 0) && (keySize != SSL3_MASTER_SECRET_LENGTH)) {
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }

            /* Do the key derivation */
            pms.data = (unsigned char *)att->attrib.pValue;
            pms.len = att->attrib.ulValueLen;
            seed.data = ems_params->pSessionHash;
            seed.len = ems_params->ulSessionHashLen;
            master.data = key_block;
            master.len = SSL3_MASTER_SECRET_LENGTH;
            if (ems_params->prfHashMechanism == CKM_TLS_PRF) {
                /*
                 * In this case, the session hash is the concatenation of SHA-1
                 * and MD5, so it should be 36 bytes long.
                 */
                if (seed.len != MD5_LENGTH + SHA1_LENGTH) {
                    crv = CKR_TEMPLATE_INCONSISTENT;
                    break;
                }

                status = TLS_PRF(&pms, "extended master secret",
                                 &seed, &master, isFIPS);
            } else {
                const SECHashObject *hashObj;

                tlsPrfHash = GetHashTypeFromMechanism(ems_params->prfHashMechanism);
                if (tlsPrfHash == HASH_AlgNULL) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }

                hashObj = HASH_GetRawHashObject(tlsPrfHash);
                if (seed.len != hashObj->length) {
                    crv = CKR_TEMPLATE_INCONSISTENT;
                    break;
                }

                status = TLS_P_hash(tlsPrfHash, &pms, "extended master secret",
                                    &seed, &master, isFIPS);
            }
            if (status != SECSuccess) {
                crv = CKR_FUNCTION_FAILED;
                break;
            }

            /* Reflect the version if required */
            if (ems_params->pVersion) {
                SFTKSessionObject *sessKey = sftk_narrowToSessionObject(key);
                rsa_pms = (SSL3RSAPreMasterSecret *)att->attrib.pValue;
                /* don't leak more key material than necessary for SSL to work */
                if ((sessKey == NULL) || sessKey->wasDerived) {
                    ems_params->pVersion->major = 0xff;
                    ems_params->pVersion->minor = 0xff;
                } else {
                    ems_params->pVersion->major = rsa_pms->client_version[0];
                    ems_params->pVersion->minor = rsa_pms->client_version[1];
                }
            }

            /* Store the results */
            crv = sftk_forceAttribute(key, CKA_VALUE, key_block,
                                      SSL3_MASTER_SECRET_LENGTH);
            break;
        }

        case CKM_TLS12_KEY_AND_MAC_DERIVE:
        case CKM_NSS_TLS_KEY_AND_MAC_DERIVE_SHA256:
        case CKM_TLS_KEY_AND_MAC_DERIVE:
        case CKM_SSL3_KEY_AND_MAC_DERIVE: {
            CK_SSL3_KEY_MAT_PARAMS *ssl3_keys;
            CK_SSL3_KEY_MAT_OUT *ssl3_keys_out;
            CK_ULONG effKeySize;
            unsigned int block_needed;
            unsigned char srcrdata[SSL3_RANDOM_LENGTH * 2];
            unsigned char crsrdata[SSL3_RANDOM_LENGTH * 2];

            if (mechanism == CKM_TLS12_KEY_AND_MAC_DERIVE) {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_TLS12_KEY_MAT_PARAMS))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                CK_TLS12_KEY_MAT_PARAMS *tls12_keys =
                    (CK_TLS12_KEY_MAT_PARAMS *)pMechanism->pParameter;
                tlsPrfHash = GetHashTypeFromMechanism(tls12_keys->prfHashMechanism);
                if (tlsPrfHash == HASH_AlgNULL) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
            } else if (mechanism == CKM_NSS_TLS_KEY_AND_MAC_DERIVE_SHA256) {
                tlsPrfHash = HASH_AlgSHA256;
            }

            if (mechanism != CKM_SSL3_KEY_AND_MAC_DERIVE) {
                isTLS = PR_TRUE;
            }

            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            if (att->attrib.ulValueLen != SSL3_MASTER_SECRET_LENGTH) {
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }
            att2 = sftk_FindAttribute(sourceKey, CKA_KEY_TYPE);
            if ((att2 == NULL) || (*(CK_KEY_TYPE *)att2->attrib.pValue !=
                                   CKK_GENERIC_SECRET)) {
                if (att2)
                    sftk_FreeAttribute(att2);
                crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
                break;
            }
            sftk_FreeAttribute(att2);
            md5 = MD5_NewContext();
            if (md5 == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            sha = SHA1_NewContext();
            if (sha == NULL) {
                MD5_DestroyContext(md5, PR_TRUE);
                crv = CKR_HOST_MEMORY;
                break;
            }

            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_SSL3_KEY_MAT_PARAMS))) {
                MD5_DestroyContext(md5, PR_TRUE);
                SHA1_DestroyContext(sha, PR_TRUE);
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            ssl3_keys = (CK_SSL3_KEY_MAT_PARAMS *)pMechanism->pParameter;

            PORT_Memcpy(srcrdata,
                        ssl3_keys->RandomInfo.pServerRandom, SSL3_RANDOM_LENGTH);
            PORT_Memcpy(srcrdata + SSL3_RANDOM_LENGTH,
                        ssl3_keys->RandomInfo.pClientRandom, SSL3_RANDOM_LENGTH);

            PORT_Memcpy(crsrdata,
                        ssl3_keys->RandomInfo.pClientRandom, SSL3_RANDOM_LENGTH);
            PORT_Memcpy(crsrdata + SSL3_RANDOM_LENGTH,
                        ssl3_keys->RandomInfo.pServerRandom, SSL3_RANDOM_LENGTH);

            /*
             * clear out our returned keys so we can recover on failure
             */
            ssl3_keys_out = ssl3_keys->pReturnedKeyMaterial;
            ssl3_keys_out->hClientMacSecret = CK_INVALID_HANDLE;
            ssl3_keys_out->hServerMacSecret = CK_INVALID_HANDLE;
            ssl3_keys_out->hClientKey = CK_INVALID_HANDLE;
            ssl3_keys_out->hServerKey = CK_INVALID_HANDLE;

            /*
             * How much key material do we need?
             */
            macSize = ssl3_keys->ulMacSizeInBits / 8;
            effKeySize = ssl3_keys->ulKeySizeInBits / 8;
            IVSize = ssl3_keys->ulIVSizeInBits / 8;
            if (keySize == 0) {
                effKeySize = keySize;
            }

            /* bIsExport must be false. */
            if (ssl3_keys->bIsExport) {
                MD5_DestroyContext(md5, PR_TRUE);
                SHA1_DestroyContext(sha, PR_TRUE);
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }

            block_needed = 2 * (macSize + effKeySize + IVSize);
            PORT_Assert(block_needed <= sizeof key_block);
            if (block_needed > sizeof key_block)
                block_needed = sizeof key_block;

            /*
             * generate the key material: This looks amazingly similar to the
             * PMS code, and is clearly crying out for a function to provide it.
             */
            if (isTLS) {
                SECStatus status;
                SECItem srcr = { siBuffer, NULL, 0 };
                SECItem keyblk = { siBuffer, NULL, 0 };
                SECItem master = { siBuffer, NULL, 0 };

                srcr.data = srcrdata;
                srcr.len = sizeof srcrdata;
                keyblk.data = key_block;
                keyblk.len = block_needed;
                master.data = (unsigned char *)att->attrib.pValue;
                master.len = att->attrib.ulValueLen;

                if (tlsPrfHash != HASH_AlgNULL) {
                    status = TLS_P_hash(tlsPrfHash, &master, "key expansion",
                                        &srcr, &keyblk, isFIPS);
                } else {
                    status = TLS_PRF(&master, "key expansion", &srcr, &keyblk,
                                     isFIPS);
                }
                if (status != SECSuccess) {
                    goto key_and_mac_derive_fail;
                }
            } else {
                unsigned int block_bytes = 0;
                /* key_block =
                 *     MD5(master_secret + SHA('A' + master_secret +
                 *                      ServerHello.random + ClientHello.random)) +
                 *     MD5(master_secret + SHA('BB' + master_secret +
                 *                      ServerHello.random + ClientHello.random)) +
                 *     MD5(master_secret + SHA('CCC' + master_secret +
                 *                      ServerHello.random + ClientHello.random)) +
                 *     [...];
                 */
                for (i = 0; i < NUM_MIXERS && block_bytes < block_needed; i++) {
                    SHA1_Begin(sha);
                    SHA1_Update(sha, (unsigned char *)mixers[i], strlen(mixers[i]));
                    SHA1_Update(sha, (const unsigned char *)att->attrib.pValue,
                                att->attrib.ulValueLen);
                    SHA1_Update(sha, srcrdata, sizeof srcrdata);
                    SHA1_End(sha, sha_out, &outLen, SHA1_LENGTH);
                    PORT_Assert(outLen == SHA1_LENGTH);
                    MD5_Begin(md5);
                    MD5_Update(md5, (const unsigned char *)att->attrib.pValue,
                               att->attrib.ulValueLen);
                    MD5_Update(md5, sha_out, outLen);
                    MD5_End(md5, &key_block[i * MD5_LENGTH], &outLen, MD5_LENGTH);
                    PORT_Assert(outLen == MD5_LENGTH);
                    block_bytes += outLen;
                }
            }

            /*
             * Put the key material where it goes.
             */
            i = 0; /* now shows how much consumed */

            /*
             * The key_block is partitioned as follows:
             * client_write_MAC_secret[CipherSpec.hash_size]
             */
            crv = sftk_buildSSLKey(hSession, key, PR_TRUE, &key_block[i], macSize,
                                   &ssl3_keys_out->hClientMacSecret);
            if (crv != CKR_OK)
                goto key_and_mac_derive_fail;

            i += macSize;

            /*
             * server_write_MAC_secret[CipherSpec.hash_size]
             */
            crv = sftk_buildSSLKey(hSession, key, PR_TRUE, &key_block[i], macSize,
                                   &ssl3_keys_out->hServerMacSecret);
            if (crv != CKR_OK) {
                goto key_and_mac_derive_fail;
            }
            i += macSize;

            if (keySize) {
                /*
                ** Generate Domestic write keys and IVs.
                ** client_write_key[CipherSpec.key_material]
                */
                crv = sftk_buildSSLKey(hSession, key, PR_FALSE, &key_block[i],
                                       keySize, &ssl3_keys_out->hClientKey);
                if (crv != CKR_OK) {
                    goto key_and_mac_derive_fail;
                }
                i += keySize;

                /*
                ** server_write_key[CipherSpec.key_material]
                */
                crv = sftk_buildSSLKey(hSession, key, PR_FALSE, &key_block[i],
                                       keySize, &ssl3_keys_out->hServerKey);
                if (crv != CKR_OK) {
                    goto key_and_mac_derive_fail;
                }
                i += keySize;

                /*
                ** client_write_IV[CipherSpec.IV_size]
                */
                if (IVSize > 0) {
                    PORT_Memcpy(ssl3_keys_out->pIVClient,
                                &key_block[i], IVSize);
                    i += IVSize;
                }

                /*
                ** server_write_IV[CipherSpec.IV_size]
                */
                if (IVSize > 0) {
                    PORT_Memcpy(ssl3_keys_out->pIVServer,
                                &key_block[i], IVSize);
                    i += IVSize;
                }
                PORT_Assert(i <= sizeof key_block);
            }

            crv = CKR_OK;

            if (0) {
            key_and_mac_derive_fail:
                if (crv == CKR_OK)
                    crv = CKR_FUNCTION_FAILED;
                sftk_freeSSLKeys(hSession, ssl3_keys_out);
            }
            MD5_DestroyContext(md5, PR_TRUE);
            SHA1_DestroyContext(sha, PR_TRUE);
            sftk_FreeObject(key);
            key = NULL;
            break;
        }

        case CKM_DES3_ECB_ENCRYPT_DATA:
        case CKM_DES3_CBC_ENCRYPT_DATA: {
            void *cipherInfo;
            unsigned char des3key[MAX_DES3_KEY_SIZE];
            CK_DES_CBC_ENCRYPT_DATA_PARAMS *desEncryptPtr;
            int mode;
            unsigned char *iv;
            unsigned char *data;
            CK_ULONG len;

            if (mechanism == CKM_DES3_ECB_ENCRYPT_DATA) {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)
                                pMechanism->pParameter;
                mode = NSS_DES_EDE3;
                iv = NULL;
                data = stringPtr->pData;
                len = stringPtr->ulLen;
            } else {
                mode = NSS_DES_EDE3_CBC;
                desEncryptPtr =
                    (CK_DES_CBC_ENCRYPT_DATA_PARAMS *)
                        pMechanism->pParameter;
                iv = desEncryptPtr->iv;
                data = desEncryptPtr->pData;
                len = desEncryptPtr->length;
            }
            if (att->attrib.ulValueLen == 16) {
                PORT_Memcpy(des3key, att->attrib.pValue, 16);
                PORT_Memcpy(des3key + 16, des3key, 8);
            } else if (att->attrib.ulValueLen == 24) {
                PORT_Memcpy(des3key, att->attrib.pValue, 24);
            } else {
                crv = CKR_KEY_SIZE_RANGE;
                break;
            }
            cipherInfo = DES_CreateContext(des3key, iv, mode, PR_TRUE);
            PORT_Memset(des3key, 0, 24);
            if (cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            crv = sftk_DeriveEncrypt((SFTKCipher)DES_Encrypt,
                                     cipherInfo, 8, key, keySize,
                                     data, len);
            DES_DestroyContext(cipherInfo, PR_TRUE);
            break;
        }

        case CKM_AES_ECB_ENCRYPT_DATA:
        case CKM_AES_CBC_ENCRYPT_DATA: {
            void *cipherInfo;
            CK_AES_CBC_ENCRYPT_DATA_PARAMS *aesEncryptPtr;
            int mode;
            unsigned char *iv;
            unsigned char *data;
            CK_ULONG len;

            if (mechanism == CKM_AES_ECB_ENCRYPT_DATA) {
                mode = NSS_AES;
                iv = NULL;
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)pMechanism->pParameter;
                data = stringPtr->pData;
                len = stringPtr->ulLen;
            } else {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_AES_CBC_ENCRYPT_DATA_PARAMS))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                aesEncryptPtr =
                    (CK_AES_CBC_ENCRYPT_DATA_PARAMS *)pMechanism->pParameter;
                mode = NSS_AES_CBC;
                iv = aesEncryptPtr->iv;
                data = aesEncryptPtr->pData;
                len = aesEncryptPtr->length;
            }

            cipherInfo = AES_CreateContext((unsigned char *)att->attrib.pValue,
                                           iv, mode, PR_TRUE,
                                           att->attrib.ulValueLen, 16);
            if (cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            crv = sftk_DeriveEncrypt((SFTKCipher)AES_Encrypt,
                                     cipherInfo, 16, key, keySize,
                                     data, len);
            AES_DestroyContext(cipherInfo, PR_TRUE);
            break;
        }

        case CKM_CAMELLIA_ECB_ENCRYPT_DATA:
        case CKM_CAMELLIA_CBC_ENCRYPT_DATA: {
            void *cipherInfo;
            CK_AES_CBC_ENCRYPT_DATA_PARAMS *aesEncryptPtr;
            int mode;
            unsigned char *iv;
            unsigned char *data;
            CK_ULONG len;

            if (mechanism == CKM_CAMELLIA_ECB_ENCRYPT_DATA) {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)
                                pMechanism->pParameter;
                aesEncryptPtr = NULL;
                mode = NSS_CAMELLIA;
                data = stringPtr->pData;
                len = stringPtr->ulLen;
                iv = NULL;
            } else {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_AES_CBC_ENCRYPT_DATA_PARAMS))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                stringPtr = NULL;
                aesEncryptPtr = (CK_AES_CBC_ENCRYPT_DATA_PARAMS *)
                                    pMechanism->pParameter;
                mode = NSS_CAMELLIA_CBC;
                iv = aesEncryptPtr->iv;
                data = aesEncryptPtr->pData;
                len = aesEncryptPtr->length;
            }

            cipherInfo = Camellia_CreateContext((unsigned char *)att->attrib.pValue,
                                                iv, mode, PR_TRUE,
                                                att->attrib.ulValueLen);
            if (cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            crv = sftk_DeriveEncrypt((SFTKCipher)Camellia_Encrypt,
                                     cipherInfo, 16, key, keySize,
                                     data, len);
            Camellia_DestroyContext(cipherInfo, PR_TRUE);
            break;
        }

        case CKM_SEED_ECB_ENCRYPT_DATA:
        case CKM_SEED_CBC_ENCRYPT_DATA: {
            void *cipherInfo;
            CK_AES_CBC_ENCRYPT_DATA_PARAMS *aesEncryptPtr;
            int mode;
            unsigned char *iv;
            unsigned char *data;
            CK_ULONG len;

            if (mechanism == CKM_SEED_ECB_ENCRYPT_DATA) {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                mode = NSS_SEED;
                stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)
                                pMechanism->pParameter;
                aesEncryptPtr = NULL;
                data = stringPtr->pData;
                len = stringPtr->ulLen;
                iv = NULL;
            } else {
                if (BAD_PARAM_CAST(pMechanism, sizeof(CK_AES_CBC_ENCRYPT_DATA_PARAMS))) {
                    crv = CKR_MECHANISM_PARAM_INVALID;
                    break;
                }
                mode = NSS_SEED_CBC;
                aesEncryptPtr = (CK_AES_CBC_ENCRYPT_DATA_PARAMS *)
                                    pMechanism->pParameter;
                iv = aesEncryptPtr->iv;
                data = aesEncryptPtr->pData;
                len = aesEncryptPtr->length;
            }

            cipherInfo = SEED_CreateContext((unsigned char *)att->attrib.pValue,
                                            iv, mode, PR_TRUE);
            if (cipherInfo == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            crv = sftk_DeriveEncrypt((SFTKCipher)SEED_Encrypt,
                                     cipherInfo, 16, key, keySize,
                                     data, len);
            SEED_DestroyContext(cipherInfo, PR_TRUE);
            break;
        }

        case CKM_CONCATENATE_BASE_AND_KEY: {
            SFTKObject *newKey;

            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            session = sftk_SessionFromHandle(hSession);
            if (session == NULL) {
                crv = CKR_SESSION_HANDLE_INVALID;
                break;
            }

            newKey = sftk_ObjectFromHandle(*(CK_OBJECT_HANDLE *)
                                                pMechanism->pParameter,
                                           session);
            sftk_FreeSession(session);
            if (newKey == NULL) {
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }

            if (sftk_isTrue(newKey, CKA_SENSITIVE)) {
                crv = sftk_forceAttribute(newKey, CKA_SENSITIVE, &cktrue,
                                          sizeof(CK_BBOOL));
                if (crv != CKR_OK) {
                    sftk_FreeObject(newKey);
                    break;
                }
            }

            att2 = sftk_FindAttribute(newKey, CKA_VALUE);
            if (att2 == NULL) {
                sftk_FreeObject(newKey);
                crv = CKR_KEY_HANDLE_INVALID;
                break;
            }
            tmpKeySize = att->attrib.ulValueLen + att2->attrib.ulValueLen;
            if (keySize == 0)
                keySize = tmpKeySize;
            if (keySize > tmpKeySize) {
                sftk_FreeObject(newKey);
                sftk_FreeAttribute(att2);
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            buf = (unsigned char *)PORT_Alloc(tmpKeySize);
            if (buf == NULL) {
                sftk_FreeAttribute(att2);
                sftk_FreeObject(newKey);
                crv = CKR_HOST_MEMORY;
                break;
            }

            PORT_Memcpy(buf, att->attrib.pValue, att->attrib.ulValueLen);
            PORT_Memcpy(buf + att->attrib.ulValueLen,
                        att2->attrib.pValue, att2->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, buf, keySize);
            PORT_ZFree(buf, tmpKeySize);
            sftk_FreeAttribute(att2);
            sftk_FreeObject(newKey);
            break;
        }

        case CKM_CONCATENATE_BASE_AND_DATA:
            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)pMechanism->pParameter;
            tmpKeySize = att->attrib.ulValueLen + stringPtr->ulLen;
            if (keySize == 0)
                keySize = tmpKeySize;
            if (keySize > tmpKeySize) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            buf = (unsigned char *)PORT_Alloc(tmpKeySize);
            if (buf == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }

            PORT_Memcpy(buf, att->attrib.pValue, att->attrib.ulValueLen);
            PORT_Memcpy(buf + att->attrib.ulValueLen, stringPtr->pData,
                        stringPtr->ulLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, buf, keySize);
            PORT_ZFree(buf, tmpKeySize);
            break;
        case CKM_CONCATENATE_DATA_AND_BASE:
            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)pMechanism->pParameter;
            tmpKeySize = att->attrib.ulValueLen + stringPtr->ulLen;
            if (keySize == 0)
                keySize = tmpKeySize;
            if (keySize > tmpKeySize) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            buf = (unsigned char *)PORT_Alloc(tmpKeySize);
            if (buf == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }

            PORT_Memcpy(buf, stringPtr->pData, stringPtr->ulLen);
            PORT_Memcpy(buf + stringPtr->ulLen, att->attrib.pValue,
                        att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, buf, keySize);
            PORT_ZFree(buf, tmpKeySize);
            break;
        case CKM_XOR_BASE_AND_DATA:
            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_KEY_DERIVATION_STRING_DATA))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)pMechanism->pParameter;
            tmpKeySize = PR_MIN(att->attrib.ulValueLen, stringPtr->ulLen);
            if (keySize == 0)
                keySize = tmpKeySize;
            if (keySize > tmpKeySize) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            buf = (unsigned char *)PORT_Alloc(keySize);
            if (buf == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }

            PORT_Memcpy(buf, att->attrib.pValue, keySize);
            for (i = 0; i < (int)keySize; i++) {
                buf[i] ^= stringPtr->pData[i];
            }

            crv = sftk_forceAttribute(key, CKA_VALUE, buf, keySize);
            PORT_ZFree(buf, keySize);
            break;

        case CKM_EXTRACT_KEY_FROM_KEY: {
            if (BAD_PARAM_CAST(pMechanism, sizeof(CK_EXTRACT_PARAMS))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            /* the following assumes 8 bits per byte */
            CK_ULONG extract = *(CK_EXTRACT_PARAMS *)pMechanism->pParameter;
            CK_ULONG shift = extract & 0x7; /* extract mod 8 the fast way */
            CK_ULONG offset = extract >> 3; /* extract div 8 the fast way */

            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            if (keySize == 0) {
                crv = CKR_TEMPLATE_INCOMPLETE;
                break;
            }
            /* make sure we have enough bits in the original key */
            if (att->attrib.ulValueLen <
                (offset + keySize + ((shift != 0) ? 1 : 0))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            buf = (unsigned char *)PORT_Alloc(keySize);
            if (buf == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }

            /* copy the bits we need into the new key */
            for (i = 0; i < (int)keySize; i++) {
                unsigned char *value =
                    ((unsigned char *)att->attrib.pValue) + offset + i;
                if (shift) {
                    buf[i] = (value[0] << (shift)) | (value[1] >> (8 - shift));
                } else {
                    buf[i] = value[0];
                }
            }

            crv = sftk_forceAttribute(key, CKA_VALUE, buf, keySize);
            PORT_ZFree(buf, keySize);
            break;
        }
        case CKM_MD2_KEY_DERIVATION:
            if (keySize == 0)
                keySize = MD2_LENGTH;
            if (keySize > MD2_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            /* now allocate the hash contexts */
            md2 = MD2_NewContext();
            if (md2 == NULL) {
                crv = CKR_HOST_MEMORY;
                break;
            }
            MD2_Begin(md2);
            MD2_Update(md2, (const unsigned char *)att->attrib.pValue,
                       att->attrib.ulValueLen);
            MD2_End(md2, key_block, &outLen, MD2_LENGTH);
            MD2_DestroyContext(md2, PR_TRUE);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;
        case CKM_MD5_KEY_DERIVATION:
            if (keySize == 0)
                keySize = MD5_LENGTH;
            if (keySize > MD5_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            MD5_HashBuf(key_block, (const unsigned char *)att->attrib.pValue,
                        att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;
        case CKM_SHA1_KEY_DERIVATION:
            if (keySize == 0)
                keySize = SHA1_LENGTH;
            if (keySize > SHA1_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            SHA1_HashBuf(key_block, (const unsigned char *)att->attrib.pValue,
                         att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;

        case CKM_SHA224_KEY_DERIVATION:
            if (keySize == 0)
                keySize = SHA224_LENGTH;
            if (keySize > SHA224_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            SHA224_HashBuf(key_block, (const unsigned char *)att->attrib.pValue,
                           att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;

        case CKM_SHA256_KEY_DERIVATION:
            if (keySize == 0)
                keySize = SHA256_LENGTH;
            if (keySize > SHA256_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            SHA256_HashBuf(key_block, (const unsigned char *)att->attrib.pValue,
                           att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;

        case CKM_SHA384_KEY_DERIVATION:
            if (keySize == 0)
                keySize = SHA384_LENGTH;
            if (keySize > SHA384_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            SHA384_HashBuf(key_block, (const unsigned char *)att->attrib.pValue,
                           att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;

        case CKM_SHA512_KEY_DERIVATION:
            if (keySize == 0)
                keySize = SHA512_LENGTH;
            if (keySize > SHA512_LENGTH) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            SHA512_HashBuf(key_block, (const unsigned char *)att->attrib.pValue,
                           att->attrib.ulValueLen);

            crv = sftk_forceAttribute(key, CKA_VALUE, key_block, keySize);
            break;

        case CKM_DH_PKCS_DERIVE: {
            SECItem derived, dhPublic;
            SECItem dhPrime, dhSubPrime, dhValue;
            /* sourceKey - values for the local existing low key */
            /* get prime and value attributes */
            crv = sftk_Attribute2SecItem(NULL, &dhPrime, sourceKey, CKA_PRIME);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SecItem(NULL, &dhValue, sourceKey, CKA_VALUE);
            if (crv != CKR_OK) {
                PORT_Free(dhPrime.data);
                break;
            }

            dhPublic.data = pMechanism->pParameter;
            dhPublic.len = pMechanism->ulParameterLen;

            /* If the caller bothered to provide Q, use Q to validate
             * the public key. */
            crv = sftk_Attribute2SecItem(NULL, &dhSubPrime, sourceKey, CKA_SUBPRIME);
            if (crv == CKR_OK) {
                rv = KEA_Verify(&dhPublic, &dhPrime, &dhSubPrime);
                PORT_Free(dhSubPrime.data);
                if (rv != SECSuccess) {
                    crv = CKR_ARGUMENTS_BAD;
                    PORT_Free(dhPrime.data);
                    PORT_Free(dhValue.data);
                    break;
                }
            }

            /* calculate private value - oct */
            rv = DH_Derive(&dhPublic, &dhPrime, &dhValue, &derived, keySize);

            PORT_Free(dhPrime.data);
            PORT_Free(dhValue.data);

            if (rv == SECSuccess) {
                sftk_forceAttribute(key, CKA_VALUE, derived.data, derived.len);
                PORT_ZFree(derived.data, derived.len);
                crv = CKR_OK;
            } else
                crv = CKR_HOST_MEMORY;

            break;
        }

        case CKM_ECDH1_DERIVE:
        case CKM_ECDH1_COFACTOR_DERIVE: {
            SECItem ecScalar, ecPoint;
            SECItem tmp;
            PRBool withCofactor = PR_FALSE;
            unsigned char *secret;
            unsigned char *keyData = NULL;
            unsigned int secretlen, pubKeyLen;
            CK_ECDH1_DERIVE_PARAMS *mechParams;
            NSSLOWKEYPrivateKey *privKey;
            PLArenaPool *arena = NULL;

            /* Check mechanism parameters */
            mechParams = (CK_ECDH1_DERIVE_PARAMS *)pMechanism->pParameter;
            if ((pMechanism->ulParameterLen != sizeof(CK_ECDH1_DERIVE_PARAMS)) ||
                ((mechParams->kdf == CKD_NULL) &&
                 ((mechParams->ulSharedDataLen != 0) ||
                  (mechParams->pSharedData != NULL)))) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }

            privKey = sftk_GetPrivKey(sourceKey, CKK_EC, &crv);
            if (privKey == NULL) {
                break;
            }

            /* Now we are working with a non-NULL private key */
            SECITEM_CopyItem(NULL, &ecScalar, &privKey->u.ec.privateValue);

            ecPoint.data = mechParams->pPublicData;
            ecPoint.len = mechParams->ulPublicDataLen;

            pubKeyLen = EC_GetPointSize(&privKey->u.ec.ecParams);

            /* if the len is too large, might be an encoded point */
            if (ecPoint.len > pubKeyLen) {
                SECItem newPoint;

                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena == NULL) {
                    goto ec_loser;
                }

                rv = SEC_QuickDERDecodeItem(arena, &newPoint,
                                            SEC_ASN1_GET(SEC_OctetStringTemplate),
                                            &ecPoint);
                if (rv != SECSuccess) {
                    goto ec_loser;
                }
                ecPoint = newPoint;
            }

            if (mechanism == CKM_ECDH1_COFACTOR_DERIVE) {
                withCofactor = PR_TRUE;
            }

            rv = ECDH_Derive(&ecPoint, &privKey->u.ec.ecParams, &ecScalar,
                             withCofactor, &tmp);
            PORT_ZFree(ecScalar.data, ecScalar.len);
            ecScalar.data = NULL;
            if (privKey != sourceKey->objectInfo) {
                nsslowkey_DestroyPrivateKey(privKey);
                privKey = NULL;
            }
            if (arena) {
                PORT_FreeArena(arena, PR_FALSE);
                arena = NULL;
            }

            if (rv != SECSuccess) {
                crv = sftk_MapCryptError(PORT_GetError());
                break;
            }

            /*
             * apply the kdf function.
             */
            if (mechParams->kdf == CKD_NULL) {
                /*
             * tmp is the raw data created by ECDH_Derive,
             * secret and secretlen are the values we will
             * eventually pass as our generated key.
             */
                secret = tmp.data;
                secretlen = tmp.len;
            } else {
                secretlen = keySize;
                crv = sftk_ANSI_X9_63_kdf(&secret, keySize,
                                          &tmp, mechParams->pSharedData,
                                          mechParams->ulSharedDataLen, mechParams->kdf);
                PORT_ZFree(tmp.data, tmp.len);
                if (crv != CKR_OK) {
                    break;
                }
                tmp.data = secret;
                tmp.len = secretlen;
            }

            /*
             * if keySize is supplied, then we are generating a key of a specific
             * length. This is done by taking the least significant 'keySize'
             * bytes from the unsigned value calculated by ECDH. Note: this may
             * mean padding temp with extra leading zeros from what ECDH_Derive
             * already returned (which itself may contain leading zeros).
             */
            if (keySize) {
                if (secretlen < keySize) {
                    keyData = PORT_ZAlloc(keySize);
                    if (!keyData) {
                        PORT_ZFree(tmp.data, tmp.len);
                        crv = CKR_HOST_MEMORY;
                        break;
                    }
                    PORT_Memcpy(&keyData[keySize - secretlen], secret, secretlen);
                    secret = keyData;
                } else {
                    secret += (secretlen - keySize);
                }
                secretlen = keySize;
            }

            sftk_forceAttribute(key, CKA_VALUE, secret, secretlen);
            PORT_ZFree(tmp.data, tmp.len);
            if (keyData) {
                PORT_ZFree(keyData, keySize);
            }
            break;

        ec_loser:
            crv = CKR_ARGUMENTS_BAD;
            PORT_Free(ecScalar.data);
            if (privKey != sourceKey->objectInfo)
                nsslowkey_DestroyPrivateKey(privKey);
            if (arena) {
                PORT_FreeArena(arena, PR_FALSE);
            }
            break;
        }

        /* See RFC 5869 and CK_NSS_HKDFParams for documentation. */
        case CKM_NSS_HKDF_SHA1:
            hashType = HASH_AlgSHA1;
            goto hkdf;
        case CKM_NSS_HKDF_SHA256:
            hashType = HASH_AlgSHA256;
            goto hkdf;
        case CKM_NSS_HKDF_SHA384:
            hashType = HASH_AlgSHA384;
            goto hkdf;
        case CKM_NSS_HKDF_SHA512:
            hashType = HASH_AlgSHA512;
            goto hkdf;
        hkdf : {
            const CK_NSS_HKDFParams *params =
                (const CK_NSS_HKDFParams *)pMechanism->pParameter;
            const SECHashObject *rawHash;
            unsigned hashLen;
            CK_BYTE hashbuf[HASH_LENGTH_MAX];
            CK_BYTE *prk; /* psuedo-random key */
            CK_ULONG prkLen;
            CK_BYTE *okm;                 /* output keying material */
            unsigned allocated_space = 0; /* If we need more work space, track it */
            unsigned char *key_buf = &key_block[0];

            rawHash = HASH_GetRawHashObject(hashType);
            if (rawHash == NULL || rawHash->length > sizeof(hashbuf)) {
                crv = CKR_FUNCTION_FAILED;
                break;
            }
            hashLen = rawHash->length;

            if (pMechanism->ulParameterLen != sizeof(CK_NSS_HKDFParams) ||
                !params || (!params->bExpand && !params->bExtract) ||
                (params->bExtract && params->ulSaltLen > 0 && !params->pSalt) ||
                (params->bExpand && params->ulInfoLen > 0 && !params->pInfo)) {
                crv = CKR_MECHANISM_PARAM_INVALID;
                break;
            }
            if (keySize == 0 ||
                (!params->bExpand && keySize > hashLen) ||
                (params->bExpand && keySize > 255 * hashLen)) {
                crv = CKR_TEMPLATE_INCONSISTENT;
                break;
            }
            crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv != CKR_OK)
                break;

            /* HKDF-Extract(salt, base key value) */
            if (params->bExtract) {
                CK_BYTE *salt;
                CK_ULONG saltLen;
                HMACContext *hmac;
                unsigned int bufLen;

                salt = params->pSalt;
                saltLen = params->ulSaltLen;
                if (salt == NULL) {
                    saltLen = hashLen;
                    salt = hashbuf;
                    memset(salt, 0, saltLen);
                }
                hmac = HMAC_Create(rawHash, salt, saltLen, isFIPS);
                if (!hmac) {
                    crv = CKR_HOST_MEMORY;
                    break;
                }
                HMAC_Begin(hmac);
                HMAC_Update(hmac, (const unsigned char *)att->attrib.pValue,
                            att->attrib.ulValueLen);
                HMAC_Finish(hmac, hashbuf, &bufLen, sizeof(hashbuf));
                HMAC_Destroy(hmac, PR_TRUE);
                PORT_Assert(bufLen == rawHash->length);
                prk = hashbuf;
                prkLen = bufLen;
            } else {
                /* PRK = base key value */
                prk = (CK_BYTE *)att->attrib.pValue;
                prkLen = att->attrib.ulValueLen;
            }

            /* HKDF-Expand */
            if (!params->bExpand) {
                okm = prk;
            } else {
                /* T(1) = HMAC-Hash(prk, "" | info | 0x01)
                 * T(n) = HMAC-Hash(prk, T(n-1) | info | n
                 * key material = T(1) | ... | T(n)
                 *
                 * If the requested output length does not fit
                 * within |key_block|, allocate space for expansion.
                 */
                HMACContext *hmac;
                CK_BYTE bi;
                unsigned n_bytes = PR_ROUNDUP(keySize, hashLen);
                unsigned iterations = n_bytes / hashLen;
                hmac = HMAC_Create(rawHash, prk, prkLen, isFIPS);
                if (hmac == NULL) {
                    crv = CKR_HOST_MEMORY;
                    break;
                }
                if (n_bytes > sizeof(key_block)) {
                    key_buf = PORT_Alloc(n_bytes);
                    if (key_buf == NULL) {
                        crv = CKR_HOST_MEMORY;
                        break;
                    }
                    allocated_space = n_bytes;
                }
                for (bi = 1; bi <= iterations && bi > 0; ++bi) {
                    unsigned len;
                    HMAC_Begin(hmac);
                    if (bi > 1) {
                        HMAC_Update(hmac, key_buf + ((bi - 2) * hashLen), hashLen);
                    }
                    if (params->ulInfoLen != 0) {
                        HMAC_Update(hmac, params->pInfo, params->ulInfoLen);
                    }
                    HMAC_Update(hmac, &bi, 1);
                    HMAC_Finish(hmac, key_buf + ((bi - 1) * hashLen), &len,
                                hashLen);
                    PORT_Assert(len == hashLen);
                }
                HMAC_Destroy(hmac, PR_TRUE);
                okm = key_buf;
            }
            /* key material = prk */
            crv = sftk_forceAttribute(key, CKA_VALUE, okm, keySize);
            if (allocated_space) {
                PORT_ZFree(key_buf, allocated_space);
            }
            break;
        } /* end of CKM_NSS_HKDF_* */

        case CKM_NSS_JPAKE_ROUND2_SHA1:
            hashType = HASH_AlgSHA1;
            goto jpake2;
        case CKM_NSS_JPAKE_ROUND2_SHA256:
            hashType = HASH_AlgSHA256;
            goto jpake2;
        case CKM_NSS_JPAKE_ROUND2_SHA384:
            hashType = HASH_AlgSHA384;
            goto jpake2;
        case CKM_NSS_JPAKE_ROUND2_SHA512:
            hashType = HASH_AlgSHA512;
            goto jpake2;
        jpake2:
            if (pMechanism->pParameter == NULL ||
                pMechanism->ulParameterLen != sizeof(CK_NSS_JPAKERound2Params))
                crv = CKR_MECHANISM_PARAM_INVALID;
            if (crv == CKR_OK && sftk_isTrue(key, CKA_TOKEN))
                crv = CKR_TEMPLATE_INCONSISTENT;
            if (crv == CKR_OK)
                crv = sftk_DeriveSensitiveCheck(sourceKey, key);
            if (crv == CKR_OK)
                crv = jpake_Round2(hashType,
                                   (CK_NSS_JPAKERound2Params *)pMechanism->pParameter,
                                   sourceKey, key);
            break;

        case CKM_NSS_JPAKE_FINAL_SHA1:
            hashType = HASH_AlgSHA1;
            goto jpakeFinal;
        case CKM_NSS_JPAKE_FINAL_SHA256:
            hashType = HASH_AlgSHA256;
            goto jpakeFinal;
        case CKM_NSS_JPAKE_FINAL_SHA384:
            hashType = HASH_AlgSHA384;
            goto jpakeFinal;
        case CKM_NSS_JPAKE_FINAL_SHA512:
            hashType = HASH_AlgSHA512;
            goto jpakeFinal;
        jpakeFinal:
            if (pMechanism->pParameter == NULL ||
                pMechanism->ulParameterLen != sizeof(CK_NSS_JPAKEFinalParams))
                crv = CKR_MECHANISM_PARAM_INVALID;
            /* We purposely do not do the derive sensitivity check; we want to be
               able to derive non-sensitive keys while allowing the ROUND1 and
               ROUND2 keys to be sensitive (which they always are, since they are
               in the CKO_PRIVATE_KEY class). The caller must include CKA_SENSITIVE
               in the template in order for the resultant keyblock key to be
               sensitive.
             */
            if (crv == CKR_OK)
                crv = jpake_Final(hashType,
                                  (CK_NSS_JPAKEFinalParams *)pMechanism->pParameter,
                                  sourceKey, key);
            break;

        default:
            crv = CKR_MECHANISM_INVALID;
    }
    if (att) {
        sftk_FreeAttribute(att);
    }
    sftk_FreeObject(sourceKey);
    if (crv != CKR_OK) {
        if (key)
            sftk_FreeObject(key);
        return crv;
    }

    /* link the key object into the list */
    if (key) {
        SFTKSessionObject *sessKey = sftk_narrowToSessionObject(key);
        PORT_Assert(sessKey);
        /* get the session */
        sessKey->wasDerived = PR_TRUE;
        session = sftk_SessionFromHandle(hSession);
        if (session == NULL) {
            sftk_FreeObject(key);
            return CKR_HOST_MEMORY;
        }

        crv = sftk_handleObject(key, session);
        sftk_FreeSession(session);
        *phKey = key->handle;
        sftk_FreeObject(key);
    }
    return crv;
}

/* NSC_GetFunctionStatus obtains an updated status of a function running
 * in parallel with an application. */
CK_RV
NSC_GetFunctionStatus(CK_SESSION_HANDLE hSession)
{
    CHECK_FORK();

    return CKR_FUNCTION_NOT_PARALLEL;
}

/* NSC_CancelFunction cancels a function running in parallel */
CK_RV
NSC_CancelFunction(CK_SESSION_HANDLE hSession)
{
    CHECK_FORK();

    return CKR_FUNCTION_NOT_PARALLEL;
}

/* NSC_GetOperationState saves the state of the cryptographic
 * operation in a session.
 * NOTE: This code only works for digest functions for now. eventually need
 * to add full flatten/resurect to our state stuff so that all types of state
 * can be saved */
CK_RV
NSC_GetOperationState(CK_SESSION_HANDLE hSession,
                      CK_BYTE_PTR pOperationState, CK_ULONG_PTR pulOperationStateLen)
{
    SFTKSessionContext *context;
    SFTKSession *session;
    CK_RV crv;
    CK_ULONG pOSLen = *pulOperationStateLen;

    CHECK_FORK();

    /* make sure we're legal */
    crv = sftk_GetContext(hSession, &context, SFTK_HASH, PR_TRUE, &session);
    if (crv != CKR_OK)
        return crv;

    *pulOperationStateLen = context->cipherInfoLen + sizeof(CK_MECHANISM_TYPE) + sizeof(SFTKContextType);
    if (pOperationState == NULL) {
        sftk_FreeSession(session);
        return CKR_OK;
    } else {
        if (pOSLen < *pulOperationStateLen) {
            return CKR_BUFFER_TOO_SMALL;
        }
    }
    PORT_Memcpy(pOperationState, &context->type, sizeof(SFTKContextType));
    pOperationState += sizeof(SFTKContextType);
    PORT_Memcpy(pOperationState, &context->currentMech,
                sizeof(CK_MECHANISM_TYPE));
    pOperationState += sizeof(CK_MECHANISM_TYPE);
    PORT_Memcpy(pOperationState, context->cipherInfo, context->cipherInfoLen);
    sftk_FreeSession(session);
    return CKR_OK;
}

#define sftk_Decrement(stateSize, len) \
    stateSize = ((stateSize) > (CK_ULONG)(len)) ? ((stateSize) - (CK_ULONG)(len)) : 0;

/* NSC_SetOperationState restores the state of the cryptographic
 * operation in a session. This is coded like it can restore lots of
 * states, but it only works for truly flat cipher structures. */
CK_RV
NSC_SetOperationState(CK_SESSION_HANDLE hSession,
                      CK_BYTE_PTR pOperationState, CK_ULONG ulOperationStateLen,
                      CK_OBJECT_HANDLE hEncryptionKey, CK_OBJECT_HANDLE hAuthenticationKey)
{
    SFTKSessionContext *context;
    SFTKSession *session;
    SFTKContextType type;
    CK_MECHANISM mech;
    CK_RV crv = CKR_OK;

    CHECK_FORK();

    while (ulOperationStateLen != 0) {
        /* get what type of state we're dealing with... */
        PORT_Memcpy(&type, pOperationState, sizeof(SFTKContextType));

        /* fix up session contexts based on type */
        session = sftk_SessionFromHandle(hSession);
        if (session == NULL)
            return CKR_SESSION_HANDLE_INVALID;
        context = sftk_ReturnContextByType(session, type);
        sftk_SetContextByType(session, type, NULL);
        if (context) {
            sftk_FreeContext(context);
        }
        pOperationState += sizeof(SFTKContextType);
        sftk_Decrement(ulOperationStateLen, sizeof(SFTKContextType));

        /* get the mechanism structure */
        PORT_Memcpy(&mech.mechanism, pOperationState, sizeof(CK_MECHANISM_TYPE));
        pOperationState += sizeof(CK_MECHANISM_TYPE);
        sftk_Decrement(ulOperationStateLen, sizeof(CK_MECHANISM_TYPE));
        /* should be filled in... but not necessary for hash */
        mech.pParameter = NULL;
        mech.ulParameterLen = 0;
        switch (type) {
            case SFTK_HASH:
                crv = NSC_DigestInit(hSession, &mech);
                if (crv != CKR_OK)
                    break;
                crv = sftk_GetContext(hSession, &context, SFTK_HASH, PR_TRUE,
                                      NULL);
                if (crv != CKR_OK)
                    break;
                PORT_Memcpy(context->cipherInfo, pOperationState,
                            context->cipherInfoLen);
                pOperationState += context->cipherInfoLen;
                sftk_Decrement(ulOperationStateLen, context->cipherInfoLen);
                break;
            default:
                /* do sign/encrypt/decrypt later */
                crv = CKR_SAVED_STATE_INVALID;
        }
        sftk_FreeSession(session);
        if (crv != CKR_OK)
            break;
    }
    return crv;
}

/* Dual-function cryptographic operations */

/* NSC_DigestEncryptUpdate continues a multiple-part digesting and encryption
 * operation. */
CK_RV
NSC_DigestEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                        CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart,
                        CK_ULONG_PTR pulEncryptedPartLen)
{
    CK_RV crv;

    CHECK_FORK();

    crv = NSC_EncryptUpdate(hSession, pPart, ulPartLen, pEncryptedPart,
                            pulEncryptedPartLen);
    if (crv != CKR_OK)
        return crv;
    crv = NSC_DigestUpdate(hSession, pPart, ulPartLen);

    return crv;
}

/* NSC_DecryptDigestUpdate continues a multiple-part decryption and
 * digesting operation. */
CK_RV
NSC_DecryptDigestUpdate(CK_SESSION_HANDLE hSession,
                        CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen,
                        CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
    CK_RV crv;

    CHECK_FORK();

    crv = NSC_DecryptUpdate(hSession, pEncryptedPart, ulEncryptedPartLen,
                            pPart, pulPartLen);
    if (crv != CKR_OK)
        return crv;
    crv = NSC_DigestUpdate(hSession, pPart, *pulPartLen);

    return crv;
}

/* NSC_SignEncryptUpdate continues a multiple-part signing and
 * encryption operation. */
CK_RV
NSC_SignEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                      CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart,
                      CK_ULONG_PTR pulEncryptedPartLen)
{
    CK_RV crv;

    CHECK_FORK();

    crv = NSC_EncryptUpdate(hSession, pPart, ulPartLen, pEncryptedPart,
                            pulEncryptedPartLen);
    if (crv != CKR_OK)
        return crv;
    crv = NSC_SignUpdate(hSession, pPart, ulPartLen);

    return crv;
}

/* NSC_DecryptVerifyUpdate continues a multiple-part decryption
 * and verify operation. */
CK_RV
NSC_DecryptVerifyUpdate(CK_SESSION_HANDLE hSession,
                        CK_BYTE_PTR pEncryptedData, CK_ULONG ulEncryptedDataLen,
                        CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{
    CK_RV crv;

    CHECK_FORK();

    crv = NSC_DecryptUpdate(hSession, pEncryptedData, ulEncryptedDataLen,
                            pData, pulDataLen);
    if (crv != CKR_OK)
        return crv;
    crv = NSC_VerifyUpdate(hSession, pData, *pulDataLen);

    return crv;
}

/* NSC_DigestKey continues a multi-part message-digesting operation,
 * by digesting the value of a secret key as part of the data already digested.
 */
CK_RV
NSC_DigestKey(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hKey)
{
    SFTKSession *session = NULL;
    SFTKObject *key = NULL;
    SFTKAttribute *att;
    CK_RV crv;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;

    key = sftk_ObjectFromHandle(hKey, session);
    sftk_FreeSession(session);
    if (key == NULL)
        return CKR_KEY_HANDLE_INVALID;

    /* PUT ANY DIGEST KEY RESTRICTION CHECKS HERE */

    /* make sure it's a valid  key for this operation */
    if (key->objclass != CKO_SECRET_KEY) {
        sftk_FreeObject(key);
        return CKR_KEY_TYPE_INCONSISTENT;
    }
    /* get the key value */
    att = sftk_FindAttribute(key, CKA_VALUE);
    sftk_FreeObject(key);
    if (!att) {
        return CKR_KEY_HANDLE_INVALID;
    }
    crv = NSC_DigestUpdate(hSession, (CK_BYTE_PTR)att->attrib.pValue,
                           att->attrib.ulValueLen);
    sftk_FreeAttribute(att);
    return crv;
}
