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
#include "serv.h"
#include "minihttp.h"
#include "signtextres.h"
#include "protocolf.h"
#include "base64.h"
#include "secmime.h"
#include "newproto.h"
#include "messages.h"
#include "certres.h"
#include "sechash.h"

#define SSMRESOURCE(object) (&(object)->super)

SSMStatus
SSM_CreateSignTextRequest(SECItem *msg, SSMControlConnection *conn) 
{
    SSMStatus rv;
    SSMSignTextResource *signTextRes = NULL;
    int i;
    SignTextRequest request;

    /* Decode the sign text protocol message */
    if (CMT_DecodeMessage(SignTextRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* Get the resource */
    rv = SSMControlConnection_GetResource(conn, (SSMResourceID)request.resID,
					  (SSMResource**)&signTextRes);
    if ((rv != PR_SUCCESS) || (signTextRes == NULL)) {
        goto loser;
    }

    /* Get a reference to it */
    SSM_GetResourceReference(SSMRESOURCE(signTextRes));
    if (!SSM_IsAKindOf(SSMRESOURCE(signTextRes), SSM_RESTYPE_SIGNTEXT)) {
        goto loser;
    }

    /* Set the request data */
    signTextRes->m_stringToSign = PL_strdup(request.stringToSign);
    signTextRes->m_hostName = PL_strdup(request.hostName);
    if (PL_strcasecmp(request.caOption, "auto") == 0) {
        signTextRes->m_autoFlag = PR_TRUE;
    } else if (PL_strcasecmp(request.caOption, "ask") == 0) {
        signTextRes->m_autoFlag = PR_FALSE;
    } else {
        goto loser;
    }
    signTextRes->m_numCAs = request.numCAs;
    if (signTextRes->m_numCAs > 0) {
        signTextRes->m_caNames = (char **) PR_CALLOC(sizeof(char*)*(signTextRes->m_numCAs));
        if (!signTextRes->m_caNames) {
            goto loser;
        }
        for (i=0;i<signTextRes->m_numCAs;i++) {
            signTextRes->m_caNames[i] = PL_strdup(request.caNames[i]);
        }
    }
    signTextRes->m_conn = conn;
    signTextRes->m_action = SIGN_TEXT_WAITING;
    signTextRes->m_thread = SSM_CreateAndRegisterThread(PR_USER_THREAD,
                                   SSMSignTextResource_ServiceThread,
                                   (void*)signTextRes,
                                   PR_PRIORITY_NORMAL,
                                   PR_LOCAL_THREAD,
                                   PR_UNJOINABLE_THREAD, 0);
    if (!signTextRes->m_thread) {
        goto loser;
    }
    return PR_SUCCESS;

loser:
    /* XXX Free request */
    return PR_FAILURE;
}

SSMStatus
SSMSignTextResource_Create(void *arg, SSMControlConnection * conn, SSMResource **res)
{
    SSMSignTextResource *signTextRes;
    SSMStatus rv;

    signTextRes = PR_NEWZAP(SSMSignTextResource);
    if (!signTextRes) {
        goto loser;
    }
    rv = SSMSignTextResource_Init(conn, signTextRes, SSM_RESTYPE_SIGNTEXT);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    *res = SSMRESOURCE(signTextRes);
    return PR_SUCCESS;

loser:
    if (signTextRes != NULL) {
        SSM_FreeResource(SSMRESOURCE(signTextRes));
    }
    *res = NULL;
    return PR_FAILURE;
}

SSMStatus
SSMSignTextResource_Init(SSMControlConnection *conn, SSMSignTextResource *in,
                        SSMResourceType type)
{
    SSMStatus rv;

    rv = SSMResource_Init(conn, SSMRESOURCE(in), type);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* Set all our data to zero */
    in->m_stringToSign = NULL;
    in->m_hostName = NULL;
    in->m_autoFlag = PR_FALSE;
    in->m_numCAs = 0;
    in->m_caNames = NULL;
    in->m_conn = conn;
    in->m_thread = NULL;
    in->m_certs = NULL;
    in->m_action = SIGN_TEXT_INIT;
    in->m_nickname = NULL;
    in->m_resultString = NULL;

    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

SSMStatus SSMSignTextResource_Destroy(SSMResource *res, PRBool doFree)
{
    SSMSignTextResource *signTextRes = (SSMSignTextResource*)res;

    if (res == NULL) {
        return PR_FAILURE;
    }

    PR_ASSERT(res->m_threadCount == 0);

    /* Free our data */

    /* Destroy the super class */
    SSMResource_Destroy(res, PR_FALSE);

    /* Free our memory */
    if (doFree) {
        PR_DELETE(signTextRes);
    }

    return PR_SUCCESS;
}

SSMStatus SSMSignTextResource_Shutdown(SSMResource *arg, SSMStatus status)
{
    return PR_SUCCESS;
}

void
SSMSignTextResource_SendTaskCompletedEvent(SSMSignTextResource *res)
{
    SECItem msg;
    SSMStatus rv;
    TaskCompletedEvent event;

    /* Assemble the event. */
    SSM_DEBUG("Sign Text Resource Task completed event: id %ld, status %d\n",
                  SSMRESOURCE(res)->m_id, SSMRESOURCE(res)->m_status);
    event.resourceID = SSMRESOURCE(res)->m_id;
    event.numTasks = 0;
    event.result = SSMRESOURCE(res)->m_status;
    if (CMT_EncodeMessage(TaskCompletedEventTemplate, (CMTItem*)&msg, &event) != CMTSuccess) {
        return;
    }
    if (msg.data) {
        /* Send the event to the control queue. */
        rv = SSM_SendQMessage(SSMRESOURCE(res)->m_connection->m_controlOutQ,
                              SSM_PRIORITY_NORMAL,
                              SSM_EVENT_MESSAGE | SSM_TASK_COMPLETED_EVENT,
                              (int) msg.len, (char *) msg.data, PR_FALSE);
        SSM_DEBUG("Sent message, rv = %d.\n", rv);
    }
}

void
SSMSignTextResource_ServiceThread(void *arg)
{
    SSMSignTextResource *signTextRes = (SSMSignTextResource*)arg;
    SSMStatus rv;
    unsigned char hashVal[SHA1_LENGTH];
    SECItem digest, signedResult, *tmpItem;
    SEC_PKCS7ContentInfo *ci = NULL;
    CERTCertificate *cert = NULL;
    CERTCertListNode *node;
    int i,numCerts;

    SSM_LockResource(SSMRESOURCE(signTextRes));

    PR_ASSERT(signTextRes);
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(signTextRes), SSM_RESTYPE_SIGNTEXT));

    memset(&signedResult, 0, sizeof(SECItem));

    /* Get the certificates */
    signTextRes->m_certs = CERT_MatchUserCert(signTextRes->m_conn->m_certdb, 
                                certUsageEmailSigner,
                                signTextRes->m_numCAs,
                                signTextRes->m_caNames,
                                signTextRes->m_conn);
    numCerts = SSM_CertListCount(signTextRes->m_certs);
    if((signTextRes->m_certs == NULL) || (numCerts == 0)) {
        signTextRes->m_resultString = strdup("error:noMatchingCert");
        goto loser;
    }

    /* Send UI event to client */
    if (signTextRes->m_autoFlag == PR_TRUE) {
        rv = SSMControlConnection_SendUIEvent(signTextRes->m_conn, "get", 
					      "signtext_auto", 
					      SSMRESOURCE(signTextRes), NULL,
					      &SSMRESOURCE(signTextRes)->m_clientContext,
					      PR_TRUE);
    } else {
        rv = SSMControlConnection_SendUIEvent(signTextRes->m_conn, "get", "signtext_ask", SSMRESOURCE(signTextRes), NULL,
            &SSMRESOURCE(signTextRes)->m_clientContext, PR_TRUE);
    }
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* Here we wait for the user to confirm or deny the signing */
    SSM_WaitResource(SSMRESOURCE(signTextRes), PR_INTERVAL_NO_TIMEOUT);

    switch (SSMRESOURCE(signTextRes)->m_buttonType) {
    case SSM_BUTTON_OK:
        /* Get the signing cert */
    	node = (CERTCertListNode *)PR_LIST_HEAD(&signTextRes->m_certs->list);
    	if (signTextRes->m_nickname == NULL ) {
	        /* this is the "auto" case */
    	    cert = node->cert;
    	} else {
	        /* this is the "ask" case */
    	    for ( i = 0; i < numCerts; i++ ) {
	        	if ( PL_strcasecmp(signTextRes->m_nickname, node->cert->nickname) == 0 ) {
		            cert = node->cert;
		            break;
		        }
		        node = (struct CERTCertListNodeStr *) node->links.next;
	        }
        }

        /* Hash the string to be signed */
        rv = HASH_HashBuf(HASH_AlgSHA1, hashVal,
                          (unsigned char *) signTextRes->m_stringToSign, 
                          strlen(signTextRes->m_stringToSign));
        if (rv != SECSuccess) {
            goto loser;
        }
        digest.len = SHA1_LENGTH;
        digest.data = hashVal;

        /* Sign the hash */
        ci = SECMIME_CreateSigned(cert, cert, cert->dbhandle, SEC_OID_SHA1,
                                    &digest, NULL, signTextRes->m_conn);
        if (ci == NULL) {
            goto loser;
        }

        /* Create the PKCS#7 object */
        tmpItem = SEC_PKCS7EncodeItem(NULL, &signedResult, ci, NULL, NULL,
                                      signTextRes->m_conn);
        if (tmpItem != &signedResult) {
            goto loser;
        }

        /* Convert the result to base 64 */
        signTextRes->m_resultString = BTOA_DataToAscii(signedResult.data, signedResult.len);
        if (signTextRes->m_resultString == NULL) {
            goto loser;
        }
        break;
    case SSM_BUTTON_CANCEL:
        signTextRes->m_resultString= PL_strdup("error:userCancel");
        break;
    default:
        break;
    }
    SSM_UnlockResource(SSMRESOURCE(signTextRes));
loser:
    /* Free data */
    if (signTextRes->m_stringToSign) {
        PR_Free(signTextRes->m_stringToSign);
    }
    if (signTextRes->m_hostName) {
        PR_Free(signTextRes->m_hostName);
    }
    if (signTextRes->m_certs) {
        CERT_DestroyCertList(signTextRes->m_certs);
    }

    /* Send the task complete event here */
    SSMSignTextResource_SendTaskCompletedEvent(signTextRes);
    return;
}

SSMStatus 
SSMSignTextResource_Print(SSMResource   *res,
			  char *fmt,
                          PRIntn numParam,
			  char ** value,
			  char **resultStr)
{
    char *hostName = NULL, *stringToSign = NULL;
    SSMSignTextResource *signTextRes = (SSMSignTextResource*)res;

    hostName = signTextRes->m_hostName;
    stringToSign = signTextRes->m_stringToSign;
    if (numParam) 
      SSMResource_Print(res, fmt, numParam, value, resultStr);
    else if (signTextRes->m_autoFlag) {
      CERTCertListNode *node;
      CERTCertificate *cert;
      
      /* Get the cert nickname */
      node = (CERTCertListNode*)PR_LIST_HEAD(&signTextRes->m_certs->list);
      cert = node->cert;
      
      /* Get the nickname */
      *resultStr = PR_smprintf(fmt, signTextRes->super.m_id, hostName,
			       stringToSign, cert->nickname);
    } else {
      *resultStr = PR_smprintf(fmt, signTextRes->super.m_id, hostName,
			       stringToSign);
      
    }
    return (*resultStr != NULL) ? PR_SUCCESS : PR_FAILURE;
}

SSMStatus
SSMSignTextResource_GetAttr(SSMResource *res, SSMAttributeID attrID,
			    SSMResourceAttrType attrType,
			    SSMAttributeValue *value)
{
    SSMSignTextResource *signTextRes = (SSMSignTextResource*)res;

    if (!signTextRes || !signTextRes->m_resultString) {
        goto loser;
    }

    switch (attrID) {
        case SSM_FID_SIGNTEXT_RESULT:
            if (!signTextRes->m_resultString) {
                goto loser;
            }
            value->type = SSM_STRING_ATTRIBUTE;
            value->u.string.len = PL_strlen(signTextRes->m_resultString);
            value->u.string.data = (unsigned char *) PL_strdup(signTextRes->m_resultString);
            break;
        default:
            goto loser;
    }

    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

SSMStatus
SSMSignTextResource_FormSubmitHandler(SSMResource* res,
                                      HTTPRequest* req)
{
    SSMStatus rv;
    char* tmpStr;
    SSMSignTextResource *signTextRes = (SSMSignTextResource*)res;

    SSM_LockResource(res);

    rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
    if (rv != SSM_SUCCESS ||
        PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
        goto loser;
    }

    /* Detect which button was pressed */
    if (res->m_buttonType == SSM_BUTTON_OK) {
        /* Get the certificate nickname */
        SSM_HTTPParamValue(req, "chose", &signTextRes->m_nickname);
    }

loser:
    /* Close the window */
    SSM_HTTPCloseAndSleep(req);

    /* Unblock the sign text thread waiting on this */
    SSM_NotifyResource(res);
    SSM_UnlockResource(res);

    return rv;
}

SSMStatus SSM_SignTextCertListKeywordHandler(SSMTextGenContext* cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMResource* target = NULL;
    SSMSignTextResource* res;
    char* prefix = NULL;
    char* wrapper = NULL;
    char* suffix = NULL;
    char* tmpStr = NULL;
    char* fmt = NULL;
    const PRIntn CERT_LIST_PREFIX = (PRIntn)0;
    const PRIntn CERT_LIST_WRAPPER = (PRIntn)1;
    const PRIntn CERT_LIST_SUFFIX = (PRIntn)2;
    const PRIntn CERT_LIST_PARAM_COUNT = (PRIntn)3;

    /* 
     * make sure cx, cx->m_request, cx->m_params, cx->m_result are not
     * NULL
     */
    PR_ASSERT(cx != NULL && cx->m_request != NULL && cx->m_params != NULL &&
	      cx->m_result != NULL);
    if (cx == NULL || cx->m_request == NULL || cx->m_params == NULL ||
	    cx->m_result == NULL) {
    	PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    	goto loser;
    }

    if (SSM_Count(cx->m_params) != CERT_LIST_PARAM_COUNT) {
        SSM_HTTPReportSpecificError(cx->m_request, "_signtext_certList: ",
				    "Incorrect number of parameters "
				    " (%d supplied, %d needed).\n",
				    SSM_Count(cx->m_params), 
				    CERT_LIST_PARAM_COUNT);
	    goto loser;
    }

    /* get the connection object */
    target = SSMTextGen_GetTargetObject(cx);
    PR_ASSERT(target != NULL);
    res = (SSMSignTextResource*)target;

    /* form the MessageFormat object */
    /* find arguments (prefix, wrapper, and suffix) */
    prefix = (char *) SSM_At(cx->m_params, CERT_LIST_PREFIX);
    wrapper = (char *) SSM_At(cx->m_params, CERT_LIST_WRAPPER);
    suffix = (char *) SSM_At(cx->m_params, CERT_LIST_SUFFIX);
    PR_ASSERT(prefix != NULL && wrapper != NULL && suffix != NULL);

    /* grab the prefix and expand it */
    rv = SSM_GetAndExpandTextKeyedByString(cx, prefix, &tmpStr);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }

    /* append the prefix */
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
    SSMTextGen_UTF8StringClear(&tmpStr);
    SSM_DebugUTF8String("signtext cert list prefix", cx->m_result);

    /* grab the wrapper */
    rv = SSM_GetAndExpandTextKeyedByString(cx, wrapper, &tmpStr);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    SSM_DebugUTF8String("sign text cert list wrapper", fmt);

    /* form the wrapped cert list UnicodeString */
    rv = ssm_signtext_get_unicode_cert_list(res, tmpStr, &tmpStr);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    /* append the wrapped cert list */
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    SSMTextGen_UTF8StringClear(&tmpStr);

    /* grab the suffix and expand it */
    rv = SSM_GetAndExpandTextKeyedByString(cx, suffix, &tmpStr);
    if (rv != SSM_SUCCESS) {
	goto loser;
    }
    SSM_DebugUTF8String("client cert list suffix", tmpStr);

    /* append the suffix */
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tmpStr);
    if (rv != SSM_SUCCESS) {
      goto loser;
    }
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
	rv = SSM_FAILURE;
    }
