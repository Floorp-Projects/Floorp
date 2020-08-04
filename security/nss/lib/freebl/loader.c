/*
 * loader.c - load platform dependent DSO containing freebl implementation.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "loader.h"
#include "prmem.h"
#include "prerror.h"
#include "prinit.h"
#include "prenv.h"
#include "blname.c"

#include "prio.h"
#include "prprf.h"
#include <stdio.h>
#include "prsystem.h"

static const char *NameOfThisSharedLib =
    SHLIB_PREFIX "softokn" SOFTOKEN_SHLIB_VERSION "." SHLIB_SUFFIX;

static PRLibrary *blLib = NULL;

#define LSB(x) ((x)&0xff)
#define MSB(x) ((x) >> 8)

static const FREEBLVector *vector;
static const char *libraryName = NULL;

#include "genload.c"

/* This function must be run only once. */
/*  determine if hybrid platform, then actually load the DSO. */
static PRStatus
freebl_LoadDSO(void)
{
    PRLibrary *handle;
    const char *name = getLibName();

    if (!name) {
        PR_SetError(PR_LOAD_LIBRARY_ERROR, 0);
        return PR_FAILURE;
    }

    handle = loader_LoadLibrary(name);
    if (handle) {
        PRFuncPtr address = PR_FindFunctionSymbol(handle, "FREEBL_GetVector");
        if (address) {
            FREEBLGetVectorFn *getVector = (FREEBLGetVectorFn *)address;
            const FREEBLVector *dsoVector = getVector();
            if (dsoVector) {
                unsigned short dsoVersion = dsoVector->version;
                unsigned short myVersion = FREEBL_VERSION;
                if (MSB(dsoVersion) == MSB(myVersion) &&
                    LSB(dsoVersion) >= LSB(myVersion) &&
                    dsoVector->length >= sizeof(FREEBLVector)) {
                    vector = dsoVector;
                    libraryName = name;
                    blLib = handle;
                    return PR_SUCCESS;
                }
            }
        }
#ifdef DEBUG
        if (blLib) {
            PRStatus status = PR_UnloadLibrary(blLib);
            PORT_Assert(PR_SUCCESS == status);
        }
#else
        if (blLib)
            PR_UnloadLibrary(blLib);
#endif
    }
    return PR_FAILURE;
}

static const PRCallOnceType pristineCallOnce;
static PRCallOnceType loadFreeBLOnce;

static PRStatus
freebl_RunLoaderOnce(void)
{
    PRStatus status;

    status = PR_CallOnce(&loadFreeBLOnce, &freebl_LoadDSO);
    return status;
}

SECStatus
BL_Init(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_BL_Init)();
}

RSAPrivateKey *
RSA_NewKey(int keySizeInBits, SECItem *publicExponent)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_RSA_NewKey)(keySizeInBits, publicExponent);
}

SECStatus
RSA_PublicKeyOp(RSAPublicKey *key,
                unsigned char *output,
                const unsigned char *input)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_PublicKeyOp)(key, output, input);
}

SECStatus
RSA_PrivateKeyOp(RSAPrivateKey *key,
                 unsigned char *output,
                 const unsigned char *input)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_PrivateKeyOp)(key, output, input);
}

SECStatus
RSA_PrivateKeyOpDoubleChecked(RSAPrivateKey *key,
                              unsigned char *output,
                              const unsigned char *input)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_PrivateKeyOpDoubleChecked)(key, output, input);
}

SECStatus
RSA_PrivateKeyCheck(const RSAPrivateKey *key)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_PrivateKeyCheck)(key);
}

SECStatus
DSA_NewKey(const PQGParams *params, DSAPrivateKey **privKey)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DSA_NewKey)(params, privKey);
}

SECStatus
DSA_SignDigest(DSAPrivateKey *key, SECItem *signature, const SECItem *digest)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DSA_SignDigest)(key, signature, digest);
}

SECStatus
DSA_VerifyDigest(DSAPublicKey *key, const SECItem *signature,
                 const SECItem *digest)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DSA_VerifyDigest)(key, signature, digest);
}

SECStatus
DSA_NewKeyFromSeed(const PQGParams *params, const unsigned char *seed,
                   DSAPrivateKey **privKey)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DSA_NewKeyFromSeed)(params, seed, privKey);
}

SECStatus
DSA_SignDigestWithSeed(DSAPrivateKey *key, SECItem *signature,
                       const SECItem *digest, const unsigned char *seed)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DSA_SignDigestWithSeed)(key, signature, digest, seed);
}

SECStatus
DSA_NewRandom(PLArenaPool *arena, const SECItem *q, SECItem *seed)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DSA_NewRandom)(arena, q, seed);
}

SECStatus
DH_GenParam(int primeLen, DHParams **params)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DH_GenParam)(primeLen, params);
}

SECStatus
DH_NewKey(DHParams *params, DHPrivateKey **privKey)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DH_NewKey)(params, privKey);
}

SECStatus
DH_Derive(SECItem *publicValue, SECItem *prime, SECItem *privateValue,
          SECItem *derivedSecret, unsigned int maxOutBytes)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DH_Derive)(publicValue, prime, privateValue,
                                 derivedSecret, maxOutBytes);
}

SECStatus
KEA_Derive(SECItem *prime, SECItem *public1, SECItem *public2,
           SECItem *private1, SECItem *private2, SECItem *derivedSecret)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_KEA_Derive)(prime, public1, public2,
                                  private1, private2, derivedSecret);
}

PRBool
KEA_Verify(SECItem *Y, SECItem *prime, SECItem *subPrime)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return PR_FALSE;
    return (vector->p_KEA_Verify)(Y, prime, subPrime);
}

PRBool
KEA_PrimeCheck(SECItem *prime)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return PR_FALSE;
    return (vector->p_KEA_PrimeCheck)(prime);
}

RC4Context *
RC4_CreateContext(const unsigned char *key, int len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_RC4_CreateContext)(key, len);
}

