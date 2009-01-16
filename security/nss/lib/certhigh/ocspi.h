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
 * ocspi.h - NSS internal interfaces to OCSP code
 *
 * $Id: ocspi.h,v 1.10 2008/07/08 21:34:32 alexei.volkov.bugs%sun.com Exp $
 */

#ifndef _OCSPI_H_
#define _OCSPI_H_

SECStatus OCSP_InitGlobal(void);
SECStatus OCSP_ShutdownGlobal(void);

ocspResponseData *
ocsp_GetResponseData(CERTOCSPResponse *response, SECItem **tbsResponseDataDER);

ocspSignature *
ocsp_GetResponseSignature(CERTOCSPResponse *response);

PRBool
ocsp_CertIsOCSPDefaultResponder(CERTCertDBHandle *handle, CERTCertificate *cert);

CERTCertificate *
ocsp_GetSignerCertificate(CERTCertDBHandle *handle, ocspResponseData *tbsData,
                          ocspSignature *signature, CERTCertificate *issuer);

SECStatus
ocsp_VerifyResponseSignature(CERTCertificate *signerCert,
                             ocspSignature *signature,
                             SECItem *tbsResponseDataDER,
                             void *pwArg);

CERTOCSPRequest *
cert_CreateSingleCertOCSPRequest(CERTOCSPCertID *certID, 
                                 CERTCertificate *singleCert, 
                                 int64 time, 
                                 PRBool addServiceLocator,
                                 CERTCertificate *signerCert);

SECStatus
ocsp_GetCachedOCSPResponseStatusIfFresh(CERTOCSPCertID *certID, 
                                        int64 time, 
                                        PRBool ignoreOcspFailureMode,
                                        SECStatus *rvOcsp,
                                        SECErrorCodes *missingResponseError);

/*
 * FUNCTION: cert_ProcessOCSPResponse
 *  Same behavior and basic parameters as CERT_GetOCSPStatusForCertID.
 *  In addition it can update the OCSP cache (using information
 *  available internally to this function).
 * INPUTS:
 *  CERTCertDBHandle *handle
 *    certificate DB of the cert that is being checked
 *  CERTOCSPResponse *response
 *    the OCSP response we want to retrieve status from.
 *  CERTOCSPCertID *certID
 *    the ID we want to look for from the response.
 *  CERTCertificate *signerCert
 *    the certificate that was used to sign the OCSP response.
 *    must be obtained via a call to CERT_VerifyOCSPResponseSignature.
 *  int64 time
 *    The time at which we're checking the status for.
 *  PRBool *certIDWasConsumed
 *    In and Out parameter.
 *    If certIDWasConsumed is NULL on input,
 *    this function might produce a deep copy of cert ID
 *    for storing it in the cache.
 *    If out value is true, ownership of parameter certID was
 *    transferred to the OCSP cache.
 *  SECStatus *cacheUpdateStatus
 *    This optional out parameter will contain the result
 *    of the cache update operation (if requested).
 *  RETURN:
 *    The return value is not influenced by the cache operation,
 *    it matches the documentation for CERT_CheckOCSPStatus
 */

SECStatus
cert_ProcessOCSPResponse(CERTCertDBHandle *handle, 
                         CERTOCSPResponse *response, 
                         CERTOCSPCertID   *certID,
                         CERTCertificate  *signerCert,
                         int64             time,
                         PRBool           *certIDWasConsumed,
                         SECStatus        *cacheUpdateStatus);

/*
 * FUNCTION: cert_RememberOCSPProcessingFailure
 *  If an application notices a failure during OCSP processing,
 *  it should finally call this function. The failure will be recorded
 *  in the OCSP cache in order to avoid repetitive failures.
 * INPUTS:
 *  CERTOCSPCertID *certID
 *    the ID that was used for the failed OCSP processing
 *  PRBool *certIDWasConsumed
 *    Out parameter, if set to true, ownership of parameter certID was
 *    transferred to the OCSP cache.
 *  RETURN:
 *    Status of the cache update operation.
 */

SECStatus
cert_RememberOCSPProcessingFailure(CERTOCSPCertID *certID,
                                   PRBool         *certIDWasConsumed);

/*
 * FUNCTION: ocsp_GetResponderLocation
 *  Check ocspx context for user-designated responder URI first. If not
 *  found, checks cert AIA extension.
 * INPUTS:
 *  CERTCertDBHandle *handle
 *    certificate DB of the cert that is being checked
 *  CERTCertificate *cert
 *     The certificate being examined.
 *  PRBool *certIDWasConsumed
 *    Out parameter, if set to true, URI of default responder is
 *    returned.
 *  RETURN:
 *    Responder URI.
 */
char *
ocsp_GetResponderLocation(CERTCertDBHandle *handle,
                          CERTCertificate *cert,
                          PRBool *isDefault);


#endif /* _OCSPI_H_ */
