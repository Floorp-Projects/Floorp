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

#ifndef _LOWKEYI_H_
#define _LOWKEYI_H_

#include "prtypes.h"
#include "seccomon.h"
#include "secoidt.h"
#include "pcertt.h"
#include "lowkeyti.h"

SEC_BEGIN_PROTOS

/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */
extern void prepare_low_rsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_pqg_params_for_asn1(PQGParams *params);
extern void prepare_low_dsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_dsa_priv_key_export_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_dh_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
#ifdef NSS_ENABLE_ECC
extern void prepare_low_ec_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_ecparams_for_asn1(ECParams *params);
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
 * Clear out all the keys in the existing database
 */
extern SECStatus nsslowkey_ResetKeyDB(NSSLOWKEYDBHandle *handle);

/*
** Close the specified key database.
*/
extern void nsslowkey_CloseKeyDB(NSSLOWKEYDBHandle *handle);

/*
 * Get the version number of the database
 */
extern int nsslowkey_GetKeyDBVersion(NSSLOWKEYDBHandle *handle);

/*
** Support a default key database.
*/
extern void nsslowkey_SetDefaultKeyDB(NSSLOWKEYDBHandle *handle);
extern NSSLOWKEYDBHandle *nsslowkey_GetDefaultKeyDB(void);

/* set the alg id of the key encryption algorithm */
extern void nsslowkey_SetDefaultKeyDBAlg(SECOidTag alg);

/*
 * given a password and salt, produce a hash of the password
 */
extern SECItem *nsslowkey_HashPassword(char *pw, SECItem *salt);

/*
 * Derive the actual password value for a key database from the
 * password string value.  The derivation uses global salt value
 * stored in the key database.
 */
extern SECItem *
nsslowkey_DeriveKeyDBPassword(NSSLOWKEYDBHandle *handle, char *pw);

/*
** Delete a key from the database
*/
extern SECStatus nsslowkey_DeleteKey(NSSLOWKEYDBHandle *handle, 
				  SECItem *pubkey);

/*
** Store a key in the database, indexed by its public key modulus.
**	"pk" is the private key to store
**	"f" is a the callback function for getting the password
**	"arg" is the argument for the callback
*/
extern SECStatus nsslowkey_StoreKeyByPublicKey(NSSLOWKEYDBHandle *handle, 
					    NSSLOWKEYPrivateKey *pk,
					    SECItem *pubKeyData,
					    char *nickname,
					    SECItem *arg);

/* does the key for this cert exist in the database filed by modulus */
extern PRBool nsslowkey_KeyForCertExists(NSSLOWKEYDBHandle *handle,
					 NSSLOWCERTCertificate *cert);
/* does a key with this ID already exist? */
extern PRBool nsslowkey_KeyForIDExists(NSSLOWKEYDBHandle *handle, SECItem *id);


extern SECStatus nsslowkey_HasKeyDBPassword(NSSLOWKEYDBHandle *handle);
extern SECStatus nsslowkey_SetKeyDBPassword(NSSLOWKEYDBHandle *handle,
				     SECItem *pwitem);
extern SECStatus nsslowkey_CheckKeyDBPassword(NSSLOWKEYDBHandle *handle,
					   SECItem *pwitem);
extern SECStatus nsslowkey_ChangeKeyDBPassword(NSSLOWKEYDBHandle *handle,
					    SECItem *oldpwitem,
					    SECItem *newpwitem);

/*
** Destroy a private key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void nsslowkey_DestroyPrivateKey(NSSLOWKEYPrivateKey *key);

/*
** Destroy a public key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void nsslowkey_DestroyPublicKey(NSSLOWKEYPublicKey *key);

/*
** Return the modulus length of "pubKey".
*/
extern unsigned int nsslowkey_PublicModulusLen(NSSLOWKEYPublicKey *pubKey);


/*
** Return the modulus length of "privKey".
*/
extern unsigned int nsslowkey_PrivateModulusLen(NSSLOWKEYPrivateKey *privKey);


/*
** Convert a low private key "privateKey" into a public low key
*/
extern NSSLOWKEYPublicKey 
		*nsslowkey_ConvertToPublicKey(NSSLOWKEYPrivateKey *privateKey);

/*
 * Set the Key Database password.
 *   handle is a handle to the key database
 *   pwitem is the new password
 *   algorithm is the algorithm by which the key database 
 *   	password is to be encrypted.
 * On failure, SECFailure is returned, otherwise SECSuccess is 
 * returned.
 */
extern SECStatus 
nsslowkey_SetKeyDBPasswordAlg(NSSLOWKEYDBHandle *handle,
			SECItem *pwitem, 
			SECOidTag algorithm);

/* Check the key database password.
 *   handle is a handle to the key database
 *   pwitem is the suspect password
 *   algorithm is the algorithm by which the key database 
 *   	password is to be encrypted.
 * The password is checked against plaintext to see if it is the
 * actual password.  If it is not, SECFailure is returned.
 */
extern SECStatus 
nsslowkey_CheckKeyDBPasswordAlg(NSSLOWKEYDBHandle *handle,
				SECItem *pwitem, 
				SECOidTag algorithm);

/* Change the key database password and/or algorithm by which
 * the password is stored with.  
 *   handle is a handle to the key database
 *   old_pwitem is the current password
 *   new_pwitem is the new password
 *   old_algorithm is the algorithm by which the key database 
 *   	password is currently encrypted.
 *   new_algorithm is the algorithm with which the new password
 *      is to be encrypted.
 * A return of anything but SECSuccess indicates failure.
 */
extern SECStatus 
nsslowkey_ChangeKeyDBPasswordAlg(NSSLOWKEYDBHandle *handle,
			      SECItem *oldpwitem, SECItem *newpwitem,
			      SECOidTag old_algorithm);

SECStatus
nsslowkey_UpdateNickname(NSSLOWKEYDBHandle *handle,
                           NSSLOWKEYPrivateKey *privkey,
                           SECItem *pubKeyData,
                           char *nickname,
                           SECItem *arg);

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
			      SECItem *arg,
			      SECOidTag algorithm,
                              PRBool update); 

/* Find key by modulus.  This function is the inverse of store key
 * by modulus.  An attempt to locate the key with "modulus" is 
 * performed.  If the key is found, the private key is returned,
 * else NULL is returned.
 *   modulus is the modulus to locate
 */
extern NSSLOWKEYPrivateKey *
nsslowkey_FindKeyByPublicKey(NSSLOWKEYDBHandle *handle, SECItem *modulus, 
			  SECItem *arg);

extern char *
nsslowkey_FindKeyNicknameByPublicKey(NSSLOWKEYDBHandle *handle,
                                        SECItem *modulus, SECItem *pwitem);


/* Make a copy of a low private key in it's own arena.
 * a return of NULL indicates an error.
 */
extern NSSLOWKEYPrivateKey *
nsslowkey_CopyPrivateKey(NSSLOWKEYPrivateKey *privKey);


SEC_END_PROTOS

#endif /* _LOWKEYI_H_ */
