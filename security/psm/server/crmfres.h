/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#ifndef _CRMFRES_H_
#define _CRMFRES_H_
#include "cmmf.h"
#include "keyres.h"
#include "ssmdefs.h"
#include "ctrlconn.h"

/*
 * FILE: crmfres.h
 * ---------------
 * This file defines the public interface for the class SSMCRMFRequest
 * which is to be used in Cartman to implement all necessary CRMF 
 * functionality.
 */

/*
 * STRUCTURE: SSMCRMFRequest
 * -------------------------
 * This is the structure used to implement the SSMCRMFRequest
 * class.  This class will be used to create CRMF Requests within
 * Cartman.  SSMCRMFRequest sub-classess off of SSMResource so it 
 * must always follow any guidelines for naming and defining presented
 * by the SSMResource class.
 *
 * The fields:
 *
 * super           The mandatory first member for any class sub-classing
 *                 from SSMResource.
 *
 * m_KeyGenType     An enumerated type indicating what type of key pair 
 *                  we are creating a request for.  This value is used 
 *                  to set the appropriate keyUsage extensions and also 
 *                  determining the proper method to set for 
 *                  Proof-Of-Possession.  Look at the file
 *                  ns/security/ssm/lib/protocol/ssmders.h for valid values.
 *                  This value is set by the client.
 *
 * m_KeyPair        A pointer to a key pair associated with the instantiation
 *                  of the CRMF request.  The members mPrivKey and mPubKey
 *                  must be non-NULL values when a new request is created.
 *
 * m_CRMFRequest    This is the member that holds all of the state information
 *                  about the CRMF request.
 *
 * m_Connection     This is the control connection associated with the 
 *                  request.
 */
typedef struct SSMCRMFRequest {
  SSMResource super; /* This must always be the first member of the
                      * structure.  Don't even think of adding another
                      * field before it or removing it.
                      */

  SSMKeyGenType         m_KeyGenType;
  SSMKeyPair           *m_KeyPair;
  CRMFCertRequest      *m_CRMFRequest;
  SSMControlConnection *m_Connection;
} SSMCRMFRequest;

/*
 * STRUCTURE: SSMCRMFRequestArg
 * ----------------------------
 * This structure will be used to pass the initialization information
 * to the SSMCRMFRequest_Init function.
 *
 * keyPair          The key pair that will be associated with the 
 *                  new CRMF request.
 *
 * connection       The control connection to be associated with the
 *                  new CRMF request.  The SSMCRMFRequest object will
 *                  use to get to the certificate and key databases.
 */
typedef struct SSMCRMFRequestArgStr {
  SSMKeyPair           *keyPair;
  SSMControlConnection *connection;
} SSMCRMFRequestArg;

/*
 * FUNCTION:SSM_CreateNewCRMFRequest
 * ---------------------------------
 * INPUTS:
 *    msg
 *        The message from the client requesting a new CRMF request.
 *    connection
 *        The control connection to be associated with the new request.
 *    destID
 *        Pointer to a pre-allocated chunk of memory where the function
 *        will place the ID of the newly created SSMCRMFRequest Object.
 *
 * NOTES:
 * This function takes the message received by the client, interprets
 * the message, creates a new SSMCRMFObject, adds it to the Global Resource
 * table, and places the resource id of the new object at *destID.  To free
 * the returned object, call SSM_FreeResource and pass in the object object 
 * ID placed at *destID by this function.
 *
 * RETURN:
 * The function returns PR_SUCCESS when successful.  Any other return value
 * should be interpreted as an error.
 */
SSMStatus SSM_CreateNewCRMFRequest(SECItem *msg, 
                                  SSMControlConnection *connection,
                                  SSMResourceID *destID);

/*
 * FUNCTION: SSMCRMFRequest_Create
 * -------------------------------
 * INPUTS:
 *    arg
 *        The creation argument for the SSMCRMFRequest class.  This should
 *        be a pointer to SSMKeyPair resource.
 *    res
 *        Pointer to a pre-allocated chunk of memory where the function can
 *        place the value of the pointer to the newly created SSMCRMFRequest
 *        object
 *
 * NOTES:
 * This function serves as the constructor for the SSMCRMFRequest class.  This 
 * function over-rides the function SSMResource_Create which is the super
 * class for SSMCRMFRequest.
 *
 * RETURN:
 * Returns PR_SUCCESS upon successful creation.  Any other error indicates an
 * error.
 */
SSMStatus SSMCRMFRequest_Create(void *arg, SSMControlConnection * conn, 
                               SSMResource **res);

