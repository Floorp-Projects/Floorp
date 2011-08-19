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
#define SEC_CT_NAME			"name"

#define NS_CERTREQ_HEADER "-----BEGIN NEW CERTIFICATE REQUEST-----"
#define NS_CERTREQ_TRAILER "-----END NEW CERTIFICATE REQUEST-----"

#define NS_CERT_HEADER "-----BEGIN CERTIFICATE-----"
#define NS_CERT_TRAILER "-----END CERTIFICATE-----"

#define NS_CRL_HEADER  "-----BEGIN CRL-----"
#define NS_CRL_TRAILER "-----END CRL-----"

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
	PW_PLAINTEXT = 2,
	PW_EXTERNAL = 3
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

/*
** Change a password on a token, or initialize a token with a password
** if it does not already have one.
** In this function, you can specify both the old and new passwords
** as either a string or file. NOTE: any you don't specify will
** be prompted for
*/
SECStatus SECU_ChangePW2(PK11SlotInfo *slot, char *oldPass, char *newPass,
                        char *oldPwFile, char *newPwFile);

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

/* revalidate the cert and print information about cert verification
 * failure at time == now */
extern void
SECU_printCertProblems(FILE *outfile, CERTCertDBHandle *handle, 
	CERTCertificate *cert, PRBool checksig, 
	SECCertificateUsage certUsage, void *pinArg, PRBool verbose);

/* revalidate the cert and print information about cert verification
 * failure at specified time */
extern void
SECU_printCertProblemsOnDate(FILE *outfile, CERTCertDBHandle *handle, 
	CERTCertificate *cert, PRBool checksig, SECCertificateUsage certUsage, 
	void *pinArg, PRBool verbose, PRTime datetime);

/* print out CERTVerifyLog info. */
extern void
SECU_displayVerifyLog(FILE *outfile, CERTVerifyLog *log,
                      PRBool verbose);

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
extern SECOidTag SECU_PrintObjectID(FILE *out, SECItem *oid, char *m, int level);

/* Print AlgorithmIdentifier symbolically */
extern void SECU_PrintAlgorithmID(FILE *out, SECAlgorithmID *a, char *m,
				  int level);

/* Print SECItem as hex */
extern void SECU_PrintAsHex(FILE *out, SECItem *i, const char *m, int level);

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

/*
 * Format and print the UTC or Generalized Time "t".  If the tag message
 * "m" is not NULL, do indent formatting based on "level" and add a newline
 * afterward; otherwise just print the formatted time string only.
 */
extern void SECU_PrintTimeChoice(FILE *out, SECItem *t, char *m, int level);

/* callback for listing certs through pkcs11 */
extern SECStatus SECU_PrintCertNickname(CERTCertListNode* cert, void *data);

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

/* Dump contents of a DER certificate name (issuer or subject) */
extern int SECU_PrintDERName(FILE *out, SECItem *der, const char *m, int level);

/* print trust flags on a cert */
extern void SECU_PrintTrustFlags(FILE *out, CERTCertTrust *trust, char *m, 
                                 int level);

/* Dump contents of an RSA public key */
extern int SECU_PrintRSAPublicKey(FILE *out, SECItem *der, char *m, int level);

extern int SECU_PrintSubjectPublicKeyInfo(FILE *out, SECItem *der, char *m, 
                                          int level);

#ifdef HAVE_EPV_TEMPLATE
/* Dump contents of private key */
extern int SECU_PrintPrivateKey(FILE *out, SECItem *der, char *m, int level);
#endif

/* Print the MD5 and SHA1 fingerprints of a cert */
extern int SECU_PrintFingerprints(FILE *out, SECItem *derCert, char *m,
                                  int level);

/* Pretty-print any PKCS7 thing */
extern int SECU_PrintPKCS7ContentInfo(FILE *out, SECItem *der, char *m, 
				      int level);

/* Init PKCS11 stuff */
extern SECStatus SECU_PKCS11Init(PRBool readOnly);

/* Dump contents of signed data */
extern int SECU_PrintSignedData(FILE *out, SECItem *der, const char *m, 
                                int level, SECU_PPFunc inner);

/* Print cert data and its trust flags */
extern SECStatus SEC_PrintCertificateAndTrust(CERTCertificate *cert,
                                              const char *label,
                                              CERTCertTrust *trust);

extern int SECU_PrintCrl(FILE *out, SECItem *der, char *m, int level);

extern void
SECU_PrintCRLInfo(FILE *out, CERTCrl *crl, char *m, int level);

extern void SECU_PrintString(FILE *out, SECItem *si, char *m, int level);
extern void SECU_PrintAny(FILE *out, SECItem *i, char *m, int level);

extern void SECU_PrintPolicy(FILE *out, SECItem *value, char *msg, int level);
extern void SECU_PrintPrivKeyUsagePeriodExtension(FILE *out, SECItem *value,
                                 char *msg, int level);

