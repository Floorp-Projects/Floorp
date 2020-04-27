/*
 * ldvector.c - platform dependent DSO containing freebl implementation.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
extern int FREEBL_InitStubs(void);
#endif

#include "loader.h"
#include "cmac.h"
#include "alghmac.h"
#include "hmacct.h"
#include "blapii.h"

static const struct FREEBLVectorStr vector =
    {

      sizeof vector,
      FREEBL_VERSION,

      RSA_NewKey,
      RSA_PublicKeyOp,
      RSA_PrivateKeyOp,
      DSA_NewKey,
      DSA_SignDigest,
      DSA_VerifyDigest,
      DSA_NewKeyFromSeed,
      DSA_SignDigestWithSeed,
      DH_GenParam,
      DH_NewKey,
      DH_Derive,
      KEA_Derive,
      KEA_Verify,
      RC4_CreateContext,
      RC4_DestroyContext,
      RC4_Encrypt,
      RC4_Decrypt,
      RC2_CreateContext,
      RC2_DestroyContext,
      RC2_Encrypt,
      RC2_Decrypt,
      RC5_CreateContext,
      RC5_DestroyContext,
      RC5_Encrypt,
      RC5_Decrypt,
      DES_CreateContext,
      DES_DestroyContext,
      DES_Encrypt,
      DES_Decrypt,
      AES_CreateContext,
      AES_DestroyContext,
      AES_Encrypt,
      AES_Decrypt,
      MD5_Hash,
      MD5_HashBuf,
      MD5_NewContext,
      MD5_DestroyContext,
      MD5_Begin,
      MD5_Update,
      MD5_End,
      MD5_FlattenSize,
      MD5_Flatten,
      MD5_Resurrect,
      MD5_TraceState,
      MD2_Hash,
      MD2_NewContext,
      MD2_DestroyContext,
      MD2_Begin,
      MD2_Update,
      MD2_End,
      MD2_FlattenSize,
      MD2_Flatten,
      MD2_Resurrect,
      SHA1_Hash,
      SHA1_HashBuf,
      SHA1_NewContext,
      SHA1_DestroyContext,
      SHA1_Begin,
      SHA1_Update,
      SHA1_End,
      SHA1_TraceState,
      SHA1_FlattenSize,
      SHA1_Flatten,
      SHA1_Resurrect,
      RNG_RNGInit,
      RNG_RandomUpdate,
      RNG_GenerateGlobalRandomBytes,
      RNG_RNGShutdown,
      PQG_ParamGen,
      PQG_ParamGenSeedLen,
      PQG_VerifyParams,

      /* End of Version 3.001. */

      RSA_PrivateKeyOpDoubleChecked,
      RSA_PrivateKeyCheck,
      BL_Cleanup,

      /* End of Version 3.002. */

      SHA256_NewContext,
      SHA256_DestroyContext,
      SHA256_Begin,
      SHA256_Update,
      SHA256_End,
      SHA256_HashBuf,
      SHA256_Hash,
      SHA256_TraceState,
      SHA256_FlattenSize,
      SHA256_Flatten,
      SHA256_Resurrect,

      SHA512_NewContext,
      SHA512_DestroyContext,
      SHA512_Begin,
      SHA512_Update,
      SHA512_End,
      SHA512_HashBuf,
      SHA512_Hash,
      SHA512_TraceState,
      SHA512_FlattenSize,
      SHA512_Flatten,
      SHA512_Resurrect,

      SHA384_NewContext,
      SHA384_DestroyContext,
      SHA384_Begin,
      SHA384_Update,
      SHA384_End,
      SHA384_HashBuf,
      SHA384_Hash,
      SHA384_TraceState,
      SHA384_FlattenSize,
      SHA384_Flatten,
      SHA384_Resurrect,

      /* End of Version 3.003. */

      AESKeyWrap_CreateContext,
      AESKeyWrap_DestroyContext,
      AESKeyWrap_Encrypt,
      AESKeyWrap_Decrypt,

      /* End of Version 3.004. */

      BLAPI_SHVerify,
      BLAPI_VerifySelf,

      /* End of Version 3.005. */

      EC_NewKey,
      EC_NewKeyFromSeed,
      EC_ValidatePublicKey,
      ECDH_Derive,
      ECDSA_SignDigest,
      ECDSA_VerifyDigest,
      ECDSA_SignDigestWithSeed,

      /* End of Version 3.006. */
      /* End of Version 3.007. */

      AES_InitContext,
      AESKeyWrap_InitContext,
      DES_InitContext,
      RC2_InitContext,
      RC4_InitContext,

      AES_AllocateContext,
      AESKeyWrap_AllocateContext,
      DES_AllocateContext,
      RC2_AllocateContext,
      RC4_AllocateContext,

      MD2_Clone,
      MD5_Clone,
      SHA1_Clone,
      SHA256_Clone,
      SHA384_Clone,
      SHA512_Clone,

      TLS_PRF,
      HASH_GetRawHashObject,

      HMAC_Create,
      HMAC_Init,
      HMAC_Begin,
      HMAC_Update,
      HMAC_Clone,
      HMAC_Finish,
      HMAC_Destroy,

      RNG_SystemInfoForRNG,

      /* End of Version 3.008. */

      FIPS186Change_GenerateX,
      FIPS186Change_ReduceModQForDSA,

      /* End of Version 3.009. */
      Camellia_InitContext,
      Camellia_AllocateContext,
      Camellia_CreateContext,
      Camellia_DestroyContext,
      Camellia_Encrypt,
      Camellia_Decrypt,

      PQG_DestroyParams,
      PQG_DestroyVerify,

      /* End of Version 3.010. */

      SEED_InitContext,
      SEED_AllocateContext,
      SEED_CreateContext,
      SEED_DestroyContext,
      SEED_Encrypt,
      SEED_Decrypt,

      BL_Init,
      BL_SetForkState,

      PRNGTEST_Instantiate,
      PRNGTEST_Reseed,
      PRNGTEST_Generate,

      PRNGTEST_Uninstantiate,

      /* End of Version 3.011. */

      RSA_PopulatePrivateKey,

      DSA_NewRandom,

      JPAKE_Sign,
      JPAKE_Verify,
      JPAKE_Round2,
      JPAKE_Final,

      /* End of Version 3.012 */

      TLS_P_hash,
      SHA224_NewContext,
      SHA224_DestroyContext,
      SHA224_Begin,
      SHA224_Update,
      SHA224_End,
      SHA224_HashBuf,
      SHA224_Hash,
      SHA224_TraceState,
      SHA224_FlattenSize,
      SHA224_Flatten,
      SHA224_Resurrect,
      SHA224_Clone,
      BLAPI_SHVerifyFile,

      /* End of Version 3.013 */

      PQG_ParamGenV2,
      PRNGTEST_RunHealthTests,

      /* End of Version 3.014 */

      HMAC_ConstantTime,
      SSLv3_MAC_ConstantTime,

      /* End of Version 3.015 */

      RSA_SignRaw,
      RSA_CheckSignRaw,
      RSA_CheckSignRecoverRaw,
      RSA_EncryptRaw,
      RSA_DecryptRaw,
      RSA_EncryptOAEP,
      RSA_DecryptOAEP,
      RSA_EncryptBlock,
      RSA_DecryptBlock,
      RSA_SignPSS,
      RSA_CheckSignPSS,
      RSA_Sign,
      RSA_CheckSign,
      RSA_CheckSignRecover,

      /* End of Version 3.016 */

      EC_FillParams,
      EC_DecodeParams,
      EC_CopyParams,

      /* End of Version 3.017 */

      ChaCha20Poly1305_InitContext,
      ChaCha20Poly1305_CreateContext,
      ChaCha20Poly1305_DestroyContext,
      ChaCha20Poly1305_Seal,
      ChaCha20Poly1305_Open,

      /* End of Version 3.018 */

      EC_GetPointSize,

      /* End of Version 3.019 */

      BLAKE2B_Hash,
      BLAKE2B_HashBuf,
      BLAKE2B_MAC_HashBuf,
      BLAKE2B_NewContext,
      BLAKE2B_DestroyContext,
      BLAKE2B_Begin,
      BLAKE2B_MAC_Begin,
      BLAKE2B_Update,
      BLAKE2B_End,
      BLAKE2B_FlattenSize,
      BLAKE2B_Flatten,
      BLAKE2B_Resurrect,

      /* End of Version 3.020 */

      ChaCha20_Xor,

      /* End of version 3.021 */

      CMAC_Init,
      CMAC_Create,
      CMAC_Begin,
      CMAC_Update,
      CMAC_Finish,
      CMAC_Destroy,

      /* End of version 3.022 */
      ChaCha20Poly1305_Encrypt,
      ChaCha20Poly1305_Decrypt,
      AES_AEAD,
      AESKeyWrap_EncryptKWP,
      AESKeyWrap_DecryptKWP

      /* End of version 3.023 */
    };

