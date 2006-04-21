/*
 * PKCS #11 FIPS Power-Up Self Test.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/* $Id: fipstest.c,v 1.14 2006/04/21 17:13:50 wtchang%redhat.com Exp $ */

#include "softoken.h"   /* Required for RC2-ECB, RC2-CBC, RC4, DES-ECB,  */
                        /*              DES-CBC, DES3-ECB, DES3-CBC, RSA */
                        /*              and DSA.                         */
#include "seccomon.h"   /* Required for RSA and DSA. */
#include "lowkeyi.h"     /* Required for RSA and DSA. */
#include "pkcs11.h"     /* Required for PKCS #11. */
#include "secerr.h"

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
#define FIPS_RSA_MESSAGE_LENGTH                 128 /* 1024-bits */
#define FIPS_RSA_COEFFICIENT_LENGTH              64 /*  512-bits */
#define FIPS_RSA_PRIME0_LENGTH                   64 /*  512-bits */
#define FIPS_RSA_PRIME1_LENGTH                   64 /*  512-bits */
#define FIPS_RSA_EXPONENT0_LENGTH                64 /*  512-bits */
#define FIPS_RSA_EXPONENT1_LENGTH                64 /*  512-bits */
#define FIPS_RSA_PRIVATE_EXPONENT_LENGTH        128 /* 1024-bits */
#define FIPS_RSA_ENCRYPT_LENGTH                 128 /* 1024-bits */
#define FIPS_RSA_DECRYPT_LENGTH                 128 /* 1024-bits */
#define FIPS_RSA_SIGNATURE_LENGTH               128 /* 1024-bits */
#define FIPS_RSA_MODULUS_LENGTH                 128 /* 1024-bits */


