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
#include "p7econn.h"
#include "serv.h"
#include "servimpl.h"
#include "ssmerrs.h"
#include "newproto.h"
#include "messages.h"
#include "collectn.h"

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(conn) (&(conn)->super.super.super)
#define SSMCONNECTION(conn) (&(conn)->super.super)
#define SSMDATACONNECTION(conn) (&(conn)->super)
#define SSM_PARENT_CONN(conn) ((SSMControlConnection*)((conn)->super.super.m_parent))

void SSMP7EncodeConnection_ServiceThread(void * arg);

/* callbacks for PKCS7 encoder */
void SSMP7EncodeConnection_ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len);
SECItem * SSMP7EncodeConnection_GetPasswordKey(void *arg,
                                               SECKEYKeyDBHandle *handle);

SSMStatus SSMP7EncodeConnection_Create(void *arg, 
                                      SSMControlConnection *connection, 
                                      SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7EncodeConnection *conn;
    *res = NULL; /* in case we fail */
    
    conn = (SSMP7EncodeConnection *) PR_CALLOC(sizeof(SSMP7EncodeConnection));
    if (!conn)
        goto loser;

    SSMRESOURCE(conn)->m_connection = connection;
    rv = SSMP7EncodeConnection_Init(conn, (SSMInfoP7Encode *) arg, 
                                    SSM_RESTYPE_PKCS7_ENCODE_CONNECTION);
    if (rv != PR_SUCCESS)
        goto loser;

    SSMP7EncodeConnection_Invariant(conn);
    
    *res = SSMRESOURCE(conn);
    return PR_SUCCESS;

loser:
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;

    if (conn) {
        SSM_ShutdownResource(SSMRESOURCE(conn), rv); /* force destroy */
        SSM_FreeResource(SSMRESOURCE(conn));
    }
        
    return rv;
}

SSMStatus SSMP7EncodeConnection_Init(SSMP7EncodeConnection *conn,
                                    SSMInfoP7Encode *info,
                                    SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7ContentInfo *cinfo;
 
    rv = SSMDataConnection_Init(SSMDATACONNECTION(conn), info->ctrl, type);
    if (rv != PR_SUCCESS)
        goto loser;

    /* Spin the service thread. */
    SSM_DEBUG("Creating encoder service thread.\n");

    SSM_GetResourceReference(SSMRESOURCE(conn));

    rv = SSMControlConnection_GetResource(SSM_PARENT_CONN(conn), info->ciRID,
                                          (SSMResource **)&cinfo);
    if (rv != PR_SUCCESS)
        goto loser;

    conn->m_cinfo = cinfo;

    /* Save the client UI context */
    SSMRESOURCE(conn)->m_clientContext = info->clientContext;

    SSMDATACONNECTION(conn)->m_dataServiceThread =
        SSM_CreateThread(SSMRESOURCE(conn), 
                         SSMP7EncodeConnection_ServiceThread);
    if (SSMDATACONNECTION(conn)->m_dataServiceThread == NULL) {
        goto loser;
    }

    return PR_SUCCESS;

loser:
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
    return rv;
}

SSMStatus
SSMP7EncodeConnection_FinishEncoding(SSMP7EncodeConnection *conn)
{
	SECStatus rv = SECSuccess;

    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_cinfo) {
        SSM_FreeResource(&conn->m_cinfo->super);
        conn->m_cinfo = NULL;
    }

    if (conn->m_context) {
        conn->m_retValue = SEC_PKCS7EncoderFinish(conn->m_context,
                                    SSMP7EncodeConnection_GetPasswordKey,
                                    conn);
        conn->m_error = PR_GetError();
        conn->m_context = NULL;
    }
    SSM_UnlockResource(SSMRESOURCE(conn));
    return rv;
}

SSMStatus
SSMP7EncodeConnection_StartEncoding(SSMP7EncodeConnection *conn)
{
    PR_ASSERT(conn->m_context == NULL);
    
    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_context)
        SSMP7EncodeConnection_FinishEncoding(conn);

    conn->m_context = 
        SEC_PKCS7EncoderStart(conn->m_cinfo->m_cinfo,
                              SSMP7EncodeConnection_ContentCallback,
                              conn, NULL);

    SSM_UnlockResource(SSMRESOURCE(conn));
    return PR_SUCCESS;
}

