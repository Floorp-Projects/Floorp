/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Moved from secpkcs7.c
 *
 * $Id$
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
#if defined(DPC_RWLOCK) || defined(GLOBAL_RWLOCK)
#include "nssrwlk.h"
#endif
#include "pk11priv.h"

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
 * XXX Also, these templates need to be tested; Lisa did the obvious
 * translation but they still should be verified.
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

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(CERT_TimeChoiceTemplate)

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
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTCrlEntry,revocationDate),
          SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCrlEntry, extensions),
	  SEC_CERTExtensionTemplate},
    { 0 }
};

const SEC_ASN1Template CERT_CrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCrl) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL, offsetof (CERTCrl, version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTCrl,signatureAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate)},
    { SEC_ASN1_SAVE,
	  offsetof(CERTCrl,derName) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCrl,name),
	  CERT_NameTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTCrl,lastUpdate),
          SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN,
	  offsetof(CERTCrl,nextUpdate),
          SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
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
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTCrl,signatureAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTCrl,derName) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCrl,name),
	  CERT_NameTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTCrl,lastUpdate),
          SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN,
	  offsetof(CERTCrl,nextUpdate),
          SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
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
    { SEC_ASN1_SKIP | SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(CERTCrl,lastUpdate),
        SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { SEC_ASN1_SKIP | SEC_ASN1_INLINE | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN,
        offsetof(CERTCrl,nextUpdate),
        SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTCrl,entries),
	  cert_CrlEntryTemplate }, /* decode entries */
    { SEC_ASN1_SKIP_REST },
    { 0 }
};

const SEC_ASN1Template CERT_SignedCrlTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTSignedCrl) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTSignedCrl,signatureWrap.data) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTSignedCrl,crl),
	  CERT_CrlTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN ,
	  offsetof(CERTSignedCrl,signatureWrap.signatureAlgorithm),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
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
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTSignedCrl,signatureWrap.signatureAlgorithm),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CERTSignedCrl,signatureWrap.signature) },
    { 0 }
};

const SEC_ASN1Template CERT_SetOfSignedCrlTemplate[] = {
    { SEC_ASN1_SET_OF, 0, CERT_SignedCrlTemplate },
};

/* get CRL version */
int cert_get_crl_version(CERTCrl * crl)
{
    /* CRL version is defaulted to v1 */
    int version = SEC_CRL_VERSION_1;
    if (crl && crl->version.data != 0) {
	version = (int)DER_GetUInteger (&crl->version);
    }
    return version;
}


/* check the entries in the CRL */
SECStatus cert_check_crl_entries (CERTCrl *crl)
{
    CERTCrlEntry **entries;
    CERTCrlEntry *entry;
    PRBool hasCriticalExten = PR_FALSE;
    SECStatus rv = SECSuccess;

    if (!crl) {
        return SECFailure;
    }

    if (crl->entries == NULL) {
        /* CRLs with no entries are valid */
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
                if (hasCriticalExten) {
                    if (cert_get_crl_version(crl) != SEC_CRL_VERSION_2) { 
                        /* only CRL v2 critical extensions are supported */
                        PORT_SetError(SEC_ERROR_CRL_V1_CRITICAL_EXTENSION);
                        rv = SECFailure;
                        break;
                    }
                }
            }

	    /* For each entry, make sure that it does not contain an unknown
	       critical extension.  If it does, we must reject the CRL since
	       we don't know how to process the extension.
	    */
	    if (cert_HasUnknownCriticalExten (entry->extensions) == PR_TRUE) {
		PORT_SetError (SEC_ERROR_CRL_UNKNOWN_CRITICAL_EXTENSION);
		rv = SECFailure;
		break;
	    }
	}
	++entries;
    }
    return(rv);
}

/* Check the version of the CRL.  If there is a critical extension in the crl
   or crl entry, then the version must be v2. Otherwise, it should be v1. If
   the crl contains critical extension(s), then we must recognized the
   extension's OID.
   */
SECStatus cert_check_crl_version (CERTCrl *crl)
{
    PRBool hasCriticalExten = PR_FALSE;
    int version = cert_get_crl_version(crl);
	
    if (version > SEC_CRL_VERSION_2) {
	PORT_SetError (SEC_ERROR_CRL_INVALID_VERSION);
	return (SECFailure);
    }

    /* Check the crl extensions for a critial extension.  If one is found,
       and the version is not v2, then we are done.
     */
    if (crl->extensions) {
	hasCriticalExten = cert_HasCriticalExtension (crl->extensions);
	if (hasCriticalExten) {
            if (version != SEC_CRL_VERSION_2) {
                /* only CRL v2 critical extensions are supported */
                PORT_SetError(SEC_ERROR_CRL_V1_CRITICAL_EXTENSION);
                return (SECFailure);
            }
	    /* make sure that there is no unknown critical extension */
	    if (cert_HasUnknownCriticalExten (crl->extensions) == PR_TRUE) {
		PORT_SetError (SEC_ERROR_CRL_UNKNOWN_CRITICAL_EXTENSION);
		return (SECFailure);
	    }
	}
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
    PRArenaPool* myArena;

    if (!arena) {
        /* arena needed for QuickDER */
        myArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    } else {
        myArena = arena;
    }
    PORT_Memset (&sd, 0, sizeof (sd));
    rv = SEC_QuickDERDecodeItem (myArena, &sd, CERT_SignedDataTemplate, derCrl);
    if (SECSuccess == rv) {
        PORT_Memset (&crlkey, 0, sizeof (crlkey));
        rv = SEC_QuickDERDecodeItem(myArena, &crlkey, cert_CrlKeyTemplate, &sd.data);
    }

    /* make a copy so the data doesn't point to memory inside derCrl, which
       may be temporary */
    if (SECSuccess == rv) {
        rv = SECITEM_CopyItem(arena, key, &crlkey.derName);
    }

    if (myArena != arena) {
        PORT_FreeArena(myArena, PR_FALSE);
    }

    return rv;
}

#define GetOpaqueCRLFields(x) ((OpaqueCRLFields*)x->opaque)

SECStatus CERT_CompleteCRLDecodeEntries(CERTSignedCrl* crl)
{
    SECStatus rv = SECSuccess;
    SECItem* crldata = NULL;
    OpaqueCRLFields* extended = NULL;

    if ( (!crl) ||
         (!(extended = (OpaqueCRLFields*) crl->opaque)) ||
         (PR_TRUE == extended->decodingError) ) {
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
            extended->decodingError = PR_TRUE;
            extended->badEntries = PR_TRUE;
            /* cache the decoding failure. If it fails the first time,
               it will fail again, which will grow the arena and leak
               memory, so we want to avoid it */
        }
        rv = cert_check_crl_entries(&crl->crl);
        if (rv != SECSuccess) {
            extended->badExtensions = PR_TRUE;
        }
    }
    return rv;
}

/*
 * take a DER CRL and decode it into a CRL structure
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
    const SEC_ASN1Template* crlTemplate = CERT_SignedCrlTemplate;
    PRInt32 testOptions = options;

    PORT_Assert(derSignedCrl);
    if (!derSignedCrl) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    /* Adopting DER requires not copying it.  Code that sets ADOPT flag 
     * but doesn't set DONT_COPY probably doesn't know What it is doing.  
     * That condition is a programming error in the caller.
     */
    testOptions &= (CRL_DECODE_ADOPT_HEAP_DER | CRL_DECODE_DONT_COPY_DER);
    PORT_Assert(testOptions != CRL_DECODE_ADOPT_HEAP_DER);
    if (testOptions == CRL_DECODE_ADOPT_HEAP_DER) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    /* make a new arena if needed */
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
    if (options & CRL_DECODE_ADOPT_HEAP_DER) {
        extended->heapDER = PR_TRUE;
    }
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
        /* check for critical extensions */
        rv =  cert_check_crl_version (&crl->crl);
        if (rv != SECSuccess) {
            extended->badExtensions = PR_TRUE;
            break;
        }

        if (PR_TRUE == extended->partial) {
            /* partial decoding, don't verify entries */
            break;
        }

        rv = cert_check_crl_entries(&crl->crl);
        if (rv != SECSuccess) {
            extended->badExtensions = PR_TRUE;
        }

        break;

    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
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
        if (extended) {
            extended->decodingError = PR_TRUE;
        }
        if (crl) {
            crl->referenceCount = 1;
            return(crl);
        }
    }

    if ((narena == NULL) && arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(0);
}

/*
 * take a DER CRL and decode it into a CRL structure
 */
CERTSignedCrl *
CERT_DecodeDERCrl(PRArenaPool *narena, SECItem *derSignedCrl, int type)
{
    return CERT_DecodeDERCrlWithFlags(narena, derSignedCrl, type,
                                      CRL_DECODE_DEFAULT_OPTIONS);
}

/*
 * Lookup a CRL in the databases. We mirror the same fast caching data base
 *  caching stuff used by certificates....?
 * return values :
 *
 * SECSuccess means we got a valid decodable DER CRL, or no CRL at all.
 * Caller may distinguish those cases by the value returned in "decoded".
 * When DER CRL is not found, error code will be SEC_ERROR_CRL_NOT_FOUND.
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

    PORT_Assert(decoded);
    if (!decoded) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    derCrl = PK11_FindCrlByName(&slot, &crlHandle, crlKey, type, &url);
    if (derCrl == NULL) {
	/* if we had a problem other than the CRL just didn't exist, return
	 * a failure to the upper level */
	int nsserror = PORT_GetError();
	if (nsserror != SEC_ERROR_CRL_NOT_FOUND) {
	    rv = SECFailure;
	}
	goto loser;
    }
    PORT_Assert(crlHandle != CK_INVALID_HANDLE);
    /* PK11_FindCrlByName obtained a slot reference. */
    
    /* derCRL is a fresh HEAP copy made for us by PK11_FindCrlByName.
       Force adoption of the DER CRL from the heap - this will cause it 
       to be automatically freed when SEC_DestroyCrl is invoked */
    decodeoptions |= (CRL_DECODE_ADOPT_HEAP_DER | CRL_DECODE_DONT_COPY_DER);

    crl = CERT_DecodeDERCrlWithFlags(NULL, derCrl, type, decodeoptions);
    if (crl) {
        crl->slot = slot;
        slot = NULL; /* adopt it */
	derCrl = NULL; /* adopted by the crl struct */
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
	SECITEM_FreeItem(derCrl, PR_TRUE);
    }

    *decoded = crl;

    return rv;
}


