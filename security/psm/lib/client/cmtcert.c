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
#include "cmtcmn.h"
#if defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#ifdef XP_MAC
#include "macsocket.h"
#include "cmtmac.h"
#else
#include <windows.h>
#include <winsock.h>
#endif
#endif
#include <errno.h>
#include "cmtutils.h"
#include "messages.h"
#include <string.h>
#include "cmtjs.h"

CMUint32 CMT_DecodeAndCreateTempCert(PCMT_CONTROL control, char * data, 
				      CMUint32 len, int type) 
{
  CMTItem message;
  DecodeAndCreateTempCertRequest request;
  SingleNumMessage reply;

  /* Set up the request */
  request.type = type;
  request.cert.len = len;
  request.cert.data = (unsigned char *) data;

  /* Encode the request */
  if (CMT_EncodeMessage(DecodeAndCreateTempCertRequestTemplate, &message, &request) != CMTSuccess) {
      goto loser;
  }

  /* Set the message request type */
  message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_DECODE_TEMP_CERT;

  /* Send the message and get the response */
  if (CMT_SendMessage(control, &message) == CMTFailure) {
    goto loser;
  }

  /* Validate the message reply type */
  if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_DECODE_TEMP_CERT)) {
    goto loser;
  }

  /* Decode the reply */
  if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
      goto loser;
  }

  /* Return the cert id */
  return reply.value;
loser:
  return 0;
}

void CMT_DestroyCertificate(PCMT_CONTROL control, CMUint32 certID)
{
  CMTItem message;
  SingleNumMessage request;

  /* Set up the request */
  request.value = certID;

  /* Encode the request */
  if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) != CMTSuccess) {
      goto loser;
  }

  /* Set the message request type */
  message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_DESTROY_CERT;

  /* Send the message and get the response */
  if (CMT_SendMessage(control, &message) == CMTFailure) {
    goto loser;
  }

  /* Validate the message reply type */
  if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_DESTROY_CERT)) {
    goto loser;
  }

loser:
  /* do something on fail ? */
  return;
}

char * cmt_processreplytooldkeygen(CMTItem *message,
                                   CMKeyGenTagArg * arg,
                                   CMKeyGenTagReq *next)
{
  char * keystring = NULL;
  SingleStringMessage keyreply;
  GenKeyOldStyleTokenRequest tokenrequest;
  NameList * tokens = NULL;
  GenKeyOldStylePasswordRequest pwdrequest;
  CMKeyGenPassword * pwdstruct = NULL;
  int i;

  /* check what kind of response we got */
  switch (message->type) {
  case (SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_KEYGEN_DONE):
      /* Decode the reply */
      if (CMT_DecodeMessage(SingleStringMessageTemplate, &keyreply, message) 
          != CMTSuccess) 
          goto loser;
      keystring = strdup(keyreply.string);
      *next = CM_KEYGEN_DONE;
      break;
  case (SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_KEYGEN_TOKEN):
      /* Decode the reply */
      if (CMT_DecodeMessage(GenKeyOldStyleTokenRequestTemplate, &tokenrequest,
                            message) != CMTSuccess) 
          goto loser;
      tokens = (NameList *) malloc(sizeof(NameList));
      tokens->numitems = tokenrequest.numtokens;
      tokens->names = (char **) calloc(tokenrequest.numtokens, sizeof(char *));
      for (i = 0; i<tokenrequest.numtokens; i++)
          tokens->names[i] = strdup(tokenrequest.tokenNames[i]);
      arg->rid = tokenrequest.rid;
      arg->current = tokens;
      *next = CM_KEYGEN_PICK_TOKEN ;
      break;
  case (SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_KEYGEN_PASSWORD):
       if (CMT_DecodeMessage(GenKeyOldStylePasswordRequestTemplate, 
                             &pwdrequest,message) != CMTSuccess) 
           goto loser;
       arg->rid = pwdrequest.rid;
       pwdstruct = (CMKeyGenPassword *) malloc(sizeof(CMKeyGenPassword));
       pwdstruct->password = NULL;
       pwdstruct->minpwd = pwdrequest.minpwdlen;
       pwdstruct->maxpwd = pwdrequest.maxpwdlen;
       pwdstruct->internalToken = pwdrequest.internal;
       arg->current = pwdstruct;
       *next = CM_KEYGEN_SET_PASSWORD;
       break;
  default:
      /* error or bad message type */
      *next = CM_KEYGEN_ERR;
      break;
  }
  return keystring;
 loser:
  if (keystring)
      free (keystring);
  return NULL;
}

