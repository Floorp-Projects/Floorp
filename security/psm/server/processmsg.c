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
#include "ctrlconn.h"
#include "dataconn.h"
#include "sslconn.h"
#include "servimpl.h"
#include "serv.h"
#include "ssmerrs.h"
#include "certt.h"
#include "keyres.h"
#include "crmfres.h"
#include "kgenctxt.h"
#include "processmsg.h"
#include "signtextres.h"
#include "textgen.h"
#include "secmod.h"
#include "cert.h"
#include "newproto.h"
#include "messages.h"
#include "signtextres.h"
#include "advisor.h"
#include "ssl.h"
#include "protocolshr.h"
#include "msgthread.h"
#include "pk11sdr.h"
#include "sdrres.h"

#define SSL_SC_RSA              0x00000001L
#define SSL_SC_MD2              0x00000010L
#define SSL_SC_MD5              0x00000020L
#define SSL_SC_RC2_CBC          0x00001000L
#define SSL_SC_RC4              0x00002000L
#define SSL_SC_DES_CBC          0x00004000L
#define SSL_SC_DES_EDE3_CBC     0x00008000L
#define SSL_SC_IDEA_CBC         0x00010000L

/* The ONLY reason why we can use these macros for both control and
   data connections is that they inherit from the same superclass.
   Do NOT try this at home. */
#define SSMCONNECTION(c) (&(c)->super)
#define SSMRESOURCE(c) (&(c)->super.super)

SSMStatus
SSMControlConnection_ProcessGenKeyOldStyleToken(SSMControlConnection * ctrl, 
                                                SECItem * msg)
{
    GenKeyOldStyleTokenReply reply;
    SSMResource * res;
    SSMStatus rv = PR_FAILURE;

    /* message contains token name to use for KEYGEN */
    if (CMT_DecodeMessage(GenKeyOldStyleTokenReplyTemplate, &reply, 
                          (CMTItem*)msg) != CMTSuccess) 
      goto loser;
    
    rv = SSMControlConnection_GetResource(ctrl, reply.rid, &res);
    if (rv != SSM_SUCCESS || !res)
        goto loser;
    
    if (!reply.cancel) 
        res->m_uiData = (void *)PK11_FindSlotByName(reply.tokenName);
    SSM_NotifyUIEvent(res);

    /* now generate the key */

    rv = SSM_ERR_DEFER_RESPONSE;
 loser:
    if (res)  /* release reference */
        SSM_FreeResource(res);
    return rv;
}

SSMStatus
SSMControlConnection_ProcessGenKeyPassword(SSMControlConnection * ctrl,
                                           SECItem * msg)
{
    GenKeyOldStylePasswordReply passwordreply;
    SSMStatus rv = SSM_FAILURE;
    SSMResource * res = NULL;
    PK11SlotInfo *slot = NULL;

    if (CMT_DecodeMessage(GenKeyOldStylePasswordReplyTemplate, &passwordreply, 
                          (CMTItem*)msg) != CMTSuccess) 
        goto loser;  
    rv = SSMControlConnection_GetResource(ctrl, passwordreply.rid, &res);
    if (rv != SSM_SUCCESS) 
        goto loser;
    
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_KEYGEN_CONTEXT))
        goto loser;
    
    slot = ((SSMKeyGenContext *)res)->slot;
    /* we are here because there is no password on the slot */
    if (!passwordreply.cancel)
        PK11_InitPin(slot, NULL, passwordreply.password);
    SSM_NotifyUIEvent(res);
    rv = SSM_ERR_DEFER_RESPONSE; 
    
loser:
    return rv;
}


/* Thread functions for SDR_ENCRYPT */
static SSMStatus
sdrencrypt(SSMControlConnection *ctrl, SECItem *msg)
{
  SSMStatus rv = SSM_SUCCESS;
  CMTStatus crv;
  PK11SlotInfo *slot = PK11_GetInternalKeySlot();
  EncryptRequestMessage request;
  EncryptReplyMessage reply;
  SECStatus s;
  SSMResourceID id;
  SSMResource *res = NULL;

  SSM_DEBUG("sdrEncrypt\n");

  request.keyid.data = NULL;
  request.data.data = NULL;
  request.ctx.data = NULL;

  reply.item.data = NULL;

  /* Decode the message (frees message data) */
  crv = CMT_DecodeMessage(EncryptRequestTemplate, &request, (CMTItem*)msg);
  if (crv != CMTSuccess) { rv = SSM_FAILURE; goto loser; }

  /* Create a resource for handling UI events */
  rv = SSM_CreateResource(SSM_RESTYPE_SDR_CONTEXT, 0, ctrl, &id, &res);
  if (rv != SSM_SUCCESS) goto loser;

  /* Set client context field for UI events
   * NOTE: the resource will be deleted before the request data
   * is freed
   */
  res->m_clientContext = request.ctx;

  /* Make sure user has initialized database password */
  if (PK11_NeedUserInit(slot)) {
    SSM_DEBUG("Calling SSM_SetUserPassword\n");
    rv = SSM_SetUserPassword(slot, res);
    SSM_DEBUG("SSM_SetUserPassword returns %d\n", rv);
    if (rv != SSM_SUCCESS) { rv = SSM_ERR_NEED_USER_INIT_DB; goto loser; }
  }

  if (PK11_Authenticate(PK11_GetInternalKeySlot(), PR_TRUE, res) != SECSuccess) {
    rv = SSM_ERR_BAD_DB_PASSWORD;
    goto loser;
  }

  s = PK11SDR_Encrypt((SECItem*)&request.keyid, (SECItem*)&request.data, 
          (SECItem*)&reply.item, res);
  SSM_DEBUG("Encrypt returns %d\n", s);
  SSM_DEBUG("  -> Item: %lx (%d)\n", reply.item.data, reply.item.len);

  /* Generate response */
  msg->type = SSM_SDR_ENCRYPT_REPLY;
  crv = CMT_EncodeMessage(EncryptReplyTemplate, (CMTItem*)msg, &reply);
  if (crv != CMTSuccess) { rv = SSM_FAILURE; goto loser;  /* Unknown error */ }

loser:
  if (res) SSM_FreeResource(res);
  if (request.keyid.data) PR_Free(request.keyid.data);
  if (request.data.data) PR_Free(request.data.data);
  if (request.ctx.data) PR_Free(request.ctx.data);
  if (reply.item.data) PR_Free(reply.item.data);

  return rv;
}

static SSMStatus
sdrdecrypt(SSMControlConnection *ctrl, SECItem *msg)
{
  SSMStatus rv = SSM_SUCCESS;
  CMTStatus crv;
  SECStatus s;
  DecryptRequestMessage request;
  DecryptReplyMessage reply;
  SSMResourceID id;
  SSMResource *res = NULL;

  request.data.data = NULL;
  request.ctx.data = NULL;
  reply.item.data = NULL;

  SSM_DEBUG("sdrDecrypt\n");

  crv = CMT_DecodeMessage(DecryptRequestTemplate, &request, (CMTItem*)msg);
  if (crv != CMTSuccess) { rv = SSM_FAILURE; goto loser; }

  /* Create a resource for handling UI events */
  rv = SSM_CreateResource(SSM_RESTYPE_SDR_CONTEXT, 0, ctrl, &id, &res);
  if (rv != SSM_SUCCESS) goto loser;

  /* Set client context field for UI events
   * NOTE: the resource will be deleted before the request data
   * is freed
   */
  res->m_clientContext = request.ctx;

  if (PK11_Authenticate(PK11_GetInternalKeySlot(), PR_TRUE, res) != SECSuccess) {
    rv = SSM_ERR_BAD_DB_PASSWORD;
    goto loser;
  }

  s = PK11SDR_Decrypt((SECItem*)&request.data, (SECItem*)&reply.item, res);
  if (s != SECSuccess) { rv = SSM_FAILURE; goto loser; }

  msg->type = SSM_SDR_DECRYPT_REPLY;
  crv = CMT_EncodeMessage(DecryptReplyTemplate, (CMTItem*)msg, &reply);
  if (crv != CMTSuccess) { rv = SSM_FAILURE; goto loser;  /* Unknown error */ }

loser:
  if (res) SSM_FreeResource(res);
  if (request.data.data) PR_Free(request.data.data);
  if (request.ctx.data) PR_Free(request.ctx.data);
  if (reply.item.data) PR_Free(reply.item.data);

  return rv;
}

static SSMStatus
sdrChangePassword(SSMControlConnection *ctrl, SECItem *msg)
{
  SSMStatus rv = SSM_SUCCESS;
  SingleItemMessage req;
  PK11SlotInfo *slot = PK11_GetInternalKeySlot();
  SSMResourceID id;
  SSMResource *res = NULL;

  SSM_DEBUG("sdrChangePassword\n");

  req.item.data = 0;

  if (CMT_DecodeMessage(SingleItemMessageTemplate, &req, (CMTItem*)msg) != CMTSuccess) {
    rv = SSM_FAILURE;
    goto loser;
  }

  /* Create a resource for handling UI events */
  rv = SSM_CreateResource(SSM_RESTYPE_SDR_CONTEXT, 0, ctrl, &id, &res);
  if (rv != SSM_SUCCESS) goto loser;

  /* Set client context field for UI events
   * NOTE: the resource will be deleted before the request data
   * is freed
   */
  res->m_clientContext = req.item;
  
  /* Invoke the UI for setting password */
  rv = SSM_SetUserPassword(slot, res);

loser:
  if (res) SSM_FreeResource(res);
  if (req.item.data) PR_Free(req.item.data);

  return rv;
}

static SSMStatus
ProcessMiscUI(SSMControlConnection *ctrl, SECItem *msg)
{
  SSMStatus rv;

  rv = SSM_ProcessMsgOnThreadReply(sdrChangePassword, ctrl, msg);
  msg->type = (SSM_REPLY_OK_MESSAGE|SSM_MISC_ACTION|SSM_MISC_UI|SSM_UI_CHANGE_PASSWORD);

  return rv;
}


SSMStatus 
SSMControlConnection_ProcessMiscRequest(SSMControlConnection * ctrl, 
                                        SECItem * msg)
{
    SingleNumMessage req;
    SingleItemMessage reply;
    char *buf = NULL;
    SSMStatus rv = SSM_SUCCESS;
    SECStatus srv;
  
    SSM_DEBUG("Got a misc request.\n");
    switch (msg->type & SSM_SUBTYPE_MASK) 
    {
    case SSM_MISC_GET_RNG_DATA:
        if (CMT_DecodeMessage(SingleNumMessageTemplate, &req, 
                              (CMTItem*)msg) != CMTSuccess) 
            goto loser;  

        /* Generate as much random data as they want. */
        SSM_DEBUG("The client wants %ld bytes of random data.\n", req.value);
        buf = (char *) PR_CALLOC(req.value);
        if (!buf)
            goto loser;
        srv = RNG_GenerateGlobalRandomBytes(buf, req.value);
        if (srv != SECSuccess)
            goto loser;

        /* Presumably we have random bytes now. Send them back. */
        reply.item.len = req.value;
        reply.item.data = (unsigned char *) buf;

        if (CMT_EncodeMessage(SingleItemMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess)
            goto loser;
        if (msg->data == NULL || msg->len == 0) 
            goto loser;
        msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_MISC_ACTION | SSM_MISC_GET_RNG_DATA);
        goto done;


    case SSM_MISC_SDR_ENCRYPT:
	rv = SSM_ProcessMsgOnThread(sdrencrypt, ctrl, msg);
	if (rv != PR_SUCCESS) goto loser;

        rv = SSM_ERR_DEFER_RESPONSE;
        goto done;

    case SSM_MISC_SDR_DECRYPT:
	rv = SSM_ProcessMsgOnThread(sdrdecrypt, ctrl, msg);
        if (rv != PR_SUCCESS) goto loser;

        rv = SSM_ERR_DEFER_RESPONSE;

        goto done;

    case SSM_MISC_UI:
        rv = ProcessMiscUI(ctrl, msg);
        if (rv != PR_SUCCESS) goto loser;

/*         rv = SSM_ERR_DEFER_RESPONSE; */
        goto done;

    case SSM_MISC_PUT_RNG_DATA:
    default:
        SSM_DEBUG("Unknown misc request (%lx).\n", (msg->type & SSM_SUBTYPE_MASK));
        goto loser;
    }
    goto done;
 loser:
    SSM_DEBUG("ProcessMiscRequest: loser hit, rv = %ld.\n",
              rv);
    if (msg->data)
    {
        PR_Free(msg->data);
        msg->data = NULL;
        msg->len = 0;
    }
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    if (buf)
        PR_Free(buf);
    return rv;
}

