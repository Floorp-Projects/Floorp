/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: lowkeyi.h,v 1.5 2012/04/25 14:50:11 gerv%gerv.net Exp $ */

#ifndef _LOWKEYI_H_
#define _LOWKEYI_H_

#include "prtypes.h"
#include "seccomon.h"
#include "secoidt.h"
#include "pcertt.h"
#include "lowkeyti.h"
#include "sdb.h" 

SEC_BEGIN_PROTOS

/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */
extern void lg_prepare_low_rsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void lg_prepare_low_pqg_params_for_asn1(PQGParams *params);
extern void lg_prepare_low_dsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void lg_prepare_low_dh_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
#ifdef NSS_ENABLE_ECC
extern void lg_prepare_low_ec_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void lg_prepare_low_ecparams_for_asn1(ECParams *params);
#endif /* NSS_ENABLE_ECC */

typedef char * (* NSSLOWKEYDBNameFunc)(void *arg, int dbVersion);
    
/*
** Open a key database.
*/
extern NSSLOWKEYDBHandle *nsslowkey_OpenKeyDB(PRBool readOnly,
					   const char *domain,
					   const char *prefix,
					   NSSLOWKEYDBNameFunc namecb,
					   void *cbarg);

/*
** Close the specified key database.
*/
extern void nsslowkey_CloseKeyDB(NSSLOWKEYDBHandle *handle);

/*
 * Get the version number of the database
 */
extern int nsslowkey_GetKeyDBVersion(NSSLOWKEYDBHandle *handle);

/*
** Delete a key from the database
*/
extern SECStatus nsslowkey_DeleteKey(NSSLOWKEYDBHandle *handle, 
				  const SECItem *pubkey);

/*
** Store a key in the database, indexed by its public key modulus.
**	"pk" is the private key to store
**	"f" is the callback function for getting the password
**	"arg" is the argument for the callback
*/
extern SECStatus nsslowkey_StoreKeyByPublicKey(NSSLOWKEYDBHandle *handle, 
					    NSSLOWKEYPrivateKey *pk,
					    SECItem *pubKeyData,
					    char *nickname,
					    SDB *sdb);

/* does the key for this cert exist in the database filed by modulus */
extern PRBool nsslowkey_KeyForCertExists(NSSLOWKEYDBHandle *handle,
					 NSSLOWCERTCertificate *cert);
/* does a key with this ID already exist? */
extern PRBool nsslowkey_KeyForIDExists(NSSLOWKEYDBHandle *handle, SECItem *id);

/*
** Destroy a private key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void lg_nsslowkey_DestroyPrivateKey(NSSLOWKEYPrivateKey *key);

/*
** Destroy a public key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void lg_nsslowkey_DestroyPublicKey(NSSLOWKEYPublicKey *key);


/*
** Convert a low private key "privateKey" into a public low key
*/
extern NSSLOWKEYPublicKey 
	*lg_nsslowkey_ConvertToPublicKey(NSSLOWKEYPrivateKey *privateKey);


SECStatus
nsslowkey_UpdateNickname(NSSLOWKEYDBHandle *handle,
                           NSSLOWKEYPrivateKey *privkey,
                           SECItem *pubKeyData,
                           char *nickname,
                           SDB *sdb);

/* Store key by modulus and specify an encryption algorithm to use.
 *   handle is the pointer to the key database,
 *   privkey is the private key to be stored,
 *   f and arg are the function and arguments to the callback
 *       to get a password,
 *   algorithm is the algorithm which the privKey is to be stored.
 * A return of anything but SECSuccess indicates failure.
 */
extern SECStatus 
nsslowkey_StoreKeyByPublicKeyAlg(NSSLOWKEYDBHandle *handle, 
			      NSSLOWKEYPrivateKey *privkey, 
			      SECItem *pubKeyData,
			      char *nickname,
			      SDB *sdb,
                              PRBool update); 

/* Find key by modulus.  This function is the inverse of store key
 * by modulus.  An attempt to locate the key with "modulus" is 
 * performed.  If the key is found, the private key is returned,
 * else NULL is returned.
 *   modulus is the modulus to locate
 */
extern NSSLOWKEYPrivateKey *
nsslowkey_FindKeyByPublicKey(NSSLOWKEYDBHandle *handle, SECItem *modulus, 
			  SDB *sdb);

extern char *
nsslowkey_FindKeyNicknameByPublicKey(NSSLOWKEYDBHandle *handle,
                                        SECItem *modulus, SDB *sdb);

#ifdef NSS_ENABLE_ECC
/*
 * smaller version of EC_FillParams. In this code, we only need
 * oid and DER data.
 */
SECStatus LGEC_FillParams(PRArenaPool *arena, const SECItem *encodedParams, 
    ECParams *params);

/* Copy all of the fields from srcParams into dstParams */
SECStatus LGEC_CopyParams(PRArenaPool *arena, ECParams *dstParams,
	      const ECParams *srcParams);
#endif
SEC_END_PROTOS

#endif /* _LOWKEYI_H_ */
