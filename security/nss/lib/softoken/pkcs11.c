/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *	Dr Stephen Henson <stephen.henson@gemplus.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/*
 * This file implements PKCS 11 on top of our existing security modules
 *
 * For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *   This implementation has two slots:
 *	slot 1 is our generic crypto support. It does not require login.
 *   It supports Public Key ops, and all they bulk ciphers and hashes. 
 *   It can also support Private Key ops for imported Private keys. It does 
 *   not have any token storage.
 *	slot 2 is our private key support. It requires a login before use. It
 *   can store Private Keys and Certs as token objects. Currently only private
 *   keys and their associated Certificates are saved on the token.
 *
 *   In this implementation, session objects are only visible to the session
 *   that created or generated them.
 */
#include "seccomon.h"
#include "secitem.h"
#include "pkcs11.h"
#include "pkcs11i.h"
#include "softoken.h"
#include "cert.h"
#include "keylow.h"
#include "blapi.h"
#include "secder.h"
#include "secport.h"
#include "certdb.h"

#include "private.h"

 
/*
 * ******************** Static data *******************************
 */

/* The next three strings must be exactly 32 characters long */
static char *manufacturerID      = "mozilla.org                     ";
static char manufacturerID_space[33];
static char *libraryDescription  = "NSS Internal Crypto Services    ";
static char libraryDescription_space[33];
static char *tokDescription      = "NSS Generic Crypto Services     ";
static char tokDescription_space[33];
static char *privTokDescription  = "NSS Certificate DB              ";
static char privTokDescription_space[33];
/* The next two strings must be exactly 64 characters long, with the
   first 32 characters meaningful  */
static char *slotDescription     = 
	"NSS Internal Cryptographic Services Version 3.2                 ";
static char slotDescription_space[65];
static char *privSlotDescription = 
	"NSS User Private Key and Certificate Services                   ";
static char privSlotDescription_space[65];
static int minimumPinLen = 0;

#define __PASTE(x,y)    x##y

/*
 * we renamed all our internal functions, get the correct
 * definitions for them...
 */ 
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_EXTERN extern
#define CK_PKCS11_FUNCTION_INFO(func) \
		CK_RV __PASTE(NS,func)
#define CK_NEED_ARG_LIST	1
 
#include "pkcs11f.h"
 
 
 
/* build the crypto module table */
static CK_FUNCTION_LIST pk11_funcList = {
    { 1, 10 },
 
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST
 
#define CK_PKCS11_FUNCTION_INFO(func) \
				__PASTE(NS,func),
#include "pkcs11f.h"
 
};
 
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST
 
 
#undef __PASTE

/* List of DES Weak Keys */ 
typedef unsigned char desKey[8];
static desKey  pk11_desWeakTable[] = {
#ifdef noParity
    /* weak */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x1e, 0x1e, 0x1e, 0x1e, 0x0e, 0x0e, 0x0e, 0x0e },
    { 0xe0, 0xe0, 0xe0, 0xe0, 0xf0, 0xf0, 0xf0, 0xf0 },
    { 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe },
    /* semi-weak */
    { 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe },
    { 0xfe, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0xfe },

    { 0x1e, 0xe0, 0x1e, 0xe0, 0x0e, 0xf0, 0x0e, 0xf0 },
    { 0xe0, 0x1e, 0xe0, 0x1e, 0xf0, 0x0e, 0xf0, 0x0e },

    { 0x00, 0xe0, 0x00, 0xe0, 0x00, 0x0f, 0x00, 0x0f },
    { 0xe0, 0x00, 0xe0, 0x00, 0xf0, 0x00, 0xf0, 0x00 },

    { 0x1e, 0xfe, 0x1e, 0xfe, 0x0e, 0xfe, 0x0e, 0xfe },
    { 0xfe, 0x1e, 0xfe, 0x1e, 0xfe, 0x0e, 0xfe, 0x0e },

    { 0x00, 0x1e, 0x00, 0x1e, 0x00, 0x0e, 0x00, 0x0e },
    { 0x1e, 0x00, 0x1e, 0x00, 0x0e, 0x00, 0x0e, 0x00 },

    { 0xe0, 0xfe, 0xe0, 0xfe, 0xf0, 0xfe, 0xf0, 0xfe },
    { 0xfe, 0xe0, 0xfe, 0xe0, 0xfe, 0xf0, 0xfe, 0xf0 },
#else
    /* weak */
    { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
    { 0x1f, 0x1f, 0x1f, 0x1f, 0x0e, 0x0e, 0x0e, 0x0e },
    { 0xe0, 0xe0, 0xe0, 0xe0, 0xf1, 0xf1, 0xf1, 0xf1 },
    { 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe },

    /* semi-weak */
    { 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe },
    { 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01 },

    { 0x1f, 0xe0, 0x1f, 0xe0, 0x0e, 0xf1, 0x0e, 0xf1 },
    { 0xe0, 0x1f, 0xe0, 0x1f, 0xf1, 0x0e, 0xf1, 0x0e },

    { 0x01, 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1 },
    { 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1, 0x01 },

    { 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e, 0xfe },
    { 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e },

    { 0x01, 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e },
    { 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e, 0x01 },

    { 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1, 0xfe },
    { 0xfe, 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1 }
#endif
};

    
static int pk11_desWeakTableSize = sizeof(pk11_desWeakTable)/
						sizeof(pk11_desWeakTable[0]);

/* DES KEY Parity conversion table. Takes each byte/2 as an index, returns
 * that byte with the proper parity bit set */
static unsigned char parityTable[256] = {
/* Even...0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e */
/* E */   0x01,0x02,0x04,0x07,0x08,0x0b,0x0d,0x0e,
/* Odd....0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e */
/* O */   0x10,0x13,0x15,0x16,0x19,0x1a,0x1c,0x1f,
/* Odd....0x20,0x22,0x24,0x26,0x28,0x2a,0x2c,0x2e */
/* O */   0x20,0x23,0x25,0x26,0x29,0x2a,0x2c,0x2f,
/* Even...0x30,0x32,0x34,0x36,0x38,0x3a,0x3c,0x3e */
/* E */   0x31,0x32,0x34,0x37,0x38,0x3b,0x3d,0x3e,
/* Odd....0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e */
/* O */   0x40,0x43,0x45,0x46,0x49,0x4a,0x4c,0x4f,
/* Even...0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e */
/* E */   0x51,0x52,0x54,0x57,0x58,0x5b,0x5d,0x5e,
/* Even...0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e */
/* E */   0x61,0x62,0x64,0x67,0x68,0x6b,0x6d,0x6e,
/* Odd....0x70,0x72,0x74,0x76,0x78,0x7a,0x7c,0x7e */
/* O */   0x70,0x73,0x75,0x76,0x79,0x7a,0x7c,0x7f,
/* Odd....0x80,0x82,0x84,0x86,0x88,0x8a,0x8c,0x8e */
/* O */   0x80,0x83,0x85,0x86,0x89,0x8a,0x8c,0x8f,
/* Even...0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e */
/* E */   0x91,0x92,0x94,0x97,0x98,0x9b,0x9d,0x9e,
/* Even...0xa0,0xa2,0xa4,0xa6,0xa8,0xaa,0xac,0xae */
/* E */   0xa1,0xa2,0xa4,0xa7,0xa8,0xab,0xad,0xae,
/* Odd....0xb0,0xb2,0xb4,0xb6,0xb8,0xba,0xbc,0xbe */
/* O */   0xb0,0xb3,0xb5,0xb6,0xb9,0xba,0xbc,0xbf,
/* Even...0xc0,0xc2,0xc4,0xc6,0xc8,0xca,0xcc,0xce */
/* E */   0xc1,0xc2,0xc4,0xc7,0xc8,0xcb,0xcd,0xce,
/* Odd....0xd0,0xd2,0xd4,0xd6,0xd8,0xda,0xdc,0xde */
/* O */   0xd0,0xd3,0xd5,0xd6,0xd9,0xda,0xdc,0xdf,
/* Odd....0xe0,0xe2,0xe4,0xe6,0xe8,0xea,0xec,0xee */
/* O */   0xe0,0xe3,0xe5,0xe6,0xe9,0xea,0xec,0xef,
/* Even...0xf0,0xf2,0xf4,0xf6,0xf8,0xfa,0xfc,0xfe */
/* E */   0xf1,0xf2,0xf4,0xf7,0xf8,0xfb,0xfd,0xfe,
};

/* Mechanisms */
struct mechanismList {
    CK_MECHANISM_TYPE	type;
    CK_MECHANISM_INFO	domestic;
    PRBool		privkey;
};

/*
 * the following table includes a complete list of mechanism defined by
 * PKCS #11 version 2.01. Those Mechanisms not supported by this PKCS #11
 * module are ifdef'ed out.
 */
#define CKF_EN_DE		CKF_ENCRYPT      | CKF_DECRYPT
#define CKF_WR_UN		CKF_WRAP         | CKF_UNWRAP
#define CKF_SN_VR		CKF_SIGN         | CKF_VERIFY
#define CKF_SN_RE		CKF_SIGN_RECOVER | CKF_VERIFY_RECOVER

#define CKF_EN_DE_WR_UN 	CKF_EN_DE       | CKF_WR_UN
#define CKF_SN_VR_RE		CKF_SN_VR       | CKF_SN_RE
#define CKF_DUZ_IT_ALL		CKF_EN_DE_WR_UN | CKF_SN_VR_RE

static struct mechanismList mechanisms[] = {

     /*
      * PKCS #11 Mechanism List.
      *
      * The first argument is the PKCS #11 Mechanism we support.
      * The second argument is Mechanism info structure. It includes:
      *    The minimum key size,
      *       in bits for RSA, DSA, DH, KEA, RC2 and RC4 * algs.
      *       in bytes for RC5, AES, and CAST*
      *       ignored for DES*, IDEA and FORTEZZA based
      *    The maximum key size,
      *       in bits for RSA, DSA, DH, KEA, RC2 and RC4 * algs.
      *       in bytes for RC5, AES, and CAST*
      *       ignored for DES*, IDEA and FORTEZZA based
      *     Flags
      *	      What operations are supported by this mechanism.
      *  The third argument is a bool which tells if this mechanism is 
      *    supported in the database token.
      *
      */

