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
#include "ssmerrs.h"
#include "ctrlconn.h"
#include "prinrval.h"
#include "crmf.h"




#define SSMRESOURCE(conn) (&(conn)->super)

/* 
   ssm_ConnectionCreate creates an SSMConnection object. 
   The value argument is ignored.
*/
SSMStatus
SSMConnection_Create(void *arg, SSMControlConnection * connection, 
                     SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMConnection *conn;
    SSMControlConnection * owner;
    *res = NULL; /* in case we fail */
    
    conn = (SSMConnection *) PR_CALLOC(sizeof(SSMConnection));
    if (!conn) 
       goto loser;
    if (connection) 
        owner = connection;
    else 
        owner = (SSMControlConnection *)conn;
    rv = SSMConnection_Init(owner, (SSMConnection *) arg, 
                            SSM_RESTYPE_CONNECTION);
    if (rv != PR_SUCCESS) goto loser;

    SSMConnection_Invariant(conn);
    
    *res = &conn->super;
    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;

    if (conn) 
    {
		SSM_ShutdownResource(SSMRESOURCE(conn), rv);
      
        SSM_FreeResource(&conn->super);
    }
        
    return rv;
}

/* As a sanity check, make sure we have data structures consistent
   with our type. */
void SSMConnection_Invariant(SSMConnection *conn)
{
#ifdef DEBUG
    if (conn)
    {
        SSMResource_Invariant(&(conn->super));
        SSM_LockResource(SSMRESOURCE(conn));
        PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(conn), SSM_RESTYPE_CONNECTION));
        PR_ASSERT(conn->m_children != NULL);
        SSM_UnlockResource(SSMRESOURCE(conn));

#if 0
        if (conntype == SSM_DATA_CONNECTION)
        {
            PR_ASSERT(!conn->m_children);
            PR_ASSERT(!conn->m_readThread);
            PR_ASSERT(!conn->m_writeThread);
            PR_ASSERT(!conn->m_controlOutQ);
        }
        else /* control connection */
        {
            PR_ASSERT(conn->m_children);
            PR_ASSERT(!conn->m_readClientThread);
            PR_ASSERT(!conn->m_writeClientThread);
            PR_ASSERT(!conn->m_readTargetThread);
            PR_ASSERT(!conn->m_writeTargetThread);
            PR_ASSERT(!conn->m_incomingQ);
            PR_ASSERT(!conn->m_outgoingQ);
            PR_ASSERT(conn->m_resourceID == 0);
        }
#endif
    }
#endif
}

SSMStatus
SSMConnection_DetachChild(SSMConnection *parent, SSMConnection *child)
{
    SSMStatus rv;
    SSM_LockResource(&parent->super);

    /* Remove the child from the parent's collection of children. */
    rv = SSM_Remove(parent->m_children, child);
    child->m_parent = NULL;

    /* If the parent is detaching (due to destruction), free the child. */
    if (&parent->super.m_refCount == 0)
        SSM_FreeResource(&child->super); 

    SSM_UnlockResource(&parent->super);
    return rv;
}

SSMStatus
SSMConnection_AttachChild(SSMConnection *parent, SSMConnection *child)
{
    SSMStatus rv;
    SSM_LockResource(&parent->super);
    rv = SSM_Insert(parent->m_children, SSM_PRIORITY_NORMAL, child, NULL);
    SSM_UnlockResource(&parent->super);
    return rv;
}

SSMStatus
SSMConnection_Shutdown(SSMResource *res, SSMStatus status)
{
    SSMConnection *conn = (SSMConnection *) res;
    SSMStatus rv;

    SSM_LockResource(SSMRESOURCE(conn));
    rv = SSMResource_Shutdown(res, status);

    if (SSMRESOURCE(conn)->m_threadCount == 0)
    {
        SSMConnection *child;

        /* Shut down child connections. */
        SSM_Dequeue(conn->m_children, SSM_PRIORITY_ANY, 
                    (void **) &child, PR_FALSE);
        while(child != NULL)
        {
            SSM_ShutdownResource(&child->super, (SSMStatus) PR_PENDING_INTERRUPT_ERROR);
            SSMConnection_DetachChild(conn, child);
            child = NULL;
        }
    }
    SSM_UnlockResource(SSMRESOURCE(conn));
    
    return rv;
}

