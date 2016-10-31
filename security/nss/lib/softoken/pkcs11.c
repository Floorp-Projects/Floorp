/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements PKCS 11 on top of our existing security modules
 *
 * For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *   This implementation has two slots:
 *      slot 1 is our generic crypto support. It does not require login.
 *   It supports Public Key ops, and all they bulk ciphers and hashes.
 *   It can also support Private Key ops for imported Private keys. It does
 *   not have any token storage.
 *      slot 2 is our private key support. It requires a login before use. It
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
#include "secrng.h"
#include "prtypes.h"
#include "nspr.h"
#include "softkver.h"
#include "secoid.h"
#include "sftkdb.h"
#include "utilpars.h"
#include "ec.h"
#include "secasn1.h"
#include "secerr.h"
#include "lgglue.h"

PRBool parentForkedAfterC_Initialize;

#ifndef NO_FORK_CHECK

PRBool sftkForkCheckDisabled;

#if defined(CHECK_FORK_PTHREAD) || defined(CHECK_FORK_MIXED)
PRBool forked = PR_FALSE;
#endif

#if defined(CHECK_FORK_GETPID) || defined(CHECK_FORK_MIXED)
#include <unistd.h>
pid_t myPid;
#endif

#ifdef CHECK_FORK_MIXED
#include <sys/systeminfo.h>
PRBool usePthread_atfork;
#endif

#endif

/*
 * ******************** Static data *******************************
 */

/* The next three strings must be exactly 32 characters long */
static char *manufacturerID = "Mozilla Foundation              ";
static char manufacturerID_space[33];
static char *libraryDescription = "NSS Internal Crypto Services    ";
static char libraryDescription_space[33];

/*
 * In FIPS mode, we disallow login attempts for 1 second after a login
 * failure so that there are at most 60 login attempts per minute.
 */
static PRIntervalTime loginWaitTime;
static PRUint32 minSessionObjectHandle = 1U;

#define __PASTE(x, y) x##y

/*
 * we renamed all our internal functions, get the correct
 * definitions for them...
 */
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_EXTERN extern
#define CK_PKCS11_FUNCTION_INFO(func) \
    CK_RV __PASTE(NS, func)
#define CK_NEED_ARG_LIST 1

#include "pkcs11f.h"

/* build the crypto module table */
static const CK_FUNCTION_LIST sftk_funcList = {
    { 1, 10 },

#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_PKCS11_FUNCTION_INFO(func) \
    __PASTE(NS, func)                 \
    ,
#include "pkcs11f.h"

};

#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#undef __PASTE

/* List of DES Weak Keys */
typedef unsigned char desKey[8];
static const desKey sftk_desWeakTable[] = {
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

static const int sftk_desWeakTableSize = sizeof(sftk_desWeakTable) /
                                         sizeof(sftk_desWeakTable[0]);

/* DES KEY Parity conversion table. Takes each byte/2 as an index, returns
 * that byte with the proper parity bit set */
static const unsigned char parityTable[256] = {
    /* Even...0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e */
    /* E */ 0x01, 0x02, 0x04, 0x07, 0x08, 0x0b, 0x0d, 0x0e,
    /* Odd....0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e */
    /* O */ 0x10, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c, 0x1f,
    /* Odd....0x20,0x22,0x24,0x26,0x28,0x2a,0x2c,0x2e */
    /* O */ 0x20, 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x2f,
    /* Even...0x30,0x32,0x34,0x36,0x38,0x3a,0x3c,0x3e */
    /* E */ 0x31, 0x32, 0x34, 0x37, 0x38, 0x3b, 0x3d, 0x3e,
    /* Odd....0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x4e */
    /* O */ 0x40, 0x43, 0x45, 0x46, 0x49, 0x4a, 0x4c, 0x4f,
    /* Even...0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x5e */
    /* E */ 0x51, 0x52, 0x54, 0x57, 0x58, 0x5b, 0x5d, 0x5e,
    /* Even...0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e */
    /* E */ 0x61, 0x62, 0x64, 0x67, 0x68, 0x6b, 0x6d, 0x6e,
    /* Odd....0x70,0x72,0x74,0x76,0x78,0x7a,0x7c,0x7e */
    /* O */ 0x70, 0x73, 0x75, 0x76, 0x79, 0x7a, 0x7c, 0x7f,
    /* Odd....0x80,0x82,0x84,0x86,0x88,0x8a,0x8c,0x8e */
    /* O */ 0x80, 0x83, 0x85, 0x86, 0x89, 0x8a, 0x8c, 0x8f,
    /* Even...0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e */
    /* E */ 0x91, 0x92, 0x94, 0x97, 0x98, 0x9b, 0x9d, 0x9e,
    /* Even...0xa0,0xa2,0xa4,0xa6,0xa8,0xaa,0xac,0xae */
    /* E */ 0xa1, 0xa2, 0xa4, 0xa7, 0xa8, 0xab, 0xad, 0xae,
    /* Odd....0xb0,0xb2,0xb4,0xb6,0xb8,0xba,0xbc,0xbe */
    /* O */ 0xb0, 0xb3, 0xb5, 0xb6, 0xb9, 0xba, 0xbc, 0xbf,
    /* Even...0xc0,0xc2,0xc4,0xc6,0xc8,0xca,0xcc,0xce */
    /* E */ 0xc1, 0xc2, 0xc4, 0xc7, 0xc8, 0xcb, 0xcd, 0xce,
    /* Odd....0xd0,0xd2,0xd4,0xd6,0xd8,0xda,0xdc,0xde */
    /* O */ 0xd0, 0xd3, 0xd5, 0xd6, 0xd9, 0xda, 0xdc, 0xdf,
    /* Odd....0xe0,0xe2,0xe4,0xe6,0xe8,0xea,0xec,0xee */
    /* O */ 0xe0, 0xe3, 0xe5, 0xe6, 0xe9, 0xea, 0xec, 0xef,
    /* Even...0xf0,0xf2,0xf4,0xf6,0xf8,0xfa,0xfc,0xfe */
    /* E */ 0xf1, 0xf2, 0xf4, 0xf7, 0xf8, 0xfb, 0xfd, 0xfe,
};

/* Mechanisms */
struct mechanismList {
    CK_MECHANISM_TYPE type;
    CK_MECHANISM_INFO info;
    PRBool privkey;
};

/*
 * the following table includes a complete list of mechanism defined by
 * PKCS #11 version 2.01. Those Mechanisms not supported by this PKCS #11
 * module are ifdef'ed out.
 */
#define CKF_EN_DE CKF_ENCRYPT | CKF_DECRYPT
#define CKF_WR_UN CKF_WRAP | CKF_UNWRAP
#define CKF_SN_VR CKF_SIGN | CKF_VERIFY
#define CKF_SN_RE CKF_SIGN_RECOVER | CKF_VERIFY_RECOVER

#define CKF_EN_DE_WR_UN CKF_EN_DE | CKF_WR_UN
#define CKF_SN_VR_RE CKF_SN_VR | CKF_SN_RE
#define CKF_DUZ_IT_ALL CKF_EN_DE_WR_UN | CKF_SN_VR_RE

#define CKF_EC_PNU CKF_EC_FP | CKF_EC_NAMEDCURVE | CKF_EC_UNCOMPRESS

#define CKF_EC_BPNU CKF_EC_F_2M | CKF_EC_PNU

#define CK_MAX 0xffffffff

static const struct mechanismList mechanisms[] = {

    /*
      * PKCS #11 Mechanism List.
      *
      * The first argument is the PKCS #11 Mechanism we support.
      * The second argument is Mechanism info structure. It includes:
      *    The minimum key size,
      *       in bits for RSA, DSA, DH, EC*, KEA, RC2 and RC4 * algs.
      *       in bytes for RC5, AES, Camellia, and CAST*
      *       ignored for DES*, IDEA and FORTEZZA based
      *    The maximum key size,
      *       in bits for RSA, DSA, DH, EC*, KEA, RC2 and RC4 * algs.
      *       in bytes for RC5, AES, Camellia, and CAST*
      *       ignored for DES*, IDEA and FORTEZZA based
      *     Flags
      *       What operations are supported by this mechanism.
      *  The third argument is a bool which tells if this mechanism is
      *    supported in the database token.
      *
      */

    /* ------------------------- RSA Operations ---------------------------*/
    { CKM_RSA_PKCS_KEY_PAIR_GEN, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_GENERATE_KEY_PAIR }, PR_TRUE },
    { CKM_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_DUZ_IT_ALL }, PR_TRUE },
    { CKM_RSA_PKCS_PSS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_RSA_PKCS_OAEP, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_EN_DE_WR_UN }, PR_TRUE },
#ifdef SFTK_RSA9796_SUPPORTED
    { CKM_RSA_9796, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_DUZ_IT_ALL }, PR_TRUE },
#endif
    { CKM_RSA_X_509, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_DUZ_IT_ALL }, PR_TRUE },
    /* -------------- RSA Multipart Signing Operations -------------------- */
    { CKM_MD2_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_MD5_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA1_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA224_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA256_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA384_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA512_RSA_PKCS, { RSA_MIN_MODULUS_BITS, CK_MAX, CKF_SN_VR }, PR_TRUE },
    /* ------------------------- DSA Operations --------------------------- */
    { CKM_DSA_KEY_PAIR_GEN, { DSA_MIN_P_BITS, DSA_MAX_P_BITS, CKF_GENERATE_KEY_PAIR }, PR_TRUE },
    { CKM_DSA, { DSA_MIN_P_BITS, DSA_MAX_P_BITS, CKF_SN_VR }, PR_TRUE },
    { CKM_DSA_PARAMETER_GEN, { DSA_MIN_P_BITS, DSA_MAX_P_BITS, CKF_GENERATE }, PR_TRUE },
    { CKM_DSA_SHA1, { DSA_MIN_P_BITS, DSA_MAX_P_BITS, CKF_SN_VR }, PR_TRUE },
    /* -------------------- Diffie Hellman Operations --------------------- */
    /* no diffie hellman yet */
    { CKM_DH_PKCS_KEY_PAIR_GEN, { DH_MIN_P_BITS, DH_MAX_P_BITS, CKF_GENERATE_KEY_PAIR }, PR_TRUE },
    { CKM_DH_PKCS_DERIVE, { DH_MIN_P_BITS, DH_MAX_P_BITS, CKF_DERIVE }, PR_TRUE },
#ifndef NSS_DISABLE_ECC
    /* -------------------- Elliptic Curve Operations --------------------- */
    { CKM_EC_KEY_PAIR_GEN, { EC_MIN_KEY_BITS, EC_MAX_KEY_BITS, CKF_GENERATE_KEY_PAIR | CKF_EC_BPNU }, PR_TRUE },
    { CKM_ECDH1_DERIVE, { EC_MIN_KEY_BITS, EC_MAX_KEY_BITS, CKF_DERIVE | CKF_EC_BPNU }, PR_TRUE },
    { CKM_ECDSA, { EC_MIN_KEY_BITS, EC_MAX_KEY_BITS, CKF_SN_VR | CKF_EC_BPNU }, PR_TRUE },
    { CKM_ECDSA_SHA1, { EC_MIN_KEY_BITS, EC_MAX_KEY_BITS, CKF_SN_VR | CKF_EC_BPNU }, PR_TRUE },
