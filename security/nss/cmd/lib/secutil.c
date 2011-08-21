/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
** secutil.c - various functions used by security stuff
**
*/

#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"
#include "prerror.h"
#include "prprf.h"
#include "plgetopt.h"
#include "prenv.h"
#include "prnetdb.h"

#include "cryptohi.h"
#include "secutil.h"
#include "secpkcs7.h"
#include "secpkcs5.h"
#include <stdarg.h>
#if !defined(_WIN32_WCE)
#include <sys/stat.h>
#include <errno.h>
#endif

#ifdef XP_UNIX
#include <unistd.h>
#endif

/* for SEC_TraverseNames */
#include "cert.h"
#include "certt.h"
#include "certdb.h"

/* #include "secmod.h" */
#include "pk11func.h"
#include "secoid.h"

static char consoleName[] =  {
#ifdef XP_UNIX
    "/dev/tty"
#else
#ifdef XP_OS2
    "\\DEV\\CON"
#else
    "CON:"
#endif
#endif
};

#include "nssutil.h"
#include "ssl.h"


void 
SECU_PrintErrMsg(FILE *out, int level, char *progName, char *msg, ...)
{
    va_list args;
    PRErrorCode err = PORT_GetError();
    const char * errString = SECU_Strerror(err);

    va_start(args, msg);

    SECU_Indent(out, level);
    fprintf(out, "%s: ", progName);
    vfprintf(out, msg, args);
    if (errString != NULL && PORT_Strlen(errString) > 0)
	fprintf(out, ": %s\n", errString);
    else
	fprintf(out, ": error %d\n", (int)err);

    va_end(args);
}

void 
SECU_PrintError(char *progName, char *msg, ...)
{
    va_list args;
    PRErrorCode err = PORT_GetError();
    const char * errString = SECU_Strerror(err);

    va_start(args, msg);

    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);
    if (errString != NULL && PORT_Strlen(errString) > 0)
	fprintf(stderr, ": %s\n", errString);
    else
	fprintf(stderr, ": error %d\n", (int)err);

    va_end(args);
}

void
SECU_PrintSystemError(char *progName, char *msg, ...)
{
    va_list args;

    va_start(args, msg);
    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);
#if defined(_WIN32_WCE)
    fprintf(stderr, ": %d\n", PR_GetOSError());
#else
    fprintf(stderr, ": %s\n", strerror(errno));
#endif
    va_end(args);
}

static void
secu_ClearPassword(char *p)
{
    if (p) {
	PORT_Memset(p, 0, PORT_Strlen(p));
	PORT_Free(p);
    }
}

char *
SECU_GetPasswordString(void *arg, char *prompt)
{
#ifndef _WINDOWS
    char *p = NULL;
    FILE *input, *output;

    /* open terminal */
    input = fopen(consoleName, "r");
    if (input == NULL) {
	fprintf(stderr, "Error opening input terminal for read\n");
	return NULL;
    }

    output = fopen(consoleName, "w");
    if (output == NULL) {
	fprintf(stderr, "Error opening output terminal for write\n");
	return NULL;
    }

    p = SEC_GetPassword (input, output, prompt, SEC_BlindCheckPassword);
        

    fclose(input);
    fclose(output);

    return p;

#else
    /* Win32 version of above. opening the console may fail
       on windows95, and certainly isn't necessary.. */

    char *p = NULL;

    p = SEC_GetPassword (stdin, stdout, prompt, SEC_BlindCheckPassword);
    return p;

#endif
}


/*
 *  p a s s w o r d _ h a r d c o d e 
 *
 *  A function to use the password passed in the -f(pwfile) argument
 *  of the command line.  
 *  After use once, null it out otherwise PKCS11 calls us forever.?
 *
 */
char *
SECU_FilePasswd(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    char* phrases, *phrase;
    PRFileDesc *fd;
    PRInt32 nb;
    char *pwFile = arg;
    int i;
    const long maxPwdFileSize = 4096;
    char* tokenName = NULL;
    int tokenLen = 0;

    if (!pwFile)
	return 0;

    if (retry) {
	return 0;  /* no good retrying - the files contents will be the same */
    }

    phrases = PORT_ZAlloc(maxPwdFileSize);

    if (!phrases) {
        return 0; /* out of memory */
    }
 
    fd = PR_Open(pwFile, PR_RDONLY, 0);
    if (!fd) {
	fprintf(stderr, "No password file \"%s\" exists.\n", pwFile);
        PORT_Free(phrases);
	return NULL;
    }

    nb = PR_Read(fd, phrases, maxPwdFileSize);
  
    PR_Close(fd);

    if (nb == 0) {
        fprintf(stderr,"password file contains no data\n");
        PORT_Free(phrases);
        return NULL;
    }

    if (slot) {
        tokenName = PK11_GetTokenName(slot);
        if (tokenName) {
            tokenLen = PORT_Strlen(tokenName);
        }
    }
    i = 0;
    do
    {
        int startphrase = i;
        int phraseLen;

        /* handle the Windows EOL case */
        while (phrases[i] != '\r' && phrases[i] != '\n' && i < nb) i++;
        /* terminate passphrase */
        phrases[i++] = '\0';
        /* clean up any EOL before the start of the next passphrase */
        while ( (i<nb) && (phrases[i] == '\r' || phrases[i] == '\n')) {
            phrases[i++] = '\0';
        }
        /* now analyze the current passphrase */
        phrase = &phrases[startphrase];
        if (!tokenName)
            break;
        if (PORT_Strncmp(phrase, tokenName, tokenLen)) continue;
        phraseLen = PORT_Strlen(phrase);
        if (phraseLen < (tokenLen+1)) continue;
        if (phrase[tokenLen] != ':') continue;
        phrase = &phrase[tokenLen+1];
        break;

    } while (i<nb);

    phrase = PORT_Strdup((char*)phrase);
    PORT_Free(phrases);
    return phrase;
}

char *
SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg) 
{
    char prompt[255];
    secuPWData *pwdata = (secuPWData *)arg;
    secuPWData pwnull = { PW_NONE, 0 };
    secuPWData pwxtrn = { PW_EXTERNAL, "external" };
    char *pw;

    if (pwdata == NULL)
	pwdata = &pwnull;

    if (PK11_ProtectedAuthenticationPath(slot)) {
	pwdata = &pwxtrn;
    }
    if (retry && pwdata->source != PW_NONE) {
	PR_fprintf(PR_STDERR, "Incorrect password/PIN entered.\n");
    	return NULL;
    }

    switch (pwdata->source) {
    case PW_NONE:
	sprintf(prompt, "Enter Password or Pin for \"%s\":",
	                 PK11_GetTokenName(slot));
	return SECU_GetPasswordString(NULL, prompt);
    case PW_FROMFILE:
	/* Instead of opening and closing the file every time, get the pw
	 * once, then keep it in memory (duh).
	 */
	pw = SECU_FilePasswd(slot, retry, pwdata->data);
	pwdata->source = PW_PLAINTEXT;
	pwdata->data = PL_strdup(pw);
	/* it's already been dup'ed */
	return pw;
    case PW_EXTERNAL:
	sprintf(prompt, 
	        "Press Enter, then enter PIN for \"%s\" on external device.\n",
		PK11_GetTokenName(slot));
	(void) SECU_GetPasswordString(NULL, prompt);
    	/* Fall Through */
    case PW_PLAINTEXT:
	return PL_strdup(pwdata->data);
    default:
	break;
    }

    PR_fprintf(PR_STDERR, "Password check failed:  No password found.\n");
    return NULL;
}

char *
secu_InitSlotPassword(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    char *p0 = NULL;
    char *p1 = NULL;
    FILE *input, *output;
    secuPWData *pwdata = arg;

    if (pwdata->source == PW_FROMFILE) {
	return SECU_FilePasswd(slot, retry, pwdata->data);
    } 
    if (pwdata->source == PW_PLAINTEXT) {
	return PL_strdup(pwdata->data);
    }
    
    /* PW_NONE - get it from tty */
    /* open terminal */
#ifdef _WINDOWS
    input = stdin;
#else
    input = fopen(consoleName, "r");
#endif
    if (input == NULL) {
	PR_fprintf(PR_STDERR, "Error opening input terminal for read\n");
	return NULL;
    }

    /* we have no password, so initialize database with one */
    PR_fprintf(PR_STDERR, 
        "Enter a password which will be used to encrypt your keys.\n"
     	"The password should be at least 8 characters long,\n"
     	"and should contain at least one non-alphabetic character.\n\n");

    output = fopen(consoleName, "w");
    if (output == NULL) {
	PR_fprintf(PR_STDERR, "Error opening output terminal for write\n");
	return NULL;
    }


    for (;;) {
	if (p0) 
	    PORT_Free(p0);
	p0 = SEC_GetPassword(input, output, "Enter new password: ",
			     SEC_BlindCheckPassword);

	if (p1)
	    PORT_Free(p1);
	p1 = SEC_GetPassword(input, output, "Re-enter password: ",
			     SEC_BlindCheckPassword);
	if (p0 && p1 && !PORT_Strcmp(p0, p1)) {
	    break;
	}
	PR_fprintf(PR_STDERR, "Passwords do not match. Try again.\n");
    }
        
    /* clear out the duplicate password string */
    secu_ClearPassword(p1);
    
    fclose(input);
    fclose(output);

    return p0;
}

SECStatus
SECU_ChangePW(PK11SlotInfo *slot, char *passwd, char *pwFile)
{
    return SECU_ChangePW2(slot, passwd, 0, pwFile, 0);
}

SECStatus
SECU_ChangePW2(PK11SlotInfo *slot, char *oldPass, char *newPass,
			char *oldPwFile, char *newPwFile)
{
    SECStatus rv;
    secuPWData pwdata, newpwdata;
    char *oldpw = NULL, *newpw = NULL;

    if (oldPass) {
	pwdata.source = PW_PLAINTEXT;
	pwdata.data = oldPass;
    } else if (oldPwFile) {
	pwdata.source = PW_FROMFILE;
	pwdata.data = oldPwFile;
    } else {
	pwdata.source = PW_NONE;
	pwdata.data = NULL;
    }

    if (newPass) {
	newpwdata.source = PW_PLAINTEXT;
	newpwdata.data = newPass;
    } else if (newPwFile) {
	newpwdata.source = PW_FROMFILE;
	newpwdata.data = newPwFile;
    } else {
	newpwdata.source = PW_NONE;
	newpwdata.data = NULL;
    }

    if (PK11_NeedUserInit(slot)) {
	newpw = secu_InitSlotPassword(slot, PR_FALSE, &pwdata);
	rv = PK11_InitPin(slot, (char*)NULL, newpw);
	goto done;
    }

    for (;;) {
	oldpw = SECU_GetModulePassword(slot, PR_FALSE, &pwdata);

	if (PK11_CheckUserPassword(slot, oldpw) != SECSuccess) {
	    if (pwdata.source == PW_NONE) {
		PR_fprintf(PR_STDERR, "Invalid password.  Try again.\n");
	    } else {
		PR_fprintf(PR_STDERR, "Invalid password.\n");
		PORT_Memset(oldpw, 0, PL_strlen(oldpw));
		PORT_Free(oldpw);
		return SECFailure;
	    }
	} else
	    break;

	PORT_Free(oldpw);
    }

    newpw = secu_InitSlotPassword(slot, PR_FALSE, &newpwdata);

    if (PK11_ChangePW(slot, oldpw, newpw) != SECSuccess) {
	PR_fprintf(PR_STDERR, "Failed to change password.\n");
	return SECFailure;
    }

    PORT_Memset(oldpw, 0, PL_strlen(oldpw));
    PORT_Free(oldpw);

    PR_fprintf(PR_STDOUT, "Password changed successfully.\n");

done:
    PORT_Memset(newpw, 0, PL_strlen(newpw));
    PORT_Free(newpw);
    return SECSuccess;
}

struct matchobj {
    SECItem index;
    char *nname;
    PRBool found;
};

char *
SECU_DefaultSSLDir(void)
{
    char *dir;
    static char sslDir[1000];

    dir = PR_GetEnv("SSL_DIR");
    if (!dir)
	return NULL;

    sprintf(sslDir, "%s", dir);

    if (sslDir[strlen(sslDir)-1] == '/')
	sslDir[strlen(sslDir)-1] = 0;

    return sslDir;
}

char *
SECU_AppendFilenameToDir(char *dir, char *filename)
{
    static char path[1000];

    if (dir[strlen(dir)-1] == '/')
	sprintf(path, "%s%s", dir, filename);
    else
	sprintf(path, "%s/%s", dir, filename);
    return path;
}

char *
SECU_ConfigDirectory(const char* base)
{
    static PRBool initted = PR_FALSE;
    const char *dir = ".netscape";
    char *home;
    static char buf[1000];

    if (initted) return buf;
    

    if (base == NULL || *base == 0) {
	home = PR_GetEnv("HOME");
	if (!home) home = "";

	if (*home && home[strlen(home) - 1] == '/')
	    sprintf (buf, "%.900s%s", home, dir);
	else
	    sprintf (buf, "%.900s/%s", home, dir);
    } else {
	sprintf(buf, "%.900s", base);
	if (buf[strlen(buf) - 1] == '/')
	    buf[strlen(buf) - 1] = 0;
    }


    initted = PR_TRUE;
    return buf;
}

/*Turn off SSL for now */
/* This gets called by SSL when server wants our cert & key */
int
SECU_GetClientAuthData(void *arg, PRFileDesc *fd,
		       struct CERTDistNamesStr *caNames,
                      struct CERTCertificateStr **pRetCert,
                      struct SECKEYPrivateKeyStr **pRetKey)
{
    SECKEYPrivateKey *key;
    CERTCertificate *cert;
    int errsave;

    if (arg == NULL) {
        fprintf(stderr, "no key/cert name specified for client auth\n");
        return -1;
    }
    cert = PK11_FindCertFromNickname(arg, NULL);
    errsave = PORT_GetError();
    if (!cert) {
        if (errsave == SEC_ERROR_BAD_PASSWORD)
            fprintf(stderr, "Bad password\n");
        else if (errsave > 0)
            fprintf(stderr, "Unable to read cert (error %d)\n", errsave);
        else if (errsave == SEC_ERROR_BAD_DATABASE)
            fprintf(stderr, "Unable to get cert from database (%d)\n", errsave);
        else
            fprintf(stderr, "SECKEY_FindKeyByName: internal error %d\n", errsave);
        return -1;
    }

    key = PK11_FindKeyByAnyCert(arg,NULL);
    if (!key) {
        fprintf(stderr, "Unable to get key (%d)\n", PORT_GetError());
        return -1;
    }


    *pRetCert = cert;
    *pRetKey = key;

    return 0;
}

SECStatus
secu_StdinToItem(SECItem *dst)
{
    unsigned char buf[1000];
    PRInt32 numBytes;
    PRBool notDone = PR_TRUE;

    dst->len = 0;
    dst->data = NULL;

    while (notDone) {
	numBytes = PR_Read(PR_STDIN, buf, sizeof(buf));

	if (numBytes < 0) {
	    return SECFailure;
	}

	if (numBytes == 0)
	    break;

	if (dst->data) {
	    unsigned char * p = dst->data;
	    dst->data = (unsigned char*)PORT_Realloc(p, dst->len + numBytes);
	    if (!dst->data) {
	    	PORT_Free(p);
	    }
	} else {
	    dst->data = (unsigned char*)PORT_Alloc(numBytes);
	}
	if (!dst->data) {
	    return SECFailure;
	}
	PORT_Memcpy(dst->data + dst->len, buf, numBytes);
	dst->len += numBytes;
    }

    return SECSuccess;
}

SECStatus
SECU_FileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;

    if (src == PR_STDIN)
	return secu_StdinToItem(dst);

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, info.size))
	goto loser;

    numBytes = PR_Read(src, dst->data, info.size);
    if (numBytes != info.size) {
	PORT_SetError(SEC_ERROR_IO);
	goto loser;
    }

    return SECSuccess;
loser:
    SECITEM_FreeItem(dst, PR_FALSE);
    dst->data = NULL;
    return SECFailure;
}

