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
#include "connect.h"
#include "dataconn.h"
#include "ctrlconn.h"
#include "servimpl.h"
#include "serv.h"
#include "ssmerrs.h"

#define SSMCONNECTION(p) (&(p)->super)
#define SSMRESOURCE(p) (&(p)->super.super)

/* How many milliseconds to wait for client input */
#define SSM_READCLIENT_POKE_INTERVAL 30000

/* keep track of the number of data connections pending. used to throttle cpu usage */
static int gNumDataConnections = 0;

PRBool AreConnectionsActive(void)
{
	return gNumDataConnections > 0;
}


SSMStatus SSMDataConnection_Create(void *arg, SSMControlConnection * connection,
                                  SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMDataConnection *conn;
    *res = NULL; /* in case we fail */
    
    conn = (SSMDataConnection *) PR_CALLOC(sizeof(SSMDataConnection));
    if (!conn) goto loser;

    rv = SSMDataConnection_Init(conn, (SSMControlConnection *) arg, 
                            SSM_RESTYPE_DATA_CONNECTION);
    if (rv != PR_SUCCESS) goto loser;

    SSMDataConnection_Invariant(conn);
    
    *res = SSMRESOURCE(conn);

    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (conn) 
    {
		SSM_ShutdownResource(SSMRESOURCE(conn), rv);
        SSM_FreeResource(SSMRESOURCE(conn));
    }
        
    return rv;
}

SSMStatus SSMDataConnection_Init(SSMDataConnection *conn, 
                                SSMControlConnection *parent, 
                                SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
    SSMResourceType parentType = RESOURCE_CLASS(parent);

    PR_ASSERT(parent != NULL);
    PR_ASSERT((parentType == SSM_RESTYPE_CONTROL_CONNECTION) || (parentType == SSM_RESTYPE_CONNECTION));
    if (!parent) goto loser;
    rv = SSMConnection_Init(parent, SSMCONNECTION(conn), type);
    if (rv != PR_SUCCESS) goto loser;

    /* Initialize data shutdown queue. */
    conn->m_shutdownQ = SSM_NewCollection();
    if (conn->m_shutdownQ == NULL) {
        goto loser;
    }

    /* keep track of the number of data connections pending. */
    gNumDataConnections++;

    /* Hang our shutdown func. */
    SSMCONNECTION(conn)->m_auth_func = SSMDataConnection_Authenticate;

    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    return rv;
}

SSMStatus SSMDataConnection_Destroy(SSMResource *res, PRBool doFree)
{
    SSMDataConnection *conn = (SSMDataConnection *) res;

    /* We should be shut down. */
    PR_ASSERT(res->m_threadCount == 0);

    /* Drain and destroy the queue. */
    if (conn->m_shutdownQ) {
        ssm_DrainAndDestroyQueue(&(conn->m_shutdownQ));
    }

    /* Destroy superclass fields. */
    SSMConnection_Destroy(&(conn->super.super), PR_FALSE);

    /* Free the connection object if asked. */
    if (doFree)
        PR_DELETE(conn);

    return PR_SUCCESS;
}

void
SSMDataConnection_Invariant(SSMDataConnection *conn)
{
    if (conn)
    {
        SSMConnection_Invariant(&conn->super);
        SSM_LockResource(SSMRESOURCE(conn));
        PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(conn), SSM_RESTYPE_DATA_CONNECTION));
        PR_ASSERT(conn->m_shutdownQ != NULL);
        SSM_UnlockResource(SSMRESOURCE(conn));
    }
}

