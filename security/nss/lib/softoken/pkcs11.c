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
#include "lowkeyi.h"
#include "blapi.h"
#include "secder.h"
#include "secport.h"
#include "pcert.h"

#include "keydbi.h" 

 
/*
 * ******************** Static data *******************************
 */

/* The next three strings must be exactly 32 characters long */
static char *manufacturerID      = "mozilla.org                     ";
static char manufacturerID_space[33];
static char *libraryDescription  = "NSS Internal Crypto Services    ";
static char libraryDescription_space[33];
#ifdef notdef
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
/* The next two strings must be exactly 64 characters long, with the
   first 32 characters meaningful  */
static char *slotDescription     =
        "Netscape Internal FIPS-140-1 Cryptographic Services             ";
static char *privSlotDescription =
        "Netscape FIPS-140-1 User Private Key Services                   ";
#endif

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
static const CK_FUNCTION_LIST pk11_funcList = {
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
static const desKey  pk11_desWeakTable[] = {
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

    
static const int pk11_desWeakTableSize = sizeof(pk11_desWeakTable)/
						sizeof(pk11_desWeakTable[0]);

/* DES KEY Parity conversion table. Takes each byte/2 as an index, returns
 * that byte with the proper parity bit set */
static const unsigned char parityTable[256] = {
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

#define CK_MAX 0xffffffff

static const struct mechanismList mechanisms[] = {

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
     {CKM_RSA_PKCS_KEY_PAIR_GEN,{128,CK_MAX,CKF_GENERATE_KEY_PAIR},PR_TRUE},
     {CKM_RSA_PKCS,		{128,CK_MAX,CKF_DUZ_IT_ALL},	PR_TRUE},
#ifdef PK11_RSA9796_SUPPORTED
     {CKM_RSA_9796,		{128,CK_MAX,CKF_DUZ_IT_ALL},	PR_TRUE}, 
#endif
     {CKM_RSA_X_509,		{128,CK_MAX,CKF_DUZ_IT_ALL},	PR_TRUE}, 
     /* -------------- RSA Multipart Signing Operations -------------------- */
     {CKM_MD2_RSA_PKCS,		{128,CK_MAX,CKF_SN_VR}, 	PR_TRUE},
     {CKM_MD5_RSA_PKCS,		{128,CK_MAX,CKF_SN_VR}, 	PR_TRUE},
     {CKM_SHA1_RSA_PKCS,	{128,CK_MAX,CKF_SN_VR}, 	PR_TRUE},
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
#ifdef NSS_SOFTOKEN_DOES_CAST
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
#ifdef NSS_SOFTOKEN_DOES_IDEA
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
     {CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC, {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_DES3_EDE_CBC,	     {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_DES2_EDE_CBC,	     {24,24, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC2_40_CBC,		     {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC2_128_CBC,		     {128,128, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC4_40,		     {40,40, CKF_GENERATE}, PR_TRUE},
     {CKM_PBE_SHA1_RC4_128,		     {128,128, CKF_GENERATE}, PR_TRUE},
     {CKM_PBA_SHA1_WITH_SHA1_HMAC,	     {20,20, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN,    {20,20, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN,     {16,16, CKF_GENERATE}, PR_TRUE},
     {CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN,     {16,16, CKF_GENERATE}, PR_TRUE},
};
static CK_ULONG mechanismCount = sizeof(mechanisms)/sizeof(mechanisms[0]);

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
pk11_configure(char *man, char *libdes)
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

    return CKR_OK;
}

/*
 * ******************** Password Utilities *******************************
 */

/*
 * see if the key DB password is enabled
 */
PRBool
pk11_hasNullPassword(NSSLOWKEYDBHandle *keydb,SECItem **pwitem)
{
    PRBool pwenabled;
    
    pwenabled = PR_FALSE;
    *pwitem = NULL;
    if (nsslowkey_HasKeyDBPassword (keydb) == SECSuccess) {
	*pwitem = nsslowkey_HashPassword("", keydb->global_salt);
	if ( *pwitem ) {
	    if (nsslowkey_CheckKeyDBPassword (keydb, *pwitem) == SECSuccess) {
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
    CK_CERTIFICATE_TYPE type;
    PK11Attribute *attribute;
    CK_RV crv;
    PK11SessionObject *sessObject = pk11_narrowToSessionObject(object);

    PORT_Assert(sessObject);

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

    /* in PKCS #11, Issuer is a required field */
    if ( !pk11_hasAttribute(object,CKA_ISSUER) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    /* in PKCS #11, Serial is a required field */
    if ( !pk11_hasAttribute(object,CKA_SERIAL_NUMBER) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    /* add it to the object */
    object->objectInfo = NULL;
    object->infoFree = (PK11Free) NULL;
    
    /* now just verify the required date fields */
    crv = pk11_defaultAttribute(object, CKA_ID, NULL, 0);
    if (crv != CKR_OK) { return crv; }

    if (pk11_isTrue(object,CKA_TOKEN)) {
	PK11Slot *slot = session->slot;
	SECItem derCert;
	NSSLOWCERTCertificate *cert;
 	NSSLOWCERTCertTrust *trust = NULL;
 	NSSLOWCERTCertTrust userTrust = 
		{ CERTDB_USER, CERTDB_USER, CERTDB_USER };
 	NSSLOWCERTCertTrust defTrust = 
		{ CERTDB_TRUSTED_UNKNOWN, 
			CERTDB_TRUSTED_UNKNOWN, CERTDB_TRUSTED_UNKNOWN };
	char *label;
	SECStatus rv;
	PRBool inDB = PR_TRUE;

	if (slot->certDB == NULL) {
	    return CKR_TOKEN_WRITE_PROTECTED;
	}

	/* get the der cert */ 
	attribute = pk11_FindAttribute(object,CKA_VALUE);
	PORT_Assert(attribute);

	derCert.data = (unsigned char *)attribute->attrib.pValue;
	derCert.len = attribute->attrib.ulValueLen ;

	label = pk11_getString(object,CKA_LABEL);

	cert =  nsslowcert_FindCertByDERCert(slot->certDB, &derCert);
        if (cert == NULL) {
	    cert = nsslowcert_DecodeDERCertificate(&derCert,PR_FALSE,label);
	    inDB = PR_FALSE;
	}
	if (cert == NULL) {
	    if (label) PORT_Free(label);
    	    pk11_FreeAttribute(attribute);
	    return CKR_ATTRIBUTE_VALUE_INVALID;
	}

	if (slot->keyDB && nsslowkey_KeyForCertExists(slot->keyDB,cert)) {
	    trust = &userTrust;
	}
	if (!inDB) {
	    if (!trust) trust = &defTrust;
	    rv = nsslowcert_AddPermCert(slot->certDB, cert, label, trust);
	} else {
	    rv = trust ? nsslowcert_ChangeCertTrust(slot->certDB,cert,trust) :
				SECSuccess;
	}

	if (label) PORT_Free(label);
	pk11_FreeAttribute(attribute);
	if (rv != SECSuccess) {
	    nsslowcert_DestroyCertificate(cert);
	    return CKR_DEVICE_ERROR;
	}
	object->handle=pk11_mkHandle(slot,&cert->certKey,PK11_TOKEN_TYPE_CERT);
	nsslowcert_DestroyCertificate(cert);
    }

    return CKR_OK;
}
unsigned int
pk11_MapTrust(CK_TRUST trust, PRBool clientAuth)
{
    unsigned int trustCA = clientAuth ? CERTDB_TRUSTED_CLIENT_CA :
							CERTDB_TRUSTED_CA;
    switch (trust) {
    case CKT_NETSCAPE_TRUSTED:
	return CERTDB_VALID_PEER|CERTDB_TRUSTED;
    case CKT_NETSCAPE_TRUSTED_DELEGATOR:
	return CERTDB_VALID_CA|trustCA;
    case CKT_NETSCAPE_UNTRUSTED:
	return CERTDB_NOT_TRUSTED;
    case CKT_NETSCAPE_MUST_VERIFY:
	return 0;
    case CKT_NETSCAPE_VALID: /* implies must verify */
	return CERTDB_VALID_PEER;
    case CKT_NETSCAPE_VALID_DELEGATOR: /* implies must verify */
	return CERTDB_VALID_CA;
    default:
	break;
    }
    return CERTDB_TRUSTED_UNKNOWN;
}
    
	
/*
 * check the consistancy and initialize a Trust Object 
 */
static CK_RV
pk11_handleTrustObject(PK11Session *session,PK11Object *object)
{
    NSSLOWCERTIssuerAndSN issuerSN;

    /* we can't store any certs private */
    if (pk11_isTrue(object,CKA_PRIVATE)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* certificates must have a type */
    if ( !pk11_hasAttribute(object,CKA_ISSUER) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    if ( !pk11_hasAttribute(object,CKA_SERIAL_NUMBER) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    if ( !pk11_hasAttribute(object,CKA_CERT_SHA1_HASH) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    if ( !pk11_hasAttribute(object,CKA_CERT_MD5_HASH) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    if (pk11_isTrue(object,CKA_TOKEN)) {
	PK11Slot *slot = session->slot;
	PK11Attribute *issuer = NULL;
	PK11Attribute *serial = NULL;
	NSSLOWCERTCertificate *cert = NULL;
	PK11Attribute *trust;
        CK_TRUST sslTrust = CKT_NETSCAPE_TRUST_UNKNOWN;
        CK_TRUST clientTrust = CKT_NETSCAPE_TRUST_UNKNOWN;
        CK_TRUST emailTrust = CKT_NETSCAPE_TRUST_UNKNOWN;
        CK_TRUST signTrust = CKT_NETSCAPE_TRUST_UNKNOWN;
 	NSSLOWCERTCertTrust dbTrust;
	SECStatus rv;


	if (slot->certDB == NULL) {
	    return CKR_TOKEN_WRITE_PROTECTED;
	}
	issuer = pk11_FindAttribute(object,CKA_ISSUER);
	PORT_Assert(issuer);
	issuerSN.derIssuer.data = (unsigned char *)issuer->attrib.pValue;
	issuerSN.derIssuer.len = issuer->attrib.ulValueLen ;

	serial = pk11_FindAttribute(object,CKA_SERIAL_NUMBER);
	PORT_Assert(serial);
	issuerSN.serialNumber.data = (unsigned char *)serial->attrib.pValue;
	issuerSN.serialNumber.len = serial->attrib.ulValueLen ;

	cert = nsslowcert_FindCertByIssuerAndSN(slot->certDB,&issuerSN);
	pk11_FreeAttribute(serial);
	pk11_FreeAttribute(issuer);

	if (cert == NULL) {
	    return CKR_ATTRIBUTE_VALUE_INVALID;
	}
	
	trust = pk11_FindAttribute(object,CKA_TRUST_SERVER_AUTH);
	if (trust) {
	    if (trust->attrib.ulValueLen == sizeof(CK_TRUST)) {
		PORT_Memcpy(&sslTrust,trust->attrib.pValue, sizeof(sslTrust));
	    }
	    pk11_FreeAttribute(trust);
	}
	trust = pk11_FindAttribute(object,CKA_TRUST_CLIENT_AUTH);
	if (trust) {
	    if (trust->attrib.ulValueLen == sizeof(CK_TRUST)) {
		PORT_Memcpy(&clientTrust,trust->attrib.pValue,
							 sizeof(clientTrust));
	    }
	    pk11_FreeAttribute(trust);
	}
	trust = pk11_FindAttribute(object,CKA_TRUST_EMAIL_PROTECTION);
	if (trust) {
	    if (trust->attrib.ulValueLen == sizeof(CK_TRUST)) {
		PORT_Memcpy(&emailTrust,trust->attrib.pValue,
							sizeof(emailTrust));
	    }
	    pk11_FreeAttribute(trust);
	}
	trust = pk11_FindAttribute(object,CKA_TRUST_CODE_SIGNING);
	if (trust) {
	    if (trust->attrib.ulValueLen == sizeof(CK_TRUST)) {
		PORT_Memcpy(&signTrust,trust->attrib.pValue,
							sizeof(signTrust));
	    }
	    pk11_FreeAttribute(trust);
	}

	/* preserve certain old fields */
	if (cert->trust) {
	    dbTrust.sslFlags =
			  cert->trust->sslFlags & CERTDB_PRESERVE_TRUST_BITS;
	    dbTrust.emailFlags=
			cert->trust->emailFlags & CERTDB_PRESERVE_TRUST_BITS;
	    dbTrust.objectSigningFlags = 
		cert->trust->objectSigningFlags & CERTDB_PRESERVE_TRUST_BITS;
	}

	dbTrust.sslFlags = pk11_MapTrust(sslTrust,PR_FALSE);
	dbTrust.sslFlags |= pk11_MapTrust(clientTrust,PR_TRUE);
	dbTrust.emailFlags = pk11_MapTrust(emailTrust,PR_FALSE);
	dbTrust.objectSigningFlags = pk11_MapTrust(signTrust,PR_FALSE);

	rv = nsslowcert_ChangeCertTrust(slot->certDB,cert,&dbTrust);
	object->handle=pk11_mkHandle(slot,&cert->certKey,PK11_TOKEN_TYPE_TRUST);
	nsslowcert_DestroyCertificate(cert);
	if (rv != SECSuccess) {
	   return CKR_DEVICE_ERROR;
	}
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Trust Object 
 */
static CK_RV
pk11_handleSMimeObject(PK11Session *session,PK11Object *object)
{

    /* we can't store any certs private */
    if (pk11_isTrue(object,CKA_PRIVATE)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* certificates must have a type */
    if ( !pk11_hasAttribute(object,CKA_SUBJECT) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    if ( !pk11_hasAttribute(object,CKA_NETSCAPE_EMAIL) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    if (pk11_isTrue(object,CKA_TOKEN)) {
	PK11Slot *slot = session->slot;
	SECItem derSubj,rawProfile,rawTime,emailKey;
	SECItem *pRawProfile = NULL;
	SECItem *pRawTime = NULL;
	char *email = NULL;
    	PK11Attribute *subject,*profile,*time;
	SECStatus rv;

	PORT_Assert(slot);
	if (slot->certDB == NULL) {
	    return CKR_TOKEN_WRITE_PROTECTED;
	}

	/* lookup SUBJECT */
	subject = pk11_FindAttribute(object,CKA_SUBJECT);
	PORT_Assert(subject);
	derSubj.data = (unsigned char *)subject->attrib.pValue;
	derSubj.len = subject->attrib.ulValueLen ;

	/* lookup VALUE */
	profile = pk11_FindAttribute(object,CKA_VALUE);
	if (profile) {
	    rawProfile.data = (unsigned char *)profile->attrib.pValue;
	    rawProfile.len = profile->attrib.ulValueLen ;
	    pRawProfile = &rawProfile;
	}

	/* lookup Time */
	time = pk11_FindAttribute(object,CKA_NETSCAPE_SMIME_TIMESTAMP);
	if (time) {
	    rawTime.data = (unsigned char *)time->attrib.pValue;
	    rawTime.len = time->attrib.ulValueLen ;
	    pRawTime = &rawTime;
	}


	email = pk11_getString(object,CKA_NETSCAPE_EMAIL);

	/* Store CRL by SUBJECT */
	rv = nsslowcert_SaveSMimeProfile(slot->certDB, email, &derSubj, 
				pRawProfile,pRawTime);

    	pk11_FreeAttribute(subject);
    	if (profile) pk11_FreeAttribute(profile);
    	if (time) pk11_FreeAttribute(time);
	if (rv != SECSuccess) {
    	    PORT_Free(email);
	    return CKR_DEVICE_ERROR;
	}
	emailKey.data = (unsigned char *)email;
	emailKey.len = PORT_Strlen(email)+1;

	object->handle = pk11_mkHandle(slot, &emailKey, PK11_TOKEN_TYPE_SMIME);
    	PORT_Free(email);
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Trust Object 
 */
static CK_RV
pk11_handleCrlObject(PK11Session *session,PK11Object *object)
{

    /* we can't store any certs private */
    if (pk11_isTrue(object,CKA_PRIVATE)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* certificates must have a type */
    if ( !pk11_hasAttribute(object,CKA_SUBJECT) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }
    if ( !pk11_hasAttribute(object,CKA_VALUE) ) {
	return CKR_TEMPLATE_INCOMPLETE;
    }

    if (pk11_isTrue(object,CKA_TOKEN)) {
	PK11Slot *slot = session->slot;
	PRBool isKRL = PR_FALSE;
	SECItem derSubj,derCrl;
	char *url = NULL;
    	PK11Attribute *subject,*crl;
	SECStatus rv;

	PORT_Assert(slot);
	if (slot->certDB == NULL) {
	    return CKR_TOKEN_WRITE_PROTECTED;
	}

	/* lookup SUBJECT */
	subject = pk11_FindAttribute(object,CKA_SUBJECT);
	PORT_Assert(subject);
	derSubj.data = (unsigned char *)subject->attrib.pValue;
	derSubj.len = subject->attrib.ulValueLen ;

	/* lookup VALUE */
	crl = pk11_FindAttribute(object,CKA_VALUE);
	PORT_Assert(crl);
	derCrl.data = (unsigned char *)crl->attrib.pValue;
	derCrl.len = crl->attrib.ulValueLen ;


	url = pk11_getString(object,CKA_NETSCAPE_URL);
	isKRL = pk11_isTrue(object,CKA_NETSCAPE_KRL);

	/* Store CRL by SUBJECT */
	rv = nsslowcert_AddCrl(slot->certDB, &derCrl, &derSubj, url, isKRL);

	if (url) {
	    PORT_Free(url);
	}
    	pk11_FreeAttribute(crl);
	if (rv != SECSuccess) {
    	    pk11_FreeAttribute(subject);
	    return CKR_DEVICE_ERROR;
	}

	object->handle = pk11_mkHandle(slot,&derSubj,
			isKRL ? PK11_TOKEN_KRL_HANDLE : PK11_TOKEN_TYPE_CRL);
    	pk11_FreeAttribute(subject);
    }

    return CKR_OK;
}

NSSLOWKEYPublicKey * pk11_GetPubKey(PK11Object *object,CK_KEY_TYPE key);
/*
 * check the consistancy and initialize a Public Key Object 
 */
static CK_RV
pk11_handlePublicKeyObject(PK11Session *session, PK11Object *object,
							 CK_KEY_TYPE key_type)
{
    CK_BBOOL encrypt = CK_TRUE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL wrap = CK_TRUE;
    CK_BBOOL derive = CK_FALSE;
    CK_BBOOL verify = CK_TRUE;
    CK_ATTRIBUTE_TYPE pubKeyAttr = CKA_VALUE;
    CK_RV crv;

    switch (key_type) {
    case CKK_RSA:
	if ( !pk11_hasAttribute(object, CKA_MODULUS)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	if ( !pk11_hasAttribute(object, CKA_PUBLIC_EXPONENT)) {
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	pubKeyAttr = CKA_MODULUS;
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
	if (key_type == CKK_DH) {
	    verify = CK_FALSE;
	    derive = CK_TRUE;
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
    crv = pk11_defaultAttribute(object,CKA_VERIFY,&verify,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_VERIFY_RECOVER,
						&recover,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_WRAP,&wrap,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 
    crv = pk11_defaultAttribute(object,CKA_DERIVE,&derive,sizeof(CK_BBOOL));
    if (crv != CKR_OK)  return crv; 

    object->objectInfo = pk11_GetPubKey(object,key_type);
    object->infoFree = (PK11Free) nsslowkey_DestroyPublicKey;

    if (pk11_isTrue(object,CKA_TOKEN)) {
	PK11Slot *slot = session->slot;
	NSSLOWKEYPrivateKey *priv;
	SECItem pubKey;

	crv = pk11_Attribute2SSecItem(NULL,&pubKey,object,pubKeyAttr);
	if (crv != CKR_OK) return crv;

	PORT_Assert(pubKey.data);
	if (slot->keyDB == NULL) {
	    PORT_Free(pubKey.data);
	    return CKR_TOKEN_WRITE_PROTECTED;
	}
	/* make sure the associated private key already exists */
	/* only works if we are logged in */
	priv = nsslowkey_FindKeyByPublicKey(slot->keyDB, &pubKey,
							 slot->password);
	if (priv == NULL) {
	    PORT_Free(pubKey.data);
	    return CKR_ATTRIBUTE_VALUE_INVALID;
	}
	nsslowkey_DestroyPrivateKey(priv);

	object->handle = pk11_mkHandle(slot, &pubKey, PK11_TOKEN_TYPE_PUB);
	PORT_Free(pubKey.data);
    }

    return CKR_OK;
}

static NSSLOWKEYPrivateKey * pk11_mkPrivKey(PK11Object *object,CK_KEY_TYPE key);
/*
 * check the consistancy and initialize a Private Key Object 
 */
static CK_RV
pk11_handlePrivateKeyObject(PK11Session *session,PK11Object *object,CK_KEY_TYPE key_type)
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
	PK11Slot *slot = session->slot;
	NSSLOWKEYPrivateKey *privKey;
	char *label;
	SECStatus rv = SECSuccess;
	SECItem pubKey;

	if (slot->keyDB == NULL) {
	    return CKR_TOKEN_WRITE_PROTECTED;
	}

	privKey=pk11_mkPrivKey(object,key_type);
	if (privKey == NULL) return CKR_HOST_MEMORY;
	label = pk11_getString(object,CKA_LABEL);

	crv = pk11_Attribute2SSecItem(NULL,&pubKey,object,CKA_NETSCAPE_DB);
	if (crv != CKR_OK) {
	    if (label) PORT_Free(label);
	    nsslowkey_DestroyPrivateKey(privKey);
	    return CKR_TEMPLATE_INCOMPLETE;
	}
	rv = nsslowkey_StoreKeyByPublicKey(object->slot->keyDB,
			privKey, &pubKey, label, object->slot->password);

	if (label) PORT_Free(label);
	object->handle = pk11_mkHandle(slot,&pubKey,PK11_TOKEN_TYPE_PRIV);
	if (pubKey.data) PORT_Free(pubKey.data);
	nsslowkey_DestroyPrivateKey(privKey);
	if (rv != SECSuccess) return CKR_DEVICE_ERROR;
    } else {
	object->objectInfo = pk11_mkPrivKey(object,key_type);
	if (object->objectInfo == NULL) return CKR_HOST_MEMORY;
	object->infoFree = (PK11Free) nsslowkey_DestroyPrivateKey;
	/* now NULL out the sensitive attributes */
	if (pk11_isTrue(object,CKA_SENSITIVE)) {
	    pk11_nullAttribute(object,CKA_PRIVATE_EXPONENT);
	    pk11_nullAttribute(object,CKA_PRIME_1);
	    pk11_nullAttribute(object,CKA_PRIME_2);
	    pk11_nullAttribute(object,CKA_EXPONENT_1);
	    pk11_nullAttribute(object,CKA_EXPONENT_2);
	    pk11_nullAttribute(object,CKA_COEFFICIENT);
	}
    }
    return CKR_OK;
}

/* forward delcare the DES formating function for handleSecretKey */
void pk11_FormatDESKey(unsigned char *key, int length);
static NSSLOWKEYPrivateKey *pk11_mkSecretKeyRep(PK11Object *object);

/* Validate secret key data, and set defaults */
static CK_RV
validateSecretKey(PK11Session *session, PK11Object *object, 
					CK_KEY_TYPE key_type, PRBool isFIPS)
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
#ifdef NSS_SOFTOKEN_DOES_CAST
    case CKK_CAST:
    case CKK_CAST3:
    case CKK_CAST5:
#endif
#if NSS_SOFTOKEN_DOES_IDEA
    case CKK_IDEA:
#endif
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
pk11_handleSecretKeyObject(PK11Session *session,PK11Object *object,
					CK_KEY_TYPE key_type, PRBool isFIPS)
{
    CK_RV crv;
    NSSLOWKEYPrivateKey *privKey = NULL;
    SECItem pubKey;
    char *label = NULL;

    pubKey.data = 0;

    /* First validate and set defaults */
    crv = validateSecretKey(session, object, key_type, isFIPS);
    if (crv != CKR_OK) goto loser;

    /* If the object is a TOKEN object, store in the database */
    if (pk11_isTrue(object,CKA_TOKEN)) {
	PK11Slot *slot = session->slot;
	SECStatus rv = SECSuccess;

	if (slot->keyDB == NULL) {
	    return CKR_TOKEN_WRITE_PROTECTED;
	}
	privKey=pk11_mkSecretKeyRep(object);
	if (privKey == NULL) return CKR_HOST_MEMORY;

	label = pk11_getString(object,CKA_LABEL);

	crv = pk11_Attribute2SecItem(NULL, &pubKey, object, CKA_ID);  
						/* Should this be ID? */
	if (crv != CKR_OK) goto loser;

	PORT_Assert(slot->keyDB);
	rv = nsslowkey_StoreKeyByPublicKey(slot->keyDB,
			privKey, &pubKey, label, slot->password);
	if (rv != SECSuccess) {
	    crv = CKR_DEVICE_ERROR;
	    goto loser;
	}

	object->handle = pk11_mkHandle(slot,&pubKey,PK11_TOKEN_TYPE_KEY);
    }

loser:
    if (label) PORT_Free(label);
    if (privKey) nsslowkey_DestroyPrivateKey(privKey);
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
	return pk11_handlePublicKeyObject(session,object,key_type);
    case CKO_PRIVATE_KEY:
	return pk11_handlePrivateKeyObject(session,object,key_type);
    case CKO_SECRET_KEY:
	/* make sure the required fields exist */
	return pk11_handleSecretKeyObject(session,object,key_type,
			     (PRBool)(session->slot->slotID == FIPS_SLOT_ID));
    default:
	break;
    }
    return CKR_ATTRIBUTE_VALUE_INVALID;
}

/* 
 * Handle Object does all the object consistancy checks, automatic attribute
 * generation, attribute defaulting, etc. If handleObject succeeds, the object
 * will be assigned an object handle, and the object installed in the session
 * or stored in the DB.
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
	break;
    case CKO_CERTIFICATE:
	crv = pk11_handleCertObject(session,object);
	break;
    case CKO_NETSCAPE_TRUST:
	crv = pk11_handleTrustObject(session,object);
	break;
    case CKO_NETSCAPE_CRL:
	crv = pk11_handleCrlObject(session,object);
	break;
    case CKO_NETSCAPE_SMIME:
	crv = pk11_handleSMimeObject(session,object);
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
    if (pk11_isToken(object->handle)) {
	pk11_convertSessionToToken(object);
    } else {
	object->slot = slot;
	pk11_AddObject(session,object);
    }

    return CKR_OK;
}

/*
 * ******************** Public Key Utilities ***************************
 */
/* Generate a low public key structure from an object */
NSSLOWKEYPublicKey *pk11_GetPubKey(PK11Object *object,CK_KEY_TYPE key_type)
{
    NSSLOWKEYPublicKey *pubKey;
    PLArenaPool *arena;
    CK_RV crv;

    if (object->objclass != CKO_PUBLIC_KEY) {
	return NULL;
    }

    if (pk11_isToken(object->handle)) {
/* ferret out the token object handle */
    }

    /* If we already have a key, use it */
    if (object->objectInfo) {
	return (NSSLOWKEYPublicKey *)object->objectInfo;
    }

    /* allocate the structure */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;

    pubKey = (NSSLOWKEYPublicKey *)
			PORT_ArenaAlloc(arena,sizeof(NSSLOWKEYPublicKey));
    if (pubKey == NULL) {
    	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }

    /* fill in the structure */
    pubKey->arena = arena;
    switch (key_type) {
    case CKK_RSA:
	pubKey->keyType = NSSLOWKEYRSAKey;
	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.rsa.modulus,
							object,CKA_MODULUS);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&pubKey->u.rsa.publicExponent,
						object,CKA_PUBLIC_EXPONENT);
	break;
    case CKK_DSA:
	pubKey->keyType = NSSLOWKEYDSAKey;
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
	pubKey->keyType = NSSLOWKEYDHKey;
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
    object->infoFree = (PK11Free) nsslowkey_DestroyPublicKey;
    return pubKey;
}

/* make a private key from a verified object */
static NSSLOWKEYPrivateKey *
pk11_mkPrivKey(PK11Object *object,CK_KEY_TYPE key_type)
{
    NSSLOWKEYPrivateKey *privKey;
    PLArenaPool *arena;
    CK_RV crv = CKR_OK;
    SECStatus rv;

    PORT_Assert(!pk11_isToken(object->handle));
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;

    privKey = (NSSLOWKEYPrivateKey *)
			PORT_ArenaAlloc(arena,sizeof(NSSLOWKEYPrivateKey));
    if (privKey == NULL)  {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }

    /* in future this would be a switch on key_type */
    privKey->arena = arena;
    switch (key_type) {
    case CKK_RSA:
	privKey->keyType = NSSLOWKEYRSAKey;
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
                          NSSLOWKEY_VERSION);
	if (rv != SECSuccess) crv = CKR_HOST_MEMORY;
	break;

    case CKK_DSA:
	privKey->keyType = NSSLOWKEYDSAKey;
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
	privKey->keyType = NSSLOWKEYDHKey;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.prime,
							object,CKA_PRIME);
    	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.base,
							object,CKA_BASE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.privateValue,
							object,CKA_VALUE);
    	if (crv != CKR_OK) break;
    	crv = pk11_Attribute2SSecItem(arena,&privKey->u.dh.publicValue,
							object,CKA_NETSCAPE_DB);
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
NSSLOWKEYPrivateKey *
pk11_GetPrivKey(PK11Object *object,CK_KEY_TYPE key_type)
{
    NSSLOWKEYPrivateKey *priv = NULL;

    if (object->objclass != CKO_PRIVATE_KEY) {
	return NULL;
    }
    if (object->objectInfo) {
	return (NSSLOWKEYPrivateKey *)object->objectInfo;
    }

    if (pk11_isToken(object->handle)) {
	/* grab it from the data base */
	PK11TokenObject *to = pk11_narrowToTokenObject(object);

	PORT_Assert(to);
	PORT_Assert(object->slot->keyDB);	
	priv = nsslowkey_FindKeyByPublicKey(object->slot->keyDB, &to->dbKey,
				       object->slot->password);
    } else {
	priv = pk11_mkPrivKey(object, key_type);
    }
    object->objectInfo = priv;
    object->infoFree = (PK11Free) nsslowkey_DestroyPrivateKey;
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
static NSSLOWKEYPrivateKey *
pk11_mkSecretKeyRep(PK11Object *object)
{
    NSSLOWKEYPrivateKey *privKey;
    PLArenaPool *arena = 0;
    CK_RV crv;
    SECStatus rv;
    static unsigned char derZero[1] = { 0 };

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) { crv = CKR_HOST_MEMORY; goto loser; }

    privKey = (NSSLOWKEYPrivateKey *)
			PORT_ArenaAlloc(arena,sizeof(NSSLOWKEYPrivateKey));
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
    privKey->keyType = NSSLOWKEYRSAKey;

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
    rv = DER_SetUInteger(privKey->arena, 
			&privKey->u.rsa.version, NSSLOWKEY_VERSION);
    if (rv != SECSuccess) { crv = CKR_HOST_MEMORY; goto loser; }

loser:
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	privKey = 0;
    }

    return privKey;
}

static PRBool
isSecretKey(NSSLOWKEYPrivateKey *privKey)
{
  if (privKey->keyType == NSSLOWKEYRSAKey && 
		privKey->u.rsa.publicExponent.len == 1 &&
      				privKey->u.rsa.publicExponent.data[0] == 0)
    return PR_TRUE;

  return PR_FALSE;
}

/**********************************************************************
 *
 *     Start of PKCS 11 functions 
 *
 **********************************************************************/


/* return the function list */
CK_RV NSC_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{
    *pFunctionList = (CK_FUNCTION_LIST_PTR) &pk11_funcList;
    return CKR_OK;
}

/* return the function list */
CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{
    return NSC_GetFunctionList(pFunctionList);
}

static PLHashNumber
pk11_HashNumber(const void *key)
{
    return (PLHashNumber) key;
}

/*
 * eventually I'd like to expunge all occurances of XXX_SLOT_ID and
 * just go with the info in the slot. This is one place, however,
 * where it might be a little difficult.
 */
char *
pk11_getDefTokName(CK_SLOT_ID slotID)
{
    static char buf[33];

    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	return "NSS Generic Crypto Services     ";
    case PRIVATE_KEY_SLOT_ID:
	return "NSS Certificate DB              ";
    case FIPS_SLOT_ID:
        return "NSS FIPS-140-1 Cerificate DB    ";
    default:
	break;
    }
    sprintf(buf,"NSS Application Token %08x  ",(unsigned int) slotID);
    return buf;
}

char *
pk11_getDefSlotName(CK_SLOT_ID slotID)
{
    static char buf[65];

    switch (slotID) {
    case NETSCAPE_SLOT_ID:
	return 
	 "NSS Internal Cryptographic Services Version 3.4                 ";
    case PRIVATE_KEY_SLOT_ID:
	return 
	 "NSS User Private Key and Certificate Services                   ";
    case FIPS_SLOT_ID:
        return 
         "Netscape FIPS-140-1 User Private Key Services                   ";
    default:
	break;
    }
    sprintf(buf,
     "NSS Application Slot %08x                                   ",
							(unsigned int) slotID);
    return buf;
}

static CK_ULONG nscSlotCount = 0;
static CK_SLOT_ID_PTR nscSlotList = NULL;
static CK_ULONG nscSlotListSize = 0;
static PLHashTable *nscSlotHashTable = NULL;

/* look up a slot structure from the ID (used to be a macro when we only
 * had two slots) */
PK11Slot *
pk11_SlotFromID(CK_SLOT_ID slotID)
{
    return (PK11Slot *)PL_HashTableLookupConst(nscSlotHashTable,
							(void *)slotID);
}

PK11Slot *
pk11_SlotFromSessionHandle(CK_SESSION_HANDLE handle)
{
    int slotIDIndex = (handle >> 24) & 0xff;

    if (slotIDIndex >= nscSlotCount) {
	return NULL;
    }

    return pk11_SlotFromID(nscSlotList[slotIDIndex]);
}
 
PK11Slot * pk11_NewSlotFromID(CK_SLOT_ID slotID)
{
    PK11Slot *slot = NULL;
    PLHashEntry *entry;

    if (nscSlotList == NULL) {
	nscSlotListSize = NSC_SLOT_LIST_BLOCK_SIZE;
	nscSlotList = (CK_SLOT_ID *)
			PORT_ZAlloc(nscSlotListSize*sizeof(CK_SLOT_ID));
	if (nscSlotList == NULL) {
	    return NULL;
	}
    }
    if (nscSlotCount >= nscSlotListSize) {
	nscSlotListSize += NSC_SLOT_LIST_BLOCK_SIZE;
	nscSlotList = (CK_SLOT_ID *) PORT_Realloc(nscSlotList,
					nscSlotListSize*sizeof(CK_SLOT_ID));
	if (nscSlotList == NULL) {
	    return NULL;
	}
    }

    if (nscSlotHashTable == NULL) {
	nscSlotHashTable = PL_NewHashTable(64,pk11_HashNumber,PL_CompareValues,
					PL_CompareValues, NULL, 0);
	if (nscSlotHashTable == NULL) {
	    return NULL;
	}
    }

    slot = (PK11Slot *) PORT_ZAlloc(sizeof(PK11Slot));
    if (slot == NULL) {
	return NULL;
    }

    entry = PL_HashTableAdd(nscSlotHashTable,(void *)slotID,slot);
    if (entry == NULL) {
	PORT_Free(slot);
	return NULL;
    }
    slot->index = nscSlotCount;
    nscSlotList[nscSlotCount++] = slotID;

    return slot;
}

/*
 * initialize one of the slot structures. figure out which by the ID
 */
CK_RV
PK11_SlotInit(char *configdir,pk11_token_parameters *params)
{
    int i;
    CK_SLOT_ID slotID = params->slotID;
    PK11Slot *slot = pk11_NewSlotFromID(slotID);
    PRBool needLogin = !params->noKeyDB;
    CK_RV crv;

    if (slot == NULL) {
	return CKR_HOST_MEMORY;
    }

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
    slot->tokenHashTable = PL_NewHashTable(64,pk11_HashNumber,PL_CompareValues,
					SECITEM_HashCompare, NULL, 0);
    if (slot->tokenHashTable == NULL) {
	return CKR_HOST_MEMORY;
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
    slot->certDB = NULL;
    slot->keyDB = NULL;
    slot->minimumPinLen = 0;
    slot->readOnly = params->readOnly;
    pk11_setStringName(params->tokdes ? params->tokdes : 
	pk11_getDefTokName(slotID), slot->tokDescription, 
						sizeof(slot->tokDescription));
    pk11_setStringName(params->slotdes ? params->slotdes : 
	pk11_getDefSlotName(slotID), slot->slotDescription, 
						sizeof(slot->slotDescription));

    if ((!params->noCertDB) || (!params->noKeyDB)) {
	crv = pk11_DBInit(params->configdir ? params->configdir : configdir,
		params->certPrefix, params->keyPrefix, params->readOnly,
		params->noCertDB, params->noKeyDB, params->forceOpen, 
						&slot->certDB, &slot->keyDB);
	if (crv != CKR_OK) {
	    /* shoutdown slot? */
	    return crv;
	}
    }
    if (needLogin) {
	/* if the data base is initialized with a null password,remember that */
	slot->needLogin = 
		(PRBool)!pk11_hasNullPassword(slot->keyDB,&slot->password);
	if (params->minPW <= PK11_MAX_PIN) {
	    slot->minimumPinLen = params->minPW;
	}
	if ((slot->minimumPinLen == 0) && (params->pwRequired) && 
		(slot->minimumPinLen <= PK11_MAX_PIN)) {
	    slot->minimumPinLen = 1;
	}
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
CK_RV nsc_CommonInitialize(CK_VOID_PTR pReserved, PRBool isFIPS)
{
    static PRBool init = PR_FALSE;
    CK_RV crv = CKR_OK;
    SECStatus rv;
    CK_C_INITIALIZE_ARGS *init_args = (CK_C_INITIALIZE_ARGS *) pReserved;
    int i;

    if (init) {
	return crv;
    }

    rv = RNG_RNGInit();         /* initialize random number generator */
    if (rv != SECSuccess) {
	crv = CKR_DEVICE_ERROR;
	goto loser;
    }
    RNG_SystemInfoForRNG();


    /* NOTE:
     * we should be getting out mutexes from this list, not statically binding
     * them from NSPR. This should happen before we allow the internal to split
     * off from the rest on NSS.
     */

    /* initialize the key and cert db's */
    nsslowkey_SetDefaultKeyDBAlg
			     (SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC);
    crv = CKR_ARGUMENTS_BAD;
    if ((init_args && init_args->LibraryParameters)) {
	pk11_parameters paramStrings;
       
	crv = secmod_parseParameters
		((char *)init_args->LibraryParameters,&paramStrings, isFIPS);
	if (crv != CKR_OK) {
	    return crv;
	}
	crv = pk11_configure(paramStrings.man, paramStrings.libdes);
        if (crv != CKR_OK) {
	    goto loser;
	}

	for (i=0; i < paramStrings.token_count; i++) {
	    crv = 
		PK11_SlotInit(paramStrings.configdir,&paramStrings.tokens[i]);
	    if (crv != CKR_OK) break;
	}
loser:
	secmod_freeParams(&paramStrings);
    }
    init = (PRBool) (crv == CKR_OK);

    return crv;
}

CK_RV NSC_Initialize(CK_VOID_PTR pReserved)
{
    return nsc_CommonInitialize(pReserved,PR_FALSE);
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
    pInfo->cryptokiVersion.minor = 11;
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
    *pulCount = nscSlotCount;
    if (pSlotList != NULL) {
	PORT_Memcpy(pSlotList,nscSlotList,nscSlotCount*sizeof(CK_SLOT_ID));
    }
    return CKR_OK;
}
	
/* NSC_GetSlotInfo obtains information about a particular slot in the system. */
CK_RV NSC_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
    PK11Slot *slot = pk11_SlotFromID(slotID);
    if (slot == NULL) return CKR_SLOT_ID_INVALID;

    pInfo->firmwareVersion.major = 0;
    pInfo->firmwareVersion.minor = 0;
    PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
    PORT_Memcpy(pInfo->slotDescription,slot->slotDescription,64);
    pInfo->flags = CKF_TOKEN_PRESENT;
    /* ok we really should read it out of the keydb file. */
    /* pInfo->hardwareVersion.major = NSSLOWKEY_DB_FILE_VERSION; */
    pInfo->hardwareVersion.major = 3;
    pInfo->hardwareVersion.minor = 2;
    return CKR_OK;
}

#define CKF_THREAD_SAFE 0x8000 /* for now */

/* NSC_GetTokenInfo obtains information about a particular token in 
 * the system. */
CK_RV NSC_GetTokenInfo(CK_SLOT_ID slotID,CK_TOKEN_INFO_PTR pInfo)
{
    PK11Slot *slot = pk11_SlotFromID(slotID);
    NSSLOWKEYDBHandle *handle;

    if (slot == NULL) return CKR_SLOT_ID_INVALID;

    PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
    PORT_Memcpy(pInfo->model,"NSS 3           ",16);
    PORT_Memcpy(pInfo->serialNumber,"0000000000000000",16);
    pInfo->ulMaxSessionCount = 0; /* arbitrarily large */
    pInfo->ulSessionCount = slot->sessionCount;
    pInfo->ulMaxRwSessionCount = 0; /* arbitarily large */
    pInfo->ulRwSessionCount = slot->rwSessionCount;
    pInfo->firmwareVersion.major = 0;
    pInfo->firmwareVersion.minor = 0;
    PORT_Memcpy(pInfo->label,slot->tokDescription,32);
    handle = slot->keyDB;
    if (handle == NULL) {
        pInfo->flags= CKF_RNG | CKF_WRITE_PROTECTED | CKF_THREAD_SAFE;
	pInfo->ulMaxPinLen = 0;
	pInfo->ulMinPinLen = 0;
	pInfo->ulTotalPublicMemory = 0;
	pInfo->ulFreePublicMemory = 0;
	pInfo->ulTotalPrivateMemory = 0;
	pInfo->ulFreePrivateMemory = 0;
	pInfo->hardwareVersion.major = 4;
	pInfo->hardwareVersion.minor = 0;
    } else {
	/*
	 * we have three possible states which we may be in:
	 *   (1) No DB password has been initialized. This also means we
	 *   have no keys in the key db.
	 *   (2) Password initialized to NULL. This means we have keys, but
	 *   the user has chosen not use a password.
	 *   (3) Finally we have an initialized password whicn is not NULL, and
	 *   we will need to prompt for it.
	 */
	if (nsslowkey_HasKeyDBPassword(handle) == SECFailure) {
	    pInfo->flags = CKF_THREAD_SAFE | CKF_LOGIN_REQUIRED;
	} else if (!slot->needLogin) {
	    pInfo->flags = CKF_THREAD_SAFE | CKF_USER_PIN_INITIALIZED;
	} else {
	    pInfo->flags = CKF_THREAD_SAFE | 
				CKF_LOGIN_REQUIRED | CKF_USER_PIN_INITIALIZED;
	}
	pInfo->ulMaxPinLen = PK11_MAX_PIN;
	pInfo->ulMinPinLen = 0;
	if (slot->minimumPinLen > 0) {
	    pInfo->ulMinPinLen = (CK_ULONG)slot->minimumPinLen;
	}
	pInfo->ulTotalPublicMemory = 1;
	pInfo->ulFreePublicMemory = 1;
	pInfo->ulTotalPrivateMemory = 1;
	pInfo->ulFreePrivateMemory = 1;
	pInfo->hardwareVersion.major = CERT_DB_FILE_VERSION;
	pInfo->hardwareVersion.minor = 0;
    }
    return CKR_OK;
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
	break;
     default:
	*pulCount = 0;
	for (i=0; i < (int) mechanismCount; i++) {
	    if (mechanisms[i].privkey) {
		(*pulCount)++;
		if (pMechanismList != NULL) {
		    *pMechanismList++ = mechanisms[i].type;
		}
	    }
	}
	break;
    }
    return CKR_OK;
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
    default:
	isPrivateKey = PR_TRUE;
	break;
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
pk11_TurnOffUser(NSSLOWCERTCertificate *cert, SECItem *k, void *arg)
{
   NSSLOWCERTCertTrust trust;
   SECStatus rv;

   rv = nsslowcert_GetCertTrust(cert,&trust);
   if (rv == SECSuccess && ((trust.emailFlags & CERTDB_USER) ||
			    (trust.sslFlags & CERTDB_USER) ||
			    (trust.objectSigningFlags & CERTDB_USER))) {
	trust.emailFlags &= ~CERTDB_USER;
	trust.sslFlags &= ~CERTDB_USER;
	trust.objectSigningFlags &= ~CERTDB_USER;
	nsslowcert_ChangeCertTrust(cert->dbhandle,cert,&trust);
   }
   return SECSuccess;
}

/* NSC_InitToken initializes a token. */
CK_RV NSC_InitToken(CK_SLOT_ID slotID,CK_CHAR_PTR pPin,
 				CK_ULONG ulPinLen,CK_CHAR_PTR pLabel) {
    PK11Slot *slot = pk11_SlotFromID(slotID);
    NSSLOWKEYDBHandle *handle;
    NSSLOWCERTCertDBHandle *certHandle;
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
    handle = slot->keyDB;
    if (handle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* what to do on an error here? */
    rv = nsslowkey_ResetKeyDB(handle);

    /* finally  mark all the user certs as non-user certs */
    certHandle = slot->certDB;
    if (certHandle == NULL) return CKR_OK;

    nsslowcert_TraversePermCerts(certHandle,pk11_TurnOffUser, NULL);

    return CKR_OK; /*is this the right function for not implemented*/
}


/* NSC_InitPIN initializes the normal user's PIN. */
CK_RV NSC_InitPIN(CK_SESSION_HANDLE hSession,
    					CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    PK11Session *sp;
    PK11Slot *slot;
    NSSLOWKEYDBHandle *handle;
    SECItem *newPin;
    char newPinStr[PK11_MAX_PIN+1];
    SECStatus rv;

    
    sp = pk11_SessionFromHandle(hSession);
    if (sp == NULL) {
	return CKR_SESSION_HANDLE_INVALID;
    }

    slot = pk11_SlotFromSession(sp);
    if (slot == NULL) {
	pk11_FreeSession(sp);
	return CKR_SESSION_HANDLE_INVALID;;
    }

    handle = slot->keyDB;
    if (handle == NULL) {
	pk11_FreeSession(sp);
	return CKR_PIN_LEN_RANGE;
    }


    if (sp->info.state != CKS_RW_SO_FUNCTIONS) {
	pk11_FreeSession(sp);
	return CKR_USER_NOT_LOGGED_IN;
    }

    pk11_FreeSession(sp);

    /* make sure the pins aren't too long */
    if (ulPinLen > PK11_MAX_PIN) {
	return CKR_PIN_LEN_RANGE;
    }
    if (ulPinLen < slot->minimumPinLen) {
	return CKR_PIN_LEN_RANGE;
    }

    if (nsslowkey_HasKeyDBPassword(handle) != SECFailure) {
	return CKR_DEVICE_ERROR;
    }

    /* convert to null terminated string */
    PORT_Memcpy(newPinStr,pPin,ulPinLen);
    newPinStr[ulPinLen] = 0; 

    /* build the hashed pins which we pass around */
    newPin = nsslowkey_HashPassword(newPinStr,handle->global_salt);
    PORT_Memset(newPinStr,0,sizeof(newPinStr));

    /* change the data base */
    rv = nsslowkey_SetKeyDBPassword(handle,newPin);

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
    NSSLOWKEYDBHandle *handle;
    SECItem *newPin;
    SECItem *oldPin;
    char newPinStr[PK11_MAX_PIN+1],oldPinStr[PK11_MAX_PIN+1];
    SECStatus rv;

    
    sp = pk11_SessionFromHandle(hSession);
    if (sp == NULL) {
	return CKR_SESSION_HANDLE_INVALID;
    }

    slot = pk11_SlotFromSession(sp);
    if (!slot) {
	pk11_FreeSession(sp);
	return CKR_SESSION_HANDLE_INVALID;;
    }

    handle = slot->keyDB;
    if (handle == NULL) {
	pk11_FreeSession(sp);
	return CKR_PIN_LEN_RANGE;
    }

    if (slot->needLogin && sp->info.state != CKS_RW_USER_FUNCTIONS) {
	pk11_FreeSession(sp);
	return CKR_USER_NOT_LOGGED_IN;
    }

    pk11_FreeSession(sp);

    /* make sure the pins aren't too long */
    if ((ulNewLen > PK11_MAX_PIN) || (ulOldLen > PK11_MAX_PIN)) {
	return CKR_PIN_LEN_RANGE;
    }
    if (ulNewLen < slot->minimumPinLen) {
	return CKR_PIN_LEN_RANGE;
    }


    /* convert to null terminated string */
    PORT_Memcpy(newPinStr,pNewPin,ulNewLen);
    newPinStr[ulNewLen] = 0; 
    PORT_Memcpy(oldPinStr,pOldPin,ulOldLen);
    oldPinStr[ulOldLen] = 0; 

    /* build the hashed pins which we pass around */
    newPin = nsslowkey_HashPassword(newPinStr,handle->global_salt);
    oldPin = nsslowkey_HashPassword(oldPinStr,handle->global_salt);
    PORT_Memset(newPinStr,0,sizeof(newPinStr));
    PORT_Memset(oldPinStr,0,sizeof(oldPinStr));

    /* change the data base */
    rv = nsslowkey_ChangeKeyDBPassword(handle,oldPin,newPin);

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
    sessionID = slot->sessionIDCount++ | (slot->index << 24);
    if (slot->readOnly && (flags & CKF_RW_SESSION)) {
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
    NSSLOWKEYDBHandle *handle;
    SECItem *pin;
    char pinStr[PK11_MAX_PIN+1];


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

    if (ulPinLen > PK11_MAX_PIN) return CKR_PIN_LEN_RANGE;

    /* convert to null terminated string */
    PORT_Memcpy(pinStr,pPin,ulPinLen);
    pinStr[ulPinLen] = 0; 

    handle = slot->keyDB;
    if (handle == NULL) {
	 return CKR_USER_TYPE_INVALID;
    }

    /*
     * Deal with bootstrap. We allow the SSO to login in with a NULL
     * password if and only if we haven't initialized the KEY DB yet.
     * We only allow this on a RW session.
     */
    if (nsslowkey_HasKeyDBPassword(handle) == SECFailure) {
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
    pin = nsslowkey_HashPassword(pinStr,handle->global_salt);
    if (pin == NULL) return CKR_HOST_MEMORY;

    if (nsslowkey_CheckKeyDBPassword(handle,pin) == SECSuccess) {
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
    *phObject = object->handle;
    pk11_FreeSession(session);
    pk11_FreeObject(object);

    return crv;
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
    pk11_FreeObject(destObject);
    
    return crv;
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
    return crv;
}

/*
 * find any certs that may match the template and load them.
 */
#define NSC_CERT	0x00000001
#define NSC_TRUST	0x00000002
#define NSC_CRL		0x00000004
#define NSC_SMIME	0x00000008
#define NSC_PRIVATE	0x00000010
#define NSC_PUBLIC	0x00000020
#define NSC_KEY		0x00000040

/*
 * structure to collect key handles.
 */
typedef struct pk11CrlDataStr {
    PK11Slot *slot;
    PK11SearchResults *searchHandles;
    CK_ATTRIBUTE *template;
    CK_ULONG templ_count;
} pk11CrlData;


static SECStatus
pk11_crl_collect(SECItem *data, SECItem *key, certDBEntryType type, void *arg)
{
    pk11CrlData *crlData;
    CK_OBJECT_HANDLE class_handle;
    PK11Slot *slot;
    
    crlData = (pk11CrlData *)arg;
    slot = crlData->slot;

    class_handle = (type == certDBEntryTypeRevocation) ? PK11_TOKEN_TYPE_CRL :
							PK11_TOKEN_KRL_HANDLE;
    if (pk11_tokenMatch(slot, key, class_handle,
			crlData->template, crlData->templ_count)) {
	pk11_addHandle(crlData->searchHandles,
				 pk11_mkHandle(slot,key,class_handle));
    }
    return(SECSuccess);
}

static void
pk11_searchCrls(PK11Slot *slot, SECItem *derSubject, PRBool isKrl, 
		unsigned long classFlags, PK11SearchResults *search,
		CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;

    certHandle = slot->certDB;
    if (certHandle == NULL) {
	return;
    }
    if (derSubject->data != NULL)  {
	SECItem *crl = 
	    nsslowcert_FindCrlByKey(certHandle,derSubject,NULL,isKrl);

	if (crl != NULL) {
	    pk11_addHandle(search, pk11_mkHandle(slot,derSubject,
		isKrl ? PK11_TOKEN_KRL_HANDLE : PK11_TOKEN_TYPE_CRL));
	}
    } else {
	pk11CrlData crlData;

	/* traverse */
	crlData.slot = slot;
	crlData.searchHandles = search;
	crlData.template = pTemplate;
	crlData.templ_count = ulCount;
	nsslowcert_TraverseDBEntries(certHandle, certDBEntryTypeRevocation,
		pk11_crl_collect, (void *)&crlData);
	nsslowcert_TraverseDBEntries(certHandle, certDBEntryTypeKeyRevocation,
		pk11_crl_collect, (void *)&crlData);
    } 
}

/*
 * structure to collect key handles.
 */
typedef struct pk11KeyDataStr {
    PK11Slot *slot;
    PK11SearchResults *searchHandles;
    SECItem *id;
    CK_ATTRIBUTE *template;
    CK_ULONG templ_count;
    unsigned long classFlags;
    PRBool isLoggedIn;
    PRBool strict;
} pk11KeyData;


static SECStatus
pk11_key_collect(DBT *key, DBT *data, void *arg)
{
    pk11KeyData *keyData;
    NSSLOWKEYPrivateKey *privKey = NULL;
    SECItem tmpDBKey;
    PK11Slot *slot;
    
    keyData = (pk11KeyData *)arg;
    slot = keyData->slot;

    tmpDBKey.data = key->data;
    tmpDBKey.len = key->size;

    PORT_Assert(slot->keyDB);
    if (!keyData->strict && keyData->id) {
	SECItem result;
	unsigned char hashKey[SHA1_LENGTH];
	result.data = hashKey;
	result.len = sizeof(hashKey);

	SHA1_HashBuf( hashKey, key->data, key->size );
	if (SECITEM_ItemsAreEqual(keyData->id,&result)) {
	    if (keyData->classFlags & NSC_PRIVATE)  {
		pk11_addHandle(keyData->searchHandles,
			pk11_mkHandle(slot,&tmpDBKey,PK11_TOKEN_TYPE_PRIV));
	    }
	    if (keyData->classFlags & NSC_PUBLIC) {
		pk11_addHandle(keyData->searchHandles,
			pk11_mkHandle(slot,&tmpDBKey,PK11_TOKEN_TYPE_PUB));
	    }
	    /* NSC_KEY Already handled */
	}
	return SECSuccess;
    }

    privKey = nsslowkey_FindKeyByPublicKey(keyData->slot->keyDB, &tmpDBKey, 
						 keyData->slot->password);
    if ( privKey == NULL ) {
	goto loser;
    }

    if (isSecretKey(privKey)) {
	if ((keyData->classFlags & NSC_KEY) && 
		pk11_tokenMatch(keyData->slot, &tmpDBKey, PK11_TOKEN_TYPE_KEY,
			keyData->template, keyData->templ_count)) {
	    pk11_addHandle(keyData->searchHandles,
		pk11_mkHandle(keyData->slot, &tmpDBKey, PK11_TOKEN_TYPE_KEY));
	}
    } else {
	if ((keyData->classFlags & NSC_PRIVATE) && 
		pk11_tokenMatch(keyData->slot, &tmpDBKey, PK11_TOKEN_TYPE_PRIV,
			keyData->template, keyData->templ_count)) {
	    pk11_addHandle(keyData->searchHandles,
		pk11_mkHandle(keyData->slot,&tmpDBKey,PK11_TOKEN_TYPE_PRIV));
	}
	if ((keyData->classFlags & NSC_PUBLIC) && 
		pk11_tokenMatch(keyData->slot, &tmpDBKey, PK11_TOKEN_TYPE_PUB,
			keyData->template, keyData->templ_count)) {
	    pk11_addHandle(keyData->searchHandles,
		pk11_mkHandle(keyData->slot, &tmpDBKey,PK11_TOKEN_TYPE_PUB));
	}
    }

loser:
    if ( privKey ) {
	nsslowkey_DestroyPrivateKey(privKey);
    }
    return(SECSuccess);
}

static void
pk11_searchKeys(PK11Slot *slot, SECItem *key_id, PRBool isLoggedIn,
	unsigned long classFlags, PK11SearchResults *search,
	CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount)
{
    NSSLOWKEYDBHandle *keyHandle = NULL;
    NSSLOWKEYPrivateKey *privKey;
    pk11KeyData keyData;

    keyHandle = slot->keyDB;
    if (keyHandle == NULL) {
	return;
    }

    if (key_id->data && (classFlags & NSC_KEY)) {
	privKey = nsslowkey_FindKeyByPublicKey(keyHandle,key_id, slot->password);
	if (privKey) {
    	    pk11_addHandle(search,pk11_mkHandle(slot,key_id,PK11_TOKEN_TYPE_KEY));
    	    nsslowkey_DestroyPrivateKey(privKey);
	}
	if ( !(classFlags & (NSC_PRIVATE|NSC_PUBLIC)) ) {
	    /* skip the traverse, nothing new to find */
	    return;
	}
    }
    keyData.slot = slot;
    keyData.searchHandles = search;
    keyData.id = key_id;
    keyData.template = pTemplate;
    keyData.templ_count = ulCount;
    keyData.isLoggedIn = isLoggedIn;
    keyData.classFlags = classFlags;
    keyData.strict = NSC_STRICT;

    nsslowkey_TraverseKeys(keyHandle, pk11_key_collect, &keyData);
}

/*
 * structure to collect certs into
 */
typedef struct pk11CertDataStr {
    PK11Slot *slot;
    int cert_count;
    int max_cert_count;
    NSSLOWCERTCertificate **certs;
    CK_ATTRIBUTE *template;
    CK_ULONG	templ_count;
    unsigned long classFlags;
    PRBool	strict;
} pk11CertData;

/*
 * collect all the certs from the traverse call.
 */	
static SECStatus
pk11_cert_collect(NSSLOWCERTCertificate *cert,void *arg)
{
    pk11CertData *cd = (pk11CertData *)arg;

    if (cd->certs == NULL) {
	return SECFailure;
    }

    if (cd->strict) {
	if ((cd->classFlags & NSC_CERT) && !pk11_tokenMatch(cd->slot,
	  &cert->certKey, PK11_TOKEN_TYPE_CERT, cd->template,cd->templ_count)) {
	    return SECSuccess;
	}
	if ((cd->classFlags & NSC_TRUST) && !pk11_tokenMatch(cd->slot,
	  &cert->certKey, PK11_TOKEN_TYPE_TRUST, 
					     cd->template, cd->templ_count)) {
	    return SECSuccess;
	}
    }

    /* allocate more space if we need it. This should only happen in
     * the general traversal case */
    if (cd->cert_count >= cd->max_cert_count) {
	int size;
	cd->max_cert_count += NSC_CERT_BLOCK_SIZE;
	size = cd->max_cert_count * sizeof (NSSLOWCERTCertificate *);
	cd->certs = (NSSLOWCERTCertificate **)PORT_Realloc(cd->certs,size);
	if (cd->certs == NULL) {
	    return SECFailure;
	}
    }

    cd->certs[cd->cert_count++] = nsslowcert_DupCertificate(cert);
    return SECSuccess;
}

/* provide impedence matching ... */
static SECStatus
pk11_cert_collect2(NSSLOWCERTCertificate *cert, SECItem *dymmy, void *arg)
{
    return pk11_cert_collect(cert, arg);
}

static void
pk11_searchSingleCert(pk11CertData *certData,NSSLOWCERTCertificate *cert)
{
    if (cert == NULL) {
	    return;
    }
    if (certData->strict && 
	!pk11_tokenMatch(certData->slot, &cert->certKey, PK11_TOKEN_TYPE_CERT, 
				certData->template,certData->templ_count)) {
	nsslowcert_DestroyCertificate(cert);
	return;
    }
    certData->certs = (NSSLOWCERTCertificate **) 
				PORT_Alloc(sizeof (NSSLOWCERTCertificate *));
    if (certData->certs == NULL) {
	nsslowcert_DestroyCertificate(cert);
	return;
    }
    certData->certs[0] = cert;
    certData->cert_count = 1;
}

static void
pk11_CertSetupData(pk11CertData *certData,int count)
{
    certData->max_cert_count = count;

    if (certData->max_cert_count <= 0) {
	return;
    }
    certData->certs = (NSSLOWCERTCertificate **)
			 PORT_Alloc( count * sizeof(NSSLOWCERTCertificate *));
    return;
}

static void
pk11_searchCertsAndTrust(PK11Slot *slot, SECItem *derCert, SECItem *name, 
			SECItem *derSubject, NSSLOWCERTIssuerAndSN *issuerSN, 
			SECItem *email,
			unsigned long classFlags, PK11SearchResults *handles, 
			CK_ATTRIBUTE *pTemplate, CK_LONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;
    pk11CertData certData;
    int i;

    certHandle = slot->certDB;
    if (certHandle == NULL) return;

    certData.slot = slot;
    certData.max_cert_count = 0;
    certData.certs = NULL;
    certData.cert_count = 0;
    certData.template = pTemplate;
    certData.templ_count = ulCount;
    certData.classFlags = classFlags; 
    certData.strict = NSC_STRICT; 


    /*
     * Find the Cert.
     */
    if (derCert->data != NULL) {
	NSSLOWCERTCertificate *cert = 
			nsslowcert_FindCertByDERCert(certHandle,derCert);
	pk11_searchSingleCert(&certData,cert);
    } else if (name->data != NULL) {
	char *tmp_name = (char*)PORT_Alloc(name->len+1);
	int count;

	if (tmp_name == NULL) {
	    return;
	}
	PORT_Memcpy(tmp_name,name->data,name->len);
	tmp_name[name->len] = 0;

	count= nsslowcert_NumPermCertsForNickname(certHandle,tmp_name);
	pk11_CertSetupData(&certData,count);
	nsslowcert_TraversePermCertsForNickname(certHandle,tmp_name,
				pk11_cert_collect, &certData);
	PORT_Free(tmp_name);
    } else if (derSubject->data != NULL) {
	int count;

	count = nsslowcert_NumPermCertsForSubject(certHandle,derSubject);
	pk11_CertSetupData(&certData,count);
	nsslowcert_TraversePermCertsForSubject(certHandle,derSubject,
				pk11_cert_collect, &certData);
    } else if ((issuerSN->derIssuer.data != NULL) && 
			(issuerSN->serialNumber.data != NULL)) {
	NSSLOWCERTCertificate *cert = 
		nsslowcert_FindCertByIssuerAndSN(certHandle,issuerSN);

	pk11_searchSingleCert(&certData,cert);
    } else if (email->data != NULL) {
	char *tmp_name = (char*)PORT_Alloc(email->len+1);
	certDBEntrySMime *entry = NULL;

	if (tmp_name == NULL) {
	    return;
	}
	PORT_Memcpy(tmp_name,email->data,email->len);
	tmp_name[email->len] = 0;

	entry = nsslowcert_ReadDBSMimeEntry(certHandle,tmp_name);
	if (entry) {
	    int count;
	    SECItem *subjectName = &entry->subjectName;

	    count = nsslowcert_NumPermCertsForSubject(certHandle, subjectName);
	    pk11_CertSetupData(&certData,count);
	    nsslowcert_TraversePermCertsForSubject(certHandle, subjectName, 
						pk11_cert_collect, &certData);

	    nsslowcert_DestroyDBEntry((certDBEntry *)entry);
	}
	PORT_Free(tmp_name);
    } else {
	/* we aren't filtering the certs, we are working on all, so turn
	 * on the strict filters. */
        certData.strict = PR_TRUE;
	pk11_CertSetupData(&certData,NSC_CERT_BLOCK_SIZE);
	nsslowcert_TraversePermCerts(certHandle, pk11_cert_collect2, &certData);
    }

    /*
     * build the handles
     */	
    for (i=0 ; i < certData.cert_count ; i++) {
	NSSLOWCERTCertificate *cert = certData.certs[i];

	/* if we filtered it would have been on the stuff above */
	if (classFlags & NSC_CERT) {
	    pk11_addHandle(handles,
		pk11_mkHandle(slot,&cert->certKey,PK11_TOKEN_TYPE_CERT));
	}
	if ((classFlags & NSC_TRUST) && nsslowcert_hasTrust(cert)) {
	    pk11_addHandle(handles,
		pk11_mkHandle(slot,&cert->certKey,PK11_TOKEN_TYPE_TRUST));
	}
	nsslowcert_DestroyCertificate(cert);
    }

    if (certData.certs) PORT_Free(certData.certs);
    return;
}

static void
pk11_searchSMime(PK11Slot *slot, SECItem *email, PK11SearchResults *handles, 
			CK_ATTRIBUTE *pTemplate, CK_LONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;
    certDBEntrySMime *entry;

    certHandle = slot->certDB;
    if (certHandle == NULL) return;

    if (email->data != NULL) {
	char *tmp_name = (char*)PORT_Alloc(email->len+1);

	if (tmp_name == NULL) {
	    return;
	}
	PORT_Memcpy(tmp_name,email->data,email->len);
	tmp_name[email->len] = 0;

	entry = nsslowcert_ReadDBSMimeEntry(certHandle,tmp_name);
	if (entry) {
	    SECItem emailKey;

	    emailKey.data = (unsigned char *)tmp_name;
	    emailKey.len = PORT_Strlen(tmp_name)+1;
	    pk11_addHandle(handles,
		pk11_mkHandle(slot,&emailKey,PK11_TOKEN_TYPE_SMIME));
	    nsslowcert_DestroyDBEntry((certDBEntry *)entry);
	}
	PORT_Free(tmp_name);
    }
    return;
}

static void
pk11_searchSMimeForCert(PK11Slot *slot, SECItem *email, 
			unsigned long classFlags, 
                        PK11SearchResults *handles, 
			CK_ATTRIBUTE *pTemplate, CK_LONG ulCount)
{
    NSSLOWCERTCertDBHandle *certHandle = NULL;
    certDBEntrySMime *entry;
    int i;

    certHandle = slot->certDB;
    if (certHandle == NULL) return;

    if (email->data != NULL) {
	char *tmp_name = (char*)PORT_Alloc(email->len+1);

	if (tmp_name == NULL) {
	    return;
	}
	PORT_Memcpy(tmp_name,email->data,email->len);
	tmp_name[email->len] = 0;

	entry = nsslowcert_ReadDBSMimeEntry(certHandle,tmp_name);
	if (entry) {
	    int count;
	    pk11CertData certData;
	    certData.slot = slot;
	    certData.max_cert_count = 0;
	    certData.certs = NULL;
	    certData.cert_count = 0;
	    certData.template = pTemplate;
	    certData.templ_count = ulCount;
	    certData.classFlags = classFlags; 
	    certData.strict = NSC_STRICT; 
	    count = nsslowcert_NumPermCertsForSubject(certHandle,&entry->subjectName);
	    pk11_CertSetupData(&certData,count);
	    nsslowcert_TraversePermCertsForSubject(certHandle,&entry->subjectName,
				                pk11_cert_collect, &certData);

	    /*
	     * build the handles
	     */	
	    for (i=0 ; i < certData.cert_count ; i++) {
		NSSLOWCERTCertificate *cert = certData.certs[i];
		pk11_addHandle(handles,
		      pk11_mkHandle(slot,&cert->certKey,PK11_TOKEN_TYPE_CERT));
		nsslowcert_DestroyCertificate(cert);
	    }

	    nsslowcert_DestroyDBEntry((certDBEntry *)entry);
	    if (certData.certs) PORT_Free(certData.certs);
	}
	PORT_Free(tmp_name);
    }
    return;
}


static CK_RV
pk11_searchTokenList(PK11Slot *slot, PK11SearchResults *search,
	 		CK_ATTRIBUTE *pTemplate, CK_LONG ulCount, 
			PRBool *tokenOnly, PRBool isLoggedIn)
{
    int i;
    PRBool isKrl = PR_FALSE;
    SECItem derCert = { siBuffer, NULL, 0 };
    SECItem derSubject = { siBuffer, NULL, 0 };
    SECItem name = { siBuffer, NULL, 0 };
    SECItem email = { siBuffer, NULL, 0 };
    SECItem key_id = { siBuffer, NULL, 0 };
    SECItem cert_sha1_hash = { siBuffer, NULL, 0 };
    SECItem cert_md5_hash  = { siBuffer, NULL, 0 };
    NSSLOWCERTIssuerAndSN issuerSN = {
	{ siBuffer, NULL, 0 },
	{ siBuffer, NULL, 0 }
    };
    SECItem *copy = NULL;
    unsigned long classFlags = 
	NSC_CERT|NSC_TRUST|NSC_PRIVATE|NSC_PUBLIC|NSC_KEY|NSC_SMIME;

    /* if we aren't logged in, don't look for private or secret keys */
    if (!isLoggedIn) {
	classFlags &= ~(NSC_PRIVATE|NSC_KEY);
    }

    /*
     * look for things to search on token objects for. If the right options
     * are specified, we can use them as direct indeces into the database
     * (rather than using linear searches. We can also use the attributes to
     * limit the kinds of objects we are searching for. Later we can use this
     * array to filter the remaining objects more finely.
     */
    for (i=0 ;classFlags && i < (int)ulCount; i++) {

	switch (pTemplate[i].type) {
	case CKA_SUBJECT:
	    copy = &derSubject;
	    classFlags &= (NSC_CERT|NSC_PRIVATE|NSC_PUBLIC|NSC_SMIME);
	    break;
	case CKA_ISSUER: 
	    copy = &issuerSN.derIssuer;
	    classFlags &= (NSC_CERT|NSC_CRL|NSC_TRUST);
	    break;
	case CKA_SERIAL_NUMBER: 
	    copy = &issuerSN.serialNumber;
	    classFlags &= (NSC_CERT|NSC_TRUST);
	    break;
	case CKA_VALUE:
	    copy = &derCert;
	    classFlags &= (NSC_CERT|NSC_CRL|NSC_SMIME);
	    break;
	case CKA_LABEL:
	    copy = &name;
	    break;
	case CKA_NETSCAPE_EMAIL:
	    copy = &email;
	    classFlags &= NSC_SMIME|NSC_CERT;
	    break;
	case CKA_NETSCAPE_SMIME_TIMESTAMP:
	    classFlags &= NSC_SMIME;
	    break;
	case CKA_CLASS:
	    if (pTemplate[i].ulValueLen != sizeof(CK_OBJECT_CLASS)) {
		classFlags = 0;
		break;;
	    }
	    switch (*((CK_OBJECT_CLASS *)pTemplate[i].pValue)) {
	    case CKO_CERTIFICATE:
		classFlags &= NSC_CERT;
		break;
	    case CKO_NETSCAPE_TRUST:
		classFlags &= NSC_TRUST;
		break;
	    case CKO_NETSCAPE_CRL:
		classFlags &= NSC_CRL;
		break;
	    case CKO_NETSCAPE_SMIME:
		classFlags &= NSC_SMIME;
		break;
	    case CKO_PRIVATE_KEY:
		classFlags &= NSC_PRIVATE;
		break;
	    case CKO_PUBLIC_KEY:
		classFlags &= NSC_PUBLIC;
		break;
	    case CKO_SECRET_KEY:
		classFlags &= NSC_KEY;
		break;
	    default:
		classFlags = 0;
		break;
	    }
	    break;
	case CKA_PRIVATE:
	    if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
		classFlags = 0;
	    }
	    if (*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE) {
		classFlags &= (NSC_PRIVATE|NSC_KEY);
	    } else {
		classFlags &= ~(NSC_PRIVATE|NSC_KEY);
	    }
	    break;
	case CKA_SENSITIVE:
	    if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
		classFlags = 0;
	    }
	    if (*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE) {
		classFlags &= (NSC_PRIVATE|NSC_KEY);
	    } else {
		classFlags = 0;
	    }
	    break;
	case CKA_TOKEN:
	    if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
		classFlags = 0;
	    }
	    if (*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE) {
		*tokenOnly = PR_TRUE;
	    } else {
		classFlags = 0;
	    }
	    break;
	case CKA_CERT_SHA1_HASH:
	    classFlags &= NSC_TRUST;
	    copy = &cert_sha1_hash; break;
	case CKA_CERT_MD5_HASH:
	    classFlags &= NSC_TRUST;
	    copy = &cert_md5_hash; break;
	case CKA_CERTIFICATE_TYPE:
	    if (pTemplate[i].ulValueLen != sizeof(CK_CERTIFICATE_TYPE)) {
		classFlags = 0;
	    }
	    classFlags &= NSC_CERT;
	    if (*((CK_CERTIFICATE_TYPE *)pTemplate[i].pValue) != CKC_X_509) {
		classFlags = 0;
	    }
	    break;
	case CKA_ID:
	    copy = &key_id;
	    classFlags &= (NSC_CERT|NSC_PRIVATE|NSC_KEY|NSC_PUBLIC);
	    break;
	case CKA_NETSCAPE_KRL:
	    if (pTemplate[i].ulValueLen != sizeof(CK_BBOOL)) {
		classFlags = 0;
	    }
	    classFlags &= NSC_CRL;
	    isKrl = (PRBool)(*((CK_BBOOL *)pTemplate[i].pValue) == CK_TRUE);
	    break;
	case CKA_MODIFIABLE:
	     break;
	case CKA_KEY_TYPE:
	case CKA_DERIVE:
	    classFlags &= NSC_PUBLIC|NSC_PRIVATE|NSC_KEY;
	    break;
	case CKA_VERIFY_RECOVER:
	    classFlags &= NSC_PUBLIC;
	    break;
	case CKA_SIGN_RECOVER:
	    classFlags &= NSC_PRIVATE;
	    break;
	case CKA_ENCRYPT:
	case CKA_VERIFY:
	case CKA_WRAP:
	    classFlags &= NSC_PUBLIC|NSC_KEY;
	    break;
	case CKA_DECRYPT:
	case CKA_SIGN:
	case CKA_UNWRAP:
	case CKA_ALWAYS_SENSITIVE:
	case CKA_EXTRACTABLE:
	case CKA_NEVER_EXTRACTABLE:
	    classFlags &= NSC_PRIVATE|NSC_KEY;
	    break;
	/* can't be a certificate if it doesn't match one of the above 
	 * attributes */
	default: 
	     classFlags  = 0;
	     break;
	}
 	if (copy) {
	    copy->data = (unsigned char*)pTemplate[i].pValue;
	    copy->len = pTemplate[i].ulValueLen;
	}
	copy = NULL;
    }


    /* certs */
    if (classFlags & (NSC_CERT|NSC_TRUST)) {
	pk11_searchCertsAndTrust(slot,&derCert,&name,&derSubject,
				 &issuerSN, &email,classFlags,search, 
				pTemplate, ulCount);
    }

    /* keys */
    if (classFlags & (NSC_PRIVATE|NSC_PUBLIC|NSC_KEY)) {
	pk11_searchKeys(slot, &key_id, isLoggedIn, classFlags, search,
			pTemplate, ulCount);
    }

    /* crl's */
    if (classFlags & NSC_CRL) {
	pk11_searchCrls(slot, &derSubject, isKrl, classFlags, search,
			pTemplate, ulCount);
    }
    /* Add S/MIME entry stuff */
    if (classFlags & NSC_SMIME) {
	pk11_searchSMime(slot, &email, search, pTemplate, ulCount);
    }
    return CKR_OK;
}
	

/* NSC_FindObjectsInit initializes a search for token and session objects 
 * that match a template. */
CK_RV NSC_FindObjectsInit(CK_SESSION_HANDLE hSession,
    			CK_ATTRIBUTE_PTR pTemplate,CK_ULONG ulCount)
{
    PK11SearchResults *search,*freeSearch;
    PK11Session *session = NULL;
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PRBool tokenOnly = PR_FALSE;
    CK_RV crv = CKR_OK;
    PRBool isLoggedIn;
    
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	crv = CKR_SESSION_HANDLE_INVALID;
	goto loser;
    }
   
    search = (PK11SearchResults *)PORT_Alloc(sizeof(PK11SearchResults));
    if (search == NULL) {
	crv = CKR_HOST_MEMORY;
	goto loser;
    }
    search->handles = (CK_OBJECT_HANDLE *)
		PORT_Alloc(sizeof(CK_OBJECT_HANDLE) * NSC_SEARCH_BLOCK_SIZE);
    if (search->handles == NULL) {
	crv = CKR_HOST_MEMORY;
	goto loser;
    }
    search->index = 0;
    search->size = 0;
    search->array_size = NSC_SEARCH_BLOCK_SIZE;
    isLoggedIn = (PRBool)((!slot->needLogin) || slot->isLoggedIn);

    crv = pk11_searchTokenList(slot, search, pTemplate, ulCount, &tokenOnly,
								isLoggedIn);
    if (crv != CKR_OK) {
	goto loser;
    }
    
    /* build list of found objects in the session */
    if (!tokenOnly) {
	crv = pk11_searchObjectList(search, slot->tokObjects,
			slot->objectLock, pTemplate, ulCount, isLoggedIn);
    }
    if (crv != CKR_OK) {
	goto loser;
    }

    if ((freeSearch = session->search) != NULL) {
	session->search = NULL;
	pk11_FreeSearch(freeSearch);
    }
    session->search = search;
    pk11_FreeSession(session);
    return CKR_OK;

loser:
    if (freeSearch) {
	pk11_FreeSearch(freeSearch);
    }
    if (session) {
	pk11_FreeSession(session);
    }
    return crv;
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
    if (transfer > 0) {
	PORT_Memcpy(phObject,&search->handles[search->index],
                                        transfer*sizeof(CK_OBJECT_HANDLE_PTR));
    } else {
       *phObject = CK_INVALID_HANDLE;
    }

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