SECStatus
SECU_TextFileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;
    unsigned char *buf;

    if (src == PR_STDIN)
	return secu_StdinToItem(dst);

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }

    buf = (unsigned char*)PORT_Alloc(info.size);
    if (!buf)
	return SECFailure;

    numBytes = PR_Read(src, buf, info.size);
    if (numBytes != info.size) {
	PORT_SetError(SEC_ERROR_IO);
	goto loser;
    }

    if (buf[numBytes-1] == '\n') numBytes--;
#ifdef _WINDOWS
    if (buf[numBytes-1] == '\r') numBytes--;
#endif

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, numBytes))
	goto loser;

    memcpy(dst->data, buf, numBytes);

    PORT_Free(buf);
    return SECSuccess;
loser:
    PORT_Free(buf);
    return SECFailure;
}

SECStatus
SECU_ReadDERFromFile(SECItem *der, PRFileDesc *inFile, PRBool ascii)
{
    SECStatus rv;
    if (ascii) {
	/* First convert ascii to binary */
	SECItem filedata;
	char *asc, *body;

	/* Read in ascii data */
	rv = SECU_FileToItem(&filedata, inFile);
	asc = (char *)filedata.data;
	if (!asc) {
	    fprintf(stderr, "unable to read data from input file\n");
	    return SECFailure;
	}

	/* check for headers and trailers and remove them */
	if ((body = strstr(asc, "-----BEGIN")) != NULL) {
	    char *trailer = NULL;
	    asc = body;
	    body = PORT_Strchr(body, '\n');
	    if (!body)
		body = PORT_Strchr(asc, '\r'); /* maybe this is a MAC file */
	    if (body)
		trailer = strstr(++body, "-----END");
	    if (trailer != NULL) {
		*trailer = '\0';
	    } else {
		fprintf(stderr, "input has header but no trailer\n");
		PORT_Free(filedata.data);
		return SECFailure;
	    }
	} else {
	    body = asc;
	}
     
	/* Convert to binary */
	rv = ATOB_ConvertAsciiToItem(der, body);
	if (rv) {
	    fprintf(stderr, "error converting ascii to binary (%s)\n",
		    SECU_Strerror(PORT_GetError()));
	    PORT_Free(filedata.data);
	    return SECFailure;
	}

	PORT_Free(filedata.data);
    } else {
	/* Read in binary der */
	rv = SECU_FileToItem(der, inFile);
	if (rv) {
	    fprintf(stderr, "error converting der (%s)\n", 
		    SECU_Strerror(PORT_GetError()));
	    return SECFailure;
	}
    }
    return SECSuccess;
}

#define INDENT_MULT	4
void
SECU_Indent(FILE *out, int level)
{
    int i;

    for (i = 0; i < level; i++) {
	fprintf(out, "    ");
    }
}

static void secu_Newline(FILE *out)
{
    fprintf(out, "\n");
}

void
SECU_PrintAsHex(FILE *out, SECItem *data, const char *m, int level)
{
    unsigned i;
    int column;
    PRBool isString     = PR_TRUE;
    PRBool isWhiteSpace = PR_TRUE;
    PRBool printedHex   = PR_FALSE;
    unsigned int limit = 15;

    if ( m ) {
	SECU_Indent(out, level); fprintf(out, "%s:\n", m);
	level++;
    }
    
    SECU_Indent(out, level); column = level*INDENT_MULT;
    if (!data->len) {
	fprintf(out, "(empty)\n");
	return;
    }
    /* take a pass to see if it's all printable. */
    for (i = 0; i < data->len; i++) {
	unsigned char val = data->data[i];
        if (!val || !isprint(val)) {
	    isString = PR_FALSE;
	    break;
	}
	if (isWhiteSpace && !isspace(val)) {
	    isWhiteSpace = PR_FALSE;
	}
    }

    /* Short values, such as bit strings (which are printed with this
    ** function) often look like strings, but we want to see the bits.
    ** so this test assures that short values will be printed in hex,
    ** perhaps in addition to being printed as strings.
    ** The threshold size (4 bytes) is arbitrary.
    */
    if (!isString || data->len <= 4) {
      for (i = 0; i < data->len; i++) {
	if (i != data->len - 1) {
	    fprintf(out, "%02x:", data->data[i]);
	    column += 3;
	} else {
	    fprintf(out, "%02x", data->data[i]);
	    column += 2;
	    break;
	}
	if (column > 76 || (i % 16 == limit)) {
	    secu_Newline(out);
	    SECU_Indent(out, level); 
	    column = level*INDENT_MULT;
	    limit = i % 16;
	}
      }
      printedHex = PR_TRUE;
    }
    if (isString && !isWhiteSpace) {
	if (printedHex != PR_FALSE) {
	    secu_Newline(out);
	    SECU_Indent(out, level); column = level*INDENT_MULT;
	}
	for (i = 0; i < data->len; i++) {
	    unsigned char val = data->data[i];

	    if (val) {
		fprintf(out,"%c",val);
		column++;
	    } else {
		column = 77;
	    }
	    if (column > 76) {
		secu_Newline(out);
        	SECU_Indent(out, level); column = level*INDENT_MULT;
	    }
	}
    }
	    
    if (column != level*INDENT_MULT) {
	secu_Newline(out);
    }
}

static const char *hex = "0123456789abcdef";

static const char printable[257] = {
	"................"	/* 0x */
	"................"	/* 1x */
	" !\"#$%&'()*+,-./"	/* 2x */
	"0123456789:;<=>?"	/* 3x */
	"@ABCDEFGHIJKLMNO"	/* 4x */
	"PQRSTUVWXYZ[\\]^_"	/* 5x */
	"`abcdefghijklmno"	/* 6x */
	"pqrstuvwxyz{|}~."	/* 7x */
	"................"	/* 8x */
	"................"	/* 9x */
	"................"	/* ax */
	"................"	/* bx */
	"................"	/* cx */
	"................"	/* dx */
	"................"	/* ex */
	"................"	/* fx */
};

void 
SECU_PrintBuf(FILE *out, const char *msg, const void *vp, int len)
{
    const unsigned char *cp = (const unsigned char *)vp;
    char buf[80];
    char *bp;
    char *ap;

    fprintf(out, "%s [Len: %d]\n", msg, len);
    memset(buf, ' ', sizeof buf);
    bp = buf;
    ap = buf + 50;
    while (--len >= 0) {
	unsigned char ch = *cp++;
	*bp++ = hex[(ch >> 4) & 0xf];
	*bp++ = hex[ch & 0xf];
	*bp++ = ' ';
	*ap++ = printable[ch];
	if (ap - buf >= 66) {
	    *ap = 0;
	    fprintf(out, "   %s\n", buf);
	    memset(buf, ' ', sizeof buf);
	    bp = buf;
	    ap = buf + 50;
	}
    }
    if (bp > buf) {
	*ap = 0;
	fprintf(out, "   %s\n", buf);
    }
}

SECStatus
SECU_StripTagAndLength(SECItem *i)
{
    unsigned int start;

    if (!i || !i->data || i->len < 2) { /* must be at least tag and length */
        return SECFailure;
    }
    start = ((i->data[1] & 0x80) ? (i->data[1] & 0x7f) + 2 : 2);
    if (i->len < start) {
        return SECFailure;
    }
    i->data += start;
    i->len  -= start;
    return SECSuccess;
}


/* This expents i->data[0] to be the MSB of the integer.
** if you want to print a DER-encoded integer (with the tag and length)
** call SECU_PrintEncodedInteger();
*/
void
SECU_PrintInteger(FILE *out, SECItem *i, char *m, int level)
{
    int iv;

    if (!i || !i->len || !i->data) {
	SECU_Indent(out, level); 
	if (m) {
	    fprintf(out, "%s: (null)\n", m);
	} else {
	    fprintf(out, "(null)\n");
	}
    } else if (i->len > 4) {
	SECU_PrintAsHex(out, i, m, level);
    } else {
   	if (i->type == siUnsignedInteger && *i->data & 0x80) {
            /* Make sure i->data has zero in the highest bite 
             * if i->data is an unsigned integer */
            SECItem tmpI;
            char data[] = {0, 0, 0, 0, 0};

            PORT_Memcpy(data + 1, i->data, i->len);
            tmpI.len = i->len + 1;
            tmpI.data = (void*)data;

            iv = DER_GetInteger(&tmpI);
	} else {
            iv = DER_GetInteger(i);
	}
	SECU_Indent(out, level); 
	if (m) {
	    fprintf(out, "%s: %d (0x%x)\n", m, iv, iv);
	} else {
	    fprintf(out, "%d (0x%x)\n", iv, iv);
	}
    }
}

static void
secu_PrintRawString(FILE *out, SECItem *si, const char *m, int level)
{
    int column;
    unsigned int i;

    if ( m ) {
	SECU_Indent(out, level); fprintf(out, "%s: ", m);
	column = (level * INDENT_MULT) + strlen(m) + 2;
	level++;
    } else {
	SECU_Indent(out, level); 
	column = level*INDENT_MULT;
    }
    fprintf(out, "\""); column++;

    for (i = 0; i < si->len; i++) {
	unsigned char val = si->data[i];
	if (column > 76) {
	    secu_Newline(out);
	    SECU_Indent(out, level); column = level*INDENT_MULT;
	}

	fprintf(out,"%c", printable[val]); column++;
    }

    fprintf(out, "\""); column++;
    if (column != level*INDENT_MULT || column > 76) {
	secu_Newline(out);
    }
}

void
SECU_PrintString(FILE *out, SECItem *si, char *m, int level)
{
    SECItem my = *si;

    if (SECSuccess != SECU_StripTagAndLength(&my) || !my.len)
    	return;
    secu_PrintRawString(out, &my, m, level);
}

/* print an unencoded boolean */
static void
secu_PrintBoolean(FILE *out, SECItem *i, const char *m, int level)
{
    int val = 0;
    
    if ( i->data && i->len ) {
	val = i->data[0];
    }

    if (!m) {
    	m = "Boolean";
    }
    SECU_Indent(out, level); 
    fprintf(out, "%s: %s\n", m, (val ? "True" : "False"));
}

/*
 * Format and print "time".  If the tag message "m" is not NULL,
 * do indent formatting based on "level" and add a newline afterward;
 * otherwise just print the formatted time string only.
 */
static void
secu_PrintTime(FILE *out, int64 time, char *m, int level)
{
    PRExplodedTime printableTime; 
    char *timeString;

    /* Convert to local time */
    PR_ExplodeTime(time, PR_GMTParameters, &printableTime);

    timeString = PORT_Alloc(256);
    if (timeString == NULL)
	return;

    if (m != NULL) {
	SECU_Indent(out, level);
	fprintf(out, "%s: ", m);
    }

    if (PR_FormatTime(timeString, 256, "%a %b %d %H:%M:%S %Y", &printableTime)) {
        fputs(timeString, out);
    }

    if (m != NULL)
	fprintf(out, "\n");

    PORT_Free(timeString);
}

/*
 * Format and print the UTC Time "t".  If the tag message "m" is not NULL,
 * do indent formatting based on "level" and add a newline afterward;
 * otherwise just print the formatted time string only.
 */
void
SECU_PrintUTCTime(FILE *out, SECItem *t, char *m, int level)
{
    int64 time;
    SECStatus rv;

    rv = DER_UTCTimeToTime(&time, t);
    if (rv != SECSuccess)
	return;

    secu_PrintTime(out, time, m, level);
}

/*
 * Format and print the Generalized Time "t".  If the tag message "m"
 * is not NULL, * do indent formatting based on "level" and add a newline
 * afterward; otherwise just print the formatted time string only.
 */
void
SECU_PrintGeneralizedTime(FILE *out, SECItem *t, char *m, int level)
{
    int64 time;
    SECStatus rv;


    rv = DER_GeneralizedTimeToTime(&time, t);
    if (rv != SECSuccess)
	return;

    secu_PrintTime(out, time, m, level);
}

/*
 * Format and print the UTC or Generalized Time "t".  If the tag message
 * "m" is not NULL, do indent formatting based on "level" and add a newline
 * afterward; otherwise just print the formatted time string only.
 */
void
SECU_PrintTimeChoice(FILE *out, SECItem *t, char *m, int level)
{
    switch (t->type) {
        case siUTCTime:
            SECU_PrintUTCTime(out, t, m, level);
            break;

        case siGeneralizedTime:
            SECU_PrintGeneralizedTime(out, t, m, level);
            break;

        default:
            PORT_Assert(0);
            break;
    }
}


/* This prints a SET or SEQUENCE */
void
SECU_PrintSet(FILE *out, SECItem *t, char *m, int level)
{
    int            type        = t->data[0] & SEC_ASN1_TAGNUM_MASK;
    int            constructed = t->data[0] & SEC_ASN1_CONSTRUCTED;
    const char *   label;
    SECItem        my          = *t;

    if (!constructed) {
	SECU_PrintAsHex(out, t, m, level);
        return;
    }
    if (SECSuccess != SECU_StripTagAndLength(&my))
    	return;

    SECU_Indent(out, level);
    if (m) {
    	fprintf(out, "%s: ", m);
    }

    if (type == SEC_ASN1_SET)
    	label = "Set ";
    else if (type == SEC_ASN1_SEQUENCE)
    	label = "Sequence ";
    else
    	label = "";
    fprintf(out,"%s{\n", label); /* } */

    while (my.len >= 2) {
	SECItem  tmp = my;

        if (tmp.data[1] & 0x80) {
	    unsigned int i;
	    unsigned int lenlen = tmp.data[1] & 0x7f;
	    if (lenlen > sizeof tmp.len)
	        break;
	    tmp.len = 0;
	    for (i=0; i < lenlen; i++) {
		tmp.len = (tmp.len << 8) | tmp.data[2+i];
	    }
	    tmp.len += lenlen + 2;
	} else {
	    tmp.len = tmp.data[1] + 2;
	}
	if (tmp.len > my.len) {
	    tmp.len = my.len;
	}
	my.data += tmp.len;
	my.len  -= tmp.len;
	SECU_PrintAny(out, &tmp, NULL, level + 1);
    }
    SECU_Indent(out, level); fprintf(out, /* { */ "}\n");
}

static void
secu_PrintContextSpecific(FILE *out, SECItem *i, char *m, int level)
{
    int type        = i->data[0] & SEC_ASN1_TAGNUM_MASK;
    int constructed = i->data[0] & SEC_ASN1_CONSTRUCTED;
    SECItem tmp;

    if (constructed) {
	char * m2;
	if (!m) 
	    m2 = PR_smprintf("[%d]", type);
	else
	    m2 = PR_smprintf("%s: [%d]", m, type);
	if (m2) {
	    SECU_PrintSet(out, i, m2, level);
	    PR_smprintf_free(m2);
	}
	return;
    }

    SECU_Indent(out, level);
    if (m) {
    	fprintf(out, "%s: ", m);
    }
    fprintf(out,"[%d]\n", type);

    tmp = *i;
    if (SECSuccess == SECU_StripTagAndLength(&tmp))
	SECU_PrintAsHex(out, &tmp, m, level+1);
}

static void
secu_PrintOctetString(FILE *out, SECItem *i, char *m, int level)
{
    SECItem tmp = *i;
    if (SECSuccess == SECU_StripTagAndLength(&tmp))
	SECU_PrintAsHex(out, &tmp, m, level);
}

static void
secu_PrintBitString(FILE *out, SECItem *i, char *m, int level)
{
    int unused_bits;
    SECItem tmp = *i;

    if (SECSuccess != SECU_StripTagAndLength(&tmp) || tmp.len < 2)
    	return;

    unused_bits = *tmp.data++;
    tmp.len--;

    SECU_PrintAsHex(out, &tmp, m, level);
    if (unused_bits) {
	SECU_Indent(out, level + 1);
	fprintf(out, "(%d least significant bits unused)\n", unused_bits);
    }
}

