/*
 * PKCS #11 FIPS Power-Up Self Test.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "softoken.h"   /* Required for RC2-ECB, RC2-CBC, RC4, DES-ECB,  */
                        /*              DES-CBC, DES3-ECB, DES3-CBC, RSA */
                        /*              and DSA.                         */
#include "seccomon.h"   /* Required for RSA and DSA. */
#include "lowkeyi.h"    /* Required for RSA and DSA. */
#include "pkcs11.h"     /* Required for PKCS #11. */
#include "secerr.h"

#ifndef NSS_DISABLE_ECC
#include "ec.h"         /* Required for ECDSA */
#endif


/* FIPS preprocessor directives for RC2-ECB and RC2-CBC.        */
#define FIPS_RC2_KEY_LENGTH                      5  /*  40-bits */
#define FIPS_RC2_ENCRYPT_LENGTH                  8  /*  64-bits */
#define FIPS_RC2_DECRYPT_LENGTH                  8  /*  64-bits */


/* FIPS preprocessor directives for RC4.                        */
#define FIPS_RC4_KEY_LENGTH                      5  /*  40-bits */
#define FIPS_RC4_ENCRYPT_LENGTH                  8  /*  64-bits */
#define FIPS_RC4_DECRYPT_LENGTH                  8  /*  64-bits */


/* FIPS preprocessor directives for DES-ECB and DES-CBC.        */
#define FIPS_DES_ENCRYPT_LENGTH                  8  /*  64-bits */
#define FIPS_DES_DECRYPT_LENGTH                  8  /*  64-bits */


/* FIPS preprocessor directives for DES3-CBC and DES3-ECB.      */
#define FIPS_DES3_ENCRYPT_LENGTH                 8  /*  64-bits */
#define FIPS_DES3_DECRYPT_LENGTH                 8  /*  64-bits */


/* FIPS preprocessor directives for AES-ECB and AES-CBC.        */
#define FIPS_AES_BLOCK_SIZE                     16  /* 128-bits */
#define FIPS_AES_ENCRYPT_LENGTH                 16  /* 128-bits */
#define FIPS_AES_DECRYPT_LENGTH                 16  /* 128-bits */
#define FIPS_AES_128_KEY_SIZE                   16  /* 128-bits */
#define FIPS_AES_192_KEY_SIZE                   24  /* 192-bits */
#define FIPS_AES_256_KEY_SIZE                   32  /* 256-bits */


/* FIPS preprocessor directives for message digests             */
#define FIPS_KNOWN_HASH_MESSAGE_LENGTH          64  /* 512-bits */


/* FIPS preprocessor directives for RSA.                         */
#define FIPS_RSA_TYPE                           siBuffer
#define FIPS_RSA_PUBLIC_EXPONENT_LENGTH           3 /*   24-bits */
#define FIPS_RSA_PRIVATE_VERSION_LENGTH           1 /*    8-bits */
#define FIPS_RSA_MESSAGE_LENGTH                 256 /* 2048-bits */
#define FIPS_RSA_COEFFICIENT_LENGTH             128 /* 1024-bits */
#define FIPS_RSA_PRIME0_LENGTH                  128 /* 1024-bits */
#define FIPS_RSA_PRIME1_LENGTH                  128 /* 1024-bits */
#define FIPS_RSA_EXPONENT0_LENGTH               128 /* 1024-bits */
#define FIPS_RSA_EXPONENT1_LENGTH               128 /* 1024-bits */
#define FIPS_RSA_PRIVATE_EXPONENT_LENGTH        256 /* 2048-bits */
#define FIPS_RSA_ENCRYPT_LENGTH                 256 /* 2048-bits */
#define FIPS_RSA_DECRYPT_LENGTH                 256 /* 2048-bits */
#define FIPS_RSA_SIGNATURE_LENGTH               256 /* 2048-bits */
#define FIPS_RSA_MODULUS_LENGTH                 256 /* 2048-bits */


/* FIPS preprocessor directives for DSA.                        */
#define FIPS_DSA_TYPE                           siBuffer
#define FIPS_DSA_DIGEST_LENGTH                  20 /*  160-bits */
#define FIPS_DSA_SUBPRIME_LENGTH                20 /*  160-bits */
#define FIPS_DSA_SIGNATURE_LENGTH               40 /*  320-bits */
#define FIPS_DSA_PRIME_LENGTH                  128 /* 1024-bits */
#define FIPS_DSA_BASE_LENGTH                   128 /* 1024-bits */

/* FIPS preprocessor directives for RNG.                        */
#define FIPS_RNG_XKEY_LENGTH                    32  /* 256-bits */

