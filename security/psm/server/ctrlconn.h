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
#ifndef __SSM_CTRLCONN_H__
#define __SSM_CTRLCONN_H__

#include "connect.h"
#include "cert.h"
#include "key.h"
#include "hashtbl.h"
#include "prefs.h"

/*
  Control connections.
 */
extern SSMHashTable  * ctrlConnections;

struct SSMControlConnection
{
    SSMConnection super;

    /*
      ---------------------------------------------
      Control connection-specific fields
      ---------------------------------------------
    */

    PRUint32 m_version; /* Protocol version supported by client */
    char * m_nonce; /* Nonce used for verifying data connections */

    PRFileDesc * m_socket;   /* Socket serviced by this connection object */
    PRThread * m_writeThread;/* Write Control thread (writes m_socket) */
    PRThread * m_frontEndThread; /* Front end thread - reads m_socket */
    PRThread * m_certRenewalThread; /* Front end thread - reads m_socket */
    
    char * m_profileName; /* Name of user profile (where to find 
                             certs etc) */
    char * m_dirRoot; /* Path to directory for certs for control connection */

    /* Queue for outgoing messages */
    SSMCollection *m_controlOutQ;    /* Control msg queue: from readMsg thread 
                                     to writeMsg thread  */

    CERTCertDBHandle *m_certdb;
    SECKEYKeyDBHandle *m_keydb;

    /* Fields used for out-of-band password requests */
    SSMHashTable * m_passwdTable;
    PRMonitor * m_passwdLock;
    SSMHashTable * m_encrPasswdTable;
    PRMonitor * m_encrPasswdLock;
    PRInt32 m_waiting;

    SSMHashTable *m_resourceDB;
    SSMHashTable *m_classRegistry;

    SSMHashTable * m_resourceIdDB;
    SSMResourceID m_lastRID;
    SSMHashTable * m_certIdDB;
    SECItem * m_secAdvisorList;
    PRInt32 m_certNext;
    /* Data socket and port */
    PRFileDesc * m_dataSocket;
    PRIntn  m_dataPort;
    
    PRBool m_doesUI;

    PrefSet* m_prefs;
    PRBool m_pkcs11Init;
};

SSMStatus SSM_InitPolicyHandler(void);

SSMStatus SSMControlConnection_Create(void *arg, SSMControlConnection * conn, 
                                     SSMResource **res);
SSMStatus SSMControlConnection_Init(SSMControlConnection *res, 
                                   SSMResourceType type,
                                   PRFileDesc *socket);
SSMStatus SSMControlConnection_Shutdown(SSMResource *conn, SSMStatus status);
SSMStatus SSMControlConnection_Destroy(SSMResource *res, PRBool doFree);
SSMStatus SSMControlConnection_GetAttrIDs(SSMResource* res, SSMAttributeID** ids,
                                         PRIntn* count);
SSMStatus SSMControlConnection_GetAttr(SSMResource *res, SSMAttributeID attrID,
                                       SSMResourceAttrType attrType,
                                      SSMAttributeValue *value);
void SSMControlConnection_Invariant(SSMControlConnection *conn);

SSMStatus SSMControlConnection_ProcessMessage(SSMControlConnection* control, 
                                             SECItem* msg);
void SSM_WriteCtrlThread(void * arg);
void SSM_FrontEndThread(void * arg);
void SSM_CertificateRenewalThread(void * arg);

SSMStatus SSMControlConnection_Authenticate(SSMConnection *arg, char *nonce);
void SSMControlConnection_CertLookUp(SSMControlConnection * connection, 
                                     void * arg, SSMResource ** res);

SSMStatus SSMControlConnection_SendUIEvent(SSMControlConnection *conn,
                                           char *command,
                                           char *baseRef, 
                                           SSMResource *target, /* can pass NULL */
                                           char *otherParams /* can pass NULL */,
                                           CMTItem * clientContext /* can pass NULL */,
                                           PRBool isModal);

/*
 * NOTES
 * These functions save the pref change properly in memory and in client file.
 * They check first whether the value has changed and perform saving
 * operations.
 * These functions do not belong to the prefs API because these specifically
 * send the changes to the plugin.  Once we have our own prefs library ready
 * and complete the migration, these functions should be called only when
 * application-specific prefs are saved back to client pref file.
 * Since these functions pack one item only, if you have to send a lot of
 * pref changes and performance is critical, it is not recommended to call
 * these functions repeatedly.
 *
 */
SSMStatus SSMControlConnection_SaveStringPref(SSMControlConnection* ctrl,
                                              char* key, char* value);