char * CMT_GetGenKeyResponse(PCMT_CONTROL control, CMKeyGenTagArg * arg, 
                             CMKeyGenTagReq *next)
{
    CMTItem message;
    char *keystring = NULL;
    CMTSocket sock;

    memset(&message, 0, sizeof(CMTItem));
    CMT_LOCK(control->mutex);
    sock = control->sock;
    if (control->sockFuncs.select(&sock, 1, 1) == sock) {
        /* Make sure there's a message waiting to be read so 
         * that we avoid deadlock
         */
        CMT_ReadMessageDispatchEvents(control, &message);
        keystring = cmt_processreplytooldkeygen(&message, arg, next);
    }
    CMT_UNLOCK(control->mutex);
    return keystring;
}

char * CMT_GenKeyOldStyle(PCMT_CONTROL control, CMKeyGenTagArg * arg, 
                          CMKeyGenTagReq *next)
{
  char * keystring = NULL;
  CMTItem message;
  GenKeyOldStyleRequest request;
  GenKeyOldStyleTokenReply tokenreply;
  GenKeyOldStylePasswordReply passwordreply;
  
  if (!arg || !next) 
      goto loser;
  
  /* Set up appropriate request */
  switch (arg->op) {
  case CM_KEYGEN_START: 
      {
          CMKeyGenParams * params = (CMKeyGenParams *) arg->current;
          request.choiceString = params->choiceString;
          request.challenge = params->challenge;
          request.typeString = params->typeString;
          request.pqgString = params->pqgString;
          if (CMT_EncodeMessage(GenKeyOldStyleRequestTemplate, &message, 
                                &request) != CMTSuccess) 
              goto loser;
          message.type = (SSM_REQUEST_MESSAGE | SSM_KEYGEN_TAG | 
                          SSM_KEYGEN_START);
       
      }
  break;
  case CM_KEYGEN_PICK_TOKEN:
      tokenreply.rid = arg->rid;
      tokenreply.cancel = (CMBool) arg->cancel;
      if (!arg->cancel)
          tokenreply.tokenName = arg->tokenName;
      /* Encode the request */
      if (CMT_EncodeMessage(GenKeyOldStyleTokenReplyTemplate, &message, 
                            &tokenreply) != CMTSuccess) 
          goto loser;  
      message.type = (SSM_REQUEST_MESSAGE | SSM_KEYGEN_TAG |SSM_KEYGEN_TOKEN);
      break;
  case CM_KEYGEN_SET_PASSWORD:
      passwordreply.rid = arg->rid;
      passwordreply.cancel = (CMBool) arg->cancel;
      if (!arg->cancel)
          passwordreply.password = ((CMKeyGenPassword*)arg->current)->password;
      /* Encode the request */
    if (CMT_EncodeMessage(GenKeyOldStylePasswordReplyTemplate, &message, 
                          &passwordreply) != CMTSuccess) 
        goto loser;  
    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE |SSM_KEYGEN_TAG |SSM_KEYGEN_PASSWORD;
    break;
  default:
      /* don't know what to do - bad argument? */
      goto loser;
  }
      
  /* Send the message */
  if (CMT_SendMessage(control, &message) == CMTFailure) {
    goto loser;
  }
  if (arg->current)
      free(arg->current);
  arg->current = NULL;
  
  keystring = cmt_processreplytooldkeygen(&message, arg, next);

loser:
  return keystring;
}


