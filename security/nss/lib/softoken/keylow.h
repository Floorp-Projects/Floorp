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
 * $Id: keylow.h,v 1.1 2000/03/31 19:26:01 relyea%netscape.com Exp $
 */

#ifndef _KEYLOW_H_
#define _KEYLOW_H_

#include "prtypes.h"
#include "seccomon.h"
#include "keydbt.h"
#include "secoidt.h"
#include "certt.h"
#include "keythi.h"

SEC_BEGIN_PROTOS

typedef char * (* SECKEYDBNameFunc)(void *arg, int dbVersion);
    
/*
** Open a key database.
*/
extern SECKEYKeyDBHandle *SECKEY_OpenKeyDB(PRBool readOnly,
					   SECKEYDBNameFunc namecb,
					   void *cbarg);

extern SECKEYKeyDBHandle *SECKEY_OpenKeyDBFilename(char *filename,
						   PRBool readOnly);

/*
** Update the database
*/
extern SECStatus SECKEY_UpdateKeyDBPass1(SECKEYKeyDBHandle *handle);
extern SECStatus SECKEY_UpdateKeyDBPass2(SECKEYKeyDBHandle *handle,
					 SECItem *pwitem);

/*
 * Clear out all the keys in the existing database
 */
extern SECStatus SECKEY_ResetKeyDB(SECKEYKeyDBHandle *handle);

/*
** Close the specified key database.
*/
extern void SECKEY_CloseKeyDB(SECKEYKeyDBHandle *handle);

/*
 * Get the version number of the database
 */
extern int SECKEY_GetKeyDBVersion(SECKEYKeyDBHandle *handle);

/*
** Support a default key database.
*/
extern void SECKEY_SetDefaultKeyDB(SECKEYKeyDBHandle *handle);
extern SECKEYKeyDBHandle *SECKEY_GetDefaultKeyDB(void);

/* set the alg id of the key encryption algorithm */
extern void SECKEY_SetDefaultKeyDBAlg(SECOidTag alg);

/*
 * given a password and salt, produce a hash of the password
 */
extern SECItem *SECKEY_HashPassword(char *pw, SECItem *salt);

/*
 * Derive the actual password value for a key database from the
 * password string value.  The derivation uses global salt value
 * stored in the key database.
 */
extern SECItem *
SECKEY_DeriveKeyDBPassword(SECKEYKeyDBHandle *handle, char *pw);

/*
** Delete a key from the database
*/
extern SECStatus SECKEY_DeleteKey(SECKEYKeyDBHandle *handle, 
				  SECItem *pubkey);

/*
** Store a key in the database, indexed by its public key modulus.
**	"pk" is the private key to store
**	"f" is a the callback function for getting the password
**	"arg" is the argument for the callback
*/
extern SECStatus SECKEY_StoreKeyByPublicKey(SECKEYKeyDBHandle *handle, 
					    SECKEYLowPrivateKey *pk,
					    SECItem *pubKeyData,
					    char *nickname,
					    SECKEYGetPasswordKey f, void *arg);

/* does the key for this cert exist in the database filed by modulus */
extern SECStatus SECKEY_KeyForCertExists(SECKEYKeyDBHandle *handle,
					 CERTCertificate *cert);

SECKEYLowPrivateKey *
SECKEY_FindKeyByCert(SECKEYKeyDBHandle *handle, CERTCertificate *cert,
		     SECKEYGetPasswordKey f, void *arg);

extern SECStatus SECKEY_HasKeyDBPassword(SECKEYKeyDBHandle *handle);
extern SECStatus SECKEY_SetKeyDBPassword(SECKEYKeyDBHandle *handle,
				     SECItem *pwitem);
extern SECStatus SECKEY_CheckKeyDBPassword(SECKEYKeyDBHandle *handle,
					   SECItem *pwitem);
extern SECStatus SECKEY_ChangeKeyDBPassword(SECKEYKeyDBHandle *handle,
					    SECItem *oldpwitem,
					    SECItem *newpwitem);

/*
** Destroy a private key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void SECKEY_LowDestroyPrivateKey(SECKEYLowPrivateKey *key);

/*
** Destroy a public key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void SECKEY_LowDestroyPublicKey(SECKEYLowPublicKey *key);

/*
** Return the modulus length of "pubKey".
*/
extern unsigned int SECKEY_LowPublicModulusLen(SECKEYLowPublicKey *pubKey);


/*
** Return the modulus length of "privKey".
*/
extern unsigned int SECKEY_LowPrivateModulusLen(SECKEYLowPrivateKey *privKey);


/*
** Convert a low private key "privateKey" into a public low key
*/
extern SECKEYLowPublicKey 
		*SECKEY_LowConvertToPublicKey(SECKEYLowPrivateKey *privateKey);

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
SECKEY_SetKeyDBPasswordAlg(SECKEYKeyDBHandle *handle,
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
SECKEY_CheckKeyDBPasswordAlg(SECKEYKeyDBHandle *handle,
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
SECKEY_ChangeKeyDBPasswordAlg(SECKEYKeyDBHandle *handle,
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
SECKEY_StoreKeyByPublicKeyAlg(SECKEYKeyDBHandle *handle, 
			      SECKEYLowPrivateKey *privkey, 
			      SECItem *pubKeyData,
			      char *nickname,
			      SECKEYGetPasswordKey f, void *arg,
			      SECOidTag algorithm); 

/* Find key by modulus.  This function is the inverse of store key
 * by modulus.  An attempt to locate the key with "modulus" is 
 * performed.  If the key is found, the private key is returned,
 * else NULL is returned.
 *   modulus is the modulus to locate
 */
extern SECKEYLowPrivateKey *
SECKEY_FindKeyByPublicKey(SECKEYKeyDBHandle *handle, SECItem *modulus, 
			  SECKEYGetPasswordKey f, void *arg);

/* Make a copy of a low private key in it's own arena.
 * a return of NULL indicates an error.
 */
extern SECKEYLowPrivateKey *
SECKEY_CopyLowPrivateKey(SECKEYLowPrivateKey *privKey);


SEC_END_PROTOS

#endif /* _KEYLOW_H_ */
