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
 * $Id: crl.c,v 1.34 2003/07/08 18:41:25 wtc%netscape.com Exp $
 */
 
#include "cert.h"
#include "certi.h"
#include "secder.h"
#include "secasn1.h"
#include "secoid.h"
#include "certdb.h"
#include "certxutl.h"
#include "prtime.h"
#include "secerr.h"
#include "pk11func.h"
#include "dev.h"
#include "dev3hack.h"
#include "nssbase.h"
#ifdef USE_RWLOCK
#include "nssrwlk.h"
#endif

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

const SEC_ASN1Template CERT_CrlTemplateNoEntries[] = {
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
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF |
      SEC_ASN1_SKIP }, /* skip entries */
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_EXPLICIT | 0,
	  offsetof(CERTCrl,extensions),
	  SEC_CERTExtensionsTemplate },
    { 0 }
};

const SEC_ASN1Template CERT_CrlTemplateEntriesOnly[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrl) },
    { SEC_ASN1_SKIP | SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL },
    { SEC_ASN1_SKIP },
    { SEC_ASN1_SKIP },
    { SEC_ASN1_SKIP | SEC_ASN1_UTC_TIME },
    { SEC_ASN1_SKIP | SEC_ASN1_OPTIONAL | SEC_ASN1_UTC_TIME },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCrl,entries),
	  cert_CrlEntryTemplate }, /* decode entries */
    { SEC_ASN1_SKIP_REST },
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

static const SEC_ASN1Template cert_SignedCrlTemplateNoEntries[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTSignedCrl) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTSignedCrl,signatureWrap.data) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSignedCrl,crl),
	  CERT_CrlTemplateNoEntries },
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

#define GetOpaqueCRLFields(x) ((OpaqueCRLFields*)x->opaque)

/*
PRBool CERT_CRLIsInvalid(CERTSignedCrl* crl)
{
    OpaqueCRLFields* extended = NULL;

    if (crl && (extended = (OpaqueCRLFields*) crl->opaque)) {
        return extended->bad;
    }
    return PR_TRUE;
}
*/

SECStatus CERT_CompleteCRLDecodeEntries(CERTSignedCrl* crl)
{
    SECStatus rv = SECSuccess;
    SECItem* crldata = NULL;
    OpaqueCRLFields* extended = NULL;

    if ( (!crl) ||
        (!(extended = (OpaqueCRLFields*) crl->opaque))  ) {
        rv = SECFailure;
    } else {
        if (PR_FALSE == extended->partial) {
            /* the CRL has already been fully decoded */
            return SECSuccess;
        }
        if (PR_TRUE == extended->badEntries) {
            /* the entries decoding already failed */
            return SECFailure;
        }
        crldata = &crl->signatureWrap.data;
        if (!crldata) {
            rv = SECFailure;
        }
    }

    if (SECSuccess == rv) {
        rv = SEC_QuickDERDecodeItem(crl->arena,
            &crl->crl,
            CERT_CrlTemplateEntriesOnly,
            crldata);
        if (SECSuccess == rv) {
            extended->partial = PR_FALSE; /* successful decode, avoid
                decoding again */
        } else {
            extended->bad = PR_TRUE;
            extended->badEntries = PR_TRUE;
            /* cache the decoding failure. If it fails the first time,
               it will fail again, which will grow the arena and leak
               memory, so we want to avoid it */
        }
    }
    return rv;
}

/*
 * take a DER CRL or KRL  and decode it into a CRL structure
 * allow reusing the input DER without making a copy
 */
