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
#ifndef __SSM_SSLCONN_H__
#define __SSM_SSLCONN_H__

#include <stdio.h>
#include <string.h>
#include "prerror.h"
#include "pk11func.h"
#include "secitem.h"
#include "ssl.h"
#include "prnetdb.h"
#include "certt.h"
#include "nss.h"
#include "nspr.h"
#include "secrng.h"
#include "secder.h"
#include "key.h"
#include "sslproto.h"

#include "ctrlconn.h"
#include "dataconn.h"
#include "sslskst.h"

/* Initialization parameters for an SSMSSLDataConnection. */
typedef struct SSMInfoSSL {
    SSMControlConnection* parent;
    char* hostIP;
    char* hostName;
    PRUint32 flags;
    PRUint32 port;
    PRBool forceHandshake;
    PRBool isTLS;
    CMTItem clientContext;
} SSMInfoSSL;

/* enum for determining how long we will accept this server cert */
typedef enum {BSCA_PERMANENT = 0, BSCA_SESSION = 1, BSCA_NO = 2} SSMBadServerCertAccept;

/* data related with UI */
typedef struct _SSMSSLUIInfo {
    PRBool UICompleted;
    int numFilteredCerts;
    char** certNicknames;
    int chosen;    /* index of the chosen cert */

    /* fields for bad server cert UI */
    SSMBadServerCertAccept trustBadServerCert;
    /* whether the user decided to use the cert */
} SSMSSLUIInfo;

/*
  Data connections.
 */

typedef struct SSMSSLDataConnection {
    SSMDataConnection super;

    PRBool clientSSL;
    char* hostIP;
    char* hostName;
    PRUint32 flags;
    PRUint32 port;
    PRBool forceHandshake;
    PRBool isTLS; /* TRUE if the connection starts out as a cleartext and
                   * is SSL-ified later
                   */
    PRBool isSecure;    /* SMTP | Proxy: whether the connection is secure
                         * SSL: always true
                         */
    PRFileDesc* socketSSL;
    PRFileDesc* stepUpFD; /* FD used by control connection to wake up
                             SSL thread when it's time to enable TLS/SSL
                             on a previously cleartext connection (eg SMTP) */
    CMTItem* stepUpContext; /* Client context used for step-up */

#if 0
    PRMonitor* sslLock;
#endif

    SSMSSLSocketStatus* m_sockStat;
    PRInt32 m_error;

    SSMSSLUIInfo m_UIInfo;    /* field related with UI action */
	PRInt32 m_sslServerError;
    PRBool m_statusFetched;
} SSMSSLDataConnection;

SSMStatus SSMSSLDataConnection_Create(void* arg, 
									 SSMControlConnection* ctrlconn, 
                                     SSMResource** res);
SSMStatus SSMSSLDataConnection_Shutdown(SSMResource* arg, SSMStatus status);
SSMStatus SSMSSLDataConnection_Init(SSMSSLDataConnection* conn, 
				   SSMInfoSSL* info, SSMResourceType type);
SSMStatus SSMSSLDataConnection_Destroy(SSMResource* res, PRBool doFree);
void SSMSSLDataConnection_Invariant(SSMSSLDataConnection* conn);

SSMStatus SSMSSLDataConnection_GetAttrIDs(SSMResource* res,
                                         SSMAttributeID** ids, PRIntn* count);
SSMStatus SSMSSLDataConnection_GetAttr(SSMResource* res, SSMAttributeID attrID,
                                       SSMResourceAttrType attrType,
                                       SSMAttributeValue *value);
SSMStatus SSMSSLDataConnection_SetAttr(SSMResource * res,
                                       SSMResourceAttrType attrID,
                                       SSMAttributeValue *value);

SSMStatus SSMSSLDataConnection_PickleSecurityStatus(SSMSSLDataConnection* conn,
                                                   PRIntn* len, void** blob,
                                                   PRIntn* securityLevel);
SSMStatus SSMSSLDataConnection_FormSubmitHandler(SSMResource* res,
                                                 HTTPRequest* req);
SECStatus SSM_DoubleCheckServerCert(CERTCertificate* cert,
                                    SSMSSLDataConnection* conn);
SECStatus SSM_LDAPSSLGetClientAuthData(void* arg, PRFileDesc* socket,
                                       CERTDistNames* caNames,
                                       CERTCertificate** pRetCert,
                                       SECKEYPrivateKey** pRetKey);
SECStatus SSM_LDAPSetupSSL(PRFileDesc* socket, PRFileDesc** sslSocket,
                           CERTCertDBHandle* certdb, const char* hostname, 
                           SSMResource* caller);

#endif /* __SSM_SSLCONN_H__ */
