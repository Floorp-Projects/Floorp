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

/*
 * These are the definitions needed by the library internally to implement
 * CMMF.  Most of these will be helper utilities for manipulating internal
 * data strucures.
 */
#ifndef _CMMFI_H_
#define _CMMFI_H_
#include "cmmfit.h"
#include "crmfi.h"

#define CMMF_MAX_CHALLENGES 10
#define CMMF_MAX_KEY_PAIRS  50

/*
 * Some templates that the code will need to implement CMMF.
 */
extern const SEC_ASN1Template CMMFCertOrEncCertCertificateTemplate[];
extern const SEC_ASN1Template CMMFCertOrEncCertEncryptedCertTemplate[];
extern const SEC_ASN1Template CMMFPOPODecKeyRespContentTemplate[];
extern const SEC_ASN1Template CMMFRandTemplate[];
extern const SEC_ASN1Template CMMFSequenceOfCertsTemplate[];
extern const SEC_ASN1Template CMMFPKIStatusInfoTemplate[];
extern const SEC_ASN1Template CMMFCertifiedKeyPairTemplate[];


/*
 * Some utility functions that are shared by multiple files in this 
 * implementation.
 */

extern SECStatus cmmf_CopyCertResponse (PRArenaPool      *poolp, 
					CMMFCertResponse *dest, 
					CMMFCertResponse *src);

extern SECStatus cmmf_CopyPKIStatusInfo (PRArenaPool       *poolp, 
					 CMMFPKIStatusInfo *dest,
					 CMMFPKIStatusInfo *src);

extern SECStatus cmmf_CopyCertifiedKeyPair(PRArenaPool          *poolp, 
					   CMMFCertifiedKeyPair *dest,
					   CMMFCertifiedKeyPair *src);

extern SECStatus cmmf_DestroyPKIStatusInfo(CMMFPKIStatusInfo *info, 
					   PRBool freeit);

extern SECStatus cmmf_DestroyCertOrEncCert(CMMFCertOrEncCert *certOrEncCert, 
					   PRBool freeit);

extern SECStatus cmmf_PKIStatusInfoSetStatus(CMMFPKIStatusInfo    *statusInfo,
					     PRArenaPool          *poolp,
					     CMMFPKIStatus         inStatus);

extern SECStatus cmmf_ExtractCertsFromList(CERTCertList      *inCertList,
					   PRArenaPool       *poolp,
					   CERTCertificate ***certArray);

extern SECStatus 
       cmmf_CertOrEncCertSetCertificate(CMMFCertOrEncCert *certOrEncCert,
					PRArenaPool       *poolp,
					CERTCertificate   *inCert);

extern CMMFPKIStatus 
       cmmf_PKIStatusInfoGetStatus(CMMFPKIStatusInfo *inStatus);

extern CERTCertList*
       cmmf_MakeCertList(CERTCertificate **inCerts);

extern CERTCertificate*
cmmf_CertOrEncCertGetCertificate(CMMFCertOrEncCert *certOrEncCert,
                                 CERTCertDBHandle  *certdb);

extern SECStatus
cmmf_decode_process_cert_response(PRArenaPool      *poolp, 
				  CERTCertDBHandle *db,
				  CMMFCertResponse *inCertResp);

extern SECStatus
cmmf_decode_process_certified_key_pair(PRArenaPool          *poolp,
				       CERTCertDBHandle     *db,
				       CMMFCertifiedKeyPair *inCertKeyPair);

extern SECStatus
cmmf_user_encode(void *src, CRMFEncoderOutputCallback inCallback, void *inArg,
		 const SEC_ASN1Template *inTemplate);

extern SECStatus
cmmf_copy_secitem (PRArenaPool *poolp, SECItem *dest, SECItem *src);
#endif /*_CMMFI_H_*/