CERTSignedCrl *
CERT_DecodeDERCrlWithFlags(PRArenaPool *narena, SECItem *derSignedCrl,
                          int type, PRInt32 options)
{
    PRArenaPool *arena;
    CERTSignedCrl *crl;
    SECStatus rv;
    OpaqueCRLFields* extended = NULL;
    const SEC_ASN1Template* crlTemplate = cert_SignedCrlTemplate;

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
        PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    crl->arena = arena;

    /* allocate opaque fields */
    crl->opaque = (void*)PORT_ArenaZAlloc(arena, sizeof(OpaqueCRLFields));
    if ( !crl->opaque ) {
	goto loser;
    }
    extended = (OpaqueCRLFields*) crl->opaque;

    if (options & CRL_DECODE_DONT_COPY_DER) {
        crl->derCrl = derSignedCrl; /* DER is not copied . The application
                                       must keep derSignedCrl until it
                                       destroys the CRL */
    } else {
        crl->derCrl = (SECItem *)PORT_ArenaZAlloc(arena,sizeof(SECItem));
        if (crl->derCrl == NULL) {
            goto loser;
        }
        rv = SECITEM_CopyItem(arena, crl->derCrl, derSignedCrl);
        if (rv != SECSuccess) {
            goto loser;
        }
    }

    /* Save the arena in the inner crl for CRL extensions support */
    crl->crl.arena = arena;
    if (options & CRL_DECODE_SKIP_ENTRIES) {
        crlTemplate = cert_SignedCrlTemplateNoEntries;
        extended->partial = PR_TRUE;
    }

    /* decode the CRL info */
    switch (type) {
    case SEC_CRL_TYPE:
        rv = SEC_QuickDERDecodeItem(arena, crl, crlTemplate, crl->derCrl);
	if (rv != SECSuccess) {
            extended->badDER = PR_TRUE;
            break;
        }
        /* check for critical extentions */
	rv =  cert_check_crl_version (&crl->crl);
	if (rv != SECSuccess) {
            extended->badExtensions = PR_TRUE;
        }
	break;

    case SEC_KRL_TYPE:
	rv = SEC_QuickDERDecodeItem
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
    if (options & CRL_DECODE_KEEP_BAD_CRL) {
        extended->bad = PR_TRUE;
        crl->referenceCount = 1;
        return(crl);
    }

    if ((narena == NULL) && arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(0);
}

/*
 * take a DER CRL or KRL  and decode it into a CRL structure
 */
CERTSignedCrl *
CERT_DecodeDERCrl(PRArenaPool *narena, SECItem *derSignedCrl, int type)
{
    return CERT_DecodeDERCrlWithFlags(narena, derSignedCrl, type, CRL_DECODE_DEFAULT_OPTIONS);
}

/*
 * Lookup a CRL in the databases. We mirror the same fast caching data base
 *  caching stuff used by certificates....?
 * return values :
 *
 * SECSuccess means we got a valid DER CRL (passed in "decoded"), or no CRL at all
 *
 * SECFailure means we got a fatal error - most likely, we found a CRL,
 * and it failed decoding, or there was an out of memory error. Do NOT ignore
 * it and specifically do NOT treat it the same as having no CRL, as this
 * can compromise security !!! Ideally, you should treat this case as if you
 * received a "catch-all" CRL where all certs you were looking up are
 * considered to be revoked
 */
static SECStatus
SEC_FindCrlByKeyOnSlot(PK11SlotInfo *slot, SECItem *crlKey, int type,
                       CERTSignedCrl** decoded, PRInt32 decodeoptions)
{
    SECStatus rv = SECSuccess;
    CERTSignedCrl *crl = NULL;
    SECItem *derCrl = NULL;
    CK_OBJECT_HANDLE crlHandle = 0;
    char *url = NULL;
    int nsserror;

    PORT_Assert(decoded);
    if (!decoded) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* XXX it would be really useful to be able to fetch the CRL directly into an
       arena. This would avoid a copy later on in the decode step */
    PORT_SetError(0);
    derCrl = PK11_FindCrlByName(&slot, &crlHandle, crlKey, type, &url);
    if (derCrl == NULL) {
	/* if we had a problem other than the CRL just didn't exist, return
	 * a failure to the upper level */
	nsserror = PORT_GetError();
	if ((nsserror != 0) && (nsserror != SEC_ERROR_CRL_NOT_FOUND)) {
	    rv = SECFailure;
	}
	goto loser;
    }
    PORT_Assert(crlHandle != CK_INVALID_HANDLE);
    /* PK11_FindCrlByName obtained a slot reference. */
    
    crl = CERT_DecodeDERCrlWithFlags(NULL, derCrl, type, decodeoptions);
    if (crl) {
        crl->slot = slot;
        slot = NULL; /* adopt it */
        crl->pkcs11ID = crlHandle;
        if (url) {
            crl->url = PORT_ArenaStrdup(crl->arena,url);
        }
    } else {
        rv = SECFailure;
    }
    
    if (url) {
	PORT_Free(url);
    }

    if (slot) {
	PK11_FreeSlot(slot);
    }

loser:
    if (derCrl) {
        /* destroy the DER, unless a decoded CRL was returned with DER
           allocated on the heap. This is solely for cache purposes */
        if (crl && (decodeoptions & CRL_DECODE_DONT_COPY_DER)) {
            /* mark the DER as having come from the heap instead of the
               arena, so it can be destroyed */
            GetOpaqueCRLFields(crl)->heapDER = PR_TRUE;
        } else {
            SECITEM_FreeItem(derCrl, PR_TRUE);
        }
    }

    *decoded = crl;

    return rv;
}

SECStatus SEC_DestroyCrl(CERTSignedCrl *crl);

CERTSignedCrl *
crl_storeCRL (PK11SlotInfo *slot,char *url,
                  CERTSignedCrl *newCrl, SECItem *derCrl, int type)
{
    CERTSignedCrl *oldCrl = NULL, *crl = NULL;
    PRBool deleteOldCrl = PR_FALSE;
    CK_OBJECT_HANDLE crlHandle;

    PORT_Assert(newCrl);
    PORT_Assert(derCrl);

    /* we can't use the cache here because we must look in the same
       token */
    SEC_FindCrlByKeyOnSlot(slot, &newCrl->crl.derName, type,
                                &oldCrl, CRL_DECODE_SKIP_ENTRIES);

    /* if there is an old crl on the token, make sure the one we are
       installing is newer. If not, exit out, otherwise delete the
       old crl.
     */
    if (oldCrl != NULL) {
	/* if it's already there, quietly continue */
	if (SECITEM_CompareItem(newCrl->derCrl, oldCrl->derCrl) 
						== SECEqual) {
	    crl = newCrl;
	    crl->slot = PK11_ReferenceSlot(slot);
	    crl->pkcs11ID = oldCrl->pkcs11ID;
	    goto done;
	}
        if (!SEC_CrlIsNewer(&newCrl->crl,&oldCrl->crl)) {

            if (type == SEC_CRL_TYPE) {
                PORT_SetError(SEC_ERROR_OLD_CRL);
            } else {
                PORT_SetError(SEC_ERROR_OLD_KRL);
            }

            goto done;
        }

        if ((SECITEM_CompareItem(&newCrl->crl.derName,
                &oldCrl->crl.derName) != SECEqual) &&
            (type == SEC_KRL_TYPE) ) {

            PORT_SetError(SEC_ERROR_CKL_CONFLICT);
            goto done;
        }

        /* if we have a url in the database, use that one */
        if (oldCrl->url) {
	    url = oldCrl->url;
        }

        /* really destroy this crl */
        /* first drum it out of the permanment Data base */
	deleteOldCrl = PR_TRUE;
    }

    /* invalidate CRL cache for this issuer */
    CERT_CRLCacheRefreshIssuer(NULL, &newCrl->crl.derName);
    /* Write the new entry into the data base */
    crlHandle = PK11_PutCrl(slot, derCrl, &newCrl->crl.derName, url, type);
    if (crlHandle != CK_INVALID_HANDLE) {
	crl = newCrl;
	crl->slot = PK11_ReferenceSlot(slot);
	crl->pkcs11ID = crlHandle;
	if (url) {
	    crl->url = PORT_ArenaStrdup(crl->arena,url);
	}
    }

done:
    if (oldCrl) {
	if (deleteOldCrl && crlHandle != CK_INVALID_HANDLE) {
	    SEC_DeletePermCRL(oldCrl);
	}
	SEC_DestroyCrl(oldCrl);
    }

    return crl;
}

/*
 *
 * create a new CRL from DER material.
 *
 * The signature on this CRL must be checked before you
 * load it. ???
 */
CERTSignedCrl *
SEC_NewCrl(CERTCertDBHandle *handle, char *url, SECItem *derCrl, int type)
{
    CERTSignedCrl* retCrl = NULL;
    PK11SlotInfo* slot = PK11_GetInternalKeySlot();
    retCrl = PK11_ImportCRL(slot, derCrl, url, type, NULL,
        CRL_IMPORT_BYPASS_CHECKS, NULL, CRL_DECODE_DEFAULT_OPTIONS);
    PK11_FreeSlot(slot);

    return retCrl;
}
    
CERTSignedCrl *
SEC_FindCrlByDERCert(CERTCertDBHandle *handle, SECItem *derCrl, int type)
{
    PRArenaPool *arena;
    SECItem crlKey;
    SECStatus rv;
    CERTSignedCrl *crl = NULL;
    
    /* create a scratch arena */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	return(NULL);
    }
    
    /* extract the database key from the cert */
    rv = CERT_KeyFromDERCrl(arena, derCrl, &crlKey);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* find the crl */
    crl = SEC_FindCrlByName(handle, &crlKey, type);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(crl);
}

CERTSignedCrl* SEC_DupCrl(CERTSignedCrl* acrl)
{
    if (acrl)
    {
        PR_AtomicIncrement(&acrl->referenceCount);
        return acrl;
    }
    return NULL;
}

SECStatus
SEC_DestroyCrl(CERTSignedCrl *crl)
{
    if (crl) {
	if (PR_AtomicDecrement(&crl->referenceCount) < 1) {
	    if (crl->slot) {
		PK11_FreeSlot(crl->slot);
	    }
            if (PR_TRUE == GetOpaqueCRLFields(crl)->heapDER) {
                SECITEM_FreeItem(crl->derCrl, PR_TRUE);
            }
	    PORT_FreeArena(crl->arena, PR_FALSE);
	}
    }
    return SECSuccess;
}

SECStatus
SEC_LookupCrls(CERTCertDBHandle *handle, CERTCrlHeadNode **nodes, int type)
{
    CERTCrlHeadNode *head;
    PRArenaPool *arena = NULL;
    SECStatus rv;

    *nodes = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	return SECFailure;
    }

    /* build a head structure */
    head = (CERTCrlHeadNode *)PORT_ArenaAlloc(arena, sizeof(CERTCrlHeadNode));
    head->arena = arena;
    head->first = NULL;
    head->last = NULL;
    head->dbhandle = handle;

    /* Look up the proper crl types */
    *nodes = head;

    rv = PK11_LookupCrls(head, type, NULL);
    
    if (rv != SECSuccess) {
	if ( arena ) {
	    PORT_FreeArena(arena, PR_FALSE);
	    *nodes = NULL;
	}
    }

    return rv;
}