const FREEBLVector*
FREEBL_GetVector(void)
{
#ifdef FREEBL_NO_DEPEND
    SECStatus rv;
#endif

#define NSS_VERSION_VARIABLE __nss_freebl_version
#include "verref.h"

#ifdef FREEBL_NO_DEPEND
    /* this entry point is only valid if nspr and nss-util has been loaded */
    rv = FREEBL_InitStubs();
    if (rv != SECSuccess) {
        return NULL;
    }
#endif

#ifndef NSS_FIPS_DISABLED
    /* In FIPS mode make sure the Full self tests have been run before
     * continuing. */
    BL_POSTRan(PR_FALSE);
#endif

    return &vector;
}

#ifdef FREEBL_LOWHASH
static const struct NSSLOWVectorStr nssvector =
    {
      sizeof nssvector,
      NSSLOW_VERSION,
      FREEBL_GetVector,
      NSSLOW_Init,
      NSSLOW_Shutdown,
      NSSLOW_Reset,
      NSSLOWHASH_NewContext,
      NSSLOWHASH_Begin,
      NSSLOWHASH_Update,
      NSSLOWHASH_End,
      NSSLOWHASH_Destroy,
      NSSLOWHASH_Length
    };

const NSSLOWVector*
NSSLOW_GetVector(void)
{
    /* POST check and  stub init happens in FREEBL_GetVector() and
     * NSSLOW_Init() respectively */
    return &nssvector;
}
#endif
