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
#ifndef __SSM_P7ECONN_H__
#define __SSM_P7ECONN_H__

#include "prerror.h"
#include "secpkcs7.h"

#include "dataconn.h"
#include "p7cinfo.h"

/*
  PKCS7 encoder data connections.
 */

/* Initialization parameters for an SSMP7EncodeConnection. */
typedef struct SSMInfoP7Encode {
    SSMControlConnection *ctrl;
    SSMResourceID ciRID;
    CMTItem clientContext;
} SSMInfoP7Encode;

typedef struct SSMP7EncodeConnection {
    SSMDataConnection super;

    SEC_PKCS7EncoderContext *m_context;
    SSMP7ContentInfo *m_cinfo;
    PRInt32 m_retValue;
    PRInt32 m_error;
} SSMP7EncodeConnection;


SSMStatus SSMP7EncodeConnection_Create(void *arg, SSMControlConnection * conn,
                                      SSMResource **res);
SSMStatus SSMP7EncodeConnection_Init(SSMP7EncodeConnection *conn,
                                    SSMInfoP7Encode *info,
                                    SSMResourceType type);
SSMStatus SSMP7EncodeConnection_Destroy(SSMResource *res, PRBool doFree);
void SSMP7EncodeConnection_Invariant(SSMP7EncodeConnection *conn);

SSMStatus SSMP7EncodeConnection_Shutdown(SSMResource *arg, SSMStatus status);

SSMStatus SSMP7EncodeConnection_GetAttrIDs(SSMResource *res,
                                         SSMAttributeID **ids,
                                         PRIntn *count);

SSMStatus SSMP7EncodeConnection_GetAttr(SSMResource *res,
                                        SSMAttributeID attrID,
                                        SSMResourceAttrType attrType,
                                        SSMAttributeValue *value);

SSMStatus SSMP7EncodeConnection_SetAttr(SSMResource *res,
                                 SSMAttributeID attrID,
                                 SSMAttributeValue *value);

#endif  /* __SSM_P7ECONN_H__ */
