/* ***** BEGIN LICENSE BLOCK *****
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
/* $Id: nsslowhash.c,v 1.6 2010/09/10 00:42:36 emaldona%redhat.com Exp $ */

#include "stubs.h"
#include "prtypes.h"
#include "secerr.h"
#include "pkcs11t.h"
#include "blapi.h"
#include "sechash.h"
#include "nsslowhash.h"

/* FIPS preprocessor directives for message digests             */
#define FIPS_KNOWN_HASH_MESSAGE_LENGTH          64  /* 512-bits */

/* Known Hash Message (512-bits).  Used for all hashes (incl. SHA-N [N>1]). */
static const PRUint8 known_hash_message[] = {
  "The test message for the MD2, MD5, and SHA-1 hashing algorithms." };

static CK_RV
freebl_fips_MD2_PowerUpSelfTest( void )
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
freebl_fips_MD5_PowerUpSelfTest( void )
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

static CK_RV
freebl_fips_SHA_PowerUpSelfTest( void )
{
    /* SHA-1 Known Digest Message (160-bits). */
    static const PRUint8 sha1_known_digest[] = {
			       0x0a,0x6d,0x07,0xba,0x1e,0xbd,0x8a,0x1b,
			       0x72,0xf6,0xc7,0x22,0xf1,0x27,0x9f,0xf0,
			       0xe0,0x68,0x47,0x7a};

    /* SHA-224 Known Digest Message (224-bits). */
    static const PRUint8 sha224_known_digest[] = {
        0x1c,0xc3,0x06,0x8e,0xce,0x37,0x68,0xfb, 
        0x1a,0x82,0x4a,0xbe,0x2b,0x00,0x51,0xf8,
        0x9d,0xb6,0xe0,0x90,0x0d,0x00,0xc9,0x64,
        0x9a,0xb8,0x98,0x4e};

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


static CK_RV
freebl_fipsSoftwareIntegrityTest(void)
{
    CK_RV crv = CKR_OK;

    /* make sure that our check file signatures are OK */
    if (!BLAPI_VerifySelf(SHLIB_PREFIX"freebl"SHLIB_VERSION"."SHLIB_SUFFIX)) {
	crv = CKR_DEVICE_ERROR; /* better error code? checksum error? */
    }
    return crv;
}

CK_RV
freebl_fipsPowerUpSelfTest( void )
{
    CK_RV rv;

    /* MD2 Power-Up SelfTest(s). */
    rv = freebl_fips_MD2_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* MD5 Power-Up SelfTest(s). */
    rv = freebl_fips_MD5_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* SHA-X Power-Up SelfTest(s). */
    rv = freebl_fips_SHA_PowerUpSelfTest();

    if( rv != CKR_OK )
        return rv;

    /* Software/Firmware Integrity Test. */
    rv = freebl_fipsSoftwareIntegrityTest();

    if( rv != CKR_OK )
        return rv;

    /* Passed Power-Up SelfTest(s). */
    return( CKR_OK );
}

struct NSSLOWInitContextStr {
   int count;
};

struct NSSLOWHASHContextStr {
    const SECHashObject *hashObj;
    void *hashCtxt;
   
};

static int nsslow_GetFIPSEnabled(void) {
#ifdef LINUX
    FILE *f;
    char d;
    size_t size;

    f = fopen("/proc/sys/crypto/fips_enabled", "r");
    if (!f)
        return 0;

    size = fread(&d, 1, 1, f);
    fclose(f);
    if (size != 1)
        return 0;
    if (d != '1')
        return 0;
#endif
    return 1;
}


static int post = 0;
static int post_failed = 0;

static NSSLOWInitContext dummyContext = { 0 };

NSSLOWInitContext *
NSSLOW_Init(void)
{
    SECStatus rv;
    CK_RV crv;
    PRBool nsprAvailable = PR_FALSE;


    rv = FREEBL_InitStubs();
    nsprAvailable = (rv ==  SECSuccess ) ? PR_TRUE : PR_FALSE;

    if (post_failed) {
	return NULL;
    }
	

    if (!post && nsslow_GetFIPSEnabled()) {
	crv = freebl_fipsPowerUpSelfTest();
	if (crv != CKR_OK) {
	    post_failed = 1;
	    return NULL;
	}
    }
    post = 1;

    
    return &dummyContext;
}

void
NSSLOW_Shutdown(NSSLOWInitContext *context)
{
   PORT_Assert(context == &dummyContext);
   return;
}

void
NSSLOW_Reset(NSSLOWInitContext *context)
{
   PORT_Assert(context == &dummyContext);
   post_failed = 0;
   post = 0;
   return;
}

NSSLOWHASHContext *
NSSLOWHASH_NewContext(NSSLOWInitContext *initContext, 
			HASH_HashType hashType)
{
   NSSLOWHASHContext *context;

   if (post_failed) {
	PORT_SetError(SEC_ERROR_PKCS11_DEVICE_ERROR);
	return NULL;
   }

   if (initContext != &dummyContext) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return (NULL);
   }

   context = PORT_ZNew(NSSLOWHASHContext);
   if (!context) {
	return NULL;
   }
   context->hashObj = HASH_GetRawHashObject(hashType);
   if (!context->hashObj) {
	PORT_Free(context);
	return NULL;
   }
   context->hashCtxt = context->hashObj->create();
   if (!context->hashCtxt) {
	PORT_Free(context);
	return NULL;
   }

   return context;
}

void
NSSLOWHASH_Begin(NSSLOWHASHContext *context)
{
   return context->hashObj->begin(context->hashCtxt);
}

void
NSSLOWHASH_Update(NSSLOWHASHContext *context, const unsigned char *buf, 
						 unsigned int len)
{
   return context->hashObj->update(context->hashCtxt, buf, len);
}

void
NSSLOWHASH_End(NSSLOWHASHContext *context, unsigned char *buf, 
					 unsigned int *ret, unsigned int len)
{
   return context->hashObj->end(context->hashCtxt, buf, ret, len);
}

void
NSSLOWHASH_Destroy(NSSLOWHASHContext *context)
{
   context->hashObj->destroy(context->hashCtxt, PR_TRUE);
   PORT_Free(context);
}

unsigned int
NSSLOWHASH_Length(NSSLOWHASHContext *context)
{
   return context->hashObj->length;
}
