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
#include "sslconn.h"
#include "ctrlconn.h"
#include "sslerror.h"
#include "serv.h"
#include "servimpl.h"
#include "sslskst.h"
#include "ssmerrs.h"
#include "ssldlgs.h"
#include "collectn.h"
#include "prefs.h"

/* NSS headers */
#include "seccomon.h"
#include "keyt.h"
#include "cert.h"
#include "secasn1.h"
#include "genname.h"

/* Shorthand macros for inherited classes */
#define SSMRESOURCE(sslconn) (&(sslconn)->super.super.super)
#define SSMCONNECTION(sslconn) (&(sslconn)->super.super)
#define SSMDATACONNECTION(sslconn) (&(sslconn)->super)
#define SSMCONTROLCONNECTION(sslconn) ((SSMControlConnection*)(sslconn->super.super.m_parent))

/* SSM_UserCertChoice: enum for cert choice info */
typedef enum {ASK, AUTO} SSM_UserCertChoice;

/* persistent cert choice object we will use here */
static SSM_UserCertChoice certChoice;

/* strings for marking invalid user cert nicknames */
#define NICKNAME_EXPIRED_STRING " (expired)"
#define NICKNAME_NOT_YET_VALID_STRING " (not yet valid)"

/* private function prototypes */
void SSMSSLDataConnection_InitializeUIInfo(SSMSSLUIInfo* info);
void SSMSSLDataConnection_DestroyUIInfo(SSMSSLUIInfo* info);
SSMStatus SSMSSLDataConnection_UpdateSecurityStatus(SSMSSLDataConnection* conn);
SECStatus SSM_SetupSSLSocket(PRFileDesc* socket, PRFileDesc** sslsocket,
                             CERTCertDBHandle* handle, char* hostname,
                             void* wincx);
SSMStatus SSM_GetSSLSocket(SSMSSLDataConnection* conn);
void SSM_SSLDataServiceThread(void* arg);
SSMStatus SSM_SetUserCertChoice(SSMSSLDataConnection* conn);
SECStatus SSM_ConvertCANamesToStrings(PRArenaPool* arena, char** caNameStrings,
                                      CERTDistNames* caNames);
SSMStatus ssm_client_auth_prepare_nicknames(SSMSSLDataConnection* conn,
                                            CERTCertNicknames* nicknames);
PRBool SSM_SSLErrorNeedsDialog(int error);
SECStatus SSM_SSLMakeBadServerCertDialog(int error, 
                                         CERTCertificate* cert,
                                         SSMSSLDataConnection* conn);
SECStatus SSM_SSLVerifyServerCert(CERTCertDBHandle* handle,
                                  CERTCertificate* cert, PRBool checkSig, 
                                  SSMSSLDataConnection* conn);
static PRStatus SSMSSLDataConnection_TLSStepUp(SSMSSLDataConnection* conn);
/* callback functions */
SECStatus SSM_SSLAuthCertificate(void* arg, PRFileDesc* socket,
                                 PRBool checkSig, PRBool isServer);
SECStatus SSM_SSLBadCertHandler(void* arg, PRFileDesc* socket);
SECStatus SSM_SSLGetClientAuthData(void* arg,
                                   PRFileDesc* socket,
								   struct CERTDistNamesStr* caNames,
								   struct CERTCertificateStr** pRetCert,
								   struct SECKEYPrivateKeyStr** pRetKey);
void SSM_SSLHandshakeCallback(PRFileDesc* socket, void* clientData);



/* implementations */
/* class functions */
SSMStatus SSMSSLDataConnection_Create(void* arg, 
                                     SSMControlConnection* ctrlconn,
                                     SSMResource** res)
{
    PRStatus rv = PR_SUCCESS;
    SSMSSLDataConnection* conn=NULL;

    /* check arguments */
    if (arg == NULL || res == NULL) {
        goto loser;
    }
	
    *res = NULL; /* in case we fail */
    
    conn = (SSMSSLDataConnection*)PR_CALLOC(sizeof(SSMSSLDataConnection));
    if (!conn) {
        goto loser;
    }

    rv = SSMSSLDataConnection_Init(conn, (SSMInfoSSL*)arg, 
                                   SSM_RESTYPE_SSL_DATA_CONNECTION);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    SSMSSLDataConnection_Invariant(conn);
    
    *res = &conn->super.super.super;
    return SSM_SUCCESS;

loser:
    if (rv == PR_SUCCESS) {
        rv = PR_FAILURE;
    }

    if (conn) {
        SSM_ShutdownResource(SSMRESOURCE(conn), rv);
        SSM_FreeResource(SSMRESOURCE(conn));
    }
        
    return (SSMStatus)rv;
}

SSMStatus SSMSSLDataConnection_Shutdown(SSMResource* arg, SSMStatus status)
{
    SSMStatus rv;
    SSMSSLDataConnection* conn = (SSMSSLDataConnection*)arg;
	PRBool firstTime = PR_TRUE;
    /* ### sjlee: firstTime doesn't seem to be set anywhere */
    
	/* check argument */
	if (arg == NULL) {
		return PR_FAILURE;
	}
	
    /*SSMSSLDataConnection_Invariant(conn); -- this could have been called from loser */

    SSM_LockResource(arg);

    /* shut down base class */
    rv = SSMDataConnection_Shutdown(arg, status);

	if (firstTime) {
		SSM_DEBUG("First time shutting down SSL connection.\n");
	}

    /* If service threads are done, close the SSL socket */
    if (SSMRESOURCE(conn)->m_threadCount == 0) {
        if (conn->socketSSL) {
			SSM_DEBUG("Closing SSL socket with linger.\n");
            rv = PR_Close(conn->socketSSL);
            conn->socketSSL = NULL;
            SSM_DEBUG("Closed SSL socket (rv == %d).\n", rv);
        }
    }
    SSM_UnlockResource(arg);

	if (!firstTime) {
		rv = (SSMStatus)SSM_ERR_ALREADY_SHUT_DOWN;
	}
    return rv;
}

SSMStatus SSMSSLDataConnection_Init(SSMSSLDataConnection* conn, 
								   SSMInfoSSL* info, SSMResourceType type)
{
    SSMStatus rv = PR_SUCCESS;
    SSMControlConnection* parent;
 
 	/* check arguments */
	if (conn == NULL || info == NULL) {
		goto loser;
	}

    /* fill in the information we got from the request packet */
	parent = info->parent;
    rv = SSMDataConnection_Init(SSMDATACONNECTION(conn), parent, type);
    if (rv != PR_SUCCESS) {
		goto loser;
	}
    conn->isTLS = info->isTLS;
    conn->port = info->port;
    conn->hostIP = info->hostIP;
    conn->hostName = info->hostName;
    /* Initialize UI info */
    SSMSSLDataConnection_InitializeUIInfo(&conn->m_UIInfo);

    if (conn->isTLS) {
        conn->isSecure = PR_FALSE;

        /* Create the step-up FD so that the control connection
           can wake us up */
        conn->stepUpFD = PR_NewPollableEvent();
        if (conn->stepUpFD == NULL)
            goto loser;
    }
    else {
        conn->isSecure = PR_TRUE;
        conn->forceHandshake = info->forceHandshake;

        /* Save the client context */
        SSMRESOURCE(conn)->m_clientContext = info->clientContext;
    }

    /* Spawn the data service thread */
    SSM_DEBUG("Creating SSL data service thread.  Getting ref on SSL "
              "connection.\n");

    /*
     * This reference is for the socket status.  The client needs to tell
     * us when to get rid of it.
     */
    SSM_GetResourceReference(SSMRESOURCE(conn));

    SSMDATACONNECTION(conn)->m_dataServiceThread =
        SSM_CreateThread(SSMRESOURCE(conn), SSM_SSLDataServiceThread);
    if (SSMDATACONNECTION(conn)->m_dataServiceThread == NULL) {
        goto loser;
    }
    conn->m_statusFetched = PR_FALSE;
    return PR_SUCCESS;

loser:
    if (rv == PR_SUCCESS) {
		rv = PR_FAILURE;
	}
    return rv;
}

SSMStatus SSMSSLDataConnection_Destroy(SSMResource* res, PRBool doFree)
{
    SSMSSLDataConnection* conn = (SSMSSLDataConnection*)res;

	if (res == NULL) {
		return PR_FAILURE;
	}
    /* We should be shut down. */
    PR_ASSERT(res->m_threadCount == 0);

    /* Destroy our fields. */
    PR_FREEIF(conn->hostIP);
    PR_FREEIF(conn->hostName);

    /* Destroy UI info. */
    SSMSSLDataConnection_DestroyUIInfo(&conn->m_UIInfo);

    /* Destroy step-up FD if we still have one. */
    if (conn->stepUpFD)
        PR_DestroyPollableEvent(conn->stepUpFD);

    /* Destroy superclass fields. */
    SSMDataConnection_Destroy(SSMRESOURCE(conn), PR_FALSE);

    /* Destroy the socket status if it's there. */
    if (conn->m_sockStat) {
        SSM_FreeResource(&conn->m_sockStat->super);
    }

    /* Free the connection object if asked. */
    if (doFree) {
        PR_DELETE(conn);
	}

    return PR_SUCCESS;
}

void SSMSSLDataConnection_Invariant(SSMSSLDataConnection* conn)
{
    SSMDataConnection_Invariant(SSMDATACONNECTION(conn));

    SSM_LockResource(SSMRESOURCE(conn));
    PR_ASSERT(SSM_IsAKindOf(SSMRESOURCE(conn), 
                            SSM_RESTYPE_SSL_DATA_CONNECTION));
    SSM_UnlockResource(SSMRESOURCE(conn));
}

