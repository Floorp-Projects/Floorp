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
#ifndef _SEC_UTIL_H_
#define _SEC_UTIL_H_

#include "seccomon.h"
#include "secitem.h"
#include "prerror.h"
#include "base64.h"
#include "key.h"
#include "secpkcs7.h"
#include "secasn1.h"
#include "secder.h"
#include <stdio.h>

#define SEC_CT_PRIVATE_KEY		"private-key"
#define SEC_CT_PUBLIC_KEY		"public-key"
#define SEC_CT_CERTIFICATE		"certificate"
#define SEC_CT_CERTIFICATE_REQUEST	"certificate-request"
#define SEC_CT_PKCS7			"pkcs7"
#define SEC_CT_CRL			"crl"

#define NS_CERTREQ_HEADER "-----BEGIN NEW CERTIFICATE REQUEST-----"
#define NS_CERTREQ_TRAILER "-----END NEW CERTIFICATE REQUEST-----"

#define NS_CERT_HEADER "-----BEGIN CERTIFICATE-----"
#define NS_CERT_TRAILER "-----END CERTIFICATE-----"

/* From libsec/pcertdb.c --- it's not declared in sec.h */
extern SECStatus SEC_AddPermCertificate(CERTCertDBHandle *handle,
		SECItem *derCert, char *nickname, CERTCertTrust *trust);


#ifdef SECUTIL_NEW
typedef int (*SECU_PPFunc)(PRFileDesc *out, SECItem *item, 
                           char *msg, int level);
#else
typedef int (*SECU_PPFunc)(FILE *out, SECItem *item, char *msg, int level);
#endif

typedef struct {
    enum {
	PW_NONE = 0,
	PW_FROMFILE = 1,
	PW_PLAINTEXT = 2
    } source;
    char *data;
} secuPWData;

/*
** Change a password on a token, or initialize a token with a password
** if it does not already have one.
** Use passwd to send the password in plaintext, pwFile to specify a
** file containing the password, or NULL for both to prompt the user.
*/
SECStatus SECU_ChangePW(PK11SlotInfo *slot, char *passwd, char *pwFile);

/*  These were stolen from the old sec.h... */
/*
** Check a password for legitimacy. Passwords must be at least 8
** characters long and contain one non-alphabetic. Return DSTrue if the
** password is ok, DSFalse otherwise.
*/
extern PRBool SEC_CheckPassword(char *password);

/*
** Blind check of a password. Complement to SEC_CheckPassword which 
** ignores length and content type, just retuning DSTrue is the password
** exists, DSFalse if NULL
*/
extern PRBool SEC_BlindCheckPassword(char *password);

/*
** Get a password.
** First prompt with "msg" on "out", then read the password from "in".
** The password is then checked using "chkpw".
*/
extern char *SEC_GetPassword(FILE *in, FILE *out, char *msg,
				      PRBool (*chkpw)(char *));

char *SECU_FilePasswd(PK11SlotInfo *slot, PRBool retry, void *arg);

char *SECU_GetPasswordString(void *arg, char *prompt);

/*
** Write a dongle password.
** Uses MD5 to hash constant system data (hostname, etc.), and then
** creates RC4 key to encrypt a password "pw" into a file "fd".
*/
extern SECStatus SEC_WriteDongleFile(int fd, char *pw);

/*
** Get a dongle password.
** Uses MD5 to hash constant system data (hostname, etc.), and then
** creates RC4 key to decrypt and return a password from file "fd".
*/
extern char *SEC_ReadDongleFile(int fd);


/* End stolen headers */


/* Get the Key ID (modulus) from the cert with the given nickname. */
extern SECItem * SECU_GetKeyIDFromNickname(char *name);

/* Change the key db password in the database */
extern SECStatus SECU_ChangeKeyDBPassword(SECKEYKeyDBHandle *kdbh);

/* Check if a key name exists. Return PR_TRUE if true, PR_FALSE if not */
extern PRBool SECU_CheckKeyNameExists(SECKEYKeyDBHandle *handle, char *nickname);

