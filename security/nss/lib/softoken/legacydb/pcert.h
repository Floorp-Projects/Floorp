/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PCERTDB_H_
#define _PCERTDB_H_

#include "plarena.h"
#include "prlong.h"
#include "pcertt.h"

#include "lowkeyti.h" /* for struct NSSLOWKEYPublicKeyStr */

SEC_BEGIN_PROTOS

/*
 * initialize any global certificate locks
 */
SECStatus nsslowcert_InitLocks(void);

/*
** Add a DER encoded certificate to the permanent database.
**  "derCert" is the DER encoded certificate.
**  "nickname" is the nickname to use for the cert
**  "trust" is the trust parameters for the cert
*/
SECStatus nsslowcert_AddPermCert(NSSLOWCERTCertDBHandle *handle,
                                 NSSLOWCERTCertificate *cert,
                                 char *nickname, NSSLOWCERTCertTrust *trust);
SECStatus nsslowcert_AddPermNickname(NSSLOWCERTCertDBHandle *dbhandle,
                                     NSSLOWCERTCertificate *cert, char *nickname);

SECStatus nsslowcert_DeletePermCertificate(NSSLOWCERTCertificate *cert);

typedef SECStatus(PR_CALLBACK *PermCertCallback)(NSSLOWCERTCertificate *cert,
                                                 SECItem *k, void *pdata);
/*
** Traverse the entire permanent database, and pass the certs off to a
** user supplied function.
**  "certfunc" is the user function to call for each certificate
**  "udata" is the user's data, which is passed through to "certfunc"
*/
SECStatus
nsslowcert_TraversePermCerts(NSSLOWCERTCertDBHandle *handle,
                             PermCertCallback certfunc,
                             void *udata);

PRBool
nsslowcert_CertDBKeyConflict(SECItem *derCert, NSSLOWCERTCertDBHandle *handle);

certDBEntryRevocation *
nsslowcert_FindCrlByKey(NSSLOWCERTCertDBHandle *handle,
                        SECItem *crlKey, PRBool isKRL);

SECStatus
nsslowcert_DeletePermCRL(NSSLOWCERTCertDBHandle *handle, const SECItem *derName,
                         PRBool isKRL);
SECStatus
nsslowcert_AddCrl(NSSLOWCERTCertDBHandle *handle, SECItem *derCrl,
                  SECItem *derKey, char *url, PRBool isKRL);

NSSLOWCERTCertDBHandle *nsslowcert_GetDefaultCertDB();
NSSLOWKEYPublicKey *nsslowcert_ExtractPublicKey(NSSLOWCERTCertificate *);

NSSLOWCERTCertificate *
nsslowcert_NewTempCertificate(NSSLOWCERTCertDBHandle *handle, SECItem *derCert,
                              char *nickname, PRBool isperm, PRBool copyDER);
NSSLOWCERTCertificate *
nsslowcert_DupCertificate(NSSLOWCERTCertificate *cert);
void nsslowcert_DestroyCertificate(NSSLOWCERTCertificate *cert);
void nsslowcert_DestroyTrust(NSSLOWCERTTrust *Trust);

/*
 * Lookup a certificate in the databases without locking
 *  "certKey" is the database key to look for
 *
 * XXX - this should be internal, but pkcs 11 needs to call it during a
 * traversal.
 */
NSSLOWCERTCertificate *
nsslowcert_FindCertByKey(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey);

/*
 * Lookup trust for a certificate in the databases without locking
 *  "certKey" is the database key to look for
 *
 * XXX - this should be internal, but pkcs 11 needs to call it during a
 * traversal.
 */
NSSLOWCERTTrust *
nsslowcert_FindTrustByKey(NSSLOWCERTCertDBHandle *handle, const SECItem *certKey);

/*
** Generate a certificate key from the issuer and serialnumber, then look it
** up in the database.  Return the cert if found.
**  "issuerAndSN" is the issuer and serial number to look for
*/
extern NSSLOWCERTCertificate *
nsslowcert_FindCertByIssuerAndSN(NSSLOWCERTCertDBHandle *handle, NSSLOWCERTIssuerAndSN *issuerAndSN);

/*
** Generate a certificate key from the issuer and serialnumber, then look it
** up in the database.  Return the cert if found.
**  "issuerAndSN" is the issuer and serial number to look for
*/
extern NSSLOWCERTTrust *
nsslowcert_FindTrustByIssuerAndSN(NSSLOWCERTCertDBHandle *handle, NSSLOWCERTIssuerAndSN *issuerAndSN);