SSMStatus SSMSSLDataConnection_GetAttrIDs(SSMResource* res,
										 SSMAttributeID** ids, PRIntn* count)
{
    SSMStatus rv=SSM_SUCCESS;

	if (res == NULL || ids == NULL || count == NULL) {
		goto loser;
	}
    rv = SSMDataConnection_GetAttrIDs(res, ids, count);
    if (rv != PR_SUCCESS) {
        goto loser;
	}

    *ids = (SSMAttributeID *) PR_REALLOC(*ids, (*count + 2)*sizeof(SSMAttributeID));
    if (!*ids) {
		goto loser;
	}

    (*ids)[*count++] = SSM_FID_SSLDATA_SOCKET_STATUS;
    (*ids)[*count++] = SSM_FID_SSLDATA_DISCARD_SOCKET_STATUS;
    goto done;
loser:
    if (rv == SSM_SUCCESS) {
		rv = SSM_FAILURE;
	}
done:
    return rv;
}

SSMStatus SSMSSLDataConnection_SetAttr(SSMResource * res,
                                       SSMResourceAttrType attrType,
                                       SSMAttributeValue *value)
{
    SSMStatus rv = SSM_FAILURE;
    SSMSSLDataConnection *conn = (SSMSSLDataConnection*)res;

    if (!SSM_IsAKindOf(res, SSM_RESTYPE_SSL_DATA_CONNECTION)) {
        return SSM_FAILURE;
    }
    switch (attrType) {
    case SSM_FID_SSLDATA_DISCARD_SOCKET_STATUS:
        /*
         * Just release the reference we were holding onto.
         */
        if (!conn->m_statusFetched) {
            SSM_FreeResource(res);
            conn->m_statusFetched = PR_TRUE;
        }
        rv = SSM_SUCCESS;
        break;
    default:
        break;
    }
    return rv;
}

SSMStatus SSMSSLDataConnection_GetAttr(SSMResource *res, SSMAttributeID attrID,
                                       SSMResourceAttrType attrType,
                                       SSMAttributeValue *value)
{
    SSMSSLDataConnection* conn = (SSMSSLDataConnection*)res;
    SSMStatus rv = PR_SUCCESS;

	if (res == NULL || value == NULL) {
		goto loser;
	}

    /* see what it is */
    switch(attrID) {
    case SSM_FID_SSLDATA_SOCKET_STATUS:
        /* if the socket status does not exist at this time (which
         * is very unlikely usually), we will wait until the handshake
         * callback creates it
         */
        SSM_LockResource(SSMRESOURCE(conn));
        while (conn->m_sockStat == NULL || 
               conn->m_sockStat->m_cipherName == NULL) {
            SSM_DEBUG("Oops, the security status has not been updated.  Waiting...\n");
            SSM_WaitResource(SSMRESOURCE(conn), PR_INTERVAL_NO_TIMEOUT);
        }
        if (conn->m_sockStat == NULL) {
            SSM_DEBUG("No socket status on dead socket.\n");
            SSM_UnlockResource(SSMRESOURCE(conn));
            goto loser;
        }
        /* We have a socket status object, return its resource ID. */
        value->u.rid = conn->m_sockStat->super.m_id;
        value->type = SSM_RID_ATTRIBUTE;
        SSM_UnlockResource(SSMRESOURCE(conn));

        /*
        if (conn->m_sockStat) {
        }
        else {
            rv = SSM_ERR_ATTRIBUTE_MISSING;
            goto loser;
        }
        */
		break;
    case SSM_FID_SSLDATA_ERROR_VALUE:
        value->type = SSM_NUMERIC_ATTRIBUTE;
        SSM_LockResource(SSMRESOURCE(conn));
        value->u.numeric = conn->m_error;
        SSM_DEBUG("Reported error: %ld\n", conn->m_error);
        SSM_UnlockResource(SSMRESOURCE(conn));
        break;
    default:
        rv = SSMConnection_GetAttr(res, attrID, attrType, value);
        if (rv != PR_SUCCESS) {
			goto loser;
		}
    }

    goto done;
loser:
    value->type = SSM_NO_ATTRIBUTE;
    if (rv == PR_SUCCESS) {
		rv = PR_FAILURE;
	}
done:
    return rv;
}


/*
 * Function: SSMStatus SSMSSLDataConnection_PickleSecurityStatus()
 * Purpose: fills in information on security status (pickled socket status
 *          and the security level) on PickleSecurityStatus request
 *
 * Arguments and return values:
 * - conn: SSL connection object
 * - len: length of the pickled data
 * - blob: pickled data
 * - securityLevel: security level
 * - returns: PR_SUCCESS if successful; error code otherwise
 *
 * Note: Note that this is not really a pickle class function.  This is
 *       specially designed to handle security status requests efficiently.
 */
SSMStatus SSMSSLDataConnection_PickleSecurityStatus(SSMSSLDataConnection* conn,
                                                   PRIntn* len, void** blob,
                                                   PRIntn* securityLevel)
{
    SSMStatus rv = PR_SUCCESS;

    PR_ASSERT(conn != NULL);

    /* in case of failure */
    *len = 0;
    *blob = NULL;
    *securityLevel = 0;

    /* the connection may not be secure yet in case of proxy connections:
     * we should just return and indicate no security if that's the case
     */
    if (!conn->isSecure) {
        return rv;
    }

    /* if the socket status does not exist at this time (which is usually
     * very unlikely), we will wait until the handshake callback creates it
     */
    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_sockStat == NULL ||
        conn->m_sockStat->m_cipherName == NULL) {
        SSM_DEBUG("Oops, the security status has not been updated.  "
                  "Waiting...\n");
        SSM_WaitResource(SSMRESOURCE(conn), PR_TicksPerSecond());
    }
    if (conn->m_sockStat == NULL) {
        SSM_DEBUG("No socket status on dead socket.\n");
        SSM_UnlockResource(SSMRESOURCE(conn));
        rv = PR_FAILURE;
        goto loser;
    }
    /* it's probably OK to unlock it here */
    SSM_UnlockResource(SSMRESOURCE(conn));

    /* we have a socket status object. */
    rv = SSMSSLSocketStatus_Pickle(&conn->m_sockStat->super, len, blob);
    if (rv != PR_SUCCESS) {
        SSM_DEBUG("Failed to pickle the socket status.\n");
        goto loser;
    }

    /* get the security level */
    *securityLevel = conn->m_sockStat->m_level;

    /*
     * The client retrieved the socket status, so we can release our
     * refernce now.
     */
    if (!conn->m_statusFetched) {
        SSM_FreeResource(SSMRESOURCE(conn));
        conn->m_statusFetched = PR_TRUE;
    }
 loser:
    return rv;
}


/*
 * Function: SSMStatus SSMSSLDataConnection_FormSubmitHandler()
 * Purpose: handles any UI form submission that has an SSMSSLDataConnection
 *          object as target
 * Expected request:
 * - command: "formsubmit_handler"
 * - baseRef: "formsubmit_dosubmit_js"
 * - target: an SSMSSLDataConnection object
 * - formName: "cert_selection" | "bad_server_cert"
 * Arguments and return values
 * - res: SSMSSLDataConnection object
 * - req: the HTTPRequest object to be processed
 * - returns: SSM_SUCCESS if successful; error otherwise
 *
 * Note: add more handling helper functions if there are other form
 *       submission scenarios
 */
SSMStatus SSMSSLDataConnection_FormSubmitHandler(SSMResource* res,
                                                 HTTPRequest* req)
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

    /* differentiate among different form submission events by looking up
     * formName, and dispatch the request to the appropriate handler 
     */
    /* cleaning up (unlocking monitors, etc.) is handled by individual
     * handlers, so just go to done
     */
    if (res->m_formName == NULL) {
        goto loser;
    }
    if (PL_strcmp(res->m_formName, "cert_selection") == 0) {
        rv = SSM_ClientAuthCertSelectionButtonHandler(req);
        goto done;
    }
    else if (PL_strcmp(res->m_formName, "bad_server_cert") == 0) {
        /* server auth failure other than unknown issuer case */
        rv = SSM_ServerAuthFailureButtonHandler(req);
        goto done;
    }
    else {
        /* unknown form name; abort */
        SSM_DEBUG("don't know how to handle this form.\n");
        goto loser;
    }
loser:
    if (rv == SSM_SUCCESS) {
        rv = SSM_FAILURE;
    }
    SSM_LockResource(req->target);
    conn->m_UIInfo.chosen = -1;
    conn->m_UIInfo.trustBadServerCert = BSCA_NO;
    conn->m_UIInfo.UICompleted = PR_TRUE;
    SSM_NotifyResource(req->target);
    SSM_UnlockResource(req->target);
done:
    return rv;
}


void SSMSSLDataConnection_InitializeUIInfo(SSMSSLUIInfo* info)
{
    info->UICompleted = PR_FALSE;
    info->numFilteredCerts = 0;
    info->certNicknames = NULL;
    info->chosen = -1;    /* negative value denotes no cert was chosen */
    info->trustBadServerCert = BSCA_NO;
}

void SSMSSLDataConnection_DestroyUIInfo(SSMSSLUIInfo* info)
{
    int i;

    if (info->numFilteredCerts != 0) {
        for (i = 0; i < info->numFilteredCerts; i++) {
            PR_FREEIF(info->certNicknames[i]);
        }
        PR_FREEIF(info->certNicknames);
    }
}

SSMStatus SSMSSLDataConnection_UpdateSecurityStatus(SSMSSLDataConnection* conn)
{
    SSMStatus rv = PR_SUCCESS;

	PR_ASSERT(conn != NULL);
	
	SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_sockStat == NULL) {
        /* socket status has not been created.  The security status will
         * be properly populated by creating the socket status.
         */
        SSMResourceID statRID;

        SSM_CreateResource(SSM_RESTYPE_SSL_SOCKET_STATUS, conn->socketSSL, 
                           SSMRESOURCE(conn)->m_connection, &statRID, 
                           (SSMResource**)&conn->m_sockStat);
		conn->m_sockStat->m_error = conn->m_sslServerError;
        /*
         * Just in case someone's waiting on the resource.
         */
        SSM_LockResource(SSMRESOURCE(conn));
        SSM_NotifyResource(SSMRESOURCE(conn));
        SSM_UnlockResource(SSMRESOURCE(conn));
    }
    else {
        /* just update the values */
        rv = SSMSSLSocketStatus_UpdateSecurityStatus(conn->m_sockStat, 
                                                     conn->socketSSL);
        if (rv != PR_SUCCESS) {
            goto loser;
        }
    }
