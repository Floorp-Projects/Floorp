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
#include "hashconn.h"
#include "dataconn.h"
#include "serv.h"
#include "servimpl.h"
#include "ssmerrs.h"
#include "newproto.h"
#include "messages.h"
#include "collectn.h"

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(sslconn) (&(sslconn)->super.super.super)
#define SSMCONNECTION(sslconn) (&(sslconn)->super.super)
#define SSMDATACONNECTION(sslconn) (&(sslconn)->super)

void SSMHashConnection_ServiceThread(void * arg);
SSMStatus SSMHashConnection_StartHashing(SSMHashConnection *conn);
SSMStatus SSMHashConnection_FinishHashing(SSMHashConnection *conn);

SSMStatus SSMHashConnection_Create(void *arg, SSMControlConnection * connection,
                                  SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMHashConnection *conn;
    *res = NULL; /* in case we fail */
    
    conn = (SSMHashConnection *) PR_CALLOC(sizeof(SSMHashConnection));
    if (!conn) goto loser;

    SSMRESOURCE(conn)->m_connection = connection;
    rv = SSMHashConnection_Init(conn, (SSMHashInitializer *) arg, 
                                SSM_RESTYPE_HASH_CONNECTION);
    if (rv != PR_SUCCESS) goto loser;

    SSMHashConnection_Invariant(conn);
    
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

SSMStatus SSMHashConnection_Init(SSMHashConnection *conn,
                                SSMHashInitializer *init,
                                SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
    SSMControlConnection *parent = init->m_parent;
 
    rv = SSMDataConnection_Init(SSMDATACONNECTION(conn), parent, type);
    if (rv != PR_SUCCESS) goto loser;

    /* Add the hash type. */
    conn->m_type = init->m_hashtype;

    /* Verify that the hash type is something we can work with. */
    SSMHashConnection_StartHashing(conn);
    rv = (conn->m_context) ? PR_SUCCESS : PR_INVALID_ARGUMENT_ERROR;
    SSMHashConnection_FinishHashing(conn);
    conn->m_resultLen = 0;
    if (rv != PR_SUCCESS) goto loser;

    /* Spin the service thread to set up the SSL connection. */
    SSM_DEBUG("Creating hash service thread.\n");

    SSM_GetResourceReference(SSMRESOURCE(conn));
    SSMDATACONNECTION(conn)->m_dataServiceThread =
        SSM_CreateThread(SSMRESOURCE(conn),
                         SSMHashConnection_ServiceThread);
    if (SSMDATACONNECTION(conn)->m_dataServiceThread == NULL) {
        goto loser;
    }

    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    return rv;
}

SSMStatus
SSMHashConnection_FinishHashing(SSMHashConnection *conn)
{
	SSMStatus rv = SSM_SUCCESS;

	SSM_LockResource(SSMRESOURCE(conn));
    /* If we have a context, end the hash. */
    if (conn->m_context)
    {
        HASH_End(conn->m_context, conn->m_result, &conn->m_resultLen,
                 SSM_HASH_RESULT_LEN);

        if (conn->m_resultLen == 0)
            rv = SSM_FAILURE;

        HASH_Destroy(conn->m_context);
        conn->m_context = NULL;
    }
	SSM_UnlockResource(SSMRESOURCE(conn));
    return rv;
}

SSMStatus 
SSMHashConnection_StartHashing(SSMHashConnection *conn)
{
    PR_ASSERT(conn->m_context == NULL);
    
    SSM_LockResource(SSMRESOURCE(conn));

    if (conn->m_context)
        SSMHashConnection_FinishHashing(conn);
    conn->m_resultLen = 0;
    conn->m_context = HASH_Create(conn->m_type);

    SSM_UnlockResource(SSMRESOURCE(conn));

    return (conn->m_context ? PR_SUCCESS : PR_FAILURE);
}

SSMStatus SSMHashConnection_Destroy(SSMResource *res, PRBool doFree)
{
    SSMHashConnection *conn = (SSMHashConnection *) res;

    if (doFree)
        SSM_DEBUG("SSMHashConnection_Destroy called.\n");
    /* We should be shut down. */
    PR_ASSERT(res->m_threadCount == 0);

    /* Destroy our fields. */

    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_context)
        SSMHashConnection_FinishHashing(conn);
    SSM_UnlockResource(SSMRESOURCE(conn));
    
    /* Destroy superclass fields. */
    SSMDataConnection_Destroy(SSMRESOURCE(conn), PR_FALSE);

    /* Free the connection object if asked. */
    if (doFree)
        PR_DELETE(conn);

    return PR_SUCCESS;
}

