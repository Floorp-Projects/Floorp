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
 * cert.h - public data structures and prototypes for the certificate library
 *
 * $Id: cert.h,v 1.47 2004/07/07 00:48:53 wchang0222%aol.com Exp $
 */

#ifndef _CERT_H_
#define _CERT_H_

#include "plarena.h"
#include "plhash.h"
#include "prlong.h"
#include "prlog.h"

#include "seccomon.h"
#include "secdert.h"
#include "secoidt.h"
#include "keyt.h"
#include "certt.h"

SEC_BEGIN_PROTOS
   
/****************************************************************************
 *
 * RFC1485 ascii to/from X.? RelativeDistinguishedName (aka CERTName)
 *
 ****************************************************************************/

/*
** Convert an ascii RFC1485 encoded name into its CERTName equivalent.
*/
extern CERTName *CERT_AsciiToName(char *string);

/*
** Convert an CERTName into its RFC1485 encoded equivalent.
** Returns a string that must be freed with PORT_Free().
*/
extern char *CERT_NameToAscii(CERTName *name);

extern CERTAVA *CERT_CopyAVA(PRArenaPool *arena, CERTAVA *src);

/* convert an OID to dotted-decimal representation */
/* Returns a string that must be freed with PR_smprintf_free(). */
extern char * CERT_GetOidString(const SECItem *oid);

/*
** Examine an AVA and return the tag that refers to it. The AVA tags are
** defined as SEC_OID_AVA*.
*/
extern SECOidTag CERT_GetAVATag(CERTAVA *ava);

/*
** Compare two AVA's, returning the difference between them.
*/
extern SECComparison CERT_CompareAVA(const CERTAVA *a, const CERTAVA *b);

/*
** Create an RDN (relative-distinguished-name). The argument list is a
** NULL terminated list of AVA's.
*/
extern CERTRDN *CERT_CreateRDN(PRArenaPool *arena, CERTAVA *avas, ...);

/*
** Make a copy of "src" storing it in "dest".
*/
extern SECStatus CERT_CopyRDN(PRArenaPool *arena, CERTRDN *dest, CERTRDN *src);

/*
** Destory an RDN object.
**	"rdn" the RDN to destroy
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void CERT_DestroyRDN(CERTRDN *rdn, PRBool freeit);

/*
** Add an AVA to an RDN.
**	"rdn" the RDN to add to
**	"ava" the AVA to add
*/
extern SECStatus CERT_AddAVA(PRArenaPool *arena, CERTRDN *rdn, CERTAVA *ava);

/*
** Compare two RDN's, returning the difference between them.
*/
extern SECComparison CERT_CompareRDN(CERTRDN *a, CERTRDN *b);

/*
** Create an X.500 style name using a NULL terminated list of RDN's.
*/
extern CERTName *CERT_CreateName(CERTRDN *rdn, ...);

/*
** Make a copy of "src" storing it in "dest". Memory is allocated in
** "dest" for each of the appropriate sub objects. Memory is not freed in
** "dest" before allocation is done (use CERT_DestroyName(dest, PR_FALSE) to
** do that).
*/
extern SECStatus CERT_CopyName(PRArenaPool *arena, CERTName *dest, CERTName *src);

/*
** Destroy a Name object.
**	"name" the CERTName to destroy
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void CERT_DestroyName(CERTName *name);

/*
** Add an RDN to a name.
**	"name" the name to add the RDN to
**	"rdn" the RDN to add to name
*/
extern SECStatus CERT_AddRDN(CERTName *name, CERTRDN *rdn);

/*
** Compare two names, returning the difference between them.
*/
extern SECComparison CERT_CompareName(CERTName *a, CERTName *b);

/*
** Convert a CERTName into something readable
*/
extern char *CERT_FormatName (CERTName *name);

/*
** Convert a der-encoded integer to a hex printable string form.
** Perhaps this should be a SEC function but it's only used for certs.
*/
extern char *CERT_Hexify (SECItem *i, int do_colon);

/******************************************************************************
 *
 * Certificate handling operations
 *
 *****************************************************************************/

/*
** Create a new validity object given two unix time values.
**	"notBefore" the time before which the validity is not valid
**	"notAfter" the time after which the validity is not valid
*/
extern CERTValidity *CERT_CreateValidity(int64 notBefore, int64 notAfter);

/*
** Destroy a validity object.
**	"v" the validity to destroy
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void CERT_DestroyValidity(CERTValidity *v);

/*
** Copy the "src" object to "dest". Memory is allocated in "dest" for
** each of the appropriate sub-objects. Memory in "dest" is not freed
** before memory is allocated (use CERT_DestroyValidity(v, PR_FALSE) to do
** that).
*/
extern SECStatus CERT_CopyValidity
   (PRArenaPool *arena, CERTValidity *dest, CERTValidity *src);

/*
** The cert lib considers a cert or CRL valid if the "notBefore" time is
** in the not-too-distant future, e.g. within the next 24 hours. This 
** prevents freshly issued certificates from being considered invalid
** because the local system's time zone is incorrectly set.  
** The amount of "pending slop time" is adjustable by the application.
** Units of SlopTime are seconds.  Default is 86400  (24 hours).
** Negative SlopTime values are not allowed.
*/
PRInt32 CERT_GetSlopTime(void);

SECStatus CERT_SetSlopTime(PRInt32 slop);

/*
** Create a new certificate object. The result must be wrapped with an
** CERTSignedData to create a signed certificate.
**	"serialNumber" the serial number
**	"issuer" the name of the certificate issuer
**	"validity" the validity period of the certificate
**	"req" the certificate request that prompted the certificate issuance
*/
extern CERTCertificate *
CERT_CreateCertificate (unsigned long serialNumber, CERTName *issuer,
			CERTValidity *validity, CERTCertificateRequest *req);

/*
** Destroy a certificate object
**	"cert" the certificate to destroy
** NOTE: certificate's are reference counted. This call decrements the
** reference count, and if the result is zero, then the object is destroyed
** and optionally freed.
*/
extern void CERT_DestroyCertificate(CERTCertificate *cert);