loser:
    /* in case we fail to update the security status, we still notify
     * because we don't want to quit just because this failed...
     */
    SSM_NotifyResource(SSMRESOURCE(conn));
    /* notify SSMSSLDataConnection_GetAttr() for the socket RID */
	SSM_UnlockResource(SSMRESOURCE(conn));

    return rv;
}


/* updates the error code in case of an exception: returns the current
 * error code (0 if no error) 
 */
static 
SSMStatus SSMSSLDataConnection_UpdateErrorCode(SSMSSLDataConnection* conn)
{
    PR_ASSERT(conn != NULL);
    SSM_LockResource(SSMRESOURCE(conn));
    if (conn->m_error == 0) {
        /* update the error code only if it is not already set:
         * otherwise preserve the initial error code
         */
        PRInt32 rv;

        rv = PR_GetError();
        if (rv != SEC_ERROR_LIBRARY_FAILURE) {
            /* XXX sometimes NSS reports this error even when connection
             *     terminates normally: will escape this error here so that
             *     this is not reported erroneously
             */
            conn->m_error = rv;
        }
    }
    SSM_UnlockResource(SSMRESOURCE(conn));

    return conn->m_error;
}

/* functions for setting up and configuring SSL connection */
SECStatus SSM_SetupSSLSocket(PRFileDesc* socket, PRFileDesc** sslsocket,
                             CERTCertDBHandle* handle, char* hostname,
                             void* wincx)
{
    SECStatus rv = SECFailure;

    /* check arguments */
    if ((socket == NULL) || (sslsocket == NULL) || (handle == NULL) || 
        (hostname == NULL) || (wincx == NULL)) {
        goto loser;
    }
    *sslsocket = NULL;

  	/* import the socket into SSL layer */
    *sslsocket = SSL_ImportFD(NULL, socket);
    if (*sslsocket == NULL) {
        goto loser;
    }
  
  	/* set some SSL settings for the socket */
    rv = SSL_Enable(*sslsocket, SSL_SECURITY, PR_TRUE);
    if (rv != SECSuccess) {
		goto loser;
	}

  	rv = SSL_Enable(*sslsocket, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
    if (rv != SECSuccess) {
		goto loser;
	}

	rv = SSL_Enable(*sslsocket, SSL_ENABLE_FDX, PR_TRUE);
    if (rv != SECSuccess) {
		goto loser;
	}
	
  	/* set callbacks */
  	/* client authentication */
  	rv = SSL_GetClientAuthDataHook(*sslsocket, 
                                   (SSLGetClientAuthData)SSM_SSLGetClientAuthData, 
                                   NULL) == 0 ? SECSuccess : SECFailure;
    if (rv  != SECSuccess) {
		goto loser;
	}
  
  	/* server cert authentication */
	rv = SSL_AuthCertificateHook(*sslsocket,
                                 (SSLAuthCertificate)SSM_SSLAuthCertificate,
                                 (void*)handle) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
		goto loser;
	}
  
  	rv = SSL_BadCertHook(*sslsocket, (SSLBadCertHandler)SSM_SSLBadCertHandler, 
                         wincx) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
  		goto loser;
	}

    rv = SSL_HandshakeCallback(*sslsocket, 
                               (SSLHandshakeCallback)SSM_SSLHandshakeCallback,
                               wincx) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

    SSM_DEBUG("setting PKCS11 pin arg.\n");
    rv = SSL_SetPKCS11PinArg(*sslsocket, wincx) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

    /* prepare against man-in-the-middle attacks */
    rv = SSL_SetURL(*sslsocket, hostname) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

  	return rv;
loser:
	if (*sslsocket == NULL) {
		/* aborted before SSL_ImportFD() */
		if (socket != NULL) {
			PR_Close(socket);
		}
	}
	else {
		PR_Close(*sslsocket);
        *sslsocket = NULL;
	}

	return rv;
}


SSMStatus SSM_GetSSLSocket(SSMSSLDataConnection* conn)
{
    SECStatus secstatus = SECSuccess;
    PRStatus prstatus = PR_SUCCESS;
    PRFileDesc* socket = NULL;
    PRSocketOptionData sockdata;
    SSMControlConnection* ctrlconn;
    PRNetAddr addr;
    PRHostEnt hostentry;
    char buffer[256];

    SSM_DEBUG("SSM_GetSSLSocket entered.\n");

	/* check argument first */
	if (conn == NULL) {
		return PR_FAILURE;
	}
	
    /* Enter SSL lock. We will release once connection is set up. 
     * If GetSecurityStatus request comes in before we are done, it'll spin 
     * on the SSL lock. (This lock is not used anyplace else.)
     */
  
    /*PR_EnterMonitor(conn->sslLock);*/

    /* set up SSL secure socket */
    SSM_DEBUG("setting up secure socket.\n");
    socket = PR_OpenTCPSocket(PR_AF_INET6);
    if (socket == NULL) {
        goto loser;
    }

    /* make the socket blocking - default on some platforms is non-blocking */
    sockdata.option = PR_SockOpt_Nonblocking;
    sockdata.value.non_blocking = PR_FALSE;
    if (PR_SetSocketOption(socket, &sockdata) != PR_SUCCESS) {
        goto loser;
    }


    if (!conn->isTLS) {
        /* set up SSL secure socket */
        SSM_DEBUG("setting up secure socket.\n");

        ctrlconn = SSMCONTROLCONNECTION(conn);
        secstatus = SSM_SetupSSLSocket(socket, &conn->socketSSL,
                                       ctrlconn->m_certdb, conn->hostName, 
                                       (void*)conn);
        if (secstatus != SECSuccess) {
            goto loser;
        }
    }
    else {
        /* do not need to create a secure socket here */
        conn->socketSSL = socket;
    }
	
    /* prepare and setup network connection */
    SSM_DEBUG("preparing and setting up network connection.\n");
    if (PR_StringToNetAddr(conn->hostIP, &addr) != PR_SUCCESS) {
        PRIntn hostIndex;

        prstatus = PR_GetIPNodeByName(conn->hostName, PR_AF_INET6, PR_AI_DEFAULT,
			                          buffer, sizeof(buffer), &hostentry);
	if (prstatus != PR_SUCCESS) {
	    goto loser;
	}

  
	SSM_DEBUG("SSL server port: %d / host: %s \n", conn->port, 
		  conn->hostName);
	do {
	    hostIndex = PR_EnumerateHostEnt(0, &hostentry, 
					    (PRUint16) conn->port, &addr);
	    if (hostIndex < 0) {
	        goto loser;
	    }
	} while (PR_Connect(conn->socketSSL, &addr, PR_INTERVAL_NO_TIMEOUT) != PR_SUCCESS
		 && hostIndex > 0);

	SSM_DEBUG("Connected to target.\n");
    } else {
	if (PR_NetAddrFamily(&addr) == PR_AF_INET) {
	    PRIPv6Addr v6addr;
	    PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &v6addr);
	    if (PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, (PRUint16)conn->port, &addr) != PR_SUCCESS) {
		goto loser;
	    }
	    addr.ipv6.ip = v6addr;
	} else if (PR_SetNetAddr(PR_IpAddrNull, PR_AF_INET6, (PRUint16)conn->port, &addr) != PR_SUCCESS) {
	    goto loser;
	}
	if (PR_Connect(conn->socketSSL, &addr, PR_INTERVAL_NO_TIMEOUT) != PR_SUCCESS) {
	  goto loser;
	}
	SSM_DEBUG("SSL server port: %d / host: %s\n", conn->port, 
		  conn->hostName);
	SSM_DEBUG("connected using hostIP: %s\n", conn->hostIP);
    }

#ifdef _HACK_
    /* ### mwelch Fix for NSPR20 connect problem under 95/NT. */
    PR_Sleep(PR_INTERVAL_NO_WAIT);
#endif

    /* established SSL connection, ready to send data */
    if (!conn->isTLS && conn->forceHandshake) {
        /* if the client wants a forced handshake (e.g. SSL/IMAP), 
         * do it here 
         */
        SSM_DEBUG("forcing handshake.\n");
        secstatus = (SSL_ForceHandshake(conn->socketSSL) == 0)? SECSuccess:SECFailure;
        if (secstatus != SECSuccess) {
            goto loser;
        }
    }

    /*
	if (conn->m_sockStat) {
        SSM_FreeResource(&conn->m_sockStat->super);
	}
    SSM_CreateResource(SSM_RESTYPE_SSL_SOCKET_STATUS, conn->socketSSL, 
                       SSMRESOURCE(conn)->m_connection, &statRID, 
                       (SSMResource**)&conn->m_sockStat);
    */	
	/* check everything is hunky-dorey */
    /*	SSMSSLSocketStatus_Invariant(conn->m_sockStat); */
    /* end of the block */

    /* release SSL lock. SSL connection is established. */
    /*PR_Notify(conn->sslLock);
      PR_ExitMonitor(conn->sslLock);*/

    SSM_DEBUG("returning OK.\n");

    return prstatus;

loser:
    (void)SSMSSLDataConnection_UpdateErrorCode(conn);
    SSM_LockResource(SSMRESOURCE(conn));
    SSM_DEBUG("error %d (%s), exiting abnormally.\n", conn->m_error, 
              SSL_Strerror(conn->m_error));
    SSM_UnlockResource(SSMRESOURCE(conn));
    return PR_FAILURE;
}


