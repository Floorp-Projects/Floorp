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
#ifndef P12RES_H_
#define P12RES_H_
#include "resource.h"
#include "ctrlconn.h"
/*
 * Initially, this will only be used as a resource to use in doing UI
 * events necessary for PKCS12 operations.
 */

typedef struct SSMPKCS12ContextStr SSMPKCS12Context;

typedef struct SSMPKCS12BackupThreadArgStr {
    CERTCertificate **certs;
    PRIntn            numCerts;
} SSMPKCS12BackupThreadArg;

struct SSMPKCS12ContextStr {
    SSMResource  super;
    char        *m_password;
    PRFileDesc  *m_file;
    PRFileDesc  *m_digestFile;
    char        *m_tempFilePath;
    PRBool       m_error;
    PRBool       m_inputProcessed;
    PRBool       m_isExportContext; /* We're either importing or exporting. */
    CERTCertificate *m_cert;
    PRThread        *m_thread; /* A thread for doing backup when doing
                                * CMMF.
                                */
    SSMPKCS12BackupThreadArg *arg;
};

typedef struct SSMPKCS12CreateArgStr {
    PRBool isExportContext;
} SSMPKCS12CreateArg;

SSMStatus
SSMPKCS12Context_Create(void *arg, SSMControlConnection *ctrl,
                        SSMResource **res);

SSMStatus
SSMPKCS12Context_Init(SSMControlConnection *ctrl, SSMPKCS12Context *res,
                      SSMResourceType type, PRBool isExportContext);


SSMStatus
SSMPKCS12Context_Destroy(SSMResource *res, PRBool doFree);

SSMStatus
SSMPKCS12Context_FormSubmitHandler(SSMResource *res, HTTPRequest *req);

SSMStatus
SSMPKCS12Context_Print(SSMResource *res,
                       char        *fmt,
                       PRIntn       numParams,
                       char       **value,
                       char       **resultStr);

SSMStatus
SSMPKCS12Context_CreatePKCS12File(SSMPKCS12Context *cxt, 
                                  PRBool forceAuthenticate);

SSMStatus
SSMPKCS12Context_RestoreCertFromPKCS12File(SSMPKCS12Context *cxt);

SSMStatus
SSMPKCS12Context_ProcessPromptReply(SSMResource *res,
                                    char *reply);

PRBool
SSM_UCS2_ASCIIConversion(PRBool toUnicode, 
                         unsigned char *inBuf, unsigned int inBufLen,
                         unsigned char *outBuf, unsigned int maxOutBufLen,
                         unsigned int *outBufLen, PRBool swapBytes);

PRBool
SSM_UCS2_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
                           unsigned int inBufLen,unsigned char *outBuf,
                           unsigned int maxOutBufLen, unsigned int *outBufLen);

SSMStatus
SSMPKCS12Context_CreatePKCS12FileForMultipleCerts(SSMPKCS12Context *p12Cxt, 
                                                  PRBool forceAuthenticate,
                                                  CERTCertificate **certArr,
                                                  PRIntn numCerts);

void SSMPKCS12Context_BackupMultipleCertsThread(void *arg);
#endif /*P12RES_H_*/