void
RC4_DestroyContext(RC4Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_RC4_DestroyContext)(cx, freeit);
}

SECStatus
RC4_Encrypt(RC4Context *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RC4_Encrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
}

SECStatus
RC4_Decrypt(RC4Context *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RC4_Decrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
}

RC2Context *
RC2_CreateContext(const unsigned char *key, unsigned int len,
                  const unsigned char *iv, int mode, unsigned effectiveKeyLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
#ifndef NSS_DISABLE_DEPRECATED_RC2
    return (vector->p_RC2_CreateContext)(key, len, iv, mode, effectiveKeyLen);
#else
    return NULL;
#endif
}

void
RC2_DestroyContext(RC2Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
#ifndef NSS_DISABLE_DEPRECATED_RC2
    (vector->p_RC2_DestroyContext)(cx, freeit);
#else
    return;
#endif
}

SECStatus
RC2_Encrypt(RC2Context *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
#ifndef NSS_DISABLE_DEPRECATED_RC2
    return (vector->p_RC2_Encrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
#else
    return SECFailure;
#endif
}

SECStatus
RC2_Decrypt(RC2Context *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
#ifndef NSS_DISABLE_DEPRECATED_RC2
    return (vector->p_RC2_Decrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
#else
    return SECFailure;
#endif
}

RC5Context *
RC5_CreateContext(const SECItem *key, unsigned int rounds,
                  unsigned int wordSize, const unsigned char *iv, int mode)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_RC5_CreateContext)(key, rounds, wordSize, iv, mode);
}

void
RC5_DestroyContext(RC5Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_RC5_DestroyContext)(cx, freeit);
}

SECStatus
RC5_Encrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RC5_Encrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
}

SECStatus
RC5_Decrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RC5_Decrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
}

DESContext *
DES_CreateContext(const unsigned char *key, const unsigned char *iv,
                  int mode, PRBool encrypt)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_DES_CreateContext)(key, iv, mode, encrypt);
}

void
DES_DestroyContext(DESContext *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_DES_DestroyContext)(cx, freeit);
}

SECStatus
DES_Encrypt(DESContext *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DES_Encrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
}

SECStatus
DES_Decrypt(DESContext *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DES_Decrypt)(cx, output, outputLen, maxOutputLen, input,
                                   inputLen);
}
SEEDContext *
SEED_CreateContext(const unsigned char *key, const unsigned char *iv,
                   int mode, PRBool encrypt)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
#ifndef NSS_DISABLE_DEPRECATED_SEED
    return (vector->p_SEED_CreateContext)(key, iv, mode, encrypt);
#else
    return NULL;
#endif
}

void
SEED_DestroyContext(SEEDContext *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
#ifndef NSS_DISABLE_DEPRECATED_SEED
    (vector->p_SEED_DestroyContext)(cx, freeit);
#else
    return;
#endif
}

SECStatus
SEED_Encrypt(SEEDContext *cx, unsigned char *output, unsigned int *outputLen,
             unsigned int maxOutputLen, const unsigned char *input,
             unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
#ifndef NSS_DISABLE_DEPRECATED_SEED
    return (vector->p_SEED_Encrypt)(cx, output, outputLen, maxOutputLen, input,
                                    inputLen);
#else
    return SECFailure;
#endif
}

SECStatus
SEED_Decrypt(SEEDContext *cx, unsigned char *output, unsigned int *outputLen,
             unsigned int maxOutputLen, const unsigned char *input,
             unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
#ifndef NSS_DISABLE_DEPRECATED_SEED
    return (vector->p_SEED_Decrypt)(cx, output, outputLen, maxOutputLen, input,
                                    inputLen);
#else
    return SECFailure;
#endif
}

AESContext *
AES_CreateContext(const unsigned char *key, const unsigned char *iv,
                  int mode, int encrypt,
                  unsigned int keylen, unsigned int blocklen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_AES_CreateContext)(key, iv, mode, encrypt, keylen,
                                         blocklen);
}

void
AES_DestroyContext(AESContext *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_AES_DestroyContext)(cx, freeit);
}

SECStatus
AES_Encrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_AES_Encrypt)(cx, output, outputLen, maxOutputLen,
                                   input, inputLen);
}

SECStatus
AES_Decrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_AES_Decrypt)(cx, output, outputLen, maxOutputLen,
                                   input, inputLen);
}

SECStatus
AES_AEAD(AESContext *cx, unsigned char *output,
         unsigned int *outputLen, unsigned int maxOutputLen,
         const unsigned char *input, unsigned int inputLen,
         void *params, unsigned int paramsLen,
         const unsigned char *aad, unsigned int aadLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_AES_AEAD)(cx, output, outputLen, maxOutputLen, input,
                                inputLen, params, paramsLen, aad, aadLen);
}

SECStatus
MD5_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_MD5_Hash)(dest, src);
}

SECStatus
MD5_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_MD5_HashBuf)(dest, src, src_length);
}

MD5Context *
MD5_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_MD5_NewContext)();
}

void
MD5_DestroyContext(MD5Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD5_DestroyContext)(cx, freeit);
}

void
MD5_Begin(MD5Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD5_Begin)(cx);
}

void
MD5_Update(MD5Context *cx, const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD5_Update)(cx, input, inputLen);
}

void
MD5_End(MD5Context *cx, unsigned char *digest,
        unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD5_End)(cx, digest, digestLen, maxDigestLen);
}

unsigned int
MD5_FlattenSize(MD5Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_MD5_FlattenSize)(cx);
}

SECStatus
MD5_Flatten(MD5Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_MD5_Flatten)(cx, space);
}

MD5Context *
MD5_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_MD5_Resurrect)(space, arg);
}

void
MD5_TraceState(MD5Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD5_TraceState)(cx);
}

SECStatus
MD2_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_MD2_Hash)(dest, src);
}

MD2Context *
MD2_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_MD2_NewContext)();
}