#endif /* NSS_DISABLE_ECC */
    /* ------------------------- RC2 Operations --------------------------- */
    { CKM_RC2_KEY_GEN, { 1, 128, CKF_GENERATE }, PR_TRUE },
    { CKM_RC2_ECB, { 1, 128, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_RC2_CBC, { 1, 128, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_RC2_MAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_RC2_MAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_RC2_CBC_PAD, { 1, 128, CKF_EN_DE_WR_UN }, PR_TRUE },
    /* ------------------------- RC4 Operations --------------------------- */
    { CKM_RC4_KEY_GEN, { 1, 256, CKF_GENERATE }, PR_FALSE },
    { CKM_RC4, { 1, 256, CKF_EN_DE_WR_UN }, PR_FALSE },
    /* ------------------------- DES Operations --------------------------- */
    { CKM_DES_KEY_GEN, { 8, 8, CKF_GENERATE }, PR_TRUE },
    { CKM_DES_ECB, { 8, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_DES_CBC, { 8, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_DES_MAC, { 8, 8, CKF_SN_VR }, PR_TRUE },
    { CKM_DES_MAC_GENERAL, { 8, 8, CKF_SN_VR }, PR_TRUE },
    { CKM_DES_CBC_PAD, { 8, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_DES2_KEY_GEN, { 24, 24, CKF_GENERATE }, PR_TRUE },
    { CKM_DES3_KEY_GEN, { 24, 24, CKF_GENERATE }, PR_TRUE },
    { CKM_DES3_ECB, { 24, 24, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_DES3_CBC, { 24, 24, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_DES3_MAC, { 24, 24, CKF_SN_VR }, PR_TRUE },
    { CKM_DES3_MAC_GENERAL, { 24, 24, CKF_SN_VR }, PR_TRUE },
    { CKM_DES3_CBC_PAD, { 24, 24, CKF_EN_DE_WR_UN }, PR_TRUE },
    /* ------------------------- CDMF Operations --------------------------- */
    { CKM_CDMF_KEY_GEN, { 8, 8, CKF_GENERATE }, PR_TRUE },
    { CKM_CDMF_ECB, { 8, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CDMF_CBC, { 8, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CDMF_MAC, { 8, 8, CKF_SN_VR }, PR_TRUE },
    { CKM_CDMF_MAC_GENERAL, { 8, 8, CKF_SN_VR }, PR_TRUE },
    { CKM_CDMF_CBC_PAD, { 8, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    /* ------------------------- AES Operations --------------------------- */
    { CKM_AES_KEY_GEN, { 16, 32, CKF_GENERATE }, PR_TRUE },
    { CKM_AES_ECB, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_AES_CBC, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_AES_MAC, { 16, 32, CKF_SN_VR }, PR_TRUE },
    { CKM_AES_MAC_GENERAL, { 16, 32, CKF_SN_VR }, PR_TRUE },
    { CKM_AES_CBC_PAD, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_AES_CTS, { 16, 32, CKF_EN_DE }, PR_TRUE },
    { CKM_AES_CTR, { 16, 32, CKF_EN_DE }, PR_TRUE },
    { CKM_AES_GCM, { 16, 32, CKF_EN_DE }, PR_TRUE },
    /* ------------------------- Camellia Operations --------------------- */
    { CKM_CAMELLIA_KEY_GEN, { 16, 32, CKF_GENERATE }, PR_TRUE },
    { CKM_CAMELLIA_ECB, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAMELLIA_CBC, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAMELLIA_MAC, { 16, 32, CKF_SN_VR }, PR_TRUE },
    { CKM_CAMELLIA_MAC_GENERAL, { 16, 32, CKF_SN_VR }, PR_TRUE },
    { CKM_CAMELLIA_CBC_PAD, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    /* ------------------------- SEED Operations --------------------------- */
    { CKM_SEED_KEY_GEN, { 16, 16, CKF_GENERATE }, PR_TRUE },
    { CKM_SEED_ECB, { 16, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_SEED_CBC, { 16, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_SEED_MAC, { 16, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_SEED_MAC_GENERAL, { 16, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_SEED_CBC_PAD, { 16, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
#ifndef NSS_DISABLE_CHACHAPOLY
    /* ------------------------- ChaCha20 Operations ---------------------- */
    { CKM_NSS_CHACHA20_KEY_GEN, { 32, 32, CKF_GENERATE }, PR_TRUE },
    { CKM_NSS_CHACHA20_POLY1305, { 32, 32, CKF_EN_DE }, PR_TRUE },
#endif /* NSS_DISABLE_CHACHAPOLY */
    /* ------------------------- Hashing Operations ----------------------- */
    { CKM_MD2, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_MD2_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_MD2_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_MD5, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_MD5_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_MD5_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA_1, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_SHA_1_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA_1_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA224, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_SHA224_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA224_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA256, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_SHA256_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA256_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA384, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_SHA384_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA384_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA512, { 0, 0, CKF_DIGEST }, PR_FALSE },
    { CKM_SHA512_HMAC, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_SHA512_HMAC_GENERAL, { 1, 128, CKF_SN_VR }, PR_TRUE },
    { CKM_TLS_PRF_GENERAL, { 0, 512, CKF_SN_VR }, PR_FALSE },
    { CKM_TLS_MAC, { 0, 512, CKF_SN_VR }, PR_FALSE },
    { CKM_NSS_TLS_PRF_GENERAL_SHA256,
      { 0, 512, CKF_SN_VR },
      PR_FALSE },
    /* ------------------------- HKDF Operations -------------------------- */
    { CKM_NSS_HKDF_SHA1, { 1, 128, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_HKDF_SHA256, { 1, 128, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_HKDF_SHA384, { 1, 128, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_HKDF_SHA512, { 1, 128, CKF_DERIVE }, PR_TRUE },
/* ------------------------- CAST Operations --------------------------- */
#ifdef NSS_SOFTOKEN_DOES_CAST
    /* Cast operations are not supported ( yet? ) */
    { CKM_CAST_KEY_GEN, { 1, 8, CKF_GENERATE }, PR_TRUE },
    { CKM_CAST_ECB, { 1, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST_CBC, { 1, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST_MAC, { 1, 8, CKF_SN_VR }, PR_TRUE },
    { CKM_CAST_MAC_GENERAL, { 1, 8, CKF_SN_VR }, PR_TRUE },
    { CKM_CAST_CBC_PAD, { 1, 8, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST3_KEY_GEN, { 1, 16, CKF_GENERATE }, PR_TRUE },
    { CKM_CAST3_ECB, { 1, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST3_CBC, { 1, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST3_MAC, { 1, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_CAST3_MAC_GENERAL, { 1, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_CAST3_CBC_PAD, { 1, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST5_KEY_GEN, { 1, 16, CKF_GENERATE }, PR_TRUE },
    { CKM_CAST5_ECB, { 1, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST5_CBC, { 1, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_CAST5_MAC, { 1, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_CAST5_MAC_GENERAL, { 1, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_CAST5_CBC_PAD, { 1, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
#endif
#if NSS_SOFTOKEN_DOES_RC5
    /* ------------------------- RC5 Operations --------------------------- */
    { CKM_RC5_KEY_GEN, { 1, 32, CKF_GENERATE }, PR_TRUE },
    { CKM_RC5_ECB, { 1, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_RC5_CBC, { 1, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_RC5_MAC, { 1, 32, CKF_SN_VR }, PR_TRUE },
    { CKM_RC5_MAC_GENERAL, { 1, 32, CKF_SN_VR }, PR_TRUE },
    { CKM_RC5_CBC_PAD, { 1, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
#endif
#ifdef NSS_SOFTOKEN_DOES_IDEA
    /* ------------------------- IDEA Operations -------------------------- */
    { CKM_IDEA_KEY_GEN, { 16, 16, CKF_GENERATE }, PR_TRUE },
    { CKM_IDEA_ECB, { 16, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_IDEA_CBC, { 16, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_IDEA_MAC, { 16, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_IDEA_MAC_GENERAL, { 16, 16, CKF_SN_VR }, PR_TRUE },
    { CKM_IDEA_CBC_PAD, { 16, 16, CKF_EN_DE_WR_UN }, PR_TRUE },
#endif
    /* --------------------- Secret Key Operations ------------------------ */
    { CKM_GENERIC_SECRET_KEY_GEN, { 1, 32, CKF_GENERATE }, PR_TRUE },
    { CKM_CONCATENATE_BASE_AND_KEY, { 1, 32, CKF_GENERATE }, PR_FALSE },
    { CKM_CONCATENATE_BASE_AND_DATA, { 1, 32, CKF_GENERATE }, PR_FALSE },
    { CKM_CONCATENATE_DATA_AND_BASE, { 1, 32, CKF_GENERATE }, PR_FALSE },
    { CKM_XOR_BASE_AND_DATA, { 1, 32, CKF_GENERATE }, PR_FALSE },
    { CKM_EXTRACT_KEY_FROM_KEY, { 1, 32, CKF_DERIVE }, PR_FALSE },
    /* ---------------------- SSL Key Derivations ------------------------- */
    { CKM_SSL3_PRE_MASTER_KEY_GEN, { 48, 48, CKF_GENERATE }, PR_FALSE },
    { CKM_SSL3_MASTER_KEY_DERIVE, { 48, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_SSL3_MASTER_KEY_DERIVE_DH, { 8, 128, CKF_DERIVE }, PR_FALSE },
    { CKM_SSL3_KEY_AND_MAC_DERIVE, { 48, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_SSL3_MD5_MAC, { 0, 16, CKF_DERIVE }, PR_FALSE },
    { CKM_SSL3_SHA1_MAC, { 0, 20, CKF_DERIVE }, PR_FALSE },
    { CKM_MD5_KEY_DERIVATION, { 0, 16, CKF_DERIVE }, PR_FALSE },
    { CKM_MD2_KEY_DERIVATION, { 0, 16, CKF_DERIVE }, PR_FALSE },
    { CKM_SHA1_KEY_DERIVATION, { 0, 20, CKF_DERIVE }, PR_FALSE },
    { CKM_SHA224_KEY_DERIVATION, { 0, 28, CKF_DERIVE }, PR_FALSE },
    { CKM_SHA256_KEY_DERIVATION, { 0, 32, CKF_DERIVE }, PR_FALSE },
    { CKM_SHA384_KEY_DERIVATION, { 0, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_SHA512_KEY_DERIVATION, { 0, 64, CKF_DERIVE }, PR_FALSE },
    { CKM_TLS_MASTER_KEY_DERIVE, { 48, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_TLS12_MASTER_KEY_DERIVE, { 48, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_NSS_TLS_MASTER_KEY_DERIVE_SHA256,
      { 48, 48, CKF_DERIVE },
      PR_FALSE },
    { CKM_TLS_MASTER_KEY_DERIVE_DH, { 8, 128, CKF_DERIVE }, PR_FALSE },
    { CKM_TLS12_MASTER_KEY_DERIVE_DH, { 8, 128, CKF_DERIVE }, PR_FALSE },
    { CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256,
      { 8, 128, CKF_DERIVE },
      PR_FALSE },
    { CKM_TLS_KEY_AND_MAC_DERIVE, { 48, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_TLS12_KEY_AND_MAC_DERIVE, { 48, 48, CKF_DERIVE }, PR_FALSE },
    { CKM_NSS_TLS_KEY_AND_MAC_DERIVE_SHA256,
      { 48, 48, CKF_DERIVE },
      PR_FALSE },
    { CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE,
      { 48, 128, CKF_DERIVE },
      PR_FALSE },
    { CKM_NSS_TLS_EXTENDED_MASTER_KEY_DERIVE_DH,
      { 48, 128, CKF_DERIVE },
      PR_FALSE },
    /* ---------------------- PBE Key Derivations  ------------------------ */
    { CKM_PBE_MD2_DES_CBC, { 8, 8, CKF_DERIVE }, PR_TRUE },
    { CKM_PBE_MD5_DES_CBC, { 8, 8, CKF_DERIVE }, PR_TRUE },
    /* ------------------ NETSCAPE PBE Key Derivations  ------------------- */
    { CKM_NETSCAPE_PBE_SHA1_DES_CBC, { 8, 8, CKF_GENERATE }, PR_TRUE },
    { CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC, { 24, 24, CKF_GENERATE }, PR_TRUE },
    { CKM_PBE_SHA1_DES3_EDE_CBC, { 24, 24, CKF_GENERATE }, PR_TRUE },
    { CKM_PBE_SHA1_DES2_EDE_CBC, { 24, 24, CKF_GENERATE }, PR_TRUE },
    { CKM_PBE_SHA1_RC2_40_CBC, { 40, 40, CKF_GENERATE }, PR_TRUE },
    { CKM_PBE_SHA1_RC2_128_CBC, { 128, 128, CKF_GENERATE }, PR_TRUE },
    { CKM_PBE_SHA1_RC4_40, { 40, 40, CKF_GENERATE }, PR_TRUE },
    { CKM_PBE_SHA1_RC4_128, { 128, 128, CKF_GENERATE }, PR_TRUE },
    { CKM_PBA_SHA1_WITH_SHA1_HMAC, { 20, 20, CKF_GENERATE }, PR_TRUE },
    { CKM_PKCS5_PBKD2, { 1, 256, CKF_GENERATE }, PR_TRUE },
    { CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN, { 20, 20, CKF_GENERATE }, PR_TRUE },
    { CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN, { 16, 16, CKF_GENERATE }, PR_TRUE },
    { CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN, { 16, 16, CKF_GENERATE }, PR_TRUE },
    /* ------------------ AES Key Wrap (also encrypt)  ------------------- */
    { CKM_NETSCAPE_AES_KEY_WRAP, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    { CKM_NETSCAPE_AES_KEY_WRAP_PAD, { 16, 32, CKF_EN_DE_WR_UN }, PR_TRUE },
    /* --------------------------- J-PAKE -------------------------------- */
    { CKM_NSS_JPAKE_ROUND1_SHA1, { 0, 0, CKF_GENERATE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND1_SHA256, { 0, 0, CKF_GENERATE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND1_SHA384, { 0, 0, CKF_GENERATE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND1_SHA512, { 0, 0, CKF_GENERATE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND2_SHA1, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND2_SHA256, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND2_SHA384, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_ROUND2_SHA512, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_FINAL_SHA1, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_FINAL_SHA256, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_FINAL_SHA384, { 0, 0, CKF_DERIVE }, PR_TRUE },
    { CKM_NSS_JPAKE_FINAL_SHA512, { 0, 0, CKF_DERIVE }, PR_TRUE },
    /* -------------------- Constant Time TLS MACs ----------------------- */
    { CKM_NSS_HMAC_CONSTANT_TIME, { 0, 0, CKF_DIGEST }, PR_TRUE },
    { CKM_NSS_SSL3_MAC_CONSTANT_TIME, { 0, 0, CKF_DIGEST }, PR_TRUE }
};
static const CK_ULONG mechanismCount = sizeof(mechanisms) / sizeof(mechanisms[0]);

/* sigh global so fipstokn can read it */
PRBool nsc_init = PR_FALSE;

#if defined(CHECK_FORK_PTHREAD) || defined(CHECK_FORK_MIXED)

#include <pthread.h>

static void
ForkedChild(void)
{
    if (nsc_init || nsf_init) {
        forked = PR_TRUE;
    }
}

#endif

static char *
sftk_setStringName(const char *inString, char *buffer, int buffer_length, PRBool nullTerminate)
{
    int full_length, string_length;

    full_length = nullTerminate ? buffer_length - 1 : buffer_length;
    string_length = PORT_Strlen(inString);
    /*
     *  shorten the string, respecting utf8 encoding
     *  to do so, we work backward from the end
     *  bytes looking from the end are either:
     *    - ascii [0x00,0x7f]
     *    - the [2-n]th byte of a multibyte sequence
     *        [0x3F,0xBF], i.e, most significant 2 bits are '10'
     *    - the first byte of a multibyte sequence [0xC0,0xFD],
     *        i.e, most significant 2 bits are '11'
     *
     *    When the string is too long, we lop off any trailing '10' bytes,
     *  if any. When these are all eliminated we lop off
     *  one additional byte. Thus if we lopped any '10'
     *  we'll be lopping a '11' byte (the first byte of the multibyte sequence),
     *  otherwise we're lopping off an ascii character.
     *
     *    To test for '10' bytes, we first AND it with
     *  11000000 (0xc0) so that we get 10000000 (0x80) if and only if
     *  the byte starts with 10. We test for equality.
     */
    while (string_length > full_length) {
        /* need to shorten */
        while (string_length > 0 &&
               ((inString[string_length - 1] & (char)0xc0) == (char)0x80)) {
            /* lop off '10' byte */
            string_length--;
        }
        /*
         * test string_length in case bad data is received
         * and string consisted of all '10' bytes,
         * avoiding any infinite loop
         */
        if (string_length) {
            /* remove either '11' byte or an asci byte */
            string_length--;
        }
    }
    PORT_Memset(buffer, ' ', full_length);
    if (nullTerminate) {
        buffer[full_length] = 0;
    }
    PORT_Memcpy(buffer, inString, string_length);
    return buffer;
}
/*
 * Configuration utils
 */
static CK_RV
sftk_configure(const char *man, const char *libdes)
{

    /* make sure the internationalization was done correctly... */
    if (man) {
        manufacturerID = sftk_setStringName(man, manufacturerID_space,
                                            sizeof(manufacturerID_space), PR_TRUE);
    }
    if (libdes) {
        libraryDescription = sftk_setStringName(libdes,
                                                libraryDescription_space, sizeof(libraryDescription_space),
                                                PR_TRUE);
    }

    return CKR_OK;
}

/*
 * ******************** Password Utilities *******************************
 */

/*
 * see if the key DB password is enabled
 */
static PRBool
sftk_hasNullPassword(SFTKSlot *slot, SFTKDBHandle *keydb)
{
    PRBool pwenabled;

    pwenabled = PR_FALSE;
    if (sftkdb_HasPasswordSet(keydb) == SECSuccess) {
        PRBool tokenRemoved = PR_FALSE;
        SECStatus rv = sftkdb_CheckPassword(keydb, "", &tokenRemoved);
        if (tokenRemoved) {
            sftk_CloseAllSessions(slot, PR_FALSE);
        }
        return (rv == SECSuccess);
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
sftk_defaultAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                      const void *value, unsigned int len)
{
    if (!sftk_hasAttribute(object, type)) {
        return sftk_AddAttributeType(object, type, value, len);
    }
    return CKR_OK;
}

/*
 * check the consistancy and initialize a Data Object
 */
static CK_RV
sftk_handleDataObject(SFTKSession *session, SFTKObject *object)
{
    CK_RV crv;

    /* first reject private and token data objects */
    if (sftk_isTrue(object, CKA_PRIVATE) || sftk_isTrue(object, CKA_TOKEN)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* now just verify the required date fields */
    crv = sftk_defaultAttribute(object, CKA_APPLICATION, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_VALUE, NULL, 0);
    if (crv != CKR_OK)
        return crv;

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Certificate Object
 */
static CK_RV
sftk_handleCertObject(SFTKSession *session, SFTKObject *object)
{
    CK_CERTIFICATE_TYPE type;
    SFTKAttribute *attribute;
    CK_RV crv;

    /* certificates must have a type */
    if (!sftk_hasAttribute(object, CKA_CERTIFICATE_TYPE)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* we can't store any certs private */
    if (sftk_isTrue(object, CKA_PRIVATE)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* We only support X.509 Certs for now */
    attribute = sftk_FindAttribute(object, CKA_CERTIFICATE_TYPE);
    if (attribute == NULL)
        return CKR_TEMPLATE_INCOMPLETE;
    type = *(CK_CERTIFICATE_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);

    if (type != CKC_X_509) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* X.509 Certificate */

    /* make sure we have a cert */
    if (!sftk_hasAttribute(object, CKA_VALUE)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* in PKCS #11, Subject is a required field */
    if (!sftk_hasAttribute(object, CKA_SUBJECT)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* in PKCS #11, Issuer is a required field */
    if (!sftk_hasAttribute(object, CKA_ISSUER)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* in PKCS #11, Serial is a required field */
    if (!sftk_hasAttribute(object, CKA_SERIAL_NUMBER)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* add it to the object */
    object->objectInfo = NULL;
    object->infoFree = (SFTKFree)NULL;

    /* now just verify the required date fields */
    crv = sftk_defaultAttribute(object, CKA_ID, NULL, 0);
    if (crv != CKR_OK) {
        return crv;
    }

    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *certHandle = sftk_getCertDB(slot);

        if (certHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(certHandle, object, &object->handle);
        sftk_freeDB(certHandle);
        return crv;
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Trust Object
 */
static CK_RV
sftk_handleTrustObject(SFTKSession *session, SFTKObject *object)
{
    /* we can't store any certs private */
    if (sftk_isTrue(object, CKA_PRIVATE)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* certificates must have a type */
    if (!sftk_hasAttribute(object, CKA_ISSUER)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    if (!sftk_hasAttribute(object, CKA_SERIAL_NUMBER)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    if (!sftk_hasAttribute(object, CKA_CERT_SHA1_HASH)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    if (!sftk_hasAttribute(object, CKA_CERT_MD5_HASH)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *certHandle = sftk_getCertDB(slot);
        CK_RV crv;

        if (certHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(certHandle, object, &object->handle);
        sftk_freeDB(certHandle);
        return crv;
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Trust Object
 */
static CK_RV
sftk_handleSMimeObject(SFTKSession *session, SFTKObject *object)
{

    /* we can't store any certs private */
    if (sftk_isTrue(object, CKA_PRIVATE)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* certificates must have a type */
    if (!sftk_hasAttribute(object, CKA_SUBJECT)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    if (!sftk_hasAttribute(object, CKA_NETSCAPE_EMAIL)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *certHandle;
        CK_RV crv;

        PORT_Assert(slot);
        if (slot == NULL) {
            return CKR_SESSION_HANDLE_INVALID;
        }

        certHandle = sftk_getCertDB(slot);
        if (certHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(certHandle, object, &object->handle);
        sftk_freeDB(certHandle);
        return crv;
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Trust Object
 */
static CK_RV
sftk_handleCrlObject(SFTKSession *session, SFTKObject *object)
{

    /* we can't store any certs private */
    if (sftk_isTrue(object, CKA_PRIVATE)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* certificates must have a type */
    if (!sftk_hasAttribute(object, CKA_SUBJECT)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    if (!sftk_hasAttribute(object, CKA_VALUE)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *certHandle = sftk_getCertDB(slot);
        CK_RV crv;

        if (certHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(certHandle, object, &object->handle);
        sftk_freeDB(certHandle);
        return crv;
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Public Key Object
 */
static CK_RV
sftk_handlePublicKeyObject(SFTKSession *session, SFTKObject *object,
                           CK_KEY_TYPE key_type)
{
    CK_BBOOL encrypt = CK_TRUE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL wrap = CK_TRUE;
    CK_BBOOL derive = CK_FALSE;
    CK_BBOOL verify = CK_TRUE;
    CK_RV crv;

    switch (key_type) {
        case CKK_RSA:
            crv = sftk_ConstrainAttribute(object, CKA_MODULUS,
                                          RSA_MIN_MODULUS_BITS, 0, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            crv = sftk_ConstrainAttribute(object, CKA_PUBLIC_EXPONENT, 2, 0, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            break;
        case CKK_DSA:
            crv = sftk_ConstrainAttribute(object, CKA_SUBPRIME,
                                          DSA_MIN_Q_BITS, DSA_MAX_Q_BITS, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            crv = sftk_ConstrainAttribute(object, CKA_PRIME,
                                          DSA_MIN_P_BITS, DSA_MAX_P_BITS, 64);
            if (crv != CKR_OK) {
                return crv;
            }
            crv = sftk_ConstrainAttribute(object, CKA_BASE, 2, DSA_MAX_P_BITS, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            crv = sftk_ConstrainAttribute(object, CKA_VALUE, 2, DSA_MAX_P_BITS, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            encrypt = CK_FALSE;
            recover = CK_FALSE;
            wrap = CK_FALSE;
            break;
        case CKK_DH:
            crv = sftk_ConstrainAttribute(object, CKA_PRIME,
                                          DH_MIN_P_BITS, DH_MAX_P_BITS, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            crv = sftk_ConstrainAttribute(object, CKA_BASE, 2, DH_MAX_P_BITS, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            crv = sftk_ConstrainAttribute(object, CKA_VALUE, 2, DH_MAX_P_BITS, 0);
            if (crv != CKR_OK) {
                return crv;
            }
            verify = CK_FALSE;
            derive = CK_TRUE;
            encrypt = CK_FALSE;
            recover = CK_FALSE;
            wrap = CK_FALSE;
            break;
#ifndef NSS_DISABLE_ECC
        case CKK_EC:
            if (!sftk_hasAttribute(object, CKA_EC_PARAMS)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            if (!sftk_hasAttribute(object, CKA_EC_POINT)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            derive = CK_TRUE; /* for ECDH */
            verify = CK_TRUE; /* for ECDSA */
            encrypt = CK_FALSE;
            recover = CK_FALSE;
            wrap = CK_FALSE;
            break;
#endif /* NSS_DISABLE_ECC */
        default:
            return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* make sure the required fields exist */
    crv = sftk_defaultAttribute(object, CKA_SUBJECT, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_ENCRYPT, &encrypt, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_VERIFY, &verify, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_VERIFY_RECOVER,
                                &recover, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_WRAP, &wrap, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_DERIVE, &derive, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

    object->objectInfo = sftk_GetPubKey(object, key_type, &crv);
    if (object->objectInfo == NULL) {
        return crv;
    }
    object->infoFree = (SFTKFree)nsslowkey_DestroyPublicKey;

    /* Check that an imported EC key is valid */
    if (key_type == CKK_EC) {
        NSSLOWKEYPublicKey *pubKey = (NSSLOWKEYPublicKey *)object->objectInfo;
        SECStatus rv = EC_ValidatePublicKey(&pubKey->u.ec.ecParams,
                                            &pubKey->u.ec.publicValue);

        if (rv != SECSuccess) {
            return CKR_TEMPLATE_INCONSISTENT;
        }
    }

    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *certHandle = sftk_getCertDB(slot);

        if (certHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(certHandle, object, &object->handle);
        sftk_freeDB(certHandle);
        return crv;
    }

    return CKR_OK;
}

static NSSLOWKEYPrivateKey *
sftk_mkPrivKey(SFTKObject *object, CK_KEY_TYPE key, CK_RV *rvp);

static SECStatus
sftk_verifyRSAPrivateKey(SFTKObject *object, PRBool fillIfNeeded);

/*
 * check the consistancy and initialize a Private Key Object
 */
static CK_RV
sftk_handlePrivateKeyObject(SFTKSession *session, SFTKObject *object, CK_KEY_TYPE key_type)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL encrypt = CK_TRUE;
    CK_BBOOL sign = CK_FALSE;
    CK_BBOOL recover = CK_TRUE;
    CK_BBOOL wrap = CK_TRUE;
    CK_BBOOL derive = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    PRBool createObjectInfo = PR_TRUE;
    PRBool fillPrivateKey = PR_FALSE;
    int missing_rsa_mod_component = 0;
    int missing_rsa_exp_component = 0;
    int missing_rsa_crt_component = 0;

    SECItem mod;
    CK_RV crv;
    SECStatus rv;

    switch (key_type) {
        case CKK_RSA:
            if (!sftk_hasAttribute(object, CKA_MODULUS)) {
                missing_rsa_mod_component++;
            }
            if (!sftk_hasAttribute(object, CKA_PUBLIC_EXPONENT)) {
                missing_rsa_exp_component++;
            }
            if (!sftk_hasAttribute(object, CKA_PRIVATE_EXPONENT)) {
                missing_rsa_exp_component++;
            }
            if (!sftk_hasAttribute(object, CKA_PRIME_1)) {
                missing_rsa_mod_component++;
            }
            if (!sftk_hasAttribute(object, CKA_PRIME_2)) {
                missing_rsa_mod_component++;
            }
            if (!sftk_hasAttribute(object, CKA_EXPONENT_1)) {
                missing_rsa_crt_component++;
            }
            if (!sftk_hasAttribute(object, CKA_EXPONENT_2)) {
                missing_rsa_crt_component++;
            }
            if (!sftk_hasAttribute(object, CKA_COEFFICIENT)) {
                missing_rsa_crt_component++;
            }
            if (missing_rsa_mod_component || missing_rsa_exp_component ||
                missing_rsa_crt_component) {
                /* we are missing a component, see if we have enough to rebuild
                 * the rest */
                int have_exp = 2 - missing_rsa_exp_component;
                int have_component = 5 -
                                     (missing_rsa_exp_component + missing_rsa_mod_component);

                if ((have_exp == 0) || (have_component < 3)) {
                    /* nope, not enough to reconstruct the private key */
                    return CKR_TEMPLATE_INCOMPLETE;
                }
                fillPrivateKey = PR_TRUE;
            }
            /*verify the parameters for consistency*/
            rv = sftk_verifyRSAPrivateKey(object, fillPrivateKey);
            if (rv != SECSuccess) {
                return CKR_TEMPLATE_INCOMPLETE;
            }

            /* make sure Netscape DB attribute is set correctly */
            crv = sftk_Attribute2SSecItem(NULL, &mod, object, CKA_MODULUS);
            if (crv != CKR_OK)
                return crv;
            crv = sftk_forceAttribute(object, CKA_NETSCAPE_DB,
                                      sftk_item_expand(&mod));
            if (mod.data)
                PORT_Free(mod.data);
            if (crv != CKR_OK)
                return crv;

            sign = CK_TRUE;
            derive = CK_FALSE;
            break;
        case CKK_DSA:
            if (!sftk_hasAttribute(object, CKA_SUBPRIME)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            sign = CK_TRUE;
            derive = CK_FALSE;
        /* fall through */
        case CKK_DH:
            if (!sftk_hasAttribute(object, CKA_PRIME)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            if (!sftk_hasAttribute(object, CKA_BASE)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            if (!sftk_hasAttribute(object, CKA_VALUE)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            encrypt = CK_FALSE;
            recover = CK_FALSE;
            wrap = CK_FALSE;
            break;
#ifndef NSS_DISABLE_ECC
        case CKK_EC:
            if (!sftk_hasAttribute(object, CKA_EC_PARAMS)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            if (!sftk_hasAttribute(object, CKA_VALUE)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
            encrypt = CK_FALSE;
            sign = CK_TRUE;
            recover = CK_FALSE;
            wrap = CK_FALSE;
            break;
#endif /* NSS_DISABLE_ECC */
        case CKK_NSS_JPAKE_ROUND1:
            if (!sftk_hasAttribute(object, CKA_PRIME) ||
                !sftk_hasAttribute(object, CKA_SUBPRIME) ||
                !sftk_hasAttribute(object, CKA_BASE)) {
                return CKR_TEMPLATE_INCOMPLETE;
            }
        /* fall through */
        case CKK_NSS_JPAKE_ROUND2:
            /* CKA_NSS_JPAKE_SIGNERID and CKA_NSS_JPAKE_PEERID are checked in
               the J-PAKE code. */
            encrypt = sign = recover = wrap = CK_FALSE;
            derive = CK_TRUE;
            createObjectInfo = PR_FALSE;
            break;
        default:
            return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    crv = sftk_defaultAttribute(object, CKA_SUBJECT, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_SENSITIVE, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_EXTRACTABLE, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_DECRYPT, &encrypt, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_SIGN, &sign, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_SIGN_RECOVER, &recover,
                                sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_UNWRAP, &wrap, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_DERIVE, &derive, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    /* the next two bits get modified only in the key gen and token cases */
    crv = sftk_forceAttribute(object, CKA_ALWAYS_SENSITIVE,
                              &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_forceAttribute(object, CKA_NEVER_EXTRACTABLE,
                              &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

    /* should we check the non-token RSA private keys? */

    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *keyHandle = sftk_getKeyDB(slot);

        if (keyHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(keyHandle, object, &object->handle);
        sftk_freeDB(keyHandle);
        return crv;
    } else if (createObjectInfo) {
        object->objectInfo = sftk_mkPrivKey(object, key_type, &crv);
        if (object->objectInfo == NULL)
            return crv;
        object->infoFree = (SFTKFree)nsslowkey_DestroyPrivateKey;
    }
    return CKR_OK;
}

/* forward declare the DES formating function for handleSecretKey */
void sftk_FormatDESKey(unsigned char *key, int length);

/* Validate secret key data, and set defaults */
static CK_RV
validateSecretKey(SFTKSession *session, SFTKObject *object,
                  CK_KEY_TYPE key_type, PRBool isFIPS)
{
    CK_RV crv;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    SFTKAttribute *attribute = NULL;
    unsigned long requiredLen;

    crv = sftk_defaultAttribute(object, CKA_SENSITIVE,
                                isFIPS ? &cktrue : &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_EXTRACTABLE,
                                &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_ENCRYPT, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_DECRYPT, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_SIGN, &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_VERIFY, &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_WRAP, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_UNWRAP, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

    if (!sftk_hasAttribute(object, CKA_VALUE)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    /* the next two bits get modified only in the key gen and token cases */
    crv = sftk_forceAttribute(object, CKA_ALWAYS_SENSITIVE,
                              &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_forceAttribute(object, CKA_NEVER_EXTRACTABLE,
                              &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

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
            attribute = sftk_FindAttribute(object, CKA_VALUE);
            /* shouldn't happen */
            if (attribute == NULL)
                return CKR_TEMPLATE_INCOMPLETE;
            crv = sftk_forceAttribute(object, CKA_VALUE_LEN,
                                      &attribute->attrib.ulValueLen, sizeof(CK_ULONG));
            sftk_FreeAttribute(attribute);
            break;
        /* force the value to have the correct parity */
        case CKK_DES:
        case CKK_DES2:
        case CKK_DES3:
        case CKK_CDMF:
            attribute = sftk_FindAttribute(object, CKA_VALUE);
            /* shouldn't happen */
            if (attribute == NULL)
                return CKR_TEMPLATE_INCOMPLETE;
            requiredLen = sftk_MapKeySize(key_type);
            if (attribute->attrib.ulValueLen != requiredLen) {
                sftk_FreeAttribute(attribute);
                return CKR_KEY_SIZE_RANGE;
            }
            sftk_FormatDESKey((unsigned char *)attribute->attrib.pValue,
                              attribute->attrib.ulValueLen);
            sftk_FreeAttribute(attribute);
            break;
        case CKK_AES:
            attribute = sftk_FindAttribute(object, CKA_VALUE);
            /* shouldn't happen */
            if (attribute == NULL)
                return CKR_TEMPLATE_INCOMPLETE;
            if (attribute->attrib.ulValueLen != 16 &&
                attribute->attrib.ulValueLen != 24 &&
                attribute->attrib.ulValueLen != 32) {
                sftk_FreeAttribute(attribute);
                return CKR_KEY_SIZE_RANGE;
            }
            crv = sftk_forceAttribute(object, CKA_VALUE_LEN,
                                      &attribute->attrib.ulValueLen, sizeof(CK_ULONG));
            sftk_FreeAttribute(attribute);
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
sftk_handleSecretKeyObject(SFTKSession *session, SFTKObject *object,
                           CK_KEY_TYPE key_type, PRBool isFIPS)
{
    CK_RV crv;

    /* First validate and set defaults */
    crv = validateSecretKey(session, object, key_type, isFIPS);
    if (crv != CKR_OK)
        goto loser;

    /* If the object is a TOKEN object, store in the database */
    if (sftk_isTrue(object, CKA_TOKEN)) {
        SFTKSlot *slot = session->slot;
        SFTKDBHandle *keyHandle = sftk_getKeyDB(slot);
        CK_RV crv;

        if (keyHandle == NULL) {
            return CKR_TOKEN_WRITE_PROTECTED;
        }

        crv = sftkdb_write(keyHandle, object, &object->handle);
        sftk_freeDB(keyHandle);
        return crv;
    }

loser:

    return crv;
}

/*
 * check the consistancy and initialize a Key Object
 */
static CK_RV
sftk_handleKeyObject(SFTKSession *session, SFTKObject *object)
{
    SFTKAttribute *attribute;
    CK_KEY_TYPE key_type;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_RV crv;

    /* verify the required fields */
    if (!sftk_hasAttribute(object, CKA_KEY_TYPE)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* now verify the common fields */
    crv = sftk_defaultAttribute(object, CKA_ID, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_START_DATE, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_END_DATE, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    /* CKA_DERIVE is common to all keys, but it's default value is
     * key dependent */
    crv = sftk_defaultAttribute(object, CKA_LOCAL, &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

    /* get the key type */
    attribute = sftk_FindAttribute(object, CKA_KEY_TYPE);
    if (!attribute) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);

    switch (object->objclass) {
        case CKO_PUBLIC_KEY:
            return sftk_handlePublicKeyObject(session, object, key_type);
        case CKO_PRIVATE_KEY:
            return sftk_handlePrivateKeyObject(session, object, key_type);
        case CKO_SECRET_KEY:
            /* make sure the required fields exist */
            return sftk_handleSecretKeyObject(session, object, key_type,
                                              (PRBool)(session->slot->slotID == FIPS_SLOT_ID));
        default:
            break;
    }
    return CKR_ATTRIBUTE_VALUE_INVALID;
}

/*
 * check the consistancy and Verify a DSA Parameter Object
 */
static CK_RV
sftk_handleDSAParameterObject(SFTKSession *session, SFTKObject *object)
{
    SFTKAttribute *primeAttr = NULL;
    SFTKAttribute *subPrimeAttr = NULL;
    SFTKAttribute *baseAttr = NULL;
    SFTKAttribute *seedAttr = NULL;
    SFTKAttribute *hAttr = NULL;
    SFTKAttribute *attribute;
    CK_RV crv = CKR_TEMPLATE_INCOMPLETE;
    PQGParams params;
    PQGVerify vfy, *verify = NULL;
    SECStatus result, rv;
    /* This bool keeps track of whether or not we need verify parameters.
     * If a P, Q and G or supplied, we dont' need verify parameters, as we
     * have PQ and G.
     *   - If G is not supplied, the presumption is that we want to
     * verify P and Q only.
     *   - If counter is supplied, it is presumed we want to verify PQ because
     * the counter is only used in verification.
     *   - If H is supplied, is is presumed we want to verify G because H is
     * only used to verify G.
     *   - Any verification step must have the SEED (counter or H could be
     * missing depending on exactly what we want to verify). If SEED is supplied,
     * the code just goes ahead and runs verify (other errors are parameter
     * errors are detected by the PQG_VerifyParams function). If SEED is not
     * supplied, but we determined that we are trying to verify (because needVfy
     * is set, go ahead and return CKR_TEMPLATE_INCOMPLETE.
     */
    PRBool needVfy = PR_FALSE;

    primeAttr = sftk_FindAttribute(object, CKA_PRIME);
    if (primeAttr == NULL)
        goto loser;
    params.prime.data = primeAttr->attrib.pValue;
    params.prime.len = primeAttr->attrib.ulValueLen;

    subPrimeAttr = sftk_FindAttribute(object, CKA_SUBPRIME);
    if (subPrimeAttr == NULL)
        goto loser;
    params.subPrime.data = subPrimeAttr->attrib.pValue;
    params.subPrime.len = subPrimeAttr->attrib.ulValueLen;

    baseAttr = sftk_FindAttribute(object, CKA_BASE);
    if (baseAttr != NULL) {
        params.base.data = baseAttr->attrib.pValue;
        params.base.len = baseAttr->attrib.ulValueLen;
    } else {
        params.base.data = NULL;
        params.base.len = 0;
        needVfy = PR_TRUE; /* presumably only including PQ so we can verify
                            * them. */
    }

    attribute = sftk_FindAttribute(object, CKA_NETSCAPE_PQG_COUNTER);
    if (attribute != NULL) {
        vfy.counter = *(CK_ULONG *)attribute->attrib.pValue;
        sftk_FreeAttribute(attribute);
        needVfy = PR_TRUE; /* included a count so we can verify PQ */
    } else {
        vfy.counter = -1;
    }

    hAttr = sftk_FindAttribute(object, CKA_NETSCAPE_PQG_H);
    if (hAttr != NULL) {
        vfy.h.data = hAttr->attrib.pValue;
        vfy.h.len = hAttr->attrib.ulValueLen;
        needVfy = PR_TRUE; /* included H so we can verify G */
    } else {
        vfy.h.data = NULL;
        vfy.h.len = 0;
    }
    seedAttr = sftk_FindAttribute(object, CKA_NETSCAPE_PQG_SEED);
    if (seedAttr != NULL) {
        vfy.seed.data = seedAttr->attrib.pValue;
        vfy.seed.len = seedAttr->attrib.ulValueLen;

        verify = &vfy;
    } else if (needVfy) {
        goto loser; /* Verify always needs seed, if we need verify and not seed
                     * then fail */
    }

    crv = CKR_FUNCTION_FAILED;
    rv = PQG_VerifyParams(&params, verify, &result);
    if (rv == SECSuccess) {
        crv = (result == SECSuccess) ? CKR_OK : CKR_ATTRIBUTE_VALUE_INVALID;
    }

loser:
    if (hAttr)
        sftk_FreeAttribute(hAttr);
    if (seedAttr)
        sftk_FreeAttribute(seedAttr);
    if (baseAttr)
        sftk_FreeAttribute(baseAttr);
    if (subPrimeAttr)
        sftk_FreeAttribute(subPrimeAttr);
    if (primeAttr)
        sftk_FreeAttribute(primeAttr);

    return crv;
}

/*
 * check the consistancy and initialize a Key Parameter Object
 */
static CK_RV
sftk_handleKeyParameterObject(SFTKSession *session, SFTKObject *object)
{
    SFTKAttribute *attribute;
    CK_KEY_TYPE key_type;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_RV crv;

    /* verify the required fields */
    if (!sftk_hasAttribute(object, CKA_KEY_TYPE)) {
        return CKR_TEMPLATE_INCOMPLETE;
    }

    /* now verify the common fields */
    crv = sftk_defaultAttribute(object, CKA_LOCAL, &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

    /* get the key type */
    attribute = sftk_FindAttribute(object, CKA_KEY_TYPE);
    if (!attribute) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);

    switch (key_type) {
        case CKK_DSA:
            return sftk_handleDSAParameterObject(session, object);

        default:
            break;
    }
    return CKR_KEY_TYPE_INCONSISTENT;
}

/*
 * Handle Object does all the object consistancy checks, automatic attribute
 * generation, attribute defaulting, etc. If handleObject succeeds, the object
 * will be assigned an object handle, and the object installed in the session
 * or stored in the DB.
 */
CK_RV
sftk_handleObject(SFTKObject *object, SFTKSession *session)
{
    SFTKSlot *slot = session->slot;
    SFTKAttribute *attribute;
    SFTKObject *duplicateObject = NULL;
    CK_OBJECT_HANDLE handle;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_BBOOL cktrue = CK_TRUE;
    CK_RV crv;

    /* make sure all the base object types are defined. If not set the
     * defaults */
    crv = sftk_defaultAttribute(object, CKA_TOKEN, &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_PRIVATE, &ckfalse, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_LABEL, NULL, 0);
    if (crv != CKR_OK)
        return crv;
    crv = sftk_defaultAttribute(object, CKA_MODIFIABLE, &cktrue, sizeof(CK_BBOOL));
    if (crv != CKR_OK)
        return crv;

    /* don't create a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
        (sftk_isTrue(object, CKA_PRIVATE))) {
        return CKR_USER_NOT_LOGGED_IN;
    }

    if (((session->info.flags & CKF_RW_SESSION) == 0) &&
        (sftk_isTrue(object, CKA_TOKEN))) {
        return CKR_SESSION_READ_ONLY;
    }

    /* Assign a unique SESSION object handle to every new object,
     * whether it is a session object or a token object.
     * At this point, all new objects are structured as session objects.
     * Objects with the CKA_TOKEN attribute true will be turned into
     * token objects and will have a token object handle assigned to
     * them by a call to sftk_mkHandle in the handler for each object
     * class, invoked below.
     *
     * It may be helpful to note/remember that
     * sftk_narrowToXxxObject uses sftk_isToken,
     * sftk_isToken examines the sign bit of the object's handle, but
     * sftk_isTrue(...,CKA_TOKEN) examines the CKA_TOKEN attribute.
     */
    do {
        PRUint32 wrappedAround;

        duplicateObject = NULL;
        PZ_Lock(slot->objectLock);
        wrappedAround = slot->sessionObjectHandleCount & SFTK_TOKEN_MASK;
        handle = slot->sessionObjectHandleCount & ~SFTK_TOKEN_MASK;
        if (!handle) /* don't allow zero handle */
            handle = minSessionObjectHandle;
        slot->sessionObjectHandleCount = (handle + 1U) | wrappedAround;
        /* Is there already a session object with this handle? */
        if (wrappedAround) {
            sftkqueue_find(duplicateObject, handle, slot->sessObjHashTable,
                           slot->sessObjHashSize);
        }
        PZ_Unlock(slot->objectLock);
    } while (duplicateObject != NULL);
    object->handle = handle;

    /* get the object class */
    attribute = sftk_FindAttribute(object, CKA_CLASS);
    if (attribute == NULL) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    object->objclass = *(CK_OBJECT_CLASS *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);

    /* Now handle the specific object class.
     * At this point, all objects are session objects, and the session
     * number must be passed to the object class handlers.
     */
    switch (object->objclass) {
        case CKO_DATA:
            crv = sftk_handleDataObject(session, object);
            break;
        case CKO_CERTIFICATE:
            crv = sftk_handleCertObject(session, object);
            break;
        case CKO_NETSCAPE_TRUST:
            crv = sftk_handleTrustObject(session, object);
            break;
        case CKO_NETSCAPE_CRL:
            crv = sftk_handleCrlObject(session, object);
            break;
        case CKO_NETSCAPE_SMIME:
            crv = sftk_handleSMimeObject(session, object);
            break;
        case CKO_PRIVATE_KEY:
        case CKO_PUBLIC_KEY:
        case CKO_SECRET_KEY:
            crv = sftk_handleKeyObject(session, object);
            break;
        case CKO_KG_PARAMETERS:
            crv = sftk_handleKeyParameterObject(session, object);
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

    /* Now link the object into the slot and session structures.
     * If the object has a true CKA_TOKEN attribute, the above object
     * class handlers will have set the sign bit in the object handle,
     * causing the following test to be true.
     */
    if (sftk_isToken(object->handle)) {
        sftk_convertSessionToToken(object);
    } else {
        object->slot = slot;
        sftk_AddObject(session, object);
    }

    return CKR_OK;
}

/*
 * ******************** Public Key Utilities ***************************
 */
/* Generate a low public key structure from an object */
NSSLOWKEYPublicKey *
sftk_GetPubKey(SFTKObject *object, CK_KEY_TYPE key_type,
               CK_RV *crvp)
{
    NSSLOWKEYPublicKey *pubKey;
    PLArenaPool *arena;
    CK_RV crv;

    if (object->objclass != CKO_PUBLIC_KEY) {
        *crvp = CKR_KEY_TYPE_INCONSISTENT;
        return NULL;
    }

    if (sftk_isToken(object->handle)) {
        /* ferret out the token object handle */
    }

    /* If we already have a key, use it */
    if (object->objectInfo) {
        *crvp = CKR_OK;
        return (NSSLOWKEYPublicKey *)object->objectInfo;
    }

    /* allocate the structure */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        *crvp = CKR_HOST_MEMORY;
        return NULL;
    }

    pubKey = (NSSLOWKEYPublicKey *)
        PORT_ArenaAlloc(arena, sizeof(NSSLOWKEYPublicKey));
    if (pubKey == NULL) {
        PORT_FreeArena(arena, PR_FALSE);
        *crvp = CKR_HOST_MEMORY;
        return NULL;
    }

    /* fill in the structure */
    pubKey->arena = arena;
    switch (key_type) {
        case CKK_RSA:
            pubKey->keyType = NSSLOWKEYRSAKey;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.rsa.modulus,
                                          object, CKA_MODULUS);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.rsa.publicExponent,
                                          object, CKA_PUBLIC_EXPONENT);
            break;
        case CKK_DSA:
            pubKey->keyType = NSSLOWKEYDSAKey;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dsa.params.prime,
                                          object, CKA_PRIME);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dsa.params.subPrime,
                                          object, CKA_SUBPRIME);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dsa.params.base,
                                          object, CKA_BASE);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dsa.publicValue,
                                          object, CKA_VALUE);
            break;
        case CKK_DH:
            pubKey->keyType = NSSLOWKEYDHKey;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dh.prime,
                                          object, CKA_PRIME);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dh.base,
                                          object, CKA_BASE);
            if (crv != CKR_OK)
                break;
            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.dh.publicValue,
                                          object, CKA_VALUE);
            break;
#ifndef NSS_DISABLE_ECC
        case CKK_EC:
            pubKey->keyType = NSSLOWKEYECKey;
            crv = sftk_Attribute2SSecItem(arena,
                                          &pubKey->u.ec.ecParams.DEREncoding,
                                          object, CKA_EC_PARAMS);
            if (crv != CKR_OK)
                break;

            /* Fill out the rest of the ecParams structure
             * based on the encoded params
             */
            if (EC_FillParams(arena, &pubKey->u.ec.ecParams.DEREncoding,
                              &pubKey->u.ec.ecParams) != SECSuccess) {
                crv = CKR_DOMAIN_PARAMS_INVALID;
                break;
            }

            crv = sftk_Attribute2SSecItem(arena, &pubKey->u.ec.publicValue,
                                          object, CKA_EC_POINT);
            if (crv == CKR_OK) {
                unsigned int keyLen = pubKey->u.ec.ecParams.pointSize;

                /* special note: We can't just use the first byte to distinguish
                 * between EC_POINT_FORM_UNCOMPRESSED and SEC_ASN1_OCTET_STRING.
                 * Both are 0x04. */

                /* Handle the non-DER encoded case.
                 * Some curves are always pressumed to be non-DER.
                 */
                if (pubKey->u.ec.publicValue.len == keyLen &&
                    (pubKey->u.ec.ecParams.fieldID.type == ec_field_plain ||
                     pubKey->u.ec.publicValue.data[0] == EC_POINT_FORM_UNCOMPRESSED)) {
                    break; /* key was not DER encoded, no need to unwrap */
                }

                PORT_Assert(pubKey->u.ec.ecParams.name != ECCurve25519);

                /* handle the encoded case */
                if ((pubKey->u.ec.publicValue.data[0] == SEC_ASN1_OCTET_STRING) &&
                    pubKey->u.ec.publicValue.len > keyLen) {
                    SECItem publicValue;
                    SECStatus rv;

                    rv = SEC_QuickDERDecodeItem(arena, &publicValue,
                                                SEC_ASN1_GET(SEC_OctetStringTemplate),
                                                &pubKey->u.ec.publicValue);
                    /* nope, didn't decode correctly */
                    if ((rv != SECSuccess) || (publicValue.data[0] != EC_POINT_FORM_UNCOMPRESSED) || (publicValue.len != keyLen)) {
                        crv = CKR_ATTRIBUTE_VALUE_INVALID;
                        break;
                    }
                    /* replace our previous with the decoded key */
                    pubKey->u.ec.publicValue = publicValue;
                    break;
                }
                crv = CKR_ATTRIBUTE_VALUE_INVALID;
            }
            break;
#endif /* NSS_DISABLE_ECC */
        default:
            crv = CKR_KEY_TYPE_INCONSISTENT;
            break;
    }
    *crvp = crv;
    if (crv != CKR_OK) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    object->objectInfo = pubKey;
    object->infoFree = (SFTKFree)nsslowkey_DestroyPublicKey;
    return pubKey;
}

/* make a private key from a verified object */
static NSSLOWKEYPrivateKey *
sftk_mkPrivKey(SFTKObject *object, CK_KEY_TYPE key_type, CK_RV *crvp)
{
    NSSLOWKEYPrivateKey *privKey;
    SFTKItemTemplate itemTemplate[SFTK_MAX_ITEM_TEMPLATE];
    int itemTemplateCount = 0;
    PLArenaPool *arena;
    CK_RV crv = CKR_OK;
    SECStatus rv;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        *crvp = CKR_HOST_MEMORY;
        return NULL;
    }

    privKey = (NSSLOWKEYPrivateKey *)
        PORT_ArenaZAlloc(arena, sizeof(NSSLOWKEYPrivateKey));
    if (privKey == NULL) {
        PORT_FreeArena(arena, PR_FALSE);
        *crvp = CKR_HOST_MEMORY;
        return NULL;
    }

    /* in future this would be a switch on key_type */
    privKey->arena = arena;
    switch (key_type) {
        case CKK_RSA:
            privKey->keyType = NSSLOWKEYRSAKey;

            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.modulus, CKA_MODULUS);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.publicExponent, CKA_PUBLIC_EXPONENT);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.privateExponent, CKA_PRIVATE_EXPONENT);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.prime1, CKA_PRIME_1);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.prime2, CKA_PRIME_2);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.exponent1, CKA_EXPONENT_1);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.exponent2, CKA_EXPONENT_2);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.rsa.coefficient, CKA_COEFFICIENT);
            itemTemplateCount++;
            rv = DER_SetUInteger(privKey->arena, &privKey->u.rsa.version,
                                 NSSLOWKEY_PRIVATE_KEY_INFO_VERSION);
            if (rv != SECSuccess)
                crv = CKR_HOST_MEMORY;
            break;

        case CKK_DSA:
            privKey->keyType = NSSLOWKEYDSAKey;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dsa.params.prime, CKA_PRIME);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dsa.params.subPrime, CKA_SUBPRIME);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dsa.params.base, CKA_BASE);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dsa.privateValue, CKA_VALUE);
            itemTemplateCount++;
            /* privKey was zero'd so public value is already set to NULL, 0
             * if we don't set it explicitly */
            break;

        case CKK_DH:
            privKey->keyType = NSSLOWKEYDHKey;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dh.prime, CKA_PRIME);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dh.base, CKA_BASE);
            itemTemplateCount++;
            SFTK_SET_ITEM_TEMPLATE(itemTemplate, itemTemplateCount,
                                   &privKey->u.dh.privateValue, CKA_VALUE);
            itemTemplateCount++;
            /* privKey was zero'd so public value is already set to NULL, 0
             * if we don't set it explicitly */
            break;

#ifndef NSS_DISABLE_ECC
        case CKK_EC:
            privKey->keyType = NSSLOWKEYECKey;
            crv = sftk_Attribute2SSecItem(arena,
                                          &privKey->u.ec.ecParams.DEREncoding,
                                          object, CKA_EC_PARAMS);
            if (crv != CKR_OK)
                break;

            /* Fill out the rest of the ecParams structure
             * based on the encoded params
             */
            if (EC_FillParams(arena, &privKey->u.ec.ecParams.DEREncoding,
                              &privKey->u.ec.ecParams) != SECSuccess) {
                crv = CKR_DOMAIN_PARAMS_INVALID;
                break;
            }
            crv = sftk_Attribute2SSecItem(arena, &privKey->u.ec.privateValue,
                                          object, CKA_VALUE);
            if (crv != CKR_OK)
                break;

            if (sftk_hasAttribute(object, CKA_NETSCAPE_DB)) {
                crv = sftk_Attribute2SSecItem(arena, &privKey->u.ec.publicValue,
                                              object, CKA_NETSCAPE_DB);
                if (crv != CKR_OK)
                    break;
                /* privKey was zero'd so public value is already set to NULL, 0
             * if we don't set it explicitly */
            }
            rv = DER_SetUInteger(privKey->arena, &privKey->u.ec.version,
                                 NSSLOWKEY_EC_PRIVATE_KEY_VERSION);
            if (rv != SECSuccess) {
                crv = CKR_HOST_MEMORY;
/* The following ifdef is needed for Linux arm distros and
 * Android as gcc 4.6 has a bug when targeting arm (but not
 * thumb). The bug has been fixed in gcc 4.7.
 * http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56561
 */
#if defined(__arm__) && !defined(__thumb__) && defined(__GNUC__)
                *crvp = CKR_HOST_MEMORY;
                break;
#endif
            }
            break;
#endif /* NSS_DISABLE_ECC */

        default:
            crv = CKR_KEY_TYPE_INCONSISTENT;
            break;
    }
    if (crv == CKR_OK && itemTemplateCount != 0) {
        PORT_Assert(itemTemplateCount > 0);
        PORT_Assert(itemTemplateCount <= SFTK_MAX_ITEM_TEMPLATE);
        crv = sftk_MultipleAttribute2SecItem(arena, object, itemTemplate,
                                             itemTemplateCount);
    }
    *crvp = crv;
    if (crv != CKR_OK) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }
    return privKey;
}

/*
 * If a partial RSA private key is present, fill in the rest if necessary,
 * and then verify the parameters are well-formed
 */
static SECStatus
sftk_verifyRSAPrivateKey(SFTKObject *object, PRBool fillIfNeeded)
{
    RSAPrivateKey tmpKey = { 0 };
    SFTKAttribute *modulus = NULL;
    SFTKAttribute *prime1 = NULL;
    SFTKAttribute *prime2 = NULL;
    SFTKAttribute *privateExponent = NULL;
    SFTKAttribute *publicExponent = NULL;
    SFTKAttribute *exponent1 = NULL;
    SFTKAttribute *exponent2 = NULL;
    SFTKAttribute *coefficient = NULL;
    SECStatus rv;
    CK_RV crv;

    /* first fill in the components that we have. Populate only uses
     * the non-crt components, so only fill those in  */
    tmpKey.arena = NULL;
    modulus = sftk_FindAttribute(object, CKA_MODULUS);
    if (modulus) {
        tmpKey.modulus.data = modulus->attrib.pValue;
        tmpKey.modulus.len = modulus->attrib.ulValueLen;
    }
    prime1 = sftk_FindAttribute(object, CKA_PRIME_1);
    if (prime1) {
        tmpKey.prime1.data = prime1->attrib.pValue;
        tmpKey.prime1.len = prime1->attrib.ulValueLen;
    }
    prime2 = sftk_FindAttribute(object, CKA_PRIME_2);
    if (prime2) {
        tmpKey.prime2.data = prime2->attrib.pValue;
        tmpKey.prime2.len = prime2->attrib.ulValueLen;
    }
    privateExponent = sftk_FindAttribute(object, CKA_PRIVATE_EXPONENT);
    if (privateExponent) {
        tmpKey.privateExponent.data = privateExponent->attrib.pValue;
        tmpKey.privateExponent.len = privateExponent->attrib.ulValueLen;
    }
    publicExponent = sftk_FindAttribute(object, CKA_PUBLIC_EXPONENT);
    if (publicExponent) {
        tmpKey.publicExponent.data = publicExponent->attrib.pValue;
        tmpKey.publicExponent.len = publicExponent->attrib.ulValueLen;
    }
    exponent1 = sftk_FindAttribute(object, CKA_EXPONENT_1);
    if (exponent1) {
        tmpKey.exponent1.data = exponent1->attrib.pValue;
        tmpKey.exponent1.len = exponent1->attrib.ulValueLen;
    }
    exponent2 = sftk_FindAttribute(object, CKA_EXPONENT_2);
    if (exponent2) {
        tmpKey.exponent2.data = exponent2->attrib.pValue;
        tmpKey.exponent2.len = exponent2->attrib.ulValueLen;
    }
    coefficient = sftk_FindAttribute(object, CKA_COEFFICIENT);
    if (coefficient) {
        tmpKey.coefficient.data = coefficient->attrib.pValue;
        tmpKey.coefficient.len = coefficient->attrib.ulValueLen;
    }

    if (fillIfNeeded) {
        /*
         * populate requires one exponent plus 2 other components to work.
         * we expected our caller to check that first. If that didn't happen,
         * populate will simply return an error here.
         */
        rv = RSA_PopulatePrivateKey(&tmpKey);
        if (rv != SECSuccess) {
            goto loser;
        }
    }
    rv = RSA_PrivateKeyCheck(&tmpKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* now that we have a fully populated key, set all our attribute values */
    rv = SECFailure;
    if (!modulus || modulus->attrib.pValue != tmpKey.modulus.data) {
        crv = sftk_forceAttribute(object, CKA_MODULUS,
                                  sftk_item_expand(&tmpKey.modulus));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!publicExponent ||
        publicExponent->attrib.pValue != tmpKey.publicExponent.data) {
        crv = sftk_forceAttribute(object, CKA_PUBLIC_EXPONENT,
                                  sftk_item_expand(&tmpKey.publicExponent));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!privateExponent ||
        privateExponent->attrib.pValue != tmpKey.privateExponent.data) {
        crv = sftk_forceAttribute(object, CKA_PRIVATE_EXPONENT,
                                  sftk_item_expand(&tmpKey.privateExponent));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!prime1 || prime1->attrib.pValue != tmpKey.prime1.data) {
        crv = sftk_forceAttribute(object, CKA_PRIME_1,
                                  sftk_item_expand(&tmpKey.prime1));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!prime2 || prime2->attrib.pValue != tmpKey.prime2.data) {
        crv = sftk_forceAttribute(object, CKA_PRIME_2,
                                  sftk_item_expand(&tmpKey.prime2));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!exponent1 || exponent1->attrib.pValue != tmpKey.exponent1.data) {
        crv = sftk_forceAttribute(object, CKA_EXPONENT_1,
                                  sftk_item_expand(&tmpKey.exponent1));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!exponent2 || exponent2->attrib.pValue != tmpKey.exponent2.data) {
        crv = sftk_forceAttribute(object, CKA_EXPONENT_2,
                                  sftk_item_expand(&tmpKey.exponent2));
        if (crv != CKR_OK)
            goto loser;
    }
    if (!coefficient || coefficient->attrib.pValue != tmpKey.coefficient.data) {
        crv = sftk_forceAttribute(object, CKA_COEFFICIENT,
                                  sftk_item_expand(&tmpKey.coefficient));
        if (crv != CKR_OK)
            goto loser;
    }
    rv = SECSuccess;

/* we're done (one way or the other), clean up all our stuff */
loser:
    if (tmpKey.arena) {
        PORT_FreeArena(tmpKey.arena, PR_TRUE);
    }
    if (modulus) {
        sftk_FreeAttribute(modulus);
    }
    if (prime1) {
        sftk_FreeAttribute(prime1);
    }
    if (prime2) {
        sftk_FreeAttribute(prime2);
    }
    if (privateExponent) {
        sftk_FreeAttribute(privateExponent);
    }
    if (publicExponent) {
        sftk_FreeAttribute(publicExponent);
    }
    if (exponent1) {
        sftk_FreeAttribute(exponent1);
    }
    if (exponent2) {
        sftk_FreeAttribute(exponent2);
    }
    if (coefficient) {
        sftk_FreeAttribute(coefficient);
    }
    return rv;
}

/* Generate a low private key structure from an object */
NSSLOWKEYPrivateKey *
sftk_GetPrivKey(SFTKObject *object, CK_KEY_TYPE key_type, CK_RV *crvp)
{
    NSSLOWKEYPrivateKey *priv = NULL;

    if (object->objclass != CKO_PRIVATE_KEY) {
        *crvp = CKR_KEY_TYPE_INCONSISTENT;
        return NULL;
    }
    if (object->objectInfo) {
        *crvp = CKR_OK;
        return (NSSLOWKEYPrivateKey *)object->objectInfo;
    }

    priv = sftk_mkPrivKey(object, key_type, crvp);
    object->objectInfo = priv;
    object->infoFree = (SFTKFree)nsslowkey_DestroyPrivateKey;
    return priv;
}

/*
 **************************** Symetric Key utils ************************
 */
/*
 * set the DES key with parity bits correctly
 */
void
sftk_FormatDESKey(unsigned char *key, int length)
{
    int i;

    /* format the des key */
    for (i = 0; i < length; i++) {
        key[i] = parityTable[key[i] >> 1];
    }
}

/*
 * check a des key (des2 or des3 subkey) for weak keys.
 */
PRBool
sftk_CheckDESKey(unsigned char *key)
{
    int i;

    /* format the des key with parity  */
    sftk_FormatDESKey(key, 8);

    for (i = 0; i < sftk_desWeakTableSize; i++) {
        if (PORT_Memcmp(key, sftk_desWeakTable[i], 8) == 0) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

/*
 * check if a des or triple des key is weak.
 */
PRBool
sftk_IsWeakKey(unsigned char *key, CK_KEY_TYPE key_type)
{

    switch (key_type) {
        case CKK_DES:
            return sftk_CheckDESKey(key);
        case CKM_DES2_KEY_GEN:
            if (sftk_CheckDESKey(key))
                return PR_TRUE;
            return sftk_CheckDESKey(&key[8]);
        case CKM_DES3_KEY_GEN:
            if (sftk_CheckDESKey(key))
                return PR_TRUE;
            if (sftk_CheckDESKey(&key[8]))
                return PR_TRUE;
            return sftk_CheckDESKey(&key[16]);
        default:
            break;
    }
    return PR_FALSE;
}

/**********************************************************************
 *
 *     Start of PKCS 11 functions
 *
 **********************************************************************/

/* return the function list */
CK_RV
NSC_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{
    *pFunctionList = (CK_FUNCTION_LIST_PTR)&sftk_funcList;
    return CKR_OK;
}

/* return the function list */
CK_RV
C_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{
    return NSC_GetFunctionList(pFunctionList);
}

static PLHashNumber
sftk_HashNumber(const void *key)
{
    return (PLHashNumber)((char *)key - (char *)NULL);
}

/*
 * eventually I'd like to expunge all occurances of XXX_SLOT_ID and
 * just go with the info in the slot. This is one place, however,
 * where it might be a little difficult.
 */
const char *
sftk_getDefTokName(CK_SLOT_ID slotID)
{
    static char buf[33];

    switch (slotID) {
        case NETSCAPE_SLOT_ID:
            return "NSS Generic Crypto Services     ";
        case PRIVATE_KEY_SLOT_ID:
            return "NSS Certificate DB              ";
        case FIPS_SLOT_ID:
            return "NSS FIPS 140-2 Certificate DB   ";
        default:
            break;
    }
    sprintf(buf, "NSS Application Token %08x  ", (unsigned int)slotID);
    return buf;
}

const char *
sftk_getDefSlotName(CK_SLOT_ID slotID)
{
    static char buf[65];

    switch (slotID) {
        case NETSCAPE_SLOT_ID:
            return "NSS Internal Cryptographic Services                             ";
        case PRIVATE_KEY_SLOT_ID:
            return "NSS User Private Key and Certificate Services                   ";
        case FIPS_SLOT_ID:
            return "NSS FIPS 140-2 User Private Key Services                        ";
        default:
            break;
    }
    sprintf(buf,
            "NSS Application Slot %08x                                   ",
            (unsigned int)slotID);
    return buf;
}

static CK_ULONG nscSlotCount[2] = { 0, 0 };
static CK_SLOT_ID_PTR nscSlotList[2] = { NULL, NULL };
static CK_ULONG nscSlotListSize[2] = { 0, 0 };
static PLHashTable *nscSlotHashTable[2] = { NULL, NULL };

static unsigned int
sftk_GetModuleIndex(CK_SLOT_ID slotID)
{
    if ((slotID == FIPS_SLOT_ID) || (slotID >= SFTK_MIN_FIPS_USER_SLOT_ID)) {
        return NSC_FIPS_MODULE;
    }
    return NSC_NON_FIPS_MODULE;
}

/* look up a slot structure from the ID (used to be a macro when we only
 * had two slots) */
/* if all is true, return the slot even if it has been 'unloaded' */
/* if all is false, only return the slots which are present */
SFTKSlot *
sftk_SlotFromID(CK_SLOT_ID slotID, PRBool all)
{
    SFTKSlot *slot;
    int index = sftk_GetModuleIndex(slotID);

    if (nscSlotHashTable[index] == NULL)
        return NULL;
    slot = (SFTKSlot *)PL_HashTableLookupConst(nscSlotHashTable[index],
                                               (void *)slotID);
    /* cleared slots shouldn't 'show up' */
    if (slot && !all && !slot->present)
        slot = NULL;
    return slot;
}

SFTKSlot *
sftk_SlotFromSessionHandle(CK_SESSION_HANDLE handle)
{
    CK_ULONG slotIDIndex = (handle >> 24) & 0x7f;
    CK_ULONG moduleIndex = (handle >> 31) & 1;

    if (slotIDIndex >= nscSlotCount[moduleIndex]) {
        return NULL;
    }

    return sftk_SlotFromID(nscSlotList[moduleIndex][slotIDIndex], PR_FALSE);
}

static CK_RV
sftk_RegisterSlot(SFTKSlot *slot, int moduleIndex)
{
    PLHashEntry *entry;
    unsigned int index;

    index = sftk_GetModuleIndex(slot->slotID);

    /* make sure the slotID for this module is valid */
    if (moduleIndex != index) {
        return CKR_SLOT_ID_INVALID;
    }

    if (nscSlotList[index] == NULL) {
        nscSlotListSize[index] = NSC_SLOT_LIST_BLOCK_SIZE;
        nscSlotList[index] = (CK_SLOT_ID *)
            PORT_ZAlloc(nscSlotListSize[index] * sizeof(CK_SLOT_ID));
        if (nscSlotList[index] == NULL) {
            return CKR_HOST_MEMORY;
        }
    }
    if (nscSlotCount[index] >= nscSlotListSize[index]) {
        CK_SLOT_ID *oldNscSlotList = nscSlotList[index];
        CK_ULONG oldNscSlotListSize = nscSlotListSize[index];
        nscSlotListSize[index] += NSC_SLOT_LIST_BLOCK_SIZE;
        nscSlotList[index] = (CK_SLOT_ID *)PORT_Realloc(oldNscSlotList,
                                                        nscSlotListSize[index] * sizeof(CK_SLOT_ID));
        if (nscSlotList[index] == NULL) {
            nscSlotList[index] = oldNscSlotList;
            nscSlotListSize[index] = oldNscSlotListSize;
            return CKR_HOST_MEMORY;
        }
    }

    if (nscSlotHashTable[index] == NULL) {
        nscSlotHashTable[index] = PL_NewHashTable(64, sftk_HashNumber,
                                                  PL_CompareValues, PL_CompareValues, NULL, 0);
        if (nscSlotHashTable[index] == NULL) {
            return CKR_HOST_MEMORY;
        }
    }

    entry = PL_HashTableAdd(nscSlotHashTable[index], (void *)slot->slotID, slot);
    if (entry == NULL) {
        return CKR_HOST_MEMORY;
    }
    slot->index = (nscSlotCount[index] & 0x7f) | ((index << 7) & 0x80);
    nscSlotList[index][nscSlotCount[index]++] = slot->slotID;

    return CKR_OK;
}

/*
 * ths function has all the common initialization that happens whenever we
 * create a new slot or repurpose an old slot (only valid for slotID's 4
 * and greater).
 *
 * things that are not reinitialized are:
 *   slotID (can't change)
 *   slotDescription (can't change once defined)
 *   the locks and hash tables (difficult to change in running code, and
 *     unnecessary. hash tables and list are cleared on shutdown, but they
 *     are cleared in a 'friendly' way).
 *   session and object ID counters -- so any old sessions and objects in the
 *     application will get properly notified that the world has changed.
 *
 * things that are reinitialized:
 *   database (otherwise what would the point be;).
 *   state variables related to databases.
 *   session count stat info.
 *   tokenDescription.
 *
 * NOTE: slotID's 4 and greater show up as removable devices.
 *
 */
CK_RV
SFTK_SlotReInit(SFTKSlot *slot, char *configdir, char *updatedir,
                char *updateID, sftk_token_parameters *params, int moduleIndex)
{
    PRBool needLogin = !params->noKeyDB;
    CK_RV crv;

    slot->hasTokens = PR_FALSE;
    slot->sessionIDConflict = 0;
    slot->sessionCount = 0;
    slot->rwSessionCount = 0;
    slot->needLogin = PR_FALSE;
    slot->isLoggedIn = PR_FALSE;
    slot->ssoLoggedIn = PR_FALSE;
    slot->DB_loaded = PR_FALSE;
    slot->certDB = NULL;
    slot->keyDB = NULL;
    slot->minimumPinLen = 0;
    slot->readOnly = params->readOnly;
    sftk_setStringName(params->tokdes ? params->tokdes : sftk_getDefTokName(slot->slotID), slot->tokDescription,
                       sizeof(slot->tokDescription), PR_TRUE);
    sftk_setStringName(params->updtokdes ? params->updtokdes : " ",
                       slot->updateTokDescription,
                       sizeof(slot->updateTokDescription), PR_TRUE);

    if ((!params->noCertDB) || (!params->noKeyDB)) {
        SFTKDBHandle *certHandle = NULL;
        SFTKDBHandle *keyHandle = NULL;
        crv = sftk_DBInit(params->configdir ? params->configdir : configdir,
                          params->certPrefix, params->keyPrefix,
                          params->updatedir ? params->updatedir : updatedir,
                          params->updCertPrefix, params->updKeyPrefix,
                          params->updateID ? params->updateID : updateID,
                          params->readOnly, params->noCertDB, params->noKeyDB,
                          params->forceOpen,
                          moduleIndex == NSC_FIPS_MODULE,
                          &certHandle, &keyHandle);
        if (crv != CKR_OK) {
            goto loser;
        }

        slot->certDB = certHandle;
        slot->keyDB = keyHandle;
    }
    if (needLogin) {
        /* if the data base is initialized with a null password,remember that */
        slot->needLogin =
            (PRBool)!sftk_hasNullPassword(slot, slot->keyDB);
        if ((params->minPW >= 0) && (params->minPW <= SFTK_MAX_PIN)) {
            slot->minimumPinLen = params->minPW;
        }
        if ((slot->minimumPinLen == 0) && (params->pwRequired)) {
            slot->minimumPinLen = 1;
        }
        /* Make sure the pin len is set to the Minimum allowed value for fips
         * when in FIPS mode. NOTE: we don't set it if the database has not
         * been initialized yet so that we can init into level1 mode if needed
         */
        if ((sftkdb_HasPasswordSet(slot->keyDB) == SECSuccess) &&
            (moduleIndex == NSC_FIPS_MODULE) &&
            (slot->minimumPinLen < FIPS_MIN_PIN)) {
            slot->minimumPinLen = FIPS_MIN_PIN;
        }
    }

    slot->present = PR_TRUE;
    return CKR_OK;

loser:
    SFTK_ShutdownSlot(slot);
    return crv;
}

/*
 * initialize one of the slot structures. figure out which by the ID
 */
CK_RV
SFTK_SlotInit(char *configdir, char *updatedir, char *updateID,
              sftk_token_parameters *params, int moduleIndex)
{
    unsigned int i;
    CK_SLOT_ID slotID = params->slotID;
    SFTKSlot *slot;
    CK_RV crv = CKR_HOST_MEMORY;

    /*
     * first we initialize everything that is 'permanent' with this slot.
     * that is everything we aren't going to shutdown if we close this slot
     * and open it up again with different databases */

    slot = PORT_ZNew(SFTKSlot);

    if (slot == NULL) {
        return CKR_HOST_MEMORY;
    }

    slot->optimizeSpace = params->optimizeSpace;
    if (slot->optimizeSpace) {
        slot->sessObjHashSize = SPACE_SESSION_OBJECT_HASH_SIZE;
        slot->sessHashSize = SPACE_SESSION_HASH_SIZE;
        slot->numSessionLocks = 1;
    } else {
        slot->sessObjHashSize = TIME_SESSION_OBJECT_HASH_SIZE;
        slot->sessHashSize = TIME_SESSION_HASH_SIZE;
        slot->numSessionLocks = slot->sessHashSize / BUCKETS_PER_SESSION_LOCK;
    }
    slot->sessionLockMask = slot->numSessionLocks - 1;

    slot->slotLock = PZ_NewLock(nssILockSession);
    if (slot->slotLock == NULL)
        goto mem_loser;
    slot->sessionLock = PORT_ZNewArray(PZLock *, slot->numSessionLocks);
    if (slot->sessionLock == NULL)
        goto mem_loser;
    for (i = 0; i < slot->numSessionLocks; i++) {
        slot->sessionLock[i] = PZ_NewLock(nssILockSession);
        if (slot->sessionLock[i] == NULL)
            goto mem_loser;
    }
    slot->objectLock = PZ_NewLock(nssILockObject);
    if (slot->objectLock == NULL)
        goto mem_loser;
    slot->pwCheckLock = PR_NewLock();
    if (slot->pwCheckLock == NULL)
        goto mem_loser;
    slot->head = PORT_ZNewArray(SFTKSession *, slot->sessHashSize);
    if (slot->head == NULL)
        goto mem_loser;
    slot->sessObjHashTable = PORT_ZNewArray(SFTKObject *, slot->sessObjHashSize);
    if (slot->sessObjHashTable == NULL)
        goto mem_loser;
    slot->tokObjHashTable = PL_NewHashTable(64, sftk_HashNumber, PL_CompareValues,
                                            SECITEM_HashCompare, NULL, 0);
    if (slot->tokObjHashTable == NULL)
        goto mem_loser;

    slot->sessionIDCount = 0;
    slot->sessionObjectHandleCount = minSessionObjectHandle;
    slot->slotID = slotID;
    sftk_setStringName(params->slotdes ? params->slotdes : sftk_getDefSlotName(slotID), slot->slotDescription,
                       sizeof(slot->slotDescription), PR_TRUE);

    /* call the reinit code to set everything that changes between token
     * init calls */
    crv = SFTK_SlotReInit(slot, configdir, updatedir, updateID,
                          params, moduleIndex);
    if (crv != CKR_OK) {
        goto loser;
    }
    crv = sftk_RegisterSlot(slot, moduleIndex);
    if (crv != CKR_OK) {
        goto loser;
    }
    return CKR_OK;

mem_loser:
    crv = CKR_HOST_MEMORY;
loser:
    SFTK_DestroySlotData(slot);
    return crv;
}

CK_RV
sftk_CloseAllSessions(SFTKSlot *slot, PRBool logout)
{
    SFTKSession *session;
    unsigned int i;
    SFTKDBHandle *handle;

    /* first log out the card */
    /* special case - if we are in a middle of upgrade, we want to close the
     * sessions to fake a token removal to tell the upper level code we have
     * switched from one database to another, but we don't want to
     * explicity logout in case we can continue the upgrade with the
     * existing password if possible.
     */
    if (logout) {
        handle = sftk_getKeyDB(slot);
        SKIP_AFTER_FORK(PZ_Lock(slot->slotLock));
        slot->isLoggedIn = PR_FALSE;
        if (slot->needLogin && handle) {
            sftkdb_ClearPassword(handle);
        }
        SKIP_AFTER_FORK(PZ_Unlock(slot->slotLock));
        if (handle) {
            sftk_freeDB(handle);
        }
    }

    /* now close all the current sessions */
    /* NOTE: If you try to open new sessions before NSC_CloseAllSessions
     * completes, some of those new sessions may or may not be closed by
     * NSC_CloseAllSessions... but any session running when this code starts
     * will guarrenteed be close, and no session will be partially closed */
    for (i = 0; i < slot->sessHashSize; i++) {
        PZLock *lock = SFTK_SESSION_LOCK(slot, i);
        do {
            SKIP_AFTER_FORK(PZ_Lock(lock));
            session = slot->head[i];
            /* hand deque */
            /* this duplicates function of NSC_close session functions, but
             * because we know that we are freeing all the sessions, we can
             * do more efficient processing */
            if (session) {
                slot->head[i] = session->next;
                if (session->next)
                    session->next->prev = NULL;
                session->next = session->prev = NULL;
                SKIP_AFTER_FORK(PZ_Unlock(lock));
                SKIP_AFTER_FORK(PZ_Lock(slot->slotLock));
                --slot->sessionCount;
                SKIP_AFTER_FORK(PZ_Unlock(slot->slotLock));
                if (session->info.flags & CKF_RW_SESSION) {
                    (void)PR_ATOMIC_DECREMENT(&slot->rwSessionCount);
                }
            } else {
                SKIP_AFTER_FORK(PZ_Unlock(lock));
            }
            if (session)
                sftk_FreeSession(session);
        } while (session != NULL);
    }
    return CKR_OK;
}

/*
 * shut down the databases.
 * we get the slot lock (which also protects slot->certDB and slot->keyDB)
 * and clear the values so the new users will not find the databases.
 * once things are clear, we can release our references to the databases.
 * The databases will close when the last reference is released.
 *
 * We use reference counts so that we don't crash if someone shuts down
 * a token that another thread is actively using.
 */
static void
sftk_DBShutdown(SFTKSlot *slot)
{
    SFTKDBHandle *certHandle;
    SFTKDBHandle *keyHandle;
    SKIP_AFTER_FORK(PZ_Lock(slot->slotLock));
    certHandle = slot->certDB;
    slot->certDB = NULL;
    keyHandle = slot->keyDB;
    slot->keyDB = NULL;
    SKIP_AFTER_FORK(PZ_Unlock(slot->slotLock));
    if (certHandle) {
        sftk_freeDB(certHandle);
    }
    if (keyHandle) {
        sftk_freeDB(keyHandle);
    }
}

CK_RV
SFTK_ShutdownSlot(SFTKSlot *slot)
{
    /* make sure no new PK11 calls work except C_GetSlotInfo */
    slot->present = PR_FALSE;

    /* close all outstanding sessions
     * the sessHashSize variable guarentees we have all the session
     * mechanism set up */
    if (slot->head) {
        sftk_CloseAllSessions(slot, PR_TRUE);
    }

    /* clear all objects.. session objects are cleared as a result of
     * closing all the sessions. We just need to clear the token object
     * cache. slot->tokObjHashTable guarentees we have the token
     * infrastructure set up. */
    if (slot->tokObjHashTable) {
        SFTK_ClearTokenKeyHashTable(slot);
    }

    /* clear the slot description for the next guy */
    PORT_Memset(slot->tokDescription, 0, sizeof(slot->tokDescription));

    /* now shut down the databases. */
    sftk_DBShutdown(slot);
    return CKR_OK;
}

/*
 * initialize one of the slot structures. figure out which by the ID
 */
CK_RV
SFTK_DestroySlotData(SFTKSlot *slot)
{
    unsigned int i;

    SFTK_ShutdownSlot(slot);

    if (slot->tokObjHashTable) {
        PL_HashTableDestroy(slot->tokObjHashTable);
        slot->tokObjHashTable = NULL;
    }

    if (slot->sessObjHashTable) {
        PORT_Free(slot->sessObjHashTable);
        slot->sessObjHashTable = NULL;
    }
    slot->sessObjHashSize = 0;

    if (slot->head) {
        PORT_Free(slot->head);
        slot->head = NULL;
    }
    slot->sessHashSize = 0;

    /* OK everything has been disassembled, now we can finally get rid
     * of the locks */
    SKIP_AFTER_FORK(PZ_DestroyLock(slot->slotLock));
    slot->slotLock = NULL;
    if (slot->sessionLock) {
        for (i = 0; i < slot->numSessionLocks; i++) {
            if (slot->sessionLock[i]) {
                SKIP_AFTER_FORK(PZ_DestroyLock(slot->sessionLock[i]));
                slot->sessionLock[i] = NULL;
            }
        }
        PORT_Free(slot->sessionLock);
        slot->sessionLock = NULL;
    }
    if (slot->objectLock) {
        SKIP_AFTER_FORK(PZ_DestroyLock(slot->objectLock));
        slot->objectLock = NULL;
    }
    if (slot->pwCheckLock) {
        SKIP_AFTER_FORK(PR_DestroyLock(slot->pwCheckLock));
        slot->pwCheckLock = NULL;
    }
    PORT_Free(slot);
    return CKR_OK;
}

/*
 * handle the SECMOD.db
 */
char **
NSC_ModuleDBFunc(unsigned long function, char *parameters, void *args)
{
    char *secmod = NULL;
    char *appName = NULL;
    char *filename = NULL;
    NSSDBType dbType = NSS_DB_TYPE_NONE;
    PRBool rw;
    static char *success = "Success";
    char **rvstr = NULL;

    rvstr = NSSUTIL_DoModuleDBFunction(function, parameters, args);
    if (rvstr != NULL) {
        return rvstr;
    }

    if (PORT_GetError() != SEC_ERROR_LEGACY_DATABASE) {
        return NULL;
    }

    /* The legacy database uses the old dbm, which is only linked with the
     * legacy DB handler, which is only callable from softoken */

    secmod = _NSSUTIL_GetSecmodName(parameters, &dbType, &appName,
                                    &filename, &rw);

    switch (function) {
        case SECMOD_MODULE_DB_FUNCTION_FIND:
            if (secmod == NULL) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                goto loser;
            }
            if (rw && (dbType != NSS_DB_TYPE_LEGACY) &&
                (dbType != NSS_DB_TYPE_MULTIACCESS)) {
                /* if we get here, we are trying to update the local database */
                /* force data from the legacy DB */
                char *oldSecmod = NULL;
                char *oldAppName = NULL;
                char *oldFilename = NULL;
                PRBool oldrw;
                char **strings = NULL;
                int i;

                dbType = NSS_DB_TYPE_LEGACY;
                oldSecmod = _NSSUTIL_GetSecmodName(parameters, &dbType, &oldAppName,
                                                   &oldFilename, &oldrw);
                strings = sftkdbCall_ReadSecmodDB(appName, oldFilename, oldSecmod,
                                                  (char *)parameters, oldrw);
                if (strings) {
                    /* write out the strings */
                    for (i = 0; strings[i]; i++) {
                        NSSUTIL_DoModuleDBFunction(SECMOD_MODULE_DB_FUNCTION_ADD,
                                                   parameters, strings[i]);
                    }
                    sftkdbCall_ReleaseSecmodDBData(oldAppName, oldFilename, oldSecmod,
                                                   (char **)strings, oldrw);
                } else {
                    /* write out a dummy record */
                    NSSUTIL_DoModuleDBFunction(SECMOD_MODULE_DB_FUNCTION_ADD,
                                               parameters, " ");
                }
                if (oldSecmod) {
                    PR_smprintf_free(oldSecmod);
                }
                if (oldAppName) {
                    PORT_Free(oldAppName);
                }
                if (oldFilename) {
                    PORT_Free(oldFilename);
                }
                rvstr = NSSUTIL_DoModuleDBFunction(function, parameters, args);
                break;
            }
            rvstr = sftkdbCall_ReadSecmodDB(appName, filename, secmod,
                                            (char *)parameters, rw);
            break;
        case SECMOD_MODULE_DB_FUNCTION_ADD:
            if (secmod == NULL) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                goto loser;
            }
            rvstr = (sftkdbCall_AddSecmodDB(appName, filename, secmod,
                                            (char *)args, rw) == SECSuccess)
                        ? &success
                        : NULL;
            break;
        case SECMOD_MODULE_DB_FUNCTION_DEL:
            if (secmod == NULL) {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                goto loser;
            }
            rvstr = (sftkdbCall_DeleteSecmodDB(appName, filename, secmod,
                                               (char *)args, rw) == SECSuccess)
                        ? &success
                        : NULL;
            break;
        case SECMOD_MODULE_DB_FUNCTION_RELEASE:
            rvstr = (sftkdbCall_ReleaseSecmodDBData(appName, filename, secmod,
                                                    (char **)args, rw) == SECSuccess)
                        ? &success
                        : NULL;
            break;
    }

loser:
    if (secmod)
        PR_smprintf_free(secmod);
    if (appName)
        PORT_Free(appName);
    if (filename)
        PORT_Free(filename);
    return rvstr;
}

static void
nscFreeAllSlots(int moduleIndex)
{
    /* free all the slots */
    SFTKSlot *slot = NULL;
    CK_SLOT_ID slotID;
    int i;

    if (nscSlotList[moduleIndex]) {
        CK_ULONG tmpSlotCount = nscSlotCount[moduleIndex];
        CK_SLOT_ID_PTR tmpSlotList = nscSlotList[moduleIndex];
        PLHashTable *tmpSlotHashTable = nscSlotHashTable[moduleIndex];

        /* first close all the session */
        for (i = 0; i < (int)tmpSlotCount; i++) {
            slotID = tmpSlotList[i];
            (void)NSC_CloseAllSessions(slotID);
        }

        /* now clear out the statics */
        nscSlotList[moduleIndex] = NULL;
        nscSlotCount[moduleIndex] = 0;
        nscSlotHashTable[moduleIndex] = NULL;
        nscSlotListSize[moduleIndex] = 0;

        for (i = 0; i < (int)tmpSlotCount; i++) {
            slotID = tmpSlotList[i];
            slot = (SFTKSlot *)
                PL_HashTableLookup(tmpSlotHashTable, (void *)slotID);
            PORT_Assert(slot);
            if (!slot)
                continue;
            SFTK_DestroySlotData(slot);
            PL_HashTableRemove(tmpSlotHashTable, (void *)slotID);
        }
        PORT_Free(tmpSlotList);
        PL_HashTableDestroy(tmpSlotHashTable);
    }
}

static void
sftk_closePeer(PRBool isFIPS)
{
    CK_SLOT_ID slotID = isFIPS ? PRIVATE_KEY_SLOT_ID : FIPS_SLOT_ID;
    SFTKSlot *slot;
    int moduleIndex = isFIPS ? NSC_NON_FIPS_MODULE : NSC_FIPS_MODULE;
    PLHashTable *tmpSlotHashTable = nscSlotHashTable[moduleIndex];

    slot = (SFTKSlot *)PL_HashTableLookup(tmpSlotHashTable, (void *)slotID);
    if (slot == NULL) {
        return;
    }
    sftk_DBShutdown(slot);
    return;
}

/* NSC_Initialize initializes the Cryptoki library. */
CK_RV
nsc_CommonInitialize(CK_VOID_PTR pReserved, PRBool isFIPS)
{
    CK_RV crv = CKR_OK;
    SECStatus rv;
    CK_C_INITIALIZE_ARGS *init_args = (CK_C_INITIALIZE_ARGS *)pReserved;
    int i;
    int moduleIndex = isFIPS ? NSC_FIPS_MODULE : NSC_NON_FIPS_MODULE;

    if (isFIPS) {
        loginWaitTime = PR_SecondsToInterval(1);
    }

    ENABLE_FORK_CHECK();

    rv = SECOID_Init();
    if (rv != SECSuccess) {
        crv = CKR_DEVICE_ERROR;
        return crv;
    }

    rv = RNG_RNGInit(); /* initialize random number generator */
    if (rv != SECSuccess) {
        crv = CKR_DEVICE_ERROR;
        return crv;
    }
    rv = BL_Init(); /* initialize freebl engine */
    if (rv != SECSuccess) {
        crv = CKR_DEVICE_ERROR;
        return crv;
    }

    /* NOTE:
     * we should be getting out mutexes from this list, not statically binding
     * them from NSPR. This should happen before we allow the internal to split
     * off from the rest on NSS.
     */

    /* initialize the key and cert db's */
    if (init_args && (!(init_args->flags & CKF_OS_LOCKING_OK))) {
        if (init_args->CreateMutex && init_args->DestroyMutex &&
            init_args->LockMutex && init_args->UnlockMutex) {
            /* softoken always uses NSPR (ie. OS locking), and doesn't know how
             * to use the lock functions provided by the application.
             */
            crv = CKR_CANT_LOCK;
            return crv;
        }
        if (init_args->CreateMutex || init_args->DestroyMutex ||
            init_args->LockMutex || init_args->UnlockMutex) {
            /* only some of the lock functions were provided by the
             * application. This is invalid per PKCS#11 spec.
             */
            crv = CKR_ARGUMENTS_BAD;
            return crv;
        }
    }
    crv = CKR_ARGUMENTS_BAD;
    if ((init_args && init_args->LibraryParameters)) {
        sftk_parameters paramStrings;

        crv = sftk_parseParameters((char *)init_args->LibraryParameters, &paramStrings, isFIPS);
        if (crv != CKR_OK) {
            return crv;
        }
        crv = sftk_configure(paramStrings.man, paramStrings.libdes);
        if (crv != CKR_OK) {
            goto loser;
        }

        /* if we have a peer already open, have him close his DB's so we
         * don't clobber each other. */
        if ((isFIPS && nsc_init) || (!isFIPS && nsf_init)) {
            sftk_closePeer(isFIPS);
            if (sftk_audit_enabled) {
                if (isFIPS && nsc_init) {
                    sftk_LogAuditMessage(NSS_AUDIT_INFO, NSS_AUDIT_FIPS_STATE,
                                         "enabled FIPS mode");
                } else {
                    sftk_LogAuditMessage(NSS_AUDIT_INFO, NSS_AUDIT_FIPS_STATE,
                                         "disabled FIPS mode");
                }
            }
        }

        for (i = 0; i < paramStrings.token_count; i++) {
            crv = SFTK_SlotInit(paramStrings.configdir,
                                paramStrings.updatedir, paramStrings.updateID,
                                &paramStrings.tokens[i], moduleIndex);
            if (crv != CKR_OK) {
                nscFreeAllSlots(moduleIndex);
                break;
            }
        }
    loser:
        sftk_freeParams(&paramStrings);
    }
    if (CKR_OK == crv) {
        sftk_InitFreeLists();
    }

#ifndef NO_FORK_CHECK
    if (CKR_OK == crv) {
#if defined(CHECK_FORK_MIXED)
        /* Before Solaris 10, fork handlers are not unregistered at dlclose()
         * time. So, we only use pthread_atfork on Solaris 10 and later. For
         * earlier versions, we use PID checks.
         */
        char buf[200];
        int major = 0, minor = 0;

        long rv = sysinfo(SI_RELEASE, buf, sizeof(buf));
        if (rv > 0 && rv < sizeof(buf)) {
            if (2 == sscanf(buf, "%d.%d", &major, &minor)) {
                /* Are we on Solaris 10 or greater ? */
                if (major > 5 || (5 == major && minor >= 10)) {
                    /* we are safe to use pthread_atfork */
                    usePthread_atfork = PR_TRUE;
                }
            }
        }
        if (usePthread_atfork) {
            pthread_atfork(NULL, NULL, ForkedChild);
        } else {
            myPid = getpid();
        }

#elif defined(CHECK_FORK_PTHREAD)
        pthread_atfork(NULL, NULL, ForkedChild);
#elif defined(CHECK_FORK_GETPID)
        myPid = getpid();
#else
#error Incorrect fork check method.
#endif
    }
#endif
    return crv;
}

CK_RV
NSC_Initialize(CK_VOID_PTR pReserved)
{
    CK_RV crv;

    sftk_ForkReset(pReserved, &crv);

    if (nsc_init) {
        return CKR_CRYPTOKI_ALREADY_INITIALIZED;
    }
    crv = nsc_CommonInitialize(pReserved, PR_FALSE);
    nsc_init = (PRBool)(crv == CKR_OK);
    return crv;
}

/* NSC_Finalize indicates that an application is done with the
 * Cryptoki library.*/
CK_RV
nsc_CommonFinalize(CK_VOID_PTR pReserved, PRBool isFIPS)
{
    /* propagate the fork status to freebl and util */
    BL_SetForkState(parentForkedAfterC_Initialize);
    UTIL_SetForkState(parentForkedAfterC_Initialize);

    nscFreeAllSlots(isFIPS ? NSC_FIPS_MODULE : NSC_NON_FIPS_MODULE);

    /* don't muck with the globals if our peer is still initialized */
    if (isFIPS && nsc_init) {
        return CKR_OK;
    }
    if (!isFIPS && nsf_init) {
        return CKR_OK;
    }

    sftk_CleanupFreeLists();
    sftkdb_Shutdown();

    /* This function does not discard all our previously aquired entropy. */
    RNG_RNGShutdown();

    /* tell freeBL to clean up after itself */
    BL_Cleanup();

    /* reset fork status in freebl. We must do this before BL_Unload so that
     * this call doesn't force freebl to be reloaded. */
    BL_SetForkState(PR_FALSE);

    /* unload freeBL shared library from memory. This may only decrement the
     * OS refcount if it's been loaded multiple times, eg. by libssl */
    BL_Unload();

    /* clean up the default OID table */
    SECOID_Shutdown();

    /* reset fork status in util */
    UTIL_SetForkState(PR_FALSE);

    nsc_init = PR_FALSE;

#ifdef CHECK_FORK_MIXED
    if (!usePthread_atfork) {
        myPid = 0; /* allow CHECK_FORK in the next softoken initialization to
                    * succeed */
    } else {
        forked = PR_FALSE; /* allow reinitialization */
    }
#elif defined(CHECK_FORK_GETPID)
    myPid = 0; /* allow reinitialization */
#elif defined(CHECK_FORK_PTHREAD)
    forked = PR_FALSE; /* allow reinitialization */
#endif
    return CKR_OK;
}

/* Hard-reset the entire softoken PKCS#11 module if the parent process forked
 * while it was initialized. */
PRBool
sftk_ForkReset(CK_VOID_PTR pReserved, CK_RV *crv)
{
#ifndef NO_FORK_CHECK
    if (PARENT_FORKED()) {
        parentForkedAfterC_Initialize = PR_TRUE;
        if (nsc_init) {
            /* finalize non-FIPS token */
            *crv = nsc_CommonFinalize(pReserved, PR_FALSE);
            PORT_Assert(CKR_OK == *crv);
            nsc_init = (PRBool) !(*crv == CKR_OK);
        }
        if (nsf_init) {
            /* finalize FIPS token */
            *crv = nsc_CommonFinalize(pReserved, PR_TRUE);
            PORT_Assert(CKR_OK == *crv);
            nsf_init = (PRBool) !(*crv == CKR_OK);
        }
        parentForkedAfterC_Initialize = PR_FALSE;
        return PR_TRUE;
    }
#endif
    return PR_FALSE;
}

/* NSC_Finalize indicates that an application is done with the
 * Cryptoki library.*/
CK_RV
NSC_Finalize(CK_VOID_PTR pReserved)
{
    CK_RV crv;

    /* reset entire PKCS#11 module upon fork */
    if (sftk_ForkReset(pReserved, &crv)) {
        return crv;
    }

    if (!nsc_init) {
        return CKR_OK;
    }

    crv = nsc_CommonFinalize(pReserved, PR_FALSE);

    nsc_init = (PRBool) !(crv == CKR_OK);

    return crv;
}

extern const char __nss_softokn_version[];

/* NSC_GetInfo returns general information about Cryptoki. */
CK_RV
NSC_GetInfo(CK_INFO_PTR pInfo)
{
#define NSS_VERSION_VARIABLE __nss_softokn_version
#include "verref.h"

    CHECK_FORK();

    pInfo->cryptokiVersion.major = 2;
    pInfo->cryptokiVersion.minor = 20;
    PORT_Memcpy(pInfo->manufacturerID, manufacturerID, 32);
    pInfo->libraryVersion.major = SOFTOKEN_VMAJOR;
    pInfo->libraryVersion.minor = SOFTOKEN_VMINOR;
    PORT_Memcpy(pInfo->libraryDescription, libraryDescription, 32);
    pInfo->flags = 0;
    return CKR_OK;
}

/* NSC_GetSlotList obtains a list of slots in the system. */
CK_RV
nsc_CommonGetSlotList(CK_BBOOL tokenPresent,
                      CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount, int moduleIndex)
{
    *pulCount = nscSlotCount[moduleIndex];
    if (pSlotList != NULL) {
        PORT_Memcpy(pSlotList, nscSlotList[moduleIndex],
                    nscSlotCount[moduleIndex] * sizeof(CK_SLOT_ID));
    }
    return CKR_OK;
}

/* NSC_GetSlotList obtains a list of slots in the system. */
CK_RV
NSC_GetSlotList(CK_BBOOL tokenPresent,
                CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount)
{
    CHECK_FORK();
    return nsc_CommonGetSlotList(tokenPresent, pSlotList, pulCount,
                                 NSC_NON_FIPS_MODULE);
}

/* NSC_GetSlotInfo obtains information about a particular slot in the system. */
CK_RV
NSC_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
    SFTKSlot *slot = sftk_SlotFromID(slotID, PR_TRUE);

    CHECK_FORK();

    if (slot == NULL)
        return CKR_SLOT_ID_INVALID;

    PORT_Memcpy(pInfo->manufacturerID, manufacturerID,
                sizeof(pInfo->manufacturerID));
    PORT_Memcpy(pInfo->slotDescription, slot->slotDescription,
                sizeof(pInfo->slotDescription));
    pInfo->flags = (slot->present) ? CKF_TOKEN_PRESENT : 0;

    /* all user defined slots are defined as removable */
    if (slotID >= SFTK_MIN_USER_SLOT_ID) {
        pInfo->flags |= CKF_REMOVABLE_DEVICE;
    } else {
        /* In the case where we are doing a merge update, we need
         * the DB slot to be removable so the token name can change
         * appropriately. */
        SFTKDBHandle *handle = sftk_getKeyDB(slot);
        if (handle) {
            if (sftkdb_InUpdateMerge(handle)) {
                pInfo->flags |= CKF_REMOVABLE_DEVICE;
            }
            sftk_freeDB(handle);
        }
    }

    /* ok we really should read it out of the keydb file. */
    /* pInfo->hardwareVersion.major = NSSLOWKEY_DB_FILE_VERSION; */
    pInfo->hardwareVersion.major = SOFTOKEN_VMAJOR;
    pInfo->hardwareVersion.minor = SOFTOKEN_VMINOR;
    pInfo->firmwareVersion.major = SOFTOKEN_VPATCH;
    pInfo->firmwareVersion.minor = SOFTOKEN_VBUILD;
    return CKR_OK;
}

/*
 * check the current state of the 'needLogin' flag in case the database has
 * been changed underneath us.
 */
static PRBool
sftk_checkNeedLogin(SFTKSlot *slot, SFTKDBHandle *keyHandle)
{
    if (sftkdb_PWCached(keyHandle) == SECSuccess) {
        return slot->needLogin;
    }
    slot->needLogin = (PRBool)!sftk_hasNullPassword(slot, keyHandle);
    return (slot->needLogin);
}

static PRBool
sftk_isBlank(const char *s, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (s[i] != ' ') {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

/* NSC_GetTokenInfo obtains information about a particular token in
 * the system. */
CK_RV
NSC_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
    SFTKSlot *slot;
    SFTKDBHandle *handle;

    CHECK_FORK();

    if (!nsc_init && !nsf_init)
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    slot = sftk_SlotFromID(slotID, PR_FALSE);
    if (slot == NULL)
        return CKR_SLOT_ID_INVALID;

    PORT_Memcpy(pInfo->manufacturerID, manufacturerID, 32);
    PORT_Memcpy(pInfo->model, "NSS 3           ", 16);
    PORT_Memcpy(pInfo->serialNumber, "0000000000000000", 16);
    PORT_Memcpy(pInfo->utcTime, "0000000000000000", 16);
    pInfo->ulMaxSessionCount = 0; /* arbitrarily large */
    pInfo->ulSessionCount = slot->sessionCount;
    pInfo->ulMaxRwSessionCount = 0; /* arbitarily large */
    pInfo->ulRwSessionCount = slot->rwSessionCount;
    pInfo->firmwareVersion.major = 0;
    pInfo->firmwareVersion.minor = 0;
    PORT_Memcpy(pInfo->label, slot->tokDescription, sizeof(pInfo->label));
    handle = sftk_getKeyDB(slot);
    pInfo->flags = CKF_RNG | CKF_DUAL_CRYPTO_OPERATIONS;
    if (handle == NULL) {
        pInfo->flags |= CKF_WRITE_PROTECTED;
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
        if (sftkdb_HasPasswordSet(handle) == SECFailure) {
            pInfo->flags |= CKF_LOGIN_REQUIRED;
        } else if (!sftk_checkNeedLogin(slot, handle)) {
            pInfo->flags |= CKF_USER_PIN_INITIALIZED;
        } else {
            pInfo->flags |= CKF_LOGIN_REQUIRED | CKF_USER_PIN_INITIALIZED;
            /*
             * if we are doing a merge style update, and we need to get the password
             * of our source database (the database we are updating from), make sure we
             * return a token name that will match the database we are prompting for.
             */
            if (sftkdb_NeedUpdateDBPassword(handle)) {
                /* if we have an update tok description, use it. otherwise
                 * use the updateID for this database */
                if (!sftk_isBlank(slot->updateTokDescription,
                                  sizeof(pInfo->label))) {
                    PORT_Memcpy(pInfo->label, slot->updateTokDescription,
                                sizeof(pInfo->label));
                } else {
                    /* build from updateID */
                    const char *updateID = sftkdb_GetUpdateID(handle);
                    if (updateID) {
                        sftk_setStringName(updateID, (char *)pInfo->label,
                                           sizeof(pInfo->label), PR_FALSE);
                    }
                }
            }
        }
        pInfo->ulMaxPinLen = SFTK_MAX_PIN;
        pInfo->ulMinPinLen = (CK_ULONG)slot->minimumPinLen;
        pInfo->ulTotalPublicMemory = 1;
        pInfo->ulFreePublicMemory = 1;
        pInfo->ulTotalPrivateMemory = 1;
        pInfo->ulFreePrivateMemory = 1;
#ifdef SHDB_FIXME
        pInfo->hardwareVersion.major = CERT_DB_FILE_VERSION;
        pInfo->hardwareVersion.minor = handle->version;
#else
        pInfo->hardwareVersion.major = 0;
        pInfo->hardwareVersion.minor = 0;
#endif
        sftk_freeDB(handle);
    }
    /*
     * CKF_LOGIN_REQUIRED CKF_USER_PIN_INITIALIZED  how CKF_TOKEN_INITIALIZED
     *                                              should be set
     *         0                   0                           1
     *         1                   0                           0
     *         0                   1                           1
     *         1                   1                           1
     */
    if (!(pInfo->flags & CKF_LOGIN_REQUIRED) ||
        (pInfo->flags & CKF_USER_PIN_INITIALIZED)) {
        pInfo->flags |= CKF_TOKEN_INITIALIZED;
    }
    return CKR_OK;
}

/* NSC_GetMechanismList obtains a list of mechanism types
 * supported by a token. */
CK_RV
NSC_GetMechanismList(CK_SLOT_ID slotID,
                     CK_MECHANISM_TYPE_PTR pMechanismList, CK_ULONG_PTR pulCount)
{
    CK_ULONG i;

    CHECK_FORK();

    switch (slotID) {
        /* default: */
        case NETSCAPE_SLOT_ID:
            *pulCount = mechanismCount;
            if (pMechanismList != NULL) {
                for (i = 0; i < mechanismCount; i++) {
                    pMechanismList[i] = mechanisms[i].type;
                }
            }
            break;
        default:
            *pulCount = 0;
            for (i = 0; i < mechanismCount; i++) {
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
CK_RV
NSC_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type,
                     CK_MECHANISM_INFO_PTR pInfo)
{
    PRBool isPrivateKey;
    CK_ULONG i;

    CHECK_FORK();

    switch (slotID) {
        case NETSCAPE_SLOT_ID:
            isPrivateKey = PR_FALSE;
            break;
        default:
            isPrivateKey = PR_TRUE;
            break;
    }
    for (i = 0; i < mechanismCount; i++) {
        if (type == mechanisms[i].type) {
            if (isPrivateKey && !mechanisms[i].privkey) {
                return CKR_MECHANISM_INVALID;
            }
            PORT_Memcpy(pInfo, &mechanisms[i].info, sizeof(CK_MECHANISM_INFO));
            return CKR_OK;
        }
    }
    return CKR_MECHANISM_INVALID;
}

CK_RV
sftk_MechAllowsOperation(CK_MECHANISM_TYPE type, CK_ATTRIBUTE_TYPE op)
{
    CK_ULONG i;
    CK_FLAGS flags;

    switch (op) {
        case CKA_ENCRYPT:
            flags = CKF_ENCRYPT;
            break;
        case CKA_DECRYPT:
            flags = CKF_DECRYPT;
            break;
        case CKA_WRAP:
            flags = CKF_WRAP;
            break;
        case CKA_UNWRAP:
            flags = CKF_UNWRAP;
            break;
        case CKA_SIGN:
            flags = CKF_SIGN;
            break;
        case CKA_SIGN_RECOVER:
            flags = CKF_SIGN_RECOVER;
            break;
        case CKA_VERIFY:
            flags = CKF_VERIFY;
            break;
        case CKA_VERIFY_RECOVER:
            flags = CKF_VERIFY_RECOVER;
            break;
        case CKA_DERIVE:
            flags = CKF_DERIVE;
            break;
        default:
            return CKR_ARGUMENTS_BAD;
    }
    for (i = 0; i < mechanismCount; i++) {
        if (type == mechanisms[i].type) {
            return (flags & mechanisms[i].info.flags) ? CKR_OK
                                                      : CKR_MECHANISM_INVALID;
        }
    }
    return CKR_MECHANISM_INVALID;
}

/* NSC_InitToken initializes a token. */
CK_RV
NSC_InitToken(CK_SLOT_ID slotID, CK_CHAR_PTR pPin,
              CK_ULONG ulPinLen, CK_CHAR_PTR pLabel)
{
    SFTKSlot *slot = sftk_SlotFromID(slotID, PR_FALSE);
    SFTKDBHandle *handle;
    SFTKDBHandle *certHandle;
    SECStatus rv;
    unsigned int i;
    SFTKObject *object;

    CHECK_FORK();

    if (slot == NULL)
        return CKR_SLOT_ID_INVALID;

    /* don't initialize the database if we aren't talking to a token
     * that uses the key database.
     */
    if (slotID == NETSCAPE_SLOT_ID) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* first, delete all our loaded key and cert objects from our
     * internal list. */
    PZ_Lock(slot->objectLock);
    for (i = 0; i < slot->sessObjHashSize; i++) {
        do {
            object = slot->sessObjHashTable[i];
            /* hand deque */
            /* this duplicates function of NSC_close session functions, but
             * because we know that we are freeing all the sessions, we can
             * do more efficient processing */
            if (object) {
                slot->sessObjHashTable[i] = object->next;

                if (object->next)
                    object->next->prev = NULL;
                object->next = object->prev = NULL;
            }
            if (object)
                sftk_FreeObject(object);
        } while (object != NULL);
    }
    slot->DB_loaded = PR_FALSE;
    PZ_Unlock(slot->objectLock);

    /* then clear out the key database */
    handle = sftk_getKeyDB(slot);
    if (handle == NULL) {
        return CKR_TOKEN_WRITE_PROTECTED;
    }

    rv = sftkdb_ResetKeyDB(handle);
    sftk_freeDB(handle);
    if (rv != SECSuccess) {
        return CKR_DEVICE_ERROR;
    }

    /* finally  mark all the user certs as non-user certs */
    certHandle = sftk_getCertDB(slot);
    if (certHandle == NULL)
        return CKR_OK;

    sftk_freeDB(certHandle);

    return CKR_OK; /*is this the right function for not implemented*/
}

/* NSC_InitPIN initializes the normal user's PIN. */
CK_RV
NSC_InitPIN(CK_SESSION_HANDLE hSession,
            CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    SFTKSession *sp = NULL;
    SFTKSlot *slot;
    SFTKDBHandle *handle = NULL;
    char newPinStr[SFTK_MAX_PIN + 1];
    SECStatus rv;
    CK_RV crv = CKR_SESSION_HANDLE_INVALID;
    PRBool tokenRemoved = PR_FALSE;

    CHECK_FORK();

    sp = sftk_SessionFromHandle(hSession);
    if (sp == NULL) {
        goto loser;
    }

    slot = sftk_SlotFromSession(sp);
    if (slot == NULL) {
        goto loser;
    }

    handle = sftk_getKeyDB(slot);
    if (handle == NULL) {
        crv = CKR_PIN_LEN_RANGE;
        goto loser;
    }

    if (sp->info.state != CKS_RW_SO_FUNCTIONS) {
        crv = CKR_USER_NOT_LOGGED_IN;
        goto loser;
    }

    sftk_FreeSession(sp);
    sp = NULL;

    /* make sure the pins aren't too long */
    if (ulPinLen > SFTK_MAX_PIN) {
        crv = CKR_PIN_LEN_RANGE;
        goto loser;
    }
    if (ulPinLen < (CK_ULONG)slot->minimumPinLen) {
        crv = CKR_PIN_LEN_RANGE;
        goto loser;
    }

    if (sftkdb_HasPasswordSet(handle) != SECFailure) {
        crv = CKR_DEVICE_ERROR;
        goto loser;
    }

    /* convert to null terminated string */
    PORT_Memcpy(newPinStr, pPin, ulPinLen);
    newPinStr[ulPinLen] = 0;

    /* build the hashed pins which we pass around */

    /* change the data base */
    rv = sftkdb_ChangePassword(handle, NULL, newPinStr, &tokenRemoved);
    if (tokenRemoved) {
        sftk_CloseAllSessions(slot, PR_FALSE);
    }
    sftk_freeDB(handle);
    handle = NULL;

    /* Now update our local copy of the pin */
    if (rv == SECSuccess) {
        if (ulPinLen == 0)
            slot->needLogin = PR_FALSE;
        /* database has been initialized, now force min password in FIPS
         * mode. NOTE: if we are in level1, we may not have a password, but
         * forcing it now will prevent an insufficient password from being set.
         */
        if ((sftk_GetModuleIndex(slot->slotID) == NSC_FIPS_MODULE) &&
            (slot->minimumPinLen < FIPS_MIN_PIN)) {
            slot->minimumPinLen = FIPS_MIN_PIN;
        }
        return CKR_OK;
    }
    crv = CKR_PIN_INCORRECT;

loser:
    if (sp) {
        sftk_FreeSession(sp);
    }
    if (handle) {
        sftk_freeDB(handle);
    }
    return crv;
}

/* NSC_SetPIN modifies the PIN of user that is currently logged in. */
/* NOTE: This is only valid for the PRIVATE_KEY_SLOT */
CK_RV
NSC_SetPIN(CK_SESSION_HANDLE hSession, CK_CHAR_PTR pOldPin,
           CK_ULONG ulOldLen, CK_CHAR_PTR pNewPin, CK_ULONG ulNewLen)
{
    SFTKSession *sp = NULL;
    SFTKSlot *slot;
    SFTKDBHandle *handle = NULL;
    char newPinStr[SFTK_MAX_PIN + 1], oldPinStr[SFTK_MAX_PIN + 1];
    SECStatus rv;
    CK_RV crv = CKR_SESSION_HANDLE_INVALID;
    PRBool tokenRemoved = PR_FALSE;

    CHECK_FORK();

    sp = sftk_SessionFromHandle(hSession);
    if (sp == NULL) {
        goto loser;
    }

    slot = sftk_SlotFromSession(sp);
    if (!slot) {
        goto loser;
    }

    handle = sftk_getKeyDB(slot);
    if (handle == NULL) {
        sftk_FreeSession(sp);
        return CKR_PIN_LEN_RANGE; /* XXX FIXME wrong return value */
    }

    if (slot->needLogin && sp->info.state != CKS_RW_USER_FUNCTIONS) {
        crv = CKR_USER_NOT_LOGGED_IN;
        goto loser;
    }

    sftk_FreeSession(sp);
    sp = NULL;

    /* make sure the pins aren't too long */
    if ((ulNewLen > SFTK_MAX_PIN) || (ulOldLen > SFTK_MAX_PIN)) {
        crv = CKR_PIN_LEN_RANGE;
        goto loser;
    }
    if (ulNewLen < (CK_ULONG)slot->minimumPinLen) {
        crv = CKR_PIN_LEN_RANGE;
        goto loser;
    }

    /* convert to null terminated string */
    PORT_Memcpy(newPinStr, pNewPin, ulNewLen);
    newPinStr[ulNewLen] = 0;
    PORT_Memcpy(oldPinStr, pOldPin, ulOldLen);
    oldPinStr[ulOldLen] = 0;

    /* change the data base password */
    PR_Lock(slot->pwCheckLock);
    rv = sftkdb_ChangePassword(handle, oldPinStr, newPinStr, &tokenRemoved);
    if (tokenRemoved) {
        sftk_CloseAllSessions(slot, PR_FALSE);
    }
    if ((rv != SECSuccess) && (slot->slotID == FIPS_SLOT_ID)) {
        PR_Sleep(loginWaitTime);
    }
    PR_Unlock(slot->pwCheckLock);

    /* Now update our local copy of the pin */
    if (rv == SECSuccess) {
        slot->needLogin = (PRBool)(ulNewLen != 0);
        /* Reset login flags. */
        if (ulNewLen == 0) {
            PRBool tokenRemoved = PR_FALSE;
            PZ_Lock(slot->slotLock);
            slot->isLoggedIn = PR_FALSE;
            slot->ssoLoggedIn = PR_FALSE;
            PZ_Unlock(slot->slotLock);

            rv = sftkdb_CheckPassword(handle, "", &tokenRemoved);
            if (tokenRemoved) {
                sftk_CloseAllSessions(slot, PR_FALSE);
            }
        }
        sftk_update_all_states(slot);
        sftk_freeDB(handle);
        return CKR_OK;
    }
    crv = CKR_PIN_INCORRECT;
loser:
    if (sp) {
        sftk_FreeSession(sp);
    }
    if (handle) {
        sftk_freeDB(handle);
    }
    return crv;
}

/* NSC_OpenSession opens a session between an application and a token. */
CK_RV
NSC_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
                CK_VOID_PTR pApplication, CK_NOTIFY Notify, CK_SESSION_HANDLE_PTR phSession)
{
    SFTKSlot *slot;
    CK_SESSION_HANDLE sessionID;
    SFTKSession *session;
    SFTKSession *sameID;

    CHECK_FORK();

    slot = sftk_SlotFromID(slotID, PR_FALSE);
    if (slot == NULL)
        return CKR_SLOT_ID_INVALID;

    /* new session (we only have serial sessions) */
    session = sftk_NewSession(slotID, Notify, pApplication,
                              flags | CKF_SERIAL_SESSION);
    if (session == NULL)
        return CKR_HOST_MEMORY;

    if (slot->readOnly && (flags & CKF_RW_SESSION)) {
        /* NETSCAPE_SLOT_ID is Read ONLY */
        session->info.flags &= ~CKF_RW_SESSION;
    }
    PZ_Lock(slot->slotLock);
    ++slot->sessionCount;
    PZ_Unlock(slot->slotLock);
    if (session->info.flags & CKF_RW_SESSION) {
        (void)PR_ATOMIC_INCREMENT(&slot->rwSessionCount);
    }

    do {
        PZLock *lock;
        do {
            sessionID = (PR_ATOMIC_INCREMENT(&slot->sessionIDCount) & 0xffffff) | (slot->index << 24);
        } while (sessionID == CK_INVALID_HANDLE);
        lock = SFTK_SESSION_LOCK(slot, sessionID);
        PZ_Lock(lock);
        sftkqueue_find(sameID, sessionID, slot->head, slot->sessHashSize);
        if (sameID == NULL) {
            session->handle = sessionID;
            sftk_update_state(slot, session);
            sftkqueue_add(session, sessionID, slot->head, slot->sessHashSize);
        } else {
            slot->sessionIDConflict++; /* for debugging */
        }
        PZ_Unlock(lock);
    } while (sameID != NULL);

    *phSession = sessionID;
    return CKR_OK;
}

/* NSC_CloseSession closes a session between an application and a token. */
CK_RV
NSC_CloseSession(CK_SESSION_HANDLE hSession)
{
    SFTKSlot *slot;
    SFTKSession *session;
    PRBool sessionFound;
    PZLock *lock;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    slot = sftk_SlotFromSession(session);
    sessionFound = PR_FALSE;

    /* lock */
    lock = SFTK_SESSION_LOCK(slot, hSession);
    PZ_Lock(lock);
    if (sftkqueue_is_queued(session, hSession, slot->head, slot->sessHashSize)) {
        sessionFound = PR_TRUE;
        sftkqueue_delete(session, hSession, slot->head, slot->sessHashSize);
        session->refCount--; /* can't go to zero while we hold the reference */
        PORT_Assert(session->refCount > 0);
    }
    PZ_Unlock(lock);

    if (sessionFound) {
        SFTKDBHandle *handle;
        handle = sftk_getKeyDB(slot);
        PZ_Lock(slot->slotLock);
        if (--slot->sessionCount == 0) {
            slot->isLoggedIn = PR_FALSE;
            if (slot->needLogin && handle) {
                sftkdb_ClearPassword(handle);
            }
        }
        PZ_Unlock(slot->slotLock);
        if (handle) {
            sftk_freeDB(handle);
        }
        if (session->info.flags & CKF_RW_SESSION) {
            (void)PR_ATOMIC_DECREMENT(&slot->rwSessionCount);
        }
    }

    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_CloseAllSessions closes all sessions with a token. */
CK_RV
NSC_CloseAllSessions(CK_SLOT_ID slotID)
{
    SFTKSlot *slot;

#ifndef NO_FORK_CHECK
    /* skip fork check if we are being called from C_Initialize or C_Finalize */
    if (!parentForkedAfterC_Initialize) {
        CHECK_FORK();
    }
#endif

    slot = sftk_SlotFromID(slotID, PR_FALSE);
    if (slot == NULL)
        return CKR_SLOT_ID_INVALID;

    return sftk_CloseAllSessions(slot, PR_TRUE);
}

/* NSC_GetSessionInfo obtains information about the session. */
CK_RV
NSC_GetSessionInfo(CK_SESSION_HANDLE hSession,
                   CK_SESSION_INFO_PTR pInfo)
{
    SFTKSession *session;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;

    PORT_Memcpy(pInfo, &session->info, sizeof(CK_SESSION_INFO));
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_Login logs a user into a token. */
CK_RV
NSC_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType,
          CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    SFTKSlot *slot;
    SFTKSession *session;
    SFTKDBHandle *handle;
    CK_FLAGS sessionFlags;
    SECStatus rv;
    CK_RV crv;
    char pinStr[SFTK_MAX_PIN + 1];
    PRBool tokenRemoved = PR_FALSE;

    CHECK_FORK();

    /* get the slot */
    slot = sftk_SlotFromSessionHandle(hSession);
    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    /* make sure the session is valid */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    sessionFlags = session->info.flags;
    sftk_FreeSession(session);
    session = NULL;

    /* can't log into the Netscape Slot */
    if (slot->slotID == NETSCAPE_SLOT_ID) {
        return CKR_USER_TYPE_INVALID;
    }

    if (slot->isLoggedIn)
        return CKR_USER_ALREADY_LOGGED_IN;
    if (!slot->needLogin) {
        return ulPinLen ? CKR_PIN_INCORRECT : CKR_OK;
    }
    slot->ssoLoggedIn = PR_FALSE;

    if (ulPinLen > SFTK_MAX_PIN)
        return CKR_PIN_LEN_RANGE;

    /* convert to null terminated string */
    PORT_Memcpy(pinStr, pPin, ulPinLen);
    pinStr[ulPinLen] = 0;

    handle = sftk_getKeyDB(slot);
    if (handle == NULL) {
        return CKR_USER_TYPE_INVALID;
    }

    /*
     * Deal with bootstrap. We allow the SSO to login in with a NULL
     * password if and only if we haven't initialized the KEY DB yet.
     * We only allow this on a RW session.
     */
    rv = sftkdb_HasPasswordSet(handle);
    if (rv == SECFailure) {
        /* allow SSO's to log in only if there is not password on the
         * key database */
        if (((userType == CKU_SO) && (sessionFlags & CKF_RW_SESSION))
            /* fips always needs to authenticate, even if there isn't a db */
            || (slot->slotID == FIPS_SLOT_ID)) {
            /* should this be a fixed password? */
            if (ulPinLen == 0) {
                sftkdb_ClearPassword(handle);
                PZ_Lock(slot->slotLock);
                slot->isLoggedIn = PR_TRUE;
                slot->ssoLoggedIn = (PRBool)(userType == CKU_SO);
                PZ_Unlock(slot->slotLock);
                sftk_update_all_states(slot);
                crv = CKR_OK;
                goto done;
            }
            crv = CKR_PIN_INCORRECT;
            goto done;
        }
        crv = CKR_USER_TYPE_INVALID;
        goto done;
    }

    /* don't allow the SSO to log in if the user is already initialized */
    if (userType != CKU_USER) {
        crv = CKR_USER_TYPE_INVALID;
        goto done;
    }

    /* build the hashed pins which we pass around */
    PR_Lock(slot->pwCheckLock);
    rv = sftkdb_CheckPassword(handle, pinStr, &tokenRemoved);
    if (tokenRemoved) {
        sftk_CloseAllSessions(slot, PR_FALSE);
    }
    if ((rv != SECSuccess) && (slot->slotID == FIPS_SLOT_ID)) {
        PR_Sleep(loginWaitTime);
    }
    PR_Unlock(slot->pwCheckLock);
    if (rv == SECSuccess) {
        PZ_Lock(slot->slotLock);
        /* make sure the login state matches the underlying
         * database state */
        slot->isLoggedIn = sftkdb_PWCached(handle) == SECSuccess ? PR_TRUE : PR_FALSE;
        PZ_Unlock(slot->slotLock);

        sftk_freeDB(handle);
        handle = NULL;

        /* update all sessions */
        sftk_update_all_states(slot);
        return CKR_OK;
    }

    crv = CKR_PIN_INCORRECT;
done:
    if (handle) {
        sftk_freeDB(handle);
    }
    return crv;
}

/* NSC_Logout logs a user out from a token. */
CK_RV
NSC_Logout(CK_SESSION_HANDLE hSession)
{
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SFTKSession *session;
    SFTKDBHandle *handle;

    CHECK_FORK();

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    sftk_FreeSession(session);
    session = NULL;

    if (!slot->isLoggedIn)
        return CKR_USER_NOT_LOGGED_IN;

    handle = sftk_getKeyDB(slot);
    PZ_Lock(slot->slotLock);
    slot->isLoggedIn = PR_FALSE;
    slot->ssoLoggedIn = PR_FALSE;
    if (slot->needLogin && handle) {
        sftkdb_ClearPassword(handle);
    }
    PZ_Unlock(slot->slotLock);
    if (handle) {
        sftk_freeDB(handle);
    }

    sftk_update_all_states(slot);
    return CKR_OK;
}

/*
 * Create or remove a new slot on the fly.
 * When creating a slot, "slot" is the slot that the request came from. The
 * resulting slot will live in the same module as "slot".
 * When removing a slot, "slot" is the slot to be removed.
 * "object" is the creation object that specifies the module spec for the slot
 * to add or remove.
 */
static CK_RV
sftk_CreateNewSlot(SFTKSlot *slot, CK_OBJECT_CLASS class,
                   SFTKObject *object)
{
    PRBool isValidUserSlot = PR_FALSE;
    PRBool isValidFIPSUserSlot = PR_FALSE;
    PRBool isValidSlot = PR_FALSE;
    PRBool isFIPS = PR_FALSE;
    unsigned long moduleIndex = NSC_NON_FIPS_MODULE;
    SFTKAttribute *attribute;
    sftk_parameters paramStrings;
    char *paramString;
    CK_SLOT_ID slotID = 0;
    SFTKSlot *newSlot = NULL;
    CK_RV crv = CKR_OK;

    if (class != CKO_NETSCAPE_DELSLOT && class != CKO_NETSCAPE_NEWSLOT) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    if (class == CKO_NETSCAPE_NEWSLOT && slot->slotID == FIPS_SLOT_ID) {
        isFIPS = PR_TRUE;
    }
    attribute = sftk_FindAttribute(object, CKA_NETSCAPE_MODULE_SPEC);
    if (attribute == NULL) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    paramString = (char *)attribute->attrib.pValue;
    crv = sftk_parseParameters(paramString, &paramStrings, isFIPS);
    if (crv != CKR_OK) {
        goto loser;
    }

    /* enforce only one at a time */
    if (paramStrings.token_count != 1) {
        crv = CKR_ATTRIBUTE_VALUE_INVALID;
        goto loser;
    }

    slotID = paramStrings.tokens[0].slotID;

    /* stay within the valid ID space */
    isValidUserSlot = (slotID >= SFTK_MIN_USER_SLOT_ID &&
                       slotID <= SFTK_MAX_USER_SLOT_ID);
    isValidFIPSUserSlot = (slotID >= SFTK_MIN_FIPS_USER_SLOT_ID &&
                           slotID <= SFTK_MAX_FIPS_USER_SLOT_ID);

    if (class == CKO_NETSCAPE_DELSLOT) {
        if (slot->slotID == slotID) {
            isValidSlot = isValidUserSlot || isValidFIPSUserSlot;
        }
    } else {
        /* only the crypto or FIPS slots can create new slot objects */
        if (slot->slotID == NETSCAPE_SLOT_ID) {
            isValidSlot = isValidUserSlot;
            moduleIndex = NSC_NON_FIPS_MODULE;
        } else if (slot->slotID == FIPS_SLOT_ID) {
            isValidSlot = isValidFIPSUserSlot;
            moduleIndex = NSC_FIPS_MODULE;
        }
    }

    if (!isValidSlot) {
        crv = CKR_ATTRIBUTE_VALUE_INVALID;
        goto loser;
    }

    /* unload any existing slot at this id */
    newSlot = sftk_SlotFromID(slotID, PR_TRUE);
    if (newSlot && newSlot->present) {
        crv = SFTK_ShutdownSlot(newSlot);
        if (crv != CKR_OK) {
            goto loser;
        }
    }

    /* if we were just planning on deleting the slot, then do so now */
    if (class == CKO_NETSCAPE_DELSLOT) {
        /* sort of a unconventional use of this error code, be we are
         * overusing CKR_ATTRIBUTE_VALUE_INVALID, and it does apply */
        crv = newSlot ? CKR_OK : CKR_SLOT_ID_INVALID;
        goto loser; /* really exit */
    }

    if (newSlot) {
        crv = SFTK_SlotReInit(newSlot, paramStrings.configdir,
                              paramStrings.updatedir, paramStrings.updateID,
                              &paramStrings.tokens[0], moduleIndex);
    } else {
        crv = SFTK_SlotInit(paramStrings.configdir,
                            paramStrings.updatedir, paramStrings.updateID,
                            &paramStrings.tokens[0], moduleIndex);
    }

loser:
    sftk_freeParams(&paramStrings);
    sftk_FreeAttribute(attribute);

    return crv;
}

/* NSC_CreateObject creates a new object. */
CK_RV
NSC_CreateObject(CK_SESSION_HANDLE hSession,
                 CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
                 CK_OBJECT_HANDLE_PTR phObject)
{
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SFTKSession *session;
    SFTKObject *object;
    /* make sure class isn't randomly CKO_NETSCAPE_NEWSLOT or
     * CKO_NETSCPE_DELSLOT. */
    CK_OBJECT_CLASS class = CKO_VENDOR_DEFINED;
    CK_RV crv;
    int i;

    CHECK_FORK();

    *phObject = CK_INVALID_HANDLE;

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * now lets create an object to hang the attributes off of
     */
    object = sftk_NewObject(slot); /* fill in the handle later */
    if (object == NULL) {
        return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i = 0; i < (int)ulCount; i++) {
        crv = sftk_AddAttributeType(object, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK) {
            sftk_FreeObject(object);
            return crv;
        }
        if ((pTemplate[i].type == CKA_CLASS) && pTemplate[i].pValue) {
            class = *(CK_OBJECT_CLASS *)pTemplate[i].pValue;
        }
    }

    /* get the session */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        sftk_FreeObject(object);
        return CKR_SESSION_HANDLE_INVALID;
    }

    /*
     * handle pseudo objects (CKO_NEWSLOT)
     */
    if ((class == CKO_NETSCAPE_NEWSLOT) || (class == CKO_NETSCAPE_DELSLOT)) {
        crv = sftk_CreateNewSlot(slot, class, object);
        goto done;
    }

    /*
     * handle the base object stuff
     */
    crv = sftk_handleObject(object, session);
    *phObject = object->handle;
done:
    sftk_FreeSession(session);
    sftk_FreeObject(object);

    return crv;
}

/* NSC_CopyObject copies an object, creating a new object for the copy. */
CK_RV
NSC_CopyObject(CK_SESSION_HANDLE hSession,
               CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
               CK_OBJECT_HANDLE_PTR phNewObject)
{
    SFTKObject *destObject, *srcObject;
    SFTKSession *session;
    CK_RV crv = CKR_OK;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    int i;

    CHECK_FORK();

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /* Get srcObject so we can find the class */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    srcObject = sftk_ObjectFromHandle(hObject, session);
    if (srcObject == NULL) {
        sftk_FreeSession(session);
        return CKR_OBJECT_HANDLE_INVALID;
    }
    /*
     * create an object to hang the attributes off of
     */
    destObject = sftk_NewObject(slot); /* fill in the handle later */
    if (destObject == NULL) {
        sftk_FreeSession(session);
        sftk_FreeObject(srcObject);
        return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i = 0; i < (int)ulCount; i++) {
        if (sftk_modifyType(pTemplate[i].type, srcObject->objclass) == SFTK_NEVER) {
            crv = CKR_ATTRIBUTE_READ_ONLY;
            break;
        }
        crv = sftk_AddAttributeType(destObject, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK) {
            break;
        }
    }
    if (crv != CKR_OK) {
        sftk_FreeSession(session);
        sftk_FreeObject(srcObject);
        sftk_FreeObject(destObject);
        return crv;
    }

    /* sensitive can only be changed to CK_TRUE */
    if (sftk_hasAttribute(destObject, CKA_SENSITIVE)) {
        if (!sftk_isTrue(destObject, CKA_SENSITIVE)) {
            sftk_FreeSession(session);
            sftk_FreeObject(srcObject);
            sftk_FreeObject(destObject);
            return CKR_ATTRIBUTE_READ_ONLY;
        }
    }

    /*
     * now copy the old attributes from the new attributes
     */
    /* don't create a token object if we aren't in a rw session */
    /* we need to hold the lock to copy a consistant version of
     * the object. */
    crv = sftk_CopyObject(destObject, srcObject);

    destObject->objclass = srcObject->objclass;
    sftk_FreeObject(srcObject);
    if (crv != CKR_OK) {
        sftk_FreeObject(destObject);
        sftk_FreeSession(session);
        return crv;
    }

    crv = sftk_handleObject(destObject, session);
    *phNewObject = destObject->handle;
    sftk_FreeSession(session);
    sftk_FreeObject(destObject);

    return crv;
}

/* NSC_GetObjectSize gets the size of an object in bytes. */
CK_RV
NSC_GetObjectSize(CK_SESSION_HANDLE hSession,
                  CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize)
{
    CHECK_FORK();

    *pulSize = 0;
    return CKR_OK;
}

/* NSC_GetAttributeValue obtains the value of one or more object attributes. */
CK_RV
NSC_GetAttributeValue(CK_SESSION_HANDLE hSession,
                      CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SFTKSession *session;
    SFTKObject *object;
    SFTKAttribute *attribute;
    PRBool sensitive;
    CK_RV crv;
    int i;

    CHECK_FORK();

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * make sure we're allowed
     */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    /* short circuit everything for token objects */
    if (sftk_isToken(hObject)) {
        SFTKSlot *slot = sftk_SlotFromSession(session);
        SFTKDBHandle *dbHandle = sftk_getDBForTokenObject(slot, hObject);
        SFTKDBHandle *keydb = NULL;

        if (dbHandle == NULL) {
            sftk_FreeSession(session);
            return CKR_OBJECT_HANDLE_INVALID;
        }

        crv = sftkdb_GetAttributeValue(dbHandle, hObject, pTemplate, ulCount);

        /* make sure we don't export any sensitive information */
        keydb = sftk_getKeyDB(slot);
        if (dbHandle == keydb) {
            for (i = 0; i < (int)ulCount; i++) {
                if (sftk_isSensitive(pTemplate[i].type, CKO_PRIVATE_KEY)) {
                    crv = CKR_ATTRIBUTE_SENSITIVE;
                    if (pTemplate[i].pValue && (pTemplate[i].ulValueLen != -1)) {
                        PORT_Memset(pTemplate[i].pValue, 0,
                                    pTemplate[i].ulValueLen);
                    }
                    pTemplate[i].ulValueLen = -1;
                }
            }
        }

        sftk_FreeSession(session);
        sftk_freeDB(dbHandle);
        if (keydb) {
            sftk_freeDB(keydb);
        }
        return crv;
    }

    /* handle the session object */
    object = sftk_ObjectFromHandle(hObject, session);
    sftk_FreeSession(session);
    if (object == NULL) {
        return CKR_OBJECT_HANDLE_INVALID;
    }

    /* don't read a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
        (sftk_isTrue(object, CKA_PRIVATE))) {
        sftk_FreeObject(object);
        return CKR_USER_NOT_LOGGED_IN;
    }

    crv = CKR_OK;
    sensitive = sftk_isTrue(object, CKA_SENSITIVE);
    for (i = 0; i < (int)ulCount; i++) {
        /* Make sure that this attribute is retrievable */
        if (sensitive && sftk_isSensitive(pTemplate[i].type, object->objclass)) {
            crv = CKR_ATTRIBUTE_SENSITIVE;
            pTemplate[i].ulValueLen = -1;
            continue;
        }
        attribute = sftk_FindAttribute(object, pTemplate[i].type);
        if (attribute == NULL) {
            crv = CKR_ATTRIBUTE_TYPE_INVALID;
            pTemplate[i].ulValueLen = -1;
            continue;
        }
        if (pTemplate[i].pValue != NULL) {
            PORT_Memcpy(pTemplate[i].pValue, attribute->attrib.pValue,
                        attribute->attrib.ulValueLen);
        }
        pTemplate[i].ulValueLen = attribute->attrib.ulValueLen;
        sftk_FreeAttribute(attribute);
    }

    sftk_FreeObject(object);
    return crv;
}

/* NSC_SetAttributeValue modifies the value of one or more object attributes */
CK_RV
NSC_SetAttributeValue(CK_SESSION_HANDLE hSession,
                      CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    SFTKSession *session;
    SFTKAttribute *attribute;
    SFTKObject *object;
    PRBool isToken;
    CK_RV crv = CKR_OK;
    CK_BBOOL legal;
    int i;

    CHECK_FORK();

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    /*
     * make sure we're allowed
     */
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    object = sftk_ObjectFromHandle(hObject, session);
    if (object == NULL) {
        sftk_FreeSession(session);
        return CKR_OBJECT_HANDLE_INVALID;
    }

    /* don't modify a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
        (sftk_isTrue(object, CKA_PRIVATE))) {
        sftk_FreeSession(session);
        sftk_FreeObject(object);
        return CKR_USER_NOT_LOGGED_IN;
    }

    /* don't modify a token object if we aren't in a rw session */
    isToken = sftk_isTrue(object, CKA_TOKEN);
    if (((session->info.flags & CKF_RW_SESSION) == 0) && isToken) {
        sftk_FreeSession(session);
        sftk_FreeObject(object);
        return CKR_SESSION_READ_ONLY;
    }
    sftk_FreeSession(session);

    /* only change modifiable objects */
    if (!sftk_isTrue(object, CKA_MODIFIABLE)) {
        sftk_FreeObject(object);
        return CKR_ATTRIBUTE_READ_ONLY;
    }

    for (i = 0; i < (int)ulCount; i++) {
        /* Make sure that this attribute is changeable */
        switch (sftk_modifyType(pTemplate[i].type, object->objclass)) {
            case SFTK_NEVER:
            case SFTK_ONCOPY:
            default:
                crv = CKR_ATTRIBUTE_READ_ONLY;
                break;

            case SFTK_SENSITIVE:
                legal = (pTemplate[i].type == CKA_EXTRACTABLE) ? CK_FALSE : CK_TRUE;
                if ((*(CK_BBOOL *)pTemplate[i].pValue) != legal) {
                    crv = CKR_ATTRIBUTE_READ_ONLY;
                }
                break;
            case SFTK_ALWAYS:
                break;
        }
        if (crv != CKR_OK)
            break;

        /* find the old attribute */
        attribute = sftk_FindAttribute(object, pTemplate[i].type);
        if (attribute == NULL) {
            crv = CKR_ATTRIBUTE_TYPE_INVALID;
            break;
        }
        sftk_FreeAttribute(attribute);
        crv = sftk_forceAttribute(object, sftk_attr_expand(&pTemplate[i]));
        if (crv != CKR_OK)
            break;
    }

    sftk_FreeObject(object);
    return crv;
}

static CK_RV
sftk_expandSearchList(SFTKSearchResults *search, int count)
{
    search->array_size += count;
    search->handles = (CK_OBJECT_HANDLE *)PORT_Realloc(search->handles,
                                                       sizeof(CK_OBJECT_HANDLE) * search->array_size);
    return search->handles ? CKR_OK : CKR_HOST_MEMORY;
}

static CK_RV
sftk_searchDatabase(SFTKDBHandle *handle, SFTKSearchResults *search,
                    const CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount)
{
    CK_RV crv;
    int objectListSize = search->array_size - search->size;
    CK_OBJECT_HANDLE *array = &search->handles[search->size];
    SDBFind *find;
    CK_ULONG count;

    crv = sftkdb_FindObjectsInit(handle, pTemplate, ulCount, &find);
    if (crv != CKR_OK)
        return crv;
    do {
        crv = sftkdb_FindObjects(handle, find, array, objectListSize, &count);
        if ((crv != CKR_OK) || (count == 0))
            break;
        search->size += count;
        objectListSize -= count;
        if (objectListSize > 0)
            break;
        crv = sftk_expandSearchList(search, NSC_SEARCH_BLOCK_SIZE);
        objectListSize = NSC_SEARCH_BLOCK_SIZE;
        array = &search->handles[search->size];
    } while (crv == CKR_OK);
    sftkdb_FindObjectsFinal(handle, find);

    return crv;
}

/* softoken used to search the SMimeEntries automatically instead of
 * doing this in pk11wrap. This code should really be up in
 * pk11wrap so that it will work with other tokens other than softoken.
 */
CK_RV
sftk_emailhack(SFTKSlot *slot, SFTKDBHandle *handle,
               SFTKSearchResults *search, CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount)
{
    PRBool isCert = PR_FALSE;
    int emailIndex = -1;
    unsigned int i;
    SFTKSearchResults smime_search;
    CK_ATTRIBUTE smime_template[2];
    CK_OBJECT_CLASS smime_class = CKO_NETSCAPE_SMIME;
    SFTKAttribute *attribute = NULL;
    SFTKObject *object = NULL;
    CK_RV crv = CKR_OK;

    smime_search.handles = NULL; /* paranoia, some one is bound to add a goto
                                  * loser before this gets initialized */

    /* see if we are looking for email certs */
    for (i = 0; i < ulCount; i++) {
        if (pTemplate[i].type == CKA_CLASS) {
            if ((pTemplate[i].ulValueLen != sizeof(CK_OBJECT_CLASS) ||
                 (*(CK_OBJECT_CLASS *)pTemplate[i].pValue) != CKO_CERTIFICATE)) {
                /* not a cert, skip out */
                break;
            }
            isCert = PR_TRUE;
        } else if (pTemplate[i].type == CKA_NETSCAPE_EMAIL) {
            emailIndex = i;
        }
        if (isCert && (emailIndex != -1))
            break;
    }

    if (!isCert || (emailIndex == -1)) {
        return CKR_OK;
    }

    /* we are doing a cert and email search, find the SMimeEntry */
    smime_template[0].type = CKA_CLASS;
    smime_template[0].pValue = &smime_class;
    smime_template[0].ulValueLen = sizeof(smime_class);
    smime_template[1] = pTemplate[emailIndex];

    smime_search.handles = (CK_OBJECT_HANDLE *)
        PORT_Alloc(sizeof(CK_OBJECT_HANDLE) * NSC_SEARCH_BLOCK_SIZE);
    if (smime_search.handles == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }
    smime_search.index = 0;
    smime_search.size = 0;
    smime_search.array_size = NSC_SEARCH_BLOCK_SIZE;

    crv = sftk_searchDatabase(handle, &smime_search, smime_template, 2);
    if (crv != CKR_OK || smime_search.size == 0) {
        goto loser;
    }

    /* get the SMime subject */
    object = sftk_NewTokenObject(slot, NULL, smime_search.handles[0]);
    if (object == NULL) {
        crv = CKR_HOST_MEMORY; /* is there any other reason for this failure? */
        goto loser;
    }
    attribute = sftk_FindAttribute(object, CKA_SUBJECT);
    if (attribute == NULL) {
        crv = CKR_ATTRIBUTE_TYPE_INVALID;
        goto loser;
    }

    /* now find the certs with that subject */
    pTemplate[emailIndex] = attribute->attrib;
    /* now add the appropriate certs to the search list */
    crv = sftk_searchDatabase(handle, search, pTemplate, ulCount);
    pTemplate[emailIndex] = smime_template[1]; /* restore the user's template*/

loser:
    if (attribute) {
        sftk_FreeAttribute(attribute);
    }
    if (object) {
        sftk_FreeObject(object);
    }
    if (smime_search.handles) {
        PORT_Free(smime_search.handles);
    }

    return crv;
}

static void
sftk_pruneSearch(CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount,
                 PRBool *searchCertDB, PRBool *searchKeyDB)
{
    CK_ULONG i;

    *searchCertDB = PR_TRUE;
    *searchKeyDB = PR_TRUE;
    for (i = 0; i < ulCount; i++) {
        if (pTemplate[i].type == CKA_CLASS && pTemplate[i].pValue != NULL) {
            CK_OBJECT_CLASS class = *((CK_OBJECT_CLASS *)pTemplate[i].pValue);
            if (class == CKO_PRIVATE_KEY || class == CKO_SECRET_KEY) {
                *searchCertDB = PR_FALSE;
            } else {
                *searchKeyDB = PR_FALSE;
            }
            break;
        }
    }
}

static CK_RV
sftk_searchTokenList(SFTKSlot *slot, SFTKSearchResults *search,
                     CK_ATTRIBUTE *pTemplate, CK_ULONG ulCount,
                     PRBool *tokenOnly, PRBool isLoggedIn)
{
    CK_RV crv = CKR_OK;
    CK_RV crv2;
    PRBool searchCertDB;
    PRBool searchKeyDB;

    sftk_pruneSearch(pTemplate, ulCount, &searchCertDB, &searchKeyDB);

    if (searchCertDB) {
        SFTKDBHandle *certHandle = sftk_getCertDB(slot);
        crv = sftk_searchDatabase(certHandle, search, pTemplate, ulCount);
        crv2 = sftk_emailhack(slot, certHandle, search, pTemplate, ulCount);
        if (crv == CKR_OK)
            crv = crv2;
        sftk_freeDB(certHandle);
    }

    if (crv == CKR_OK && isLoggedIn && searchKeyDB) {
        SFTKDBHandle *keyHandle = sftk_getKeyDB(slot);
        crv = sftk_searchDatabase(keyHandle, search, pTemplate, ulCount);
        sftk_freeDB(keyHandle);
    }
    return crv;
}

/* NSC_FindObjectsInit initializes a search for token and session objects
 * that match a template. */
CK_RV
NSC_FindObjectsInit(CK_SESSION_HANDLE hSession,
                    CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
    SFTKSearchResults *search = NULL, *freeSearch = NULL;
    SFTKSession *session = NULL;
    SFTKSlot *slot = sftk_SlotFromSessionHandle(hSession);
    PRBool tokenOnly = PR_FALSE;
    CK_RV crv = CKR_OK;
    PRBool isLoggedIn;

    CHECK_FORK();

    if (slot == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL) {
        crv = CKR_SESSION_HANDLE_INVALID;
        goto loser;
    }

    search = (SFTKSearchResults *)PORT_Alloc(sizeof(SFTKSearchResults));
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

    crv = sftk_searchTokenList(slot, search, pTemplate, ulCount, &tokenOnly,
                               isLoggedIn);
    if (crv != CKR_OK) {
        goto loser;
    }

    /* build list of found objects in the session */
    if (!tokenOnly) {
        crv = sftk_searchObjectList(search, slot->sessObjHashTable,
                                    slot->sessObjHashSize, slot->objectLock,
                                    pTemplate, ulCount, isLoggedIn);
    }
    if (crv != CKR_OK) {
        goto loser;
    }

    if ((freeSearch = session->search) != NULL) {
        session->search = NULL;
        sftk_FreeSearch(freeSearch);
    }
    session->search = search;
    sftk_FreeSession(session);
    return CKR_OK;

loser:
    if (search) {
        sftk_FreeSearch(search);
    }
    if (session) {
        sftk_FreeSession(session);
    }
    return crv;
}

/* NSC_FindObjects continues a search for token and session objects
 * that match a template, obtaining additional object handles. */
CK_RV
NSC_FindObjects(CK_SESSION_HANDLE hSession,
                CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxObjectCount,
                CK_ULONG_PTR pulObjectCount)
{
    SFTKSession *session;
    SFTKSearchResults *search;
    int transfer;
    int left;

    CHECK_FORK();

    *pulObjectCount = 0;
    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    if (session->search == NULL) {
        sftk_FreeSession(session);
        return CKR_OK;
    }
    search = session->search;
    left = session->search->size - session->search->index;
    transfer = ((int)ulMaxObjectCount > left) ? left : ulMaxObjectCount;
    if (transfer > 0) {
        PORT_Memcpy(phObject, &search->handles[search->index],
                    transfer * sizeof(CK_OBJECT_HANDLE));
    } else {
        *phObject = CK_INVALID_HANDLE;
    }

    search->index += transfer;
    if (search->index == search->size) {
        session->search = NULL;
        sftk_FreeSearch(search);
    }
    *pulObjectCount = transfer;
    sftk_FreeSession(session);
    return CKR_OK;
}

/* NSC_FindObjectsFinal finishes a search for token and session objects. */
CK_RV
NSC_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
    SFTKSession *session;
    SFTKSearchResults *search;

    CHECK_FORK();

    session = sftk_SessionFromHandle(hSession);
    if (session == NULL)
        return CKR_SESSION_HANDLE_INVALID;
    search = session->search;
    session->search = NULL;
    sftk_FreeSession(session);
    if (search != NULL) {
        sftk_FreeSearch(search);
    }
    return CKR_OK;
}

CK_RV
NSC_WaitForSlotEvent(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot,
                     CK_VOID_PTR pReserved)
{
    CHECK_FORK();

    return CKR_FUNCTION_NOT_SUPPORTED;
}