static CK_RV
sftk_fips_RC2_PowerUpSelfTest( void )
{
    /* RC2 Known Key (40-bits). */
    static const PRUint8 rc2_known_key[] = { "RSARC" };

    /* RC2-CBC Known Initialization Vector (64-bits). */
    static const PRUint8 rc2_cbc_known_initialization_vector[] = {"Security"};

    /* RC2 Known Plaintext (64-bits). */
    static const PRUint8 rc2_ecb_known_plaintext[] = {"Netscape"};
    static const PRUint8 rc2_cbc_known_plaintext[] = {"Netscape"};

    /* RC2 Known Ciphertext (64-bits). */
    static const PRUint8 rc2_ecb_known_ciphertext[] = {
				  0x1a,0x71,0x33,0x54,0x8d,0x5c,0xd2,0x30};
    static const PRUint8 rc2_cbc_known_ciphertext[] = {
				  0xff,0x41,0xdb,0x94,0x8a,0x4c,0x33,0xb3};

    /* RC2 variables. */
    PRUint8        rc2_computed_ciphertext[FIPS_RC2_ENCRYPT_LENGTH];
    PRUint8        rc2_computed_plaintext[FIPS_RC2_DECRYPT_LENGTH];
    RC2Context *   rc2_context;
    unsigned int   rc2_bytes_encrypted;
    unsigned int   rc2_bytes_decrypted;
    SECStatus      rc2_status;


    /******************************************************/
    /* RC2-ECB Single-Round Known Answer Encryption Test: */
    /******************************************************/

    rc2_context = RC2_CreateContext( rc2_known_key, FIPS_RC2_KEY_LENGTH,
                                     NULL, NSS_RC2,
                                     FIPS_RC2_KEY_LENGTH );

    if( rc2_context == NULL )
        return( CKR_HOST_MEMORY );

    rc2_status = RC2_Encrypt( rc2_context, rc2_computed_ciphertext,
                              &rc2_bytes_encrypted, FIPS_RC2_ENCRYPT_LENGTH,
                              rc2_ecb_known_plaintext,
                              FIPS_RC2_DECRYPT_LENGTH );

    RC2_DestroyContext( rc2_context, PR_TRUE );

    if( ( rc2_status != SECSuccess ) ||
        ( rc2_bytes_encrypted != FIPS_RC2_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( rc2_computed_ciphertext, rc2_ecb_known_ciphertext,
                       FIPS_RC2_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* RC2-ECB Single-Round Known Answer Decryption Test: */
    /******************************************************/

    rc2_context = RC2_CreateContext( rc2_known_key, FIPS_RC2_KEY_LENGTH,
                                     NULL, NSS_RC2,
                                     FIPS_RC2_KEY_LENGTH );

    if( rc2_context == NULL )
        return( CKR_HOST_MEMORY );

    rc2_status = RC2_Decrypt( rc2_context, rc2_computed_plaintext,
                              &rc2_bytes_decrypted, FIPS_RC2_DECRYPT_LENGTH,
                              rc2_ecb_known_ciphertext,
                              FIPS_RC2_ENCRYPT_LENGTH );

    RC2_DestroyContext( rc2_context, PR_TRUE );

    if( ( rc2_status != SECSuccess ) ||
        ( rc2_bytes_decrypted != FIPS_RC2_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( rc2_computed_plaintext, rc2_ecb_known_plaintext,
                       FIPS_RC2_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* RC2-CBC Single-Round Known Answer Encryption Test: */
    /******************************************************/

    rc2_context = RC2_CreateContext( rc2_known_key, FIPS_RC2_KEY_LENGTH,
                                     rc2_cbc_known_initialization_vector,
                                     NSS_RC2_CBC, FIPS_RC2_KEY_LENGTH );

    if( rc2_context == NULL )
        return( CKR_HOST_MEMORY );

    rc2_status = RC2_Encrypt( rc2_context, rc2_computed_ciphertext,
                              &rc2_bytes_encrypted, FIPS_RC2_ENCRYPT_LENGTH,
                              rc2_cbc_known_plaintext,
                              FIPS_RC2_DECRYPT_LENGTH );

    RC2_DestroyContext( rc2_context, PR_TRUE );

    if( ( rc2_status != SECSuccess ) ||
        ( rc2_bytes_encrypted != FIPS_RC2_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( rc2_computed_ciphertext, rc2_cbc_known_ciphertext,
                       FIPS_RC2_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* RC2-CBC Single-Round Known Answer Decryption Test: */
    /******************************************************/

    rc2_context = RC2_CreateContext( rc2_known_key, FIPS_RC2_KEY_LENGTH,
                                     rc2_cbc_known_initialization_vector,
                                     NSS_RC2_CBC, FIPS_RC2_KEY_LENGTH );

    if( rc2_context == NULL )
        return( CKR_HOST_MEMORY );

    rc2_status = RC2_Decrypt( rc2_context, rc2_computed_plaintext,
                              &rc2_bytes_decrypted, FIPS_RC2_DECRYPT_LENGTH,
                              rc2_cbc_known_ciphertext,
                              FIPS_RC2_ENCRYPT_LENGTH );

    RC2_DestroyContext( rc2_context, PR_TRUE );

    if( ( rc2_status != SECSuccess ) ||
        ( rc2_bytes_decrypted != FIPS_RC2_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( rc2_computed_plaintext, rc2_ecb_known_plaintext,
                       FIPS_RC2_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
sftk_fips_RC4_PowerUpSelfTest( void )
{
    /* RC4 Known Key (40-bits). */
    static const PRUint8 rc4_known_key[] = { "RSARC" };

    /* RC4 Known Plaintext (64-bits). */
    static const PRUint8 rc4_known_plaintext[] = { "Netscape" };

    /* RC4 Known Ciphertext (64-bits). */
    static const PRUint8 rc4_known_ciphertext[] = {
				0x29,0x33,0xc7,0x9a,0x9d,0x6c,0x09,0xdd};

    /* RC4 variables. */
    PRUint8        rc4_computed_ciphertext[FIPS_RC4_ENCRYPT_LENGTH];
    PRUint8        rc4_computed_plaintext[FIPS_RC4_DECRYPT_LENGTH];
    RC4Context *   rc4_context;
    unsigned int   rc4_bytes_encrypted;
    unsigned int   rc4_bytes_decrypted;
    SECStatus      rc4_status;


    /**************************************************/
    /* RC4 Single-Round Known Answer Encryption Test: */
    /**************************************************/

    rc4_context = RC4_CreateContext( rc4_known_key, FIPS_RC4_KEY_LENGTH );

    if( rc4_context == NULL )
        return( CKR_HOST_MEMORY );

    rc4_status = RC4_Encrypt( rc4_context, rc4_computed_ciphertext,
                              &rc4_bytes_encrypted, FIPS_RC4_ENCRYPT_LENGTH,
                              rc4_known_plaintext, FIPS_RC4_DECRYPT_LENGTH );

    RC4_DestroyContext( rc4_context, PR_TRUE );

    if( ( rc4_status != SECSuccess ) ||
        ( rc4_bytes_encrypted != FIPS_RC4_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( rc4_computed_ciphertext, rc4_known_ciphertext,
                       FIPS_RC4_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /**************************************************/
    /* RC4 Single-Round Known Answer Decryption Test: */
    /**************************************************/

    rc4_context = RC4_CreateContext( rc4_known_key, FIPS_RC4_KEY_LENGTH );

    if( rc4_context == NULL )
        return( CKR_HOST_MEMORY );

    rc4_status = RC4_Decrypt( rc4_context, rc4_computed_plaintext,
                              &rc4_bytes_decrypted, FIPS_RC4_DECRYPT_LENGTH,
                              rc4_known_ciphertext, FIPS_RC4_ENCRYPT_LENGTH );

    RC4_DestroyContext( rc4_context, PR_TRUE );

    if( ( rc4_status != SECSuccess ) ||
        ( rc4_bytes_decrypted != FIPS_RC4_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( rc4_computed_plaintext, rc4_known_plaintext,
                       FIPS_RC4_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
sftk_fips_DES_PowerUpSelfTest( void )
{
    /* DES Known Key (56-bits). */
    static const PRUint8 des_known_key[] = { "ANSI DES" };

    /* DES-CBC Known Initialization Vector (64-bits). */
    static const PRUint8 des_cbc_known_initialization_vector[] = { "Security" };

    /* DES Known Plaintext (64-bits). */
    static const PRUint8 des_ecb_known_plaintext[] = { "Netscape" };
    static const PRUint8 des_cbc_known_plaintext[] = { "Netscape" };

    /* DES Known Ciphertext (64-bits). */
    static const PRUint8 des_ecb_known_ciphertext[] = {
			       0x26,0x14,0xe9,0xc3,0x28,0x80,0x50,0xb0};
    static const PRUint8 des_cbc_known_ciphertext[]  = {
			       0x5e,0x95,0x94,0x5d,0x76,0xa2,0xd3,0x7d};

    /* DES variables. */
    PRUint8        des_computed_ciphertext[FIPS_DES_ENCRYPT_LENGTH];
    PRUint8        des_computed_plaintext[FIPS_DES_DECRYPT_LENGTH];
    DESContext *   des_context;
    unsigned int   des_bytes_encrypted;
    unsigned int   des_bytes_decrypted;
    SECStatus      des_status;


    /******************************************************/
    /* DES-ECB Single-Round Known Answer Encryption Test: */
    /******************************************************/

    des_context = DES_CreateContext( des_known_key, NULL, NSS_DES, PR_TRUE );

    if( des_context == NULL )
        return( CKR_HOST_MEMORY );

    des_status = DES_Encrypt( des_context, des_computed_ciphertext,
                              &des_bytes_encrypted, FIPS_DES_ENCRYPT_LENGTH,
                              des_ecb_known_plaintext,
                              FIPS_DES_DECRYPT_LENGTH );

    DES_DestroyContext( des_context, PR_TRUE );

    if( ( des_status != SECSuccess ) ||
        ( des_bytes_encrypted != FIPS_DES_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( des_computed_ciphertext, des_ecb_known_ciphertext,
                       FIPS_DES_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* DES-ECB Single-Round Known Answer Decryption Test: */
    /******************************************************/

    des_context = DES_CreateContext( des_known_key, NULL, NSS_DES, PR_FALSE );

    if( des_context == NULL )
        return( CKR_HOST_MEMORY );

    des_status = DES_Decrypt( des_context, des_computed_plaintext,
                              &des_bytes_decrypted, FIPS_DES_DECRYPT_LENGTH,
                              des_ecb_known_ciphertext,
                              FIPS_DES_ENCRYPT_LENGTH );

    DES_DestroyContext( des_context, PR_TRUE );

    if( ( des_status != SECSuccess ) ||
        ( des_bytes_decrypted != FIPS_DES_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( des_computed_plaintext, des_ecb_known_plaintext,
                       FIPS_DES_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* DES-CBC Single-Round Known Answer Encryption Test. */
    /******************************************************/

    des_context = DES_CreateContext( des_known_key,
                                     des_cbc_known_initialization_vector,
                                     NSS_DES_CBC, PR_TRUE );

    if( des_context == NULL )
        return( CKR_HOST_MEMORY );

    des_status = DES_Encrypt( des_context, des_computed_ciphertext,
                              &des_bytes_encrypted, FIPS_DES_ENCRYPT_LENGTH,
                              des_cbc_known_plaintext,
                              FIPS_DES_DECRYPT_LENGTH );

    DES_DestroyContext( des_context, PR_TRUE );

    if( ( des_status != SECSuccess ) ||
        ( des_bytes_encrypted != FIPS_DES_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( des_computed_ciphertext, des_cbc_known_ciphertext,
                       FIPS_DES_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* DES-CBC Single-Round Known Answer Decryption Test. */
    /******************************************************/

    des_context = DES_CreateContext( des_known_key,
                                     des_cbc_known_initialization_vector,
                                     NSS_DES_CBC, PR_FALSE );

    if( des_context == NULL )
        return( CKR_HOST_MEMORY );

    des_status = DES_Decrypt( des_context, des_computed_plaintext,
                              &des_bytes_decrypted, FIPS_DES_DECRYPT_LENGTH,
                              des_cbc_known_ciphertext,
                              FIPS_DES_ENCRYPT_LENGTH );

    DES_DestroyContext( des_context, PR_TRUE );

    if( ( des_status != SECSuccess ) ||
        ( des_bytes_decrypted != FIPS_DES_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( des_computed_plaintext, des_cbc_known_plaintext,
                       FIPS_DES_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
sftk_fips_DES3_PowerUpSelfTest( void )
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
			   0x55,0x8e,0xad,0x3c,0xee,0x49,0x69,0xbe};
    static const PRUint8 des3_cbc_known_ciphertext[] = {
			   0x43,0xdc,0x6a,0xc1,0xaf,0xa6,0x32,0xf5};

    /* DES3 variables. */
    PRUint8        des3_computed_ciphertext[FIPS_DES3_ENCRYPT_LENGTH];
    PRUint8        des3_computed_plaintext[FIPS_DES3_DECRYPT_LENGTH];
    DESContext *   des3_context;
    unsigned int   des3_bytes_encrypted;
    unsigned int   des3_bytes_decrypted;
    SECStatus      des3_status;


    /*******************************************************/
    /* DES3-ECB Single-Round Known Answer Encryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext( des3_known_key, NULL,
                                     NSS_DES_EDE3, PR_TRUE );

    if( des3_context == NULL )
        return( CKR_HOST_MEMORY );

    des3_status = DES_Encrypt( des3_context, des3_computed_ciphertext,
                               &des3_bytes_encrypted, FIPS_DES3_ENCRYPT_LENGTH,
                               des3_ecb_known_plaintext,
                               FIPS_DES3_DECRYPT_LENGTH );

    DES_DestroyContext( des3_context, PR_TRUE );

    if( ( des3_status != SECSuccess ) ||
        ( des3_bytes_encrypted != FIPS_DES3_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( des3_computed_ciphertext, des3_ecb_known_ciphertext,
                       FIPS_DES3_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /*******************************************************/
    /* DES3-ECB Single-Round Known Answer Decryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext( des3_known_key, NULL,
                                     NSS_DES_EDE3, PR_FALSE );

    if( des3_context == NULL )
        return( CKR_HOST_MEMORY );

    des3_status = DES_Decrypt( des3_context, des3_computed_plaintext,
                               &des3_bytes_decrypted, FIPS_DES3_DECRYPT_LENGTH,
                               des3_ecb_known_ciphertext,
                               FIPS_DES3_ENCRYPT_LENGTH );

    DES_DestroyContext( des3_context, PR_TRUE );

    if( ( des3_status != SECSuccess ) ||
        ( des3_bytes_decrypted != FIPS_DES3_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( des3_computed_plaintext, des3_ecb_known_plaintext,
                       FIPS_DES3_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /*******************************************************/
    /* DES3-CBC Single-Round Known Answer Encryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext( des3_known_key,
                                      des3_cbc_known_initialization_vector,
                                      NSS_DES_EDE3_CBC, PR_TRUE );

    if( des3_context == NULL )
        return( CKR_HOST_MEMORY );

    des3_status = DES_Encrypt( des3_context, des3_computed_ciphertext,
                               &des3_bytes_encrypted, FIPS_DES3_ENCRYPT_LENGTH,
                               des3_cbc_known_plaintext,
                               FIPS_DES3_DECRYPT_LENGTH );

    DES_DestroyContext( des3_context, PR_TRUE );

    if( ( des3_status != SECSuccess ) ||
        ( des3_bytes_encrypted != FIPS_DES3_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( des3_computed_ciphertext, des3_cbc_known_ciphertext,
                       FIPS_DES3_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /*******************************************************/
    /* DES3-CBC Single-Round Known Answer Decryption Test. */
    /*******************************************************/

    des3_context = DES_CreateContext( des3_known_key,
                                      des3_cbc_known_initialization_vector,
                                      NSS_DES_EDE3_CBC, PR_FALSE );

    if( des3_context == NULL )
        return( CKR_HOST_MEMORY );

    des3_status = DES_Decrypt( des3_context, des3_computed_plaintext,
                               &des3_bytes_decrypted, FIPS_DES3_DECRYPT_LENGTH,
                               des3_cbc_known_ciphertext,
                               FIPS_DES3_ENCRYPT_LENGTH );

    DES_DestroyContext( des3_context, PR_TRUE );

    if( ( des3_status != SECSuccess ) ||
        ( des3_bytes_decrypted != FIPS_DES3_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( des3_computed_plaintext, des3_cbc_known_plaintext,
                       FIPS_DES3_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


/* AES self-test for 128-bit, 192-bit, or 256-bit key sizes*/
static CK_RV
sftk_fips_AES_PowerUpSelfTest( int aes_key_size )
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
        0x3c,0xa5,0x96,0xf3,0x34,0x6a,0x96,0xc1,
        0x03,0x88,0x16,0x7b,0x20,0xbf,0x35,0x47 };

    static const PRUint8 aes_cbc128_known_ciphertext[]  = {
        0xcf,0x15,0x1d,0x4f,0x96,0xe4,0x4f,0x63,
        0x15,0x54,0x14,0x1d,0x4e,0xd8,0xd5,0xea };

    /* AES Known Ciphertext (192-bit key). */
    static const PRUint8 aes_ecb192_known_ciphertext[] = { 
        0xa0,0x18,0x62,0xed,0x88,0x19,0xcb,0x62,
        0x88,0x1d,0x4d,0xfe,0x84,0x02,0x89,0x0e };

    static const PRUint8 aes_cbc192_known_ciphertext[]  = { 
        0x83,0xf7,0xa4,0x76,0xd1,0x6f,0x07,0xbe,
        0x07,0xbc,0x43,0x2f,0x6d,0xad,0x29,0xe1 };

    /* AES Known Ciphertext (256-bit key). */
    static const PRUint8 aes_ecb256_known_ciphertext[] = { 
        0xdb,0xa6,0x52,0x01,0x8a,0x70,0xae,0x66,
        0x3a,0x99,0xd8,0x95,0x7f,0xfb,0x01,0x67 };
    
    static const PRUint8 aes_cbc256_known_ciphertext[]  = { 
        0x37,0xea,0x07,0x06,0x31,0x1c,0x59,0x27,
        0xc5,0xc5,0x68,0x71,0x6e,0x34,0x40,0x16 };

    const PRUint8 *aes_ecb_known_ciphertext =
        ( aes_key_size == FIPS_AES_128_KEY_SIZE) ? aes_ecb128_known_ciphertext :
        ( aes_key_size == FIPS_AES_192_KEY_SIZE) ? aes_ecb192_known_ciphertext :
                                aes_ecb256_known_ciphertext;

    const PRUint8 *aes_cbc_known_ciphertext =
        ( aes_key_size == FIPS_AES_128_KEY_SIZE) ? aes_cbc128_known_ciphertext :
        ( aes_key_size == FIPS_AES_192_KEY_SIZE) ? aes_cbc192_known_ciphertext :
                                aes_cbc256_known_ciphertext;

    /* AES variables. */
    PRUint8        aes_computed_ciphertext[FIPS_AES_ENCRYPT_LENGTH];
    PRUint8        aes_computed_plaintext[FIPS_AES_DECRYPT_LENGTH];
    AESContext *   aes_context;
    unsigned int   aes_bytes_encrypted;
    unsigned int   aes_bytes_decrypted;
    SECStatus      aes_status;

    /*check if aes_key_size is 128, 192, or 256 bits */
    if ((aes_key_size != FIPS_AES_128_KEY_SIZE) && 
        (aes_key_size != FIPS_AES_192_KEY_SIZE) && 
        (aes_key_size != FIPS_AES_256_KEY_SIZE)) 
            return( CKR_DEVICE_ERROR );

    /******************************************************/
    /* AES-ECB Single-Round Known Answer Encryption Test: */
    /******************************************************/

    aes_context = AES_CreateContext( aes_known_key, NULL, NSS_AES, PR_TRUE,
                                     aes_key_size, FIPS_AES_BLOCK_SIZE );

    if( aes_context == NULL )
        return( CKR_HOST_MEMORY );

    aes_status = AES_Encrypt( aes_context, aes_computed_ciphertext,
                              &aes_bytes_encrypted, FIPS_AES_ENCRYPT_LENGTH,
                              aes_known_plaintext,
                              FIPS_AES_DECRYPT_LENGTH );
    
    AES_DestroyContext( aes_context, PR_TRUE );

    if( ( aes_status != SECSuccess ) ||
        ( aes_bytes_encrypted != FIPS_AES_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( aes_computed_ciphertext, aes_ecb_known_ciphertext,
                       FIPS_AES_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* AES-ECB Single-Round Known Answer Decryption Test: */
    /******************************************************/

    aes_context = AES_CreateContext( aes_known_key, NULL, NSS_AES, PR_FALSE,
                                     aes_key_size, FIPS_AES_BLOCK_SIZE );

    if( aes_context == NULL )
        return( CKR_HOST_MEMORY );

    aes_status = AES_Decrypt( aes_context, aes_computed_plaintext,
                              &aes_bytes_decrypted, FIPS_AES_DECRYPT_LENGTH,
                              aes_ecb_known_ciphertext,
                              FIPS_AES_ENCRYPT_LENGTH );

    AES_DestroyContext( aes_context, PR_TRUE );

    if( ( aes_status != SECSuccess ) ||         
        ( aes_bytes_decrypted != FIPS_AES_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( aes_computed_plaintext, aes_known_plaintext,
                       FIPS_AES_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* AES-CBC Single-Round Known Answer Encryption Test. */
    /******************************************************/

    aes_context = AES_CreateContext( aes_known_key,
                                     aes_cbc_known_initialization_vector,
                                     NSS_AES_CBC, PR_TRUE, aes_key_size, 
                                     FIPS_AES_BLOCK_SIZE );

    if( aes_context == NULL )
        return( CKR_HOST_MEMORY );

    aes_status = AES_Encrypt( aes_context, aes_computed_ciphertext,
                              &aes_bytes_encrypted, FIPS_AES_ENCRYPT_LENGTH,
                              aes_known_plaintext,
                              FIPS_AES_DECRYPT_LENGTH );

    AES_DestroyContext( aes_context, PR_TRUE );

    if( ( aes_status != SECSuccess ) ||
        ( aes_bytes_encrypted != FIPS_AES_ENCRYPT_LENGTH ) ||
        ( PORT_Memcmp( aes_computed_ciphertext, aes_cbc_known_ciphertext,
                       FIPS_AES_ENCRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );


    /******************************************************/
    /* AES-CBC Single-Round Known Answer Decryption Test. */
    /******************************************************/

    aes_context = AES_CreateContext( aes_known_key,
                                     aes_cbc_known_initialization_vector,
                                     NSS_AES_CBC, PR_FALSE, aes_key_size, 
                                     FIPS_AES_BLOCK_SIZE );

    if( aes_context == NULL )
        return( CKR_HOST_MEMORY );

    aes_status = AES_Decrypt( aes_context, aes_computed_plaintext,
                              &aes_bytes_decrypted, FIPS_AES_DECRYPT_LENGTH,
                              aes_cbc_known_ciphertext,
                              FIPS_AES_ENCRYPT_LENGTH );

    AES_DestroyContext( aes_context, PR_TRUE );

    if( ( aes_status != SECSuccess ) ||
        ( aes_bytes_decrypted != FIPS_AES_DECRYPT_LENGTH ) ||
        ( PORT_Memcmp( aes_computed_plaintext, aes_known_plaintext,
                       FIPS_AES_DECRYPT_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}

/* Known Hash Message (512-bits).  Used for all hashes (incl. SHA-N [N>1]). */
static const PRUint8 known_hash_message[] = {
  "The test message for the MD2, MD5, and SHA-1 hashing algorithms." };


static CK_RV
sftk_fips_MD2_PowerUpSelfTest( void )
{
    /* MD2 Known Digest Message (128-bits). */
    static const PRUint8 md2_known_digest[]  = {
                                   0x41,0x5a,0x12,0xb2,0x3f,0x28,0x97,0x17,
                                   0x0c,0x71,0x4e,0xcc,0x40,0xc8,0x1d,0x1b};

    /* MD2 variables. */
    MD2Context * md2_context;
    unsigned int md2_bytes_hashed;
    PRUint8      md2_computed_digest[MD2_LENGTH];


    /***********************************************/
    /* MD2 Single-Round Known Answer Hashing Test. */
    /***********************************************/

    md2_context = MD2_NewContext();

    if( md2_context == NULL )
        return( CKR_HOST_MEMORY );

    MD2_Begin( md2_context );

    MD2_Update( md2_context, known_hash_message,
                FIPS_KNOWN_HASH_MESSAGE_LENGTH );

    MD2_End( md2_context, md2_computed_digest, &md2_bytes_hashed, MD2_LENGTH );

    MD2_DestroyContext( md2_context , PR_TRUE );
    
    if( ( md2_bytes_hashed != MD2_LENGTH ) ||
        ( PORT_Memcmp( md2_computed_digest, md2_known_digest,
                       MD2_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
sftk_fips_MD5_PowerUpSelfTest( void )
{
    /* MD5 Known Digest Message (128-bits). */
    static const PRUint8 md5_known_digest[]  = {
				   0x25,0xc8,0xc0,0x10,0xc5,0x6e,0x68,0x28,
				   0x28,0xa4,0xa5,0xd2,0x98,0x9a,0xea,0x2d};

    /* MD5 variables. */
    PRUint8        md5_computed_digest[MD5_LENGTH];
    SECStatus      md5_status;


    /***********************************************/
    /* MD5 Single-Round Known Answer Hashing Test. */
    /***********************************************/

    md5_status = MD5_HashBuf( md5_computed_digest, known_hash_message,
                              FIPS_KNOWN_HASH_MESSAGE_LENGTH );

    if( ( md5_status != SECSuccess ) ||
        ( PORT_Memcmp( md5_computed_digest, md5_known_digest,
                       MD5_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}

/****************************************************/
/* Single Round HMAC SHA-X test                     */
/****************************************************/
static SECStatus
sftk_fips_HMAC(unsigned char *hmac_computed,
               const PRUint8 *secret_key,
               unsigned int secret_key_length,
               const PRUint8 *message,
               unsigned int message_length,
               HASH_HashType hashAlg )
{
    SECStatus hmac_status = SECFailure;
    HMACContext *cx = NULL;
    SECHashObject *hashObj = NULL;
    unsigned int bytes_hashed = 0;

    hashObj = (SECHashObject *) HASH_GetRawHashObject(hashAlg);
 
    if (!hashObj) 
        return( SECFailure );

    cx = HMAC_Create(hashObj, secret_key, 
                     secret_key_length, 
                     PR_TRUE);  /* PR_TRUE for in FIPS mode */

    if (cx == NULL) 
        return( SECFailure );

    HMAC_Begin(cx);
    HMAC_Update(cx, message, message_length);
    hmac_status = HMAC_Finish(cx, hmac_computed, &bytes_hashed, 
                              hashObj->length);

    HMAC_Destroy(cx, PR_TRUE);

    return( hmac_status );
}

static CK_RV
sftk_fips_HMAC_PowerUpSelfTest( void )
{
    static const PRUint8 HMAC_known_secret_key[] = {
                         "Firefox and ThunderBird are awesome!"};

    static const PRUint8 HMAC_known_secret_key_length 
                         = sizeof HMAC_known_secret_key;

    /* known SHA1 hmac (20 bytes) */
    static const PRUint8 known_SHA1_hmac[] = {
        0xd5, 0x85, 0xf6, 0x5b, 0x39, 0xfa, 0xb9, 0x05, 
        0x3b, 0x57, 0x1d, 0x61, 0xe7, 0xb8, 0x84, 0x1e, 
        0x5d, 0x0e, 0x1e, 0x11};

    /* known SHA224 hmac (28 bytes) */
    static const PRUint8 known_SHA224_hmac[] = {
        0x1c, 0xc3, 0x06, 0x8e, 0xce, 0x37, 0x68, 0xfb, 
        0x1a, 0x82, 0x4a, 0xbe, 0x2b, 0x00, 0x51, 0xf8,
        0x9d, 0xb6, 0xe0, 0x90, 0x0d, 0x00, 0xc9, 0x64,
        0x9a, 0xb8, 0x98, 0x4e};

    /* known SHA256 hmac (32 bytes) */
    static const PRUint8 known_SHA256_hmac[] = {
        0x05, 0x75, 0x9a, 0x9e, 0x70, 0x5e, 0xe7, 0x44, 
        0xe2, 0x46, 0x4b, 0x92, 0x22, 0x14, 0x22, 0xe0, 
        0x1b, 0x92, 0x8a, 0x0c, 0xfe, 0xf5, 0x49, 0xe9, 
        0xa7, 0x1b, 0x56, 0x7d, 0x1d, 0x29, 0x40, 0x48};        

    /* known SHA384 hmac (48 bytes) */
    static const PRUint8 known_SHA384_hmac[] = {
        0xcd, 0x56, 0x14, 0xec, 0x05, 0x53, 0x06, 0x2b,
        0x7e, 0x9c, 0x8a, 0x18, 0x5e, 0xea, 0xf3, 0x91,
        0x33, 0xfb, 0x64, 0xf6, 0xe3, 0x9f, 0x89, 0x0b,
        0xaf, 0xbe, 0x83, 0x4d, 0x3f, 0x3c, 0x43, 0x4d,
        0x4a, 0x0c, 0x56, 0x98, 0xf8, 0xca, 0xb4, 0xaa,
        0x9a, 0xf4, 0x0a, 0xaf, 0x4f, 0x69, 0xca, 0x87};

    /* known SHA512 hmac (64 bytes) */
    static const PRUint8 known_SHA512_hmac[] = {
        0xf6, 0x0e, 0x97, 0x12, 0x00, 0x67, 0x6e, 0xb9,
        0x0c, 0xb2, 0x63, 0xf0, 0x60, 0xac, 0x75, 0x62,
        0x70, 0x95, 0x2a, 0x52, 0x22, 0xee, 0xdd, 0xd2,
        0x71, 0xb1, 0xe8, 0x26, 0x33, 0xd3, 0x13, 0x27,
        0xcb, 0xff, 0x44, 0xef, 0x87, 0x97, 0x16, 0xfb,
        0xd3, 0x0b, 0x48, 0xbe, 0x12, 0x4e, 0xda, 0xb1,
        0x89, 0x90, 0xfb, 0x06, 0x0c, 0xbe, 0xe5, 0xc4,
        0xff, 0x24, 0x37, 0x3d, 0xc7, 0xe4, 0xe4, 0x37};

    SECStatus    hmac_status;
    PRUint8      hmac_computed[HASH_LENGTH_MAX]; 

    /***************************************************/
    /* HMAC SHA-1 Single-Round Known Answer HMAC Test. */
    /***************************************************/

    hmac_status = sftk_fips_HMAC(hmac_computed, 
                                 HMAC_known_secret_key,
                                 HMAC_known_secret_key_length,
                                 known_hash_message,
                                 FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                 HASH_AlgSHA1); 

    if( ( hmac_status != SECSuccess ) || 
        ( PORT_Memcmp( hmac_computed, known_SHA1_hmac,
                       SHA1_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* HMAC SHA-224 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = sftk_fips_HMAC(hmac_computed, 
                                 HMAC_known_secret_key,
                                 HMAC_known_secret_key_length,
                                 known_hash_message,
                                 FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                 HASH_AlgSHA224);

    if( ( hmac_status != SECSuccess ) || 
        ( PORT_Memcmp( hmac_computed, known_SHA224_hmac,
                       SHA224_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* HMAC SHA-256 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = sftk_fips_HMAC(hmac_computed, 
                                 HMAC_known_secret_key,
                                 HMAC_known_secret_key_length,
                                 known_hash_message,
                                 FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                 HASH_AlgSHA256); 

    if( ( hmac_status != SECSuccess ) || 
        ( PORT_Memcmp( hmac_computed, known_SHA256_hmac,
                       SHA256_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* HMAC SHA-384 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = sftk_fips_HMAC(hmac_computed,
                                 HMAC_known_secret_key,
                                 HMAC_known_secret_key_length,
                                 known_hash_message,
                                 FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                 HASH_AlgSHA384);

    if( ( hmac_status != SECSuccess ) ||
        ( PORT_Memcmp( hmac_computed, known_SHA384_hmac,
                       SHA384_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* HMAC SHA-512 Single-Round Known Answer Test.    */
    /***************************************************/

    hmac_status = sftk_fips_HMAC(hmac_computed,
                                 HMAC_known_secret_key,
                                 HMAC_known_secret_key_length,
                                 known_hash_message,
                                 FIPS_KNOWN_HASH_MESSAGE_LENGTH,
                                 HASH_AlgSHA512);

    if( ( hmac_status != SECSuccess ) ||
        ( PORT_Memcmp( hmac_computed, known_SHA512_hmac,
                       SHA512_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}

static CK_RV
sftk_fips_SHA_PowerUpSelfTest( void )
{
    /* SHA-1 Known Digest Message (160-bits). */
    static const PRUint8 sha1_known_digest[] = {
			       0x0a,0x6d,0x07,0xba,0x1e,0xbd,0x8a,0x1b,
			       0x72,0xf6,0xc7,0x22,0xf1,0x27,0x9f,0xf0,
			       0xe0,0x68,0x47,0x7a};

    /* SHA-224 Known Digest Message (224-bits). */
    static const PRUint8 sha224_known_digest[] = {
        0x89,0x5e,0x7f,0xfd,0x0e,0xd8,0x35,0x6f,
        0x64,0x6d,0xf2,0xde,0x5e,0xed,0xa6,0x7f, 
        0x29,0xd1,0x12,0x73,0x42,0x84,0x95,0x4f, 
        0x8e,0x08,0xe5,0xcb};

    /* SHA-256 Known Digest Message (256-bits). */
    static const PRUint8 sha256_known_digest[] = {
        0x38,0xa9,0xc1,0xf0,0x35,0xf6,0x5d,0x61,
        0x11,0xd4,0x0b,0xdc,0xce,0x35,0x14,0x8d,
        0xf2,0xdd,0xaf,0xaf,0xcf,0xb7,0x87,0xe9,
        0x96,0xa5,0xd2,0x83,0x62,0x46,0x56,0x79};
 
    /* SHA-384 Known Digest Message (384-bits). */
    static const PRUint8 sha384_known_digest[] = {
        0x11,0xfe,0x1c,0x00,0x89,0x48,0xde,0xb3,
        0x99,0xee,0x1c,0x18,0xb4,0x10,0xfb,0xfe,
        0xe3,0xa8,0x2c,0xf3,0x04,0xb0,0x2f,0xc8,
        0xa3,0xc4,0x5e,0xea,0x7e,0x60,0x48,0x7b,
        0xce,0x2c,0x62,0xf7,0xbc,0xa7,0xe8,0xa3,
        0xcf,0x24,0xce,0x9c,0xe2,0x8b,0x09,0x72};

    /* SHA-512 Known Digest Message (512-bits). */
    static const PRUint8 sha512_known_digest[] = {
        0xc8,0xb3,0x27,0xf9,0x0b,0x24,0xc8,0xbf,
        0x4c,0xba,0x33,0x54,0xf2,0x31,0xbf,0xdb,
        0xab,0xfd,0xb3,0x15,0xd7,0xfa,0x48,0x99,
        0x07,0x60,0x0f,0x57,0x41,0x1a,0xdd,0x28,
        0x12,0x55,0x25,0xac,0xba,0x3a,0x99,0x12,
        0x2c,0x7a,0x8f,0x75,0x3a,0xe1,0x06,0x6f,
        0x30,0x31,0xc9,0x33,0xc6,0x1b,0x90,0x1a,
        0x6c,0x98,0x9a,0x87,0xd0,0xb2,0xf8,0x07};

    /* SHA-X variables. */
    PRUint8        sha_computed_digest[HASH_LENGTH_MAX];
    SECStatus      sha_status;

    /*************************************************/
    /* SHA-1 Single-Round Known Answer Hashing Test. */
    /*************************************************/

    sha_status = SHA1_HashBuf( sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH );
 
    if( ( sha_status != SECSuccess ) ||
        ( PORT_Memcmp( sha_computed_digest, sha1_known_digest,
                       SHA1_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* SHA-224 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA224_HashBuf( sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH );

    if( ( sha_status != SECSuccess ) ||
        ( PORT_Memcmp( sha_computed_digest, sha224_known_digest,
                       SHA224_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* SHA-256 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA256_HashBuf( sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH );

    if( ( sha_status != SECSuccess ) ||
        ( PORT_Memcmp( sha_computed_digest, sha256_known_digest,
                       SHA256_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* SHA-384 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA384_HashBuf( sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH );

    if( ( sha_status != SECSuccess ) ||
        ( PORT_Memcmp( sha_computed_digest, sha384_known_digest,
                       SHA384_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    /***************************************************/
    /* SHA-512 Single-Round Known Answer Hashing Test. */
    /***************************************************/

    sha_status = SHA512_HashBuf( sha_computed_digest, known_hash_message,
                                FIPS_KNOWN_HASH_MESSAGE_LENGTH );

    if( ( sha_status != SECSuccess ) ||
        ( PORT_Memcmp( sha_computed_digest, sha512_known_digest,
                       SHA512_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}

/*
* Single round RSA Signature Known Answer Test
*/
static SECStatus
sftk_fips_RSA_PowerUpSigSelfTest (HASH_HashType  shaAlg,
                                  NSSLOWKEYPublicKey *rsa_public_key,
                                  NSSLOWKEYPrivateKey *rsa_private_key,
                                  const unsigned char *rsa_known_msg,
                                  const unsigned int rsa_kmsg_length,
                                  const unsigned char *rsa_known_signature)
{
    SECOidTag      shaOid;   /* SHA OID */
    unsigned char  sha[HASH_LENGTH_MAX];    /* SHA digest */
    unsigned int   shaLength = 0;           /* length of SHA */
    unsigned int   rsa_bytes_signed;
    unsigned char  rsa_computed_signature[FIPS_RSA_SIGNATURE_LENGTH];
    SECStatus      rv;

    if (shaAlg == HASH_AlgSHA1) {
        if (SHA1_HashBuf(sha, rsa_known_msg, rsa_kmsg_length)
                            != SECSuccess) {
             goto loser;
        }
        shaLength = SHA1_LENGTH;
        shaOid = SEC_OID_SHA1;
    } else if (shaAlg == HASH_AlgSHA256) {
        if (SHA256_HashBuf(sha, rsa_known_msg, rsa_kmsg_length)
                            != SECSuccess) {
             goto loser;
        }
        shaLength = SHA256_LENGTH;
        shaOid = SEC_OID_SHA256;
    } else if (shaAlg == HASH_AlgSHA384) {
        if (SHA384_HashBuf(sha, rsa_known_msg, rsa_kmsg_length)
                            != SECSuccess) {
             goto loser;
        }
        shaLength = SHA384_LENGTH;
        shaOid = SEC_OID_SHA384;
    } else if (shaAlg == HASH_AlgSHA512) {
        if (SHA512_HashBuf(sha, rsa_known_msg, rsa_kmsg_length)
                            != SECSuccess) {
             goto loser;
        }
        shaLength = SHA512_LENGTH;
        shaOid = SEC_OID_SHA512;
    } else {
        goto loser;
    }

    /*************************************************/
    /* RSA Single-Round Known Answer Signature Test. */
    /*************************************************/

    /* Perform RSA signature with the RSA private key. */
    rv = RSA_HashSign( shaOid,
                       rsa_private_key,
                       rsa_computed_signature,
                       &rsa_bytes_signed,
                       FIPS_RSA_SIGNATURE_LENGTH,
                       sha,
                       shaLength);

    if( ( rv != SECSuccess ) ||
        ( rsa_bytes_signed != FIPS_RSA_SIGNATURE_LENGTH ) ||
        ( PORT_Memcmp( rsa_computed_signature, rsa_known_signature,
                       FIPS_RSA_SIGNATURE_LENGTH ) != 0 ) ) {
        goto loser;
    }

    /****************************************************/
    /* RSA Single-Round Known Answer Verification Test. */
    /****************************************************/

    /* Perform RSA verification with the RSA public key. */
    rv = RSA_HashCheckSign( shaOid,
                            rsa_public_key,
                            rsa_computed_signature,
                            rsa_bytes_signed,
                            sha,
                            shaLength);

    if( rv != SECSuccess ) {
         goto loser;
    }
    return( SECSuccess );

loser:

    return( SECFailure );

}

static CK_RV
sftk_fips_RSA_PowerUpSelfTest( void )
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
                            0x2f, 0x4b, 0x65, 0x82, 0x14, 0xdd, 0x29, 0x5b};

    /* RSA Known Public Key Values (24-bits). */
    static const PRUint8 rsa_public_exponent[FIPS_RSA_PUBLIC_EXPONENT_LENGTH] 
                                                       = { 0x01, 0x00, 0x01 };
    /* RSA Known Private Key Values (version                 is    8-bits), */
    /*                              (private exponent        is 2048-bits), */
    /*                              (private prime0          is 1024-bits), */
    /*                              (private prime1          is 1024-bits), */
    /*                              (private prime exponent0 is 1024-bits), */
    /*                              (private prime exponent1 is 1024-bits), */
    /*                          and (private coefficient     is 1024-bits). */
    static const PRUint8 rsa_version[] = { 0x00 };

    static const PRUint8 rsa_private_exponent[FIPS_RSA_PRIVATE_EXPONENT_LENGTH]
                         = {0x29, 0x08, 0x05, 0x53, 0x89, 0x76, 0xe6, 0x6c,
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
                            0xcc, 0x39, 0xe5, 0xfb, 0x13, 0x7b, 0x6f, 0x81 };

    static const PRUint8 rsa_prime0[FIPS_RSA_PRIME0_LENGTH]   = {
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
                            0x33, 0xe3, 0x81, 0xea, 0x0c, 0x6c, 0xa2, 0x05};
    static const PRUint8 rsa_prime1[FIPS_RSA_PRIME1_LENGTH]   = {
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
                            0x22, 0xe4, 0xe7, 0xfe, 0x36, 0x07, 0x9b, 0xdf};
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
                            0x41, 0xcf, 0x7f, 0x66, 0x53, 0xac, 0x31, 0x7d};
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
                            0x4c, 0xe8, 0x0e, 0xd0, 0xc9, 0xaa, 0xdf, 0x59};
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
                            0xcf, 0x0c, 0x0d, 0x60, 0x09, 0xa9, 0x44, 0x02};

    /* RSA Known Plaintext Message (1024-bits). */
    static const PRUint8 rsa_known_plaintext_msg[FIPS_RSA_MESSAGE_LENGTH] = {
                                         "Known plaintext message utilized"
                                         "for RSA Encryption &  Decryption"
                                         "blocks SHA256, SHA384  and      "
                                         "SHA512 RSA Signature KAT tests. "
                                         "Known plaintext message utilized"
                                         "for RSA Encryption &  Decryption"
                                         "blocks SHA256, SHA384  and      "
                                         "SHA512 RSA Signature KAT  tests."};

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
                            0x44, 0x54, 0xca, 0xbe, 0xa3, 0x0e, 0x5f, 0xef};

    /* RSA Known Signed Hash (2048-bits). */
    static const PRUint8 rsa_known_sha256_signature[] = {
                            0x8c, 0x2d, 0x2e, 0xfb, 0x37, 0xb5, 0x6f, 0x38,
                            0x9f, 0x06, 0x5a, 0xf3, 0x8c, 0xa0, 0xd0, 0x7a,
                            0xde, 0xcf, 0xf9, 0x14, 0x95, 0x59, 0xd3, 0x5f,
                            0x51, 0x5d, 0x5d, 0xad, 0xd8, 0x71, 0x33, 0x50,
                            0x1d, 0x03, 0x3b, 0x3a, 0x32, 0x00, 0xb4, 0xde,
                            0x7f, 0xe4, 0xb1, 0xe5, 0x6b, 0x83, 0xf4, 0x80,
                            0x10, 0x3b, 0xb8, 0x8a, 0xdb, 0xe8, 0x0a, 0x42,
                            0x9e, 0x8d, 0xd7, 0xbe, 0xed, 0xde, 0x5a, 0x3d,
                            0xc6, 0xdb, 0xfe, 0x49, 0x6a, 0xe9, 0x1e, 0x75,
                            0x66, 0xf1, 0x3f, 0x9e, 0x3f, 0xff, 0x05, 0x65,
                            0xde, 0xca, 0x62, 0x62, 0xf3, 0xec, 0x53, 0x09,
                            0xa0, 0x37, 0xd5, 0x66, 0x62, 0x72, 0x14, 0xb6,
                            0x51, 0x32, 0x67, 0x50, 0xc1, 0xe1, 0x2f, 0x9e,
                            0x98, 0x4e, 0x53, 0x96, 0x55, 0x4b, 0xc4, 0x92,
                            0xc3, 0xb4, 0x80, 0xf0, 0x35, 0xc9, 0x00, 0x4b,
                            0x5c, 0x85, 0x92, 0xb1, 0xe8, 0x6e, 0xa5, 0x51,
                            0x38, 0x9f, 0xc9, 0x11, 0xb6, 0x14, 0xdf, 0x34,
                            0x64, 0x40, 0x82, 0x82, 0xde, 0x16, 0x69, 0x93,
                            0x89, 0x4e, 0x5c, 0x32, 0xf2, 0x0a, 0x4e, 0x9e,
                            0xbd, 0x63, 0x99, 0x4f, 0xf3, 0x15, 0x90, 0xc2,
                            0xfe, 0x6f, 0xb7, 0xf4, 0xad, 0xd4, 0x8e, 0x0b,
                            0xd2, 0xf5, 0x22, 0xd2, 0x71, 0x65, 0x13, 0xf7,
                            0x82, 0x7b, 0x75, 0xb6, 0xc1, 0xb4, 0x45, 0xbd,
                            0x8f, 0x95, 0xcf, 0x5b, 0x95, 0x32, 0xef, 0x18,
                            0x5f, 0xd3, 0xdf, 0x7e, 0x22, 0xdd, 0x25, 0xeb,
                            0xe1, 0xbf, 0x3b, 0x9a, 0x55, 0x75, 0x4f, 0x3c,
                            0x38, 0x67, 0x57, 0x04, 0x04, 0x57, 0x27, 0xf6,
                            0x34, 0x0e, 0x57, 0x8a, 0x7c, 0xff, 0x7d, 0xca,
                            0x8c, 0x06, 0xf8, 0x9d, 0xdb, 0xe4, 0xd8, 0x19,
                            0xdd, 0x4d, 0xfd, 0x8f, 0xa0, 0x06, 0x53, 0xe8,
                            0x33, 0x00, 0x70, 0x3f, 0x6b, 0xc3, 0xbd, 0x9a,
                            0x78, 0xb5, 0xa9, 0xef, 0x6d, 0xda, 0x67, 0x92};

   /* RSA Known Signed Hash (2048-bits). */
   static const PRUint8 rsa_known_sha384_signature[] = {
                            0x20, 0x2d, 0x21, 0x3a, 0xaa, 0x1e, 0x05, 0x15,
                            0x5c, 0xca, 0x84, 0x86, 0xc0, 0x15, 0x81, 0xdf,
                            0xd4, 0x06, 0x9f, 0xe0, 0xc1, 0xed, 0xef, 0x0f,
                            0xfe, 0xb3, 0xc3, 0xbb, 0x28, 0xa5, 0x56, 0xbf,
                            0xe3, 0x11, 0x5c, 0xc2, 0xc0, 0x0b, 0xfa, 0xfa,
                            0x3d, 0xd3, 0x06, 0x20, 0xe2, 0xc9, 0xe4, 0x66,
                            0x28, 0xb7, 0xc0, 0x3b, 0x3c, 0x96, 0xc6, 0x49,
                            0x3b, 0xcf, 0x86, 0x49, 0x31, 0xaf, 0x5b, 0xa3,
                            0xec, 0x63, 0x10, 0xdf, 0xda, 0x2f, 0x68, 0xac,
                            0x7b, 0x3a, 0x49, 0xfa, 0xe6, 0x0d, 0xfe, 0x37,
                            0x17, 0x56, 0x8e, 0x5c, 0x48, 0x97, 0x43, 0xf7,
                            0xa0, 0xbc, 0xe3, 0x4b, 0x42, 0xde, 0x58, 0x1d,
                            0xd9, 0x5d, 0xb3, 0x08, 0x35, 0xbd, 0xa4, 0xe1,
                            0x80, 0xc3, 0x64, 0xab, 0x21, 0x97, 0xad, 0xfb,
                            0x71, 0xee, 0xa3, 0x3d, 0x9c, 0xaa, 0xfa, 0x16,
                            0x60, 0x46, 0x32, 0xda, 0x44, 0x2e, 0x10, 0x92,
                            0x20, 0xd8, 0x98, 0x80, 0x84, 0x75, 0x5b, 0x70,
                            0x91, 0x00, 0x33, 0x19, 0x69, 0xc9, 0x2a, 0xec,
                            0x3d, 0xe5, 0x5f, 0x0f, 0x9a, 0xa7, 0x97, 0x1f,
                            0x79, 0xc3, 0x1d, 0x65, 0x74, 0x62, 0xc5, 0xa1,
                            0x23, 0x65, 0x4b, 0x84, 0xa1, 0x03, 0x98, 0xf3,
                            0xf1, 0x02, 0x24, 0xca, 0xe5, 0xd4, 0xc8, 0xa2,
                            0x30, 0xad, 0x72, 0x7d, 0x29, 0x60, 0x1a, 0x8e,
                            0x6f, 0x23, 0xa4, 0xda, 0x68, 0xa4, 0x45, 0x9c,
                            0x39, 0x70, 0x44, 0x18, 0x4b, 0x73, 0xfe, 0xf8,
                            0x33, 0x53, 0x1d, 0x7e, 0x93, 0x93, 0xac, 0xc7,
                            0x1e, 0x6e, 0x6b, 0xfd, 0x9e, 0xba, 0xa6, 0x71,
                            0x70, 0x47, 0x6a, 0xd6, 0x82, 0x32, 0xa2, 0x6e,
                            0x20, 0x72, 0xb0, 0xba, 0xec, 0x91, 0xbb, 0x6b,
                            0xcc, 0x84, 0x0a, 0x33, 0x2b, 0x8a, 0x8d, 0xeb,
                            0x71, 0xcd, 0xca, 0x67, 0x1b, 0xad, 0x10, 0xd4,
                            0xce, 0x4f, 0xc0, 0x29, 0xec, 0xfa, 0xed, 0xfa};

   /* RSA Known Signed Hash (2048-bits). */
   static const PRUint8 rsa_known_sha512_signature[] = {
                            0x35, 0x0e, 0x74, 0x9d, 0xeb, 0xc7, 0x67, 0x31,
                            0x9f, 0xff, 0x0b, 0xbb, 0x5e, 0x66, 0xb4, 0x2f,
                            0xbf, 0x72, 0x60, 0x4f, 0xe9, 0xbd, 0xec, 0xc8,
                            0x17, 0x79, 0x5f, 0x39, 0x83, 0xb4, 0x54, 0x2e,
                            0x01, 0xb9, 0xd3, 0x20, 0x47, 0xcb, 0xd4, 0x42,
                            0xf2, 0x6e, 0x36, 0xc1, 0x97, 0xad, 0xef, 0x8e,
                            0xe6, 0x51, 0xee, 0x5e, 0x9e, 0x88, 0xb4, 0x9d,
                            0xda, 0x3e, 0x77, 0x4b, 0xe8, 0xae, 0x48, 0x53,
                            0x2c, 0xc4, 0xd3, 0x25, 0x6b, 0x23, 0xb7, 0x54,
                            0x3c, 0x95, 0x8f, 0xfb, 0x6f, 0x6d, 0xc5, 0x56,
                            0x39, 0x69, 0x28, 0x0e, 0x74, 0x9b, 0x31, 0xe8,
                            0x76, 0x77, 0x2b, 0xc1, 0x44, 0x89, 0x81, 0x93,
                            0xfc, 0xf6, 0xec, 0x5f, 0x8f, 0x89, 0xfc, 0x1d,
                            0xa4, 0x53, 0x58, 0x8c, 0xe9, 0xc0, 0xc0, 0x26,
                            0xe6, 0xdf, 0x6d, 0x27, 0xb1, 0x8e, 0x3e, 0xb6,
                            0x47, 0xe1, 0x02, 0x96, 0xc2, 0x5f, 0x7f, 0x3d,
                            0xc5, 0x6c, 0x2f, 0xea, 0xaa, 0x5e, 0x39, 0xfc,
                            0x77, 0xca, 0x00, 0x02, 0x5c, 0x64, 0x7c, 0xce,
                            0x7d, 0x63, 0x82, 0x05, 0xed, 0xf7, 0x5b, 0x55,
                            0x58, 0xc0, 0xeb, 0x76, 0xd7, 0x95, 0x55, 0x37,
                            0x85, 0x7d, 0x17, 0xad, 0xd2, 0x11, 0xfd, 0x97,
                            0x48, 0xb5, 0xc2, 0x5e, 0xc7, 0x62, 0xc0, 0xe0,
                            0x68, 0xa8, 0x61, 0x14, 0x41, 0xca, 0x25, 0x3a,
                            0xec, 0x48, 0x54, 0x22, 0x83, 0x2b, 0x69, 0x54,
                            0xfd, 0xc8, 0x99, 0x9a, 0xee, 0x37, 0x03, 0xa3,
                            0x8f, 0x0f, 0x32, 0xb0, 0xaa, 0x74, 0x39, 0x04,
                            0x7c, 0xd9, 0xc2, 0x8f, 0xbe, 0xf2, 0xc4, 0xbe,
                            0xdd, 0x7a, 0x7a, 0x7f, 0x72, 0xd3, 0x80, 0x59,
                            0x18, 0xa0, 0xa1, 0x2d, 0x6f, 0xa3, 0xa9, 0x48,
                            0xed, 0x20, 0xa6, 0xea, 0xaa, 0x10, 0x83, 0x98,
                            0x0c, 0x13, 0x69, 0x6e, 0xcd, 0x31, 0x6b, 0xd0,
                            0x66, 0xa6, 0x5e, 0x30, 0x0c, 0x82, 0xd5, 0x81};

    static const RSAPublicKey    bl_public_key = { NULL,
      { FIPS_RSA_TYPE, (unsigned char *)rsa_modulus,         
                                        FIPS_RSA_MODULUS_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_public_exponent, 
                                        FIPS_RSA_PUBLIC_EXPONENT_LENGTH }
    };
    static const RSAPrivateKey   bl_private_key = { NULL,
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
#ifdef CREATE_TEMP_ARENAS
    PLArenaPool *         rsa_public_arena;
    PLArenaPool *         rsa_private_arena;
#endif
    NSSLOWKEYPublicKey *  rsa_public_key;
    NSSLOWKEYPrivateKey * rsa_private_key;
    SECStatus             rsa_status;

    NSSLOWKEYPublicKey    low_public_key   = { NULL, NSSLOWKEYRSAKey, };
    NSSLOWKEYPrivateKey   low_private_key  = { NULL, NSSLOWKEYRSAKey, };
    PRUint8               rsa_computed_ciphertext[FIPS_RSA_ENCRYPT_LENGTH];
    PRUint8               rsa_computed_plaintext[FIPS_RSA_DECRYPT_LENGTH];

    /****************************************/
    /* Compose RSA Public/Private Key Pair. */
    /****************************************/

    low_public_key.u.rsa  = bl_public_key;
    low_private_key.u.rsa = bl_private_key;

    rsa_public_key  = &low_public_key;
    rsa_private_key = &low_private_key;

#ifdef CREATE_TEMP_ARENAS
    /* Create some space for the RSA public key. */
    rsa_public_arena = PORT_NewArena( NSS_SOFTOKEN_DEFAULT_CHUNKSIZE );

    if( rsa_public_arena == NULL ) {
        PORT_SetError( SEC_ERROR_NO_MEMORY );
        return( CKR_HOST_MEMORY );
    }

    /* Create some space for the RSA private key. */
    rsa_private_arena = PORT_NewArena( NSS_SOFTOKEN_DEFAULT_CHUNKSIZE );

    if( rsa_private_arena == NULL ) {
        PORT_FreeArena( rsa_public_arena, PR_TRUE );
        PORT_SetError( SEC_ERROR_NO_MEMORY );
        return( CKR_HOST_MEMORY );
    }

    rsa_public_key->arena = rsa_public_arena;
    rsa_private_key->arena = rsa_private_arena;
#endif

    /**************************************************/
    /* RSA Single-Round Known Answer Encryption Test. */
    /**************************************************/

    /* Perform RSA Public Key Encryption. */
    rsa_status = RSA_PublicKeyOp(&rsa_public_key->u.rsa,
                                 rsa_computed_ciphertext,
                                 rsa_known_plaintext_msg);

    if( ( rsa_status != SECSuccess ) ||
        ( PORT_Memcmp( rsa_computed_ciphertext, rsa_known_ciphertext,
                       FIPS_RSA_ENCRYPT_LENGTH ) != 0 ) )
        goto rsa_loser;

    /**************************************************/
    /* RSA Single-Round Known Answer Decryption Test. */
    /**************************************************/

    /* Perform RSA Private Key Decryption. */
    rsa_status = RSA_PrivateKeyOp(&rsa_private_key->u.rsa,
                                  rsa_computed_plaintext,
                                  rsa_known_ciphertext);

    if( ( rsa_status != SECSuccess ) ||
        ( PORT_Memcmp( rsa_computed_plaintext, rsa_known_plaintext_msg,
                       FIPS_RSA_DECRYPT_LENGTH ) != 0 ) )
        goto rsa_loser;

    rsa_status = sftk_fips_RSA_PowerUpSigSelfTest (HASH_AlgSHA256,
                           rsa_public_key, rsa_private_key,
                           rsa_known_plaintext_msg, FIPS_RSA_MESSAGE_LENGTH,
                           rsa_known_sha256_signature);
    if( rsa_status != SECSuccess )
        goto rsa_loser;

    rsa_status = sftk_fips_RSA_PowerUpSigSelfTest (HASH_AlgSHA384,
                           rsa_public_key, rsa_private_key,
                           rsa_known_plaintext_msg, FIPS_RSA_MESSAGE_LENGTH,
                           rsa_known_sha384_signature);
    if( rsa_status != SECSuccess )
        goto rsa_loser;

    rsa_status = sftk_fips_RSA_PowerUpSigSelfTest (HASH_AlgSHA512,
                           rsa_public_key, rsa_private_key,
                           rsa_known_plaintext_msg, FIPS_RSA_MESSAGE_LENGTH,
                           rsa_known_sha512_signature);
    if( rsa_status != SECSuccess )
        goto rsa_loser;

    /* Dispose of all RSA key material. */
    nsslowkey_DestroyPublicKey( rsa_public_key );
    nsslowkey_DestroyPrivateKey( rsa_private_key );

    return( CKR_OK );

rsa_loser:

    nsslowkey_DestroyPublicKey( rsa_public_key );
    nsslowkey_DestroyPrivateKey( rsa_private_key );

    return( CKR_DEVICE_ERROR );
}

#ifndef NSS_DISABLE_ECC

static CK_RV
sftk_fips_ECDSA_Test(const PRUint8 *encodedParams, 
                     unsigned int encodedParamsLen,
                     const PRUint8 *knownSignature, 
                     unsigned int knownSignatureLen) {

    /* ECDSA Known Seed info for curves nistp256 and nistk283  */
    static const PRUint8 ecdsa_Known_Seed[] = {
                            0x6a, 0x9b, 0xf6, 0xf7, 0xce, 0xed, 0x79, 0x11,
                            0xf0, 0xc7, 0xc8, 0x9a, 0xa5, 0xd1, 0x57, 0xb1,
                            0x7b, 0x5a, 0x3b, 0x76, 0x4e, 0x7b, 0x7c, 0xbc,
                            0xf2, 0x76, 0x1c, 0x1c, 0x7f, 0xc5, 0x53, 0x2f};

    static const PRUint8 msg[] = {
                            "Firefox and ThunderBird are awesome!"};

    unsigned char sha1[SHA1_LENGTH];  /* SHA-1 hash (160 bits) */
    unsigned char sig[2*MAX_ECKEY_LEN];
    SECItem signature, digest;
    SECItem encodedparams;
    ECParams *ecparams = NULL;
    ECPrivateKey *ecdsa_private_key = NULL;
    ECPublicKey ecdsa_public_key;
    SECStatus ecdsaStatus = SECSuccess;

    /* construct the ECDSA private/public key pair */
    encodedparams.type = siBuffer;
    encodedparams.data = (unsigned char *) encodedParams;
    encodedparams.len = encodedParamsLen;
    
    if (EC_DecodeParams(&encodedparams, &ecparams) != SECSuccess) {
        return( CKR_DEVICE_ERROR );
    }

    /* Generates a new EC key pair. The private key is a supplied
     * random value (in seed) and the public key is the result of 
     * performing a scalar point multiplication of that value with 
     * the curve's base point.
     */
    ecdsaStatus = EC_NewKeyFromSeed(ecparams, &ecdsa_private_key, 
                                   ecdsa_Known_Seed, 
                                   sizeof(ecdsa_Known_Seed));
    /* free the ecparams they are no longer needed */
    PORT_FreeArena(ecparams->arena, PR_FALSE);
    ecparams = NULL;
    if (ecdsaStatus != SECSuccess) {
        return ( CKR_DEVICE_ERROR );    
    }

    /* construct public key from private key. */
    ecdsaStatus = EC_CopyParams(ecdsa_private_key->ecParams.arena, 
                                &ecdsa_public_key.ecParams,
                                &ecdsa_private_key->ecParams);
    if (ecdsaStatus != SECSuccess) {
        goto loser;
    }
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

    if( ( signature.len != knownSignatureLen ) ||
        ( PORT_Memcmp( signature.data, knownSignature,
                    knownSignatureLen ) != 0 ) ) {
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
    if (ecdsa_private_key != NULL) {
        PORT_FreeArena(ecdsa_private_key->ecParams.arena, PR_FALSE);
    }

    if (ecdsaStatus != SECSuccess) {
        return CKR_DEVICE_ERROR ;
    }
    return( CKR_OK );
}

static CK_RV
sftk_fips_ECDSA_PowerUpSelfTest() {

   /* ECDSA Known curve nistp256 == SEC_OID_SECG_EC_SECP256R1 params */
    static const PRUint8 ecdsa_known_P256_EncodedParams[] = {
                            0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,
                            0x01,0x07};

    static const PRUint8 ecdsa_known_P256_signature[] = {
                            0x07,0xb1,0xcb,0x57,0x20,0xa7,0x10,0xd6, 
                            0x9d,0x37,0x4b,0x1c,0xdc,0x35,0x90,0xff, 
                            0x1a,0x2d,0x98,0x95,0x1b,0x2f,0xeb,0x7f, 
                            0xbb,0x81,0xca,0xc0,0x69,0x75,0xea,0xc5,
                            0x59,0x6a,0x62,0x49,0x3d,0x50,0xc9,0xe1, 
                            0x27,0x3b,0xff,0x9b,0x13,0x66,0x67,0xdd, 
                            0x7d,0xd1,0x0d,0x2d,0x7c,0x44,0x04,0x1b, 
                            0x16,0x21,0x12,0xc5,0xcb,0xbd,0x9e,0x75};

#ifdef NSS_ECC_MORE_THAN_SUITE_B
    /* ECDSA Known curve nistk283 == SEC_OID_SECG_EC_SECT283K1 params */
    static const PRUint8 ecdsa_known_K283_EncodedParams[] = {
                            0x06,0x05,0x2b,0x81,0x04,0x00,0x10};
                         
    static const PRUint8 ecdsa_known_K283_signature[] = {
                            0x00,0x45,0x88,0xc0,0x79,0x09,0x07,0xd1, 
                            0x4e,0x88,0xe6,0xd5,0x2f,0x22,0x04,0x74, 
                            0x35,0x24,0x65,0xe8,0x15,0xde,0x90,0x66, 
                            0x94,0x70,0xdd,0x3a,0x14,0x70,0x02,0xd1,
                            0xef,0x86,0xbd,0x15,0x00,0xd9,0xdc,0xfc, 
                            0x87,0x2e,0x7c,0x99,0xe2,0xe3,0x79,0xb8, 
                            0xd9,0x10,0x49,0x78,0x4b,0x59,0x8b,0x05, 
                            0x77,0xec,0x6c,0xe8,0x35,0xe6,0x2e,0xa9,
                            0xf9,0x77,0x1f,0x71,0x86,0xa5,0x4a,0xd0};
#endif

    CK_RV crv;

    /* ECDSA GF(p) prime field curve test */
    crv = sftk_fips_ECDSA_Test(ecdsa_known_P256_EncodedParams,
                               sizeof ecdsa_known_P256_EncodedParams,
                               ecdsa_known_P256_signature,
                               sizeof ecdsa_known_P256_signature );
    if (crv != CKR_OK) {
        return( CKR_DEVICE_ERROR );
    }

#ifdef NSS_ECC_MORE_THAN_SUITE_B
    /* ECDSA GF(2m) binary field curve test */
    crv = sftk_fips_ECDSA_Test(ecdsa_known_K283_EncodedParams,
                               sizeof ecdsa_known_K283_EncodedParams,
                               ecdsa_known_K283_signature,
                               sizeof ecdsa_known_K283_signature );
    if (crv != CKR_OK) {
        return( CKR_DEVICE_ERROR );
    }
#endif

    return( CKR_OK );
}

#endif /* NSS_DISABLE_ECC */

static CK_RV
sftk_fips_DSA_PowerUpSelfTest( void )
{
    /* DSA Known P (1024-bits), Q (160-bits), and G (1024-bits) Values. */
    static const PRUint8 dsa_P[] = {
         0x80,0xb0,0xd1,0x9d,0x6e,0xa4,0xf3,0x28, 
         0x9f,0x24,0xa9,0x8a,0x49,0xd0,0x0c,0x63, 
         0xe8,0x59,0x04,0xf9,0x89,0x4a,0x5e,0xc0, 
         0x6d,0xd2,0x67,0x6b,0x37,0x81,0x83,0x0c,
         0xfe,0x3a,0x8a,0xfd,0xa0,0x3b,0x08,0x91, 
         0x1c,0xcb,0xb5,0x63,0xb0,0x1c,0x70,0xd0, 
         0xae,0xe1,0x60,0x2e,0x12,0xeb,0x54,0xc7, 
         0xcf,0xc6,0xcc,0xae,0x97,0x52,0x32,0x63,
         0xd3,0xeb,0x55,0xea,0x2f,0x4c,0xd5,0xd7, 
         0x3f,0xda,0xec,0x49,0x27,0x0b,0x14,0x56, 
         0xc5,0x09,0xbe,0x4d,0x09,0x15,0x75,0x2b, 
         0xa3,0x42,0x0d,0x03,0x71,0xdf,0x0f,0xf4,
         0x0e,0xe9,0x0c,0x46,0x93,0x3d,0x3f,0xa6, 
         0x6c,0xdb,0xca,0xe5,0xac,0x96,0xc8,0x64, 
         0x5c,0xec,0x4b,0x35,0x65,0xfc,0xfb,0x5a, 
         0x1b,0x04,0x1b,0xa1,0x0e,0xfd,0x88,0x15};
        
    static const PRUint8 dsa_Q[] = {
         0xad,0x22,0x59,0xdf,0xe5,0xec,0x4c,0x6e, 
         0xf9,0x43,0xf0,0x4b,0x2d,0x50,0x51,0xc6, 
         0x91,0x99,0x8b,0xcf};
        
    static const PRUint8 dsa_G[] = {
         0x78,0x6e,0xa9,0xd8,0xcd,0x4a,0x85,0xa4, 
         0x45,0xb6,0x6e,0x5d,0x21,0x50,0x61,0xf6, 
         0x5f,0xdf,0x5c,0x7a,0xde,0x0d,0x19,0xd3, 
         0xc1,0x3b,0x14,0xcc,0x8e,0xed,0xdb,0x17,
         0xb6,0xca,0xba,0x86,0xa9,0xea,0x51,0x2d, 
         0xc1,0xa9,0x16,0xda,0xf8,0x7b,0x59,0x8a, 
         0xdf,0xcb,0xa4,0x67,0x00,0x44,0xea,0x24, 
         0x73,0xe5,0xcb,0x4b,0xaf,0x2a,0x31,0x25,
         0x22,0x28,0x3f,0x16,0x10,0x82,0xf7,0xeb, 
         0x94,0x0d,0xdd,0x09,0x22,0x14,0x08,0x79, 
         0xba,0x11,0x0b,0xf1,0xff,0x2d,0x67,0xac, 
         0xeb,0xb6,0x55,0x51,0x69,0x97,0xa7,0x25,
         0x6b,0x9c,0xa0,0x9b,0xd5,0x08,0x9b,0x27, 
         0x42,0x1c,0x7a,0x69,0x57,0xe6,0x2e,0xed, 
         0xa9,0x5b,0x25,0xe8,0x1f,0xd2,0xed,0x1f, 
         0xdf,0xe7,0x80,0x17,0xba,0x0d,0x4d,0x38};

    /* DSA Known Random Values (known random key block       is 160-bits)  */
    /*                     and (known random signature block is 160-bits). */
    static const PRUint8 dsa_known_random_key_block[] = {
                                                      "Mozilla Rules World!"};
    static const PRUint8 dsa_known_random_signature_block[] = {
                                                      "Random DSA Signature"};

    /* DSA Known Digest (160-bits) */
    static const PRUint8 dsa_known_digest[] = { "DSA Signature Digest" };

    /* DSA Known Signature (320-bits). */
    static const PRUint8 dsa_known_signature[] = {
        0x25,0x7c,0x3a,0x79,0x32,0x45,0xb7,0x32, 
        0x70,0xca,0x62,0x63,0x2b,0xf6,0x29,0x2c, 
        0x22,0x2a,0x03,0xce,0x48,0x15,0x11,0x72, 
        0x7b,0x7e,0xf5,0x7a,0xf3,0x10,0x3b,0xde,
        0x34,0xc1,0x9e,0xd7,0x27,0x9e,0x77,0x38};

    /* DSA variables. */
    DSAPrivateKey *        dsa_private_key;
    SECStatus              dsa_status;
    SECItem                dsa_signature_item;
    SECItem                dsa_digest_item;
    DSAPublicKey           dsa_public_key;
    PRUint8                dsa_computed_signature[FIPS_DSA_SIGNATURE_LENGTH];
    static const PQGParams dsa_pqg = { NULL,
			    { FIPS_DSA_TYPE, (unsigned char *)dsa_P, FIPS_DSA_PRIME_LENGTH },
			    { FIPS_DSA_TYPE, (unsigned char *)dsa_Q, FIPS_DSA_SUBPRIME_LENGTH },
			    { FIPS_DSA_TYPE, (unsigned char *)dsa_G, FIPS_DSA_BASE_LENGTH }};

    /*******************************************/
    /* Generate a DSA public/private key pair. */
    /*******************************************/

    /* Generate a DSA public/private key pair. */
    dsa_status = DSA_NewKeyFromSeed(&dsa_pqg, dsa_known_random_key_block,
                                    &dsa_private_key);

    if( dsa_status != SECSuccess )
        return( CKR_HOST_MEMORY );

    /* construct public key from private key. */
    dsa_public_key.params      = dsa_private_key->params;
    dsa_public_key.publicValue = dsa_private_key->publicValue;

    /*************************************************/
    /* DSA Single-Round Known Answer Signature Test. */
    /*************************************************/

    dsa_signature_item.data = dsa_computed_signature;
    dsa_signature_item.len  = sizeof dsa_computed_signature;

    dsa_digest_item.data    = (unsigned char *)dsa_known_digest;
    dsa_digest_item.len     = SHA1_LENGTH;

    /* Perform DSA signature process. */
    dsa_status = DSA_SignDigestWithSeed( dsa_private_key, 
                                         &dsa_signature_item,
                                         &dsa_digest_item,
                                         dsa_known_random_signature_block );

    if( ( dsa_status != SECSuccess ) ||
        ( dsa_signature_item.len != FIPS_DSA_SIGNATURE_LENGTH ) ||
        ( PORT_Memcmp( dsa_computed_signature, dsa_known_signature,
                       FIPS_DSA_SIGNATURE_LENGTH ) != 0 ) ) {
        dsa_status = SECFailure;
    } else {

    /****************************************************/
    /* DSA Single-Round Known Answer Verification Test. */
    /****************************************************/

    /* Perform DSA verification process. */
    dsa_status = DSA_VerifyDigest( &dsa_public_key, 
                                   &dsa_signature_item,
                                   &dsa_digest_item);
    }

    PORT_FreeArena(dsa_private_key->params.arena, PR_TRUE);
    /* Don't free public key, it uses same arena as private key */

    /* Verify DSA signature. */
    if( dsa_status != SECSuccess )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );


}

static CK_RV
sftk_fips_RNG_PowerUpSelfTest( void )
{
   static const PRUint8 Q[] = {
			0x85,0x89,0x9c,0x77,0xa3,0x79,0xff,0x1a,
			0x86,0x6f,0x2f,0x3e,0x2e,0xf9,0x8c,0x9c,
			0x9d,0xef,0xeb,0xed};
   static const PRUint8 GENX[] = {
			0x65,0x48,0xe3,0xca,0xac,0x64,0x2d,0xf7,
			0x7b,0xd3,0x4e,0x79,0xc9,0x7d,0xa6,0xa8,
			0xa2,0xc2,0x1f,0x8f,0xe9,0xb9,0xd3,0xa1,
			0x3f,0xf7,0x0c,0xcd,0xa6,0xca,0xbf,0xce,
			0x84,0x0e,0xb6,0xf1,0x0d,0xbe,0xa9,0xa3};
   static const PRUint8 rng_known_DSAX[] = {
			0x7a,0x86,0xf1,0x7f,0xbd,0x4e,0x6e,0xd9,
			0x0a,0x26,0x21,0xd0,0x19,0xcb,0x86,0x73,
			0x10,0x1f,0x60,0xd7};



   SECStatus rng_status = SECSuccess;
   PRUint8 DSAX[FIPS_DSA_SUBPRIME_LENGTH];

   /*******************************************/
   /*   Run the SP 800-90 Health tests        */
   /*******************************************/
   rng_status = PRNGTEST_RunHealthTests();
   if (rng_status != SECSuccess) {
	return (CKR_DEVICE_ERROR);
   }
  
   /*******************************************/
   /* Generate DSAX fow given Q.              */
   /*******************************************/

   rng_status = FIPS186Change_ReduceModQForDSA(GENX, Q, DSAX);

   /* Verify DSAX to perform the RNG integrity check */
   if( ( rng_status != SECSuccess ) ||
       ( PORT_Memcmp( DSAX, rng_known_DSAX,
                      (FIPS_DSA_SUBPRIME_LENGTH) ) != 0 ) )
       return( CKR_DEVICE_ERROR );
       
   return( CKR_OK ); 
}

static CK_RV
sftk_fipsSoftwareIntegrityTest(void)
{
    CK_RV crv = CKR_OK;

    /* make sure that our check file signatures are OK */
    if( !BLAPI_VerifySelf( NULL ) || 
	!BLAPI_SHVerify( SOFTOKEN_LIB_NAME, (PRFuncPtr) sftk_fips_HMAC ) ) {
	crv = CKR_DEVICE_ERROR; /* better error code? checksum error? */
    }
    return crv;
}

CK_RV
sftk_fipsPowerUpSelfTest( void )
{
    CK_RV rv;

    /* RC2 Power-Up SelfTest(s). */
    rv = sftk_fips_RC2_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* RC4 Power-Up SelfTest(s). */
    rv = sftk_fips_RC4_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* DES Power-Up SelfTest(s). */
    rv = sftk_fips_DES_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* DES3 Power-Up SelfTest(s). */
    rv = sftk_fips_DES3_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;
 
    /* AES Power-Up SelfTest(s) for 128-bit key. */
    rv = sftk_fips_AES_PowerUpSelfTest(FIPS_AES_128_KEY_SIZE);

    if( rv != CKR_OK )
        return rv;

    /* AES Power-Up SelfTest(s) for 192-bit key. */
    rv = sftk_fips_AES_PowerUpSelfTest(FIPS_AES_192_KEY_SIZE);

    if( rv != CKR_OK )
        return rv;

    /* AES Power-Up SelfTest(s) for 256-bit key. */
    rv = sftk_fips_AES_PowerUpSelfTest(FIPS_AES_256_KEY_SIZE);

    if( rv != CKR_OK )
        return rv;

    /* MD2 Power-Up SelfTest(s). */
    rv = sftk_fips_MD2_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* MD5 Power-Up SelfTest(s). */
    rv = sftk_fips_MD5_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* SHA-X Power-Up SelfTest(s). */
    rv = sftk_fips_SHA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* HMAC SHA-X Power-Up SelfTest(s). */
    rv = sftk_fips_HMAC_PowerUpSelfTest();
 
    if( rv != CKR_OK )
        return rv;

    /* RSA Power-Up SelfTest(s). */
    rv = sftk_fips_RSA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* DSA Power-Up SelfTest(s). */
    rv = sftk_fips_DSA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* RNG Power-Up SelfTest(s). */
    rv = sftk_fips_RNG_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;
    
#ifndef NSS_DISABLE_ECC
    /* ECDSA Power-Up SelfTest(s). */
    rv = sftk_fips_ECDSA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;
#endif

    /* Software/Firmware Integrity Test. */
    rv = sftk_fipsSoftwareIntegrityTest();

    if( rv != CKR_OK )
        return rv;

    /* Passed Power-Up SelfTest(s). */
    return( CKR_OK );
}

