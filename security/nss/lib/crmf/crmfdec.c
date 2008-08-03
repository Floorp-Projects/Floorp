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


#include "crmf.h"
#include "crmfi.h"
#include "secitem.h"

static CRMFPOPChoice
crmf_get_popchoice_from_der(SECItem *derPOP)
{
    CRMFPOPChoice retChoice;

    switch (derPOP->data[0] & 0x0f) {
    case 0:
        retChoice = crmfRAVerified;
	break;
    case 1:
        retChoice = crmfSignature;
	break;
    case 2:
        retChoice = crmfKeyEncipherment;
	break;
    case 3:
        retChoice = crmfKeyAgreement;
	break;
    default:
        retChoice = crmfNoPOPChoice;
	break;
    }
    return retChoice;
}

static SECStatus
crmf_decode_process_raverified(CRMFCertReqMsg *inCertReqMsg)
{   
    CRMFProofOfPossession *pop;
    /* Just set up the structure so that the message structure
     * looks like one that was created using the API
     */
    pop = inCertReqMsg->pop;
    pop->popChoice.raVerified.data = NULL;
    pop->popChoice.raVerified.len  = 0;
    return SECSuccess;
}

static SECStatus
crmf_decode_process_signature(CRMFCertReqMsg *inCertReqMsg)
{
    PORT_Assert(inCertReqMsg->poolp);
    if (!inCertReqMsg->poolp) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    return SEC_ASN1Decode(inCertReqMsg->poolp,
			  &inCertReqMsg->pop->popChoice.signature,
			  CRMFPOPOSigningKeyTemplate, 
			  (const char*)inCertReqMsg->derPOP.data,
			  inCertReqMsg->derPOP.len);
}

static CRMFPOPOPrivKeyChoice
crmf_get_messagechoice_from_der(SECItem *derPOP)
{
    CRMFPOPOPrivKeyChoice retChoice;

    switch (derPOP->data[2] & 0x0f) {
    case 0:
        retChoice = crmfThisMessage;
	break;
    case 1:
        retChoice = crmfSubsequentMessage;
	break;
    case 2:
        retChoice = crmfDHMAC;
	break;
    default:
        retChoice = crmfNoMessage;
    }
    return retChoice;
}

static SECStatus
crmf_decode_process_popoprivkey(CRMFCertReqMsg *inCertReqMsg)
{
    /* We've got a union, so a pointer to one POPOPrivKey
     * struct is the same as having a pointer to the other 
     * one.
     */
    CRMFPOPOPrivKey *popoPrivKey = 
                    &inCertReqMsg->pop->popChoice.keyEncipherment;
    SECItem         *derPOP, privKeyDer;
    SECStatus        rv;

    derPOP = &inCertReqMsg->derPOP;
    popoPrivKey->messageChoice = crmf_get_messagechoice_from_der(derPOP);
    if (popoPrivKey->messageChoice == crmfNoMessage) {
        return SECFailure;
    }
    /* If we ever encounter BER encodings of this, we'll get in trouble*/
    switch (popoPrivKey->messageChoice) {
    case crmfThisMessage:
    case crmfDHMAC:
        privKeyDer.data = &derPOP->data[5];
	privKeyDer.len  = derPOP->len - 5;
	break;
    case crmfSubsequentMessage:
        privKeyDer.data = &derPOP->data[4];
	privKeyDer.len  = derPOP->len - 4;
	break;
    default:
        rv = SECFailure;
    }

    rv = SECITEM_CopyItem(inCertReqMsg->poolp, 
			  &popoPrivKey->message.subsequentMessage,
			  &privKeyDer);

    if (rv != SECSuccess) {
        return rv;
    }

    if (popoPrivKey->messageChoice == crmfThisMessage ||
	popoPrivKey->messageChoice == crmfDHMAC) {

        popoPrivKey->message.thisMessage.len = 
	    CRMF_BYTES_TO_BITS(privKeyDer.len) - (int)derPOP->data[4];
        
    }
    return SECSuccess;    
}

static SECStatus
crmf_decode_process_keyagreement(CRMFCertReqMsg *inCertReqMsg)
{
    return crmf_decode_process_popoprivkey(inCertReqMsg);
}

