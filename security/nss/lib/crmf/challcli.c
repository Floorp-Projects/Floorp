/* -*- Mode: C; tab-width: 8 -*-*/
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

#include "cmmf.h"
#include "cmmfi.h"
#include "secitem.h"
#include "pk11func.h"
#include "secder.h"
#include "sechash.h"

CMMFPOPODecKeyChallContent*
CMMF_CreatePOPODecKeyChallContentFromDER(const char *buf, long len)
{
    PRArenaPool                *poolp;
    CMMFPOPODecKeyChallContent *challContent;
    SECStatus                   rv;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    challContent = PORT_ArenaZNew(poolp, CMMFPOPODecKeyChallContent);
    if (challContent == NULL) {
        goto loser;
    }
    challContent->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, challContent, 
			CMMFPOPODecKeyChallContentTemplate, buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (challContent->challenges) {
      while (challContent->challenges[challContent->numChallenges] != NULL) {
	  challContent->numChallenges++;
      }
      challContent->numAllocated = challContent->numChallenges;
    }
    return challContent;
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

int
CMMF_POPODecKeyChallContentGetNumChallenges 
                              (CMMFPOPODecKeyChallContent *inKeyChallCont)
{
    PORT_Assert(inKeyChallCont != NULL);
    if (inKeyChallCont == NULL) {
        return 0;
    }
    return inKeyChallCont->numChallenges;
}

SECItem* 
CMMF_POPODecKeyChallContentGetPublicValue
                                   (CMMFPOPODecKeyChallContent *inKeyChallCont,
				    int                         inIndex)
{
    PORT_Assert(inKeyChallCont != NULL);
    if (inKeyChallCont == NULL || (inIndex > inKeyChallCont->numChallenges-1)||
	inIndex < 0) {
        return NULL;
    }
    return SECITEM_DupItem(&inKeyChallCont->challenges[inIndex]->key);
}

static SECAlgorithmID*
cmmf_get_owf(CMMFPOPODecKeyChallContent *inChalCont, 
	     int                         inIndex)
{
   int i;
   
   for (i=inIndex; i >= 0; i--) {
       if (inChalCont->challenges[i]->owf != NULL) {
	   return inChalCont->challenges[i]->owf;
       }
   }
   return NULL;
}

SECStatus 
CMMF_POPODecKeyChallContDecryptChallenge(CMMFPOPODecKeyChallContent *inChalCont,
					 int                         inIndex,
					 SECKEYPrivateKey           *inPrivKey)
{
    CMMFChallenge  *challenge;
    SECItem        *decryptedRand=NULL;
    SECAlgorithmID *owf;
    PK11SlotInfo   *slot;
    PK11SymKey     *symKey = NULL;
    SECStatus       rv     = SECFailure;
    CMMFRand        randStr;
    SECItem         hashItem;
    SECOidTag       tag;
    unsigned char   hash[HASH_LENGTH_MAX]; 
    PRArenaPool    *poolp  = NULL;

    PORT_Assert(inChalCont != NULL && inPrivKey != NULL);
    if (inChalCont == NULL || inIndex <0 || inIndex > inChalCont->numChallenges
	|| inPrivKey == NULL){
        return SECFailure;
    }
    challenge = inChalCont->challenges[inIndex];
    decryptedRand = PORT_ZNew(SECItem);
    if (decryptedRand == NULL) {
        goto loser;
    }
    decryptedRand->data = 
        PORT_NewArray(unsigned char, challenge->challenge.len);
    if (decryptedRand->data == NULL) {
        goto loser;
    }
    slot = inPrivKey->pkcs11Slot;
    symKey = PK11_PubUnwrapSymKey(inPrivKey, &challenge->challenge, 
				  CKM_RSA_PKCS, CKA_VALUE, 0);
    if (symKey == NULL) {
      rv = SECFailure;
      goto loser;
    }
    rv = PK11_ExtractKeyValue(symKey);
    if (rv != SECSuccess) {
      goto loser;
    }
    decryptedRand = PK11_GetKeyData(symKey);

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }
    rv = SEC_ASN1DecodeItem(poolp, &randStr, CMMFRandTemplate,
			    decryptedRand); 
    /* The decryptedRand returned points to a member within the symKey 
     * structure, so we don't want to free it. Let the symKey destruction 
     * function deal with freeing that memory.
     */
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECFailure; /* Just so that when we do go to loser,
		      * I won't have to set it again.
		      */
    owf = cmmf_get_owf(inChalCont, inIndex);
    if (owf == NULL) {
        /* No hashing algorithm came with the challenges.  Can't verify */
        goto loser;
    }
    /* Verify the hashes in the challenge */
    tag = SECOID_FindOIDTag(&owf->algorithm);
    hashItem.len = HASH_ResultLenByOidTag(tag);
    if (!hashItem.len)
        goto loser;	/* error code has been set */

    rv = PK11_HashBuf(tag, hash, randStr.integer.data, randStr.integer.len);
    if (rv != SECSuccess) {
        goto loser;
    }
    hashItem.data = hash;
    if (SECITEM_CompareItem(&hashItem, &challenge->witness) != SECEqual) {
        /* The hash for the data we decrypted doesn't match the hash provided
	 * in the challenge.  Bail out.
	 */
        rv = SECFailure;
	goto loser;
    }
    rv = PK11_HashBuf(tag, hash, challenge->senderDER.data, 
		      challenge->senderDER.len);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (SECITEM_CompareItem(&hashItem, &randStr.senderHash) != SECEqual) {
        /* The hash for the data we decrypted doesn't match the hash provided
	 * in the challenge.  Bail out.
	 */
        rv = SECFailure;
	goto loser;
    }
    /* All of the hashes have verified, so we can now store the integer away.*/
    rv = SECITEM_CopyItem(inChalCont->poolp, &challenge->randomNumber,
			  &randStr.integer);
 loser:
    if (symKey != NULL) {
        PK11_FreeSymKey(symKey);
    }
    if (poolp) {
    	PORT_FreeArena(poolp, PR_FALSE);
    }
    return rv;
}

