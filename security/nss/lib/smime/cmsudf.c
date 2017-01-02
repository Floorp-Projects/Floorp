/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS User Define Types
 */

#include "cmslocal.h"

#include "prinit.h"
#include "pk11func.h"
#include "secitem.h"
#include "secoid.h"
#include "secerr.h"
#include "nss.h"

typedef struct nsscmstypeInfoStr nsscmstypeInfo;
struct nsscmstypeInfoStr {
    SECOidTag type;
    SEC_ASN1Template *template;
    size_t size;
    PRBool isData;
    NSSCMSGenericWrapperDataDestroy  destroy;
    NSSCMSGenericWrapperDataCallback decode_before;
    NSSCMSGenericWrapperDataCallback decode_after;
    NSSCMSGenericWrapperDataCallback decode_end;
    NSSCMSGenericWrapperDataCallback encode_start;
    NSSCMSGenericWrapperDataCallback encode_before;
    NSSCMSGenericWrapperDataCallback encode_after;
};

/* make sure the global tables are only initialized once */
static PRCallOnceType nsscmstypeOnce;
static PRCallOnceType nsscmstypeClearOnce;
/* lock for adding a new entry */
static PRLock *nsscmstypeAddLock;
/* lock for the hash table */
static PRLock *nsscmstypeHashLock;
/* the hash table itself */
static PLHashTable *nsscmstypeHash;
/* arena to hold all the hash table data */
static PLArenaPool *nsscmstypeArena;

/*
 * clean up our global tables
 */
SECStatus
nss_cmstype_shutdown(void *appData, void *reserved)
{
    if (nsscmstypeHashLock) {
	PR_Lock(nsscmstypeHashLock);
    }
    if (nsscmstypeHash) {
	PL_HashTableDestroy(nsscmstypeHash);
	nsscmstypeHash = NULL;
    }
    if (nsscmstypeArena) {
	PORT_FreeArena(nsscmstypeArena, PR_FALSE);
	nsscmstypeArena = NULL;
    }
    if (nsscmstypeAddLock) {
	PR_DestroyLock(nsscmstypeAddLock);
    }
    if (nsscmstypeHashLock) {
	PRLock *oldLock = nsscmstypeHashLock;
	nsscmstypeHashLock = NULL;
	PR_Unlock(oldLock);
	PR_DestroyLock(oldLock);
    }

    /* don't clear out the PR_ONCE data if we failed our inital call */
    if (appData == NULL) {
    	nsscmstypeOnce = nsscmstypeClearOnce;
    }
    return SECSuccess;
}

static PLHashNumber
nss_cmstype_hash_key(const void *key)
{
   return (PLHashNumber)((char *)key - (char *)NULL);
}

static PRIntn
nss_cmstype_compare_keys(const void *v1, const void *v2)
{
   PLHashNumber value1 = nss_cmstype_hash_key(v1);
   PLHashNumber value2 = nss_cmstype_hash_key(v2);

   return (value1 == value2);
}

/*
 * initialize our hash tables, called once on the first attemat to register
 * a new SMIME type.
 */
static PRStatus
nss_cmstype_init(void)
{
    SECStatus rv;
        
    nsscmstypeHashLock = PR_NewLock();
    if (nsscmstypeHashLock == NULL) {
	return PR_FAILURE;
    }
    nsscmstypeAddLock = PR_NewLock();
    if (nsscmstypeHashLock == NULL) {
	goto fail;
    }
    nsscmstypeHash = PL_NewHashTable(64, nss_cmstype_hash_key, 
		nss_cmstype_compare_keys, PL_CompareValues, NULL, NULL);
    if (nsscmstypeHash == NULL) {
	goto fail;
    }
    nsscmstypeArena = PORT_NewArena(2048);
    if (nsscmstypeArena == NULL) {
	goto fail;
    }
    rv = NSS_RegisterShutdown(nss_cmstype_shutdown, NULL);
    if (rv != SECSuccess) {
	goto fail;
    }
    return PR_SUCCESS;

fail:
    nss_cmstype_shutdown(&nsscmstypeOnce, NULL);
    return PR_FAILURE;
}

 
/*
 * look up and registered SIME type
 */