/* in a decoded bit string, the len member is a bit length. */
static void
secu_PrintDecodedBitString(FILE *out, SECItem *i, char *m, int level)
{
    int unused_bits;
    SECItem tmp = *i;


    unused_bits = (tmp.len & 0x7) ? 8 - (tmp.len & 7) : 0;
    DER_ConvertBitString(&tmp); /* convert length to byte length */

    SECU_PrintAsHex(out, &tmp, m, level);
    if (unused_bits) {
	SECU_Indent(out, level + 1);
	fprintf(out, "(%d least significant bits unused)\n", unused_bits);
    }
}


/* Print a DER encoded Boolean */
void
SECU_PrintEncodedBoolean(FILE *out, SECItem *i, char *m, int level)
{
    SECItem my    = *i;
    if (SECSuccess == SECU_StripTagAndLength(&my))
	secu_PrintBoolean(out, &my, m, level);
}

/* Print a DER encoded integer */
void
SECU_PrintEncodedInteger(FILE *out, SECItem *i, char *m, int level)
{
    SECItem my    = *i;
    if (SECSuccess == SECU_StripTagAndLength(&my))
	SECU_PrintInteger(out, &my, m, level);
}

/* Print a DER encoded OID */
void
SECU_PrintEncodedObjectID(FILE *out, SECItem *i, char *m, int level)
{
    SECItem my    = *i;
    if (SECSuccess == SECU_StripTagAndLength(&my))
	SECU_PrintObjectID(out, &my, m, level);
}

static void
secu_PrintBMPString(FILE *out, SECItem *i, char *m, int level)
{
    unsigned char * s;
    unsigned char * d;
    int      len;
    SECItem  tmp = {0, 0, 0};
    SECItem  my  = *i;

    if (SECSuccess != SECU_StripTagAndLength(&my))
	goto loser;
    if (my.len % 2) 
    	goto loser;
    len = (int)(my.len / 2);
    tmp.data = (unsigned char *)PORT_Alloc(len);
    if (!tmp.data)
    	goto loser;
    tmp.len = len;
    for (s = my.data, d = tmp.data ; len > 0; len--) {
    	PRUint32 bmpChar = (s[0] << 8) | s[1]; s += 2;
	if (!isprint(bmpChar))
	    goto loser;
	*d++ = (unsigned char)bmpChar;
    }
    secu_PrintRawString(out, &tmp, m, level);
    PORT_Free(tmp.data);
    return;

loser:
    SECU_PrintAsHex(out, i, m, level);
    if (tmp.data)
	PORT_Free(tmp.data);
}

static void
secu_PrintUniversalString(FILE *out, SECItem *i, char *m, int level)
{
    unsigned char * s;
    unsigned char * d;
    int      len;
    SECItem  tmp = {0, 0, 0};
    SECItem  my  = *i;

    if (SECSuccess != SECU_StripTagAndLength(&my))
	goto loser;
    if (my.len % 4) 
    	goto loser;
    len = (int)(my.len / 4);
    tmp.data = (unsigned char *)PORT_Alloc(len);
    if (!tmp.data)
    	goto loser;
    tmp.len = len;
    for (s = my.data, d = tmp.data ; len > 0; len--) {
    	PRUint32 bmpChar = (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
	s += 4;
	if (!isprint(bmpChar))
	    goto loser;
	*d++ = (unsigned char)bmpChar;
    }
    secu_PrintRawString(out, &tmp, m, level);
    PORT_Free(tmp.data);
    return;

loser:
    SECU_PrintAsHex(out, i, m, level);
    if (tmp.data)
	PORT_Free(tmp.data);
}

static void
secu_PrintUniversal(FILE *out, SECItem *i, char *m, int level)
{
	switch (i->data[0] & SEC_ASN1_TAGNUM_MASK) {
	  case SEC_ASN1_ENUMERATED:
	  case SEC_ASN1_INTEGER:
	    SECU_PrintEncodedInteger(out, i, m, level);
	    break;
	  case SEC_ASN1_OBJECT_ID:
	    SECU_PrintEncodedObjectID(out, i, m, level);
	    break;
	  case SEC_ASN1_BOOLEAN:
	    SECU_PrintEncodedBoolean(out, i, m, level);
	    break;
	  case SEC_ASN1_UTF8_STRING:
	  case SEC_ASN1_PRINTABLE_STRING:
	  case SEC_ASN1_VISIBLE_STRING:
	  case SEC_ASN1_IA5_STRING:
	  case SEC_ASN1_T61_STRING:
	    SECU_PrintString(out, i, m, level);
	    break;
	  case SEC_ASN1_GENERALIZED_TIME:
	    SECU_PrintGeneralizedTime(out, i, m, level);
	    break;
	  case SEC_ASN1_UTC_TIME:
	    SECU_PrintUTCTime(out, i, m, level);
	    break;
	  case SEC_ASN1_NULL:
	    SECU_Indent(out, level); 
	    if (m && m[0]) 
	      fprintf(out, "%s: NULL\n", m);
	    else
	      fprintf(out, "NULL\n");
	    break;
          case SEC_ASN1_SET:
          case SEC_ASN1_SEQUENCE:
	    SECU_PrintSet(out, i, m, level);
	    break;
	  case SEC_ASN1_OCTET_STRING:
	    secu_PrintOctetString(out, i, m, level);
	    break;
	  case SEC_ASN1_BIT_STRING:
	    secu_PrintBitString(out, i, m, level);
	    break;
	  case SEC_ASN1_BMP_STRING:
	    secu_PrintBMPString(out, i, m, level);
	    break;
	  case SEC_ASN1_UNIVERSAL_STRING:
	    secu_PrintUniversalString(out, i, m, level);
	    break;
	  default:
	    SECU_PrintAsHex(out, i, m, level);
	    break;
	}
}

void
SECU_PrintAny(FILE *out, SECItem *i, char *m, int level)
{
    if ( i && i->len && i->data ) {
	switch (i->data[0] & SEC_ASN1_CLASS_MASK) {
	case SEC_ASN1_CONTEXT_SPECIFIC:
	    secu_PrintContextSpecific(out, i, m, level);
	    break;
	case SEC_ASN1_UNIVERSAL:
	    secu_PrintUniversal(out, i, m, level);
	    break;
	default:
	    SECU_PrintAsHex(out, i, m, level);
	    break;
	}
    }
}

static int
secu_PrintValidity(FILE *out, CERTValidity *v, char *m, int level)
{
    SECU_Indent(out, level);  fprintf(out, "%s:\n", m);
    SECU_PrintTimeChoice(out, &v->notBefore, "Not Before", level+1);
    SECU_PrintTimeChoice(out, &v->notAfter,  "Not After ", level+1);
    return 0;
}

/* This function does NOT expect a DER type and length. */
SECOidTag
SECU_PrintObjectID(FILE *out, SECItem *oid, char *m, int level)
{
    SECOidData *oiddata;
    char *      oidString = NULL;
    
    oiddata = SECOID_FindOID(oid);
    if (oiddata != NULL) {
	const char *name = oiddata->desc;
	SECU_Indent(out, level);
	if (m != NULL)
	    fprintf(out, "%s: ", m);
	fprintf(out, "%s\n", name);
	return oiddata->offset;
    } 
    oidString = CERT_GetOidString(oid);
    if (oidString) {
	SECU_Indent(out, level);
	if (m != NULL)
	    fprintf(out, "%s: ", m);
	fprintf(out, "%s\n", oidString);
	PR_smprintf_free(oidString);
	return SEC_OID_UNKNOWN;
    }
    SECU_PrintAsHex(out, oid, m, level);
    return SEC_OID_UNKNOWN;
}

typedef struct secuPBEParamsStr {
    SECItem salt;
    SECItem iterationCount;
    SECItem keyLength;
    SECAlgorithmID cipherAlg;
    SECAlgorithmID kdfAlg;
} secuPBEParams;

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate);

/* SECOID_PKCS5_PBKDF2 */
const SEC_ASN1Template secuKDF2Params[] =
{
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(secuPBEParams) },
    { SEC_ASN1_OCTET_STRING, offsetof(secuPBEParams, salt) },
    { SEC_ASN1_INTEGER, offsetof(secuPBEParams, iterationCount) },
    { SEC_ASN1_INTEGER, offsetof(secuPBEParams, keyLength) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(secuPBEParams, kdfAlg),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { 0 }
};

/* PKCS5v1 & PKCS12 */
const SEC_ASN1Template secuPBEParamsTemp[] =
{
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(secuPBEParams) },
    { SEC_ASN1_OCTET_STRING, offsetof(secuPBEParams, salt) },
    { SEC_ASN1_INTEGER, offsetof(secuPBEParams, iterationCount) },
    { 0 }
};

/* SEC_OID_PKCS5_PBES2, SEC_OID_PKCS5_PBMAC1 */
const SEC_ASN1Template secuPBEV2Params[] =
{
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(secuPBEParams)},
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(secuPBEParams, kdfAlg),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(secuPBEParams, cipherAlg),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { 0 }
};

void
secu_PrintRSAPSSParams(FILE *out, SECItem *value, char *m, int level)
{
    PRArenaPool *pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SECStatus rv;
    SECKEYRSAPSSParams param;
    SECAlgorithmID maskHashAlg;

    if (m) {
	SECU_Indent(out, level);
	fprintf (out, "%s:\n", m);
    }

    if (!pool) {
	SECU_Indent(out, level);
	fprintf(out, "Out of memory\n");
	return;
    }

    PORT_Memset(&param, 0, sizeof param);

    rv = SEC_QuickDERDecodeItem(pool, &param,
				SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate),
				value);
    if (rv == SECSuccess) {
	if (!param.hashAlg) {
	    SECU_Indent(out, level+1);
	    fprintf(out, "Hash algorithm: default, SHA-1\n");
	} else {
	    SECU_PrintObjectID(out, &param.hashAlg->algorithm,
			       "Hash algorithm", level+1);
	}
	if (!param.maskAlg) {
	    SECU_Indent(out, level+1);
	    fprintf(out, "Mask algorithm: default, MGF1\n");
	    SECU_Indent(out, level+1);
	    fprintf(out, "Mask hash algorithm: default, SHA-1\n");
	} else {
	    SECU_PrintObjectID(out, &param.maskAlg->algorithm,
			       "Mask algorithm", level+1);
	    rv = SEC_QuickDERDecodeItem(pool, &maskHashAlg,
		     SEC_ASN1_GET(SECOID_AlgorithmIDTemplate),
		     &param.maskAlg->parameters);
	    if (rv == SECSuccess) {
		SECU_PrintObjectID(out, &maskHashAlg.algorithm,
				   "Mask hash algorithm", level+1);
	    } else {
		SECU_Indent(out, level+1);
		fprintf(out, "Invalid mask generation algorithm parameters\n");
	    }
	}
	if (!param.saltLength.data) {
	    SECU_Indent(out, level+1);
	    fprintf(out, "Salt length: default, %i (0x%2X)\n", 20, 20);
	} else {
	    SECU_PrintInteger(out, &param.saltLength, "Salt Length", level+1);
	}
    } else {
	SECU_Indent(out, level+1);
	fprintf(out, "Invalid RSA-PSS parameters\n");
    }
    PORT_FreeArena(pool, PR_FALSE);
}

void
secu_PrintKDF2Params(FILE *out, SECItem *value, char *m, int level)
{
    PRArenaPool *pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SECStatus rv;
    secuPBEParams param;

    if (m) {
	SECU_Indent(out, level);
	fprintf (out, "%s:\n", m);
    }

    if (!pool) {
	SECU_Indent(out, level);
	fprintf(out, "Out of memory\n");
	return;
    }

    PORT_Memset(&param, 0, sizeof param);
    rv = SEC_QuickDERDecodeItem(pool, &param, secuKDF2Params, value);
    if (rv == SECSuccess) {
	SECU_PrintAsHex(out, &param.salt, "Salt", level+1);
	SECU_PrintInteger(out, &param.iterationCount, "Iteration Count", 
			level+1);
	SECU_PrintInteger(out, &param.keyLength, "Key Length", level+1);
	SECU_PrintAlgorithmID(out, &param.kdfAlg, "KDF algorithm", level+1);
    }
    PORT_FreeArena(pool, PR_FALSE);
}

void
secu_PrintPKCS5V2Params(FILE *out, SECItem *value, char *m, int level)
{
    PRArenaPool *pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SECStatus rv;
    secuPBEParams param;

    if (m) {
	SECU_Indent(out, level);
	fprintf (out, "%s:\n", m);
    }

    if (!pool) {
	SECU_Indent(out, level);
	fprintf(out, "Out of memory\n");
	return;
    }

    PORT_Memset(&param, 0, sizeof param);
    rv = SEC_QuickDERDecodeItem(pool, &param, secuPBEV2Params, value);
    if (rv == SECSuccess) {
	SECU_PrintAlgorithmID(out, &param.kdfAlg, "KDF", level+1);
	SECU_PrintAlgorithmID(out, &param.cipherAlg, "Cipher", level+1);
    }
    PORT_FreeArena(pool, PR_FALSE);
}

void
secu_PrintPBEParams(FILE *out, SECItem *value, char *m, int level)
{
    PRArenaPool *pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SECStatus rv;
    secuPBEParams param;

    if (m) {
	SECU_Indent(out, level);
	fprintf (out, "%s:\n", m);
    }

    if (!pool) {
	SECU_Indent(out, level);
	fprintf(out, "Out of memory\n");
	return;
    }

    PORT_Memset(&param, 0, sizeof(secuPBEParams));
    rv = SEC_QuickDERDecodeItem(pool, &param, secuPBEParamsTemp, value);
    if (rv == SECSuccess) {
	SECU_PrintAsHex(out, &param.salt, "Salt", level+1);
	SECU_PrintInteger(out, &param.iterationCount, "Iteration Count", 
			level+1);
    }
    PORT_FreeArena(pool, PR_FALSE);
}

/* This function does NOT expect a DER type and length. */
void
SECU_PrintAlgorithmID(FILE *out, SECAlgorithmID *a, char *m, int level)
{
    SECOidTag algtag;
    SECU_PrintObjectID(out, &a->algorithm, m, level);

    algtag = SECOID_GetAlgorithmTag(a);
    if (SEC_PKCS5IsAlgorithmPBEAlgTag(algtag)) {
	switch (algtag) {
	case SEC_OID_PKCS5_PBKDF2:
	    secu_PrintKDF2Params(out, &a->parameters, "Parameters", level+1);
	    break;
	case SEC_OID_PKCS5_PBES2:
	    secu_PrintPKCS5V2Params(out, &a->parameters, "Encryption", level+1);
	    break;
	case SEC_OID_PKCS5_PBMAC1:
	    secu_PrintPKCS5V2Params(out, &a->parameters, "MAC", level+1);
	    break;
	default:
	    secu_PrintPBEParams(out, &a->parameters, "Parameters", level+1);
	    break;
	}
	return;
    }

    if (algtag == SEC_OID_PKCS1_RSA_PSS_SIGNATURE) {
	secu_PrintRSAPSSParams(out, &a->parameters, "Parameters", level+1);
	return;
    }

    if (a->parameters.len == 0
	|| (a->parameters.len == 2
	    && PORT_Memcmp(a->parameters.data, "\005\000", 2) == 0)) {
	/* No arguments or NULL argument */
    } else {
	/* Print args to algorithm */
	SECU_PrintAsHex(out, &a->parameters, "Args", level+1);
    }
}

static void
secu_PrintAttribute(FILE *out, SEC_PKCS7Attribute *attr, char *m, int level)
{
    SECItem *value;
    int i;
    char om[100];

    if (m) {
    	SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    }

    /*
     * Should make this smarter; look at the type field and then decode
     * and print the value(s) appropriately!
     */
    SECU_PrintObjectID(out, &(attr->type), "Type", level+1);
    if (attr->values != NULL) {
	i = 0;
	while ((value = attr->values[i++]) != NULL) {
	    sprintf(om, "Value (%d)%s", i, attr->encoded ? " (encoded)" : ""); 
	    if (attr->encoded || attr->typeTag == NULL) {
		SECU_PrintAny(out, value, om, level+1);
	    } else {
		switch (attr->typeTag->offset) {
		  default:
		    SECU_PrintAsHex(out, value, om, level+1);
		    break;
		  case SEC_OID_PKCS9_CONTENT_TYPE:
		    SECU_PrintObjectID(out, value, om, level+1);
		    break;
		  case SEC_OID_PKCS9_SIGNING_TIME:
		    SECU_PrintTimeChoice(out, value, om, level+1);
		    break;
		}
	    }
	}
    }
}

