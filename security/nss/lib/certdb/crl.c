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

/*
 * Moved from secpkcs7.c
 *
 * $Id: crl.c,v 1.1 2000/03/31 19:42:37 relyea%netscape.com Exp $
 */

#include "cert.h"
#include "secder.h"
#include "secasn1.h"
#include "secoid.h"
#include "certdb.h"
#include "certxutl.h"
#include "prtime.h"
#include "secerr.h"

const SEC_ASN1Template SEC_CERTExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCertExtension) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTCertExtension,id) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,		/* XXX DER_DEFAULT */
	  offsetof(CERTCertExtension,critical), },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(CERTCertExtension,value) },
    { 0, }
};

static const SEC_ASN1Template SEC_CERTExtensionsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0,  SEC_CERTExtensionTemplate}
};

/*
 * XXX Also, these templates, especially the Krl/FORTEZZA ones, need to
 * be tested; Lisa did the obvious translation but they still should be
 * verified.
 */

const SEC_ASN1Template CERT_IssuerAndSNTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTIssuerAndSN) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTIssuerAndSN,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTIssuerAndSN,issuer),
	  CERT_NameTemplate },
    { SEC_ASN1_INTEGER,
	  offsetof(CERTIssuerAndSN,serialNumber) },
    { 0 }
};

static const SEC_ASN1Template cert_KrlEntryTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrlEntry) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(CERTCrlEntry,serialNumber) },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTCrlEntry,revocationDate) },
    { 0 }
};

static const SEC_ASN1Template cert_KrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrl) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCrl,signatureAlg),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_SAVE,
	  offsetof(CERTCrl,derName) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCrl,name),
	  CERT_NameTemplate },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTCrl,lastUpdate) },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTCrl,nextUpdate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCrl,entries),
	  cert_KrlEntryTemplate },
    { 0 }
};

static const SEC_ASN1Template cert_SignedKrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTSignedCrl) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTSignedCrl,signatureWrap.data) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSignedCrl,crl),
	  cert_KrlTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSignedCrl,signatureWrap.signatureAlgorithm),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CERTSignedCrl,signatureWrap.signature) },
    { 0 }
};

static const SEC_ASN1Template cert_CrlKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrlKey) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof(CERTCrlKey,dummy) },
    { SEC_ASN1_SKIP },
    { SEC_ASN1_ANY, offsetof(CERTCrlKey,derName) },
    { SEC_ASN1_SKIP_REST },
    { 0 }
};

static const SEC_ASN1Template cert_CrlEntryTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrlEntry) },
    { SEC_ASN1_INTEGER,
	  offsetof(CERTCrlEntry,serialNumber) },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTCrlEntry,revocationDate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCrlEntry, extensions),
	  SEC_CERTExtensionTemplate},
    { 0 }
};

const SEC_ASN1Template CERT_CrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrl) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof (CERTCrl, version) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCrl,signatureAlg),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_SAVE,
	  offsetof(CERTCrl,derName) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCrl,name),
	  CERT_NameTemplate },
    { SEC_ASN1_UTC_TIME,
	  offsetof(CERTCrl,lastUpdate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_UTC_TIME,
	  offsetof(CERTCrl,nextUpdate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCrl,entries),
	  cert_CrlEntryTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_EXPLICIT | 0,
	  offsetof(CERTCrl,extensions),
	  SEC_CERTExtensionsTemplate},
    { 0 }
};

static const SEC_ASN1Template cert_SignedCrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTSignedCrl) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTSignedCrl,signatureWrap.data) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSignedCrl,crl),
	  CERT_CrlTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSignedCrl,signatureWrap.signatureAlgorithm),
	  SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CERTSignedCrl,signatureWrap.signature) },
    { 0 }
};

const SEC_ASN1Template CERT_SetOfSignedCrlTemplate[] = {
    { SEC_ASN1_SET_OF, 0, cert_SignedCrlTemplate },
};

/* Check the version of the CRL.  If there is a critical extension in the crl
   or crl entry, then the version must be v2. Otherwise, it should be v1.  If
   the crl contains critical extension(s), then we must recognized the extension's
   OID.
   */