static const nsscmstypeInfo *
nss_cmstype_lookup(SECOidTag type)
{
    nsscmstypeInfo *typeInfo = NULL;;
    if (!nsscmstypeHash) {
	return NULL;
    }
    PR_Lock(nsscmstypeHashLock);
    if (nsscmstypeHash) {
	typeInfo = PL_HashTableLookupConst(nsscmstypeHash, (void *)type);
    }
    PR_Unlock(nsscmstypeHashLock);
    return typeInfo;
}

/*
 * add a new type to the SMIME type table
 */
static SECStatus
nss_cmstype_add(SECOidTag type, nsscmstypeInfo *typeinfo)
{
    PLHashEntry *entry;

    if (!nsscmstypeHash) {
	/* assert? this shouldn't happen */
	return SECFailure;
    }
    PR_Lock(nsscmstypeHashLock);
    /* this is really paranoia. If we really are racing nsscmstypeHash, we'll
     * also be racing nsscmstypeHashLock... */
    if (!nsscmstypeHash) {
	PR_Unlock(nsscmstypeHashLock);
	return SECFailure;
    }
    entry = PL_HashTableAdd(nsscmstypeHash, (void *)type, typeinfo);
    PR_Unlock(nsscmstypeHashLock);
    return entry ? SECSuccess : SECFailure;
}


/* helper functions to manage new content types
 */

PRBool
NSS_CMSType_IsWrapper(SECOidTag type)
{
    const nsscmstypeInfo *typeInfo = NULL;

    switch (type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	return PR_TRUE;
    default:
	typeInfo = nss_cmstype_lookup(type);
	if (typeInfo && !typeInfo->isData) {
	    return PR_TRUE;
	}
    }
    return PR_FALSE;
}

PRBool
NSS_CMSType_IsData(SECOidTag type)
{
    const nsscmstypeInfo *typeInfo = NULL;

    switch (type) {
    case SEC_OID_PKCS7_DATA:
	return PR_TRUE;
    default:
	typeInfo = nss_cmstype_lookup(type);
	if (typeInfo && typeInfo->isData) {
	    return PR_TRUE;
	}
    }
    return PR_FALSE;
}

const SEC_ASN1Template *
NSS_CMSType_GetTemplate(SECOidTag type)
{
    const nsscmstypeInfo *typeInfo = nss_cmstype_lookup(type);

    if (typeInfo && typeInfo->template) {
	return typeInfo->template;
    }
    return SEC_ASN1_GET(SEC_PointerToOctetStringTemplate);
}

size_t
NSS_CMSType_GetContentSize(SECOidTag type)
{
    const nsscmstypeInfo *typeInfo = nss_cmstype_lookup(type);

    if (typeInfo) {
	return typeInfo->size;
    }
    return sizeof(SECItem *);

}

void
NSS_CMSGenericWrapperData_Destroy(SECOidTag type, NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo = nss_cmstype_lookup(type);

    if (typeInfo && typeInfo->destroy) {
	(*typeInfo->destroy)(gd);
    }
    
}


SECStatus
NSS_CMSGenericWrapperData_Decode_BeforeData(SECOidTag type, 
				     NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo;

    /* short cut common case */
    if (type == SEC_OID_PKCS7_DATA) {
	return SECSuccess;
    }

    typeInfo = nss_cmstype_lookup(type);
    if (typeInfo) {
	if  (typeInfo->decode_before) {
	    return (*typeInfo->decode_before)(gd);
	}
	/* decoder ops optional for data tags */
	if (typeInfo->isData) {
	    return SECSuccess;
	}
    }
    /* expected a function, but none existed */
    return SECFailure;
    
}

SECStatus
NSS_CMSGenericWrapperData_Decode_AfterData(SECOidTag type, 
				    NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo;

    /* short cut common case */
    if (type == SEC_OID_PKCS7_DATA) {
	return SECSuccess;
    }

    typeInfo = nss_cmstype_lookup(type);
    if (typeInfo) {
	if  (typeInfo->decode_after) {
	    return (*typeInfo->decode_after)(gd);
	}
	/* decoder ops optional for data tags */
	if (typeInfo->isData) {
	    return SECSuccess;
	}
    }
    /* expected a function, but none existed */
    return SECFailure;
}

