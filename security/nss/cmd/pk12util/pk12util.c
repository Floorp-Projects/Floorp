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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1994-2000 Netscape Communications Corporation.	 All
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

#include "nspr.h"
#include "secutil.h"
#include "pk11func.h"
#include "pkcs12.h"
#include "p12plcy.h"
#include "pk12util.h"
#include "nss.h"

#define PKCS12_IN_BUFFER_SIZE	200

static char *progName;
PRBool pk12_debugging = PR_FALSE;

PRIntn pk12uErrno = 0;

static void
Usage(char *progName)
{
#define FPS PR_fprintf(PR_STDERR,
    FPS "Usage:	 %s -i importfile [-d certdir] [-P dbprefix] [-h tokenname]\n",
				 progName);
    FPS "\t\t [-k slotpwfile | -K slotpw] [-w p12filepwfile | -W p12filepw]\n");
    FPS "Usage:	 %s -o exportfile -n certname [-d certdir] [-P dbprefix]\n", progName);
    FPS "\t\t [-k slotpwfile | -K slotpw] [-w p12filepwfile | -W p12filepw]\n");
    exit(PK12UERR_USAGE);
}

static PRBool
p12u_OpenExportFile(p12uContext *p12cxt, PRBool fileRead)
{
    if(!p12cxt || !p12cxt->filename) {
	return PR_FALSE;
    }

    if(fileRead) {
	p12cxt->file = PR_Open(p12cxt->filename,
				 PR_RDONLY, 0400);
    } else {
	p12cxt->file = PR_Open(p12cxt->filename,
				 PR_CREATE_FILE | PR_RDWR | PR_TRUNCATE,
				 0600);
    }

    if(!p12cxt->file) {
	p12cxt->error = PR_TRUE;
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	return PR_FALSE;
    }

    return PR_TRUE;
}

static void
p12u_DestroyExportFileInfo(p12uContext **exp_ptr, PRBool removeFile)
{
    if(!exp_ptr || !(*exp_ptr)) {
	return;
    }

    if((*exp_ptr)->file != NULL) {
	PR_Close((*exp_ptr)->file);
    }

    if((*exp_ptr)->data != NULL) {
	SECITEM_ZfreeItem((*exp_ptr)->data, PR_FALSE);
    }

    if((*exp_ptr)->filename != NULL) {
	if(removeFile) {
	    PR_Delete((*exp_ptr)->filename);
	}
	PR_Free((*exp_ptr)->filename);
    }

    PR_Free(*exp_ptr);
    *exp_ptr = NULL;
}

static p12uContext *
p12u_InitFile(PRBool fileImport, char *filename)
{
    p12uContext *p12cxt;
    PRBool fileExist;

    if(fileImport)
	fileExist = PR_TRUE;
    else
	fileExist = PR_FALSE;

    p12cxt = (p12uContext *)PORT_ZAlloc(sizeof(p12uContext));
    if(!p12cxt) {
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	return NULL;
    }

    p12cxt->error = PR_FALSE;
    p12cxt->errorValue = 0;
    p12cxt->filename = strdup(filename);

    if(!p12u_OpenExportFile(p12cxt, fileImport)) {
	PR_SetError(p12cxt->errorValue, 0);
	p12u_DestroyExportFileInfo(&p12cxt, PR_FALSE);
	return NULL;
    }

    return p12cxt;
}

#define TEMPFILE "Pk12uTemp"

