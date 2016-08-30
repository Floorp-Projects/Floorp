/*
 * PKCS #11 FIPS Power-Up Self Test.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: fipstest.c,v 1.31 2012/06/28 17:55:06 rrelyea%redhat.com Exp $ */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapi.h"
#include "seccomon.h" /* Required for RSA and DSA. */
#include "secerr.h"
#include "prtypes.h"

#ifdef NSS_ENABLE_ECC
#include "ec.h" /* Required for ECDSA */
#endif

/*
 * different platforms have different ways of calling and initial entry point
 * when the dll/.so is loaded. Most platforms support either a posix pragma
 * or the GCC attribute. Some platforms suppor a pre-defined name, and some
 * platforms have a link line way of invoking this function.
 */

/* The pragma */
#if defined(USE_INIT_PRAGMA)
#pragma init(bl_startup_tests)
#endif

/* GCC Attribute */
#if defined(__GNUC__) && !defined(NSS_NO_INIT_SUPPORT)
#define INIT_FUNCTION __attribute__((constructor))
#else
#define INIT_FUNCTION
#endif

static void INIT_FUNCTION bl_startup_tests(void);

/* Windows pre-defined entry */
#if defined(XP_WIN) && !defined(NSS_NO_INIT_SUPPORT)
#include <windows.h>

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL, // handle to DLL module
    DWORD fdwReason,    // reason for calling function
    LPVOID lpReserved)  // reserved
{
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // Initialize once for each new process.
            // Return FALSE to fail DLL load.
            bl_startup_tests();
            break;

        case DLL_THREAD_ATTACH:
            // Do thread-specific initialization.
            break;

        case DLL_THREAD_DETACH:
            // Do thread-specific cleanup.
            break;

        case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
            break;
    }
    return TRUE; // Successful DLL_PROCESS_ATTACH.
}
#endif

/* insert other platform dependent init entry points here, or modify
 * the linker line */

/* FIPS preprocessor directives for RC2-ECB and RC2-CBC.        */
#define FIPS_RC2_KEY_LENGTH 5     /*  40-bits */
#define FIPS_RC2_ENCRYPT_LENGTH 8 /*  64-bits */
#define FIPS_RC2_DECRYPT_LENGTH 8 /*  64-bits */

/* FIPS preprocessor directives for RC4.                        */
#define FIPS_RC4_KEY_LENGTH 5     /*  40-bits */
#define FIPS_RC4_ENCRYPT_LENGTH 8 /*  64-bits */
#define FIPS_RC4_DECRYPT_LENGTH 8 /*  64-bits */

/* FIPS preprocessor directives for DES-ECB and DES-CBC.        */
#define FIPS_DES_ENCRYPT_LENGTH 8 /*  64-bits */
#define FIPS_DES_DECRYPT_LENGTH 8 /*  64-bits */

/* FIPS preprocessor directives for DES3-CBC and DES3-ECB.      */
#define FIPS_DES3_ENCRYPT_LENGTH 8 /*  64-bits */
#define FIPS_DES3_DECRYPT_LENGTH 8 /*  64-bits */

/* FIPS preprocessor directives for AES-ECB and AES-CBC.        */
#define FIPS_AES_BLOCK_SIZE 16     /* 128-bits */
#define FIPS_AES_ENCRYPT_LENGTH 16 /* 128-bits */
#define FIPS_AES_DECRYPT_LENGTH 16 /* 128-bits */
#define FIPS_AES_128_KEY_SIZE 16   /* 128-bits */
#define FIPS_AES_192_KEY_SIZE 24   /* 192-bits */
#define FIPS_AES_256_KEY_SIZE 32   /* 256-bits */

/* FIPS preprocessor directives for message digests             */
#define FIPS_KNOWN_HASH_MESSAGE_LENGTH 64 /* 512-bits */

/* FIPS preprocessor directives for RSA.                         */
#define FIPS_RSA_TYPE siBuffer
#define FIPS_RSA_PUBLIC_EXPONENT_LENGTH 3    /*   24-bits */
#define FIPS_RSA_PRIVATE_VERSION_LENGTH 1    /*    8-bits */
#define FIPS_RSA_MESSAGE_LENGTH 256          /* 2048-bits */
#define FIPS_RSA_COEFFICIENT_LENGTH 128      /* 1024-bits */
#define FIPS_RSA_PRIME0_LENGTH 128           /* 1024-bits */
#define FIPS_RSA_PRIME1_LENGTH 128           /* 1024-bits */
#define FIPS_RSA_EXPONENT0_LENGTH 128        /* 1024-bits */
#define FIPS_RSA_EXPONENT1_LENGTH 128        /* 1024-bits */
#define FIPS_RSA_PRIVATE_EXPONENT_LENGTH 256 /* 2048-bits */
#define FIPS_RSA_ENCRYPT_LENGTH 256          /* 2048-bits */
#define FIPS_RSA_DECRYPT_LENGTH 256          /* 2048-bits */
#define FIPS_RSA_SIGNATURE_LENGTH 256        /* 2048-bits */
#define FIPS_RSA_MODULUS_LENGTH 256          /* 2048-bits */

/* FIPS preprocessor directives for DSA.                        */
#define FIPS_DSA_TYPE siBuffer
#define FIPS_DSA_DIGEST_LENGTH 20    /*  160-bits */
#define FIPS_DSA_SUBPRIME_LENGTH 20  /*  160-bits */
#define FIPS_DSA_SIGNATURE_LENGTH 40 /*  320-bits */
#define FIPS_DSA_PRIME_LENGTH 128    /* 1024-bits */
#define FIPS_DSA_BASE_LENGTH 128     /* 1024-bits */

/* FIPS preprocessor directives for RNG.                        */
#define FIPS_RNG_XKEY_LENGTH 32 /* 256-bits */