CERTSignedCrl *
crl_storeCRL (PK11SlotInfo *slot,char *url,
                  CERTSignedCrl *newCrl, SECItem *derCrl, int type)
{
    CERTSignedCrl *oldCrl = NULL, *crl = NULL;
    PRBool deleteOldCrl = PR_FALSE;
    CK_OBJECT_HANDLE crlHandle = CK_INVALID_HANDLE;
    SECStatus rv;

    PORT_Assert(newCrl);
    PORT_Assert(derCrl);
    PORT_Assert(type == SEC_CRL_TYPE);

    if (type != SEC_CRL_TYPE) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    /* we can't use the cache here because we must look in the same
       token */
    rv = SEC_FindCrlByKeyOnSlot(slot, &newCrl->crl.derName, type,
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
	    if (oldCrl->url && !url)
	        url = oldCrl->url;
	    if (url)
		crl->url = PORT_ArenaStrdup(crl->arena, url);
	    goto done;
	}
        if (!SEC_CrlIsNewer(&newCrl->crl,&oldCrl->crl)) {
            PORT_SetError(SEC_ERROR_OLD_CRL);
            goto done;
        }

        /* if we have a url in the database, use that one */
        if (oldCrl->url && !url) {
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
        PR_ATOMIC_INCREMENT(&acrl->referenceCount);
        return acrl;
    }
    return NULL;
}

SECStatus
SEC_DestroyCrl(CERTSignedCrl *crl)
{
    if (crl) {
	if (PR_ATOMIC_DECREMENT(&crl->referenceCount) < 1) {
	    if (crl->slot) {
		PK11_FreeSlot(crl->slot);
	    }
            if (GetOpaqueCRLFields(crl) &&
                PR_TRUE == GetOpaqueCRLFields(crl)->heapDER) {
                SECITEM_FreeItem(crl->derCrl, PR_TRUE);
            }
            if (crl->arena) {
                PORT_FreeArena(crl->arena, PR_FALSE);
            }
	}
        return SECSuccess;
    } else {
        return SECFailure;
    }
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
SEC_ASN1_CHOOSER_IMPLEMENT(CERT_SignedCrlTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(CERT_SetOfSignedCrlTemplate)

/* CRL cache code starts here */

/* constructor */
static SECStatus CachedCrl_Create(CachedCrl** returned, CERTSignedCrl* crl,
                           CRLOrigin origin);
/* destructor */
static SECStatus CachedCrl_Destroy(CachedCrl* crl);

/* create hash table of CRL entries */
static SECStatus CachedCrl_Populate(CachedCrl* crlobject);

/* empty the cache content */
static SECStatus CachedCrl_Depopulate(CachedCrl* crl);

/* are these CRLs the same, as far as the cache is concerned ?
   Or are they the same token object, but with different DER ? */

static SECStatus CachedCrl_Compare(CachedCrl* a, CachedCrl* b, PRBool* isDupe,
                                PRBool* isUpdated);

/* create a DPCache object */
static SECStatus DPCache_Create(CRLDPCache** returned, CERTCertificate* issuer,
                         const SECItem* subject, SECItem* dp);

/* destructor for CRL DPCache object */
static SECStatus DPCache_Destroy(CRLDPCache* cache);

/* add a new CRL object to the dynamic array of CRLs of the DPCache, and
   returns the cached CRL object . Needs write access to DPCache. */
static SECStatus DPCache_AddCRL(CRLDPCache* cache, CachedCrl* crl,
                                PRBool* added);

/* fetch the CRL for this DP from the PKCS#11 tokens */
static SECStatus DPCache_FetchFromTokens(CRLDPCache* cache, PRTime vfdate,
                                         void* wincx);

/* update the content of the CRL cache, including fetching of CRLs, and
   reprocessing with specified issuer and date */
static SECStatus DPCache_GetUpToDate(CRLDPCache* cache, CERTCertificate* issuer,
                         PRBool readlocked, PRTime vfdate, void* wincx);

/* returns true if there are CRLs from PKCS#11 slots */
static PRBool DPCache_HasTokenCRLs(CRLDPCache* cache);

/* remove CRL at offset specified */
static SECStatus DPCache_RemoveCRL(CRLDPCache* cache, PRUint32 offset);

/* Pick best CRL to use . needs write access */
static SECStatus DPCache_SelectCRL(CRLDPCache* cache);

/* create an issuer cache object (per CA subject ) */
static SECStatus IssuerCache_Create(CRLIssuerCache** returned,
                             CERTCertificate* issuer,
                             const SECItem* subject, const SECItem* dp);

/* destructor for CRL IssuerCache object */
SECStatus IssuerCache_Destroy(CRLIssuerCache* cache);

/* add a DPCache to the issuer cache */
static SECStatus IssuerCache_AddDP(CRLIssuerCache* cache,
                                   CERTCertificate* issuer,
                                   const SECItem* subject,
                                   const SECItem* dp, CRLDPCache** newdpc);

/* get a particular DPCache object from an IssuerCache */
static CRLDPCache* IssuerCache_GetDPCache(CRLIssuerCache* cache,
                                          const SECItem* dp);

/*
** Pre-allocator hash allocator ops.
*/

/* allocate memory for hash table */
static void * PR_CALLBACK
PreAllocTable(void *pool, PRSize size)
{
    PreAllocator* alloc = (PreAllocator*)pool;
    PORT_Assert(alloc);
    if (!alloc)
    {
        /* no allocator, or buffer full */
        return NULL;
    }
    if (size > (alloc->len - alloc->used))
    {
        /* initial buffer full, let's use the arena */
        alloc->extra += size;
        return PORT_ArenaAlloc(alloc->arena, size);
    }
    /* use the initial buffer */
    alloc->used += size;
    return (char*) alloc->data + alloc->used - size;
}

/* free hash table memory.
   Individual PreAllocator elements cannot be freed, so this is a no-op. */
static void PR_CALLBACK
PreFreeTable(void *pool, void *item)
{
}

/* allocate memory for hash table */
static PLHashEntry * PR_CALLBACK
PreAllocEntry(void *pool, const void *key)
{
    return PreAllocTable(pool, sizeof(PLHashEntry));
}

/* free hash table entry.
   Individual PreAllocator elements cannot be freed, so this is a no-op. */
static void PR_CALLBACK
PreFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
}

/* methods required for PL hash table functions */
static PLHashAllocOps preAllocOps =
{
    PreAllocTable, PreFreeTable,
    PreAllocEntry, PreFreeEntry
};

/* destructor for PreAllocator object */
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
}

/* constructor for PreAllocator object */
PreAllocator* PreAllocator_Create(PRSize size)
{
    PRArenaPool* arena = NULL;
    PreAllocator* prebuffer = NULL;
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
    {
        return NULL;
    }
    prebuffer = (PreAllocator*)PORT_ArenaZAlloc(arena,
                                                sizeof(PreAllocator));
    if (!prebuffer)
    {
        PORT_FreeArena(arena, PR_TRUE);
        return NULL;
    }
    prebuffer->arena = arena;

    if (size)
    {
        prebuffer->len = size;
        prebuffer->data = PORT_ArenaAlloc(arena, size);
        if (!prebuffer->data)
        {
            PORT_FreeArena(arena, PR_TRUE);
            return NULL;
        }
    }
    return prebuffer;
}

/* global Named CRL cache object */
static NamedCRLCache namedCRLCache = { NULL, NULL };

/* global CRL cache object */
static CRLCache crlcache = { NULL, NULL };

/* initial state is off */
static PRBool crlcache_initialized = PR_FALSE;

PRTime CRLCache_Empty_TokenFetch_Interval = 60 * 1000000; /* how often
    to query the tokens for CRL objects, in order to discover new objects, if
    the cache does not contain any token CRLs . In microseconds */

PRTime CRLCache_TokenRefetch_Interval = 600 * 1000000 ; /* how often
    to query the tokens for CRL objects, in order to discover new objects, if
    the cache already contains token CRLs In microseconds */

PRTime CRLCache_ExistenceCheck_Interval = 60 * 1000000; /* how often to check
    if a token CRL object still exists. In microseconds */

/* this function is called at NSS initialization time */
SECStatus InitCRLCache(void)
{
    if (PR_FALSE == crlcache_initialized)
    {
        PORT_Assert(NULL == crlcache.lock);
        PORT_Assert(NULL == crlcache.issuers);
        PORT_Assert(NULL == namedCRLCache.lock);
        PORT_Assert(NULL == namedCRLCache.entries);
        if (crlcache.lock || crlcache.issuers || namedCRLCache.lock ||
            namedCRLCache.entries)
        {
            /* CRL cache already partially initialized */
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
#ifdef GLOBAL_RWLOCK
        crlcache.lock = NSSRWLock_New(NSS_RWLOCK_RANK_NONE, NULL);
#else
        crlcache.lock = PR_NewLock();
#endif
        namedCRLCache.lock = PR_NewLock();
        crlcache.issuers = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
                                  PL_CompareValues, NULL, NULL);
        namedCRLCache.entries = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
                                  PL_CompareValues, NULL, NULL);
        if (!crlcache.lock || !namedCRLCache.lock || !crlcache.issuers ||
            !namedCRLCache.entries)
        {
            if (crlcache.lock)
            {
#ifdef GLOBAL_RWLOCK
                NSSRWLock_Destroy(crlcache.lock);
#else
                PR_DestroyLock(crlcache.lock);
#endif
                crlcache.lock = NULL;
            }
            if (namedCRLCache.lock)
            {
                PR_DestroyLock(namedCRLCache.lock);
                namedCRLCache.lock = NULL;
            }
            if (crlcache.issuers)
            {
                PL_HashTableDestroy(crlcache.issuers);
                crlcache.issuers = NULL;
            }
            if (namedCRLCache.entries)
            {
                PL_HashTableDestroy(namedCRLCache.entries);
                namedCRLCache.entries = NULL;
            }

            return SECFailure;
        }
        crlcache_initialized = PR_TRUE;
        return SECSuccess;
    }
    else
    {
        PORT_Assert(crlcache.lock);
        PORT_Assert(crlcache.issuers);
        if ( (NULL == crlcache.lock) || (NULL == crlcache.issuers) )
        {
            /* CRL cache not fully initialized */
            return SECFailure;
        }
        else
        {
            /* CRL cache already initialized */
            return SECSuccess;
        }
    }
}

