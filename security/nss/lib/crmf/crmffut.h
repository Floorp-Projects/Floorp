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

/*
 * These functions to be implemented in the future if the features
 * which these functions would implement wind up being needed.
 */

/*
 * Use this function to create the CRMFSinglePubInfo* variables that will 
 * populate the inPubInfoArray parameter for the function
 * CRMF_CreatePKIPublicationInfo.
 *
 * "inPubMethod" specifies which publication method will be used
 * "pubLocation" is a representation of the location where 
 */
extern CRMFSinglePubInfo* 
      CRMF_CreateSinglePubInfo(CRMFPublicationMethod  inPubMethod,
			       CRMFGeneralName       *pubLocation);

/*
 * Create a PKIPublicationInfo that can later be passed to the function
 * CRMFAddPubInfoControl.
 */
extern CRMFPKIPublicationInfo *
     CRMF_CreatePKIPublicationInfo(CRMFPublicationAction  inAction,
				   CRMFSinglePubInfo    **inPubInfoArray,
				   int                    numPubInfo);

/*
 * Only call this function on a CRMFPublicationInfo that was created by
 * CRMF_CreatePKIPublicationInfo that was passed in NULL for arena.
 */

extern SECStatus 
       CRMF_DestroyPKIPublicationInfo(CRMFPKIPublicationInfo *inPubInfo);

extern SECStatus CRMF_AddPubInfoControl(CRMFCertRequest        *inCertReq,
					CRMFPKIPublicationInfo *inPubInfo);

/*
 * This is to create a Cert ID Control which can later be added to 
 * a certificate request.
 */
extern CRMFCertID* CRMF_CreateCertID(CRMFGeneralName *issuer,
				     long             serialNumber);

extern SECStatus CRMF_DestroyCertID(CRMFCertID* certID);

extern SECStatus CRMF_AddCertIDControl(CRMFCertRequest *inCertReq,
				       CRMFCertID      *certID);

extern SECStatus 
       CRMF_AddProtocolEncryptioKeyControl(CRMFCertRequest          *inCertReq,
					   CERTSubjectPublicKeyInfo *spki);

/*
 * Add the ASCII Pairs Registration Info to the Certificate Request.
 * The SECItem must be an OCTET string representation.
 */
extern SECStatus
       CRMF_AddUTF8PairsRegInfo(CRMFCertRequest *inCertReq,
				 SECItem         *asciiPairs);

/*
 * This takes a CertRequest and adds it to another CertRequest.  
 */
extern SECStatus
       CRMF_AddCertReqToRegInfo(CRMFCertRequest *certReqToAddTo,
				CRMFCertRequest *certReqBeingAdded);

/*
 * Returns which option was used for the authInfo field of POPOSigningKeyInput
 */
extern CRMFPOPOSkiInputAuthChoice 
       CRMF_GetSignKeyInputAuthChoice(CRMFPOPOSigningKeyInput *inKeyInput);

/*
 * Gets the PKMACValue associated with the POPOSigningKeyInput.
 * If the POPOSigningKeyInput did not use authInfo.publicKeyMAC 
 * the function returns SECFailure and the value at *destValue is unchanged.
 *
 * If the POPOSigningKeyInput did use authInfo.publicKeyMAC, the function
 * returns SECSuccess and places the PKMACValue at *destValue.
 */
extern SECStatus 
       CRMF_GetSignKeyInputPKMACValue(CRMFPOPOSigningKeyInput *inKeyInput,
				      CRMFPKMACValue          **destValue);
/*
 * Gets the SubjectPublicKeyInfo from the POPOSigningKeyInput
 */
extern CERTSubjectPublicKeyInfo *
       CRMF_GetSignKeyInputPublicKey(CRMFPOPOSigningKeyInput *inKeyInput);


/*
 * Return the value for the PKIPublicationInfo Control.
 * A return value of NULL indicates that the Control was 
 * not a PKIPublicationInfo Control.  Call 
 * CRMF_DestroyPKIPublicationInfo on the return value when done
 * using the pointer.
 */
extern CRMFPKIPublicationInfo* CRMF_GetPKIPubInfo(CRMFControl *inControl);

/*
 * Free up a CRMFPKIPublicationInfo structure.
 */
extern SECStatus 
       CRMF_DestroyPKIPublicationInfo(CRMFPKIPublicationInfo *inPubInfo);

/*
 * Get the choice used for action in this PKIPublicationInfo.
 */
extern CRMFPublicationAction 
       CRMF_GetPublicationAction(CRMFPKIPublicationInfo *inPubInfo);

/*
 * Get the number of pubInfos are stored in the PKIPubicationInfo.
 */
extern int CRMF_GetNumPubInfos(CRMFPKIPublicationInfo *inPubInfo);

/*
 * Get the pubInfo at index for the given PKIPubicationInfo.
 * Indexing is done like a traditional C Array. (0 .. numElements-1)
 */