void
MD2_DestroyContext(MD2Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD2_DestroyContext)(cx, freeit);
}

void
MD2_Begin(MD2Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD2_Begin)(cx);
}

void
MD2_Update(MD2Context *cx, const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD2_Update)(cx, input, inputLen);
}

void
MD2_End(MD2Context *cx, unsigned char *digest,
        unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD2_End)(cx, digest, digestLen, maxDigestLen);
}

unsigned int
MD2_FlattenSize(MD2Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_MD2_FlattenSize)(cx);
}

SECStatus
MD2_Flatten(MD2Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_MD2_Flatten)(cx, space);
}

MD2Context *
MD2_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_MD2_Resurrect)(space, arg);
}

SECStatus
SHA1_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA1_Hash)(dest, src);
}

SECStatus
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA1_HashBuf)(dest, src, src_length);
}

SHA1Context *
SHA1_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA1_NewContext)();
}

void
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA1_DestroyContext)(cx, freeit);
}

void
SHA1_Begin(SHA1Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA1_Begin)(cx);
}

void
SHA1_Update(SHA1Context *cx, const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA1_Update)(cx, input, inputLen);
}

void
SHA1_End(SHA1Context *cx, unsigned char *digest,
         unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA1_End)(cx, digest, digestLen, maxDigestLen);
}

void
SHA1_TraceState(SHA1Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA1_TraceState)(cx);
}

unsigned int
SHA1_FlattenSize(SHA1Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_SHA1_FlattenSize)(cx);
}

SECStatus
SHA1_Flatten(SHA1Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA1_Flatten)(cx, space);
}

SHA1Context *
SHA1_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA1_Resurrect)(space, arg);
}

SECStatus
RNG_RNGInit(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RNG_RNGInit)();
}

SECStatus
RNG_RandomUpdate(const void *data, size_t bytes)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RNG_RandomUpdate)(data, bytes);
}

SECStatus
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RNG_GenerateGlobalRandomBytes)(dest, len);
}

void
RNG_RNGShutdown(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_RNG_RNGShutdown)();
}

SECStatus
PQG_ParamGen(unsigned int j, PQGParams **pParams, PQGVerify **pVfy)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PQG_ParamGen)(j, pParams, pVfy);
}

SECStatus
PQG_ParamGenSeedLen(unsigned int j, unsigned int seedBytes,
                    PQGParams **pParams, PQGVerify **pVfy)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PQG_ParamGenSeedLen)(j, seedBytes, pParams, pVfy);
}

SECStatus
PQG_VerifyParams(const PQGParams *params, const PQGVerify *vfy,
                 SECStatus *result)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PQG_VerifyParams)(params, vfy, result);
}

void
PQG_DestroyParams(PQGParams *params)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_PQG_DestroyParams)(params);
}

void
PQG_DestroyVerify(PQGVerify *vfy)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_PQG_DestroyVerify)(vfy);
}

void
BL_Cleanup(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_BL_Cleanup)();
}

void
BL_Unload(void)
{
    /* This function is not thread-safe, but doesn't need to be, because it is
     * only called from functions that are also defined as not thread-safe,
     * namely C_Finalize in softoken, and the SSL bypass shutdown callback called
     * from NSS_Shutdown. */
    char *disableUnload = NULL;
    vector = NULL;
    disableUnload = PR_GetEnvSecure("NSS_DISABLE_UNLOAD");
    if (blLib && !disableUnload) {
#ifdef DEBUG
        PRStatus status = PR_UnloadLibrary(blLib);
        PORT_Assert(PR_SUCCESS == status);
#else
        PR_UnloadLibrary(blLib);
#endif
    }
    blLib = NULL;
    loadFreeBLOnce = pristineCallOnce;
}

/* ============== New for 3.003 =============================== */

SECStatus
SHA256_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA256_Hash)(dest, src);
}

SECStatus
SHA256_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA256_HashBuf)(dest, src, src_length);
}

SHA256Context *
SHA256_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA256_NewContext)();
}

void
SHA256_DestroyContext(SHA256Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA256_DestroyContext)(cx, freeit);
}

void
SHA256_Begin(SHA256Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA256_Begin)(cx);
}

void
SHA256_Update(SHA256Context *cx, const unsigned char *input,
              unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA256_Update)(cx, input, inputLen);
}

void
SHA256_End(SHA256Context *cx, unsigned char *digest,
           unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA256_End)(cx, digest, digestLen, maxDigestLen);
}

void
SHA256_TraceState(SHA256Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA256_TraceState)(cx);
}

unsigned int
SHA256_FlattenSize(SHA256Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_SHA256_FlattenSize)(cx);
}

SECStatus
SHA256_Flatten(SHA256Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA256_Flatten)(cx, space);
}

SHA256Context *
SHA256_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA256_Resurrect)(space, arg);
}

SECStatus
SHA512_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA512_Hash)(dest, src);
}

SECStatus
SHA512_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA512_HashBuf)(dest, src, src_length);
}

SHA512Context *
SHA512_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA512_NewContext)();
}

void
SHA512_DestroyContext(SHA512Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA512_DestroyContext)(cx, freeit);
}

void
SHA512_Begin(SHA512Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA512_Begin)(cx);
}

void
SHA512_Update(SHA512Context *cx, const unsigned char *input,
              unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA512_Update)(cx, input, inputLen);
}

void
SHA512_End(SHA512Context *cx, unsigned char *digest,
           unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA512_End)(cx, digest, digestLen, maxDigestLen);
}

void
SHA512_TraceState(SHA512Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA512_TraceState)(cx);
}

unsigned int
SHA512_FlattenSize(SHA512Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_SHA512_FlattenSize)(cx);
}

SECStatus
SHA512_Flatten(SHA512Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA512_Flatten)(cx, space);
}

SHA512Context *
SHA512_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA512_Resurrect)(space, arg);
}

SECStatus
SHA384_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA384_Hash)(dest, src);
}

