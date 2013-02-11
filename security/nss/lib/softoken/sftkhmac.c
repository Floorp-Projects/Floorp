/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "seccomon.h"
#include "secerr.h"
#include "blapi.h"
#include "pkcs11i.h"
#include "softoken.h"
#include "hmacct.h"

/* MACMechanismToHash converts a PKCS#11 MAC mechanism into a freebl hash
 * type. */
static HASH_HashType
MACMechanismToHash(CK_MECHANISM_TYPE mech)
{
    switch (mech) {
	case CKM_MD5_HMAC:
	case CKM_SSL3_MD5_MAC:
	    return HASH_AlgMD5;
	case CKM_SHA_1_HMAC:
	case CKM_SSL3_SHA1_MAC:
	    return HASH_AlgSHA1;
	case CKM_SHA224_HMAC:
	    return HASH_AlgSHA224;
	case CKM_SHA256_HMAC:
	    return HASH_AlgSHA256;
	case CKM_SHA384_HMAC:
	    return HASH_AlgSHA384;
	case CKM_SHA512_HMAC:
	    return HASH_AlgSHA512;
    }
    return HASH_AlgNULL;
}

static sftk_MACConstantTimeCtx *
SetupMAC(CK_MECHANISM_PTR mech, SFTKObject *key)
{
    CK_NSS_MAC_CONSTANT_TIME_PARAMS *params =
	(CK_NSS_MAC_CONSTANT_TIME_PARAMS *) mech->pParameter;
    sftk_MACConstantTimeCtx *ctx;
    HASH_HashType alg;
    SFTKAttribute *keyval;
    unsigned char secret[sizeof(ctx->secret)];
    unsigned int secretLength;

    if (mech->ulParameterLen != sizeof(CK_NSS_MAC_CONSTANT_TIME_PARAMS)) {
	return NULL;
    }

    alg = MACMechanismToHash(params->macAlg);
    if (alg == HASH_AlgNULL) {
	return NULL;
    }

    keyval = sftk_FindAttribute(key,CKA_VALUE);
    if (keyval == NULL) {
	return NULL;
    }
    secretLength = keyval->attrib.ulValueLen;
    if (secretLength > sizeof(secret)) {
	sftk_FreeAttribute(keyval);
	return NULL;
    }
    memcpy(secret, keyval->attrib.pValue, secretLength);
    sftk_FreeAttribute(keyval);

    ctx = PORT_Alloc(sizeof(sftk_MACConstantTimeCtx));
    if (!ctx) {
	return NULL;
    }

    memcpy(ctx->secret, secret, secretLength);
    ctx->secretLength = secretLength;
    ctx->hash = HASH_GetRawHashObject(alg);
    ctx->totalLength = params->ulBodyTotalLen;

    return ctx;
}

sftk_MACConstantTimeCtx *
sftk_HMACConstantTime_New(CK_MECHANISM_PTR mech, SFTKObject *key)
{
    CK_NSS_MAC_CONSTANT_TIME_PARAMS *params =
	(CK_NSS_MAC_CONSTANT_TIME_PARAMS *) mech->pParameter;
    sftk_MACConstantTimeCtx *ctx;

    if (params->ulHeaderLen > sizeof(ctx->header)) {
	return NULL;
    }
    ctx = SetupMAC(mech, key);
    if (!ctx) {
	return NULL;
    }

    ctx->headerLength = params->ulHeaderLen;
    memcpy(ctx->header, params->pHeader, params->ulHeaderLen);
    return ctx;
}

sftk_MACConstantTimeCtx *
sftk_SSLv3MACConstantTime_New(CK_MECHANISM_PTR mech, SFTKObject *key)
{
    CK_NSS_MAC_CONSTANT_TIME_PARAMS *params =
	(CK_NSS_MAC_CONSTANT_TIME_PARAMS *) mech->pParameter;
    unsigned int padLength = 40, j;
    sftk_MACConstantTimeCtx *ctx;

    if (params->macAlg != CKM_SSL3_MD5_MAC &&
	params->macAlg != CKM_SSL3_SHA1_MAC) {
	return NULL;
    }
    ctx = SetupMAC(mech, key);
    if (!ctx) {
	return NULL;
    }

    if (params->macAlg == CKM_SSL3_MD5_MAC) {
	padLength = 48;
    }

    ctx->headerLength =
	ctx->secretLength +
	padLength +
	params->ulHeaderLen;

    if (ctx->headerLength > sizeof(ctx->header)) {
	goto loser;
    }

    j = 0;
    memcpy(&ctx->header[j], ctx->secret, ctx->secretLength);
    j += ctx->secretLength;
    memset(&ctx->header[j], 0x36, padLength);
    j += padLength;
    memcpy(&ctx->header[j], params->pHeader, params->ulHeaderLen);

    return ctx;

loser:
    PORT_Free(ctx);
    return NULL;
}

void
sftk_HMACConstantTime_Update(void *pctx, void *data, unsigned int len)
{
    sftk_MACConstantTimeCtx *ctx = (sftk_MACConstantTimeCtx *) pctx;
    SECStatus rv = HMAC_ConstantTime(
	ctx->mac, NULL, sizeof(ctx->mac),
	ctx->hash,
	ctx->secret, ctx->secretLength,
	ctx->header, ctx->headerLength,
	data, len,
	ctx->totalLength);
    PORT_Assert(rv == SECSuccess);
}

void
sftk_SSLv3MACConstantTime_Update(void *pctx, void *data, unsigned int len)
{
    sftk_MACConstantTimeCtx *ctx = (sftk_MACConstantTimeCtx *) pctx;
    SECStatus rv = SSLv3_MAC_ConstantTime(
	ctx->mac, NULL, sizeof(ctx->mac),
	ctx->hash,
	ctx->secret, ctx->secretLength,
	ctx->header, ctx->headerLength,
	data, len,
	ctx->totalLength);
    PORT_Assert(rv == SECSuccess);
}

void
sftk_MACConstantTime_EndHash(void *pctx, void *out, unsigned int *outLength,
			     unsigned int maxLength)
{
    const sftk_MACConstantTimeCtx *ctx = (sftk_MACConstantTimeCtx *) pctx;
    unsigned int toCopy = ctx->hash->length;
    if (toCopy > maxLength) {
	toCopy = maxLength;
    }
    memcpy(out, ctx->mac, toCopy);
    if (outLength) {
	*outLength = toCopy;
    }
}

void
sftk_MACConstantTime_DestroyContext(void *pctx, PRBool free)
{
    PORT_Free(pctx);
}
