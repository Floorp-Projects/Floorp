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
#include "secasn1.h"
#include "secitem.h"

SEC_ASN1_MKSUB(SEC_SignedCertificateTemplate)

static const SEC_ASN1Template CMMFSequenceOfCertifiedKeyPairsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, CMMFCertifiedKeyPairTemplate}
};

static const SEC_ASN1Template CMMFKeyRecRepContentTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFKeyRecRepContent)},
    { SEC_ASN1_INLINE, offsetof(CMMFKeyRecRepContent, status), 
      CMMFPKIStatusInfoTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 
		SEC_ASN1_XTRN | 0,
      offsetof(CMMFKeyRecRepContent, newSigCert),
      SEC_ASN1_SUB(SEC_SignedCertificateTemplate)},
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(CMMFKeyRecRepContent, caCerts),
      CMMFSequenceOfCertsTemplate},
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 2,
      offsetof(CMMFKeyRecRepContent, keyPairHist),
      CMMFSequenceOfCertifiedKeyPairsTemplate},
    { 0 }
};

SECStatus
CMMF_EncodeCertRepContent (CMMFCertRepContent        *inCertRepContent,
			   CRMFEncoderOutputCallback  inCallback,
			   void                      *inArg)
{
    return cmmf_user_encode(inCertRepContent, inCallback, inArg,
			    CMMFCertRepContentTemplate);
}

SECStatus
CMMF_EncodePOPODecKeyChallContent(CMMFPOPODecKeyChallContent *inDecKeyChall,
				  CRMFEncoderOutputCallback inCallback,
				  void                     *inArg)
{
    return cmmf_user_encode(inDecKeyChall, inCallback, inArg,
			    CMMFPOPODecKeyChallContentTemplate);
}

CMMFPOPODecKeyRespContent*
CMMF_CreatePOPODecKeyRespContentFromDER(const char *buf, long len)
{
    PRArenaPool               *poolp;
    CMMFPOPODecKeyRespContent *decKeyResp;
    SECStatus                  rv;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    decKeyResp = PORT_ArenaZNew(poolp, CMMFPOPODecKeyRespContent);
    if (decKeyResp == NULL) {
        goto loser;
    }
    decKeyResp->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, decKeyResp, CMMFPOPODecKeyRespContentTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    return decKeyResp;
    
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

SECStatus
CMMF_EncodeKeyRecRepContent(CMMFKeyRecRepContent      *inKeyRecRep,
			    CRMFEncoderOutputCallback  inCallback,
			    void                      *inArg)
{
    return cmmf_user_encode(inKeyRecRep, inCallback, inArg,
			    CMMFKeyRecRepContentTemplate);
}

CMMFKeyRecRepContent* 
CMMF_CreateKeyRecRepContentFromDER(CERTCertDBHandle *db, const char *buf, 
				   long len)
{
    PRArenaPool          *poolp;
    CMMFKeyRecRepContent *keyRecContent;
    SECStatus             rv;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    keyRecContent = PORT_ArenaZNew(poolp, CMMFKeyRecRepContent);
    if (keyRecContent == NULL) {
        goto loser;
    }
    keyRecContent->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, keyRecContent, CMMFKeyRecRepContentTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (keyRecContent->keyPairHist != NULL) {
        while(keyRecContent->keyPairHist[keyRecContent->numKeyPairs] != NULL) {
	    rv = cmmf_decode_process_certified_key_pair(poolp, db,
		       keyRecContent->keyPairHist[keyRecContent->numKeyPairs]);
	    if (rv != SECSuccess) {
	        goto loser;
	    }
	    keyRecContent->numKeyPairs++;
	}
	keyRecContent->allocKeyPairs = keyRecContent->numKeyPairs;
    }
    keyRecContent->isDecoded = PR_TRUE;
    return keyRecContent;
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