/*
 * FUNCTION: SSMCRMFRequest_Init
 * -----------------------------
 * INPUTS:
 *    inCRMFReq
 *        A pointer to a newly allocated SSMCRMFRequest which needs to be
 *        initialized.  
 *    type
 *        The type for the class that is being initialized.  This value should
 *        always be SSM_RESTYPE_CRMFREQ.  (Unless one day someone decides to
 *        sub-class off of SSMCRMFRequest.)
 *    inKeyPair
 *        The Key Pair to associate with the CRMF request being initialized.
 *    inConnection
 *        The control connection to be associated with the request being 
 *        initialized.
 *
 * NOTES:
 * This function intializes a new SSMCRMFRequest object.  This function will
 * initialize its super-class member and make sure the passed in key pair has
 * non-NULL values for mPrivKey and mPubKey before getting a reference to the
 * passed in key pair.  Finally, the function creates a
 * new NSS based CRMF request and sets the following fields in the request:
 *  1) version is set v3 Cert
 *  2) The Public Key value in the request
 *
 * RETURN:
 * The function returns PR_SUCCESS upon successful initialization.  Any other
 * return value should be interpreted as an error.
 */
SSMStatus SSMCRMFRequest_Init(SSMCRMFRequest       *inCRMFReq, 
			     SSMControlConnection * conn,
                             SSMResourceType       type,
                             SSMKeyPair           *inKeyPair,
                             SSMControlConnection *inConnection);

/*
 * FUNCTION: SSMCRMFRequest_Destroy
 * --------------------------------
 * INPUTS:
 *    inRequest
 *        A pointer to a SSMResource of type SSM_RESTYPE_CRMFREQ to be
 *        destroyed.
 *    doFree
 *        Until the day when someone creates a sub-class of SSMCRMFRequest,
 *        this value should always be PR_TRUE.
 * 
 * NOTES:
 * This function takes of freeing up all memory associated with the
 * SSMCRMFRequest.  First the funciton will release the reference obtained
 * for the member mKeyPair in the function SSMCRMFRequest_Init.  The function
 * then calls CRMF_DestroyCertRequest to free the memory used by mCRMFRequest.
 * Finally, the function calls SSMResource_Destroy to free up the SSMResource
 * member before calling free on the pointer passed in.
 *
 * RETURN:
 * Function returns PR_SUCCESS upon successful destruction of the object. Any
 * other return value should be interpreted as an error.
 */
SSMStatus SSMCRMFRequest_Destroy(SSMResource *inRequest, PRBool doFree);

/*
 * FUNCTION: SSMCRMFRequest_SetAttr
 * --------------------------------
 * INPUTS:
 *    res
 *        A pointer to a SSMResource of type SSM_RESTYPE_CRMFREQ
 *    attrID
 *        The attribute to set in the request.
 *    value
 *        The data to use when setting the desired attribute.
 *
 * NOTES:
 * This function over-rides SSMResource_SetAttr which all classes inherit
 * from the SSMResource class.  This function will set the following 
 * attributes (these attributes are defined in 
 * ns/security/ssm/lib/protocol/rsrcids.h):
 *
 *    SSMAttributeID                SSMResourceAttrType     Value(s)
 *    --------------                -------------------     --------
 *    SSM_FID_CRMFREQ_KEY_TYPE      SSM_NUMERIC_ATTRIBUTE   A pointer to any
 *                                                          enumerated value 
 *                                                          with type of
 *                                                          SSMKeyGenType.
 *
 *    SSM_FID_CRMFREQ_DN            SSM_STRING_ATTRIBUTE    An RFC1485 
 *                                                          formatted DN.
 *    
 *    SSM_FID_REGTOKEN              SSM_STRING_ATTRIBUTE    A string to place
 *                                                          as the Registration
 *                                                          token for the 
 *                                                          request.
 *
 *    SSM_FID_AUTHENTICATOR         SSM_STRING_ATTRIBUTE    A string to place
 *                                                          as the 
 *                                                          Authenticator token
 *                                                          in the request.
 *
 *    SSM_FID_CRMFREQ_ESCROW_AUTHORITY SSM_STRING_ATTRIBUTE A base64 encoded 
 *                                                          DER cert to use for
 *                                                          creating the 
 *                                                          PKIArchiveOptions
 *                                                          control.
 *
 * NOTES:
 * The function return PR_SUCCESS if setting the field with the given data
 * was successful.  Any other return value should be considered an error.
 *    
 */

SSMStatus SSMCRMFRequest_SetAttr(SSMResource         *res,
                                SSMAttributeID      attrID,
                                SSMAttributeValue   *value);