/* destructor for CRL DPCache object */
static SECStatus DPCache_Destroy(CRLDPCache* cache)
{
    PRUint32 i = 0;
    PORT_Assert(cache);
    if (!cache)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (cache->lock)
    {
#ifdef DPC_RWLOCK
        NSSRWLock_Destroy(cache->lock);
#else
        PR_DestroyLock(cache->lock);
#endif
    }
    else
    {
        PORT_Assert(0);
        return SECFailure;
    }
    /* destroy all our CRL objects */
    for (i=0;i<cache->ncrls;i++)
    {
        if (!cache->crls || !cache->crls[i] ||
            SECSuccess != CachedCrl_Destroy(cache->crls[i]))
        {
            return SECFailure;
        }
    }
    /* free the array of CRLs */
    if (cache->crls)
    {
	PORT_Free(cache->crls);
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
    PORT_Free(cache);
    return SECSuccess;
}

/* destructor for CRL IssuerCache object */
SECStatus IssuerCache_Destroy(CRLIssuerCache* cache)
{
    PORT_Assert(cache);
    if (!cache)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
#ifdef XCRL
    if (cache->lock)
    {
        NSSRWLock_Destroy(cache->lock);
    }
    else
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
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
    if (SECSuccess != DPCache_Destroy(cache->dpp))
    {
        PORT_Assert(0);
        return SECFailure;
    }
    PORT_Free(cache);
    return SECSuccess;
}

/* create a named CRL entry object */
static SECStatus NamedCRLCacheEntry_Create(NamedCRLCacheEntry** returned)
{
    NamedCRLCacheEntry* entry = NULL;
    if (!returned)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    *returned = NULL;
    entry = (NamedCRLCacheEntry*) PORT_ZAlloc(sizeof(NamedCRLCacheEntry));
    if (!entry)
    {
        return SECFailure;
    }
    *returned = entry;
    return SECSuccess;
}

/* destroy a named CRL entry object */
static SECStatus NamedCRLCacheEntry_Destroy(NamedCRLCacheEntry* entry)
{
    if (!entry)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (entry->crl)
    {
        /* named CRL cache owns DER memory */
        SECITEM_ZfreeItem(entry->crl, PR_TRUE);
    }
    if (entry->canonicalizedName)
    {
        SECITEM_FreeItem(entry->canonicalizedName, PR_TRUE);
    }
    PORT_Free(entry);
    return SECSuccess;
}

/* callback function used in hash table destructor */
static PRIntn PR_CALLBACK FreeIssuer(PLHashEntry *he, PRIntn i, void *arg)
{
    CRLIssuerCache* issuer = NULL;
    SECStatus* rv = (SECStatus*) arg;

    PORT_Assert(he);
    if (!he)
    {
        return HT_ENUMERATE_NEXT;
    }
    issuer = (CRLIssuerCache*) he->value;
    PORT_Assert(issuer);
    if (issuer)
    {
        if (SECSuccess != IssuerCache_Destroy(issuer))
        {
            PORT_Assert(rv);
            if (rv)
            {
                *rv = SECFailure;
            }
            return HT_ENUMERATE_NEXT;
        }
    }
    return HT_ENUMERATE_NEXT;
}

/* callback function used in hash table destructor */
static PRIntn PR_CALLBACK FreeNamedEntries(PLHashEntry *he, PRIntn i, void *arg)
{
    NamedCRLCacheEntry* entry = NULL;
    SECStatus* rv = (SECStatus*) arg;

    PORT_Assert(he);
    if (!he)
    {
        return HT_ENUMERATE_NEXT;
    }
    entry = (NamedCRLCacheEntry*) he->value;
    PORT_Assert(entry);
    if (entry)
    {
        if (SECSuccess != NamedCRLCacheEntry_Destroy(entry))
        {
            PORT_Assert(rv);
            if (rv)
            {
                *rv = SECFailure;
            }
            return HT_ENUMERATE_NEXT;
        }
    }
    return HT_ENUMERATE_NEXT;
}

/* needs to be called at NSS shutdown time
   This will destroy the global CRL cache, including 
   - the hash table of issuer cache objects
   - the issuer cache objects
   - DPCache objects in issuer cache objects */
SECStatus ShutdownCRLCache(void)
{
    SECStatus rv = SECSuccess;
    if (PR_FALSE == crlcache_initialized &&
        !crlcache.lock && !crlcache.issuers)
    {
        /* CRL cache has already been shut down */
        return SECSuccess;
    }
    if (PR_TRUE == crlcache_initialized &&
        (!crlcache.lock || !crlcache.issuers || !namedCRLCache.lock ||
         !namedCRLCache.entries))
    {
        /* CRL cache has partially been shut down */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    /* empty the CRL cache */
    /* free the issuers */
    PL_HashTableEnumerateEntries(crlcache.issuers, &FreeIssuer, &rv);
    /* free the hash table of issuers */
    PL_HashTableDestroy(crlcache.issuers);
    crlcache.issuers = NULL;
    /* free the global lock */
#ifdef GLOBAL_RWLOCK
    NSSRWLock_Destroy(crlcache.lock);
#else
    PR_DestroyLock(crlcache.lock);
#endif
    crlcache.lock = NULL;

    /* empty the named CRL cache. This must be done after freeing the CRL
     * cache, since some CRLs in this cache are in the memory for the other  */
    /* free the entries */
    PL_HashTableEnumerateEntries(namedCRLCache.entries, &FreeNamedEntries, &rv);
    /* free the hash table of issuers */
    PL_HashTableDestroy(namedCRLCache.entries);
    namedCRLCache.entries = NULL;
    /* free the global lock */
    PR_DestroyLock(namedCRLCache.lock);
    namedCRLCache.lock = NULL;

    crlcache_initialized = PR_FALSE;
    return rv;
}

/* add a new CRL object to the dynamic array of CRLs of the DPCache, and
   returns the cached CRL object . Needs write access to DPCache. */
static SECStatus DPCache_AddCRL(CRLDPCache* cache, CachedCrl* newcrl,
                                PRBool* added)
{
    CachedCrl** newcrls = NULL;
    PRUint32 i = 0;
    PORT_Assert(cache);
    PORT_Assert(newcrl);
    PORT_Assert(added);
    if (!cache || !newcrl || !added)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    *added = PR_FALSE;
    /* before adding a new CRL, check if it is a duplicate */
    for (i=0;i<cache->ncrls;i++)
    {
        CachedCrl* existing = NULL;
        SECStatus rv = SECSuccess;
        PRBool dupe = PR_FALSE, updated = PR_FALSE;
        if (!cache->crls)
        {
            PORT_Assert(0);
            return SECFailure;
        }
        existing = cache->crls[i];
        if (!existing)
        {
            PORT_Assert(0);
            return SECFailure;
        }
        rv = CachedCrl_Compare(existing, newcrl, &dupe, &updated);
        if (SECSuccess != rv)
        {
            PORT_Assert(0);
            PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
            return SECFailure;
        }
        if (PR_TRUE == dupe)
        {
            /* dupe */
            PORT_SetError(SEC_ERROR_CRL_ALREADY_EXISTS);
            return SECSuccess;
        }
        if (PR_TRUE == updated)
        {
            /* this token CRL is in the same slot and has the same object ID,
               but different content. We need to remove the old object */
            if (SECSuccess != DPCache_RemoveCRL(cache, i))
            {
                PORT_Assert(0);
                PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                return PR_FALSE;
            }
        }
    }

    newcrls = (CachedCrl**)PORT_Realloc(cache->crls,
        (cache->ncrls+1)*sizeof(CachedCrl*));
    if (!newcrls)
    {
        return SECFailure;
    }
    cache->crls = newcrls;
    cache->ncrls++;
    cache->crls[cache->ncrls-1] = newcrl;
    *added = PR_TRUE;
    return SECSuccess;
}

/* remove CRL at offset specified */
static SECStatus DPCache_RemoveCRL(CRLDPCache* cache, PRUint32 offset)
{
    CachedCrl* acrl = NULL;
    PORT_Assert(cache);
    if (!cache || (!cache->crls) || (!(offset<cache->ncrls)) )
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    acrl = cache->crls[offset];
    PORT_Assert(acrl);
    if (!acrl)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    cache->crls[offset] = cache->crls[cache->ncrls-1];
    cache->crls[cache->ncrls-1] = NULL;
    cache->ncrls--;
    if (cache->selected == acrl) {
        cache->selected = NULL;
    }
    if (SECSuccess != CachedCrl_Destroy(acrl))
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    return SECSuccess;
}

/* check whether a CRL object stored in a PKCS#11 token still exists in
   that token . This has to be efficient (the entire CRL value cannot be
   transferred accross the token boundaries), so this is accomplished by
   simply fetching the subject attribute and making sure it hasn't changed .
   Note that technically, the CRL object could have been replaced with a new
   PKCS#11 object of the same ID and subject (which actually happens in
   softoken), but this function has no way of knowing that the object
   value changed, since CKA_VALUE isn't checked. */
static PRBool TokenCRLStillExists(CERTSignedCrl* crl)
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
    if (!crl)
    {
        return PR_FALSE;
    }
    slot = crl->slot;
    PORT_Assert(crl->slot);
    if (!slot)
    {
        return PR_FALSE;
    }
    oldSubject = &crl->crl.derName;
    PORT_Assert(oldSubject);
    if (!oldSubject)
    {
        return PR_FALSE;
    }

    /* query subject and type attributes in order to determine if the
       object has been deleted */

    /* first, make an nssCryptokiObject */
    instance.handle = crl->pkcs11ID;
    PORT_Assert(instance.handle);
    if (!instance.handle)
    {
        return PR_FALSE;
    }
    instance.token = PK11Slot_GetNSSToken(slot);
    PORT_Assert(instance.token);
    if (!instance.token)
    {
        return PR_FALSE;
    }
    instance.isTokenObject = PR_TRUE;
    instance.label = NULL;

    arena = NSSArena_Create();
    PORT_Assert(arena);
    if (!arena)
    {
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
    if (PR_SUCCESS == status)
    {
        subject.data = newsubject.data;
        subject.len = newsubject.size;
        if (SECITEM_CompareItem(oldSubject, &subject) != SECEqual)
        {
            xstatus = PR_FALSE;
        }
        if (CKO_NETSCAPE_CRL != crl_class)
        {
            xstatus = PR_FALSE;
        }
    }
    else
    {
        xstatus = PR_FALSE;
    }
    NSSArena_Destroy(arena);
    return xstatus;
}

/* verify the signature of a CRL against its issuer at a given date */
static SECStatus CERT_VerifyCRL(
    CERTSignedCrl* crlobject,
    CERTCertificate* issuer,
    PRTime vfdate,
    void* wincx)
{
    return CERT_VerifySignedData(&crlobject->signatureWrap,
                                 issuer, vfdate, wincx);
}

/* verify a CRL and update cache state */
static SECStatus CachedCrl_Verify(CRLDPCache* cache, CachedCrl* crlobject,
                          PRTime vfdate, void* wincx)
{
    /*  Check if it is an invalid CRL
        if we got a bad CRL, we want to cache it in order to avoid
        subsequent fetches of this same identical bad CRL. We set
        the cache to the invalid state to ensure that all certs on this
        DP are considered to have unknown status from now on. The cache
        object will remain in this state until the bad CRL object
        is removed from the token it was fetched from. If the cause
        of the failure is that we didn't have the issuer cert to
        verify the signature, this state can be cleared when
        the issuer certificate becomes available if that causes the
        signature to verify */

    if (!cache || !crlobject)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (PR_TRUE == GetOpaqueCRLFields(crlobject->crl)->decodingError)
    {
        crlobject->sigChecked = PR_TRUE; /* we can never verify a CRL
            with bogus DER. Mark it checked so we won't try again */
        PORT_SetError(SEC_ERROR_BAD_DER);
        return SECSuccess;
    }
    else
    {
        SECStatus signstatus = SECFailure;
        if (cache->issuer)
        {
            signstatus = CERT_VerifyCRL(crlobject->crl, cache->issuer, vfdate,
                                        wincx);
        }
        if (SECSuccess != signstatus)
        {
            if (!cache->issuer)
            {
                /* we tried to verify without an issuer cert . This is
                   because this CRL came through a call to SEC_FindCrlByName.
                   So, we don't cache this verification failure. We'll try
                   to verify the CRL again when a certificate from that issuer
                   becomes available */
            } else
            {
                crlobject->sigChecked = PR_TRUE;
            }
            PORT_SetError(SEC_ERROR_CRL_BAD_SIGNATURE);
            return SECSuccess;
        } else
        {
            crlobject->sigChecked = PR_TRUE;
            crlobject->sigValid = PR_TRUE;
        }
    }
    
    return SECSuccess;
}

/* fetch the CRLs for this DP from the PKCS#11 tokens */
static SECStatus DPCache_FetchFromTokens(CRLDPCache* cache, PRTime vfdate,
                                         void* wincx)
{
    SECStatus rv = SECSuccess;
    CERTCrlHeadNode head;
    if (!cache)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    /* first, initialize list */
    memset(&head, 0, sizeof(head));
    head.arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    rv = pk11_RetrieveCrls(&head, cache->subject, wincx);

    /* if this function fails, something very wrong happened, such as an out
       of memory error during CRL decoding. We don't want to proceed and must
       mark the cache object invalid */
    if (SECFailure == rv)
    {
        /* fetch failed, add error bit */
        cache->invalid |= CRL_CACHE_LAST_FETCH_FAILED;
    } else
    {
        /* fetch was successful, clear this error bit */
        cache->invalid &= (~CRL_CACHE_LAST_FETCH_FAILED);
    }

    /* add any CRLs found to our array */
    if (SECSuccess == rv)
    {
        CERTCrlNode* crlNode = NULL;

        for (crlNode = head.first; crlNode ; crlNode = crlNode->next)
        {
            CachedCrl* returned = NULL;
            CERTSignedCrl* crlobject = crlNode->crl;
            if (!crlobject)
            {
                PORT_Assert(0);
                continue;
            }
            rv = CachedCrl_Create(&returned, crlobject, CRL_OriginToken);
            if (SECSuccess == rv)
            {
                PRBool added = PR_FALSE;
                rv = DPCache_AddCRL(cache, returned, &added);
                if (PR_TRUE != added)
                {
                    rv = CachedCrl_Destroy(returned);
                    returned = NULL;
                }
                else if (vfdate)
                {
                    rv = CachedCrl_Verify(cache, returned, vfdate, wincx);
                }
            }
            else
            {
                /* not enough memory to add the CRL to the cache. mark it
                   invalid so we will try again . */
                cache->invalid |= CRL_CACHE_LAST_FETCH_FAILED;
            }
            if (SECFailure == rv)
            {
                break;
            }
        }
    }

    if (head.arena)
    {
        CERTCrlNode* crlNode = NULL;
        /* clean up the CRL list in case we got a partial one
           during a failed fetch */
        for (crlNode = head.first; crlNode ; crlNode = crlNode->next)
        {
            if (crlNode->crl)
            {
                SEC_DestroyCrl(crlNode->crl); /* free the CRL. Either it got
                   added to the cache and the refcount got bumped, or not, and
                   thus we need to free its RAM */
            }
        }
        PORT_FreeArena(head.arena, PR_FALSE); /* destroy CRL list */
    }

    return rv;
}

static SECStatus CachedCrl_GetEntry(CachedCrl* crl, const SECItem* sn,
                                    CERTCrlEntry** returned)
{
    CERTCrlEntry* acrlEntry;
     
    PORT_Assert(crl);
    PORT_Assert(crl->entries);
    PORT_Assert(sn);
    PORT_Assert(returned);
    if (!crl || !sn || !returned || !crl->entries)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    acrlEntry = PL_HashTableLookup(crl->entries, (void*)sn);
    if (acrlEntry)
    {
        *returned = acrlEntry;
    }
    else
    {
        *returned = NULL;
    }
    return SECSuccess;
}

/* check if a particular SN is in the CRL cache and return its entry */
dpcacheStatus DPCache_Lookup(CRLDPCache* cache, const SECItem* sn,
                         CERTCrlEntry** returned)
{
    SECStatus rv;
    if (!cache || !sn || !returned)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        /* no cache or SN to look up, or no way to return entry */
        return dpcacheCallerError;
    }
    *returned = NULL;
    if (0 != cache->invalid)
    {
        /* the cache contains a bad CRL, or there was a CRL fetching error. */
        PORT_SetError(SEC_ERROR_CRL_INVALID);
        return dpcacheInvalidCacheError;
    }
    if (!cache->selected)
    {
        /* no CRL means no entry to return. This is OK, except for
         * NIST policy */
        return dpcacheEmpty;
    }
    rv = CachedCrl_GetEntry(cache->selected, sn, returned);
    if (SECSuccess != rv)
    {
        return dpcacheLookupError;
    }
    else
    {
        if (*returned)
        {
            return dpcacheFoundEntry;
        }
        else
        {
            return dpcacheNoEntry;
        }
    }
}

#if defined(DPC_RWLOCK)

#define DPCache_LockWrite() \
{ \
    if (readlocked) \
    { \
        NSSRWLock_UnlockRead(cache->lock); \
    } \
    NSSRWLock_LockWrite(cache->lock); \
}

#define DPCache_UnlockWrite() \
{ \
    if (readlocked) \
    { \
        NSSRWLock_LockRead(cache->lock); \
    } \
    NSSRWLock_UnlockWrite(cache->lock); \
}

#else

/* with a global lock, we are always locked for read before we need write
   access, so do nothing */

#define DPCache_LockWrite() \
{ \
}

#define DPCache_UnlockWrite() \
{ \
}

#endif

/* update the content of the CRL cache, including fetching of CRLs, and
   reprocessing with specified issuer and date . We are always holding
   either the read or write lock on DPCache upon entry. */
static SECStatus DPCache_GetUpToDate(CRLDPCache* cache, CERTCertificate*
                                     issuer, PRBool readlocked, PRTime vfdate,
                                     void* wincx)
{
    /* Update the CRLDPCache now. We don't cache token CRL lookup misses
       yet, as we have no way of getting notified of new PKCS#11 object
       creation that happens in a token  */
    SECStatus rv = SECSuccess;
    PRUint32 i = 0;
    PRBool forcedrefresh = PR_FALSE;
    PRBool dirty = PR_FALSE; /* whether something was changed in the
                                cache state during this update cycle */
    PRBool hastokenCRLs = PR_FALSE;
    PRTime now = 0;
    PRTime lastfetch = 0;
    PRBool mustunlock = PR_FALSE;

    if (!cache)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* first, make sure we have obtained all the CRLs we need.
       We do an expensive token fetch in the following cases :
       1) cache is empty because no fetch was ever performed yet
       2) cache is explicitly set to refresh state
       3) cache is in invalid state because last fetch failed
       4) cache contains no token CRLs, and it's been more than one minute
          since the last fetch
       5) cache contains token CRLs, and it's been more than 10 minutes since
          the last fetch
    */
    forcedrefresh = cache->refresh;
    lastfetch = cache->lastfetch;
    if (PR_TRUE != forcedrefresh && 
        (!(cache->invalid & CRL_CACHE_LAST_FETCH_FAILED)))
    {
        now = PR_Now();
        hastokenCRLs = DPCache_HasTokenCRLs(cache);
    }
    if ( (0 == lastfetch) ||

         (PR_TRUE == forcedrefresh) ||

         (cache->invalid & CRL_CACHE_LAST_FETCH_FAILED) ||

         ( (PR_FALSE == hastokenCRLs) &&
           ( (now - cache->lastfetch > CRLCache_Empty_TokenFetch_Interval) ||
             (now < cache->lastfetch)) ) ||

         ( (PR_TRUE == hastokenCRLs) &&
           ((now - cache->lastfetch > CRLCache_TokenRefetch_Interval) ||
            (now < cache->lastfetch)) ) )
    {
        /* the cache needs to be refreshed, and/or we had zero CRL for this
           DP. Try to get one from PKCS#11 tokens */
        DPCache_LockWrite();
        /* check if another thread updated before us, and skip update if so */
        if (lastfetch == cache->lastfetch)
        {
            /* we are the first */
            rv = DPCache_FetchFromTokens(cache, vfdate, wincx);
            if (PR_TRUE == cache->refresh)
            {
                cache->refresh = PR_FALSE; /* clear refresh state */
            }
            dirty = PR_TRUE;
            cache->lastfetch = PR_Now();
        }
        DPCache_UnlockWrite();
    }

    /* now, make sure we have no extraneous CRLs (deleted token objects)
       we'll do this inexpensive existence check either
       1) if there was a token object fetch
       2) every minute */
    if (( PR_TRUE != dirty) && (!now) )
    {
        now = PR_Now();
    }
    if ( (PR_TRUE == dirty) ||
         ( (now - cache->lastcheck > CRLCache_ExistenceCheck_Interval) ||
           (now < cache->lastcheck)) )
    {
        PRTime lastcheck = cache->lastcheck;
        mustunlock = PR_FALSE;
        /* check if all CRLs still exist */
        for (i = 0; (i < cache->ncrls) ; i++)
        {
            CachedCrl* savcrl = cache->crls[i];
            if ( (!savcrl) || (savcrl && CRL_OriginToken != savcrl->origin))
            {
                /* we only want to check token CRLs */
                continue;
            }
            if ((PR_TRUE != TokenCRLStillExists(savcrl->crl)))
            {
                
                /* this CRL is gone */
                if (PR_TRUE != mustunlock)
                {
                    DPCache_LockWrite();
                    mustunlock = PR_TRUE;
                }
                /* first, we need to check if another thread did an update
                   before we did */
                if (lastcheck == cache->lastcheck)
                {
                    /* the CRL is gone. And we are the one to do the update */
                    DPCache_RemoveCRL(cache, i);
                    dirty = PR_TRUE;
                }
                /* stay locked here intentionally so we do all the other
                   updates in this thread for the remaining CRLs */
            }
        }
        if (PR_TRUE == mustunlock)
        {
            cache->lastcheck = PR_Now();
            DPCache_UnlockWrite();
            mustunlock = PR_FALSE;
        }
    }

    /* add issuer certificate if it was previously unavailable */
    if (issuer && (NULL == cache->issuer) &&
        (SECSuccess == CERT_CheckCertUsage(issuer, KU_CRL_SIGN)))
    {
        /* if we didn't have a valid issuer cert yet, but we do now. add it */
        DPCache_LockWrite();
        if (!cache->issuer)
        {
            dirty = PR_TRUE;
            cache->issuer = CERT_DupCertificate(issuer);    
        }
        DPCache_UnlockWrite();
    }

    /* verify CRLs that couldn't be checked when inserted into the cache
       because the issuer cert or a verification date was unavailable.
       These are CRLs that were inserted into the cache through
       SEC_FindCrlByName, or through manual insertion, rather than through a
       certificate verification (CERT_CheckCRL) */

    if (cache->issuer && vfdate )
    {
	mustunlock = PR_FALSE;
        /* re-process all unverified CRLs */
        for (i = 0; i < cache->ncrls ; i++)
        {
            CachedCrl* savcrl = cache->crls[i];
            if (!savcrl)
            {
                continue;
            }
            if (PR_TRUE != savcrl->sigChecked)
            {
                if (!mustunlock)
                {
                    DPCache_LockWrite();
                    mustunlock = PR_TRUE;
                }
                /* first, we need to check if another thread updated
                   it before we did, and abort if it has been modified since
                   we acquired the lock. Make sure first that the CRL is still
                   in the array at the same position */
                if ( (i<cache->ncrls) && (savcrl == cache->crls[i]) &&
                     (PR_TRUE != savcrl->sigChecked) )
                {
                    /* the CRL is still there, unverified. Do it */
                    CachedCrl_Verify(cache, savcrl, vfdate, wincx);
                    dirty = PR_TRUE;
                }
                /* stay locked here intentionally so we do all the other
                   updates in this thread for the remaining CRLs */
            }
            if (mustunlock && !dirty)
            {
                DPCache_UnlockWrite();
                mustunlock = PR_FALSE;
            }
        }
    }

    if (dirty || cache->mustchoose)
    {
        /* changes to the content of the CRL cache necessitate examining all
           CRLs for selection of the most appropriate one to cache */
	if (!mustunlock)
	{
	    DPCache_LockWrite();
	    mustunlock = PR_TRUE;
	}
        DPCache_SelectCRL(cache);
        cache->mustchoose = PR_FALSE;
    }
    if (mustunlock)
	DPCache_UnlockWrite();

    return rv;
}

