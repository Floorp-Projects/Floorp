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
#include "p7cinfo.h"
#include "p7econn.h"
#include "p7dconn.h"
#include "secmime.h"
#include "hashconn.h"
#include "certres.h"
#include "cert.h"
#include "certdb.h"
#include "cdbhdl.h"
#include "servimpl.h"
#include "newproto.h"
#include "messages.h"
#include "serv.h"
#include "ssmerrs.h"
#include "minihttp.h"
#include "secmod.h"
#include "kgenctxt.h"
#include "advisor.h"
#include "processmsg.h"
#include "signtextres.h"
#include "p12res.h"
#include "p12plcy.h"
#include "secmime.h"
#include "ciferfam.h"
#include "profile.h"
#include "prefs.h"
#include "ocsp.h"
#include "msgthread.h"
#include "nlslayer.h"

#ifdef XP_MAC
#include "macshell.h"
#endif

/*
 * The structure passed to get an attribute for the control connection,
 * which may require a password prompt.
 */
typedef struct GetAttrArgStr {
    SSMResource *res;
    SSMAttributeID attrID;
    SSMResourceAttrType attrType;
} GetAttrArg;

static SSMStatus
ssmcontrolconnection_encodegetattr_reply(SECItem *msg, SSMStatus rv, 
                                         SSMAttributeValue *value,
                                         SSMResourceAttrType attrType);


/* The ONLY reason why we can use these macros for both control and
   data connections is that they inherit from the same superclass.
   Do NOT try this at home. */
#define SSMCONNECTION(c) (&(c)->super)
#define SSMRESOURCE(c) (&(c)->super.super)

/* Special resource id values */
#define SSM_BASE_RID  0x00000003
#define SSM_MAX_RID   0x0FFFFFFF


static long ssm_ctrl_count = 0;
static SSMResourceID ssm_next_ctrlrid = SSM_MAX_RID;
static char * gUserDir = NULL;

static PRMonitor *policySetLock = NULL;
static PRBool policySet = PR_FALSE;

static char *gLoadableRootsModuleName = NULL;

SSMStatus SSM_InitPolicyHandler(void)
{
    policySetLock = PR_NewMonitor();
    if (policySetLock == NULL) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}


#ifdef TIMEBOMB
#include "timebomb.h"
PRBool SSMTimeBombExpired = PR_FALSE;
#endif


SSMStatus SSMControlConnection_Create(void *arg, 
                                     SSMControlConnection * connection, 
                                     SSMResource **res)
{
    SSMStatus rv = PR_SUCCESS;
    SSMControlConnection *conn;
    *res = NULL; /* in case we fail */
    
    conn = (SSMControlConnection *) PR_CALLOC(sizeof(SSMControlConnection));
    if (!conn) goto loser;
    
    SSMRESOURCE(conn)->m_connection = conn;
    rv = SSMControlConnection_Init(conn, SSM_RESTYPE_CONTROL_CONNECTION, 
                                   (PRFileDesc *) arg);
    if (rv != PR_SUCCESS) 
        goto loser;
   
    SSMControlConnection_Invariant(conn);

    ssm_ctrl_count++;
    SSM_DEBUG("Control count is now %ld.\n", ssm_ctrl_count);

    *res = SSMRESOURCE(conn);
    rv = SSM_HashInsert(ctrlConnections, (SSMHashKey)(*res)->m_id, 
                        (void *)*res);
    if (rv != PR_SUCCESS) 
        goto loser;

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

#ifdef XP_MAC
int
toascii(char c)
{
	return (int) c;
}
#endif

SSMStatus SSMControlConnection_GenerateNonce(SSMControlConnection *conn)
{
    SSMStatus rv = PR_FAILURE;
    SECStatus srv;
    char* buf = NULL;
    char* n = NULL;
    const int NONCE_SIZE = 8;
    int i;

    conn->m_nonce = NULL; /* in case of failure */
    buf = (char*)PR_CALLOC(NONCE_SIZE);
    if (buf == NULL) {
        goto loser;
    }
    srv = RNG_GenerateGlobalRandomBytes(buf, NONCE_SIZE);
    if (srv != SECSuccess) {
        goto loser;
    }

    n = PL_strdup("nonce");
    for (i = 0; i < NONCE_SIZE; i++) {
        n = PR_sprintf_append(n, "%d", (int)toascii(buf[i]));
        if (n == NULL) {
            goto loser;
        }
    }
    conn->m_nonce = n;
    rv = PR_SUCCESS;
loser:
    if (buf != NULL) {
        PR_Free(buf);
    }
    return rv;
}

SSMStatus SSMControlConnection_Init(SSMControlConnection *conn, 
                                   SSMResourceType type,
                                   PRFileDesc *socket)
{
    SSMStatus rv = PR_SUCCESS;
    PRBool locked = PR_FALSE;
    PRNetAddr dataAddr;

    rv = SSM_HashCreate(&conn->m_resourceDB);
    if (rv != PR_SUCCESS || !conn->m_resourceDB) 
        goto loser;
    /* We need to see if there are any other control connections already
     * established.  If there are, then we need to use the next RID 
     * as ours so the correct control connection is found for UI events.
     * Before all control connections would get the RID of 3, and that would
     * cause problems.
     */
    conn->m_lastRID = ssm_next_ctrlrid;

    conn->m_secAdvisorList = NULL;
    rv = SSM_HashCreate(&conn->m_resourceIdDB);
    if (rv != PR_SUCCESS || conn->m_resourceIdDB == NULL)
        goto loser;

    rv = SSMConnection_Init(NULL, &conn->super, type);
    if (rv != PR_SUCCESS) goto loser;

    ssm_next_ctrlrid = conn->super.super.m_id;

    SSM_LockResource(SSMRESOURCE(conn));
    locked = PR_TRUE;

    /* Current version. Allow this to drop when the Hello request comes in. */
    conn->m_version = SSM_PROTOCOL_VERSION;

    /* Generate a nonce. */
    rv = SSMControlConnection_GenerateNonce(conn);
    if (rv != PR_SUCCESS) goto loser;
    SSM_DEBUG("Generated nonce of `%s'.\n",conn->m_nonce);

    conn->m_controlOutQ = SSM_NewCollection();
    if (!conn->m_controlOutQ) 
        goto loser;

    conn->m_socket = socket;

    /* Create the data socket */
    conn->m_dataSocket = SSM_OpenPort();
    if (!conn->m_dataSocket) {
        goto loser;
    }

    /* Get the data port */
    rv = PR_GetSockName(conn->m_dataSocket, &dataAddr);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    conn->m_dataPort = PR_ntohs(dataAddr.inet.port);

    SSMCONNECTION(conn)->m_auth_func = SSMControlConnection_Authenticate;
    
    /* Creat password handling stuff:temporary and long-term password tables */
    rv = SSM_HashCreate(&conn->m_passwdTable);
    if (rv != PR_SUCCESS || !conn->m_passwdTable) 
        goto loser;
    conn->m_passwdLock = PR_NewMonitor();
    if (!conn->m_passwdLock) 
        goto loser;
    conn->m_waiting = 0;
    rv = SSM_HashCreate(&conn->m_encrPasswdTable);
    if (rv != PR_SUCCESS || !conn->m_encrPasswdTable)
        goto loser;
    conn->m_encrPasswdLock = PR_NewMonitor();
    if (!conn->m_encrPasswdLock)
        goto loser;

    /* database for cert look-up by cert ID */
    rv = SSM_HashCreate(&conn->m_certIdDB);
    if (rv != PR_SUCCESS || !conn->m_certIdDB)
        goto loser;

    conn->m_prefs = PREF_NewPrefs();
    if (conn->m_prefs == NULL) {
        goto loser;
    }

    conn->m_doesUI = PR_FALSE;

    /* Spin threads after we set the shutdown function. */
    SSM_DEBUG("spawning read msg thread for %lx.\n", (long) conn);

#ifdef ALLOW_STANDALONE
    if (!standalone)
    {
#endif
        /* Get reference for front end thread */
        SSM_GetResourceReference(SSMRESOURCE(conn));
        conn->m_frontEndThread = SSM_CreateThread(SSMRESOURCE(conn),
                                                  SSM_FrontEndThread);
        if (conn->m_frontEndThread == NULL) 
            goto loser;

#ifdef ALLOW_STANDALONE
    }
#endif

    SSM_UnlockResource(SSMRESOURCE(conn));

    return PR_SUCCESS;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    if (locked)
        SSM_UnlockResource(SSMRESOURCE(conn));
    if (socket)
    {
        PR_Close(socket); /* close this */
        conn->m_socket = NULL;
    }
    if (conn->m_passwdLock) 
        PR_DestroyMonitor(conn->m_passwdLock);
    if (conn->m_passwdTable) 
        SSM_HashDestroy(conn->m_passwdTable);
    if (conn->m_encrPasswdLock) 
        PR_DestroyMonitor(conn->m_encrPasswdLock);
    if (conn->m_encrPasswdTable) 
        SSM_HashDestroy(conn->m_encrPasswdTable);
    if (conn->m_certIdDB) 
        SSM_HashDestroy(conn->m_certIdDB);
    if (conn->m_prefs)
        PREF_ClosePrefs(conn->m_prefs);
    if (conn->m_resourceDB) 
        SSM_HashDestroy(conn->m_resourceDB);
    if (conn->m_resourceIdDB)
        SSM_HashDestroy(conn->m_resourceIdDB);
 
    return rv;
}

SSMStatus SSMControlConnection_Shutdown(SSMResource *arg, SSMStatus status)
{
    SSMStatus rv, trv; /* rv propagates superclass shutdown */
    PRThread *closer = PR_GetCurrentThread();

    SSMControlConnection *conn = (SSMControlConnection *) arg;
    SSMControlConnection_Invariant(conn);
 
#ifdef TIMEBOMB
    if (SSMTimeBombExpired)
      return;
#endif

    SSM_LockResource(arg);
    
    /* if the thread calling this routine is a service thread,
       clear its place in the connection object. this is a
       prelude to the wakeup call just below. */
    arg->m_threadCount--; /* decrement if service thread */
    if (closer == conn->m_writeThread)
        conn->m_writeThread = NULL;
    if (closer == conn->m_frontEndThread)
        conn->m_frontEndThread = NULL;
    else
        arg->m_threadCount++; /* not a service thread, restore thread count */

    /* shut down our base class. */
    rv = SSMConnection_Shutdown(arg, status);

    /* wake up threads if this is the first time throwing an error */
    if ((arg->m_status != PR_SUCCESS) &&
        (rv != SSM_ERR_ALREADY_SHUT_DOWN))
    {
		SSM_DEBUG("First time aborting control connection.\n");
		SSM_DEBUG("Posting shutdown msgs to queues.\n");
        /* close queues that our threads may be listening on */
        if (conn->m_controlOutQ)
            SSM_SendQMessage(conn->m_controlOutQ, 
                                  SSM_PRIORITY_SHUTDOWN,
                                  SSM_DATA_PROVIDER_SHUTDOWN,
                                  0, NULL, PR_TRUE);

        if (conn->m_writeThread) PR_Interrupt(conn->m_writeThread);
        if (conn->m_frontEndThread) PR_Interrupt(conn->m_frontEndThread);
    }

    /* If the front end thread is down, close the client socket */
    if ((!conn->m_frontEndThread) && (conn->m_socket))
    {
        /* Got a socket but nothing to work on it with. Close the socket. */
#ifndef XP_UNIX
	/* Don't close socket with linger on UNIX since the
	 * control socket on UNIX is a UNIX domain socket.
	 */
        SSM_DEBUG("Closing control socket with linger.\n");
        trv = SSM_CloseSocketWithLinger(conn->m_socket);
#else
	SSM_DEBUG("Closing control socket.\n");
	trv = PR_Close(conn->m_socket);
#endif
        PR_ASSERT(trv == PR_SUCCESS);
        conn->m_socket = NULL; /* don't try closing more than once */
        SSM_DEBUG("Closed control socket (rv == %d).\n",rv);
    }

	if (SSMRESOURCE(conn)->m_threadCount == 0)
	{
		/* All service threads are down. Shut down NSS. */
		ssm_ShutdownNSS(conn);
	}

    SSM_UnlockResource(arg);
    return rv;
}


SSMStatus SSMControlConnection_Destroy(SSMResource *res, 
                                      PRBool doFree)
{
    SSMControlConnection *conn = (SSMControlConnection *) res;
    void *value;

    /* Drain and destroy the queue. */
    if (conn->m_controlOutQ)
        ssm_DrainAndDestroyQueue(&(conn->m_controlOutQ));


    /* Free our fields. */
    PR_FREEIF(conn->m_nonce);
    PR_FREEIF(conn->m_profileName);
    PR_FREEIF(conn->m_dirRoot);

    PR_DestroyMonitor(conn->m_passwdLock);
    PR_DestroyMonitor(conn->m_encrPasswdLock);
    SSM_HashDestroy(conn->m_passwdTable);
    SSM_HashDestroy(conn->m_encrPasswdTable);
    PREF_ClosePrefs(conn->m_prefs);
    
    /* log out all pk11 slots for now */
    if (conn->m_pkcs11Init) {
        PK11_LogoutAll();
    }

    if (conn->m_secAdvisorList)
      SECITEM_ZfreeItem(conn->m_secAdvisorList, PR_TRUE);

    /* Destroy superclass fields. */
    SSMConnection_Destroy(SSMRESOURCE(conn), PR_FALSE);

    SSM_HashRemove(ctrlConnections, 
                   (SSMHashKey)(conn->super.super.m_id), &value);
                                
    /* If this is the last control connection, quit. */
    --ssm_ctrl_count;
    SSM_DEBUG("Control count is now %ld.\n", ssm_ctrl_count);
    if ((ssm_ctrl_count <= 0)
#ifdef DEBUG
        && (PR_GetEnv("NSM_SUPPRESS_EXIT") == NULL)
#endif
        )
    {
        SSM_DEBUG("Last control connection gone, quitting.\n");
#ifdef XP_UNIX
	SSM_ReleaseLockFile();
#endif
#ifdef XP_MAC
		SetQuitFlag(PR_TRUE); // tell primordial thread to quit
#else
        exit(0);
#endif
    }
    /* Free the connection object if asked. */
    if (doFree)
        PR_DELETE(conn);

    return PR_SUCCESS;
}

SSMStatus SSMControlConnection_GetAttrIDs(SSMResource* res,
										 SSMAttributeID** ids, PRIntn* count)
{
    SSMStatus rv;

	if (res == NULL || ids == NULL || count == NULL) {
		goto loser;
	}
    rv = SSMConnection_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS) {
        goto loser;
	}

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 2)*sizeof(SSMAttributeID));
    if (!*ids) {
		goto loser;
	}

    (*ids)[*count++] = SSM_FID_DEFAULT_EMAIL_SIGNER_CERT;
    (*ids)[*count++] = SSM_FID_DEFAULT_EMAIL_RECIPIENT_CERT;

    goto done;