static p12uContext *
p12u_CreateTemporaryDigestFile(void)
{
    p12uContext *p12cxt;
#if defined(_WIN32) || defined(_WINDOWS) || defined(XP_OS2)
    char *tmpdir,*filename,*last;
    int len;
#endif

    p12cxt = (p12uContext *)PR_CALLOC(sizeof(p12uContext));
    if(!p12cxt) {
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	return NULL;
    }
#if defined(_WIN32) || defined(_WINDOWS) || defined(XP_OS2)
    tmpdir = getenv("TMP");
    if (!tmpdir)
	tmpdir = getenv("TMPDIR");
    if (!tmpdir)
	tmpdir = getenv("TEMP");
    if (!tmpdir)
	tmpdir = ".";
    len = strlen(tmpdir);
    filename = PORT_Alloc(len+sizeof(TEMPFILE)+2);
    if (filename == NULL) {
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	p12u_DestroyExportFileInfo(&p12cxt, PR_FALSE);
	return NULL;
    }
    strcpy(filename, tmpdir);
    len = strlen(filename);
    last = tmpdir[len - 1];
    if ((last != '/') && (last != '\\')) {
	strcat(filename,"\\");
    }
    strcat(filename,TEMPFILE);
    p12cxt->filename = filename;
#else
    p12cxt->filename = strdup("/tmp/"TEMPFILE); 
#endif

    if (!p12cxt->filename) {
	PR_SetError(SEC_ERROR_NO_MEMORY, 0);
	p12u_DestroyExportFileInfo(&p12cxt, PR_FALSE);
	return NULL;
    }

    return p12cxt;
}

static SECStatus
p12u_DigestOpen(void *arg, PRBool readData)
{
    p12uContext *p12cxt = arg;


    if(!p12cxt || p12cxt->error || !p12cxt->filename) {
	return SECFailure;
    }

    if(readData) {
	p12cxt->file = PR_Open(p12cxt->filename, PR_RDONLY, 0400);
    } else {
	p12cxt->file = PR_Open(p12cxt->filename,
				   PR_CREATE_FILE | PR_RDWR | PR_TRUNCATE,
				   0600);
    }

    if(p12cxt->file == NULL) {
	p12cxt->error = PR_TRUE;
	return SECFailure;
    }

    return SECSuccess;
}

static SECStatus
p12u_DigestClose(void *arg, PRBool removeFile)
{
    p12uContext *p12cxt = arg;
    if(!p12cxt || p12cxt->error ||
       !p12cxt->filename || !p12cxt->file) {
	return SECFailure;
    }

    PR_Close(p12cxt->file);
    p12cxt->file = NULL;

    if(removeFile) {
	PR_Delete(p12cxt->filename);
	PR_Free(p12cxt->filename);
	p12cxt->filename = NULL;
    }

    return SECSuccess;
}

static int
p12u_DigestRead(void *arg, unsigned char *buf, unsigned long len)
{
    p12uContext *p12cxt = arg;

    if(!p12cxt || p12cxt->error ||
       !p12cxt->filename || !p12cxt->file) {
	return -1;
    }

    if(!buf || len == 0) {
	return -1;
    }

    return PR_Read(p12cxt->file, buf, len);
}

static int
p12u_DigestWrite(void *arg, unsigned char *buf, unsigned long len)
{
    p12uContext *p12cxt = arg;

    if(!p12cxt || p12cxt->error ||
       !p12cxt->filename || !p12cxt->file) {
	return -1;
    }

    if(!buf || len == 0) {
	return -1;
    }

    return PR_Write(p12cxt->file, buf, len);
}

SECItem *
P12U_NicknameCollisionCallback(SECItem *old_nick, PRBool *cancel, void *wincx)
{
    char *nick = NULL;
    SECItem *ret_nick = NULL;

    if(cancel == NULL) {
      pk12uErrno = PK12UERR_USER_CANCELLED;
      return NULL;
    }

    if (!old_nick)
      fprintf(stdout, "pk12util: no nickname for cert...not handled\n");

    /* XXX not handled yet  */
    *cancel = PR_TRUE;
    return NULL;

#if 0
    nick = strdup( DEFAULT_CERT_NICKNAME );

    if(old_nick && !PORT_Strcmp((char *)old_nick->data, nick)) {
	PORT_Free(nick);
	return NULL;
    }

    ret_nick = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
    if(ret_nick == NULL) {
	PORT_Free(nick);
	return NULL;
    }

    ret_nick->data = (unsigned char *)nick;
    ret_nick->len = PORT_Strlen(nick);

    return ret_nick;
#endif
}

