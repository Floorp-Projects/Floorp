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
#include "p7dconn.h"
#include "serv.h"
#include "servimpl.h"
#include "ssmerrs.h"
#include "newproto.h"
#include "messages.h"
#include "collectn.h"
#include "secmime.h"

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(conn) (&(conn)->super.super.super)
#define SSMCONNECTION(conn) (&(conn)->super.super)
#define SSMDATACONNECTION(conn) (&(conn)->super)
#define SSM_PARENT_CONN(conn) ((SSMControlConnection*)((conn)->super.super.m_parent))

void SSMP7DecodeConnection_ServiceThread(void * arg);

SSMStatus SSMP7DecodeConnection_StartDecoding(SSMP7DecodeConnection *conn);

/* callbacks for PKCS7 decoder */
void SSMP7DecodeConnection_ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len);
PK11SymKey * SSMP7DecodeConnection_GetDecryptKey(void *arg, 
                                                 SECAlgorithmID *algid);
PRBool SSMP7DecodeConnection_DecryptionAllowed(SECAlgorithmID *algid,  
                                               PK11SymKey *bulkkey);
SECItem * SSMP7DecodeConnection_GetPasswordKey(void *arg,
                                               SECKEYKeyDBHandle *handle);

SSMStatus SSMP7DecodeConnection_Create(void *arg, 
                                      SSMControlConnection * connection, 
                                      SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7DecodeConnection *conn;
    *res = NULL; /* in case we fail */
    
    conn = (SSMP7DecodeConnection *) PR_CALLOC(sizeof(SSMP7DecodeConnection));
    if (!conn) goto loser;

    SSMRESOURCE(conn)->m_connection = connection;
    rv = SSMP7DecodeConnection_Init(conn, (SSMInfoP7Decode*) arg, 
                                    SSM_RESTYPE_PKCS7_DECODE_CONNECTION);
    if (rv != PR_SUCCESS) goto loser;

    SSMP7DecodeConnection_Invariant(conn);
    
    *res = SSMRESOURCE(conn);
    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (conn) 
    {
        SSM_ShutdownResource(SSMRESOURCE(conn), rv); /* force destroy */
        SSM_FreeResource(SSMRESOURCE(conn));
    }
        
    return rv;
}

SSMStatus SSMP7DecodeConnection_Init(SSMP7DecodeConnection *conn,
                                    SSMInfoP7Decode *info,
                                    SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
 
    rv = SSMDataConnection_Init(SSMDATACONNECTION(conn), info->ctrl, type);
    if (rv != PR_SUCCESS) goto loser;

	/* Start the decoder */
	if (SSMP7DecodeConnection_StartDecoding(conn) != SSM_SUCCESS) {
		goto loser;
	}

    /* Save the client UI context */
    SSMRESOURCE(conn)->m_clientContext = info->clientContext;

    /* Spin the service thread. */
    SSM_DEBUG("Creating decoder service thread.\n");

    SSM_GetResourceReference(SSMRESOURCE(conn));
    SSMDATACONNECTION(conn)->m_dataServiceThread =
        SSM_CreateThread(SSMRESOURCE(conn), 
                         SSMP7DecodeConnection_ServiceThread);
    if (SSMDATACONNECTION(conn)->m_dataServiceThread == NULL) {
        goto loser;
    }

    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    return rv;
}

SSMStatus
SSMP7DecodeConnection_FinishDecoding(SSMP7DecodeConnection *conn)
{
	SEC_PKCS7ContentInfo *p7info = NULL;
	SSMResourceID resID;
	SSMStatus rv = SSM_SUCCESS;

    if (conn->m_cinfo)
    {
        SSM_FreeResource(&conn->m_cinfo->super);
        conn->m_cinfo = NULL;
    }

    if (conn->m_context)
    {
        p7info = SEC_PKCS7DecoderFinish(conn->m_context);
		if (!p7info) {
			conn->m_error = PR_GetError();
		}
        conn->m_context = NULL;
		if (p7info)
		{
			rv = SSM_CreateResource(SSM_RESTYPE_PKCS7_CONTENT_INFO,
									p7info,
                                    SSM_PARENT_CONN(conn),
									&resID,
									(SSMResource **) &conn->m_cinfo);
		}
		else
			rv = PR_FAILURE;

        SSM_LockResource(&conn->super.super.super);
        SSM_NotifyResource(&conn->super.super.super);
        SSM_UnlockResource(&conn->super.super.super);
    }
    return rv;
}