/* callback for qsort to sort by thisUpdate */
static int SortCRLsByThisUpdate(const void* arg1, const void* arg2)
{
    PRTime timea, timeb;
    SECStatus rv = SECSuccess;
    CachedCrl* a, *b;

    a = *(CachedCrl**) arg1;
    b = *(CachedCrl**) arg2;

    if (!a || !b)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        rv = SECFailure;
    }

    if (SECSuccess == rv)
    {
        rv = DER_DecodeTimeChoice(&timea, &a->crl->crl.lastUpdate);
    }                       
    if (SECSuccess == rv)
    {
        rv = DER_DecodeTimeChoice(&timeb, &b->crl->crl.lastUpdate);
    }
    if (SECSuccess == rv)
    {
        if (timea > timeb)
        {
            return 1; /* a is better than b */
        }
        if (timea < timeb )
        {
            return -1; /* a is not as good as b */
        }
    }

    /* if they are equal, or if all else fails, use pointer differences */
    PORT_Assert(a != b); /* they should never be equal */
    return a>b?1:-1;
}

/* callback for qsort to sort a set of disparate CRLs, some of which are
   invalid DER or failed signature check.
   
   Validated CRLs are differentiated by thisUpdate .
   Validated CRLs are preferred over non-validated CRLs .
   Proper DER CRLs are preferred over non-DER data .
*/
static int SortImperfectCRLs(const void* arg1, const void* arg2)
{
    CachedCrl* a, *b;

    a = *(CachedCrl**) arg1;
    b = *(CachedCrl**) arg2;

    if (!a || !b)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        PORT_Assert(0);
    }
    else
    {
        PRBool aDecoded = PR_FALSE, bDecoded = PR_FALSE;
        if ( (PR_TRUE == a->sigValid) && (PR_TRUE == b->sigValid) )
        {
            /* both CRLs have been validated, choose the latest one */
            return SortCRLsByThisUpdate(arg1, arg2);
        }
        if (PR_TRUE == a->sigValid)
        {
            return 1; /* a is greater than b */
        }
        if (PR_TRUE == b->sigValid)
        {
            return -1; /* a is not as good as b */
        }
        aDecoded = GetOpaqueCRLFields(a->crl)->decodingError;
        bDecoded = GetOpaqueCRLFields(b->crl)->decodingError;
        /* neither CRL had its signature check pass */
        if ( (PR_FALSE == aDecoded) && (PR_FALSE == bDecoded) )
        {
            /* both CRLs are proper DER, choose the latest one */
            return SortCRLsByThisUpdate(arg1, arg2);
        }
        if (PR_FALSE == aDecoded)
        {
            return 1; /* a is better than b */
        }
        if (PR_FALSE == bDecoded)
        {
            return -1; /* a is not as good as b */
        }
        /* both are invalid DER. sigh. */
    }
    /* if they are equal, or if all else fails, use pointer differences */
    PORT_Assert(a != b); /* they should never be equal */
    return a>b?1:-1;
}