loser:
    if (rv == PR_SUCCESS) {
		rv = PR_FAILURE;
	}
done:
    return rv;
}

static void ssmcontrolconnection_getattr_thread(void* inArg)
{
    GetAttrArg *arg = (GetAttrArg*)inArg;
    SSMResource *res = arg->res;
    SSMAttributeID attrID = arg->attrID;
    SSMResourceAttrType attrType = arg->attrType;
    SSMControlConnection* conn = (SSMControlConnection*)res;
    SSMStatus rv = PR_SUCCESS;
    CERTCertificate *cert = NULL;
    SSMResourceCert *certRes;
    SSMResourceID certID = 0;
    char *certNickname = NULL;
    PRBool locked = PR_FALSE;
    SSMAttributeValue realValue, *value;
    SECItem msg;

    value = &realValue;
    /* see what it is */
#ifdef DEBUG
    SSM_RegisterThread("ctrlconn getattr",NULL);
#endif    
    switch(attrID) {
    case SSM_FID_DEFAULT_EMAIL_SIGNER_CERT:
        SSM_LockResource(SSMRESOURCE(conn));
        locked = PR_TRUE;
        rv = PREF_GetStringPref(conn->m_prefs, "security.default_mail_cert", 
                                &certNickname);
        if (rv != PR_SUCCESS) {
            goto loser;
        }

        if (certNickname) {
            cert = CERT_FindUserCertByUsage(conn->m_certdb,
                                            certNickname,
                                            certUsageEmailSigner,
                                            PR_FALSE,
                                            conn);
            if (cert) {
                SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, cert, conn,
                                   &certID, (SSMResource**)&certRes);
                rv = SSM_ClientGetResourceReference(&certRes->super, &certID);
                SSM_FreeResource(&certRes->super);
                if (rv != SSM_SUCCESS)
                    goto loser;
            }
        }
        if (cert == NULL)
            goto loser;

        value->u.rid = certID;
        value->type = SSM_RID_ATTRIBUTE;
        SSM_UnlockResource(SSMRESOURCE(conn));
        locked = PR_FALSE;
        break;

    case SSM_FID_DEFAULT_EMAIL_RECIPIENT_CERT:
        SSM_LockResource(SSMRESOURCE(conn));
        locked = PR_TRUE;
        rv = PREF_GetStringPref(conn->m_prefs, "security.default_mail_cert", 
                                &certNickname);
        if (rv != PR_SUCCESS) {
            goto loser;
        }

        if (certNickname) {
            cert = CERT_FindUserCertByUsage(conn->m_certdb,
                                            certNickname,
                                            certUsageEmailRecipient,
                                            PR_FALSE,
                                            conn);
            if (cert) {
                SSM_CreateResource(SSM_RESTYPE_CERTIFICATE, cert, conn,
                                   &certID, (SSMResource**)&certRes);
                rv = SSM_ClientGetResourceReference(&certRes->super, &certID);
                SSM_FreeResource(&certRes->super);
                if (rv != SSM_SUCCESS)
                    goto loser;
            }
        }
        value->u.rid = certID;
        value->type = SSM_RID_ATTRIBUTE;
        SSM_UnlockResource(SSMRESOURCE(conn));
        locked = PR_FALSE;
		break;
    default:
        rv = SSMConnection_GetAttr(res, attrID, attrType, value);
        if (rv != PR_SUCCESS) {
			goto loser;
		}
    }
    if (value->type != attrType) {
        goto loser;
    }
    goto done;
loser:
    value->type = SSM_NO_ATTRIBUTE;
    if (rv == PR_SUCCESS) {
		rv = PR_FAILURE;
	}
done:
    if (locked) {
        SSM_UnlockResource(SSMRESOURCE(conn));
    }
    rv = ssmcontrolconnection_encodegetattr_reply(&msg, rv, value,
                                                  attrType);
    if (rv != SSM_SUCCESS) {
        rv = ssmcontrolconnection_encode_err_reply(&msg, rv);
        PR_ASSERT(rv == SSM_SUCCESS);
    }
    ssmcontrolconnection_send_message_to_client(conn, &msg);
    PR_FREEIF(msg.data);
    SSM_FreeResource(res);
}

SSMStatus SSMControlConnection_GetAttr(SSMResource *res, SSMAttributeID attrID,
                                       SSMResourceAttrType attrType,
                                       SSMAttributeValue *value)
{
    GetAttrArg *arg = NULL;
    if (res == NULL || value == NULL) {
        goto loser;
    }
    arg = SSM_NEW(GetAttrArg);
    if (arg == NULL) {
        goto loser;
    }
    arg->res = res;
    arg->attrID = attrID;
    arg->attrType = attrType;

    if (SSM_CreateAndRegisterThread(PR_USER_THREAD, ssmcontrolconnection_getattr_thread, 
                        (void *)arg, PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                        PR_UNJOINABLE_THREAD, 0) == NULL) {
        goto loser;
    }
    return SSM_ERR_DEFER_RESPONSE;
 loser:
    if (arg != NULL) {
        PR_Free(arg);
    }
    return SSM_FAILURE;
}

void
SSMControlConnection_Invariant(SSMControlConnection *conn)
{
    if (conn)
    {
        SSMConnection_Invariant(SSMCONNECTION(conn));
        SSM_LockResource(SSMRESOURCE(conn));
        PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(conn), SSM_RESTYPE_CONTROL_CONNECTION));
        PR_ASSERT(conn->m_controlOutQ != NULL);
        SSM_UnlockResource(SSMRESOURCE(conn));
    }
}

void SSMControlConnection_RecycleItem(SECItem* msg)
{
    PR_ASSERT(msg != NULL);

    if (msg->data != NULL) {
        cmt_free(msg->data);
        msg->data = NULL;
    }
    return;
}



/*
 * Read msgs from the control connection queue and send them back to client
 */
void SSM_WriteCtrlThread(void * arg)
{
    PRIntn sent, len, type;
    SSMControlConnection * ctrl    = NULL;
    SSMStatus rv= PR_FAILURE;
    char * data;

#ifdef TIMEBOMB
    if (SSMTimeBombExpired)
      return;
#endif 

    ctrl = (SSMControlConnection *)arg;
    SSM_RegisterNewThread("ctrl write", (SSMResource*) arg);
    SSM_DEBUG("initializing.\n");
    if (!ctrl)
    {
        rv = (SSMStatus) PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }
    if (!ctrl->m_socket)     
    {
        rv = (SSMStatus) PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }
    ctrl->m_writeThread = PR_GetCurrentThread();

    /* wait for SSM_DATA_PROVIDER_OPEN message */
    rv = SSM_RecvQMessage(ctrl->m_controlOutQ, 
                          SSM_PRIORITY_ANY,
                          &type, &len, &data, 
                          PR_TRUE);
    if (rv != PR_SUCCESS || type != SSM_DATA_PROVIDER_OPEN)
        goto loser;
    SSM_DEBUG("got queue open message.\n");
  
    /* look for data in the incoming queue and send it to the client */
    while ((SSMRESOURCE(ctrl)->m_status == PR_SUCCESS) && (rv == PR_SUCCESS))
    {
        rv = SSM_RecvQMessage(ctrl->m_controlOutQ, 
                              SSM_PRIORITY_ANY,
                              &type, &len, &data, 
                              PR_TRUE);
        if (rv != PR_SUCCESS)
        {
            SSM_DEBUG("Couldn't read or block on outgoing queue (%d).\n",
		      rv);
            goto loser;
	    }
        switch (type) 
        {
          case SSM_DATA_PROVIDER_SHUTDOWN:
              SSM_DEBUG("got queue close message.\n");
              goto loser;
          default:
              {
                  CMTMessageHeader header;

                  /* got a regular control message. send it to the client */
                  SSM_DEBUG("got message for client (type=%lx,len=%ld).\n", 
                            type, len);
                  header.type = PR_htonl(type);
                  header.len = PR_htonl(len);

                  /* Send the message header */
                  sent = SSM_WriteThisMany(ctrl->m_socket, &header, sizeof(CMTMessageHeader));
                  if (sent != sizeof(CMTMessageHeader)) {
                      rv = (SSMStatus) PR_GetError();
                      SSM_DEBUG("cannot send message type: %d.\n", rv);
                      goto loser;
                  }

                  /* Send the message body */
                  sent = SSM_WriteThisMany(ctrl->m_socket, data, len);
                  if (sent != len) 
                  {
                      rv = (SSMStatus) PR_GetError();
                      SSM_DEBUG("cannot send message data: %d.\n", rv);
                    goto loser;
                  }
                  PR_Free(data);
              }
              break;
        }  /* end of switch */
    } /* end of while alive loop */
  
 loser: 
    if (ctrl)
    {
        SSM_DEBUG("Shutting down, rv = %d.\n", rv);
        SSM_ShutdownResource(SSMRESOURCE(ctrl), rv);
        SSM_FreeResource(SSMRESOURCE(ctrl));
    }
    return;
}


/* 
 * Check if given cert (arg) already exists in our cert resource db.
 */
void SSMControlConnection_CertLookUp(SSMControlConnection * connection, 
                                     void * arg, SSMResource ** res)
{
    PR_ASSERT(res);
    SSM_HashFind(connection->m_certIdDB, (SSMHashKey) arg, (void **)res);
    if (*res != NULL)
        SSM_GetResourceReference(*res);
}