/* 
 * Function: PRStatus SSMSSLDataConnection_TLSStepUp()
 * Purpose: "step up" the connection and set up an SSL socket for the 
 *          underlying TCP socket
 * Arguments and return values:
 * - conn: TLS connection object to be manipulated
 * - returns: PR_SUCCESS if successful; PR_FAILURE otherwise
 *
 * Notes: SSL handshakes are not done here; it is performed when the first
 *        read/write operation is done on the secure socket
 *        This function must be called while the underlying resource is
 *        locked
 */
static PRStatus SSMSSLDataConnection_TLSStepUp(SSMSSLDataConnection* conn)
{
    PRFileDesc* socket = conn->socketSSL;
    PRFileDesc* sslsocket = NULL;

    PR_ASSERT(conn->isTLS == PR_TRUE);

    if (conn->isSecure == PR_TRUE) {
        /* socket is already secure */
        return PR_SUCCESS;
    }

    SSM_DEBUG("Setting up secure socket for this TLS connection.\n");
    if (SSM_SetupSSLSocket(socket, &sslsocket,
                           SSMCONTROLCONNECTION(conn)->m_certdb, 
                           conn->hostName, (void*)conn) != SECSuccess) {
        goto loser;
    }

    SSM_DEBUG("Resetting handshake.\n");
    if (SSL_ResetHandshake(sslsocket, PR_FALSE) != SECSuccess) {
        goto loser;
    }

    /* XXX it is important that the SSL handshake does not take place here
     *     because that would create a deadlock on this thread
     */
    SSM_DEBUG("We now have a secure socket.\n");
    /* secure socket is established */
    conn->socketSSL = sslsocket;
    conn->isSecure = PR_TRUE;

    return PR_SUCCESS;
loser:
    (void)SSMSSLDataConnection_UpdateErrorCode(conn);
    if (sslsocket == NULL) {
        if (socket != NULL) {
            PR_Close(socket);
        }
    }
    else {
        PR_Close(sslsocket);
    }

    return PR_FAILURE;
}


/* thread function */
/* SSL connection is serviced by the data service thread that works on
 * the client data socket and the SSL socket.
 * 
 * Our poll descriptor array contains the client data socket and the SSL 
 * target socket in this order.
 */
#define SSL_PDS 3
#define READSSL_CHUNKSIZE 16384

/* Helper functions for thread */
SSMStatus
SSM_SSLThreadReadFromClient(SSMSSLDataConnection* conn, 
			    PRIntn oBufSize, char *outbound, PRPollDesc *pds)
{
  PRIntn read;
  PRIntn sent;
  SSMStatus rv = SSM_SUCCESS;

  SSM_DEBUG("Attempting to read %ld bytes from client "
	    "socket.\n", oBufSize);
  read = PR_Recv(SSMDATACONNECTION(conn)->m_clientSocket,
		 outbound, oBufSize, 0,
		 PR_INTERVAL_NO_TIMEOUT);
  
  if (read <= 0) {
    if (read < 0) {
      /* Got an error */
      rv = SSMSSLDataConnection_UpdateErrorCode(conn);
      SSM_DEBUG("Error receiving data over client "
		"socket, status == %ld\n", (long)rv);
    }
    else {
      /* We got an EOF */
      SSM_DEBUG("Got EOF at client socket.\n");
    }
    
    if (conn->socketSSL != NULL) {
      SSM_DEBUG("Shutting down the target socket.\n");
      PR_Shutdown(conn->socketSSL, PR_SHUTDOWN_SEND);
    }
    
    /* remove the socket from the array */
    pds->in_flags = 0;
    /* Stop processing data at this point.*/
    rv = SSM_FAILURE;
  }
  else {
    SSM_DEBUG("data: <%s>\n" "Send the data to the "
	      "target.\n", outbound);
    sent = PR_Send(conn->socketSSL, outbound, read, 0,
		   PR_INTERVAL_NO_TIMEOUT);
    if (sent != read) {
      SSM_DEBUG("Couldn't send all data to the target socket (sent: %d read: %d)\n",
		sent, read);
      rv = SSMSSLDataConnection_UpdateErrorCode(conn);
      
      if (SSMDATACONNECTION(conn)->m_clientSocket != 
	  NULL) {
	SSM_DEBUG("Shutting down the client socket.\n");
	PR_Shutdown(SSMDATACONNECTION(conn)->m_clientSocket, 
		    PR_SHUTDOWN_SEND);
      }

      PR_Shutdown(conn->socketSSL, PR_SHUTDOWN_RCV);
      /* remove the socket from the array */
      pds->in_flags = 0;
      rv = SSM_FAILURE;
    }
  }
  return rv;
}

SSMStatus
SSM_SSLThreadReadFromServer(SSMSSLDataConnection* conn, 
			    char *inbound, PRIntn bufsize,
			    PRPollDesc* pds)
{
  PRIntn read;
  PRIntn sent;
  SSMStatus rv=SSM_SUCCESS;

  SSM_DEBUG("Reading data from target socket.\n");
  read = PR_Recv(conn->socketSSL, inbound, bufsize,
		 0, PR_INTERVAL_NO_TIMEOUT);
  
  if (read <= 0) {
    if (read < 0) {
      /* Got error */
      rv = SSMSSLDataConnection_UpdateErrorCode(conn);
      SSM_DEBUG("Error receiving data from target "
		"socket, status = %ld\n", rv);
    }
    else {
      /* We got an EOF */
      SSM_DEBUG("Got EOF at target socket.\n");
    }
    
    if (SSMDATACONNECTION(conn)->m_clientSocket != NULL) {
      SSM_DEBUG("Shutting down the client socket.\n");
      PR_Shutdown(SSMDATACONNECTION(conn)->m_clientSocket, 
		  PR_SHUTDOWN_SEND);
    }
    
    /* remove the socket from the array */
    pds->in_flags = 0;
    PR_Shutdown(conn->socketSSL, PR_SHUTDOWN_RCV);
    rv = SSM_FAILURE;
  }
  else {
    /* Got data, write it to the client socket */
    SSM_DEBUG("Writing to client socket.\n");
#if 0
    SSM_DumpBuffer(inbound, read);
#endif
    sent = PR_Send(SSMDATACONNECTION(conn)->m_clientSocket,
		   inbound, read, 0, 
		   PR_INTERVAL_NO_TIMEOUT);
    if (sent != read) {
      rv = SSMSSLDataConnection_UpdateErrorCode(conn);
      
      if (conn->socketSSL != NULL) {
	SSM_DEBUG("Shutting down the target socket.\n");
	PR_Shutdown(conn->socketSSL, PR_SHUTDOWN_SEND);
      }
      
      /* remove the socket from the array */
      pds->in_flags = 0;
      PR_Shutdown(conn->socketSSL, PR_SHUTDOWN_RCV);
      rv = SSM_FAILURE;
    }
    SSM_DEBUG("Wrote %ld bytes.\n", sent);
  }
  return rv;
}

SSMStatus
SSM_SSLThreadMakeConnectionSSL(SSMSSLDataConnection* conn,
			       PRPollDesc* pds)			       
{
  /* We've been told to enable TLS on this connection.
     Clear the event, step up, and reconfigure sockets. */
  
  PRFileDesc *stepUpFD = conn->stepUpFD;
  SSMStatus rv = SSM_SUCCESS;
  
  /* Make sure the control connection has settled */
  SSM_LockResource(SSMRESOURCE(conn));
  if ((pds->out_flags & PR_POLL_READ) != 0)
    /* it's readable, so it should return immediately */
    PR_WaitForPollableEvent(stepUpFD);
  
  /* Step up to encryption. */
  rv = SSMSSLDataConnection_TLSStepUp(conn);
  conn->m_error = rv; /* tell the control connection */
  
  /* Remove the step-up fd */
  conn->stepUpFD = NULL;
  pds->in_flags = 0;
  PR_DestroyPollableEvent(stepUpFD);
  
  /* Notify ourselves to wake up the control connection */
  SSM_NotifyResource(SSMRESOURCE(conn));
  SSM_UnlockResource(SSMRESOURCE(conn));
  return rv;
}

PRBool
SSM_WaitingOnData(PRPollDesc *pds, PRIntn numPDs)
{
    PRBool retVal = PR_FALSE;
    int i;

    for (i=0; i<numPDs;i++) {
        if (pds[i].in_flags & PR_POLL_READ) {
	    retVal = PR_TRUE;
	    break;
        }
    }

    return retVal;
}

