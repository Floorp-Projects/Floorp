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
#ifndef __SSM_DATACONN_H__
#define __SSM_DATACONN_H__

#include "connect.h"
#include "ctrlconn.h"

/*
  Cartman data connection/thread architecture:

  Data connections are opened by the control connection in response
  to a Request {SSL,etc.} Data message. There is one thread per data
  connection that handles the data traffic, and there is one queue that
  receives a shutdown message from a control thread.

  If an exception or other termination occurs on either side, the 
  m_status flag is changed to the specific exception code, and sockets 
  are closed on either side (after final thread shutdown).
  (### mwelch This has the effect of setting SO_LINGER on the outgoing 
  data stream, is this an issue?)
 */


/*
  Data connection objects.
 */

typedef struct SSMDataConnection
{
    SSMConnection super;

    /* 
       ---------------------------------------------
       Data connection-specific fields - need to 
       merge with control fields or do something more 
       elegant in the long term.
       ---------------------------------------------
    */
    PRUint32 m_dataType;           /* What type of service are we 
                                      providing to client? */

    PRThread* m_dataServiceThread;    /* one and only data thread */

    SSMCollection* m_shutdownQ;    /* data queue that delivers a shutdown
                                      message to the data service thread */

    PRFileDesc *m_clientSocket;    /* Client socket */

    PRBool m_sendResult;           /* When this connection shuts down,
                                      send a result back to the client
                                      (usage depends on subclass) */
} SSMDataConnection;

PR_BEGIN_EXTERN_C

PRBool AreConnectionsActive(void);


SSMStatus SSMDataConnection_Create(void *arg, SSMControlConnection * conn, 
                                  SSMResource **res);
SSMStatus SSMDataConnection_Init(SSMDataConnection *conn, 
                                SSMControlConnection *parent, 
                                SSMResourceType type);
SSMStatus SSMDataConnection_Destroy(SSMResource *res, PRBool doFree);
void SSMDataConnection_Invariant(SSMDataConnection *conn);

SSMStatus SSMDataConnection_GetAttrIDs(SSMResource *res,
                                       SSMAttributeID **ids,
                                       PRIntn *count);

SSMStatus SSMDataConnection_GetAttr(SSMResource *res,
                                    SSMAttributeID attrID,
                                    SSMResourceAttrType attrType,
                                    SSMAttributeValue *value);

SSMStatus SSMDataConnection_Shutdown(SSMResource *arg, SSMStatus status);
SSMStatus SSMDataConnection_Authenticate(SSMConnection *arg, char *nonce);

/*
 * Function: SSMStatus SSMDataConnection_SetupClientSocket()
 * Purpose: sets up the client data socket and authenticates the connection.
 *          (blocking I/O)
 * Arguments and return values:
 * - conn: data connection object
 * - returns: PR_SUCCESS if successful, error code otherwise
 */
SSMStatus SSMDataConnection_SetupClientSocket(SSMDataConnection* conn);

/*
 * Function: SSMStatus SSMDataConnection_ReadFromSocket()
 * Purpose: reads data from the client data socket and fill the buffer.  When
 *          particular write target threads read data from the data socket,
 *          it is recommended that they use this function with the notable
 *          exception of SSL connections.
 * Arguments and return values:
 * - conn: data connection object
 * - read: this is a value-result argument.  It should be set to the data
 *         chunk size.  One should always use LINESIZE unless there is a
 *         compelling reason to do otherwise.  On return, this value will be
 *         filled with the actual size of the data (or 0 if EOF or < 0 if
 *         error).
 * - buffer: data buffer.  The memory should have been allocated before this
 *           function is called.  The buffer size should be at least equal to
 *           the read size (thus the reason for specifying read value upon
 *           input).
 * - returns: PR_SUCCESS if successful, error code otherwise
 *
 * Note: this is a blocking I/O operation.
 */
SSMStatus SSMDataConnection_ReadFromSocket(SSMDataConnection* conn,
                                          PRInt32* read,
                                          char* buffer);
PR_END_EXTERN_C

#endif