/* NSS_Init does not open the cert and key db's read/write.  Cartman needs
 * them to be opened read/write in order to do key gen's and import 
 * certificate chains.
 *
 * For now, we pretty much re-create the code of NSS_Init in this file.  If
 * NSS_Init is ever modified so that we can open the db's read/write, then
 * we just call the new function from the function SSM_InitNSS.
 */

static char *
ssm_certdb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char*)arg;
    const char *dbver;

    switch (dbVersion) {
    case 7:
      dbver = "7";
      break;
    case 6:
      dbver = "6";
      break;
    case 5:
      dbver = "5";
      break;
    case 4:
    default:
      dbver = "";
      break;
    }

#ifdef XP_MAC
	/* on Mac, :: means parent directory. does non-Mac get // with Seamonkey? */
    if(configdir[PL_strlen(configdir) - 1] == ':') {
    	return PR_smprintf("%sCertificates%s", configdir, dbver);
    } else {
    	return PR_smprintf("%s:Certificates%s", configdir, dbver);
    }
#else
    return PR_smprintf("%s/cert%s.db", configdir, dbver);
#endif
}

static char *ssm_keydb_name_cb(void *arg, int dbVersion)
{
    const char *configdir = (const char*)arg;
    const char *dbver;

    switch (dbVersion) {
    case 3:
      dbver = "3";
      break;
    case 2:
    default:
      dbver = "";
      break;
    }
#ifdef XP_MAC
    if (configdir[PL_strlen(configdir) - 1] == ':') {
    	return PR_smprintf("%sKey Database%s", configdir, dbver);
    } else {
    	return PR_smprintf("%s:Key Database%s", configdir, dbver);
    }
#else
    return PR_smprintf("%s/key%s.db", configdir, dbver);
#endif
}

SECStatus
ssm_OpenCertDB(const char * configdir,SSMControlConnection *ctrl)
{
    CERTCertDBHandle *certdb;
    SECStatus         status = SECFailure;
#ifdef ALLOW_STANDALONE
    PRBool readonly = standalone;
#else
    PRBool readonly = PR_FALSE;
#endif
    
    /* We should not be doing this! */
    if (0) {
        certdb = CERT_GetDefaultCertDB();
        if (certdb) {
            return SECSuccess;
        }
    }
    certdb = PORT_ZNew(CERTCertDBHandle);
    if (certdb == NULL) {
        goto loser;
    }
    status = CERT_OpenCertDB(certdb, readonly, ssm_certdb_name_cb, 
			     (void*)configdir);
    if (status == SECSuccess) {
        CERT_SetDefaultCertDB(certdb);
        ctrl->m_certdb = certdb;
    } else {
        PR_Free(certdb);
	ctrl->m_certdb = NULL;
    }
loser:
    return status;
}

SECStatus
ssm_OpenKeyDB(const char * configdir, SSMControlConnection *ctrl)
{
    SECKEYKeyDBHandle *keydb;
#ifdef ALLOW_STANDALONE
    PRBool readonly = standalone;
#else
    PRBool readonly = PR_FALSE;
#endif
    
    /* we should not be doing this! */
    if (0) { 
        keydb = SECKEY_GetDefaultKeyDB();
        if (keydb) {
            return SECSuccess;
        }
    }
    
    keydb = SECKEY_OpenKeyDB(readonly, ssm_keydb_name_cb, (void*)configdir);
    if (keydb == NULL) {
        ctrl->m_keydb = NULL;
        return SECFailure;
    }
    SECKEY_SetDefaultKeyDB(keydb);
    ctrl->m_keydb = keydb;
    return SECSuccess;
}

SECStatus
ssm_OpenSecModDB(const char * configdir)
{   
	char *secmodname = NULL;
#ifdef XP_UNIX
    secmodname = PR_smprintf("%s/secmodule.db", configdir);
#elif defined(XP_MAC)
    if (configdir[PL_strlen(configdir) - 1] == ':') {
    	secmodname = PR_smprintf("%sSecurity Modules", configdir);
    } else {
    	secmodname = PR_smprintf("%s:Security Modules", configdir);
    }
#else
    secmodname = PR_smprintf("%s/secmod.db", configdir);
#endif
    if (secmodname == NULL) {
        return SECFailure;
    }
    SECMOD_init(secmodname);
    return SECSuccess;
}

void
ssm_ShutdownNSS(SSMControlConnection *ctrl)
{
    if (ctrl->m_certdb != NULL) {
        CERT_ClosePermCertDB(ctrl->m_certdb);
    }
    if (ctrl->m_keydb != NULL) {
        SECKEY_CloseKeyDB(ctrl->m_keydb);
    }
    if (gLoadableRootsModuleName) {
    	int type;
    	
    	SECMOD_DeleteModule(gLoadableRootsModuleName, &type);
    	PR_FREEIF(gLoadableRootsModuleName);
    }
}

#if 0
SSMPolicyType
SSM_ConvertSVRPlcyToSSMPolicy(PRInt32 policy)
{
    return ssmDomestic;
}
#endif

void
SSM_SetP12Export(void)
{
    SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC4_128, 0);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 0);
    SEC_PKCS12EnableCipher(PKCS12_DES_56, 0);
    SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 0);
}

void
SSM_SetSMIMEExport(void)
{
    SECMIME_EnableCipher(SMIME_RC2_CBC_40, 1);
#if 0
    SECMIME_EnableCipher(SMIME_RC2_CBC_64, 0);
    SECMIME_EnableCipher(SMIME_RC2_CBC_128, 0);
    SECMIME_EnableCipher(SMIME_RC5PAD_64_16_40, 1);
    SECMIME_EnableCipher(SMIME_RC5PAD_64_16_64, 0);
    SECMIME_EnableCipher(SMIME_RC5PAD_64_16_128, 0);
    SECMIME_EnableCipher(SMIME_DES_CBC_56, 0);
    SECMIME_EnableCipher(SMIME_DES_EDE3_168, 0);
    SECMIME_EnableCipher(SMIME_FORTEZZA, 0);
#endif
    SECMIME_EnableCipher(CIPHER_FAMILYID_MASK, 0);
}

#ifdef XP_MAC
#ifdef DEBUG
#define LOADABLE_CERTS_MODULE ":Essential Files:NSSckbiDebug.shlb"
#else
#define LOADABLE_CERTS_MODULE ":Essential Files:NSSckbi.shlb"
#endif /*DEBUG*/ 
#endif /*XP_MAC*/

SECStatus
SSM_InitNSS(char* certpath, SSMControlConnection *ctrl, PRInt32 policy)
{
    SECStatus status;
    SECStatus rv = SECFailure;
    PRBool hasRoot = PR_FALSE;
    PK11SlotList *slotList = NULL;
    PK11SlotListElement *listElement;
    SSMTextGenContext *cx = NULL;
    char *modName=NULL, *processDir = NULL, *fullModuleName=NULL;
    int modType;
    SSMStatus srv;

    PR_EnterMonitor(policySetLock);
    if (policySet) {
        SSM_DEBUG("We got another hello request. If this is for "
                  "a different user then the first, bad things may happen\n");
#if 0
        if (reqPolicy != policyType) {
            SSM_DEBUG("Got a hello request with different policy type "
                      "than has already been established.  "
                      "Refusing connection\n");
            rv = SECFailure;
            goto loser;
        }
#endif
    } 
#if 0
    else if (policyType != reqPolicy) {
        SSM_DEBUG("Initial hello message has different policy type than "
                  "the server.  Setting policy to Export\n");
        NSS_SetExportPolicy();
        SSM_SetP12Export();
        SSM_SetSMIMEExport();
        policyType = ssmExport;
    }
#endif
    SSM_DEBUG("First time initializing NSS.\n");


    status = ssm_OpenCertDB(certpath, ctrl);
    if (status != SECSuccess) {
        goto loser;
    }

    status = ssm_OpenKeyDB(certpath, ctrl);
    if (status != SECSuccess) {
        goto loser;
    }

    status = ssm_OpenSecModDB(certpath);
    if (status != SECSuccess) {
        goto loser;
    }
    ctrl->m_pkcs11Init = PR_TRUE;
    rv = SECSuccess;
    PORT_SetUCS2_ASCIIConversionFunction(SSM_UCS2_ASCIIConversion);
    PORT_SetUCS2_UTF8ConversionFunction(SSM_UCS2_UTF8Conversion);
    policySet = PR_TRUE;

    /* set default policy strings */
    CERT_SetCAPolicyStringCallback(SSM_GetCAPolicyString, ctrl);

    /*
     * Load the PKCS#11 module that has the root CA's in it.
     */
     SSM_DEBUG("Trying to load the PKCS#11 module that contains the root certs\n");
    slotList = PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, ctrl); 
	if (slotList) {
	    for (listElement=slotList->head; listElement != NULL; 
	         listElement = listElement->next) {
	         if (PK11_HasRootCerts(listElement->slot)) {
	             hasRoot = PR_TRUE;
	         }    
	    }     
	}
	if (!hasRoot) {
	    srv = SSMTextGen_NewTopLevelContext(NULL, &cx);
	    if (srv != SSM_SUCCESS) {
	        SSM_DEBUG("Couldn't create a new TextGenContext\n");
	        goto loser;
	    }
	    srv = SSM_FindUTF8StringInBundles(cx, "root_certificates", &modName);
	    SSMTextGen_DestroyContext(cx);
	    if (srv != SSM_SUCCESS) {
	        SSM_DEBUG("Couldn't get the value for \"root_certificates\" "
	                  "from properties file\n");
	        goto loser;          
	    }
        processDir = xpcomGetProcessDir();
#ifdef XP_MAC
        if (processDir == NULL) {
            goto loser;
        }
        SSM_DEBUG("I think the process lives in <%s>\n", processDir);
        fullModuleName = PR_smprintf("%s%s", processDir, LOADABLE_CERTS_MODULE);
        fullModuleName = SSM_ConvertMacPathToUnix(fullModuleName);
#else
        fullModuleName = PR_GetLibraryName(processDir, "nssckbi");
#endif
        PR_FREEIF(processDir);
        /* If a module exists with the same name, delete it. */
        SECMOD_DeleteModule(modName, &modType);
        gLoadableRootsModuleName = modName;
        SSM_DEBUG("Will try to load <%s> for root certs.\n",fullModuleName);
	    if (SECMOD_AddNewModule(modName, fullModuleName, 0, 0) != SECSuccess) {
	        SSM_DEBUG("Couldn't load the module at <%s>",fullModuleName);
	    }
	    /*We want to keep the modName around in gLoadableRootsModule 
             *so that we can delete it from secmod.db on the way out.
             */
	    modName = NULL;
	}

loser:
    PR_FREEIF(modName);
    if (slotList) {
        PK11_FreeSlotList(slotList);
    }
    if (rv != SECSuccess) {
        ssm_ShutdownNSS(ctrl);
    }
    PR_ExitMonitor(policySetLock);
    return rv;
}
/* End the section of code shamelessly copied and modified from NSS_Init*/


static int ssm_ask_pref_to_pk11(int asktype)
{
    switch (asktype) {
    case 1:
        return -1;
    case 2:
        return 1;
    }
    return 0;
}

#ifdef XP_MAC
extern OSErr ConvertMacPathToUnixPath(const char *macPath, char **unixPath);
#endif

