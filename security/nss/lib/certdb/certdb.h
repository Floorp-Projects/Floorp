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

#ifndef _CERTDB_H_
#define _CERTDB_H_

#include "plarena.h"
#include "prlong.h"
/*
 * Certificate Database related definitions and data structures
 */

/* version number of certificate database */
#define CERT_DB_FILE_VERSION		7
#ifdef USE_NS_ROOTS
#define CERT_DB_CONTENT_VERSION		28
#else
#define CERT_DB_CONTENT_VERSION		2
#endif

#define SEC_DB_ENTRY_HEADER_LEN		3
#define SEC_DB_KEY_HEADER_LEN		1

/* All database entries have this form:
 * 	
 *	byte offset	field
 *	-----------	-----
 *	0		version
 *	1		type
 *	2		flags
 */

/* database entry types */
typedef enum {
    certDBEntryTypeVersion = 0,
    certDBEntryTypeCert = 1,
    certDBEntryTypeNickname = 2,
    certDBEntryTypeSubject = 3,
    certDBEntryTypeRevocation = 4,
    certDBEntryTypeKeyRevocation = 5,
    certDBEntryTypeSMimeProfile = 6,
    certDBEntryTypeContentVersion = 7
} certDBEntryType;

typedef struct {
    certDBEntryType type;
    unsigned int version;
    unsigned int flags;
    PRArenaPool *arena;
} certDBEntryCommon;

/*
 * Certificate entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	0		sslFlags-msb
 *	1		sslFlags-lsb
 *	2		emailFlags-msb
 *	3		emailFlags-lsb
 *	4		objectSigningFlags-msb
 *	5		objectSigningFlags-lsb
 *	6		derCert-len-msb
 *	7		derCert-len-lsb
 *	8		nickname-len-msb
 *	9		nickname-len-lsb
 *	...		derCert
 *	...		nickname
 *
 * NOTE: the nickname string as stored in the database is null terminated,
 *		in other words, the last byte of the db entry is always 0
 *		if a nickname is present.
 * NOTE: if nickname is not present, then nickname-len-msb and
 *		nickname-len-lsb will both be zero.
 */
struct _certDBEntryCert {
    certDBEntryCommon common;
    CERTCertTrust trust;
    SECItem derCert;
    char *nickname;
};

/*
 * Certificate Nickname entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	0		subjectname-len-msb
 *	1	        subjectname-len-lsb
 *	2...		subjectname
 *
 * The database key for this type of entry is a nickname string
 * The "subjectname" value is the DER encoded DN of the identity
 *   that matches this nickname.
 */
typedef struct {
    certDBEntryCommon common;
    char *nickname;
    SECItem subjectName;
} certDBEntryNickname;

#define DB_NICKNAME_ENTRY_HEADER_LEN 2

/*
 * Certificate Subject entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	0		ncerts-msb
 *	1		ncerts-lsb
 *	2		nickname-msb
 *	3		nickname-lsb
 *	4		emailAddr-msb
 *	5		emailAddr-lsb
 *	...		nickname
 *	...		emailAddr
 *	...+2*i		certkey-len-msb
 *	...+1+2*i       certkey-len-lsb
 *	...+2*ncerts+2*i keyid-len-msb
 *	...+1+2*ncerts+2*i keyid-len-lsb
 *	...		certkeys
 *	...		keyids
 *
 * The database key for this type of entry is the DER encoded subject name
 * The "certkey" value is an array of  certificate database lookup keys that
 *   points to the database entries for the certificates that matche
 *   this subject.
 *
 */
typedef struct _certDBEntrySubject {
    certDBEntryCommon common;
    SECItem derSubject;
    unsigned int ncerts;
    char *nickname;
    char *emailAddr;
    SECItem *certKeys;
    SECItem *keyIDs;
} certDBEntrySubject;

#define DB_SUBJECT_ENTRY_HEADER_LEN 6