SECStatus
SHA384_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA384_HashBuf)(dest, src, src_length);
}

SHA384Context *
SHA384_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA384_NewContext)();
}

void
SHA384_DestroyContext(SHA384Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA384_DestroyContext)(cx, freeit);
}

void
SHA384_Begin(SHA384Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA384_Begin)(cx);
}

void
SHA384_Update(SHA384Context *cx, const unsigned char *input,
              unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA384_Update)(cx, input, inputLen);
}

void
SHA384_End(SHA384Context *cx, unsigned char *digest,
           unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA384_End)(cx, digest, digestLen, maxDigestLen);
}

void
SHA384_TraceState(SHA384Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA384_TraceState)(cx);
}

unsigned int
SHA384_FlattenSize(SHA384Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_SHA384_FlattenSize)(cx);
}

SECStatus
SHA384_Flatten(SHA384Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA384_Flatten)(cx, space);
}

SHA384Context *
SHA384_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA384_Resurrect)(space, arg);
}

AESKeyWrapContext *
AESKeyWrap_CreateContext(const unsigned char *key, const unsigned char *iv,
                         int encrypt, unsigned int keylen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return vector->p_AESKeyWrap_CreateContext(key, iv, encrypt, keylen);
}

void
AESKeyWrap_DestroyContext(AESKeyWrapContext *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    vector->p_AESKeyWrap_DestroyContext(cx, freeit);
}

SECStatus
AESKeyWrap_Encrypt(AESKeyWrapContext *cx, unsigned char *output,
                   unsigned int *outputLen, unsigned int maxOutputLen,
                   const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return vector->p_AESKeyWrap_Encrypt(cx, output, outputLen, maxOutputLen,
                                        input, inputLen);
}

SECStatus
AESKeyWrap_Decrypt(AESKeyWrapContext *cx, unsigned char *output,
                   unsigned int *outputLen, unsigned int maxOutputLen,
                   const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return vector->p_AESKeyWrap_Decrypt(cx, output, outputLen, maxOutputLen,
                                        input, inputLen);
}

SECStatus
AESKeyWrap_EncryptKWP(AESKeyWrapContext *cx, unsigned char *output,
                      unsigned int *outputLen, unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return vector->p_AESKeyWrap_EncryptKWP(cx, output, outputLen, maxOutputLen,
                                           input, inputLen);
}

SECStatus
AESKeyWrap_DecryptKWP(AESKeyWrapContext *cx, unsigned char *output,
                      unsigned int *outputLen, unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return vector->p_AESKeyWrap_DecryptKWP(cx, output, outputLen, maxOutputLen,
                                           input, inputLen);
}

PRBool
BLAPI_SHVerify(const char *name, PRFuncPtr addr)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return PR_FALSE;
    return vector->p_BLAPI_SHVerify(name, addr);
}

/*
 * The Caller is expected to pass NULL as the name, which will
 * trigger the p_BLAPI_VerifySelf() to return 'TRUE'. Pass the real
 * name of the shared library we loaded (the static libraryName set
 * in freebl_LoadDSO) to p_BLAPI_VerifySelf.
 */
PRBool
BLAPI_VerifySelf(const char *name)
{
    PORT_Assert(!name);
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return PR_FALSE;
    return vector->p_BLAPI_VerifySelf(libraryName);
}

/* ============== New for 3.006 =============================== */

SECStatus
EC_NewKey(ECParams *params, ECPrivateKey **privKey)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_NewKey)(params, privKey);
}

SECStatus
EC_NewKeyFromSeed(ECParams *params, ECPrivateKey **privKey,
                  const unsigned char *seed, int seedlen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_NewKeyFromSeed)(params, privKey, seed, seedlen);
}

SECStatus
EC_ValidatePublicKey(ECParams *params, SECItem *publicValue)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_ValidatePublicKey)(params, publicValue);
}

SECStatus
ECDH_Derive(SECItem *publicValue, ECParams *params, SECItem *privateValue,
            PRBool withCofactor, SECItem *derivedSecret)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ECDH_Derive)(publicValue, params, privateValue,
                                   withCofactor, derivedSecret);
}

SECStatus
ECDSA_SignDigest(ECPrivateKey *key, SECItem *signature,
                 const SECItem *digest)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ECDSA_SignDigest)(key, signature, digest);
}

SECStatus
ECDSA_VerifyDigest(ECPublicKey *key, const SECItem *signature,
                   const SECItem *digest)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ECDSA_VerifyDigest)(key, signature, digest);
}

SECStatus
ECDSA_SignDigestWithSeed(ECPrivateKey *key, SECItem *signature,
                         const SECItem *digest, const unsigned char *seed, const int seedlen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ECDSA_SignDigestWithSeed)(key, signature, digest,
                                                seed, seedlen);
}

/* ============== New for 3.008 =============================== */

AESContext *
AES_AllocateContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_AES_AllocateContext)();
}

AESKeyWrapContext *
AESKeyWrap_AllocateContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_AESKeyWrap_AllocateContext)();
}

DESContext *
DES_AllocateContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_DES_AllocateContext)();
}

RC2Context *
RC2_AllocateContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
#ifndef NSS_DISABLE_DEPRECATED_RC2
    return (vector->p_RC2_AllocateContext)();
#else
    return NULL;
#endif
}

RC4Context *
RC4_AllocateContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_RC4_AllocateContext)();
}

SECStatus
AES_InitContext(AESContext *cx, const unsigned char *key,
                unsigned int keylen, const unsigned char *iv, int mode,
                unsigned int encrypt, unsigned int blocklen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_AES_InitContext)(cx, key, keylen, iv, mode, encrypt,
                                       blocklen);
}

SECStatus
AESKeyWrap_InitContext(AESKeyWrapContext *cx, const unsigned char *key,
                       unsigned int keylen, const unsigned char *iv, int mode,
                       unsigned int encrypt, unsigned int blocklen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_AESKeyWrap_InitContext)(cx, key, keylen, iv, mode,
                                              encrypt, blocklen);
}