SECStatus cert_check_crl_version (CERTCrl *crl)
{
    CERTCrlEntry **entries;
    CERTCrlEntry *entry;
    PRBool hasCriticalExten = PR_FALSE;
    SECStatus rv = SECSuccess;
    int version;

    /* CRL version is defaulted to v1 */
    version = SEC_CRL_VERSION_1;
    if (crl->version.data != 0) 
	version = (int)DER_GetUInteger (&crl->version);
	
    if (version > SEC_CRL_VERSION_2) {
	PORT_SetError (SEC_ERROR_BAD_DER);
	return (SECFailure);
    }

    /* Check the crl extensions for a critial extension.  If one is found,
       and the version is not v2, then we are done.
     */
    if (crl->extensions) {
	hasCriticalExten = cert_HasCriticalExtension (crl->extensions);
	if (hasCriticalExten) {
	    if (version != SEC_CRL_VERSION_2)
		return (SECFailure);
	    /* make sure that there is no unknown critical extension */
	    if (cert_HasUnknownCriticalExten (crl->extensions) == PR_TRUE) {
		PORT_SetError (SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
		return (SECFailure);
	    }
	}
    }

	
    if (crl->entries == NULL) {
	if (hasCriticalExten == PR_FALSE && version == SEC_CRL_VERSION_2) {
	    PORT_SetError (SEC_ERROR_BAD_DER);
	    return (SECFailure);
	}
        return (SECSuccess);
    }
    /* Look in the crl entry extensions.  If there is a critical extension,
       then the crl version must be v2; otherwise, it should be v1.
     */
    entries = crl->entries;
    while (*entries) {
	entry = *entries;
	if (entry->extensions) {
	    /* If there is a critical extension in the entries, then the
	       CRL must be of version 2.  If we already saw a critical extension,
	       there is no need to check the version again.
	    */
	    if (hasCriticalExten == PR_FALSE) {
		hasCriticalExten = cert_HasCriticalExtension (entry->extensions);
		if (hasCriticalExten && version != SEC_CRL_VERSION_2) { 
		    rv = SECFailure;
		    break;
		}
	    }

	    /* For each entry, make sure that it does not contain an unknown
	       critical extension.  If it does, we must reject the CRL since
	       we don't know how to process the extension.
	    */
	    if (cert_HasUnknownCriticalExten (entry->extensions) == PR_TRUE) {
		PORT_SetError (SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
		rv = SECFailure;
		break;
	    }
	}
	++entries;
    }
    if (rv == SECFailure)
	return (rv);

    /* There is no critical extension, but the version is set to v2 */
    if (version != SEC_CRL_VERSION_1 && hasCriticalExten == PR_FALSE) {
	PORT_SetError (SEC_ERROR_BAD_DER);
	return (SECFailure);
    }
    return (SECSuccess);
}

/*
 * Generate a database key, based on the issuer name from a
 * DER crl.
 */
SECStatus
CERT_KeyFromDERCrl(PRArenaPool *arena, SECItem *derCrl, SECItem *key)
{
    SECStatus rv;
    CERTSignedData sd;
    CERTCrlKey crlkey;

    PORT_Memset (&sd, 0, sizeof (sd));
    rv = SEC_ASN1DecodeItem (arena, &sd, CERT_SignedDataTemplate, derCrl);
    if (rv != SECSuccess) {
	return rv;
    }

    PORT_Memset (&crlkey, 0, sizeof (crlkey));
    rv = SEC_ASN1DecodeItem(arena, &crlkey, cert_CrlKeyTemplate, &sd.data);
    if (rv != SECSuccess) {
	return rv;
    }

    key->len =  crlkey.derName.len;
    key->data = crlkey.derName.data;

    return(SECSuccess);
}

/*
 * take a DER CRL or KRL  and decode it into a CRL structure
 */
CERTSignedCrl *
CERT_DecodeDERCrl(PRArenaPool *narena, SECItem *derSignedCrl, int type)
{
    PRArenaPool *arena;
    CERTSignedCrl *crl;
    SECStatus rv;

    /* make a new arena */
    if (narena == NULL) {
    	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if ( !arena ) {
	    return NULL;
	}
    } else {
	arena = narena;
    }

    /* allocate the CRL structure */
    crl = (CERTSignedCrl *)PORT_ArenaZAlloc(arena, sizeof(CERTSignedCrl));
    if ( !crl ) {
	goto loser;
    }
    
    crl->arena = arena;

    /* Save the arena in the inner crl for CRL extensions support */
    crl->crl.arena = arena;

    /* decode the CRL info */
    switch (type) {
    case SEC_CRL_TYPE: 
	rv = SEC_ASN1DecodeItem
	     (arena, crl, cert_SignedCrlTemplate, derSignedCrl);
	if (rv != SECSuccess)
	    break;

	/* If the version is set to v2, make sure that it contains at
	   least 1 critical extension either the crl extensions or
	   crl entry extensions. */
	rv =  cert_check_crl_version (&crl->crl);
	break;

    case SEC_KRL_TYPE:
	rv = SEC_ASN1DecodeItem
	     (arena, crl, cert_SignedKrlTemplate, derSignedCrl);
	break;
    default:
	rv = SECFailure;
	break;
    }

    if (rv != SECSuccess) {
	goto loser;
    }

    crl->referenceCount = 1;
    
    return(crl);
    
loser:

    if ((narena == NULL) && arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(0);
}
