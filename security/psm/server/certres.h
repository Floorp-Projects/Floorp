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

#ifndef __NSM_CERTRES_H__
#define __NSM_CERTRES_H__

#include "servimpl.h"
#include "resource.h"
#include "cert.h"
#include "certdb.h"
#include "secder.h"
#include "ctrlconn.h"
#include "minihttp.h"
#include "textgen.h"

struct _ssm_resource_cert;

typedef SSMStatus (*SSMCertResourceVerifyFunc)
     (struct _ssm_resource_cert * resource,
      SECCertUsage certUsage); 
typedef SSMStatus (*SSMCertResourceDeleteCertFunc)
     (struct _ssm_resource_cert * resource);
				    

typedef struct _ssm_resource_cert
{ 
  SSMResource super;
  CERTCertificate * cert;
  SSMCertResourceVerifyFunc m_verify_func;
  SSMCertResourceDeleteCertFunc m_deletecert_func;
  PRBool m_markedForDeletion;
  PRBool m_renewCert;
} SSMResourceCert;

/* 
 * Cert resource can be created from a CERTCertificate or when unpickling. 
 */
typedef enum {
  SSM_RESOURCE_CERT_CREATE_NEW = 0L,
  SSM_RESOURCE_CERT_CREATE_UNPICKLE  
} SSMResourceCertCreate;

/* Functions of the certResource class */
SSMStatus SSMResourceCert_Create(void * arg, SSMControlConnection * conn,
				SSMResource **res);
SSMStatus SSMResourceCert_Init(SSMResourceCert * certResource, 
			      SSMControlConnection * conn,
			      void * arg); 

SSMStatus SSMResourceCert_GetAttr(SSMResource *res,
				  SSMAttributeID attrID,
				  SSMResourceAttrType attrType,
				  SSMAttributeValue *value);
SSMStatus SSMResourceCert_GetAttrIDs(SSMResource *res,
				    SSMAttributeID **ids,
				    PRIntn *count);
SSMStatus SSMResourceCert_Destroy(SSMResource *res, PRBool doFree);
SSMStatus SSMResourceCert_Pickle(SSMResource *res, PRIntn *len, void ** rdata);
SSMStatus SSMResourceCert_Verify(SSMResourceCert * resource, 
				SECCertUsage certUsage);
SSMStatus SSMResourceCert_DeleteCert(SSMResourceCert * resource);

SSMStatus SSMResourceCert_Unpickle(SSMResource ** resource, 
				  SSMControlConnection * conn, PRInt32 len,
                                  void * value);
SSMStatus SSMResourceCert_HTML(SSMResource *res, PRIntn * len, void ** value);

SSMStatus SSMResourceCert_FormSubmitHandler(SSMResource *res, HTTPRequest *req);


/* Other cert resource functions */
SSMStatus SSM_VerifyCert(SSMResourceCert  * resource,
                        SECCertUsage certUsage);
SSMStatus SSM_DeleteCert(SSMResourceCert * resource);
SSMStatus SSM_DeleteCertHandler(HTTPRequest * req);
SSMStatus SSM_HTTPCertListHandler(HTTPRequest * req);
SSMStatus SSM_ViewCertInfoKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_VerifyCertKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_SelectCertKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_ChooseCertUsageHandler(HTTPRequest * req);
SSMStatus SSM_EditCertKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_EditCertificateTrustHandler(HTTPRequest * req);
SSMStatus SSM_DeleteCertHelpKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_DeleteCertWarnKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_ObtainNewCertSite(SSMTextGenContext * cx);
SSMStatus SSM_ProcessLDAPRequestHandler(HTTPRequest * req);
SSMStatus SSM_LDAPServerListKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_ProcessLDAPWindow(HTTPRequest * req);
SSMStatus SSM_ShowCertIssuer(HTTPRequest *req);
SSMStatus SSM_GetWindowOffset(SSMTextGenContext *cx);
SSMStatus SSM_PrettyPrintCert(HTTPRequest *req);

SSMStatus
SSM_ProcessCertUIAction(HTTPRequest * req, CERTCertificate * cert);
SSMStatus SSM_ProcessCertDeleteButton(HTTPRequest * req);

int SSM_CertListCount(CERTCertList *certList);

SSMStatus SSM_OCSPOptionsKeywordHandler(SSMTextGenContext *cx);
SSMStatus SSM_OCSPDefaultResponderKeywordHandler(SSMTextGenContext *cx);
SSMStatus SSM_FillTextWithEmails(SSMTextGenContext *cx);
SSMStatus SSM_MakeUniqueNameForIssuerWindow(SSMTextGenContext *cx);

SSMStatus 
SSM_CompleteLDAPLookup(SSMControlConnection *ctrl, char * ldapserver, 
			char * emailaddr);
char * SSM_GetCAPolicyString(char * org, unsigned long noticeNum, void * arg);

#endif