     /* ------------------------- RSA Operations ---------------------------*/
     {CKM_RSA_PKCS_KEY_PAIR_GEN,{128,0xffffffff,CKF_GENERATE_KEY_PAIR},PR_TRUE},
     {CKM_RSA_PKCS,		{128,0xffffffff, CKF_DUZ_IT_ALL},PR_TRUE},
#ifdef PK11_RSA9796_SUPPORTED
     {CKM_RSA_9796,		{128,0xffffffff, CKF_DUZ_IT_ALL},PR_TRUE}, 
#endif
     {CKM_RSA_X_509,		{128,0xffffffff, CKF_DUZ_IT_ALL},PR_TRUE}, 
     /* -------------- RSA Multipart Signing Operations -------------------- */
     {CKM_MD2_RSA_PKCS,		{128,0xffffffff, CKF_SN_VR},	PR_TRUE},
     {CKM_MD5_RSA_PKCS,		{128,0xffffffff, CKF_SN_VR},	PR_TRUE},
     {CKM_SHA1_RSA_PKCS,	{128,0xffffffff, CKF_SN_VR}, 	PR_TRUE},
     /* ------------------------- DSA Operations --------------------------- */
     {CKM_DSA_KEY_PAIR_GEN,	{512, 1024, CKF_GENERATE_KEY_PAIR}, PR_TRUE},
     {CKM_DSA,			{512, 1024, CKF_SN_VR},		PR_TRUE},
     {CKM_DSA_SHA1,		{512, 1024, CKF_SN_VR},		PR_TRUE},
     /* -------------------- Diffie Hellman Operations --------------------- */
     /* no diffie hellman yet */
     {CKM_DH_PKCS_KEY_PAIR_GEN,	{128, 1024, CKF_GENERATE_KEY_PAIR}, PR_TRUE}, 
     {CKM_DH_PKCS_DERIVE,	{128, 1024, CKF_DERIVE}, 	PR_TRUE}, 
     /* ------------------------- RC2 Operations --------------------------- */
     {CKM_RC2_KEY_GEN,		{1, 128, CKF_GENERATE},		PR_FALSE},
     {CKM_RC2_ECB,		{1, 128, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_RC2_CBC,		{1, 128, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_RC2_MAC,		{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_RC2_MAC_GENERAL,	{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_RC2_CBC_PAD,		{1, 128, CKF_EN_DE_WR_UN},	PR_FALSE},
     /* ------------------------- RC4 Operations --------------------------- */
     {CKM_RC4_KEY_GEN,		{1, 256, CKF_GENERATE},		PR_FALSE},
     {CKM_RC4,			{1, 256, CKF_EN_DE_WR_UN},	PR_FALSE},
     /* ------------------------- DES Operations --------------------------- */
     {CKM_DES_KEY_GEN,		{ 8,  8, CKF_GENERATE},		PR_FALSE},
     {CKM_DES_ECB,		{ 8,  8, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_DES_CBC,		{ 8,  8, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_DES_MAC,		{ 8,  8, CKF_SN_VR},		PR_FALSE},
     {CKM_DES_MAC_GENERAL,	{ 8,  8, CKF_SN_VR},		PR_FALSE},
     {CKM_DES_CBC_PAD,		{ 8,  8, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_DES2_KEY_GEN,		{24, 24, CKF_GENERATE},		PR_FALSE},
     {CKM_DES3_KEY_GEN,		{24, 24, CKF_GENERATE},		PR_TRUE },
     {CKM_DES3_ECB,		{24, 24, CKF_EN_DE_WR_UN},	PR_TRUE },
     {CKM_DES3_CBC,		{24, 24, CKF_EN_DE_WR_UN},	PR_TRUE },
     {CKM_DES3_MAC,		{24, 24, CKF_SN_VR},		PR_TRUE },
     {CKM_DES3_MAC_GENERAL,	{24, 24, CKF_SN_VR},		PR_TRUE },
     {CKM_DES3_CBC_PAD,		{24, 24, CKF_EN_DE_WR_UN},	PR_TRUE },
     /* ------------------------- CDMF Operations --------------------------- */
     {CKM_CDMF_KEY_GEN,		{8,  8, CKF_GENERATE},		PR_FALSE},
     {CKM_CDMF_ECB,		{8,  8, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_CDMF_CBC,		{8,  8, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_CDMF_MAC,		{8,  8, CKF_SN_VR},		PR_FALSE},
     {CKM_CDMF_MAC_GENERAL,	{8,  8, CKF_SN_VR},		PR_FALSE},
     {CKM_CDMF_CBC_PAD,		{8,  8, CKF_EN_DE_WR_UN},	PR_FALSE},
     /* ------------------------- AES Operations --------------------------- */
     {CKM_AES_KEY_GEN,		{16, 32, CKF_GENERATE},		PR_FALSE},
     {CKM_AES_ECB,		{16, 32, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_AES_CBC,		{16, 32, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_AES_MAC,		{16, 32, CKF_SN_VR},		PR_FALSE},
     {CKM_AES_MAC_GENERAL,	{16, 32, CKF_SN_VR},		PR_FALSE},
     {CKM_AES_CBC_PAD,		{16, 32, CKF_EN_DE_WR_UN},	PR_FALSE},
     /* ------------------------- Hashing Operations ----------------------- */
     {CKM_MD2,			{0,   0, CKF_DIGEST},		PR_FALSE},
     {CKM_MD2_HMAC,		{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_MD2_HMAC_GENERAL,	{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_MD5,			{0,   0, CKF_DIGEST},		PR_FALSE},
     {CKM_MD5_HMAC,		{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_MD5_HMAC_GENERAL,	{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_SHA_1,		{0,   0, CKF_DIGEST},		PR_FALSE},
     {CKM_SHA_1_HMAC,		{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_SHA_1_HMAC_GENERAL,	{1, 128, CKF_SN_VR},		PR_FALSE},
     {CKM_TLS_PRF_GENERAL,	{0, 512, CKF_SN_VR},		PR_FALSE},
     /* ------------------------- CAST Operations --------------------------- */
#ifdef PK11_CAST_SUPPORTED
     /* Cast operations are not supported ( yet? ) */
     {CKM_CAST_KEY_GEN,		{1,  8, CKF_GENERATE},		PR_FALSE}, 
     {CKM_CAST_ECB,		{1,  8, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST_CBC,		{1,  8, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST_MAC,		{1,  8, CKF_SN_VR},		PR_FALSE}, 
     {CKM_CAST_MAC_GENERAL,	{1,  8, CKF_SN_VR},		PR_FALSE}, 
     {CKM_CAST_CBC_PAD,		{1,  8, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST3_KEY_GEN,	{1, 16, CKF_GENERATE},		PR_FALSE}, 
     {CKM_CAST3_ECB,		{1, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST3_CBC,		{1, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST3_MAC,		{1, 16, CKF_SN_VR},		PR_FALSE}, 
     {CKM_CAST3_MAC_GENERAL,	{1, 16, CKF_SN_VR},		PR_FALSE}, 
     {CKM_CAST3_CBC_PAD,	{1, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST5_KEY_GEN,	{1, 16, CKF_GENERATE},		PR_FALSE}, 
     {CKM_CAST5_ECB,		{1, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST5_CBC,		{1, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_CAST5_MAC,		{1, 16, CKF_SN_VR},		PR_FALSE}, 
     {CKM_CAST5_MAC_GENERAL,	{1, 16, CKF_SN_VR},		PR_FALSE}, 
     {CKM_CAST5_CBC_PAD,	{1, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
#endif
#if NSS_SOFTOKEN_DOES_RC5
     /* ------------------------- RC5 Operations --------------------------- */
     {CKM_RC5_KEY_GEN,		{1, 32, CKF_GENERATE}, 	PR_FALSE},
     {CKM_RC5_ECB,		{1, 32, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_RC5_CBC,		{1, 32, CKF_EN_DE_WR_UN},	PR_FALSE},
     {CKM_RC5_MAC,		{1, 32, CKF_SN_VR},  		PR_FALSE},
     {CKM_RC5_MAC_GENERAL,	{1, 32, CKF_SN_VR},  		PR_FALSE},
     {CKM_RC5_CBC_PAD,		{1, 32, CKF_EN_DE_WR_UN}, 	PR_FALSE},
#endif
#ifdef PK11_IDEA_SUPPORTED
     /* ------------------------- IDEA Operations -------------------------- */
     {CKM_IDEA_KEY_GEN,		{16, 16, CKF_GENERATE}, 	PR_FALSE}, 
     {CKM_IDEA_ECB,		{16, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_IDEA_CBC,		{16, 16, CKF_EN_DE_WR_UN},	PR_FALSE}, 
     {CKM_IDEA_MAC,		{16, 16, CKF_SN_VR},		PR_FALSE}, 
     {CKM_IDEA_MAC_GENERAL,	{16, 16, CKF_SN_VR},		PR_FALSE}, 
     {CKM_IDEA_CBC_PAD,		{16, 16, CKF_EN_DE_WR_UN}, 	PR_FALSE}, 
#endif
     /* --------------------- Secret Key Operations ------------------------ */
     {CKM_GENERIC_SECRET_KEY_GEN,	{1, 32, CKF_GENERATE}, PR_FALSE}, 
     {CKM_CONCATENATE_BASE_AND_KEY,	{1, 32, CKF_GENERATE}, PR_FALSE}, 
     {CKM_CONCATENATE_BASE_AND_DATA,	{1, 32, CKF_GENERATE}, PR_FALSE}, 
     {CKM_CONCATENATE_DATA_AND_BASE,	{1, 32, CKF_GENERATE}, PR_FALSE}, 
     {CKM_XOR_BASE_AND_DATA,		{1, 32, CKF_GENERATE}, PR_FALSE}, 
     {CKM_EXTRACT_KEY_FROM_KEY,		{1, 32, CKF_DERIVE},   PR_FALSE}, 
     /* ---------------------- SSL Key Derivations ------------------------- */
     {CKM_SSL3_PRE_MASTER_KEY_GEN,	{48, 48, CKF_GENERATE}, PR_FALSE}, 
     {CKM_SSL3_MASTER_KEY_DERIVE,	{48, 48, CKF_DERIVE},   PR_FALSE}, 
     {CKM_SSL3_MASTER_KEY_DERIVE_DH,	{8, 128, CKF_DERIVE},   PR_FALSE}, 
     {CKM_SSL3_KEY_AND_MAC_DERIVE,	{48, 48, CKF_DERIVE},   PR_FALSE}, 
     {CKM_SSL3_MD5_MAC,			{ 0, 16, CKF_DERIVE},   PR_FALSE}, 
     {CKM_SSL3_SHA1_MAC,		{ 0, 20, CKF_DERIVE},   PR_FALSE}, 
     {CKM_MD5_KEY_DERIVATION,		{ 0, 16, CKF_DERIVE},   PR_FALSE}, 
     {CKM_MD2_KEY_DERIVATION,		{ 0, 16, CKF_DERIVE},   PR_FALSE}, 
     {CKM_SHA1_KEY_DERIVATION,		{ 0, 20, CKF_DERIVE},   PR_FALSE}, 
     {CKM_TLS_MASTER_KEY_DERIVE,	{48, 48, CKF_DERIVE},   PR_FALSE}, 
     {CKM_TLS_MASTER_KEY_DERIVE_DH,	{8, 128, CKF_DERIVE},   PR_FALSE}, 
     {CKM_TLS_KEY_AND_MAC_DERIVE,	{48, 48, CKF_DERIVE},   PR_FALSE}, 
     /* ---------------------- PBE Key Derivations  ------------------------ */
     {CKM_PBE_MD2_DES_CBC,		{8, 8, CKF_DERIVE},   PR_TRUE},
     {CKM_PBE_MD5_DES_CBC,		{8, 8, CKF_DERIVE},   PR_TRUE},
     /* ------------------ NETSCAPE PBE Key Derivations  ------------------- */
     {CKM_NETSCAPE_PBE_SHA1_DES_CBC,	     { 8, 8, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC,  {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC, {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC,  {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC, {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4,	     {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4,     {128,128, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_DES3_EDE_CBC,	     {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_DES2_EDE_CBC,	     {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC2_40_CBC,		     {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC2_128_CBC,		     {128,128, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC4_40,		     {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC4_128,		     {128,128, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN,    {1,32, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN,     {1,32, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN,     {1,32, CKF_GENERATE}, PR_TRUE},
};
static CK_ULONG mechanismCount = sizeof(mechanisms)/sizeof(mechanisms[0]);
/* load up our token database */
static CK_RV pk11_importKeyDB(PK11Slot *slot);


static char *
pk11_setStringName(char *inString, char *buffer, int buffer_length) {
    int full_length, string_length;

    full_length = buffer_length -1;
    string_length = PORT_Strlen(inString);
    if (string_length > full_length) string_length = full_length;
    PORT_Memset(buffer,' ',full_length);
    buffer[full_length] = 0;
    PORT_Memcpy(buffer,inString,full_length);
    return buffer;
}
/*
 * Configuration utils
 */
static CK_RV
pk11_configure(char *man, char *libdes, char *tokdes, char *ptokdes,
	char *slotdes, char *pslotdes, char *fslotdes, char *fpslotdes,
	int minPwd, int pwRequired) 
{

    /* make sure the internationalization was done correctly... */
    if (man) {
	manufacturerID = pk11_setStringName(man,manufacturerID_space,
						sizeof(manufacturerID_space));
    }
    if (libdes) {
	libraryDescription = pk11_setStringName(libdes,
		libraryDescription_space, sizeof(libraryDescription_space));
    }
    if (tokdes) {
	tokDescription = pk11_setStringName(tokdes,tokDescription_space,
						 sizeof(tokDescription_space));
    }
    if (ptokdes) {
	privTokDescription = pk11_setStringName(ptokdes,
		privTokDescription_space, sizeof(privTokDescription_space));
    }
    if (slotdes) {
	slotDescription = pk11_setStringName(slotdes,slotDescription_space, 
					sizeof(slotDescription_space));
    }
    if (pslotdes) {
	privSlotDescription = pk11_setStringName(pslotdes,
		privSlotDescription_space, sizeof(privSlotDescription_space));
    }

    if (minimumPinLen <= PK11_MAX_PIN) {
	minimumPinLen = minPwd;
    }
    if ((minimumPinLen == 0) && (pwRequired) && 
		(minimumPinLen <= PK11_MAX_PIN)) {
	minimumPinLen = 1;
    }

    PK11_ConfigureFIPS(fslotdes,fpslotdes);

    return CKR_OK;
}

/*
 * ******************** Password Utilities *******************************
 */

/* Handle to give the password to the database. user arg should be a pointer
 * to the slot. */
static SECItem *pk11_givePass(void *sp,SECKEYKeyDBHandle *handle)
{
    PK11Slot *slot = (PK11Slot *)sp;

    if (slot->password == NULL) return NULL;

    return SECITEM_DupItem(slot->password);
}

/*
 * see if the key DB password is enabled
 */
PRBool
pk11_hasNullPassword(SECItem **pwitem)
{
    PRBool pwenabled;
    SECKEYKeyDBHandle *keydb;
    
    keydb = SECKEY_GetDefaultKeyDB();
    pwenabled = PR_FALSE;
    *pwitem = NULL;
    if (SECKEY_HasKeyDBPassword (keydb) == SECSuccess) {
	*pwitem = SECKEY_HashPassword("", keydb->global_salt);
	if ( *pwitem ) {
	    if (SECKEY_CheckKeyDBPassword (keydb, *pwitem) == SECSuccess) {
		pwenabled = PR_TRUE;
	    } else {
	    	SECITEM_ZfreeItem(*pwitem, PR_TRUE);
		*pwitem = NULL;
	    }
	}
    }

    return pwenabled;
}

/*
 * ******************** Object Creation Utilities ***************************
 */


/* Make sure a given attribute exists. If it doesn't, initialize it to
 * value and len
 */
CK_RV
pk11_defaultAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type,void *value,
							unsigned int len)
{
    if ( !pk11_hasAttribute(object, type)) {
	return pk11_AddAttributeType(object,type,value,len);
    }
    return CKR_OK;
}

/*
 * check the consistancy and initialize a Data Object 
 */
static CK_RV
pk11_handleDataObject(PK11Session *session,PK11Object *object)
{
    CK_RV crv;

    /* first reject private and token data objects */
    if (pk11_isTrue(object,CKA_PRIVATE) || pk11_isTrue(object,CKA_TOKEN)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* now just verify the required date fields */
    crv = pk11_defaultAttribute(object,CKA_APPLICATION,NULL,0);
    if (crv != CKR_OK) return crv;
    crv = pk11_defaultAttribute(object,CKA_VALUE,NULL,0);
    if (crv != CKR_OK) return crv;

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Certificate Object 
 */
static CK_RV
pk11_handleCertObject(PK11Session *session,PK11Object *object)
{
    PK11Attribute *attribute;
    CK_CERTIFICATE_TYPE type;
    SECItem derCert;
    char *label;
    CERTCertDBHandle *handle;
    CERTCertificate *cert;
    CK_RV crv;

    /* certificates must have a type */
    if ( !pk11_hasAttribute(object,CKA_CERTIFICATE_TYPE) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    /* we can't store any certs private */
    if (pk11_isTrue(object,CKA_PRIVATE)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
	
    /* We only support X.509 Certs for now */
    attribute = pk11_FindAttribute(object,CKA_CERTIFICATE_TYPE);
    if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;
    type = *(CK_CERTIFICATE_TYPE *)attribute->attrib.pValue;
    pk11_FreeAttribute(attribute);

    if (type != CKC_X_509) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* X.509 Certificate */

    /* make sure we have a cert */
    if ( !pk11_hasAttribute(object,CKA_VALUE) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    /* in PKCS #11, Subject is a required field */
    if ( !pk11_hasAttribute(object,CKA_SUBJECT) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    /*
     * now parse the certificate
     */
    handle = CERT_GetDefaultCertDB();

    /* get the nickname */
    label = pk11_getString(object,CKA_LABEL);
    object->label = label;
    if (label == NULL) {
	return CKR_HOST_MEMORY;
    }
  
    /* get the der cert */ 
    attribute = pk11_FindAttribute(object,CKA_VALUE);
    derCert.data = (unsigned char *)attribute->attrib.pValue;
    derCert.len = attribute->attrib.ulValueLen ;
    
    cert = CERT_NewTempCertificate(handle, &derCert, label, PR_FALSE, PR_TRUE);
    pk11_FreeAttribute(attribute);
    if (cert == NULL) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* add it to the object */
    object->objectInfo = cert;
    object->infoFree = (PK11Free) CERT_DestroyCertificate;
    
    /* now just verify the required date fields */
    crv = pk11_defaultAttribute(object, CKA_ID, NULL, 0);
    if (crv != CKR_OK) { return crv; }
    crv = pk11_defaultAttribute(object,CKA_ISSUER,
				pk11_item_expand(&cert->derIssuer));
    if (crv != CKR_OK) { return crv; }
    crv = pk11_defaultAttribute(object,CKA_SERIAL_NUMBER,
				pk11_item_expand(&cert->serialNumber));
    if (crv != CKR_OK) { return crv; }


    if (pk11_isTrue(object,CKA_TOKEN)) {
	SECCertUsage *certUsage = NULL;
	CERTCertTrust trust = { CERTDB_USER, CERTDB_USER, CERTDB_USER };

	attribute = pk11_FindAttribute(object,CKA_NETSCAPE_TRUST);
	if(attribute) {
	    certUsage = (SECCertUsage*)attribute->attrib.pValue;
	    pk11_FreeAttribute(attribute);
	}

	/* Temporary for PKCS 12 */
	if(cert->nickname == NULL) {
	    /* use the arena so we at least don't leak memory  */
	    cert->nickname = PORT_ArenaStrdup(cert->arena, label);
	}

	/* only add certs that have a private key */
	if (SECKEY_KeyForCertExists(SECKEY_GetDefaultKeyDB(),cert) 
							!= SECSuccess) {
	    return CKR_ATTRIBUTE_VALUE_INVALID;
	}
	if (!cert->isperm) {
	    if (CERT_AddTempCertToPerm(cert, label, &trust) != SECSuccess) {
		return CKR_HOST_MEMORY;
	    }
        } else {
	    CERT_ChangeCertTrust(cert->dbhandle,cert,&trust);
	}
	if(certUsage) {
	    if(CERT_ChangeCertTrustByUsage(CERT_GetDefaultCertDB(),
				cert, *certUsage) != SECSuccess) {
		return CKR_HOST_MEMORY;
	    }
	}
	object->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_CERT);
	object->inDB = PR_TRUE;
    }

    /* label has been adopted by object->label */
    /*PORT_Free(label); */

    return CKR_OK;
}

SECKEYLowPublicKey * pk11_GetPubKey(PK11Object *object,CK_KEY_TYPE key);
/*
 * check the consistancy and initialize a Public Key Object 
 */
static CK_RV
pk11_handlePublicKeyObject(PK11Object *object,CK_KEY_TYPE key_type)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL encrypt = CK_TRUE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL wrap = CK_TRUE;
    CK_RV crv;

    switch (key_type) {
    case CKK_RSA:
	if ( !pk11_hasAttribute(object, CKA_MODULUS)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_PUBLIC_EXPONENT)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	break;
    case CKK_DSA:
	if ( !pk11_hasAttribute(object, CKA_SUBPRIME)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
    case CKK_DH:
	if ( !pk11_hasAttribute(object, CKA_PRIME)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_BASE)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_VALUE)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	encrypt = CK_FALSE;
	recover = CK_FALSE;
	wrap = CK_FALSE;
	break;
    default:
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* make sure the required fields exist */
    crv = pk11_defaultAttribute(object,CKA_SUBJECT,NULL,0);
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_ENCRYPT,&encrypt,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_VERIFY,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_VERIFY_RECOVER,
						&recover,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_WRAP,&wrap,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 

    object->objectInfo = pk11_GetPubKey(object,key_type);
    object->infoFree = (PK11Free) SECKEY_LowDestroyPublicKey;

    if (pk11_isTrue(object,CKA_TOKEN)) {
        object->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PUB);
    }

    return CKR_OK;
}
/* pk11_GetPubItem returns data associated with the public key.
 * one only needs to free the public key. This comment is here
 * because this sematic would be non-obvious otherwise. All callers
 * should include this comment.
 */
static SECItem *
pk11_GetPubItem(SECKEYPublicKey *pubKey) {
    SECItem *pubItem = NULL;
    /* get value to compare from the cert's public key */
    switch ( pubKey->keyType ) {
    case rsaKey:
	    pubItem = &pubKey->u.rsa.modulus;
	    break;
    case dsaKey:
	    pubItem = &pubKey->u.dsa.publicValue;
	    break;
    default:
	    break;
    }
    return pubItem;
}

/* convert a high key type to a low key type. This will go away when
 * the last SECKEYPublicKey structs go away.
 */
LowKeyType
seckeyLow_KeyType(KeyType keyType)
{
    switch (keyType) {
    case rsaKey:
	return lowRSAKey;
    case dsaKey:
	return lowDSAKey;
    case dhKey:
	return lowDHKey;
    default:
	break;
    }
    return lowNullKey;
}

typedef struct {
    CERTCertificate *cert;
    SECItem *pubKey;
} find_cert_callback_arg;

static SECStatus
find_cert_by_pub_key(CERTCertificate *cert, SECItem *k, void *arg)
{
    find_cert_callback_arg *cbarg;
    SECKEYPublicKey *pubKey = NULL;
    SECItem *pubItem;

    if((cert == NULL) || (arg == NULL)) {
	return SECFailure;
    }

    /* if this cert doesn't look like a user cert, we aren't interested */
    if (!((cert->isperm) && (cert->trust) && 
    	  (( cert->trust->sslFlags & CERTDB_USER ) ||
	   ( cert->trust->emailFlags & CERTDB_USER ) ||
	   ( cert->trust->objectSigningFlags & CERTDB_USER )) &&
					  ( cert->nickname != NULL ) ) ) {
	goto done;
    }

    /* get cert's public key */
    pubKey = CERT_ExtractPublicKey(cert);
    if ( pubKey == NULL ) {
	goto done;
    }
    /* pk11_GetPubItem returns data associated with the public key.
     * one only needs to free the public key. This comment is here
     * because this sematic would be non-obvious otherwise. All callers
     * should include this comment.
     */
    pubItem = pk11_GetPubItem(pubKey);
    if (pubItem == NULL) goto done;
    
    cbarg = (find_cert_callback_arg *)arg;

    if(SECITEM_CompareItem(pubItem, cbarg->pubKey) == SECEqual) {
	cbarg->cert = CERT_DupCertificate(cert);
	return SECFailure;
    }

done:
    if ( pubKey ) {
	SECKEY_DestroyPublicKey(pubKey);
    }

    return (SECSuccess);
}

static PK11Object *pk11_importCertificate(PK11Slot *slot,CERTCertificate *cert,
		 unsigned char *data, unsigned int size, PRBool needCert);
/* 
 * find a cert associated with the key and load it.
 */
static SECStatus
reload_existing_certificate(PK11Object *privKeyObject,SECItem *pubKey)
{
    find_cert_callback_arg cbarg;
    SECItem nickName;
    CERTCertificate *cert = NULL;
    CK_RV crv;
    SECStatus rv;
		    
    cbarg.pubKey = pubKey;
    cbarg.cert = NULL;
    SEC_TraversePermCerts(CERT_GetDefaultCertDB(),
				       find_cert_by_pub_key, (void *)&cbarg);
    if (cbarg.cert != NULL) {
	CERTCertificate *cert = NULL;
	/* can anyone tell me why this is call is necessary? rjr */
	cert = CERT_FindCertByDERCert(CERT_GetDefaultCertDB(),
							&cbarg.cert->derCert);

	/* does the certificate in the database have a 
	 * nickname?  if not, it probably was inserted 
	 * through SMIME and a nickname needs to be 
	 * set.
	 */
	if (cert && !cert->nickname) {
	    crv=pk11_Attribute2SecItem(NULL,&nickName,privKeyObject,CKA_LABEL);
	    if (crv != CKR_OK) {
		goto loser;
	    }
	    rv = CERT_AddPermNickname(cert, (char *)nickName.data);
	    SECITEM_ZfreeItem(&nickName, PR_FALSE);
	    if (rv != SECSuccess) {
		goto loser;
	    }
	}

	/* associate the certificate with the key */
	pk11_importCertificate(privKeyObject->slot, cert, pubKey->data, 
				pubKey->len, PR_FALSE);
    }

    return SECSuccess;
loser:
    if (cert) CERT_DestroyCertificate(cert);
    if (cbarg.cert) CERT_DestroyCertificate(cbarg.cert);
    return SECFailure;
}

static SECKEYLowPrivateKey * pk11_mkPrivKey(PK11Object *object,CK_KEY_TYPE key);
/*
 * check the consistancy and initialize a Private Key Object 
 */
static CK_RV
pk11_handlePrivateKeyObject(PK11Object *object,CK_KEY_TYPE key_type)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL encrypt = CK_TRUE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL wrap = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    SECItem mod;
    CK_RV crv;

    switch (key_type) {
    case CKK_RSA:
	if ( !pk11_hasAttribute(object, CKA_MODULUS)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_PUBLIC_EXPONENT)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_PRIVATE_EXPONENT)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_PRIME_1)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_PRIME_2)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_EXPONENT_1)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_EXPONENT_2)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_COEFFICIENT)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	/* make sure Netscape DB attribute is set correctly */
	crv = pk11_Attribute2SSecItem(NULL, &mod, object, CKA_MODULUS);
	if (crv != CKR_OK) return crv;
	crv = pk11_forceAttribute(object, CKA_NETSCAPE_DB, 
						pk11_item_expand(&mod));
	if (mod.data) PORT_Free(mod.data);
	if (crv != CKR_OK) return crv;
	
	break;
    case CKK_DSA:
	if ( !pk11_hasAttribute(object, CKA_SUBPRIME)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_NETSCAPE_DB)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
    case CKK_DH:
	if ( !pk11_hasAttribute(object, CKA_PRIME)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_BASE)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_VALUE)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	encrypt = CK_FALSE;
	recover = CK_FALSE;
	wrap = CK_FALSE;
	break;
    default:
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    crv = pk11_defaultAttribute(object,CKA_SUBJECT,NULL,0);
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_SENSITIVE,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_EXTRACTABLE,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_DECRYPT,&encrypt,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_SIGN,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_SIGN_RECOVER,&recover,
							     sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_UNWRAP,&wrap,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    /* the next two bits get modified only in the key gen and token cases */
    crv = pk11_forceAttribute(object,CKA_ALWAYS_SENSITIVE,
						&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_forceAttribute(object,CKA_NEVER_EXTRACTABLE,
						&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 

    if (pk11_isTrue(object,CKA_TOKEN)) {
	SECKEYLowPrivateKey *privKey;
	char *label;
	SECStatus rv = SECSuccess;
	SECItem pubKey;

	privKey=pk11_mkPrivKey(object,key_type);
	if (privKey == NULL) return CKR_HOST_MEMORY;
	label = object->label = pk11_getString(object,CKA_LABEL);

	crv = pk11_Attribute2SecItem(NULL,&pubKey,object,CKA_NETSCAPE_DB);
	if (crv == CKR_OK) {
	    rv = SECKEY_StoreKeyByPublicKey(SECKEY_GetDefaultKeyDB(),
			privKey, &pubKey, label,
			(SECKEYLowGetPasswordKey) pk11_givePass, object->slot);

	    /* check for the existance of an existing certificate and activate
	     * it if necessary */
	    if (rv == SECSuccess) {
	        reload_existing_certificate(object,&pubKey);
	    }

	    if (pubKey.data) PORT_Free(pubKey.data);
	} else {
	    rv = SECFailure;
	}

	SECKEY_LowDestroyPrivateKey(privKey);
	if (rv != SECSuccess) return CKR_DEVICE_ERROR;
	object->inDB = PR_TRUE;
        object->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    } else {
	object->objectInfo = pk11_mkPrivKey(object,key_type);
	if (object->objectInfo == NULL) return CKR_HOST_MEMORY;
	object->infoFree = (PK11Free) SECKEY_LowDestroyPrivateKey;
    }
    /* now NULL out the sensitive attributes */
    if (pk11_isTrue(object,CKA_SENSITIVE)) {
	pk11_nullAttribute(object,CKA_PRIVATE_EXPONENT);
	pk11_nullAttribute(object,CKA_PRIME_1);
	pk11_nullAttribute(object,CKA_PRIME_2);
	pk11_nullAttribute(object,CKA_EXPONENT_1);
	pk11_nullAttribute(object,CKA_EXPONENT_2);
	pk11_nullAttribute(object,CKA_COEFFICIENT);
    }
    return CKR_OK;
}

/* forward delcare the DES formating function for handleSecretKey */
void pk11_FormatDESKey(unsigned char *key, int length);
static SECKEYLowPrivateKey *pk11_mkSecretKeyRep(PK11Object *object);

/* Validate secret key data, and set defaults */
static CK_RV
validateSecretKey(PK11Object *object, CK_KEY_TYPE key_type, PRBool isFIPS)
{
    CK_RV crv;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    PK11Attribute *attribute = NULL;
    crv = pk11_defaultAttribute(object,CKA_SENSITIVE,
				isFIPS?&cktrue:&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_EXTRACTABLE,
						&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_ENCRYPT,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_DECRYPT,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_SIGN,&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_VERIFY,&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_WRAP,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_UNWRAP,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 

    if ( !pk11_hasAttribute(object, CKA_VALUE)) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    /* the next two bits get modified only in the key gen and token cases */
    crv = pk11_forceAttribute(object,CKA_ALWAYS_SENSITIVE,
						&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_forceAttribute(object,CKA_NEVER_EXTRACTABLE,
						&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 

    /* some types of keys have a value length */
    crv = CKR_OK;
    switch (key_type) {
    /* force CKA_VALUE_LEN to be set */
    case CKK_GENERIC_SECRET:
    case CKK_RC2:
    case CKK_RC4:
#if NSS_SOFTOKEN_DOES_RC5
    case CKK_RC5:
#endif
    case CKK_CAST:
    case CKK_CAST3:
    case CKK_CAST5:
	attribute = pk11_FindAttribute(object,CKA_VALUE);
	/* shouldn't happen */
	if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;
	crv = pk11_forceAttribute(object, CKA_VALUE_LEN, 
			&attribute->attrib.ulValueLen, sizeof(CK_ULONG));
	pk11_FreeAttribute(attribute);
	break;
    /* force the value to have the correct parity */
    case CKK_DES:
    case CKK_DES2:
    case CKK_DES3:
    case CKK_CDMF:
	attribute = pk11_FindAttribute(object,CKA_VALUE);
	/* shouldn't happen */
	if (attribute == NULL) return CKR_TEMPLATE_INCOMPLETE;
	pk11_FormatDESKey((unsigned char*)attribute->attrib.pValue,
						 attribute->attrib.ulValueLen);
	pk11_FreeAttribute(attribute);
	break;
    default:
	break;
    }

    return crv;
}
/*
 * check the consistancy and initialize a Secret Key Object 
 */
static CK_RV
pk11_handleSecretKeyObject(PK11Object *object,CK_KEY_TYPE key_type,
								PRBool isFIPS)
{
    CK_RV crv;
    SECKEYLowPrivateKey *privKey = NULL;
    SECItem pubKey;

    pubKey.data = 0;

    /* First validate and set defaults */
    crv = validateSecretKey(object, key_type, isFIPS);
    if (crv != CKR_OK) goto loser;

    /* If the object is a TOKEN object, store in the database */
    if (pk11_isTrue(object,CKA_TOKEN)) {
	char *label;
	SECStatus rv = SECSuccess;

	privKey=pk11_mkSecretKeyRep(object);
	if (privKey == NULL) return CKR_HOST_MEMORY;

	label = object->label = pk11_getString(object,CKA_LABEL);

	crv = pk11_Attribute2SecItem(NULL,&pubKey,object,CKA_ID);  /* Should this be ID? */
	if (crv != CKR_OK) goto loser;

	rv = SECKEY_StoreKeyByPublicKey(SECKEY_GetDefaultKeyDB(),
			privKey, &pubKey, label,
			(SECKEYLowGetPasswordKey) pk11_givePass, object->slot);
	if (rv != SECSuccess) {
	    crv = CKR_DEVICE_ERROR;
	}

	object->inDB = PR_TRUE;
        object->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    }

loser:
    if (privKey) SECKEY_LowDestroyPrivateKey(privKey);
    if (pubKey.data) PORT_Free(pubKey.data);

    return crv;
}

/*
 * check the consistancy and initialize a Key Object 
 */
static CK_RV
pk11_handleKeyObject(PK11Session *session, PK11Object *object)
{
    PK11Attribute *attribute;
    CK_KEY_TYPE key_type;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_RV crv;

    /* verify the required fields */
    if ( !pk11_hasAttribute(object,CKA_KEY_TYPE) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    /* now verify the common fields */
    crv = pk11_defaultAttribute(object,CKA_ID,NULL,0);
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_START_DATE,NULL,0);
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_END_DATE,NULL,0);
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_DERIVE,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_LOCAL,&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 

    /* get the key type */
    attribute = pk11_FindAttribute(object,CKA_KEY_TYPE);
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    pk11_FreeAttribute(attribute);

    switch (object->objclass) {
    case CKO_PUBLIC_KEY:
	return pk11_handlePublicKeyObject(object,key_type);
    case CKO_PRIVATE_KEY:
	return pk11_handlePrivateKeyObject(object,key_type);
    case CKO_SECRET_KEY:
	/* make sure the required fields exist */
	return pk11_handleSecretKeyObject(object,key_type,
			     (PRBool)(session->slot->slotID == FIPS_SLOT_ID));
    default:
	break;
    }
    return CKR_ATTRIBUTE_VALUE_INVALID;
}

/* 
 * Handle Object does all the object consistancy checks, automatic attribute
 * generation, attribute defaulting, etc. If handleObject succeeds, the object
 * will be assigned an object handle, and the object pointer will be adopted
 * by the session. (that is don't free object).
 */
CK_RV
pk11_handleObject(PK11Object *object, PK11Session *session)
{
    PK11Slot *slot = session->slot;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_BBOOL cktrue = CK_TRUE;
    PK11Attribute *attribute;
    CK_RV crv;

    /* make sure all the base object types are defined. If not set the
     * defaults */
    crv = pk11_defaultAttribute(object,CKA_TOKEN,&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK) return crv;
    crv = pk11_defaultAttribute(object,CKA_PRIVATE,&ckfalse,sizeof(CK_BBOOL));
    if (crv != CKR_OK) return crv;
    crv = pk11_defaultAttribute(object,CKA_LABEL,NULL,0);
    if (crv != CKR_OK) return crv;
    crv = pk11_defaultAttribute(object,CKA_MODIFIABLE,&cktrue,sizeof(CK_BBOOL));
    if (crv != CKR_OK) return crv;

    /* don't create a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
				(pk11_isTrue(object,CKA_PRIVATE))) {
	return CKR_USER_NOT_LOGGED_IN;
    }


    if (((session->info.flags & CKF_RW_SESSION) == 0) &&
				(pk11_isTrue(object,CKA_TOKEN))) {
	return CKR_SESSION_READ_ONLY;
    }

    if (pk11_isTrue(object, CKA_TOKEN)) {
	if (slot->DB_loaded == PR_FALSE) {
	    /* we are creating a token object, make sure we load the database
	     * first so we don't get duplicates....
	     * ... NOTE: This assumes we are logged in as well!
	     */
	    pk11_importKeyDB(slot);
	    slot->DB_loaded = PR_TRUE;
	}
    }
	
    /* PKCS #11 object ID's are unique for all objects on a
     * token */
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    object->handle = slot->tokenIDCount++;
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)

    /* get the object class */
    attribute = pk11_FindAttribute(object,CKA_CLASS);
    if (attribute == NULL) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    object->objclass = *(CK_OBJECT_CLASS *)attribute->attrib.pValue;
    pk11_FreeAttribute(attribute);

    /* now handle the specific. Get a session handle for these functions
     * to use */
    switch (object->objclass) {
    case CKO_DATA:
	crv = pk11_handleDataObject(session,object);
    case CKO_CERTIFICATE:
	crv = pk11_handleCertObject(session,object);
	break;
    case CKO_PRIVATE_KEY:
    case CKO_PUBLIC_KEY:
    case CKO_SECRET_KEY:
	crv = pk11_handleKeyObject(session,object);
	break;
    default:
	crv = CKR_ATTRIBUTE_VALUE_INVALID;
	break;
    }

    /* can't fail from here on out unless the pk_handlXXX functions have
     * failed the request */
    if (crv != CKR_OK) {
	return crv;
    }

    /* now link the object into the slot and session structures */
    object->slot = slot;
    pk11_AddObject(session,object);

    return CKR_OK;
}

/* import a private key as an object. We don't call handle object.
 * because we the private key came from the key DB and we don't want to
 * write back out again */
static PK11Object *
pk11_importPrivateKey(PK11Slot *slot,SECKEYLowPrivateKey *lowPriv,
							SECItem *dbKey)
{
    PK11Object *privateKey;
    CK_KEY_TYPE key_type;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_BBOOL sign = CK_TRUE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL decrypt = CK_TRUE;
    CK_BBOOL derive = CK_FALSE;
    CK_RV crv = CKR_OK;
    CK_OBJECT_CLASS privClass = CKO_PRIVATE_KEY;
    unsigned char cka_id[SHA1_LENGTH];

    /*
     * now lets create an object to hang the attributes off of
     */
    privateKey = pk11_NewObject(slot); /* fill in the handle later */
    if (privateKey == NULL) {
	pk11_FreeObject(privateKey);
	return NULL;
    }

    /* Netscape Private Attribute for dealing with database storeage */	
    if (pk11_AddAttributeType(privateKey, CKA_NETSCAPE_DB,
					pk11_item_expand(dbKey)) ) {
	 pk11_FreeObject(privateKey);
	 return NULL;
    }

    /* now force the CKA_ID */
    SHA1_HashBuf(cka_id, (unsigned char *)dbKey->data, (uint32)dbKey->len);
    if (pk11_AddAttributeType(privateKey, CKA_ID, cka_id, sizeof(cka_id))) {
	 pk11_FreeObject(privateKey);
	 return NULL;
    }


    /* Fill in the common Default values */
    if (pk11_AddAttributeType(privateKey,CKA_CLASS, &privClass,
					sizeof(CK_OBJECT_CLASS)) != CKR_OK) {
	 pk11_FreeObject(privateKey);
	 return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_TOKEN, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_PRIVATE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_MODIFIABLE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_LABEL, NULL, 0) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_START_DATE, NULL, 0) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_END_DATE, NULL, 0) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_DERIVE, &ckfalse,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    /* local: well we really don't know for sure... it could have been an 
     * imported key, but it's not a useful attribute anyway. */
    if (pk11_AddAttributeType(privateKey,CKA_LOCAL, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_SUBJECT, NULL, 0) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_SENSITIVE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_EXTRACTABLE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    /* is this really true? Maybe we should just say false here? */
    if (pk11_AddAttributeType(privateKey,CKA_ALWAYS_SENSITIVE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_NEVER_EXTRACTABLE, &ckfalse,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }

    /* Now Set up the parameters to generate the key (based on mechanism) */
    /* NOTE: for safety sake we *DO NOT* remember critical attributes. PKCS #11
     * will look them up again from the database when it needs them.
     */
    switch (lowPriv->keyType) {
    case lowRSAKey:
	/* format the keys */
	key_type = CKK_RSA;
	sign = CK_TRUE;
	recover = CK_TRUE;
	decrypt = CK_TRUE;
	derive = CK_FALSE;
        /* now fill in the RSA dependent parameters in the public key */
        crv = pk11_AddAttributeType(privateKey,CKA_MODULUS,
			   pk11_item_expand(&lowPriv->u.rsa.modulus));
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_PRIVATE_EXPONENT,NULL,0);
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_PUBLIC_EXPONENT,
			   pk11_item_expand(&lowPriv->u.rsa.publicExponent));
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_PRIME_1,NULL,0);
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_PRIME_2,NULL,0);
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_EXPONENT_1,NULL,0);
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_EXPONENT_2,NULL,0);
	if (crv != CKR_OK)  break;
        crv = pk11_AddAttributeType(privateKey,CKA_COEFFICIENT,NULL,0);
	break;
    case lowDSAKey:
	key_type = CKK_DSA;
	sign = CK_TRUE;
	recover = CK_FALSE;
	decrypt = CK_FALSE;
	derive = CK_FALSE;
	crv = pk11_AddAttributeType(privateKey,CKA_PRIME,
			   pk11_item_expand(&lowPriv->u.dsa.params.prime));
	if (crv != CKR_OK) break;
	crv = pk11_AddAttributeType(privateKey,CKA_SUBPRIME,
			   pk11_item_expand(&lowPriv->u.dsa.params.subPrime));
	if (crv != CKR_OK) break;
	crv = pk11_AddAttributeType(privateKey,CKA_BASE,
			   pk11_item_expand(&lowPriv->u.dsa.params.base));
	if (crv != CKR_OK) break;
	crv = pk11_AddAttributeType(privateKey,CKA_VALUE,NULL,0);
	if (crv != CKR_OK) break;
	break;
    case lowDHKey:
	key_type = CKK_DH;
	sign = CK_FALSE;
	decrypt = CK_FALSE;
	recover = CK_FALSE;
	derive = CK_TRUE;
	crv = CKR_MECHANISM_INVALID;
	break;
    default:
	crv = CKR_MECHANISM_INVALID;
    }

    if (crv != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }


    if (pk11_AddAttributeType(privateKey,CKA_SIGN, &sign,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_SIGN_RECOVER, &recover,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_DECRYPT, &decrypt,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_UNWRAP, &decrypt,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_DERIVE, &derive,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	pk11_FreeObject(privateKey);
	return NULL;
    }
    if (pk11_AddAttributeType(privateKey,CKA_KEY_TYPE,&key_type,
					     sizeof(CK_KEY_TYPE)) != CKR_OK) {
	 pk11_FreeObject(privateKey);
	 return NULL;
    }
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    privateKey->handle = slot->tokenIDCount++;
    privateKey->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
    privateKey->objclass = privClass;
    privateKey->slot = slot;
    privateKey->inDB = PR_TRUE;

    return privateKey;
}

/* import a private key or cert as a public key object.*/
static PK11Object *
pk11_importPublicKey(PK11Slot *slot,
	SECKEYLowPrivateKey *lowPriv, CERTCertificate *cert, SECItem *dbKey)
{
    PK11Object *publicKey = NULL;
    CK_KEY_TYPE key_type;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_BBOOL verify = CK_TRUE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL encrypt = CK_TRUE;
    CK_BBOOL derive = CK_FALSE;
    CK_RV crv = CKR_OK;
    CK_OBJECT_CLASS pubClass = CKO_PUBLIC_KEY;
    unsigned char cka_id[SHA1_LENGTH];
    LowKeyType keyType = nullKey;
    SECKEYPublicKey *pubKey = NULL;
    CK_ATTRIBUTE theTemplate[2];
    PK11ObjectListElement *objectList = NULL;

    if (lowPriv == NULL) {
	pubKey = CERT_ExtractPublicKey(cert);
	if (pubKey == NULL) {
	    goto failed;
	}
	/* pk11_GetPubItem returns data associated with the public key.
	 * one only needs to free the public key. This comment is here
	 * because this sematic would be non-obvious otherwise. All callers
	 * should include this comment.
	 */
	dbKey = pk11_GetPubItem(pubKey);
	if (dbKey == NULL) {
	    goto failed;
	}
    }
    SHA1_HashBuf(cka_id, (unsigned char *)dbKey->data, (uint32)dbKey->len);
    theTemplate[0].type = CKA_ID;
    theTemplate[0].pValue = cka_id;
    theTemplate[0].ulValueLen = sizeof(cka_id);
    theTemplate[1].type = CKA_CLASS;
    theTemplate[1].pValue = &pubClass;
    theTemplate[1].ulValueLen = sizeof(CK_OBJECT_CLASS);
    crv = pk11_searchObjectList(&objectList,slot->tokObjects,
		slot->objectLock, theTemplate, 2, slot->isLoggedIn);
    if ((crv == CKR_OK) && (objectList != NULL)) {
	goto failed;
    }
    /*
     * now lets create an object to hang the attributes off of
     */
    publicKey = pk11_NewObject(slot); /* fill in the handle later */
    if (publicKey == NULL) {
	goto failed;
    }

    /* now force the CKA_ID */
    if (pk11_AddAttributeType(publicKey, CKA_ID, cka_id, sizeof(cka_id))) {
	goto failed;
    }

    /* Fill in the common Default values */
    if (pk11_AddAttributeType(publicKey,CKA_CLASS,&pubClass,
					sizeof(CK_OBJECT_CLASS)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_TOKEN, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_PRIVATE, &ckfalse,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_MODIFIABLE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_LABEL, NULL, 0) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_START_DATE, NULL, 0) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_END_DATE, NULL, 0) != CKR_OK) {
	goto failed;
    }
    /* local: well we really don't know for sure... it could have been an 
     * imported key, but it's not a useful attribute anyway. */
    if (pk11_AddAttributeType(publicKey,CKA_LOCAL, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_SUBJECT, NULL, 0) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_SENSITIVE, &ckfalse,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_EXTRACTABLE, &cktrue,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_ALWAYS_SENSITIVE, &ckfalse,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_NEVER_EXTRACTABLE, &ckfalse,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }

    /* Now Set up the parameters to generate the key (based on mechanism) */
    if (lowPriv == NULL) {

	keyType = seckeyLow_KeyType(pubKey->keyType); 
    } else {
	keyType = lowPriv->keyType;
    } 


    switch (keyType) {
    case lowRSAKey:
	/* format the keys */
	key_type = CKK_RSA;
	verify = CK_TRUE;
	recover = CK_TRUE;
	encrypt = CK_TRUE;
	derive = CK_FALSE;
        /* now fill in the RSA dependent parameters in the public key */
 	if (lowPriv) {
            crv = pk11_AddAttributeType(publicKey,CKA_MODULUS,
			   pk11_item_expand(&lowPriv->u.rsa.modulus));
	    if (crv != CKR_OK) break;
            crv = pk11_AddAttributeType(publicKey,CKA_PUBLIC_EXPONENT,
			   pk11_item_expand(&lowPriv->u.rsa.publicExponent));
	    if (crv != CKR_OK) break;
	} else {
            crv = pk11_AddAttributeType(publicKey,CKA_MODULUS,
			   pk11_item_expand(&pubKey->u.rsa.modulus));
	    if (crv != CKR_OK) break;
            crv = pk11_AddAttributeType(publicKey,CKA_PUBLIC_EXPONENT,
			   pk11_item_expand(&pubKey->u.rsa.publicExponent));
	    if (crv != CKR_OK) break;
	}
	break;
    case lowDSAKey:
	key_type = CKK_DSA;
	verify = CK_TRUE;
	recover = CK_FALSE;
	encrypt = CK_FALSE;
	derive = CK_FALSE;
 	if (lowPriv) {
	    crv = pk11_AddAttributeType(publicKey,CKA_PRIME,
			   pk11_item_expand(&lowPriv->u.dsa.params.prime));
	    if (crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(publicKey,CKA_SUBPRIME,
			   pk11_item_expand(&lowPriv->u.dsa.params.subPrime));
	    if (crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(publicKey,CKA_BASE,
			   pk11_item_expand(&lowPriv->u.dsa.params.base));
	    if (crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(publicKey,CKA_VALUE,
			   pk11_item_expand(&lowPriv->u.dsa.publicValue));
	    if (crv != CKR_OK) break;
	} else {
	    crv = pk11_AddAttributeType(publicKey,CKA_PRIME,
			   pk11_item_expand(&pubKey->u.dsa.params.prime));
	    if (crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(publicKey,CKA_SUBPRIME,
			   pk11_item_expand(&pubKey->u.dsa.params.subPrime));
	    if (crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(publicKey,CKA_BASE,
			   pk11_item_expand(&pubKey->u.dsa.params.base));
	    if (crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(publicKey,CKA_VALUE,
			   pk11_item_expand(&pubKey->u.dsa.publicValue));
	    if (crv != CKR_OK) break;
	}
	break;
    case lowDHKey:
	key_type = CKK_DH;
	verify = CK_FALSE;
	encrypt = CK_FALSE;
	recover = CK_FALSE;
	derive = CK_TRUE;
	crv = CKR_MECHANISM_INVALID;
	break;
    default:
	crv = CKR_MECHANISM_INVALID;
    }

    if (pubKey) {
	SECKEY_DestroyPublicKey(pubKey);
	pubKey = NULL;
    }

    if (crv != CKR_OK) {
	goto failed;
    }

    if (pk11_AddAttributeType(publicKey,CKA_VERIFY, &verify,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_VERIFY_RECOVER, &recover,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_ENCRYPT, &encrypt,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_WRAP, &encrypt,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_DERIVE, &derive,
					      sizeof(CK_BBOOL)) != CKR_OK) {
	goto failed;
    }
    if (pk11_AddAttributeType(publicKey,CKA_KEY_TYPE,&key_type,
					     sizeof(CK_KEY_TYPE)) != CKR_OK) {
	goto failed;
    }
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    publicKey->handle = slot->tokenIDCount++;
    publicKey->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PUB);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
    publicKey->objclass = pubClass;
    publicKey->slot = slot;
    publicKey->inDB = PR_FALSE; /* not really in the Database */

    return publicKey;
failed:
    if (pubKey) SECKEY_DestroyPublicKey(pubKey);
    if (publicKey) pk11_FreeObject(publicKey);
    return NULL;
}

/*
 * Question.. Why doesn't import Cert call pk11_handleObject, or
 * pk11 handleCertObject? Answer: because they will try to write
 * this cert back out to the Database, even though it is already in
 * the database.
 */
static PK11Object *
pk11_importCertificate(PK11Slot *slot, CERTCertificate *cert, 
		unsigned char *data, unsigned int size, PRBool needObject)
{
    PK11Object *certObject = NULL;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_CERTIFICATE_TYPE certType = CKC_X_509;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    unsigned char cka_id[SHA1_LENGTH];
    CK_ATTRIBUTE theTemplate;
    PK11ObjectListElement *objectList = NULL;
    CK_RV crv;


    /*
     * first make sure that no object for this cert already exists.
     */
    theTemplate.type = CKA_VALUE;
    theTemplate.pValue = cert->derCert.data;
    theTemplate.ulValueLen = cert->derCert.len;
    crv = pk11_searchObjectList(&objectList,slot->tokObjects,
		slot->objectLock, &theTemplate, 1, slot->isLoggedIn);
    if ((crv == CKR_OK) && (objectList != NULL)) {
	if (needObject) {
    	    pk11_ReferenceObject(objectList->object);
	    certObject = objectList->object;
	}
	pk11_FreeObjectList(objectList);
	return certObject;
    }

    /*
     * now lets create an object to hang the attributes off of
     */
    certObject = pk11_NewObject(slot); /* fill in the handle later */
    if (certObject == NULL) {
	return NULL;
    }

    /* First set the CKA_ID */
    if (data == NULL) {
	SECKEYPublicKey *pubKey = CERT_ExtractPublicKey(cert);
	SECItem *pubItem;
	if (pubKey == NULL) {
	    pk11_FreeObject(certObject);
	    return NULL;
	}
	/* pk11_GetPubItem returns data associated with the public key.
	 * one only needs to free the public key. This comment is here
	 * because this sematic would be non-obvious otherwise.
	 */
	pubItem =pk11_GetPubItem(pubKey);
	if (pubItem == NULL)  {
	    SECKEY_DestroyPublicKey(pubKey);
	    pk11_FreeObject(certObject);
	    return NULL;
	} 
    	SHA1_HashBuf(cka_id, (unsigned char *)pubItem->data, 
							(uint32)pubItem->len);
	SECKEY_DestroyPublicKey(pubKey);
    } else {
	SHA1_HashBuf(cka_id, (unsigned char *)data, (uint32)size);
    }
    if (pk11_AddAttributeType(certObject, CKA_ID, cka_id, sizeof(cka_id))) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }

    /* initalize the certificate attributes */
    if (pk11_AddAttributeType(certObject, CKA_CLASS, &certClass,
					 sizeof(CK_OBJECT_CLASS)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_TOKEN, &cktrue,
					       sizeof(CK_BBOOL)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_PRIVATE, &ckfalse,
					       sizeof(CK_BBOOL)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_LABEL, cert->nickname,
					PORT_Strlen(cert->nickname))
								!= CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_MODIFIABLE, &cktrue,
					       sizeof(CK_BBOOL)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_CERTIFICATE_TYPE,  &certType,
					sizeof(CK_CERTIFICATE_TYPE))!=CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_VALUE, 
			 	pk11_item_expand(&cert->derCert)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_ISSUER, 
			 	pk11_item_expand(&cert->derIssuer)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_SUBJECT, 
		 	pk11_item_expand(&cert->derSubject)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }
    if (pk11_AddAttributeType(certObject, CKA_SERIAL_NUMBER, 
		 	pk11_item_expand(&cert->serialNumber)) != CKR_OK) {
	 pk11_FreeObject(certObject);
	 return NULL;
    }

  
    certObject->objectInfo = CERT_DupCertificate(cert);
    certObject->infoFree = (PK11Free) CERT_DestroyCertificate;
    
    /* now just verify the required date fields */
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    certObject->handle = slot->tokenIDCount++;
    certObject->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_CERT);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
    certObject->objclass = certClass;
    certObject->slot = slot;
    certObject->inDB = PR_TRUE;
    pk11_AddSlotObject(slot, certObject);
    if (needObject) {
	pk11_ReferenceObject(certObject);
    } else {
	certObject = NULL;
    }

    return certObject;
}

/*
 * ******************** Public Key Utilities ***************************
 */
/* Generate a low public key structure from an object */
SECKEYLowPublicKey *pk11_GetPubKey(PK11Object *object,CK_KEY_TYPE key_type)
{
    SECKEYLowPublicKey *pubKey;
    PLArenaPool *arena;
    CK_RV crv;

    if (object->objclass != CKO_PUBLIC_KEY) {
	return NULL;
    }

    /* If we already have a key, use it */
    if (object->objectInfo) {
	return (SECKEYLowPublicKey *)object->objectInfo;
    }

    /* allocate the structure */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;

    pubKey = (SECKEYLowPublicKey *)
			PORT_ArenaAlloc(arena,sizeof(SECKEYLowPublicKey));
    if (pubKey == NULL) {
    	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }

    /* fill in the structure */
    pubKey->arena = arena;
    switch (key_type) {
    case CKK_RSA:
	pubKey->keyType = lowRSAKey;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.rsa.modulus,
							object,CKA_MODULUS);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.rsa.publicExponent,
						object,CKA_PUBLIC_EXPONENT);
	break;
    case CKK_DSA:
	pubKey->keyType = lowDSAKey;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dsa.params.prime,
							object,CKA_PRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dsa.params.subPrime,
							object,CKA_SUBPRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dsa.params.base,
							object,CKA_BASE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dsa.publicValue,
							object,CKA_VALUE);
	break;
    case CKK_DH:
	pubKey->keyType = lowDHKey;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dh.prime,
							object,CKA_PRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dh.base,
							object,CKA_BASE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.dsa.publicValue,
							object,CKA_VALUE);
	break;
    default:
	crv = CKR_KEY_TYPE_INCONSISTENT;
	break;
    }
    if (crv != CKR_OK) {
    	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }

    object->objectInfo = pubKey;
    object->infoFree = (PK11Free) SECKEY_LowDestroyPublicKey;
    return pubKey;
}

/* make a private key from a verified object */
static SECKEYLowPrivateKey *
pk11_mkPrivKey(PK11Object *object,CK_KEY_TYPE key_type)
{
    SECKEYLowPrivateKey *privKey;
    PLArenaPool *arena;
    CK_RV crv = CKR_OK;
    SECStatus rv;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;

    privKey = (SECKEYLowPrivateKey *)
			PORT_ArenaAlloc(arena,sizeof(SECKEYLowPrivateKey));
    if (privKey == NULL)  {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }

    /* in future this would be a switch on key_type */
    privKey->arena = arena;
    switch (key_type) {
    case CKK_RSA:
	privKey->keyType = lowRSAKey;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.modulus,
							object,CKA_MODULUS);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.publicExponent,object,
							CKA_PUBLIC_EXPONENT);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.privateExponent,object,
							CKA_PRIVATE_EXPONENT);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.prime1,object,
								CKA_PRIME_1);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.prime2,object,
								CKA_PRIME_2);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.exponent1,
						object, CKA_EXPONENT_1);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.exponent2,
							object, CKA_EXPONENT_2);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(arena,&privKey->u.rsa.coefficient,object,
							      CKA_COEFFICIENT);
	if (crv != CKR_OK) break;
        rv = DER_SetUInteger(privKey->arena, &privKey->u.rsa.version,
                          SEC_PRIVATE_KEY_VERSION);
	if (rv != SECSuccess) crv = CKR_HOST_MEMORY;
	break;

    case CKK_DSA:
	privKey->keyType = lowDSAKey;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dsa.params.prime,
							object,CKA_PRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dsa.params.subPrime,
							object,CKA_SUBPRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dsa.params.base,
							object,CKA_BASE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dsa.privateValue,
							object,CKA_VALUE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dsa.publicValue,
							object,CKA_NETSCAPE_DB);
	/* can't set the public value.... */
	break;

    case CKK_DH:
	privKey->keyType = lowDHKey;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.prime,
							object,CKA_PRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.base,
							object,CKA_BASE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.privateValue,
							object,CKA_VALUE);
	break;
    default:
	crv = CKR_KEY_TYPE_INCONSISTENT;
	break;
    }
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    return privKey;
}


/* Generate a low private key structure from an object */
SECKEYLowPrivateKey *
pk11_GetPrivKey(PK11Object *object,CK_KEY_TYPE key_type)
{
    SECKEYLowPrivateKey *priv = NULL;

    if (object->objclass != CKO_PRIVATE_KEY) {
	return NULL;
    }
    if (object->objectInfo) {
	return (SECKEYLowPrivateKey *)object->objectInfo;
    }

    if (pk11_isTrue(object,CKA_TOKEN)) {
	/* grab it from the data base */
	SECItem pubKey;
	CK_RV crv;

	/* KEYID is the public KEY for DSA and DH, and the MODULUS for
	 *  RSA */
	crv=pk11_Attribute2SecItem(NULL,&pubKey,object,CKA_NETSCAPE_DB);
	if (crv != CKR_OK) return NULL;
	
	priv=SECKEY_FindKeyByPublicKey(SECKEY_GetDefaultKeyDB(),&pubKey,
				       (SECKEYLowGetPasswordKey) pk11_givePass,
				       object->slot);
	if (!priv && pubKey.data[0] == 0) {
	    /* Because of legacy code issues, sometimes the public key has
	     * a '0' prepended to it, forcing it to be unsigned.  The database
	     * may not store that '0', so remove it and try again.
	     */
	    SECItem tmpPubKey;
	    tmpPubKey.data = pubKey.data + 1;
	    tmpPubKey.len = pubKey.len - 1;
	    priv=SECKEY_FindKeyByPublicKey(SECKEY_GetDefaultKeyDB(),&tmpPubKey,
				           (SECKEYLowGetPasswordKey) pk11_givePass,
				           object->slot);
	}
	if (pubKey.data) PORT_Free(pubKey.data);

	/* don't 'cache' DB private keys */
	return priv;
    } 

    priv = pk11_mkPrivKey(object, key_type);
    object->objectInfo = priv;
    object->infoFree = (PK11Free) SECKEY_LowDestroyPrivateKey;
    return priv;
}

/*
 **************************** Symetric Key utils ************************
 */
/*
 * set the DES key with parity bits correctly
 */
void
pk11_FormatDESKey(unsigned char *key, int length)
{
    int i;

    /* format the des key */
    for (i=0; i < length; i++) {
	key[i] = parityTable[key[i]>>1];
    }
}

/*
 * check a des key (des2 or des3 subkey) for weak keys.
 */
PRBool
pk11_CheckDESKey(unsigned char *key)
{
    int i;

    /* format the des key with parity  */
    pk11_FormatDESKey(key, 8);

    for (i=0; i < pk11_desWeakTableSize; i++) {
	if (PORT_Memcmp(key,pk11_desWeakTable[i],8) == 0) {
	    return PR_TRUE;
	}
    }
    return PR_FALSE;
}

/*
 * check if a des or triple des key is weak.
 */
PRBool
pk11_IsWeakKey(unsigned char *key,CK_KEY_TYPE key_type)
{

    switch(key_type) {
    case CKK_DES:
	return pk11_CheckDESKey(key);
    case CKM_DES2_KEY_GEN:
	if (pk11_CheckDESKey(key)) return PR_TRUE;
	return pk11_CheckDESKey(&key[8]);
    case CKM_DES3_KEY_GEN:
	if (pk11_CheckDESKey(key)) return PR_TRUE;
	if (pk11_CheckDESKey(&key[8])) return PR_TRUE;
	return pk11_CheckDESKey(&key[16]);
    default:
	break;
    }
    return PR_FALSE;
}


/* make a fake private key representing a symmetric key */
static SECKEYLowPrivateKey *
pk11_mkSecretKeyRep(PK11Object *object)
{
    SECKEYLowPrivateKey *privKey;
    PLArenaPool *arena = 0;
    CK_RV crv;
    SECStatus rv;
    static unsigned char derZero[1] = { 0 };

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) { crv = CKR_HOST_MEMORY; goto loser; }

    privKey = (SECKEYLowPrivateKey *)
			PORT_ArenaAlloc(arena,sizeof(SECKEYLowPrivateKey));
    if (privKey == NULL) { crv = CKR_HOST_MEMORY; goto loser; }

    privKey->arena = arena;

    /* Secret keys are represented in the database as "fake" RSA keys.  The RSA key
     * is marked as a secret key representation by setting the public exponent field
     * to 0, which is an invalid RSA exponent.  The other fields are set as follows:
     *   modulus - CKA_ID value for the secret key
     *   private exponent - CKA_VALUE (the key itself)
     *   coefficient - CKA_KEY_TYPE, which indicates what encryption algorithm
     *      is used for the key.
     *   all others - set to integer 0
     */
    privKey->keyType = lowRSAKey;

    /* The modulus is set to the key id of the symmetric key */
    crv=pk11_Attribute2SecItem(arena,&privKey->u.rsa.modulus,object,CKA_ID);
    if (crv != CKR_OK) goto loser;

    /* The public exponent is set to 0 length to indicate a special key */
    privKey->u.rsa.publicExponent.len = sizeof derZero;
    privKey->u.rsa.publicExponent.data = derZero;

    /* The private exponent is the actual key value */
    crv=pk11_Attribute2SecItem(arena,&privKey->u.rsa.privateExponent,object,CKA_VALUE);
    if (crv != CKR_OK) goto loser;

    /* All other fields empty - needs testing */
    privKey->u.rsa.prime1.len = sizeof derZero;
    privKey->u.rsa.prime1.data = derZero;

    privKey->u.rsa.prime2.len = sizeof derZero;
    privKey->u.rsa.prime2.data = derZero;

    privKey->u.rsa.exponent1.len = sizeof derZero;
    privKey->u.rsa.exponent1.data = derZero;

    privKey->u.rsa.exponent2.len = sizeof derZero;
    privKey->u.rsa.exponent2.data = derZero;

    /* Coeficient set to KEY_TYPE */
    crv=pk11_Attribute2SecItem(arena,&privKey->u.rsa.coefficient,object,CKA_KEY_TYPE);
    if (crv != CKR_OK) goto loser;

    /* Private key version field set normally for compatibility */
    rv = DER_SetUInteger(privKey->arena, &privKey->u.rsa.version,SEC_PRIVATE_KEY_VERSION);
    if (rv != SECSuccess) { crv = CKR_HOST_MEMORY; goto loser; }

loser:
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	privKey = 0;
    }

    return privKey;
}

static PRBool
isSecretKey(SECKEYLowPrivateKey *privKey)
{
  if (privKey->keyType == lowRSAKey && privKey->u.rsa.publicExponent.len == 1 &&
      privKey->u.rsa.publicExponent.data[0] == 0)
    return PR_TRUE;

  return PR_FALSE;
}

/* Import a Secret Key */
static PRBool
importSecretKey(PK11Slot *slot, SECKEYLowPrivateKey *priv)
{
    PK11Object *object;
    CK_OBJECT_CLASS secretClass = CKO_SECRET_KEY;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_KEY_TYPE key_type;

    /* Check for secret key representation, return if it isn't one */
    if (!isSecretKey(priv))
        return PR_FALSE;

    /*
     * now lets create an object to hang the attributes off of
     */
    object = pk11_NewObject(slot); /* fill in the handle later */
    if (object == NULL) {
	goto loser;
    }

    /* Set the ID value */
    if (pk11_AddAttributeType(object, CKA_ID, 
          priv->u.rsa.modulus.data, priv->u.rsa.modulus.len)) {
	 pk11_FreeObject(object);
	 goto loser;
    }

    /* initalize the object attributes */
    if (pk11_AddAttributeType(object, CKA_CLASS, &secretClass,
					 sizeof(secretClass)) != CKR_OK) {
	 pk11_FreeObject(object);
	 goto loser;
    }

    if (pk11_AddAttributeType(object, CKA_TOKEN, &cktrue,
					       sizeof(cktrue)) != CKR_OK) {
	 pk11_FreeObject(object);
	 goto loser;
    }

    if (pk11_AddAttributeType(object, CKA_PRIVATE, &ckfalse,
					       sizeof(CK_BBOOL)) != CKR_OK) {
	 pk11_FreeObject(object);
	 goto loser;
    }

    if (pk11_AddAttributeType(object, CKA_VALUE, 
			 	pk11_item_expand(&priv->u.rsa.privateExponent)) != CKR_OK) {
	 pk11_FreeObject(object);
	 goto loser;
    }

    if (pk11_AddAttributeType(object, CKA_KEY_TYPE, 
			 	pk11_item_expand(&priv->u.rsa.coefficient)) != CKR_OK) {
	 pk11_FreeObject(object);
	 goto loser;
    }
    key_type = *(CK_KEY_TYPE*)priv->u.rsa.coefficient.data;

    /* Validate and add default attributes */
    validateSecretKey(object, key_type, (PRBool)(slot->slotID == FIPS_SLOT_ID));

    /* now just verify the required date fields */
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    object->handle = slot->tokenIDCount++;
    object->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)

    object->objclass = secretClass;
    object->slot = slot;
    object->inDB = PR_TRUE;
    pk11_AddSlotObject(slot, object);

loser:
    return PR_TRUE;
}


/**********************************************************************
 *
 *     Start of PKCS 11 functions 
 *
 **********************************************************************/


/* return the function list */
CK_RV NSC_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{
    *pFunctionList = &pk11_funcList;
    return CKR_OK;
}

/*
 * initialize one of the slot structures. figure out which by the ID
 */
CK_RV
PK11_SlotInit(CK_SLOT_ID slotID, PRBool needLogin)
{
    int i;
    PK11Slot *slot = pk11_SlotFromID(slotID);
#ifdef PKCS11_USE_THREADS
    slot->sessionLock = PZ_NewLock(nssILockSession);
    if (slot->sessionLock == NULL) return CKR_HOST_MEMORY;
    slot->objectLock = PZ_NewLock(nssILockObject);
    if (slot->objectLock == NULL) return CKR_HOST_MEMORY;
#else
    slot->sessionLock = NULL;
    slot->objectLock = NULL;
#endif
    for(i=0; i < SESSION_HASH_SIZE; i++) {
	slot->head[i] = NULL;
    }
    for(i=0; i < TOKEN_OBJECT_HASH_SIZE; i++) {
	slot->tokObjects[i] = NULL;
    }
    slot->password = NULL;
    slot->hasTokens = PR_FALSE;
    slot->sessionIDCount = 1;
    slot->sessionCount = 0;
    slot->rwSessionCount = 0;
    slot->tokenIDCount = 1;
    slot->needLogin = PR_FALSE;
    slot->isLoggedIn = PR_FALSE;
    slot->ssoLoggedIn = PR_FALSE;
    slot->DB_loaded = PR_FALSE;
    slot->slotID = slotID;
    if (needLogin) {
	/* if the data base is initialized with a null password,remember that */
	slot->needLogin = (PRBool)!pk11_hasNullPassword(&slot->password);
    }
    return CKR_OK;
}


/*
 * common initialization routines between PKCS #11 and FIPS
 */
CK_RV PK11_LowInitialize(CK_VOID_PTR pReserved)
{
    CK_RV crv = CKR_OK;
    CK_C_INITIALIZE_ARGS *init_args = (CK_C_INITIALIZE_ARGS *) pReserved;

    /* NOTE:
     * we should be getting out mutexes from this list, not statically binding
     * them from NSPR. This should happen before we allow the internal to split
     * off from the rest on NSS.
     */

    /* initialize the key and cert db's */
    SECKEY_SetDefaultKeyDBAlg(SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC);

    if ((init_args && init_args->LibraryParameters)) {
	pk11_parameters paramStrings;
       
	crv = secmod_parseParameters
		((char *)init_args->LibraryParameters,&paramStrings);
	if (crv != CKR_OK) {
	    return crv;
	}
	    
	crv = pk11_DBInit(paramStrings.configdir,paramStrings.certPrefix,
	    paramStrings.keyPrefix,paramStrings.secmodName,				    paramStrings.readOnly,paramStrings.noCertDB,
	    paramStrings.noModDB, paramStrings.forceOpen);
	if (crv != CKR_OK) {
	    secmod_freeParams(&paramStrings);
	    return crv;
	}
	crv = pk11_configure(paramStrings.man, paramStrings.libdes, 
		paramStrings.tokdes,paramStrings.ptokdes,paramStrings.slotdes,
		paramStrings.pslotdes,paramStrings.fslotdes,
		paramStrings.fpslotdes, paramStrings.minPW, 
					paramStrings.pwRequired);
	secmod_freeParams(&paramStrings);
	if (crv != CKR_OK) {
	    return crv;
	}
    } else {
	return CKR_ARGUMENTS_BAD;
    }


    return CKR_OK;
}

/*
 * handle the SECMOD.db
 */
char **
NSC_ModuleDBFunc(unsigned long function,char *parameters, char *args)
{
    char *secmod;
    PRBool rw;
    static char *success="Success";

    secmod = secmod_getSecmodName(parameters,&rw);

    switch (function) {
    case SECMOD_MODULE_DB_FUNCTION_FIND:
	return secmod_ReadPermDB(secmod,parameters,rw);
    case SECMOD_MODULE_DB_FUNCTION_ADD:
	return (secmod_AddPermDB(secmod,args,rw) == SECSuccess) 
							? &success: NULL;
    case SECMOD_MODULE_DB_FUNCTION_DEL:
	return (secmod_DeletePermDB(secmod,args,rw) == SECSuccess) 
							? &success: NULL;
    }
    return NULL;
}


/* NSC_Initialize initializes the Cryptoki library. */
CK_RV NSC_Initialize(CK_VOID_PTR pReserved)
{
    static PRBool init = PR_FALSE;
    CK_RV crv = CKR_OK;

    crv = PK11_LowInitialize(pReserved);
    if (crv != CKR_OK) return crv;

    /* intialize all the slots */
    if (!init) {
	crv = PK11_SlotInit(NETSCAPE_SLOT_ID,PR_FALSE);
	if (crv != CKR_OK) return crv;
	crv = PK11_SlotInit(PRIVATE_KEY_SLOT_ID,PR_TRUE);
	init = PR_TRUE;
    }

    return crv;
}

/* NSC_Finalize indicates that an application is done with the 
 * Cryptoki library.*/
CK_RV NSC_Finalize (CK_VOID_PTR pReserved)
{
    return CKR_OK;
}


/* NSC_GetInfo returns general information about Cryptoki. */
CK_RV  NSC_GetInfo(CK_INFO_PTR pInfo)
{
    pInfo->cryptokiVersion.major = 2;
    pInfo->cryptokiVersion.minor = 1;
    PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
    pInfo->libraryVersion.major = 3;
    pInfo->libraryVersion.minor = 2;
    PORT_Memcpy(pInfo->libraryDescription,libraryDescription,32);
    pInfo->flags = 0;
    return CKR_OK;
}

/* NSC_GetSlotList obtains a list of slots in the system. */
CK_RV NSC_GetSlotList(CK_BBOOL tokenPresent,
	 		CK_SLOT_ID_PTR	pSlotList, CK_ULONG_PTR pulCount)
{
    *pulCount = 2;
    if (pSlotList != NULL) {
	pSlotList[0] = NETSCAPE_SLOT_ID;
	pSlotList[1] = PRIVATE_KEY_SLOT_ID;
    }
    return CKR_OK;
}
	
/* NSC_GetSlotInfo obtains information about a particular slot in the system. */
CK_RV NSC_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
    pInfo->firmwareVersion.major = 0;
    pInfo->firmwareVersion.minor = 0;
    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
	PORT_Memcpy(pInfo->slotDescription,slotDescription,64);
	pInfo->flags = CKF_TOKEN_PRESENT;
        pInfo->hardwareVersion.major = 3;
        pInfo->hardwareVersion.minor = 2;
	return CKR_OK;
    case PRIVATE_KEY_SLOT_ID:
	PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
	PORT_Memcpy(pInfo->slotDescription,privSlotDescription,64);
	pInfo->flags = CKF_TOKEN_PRESENT;
	/* ok we really should read it out of the keydb file. */
        pInfo->hardwareVersion.major = PRIVATE_KEY_DB_FILE_VERSION;
        pInfo->hardwareVersion.minor = 0;
	return CKR_OK;
    default:
	break;
    }
    return CKR_SLOT_ID_INVALID;
}

#define CKF_THREAD_SAFE 0x8000 /* for now */

/* NSC_GetTokenInfo obtains information about a particular token in 
 * the system. */
CK_RV NSC_GetTokenInfo(CK_SLOT_ID slotID,CK_TOKEN_INFO_PTR pInfo)
{
    PK11Slot *slot = pk11_SlotFromID(slotID);
    SECKEYKeyDBHandle *handle;

    if (slot == NULL) return CKR_SLOT_ID_INVALID;

    PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
    PORT_Memcpy(pInfo->model,"Libsec 4.0      ",16);
    PORT_Memcpy(pInfo->serialNumber,"0000000000000000",16);
    pInfo->ulMaxSessionCount = 0; /* arbitrarily large */
    pInfo->ulSessionCount = slot->sessionCount;
    pInfo->ulMaxRwSessionCount = 0; /* arbitarily large */
    pInfo->ulRwSessionCount = slot->rwSessionCount;
    pInfo->firmwareVersion.major = 0;
    pInfo->firmwareVersion.minor = 0;
    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	PORT_Memcpy(pInfo->label,tokDescription,32);
        pInfo->flags= CKF_RNG | CKF_WRITE_PROTECTED | CKF_THREAD_SAFE;
	pInfo->ulMaxPinLen = 0;
	pInfo->ulMinPinLen = 0;
	pInfo->ulTotalPublicMemory = 0;
	pInfo->ulFreePublicMemory = 0;
	pInfo->ulTotalPrivateMemory = 0;
	pInfo->ulFreePrivateMemory = 0;
	pInfo->hardwareVersion.major = 4;
	pInfo->hardwareVersion.minor = 0;
	return CKR_OK;
    case PRIVATE_KEY_SLOT_ID:
	PORT_Memcpy(pInfo->label,privTokDescription,32);
    	handle = SECKEY_GetDefaultKeyDB();
	/*
	 * we have three possible states which we may be in:
	 *   (1) No DB password has been initialized. This also means we
	 *   have no keys in the key db.
	 *   (2) Password initialized to NULL. This means we have keys, but
	 *   the user has chosen not use a password.
	 *   (3) Finally we have an initialized password whicn is not NULL, and
	 *   we will need to prompt for it.
	 */
	if (SECKEY_HasKeyDBPassword(handle) == SECFailure) {
	    pInfo->flags = CKF_THREAD_SAFE | CKF_LOGIN_REQUIRED;
	} else if (!slot->needLogin) {
	    pInfo->flags = CKF_THREAD_SAFE | CKF_USER_PIN_INITIALIZED;
	} else {
	    pInfo->flags = CKF_THREAD_SAFE | 
				CKF_LOGIN_REQUIRED | CKF_USER_PIN_INITIALIZED;
	}
	pInfo->ulMaxPinLen = PK11_MAX_PIN;
	pInfo->ulMinPinLen = 0;
	if (minimumPinLen > 0) {
	    pInfo->ulMinPinLen = (CK_ULONG)minimumPinLen;
	}
	pInfo->ulTotalPublicMemory = 1;
	pInfo->ulFreePublicMemory = 1;
	pInfo->ulTotalPrivateMemory = 1;
	pInfo->ulFreePrivateMemory = 1;
	pInfo->hardwareVersion.major = CERT_DB_FILE_VERSION;
	pInfo->hardwareVersion.minor = 0;
	return CKR_OK;
    default:
	break;
    }
    return CKR_SLOT_ID_INVALID;
}



/* NSC_GetMechanismList obtains a list of mechanism types 
 * supported by a token. */
CK_RV NSC_GetMechanismList(CK_SLOT_ID slotID,
	CK_MECHANISM_TYPE_PTR pMechanismList, CK_ULONG_PTR pulCount)
{
    int i;

    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	*pulCount = mechanismCount;
	if (pMechanismList != NULL) {
	    for (i=0; i < (int) mechanismCount; i++) {
		pMechanismList[i] = mechanisms[i].type;
	    }
	}
	return CKR_OK;
    case PRIVATE_KEY_SLOT_ID:
	*pulCount = 0;
	for (i=0; i < (int) mechanismCount; i++) {
	    if (mechanisms[i].privkey) {
		(*pulCount)++;
		if (pMechanismList != NULL) {
		    *pMechanismList++ = mechanisms[i].type;
		}
	    }
	}
	return CKR_OK;
    default:
	break;
    }
    return CKR_SLOT_ID_INVALID;
}


/* NSC_GetMechanismInfo obtains information about a particular mechanism 
 * possibly supported by a token. */
CK_RV NSC_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type,
    					CK_MECHANISM_INFO_PTR pInfo)
{
    PRBool isPrivateKey;
    int i;

    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	isPrivateKey = PR_FALSE;
	break;
    case PRIVATE_KEY_SLOT_ID:
	isPrivateKey = PR_TRUE;
	break;
    default:
	return CKR_SLOT_ID_INVALID;
    }
    for (i=0; i < (int) mechanismCount; i++) {
        if (type == mechanisms[i].type) {
	    if (isPrivateKey && !mechanisms[i].privkey) {
    		return CKR_MECHANISM_INVALID;
	    }
	    PORT_Memcpy(pInfo,&mechanisms[i].domestic,
						sizeof(CK_MECHANISM_INFO));
	    return CKR_OK;
	}
    }
    return CKR_MECHANISM_INVALID;
}

static SECStatus
pk11_TurnOffUser(CERTCertificate *cert, SECItem *k, void *arg)
{
   CERTCertTrust trust;
   SECStatus rv;

   rv = CERT_GetCertTrust(cert,&trust);
   if (rv == SECSuccess && ((trust.emailFlags & CERTDB_USER) ||
			    (trust.sslFlags & CERTDB_USER) ||
			    (trust.objectSigningFlags & CERTDB_USER))) {
	trust.emailFlags &= ~CERTDB_USER;
	trust.sslFlags &= ~CERTDB_USER;
	trust.objectSigningFlags &= ~CERTDB_USER;
	CERT_ChangeCertTrust(cert->dbhandle,cert,&trust);
   }
   return SECSuccess;
}

/* NSC_InitToken initializes a token. */
CK_RV NSC_InitToken(CK_SLOT_ID slotID,CK_CHAR_PTR pPin,
 				CK_ULONG ulPinLen,CK_CHAR_PTR pLabel) {
    PK11Slot *slot = pk11_SlotFromID(slotID);
    SECKEYKeyDBHandle *handle;
    CERTCertDBHandle *certHandle;
    SECStatus rv;
    int i;
    PK11Object *object;

    if (slot == NULL) return CKR_SLOT_ID_INVALID;

    /* don't initialize the database if we aren't talking to a token
     * that uses the key database.
     */
    if (slotID == NETSCAPE_SLOT_ID) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* first, delete all our loaded key and cert objects from our 
     * internal list. */
    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    for (i=0; i < TOKEN_OBJECT_HASH_SIZE; i++) {
	do {
	    object = slot->tokObjects[i];
	    /* hand deque */
	    /* this duplicates function of NSC_close session functions, but 
	     * because we know that we are freeing all the sessions, we can
	     * do more efficient processing */
	    if (object) {
		slot->tokObjects[i] = object->next;

		if (object->next) object->next->prev = NULL;
		object->next = object->prev = NULL;
	    }
	    if (object) pk11_FreeObject(object);
	} while (object != NULL);
    }
    slot->DB_loaded = PR_FALSE;
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)

    /* then clear out the key database */
    handle = SECKEY_GetDefaultKeyDB();
    if (handle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* what to do on an error here? */
    rv = SECKEY_ResetKeyDB(handle);

    /* finally  mark all the user certs as non-user certs */
    certHandle = CERT_GetDefaultCertDB();
    if (certHandle == NULL) return CKR_OK;

    SEC_TraversePermCerts(certHandle,pk11_TurnOffUser, NULL);

    return CKR_OK; /*is this the right function for not implemented*/
}


/* NSC_InitPIN initializes the normal user's PIN. */
CK_RV NSC_InitPIN(CK_SESSION_HANDLE hSession,
    					CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    PK11Session *sp;
    PK11Slot *slot;
    SECKEYKeyDBHandle *handle;
    SECItem *newPin;
    char newPinStr[256];
    SECStatus rv;

    
    sp = pk11_SessionFromHandle(hSession);
    if (sp == NULL) {
	return CKR_SESSION_HANDLE_INVALID;
    }

    if (sp->info.slotID == NETSCAPE_SLOT_ID) {
	pk11_FreeSession(sp);
	return CKR_PIN_LEN_RANGE;
    }

    /* should be an assert */
    if (!((sp->info.slotID == PRIVATE_KEY_SLOT_ID) || 
				(sp->info.slotID == FIPS_SLOT_ID)) ) {
	pk11_FreeSession(sp);
	return CKR_SESSION_HANDLE_INVALID;;
    }

    if (sp->info.state != CKS_RW_SO_FUNCTIONS) {
	pk11_FreeSession(sp);
	return CKR_USER_NOT_LOGGED_IN;
    }

    slot = pk11_SlotFromSession(sp);
    pk11_FreeSession(sp);

    /* make sure the pins aren't too long */
    if (ulPinLen > 255) {
	return CKR_PIN_LEN_RANGE;
    }

    handle = SECKEY_GetDefaultKeyDB();
    if (handle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }
    if (SECKEY_HasKeyDBPassword(handle) != SECFailure) {
	return CKR_DEVICE_ERROR;
    }

    /* convert to null terminated string */
    PORT_Memcpy(newPinStr,pPin,ulPinLen);
    newPinStr[ulPinLen] = 0; 

    /* build the hashed pins which we pass around */
    newPin = SECKEY_HashPassword(newPinStr,handle->global_salt);
    PORT_Memset(newPinStr,0,sizeof(newPinStr));

    /* change the data base */
    rv = SECKEY_SetKeyDBPassword(handle,newPin);

    /* Now update our local copy of the pin */
    if (rv == SECSuccess) {
	if (slot->password) {
    	    SECITEM_ZfreeItem(slot->password, PR_TRUE);
	}
	slot->password = newPin;
	if (ulPinLen == 0) slot->needLogin = PR_FALSE;
	return CKR_OK;
    }
    SECITEM_ZfreeItem(newPin, PR_TRUE);
    return CKR_PIN_INCORRECT;
}


/* NSC_SetPIN modifies the PIN of user that is currently logged in. */
/* NOTE: This is only valid for the PRIVATE_KEY_SLOT */
CK_RV NSC_SetPIN(CK_SESSION_HANDLE hSession, CK_CHAR_PTR pOldPin,
    CK_ULONG ulOldLen, CK_CHAR_PTR pNewPin, CK_ULONG ulNewLen)
{
    PK11Session *sp;
    PK11Slot *slot;
    SECKEYKeyDBHandle *handle;
    SECItem *newPin;
    SECItem *oldPin;
    char newPinStr[256],oldPinStr[256];
    SECStatus rv;

    
    sp = pk11_SessionFromHandle(hSession);
    if (sp == NULL) {
	return CKR_SESSION_HANDLE_INVALID;
    }

    if (sp->info.slotID == NETSCAPE_SLOT_ID) {
	pk11_FreeSession(sp);
	return CKR_PIN_LEN_RANGE;
    }

    /* should be an assert */
    /* should be an assert */
    if (!((sp->info.slotID == PRIVATE_KEY_SLOT_ID) || 
				(sp->info.slotID == FIPS_SLOT_ID)) ) {
	pk11_FreeSession(sp);
	return CKR_SESSION_HANDLE_INVALID;;
    }

    slot = pk11_SlotFromSession(sp);
    if (slot->needLogin && sp->info.state != CKS_RW_USER_FUNCTIONS) {
	pk11_FreeSession(sp);
	return CKR_USER_NOT_LOGGED_IN;
    }

    pk11_FreeSession(sp);

    /* make sure the pins aren't too long */
    if ((ulNewLen > 255) || (ulOldLen > 255)) {
	return CKR_PIN_LEN_RANGE;
    }


    handle = SECKEY_GetDefaultKeyDB();
    if (handle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* convert to null terminated string */
    PORT_Memcpy(newPinStr,pNewPin,ulNewLen);
    newPinStr[ulNewLen] = 0; 
    PORT_Memcpy(oldPinStr,pOldPin,ulOldLen);
    oldPinStr[ulOldLen] = 0; 

    /* build the hashed pins which we pass around */
    newPin = SECKEY_HashPassword(newPinStr,handle->global_salt);
    oldPin = SECKEY_HashPassword(oldPinStr,handle->global_salt);
    PORT_Memset(newPinStr,0,sizeof(newPinStr));
    PORT_Memset(oldPinStr,0,sizeof(oldPinStr));

    /* change the data base */
    rv = SECKEY_ChangeKeyDBPassword(handle,oldPin,newPin);

    /* Now update our local copy of the pin */
    SECITEM_ZfreeItem(oldPin, PR_TRUE);
    if (rv == SECSuccess) {
	if (slot->password) {
    	    SECITEM_ZfreeItem(slot->password, PR_TRUE);
	}
	slot->password = newPin;
	slot->needLogin = (PRBool)(ulNewLen != 0);
	return CKR_OK;
    }
    SECITEM_ZfreeItem(newPin, PR_TRUE);
    return CKR_PIN_INCORRECT;
}

/* NSC_OpenSession opens a session between an application and a token. */
CK_RV NSC_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
   CK_VOID_PTR pApplication,CK_NOTIFY Notify,CK_SESSION_HANDLE_PTR phSession)
{
    PK11Slot *slot;
    CK_SESSION_HANDLE sessionID;
    PK11Session *session;

    slot = pk11_SlotFromID(slotID);
    if (slot == NULL) return CKR_SLOT_ID_INVALID;

    /* new session (we only have serial sessions) */
    session = pk11_NewSession(slotID, Notify, pApplication,
						 flags | CKF_SERIAL_SESSION);
    if (session == NULL) return CKR_HOST_MEMORY;

    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
    sessionID = slot->sessionIDCount++;
    if (slotID == PRIVATE_KEY_SLOT_ID) {
	sessionID |= PK11_PRIVATE_KEY_FLAG;
    } else if (slotID == FIPS_SLOT_ID) {
	sessionID |= PK11_FIPS_FLAG;
    } else if (flags & CKF_RW_SESSION) {
	/* NETSCAPE_SLOT_ID is Read ONLY */
	session->info.flags &= ~CKF_RW_SESSION;
    }

    session->handle = sessionID;
    pk11_update_state(slot, session);
    pk11queue_add(session, sessionID, slot->head, SESSION_HASH_SIZE);
    slot->sessionCount++;
    if (session->info.flags & CKF_RW_SESSION) {
	slot->rwSessionCount++;
    }
    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)

    *phSession = sessionID;
    return CKR_OK;
}


/* NSC_CloseSession closes a session between an application and a token. */
CK_RV NSC_CloseSession(CK_SESSION_HANDLE hSession)
{
    PK11Slot *slot;
    PK11Session *session;
    SECItem *pw = NULL;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    slot = pk11_SlotFromSession(session);

    /* lock */
    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
    if (pk11queue_is_queued(session,hSession,slot->head,SESSION_HASH_SIZE)) {
	pk11queue_delete(session,hSession,slot->head,SESSION_HASH_SIZE);
	session->refCount--; /* can't go to zero while we hold the reference */
	slot->sessionCount--;
	if (session->info.flags & CKF_RW_SESSION) {
	    slot->rwSessionCount--;
	}
    }
    if (slot->sessionCount == 0) {
	pw = slot->password;
	slot->isLoggedIn = PR_FALSE;
	slot->password = NULL;
    }
    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)

    pk11_FreeSession(session);
    if (pw) SECITEM_ZfreeItem(pw, PR_TRUE);
    return CKR_OK;
}


/* NSC_CloseAllSessions closes all sessions with a token. */
CK_RV NSC_CloseAllSessions (CK_SLOT_ID slotID)
{
    PK11Slot *slot;
    SECItem *pw = NULL;
    PK11Session *session;
    int i;

    slot = pk11_SlotFromID(slotID);
    if (slot == NULL) return CKR_SLOT_ID_INVALID;

    /* first log out the card */
    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
    pw = slot->password;
    slot->isLoggedIn = PR_FALSE;
    slot->password = NULL;
    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)
    if (pw) SECITEM_ZfreeItem(pw, PR_TRUE);

    /* now close all the current sessions */
    /* NOTE: If you try to open new sessions before NSC_CloseAllSessions
     * completes, some of those new sessions may or may not be closed by
     * NSC_CloseAllSessions... but any session running when this code starts
     * will guarrenteed be close, and no session will be partially closed */
    for (i=0; i < SESSION_HASH_SIZE; i++) {
	do {
	    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
	    session = slot->head[i];
	    /* hand deque */
	    /* this duplicates function of NSC_close session functions, but 
	     * because we know that we are freeing all the sessions, we can
	     * do more efficient processing */
	    if (session) {
		slot->head[i] = session->next;
		if (session->next) session->next->prev = NULL;
		session->next = session->prev = NULL;
		slot->sessionCount--;
		if (session->info.flags & CKF_RW_SESSION) {
		    slot->rwSessionCount--;
		}
	    }
	    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)
	    if (session) pk11_FreeSession(session);
	} while (session != NULL);
    }
    return CKR_OK;
}


/* NSC_GetSessionInfo obtains information about the session. */
CK_RV NSC_GetSessionInfo(CK_SESSION_HANDLE hSession,
    						CK_SESSION_INFO_PTR pInfo)
{
    PK11Session *session;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;

    PORT_Memcpy(pInfo,&session->info,sizeof(CK_SESSION_INFO));
    pk11_FreeSession(session);
    return CKR_OK;
}

/* NSC_Login logs a user into a token. */
CK_RV NSC_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType,
				    CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    PK11Slot *slot;
    PK11Session *session;
    SECKEYKeyDBHandle *handle;
    SECItem *pin;
    char pinStr[256];


    /* get the slot */
    slot = pk11_SlotFromSessionHandle(hSession);

    /* make sure the session is valid */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    }
    pk11_FreeSession(session);

    /* can't log into the Netscape Slot */
    if (slot->slotID == NETSCAPE_SLOT_ID)
	 return CKR_USER_TYPE_INVALID;

    if (slot->isLoggedIn) return CKR_USER_ALREADY_LOGGED_IN;
    slot->ssoLoggedIn = PR_FALSE;

    if (ulPinLen > 255) return CKR_PIN_LEN_RANGE;

    /* convert to null terminated string */
    PORT_Memcpy(pinStr,pPin,ulPinLen);
    pinStr[ulPinLen] = 0; 

    handle = SECKEY_GetDefaultKeyDB();

    /*
     * Deal with bootstrap. We allow the SSO to login in with a NULL
     * password if and only if we haven't initialized the KEY DB yet.
     * We only allow this on a RW session.
     */
    if (SECKEY_HasKeyDBPassword(handle) == SECFailure) {
	/* allow SSO's to log in only if there is not password on the
	 * key database */
	if (((userType == CKU_SO) && (session->info.flags & CKF_RW_SESSION))
	    /* fips always needs to authenticate, even if there isn't a db */
					|| (slot->slotID == FIPS_SLOT_ID)) {
	    /* should this be a fixed password? */
	    if (ulPinLen == 0) {
		SECItem *pw;
    		PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
		pw = slot->password;
		slot->password = NULL;
		slot->isLoggedIn = PR_TRUE;
		slot->ssoLoggedIn = (PRBool)(userType == CKU_SO);
		PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)
		pk11_update_all_states(slot);
		SECITEM_ZfreeItem(pw,PR_TRUE);
		return CKR_OK;
	    }
	    return CKR_PIN_INCORRECT;
	} 
	return CKR_USER_TYPE_INVALID;
    } 

    /* don't allow the SSO to log in if the user is already initialized */
    if (userType != CKU_USER) { return CKR_USER_TYPE_INVALID; }


    /* build the hashed pins which we pass around */
    pin = SECKEY_HashPassword(pinStr,handle->global_salt);
    if (pin == NULL) return CKR_HOST_MEMORY;

    if (SECKEY_CheckKeyDBPassword(handle,pin) == SECSuccess) {
	SECItem *tmp;
	PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
	tmp = slot->password;
	slot->isLoggedIn = PR_TRUE;
	slot->password = pin;
	PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)
        if (tmp) SECITEM_ZfreeItem(tmp, PR_TRUE);

	/* update all sessions */
	pk11_update_all_states(slot);
	return CKR_OK;
    }

    SECITEM_ZfreeItem(pin, PR_TRUE);
    return CKR_PIN_INCORRECT;
}

/* NSC_Logout logs a user out from a token. */
CK_RV NSC_Logout(CK_SESSION_HANDLE hSession)
{
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    SECItem *pw = NULL;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    }

    if (!slot->isLoggedIn) return CKR_USER_NOT_LOGGED_IN;

    PK11_USE_THREADS(PZ_Lock(slot->sessionLock);)
    pw = slot->password;
    slot->isLoggedIn = PR_FALSE;
    slot->ssoLoggedIn = PR_FALSE;
    slot->password = NULL;
    PK11_USE_THREADS(PZ_Unlock(slot->sessionLock);)
    if (pw) SECITEM_ZfreeItem(pw, PR_TRUE);

    pk11_update_all_states(slot);
    return CKR_OK;
}


/* NSC_CreateObject creates a new object. */
CK_RV NSC_CreateObject(CK_SESSION_HANDLE hSession,
		CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, 
					CK_OBJECT_HANDLE_PTR phObject)
{
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    PK11Object *object;
    CK_RV crv;
    int i;


    /*
     * now lets create an object to hang the attributes off of
     */
    object = pk11_NewObject(slot); /* fill in the handle later */
    if (object == NULL) {
	return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i=0; i < (int) ulCount; i++) {
	crv = pk11_AddAttributeType(object,pk11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) {
	    pk11_FreeObject(object);
	    return crv;
	}
    }

    /* get the session */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	pk11_FreeObject(object);
        return CKR_SESSION_HANDLE_INVALID;
    }

    /*
     * handle the base object stuff
     */
    crv = pk11_handleObject(object,session);
    pk11_FreeSession(session);
    if (crv != CKR_OK) {
	pk11_FreeObject(object);
	return crv;
    }

    *phObject = object->handle;
    return CKR_OK;
}


/* NSC_CopyObject copies an object, creating a new object for the copy. */
CK_RV NSC_CopyObject(CK_SESSION_HANDLE hSession,
       CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
					CK_OBJECT_HANDLE_PTR phNewObject) 
{
    PK11Object *destObject,*srcObject;
    PK11Session *session;
    CK_RV crv = CKR_OK;
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    int i;

    /* Get srcObject so we can find the class */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    srcObject = pk11_ObjectFromHandle(hObject,session);
    if (srcObject == NULL) {
	pk11_FreeSession(session);
	return CKR_OBJECT_HANDLE_INVALID;
    }
    /*
     * create an object to hang the attributes off of
     */
    destObject = pk11_NewObject(slot); /* fill in the handle later */
    if (destObject == NULL) {
	pk11_FreeSession(session);
        pk11_FreeObject(srcObject);
	return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i=0; i < (int) ulCount; i++) {
	if (pk11_modifyType(pTemplate[i].type,srcObject->objclass) == PK11_NEVER) {
	    crv = CKR_ATTRIBUTE_READ_ONLY;
	    break;
	}
	crv = pk11_AddAttributeType(destObject,pk11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) { break; }
    }
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
        pk11_FreeObject(srcObject);
	pk11_FreeObject(destObject);
	return crv;
    }

    /* sensitive can only be changed to CK_TRUE */
    if (pk11_hasAttribute(destObject,CKA_SENSITIVE)) {
	if (!pk11_isTrue(destObject,CKA_SENSITIVE)) {
	    pk11_FreeSession(session);
            pk11_FreeObject(srcObject);
	    pk11_FreeObject(destObject);
	    return CKR_ATTRIBUTE_READ_ONLY;
	}
    }

    /*
     * now copy the old attributes from the new attributes
     */
    /* don't create a token object if we aren't in a rw session */
    /* we need to hold the lock to copy a consistant version of
     * the object. */
    crv = pk11_CopyObject(destObject,srcObject);

    destObject->objclass = srcObject->objclass;
    pk11_FreeObject(srcObject);
    if (crv != CKR_OK) {
	pk11_FreeObject(destObject);
	pk11_FreeSession(session);
    }

    crv = pk11_handleObject(destObject,session);
    *phNewObject = destObject->handle;
    pk11_FreeSession(session);
    if (crv != CKR_OK) {
	pk11_FreeObject(destObject);
	return crv;
    }
    
    return CKR_OK;
}


/* NSC_GetObjectSize gets the size of an object in bytes. */
CK_RV NSC_GetObjectSize(CK_SESSION_HANDLE hSession,
    			CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize) {
    *pulSize = 0;
    return CKR_OK;
}


/* NSC_GetAttributeValue obtains the value of one or more object attributes. */
CK_RV NSC_GetAttributeValue(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE hObject,CK_ATTRIBUTE_PTR pTemplate,CK_ULONG ulCount) {
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    PK11Object *object;
    PK11Attribute *attribute;
    PRBool sensitive;
    int i;

    /*
     * make sure we're allowed
     */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    object = pk11_ObjectFromHandle(hObject,session);
    pk11_FreeSession(session);
    if (object == NULL) {
	return CKR_OBJECT_HANDLE_INVALID;
    }

    /* don't read a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
				(pk11_isTrue(object,CKA_PRIVATE))) {
	pk11_FreeObject(object);
	return CKR_USER_NOT_LOGGED_IN;
    }

    sensitive = pk11_isTrue(object,CKA_SENSITIVE);
    for (i=0; i < (int) ulCount; i++) {
	/* Make sure that this attribute is retrievable */
	if (sensitive && pk11_isSensitive(pTemplate[i].type,object->objclass)) {
	    pk11_FreeObject(object);
	    return CKR_ATTRIBUTE_SENSITIVE;
	}
	attribute = pk11_FindAttribute(object,pTemplate[i].type);
	if (attribute == NULL) {
	    pk11_FreeObject(object);
	    return CKR_ATTRIBUTE_TYPE_INVALID;
	}
	if (pTemplate[i].pValue != NULL) {
	    PORT_Memcpy(pTemplate[i].pValue,attribute->attrib.pValue,
						attribute->attrib.ulValueLen);
	}
	pTemplate[i].ulValueLen = attribute->attrib.ulValueLen;
	pk11_FreeAttribute(attribute);
    }

    pk11_FreeObject(object);
    return CKR_OK;
}

/* NSC_SetAttributeValue modifies the value of one or more object attributes */
CK_RV NSC_SetAttributeValue (CK_SESSION_HANDLE hSession,
 CK_OBJECT_HANDLE hObject,CK_ATTRIBUTE_PTR pTemplate,CK_ULONG ulCount) {
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    PK11Attribute *attribute;
    PK11Object *object;
    PRBool isToken;
    CK_RV crv = CKR_OK;
    CK_BBOOL legal;
    int i;

    /*
     * make sure we're allowed
     */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    object = pk11_ObjectFromHandle(hObject,session);
    if (object == NULL) {
        pk11_FreeSession(session);
	return CKR_OBJECT_HANDLE_INVALID;
    }

    /* don't modify a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
				(pk11_isTrue(object,CKA_PRIVATE))) {
	pk11_FreeSession(session);
	pk11_FreeObject(object);
	return CKR_USER_NOT_LOGGED_IN;
    }

    /* don't modify a token object if we aren't in a rw session */
    isToken = pk11_isTrue(object,CKA_TOKEN);
    if (((session->info.flags & CKF_RW_SESSION) == 0) && isToken) {
	pk11_FreeSession(session);
	pk11_FreeObject(object);
	return CKR_SESSION_READ_ONLY;
    }
    pk11_FreeSession(session);

    /* only change modifiable objects */
    if (!pk11_isTrue(object,CKA_MODIFIABLE)) {
	pk11_FreeObject(object);
	return CKR_ATTRIBUTE_READ_ONLY;
    }

    for (i=0; i < (int) ulCount; i++) {
	/* Make sure that this attribute is changeable */
	switch (pk11_modifyType(pTemplate[i].type,object->objclass)) {
	case PK11_NEVER:
	case PK11_ONCOPY:
        default:
	    crv = CKR_ATTRIBUTE_READ_ONLY;
	    break;

        case PK11_SENSITIVE:
	    legal = (pTemplate[i].type == CKA_EXTRACTABLE) ? CK_FALSE : CK_TRUE;
	    if ((*(CK_BBOOL *)pTemplate[i].pValue) != legal) {
	        crv = CKR_ATTRIBUTE_READ_ONLY;
	    }
	    break;
        case PK11_ALWAYS:
	    break;
	}
	if (crv != CKR_OK) break;

	/* find the old attribute */
	attribute = pk11_FindAttribute(object,pTemplate[i].type);
	if (attribute == NULL) {
	    crv =CKR_ATTRIBUTE_TYPE_INVALID;
	    break;
	}
    	pk11_FreeAttribute(attribute);
	crv = pk11_forceAttribute(object,pk11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) break;

    }

    pk11_FreeObject(object);
    return CKR_OK;
}

/* stolen from keydb.c */
#define KEYDB_PW_CHECK_STRING   "password-check"
#define KEYDB_PW_CHECK_LEN      14

SECKEYLowPrivateKey * SECKEY_DecryptKey(DBT *key, SECItem *pwitem,
					SECKEYKeyDBHandle *handle);
typedef struct pk11keyNodeStr {
    struct pk11keyNodeStr *next;
    SECKEYLowPrivateKey *privKey;
    CERTCertificate *cert;
    SECItem *pubItem;
} pk11keyNode;

typedef struct {
    PLArenaPool *arena;
    pk11keyNode *head;
    PK11Slot *slot;
} keyList;

static SECStatus
add_key_to_list(DBT *key, DBT *data, void *arg)
{
    keyList *keylist;
    pk11keyNode *node;
    void *keydata;
    SECKEYLowPrivateKey *privKey = NULL;
    
    keylist = (keyList *)arg;

    privKey = SECKEY_DecryptKey(key, keylist->slot->password,
				SECKEY_GetDefaultKeyDB());
    if ( privKey == NULL ) {
	goto loser;
    }

    /* allocate the node struct */
    node = (pk11keyNode*)PORT_ArenaZAlloc(keylist->arena, sizeof(pk11keyNode));
    if ( node == NULL ) {
	goto loser;
    }
    
    /* allocate room for key data */
    keydata = PORT_ArenaZAlloc(keylist->arena, key->size);
    if ( keydata == NULL ) {
	goto loser;
    }

    /* link node into list */
    node->next = keylist->head;
    keylist->head = node;
    
    node->privKey = privKey;
    switch (privKey->keyType) {
      case lowRSAKey:
	node->pubItem = &privKey->u.rsa.modulus;
	break;
      case lowDSAKey:
	node->pubItem = &privKey->u.dsa.publicValue;
	break;
      default:
	break;
    }
    
    return(SECSuccess);
loser:
    if ( privKey ) {
	SECKEY_LowDestroyPrivateKey(privKey);
    }
    return(SECSuccess);
}

/*
 * If the cert is a user cert, then try to match it to a key on the
 * linked list of private keys built earlier.
 * If the cert matches one on the list, then save it.
 */
static SECStatus
add_cert_to_list(CERTCertificate *cert, SECItem *k, void *pdata)
{
    keyList *keylist;
    pk11keyNode *node;
    SECKEYPublicKey *pubKey = NULL;
    SECItem *pubItem;
    CERTCertificate *oldcert;
    
    keylist = (keyList *)pdata;
    
    /* only if it is a user cert and has a nickname!! */
    if ( ( ( cert->trust->sslFlags & CERTDB_USER ) ||
	  ( cert->trust->emailFlags & CERTDB_USER ) ||
	  ( cert->trust->objectSigningFlags & CERTDB_USER ) ) &&
	( cert->nickname != NULL ) ) {

	/* get cert's public key */
	pubKey = CERT_ExtractPublicKey(cert);
	if ( pubKey == NULL ) {
	    goto done;
	}

	/* pk11_GetPubItem returns data associated with the public key.
	 * one only needs to free the public key. This comment is here
	 * because this sematic would be non-obvious otherwise.
	 */
	pubItem = pk11_GetPubItem(pubKey);
	if (pubItem == NULL) goto done;

	node = keylist->head;
	while ( node ) {
	    /* if key type is different, then there is no match */
	    if (node->privKey->keyType == seckeyLow_KeyType(pubKey->keyType)) { 

		/* compare public value from cert with public value from
		 * the key
		 */
		if ( SECITEM_CompareItem(pubItem, node->pubItem) == SECEqual ){
		    /* this is a match */

		    /* if no cert has yet been found for this key, or this
		     * cert is newer, then save this cert
		     */
		    if ( ( node->cert == NULL ) || 
			CERT_IsNewer(cert, node->cert ) ) {

			oldcert = node->cert;
			
			/* get a real DB copy of the cert, since the one
			 * passed in here is not properly recorded in the
			 * temp database
			 */

			/* We need a better way to deal with this */
			node->cert =
			    CERT_FindCertByKeyNoLocking(CERT_GetDefaultCertDB(),
						        &cert->certKey);

			/* free the old cert if there was one */
			if ( oldcert ) {
			    CERT_DestroyCertificate(oldcert);
			}
		    }
		}
	    }
	    
	    node = node->next;
	}
    }
done:
    if ( pubKey ) {
	SECKEY_DestroyPublicKey(pubKey);
    }

    return(SECSuccess);
}

#if 0
/* This appears to be obsolete - TNH */
static SECItem *
decodeKeyDBGlobalSalt(DBT *saltData)
{
    SECItem *saltitem;
    
    saltitem = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if ( saltitem == NULL ) {
	return(NULL);
    }
    
    saltitem->data = (unsigned char *)PORT_ZAlloc(saltData->size);
    if ( saltitem->data == NULL ) {
	PORT_Free(saltitem);
	return(NULL);
    }
    
    saltitem->len = saltData->size;
    PORT_Memcpy(saltitem->data, saltData->data, saltitem->len);
    
    return(saltitem);
}
#endif

#if 0
/*
 * Create a (fixed) DES3 key [ testing ]
 */
static unsigned char keyValue[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
};

static SECItem keyItem = {
  0,
  keyValue,
  sizeof keyValue
};

static unsigned char keyID[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static SECItem keyIDItem = {
  0,
  keyID,
  sizeof keyID
};
/* +AAA */
static CK_RV
pk11_createFixedDES3Key(PK11Slot *slot)
{
    CK_RV rv = CKR_OK;
    PK11Object *keyObject;
    CK_BBOOL true = CK_TRUE;
    CK_OBJECT_CLASS class = CKO_SECRET_KEY;
    CK_KEY_TYPE keyType = CKK_DES3;

    /*
     * Create the object
     */
    keyObject = pk11_NewObject(slot); /* fill in the handle later */
    if (keyObject == NULL) {
	return CKR_HOST_MEMORY;
    }

    /* Add attributes to the object */
    rv = pk11_AddAttributeType(keyObject, CKA_ID, keyID, sizeof keyID);
    if (rv != CKR_OK) {
	 pk11_FreeObject(keyObject);
	 return rv;
    }

    rv = pk11_AddAttributeType(keyObject, CKA_VALUE, keyValue, sizeof keyValue);
    if (rv != CKR_OK) {
	 pk11_FreeObject(keyObject);
	 return rv;
    }

    rv = pk11_AddAttributeType(keyObject, CKA_TOKEN, &true, sizeof true);
    if (rv != CKR_OK) {
	 pk11_FreeObject(keyObject);
	 return rv;
    }

    rv = pk11_AddAttributeType(keyObject, CKA_CLASS, &class, sizeof class);
    if (rv != CKR_OK) {
	 pk11_FreeObject(keyObject);
	 return rv;
    }

    rv = pk11_AddAttributeType(keyObject, CKA_KEY_TYPE, &keyType, sizeof keyType);
    if (rv != CKR_OK) {
	 pk11_FreeObject(keyObject);
	 return rv;
    }

    pk11_handleSecretKeyObject(keyObject, keyType, PR_TRUE);

    PK11_USE_THREADS(PZ_Lock(slot->objectLock);)
    keyObject->handle = slot->tokenIDCount++;
    PK11_USE_THREADS(PZ_Unlock(slot->objectLock);)
    keyObject->slot = slot;
    keyObject->objclass = CKO_SECRET_KEY;
    pk11_AddSlotObject(slot, keyObject);

    return rv;
}
#endif /* Fixed DES key */

/*
 * load up our token database
 */
static CK_RV
pk11_importKeyDB(PK11Slot *slot)
{
    keyList keylist;
    pk11keyNode *node;
    CK_RV crv;
    SECStatus rv;
    PK11Object *privateKeyObject;
    PK11Object *publicKeyObject;
    PK11Object *certObject;

    /* traverse the database, collecting the index keys of all
     * records into a linked list
     */
    keylist.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( keylist.arena  == NULL ) {
	return CKR_HOST_MEMORY;
    }
    keylist.head = NULL;
    keylist.slot = slot;
	
    /* collect all of the keys */
    rv = SECKEY_TraverseKeys(SECKEY_GetDefaultKeyDB(),
			      add_key_to_list, (void *)&keylist);
    if (rv != SECSuccess) {
	PORT_FreeArena(keylist.arena, PR_FALSE);
	return CKR_HOST_MEMORY;
    }

    /* find certs that match any of the keys */
    crv = SEC_TraversePermCerts(CERT_GetDefaultCertDB(),
				       add_cert_to_list, (void *)&keylist);
    if ( crv != SECSuccess ) {
	PORT_FreeArena(keylist.arena, PR_FALSE);
	return CKR_HOST_MEMORY;
    }

    /* now traverse the list and entry certs/keys into the
     * pkcs11 world
     */
    for (node = keylist.head;  node != NULL; node=node->next ) {
        /* Check for "special" private key that wraps a symmetric key */
        if (isSecretKey(node->privKey)) {
          importSecretKey(slot, node->privKey);
          goto end_loop;
        }

	/* create the private key object */
	privateKeyObject = pk11_importPrivateKey(slot, node->privKey, 
								node->pubItem);
	if ( privateKeyObject == NULL ) {
	    goto end_loop;
	}

	publicKeyObject = pk11_importPublicKey(slot, node->privKey, NULL,
								node->pubItem);
	if ( node->cert ) {
	    /* Now import the Cert */
	    certObject = pk11_importCertificate(slot, node->cert,
						node->pubItem->data,
						node->pubItem->len, PR_TRUE);

	    /* Copy the subject */
	    if ( certObject ) {
		/* NOTE: cert has been adopted */
		PK11Attribute *attribute;

		/* Copy the Subject */
		attribute = pk11_FindAttribute(certObject,CKA_SUBJECT);
		if (attribute) {
		    pk11_forceAttribute(privateKeyObject,
				pk11_attr_expand(&attribute->attrib));
		    if (publicKeyObject) {
			pk11_forceAttribute(publicKeyObject,
				pk11_attr_expand(&attribute->attrib));
		    }
		    pk11_FreeAttribute(attribute);
		}
		pk11_FreeObject(certObject);
	    }
	}

	if ( publicKeyObject != NULL ) {
	    pk11_AddSlotObject(slot, publicKeyObject);
	}

	pk11_AddSlotObject(slot, privateKeyObject);

end_loop:
	SECKEY_LowDestroyPrivateKey(node->privKey);
	if ( node->cert ) {
	    CERT_DestroyCertificate(node->cert);
	}
		
    }
    PORT_FreeArena(keylist.arena, PR_FALSE);

    return CKR_OK;
}

/*
 * structure to collect certs into
 */
typedef struct pk11CertDataStr {
    int cert_count;
    int max_cert_count;
    CERTCertificate **certs;
} pk11CertData;

/*
 * collect all the certs from the traverse call.
 */	
static SECStatus
pk11_cert_collect(CERTCertificate *cert,void *arg) {
    pk11CertData *cd = (pk11CertData *)arg;

    /* shouldn't happen, but don't crash if it does */
    if (cd->cert_count >= cd->max_cert_count) {
	PORT_Assert(0);
	return SECFailure;
    }

    cd->certs[cd->cert_count++] = CERT_DupCertificate(cert);
    return SECSuccess;
}

/*
 * find any certs that may match the template and load them.
 */
static void
pk11_searchCerts(PK11Slot *slot, CK_ATTRIBUTE *pTemplate, CK_LONG ulCount) {
    int i;
    SECItem derCert = { siBuffer, NULL, 0 };
    SECItem derSubject = { siBuffer, NULL, 0 };
    SECItem name = { siBuffer, NULL, 0 };
    CERTIssuerAndSN issuerSN = {
	{ siBuffer, NULL, 0 },
	{ NULL, NULL },
	{ siBuffer, NULL, 0 }
    };
    SECItem *copy = NULL;
    CERTCertDBHandle *handle = NULL;
    SECKEYKeyDBHandle *keyHandle = NULL;
    pk11CertData certData;


    /*
     * These should be stored in the slot some day in the future
     */
    handle = CERT_GetDefaultCertDB();
    if (handle == NULL) return;
    keyHandle = SECKEY_GetDefaultKeyDB();
    if (keyHandle == NULL) return;

    /*
     * look for things to search on certs for. We only need one of these
     * items. If we find all the certs that match that item, import them
     * (as long as they are user certs). We'll let find objects filter out
     * the ones that don't apply.
     */
    for (i=0 ;i < (int)ulCount; i++) {

	switch (pTemplate[i].type) {
	case CKA_SUBJECT: copy = &derSubject; break;
	case CKA_ISSUER: copy = &issuerSN.derIssuer; break;
	case CKA_SERIAL_NUMBER: copy = &issuerSN.serialNumber; break;
	case CKA_VALUE: copy = &derCert; break;
	case CKA_LABEL: copy = &name; break;
	case CKA_CLASS:
	    if (pTemplate[i].ulValueLen != sizeof(CK_OBJECT_CLASS)) {
		return;
	    }
	    if (*((CK_OBJECT_CLASS *)pTemplate[i].pValue) != CKO_CERTIFICATE) {
		return;
	    }
	    copy = NULL; break;
	case CKA_PRIVATE:
	    if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
		return;
	    }
	    if (*((CK_BBOOL *)pTemplate[i].pValue) != CK_FALSE) {
		return;
	    }
	    copy = NULL; break;
	case CKA_TOKEN:
	    if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
		return;
	    }
	    if (*((CK_BBOOL *)pTemplate[i].pValue) != CK_TRUE) {
		return;
	    }
	    copy = NULL; break;
	case CKA_CERTIFICATE_TYPE:
	case CKA_ID:
	case CKA_MODIFIABLE:
	     copy = NULL; break;
	/* can't be a certificate if it doesn't match one of the above 
	 * attributes */
	default: return;
	}
 	if (copy) {
	    copy->data = (unsigned char*)pTemplate[i].pValue;
	    copy->len = pTemplate[i].ulValueLen;
	}
    }