/* These functions simply return the address of the above-declared templates.
** This is necessary for Windows DLLs.  Sigh.
*/
SEC_ASN1_CHOOSER_IMPLEMENT(CERT_IssuerAndSNTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(CERT_CrlTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(CERT_SetOfSignedCrlTemplate)

/*
** Pre-allocator hash allocator ops.
*/
static void * PR_CALLBACK
PreAllocTable(void *pool, PRSize size)
{
    PreAllocator* alloc = (PreAllocator*)pool;
    PR_ASSERT(alloc);
    if (!alloc)
    {
        /* no allocator, or buffer full */
        return NULL;
    }
    if (size > (alloc->len - alloc->used))
    {
        alloc->extra += size;
        return PORT_ArenaAlloc(alloc->arena, size);
    }
    alloc->used += size;
    return (char*) alloc->data + alloc->used - size;
}

static void PR_CALLBACK
PreFreeTable(void *pool, void *item)
{
}

static PLHashEntry * PR_CALLBACK
PreAllocEntry(void *pool, const void *key)
{
    return PreAllocTable(pool, sizeof(PLHashEntry));
}

static void PR_CALLBACK
PreFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
}

static PLHashAllocOps preAllocOps = {
    PreAllocTable, PreFreeTable,
    PreAllocEntry, PreFreeEntry
};

void PreAllocator_Destroy(PreAllocator* PreAllocator)
{
    if (!PreAllocator)
    {
        return;
    }
    if (PreAllocator->arena)
    {
        PORT_FreeArena(PreAllocator->arena, PR_TRUE);
    }
    if (PreAllocator->data)
    {
        PORT_Free(PreAllocator->data);
    }
    PORT_Free(PreAllocator);
}

PreAllocator* PreAllocator_Create(PRSize size)
{
    PreAllocator prebuffer;
    PreAllocator* prepointer = NULL;
    memset(&prebuffer, 0, sizeof(PreAllocator));
    prebuffer.len = size;
    prebuffer.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    PR_ASSERT(prebuffer.arena);
    if (!prebuffer.arena) {
        PreAllocator_Destroy(&prebuffer);
        return NULL;
    }
    if (prebuffer.len) {
        prebuffer.data = PR_Malloc(prebuffer.len);
        if (!prebuffer.data) {
            PreAllocator_Destroy(&prebuffer);
            return NULL;
        }
    } else {
        prebuffer.data = NULL;
    }
    prepointer = (PreAllocator*)PR_Malloc(sizeof(PreAllocator));
    if (!prepointer) {
        PreAllocator_Destroy(&prebuffer);
        return NULL;
    }
    *prepointer = prebuffer;
    return prepointer;
}

static CRLCache crlcache = { NULL, NULL };

static PRBool crlcache_initialized = PR_FALSE;

/* this needs to be called at NSS initialization time */

SECStatus InitCRLCache(void)
{
    if (PR_FALSE == crlcache_initialized)
    {
        PR_ASSERT(NULL == crlcache.lock);
        crlcache.lock = PR_NewLock();
        if (!crlcache.lock)
        {
            return SECFailure;
        }
        PR_ASSERT(NULL == crlcache.issuers);
        crlcache.issuers = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
                                  PL_CompareValues, NULL, NULL);
        if (!crlcache.issuers)
        {
            PR_DestroyLock(crlcache.lock);
            crlcache.lock = NULL;
            return SECFailure;
        }
        crlcache_initialized = PR_TRUE;
        return SECSuccess;
    }
    else
    {
        PR_ASSERT(crlcache.lock);
        PR_ASSERT(crlcache.issuers);
        if ( (NULL == crlcache.lock) || (NULL == crlcache.issuers) )
        {
            return SECFailure;
        }
        else
        {
            return SECSuccess;
        }
    }
}