static SECStatus
p12u_SwapUnicodeBytes(SECItem *uniItem)
{
    unsigned int i;
    unsigned char a;
    if((uniItem == NULL) || (uniItem->len % 2)) {
	return SECFailure;
    }
    for(i = 0; i < uniItem->len; i += 2) {
	a = uniItem->data[i];
	uniItem->data[i] = uniItem->data[i+1];
	uniItem->data[i+1] = a;
    }
    return SECSuccess;
}

static PRBool
p12u_ucs2_ascii_conversion_function(PRBool	   toUnicode,
				    unsigned char *inBuf,
				    unsigned int   inBufLen,
				    unsigned char *outBuf,
				    unsigned int   maxOutBufLen,
				    unsigned int  *outBufLen,
				    PRBool	   swapBytes)
{
    SECItem it = { 0 };
    SECItem *dup = NULL;
    PRBool ret;
    /* If converting Unicode to ASCII, swap bytes before conversion
     * as neccessary.
     */
    if (pk12_debugging) {
	int i;
	printf("Converted from:\n");
	for (i=0; i<inBufLen; i++) {
	    printf("%2x ", inBuf[i]);
	    /*if (i%60 == 0) printf("\n");*/
	}
	printf("\n");
    }
    it.data = inBuf;
    it.len = inBufLen;
    dup = SECITEM_DupItem(&it);
    if (!toUnicode && swapBytes) {
	if (p12u_SwapUnicodeBytes(dup) != SECSuccess) {
	    SECITEM_ZfreeItem(dup, PR_TRUE);
	    return PR_FALSE;
	}
    }
    /* Perform the conversion. */
    ret = PORT_UCS2_UTF8Conversion(toUnicode, dup->data, dup->len,
                                   outBuf, maxOutBufLen, outBufLen);
    if (dup)
	SECITEM_ZfreeItem(dup, PR_TRUE);
    /* If converting ASCII to Unicode, swap bytes before returning
     * as neccessary.
     */
#if 0
    if (toUnicode && swapBytes) {
	it.data = outBuf;
	it.len = *outBufLen;
	dup = SECITEM_DupItem(&it);
	if (p12u_SwapUnicodeBytes(dup) != SECSuccess) {
	    SECITEM_ZfreeItem(dup, PR_TRUE);
	    return PR_FALSE;
	}
	memcpy(outBuf, dup->data, *outBufLen);
	SECITEM_ZfreeItem(dup, PR_TRUE);
    }
#endif
    if (pk12_debugging) {
	int i;
	printf("Converted to:\n");
	for (i=0; i<*outBufLen; i++) {
	    printf("%2x ", outBuf[i]);
	    /*if (i%60 == 0) printf("\n");*/
	}
	printf("\n");
    }
    return ret;
}

SECStatus
P12U_UnicodeConversion(PRArenaPool *arena, SECItem *dest, SECItem *src,
		       PRBool toUnicode, PRBool swapBytes)
{
    unsigned int allocLen;
    if(!dest || !src) {
	return SECFailure;
    }
    allocLen = ((toUnicode) ? (src->len << 2) : src->len);
    if(arena) {
	dest->data = PORT_ArenaZAlloc(arena, allocLen);
    } else {
	dest->data = PORT_ZAlloc(allocLen);
    }
    if(PORT_UCS2_ASCIIConversion(toUnicode, src->data, src->len,
				 dest->data, allocLen, &dest->len,
				 swapBytes) == PR_FALSE) {
	if(!arena) {
	    PORT_Free(dest->data);
	}
	dest->data = NULL;
	return SECFailure;
    }
    return SECSuccess;
}

/*
 *
 */