char ** CMT_GetKeyChoiceList(PCMT_CONTROL control, char * type, char * pqgString)
{
  CMTItem message;
  int i;
  char **result = NULL;
  GetKeyChoiceListRequest request;
  GetKeyChoiceListReply reply;

  /* Set up the request */
  request.type = type;
  request.pqgString = pqgString;

  /* Encode the message */
  if (CMT_EncodeMessage(GetKeyChoiceListRequestTemplate, &message, &request) != CMTSuccess) {
      goto loser;
  }
  
  /* Set the message request type */
  message.type = SSM_REQUEST_MESSAGE | SSM_KEYGEN_TAG | SSM_GET_KEY_CHOICE;

  /* Send the message */
  if (CMT_SendMessage(control, &message) == CMTFailure) {
     goto loser;
  }

  /* Validate the message response type */
  if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_GET_KEY_CHOICE)) {
      goto loser;
  }

  /* Decode the reply */
  if (CMT_DecodeMessage(GetKeyChoiceListReplyTemplate, &reply, &message) != CMTSuccess) {
      goto loser;
  }

  result = (char **) calloc(reply.nchoices+1, sizeof(char *));
  if (!result) {
      goto loser;
  }

  for (i = 0; i<reply.nchoices; i++) {
      result[i] = reply.choices[i];
  }
  result[i] = 0;
loser:
  return result;
}
 

CMTStatus CMT_ImportCertificate(PCMT_CONTROL control, CMTItem * cert, CMUint32 * certResourceID)
{
    CMTItem message;
    SingleItemMessage request;
    ImportCertReply reply;

    /* Do some parameter checking */
    if (!control || !cert || !certResourceID) {
        goto loser;
    }

    /* Set up the request */
    request.item = *cert;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleItemMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_IMPORT_CERT;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the reply */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_IMPORT_CERT)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(ImportCertReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Success */
    if (reply.result == 0) {
        *certResourceID = reply.resID;
        return CMTSuccess;
    }

loser:
    *certResourceID = 0;
    return CMTFailure;
}

CMUint32 CMT_DecodeCertFromPackage(PCMT_CONTROL  control, 
				 char * certbuf, int certlen)
{
    CMTItem message;
    SingleItemMessage request;
    SingleNumMessage reply;

    /* check parameters */
    if (!control || !certbuf || certlen == 0) {
        goto loser;
    }

    /* Set up the request */
    request.item.data = (unsigned char *) certbuf;
    request.item.len = certlen;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleItemMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_DECODE_CERT;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_DECODE_CERT)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Return cert id */
    return reply.value;
loser:
    return 0;
}

CMTStatus CMT_VerifyCertificate(PCMT_CONTROL control, CMUint32 certResourceID, CMUint32 certUsage, CMInt32 * result)
{
    CMTItem message;
    VerifyCertRequest request;
    SingleNumMessage reply;

    /* Do some parameter checking */
    if (!control || !result) {
        goto loser;
    }

    /* Set the request */
    request.resID = certResourceID;
    request.certUsage = certUsage;

    /* Encode the request */
    if (CMT_EncodeMessage(VerifyCertRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_VERIFY_CERT;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_VERIFY_CERT)) {
        goto loser;
    }
    
    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }
    *result = reply.value;

    if (*result == 0) {
        return CMTSuccess;
    }
loser:
    return CMTFailure;
}

CMTStatus CMT_FindCertificateByNickname(PCMT_CONTROL control, char * nickname, CMUint32 *resID)
{
    CMTItem message;
    SingleStringMessage request;
    SingleNumMessage reply;

    /* Do some basic parameter checking */
    if (!control || !nickname) {
        goto loser;
    }

    /* Set the request */
    request.string = nickname;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleStringMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_FIND_BY_NICKNAME;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_FIND_BY_NICKNAME)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *resID = reply.value;
    return CMTSuccess;
loser:
    *resID = 0;
    return CMTFailure;
}

