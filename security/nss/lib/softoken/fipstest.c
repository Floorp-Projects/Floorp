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


/* FIPS preprocessor directives for MD2.                        */
#define FIPS_MD2_HASH_MESSAGE_LENGTH            64  /* 512-bits */


/* FIPS preprocessor directives for MD5.                        */
#define FIPS_MD5_HASH_MESSAGE_LENGTH            64  /* 512-bits */


/* FIPS preprocessor directives for SHA-1.                      */
#define FIPS_SHA1_HASH_MESSAGE_LENGTH           64  /* 512-bits */


/* FIPS preprocessor directives for RSA.                        */
#define FIPS_RSA_TYPE                           siBuffer
#define FIPS_RSA_PUBLIC_EXPONENT_LENGTH          1  /*   8-bits */
#define FIPS_RSA_PRIVATE_VERSION_LENGTH          1  /*   8-bits */
#define FIPS_RSA_MESSAGE_LENGTH                 16  /* 128-bits */
#define FIPS_RSA_COEFFICIENT_LENGTH             32  /* 256-bits */
#define FIPS_RSA_PRIME0_LENGTH                  33  /* 264-bits */
#define FIPS_RSA_PRIME1_LENGTH                  33  /* 264-bits */
#define FIPS_RSA_EXPONENT0_LENGTH               33  /* 264-bits */
#define FIPS_RSA_EXPONENT1_LENGTH               33  /* 264-bits */
#define FIPS_RSA_PRIVATE_EXPONENT_LENGTH        64  /* 512-bits */
#define FIPS_RSA_ENCRYPT_LENGTH                 64  /* 512-bits */
#define FIPS_RSA_DECRYPT_LENGTH                 64  /* 512-bits */
#define FIPS_RSA_CRYPTO_LENGTH                  64  /* 512-bits */
#define FIPS_RSA_SIGNATURE_LENGTH               64  /* 512-bits */
#define FIPS_RSA_MODULUS_LENGTH                 65  /* 520-bits */


/* FIPS preprocessor directives for DSA.                        */
#define FIPS_DSA_TYPE                           siBuffer
#define FIPS_DSA_DIGEST_LENGTH                  20  /* 160-bits */
#define FIPS_DSA_SUBPRIME_LENGTH                20  /* 160-bits */
#define FIPS_DSA_SIGNATURE_LENGTH               40  /* 320-bits */
#define FIPS_DSA_PRIME_LENGTH                   64  /* 512-bits */
#define FIPS_DSA_BASE_LENGTH                    64  /* 512-bits */

static CK_RV
pk11_fips_RC2_PowerUpSelfTest( void )
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
pk11_fips_RC4_PowerUpSelfTest( void )
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
pk11_fips_DES_PowerUpSelfTest( void )
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
pk11_fips_DES3_PowerUpSelfTest( void )
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