SSMStatus SSMControlConnection_SaveBoolPref(SSMControlConnection* ctrl,
                                            char* key, PRBool value);
SSMStatus SSMControlConnection_SaveIntPref(SSMControlConnection* ctrl,
                                           char* key, PRIntn value);

void SSMControlConnection_CertLookUp(SSMControlConnection * connection,
                                     void * arg, SSMResource ** res);
void SSM_LockPasswdTable(SSMConnection * conn);
SSMStatus SSM_UnlockPasswdTable(SSMConnection *conn);
SSMStatus SSM_WaitPasswdTable(SSMConnection * conn);
SSMStatus SSM_NotifyAllPasswdTable(SSMConnection * conn);


SSMStatus SSMControlConnection_AddResource(SSMResource * res, SSMResourceID rid);

SSMStatus SSMControlConnection_GetResource(SSMControlConnection * connection,
                                          SSMResourceID rid,
                                          SSMResource ** res);
SSMStatus SSMControlConnection_GetGlobalResourceID(SSMControlConnection
                                                  *connection,
                                                  SSMResource * res,
                                                  SSMResourceID * rid);
SSMResourceID SSMControlConnection_GenerateResourceID(SSMControlConnection *conn);
SSMStatus SSM_GetControlConnection(SSMResourceID rid,
                                  SSMControlConnection **connection);
SSMStatus SSMControlConnection_FormSubmitHandler(SSMResource* res,
                                                 HTTPRequest* req);
void SSMControlConnection_RecycleItem(SECItem* msg);
SSMStatus SSMControlConnection_GenerateNonce(SSMControlConnection *conn);

/* from processmsg.c */
SSMStatus 
SSMControlConnection_ProcessVerifyCertRequest(SSMControlConnection * ctrl, 
                                              SECItem * msg);
SSMStatus
SSMControlConnection_ProcessImportCertRequest(SSMControlConnection * ctrl,
                                              SECItem * msg);
SSMStatus
SSMControlConnection_ProcessConserveRequest(SSMControlConnection * ctrl, 
                                            SECItem * msg);
SSMStatus 
SSMControlConnection_ProcessPickleRequest(SSMControlConnection * ctrl, 
                                          SECItem * msg);
SSMStatus 
SSMControlConnection_ProcessUnpickleRequest(SSMControlConnection * ctrl, 
                                            SECItem * msg);

SSMStatus 
SSMControlConnection_ProcessCertRequest(SSMControlConnection * ctrl, 
                                        SECItem * msg);

PRStatus
SSMControlConnection_ProcessKeygenTag(SSMControlConnection * ctrl,
                                        SECItem * msg);

SSMStatus
SSMControlConnection_ProcessPKCS11Request(SSMControlConnection * ctrl, 
					  SECItem * msg);

SSMStatus
SSMControlConnection_ProcessCRMFRequest(SSMControlConnection * ctrl,
					SECItem * msg);

SSMStatus
SSMControlConnection_ProcessMiscRequest(SSMControlConnection * ctrl,
                                        SECItem * msg);

SSMStatus
SSMControlConnection_ProcessFormSigningRequest(SSMControlConnection * ctrl,
                                        SECItem *msg);
SSMStatus 
SSMControlConnection_ProcessTLSRequest(SSMControlConnection * ctrl,
                                        SECItem *msg);
SSMStatus
SSMControlConnection_ProcessProxyStepUpRequest(SSMControlConnection* ctrl,
                                               SECItem* msg);
SSMStatus
SSMControlConnection_ProcessSecCfgRequest(SSMControlConnection * ctrl,
                                        SECItem *msg);
SSMStatus
SSMControlConnection_ProcessGenKeyOldStyleToken(SSMControlConnection * ctrl,
                                        SECItem *msg);
SSMStatus
SSMControlConnection_ProcessGenKeyPassword(SSMControlConnection * ctrl, 
					   SECItem *msg);
SSMStatus 
SSM_CertCAImportCommandHandler2(HTTPRequest * req);
void
ssm_ShutdownNSS(SSMControlConnection *ctrl);

SSMStatus
SSM_UseAsDefaultEmailIfNoneSet(SSMControlConnection *ctrl, 
                               CERTCertificate *cert, PRBool onFrontEndThread);

CERTCertList *
SSMControlConnection_CreateCertListByNickname(SSMControlConnection * ctrl,
                                              char * nick, PRBool email);

SSMStatus 
ssmcontrolconnection_encode_err_reply(SECItem *msg, SSMStatus rv);
SSMStatus
ssmcontrolconnection_send_message_to_client(SSMControlConnection *ctrl,
                                            SECItem *msg);
#endif /* __SSM_CTRLCONN_H__ */