/*
 * Certificate SMIME profile entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	0		subjectname-len-msb
 *	1	        subjectname-len-lsb
 *	2		smimeoptions-len-msb
 *	3		smimeoptions-len-lsb
 *	4		options-date-len-msb
 *	5		options-date-len-lsb
 *	6...		subjectname
 *	...		smimeoptions
 *	...		options-date
 *
 * The database key for this type of entry is the email address string
 * The "subjectname" value is the DER encoded DN of the identity
 *   that matches this nickname.
 * The "smimeoptions" value is a string that represents the algorithm
 *   capabilities on the remote user.
 * The "options-date" is the date that the smime options value was created.
 *   This is generally the signing time of the signed message that contained
 *   the options.  It is a UTCTime value.
 */
typedef struct {
    certDBEntryCommon common;
    char *emailAddr;
    SECItem subjectName;
    SECItem smimeOptions;
    SECItem optionsDate;
} certDBEntrySMime;

#define DB_SMIME_ENTRY_HEADER_LEN 6

/*
 * Crl/krl entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	0		derCert-len-msb
 *	1		derCert-len-lsb
 *	2		url-len-msb
 *	3		url-len-lsb
 *	...		derCert
 *	...		url
 *
 * NOTE: the url string as stored in the database is null terminated,
 *		in other words, the last byte of the db entry is always 0
 *		if a nickname is present. 
 * NOTE: if url is not present, then url-len-msb and
 *		url-len-lsb will both be zero.
 */
#define DB_CRL_ENTRY_HEADER_LEN	4
struct _certDBEntryRevocation {
    certDBEntryCommon common;
    SECItem	derCrl;
    char	*url;	/* where to load the crl from */
};

/*
 * Database Version Entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	only the low level header...
 *
 * The database key for this type of entry is the string "Version"
 */
typedef struct {
    certDBEntryCommon common;
} certDBEntryVersion;

#define SEC_DB_VERSION_KEY "Version"
#define SEC_DB_VERSION_KEY_LEN sizeof(SEC_DB_VERSION_KEY)

/*
 * Database Content Version Entry:
 *
 *	byte offset	field
 *	-----------	-----
 *	0		contentVersion
 *
 * The database key for this type of entry is the string "ContentVersion"
 */
typedef struct {
    certDBEntryCommon common;
    char contentVersion;
} certDBEntryContentVersion;

#define SEC_DB_CONTENT_VERSION_KEY "ContentVersion"
#define SEC_DB_CONTENT_VERSION_KEY_LEN sizeof(SEC_DB_CONTENT_VERSION_KEY)

typedef union {
    certDBEntryCommon common;
    certDBEntryVersion version;
    certDBEntryCert cert;
    certDBEntryNickname nickname;
    certDBEntrySubject subject;
    certDBEntryRevocation revocation;
} certDBEntry;

/* length of the fixed part of a database entry */
#define DBCERT_V4_HEADER_LEN	7
#define DB_CERT_V5_ENTRY_HEADER_LEN	7
#define DB_CERT_V6_ENTRY_HEADER_LEN	7
#define DB_CERT_ENTRY_HEADER_LEN	10

/* common flags for all types of certificates */
#define CERTDB_VALID_PEER	(1<<0)
#define CERTDB_TRUSTED		(1<<1)
#define CERTDB_SEND_WARN	(1<<2)
#define CERTDB_VALID_CA		(1<<3)
#define CERTDB_TRUSTED_CA	(1<<4) /* trusted for issuing server certs */
#define CERTDB_NS_TRUSTED_CA	(1<<5)
#define CERTDB_USER		(1<<6)
#define CERTDB_TRUSTED_CLIENT_CA (1<<7) /* trusted for issuing client certs */
#define CERTDB_INVISIBLE_CA	(1<<8) /* don't show in UI */
#define CERTDB_GOVT_APPROVED_CA	(1<<9) /* can do strong crypto in export ver */

SEC_BEGIN_PROTOS

