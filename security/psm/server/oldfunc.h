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
#ifndef _OLDFUNC_H_
#define _OLDFUNC_H_

#include "keyres.h"
#include "base64.h"
#include "secasn1.h"
#include "secder.h"
#include "certt.h"
#include "certdb.h"
#include "cryptohi.h"
#include "kgenctxt.h"
#include "textgen.h"
#include "secdert.h"
#include "minihttp.h"
#include "ssldlgs.h"

#define SSM_MAX_KEY_BITS  2048L

/* this needs to be localized */
#define XP_HIGH_GRADE      "1024 (High Grade)"
#define XP_MEDIUM_GRADE    " 768 (Medium Grade)"
#define XP_LOW_GRADE       " 512 (Low Grade)"

typedef struct SECKeySizeChoiceInfoStr {
    char *name;
    int size;
    unsigned int enabled;
} SECKeySizeChoiceInfo;


extern void _ssm_compress_spaces(char *psrc);
SSMStatus SSM_OKButtonCommandHandler(HTTPRequest *req);
SSMStatus SSM_CertCAImportCommandHandler1(HTTPRequest * req);
SSMStatus SSM_CertCAImportCommandHandler2(HTTPRequest * req);
SSMStatus SSM_FinishImportCACertHandler(HTTPRequest * req);
SSMStatus SSM_SubmitFormFromButtonHandler(HTTPRequest * req);
SSMStatus SSM_SubmitFormFromButtonAndFreeTarget(HTTPRequest * req);
SSMStatus SSM_CACertKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_CAPolicyKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_EnableHighGradeKeyGen(void);
PRBool SSM_KeyGenAllowedForSize(int size);

char * default_nickname(CERTCertificate *cert, SSMControlConnection *conn);
PRBool certificate_conflict(SSMControlConnection * cx, SECItem * derCert,
                            CERTCertDBHandle * handle, PRBool sendUIEvent);
#endif
