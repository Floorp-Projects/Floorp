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
#include "textgen.h"
#include "minihttp.h"
#include "certres.h"

typedef enum
{
  certHashRemove = (int) -1, 
  certHashAdd = 1
} ssmCertHashAction;

/* Since there isn't a strict 1-1 match between SSLCA and SECCertUsage
   (we lump SSL and SSL-with-step-up certs together at the moment, for eg),
   we have an enum of our own to indicate what to get from the cert db. */
typedef enum
{
    clNoUsage = -1,
    clAllMine = 0,
    clSSLClient,
    clEmailSigner,
    clMyObjSigning,
    clSSLServer,
    clEmailRecipient,
    clAllCA
} ssmCLCertUsage;

typedef struct 
{
    char * certEntry;
    ssmCLCertUsage usage;
    PRBool isEmailSigner;
} ssmCertData;

SSMStatus SSM_CertListKeywordHandler(SSMTextGenContext *cx);
SSMStatus SSM_SetLocaleCommandHandler(HTTPRequest *req);
SSMStatus SSM_ReloadTextCommandHandler(HTTPRequest *req);
SSMStatus SSM_ChangeCertSecAdvisorList(HTTPRequest * req, char * nickname, 
				       ssmCertHashAction action);

PRIntn certlist_compare_strings(const void * v1, const void *v2);
