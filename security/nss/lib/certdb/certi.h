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
 * certi.h - private data structures for the certificate library
 *
 * $Id: certi.h,v 1.10 2003/06/06 01:17:04 nelsonb%netscape.com Exp $
 */
#ifndef _CERTI_H_
#define _CERTI_H_

#include "certt.h"
#include "nssrwlkt.h"

#define USE_RWLOCK 1

/* all definitions in this file are subject to change */

typedef struct OpaqueCRLFieldsStr OpaqueCRLFields;
typedef struct CRLEntryCacheStr CRLEntryCache;
typedef struct CRLDPCacheStr CRLDPCache;
typedef struct CRLIssuerCacheStr CRLIssuerCache;
typedef struct CRLCacheStr CRLCache;

struct OpaqueCRLFieldsStr {
    PRBool partial;
    PRBool badEntries;
    PRBool bad;
    PRBool badDER;
    PRBool badExtensions;
    PRBool deleted;
    PRBool heapDER;
    PRBool unverified;
};

typedef struct PreAllocatorStr PreAllocator;

struct PreAllocatorStr
{
    PRSize len;
    void* data;
    PRSize used;
    PRArenaPool* arena;
    PRSize extra;
};

/*  CRL entry cache.
    This is the same as an entry plus the next/prev pointers for the hash table
*/

struct CRLEntryCacheStr {
    CERTCrlEntry entry;
    CRLEntryCache *prev, *next;
};

#define CRL_CACHE_INVALID_CRLS              0x0001 /* this state will be set
        if we have CRL objects with an invalid DER or signature. Can be
        cleared if the invalid objects are deleted from the token */
#define CRL_CACHE_LAST_FETCH_FAILED         0x0002 /* this state will be set
        if the last CRL fetch encountered an error. Can be cleared if a
        new fetch succeeds */

#define CRL_CACHE_OUT_OF_MEMORY             0x0004 /* this state will be set
        if we don't have enough memory to build the hash table of entries */

/*  CRL distribution point cache object
    This is a cache of CRL entries for a given distribution point of an issuer
    It is built from a collection of one full and 0 or more delta CRLs.
*/

struct CRLDPCacheStr {
#ifdef USE_RWLOCK
    NSSRWLock* lock;
#else
    PRLock* lock;
#endif
    CERTCertificate* issuer;    /* cert issuer */
    SECItem* subject;           /* DER of issuer subject */
    SECItem* distributionPoint; /* DER of distribution point. This may be
                                   NULL when distribution points aren't
                                   in use (ie. the CA has a single CRL) */

    /* hash table of entries. We use a PLHashTable and pre-allocate the
       required amount of memory in one shot, so that our allocator can
       simply pass offsets into it when hashing.

       This won't work anymore when we support delta CRLs and iCRLs, because
       the size of the hash table will vary over time. At that point, the best
       solution will be to allocate large CRLEntry structures by modifying
       the DER decoding template. The extra space would be for next/prev
       pointers. This would allow entries from different CRLs to be mixed in
       the same hash table.
    */
    PLHashTable* entries;
    PreAllocator* prebuffer; /* big pre-allocated buffer mentioned above */

    /* array of CRLs matching this distribution point */
    PRUint32 ncrls;              /* total number of CRLs in crls */
    CERTSignedCrl** crls;       /* array of all matching DER CRLs
                                   from all tokens */
    /* XCRL With iCRLs and multiple DPs, the CRL can be shared accross several
       issuers. In the future, we'll need to globally recycle the CRL in a
       separate list in order to avoid extra lookups, decodes, and copies */

    /* pointers to good decoded CRLs used to build the cache */
    CERTSignedCrl* full;    /* full CRL used for the cache */
#if 0
    /* for future use */
    PRInt32 numdeltas;      /* number of delta CRLs used for the cache */
    CERTSignedCrl** deltas; /* delta CRLs used for the cache */
#endif
    /* invalidity bitflag */
    PRUint16 invalid;       /* this state will be set if either
             CRL_CACHE_INVALID_CRLS or CRL_CACHE_LAST_FETCH_FAILED is set.
             In those cases, all certs are considered revoked as a
             security precaution. The invalid state can only be cleared
             during an update if all error states are cleared */
};

/*  CRL issuer cache object
    This object tracks all the distribution point caches for a given issuer.
    XCRL once we support multiple issuing distribution points, this object
    will be a hash table. For now, it just holds the single CRL distribution
    point cache structure.
*/

struct CRLIssuerCacheStr {
    SECItem* subject;           /* DER of issuer subject */
    CRLDPCache dp;              /* DER of distribution point */
    CRLDPCache* dpp;
#if 0
    /* XCRL for future use.
       We don't need to lock at the moment because we only have one DP,
       which gets created at the same time as this object */
    NSSRWLock* lock;
    CRLDPCache** dps;
    PLHashTable* distributionpoints;
    CERTCertificate* issuer;
#endif
};

/*  CRL revocation cache object
    This object tracks all the issuer caches
*/

struct CRLCacheStr {
    PRLock* lock;
    /* hash table of issuer to CRLIssuerCacheStr,
       indexed by issuer DER subject */
    PLHashTable* issuers;
};

SECStatus InitCRLCache(void);
SECStatus ShutdownCRLCache(void);

/* Returns a pointer to an environment-like string, a series of
** null-terminated strings, terminated by a zero-length string.
** This function is intended to be internal to NSS.
*/
extern char * cert_GetCertificateEmailAddresses(CERTCertificate *cert);

/*
 * These functions are used to map subjectKeyID extension values to certs.
 */
SECStatus
cert_CreateSubjectKeyIDHashTable(void);

SECStatus
cert_AddSubjectKeyIDMapping(SECItem *subjKeyID, CERTCertificate *cert);

/*
 * Call this function to remove an entry from the mapping table.
 */
SECStatus
cert_RemoveSubjectKeyIDMapping(SECItem *subjKeyID);

SECStatus
cert_DestroySubjectKeyIDHashTable(void);

SECItem*
cert_FindDERCertBySubjectKeyID(SECItem *subjKeyID);

/* return maximum length of AVA value based on its type OID tag. */
extern int cert_AVAOidTagToMaxLen(SECOidTag tag);

#endif /* _CERTI_H_ */