SECStatus
CMMF_POPODecKeyChallContentGetRandomNumber
                                   (CMMFPOPODecKeyChallContent *inKeyChallCont,
				    int                          inIndex,
				    long                        *inDest)
{
    CMMFChallenge *challenge;
    
    PORT_Assert(inKeyChallCont != NULL);
    if (inKeyChallCont == NULL || inIndex > 0 || inIndex >= 
	inKeyChallCont->numChallenges) {
        return SECFailure;
    }
    challenge = inKeyChallCont->challenges[inIndex];
    if (challenge->randomNumber.data == NULL) {
        /* There is no random number here, nothing to see. */
        return SECFailure;
    }
    *inDest = DER_GetInteger(&challenge->randomNumber);
    return (*inDest == -1) ? SECFailure : SECSuccess;
}

SECStatus 
CMMF_EncodePOPODecKeyRespContent(long                     *inDecodedRand,
				 int                       inNumRand,
				 CRMFEncoderOutputCallback inCallback,
				 void                     *inArg)
{
    PRArenaPool *poolp;
    CMMFPOPODecKeyRespContent *response;
    SECItem *currItem;
    SECStatus rv=SECFailure;
    int i;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return SECFailure;
    }
    response = PORT_ArenaZNew(poolp, CMMFPOPODecKeyRespContent);
    if (response == NULL) {
        goto loser;
    }
    response->responses = PORT_ArenaZNewArray(poolp, SECItem*, inNumRand+1);
    if (response->responses == NULL) {
        goto loser;
    }
    for (i=0; i<inNumRand; i++) {
        currItem = response->responses[i] = PORT_ArenaZNew(poolp,SECItem);
	if (currItem == NULL) {
	    goto loser;
	}
	SEC_ASN1EncodeInteger(poolp, currItem,inDecodedRand[i]);
    }
    rv = cmmf_user_encode(response, inCallback, inArg,
			  CMMFPOPODecKeyRespContentTemplate);
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return rv;
}