SECStatus
NSS_CMSGenericWrapperData_Decode_AfterEnd(SECOidTag type, 
				   NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo;

    /* short cut common case */
    if (type == SEC_OID_PKCS7_DATA) {
	return SECSuccess;
    }

    typeInfo = nss_cmstype_lookup(type);
    if (typeInfo) {
	if  (typeInfo->decode_end) {
	    return (*typeInfo->decode_end)(gd);
	}
	/* decoder ops optional for data tags */
	if (typeInfo->isData) {
	    return SECSuccess;
	}
    }
    /* expected a function, but none existed */
    return SECFailure;
}

SECStatus
NSS_CMSGenericWrapperData_Encode_BeforeStart(SECOidTag type, 
				      NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo;

    /* short cut common case */
    if (type == SEC_OID_PKCS7_DATA) {
	return SECSuccess;
    }

    typeInfo = nss_cmstype_lookup(type);
    if (typeInfo) {
	if  (typeInfo->encode_start) {
	    return (*typeInfo->encode_start)(gd);
	}
	/* decoder ops optional for data tags */
	if (typeInfo->isData) {
	    return SECSuccess;
	}
    }
    /* expected a function, but none existed */
    return SECFailure;
}

SECStatus
NSS_CMSGenericWrapperData_Encode_BeforeData(SECOidTag type, 
				     NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo;

    /* short cut common case */
    if (type == SEC_OID_PKCS7_DATA) {
	return SECSuccess;
    }

    typeInfo = nss_cmstype_lookup(type);
    if (typeInfo) {
	if  (typeInfo->encode_before) {
	    return (*typeInfo->encode_before)(gd);
	}
	/* decoder ops optional for data tags */
	if (typeInfo->isData) {
	    return SECSuccess;
	}
    }
    /* expected a function, but none existed */
    return SECFailure;
}

SECStatus
NSS_CMSGenericWrapperData_Encode_AfterData(SECOidTag type, 
				    NSSCMSGenericWrapperData *gd)
{
    const nsscmstypeInfo *typeInfo;

    /* short cut common case */
    if (type == SEC_OID_PKCS7_DATA) {
	return SECSuccess;
    }

    typeInfo = nss_cmstype_lookup(type);
    if (typeInfo) {
	if  (typeInfo->encode_after) {
	    return (*typeInfo->encode_after)(gd);
	}
	/* decoder ops optional for data tags */
	if (typeInfo->isData) {
	    return SECSuccess;
	}
    }
    /* expected a function, but none existed */
    return SECFailure;
}


SECStatus
NSS_CMSType_RegisterContentType(SECOidTag type, 
			SEC_ASN1Template *asn1Template, size_t size, 
			NSSCMSGenericWrapperDataDestroy destroy,
			NSSCMSGenericWrapperDataCallback decode_before,
			NSSCMSGenericWrapperDataCallback decode_after,
			NSSCMSGenericWrapperDataCallback decode_end,
			NSSCMSGenericWrapperDataCallback encode_start,
			NSSCMSGenericWrapperDataCallback encode_before,
			NSSCMSGenericWrapperDataCallback encode_after,
			PRBool isData)
{
    PRStatus rc;
    SECStatus rv;
    nsscmstypeInfo *typeInfo;
    const nsscmstypeInfo *exists;

    rc = PR_CallOnce( &nsscmstypeOnce, nss_cmstype_init);
    if (rc == PR_FAILURE) {
	return SECFailure;
    }
    PR_Lock(nsscmstypeAddLock);
    exists = nss_cmstype_lookup(type);
    if (exists) {
	PR_Unlock(nsscmstypeAddLock);
	/* already added */
	return SECSuccess;
    }
    typeInfo = PORT_ArenaNew(nsscmstypeArena, nsscmstypeInfo);
    typeInfo->type = type;
    typeInfo->size = size;
    typeInfo->isData = isData;
    typeInfo->template = asn1Template;
    typeInfo->destroy = destroy;
    typeInfo->decode_before = decode_before;
    typeInfo->decode_after = decode_after;
    typeInfo->decode_end = decode_end;
    typeInfo->encode_start = encode_start;
    typeInfo->encode_before = encode_before;
    typeInfo->encode_after = encode_after;
    rv = nss_cmstype_add(type, typeInfo);
    PR_Unlock(nsscmstypeAddLock);
    return rv;
}