    certData.max_cert_count = 0;
    certData.certs = NULL;
    certData.cert_count = 0;

    if (derCert.data != NULL) {
	CERTCertificate *cert = CERT_FindCertByDERCert(handle,&derCert);
	if (cert != NULL) {
	   certData.certs = 
		(CERTCertificate **) PORT_Alloc(sizeof (CERTCertificate *));
	   if (certData.certs) {
		certData.certs[0] = cert;
	   	certData.cert_count = 1;
	   } else CERT_DestroyCertificate(cert);
	}
    } else if (name.data != NULL) {
	char *tmp_name = (char*)PORT_Alloc(name.len+1);

	if (tmp_name == NULL) {
	    return;
	}
	PORT_Memcpy(tmp_name,name.data,name.len);
	tmp_name[name.len] = 0;

	certData.max_cert_count=CERT_NumPermCertsForNickname(handle,tmp_name);
	if (certData.max_cert_count > 0) {
	    certData.certs = (CERTCertificate **) 
		PORT_Alloc(certData.max_cert_count *sizeof(CERTCertificate *));
	    if (certData.certs) {
	    	CERT_TraversePermCertsForNickname(handle,tmp_name,
				pk11_cert_collect, &certData);
	    }
	   
	}

	PORT_Free(tmp_name);
    } else if (derSubject.data != NULL) {
	certData.max_cert_count=CERT_NumPermCertsForSubject(handle,&derSubject);
	if (certData.max_cert_count > 0) {
	    certData.certs = (CERTCertificate **) 
		PORT_Alloc(certData.max_cert_count *sizeof(CERTCertificate *));
	    if (certData.certs) {
	    	CERT_TraversePermCertsForSubject(handle,&derSubject,
				pk11_cert_collect, &certData);
	    }
	}
    } else if ((issuerSN.derIssuer.data != NULL) && 
			(issuerSN.serialNumber.data != NULL)) {
	CERTCertificate *cert = CERT_FindCertByIssuerAndSN(handle,&issuerSN);

	if (cert != NULL) {
	   certData.certs = 
		(CERTCertificate **) PORT_Alloc(sizeof (CERTCertificate *));
	   if (certData.certs) {
		certData.certs[0] = cert;
	   	certData.cert_count = 1;
	   } else CERT_DestroyCertificate(cert);
	}
    } else {
	/* PORT_Assert(0); may get called when not looking for certs */
	/* look up all the certs sometime, and get rid of the assert */;
    }