static void
secu_PrintRSAPublicKey(FILE *out, SECKEYPublicKey *pk, char *m, int level)
{

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &pk->u.rsa.modulus, "Modulus", level+1);
    SECU_PrintInteger(out, &pk->u.rsa.publicExponent, "Exponent", level+1);
    if (pk->u.rsa.publicExponent.len == 1 &&
        pk->u.rsa.publicExponent.data[0] == 1) {
	SECU_Indent(out, level +1); fprintf(out, "Error: INVALID RSA KEY!\n");
    }
}

static void
secu_PrintDSAPublicKey(FILE *out, SECKEYPublicKey *pk, char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &pk->u.dsa.params.prime, "Prime", level+1);
    SECU_PrintInteger(out, &pk->u.dsa.params.subPrime, "Subprime", level+1);
    SECU_PrintInteger(out, &pk->u.dsa.params.base, "Base", level+1);
    SECU_PrintInteger(out, &pk->u.dsa.publicValue, "PublicValue", level+1);
}

#ifdef NSS_ENABLE_ECC
static void
secu_PrintECPublicKey(FILE *out, SECKEYPublicKey *pk, char *m, int level)
{
    SECItem curveOID = { siBuffer, NULL, 0};

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &pk->u.ec.publicValue, "PublicValue", level+1);
    /* For named curves, the DEREncodedParams field contains an
     * ASN Object ID (0x06 is SEC_ASN1_OBJECT_ID).
     */
    if ((pk->u.ec.DEREncodedParams.len > 2) &&
	(pk->u.ec.DEREncodedParams.data[0] == 0x06)) {
        curveOID.len = pk->u.ec.DEREncodedParams.data[1];
	curveOID.data = pk->u.ec.DEREncodedParams.data + 2;
	SECU_PrintObjectID(out, &curveOID, "Curve", level +1);
    }
}
#endif /* NSS_ENABLE_ECC */

static void
secu_PrintSubjectPublicKeyInfo(FILE *out, PRArenaPool *arena,
		       CERTSubjectPublicKeyInfo *i,  char *msg, int level)
{
    SECKEYPublicKey *pk;

    SECU_Indent(out, level); fprintf(out, "%s:\n", msg);
    SECU_PrintAlgorithmID(out, &i->algorithm, "Public Key Algorithm", level+1);

    pk = SECKEY_ExtractPublicKey(i);
    if (pk) {
	switch (pk->keyType) {
	case rsaKey:
	    secu_PrintRSAPublicKey(out, pk, "RSA Public Key", level +1);
	    break;

	case dsaKey:
	    secu_PrintDSAPublicKey(out, pk, "DSA Public Key", level +1);
	    break;

#ifdef NSS_ENABLE_ECC
	case ecKey:
	    secu_PrintECPublicKey(out, pk, "EC Public Key", level +1);
	    break;
#endif

	case dhKey:
	case fortezzaKey:
	case keaKey:
	    SECU_Indent(out, level);
    	    fprintf(out, "unable to format this SPKI algorithm type\n");
	    goto loser;
	default:
	    SECU_Indent(out, level);
	    fprintf(out, "unknown SPKI algorithm type\n");
	    goto loser;
	}
	PORT_FreeArena(pk->arena, PR_FALSE);
    } else {
	SECU_PrintErrMsg(out, level, "Error", "Parsing public key");
loser:
	if (i->subjectPublicKey.data) {
	    SECU_PrintAny(out, &i->subjectPublicKey, "Raw", level);
	}
    }
}

static SECStatus
secu_PrintX509InvalidDate(FILE *out, SECItem *value, char *msg, int level)
{
    SECItem decodedValue;
    SECStatus rv;
    int64 invalidTime;
    char *formattedTime = NULL;

    decodedValue.data = NULL;
    rv = SEC_ASN1DecodeItem (NULL, &decodedValue, 
			    SEC_ASN1_GET(SEC_GeneralizedTimeTemplate),
			    value);
    if (rv == SECSuccess) {
	rv = DER_GeneralizedTimeToTime(&invalidTime, &decodedValue);
	if (rv == SECSuccess) {
	    formattedTime = CERT_GenTime2FormattedAscii
			    (invalidTime, "%a %b %d %H:%M:%S %Y");
	    SECU_Indent(out, level +1);
	    fprintf (out, "%s: %s\n", msg, formattedTime);
	    PORT_Free (formattedTime);
	}
    }
    PORT_Free (decodedValue.data);
    return (rv);
}

static SECStatus
PrintExtKeyUsageExtension  (FILE *out, SECItem *value, char *msg, int level)
{
    CERTOidSequence *os;
    SECItem **op;

    os = CERT_DecodeOidSequence(value);
    if( (CERTOidSequence *)NULL == os ) {
	return SECFailure;
    }

    for( op = os->oids; *op; op++ ) {
	SECU_PrintObjectID(out, *op, msg, level + 1);
    }
    CERT_DestroyOidSequence(os);
    return SECSuccess;
}

static SECStatus
secu_PrintBasicConstraints(FILE *out, SECItem *value, char *msg, int level) {
    CERTBasicConstraints constraints;
    SECStatus rv;

    SECU_Indent(out, level);
    if (msg) {
	    fprintf(out,"%s: ",msg);
    } 
    rv = CERT_DecodeBasicConstraintValue(&constraints,value);
    if (rv == SECSuccess && constraints.isCA) {
	if (constraints.pathLenConstraint >= 0) {
	    fprintf(out,"Is a CA with a maximum path length of %d.\n",
			constraints.pathLenConstraint);
    	} else {
	    fprintf(out,"Is a CA with no maximum path length.\n");
	}
    } else  {
	fprintf(out,"Is not a CA.\n");
    }
    return SECSuccess;
}

static const char * const nsTypeBits[] = {
    "SSL Client",
    "SSL Server",
    "S/MIME",
    "Object Signing",
    "Reserved",
    "SSL CA",
    "S/MIME CA",
    "ObjectSigning CA" 
};

/* NSCertType is merely a bit string whose bits are displayed symbolically */
static SECStatus
secu_PrintNSCertType(FILE *out, SECItem *value, char *msg, int level) 
{
    int     unused;
    int     NS_Type;
    int     i;
    int     found   = 0;
    SECItem my      = *value;

    if ((my.data[0] != SEC_ASN1_BIT_STRING) || 
        SECSuccess != SECU_StripTagAndLength(&my)) {
	SECU_PrintAny(out, value, "Data", level);
	return SECSuccess;
    }

    unused = (my.len == 2) ? (my.data[0] & 0x0f) : 0;  
    NS_Type = my.data[1] & (0xff << unused);
	

    SECU_Indent(out, level);
    if (msg) {
	fprintf(out,"%s: ",msg);
    } else {
	fprintf(out,"Netscape Certificate Type: ");
    }
    for (i=0; i < 8; i++) {
	if ( (0x80 >> i) & NS_Type) {
	    fprintf(out, "%c%s", (found ? ',' : '<'), nsTypeBits[i]);
	    found = 1;
	}
    }
    fprintf(out, (found ? ">\n" : "none\n"));
    return SECSuccess;
}

static const char * const usageBits[] = {
    "Digital Signature",   /* 0x80 */
    "Non-Repudiation",     /* 0x40 */
    "Key Encipherment",    /* 0x20 */
    "Data Encipherment",   /* 0x10 */
    "Key Agreement",       /* 0x08 */
    "Certificate Signing", /* 0x04 */
    "CRL Signing",         /* 0x02 */
    "Encipher Only",       /* 0x01 */
    "Decipher Only",       /* 0x0080 */ 
    NULL
};

/* X509KeyUsage is merely a bit string whose bits are displayed symbolically */
static void
secu_PrintX509KeyUsage(FILE *out, SECItem *value, char *msg, int level) 
{
    int     unused;
    int     usage;
    int     i;
    int     found   = 0;
    SECItem my      = *value;

    if ((my.data[0] != SEC_ASN1_BIT_STRING) || 
        SECSuccess != SECU_StripTagAndLength(&my)) {
	SECU_PrintAny(out, value, "Data", level);
	return;
    }

    unused = (my.len >= 2) ? (my.data[0] & 0x0f) : 0;  
    usage  = (my.len == 2) ? (my.data[1] & (0xff << unused)) << 8
                           : (my.data[1] << 8) | 
			     (my.data[2] & (0xff << unused));

    SECU_Indent(out, level);
    fprintf(out, "Usages: ");
    for (i=0; usageBits[i]; i++) {
	if ( (0x8000 >> i) & usage) {
	    if (found)
		SECU_Indent(out, level + 2);
	    fprintf(out, "%s\n", usageBits[i]);
	    found = 1;
	}
    }
    if (!found) {
	fprintf(out, "(none)\n");
    }
}

static void
secu_PrintIPAddress(FILE *out, SECItem *value, char *msg, int level)
{
    PRStatus   st;
    PRNetAddr  addr;
    char       addrBuf[80];

    memset(&addr, 0, sizeof addr);
    if (value->len == 4) {
	addr.inet.family = PR_AF_INET;
	memcpy(&addr.inet.ip, value->data, value->len);
    } else if (value->len == 16) {
	addr.ipv6.family = PR_AF_INET6;
	memcpy(addr.ipv6.ip.pr_s6_addr, value->data, value->len);
	if (PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped)) {
	    /* convert to IPv4.  */
	    addr.inet.family = PR_AF_INET;
	    memcpy(&addr.inet.ip, &addr.ipv6.ip.pr_s6_addr[12], 4);
	    memset(&addr.inet.pad[0], 0, sizeof addr.inet.pad);
	}
    } else {
	goto loser;
    }

    st = PR_NetAddrToString(&addr, addrBuf, sizeof addrBuf);
    if (st == PR_SUCCESS) {
	SECU_Indent(out, level);
	fprintf(out, "%s: %s\n", msg, addrBuf);
    } else {
loser:
	SECU_PrintAsHex(out, value, msg, level);
    }
}


static void
secu_PrintGeneralName(FILE *out, CERTGeneralName *gname, char *msg, int level) 
{
    char label[40];
    if (msg && msg[0]) {
    	SECU_Indent(out, level++); fprintf(out, "%s: \n", msg);
    }
    switch (gname->type) {
    case certOtherName :
	SECU_PrintAny(     out, &gname->name.OthName.name, "Other Name", level);
	SECU_PrintObjectID(out, &gname->name.OthName.oid,  "OID",      level+1);
	break;
    case certDirectoryName :
	SECU_PrintName(out, &gname->name.directoryName, "Directory Name", level);
	break;
    case certRFC822Name :
	secu_PrintRawString(   out, &gname->name.other, "RFC822 Name", level);
	break;
    case certDNSName :
	secu_PrintRawString(   out, &gname->name.other, "DNS name", level);
	break;
    case certURI :
	secu_PrintRawString(   out, &gname->name.other, "URI", level);
	break;
    case certIPAddress :
	secu_PrintIPAddress(out, &gname->name.other, "IP Address", level);
	break;
    case certRegisterID :
	SECU_PrintObjectID( out, &gname->name.other, "Registered ID", level);
	break;
    case certX400Address :
	SECU_PrintAny(      out, &gname->name.other, "X400 Address", level);
	break;
    case certEDIPartyName :
	SECU_PrintAny(      out, &gname->name.other, "EDI Party", level);
	break;
    default:
	PR_snprintf(label, sizeof label, "unknown type [%d]", 
	                                (int)gname->type - 1);
	SECU_PrintAsHex(out, &gname->name.other, label, level);
	break;
    }
}

static void
secu_PrintGeneralNames(FILE *out, CERTGeneralName *gname, char *msg, int level) 
{
    CERTGeneralName *name = gname;
    do { 
    	secu_PrintGeneralName(out, name, msg, level);
	name = CERT_GetNextGeneralName(name);
    } while (name && name != gname);
}


static void
secu_PrintAuthKeyIDExtension(FILE *out, SECItem *value, char *msg, int level) 
{
    CERTAuthKeyID *kid  = NULL;
    PLArenaPool   *pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if (!pool) {
	SECU_PrintError("Error", "Allocating new ArenaPool");
	return;
    }
    kid = CERT_DecodeAuthKeyID(pool, value);
    if (!kid) {
	SECU_PrintErrMsg(out, level, "Error", "Parsing extension");
	SECU_PrintAny(out, value, "Data", level);
    } else {
	int keyIDPresent  = (kid->keyID.data && kid->keyID.len);
	int issuerPresent = kid->authCertIssuer != NULL;
	int snPresent = (kid->authCertSerialNumber.data &&
	                 kid->authCertSerialNumber.len);

	if (keyIDPresent)
	    SECU_PrintAsHex(out, &kid->keyID, "Key ID", level);
	if (issuerPresent)
	    secu_PrintGeneralName(out, kid->authCertIssuer, "Issuer", level);
	if (snPresent)
	    SECU_PrintInteger(out, &kid->authCertSerialNumber, 
	                    "Serial Number", level);
    }
    PORT_FreeArena(pool, PR_FALSE);
}


static void
secu_PrintAltNameExtension(FILE *out, SECItem *value, char *msg, int level)
{
    CERTGeneralName * nameList;
    CERTGeneralName * current;
    PLArenaPool     * pool      = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if (!pool) {
	SECU_PrintError("Error", "Allocating new ArenaPool");
	return;
    }
    nameList = current = CERT_DecodeAltNameExtension(pool, value);
    if (!current) {
	if (PORT_GetError() == SEC_ERROR_EXTENSION_NOT_FOUND) {
	    /* Decoder found empty sequence, which is invalid. */
	    PORT_SetError(SEC_ERROR_EXTENSION_VALUE_INVALID);
	}
	SECU_PrintErrMsg(out, level, "Error", "Parsing extension");
	SECU_PrintAny(out, value, "Data", level);
    } else {
	do {
	    secu_PrintGeneralName(out, current, msg, level);
	    current = CERT_GetNextGeneralName(current);
	} while (current != nameList);
    }
    PORT_FreeArena(pool, PR_FALSE);
}

static void
secu_PrintCRLDistPtsExtension(FILE *out, SECItem *value, char *msg, int level)
{
    CERTCrlDistributionPoints * dPoints;
    PLArenaPool * pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if (!pool) {
	SECU_PrintError("Error", "Allocating new ArenaPool");
	return;
    }
    dPoints = CERT_DecodeCRLDistributionPoints(pool, value);
    if (dPoints && dPoints->distPoints && dPoints->distPoints[0]) {
	CRLDistributionPoint ** pPoints = dPoints->distPoints;
	CRLDistributionPoint *  pPoint;
	while (NULL != (pPoint = *pPoints++)) {
	    SECU_Indent(out, level); fputs("Distribution point:\n", out);
	    if (pPoint->distPointType == generalName && 
	        pPoint->distPoint.fullName != NULL) {
		secu_PrintGeneralNames(out, pPoint->distPoint.fullName, NULL,
		                       level + 1);
	    } else if (pPoint->distPointType == relativeDistinguishedName &&
	               pPoint->distPoint.relativeName.avas) {
		SECU_PrintRDN(out, &pPoint->distPoint.relativeName, "RDN", 
		              level + 1);
	    } else if (pPoint->derDistPoint.data) {
		SECU_PrintAny(out, &pPoint->derDistPoint, "Point", level + 1);
	    }
	    if (pPoint->reasons.data) {
		secu_PrintDecodedBitString(out, &pPoint->reasons, "Reasons", 
		                           level + 1);
	    }
	    if (pPoint->crlIssuer) {
		secu_PrintGeneralName(out, pPoint->crlIssuer, "CRL issuer",
				      level + 1);
	    }
	}
    } else {
	SECU_PrintErrMsg(out, level, "Error", "Parsing extension");
	SECU_PrintAny(out, value, "Data", level);
    }
    PORT_FreeArena(pool, PR_FALSE);
}