/*
** Make a shallow copy of a certificate "c". Just increments the
** reference count on "c".
*/
extern CERTCertificate *CERT_DupCertificate(CERTCertificate *c);

/*
** Create a new certificate request. This result must be wrapped with an
** CERTSignedData to create a signed certificate request.
**	"name" the subject name (who the certificate request is from)
**	"spki" describes/defines the public key the certificate is for
**	"attributes" if non-zero, some optional attribute data
*/
extern CERTCertificateRequest *
CERT_CreateCertificateRequest (CERTName *name, CERTSubjectPublicKeyInfo *spki,
			       SECItem **attributes);

/*
** Destroy a certificate-request object
**	"r" the certificate-request to destroy
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void CERT_DestroyCertificateRequest(CERTCertificateRequest *r);

/*
** Extract a public key object from a certificate
*/
extern SECKEYPublicKey *CERT_ExtractPublicKey(CERTCertificate *cert);

/*
 * used to get a public key with Key Material ID. Only used for fortezza V1
 * certificates.
 */
extern SECKEYPublicKey *CERT_KMIDPublicKey(CERTCertificate *cert);


/*
** Retrieve the Key Type associated with the cert we're dealing with
*/

extern KeyType CERT_GetCertKeyType (CERTSubjectPublicKeyInfo *spki);

/*
** Initialize the certificate database.  This is called to create
**  the initial list of certificates in the database.
*/
extern SECStatus CERT_InitCertDB(CERTCertDBHandle *handle);

extern int CERT_GetDBContentVersion(CERTCertDBHandle *handle);

/*
** Default certificate database routines
*/
extern void CERT_SetDefaultCertDB(CERTCertDBHandle *handle);

extern CERTCertDBHandle *CERT_GetDefaultCertDB(void);

extern CERTCertList *CERT_GetCertChainFromCert(CERTCertificate *cert, 
					       int64 time, 
					       SECCertUsage usage);
extern CERTCertificate *
CERT_NewTempCertificate (CERTCertDBHandle *handle, SECItem *derCert,
                         char *nickname, PRBool isperm, PRBool copyDER);


/******************************************************************************
 *
 * X.500 Name handling operations
 *
 *****************************************************************************/

/*
** Create an AVA (attribute-value-assertion)
**	"arena" the memory arena to alloc from
**	"kind" is one of SEC_OID_AVA_*
**	"valueType" is one of DER_PRINTABLE_STRING, DER_IA5_STRING, or
**	   DER_T61_STRING
**	"value" is the null terminated string containing the value
*/
extern CERTAVA *CERT_CreateAVA
   (PRArenaPool *arena, SECOidTag kind, int valueType, char *value);

/*
** Extract the Distinguished Name from a DER encoded certificate
**	"derCert" is the DER encoded certificate
**	"derName" is the SECItem that the name is returned in
*/
extern SECStatus CERT_NameFromDERCert(SECItem *derCert, SECItem *derName);

/*
** Extract the Issuers Distinguished Name from a DER encoded certificate
**	"derCert" is the DER encoded certificate
**	"derName" is the SECItem that the name is returned in
*/
extern SECStatus CERT_IssuerNameFromDERCert(SECItem *derCert, 
					    SECItem *derName);



/*
** Generate a database search key for a certificate, based on the
** issuer and serial number.
**	"arena" the memory arena to alloc from
**	"derCert" the DER encoded certificate
**	"key" the returned key
*/
extern SECStatus CERT_KeyFromDERCert(PRArenaPool *arena, SECItem *derCert, SECItem *key);

extern SECStatus CERT_KeyFromIssuerAndSN(PRArenaPool *arena, SECItem *issuer,
					 SECItem *sn, SECItem *key);

extern SECStatus CERT_SerialNumberFromDERCert(SECItem *derCert, 
						SECItem *derName);


/*
** Generate a database search key for a crl, based on the
** issuer.
**	"arena" the memory arena to alloc from
**	"derCrl" the DER encoded crl
**	"key" the returned key
*/
extern SECStatus CERT_KeyFromDERCrl(PRArenaPool *arena, SECItem *derCrl, SECItem *key);

/*
** Open the certificate database.  Use callback to get name of database.
*/
extern SECStatus CERT_OpenCertDB(CERTCertDBHandle *handle, PRBool readOnly,
				 CERTDBNameFunc namecb, void *cbarg);

/* Open the certificate database.  Use given filename for database. */
extern SECStatus CERT_OpenCertDBFilename(CERTCertDBHandle *handle,
					 char *certdbname, PRBool readOnly);

/*
** Open and initialize a cert database that is entirely in memory.  This
** can be used when the permanent database can not be opened or created.
*/
extern SECStatus CERT_OpenVolatileCertDB(CERTCertDBHandle *handle);

/*
** Check the hostname to make sure that it matches the shexp that
** is given in the common name of the certificate.
*/
extern SECStatus CERT_VerifyCertName(CERTCertificate *cert, const char *hostname);

/*
** Add a domain name to the list of names that the user has explicitly
** allowed (despite cert name mismatches) for use with a server cert.
*/
extern SECStatus CERT_AddOKDomainName(CERTCertificate *cert, const char *hostname);

/*
** Decode a DER encoded certificate into an CERTCertificate structure
**	"derSignedCert" is the DER encoded signed certificate
**	"copyDER" is true if the DER should be copied, false if the
**		existing copy should be referenced
**	"nickname" is the nickname to use in the database.  If it is NULL
**		then a temporary nickname is generated.
*/
extern CERTCertificate *
CERT_DecodeDERCertificate (SECItem *derSignedCert, PRBool copyDER, char *nickname);
/*
** Decode a DER encoded CRL/KRL into an CERTSignedCrl structure
**	"derSignedCrl" is the DER encoded signed crl/krl.
**	"type" is this a CRL or KRL.
*/
#define SEC_CRL_TYPE	1
#define SEC_KRL_TYPE	0

extern CERTSignedCrl *
CERT_DecodeDERCrl (PRArenaPool *arena, SECItem *derSignedCrl,int type);

