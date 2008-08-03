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

#ifndef _CMMFIT_H_
#define _CMMFIT_H_

/*
 * All fields marked by a PKIStausInfo in comments is an integer
 * with the following possible values.
 *
 *  Integer Value          Meaning
 *  -------------          -------
 *         0               granted- got exactly what you asked for.
 *
 *         1               grantedWithMods-got something like what you asked 
 *                          for;requester is responsible for ascertainging the
 *                          differences.
 *
 *         2               rejection-you don't get what you asked for; more 
 *                          information elsewhere in the message
 *
 *         3               waiting-the request body part has not yet been 
 *                          processed, expect to hear more later.
 *
 *         4               revocationWarning-this message contains a warning 
 *                          that a revocation is imminent.
 *
 *         5               revocationNotification-notification that a 
 *                          revocation has occurred.
 *
 *         6               keyUpdateWarning-update already done for the 
 *                          oldCertId specified in FullCertTemplate.
 */

struct CMMFPKIStatusInfoStr {
    SECItem status;
    SECItem statusString;
    SECItem failInfo;
};

struct CMMFCertOrEncCertStr {
    union { 
        CERTCertificate    *certificate;
        CRMFEncryptedValue *encryptedCert;
    } cert;
    CMMFCertOrEncCertChoice choice;
    SECItem                 derValue;
};

struct CMMFCertifiedKeyPairStr {
    CMMFCertOrEncCert   certOrEncCert;
    CRMFEncryptedValue *privateKey;
    SECItem             derPublicationInfo; /* We aren't creating 
					     * PKIPublicationInfo's, so 
					     * we'll store away the der 
					     * here if we decode one that
					     * does have pubInfo.
					     */
    SECItem unwrappedPrivKey;
};

struct CMMFCertResponseStr {
    SECItem               certReqId;
    CMMFPKIStatusInfo     status; /*PKIStatusInfo*/
    CMMFCertifiedKeyPair *certifiedKeyPair;
};

struct CMMFCertRepContentStr {
    CERTCertificate  **caPubs;
    CMMFCertResponse **response;
    PRArenaPool       *poolp;
    PRBool             isDecoded;
};

struct CMMFChallengeStr {
    SECAlgorithmID  *owf;
    SECItem          witness;
    SECItem          senderDER;
    SECItem          key;
    SECItem          challenge;
    SECItem          randomNumber;
};

struct CMMFRandStr {
    SECItem          integer;
    SECItem          senderHash;
    CERTGeneralName *sender;
};

struct CMMFPOPODecKeyChallContentStr {
    CMMFChallenge **challenges;
    PRArenaPool    *poolp;
    int             numChallenges;
    int             numAllocated;
};

struct CMMFPOPODecKeyRespContentStr {
    SECItem     **responses;
    PRArenaPool  *poolp;
};

struct CMMFKeyRecRepContentStr {
    CMMFPKIStatusInfo      status; /* PKIStatusInfo */
    CERTCertificate       *newSigCert;
    CERTCertificate      **caCerts;
    CMMFCertifiedKeyPair **keyPairHist;
    PRArenaPool           *poolp;
    int                    numKeyPairs;
    int                    allocKeyPairs;
    PRBool                 isDecoded;
};

#endif /* _CMMFIT_H_ */