/* Set up profile and password information. */
SSMStatus
SSMControlConnection_SetupNSS(SSMControlConnection *ctrl, PRInt32 policy)
{
    SSMStatus rv = PR_SUCCESS;
    SECStatus srv = SECSuccess;
    PRFileInfo info;
#ifdef XP_MAC
	char *unixPath;
#endif

    /* check the profile directory */
#ifdef XP_UNIX
    /* we expect a non-empty string here: bail if we don't have one */
    if ((PR_GetFileInfo(ctrl->m_dirRoot, &info) != PR_SUCCESS) ||
        (info.type != PR_FILE_DIRECTORY)) {
        SSM_DEBUG("Cannot find a profile in %s.\n", ctrl->m_dirRoot);
        goto loser;
    }
#elif defined(XP_MAC)
	/* we expect a path that is already Macified. */
	ConvertMacPathToUnixPath(ctrl->m_dirRoot, &unixPath);
	if (!unixPath)
		goto loser;
	if (unixPath[0] != '\0')
	{
        /* profile directory was supplied: check to make sure it exists */
        if ((PR_GetFileInfo(unixPath, &info) != PR_SUCCESS) ||
            (info.type != PR_FILE_DIRECTORY)) {
            SSM_DEBUG("Cannot find a profile in %s.\n", ctrl->m_dirRoot);
            goto loser;
        }
    }
    else 
		goto loser; /* for now */
	/* ### mwelch - we're leaking unixPath for now */
#else
    if (ctrl->m_dirRoot[0] != '\0') {
        /* profile directory was supplied: check to make sure it exists */
        if ((PR_GetFileInfo(ctrl->m_dirRoot, &info) != PR_SUCCESS) ||
            (info.type != PR_FILE_DIRECTORY)) {
            SSM_DEBUG("Cannot find a profile in %s.\n", ctrl->m_dirRoot);
            goto loser;
        }
    }
    else {
        /* profile directory was not supplied: get the "default" directory */
        if (ctrl->m_profileName[0] == '\0') {
            /* this is (almost always) a 3rd party application on win32 that
             * wants to use a "default" profile
             * set it to "Default" and hope for the best
             */
            ctrl->m_profileName = PL_strdup("Default");
        }
        ctrl->m_dirRoot = SSM_PROF_GetProfileDirectory(ctrl);
        if (ctrl->m_dirRoot == NULL) {
            goto loser;
        }
    }
#endif

    /* Do the libsec initialization */
    
    if (gUserDir != NULL) {
#ifdef XP_MAC
		ctrl->m_certdb = CERT_GetDefaultCertDB();
        ctrl->m_keydb  = SECKEY_GetDefaultKeyDB();
        ssm_ShutdownNSS(ctrl);
        free(gUserDir);
        ctrl->m_certdb = NULL;
        ctrl->m_keydb  = NULL;
        gUserDir = strdup(ctrl->m_dirRoot);
        srv = SSM_InitNSS(ctrl->m_dirRoot, ctrl, policy);
        if (srv != SECSuccess) {
            goto loser;
        }
#else
#ifndef WIN32
        if (strcmp(ctrl->m_dirRoot, gUserDir) != 0) {
            goto loser;
        }    
#else 
        if (STRNCASECMP(ctrl->m_dirRoot, gUserDir, strlen(gUserDir))) {
            goto loser;
        }
#endif  
        /*
         * This is the best way to get the current key and cert dbs.
         * When NSS fully supports multiple profiles, we'll have to do 
         * away with this call.
         */
        ctrl->m_certdb = CERT_GetDefaultCertDB();
        ctrl->m_keydb  = SECKEY_GetDefaultKeyDB();
#endif        
    } else {
        gUserDir = strdup(ctrl->m_dirRoot);
        /*
         * We only want to initialize NSS once.
         */
        srv = SSM_InitNSS(ctrl->m_dirRoot, ctrl, policy);
        if (srv != SECSuccess) {
            goto loser;
        }
    }

#ifdef ALLOW_STANDALONE
    if (standalone)
    {
        /* Set password callback to be a function which uses a
           preset password. Very insecure, which is why this is
           set in debug code only. */
        PK11_SetPasswordFunc(SSM_StandaloneGetPasswdCallback);
        PK11_SetVerifyPasswordFunc(SSM_StandaloneVerifyPasswdCallback);
    }
    else
    {
#endif
        /* set passwd callback */
        PK11_SetPasswordFunc(SSM_GetPasswdCallback);
        /* verify passwd */
        PK11_SetVerifyPasswordFunc(SSM_VerifyPasswdCallback);
#ifdef ALLOW_STANDALONE
    }
#endif

    goto done;

 loser:
    /* Gather the error from wherever it came */
    if (rv == PR_SUCCESS)
        rv = (srv == SECSuccess) ? PR_SUCCESS : PR_FAILURE;
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    
 done:
    return rv;
}