void
SSMHashConnection_Invariant(SSMHashConnection *conn)
{
    SSMDataConnection_Invariant(SSMDATACONNECTION(conn));
    /* our specific invariants */

	SSM_LockResource(SSMRESOURCE(conn));
    /* we should not be simultaneously hashed and hashing */
    if (conn->m_context)
        PR_ASSERT(conn->m_resultLen == 0);
	SSM_UnlockResource(SSMRESOURCE(conn));
}

SSMStatus 
SSMHashConnection_Shutdown(SSMResource *arg, SSMStatus status)
{
    SSMStatus rv;

    PR_ASSERT(SSM_IsAKindOf(arg, SSM_RESTYPE_HASH_CONNECTION));
    
    /* Call our superclass shutdown. Thread control is handled there. */
    rv = SSMDataConnection_Shutdown(arg, status);

    return rv;
}


/* 
   Hashing is performed by a single service thread. This thread
   feeds data from the outgoing queue into the hash context.
 */
void SSMHashConnection_ServiceThread(void * arg)
{
    SSMStatus rv = PR_SUCCESS;
    SSMHashConnection * conn;
    SSMControlConnection *ctrl;
    SECItem *msg;
    PRIntn read;
    char buffer[LINESIZE+1] = {0};


    SSM_RegisterNewThread("hash service", (SSMResource *) arg);
    conn = (SSMHashConnection *)arg;
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

    /* Start hashing. */
    SSM_DEBUG("creating hash context.\n");
    SSMHashConnection_StartHashing(conn);

    /* Loop for as long as we have data. */
    while ((SSMRESOURCE(conn)->m_status == PR_SUCCESS) && 
           (SSM_Count(SSMDATACONNECTION(conn)->m_shutdownQ) == 0) && 
           (rv == PR_SUCCESS)) {
        read = LINESIZE;    /* set the read size to the default line size */
        rv = SSMDataConnection_ReadFromSocket(SSMDATACONNECTION(conn),
                                              (PRInt32*)&read, buffer);

        if (read > 0) {
            /* there is data, add it to hash */
            SSM_DEBUG("Received %ld bytes of data for hasher.\n", read);
            PR_ASSERT(conn->m_context);
            HASH_Update(conn->m_context, (unsigned char *) buffer, (unsigned int)read);
        }
        else {
            /* either EOF or an error condition */
            goto finish;
        }
    }

finish:
    SSM_DEBUG("Got shutdown msg.\n");
    
    /* Lock the resource object, so that we know that attribute requests
       won't be pending while we work. */
	SSM_LockResource(SSMRESOURCE(conn));

    /*    SSM_ShutdownResource(SSMRESOURCE(conn), PR_SUCCESS);*/
    SSMHashConnection_FinishHashing(conn);

    /* If we've been asked to return the result, do it now. */
    if (SSMDATACONNECTION(conn)->m_sendResult) {
        SSM_DEBUG("Responding to deferred hash result request.\n");

        msg = (SECItem*)PORT_ZAlloc(sizeof(SECItem));
        PR_ASSERT(msg != NULL);    /* need to have some preallocated
                                      failure to send */
        if (conn->m_result) {
            SSMAttributeValue value;
            GetAttribReply reply;

            value.type = SSM_STRING_ATTRIBUTE;
            value.u.string.len = conn->m_resultLen;
            value.u.string.data = conn->m_result;
            msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_RESOURCE_ACTION
                | SSM_GET_ATTRIBUTE | SSM_STRING_ATTRIBUTE);
            reply.result = rv;
            reply.value = value;
            if (CMT_EncodeMessage(GetAttribReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
                goto loser;
            }
            SSM_DEBUG("Generated a reply (t %lx/l %ld/d %lx)\n", 
                      msg->type, msg->len, msg->data);
            value.u.string.data = NULL;
            SSM_DestroyAttrValue(&value, PR_FALSE);
        }
        else {
            SingleNumMessage reply;

            msg->type = (SECItemType) (SSM_REPLY_ERR_MESSAGE | SSM_RESOURCE_ACTION
                | SSM_GET_ATTRIBUTE | SSM_STRING_ATTRIBUTE);
            reply.value = SSM_ERR_ATTRIBUTE_MISSING;
            if (CMT_EncodeMessage(GetAttribReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
                goto loser;
            }
            SSM_DEBUG("Generated error reply (t %lx/l %ld/d %lx)\n", 
                      msg->type, msg->len, msg->data);
        }
        /* Post this message to the parent's control queue
           for delivery back to the client. */
        ctrl = (SSMControlConnection*)SSMCONNECTION(conn)->m_parent;
        PR_ASSERT(SSM_IsAKindOf(&ctrl->super.super, SSM_RESTYPE_CONTROL_CONNECTION));
        rv = SSM_SendQMessage(ctrl->m_controlOutQ, 
                              SSM_PRIORITY_NORMAL,
                              msg->type, msg->len,
                              (char*)msg->data, PR_TRUE);
        SSM_FreeMessage(msg);
        if (rv != PR_SUCCESS) goto loser;
    }
	SSM_UnlockResource(SSMRESOURCE(conn));
loser:
    SSM_DEBUG("** Thread shutting down ** (%ld)\n", rv);
    if (conn != NULL) {
        SSM_ShutdownResource(SSMRESOURCE(conn), rv);
        SSM_FreeResource(SSMRESOURCE(conn));
    }
}

SSMStatus 
SSMHashConnection_GetAttrIDs(SSMResource *res,
                             SSMAttributeID **ids,
                             PRIntn *count)
{
    SSMStatus rv;

    rv = SSMDataConnection_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 1) * sizeof(SSMAttributeID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_HASHCONN_RESULT;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus 
SSMHashConnection_GetAttr(SSMResource *res,
                          SSMAttributeID attrID,
                          SSMResourceAttrType attrType,
                          SSMAttributeValue *value)
{
    SSMStatus rv = PR_SUCCESS;
    SSMHashConnection *hc = (SSMHashConnection *) res;
	if (! hc)
		return PR_INVALID_ARGUMENT_ERROR;

    SSMHashConnection_Invariant(hc);

	SSM_LockResource(SSMRESOURCE(hc));

    /* see what it is */
    switch(attrID)
    {
    case SSM_FID_HASHCONN_RESULT:
        if (hc->m_resultLen == 0)
        {
            /* Still waiting on hashing to finish. 
               Set the flag which tells us to return the attribute
               when we finish. */
            SSMDATACONNECTION(hc)->m_sendResult = PR_TRUE;
            rv = SSM_ERR_DEFER_RESPONSE;
            goto loser;
        }
        
        /* XXX Fix this. Return the hash result in a string. */
        value->type = SSM_STRING_ATTRIBUTE;
        value->u.string.len = hc->m_resultLen;
        value->u.string.data = (unsigned char *) PR_CALLOC(hc->m_resultLen);
        if (value->u.string.data)
            memcpy(value->u.string.data, hc->m_result, hc->m_resultLen);
        else
            value->u.string.len = 0;
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
	SSM_UnlockResource(SSMRESOURCE(hc));
    return rv;
}
