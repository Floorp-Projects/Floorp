/*
 * loader.h - load platform dependent DSO containing freebl implementation.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LOADER_H_
#define _LOADER_H_ 1

#include "blapi.h"

#define FREEBL_VERSION 0x0312

struct FREEBLVectorStr {

    unsigned short length;  /* of this struct in bytes */
    unsigned short version; /* of this struct. */

    RSAPrivateKey *(*p_RSA_NewKey)(int keySizeInBits,
                                   SECItem *publicExponent);

    SECStatus (*p_RSA_PublicKeyOp)(RSAPublicKey *key,
                                   unsigned char *output,
                                   const unsigned char *input);

    SECStatus (*p_RSA_PrivateKeyOp)(RSAPrivateKey *key,
                                    unsigned char *output,
                                    const unsigned char *input);

    SECStatus (*p_DSA_NewKey)(const PQGParams *params,
                              DSAPrivateKey **privKey);

    SECStatus (*p_DSA_SignDigest)(DSAPrivateKey *key,
                                  SECItem *signature,
                                  const SECItem *digest);

    SECStatus (*p_DSA_VerifyDigest)(DSAPublicKey *key,
                                    const SECItem *signature,
                                    const SECItem *digest);

    SECStatus (*p_DSA_NewKeyFromSeed)(const PQGParams *params,
                                      const unsigned char *seed,
                                      DSAPrivateKey **privKey);

    SECStatus (*p_DSA_SignDigestWithSeed)(DSAPrivateKey *key,
                                          SECItem *signature,
                                          const SECItem *digest,
                                          const unsigned char *seed);

    SECStatus (*p_DH_GenParam)(int primeLen, DHParams **params);

    SECStatus (*p_DH_NewKey)(DHParams *params,
                             DHPrivateKey **privKey);

    SECStatus (*p_DH_Derive)(SECItem *publicValue,
                             SECItem *prime,
                             SECItem *privateValue,
                             SECItem *derivedSecret,
                             unsigned int maxOutBytes);

    SECStatus (*p_KEA_Derive)(SECItem *prime,
                              SECItem *public1,
                              SECItem *public2,
                              SECItem *private1,
                              SECItem *private2,
                              SECItem *derivedSecret);

    PRBool (*p_KEA_Verify)(SECItem *Y, SECItem *prime, SECItem *subPrime);

    RC4Context *(*p_RC4_CreateContext)(const unsigned char *key, int len);

    void (*p_RC4_DestroyContext)(RC4Context *cx, PRBool freeit);

    SECStatus (*p_RC4_Encrypt)(RC4Context *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_RC4_Decrypt)(RC4Context *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    RC2Context *(*p_RC2_CreateContext)(const unsigned char *key,
                                       unsigned int len, const unsigned char *iv,
                                       int mode, unsigned effectiveKeyLen);

    void (*p_RC2_DestroyContext)(RC2Context *cx, PRBool freeit);

    SECStatus (*p_RC2_Encrypt)(RC2Context *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_RC2_Decrypt)(RC2Context *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    RC5Context *(*p_RC5_CreateContext)(const SECItem *key, unsigned int rounds,
                                       unsigned int wordSize, const unsigned char *iv, int mode);

    void (*p_RC5_DestroyContext)(RC5Context *cx, PRBool freeit);

    SECStatus (*p_RC5_Encrypt)(RC5Context *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_RC5_Decrypt)(RC5Context *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    DESContext *(*p_DES_CreateContext)(const unsigned char *key,
                                       const unsigned char *iv,
                                       int mode, PRBool encrypt);

    void (*p_DES_DestroyContext)(DESContext *cx, PRBool freeit);

    SECStatus (*p_DES_Encrypt)(DESContext *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_DES_Decrypt)(DESContext *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    AESContext *(*p_AES_CreateContext)(const unsigned char *key,
                                       const unsigned char *iv,
                                       int mode, int encrypt, unsigned int keylen,
                                       unsigned int blocklen);

    void (*p_AES_DestroyContext)(AESContext *cx, PRBool freeit);

    SECStatus (*p_AES_Encrypt)(AESContext *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_AES_Decrypt)(AESContext *cx, unsigned char *output,
                               unsigned int *outputLen, unsigned int maxOutputLen,
                               const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_MD5_Hash)(unsigned char *dest, const char *src);

    SECStatus (*p_MD5_HashBuf)(unsigned char *dest, const unsigned char *src,
                               PRUint32 src_length);

    MD5Context *(*p_MD5_NewContext)(void);

    void (*p_MD5_DestroyContext)(MD5Context *cx, PRBool freeit);

    void (*p_MD5_Begin)(MD5Context *cx);

    void (*p_MD5_Update)(MD5Context *cx,
                         const unsigned char *input, unsigned int inputLen);

    void (*p_MD5_End)(MD5Context *cx, unsigned char *digest,
                      unsigned int *digestLen, unsigned int maxDigestLen);

    unsigned int (*p_MD5_FlattenSize)(MD5Context *cx);

    SECStatus (*p_MD5_Flatten)(MD5Context *cx, unsigned char *space);

    MD5Context *(*p_MD5_Resurrect)(unsigned char *space, void *arg);

    void (*p_MD5_TraceState)(MD5Context *cx);

    SECStatus (*p_MD2_Hash)(unsigned char *dest, const char *src);

    MD2Context *(*p_MD2_NewContext)(void);

    void (*p_MD2_DestroyContext)(MD2Context *cx, PRBool freeit);

    void (*p_MD2_Begin)(MD2Context *cx);

    void (*p_MD2_Update)(MD2Context *cx,
                         const unsigned char *input, unsigned int inputLen);

    void (*p_MD2_End)(MD2Context *cx, unsigned char *digest,
                      unsigned int *digestLen, unsigned int maxDigestLen);

    unsigned int (*p_MD2_FlattenSize)(MD2Context *cx);

    SECStatus (*p_MD2_Flatten)(MD2Context *cx, unsigned char *space);

    MD2Context *(*p_MD2_Resurrect)(unsigned char *space, void *arg);

    SECStatus (*p_SHA1_Hash)(unsigned char *dest, const char *src);

    SECStatus (*p_SHA1_HashBuf)(unsigned char *dest, const unsigned char *src,
                                PRUint32 src_length);

    SHA1Context *(*p_SHA1_NewContext)(void);

    void (*p_SHA1_DestroyContext)(SHA1Context *cx, PRBool freeit);

    void (*p_SHA1_Begin)(SHA1Context *cx);

    void (*p_SHA1_Update)(SHA1Context *cx, const unsigned char *input,
                          unsigned int inputLen);

    void (*p_SHA1_End)(SHA1Context *cx, unsigned char *digest,
                       unsigned int *digestLen, unsigned int maxDigestLen);

    void (*p_SHA1_TraceState)(SHA1Context *cx);

    unsigned int (*p_SHA1_FlattenSize)(SHA1Context *cx);

    SECStatus (*p_SHA1_Flatten)(SHA1Context *cx, unsigned char *space);

    SHA1Context *(*p_SHA1_Resurrect)(unsigned char *space, void *arg);

    SECStatus (*p_RNG_RNGInit)(void);

    SECStatus (*p_RNG_RandomUpdate)(const void *data, size_t bytes);

    SECStatus (*p_RNG_GenerateGlobalRandomBytes)(void *dest, size_t len);

    void (*p_RNG_RNGShutdown)(void);

    SECStatus (*p_PQG_ParamGen)(unsigned int j, PQGParams **pParams,
                                PQGVerify **pVfy);

    SECStatus (*p_PQG_ParamGenSeedLen)(unsigned int j, unsigned int seedBytes,
                                       PQGParams **pParams, PQGVerify **pVfy);

    SECStatus (*p_PQG_VerifyParams)(const PQGParams *params,
                                    const PQGVerify *vfy, SECStatus *result);

    /* Version 3.001 came to here */

    SECStatus (*p_RSA_PrivateKeyOpDoubleChecked)(RSAPrivateKey *key,
                                                 unsigned char *output,
                                                 const unsigned char *input);

    SECStatus (*p_RSA_PrivateKeyCheck)(const RSAPrivateKey *key);

    void (*p_BL_Cleanup)(void);

    /* Version 3.002 came to here */

    SHA256Context *(*p_SHA256_NewContext)(void);
    void (*p_SHA256_DestroyContext)(SHA256Context *cx, PRBool freeit);
    void (*p_SHA256_Begin)(SHA256Context *cx);
    void (*p_SHA256_Update)(SHA256Context *cx, const unsigned char *input,
                            unsigned int inputLen);
    void (*p_SHA256_End)(SHA256Context *cx, unsigned char *digest,
                         unsigned int *digestLen, unsigned int maxDigestLen);
    SECStatus (*p_SHA256_HashBuf)(unsigned char *dest, const unsigned char *src,
                                  PRUint32 src_length);
    SECStatus (*p_SHA256_Hash)(unsigned char *dest, const char *src);
    void (*p_SHA256_TraceState)(SHA256Context *cx);
    unsigned int (*p_SHA256_FlattenSize)(SHA256Context *cx);
    SECStatus (*p_SHA256_Flatten)(SHA256Context *cx, unsigned char *space);
    SHA256Context *(*p_SHA256_Resurrect)(unsigned char *space, void *arg);

    SHA512Context *(*p_SHA512_NewContext)(void);
    void (*p_SHA512_DestroyContext)(SHA512Context *cx, PRBool freeit);
    void (*p_SHA512_Begin)(SHA512Context *cx);
    void (*p_SHA512_Update)(SHA512Context *cx, const unsigned char *input,
                            unsigned int inputLen);
    void (*p_SHA512_End)(SHA512Context *cx, unsigned char *digest,
                         unsigned int *digestLen, unsigned int maxDigestLen);
    SECStatus (*p_SHA512_HashBuf)(unsigned char *dest, const unsigned char *src,
                                  PRUint32 src_length);
    SECStatus (*p_SHA512_Hash)(unsigned char *dest, const char *src);
    void (*p_SHA512_TraceState)(SHA512Context *cx);
    unsigned int (*p_SHA512_FlattenSize)(SHA512Context *cx);
    SECStatus (*p_SHA512_Flatten)(SHA512Context *cx, unsigned char *space);
    SHA512Context *(*p_SHA512_Resurrect)(unsigned char *space, void *arg);

    SHA384Context *(*p_SHA384_NewContext)(void);
    void (*p_SHA384_DestroyContext)(SHA384Context *cx, PRBool freeit);
    void (*p_SHA384_Begin)(SHA384Context *cx);
    void (*p_SHA384_Update)(SHA384Context *cx, const unsigned char *input,
                            unsigned int inputLen);
    void (*p_SHA384_End)(SHA384Context *cx, unsigned char *digest,
                         unsigned int *digestLen, unsigned int maxDigestLen);
    SECStatus (*p_SHA384_HashBuf)(unsigned char *dest, const unsigned char *src,
                                  PRUint32 src_length);
    SECStatus (*p_SHA384_Hash)(unsigned char *dest, const char *src);
    void (*p_SHA384_TraceState)(SHA384Context *cx);
    unsigned int (*p_SHA384_FlattenSize)(SHA384Context *cx);
    SECStatus (*p_SHA384_Flatten)(SHA384Context *cx, unsigned char *space);
    SHA384Context *(*p_SHA384_Resurrect)(unsigned char *space, void *arg);

    /* Version 3.003 came to here */

    AESKeyWrapContext *(*p_AESKeyWrap_CreateContext)(const unsigned char *key,
                                                     const unsigned char *iv, int encrypt, unsigned int keylen);

    void (*p_AESKeyWrap_DestroyContext)(AESKeyWrapContext *cx, PRBool freeit);

    SECStatus (*p_AESKeyWrap_Encrypt)(AESKeyWrapContext *cx,
                                      unsigned char *output,
                                      unsigned int *outputLen, unsigned int maxOutputLen,
                                      const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_AESKeyWrap_Decrypt)(AESKeyWrapContext *cx,
                                      unsigned char *output,
                                      unsigned int *outputLen, unsigned int maxOutputLen,
                                      const unsigned char *input, unsigned int inputLen);

    /* Version 3.004 came to here */

    PRBool (*p_BLAPI_SHVerify)(const char *name, PRFuncPtr addr);
    PRBool (*p_BLAPI_VerifySelf)(const char *name);

    /* Version 3.005 came to here */

    SECStatus (*p_EC_NewKey)(ECParams *params,
                             ECPrivateKey **privKey);

    SECStatus (*p_EC_NewKeyFromSeed)(ECParams *params,
                                     ECPrivateKey **privKey,
                                     const unsigned char *seed,
                                     int seedlen);

    SECStatus (*p_EC_ValidatePublicKey)(ECParams *params,
                                        SECItem *publicValue);

    SECStatus (*p_ECDH_Derive)(SECItem *publicValue,
                               ECParams *params,
                               SECItem *privateValue,
                               PRBool withCofactor,
                               SECItem *derivedSecret);

    SECStatus (*p_ECDSA_SignDigest)(ECPrivateKey *key,
                                    SECItem *signature,
                                    const SECItem *digest);

    SECStatus (*p_ECDSA_VerifyDigest)(ECPublicKey *key,
                                      const SECItem *signature,
                                      const SECItem *digest);

    SECStatus (*p_ECDSA_SignDigestWithSeed)(ECPrivateKey *key,
                                            SECItem *signature,
                                            const SECItem *digest,
                                            const unsigned char *seed,
                                            const int seedlen);

    /* Version 3.006 came to here */

    /* no modification to FREEBLVectorStr itself
   * but ECParamStr was modified
   */

    /* Version 3.007 came to here */

    SECStatus (*p_AES_InitContext)(AESContext *cx,
                                   const unsigned char *key,
                                   unsigned int keylen,
                                   const unsigned char *iv,
                                   int mode,
                                   unsigned int encrypt,
                                   unsigned int blocklen);
    SECStatus (*p_AESKeyWrap_InitContext)(AESKeyWrapContext *cx,
                                          const unsigned char *key,
                                          unsigned int keylen,
                                          const unsigned char *iv,
                                          int mode,
                                          unsigned int encrypt,
                                          unsigned int blocklen);
    SECStatus (*p_DES_InitContext)(DESContext *cx,
                                   const unsigned char *key,
                                   unsigned int keylen,
                                   const unsigned char *iv,
                                   int mode,
                                   unsigned int encrypt,
                                   unsigned int);
    SECStatus (*p_RC2_InitContext)(RC2Context *cx,
                                   const unsigned char *key,
                                   unsigned int keylen,
                                   const unsigned char *iv,
                                   int mode,
                                   unsigned int effectiveKeyLen,
                                   unsigned int);
    SECStatus (*p_RC4_InitContext)(RC4Context *cx,
                                   const unsigned char *key,
                                   unsigned int keylen,
                                   const unsigned char *,
                                   int,
                                   unsigned int,
                                   unsigned int);

    AESContext *(*p_AES_AllocateContext)(void);
    AESKeyWrapContext *(*p_AESKeyWrap_AllocateContext)(void);
    DESContext *(*p_DES_AllocateContext)(void);
    RC2Context *(*p_RC2_AllocateContext)(void);
    RC4Context *(*p_RC4_AllocateContext)(void);

    void (*p_MD2_Clone)(MD2Context *dest, MD2Context *src);
    void (*p_MD5_Clone)(MD5Context *dest, MD5Context *src);
    void (*p_SHA1_Clone)(SHA1Context *dest, SHA1Context *src);
    void (*p_SHA256_Clone)(SHA256Context *dest, SHA256Context *src);
    void (*p_SHA384_Clone)(SHA384Context *dest, SHA384Context *src);
    void (*p_SHA512_Clone)(SHA512Context *dest, SHA512Context *src);

    SECStatus (*p_TLS_PRF)(const SECItem *secret, const char *label,
                           SECItem *seed, SECItem *result, PRBool isFIPS);

    const SECHashObject *(*p_HASH_GetRawHashObject)(HASH_HashType hashType);

    HMACContext *(*p_HMAC_Create)(const SECHashObject *hashObj,
                                  const unsigned char *secret,
                                  unsigned int secret_len, PRBool isFIPS);
    SECStatus (*p_HMAC_Init)(HMACContext *cx, const SECHashObject *hash_obj,
                             const unsigned char *secret,
                             unsigned int secret_len, PRBool isFIPS);
    void (*p_HMAC_Begin)(HMACContext *cx);
    void (*p_HMAC_Update)(HMACContext *cx, const unsigned char *data,
                          unsigned int data_len);
    HMACContext *(*p_HMAC_Clone)(HMACContext *cx);
    SECStatus (*p_HMAC_Finish)(HMACContext *cx, unsigned char *result,
                               unsigned int *result_len,
                               unsigned int max_result_len);
    void (*p_HMAC_Destroy)(HMACContext *cx, PRBool freeit);

    void (*p_RNG_SystemInfoForRNG)(void);

    /* Version 3.008 came to here */

    SECStatus (*p_FIPS186Change_GenerateX)(unsigned char *XKEY,
                                           const unsigned char *XSEEDj,
                                           unsigned char *x_j);
    SECStatus (*p_FIPS186Change_ReduceModQForDSA)(const unsigned char *w,
                                                  const unsigned char *q,
                                                  unsigned char *xj);

    /* Version 3.009 came to here */

    SECStatus (*p_Camellia_InitContext)(CamelliaContext *cx,
                                        const unsigned char *key,
                                        unsigned int keylen,
                                        const unsigned char *iv,
                                        int mode,
                                        unsigned int encrypt,
                                        unsigned int unused);

    CamelliaContext *(*p_Camellia_AllocateContext)(void);
    CamelliaContext *(*p_Camellia_CreateContext)(const unsigned char *key,
                                                 const unsigned char *iv,
                                                 int mode, int encrypt,
                                                 unsigned int keylen);
    void (*p_Camellia_DestroyContext)(CamelliaContext *cx, PRBool freeit);

    SECStatus (*p_Camellia_Encrypt)(CamelliaContext *cx, unsigned char *output,
                                    unsigned int *outputLen,
                                    unsigned int maxOutputLen,
                                    const unsigned char *input,
                                    unsigned int inputLen);

    SECStatus (*p_Camellia_Decrypt)(CamelliaContext *cx, unsigned char *output,
                                    unsigned int *outputLen,
                                    unsigned int maxOutputLen,
                                    const unsigned char *input,
                                    unsigned int inputLen);

    void (*p_PQG_DestroyParams)(PQGParams *params);

    void (*p_PQG_DestroyVerify)(PQGVerify *vfy);

    /* Version 3.010 came to here */

    SECStatus (*p_SEED_InitContext)(SEEDContext *cx,
                                    const unsigned char *key,
                                    unsigned int keylen,
                                    const unsigned char *iv,
                                    int mode,
                                    unsigned int encrypt,
                                    unsigned int);

    SEEDContext *(*p_SEED_AllocateContext)(void);

    SEEDContext *(*p_SEED_CreateContext)(const unsigned char *key,
                                         const unsigned char *iv,
                                         int mode, PRBool encrypt);

    void (*p_SEED_DestroyContext)(SEEDContext *cx, PRBool freeit);

    SECStatus (*p_SEED_Encrypt)(SEEDContext *cx, unsigned char *output,
                                unsigned int *outputLen, unsigned int maxOutputLen,
                                const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_SEED_Decrypt)(SEEDContext *cx, unsigned char *output,
                                unsigned int *outputLen, unsigned int maxOutputLen,
                                const unsigned char *input, unsigned int inputLen);

    SECStatus (*p_BL_Init)(void);
    void (*p_BL_SetForkState)(PRBool);

    SECStatus (*p_PRNGTEST_Instantiate)(const PRUint8 *entropy,
                                        unsigned int entropy_len,
                                        const PRUint8 *nonce,
                                        unsigned int nonce_len,
                                        const PRUint8 *personal_string,
                                        unsigned int ps_len);

    SECStatus (*p_PRNGTEST_Reseed)(const PRUint8 *entropy,
                                   unsigned int entropy_len,
                                   const PRUint8 *additional,
                                   unsigned int additional_len);

    SECStatus (*p_PRNGTEST_Generate)(PRUint8 *bytes,
                                     unsigned int bytes_len,
                                     const PRUint8 *additional,
                                     unsigned int additional_len);

    SECStatus (*p_PRNGTEST_Uninstantiate)(void);
    /* Version 3.011 came to here */

    SECStatus (*p_RSA_PopulatePrivateKey)(RSAPrivateKey *key);

    SECStatus (*p_DSA_NewRandom)(PLArenaPool *arena, const SECItem *q,
                                 SECItem *seed);

    SECStatus (*p_JPAKE_Sign)(PLArenaPool *arena, const PQGParams *pqg,
                              HASH_HashType hashType, const SECItem *signerID,
                              const SECItem *x, const SECItem *testRandom,
                              const SECItem *gxIn, SECItem *gxOut,
                              SECItem *gv, SECItem *r);

    SECStatus (*p_JPAKE_Verify)(PLArenaPool *arena, const PQGParams *pqg,
                                HASH_HashType hashType, const SECItem *signerID,
                                const SECItem *peerID, const SECItem *gx,
                                const SECItem *gv, const SECItem *r);

    SECStatus (*p_JPAKE_Round2)(PLArenaPool *arena, const SECItem *p,
                                const SECItem *q, const SECItem *gx1,
                                const SECItem *gx3, const SECItem *gx4,
                                SECItem *base, const SECItem *x2,
                                const SECItem *s, SECItem *x2s);

    SECStatus (*p_JPAKE_Final)(PLArenaPool *arena, const SECItem *p,
                               const SECItem *q, const SECItem *x2,
                               const SECItem *gx4, const SECItem *x2s,
                               const SECItem *B, SECItem *K);

    /* Version 3.012 came to here */

    SECStatus (*p_TLS_P_hash)(HASH_HashType hashAlg,
                              const SECItem *secret,
                              const char *label,
                              SECItem *seed,
                              SECItem *result,
                              PRBool isFIPS);

    SHA224Context *(*p_SHA224_NewContext)(void);
    void (*p_SHA224_DestroyContext)(SHA224Context *cx, PRBool freeit);
    void (*p_SHA224_Begin)(SHA224Context *cx);
    void (*p_SHA224_Update)(SHA224Context *cx, const unsigned char *input,
                            unsigned int inputLen);
    void (*p_SHA224_End)(SHA224Context *cx, unsigned char *digest,
                         unsigned int *digestLen, unsigned int maxDigestLen);
    SECStatus (*p_SHA224_HashBuf)(unsigned char *dest, const unsigned char *src,
                                  PRUint32 src_length);
    SECStatus (*p_SHA224_Hash)(unsigned char *dest, const char *src);
    void (*p_SHA224_TraceState)(SHA224Context *cx);
    unsigned int (*p_SHA224_FlattenSize)(SHA224Context *cx);
    SECStatus (*p_SHA224_Flatten)(SHA224Context *cx, unsigned char *space);
    SHA224Context *(*p_SHA224_Resurrect)(unsigned char *space, void *arg);
    void (*p_SHA224_Clone)(SHA224Context *dest, SHA224Context *src);
    PRBool (*p_BLAPI_SHVerifyFile)(const char *name);

    /* Version 3.013 came to here */

    SECStatus (*p_PQG_ParamGenV2)(unsigned int L, unsigned int N,
                                  unsigned int seedBytes,
                                  PQGParams **pParams, PQGVerify **pVfy);
    SECStatus (*p_PRNGTEST_RunHealthTests)(void);

    /* Version 3.014 came to here */

    SECStatus (*p_HMAC_ConstantTime)(
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
        unsigned int bodyTotalLen);

    SECStatus (*p_SSLv3_MAC_ConstantTime)(
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
        unsigned int bodyTotalLen);

    /* Version 3.015 came to here */

    SECStatus (*p_RSA_SignRaw)(RSAPrivateKey *key,
                               unsigned char *output,
                               unsigned int *outputLen,
                               unsigned int maxOutputLen,
                               const unsigned char *input,
                               unsigned int inputLen);
    SECStatus (*p_RSA_CheckSignRaw)(RSAPublicKey *key,
                                    const unsigned char *sig,
                                    unsigned int sigLen,
                                    const unsigned char *hash,
                                    unsigned int hashLen);
    SECStatus (*p_RSA_CheckSignRecoverRaw)(RSAPublicKey *key,
                                           unsigned char *data,
                                           unsigned int *dataLen,
                                           unsigned int maxDataLen,
                                           const unsigned char *sig,
                                           unsigned int sigLen);
    SECStatus (*p_RSA_EncryptRaw)(RSAPublicKey *key,
                                  unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen);
    SECStatus (*p_RSA_DecryptRaw)(RSAPrivateKey *key,
                                  unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen);
    SECStatus (*p_RSA_EncryptOAEP)(RSAPublicKey *key,
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
                                   unsigned int inputLen);
    SECStatus (*p_RSA_DecryptOAEP)(RSAPrivateKey *key,
                                   HASH_HashType hashAlg,
                                   HASH_HashType maskHashAlg,
                                   const unsigned char *label,
                                   unsigned int labelLen,
                                   unsigned char *output,
                                   unsigned int *outputLen,
                                   unsigned int maxOutputLen,
                                   const unsigned char *input,
                                   unsigned int inputLen);
    SECStatus (*p_RSA_EncryptBlock)(RSAPublicKey *key,
                                    unsigned char *output,
                                    unsigned int *outputLen,
                                    unsigned int maxOutputLen,
                                    const unsigned char *input,
                                    unsigned int inputLen);
    SECStatus (*p_RSA_DecryptBlock)(RSAPrivateKey *key,
                                    unsigned char *output,
                                    unsigned int *outputLen,
                                    unsigned int maxOutputLen,
                                    const unsigned char *input,
                                    unsigned int inputLen);
    SECStatus (*p_RSA_SignPSS)(RSAPrivateKey *key,
                               HASH_HashType hashAlg,
                               HASH_HashType maskHashAlg,
                               const unsigned char *salt,
                               unsigned int saltLen,
                               unsigned char *output,
                               unsigned int *outputLen,
                               unsigned int maxOutputLen,
                               const unsigned char *input,
                               unsigned int inputLen);
    SECStatus (*p_RSA_CheckSignPSS)(RSAPublicKey *key,
                                    HASH_HashType hashAlg,
                                    HASH_HashType maskHashAlg,
                                    unsigned int saltLen,
                                    const unsigned char *sig,
                                    unsigned int sigLen,
                                    const unsigned char *hash,
                                    unsigned int hashLen);
    SECStatus (*p_RSA_Sign)(RSAPrivateKey *key,
                            unsigned char *output,
                            unsigned int *outputLen,
                            unsigned int maxOutputLen,
                            const unsigned char *input,
                            unsigned int inputLen);
    SECStatus (*p_RSA_CheckSign)(RSAPublicKey *key,
                                 const unsigned char *sig,
                                 unsigned int sigLen,
                                 const unsigned char *data,
                                 unsigned int dataLen);
    SECStatus (*p_RSA_CheckSignRecover)(RSAPublicKey *key,
                                        unsigned char *output,
                                        unsigned int *outputLen,
                                        unsigned int maxOutputLen,
                                        const unsigned char *sig,
                                        unsigned int sigLen);

    /* Version 3.016 came to here */

    SECStatus (*p_EC_FillParams)(PLArenaPool *arena,
                                 const SECItem *encodedParams, ECParams *params);
    SECStatus (*p_EC_DecodeParams)(const SECItem *encodedParams,
                                   ECParams **ecparams);
    SECStatus (*p_EC_CopyParams)(PLArenaPool *arena, ECParams *dstParams,
                                 const ECParams *srcParams);

    /* Version 3.017 came to here */

    SECStatus (*p_ChaCha20Poly1305_InitContext)(ChaCha20Poly1305Context *ctx,
                                                const unsigned char *key,
                                                unsigned int keyLen,
                                                unsigned int tagLen);

    ChaCha20Poly1305Context *(*p_ChaCha20Poly1305_CreateContext)(
        const unsigned char *key, unsigned int keyLen, unsigned int tagLen);

    void (*p_ChaCha20Poly1305_DestroyContext)(ChaCha20Poly1305Context *ctx,
                                              PRBool freeit);

    SECStatus (*p_ChaCha20Poly1305_Seal)(
        const ChaCha20Poly1305Context *ctx, unsigned char *output,
        unsigned int *outputLen, unsigned int maxOutputLen,
        const unsigned char *input, unsigned int inputLen,
        const unsigned char *nonce, unsigned int nonceLen,
        const unsigned char *ad, unsigned int adLen);

    SECStatus (*p_ChaCha20Poly1305_Open)(
        const ChaCha20Poly1305Context *ctx, unsigned char *output,
        unsigned int *outputLen, unsigned int maxOutputLen,
        const unsigned char *input, unsigned int inputLen,
        const unsigned char *nonce, unsigned int nonceLen,
        const unsigned char *ad, unsigned int adLen);

    /* Version 3.018 came to here */

    /* Add new function pointers at the end of this struct and bump
     * FREEBL_VERSION at the beginning of this file. */
};

typedef struct FREEBLVectorStr FREEBLVector;

#ifdef FREEBL_LOWHASH
#include "nsslowhash.h"

#define NSSLOW_VERSION 0x0300

struct NSSLOWVectorStr {
    unsigned short length;  /* of this struct in bytes */
    unsigned short version; /* of this struct. */
    const FREEBLVector *(*p_FREEBL_GetVector)(void);
    NSSLOWInitContext *(*p_NSSLOW_Init)(void);
    void (*p_NSSLOW_Shutdown)(NSSLOWInitContext *context);
    void (*p_NSSLOW_Reset)(NSSLOWInitContext *context);
    NSSLOWHASHContext *(*p_NSSLOWHASH_NewContext)(
        NSSLOWInitContext *initContext,
        HASH_HashType hashType);
    void (*p_NSSLOWHASH_Begin)(NSSLOWHASHContext *context);
    void (*p_NSSLOWHASH_Update)(NSSLOWHASHContext *context,
                                const unsigned char *buf,
                                unsigned int len);
    void (*p_NSSLOWHASH_End)(NSSLOWHASHContext *context,
                             unsigned char *buf,
                             unsigned int *ret, unsigned int len);
    void (*p_NSSLOWHASH_Destroy)(NSSLOWHASHContext *context);
    unsigned int (*p_NSSLOWHASH_Length)(NSSLOWHASHContext *context);
};

typedef struct NSSLOWVectorStr NSSLOWVector;
#endif

SEC_BEGIN_PROTOS

#ifdef FREEBL_LOWHASH
typedef const NSSLOWVector *NSSLOWGetVectorFn(void);

extern NSSLOWGetVectorFn NSSLOW_GetVector;
#endif

typedef const FREEBLVector *FREEBLGetVectorFn(void);

extern FREEBLGetVectorFn FREEBL_GetVector;

SEC_END_PROTOS

#endif