SECStatus DPCache_Destroy(CRLDPCache* cache)
{
    PRUint32 i = 0;
    PR_ASSERT(cache);
    if (!cache) {
        return SECFailure;
    }
    if (cache->lock)
    {
#ifdef USE_RWLOCK
        NSSRWLock_Destroy(cache->lock);
#else
        PR_DestroyLock(cache->lock);
#endif
    }
    /* destroy all our CRL objects */
    for (i=0;i<cache->ncrls;i++)
    {
        SEC_DestroyCrl(cache->crls[i]);
    }
    /* free the array of CRLs */
    if (cache->crls)
    {
	PR_Free(cache->crls);
    }
    /* destroy the hash table */
    if (cache->entries)
    {
        PL_HashTableDestroy(cache->entries);
    }
    /* free the pre buffer */
    if (cache->prebuffer)
    {
        PreAllocator_Destroy(cache->prebuffer);
    }
    /* destroy the cert */
    if (cache->issuer)
    {
        CERT_DestroyCertificate(cache->issuer);
    }
    /* free the subject */
    if (cache->subject)
    {
        SECITEM_FreeItem(cache->subject, PR_TRUE);
    }
    /* free the distribution points */
    if (cache->distributionPoint)
    {
        SECITEM_FreeItem(cache->distributionPoint, PR_TRUE);
    }
    return SECSuccess;
}

SECStatus IssuerCache_Destroy(CRLIssuerCache* cache)
{
    PORT_Assert(cache);
    if (!cache)
    {
        return SECFailure;
    }
#if 0
    /* XCRL */
    if (cache->lock)
    {
        NSSRWLock_Destroy(cache->lock);
    }
    if (cache->issuer)
    {
        CERT_DestroyCertificate(cache->issuer);
    }
#endif
    /* free the subject */
    if (cache->subject)
    {
        SECITEM_FreeItem(cache->subject, PR_TRUE);
    }
    DPCache_Destroy(&cache->dp);
    PR_Free(cache);
    return SECSuccess;
}

PRIntn PR_CALLBACK FreeIssuer(PLHashEntry *he, PRIntn i, void *arg)
{
    CRLIssuerCache* issuer = NULL;
    PR_ASSERT(he);
    if (!he) {
        return HT_ENUMERATE_NEXT;
    }
    issuer = (CRLIssuerCache*) he->value;
    PR_ASSERT(issuer);
    if (issuer) {
        IssuerCache_Destroy(issuer);
    }
    return HT_ENUMERATE_NEXT;
}

SECStatus ShutdownCRLCache(void)
{
    if (!crlcache.lock || !crlcache.issuers)
    {
        return SECFailure;
    }
    /* empty the cache */
    PL_HashTableEnumerateEntries(crlcache.issuers, &FreeIssuer, NULL);
    PL_HashTableDestroy(crlcache.issuers);
    crlcache.issuers = NULL;
    PR_DestroyLock(crlcache.lock);
    crlcache.lock = NULL;
    crlcache_initialized = PR_FALSE;
    return SECSuccess;
}

SECStatus DPCache_AddCRL(CRLDPCache* cache, CERTSignedCrl* crl)
{
    CERTSignedCrl** newcrls = NULL;
    PORT_Assert(cache);
    PORT_Assert(crl);
    if (!cache || !crl) {
        return SECFailure;
    }

    newcrls = (CERTSignedCrl**)PORT_Realloc(cache->crls,
        (cache->ncrls+1)*sizeof(CERTSignedCrl*));
    if (!newcrls) {
        return SECFailure;
    }
    cache->crls = newcrls;
    cache->ncrls++;
    cache->crls[cache->ncrls-1] = crl;
    return SECSuccess;
}

SECStatus DPCache_Cleanup(CRLDPCache* cache)
{
    /* remove deleted CRLs from memory */
    PRUint32 i = 0;
    PORT_Assert(cache);
    if (!cache) {
        return SECFailure;
    }
    for (i=0;i<cache->ncrls;i++) {
        CERTSignedCrl* acrl = cache->crls[i];
        if (acrl && (PR_TRUE == GetOpaqueCRLFields(acrl)->deleted)) {
            cache->crls[i] = cache->crls[cache->ncrls-1];
            cache->crls[cache->ncrls-1] = NULL;
            cache->ncrls--;
        }
    }
    return SECSuccess;
}

PRBool CRLStillExists(CERTSignedCrl* crl)
{
    NSSItem newsubject;
    SECItem subject;
    CK_ULONG crl_class;
    PRStatus status;
    PK11SlotInfo* slot = NULL;
    nssCryptokiObject instance;
    NSSArena* arena;
    PRBool xstatus = PR_TRUE;
    SECItem* oldSubject = NULL;

    PORT_Assert(crl);
    if (!crl) {
        return PR_FALSE;
    }
    slot = crl->slot;
    PORT_Assert(slot);
    if (!slot) {
        return PR_FALSE;
    }
    oldSubject = &crl->crl.derName;
    PR_ASSERT(oldSubject);
    if (!oldSubject) {
        return PR_FALSE;
    }

    /* query subject and type attributes in order to determine if the
       object has been deleted */

    /* first, make an nssCryptokiObject */
    instance.handle = crl->pkcs11ID;
    PORT_Assert(instance.handle);
    if (!instance.handle) {
        return PR_FALSE;
    }
    instance.token = PK11Slot_GetNSSToken(slot);
    PORT_Assert(instance.token);
    if (!instance.token) {
        return PR_FALSE;
    }
    instance.isTokenObject = PR_TRUE;
    instance.label = NULL;

    arena = NSSArena_Create();
    PORT_Assert(arena);
    if (!arena) {
        return PR_FALSE;
    }

    status = nssCryptokiCRL_GetAttributes(&instance,
                                          NULL,  /* XXX sessionOpt */
                                          arena,
                                          NULL,
                                          &newsubject,  /* subject */
                                          &crl_class,   /* class */
                                          NULL,
                                          NULL);
    if (PR_SUCCESS == status) {
        subject.data = newsubject.data;
        subject.len = newsubject.size;
        if (SECITEM_CompareItem(oldSubject, &subject) != SECEqual) {
            xstatus = PR_FALSE;
        }
        if (CKO_NETSCAPE_CRL != crl_class) {
            xstatus = PR_FALSE;
        }
    } else {
        xstatus = PR_FALSE;
    }
    NSSArena_Destroy(arena);
    return xstatus;
}

