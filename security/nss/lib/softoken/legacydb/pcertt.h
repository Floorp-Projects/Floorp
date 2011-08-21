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
/*
 * certt.h - public data structures for the certificate library
 *
 * $Id: pcertt.h,v 1.4 2011/04/13 00:10:27 rrelyea%redhat.com Exp $
 */
#ifndef _PCERTT_H_
#define _PCERTT_H_

#include "prclist.h"
#include "pkcs11t.h"
#include "seccomon.h"
#include "secoidt.h"
#include "plarena.h"
#include "prcvar.h"
#include "nssilock.h"
#include "prio.h"
#include "prmon.h"

/* Non-opaque objects */
typedef struct NSSLOWCERTCertDBHandleStr               NSSLOWCERTCertDBHandle;
typedef struct NSSLOWCERTCertKeyStr                    NSSLOWCERTCertKey;

typedef struct NSSLOWCERTTrustStr                      NSSLOWCERTTrust;
typedef struct NSSLOWCERTCertTrustStr                  NSSLOWCERTCertTrust;
typedef struct NSSLOWCERTCertificateStr                NSSLOWCERTCertificate;
typedef struct NSSLOWCERTCertificateListStr            NSSLOWCERTCertificateList;
typedef struct NSSLOWCERTIssuerAndSNStr                NSSLOWCERTIssuerAndSN;
typedef struct NSSLOWCERTSignedDataStr                 NSSLOWCERTSignedData;
typedef struct NSSLOWCERTSubjectPublicKeyInfoStr       NSSLOWCERTSubjectPublicKeyInfo;
typedef struct NSSLOWCERTValidityStr                   NSSLOWCERTValidity;

/*
** An X.509 validity object
*/
struct NSSLOWCERTValidityStr {
    PRArenaPool *arena;
    SECItem notBefore;
    SECItem notAfter;
};

/*
 * A serial number and issuer name, which is used as a database key
 */
struct NSSLOWCERTCertKeyStr {
    SECItem serialNumber;
    SECItem derIssuer;
};

/*
** A signed data object. Used to implement the "signed" macro used
** in the X.500 specs.
*/
struct NSSLOWCERTSignedDataStr {
    SECItem data;
    SECAlgorithmID signatureAlgorithm;
    SECItem signature;
};

/*
** An X.509 subject-public-key-info object
*/
struct NSSLOWCERTSubjectPublicKeyInfoStr {
    PRArenaPool *arena;
    SECAlgorithmID algorithm;
    SECItem subjectPublicKey;
};

typedef struct _certDBEntryCert certDBEntryCert;
typedef struct _certDBEntryRevocation certDBEntryRevocation;

struct NSSLOWCERTCertTrustStr {
    unsigned int sslFlags;
    unsigned int emailFlags;
    unsigned int objectSigningFlags;
};

/*
** PKCS11 Trust representation
*/
struct NSSLOWCERTTrustStr {
    NSSLOWCERTTrust *next;
    NSSLOWCERTCertDBHandle *dbhandle;
    SECItem dbKey;			/* database key for this cert */
    certDBEntryCert *dbEntry;		/* database entry struct */
    NSSLOWCERTCertTrust *trust;
    SECItem *derCert;			/* original DER for the cert */
    unsigned char dbKeySpace[512];
};

/*
** An X.509 certificate object (the unsigned form)
*/
struct NSSLOWCERTCertificateStr {
    /* the arena is used to allocate any data structures that have the same
     * lifetime as the cert.  This is all stuff that hangs off of the cert
     * structure, and is all freed at the same time.  I is used when the
     * cert is decoded, destroyed, and at some times when it changes
     * state
     */
    NSSLOWCERTCertificate *next;
    NSSLOWCERTCertDBHandle *dbhandle;

    SECItem derCert;			/* original DER for the cert */
    SECItem derIssuer;			/* DER for issuer name */
    SECItem derSN;
    SECItem serialNumber;
    SECItem derSubject;			/* DER for subject name */
    SECItem derSubjKeyInfo;
    NSSLOWCERTSubjectPublicKeyInfo *subjectPublicKeyInfo;
    SECItem certKey;			/* database key for this cert */
    SECItem validity;
    certDBEntryCert *dbEntry;		/* database entry struct */
    SECItem subjectKeyID;	/* x509v3 subject key identifier */
    SECItem extensions;
    char *nickname;
    char *emailAddr;
    NSSLOWCERTCertTrust *trust;

