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
 *
 * key.h - public data structures and prototypes for the private key library
 *
 * $Id: lowkeyi.h,v 1.2 2001/11/08 00:15:35 relyea%netscape.com Exp $
 */

#ifndef _LOWKEYI_H_
#define _LOWKEYI_H_

#include "prtypes.h"
#include "seccomon.h"
#include "secoidt.h"
#include "pcertt.h"
#include "lowkeyti.h"

SEC_BEGIN_PROTOS

typedef char * (* NSSLOWKEYDBNameFunc)(void *arg, int dbVersion);
    
/*
** Open a key database.
*/
extern NSSLOWKEYDBHandle *nsslowkey_OpenKeyDB(PRBool readOnly,
					   NSSLOWKEYDBNameFunc namecb,
					   void *cbarg);

extern NSSLOWKEYDBHandle *nsslowkey_OpenKeyDBFilename(char *filename,
						   PRBool readOnly);

/*
** Update the database
*/
extern SECStatus nsslowkey_UpdateKeyDBPass1(NSSLOWKEYDBHandle *handle);
extern SECStatus nsslowkey_UpdateKeyDBPass2(NSSLOWKEYDBHandle *handle,
					 SECItem *pwitem);

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
			      SECOidTag algorithm); 

/* Find key by modulus.  This function is the inverse of store key
 * by modulus.  An attempt to locate the key with "modulus" is 
 * performed.  If the key is found, the private key is returned,
 * else NULL is returned.
 *   modulus is the modulus to locate
 */
extern NSSLOWKEYPrivateKey *
nsslowkey_FindKeyByPublicKey(NSSLOWKEYDBHandle *handle, SECItem *modulus, 
			  SECItem *arg);

/* Make a copy of a low private key in it's own arena.
 * a return of NULL indicates an error.
 */
extern NSSLOWKEYPrivateKey *
nsslowkey_CopyPrivateKey(NSSLOWKEYPrivateKey *privKey);


SEC_END_PROTOS

#endif /* _LOWKEYI_H_ */
