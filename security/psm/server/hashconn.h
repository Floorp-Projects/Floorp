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
#ifndef __SSM_HASHCONN_H__
#define __SSM_HASHCONN_H__

#include "prerror.h"
#include "sechash.h"

#include "ctrlconn.h"
#include "dataconn.h"

#define SSM_HASH_RESULT_LEN 1024

/* Used to initialize an SSMHashConnection */
typedef struct SSMHashInitializer
{
    SSMControlConnection *m_parent;
    HASH_HashType m_hashtype;
} SSMHashInitializer;

/*
  Hash connection object 
 */

typedef struct SSMHashConnection
{
    SSMDataConnection super;

    HASHContext    *m_context;
    HASH_HashType  m_type;
    PRUint32       m_resultLen;
    unsigned char  m_result[SSM_HASH_RESULT_LEN];
} SSMHashConnection;

SSMStatus SSMHashConnection_Create(void *arg, SSMControlConnection * conn, 
                                  SSMResource **res);
SSMStatus SSMHashConnection_Init(SSMHashConnection *conn, 
                                SSMHashInitializer *init,
                                SSMResourceType type);
SSMStatus SSMHashConnection_Destroy(SSMResource *res, PRBool doFree);
void SSMHashConnection_Invariant(SSMHashConnection *conn);

SSMStatus SSMHashConnection_Shutdown(SSMResource *arg, SSMStatus status);

SSMStatus SSMHashConnection_GetAttrIDs(SSMResource *res,
                                         SSMAttributeID **ids,
                                         PRIntn *count);

SSMStatus SSMHashConnection_GetAttr(SSMResource *res,
                                    SSMAttributeID attrID,
                                    SSMResourceAttrType attrType,
                                    SSMAttributeValue *value);

#endif
