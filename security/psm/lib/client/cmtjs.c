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
#include "cmtutils.h"
#include "cmtjs.h"
#include "messages.h"

CMTStatus
CMT_GenerateKeyPair(PCMT_CONTROL control, CMUint32 keyGenContext, 
		    CMUint32 mechType, CMTItem *param, CMUint32 keySize, 
		    CMUint32 *keyPairId)
{
    CMTItem message;
    CMTStatus rv;
    KeyPairGenRequest request = {0, 0, 0, {0, NULL, 0}};
    SingleNumMessage reply;

    if (!control) {
        return CMTFailure;
    }

    request.keyGenCtxtID = keyGenContext;
    request.genMechanism = mechType;
    if (param) {
        request.params = *param;
    }
    request.keySize = keySize;

    /* Encode the message */
    if (CMT_EncodeMessage(KeyPairGenRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    message.type = SSM_REQUEST_MESSAGE | SSM_PKCS11_ACTION | SSM_CREATE_KEY_PAIR;

    /* Send the message and get the response */
    rv = CMT_SendMessage(control, &message); 
    if (rv != CMTSuccess) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | SSM_CREATE_KEY_PAIR)) {
        goto loser;
    }

    /* Decode the message */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }
    *keyPairId = reply.value;
    return CMTSuccess;
    
loser:
    *keyPairId = 0;
    return CMTFailure;
}


