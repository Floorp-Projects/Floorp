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
#include "p12res.h"
#include "minihttp.h"
#include "pk11func.h"
#include "secmod.h"
#include "p12.h"
#include "p12plcy.h"
#include "secerr.h"
#include "newproto.h"
#include "messages.h"
#include "advisor.h"
#include "nlslayer.h"

#define SSMRESOURCE(object) (&object->super)

#define PKCS12_IN_BUFFER_SIZE 2048

static SSMStatus
ssmpkcs12context_createpkcs12file(SSMPKCS12Context *cx,
                                  PRBool forceAuthenticate,
                                  CERTCertificate **certArr,
                                  PRIntn numCerts);
#ifdef XP_MAC

char* SSM_ConvertMacPathToUnix(char *path)
{
	char *cursor;
	int len;
		
	len = PL_strlen(path);
	cursor = PR_Realloc(path, len+2);
	memmove(cursor+1, cursor, len+1);
	path = cursor;
	*cursor = '/';
	while ((cursor = PL_strchr(cursor, ':')) != NULL) {
		*cursor = '/';
		cursor++;
	}
	return path;
}

#endif

SECStatus
SSM_UnicodeConversion(SECItem *dest, SECItem *src,
                      PRBool toUnicode, PRBool swapBytes)
{
    unsigned int allocLen;

    if(!dest || !src) {
        return SECFailure;
    }

    allocLen = ((toUnicode) ? (src->len << 2) : src->len);
    if (allocLen == 0) {
        /* empty string: we need to pad it by 2 bytes */
        allocLen = 2;
    }

    dest->data = SSM_ZNEW_ARRAY(unsigned char, allocLen);
    if(!SSM_UCS2_ASCIIConversion(toUnicode, src->data, src->len,
                                 dest->data, allocLen, &dest->len,
                                 swapBytes)) {
        PR_Free(dest->data);
        dest->data = NULL;
        return SECFailure;
    }
    return SECSuccess;
}

SSMStatus
SSMPKCS12Context_Create(void *arg, SSMControlConnection *ctrl,
                        SSMResource **res)
{
    SSMPKCS12Context *cxt = NULL;
    SSMPKCS12CreateArg *createArg = (SSMPKCS12CreateArg*)arg;
    SSMStatus rv;
  
    cxt = SSM_ZNEW(SSMPKCS12Context);
    if (cxt == NULL) {
        return SSM_ERR_OUT_OF_MEMORY;
    }
    rv = SSMPKCS12Context_Init(ctrl,cxt,SSM_RESTYPE_PKCS12_CONTEXT ,
                               createArg->isExportContext);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    *res = SSMRESOURCE(cxt);
    return PR_SUCCESS;
 loser:
    if (cxt != NULL) {
        SSM_FreeResource(SSMRESOURCE(cxt));
    }
    *res = NULL;
    return rv;
}

SSMStatus
SSMPKCS12Context_Init(SSMControlConnection *ctrl, SSMPKCS12Context *res,
                      SSMResourceType type, PRBool isExportContext)
{
    res->m_isExportContext = isExportContext;
    res->m_password        = NULL;
    res->m_inputProcessed  = PR_FALSE;
    res->m_file            = NULL;
    res->m_digestFile      = NULL;
    res->m_error           = PR_FALSE;
    return SSMResource_Init(ctrl, SSMRESOURCE(res), type);
}

SSMStatus
SSMPKCS12Context_Destroy(SSMResource *res, PRBool doFree) 
{
    SSMPKCS12Context *cxt = (SSMPKCS12Context*)res;
    
    SSMResource_Destroy(res, PR_FALSE);
    if (cxt->m_password != NULL) {
        PR_Free(cxt->m_password);
        cxt->m_password = NULL;
    }
    if (cxt->m_cert != NULL) {
        CERT_DestroyCertificate(cxt->m_cert);
        cxt->m_cert = NULL;
    }
    if (doFree) {
        PR_Free(res);
    }
    return PR_SUCCESS;
}