SECItem *
P12U_GetP12FilePassword(PRBool confirmPw, secuPWData *p12FilePw)
{
    char *p0 = NULL, *p1 = NULL;
    SECItem *pwItem = NULL;

    if (p12FilePw == NULL || p12FilePw->source == PW_NONE) {
	for (;;) {
	    p0 = SECU_GetPasswordString(NULL,
					"Enter password for PKCS12 file: ");
	    if (!confirmPw)
		break;
	    p1 = SECU_GetPasswordString(NULL, "Re-enter password: ");
	    if (PL_strcmp(p0, p1) == 0)
		break;
	}
    } else if (p12FilePw->source == PW_FROMFILE) {
	p0 = SECU_FilePasswd(NULL, PR_FALSE, p12FilePw->data);
    } else { /* Plaintext */
	p0 = p12FilePw->data;
    }

    pwItem = SECITEM_AllocItem(NULL, NULL, PL_strlen(p0) + 1);
    memcpy(pwItem->data, p0, pwItem->len);

    PORT_Memset(p0, 0, PL_strlen(p0));
    PORT_Free(p0);

    PORT_Memset(p1, 0, PL_strlen(p1));
    PORT_Free(p1);

    return pwItem;
}

SECStatus
P12U_InitSlot(PK11SlotInfo *slot, secuPWData *slotPw)
{
    SECStatus rv;

    /*	New databases, initialize keydb password. */
    if (PK11_NeedUserInit(slot)) {
	rv = SECU_ChangePW(slot,
			   (slotPw->source == PW_PLAINTEXT) ? slotPw->data : 0,
			   (slotPw->source == PW_FROMFILE) ? slotPw->data : 0);
	if (rv != SECSuccess) {
	    SECU_PrintError(progName, "Failed to initialize slot \"%s\"",
				    PK11_GetSlotName(slot));
	    return SECFailure;
	}
    }

    if (PK11_Authenticate(slot, PR_TRUE, slotPw) != SECSuccess) {
	SECU_PrintError(progName,
			 "Failed to authenticate to PKCS11 slot");
	PORT_SetError(SEC_ERROR_USER_CANCELLED);
	pk12uErrno = PK12UERR_USER_CANCELLED;
	return SECFailure;
    }

    return SECSuccess;
}

/*
 * given a filename for pkcs12 file, imports certs and keys
 *
 * Change: altitude
 *  I've changed this function so that it takes the keydb and pkcs12 file
 *  passwords from files.  The "pwdKeyDB" and "pwdP12File"
 *  variables have been added for this purpose.
 */
