/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Vendors should replace this header file with the file containing those
 * algorithms which have NIST algorithm Certificates.
 */

/* handle special cases. Classes require existing code to already be
 * in place for that class */
typedef enum {
    SFTKFIPSNone = 0,
    SFTKFIPSDH,  /* allow only specific primes */
    SFTKFIPSECC, /* not just keys but specific curves */
    SFTKFIPSAEAD /* single shot AEAD functions not allowed in FIPS mode */
} SFTKFIPSSpecialClass;

typedef struct SFTKFIPSAlgorithmListStr SFTKFIPSAlgorithmList;
struct SFTKFIPSAlgorithmListStr {
    CK_MECHANISM_TYPE type;
    CK_MECHANISM_INFO info;
    CK_ULONG step;
    SFTKFIPSSpecialClass special;
};

SFTKFIPSAlgorithmList sftk_fips_mechs[] = {
/* A sample set of algorithms to allow basic testing in our continous
 * testing infrastructure. The vendor version should replace this with
 * a version that matches their algorithm testing and security policy */
/* NOTE, This looks a lot like the PKCS #11 mechanism list in pkcs11.c, it
 * differs in the following ways:
 *    1) the addition of step and class elements to help restrict
 *       the supported key sizes and types.
 *    2) The mechanism flags are restricted to only those that map to
 *       fips approved operations.
 *    3) All key sizes are in bits, independent of mechanism.
 *    4) You can add more then one entry for the same mechanism to handle
 *       multiple descrete keys where the MIN/MAX/STEP semantics doesn't apply
 *       or where different operations have different key requirements.
 * This table does not encode all the modules legal FIPS semantics, only
 * those semantics that might possibly change due to algorithms dropping
 * of the security policy late in the process. */
/* handy common flag types */
#define CKF_KPG CKF_GENERATE_KEY_PAIR
#define CKF_GEN CKF_GENERATE
#define CKF_SGN (CKF_SIGN | CKF_VERIFY)
#define CKF_ENC (CKF_ENCRYPT | CKF_DECRYPT | CKF_WRAP | CKF_UNWRAP)
#define CKF_KEK (CKF_WRAP | CKF_UNWRAP)
#define CKF_KEA CKF_DERIVE
#define CKF_KDF CKF_DERIVE
#define CKF_HSH CKF_DIGEST
#define CK_MAX 0xffffffffUL
/* mechanisms using the same key types share the same key type
 * limits */
#define RSA_FB_KEY 2048, 4096 /* min, max */
#define RSA_FB_STEP 1024
#define DSA_FB_KEY 2048, 4096 /* min, max */
#define DSA_FB_STEP 1024
#define DH_FB_KEY 2048, 4096 /* min, max */
#define DH_FB_STEP 1024
#define EC_FB_KEY 256, 521 /* min, max */
#define EC_FB_STEP 1       /* key limits handled by special operation */
#define AES_FB_KEY 128, 256
#define AES_FB_STEP 64
    { CKM_RSA_PKCS_KEY_PAIR_GEN, { RSA_FB_KEY, CKF_KPG }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_RSA_PKCS_PSS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_RSA_PKCS_OAEP, { RSA_FB_KEY, CKF_ENC }, RSA_FB_STEP, SFTKFIPSNone },
    /* -------------- RSA Multipart Signing Operations -------------------- */
    { CKM_SHA224_RSA_PKCS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA256_RSA_PKCS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA384_RSA_PKCS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA512_RSA_PKCS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA224_RSA_PKCS_PSS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA256_RSA_PKCS_PSS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA384_RSA_PKCS_PSS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    { CKM_SHA512_RSA_PKCS_PSS, { RSA_FB_KEY, CKF_SGN }, RSA_FB_STEP, SFTKFIPSNone },
    /* ------------------------- DSA Operations --------------------------- */
    { CKM_DSA_KEY_PAIR_GEN, { DSA_FB_KEY, CKF_KPG }, DSA_FB_STEP, SFTKFIPSNone },
    { CKM_DSA, { DSA_FB_KEY, CKF_SGN }, DSA_FB_STEP, SFTKFIPSNone },
    { CKM_DSA_PARAMETER_GEN, { DSA_FB_KEY, CKF_KPG }, DSA_FB_STEP, SFTKFIPSNone },
    { CKM_DSA_SHA224, { DSA_FB_KEY, CKF_SGN }, DSA_FB_STEP, SFTKFIPSNone },
    { CKM_DSA_SHA256, { DSA_FB_KEY, CKF_SGN }, DSA_FB_STEP, SFTKFIPSNone },
    { CKM_DSA_SHA384, { DSA_FB_KEY, CKF_SGN }, DSA_FB_STEP, SFTKFIPSNone },
    { CKM_DSA_SHA512, { DSA_FB_KEY, CKF_SGN }, DSA_FB_STEP, SFTKFIPSNone },
    /* -------------------- Diffie Hellman Operations --------------------- */
    /* no diffie hellman yet */
    { CKM_DH_PKCS_KEY_PAIR_GEN, { DH_FB_KEY, CKF_KPG }, DH_FB_STEP, SFTKFIPSDH },
    { CKM_DH_PKCS_DERIVE, { DH_FB_KEY, CKF_KEA }, DH_FB_STEP, SFTKFIPSDH },
    /* -------------------- Elliptic Curve Operations --------------------- */
    { CKM_EC_KEY_PAIR_GEN, { EC_FB_KEY, CKF_KPG }, EC_FB_STEP, SFTKFIPSECC },
    { CKM_ECDH1_DERIVE, { EC_FB_KEY, CKF_KEA }, EC_FB_STEP, SFTKFIPSECC },
    { CKM_ECDSA, { EC_FB_KEY, CKF_SGN }, EC_FB_STEP, SFTKFIPSECC },
    { CKM_ECDSA_SHA224, { EC_FB_KEY, CKF_SGN }, EC_FB_STEP, SFTKFIPSECC },
    { CKM_ECDSA_SHA256, { EC_FB_KEY, CKF_SGN }, EC_FB_STEP, SFTKFIPSECC },
    { CKM_ECDSA_SHA384, { EC_FB_KEY, CKF_SGN }, EC_FB_STEP, SFTKFIPSECC },
    { CKM_ECDSA_SHA512, { EC_FB_KEY, CKF_SGN }, EC_FB_STEP, SFTKFIPSECC },
    /* ------------------------- RC2 Operations --------------------------- */
    /* ------------------------- AES Operations --------------------------- */
    { CKM_AES_KEY_GEN, { AES_FB_KEY, CKF_GEN }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_ECB, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_CBC, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_MAC, { AES_FB_KEY, CKF_SGN }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_MAC_GENERAL, { AES_FB_KEY, CKF_SGN }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_CMAC, { AES_FB_KEY, CKF_SGN }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_CMAC_GENERAL, { AES_FB_KEY, CKF_SGN }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_CBC_PAD, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_CTS, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_CTR, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_GCM, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSAEAD },
    { CKM_AES_KEY_WRAP, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_KEY_WRAP_PAD, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_KEY_WRAP_KWP, { AES_FB_KEY, CKF_ENC }, AES_FB_STEP, SFTKFIPSNone },
    { CKM_AES_XCBC_MAC_96, { 96, 96, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_AES_XCBC_MAC, { 128, 128, CKF_SGN }, 1, SFTKFIPSNone },
    /* ------------------------- Hashing Operations ----------------------- */
    { CKM_SHA224, { 0, 0, CKF_HSH }, 1, SFTKFIPSNone },
    { CKM_SHA224_HMAC, { 112, 224, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA224_HMAC_GENERAL, { 112, 224, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA256, { 0, 0, CKF_HSH }, 1, SFTKFIPSNone },
    { CKM_SHA256_HMAC, { 128, 256, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA256_HMAC_GENERAL, { 128, 256, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA384, { 0, 0, CKF_HSH }, 1, SFTKFIPSNone },
    { CKM_SHA384_HMAC, { 192, 384, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA384_HMAC_GENERAL, { 192, 384, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA512, { 0, 0, CKF_HSH }, 1, SFTKFIPSNone },
    { CKM_SHA512_HMAC, { 256, 512, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_SHA512_HMAC_GENERAL, { 256, 512, CKF_SGN }, 1, SFTKFIPSNone },
    /* --------------------- Secret Key Operations ------------------------ */
    { CKM_GENERIC_SECRET_KEY_GEN, { 8, 256, CKF_GEN }, 1, SFTKFIPSNone },
    /* ---------------------- SSL/TLS operations ------------------------- */
    { CKM_SHA224_KEY_DERIVATION, { 112, 224, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_SHA256_KEY_DERIVATION, { 128, 256, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_SHA384_KEY_DERIVATION, { 192, 284, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_SHA512_KEY_DERIVATION, { 256, 512, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_TLS12_MASTER_KEY_DERIVE, { 384, 384, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_TLS12_MASTER_KEY_DERIVE_DH, { DH_FB_KEY, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_TLS12_KEY_AND_MAC_DERIVE, { 384, 384, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_TLS_PRF_GENERAL, { 8, 512, CKF_SGN }, 1, SFTKFIPSNone },
    { CKM_TLS_MAC, { 8, 512, CKF_SGN }, 1, SFTKFIPSNone },
    /* sigh, is this algorithm really tested. ssl doesn't seem to have a
     * way of turning the extension off */
    { CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE, { 192, 1024, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_DH, { 192, 1024, CKF_DERIVE }, 1, SFTKFIPSNone },

    /* ------------------------- HKDF Operations -------------------------- */
    { CKM_HKDF_DERIVE, { 8, 255 * 64 * 8, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_HKDF_DATA, { 8, 255 * 64 * 8, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_HKDF_KEY_GEN, { 160, 224, CKF_GEN }, 1, SFTKFIPSNone },
    { CKM_HKDF_KEY_GEN, { 256, 512, CKF_GEN }, 128, SFTKFIPSNone },
    /* ------------------ NIST 800-108 Key Derivations  ------------------- */
    { CKM_SP800_108_COUNTER_KDF, { 0, CK_MAX, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_SP800_108_FEEDBACK_KDF, { 0, CK_MAX, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_SP800_108_DOUBLE_PIPELINE_KDF, { 0, CK_MAX, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_SP800_108_COUNTER_KDF_DERIVE_DATA, { 0, CK_MAX, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_SP800_108_FEEDBACK_KDF_DERIVE_DATA, { 0, CK_MAX, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_SP800_108_DOUBLE_PIPELINE_KDF_DERIVE_DATA, { 0, CK_MAX, CKF_KDF }, 1, SFTKFIPSNone },
    /* --------------------IPSEC ----------------------- */
    { CKM_NSS_IKE_PRF_PLUS_DERIVE, { 8, 255 * 64, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_IKE_PRF_DERIVE, { 8, 64, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_IKE1_PRF_DERIVE, { 8, 64, CKF_KDF }, 1, SFTKFIPSNone },
    { CKM_NSS_IKE1_APP_B_PRF_DERIVE, { 8, 255 * 64, CKF_KDF }, 1, SFTKFIPSNone },
    /* ------------------ PBE Key Derivations  ------------------- */
    { CKM_PKCS5_PBKD2, { 1, 256, CKF_GEN }, 1, SFTKFIPSNone },
    { CKM_NSS_PKCS12_PBE_SHA224_HMAC_KEY_GEN, { 224, 224, CKF_GEN }, 1, SFTKFIPSNone },
    { CKM_NSS_PKCS12_PBE_SHA256_HMAC_KEY_GEN, { 256, 256, CKF_GEN }, 1, SFTKFIPSNone },
    { CKM_NSS_PKCS12_PBE_SHA384_HMAC_KEY_GEN, { 384, 384, CKF_GEN }, 1, SFTKFIPSNone },
    { CKM_NSS_PKCS12_PBE_SHA512_HMAC_KEY_GEN, { 512, 512, CKF_GEN }, 1, SFTKFIPSNone }
};
const int SFTK_NUMBER_FIPS_ALGORITHMS = PR_ARRAY_SIZE(sftk_fips_mechs);