done:
    PR_FREEIF(fmt);
    PR_FREEIF(tmpStr);
    return rv;
}

/*
 * Function: SSMStatus ssm_client_auth_get_unicode_cert_list()
 * Purpose: forms the cert list UnicodeString
 *
 * Arguments and return values:
 * - conn: SSL connection object
 * - fmt: cert wrapper UnicodeString format
 * - result: resulting UnicodeString
 *
 * Note: if we include the expired certs, we need to append to the end as
 *       well
 */
SSMStatus ssm_signtext_get_unicode_cert_list(SSMSignTextResource* res, 
					     char* fmt, 
					     char** result)
{
    SSMStatus rv = SSM_SUCCESS;
    char * tmpStr = NULL;
    char * finalStr = NULL;
    int i;
    CERTCertListNode *head = NULL;
    int numCerts;

    PR_ASSERT(res != NULL && fmt != NULL && result != NULL);
    /* in case we fail */
    *result = NULL;

    /* concatenate the string using the nicknames */
    head = (CERTCertListNode *)PR_LIST_HEAD(&res->m_certs->list);
    numCerts = SSM_CertListCount(res->m_certs);
    for (i = 0; i < numCerts; i++) {
        tmpStr = PR_smprintf(fmt, i, head->cert->nickname);
	rv = SSM_ConcatenateUTF8String(&finalStr, tmpStr);
	if (rv != SSM_SUCCESS) {
	  goto loser;
	}
	PR_Free(tmpStr);
        tmpStr = NULL;
        head = (CERTCertListNode *)head->links.next;
    }

    SSM_DebugUTF8String("client auth: final cert list", finalStr);
    *result = finalStr;
    return SSM_SUCCESS;
loser:
    SSM_DEBUG("client auth: wrapping cert list failed.\n");
    if (rv == SSM_SUCCESS) {
        rv = SSM_FAILURE;
    }
    PR_FREEIF(finalStr);
    PR_FREEIF(tmpStr);
    return rv;
}