SSMStatus 
SSMP7DecodeConnection_StartDecoding(SSMP7DecodeConnection *conn)
{
    PR_ASSERT(conn->m_context == NULL);
    
    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_context)
        SSMP7DecodeConnection_FinishDecoding(conn);

    conn->m_context = 
        SEC_PKCS7DecoderStart(SSMP7DecodeConnection_ContentCallback,
                              conn,
                              SSMP7DecodeConnection_GetPasswordKey,
                              conn,
                              SSMP7DecodeConnection_GetDecryptKey,
                              conn,
                              SSMP7DecodeConnection_DecryptionAllowed);
    SSM_UnlockResource(SSMRESOURCE(conn));

	/* Did we get a context */
	if (!conn->m_context) {
		conn->m_error = PR_GetError();
		return PR_FAILURE;
	} else {
		return PR_SUCCESS;
	}
}

SSMStatus SSMP7DecodeConnection_Destroy(SSMResource *res, PRBool doFree)
{
    SSMP7DecodeConnection *conn = (SSMP7DecodeConnection *) res;

    if (doFree)
        SSM_DEBUG("SSMP7DecodeConnection_Destroy called.\n");

    /* We should be shut down. */
    PR_ASSERT(res->m_threadCount == 0);

    /* Destroy our fields. */

    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_context) 
        SSMP7DecodeConnection_FinishDecoding(conn);

    if (conn->m_cinfo)
    {
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
SSMP7DecodeConnection_Invariant(SSMP7DecodeConnection *conn)
{
    SSMDataConnection_Invariant(SSMDATACONNECTION(conn));
    /* our specific invariants */

    SSM_LockResource(SSMRESOURCE(conn));
    /* check class */
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(conn), SSM_RESTYPE_PKCS7_DECODE_CONNECTION));
    /* we should not be simultaneously decoded and decoding */
    if (conn->m_context)
        PR_ASSERT(conn->m_cinfo == NULL);
    SSM_UnlockResource(SSMRESOURCE(conn));
}

SSMStatus 
SSMP7DecodeConnection_Shutdown(SSMResource *arg, SSMStatus status)
{
    SSMStatus rv = SSM_SUCCESS;
    
    /* Call our superclass shutdown. */
    rv = SSMDataConnection_Shutdown(arg, status);

    return rv;
}


/* 
   PKCS7 decoding is performed by a single service thread. This thread
   feeds data from the outgoing queue into the PKCS7 decoder. 
 */
void SSMP7DecodeConnection_ServiceThread(void * arg)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7DecodeConnection* conn;
    SSMControlConnection* ctrl;
    SECItem* msg;
    PRIntn read;
    char buffer[LINESIZE+1] = {0};

    SSM_RegisterNewThread("p7decode", (SSMResource *) arg);
    conn = (SSMP7DecodeConnection *)arg;
    SSM_DEBUG("initializing.\n");
    if (!arg) 
    { 
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto loser;
    }

    SSMDATACONNECTION(conn)->m_dataServiceThread = PR_GetCurrentThread();

    /* set up the client data socket and authenticate it with nonce */
    rv = SSMDataConnection_SetupClientSocket(SSMDATACONNECTION(conn));
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* Start decoding */
    SSM_DEBUG("Start updating decoder..\n");
    while ((SSMRESOURCE(conn)->m_status == PR_SUCCESS) && 
           (SSM_Count(SSMDATACONNECTION(conn)->m_shutdownQ) == 0) && 
           (rv == PR_SUCCESS)) {
        read = LINESIZE;    /* set the read size to the default line size */
        rv = SSMDataConnection_ReadFromSocket(SSMDATACONNECTION(conn),
                                              (PRInt32*)&read, buffer);
        if (read > 0) {
            /* there is data, pass it along to PKCS7 */
            SSM_DEBUG("Received %ld bytes of data for decoder.\n", read);
            PR_ASSERT(conn->m_context);
            if (SEC_PKCS7DecoderUpdate(conn->m_context, buffer, read) != SECSuccess) {
				conn->m_error = PR_GetError();
				goto finish;
			}
#ifdef XP_MAC
			if (read < LINESIZE) {
				break;
			}
#endif						
        } else {
            /* either EOF or an error condition */
		    /* If we have a decoder in progress, stop it. */
		    SSM_LockResource(SSMRESOURCE(conn));
			if (conn->m_context) {
			    SSM_DEBUG("Stopping PKCS7 decoder, generating content info struct.\n");
			    SSMP7DecodeConnection_FinishDecoding(conn);
			}
			SSM_UnlockResource(SSMRESOURCE(conn));
			break;
        }
    }