/* Pick best CRL to use . needs write access */
static SECStatus DPCache_SelectCRL(CRLDPCache* cache)
{
    PRUint32 i;
    PRBool valid = PR_TRUE;
    CachedCrl* selected = NULL;

    PORT_Assert(cache);
    if (!cache)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    /* if any invalid CRL is present, then the CRL cache is
       considered invalid, for security reasons */
    for (i = 0 ; i<cache->ncrls; i++)
    {
        if (!cache->crls[i] || !cache->crls[i]->sigChecked ||
            !cache->crls[i]->sigValid)
        {
            valid = PR_FALSE;
            break;
        }
    }
    if (PR_TRUE == valid)
    {
        /* all CRLs are valid, clear this error */
        cache->invalid &= (~CRL_CACHE_INVALID_CRLS);
    } else
    {
        /* some CRLs are invalid, set this error */
        cache->invalid |= CRL_CACHE_INVALID_CRLS;
    }

    if (cache->invalid)
    {
        /* cache is in an invalid state, so reset it */
        if (cache->selected)
        {
            cache->selected = NULL;
        }
        /* also sort the CRLs imperfectly */
        qsort(cache->crls, cache->ncrls, sizeof(CachedCrl*),
              SortImperfectCRLs);
        return SECSuccess;
    }
    /* all CRLs are good, sort them by thisUpdate */
    qsort(cache->crls, cache->ncrls, sizeof(CachedCrl*),
          SortCRLsByThisUpdate);

    if (cache->ncrls)
    {
        /* pick the newest CRL */
        selected = cache->crls[cache->ncrls-1];
    
        /* and populate the cache */
        if (SECSuccess != CachedCrl_Populate(selected))
        {
            return SECFailure;
        }
    }

    cache->selected = selected;

    return SECSuccess;
}

/* initialize a DPCache object */
static SECStatus DPCache_Create(CRLDPCache** returned, CERTCertificate* issuer,
                         const SECItem* subject, SECItem* dp)
{
    CRLDPCache* cache = NULL;
    PORT_Assert(returned);
    /* issuer and dp are allowed to be NULL */
    if (!returned || !subject)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    *returned = NULL;
    cache = PORT_ZAlloc(sizeof(CRLDPCache));
    if (!cache)
    {
        return SECFailure;
    }
#ifdef DPC_RWLOCK
    cache->lock = NSSRWLock_New(NSS_RWLOCK_RANK_NONE, NULL);
#else
    cache->lock = PR_NewLock();
#endif
    if (!cache->lock)
    {
	PORT_Free(cache);
        return SECFailure;
    }
    if (issuer)
    {
        cache->issuer = CERT_DupCertificate(issuer);
    }
    cache->distributionPoint = SECITEM_DupItem(dp);
    cache->subject = SECITEM_DupItem(subject);
    cache->lastfetch = 0;
    cache->lastcheck = 0;
    *returned = cache;
    return SECSuccess;
}

/* create an issuer cache object (per CA subject ) */
static SECStatus IssuerCache_Create(CRLIssuerCache** returned,
                             CERTCertificate* issuer,
                             const SECItem* subject, const SECItem* dp)
{
    SECStatus rv = SECSuccess;
    CRLIssuerCache* cache = NULL;
    PORT_Assert(returned);
    PORT_Assert(subject);
    /* issuer and dp are allowed to be NULL */
    if (!returned || !subject)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    *returned = NULL;
    cache = (CRLIssuerCache*) PORT_ZAlloc(sizeof(CRLIssuerCache));
    if (!cache)
    {
        return SECFailure;
    }
    cache->subject = SECITEM_DupItem(subject);
#ifdef XCRL
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
        PORT_Assert(SECSuccess == IssuerCache_Destroy(cache));
        return SECFailure;
    }
    *returned = cache;
    return SECSuccess;
}

/* add a DPCache to the issuer cache */
static SECStatus IssuerCache_AddDP(CRLIssuerCache* cache,
                                   CERTCertificate* issuer,
                                   const SECItem* subject,
                                   const SECItem* dp,
                                   CRLDPCache** newdpc)
{
    /* now create the required DP cache object */
    if (!cache || !subject || !newdpc)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (!dp)
    {
        /* default distribution point */
        SECStatus rv = DPCache_Create(&cache->dpp, issuer, subject, NULL);
        if (SECSuccess == rv)
        {
            *newdpc = cache->dpp;
            return SECSuccess;
        }
    }
    else
    {
        /* we should never hit this until we support multiple DPs */
        PORT_Assert(dp);
        /* XCRL allocate a new distribution point cache object, initialize it,
           and add it to the hash table of DPs */
    }
    return SECFailure;
}