SECStatus
DES_InitContext(DESContext *cx, const unsigned char *key,
                unsigned int keylen, const unsigned char *iv, int mode,
                unsigned int encrypt, unsigned int xtra)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_DES_InitContext)(cx, key, keylen, iv, mode, encrypt, xtra);
}

SECStatus
SEED_InitContext(SEEDContext *cx, const unsigned char *key,
                 unsigned int keylen, const unsigned char *iv, int mode,
                 unsigned int encrypt, unsigned int xtra)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
#ifndef NSS_DISABLE_DEPRECATED_SEED
    return (vector->p_SEED_InitContext)(cx, key, keylen, iv, mode, encrypt, xtra);
#else
    return SECFailure;
#endif
}

SECStatus
RC2_InitContext(RC2Context *cx, const unsigned char *key,
                unsigned int keylen, const unsigned char *iv, int mode,
                unsigned int effectiveKeyLen, unsigned int xtra)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
#ifndef NSS_DISABLE_DEPRECATED_RC2
    return (vector->p_RC2_InitContext)(cx, key, keylen, iv, mode,
                                       effectiveKeyLen, xtra);
#else
    return SECFailure;
#endif
}

SECStatus
RC4_InitContext(RC4Context *cx, const unsigned char *key,
                unsigned int keylen, const unsigned char *x1, int x2,
                unsigned int x3, unsigned int x4)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RC4_InitContext)(cx, key, keylen, x1, x2, x3, x4);
}

void
MD2_Clone(MD2Context *dest, MD2Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD2_Clone)(dest, src);
}

void
MD5_Clone(MD5Context *dest, MD5Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_MD5_Clone)(dest, src);
}

void
SHA1_Clone(SHA1Context *dest, SHA1Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA1_Clone)(dest, src);
}

void
SHA256_Clone(SHA256Context *dest, SHA256Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA256_Clone)(dest, src);
}

void
SHA384_Clone(SHA384Context *dest, SHA384Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA384_Clone)(dest, src);
}

void
SHA512_Clone(SHA512Context *dest, SHA512Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA512_Clone)(dest, src);
}

SECStatus
TLS_PRF(const SECItem *secret, const char *label,
        SECItem *seed, SECItem *result, PRBool isFIPS)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_TLS_PRF)(secret, label, seed, result, isFIPS);
}

const SECHashObject *
HASH_GetRawHashObject(HASH_HashType hashType)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_HASH_GetRawHashObject)(hashType);
}

void
HMAC_Destroy(HMACContext *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_HMAC_Destroy)(cx, freeit);
}

HMACContext *
HMAC_Create(const SECHashObject *hashObj, const unsigned char *secret,
            unsigned int secret_len, PRBool isFIPS)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_HMAC_Create)(hashObj, secret, secret_len, isFIPS);
}

SECStatus
HMAC_Init(HMACContext *cx, const SECHashObject *hashObj,
          const unsigned char *secret, unsigned int secret_len, PRBool isFIPS)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_HMAC_Init)(cx, hashObj, secret, secret_len, isFIPS);
}

void
HMAC_Begin(HMACContext *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_HMAC_Begin)(cx);
}

void
HMAC_Update(HMACContext *cx, const unsigned char *data, unsigned int data_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_HMAC_Update)(cx, data, data_len);
}

SECStatus
HMAC_Finish(HMACContext *cx, unsigned char *result, unsigned int *result_len,
            unsigned int max_result_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_HMAC_Finish)(cx, result, result_len, max_result_len);
}

HMACContext *
HMAC_Clone(HMACContext *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_HMAC_Clone)(cx);
}

void
RNG_SystemInfoForRNG(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_RNG_SystemInfoForRNG)();
}

SECStatus
FIPS186Change_GenerateX(unsigned char *XKEY, const unsigned char *XSEEDj,
                        unsigned char *x_j)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_FIPS186Change_GenerateX)(XKEY, XSEEDj, x_j);
}

SECStatus
FIPS186Change_ReduceModQForDSA(const unsigned char *w,
                               const unsigned char *q,
                               unsigned char *xj)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_FIPS186Change_ReduceModQForDSA)(w, q, xj);
}

/* === new for Camellia === */
SECStatus
Camellia_InitContext(CamelliaContext *cx, const unsigned char *key,
                     unsigned int keylen, const unsigned char *iv, int mode,
                     unsigned int encrypt, unsigned int unused)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_Camellia_InitContext)(cx, key, keylen, iv, mode, encrypt,
                                            unused);
}

CamelliaContext *
Camellia_AllocateContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_Camellia_AllocateContext)();
}

CamelliaContext *
Camellia_CreateContext(const unsigned char *key, const unsigned char *iv,
                       int mode, int encrypt,
                       unsigned int keylen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_Camellia_CreateContext)(key, iv, mode, encrypt, keylen);
}

void
Camellia_DestroyContext(CamelliaContext *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_Camellia_DestroyContext)(cx, freeit);
}

SECStatus
Camellia_Encrypt(CamelliaContext *cx, unsigned char *output,
                 unsigned int *outputLen, unsigned int maxOutputLen,
                 const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_Camellia_Encrypt)(cx, output, outputLen, maxOutputLen,
                                        input, inputLen);
}

SECStatus
Camellia_Decrypt(CamelliaContext *cx, unsigned char *output,
                 unsigned int *outputLen, unsigned int maxOutputLen,
                 const unsigned char *input, unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_Camellia_Decrypt)(cx, output, outputLen, maxOutputLen,
                                        input, inputLen);
}

void
BL_SetForkState(PRBool forked)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_BL_SetForkState)(forked);
}

SECStatus
PRNGTEST_Instantiate(const PRUint8 *entropy, unsigned int entropy_len,
                     const PRUint8 *nonce, unsigned int nonce_len,
                     const PRUint8 *personal_string, unsigned int ps_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PRNGTEST_Instantiate)(entropy, entropy_len,
                                            nonce, nonce_len,
                                            personal_string, ps_len);
}