extern void SECU_PrintExtensions(FILE *out, CERTCertExtension **extensions,
				 char *msg, int level);

extern void SECU_PrintName(FILE *out, CERTName *name, const char *msg,
                           int level);
extern void SECU_PrintRDN(FILE *out, CERTRDN *rdn, const char *msg, int level);

#ifdef SECU_GetPassword
/* Convert a High public Key to a Low public Key */
extern SECKEYLowPublicKey *SECU_ConvHighToLow(SECKEYPublicKey *pubHighKey);
#endif

extern char *SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg);

extern SECStatus DER_PrettyPrint(FILE *out, SECItem *it, PRBool raw);

extern char *SECU_SECModDBName(void);

extern void SECU_PrintPRandOSError(char *progName);

extern SECStatus SECU_RegisterDynamicOids(void);

/* Identifies hash algorithm tag by its string representation. */
extern SECOidTag SECU_StringToSignatureAlgTag(const char *alg);

/* Store CRL in output file or pk11 db. Also
 * encodes with base64 and exports to file if ascii flag is set
 * and file is not NULL. */
extern SECStatus SECU_StoreCRL(PK11SlotInfo *slot, SECItem *derCrl,
                               PRFileDesc *outFile, PRBool ascii, char *url);


/*
** DER sign a single block of data using private key encryption and the
** MD5 hashing algorithm. This routine first computes a digital signature
** using SEC_SignData, then wraps it with an CERTSignedData and then der
** encodes the result.
**	"arena" is the memory arena to use to allocate data from
**      "sd" returned CERTSignedData 
** 	"result" the final der encoded data (memory is allocated)
** 	"buf" the input data to sign
** 	"len" the amount of data to sign
** 	"pk" the private key to encrypt with
*/
extern SECStatus SECU_DerSignDataCRL(PRArenaPool *arena, CERTSignedData *sd,
                                     unsigned char *buf, int len,
                                     SECKEYPrivateKey *pk, SECOidTag algID);

typedef enum  {
    noKeyFound = 1,
    noSignatureMatch = 2,
    failToEncode = 3,
    failToSign = 4,
    noMem = 5
} SignAndEncodeFuncExitStat;

extern SECStatus
SECU_SignAndEncodeCRL(CERTCertificate *issuer, CERTSignedCrl *signCrl,
                      SECOidTag hashAlgTag, SignAndEncodeFuncExitStat *resCode);

extern SECStatus
SECU_CopyCRL(PRArenaPool *destArena, CERTCrl *destCrl, CERTCrl *srcCrl);

/*
** Finds the crl Authority Key Id extension. Returns NULL if no such extension
** was found.
*/
CERTAuthKeyID *
SECU_FindCRLAuthKeyIDExten (PRArenaPool *arena, CERTSignedCrl *crl);

/*
 * Find the issuer of a crl. Cert usage should be checked before signing a crl.
 */
CERTCertificate *
SECU_FindCrlIssuer(CERTCertDBHandle *dbHandle, SECItem* subject,
                   CERTAuthKeyID* id, PRTime validTime);


/* call back function used in encoding of an extension. Called from
 * SECU_EncodeAndAddExtensionValue */
typedef SECStatus (* EXTEN_EXT_VALUE_ENCODER) (PRArenaPool *extHandleArena,
                                               void *value, SECItem *encodedValue);

/* Encodes and adds extensions to the CRL or CRL entries. */
SECStatus 
SECU_EncodeAndAddExtensionValue(PRArenaPool *arena, void *extHandle, 
                                void *value, PRBool criticality, int extenType, 
                                EXTEN_EXT_VALUE_ENCODER EncodeValueFn);

/* Caller ensures that dst is at least item->len*2+1 bytes long */
void
SECU_SECItemToHex(const SECItem * item, char * dst);

/* Requires 0x prefix. Case-insensitive. Will do in-place replacement if
 * successful */
SECStatus
SECU_SECItemHexStringToBinary(SECItem* srcdest);

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
    char *longform;
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
SECU_ParseCommandLine(int argc, char **argv, char *progName,
		      const secuCommand *cmd);
char *
SECU_GetOptionArg(const secuCommand *cmd, int optionNum);

/*
 *
 *  Error messaging
 *
 */

void printflags(char *trusts, unsigned int flags);

#if !defined(XP_UNIX) && !defined(XP_OS2)
extern int ffs(unsigned int i);
#endif

/* Finds certificate by searching it in the DB or by examinig file
 * in the local directory. */
CERTCertificate*
SECU_FindCertByNicknameOrFilename(CERTCertDBHandle *handle,
                                  char *name, PRBool ascii,
                                  void *pwarg);
#include "secerr.h"
#include "sslerr.h"

#endif /* _SEC_UTIL_H_ */