/*
** Find a certificate in the database by a DER encoded certificate
**  "derCert" is the DER encoded certificate
*/
extern NSSLOWCERTCertificate *
nsslowcert_FindCertByDERCert(NSSLOWCERTCertDBHandle *handle, SECItem *derCert);

/* convert an email address to lower case */
char *nsslowcert_FixupEmailAddr(char *emailAddr);

/*
** Decode a DER encoded certificate into an NSSLOWCERTCertificate structure
**      "derSignedCert" is the DER encoded signed certificate
**      "copyDER" is true if the DER should be copied, false if the
**              existing copy should be referenced
**      "nickname" is the nickname to use in the database.  If it is NULL
**              then a temporary nickname is generated.
*/
extern NSSLOWCERTCertificate *
nsslowcert_DecodeDERCertificate(SECItem *derSignedCert, char *nickname);

SECStatus
nsslowcert_KeyFromDERCert(PLArenaPool *arena, SECItem *derCert, SECItem *key);

certDBEntrySMime *
nsslowcert_ReadDBSMimeEntry(NSSLOWCERTCertDBHandle *certHandle,
                            char *emailAddr);
void
nsslowcert_DestroyDBEntry(certDBEntry *entry);

SECStatus
nsslowcert_OpenCertDB(NSSLOWCERTCertDBHandle *handle, PRBool readOnly,
                      const char *domain, const char *prefix,
                      NSSLOWCERTDBNameFunc namecb, void *cbarg, PRBool openVolatile);

void
nsslowcert_ClosePermCertDB(NSSLOWCERTCertDBHandle *handle);

/*
 * is certa newer than certb?  If one is expired, pick the other one.
 */
PRBool
nsslowcert_IsNewer(NSSLOWCERTCertificate *certa, NSSLOWCERTCertificate *certb);

SECStatus
nsslowcert_TraverseDBEntries(NSSLOWCERTCertDBHandle *handle,
                             certDBEntryType type,
                             SECStatus (*callback)(SECItem *data, SECItem *key,
                                                   certDBEntryType type, void *pdata),
                             void *udata);
SECStatus
nsslowcert_TraversePermCertsForSubject(NSSLOWCERTCertDBHandle *handle,
                                       SECItem *derSubject,
                                       NSSLOWCERTCertCallback cb, void *cbarg);
int
nsslowcert_NumPermCertsForSubject(NSSLOWCERTCertDBHandle *handle,
                                  SECItem *derSubject);
SECStatus
nsslowcert_TraversePermCertsForNickname(NSSLOWCERTCertDBHandle *handle,
                                        char *nickname, NSSLOWCERTCertCallback cb, void *cbarg);

int
nsslowcert_NumPermCertsForNickname(NSSLOWCERTCertDBHandle *handle,
                                   char *nickname);
SECStatus
nsslowcert_GetCertTrust(NSSLOWCERTCertificate *cert,
                        NSSLOWCERTCertTrust *trust);

SECStatus
nsslowcert_SaveSMimeProfile(NSSLOWCERTCertDBHandle *dbhandle, char *emailAddr,
                            SECItem *derSubject, SECItem *emailProfile, SECItem *profileTime);

/*
 * Change the trust attributes of a certificate and make them permanent
 * in the database.
 */
SECStatus
nsslowcert_ChangeCertTrust(NSSLOWCERTCertDBHandle *handle,
                           NSSLOWCERTCertificate *cert, NSSLOWCERTCertTrust *trust);

PRBool
nsslowcert_needDBVerify(NSSLOWCERTCertDBHandle *handle);

void
nsslowcert_setDBVerify(NSSLOWCERTCertDBHandle *handle, PRBool value);

PRBool
nsslowcert_hasTrust(NSSLOWCERTCertTrust *trust);

void
nsslowcert_DestroyFreeLists(void);

void
nsslowcert_DestroyGlobalLocks(void);

void
pkcs11_freeNickname(char *nickname, char *space);

char *
pkcs11_copyNickname(char *nickname, char *space, int spaceLen);

void
pkcs11_freeStaticData(unsigned char *data, unsigned char *space);

unsigned char *
pkcs11_allocStaticData(int datalen, unsigned char *space, int spaceLen);

unsigned char *
pkcs11_copyStaticData(unsigned char *data, int datalen, unsigned char *space,
                      int spaceLen);
NSSLOWCERTCertificate *
nsslowcert_CreateCert(void);

certDBEntry *
nsslowcert_DecodeAnyDBEntry(SECItem *dbData, const SECItem *dbKey,
                            certDBEntryType entryType, void *pdata);

SEC_END_PROTOS

#endif /* _PCERTDB_H_ */