/* add an IssuerCache to the global hash table of issuers */
static SECStatus CRLCache_AddIssuer(CRLIssuerCache* issuer)
{    
    PORT_Assert(issuer);
    PORT_Assert(crlcache.issuers);
    if (!issuer || !crlcache.issuers)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (NULL == PL_HashTableAdd(crlcache.issuers, (void*) issuer->subject,
                                (void*) issuer))
    {
        return SECFailure;
    }
    return SECSuccess;
}

/* retrieve the issuer cache object for a given issuer subject */
static SECStatus CRLCache_GetIssuerCache(CRLCache* cache,
                                         const SECItem* subject,
                                         CRLIssuerCache** returned)
{
    /* we need to look up the issuer in the hash table */
    SECStatus rv = SECSuccess;
    PORT_Assert(cache);
    PORT_Assert(subject);
    PORT_Assert(returned);
    PORT_Assert(crlcache.issuers);
    if (!cache || !subject || !returned || !crlcache.issuers)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        rv = SECFailure;
    }

    if (SECSuccess == rv)
    {
        *returned = (CRLIssuerCache*) PL_HashTableLookup(crlcache.issuers,
                                                         (void*) subject);
    }

    return rv;
}

/* retrieve the full CRL object that best matches the content of a DPCache */
static CERTSignedCrl* GetBestCRL(CRLDPCache* cache, PRBool entries)
{
    CachedCrl* acrl = NULL;

    PORT_Assert(cache);
    if (!cache)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL;
    }

    if (0 == cache->ncrls)
    {
        /* empty cache*/
        PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
        return NULL;
    }    

    /* if we have a valid full CRL selected, return it */
    if (cache->selected)
    {
        return SEC_DupCrl(cache->selected->crl);
    }

    /* otherwise, use latest valid DER CRL */
    acrl = cache->crls[cache->ncrls-1];

    if (acrl && (PR_FALSE == GetOpaqueCRLFields(acrl->crl)->decodingError) )
    {
        SECStatus rv = SECSuccess;
        if (PR_TRUE == entries)
        {
            rv = CERT_CompleteCRLDecodeEntries(acrl->crl);
        }
        if (SECSuccess == rv)
        {
            return SEC_DupCrl(acrl->crl);
        }
    }

    PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
    return NULL;
}

/* get a particular DPCache object from an IssuerCache */
static CRLDPCache* IssuerCache_GetDPCache(CRLIssuerCache* cache, const SECItem* dp)
{
    CRLDPCache* dpp = NULL;
    PORT_Assert(cache);
    /* XCRL for now we only support the "default" DP, ie. the
       full CRL. So we can return the global one without locking. In
       the future we will have a lock */
    PORT_Assert(NULL == dp);
    if (!cache || dp)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return NULL;
    }
#ifdef XCRL
    NSSRWLock_LockRead(cache->lock);
#endif
    dpp = cache->dpp;
#ifdef XCRL
    NSSRWLock_UnlockRead(cache->lock);
#endif
    return dpp;
}

/* get a DPCache object for the given issuer subject and dp
   Automatically creates the cache object if it doesn't exist yet.
   */
