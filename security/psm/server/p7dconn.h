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
#ifndef __SSM_P7DCONN_H__
#define __SSM_P7DCONN_H__

#include "prerror.h"
#include "secpkcs7.h"

#include "dataconn.h"
#include "p7cinfo.h"

/* Initialization parameters for an SSMP7DecodeConnection. */
typedef struct SSMInfoP7Decode {
    SSMControlConnection *ctrl;
    CMTItem clientContext;
} SSMInfoP7Decode;

/*
  PKCS7 decoder data connections.
 */

typedef struct SSMP7DecodeConnection
{
    SSMDataConnection super;

    SEC_PKCS7DecoderContext *m_context;
    SSMP7ContentInfo *m_cinfo;
	PRInt32 m_error;
} SSMP7DecodeConnection;

SSMStatus SSMP7DecodeConnection_Create(void *arg, SSMControlConnection * conn,
                                      SSMResource **res);
SSMStatus SSMP7DecodeConnection_Init(SSMP7DecodeConnection *conn, 
                                    SSMInfoP7Decode *info,
                                    SSMResourceType type);
SSMStatus SSMP7DecodeConnection_Destroy(SSMResource *res, PRBool doFree);
void SSMP7DecodeConnection_Invariant(SSMP7DecodeConnection *conn);

SSMStatus SSMP7DecodeConnection_Shutdown(SSMResource *arg, SSMStatus status);

SSMStatus SSMP7DecodeConnection_GetAttrIDs(SSMResource *res,
                                         SSMAttributeID **ids,
                                         PRIntn *count);

SSMStatus SSMP7DecodeConnection_GetAttr(SSMResource *res,
                                        SSMAttributeID attrID,
                                        SSMResourceAttrType attrType,
                                        SSMAttributeValue *value);

#endif
