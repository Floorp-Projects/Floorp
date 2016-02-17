/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cmmf.h"
#include "cmmfi.h"
#include "secitem.h"
#include "pk11func.h"
#include "secder.h"
#include "sechash.h"

CMMFPOPODecKeyChallContent*
CMMF_CreatePOPODecKeyChallContentFromDER(const char *buf, long len)
{
    PLArenaPool                *poolp;
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
    PLArenaPool    *poolp  = NULL;
    SECAlgorithmID *owf;
    SECStatus       rv     = SECFailure;
    SECOidTag       tag;
    CMMFRand        randStr;
    SECItem         hashItem;
    unsigned char   hash[HASH_LENGTH_MAX]; 

    PORT_Assert(inChalCont != NULL && inPrivKey != NULL);
    if (inChalCont == NULL || inIndex <0 || inIndex > inChalCont->numChallenges
	|| inPrivKey == NULL){
        return SECFailure;
    }

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }

    challenge = inChalCont->challenges[inIndex];
    decryptedRand = SECITEM_AllocItem(poolp, NULL, challenge->challenge.len);
    if (decryptedRand == NULL) {
        goto loser;
    }
    rv = PK11_PrivDecryptPKCS1(inPrivKey, decryptedRand->data, 
    			&decryptedRand->len, decryptedRand->len, 
			challenge->challenge.data, challenge->challenge.len);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(poolp, &randStr, CMMFRandTemplate,
			    decryptedRand); 
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
	PORT_SetError(SEC_ERROR_BAD_DATA);
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
	PORT_SetError(SEC_ERROR_BAD_DATA);
        rv = SECFailure;
	goto loser;
    }
    /* All of the hashes have verified, so we can now store the integer away.*/
    rv = SECITEM_CopyItem(inChalCont->poolp, &challenge->randomNumber,
			  &randStr.integer);
 loser:
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
    PLArenaPool *poolp;
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
	currItem = SEC_ASN1EncodeInteger(poolp, currItem, inDecodedRand[i]);
	if (currItem == NULL) {
	    goto loser;
	}
    }
    rv = cmmf_user_encode(response, inCallback, inArg,
			  CMMFPOPODecKeyRespContentTemplate);
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return rv;
}