    /* the reference count is modified whenever someone looks up, dups
     * or destroys a certificate
     */
    int referenceCount;

    char nicknameSpace[200];
    char emailAddrSpace[200];
    unsigned char certKeySpace[512];
};

#define SEC_CERTIFICATE_VERSION_1		0	/* default created */
#define SEC_CERTIFICATE_VERSION_2		1	/* v2 */
#define SEC_CERTIFICATE_VERSION_3		2	/* v3 extensions */

#define SEC_CRL_VERSION_1		0	/* default */
#define SEC_CRL_VERSION_2		1	/* v2 extensions */

#define NSS_MAX_LEGACY_DB_KEY_SIZE (60 * 1024)

struct NSSLOWCERTIssuerAndSNStr {
    SECItem derIssuer;
    SECItem serialNumber;
};

typedef SECStatus (* NSSLOWCERTCertCallback)(NSSLOWCERTCertificate *cert, void *arg);

/* This is the typedef for the callback passed to nsslowcert_OpenCertDB() */
/* callback to return database name based on version number */
typedef char * (*NSSLOWCERTDBNameFunc)(void *arg, int dbVersion);

/* XXX Lisa thinks the template declarations belong in cert.h, not here? */

#include "secasn1t.h"	/* way down here because I expect template stuff to
			 * move out of here anyway */

/*
 * Certificate Database related definitions and data structures
 */

/* version number of certificate database */
#define CERT_DB_FILE_VERSION		8
#define CERT_DB_V7_FILE_VERSION		7
#define CERT_DB_CONTENT_VERSION		2

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
    certDBEntryTypeContentVersion = 7,
    certDBEntryTypeBlob = 8
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
    certDBEntryCert *next;
    NSSLOWCERTCertTrust trust;
    SECItem derCert;
    char *nickname;
    char nicknameSpace[200];
    unsigned char derCertSpace[2048];
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
    SECItem *certKeys;
    SECItem *keyIDs;
    char **emailAddrs;
    unsigned int nemailAddrs;
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
    certDBEntryCommon         common;
    certDBEntryCert           cert;
    certDBEntryContentVersion content;
    certDBEntryNickname       nickname;
    certDBEntryRevocation     revocation;
    certDBEntrySMime          smime;
    certDBEntrySubject        subject;
    certDBEntryVersion        version;
} certDBEntry;

/* length of the fixed part of a database entry */
#define DBCERT_V4_HEADER_LEN	7
#define DB_CERT_V5_ENTRY_HEADER_LEN	7
#define DB_CERT_V6_ENTRY_HEADER_LEN	7
#define DB_CERT_ENTRY_HEADER_LEN	10

/* common flags for all types of certificates */
#define CERTDB_TERMINAL_RECORD	(1<<0)
#define CERTDB_TRUSTED		(1<<1)
#define CERTDB_SEND_WARN	(1<<2)
#define CERTDB_VALID_CA		(1<<3)
#define CERTDB_TRUSTED_CA	(1<<4) /* trusted for issuing server certs */
#define CERTDB_NS_TRUSTED_CA	(1<<5)
#define CERTDB_USER		(1<<6)
#define CERTDB_TRUSTED_CLIENT_CA (1<<7) /* trusted for issuing client certs */
#define CERTDB_INVISIBLE_CA	(1<<8) /* don't show in UI */
#define CERTDB_GOVT_APPROVED_CA	(1<<9) /* can do strong crypto in export ver */
#define CERTDB_MUST_VERIFY	(1<<10) /* explicitly don't trust this cert */
#define CERTDB_TRUSTED_UNKNOWN	(1<<11) /* accept trust from another source */

/* bits not affected by the CKO_NETSCAPE_TRUST object */
#define CERTDB_PRESERVE_TRUST_BITS (CERTDB_USER | \
        CERTDB_NS_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_INVISIBLE_CA | \
                                        CERTDB_GOVT_APPROVED_CA)

#endif /* _PCERTT_H_ */