SECStatus AcquireDPCache(CERTCertificate* issuer, const SECItem* subject,
                         const SECItem* dp, PRTime t, void* wincx,
                         CRLDPCache** dpcache, PRBool* writeLocked)
{
    SECStatus rv = SECSuccess;
    CRLIssuerCache* issuercache = NULL;
#ifdef GLOBAL_RWLOCK
    PRBool globalwrite = PR_FALSE;
#endif
    PORT_Assert(crlcache.lock);
    if (!crlcache.lock)
    {
        /* CRL cache is not initialized */
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
#ifdef GLOBAL_RWLOCK
    NSSRWLock_LockRead(crlcache.lock);
#else
    PR_Lock(crlcache.lock);
#endif
    rv = CRLCache_GetIssuerCache(&crlcache, subject, &issuercache);
    if (SECSuccess != rv)
    {
#ifdef GLOBAL_RWLOCK
        NSSRWLock_UnlockRead(crlcache.lock);
#else
        PR_Unlock(crlcache.lock);
#endif
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    if (!issuercache)
    {
        /* there is no cache for this issuer yet. This means this is the
           first time we look up a cert from that issuer, and we need to
           create the cache. */
        
        rv = IssuerCache_Create(&issuercache, issuer, subject, dp);
        if (SECSuccess == rv && !issuercache)
        {
            PORT_Assert(issuercache);
            rv = SECFailure;
        }

        if (SECSuccess == rv)
        {
            /* This is the first time we look up a cert of this issuer.
               Create the DPCache for this DP . */
            rv = IssuerCache_AddDP(issuercache, issuer, subject, dp, dpcache);
        }

        if (SECSuccess == rv)
        {
            /* lock the DPCache for write to ensure the update happens in this
               thread */
            *writeLocked = PR_TRUE;
#ifdef DPC_RWLOCK
            NSSRWLock_LockWrite((*dpcache)->lock);
#else
            PR_Lock((*dpcache)->lock);
#endif
        }
        
        if (SECSuccess == rv)
        {
            /* now add the new issuer cache to the global hash table of
               issuers */
#ifdef GLOBAL_RWLOCK
            CRLIssuerCache* existing = NULL;
            NSSRWLock_UnlockRead(crlcache.lock);
            /* when using a r/w lock for the global cache, check if the issuer
               already exists before adding to the hash table */
            NSSRWLock_LockWrite(crlcache.lock);
            globalwrite = PR_TRUE;
            rv = CRLCache_GetIssuerCache(&crlcache, subject, &existing);
            if (!existing)
            {
#endif
                rv = CRLCache_AddIssuer(issuercache);
                if (SECSuccess != rv)
                {
                    /* failure */
                    rv = SECFailure;
                }
#ifdef GLOBAL_RWLOCK
            }
            else
            {
                /* somebody else updated before we did */
                IssuerCache_Destroy(issuercache); /* destroy the new object */
                issuercache = existing; /* use the existing one */
                *dpcache = IssuerCache_GetDPCache(issuercache, dp);
            }
#endif
        }

        /* now unlock the global cache. We only want to lock the issuer hash
           table addition. Holding it longer would hurt scalability */
#ifdef GLOBAL_RWLOCK
        if (PR_TRUE == globalwrite)
        {
            NSSRWLock_UnlockWrite(crlcache.lock);
            globalwrite = PR_FALSE;
        }
        else
        {
            NSSRWLock_UnlockRead(crlcache.lock);
        }
#else
        PR_Unlock(crlcache.lock);
#endif

        /* if there was a failure adding an issuer cache object, destroy it */
        if (SECSuccess != rv && issuercache)
        {
            if (PR_TRUE == *writeLocked)
            {
#ifdef DPC_RWLOCK
                NSSRWLock_UnlockWrite((*dpcache)->lock);
#else
                PR_Unlock((*dpcache)->lock);
#endif
            }
            IssuerCache_Destroy(issuercache);
            issuercache = NULL;
        }

        if (SECSuccess != rv)
        {
            return SECFailure;
        }
    } else
    {
#ifdef GLOBAL_RWLOCK
        NSSRWLock_UnlockRead(crlcache.lock);
#else
        PR_Unlock(crlcache.lock);
#endif
        *dpcache = IssuerCache_GetDPCache(issuercache, dp);
    }
    /* we now have a DPCache that we can use for lookups */
    /* lock it for read, unless we already locked for write */
    if (PR_FALSE == *writeLocked)
    {
#ifdef DPC_RWLOCK
        NSSRWLock_LockRead((*dpcache)->lock);
#else
        PR_Lock((*dpcache)->lock);
#endif
    }
    
    if (SECSuccess == rv)
    {
        /* currently there is always one and only one DPCache per issuer */
        PORT_Assert(*dpcache);
        if (*dpcache)
        {
            /* make sure the DP cache is up to date before using it */
            rv = DPCache_GetUpToDate(*dpcache, issuer, PR_FALSE == *writeLocked,
                                     t, wincx);
        }
        else
        {
            rv = SECFailure;
        }
    }
    return rv;
}

/* unlock access to the DPCache */
void ReleaseDPCache(CRLDPCache* dpcache, PRBool writeLocked)
{
    if (!dpcache)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return;
    }
#ifdef DPC_RWLOCK
    if (PR_TRUE == writeLocked)
    {
        NSSRWLock_UnlockWrite(dpcache->lock);
    }
    else
    {
        NSSRWLock_UnlockRead(dpcache->lock);
    }
#else
    PR_Unlock(dpcache->lock);
#endif
}

SECStatus
cert_CheckCertRevocationStatus(CERTCertificate* cert, CERTCertificate* issuer,
                               const SECItem* dp, PRTime t, void *wincx,
                               CERTRevocationStatus *revStatus,
                               CERTCRLEntryReasonCode *revReason)
{
    PRBool lockedwrite = PR_FALSE;
    SECStatus rv = SECSuccess;
    CRLDPCache* dpcache = NULL;
    CERTRevocationStatus status = certRevocationStatusRevoked;
    CERTCRLEntryReasonCode reason = crlEntryReasonUnspecified;
    CERTCrlEntry* entry = NULL;
    dpcacheStatus ds;

    if (!cert || !issuer)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    if (revStatus)
    {
        *revStatus = status;
    }
    if (revReason)
    {
        *revReason = reason;
    }

    if (t && secCertTimeValid != CERT_CheckCertValidTimes(issuer, t, PR_FALSE))
    {
        /* we won't be able to check the CRL's signature if the issuer cert
           is expired as of the time we are verifying. This may cause a valid
           CRL to be cached as bad. short-circuit to avoid this case. */
        PORT_SetError(SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE);
        return SECFailure;
    }

    rv = AcquireDPCache(issuer, &issuer->derSubject, dp, t, wincx, &dpcache,
                        &lockedwrite);
    PORT_Assert(SECSuccess == rv);
    if (SECSuccess != rv)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    /* now look up the certificate SN in the DP cache's CRL */
    ds = DPCache_Lookup(dpcache, &cert->serialNumber, &entry);
    switch (ds)
    {
        case dpcacheFoundEntry:
            PORT_Assert(entry);
            /* check the time if we have one */
            if (entry->revocationDate.data && entry->revocationDate.len)
            {
                PRTime revocationDate = 0;
                if (SECSuccess == DER_DecodeTimeChoice(&revocationDate,
                                               &entry->revocationDate))
                {
                    /* we got a good revocation date, only consider the
                       certificate revoked if the time we are inquiring about
                       is past the revocation date */
                    if (t>=revocationDate)
                    {
                        rv = SECFailure;
                    }
                    else
                    {
                        status = certRevocationStatusValid;
                    }
                }
                else
                {
                    /* invalid revocation date, consider the certificate
                       permanently revoked */
                    rv = SECFailure;
                }
            }
            else
            {
                /* no revocation date, certificate is permanently revoked */
                rv = SECFailure;
            }
            if (SECFailure == rv)
            {
                SECStatus rv2 = CERT_FindCRLEntryReasonExten(entry, &reason);
                PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
            }
            break;

        case dpcacheEmpty:
            /* useful for NIST policy */
            status = certRevocationStatusUnknown;
            break;

        case dpcacheNoEntry:
            status = certRevocationStatusValid;
            break;

        case dpcacheInvalidCacheError:
            /* treat it as unknown and let the caller decide based on
               the policy */
            status = certRevocationStatusUnknown;
            break;

        default:
            /* leave status as revoked */
            break;
    }

    ReleaseDPCache(dpcache, lockedwrite);
    if (revStatus)
    {
        *revStatus = status;
    }
    if (revReason)
    {
        *revReason = reason;
    }
    return rv;
}

/* check CRL revocation status of given certificate and issuer */
SECStatus
CERT_CheckCRL(CERTCertificate* cert, CERTCertificate* issuer,
              const SECItem* dp, PRTime t, void* wincx)
{
    return cert_CheckCertRevocationStatus(cert, issuer, dp, t, wincx,
                                          NULL, NULL);
}

/* retrieve full CRL object that best matches the cache status */
CERTSignedCrl *
SEC_FindCrlByName(CERTCertDBHandle *handle, SECItem *crlKey, int type)
{
    CERTSignedCrl* acrl = NULL;
    CRLDPCache* dpcache = NULL;
    SECStatus rv = SECSuccess;
    PRBool writeLocked = PR_FALSE;

    if (!crlKey)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    rv = AcquireDPCache(NULL, crlKey, NULL, 0, NULL, &dpcache, &writeLocked);
    if (SECSuccess == rv)
    {
        acrl = GetBestCRL(dpcache, PR_TRUE); /* decode entries, because
        SEC_FindCrlByName always returned fully decoded CRLs in the past */
        ReleaseDPCache(dpcache, writeLocked);
    }
    return acrl;
}

/* invalidate the CRL cache for a given issuer, which forces a refetch of
   CRL objects from PKCS#11 tokens */
void CERT_CRLCacheRefreshIssuer(CERTCertDBHandle* dbhandle, SECItem* crlKey)
{
    CRLDPCache* cache = NULL;
    SECStatus rv = SECSuccess;
    PRBool writeLocked = PR_FALSE;
    PRBool readlocked;

    (void) dbhandle; /* silence compiler warnings */

    /* XCRL we will need to refresh all the DPs of the issuer in the future,
            not just the default one */
    rv = AcquireDPCache(NULL, crlKey, NULL, 0, NULL, &cache, &writeLocked);
    if (SECSuccess != rv)
    {
        return;
    }
    /* we need to invalidate the DPCache here */
    readlocked = (writeLocked == PR_TRUE? PR_FALSE : PR_TRUE);
    DPCache_LockWrite();
    cache->refresh = PR_TRUE;
    DPCache_UnlockWrite();
    ReleaseDPCache(cache, writeLocked);
    return;
}

/* add the specified RAM CRL object to the cache */
SECStatus CERT_CacheCRL(CERTCertDBHandle* dbhandle, SECItem* newdercrl)
{
    CRLDPCache* cache = NULL;
    SECStatus rv = SECSuccess;
    PRBool writeLocked = PR_FALSE;
    PRBool readlocked;
    CachedCrl* returned = NULL;
    PRBool added = PR_FALSE;
    CERTSignedCrl* newcrl = NULL;
    int realerror = 0;
    
    if (!dbhandle || !newdercrl)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* first decode the DER CRL to make sure it's OK */
    newcrl = CERT_DecodeDERCrlWithFlags(NULL, newdercrl, SEC_CRL_TYPE,
                                        CRL_DECODE_DONT_COPY_DER |
                                        CRL_DECODE_SKIP_ENTRIES);

    if (!newcrl)
    {
        return SECFailure;
    }

    /* XXX check if it has IDP extension. If so, do not proceed and set error */

    rv = AcquireDPCache(NULL,
                        &newcrl->crl.derName,
                        NULL, 0, NULL, &cache, &writeLocked);
    if (SECSuccess == rv)
    {
        readlocked = (writeLocked == PR_TRUE? PR_FALSE : PR_TRUE);
    
        rv = CachedCrl_Create(&returned, newcrl, CRL_OriginExplicit);
        if (SECSuccess == rv && returned)
        {
            DPCache_LockWrite();
            rv = DPCache_AddCRL(cache, returned, &added);
            if (PR_TRUE != added)
            {
                realerror = PORT_GetError();
                CachedCrl_Destroy(returned);
                returned = NULL;
            }
            DPCache_UnlockWrite();
        }
    
        ReleaseDPCache(cache, writeLocked);
    
        if (!added)
        {
            rv = SECFailure;
        }
    }
    SEC_DestroyCrl(newcrl); /* free the CRL. Either it got added to the cache
        and the refcount got bumped, or not, and thus we need to free its
        RAM */
    if (realerror)
    {
        PORT_SetError(realerror);
    }
    return rv;
}

/* remove the specified RAM CRL object from the cache */
SECStatus CERT_UncacheCRL(CERTCertDBHandle* dbhandle, SECItem* olddercrl)
{
    CRLDPCache* cache = NULL;
    SECStatus rv = SECSuccess;
    PRBool writeLocked = PR_FALSE;
    PRBool readlocked;
    PRBool removed = PR_FALSE;
    PRUint32 i;
    CERTSignedCrl* oldcrl = NULL;
    
    if (!dbhandle || !olddercrl)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* first decode the DER CRL to make sure it's OK */
    oldcrl = CERT_DecodeDERCrlWithFlags(NULL, olddercrl, SEC_CRL_TYPE,
                                        CRL_DECODE_DONT_COPY_DER |
                                        CRL_DECODE_SKIP_ENTRIES);

    if (!oldcrl)
    {
        /* if this DER CRL can't decode, it can't be in the cache */
        return SECFailure;
    }

    rv = AcquireDPCache(NULL,
                        &oldcrl->crl.derName,
                        NULL, 0, NULL, &cache, &writeLocked);
    if (SECSuccess == rv)
    {
        CachedCrl* returned = NULL;

        readlocked = (writeLocked == PR_TRUE? PR_FALSE : PR_TRUE);
    
        rv = CachedCrl_Create(&returned, oldcrl, CRL_OriginExplicit);
        if (SECSuccess == rv && returned)
        {
            DPCache_LockWrite();
            for (i=0;i<cache->ncrls;i++)
            {
                PRBool dupe = PR_FALSE, updated = PR_FALSE;
                rv = CachedCrl_Compare(returned, cache->crls[i],
                                                      &dupe, &updated);
                if (SECSuccess != rv)
                {
                    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
                    break;
                }
                if (PR_TRUE == dupe)
                {
                    rv = DPCache_RemoveCRL(cache, i); /* got a match */
                    if (SECSuccess == rv) {
                        cache->mustchoose = PR_TRUE;
                        removed = PR_TRUE;
                    }
                    break;
                }
            }
            
            DPCache_UnlockWrite();

            if (SECSuccess != CachedCrl_Destroy(returned) ) {
                rv = SECFailure;
            }
        }

        ReleaseDPCache(cache, writeLocked);
    }
    if (SECSuccess != SEC_DestroyCrl(oldcrl) ) { 
        /* need to do this because object is refcounted */
        rv = SECFailure;
    }
    if (SECSuccess == rv && PR_TRUE != removed)
    {
        PORT_SetError(SEC_ERROR_CRL_NOT_FOUND);
    }
    return rv;
}

SECStatus cert_AcquireNamedCRLCache(NamedCRLCache** returned)
{
    PORT_Assert(returned);
    if (!namedCRLCache.lock)
    {
        PORT_Assert(0);
        return SECFailure;
    }
    PR_Lock(namedCRLCache.lock);
    *returned = &namedCRLCache;
    return SECSuccess;
}

/* This must be called only while cache is acquired, and the entry is only
 * valid until cache is released.
 */
SECStatus cert_FindCRLByGeneralName(NamedCRLCache* ncc,
                                    const SECItem* canonicalizedName,
                                    NamedCRLCacheEntry** retEntry)
{
    if (!ncc || !canonicalizedName || !retEntry)
    {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    *retEntry = (NamedCRLCacheEntry*) PL_HashTableLookup(namedCRLCache.entries,
                                         (void*) canonicalizedName);
    return SECSuccess;
}

SECStatus cert_ReleaseNamedCRLCache(NamedCRLCache* ncc)
{
    if (!ncc)
    {
        return SECFailure;
    }
    if (!ncc->lock)
    {
        PORT_Assert(0);
        return SECFailure;
    }
    PR_Unlock(namedCRLCache.lock);
    return SECSuccess;
}

/* creates new named cache entry from CRL, and tries to add it to CRL cache */
static SECStatus addCRLToCache(CERTCertDBHandle* dbhandle, SECItem* crl,
                                    const SECItem* canonicalizedName,
                                    NamedCRLCacheEntry** newEntry)
{
    SECStatus rv = SECSuccess;
    NamedCRLCacheEntry* entry = NULL;

    /* create new named entry */
    if (SECSuccess != NamedCRLCacheEntry_Create(newEntry) || !*newEntry)
    {
        /* no need to keep unused CRL around */
        SECITEM_ZfreeItem(crl, PR_TRUE);
        return SECFailure;
    }
    entry = *newEntry;
    entry->crl = crl; /* named CRL cache owns DER */
    entry->lastAttemptTime = PR_Now();
    entry->canonicalizedName = SECITEM_DupItem(canonicalizedName);
    if (!entry->canonicalizedName)
    {
        rv = NamedCRLCacheEntry_Destroy(entry); /* destroys CRL too */
        PORT_Assert(SECSuccess == rv);
        return SECFailure;
    }
    /* now, attempt to insert CRL into CRL cache */
    if (SECSuccess == CERT_CacheCRL(dbhandle, entry->crl))
    {
        entry->inCRLCache = PR_TRUE;
        entry->successfulInsertionTime = entry->lastAttemptTime;
    }
    else
    {
        switch (PR_GetError())
        {
            case SEC_ERROR_CRL_ALREADY_EXISTS:
                entry->dupe = PR_TRUE;
                break;

            case SEC_ERROR_BAD_DER:
                entry->badDER = PR_TRUE;
                break;

            /* all other reasons */
            default:
                entry->unsupported = PR_TRUE;
                break;
        }
        rv = SECFailure;
        /* no need to keep unused CRL around */
        SECITEM_ZfreeItem(entry->crl, PR_TRUE);
        entry->crl = NULL;
    }
    return rv;
}

/* take ownership of CRL, and insert it into the named CRL cache
 * and indexed CRL cache
 */
SECStatus cert_CacheCRLByGeneralName(CERTCertDBHandle* dbhandle, SECItem* crl,
                                     const SECItem* canonicalizedName)
{
    NamedCRLCacheEntry* oldEntry, * newEntry = NULL;
    NamedCRLCache* ncc = NULL;
    SECStatus rv = SECSuccess, rv2;

    PORT_Assert(namedCRLCache.lock);
    PORT_Assert(namedCRLCache.entries);

    if (!crl || !canonicalizedName)
    {
        PORT_Assert(0);
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    rv = cert_AcquireNamedCRLCache(&ncc);
    PORT_Assert(SECSuccess == rv);
    if (SECSuccess != rv)
    {
        SECITEM_ZfreeItem(crl, PR_TRUE);
        return SECFailure;
    }
    rv = cert_FindCRLByGeneralName(ncc, canonicalizedName, &oldEntry);
    PORT_Assert(SECSuccess == rv);
    if (SECSuccess != rv)
    {
        rv = cert_ReleaseNamedCRLCache(ncc);
        SECITEM_ZfreeItem(crl, PR_TRUE);
        return SECFailure;
    }
    if (SECSuccess == addCRLToCache(dbhandle, crl, canonicalizedName,
                                    &newEntry) )
    {
        if (!oldEntry)
        {
            /* add new good entry to the hash table */
            if (NULL == PL_HashTableAdd(namedCRLCache.entries,
                                        (void*) newEntry->canonicalizedName,
                                        (void*) newEntry))
            {
                PORT_Assert(0);
                rv2 = NamedCRLCacheEntry_Destroy(newEntry);
                PORT_Assert(SECSuccess == rv2);
                rv = SECFailure;
            }
        }
        else
        {
            PRBool removed;
            /* remove the old CRL from the cache if needed */
            if (oldEntry->inCRLCache)
            {
                rv = CERT_UncacheCRL(dbhandle, oldEntry->crl);
                PORT_Assert(SECSuccess == rv);
            }
            removed = PL_HashTableRemove(namedCRLCache.entries,
                                      (void*) oldEntry->canonicalizedName);
            PORT_Assert(removed);
            if (!removed)
            {
                rv = SECFailure;
		/* leak old entry since we couldn't remove it from the hash table */
            }
            else
            {
                rv2 = NamedCRLCacheEntry_Destroy(oldEntry);
                PORT_Assert(SECSuccess == rv2);
            }
            if (NULL == PL_HashTableAdd(namedCRLCache.entries,
                                      (void*) newEntry->canonicalizedName,
                                      (void*) newEntry))
            {
                PORT_Assert(0);
                rv = SECFailure;
            }
        }
    } else
    {
        /* error adding new CRL to cache */
        if (!oldEntry)
        {
            /* no old cache entry, use the new one even though it's bad */
            if (NULL == PL_HashTableAdd(namedCRLCache.entries,
                                        (void*) newEntry->canonicalizedName,
                                        (void*) newEntry))
            {
                PORT_Assert(0);
                rv = SECFailure;
            }
        }
        else
        {
            if (oldEntry->inCRLCache)
            {
                /* previous cache entry was good, keep it and update time */
                oldEntry-> lastAttemptTime = newEntry->lastAttemptTime;
                /* throw away new bad entry */
                rv = NamedCRLCacheEntry_Destroy(newEntry);
                PORT_Assert(SECSuccess == rv);
            }
            else
            {
                /* previous cache entry was bad, just replace it */
                PRBool removed = PL_HashTableRemove(namedCRLCache.entries,
                                          (void*) oldEntry->canonicalizedName);
                PORT_Assert(removed);
                if (!removed)
                {
		    /* leak old entry since we couldn't remove it from the hash table */
                    rv = SECFailure;
                }
                else
                {
                    rv2 = NamedCRLCacheEntry_Destroy(oldEntry);
                    PORT_Assert(SECSuccess == rv2);
                }
                if (NULL == PL_HashTableAdd(namedCRLCache.entries,
                                          (void*) newEntry->canonicalizedName,
                                          (void*) newEntry))
                {
                    PORT_Assert(0);
                    rv = SECFailure;
                }
            }
        }
    }
    rv2 = cert_ReleaseNamedCRLCache(ncc);
    PORT_Assert(SECSuccess == rv2);

    return rv;
}

static SECStatus CachedCrl_Create(CachedCrl** returned, CERTSignedCrl* crl,
                                  CRLOrigin origin)
{
    CachedCrl* newcrl = NULL;
    if (!returned)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    newcrl = PORT_ZAlloc(sizeof(CachedCrl));
    if (!newcrl)
    {
        return SECFailure;
    }
    newcrl->crl = SEC_DupCrl(crl);
    newcrl->origin = origin;
    *returned = newcrl;
    return SECSuccess;
}

/* empty the cache content */
static SECStatus CachedCrl_Depopulate(CachedCrl* crl)
{
    if (!crl)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
     /* destroy the hash table */
    if (crl->entries)
    {
        PL_HashTableDestroy(crl->entries);
        crl->entries = NULL;
    }

    /* free the pre buffer */
    if (crl->prebuffer)
    {
        PreAllocator_Destroy(crl->prebuffer);
        crl->prebuffer = NULL;
    }
    return SECSuccess;
}

static SECStatus CachedCrl_Destroy(CachedCrl* crl)
{
    if (!crl)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    CachedCrl_Depopulate(crl);
    SEC_DestroyCrl(crl->crl);
    PORT_Free(crl);
    return SECSuccess;
}

/* create hash table of CRL entries */
static SECStatus CachedCrl_Populate(CachedCrl* crlobject)
{
    SECStatus rv = SECFailure;
    CERTCrlEntry** crlEntry = NULL;
    PRUint32 numEntries = 0;

    if (!crlobject)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    /* complete the entry decoding . XXX thread-safety of CRL object */
    rv = CERT_CompleteCRLDecodeEntries(crlobject->crl);
    if (SECSuccess != rv)
    {
        crlobject->unbuildable = PR_TRUE; /* don't try to build this again */
        return SECFailure;
    }

    if (crlobject->entries && crlobject->prebuffer)
    {
        /* cache is already built */
        return SECSuccess;
    }

    /* build the hash table from the full CRL */    
    /* count CRL entries so we can pre-allocate space for hash table entries */
    for (crlEntry = crlobject->crl->crl.entries; crlEntry && *crlEntry;
         crlEntry++)
    {
        numEntries++;
    }
    crlobject->prebuffer = PreAllocator_Create(numEntries*sizeof(PLHashEntry));
    PORT_Assert(crlobject->prebuffer);
    if (!crlobject->prebuffer)
    {
        return SECFailure;
    }
    /* create a new hash table */
    crlobject->entries = PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
                         PL_CompareValues, &preAllocOps, crlobject->prebuffer);
    PORT_Assert(crlobject->entries);
    if (!crlobject->entries)
    {
        return SECFailure;
    }
    /* add all serial numbers to the hash table */
    for (crlEntry = crlobject->crl->crl.entries; crlEntry && *crlEntry;
         crlEntry++)
    {
        PL_HashTableAdd(crlobject->entries, &(*crlEntry)->serialNumber,
                        *crlEntry);
    }

    return SECSuccess;
}

/* returns true if there are CRLs from PKCS#11 slots */
static PRBool DPCache_HasTokenCRLs(CRLDPCache* cache)
{
    PRBool answer = PR_FALSE;
    PRUint32 i;
    for (i=0;i<cache->ncrls;i++)
    {
        if (cache->crls[i] && (CRL_OriginToken == cache->crls[i]->origin) )
        {
            answer = PR_TRUE;
            break;
        }
    }
    return answer;
}

/* are these CRLs the same, as far as the cache is concerned ? */
/* are these CRLs the same token object but with different DER ?
   This can happen if the DER CRL got updated in the token, but the PKCS#11
   object ID did not change. NSS softoken has the unfortunate property to
   never change the object ID for CRL objects. */
static SECStatus CachedCrl_Compare(CachedCrl* a, CachedCrl* b, PRBool* isDupe,
                                PRBool* isUpdated)
{
    PORT_Assert(a);
    PORT_Assert(b);
    PORT_Assert(isDupe);
    PORT_Assert(isUpdated);
    if (!a || !b || !isDupe || !isUpdated || !a->crl || !b->crl)
    {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    *isDupe = *isUpdated = PR_FALSE;

    if (a == b)
    {
        /* dupe */
        *isDupe = PR_TRUE;
        *isUpdated = PR_FALSE;
        return SECSuccess;
    }
    if (b->origin != a->origin)
    {
        /* CRLs of different origins are not considered dupes,
           and can't be updated either */
        return SECSuccess;
    }
    if (CRL_OriginToken == b->origin)
    {
        /* for token CRLs, slot and PKCS#11 object handle must match for CRL
           to truly be a dupe */
        if ( (b->crl->slot == a->crl->slot) &&
             (b->crl->pkcs11ID == a->crl->pkcs11ID) )
        {
            /* ASN.1 DER needs to match for dupe check */
            /* could optimize by just checking a few fields like thisUpdate */
            if ( SECEqual == SECITEM_CompareItem(b->crl->derCrl,
                                                 a->crl->derCrl) )
            {
                *isDupe = PR_TRUE;
            }
            else
            {
                *isUpdated = PR_TRUE;
            }
        }
        return SECSuccess;
    }
    if (CRL_OriginExplicit == b->origin)
    {
        /* We need to make sure this is the same object that the user provided
           to CERT_CacheCRL previously. That API takes a SECItem*, thus, we
           just do a pointer comparison here.
        */
        if (b->crl->derCrl == a->crl->derCrl)
        {
            *isDupe = PR_TRUE;
        }
    }
    return SECSuccess;
}