PRIntn
P12U_ImportPKCS12Object(char *in_file, PK11SlotInfo *slot,
			secuPWData *slotPw, secuPWData *p12FilePw)
{
    p12uContext *p12cxt = NULL, *tmpcxt = NULL;
    unsigned char inBuf[PKCS12_IN_BUFFER_SIZE];
    SEC_PKCS12DecoderContext *p12dcx = NULL;
    SECItem *pwitem = NULL, uniPwitem = { 0 };
    SECItem p12file = { 0 };
    SECStatus rv = SECFailure;
    PRBool swapUnicode = PR_FALSE;
    int error;

#ifdef IS_LITTLE_ENDIAN
    swapUnicode = PR_TRUE;
#endif

    rv = P12U_InitSlot(slot, slotPw);
    if (rv != SECSuccess) {
	SECU_PrintError(progName, "Failed to authenticate to \"%s\"",
			       			 PK11_GetSlotName(slot));
	pk12uErrno = PK12UERR_PK11GETSLOT;
	goto loser;
    }

    p12cxt = p12u_InitFile(PR_TRUE, in_file);
    if(!p12cxt) {
	SECU_PrintError(progName,"Initialization failed: %s", in_file);
	pk12uErrno = PK12UERR_INIT_FILE;
	goto loser;
    }

    /* get the password */
    pwitem = P12U_GetP12FilePassword(PR_FALSE, p12FilePw);
    if (!pwitem) {
	pk12uErrno = PK12UERR_USER_CANCELLED;
	goto loser;
    }

    if(P12U_UnicodeConversion(NULL, &uniPwitem, pwitem, PR_TRUE,
			      swapUnicode) != SECSuccess) {
	SECU_PrintError(progName,"Unicode conversion failed");
	pk12uErrno = PK12UERR_UNICODECONV;
	goto loser;
    }

    tmpcxt = p12u_CreateTemporaryDigestFile();
    if(!tmpcxt) {
	SECU_PrintError(progName,"Create Temporary digest file failed");
	pk12uErrno = PK12UERR_TMPDIGCREATE;
	goto loser;
    }

    /* init the decoder context */
    p12dcx = SEC_PKCS12DecoderStart(&uniPwitem, slot, NULL,
				    p12u_DigestOpen, p12u_DigestClose,
				    p12u_DigestRead, p12u_DigestWrite,
				    tmpcxt);
    if(!p12dcx) {
	SECU_PrintError(progName,"PKCS12 decoder start failed");
	pk12uErrno = PK12UERR_PK12DECODESTART;
	goto loser;
    }

    /* decode the item */
    rv = SECU_FileToItem(&p12file, p12cxt->file);
    if (rv != SECSuccess) {
	SECU_PrintError(progName,"Failed to read from import file");
	goto loser;
    }
    rv = SEC_PKCS12DecoderUpdate(p12dcx, p12file.data, p12file.len);

    if(rv != SECSuccess) {
	error = PR_GetError();
	if(error == SEC_ERROR_DECRYPTION_DISALLOWED) {
	    PR_SetError(error, 0);
	    goto loser;
	}
#ifdef EXTRA
	/* unable to import as a new blob, it might be an old one */
	if(p12u_TryToImportOldPDU(p12cxt, pwitem, slot, import_arg->nickCb,
				  import_arg->proto_win) != SECSuccess) {
	    goto loser;
	}
	goto tried_pdu_import;
#endif /* EXTRA */
	SECU_PrintError(progName,"PKCS12 decoding failed");
	pk12uErrno = PK12UERR_DECODE;
    }

    rv = SECFailure;

    /* does the blob authenticate properly? */
    if(SEC_PKCS12DecoderVerify(p12dcx) != SECSuccess) {
	SECU_PrintError(progName,"PKCS12 decode not verified");
	pk12uErrno = PK12UERR_DECODEVERIFY;
	goto loser;
    }

    /* make sure the bags are okey dokey -- nicknames correct, etc. */
    if (SEC_PKCS12DecoderValidateBags(p12dcx, P12U_NicknameCollisionCallback)
	 != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_PKCS12_DUPLICATE_DATA) {
	    pk12uErrno = PK12UERR_CERTALREADYEXISTS;
	} else {
	    pk12uErrno = PK12UERR_DECODEVALIBAGS;
	}
	SECU_PrintError(progName,"PKCS12 decode validate bags failed");
	goto loser;
    }

    /* stuff 'em in */
    if(SEC_PKCS12DecoderImportBags(p12dcx) != SECSuccess) {
	SECU_PrintError(progName,"PKCS12 decode import bags failed");
	pk12uErrno = PK12UERR_DECODEIMPTBAGS;
	goto loser;
    }

#if 0
    /* important - to add the password hash into the key database */
    rv = PK11_CheckUserPassword(slot, pw_string);
    if( rv != SECSuccess ) {
	SECU_PrintError(progName,"Failed to CheckUserPassword");
	exit(-1);
    }
#endif

    PR_Close(p12cxt->file);
    p12cxt->file = NULL;
    PK11_FreeSlot(slot);

    rv = SECSuccess;

loser:
    if (rv != SECSuccess) {
      /* pk12u_report_failure */
    } else {
      /* pk12u_report_success ? */
    }

    if (p12dcx) {
	SEC_PKCS12DecoderFinish(p12dcx);
    }
    p12u_DestroyExportFileInfo(&p12cxt, PR_FALSE);

    if (uniPwitem.data) {
	SECITEM_ZfreeItem(&uniPwitem, PR_FALSE);
    }

    if (pwitem) {
	SECITEM_ZfreeItem(pwitem, PR_TRUE);
    }


    return rv;
}