/*
 * same as CERT_DecodeDERCrl, plus allow options to be passed in
 */

extern CERTSignedCrl *
CERT_DecodeDERCrlWithFlags(PRArenaPool *narena, SECItem *derSignedCrl,
                          int type, PRInt32 options);

/* CRL options to pass */

#define CRL_DECODE_DEFAULT_OPTIONS          0x00000000

/* when CRL_DECODE_DONT_COPY_DER is set, the DER is not copied . The
   application must then keep derSignedCrl until it destroys the
   CRL . Ideally, it should allocate derSignedCrl in an arena
   and pass that arena in as the first argument to
   CERT_DecodeDERCrlWithFlags */

#define CRL_DECODE_DONT_COPY_DER            0x00000001
#define CRL_DECODE_SKIP_ENTRIES             0x00000002
#define CRL_DECODE_KEEP_BAD_CRL             0x00000004

/* complete the decoding of a partially decoded CRL, ie. decode the
   entries. Note that entries is an optional field in a CRL, so the
   "entries" pointer in CERTCrlStr may still be NULL even after
   function returns SECSuccess */

extern SECStatus CERT_CompleteCRLDecodeEntries(CERTSignedCrl* crl);

/* Validate CRL then import it to the dbase.  If there is already a CRL with the
 * same CA in the dbase, it will be replaced if derCRL is more up to date.  
 * If the process successes, a CRL will be returned.  Otherwise, a NULL will 
 * be returned. The caller should call PORT_GetError() for the exactly error 
 * code.
 */
extern CERTSignedCrl *
CERT_ImportCRL (CERTCertDBHandle *handle, SECItem *derCRL, char *url, 
						int type, void * wincx);

extern void CERT_DestroyCrl (CERTSignedCrl *crl);

/* this is a hint to flush the CRL cache. crlKey is the DER subject of
   the issuer (CA). */
void CERT_CRLCacheRefreshIssuer(CERTCertDBHandle* dbhandle, SECItem* crlKey);

/*
** Decode a certificate and put it into the temporary certificate database
*/
extern CERTCertificate *
CERT_DecodeCertificate (SECItem *derCert, char *nickname,PRBool copyDER);

/*
** Find a certificate in the database
**	"key" is the database key to look for
*/
extern CERTCertificate *CERT_FindCertByKey(CERTCertDBHandle *handle, SECItem *key);

/*
** Find a certificate in the database by name
**	"name" is the distinguished name to look up
*/
extern CERTCertificate *
CERT_FindCertByName (CERTCertDBHandle *handle, SECItem *name);

/*
** Find a certificate in the database by name
**	"name" is the distinguished name to look up (in ascii)
*/
extern CERTCertificate *
CERT_FindCertByNameString (CERTCertDBHandle *handle, char *name);

/*
** Find a certificate in the database by name and keyid
**	"name" is the distinguished name to look up
**	"keyID" is the value of the subjectKeyID to match
*/
extern CERTCertificate *
CERT_FindCertByKeyID (CERTCertDBHandle *handle, SECItem *name, SECItem *keyID);

/*
** Generate a certificate key from the issuer and serialnumber, then look it
** up in the database.  Return the cert if found.
**	"issuerAndSN" is the issuer and serial number to look for
*/
extern CERTCertificate *
CERT_FindCertByIssuerAndSN (CERTCertDBHandle *handle, CERTIssuerAndSN *issuerAndSN);

/*
** Find a certificate in the database by a subject key ID
**	"subjKeyID" is the subject Key ID to look for
*/
extern CERTCertificate *
CERT_FindCertBySubjectKeyID (CERTCertDBHandle *handle, SECItem *subjKeyID);

/*
** Find a certificate in the database by a nickname
**	"nickname" is the ascii string nickname to look for
*/
extern CERTCertificate *
CERT_FindCertByNickname (CERTCertDBHandle *handle, char *nickname);

/*
** Find a certificate in the database by a DER encoded certificate
**	"derCert" is the DER encoded certificate
*/
extern CERTCertificate *
CERT_FindCertByDERCert(CERTCertDBHandle *handle, SECItem *derCert);

/*
** Find a certificate in the database by a email address
**	"emailAddr" is the email address to look up
*/
CERTCertificate *
CERT_FindCertByEmailAddr(CERTCertDBHandle *handle, char *emailAddr);

/*
** Find a certificate in the database by a email address or nickname
**	"name" is the email address or nickname to look up
*/
CERTCertificate *
CERT_FindCertByNicknameOrEmailAddr(CERTCertDBHandle *handle, char *name);

/*
** Find a certificate in the database by a digest of a subject public key
**	"spkDigest" is the digest to look up
*/
extern CERTCertificate *
CERT_FindCertBySPKDigest(CERTCertDBHandle *handle, SECItem *spkDigest);

/*
 * Find the issuer of a cert
 */
CERTCertificate *
CERT_FindCertIssuer(CERTCertificate *cert, int64 validTime, SECCertUsage usage);

/*
** Check the validity times of a certificate vs. time 't', allowing
** some slop for broken clocks and stuff.
**	"cert" is the certificate to be checked
**	"t" is the time to check against
**	"allowOverride" if true then check to see if the invalidity has
**		been overridden by the user.
*/
extern SECCertTimeValidity CERT_CheckCertValidTimes(CERTCertificate *cert,
						    PRTime t,
						    PRBool allowOverride);

/*
** WARNING - this function is depricated, and will either go away or have
**		a new API in the near future.
**
** Check the validity times of a certificate vs. the current time, allowing
** some slop for broken clocks and stuff.
**	"cert" is the certificate to be checked
*/
extern SECStatus CERT_CertTimesValid(CERTCertificate *cert);

/*
** Extract the validity times from a certificate
**	"c" is the certificate
**	"notBefore" is the start of the validity period
**	"notAfter" is the end of the validity period
*/
extern SECStatus
CERT_GetCertTimes (CERTCertificate *c, PRTime *notBefore, PRTime *notAfter);

/*
** Extract the issuer and serial number from a certificate
*/
extern CERTIssuerAndSN *CERT_GetCertIssuerAndSN(PRArenaPool *, 
							CERTCertificate *);