/*
** Add a DER encoded certificate to the permanent database.
**	"derCert" is the DER encoded certificate.
**	"nickname" is the nickname to use for the cert
**	"trust" is the trust parameters for the cert
*/
SECStatus SEC_AddPermCertificate(CERTCertDBHandle *handle, SECItem *derCert,
				char *nickname, CERTCertTrust *trust);

certDBEntryCert *
SEC_FindPermCertByKey(CERTCertDBHandle *handle, SECItem *certKey);

certDBEntryCert
*SEC_FindPermCertByName(CERTCertDBHandle *handle, SECItem *name);

SECStatus SEC_OpenPermCertDB(CERTCertDBHandle *handle,
			     PRBool readOnly,
			     CERTDBNameFunc namecb,
			     void *cbarg);

SECStatus SEC_DeletePermCertificate(CERTCertificate *cert);

typedef SECStatus (PR_CALLBACK * PermCertCallback)(CERTCertificate *cert,
                                                   SECItem *k, void *pdata);
/*
** Traverse the entire permanent database, and pass the certs off to a
** user supplied function.
**	"certfunc" is the user function to call for each certificate
**	"udata" is the user's data, which is passed through to "certfunc"
*/
SECStatus
SEC_TraversePermCerts(CERTCertDBHandle *handle,
		      PermCertCallback certfunc,
		      void *udata );

SECStatus
SEC_AddTempNickname(CERTCertDBHandle *handle, char *nickname, SECItem *certKey);

SECStatus
SEC_DeleteTempNickname(CERTCertDBHandle *handle, char *nickname);

PRBool
SEC_CertNicknameConflict(char *nickname, SECItem *derSubject,
			 CERTCertDBHandle *handle);

PRBool
SEC_CertDBKeyConflict(SECItem *derCert, CERTCertDBHandle *handle);

SECStatus
SEC_GetCrlTimes(CERTCrl *dates, PRTime *notBefore, PRTime *notAfter);

SECCertTimeValidity
SEC_CheckCrlTimes(CERTCrl *crl, PRTime t);

PRBool
SEC_CrlIsNewer(CERTCrl *inNew, CERTCrl *old);

CERTSignedCrl *
SEC_AddPermCrlToTemp(CERTCertDBHandle *handle, certDBEntryRevocation *entry);

SECStatus
SEC_DeleteTempCrl(CERTSignedCrl *crl);

CERTSignedCrl *
SEC_FindCrlByKey(CERTCertDBHandle *handle, SECItem *crlKey, int type);

CERTSignedCrl *
SEC_FindCrlByName(CERTCertDBHandle *handle, SECItem *crlKey, int type);

CERTSignedCrl *
SEC_FindCrlByDERCert(CERTCertDBHandle *handle, SECItem *derCrl, int type);

SECStatus 
SEC_DestroyCrl(CERTSignedCrl *crl);

CERTSignedCrl *
SEC_NewCrl(CERTCertDBHandle *handle, char *url, SECItem *derCrl, int type);

CERTSignedCrl *
cert_DBInsertCRL
   (CERTCertDBHandle *handle, char *url,
    CERTSignedCrl *newCrl, SECItem *derCrl, int type);

SECStatus
SEC_CheckKRL(CERTCertDBHandle *handle,SECKEYPublicKey *key,
	     CERTCertificate *rootCert, int64 t, void *wincx);

SECStatus
SEC_CheckCRL(CERTCertDBHandle *handle,CERTCertificate *cert,
	     CERTCertificate *caCert, int64 t, void *wincx);

SECStatus
SEC_DeletePermCRL(CERTSignedCrl *crl);


SECStatus
SEC_LookupCrls(CERTCertDBHandle *handle, CERTCrlHeadNode **nodes, int type);

SECStatus
SEC_CrlReplaceUrl(CERTSignedCrl *crl,char *url);

SEC_END_PROTOS

#endif /* _CERTDB_H_ */