/* FIPS preprocessor directives for DSA.                        */
#define FIPS_DSA_TYPE                           siBuffer
#define FIPS_DSA_DIGEST_LENGTH                  20  /* 160-bits */
#define FIPS_DSA_SUBPRIME_LENGTH                20  /* 160-bits */
#define FIPS_DSA_SIGNATURE_LENGTH               40  /* 320-bits */
#define FIPS_DSA_PRIME_LENGTH                   64  /* 512-bits */
#define FIPS_DSA_BASE_LENGTH                    64  /* 512-bits */

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
    /* RSA Known Modulus used in both Public/Private Key Values (1024-bits). */
    static const PRUint8 rsa_modulus[FIPS_RSA_MODULUS_LENGTH] = {
                               0xd5, 0x84, 0x95, 0x07, 0xf4, 0xd0, 0x1f, 0x82,
                               0xf3, 0x79, 0xf4, 0x99, 0x48, 0x10, 0xe1, 0x71,
                               0xa5, 0x62, 0x22, 0xa3, 0x4b, 0x00, 0xe3, 0x5b,
                               0x3a, 0xcc, 0x10, 0x83, 0xe0, 0xaf, 0x61, 0x13,
                               0x54, 0x6a, 0xa2, 0x6a, 0x2c, 0x5e, 0xb3, 0xcc,
                               0xa3, 0x71, 0x9a, 0xb2, 0x3e, 0x78, 0xec, 0xb5,
                               0x0e, 0x6e, 0x31, 0x3b, 0x77, 0x1f, 0x6e, 0x94,
                               0x41, 0x60, 0xd5, 0x6e, 0xd9, 0xc6, 0xf9, 0x29,
                               0xc3, 0x40, 0x36, 0x25, 0xdb, 0xea, 0x0b, 0x07,
                               0xae, 0x76, 0xfd, 0x99, 0x29, 0xf4, 0x22, 0xc1,
                               0x1a, 0x8f, 0x05, 0xfe, 0x98, 0x09, 0x07, 0x05,
                               0xc2, 0x0f, 0x0b, 0x11, 0x83, 0x39, 0xca, 0xc7,
                               0x43, 0x63, 0xff, 0x33, 0x80, 0xe7, 0xc3, 0x78,
                               0xae, 0xf1, 0x73, 0x52, 0x98, 0x1d, 0xde, 0x5c,
                               0x53, 0x6e, 0x01, 0x73, 0x0d, 0x12, 0x7e, 0x77,
                               0x03, 0xf1, 0xef, 0x1b, 0xc8, 0xa8, 0x0f, 0x97};

    /* RSA Known Public Key Values (24-bits). */
    static const PRUint8 rsa_public_exponent[FIPS_RSA_PUBLIC_EXPONENT_LENGTH] 
                                                       = { 0x01, 0x00, 0x01 };
    /* RSA Known Private Key Values (version                 is    8-bits), */
    /*                              (private exponent        is 1024-bits), */
    /*                              (private prime0          is  512-bits), */
    /*                              (private prime1          is  512-bits), */
    /*                              (private prime exponent0 is  512-bits), */
    /*                              (private prime exponent1 is  512-bits), */
    /*                          and (private coefficient     is  512-bits). */
    static const PRUint8 rsa_version[] = { 0x00 };

    static const PRUint8 rsa_private_exponent[FIPS_RSA_PRIVATE_EXPONENT_LENGTH]
                           = { 0x85, 0x27, 0x47, 0x61, 0x4c, 0xd4, 0xb5, 0xb2,
                               0x0e, 0x70, 0x91, 0x8f, 0x3d, 0x97, 0xf9, 0x5f,
                               0xcc, 0x09, 0x65, 0x1c, 0x7c, 0x5b, 0xb3, 0x6d,
                               0x63, 0x3f, 0x7b, 0x55, 0x22, 0xbb, 0x7c, 0x48,
                               0x77, 0xae, 0x80, 0x56, 0xc2, 0x10, 0xd5, 0x03,
                               0xdb, 0x31, 0xaf, 0x8d, 0x54, 0xd4, 0x48, 0x99,
                               0xa8, 0xc4, 0x23, 0x43, 0xb8, 0x48, 0x0b, 0xc7,
                               0xbc, 0xf5, 0xcc, 0x64, 0x72, 0xbf, 0x59, 0x06,
                               0x04, 0x1c, 0x32, 0xf5, 0x14, 0x2e, 0x6e, 0xe2,
                               0x0f, 0x5c, 0xde, 0x36, 0x3c, 0x6e, 0x7c, 0x4d,
                               0xcc, 0xd3, 0x00, 0x6e, 0xe5, 0x45, 0x46, 0xef,
                               0x4d, 0x25, 0x46, 0x6d, 0x7f, 0xed, 0xbb, 0x4f,
                               0x4d, 0x9f, 0xda, 0x87, 0x47, 0x8f, 0x74, 0x44,
                               0xb7, 0xbe, 0x9d, 0xf5, 0xdd, 0xd2, 0x4c, 0xa5,
                               0xab, 0x74, 0xe5, 0x29, 0xa1, 0xd2, 0x45, 0x3b,
                               0x33, 0xde, 0xd5, 0xae, 0xf7, 0x03, 0x10, 0x21};

    static const PRUint8 rsa_prime0[FIPS_RSA_PRIME0_LENGTH]   = {
                               0xf9, 0x74, 0x8f, 0x16, 0x02, 0x6b, 0xa0, 0xee,
                               0x7f, 0x28, 0x97, 0x91, 0xdc, 0xec, 0xc0, 0x7c,
                               0x49, 0xc2, 0x85, 0x76, 0xee, 0x66, 0x74, 0x2d,
                               0x1a, 0xb8, 0xf7, 0x2f, 0x11, 0x5b, 0x36, 0xd8,
                               0x46, 0x33, 0x3b, 0xd8, 0xf3, 0x2d, 0xa1, 0x03,
                               0x83, 0x2b, 0xec, 0x35, 0x43, 0x32, 0xff, 0xdd,
                               0x81, 0x7c, 0xfd, 0x65, 0x13, 0x04, 0x7c, 0xfc,
                               0x03, 0x97, 0xf0, 0xd5, 0x62, 0xdc, 0x0d, 0xbf};
    static const PRUint8 rsa_prime1[FIPS_RSA_PRIME1_LENGTH]   = {
                               0xdb, 0x1e, 0xa7, 0x3d, 0xe7, 0xfa, 0x8b, 0x04,
                               0x83, 0x48, 0xf3, 0xa5, 0x31, 0x9d, 0x35, 0x5e,
                               0x4d, 0x54, 0x77, 0xcc, 0x84, 0x09, 0xf3, 0x11,
                               0x0d, 0x54, 0xed, 0x85, 0x39, 0xa9, 0xca, 0xa8,
                               0xea, 0xae, 0x19, 0x9c, 0x75, 0xdb, 0x88, 0xb8,
                               0x04, 0x8d, 0x54, 0xc6, 0xa4, 0x80, 0xf8, 0x93,
                               0xf0, 0xdb, 0x19, 0xef, 0xd7, 0x87, 0x8a, 0x8f,
                               0x5a, 0x09, 0x2e, 0x54, 0xf3, 0x45, 0x24, 0x29};
    static const PRUint8 rsa_exponent0[FIPS_RSA_EXPONENT0_LENGTH] = {
                               0x6a, 0xd1, 0x25, 0x80, 0x18, 0x33, 0x3c, 0x2b,
                               0x44, 0x19, 0xfe, 0xa5, 0x40, 0x03, 0xc4, 0xfc,
                               0xb3, 0x9c, 0xef, 0x07, 0x99, 0x58, 0x17, 0xc1,
                               0x44, 0xa3, 0x15, 0x7d, 0x7b, 0x22, 0x22, 0xdf,
                               0x03, 0x58, 0x66, 0xf5, 0x24, 0x54, 0x52, 0x91,
                               0x2d, 0x76, 0xfe, 0x63, 0x64, 0x4e, 0x0f, 0x50,
                               0x2b, 0x65, 0x79, 0x1f, 0xf1, 0xbf, 0xc7, 0x41,
                               0x26, 0xcc, 0xc6, 0x1c, 0xa9, 0x83, 0x6f, 0x03};
    static const PRUint8 rsa_exponent1[FIPS_RSA_EXPONENT1_LENGTH] = {
                               0x12, 0x84, 0x1a, 0x99, 0xce, 0x9a, 0x8b, 0x58,
                               0xcc, 0x47, 0x43, 0xdf, 0x77, 0xbb, 0xd3, 0x20,
                               0xae, 0xe4, 0x2e, 0x63, 0x67, 0xdc, 0xf7, 0x5f,
                               0x3f, 0x83, 0x27, 0xb7, 0x14, 0x52, 0x56, 0xbf,
                               0xc3, 0x65, 0x06, 0xe1, 0x03, 0xcc, 0x93, 0x57,
                               0x09, 0x7b, 0x6f, 0xe8, 0x81, 0x4a, 0x2c, 0xb7,
                               0x43, 0xa9, 0x20, 0x1d, 0xf6, 0x56, 0x8b, 0xcc,
                               0xe5, 0x4c, 0xd5, 0x4f, 0x74, 0x67, 0x29, 0x51};
    static const PRUint8 rsa_coefficient[FIPS_RSA_COEFFICIENT_LENGTH] = {
                               0x23, 0xab, 0xf4, 0x03, 0x2f, 0x29, 0x95, 0x74,
                               0xac, 0x1a, 0x33, 0x96, 0x62, 0xed, 0xf7, 0xf6,
                               0xae, 0x07, 0x2a, 0x2e, 0xe8, 0xab, 0xfb, 0x1e,
                               0xb9, 0xb2, 0x88, 0x1e, 0x85, 0x05, 0x42, 0x64,
                               0x03, 0xb2, 0x8b, 0xc1, 0x81, 0x75, 0xd7, 0xba,
                               0xaa, 0xd4, 0x31, 0x3c, 0x8a, 0x96, 0x23, 0x9d,
                               0x3f, 0x06, 0x3e, 0x44, 0xa9, 0x62, 0x2f, 0x61,
                               0x5a, 0x51, 0x82, 0x2c, 0x04, 0x85, 0x73, 0xd1};

    /* RSA Known Plaintext Message (1024-bits). */
    static const PRUint8 rsa_known_plaintext_msg[FIPS_RSA_MESSAGE_LENGTH] = {
                                         "Known plaintext message utilized" 
                                         "for RSA Encryption &  Decryption"
                                         "block, SHA1, SHA256, SHA384  and"
                                         "SHA512 RSA Signature KAT tests."};

    /* RSA Known Ciphertext (1024-bits). */
    static const PRUint8 rsa_known_ciphertext[] = {
                               0x1e, 0x7e, 0x12, 0xbb, 0x15, 0x62, 0xd0, 0x23,
                               0x53, 0x4c, 0x51, 0x97, 0x77, 0x06, 0xa0, 0xbb,
                               0x26, 0x99, 0x9a, 0x8f, 0x39, 0xad, 0x88, 0x5c,
                               0xc4, 0xce, 0x33, 0x40, 0x94, 0x92, 0xb4, 0x0e,
                               0xab, 0x71, 0xa9, 0x5d, 0x9a, 0x37, 0xe3, 0x9a,
                               0x24, 0x95, 0x13, 0xea, 0x0f, 0xbb, 0xf7, 0xff,
                               0xdf, 0x31, 0x33, 0x23, 0x1d, 0xce, 0x26, 0x9e,
                               0xd1, 0xde, 0x98, 0x40, 0xde, 0x57, 0x86, 0x12,
                               0xf1, 0xe6, 0x5a, 0x3f, 0x08, 0x02, 0x81, 0x85,
                               0xe0, 0xd9, 0xad, 0x3c, 0x8c, 0x71, 0xf8, 0xcf,
                               0x0a, 0x98, 0xc5, 0x08, 0xdc, 0xc4, 0xca, 0x8c,
                               0x23, 0x1b, 0x4d, 0x9b, 0xb5, 0x13, 0x44, 0xe1,
                               0x5f, 0xf9, 0x30, 0x80, 0x25, 0xe0, 0x1e, 0x94,
                               0xa3, 0x0c, 0xdc, 0x82, 0x2e, 0xfb, 0x30, 0xbe,
                               0x89, 0xba, 0x76, 0xb6, 0x23, 0xf7, 0xda, 0x7c,
                               0xca, 0xe6, 0x02, 0xbd, 0x92, 0xce, 0x64, 0xfc};

    /* RSA Known Signed Hash (1024-bits). */
    static const PRUint8 rsa_known_sha1_signature[] = {
                               0xd2, 0xa4, 0xe0, 0x2b, 0xc7, 0x03, 0x7f, 0xc6,
                               0x06, 0x9e, 0xa2, 0x82, 0x19, 0xe9, 0x2b, 0xaf,
                               0xe3, 0x48, 0x88, 0xc1, 0xf3, 0xb5, 0x0d, 0xe4,
                               0x52, 0x9e, 0xad, 0xd5, 0x58, 0xb5, 0x9f, 0xe8,
                               0x40, 0xe9, 0xb7, 0x2e, 0xc6, 0x71, 0x58, 0x56,
                               0x04, 0xac, 0xb0, 0xf3, 0x3a, 0x42, 0x38, 0x08,
                               0xc4, 0x43, 0x39, 0xba, 0x19, 0xce, 0xb1, 0x99,
                               0xf1, 0x8d, 0x89, 0xd8, 0x50, 0x07, 0x14, 0x3d,
                               0xcf, 0xd0, 0xb6, 0x79, 0xde, 0x9c, 0x89, 0x32,
                               0xb0, 0x73, 0x3f, 0xed, 0x03, 0x0b, 0xdf, 0x6d,
                               0x7e, 0xc9, 0x1c, 0x39, 0xe8, 0x2b, 0x16, 0x09,
                               0xbb, 0x5f, 0x99, 0x2f, 0xeb, 0xf3, 0x37, 0x73,
                               0x0d, 0x0e, 0xcc, 0x95, 0xad, 0x90, 0x80, 0x03,
                               0x1d, 0x80, 0x55, 0x37, 0xa1, 0x2a, 0x71, 0x76,
                               0x23, 0x87, 0x8c, 0x9b, 0x41, 0x07, 0xc6, 0x3d,
                               0xc6, 0xa3, 0x7d, 0x1b, 0xff, 0x4e, 0x11, 0x19};

    /* RSA Known Signed Hash (1024-bits). */
    static const PRUint8 rsa_known_sha256_signature[] = {
                               0x27, 0x35, 0xdd, 0xc4, 0xf8, 0xe2, 0x0b, 0xa3,
                               0xef, 0x63, 0x57, 0x3b, 0xe1, 0x58, 0x9a, 0xbc,
                               0x20, 0x9c, 0x25, 0x12, 0x01, 0xbf, 0xbb, 0x29,
                               0x80, 0x1a, 0xb1, 0x37, 0x9c, 0xcd, 0x67, 0xc7,
                               0x0d, 0xf8, 0x64, 0x10, 0x9f, 0xe2, 0xa1, 0x9b,
                               0x21, 0x90, 0xcc, 0xda, 0x8b, 0x76, 0x5e, 0x79,
                               0x00, 0x9d, 0x58, 0x8b, 0x8a, 0xb3, 0xc3, 0xb5,
                               0xf1, 0x54, 0xc5, 0x8c, 0x72, 0xba, 0xde, 0x51,
                               0x3c, 0x6b, 0x94, 0xd6, 0xf3, 0x1b, 0xa2, 0x53,
                               0xe6, 0x1a, 0x46, 0x1d, 0x7f, 0x14, 0x86, 0xcc,
                               0xa6, 0x30, 0x92, 0x96, 0xc0, 0x96, 0x24, 0xf0,
                               0x42, 0x53, 0x4c, 0xdd, 0x27, 0xdf, 0x1d, 0x2e,
                               0x8b, 0x83, 0xbe, 0xed, 0x85, 0x1d, 0x50, 0x46,
                               0xa3, 0x7d, 0x20, 0xea, 0x3e, 0x91, 0xfb, 0xf6,
                               0x86, 0x51, 0xfd, 0x8c, 0xe5, 0x31, 0xe6, 0x7e,
                               0x60, 0x08, 0x0e, 0xec, 0xa6, 0xea, 0x24, 0x8d};

    /* RSA Known Signed Hash (1024-bits). */
    static const PRUint8 rsa_known_sha384_signature[] = {
                               0x0b, 0x03, 0x94, 0x4f, 0x94, 0x78, 0x9b, 0x96,
                               0x76, 0xeb, 0x72, 0x58, 0xe1, 0xc5, 0xc7, 0x5f,
                               0x85, 0x01, 0xa8, 0xc4, 0xf6, 0x1a, 0xb5, 0x2c,
                               0xd1, 0xd8, 0x87, 0xde, 0x3a, 0x9c, 0x9f, 0x57,
                               0x81, 0x2a, 0x1e, 0x23, 0x07, 0x70, 0xb0, 0xf9,
                               0x28, 0x3d, 0xfa, 0xe5, 0x2e, 0x1b, 0x9a, 0x72,
                               0xc3, 0x74, 0xb3, 0x42, 0x1c, 0x9a, 0x13, 0xdc,
                               0xc9, 0xd6, 0xd5, 0x88, 0xc9, 0x9c, 0x46, 0xf1,
                               0x0c, 0xa6, 0xf7, 0xd8, 0x06, 0xa3, 0x1b, 0xdf,
                               0x55, 0xb3, 0x1b, 0x7b, 0x58, 0x1d, 0xff, 0x19,
                               0xc7, 0xe0, 0xdd, 0x59, 0xac, 0x2f, 0x78, 0x71,
                               0xe7, 0xe0, 0x17, 0xa3, 0x1c, 0x5c, 0x92, 0xef,
                               0xb6, 0x75, 0xed, 0xbe, 0x18, 0x39, 0x6b, 0xd7,
                               0xc9, 0x08, 0x62, 0x55, 0x62, 0xac, 0x5d, 0xa1,
                               0x9b, 0xd5, 0xb8, 0x98, 0x15, 0xc0, 0xf5, 0x41,
                               0x85, 0x44, 0x96, 0xca, 0x10, 0xdc, 0x57, 0x21};

    /* RSA Known Signed Hash (1024-bits). */
    static const PRUint8 rsa_known_sha512_signature[] = {
                               0xa5, 0xd0, 0x80, 0x04, 0x22, 0xfc, 0x80, 0x73,
                               0x7d, 0x46, 0xc8, 0x7b, 0xac, 0x44, 0x7b, 0xe6,
                               0x07, 0xe5, 0x61, 0x4c, 0x33, 0x7f, 0x6f, 0x46,
                               0x7c, 0x30, 0xe3, 0x75, 0x59, 0x4b, 0x42, 0xf3,
                               0x9f, 0x35, 0x3c, 0x10, 0x56, 0xdb, 0xd2, 0x69,
                               0x43, 0xcb, 0x77, 0xe9, 0x7d, 0xcd, 0x07, 0x43,
                               0xc5, 0xd4, 0x0c, 0x9d, 0xf5, 0x92, 0xbd, 0x0e,
                               0x3b, 0xb7, 0x68, 0x88, 0x84, 0xca, 0xae, 0x0d,
                               0xab, 0x71, 0x10, 0xad, 0xab, 0x27, 0xe4, 0xa3,
                               0x24, 0x41, 0xeb, 0x1c, 0xa6, 0x5f, 0xf1, 0x85,
                               0xd0, 0xf6, 0x22, 0x74, 0x3d, 0x81, 0xbe, 0xdd,
                               0x1b, 0x2a, 0x4c, 0xd1, 0x6c, 0xb5, 0x6d, 0x7a,
                               0xbb, 0x99, 0x69, 0x01, 0xa6, 0xc0, 0x98, 0xfa,
                               0x97, 0xa3, 0xd1, 0xb0, 0xdf, 0x09, 0xe3, 0x3d,
                               0x88, 0xee, 0x90, 0xf3, 0x10, 0x41, 0x0f, 0x06,
                               0x31, 0xe9, 0x60, 0x2d, 0xbf, 0x63, 0x7b, 0xf8};

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

    rsa_status = sftk_fips_RSA_PowerUpSigSelfTest (HASH_AlgSHA1,
                           rsa_public_key, rsa_private_key,
                           rsa_known_plaintext_msg, FIPS_RSA_MESSAGE_LENGTH, 
                           rsa_known_sha1_signature);
    if( rsa_status != SECSuccess )
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