extern CRMFSinglePubInfo* 
       CRMF_GetPubInfoAtIndex(CRMFPKIPublicationInfo *inPubInfo,
			      int                     index);

/*
 * Destroy the CRMFSinglePubInfo.
 */
extern SECStatus CRMF_DestroySinglePubInfo(CRMFSinglePubInfo *inPubInfo);

/*
 * Get the pubMethod used by the SinglePubInfo.
 */
extern CRMFPublicationMethod 
       CRMF_GetPublicationMethod(CRMFSinglePubInfo *inPubInfo);

/*
 * Get the pubLocation associated with the SinglePubInfo.
 * A NULL return value indicates there was no pubLocation associated
 * with the SinglePuInfo.
 */
extern CRMFGeneralName* CRMF_GetPubLocation(CRMFSinglePubInfo *inPubInfo);

/*
 * Get the authInfo.sender field out of the POPOSigningKeyInput.
 * If the POPOSigningKeyInput did not use the authInfo the function
 * returns SECFailure and the value at *destName is unchanged.
 *
 * If the POPOSigningKeyInput did use authInfo.sender, the function returns
 * SECSuccess and puts the authInfo.sender at *destName/
 */
extern SECStatus CRMF_GetSignKeyInputSender(CRMFPOPOSigningKeyInput *keyInput,
					    CRMFGeneralName        **destName);

/**************** CMMF Functions that need to be added. **********************/

/*
 * FUNCTION: CMMF_POPODecKeyChallContentSetNextChallenge
 * INPUTS:
 *    inDecKeyChall
 *        The CMMFPOPODecKeyChallContent to operate on.
 *    inRandom
 *        The random number to use when generating the challenge,
 *    inSender
 *        The GeneralName representation of the sender of the challenge.
 *    inPubKey
 *        The public key to use when encrypting the challenge.
 * NOTES:
 *    This function adds a challenge to the end of the list of challenges
 *    contained by 'inDecKeyChall'.  Refer to the CMMF draft on how the
 *    the random number passed in and the sender's GeneralName are used
 *    to generate the challenge and witness fields of the challenge.  This
 *    library will use SHA1 as the one-way function for generating the 
 *    witess field of the challenge.
 *
 * RETURN:
 *    SECSuccess if generating the challenge and adding to the end of list
 *    of challenges was successful.  Any other return value indicates an error
 *    while trying to generate the challenge.
 */
extern SECStatus
CMMF_POPODecKeyChallContentSetNextChallenge
                                   (CMMFPOPODecKeyChallContent *inDecKeyChall,
				    long                        inRandom,
				    CERTGeneralName            *inSender,
				    SECKEYPublicKey            *inPubKey);

/*
 * FUNCTION: CMMF_POPODecKeyChallContentGetNumChallenges
 * INPUTS:
 *    inKeyChallCont
 *        The CMMFPOPODecKeyChallContent to operate on.
 * RETURN:
 *    This function returns the number of CMMFChallenges are contained in 
 *    the CMMFPOPODecKeyChallContent structure.
 */
extern int CMMF_POPODecKeyChallContentGetNumChallenges
                                  (CMMFPOPODecKeyChallContent *inKeyChallCont);

/*
 * FUNCTION: CMMF_ChallengeGetRandomNumber
 * INPUTS:
 *    inChallenge
 *        The CMMFChallenge to operate on.
 *    inDest
 *        A pointer to a user supplied buffer where the library
 *        can place a copy of the random integer contatained in the
 *        challenge.
 * NOTES:
 *    This function returns the value held in the decrypted Rand structure
 *    corresponding to the random integer.  The user must call 
 *    CMMF_ChallengeDecryptWitness before calling this function.  Call 
 *    CMMF_ChallengeIsDecrypted to find out if the challenge has been 
 *    decrypted.
 *
 * RETURN:
 *    SECSuccess indicates the witness field has been previously decrypted
 *    and the value for the random integer was successfully placed at *inDest.
 *    Any other return value indicates an error and that the value at *inDest
 *    is not a valid value.
 */
extern SECStatus CMMF_ChallengeGetRandomNumber(CMMFChallenge *inChallenge,
					       long          *inDest);

/*
 * FUNCTION: CMMF_ChallengeGetSender
 * INPUTS:
 *    inChallenge
 *        the CMMFChallenge to operate on.
 * NOTES:
 *    This function returns the value held in the decrypted Rand structure
 *    corresponding to the sender.  The user must call 
 *    CMMF_ChallengeDecryptWitness before calling this function.  Call 
 *    CMMF_ChallengeIsDecrypted to find out if the witness field has been
 *    decrypted.  The user must call CERT_DestroyGeneralName after the return
 *    value is no longer needed.
 *
 * RETURN:
 *    A pointer to a copy of the sender CERTGeneralName.  A return value of
 *    NULL indicates an error in trying to copy the information or that the
 *    witness field has not been decrypted.
 */