SECStatus DPCache_Refresh(CRLDPCache* cache, CERTSignedCrl* crlobject,
                          void* wincx)
{
    SECStatus rv = SECSuccess;
    /*  Check if it is an invalid CRL
        if we got a bad CRL, we want to cache it in order to avoid
        subsequent fetches of this same identical bad CRL. We set
        the cache to the invalid state to ensure that all certs
        on this DP are considered revoked from now on. The cache
        object will remain in this state until the bad CRL object
        is removed from the token it was fetched from. If the cause
        of the failure is that we didn't have the issuer cert to
        verify the signature, this state can be cleared when
        the issuer certificate becomes available if that causes the
        signature to verify */

    if (PR_TRUE == GetOpaqueCRLFields(crlobject)->bad) {
        PORT_SetError(SEC_ERROR_BAD_DER);
        cache->invalid |= CRL_CACHE_INVALID_CRLS;
        return SECSuccess;
    } else {
        SECStatus signstatus = SECFailure;
        if (cache->issuer) {
            int64 issuingDate = 0;
            signstatus = DER_UTCTimeToTime(&issuingDate, &crlobject->crl.lastUpdate);
            if (SECSuccess == signstatus) {
                signstatus = CERT_VerifySignedData(&crlobject->signatureWrap,
                                                    cache->issuer, issuingDate, wincx);
            }
        }
        if (SECSuccess != signstatus) {
            if (!cache->issuer) {
                /* we tried to verify without an issuer cert . This is
                   because this CRL came through a call to SEC_FindCrlByName.
                   So we don't cache this verification failure. We'll try
                   to verify the CRL again when a certificate from that issuer
                   becomes available */
                GetOpaqueCRLFields(crlobject)->unverified = PR_TRUE;
            } else {
                GetOpaqueCRLFields(crlobject)->unverified = PR_FALSE;
            }
            PORT_SetError(SEC_ERROR_CRL_BAD_SIGNATURE);
            cache->invalid |= CRL_CACHE_INVALID_CRLS;
            return SECSuccess;
        } else {
            GetOpaqueCRLFields(crlobject)->unverified = PR_FALSE;
        }
    }
    
    /* complete the entry decoding */
    rv = CERT_CompleteCRLDecodeEntries(crlobject);
    if (SECSuccess == rv) {
        /* XCRL : if this is a delta, add it to the hash table */
        /* for now, always build the hash table from the full CRL */
        CERTCrlEntry** crlEntry = NULL;
        PRUint32 numEntries = 0;
        if (cache->entries) {
            /* we already have a hash table, destroy it */
            PL_HashTableDestroy(cache->entries);
            cache->entries = NULL;
        }
        /* also destroy the PreAllocator */
        if (cache->prebuffer)
        {
            PreAllocator_Destroy(cache->prebuffer);
            cache->prebuffer = NULL;
        }
        /* count CRL entries so we can pre-allocate space for hash table entries */
        for (crlEntry = crlobject->crl.entries; crlEntry && *crlEntry; crlEntry++) {
            numEntries++;
        }
        cache->prebuffer = PreAllocator_Create(numEntries*sizeof(PLHashEntry));
        PR_ASSERT(cache->prebuffer);
        if (cache->prebuffer) {
            /* create a new hash table */
            cache->entries = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
                                      PL_CompareValues, &preAllocOps, cache->prebuffer);
        }
        PR_ASSERT(cache->entries);
        if (!cache->entries) {
            rv = SECFailure;
        }
        if (SECSuccess == rv) {
            /* add all serial numbers to the hash table */
            for (crlEntry = crlobject->crl.entries; crlEntry && *crlEntry; crlEntry++) {
                PL_HashTableAdd(cache->entries, &(*crlEntry)->serialNumber, *crlEntry);
            }
            cache->full = crlobject;
            cache->invalid = 0; /* valid cache */
        } else {
            cache->invalid |= CRL_CACHE_OUT_OF_MEMORY;
        }
    } else {
        cache->invalid |= CRL_CACHE_INVALID_CRLS;
    }
    return rv;
}

void DPCache_Empty(CRLDPCache* cache)
{
    PRUint32 i;
    PR_ASSERT(cache);
    if (!cache)
    {
        return;
    }
    cache->full = NULL;

    cache->invalid = 0;

    if (cache->entries) {
        /* we already have a hash table, destroy it */
        PL_HashTableDestroy(cache->entries);
        cache->entries = NULL;
    }
    /* also destroy the PreAllocator */
    if (cache->prebuffer)
    {
        PreAllocator_Destroy(cache->prebuffer);
        cache->prebuffer = NULL;
    }

    for (i=0;i<cache->ncrls;i++)
    {
        CERTSignedCrl* crl = cache->crls[i];
        if (crl)
        {
            GetOpaqueCRLFields(crl)->deleted = PR_TRUE;
        }
    }
}

SECStatus DPCache_Fetch(CRLDPCache* cache, void* wincx)
{
    SECStatus rv = SECSuccess;
    CERTSignedCrl* crlobject = NULL;
    PRUint32 i=0;
    /* XCRL For now, we can only get one full CRL. In the future, we'll be able to
       find more than one object, because of multiple tokens and deltas */
    rv = SEC_FindCrlByKeyOnSlot(NULL, cache->subject, SEC_CRL_TYPE,
                                &crlobject, CRL_DECODE_DONT_COPY_DER |
                                            CRL_DECODE_SKIP_ENTRIES  |
                                            CRL_DECODE_KEEP_BAD_CRL);
    /* if this function fails, something very wrong happened, such as an out
       of memory error during CRL decoding. We don't want to proceed and must
       mark the cache object invalid */
    if (SECFailure == rv) {
        cache->invalid |= CRL_CACHE_LAST_FETCH_FAILED;
    } else {
        cache->invalid &= (~CRL_CACHE_LAST_FETCH_FAILED);
    }

    if ((SECSuccess == rv) && (!crlobject)) {
        /* no CRL was found. This is OK */
        DPCache_Empty(cache);
        return SECSuccess;
    }

    /* now check if we already have a binary equivalent DER CRL */
    for (i=0;i<cache->ncrls;i++) {
        CERTSignedCrl* existing = cache->crls[i];
        if (existing && (SECEqual == SECITEM_CompareItem(existing->derCrl, crlobject->derCrl))) {
            /* yes. Has the matching CRL been marked deleted ? */
            if (PR_TRUE == GetOpaqueCRLFields(crlobject)->deleted) {
                /* Yes. Just replace the CK object ID and slot in the existing object.
                   This avoids an unnecessary signature verification & entry decode */
                /* XCRL we'll need to lock the CRL here in the future for iCRLs that are
                   shared between multiple CAs */
                existing->pkcs11ID = crlobject->pkcs11ID;
                PK11_FreeSlot(existing->slot); /* release reference to old
                                                  CRL slot */
                existing->slot = crlobject->slot; /* adopt new CRL slot */
                crlobject->slot = NULL; /* clear slot to avoid double-freeing it
                                           during CRL destroy */
                rv = SEC_DestroyCrl(crlobject);
                PORT_Assert(SECSuccess == rv);
                return rv;
            } else {
                /* We got an identical CRL from a different token.
                   Throw it away. */
                return SEC_DestroyCrl(crlobject);
            }
        }
    }

    /* add the CRL to our array */
    if (SECSuccess == rv) {
        rv = DPCache_AddCRL(cache, crlobject);
    }

    /* update the cache with this new CRL */
    if (SECSuccess == rv) {
        rv = DPCache_Refresh(cache, crlobject, wincx);
    }
    return rv;
}