SECStatus
PRNGTEST_Reseed(const PRUint8 *entropy, unsigned int entropy_len,
                const PRUint8 *additional, unsigned int additional_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PRNGTEST_Reseed)(entropy, entropy_len,
                                       additional, additional_len);
}

SECStatus
PRNGTEST_Generate(PRUint8 *bytes, unsigned int bytes_len,
                  const PRUint8 *additional, unsigned int additional_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PRNGTEST_Generate)(bytes, bytes_len,
                                         additional, additional_len);
}

SECStatus
PRNGTEST_Uninstantiate()
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PRNGTEST_Uninstantiate)();
}

SECStatus
RSA_PopulatePrivateKey(RSAPrivateKey *key)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_PopulatePrivateKey)(key);
}

SECStatus
JPAKE_Sign(PLArenaPool *arena, const PQGParams *pqg, HASH_HashType hashType,
           const SECItem *signerID, const SECItem *x,
           const SECItem *testRandom, const SECItem *gxIn, SECItem *gxOut,
           SECItem *gv, SECItem *r)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_JPAKE_Sign)(arena, pqg, hashType, signerID, x,
                                  testRandom, gxIn, gxOut, gv, r);
}

SECStatus
JPAKE_Verify(PLArenaPool *arena, const PQGParams *pqg,
             HASH_HashType hashType, const SECItem *signerID,
             const SECItem *peerID, const SECItem *gx,
             const SECItem *gv, const SECItem *r)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_JPAKE_Verify)(arena, pqg, hashType, signerID, peerID,
                                    gx, gv, r);
}

SECStatus
JPAKE_Round2(PLArenaPool *arena, const SECItem *p, const SECItem *q,
             const SECItem *gx1, const SECItem *gx3, const SECItem *gx4,
             SECItem *base, const SECItem *x2, const SECItem *s, SECItem *x2s)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_JPAKE_Round2)(arena, p, q, gx1, gx3, gx4, base, x2, s, x2s);
}

SECStatus
JPAKE_Final(PLArenaPool *arena, const SECItem *p, const SECItem *q,
            const SECItem *x2, const SECItem *gx4, const SECItem *x2s,
            const SECItem *B, SECItem *K)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_JPAKE_Final)(arena, p, q, x2, gx4, x2s, B, K);
}

SECStatus
TLS_P_hash(HASH_HashType hashAlg, const SECItem *secret, const char *label,
           SECItem *seed, SECItem *result, PRBool isFIPS)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_TLS_P_hash)(hashAlg, secret, label, seed, result, isFIPS);
}

SECStatus
SHA224_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA224_Hash)(dest, src);
}

SECStatus
SHA224_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA224_HashBuf)(dest, src, src_length);
}

SHA224Context *
SHA224_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA224_NewContext)();
}

void
SHA224_DestroyContext(SHA224Context *cx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA224_DestroyContext)(cx, freeit);
}

void
SHA224_Begin(SHA256Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA224_Begin)(cx);
}

void
SHA224_Update(SHA224Context *cx, const unsigned char *input,
              unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA224_Update)(cx, input, inputLen);
}

void
SHA224_End(SHA224Context *cx, unsigned char *digest,
           unsigned int *digestLen, unsigned int maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA224_End)(cx, digest, digestLen, maxDigestLen);
}

void
SHA224_TraceState(SHA224Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA224_TraceState)(cx);
}

unsigned int
SHA224_FlattenSize(SHA224Context *cx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return 0;
    return (vector->p_SHA224_FlattenSize)(cx);
}

SECStatus
SHA224_Flatten(SHA224Context *cx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SHA224_Flatten)(cx, space);
}

SHA224Context *
SHA224_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_SHA224_Resurrect)(space, arg);
}

void
SHA224_Clone(SHA224Context *dest, SHA224Context *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_SHA224_Clone)(dest, src);
}

PRBool
BLAPI_SHVerifyFile(const char *name)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return PR_FALSE;
    return vector->p_BLAPI_SHVerifyFile(name);
}

/* === new for DSA-2 === */
SECStatus
PQG_ParamGenV2(unsigned int L, unsigned int N, unsigned int seedBytes,
               PQGParams **pParams, PQGVerify **pVfy)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_PQG_ParamGenV2)(L, N, seedBytes, pParams, pVfy);
}

SECStatus
PRNGTEST_RunHealthTests(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return vector->p_PRNGTEST_RunHealthTests();
}

SECStatus
SSLv3_MAC_ConstantTime(
    unsigned char *result,
    unsigned int *resultLen,
    unsigned int maxResultLen,
    const SECHashObject *hashObj,
    const unsigned char *secret,
    unsigned int secretLen,
    const unsigned char *header,
    unsigned int headerLen,
    const unsigned char *body,
    unsigned int bodyLen,
    unsigned int bodyTotalLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_SSLv3_MAC_ConstantTime)(
        result, resultLen, maxResultLen,
        hashObj,
        secret, secretLen,
        header, headerLen,
        body, bodyLen, bodyTotalLen);
}

SECStatus
HMAC_ConstantTime(
    unsigned char *result,
    unsigned int *resultLen,
    unsigned int maxResultLen,
    const SECHashObject *hashObj,
    const unsigned char *secret,
    unsigned int secretLen,
    const unsigned char *header,
    unsigned int headerLen,
    const unsigned char *body,
    unsigned int bodyLen,
    unsigned int bodyTotalLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_HMAC_ConstantTime)(
        result, resultLen, maxResultLen,
        hashObj,
        secret, secretLen,
        header, headerLen,
        body, bodyLen, bodyTotalLen);
}

SECStatus
RSA_SignRaw(RSAPrivateKey *key,
            unsigned char *output,
            unsigned int *outputLen,
            unsigned int maxOutputLen,
            const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_SignRaw)(key, output, outputLen, maxOutputLen, input,
                                   inputLen);
}