CMTStatus CMT_FindCertificateByKey(PCMT_CONTROL control, CMTItem *key, CMUint32 *resID)
{
    CMTItem message;
    SingleItemMessage request;
    SingleNumMessage reply;

    /* Do some basic parameter checking */
    if (!control || !key || !resID) {
        goto loser;
    }

    /* Set up the request */
    request.item = *key;

    /* Encode the request */
    if (CMT_EncodeMessage(SingleItemMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_FIND_BY_KEY;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_FIND_BY_KEY)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }
    *resID = reply.value;
    return CMTSuccess;
loser:
    *resID = 0;
    return CMTFailure;
}

CMTStatus CMT_FindCertificateByEmailAddr(PCMT_CONTROL control, char * emailAddr, CMUint32 *resID)
{
    CMTItem message;
    SingleStringMessage request;
    SingleNumMessage reply;

    /* Do some basic parameter checking */
    if (!control || !emailAddr) {
        goto loser;
    }

    /* Set up the request */
    request.string = emailAddr;

    /* Encode the message */
    if (CMT_EncodeMessage(SingleStringMessageTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_FIND_BY_EMAILADDR;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_FIND_BY_EMAILADDR)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    *resID = reply.value;
    return CMTSuccess;

loser:
    *resID = 0;
    return CMTFailure;
}

CMTStatus CMT_AddCertificateToDB(PCMT_CONTROL control, CMUint32 resID, char *nickname, CMInt32 ssl, CMInt32 email, CMInt32 objectSigning)
{
    CMTItem message;
    AddTempCertToDBRequest request;

    /* Do some basic parameter checking */
    if (!control || !nickname) {
        goto loser;
    }

    /* Set up the request */
    request.resID = resID;
    request.nickname = nickname;
    request.sslFlags = ssl;
    request.emailFlags = email;
    request.objSignFlags = objectSigning;

    /* Encode the request */
    if (CMT_EncodeMessage(AddTempCertToDBRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_ADD_TO_DB;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_ADD_TO_DB)) {
        goto loser;
    }
    return CMTSuccess;
loser:
    return CMTFailure;
}

CMT_CERT_LIST *CMT_MatchUserCert(PCMT_CONTROL control, CMInt32 certUsage, CMInt32 numCANames, char **caNames)
{
    CMTItem message;
    CMT_CERT_LIST *certList;
    int i;
    MatchUserCertRequest request;
    MatchUserCertReply reply;

    /* Set up the request */
    request.certType = certUsage;
    request.numCANames = numCANames;
    request.caNames = caNames;

    /* Encode the request */
    if (CMT_EncodeMessage(MatchUserCertRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

    /* Set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_MATCH_USER_CERT;

    /* Send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* Validate the message reply type */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_MATCH_USER_CERT)) {
        goto loser;
    }

    /* Decode the reply */
    if (CMT_DecodeMessage(MatchUserCertReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

    /* Return a list of cert ids to the client */
    certList = (CMT_CERT_LIST*)malloc(sizeof(CMT_CERT_LIST));
    if (!certList) {
        goto loser;
    }

    CMT_INIT_CLIST(&certList->certs);
    certList->count = reply.numCerts;
    for (i=0; i<reply.numCerts; i++) {
        CMT_CERT_LIST_ELEMENT *cert;
        cert = (CMT_CERT_LIST_ELEMENT*)malloc(sizeof(CMT_CERT_LIST_ELEMENT));
        if (!cert) {
            goto loser;
        }
        CMT_INIT_CLIST(&cert->links);
        cert->certResID = reply.certs[i];
        CMT_APPEND_LINK(&cert->links, &certList->certs);
    }

    /* Clean up */
    return certList;
loser:
    CMT_DestroyCertList(certList);
    return NULL;
}

void CMT_DestroyCertList(CMT_CERT_LIST *certList)
{
    /* XXX */
    return;
}


CMTStatus CMT_CompareForRedirect(PCMT_CONTROL control, CMTItem *status1, 
                                 CMTItem *status2, CMUint32 *res)
{
    RedirectCompareRequest request;
    CMTItem message = { 0, NULL, 0 };
    SingleNumMessage reply;

    if (status1 == NULL || status2 == NULL || res == NULL) {
        return CMTFailure;
    }

    request.socketStatus1Data.len  = status1->len;
    request.socketStatus1Data.data = status1->data;
    request.socketStatus2Data.len  = status2->len;
    request.socketStatus2Data.data = status2->data;
    
    if (CMT_EncodeMessage(RedirectCompareRequestTemplate, &message, &request)
        != CMTSuccess) {
        goto loser;
    }
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | 
                   SSM_REDIRECT_COMPARE;
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message)
        != CMTSuccess) {
        goto loser;
    }
    *res = reply.value;
    free (message.data);
    return CMTSuccess;
 loser:
    *res = 0;
    if (message.data != NULL) {
        free (message.data);
    }
    return CMTFailure;
}

CMTStatus
CMT_DecodeAndAddCRL(PCMT_CONTROL control, unsigned char *derCrl,
		    CMUint32 len, char *url, int type, 
                    char **errMess)
{
    DecodeAndAddCRLRequest request;
    SingleNumMessage reply;
    CMTItem message = { 0, NULL, 0 };

    if (*errMess) *errMess = NULL;
    request.derCrl.data = derCrl;
    request.derCrl.len  = len;
    request.type        = type;
    request.url         = url;
    if (CMT_EncodeMessage(DecodeAndAddCRLRequestTemplate, &message, &request)
	!= CMTSuccess) {
        goto loser;
    }
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION |
                   SSM_DECODE_CRL;

    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }
    
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) 
	!= CMTSuccess) {
        goto loser;
    }
    if (reply.value == 0) {
        return CMTSuccess;
    }
    if (*errMess) {
        if (CMT_GetLocalizedString(control, (SSMLocalizedString) reply.value, errMess) 
	    != CMTSuccess) {
	    *errMess = NULL;
	} 
    }
 loser:
    return CMTFailure;
}


