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
#ifndef __ADVISOR_H__
#define __ADVISOR_H__
/*
  Security advisor object. We create this in order to determine what to show
  in the generic "Security Info" pane, and to convey the right URL back to
  the client for loading.

  Created by mwelch 1999 February
 */
#include "resource.h"
#include "ctrlconn.h"
#include "certres.h"
#include "p7cinfo.h"
#include "slist.h"
#include "sslskst.h"

typedef struct InfoSecAdvisor
{
    PRInt32 infoContext;
    PRInt32 resID;
    char *hostname;
	char *senderAddr;
	PRUint32 encryptedP7CInfo;
	PRUint32 signedP7CInfo;
	PRInt32 decodeError;
	PRInt32 verifyError;
	PRInt32 encryptthis;
	PRInt32 signthis;
	PRInt32 numRecipients;
	char **recipients;
} InfoSecAdvisor;

typedef struct SSMSecurityAdvisorContext
{
    SSMResource super;

    /* Reference to Cartman object which we use to display 
       "Security Info" pane */
    SSMResource *m_infoSource;

    /* Width and height of the security advisor window, 
       loaded from resource file */
    PRUint32 m_width, m_height;

    /* Cached URL which we send back to client upon request */
    char *m_url;

    SSMSortedList *m_certhash;
    PRIntn m_certsIncluded;

    char *m_nickname;
    PRInt32 infoContext;
    PRInt32 resID;
    char *hostname;
	char *senderAddr;
	PRUint32 encryptedP7CInfo;
	PRUint32 signedP7CInfo;
	PRInt32 decodeError;
	PRInt32 verifyError;
	PRInt32 encryptthis;
	PRInt32 signthis;
	PRInt32 numRecipients;
	char **recipients;
    char *selectedItemPage;
	PRInt32 encrypted_b;
	PRInt32 signed_b;
	SSMP7ContentInfo * encryptedP7CInfoRes;
	SSMP7ContentInfo * signedP7CInfoRes;
    SSMSSLSocketStatus * socketStatus;
#if 0
    /* data for Java principals */
    char* m_principalsData;
#endif
} SSMSecurityAdvisorContext;

SSMStatus SSMSecurityAdvisorContext_Create(SSMControlConnection *ctrl,
                                          InfoSecAdvisor *info, 
                                          SSMResource **res);

SSMStatus SSMSecurityAdvisorContext_Destroy(SSMResource *res, PRBool doFree);
                                     
void SSMSecurityAdvisorContext_Invariant(SSMSecurityAdvisorContext *ct);


SSMStatus SSMSecurityAdvisorContext_GetAttrIDs(SSMResource *res,
                                              SSMAttributeID **ids,
                                              PRIntn *count);

SSMStatus SSMSecurityAdvisorContext_GetAttr(SSMResource *res,
                                           SSMAttributeID attrID,
                                           SSMResourceAttrType attrType,
                                           SSMAttributeValue *value);

SSMStatus SSMSecurityAdvisorContext_SetAttr(SSMResource *res,
                                  SSMAttributeID attrID,
                                  SSMAttributeValue *value);

SSMStatus SSMSecurityAdvisorContext_FormSubmitHandler(SSMResource *res,
                                                      HTTPRequest *req);

SSMStatus SSMSecurityAdvisorContext_Print(SSMResource *res,
                                         char *fmt, PRIntn numParam,
                                         char **value, char **resultStr);

SSMStatus SSMSecurityAdvisorContext_DoPKCS12Response(SSMSecurityAdvisorContext *cx,
                                                     HTTPRequest *req,
                                           			const char  *responseKey);

SSMStatus SSMSecurityAdvisorContext_DoPKCS12Restore(SSMSecurityAdvisorContext *res,
													HTTPRequest               *req);

SSMStatus SSMSecurityAdvisorContext_DoPKCS12Backup(SSMSecurityAdvisorContext *cx,
                                              		HTTPRequest               *req);

SSMStatus SSMSecurityAdvisorContext_Process_cert_mine_form(SSMSecurityAdvisorContext *res,
                                                			HTTPRequest *req);
SSMStatus SSMSecurityAdvisorContext_sa_selected_item(SSMTextGenContext* cx);

SSMStatus SSM_FreeTarget(SSMTextGenContext *cx);

SSMStatus 
SSMSecurityAdvisorContext_GetPrefListKeywordHandler(SSMTextGenContext* cx);
SSMStatus SSM_SetDBPasswordHandler(HTTPRequest * req);
#if 0
SSMStatus SSM_JavaPrincipalsKeywordHandler(SSMTextGenContext* cx);
void SSM_HandleGetJavaPrincipalsReply(SSMControlConnection* ctrl,
                                      SECItem* message);
#endif
SSMStatus SSM_RemovePrivilegesHandler(HTTPRequest* req);
SSMStatus SSM_OCSPResponderList(SSMTextGenContext* cx);
char* SSM_GetOCSPURL(CERTCertificate *cert, PrefSet *prefs);
PRBool SSM_IsOCSPEnabled(SSMControlConnection *connection);
SSMStatus SSM_DisplayCRLButton(SSMTextGenContext *cx);
SSMStatus SSM_ListCRLs(SSMTextGenContext *cx);
SSMStatus SSM_LayoutSMIMETab(SSMTextGenContext *cx);
SSMStatus SSM_LayoutJavaJSTab(SSMTextGenContext *cx);
SSMStatus SSM_LayoutOthersTab(SSMTextGenContext *cx);
#endif
