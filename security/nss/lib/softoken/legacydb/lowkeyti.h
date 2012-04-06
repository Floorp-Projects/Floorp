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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
#ifndef _LOWKEYTI_H_
#define _LOWKEYTI_H_ 1

#include "blapit.h"
#include "prtypes.h"
#include "plarena.h"
#include "secitem.h"
#include "secasn1t.h"
#include "secoidt.h"


/*
 * a key in/for the data base
 */
struct NSSLOWKEYDBKeyStr {
    PLArenaPool *arena;
    int version;
    char *nickname;
    SECItem salt;
    SECItem derPK;
};
typedef struct NSSLOWKEYDBKeyStr NSSLOWKEYDBKey;

typedef struct NSSLOWKEYDBHandleStr NSSLOWKEYDBHandle;

#ifdef NSS_USE_KEY4_DB
#define NSSLOWKEY_DB_FILE_VERSION 4
#else
#define NSSLOWKEY_DB_FILE_VERSION 3
#endif

#define NSSLOWKEY_VERSION	    0	/* what we *create* */

/*
** Typedef for callback to get a password "key".
*/
extern const SEC_ASN1Template lg_nsslowkey_PQGParamsTemplate[];
extern const SEC_ASN1Template lg_nsslowkey_RSAPrivateKeyTemplate[];
extern const SEC_ASN1Template lg_nsslowkey_RSAPrivateKeyTemplate2[];
extern const SEC_ASN1Template lg_nsslowkey_DSAPrivateKeyTemplate[];
extern const SEC_ASN1Template lg_nsslowkey_DHPrivateKeyTemplate[];
extern const SEC_ASN1Template lg_nsslowkey_DHPrivateKeyExportTemplate[];
#ifdef NSS_ENABLE_ECC
#define NSSLOWKEY_EC_PRIVATE_KEY_VERSION   1  /* as per SECG 1 C.4 */
extern const SEC_ASN1Template lg_nsslowkey_ECParamsTemplate[];
extern const SEC_ASN1Template lg_nsslowkey_ECPrivateKeyTemplate[];
#endif /* NSS_ENABLE_ECC */

extern const SEC_ASN1Template lg_nsslowkey_PrivateKeyInfoTemplate[];
extern const SEC_ASN1Template nsslowkey_EncryptedPrivateKeyInfoTemplate[];

/*
 * PKCS #8 attributes
 */
struct NSSLOWKEYAttributeStr {
    SECItem attrType;
    SECItem *attrValue;
};
typedef struct NSSLOWKEYAttributeStr NSSLOWKEYAttribute;

/*
** A PKCS#8 private key info object
*/
struct NSSLOWKEYPrivateKeyInfoStr {
    PLArenaPool *arena;
    SECItem version;
    SECAlgorithmID algorithm;
    SECItem privateKey;
    NSSLOWKEYAttribute **attributes;
};
typedef struct NSSLOWKEYPrivateKeyInfoStr NSSLOWKEYPrivateKeyInfo;
#define NSSLOWKEY_PRIVATE_KEY_INFO_VERSION	0	/* what we *create* */

/*
** A PKCS#8 private key info object
*/
struct NSSLOWKEYEncryptedPrivateKeyInfoStr {
    PLArenaPool *arena;
    SECAlgorithmID algorithm;
    SECItem encryptedData;
};
typedef struct NSSLOWKEYEncryptedPrivateKeyInfoStr NSSLOWKEYEncryptedPrivateKeyInfo;


typedef enum { 
    NSSLOWKEYNullKey = 0, 
    NSSLOWKEYRSAKey = 1, 
    NSSLOWKEYDSAKey = 2, 
    NSSLOWKEYDHKey = 4,
    NSSLOWKEYECKey = 5
} NSSLOWKEYType;

/*
** An RSA public key object.
*/
struct NSSLOWKEYPublicKeyStr {
    PLArenaPool *arena;
    NSSLOWKEYType keyType ;
    union {
        RSAPublicKey rsa;
	DSAPublicKey dsa;
	DHPublicKey  dh;
	ECPublicKey  ec;
    } u;
};
typedef struct NSSLOWKEYPublicKeyStr NSSLOWKEYPublicKey;

/*
** Low Level private key object
** This is only used by the raw Crypto engines (crypto), keydb (keydb),
** and PKCS #11. Everyone else uses the high level key structure.
*/
struct NSSLOWKEYPrivateKeyStr {
    PLArenaPool *arena;
    NSSLOWKEYType keyType;
    union {
        RSAPrivateKey rsa;
	DSAPrivateKey dsa;
	DHPrivateKey  dh;
	ECPrivateKey  ec;
    } u;
};
typedef struct NSSLOWKEYPrivateKeyStr NSSLOWKEYPrivateKey;


typedef struct NSSLOWKEYPasswordEntryStr NSSLOWKEYPasswordEntry;
struct NSSLOWKEYPasswordEntryStr {
    SECItem salt;
    SECItem value;
    unsigned char data[128];
};


#endif	/* _LOWKEYTI_H_ */