/*
** verify the signature of a signed data object with a given certificate
**	"sd" the signed data object to be verified
**	"cert" the certificate to use to check the signature
*/
extern SECStatus CERT_VerifySignedData(CERTSignedData *sd,
				       CERTCertificate *cert,
				       int64 t,
				       void *wincx);
/*
** verify the signature of a signed data object with the given DER publickey
*/
extern SECStatus
CERT_VerifySignedDataWithPublicKeyInfo(CERTSignedData *sd,
                                       CERTSubjectPublicKeyInfo *pubKeyInfo,
                                       void *wincx);

/*
** verify the signature of a signed data object with a SECKEYPublicKey.
*/
extern SECStatus
CERT_VerifySignedDataWithPublicKey(CERTSignedData *sd,
                                   SECKEYPublicKey *pubKey, void *wincx);

/*
** NEW FUNCTIONS with new bit-field-FIELD SECCertificateUsage - please use
** verify a certificate by checking validity times against a certain time,
** that we trust the issuer, and that the signature on the certificate is
** valid.
**	"cert" the certificate to verify
**	"checkSig" only check signatures if true
*/
extern SECStatus
CERT_VerifyCertificate(CERTCertDBHandle *handle, CERTCertificate *cert,
		PRBool checkSig, SECCertificateUsage requiredUsages,
                int64 t, void *wincx, CERTVerifyLog *log,
                SECCertificateUsage* returnedUsages);

/* same as above, but uses current time */
extern SECStatus
CERT_VerifyCertificateNow(CERTCertDBHandle *handle, CERTCertificate *cert,
		   PRBool checkSig, SECCertificateUsage requiredUsages,
                   void *wincx, SECCertificateUsage* returnedUsages);

/*
** Verify that a CA cert can certify some (unspecified) leaf cert for a given
** purpose. This is used by UI code to help identify where a chain may be
** broken and why. This takes identical parameters to CERT_VerifyCert
*/
extern SECStatus
CERT_VerifyCACertForUsage(CERTCertDBHandle *handle, CERTCertificate *cert,
		PRBool checkSig, SECCertUsage certUsage, int64 t,
		void *wincx, CERTVerifyLog *log);

/*
** OLD OBSOLETE FUNCTIONS with enum SECCertUsage - DO NOT USE FOR NEW CODE
** verify a certificate by checking validity times against a certain time,
** that we trust the issuer, and that the signature on the certificate is
** valid.
**	"cert" the certificate to verify
**	"checkSig" only check signatures if true
*/
extern SECStatus
CERT_VerifyCert(CERTCertDBHandle *handle, CERTCertificate *cert,
		PRBool checkSig, SECCertUsage certUsage, int64 t,
		void *wincx, CERTVerifyLog *log);

/* same as above, but uses current time */
extern SECStatus
CERT_VerifyCertNow(CERTCertDBHandle *handle, CERTCertificate *cert,
		   PRBool checkSig, SECCertUsage certUsage, void *wincx);

SECStatus
CERT_VerifyCertChain(CERTCertDBHandle *handle, CERTCertificate *cert,
		     PRBool checkSig, SECCertUsage certUsage, int64 t,
		     void *wincx, CERTVerifyLog *log);

/*
** Read a base64 ascii encoded DER certificate and convert it to our
** internal format.
**	"certstr" is a null-terminated string containing the certificate
*/
extern CERTCertificate *CERT_ConvertAndDecodeCertificate(char *certstr);

/*
** Read a certificate in some foreign format, and convert it to our
** internal format.
**	"certbuf" is the buffer containing the certificate
**	"certlen" is the length of the buffer
** NOTE - currently supports netscape base64 ascii encoded raw certs
**  and netscape binary DER typed files.
*/
extern CERTCertificate *CERT_DecodeCertFromPackage(char *certbuf, int certlen);

extern SECStatus
CERT_ImportCAChain (SECItem *certs, int numcerts, SECCertUsage certUsage);

extern SECStatus
CERT_ImportCAChainTrusted(SECItem *certs, int numcerts, SECCertUsage certUsage);

/*
** Read a certificate chain in some foreign format, and pass it to a 
** callback function.
**	"certbuf" is the buffer containing the certificate
**	"certlen" is the length of the buffer
**	"f" is the callback function
**	"arg" is the callback argument
*/
typedef SECStatus (PR_CALLBACK *CERTImportCertificateFunc)
   (void *arg, SECItem **certs, int numcerts);

extern SECStatus
CERT_DecodeCertPackage(char *certbuf, int certlen, CERTImportCertificateFunc f,
		       void *arg);

/*
** Pretty print a certificate in HTML
**	"cert" is the certificate to print
**	"showImages" controls whether or not to use about:security URLs
**		for subject and issuer images.  This should only be true
**		in the browser.
*/
extern char *CERT_HTMLCertInfo(CERTCertificate *cert, PRBool showImages,
			       PRBool showIssuer);

/* 
** Returns the value of an AVA.  This was a formerly static 
** function that has been exposed due to the need to decode
** and convert unicode strings to UTF8.  
**
** XXX This function resides in certhtml.c, should it be
** moved elsewhere?
*/
extern SECItem *CERT_DecodeAVAValue(const SECItem *derAVAValue);



/*
** extract various element strings from a distinguished name.
**	"name" the distinguished name
*/

extern char *CERT_GetCertificateEmailAddress(CERTCertificate *cert);

extern char *CERT_GetCertEmailAddress(CERTName *name);

extern const char * CERT_GetFirstEmailAddress(CERTCertificate * cert);

extern const char * CERT_GetNextEmailAddress(CERTCertificate * cert, 
                                             const char * prev);

/* The return value must be freed with PORT_Free. */
extern char *CERT_GetCommonName(CERTName *name);

extern char *CERT_GetCountryName(CERTName *name);

extern char *CERT_GetLocalityName(CERTName *name);

extern char *CERT_GetStateName(CERTName *name);

extern char *CERT_GetOrgName(CERTName *name);