/* These functions are used by requests related with javascript
 * "SecurityConfig".
 */
/* adds base64 encoded cert to the temp db and gets a lookup key */
CMTItem* CMT_SCAddCertToTempDB(PCMT_CONTROL control, char* certStr,
                               CMUint32 certLen)
{
    SingleItemMessage request;
    SingleItemMessage reply;
    CMTItem message;
    CMTItem* certKey = NULL;

    if ((certStr == NULL) || (certLen == 0)) {
        goto loser;
    }

    /* pack the request */
    request.item.len = certLen;
    request.item.data = (unsigned char *) certStr;

    /* encode the request */
    if (CMT_EncodeMessage(SingleItemMessageTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }

    /* set the message type */
    message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_ADD_CERT_TO_TEMP_DB;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    /* decode the reply */
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_ADD_CERT_TO_TEMP_DB)) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    certKey = (CMTItem*)malloc(sizeof(CMTItem));
    if (certKey == NULL) {
        goto loser;
    }
    certKey->len = reply.item.len;
    certKey->data = reply.item.data;

 loser:
    return certKey;
}

/* adds a cert keyed by certKey to the perm DB w/ trustStr info */
CMTStatus CMT_SCAddTempCertToPermDB(PCMT_CONTROL control, CMTItem* certKey,
                                    char* trustStr, char* nickname)
{
    SCAddTempCertToPermDBRequest request;
    CMTItem message = {0, NULL, 0};
    SingleNumMessage reply;

    if ((certKey == NULL) || (trustStr == NULL)) {
        return CMTFailure;
    }

    request.certKey.len = certKey->len;
    request.certKey.data = certKey->data;
    request.trustStr = trustStr;
    request.nickname = nickname;

    if (CMT_EncodeMessage(SCAddTempCertToPermDBRequestTemplate, &message,
                          &request) != CMTSuccess) {
        goto loser;
    }
    message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_ADD_TEMP_CERT_TO_DB;
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    if (reply.value == 0) {
        return CMTSuccess;
    }
loser:
    return CMTFailure;
}

