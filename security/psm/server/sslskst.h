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
#ifndef __SSM_SSLSKST_H__
#define __SSM_SSLSKST_H__

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
#include "certres.h"

/* Initialization parameters for an SSMSSLSocketStatus. */

/*
  SSL Socket Status object. This encapsulates the information about an SSL
  connection.
 */

typedef struct SSMSSLSocketStatus
{
    SSMResource super;

    PRUint32 m_keySize;
    PRUint32 m_secretKeySize;
    PRInt32 m_level;
    char *m_cipherName;
	PRInt32 m_error;
    SSMResourceCert *m_cert; /* placeholder - will we want to get an
                                SSMCertificate object ref later? */
} SSMSSLSocketStatus;

SSMStatus SSMSSLSocketStatus_Create(void *arg, SSMControlConnection * conn, 
                                   SSMResource **res);
SSMStatus SSMSSLSocketStatus_Init(SSMSSLSocketStatus *ss, 
                                 SSMControlConnection * connection,
                                 PRFileDesc *fd, SSMResourceType type);
SSMStatus SSMSSLSocketStatus_Destroy(SSMResource *res, PRBool doFree);
void SSMSSLSocketStatus_Invariant(SSMSSLSocketStatus *ss);

SSMStatus SSMSSLSocketStatus_GetAttrIDs(SSMResource *res,
                                       SSMAttributeID **ids,
                                       PRIntn *count);

SSMStatus SSMSSLSocketStatus_GetAttr(SSMResource *res,
                                     SSMAttributeID attrID,
                                     SSMResourceAttrType attrType,
                                     SSMAttributeValue *value);
SSMStatus SSMSSLSocketStatus_Pickle(SSMResource *res, PRIntn * len,
                                   void **value);
SSMStatus SSMSSLSocketStatus_Unpickle(SSMResource **res, 
                                     SSMControlConnection * conn, PRIntn len, 
                                     void *value);
SSMStatus SSMSSLSocketStatus_HTML(SSMResource *res, PRIntn *len, void ** value);
/*
 * Function: SSMStatus SSMSSLSocketStatus_UpdateSecurityStatus()
 * Purpose: updates the security status
 * Arguments and return values:
 * - ss: socket status (it should not be NULL when called)
 * - socket: SSL socket
 * - returns: PR_SUCCESS if the update is successful; PR_FAILURE otherwise
 *
 * Note: if the update fails, one can keep doing other work but (obviously)
 *       should not attempt to use any of the security-related values
 */
SSMStatus SSMSSLSocketStatus_UpdateSecurityStatus(SSMSSLSocketStatus* ss,
                                                 PRFileDesc* socket);

/*
 * The following string are used to present security status in HTML format.
 */
#ifdef XP_UNIX

#define SECURITY_LOW_MESSAGE                        "This is a secure document that uses a medium-grade encryption key suited\nfor U.S. export"

#define SECURITY_HIGH_MESSAGE                        "This is a secure document that uses a high-grade encryption key for U.S.\ndomestic use only"

#define SECURITY_NO_MESSAGE                          "This is an insecure document that is not encrypted and offers no security\nprotection."

#else

#define SECURITY_LOW_MESSAGE                         "This is a secure document that uses a medium-grade encryption key suited for U.S. export"

#define SECURITY_HIGH_MESSAGE                         "This is a secure document that uses a high-grade encryption key for U.S. domestic use only"

#define SECURITY_NO_MESSAGE                           "This is an insecure document that is not encrypted and offers no security protection."

#endif


#endif