finish:
	/* we have an EOF, shut down the client socket */
    PR_ASSERT(SSMDATACONNECTION(conn)->m_clientSocket != NULL);
    if (SSMDATACONNECTION(conn)->m_clientSocket != NULL) {
		SSM_DEBUG("shutting down client socket.\n");
        SSM_LockResource(SSMRESOURCE(conn));
        PR_Shutdown(SSMDATACONNECTION(conn)->m_clientSocket,
                    PR_SHUTDOWN_SEND);
        SSM_UnlockResource(SSMRESOURCE(conn));
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
            reply.result = SSM_SUCCESS;
            reply.value = value;
            if (CMT_EncodeMessage(GetAttribReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
                goto loser;
            }
            SSM_DEBUG("Generated a reply (t %lx/l %ld/d %lx)\n", 
                      msg->type, msg->len, msg->data);
            /*
             * We don't need the content info anymore since we sent it back
             * Is it OK to release our reference?
             */
            SSM_FreeResource(&conn->m_cinfo->super);
            conn->m_cinfo = NULL;
        }
        else {
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
        PR_ASSERT(SSM_IsAKindOf(&ctrl->super.super, SSM_RESTYPE_CONTROL_CONNECTION));
        rv = SSM_SendQMessage(ctrl->m_controlOutQ, 
                              SSM_PRIORITY_NORMAL,
                              msg->type, msg->len,
                              (char*)msg->data, PR_TRUE);
        SSM_FreeMessage(msg);
        if (rv != PR_SUCCESS) goto loser;
    }
    
loser:
    SSM_DEBUG("** Thread shutting down ** (%ld)\n", rv);
    if (conn != NULL) {
        SSM_ShutdownResource(SSMRESOURCE(conn), rv);
        SSM_FreeResource(SSMRESOURCE(conn));
    }
}

SSMStatus 
SSMP7DecodeConnection_GetAttrIDs(SSMResource *res,
                                 SSMAttributeID **ids,
                                 PRIntn *count)
{
    SSMStatus rv;

    rv = SSMDataConnection_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 1) * sizeof(SSMAttributeID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_P7CONN_CONTENT_INFO;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus 
SSMP7DecodeConnection_GetAttr(SSMResource *res,
                              SSMAttributeID attrID,
                              SSMResourceAttrType attrType,
                              SSMAttributeValue *value)
{
    SSMStatus rv = PR_SUCCESS;
    SSMP7DecodeConnection *dc = (SSMP7DecodeConnection *) res;

    SSMP7DecodeConnection_Invariant(dc);

    /* see what it is */
    switch(attrID)
    {
    case SSM_FID_P7CONN_CONTENT_INFO:
        if (!dc->m_cinfo) {
            SSM_LockResource(res);
            SSM_WaitResource(res, PR_TicksPerSecond());
            SSM_UnlockResource(res);
            /*
             * If it's still NULL, then something really bad happened.
             */
            if (!dc->m_cinfo) 
                goto loser;
        }

        /* Allocate a resource ID */
        value->type = SSM_RID_ATTRIBUTE;
        rv = SSM_ClientGetResourceReference(&dc->m_cinfo->super, &value->u.rid);
        if (rv != PR_SUCCESS)
            goto loser;
        /* Let's get rid of our reference to it and let the client inherit it.
         */
        SSM_LockResource(res);
        SSM_FreeResource(&dc->m_cinfo->super);
        dc->m_cinfo = NULL;
        SSM_UnlockResource(res);
        break;

	case SSM_FID_RESOURCE_ERROR:
		value->type = SSM_NUMERIC_ATTRIBUTE;
		value->u.numeric = dc->m_error;
		break;

    default:
        rv = SSMDataConnection_GetAttr(res,attrID,attrType,value);
        if (rv != PR_SUCCESS)
            goto loser;
    }

    goto done;
 loser:
    value->type = SSM_NO_ATTRIBUTE;
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
 done:
    return rv;
}

/* Callback functions for decoder. For now, use empty/default functions. */
void SSMP7DecodeConnection_ContentCallback(void *arg, 
                                           const char *buf,
                                           unsigned long len)
{
    SSMStatus rv;
    SSMP7DecodeConnection *conn = (SSMP7DecodeConnection *)arg;
    PRIntn sent = 0;
    PRInt32 osErr;

    SSM_DEBUG("writing data to socket.\n");
    PR_ASSERT(SSMDATACONNECTION(conn)->m_clientSocket != NULL);
    sent = PR_Send(SSMDATACONNECTION(conn)->m_clientSocket, (void*)buf,
                   (PRIntn)len, 0, PR_INTERVAL_NO_TIMEOUT);
    if (sent != (PRIntn)len) {
        rv = PR_GetError();
        osErr = PR_GetOSError();
        SSM_DEBUG("error writing data: %d OS error: %d\n", rv, osErr);
    }
}

PK11SymKey * SSMP7DecodeConnection_GetDecryptKey(void *arg, 
                                                 SECAlgorithmID *algid)
{
	return NULL;
}

PRBool SSMP7DecodeConnection_DecryptionAllowed(SECAlgorithmID *algid,  
                                               PK11SymKey *bulkkey)
{
	return SECMIME_DecryptionAllowed(algid, bulkkey);
}

SECItem * SSMP7DecodeConnection_GetPasswordKey(void *arg,
                                               SECKEYKeyDBHandle *handle)
{
	return NULL;
}