extern char *CERT_GetOrgUnitName(CERTName *name);

extern char *CERT_GetDomainComponentName(CERTName *name);

extern char *CERT_GetCertUid(CERTName *name);

/* manipulate the trust parameters of a certificate */

extern SECStatus CERT_GetCertTrust(CERTCertificate *cert, CERTCertTrust *trust);

extern SECStatus
CERT_ChangeCertTrust (CERTCertDBHandle *handle, CERTCertificate *cert,
		      CERTCertTrust *trust);

extern SECStatus
CERT_ChangeCertTrustByUsage(CERTCertDBHandle *certdb, CERTCertificate *cert,
			    SECCertUsage usage);

/*************************************************************************
 *
 * manipulate the extensions of a certificate
 *
 ************************************************************************/

/*
** Set up a cert for adding X509v3 extensions.  Returns an opaque handle
** used by the next two routines.
**	"cert" is the certificate we are adding extensions to
*/
extern void *CERT_StartCertExtensions(CERTCertificate *cert);

/*
** Add an extension to a certificate.
**	"exthandle" is the handle returned by the previous function
**	"idtag" is the integer tag for the OID that should ID this extension
**	"value" is the value of the extension
**	"critical" is the critical extension flag
**	"copyData" is a flag indicating whether the value data should be
**		copied.
*/
extern SECStatus CERT_AddExtension (void *exthandle, int idtag, 
			SECItem *value, PRBool critical, PRBool copyData);

extern SECStatus CERT_AddExtensionByOID (void *exthandle, SECItem *oid,
			 SECItem *value, PRBool critical, PRBool copyData);

extern SECStatus CERT_EncodeAndAddExtension
   (void *exthandle, int idtag, void *value, PRBool critical,
    const SEC_ASN1Template *atemplate);

extern SECStatus CERT_EncodeAndAddBitStrExtension
   (void *exthandle, int idtag, SECItem *value, PRBool critical);


extern SECStatus
CERT_EncodeAltNameExtension(PRArenaPool *arena,  CERTGeneralName  *value, SECItem *encodedValue);


/*
** Finish adding cert extensions.  Does final processing on extension
** data, putting it in the right format, and freeing any temporary
** storage.
**	"exthandle" is the handle used to add extensions to a certificate
*/
extern SECStatus CERT_FinishExtensions(void *exthandle);


/* If the extension is found, return its criticality and value.
** This allocate storage for the returning extension value.
*/
extern SECStatus CERT_GetExtenCriticality
   (CERTCertExtension **extensions, int tag, PRBool *isCritical);

extern void
CERT_DestroyOidSequence(CERTOidSequence *oidSeq);

/****************************************************************************
 *
 * DER encode and decode extension values
 *
 ****************************************************************************/

/* Encode the value of the basicConstraint extension.
**	arena - where to allocate memory for the encoded value.
**	value - extension value to encode
**	encodedValue - output encoded value
*/
extern SECStatus CERT_EncodeBasicConstraintValue
   (PRArenaPool *arena, CERTBasicConstraints *value, SECItem *encodedValue);

/*
** Encode the value of the authorityKeyIdentifier extension.
*/
extern SECStatus CERT_EncodeAuthKeyID
   (PRArenaPool *arena, CERTAuthKeyID *value, SECItem *encodedValue);

/*
** Encode the value of the crlDistributionPoints extension.
*/
extern SECStatus CERT_EncodeCRLDistributionPoints
   (PRArenaPool *arena, CERTCrlDistributionPoints *value,SECItem *derValue);

/*
** Decodes a DER encoded basicConstaint extension value into a readable format
**	value - decoded value
**	encodedValue - value to decoded
*/
extern SECStatus CERT_DecodeBasicConstraintValue
   (CERTBasicConstraints *value, SECItem *encodedValue);

/* Decodes a DER encoded authorityKeyIdentifier extension value into a
** readable format.
**	arena - where to allocate memory for the decoded value
**	encodedValue - value to be decoded
**	Returns a CERTAuthKeyID structure which contains the decoded value
*/
extern CERTAuthKeyID *CERT_DecodeAuthKeyID 
			(PRArenaPool *arena, SECItem *encodedValue);


/* Decodes a DER encoded crlDistributionPoints extension value into a 
** readable format.
**	arena - where to allocate memory for the decoded value
**	der - value to be decoded
**	Returns a CERTCrlDistributionPoints structure which contains the 
**          decoded value
*/
extern CERTCrlDistributionPoints * CERT_DecodeCRLDistributionPoints
   (PRArenaPool *arena, SECItem *der);

/* Extract certain name type from a generalName */
extern void *CERT_GetGeneralNameByType
   (CERTGeneralName *genNames, CERTGeneralNameType type, PRBool derFormat);


extern CERTOidSequence *
CERT_DecodeOidSequence(SECItem *seqItem);




/****************************************************************************
 *
 * Find extension values of a certificate 
 *
 ***************************************************************************/

extern SECStatus CERT_FindCertExtension
   (CERTCertificate *cert, int tag, SECItem *value);

extern SECStatus CERT_FindNSCertTypeExtension
   (CERTCertificate *cert, SECItem *value);

extern char * CERT_FindNSStringExtension (CERTCertificate *cert, int oidtag);

extern SECStatus CERT_FindIssuerCertExtension
   (CERTCertificate *cert, int tag, SECItem *value);

extern SECStatus CERT_FindCertExtensionByOID
   (CERTCertificate *cert, SECItem *oid, SECItem *value);

extern char *CERT_FindCertURLExtension (CERTCertificate *cert, int tag, 
								int catag);

/* Returns the decoded value of the authKeyID extension.
**   Note that this uses passed in the arena to allocate storage for the result
*/
extern CERTAuthKeyID * CERT_FindAuthKeyIDExten (PRArenaPool *arena,CERTCertificate *cert);

/* Returns the decoded value of the basicConstraint extension.
 */
extern SECStatus CERT_FindBasicConstraintExten
   (CERTCertificate *cert, CERTBasicConstraints *value);