static void
p12u_DoPKCS12ExportErrors()
{
    int error_value;

    error_value = PORT_GetError();
    if ((error_value == SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY) ||
	(error_value == SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME) ||
	(error_value == SEC_ERROR_PKCS12_UNABLE_TO_WRITE)) {
	fprintf(stderr, SECU_ErrorStringRaw((int16)error_value));
    } else if(error_value == SEC_ERROR_USER_CANCELLED) {
	;
    } else {
	fprintf(stderr, SECU_ErrorStringRaw(SEC_ERROR_EXPORTING_CERTIFICATES));
    }
}

static void
p12u_WriteToExportFile(void *arg, const char *buf, unsigned long len)
{
    p12uContext *p12cxt = arg;
    int writeLen;

    if(!p12cxt || (p12cxt->error == PR_TRUE)) {
	return;
    }

    if(p12cxt->file == NULL) {
	p12cxt->errorValue = SEC_ERROR_PKCS12_UNABLE_TO_WRITE;
	p12cxt->error = PR_TRUE;
	return;
    }

    writeLen = PR_Write(p12cxt->file, (unsigned char *)buf, (int32)len);

    if(writeLen != (int)len) {
	PR_Close(p12cxt->file);
	PR_Free(p12cxt->filename);
	p12cxt->filename = NULL;
	p12cxt->file = NULL;
	p12cxt->errorValue = SEC_ERROR_PKCS12_UNABLE_TO_WRITE;
	p12cxt->error = PR_TRUE;
    }
}

void
P12U_ExportPKCS12Object(char *nn, char *outfile,
			secuPWData *slotPw, secuPWData *p12FilePw)
{
    SEC_PKCS12ExportContext *p12ecx = NULL;
    SEC_PKCS12SafeInfo *keySafe = NULL, *certSafe = NULL;
    SECItem *pwitem = NULL;
    PK11SlotInfo *slot = NULL;
    p12uContext *p12cxt = NULL;
    CERTCertificate *cert;

    cert = PK11_FindCertFromNickname(nn, NULL);
    if(!cert) {
	SECU_PrintError(progName,"find cert by nickname failed");
	pk12uErrno = PK12UERR_FINDCERTBYNN;
	return;
    }

    if (!cert->slot) {
	SECU_PrintError(progName,"cert does not have a slot");
	pk12uErrno = PK12UERR_FINDCERTBYNN;
	goto loser;
    }
    if (P12U_InitSlot(cert->slot, slotPw) != SECSuccess) {
	SECU_PrintError(progName,"Failed to authenticate to \"%s\"",
			  PK11_GetSlotName(cert->slot));
	pk12uErrno = PK12UERR_PK11GETSLOT;
	goto loser;
    }

    /*	Password to use for PKCS12 file.  */
    pwitem = P12U_GetP12FilePassword(PR_TRUE, p12FilePw);
    if(!pwitem) {
	goto loser;
    }

    p12ecx = SEC_PKCS12CreateExportContext(NULL, NULL, cert->slot, NULL);
    if(!p12ecx) {
	SECU_PrintError(progName,"export context creation failed");
	pk12uErrno = PK12UERR_EXPORTCXCREATE;
	goto loser;
    }

    if(SEC_PKCS12AddPasswordIntegrity(p12ecx, pwitem, SEC_OID_SHA1)
       != SECSuccess) {
	SECU_PrintError(progName,"PKCS12 add password integrity failed");
	pk12uErrno = PK12UERR_PK12ADDPWDINTEG;
	goto loser;
    }

    keySafe = SEC_PKCS12CreateUnencryptedSafe(p12ecx);
    if(/*!SEC_PKCS12IsEncryptionAllowed() || */ PK11_IsFIPS()) {
	certSafe = keySafe;
    } else {
	certSafe = SEC_PKCS12CreatePasswordPrivSafe(p12ecx, pwitem,
			SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC);
    }

    if(!certSafe || !keySafe) {
	SECU_PrintError(progName,"key or cert safe creation failed");
	pk12uErrno = PK12UERR_CERTKEYSAFE;
	goto loser;
    }

    if(SEC_PKCS12AddCertAndKey(p12ecx, certSafe, NULL, cert,
			CERT_GetDefaultCertDB(), keySafe, NULL, PR_TRUE, pwitem,
			SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC)
			!= SECSuccess) {
	SECU_PrintError(progName,"add cert and key failed");
	pk12uErrno = PK12UERR_ADDCERTKEY;
	goto loser;
    }

    p12cxt = p12u_InitFile(PR_FALSE, outfile); 
    if(!p12cxt) {
	SECU_PrintError(progName,"Initialization failed: %s", outfile);
	pk12uErrno = PK12UERR_INIT_FILE;
	goto loser;
    }

    if(SEC_PKCS12Encode(p12ecx, p12u_WriteToExportFile, p12cxt)
			!= SECSuccess) {
	SECU_PrintError(progName,"PKCS12 encode failed");
	pk12uErrno = PK12UERR_ENCODE;
	goto loser;
    }

    p12u_DestroyExportFileInfo(&p12cxt, PR_FALSE);
    SECITEM_ZfreeItem(pwitem, PR_TRUE);
    CERT_DestroyCertificate(cert);
    if(slot) {
	PK11_FreeSlot(slot);
    }

    fprintf(stdout, "%s: PKCS12 EXPORT SUCCESSFUL\n", progName);
    SEC_PKCS12DestroyExportContext(p12ecx);

    return;

loser:
    SEC_PKCS12DestroyExportContext(p12ecx);

    if (slotPw)
	PR_Free(slotPw->data);

    if (p12FilePw)
	PR_Free(p12FilePw->data);

    if(slot && (slot != cert->slot)) {
	PK11_FreeSlot(slot);
    }
    if(cert) {
	CERT_DestroyCertificate(cert);
    }
    p12u_DestroyExportFileInfo(&p12cxt, PR_TRUE);
    if(pwitem) {
	SECITEM_ZfreeItem(pwitem, PR_TRUE);
    }
    p12u_DoPKCS12ExportErrors();
    return;
}

