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
#ifndef __SIGNTEXTRES_H__
#define __SIGNTEXTRES_H__

#include "resource.h"
#include "ctrlconn.h"
#include "textgen.h"

typedef enum {
    SIGN_TEXT_INIT,
    SIGN_TEXT_WAITING,
    SIGN_TEXT_OK,
    SIGN_TEXT_CANCEL
} SignTextAction;

typedef struct
{
    SSMResource super;
    char *m_stringToSign;
    char *m_hostName;
    PRBool m_autoFlag;
    PRInt32 m_numCAs;
    char **m_caNames;
    SSMControlConnection *m_conn;
    PRThread *m_thread;
    CERTCertList *m_certs;    
    SignTextAction m_action;
    char *m_nickname;
    char *m_resultString;
} SSMSignTextResource;

SSMStatus SSM_CreateSignTextRequest(SECItem* msg, SSMControlConnection* conn);
SSMStatus SSM_CreateNewSignTextRequest(SECItem *msg, SSMControlConnection *conn);
SSMStatus SSMSignTextResource_Create(void *arg, SSMControlConnection *conn, SSMResource **res);
SSMStatus SSMSignTextResource_Init(SSMControlConnection *conn, SSMSignTextResource *res, SSMResourceType type);
SSMStatus SSMSignTextResource_Destroy(SSMResource *res, PRBool doFree);
SSMStatus SSMSignTextResource_Shutdown(SSMResource *res, SSMStatus status);
SSMStatus SSMSignTextResource_GetAttr(SSMResource *res, SSMAttributeID attrID,
				      SSMResourceAttrType attrType, 
				      SSMAttributeValue *value);
SSMStatus SSMSignTextResource_Print(SSMResource *res, char *fmt, PRIntn numParam,
				   char ** value, char **resultStr);
SSMStatus SSMSignTextResource_FormSubmitHandler(SSMResource* res, HTTPRequest* req);
void SSMSignTextResource_ServiceThread(void *arg);
SSMStatus SSM_SignTextCertListKeywordHandler(SSMTextGenContext* cx);
SSMStatus ssm_signtext_get_unicode_cert_list(SSMSignTextResource* res, 
					     char* fmt, 
					     char** result);
SSMStatus SSM_CreateSignTextRequest(SECItem *msg, SSMControlConnection *conn);
#endif /* __SIGNTEXTRES_H__ */