/* Returns the decoded value of the crlDistributionPoints extension.
**  Note that the arena in cert is used to allocate storage for the result
*/
extern CERTCrlDistributionPoints * CERT_FindCRLDistributionPoints
   (CERTCertificate *cert);

/* Returns value of the keyUsage extension.  This uses PR_Alloc to allocate 
** buffer for the decoded value. The caller should free up the storage 
** allocated in value->data.
*/
extern SECStatus CERT_FindKeyUsageExtension (CERTCertificate *cert, 
							SECItem *value);

/* Return the decoded value of the subjectKeyID extension. The caller should 
** free up the storage allocated in retItem->data.
*/
extern SECStatus CERT_FindSubjectKeyIDExtension (CERTCertificate *cert, 
							   SECItem *retItem);

/*
** If cert is a v3 certificate, and a critical keyUsage extension is included,
** then check the usage against the extension value.  If a non-critical 
** keyUsage extension is included, this will return SECSuccess without 
** checking, since the extension is an advisory field, not a restriction.  
** If cert is not a v3 certificate, this will return SECSuccess.
**	cert - certificate
**	usage - one of the x.509 v3 the Key Usage Extension flags
*/
extern SECStatus CERT_CheckCertUsage (CERTCertificate *cert, 
							unsigned char usage);

/****************************************************************************
 *
 *  CRL v2 Extensions supported routines
 *
 ****************************************************************************/

extern SECStatus CERT_FindCRLExtensionByOID
   (CERTCrl *crl, SECItem *oid, SECItem *value);

extern SECStatus CERT_FindCRLExtension
   (CERTCrl *crl, int tag, SECItem *value);

extern SECStatus
   CERT_FindInvalidDateExten (CERTCrl *crl, int64 *value);

extern void *CERT_StartCRLExtensions (CERTCrl *crl);

extern CERTCertNicknames *CERT_GetCertNicknames (CERTCertDBHandle *handle,
						 int what, void *wincx);

/*
** Finds the crlNumber extension and decodes its value into 'value'
*/
extern SECStatus CERT_FindCRLNumberExten (CERTCrl *crl, CERTCrlNumber *value);

extern void CERT_FreeNicknames(CERTCertNicknames *nicknames);

extern PRBool CERT_CompareCerts(CERTCertificate *c1, CERTCertificate *c2);

extern PRBool CERT_CompareCertsForRedirection(CERTCertificate *c1,
							 CERTCertificate *c2);

/*
** Generate an array of the Distinguished Names that the given cert database
** "trusts"
*/
extern CERTDistNames *CERT_GetSSLCACerts(CERTCertDBHandle *handle);

extern void CERT_FreeDistNames(CERTDistNames *names);

/*
** Generate an array of Distinguished names from an array of nicknames
*/
extern CERTDistNames *CERT_DistNamesFromNicknames
   (CERTCertDBHandle *handle, char **nicknames, int nnames);

/*
** Generate a certificate chain from a certificate.
*/
extern CERTCertificateList *
CERT_CertChainFromCert(CERTCertificate *cert, SECCertUsage usage,
		       PRBool includeRoot);

extern CERTCertificateList *
CERT_CertListFromCert(CERTCertificate *cert);

extern CERTCertificateList *
CERT_DupCertList(CERTCertificateList * oldList);

extern void CERT_DestroyCertificateList(CERTCertificateList *list);

/*
** is cert a user cert? i.e. does it have CERTDB_USER trust,
** i.e. a private key?
*/
PRBool CERT_IsUserCert(CERTCertificate* cert);

/* is cert a newer than cert b? */
PRBool CERT_IsNewer(CERTCertificate *certa, CERTCertificate *certb);

/* currently a stub for address book */
PRBool
CERT_IsCertRevoked(CERTCertificate *cert);

void
CERT_DestroyCertArray(CERTCertificate **certs, unsigned int ncerts);

/* convert an email address to lower case */
char *CERT_FixupEmailAddr(char *emailAddr);

/* decode string representation of trust flags into trust struct */
SECStatus
CERT_DecodeTrustString(CERTCertTrust *trust, char *trusts);

/* encode trust struct into string representation of trust flags */
char *
CERT_EncodeTrustString(CERTCertTrust *trust);

/* find the next or prev cert in a subject list */
CERTCertificate *
CERT_PrevSubjectCert(CERTCertificate *cert);
CERTCertificate *
CERT_NextSubjectCert(CERTCertificate *cert);

/*
 * import a collection of certs into the temporary or permanent cert
 * database
 */
SECStatus
CERT_ImportCerts(CERTCertDBHandle *certdb, SECCertUsage usage,
		 unsigned int ncerts, SECItem **derCerts,
		 CERTCertificate ***retCerts, PRBool keepCerts,
		 PRBool caOnly, char *nickname);

SECStatus
CERT_SaveImportedCert(CERTCertificate *cert, SECCertUsage usage,
		      PRBool caOnly, char *nickname);

char *
CERT_MakeCANickname(CERTCertificate *cert);

PRBool
CERT_IsCACert(CERTCertificate *cert, unsigned int *rettype);

PRBool
CERT_IsCADERCert(SECItem *derCert, unsigned int *rettype);

PRBool
CERT_IsRootDERCert(SECItem *derCert);

SECStatus
CERT_SaveSMimeProfile(CERTCertificate *cert, SECItem *emailProfile,
		      SECItem *profileTime);

/*
 * find the smime symmetric capabilities profile for a given cert
 */
SECItem *
CERT_FindSMimeProfile(CERTCertificate *cert);

SECStatus
CERT_AddNewCerts(CERTCertDBHandle *handle);

CERTPackageType
CERT_CertPackageType(SECItem *package, SECItem *certitem);

CERTCertificatePolicies *
CERT_DecodeCertificatePoliciesExtension(SECItem *extnValue);

void
CERT_DestroyCertificatePoliciesExtension(CERTCertificatePolicies *policies);

CERTUserNotice *
CERT_DecodeUserNotice(SECItem *noticeItem);

extern CERTGeneralName *
CERT_DecodeAltNameExtension(PRArenaPool *arena, SECItem *EncodedAltName);