SSMStatus SSMControlConnection_ProcessHello(SSMControlConnection *ctrl, SECItem *msg)
{
    SSMStatus rv = PR_SUCCESS;
    HelloRequest request;
    HelloReply reply;

    /* parse client info and store it in the connection object 
     */
#ifdef NSM_PROTOCOL_STRICT
    /* we should only ever get one hello request */
    PR_ASSERT(ctrl->m_profileName == NULL);
#endif
    if (CMT_DecodeMessage(HelloRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    /* allow earlier client libs to try to talk to us, but not later ones */
    if (request.version > ctrl->m_version) {
        goto loser;
    }
    ctrl->m_doesUI = request.doesUI;
    ctrl->m_profileName = request.profile;

    if (request.profileDir == NULL) {
        /* it may be an empty string but it should not be NULL */
        goto loser;
    }

    ctrl->m_dirRoot = request.profileDir;
    SSM_DEBUG("Selected profile directory <%s>.\n", ctrl->m_dirRoot);

    msg->data = NULL;
    rv = SSMControlConnection_SetupNSS(ctrl, request.policy);

	/* Create the auto-renewal thread */
	ctrl->m_certRenewalThread = SSM_CreateThread(SSMRESOURCE(ctrl),
					SSM_CertificateRenewalThread);
    goto done;

 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    
 done:
    /* construct HELLO_REPLY message
     */
    SSM_DEBUG("composing Hello reply.\n");
    reply.result = rv;
    reply.sessionID = SSMRESOURCE(ctrl)->m_id;
    reply.version = ctrl->m_version;
    reply.httpPort = SSM_GetHTTPPort();
    reply.nonce.len = strlen(ctrl->m_nonce);
    reply.nonce.data = (unsigned char *) ctrl->m_nonce;
    reply.policy = SSM_GetPolicy();
    reply.stringVersion = SSMVersionString;
    if (rv == PR_SUCCESS) {
        msg->type = (SECItemType) (SSM_HELLO_MESSAGE | SSM_REPLY_OK_MESSAGE);
    }
    else {
        msg->type = (SECItemType) (SSM_HELLO_MESSAGE | SSM_REPLY_ERR_MESSAGE);
    }
    CMT_EncodeMessage(HelloReplyTemplate, (CMTItem*)msg, &reply);
    return rv;
}


typedef struct {
    char* pref; /* pref key */
    long id; /* cipher ID for NSS */
} SSMCipherPref;

/* cipher suites are listed in the order of decreasing preference in each
 * cipher family
 */
SSMCipherPref SSMSSLCiphers[] = {
    /* SSL2 ciphers */
    {"security.ssl2.rc4_128", SSL_EN_RC4_128_WITH_MD5},
    {"security.ssl2.rc2_128", SSL_EN_RC2_128_CBC_WITH_MD5},
    {"security.ssl2.des_ede3_192", SSL_EN_DES_192_EDE3_CBC_WITH_MD5},
    {"security.ssl2.des_64", SSL_EN_DES_64_CBC_WITH_MD5},
    {"security.ssl2.rc4_40", SSL_EN_RC4_128_EXPORT40_WITH_MD5},
    {"security.ssl2.rc2_40", SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5},
    /* SSL3 ciphers */
    {"security.ssl3.fortezza_fortezza_sha", 
     SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA},
    {"security.ssl3.fortezza_rc4_sha", SSL_FORTEZZA_DMS_WITH_RC4_128_SHA},
    {"security.ssl3.rsa_rc4_128_md5", SSL_RSA_WITH_RC4_128_MD5},
    {"security.ssl3.rsa_fips_des_ede3_sha", 
     SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA},
    {"security.ssl3.rsa_des_ede3_sha", SSL_RSA_WITH_3DES_EDE_CBC_SHA},
    {"security.ssl3.rsa_fips_des_sha", SSL_RSA_FIPS_WITH_DES_CBC_SHA},
    {"security.ssl3.rsa_des_sha", SSL_RSA_WITH_DES_CBC_SHA},
    {"security.ssl3.rsa_1024_rc4_56_sha",
     TLS_RSA_EXPORT1024_WITH_RC4_56_SHA},
    {"security.ssl3.rsa_1024_des_cbc_sha",
     TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA},
    {"security.ssl3.rsa_rc4_40_md5", SSL_RSA_EXPORT_WITH_RC4_40_MD5},
    {"security.ssl3.rsa_rc2_40_md5", SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5},
    {"security.ssl3.fortezza_null_sha", SSL_FORTEZZA_DMS_WITH_NULL_SHA},
    {"security.ssl3.rsa_null_md5", SSL_RSA_WITH_NULL_MD5},
    {NULL, 0} /* end marker */
};

SSMCipherPref SSMSMIMECiphers[] = {
    /* SMIME bulk ciphers */
    {"security.smime.fortezza", SMIME_FORTEZZA},
    {"security.smime.des_ede3", SMIME_DES_EDE3_168},
    {"security.smime.rc2_128", SMIME_RC2_CBC_128},
    {"security.smime.des", SMIME_DES_CBC_56},
    {"security.smime.rc2_64", SMIME_RC2_CBC_64},
    {"security.smime.rc2_40", SMIME_RC2_CBC_40},
    {NULL, 0} /* end marker */
};

static void ssm_enable_ssl_cipher_prefs(SSMControlConnection* ctrl)
{
    int i;
    PRBool boolVal = PR_TRUE;

    for (i = 0; SSMSSLCiphers[i].pref != NULL; i++) {
        if ((PREF_GetBoolPref(ctrl->m_prefs, SSMSSLCiphers[i].pref, 
                              &boolVal) == PR_SUCCESS) && 
            (boolVal == PR_FALSE)) {
            /* we only have to disable a cipher, not enable one, because 
             * prefs only restrict ciphers further over the policies
             */
            SSL_EnableCipher(SSMSSLCiphers[i].id, boolVal);
        }
    }
}

static void ssm_enable_smime_cipher_prefs(SSMControlConnection* ctrl)
{
    int i;
    PRBool boolVal = PR_TRUE;

    for (i = 0; SSMSMIMECiphers[i].pref != NULL; i++) {
        if ((PREF_GetBoolPref(ctrl->m_prefs, SSMSMIMECiphers[i].pref, 
                              &boolVal) == PR_SUCCESS) && 
            (boolVal == PR_FALSE)) {
            SECMIME_EnableCipher(SSMSMIMECiphers[i].id, boolVal);
        }
    }
}

static SSMStatus ssm_enable_security_prefs(SSMControlConnection* ctrl)
{
    PRBool prefval;
    PRIntn ask;
    PRIntn timeout;
    PK11SlotInfo* slot = NULL;
    PRBool ocspOn;
    char *ocspURL = NULL, *ocspSigner = NULL;

    PR_ASSERT((ctrl != NULL) && (ctrl->m_prefs != NULL));

    /* enforce the user's preferences for SSL cipher families */
    if (PREF_GetBoolPref(ctrl->m_prefs, "security.enable_ssl2", &prefval) != 
        PR_SUCCESS) {
        goto loser;
    }
    SSL_EnableDefault(SSL_ENABLE_SSL2, prefval);

    if (PREF_GetBoolPref(ctrl->m_prefs, "security.enable_ssl3", &prefval) != 
        PR_SUCCESS) {
        goto loser;
    }
    SSL_EnableDefault(SSL_ENABLE_SSL3, prefval);

    if (PREF_GetBoolPref(ctrl->m_prefs, "security.enable_tls", &prefval) != 
        PR_SUCCESS) {
        goto loser;
    }
    SSL_EnableDefault(SSL_ENABLE_TLS, prefval);


    /* set password values */
    if (PREF_GetIntPref(ctrl->m_prefs, "security.ask_for_password", &ask) !=
        PR_SUCCESS) {
        goto loser;
    }
    if (PREF_GetIntPref(ctrl->m_prefs, "security.password_lifetime", 
                        &timeout) != PR_SUCCESS) {
        goto loser;
    }

    slot = PK11_GetInternalKeySlot();
    PK11_SetSlotPWValues(slot, ssm_ask_pref_to_pk11((int)ask), 
                         (int)timeout);
    PK11_FreeSlot(slot);

    /* disable any additional ciphers that might be marked in prefs */
    ssm_enable_ssl_cipher_prefs(ctrl);
    ssm_enable_smime_cipher_prefs(ctrl);

    /*
     * Let's take care of OCSP prefs.
     */
    if (PREF_GetBoolPref(ctrl->m_prefs, "security.OCSP.enabled", 
                         &ocspOn) != SSM_SUCCESS   ||
        !ocspOn) {
        CERT_DisableOCSPChecking(ctrl->m_certdb);
        CERT_DisableOCSPDefaultResponder(ctrl->m_certdb);
    } else {
        /* OCSP should be enabled */
        CERT_EnableOCSPChecking(ctrl->m_certdb);
        /* Do we have a default responder set? */
        if (PREF_GetBoolPref(ctrl->m_prefs,
               "security.OCSP.useDefaultResponder", &ocspOn) == SSM_SUCCESS &&
            ocspOn) {
            /* First let's make sure the default URL and 
             * signer have been set. 
             */
            PREF_GetStringPref(ctrl->m_prefs, "security.OCSP.URL", &ocspURL);
            PREF_GetStringPref(ctrl->m_prefs, "security.OCSP.signingCA",
                               &ocspSigner);
            if (ocspURL != NULL && ocspSigner != NULL) {
                CERT_SetOCSPDefaultResponder(ctrl->m_certdb, ocspURL, 
                                             ocspSigner);
                CERT_EnableOCSPDefaultResponder(ctrl->m_certdb);
            }
        }
    }
    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}

SSMStatus SSMControlConnection_ProcessPrefs(SSMControlConnection* ctrl,
                                            SECItem* msg)
{
    SSMStatus rv = PR_SUCCESS;
    SetPrefListMessage request;
    SingleNumMessage reply;
    int i;
    PRBool boolval;
    PRIntn intval;

    SSM_DEBUG("Preferences were passed in from the plugin.\n");

    /* decode the message */
    if (CMT_DecodeMessage(SetPrefListMessageTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    for (i = 0; i < request.length; i++) {
        if (request.list[i].key == NULL) {
            /* misconfigured pref item: look at the next */
            continue;
        }

        switch (request.list[i].type) {
        case STRING_PREF:    /* string type */
            rv = PREF_SetStringPref(ctrl->m_prefs, request.list[i].key, 
                                    request.list[i].value);
            break;
        case BOOL_PREF:    /* boolean type */
            if (PL_strcmp(request.list[i].value, "true") == 0) {
                boolval = PR_TRUE;
            }
            else if (PL_strcmp(request.list[i].value, "false") == 0) {
                boolval = PR_FALSE;
            }
            else {
                /* misconfigured */
                break;
            }
            rv = PREF_SetBoolPref(ctrl->m_prefs, request.list[i].key,
                                  boolval);
            break;
        case INT_PREF:    /* integer type */
            intval = atoi(request.list[i].value);
            rv = PREF_SetIntPref(ctrl->m_prefs, request.list[i].key,
                                 intval);
            break;
        default:
            SSM_DEBUG("We do not understand the pref type.\n");
            break;
        }
    }

    /* prefs are all stored: now take action to apply prefs */
    rv = ssm_enable_security_prefs(ctrl);
    rv = PR_SUCCESS;
loser:
    reply.value = rv;
    msg->type =(SECItemType) (SSM_REPLY_OK_MESSAGE | SSM_PREF_ACTION);
    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    return rv;
}
        
SSMStatus
SSMControlConnection_ProcessDataRequest(SSMControlConnection * ctrl, 
                                        SECItem * msg)
{
    SSMInfoSSL infoSSL;
    SSMInfoP7Encode infoP7Encode;
    SSMInfoP7Decode infoP7Decode;
    SSMHashInitializer infoHash;
    SSMDataConnection *datac;
	void *createArg;
	SSMResourceType connType = SSM_RESTYPE_NULL;
    SSMStatus rv = PR_SUCCESS;
    SSMResourceID dataRID = 0;
	PRInt32 msgtype = msg->type & SSM_SUBTYPE_MASK;
    DataConnectionReply reply;

    switch (msgtype)
    { 
    case SSM_SSL_CONNECTION:
        {
        SSLDataConnectionRequest request;

        SSM_DEBUG("... specifically, an SSL data request.\n");

        /* Decode the SSL request message */
        if (CMT_DecodeMessage(SSLDataConnectionRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
            goto loser;
        }

        (void) memset(&infoSSL, 0, sizeof(SSMInfoSSL));
        infoSSL.isTLS = PR_FALSE;
        infoSSL.flags = request.flags;
        infoSSL.port = request.port;
        infoSSL.hostIP = request.hostIP;
        infoSSL.hostName = request.hostName;
        infoSSL.forceHandshake = request.forceHandshake;
        infoSSL.clientContext = request.clientContext;

        msg->data = NULL;

        /* fill in the control connection... */
        infoSSL.parent = ctrl;

		connType = SSM_RESTYPE_SSL_DATA_CONNECTION;
		createArg = &infoSSL;
        }
        break;

    case SSM_PKCS7DECODE_STREAM:
        {
        SingleItemMessage request;

        SSM_DEBUG("PKCS7 Decode Request.\n");
        if (CMT_DecodeMessage(SingleItemMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
            goto loser;
        }

		connType = SSM_RESTYPE_PKCS7_DECODE_CONNECTION;
        infoP7Decode.ctrl = ctrl;
        infoP7Decode.clientContext = request.item;
		createArg = &infoP7Decode;
        }
        break;

    case SSM_TLS_CONNECTION:
    case SSM_PROXY_CONNECTION:
        {
        TLSDataConnectionRequest request;

        SSM_DEBUG("... specifically, a TLS connection request.\n");

        /* decode the TLS request message */
        if (CMT_DecodeMessage(TLSDataConnectionRequestTemplate, &request,
                              (CMTItem*)msg) != CMTSuccess) {
            goto loser;
        }

        (void)memset(&infoSSL, 0, sizeof(SSMInfoSSL));
        /* notify that this is either an SMTP or an SSL proxy connection, not 
         * a regular SSL connection
         */
        infoSSL.isTLS = PR_TRUE;
        infoSSL.port = request.port;
        infoSSL.hostIP = request.hostIP;
        infoSSL.hostName = request.hostName;

        msg->data = NULL;

        /* fill in the control connection... */
        infoSSL.parent = ctrl;

        connType = SSM_RESTYPE_SSL_DATA_CONNECTION;
        createArg = &infoSSL;
        }
        break;

    case SSM_PKCS7ENCODE_STREAM:
        {
        PKCS7DataConnectionRequest request;

        SSM_DEBUG("... specifically, a PKCS#7 Encode request.\n");

        if (CMT_DecodeMessage(PKCS7DataConnectionRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
            goto loser;
        }
        infoP7Encode.ciRID = request.resID;
        connType = SSM_RESTYPE_PKCS7_ENCODE_CONNECTION;
        infoP7Encode.ctrl = ctrl;
        infoP7Encode.clientContext = request.clientContext;
        createArg = &infoP7Encode;
        }
        break;

    case SSM_HASH_STREAM:
        {
        SingleNumMessage request;

		connType = SSM_RESTYPE_HASH_CONNECTION;
        infoHash.m_parent = ctrl;
        if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
            goto loser;
        }
        infoHash.m_hashtype = (HASH_HashType) request.value;
        msg->data = NULL;
		createArg = &infoHash;
        }
		break;

    default:
        SSM_DEBUG("Unknown data connection type (%lx).\n", 
                  (msg->type & SSM_SUBTYPE_MASK));
    }

	if (connType != SSM_RESTYPE_NULL)
	{
		/* ... then create the data connection */
		SSM_DEBUG("Firing up data connection.\n");
		rv = SSM_CreateResource(connType, createArg, ctrl, &dataRID,
								(SSMResource **) &datac);
		if ((rv != PR_SUCCESS) || (!datac)) {
            goto loser;
        }
	} else  {
		rv = (SSMStatus) SSM_ERR_BAD_RESOURCE_TYPE;
		goto loser;
	}
	
    /* compose reply message */
    SSM_DEBUG("Composing reply.\n");
    msg->type = (SECItemType) (SSM_DATA_CONNECTION | msgtype | SSM_REPLY_OK_MESSAGE);
    reply.result = rv;
    reply.connID = dataRID;
    reply.port = ctrl->m_dataPort;
    if (CMT_EncodeMessage(DataConnectionReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    if (rv != PR_SUCCESS) goto loser;

    goto done;
 loser:
    if (msg->data) 
    {
        PR_Free(msg->data);
        msg->data = NULL;
    }
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    reply.result = PR_GetError();
    reply.connID = 0;
    reply.port = 0;
    CMT_EncodeMessage(DataConnectionReplyTemplate, (CMTItem*)msg, &reply);
 done:
    return rv;
}

SSMStatus
SSMControlConnection_ProcessDupResourceRequest(SSMControlConnection * ctrl, 
                                               SECItem * msg)
{
    SSMResourceID objID;
    SSMResource *obj = NULL;
    SSMStatus rv = PR_SUCCESS;
    SingleNumMessage request;
    DupResourceReply reply;

    SSM_DEBUG("Got a Duplicate Resource request.\n");
    /* parse message and get resource/field ID */

    if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    objID = request.value;

    /* ### mwelch Should free this here, instead of in the actual 
                  message parsing code. (4/13/99) */
    msg->data = NULL;
    
    SSM_DEBUG("Rsrc ID %ld.\n", objID);
    
    rv = SSMControlConnection_GetResource(ctrl, objID, &obj);
    if (rv != PR_SUCCESS)
        goto loser;
    
    PR_ASSERT(obj != NULL);
    rv = SSM_ClientGetResourceReference(obj, &objID);
    if (rv != PR_SUCCESS)
        goto loser;

    SSM_DEBUG("DuplicateResource: result %ld, new rsrc ID %ld.\n",
              (long) rv, (long)obj->m_id);
    goto done;
 loser:
    /* Got an error while getting the reference. This is recoverable, 
       because we just report the error back to the client. */
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    msg->data = NULL;
    msg->len = 0;

    /* compose reply message */
    SSM_DEBUG("Composing reply.\n");
    msg->type = (SECItemType) (SSM_RESOURCE_ACTION | SSM_DUPLICATE_RESOURCE
        | ((rv == SSM_SUCCESS) ? SSM_REPLY_OK_MESSAGE : SSM_REPLY_ERR_MESSAGE));
    reply.result = rv;
    reply.resID = (obj ? obj->m_id : 0);
    if (CMT_EncodeMessage(DupResourceReplyTemplate, (CMTItem*)msg, &reply) != CMTSuccess) {
        goto loser;
    }
    rv = PR_SUCCESS;
    if ((msg->data == NULL) || (msg->len == 0)) rv = PR_FAILURE;
    if (obj != NULL)
        SSM_FreeResource(obj);

    return rv;
}

SSMStatus
SSMControlConnection_ProcessDestroyRequest(SSMControlConnection * ctrl, 
                                          SECItem * msg)
{
    SSMStatus rv = PR_SUCCESS;
    DestroyResourceRequest request;
    SingleNumMessage reply;

    SSM_DEBUG("Got a Destroy Resource request.\n");
    /* parse message and get resource/field ID */
    if (CMT_DecodeMessage(DestroyResourceRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }
    msg->data = NULL;
    
    SSM_DEBUG("RID %ld, expected type %ld.\n", request.resID, request.resType);
    
    rv = SSM_ClientDestroyResource(ctrl, request.resID, (SSMResourceType) request.resType);
    goto done;
 loser:
    /* Got an error while getting the reference. This is recoverable, 
       because we just report the error back to the client. */
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    msg->data = NULL;
    msg->len = 0;

    /* compose reply message */
    SSM_DEBUG("Composing reply.\n");
    msg->type = (SECItemType) (SSM_RESOURCE_ACTION
        | SSM_DESTROY_RESOURCE
        | ((rv == SSM_SUCCESS) ? SSM_REPLY_OK_MESSAGE : SSM_REPLY_ERR_MESSAGE));
    reply.value = rv;
    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    rv = PR_SUCCESS;
    if ((msg->data == NULL) || (msg->len == 0)) rv = PR_FAILURE;

    return rv;
}

static SSMStatus
ssmcontrolconnection_encodegetattr_reply(SECItem *msg, SSMStatus rv, 
                                         SSMAttributeValue *value, 
                                         SSMResourceAttrType attrType)
{
    GetAttribReply reply;

    msg->data = NULL;
    msg->len = 0;

    /* compose reply message */
    SSM_DEBUG("Composing reply.\n");
    msg->type = (SECItemType) (SSM_RESOURCE_ACTION
        | SSM_GET_ATTRIBUTE
        | SSM_REPLY_OK_MESSAGE 
        | attrType);

    reply.result = rv;
    reply.value = *value;
    CMT_EncodeMessage(GetAttribReplyTemplate, (CMTItem*)msg, &reply);
    SSM_DestroyAttrValue(value, PR_FALSE);
    rv = SSM_SUCCESS;
    if ((msg->data == NULL) || (msg->len == 0))
        rv = SSM_FAILURE;

    return rv;
}

SSMStatus
SSMControlConnection_ProcessGetAttrRequest(SSMControlConnection * ctrl, 
                                            SECItem * msg)
{
    SSMResource *obj = NULL;
    SSMResourceAttrType mAttrType;
    SSMStatus rv;
    SSMAttributeValue value = {SSM_NO_ATTRIBUTE};
    GetAttribRequest request;


    SSM_DEBUG("Got a Get Attribute request.\n");
    /* parse message and get resource/field ID */

    if (CMT_DecodeMessage(GetAttribRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    msg->data = NULL;
 
    SSM_DEBUG("Rsrc ID %ld, attr ID %ld.\n", request.resID, request.fieldID);
    
    if (request.resID == SSM_SESSION_RESOURCE) {
        SSM_GetResourceReference(&ctrl->super.super);
        obj = (SSMResource *)ctrl;
    } else {
        rv = SSMControlConnection_GetResource(ctrl, request.resID, &obj);
        if (rv != PR_SUCCESS)
            goto loser;
    }
    
    PR_ASSERT(obj != NULL);
    mAttrType = (SSMResourceAttrType) (msg->type & SSM_SPECIFIC_MASK);
    rv = SSM_GetResAttribute(obj, (SSMAttributeID) request.fieldID, mAttrType,
                             &value);
    SSM_DEBUG("GetResAttribute: result %ld, type %lx.\n",
                          (long) rv, (long) value.type);
    if (rv == SSM_ERR_DEFER_RESPONSE)
        goto defer;
    if (rv != PR_SUCCESS)
        goto loser;
    
    /* Make sure the type returned matches what was asked. */
    if (mAttrType != value.type) {
        rv = SSM_ERR_ATTRIBUTE_TYPE_MISMATCH;
        /* pick default values for resource result that work
           for all three resource replies*/
        SSM_DestroyAttrValue(&value, PR_FALSE);
        value.type = SSM_NUMERIC_ATTRIBUTE;
        value.u.numeric = 0;
    }

    goto done;
 loser:
    /* Got an error while getting resource and/or attributes. This
       is recoverable, because we just report the error back to the
       client. */
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
    /* Create a suitable zero value for any type requested */
    SSM_DestroyAttrValue(&value, PR_FALSE);
    value.type = SSM_NUMERIC_ATTRIBUTE;
    value.u.numeric = 0;
 done:
    rv = ssmcontrolconnection_encodegetattr_reply(msg, rv, &value, 
                                             (SSMResourceAttrType) (msg->type & SSM_SPECIFIC_MASK));
    if (obj != NULL)
        SSM_FreeResource(obj);
 defer:
    return rv;
}

SSMStatus
SSMControlConnection_ProcessSetAttrRequest(SSMControlConnection * ctrl, 
					   SECItem * msg)
{
    SSMStatus       rv;
    SSMResource       *obj;
    SetAttribRequest  request;

    SSM_DEBUG("Got a Set Attribute Request.\n");
    if (CMT_DecodeMessage(SetAttribRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    PORT_Free(msg->data);
    msg->data = NULL;
    msg->len  = 0;

    SSM_DEBUG("Rsrc ID %ld, attr ID %ld.\n", request.resID, request.fieldID);

    rv = SSMControlConnection_GetResource(ctrl, request.resID, &obj);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    PR_ASSERT(obj != NULL);
    rv = SSM_SetResAttribute(obj, (SSMAttributeID)request.fieldID, &request.value);
    SSM_FreeResource(obj);
    SSM_DestroyAttrValue(&request.value, PR_FALSE);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    msg->type = (SECItemType) (SSM_RESOURCE_ACTION  | SSM_SET_ATTRIBUTE |
                SSM_REPLY_OK_MESSAGE | (msg->type & SSM_SPECIFIC_MASK));

    return PR_SUCCESS;
 loser:
    return PR_FAILURE;
}

SSMStatus
SSMControlConnection_ProcessCreateRequest(SSMControlConnection * ctrl, 
                                          SECItem * msg)
{
    SSMStatus  rv;
    SSMResourceID rid = 0;
    SSMResource *res = NULL;
    unsigned char *params = NULL;
    CreateResourceRequest request;
    CreateResourceReply reply;

    SSM_DEBUG("Got a Create Resource Request.\n");
    if (CMT_DecodeMessage(CreateResourceRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    msg->data = NULL;
    msg->len  = 0;

    SSM_DEBUG("Type %ld, param len %ld.\n", request.type, request.params.len);

    /* Bleah. Switch on the type.
       ### mwelch Must replace, since many of these resources can be
       generically created using control connection + single param */
    switch(request.type)
    {
    case SSM_RESTYPE_KEYGEN_CONTEXT:
        {
            SSMKeyGenContextCreateArg arg;

            arg.parent = ctrl;
            arg.type   = SSM_CRMF_KEYGEN;
            arg.param  = &request.params;
            SSM_DEBUG("Creating key gen context.\n");
            rv = SSM_CreateResource((SSMResourceType) request.type, &arg, 
                                    ctrl, &rid, &res);
        }
        break;
    case SSM_RESTYPE_SIGNTEXT:
        {
            rv = SSMSignTextResource_Create(request.params.data, ctrl, &res);
            if (rv == PR_SUCCESS) {
                PR_ASSERT(res != NULL);
                rid = res->m_id;
            }
        }
        break;
    default:
        rv = (SSMStatus) PR_INVALID_ARGUMENT_ERROR;
        break;
    }
    if (rv != PR_SUCCESS && rv != SSM_ERR_DEFER_RESPONSE)
        goto loser;
    PR_ASSERT(res != NULL);

    if (SSM_ClientGetResourceReference(res, &res->m_id) != PR_SUCCESS)
        goto loser;

    /* if deferred response don't create reply message */
    if (rv != PR_SUCCESS)
      goto done;

    msg->type = (SECItemType) (SSM_RESOURCE_ACTION  
        | SSM_CREATE_RESOURCE 
        | SSM_REPLY_OK_MESSAGE);

    goto done;
 loser:
    if (rv == PR_SUCCESS)
        rv = PR_FAILURE;
 done:
    if (params)
        PR_Free(params);
    /* Create a reply message here. */
    reply.result = rv;
    reply.resID = rid;
    CMT_EncodeMessage(CreateResourceReplyTemplate,(CMTItem*)msg, &reply);
    return rv;
}

SSMStatus
SSMControlConnection_ProcessResourceRequest(SSMControlConnection * ctrl, 
                                            SECItem * msg)
{
    SSMStatus rv = PR_SUCCESS;

    SSM_DEBUG("Got a resource-related request.\n");
    switch (msg->type & SSM_SUBTYPE_MASK) 
    { 
    case SSM_GET_ATTRIBUTE:
        rv = SSMControlConnection_ProcessGetAttrRequest(ctrl, msg);
        break;
    case SSM_CONSERVE_RESOURCE:
        rv = SSMControlConnection_ProcessConserveRequest(ctrl, msg);
        break;
    case SSM_DESTROY_RESOURCE:
        rv = SSMControlConnection_ProcessDestroyRequest(ctrl, msg);
        break;
    case SSM_DUPLICATE_RESOURCE:
        rv = SSMControlConnection_ProcessDupResourceRequest(ctrl, msg);
        break;
    case SSM_SET_ATTRIBUTE:
        rv = SSMControlConnection_ProcessSetAttrRequest(ctrl, msg);
	break;
    case SSM_CREATE_RESOURCE:
        rv = SSMControlConnection_ProcessCreateRequest(ctrl, msg);
        break;
    case SSM_TLS_STEPUP:
        rv = SSMControlConnection_ProcessTLSRequest(ctrl, msg);
        break;
    case SSM_PROXY_STEPUP:
        rv = SSMControlConnection_ProcessProxyStepUpRequest(ctrl, msg);
        break;
    default:
        SSM_DEBUG("Unknown resource request (%lx).\n", 
                  (msg->type & SSM_SUBTYPE_MASK));
        goto loser;
    }
    goto done;
 loser:
    SSM_DEBUG("ProcessResourceRequest: loser hit, rv = %ld.\n", rv);
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

static SSMStatus ssm_verifydetachedthread(SSMControlConnection *ctrl,
                                          SECItem *msg)
{
    VerifyDetachedSigRequest request;
    SingleNumMessage reply;
	SSMP7ContentInfo *ci;
    SSMStatus rv;
    
    SSM_DEBUG("Processing Verify Detached Signature request.\n");
    if (CMT_DecodeMessage(VerifyDetachedSigRequestTemplate, &request, 
                          (CMTItem*)msg) != CMTSuccess) {
        rv = SSM_FAILURE;
    } else {
        rv = SSM_SUCCESS;
    }
    msg->data = NULL;
    
    if (rv == SSM_SUCCESS) {
        
        /* Get the content info resource, if it exists. */
        rv = SSMControlConnection_GetResource(ctrl, request.pkcs7ContentID,
                                              (SSMResource **) &ci);
    }
    
    if (rv == SSM_SUCCESS) {
        
        PR_ASSERT(SSM_IsAKindOf(&ci->super, SSM_RESTYPE_PKCS7_CONTENT_INFO));
        SSM_DEBUG("Found content info (%s at %ld).\n",
                  SSM_ResourceClassName(&ci->super),
                  ci->super.m_id);
        rv = SSMP7ContentInfo_VerifyDetachedSignature(ci,
                                             (SECCertUsage)request.certUsage,
                                             (HASH_HashType) request.hashAlgID,
                                             (PRBool) request.keepCert, 
                                             (PRIntn) request.hash.len,
											 (char *)request.hash.data);
        SSM_DEBUG("VerifyDetachedSig rv = %d.\n", rv);
    }
    msg->type = (SECItemType) (SSM_OBJECT_SIGNING | SSM_VERIFY_DETACHED_SIG |
                               SSM_REPLY_OK_MESSAGE);
    if (rv != SSM_SUCCESS) {
        reply.value = PR_GetError();
    } else {
        reply.value = 0;
    }
    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    ssmcontrolconnection_send_message_to_client(ctrl, msg);
    return SSM_SUCCESS;
}

SSMStatus
SSMControlConnection_ProcessSigningRequest(SSMControlConnection *ctrl, 
										   SECItem *msg)
{
    SSMStatus rv = PR_FAILURE;
	SSMP7ContentInfo *ci;
	SSMResourceID ciRID;
    SEC_PKCS7ContentInfo *cinfo;
    SSMResourceCert *scert, *ecert, *rcert;
    CERTCertificate **rcerts;
    PRInt32 i;
	
	/* Handle a Verify Detached Signature message */
	switch(msg->type & SSM_SUBTYPE_MASK)
	{
	case SSM_VERIFY_DETACHED_SIG:
        {
            if (PK11_IsFIPS()) {
                /*
                 * When FIPS is enabled, we want to do the verification on a
                 * separate thread so that we don't block the front end 
                 * thread when the password response comes back
                 */
                rv = SSM_ProcessMsgOnThread(ssm_verifydetachedthread,
                                            ctrl, msg);
            } else {
                rv = ssm_verifydetachedthread(ctrl, msg);
            }
            if (rv == SSM_SUCCESS) {
                return SSM_ERR_DEFER_RESPONSE;
            } else {
                return SSM_FAILURE;
            }
        }
		break;
    case SSM_CREATE_SIGNED:
        {
        CreateSignedRequest request;
        CreateContentInfoReply reply;

        SSM_DEBUG("Processing Create Signed request.\n");
        if (CMT_DecodeMessage(CreateSignedRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
            goto create_signed_loser;
        }
        msg->data = NULL;
        rv = SSMControlConnection_GetResource(ctrl, request.scertRID,
                                              (SSMResource **)&scert);
        if (rv != PR_SUCCESS)
            goto create_signed_loser;
        if (!SSM_IsAKindOf(&scert->super, SSM_RESTYPE_CERTIFICATE))
            goto create_signed_loser;
        rv = SSMControlConnection_GetResource(ctrl, request.ecertRID,
                                              (SSMResource **)&ecert);
        if (rv == PR_SUCCESS && 
	    !SSM_IsAKindOf(&ecert->super, SSM_RESTYPE_CERTIFICATE))
            goto create_signed_loser;

        cinfo = SECMIME_CreateSigned(scert->cert,
				     (ecert) ? ecert->cert : NULL, 
				     ctrl->m_certdb,
                                     (SECOidTag) request.dig_alg, 
                                     (SECItem*)&request.digest, 
                                     (SECKEYGetPasswordKey) NULL, NULL);
        if (cinfo == NULL)
            goto create_signed_loser;
        rv = SSM_CreateResource(SSM_RESTYPE_PKCS7_CONTENT_INFO, cinfo,
                                ctrl, &ciRID, (SSMResource **)&ci);
        if (rv != PR_SUCCESS)
            goto create_signed_loser;

		/* Get a client reference */
		rv = SSM_ClientGetResourceReference(&ci->super, &ciRID);
		if (rv != PR_SUCCESS) {
            goto create_signed_loser;
		}

        msg->type = (SECItemType) (SSM_OBJECT_SIGNING | SSM_CREATE_SIGNED |
            SSM_REPLY_OK_MESSAGE);
        reply.ciRID = ciRID;
        reply.result = PR_SUCCESS;
        reply.errorCode = 0;
        CMT_EncodeMessage(CreateContentInfoReplyTemplate, (CMTItem*)msg, &reply);
        break;
        
      create_signed_loser:
        msg->type = (SECItemType) (SSM_OBJECT_SIGNING | SSM_CREATE_SIGNED |
            SSM_REPLY_ERR_MESSAGE);
        reply.ciRID = 0;
        reply.result = PR_FAILURE;
        reply.errorCode = PORT_GetError();
        CMT_EncodeMessage(CreateContentInfoReplyTemplate, (CMTItem*)msg, &reply);
        /* Return success here so that ProcessMessage doesn't just send
         * back a message with only the rv.  That's not interesting return
         * value in this case.
         */
        return PR_SUCCESS;
        }
    case SSM_CREATE_ENCRYPTED:
        {
        CreateEncryptedRequest request;
        CreateContentInfoReply reply;

        SSM_DEBUG("Processing Create Encrypted request.\n");
        if (CMT_DecodeMessage(CreateEncryptedRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
            goto create_encrypted_loser;
        }

        msg->data = NULL;
        rv = SSMControlConnection_GetResource(ctrl, request.scertRID,
                                              (SSMResource **)&scert);
        if (rv != PR_SUCCESS)
            goto create_encrypted_loser;
        if (!SSM_IsAKindOf(&scert->super, SSM_RESTYPE_CERTIFICATE))
            goto create_encrypted_loser;

        rcerts = (CERTCertificate **) PR_Calloc(request.nrcerts+1, sizeof(CERTCertificate *));
        if (rcerts == NULL)
            goto create_encrypted_loser;

        for(i = 0; i < request.nrcerts; i++) {
            rv = SSMControlConnection_GetResource(ctrl, request.rcertRIDs[i],
                                                  (SSMResource **)&rcert);
            if (rv != PR_SUCCESS)
                goto create_encrypted_loser;
            if (!SSM_IsAKindOf(&rcert->super, SSM_RESTYPE_CERTIFICATE))
                goto create_encrypted_loser;
            rcerts[i] = CERT_DupCertificate(rcert->cert);
            SSM_FreeResource(&rcert->super);
        }

        cinfo = SECMIME_CreateEncrypted(scert->cert, rcerts, ctrl->m_certdb,
                                        NULL, NULL);
        if (cinfo == NULL)
            goto create_encrypted_loser;
        rv = SSM_CreateResource(SSM_RESTYPE_PKCS7_CONTENT_INFO, cinfo,
                                ctrl, &ciRID, (SSMResource **)&ci);
        if (rv != PR_SUCCESS)
            goto create_encrypted_loser;

		/* Get a client reference */
		rv = SSM_ClientGetResourceReference(&ci->super, &ciRID);
		if (rv != PR_SUCCESS) {
            goto create_encrypted_loser;
		}

        msg->type = (SECItemType) (SSM_OBJECT_SIGNING | SSM_CREATE_ENCRYPTED |
            SSM_REPLY_OK_MESSAGE);
        reply.ciRID = ciRID;
        reply.result = PR_SUCCESS;
        CMT_EncodeMessage(CreateContentInfoReplyTemplate, (CMTItem*)msg, &reply);
        break;

        /*
        goto create_encrypted_done;
        */

      create_encrypted_loser:
        msg->type = (SECItemType) (SSM_OBJECT_SIGNING | SSM_CREATE_ENCRYPTED |
            SSM_REPLY_ERR_MESSAGE);
        reply.ciRID = 0;
        reply.result = PR_FAILURE;
        CMT_EncodeMessage(CreateContentInfoReplyTemplate, (CMTItem*)msg, &reply);

        /*
    create_encrypted_done:
        */
        if (rcerts != NULL) {
            for(i = 0; i < request.nrcerts; i++) {
                if (rcerts[i] != NULL)
                    CERT_DestroyCertificate(rcerts[i]);
            }
            PR_Free(rcerts);
        }
        }
        return PR_FAILURE;

	default: 
        {
        SingleNumMessage reply;

		SSM_DEBUG("Unknown signing-related request (%lx).\n", msg->type & SSM_SUBTYPE_MASK);
        reply.value = SSM_ERR_BAD_REQUEST;
        CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
        }
		break;
	}
	return PR_SUCCESS;
}

/*
 * Called by the ReadCtrlThread, processes messages for control connection
 *       and places response messages in the queue
 */
SSMStatus SSMControlConnection_ProcessMessage(SSMControlConnection * ctrl, SECItem * msg)
{ 
    SSMStatus rv                    = PR_FAILURE;
    SECStatus secstatus = SECFailure;

    SSM_DEBUG("SSMControlConnection_ProcessMessage called.\n");
    if (!ctrl || !msg)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto loser;
    }

    if ((msg->type & SSM_CATEGORY_MASK)!= SSM_REQUEST_MESSAGE) 
    {
        /* post CARTMAN_ERROR here */
        goto loser;
    }

    /* action depending on the message type and data */
    switch (msg->type & SSM_TYPE_MASK) 
    {
    case SSM_HELLO_MESSAGE:
        SSM_DEBUG("we have a Hello request.\n");
        rv = SSMControlConnection_ProcessHello(ctrl, msg);
        if (rv != PR_SUCCESS) goto hello_loser;
        break;

    case SSM_DATA_CONNECTION: 
        /* differentiate between different data connection types */
        SSM_DEBUG("we have a data connection request.\n");
        rv = SSMControlConnection_ProcessDataRequest(ctrl,msg);
        break;

    case SSM_RESOURCE_ACTION:
        rv = SSMControlConnection_ProcessResourceRequest(ctrl,msg);
        break;
    case SSM_CERT_ACTION:
        rv = SSMControlConnection_ProcessCertRequest(ctrl, msg);
        break;
    case SSM_KEYGEN_TAG:
        rv = SSMControlConnection_ProcessKeygenTag(ctrl, msg);
        break;
    case SSM_OBJECT_SIGNING:
		/* Handle an object signing related message */
		rv = SSMControlConnection_ProcessSigningRequest(ctrl,msg);
		break;
    case SSM_PKCS11_ACTION:
        rv = SSMControlConnection_ProcessPKCS11Request(ctrl, msg);
	break;
    case SSM_LOCALIZED_TEXT:
        rv = SSMControlConnection_ProcessLocalizedTextRequest(ctrl, msg);
        break;
    case SSM_CRMF_ACTION:
        rv = SSMControlConnection_ProcessCRMFRequest(ctrl, msg);
    	break;
    case SSM_FORMSIGN_ACTION:
        rv = SSMControlConnection_ProcessFormSigningRequest(ctrl, msg);
        break;
    case SSM_SECURITY_ADVISOR:
        rv = SSMControlConnection_ProcessSecurityAdvsiorRequest(ctrl, msg);
        break;
    case SSM_SEC_CFG_ACTION:
        rv = SSMControlConnection_ProcessSecCfgRequest(ctrl, msg);
        break;
    case SSM_PREF_ACTION:
        rv = SSMControlConnection_ProcessPrefs(ctrl, msg);
        break;
    case SSM_MISC_ACTION:
        rv = SSMControlConnection_ProcessMiscRequest(ctrl, msg);
        break;
    default:
        /* just send it back to the client or post an error 
         */
        SSM_DEBUG("Unknown request type (%lx).\n", msg->type & SSM_TYPE_MASK);
        break;
    }
    if (rv == SSM_ERR_DEFER_RESPONSE) {
        /* If asked to defer response, don't send anything back. 
           Just indicate that we did what we needed to do. */
        rv = PR_SUCCESS;
        goto done;
    }
    if (rv != PR_SUCCESS) {
        rv = ssmcontrolconnection_encode_err_reply(msg,rv);
    }

hello_loser:
    ssmcontrolconnection_send_message_to_client(ctrl, msg);
    SSMControlConnection_RecycleItem(msg);
    msg = NULL; /* so that exception handling doesn't try to free it */

    SSM_DEBUG("SSMControlConnection_ProcessMessage returning rv == %ld.\n", rv);
    goto done;

 loser: 
    if (rv == PR_SUCCESS) rv = (SSMStatus) secstatus;
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
    SSM_DEBUG("FAILURE in SSMControlConnection_ProcessMessage, rv = %ld.\n", rv);

	/* Attempt to pack a generic error message. ### mwelch todo */
	
 done:
    if (msg) SSMControlConnection_RecycleItem(msg);
    return rv;
}

SSMStatus
ssmcontrolconnection_send_message_to_client(SSMControlConnection *ctrl,
                                            SECItem *msg)
{
    SSM_DEBUG("queueing reply: type %lx, len %ld.\n", msg->type, msg->len);
    SSM_SendQMessage(ctrl->m_controlOutQ, 
                     SSM_PRIORITY_NORMAL,
                     msg->type, msg->len, 
                     (char *)msg->data, PR_TRUE);
    
    return SSM_SUCCESS;
}

SSMStatus 
ssmcontrolconnection_encode_err_reply(SECItem *msg, SSMStatus rv)
{
    SingleNumMessage reply;
    
    msg->type = (SECItemType) SSM_REPLY_ERR_MESSAGE;
    reply.value = rv;
    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
    if (!msg->data || msg->len == 0){
        rv = SSM_FAILURE;
    } else {
        rv = SSM_SUCCESS;
    }
    return rv;
}

SSMStatus
SSMControlConnection_Authenticate(SSMConnection *arg, char *nonce)
{
	SSMStatus rv = SSM_FAILURE;
	SSMControlConnection *ctrl = (SSMControlConnection *) arg;

	if (!SSM_IsAKindOf(SSMRESOURCE(ctrl), SSM_RESTYPE_CONTROL_CONNECTION))
		goto loser;

	/* Compare the nonce against what we have. */
	if (!strncmp(ctrl->m_nonce, nonce, strlen(ctrl->m_nonce)))
		rv = SSM_SUCCESS;
	
 loser:
	return rv;
}

SSMStatus 
SSMControlConnection_SendUIEvent(SSMControlConnection *conn,
                                 char *command,
                                 char *baseRef,
                                 SSMResource *target, /* can pass NULL */
                                 char *otherParams /* can pass NULL */,
                                 CMTItem * clientContext /* can pass NULL */,
                                 PRBool isModal)
{
    char *url;
    SECItem event;
    SSMStatus rv = PR_SUCCESS;
    PRUint32 width, height;
    SSMResourceID rid = 0;
    UIEvent reply;

    if (!conn->m_doesUI) {
        return SSM_FAILURE;
    }

    if ((!target) && (conn))
        target = &conn->super.super;
    PR_ASSERT(target);

    if (target)
        rid = ((SSMResource *) target)->m_id;

    /* Construct a URL out of the parameters we've been given. */
    rv = SSM_GenerateURL(conn, command, baseRef, target, otherParams,
                         &width, &height, &url);
    if (rv != PR_SUCCESS) {
      goto loser;
    }
    /* Generate the actual message to send to the client. */
    reply.resourceID = rid;
    reply.width = width;
    reply.height = height;
	reply.isModal = isModal;
    reply.url = url;
    if (clientContext) {
        reply.clientContext = *clientContext;
    } else {
        reply.clientContext.len = 0;
        reply.clientContext.data = NULL;
    }
    if (CMT_EncodeMessage(UIEventTemplate, (CMTItem*)&event, &reply) != CMTSuccess) {
        goto loser;
    }

    /* Post the message on the outgoing control channel. */
    rv = SSM_SendQMessage(conn->m_controlOutQ, SSM_PRIORITY_NORMAL, 
                          SSM_EVENT_MESSAGE | SSM_UI_EVENT,
                          (int) event.len, (char *) event.data, PR_FALSE);
    if (rv != PR_SUCCESS) goto loser;

    goto done;
 loser:
    if (rv == PR_SUCCESS) rv = PR_FAILURE;
 done:
    PR_FREEIF(event.data);
    return rv;
}

static SSMStatus SSMControlConnection_SavePref(SSMControlConnection* ctrl,
                                               char* key, char* value, 
                                               PRIntn type)
{
    SSMStatus rv = PR_FAILURE;
    SetPrefElement item = {0};
    SetPrefListMessage request = {0};
    CMTItem message = {0};

    /* pack the request */
    item.key = PL_strdup(key);
    item.type = type;
    if (value != NULL) {
        item.value = PL_strdup(value);
    }
    else if (type == STRING_PREF) {
        item.value = NULL; /* this is legal */
    }
    else {
        goto loser;
    }
     
    request.length = 1;
    request.list = &item;

    message.type = SSM_EVENT_MESSAGE | SSM_SAVE_PREF_EVENT;
    if (CMT_EncodeMessage(SetPrefListMessageTemplate, &message, 
                          &request) != CMTSuccess) {
        goto loser;
    }

    /* send the message through the control out queue */
    SSM_SendQMessage(ctrl->m_controlOutQ, SSM_PRIORITY_NORMAL,
                     message.type, message.len, (char*)message.data,
                     PR_TRUE);

    rv = PR_SUCCESS;
loser:
    PR_FREEIF(item.key);
    PR_FREEIF(item.value);
    return rv;
}

SSMStatus SSMControlConnection_SaveStringPref(SSMControlConnection* ctrl,
                                              char* key, char* value)
{
    SSMStatus rv = PR_SUCCESS;

    if ((ctrl == NULL) || (ctrl->m_prefs == NULL) || (key == NULL)) {
        return PR_FAILURE;
    }

    if (PREF_StringPrefChanged(ctrl->m_prefs, key, value)) {
        rv = PREF_SetStringPref(ctrl->m_prefs, key, value);
        if (rv != PR_SUCCESS) {
            return rv;
        }
        rv = SSMControlConnection_SavePref(ctrl, key, value, STRING_PREF);
    }
    return rv;
}

SSMStatus SSMControlConnection_SaveBoolPref(SSMControlConnection* ctrl,
                                            char* key, PRBool value)
{
    SSMStatus rv = PR_SUCCESS;

    if ((ctrl == NULL) || (ctrl->m_prefs == NULL) || (key == NULL)) {
        return PR_FAILURE;
    }

    if (PREF_BoolPrefChanged(ctrl->m_prefs, key, value)) {
        rv = PREF_SetBoolPref(ctrl->m_prefs, key, value);
        if (rv != PR_SUCCESS) {
            return rv;
        }
        if (value) {
            rv = SSMControlConnection_SavePref(ctrl, key, "true", BOOL_PREF);
        }
        else {
            rv = SSMControlConnection_SavePref(ctrl, key, "false", BOOL_PREF);
        }
    }
    return rv;
}

SSMStatus SSMControlConnection_SaveIntPref(SSMControlConnection* ctrl,
                                           char* key, PRIntn value)
{
    SSMStatus rv = PR_SUCCESS;

    if ((ctrl == NULL) || (ctrl->m_prefs == NULL) || (key == NULL)) {
        return PR_FAILURE;
    }

    if (PREF_IntPrefChanged(ctrl->m_prefs, key, value)) {
        char* tmp = NULL;

        rv = PREF_SetIntPref(ctrl->m_prefs, key, value);
        if (rv != PR_SUCCESS) {
            return rv;
        }
        tmp = PR_smprintf("%d", value);
        rv = SSMControlConnection_SavePref(ctrl, key, tmp, INT_PREF);
        PR_FREEIF(tmp);
    }
    return rv;
}

extern SSMControlConnection * SSM_PARENT_CONN (SSMConnection * x);

void SSM_LockPasswdTable(SSMConnection * conn)
{
    SSMControlConnection * control = SSM_PARENT_CONN(conn);
    PR_EnterMonitor(control->m_passwdLock);
}

SSMStatus SSM_UnlockPasswdTable(SSMConnection * conn)
{ 
    SSMControlConnection * control = SSM_PARENT_CONN(conn);
    return PR_ExitMonitor(control->m_passwdLock);
}

SSMStatus SSM_WaitPasswdTable(SSMConnection * conn)
{
    SSMControlConnection * control = SSM_PARENT_CONN(conn);

    return  PR_Wait(control->m_passwdLock, SSM_PASSWORD_WAIT_TIME);
}

SSMStatus SSM_NotifyAllPasswdTable(SSMConnection * conn)
{
    SSMControlConnection * control = SSM_PARENT_CONN(conn);
    return PR_NotifyAll(control->m_passwdLock);
}

/* Find the next unallocated resource ID */
SSMResourceID
SSMControlConnection_GenerateResourceID(SSMControlConnection *conn)
{
    void *junk;
    SSMResourceID rid;

    rid = conn->m_lastRID;
    do {
        rid++;
        if(rid > SSM_MAX_RID)
            rid = SSM_BASE_RID;
        SSM_HashFind(conn->m_resourceDB, rid, &junk);
    } while(junk != NULL);
    conn->m_lastRID = rid;
    return rid;
}

SSMStatus 
SSM_GetControlConnection(SSMResourceID rid, SSMControlConnection **res)
{
    return SSM_HashFind(ctrlConnections, rid, (void **)res);
}

SSMStatus 
SSMControlConnection_AddResource(SSMResource *res, SSMResourceID rid)
{
    SSMStatus rv;

    rv = SSM_HashInsert(res->m_connection->m_resourceDB, rid, res);
    if (rv != PR_SUCCESS) 
        return rv;
    rv = SSM_HashInsert(res->m_connection->m_resourceIdDB, (SSMHashKey)res, 
                        (void *)rid);
    return rv;
}

SSMStatus 
SSMControlConnection_GetResource(SSMControlConnection * connection,
                      SSMResourceID rid, SSMResource **res)
{
    SSMStatus rv = PR_FAILURE;
    if (connection->m_resourceDB) {
        rv = SSM_HashFind(connection->m_resourceDB, rid, (void **) res);
        if (*res)
            SSM_GetResourceReference(*res);
    }
    return rv;
}

SSMStatus 
SSMControlConnection_GetGlobalResourceID(SSMControlConnection * connection,
                      SSMResource * res, SSMResourceID *rid)
{
    return SSM_HashFind(connection->m_resourceIdDB, (SSMHashKey)res, 
                        (void **) rid);
}

SSMStatus 
SSMControlConnection_FormSubmitHandler(SSMResource * res, HTTPRequest * req)
{
    SSMStatus rv;
    char* tmpStr = NULL;
    SSMSSLDataConnection* conn;
        
    conn = (SSMSSLDataConnection*)res;

    /* make sure you got the right baseRef */
    rv = SSM_HTTPParamValue(req, "baseRef", &tmpStr);
    if (rv != SSM_SUCCESS ||
        PL_strcmp(tmpStr, "windowclose_doclose_js") != 0) {
        goto loser;
    }
    rv = SSM_HTTPCloseAndSleep(req);
    if (rv != SSM_SUCCESS)
        SSM_DEBUG("Errors closing window in FormSubmitHandler: %d\n", rv);
 
    
    if (!res->m_formName)
        goto loser;
    if (PL_strcmp(res->m_formName, "choose_cert_by_usage") == 0)
        rv = SSM_ChooseCertUsageHandler(req);
    else if (PL_strcmp(res->m_formName, "set_db_password") == 0)
        rv = SSM_SetDBPasswordHandler(req);
    else /* other cases where this could be used will go here */
        goto loser;
    return rv;
 loser:
    SSM_DEBUG("FormSubmit handler is called with no valid formName\n");
    SSM_NotifyUIEvent(res);
    return SSM_FAILURE;
}

typedef struct DefaultCertLookupArgStr {
    char *defaultEmailCert;
    SSMControlConnection *conn;
    PRBool isOwnThread;
    CERTCertificate *cert;
    SSMStatus rv;
} DefaultCertLookupArg;

static void
ssm_lookup_default_cert(void *arg) 
{
    DefaultCertLookupArg *lookupArg = (DefaultCertLookupArg*)arg;
    char *defaultEmailCert = lookupArg->defaultEmailCert;
    SSMControlConnection *conn = lookupArg->conn;
    CERTCertificate *tmp;
    CERTCertificate *cert = lookupArg->cert;
    

#ifdef DEBUG
    if (lookupArg->isOwnThread) {
        SSM_RegisterThread("email cert lookup", NULL);
    }
#endif
    SSM_DEBUG("Looking up the cert %s", defaultEmailCert);
    tmp = CERT_FindUserCertByUsage(conn->m_certdb, 
                                   defaultEmailCert,
                                   certUsageEmailSigner,
                                   PR_FALSE,
                                   conn);
    if (tmp == NULL) {
        lookupArg->rv =
            SSMControlConnection_SaveStringPref(conn, 
                                                "security.default_mail_cert",
                                                cert->nickname);

    } else {
        CERT_DestroyCertificate(tmp);
        lookupArg->rv = SSM_FAILURE;
    }
    SSM_FreeResource(&conn->super.super);
    if (lookupArg->isOwnThread) {
        PR_Free(arg);
    }
    CERT_DestroyCertificate(cert);
}

SSMStatus
SSM_UseAsDefaultEmailIfNoneSet(SSMControlConnection *conn, 
                               CERTCertificate *cert, 
                               PRBool onFrontEndThread)
{
    SSMStatus rv = SSM_FAILURE;
    char *defaultEmailCert;

    if (cert->emailAddr) {
        rv = PREF_GetStringPref(conn->m_prefs, "security.default_mail_cert", 
                                &defaultEmailCert);
        if (rv != SSM_SUCCESS || defaultEmailCert == NULL) {
            rv = SSMControlConnection_SaveStringPref(conn, 
                                                  "security.default_mail_cert",
                                                  cert->nickname);
        } else {
            DefaultCertLookupArg *arg = SSM_ZNEW(DefaultCertLookupArg);

            SSM_GetResourceReference(&conn->super.super);
            PR_ASSERT(arg);
            if (arg == NULL) {
                goto loser;
            }
            arg->defaultEmailCert = defaultEmailCert;
            arg->conn = conn;
            arg->cert = CERT_DupCertificate(cert);
            if (strchr(defaultEmailCert, ':') != NULL && onFrontEndThread) {
                /* ARGH!! The default email cert is on an external token
                 * this may require an authentication event, so in order
                 * to prevent dead-locking the front end thread, we'll
                 * do the look up on a separate thread.
                 */
                arg->isOwnThread = PR_TRUE;
                SSM_CreateAndRegisterThread(PR_USER_THREAD, ssm_lookup_default_cert, 
                                (void *)arg,
                                PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                                PR_UNJOINABLE_THREAD, 0);
                /*
                 * We don't know if the cert was made default, so we'll return
                 * failure in this case.
                 */
                rv = SSM_FAILURE;
            } else {
                /*
                 * It's in the internal database, so just lookup
                 * on this same thread.
                 */
                arg->isOwnThread = PR_FALSE;
                ssm_lookup_default_cert((void*)arg);
                rv = arg->rv;
                PR_Free(arg);
            }

        }
    }
    return rv;
 loser:
    return SSM_FAILURE;
}