static SECStatus
crmf_decode_process_keyencipherment(CRMFCertReqMsg *inCertReqMsg)
{
    SECStatus rv;

    rv = crmf_decode_process_popoprivkey(inCertReqMsg);
    if (rv != SECSuccess) {
        return rv;
    }
    if (inCertReqMsg->pop->popChoice.keyEncipherment.messageChoice == 
	crmfDHMAC) {
        /* Key Encipherment can not use the dhMAC option for
	 * POPOPrivKey. 
	 */
        return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
crmf_decode_process_pop(CRMFCertReqMsg *inCertReqMsg)
{
     SECItem               *derPOP;
     PRArenaPool           *poolp;
     CRMFProofOfPossession *pop;
     void                  *mark;
     SECStatus              rv;

     derPOP = &inCertReqMsg->derPOP;
     poolp  = inCertReqMsg->poolp;
     if (derPOP->data == NULL) {
         /* There is no Proof of Possession field in this message. */
         return SECSuccess;
     }
     mark = PORT_ArenaMark(poolp);
     pop = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
     if (pop == NULL) {
         goto loser;
     }
     pop->popUsed = crmf_get_popchoice_from_der(derPOP);
     if (pop->popUsed == crmfNoPOPChoice) {
         /* A bad encoding of CRMF.  Not a valid tag was given to the
	  * Proof Of Possession field.
	  */
         goto loser;
     }
     inCertReqMsg->pop = pop;
     switch (pop->popUsed) {
     case crmfRAVerified:
         rv = crmf_decode_process_raverified(inCertReqMsg);
	 break;
     case crmfSignature:
         rv = crmf_decode_process_signature(inCertReqMsg);
	 break;
     case crmfKeyEncipherment:
         rv = crmf_decode_process_keyencipherment(inCertReqMsg);
	 break;
     case crmfKeyAgreement:
         rv = crmf_decode_process_keyagreement(inCertReqMsg);
	 break;
     default:
         rv = SECFailure;
     }
     if (rv != SECSuccess) {
         goto loser;
     }
     PORT_ArenaUnmark(poolp, mark);
     return SECSuccess;

 loser:
     PORT_ArenaRelease(poolp, mark);
     inCertReqMsg->pop = NULL;
     return SECFailure;
     
}

static SECStatus
crmf_decode_process_single_control(PRArenaPool *poolp, 
				   CRMFControl *inControl)
{
    const SEC_ASN1Template *asn1Template = NULL;

    inControl->tag = SECOID_FindOIDTag(&inControl->derTag);
    asn1Template = crmf_get_pkiarchiveoptions_subtemplate(inControl);

    PORT_Assert (asn1Template != NULL);
    PORT_Assert (poolp != NULL);
    if (!asn1Template || !poolp) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    /* We've got a union, so passing a pointer to one element of the
     * union is the same as passing a pointer to any of the other
     * members of the union.
     */
    return SEC_ASN1Decode(poolp, &inControl->value.archiveOptions, 
			  asn1Template, (const char*)inControl->derValue.data,
			  inControl->derValue.len);
}

static SECStatus 
crmf_decode_process_controls(CRMFCertReqMsg *inCertReqMsg)
{
    int           i, numControls;
    SECStatus     rv;
    PRArenaPool  *poolp;
    CRMFControl **controls;
    
    numControls = CRMF_CertRequestGetNumControls(inCertReqMsg->certReq);
    controls = inCertReqMsg->certReq->controls;
    poolp    = inCertReqMsg->poolp;
    for (i=0; i < numControls; i++) {
        rv = crmf_decode_process_single_control(poolp, controls[i]);
	if (rv != SECSuccess) {
	    return SECFailure;
	}
    }
    return SECSuccess;
}

static SECStatus
crmf_decode_process_single_reqmsg(CRMFCertReqMsg *inCertReqMsg)
{
    SECStatus rv;

    rv = crmf_decode_process_pop(inCertReqMsg);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = crmf_decode_process_controls(inCertReqMsg);
    if (rv != SECSuccess) {
        goto loser;
    }
    inCertReqMsg->certReq->certTemplate.numExtensions = 
        CRMF_CertRequestGetNumberOfExtensions(inCertReqMsg->certReq);
    inCertReqMsg->isDecoded = PR_TRUE;
    rv = SECSuccess;
 loser:
    return rv;
}

CRMFCertReqMsg*
CRMF_CreateCertReqMsgFromDER (const char * buf, long len)
{
    PRArenaPool    *poolp;
    CRMFCertReqMsg *certReqMsg;
    SECStatus       rv;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }
    certReqMsg = PORT_ArenaZNew (poolp, CRMFCertReqMsg);
    if (certReqMsg == NULL) {
        goto loser;
    }
    certReqMsg->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, certReqMsg, CRMFCertReqMsgTemplate, buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = crmf_decode_process_single_reqmsg(certReqMsg);
    if (rv != SECSuccess) {
        goto loser;
    }

    return certReqMsg;
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

CRMFCertReqMessages*
CRMF_CreateCertReqMessagesFromDER(const char *buf, long len)
{
    long                 arenaSize;
    int                  i;
    SECStatus            rv;
    PRArenaPool         *poolp;
    CRMFCertReqMessages *certReqMsgs;

    PORT_Assert (buf != NULL);
    /* Wanna make sure the arena is big enough to store all of the requests
     * coming in.  We'll guestimate according to the length of the buffer.
     */
    arenaSize = len + len/2;
    poolp = PORT_NewArena(arenaSize);
    if (poolp == NULL) {
        return NULL;
    }
    certReqMsgs = PORT_ArenaZNew(poolp, CRMFCertReqMessages);
    if (certReqMsgs == NULL) {
        goto loser;
    }
    certReqMsgs->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, certReqMsgs, CRMFCertReqMessagesTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    for (i=0; certReqMsgs->messages[i] != NULL; i++) {
        /* The sub-routines expect the individual messages to have 
	 * an arena.  We'll give them one temporarily.
	 */
        certReqMsgs->messages[i]->poolp = poolp;
        rv = crmf_decode_process_single_reqmsg(certReqMsgs->messages[i]);
	if (rv != SECSuccess) {
	    goto loser;
	}
        certReqMsgs->messages[i]->poolp = NULL;
    }
    return certReqMsgs;

 loser:
    PORT_FreeArena(poolp, PR_FALSE);
    return NULL;
}