static void
p12u_EnableAllCiphers()
{
    SEC_PKCS12EnableCipher(PKCS12_RC4_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC4_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_40, 1);
    SEC_PKCS12EnableCipher(PKCS12_RC2_CBC_128, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_56, 1);
    SEC_PKCS12EnableCipher(PKCS12_DES_EDE3_168, 1);
    SEC_PKCS12SetPreferredCipher(PKCS12_DES_EDE3_168, 1);
}

static PRUintn
P12U_Init(char *dir, char *dbprefix)
{
    SECStatus rv;
    PK11_SetPasswordFunc(SECU_GetModulePassword);

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    rv = NSS_Initialize(dir,dbprefix,dbprefix,"secmod.db",0);
    if (rv != SECSuccess) {
    	SECU_PrintPRandOSError(progName);
        exit(-1);
    }

    /* setup unicode callback functions */
    PORT_SetUCS2_ASCIIConversionFunction(p12u_ucs2_ascii_conversion_function);
    /* use the defaults for UCS4-UTF8 and UCS2-UTF8 */

    p12u_EnableAllCiphers();

    return 0;
}

enum {
    opt_CertDir = 0,
    opt_TokenName,
    opt_Import,
    opt_SlotPWFile,
    opt_SlotPW,
    opt_Mode,
    opt_Nickname,
    opt_Export,
    opt_P12FilePWFile,
    opt_P12FilePW,
    opt_DBPrefix,
    opt_Debug
};

static secuCommandFlag pk12util_options[] =
{
    { /* opt_CertDir	       */ 'd', PR_TRUE,	 0, PR_FALSE },
    { /* opt_TokenName	       */ 'h', PR_TRUE,	 0, PR_FALSE },
    { /* opt_Import	       */ 'i', PR_TRUE,	 0, PR_FALSE },
    { /* opt_SlotPWFile	       */ 'k', PR_TRUE,	 0, PR_FALSE },
    { /* opt_SlotPW	       */ 'K', PR_TRUE,	 0, PR_FALSE },
    { /* opt_Mode	       */ 'm', PR_TRUE,	 0, PR_FALSE },
    { /* opt_Nickname	       */ 'n', PR_TRUE,	 0, PR_FALSE },
    { /* opt_Export	       */ 'o', PR_TRUE,	 0, PR_FALSE },
    { /* opt_P12FilePWFile     */ 'w', PR_TRUE,	 0, PR_FALSE },
    { /* opt_P12FilePW	       */ 'W', PR_TRUE,	 0, PR_FALSE },
    { /* opt_DBPrefix	       */ 'P', PR_TRUE,	 0, PR_FALSE },
    { /* opt_Debug	       */ 'v', PR_FALSE, 0, PR_FALSE }
};