static CK_RV
sftk_fips_DSA_PowerUpSelfTest( void )
{
    /* DSA Known P (512-bits), Q (160-bits), and G (512-bits) Values. */
    static const PRUint8 dsa_P[] = {
                           0x8d,0xf2,0xa4,0x94,0x49,0x22,0x76,0xaa,
                           0x3d,0x25,0x75,0x9b,0xb0,0x68,0x69,0xcb,
                           0xea,0xc0,0xd8,0x3a,0xfb,0x8d,0x0c,0xf7,
                           0xcb,0xb8,0x32,0x4f,0x0d,0x78,0x82,0xe5,
                           0xd0,0x76,0x2f,0xc5,0xb7,0x21,0x0e,0xaf,
                           0xc2,0xe9,0xad,0xac,0x32,0xab,0x7a,0xac,
                           0x49,0x69,0x3d,0xfb,0xf8,0x37,0x24,0xc2,
                           0xec,0x07,0x36,0xee,0x31,0xc8,0x02,0x91};
    static const PRUint8 dsa_Q[] = {
                           0xc7,0x73,0x21,0x8c,0x73,0x7e,0xc8,0xee,
                           0x99,0x3b,0x4f,0x2d,0xed,0x30,0xf4,0x8e,
                           0xda,0xce,0x91,0x5f};
    static const PRUint8 dsa_G[] = {
                           0x62,0x6d,0x02,0x78,0x39,0xea,0x0a,0x13,
                           0x41,0x31,0x63,0xa5,0x5b,0x4c,0xb5,0x00,
                           0x29,0x9d,0x55,0x22,0x95,0x6c,0xef,0xcb,
                           0x3b,0xff,0x10,0xf3,0x99,0xce,0x2c,0x2e,
                           0x71,0xcb,0x9d,0xe5,0xfa,0x24,0xba,0xbf,
                           0x58,0xe5,0xb7,0x95,0x21,0x92,0x5c,0x9c,
                           0xc4,0x2e,0x9f,0x6f,0x46,0x4b,0x08,0x8c,
                           0xc5,0x72,0xaf,0x53,0xe6,0xd7,0x88,0x02};

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
			     0x39,0x0d,0x84,0xb1,0xf7,0x52,0x89,0xba,
			     0xec,0x1e,0xa8,0xe2,0x00,0x8e,0x37,0x8f,
			     0xc2,0xf5,0xf8,0x70,0x11,0xa8,0xc7,0x02,
			     0x0e,0x75,0xcf,0x6b,0x54,0x4a,0x52,0xe8,
			     0xd8,0x6d,0x4a,0xe8,0xee,0x56,0x8e,0x59};

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

    /* Passed Power-Up SelfTest(s). */
    return( CKR_OK );
}