static SSMStatus
SSMPKCS12Context_HandlePasswordRequest(SSMResource *res,
                                       HTTPRequest *req)    
{
    char *password, *confirmPassword;
    SSMPKCS12Context *p12Cxt = (SSMPKCS12Context*)res; 
    SSMStatus rv = SSM_FAILURE;
    
    /* Let's get the password out of the dialog. */
    if (res->m_buttonType != SSM_BUTTON_OK) {
        goto loser;
    }
    rv = SSM_HTTPParamValue(req, "passwd", &password);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_HTTPParamValue(req, "confirmPasswd", &confirmPassword);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (strcmp(password, confirmPassword) != 0) {
        /* Should re-prompt, but for now we fail. */
        rv = SSM_FAILURE;
        goto loser;
    }
    p12Cxt->m_password = PL_strdup(password);
    goto done;
 loser:
    p12Cxt->m_password = NULL;
 done:
    SSM_LockResource(res);
    SSM_NotifyResource(res);
    SSM_UnlockResource(res);
    SSM_HTTPDefaultCommandHandler(req);
    p12Cxt->m_inputProcessed = PR_TRUE;
    return rv;
}

SSMStatus
SSMPKCS12Context_FormSubmitHandler(SSMResource *res, HTTPRequest *req)
{
    char *formName;
    SSMStatus rv=SSM_FAILURE;
    
    rv = SSM_HTTPParamValue(req, "formName", &formName);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (!strcmp(formName, "cert_backup_form")) {
        rv = SSMPKCS12Context_HandlePasswordRequest(res, req);
    } else if (!strcmp(formName, "set_db_password")) {
      rv = SSM_SetDBPasswordHandler(req);
    } else {
        goto loser;
    }
    return rv;
 loser:
    SSM_HTTPDefaultCommandHandler(req);
    return SSM_FAILURE;
}

static void
ssmpkcs12context_writetoexportfile(void *arg, const char *buf, 
                                   unsigned long len)
{
    SSMPKCS12Context *p12Cxt = (SSMPKCS12Context*)arg;
    PRUint32 bytesWritten;

    if (p12Cxt == NULL) {
        return;
    }
    if (p12Cxt->m_file == NULL) {
        p12Cxt->m_error = PR_TRUE;
        return;
    }
    bytesWritten = PR_Write(p12Cxt->m_file, buf, len);
    if (bytesWritten != len) {
        p12Cxt->m_error = PR_TRUE;
    }
}

SSMStatus
SSMPKCS12Context_CreatePKCS12FileForMultipleCerts(SSMPKCS12Context *p12Cxt, 
                                                  PRBool forceAuthenticate,
                                                  CERTCertificate **certArr,
                                                  PRIntn numCerts)
{
    return ssmpkcs12context_createpkcs12file(p12Cxt, forceAuthenticate, 
                                             certArr, numCerts);
}

SSMStatus
SSMPKCS12Context_CreatePKCS12File(SSMPKCS12Context *cxt, 
                                  PRBool forceAuthenticate)
{
    return ssmpkcs12context_createpkcs12file(cxt, forceAuthenticate, 
                                             &cxt->m_cert, 1);
}