/*
 * FUNCTION: SSMCRMFRequest_SetEscrowAuthority
 * -------------------------------------------
 * INPUTS:
 *    crmfReq
 *        The CRMFRequest resource to add the escrow authority to.
 *    eaCert
 *        The Certificate that belongs to the CA that wants to 
 *        escrow the private key associated with the request.
 * NOTES:
 * This function will wrap the private key in an EncryptedKey type
 * defined by CRMF and include in the CRMF request that is generated.
 *
 * RETURN:
 * PR_SUCCESS indicates the private key associated with the requested
 * was successfully wrapped and made a part of the CRMF request.  Any
 * other return value indicates an error in trying to wrap the private
 * and include it in the CRMF request.
 */
SSMStatus SSMCRMFRequest_SetEscrowAuthority(SSMCRMFRequest  *crmfReq,
                                           CERTCertificate *eaCert);

/*
 * FUNCTION: SSM_EncodeCRMFRequests
 * --------------------------------
 * INPUTS:
 *    msg
 *        The message received from the client requesting CRMF messages to 
 *        be encoded.
 *    destDER
 *        Pointer to a pre-allocated chunk of memory where the function can 
 *        place a pointer to the base64 encoded CRMF CertReqMessages
 *    destLen
 *        Pointer to a pre-allocated piece of memory where the function can
 *        place the length of the string returned in *destDER.
 *
 * NOTES:
 * This function takes a message request from the client to encode CRMF 
 * requests.  The output will be base64 DER-formatted bytes of the type
 * CertReqMessages as defined by the CRMF Internet Draft.  The function 
 * will allocate a buffer in memory to store the DER using PORT_Alloc and
 * place a pointer to the buffer at *destDER.
 *
 * RETURN:
 * The function will return PR_SUCCESS if encoding the requests was successful.
 * Any other return value should be treated as an error and the values at 
 * *destDER and *destLen should be ignored.
 */

SSMStatus SSM_EncodeCRMFRequests(SSMControlConnection * ctrl, SECItem *msg, 
                                char **destDER, SSMPRUint32 *destLen);

/*
 * FUNCTION: SSM_ProcessCMMFCertResponse
 * -------------------------------------
 * INPUTS:
 *    msg
 *        The message received from the client requesting Cartman to 
 *        process a CMMF response.
 *    connection
 *        The control connection associated with the CMMF response.
 *        The connection is necessary so that the function can find
 *        the appropriate cetificate database for placing the decoded
 *        certificates.
 *
 * NOTES:
 * This function serves as the back-end for the JavaScript method
 * crypto.importUserCertificates.  It will decode the base64 DER blob passed
 * in and process it.  Eventually this function will also start the process
 * of backing up the certificate when Cartman supports that feature.  
 * View the document at http://warp/hardcore/library/arch/cert-issue.html
 * to see a detailed explanation for what this function doess.
 * 
 */
SSMStatus SSM_ProcessCMMFCertResponse(SECItem              *msg, 
                                     SSMControlConnection *connection);

/*
 * FUNCTION: SSM_RespondToPOPChallenge
 * -----------------------------------
 * INPUTS:
 *    msg
 *        The message received from the client requesting that Cartman
 *        respond to a challenge.
 *    ctrl
 *        The Control Connection associated with the message to process.
 *    challengeResponse
 *        A pointer to a pre-allocated pointer where the function can place
 *        a copy of the base64 encoded response to the challenge.  The 
 *        response will be a POPODecKeyRespContent defined in the CMMF 
 *        internet draft.
 *    responseLen
 *        A pointer to a pre-allocated PRUint32 where the function can place
 *        the length of the response returned via the challengeResponse input
 *        parameter.
 *
 * NOTES:
 * This function servers as the back-end for the JavaScript method
 * crypto.popChallengeRespone.  The function will decode the base64 DER blob
 * passed in and process it.  The function 
 */
SSMStatus SSM_RespondToPOPChallenge(SECItem                *msg, 
                                   SSMControlConnection   *ctrl,
                                   char                  **challengeResponse, 
                                   PRUint32               *responseLen);

typedef struct SSMCRMFThreadArgStr {
    SSMControlConnection *ctrl;
    SECItem              *msg;
} SSMCRMFThreadArg;

/*
 * FUNCTION: SSM_CRMFEncodeThread
 * ------------------------------
 * INPUTS:
 *    arg
 *        A pointer to a structure of type SSMCRMFThreadArg which the function
 *        will use to encode a CRMF request.
 * NOTES:
 * This function is intended to encode a CRMF request taking a message from
 * the client as its argument along with the associated control thread.  This
 * function will send the reply back to the client in case of success or 
 * failure so the client may proceed with its operations.
 * RETURN:
 * No return value.
 */
void SSM_CRMFEncodeThread(void *arg);
#endif /* _CRMFRES_H_ */