/* deletes a cert (or certs) keyed by certKey from the database */
CMTStatus CMT_SCDeletePermCerts(PCMT_CONTROL control, CMTItem* certKey,
                                CMBool deleteAll)
{
    SCDeletePermCertsRequest request;
    CMTItem message = {0, NULL, 0};
    SingleNumMessage reply;

    if (certKey == NULL) {
        return CMTFailure;
    }

    request.certKey.len = certKey->len;
    request.certKey.data = certKey->data;
    request.deleteAll = deleteAll;

    if (CMT_EncodeMessage(SCDeletePermCertsRequestTemplate, &message,
                          &request) != CMTSuccess) {
        goto loser;
    }
    message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_DELETE_PERM_CERTS;
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    if (reply.value == 0) {
        return CMTSuccess;
    }
loser:
    return CMTFailure;
}

static CMTItem* CMT_SCFindCertKey(PCMT_CONTROL control, 
                                  SSMSecCfgFindByType subtype, char* name)
{
    CMTItem* certKey = NULL;
    SingleStringMessage request;
    CMTItem message;
    SingleItemMessage reply;

    /* pack the request */
    request.string = name;

    /* encode the request */
    if (CMT_EncodeMessage(SingleStringMessageTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }

    /* set the message request type */
    message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION | 
        SSM_FIND_CERT_KEY | subtype;

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION | 
        SSM_FIND_CERT_KEY | subtype)) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    /* copy the cert lookup key */
    certKey = (CMTItem*)malloc(sizeof(CMTItem));
    if (certKey == NULL) {
        goto loser;
    }

    certKey->len = reply.item.len;
    certKey->data = reply.item.data;

loser:
    return certKey;
}

/* fetches cert key given nickname */
CMTItem* CMT_SCFindCertKeyByNickname(PCMT_CONTROL control, char* name)
{
    return CMT_SCFindCertKey(control, SSM_FIND_KEY_BY_NICKNAME, name);
}

/* fetches cert key given email address */
CMTItem* CMT_SCFindCertKeyByEmailAddr(PCMT_CONTROL control, char* name)
{
    return CMT_SCFindCertKey(control, SSM_FIND_KEY_BY_EMAIL_ADDR, name);
}

/* fetches cert key given DN */
CMTItem* CMT_SCFindCertKeyByNameString(PCMT_CONTROL control, char* name)
{
    return CMT_SCFindCertKey(control, SSM_FIND_KEY_BY_DN, name);
}

/* packs message that contains the cert key for CertGetProp messages */
static CMTStatus cmt_sc_pack_cert_key(PCMT_CONTROL control, CMTItem* certKey,
                                      SSMSecCfgGetCertPropType subtype,
                                      CMTItem* message)
{
    SingleItemMessage request;
    CMTStatus rv = CMTFailure;

    /* pack the request */
    request.item.len = certKey->len;
    request.item.data = certKey->data;

    /* encode the request */
    rv = CMT_EncodeMessage(SingleItemMessageTemplate, message, &request);
    if (rv != CMTSuccess) {
        return rv;
    }

    /* set the message type */
    message->type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_GET_CERT_PROP_BY_KEY | subtype;

    return CMTSuccess;
}
    
/* fetches string property given cert key */
static char* CMT_SCGetCertPropString(PCMT_CONTROL control, 
                                     CMTItem* certKey, 
                                     SSMSecCfgGetCertPropType subtype)
{
    SingleStringMessage reply;
    CMTItem message;
    char* rv = NULL;

    if (certKey == NULL) {
        goto loser;
    }

    /* create the message */
    if (cmt_sc_pack_cert_key(control, certKey, subtype, &message) != 
        CMTSuccess) {
        goto loser;
    }

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_GET_CERT_PROP_BY_KEY)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(SingleStringMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    rv = reply.string;
loser:
    return rv;
}