void SSM_SSLDataServiceThread(void* arg)
{
    SSMStatus rv = PR_SUCCESS;
    SSMSSLDataConnection* conn = NULL;
    PRPollDesc pds[SSL_PDS];
    PRIntn nSockets = 0;
    PRInt32 nReady;
    int i;
    char *outbound = NULL;
    char *inbound = NULL;
    PRIntn oBufSize;
    
    SSM_RegisterNewThread("ssl data", (SSMResource *) arg);
    conn = (SSMSSLDataConnection*)arg;
    SSM_DEBUG("initializing.\n");
    if (arg == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        goto loser;
    }

    SSMDATACONNECTION(conn)->m_dataServiceThread = PR_GetCurrentThread();

	/* initialize xfer buffers */
	outbound = (char *) PR_CALLOC(READSSL_CHUNKSIZE+1);
	if (outbound == NULL)
		goto loser;

	inbound = (char *) PR_CALLOC(READSSL_CHUNKSIZE+1);
	if (inbound == NULL)
		goto loser;

    /* initialize the poll descriptors */
    for (i = 0; i < SSL_PDS; i++) {
        pds[i].fd = NULL;
	pds[i].in_flags = 0;
	pds[i].out_flags = 0;
    }

    /* set up sockets */
    /* set up the client data socket */
    rv = SSMDataConnection_SetupClientSocket(SSMDATACONNECTION(conn));
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* save the client data socket in the array */
    pds[0].fd = SSMDATACONNECTION(conn)->m_clientSocket;
    pds[0].in_flags = PR_POLL_READ;
    nSockets++;

    /* set up the SSL socket */
    rv = SSM_GetSSLSocket(conn);
    if (rv != PR_SUCCESS) {
        goto loser;
    }

    /* save the SSL socket in the array */
    pds[1].fd = conn->socketSSL;
    pds[1].in_flags = PR_POLL_READ;
    nSockets++;

    if (conn->isTLS) {
        /* SSL/SMTP connections usually require a large outbound buffer */
        oBufSize = READSSL_CHUNKSIZE;

        /* Add step-up FD to the list of FDs until we've been asked to
           encrypt */
        pds[2].fd = conn->stepUpFD;
        pds[2].in_flags = PR_POLL_READ;
        nSockets++;
    }
    else {
        oBufSize = LINESIZE;
    }

    /* spinning mode.  Exchange data between the client socket and the SSL
     * socket.
     */
    while (SSM_WaitingOnData(pds, nSockets) && 
           (SSM_Count(SSMDATACONNECTION(conn)->m_shutdownQ) == 0)) {
        /* XXX Nova-specific: Nova does not handle active close from the
         *     server properly in case of keep-alive connections: what
         *     happens is that although we perform a shutdown on the client
         *     socket, Nova never sends us a close.  If the SSL socket is 
         *     removed, we might as well close the client socket by hand.
         */
        /* if the outbound connection is shut down and there is no data
         * pending in the client socket, perhaps the client is acting up
         */
        if (pds[1].in_flags == 0) {
            /* don't block in polling */
	    SSM_DEBUG("Polling for keep-aliveness\n");
            nReady = PR_Poll(pds, SSL_PDS, PR_INTERVAL_NO_TIMEOUT);
            if (nReady <= 0) {
                /* either error or the client socket is blocked.  Bail. */
                rv = SSMSSLDataConnection_UpdateErrorCode(conn);
                if (SSMDATACONNECTION(conn)->m_clientSocket != NULL) {
                    PR_Shutdown(SSMDATACONNECTION(conn)->m_clientSocket,
                                PR_SHUTDOWN_SEND);
                }
                nSockets--;
                if (nSockets > 1)
                    {
                        /* 
                           We were waiting for notification to step up,
                           but that notification won't happen. So,
                           remove stepUpFD from the list as well.
                        */
                        pds[2].fd = NULL;
                        nSockets = 1;
                    }
                break;    /* go to loser; */
            }
        }

        SSM_DEBUG("Polling sockets for pending data.\n");
        /* poll sockets for pending data */
        /* Poll for however many sockets we're interested in. */
        SSM_DEBUG("About to poll sockets for data\n");
        nReady = PR_Poll(pds, nSockets, PR_INTERVAL_NO_TIMEOUT);
        SSM_DEBUG("Retruned from poll with %d sockets ready for reading.\n",
		  nReady);
        if (nReady <= 0) {
            rv = SSMSSLDataConnection_UpdateErrorCode(conn);
            goto loser;
        }

        /* Wind down from SSL_PDS because the pollable event
           takes priority over the I/O sockets. */
        for (i = (SSL_PDS-1); i >= 0; i--) {
            if (pds[i].fd == NULL || pds[i].in_flags == 0) {
                pds[i].out_flags = 0;
                continue;
            }

            /* check sockets for data */
            if (pds[i].out_flags & (PR_POLL_READ | PR_POLL_EXCEPT)) {
                if (pds[i].fd == SSMDATACONNECTION(conn)->m_clientSocket) {
                    /* got data at the client socket */
		    rv = SSM_SSLThreadReadFromClient(conn, 
						     oBufSize, outbound,
						     &pds[i]);
		    SSM_DEBUG("Just finished reading from client with rv=%d\n",
			      rv);
                }
                else if (pds[i].fd == conn->socketSSL) {
                    /* got data at the SSL socket */
		    rv = SSM_SSLThreadReadFromServer(conn, 
						     inbound, 
						     READSSL_CHUNKSIZE,
						     &pds[i]);
		    SSM_DEBUG("Just finished reading from server with rv=%d\n",
			      rv);
                }
                else if (pds[i].fd == conn->stepUpFD)
                {
		    rv = SSM_SSLThreadMakeConnectionSSL(conn, 
							&pds[i]);
		    SSM_DEBUG("Just finished steeping up connection with rv=%d\n",
			      rv);
                }
		if (rv != SSM_SUCCESS) {
		  goto loser;
		}
            }
            pds[i].out_flags = 0;
        }    /* end of for loop */
    }    /* end of while loop */
loser:
    SSM_DEBUG("** Closing, return value %ld. **\n", rv);
    if (conn != NULL) {
      	SSM_ShutdownResource(SSMRESOURCE(conn), rv);
        SSM_FreeResource(SSMRESOURCE(conn));
    }
    if (outbound != NULL) {
        PR_Free(outbound);
    }
    if (inbound != NULL) {
        PR_Free(inbound);
    }
    SSM_DEBUG("SSL data service thread terminated.\n");
}


/* callback functions and auxilliary functions */
/*
 * Function: SECStatus SSM_SSLAuthCertificate()
 * Purpose: this callback function is used to authenticate certificates
 *			received from the remote end of an SSL connection.
 *
 * Arguments and return values
 * - arg: certDB handle (cannot be NULL)
 * - socket: SSL socket
 * - checkSig: whether to check the signature (usually should be PR_TRUE)
 * - isServer: whether we are an SSL server (expect PR_FALSE since Cartman is
 *			   an SSL client)
 * - returns: SECSuccess if successful; error code otherwise
 *
 * ### sjlee: this is essentially a wrapper over the default SSL callback
 *			  function.  It provides minimal argument checking to the default
 *			  callback.  When we're confident that the default will do the
 *			  job, we might want to drop this...
 */
SECStatus SSM_SSLAuthCertificate(void* arg, PRFileDesc* socket, 
								 PRBool checkSig, PRBool isServer)
{
	SSM_DEBUG("checking server cert.\n");

	/* check arguments */
	if (arg == NULL || socket == NULL || isServer) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return SECFailure;
	}

	/* call the default callback */
	return (SECStatus) SSL_AuthCertificate(arg, socket, checkSig, isServer);
}


/*
 * Function: SECStatus SSM_SSLBadCertHandler()
 * Purpose: this callback function is used to handle a situation in which 
 *			server certificate verification has failed.
 *
 * Arguments and return values
 * - arg: the SSL data connection
 * - socket: SSL socket
 * - returns: SECSuccess if the case is handled and it is authorized to
 *			  continue connection; otherwise SECFailure
 */
SECStatus SSM_SSLBadCertHandler(void* arg, PRFileDesc* socket)
{
	int error;
    SSMSSLDataConnection* conn;
    CERTCertificate* cert = NULL;
    SECStatus rv = SECFailure;
	
    if (socket == NULL || arg == NULL) {
        return SECFailure;
    }
    conn = (SSMSSLDataConnection*)arg;

    if (!(SSMCONTROLCONNECTION(conn)->m_doesUI)) {
        /* UI-less application.  We choose to reject the server cert. */
        goto loser;
    }

    cert = SSL_PeerCertificate(conn->socketSSL);
    if (cert == NULL) {
        goto loser;
    }

    while (rv != SECSuccess) {
        error = PR_GetError();	/* first of all, get error code */
	
        SSM_DEBUG("Got a bad server cert: error %d (%s).\n", error, 
                  SSL_Strerror(error));
	
		/* Save error for socket status */
		conn->m_sslServerError = error;

        if (!SSM_SSLErrorNeedsDialog(error)) {
            SSM_DEBUG("Exiting abnormally...\n");
            break;
        }

        if (SSM_SSLMakeBadServerCertDialog(error, cert, conn) != SECSuccess) {
            break;
        }

        /* check the server cert again for more errors */
        rv = SSM_DoubleCheckServerCert(cert, conn);
    }

loser:
    if (rv != SECSuccess) {
        (void)SSMSSLDataConnection_UpdateErrorCode(conn);
    }

    if (cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    return rv;
}

/*
 * structs and ASN1 templates for the limited scope-of-use extension
 *
 * CertificateScopeEntry ::= SEQUENCE {
 *     name GeneralName, -- pattern, as for NameConstraints
 *     portNumber INTEGER OPTIONAL }
 *
 * CertificateScopeOfUse ::= SEQUENCE OF CertificateScopeEntry
 */
/*
 * CERTCertificateScopeEntry: struct for scope entry that can be consumed by
 *                            the code
 * certCertificateScopeOfUse: struct that represents the decoded extension data
 */
typedef struct {
    SECItem derConstraint;
    SECItem derPort;
    CERTGeneralName* constraint; /* decoded constraint */
    PRIntn port; /* decoded port number */
} CERTCertificateScopeEntry;

typedef struct {
    CERTCertificateScopeEntry** entries;
} certCertificateScopeOfUse;

/* corresponding ASN1 templates */
static const SEC_ASN1Template cert_CertificateScopeEntryTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
      0, NULL, sizeof(CERTCertificateScopeEntry) },
    { SEC_ASN1_ANY,
      offsetof(CERTCertificateScopeEntry, derConstraint) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(CERTCertificateScopeEntry, derPort) },
    { 0 }
};

static const SEC_ASN1Template cert_CertificateScopeOfUseTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, cert_CertificateScopeEntryTemplate }
};

/* 
 * decodes the extension data and create CERTCertificateScopeEntry that can
 * be consumed by the code
 */