extern CERTNameConstraints *
CERT_DecodeNameConstraintsExtension(PRArenaPool *arena, 
                                    SECItem *encodedConstraints);

/* returns addr of a NULL termainated array of pointers to CERTAuthInfoAccess */
extern CERTAuthInfoAccess **
CERT_DecodeAuthInfoAccessExtension(PRArenaPool *arena,
				   SECItem     *encodedExtension);

extern CERTPrivKeyUsagePeriod *
CERT_DecodePrivKeyUsagePeriodExtension(PLArenaPool *arena, SECItem *extnValue);

extern CERTGeneralName *
CERT_GetNextGeneralName(CERTGeneralName *current);

extern CERTGeneralName *
CERT_GetPrevGeneralName(CERTGeneralName *current);

CERTNameConstraint *
CERT_GetNextNameConstraint(CERTNameConstraint *current);

CERTNameConstraint *
CERT_GetPrevNameConstraint(CERTNameConstraint *current);

void
CERT_DestroyUserNotice(CERTUserNotice *userNotice);

typedef char * (* CERTPolicyStringCallback)(char *org,
					       unsigned long noticeNumber,
					       void *arg);
void
CERT_SetCAPolicyStringCallback(CERTPolicyStringCallback cb, void *cbarg);

char *
CERT_GetCertCommentString(CERTCertificate *cert);

PRBool
CERT_GovtApprovedBitSet(CERTCertificate *cert);

SECStatus
CERT_AddPermNickname(CERTCertificate *cert, char *nickname);

/*
 * Given a cert, find the cert with the same subject name that
 * has the given key usage.  If the given cert has the correct keyUsage, then
 * return it, otherwise search the list in order.
 */
CERTCertificate *
CERT_FindCertByUsage(CERTCertificate *basecert, unsigned int requiredKeyUsage);


CERTCertList *
CERT_MatchUserCert(CERTCertDBHandle *handle,
		   SECCertUsage usage,
		   int nCANames, char **caNames,
		   void *proto_win);

CERTCertList *
CERT_NewCertList(void);

void
CERT_DestroyCertList(CERTCertList *certs);

/* remove the node and free the cert */
void
CERT_RemoveCertListNode(CERTCertListNode *node);

SECStatus
CERT_AddCertToListTail(CERTCertList *certs, CERTCertificate *cert);

SECStatus
CERT_AddCertToListHead(CERTCertList *certs, CERTCertificate *cert);

SECStatus
CERT_AddCertToListTailWithData(CERTCertList *certs, CERTCertificate *cert,
							 void *appData);

SECStatus
CERT_AddCertToListHeadWithData(CERTCertList *certs, CERTCertificate *cert,
							 void *appData);

typedef PRBool (* CERTSortCallback)(CERTCertificate *certa,
				    CERTCertificate *certb,
				    void *arg);
SECStatus
CERT_AddCertToListSorted(CERTCertList *certs, CERTCertificate *cert,
			 CERTSortCallback f, void *arg);

/* callback for CERT_AddCertToListSorted that sorts based on validity
 * period and a given time.
 */
PRBool
CERT_SortCBValidity(CERTCertificate *certa,
		    CERTCertificate *certb,
		    void *arg);

SECStatus
CERT_CheckForEvilCert(CERTCertificate *cert);

CERTGeneralName *
CERT_GetCertificateNames(CERTCertificate *cert, PRArenaPool *arena);


SECStatus 
CERT_EncodeSubjectKeyID(PRArenaPool *arena, char *value, int len, SECItem *encodedValue);

char *
CERT_GetNickName(CERTCertificate   *cert, CERTCertDBHandle *handle, PRArenaPool *nicknameArena);

/*
 * Creates or adds to a list of all certs with a give subject name, sorted by
 * validity time, newest first.  Invalid certs are considered older than
 * valid certs. If validOnly is set, do not include invalid certs on list.
 */
CERTCertList *
CERT_CreateSubjectCertList(CERTCertList *certList, CERTCertDBHandle *handle,
			   SECItem *name, int64 sorttime, PRBool validOnly);

/*
 * Creates or adds to a list of all certs with a give nickname, sorted by
 * validity time, newest first.  Invalid certs are considered older than valid
 * certs. If validOnly is set, do not include invalid certs on list.
 */
CERTCertList *
CERT_CreateNicknameCertList(CERTCertList *certList, CERTCertDBHandle *handle,
			    char *nickname, int64 sorttime, PRBool validOnly);

/*
 * Creates or adds to a list of all certs with a give email addr, sorted by
 * validity time, newest first.  Invalid certs are considered older than valid
 * certs. If validOnly is set, do not include invalid certs on list.
 */
CERTCertList *
CERT_CreateEmailAddrCertList(CERTCertList *certList, CERTCertDBHandle *handle,
			     char *emailAddr, int64 sorttime, PRBool validOnly);

/*
 * remove certs from a list that don't have keyUsage and certType
 * that match the given usage.
 */
SECStatus
CERT_FilterCertListByUsage(CERTCertList *certList, SECCertUsage usage,
			   PRBool ca);

/*
 * check the key usage of a cert against a set of required values
 */
SECStatus
CERT_CheckKeyUsage(CERTCertificate *cert, unsigned int requiredUsage);

/*
 * return required key usage and cert type based on cert usage
 */
SECStatus
CERT_KeyUsageAndTypeForCertUsage(SECCertUsage usage,
				 PRBool ca,
				 unsigned int *retKeyUsage,
				 unsigned int *retCertType);
/*
 * return required trust flags for various cert usages for CAs
 */
SECStatus
CERT_TrustFlagsForCACertUsage(SECCertUsage usage,
			      unsigned int *retFlags,
			      SECTrustType *retTrustType);

/*
 * Find all user certificates that match the given criteria.
 * 
 *	"handle" - database to search
 *	"usage" - certificate usage to match
 *	"oneCertPerName" - if set then only return the "best" cert per
 *			name
 *	"validOnly" - only return certs that are curently valid
 *	"proto_win" - window handle passed to pkcs11
 */