CMTStatus
CMT_CreateNewCRMFRequest(PCMT_CONTROL control, CMUint32 keyPairID, 
			 SSMKeyGenType keyGenType, CMUint32 *reqID)
{
    CMTItem message;
    CMTStatus rv;
    SingleNumMessage request;
    SingleNumMessage reply;
    
    if (!control) {
        return CMTFailure;
    }

    request.value = keyPairID;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    message.type = SSM_REQUEST_MESSAGE | SSM_CRMF_ACTION | 
                    SSM_CREATE_CRMF_REQ;
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }
  
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | SSM_CREATE_CRMF_REQ)) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *reqID = reply.value;

    rv = CMT_SetNumericAttribute(control, *reqID, SSM_FID_CRMFREQ_KEY_TYPE,
				 keyGenType);
    if (rv != CMTSuccess) {
        goto loser;
    }
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus
CMT_EncodeCRMFRequest(PCMT_CONTROL control, CMUint32 *crmfReqID, 
		      CMUint32 numRequests, char ** der)
{
    CMTItem   message;
    CMTStatus  rv;
    EncodeCRMFReqRequest request;
    SingleItemMessage reply;

    if (!control) {
        return CMTFailure;
    }

    request.numRequests = numRequests;
    request.reqIDs = (long *) crmfReqID;

    /* Encode the request */
    if (CMT_EncodeMessage(EncodeCRMFReqRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    message.type = SSM_REQUEST_MESSAGE | SSM_CRMF_ACTION | SSM_DER_ENCODE_REQ;

    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | SSM_DER_ENCODE_REQ)) {
        goto loser;
    }

    /* XXX Should this be a string? Decode the message */
    if (CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *der = (char *) reply.item.data;
    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus
CMT_ProcessCMMFResponse(PCMT_CONTROL control, char *nickname,
			char *certRepString, CMBool doBackup,
			void *clientContext)
{
    CMTItem   message;
    CMTStatus  rv;
    CMMFCertResponseRequest request;

    if(!control) {
        return CMTFailure;
    }

    request.nickname = nickname;
    request.base64Der = certRepString;
    request.doBackup = doBackup;
    request.clientContext = CMT_CopyPtrToItem(clientContext);

    /* Encode the request */
    if (CMT_EncodeMessage(CMMFCertResponseRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    message.type =  SSM_REQUEST_MESSAGE | SSM_CRMF_ACTION | SSM_PROCESS_CMMF_RESP;

    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | SSM_PROCESS_CMMF_RESP)) {
        goto loser;
    }

    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus
CMT_CreateResource(PCMT_CONTROL control, SSMResourceType resType,
		   CMTItem *params, CMUint32 *rsrcId, CMUint32 *errorCode)
{
    CMTItem message;
    CMTStatus rv;
    CreateResourceRequest request = {0, {0, NULL, 0}};
    CreateResourceReply reply;

    request.type = resType;
    if (params) {
        request.params = *params;
    } 

    /* Encode the request */
    if (CMT_EncodeMessage(CreateResourceRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    message.type = SSM_REQUEST_MESSAGE | SSM_RESOURCE_ACTION | SSM_CREATE_RESOURCE;
    
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | SSM_CREATE_RESOURCE)) {
        goto loser;
    }

    /* Decode the message */
    if (CMT_DecodeMessage(CreateResourceReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *rsrcId = reply.resID;
    *errorCode = reply.result;
    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus CMT_SignText(PCMT_CONTROL control, CMUint32 resID, char* stringToSign, char* hostName, char* caOption, CMInt32 numCAs, char** caNames)
{
    CMTItem message;
    SignTextRequest request;

    
    /* So some basic parameter checking */
    if (!control || !stringToSign) {
        goto loser;
    }

    /* Set up the request */
    request.resID = resID;
    request.stringToSign = stringToSign;
    request.hostName = hostName;
    request.caOption = caOption;
    request.numCAs = numCAs;
    request.caNames = caNames;

    /* Encode the message */
    if (CMT_EncodeMessage(SignTextRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_FORMSIGN_ACTION | SSM_SIGN_TEXT;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_FORMSIGN_ACTION | SSM_SIGN_TEXT)) {
        goto loser;
    }

    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus
CMT_ProcessChallengeResponse(PCMT_CONTROL control, char *challengeString,
			     char **responseString)
{
    CMTItem   message;
    CMTStatus  rv;
    SingleStringMessage request;
    SingleStringMessage reply;

    /* Set the request */
    request.string = challengeString;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleStringMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CRMF_ACTION | SSM_CHALLENGE;

    /* Send the message */
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }
    
    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | SSM_CHALLENGE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleStringMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *responseString = reply.string;
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus
CMT_FinishGeneratingKeys(PCMT_CONTROL control, CMUint32 keyGenContext)
{
    CMTItem message;
    CMTStatus rv;
    SingleNumMessage request;

    /* Set up the request */
    request.value = keyGenContext;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_PKCS11_ACTION | SSM_FINISH_KEY_GEN;

    /* Send the message */
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    /* Validate the reply */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | SSM_FINISH_KEY_GEN)) {
        goto loser;
    }
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus
CMT_GetLocalizedString(PCMT_CONTROL        control,
                       SSMLocalizedString  whichString,
                       char              **localizedString)
{
    CMTItem message;
    CMTStatus rv;
    SingleNumMessage request;
    GetLocalizedTextReply reply;

    /* Set up the request */
    request.value = whichString;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_LOCALIZED_TEXT;

    /* Send the message */
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_LOCALIZED_TEXT)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(GetLocalizedTextReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    if (reply.whichString != whichString) {
        goto loser;
    }
    *localizedString = reply.localizedString;
    return CMTSuccess;
 loser:
    *localizedString = NULL;
    return rv; 
}

CMTStatus
CMT_AddNewModule(PCMT_CONTROL  control,
                 char         *moduleName,
                 char         *libraryPath,
                 unsigned long pubMechFlags,
                 unsigned long pubCipherFlags)
{
    CMTItem message;
    CMTStatus rv;
    AddNewSecurityModuleRequest request;
    SingleNumMessage reply;

    /* Set up the request */
    request.moduleName = moduleName;
    request.libraryPath = libraryPath;
    request.pubMechFlags = pubMechFlags;
    request.pubCipherFlags = pubCipherFlags;

    /* Encode the request */
    if (CMT_EncodeMessage(AddNewSecurityModuleRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_PKCS11_ACTION | SSM_ADD_NEW_MODULE;

    /* Send the message */
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | SSM_ADD_NEW_MODULE)) {
        goto loser;
    }

    /* Decode the response */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    return (CMTStatus) reply.value;
 loser:
    return CMTFailure;
}

CMTStatus
CMT_DeleteModule(PCMT_CONTROL  control,
                 char         *moduleName,
                 int          *moduleType)
{
    CMTItem message;
    CMTStatus rv;
    SingleStringMessage request;
    SingleNumMessage reply;

    /* Set up the request */
    request.string = moduleName;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleStringMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_PKCS11_ACTION | SSM_DEL_MODULE;

    /* Send the message */
    rv = CMT_SendMessage(control, &message);
    if (rv != CMTSuccess) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | SSM_DEL_MODULE)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *moduleType = reply.value;
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus CMT_LogoutAllTokens(PCMT_CONTROL control)
{
  CMTItem message;
  CMTStatus rv;

  message.type = SSM_REQUEST_MESSAGE | SSM_PKCS11_ACTION | SSM_LOGOUT_ALL;
  message.data = NULL;
  message.len  = 0;

  rv = CMT_SendMessage(control, &message);
  if (rv != CMTSuccess) {
    return rv;
  }
  if (message.type !=  (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | 
			SSM_LOGOUT_ALL)) {
    return CMTFailure;
  }
  return CMTSuccess;
}

CMTStatus CMT_GetSSLCapabilities(PCMT_CONTROL control, CMInt32 *capabilites)
{
    SingleNumMessage reply;
    CMTItem message;
    CMTStatus rv;

    message.type = (SSM_REQUEST_MESSAGE | SSM_PKCS11_ACTION | 
                    SSM_ENABLED_CIPHERS);
    message.data = NULL;
    message.len  = 0;

    rv = CMT_SendMessage(control, &message);

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION |
			 SSM_ENABLED_CIPHERS)) {
        goto loser;
    }
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, 
                          &message) != CMTSuccess) {
        goto loser;
    }
    *capabilites = reply.value;
    return CMTSuccess;
 loser:
    return CMTFailure;
}