static SSMStatus
ssmpkcs12context_createpkcs12file(SSMPKCS12Context *cxt,
                                  PRBool forceAuthenticate,
                                  CERTCertificate **certArr,
                                  PRIntn numCerts)
{
    SEC_PKCS12ExportContext *p12ecx  = NULL;
    SEC_PKCS12SafeInfo      *keySafe = NULL, *certSafe = NULL;
    SECItem                  pwitem  = { siBuffer, NULL, 0 };
    PK11SlotInfo            *slot    = NULL;
    PK11SlotInfo            *slotToUse = NULL;
    SSMControlConnection    *ctrl; 
    SSMStatus                rv=SSM_FAILURE;
    int                      i;
    
    if (cxt == NULL || certArr == NULL || numCerts == 0) {
        return SSM_ERR_BAD_REQUEST;
    }
    /*
     * We're about to send the UI event requesting the password to use
     * when encrypting 
     */
    SSM_LockResource(&cxt->super);
    cxt->m_inputProcessed = PR_FALSE;
    rv = SSMControlConnection_SendUIEvent(SSMRESOURCE(cxt)->m_connection,
                                          "get", "cert_backup",
                                          SSMRESOURCE(cxt), 
                                          (numCerts > 1) ? "multipleCerts=1" :
                                                           NULL, 
                                          &SSMRESOURCE(cxt)->m_clientContext,
                                          PR_TRUE);
    if (rv != SSM_SUCCESS) {
        SSM_UnlockResource(SSMRESOURCE(cxt));
        goto loser;
    }
    /*
     * Wait until the form is submitted to proceed. We'll get notified.
     */
    SSM_WaitResource(SSMRESOURCE(cxt), PR_INTERVAL_NO_TIMEOUT);
    SSM_UnlockResource(SSMRESOURCE(cxt));
    if (cxt->m_password == NULL || 
        cxt->super.m_buttonType == SSM_BUTTON_CANCEL) {
        rv = SSM_ERR_NO_PASSWORD;
        goto loser;
    }
    /* Wait for the dialog box to go down so that when it disappears,
     * the window doesn't take away the password prompt.
     */
    PR_Sleep(PR_TicksPerSecond());

    ctrl = SSMRESOURCE(cxt)->m_connection;
    pwitem.data = (unsigned char *) cxt->m_password;
    pwitem.len  = strlen(cxt->m_password);
    PK11_FindObjectForCert(certArr[0], ctrl, &slot);
    if (slot == NULL) {
        rv = SSM_FAILURE;
        goto loser;
    }
    slotToUse = slot;
    if (forceAuthenticate && PK11_NeedLogin(slot)) {
        PK11_Logout(slot);
    }
    if (PK11_Authenticate(slot, PR_TRUE, ctrl) != SECSuccess) {
        rv = SSM_ERR_BAD_DB_PASSWORD;
        goto loser;
    }
    p12ecx = SEC_PKCS12CreateExportContext(NULL, NULL, slot, ctrl);

    if (p12ecx == NULL) {
        rv = SSM_FAILURE;
        goto loser;
    }
    if (SEC_PKCS12AddPasswordIntegrity(p12ecx, &pwitem, SEC_OID_SHA1) 
        != SECSuccess) {
        rv = SSM_ERR_BAD_PASSWORD;
        goto loser;
    }
    for (i=0; i <numCerts; i++) {
        PK11_FindObjectForCert(certArr[i], ctrl, &slot);
        if (slot != slotToUse) {
            continue;
        }
        keySafe = SEC_PKCS12CreateUnencryptedSafe(p12ecx);
        if (!SEC_PKCS12IsEncryptionAllowed() || PK11_IsFIPS()) {
            certSafe = keySafe;
        } else {
            certSafe = SEC_PKCS12CreatePasswordPrivSafe(p12ecx, &pwitem,
                          SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
        }
        if (certSafe == NULL || keySafe == NULL) {
            rv = SSM_FAILURE;
            goto loser;
        }
        if (SEC_PKCS12AddCertAndKey(p12ecx, certSafe, NULL, certArr[i],
                                    SSMRESOURCE(cxt)->m_connection->m_certdb,
                                    keySafe, NULL, PR_TRUE, &pwitem,
                      SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC)
            != SECSuccess) {
            rv = SSM_FAILURE;
            goto loser;
        }
    }
    /* Done with the password, free it */
    PR_Free(cxt->m_password);
    cxt->m_password = NULL;
    rv = SSM_RequestFilePathFromUser(SSMRESOURCE(cxt),
                                     "pkcs12_export_file_prompt",
                                     "*.p12",
                                     PR_FALSE);
    if (rv != SSM_SUCCESS || cxt->super.m_fileName == NULL) {
        rv = SSM_ERR_BAD_FILENAME;
        goto loser;
    }
#ifdef XP_MAC
	cxt->super.m_fileName = SSM_ConvertMacPathToUnix(cxt->super.m_fileName);
#endif    
    cxt->m_file = PR_Open (cxt->super.m_fileName,
                           PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                           0600);
    if (cxt->m_file == NULL) {
        rv = SSM_ERR_BAD_FILENAME;
        goto loser;
    }
    if (SEC_PKCS12Encode(p12ecx, ssmpkcs12context_writetoexportfile, cxt)
        != SECSuccess) {
        rv = SSM_FAILURE;
        goto loser;
    }
    PR_Close(cxt->m_file);
    if (slotToUse) {
        PK11_FreeSlot(slotToUse);
    }
    SEC_PKCS12DestroyExportContext(p12ecx);
    return SSM_SUCCESS;
 loser:
    if (p12ecx != NULL) {
        SEC_PKCS12DestroyExportContext(p12ecx);
    }
    if (slot && cxt->m_cert && (slot != cxt->m_cert->slot)) {
        PK11_FreeSlot(slot);
    }
    PR_FREEIF(cxt->m_password);
    cxt->m_password = NULL;
    return rv;
}

void ssm_switch_endian(unsigned char *buf, unsigned int len)
{
    unsigned int i;
    unsigned char tmp;

    for (i=0; i<len; i+=2) {
        tmp      = buf[i];
        buf[i]   = buf[i+1];
        buf[i+1] = tmp;
    }
}

/* This function converts ASCII strings to UCS2 strings in Network Byte Order.
** The "swapBytes" argument is ignored.  
** The PKCS#12 code only makes it true on Little Endian systems, 
** where it was intended to force the output into NBO.
*/
PRBool 
SSM_UCS2_ASCIIConversion(PRBool toUnicode, 
                         unsigned char *inBuf,
                         unsigned int inBufLen,
                         unsigned char *outBuf,
                         unsigned int maxOutBufLen, 
                         unsigned int *outBufLen, 
                         PRBool swapBytes)
{
    if (!inBuf || !outBuf || !outBufLen) {
        return PR_FALSE;
    }

    if (toUnicode) {
    	PRBool rv;
#ifdef DEBUG
	unsigned int outLen;
	unsigned int i;
	fprintf(stderr,"\n---ssm_ConvertAsciiToUCS2---\nInput: inBuf= ");
	for (i = 0; i < inBufLen; i++) {
	    fprintf(stderr, "%c", inBuf[i]);
	}
	fprintf(stderr,"\ninBufLen=%d\n", inBufLen);
#endif
	rv = nlsASCIIToUnicode(inBuf, inBufLen, 
				    outBuf, maxOutBufLen, outBufLen);
    if (swapBytes) {
        ssm_switch_endian(outBuf, *outBufLen);
    }
#ifdef DEBUG
	outLen = *outBufLen;
	fprintf(stderr,"output: outBuf= ");
	for(i = 0; i < outLen; i++) {
	    fprintf(stderr, "%c ", outBuf[i]);
	}
	fprintf(stderr,"\noutBuf= ");
	for(i = 0; i < outLen; i++) {
	    fprintf(stderr,"%2x ", outBuf[i]);
	}
	fprintf(stderr,"\noutLen = %d\n", outLen);
#endif /* DEBUG */

	return rv;
    }
    PR_ASSERT(PR_FALSE); /* not supported yet */
    return PR_FALSE;
}

PRBool 
SSM_UCS2_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
                           unsigned int inBufLen,unsigned char *outBuf,
                           unsigned int maxOutBufLen, unsigned int *outBufLen)
{
	PRBool retval;
#ifdef DEBUG
	unsigned int i;
#endif
    char *newbuf=NULL;

    if(!inBuf || !outBuf || !outBufLen) {
        return PR_FALSE;
    }

    *outBufLen = 0;

#ifdef DEBUG
    fprintf(stderr,"---UCS2_UTF8Conversion (%s) ---\nInput: \n",
		(toUnicode?"to UCS2":"to UTF8"));
	for(i=0; i< inBufLen; i++) {
		fprintf(stderr,"%c", (char) inBuf[i]);
	}
	fprintf(stderr,"\n");
   	for(i=0; i< inBufLen; i++) {
		fprintf(stderr,"%2x ", (char) inBuf[i]);
	}
	fprintf(stderr,"\n");
#endif
    if(toUnicode) {
        retval = nlsUTF8ToUnicode(inBuf, inBufLen, outBuf, maxOutBufLen, 
                                 outBufLen);
#if IS_LITTLE_ENDIAN
        /* Our converter gives us back the buffer in host order,
         * so let's convert to network byte order
         */
        ssm_switch_endian(outBuf, *outBufLen);
#endif
    } else {
#if IS_LITTLE_ENDIAN
        /* NSS is the only place where this function gets called.  It gives
         * us the bytes in Network Byte Order, but the conversion functions
         * expect the bytes in host order.  So we'll switch the bytes around
         * before passing them to the translator.
         */
        
        /* The buffer that comes won't necessarily have the trailing ending
         * zero bytes, which our converter assumes. So we'll add them
         * here.
         */
        /* Do a check to make sure it is in Network Byte Order first. */
        if (inBuf[0] == 0) {
            newbuf = SSM_NEW_ARRAY(char, inBufLen+2);
            memcpy(newbuf, inBuf, inBufLen);
            newbuf[inBufLen] = newbuf[inBufLen+1] = 0;
            inBuf = newbuf;
            ssm_switch_endian(inBuf, inBufLen);
        }
#endif
    	retval = nlsUnicodeToUTF8(inBuf, inBufLen, outBuf, maxOutBufLen, 
                                 outBufLen);
	}
#ifdef DEBUG
    fprintf(stderr,"Output: \n");
   	for(i=0; i< *outBufLen; i++) {
		fprintf(stderr,"%c", (char) outBuf[i]);
	}
	fprintf(stderr,"\n");
   	for(i=0; i< *outBufLen; i++) {
		fprintf(stderr,"%2x ", (char) outBuf[i]);
	}
	fprintf(stderr,"\n\n");
#endif
    PR_FREEIF(newbuf);
	return retval;
}