SSMStatus SSMP7EncodeConnection_Destroy(SSMResource *res, PRBool doFree)
{
    SSMP7EncodeConnection *conn = (SSMP7EncodeConnection *) res;

    if (doFree)
        SSM_DEBUG("SSMHashConnection_Destroy called.\n");

    /* We should be shut down. */
    PR_ASSERT(res->m_threadCount == 0);

    /* Destroy our fields. */

    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_context)
        SSMP7EncodeConnection_FinishEncoding(conn);

    if (conn->m_cinfo) {
        SSM_FreeResource(&conn->m_cinfo->super);
        conn->m_cinfo = NULL;
    }
    SSM_UnlockResource(SSMRESOURCE(conn));

    
    /* Destroy superclass fields. */
    SSMDataConnection_Destroy(SSMRESOURCE(conn), PR_FALSE);

    /* Free the connection object if asked. */
    if (doFree)
        PR_DELETE(conn);

    return PR_SUCCESS;
}

void
SSMP7EncodeConnection_Invariant(SSMP7EncodeConnection *conn)
{
    SSMDataConnection_Invariant(SSMDATACONNECTION(conn));
    /* our specific invariants */

    SSM_LockResource(SSMRESOURCE(conn));
    /* check class */
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(conn),
                            SSM_RESTYPE_PKCS7_ENCODE_CONNECTION));
    /* we should not be simultaneously encoded and encoding */
    if (conn->m_context)
        PR_ASSERT(conn->m_cinfo != NULL);
    SSM_UnlockResource(SSMRESOURCE(conn));
}

SSMStatus 
SSMP7EncodeConnection_Shutdown(SSMResource *arg, SSMStatus status)
{
    SSMStatus rv = SSM_SUCCESS;
    
    /* Call our superclass shutdown. */
    rv = SSMDataConnection_Shutdown(arg, status);

    return rv;
}


/* 
   PKCS7 decoding is performed by a single service thread. This thread
   feeds data from the outgoing queue into the PKCS7 encoder. 
 */
void SSMP7EncodeConnection_ServiceThread(void * arg)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7EncodeConnection * conn;
    SSMControlConnection *ctrl;
    SECItem *msg;
    PRIntn read;
    char buffer[LINESIZE+1] = {0};

    SSM_RegisterNewThread("p7encode", (SSMResource *) arg);
    conn = (SSMP7EncodeConnection *)arg;
    SSM_DEBUG("initializing.\n");
    if (!arg) { 
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto loser;
    }

    SSMDATACONNECTION(conn)->m_dataServiceThread = PR_GetCurrentThread();

    /* set up the client data socket and authenticate it with nonce */
    rv = SSMDataConnection_SetupClientSocket(SSMDATACONNECTION(conn));
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* Start encoding */
    SSM_DEBUG("Creating encoder.\n");
    SSMP7EncodeConnection_StartEncoding(conn);

    while ((SSMRESOURCE(conn)->m_status == PR_SUCCESS) && 
           (SSM_Count(SSMDATACONNECTION(conn)->m_shutdownQ) == 0) && 
           (rv == PR_SUCCESS)) {
        read = LINESIZE;    /* set the read size to the default line size */
        rv = SSMDataConnection_ReadFromSocket(SSMDATACONNECTION(conn),
                                              (PRInt32*)&read, buffer);
        
        if (read > 0) {
            /* there is data, pass it along to PKCS7 */
            SSM_DEBUG("Received %ld bytes of data for encoder.\n", read);
            PR_ASSERT(conn->m_context);
            SEC_PKCS7EncoderUpdate(conn->m_context, buffer, read);
        }
        else {
            /* either EOF or an error condition */
            goto finish;
        }
    }