SECStatus
RSA_CheckSignRaw(RSAPublicKey *key,
                 const unsigned char *sig,
                 unsigned int sigLen,
                 const unsigned char *hash,
                 unsigned int hashLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_CheckSignRaw)(key, sig, sigLen, hash, hashLen);
}

SECStatus
RSA_CheckSignRecoverRaw(RSAPublicKey *key,
                        unsigned char *data,
                        unsigned int *dataLen,
                        unsigned int maxDataLen,
                        const unsigned char *sig,
                        unsigned int sigLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_CheckSignRecoverRaw)(key, data, dataLen, maxDataLen,
                                               sig, sigLen);
}

SECStatus
RSA_EncryptRaw(RSAPublicKey *key,
               unsigned char *output,
               unsigned int *outputLen,
               unsigned int maxOutputLen,
               const unsigned char *input,
               unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_EncryptRaw)(key, output, outputLen, maxOutputLen,
                                      input, inputLen);
}

SECStatus
RSA_DecryptRaw(RSAPrivateKey *key,
               unsigned char *output,
               unsigned int *outputLen,
               unsigned int maxOutputLen,
               const unsigned char *input,
               unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_DecryptRaw)(key, output, outputLen, maxOutputLen,
                                      input, inputLen);
}

SECStatus
RSA_EncryptOAEP(RSAPublicKey *key,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *label,
                unsigned int labelLen,
                const unsigned char *seed,
                unsigned int seedLen,
                unsigned char *output,
                unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input,
                unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_EncryptOAEP)(key, hashAlg, maskHashAlg, label,
                                       labelLen, seed, seedLen, output,
                                       outputLen, maxOutputLen, input, inputLen);
}

SECStatus
RSA_DecryptOAEP(RSAPrivateKey *key,
                HASH_HashType hashAlg,
                HASH_HashType maskHashAlg,
                const unsigned char *label,
                unsigned int labelLen,
                unsigned char *output,
                unsigned int *outputLen,
                unsigned int maxOutputLen,
                const unsigned char *input,
                unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_DecryptOAEP)(key, hashAlg, maskHashAlg, label,
                                       labelLen, output, outputLen,
                                       maxOutputLen, input, inputLen);
}

SECStatus
RSA_EncryptBlock(RSAPublicKey *key,
                 unsigned char *output,
                 unsigned int *outputLen,
                 unsigned int maxOutputLen,
                 const unsigned char *input,
                 unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_EncryptBlock)(key, output, outputLen, maxOutputLen,
                                        input, inputLen);
}

SECStatus
RSA_DecryptBlock(RSAPrivateKey *key,
                 unsigned char *output,
                 unsigned int *outputLen,
                 unsigned int maxOutputLen,
                 const unsigned char *input,
                 unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_DecryptBlock)(key, output, outputLen, maxOutputLen,
                                        input, inputLen);
}

SECStatus
RSA_SignPSS(RSAPrivateKey *key,
            HASH_HashType hashAlg,
            HASH_HashType maskHashAlg,
            const unsigned char *salt,
            unsigned int saltLen,
            unsigned char *output,
            unsigned int *outputLen,
            unsigned int maxOutputLen,
            const unsigned char *input,
            unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_SignPSS)(key, hashAlg, maskHashAlg, salt, saltLen,
                                   output, outputLen, maxOutputLen, input,
                                   inputLen);
}

SECStatus
RSA_CheckSignPSS(RSAPublicKey *key,
                 HASH_HashType hashAlg,
                 HASH_HashType maskHashAlg,
                 unsigned int saltLen,
                 const unsigned char *sig,
                 unsigned int sigLen,
                 const unsigned char *hash,
                 unsigned int hashLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_CheckSignPSS)(key, hashAlg, maskHashAlg, saltLen,
                                        sig, sigLen, hash, hashLen);
}

SECStatus
RSA_Sign(RSAPrivateKey *key,
         unsigned char *output,
         unsigned int *outputLen,
         unsigned int maxOutputLen,
         const unsigned char *input,
         unsigned int inputLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_Sign)(key, output, outputLen, maxOutputLen, input,
                                inputLen);
}

SECStatus
RSA_CheckSign(RSAPublicKey *key,
              const unsigned char *sig,
              unsigned int sigLen,
              const unsigned char *data,
              unsigned int dataLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_CheckSign)(key, sig, sigLen, data, dataLen);
}

SECStatus
RSA_CheckSignRecover(RSAPublicKey *key,
                     unsigned char *output,
                     unsigned int *outputLen,
                     unsigned int maxOutputLen,
                     const unsigned char *sig,
                     unsigned int sigLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_RSA_CheckSignRecover)(key, output, outputLen, maxOutputLen,
                                            sig, sigLen);
}

SECStatus
EC_FillParams(PLArenaPool *arena,
              const SECItem *encodedParams,
              ECParams *params)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_FillParams)(arena, encodedParams, params);
}

SECStatus
EC_DecodeParams(const SECItem *encodedParams,
                ECParams **ecparams)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_DecodeParams)(encodedParams, ecparams);
}

SECStatus
EC_CopyParams(PLArenaPool *arena, ECParams *dstParams,
              const ECParams *srcParams)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_CopyParams)(arena, dstParams, srcParams);
}

SECStatus
ChaCha20_Xor(unsigned char *output, const unsigned char *block, unsigned int len,
             const unsigned char *k, const unsigned char *nonce, PRUint32 ctr)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_ChaCha20_Xor)(output, block, len, k, nonce, ctr);
}

SECStatus
ChaCha20Poly1305_InitContext(ChaCha20Poly1305Context *ctx,
                             const unsigned char *key, unsigned int keyLen,
                             unsigned int tagLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ChaCha20Poly1305_InitContext)(ctx, key, keyLen, tagLen);
}