/* Find a key by a nickname. Calls SECKEY_FindKeyByName */
extern SECKEYLowPrivateKey *SECU_GetPrivateKey(SECKEYKeyDBHandle *kdbh, char *nickname);

/* Get key encrypted with dongle file in "pathname" */
extern SECKEYLowPrivateKey *SECU_GetPrivateDongleKey(SECKEYKeyDBHandle *handle, 
                                               char *nickname, char *pathname);

extern SECItem *SECU_GetPassword(void *arg, SECKEYKeyDBHandle *handle);

/* Just sticks the two strings together with a / if needed */
char *SECU_AppendFilenameToDir(char *dir, char *filename);

/* Returns result of getenv("SSL_DIR") or NULL */
extern char *SECU_DefaultSSLDir(void);

/*
** Should be called once during initialization to set the default 
**    directory for looking for cert.db, key.db, and cert-nameidx.db files
** Removes trailing '/' in 'base' 
** If 'base' is NULL, defaults to set to .netscape in home directory.
*/
extern char *SECU_ConfigDirectory(const char* base);


extern char *SECU_CertDBNameCallback(void *arg, int dbVersion);
extern char *SECU_KeyDBNameCallback(void *arg, int dbVersion);

extern SECKEYPrivateKey *SECU_FindPrivateKeyFromNickname(char *name);
extern SECKEYLowPrivateKey *SECU_FindLowPrivateKeyFromNickname(char *name);
extern SECStatus SECU_DeleteKeyByName(SECKEYKeyDBHandle *handle, char *nickname);

extern SECKEYKeyDBHandle *SECU_OpenKeyDB(PRBool readOnly);
extern CERTCertDBHandle *SECU_OpenCertDB(PRBool readOnly);

/* 
** Basic callback function for SSL_GetClientAuthDataHook
*/
extern int
SECU_GetClientAuthData(void *arg, PRFileDesc *fd,
		       struct CERTDistNamesStr *caNames,
		       struct CERTCertificateStr **pRetCert,
		       struct SECKEYPrivateKeyStr **pRetKey);

/* print out an error message */
extern void SECU_PrintError(char *progName, char *msg, ...);

/* print out a system error message */
extern void SECU_PrintSystemError(char *progName, char *msg, ...);

/* Return informative error string */
extern const char * SECU_Strerror(PRErrorCode errNum);

/* Read the contents of a file into a SECItem */
extern SECStatus SECU_FileToItem(SECItem *dst, PRFileDesc *src);
extern SECStatus SECU_TextFileToItem(SECItem *dst, PRFileDesc *src);

/* Read in a DER from a file, may be ascii  */
extern SECStatus 
SECU_ReadDERFromFile(SECItem *der, PRFileDesc *inFile, PRBool ascii);

/* Indent based on "level" */
extern void SECU_Indent(FILE *out, int level);

/* Print integer value and hex */
extern void SECU_PrintInteger(FILE *out, SECItem *i, char *m, int level);

/* Print ObjectIdentifier symbolically */
extern void SECU_PrintObjectID(FILE *out, SECItem *oid, char *m, int level);

/* Print AlgorithmIdentifier symbolically */
extern void SECU_PrintAlgorithmID(FILE *out, SECAlgorithmID *a, char *m,
				  int level);

/* Print SECItem as hex */
extern void SECU_PrintAsHex(FILE *out, SECItem *i, char *m, int level);

/* dump a buffer in hex and ASCII */
extern void SECU_PrintBuf(FILE *out, const char *msg, const void *vp, int len);

/*
 * Format and print the UTC Time "t".  If the tag message "m" is not NULL,
 * do indent formatting based on "level" and add a newline afterward;
 * otherwise just print the formatted time string only.
 */
extern void SECU_PrintUTCTime(FILE *out, SECItem *t, char *m, int level);

/*
 * Format and print the Generalized Time "t".  If the tag message "m"
 * is not NULL, * do indent formatting based on "level" and add a newline
 * afterward; otherwise just print the formatted time string only.
 */