static void
secu_PrintNameConstraintSubtree(FILE *out, CERTNameConstraint *value, 
                                char *msg, int level)
{
    CERTNameConstraint *head = value;
    SECU_Indent(out, level); fprintf(out, "%s Subtree:\n", msg);
    level++;
    do {
	secu_PrintGeneralName(out, &value->name, NULL, level);
	if (value->min.data)
	    SECU_PrintInteger(out, &value->min, "Minimum", level+1);
	if (value->max.data)
	    SECU_PrintInteger(out, &value->max, "Maximum", level+1);
	value = CERT_GetNextNameConstraint(value);
    } while (value != head);
}

static void
secu_PrintNameConstraintsExtension(FILE *out, SECItem *value, char *msg, int level)
{
    CERTNameConstraints * cnstrnts;
    PLArenaPool * pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if (!pool) {
	SECU_PrintError("Error", "Allocating new ArenaPool");
	return;
    }
    cnstrnts = CERT_DecodeNameConstraintsExtension(pool, value);
    if (!cnstrnts) {
	SECU_PrintErrMsg(out, level, "Error", "Parsing extension");
    	SECU_PrintAny(out, value, "Raw", level);
    } else {
	if (cnstrnts->permited)
	    secu_PrintNameConstraintSubtree(out, cnstrnts->permited, 
	                                    "Permitted", level);
	if (cnstrnts->excluded)
	    secu_PrintNameConstraintSubtree(out, cnstrnts->excluded, 
	                                    "Excluded", level);
    }
    PORT_FreeArena(pool, PR_FALSE);
}


static void
secu_PrintAuthorityInfoAcess(FILE *out, SECItem *value, char *msg, int level)
{
    CERTAuthInfoAccess **infos = NULL;
    PLArenaPool * pool = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);

    if (!pool) {
	SECU_PrintError("Error", "Allocating new ArenaPool");
	return;
    }
    infos = CERT_DecodeAuthInfoAccessExtension(pool, value);
    if (!infos) {
	SECU_PrintErrMsg(out, level, "Error", "Parsing extension");
    	SECU_PrintAny(out, value, "Raw", level);
    } else {
	CERTAuthInfoAccess *info;
	while (NULL != (info = *infos++)) {
	    if (info->method.data) {
		SECU_PrintObjectID(out, &info->method, "Method", level);
	    } else {
	    	SECU_Indent(out,level);
		fprintf(out, "Error: missing method\n");
	    }
	    if (info->location) {
		secu_PrintGeneralName(out, info->location, "Location", level);
	    } else {
		SECU_PrintAny(out, &info->derLocation, "Location", level);
	    }
	}
    }
    PORT_FreeArena(pool, PR_FALSE);
}


void
SECU_PrintExtensions(FILE *out, CERTCertExtension **extensions,
		     char *msg, int level)
{
    SECOidTag oidTag;
    
    if ( extensions ) {
	if (msg && *msg) {
	    SECU_Indent(out, level++); fprintf(out, "%s:\n", msg);
	}
	
	while ( *extensions ) {
	    SECItem *tmpitem;

	    tmpitem = &(*extensions)->id;
	    SECU_PrintObjectID(out, tmpitem, "Name", level);

	    tmpitem = &(*extensions)->critical;
	    if ( tmpitem->len ) {
		secu_PrintBoolean(out, tmpitem, "Critical", level);
	    }

	    oidTag = SECOID_FindOIDTag (&((*extensions)->id));
	    tmpitem = &((*extensions)->value);

	    switch (oidTag) {
	      	case SEC_OID_X509_INVALID_DATE:
		case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME:
		   secu_PrintX509InvalidDate(out, tmpitem, "Date", level );
		   break;
		case SEC_OID_X509_CERTIFICATE_POLICIES:
		   SECU_PrintPolicy(out, tmpitem, "Data", level );
		   break;
		case SEC_OID_NS_CERT_EXT_BASE_URL:
		case SEC_OID_NS_CERT_EXT_REVOCATION_URL:
		case SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL:
		case SEC_OID_NS_CERT_EXT_CA_CRL_URL:
		case SEC_OID_NS_CERT_EXT_CA_CERT_URL:
		case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL:
		case SEC_OID_NS_CERT_EXT_CA_POLICY_URL:
		case SEC_OID_NS_CERT_EXT_HOMEPAGE_URL:
		case SEC_OID_NS_CERT_EXT_LOST_PASSWORD_URL:
		case SEC_OID_OCSP_RESPONDER:
		    SECU_PrintString(out,tmpitem, "URL", level);
		    break;
		case SEC_OID_NS_CERT_EXT_COMMENT:
		    SECU_PrintString(out,tmpitem, "Comment", level);
		    break;
		case SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME:
		    SECU_PrintString(out,tmpitem, "ServerName", level);
		    break;
		case SEC_OID_NS_CERT_EXT_CERT_TYPE:
		    secu_PrintNSCertType(out,tmpitem,"Data",level);
		    break;
		case SEC_OID_X509_BASIC_CONSTRAINTS:
		    secu_PrintBasicConstraints(out,tmpitem,"Data",level);
		    break;
		case SEC_OID_X509_EXT_KEY_USAGE:
		    PrintExtKeyUsageExtension(out, tmpitem, NULL, level);
		    break;
		case SEC_OID_X509_KEY_USAGE:
		    secu_PrintX509KeyUsage(out, tmpitem, NULL, level );
		    break;
		case SEC_OID_X509_AUTH_KEY_ID:
		    secu_PrintAuthKeyIDExtension(out, tmpitem, NULL, level );
		    break;
		case SEC_OID_X509_SUBJECT_ALT_NAME:
		case SEC_OID_X509_ISSUER_ALT_NAME:
		    secu_PrintAltNameExtension(out, tmpitem, NULL, level );
		    break;
		case SEC_OID_X509_CRL_DIST_POINTS:
		    secu_PrintCRLDistPtsExtension(out, tmpitem, NULL, level );
		    break;
		case SEC_OID_X509_PRIVATE_KEY_USAGE_PERIOD:
		    SECU_PrintPrivKeyUsagePeriodExtension(out, tmpitem, NULL, 
							level );
		    break;
		case SEC_OID_X509_NAME_CONSTRAINTS:
		    secu_PrintNameConstraintsExtension(out, tmpitem, NULL, level);
		    break;
		case SEC_OID_X509_AUTH_INFO_ACCESS:
		    secu_PrintAuthorityInfoAcess(out, tmpitem, NULL, level);
		    break;

		case SEC_OID_X509_CRL_NUMBER:
		case SEC_OID_X509_REASON_CODE:

		/* PKIX OIDs */
		case SEC_OID_PKIX_OCSP:
		case SEC_OID_PKIX_OCSP_BASIC_RESPONSE:
		case SEC_OID_PKIX_OCSP_NONCE:
		case SEC_OID_PKIX_OCSP_CRL:
		case SEC_OID_PKIX_OCSP_RESPONSE:
		case SEC_OID_PKIX_OCSP_NO_CHECK:
		case SEC_OID_PKIX_OCSP_ARCHIVE_CUTOFF:
		case SEC_OID_PKIX_OCSP_SERVICE_LOCATOR:
		case SEC_OID_PKIX_REGCTRL_REGTOKEN:
		case SEC_OID_PKIX_REGCTRL_AUTHENTICATOR:
		case SEC_OID_PKIX_REGCTRL_PKIPUBINFO:
		case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
		case SEC_OID_PKIX_REGCTRL_OLD_CERT_ID:
		case SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY:
		case SEC_OID_PKIX_REGINFO_UTF8_PAIRS:
		case SEC_OID_PKIX_REGINFO_CERT_REQUEST:

	        /* Netscape extension OIDs. */
		case SEC_OID_NS_CERT_EXT_NETSCAPE_OK:
		case SEC_OID_NS_CERT_EXT_ISSUER_LOGO:
		case SEC_OID_NS_CERT_EXT_SUBJECT_LOGO:
		case SEC_OID_NS_CERT_EXT_ENTITY_LOGO:
		case SEC_OID_NS_CERT_EXT_USER_PICTURE:

		/* x.509 v3 Extensions */
		case SEC_OID_X509_SUBJECT_DIRECTORY_ATTR:
		case SEC_OID_X509_SUBJECT_KEY_ID:
		case SEC_OID_X509_POLICY_MAPPINGS:
		case SEC_OID_X509_POLICY_CONSTRAINTS:


	        default:
		    SECU_PrintAny(out, tmpitem, "Data", level);
		break;
	    }

	    secu_Newline(out);
	    extensions++;
	}
    }
}

/* An RDN is a subset of a DirectoryName, and we already know how to
 * print those, so make a directory name out of the RDN, and print it.
 */
void
SECU_PrintRDN(FILE *out, CERTRDN *rdn, const char *msg, int level)
{
    CERTName name;
    CERTRDN *rdns[2];

    name.arena = NULL;
    name.rdns  = rdns;
    rdns[0] = rdn;
    rdns[1] = NULL;
    SECU_PrintName(out, &name, msg, level);
}

void
SECU_PrintName(FILE *out, CERTName *name, const char *msg, int level)
{
    char *nameStr = NULL;
    char *str;
    SECItem my;

    if (!name) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return;
    }
    if (!name->rdns || !name->rdns[0]) {
	str = "(empty)";
    } else {
	str = nameStr = CERT_NameToAscii(name);
    }
    if (!str) {
    	str = "!Invalid AVA!";
    }
    my.data = (unsigned char *)str;
    my.len  = PORT_Strlen(str);
#if 1
    secu_PrintRawString(out, &my, msg, level);
#else
    SECU_Indent(out, level); fprintf(out, "%s: ", msg);
    fprintf(out, str);
    secu_Newline(out);
#endif
    PORT_Free(nameStr);
}

void
printflags(char *trusts, unsigned int flags)
{
    if (flags & CERTDB_VALID_CA)
	if (!(flags & CERTDB_TRUSTED_CA) &&
	    !(flags & CERTDB_TRUSTED_CLIENT_CA))
	    PORT_Strcat(trusts, "c");
    if (flags & CERTDB_TERMINAL_RECORD)
	if (!(flags & CERTDB_TRUSTED))
	    PORT_Strcat(trusts, "p");
    if (flags & CERTDB_TRUSTED_CA)
	PORT_Strcat(trusts, "C");
    if (flags & CERTDB_TRUSTED_CLIENT_CA)
	PORT_Strcat(trusts, "T");
    if (flags & CERTDB_TRUSTED)
	PORT_Strcat(trusts, "P");
    if (flags & CERTDB_USER)
	PORT_Strcat(trusts, "u");
    if (flags & CERTDB_SEND_WARN)
	PORT_Strcat(trusts, "w");
    if (flags & CERTDB_INVISIBLE_CA)
	PORT_Strcat(trusts, "I");
    if (flags & CERTDB_GOVT_APPROVED_CA)
	PORT_Strcat(trusts, "G");
    return;
}

/* callback for listing certs through pkcs11 */
SECStatus
SECU_PrintCertNickname(CERTCertListNode *node, void *data)
{
    CERTCertTrust *trust;
    CERTCertificate* cert;
    FILE *out;
    char trusts[30];
    char *name;

    cert = node->cert;

    PORT_Memset (trusts, 0, sizeof (trusts));
    out = (FILE *)data;
    
    name = node->appData;
    if (!name || !name[0]) {
        name = cert->nickname;
    }
    if (!name || !name[0]) {
        name = cert->emailAddr;
    }
    if (!name || !name[0]) {
        name = "(NULL)";
    }

    trust = cert->trust;
    if (trust) {
        printflags(trusts, trust->sslFlags);
        PORT_Strcat(trusts, ",");
        printflags(trusts, trust->emailFlags);
        PORT_Strcat(trusts, ",");
        printflags(trusts, trust->objectSigningFlags);
    } else {
        PORT_Memcpy(trusts,",,",3);
    }
    fprintf(out, "%-60s %-5s\n", name, trusts);

    return (SECSuccess);
}