finish:
    SSM_DEBUG("Stopping PKCS7 encoder.\n");

    /* If we have a encoder in progress, stop it. */
    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_context) {
        SSMP7EncodeConnection_FinishEncoding(conn);
    }
    SSM_UnlockResource(SSMRESOURCE(conn));

    if (rv == PR_SUCCESS) {
        /* we have an EOF, shut down the client socket. */
        PR_ASSERT(SSMDATACONNECTION(conn)->m_clientSocket != NULL);
        if (SSMDATACONNECTION(conn)->m_clientSocket != NULL) {
            SSM_DEBUG("shutting down client socket.\n");
            SSM_LockResource(SSMRESOURCE(conn));
            PR_Shutdown(SSMDATACONNECTION(conn)->m_clientSocket,
                        PR_SHUTDOWN_SEND);
            SSM_UnlockResource(SSMRESOURCE(conn));
        }
    }

    /* If we've been asked to return the result, return it. */
    if (SSMDATACONNECTION(conn)->m_sendResult) {
        SSM_DEBUG("Responding to deferred content info request.\n");

        msg = (SECItem*)PORT_ZAlloc(sizeof(SECItem));
        PR_ASSERT(msg != NULL);    /* need to have some preallocated
                                      failure to send */
        if (conn->m_cinfo) {
            SSMAttributeValue value;
            GetAttribReply reply;

            value.type = SSM_RID_ATTRIBUTE;
            rv = SSM_ClientGetResourceReference(&conn->m_cinfo->super,
                                                &value.u.rid);
            if (rv != PR_SUCCESS) {
                goto loser;
            }
            msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION
                | SSM_GET_ATTRIBUTE | SSM_RID_ATTRIBUTE);
            reply.value = value;
            reply.result = SSM_SUCCESS;
            if (CMT_EncodeMessage(GetAttribReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
                goto loser;
            }
            SSM_DEBUG("Generated a reply (t %lx/l %ld/d %lx)\n", 
                      msg->type, msg->len, msg->data);
        } else {
            SingleNumMessage reply;

            msg->type = (SECItemType) (SSM_REPLY_ERR_MESSAGE | SSM_RESOURCE_ACTION
                | SSM_GET_ATTRIBUTE | SSM_STRING_ATTRIBUTE);
            reply.value = SSM_ERR_ATTRIBUTE_MISSING;

            if (CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
                goto loser;
            }
            SSM_DEBUG("Generated error reply (t %lx/l %ld/d %lx)\n", 
                      msg->type, msg->len, msg->data);
        }
        /* Post this message to the parent's control queue
           for delivery back to the client. */
        ctrl = (SSMControlConnection*)(SSMCONNECTION(conn)->m_parent);
        PR_ASSERT(SSM_IsAKindOf(&ctrl->super.super,
                                SSM_RESTYPE_CONTROL_CONNECTION));
        rv = SSM_SendQMessage(ctrl->m_controlOutQ, 
                              SSM_PRIORITY_NORMAL,
                              msg->type, msg->len,
                              (char*)msg->data, PR_TRUE);
        SSM_FreeMessage(msg);
        if (rv != PR_SUCCESS)
            goto loser;
    }

loser:
    SSM_DEBUG("** Thread shutting down ** (%ld)\n", rv);
    if (conn != NULL) {
        SSM_ShutdownResource(SSMRESOURCE(conn), rv);
        SSM_FreeResource(SSMRESOURCE(conn));
    }
}

/* Callback functions for encoder. For now, use empty/default functions. */
void SSMP7EncodeConnection_ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len)
{
    SSMStatus rv;
    SSMP7EncodeConnection *conn = (SSMP7EncodeConnection *)arg;
    PRIntn sent = 0;

    if (len == 0)
        return;

    SSM_DEBUG("writing data to socket.\n");
    PR_ASSERT(SSMDATACONNECTION(conn)->m_clientSocket != NULL);
    sent = PR_Send(SSMDATACONNECTION(conn)->m_clientSocket, (void*)buf,
                   (PRIntn)len, 0, PR_INTERVAL_NO_TIMEOUT);
    if (sent != (PRIntn)len) {
        rv = PR_GetError();
        SSM_DEBUG("error writing data: %d \n", rv);
    }
}

SECItem * SSMP7EncodeConnection_GetPasswordKey(void *arg,
                                               SECKEYKeyDBHandle *handle)
{
	return NULL;
}

SSMStatus SSMP7EncodeConnection_SetAttr(SSMResource *res,
                              SSMAttributeID attrID,
                              SSMAttributeValue *value)
{
    switch(attrID) {
    case SSM_FID_CLIENT_CONTEXT:
      SSM_DEBUG("Setting PKCS7 Encode client context\n");
      if (value->type != SSM_STRING_ATTRIBUTE) {
          goto loser;
      }
      if (!(res->m_clientContext.data = (unsigned char *) PR_Malloc(value->u.string.len))) {
          goto loser;
      }
      memcpy(res->m_clientContext.data, value->u.string.data, value->u.string.len);
      res->m_clientContext.len = value->u.string.len;
      break;
    default:
      SSM_DEBUG("Got unknown P7 Encoder Set Attribute Request %d\n", attrID);
      goto loser;
      break;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

SSMStatus SSMP7EncodeConnection_GetAttr(SSMResource *res,
                                        SSMAttributeID attrID,
                                        SSMResourceAttrType attrType,
                                        SSMAttributeValue *value)
{
    SSMP7EncodeConnection *ec = (SSMP7EncodeConnection*)res;

    SSMP7EncodeConnection_Invariant(ec);

    switch (attrID) {
    case SSM_FID_P7CONN_RETURN_VALUE:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = ec->m_retValue;
        break;
    case SSM_FID_P7CONN_ERROR_VALUE:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        value->u.numeric = ec->m_error;
        break;
    default:
        SSM_DEBUG("Got unknown P7 Encoder Set Attribute Request %d\n", attrID);
        goto loser;
        break;
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