    for (i=0 ; i < certData.cert_count ; i++) {
	CERTCertificate *cert = certData.certs[i];

	/* we are only interested in permanment user certs here */
	if ((cert->isperm) && (cert->trust) && 
    	  (( cert->trust->sslFlags & CERTDB_USER ) ||
	   ( cert->trust->emailFlags & CERTDB_USER ) ||
	   ( cert->trust->objectSigningFlags & CERTDB_USER )) &&
	  ( cert->nickname != NULL )  &&
		(SECKEY_KeyForCertExists(keyHandle, cert) == SECSuccess)) {
	    PK11Object *obj;
	    pk11_importCertificate(slot, cert, NULL, 0, PR_FALSE);
	    obj = pk11_importPublicKey(slot, NULL, cert, NULL);
	    if (obj) pk11_AddSlotObject(slot, obj);
	}
	CERT_DestroyCertificate(cert);
    }
    if (certData.certs) PORT_Free(certData.certs);

    return;
}
	

/* NSC_FindObjectsInit initializes a search for token and session objects 
 * that match a template. */
CK_RV NSC_FindObjectsInit(CK_SESSION_HANDLE hSession,
    			CK_ATTRIBUTE_PTR pTemplate,CK_ULONG ulCount)
{
    PK11ObjectListElement *objectList = NULL;
    PK11ObjectListElement *olp;
    PK11SearchResults *search,*freeSearch;
    PK11Session *session;
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    int count, i;
    CK_RV crv;
    
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	return CKR_SESSION_HANDLE_INVALID;
    }
    
    /* resync token objects with the data base */
    if ((session->info.slotID == PRIVATE_KEY_SLOT_ID) || 
    			(session->info.slotID == FIPS_SLOT_ID)) {
	if (slot->DB_loaded == PR_FALSE) {
	    /* if we aren't logged in, we can't unload all key keys
	     * and certs. Just unload those certs we need for this search
	     */
    	    if ((!slot->isLoggedIn) && (slot->needLogin)) {
		pk11_searchCerts(slot,pTemplate,ulCount);
	    } else {
		pk11_importKeyDB(slot);
		slot->DB_loaded = PR_TRUE;
	   }
	}
    }

    
    /* build list of found objects in the session */
    crv = pk11_searchObjectList(&objectList,slot->tokObjects,
	slot->objectLock, pTemplate, ulCount, (PRBool)((!slot->needLogin) ||
							slot->isLoggedIn));
    if (crv != CKR_OK) {
	pk11_FreeObjectList(objectList);
	pk11_FreeSession(session);
	return crv;
    }


    /* copy list to session */
    count = 0;
    for (olp = objectList; olp != NULL; olp = olp->next) {
	count++;
    }
    search = (PK11SearchResults *)PORT_Alloc(sizeof(PK11SearchResults));
    if (search == NULL) {
	pk11_FreeObjectList(objectList);
	pk11_FreeSession(session);
	return CKR_HOST_MEMORY;
    }
    search->handles = (CK_OBJECT_HANDLE *)
				PORT_Alloc(sizeof(CK_OBJECT_HANDLE) * count);
    if (search->handles == NULL) {
	PORT_Free(search);
	pk11_FreeObjectList(objectList);
	pk11_FreeSession(session);
	return CKR_HOST_MEMORY;
    }
    for (i=0; i < count; i++) {
	search->handles[i] = objectList->object->handle;
	objectList = pk11_FreeObjectListElement(objectList);
    }

    /* store the search info */
    search->index = 0;
    search->size = count;
    if ((freeSearch = session->search) != NULL) {
	session->search = NULL;
	pk11_FreeSearch(freeSearch);
    }
    session->search = search;
    pk11_FreeSession(session);
    return CKR_OK;
}