static CMTStatus CMT_SCGetCertPropTime(PCMT_CONTROL control,
                                       CMTItem* certKey,
                                       SSMSecCfgGetCertPropType subtype,
                                       CMTime* time)
{
    TimeMessage reply;
    CMTItem message;

    if (certKey == NULL) {
        goto loser;
    }

    /* create the message */
    if (cmt_sc_pack_cert_key(control, certKey, subtype, &message) !=
        CMTSuccess) {
        goto loser;
    }

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_GET_CERT_PROP_BY_KEY)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(TimeMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    time->year = reply.year;
    time->month = reply.month;
    time->day = reply.day;
    time->hour = reply.hour;
    time->minute = reply.minute;
    time->second = reply.second;

    return CMTSuccess;
loser:
    return CMTFailure;
}

/* fetches a related cert key given cert key */
static CMTItem* CMT_SCGetCertPropCertKey(PCMT_CONTROL control, 
                                         CMTItem* certKey,
                                         SSMSecCfgGetCertPropType subtype)
{
    SingleItemMessage reply;
    CMTItem message;
    CMTItem* newKey = NULL;
    
    if (certKey == NULL) {
        goto loser;
    }

    /* create the message */
    if (cmt_sc_pack_cert_key(control, certKey, subtype, &message) !=
        CMTSuccess) {
        goto loser;
    }

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_GET_CERT_PROP_BY_KEY)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(SingleItemMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    newKey = (CMTItem*)malloc(sizeof(CMTItem));
    if (newKey == NULL) {
        goto loser;
    }
    newKey->len = reply.item.len;
    newKey->data = reply.item.data;
loser:
    return newKey;
}

/* fetches cert nickname given cert key */
char* CMT_SCGetCertPropNickname(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropString(control, certKey, SSM_SECCFG_GET_NICKNAME);
}

/* fetches cert email address given cert key */
char* CMT_SCGetCertPropEmailAddress(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropString(control, certKey, 
                                   SSM_SECCFG_GET_EMAIL_ADDR);
}

/* fetches cert DN given cert key */
char* CMT_SCGetCertPropDN(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropString(control, certKey, SSM_SECCFG_GET_DN);
}

/* fetches cert trust given cert key */
char* CMT_SCGetCertPropTrust(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropString(control, certKey, SSM_SECCFG_GET_TRUST);
}

/* fetches cert serial number given cert key */
char* CMT_SCGetCertPropSerialNumber(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropString(control, certKey, 
                                   SSM_SECCFG_GET_SERIAL_NO);
}

/* fetches cert issuer name given cert key */
char* CMT_SCGetCertPropIssuerName(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropString(control, certKey, 
                                   SSM_SECCFG_GET_ISSUER);
}

/* fetches validity periods as a string given cert key */
CMTStatus CMT_SCGetCertPropTimeNotBefore(PCMT_CONTROL control, 
                                         CMTItem* certKey, CMTime* time)
{
    return CMT_SCGetCertPropTime(control, certKey, 
                                 SSM_SECCFG_GET_NOT_BEFORE, time);
}

CMTStatus CMT_SCGetCertPropTimeNotAfter(PCMT_CONTROL control, 
                                        CMTItem* certKey, CMTime* time)
{
    return CMT_SCGetCertPropTime(control, certKey, 
                                 SSM_SECCFG_GET_NOT_AFTER, time);
}

/* fetches issuer cert key given cert key */
CMTItem* CMT_SCGetCertPropIssuerKey(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropCertKey(control, certKey, 
                                    SSM_SECCFG_GET_ISSUER_KEY);
}

/* fetches next subject key given cert key */
CMTItem* CMT_SCGetCertPropSubjectNext(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropCertKey(control, certKey,
                                    SSM_SECCFG_GET_SUBJECT_NEXT);
}

/* fetches previous subject key given cert key */
CMTItem* CMT_SCGetCertPropSubjectPrev(PCMT_CONTROL control, CMTItem* certKey)
{
    return CMT_SCGetCertPropCertKey(control, certKey,
                                    SSM_SECCFG_GET_SUBJECT_PREV);
}