CERTCertList *
CERT_FindUserCertsByUsage(CERTCertDBHandle *handle,
			  SECCertUsage usage,
			  PRBool oneCertPerName,
			  PRBool validOnly,
			  void *proto_win);

/*
 * Find a user certificate that matchs the given criteria.
 * 
 *	"handle" - database to search
 *	"nickname" - nickname to match
 *	"usage" - certificate usage to match
 *	"validOnly" - only return certs that are curently valid
 *	"proto_win" - window handle passed to pkcs11
 */
CERTCertificate *
CERT_FindUserCertByUsage(CERTCertDBHandle *handle,
			 char *nickname,
			 SECCertUsage usage,
			 PRBool validOnly,
			 void *proto_win);

/*
 * Filter a list of certificates, removing those certs that do not have
 * one of the named CA certs somewhere in their cert chain.
 *
 *	"certList" - the list of certificates to filter
 *	"nCANames" - number of CA names
 *	"caNames" - array of CA names in string(rfc 1485) form
 *	"usage" - what use the certs are for, this is used when
 *		selecting CA certs
 */
SECStatus
CERT_FilterCertListByCANames(CERTCertList *certList, int nCANames,
			     char **caNames, SECCertUsage usage);

/*
 * Filter a list of certificates, removing those certs that aren't user certs
 */
SECStatus
CERT_FilterCertListForUserCerts(CERTCertList *certList);

/*
 * Collect the nicknames from all certs in a CertList.  If the cert is not
 * valid, append a string to that nickname.
 *
 * "certList" - the list of certificates
 * "expiredString" - the string to append to the nickname of any expired cert
 * "notYetGoodString" - the string to append to the nickname of any cert
 *		that is not yet valid
 */
CERTCertNicknames *
CERT_NicknameStringsFromCertList(CERTCertList *certList, char *expiredString,
				 char *notYetGoodString);

/*
 * Extract the nickname from a nickmake string that may have either
 * expiredString or notYetGoodString appended.
 *
 * Args:
 *	"namestring" - the string containing the nickname, and possibly
 *		one of the validity label strings
 *	"expiredString" - the expired validity label string
 *	"notYetGoodString" - the not yet good validity label string
 *
 * Returns the raw nickname
 */
char *
CERT_ExtractNicknameString(char *namestring, char *expiredString,
			   char *notYetGoodString);

/*
 * Given a certificate, return a string containing the nickname, and possibly
 * one of the validity strings, based on the current validity state of the
 * certificate.
 *
 * "arena" - arena to allocate returned string from.  If NULL, then heap
 *	is used.
 * "cert" - the cert to get nickname from
 * "expiredString" - the string to append to the nickname if the cert is
 *		expired.
 * "notYetGoodString" - the string to append to the nickname if the cert is
 *		not yet good.
 */
char *
CERT_GetCertNicknameWithValidity(PRArenaPool *arena, CERTCertificate *cert,
				 char *expiredString, char *notYetGoodString);

/*
 * Return the string representation of a DER encoded distinguished name
 * "dername" - The DER encoded name to convert
 */
char *
CERT_DerNameToAscii(SECItem *dername);

/*
 * Supported usage values and types:
 *	certUsageSSLClient
 *	certUsageSSLServer
 *	certUsageSSLServerWithStepUp
 *	certUsageEmailSigner
 *	certUsageEmailRecipient
 *	certUsageObjectSigner
 */

CERTCertificate *
CERT_FindMatchingCert(CERTCertDBHandle *handle, SECItem *derName,
		      CERTCertOwner owner, SECCertUsage usage,
		      PRBool preferTrusted, int64 validTime, PRBool validOnly);

/*
 * Acquire the global lock on the cert database.
 * This lock is currently used for the following operations:
 *	adding or deleting a cert to either the temp or perm databases
 *	converting a temp to perm or perm to temp
 *	changing(maybe just adding?) the trust of a cert
 *	adjusting the reference count of a cert
 */
void
CERT_LockDB(CERTCertDBHandle *handle);

/*
 * Free the global cert database lock.
 */
void
CERT_UnlockDB(CERTCertDBHandle *handle);

/*
 * Get the certificate status checking configuratino data for
 * the certificate database
 */
CERTStatusConfig *
CERT_GetStatusConfig(CERTCertDBHandle *handle);

/*
 * Set the certificate status checking information for the
 * database.  The input structure becomes part of the certificate
 * database and will be freed by calling the 'Destroy' function in
 * the configuration object.
 */
void
CERT_SetStatusConfig(CERTCertDBHandle *handle, CERTStatusConfig *config);



/*
 * Acquire the cert reference count lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
void
CERT_LockCertRefCount(CERTCertificate *cert);

/*
 * Free the cert reference count lock
 */
void
CERT_UnlockCertRefCount(CERTCertificate *cert);

/*
 * Acquire the cert trust lock
 * There is currently one global lock for all certs, but I'm putting a cert
 * arg here so that it will be easy to make it per-cert in the future if
 * that turns out to be necessary.
 */
void
CERT_LockCertTrust(CERTCertificate *cert);

/*
 * Free the cert trust lock
 */
void
CERT_UnlockCertTrust(CERTCertificate *cert);

/*
 * Digest the cert's subject public key using the specified algorithm.
 * The necessary storage for the digest data is allocated.  If "fill" is
 * non-null, the data is put there, otherwise a SECItem is allocated.
 * Allocation from "arena" if it is non-null, heap otherwise.  Any problem
 * results in a NULL being returned (and an appropriate error set).
 */ 
extern SECItem *
CERT_SPKDigestValueForCert(PRArenaPool *arena, CERTCertificate *cert,
			   SECOidTag digestAlg, SECItem *fill);

/*
 * fill in nsCertType field of the cert based on the cert extension
 */
extern SECStatus cert_GetCertType(CERTCertificate *cert);


SECStatus CERT_CheckCRL(CERTCertificate* cert, CERTCertificate* issuer,
                        SECItem* dp, int64 t, void* wincx);


SEC_END_PROTOS

#endif /* _CERT_H_ */
