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
 */

#ifndef _SECPKCS5_H_
#define _SECPKCS5_H_

#include "plarena.h"
#include "secitem.h"
#include "seccomon.h"
#include "secoidt.h"
#include "hasht.h"

typedef SECItem * (* SEC_PKCS5GetPBEPassword)(void *arg);

/* used for V2 PKCS 12 Draft Spec */ 
typedef enum {
    pbeBitGenIDNull = 0,
    pbeBitGenCipherKey = 0x01,
    pbeBitGenCipherIV = 0x02,
    pbeBitGenIntegrityKey = 0x03
} PBEBitGenID;

typedef struct _pbeBitGenParameters {
    unsigned int u, v;
    SECOidTag hashAlgorithm;
} pbeBitGenParameters;

typedef struct _PBEBitGenContext {
    PRArenaPool *arena;

    /* hash algorithm information */
    pbeBitGenParameters pbeParams;
    const SECHashObject *hashObject;
    void *hash;

    /* buffers used in generation of bits */
    SECItem D, S, P, I, A, B;
    unsigned int c, n;
    unsigned int iterations;
} PBEBitGenContext;

extern const SEC_ASN1Template SEC_PKCS5PBEParameterTemplate[];
typedef struct SEC_PKCS5PBEParameterStr SEC_PKCS5PBEParameter;

struct SEC_PKCS5PBEParameterStr {
    PRArenaPool *poolp;
    SECItem	salt;		/* octet string */
    SECItem	iteration;	/* integer */

    /* used locally */
    SECOidTag	algorithm;
    int		iter;
};


SEC_BEGIN_PROTOS
/* Create a PKCS5 Algorithm ID
 * The algorithm ID is set up using the PKCS #5 parameter structure
 *  algorithm is the PBE algorithm ID for the desired algorithm
 *  salt can be specified or can be NULL, if salt is NULL then the
 *    salt is generated from random bytes
 *  iteration is the number of iterations for which to perform the
 *    hash prior to key and iv generation.
 * If an error occurs or the algorithm specified is not supported 
 * or is not a password based encryption algorithm, NULL is returned.
 * Otherwise, a pointer to the algorithm id is returned.
 */
extern SECAlgorithmID *
SEC_PKCS5CreateAlgorithmID(SECOidTag algorithm,
			   SECItem *salt, 
			   int iteration);

/* Get the initialization vector.  The password is passed in, hashing
 * is performed, and the initialization vector is returned.
 *  algid is a pointer to a PBE algorithm ID
 *  pwitem is the password
 * If an error occurs or the algorithm id is not a PBE algrithm,
 * NULL is returned.  Otherwise, the iv is returned in a secitem.
 */
extern SECItem *
SEC_PKCS5GetIV(SECAlgorithmID *algid, SECItem *pwitem, PRBool faulty3DES);

/* Get the key.  The password is passed in, hashing is performed,
 * and the key is returned.
 *  algid is a pointer to a PBE algorithm ID
 *  pwitem is the password
 * If an error occurs or the algorithm id is not a PBE algrithm,
 * NULL is returned.  Otherwise, the key is returned in a secitem.
 */
extern SECItem *
SEC_PKCS5GetKey(SECAlgorithmID *algid, SECItem *pwitem, PRBool faulty3DES);

/* Get PBE salt.  The salt for the password based algorithm is returned.
 *  algid is the PBE algorithm identifier
 * If an error occurs NULL is returned, otherwise the salt is returned
 * in a SECItem.
 */
extern SECItem *
SEC_PKCS5GetSalt(SECAlgorithmID *algid);

/* Encrypt/Decrypt data using password based encryption.  
 *  algid is the PBE algorithm identifier,
 *  pwitem is the password,
 *  src is the source for encryption/decryption,
 *  encrypt is PR_TRUE for encryption, PR_FALSE for decryption.
 * The key and iv are generated based upon PKCS #5 then the src
 * is either encrypted or decrypted.  If an error occurs, NULL
 * is returned, otherwise the ciphered contents is returned.
 */
extern SECItem *
SEC_PKCS5CipherData(SECAlgorithmID *algid, SECItem *pwitem,
		    SECItem *src, PRBool encrypt, PRBool *update);

/* Checks to see if algid algorithm is a PBE algorithm.  If
 * so, PR_TRUE is returned, otherwise PR_FALSE is returned.
 */
extern PRBool 
SEC_PKCS5IsAlgorithmPBEAlg(SECAlgorithmID *algid);

/* Destroys PBE parameter */
extern void
SEC_PKCS5DestroyPBEParameter(SEC_PKCS5PBEParameter *param);

/* Convert Algorithm ID to PBE parameter */
extern SEC_PKCS5PBEParameter *
SEC_PKCS5GetPBEParameter(SECAlgorithmID *algid);

/* Determine how large the key generated is */
extern int
SEC_PKCS5GetKeyLength(SECAlgorithmID *algid);

/* map crypto algorithm to pbe algorithm, assume sha 1 hashing for DES
 */
extern SECOidTag
SEC_PKCS5GetPBEAlgorithm(SECOidTag algTag, int keyLen);

/* return the underlying crypto algorithm */
extern SECOidTag
SEC_PKCS5GetCryptoAlgorithm(SECAlgorithmID *algid);

extern PBEBitGenContext *
PBE_CreateContext(SECOidTag hashAlgorithm, PBEBitGenID bitGenPurpose,
		  SECItem *pwitem, SECItem *salt, unsigned int bitsNeeded,
		  unsigned int interations);
		
extern SECItem *
PBE_GenerateBits(PBEBitGenContext *pbeCtxt);

extern void PBE_DestroyContext(PBEBitGenContext *pbeCtxt);

extern SECStatus PBE_PK11ParamToAlgid(SECOidTag algTag, SECItem *param,
				  PRArenaPool *arena, SECAlgorithmID *algId);
SEC_END_PROTOS

#endif