ChaCha20Poly1305Context *
ChaCha20Poly1305_CreateContext(const unsigned char *key, unsigned int keyLen,
                               unsigned int tagLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_ChaCha20Poly1305_CreateContext)(key, keyLen, tagLen);
}

void
ChaCha20Poly1305_DestroyContext(ChaCha20Poly1305Context *ctx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_ChaCha20Poly1305_DestroyContext)(ctx, freeit);
}

SECStatus
ChaCha20Poly1305_Seal(const ChaCha20Poly1305Context *ctx,
                      unsigned char *output, unsigned int *outputLen,
                      unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen,
                      const unsigned char *nonce, unsigned int nonceLen,
                      const unsigned char *ad, unsigned int adLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ChaCha20Poly1305_Seal)(
        ctx, output, outputLen, maxOutputLen, input, inputLen,
        nonce, nonceLen, ad, adLen);
}

SECStatus
ChaCha20Poly1305_Open(const ChaCha20Poly1305Context *ctx,
                      unsigned char *output, unsigned int *outputLen,
                      unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen,
                      const unsigned char *nonce, unsigned int nonceLen,
                      const unsigned char *ad, unsigned int adLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ChaCha20Poly1305_Open)(
        ctx, output, outputLen, maxOutputLen, input, inputLen,
        nonce, nonceLen, ad, adLen);
}

SECStatus
ChaCha20Poly1305_Encrypt(const ChaCha20Poly1305Context *ctx,
                         unsigned char *output, unsigned int *outputLen,
                         unsigned int maxOutputLen,
                         const unsigned char *input, unsigned int inputLen,
                         const unsigned char *nonce, unsigned int nonceLen,
                         const unsigned char *ad, unsigned int adLen,
                         unsigned char *tagOut)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ChaCha20Poly1305_Encrypt)(
        ctx, output, outputLen, maxOutputLen, input, inputLen,
        nonce, nonceLen, ad, adLen, tagOut);
}

SECStatus
ChaCha20Poly1305_Decrypt(const ChaCha20Poly1305Context *ctx,
                         unsigned char *output, unsigned int *outputLen,
                         unsigned int maxOutputLen,
                         const unsigned char *input, unsigned int inputLen,
                         const unsigned char *nonce, unsigned int nonceLen,
                         const unsigned char *ad, unsigned int adLen,
                         unsigned char *tagIn)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_ChaCha20Poly1305_Decrypt)(
        ctx, output, outputLen, maxOutputLen, input, inputLen,
        nonce, nonceLen, ad, adLen, tagIn);
}

int
EC_GetPointSize(const ECParams *params)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_EC_GetPointSize)(params);
}

SECStatus
BLAKE2B_Hash(unsigned char *dest, const char *src)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_Hash)(dest, src);
}

SECStatus
BLAKE2B_HashBuf(unsigned char *output, const unsigned char *input, PRUint32 inlen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_HashBuf)(output, input, inlen);
}

SECStatus
BLAKE2B_MAC_HashBuf(unsigned char *output, const unsigned char *input,
                    unsigned int inlen, const unsigned char *key,
                    unsigned int keylen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_MAC_HashBuf)(output, input, inlen, key, keylen);
}

BLAKE2BContext *
BLAKE2B_NewContext(void)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return NULL;
    }
    return (vector->p_BLAKE2B_NewContext)();
}

void
BLAKE2B_DestroyContext(BLAKE2BContext *ctx, PRBool freeit)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return;
    }
    (vector->p_BLAKE2B_DestroyContext)(ctx, freeit);
}

SECStatus
BLAKE2B_Begin(BLAKE2BContext *ctx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_Begin)(ctx);
}

SECStatus
BLAKE2B_MAC_Begin(BLAKE2BContext *ctx, const PRUint8 *key, const size_t keylen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_MAC_Begin)(ctx, key, keylen);
}

SECStatus
BLAKE2B_Update(BLAKE2BContext *ctx, const unsigned char *in, unsigned int inlen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_Update)(ctx, in, inlen);
}

SECStatus
BLAKE2B_End(BLAKE2BContext *ctx, unsigned char *out,
            unsigned int *digestLen, size_t maxDigestLen)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_End)(ctx, out, digestLen, maxDigestLen);
}

unsigned int
BLAKE2B_FlattenSize(BLAKE2BContext *ctx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return 0;
    }
    return (vector->p_BLAKE2B_FlattenSize)(ctx);
}

SECStatus
BLAKE2B_Flatten(BLAKE2BContext *ctx, unsigned char *space)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return SECFailure;
    }
    return (vector->p_BLAKE2B_Flatten)(ctx, space);
}

BLAKE2BContext *
BLAKE2B_Resurrect(unsigned char *space, void *arg)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce()) {
        return NULL;
    }
    return (vector->p_BLAKE2B_Resurrect)(space, arg);
}

/* == New for CMAC == */
SECStatus
CMAC_Init(CMACContext *ctx, CMACCipher type, const unsigned char *key,
          unsigned int key_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_CMAC_Init)(ctx, type, key, key_len);
}

CMACContext *
CMAC_Create(CMACCipher type, const unsigned char *key, unsigned int key_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return NULL;
    return (vector->p_CMAC_Create)(type, key, key_len);
}

SECStatus
CMAC_Begin(CMACContext *ctx)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_CMAC_Begin)(ctx);
}

SECStatus
CMAC_Update(CMACContext *ctx, const unsigned char *data, unsigned int data_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_CMAC_Update)(ctx, data, data_len);
}

SECStatus
CMAC_Finish(CMACContext *ctx, unsigned char *result, unsigned int *result_len,
            unsigned int max_result_len)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return SECFailure;
    return (vector->p_CMAC_Finish)(ctx, result, result_len, max_result_len);
}

void
CMAC_Destroy(CMACContext *ctx, PRBool free_it)
{
    if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
        return;
    (vector->p_CMAC_Destroy)(ctx, free_it);
}