/* determines whether the cert is in perm db given cert key */
CMTStatus CMT_SCGetCertPropIsPerm(PCMT_CONTROL control, CMTItem* certKey,
                                  CMBool* isPerm)
{
    CMTItem message;
    SingleNumMessage reply;

    if (certKey == NULL) {
        goto loser;
    }

    /* create the message */
    if (cmt_sc_pack_cert_key(control, certKey, SSM_SECCFG_CERT_IS_PERM,
                             &message) != CMTSuccess) {
        goto loser;
    }

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_GET_CERT_PROP_BY_KEY)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    if (reply.value) {
        *isPerm = CM_TRUE;
    }
    else {
        *isPerm = CM_FALSE;
    }

    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus CMT_SCCertIndexEnum(PCMT_CONTROL control, CMInt32 type, 
                              CMInt32* number, CMCertEnum** list)
{
    SingleNumMessage request;
    SCCertIndexEnumReply reply;
    CMTItem message;

    /* initialize these in case of failure */
    *number = 0;
    *list = NULL;

    switch (type) {
    case 0: /* nickname requested */
        message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
            SSM_CERT_INDEX_ENUM | SSM_FIND_KEY_BY_NICKNAME;
        break;
    case 1: /* email address requested */
        message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
            SSM_CERT_INDEX_ENUM | SSM_FIND_KEY_BY_EMAIL_ADDR;
        break;
    case 2: /* DN requested */
        message.type = SSM_REQUEST_MESSAGE | SSM_SEC_CFG_ACTION |
            SSM_CERT_INDEX_ENUM | SSM_FIND_KEY_BY_DN;
        break;
    default:
        goto loser;
        break;
    }

    /* pack the request */
    request.value = 0; /* not important */
    if (CMT_EncodeMessage(SingleNumMessageTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }

    /* send the message and get the response */
    if (CMT_SendMessage(control, &message) == CMTFailure) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
        SSM_CERT_INDEX_ENUM)) {
        goto loser;
    }

    /* decode the reply */
    if (CMT_DecodeMessage(SCCertIndexEnumReplyTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    *number = reply.length;
    *list = (CMCertEnum*)reply.list;

    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus CMT_FindCertExtension(PCMT_CONTROL control, CMUint32 resID, 
                                CMUint32 extension, CMTItem *extValue)
{
    GetCertExtension request;
    SingleItemMessage reply;
    CMTItem message;

    request.resID = resID;
    request.extension = extension;
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_EXTENSION_VALUE;
    if (CMT_EncodeMessage(GetCertExtensionTemplate, 
                          &message, &request) != CMTSuccess) {
        goto loser;
    }
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }
    if (message.type != (SSM_REPLY_OK_MESSAGE | 
                         SSM_CERT_ACTION      |
                         SSM_EXTENSION_VALUE)) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleItemMessageTemplate, 
                          &reply, &message) != CMTSuccess) {
        goto loser;
    }
    extValue->type = 0;
    extValue->data = reply.item.data;
    extValue->len  = reply.item.len;
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus 
CMT_HTMLCertInfo(PCMT_CONTROL control, CMUint32 certID, CMBool showImages,
                 CMBool showIssuer, char **retHtml)
{
    HTMLCertInfoRequest request;
    SingleStringMessage reply;
    CMTItem message;

    if (retHtml == NULL) {
        return CMTFailure;
    }
    request.certID     = certID;
    request.showImages = showImages;
    request.showIssuer = showIssuer;
    if (CMT_EncodeMessage(HTMLCertInfoRequestTemplate, 
                          &message, &request) != CMTSuccess) {
        goto loser;
    }
    message.type = SSM_REQUEST_MESSAGE | SSM_CERT_ACTION | SSM_HTML_INFO;
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }
    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | 
                         SSM_HTML_INFO)) {
        goto loser;
    }
    if (CMT_DecodeMessage(SingleStringMessageTemplate, 
                          &reply, &message) != CMTSuccess) {
        goto loser;
    }
    *retHtml = reply.string;
    return CMTSuccess;
 loser:
    return CMTFailure;
}