/* NSC_FindObjects continues a search for token and session objects 
 * that match a template, obtaining additional object handles. */
CK_RV NSC_FindObjects(CK_SESSION_HANDLE hSession,
    CK_OBJECT_HANDLE_PTR phObject,CK_ULONG ulMaxObjectCount,
    					CK_ULONG_PTR pulObjectCount)
{
    PK11Session *session;
    PK11SearchResults *search;
    int	transfer;
    int left;

    *pulObjectCount = 0;
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    if (session->search == NULL) {
	pk11_FreeSession(session);
	return CKR_OK;
    }
    search = session->search;
    left = session->search->size - session->search->index;
    transfer = ((int)ulMaxObjectCount > left) ? left : ulMaxObjectCount;
    PORT_Memcpy(phObject,&search->handles[search->index],
					transfer*sizeof(CK_OBJECT_HANDLE_PTR));
    search->index += transfer;
    if (search->index == search->size) {
	session->search = NULL;
	pk11_FreeSearch(search);
    }
    *pulObjectCount = transfer;
    return CKR_OK;
}

/* NSC_FindObjectsFinal finishes a search for token and session objects. */
CK_RV NSC_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
    PK11Session *session;
    PK11SearchResults *search;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    search = session->search;
    session->search = NULL;
    if (search == NULL) {
	pk11_FreeSession(session);
	return CKR_OK;
    }
    pk11_FreeSearch(search);
    return CKR_OK;
}



CK_RV NSC_WaitForSlotEvent(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot,
							 CK_VOID_PTR pReserved)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}