static
SECStatus cert_DecodeScopeOfUseEntries(PRArenaPool* arena, SECItem* extData,
                                       CERTCertificateScopeEntry*** entries,
                                       int* numEntries)
{
    certCertificateScopeOfUse* scope = NULL;
    SECStatus rv = SECSuccess;
    int i;

    *entries = NULL; /* in case of failure */
    *numEntries = 0; /* ditto */

    scope = (certCertificateScopeOfUse*)
        PORT_ArenaZAlloc(arena, sizeof(certCertificateScopeOfUse));
    if (scope == NULL) {
        goto loser;
    }

    rv = SEC_ASN1DecodeItem(arena, (void*)scope, 
                            cert_CertificateScopeOfUseTemplate, extData);
    if (rv != SECSuccess) {
        goto loser;
    }

    *entries = scope->entries;
    PR_ASSERT(*entries != NULL);

    /* first, let's count 'em. */
    for (i = 0; (*entries)[i] != NULL; i++) ;
    *numEntries = i;

    /* convert certCertificateScopeEntry sequence into what we can readily
     * use
     */
    for (i = 0; i < *numEntries; i++) {
        (*entries)[i]->constraint = 
            cert_DecodeGeneralName(arena, &((*entries)[i]->derConstraint), 
                                   NULL);
        if ((*entries)[i]->derPort.data != NULL) {
            (*entries)[i]->port = 
                (int)DER_GetInteger(&((*entries)[i]->derPort));
        }
        else {
            (*entries)[i]->port = 0;
        }
    }

    goto done;
loser:
    if (rv == SECSuccess) {
        rv = SECFailure;
    }
done:
    return rv;
}

static SECStatus cert_DecodeCertIPAddress(SECItem* genname, 
                                          PRUint32* constraint, PRUint32* mask)
{
    /* in case of failure */
    *constraint = 0;
    *mask = 0;

    PR_ASSERT(genname->data != NULL);
    if (genname->data == NULL) {
        return SECFailure;
    }
    if (genname->len != 8) {
        /* the length must be 4 byte IP address with 4 byte subnet mask */
        return SECFailure;
    }

    /* get them in the right order */
    *constraint = PR_ntohl((PRUint32)(*genname->data));
    *mask = PR_ntohl((PRUint32)(*(genname->data + 4)));

    return SECSuccess;
}

static char* _str_to_lower(char* string)
{
#ifdef XP_WIN
    return _strlwr(string);
#else
    int i;
    for (i = 0; string[i] != '\0'; i++) {
        string[i] = tolower(string[i]);
    }
    return string;
#endif
}

/*
 * Sees if the client certificate has a restriction in presenting the cert
 * to the host: returns PR_TRUE if there is no restriction or if the hostname
 * (and the port) satisfies the restriction, or PR_FALSE if the hostname (and
 * the port) does not satisfy the restriction
 */
static PRBool CERT_MatchesScopeOfUse(CERTCertificate* cert, char* hostname,
                                     char* hostIP, PRIntn port)
{
    PRBool rv = PR_TRUE; /* whether the cert can be presented */
    SECStatus srv;
    SECItem extData;
    PRArenaPool* arena = NULL;
    CERTCertificateScopeEntry** entries = NULL;
    /* arrays of decoded scope entries */
    int numEntries = 0;
    int i;
    char* hostLower = NULL;
    PRUint32 hostIPAddr = 0;

    PR_ASSERT((cert != NULL) && (hostname != NULL) && (hostIP != NULL));

    /* find cert extension */
    srv = CERT_FindCertExtension(cert, SEC_OID_NS_CERT_EXT_SCOPE_OF_USE,
                                 &extData);
    if (srv != SECSuccess) {
        /* most of the time, this means the extension was not found: also,
         * since this is not a critical extension (as of now) we may simply
         * return PR_TRUE
         */
        goto done;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto done;
    }

    /* decode the scope of use entries into pairs of GeneralNames and
     * an optional port numbers
     */
    srv = cert_DecodeScopeOfUseEntries(arena, &extData, &entries, &numEntries);
    if (srv != SECSuccess) {
        /* XXX What should we do when we failed to decode the extension?  This
         *     may mean either the extension was malformed or some (unlikely)
         *     fatal error on our part: my argument is that if the extension 
         *     was malformed the extension "disqualifies" as a valid 
         *     constraint and we may present the cert
         */
        goto done;
    }

    /* loop over these structures */
    for (i = 0; i < numEntries; i++) {
        /* determine whether the GeneralName is a DNS pattern, an IP address 
         * constraint, or else
         */
        CERTGeneralName* genname = entries[i]->constraint;

        /* if constraint is NULL, don't bother looking */
        if (genname == NULL) {
            /* this is not a failure: just continue */
            continue;
        }

        switch (genname->type) {
        case certDNSName: {
            /* we have a DNS name constraint; we should use only the host name
             * information
             */
            char* pattern = NULL;
            char* substring = NULL;

            /* null-terminate the string */
            genname->name.other.data[genname->name.other.len] = '\0';
            pattern = _str_to_lower(genname->name.other.data);

            if (hostLower == NULL) {
                /* so that it's done only if necessary and only once */
                hostLower = _str_to_lower(PL_strdup(hostname));
            }

            /* the hostname satisfies the constraint */
            if (((substring = strstr(hostLower, pattern)) != NULL) &&
                /* the hostname contains the pattern */
                (strlen(substring) == strlen(pattern)) &&
                /* the hostname ends with the pattern */
                ((substring == hostLower) || (*(substring-1) == '.'))) {
                /* the hostname either is identical to the pattern or
                 * belongs to a subdomain
                 */
                rv = PR_TRUE;
            }
            else {
                rv = PR_FALSE;
            }
            /* clean up strings if necessary */
            break;
        }
        case certIPAddress: {
            PRUint32 constraint;
            PRUint32 mask;
            PRNetAddr addr;
            
            if (hostIPAddr == 0) {
                /* so that it's done only if necessary and only once */
                PR_StringToNetAddr(hostIP, &addr);
                hostIPAddr = addr.inet.ip;
            }

            if (cert_DecodeCertIPAddress(&(genname->name.other), &constraint, 
                                         &mask) != SECSuccess) {
                continue;
            }
            if ((hostIPAddr & mask) == (constraint & mask)) {
                rv = PR_TRUE;
            }
            else {
                rv = PR_FALSE;
            }
            break;
        }
        default:
            /* ill-formed entry: abort */
            continue; /* go to the next entry */
        }

        if (!rv) {
            /* we do not need to check the port: go to the next entry */
            continue;
        }

        /* finally, check the optional port number */
        if ((entries[i]->port != 0) && (port != entries[i]->port)) {
            /* port number does not match */
            rv = PR_FALSE;
            continue;
        }

        /* we have a match */
        PR_ASSERT(rv);
        break;
    }
done:
    /* clean up entries */
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }
    if (hostLower != NULL) {
        PR_Free(hostLower);
    }
    return rv;
}

/*
 * Function: SECStatus SSM_SSLGetClientAuthData()
 * Purpose: this callback function is used to pull client certificate
 *			information upon server request
 *
 * Arguments and return values
 * - arg: SSL data connection
 * - socket: SSL socket we're dealing with
 * - caNames: list of CA names
 * - pRetCert: returns a pointer to a pointer to a valid certificate if
 *			   successful; otherwise NULL
 * - pRetKey: returns a pointer to a pointer to the corresponding key if
 *			  successful; otherwise NULL
 * - returns: SECSuccess if successful; error code otherwise
 */