static SECStatus
ssmpkcs12context_digestopen(void *arg, PRBool readData)
{
    char *tmpFileName=NULL;
    char *filePathSep;
    SSMPKCS12Context *cxt = (SSMPKCS12Context *)arg;

#if defined(XP_UNIX)
    filePathSep = "/";
#elif defined(WIN32) || defined(XP_OS2)
    filePathSep = "\\";
#elif defined(XP_MAC)
	filePathSep = "";
#else
#error Tell me what the file path separator is of this platform.
#endif

    tmpFileName = PR_smprintf("%s%s%s", 
                              SSMRESOURCE(cxt)->m_connection->m_dirRoot,
                              filePathSep,
                              ".nsm_p12_tmp");
    if (tmpFileName == NULL) {
        return SECFailure;
    }
#ifdef XP_MAC
	tmpFileName = SSM_ConvertMacPathToUnix(tmpFileName);
#endif    
    if (readData) {
        cxt->m_digestFile = PR_Open(tmpFileName,
                                    PR_RDONLY, 0400);
    } else {
        cxt->m_digestFile = PR_Open(tmpFileName,
                                    PR_CREATE_FILE | PR_RDWR | PR_TRUNCATE,
                                    0600);
    }
    cxt->m_tempFilePath = tmpFileName;
    if (cxt->m_digestFile == NULL) {
        cxt->m_error = PR_TRUE;
        return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
ssmpkcs12context_digestclose(void *arg, PRBool removeFile)
{
    SSMPKCS12Context *cxt = (SSMPKCS12Context*)arg;
    
    if (cxt == NULL || cxt->m_digestFile == NULL) {
        return SECFailure;
    }
    PR_Close(cxt->m_digestFile);
    cxt->m_digestFile = NULL;
    if (removeFile) {
        PR_Delete(cxt->m_tempFilePath);
        PR_Free(cxt->m_tempFilePath);
        cxt->m_tempFilePath = NULL;
    }
    return SECSuccess;
}

static int
ssmpkcs12context_digestread(void *arg, unsigned char *buf, unsigned long len)
{
    SSMPKCS12Context *cxt = (SSMPKCS12Context*)arg;
    
    if (cxt == NULL || cxt->m_digestFile == NULL) {
        return -1;
    }
    if (buf == NULL || len == 0) {
        return -1;
    }
    return PR_Read(cxt->m_digestFile, buf, len);
}

static int
ssmpkcs12context_digestwrite(void *arg, unsigned char *buf, unsigned long len)
{
    SSMPKCS12Context *cxt = (SSMPKCS12Context *)arg;

    if (cxt == NULL || cxt->m_digestFile == NULL) {
        return -1;
    }
    if (buf == NULL || len == 0) {
        return -1;
    }
    return PR_Write(cxt->m_digestFile, buf, len);
}

SECItem*
SSM_NicknameCollisionCallback(SECItem *old_nick, PRBool *cancel,
                              void *wincx)
{
    /* We don't handle this yet */
    *cancel = PR_TRUE;
    return NULL;
}

static PK11SlotInfo*
SSMPKCS12Context_ChooseSlotForImport(SSMPKCS12Context *cxt,
                                     PK11SlotList     *slotList)
{
    char mech[20];
    SSMStatus rv;

    PR_snprintf(mech, 20, "mech=%d&task=import&unused=unused", CKM_RSA_PKCS);
    SSM_LockUIEvent(&cxt->super);
    rv = SSMControlConnection_SendUIEvent(cxt->super.m_connection, 
                                          "get", 
                                          "select_token",
                                          &cxt->super,
                                          mech, 
                                          &SSMRESOURCE(cxt)->m_clientContext,
                                          PR_TRUE);
    if (rv != SSM_SUCCESS) {
        SSM_UnlockResource(&cxt->super);
        return NULL;
    }
    SSM_WaitUIEvent(&cxt->super, PR_INTERVAL_NO_TIMEOUT);
    /* Wait so damn window goes away without swallowing up
     * the password prompt that will come up next.
     */
    PR_Sleep(PR_TicksPerSecond());
    return (PK11SlotInfo*)cxt->super.m_uiData;
}

static PK11SlotInfo*
SSMPKCS12Context_GetSlotForImport(SSMPKCS12Context *cxt)
{
    PK11SlotList *slotList;
    PK11SlotInfo *slot = NULL;

    slotList = PK11_GetAllTokens(CKM_RSA_PKCS, PR_TRUE, PR_TRUE,
                                 cxt->super.m_connection);
    if (slotList == NULL || slotList->head == NULL) {
        /* Couldn't find a slot, let's try the internal slot 
         * and see what happens
         */
        slot = PK11_GetInternalKeySlot();
    } else if (slotList->head->next == NULL) {
        /*
         * Only one slot, return it.
         */
        slot = PK11_ReferenceSlot(slotList->head->slot);
    } else {
        slot = SSMPKCS12Context_ChooseSlotForImport(cxt, slotList);
    }
    if (slotList)
        PK11_FreeSlotList(slotList);
    return slot;
}

SSMStatus
SSMPKCS12Context_RestoreCertFromPKCS12File(SSMPKCS12Context *cxt)
{
    SSMStatus rv;
    SECItem   passwdReq;
    char     *prompt=NULL;
    PK11SlotInfo *slot=NULL;
    SEC_PKCS12DecoderContext *p12dcx=NULL;
    PRBool swapUnicode = PR_FALSE;
    unsigned char *buf=NULL;
    SECItem pwItem = { siBuffer, NULL, 0 }, uniPwItem = { siBuffer, NULL, 0 };
    SECStatus srv;
    CERTCertList *certList=NULL;
    CERTCertListNode *node=NULL;
    PromptRequest request;
    
    if (cxt == NULL || cxt->m_isExportContext) {
        return SSM_FAILURE;
    }
#ifdef IS_LITTLE_ENDIAN
    swapUnicode = PR_TRUE;
#endif
    
    rv = SSM_RequestFilePathFromUser(SSMRESOURCE(cxt),
                                     "pkcs12_import_file_prompt",
                                     "*.p12",
                                     PR_TRUE);
    if (rv != SSM_SUCCESS || cxt->super.m_fileName == NULL) {
        rv = SSM_ERR_BAD_FILENAME;
        goto loser;
    }
    prompt = SSM_GetCharFromKey("pkcs12_request_password_prompt", 
                                "ISO-8859-1");
    if (prompt == NULL) {
        rv = SSM_FAILURE;
        goto loser;
    }
    request.resID = SSMRESOURCE(cxt)->m_id;
    request.prompt = prompt;
    request.clientContext = SSMRESOURCE(cxt)->m_clientContext;
    if (CMT_EncodeMessage(PromptRequestTemplate, (CMTItem*)&passwdReq, &request) != CMTSuccess) {
        rv = SSM_FAILURE;
        goto loser;
    }
    passwdReq.type = (SECItemType) (SSM_EVENT_MESSAGE | SSM_PROMPT_EVENT);
    SSM_LockResource(SSMRESOURCE(cxt));
    cxt->m_password = NULL;
    rv = SSM_SendQMessage(SSMRESOURCE(cxt)->m_connection->m_controlOutQ,
                          20,
                          passwdReq.type,
                          passwdReq.len,
                          (char*)passwdReq.data,
                          PR_TRUE);
    SSM_WaitResource(SSMRESOURCE(cxt), SSM_PASSWORD_WAIT_TIME);
    SSM_UnlockResource(SSMRESOURCE(cxt));
    if (cxt->m_password == NULL) {
        rv = SSM_ERR_NO_PASSWORD;
        goto loser;
    }
#ifdef XP_MAC

	/*NSPR wants the path to be a UNIX style path.  So let's convert it here for MAC.*/
	SSMRESOURCE(cxt)->m_fileName = SSM_ConvertMacPathToUnix(SSMRESOURCE(cxt)->m_fileName);
	
#endif  
	cxt->m_file = PR_Open(SSMRESOURCE(cxt)->m_fileName,
                          PR_RDONLY, 0400);                        
    if (cxt->m_file == NULL) {
        rv = SSM_ERR_BAD_FILENAME;
        goto loser;
    }
    slot = SSMPKCS12Context_GetSlotForImport(cxt);
    if (slot == NULL) {
        goto loser;
    }

    if (PK11_NeedLogin(slot)) {
        /* we should log out only if the slot needs login */
        PK11_Logout(slot);
    }

    /* User has not initialize DB, ask for a password. */
    if (PK11_NeedUserInit(slot)) {
      rv = SSM_SetUserPassword(slot, SSMRESOURCE(cxt));
      if (rv != SSM_SUCCESS) {
          rv = SSM_ERR_NEED_USER_INIT_DB;
          goto loser;
      }
    }

    if (PK11_Authenticate(slot, PR_FALSE, SSMRESOURCE(cxt)->m_connection) 
        != SECSuccess) {
        rv = SSM_ERR_BAD_DB_PASSWORD;
        goto loser;
    }
    pwItem.data = (unsigned char *) cxt->m_password;
    pwItem.len = strlen(cxt->m_password);
    if (SSM_UnicodeConversion(&uniPwItem, &pwItem, PR_TRUE,
                              swapUnicode) != SECSuccess) {
        rv = SSM_ERR_BAD_PASSWORD;
        goto loser;
    }
    p12dcx = SEC_PKCS12DecoderStart(&uniPwItem, slot,
                                    SSMRESOURCE(cxt)->m_connection,
                                    ssmpkcs12context_digestopen,
                                    ssmpkcs12context_digestclose,
                                    ssmpkcs12context_digestread,
                                    ssmpkcs12context_digestwrite,
                                    cxt);
    if (p12dcx == NULL) {
        rv = SSM_FAILURE;
        goto loser;
    }
    buf = SSM_NEW_ARRAY(unsigned char, PKCS12_IN_BUFFER_SIZE);
    if (buf == NULL) {
        rv = SSM_ERR_OUT_OF_MEMORY;
        goto loser;
    }

    while (PR_TRUE) {
        int readLen = PR_Read(cxt->m_file, buf, PKCS12_IN_BUFFER_SIZE);
        if (readLen < 0) {
            rv = SSM_FAILURE;
            goto loser;
        }
        srv = SEC_PKCS12DecoderUpdate(p12dcx, buf, readLen);
        if (srv != SECSuccess || readLen != PKCS12_IN_BUFFER_SIZE) {
            break;
        }
    }
    if (srv != SECSuccess) {
        rv = SSM_ERR_CANNOT_DECODE;
        goto loser;
    }
    if (SEC_PKCS12DecoderVerify(p12dcx) != SECSuccess) {
        rv = SSM_FAILURE;
        goto loser;
    }
    if (SEC_PKCS12DecoderValidateBags(p12dcx, SSM_NicknameCollisionCallback)
        != SECSuccess) {
        if (PORT_GetError() == SEC_ERROR_PKCS12_DUPLICATE_DATA) {
            rv = SSM_PKCS12_CERT_ALREADY_EXISTS;
        } else {
            rv = SSM_FAILURE;
        }
        goto loser;
    }
    if (SEC_PKCS12DecoderImportBags(p12dcx) != SECSuccess) {
        rv = SSM_FAILURE;
        goto loser;
    }
    PR_Close(cxt->m_file);
    cxt->m_file = NULL;
    PR_Free(prompt);
    PK11_FreeSlot(slot);

    certList = SEC_PKCS12DecoderGetCerts(p12dcx);
    if (certList != NULL) {
        for (node = CERT_LIST_HEAD(certList); !CERT_LIST_END(node, certList);
             node = CERT_LIST_NEXT(node)) {
            if ((node->cert->trust) && 
                (node->cert->trust->emailFlags & CERTDB_USER) &&
                CERT_VerifyCertNow(cxt->super.m_connection->m_certdb,
                                   node->cert, PR_TRUE, certUsageEmailSigner,
                                   cxt) == SSM_SUCCESS) {
                rv = SSM_UseAsDefaultEmailIfNoneSet(cxt->super.m_connection,
                                                    node->cert, PR_FALSE);
                if (rv == SSM_SUCCESS) {
                    /* We just made this cert the default new cert */
                    rv = SSM_ERR_NEW_DEF_MAIL_CERT;
                    break;
                }
            }
        }
        CERT_DestroyCertList(certList);
        certList = NULL;
    }

    SEC_PKCS12DecoderFinish(p12dcx);
    return (rv == SSM_ERR_NEW_DEF_MAIL_CERT) ? rv : SSM_SUCCESS;
loser:
    if (cxt->m_file != NULL) {
        PR_Close(cxt->m_file);
        cxt->m_file = NULL;
    }
    if (prompt != NULL) {
        PR_Free(prompt);
    }
    if (slot != NULL) {
        PK11_FreeSlot(slot);
    }
    if (p12dcx != NULL) {
        SEC_PKCS12DecoderFinish(p12dcx);
    }
    if (buf != NULL) {
        PR_Free(buf);
    }
    cxt->m_error = PR_TRUE;
    return rv;
}

SSMStatus
SSMPKCS12Context_ProcessPromptReply(SSMResource *res,
                                    char *reply)
{
    SSMPKCS12Context *cxt = (SSMPKCS12Context*)res;

    if (!SSM_IsAKindOf(res, SSM_RESTYPE_PKCS12_CONTEXT)) {
        return PR_FAILURE;
    }
    cxt->m_password = reply?PL_strdup(reply):NULL;
    SSM_LockResource(res);
    SSM_NotifyResource(res);
    SSM_UnlockResource(res);
    return PR_SUCCESS;
}

SSMStatus
SSMPKCS12Context_Print(SSMResource *res,
                       char        *fmt,
                       PRIntn       numParams,
                       char       **value,
                       char       **resultStr)
{
    char mechStr[48];
    SSMStatus rv = SSM_FAILURE;

    PR_ASSERT(resultStr != NULL);
    if (resultStr == NULL) {
        rv = SSM_FAILURE;
        goto done;
    }
    if (numParams) { 
        rv = SSMResource_Print(res, fmt, numParams, value, resultStr);
        goto done;
    }
    PR_snprintf(mechStr, 48, "%d", CKM_RSA_PKCS);
    *resultStr = PR_smprintf(fmt, res->m_id, mechStr, "");

    rv = (*resultStr == NULL) ? SSM_FAILURE : SSM_SUCCESS;
 done: 
    return rv;
}

void SSMPKCS12Context_BackupMultipleCertsThread(void *arg)
{
    SSMPKCS12Context *p12Cxt = (SSMPKCS12Context*)arg;
    SSMStatus rv;
    SSMControlConnection *connection = p12Cxt->super.m_connection;
    PRIntn i;

    SSM_RegisterThread("PKCS12", NULL);
    SSM_DEBUG("About to backup some certs.\n");

    rv = SSMControlConnection_SendUIEvent(connection, 
                                          "get", "backup_new_cert",
                                          &p12Cxt->super, NULL, 
                                          &p12Cxt->super.m_clientContext,
                                          PR_TRUE);
    PR_ASSERT(SSMRESOURCE(p12Cxt)->m_buttonType == SSM_BUTTON_NONE);
    if (rv == SSM_SUCCESS) {
        while (SSMRESOURCE(p12Cxt)->m_buttonType == SSM_BUTTON_NONE) {
            SSM_WaitForOKCancelEvent(SSMRESOURCE(p12Cxt), 
                                     PR_INTERVAL_NO_TIMEOUT);
        } 
    }
    /*
     * Eventhough we tell Nova to use a context it provided to us, 
     * it still tries to use the top-most window to bring up the 
     * next dialog.  Meaning I still have to insert this freakin' 
     * sleep.
     *
     * XXX -javi
     */
    PR_Sleep(PR_TicksPerSecond());
    /*
     * Create a single P12 file containing all of the certs that 
     * were just issued.
     */
    SSMPKCS12Context_CreatePKCS12FileForMultipleCerts(p12Cxt, PR_FALSE,
                                                      p12Cxt->arg->certs,
                                                      p12Cxt->arg->numCerts);

#if 0
    for (i=0; i<p12Cxt->arg->numCerts;i++) {
        p12Cxt->m_cert = p12Cxt->arg->certs[i];
        SSMPKCS12Context_CreatePKCS12File(p12Cxt, PR_FALSE);
        /*
         * ARGH!!  If we do more than one, then Communicator crashes 
         * because it tries to use a window that no longer exists as
         * the base for the next window.
         */
        PR_Sleep(PR_TicksPerSecond());
    }
#endif
    PR_Free(p12Cxt->arg->certs);
    PR_Free(p12Cxt->arg);
    p12Cxt->arg = NULL;
    p12Cxt->m_thread = NULL;
    SSM_FreeResource(&p12Cxt->super);
    SSM_DEBUG("Done backing up certs.\n");
}

SSMStatus 
SSM_WarnPKCS12Incompatibility(SSMTextGenContext *cx)
{
    SSMStatus rv;
    char *value;
    
    rv = SSM_HTTPParamValue(cx->m_request, "multipleCerts", &value);
    PR_FREEIF(cx->m_result);
    cx->m_result = NULL;
    if (rv == SSM_SUCCESS) {
        rv = SSM_FindUTF8StringInBundles(cx, "pkcs12_incompatible_warn",
                                         &cx->m_result);
        if (rv != SSM_SUCCESS) {
            cx->m_result = PL_strdup("");
        }
    } else {
        cx->m_result = PL_strdup("");
    }
    return SSM_SUCCESS;
}