SECStatus DPCache_Lookup(CRLDPCache* cache, SECItem* sn, CERTCrlEntry** returned)
{
    CERTSignedCrl* crl = NULL;
    CERTCrlEntry* acrlEntry = NULL;
    if (!cache || !sn) {
        /* no cache or SN to look up, this is bad */
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (0 != cache->invalid) {
        /* the cache contains a bad CRL, consider all certs revoked
           as a security measure */
        PORT_SetError(SEC_ERROR_CRL_INVALID);
        return SECFailure;
    }
    if (!cache->full) {
        /* no CRL means no entry to return, but this is OK */
        *returned = NULL;
        return SECSuccess;
    }
    crl = cache->full;
    PR_ASSERT(cache->entries);
    if (!cache->entries)
    {
        return SECFailure;
    }
    acrlEntry = PL_HashTableLookup(cache->entries, (void*)sn);
    if (acrlEntry)
    {
        *returned = acrlEntry;
    }
    return SECSuccess;
}

#ifdef USE_RWLOCK

#define DPCache_LockWrite() { \
                if (readlocked){ \
                    NSSRWLock_UnlockRead(cache->lock); \
                } \
                NSSRWLock_LockWrite(cache->lock); \
}

#define DPCache_UnlockWrite() { \
                if (readlocked){ \
                    NSSRWLock_LockRead(cache->lock); \
                } \
                NSSRWLock_UnlockWrite(cache->lock); \
}

#else

#define DPCache_LockWrite() {}

#define DPCache_UnlockWrite() {}

#endif

SECStatus DPCache_Update(CRLDPCache* cache, CERTCertificate* issuer,
                         void* wincx, PRBool readlocked)
{
    /* Update the CRLDPCache now. We don't cache token CRL lookup misses
       yet, as we have no way of getting notified of new PKCS#11 object
       creation that happens in a token  */
    SECStatus rv = SECSuccess;
    PRUint32 i = 0;
    PRBool updated = PR_FALSE;

    if (!cache) {
        return SECFailure;
    }

    /* verify CRLs that couldn't be checked when inserted into the cache
       because the issuer cert was unavailable. These are CRLs that were
       inserted into the cache through SEC_FindCrlByName, rather than
       through a certificate verification (CERT_CheckCRL) */
    if (issuer) {
        /* if we didn't have a valid issuer cert yet, but we do now. add it */
        if (NULL == cache->issuer) {
            /* save the issuer cert */
            cache->issuer = CERT_DupCertificate(issuer);
        }

        /* re-process all unverified CRLs */
        if (cache->issuer) {
            for (i = 0; i < cache->ncrls ; i++) {
                CERTSignedCrl* acrl = cache->crls[i];
                if (PR_TRUE == GetOpaqueCRLFields(acrl)->unverified) {
                    DPCache_LockWrite();
                    /* check that we are the first thread to update */
                    if (PR_TRUE == GetOpaqueCRLFields(acrl)->unverified) {
                        DPCache_Refresh(cache, acrl, wincx);
                        /* also check all the other CRLs */
                        for (i = i+1 ; i < cache->ncrls ; i++) {
                            acrl = cache->crls[i];
                            if (acrl && (PR_TRUE == GetOpaqueCRLFields(acrl)->unverified)) {
                                DPCache_Refresh(cache, acrl, wincx);
                            }
                        }
                    }
                    DPCache_UnlockWrite();
                    break;
                }
            }
        }
    }

    if (cache->ncrls) {
        /* check if all CRLs still exist */
        for (i = 0; (i < cache->ncrls) && (PR_FALSE == updated); i++)
        {
            CERTSignedCrl* savcrl = cache->crls[i];
            if (savcrl && (PR_TRUE != CRLStillExists(savcrl))) {
                
                /* this CRL is gone */
                DPCache_LockWrite();
                /* first, we need to check if another thread updated
                   it before we did, and abort if it has been modified since
                   we acquired the lock */
                if ((savcrl == cache->crls[i]) &&
                    PR_TRUE != CRLStillExists(savcrl)) {
                    /* the CRL is gone. And we are the one to do the update */
                    /* Mark the CRL deleted */
                    GetOpaqueCRLFields(savcrl)->deleted = PR_TRUE;
                    /* also check all the other CRLs */
                    for (i = i+1 ; i < cache->ncrls ; i++) {
                        CERTSignedCrl* acrl = cache->crls[i];
                        if (acrl && (PR_TRUE != CRLStillExists(acrl))) {
                            GetOpaqueCRLFields(acrl)->deleted = PR_TRUE;
                        }
                    }
                    /* and try to fetch a new one */
                    rv = DPCache_Fetch(cache, wincx);
                    updated = PR_TRUE;
                    if (SECSuccess == rv) {
                        rv = DPCache_Cleanup(cache); /* clean up deleted CRLs
                                                   from the cache*/
                    }
                }
                DPCache_UnlockWrite();
            }
        }
    } else {
        /* we had zero CRL for this DP, try to get one from tokens */
        DPCache_LockWrite();
        /* check if another thread updated before us, and skip update if so */
        if (0 == cache->ncrls)
        {
            /* we are the first */
            rv = DPCache_Fetch(cache, wincx);
        }
        DPCache_UnlockWrite();
    }

    return rv;
}