extern void SECU_PrintGeneralizedTime(FILE *out, SECItem *t, char *m,
				      int level);

/* Dump all key nicknames */
extern int SECU_PrintKeyNames(SECKEYKeyDBHandle *handle, FILE *out);

/* callback for listing certs through pkcs11 */
extern SECStatus SECU_PrintCertNickname(CERTCertificate *cert, void *data);

/* Dump all certificate nicknames in a database */
extern SECStatus
SECU_PrintCertificateNames(CERTCertDBHandle *handle, PRFileDesc* out, 
                           PRBool sortByName, PRBool sortByTrust);

/* See if nickname already in database. Return 1 true, 0 false, -1 error */
int SECU_CheckCertNameExists(CERTCertDBHandle *handle, char *nickname);

/* Dump contents of cert req */
extern int SECU_PrintCertificateRequest(FILE *out, SECItem *der, char *m,
	int level);

/* Dump contents of certificate */
extern int SECU_PrintCertificate(FILE *out, SECItem *der, char *m, int level);

/* print trust flags on a cert */
extern void SECU_PrintTrustFlags(FILE *out, CERTCertTrust *trust, char *m, int level);

/* Dump contents of public key */
extern int SECU_PrintPublicKey(FILE *out, SECItem *der, char *m, int level);

/* Dump contents of private key */
extern int SECU_PrintPrivateKey(FILE *out, SECItem *der, char *m, int level);

/* Print the MD5 and SHA1 fingerprints of a cert */
extern int SECU_PrintFingerprints(FILE *out, SECItem *derCert, char *m,
                                  int level);

/* Pretty-print any PKCS7 thing */
extern int SECU_PrintPKCS7ContentInfo(FILE *out, SECItem *der, char *m, 
				      int level);

/* Init PKCS11 stuff */
extern SECStatus SECU_PKCS11Init(PRBool readOnly);

/* Dump contents of signed data */
extern int SECU_PrintSignedData(FILE *out, SECItem *der, char *m, int level,
				SECU_PPFunc inner);

extern int SECU_PrintCrl(FILE *out, SECItem *der, char *m, int level);

extern void
SECU_PrintCRLInfo(FILE *out, CERTCrl *crl, char *m, int level);

extern void SECU_PrintExtensions(FILE *out, CERTCertExtension **extensions,
				 char *msg, int level);

extern void SECU_PrintName(FILE *out, CERTName *name, char *msg, int level);

/* Convert a High public Key to a Low public Key */
extern SECKEYLowPublicKey *SECU_ConvHighToLow(SECKEYPublicKey *pubHighKey);

extern SECItem *SECU_GetPBEPassword(void *arg);

extern char *SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg);

extern SECStatus DER_PrettyPrint(FILE *out, SECItem *it, PRBool raw);
extern void SEC_Init(void);

extern char *SECU_SECModDBName(void);

extern void SECU_PrintPRandOSError(char *progName);

/*
 *
 *  Utilities for parsing security tools command lines 
 *
 */

/*  A single command flag  */
typedef struct {
    char flag;
    PRBool needsArg;
    char *arg;
    PRBool activated;
} secuCommandFlag;

/*  A full array of command/option flags  */
typedef struct
{
    int numCommands;
    int numOptions;

    secuCommandFlag *commands;
    secuCommandFlag *options;
} secuCommand;

/*  fill the "arg" and "activated" fields for each flag  */
SECStatus 
SECU_ParseCommandLine(int argc, char **argv, char *progName, secuCommand *cmd);
char *
SECU_GetOptionArg(secuCommand *cmd, int optionNum);

/*
 *
 *  Error messaging
 *
 */

/* Return informative error string */
char *SECU_ErrorString(int16 err);

/* Return informative error string. Does not call XP_GetString */
char *SECU_ErrorStringRaw(int16 err);

void printflags(char *trusts, unsigned int flags);

#ifndef XP_UNIX
extern int ffs(unsigned int i);
#endif

#include "secerr.h"
#include "sslerr.h"

#endif /* _SEC_UTIL_H_ */