SSMStatus
SSMDataConnection_GetAttrIDs(SSMResource *res,
                             SSMAttributeID **ids,
                             PRIntn *count)
{
    SSMStatus rv;

    rv = SSMConnection_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 1) * sizeof(SSMAttributeID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_CONN_DATA_PENDING;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus
SSMDataConnection_GetAttr(SSMResource *res,
                          SSMAttributeID attrID,
                          SSMResourceAttrType attrType,
                          SSMAttributeValue *value)
{
    SSMStatus rv = PR_SUCCESS;

    /* see what it is */
    switch(attrID)
    {
    case SSM_FID_CONN_DATA_PENDING:
        /* this is not used: will set it to zero for now */
        *(PRUint32*)value->u.numeric = (PRUint32)0;
        value->type = SSM_NUMERIC_ATTRIBUTE;
        break;

    default:
        rv = SSMConnection_GetAttr(res,attrID,attrType,value);
        if (rv != PR_SUCCESS) goto loser;
    }

    goto done;
 loser:
    value->type = SSM_NO_ATTRIBUTE;

 done:
    return rv;
}


SSMStatus SSMDataConnection_SetupClientSocket(SSMDataConnection* conn)
{
    SSMControlConnection* parent = NULL;
    char* pNonce;
    char* temp = NULL;    /* for nonce verification */
    PRFileDesc* socket = NULL;    /* client socket */
    PRNetAddr clientAddr;
    SSMStatus status = PR_FAILURE;
    PRIntn read;

    PR_ASSERT(conn != NULL);

    /* Allocate a nonce-sized chunk of memory to read into.
       (See below.) */
    parent = (SSMControlConnection*)(conn->super.m_parent);
    PR_ASSERT(parent != NULL);
    pNonce = parent->m_nonce;
    PR_ASSERT(pNonce != NULL);
    SSM_DEBUG("I think my parent's nonce is `%s'.\n", pNonce);

    temp = (char*)PORT_ZAlloc(strlen(pNonce));

    while ((socket == NULL) && (SSMRESOURCE(conn)->m_status == PR_SUCCESS)) {
        SSM_DEBUG("accepting next connect.\n");

        /* Wait forever for a connection. (for now) */
        socket = PR_Accept(SSMRESOURCE(conn)->m_connection->m_dataSocket,
                           &clientAddr, PR_INTERVAL_NO_TIMEOUT);
        SSM_DEBUG("accepted connection.\n");

        if ((SSMRESOURCE(conn)->m_status != PR_SUCCESS) && socket) {
            /* May have gotten a socket, but we're shutting down.
               Close and zero out the socket. */
            PR_Close(socket);
            SSM_LockResource(SSMRESOURCE(conn));
            socket = conn->m_clientSocket = NULL;
            SSM_UnlockResource(SSMRESOURCE(conn));
        }
        
        if (socket && !SSM_SocketPeerCheck(socket, PR_FALSE))
        {
            /* 
               Failed peer check. Close socket and listen again. 
               ### mwelch - Could have a denial of service attack here if
               someone keeps trying to connect to this port.
            */
            PR_Close(socket);
            socket = NULL;
            continue;
        }

        if (socket) {
            SSM_LockResource(SSMRESOURCE(conn));
            conn->m_clientSocket = socket;
            SSM_UnlockResource(SSMRESOURCE(conn));

            SSM_DEBUG("reading/verifying nonce.\n");
            status = PR_SUCCESS;

            /* Read the nonce from the client.  If we didn't get the right
               nonce, reject the connection. */
            if ((temp) && (pNonce != NULL)) {
                read = SSM_ReadThisMany(socket, temp, strlen(pNonce));
                if ((unsigned int)read != strlen(pNonce)) {
                    status = PR_GetError();
                }
            }

            if ((status != PR_SUCCESS) || (memcmp(temp, pNonce,
                                                  strlen(pNonce)))) {
#ifdef DEBUG
                char thing1[255];
                char thing2[255];
                
                strncpy(thing1, temp, strlen(pNonce));
                strncpy(thing2, temp, strlen(pNonce));
                /* Bad nonce, no biscuit!  Shut down connection
                   and wait for another on the data port. */
                SSM_DEBUG("Bad nonce, no biscuit!\n");
                SSM_DEBUG("(`%s' != `%s')\n", thing1, thing2);
#endif
                SSM_LockResource(SSMRESOURCE(conn));
                PR_Close(conn->m_clientSocket);
                conn->m_clientSocket = socket = NULL;
                SSM_UnlockResource(SSMRESOURCE(conn));
            }
        }
        else {
            /* Tear everything down, didn't get a connection. */
            SSM_DEBUG("Shutdown during connection setup.\n");
            goto loser;
        }
    }

    SSM_DEBUG("Nonce is valid.\n");

    /* We have a socket.  Close the data port. */
    SSM_LockResource(SSMRESOURCE(conn));
    SSM_DEBUG("Socket is %ld.\n", socket);
    SSM_UnlockResource(SSMRESOURCE(conn));
loser:
    if (temp != NULL) {
        PR_Free(temp);
    }
    return status;
}


SSMStatus SSMDataConnection_ReadFromSocket(SSMDataConnection* conn,
                                          PRInt32* read,
                                          char* buffer)
{
    SSMStatus status;
#if 0
    SSMStatus osStat;
    char statBuf[256];
#endif

    PR_ASSERT(conn != NULL);
    PR_ASSERT(buffer != NULL);

    /* Attempt to read LINESIZE bytes from the socket. */
    do {
        SSM_DEBUG("Attempting to read %ld bytes.\n", *read);
        *read = PR_Recv(conn->m_clientSocket, buffer, *read, 0,
                        PR_MillisecondsToInterval(SSM_READCLIENT_POKE_INTERVAL));
        if (*read < 0) {
            status = PR_GetError();    /* save status for later use */
#if 0
            osStat = PR_GetOSError();    /* just for fun */
            PR_GetErrorText(statBuf);
#endif
        }
        SSM_DEBUG("Got %ld bytes of data, status == %ld.\n", (long)(*read),
                  (long)status);
    }
    while ((*read == -1) && (status == PR_IO_TIMEOUT_ERROR) &&
           SSMRESOURCE(conn)->m_status == PR_SUCCESS);

    /* Don't mask an error if we got one, but set it if we didn't get any
     * data (because that indicates a socket closure).
     */
    if ((*read < 0) && (status == PR_SUCCESS)) {
        status = PR_FAILURE;
    }
    else if (*read >= 0) {
        status = PR_SUCCESS;    /* clear the error from when we waited */
    }

#if 0
    /* Null terminate the buffer so that we can dump it. */
    if (*read >= 0) {
        buffer[*read] = '\0';
    }
#endif
    if (*read > 0) {
        SSM_DEBUG("got %ld bytes of data: <%s>\n", *read, buffer);
    }

    return status;
}


SSMStatus
SSMDataConnection_Shutdown(SSMResource *res, SSMStatus status)
{
    SSMStatus rv, trv;
    PRThread *closer = PR_GetCurrentThread();

    SSMDataConnection *conn = (SSMDataConnection *) res;
    /* SSMDataConnection_Invariant(conn); -- could be called from loser */

    /* Lock down the resource before shutting it down */
    SSM_LockResource(SSMRESOURCE(conn));
    
    /* If we're called from a service thread, clear that thread's
       place in the connection object so it doesn't get interrupted */
    if ((closer == conn->m_dataServiceThread) && (closer != NULL)) {
        conn->m_dataServiceThread = NULL;

        /* Decrement living thread counter */
        SSMRESOURCE(conn)->m_threadCount--;
    }

    /* shut down base class */
    rv = SSMConnection_Shutdown(res, status);

    if ((SSMRESOURCE(conn)->m_status != PR_SUCCESS) &&
        (rv != SSM_ERR_ALREADY_SHUT_DOWN) &&
        (conn->m_clientSocket != NULL))
    {
		SSM_DEBUG("Shutting down data connection abnormally (rv == %d).\n",
                  SSMRESOURCE(conn)->m_status);
        SSM_DEBUG("shutting down client socket.\n");
        PR_Shutdown(conn->m_clientSocket, PR_SHUTDOWN_SEND);

        /* if this is called by a control thread, send a message to the
         * data service thread to shut down
         */
        if ((closer != conn->m_dataServiceThread) && 
            (conn->m_shutdownQ != NULL)) {
            SSM_DEBUG("Send shutdown msg to data Q.\n");
            trv = SSM_SendQMessage(conn->m_shutdownQ, SSM_PRIORITY_SHUTDOWN,
                                   SSM_DATA_PROVIDER_SHUTDOWN, 0, NULL,
                                   PR_TRUE);
        }

        if (conn->m_dataServiceThread) {
            PR_Interrupt(conn->m_dataServiceThread);
        }
    }
    
    /* If the client sockets is/are now unused, close them. */
    if (SSMRESOURCE(conn)->m_threadCount == 0)
    {
        /* Close the client socket with linger */
        if (conn->m_clientSocket)
        {
            SSM_DEBUG("Close data socket.\n");
            trv = PR_Close(conn->m_clientSocket);
            conn->m_clientSocket = NULL;
            SSM_DEBUG("Closed client socket (rv == %d).\n",trv);

            /* keep track of the number of data connections pending. */
            gNumDataConnections--;
            PR_ASSERT(gNumDataConnections >= 0);
        }
    }

    SSM_UnlockResource(SSMRESOURCE(conn));

    return rv; /* so that subclasses know to perform shutdown */
}

SSMStatus 
SSMDataConnection_Authenticate(SSMConnection *arg, char *nonce)
{
	SSMStatus rv = SSM_FAILURE;
    SSMConnection *parent = arg->m_parent;

	/* Parent has the nonce, so authenticate there. */
	if (parent &&
		SSM_IsAKindOf(&(parent->super), SSM_RESTYPE_CONTROL_CONNECTION))
		rv = SSMControlConnection_Authenticate(parent, nonce);

	return rv;
}