SECStatus DPCache_Initialize(CRLDPCache* cache, CERTCertificate* issuer,
                             SECItem* subject, SECItem* dp)
{
    PORT_Assert(cache);
    if (!cache) {
        return SECFailure;
    }
    memset(cache, 0, sizeof(CRLDPCache));
#ifdef USE_RWLOCK
    cache->lock = NSSRWLock_New(NSS_RWLOCK_RANK_NONE, NULL);
#else
    cache->lock = PR_NewLock();
#endif
    if (!cache->lock)
    {
        return SECFailure;
    }
    if (issuer)
    {
        cache->issuer = CERT_DupCertificate(issuer);
    }
    cache->distributionPoint = SECITEM_DupItem(dp);
    cache->subject = SECITEM_DupItem(subject);
    return SECSuccess;
}

SECStatus IssuerCache_Create(CRLIssuerCache** returned,
                             CERTCertificate* issuer,
                             SECItem* subject, SECItem* dp)
{
    SECStatus rv = SECSuccess;
    CRLIssuerCache* cache = NULL;
    PORT_Assert(returned);
    PORT_Assert(subject);
    if (!returned || !subject)
    {
        return SECFailure;
    }
    cache = (CRLIssuerCache*) PR_Malloc(sizeof(CRLIssuerCache));
    if (!cache)
    {
        return SECFailure;
    }
    memset(cache, 0, sizeof(CRLIssuerCache));
    cache->subject = SECITEM_DupItem(subject);
#if 0
    /* XCRL */
    cache->lock = NSSRWLock_New(NSS_RWLOCK_RANK_NONE, NULL);
    if (!cache->lock)
    {
        rv = SECFailure;
    }
    if (SECSuccess == rv && issuer)
    {
        cache->issuer = CERT_DupCertificate(issuer);
        if (!cache->issuer)
        {
            rv = SECFailure;
        }
    }
#endif
    if (SECSuccess != rv)
    {
        return IssuerCache_Destroy(cache);
    }
    *returned = cache;
    return SECSuccess;
}

SECStatus IssuerCache_AddDP(CRLIssuerCache* cache, CERTCertificate* issuer,
                            SECItem* subject, SECItem* dp, CRLDPCache** newdpc)
{
    SECStatus rv = SECSuccess;
    /* now create the required DP cache object */
    if (!dp) {
        /* default distribution point */
        rv = DPCache_Initialize(&cache->dp, issuer, subject, NULL);
        if (SECSuccess == rv) {
            cache->dpp = &cache->dp;
            if (newdpc) {
                *newdpc = cache->dpp;
            }
        }
    } else {
        /* we should never hit this until we support multiple DPs */
        PORT_Assert(dp);
        rv = SECFailure;
        /* XCRL allocate a new distribution point cache object, initialize it,
           and add it to the hash table of DPs */
    }
    return rv;
}

SECStatus CRLCache_AddIssuer(CRLIssuerCache* issuer)
{    
    PORT_Assert(issuer);
    PORT_Assert(crlcache.issuers);
    if (!issuer || !crlcache.issuers) {
        return SECFailure;
    }
    if (NULL == PL_HashTableAdd(crlcache.issuers, (void*) issuer->subject,
                                (void*) issuer)) {
        return SECFailure;
    }
    return SECSuccess;
}

SECStatus GetIssuerCache(CRLCache* cache, SECItem* subject, CRLIssuerCache** returned)
{
    /* we need to look up the issuer in the hash table */
    SECStatus rv = SECSuccess;
    PORT_Assert(cache);
    PORT_Assert(subject);
    PORT_Assert(returned);
    PORT_Assert(crlcache.issuers);
    if (!cache || !subject || !returned || !crlcache.issuers) {
        rv = SECFailure;
    }

    if (SECSuccess == rv){
        *returned = (CRLIssuerCache*) PL_HashTableLookup(crlcache.issuers,
                                                         (void*) subject);
    }

    return rv;
}

CERTSignedCrl* GetBestCRL(CRLDPCache* cache)
{
    PRUint32 i = 0;
    PR_ASSERT(cache);
    if (!cache) {
        return NULL;
    }
    if (0 == cache->ncrls) {
        /* no CRLs in the cache */
        return NULL;
    }
    /* first, check if we have a valid full CRL, and use that */
    if (cache->full) {
        return SEC_DupCrl(cache->full);
    }
    /* otherwise, check all the fetched CRLs for one with valid DER */
    for (i = 0; i < cache->ncrls ; i++) {
        CERTSignedCrl* acrl = cache->crls[i];
        if (PR_FALSE == GetOpaqueCRLFields(acrl)->bad) {
            SECStatus rv = CERT_CompleteCRLDecodeEntries(acrl);
            if (SECSuccess == rv) {
                return SEC_DupCrl(acrl);
            }
        }
    }
    return NULL;
}

CRLDPCache* GetDPCache(CRLIssuerCache* cache, SECItem* dp)
{
    CRLDPCache* dpp = NULL;
    PORT_Assert(cache);
    /* XCRL for now we only support the "default" DP, ie. the
       full CRL. So we can return the global one without locking. In
       the future we will have a lock */
    PORT_Assert(NULL == dp);
    if (!cache || dp) {
        return NULL;
    }
#if 0
    /* XCRL */
    NSSRWLock_LockRead(cache->lock);
#endif
    dpp = cache->dpp;
#if 0
    /* XCRL */
    NSSRWLock_UnlockRead(cache->lock);
#endif
    return dpp;
}