SSMStatus
SSMConnection_Destroy(SSMResource *res, PRBool doFree)
{
    SSMStatus rv = PR_SUCCESS;
    
    SSMConnection *conn = (SSMConnection *) res;
    SSMResource *child = NULL;

    /* Destroy the collection of child connections. */
    if (conn->m_children)
    {
        /* Detach all children. */
        do
        {
            SSM_Dequeue(conn->m_children, SSM_PRIORITY_ANY,
                        (void **) &child, PR_FALSE);
            if (child && (child != SSMRESOURCE(conn)))
            {
                SSM_DEBUG("Freeing child connection %lx.\n",
                          (unsigned long) child);
				SSM_FreeResource(child);

            }
        }
        while (child != NULL);
        PR_ASSERT(SSM_Count(conn->m_children) == 0);
        SSM_DestroyCollection(conn->m_children);
    }
    /* If we're attached to a parent, detach from the parent's collection. */
    if (conn->m_parent)
        SSMConnection_DetachChild(conn->m_parent, conn);

    SSMResource_Destroy(res, PR_FALSE);

    if (doFree) {
        PR_Free(res);
    }

    return rv;
}

SSMStatus
SSMConnection_GetAttrIDs(SSMResource *res,
                         SSMAttributeID **ids,
                         PRIntn *count)
{
    SSMStatus rv;

    rv = SSMResource_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS)
        goto loser;

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 2) * sizeof(SSMResourceID));
    if (! *ids) goto loser;

    (*ids)[*count++] = SSM_FID_CONN_ALIVE;
    (*ids)[*count++] = SSM_FID_CONN_PARENT;
    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    return rv;
}

SSMStatus
SSMConnection_GetAttr(SSMResource *res,
                      SSMAttributeID attrID, 
                      SSMResourceAttrType attrType,
                      SSMAttributeValue *value)
{
    SSMConnection *conn = (SSMConnection *) res;
    SSMStatus rv = PR_SUCCESS;

    /* see what it is */
    switch(attrID)
    {
    case SSM_FID_CONN_ALIVE:
        value->u.numeric = (PRUint32) (conn->super.m_threadCount > 0);
        value->type = SSM_NUMERIC_ATTRIBUTE;
        break;

    case SSM_FID_CONN_PARENT:
        rv = SSM_ClientGetResourceReference(SSMRESOURCE(conn->m_parent),
                                            &value->u.rid);
        value->type = SSM_RID_ATTRIBUTE;
        break;

    default:
        rv = SSMResource_GetAttr(res,attrID,attrType,value);
        if (rv != PR_SUCCESS)
            goto loser;
    }

 loser:
    return rv;
}

SSMStatus
SSMConnection_SetAttr(SSMResource *res,
                      SSMAttributeID attrID,
                      SSMAttributeValue *value)
{
    SSMStatus rv;

    /* we own nothing, defer to superclass */
	rv = SSMResource_SetAttr(res,attrID,value);

    return rv;
}

SSMStatus
SSMConnection_Init(SSMControlConnection *ctrl, SSMConnection *connection, 
			SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
    SSMControlConnection * owner;

    if (type == SSM_RESTYPE_CONTROL_CONNECTION) 
	owner = (SSMControlConnection *)connection;
    else owner = ctrl;

    rv = SSMResource_Init(owner, &connection->super, type);
    if (rv != PR_SUCCESS) goto loser;

    /* Attach ourselves to the parent first, then set the member */
    if (ctrl != NULL) {
        rv = SSMConnection_AttachChild(&(ctrl->super), connection);
        if (rv != PR_SUCCESS) 
            goto loser;
    }
    connection->m_parent = &(ctrl->super);
   
    
    connection->m_children = SSM_NewCollection();
    if (!connection->m_children) goto loser;

    /* hang our hooks */
	connection->m_auth_func = SSMConnection_Authenticate;

    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
    return rv;
}

SSMStatus SSMConnection_Authenticate(SSMConnection *conn, char *nonce)
{
	/* Abstract method, report failure by default */
	return SSM_FAILURE;
}

/* Given a RID and a proposed nonce, see if the connection exists
   and if the nonce matches. */
SSMStatus SSM_AuthenticateConnection(SSMControlConnection * ctrl, 
                                     SSMResourceID rid, char *nonce)
{
	SSMResource *obj = NULL;
	SSMConnection *conn;
    SSMStatus rv;

	/* Find the connection object. */
	rv = (SSMStatus) SSMControlConnection_GetResource(ctrl, rid, &obj);
	if (rv != SSM_SUCCESS)
		goto loser;
	PR_ASSERT(obj);

	SSM_LockResource(obj);

	if (!SSM_IsAKindOf(obj, SSM_RESTYPE_CONNECTION))
		goto loser;
	
	conn = (SSMConnection *) obj;
	if (!conn->m_auth_func)
		goto loser;

	rv = (*conn->m_auth_func)(conn, nonce);
	
	goto done;
 loser:
	if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
	if (obj) SSM_UnlockResource(obj);
	return rv;
}