SECStatus SSM_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
								   CERTDistNames* caNames,
								   CERTCertificate** pRetCert,
								   SECKEYPrivateKey** pRetKey)
{
	void* wincx = NULL;
	SECStatus rv = SECFailure;
	SSMSSLDataConnection* conn;
	SSMControlConnection* ctrlconn;
    PRArenaPool* arena = NULL;
    char** caNameStrings = NULL;
    CERTCertificate* cert = NULL;
    SECKEYPrivateKey* privKey = NULL;
    CERTCertList* certList = NULL;
    CERTCertListNode* node;
    CERTCertNicknames* nicknames = NULL;
    char* extracted = NULL;
    PRIntn keyError = 0; /* used for private key retrieval error */
	
	SSM_DEBUG("Client authentication callback function called.\n");
	
	/* do some argument checking */
	if (socket == NULL || caNames == NULL || pRetCert == NULL ||
		pRetKey == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return SECFailure;
	}
	
	/* get PKCS11 pin argument */
	wincx = SSL_RevealPinArg(socket);
	if (wincx == NULL) {
		return SECFailure;
	}
	
	/* get the right connections */
	conn = (SSMSSLDataConnection*)wincx;
	ctrlconn = (SSMControlConnection*)(SSMCONNECTION(conn)->m_parent);
	PR_ASSERT(ctrlconn);

    /* create caNameStrings */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto loser;
    }

    caNameStrings = (char**)PORT_ArenaAlloc(arena, 
                                            sizeof(char*)*(caNames->nnames));
    if (caNameStrings == NULL) {
        goto loser;
    }

    rv = SSM_ConvertCANamesToStrings(arena, caNameStrings, caNames);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* get the preference */
	if (SSM_SetUserCertChoice(conn) != SSM_SUCCESS) {
		goto loser;
	}

	/* find valid user cert and key pair */	
	if (certChoice == AUTO) {
		/* automatically find the right cert */

        /* find all user certs that are valid and for SSL */
        certList = CERT_FindUserCertsByUsage(ctrlconn->m_certdb, 
                                             certUsageSSLClient, PR_TRUE,
                                             PR_TRUE, wincx);
        if (certList == NULL) {
            goto noCert;
        }

        /* filter the list to those issued by CAs supported by the server */
        rv = CERT_FilterCertListByCANames(certList, caNames->nnames,
                                          caNameStrings, certUsageSSLClient);
        if (rv != SECSuccess) {
            goto noCert;
        }

        /* make sure the list is not empty */
        node = CERT_LIST_HEAD(certList);
        if (CERT_LIST_END(node, certList)) {
            goto noCert;
        }

        /* loop through the list until we find a cert with a key */
        while (!CERT_LIST_END(node, certList)) {
            /* if the certificate has restriction and we do not satisfy it
             * we do not use it
             */
            if (!CERT_MatchesScopeOfUse(node->cert, conn->hostName,
                                        conn->hostIP, conn->port)) {
                node = CERT_LIST_NEXT(node);
                continue;
            }

            privKey = PK11_FindKeyByAnyCert(node->cert, wincx);
            if (privKey != NULL) {
                /* this is a good cert to present */
                cert = CERT_DupCertificate(node->cert);
                break;
            }
            keyError = PR_GetError();
            if (keyError == SEC_ERROR_BAD_PASSWORD) {
                /* problem with password: bail */
                goto loser;
            }

            node = CERT_LIST_NEXT(node);
        }
        if (cert == NULL) {
            goto noCert;
        }
	}
	else {
        /* user selects a cert to present */
        int i;

        /* find all user certs that are valid and for SSL */
        /* note that we are allowing expired certs in this list */
        certList = CERT_FindUserCertsByUsage(ctrlconn->m_certdb, 
                                             certUsageSSLClient, PR_TRUE, 
                                             PR_FALSE, wincx);
        if (certList == NULL) {
            goto noCert;
        }

        if (caNames->nnames != 0) {
            /* filter the list to those issued by CAs supported by the 
             * server 
             */
            rv = CERT_FilterCertListByCANames(certList, caNames->nnames, 
                                              caNameStrings, 
                                              certUsageSSLClient);
            if (rv != SECSuccess) {
                goto loser;
            }
        }

        if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
            /* list is empty - no matching certs */
            goto noCert;
        }

        /* filter it further for hostname restriction */
        node = CERT_LIST_HEAD(certList);
        while (!CERT_LIST_END(node, certList)) {
            if (!CERT_MatchesScopeOfUse(node->cert, conn->hostName,
                                        conn->hostIP, conn->port)) {
                CERTCertListNode* removed = node;
                node = CERT_LIST_NEXT(removed);
                CERT_RemoveCertListNode(removed);
            }
            else {
                node = CERT_LIST_NEXT(node);
            }
        }
        if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
            goto noCert;
        }

        nicknames = CERT_NicknameStringsFromCertList(certList,
                                                     NICKNAME_EXPIRED_STRING,
                                                     NICKNAME_NOT_YET_VALID_STRING);
        if (nicknames == NULL) {
            goto loser;
        }

        SSM_DEBUG("%d valid user certs found.\n", nicknames->numnicknames);
        conn->m_UIInfo.numFilteredCerts = nicknames->numnicknames;

        if (ssm_client_auth_prepare_nicknames(conn, nicknames) != 
            SSM_SUCCESS) {
            goto loser;
        }

        /* create a cert selection dialog and get back the chosen nickname */
        if (SSM_SSLMakeClientAuthDialog(conn) != SSM_SUCCESS) {
            goto loser;
        }

        PR_ASSERT(conn->m_UIInfo.chosen >= 0);
        i = conn->m_UIInfo.chosen;
        SSM_DEBUG("Cert %s was selected.\n", conn->m_UIInfo.certNicknames[i]);

        /* first we need to extract the real nickname in case the cert
         * is an expired or not a valid one yet
         */
        extracted = CERT_ExtractNicknameString(conn->m_UIInfo.certNicknames[i],
                                               NICKNAME_EXPIRED_STRING,
                                               NICKNAME_NOT_YET_VALID_STRING);
        if (extracted == NULL) {
            goto loser;
        }

        /* find the cert under that nickname */
        node = CERT_LIST_HEAD(certList);
        while (!CERT_LIST_END(node, certList)) {
            if (PL_strcmp(node->cert->nickname, extracted) == 0) {
                cert = CERT_DupCertificate(node->cert);
                break;
            }
            node = CERT_LIST_NEXT(node);
        }
        if (cert == NULL) {
            goto loser;
        }
	
        /* go get the private key */
        privKey = PK11_FindKeyByAnyCert(cert, wincx);
        if (privKey == NULL) {
            keyError = PR_GetError();
            if (keyError == SEC_ERROR_BAD_PASSWORD) {
                /* problem with password: bail */
                goto loser;
            }
            else {
                goto noCert;
            }
        }
	}
	/* rv == SECSuccess */
	SSM_DEBUG("Yahoo!  Client auth complete.\n");

	goto done;
noCert:
loser:
    if (rv == SECSuccess) {
        rv = SECFailure;
    }
    if (cert != NULL) {
        CERT_DestroyCertificate(cert);
        cert = NULL;
    }
done:
    if (extracted != NULL) {
        PR_Free(extracted);
    }
    if (nicknames != NULL) {
        CERT_FreeNicknames(nicknames);
    }
    if (certList != NULL) {
        CERT_DestroyCertList(certList);
    }
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    *pRetCert = cert;
    *pRetKey = privKey;

	return rv;
}

/*
 * Function: SSMStatus SSM_SetUserCertChoice()

 * Purpose: sets certChoice by reading the preference
 *
 * Arguments and return values
 * - conn: SSMSSLDataConnection
 * - returns: SSM_SUCCESS if successful; SSM_FAILURE otherwise
 *
 * Note: If done properly, this function will read the identifier strings
 *		 for ASK and AUTO modes, read the selected strings from the
 *		 preference, compare the strings, and determine in which mode it is
 *		 in.
 *       We currently use ASK mode for UI apps and AUTO mode for UI-less
 *       apps without really asking for preferences.
 */
SSMStatus SSM_SetUserCertChoice(SSMSSLDataConnection* conn)
{
    SSMStatus rv;
    SSMControlConnection* parent;

    parent = SSMCONTROLCONNECTION(conn);

    if (parent->m_doesUI) {
        char* mode = NULL;

        rv = PREF_GetStringPref(parent->m_prefs, 
                                "security.default_personal_cert", &mode);
        if (rv != PR_SUCCESS) {
            return SSM_FAILURE;
        }
        if (PL_strcmp(mode, "Select Automatically") == 0) {
            certChoice = AUTO;
        }
        else if (PL_strcmp(mode, "Ask Every Time") == 0) {
            certChoice = ASK;
        }
        else {
            return SSM_FAILURE;
        }
    }
    else {
        SSM_DEBUG("UI-less app: use auto cert selection.\n");
        certChoice = AUTO;
    }

	return SSM_SUCCESS;
}


/*
 * Function: SECStatus SSM_ConvertCANamesToStrings()
 * Purpose: creates CA names strings from (CERTDistNames* caNames)
 *
 * Arguments and return values
 * - arena: arena to allocate strings on
 * - caNameStrings: filled with CA names strings on return
 * - caNames: CERTDistNames to extract strings from
 * - return: SECSuccess if successful; error code otherwise
 *
 * Note: copied in its entirety from Nova code
 */
SECStatus SSM_ConvertCANamesToStrings(PRArenaPool* arena, char** caNameStrings,
                                      CERTDistNames* caNames)
{
    SECItem* dername;
    SECStatus rv;
    int headerlen;
    uint32 contentlen;
    SECItem newitem;
    int n;
    char* namestring;

    for (n = 0; n < caNames->nnames; n++) {
        newitem.data = NULL;
        dername = &caNames->names[n];

        rv = DER_Lengths(dername, &headerlen, &contentlen);

        if (rv != SECSuccess) {
            goto loser;
        }

        if (headerlen + contentlen != dername->len) {
            /* This must be from an enterprise 2.x server, which sent
             * incorrectly formatted der without the outer wrapper of
             * type and length.  Fix it up by adding the top level
             * header.
             */
            if (dername->len <= 127) {
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 2);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)dername->len;
                (void)memcpy(&newitem.data[2], dername->data, dername->len);
            }
            else if (dername->len <= 255) {
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 3);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)0x81;
                newitem.data[2] = (unsigned char)dername->len;
                (void)memcpy(&newitem.data[3], dername->data, dername->len);
            }
            else {
                /* greater than 256, better be less than 64k */
                newitem.data = (unsigned char *) PR_Malloc(dername->len + 4);
                if (newitem.data == NULL) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char)0x30;
                newitem.data[1] = (unsigned char)0x82;
                newitem.data[2] = (unsigned char)((dername->len >> 8) & 0xff);
                newitem.data[3] = (unsigned char)(dername->len & 0xff);
                memcpy(&newitem.data[4], dername->data, dername->len);
            }
            dername = &newitem;
        }

        namestring = CERT_DerNameToAscii(dername);
        if (namestring == NULL) {
            /* XXX - keep going until we fail to convert the name */
            caNameStrings[n] = "";
        }
        else {
            caNameStrings[n] = PORT_ArenaStrdup(arena, namestring);
            PR_Free(namestring);
            if (caNameStrings[n] == NULL) {
                goto loser;
            }
        }

        if (newitem.data != NULL) {
            PR_Free(newitem.data);
        }
    }

    return SECSuccess;
loser:
    if (newitem.data != NULL) {
        PR_Free(newitem.data);
    }
    return SECFailure;
}


/*
 * Function: SSMStatus ssm_client_auth_prepare_nicknames()
 * Purpose: (private) picks out the filtered nicknames and populates the
 *          nicknames field of the SSL connection object with them
 * Arguments and return values:
 * - conn: SSL connection object to be manipulated
 * - nicknames: nickname list
 *
 * Note: the valid nickname list is not empty if this function was called
 *       Also the memory for the nickname list is allocated here; the caller
 *       is responsible for freeing the nickname list before the client
 *       auth callback function returns
 */