static SECStatus
freebl_fips_DES3_PowerUpSelfTest(void)
{
    /* DES3 Known Key (56-bits). */
    static const PRUint8 des3_known_key[] = { "ANSI Triple-DES Key Data" };

    /* DES3-CBC Known Initialization Vector (64-bits). */
    static const PRUint8 des3_cbc_known_initialization_vector[] = { "Security" };

    /* DES3 Known Plaintext (64-bits). */
    static const PRUint8 des3_ecb_known_plaintext[] = { "Netscape" };
    static const PRUint8 des3_cbc_known_plaintext[] = { "Netscape" };

    /* DES3 Known Ciphertext (64-bits). */
    static const PRUint8 des3_ecb_known_ciphertext[] = {
        0x55, 0x8e, 0xad, 0x3c, 0xee, 0x49, 0x69, 0xbe
    };
    static const PRUint8 des3_cbc_known_ciphertext[] = {
        0x43, 0xdc, 0x6a, 0xc1, 0xaf, 0xa6, 0x32, 0xf5
    };

    /* DES3 variables. */
    PRUint8 des3_computed_ciphertext[FIPS_DES3_ENCRYPT_LENGTH];
    PRUint8 des3_computed_plaintext[FIPS_DES3_DECRYPT_LENGTH];
    DESContext *des3_context;
    unsigned int des3_bytes_encrypted;
    unsigned int des3_bytes_decrypted;
    SECStatus des3_status;

    /*******************************************************/
    /* DES3-ECB Single-Round Known Answer Encryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext(des3_known_key, NULL,
                                     NSS_DES_EDE3, PR_TRUE);

    if (des3_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    des3_status = DES_Encrypt(des3_context, des3_computed_ciphertext,
                              &des3_bytes_encrypted, FIPS_DES3_ENCRYPT_LENGTH,
                              des3_ecb_known_plaintext,
                              FIPS_DES3_DECRYPT_LENGTH);

    DES_DestroyContext(des3_context, PR_TRUE);

    if ((des3_status != SECSuccess) ||
        (des3_bytes_encrypted != FIPS_DES3_ENCRYPT_LENGTH) ||
        (PORT_Memcmp(des3_computed_ciphertext, des3_ecb_known_ciphertext,
                     FIPS_DES3_ENCRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /*******************************************************/
    /* DES3-ECB Single-Round Known Answer Decryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext(des3_known_key, NULL,
                                     NSS_DES_EDE3, PR_FALSE);

    if (des3_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    des3_status = DES_Decrypt(des3_context, des3_computed_plaintext,
                              &des3_bytes_decrypted, FIPS_DES3_DECRYPT_LENGTH,
                              des3_ecb_known_ciphertext,
                              FIPS_DES3_ENCRYPT_LENGTH);

    DES_DestroyContext(des3_context, PR_TRUE);

    if ((des3_status != SECSuccess) ||
        (des3_bytes_decrypted != FIPS_DES3_DECRYPT_LENGTH) ||
        (PORT_Memcmp(des3_computed_plaintext, des3_ecb_known_plaintext,
                     FIPS_DES3_DECRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /*******************************************************/
    /* DES3-CBC Single-Round Known Answer Encryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext(des3_known_key,
                                     des3_cbc_known_initialization_vector,
                                     NSS_DES_EDE3_CBC, PR_TRUE);

    if (des3_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    des3_status = DES_Encrypt(des3_context, des3_computed_ciphertext,
                              &des3_bytes_encrypted, FIPS_DES3_ENCRYPT_LENGTH,
                              des3_cbc_known_plaintext,
                              FIPS_DES3_DECRYPT_LENGTH);

    DES_DestroyContext(des3_context, PR_TRUE);

    if ((des3_status != SECSuccess) ||
        (des3_bytes_encrypted != FIPS_DES3_ENCRYPT_LENGTH) ||
        (PORT_Memcmp(des3_computed_ciphertext, des3_cbc_known_ciphertext,
                     FIPS_DES3_ENCRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /*******************************************************/
    /* DES3-CBC Single-Round Known Answer Decryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext(des3_known_key,
                                     des3_cbc_known_initialization_vector,
                                     NSS_DES_EDE3_CBC, PR_FALSE);

    if (des3_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    des3_status = DES_Decrypt(des3_context, des3_computed_plaintext,
                              &des3_bytes_decrypted, FIPS_DES3_DECRYPT_LENGTH,
                              des3_cbc_known_ciphertext,
                              FIPS_DES3_ENCRYPT_LENGTH);

    DES_DestroyContext(des3_context, PR_TRUE);

    if ((des3_status != SECSuccess) ||
        (des3_bytes_decrypted != FIPS_DES3_DECRYPT_LENGTH) ||
        (PORT_Memcmp(des3_computed_plaintext, des3_cbc_known_plaintext,
                     FIPS_DES3_DECRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    return (SECSuccess);
}

/* AES self-test for 128-bit, 192-bit, or 256-bit key sizes*/
static SECStatus
freebl_fips_AES_PowerUpSelfTest(int aes_key_size)
{
    /* AES Known Key (up to 256-bits). */
    static const PRUint8 aes_known_key[] =
        { "AES-128 RIJNDAELLEADNJIR 821-SEA" };

    /* AES-CBC Known Initialization Vector (128-bits). */
    static const PRUint8 aes_cbc_known_initialization_vector[] =
        { "SecurityytiruceS" };

    /* AES Known Plaintext (128-bits). (blocksize is 128-bits) */
    static const PRUint8 aes_known_plaintext[] = { "NetscapeepacsteN" };

    /* AES Known Ciphertext (128-bit key). */
    static const PRUint8 aes_ecb128_known_ciphertext[] = {
        0x3c, 0xa5, 0x96, 0xf3, 0x34, 0x6a, 0x96, 0xc1,
        0x03, 0x88, 0x16, 0x7b, 0x20, 0xbf, 0x35, 0x47
    };

    static const PRUint8 aes_cbc128_known_ciphertext[] = {
        0xcf, 0x15, 0x1d, 0x4f, 0x96, 0xe4, 0x4f, 0x63,
        0x15, 0x54, 0x14, 0x1d, 0x4e, 0xd8, 0xd5, 0xea
    };

    /* AES Known Ciphertext (192-bit key). */
    static const PRUint8 aes_ecb192_known_ciphertext[] = {
        0xa0, 0x18, 0x62, 0xed, 0x88, 0x19, 0xcb, 0x62,
        0x88, 0x1d, 0x4d, 0xfe, 0x84, 0x02, 0x89, 0x0e
    };

    static const PRUint8 aes_cbc192_known_ciphertext[] = {
        0x83, 0xf7, 0xa4, 0x76, 0xd1, 0x6f, 0x07, 0xbe,
        0x07, 0xbc, 0x43, 0x2f, 0x6d, 0xad, 0x29, 0xe1
    };

    /* AES Known Ciphertext (256-bit key). */
    static const PRUint8 aes_ecb256_known_ciphertext[] = {
        0xdb, 0xa6, 0x52, 0x01, 0x8a, 0x70, 0xae, 0x66,
        0x3a, 0x99, 0xd8, 0x95, 0x7f, 0xfb, 0x01, 0x67
    };

    static const PRUint8 aes_cbc256_known_ciphertext[] = {
        0x37, 0xea, 0x07, 0x06, 0x31, 0x1c, 0x59, 0x27,
        0xc5, 0xc5, 0x68, 0x71, 0x6e, 0x34, 0x40, 0x16
    };

    const PRUint8 *aes_ecb_known_ciphertext =
        (aes_key_size == FIPS_AES_128_KEY_SIZE) ? aes_ecb128_known_ciphertext : (aes_key_size == FIPS_AES_192_KEY_SIZE) ? aes_ecb192_known_ciphertext : aes_ecb256_known_ciphertext;

    const PRUint8 *aes_cbc_known_ciphertext =
        (aes_key_size == FIPS_AES_128_KEY_SIZE) ? aes_cbc128_known_ciphertext : (aes_key_size == FIPS_AES_192_KEY_SIZE) ? aes_cbc192_known_ciphertext : aes_cbc256_known_ciphertext;

    /* AES variables. */
    PRUint8 aes_computed_ciphertext[FIPS_AES_ENCRYPT_LENGTH];
    PRUint8 aes_computed_plaintext[FIPS_AES_DECRYPT_LENGTH];
    AESContext *aes_context;
    unsigned int aes_bytes_encrypted;
    unsigned int aes_bytes_decrypted;
    SECStatus aes_status;

    /*check if aes_key_size is 128, 192, or 256 bits */
    if ((aes_key_size != FIPS_AES_128_KEY_SIZE) &&
        (aes_key_size != FIPS_AES_192_KEY_SIZE) &&
        (aes_key_size != FIPS_AES_256_KEY_SIZE)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /******************************************************/
    /* AES-ECB Single-Round Known Answer Encryption Test: */
    /******************************************************/

    aes_context = AES_CreateContext(aes_known_key, NULL, NSS_AES, PR_TRUE,
                                    aes_key_size, FIPS_AES_BLOCK_SIZE);

    if (aes_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    aes_status = AES_Encrypt(aes_context, aes_computed_ciphertext,
                             &aes_bytes_encrypted, FIPS_AES_ENCRYPT_LENGTH,
                             aes_known_plaintext,
                             FIPS_AES_DECRYPT_LENGTH);

    AES_DestroyContext(aes_context, PR_TRUE);

    if ((aes_status != SECSuccess) ||
        (aes_bytes_encrypted != FIPS_AES_ENCRYPT_LENGTH) ||
        (PORT_Memcmp(aes_computed_ciphertext, aes_ecb_known_ciphertext,
                     FIPS_AES_ENCRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /******************************************************/
    /* AES-ECB Single-Round Known Answer Decryption Test: */
    /******************************************************/

    aes_context = AES_CreateContext(aes_known_key, NULL, NSS_AES, PR_FALSE,
                                    aes_key_size, FIPS_AES_BLOCK_SIZE);

    if (aes_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    aes_status = AES_Decrypt(aes_context, aes_computed_plaintext,
                             &aes_bytes_decrypted, FIPS_AES_DECRYPT_LENGTH,
                             aes_ecb_known_ciphertext,
                             FIPS_AES_ENCRYPT_LENGTH);

    AES_DestroyContext(aes_context, PR_TRUE);

    if ((aes_status != SECSuccess) ||
        (aes_bytes_decrypted != FIPS_AES_DECRYPT_LENGTH) ||
        (PORT_Memcmp(aes_computed_plaintext, aes_known_plaintext,
                     FIPS_AES_DECRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /******************************************************/
    /* AES-CBC Single-Round Known Answer Encryption Test. */
    /******************************************************/

    aes_context = AES_CreateContext(aes_known_key,
                                    aes_cbc_known_initialization_vector,
                                    NSS_AES_CBC, PR_TRUE, aes_key_size,
                                    FIPS_AES_BLOCK_SIZE);

    if (aes_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    aes_status = AES_Encrypt(aes_context, aes_computed_ciphertext,
                             &aes_bytes_encrypted, FIPS_AES_ENCRYPT_LENGTH,
                             aes_known_plaintext,
                             FIPS_AES_DECRYPT_LENGTH);

    AES_DestroyContext(aes_context, PR_TRUE);

    if ((aes_status != SECSuccess) ||
        (aes_bytes_encrypted != FIPS_AES_ENCRYPT_LENGTH) ||
        (PORT_Memcmp(aes_computed_ciphertext, aes_cbc_known_ciphertext,
                     FIPS_AES_ENCRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /******************************************************/
    /* AES-CBC Single-Round Known Answer Decryption Test. */
    /******************************************************/

    aes_context = AES_CreateContext(aes_known_key,
                                    aes_cbc_known_initialization_vector,
                                    NSS_AES_CBC, PR_FALSE, aes_key_size,
                                    FIPS_AES_BLOCK_SIZE);

    if (aes_context == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    aes_status = AES_Decrypt(aes_context, aes_computed_plaintext,
                             &aes_bytes_decrypted, FIPS_AES_DECRYPT_LENGTH,
                             aes_cbc_known_ciphertext,
                             FIPS_AES_ENCRYPT_LENGTH);

    AES_DestroyContext(aes_context, PR_TRUE);

    if ((aes_status != SECSuccess) ||
        (aes_bytes_decrypted != FIPS_AES_DECRYPT_LENGTH) ||
        (PORT_Memcmp(aes_computed_plaintext, aes_known_plaintext,
                     FIPS_AES_DECRYPT_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    return (SECSuccess);
}

/* Known Hash Message (512-bits).  Used for all hashes (incl. SHA-N [N>1]). */
static const PRUint8 known_hash_message[] = {
    "The test message for the MD2, MD5, and SHA-1 hashing algorithms."
};

/****************************************************/
/* Single Round HMAC SHA-X test                     */
/****************************************************/
static SECStatus
freebl_fips_HMAC(unsigned char *hmac_computed,
                 const PRUint8 *secret_key,
                 unsigned int secret_key_length,
                 const PRUint8 *message,
                 unsigned int message_length,
                 HASH_HashType hashAlg)
{
    SECStatus hmac_status = SECFailure;
    HMACContext *cx = NULL;
    SECHashObject *hashObj = NULL;
    unsigned int bytes_hashed = 0;

    hashObj = (SECHashObject *)HASH_GetRawHashObject(hashAlg);

    if (!hashObj)
        return (SECFailure);

    cx = HMAC_Create(hashObj, secret_key,
                     secret_key_length,
                     PR_TRUE); /* PR_TRUE for in FIPS mode */

    if (cx == NULL)
        return (SECFailure);

    HMAC_Begin(cx);
    HMAC_Update(cx, message, message_length);
    hmac_status = HMAC_Finish(cx, hmac_computed, &bytes_hashed,
                              hashObj->length);

    HMAC_Destroy(cx, PR_TRUE);

    return (hmac_status);
}

static SECStatus
freebl_fips_HMAC_PowerUpSelfTest(void)
{
    static const PRUint8 HMAC_known_secret_key[] = {
        "Firefox and ThunderBird are awesome!"
    };

    static const PRUint8 HMAC_known_secret_key_length = sizeof HMAC_known_secret_key;

    /* known SHA1 hmac (20 bytes) */
    static const PRUint8 known_SHA1_hmac[] = {
        0xd5, 0x85, 0xf6, 0x5b, 0x39, 0xfa, 0xb9, 0x05,
        0x3b, 0x57, 0x1d, 0x61, 0xe7, 0xb8, 0x84, 0x1e,
        0x5d, 0x0e, 0x1e, 0x11
    };

    /* known SHA224 hmac (28 bytes) */
    static const PRUint8 known_SHA224_hmac[] = {
        0x1c, 0xc3, 0x06, 0x8e, 0xce, 0x37, 0x68, 0xfb,
        0x1a, 0x82, 0x4a, 0xbe, 0x2b, 0x00, 0x51, 0xf8,
        0x9d, 0xb6, 0xe0, 0x90, 0x0d, 0x00, 0xc9, 0x64,
        0x9a, 0xb8, 0x98, 0x4e
    };

    /* known SHA256 hmac (32 bytes) */
    static const PRUint8 known_SHA256_hmac[] = {
        0x05, 0x75, 0x9a, 0x9e, 0x70, 0x5e, 0xe7, 0x44,
        0xe2, 0x46, 0x4b, 0x92, 0x22, 0x14, 0x22, 0xe0,
        0x1b, 0x92, 0x8a, 0x0c, 0xfe, 0xf5, 0x49, 0xe9,
        0xa7, 0x1b, 0x56, 0x7d, 0x1d, 0x29, 0x40, 0x48
    };

    /* known SHA384 hmac (48 bytes) */
    static const PRUint8 known_SHA384_hmac[] = {
        0xcd, 0x56, 0x14, 0xec, 0x05, 0x53, 0x06, 0x2b,
        0x7e, 0x9c, 0x8a, 0x18, 0x5e, 0xea, 0xf3, 0x91,
        0x33, 0xfb, 0x64, 0xf6, 0xe3, 0x9f, 0x89, 0x0b,
        0xaf, 0xbe, 0x83, 0x4d, 0x3f, 0x3c, 0x43, 0x4d,
        0x4a, 0x0c, 0x56, 0x98, 0xf8, 0xca, 0xb4, 0xaa,
        0x9a, 0xf4, 0x0a, 0xaf, 0x4f, 0x69, 0xca, 0x87
    };

    /* known SHA512 hmac (64 bytes) */
    static const PRUint8 known_SHA512_hmac[] = {
        0xf6, 0x0e, 0x97, 0x12, 0x00, 0x67, 0x6e, 0xb9,
        0x0c, 0xb2, 0x63, 0xf0, 0x60, 0xac, 0x75, 0x62,
        0x70, 0x95, 0x2a, 0x52, 0x22, 0xee, 0xdd, 0xd2,
        0x71, 0xb1, 0xe8, 0x26, 0x33, 0xd3, 0x13, 0x27,
        0xcb, 0xff, 0x44, 0xef, 0x87, 0x97, 0x16, 0xfb,
        0xd3, 0x0b, 0x48, 0xbe, 0x12, 0x4e, 0xda, 0xb1,
        0x89, 0x90, 0xfb, 0x06, 0x0c, 0xbe, 0xe5, 0xc4,
        0xff, 0x24, 0x37, 0x3d, 0xc7, 0xe4, 0xe4, 0x37
    };

    SECStatus hmac_status;
    PRUint8 hmac_computed[HASH_LENGTH_MAX];

    /***************************************************/
    /* HMAC SHA-1 Single-Round Known Answer HMAC Test. */
    /***************************************************/

    hmac_status = freebl_fips_HMAC(hmac_computed,
                                   HMAC_known_secret_key,
                                   HMAC_known_secret_key_length,
                                   known_hash_message,
                                   FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                   HASH_AlgSHA1);

    if ((hmac_status != SECSuccess) ||
        (PORT_Memcmp(hmac_computed, known_SHA1_hmac,
                     SHA1_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* HMAC SHA-224 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = freebl_fips_HMAC(hmac_computed,
                                   HMAC_known_secret_key,
                                   HMAC_known_secret_key_length,
                                   known_hash_message,
                                   FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                   HASH_AlgSHA224);

    if ((hmac_status != SECSuccess) ||
        (PORT_Memcmp(hmac_computed, known_SHA224_hmac,
                     SHA224_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* HMAC SHA-256 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = freebl_fips_HMAC(hmac_computed,
                                   HMAC_known_secret_key,
                                   HMAC_known_secret_key_length,
                                   known_hash_message,
                                   FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                   HASH_AlgSHA256);

    if ((hmac_status != SECSuccess) ||
        (PORT_Memcmp(hmac_computed, known_SHA256_hmac,
                     SHA256_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* HMAC SHA-384 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = freebl_fips_HMAC(hmac_computed,
                                   HMAC_known_secret_key,
                                   HMAC_known_secret_key_length,
                                   known_hash_message,
                                   FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                   HASH_AlgSHA384);

    if ((hmac_status != SECSuccess) ||
        (PORT_Memcmp(hmac_computed, known_SHA384_hmac,
                     SHA384_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* HMAC SHA-512 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = freebl_fips_HMAC(hmac_computed,
                                   HMAC_known_secret_key,
                                   HMAC_known_secret_key_length,
                                   known_hash_message,
                                   FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                   HASH_AlgSHA512);

    if ((hmac_status != SECSuccess) ||
        (PORT_Memcmp(hmac_computed, known_SHA512_hmac,
                     SHA512_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    return (SECSuccess);
}

static SECStatus
freebl_fips_SHA_PowerUpSelfTest(void)
{
    /* SHA-1 Known Digest Message (160-bits). */
    static const PRUint8 sha1_known_digest[] = {
        0x0a, 0x6d, 0x07, 0xba, 0x1e, 0xbd, 0x8a, 0x1b,
        0x72, 0xf6, 0xc7, 0x22, 0xf1, 0x27, 0x9f, 0xf0,
        0xe0, 0x68, 0x47, 0x7a
    };

    /* SHA-224 Known Digest Message (224-bits). */
    static const PRUint8 sha224_known_digest[] = {
        0x89, 0x5e, 0x7f, 0xfd, 0x0e, 0xd8, 0x35, 0x6f,
        0x64, 0x6d, 0xf2, 0xde, 0x5e, 0xed, 0xa6, 0x7f,
        0x29, 0xd1, 0x12, 0x73, 0x42, 0x84, 0x95, 0x4f,
        0x8e, 0x08, 0xe5, 0xcb
    };

    /* SHA-256 Known Digest Message (256-bits). */
    static const PRUint8 sha256_known_digest[] = {
        0x38, 0xa9, 0xc1, 0xf0, 0x35, 0xf6, 0x5d, 0x61,
        0x11, 0xd4, 0x0b, 0xdc, 0xce, 0x35, 0x14, 0x8d,
        0xf2, 0xdd, 0xaf, 0xaf, 0xcf, 0xb7, 0x87, 0xe9,
        0x96, 0xa5, 0xd2, 0x83, 0x62, 0x46, 0x56, 0x79
    };

    /* SHA-384 Known Digest Message (384-bits). */
    static const PRUint8 sha384_known_digest[] = {
        0x11, 0xfe, 0x1c, 0x00, 0x89, 0x48, 0xde, 0xb3,
        0x99, 0xee, 0x1c, 0x18, 0xb4, 0x10, 0xfb, 0xfe,
        0xe3, 0xa8, 0x2c, 0xf3, 0x04, 0xb0, 0x2f, 0xc8,
        0xa3, 0xc4, 0x5e, 0xea, 0x7e, 0x60, 0x48, 0x7b,
        0xce, 0x2c, 0x62, 0xf7, 0xbc, 0xa7, 0xe8, 0xa3,
        0xcf, 0x24, 0xce, 0x9c, 0xe2, 0x8b, 0x09, 0x72
    };

    /* SHA-512 Known Digest Message (512-bits). */
    static const PRUint8 sha512_known_digest[] = {
        0xc8, 0xb3, 0x27, 0xf9, 0x0b, 0x24, 0xc8, 0xbf,
        0x4c, 0xba, 0x33, 0x54, 0xf2, 0x31, 0xbf, 0xdb,
        0xab, 0xfd, 0xb3, 0x15, 0xd7, 0xfa, 0x48, 0x99,
        0x07, 0x60, 0x0f, 0x57, 0x41, 0x1a, 0xdd, 0x28,
        0x12, 0x55, 0x25, 0xac, 0xba, 0x3a, 0x99, 0x12,
        0x2c, 0x7a, 0x8f, 0x75, 0x3a, 0xe1, 0x06, 0x6f,
        0x30, 0x31, 0xc9, 0x33, 0xc6, 0x1b, 0x90, 0x1a,
        0x6c, 0x98, 0x9a, 0x87, 0xd0, 0xb2, 0xf8, 0x07
    };

    /* SHA-X variables. */
    PRUint8 sha_computed_digest[HASH_LENGTH_MAX];
    SECStatus sha_status;

    /*************************************************/
    /* SHA-1 Single-Round Known Answer Hashing Test. */
    /*************************************************/

    sha_status = SHA1_HashBuf(sha_computed_digest, known_hash_message,
                              FIPS_KNOWN_HASH_MESSAGE_LENGTH);

    if ((sha_status != SECSuccess) ||
        (PORT_Memcmp(sha_computed_digest, sha1_known_digest,
                     SHA1_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* SHA-224 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA224_HashBuf(sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH);

    if ((sha_status != SECSuccess) ||
        (PORT_Memcmp(sha_computed_digest, sha224_known_digest,
                     SHA224_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* SHA-256 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA256_HashBuf(sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH);

    if ((sha_status != SECSuccess) ||
        (PORT_Memcmp(sha_computed_digest, sha256_known_digest,
                     SHA256_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* SHA-384 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA384_HashBuf(sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH);

    if ((sha_status != SECSuccess) ||
        (PORT_Memcmp(sha_computed_digest, sha384_known_digest,
                     SHA384_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /***************************************************/
    /* SHA-512 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA512_HashBuf(sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH);

    if ((sha_status != SECSuccess) ||
        (PORT_Memcmp(sha_computed_digest, sha512_known_digest,
                     SHA512_LENGTH) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    return (SECSuccess);
}

static SECStatus
freebl_fips_RSA_PowerUpSelfTest(void)
{
    /* RSA Known Modulus used in both Public/Private Key Values (2048-bits). */
    static const PRUint8 rsa_modulus[FIPS_RSA_MODULUS_LENGTH] = {
        0xb8, 0x15, 0x00, 0x33, 0xda, 0x0c, 0x9d, 0xa5,
        0x14, 0x8c, 0xde, 0x1f, 0x23, 0x07, 0x54, 0xe2,
        0xc6, 0xb9, 0x51, 0x04, 0xc9, 0x65, 0x24, 0x6e,
        0x0a, 0x46, 0x34, 0x5c, 0x37, 0x86, 0x6b, 0x88,
        0x24, 0x27, 0xac, 0xa5, 0x02, 0x79, 0xfb, 0xed,
        0x75, 0xc5, 0x3f, 0x6e, 0xdf, 0x05, 0x5f, 0x0f,
        0x20, 0x70, 0xa0, 0x5b, 0x85, 0xdb, 0xac, 0xb9,
        0x5f, 0x02, 0xc2, 0x64, 0x1e, 0x84, 0x5b, 0x3e,
        0xad, 0xbf, 0xf6, 0x2e, 0x51, 0xd6, 0xad, 0xf7,
        0xa7, 0x86, 0x75, 0x86, 0xec, 0xa7, 0xe1, 0xf7,
        0x08, 0xbf, 0xdc, 0x56, 0xb1, 0x3b, 0xca, 0xd8,
        0xfc, 0x51, 0xdf, 0x9a, 0x2a, 0x37, 0x06, 0xf2,
        0xd1, 0x6b, 0x9a, 0x5e, 0x2a, 0xe5, 0x20, 0x57,
        0x35, 0x9f, 0x1f, 0x98, 0xcf, 0x40, 0xc7, 0xd6,
        0x98, 0xdb, 0xde, 0xf5, 0x64, 0x53, 0xf7, 0x9d,
        0x45, 0xf3, 0xd6, 0x78, 0xb9, 0xe3, 0xa3, 0x20,
        0xcd, 0x79, 0x43, 0x35, 0xef, 0xd7, 0xfb, 0xb9,
        0x80, 0x88, 0x27, 0x2f, 0x63, 0xa8, 0x67, 0x3d,
        0x4a, 0xfa, 0x06, 0xc6, 0xd2, 0x86, 0x0b, 0xa7,
        0x28, 0xfd, 0xe0, 0x1e, 0x93, 0x4b, 0x17, 0x2e,
        0xb0, 0x11, 0x6f, 0xc6, 0x2b, 0x98, 0x0f, 0x15,
        0xe3, 0x87, 0x16, 0x7a, 0x7c, 0x67, 0x3e, 0x12,
        0x2b, 0xf8, 0xbe, 0x48, 0xc1, 0x97, 0x47, 0xf4,
        0x1f, 0x81, 0x80, 0x12, 0x28, 0xe4, 0x7b, 0x1e,
        0xb7, 0x00, 0xa4, 0xde, 0xaa, 0xfb, 0x0f, 0x77,
        0x84, 0xa3, 0xd6, 0xb2, 0x03, 0x48, 0xdd, 0x53,
        0x8b, 0x46, 0x41, 0x28, 0x52, 0xc4, 0x53, 0xf0,
        0x1c, 0x95, 0xd9, 0x36, 0xe0, 0x0f, 0x26, 0x46,
        0x9c, 0x61, 0x0e, 0x80, 0xca, 0x86, 0xaf, 0x39,
        0x95, 0xe5, 0x60, 0x43, 0x61, 0x3e, 0x2b, 0xb4,
        0xe8, 0xbd, 0x8d, 0x77, 0x62, 0xf5, 0x32, 0x43,
        0x2f, 0x4b, 0x65, 0x82, 0x14, 0xdd, 0x29, 0x5b
    };

    /* RSA Known Public Key Values (24-bits). */
    static const PRUint8 rsa_public_exponent[FIPS_RSA_PUBLIC_EXPONENT_LENGTH] = { 0x01, 0x00, 0x01 };
    /* RSA Known Private Key Values (version                 is    8-bits), */
    /*                              (private exponent        is 2048-bits), */
    /*                              (private prime0          is 1024-bits), */
    /*                              (private prime1          is 1024-bits), */
    /*                              (private prime exponent0 is 1024-bits), */
    /*                              (private prime exponent1 is 1024-bits), */
    /*                          and (private coefficient     is 1024-bits). */
    static const PRUint8 rsa_version[] = { 0x00 };

    static const PRUint8 rsa_private_exponent[FIPS_RSA_PRIVATE_EXPONENT_LENGTH] = {
        0x29, 0x08, 0x05, 0x53, 0x89, 0x76, 0xe6, 0x6c,
        0xb5, 0x77, 0xf0, 0xca, 0xdf, 0xf3, 0xf2, 0x67,
        0xda, 0x03, 0xd4, 0x9b, 0x4c, 0x88, 0xce, 0xe5,
        0xf8, 0x44, 0x4d, 0xc7, 0x80, 0x58, 0xe5, 0xff,
        0x22, 0x8f, 0xf5, 0x5b, 0x92, 0x81, 0xbe, 0x35,
        0xdf, 0xda, 0x67, 0x99, 0x3e, 0xfc, 0xe3, 0x83,
        0x6b, 0xa7, 0xaf, 0x16, 0xb7, 0x6f, 0x8f, 0xc0,
        0x81, 0xfd, 0x0b, 0x77, 0x65, 0x95, 0xfb, 0x00,
        0xad, 0x99, 0xec, 0x35, 0xc6, 0xe8, 0x23, 0x3e,
        0xe0, 0x88, 0x88, 0x09, 0xdb, 0x16, 0x50, 0xb7,
        0xcf, 0xab, 0x74, 0x61, 0x9e, 0x7f, 0xc5, 0x67,
        0x38, 0x56, 0xc7, 0x90, 0x85, 0x78, 0x5e, 0x84,
        0x21, 0x49, 0xea, 0xce, 0xb2, 0xa0, 0xff, 0xe4,
        0x70, 0x7f, 0x57, 0x7b, 0xa8, 0x36, 0xb8, 0x54,
        0x8d, 0x1d, 0xf5, 0x44, 0x9d, 0x68, 0x59, 0xf9,
        0x24, 0x6e, 0x85, 0x8f, 0xc3, 0x5f, 0x8a, 0x2c,
        0x94, 0xb7, 0xbc, 0x0e, 0xa5, 0xef, 0x93, 0x06,
        0x38, 0xcd, 0x07, 0x0c, 0xae, 0xb8, 0x44, 0x1a,
        0xd8, 0xe7, 0xf5, 0x9a, 0x1e, 0x9c, 0x18, 0xc7,
        0x6a, 0xc2, 0x7f, 0x28, 0x01, 0x4f, 0xb4, 0xb8,
        0x90, 0x97, 0x5a, 0x43, 0x38, 0xad, 0xe8, 0x95,
        0x68, 0x83, 0x1a, 0x1b, 0x10, 0x07, 0xe6, 0x02,
        0x52, 0x1f, 0xbf, 0x76, 0x6b, 0x46, 0xd6, 0xfb,
        0xc3, 0xbe, 0xb5, 0xac, 0x52, 0x53, 0x01, 0x1c,
        0xf3, 0xc5, 0xeb, 0x64, 0xf2, 0x1e, 0xc4, 0x38,
        0xe9, 0xaa, 0xd9, 0xc3, 0x72, 0x51, 0xa5, 0x44,
        0x58, 0x69, 0x0b, 0x1b, 0x98, 0x7f, 0xf2, 0x23,
        0xff, 0xeb, 0xf0, 0x75, 0x24, 0xcf, 0xc5, 0x1e,
        0xb8, 0x6a, 0xc5, 0x2f, 0x4f, 0x23, 0x50, 0x7d,
        0x15, 0x9d, 0x19, 0x7a, 0x0b, 0x82, 0xe0, 0x21,
        0x5b, 0x5f, 0x9d, 0x50, 0x2b, 0x83, 0xe4, 0x48,
        0xcc, 0x39, 0xe5, 0xfb, 0x13, 0x7b, 0x6f, 0x81
    };

    static const PRUint8 rsa_prime0[FIPS_RSA_PRIME0_LENGTH] = {
        0xe4, 0xbf, 0x21, 0x62, 0x9b, 0xa9, 0x77, 0x40,
        0x8d, 0x2a, 0xce, 0xa1, 0x67, 0x5a, 0x4c, 0x96,
        0x45, 0x98, 0x67, 0xbd, 0x75, 0x22, 0x33, 0x6f,
        0xe6, 0xcb, 0x77, 0xde, 0x9e, 0x97, 0x7d, 0x96,
        0x8c, 0x5e, 0x5d, 0x34, 0xfb, 0x27, 0xfc, 0x6d,
        0x74, 0xdb, 0x9d, 0x2e, 0x6d, 0xf6, 0xea, 0xfc,
        0xce, 0x9e, 0xda, 0xa7, 0x25, 0xa2, 0xf4, 0x58,
        0x6d, 0x0a, 0x3f, 0x01, 0xc2, 0xb4, 0xab, 0x38,
        0xc1, 0x14, 0x85, 0xb6, 0xfa, 0x94, 0xc3, 0x85,
        0xf9, 0x3c, 0x2e, 0x96, 0x56, 0x01, 0xe7, 0xd6,
        0x14, 0x71, 0x4f, 0xfb, 0x4c, 0x85, 0x52, 0xc4,
        0x61, 0x1e, 0xa5, 0x1e, 0x96, 0x13, 0x0d, 0x8f,
        0x66, 0xae, 0xa0, 0xcd, 0x7d, 0x25, 0x66, 0x19,
        0x15, 0xc2, 0xcf, 0xc3, 0x12, 0x3c, 0xe8, 0xa4,
        0x52, 0x4c, 0xcb, 0x28, 0x3c, 0xc4, 0xbf, 0x95,
        0x33, 0xe3, 0x81, 0xea, 0x0c, 0x6c, 0xa2, 0x05
    };
    static const PRUint8 rsa_prime1[FIPS_RSA_PRIME1_LENGTH] = {
        0xce, 0x03, 0x94, 0xf4, 0xa9, 0x2c, 0x1e, 0x06,
        0xe7, 0x40, 0x30, 0x01, 0xf7, 0xbb, 0x68, 0x8c,
        0x27, 0xd2, 0x15, 0xe3, 0x28, 0x49, 0x5b, 0xa8,
        0xc1, 0x9a, 0x42, 0x7e, 0x31, 0xf9, 0x08, 0x34,
        0x81, 0xa2, 0x0f, 0x04, 0x61, 0x34, 0xe3, 0x36,
        0x92, 0xb1, 0x09, 0x2b, 0xe9, 0xef, 0x84, 0x88,
        0xbe, 0x9c, 0x98, 0x60, 0xa6, 0x60, 0x84, 0xe9,
        0x75, 0x6f, 0xcc, 0x81, 0xd1, 0x96, 0xef, 0xdd,
        0x2e, 0xca, 0xc4, 0xf5, 0x42, 0xfb, 0x13, 0x2b,
        0x57, 0xbf, 0x14, 0x5e, 0xc2, 0x7f, 0x77, 0x35,
        0x29, 0xc4, 0xe5, 0xe0, 0xf9, 0x6d, 0x15, 0x4a,
        0x42, 0x56, 0x1c, 0x3e, 0x0c, 0xc5, 0xce, 0x70,
        0x08, 0x63, 0x1e, 0x73, 0xdb, 0x7e, 0x74, 0x05,
        0x32, 0x01, 0xc6, 0x36, 0x32, 0x75, 0x6b, 0xed,
        0x9d, 0xfe, 0x7c, 0x7e, 0xa9, 0x57, 0xb4, 0xe9,
        0x22, 0xe4, 0xe7, 0xfe, 0x36, 0x07, 0x9b, 0xdf
    };
    static const PRUint8 rsa_exponent0[FIPS_RSA_EXPONENT0_LENGTH] = {
        0x04, 0x5a, 0x3a, 0xa9, 0x64, 0xaa, 0xd9, 0xd1,
        0x09, 0x9e, 0x99, 0xe5, 0xea, 0x50, 0x86, 0x8a,
        0x89, 0x72, 0x77, 0xee, 0xdb, 0xee, 0xb5, 0xa9,
        0xd8, 0x6b, 0x60, 0xb1, 0x84, 0xb4, 0xff, 0x37,
        0xc1, 0x1d, 0xfe, 0x8a, 0x06, 0x89, 0x61, 0x3d,
        0x37, 0xef, 0x01, 0xd3, 0xa3, 0x56, 0x02, 0x6c,
        0xa3, 0x05, 0xd4, 0xc5, 0x3f, 0x6b, 0x15, 0x59,
        0x25, 0x61, 0xff, 0x86, 0xea, 0x0c, 0x84, 0x01,
        0x85, 0x72, 0xfd, 0x84, 0x58, 0xca, 0x41, 0xda,
        0x27, 0xbe, 0xe4, 0x68, 0x09, 0xe4, 0xe9, 0x63,
        0x62, 0x6a, 0x31, 0x8a, 0x67, 0x8f, 0x55, 0xde,
        0xd4, 0xb6, 0x3f, 0x90, 0x10, 0x6c, 0xf6, 0x62,
        0x17, 0x23, 0x15, 0x7e, 0x33, 0x76, 0x65, 0xb5,
        0xee, 0x7b, 0x11, 0x76, 0xf5, 0xbe, 0xe0, 0xf2,
        0x57, 0x7a, 0x8c, 0x97, 0x0c, 0x68, 0xf5, 0xf8,
        0x41, 0xcf, 0x7f, 0x66, 0x53, 0xac, 0x31, 0x7d
    };
    static const PRUint8 rsa_exponent1[FIPS_RSA_EXPONENT1_LENGTH] = {
        0x93, 0x54, 0x14, 0x6e, 0x73, 0x9d, 0x4d, 0x4b,
        0xfa, 0x8c, 0xf8, 0xc8, 0x2f, 0x76, 0x22, 0xea,
        0x38, 0x80, 0x11, 0x8f, 0x05, 0xfc, 0x90, 0x44,
        0x3b, 0x50, 0x2a, 0x45, 0x3d, 0x4f, 0xaf, 0x02,
        0x7d, 0xc2, 0x7b, 0xa2, 0xd2, 0x31, 0x94, 0x5c,
        0x2e, 0xc3, 0xd4, 0x9f, 0x47, 0x09, 0x37, 0x6a,
        0xe3, 0x85, 0xf1, 0xa3, 0x0c, 0xd8, 0xf1, 0xb4,
        0x53, 0x7b, 0xc4, 0x71, 0x02, 0x86, 0x42, 0xbb,
        0x96, 0xff, 0x03, 0xa3, 0xb2, 0x67, 0x03, 0xea,
        0x77, 0x31, 0xfb, 0x4b, 0x59, 0x24, 0xf7, 0x07,
        0x59, 0xfb, 0xa9, 0xba, 0x1e, 0x26, 0x58, 0x97,
        0x66, 0xa1, 0x56, 0x49, 0x39, 0xb1, 0x2c, 0x55,
        0x0a, 0x6a, 0x78, 0x18, 0xba, 0xdb, 0xcf, 0xf4,
        0xf7, 0x32, 0x35, 0xa2, 0x04, 0xab, 0xdc, 0xa7,
        0x6d, 0xd9, 0xd5, 0x06, 0x6f, 0xec, 0x7d, 0x40,
        0x4c, 0xe8, 0x0e, 0xd0, 0xc9, 0xaa, 0xdf, 0x59
    };
    static const PRUint8 rsa_coefficient[FIPS_RSA_COEFFICIENT_LENGTH] = {
        0x17, 0xd7, 0xf5, 0x0a, 0xf0, 0x68, 0x97, 0x96,
        0xc4, 0x29, 0x18, 0x77, 0x9a, 0x1f, 0xe3, 0xf3,
        0x12, 0x13, 0x0f, 0x7e, 0x7b, 0xb9, 0xc1, 0x91,
        0xf9, 0xc7, 0x08, 0x56, 0x5c, 0xa4, 0xbc, 0x83,
        0x71, 0xf9, 0x78, 0xd9, 0x2b, 0xec, 0xfe, 0x6b,
        0xdc, 0x2f, 0x63, 0xc9, 0xcd, 0x50, 0x14, 0x5b,
        0xd3, 0x6e, 0x85, 0x4d, 0x0c, 0xa2, 0x0b, 0xa0,
        0x09, 0xb6, 0xca, 0x34, 0x9c, 0xc2, 0xc1, 0x4a,
        0xb0, 0xbc, 0x45, 0x93, 0xa5, 0x7e, 0x99, 0xb5,
        0xbd, 0xe4, 0x69, 0x29, 0x08, 0x28, 0xd2, 0xcd,
        0xab, 0x24, 0x78, 0x48, 0x41, 0x26, 0x0b, 0x37,
        0xa3, 0x43, 0xd1, 0x95, 0x1a, 0xd6, 0xee, 0x22,
        0x1c, 0x00, 0x0b, 0xc2, 0xb7, 0xa4, 0xa3, 0x21,
        0xa9, 0xcd, 0xe4, 0x69, 0xd3, 0x45, 0x02, 0xb1,
        0xb7, 0x3a, 0xbf, 0x51, 0x35, 0x1b, 0x78, 0xc2,
        0xcf, 0x0c, 0x0d, 0x60, 0x09, 0xa9, 0x44, 0x02
    };

    /* RSA Known Plaintext Message (1024-bits). */
    static const PRUint8 rsa_known_plaintext_msg[FIPS_RSA_MESSAGE_LENGTH] = {
        "Known plaintext message utilized"
        "for RSA Encryption &  Decryption"
        "blocks SHA256, SHA384  and      "
        "SHA512 RSA Signature KAT tests. "
        "Known plaintext message utilized"
        "for RSA Encryption &  Decryption"
        "blocks SHA256, SHA384  and      "
        "SHA512 RSA Signature KAT  tests."
    };

    /* RSA Known Ciphertext (2048-bits). */
    static const PRUint8 rsa_known_ciphertext[] = {
        0x04, 0x12, 0x46, 0xe3, 0x6a, 0xee, 0xde, 0xdd,
        0x49, 0xa1, 0xd9, 0x83, 0xf7, 0x35, 0xf9, 0x70,
        0x88, 0x03, 0x2d, 0x01, 0x8b, 0xd1, 0xbf, 0xdb,
        0xe5, 0x1c, 0x85, 0xbe, 0xb5, 0x0b, 0x48, 0x45,
        0x7a, 0xf0, 0xa0, 0xe3, 0xa2, 0xbb, 0x4b, 0xf6,
        0x27, 0xd0, 0x1b, 0x12, 0xe3, 0x77, 0x52, 0x34,
        0x9e, 0x8e, 0x03, 0xd2, 0xf8, 0x79, 0x6e, 0x39,
        0x79, 0x53, 0x3c, 0x44, 0x14, 0x94, 0xbb, 0x8d,
        0xaa, 0x14, 0x44, 0xa0, 0x7b, 0xa5, 0x8c, 0x93,
        0x5f, 0x99, 0xa4, 0xa3, 0x6e, 0x7a, 0x38, 0x40,
        0x78, 0xfa, 0x36, 0x91, 0x5e, 0x9a, 0x9c, 0xba,
        0x1e, 0xd4, 0xf9, 0xda, 0x4b, 0x0f, 0xa8, 0xa3,
        0x1c, 0xf3, 0x3a, 0xd1, 0xa5, 0xb4, 0x51, 0x16,
        0xed, 0x4b, 0xcf, 0xec, 0x93, 0x7b, 0x90, 0x21,
        0xbc, 0x3a, 0xf4, 0x0b, 0xd1, 0x3a, 0x2b, 0xba,
        0xa6, 0x7d, 0x5b, 0x53, 0xd8, 0x64, 0xf9, 0x29,
        0x7b, 0x7f, 0x77, 0x3e, 0x51, 0x4c, 0x9a, 0x94,
        0xd2, 0x4b, 0x4a, 0x8d, 0x61, 0x74, 0x97, 0xae,
        0x53, 0x6a, 0xf4, 0x90, 0xc2, 0x2c, 0x49, 0xe2,
        0xfa, 0xeb, 0x91, 0xc5, 0xe5, 0x83, 0x13, 0xc9,
        0x44, 0x4b, 0x95, 0x2c, 0x57, 0x70, 0x15, 0x5c,
        0x64, 0x8d, 0x1a, 0xfd, 0x2a, 0xc7, 0xb2, 0x9c,
        0x5c, 0x99, 0xd3, 0x4a, 0xfd, 0xdd, 0xf6, 0x82,
        0x87, 0x8c, 0x5a, 0xc4, 0xa8, 0x0d, 0x2a, 0xef,
        0xc3, 0xa2, 0x7e, 0x8e, 0x67, 0x9f, 0x6f, 0x63,
        0xdb, 0xbb, 0x1d, 0x31, 0xc4, 0xbb, 0xbc, 0x13,
        0x3f, 0x54, 0xc6, 0xf6, 0xc5, 0x28, 0x32, 0xab,
        0x96, 0x42, 0x10, 0x36, 0x40, 0x92, 0xbb, 0x57,
        0x55, 0x38, 0xf5, 0x43, 0x7e, 0x43, 0xc4, 0x65,
        0x47, 0x64, 0xaa, 0x0f, 0x4c, 0xe9, 0x49, 0x16,
        0xec, 0x6a, 0x50, 0xfd, 0x14, 0x49, 0xca, 0xdb,
        0x44, 0x54, 0xca, 0xbe, 0xa3, 0x0e, 0x5f, 0xef
    };

    static const RSAPublicKey bl_public_key = {
        NULL,
        { FIPS_RSA_TYPE, (unsigned char *)rsa_modulus,
          FIPS_RSA_MODULUS_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_public_exponent,
          FIPS_RSA_PUBLIC_EXPONENT_LENGTH }
    };
    static const RSAPrivateKey bl_private_key = {
        NULL,
        { FIPS_RSA_TYPE, (unsigned char *)rsa_version,
          FIPS_RSA_PRIVATE_VERSION_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_modulus,
          FIPS_RSA_MODULUS_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_public_exponent,
          FIPS_RSA_PUBLIC_EXPONENT_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_private_exponent,
          FIPS_RSA_PRIVATE_EXPONENT_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_prime0,
          FIPS_RSA_PRIME0_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_prime1,
          FIPS_RSA_PRIME1_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_exponent0,
          FIPS_RSA_EXPONENT0_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_exponent1,
          FIPS_RSA_EXPONENT1_LENGTH },
        { FIPS_RSA_TYPE, (unsigned char *)rsa_coefficient,
          FIPS_RSA_COEFFICIENT_LENGTH }
    };

    /* RSA variables. */
    SECStatus rsa_status;
    RSAPublicKey rsa_public_key;
    RSAPrivateKey rsa_private_key;

    PRUint8 rsa_computed_ciphertext[FIPS_RSA_ENCRYPT_LENGTH];
    PRUint8 rsa_computed_plaintext[FIPS_RSA_DECRYPT_LENGTH];

    rsa_public_key = bl_public_key;
    rsa_private_key = bl_private_key;

    /**************************************************/
    /* RSA Single-Round Known Answer Encryption Test. */
    /**************************************************/

    /* Perform RSA Public Key Encryption. */
    rsa_status = RSA_PublicKeyOp(&rsa_public_key,
                                 rsa_computed_ciphertext,
                                 rsa_known_plaintext_msg);

    if ((rsa_status != SECSuccess) ||
        (PORT_Memcmp(rsa_computed_ciphertext, rsa_known_ciphertext,
                     FIPS_RSA_ENCRYPT_LENGTH) != 0))
        goto rsa_loser;

    /**************************************************/
    /* RSA Single-Round Known Answer Decryption Test. */
    /**************************************************/

    /* Perform RSA Private Key Decryption. */
    rsa_status = RSA_PrivateKeyOp(&rsa_private_key,
                                  rsa_computed_plaintext,
                                  rsa_known_ciphertext);

    if ((rsa_status != SECSuccess) ||
        (PORT_Memcmp(rsa_computed_plaintext, rsa_known_plaintext_msg,
                     FIPS_RSA_DECRYPT_LENGTH) != 0))
        goto rsa_loser;

    return (SECSuccess);

rsa_loser:

    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return (SECFailure);
}

#ifdef NSS_ENABLE_ECC

static SECStatus
freebl_fips_ECDSA_Test(ECParams *ecparams,
                       const PRUint8 *knownSignature,
                       unsigned int knownSignatureLen)
{

    /* ECDSA Known Seed info for curves nistp256 and nistk283  */
    static const PRUint8 ecdsa_Known_Seed[] = {
        0x6a, 0x9b, 0xf6, 0xf7, 0xce, 0xed, 0x79, 0x11,
        0xf0, 0xc7, 0xc8, 0x9a, 0xa5, 0xd1, 0x57, 0xb1,
        0x7b, 0x5a, 0x3b, 0x76, 0x4e, 0x7b, 0x7c, 0xbc,
        0xf2, 0x76, 0x1c, 0x1c, 0x7f, 0xc5, 0x53, 0x2f
    };

    static const PRUint8 msg[] = {
        "Firefox and ThunderBird are awesome!"
    };

    unsigned char sha1[SHA1_LENGTH]; /* SHA-1 hash (160 bits) */
    unsigned char sig[2 * MAX_ECKEY_LEN];
    SECItem signature, digest;
    ECPrivateKey *ecdsa_private_key = NULL;
    ECPublicKey ecdsa_public_key;
    SECStatus ecdsaStatus = SECSuccess;

    /* Generates a new EC key pair. The private key is a supplied
     * random value (in seed) and the public key is the result of
     * performing a scalar point multiplication of that value with
     * the curve's base point.
     */
    ecdsaStatus = EC_NewKeyFromSeed(ecparams, &ecdsa_private_key,
                                    ecdsa_Known_Seed,
                                    sizeof(ecdsa_Known_Seed));
    if (ecdsaStatus != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }

    /* construct public key from private key. */
    ecdsa_public_key.ecParams = ecdsa_private_key->ecParams;
    ecdsa_public_key.publicValue = ecdsa_private_key->publicValue;

    /* validate public key value */
    ecdsaStatus = EC_ValidatePublicKey(&ecdsa_public_key.ecParams,
                                       &ecdsa_public_key.publicValue);
    if (ecdsaStatus != SECSuccess) {
        goto loser;
    }

    /* validate public key value */
    ecdsaStatus = EC_ValidatePublicKey(&ecdsa_private_key->ecParams,
                                       &ecdsa_private_key->publicValue);
    if (ecdsaStatus != SECSuccess) {
        goto loser;
    }

    /***************************************************/
    /* ECDSA Single-Round Known Answer Signature Test. */
    /***************************************************/

    ecdsaStatus = SHA1_HashBuf(sha1, msg, sizeof msg);
    if (ecdsaStatus != SECSuccess) {
        goto loser;
    }
    digest.type = siBuffer;
    digest.data = sha1;
    digest.len = SHA1_LENGTH;

    memset(sig, 0, sizeof sig);
    signature.type = siBuffer;
    signature.data = sig;
    signature.len = sizeof sig;

    ecdsaStatus = ECDSA_SignDigestWithSeed(ecdsa_private_key, &signature,
                                           &digest, ecdsa_Known_Seed, sizeof ecdsa_Known_Seed);
    if (ecdsaStatus != SECSuccess) {
        goto loser;
    }

    if ((signature.len != knownSignatureLen) ||
        (PORT_Memcmp(signature.data, knownSignature,
                     knownSignatureLen) != 0)) {
        ecdsaStatus = SECFailure;
        goto loser;
    }

    /******************************************************/
    /* ECDSA Single-Round Known Answer Verification Test. */
    /******************************************************/

    /* Perform ECDSA verification process. */
    ecdsaStatus = ECDSA_VerifyDigest(&ecdsa_public_key, &signature, &digest);

loser:
    /* free the memory for the private key arena*/
    PORT_FreeArena(ecdsa_private_key->ecParams.arena, PR_FALSE);

    if (ecdsaStatus != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return (SECFailure);
    }
    return (SECSuccess);
}

static SECStatus
freebl_fips_ECDSA_PowerUpSelfTest()
{

    /* ECDSA Known curve nistp256 == ECCCurve_X9_62_PRIME_256V1 params */
    static const unsigned char p256_prime[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    static const unsigned char p256_a[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
    };
    static const unsigned char p256_b[] = {
        0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93, 0xE7, 0xB3, 0xEB, 0xBD, 0x55, 0x76,
        0x98, 0x86, 0xBC, 0x65, 0x1D, 0x06, 0xB0, 0xCC, 0x53, 0xB0, 0xF6, 0x3B, 0xCE,
        0x3C, 0x3E, 0x27, 0xD2, 0x60, 0x4B
    };
    static const unsigned char p256_base[] = {
        0x04,
        0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47, 0xF8, 0xBC, 0xE6, 0xE5, 0x63,
        0xA4, 0x40, 0xF2, 0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0, 0xF4, 0xA1,
        0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96,
        0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B, 0x8E, 0xE7, 0xEB, 0x4A, 0x7C,
        0x0F, 0x9E, 0x16, 0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE, 0xCB, 0xB6,
        0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5
    };
    static const unsigned char p256_order[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3, 0xB9,
        0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
    };
    static const unsigned char p256_encoding[] = {
        0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
    };
    static const ECParams ecdsa_known_P256_Params = {
        NULL, ec_params_named,                                               /* arena, type */
                                                                             /* fieldID */
        { 256, ec_field_GFp,                                                 /* size and type */
          { { siBuffer, (unsigned char *)p256_prime, sizeof(p256_prime) } }, /* u.prime */
          0,
          0,
          0 },
        /* curve */
        { /* a = curvea b = curveb */
          /* curve.a */
          { siBuffer, (unsigned char *)p256_a, sizeof(p256_a) },
          /* curve.b */
          { siBuffer, (unsigned char *)p256_b, sizeof(p256_b) },
          /* curve.seed */
          { siBuffer, NULL, 0 } },
        /* base  = 04xy*/
        { siBuffer, (unsigned char *)p256_base, sizeof(p256_base) },
        /* order */
        { siBuffer, (unsigned char *)p256_order, sizeof(p256_order) },
        1, /* cofactor */
        /* DEREncoding */
        { siBuffer, (unsigned char *)p256_encoding, sizeof(p256_encoding) },
        ECCurve_X9_62_PRIME_256V1,
        /* curveOID */
        { siBuffer, (unsigned char *)(p256_encoding) + 2, sizeof(p256_encoding) - 2 },
    };

    static const PRUint8 ecdsa_known_P256_signature[] = {
        0x07, 0xb1, 0xcb, 0x57, 0x20, 0xa7, 0x10, 0xd6,
        0x9d, 0x37, 0x4b, 0x1c, 0xdc, 0x35, 0x90, 0xff,
        0x1a, 0x2d, 0x98, 0x95, 0x1b, 0x2f, 0xeb, 0x7f,
        0xbb, 0x81, 0xca, 0xc0, 0x69, 0x75, 0xea, 0xc5,
        0x59, 0x6a, 0x62, 0x49, 0x3d, 0x50, 0xc9, 0xe1,
        0x27, 0x3b, 0xff, 0x9b, 0x13, 0x66, 0x67, 0xdd,
        0x7d, 0xd1, 0x0d, 0x2d, 0x7c, 0x44, 0x04, 0x1b,
        0x16, 0x21, 0x12, 0xc5, 0xcb, 0xbd, 0x9e, 0x75
    };

#ifdef NSS_ECC_MORE_THAN_SUITE_B
    /* ECDSA Known curve nistk283 == SEC_OID_SECG_EC_SECT283K1 params */
    static const unsigned char k283_poly[] = {
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xA1
    };
    static const unsigned char k283_a[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    static const unsigned char k283_b[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    static const unsigned char k283_base[] = {
        0x04,
        0x05, 0x03, 0x21, 0x3F, 0x78, 0xCA, 0x44, 0x88, 0x3F, 0x1A, 0x3B, 0x81, 0x62,
        0xF1, 0x88, 0xE5, 0x53, 0xCD, 0x26, 0x5F, 0x23, 0xC1, 0x56, 0x7A, 0x16, 0x87,
        0x69, 0x13, 0xB0, 0xC2, 0xAC, 0x24, 0x58, 0x49, 0x28, 0x36,
        0x01, 0xCC, 0xDA, 0x38, 0x0F, 0x1C, 0x9E, 0x31, 0x8D, 0x90, 0xF9, 0x5D, 0x07,
        0xE5, 0x42, 0x6F, 0xE8, 0x7E, 0x45, 0xC0, 0xE8, 0x18, 0x46, 0x98, 0xE4, 0x59,
        0x62, 0x36, 0x4E, 0x34, 0x11, 0x61, 0x77, 0xDD, 0x22, 0x59
    };
    static const unsigned char k283_order[] = {
        0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE9, 0xAE, 0x2E, 0xD0, 0x75, 0x77, 0x26, 0x5D,
        0xFF, 0x7F, 0x94, 0x45, 0x1E, 0x06, 0x1E, 0x16, 0x3C, 0x61
    };
    static const PRUint8 k283_encoding[] = {
        0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x10
    };

    static const ECParams ecdsa_known_K283_Params = {
        NULL, ec_params_named,                            /* arena, type */
                                                          /* fieldID */
        { 283, ec_field_GF2m,                             /* size and type */
          { { siBuffer, p283_poly, sizeof(p283_poly) } }, /* u.poly */
          0,
          0,
          0 },
        /* curve */
        { /* a = curvea b = curveb */
          /* curve.a */
          { siBuffer, p283_a, sizeof(p283_a) },
          /* curve.b */
          { siBuffer, p283_b, sizeof(p283_b) },
          /* curve.seed */
          { siBuffer, NULL, 0 } },
        /* base  = 04xy*/
        { siBuffer, p283_base, sizeof(p283_base) },
        /* order */
        { siBuffer, p283_order, sizeof(p283_order) },
        4, /* cofactor */
        /* DEREncoding */
        { siBuffer, k283_encoding, sizeof(k283_encoding) },
        /* name */
        ECCurve_SECG_CHAR2_283K1,
        /* curveOID */
        { siBuffer, k283_encoding + 2, sizeof(k283_encoding) - 2 },
    };

    static const PRUint8 ecdsa_known_K283_signature[] = {
        0x00, 0x45, 0x88, 0xc0, 0x79, 0x09, 0x07, 0xd1,
        0x4e, 0x88, 0xe6, 0xd5, 0x2f, 0x22, 0x04, 0x74,
        0x35, 0x24, 0x65, 0xe8, 0x15, 0xde, 0x90, 0x66,
        0x94, 0x70, 0xdd, 0x3a, 0x14, 0x70, 0x02, 0xd1,
        0xef, 0x86, 0xbd, 0x15, 0x00, 0xd9, 0xdc, 0xfc,
        0x87, 0x2e, 0x7c, 0x99, 0xe2, 0xe3, 0x79, 0xb8,
        0xd9, 0x10, 0x49, 0x78, 0x4b, 0x59, 0x8b, 0x05,
        0x77, 0xec, 0x6c, 0xe8, 0x35, 0xe6, 0x2e, 0xa9,
        0xf9, 0x77, 0x1f, 0x71, 0x86, 0xa5, 0x4a, 0xd0
    };
#endif
    ECParams ecparams;

    SECStatus rv;

    /* ECDSA GF(p) prime field curve test */
    ecparams = ecdsa_known_P256_Params;
    rv = freebl_fips_ECDSA_Test(&ecparams,
                                ecdsa_known_P256_signature,
                                sizeof ecdsa_known_P256_signature);
    if (rv != SECSuccess) {
        return (SECFailure);
    }

#ifdef NSS_ECC_MORE_THAN_SUITE_B
    /* ECDSA GF(2m) binary field curve test */
    ecparams = ecdsa_known_K283_Params;
    rv = freebl_fips_ECDSA_Test(&ecparams,
                                ecdsa_known_K283_signature,
                                sizeof ecdsa_known_K283_signature);
    if (rv != SECSuccess) {
        return (SECFailure);
    }
#endif

    return (SECSuccess);
}

#endif /* NSS_ENABLE_ECC */

static SECStatus
freebl_fips_DSA_PowerUpSelfTest(void)
{
    /* DSA Known P (1024-bits), Q (160-bits), and G (1024-bits) Values. */
    static const PRUint8 dsa_P[] = {
        0x80, 0xb0, 0xd1, 0x9d, 0x6e, 0xa4, 0xf3, 0x28,
        0x9f, 0x24, 0xa9, 0x8a, 0x49, 0xd0, 0x0c, 0x63,
        0xe8, 0x59, 0x04, 0xf9, 0x89, 0x4a, 0x5e, 0xc0,
        0x6d, 0xd2, 0x67, 0x6b, 0x37, 0x81, 0x83, 0x0c,
        0xfe, 0x3a, 0x8a, 0xfd, 0xa0, 0x3b, 0x08, 0x91,
        0x1c, 0xcb, 0xb5, 0x63, 0xb0, 0x1c, 0x70, 0xd0,
        0xae, 0xe1, 0x60, 0x2e, 0x12, 0xeb, 0x54, 0xc7,
        0xcf, 0xc6, 0xcc, 0xae, 0x97, 0x52, 0x32, 0x63,
        0xd3, 0xeb, 0x55, 0xea, 0x2f, 0x4c, 0xd5, 0xd7,
        0x3f, 0xda, 0xec, 0x49, 0x27, 0x0b, 0x14, 0x56,
        0xc5, 0x09, 0xbe, 0x4d, 0x09, 0x15, 0x75, 0x2b,
        0xa3, 0x42, 0x0d, 0x03, 0x71, 0xdf, 0x0f, 0xf4,
        0x0e, 0xe9, 0x0c, 0x46, 0x93, 0x3d, 0x3f, 0xa6,
        0x6c, 0xdb, 0xca, 0xe5, 0xac, 0x96, 0xc8, 0x64,
        0x5c, 0xec, 0x4b, 0x35, 0x65, 0xfc, 0xfb, 0x5a,
        0x1b, 0x04, 0x1b, 0xa1, 0x0e, 0xfd, 0x88, 0x15
    };

    static const PRUint8 dsa_Q[] = {
        0xad, 0x22, 0x59, 0xdf, 0xe5, 0xec, 0x4c, 0x6e,
        0xf9, 0x43, 0xf0, 0x4b, 0x2d, 0x50, 0x51, 0xc6,
        0x91, 0x99, 0x8b, 0xcf
    };

    static const PRUint8 dsa_G[] = {
        0x78, 0x6e, 0xa9, 0xd8, 0xcd, 0x4a, 0x85, 0xa4,
        0x45, 0xb6, 0x6e, 0x5d, 0x21, 0x50, 0x61, 0xf6,
        0x5f, 0xdf, 0x5c, 0x7a, 0xde, 0x0d, 0x19, 0xd3,
        0xc1, 0x3b, 0x14, 0xcc, 0x8e, 0xed, 0xdb, 0x17,
        0xb6, 0xca, 0xba, 0x86, 0xa9, 0xea, 0x51, 0x2d,
        0xc1, 0xa9, 0x16, 0xda, 0xf8, 0x7b, 0x59, 0x8a,
        0xdf, 0xcb, 0xa4, 0x67, 0x00, 0x44, 0xea, 0x24,
        0x73, 0xe5, 0xcb, 0x4b, 0xaf, 0x2a, 0x31, 0x25,
        0x22, 0x28, 0x3f, 0x16, 0x10, 0x82, 0xf7, 0xeb,
        0x94, 0x0d, 0xdd, 0x09, 0x22, 0x14, 0x08, 0x79,
        0xba, 0x11, 0x0b, 0xf1, 0xff, 0x2d, 0x67, 0xac,
        0xeb, 0xb6, 0x55, 0x51, 0x69, 0x97, 0xa7, 0x25,
        0x6b, 0x9c, 0xa0, 0x9b, 0xd5, 0x08, 0x9b, 0x27,
        0x42, 0x1c, 0x7a, 0x69, 0x57, 0xe6, 0x2e, 0xed,
        0xa9, 0x5b, 0x25, 0xe8, 0x1f, 0xd2, 0xed, 0x1f,
        0xdf, 0xe7, 0x80, 0x17, 0xba, 0x0d, 0x4d, 0x38
    };

    /* DSA Known Random Values (known random key block       is 160-bits)  */
    /*                     and (known random signature block is 160-bits). */
    static const PRUint8 dsa_known_random_key_block[] = {
        "Mozilla Rules World!"
    };
    static const PRUint8 dsa_known_random_signature_block[] = {
        "Random DSA Signature"
    };

    /* DSA Known Digest (160-bits) */
    static const PRUint8 dsa_known_digest[] = { "DSA Signature Digest" };

    /* DSA Known Signature (320-bits). */
    static const PRUint8 dsa_known_signature[] = {
        0x25, 0x7c, 0x3a, 0x79, 0x32, 0x45, 0xb7, 0x32,
        0x70, 0xca, 0x62, 0x63, 0x2b, 0xf6, 0x29, 0x2c,
        0x22, 0x2a, 0x03, 0xce, 0x48, 0x15, 0x11, 0x72,
        0x7b, 0x7e, 0xf5, 0x7a, 0xf3, 0x10, 0x3b, 0xde,
        0x34, 0xc1, 0x9e, 0xd7, 0x27, 0x9e, 0x77, 0x38
    };

    /* DSA variables. */
    DSAPrivateKey *dsa_private_key;
    SECStatus dsa_status;
    SECItem dsa_signature_item;
    SECItem dsa_digest_item;
    DSAPublicKey dsa_public_key;
    PRUint8 dsa_computed_signature[FIPS_DSA_SIGNATURE_LENGTH];
    static const PQGParams dsa_pqg = {
        NULL,
        { FIPS_DSA_TYPE, (unsigned char *)dsa_P, FIPS_DSA_PRIME_LENGTH },
        { FIPS_DSA_TYPE, (unsigned char *)dsa_Q, FIPS_DSA_SUBPRIME_LENGTH },
        { FIPS_DSA_TYPE, (unsigned char *)dsa_G, FIPS_DSA_BASE_LENGTH }
    };

    /*******************************************/
    /* Generate a DSA public/private key pair. */
    /*******************************************/

    /* Generate a DSA public/private key pair. */
    dsa_status = DSA_NewKeyFromSeed(&dsa_pqg, dsa_known_random_key_block,
                                    &dsa_private_key);

    if (dsa_status != SECSuccess) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return (SECFailure);
    }

    /* construct public key from private key. */
    dsa_public_key.params = dsa_private_key->params;
    dsa_public_key.publicValue = dsa_private_key->publicValue;

    /*************************************************/
    /* DSA Single-Round Known Answer Signature Test. */
    /*************************************************/

    dsa_signature_item.data = dsa_computed_signature;
    dsa_signature_item.len = sizeof dsa_computed_signature;

    dsa_digest_item.data = (unsigned char *)dsa_known_digest;
    dsa_digest_item.len = SHA1_LENGTH;

    /* Perform DSA signature process. */
    dsa_status = DSA_SignDigestWithSeed(dsa_private_key,
                                        &dsa_signature_item,
                                        &dsa_digest_item,
                                        dsa_known_random_signature_block);

    if ((dsa_status != SECSuccess) ||
        (dsa_signature_item.len != FIPS_DSA_SIGNATURE_LENGTH) ||
        (PORT_Memcmp(dsa_computed_signature, dsa_known_signature,
                     FIPS_DSA_SIGNATURE_LENGTH) != 0)) {
        dsa_status = SECFailure;
    } else {

        /****************************************************/
        /* DSA Single-Round Known Answer Verification Test. */
        /****************************************************/

        /* Perform DSA verification process. */
        dsa_status = DSA_VerifyDigest(&dsa_public_key,
                                      &dsa_signature_item,
                                      &dsa_digest_item);
    }

    PORT_FreeArena(dsa_private_key->params.arena, PR_TRUE);
    /* Don't free public key, it uses same arena as private key */

    /* Verify DSA signature. */
    if (dsa_status != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    return (SECSuccess);
}

static SECStatus
freebl_fips_RNG_PowerUpSelfTest(void)
{
    static const PRUint8 Q[] = {
        0x85, 0x89, 0x9c, 0x77, 0xa3, 0x79, 0xff, 0x1a,
        0x86, 0x6f, 0x2f, 0x3e, 0x2e, 0xf9, 0x8c, 0x9c,
        0x9d, 0xef, 0xeb, 0xed
    };
    static const PRUint8 GENX[] = {
        0x65, 0x48, 0xe3, 0xca, 0xac, 0x64, 0x2d, 0xf7,
        0x7b, 0xd3, 0x4e, 0x79, 0xc9, 0x7d, 0xa6, 0xa8,
        0xa2, 0xc2, 0x1f, 0x8f, 0xe9, 0xb9, 0xd3, 0xa1,
        0x3f, 0xf7, 0x0c, 0xcd, 0xa6, 0xca, 0xbf, 0xce,
        0x84, 0x0e, 0xb6, 0xf1, 0x0d, 0xbe, 0xa9, 0xa3
    };
    static const PRUint8 rng_known_DSAX[] = {
        0x7a, 0x86, 0xf1, 0x7f, 0xbd, 0x4e, 0x6e, 0xd9,
        0x0a, 0x26, 0x21, 0xd0, 0x19, 0xcb, 0x86, 0x73,
        0x10, 0x1f, 0x60, 0xd7
    };

    SECStatus rng_status = SECSuccess;
    PRUint8 DSAX[FIPS_DSA_SUBPRIME_LENGTH];

    /*******************************************/
    /*   Run the SP 800-90 Health tests        */
    /*******************************************/
    rng_status = PRNGTEST_RunHealthTests();
    if (rng_status != SECSuccess) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /*******************************************/
    /* Generate DSAX fow given Q.              */
    /*******************************************/

    rng_status = FIPS186Change_ReduceModQForDSA(GENX, Q, DSAX);

    /* Verify DSAX to perform the RNG integrity check */
    if ((rng_status != SECSuccess) ||
        (PORT_Memcmp(DSAX, rng_known_DSAX,
                     (FIPS_DSA_SUBPRIME_LENGTH)) != 0)) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    return (SECSuccess);
}

static SECStatus
freebl_fipsSoftwareIntegrityTest(const char *libname)
{
    SECStatus rv = SECSuccess;

    /* make sure that our check file signatures are OK */
    if (!BLAPI_VerifySelf(libname)) {
        rv = SECFailure;
    }
    return rv;
}

#define DO_FREEBL 1
#define DO_REST 2

static SECStatus
freebl_fipsPowerUpSelfTest(unsigned int tests)
{
    SECStatus rv;

    /*
     * stand alone freebl. Test hash, and rng
     */
    if (tests & DO_FREEBL) {

        /* SHA-X Power-Up SelfTest(s). */
        rv = freebl_fips_SHA_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;

        /* RNG Power-Up SelfTest(s). */
        rv = freebl_fips_RNG_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;
    }

    /*
     * test the rest of the algorithms not accessed through freebl
     * standalone */
    if (tests & DO_REST) {

        /* DES3 Power-Up SelfTest(s). */
        rv = freebl_fips_DES3_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;

        /* AES Power-Up SelfTest(s) for 128-bit key. */
        rv = freebl_fips_AES_PowerUpSelfTest(FIPS_AES_128_KEY_SIZE);

        if (rv != SECSuccess)
            return rv;

        /* AES Power-Up SelfTest(s) for 192-bit key. */
        rv = freebl_fips_AES_PowerUpSelfTest(FIPS_AES_192_KEY_SIZE);

        if (rv != SECSuccess)
            return rv;

        /* AES Power-Up SelfTest(s) for 256-bit key. */
        rv = freebl_fips_AES_PowerUpSelfTest(FIPS_AES_256_KEY_SIZE);

        if (rv != SECSuccess)
            return rv;

        /* HMAC SHA-X Power-Up SelfTest(s). */
        rv = freebl_fips_HMAC_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;

        /* NOTE: RSA can only be tested in full freebl. It requires access to
     * the locking primitives */
        /* RSA Power-Up SelfTest(s). */
        rv = freebl_fips_RSA_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;

        /* DSA Power-Up SelfTest(s). */
        rv = freebl_fips_DSA_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;

#ifdef NSS_ENABLE_ECC
        /* ECDSA Power-Up SelfTest(s). */
        rv = freebl_fips_ECDSA_PowerUpSelfTest();

        if (rv != SECSuccess)
            return rv;
#endif
    }
    /* Passed Power-Up SelfTest(s). */
    return (SECSuccess);
}

/*
 * state variables. NOTE: freebl has two uses: a standalone use which
 * provided limitted access to the hash functions throught the NSSLOWHASH_
 * interface and an joint use from softoken, using the function pointer
 * table. The standalone use can operation without nspr or nss-util, while
 * the joint use requires both to be loaded. Certain functions (like RSA)
 * needs locking from NSPR, for instance.
 *
 * At load time, we need to handle the two uses separately. If nspr and
 * nss-util  are loaded, then we can run all the selftests, but if nspr and
 * nss-util are not loaded, then we can't run all the selftests, and we need
 * to prevent the softoken function pointer table from operating until the
 * libraries are loaded and we try to use them.
 */
static PRBool self_tests_freebl_ran = PR_FALSE;
static PRBool self_tests_ran = PR_FALSE;
static PRBool self_tests_freebl_success = PR_FALSE;
static PRBool self_tests_success = PR_FALSE;
#if defined(DEBUG)
static PRBool fips_mode_available = PR_FALSE;
#endif

/*
 * accessors for freebl
 */
PRBool
BL_POSTRan(PRBool freebl_only)
{
    SECStatus rv;
    /* if the freebl self tests didn't run, there is something wrong with
     * our on load tests */
    if (!self_tests_freebl_ran) {
        return PR_FALSE;
    }
    /* if all the self tests have run, we are good */
    if (self_tests_ran) {
        return PR_TRUE;
    }
    /* if we only care about the freebl tests, we are good */
    if (freebl_only) {
        return PR_TRUE;
    }
    /* run the rest of the self tests */
    /* We could get there if freebl was loaded without the rest of the support
     * libraries, but now we want to use more than just a standalone freebl.
     * This requires the other libraries to be loaded.
     * If they are now loaded, Try to run the rest of the selftests,
     * otherwise fail (disabling access to these algorithms)  */
    self_tests_ran = PR_TRUE;
    BL_Init();     /* required by RSA */
    RNG_RNGInit(); /* required by RSA */
    rv = freebl_fipsPowerUpSelfTest(DO_REST);
    if (rv == SECSuccess) {
        self_tests_success = PR_TRUE;
    }
    return PR_TRUE;
}

#include "blname.c"

/*
 * This function is called at dll load time, the code tha makes this
 * happen is platform specific on defined above.
 */
static void
bl_startup_tests(void)
{
    const char *libraryName;
    PRBool freebl_only = PR_FALSE;
    SECStatus rv;

    PORT_Assert(self_tests_freebl_ran == PR_FALSE);
    PORT_Assert(self_tests_success == PR_FALSE);
    PORT_Assert(fips_mode_available == PR_FALSE);
    self_tests_freebl_ran = PR_TRUE;      /* we are running the tests */
    self_tests_success = PR_FALSE;        /* force it just in case */
    self_tests_freebl_success = PR_FALSE; /* force it just in case */

#ifdef FREEBL_NO_DEPEND
    rv = FREEBL_InitStubs();
    if (rv != SECSuccess) {
        freebl_only = PR_TRUE;
    }
#endif

    self_tests_freebl_ran = PR_TRUE; /* we are running the tests */

    if (!freebl_only) {
        self_tests_ran = PR_TRUE; /* we're running all the tests */
        BL_Init();                /* needs to be called before RSA can be used */
        RNG_RNGInit();
    }

    /* always run the post tests */
    rv = freebl_fipsPowerUpSelfTest(freebl_only ? DO_FREEBL : DO_FREEBL | DO_REST);
    if (rv != SECSuccess) {
        return;
    }

    libraryName = getLibName();
    rv = freebl_fipsSoftwareIntegrityTest(libraryName);
    if (rv != SECSuccess) {
        return;
    }

    /* posts are happy, allow the fips module to function now */
    self_tests_freebl_success = PR_TRUE; /* we always test the freebl stuff */
    if (!freebl_only) {
        self_tests_success = PR_TRUE;
    }
}

/*
 * this is called from the freebl init entry points that controll access to
 * all other freebl functions. This prevents freebl from operating if our
 * power on selftest failed.
 */
SECStatus
BL_FIPSEntryOK(PRBool freebl_only)
{
#ifdef NSS_NO_INIT_SUPPORT
    /* this should only be set on platforms that can't handle one of the INIT
    * schemes.  This code allows those platforms to continue to function,
    * though they don't meet the strict NIST requirements. If NSS_NO_INIT_SUPPORT
    * is not set, and init support has not been properly enabled, freebl
    * will always fail because of the test below
    */
    if (!self_tests_freebl_ran) {
        bl_startup_tests();
    }
#endif
    /* if the general self tests succeeded, we're done */
    if (self_tests_success) {
        return SECSuccess;
    }
    /* standalone freebl can initialize */
    if (freebl_only & self_tests_freebl_success) {
        return SECSuccess;
    }
    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
    return SECFailure;
}