static CK_RV
pk11_fips_MD2_PowerUpSelfTest( void )
{
    /* MD2 Known Hash Message (512-bits). */
    static const PRUint8 md2_known_hash_message[] = {
	"The test message for the MD2, MD5, and SHA-1 hashing algorithms." };

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

    MD2_Update( md2_context, md2_known_hash_message,
                FIPS_MD2_HASH_MESSAGE_LENGTH );

    MD2_End( md2_context, md2_computed_digest, &md2_bytes_hashed, MD2_LENGTH );

    MD2_DestroyContext( md2_context , PR_TRUE );

    if( ( md2_bytes_hashed != MD2_LENGTH ) ||
        ( PORT_Memcmp( md2_computed_digest, md2_known_digest,
                       MD2_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
pk11_fips_MD5_PowerUpSelfTest( void )
{
    /* MD5 Known Hash Message (512-bits). */
    static const PRUint8 md5_known_hash_message[] = {
	"The test message for the MD2, MD5, and SHA-1 hashing algorithms." };

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

    md5_status = MD5_HashBuf( md5_computed_digest, md5_known_hash_message,
                              FIPS_MD5_HASH_MESSAGE_LENGTH );

    if( ( md5_status != SECSuccess ) ||
        ( PORT_Memcmp( md5_computed_digest, md5_known_digest,
                       MD5_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
pk11_fips_SHA1_PowerUpSelfTest( void )
{
    /* SHA-1 Known Hash Message (512-bits). */
    static const PRUint8 sha1_known_hash_message[] = {
	 "The test message for the MD2, MD5, and SHA-1 hashing algorithms." };

    /* SHA-1 Known Digest Message (160-bits). */
    static const PRUint8 sha1_known_digest[] = {
			       0x0a,0x6d,0x07,0xba,0x1e,0xbd,0x8a,0x1b,
			       0x72,0xf6,0xc7,0x22,0xf1,0x27,0x9f,0xf0,
			       0xe0,0x68,0x47,0x7a};

    /* SHA-1 variables. */
    PRUint8        sha1_computed_digest[SHA1_LENGTH];
    SECStatus      sha1_status;


    /*************************************************/
    /* SHA-1 Single-Round Known Answer Hashing Test. */
    /*************************************************/

    sha1_status = SHA1_HashBuf( sha1_computed_digest, sha1_known_hash_message,
                                FIPS_SHA1_HASH_MESSAGE_LENGTH );

    if( ( sha1_status != SECSuccess ) ||
        ( PORT_Memcmp( sha1_computed_digest, sha1_known_digest,
                       SHA1_LENGTH ) != 0 ) )
        return( CKR_DEVICE_ERROR );

    return( CKR_OK );
}


static CK_RV
pk11_fips_RSA_PowerUpSelfTest( void )
{
    /* RSA Known Modulus used in both Public/Private Key Values (520-bits). */
    static const PRUint8 rsa_modulus[FIPS_RSA_MODULUS_LENGTH] = {
                                 0x00,0xa1,0xe9,0x5e,0x66,0x88,0xe2,0xf2,
                                 0x2b,0xe7,0x70,0x36,0x33,0xbc,0xeb,0x55,
                                 0x55,0xf1,0x60,0x18,0x3c,0xfb,0xd2,0x79,
                                 0xf6,0xc4,0xb8,0x09,0xe3,0x12,0xf6,0x63,
                                 0x6d,0xc7,0x8e,0x19,0xc0,0x0e,0x10,0x78,
                                 0xc1,0xfe,0x2a,0x41,0x74,0x2d,0xf7,0xc4,
                                 0x69,0xa7,0x3c,0xbc,0x8a,0xc8,0x31,0x2b,
                                 0x4f,0x60,0xf0,0xf1,0xec,0x5a,0x29,0xec,
                                 0x6b};

    /* RSA Known Public Key Values (8-bits). */
    static const PRUint8 rsa_public_exponent[] = { 0x03 };

    /* RSA Known Private Key Values (version                 is   8-bits), */
    /*                              (private exponent        is 512-bits), */
    /*                              (private prime0          is 264-bits), */
    /*                              (private prime1          is 264-bits), */
    /*                              (private prime exponent0 is 264-bits), */
    /*                              (private prime exponent1 is 264-bits), */
    /*                          and (private coefficient     is 256-bits). */
    static const PRUint8 rsa_version[] = { 0x00 };
    static const PRUint8 rsa_private_exponent[FIPS_RSA_PRIVATE_EXPONENT_LENGTH] = {
				  0x6b,0xf0,0xe9,0x99,0xb0,0x97,0x4c,0x1d,
				  0x44,0xf5,0x79,0x77,0xd3,0x47,0x8e,0x39,
				  0x4b,0x95,0x65,0x7d,0xfd,0x36,0xfb,0xf9,
				  0xd8,0x7a,0xb1,0x42,0x0c,0xa4,0x42,0x48,
				  0x20,0x1c,0x6b,0x7d,0x5d,0xa3,0x58,0xd6,
				  0x95,0xd6,0x41,0xe3,0xd6,0x73,0xad,0xdb,
				  0x3b,0x89,0x00,0x8a,0xcd,0x1d,0xb9,0x06,
				  0xac,0xac,0x0e,0x02,0x72,0x1c,0xf8,0xab };
    static const PRUint8 rsa_prime0[FIPS_RSA_PRIME0_LENGTH]   = {
				      0x00,0xd2,0x2c,0x9d,0xef,0x7c,0x8f,0x58,
				      0x93,0x19,0xa1,0x77,0x0e,0x38,0x3e,0x85,
				      0xb4,0xaf,0xcc,0x99,0xa5,0x43,0xbf,0x97,
				      0xdc,0x46,0xb8,0x3f,0x6e,0x85,0x18,0x00,
				      0x81};
    static const PRUint8 rsa_prime1[FIPS_RSA_PRIME1_LENGTH]   = {
				      0x00,0xc5,0x36,0xda,0x94,0x85,0x0c,0x1a,
				      0xed,0x03,0xc7,0x67,0x90,0x34,0x0b,0xb9,
				      0xec,0x1e,0x22,0xa2,0x15,0x50,0xc4,0xfd,
				      0xe9,0x17,0x36,0x9d,0x7a,0x29,0xe6,0x76,
				      0xeb};
    static const PRUint8 rsa_exponent0[FIPS_RSA_EXPONENT0_LENGTH] = {
					 0x00,0x8c,0x1d,0xbe,0x9f,0xa8,
					 0x5f,0x90,0x62,0x11,0x16,0x4f,
					 0x5e,0xd0,0x29,0xae,0x78,0x75,
					 0x33,0x11,0x18,0xd7,0xd5,0x0f,
					 0xe8,0x2f,0x25,0x7f,0x9f,0x03,
					 0x65,0x55,0xab};
    static const PRUint8 rsa_exponent1[FIPS_RSA_EXPONENT1_LENGTH] = {
					 0x00,0x83,0x79,0xe7,0x0d,0xae,
					 0x08,0x11,0xf3,0x57,0xda,0x45,
					 0x0a,0xcd,0x5d,0x26,0x9d,0x69,
					 0x6c,0x6c,0x0e,0x35,0xd8,0xa9,
					 0x46,0x0f,0x79,0xbe,0x51,0x71,
					 0x44,0x4f,0x47};
    static const PRUint8 rsa_coefficient[FIPS_RSA_COEFFICIENT_LENGTH] = {
					 0x54,0x8d,0xb8,0xdc,0x8b,0xde,0xbb,
					 0x08,0xc9,0x67,0xb7,0xa9,0x5f,0xa5,
					 0xc4,0x5e,0x67,0xaa,0xfe,0x1a,0x08,
					 0xeb,0x48,0x43,0xcb,0xb0,0xb9,0x38,
					 0x3a,0x31,0x39,0xde};


    /* RSA Known Plaintext (512-bits). */
    static const PRUint8 rsa_known_plaintext[] = {
                                         "Known plaintext utilized for RSA"
                                         " Encryption and Decryption test." };

    /* RSA Known Ciphertext (512-bits). */
    static const PRUint8 rsa_known_ciphertext[] = {
				      0x12,0x80,0x3a,0x53,0xee,0x93,0x81,0xa5,
				      0xf7,0x40,0xc5,0xb1,0xef,0xd9,0x27,0xaf,
				      0xef,0x4b,0x87,0x44,0x00,0xd0,0xda,0xcf,
				      0x10,0x57,0x4c,0xd5,0xc3,0xed,0x84,0xdc,
				      0x74,0x03,0x19,0x69,0x2c,0xd6,0x54,0x3e,
				      0xd2,0xe3,0x90,0xb6,0x67,0x91,0x2f,0x1f,
				      0x54,0x13,0x99,0x00,0x0b,0xfd,0x52,0x7f,
				      0xd8,0xc6,0xdb,0x8a,0xfe,0x06,0xf3,0xb1};

    /* RSA Known Message (128-bits). */
    static const PRUint8 rsa_known_message[]  = { "Netscape Forever" };

    /* RSA Known Signed Hash (512-bits). */
    static const PRUint8 rsa_known_signature[] = {
				     0x27,0x23,0xa6,0x71,0x57,0xc8,0x70,0x5f,
				     0x70,0x0e,0x06,0x7b,0x96,0x6a,0xaa,0x41,
				     0x6e,0xab,0x67,0x4b,0x5f,0x76,0xc4,0x53,
				     0x23,0xd7,0x57,0x7a,0x3a,0xbc,0x4c,0x27,
				     0x65,0xca,0xde,0x9f,0xd3,0x1d,0xa4,0x5a,
				     0xf9,0x8f,0xb2,0x05,0xa3,0x86,0xf9,0x66,
				     0x55,0x4c,0x68,0x50,0x66,0xa4,0xe9,0x17,
				     0x45,0x11,0xb8,0x1a,0xfc,0xbc,0x79,0x3b};


    static const RSAPublicKey    bl_public_key = { NULL, 
      { FIPS_RSA_TYPE, (unsigned char *)rsa_modulus,         FIPS_RSA_MODULUS_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_public_exponent, FIPS_RSA_PUBLIC_EXPONENT_LENGTH }
    };
    static const RSAPrivateKey   bl_private_key = { NULL,
      { FIPS_RSA_TYPE, (unsigned char *)rsa_version,          FIPS_RSA_PRIVATE_VERSION_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_modulus,          FIPS_RSA_MODULUS_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_public_exponent,  FIPS_RSA_PUBLIC_EXPONENT_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_private_exponent, FIPS_RSA_PRIVATE_EXPONENT_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_prime0,           FIPS_RSA_PRIME0_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_prime1,           FIPS_RSA_PRIME1_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_exponent0,        FIPS_RSA_EXPONENT0_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_exponent1,        FIPS_RSA_EXPONENT1_LENGTH },
      { FIPS_RSA_TYPE, (unsigned char *)rsa_coefficient,      FIPS_RSA_COEFFICIENT_LENGTH }
    };

    /* RSA variables. */
#ifdef CREATE_TEMP_ARENAS
    PLArenaPool *         rsa_public_arena;
    PLArenaPool *         rsa_private_arena;
#endif
    NSSLOWKEYPublicKey *  rsa_public_key;
    NSSLOWKEYPrivateKey * rsa_private_key;
    unsigned int          rsa_bytes_signed;
    SECStatus             rsa_status;

    NSSLOWKEYPublicKey    low_public_key   = { NULL, NSSLOWKEYRSAKey, };
    NSSLOWKEYPrivateKey   low_private_key  = { NULL, NSSLOWKEYRSAKey, };
    PRUint8               rsa_computed_ciphertext[FIPS_RSA_ENCRYPT_LENGTH];
    PRUint8               rsa_computed_plaintext[FIPS_RSA_DECRYPT_LENGTH];
    PRUint8               rsa_computed_signature[FIPS_RSA_SIGNATURE_LENGTH];

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
                                 rsa_computed_ciphertext, rsa_known_plaintext);

    if( ( rsa_status != SECSuccess ) ||
        ( PORT_Memcmp( rsa_computed_ciphertext, rsa_known_ciphertext,
                       FIPS_RSA_ENCRYPT_LENGTH ) != 0 ) )
        goto rsa_loser;

    /**************************************************/
    /* RSA Single-Round Known Answer Decryption Test. */
    /**************************************************/

    /* Perform RSA Private Key Decryption. */
    rsa_status = RSA_PrivateKeyOp(&rsa_private_key->u.rsa, 
                                  rsa_computed_plaintext, rsa_known_ciphertext);

    if( ( rsa_status != SECSuccess ) ||
        ( PORT_Memcmp( rsa_computed_plaintext, rsa_known_plaintext,
                       FIPS_RSA_DECRYPT_LENGTH ) != 0 ) )
        goto rsa_loser;


    /*************************************************/
    /* RSA Single-Round Known Answer Signature Test. */
    /*************************************************/

    /* Perform RSA signature with the RSA private key. */
    rsa_status = RSA_Sign( rsa_private_key, rsa_computed_signature,
                           &rsa_bytes_signed,
                           FIPS_RSA_SIGNATURE_LENGTH, (unsigned char *)rsa_known_message,
                           FIPS_RSA_MESSAGE_LENGTH );

    if( ( rsa_status != SECSuccess ) ||
        ( rsa_bytes_signed != FIPS_RSA_SIGNATURE_LENGTH ) ||
        ( PORT_Memcmp( rsa_computed_signature, rsa_known_signature,
                       FIPS_RSA_SIGNATURE_LENGTH ) != 0 ) )
        goto rsa_loser;


    /****************************************************/
    /* RSA Single-Round Known Answer Verification Test. */
    /****************************************************/

    /* Perform RSA verification with the RSA public key. */
    rsa_status = RSA_CheckSign( rsa_public_key,
                                rsa_computed_signature,
                                FIPS_RSA_SIGNATURE_LENGTH,
                                (unsigned char *)rsa_known_message,
                                FIPS_RSA_MESSAGE_LENGTH );

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
pk11_fips_DSA_PowerUpSelfTest( void )
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
pk11_fipsPowerUpSelfTest( void )
{
    CK_RV rv;

    /* RC2 Power-Up SelfTest(s). */
    rv = pk11_fips_RC2_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* RC4 Power-Up SelfTest(s). */
    rv = pk11_fips_RC4_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* DES Power-Up SelfTest(s). */
    rv = pk11_fips_DES_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* DES3 Power-Up SelfTest(s). */
    rv = pk11_fips_DES3_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* MD2 Power-Up SelfTest(s). */
    rv = pk11_fips_MD2_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* MD5 Power-Up SelfTest(s). */
    rv = pk11_fips_MD5_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* SHA-1 Power-Up SelfTest(s). */
    rv = pk11_fips_SHA1_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* RSA Power-Up SelfTest(s). */
    rv = pk11_fips_RSA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* DSA Power-Up SelfTest(s). */
    rv = pk11_fips_DSA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* Passed Power-Up SelfTest(s). */
    return( CKR_OK );
}