SSMStatus 
SSMControlConnection_ProcessCertRequest(SSMControlConnection * ctrl, 
                                        SECItem * msg)
{
  SSMStatus rv = PR_SUCCESS;
  
  SSM_DEBUG("Got a cert-related request.\n");
  switch (msg->type & SSM_SUBTYPE_MASK) {
  case SSM_VERIFY_CERT:
    rv = SSMControlConnection_ProcessVerifyCertRequest(ctrl, msg);
    break;
  case SSM_IMPORT_CERT:
    rv = SSMControlConnection_ProcessImportCertRequest(ctrl, msg);
    break;
  case SSM_DECODE_CERT:
    rv = SSMControlConnection_ProcessDecodeCertRequest(ctrl, msg);
    break;
  case SSM_FIND_BY_NICKNAME:
    rv = SSMControlConnection_ProcessFindCertByNickname(ctrl, msg);
    break;
  case SSM_FIND_BY_KEY:
    rv = SSMControlConnection_ProcessFindCertByKey(ctrl, msg);
    break;
  case SSM_FIND_BY_EMAILADDR:
    rv = SSMControlConnection_ProcessFindCertByEmailAddr(ctrl, msg);
    break;
  case SSM_ADD_TO_DB:
    rv = SSMControlConnection_ProcessAddCertToDB(ctrl, msg);
    break;
  case SSM_MATCH_USER_CERT:
    rv = SSMControlConnection_ProcessMatchUserCert(ctrl, msg);
    break;
  case SSM_DESTROY_CERT:
    rv = SSMControlConnection_ProcessDestroyCert(ctrl, msg);
    break;
  case SSM_DECODE_TEMP_CERT:
    rv = SSMControlConnection_ProcessDecodeAndCreateTempCert(ctrl, msg);
    break;
  case SSM_REDIRECT_COMPARE:
    rv = SSMControlConnection_ProcessRedirectCompare(ctrl, msg);
    break;
  case SSM_DECODE_CRL:
    rv = SSMControlConnection_ProcessDecodeCRLRequest(ctrl, msg);
    break;
  case SSM_EXTENSION_VALUE:
    rv = SSMControlConnection_ProcessGetExtensionRequest(ctrl, msg);
    break;
  case SSM_HTML_INFO:
    rv = SSMControlConnection_ProcessHTMLCertInfoRequest(ctrl, msg);
    break;
  default:
     SSM_DEBUG("Unknown cert request (%lx).\n",
                                              (msg->type & SSM_SUBTYPE_MASK));
     goto loser;
  }
  goto done;
   loser:
    SSM_DEBUG("ProcessCertRequest: loser hit, rv = %ld.\n",
                          rv);
    if (msg->data)
    {
        PR_Free(msg->data);
        msg->data = NULL;
        msg->len = 0;
    }
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

PRStatus
SSMControlConnection_ProcessKeygenTag(SSMControlConnection * ctrl, 
                                        SECItem * msg)
{
  SSMStatus rv = PR_SUCCESS;
  
  SSM_DEBUG("Got a KEYGEN form tag processing request.\n");
  switch (msg->type & SSM_SUBTYPE_MASK) {
  case SSM_GET_KEY_CHOICE:
    rv = SSMControlConnection_ProcessGetKeyChoiceList(ctrl, msg);
    break;
  case SSM_KEYGEN_TOKEN:
      rv = SSMControlConnection_ProcessGenKeyOldStyleToken(ctrl, msg);
      break;
  case SSM_KEYGEN_PASSWORD:
      rv = SSMControlConnection_ProcessGenKeyPassword(ctrl, msg);
      break;
  case SSM_KEYGEN_START:
      /* We might need to do another message exchange before 
       * we complete this request, to get slot password.
       * Therefore, generate keys on a separate thread, 
       * and let this thread service other messages. -jp
       */
      {
          genKeyArg * arg = (genKeyArg *) PR_Malloc(sizeof(genKeyArg));
          if (!arg) 
              SSM_DEBUG("Memory allocation error!\n");
          arg->ctrl = ctrl;
          arg->msg  = SECITEM_DupItem(msg);
          
          if (SSM_CreateAndRegisterThread(PR_USER_THREAD,
                              SSMControlConnection_ProcessGenKeyOldStyle,
                              (void *)arg,
                              PR_PRIORITY_NORMAL,
                              PR_LOCAL_THREAD,
                              PR_UNJOINABLE_THREAD, 0)== NULL) {
              SSM_DEBUG("Can't start a new thread for old-style keygen!\n");
              rv = SSM_FAILURE;
          }
          else rv = SSM_ERR_DEFER_RESPONSE;
      }
  break;
  default:
      SSM_DEBUG("Unknown KEYGEN request (%lx).\n",
                (msg->type & SSM_SUBTYPE_MASK));
      goto loser;
  }
  goto done;
loser:
  SSM_DEBUG("ProcessKeygenTag: loser hit, rv = %ld.\n",
            rv);
  if (msg->data)
      {
          PR_Free(msg->data);
          msg->data = NULL;
          msg->len = 0;
      }
  if (rv == PR_SUCCESS) rv = PR_FAILURE;
done:
  return (PRStatus) rv;
}

  
SSMStatus 
SSMControlConnection_ProcessVerifyCertRequest(SSMControlConnection * ctrl, 
                                              SECItem * msg)
{
  SSMResource *obj;
  SSMStatus rv;
  VerifyCertRequest request;
  SingleNumMessage reply;

    SSM_DEBUG("Got a Cert Verify request.\n");
    /* Decode message and get resource/field ID */
    if (CMT_DecodeMessage(VerifyCertRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    msg->data = NULL;
    SSM_DEBUG("Rsrc ID %ld, certUsage %d.\n", request.resID, request.certUsage);
 
    rv = SSMControlConnection_GetResource(ctrl, request.resID, &obj);
    if (rv != PR_SUCCESS) goto loser;
    PR_ASSERT(obj != NULL);
 
    /* getting this far means success, send the result of verification */
    rv = SSM_VerifyCert((SSMResourceCert *)obj, (SECCertUsage) request.certUsage);
    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_VERIFY_CERT | SSM_REPLY_OK_MESSAGE);
 
    reply.value = rv;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->data == NULL || msg->len == 0) goto loser;
    return PR_SUCCESS;
 
    /* something went wrong, could not perform cert verification */
loser:
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessDecodeCertRequest(SSMControlConnection * ctrl, 
					      SECItem * msg)
{
  SSMStatus rv;
  CERTCertificate * cert;
  SSMResourceID certID;
  SSMResource * certRes;
  SingleItemMessage request;
  SingleNumMessage reply;

  SSM_DEBUG("Got an DecodeCert request.\n");
  /* Decode message */
  if (CMT_DecodeMessage(SingleItemMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  msg->data = NULL;
  msg->len = 0; 

  /* decode the cert */
  cert = CERT_DecodeCertFromPackage((char *) request.item.data, (int) request.item.len);
  if (!cert) {
    SSM_DEBUG("Can't decode a cert from the buffer!\n");
    goto loser; 
  }

  /* create cert resource for this new cert */
  rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, cert, ctrl, &certID, &certRes);
  if (rv != PR_SUCCESS) {
    SSM_DEBUG("In decode cert: can't create certificate resource.\n"); 
    goto loser;
  }
  SSM_ClientGetResourceReference(certRes, NULL);
  msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_DECODE_CERT);
  reply.value = certID;
  if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }
  if (!msg->data || msg->len == 0)
    goto loser;  

  return PR_SUCCESS;

loser:
  /* compose error reply */
  msg->type = (SECItemType) (SSM_REPLY_ERR_MESSAGE |  SSM_CERT_ACTION | SSM_DECODE_CERT);
  if (msg->data)
    PR_Free(msg->data);
  msg->data = NULL;
  msg->len = 0;
  return PR_FAILURE;
}

char *
SSMControlConnection_GenerateKeyOldStyle(SSMControlConnection * ctrl, 
					 char * choiceString, char * challenge,
					 char * typeString, char * pqgString);

void 
SSMControlConnection_ProcessGenKeyOldStyle(void * arg) 
{
  char * keydata      = NULL;
  GenKeyOldStyleRequest request;
  SingleStringMessage reply;
  genKeyArg * myarg = (genKeyArg *)arg;
  CMTItem * msg = (CMTItem*)myarg->msg;
  SSMControlConnection * ctrl = myarg->ctrl;
  SSMStatus rv = SSM_FAILURE;

  if (CMT_DecodeMessage(GenKeyOldStyleRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }
  
  reply.string = SSMControlConnection_GenerateKeyOldStyle(ctrl, 
                                                          request.choiceString,
                                                          request.challenge, 
                                                          request.typeString, 
                                                          request.pqgString);
  if (!reply.string) 
    goto loser;
  
  /* create reply message */
  msg->type = SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_KEYGEN_DONE;
  if (CMT_EncodeMessage(SingleStringMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }
  if (!msg->len || !msg->data)
    goto loser;
  rv = SSM_SendQMessage(ctrl->m_controlOutQ, SSM_PRIORITY_NORMAL, 
		   msg->type, msg->len, (char *)msg->data, PR_TRUE);

loser:
  /* clean up */
  if (reply.string) 
    PR_Free(reply.string);
  if (request.choiceString) 
    PR_Free(request.choiceString);
  if (request.challenge)
    PR_Free(request.challenge);
  if (request.pqgString)
    PR_Free(request.pqgString);
  if (request.typeString)
    PR_Free(request.typeString);
  if (keydata)
    PR_Free(keydata);
  SSMControlConnection_RecycleItem((SECItem*)msg);
  msg = NULL;
  PR_Free(myarg);
  if (rv != SSM_SUCCESS) {
      SingleNumMessage err_reply;
      msg = (CMTItem *) PORT_ZAlloc(sizeof(CMTItem));
      SSM_DEBUG("Problems generating keys old style!\n");
      msg->type = SSM_REPLY_ERR_MESSAGE;
      err_reply.value = rv;
      CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &err_reply);
      SSM_SendQMessage(ctrl->m_controlOutQ, SSM_PRIORITY_NORMAL,
                       msg->type, msg->len, (char *)msg->data, PR_TRUE);
      SSMControlConnection_RecycleItem((SECItem*)msg);
  }
  
  return;
}
  
  
char ** SSM_GetKeyChoiceList(char * type, char *pqgString, int *nchoices);
  
SSMStatus 
SSMControlConnection_ProcessGetKeyChoiceList(SSMControlConnection * ctrl,
					     SECItem * msg)
{
  char ** choices;
  PRInt32 i=0, nchoices = 0;
  GetKeyChoiceListRequest request;
  GetKeyChoiceListReply   reply;

  if (CMT_DecodeMessage(GetKeyChoiceListRequestTemplate, &request, 
                        (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  choices = SSM_GetKeyChoiceList(request.type, request.pqgString, &nchoices);
  if (!choices)
    goto loser;

  msg->type = (SECItemType)(SSM_REPLY_OK_MESSAGE | SSM_KEYGEN_TAG | SSM_GET_KEY_CHOICE);

  reply.nchoices = nchoices;
  reply.choices = choices;
  if (CMT_EncodeMessage(GetKeyChoiceListReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }

  /* free the result array */
  while (choices[i]) 
    PR_Free(choices[i++]);
  PR_Free(choices);
 
  return PR_SUCCESS;

loser:
  /* compose error reply */
  msg->type = (SECItemType) (SSM_REPLY_ERR_MESSAGE |  SSM_KEYGEN_TAG | SSM_GET_KEY_CHOICE);

  msg->data = NULL;
  msg->len  = 0;
  if (choices) {
    /* free the result array */
    while (choices[i])
      PR_Free(choices[i++]);
    PR_Free(choices);
  }
  return PR_FAILURE;
}

  
SSMStatus
SSMControlConnection_ProcessImportCertRequest(SSMControlConnection * ctrl,
                                              SECItem * msg)
{
  SSMResource *obj;
  SSMStatus rv;
  SingleItemMessage request;
  ImportCertReply reply;
  
  SSM_DEBUG("Got an ImportCert request.\n");
  /* Decode message */
  if (CMT_DecodeMessage(SingleItemMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }
  msg->data = NULL;
  msg->len  = 0;

  /* Unpickle cert and create a resource */
  rv = SSM_UnpickleResource(&obj, SSM_RESTYPE_CERTIFICATE, ctrl, 
                            request.item.len, request.item.data);
  if (rv != PR_SUCCESS)
    goto loser;
  SSM_DEBUG("Imported cert rsrc ID %ld.\n", obj->m_id);
    /* getting this far means success, send the resource ID */
  msg->data = NULL;
  msg->len = 0;
  msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_IMPORT_CERT | SSM_REPLY_OK_MESSAGE);
  reply.result = rv;
  reply.resID = obj->m_id;
  if (CMT_EncodeMessage(ImportCertReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }
  
  if (msg->data == NULL || msg->len == 0) 
	goto loser;
  PR_Free(request.item.data);
  return PR_SUCCESS;
  
  /* something went wrong, could not import cert */
loser:
  if (request.item.data) 
    PR_Free(request.item.data);
  return PR_FAILURE;

}

SSMStatus
SSMControlConnection_ProcessFindCertByNickname(SSMControlConnection *ctrl, SECItem *msg)
{
    SSMStatus rv;
    CERTCertificate *cert = NULL;
    SSMResourceID certID;
    SSMResourceCert * certRes = NULL;
    SingleStringMessage request;
    SingleNumMessage reply;

    SSM_DEBUG("Get a Find Cert By Nickname request\n");

    /* Decode the request */
    if (CMT_DecodeMessage(SingleStringMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* Look for the cert in out db */
    cert = CERT_FindCertByNickname(ctrl->m_certdb, request.string); 

    /* Create a resource for this cert and get an id */
    if (cert) {
        rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
                                cert,
                                ctrl,
                                &certID,
                                (SSMResource**)&certRes);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        rv = SSM_ClientGetResourceReference(&certRes->super, &certID);
        SSM_FreeResource(&certRes->super);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
    } else {
        /* Not found. Return res id 0 */
        certID = 0;
    }

    /* Pack the reply */
    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_FIND_BY_NICKNAME | SSM_REPLY_OK_MESSAGE);
    reply.value = certID;

    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->data == NULL || msg->len == 0)  {
	    goto loser;
    }

    PR_Free(request.string);
    return PR_SUCCESS;
  
    /* something went wrong */
loser:
    if (request.string) {
        PR_Free(request.string);
    }
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessFindCertByKey(SSMControlConnection *ctrl, SECItem *msg)
{
    SSMStatus rv;
    CERTCertificate *cert = NULL;
    SSMResourceID certID;
    SSMResourceCert * certRes = NULL;
    SingleItemMessage request;
    SingleNumMessage reply;

    SSM_DEBUG("Get a Find Cert By Key request\n");

    /* Decode the request */
    if (CMT_DecodeMessage(SingleItemMessageTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* Look for the cert in out db */
    cert = CERT_FindCertByKey(ctrl->m_certdb, (SECItem*)&request.item); 

    /* Create a resource for this cert and get an id */
    if (cert) {
        rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
                                cert,
                                ctrl,
                                &certID,
                                (SSMResource**)&certRes);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        rv = SSM_ClientGetResourceReference(&certRes->super, &certID);
        SSM_FreeResource(&certRes->super);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
    } else {
        /* Not found. Return res id 0 */
        certID = 0;
    }

    SSM_DEBUG("Returning cert resource %d\n", certID);

    /* Pack the reply */
    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_FIND_BY_KEY | SSM_REPLY_OK_MESSAGE);
    reply.value = certID;

    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
  
    PR_Free(request.item.data);
    return PR_SUCCESS;
  
    /* something went wrong */
loser:
    if (request.item.data) {
        PR_Free(request.item.data);
    }
    return PR_FAILURE;
}

int LDAPCertSearch (const char * rcpt_address, const char * server_name,
                    const char * baseDN, int port, int connect_type,
                    const char * certdb_path, const char * auth_dn, 
                    const char * auth_password, const char * mail_attribs,
                    const char * cert_attribs, void ** cert, int * cert_len);

SSMStatus
SSMControlConnection_ProcessFindCertByEmailAddr(SSMControlConnection *ctrl,
                                                SECItem *msg)
{
    SSMStatus rv;
    CERTCertificate *cert = NULL;
    SSMResourceID certID = 0;
    SSMResourceCert * certRes = NULL;
    SingleStringMessage request;
    SingleNumMessage reply;

    SSM_DEBUG("Got a Find Cert By Email Addr request\n");

    /* Decode the request */
    if (CMT_DecodeMessage(SingleStringMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* Look for the cert in out db */
    cert = CERT_FindCertByEmailAddr(ctrl->m_certdb, request.string);

	/* If there is no search or the cert is not valid */
	if (!cert || (CERT_CheckCertValidTimes(cert, PR_Now(), PR_FALSE) != secCertTimeValid)) {
        char* default_server = NULL;

        /* get the default server name */
        rv = PREF_GetStringPref(ctrl->m_prefs, "ldap_2.default", 
                                &default_server);
        if (rv != SSM_SUCCESS) {
            /* if there is no default server, bail */
            goto loser;
        }

        rv = SSM_CompleteLDAPLookup(ctrl, default_server, request.string);
        if (rv != SSM_SUCCESS) {
			cert = NULL;
            goto done;
        }

        cert = CERT_FindCertByEmailAddr(ctrl->m_certdb, request.string);
	    if (cert && (CERT_CheckCertValidTimes(cert, PR_Now(), PR_FALSE) != secCertTimeValid)) {
			cert = NULL;
		}
	}

done:
	/* Create a resource for this cert and get an id */
    if (cert) {
        rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
                                cert,
                                ctrl,
                                &certID,
                                (SSMResource**)&certRes);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        rv = SSM_ClientGetResourceReference(&certRes->super, &certID);
        SSM_FreeResource(&certRes->super);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
    } else {
        /* Not found. Return res id 0 */
        certID = 0;
    }

    SSM_DEBUG("Returning cert resource %d\n", certID);

    /* Pack the reply */
    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_FIND_BY_EMAILADDR | SSM_REPLY_OK_MESSAGE);
    reply.value = certID;

    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->data == NULL || msg->len == 0)  {
	    goto loser;
    }

    PR_Free(request.string);
    return PR_SUCCESS;
  
    /* something went wrong */
loser:
    if (request.string)
        PR_Free(request.string);
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessAddCertToDB(SSMControlConnection *ctrl, SECItem *msg)
{
    SSMStatus rv;
    SSMResourceCert *certRes;
    CERTCertificate *cert;
    CERTCertTrust trust;
    AddTempCertToDBRequest request;

    SSM_DEBUG("Add Cert to DB");

    /* Decode the request */
    if (CMT_DecodeMessage(AddTempCertToDBRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    trust.sslFlags = request.sslFlags;
    trust.emailFlags = request.emailFlags;
    trust.objectSigningFlags = request.objSignFlags;

    /* Get the resource for this id */
    rv = SSMControlConnection_GetResource(ctrl, request.resID,
                                          (SSMResource**)&certRes);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* Get the CERTCertificate pointer for this resource */
    cert = certRes->cert;

    /* Add the certificate to the database */
    if (CERT_AddTempCertToPerm(cert, request.nickname, &trust) != SECSuccess) {
        goto loser;
    }

    /* Pack the reply */
    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_ADD_TO_DB | SSM_REPLY_OK_MESSAGE);
  
    PR_Free(request.nickname);
    return PR_SUCCESS;

loser:
    if (request.nickname) {
        PR_Free(request.nickname);
    }
    return PR_FAILURE;
}

SSMStatus 
SSMControlConnection_ProcessDestroyCert(SSMControlConnection * ctrl, 
					SECItem * msg)
{
    SSMStatus rv = PR_FAILURE;
    SSMResource * resource;
    SingleNumMessage request;

    if (!msg || !msg->data)
      goto done;

    if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto done;
    }

    PR_Free(msg->data);
    msg->data = NULL;

    rv = SSMControlConnection_GetResource(ctrl, request.value, &resource);
    if (rv != PR_SUCCESS)
      goto done;

    rv = SSMResourceCert_Destroy(resource, PR_TRUE);
    if (rv == PR_SUCCESS) {
      msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_DESTROY_CERT);
      msg->len = 0;
    }
done:
    return rv;
}

typedef struct MatchUserCertArgStr {
    PRBool isOwnThread;
    SSMControlConnection *ctrl;
    SECItem *msg;
} MatchUserCertArg;

static void
ssm_match_user_cert(void *arg)
{
    MatchUserCertArg *matchArgs = (MatchUserCertArg*)arg;
    SSMControlConnection *ctrl = matchArgs->ctrl;
    SECItem *msg = matchArgs->msg;
    SSMCertList *certList;
    CERTCertList *certs = NULL;
    CERTCertListNode *node = NULL;
    SSMResourceCert *certRes;
    SSMResourceID certResID;
    SSMStatus rv;
    int i;
    MatchUserCertRequest request;
    MatchUserCertReply reply;
    SingleNumMessage badReply;

#if DEBUG
    if (matchArgs->isOwnThread) {
        SSM_RegisterThread("match user cert", NULL);
    }
#endif
    /* Decode the request */
    if (CMT_DecodeMessage(MatchUserCertRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    certList = PR_NEWZAP(SSMCertList);
    if (!certList) {
        goto loser;
    }

    PR_INIT_CLIST(&certList->certs);

    /* Find the certs */
    certs = CERT_MatchUserCert(ctrl->m_certdb, (SECCertUsage) request.certType,
                        request.numCANames, request.caNames, ctrl);
    if (!certs) {
		reply.numCerts = 0;
		reply.certs = NULL;
        goto done;
    }

    reply.numCerts = SSM_CertListCount(certs);
    reply.certs = (CMInt32*)malloc(sizeof(CMInt32)*reply.numCerts);
    node = (CERTCertListNode*)PR_LIST_HEAD(&certs->list);
    for (i = 0; i < reply.numCerts; i++) {

        /* Create the cert resource */
        rv = SSM_CreateResource(SSM_RESTYPE_CERTIFICATE,
                                node->cert,
                                ctrl,
                                &certResID,
                                (SSMResource**)&certRes);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        reply.certs[i] = certResID;
        node = (struct CERTCertListNodeStr *) node->links.next;
}

done:
    /* Generate the reply message */
    /* Pack the reply */
    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_CERT_ACTION | SSM_MATCH_USER_CERT | SSM_REPLY_OK_MESSAGE);
    if (CMT_EncodeMessage(MatchUserCertReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->data == NULL || msg->len == 0)  {
	    goto loser;
    }
    SSM_DEBUG("queueing reply: type %lx, len %ld.\n", msg->type, msg->len);
    SSM_SendQMessage(ctrl->m_controlOutQ,
                     SSM_PRIORITY_NORMAL,
                     msg->type, msg->len,
                     (char *)msg->data, PR_TRUE);
    /* Clean up */
    /* Free the certs list */
    SSM_FreeResource(&ctrl->super.super);
    SECITEM_FreeItem(msg, PR_TRUE);
    PR_Free(arg);
    return;

loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;

    badReply.value = rv;
    if (CMT_EncodeMessage(SingleNumMessageTemplate,
                          (CMTItem*)msg, &badReply) == CMTSuccess) {
        SSM_DEBUG("queueing reply: type %lx, len %ld.\n", 
                  msg->type, msg->len);
        SSM_SendQMessage(ctrl->m_controlOutQ,
                         SSM_PRIORITY_NORMAL,
                         msg->type, msg->len,
                         (char *)msg->data, PR_TRUE);        
    } else {
        /* We need to send something back here. */
        PR_ASSERT(0);
    }
    /* Clean up */
    SSM_FreeResource(&ctrl->super.super);
    SECITEM_FreeItem(msg, PR_TRUE);
    PR_Free(arg);
    return;
}

SSMStatus
SSMControlConnection_ProcessMatchUserCert(SSMControlConnection *ctrl, 
                                          SECItem *msg)
{
    MatchUserCertArg *arg;
    PK11SlotList *slotList;
    PK11SlotListElement *currSlot;
    PRBool externalTokenExists = PR_FALSE;

    /* This could potentially require authentication to an
     * external token which would cause Cartman to dead-lock 
     * waiting for the password reply.  So we spin off a separate
     * iff external tokens are installed.
     */
    arg = SSM_ZNEW(MatchUserCertArg);
    if (arg == NULL) {
        return SSM_FAILURE;
    }
    SSM_GetResourceReference(&ctrl->super.super);
    arg->ctrl = ctrl;
    arg->msg = SECITEM_DupItem(msg);
    /* Now let's figure out if there are external tokens installed.*/
    slotList = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE,
                                 ctrl);
    PR_ASSERT(slotList);
    currSlot = slotList->head;
    do {
        if (!PK11_IsInternal(currSlot->slot)) {
            externalTokenExists = PR_TRUE;
            break;
        }
        currSlot = currSlot->next;
    } while (currSlot != slotList->head && currSlot != NULL);
    
    arg->isOwnThread = externalTokenExists;
    if (arg->isOwnThread) {
        SSM_CreateAndRegisterThread(PR_USER_THREAD, ssm_match_user_cert, (void*)arg,
                        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, 
                        PR_UNJOINABLE_THREAD, 0);
    } else {
        ssm_match_user_cert(arg);
    }
    PK11_FreeSlotList(slotList);
    return SSM_ERR_DEFER_RESPONSE;
        
}
SSMStatus
SSMControlConnection_ProcessConserveRequest(SSMControlConnection * ctrl, 
                                            SECItem * msg)
{
  SSMStatus rv = PR_SUCCESS;
  
  switch (msg->type & SSM_SPECIFIC_MASK) {
  case SSM_PICKLE_RESOURCE:
    rv = SSMControlConnection_ProcessPickleRequest(ctrl, msg);
    break;
  case SSM_UNPICKLE_RESOURCE:
    rv = SSMControlConnection_ProcessUnpickleRequest(ctrl, msg);
    break;
  case SSM_PICKLE_SECURITY_STATUS:
    rv = SSMControlConnection_ProcessPickleSecurityStatusRequest(ctrl, msg);
    break;
  default:
    rv = SSM_ERR_ATTRIBUTE_TYPE_MISMATCH;
    goto loser;
  }
  goto done;
loser:
  SSM_DEBUG("ProcessConserveResourceRequest: loser hit, rv = %ld.\n", rv);
  if (msg->data)
    {
      PR_Free(msg->data);
      msg->data = NULL;
      msg->len = 0;
    }
  if (rv == PR_SUCCESS) rv = PR_FAILURE;
done:
  return rv;
}  


SSMStatus 
SSMControlConnection_ProcessPickleRequest(SSMControlConnection * ctrl, 
                                          SECItem * msg)
{
  SSMResource *obj;
  SSMStatus rv;
  PRIntn len;
  void * dataBlob = NULL;
  SingleNumMessage request;
  PickleResourceReply reply;
  
  SSM_DEBUG("Got a PickleResource request.\n");

  /* Decode the request */
  if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  msg->data = NULL;
  SSM_DEBUG("Rsrc ID %ld.\n", request.value);
  
  rv = SSMControlConnection_GetResource(ctrl, request.value, &obj);
  if (rv != PR_SUCCESS) 
    goto loser;
  PR_ASSERT(obj != NULL);
  
  rv = SSM_PickleResource(obj, &len, &dataBlob);
  if (rv != PR_SUCCESS) 
    goto loser;
  msg->data = NULL;
  msg->len = 0;
  msg->type = (SECItemType) (SSM_RESOURCE_ACTION | SSM_PICKLE_RESOURCE 
                             | SSM_CONSERVE_RESOURCE | SSM_REPLY_OK_MESSAGE);
  reply.result = rv;
  reply.blob.len = len;
  reply.blob.data = (unsigned char *) dataBlob;
  if (CMT_EncodeMessage(PickleResourceReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }
  
  if (msg->data == NULL || msg->len == 0) goto loser;
  PR_Free(dataBlob);
  return PR_SUCCESS;
  
  /* something went wrong, could not pickle resource */
loser:
  if (dataBlob) 
    PR_Free(dataBlob);
  return PR_FAILURE;
}


SSMStatus
SSMControlConnection_ProcessUnpickleRequest(SSMControlConnection * ctrl, 
                                            SECItem * msg)
{
  SSMResource *obj;
  SSMStatus rv;
  UnpickleResourceRequest request;
  UnpickleResourceReply reply;

  
  SSM_DEBUG("Got an UnpickleResource request.\n");

  /* Decode the message */
  if (CMT_DecodeMessage(UnpickleResourceRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }
  msg->data = NULL;

  rv = SSM_UnpickleResource(&obj, (SSMResourceType) request.resourceType, ctrl, 
                            (unsigned int) request.resourceData.len, request.resourceData.data);
  if (rv != PR_SUCCESS)
    goto loser;
  SSM_DEBUG("Unpickled rsrc ID %ld.\n", obj->m_id);
    /* getting this far means success, send the resource ID */
  msg->data = NULL;
  msg->len = 0;
  msg->type = (SECItemType) (SSM_RESOURCE_ACTION | SSM_UNPICKLE_RESOURCE | 
    SSM_CONSERVE_RESOURCE | SSM_REPLY_OK_MESSAGE);
  reply.result = rv;
  reply.resID = obj->m_id;
  if (CMT_EncodeMessage(UnpickleResourceReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
      goto loser;
  }
  if (msg->data == NULL || msg->len == 0) goto loser;
  PR_Free(request.resourceData.data);
  return PR_SUCCESS;
  
  /* something went wrong, could not unpickle cert */
loser:
  if (request.resourceData.data) 
    PR_Free(request.resourceData.data);
  return PR_FAILURE;
}


SSMStatus SSMControlConnection_ProcessPickleSecurityStatusRequest(SSMControlConnection* ctrl,
                                                                 SECItem* msg)
{
    SSMStatus rv;
    SSMResource* obj = NULL;
    PRIntn len;
    void* blob = NULL;
    PRIntn securityLevel;
    SingleNumMessage request;
    PickleSecurityStatusReply reply;

    SSM_DEBUG("Got an PickleSecurityStatus request.\n");

    /* decode the message */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) !=
        CMTSuccess) {
        return PR_FAILURE;
    }
    SSM_DEBUG("Rsrc ID %ld.\n", request.value);

    rv = SSMControlConnection_GetResource(ctrl, request.value, &obj);
    if (rv != PR_SUCCESS) { 
        return rv;
    }
    PR_ASSERT(obj != NULL);
  
    /* the resource'd better be an SSMSSLDataConnection */
    if (SSM_IsA(obj, SSM_RESTYPE_SSL_DATA_CONNECTION) != PR_TRUE) {
        goto loser;
    }

    /* now have the SSL connection handle the action */
    rv = SSMSSLDataConnection_PickleSecurityStatus((SSMSSLDataConnection*)obj,
                                                   &len, &blob, 
                                                   &securityLevel);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    msg->data = NULL;
    msg->len = 0;
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION | 
        SSM_CONSERVE_RESOURCE | SSM_PICKLE_SECURITY_STATUS);

    reply.result = rv;
    reply.securityLevel = securityLevel;
    reply.blob.len = len;
    reply.blob.data = (unsigned char *) blob;
    if (CMT_EncodeMessage(PickleSecurityStatusReplyTemplate, (CMTItem*)msg, 
                          &reply) != CMTSuccess) {
        goto loser;
    }
  
    if (msg->data == NULL || msg->len == 0) {
        goto loser;
    }
    PR_Free(blob);
    SSM_FreeResource(obj);
    return PR_SUCCESS;
  
    /* something went wrong, could not pickle security status */
loser:
    if (blob != NULL) { 
        PR_Free(blob);
    }
    if (obj != NULL) {
        SSM_FreeResource(obj);
    }
    return PR_FAILURE;
}
                                                   

SECStatus
SSMControlConnection_AddNewSecurityModule(SSMControlConnection *ctrl, 
                                          SECItem              *msg)
{
    SECStatus      srv=SECFailure;
    AddNewSecurityModuleRequest request;

    if (CMT_DecodeMessage(AddNewSecurityModuleRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    srv = SECMOD_AddNewModule(request.moduleName, request.libraryPath,
                              SECMOD_PubMechFlagstoInternal(request.pubMechFlags),
                              SECMOD_PubCipherFlagstoInternal(request.pubCipherFlags));
 loser:
    if (request.moduleName != NULL) {
        PR_Free(request.moduleName);
    }
    if (request.libraryPath != NULL) {
        PR_Free(request.libraryPath);
    }
    return srv;
}

SSMStatus
SSMControlConnection_DeleteSecurityModule(SSMControlConnection *ctrl, 
                                          SECItem              *msg, 
                                          PRInt32              *moduleType)
{
    SECStatus srv;
    SingleStringMessage request;
    
    if (moduleType == NULL) {
        goto loser;
    } 

    if (CMT_DecodeMessage(SingleStringMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* To avoid any possible addition of data due to differing data types.*/
    *moduleType = 0;
    srv = SECMOD_DeleteModule(request.string, moduleType);
    if (srv != SECSuccess) {
        goto loser;
    }
    PR_Free(request.string);
    return PR_SUCCESS;
 loser:
    if (request.string != NULL) {
        PR_Free(request.string);
    }
    return PR_FAILURE;
}

static PRBool
SSM_CiphersEnabled(PRInt32 *ciphers, PRInt16 numCiphers)
{
    PRInt16 i;
    SECStatus rv;
    PRInt32 policy;

    for (i=0; i<numCiphers; i++) {
        rv = SSL_CipherPolicyGet(ciphers[i], &policy);
        if (rv == SECSuccess && policy == SSL_ALLOWED) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

#define SSL_CB_RC4_128_WITH_MD5              (SSL_EN_RC4_128_WITH_MD5)
#define SSL_CB_RC4_128_EXPORT40_WITH_MD5     (SSL_EN_RC4_128_EXPORT40_WITH_MD5)
#define SSL_CB_RC2_128_CBC_WITH_MD5          (SSL_EN_RC2_128_CBC_WITH_MD5)
#define SSL_CB_RC2_128_CBC_EXPORT40_WITH_MD5 (SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5)
#define SSL_CB_IDEA_128_CBC_WITH_MD5         (SSL_EN_IDEA_128_CBC_WITH_MD5)
#define SSL_CB_DES_64_CBC_WITH_MD5           (SSL_EN_DES_64_CBC_WITH_MD5)
#define SSL_CB_DES_192_EDE3_CBC_WITH_MD5     (SSL_EN_DES_192_EDE3_CBC_WITH_MD5)


static CMInt32
SSM_GetSSLCapabilities(void)
{
    CMInt32 allowed = (SSL_SC_RSA | SSL_SC_MD2 | SSL_SC_MD5);
    PRInt32 policies[2];

    policies[0] = SSL_CB_RC2_128_CBC_WITH_MD5;
    policies[1] = SSL_CB_RC2_128_CBC_EXPORT40_WITH_MD5;
    if (SSM_CiphersEnabled(policies, 2)) {
        allowed |= SSL_SC_RC2_CBC;
    }

    policies[0] = SSL_CB_RC4_128_WITH_MD5;
    policies[1] = SSL_CB_RC4_128_EXPORT40_WITH_MD5;
    if (SSM_CiphersEnabled(policies, 2)) {
        allowed |= SSL_SC_RC4;
    }

    policies[0] = SSL_CB_DES_64_CBC_WITH_MD5;
    if (SSM_CiphersEnabled(policies, 1)) {
        allowed |= SSL_SC_DES_CBC;
    }

    policies[0] = SSL_CB_DES_192_EDE3_CBC_WITH_MD5;
    if (SSM_CiphersEnabled(policies, 1)) {
        allowed |= SSL_SC_DES_EDE3_CBC;
    }

    policies[0] = SSL_CB_IDEA_128_CBC_WITH_MD5;
    if (SSM_CiphersEnabled(policies, 1)) {
        allowed |= SSL_SC_IDEA_CBC;
    }

    return allowed;
}

SSMStatus
SSMControlConnection_ProcessPKCS11Request(SSMControlConnection * ctrl, 
                                          SECItem * msg)
{
  SSMResourceID  rsrcid;
  SSMStatus       rv;
  SECStatus      srv;
  PRInt32        moduleType;
  SingleNumMessage reply;

  SSM_DEBUG("Got a PKCS11 request.\n");
  
  switch (msg->type & SSM_SUBTYPE_MASK) {
  case SSM_CREATE_KEY_PAIR: /*Should just call a function that does the 
			     *approprieate action */
    SSM_DEBUG("Generating a key pair.\n");
    rv = SSMKeyGenContext_BeginGeneratingKeyPair(ctrl, msg, &rsrcid);
    if (rv != PR_SUCCESS) {
      goto loser;
    }
    /* Getting this far means success */
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | SSM_CREATE_KEY_PAIR);
    msg->data = NULL;
    msg->len = 0;
    reply.value = rsrcid;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    break;
  case SSM_FINISH_KEY_GEN:
    SSM_DEBUG("Finish generating all of the key pairs. \n");
    rv = SSMKeyGenContext_FinishGeneratingAllKeyPairs(ctrl, msg);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | SSM_FINISH_KEY_GEN);
    msg->data = NULL;
    msg->len  = 0;
    break;
  case SSM_ADD_NEW_MODULE:
      SSM_DEBUG("Adding a new PKCS11 module.\n");
      srv = SSMControlConnection_AddNewSecurityModule(ctrl, msg);
      msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION | 
                  SSM_ADD_NEW_MODULE);
      reply.value = srv;
      if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
      }
      break;
  case SSM_DEL_MODULE:
      rv = SSMControlConnection_DeleteSecurityModule(ctrl, msg, &moduleType);
      if (rv != PR_SUCCESS) {
          goto loser;
      }
      PR_Free(msg->data);
      msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION |
                  SSM_DEL_MODULE);
      reply.value = moduleType;
      if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
      }
      break;
  case SSM_LOGOUT_ALL:
      PK11_LogoutAll();
      if (msg->data) {
          PR_Free(msg->data);
      }
      msg->data = NULL;
      msg->len  = 0;
      msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION |
                                 SSM_LOGOUT_ALL);
      break;
  case SSM_ENABLED_CIPHERS:
      reply.value = SSM_GetSSLCapabilities();
      msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PKCS11_ACTION |
                                 SSM_ENABLED_CIPHERS);
      if (CMT_EncodeMessage(SingleNumMessageTemplate, 
                            (CMTItem*)msg, &reply) != CMTSuccess) {
          goto loser;
      }
      break;
  default:
    SSM_DEBUG("Unknown PKCS11 message %lx\n",msg->type);
    goto loser;
  }
  return PR_SUCCESS;
  
loser:
  return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessCRMFRequest(SSMControlConnection * ctrl,
                                        SECItem *msg)
{
  SSMResourceID  rsrcid;
  char          *challengeResponse;
  SSMStatus       rv;
  PRInt32        challengeLen;

  SSM_DEBUG("Got a CRMF/CMMF request\n");

  switch(msg->type & SSM_SUBTYPE_MASK) {
  case SSM_CREATE_CRMF_REQ:
      {
        SingleNumMessage reply;
        SSM_DEBUG("Generating a new CRMF request\n");
        rv = SSM_CreateNewCRMFRequest(msg, ctrl, &rsrcid);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
        msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | SSM_CREATE_CRMF_REQ);
        reply.value = rsrcid;
        if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
            goto loser;
        }
      }
    break;
  case SSM_DER_ENCODE_REQ:
      {
          SSMCRMFThreadArg *arg;

          arg = SSM_NEW(SSMCRMFThreadArg);
          if (arg == NULL) {
              goto loser;
          }
          arg->ctrl = ctrl;
          arg->msg = SECITEM_DupItem(msg);
          if (arg->msg == NULL) {
              PR_Free(arg);
          }
          SSM_GetResourceReference(&ctrl->super.super);
          if (SSM_CreateAndRegisterThread(PR_USER_THREAD,
                              SSM_CRMFEncodeThread,
                              (void*)arg,
                              PR_PRIORITY_NORMAL,
                              PR_LOCAL_THREAD,
                              PR_UNJOINABLE_THREAD, 0) == NULL) {
              SSM_DEBUG("Couldn't start thread for CRMF encoding");
              SECITEM_FreeItem(arg->msg, PR_TRUE);
              PR_Free(arg);
              SSM_FreeResource(&ctrl->super.super);
              goto loser;
          }
          return SSM_ERR_DEFER_RESPONSE;
      }
    break;
  case SSM_PROCESS_CMMF_RESP:
    SSM_DEBUG("Process a CMMF Response.\n");
    rv = SSM_ProcessCMMFCertResponse(msg, ctrl);
    if (rv != SSM_ERR_DEFER_RESPONSE) {
      goto loser;
    }
    return rv;
  case SSM_CHALLENGE:
      {
        SingleItemMessage reply;

        SSM_DEBUG("Doing a Challenge-Response for Proof Of Possession.\n");
        rv = SSM_RespondToPOPChallenge(msg, ctrl, &challengeResponse, 
                                   (unsigned int *) &challengeLen);
        if (rv != PR_SUCCESS) {
            goto loser;
        }   
        msg->data = NULL;
        msg->len  = 0;
        msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | SSM_CHALLENGE);
        reply.item.len = challengeLen;
        reply.item.data = (unsigned char *) challengeResponse;
        if (CMT_EncodeMessage(SingleItemMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
            goto loser;
        }
        SSM_DEBUG("Answering challenge with following response:\n%s\n", 
                  challengeResponse);
        PR_Free(challengeResponse);
      }
    break;
  default:
    SSM_DEBUG("Got unkown CRMF/CMMF message %lx\n", msg->type);
    goto loser;
  }
  return PR_SUCCESS;
 loser:
  return PR_FAILURE;
}

char*
get_string_key(SSMLocalizedString whichString)
{
    char *key;
    switch(whichString) {
    case SSM_STRING_BAD_PK11_LIB_PARAM:
        key = "module_invalid_module_name";
        break;
    case SSM_STRING_BAD_PK11_LIB_PATH:
        key = "module_invalid_library";
        break;
    case SSM_STRING_ADD_MOD_SUCCESS:
        key = "module_add_success";
        break;
    case SSM_STRING_ADD_MOD_FAILURE:
        key = "module_add_failure";
        break;
    case SSM_STRING_BAD_MOD_NAME:
        key = "module_invalid_library";
        break;
    case SSM_STRING_EXT_MOD_DEL:
        key = "module_ext_mod_del";
        break;
    case SSM_STRING_INT_MOD_DEL:
        key = "module_int_mod_del";
        break;
    case SSM_STRING_MOD_DEL_FAIL:
        key = "module_del_failure";
        break;
    case SSM_STRING_ADD_MOD_WARN:
        key = "module_add_warning";
        break;
    case SSM_STRING_MOD_PROMPT:
        key = "module_prompt";
        break;
    case SSM_STRING_DLL_PROMPT:
        key = "module_library_prompt";
        break;
    case SSM_STRING_DEL_MOD_WARN:
        key = "module_del_warning";
        break;
    case SSM_STRING_INVALID_CRL:
        key = "invalid_crl";
        break;
    case SSM_STRING_INVALID_CKL:
        key = "invalid_krl";
        break;
    case SSM_STRING_ROOT_CKL_CERT_NOT_FOUND:
        key = "root_ckl_cert_not_found";
        break;
    case SSM_STRING_BAD_CRL_SIGNATURE:
        key = "bad_crl_signature";
        break;
    case SSM_STRING_BAD_CKL_SIGNATURE:
        key = "bad_ckl_signature";
        break;
    case SSM_STRING_ERR_ADD_CRL:
        key = "error_adding_crl";
        break;
    case SSM_STRING_ERR_ADD_CKL:
        key = "error_adding_ckl";
        break;
    case SSM_STRING_JAVASCRIPT_DISABLED:
        key = "javascript_diabled";
        break;
    default:
        key = NULL;
        break;
    }
    if (key == NULL) {
        return NULL;
    }
    return PL_strdup(key);
}

SSMStatus
SSMControlConnection_ProcessLocalizedTextRequest(SSMControlConnection *ctrl,
                                                 SECItem * msg)
{
    SSMStatus            rv;
    char               *localizedString;
    char               *key=NULL;
    SSMTextGenContext  *txtGenCxt=NULL;
    SingleNumMessage request;
    GetLocalizedTextReply reply;

    SSM_DEBUG("Retrieving localized text\n");
    /* Decode the message */
    if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    key = get_string_key((SSMLocalizedString) request.value);
    if (key == NULL) {
        goto loser;
    }
    rv = SSMTextGen_NewContext(NULL, NULL, NULL, NULL, &txtGenCxt);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    rv = SSM_FindUTF8StringInBundles(txtGenCxt, key, &localizedString);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_LOCALIZED_TEXT);
    reply.whichString = request.value;
    reply.localizedString = localizedString;

    if (CMT_EncodeMessage(GetLocalizedTextReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->len == 0 || msg->data == NULL) {
        goto loser;
    }
    SSM_DEBUG("Returning the string \"%s\"\n", localizedString);
    PR_Free(localizedString);
    PR_Free(key);
    return PR_SUCCESS;
loser:
    if (key != NULL) {
        PR_Free(key);
    }
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessFormSigningRequest(SSMControlConnection * ctrl,
                                        SECItem *msg)
{
    SSMStatus       rv;

    SSM_DEBUG("Got a From Signing request\n");

    switch(msg->type & SSM_SUBTYPE_MASK) {
        case SSM_SIGN_TEXT:
            SSM_DEBUG("Generating a new sign text request\n");
            rv = SSM_CreateSignTextRequest(msg, ctrl);
            if (rv != PR_SUCCESS) {
                goto loser;
            }
            msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_FORMSIGN_ACTION | SSM_SIGN_TEXT);
            PR_Free(msg->data);
            msg->data = NULL;
            msg->len = 0;
            break;
        default:
            SSM_DEBUG("Got unkown Form Signing message %lx\n", msg->type);
            goto loser;
            break;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessRedirectCompare(SSMControlConnection *ctrl, 
                                            SECItem * msg)
{
    RedirectCompareRequest request;
    SSMSSLSocketStatus *ss1=NULL, *ss2=NULL;
    SingleNumMessage reply;

    SSM_DEBUG("Comparing Certs for re-direct\n");
    if (CMT_DecodeMessage(RedirectCompareRequestTemplate, &request,
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    if (SSMSSLSocketStatus_Unpickle((SSMResource**)&ss1,
                               ctrl, request.socketStatus1Data.len,
                               request.socketStatus1Data.data) != PR_SUCCESS) {
        goto loser;
    }
    if (SSMSSLSocketStatus_Unpickle((SSMResource**)&ss2,
                               ctrl, request.socketStatus2Data.len,
                               request.socketStatus2Data.data) != PR_SUCCESS) {
        goto loser;
    }
    reply.value = 
        (CMInt32)CERT_CompareCertsForRedirection(ss1->m_cert->cert, 
                                                 ss2->m_cert->cert);
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply)
        != CMTSuccess) {
        goto loser;
    }
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_REDIRECT_COMPARE);
    SSMSSLSocketStatus_Destroy((SSMResource*)ss1, PR_TRUE);
    SSMSSLSocketStatus_Destroy((SSMResource*)ss2, PR_TRUE);
    SSM_DEBUG("Finished comparing certs for re-direction.\n");
    return PR_SUCCESS;
 loser:
    if (ss1 != NULL) {
        SSMSSLSocketStatus_Destroy((SSMResource*)ss1, PR_TRUE);
    }
    if (ss2 != NULL) {
        SSMSSLSocketStatus_Destroy((SSMResource*)ss2, PR_TRUE);
    }
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessDecodeCRLRequest(SSMControlConnection *ctrl, 
                                             SECItem *msg)
{
    DecodeAndAddCRLRequest request;
    SingleNumMessage reply;
    PRArenaPool *arena = NULL;
    CERTCertificate *caCert;
    SECItem derName = { siBuffer, NULL, 0 };
    CERTSignedData sd;
    SECStatus rv;
    int type;
    CERTSignedCrl *crl;

    SSM_DEBUG("Adding a CRL\n");
    reply.value = 0xffffffff; /* Set the return value to some invalid
                               * enumeration for localized strings.
                               */
    if (CMT_DecodeMessage(DecodeAndAddCRLRequestTemplate, &request,
			  (CMTItem*)msg) != CMTSuccess) {
        goto done;
    }
    type = request.type;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto done;
    }
    memset(&sd, 0, sizeof(sd));

    /* The Crl inherits the arena passed in.*/
    rv = CERT_KeyFromDERCrl(arena, (SECItem*)&request.derCrl, &derName);
    if (rv != SECSuccess) {
        reply.value = (type == SEC_CRL_TYPE) ? SSM_STRING_INVALID_CRL :
	                                       SSM_STRING_INVALID_CKL;
	goto done;
    }
    caCert = CERT_FindCertByName(ctrl->m_certdb, &derName);
    if (caCert == NULL) {
        if (type == SEC_KRL_TYPE){
            reply.value = SSM_STRING_ROOT_CKL_CERT_NOT_FOUND;
            goto done;
        }
    } else {
        rv = SEC_ASN1DecodeItem(arena,
                                &sd, CERT_SignedDataTemplate, 
                                (SECItem*)&request.derCrl);
        if (rv != SECSuccess) {
            reply.value = (type == SEC_CRL_TYPE) ? SSM_STRING_INVALID_CRL :
                SSM_STRING_INVALID_CKL;
            goto done;
        }
        rv = CERT_VerifySignedData(&sd, caCert, PR_Now(),
                                   ctrl);
        if (rv != SECSuccess) {
            reply.value = 
                (type == SEC_CRL_TYPE) ? SSM_STRING_BAD_CRL_SIGNATURE :
                SSM_STRING_BAD_CKL_SIGNATURE;
        }
    }
    
    crl = SEC_NewCrl(ctrl->m_certdb, request.url, (SECItem*)&request.derCrl,
                     type);
    if (!crl) {
        reply.value = (type == SEC_CRL_TYPE) ? SSM_STRING_ERR_ADD_CRL :
                                               SSM_STRING_ERR_ADD_CKL;
        goto done;
    }
    reply.value = 0;
    /* Not sure if we still need to do this, but the Nova code does it,
     * so I'll do it here as well.
     * -javi
     */
    SSL_ClearSessionCache();
    
    SEC_DestroyCrl(crl);
 done:
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) 
        != CMTSuccess) {
        return PR_FAILURE;
    }
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION  | SSM_DECODE_CRL);
    return PR_SUCCESS;
}

PRStatus 
SSMControlConnection_ProcessSecurityAdvsiorRequest(SSMControlConnection *ctrl, 
					     SECItem *msg)
{
    SecurityAdvisorRequest request;
    SingleNumMessage reply;
    InfoSecAdvisor infoSecAdvisor;
    PRStatus rv;
    SSMResource *res;

    /* Decode the request message */
    if (CMT_DecodeMessage(SecurityAdvisorRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* Get the request data */
    infoSecAdvisor.infoContext = request.infoContext;
    infoSecAdvisor.resID = request.resID;
    infoSecAdvisor.hostname = request.hostname;
 	infoSecAdvisor.senderAddr = request.senderAddr;
	infoSecAdvisor.encryptedP7CInfo = request.encryptedP7CInfo;
	infoSecAdvisor.signedP7CInfo = request.signedP7CInfo;
	infoSecAdvisor.decodeError = request.decodeError;
	infoSecAdvisor.verifyError = request.verifyError;
	infoSecAdvisor.encryptthis = request.encryptthis;
	infoSecAdvisor.signthis = request.signthis;
	infoSecAdvisor.numRecipients = request.numRecipients;
	infoSecAdvisor.recipients = request.recipients;

    /* Create the security advisor context. */
    rv = (PRStatus) SSMSecurityAdvisorContext_Create(ctrl, &infoSecAdvisor, &res);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    msg->type = (SECItemType) (SSM_SECURITY_ADVISOR | SSM_REPLY_OK_MESSAGE);
    /* Encode the reply */
    reply.value = res->m_id;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }

    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

/* XXX Do I need to destroy the cert in these functions??? */
PRStatus
SSMControlConnection_ProcessSCAddCertToTempDB(SSMControlConnection* ctrl,
                                              SECItem* msg)
{
    PRStatus rv = PR_FAILURE;
    SingleItemMessage request;
    CERTCertificate* impcert = NULL;
    CERTCertificate* cert = NULL;
    SingleItemMessage reply;

    SSM_DEBUG("SecurityConfig: add cert to temp DB\n");

    /* fill in reply in case of failure */
    reply.item.len = 0;
    reply.item.data = NULL;

    /* Decode the request */
    if (CMT_DecodeMessage(SingleItemMessageTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* decode the package that the cert came in */
    impcert = CERT_DecodeCertFromPackage((char *) request.item.data, (int) request.item.len);
    if (impcert == NULL) {
        goto done;
    }

    /* load the cert into the temporary database */
    cert = CERT_NewTempCertificate(ctrl->m_certdb, &impcert->derCert, NULL,
                                   PR_FALSE, PR_TRUE);
    CERT_DestroyCertificate(impcert);
    if (cert == NULL) {
        goto done;
    }

    reply.item.len = cert->certKey.len;
    reply.item.data = cert->certKey.data;

done:
    /* pack cert key into the reply */
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION | 
						        SSM_ADD_CERT_TO_TEMP_DB);

    if (CMT_EncodeMessage(SingleItemMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        goto loser;
    }

    rv = PR_SUCCESS;
loser:
    (void)SSMControlConnection_RecycleItem((SECItem*)&request.item);

    return rv;
}


PRStatus
SSMControlConnection_ProcessSCAddTempCertToPermDB(SSMControlConnection* ctrl,
                                                  SECItem* msg)
{
    PRStatus rv = PR_FAILURE;
    SECStatus srv = SECFailure;
    SCAddTempCertToPermDBRequest request;
    SingleNumMessage reply;
    CERTCertificate* cert = NULL;
    CERTCertTrust trust;

    SSM_DEBUG("SecurityConfig: add temp cert to perm DB\n");

    /* Decode the request */
    if (CMT_DecodeMessage(SCAddTempCertToPermDBRequestTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* look up cert in database */
    cert = CERT_FindCertByKey(ctrl->m_certdb, (SECItem*)&request.certKey);
    if (cert == NULL) {
        goto done;
    }

    /* decode the trust flags string */
    srv = CERT_DecodeTrustString(&trust, request.trustStr);
    if (srv != SECSuccess) { 
        goto done;
    }

    /* if no nickname was passed in, then there must already be a nickname
     * for the cert's subject name
     */
    if ((request.nickname == NULL) || (*request.nickname == '\0')) {
        if ((cert->subjectList == NULL) ||
            (cert->subjectList->entry == NULL) ||
            (cert->subjectList->entry->nickname == NULL)) {
            srv = SECFailure;
            goto done;
        }
        /* force zero length string case to null */
        request.nickname = NULL;
    }

    /* add the cert to the perm database */
    if (cert->isperm) {
        srv = SECFailure;
        goto done;
    }
    else {
        srv = CERT_AddTempCertToPerm(cert, request.nickname, &trust);
    }

done:
    /* pack the reply */
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION | 
        SSM_ADD_TEMP_CERT_TO_DB);

    reply.value = (CMInt32)srv;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        goto loser;
    }

    rv = PR_SUCCESS;
loser:
    (void)SSMControlConnection_RecycleItem((SECItem*)&request.certKey);
    if (request.trustStr != NULL) {
        PR_Free(request.trustStr);
    }
    if (request.nickname != NULL) {
        PR_Free(request.nickname);
    }

    return rv;
}


static SECStatus SC_DeleteCertCB(CERTCertificate* cert, void* arg)
{
    return SEC_DeletePermCertificate(cert);
}

PRStatus
SSMControlConnection_ProcessSCDeletePermCerts(SSMControlConnection* ctrl,
                                              SECItem* msg)
{
    PRStatus rv = PR_FAILURE;
    SECStatus srv = SECFailure;
    SCDeletePermCertsRequest request;
    SingleNumMessage reply;
    CERTCertificate* cert = NULL;

    SSM_DEBUG("SecurityConfig: delete perm certs\n");

    if (CMT_DecodeMessage(SCDeletePermCertsRequestTemplate, &request,
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    cert = CERT_FindCertByKey(ctrl->m_certdb, (SECItem*)&request.certKey);
    if (cert == NULL) {
        goto done;
    }

    if (request.deleteAll == PR_TRUE) {
        srv = CERT_TraversePermCertsForSubject(ctrl->m_certdb, 
                                              &cert->derSubject,
                                              SC_DeleteCertCB, NULL);
    }
    else {
        srv = SEC_DeletePermCertificate(cert);
    }

    /* XXX original Nova code actually returns PR_SUCCESS even if delete
     *     operation fails: what gives?
     */
done:
    /* pack the reply */
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION | 
        SSM_DELETE_PERM_CERTS);
    reply.value = (CMInt32)srv;
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        goto loser;
    }

    rv = PR_SUCCESS;
loser:
    (void)SSMControlConnection_RecycleItem((SECItem*)&request.certKey);

    return rv;
}


PRStatus SSMControlConnection_ProcessSCFindKey(SSMControlConnection* ctrl,
                                               SECItem* msg)
{
    SingleStringMessage request;
    CERTCertificate* cert = NULL;
    SingleItemMessage reply;

    SSM_DEBUG("SecurityConfig: find key\n");

    if (CMT_DecodeMessage(SingleStringMessageTemplate, &request,
                          (CMTItem*)msg) != CMTSuccess) {
        return (PRStatus) SSM_FAILURE;
    }

    switch (msg->type & SSM_SPECIFIC_MASK) {
    case SSM_FIND_KEY_BY_NICKNAME:
        cert = CERT_FindCertByNickname(ctrl->m_certdb, request.string);
        msg->type = (SECItemType)(SSM_SEC_CFG_ACTION | SSM_FIND_CERT_KEY |
                                  SSM_FIND_KEY_BY_NICKNAME);
        break;
    case SSM_FIND_KEY_BY_EMAIL_ADDR:
        cert = CERT_FindCertByEmailAddr(ctrl->m_certdb, request.string);
        msg->type = (SECItemType)(SSM_SEC_CFG_ACTION | SSM_FIND_CERT_KEY |
                                  SSM_FIND_KEY_BY_EMAIL_ADDR);
        break;
    case SSM_FIND_KEY_BY_DN:
        cert = CERT_FindCertByNameString(ctrl->m_certdb, request.string);
        msg->type = (SECItemType)(SSM_SEC_CFG_ACTION | SSM_FIND_CERT_KEY |
                                  SSM_FIND_KEY_BY_DN);
        break;
    default:
        SSM_DEBUG("Wrong subtype!");
        break;
    }

    /* pack cert key into the reply */
    if (cert == NULL) {
        reply.item.len = 0;
        reply.item.data = NULL;
        msg->type = (SECItemType) ((long) msg->type | (long) SSM_REPLY_ERR_MESSAGE);
    }
    else {
        reply.item.len = cert->certKey.len;
        reply.item.data = cert->certKey.data;
        msg->type = (SECItemType) ((long) msg->type | (long) SSM_REPLY_OK_MESSAGE);
    }

    if (CMT_EncodeMessage(SingleItemMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        return (PRStatus) SSM_FAILURE;
    }

    return (PRStatus) SSM_SUCCESS;
}

static SSMStatus 
SSMControlConnection_ProcessSCGetCertPropString(CERTCertificate* cert,
                                                SECItem* msg)
{
    SSMStatus rv = SSM_SUCCESS;
    SingleStringMessage reply;

    reply.string = NULL;
    switch (msg->type & SSM_SPECIFIC_MASK) {
    case SSM_SECCFG_GET_NICKNAME:
        if (cert->nickname != NULL) {
            reply.string = PL_strdup(cert->nickname);
        }
        break;
    case SSM_SECCFG_GET_EMAIL_ADDR:
        if (cert->emailAddr != NULL) {
            reply.string = PL_strdup(cert->emailAddr);
        }
        break;
    case SSM_SECCFG_GET_DN:
        if (cert->subjectName != NULL) {
            reply.string = PL_strdup(cert->subjectName);
        }
        break;
    case SSM_SECCFG_GET_TRUST:
        reply.string = CERT_EncodeTrustString(cert->trust);
        break;
    case SSM_SECCFG_GET_SERIAL_NO:
        reply.string = CERT_Hexify(&cert->serialNumber, 1);
        break;
    case SSM_SECCFG_GET_ISSUER:
        if (cert->issuerName != NULL) {
        reply.string = PL_strdup(cert->issuerName);
        }
        break;
    default:
        break;
    }

    msg->type = (SECItemType)(SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
                              SSM_GET_CERT_PROP_BY_KEY);
    if (CMT_EncodeMessage(SingleStringMessageTemplate, (CMTItem*)msg, 
                          &reply) != CMTSuccess) {
        rv = SSM_FAILURE;
        goto loser;
    }
loser:
    if (reply.string != NULL) {
        PR_Free(reply.string);
    }
    return rv;
}

static 
SSMStatus SSMControlConnection_ProcessSCGetCertPropTime(CERTCertificate* cert,
                                                        SECItem* msg)
{
    TimeMessage reply = {0};
    PRExplodedTime prtime;
    int64 time;

    switch (msg->type & SSM_SPECIFIC_MASK) {
    case SSM_SECCFG_GET_NOT_BEFORE:
        if (DER_UTCTimeToTime(&time, &cert->validity.notBefore) != 
            SECSuccess) {
            msg->type = (SECItemType)(SSM_REPLY_ERR_MESSAGE | 
                                      SSM_SEC_CFG_ACTION |
                                      SSM_GET_CERT_PROP_BY_KEY);
            goto loser;
        }
        break;
    case SSM_SECCFG_GET_NOT_AFTER:
        if (DER_UTCTimeToTime(&time, &cert->validity.notAfter) !=
            SECSuccess) {
            msg->type = (SECItemType)(SSM_REPLY_ERR_MESSAGE | 
                                      SSM_SEC_CFG_ACTION |
                                      SSM_GET_CERT_PROP_BY_KEY);
            goto loser;
        }
        break;
    default:
        break;
    }

    PR_ExplodeTime(time, PR_GMTParameters, &prtime);

    reply.year = prtime.tm_year;
    reply.month = prtime.tm_month;
    reply.day = prtime.tm_mday;
    reply.hour = prtime.tm_hour;
    reply.minute = prtime.tm_min;
    reply.second = prtime.tm_sec;

    msg->type = (SECItemType)(SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION | 
                              SSM_GET_CERT_PROP_BY_KEY);
loser:
    if (CMT_EncodeMessage(TimeMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        return SSM_FAILURE;
    }
    return SSM_SUCCESS;
}

static 
SSMStatus SSMControlConnection_ProcessSCCertIsPerm(CERTCertificate* cert,
                                                   SECItem* msg)
{
    SingleNumMessage reply;

    if (cert->isperm) {
        reply.value = 1; /* non-zero value */
    }
    else {
        reply.value = 0;
    }

    msg->type = (SECItemType)(SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
                              SSM_GET_CERT_PROP_BY_KEY);
    if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        return SSM_FAILURE;
    }

    return SSM_SUCCESS;
}

static 
SSMStatus SSMControlConnection_ProcessSCGetCertPropKey(CERTCertificate* cert,
                                                       SECItem* msg)
{
    CERTCertificate* tmpCert = NULL;
    SingleItemMessage reply;

    switch (msg->type & SSM_SPECIFIC_MASK) {
    case SSM_SECCFG_GET_ISSUER_KEY:
        tmpCert = CERT_FindCertIssuer(cert, PR_Now(), certUsageAnyCA);
        break;
    case SSM_SECCFG_GET_SUBJECT_NEXT:
        tmpCert = CERT_NextSubjectCert(cert);
        break;
    case SSM_SECCFG_GET_SUBJECT_PREV:
        tmpCert = CERT_PrevSubjectCert(cert);
        break;
    default:
        break;
    }

    if (tmpCert == NULL) {
        /* error */
        reply.item.len = 0;
        reply.item.data = NULL;
        msg->type = (SECItemType)(SSM_REPLY_ERR_MESSAGE | SSM_SEC_CFG_ACTION |
                                  SSM_GET_CERT_PROP_BY_KEY);
    }
    else {
        reply.item.len = tmpCert->certKey.len;
        reply.item.data = tmpCert->certKey.data;
        msg->type = (SECItemType)(SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION |
                                  SSM_GET_CERT_PROP_BY_KEY);
    }
    if (CMT_EncodeMessage(SingleItemMessageTemplate, (CMTItem*)msg, &reply) !=
        CMTSuccess) {
        return SSM_FAILURE;
    }
    return SSM_SUCCESS;
}

static 
SSMStatus SSMControlConnection_ProcessSCGetCertProp(SSMControlConnection* ctrl,
                                                    SECItem* msg)
{
    SSMStatus rv = SSM_FAILURE;
    SingleItemMessage request;
    CERTCertificate* cert = NULL;

    SSM_DEBUG("SecurityConfig: get cert prop\n");

    if (CMT_DecodeMessage(SingleItemMessageTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    /* find the cert itself */
    cert = CERT_FindCertByKey(ctrl->m_certdb, (SECItem*)&request.item);
    if (cert == NULL) {
        goto loser;
    }

    switch (msg->type & SSM_SPECIFIC_MASK) {
    case SSM_SECCFG_GET_NICKNAME:
    case SSM_SECCFG_GET_EMAIL_ADDR:
    case SSM_SECCFG_GET_DN:
    case SSM_SECCFG_GET_TRUST:
    case SSM_SECCFG_GET_SERIAL_NO:
    case SSM_SECCFG_GET_ISSUER:
        rv = SSMControlConnection_ProcessSCGetCertPropString(cert, msg);
        break;
    case SSM_SECCFG_GET_NOT_BEFORE:
    case SSM_SECCFG_GET_NOT_AFTER:
        rv = SSMControlConnection_ProcessSCGetCertPropTime(cert, msg);
        break;
    case SSM_SECCFG_CERT_IS_PERM:
        rv = SSMControlConnection_ProcessSCCertIsPerm(cert, msg);
        break;
    case SSM_SECCFG_GET_ISSUER_KEY:
    case SSM_SECCFG_GET_SUBJECT_NEXT:
    case SSM_SECCFG_GET_SUBJECT_PREV:
        rv = SSMControlConnection_ProcessSCGetCertPropKey(cert, msg);
        break;
    default:
        SSM_DEBUG("Got unknown specific mask.\n");
        goto loser;
        break;
    }

loser:
    (void)SSMControlConnection_RecycleItem((SECItem*)&request.item);

    return rv;
}

typedef struct {
    PRIntn type;
    PRIntn length;
    PRIntn allocSize;
    CertEnumElement* list;
} enumCertState;

#define CERT_ENUM_INIT_SIZE 10

static SECStatus EnumCertCallback(CERTCertificate* cert, SECItem* k,
                                  void* data)
{
    enumCertState* state;
    char* name;

    state = (enumCertState*)data;

    switch (state->type) {
    case 0:
        name = cert->nickname;
        break;
    case 1:
        name = cert->emailAddr;
        break;
    case 2:
        name = cert->subjectName;
        break;
    }

    if (name && (*name != '\0')) {
        if (state->length == state->allocSize) {
            /* need to resize: double it each time */
            state->list = (CertEnumElement*)
                PR_Realloc((void*)state->list, 
                           ((state->allocSize)*2)*sizeof(CertEnumElement));
            if (state->list == NULL) {
                goto loser;
            }
            state->allocSize *= 2;
        }

        /* populate the list */
        state->list[state->length].name = name;
        state->list[state->length].certKey.len = cert->certKey.len;
        state->list[state->length].certKey.data = cert->certKey.data;
        (state->length)++;
    }

    return SECSuccess;
loser:
    return SECFailure;
}

static SSMStatus 
SSMControlConnection_ProcessSCCertIndexEnum(SSMControlConnection* ctrl, 
                                            SECItem* msg)
{
    SSMStatus rv = SSM_SUCCESS;
    enumCertState state = {0};
    SCCertIndexEnumReply reply = {0};

    SSM_DEBUG("SecurityConfig: cert index enum\n");

    /* create the list with the initial size */
    state.allocSize = CERT_ENUM_INIT_SIZE;
    state.list = (CertEnumElement*)PR_Calloc(state.allocSize, 
                                             sizeof(CertEnumElement));
    if (state.list == NULL) {
        goto loser;
    }

    /* don't really need to decode the message: sufficient to look at mask */
    switch (msg->type & SSM_SPECIFIC_MASK) {
    case SSM_FIND_KEY_BY_NICKNAME:
        state.type = 0;
        break;
    case SSM_FIND_KEY_BY_EMAIL_ADDR:
        state.type = 1;
        break;
    case SSM_FIND_KEY_BY_DN:
        state.type = 2;
        break;
    default:
        goto loser;
        break;
    }

    if (SEC_TraversePermCerts(ctrl->m_certdb, EnumCertCallback, 
                              (void*)&state) != SECSuccess) {
        goto loser;
    }

    /* list is now populated */
    reply.length = state.length;
    reply.list = state.list;

    /* pack the reply */
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_SEC_CFG_ACTION | 
        SSM_CERT_INDEX_ENUM);

    if (CMT_EncodeMessage(SCCertIndexEnumReplyTemplate, (CMTItem*)msg, 
                          &reply) != CMTSuccess) {
        goto loser;
    }
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
        rv = SSM_FAILURE;
    }
done:
    if (state.list != NULL) {
        PR_Free(state.list);
    }
    return rv;
}

SSMStatus SSMControlConnection_ProcessSecCfgRequest(SSMControlConnection* ctrl,
                                                   SECItem* msg)
{
    SSMStatus rv;

    SSM_DEBUG("Got a security config related request\n");

    switch(msg->type & SSM_SUBTYPE_MASK) {
    case SSM_ADD_CERT_TO_TEMP_DB:
        rv = SSMControlConnection_ProcessSCAddCertToTempDB(ctrl, msg);
        break;
    case SSM_ADD_TEMP_CERT_TO_DB:
        rv = SSMControlConnection_ProcessSCAddTempCertToPermDB(ctrl, msg);
        break;
    case SSM_DELETE_PERM_CERTS:
        rv = SSMControlConnection_ProcessSCDeletePermCerts(ctrl, msg);
        break;
    case SSM_FIND_CERT_KEY:
        rv = SSMControlConnection_ProcessSCFindKey(ctrl, msg);
        break;
    case SSM_GET_CERT_PROP_BY_KEY:
        rv = SSMControlConnection_ProcessSCGetCertProp(ctrl, msg);
        break;
    case SSM_CERT_INDEX_ENUM:
        rv = SSMControlConnection_ProcessSCCertIndexEnum(ctrl, msg);
        break;
    default:
        SSM_DEBUG("Got unknown security config message %lx\n", msg->type);
        rv = SSM_FAILURE;
        break;
    }
    return rv;
}

SSMStatus SSMControlConnection_ProcessTLSRequest(SSMControlConnection* ctrl,
                                                SECItem* msg)
{
    SSMStatus rv;
    TLSStepUpRequest request;
    SingleNumMessage reply;
    SSMSSLDataConnection* conn = NULL;

    SSM_DEBUG("Got a TLS step-up request.\n");

    if (CMT_DecodeMessage(TLSStepUpRequestTemplate, &request, (CMTItem*)msg) !=
        CMTSuccess) {
        goto loser;
    }

    rv = SSMControlConnection_GetResource(ctrl, request.connID, 
                                          (SSMResource**)&conn);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    PR_ASSERT(conn != NULL);
    PR_ASSERT(conn->isTLS);
    PR_ASSERT(conn->stepUpFD != NULL);

    /* turn on security for this connection */
    SSM_LockResource(&(conn->super.super.super));

    /* Set the client context so that the SSL connection knows
       how to interact with the user/client. */
    conn->super.super.super.m_clientContext = request.clientContext;

    /* Poke the step-up FD to make the SSL connection object
       turn on encryption. */
    PR_SetPollableEvent(conn->stepUpFD);

    /* Wait until the connection has been {SSL,TLS}-enabled. */
    SSM_WaitResource(&(conn->super.super.super), PR_INTERVAL_NO_TIMEOUT);
    rv = conn->m_error; /* get result code */

    SSM_UnlockResource(&(conn->super.super.super));

loser:
    /* compose reply */
    reply.value = rv;
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION |
        SSM_TLS_STEPUP);

    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    rv = SSM_SUCCESS;
    if (conn != NULL) {
        SSM_FreeResource((SSMResource*)conn);
    }

    return rv;
}

SSMStatus 
SSMControlConnection_ProcessProxyStepUpRequest(SSMControlConnection* ctrl,
                                               SECItem* msg)
{
    SSMStatus rv;
    ProxyStepUpRequest request;
    SingleNumMessage reply;
    SSMSSLDataConnection* conn = NULL;

    SSM_DEBUG("Got a proxy step-up request.\n");

    if (CMT_DecodeMessage(ProxyStepUpRequestTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    rv = SSMControlConnection_GetResource(ctrl, request.connID, 
                                          (SSMResource**)&conn);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }

    PR_ASSERT(conn != NULL);
    PR_ASSERT(conn->isTLS);
    PR_ASSERT(conn->stepUpFD != NULL);

    /* turn on security for this connection */
    SSM_LockResource(&(conn->super.super.super));

    /* Set the client context so that the SSL connection knows
       how to interact with the user/client. */
    conn->super.super.super.m_clientContext = request.clientContext;

    /* replace the hostname with the actual URL (remote host) */
    /* XXX sjlee: I don't need to copy the string??? */
    conn->hostName = request.url;
    /* Poke the step-up FD to make the SSL connection object
       turn on encryption. */
    PR_SetPollableEvent(conn->stepUpFD);

    /* Wait until the connection has been {SSL,TLS}-enabled. */
    SSM_WaitResource(&(conn->super.super.super), PR_INTERVAL_NO_TIMEOUT);
    rv = conn->m_error; /* get result code */

    SSM_UnlockResource(&(conn->super.super.super));

loser:
    /* compose reply */
    reply.value = rv;
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION |
        SSM_PROXY_STEPUP);

    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    rv = SSM_SUCCESS;
    if (conn != NULL) {
        SSM_FreeResource((SSMResource*)conn);
    }

    return rv;
}

SSMStatus
SSMControlConnection_ProcessGetExtensionRequest(SSMControlConnection *ctrl, 
                                                SECItem              *msg)
{
    GetCertExtension request;
    SSMStatus rv;
    SSMResource *res=NULL;
    SSMResourceCert *certRes;
    SECStatus srv;
    SingleItemMessage reply;

    if (CMT_DecodeMessage(GetCertExtensionTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    rv = SSMControlConnection_GetResource(ctrl, request.resID, &res);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_CERTIFICATE)) {
        goto loser;
    }
    certRes = (SSMResourceCert*)res;
    reply.item.data = NULL;
    reply.item.len  = 0;
    srv = CERT_FindCertExtension(certRes->cert, request.extension, 
                                 (SECItem*)&reply.item);
    if (srv != SECSuccess) {
        goto loser;
    }
    if (CMT_EncodeMessage(SingleItemMessageTemplate, (CMTItem*)msg,
                          &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->data == NULL || msg->len == 0) {
        goto loser;
    }
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_EXTENSION_VALUE);
    SSM_FreeResource(res);
    return SSM_SUCCESS;
 loser:
    if (res) {
        SSM_FreeResource(res);
    }
    return SSM_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessHTMLCertInfoRequest(SSMControlConnection *ctrl, 
                                                SECItem              *msg)
{
    HTMLCertInfoRequest request;
    SingleStringMessage reply;
    SSMResource *res = NULL;
    SSMResourceCert *certRes = NULL;
    SSMStatus rv;

    if(CMT_DecodeMessage(HTMLCertInfoRequestTemplate, &request, 
                         (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    rv = SSMControlConnection_GetResource(ctrl, request.certID, &res);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_CERTIFICATE)) {
        goto loser;
    }
    certRes = (SSMResourceCert*)res;
    reply.string = CERT_HTMLCertInfo(certRes->cert, request.showImages,
                                     request.showIssuer);
    if (reply.string == NULL) {
        goto loser;
    }
    
    if (CMT_EncodeMessage(SingleStringMessageTemplate, (CMTItem*)msg,
                          &reply) != CMTSuccess) {
        goto loser;
    }
    if (msg->data == NULL || msg->len == 0) {
        goto loser;
    }
    msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_CERT_ACTION | SSM_HTML_INFO);
    SSM_FreeResource(res);
    return SSM_SUCCESS;
 loser:
    if (res) {
        SSM_FreeResource(res);
    }
    return SSM_FAILURE;
}