int
SECU_DecodeAndPrintExtensions(FILE *out, SECItem *any, char *m, int level)
{
    CERTCertExtension **extensions = NULL;
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    int rv = 0;

    if (!arena) 
	return SEC_ERROR_NO_MEMORY;

    rv = SEC_QuickDERDecodeItem(arena, &extensions, 
		   SEC_ASN1_GET(CERT_SequenceOfCertExtensionTemplate), any);
    if (!rv)
	SECU_PrintExtensions(out, extensions, m, level);
    else 
    	SECU_PrintAny(out, any, m, level);
    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

/* print a decoded SET OF or SEQUENCE OF Extensions */
int
SECU_PrintSetOfExtensions(FILE *out, SECItem **any, char *m, int level)
{
    int rv = 0;
    if (m && *m) {
	SECU_Indent(out, level++); fprintf(out, "%s:\n", m);
    }
    while (any && any[0]) {
    	rv |= SECU_DecodeAndPrintExtensions(out, any[0], "", level);
	any++;
    }
    return rv;
}

/* print a decoded SET OF or SEQUENCE OF "ANY" */
int
SECU_PrintSetOfAny(FILE *out, SECItem **any, char *m, int level)
{
    int rv = 0;
    if (m && *m) {
	SECU_Indent(out, level++); fprintf(out, "%s:\n", m);
    }
    while (any && any[0]) {
    	SECU_PrintAny(out, any[0], "", level);
	any++;
    }
    return rv;
}

int
SECU_PrintCertAttribute(FILE *out, CERTAttribute *attr, char *m, int level)
{
    int rv = 0;
    SECOidTag tag;
    tag = SECU_PrintObjectID(out, &attr->attrType, "Attribute Type", level);
    if (tag == SEC_OID_PKCS9_EXTENSION_REQUEST) {
	rv = SECU_PrintSetOfExtensions(out, attr->attrValue, "Extensions", level);
    } else {
	rv = SECU_PrintSetOfAny(out, attr->attrValue, "Attribute Values", level);
    }
    return rv;
}

int
SECU_PrintCertAttributes(FILE *out, CERTAttribute **attrs, char *m, int level)
{
    int rv = 0;
    while (attrs[0]) {
	rv |= SECU_PrintCertAttribute(out, attrs[0], m, level+1);
    	attrs++;
    }
    return rv;
}

int  /* sometimes a PRErrorCode, other times a SECStatus.  Sigh. */
SECU_PrintCertificateRequest(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CERTCertificateRequest *cr;
    int rv = SEC_ERROR_NO_MEMORY;

    if (!arena) 
	return rv;

    /* Decode certificate request */
    cr = PORT_ArenaZNew(arena, CERTCertificateRequest);
    if (!cr)
	goto loser;
    cr->arena = arena;
    rv = SEC_QuickDERDecodeItem(arena, cr, 
                           SEC_ASN1_GET(CERT_CertificateRequestTemplate), der);
    if (rv) 
	goto loser;

    /* Pretty print it out */
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &cr->version, "Version", level+1);
    SECU_PrintName(out, &cr->subject, "Subject", level+1);
    secu_PrintSubjectPublicKeyInfo(out, arena, &cr->subjectPublicKeyInfo,
			      "Subject Public Key Info", level+1);
    if (cr->attributes)
	SECU_PrintCertAttributes(out, cr->attributes, "Attributes", level+1);
    rv = 0;
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

int
SECU_PrintCertificate(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CERTCertificate *c;
    int rv = SEC_ERROR_NO_MEMORY;
    int iv;
    
    if (!arena)
	return rv;

    /* Decode certificate */
    c = PORT_ArenaZNew(arena, CERTCertificate);
    if (!c)
	goto loser;
    c->arena = arena;
    rv = SEC_ASN1DecodeItem(arena, c, 
                            SEC_ASN1_GET(CERT_CertificateTemplate), der);
    if (rv) {
        SECU_Indent(out, level); 
	SECU_PrintErrMsg(out, level, "Error", "Parsing extension");
	SECU_PrintAny(out, der, "Raw", level);
	goto loser;
    }
    /* Pretty print it out */
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    iv = c->version.len ? DER_GetInteger(&c->version) : 0;  /* version is optional */
    SECU_Indent(out, level+1); fprintf(out, "%s: %d (0x%x)\n", "Version", iv + 1, iv);

    SECU_PrintInteger(out, &c->serialNumber, "Serial Number", level+1);
    SECU_PrintAlgorithmID(out, &c->signature, "Signature Algorithm", level+1);
    SECU_PrintName(out, &c->issuer, "Issuer", level+1);
    secu_PrintValidity(out, &c->validity, "Validity", level+1);
    SECU_PrintName(out, &c->subject, "Subject", level+1);
    secu_PrintSubjectPublicKeyInfo(out, arena, &c->subjectPublicKeyInfo,
			      "Subject Public Key Info", level+1);
    if (c->issuerID.data) 
	secu_PrintDecodedBitString(out, &c->issuerID, "Issuer Unique ID", level+1);
    if (c->subjectID.data) 
	secu_PrintDecodedBitString(out, &c->subjectID, "Subject Unique ID", level+1);
    SECU_PrintExtensions(out, c->extensions, "Signed Extensions", level+1);
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

int
SECU_PrintRSAPublicKey(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SECKEYPublicKey key;
    int rv = SEC_ERROR_NO_MEMORY;

    if (!arena)
	return rv;

    PORT_Memset(&key, 0, sizeof(key));
    rv = SEC_ASN1DecodeItem(arena, &key, 
                            SEC_ASN1_GET(SECKEY_RSAPublicKeyTemplate), der);
    if (!rv) {
	/* Pretty print it out */
	secu_PrintRSAPublicKey(out, &key, m, level);
    }

    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

int
SECU_PrintSubjectPublicKeyInfo(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    int          rv    = SEC_ERROR_NO_MEMORY;
    CERTSubjectPublicKeyInfo spki;

    if (!arena)
	return rv;

    PORT_Memset(&spki, 0, sizeof spki);
    rv = SEC_ASN1DecodeItem(arena, &spki, 
                            SEC_ASN1_GET(CERT_SubjectPublicKeyInfoTemplate), 
			    der);
    if (!rv) {
	if (m && *m) {
	    SECU_Indent(out, level);  fprintf(out, "%s:\n", m);
	}
	secu_PrintSubjectPublicKeyInfo(out, arena, &spki,
				       "Subject Public Key Info", level+1);
    }

    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

#ifdef HAVE_EPV_TEMPLATE
int
SECU_PrintPrivateKey(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SECKEYEncryptedPrivateKeyInfo key;
    int rv = SEC_ERROR_NO_MEMORY;

    if (!arena)
	return rv;

    PORT_Memset(&key, 0, sizeof(key));
    rv = SEC_ASN1DecodeItem(arena, &key, 
		SEC_ASN1_GET(SECKEY_EncryptedPrivateKeyInfoTemplate), der);
    if (rv)
	goto loser;

    /* Pretty print it out */
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintAlgorithmID(out, &key.algorithm, "Encryption Algorithm", 
			  level+1);
    SECU_PrintAsHex(out, &key.encryptedData, "Encrypted Data", level+1);
loser:
    PORT_FreeArena(arena, PR_TRUE);
    return rv;
}
#endif

int
SECU_PrintFingerprints(FILE *out, SECItem *derCert, char *m, int level)
{
    unsigned char fingerprint[20];
    char *fpStr = NULL;
    int err     = PORT_GetError();
    SECStatus rv;
    SECItem fpItem;

    /* print MD5 fingerprint */
    memset(fingerprint, 0, sizeof fingerprint);
    rv = PK11_HashBuf(SEC_OID_MD5,fingerprint, derCert->data, derCert->len);
    fpItem.data = fingerprint;
    fpItem.len = MD5_LENGTH;
    fpStr = CERT_Hexify(&fpItem, 1);
    SECU_Indent(out, level);  fprintf(out, "%s (MD5):\n", m);
    SECU_Indent(out, level+1); fprintf(out, "%s\n", fpStr);
    PORT_Free(fpStr);
    fpStr = NULL;
    if (rv != SECSuccess && !err)
	err = PORT_GetError();

    /* print SHA1 fingerprint */
    memset(fingerprint, 0, sizeof fingerprint);
    rv = PK11_HashBuf(SEC_OID_SHA1,fingerprint, derCert->data, derCert->len);
    fpItem.data = fingerprint;
    fpItem.len = SHA1_LENGTH;
    fpStr = CERT_Hexify(&fpItem, 1);
    SECU_Indent(out, level);  fprintf(out, "%s (SHA1):\n", m);
    SECU_Indent(out, level+1); fprintf(out, "%s\n", fpStr);
    PORT_Free(fpStr);
    fprintf(out, "\n");

    if (err) 
	PORT_SetError(err);
    if (err || rv != SECSuccess)
	return SECFailure;

    return 0;
}

/*
** PKCS7 Support
*/

/* forward declaration */
static int
secu_PrintPKCS7ContentInfo(FILE *, SEC_PKCS7ContentInfo *, char *, int);

/*
** secu_PrintPKCS7EncContent
**   Prints a SEC_PKCS7EncryptedContentInfo (without decrypting it)
*/
static void
secu_PrintPKCS7EncContent(FILE *out, SEC_PKCS7EncryptedContentInfo *src, 
			  char *m, int level)
{
    if (src->contentTypeTag == NULL)
	src->contentTypeTag = SECOID_FindOID(&(src->contentType));

    SECU_Indent(out, level);
    fprintf(out, "%s:\n", m);
    SECU_Indent(out, level + 1); 
    fprintf(out, "Content Type: %s\n",
	    (src->contentTypeTag != NULL) ? src->contentTypeTag->desc
					  : "Unknown");
    SECU_PrintAlgorithmID(out, &(src->contentEncAlg),
			  "Content Encryption Algorithm", level+1);
    SECU_PrintAsHex(out, &(src->encContent), 
		    "Encrypted Content", level+1);
}

/*
** secu_PrintRecipientInfo
**   Prints a PKCS7RecipientInfo type
*/
static void
secu_PrintRecipientInfo(FILE *out, SEC_PKCS7RecipientInfo *info, char *m, 
			int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(info->version), "Version", level + 1);	

    SECU_PrintName(out, &(info->issuerAndSN->issuer), "Issuer", 
		 level + 1);
    SECU_PrintInteger(out, &(info->issuerAndSN->serialNumber), 
		      "Serial Number", level + 1);

    /* Parse and display encrypted key */
    SECU_PrintAlgorithmID(out, &(info->keyEncAlg), 
			"Key Encryption Algorithm", level + 1);
    SECU_PrintAsHex(out, &(info->encKey), "Encrypted Key", level + 1);
}

/* 
** secu_PrintSignerInfo
**   Prints a PKCS7SingerInfo type
*/
static void
secu_PrintSignerInfo(FILE *out, SEC_PKCS7SignerInfo *info, char *m, int level)
{
    SEC_PKCS7Attribute *attr;
    int iv;
    char om[100];
    
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(info->version), "Version", level + 1);	

    SECU_PrintName(out, &(info->issuerAndSN->issuer), "Issuer", 
		 level + 1);
    SECU_PrintInteger(out, &(info->issuerAndSN->serialNumber), 
		      "Serial Number", level + 1);
  
    SECU_PrintAlgorithmID(out, &(info->digestAlg), "Digest Algorithm",
			  level + 1);
    
    if (info->authAttr != NULL) {
	SECU_Indent(out, level + 1); 
	fprintf(out, "Authenticated Attributes:\n");
	iv = 0;
	while ((attr = info->authAttr[iv++]) != NULL) {
	    sprintf(om, "Attribute (%d)", iv); 
	    secu_PrintAttribute(out, attr, om, level + 2);
	}
    }
    
    /* Parse and display signature */
    SECU_PrintAlgorithmID(out, &(info->digestEncAlg), 
			"Digest Encryption Algorithm", level + 1);
    SECU_PrintAsHex(out, &(info->encDigest), "Encrypted Digest", level + 1);
    
    if (info->unAuthAttr != NULL) {
	SECU_Indent(out, level + 1); 
	fprintf(out, "Unauthenticated Attributes:\n");
	iv = 0;
	while ((attr = info->unAuthAttr[iv++]) != NULL) {
	    sprintf(om, "Attribute (%x)", iv); 
	    secu_PrintAttribute(out, attr, om, level + 2);
	}
    }
}

/* callers of this function must make sure that the CERTSignedCrl
   from which they are extracting the CERTCrl has been fully-decoded.
   Otherwise it will not have the entries even though the CRL may have
   some */

void
SECU_PrintCRLInfo(FILE *out, CERTCrl *crl, char *m, int level)
{
    CERTCrlEntry *entry;
    int iv;
    char om[100];
    
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    /* version is optional */
    iv = crl->version.len ? DER_GetInteger(&crl->version) : 0;  
    SECU_Indent(out, level+1); 
    	fprintf(out, "%s: %d (0x%x)\n", "Version", iv + 1, iv);
    SECU_PrintAlgorithmID(out, &(crl->signatureAlg), "Signature Algorithm",
			  level + 1);
    SECU_PrintName(out, &(crl->name), "Issuer", level + 1);
    SECU_PrintTimeChoice(out, &(crl->lastUpdate), "This Update", level + 1);
    if (crl->nextUpdate.data && crl->nextUpdate.len) /* is optional */
	SECU_PrintTimeChoice(out, &(crl->nextUpdate), "Next Update", level + 1);
    
    if (crl->entries != NULL) {
	iv = 0;
	while ((entry = crl->entries[iv++]) != NULL) {
	    sprintf(om, "Entry (%x):\n", iv); 
	    SECU_Indent(out, level + 1); fputs(om, out);
	    SECU_PrintInteger(out, &(entry->serialNumber), "Serial Number",
			      level + 2);
	    SECU_PrintTimeChoice(out, &(entry->revocationDate), 
	                         "Revocation Date", level + 2);
	    SECU_PrintExtensions(out, entry->extensions, 
	                         "Entry Extensions", level + 2);
	}
    }
    SECU_PrintExtensions(out, crl->extensions, "CRL Extensions", level + 1);
}

/*
** secu_PrintPKCS7Signed
**   Pretty print a PKCS7 signed data type (up to version 1).
*/
static int
secu_PrintPKCS7Signed(FILE *out, SEC_PKCS7SignedData *src,
		      const char *m, int level)
{
    SECAlgorithmID *digAlg;		/* digest algorithms */
    SECItem *aCert;			/* certificate */
    CERTSignedCrl *aCrl;		/* certificate revocation list */
    SEC_PKCS7SignerInfo *sigInfo;	/* signer information */
    int rv, iv;
    char om[100];

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    /* Parse and list digest algorithms (if any) */
    if (src->digestAlgorithms != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Digest Algorithm List:\n");
	iv = 0;
	while ((digAlg = src->digestAlgorithms[iv++]) != NULL) {
	    sprintf(om, "Digest Algorithm (%x)", iv);
	    SECU_PrintAlgorithmID(out, digAlg, om, level + 2);
	}
    }

    /* Now for the content */
    rv = secu_PrintPKCS7ContentInfo(out, &(src->contentInfo), 
				    "Content Information", level + 1);
    if (rv != 0)
	return rv;

    /* Parse and list certificates (if any) */
    if (src->rawCerts != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Certificate List:\n");
	iv = 0;
	while ((aCert = src->rawCerts[iv++]) != NULL) {
	    sprintf(om, "Certificate (%x)", iv);
	    rv = SECU_PrintSignedData(out, aCert, om, level + 2, 
				      SECU_PrintCertificate);
	    if (rv)
		return rv;
	}
    }

    /* Parse and list CRL's (if any) */
    if (src->crls != NULL) {
	SECU_Indent(out, level + 1);  
	fprintf(out, "Signed Revocation Lists:\n");
	iv = 0;
	while ((aCrl = src->crls[iv++]) != NULL) {
	    sprintf(om, "Signed Revocation List (%x)", iv);
	    SECU_Indent(out, level + 2);  fprintf(out, "%s:\n", om);
	    SECU_PrintAlgorithmID(out, &aCrl->signatureWrap.signatureAlgorithm, 
				  "Signature Algorithm", level+3);
	    DER_ConvertBitString(&aCrl->signatureWrap.signature);
	    SECU_PrintAsHex(out, &aCrl->signatureWrap.signature, "Signature",
			    level+3);
	    SECU_PrintCRLInfo(out, &aCrl->crl, "Certificate Revocation List", 
			  level + 3); 
	}
    }

    /* Parse and list signatures (if any) */
    if (src->signerInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Signer Information List:\n");
	iv = 0;
	while ((sigInfo = src->signerInfos[iv++]) != NULL) {
	    sprintf(om, "Signer Information (%x)", iv);
	    secu_PrintSignerInfo(out, sigInfo, om, level + 2);
	}
    }  

    return 0;
}

/*
** secu_PrintPKCS7Enveloped
**  Pretty print a PKCS7 enveloped data type (up to version 1).
*/
static void
secu_PrintPKCS7Enveloped(FILE *out, SEC_PKCS7EnvelopedData *src,
			 const char *m, int level)
{
    SEC_PKCS7RecipientInfo *recInfo;   /* pointer for signer information */
    int iv;
    char om[100];

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    /* Parse and list recipients (this is not optional) */
    if (src->recipientInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Recipient Information List:\n");
	iv = 0;
	while ((recInfo = src->recipientInfos[iv++]) != NULL) {
	    sprintf(om, "Recipient Information (%x)", iv);
	    secu_PrintRecipientInfo(out, recInfo, om, level + 2);
	}
    }  

    secu_PrintPKCS7EncContent(out, &src->encContentInfo, 
			      "Encrypted Content Information", level + 1);
}

/*
** secu_PrintPKCS7SignedEnveloped
**   Pretty print a PKCS7 singed and enveloped data type (up to version 1).
*/
static int
secu_PrintPKCS7SignedAndEnveloped(FILE *out,
				  SEC_PKCS7SignedAndEnvelopedData *src,
				  const char *m, int level)
{
    SECAlgorithmID *digAlg;  /* pointer for digest algorithms */
    SECItem *aCert;           /* pointer for certificate */
    CERTSignedCrl *aCrl;        /* pointer for certificate revocation list */
    SEC_PKCS7SignerInfo *sigInfo;   /* pointer for signer information */
    SEC_PKCS7RecipientInfo *recInfo; /* pointer for recipient information */
    int rv, iv;
    char om[100];

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    /* Parse and list recipients (this is not optional) */
    if (src->recipientInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Recipient Information List:\n");
	iv = 0;
	while ((recInfo = src->recipientInfos[iv++]) != NULL) {
	    sprintf(om, "Recipient Information (%x)", iv);
	    secu_PrintRecipientInfo(out, recInfo, om, level + 2);
	}
    }  

    /* Parse and list digest algorithms (if any) */
    if (src->digestAlgorithms != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Digest Algorithm List:\n");
	iv = 0;
	while ((digAlg = src->digestAlgorithms[iv++]) != NULL) {
	    sprintf(om, "Digest Algorithm (%x)", iv);
	    SECU_PrintAlgorithmID(out, digAlg, om, level + 2);
	}
    }

    secu_PrintPKCS7EncContent(out, &src->encContentInfo, 
			      "Encrypted Content Information", level + 1);

    /* Parse and list certificates (if any) */
    if (src->rawCerts != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Certificate List:\n");
	iv = 0;
	while ((aCert = src->rawCerts[iv++]) != NULL) {
	    sprintf(om, "Certificate (%x)", iv);
	    rv = SECU_PrintSignedData(out, aCert, om, level + 2, 
				      SECU_PrintCertificate);
	    if (rv)
		return rv;
	}
    }

    /* Parse and list CRL's (if any) */
    if (src->crls != NULL) {
	SECU_Indent(out, level + 1);  
	fprintf(out, "Signed Revocation Lists:\n");
	iv = 0;
	while ((aCrl = src->crls[iv++]) != NULL) {
	    sprintf(om, "Signed Revocation List (%x)", iv);
	    SECU_Indent(out, level + 2);  fprintf(out, "%s:\n", om);
	    SECU_PrintAlgorithmID(out, &aCrl->signatureWrap.signatureAlgorithm, 
				  "Signature Algorithm", level+3);
	    DER_ConvertBitString(&aCrl->signatureWrap.signature);
	    SECU_PrintAsHex(out, &aCrl->signatureWrap.signature, "Signature",
			    level+3);
	    SECU_PrintCRLInfo(out, &aCrl->crl, "Certificate Revocation List", 
			  level + 3); 
	}
    }

    /* Parse and list signatures (if any) */
    if (src->signerInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Signer Information List:\n");
	iv = 0;
	while ((sigInfo = src->signerInfos[iv++]) != NULL) {
	    sprintf(om, "Signer Information (%x)", iv);
	    secu_PrintSignerInfo(out, sigInfo, om, level + 2);
	}
    }  

    return 0;
}

int
SECU_PrintCrl (FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CERTCrl *c = NULL;
    int rv = SEC_ERROR_NO_MEMORY;

    if (!arena)
    	return rv;
    do {
	/* Decode CRL */
	c = PORT_ArenaZNew(arena, CERTCrl);
	if (!c)
	    break;

	rv = SEC_QuickDERDecodeItem(arena, c, SEC_ASN1_GET(CERT_CrlTemplate), der);
	if (rv != SECSuccess)
	    break;
	SECU_PrintCRLInfo (out, c, m, level);
    } while (0);
    PORT_FreeArena (arena, PR_FALSE);
    return rv;
}


/*
** secu_PrintPKCS7Encrypted
**   Pretty print a PKCS7 encrypted data type (up to version 1).
*/
static void
secu_PrintPKCS7Encrypted(FILE *out, SEC_PKCS7EncryptedData *src,
			 const char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    secu_PrintPKCS7EncContent(out, &src->encContentInfo, 
			      "Encrypted Content Information", level + 1);
}

/*
** secu_PrintPKCS7Digested
**   Pretty print a PKCS7 digested data type (up to version 1).
*/
static void
secu_PrintPKCS7Digested(FILE *out, SEC_PKCS7DigestedData *src,
			const char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);
    
    SECU_PrintAlgorithmID(out, &src->digestAlg, "Digest Algorithm",
			  level + 1);
    secu_PrintPKCS7ContentInfo(out, &src->contentInfo, "Content Information",
			       level + 1);
    SECU_PrintAsHex(out, &src->digest, "Digest", level + 1);  
}

/*
** secu_PrintPKCS7ContentInfo
**   Takes a SEC_PKCS7ContentInfo type and sends the contents to the 
** appropriate function
*/
static int
secu_PrintPKCS7ContentInfo(FILE *out, SEC_PKCS7ContentInfo *src,
			   char *m, int level)
{
    const char *desc;
    SECOidTag kind;
    int rv;

    SECU_Indent(out, level);  fprintf(out, "%s:\n", m);
    level++;

    if (src->contentTypeTag == NULL)
	src->contentTypeTag = SECOID_FindOID(&(src->contentType));

    if (src->contentTypeTag == NULL) {
	desc = "Unknown";
	kind = SEC_OID_PKCS7_DATA;
    } else {
	desc = src->contentTypeTag->desc;
	kind = src->contentTypeTag->offset;
    }

    if (src->content.data == NULL) {
	SECU_Indent(out, level); fprintf(out, "%s:\n", desc);
	level++;
	SECU_Indent(out, level); fprintf(out, "<no content>\n");
	return 0;
    }

    rv = 0;
    switch (kind) {
      case SEC_OID_PKCS7_SIGNED_DATA:  /* Signed Data */
	rv = secu_PrintPKCS7Signed(out, src->content.signedData, desc, level);
	break;

      case SEC_OID_PKCS7_ENVELOPED_DATA:  /* Enveloped Data */
        secu_PrintPKCS7Enveloped(out, src->content.envelopedData, desc, level);
	break;

      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:  /* Signed and Enveloped */
	rv = secu_PrintPKCS7SignedAndEnveloped(out,
					src->content.signedAndEnvelopedData,
					desc, level);
	break;

      case SEC_OID_PKCS7_DIGESTED_DATA:  /* Digested Data */
	secu_PrintPKCS7Digested(out, src->content.digestedData, desc, level);
	break;

      case SEC_OID_PKCS7_ENCRYPTED_DATA:  /* Encrypted Data */
	secu_PrintPKCS7Encrypted(out, src->content.encryptedData, desc, level);
	break;

      default:
	SECU_PrintAsHex(out, src->content.data, desc, level);
	break;
    }

    return rv;
}

/*
** SECU_PrintPKCS7ContentInfo
**   Decode and print any major PKCS7 data type (up to version 1).
*/
int
SECU_PrintPKCS7ContentInfo(FILE *out, SECItem *der, char *m, int level)
{
    SEC_PKCS7ContentInfo *cinfo;
    int rv;

    cinfo = SEC_PKCS7DecodeItem(der, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (cinfo != NULL) {
	/* Send it to recursive parsing and printing module */
	rv = secu_PrintPKCS7ContentInfo(out, cinfo, m, level);
	SEC_PKCS7DestroyContentInfo(cinfo);
    } else {
	rv = -1;
    }

    return rv;
}

/*
** End of PKCS7 functions
*/

void
printFlags(FILE *out, unsigned int flags, int level)
{
    if ( flags & CERTDB_TERMINAL_RECORD ) {
	SECU_Indent(out, level); fprintf(out, "Terminal Record\n");
    }
    if ( flags & CERTDB_TRUSTED ) {
	SECU_Indent(out, level); fprintf(out, "Trusted\n");
    }
    if ( flags & CERTDB_SEND_WARN ) {
	SECU_Indent(out, level); fprintf(out, "Warn When Sending\n");
    }
    if ( flags & CERTDB_VALID_CA ) {
	SECU_Indent(out, level); fprintf(out, "Valid CA\n");
    }
    if ( flags & CERTDB_TRUSTED_CA ) {
	SECU_Indent(out, level); fprintf(out, "Trusted CA\n");
    }
    if ( flags & CERTDB_NS_TRUSTED_CA ) {
	SECU_Indent(out, level); fprintf(out, "Netscape Trusted CA\n");
    }
    if ( flags & CERTDB_USER ) {
	SECU_Indent(out, level); fprintf(out, "User\n");
    }
    if ( flags & CERTDB_TRUSTED_CLIENT_CA ) {
	SECU_Indent(out, level); fprintf(out, "Trusted Client CA\n");
    }
    if ( flags & CERTDB_GOVT_APPROVED_CA ) {
	SECU_Indent(out, level); fprintf(out, "Step-up\n");
    }
}

void
SECU_PrintTrustFlags(FILE *out, CERTCertTrust *trust, char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_Indent(out, level+1); fprintf(out, "SSL Flags:\n");
    printFlags(out, trust->sslFlags, level+2);
    SECU_Indent(out, level+1); fprintf(out, "Email Flags:\n");
    printFlags(out, trust->emailFlags, level+2);
    SECU_Indent(out, level+1); fprintf(out, "Object Signing Flags:\n");
    printFlags(out, trust->objectSigningFlags, level+2);
}

int SECU_PrintDERName(FILE *out, SECItem *der, const char *m, int level)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CERTName *name;
    int rv = SEC_ERROR_NO_MEMORY;

    if (!arena)
	return rv;

    name = PORT_ArenaZNew(arena, CERTName);
    if (!name)
	goto loser;

    rv = SEC_ASN1DecodeItem(arena, name, SEC_ASN1_GET(CERT_NameTemplate), der);
    if (rv)
	goto loser;

    SECU_PrintName(out, name, m, level);
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

int SECU_PrintSignedData(FILE *out, SECItem *der, const char *m,
			   int level, SECU_PPFunc inner)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CERTSignedData *sd;
    int rv = SEC_ERROR_NO_MEMORY;

    if (!arena)
	return rv;

    /* Strip off the signature */
    sd = PORT_ArenaZNew(arena, CERTSignedData);
    if (!sd)
	goto loser;

    rv = SEC_ASN1DecodeItem(arena, sd, SEC_ASN1_GET(CERT_SignedDataTemplate), 
                            der);
    if (rv)
	goto loser;

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    rv = (*inner)(out, &sd->data, "Data", level+1);

    SECU_PrintAlgorithmID(out, &sd->signatureAlgorithm, "Signature Algorithm",
			  level+1);
    DER_ConvertBitString(&sd->signature);
    SECU_PrintAsHex(out, &sd->signature, "Signature", level+1);
    SECU_PrintFingerprints(out, der, "Fingerprint", level+1);
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

SECStatus
SEC_PrintCertificateAndTrust(CERTCertificate *cert,
                             const char *label,
                             CERTCertTrust *trust)
{
    SECStatus rv;
    SECItem data;
    
    data.data = cert->derCert.data;
    data.len = cert->derCert.len;

    rv = SECU_PrintSignedData(stdout, &data, label, 0,
			      SECU_PrintCertificate);
    if (rv) {
	return(SECFailure);
    }
    if (trust) {
	SECU_PrintTrustFlags(stdout, trust,
	                     "Certificate Trust Flags", 1);
    } else if (cert->trust) {
	SECU_PrintTrustFlags(stdout, cert->trust,
	                     "Certificate Trust Flags", 1);
    }

    printf("\n");

    return(SECSuccess);
}

#if defined(DEBUG) || defined(FORCE_PR_ASSERT)
/* Returns true iff a[i].flag has a duplicate in a[i+1 : count-1]  */
static PRBool HasShortDuplicate(int i, secuCommandFlag *a, int count)
{
	char target = a[i].flag;
	int j;

	/* duplicate '\0' flags are okay, they are used with long forms */
	for (j = i+1; j < count; j++) {
		if (a[j].flag && a[j].flag == target) {
			return PR_TRUE;
		}
	}
	return PR_FALSE;
}

/* Returns true iff a[i].longform has a duplicate in a[i+1 : count-1] */
static PRBool HasLongDuplicate(int i, secuCommandFlag *a, int count)
{
	int j;	
	char *target = a[i].longform;

	if (!target)
		return PR_FALSE;

	for (j = i+1; j < count; j++) {
		if (a[j].longform && strcmp(a[j].longform, target) == 0) {
			return PR_TRUE;
		}
	}
	return PR_FALSE;
}

/* Returns true iff a has no short or long form duplicates
 */
PRBool HasNoDuplicates(secuCommandFlag *a, int count)
{
    int i;

	for (i = 0; i < count; i++) {
		if (a[i].flag && HasShortDuplicate(i, a, count)) {
			return PR_FALSE;
		}
		if (a[i].longform && HasLongDuplicate(i, a, count)) {
			return PR_FALSE;
		}
	}
	return PR_TRUE;
}
#endif

SECStatus
SECU_ParseCommandLine(int argc, char **argv, char *progName,
		      const secuCommand *cmd)
{
    PRBool found;
    PLOptState *optstate;
    PLOptStatus status;
    char *optstring;
    PLLongOpt *longopts = NULL;
    int i, j;
    int lcmd = 0, lopt = 0;

    PR_ASSERT(HasNoDuplicates(cmd->commands, cmd->numCommands));
    PR_ASSERT(HasNoDuplicates(cmd->options, cmd->numOptions));

    optstring = (char *)PORT_Alloc(cmd->numCommands + 2*cmd->numOptions+1);
    if (optstring == NULL)
        return SECFailure;
    
    j = 0;
    for (i=0; i<cmd->numCommands; i++) {
	if (cmd->commands[i].flag) /* single character option ? */
	    optstring[j++] = cmd->commands[i].flag;
	if (cmd->commands[i].longform)
	    lcmd++;
    }
    for (i=0; i<cmd->numOptions; i++) {
	if (cmd->options[i].flag) {
	    optstring[j++] = cmd->options[i].flag;
	    if (cmd->options[i].needsArg)
		optstring[j++] = ':';
	}
	if (cmd->options[i].longform)
	    lopt++;
    }
    
    optstring[j] = '\0';
    
    if (lcmd + lopt > 0) {
	longopts = PORT_NewArray(PLLongOpt, lcmd+lopt+1);
	if (!longopts) {
	    PORT_Free(optstring);
	    return SECFailure;
	}

	j = 0;
	for (i=0; j<lcmd && i<cmd->numCommands; i++) {
	    if (cmd->commands[i].longform) {
		longopts[j].longOptName = cmd->commands[i].longform;
		longopts[j].longOption = 0;
		longopts[j++].valueRequired = cmd->commands[i].needsArg;
	    } 
	}
	lopt += lcmd;
	for (i=0; j<lopt && i<cmd->numOptions; i++) {
	    if (cmd->options[i].longform) {
		longopts[j].longOptName = cmd->options[i].longform;
		longopts[j].longOption = 0;
		longopts[j++].valueRequired = cmd->options[i].needsArg;
	    }
	}
	longopts[j].longOptName = NULL;
    }

    optstate = PL_CreateLongOptState(argc, argv, optstring, longopts);
    if (!optstate) {
        PORT_Free(optstring);
        PORT_Free(longopts);
        return SECFailure;
    }
    /* Parse command line arguments */
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
	const char *optstatelong;
	char        option = optstate->option;

	/*  positional parameter, single-char option or long opt? */
	if (optstate->longOptIndex == -1) {
	    /* not a long opt */
	    if (option == '\0')
	        continue;               /* it's a positional parameter */
	    optstatelong = "";
	} else {
	    /* long opt */
            if (option == '\0')
		option = '\377';        /* force unequal with all flags */
	    optstatelong = longopts[optstate->longOptIndex].longOptName;
	}

	found = PR_FALSE;

	for (i=0; i<cmd->numCommands; i++) {
	    if (cmd->commands[i].flag == option ||
	        cmd->commands[i].longform == optstatelong) {
		cmd->commands[i].activated = PR_TRUE;
		if (optstate->value) {
		    cmd->commands[i].arg = (char *)optstate->value;
		}
		found = PR_TRUE;
		break;
	    }
	}

	if (found)
	    continue;

	for (i=0; i<cmd->numOptions; i++) {
	    if (cmd->options[i].flag == option ||
		cmd->options[i].longform == optstatelong) {
		cmd->options[i].activated = PR_TRUE;
		if (optstate->value) {
		    cmd->options[i].arg = (char *)optstate->value;
		} else if (cmd->options[i].needsArg) {
		    status = PL_OPT_BAD;
		    goto loser;
		}
		found = PR_TRUE;
		break;
	    }
	}

	if (!found) {
	    status = PL_OPT_BAD;
	    break;
	}
    }

loser:
    PL_DestroyOptState(optstate);
    PORT_Free(optstring);
    if (longopts)
	PORT_Free(longopts);
    if (status == PL_OPT_BAD)
	return SECFailure;
    return SECSuccess;
}

char *
SECU_GetOptionArg(const secuCommand *cmd, int optionNum)
{
	if (optionNum < 0 || optionNum >= cmd->numOptions)
		return NULL;
	if (cmd->options[optionNum].activated)
		return PL_strdup(cmd->options[optionNum].arg);
	else
		return NULL;
}


void 
SECU_PrintPRandOSError(char *progName) 
{
    char buffer[513];
    PRInt32     errLen = PR_GetErrorTextLength();
    if (errLen > 0 && errLen < sizeof buffer) {
        PR_GetErrorText(buffer);
    }
    SECU_PrintError(progName, "function failed");
    if (errLen > 0 && errLen < sizeof buffer) {
        PR_fprintf(PR_STDERR, "\t%s\n", buffer);
    }
}


static char *
bestCertName(CERTCertificate *cert) {
    if (cert->nickname) {
	return cert->nickname;
    }
    if (cert->emailAddr && cert->emailAddr[0]) {
	return cert->emailAddr;
    }
    return cert->subjectName;
}

void
SECU_printCertProblemsOnDate(FILE *outfile, CERTCertDBHandle *handle, 
	CERTCertificate *cert, PRBool checksig, 
	SECCertificateUsage certUsage, void *pinArg, PRBool verbose,
	PRTime datetime)
{
    CERTVerifyLog      log;
    CERTVerifyLogNode *node;

    PRErrorCode	       err    = PORT_GetError();

    log.arena = PORT_NewArena(512);
    log.head = log.tail = NULL;
    log.count = 0;
    CERT_VerifyCertificate(handle, cert, checksig, certUsage, datetime, pinArg, &log, NULL);

    SECU_displayVerifyLog(outfile, &log, verbose);

    for (node = log.head; node; node = node->next) {
        if (node->cert)
            CERT_DestroyCertificate(node->cert);
    }
    PORT_FreeArena(log.arena, PR_FALSE);

    PORT_SetError(err); /* restore original error code */
}

void
SECU_displayVerifyLog(FILE *outfile, CERTVerifyLog *log,
                      PRBool verbose)
{
    CERTVerifyLogNode *node   = NULL;
    unsigned int       depth  = (unsigned int)-1;
    unsigned int       flags  = 0;
    char *             errstr = NULL;

    if (log->count > 0) {
	fprintf(outfile,"PROBLEM WITH THE CERT CHAIN:\n");
	for (node = log->head; node; node = node->next) {
	    if (depth != node->depth) {
		depth = node->depth;
		fprintf(outfile,"CERT %d. %s %s:\n", depth,
				 bestCertName(node->cert), 
			  	 depth ? "[Certificate Authority]": "");
	    	if (verbose) {
		    const char * emailAddr;
		    emailAddr = CERT_GetFirstEmailAddress(node->cert);
		    if (emailAddr) {
		    	fprintf(outfile,"Email Address(es): ");
			do {
			    fprintf(outfile, "%s\n", emailAddr);
			    emailAddr = CERT_GetNextEmailAddress(node->cert,
			                                         emailAddr);
			} while (emailAddr);
		    }
		}
	    }
	    fprintf(outfile,"  ERROR %ld: %s\n", node->error,
						SECU_Strerror(node->error));
	    errstr = NULL;
	    switch (node->error) {
	    case SEC_ERROR_INADEQUATE_KEY_USAGE:
		flags = (unsigned int)node->arg;
		switch (flags) {
		case KU_DIGITAL_SIGNATURE:
		    errstr = "Cert cannot sign.";
		    break;
		case KU_KEY_ENCIPHERMENT:
		    errstr = "Cert cannot encrypt.";
		    break;
		case KU_KEY_CERT_SIGN:
		    errstr = "Cert cannot sign other certs.";
		    break;
		default:
		    errstr = "[unknown usage].";
		    break;
		}
	    case SEC_ERROR_INADEQUATE_CERT_TYPE:
		flags = (unsigned int)node->arg;
		switch (flags) {
		case NS_CERT_TYPE_SSL_CLIENT:
		case NS_CERT_TYPE_SSL_SERVER:
		    errstr = "Cert cannot be used for SSL.";
		    break;
		case NS_CERT_TYPE_SSL_CA:
		    errstr = "Cert cannot be used as an SSL CA.";
		    break;
		case NS_CERT_TYPE_EMAIL:
		    errstr = "Cert cannot be used for SMIME.";
		    break;
		case NS_CERT_TYPE_EMAIL_CA:
		    errstr = "Cert cannot be used as an SMIME CA.";
		    break;
		case NS_CERT_TYPE_OBJECT_SIGNING:
		    errstr = "Cert cannot be used for object signing.";
		    break;
		case NS_CERT_TYPE_OBJECT_SIGNING_CA:
		    errstr = "Cert cannot be used as an object signing CA.";
		    break;
		default:
		    errstr = "[unknown usage].";
		    break;
		}
	    case SEC_ERROR_UNKNOWN_ISSUER:
	    case SEC_ERROR_UNTRUSTED_ISSUER:
	    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
		errstr = node->cert->issuerName;
		break;
	    default:
		break;
	    }
	    if (errstr) {
		fprintf(stderr,"    %s\n",errstr);
	    }
	}    
    }
}

void
SECU_printCertProblems(FILE *outfile, CERTCertDBHandle *handle, 
	CERTCertificate *cert, PRBool checksig, 
	SECCertificateUsage certUsage, void *pinArg, PRBool verbose)
{
    SECU_printCertProblemsOnDate(outfile, handle, cert, checksig, 
	                         certUsage, pinArg, verbose, PR_Now());
}

SECOidTag 
SECU_StringToSignatureAlgTag(const char *alg)
{
    SECOidTag hashAlgTag = SEC_OID_UNKNOWN;

    if (alg) {
	if (!PL_strcmp(alg, "MD2")) {
	    hashAlgTag = SEC_OID_MD2;
	} else if (!PL_strcmp(alg, "MD4")) {
	    hashAlgTag = SEC_OID_MD4;
	} else if (!PL_strcmp(alg, "MD5")) {
	    hashAlgTag = SEC_OID_MD5;
	} else if (!PL_strcmp(alg, "SHA1")) {
	    hashAlgTag = SEC_OID_SHA1;
	} else if (!PL_strcmp(alg, "SHA256")) {
	    hashAlgTag = SEC_OID_SHA256;
	} else if (!PL_strcmp(alg, "SHA384")) {
	    hashAlgTag = SEC_OID_SHA384;
	} else if (!PL_strcmp(alg, "SHA512")) {
	    hashAlgTag = SEC_OID_SHA512;
	}
    }
    return hashAlgTag;
}


SECStatus
SECU_StoreCRL(PK11SlotInfo *slot, SECItem *derCrl, PRFileDesc *outFile,
              PRBool ascii, char *url)
{
    PORT_Assert(derCrl != NULL);
    if (!derCrl) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (outFile != NULL) {
        if (ascii) {
            PR_fprintf(outFile, "%s\n%s\n%s\n", NS_CRL_HEADER, 
                       BTOA_DataToAscii(derCrl->data, derCrl->len), 
                       NS_CRL_TRAILER);
        } else {
            if (PR_Write(outFile, derCrl->data, derCrl->len) != derCrl->len) {
                return SECFailure;
            }
        }
    }
    if (slot) {
        CERTSignedCrl *newCrl = PK11_ImportCRL(slot, derCrl, url,
                                               SEC_CRL_TYPE, NULL, 0, NULL, 0);
        if (newCrl != NULL) {
            SEC_DestroyCrl(newCrl);
            return SECSuccess;
        }
        return SECFailure;
    }
    if (!outFile && !slot) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus
SECU_SignAndEncodeCRL(CERTCertificate *issuer, CERTSignedCrl *signCrl,
                      SECOidTag hashAlgTag, SignAndEncodeFuncExitStat *resCode)
{
    SECItem der;
    SECKEYPrivateKey *caPrivateKey = NULL;    
    SECStatus rv;
    PRArenaPool *arena;
    SECOidTag algID;
    void *dummy;

    PORT_Assert(issuer != NULL && signCrl != NULL);
    if (!issuer || !signCrl) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    arena = signCrl->arena;

    caPrivateKey = PK11_FindKeyByAnyCert(issuer, NULL);
    if (caPrivateKey == NULL) {
        *resCode = noKeyFound;
        return SECFailure;
    }

    algID = SEC_GetSignatureAlgorithmOidTag(caPrivateKey->keyType, hashAlgTag);
    if (algID == SEC_OID_UNKNOWN) {
        *resCode = noSignatureMatch;
        rv = SECFailure;
        goto done;
    }

    if (!signCrl->crl.signatureAlg.parameters.data) {
        rv = SECOID_SetAlgorithmID(arena, &signCrl->crl.signatureAlg, algID, 0);
        if (rv != SECSuccess) {
            *resCode = failToEncode;
            goto done;
        }
    }

    der.len = 0;
    der.data = NULL;
    dummy = SEC_ASN1EncodeItem(arena, &der, &signCrl->crl,
                               SEC_ASN1_GET(CERT_CrlTemplate));
    if (!dummy) {
        *resCode = failToEncode;
        rv = SECFailure;
        goto done;
    }

    rv = SECU_DerSignDataCRL(arena, &signCrl->signatureWrap,
                             der.data, der.len, caPrivateKey, algID);
    if (rv != SECSuccess) {
        *resCode = failToSign;
        goto done;
    }

    signCrl->derCrl = PORT_ArenaZNew(arena, SECItem);
    if (signCrl->derCrl == NULL) {
        *resCode = noMem;
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        rv = SECFailure;
        goto done;
    }

    signCrl->derCrl->len = 0;
    signCrl->derCrl->data = NULL;
    dummy = SEC_ASN1EncodeItem (arena, signCrl->derCrl, signCrl,
                                SEC_ASN1_GET(CERT_SignedCrlTemplate));
    if (!dummy) {
        *resCode = failToEncode;
        rv = SECFailure;
        goto done;
    }

done:
    if (caPrivateKey) {
        SECKEY_DestroyPrivateKey(caPrivateKey);
    }
    return rv;
}



SECStatus
SECU_CopyCRL(PRArenaPool *destArena, CERTCrl *destCrl, CERTCrl *srcCrl)
{
    void *dummy;
    SECStatus rv = SECSuccess;
    SECItem der;

    PORT_Assert(destArena && srcCrl && destCrl);
    if (!destArena || !srcCrl || !destCrl) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    der.len = 0;
    der.data = NULL;
    dummy = SEC_ASN1EncodeItem (destArena, &der, srcCrl,
                                SEC_ASN1_GET(CERT_CrlTemplate));
    if (!dummy) {
        return SECFailure;
    }

    rv = SEC_QuickDERDecodeItem(destArena, destCrl,
                                SEC_ASN1_GET(CERT_CrlTemplate), &der);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    
    destCrl->arena = destArena;

    return rv;
}

SECStatus
SECU_DerSignDataCRL(PRArenaPool *arena, CERTSignedData *sd,
                    unsigned char *buf, int len, SECKEYPrivateKey *pk,
                    SECOidTag algID)
{
    SECItem it;
    SECStatus rv;

    it.data = 0;

    /* XXX We should probably have some asserts here to make sure the key type
     * and algID match
     */

    /* Sign input buffer */
    rv = SEC_SignData(&it, buf, len, pk, algID);
    if (rv) goto loser;

    /* Fill out SignedData object */
    PORT_Memset(sd, 0, sizeof(*sd));
    sd->data.data = buf;
    sd->data.len = len;
    sd->signature.data = it.data;
    sd->signature.len = it.len << 3;		/* convert to bit string */
    rv = SECOID_SetAlgorithmID(arena, &sd->signatureAlgorithm, algID, 0);
    if (rv) goto loser;

    return rv;

  loser:
    PORT_Free(it.data);
    return rv;
}

#if 0

/* we need access to the private function cert_FindExtension for this code to work */

CERTAuthKeyID *
SECU_FindCRLAuthKeyIDExten (PRArenaPool *arena, CERTSignedCrl *scrl)
{
    SECItem encodedExtenValue;
    SECStatus rv;
    CERTAuthKeyID *ret;
    CERTCrl* crl;

    if (!scrl) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    crl = &scrl->crl;
    
    encodedExtenValue.data = NULL;
    encodedExtenValue.len = 0;

    rv = cert_FindExtension(crl->extensions, SEC_OID_X509_AUTH_KEY_ID,
			    &encodedExtenValue);
    if ( rv != SECSuccess ) {
	return (NULL);
    }

    ret = CERT_DecodeAuthKeyID (arena, &encodedExtenValue);

    PORT_Free(encodedExtenValue.data);
    encodedExtenValue.data = NULL;
    
    return(ret);
}

#endif

/*
 * Find the issuer of a Crl.  Use the authorityKeyID if it exists.
 */
CERTCertificate *
SECU_FindCrlIssuer(CERTCertDBHandle *dbhandle, SECItem* subject,
                   CERTAuthKeyID* authorityKeyID, PRTime validTime)
{
    CERTCertificate *issuerCert = NULL;
    CERTCertList *certList = NULL;

    if (!subject) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    certList =
        CERT_CreateSubjectCertList(NULL, dbhandle, subject,
                                   validTime, PR_TRUE);
    if (certList) {
        CERTCertListNode *node = CERT_LIST_HEAD(certList);
    
        /* XXX and authoritykeyid in the future */
        while ( ! CERT_LIST_END(node, certList) ) {
            CERTCertificate *cert = node->cert;
            /* check cert CERTCertTrust data is allocated, check cert
               usage extension, check that cert has pkey in db. Select
               the first (newest) user cert */
            if (cert->trust &&
                CERT_CheckCertUsage(cert, KU_CRL_SIGN) == SECSuccess &&
                CERT_IsUserCert(cert)) {
                
                issuerCert = CERT_DupCertificate(cert);
                break;
            }
            node = CERT_LIST_NEXT(node);   
        }
        CERT_DestroyCertList(certList);
    }
    return(issuerCert);
}


/* Encodes and adds extensions to the CRL or CRL entries. */
SECStatus 
SECU_EncodeAndAddExtensionValue(PRArenaPool *arena, void *extHandle, 
                                void *value, PRBool criticality, int extenType, 
                                EXTEN_EXT_VALUE_ENCODER EncodeValueFn)
{
    SECItem encodedValue;
    SECStatus rv;

    encodedValue.data = NULL;
    encodedValue.len = 0;
    do {
        rv = (*EncodeValueFn)(arena, value, &encodedValue);
        if (rv != SECSuccess)
            break;

        rv = CERT_AddExtension(extHandle, extenType, &encodedValue,
                               criticality, PR_TRUE);
        if (rv != SECSuccess)
            break;
    } while (0);

    return (rv);
}

/* Caller ensures that dst is at least item->len*2+1 bytes long */
void
SECU_SECItemToHex(const SECItem * item, char * dst)
{
    if (dst && item && item->data) {
	unsigned char * src = item->data;
	unsigned int    len = item->len;
	for (; len > 0; --len, dst += 2) {
	    sprintf(dst, "%02x", *src++);
	}
	*dst = '\0';
    }
}

static unsigned char nibble(char c) {
    c = PORT_Tolower(c);
    return ( c >= '0' && c <= '9') ? c - '0' :
           ( c >= 'a' && c <= 'f') ? c - 'a' +10 : -1;
}

SECStatus
SECU_SECItemHexStringToBinary(SECItem* srcdest)
{
    int i;

    if (!srcdest) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (srcdest->len < 4 || (srcdest->len % 2) ) {
        /* too short to convert, or even number of characters */
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }
    if (PORT_Strncasecmp((const char*)srcdest->data, "0x", 2)) {
        /* wrong prefix */
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    /* 1st pass to check for hex characters */
    for (i=2; i<srcdest->len; i++) {
        char c = PORT_Tolower(srcdest->data[i]);
        if (! ( ( c >= '0' && c <= '9') ||
                ( c >= 'a' && c <= 'f')
              ) ) {
            PORT_SetError(SEC_ERROR_BAD_DATA);
            return SECFailure;
        }
    }

    /* 2nd pass to convert */
    for (i=2; i<srcdest->len; i+=2) {
        srcdest->data[(i-2)/2] = (nibble(srcdest->data[i]) << 4) +
                                 nibble(srcdest->data[i+1]);
    }

    /* adjust length */
    srcdest->len -= 2;
    srcdest->len /= 2;
    return SECSuccess;
}

CERTCertificate*
SECU_FindCertByNicknameOrFilename(CERTCertDBHandle *handle,
                                  char *name, PRBool ascii,
                                  void *pwarg)
{
    CERTCertificate *the_cert;
    the_cert = CERT_FindCertByNicknameOrEmailAddr(handle, name);
    if (the_cert) {
        return the_cert;
    }
    the_cert = PK11_FindCertFromNickname(name, pwarg);
    if (!the_cert) {
        /* Don't have a cert with name "name" in the DB. Try to
         * open a file with such name and get the cert from there.*/
        SECStatus rv;
        SECItem item = {0, NULL, 0};
        PRFileDesc* fd = PR_Open(name, PR_RDONLY, 0777); 
        if (!fd) {
            return NULL;
        }
        rv = SECU_ReadDERFromFile(&item, fd, ascii);
        PR_Close(fd);
        if (rv != SECSuccess || !item.len) {
            PORT_Free(item.data);
            return NULL;
        }
        the_cert = CERT_NewTempCertificate(handle, &item, 
                                           NULL     /* nickname */, 
                                           PR_FALSE /* isPerm */, 
                                           PR_TRUE  /* copyDER */);
        PORT_Free(item.data);
    }
    return the_cert;
}