SECStatus AcquireDPCache(CERTCertificate* issuer, SECItem* subject, SECItem* dp,
                         int64 t, void* wincx, CRLDPCache** dpcache,
                         PRBool* writeLocked)
{
    SECStatus rv = SECSuccess;
    CRLIssuerCache* issuercache = NULL;

    PORT_Assert(crlcache.lock);
    if (!crlcache.lock) {
        /* CRL cache is not initialized */
        return SECFailure;
    }
    PR_Lock(crlcache.lock);
    rv = GetIssuerCache(&crlcache, subject, &issuercache);
    if (SECSuccess != rv) {
        PR_Unlock(crlcache.lock);
        return SECFailure;
    }
    if (!issuercache) {
        /* there is no cache for this issuer yet. This means this is the
           first time we look up a cert from that issuer, and we need to
           create the cache. Do it within the global cache lock to ensure
           no two threads will simultaneously try to create the same issuer
           cache. XXX this could be optimized with a r/w lock at this level
           too. But the code would have to check if it already exists when
           adding to the hash table */
        
        rv = IssuerCache_Create(&issuercache, issuer, subject, dp);
        if (SECSuccess == rv && !issuercache) {
            PORT_Assert(issuercache);
            rv = SECFailure;
        }

        if (SECSuccess == rv) {
            /* This is the first time we look up a cert of this issuer.
               Create the DPCache for this DP . */
            rv = IssuerCache_AddDP(issuercache, issuer, subject, dp, dpcache);
        }

        if (SECSuccess == rv) {
            /* lock the DPCache for write to ensure the update happens in this thread */
            *writeLocked = PR_TRUE;
#ifdef USE_RWLOCK
            NSSRWLock_LockWrite((*dpcache)->lock);
#else
            PR_Lock((*dpcache)->lock);
#endif
        }
        
        if (SECSuccess == rv) {
            /* now add the new issuer cache to the global hash table of issuers */
            rv = CRLCache_AddIssuer(issuercache);
            if (SECSuccess != rv) {
                /* failure */
                rv = SECFailure;
            }
        }

        /* now unlock the global cache. We only want to lock the hash table
           addition. Holding it longer would hurt scalability */
        PR_Unlock(crlcache.lock);

        if (SECSuccess != rv && issuercache) {
            if (PR_TRUE == *writeLocked) {
#ifdef USE_RWLOCK
                NSSRWLock_UnlockWrite((*dpcache)->lock);
#else
                PR_Unlock((*dpcache)->lock);
#endif
            }
            IssuerCache_Destroy(issuercache);
            issuercache = NULL;
        }

        if (SECSuccess != rv) {
            return SECFailure;
        }
    } else {
        PR_Unlock(crlcache.lock);
        *dpcache = GetDPCache(issuercache, dp);
    }
    /* we now have a DPCache that we can use for lookups */
    /* lock it for read, unless we already locked for write */
    if (PR_FALSE == *writeLocked)
    {
#ifdef USE_RWLOCK
        NSSRWLock_LockRead((*dpcache)->lock);
#else
        PR_Lock((*dpcache)->lock);
#endif
    }
    
    if (SECSuccess == rv) {
        /* currently there is always one and only one DPCache */
        PORT_Assert(*dpcache);
        if (*dpcache)
        {
            /* make sure the DP cache is up to date before using it */
            rv = DPCache_Update(*dpcache, issuer, wincx, PR_FALSE == *writeLocked);
        }
        else
        {
            rv = SECFailure;
        }
    }
    return rv;
}

void ReleaseDPCache(CRLDPCache* dpcache, PRBool writeLocked)
{
    if (!dpcache) {
        return;
    }
    if (PR_TRUE == writeLocked) {
#ifdef USE_RWLOCK
        NSSRWLock_UnlockWrite(dpcache->lock);
#else
        PR_Unlock(dpcache->lock);
#endif
    } else {
#ifdef USE_RWLOCK
        NSSRWLock_UnlockRead(dpcache->lock);
#else
        PR_Unlock(dpcache->lock);
#endif
    }
}

SECStatus
CERT_CheckCRL(CERTCertificate* cert, CERTCertificate* issuer, SECItem* dp,
              int64 t, void* wincx)
{
    PRBool lockedwrite = PR_FALSE;
    SECStatus rv = SECSuccess;
    CRLDPCache* dpcache = NULL;
    if (!cert || !issuer) {
        return SECFailure;
    }

    rv = AcquireDPCache(issuer, &issuer->derSubject, dp, t, wincx, &dpcache, &lockedwrite);
    
    if (SECSuccess == rv) {
        /* now look up the certificate SN in the DP cache's CRL */
        CERTCrlEntry* entry = NULL;
        rv = DPCache_Lookup(dpcache, &cert->serialNumber, &entry);
        if (SECSuccess == rv && entry) {
            /* check the time if we have one */
            if (entry->revocationDate.data && entry->revocationDate.len) {
                int64 revocationDate = 0;
                if (SECSuccess == DER_UTCTimeToTime(&revocationDate,
                                                    &entry->revocationDate)) {
                    /* we got a good revocation date, only consider the
                       certificate revoked if the time we are inquiring about
                       is past the revocation date */
                    if (t>=revocationDate) {
                        rv = SECFailure;
                    }
                } else {
                    /* invalid revocation date, consider the certificate
                       permanently revoked */
                    rv = SECFailure;
                }
            } else {
                /* no revocation date, certificate is permanently revoked */
                rv = SECFailure;
            }
            if (SECFailure == rv) {
                PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
            }
        }
    }

    ReleaseDPCache(dpcache, lockedwrite);
    return rv;
}

CERTSignedCrl *
SEC_FindCrlByName(CERTCertDBHandle *handle, SECItem *crlKey, int type)
{
    CERTSignedCrl* acrl = NULL;
    CRLDPCache* dpcache = NULL;
    SECStatus rv = SECSuccess;
    PRBool writeLocked = PR_FALSE;

    rv = AcquireDPCache(NULL, crlKey, NULL, 0, NULL, &dpcache, &writeLocked);
    if (SECSuccess == rv)
    {
        acrl = GetBestCRL(dpcache);
        ReleaseDPCache(dpcache, writeLocked);
    }
    return acrl;
}

void CERT_CRLCacheRefreshIssuer(CERTCertDBHandle* dbhandle, SECItem* crlKey)
{
    CERTSignedCrl* acrl = NULL;
    CRLDPCache* cache = NULL;
    SECStatus rv = SECSuccess;
    PRBool writeLocked = PR_FALSE;

    (void) dbhandle; /* silence compiler warnings */

    rv = AcquireDPCache(NULL, crlKey, NULL, 0, NULL, &cache, &writeLocked);
    if (SECSuccess != rv)
    {
        return;
    }
    if (PR_TRUE == writeLocked)
    {
        /* the DPCache is write-locked. This means that the issuer was just
           added to the CRL cache. There is no need to do anything */
    }
    else
    {
        PRBool readlocked = PR_TRUE;
        /* we need to invalidate the DPCache here */
        DPCache_LockWrite();
        DPCache_Empty(cache);
        DPCache_Cleanup(cache);
        DPCache_UnlockWrite();
    }
    ReleaseDPCache(cache, writeLocked);
    return;
}