extern CERTGeneralName* CMMF_ChallengeGetSender(CMMFChallenge *inChallenge);

/*
 * FUNCTION: CMMF_ChallengeGetAlgId
 * INPUTS:
 *    inChallenge
 *        The CMMFChallenge to operate on.
 *    inDestAlgId
 *        A pointer to memory where a pointer to a copy of the algorithm
 *        id can be placed.
 * NOTES:
 *    This function retrieves the one way function algorithm identifier 
 *    contained within the CMMFChallenge if the optional field is present.
 *
 * RETURN:
 *    SECSucces indicates the function was able to place a pointer to a copy of
 *    the alogrithm id at *inAlgId.  If the value at *inDestAlgId is NULL, 
 *    that means there was no algorithm identifier present in the 
 *    CMMFChallenge.  Any other return value indicates the function was not 
 *    able to make a copy of the algorithm identifier.  In this case the value 
 *    at *inDestAlgId is not valid.
 */
extern SECStatus CMMF_ChallengeGetAlgId(CMMFChallenge  *inChallenge,
					SECAlgorithmID *inAlgId);

/*
 * FUNCTION: CMMF_DestroyChallenge
 * INPUTS:
 *    inChallenge
 *        The CMMFChallenge to free up.
 * NOTES:
 *    This function frees up all the memory associated with the CMMFChallenge 
 *    passed in.
 * RETURN:
 *    SECSuccess if freeing all the memory associated with the CMMFChallenge
 *    passed in is successful.  Any other return value indicates an error 
 *    while freeing the memory.
 */
extern SECStatus CMMF_DestroyChallenge (CMMFChallenge *inChallenge);

/*
 * FUNCTION: CMMF_DestroyPOPODecKeyRespContent
 * INPUTS:
 *    inDecKeyResp
 *        The CMMFPOPODecKeyRespContent structure to free.
 * NOTES:
 *    This function frees up all the memory associate with the 
 *    CMMFPOPODecKeyRespContent.
 *
 * RETURN:
 *    SECSuccess if freeint up all the memory associated with the
 *    CMMFPOPODecKeyRespContent structure is successful.  Any other
 *    return value indicates an error while freeing the memory.
 */
extern SECStatus
     CMMF_DestroyPOPODecKeyRespContent(CMMFPOPODecKeyRespContent *inDecKeyResp);

/*
 * FUNCTION: CMMF_ChallengeDecryptWitness
 * INPUTS:
 *    inChallenge
 *        The CMMFChallenge to operate on.
 *    inPrivKey
 *        The private key to use to decrypt the witness field.
 * NOTES:
 *    This function uses the private key to decrypt the challenge field
 *    contained in the CMMFChallenge.  Make sure the private key matches the
 *    public key that was used to encrypt the witness.  The creator of 
 *    the challenge will most likely be an RA that has the public key
 *    from a Cert request.  So the private key should be the private key
 *    associated with public key in that request.  This function will also
 *    verify the witness field of the challenge.
 *
 * RETURN:
 *    SECSuccess if decrypting the witness field was successful.  This does
 *    not indicate that the decrypted data is valid, since the private key 
 *    passed in may not be the actual key needed to properly decrypt the 
 *    witness field.  Meaning that there is a decrypted structure now, but
 *    may be garbage because the private key was incorrect.
 *    Any other return value indicates the function could not complete the
 *    decryption process.
 */
extern SECStatus CMMF_ChallengeDecryptWitness(CMMFChallenge    *inChallenge,
					      SECKEYPrivateKey *inPrivKey);

/*
 * FUNCTION: CMMF_ChallengeIsDecrypted
 * INPUTS:
 *    inChallenge
 *        The CMMFChallenge to operate on.
 * RETURN:
 *    This is a predicate function that returns PR_TRUE if the decryption 
 *    process has already been performed.  The function return PR_FALSE if 
 *    the decryption process has not been performed yet.
 */
extern PRBool CMMF_ChallengeIsDecrypted(CMMFChallenge *inChallenge);

/*
 * FUNCTION: CMMF_DestroyPOPODecKeyChallContent
 * INPUTS:
 *    inDecKeyCont
 *        The CMMFPOPODecKeyChallContent to free
 * NOTES:
 *    This function frees up all the memory associated with the 
 *    CMMFPOPODecKeyChallContent 
 * RETURN:
 *    SECSuccess if freeing up all the memory associatd with the 
 *    CMMFPOPODecKeyChallContent is successful.  Any other return value
 *    indicates an error while freeing the memory.
 *
 */
extern SECStatus 
 CMMF_DestroyPOPODecKeyChallContent (CMMFPOPODecKeyChallContent *inDecKeyCont);