SSMStatus ssm_client_auth_prepare_nicknames(SSMSSLDataConnection* conn,
                                            CERTCertNicknames* nicknames)
{
    int i;
    int number;

    PR_ASSERT(conn != NULL && conn->m_UIInfo.numFilteredCerts > 0 && 
	      nicknames != NULL);
    number = conn->m_UIInfo.numFilteredCerts;

    /* allocate memory for nickname list */
    conn->m_UIInfo.certNicknames = (char**)PR_Calloc(number, sizeof(char*));
    if (conn->m_UIInfo.certNicknames == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto loser;
    }

    /* fill in cert nicknames */
    for (i = 0; i < number; i++) {
        conn->m_UIInfo.certNicknames[i] = PL_strdup(nicknames->nicknames[i]);
        if (conn->m_UIInfo.certNicknames[i] == NULL) {
            goto loser;
        }
    }
    
    return SSM_SUCCESS;
loser:
    return SSM_FAILURE;
}


void SSM_SSLHandshakeCallback(PRFileDesc* socket, void* clientData)
{
	SSMSSLDataConnection* conn;
	
	PR_ASSERT(socket != NULL);
	PR_ASSERT(clientData != NULL);
	
	SSM_DEBUG("Handshake callback called.\n");
	
	conn = (SSMSSLDataConnection*)clientData;
	
	/* update the security status info */
    (void)SSMSSLDataConnection_UpdateSecurityStatus(conn);
    /* we want to proceed even if the update has failed... */

	/* check everything is hunky-dorey */    
	SSMSSLSocketStatus_Invariant(conn->m_sockStat);
}


/*
 * Function: PRBool SSM_SSLErrorNeedsDialog()
 * Purpose: sorts out errors that require dialogs
 *
 * Arguments and return values
 * - error: error code
 * - returns: PR_TRUE if dialog needed; PR_FALSE otherwise
 */
PRBool SSM_SSLErrorNeedsDialog(int error)
{
	return ((error == SEC_ERROR_UNKNOWN_ISSUER) ||
            (error == SEC_ERROR_UNTRUSTED_ISSUER) ||
            (error == SSL_ERROR_BAD_CERT_DOMAIN) ||
            (error == SSL_ERROR_POST_WARNING) ||
            (error == SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE) ||
            (error == SEC_ERROR_CA_CERT_INVALID) ||
            (error == SEC_ERROR_EXPIRED_CERTIFICATE));
}



SECStatus SSM_SSLMakeBadServerCertDialog(int error, 
                                         CERTCertificate* cert,
                                         SSMSSLDataConnection* conn)
{
    SECStatus rv;

    if ((error == SEC_ERROR_UNKNOWN_ISSUER) ||
        (error == SEC_ERROR_CA_CERT_INVALID) ||
        (error == SEC_ERROR_UNTRUSTED_ISSUER)) {
        rv = SSM_SSLMakeUnknownIssuerDialog(cert, conn);
    } 
    else if (error == SSL_ERROR_BAD_CERT_DOMAIN) {
        rv = SSM_SSLMakeCertBadDomainDialog(cert, conn);
    }
    else if (error == SSL_ERROR_POST_WARNING) {
        /*rv = SEC_MakeCertPostWarnDialog(proto_win, cert);*/
        rv = SECSuccess;
    }
    else if (error == SEC_ERROR_EXPIRED_CERTIFICATE) {
        rv = SSM_SSLMakeCertExpiredDialog(cert, conn);
    } 
    else if (error == SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE) {
        /*rv = SEC_MakeCAExpiredDialog(proto_win, cert);*/
        rv = SECSuccess;
    }
    else {
        rv = SECFailure;
    }
    return rv;
}


SECStatus SSM_DoubleCheckServerCert(CERTCertificate* cert,
                                    SSMSSLDataConnection* conn)
{
    PRBool checkSig = PR_FALSE;
    /* ### sjlee: it is bit discomforting to set checkSig to false, but
     *     the user already decided to be generous about the cert, so it
     *     could be OK???
     */
    SSM_DEBUG("One server cert problem was handled.  See if there's more.\n");

    return SSM_SSLVerifyServerCert(SSMCONTROLCONNECTION(conn)->m_certdb, cert, 
                                   checkSig, conn);
}


/* This function does the same thing as SSL_AuthCertificate(),
** but takes different arguments.  
*/
SECStatus SSM_SSLVerifyServerCert(CERTCertDBHandle* handle,
                                  CERTCertificate* cert, PRBool checkSig, 
                                  SSMSSLDataConnection* conn)
{
    SECStatus rv;
    char* hostname = NULL;

    rv = CERT_VerifyCertNow(handle, cert, checkSig, certUsageSSLServer,
                            (void*)conn);
    if (rv != SECSuccess) {
	return rv;
    }

    /* cert is OK. we will check the name field in the cert against the
     * desired hostname.
     */
    hostname = SSL_RevealURL(conn->socketSSL);
    if (hostname && hostname[0]) {
        rv = CERT_VerifyCertName(cert, hostname);
    }
    else {
        rv = SECFailure;
    }

    if (rv != SECSuccess) {
	PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
    }
    if (hostname)
    	PR_Free(hostname);

    return rv;
}

/* The following routines are callbacks and assorted functions that are used
 * with SSL/LDAP.
 */
/* SSL client-auth callback used for SSL/LDAP */
SECStatus SSM_LDAPSSLGetClientAuthData(void* arg, PRFileDesc* socket,
                                       CERTDistNames* caNames,
                                       CERTCertificate** pRetCert,
                                       SECKEYPrivateKey** pRetKey)
{
    SECStatus rv = SECFailure;
    void* wincx = NULL;
    PRArenaPool* arena = NULL;
    char** caNameStrings = NULL;
    CERTCertList* certList = NULL;
    CERTCertListNode* node;
    CERTCertificate* cert = NULL;
    SECKEYPrivateKey* privKey = NULL;
    CERTCertDBHandle* certdb;

    SSM_DEBUG("Doing client auth for LDAP/SSL.\n");

    /* check arguments */
    if ((arg == NULL) || (socket == NULL) || (caNames == NULL) || 
        (pRetCert == NULL) || (pRetKey == NULL)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return SECFailure;
    }

    /* get PKCS11 pin argument */
    wincx = SSL_RevealPinArg(socket);
    if (wincx == NULL) {
        return SECFailure;
    }

    /* create caNameStrings */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto loser;
    }

    caNameStrings = (char**)PORT_ArenaAlloc(arena, 
                                            sizeof(char*)*(caNames->nnames));
    if (caNameStrings == NULL) {
        goto loser;
    }

    /* XXX to do: publish the following function in sslconn.h */
    rv = SSM_ConvertCANamesToStrings(arena, caNameStrings, caNames);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* we will just do automatic cert selection for SSL/LDAP */
    /* get the cert DB */
    certdb = (CERTCertDBHandle*)arg;

    /* find all user certs that are valid and for SSL */
    certList = CERT_FindUserCertsByUsage(certdb, certUsageSSLClient, PR_TRUE,
                                         PR_TRUE, wincx);
    if (certList == NULL) {
        goto noCert;
    }

    /* filter the list to those issued by CAs supported by the server */
    rv = CERT_FilterCertListByCANames(certList, caNames->nnames, caNameStrings,
                                      certUsageSSLClient);
    if (rv != SECSuccess) {
        goto noCert;
    }

    /* make sure the list is not empty */
    node = CERT_LIST_HEAD(certList);
    if (CERT_LIST_END(node, certList)) {
        goto noCert;
    }

    /* loop through the list until we find a cert with a key */
    while (!CERT_LIST_END(node, certList)) {
        privKey = PK11_FindKeyByAnyCert(node->cert, wincx);
        if (privKey != NULL) {
            /* this is a good cert to present */
            cert = CERT_DupCertificate(node->cert);
            break;
        }
        node = CERT_LIST_NEXT(node);
    }
    if (cert == NULL) {
        goto noCert;
    }

    goto done;
noCert:
    PR_SetError(SSL_ERROR_NO_CERTIFICATE, 0);
    rv = SECFailure;
loser:
    if (rv == SECSuccess) {
        rv = SECFailure;
    }
    if (cert != NULL) {
        CERT_DestroyCertificate(cert);
        cert = NULL;
    }
done:
    if (certList != NULL) {
        CERT_DestroyCertList(certList);
    }
    if (arena != NULL) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    *pRetCert = cert;
    *pRetKey = privKey;

    return rv;
}

/* This function must get called when the socket was created but was not
 * SSL-ified or connected
 */
SECStatus SSM_LDAPSetupSSL(PRFileDesc* socket, PRFileDesc** sslSocket,
                           CERTCertDBHandle* certdb, const char* hostname, 
                           SSMResource* caller)
{
    SECStatus rv = SECFailure;

    /* check arguments */
    if ((socket == NULL) || (sslSocket == NULL) || (certdb == NULL) ||
        (hostname == NULL) || (caller == NULL)) {
        goto loser;
    }
    *sslSocket = NULL;

    /* import the socket into SSL layer */
    *sslSocket = SSL_ImportFD(NULL, socket);
    if (*sslSocket == NULL) {
        goto loser;
    }

    /* set some SSL settings for the socket */
    rv = SSL_Enable(*sslSocket, SSL_SECURITY, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SSL_Enable(*sslSocket, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* set callbacks */
    rv = SSL_GetClientAuthDataHook(*sslSocket, 
                                   (SSLGetClientAuthData)SSM_LDAPSSLGetClientAuthData,
                                   (void*)certdb) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SSL_AuthCertificateHook(*sslSocket, 
                                 (SSLAuthCertificate)SSM_SSLAuthCertificate,
                                 (void*)certdb) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SSL_SetPKCS11PinArg(*sslSocket, (void*)caller) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SSL_SetURL(*sslSocket, hostname) == 0 ? SECSuccess : SECFailure;
    if (rv != SECSuccess) {
        goto loser;
    }

    return rv;
loser:
    if (*sslSocket == NULL) {
        if (socket != NULL) {
            PR_Close(socket);
        }
    }
    else {
        PR_Close(*sslSocket);
        *sslSocket = NULL;
    }
    return rv;
}