int
main(int argc, char **argv)
{
    PRIntn ret = 0;
    secuPWData slotPw = { PW_NONE, NULL };
    secuPWData p12FilePw = { PW_NONE, NULL };
    PK11SlotInfo *slot;
    char *slotname = NULL;
    char *import_file = NULL;
    char *export_file = NULL;
    char *dbprefix = "";
    SECStatus rv;

    secuCommand pk12util;
    pk12util.numCommands = 0;
    pk12util.commands = 0;
    pk12util.numOptions = sizeof(pk12util_options) / sizeof(secuCommandFlag);
    pk12util.options = pk12util_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    rv = SECU_ParseCommandLine(argc, argv, progName, &pk12util);

    if (rv != SECSuccess)
	Usage(progName);

    pk12_debugging = pk12util.options[opt_Debug].activated;

    if (pk12util.options[opt_Import].activated &&
	pk12util.options[opt_Export].activated) {
	Usage(progName);
    }

    if (pk12util.options[opt_Export].activated &&
       !pk12util.options[opt_Nickname].activated) {
	Usage(progName);
    }

    slotname = SECU_GetOptionArg(&pk12util, opt_TokenName);
    import_file = SECU_GetOptionArg(&pk12util, opt_Import);
    export_file = SECU_GetOptionArg(&pk12util, opt_Export);

    if (pk12util.options[opt_P12FilePWFile].activated) {
	p12FilePw.source = PW_FROMFILE;
	p12FilePw.data = PL_strdup(pk12util.options[opt_P12FilePWFile].arg);
    }

    if (pk12util.options[opt_P12FilePW].activated) {
	p12FilePw.source = PW_PLAINTEXT;
	p12FilePw.data = PL_strdup(pk12util.options[opt_P12FilePW].arg);
    }

    if (pk12util.options[opt_SlotPWFile].activated) {
	slotPw.source = PW_FROMFILE;
	slotPw.data = PL_strdup(pk12util.options[opt_SlotPWFile].arg);
    }

    if (pk12util.options[opt_SlotPW].activated) {
	slotPw.source = PW_PLAINTEXT;
	slotPw.data = PL_strdup(pk12util.options[opt_SlotPW].arg);
    }

    if (pk12util.options[opt_CertDir].activated) {
	SECU_ConfigDirectory(pk12util.options[opt_CertDir].arg);
    }
    if (pk12util.options[opt_DBPrefix].activated) {
    	dbprefix = pk12util.options[opt_DBPrefix].arg;
    }
    P12U_Init(SECU_ConfigDirectory(NULL),dbprefix);

    if (pk12util.options[opt_Import].activated) {

	if (!slotname || PL_strcmp(slotname, "internal") == 0)
	    slot = PK11_GetInternalKeySlot();
	else
	    slot = PK11_FindSlotByName(slotname);

	if (!slot) {
	    SECU_PrintError(progName,"Invalid slot \"%s\"", slotname);
	    goto done;
	}

	if ((ret = P12U_ImportPKCS12Object(import_file, slot, &slotPw,
					   &p12FilePw)) != 0)
	    goto done;

    } else if (pk12util.options[opt_Export].activated) {

	P12U_ExportPKCS12Object(pk12util.options[opt_Nickname].arg,
				export_file, &slotPw, &p12FilePw);
    } else {
	Usage(progName);
	pk12uErrno = PK12UERR_USAGE;
    }

done:
    NSS_Shutdown();
    exit(pk12uErrno);
}
