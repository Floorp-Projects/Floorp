/* This file implements the SERVER Session ID cache. 
 * NOTE:  The contents of this file are NOT used by the client.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
/* $Id: sslsnce.c,v 1.30 2004/06/19 03:21:39 jpierre%netscape.com Exp $ */

/* Note: ssl_FreeSID() in sslnonce.c gets used for both client and server 
 * cache sids!
 *
 * About record locking among different server processes:
 *
 * All processes that are part of the same conceptual server (serving on 
 * the same address and port) MUST share a common SSL session cache. 
 * This code makes the content of the shared cache accessible to all
 * processes on the same "server".  This code works on Unix and Win32 only.
 *
 * We use NSPR anonymous shared memory and move data to & from shared memory.
 * We must do explicit locking of the records for all reads and writes.
 * The set of Cache entries are divided up into "sets" of 128 entries. 
 * Each set is protected by a lock.  There may be one or more sets protected
 * by each lock.  That is, locks to sets are 1:N.
 * There is one lock for the entire cert cache.
 * There is one lock for the set of wrapped sym wrap keys.
 *
 * The anonymous shared memory is laid out as if it were declared like this:
 *
 * struct {
 *     cacheDescriptor          desc;
 *     sidCacheLock             sidCacheLocks[ numSIDCacheLocks];
 *     sidCacheLock             keyCacheLock;
 *     sidCacheLock             certCacheLock;
 *     sidCacheSet              sidCacheSets[ numSIDCacheSets ];
 *     sidCacheEntry            sidCacheData[ numSIDCacheEntries];
 *     certCacheEntry           certCacheData[numCertCacheEntries];
 *     SSLWrappedSymWrappingKey keyCacheData[kt_kea_size][SSL_NUM_WRAP_MECHS];
 * } cacheMemCacheData;
 */
#include "nssrenam.h"
#include "seccomon.h"

#if (defined(XP_UNIX) || defined(XP_WIN32) || defined (XP_OS2) || defined(XP_BEOS)) && !defined(_WIN32_WCE)

#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "pk11func.h"
#include "base64.h"

#include <stdio.h>

#if defined(XP_UNIX) || defined(XP_BEOS)

#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "unix_err.h"

#else

#ifdef XP_WIN32
#include <wtypes.h>
#include "win32err.h"
#endif

#endif 
#include <sys/types.h>

#define SET_ERROR_CODE /* reminder */

#include "nspr.h"
#include "nsslocks.h"
#include "sslmutex.h"

#ifdef XP_OS2_VACPP
#pragma pack(1)
#endif

/*
** Format of a cache entry in the shared memory.
*/ 
struct sidCacheEntryStr {
/* 16 */    PRIPv6Addr  addr;	/* client's IP address */
/*  4 */    PRUint32    creationTime;
/*  4 */    PRUint32    lastAccessTime;	
/*  4 */    PRUint32    expirationTime;
/*  2 */    PRUint16	version;
/*  1 */    PRUint8	valid;
/*  1 */    PRUint8     sessionIDLength;
/* 32 */    PRUint8     sessionID[SSL3_SESSIONID_BYTES];
/*  2 */    PRUint16    authAlgorithm;
/*  2 */    PRUint16    authKeyBits;
/*  2 */    PRUint16    keaType;
/*  2 */    PRUint16    keaKeyBits;
/* 72  - common header total */

    union {
	struct {
/* 64 */    PRUint8	masterKey[SSL_MAX_MASTER_KEY_BYTES];
/* 32 */    PRUint8	cipherArg[SSL_MAX_CYPHER_ARG_BYTES];

/*  1 */    PRUint8	cipherType;
/*  1 */    PRUint8	masterKeyLen;
/*  1 */    PRUint8	keyBits;
/*  1 */    PRUint8	secretKeyBits;
/*  1 */    PRUint8	cipherArgLen;
/*101 */} ssl2;

	struct {
/*  2 */    ssl3CipherSuite  cipherSuite;
/*  2 */    PRUint16    compression; 	/* SSL3CompressionMethod */

/*122 */    ssl3SidKeys keys;	/* keys and ivs, wrapped as needed. */
/*  1 */    PRUint8     hasFortezza;
/*  1 */    PRUint8     resumable;

/*  4 */    PRUint32    masterWrapMech; 
/*  4 */    SSL3KEAType exchKeyType;
/*  4 */    PRInt32     certIndex;
/*140 */} ssl3;
#if defined(LINUX)
	struct {
	    PRUint8     filler[144];	
	} forceSize;
#endif
    } u;
};
typedef struct sidCacheEntryStr sidCacheEntry;

/* The length of this struct is supposed to be a power of 2, e.g. 4KB */
struct certCacheEntryStr {
    PRUint16    certLength;				/*    2 */
    PRUint16    sessionIDLength;			/*    2 */
    PRUint8 	sessionID[SSL3_SESSIONID_BYTES];	/*   32 */
    PRUint8 	cert[SSL_MAX_CACHED_CERT_LEN];		/* 4060 */
};						/* total   4096 */
typedef struct certCacheEntryStr certCacheEntry;

struct sidCacheLockStr {
    PRUint32	timeStamp;
    sslMutex	mutex;
    sslPID	pid;
};
typedef struct sidCacheLockStr sidCacheLock;

struct sidCacheSetStr {
    PRIntn	next;
};
typedef struct sidCacheSetStr sidCacheSet;

struct cacheDescStr {

    PRUint32            cacheMemSize;

    PRUint32		numSIDCacheLocks;
    PRUint32		numSIDCacheSets;
    PRUint32		numSIDCacheSetsPerLock;

    PRUint32            numSIDCacheEntries; 
    PRUint32            sidCacheSize;

    PRUint32            numCertCacheEntries;
    PRUint32            certCacheSize;

    PRUint32            numKeyCacheEntries;
    PRUint32            keyCacheSize;

    PRUint32		ssl2Timeout;
    PRUint32		ssl3Timeout;

    PRUint32            numSIDCacheLocksInitialized;

    /* These values are volatile, and are accessed through sharedCache-> */
    PRUint32		nextCertCacheEntry;	/* certCacheLock protects */
    PRBool      	stopPolling;
    PRBool		everInherited;

    /* The private copies of these values are pointers into shared mem */
    /* The copies of these values in shared memory are merely offsets */
    sidCacheLock    *          sidCacheLocks;
    sidCacheLock    *          keyCacheLock;
    sidCacheLock    *          certCacheLock;
    sidCacheSet     *          sidCacheSets;
    sidCacheEntry   *          sidCacheData;
    certCacheEntry  *          certCacheData;
    SSLWrappedSymWrappingKey * keyCacheData;

    /* Only the private copies of these pointers are valid */
    char *                     cacheMem;
    struct cacheDescStr *      sharedCache;  /* shared copy of this struct */
    PRFileMap *                cacheMemMap;
    PRThread  *                poller;
    PRBool                     shared;
};
typedef struct cacheDescStr cacheDesc;

static cacheDesc globalCache;

static const char envVarName[] = { SSL_ENV_VAR_NAME };

static PRBool isMultiProcess  = PR_FALSE;


#define DEF_SID_CACHE_ENTRIES  10000
#define DEF_CERT_CACHE_ENTRIES 250
#define MIN_CERT_CACHE_ENTRIES 125 /* the effective size in old releases. */
#define DEF_KEY_CACHE_ENTRIES  250

#define SID_CACHE_ENTRIES_PER_SET  128
#define SID_ALIGNMENT          16

#define DEF_SSL2_TIMEOUT	100   /* seconds */
#define MAX_SSL2_TIMEOUT	100   /* seconds */
#define MIN_SSL2_TIMEOUT	  5   /* seconds */

#define DEF_SSL3_TIMEOUT      86400L  /* 24 hours */
#define MAX_SSL3_TIMEOUT      86400L  /* 24 hours */
#define MIN_SSL3_TIMEOUT          5   /* seconds  */

#if defined(AIX) || defined(LINUX) || defined(VMS)
#define MAX_SID_CACHE_LOCKS 8	/* two FDs per lock */
#elif defined(OSF1)
#define MAX_SID_CACHE_LOCKS 16	/* one FD per lock */
#else
#define MAX_SID_CACHE_LOCKS 256
#endif

#define SID_HOWMANY(val, size) (((val) + ((size) - 1)) / (size))
#define SID_ROUNDUP(val, size) ((size) * SID_HOWMANY((val), (size)))


static sslPID myPid;
static PRUint32  ssl_max_sid_cache_locks = MAX_SID_CACHE_LOCKS;

/* forward static function declarations */
static PRUint32 SIDindex(cacheDesc *cache, const PRIPv6Addr *addr, PRUint8 *s, 
                         unsigned nl);
static SECStatus LaunchLockPoller(cacheDesc *cache);


struct inheritanceStr {
    PRUint32 cacheMemSize;
    PRUint32 fmStrLen;
};

typedef struct inheritanceStr inheritance;

#if defined(_WIN32) || defined(XP_OS2)

#define DEFAULT_CACHE_DIRECTORY "\\temp"

#endif /* _win32 */

#if defined(XP_UNIX) || defined(XP_BEOS)

#define DEFAULT_CACHE_DIRECTORY "/tmp"

#endif /* XP_UNIX || XP_BEOS */


/************************************************************************/

static PRUint32
LockSidCacheLock(sidCacheLock *lock, PRUint32 now)
{
    SECStatus      rv      = sslMutex_Lock(&lock->mutex);
    if (rv != SECSuccess)
    	return 0;
    if (!now)
	now  = ssl_Time();
    lock->timeStamp = now;
    lock->pid       = myPid;
    return now;
}

static SECStatus
UnlockSidCacheLock(sidCacheLock *lock)
{
    SECStatus      rv;

    lock->pid = 0;
    rv        = sslMutex_Unlock(&lock->mutex);
    return rv;
}

/* returns the value of ssl_Time on success, zero on failure. */
static PRUint32
LockSet(cacheDesc *cache, PRUint32 set, PRUint32 now)
{
    PRUint32       lockNum = set % cache->numSIDCacheLocks;
    sidCacheLock * lock    = cache->sidCacheLocks + lockNum;

    return LockSidCacheLock(lock, now);
}

static SECStatus
UnlockSet(cacheDesc *cache, PRUint32 set)
{
    PRUint32       lockNum = set % cache->numSIDCacheLocks;
    sidCacheLock * lock    = cache->sidCacheLocks + lockNum;

    return UnlockSidCacheLock(lock);
}

/************************************************************************/


/* Put a certificate in the cache.  Update the cert index in the sce.
*/
static PRUint32
CacheCert(cacheDesc * cache, CERTCertificate *cert, sidCacheEntry *sce)
{
    PRUint32        now;
    certCacheEntry  cce;

    if ((cert->derCert.len > SSL_MAX_CACHED_CERT_LEN) ||
        (cert->derCert.len <= 0) ||
	(cert->derCert.data == NULL)) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return 0;
    }

    cce.sessionIDLength = sce->sessionIDLength;
    PORT_Memcpy(cce.sessionID, sce->sessionID, cce.sessionIDLength);

    cce.certLength = cert->derCert.len;
    PORT_Memcpy(cce.cert, cert->derCert.data, cce.certLength);

    /* get lock on cert cache */
    now = LockSidCacheLock(cache->certCacheLock, 0);
    if (now) {

	/* Find where to place the next cert cache entry. */
	cacheDesc * sharedCache = cache->sharedCache;
	PRUint32    ndx         = sharedCache->nextCertCacheEntry;

	/* write the entry */
	cache->certCacheData[ndx] = cce;

	/* remember where we put it. */
	sce->u.ssl3.certIndex = ndx;

	/* update the "next" cache entry index */
	sharedCache->nextCertCacheEntry = 
					(ndx + 1) % cache->numCertCacheEntries;

	UnlockSidCacheLock(cache->certCacheLock);
    }
    return now;

}

/*
** Convert local SID to shared memory one
*/
static void 
ConvertFromSID(sidCacheEntry *to, sslSessionID *from)
{
    to->valid   = 1;
    to->version = from->version;
    to->addr    = from->addr;
    to->creationTime    = from->creationTime;
    to->lastAccessTime  = from->lastAccessTime;
    to->expirationTime  = from->expirationTime;
    to->authAlgorithm	= from->authAlgorithm;
    to->authKeyBits	= from->authKeyBits;
    to->keaType		= from->keaType;
    to->keaKeyBits	= from->keaKeyBits;

    if (from->version < SSL_LIBRARY_VERSION_3_0) {
	if ((from->u.ssl2.masterKey.len > SSL_MAX_MASTER_KEY_BYTES) ||
	    (from->u.ssl2.cipherArg.len > SSL_MAX_CYPHER_ARG_BYTES)) {
	    SSL_DBG(("%d: SSL: masterKeyLen=%d cipherArgLen=%d",
		     myPid, from->u.ssl2.masterKey.len,
		     from->u.ssl2.cipherArg.len));
	    to->valid = 0;
	    return;
	}

	to->u.ssl2.cipherType    = from->u.ssl2.cipherType;
	to->u.ssl2.masterKeyLen  = from->u.ssl2.masterKey.len;
	to->u.ssl2.cipherArgLen  = from->u.ssl2.cipherArg.len;
	to->u.ssl2.keyBits       = from->u.ssl2.keyBits;
	to->u.ssl2.secretKeyBits = from->u.ssl2.secretKeyBits;
	to->sessionIDLength      = SSL2_SESSIONID_BYTES;
	PORT_Memcpy(to->sessionID, from->u.ssl2.sessionID, SSL2_SESSIONID_BYTES);
	PORT_Memcpy(to->u.ssl2.masterKey, from->u.ssl2.masterKey.data,
		  from->u.ssl2.masterKey.len);
	PORT_Memcpy(to->u.ssl2.cipherArg, from->u.ssl2.cipherArg.data,
		  from->u.ssl2.cipherArg.len);
#ifdef DEBUG
	PORT_Memset(to->u.ssl2.masterKey+from->u.ssl2.masterKey.len, 0,
		  sizeof(to->u.ssl2.masterKey) - from->u.ssl2.masterKey.len);
	PORT_Memset(to->u.ssl2.cipherArg+from->u.ssl2.cipherArg.len, 0,
		  sizeof(to->u.ssl2.cipherArg) - from->u.ssl2.cipherArg.len);
#endif
	SSL_TRC(8, ("%d: SSL: ConvertSID: masterKeyLen=%d cipherArgLen=%d "
		    "time=%d addr=0x%08x%08x%08x%08x cipherType=%d", myPid,
		    to->u.ssl2.masterKeyLen, to->u.ssl2.cipherArgLen,
		    to->creationTime, to->addr.pr_s6_addr32[0],
		    to->addr.pr_s6_addr32[1], to->addr.pr_s6_addr32[2],
		    to->addr.pr_s6_addr32[3], to->u.ssl2.cipherType));
    } else {
	/* This is an SSL v3 session */

	to->u.ssl3.cipherSuite      = from->u.ssl3.cipherSuite;
	to->u.ssl3.compression      = (uint16)from->u.ssl3.compression;
	to->u.ssl3.resumable        = from->u.ssl3.resumable;
	to->u.ssl3.hasFortezza      = from->u.ssl3.hasFortezza;
	to->u.ssl3.keys             = from->u.ssl3.keys;
	to->u.ssl3.masterWrapMech   = from->u.ssl3.masterWrapMech;
	to->u.ssl3.exchKeyType      = from->u.ssl3.exchKeyType;
	to->sessionIDLength         = from->u.ssl3.sessionIDLength;
	to->u.ssl3.certIndex        = -1;

	PORT_Memcpy(to->sessionID, from->u.ssl3.sessionID,
		    to->sessionIDLength);

	SSL_TRC(8, ("%d: SSL3: ConvertSID: time=%d addr=0x%08x%08x%08x%08x "
	            "cipherSuite=%d",
		    myPid, to->creationTime, to->addr.pr_s6_addr32[0],
		    to->addr.pr_s6_addr32[1], to->addr.pr_s6_addr32[2],
		    to->addr.pr_s6_addr32[3], to->u.ssl3.cipherSuite));
    }
}

/*
** Convert shared memory cache-entry to local memory based one
** This is only called from ServerSessionIDLookup().
** Caller must hold cache lock when calling this.
*/
static sslSessionID *
ConvertToSID(sidCacheEntry *from, certCacheEntry *pcce, 
             CERTCertDBHandle * dbHandle)
{
    sslSessionID *to;
    uint16 version = from->version;

    to = (sslSessionID*) PORT_ZAlloc(sizeof(sslSessionID));
    if (!to) {
	return 0;
    }

    if (version < SSL_LIBRARY_VERSION_3_0) {
	/* This is an SSL v2 session */
	to->u.ssl2.masterKey.data =
	    (unsigned char*) PORT_Alloc(from->u.ssl2.masterKeyLen);
	if (!to->u.ssl2.masterKey.data) {
	    goto loser;
	}
	if (from->u.ssl2.cipherArgLen) {
	    to->u.ssl2.cipherArg.data = 
	    	(unsigned char*)PORT_Alloc(from->u.ssl2.cipherArgLen);
	    if (!to->u.ssl2.cipherArg.data) {
		goto loser;
	    }
	    PORT_Memcpy(to->u.ssl2.cipherArg.data, from->u.ssl2.cipherArg,
		        from->u.ssl2.cipherArgLen);
	}

	to->u.ssl2.cipherType    = from->u.ssl2.cipherType;
	to->u.ssl2.masterKey.len = from->u.ssl2.masterKeyLen;
	to->u.ssl2.cipherArg.len = from->u.ssl2.cipherArgLen;
	to->u.ssl2.keyBits       = from->u.ssl2.keyBits;
	to->u.ssl2.secretKeyBits = from->u.ssl2.secretKeyBits;
/*	to->sessionIDLength      = SSL2_SESSIONID_BYTES; */
	PORT_Memcpy(to->u.ssl2.sessionID, from->sessionID, SSL2_SESSIONID_BYTES);
	PORT_Memcpy(to->u.ssl2.masterKey.data, from->u.ssl2.masterKey,
		    from->u.ssl2.masterKeyLen);

	SSL_TRC(8, ("%d: SSL: ConvertToSID: masterKeyLen=%d cipherArgLen=%d "
		    "time=%d addr=0x%08x%08x%08x%08x cipherType=%d",
		    myPid, to->u.ssl2.masterKey.len,
		    to->u.ssl2.cipherArg.len, to->creationTime,
		    to->addr.pr_s6_addr32[0], to->addr.pr_s6_addr32[1],
		    to->addr.pr_s6_addr32[2], to->addr.pr_s6_addr32[3],
		    to->u.ssl2.cipherType));
    } else {
	/* This is an SSL v3 session */

	to->u.ssl3.sessionIDLength  = from->sessionIDLength;
	to->u.ssl3.cipherSuite      = from->u.ssl3.cipherSuite;
	to->u.ssl3.compression      = (SSL3CompressionMethod)from->u.ssl3.compression;
	to->u.ssl3.resumable        = from->u.ssl3.resumable;
	to->u.ssl3.hasFortezza      = from->u.ssl3.hasFortezza;
	to->u.ssl3.keys             = from->u.ssl3.keys;
	to->u.ssl3.masterWrapMech   = from->u.ssl3.masterWrapMech;
	to->u.ssl3.exchKeyType      = from->u.ssl3.exchKeyType;

	PORT_Memcpy(to->u.ssl3.sessionID, from->sessionID, from->sessionIDLength);

	/* the portions of the SID that are only restored on the client
	 * are set to invalid values on the server.
	 */
	to->u.ssl3.clientWriteKey   = NULL;
	to->u.ssl3.serverWriteKey   = NULL;
	to->u.ssl3.tek              = NULL;
	to->urlSvrName              = NULL;

	to->u.ssl3.masterModuleID   = (SECMODModuleID)-1; /* invalid value */
	to->u.ssl3.masterSlotID     = (CK_SLOT_ID)-1;     /* invalid value */
	to->u.ssl3.masterWrapIndex  = 0;
	to->u.ssl3.masterWrapSeries = 0;
	to->u.ssl3.masterValid      = PR_FALSE;

	to->u.ssl3.clAuthModuleID   = (SECMODModuleID)-1; /* invalid value */
	to->u.ssl3.clAuthSlotID     = (CK_SLOT_ID)-1;     /* invalid value */
	to->u.ssl3.clAuthSeries     = 0;
	to->u.ssl3.clAuthValid      = PR_FALSE;

	to->u.ssl3.clientWriteSaveLen = 0;

	if (from->u.ssl3.certIndex != -1 && pcce) {
	    SECItem          derCert;

	    derCert.len  = pcce->certLength;
	    derCert.data = pcce->cert;

	    to->peerCert = CERT_NewTempCertificate(dbHandle, &derCert, NULL,
					           PR_FALSE, PR_TRUE);
	    if (to->peerCert == NULL)
		goto loser;
	}
    }

    to->version         = from->version;
    to->creationTime    = from->creationTime;
    to->lastAccessTime  = from->lastAccessTime;
    to->expirationTime  = from->expirationTime;
    to->cached          = in_server_cache;
    to->addr            = from->addr;
    to->references      = 1;
    to->authAlgorithm	= from->authAlgorithm;
    to->authKeyBits	= from->authKeyBits;
    to->keaType		= from->keaType;
    to->keaKeyBits	= from->keaKeyBits;
    
    return to;

  loser:
    if (to) {
	if (version < SSL_LIBRARY_VERSION_3_0) {
	    if (to->u.ssl2.masterKey.data)
		PORT_Free(to->u.ssl2.masterKey.data);
	    if (to->u.ssl2.cipherArg.data)
		PORT_Free(to->u.ssl2.cipherArg.data);
	}
	PORT_Free(to);
    }
    return NULL;
}



/*
** Perform some mumbo jumbo on the ip-address and the session-id value to
** compute a hash value.
*/
static PRUint32 
SIDindex(cacheDesc *cache, const PRIPv6Addr *addr, PRUint8 *s, unsigned nl)
{
    PRUint32 rv;
    PRUint32 x[8];

    memset(x, 0, sizeof x);
    if (nl > sizeof x)
    	nl = sizeof x;
    memcpy(x, s, nl);

    rv = (addr->pr_s6_addr32[0] ^ addr->pr_s6_addr32[1] ^
	  addr->pr_s6_addr32[2] ^ addr->pr_s6_addr32[3] ^
          x[0] ^ x[1] ^ x[2] ^ x[3] ^ x[4] ^ x[5] ^ x[6] ^ x[7])
	  % cache->numSIDCacheSets;
    return rv;
}



/*
** Look something up in the cache. This will invalidate old entries
** in the process. Caller has locked the cache set!
** Returns PR_TRUE if found a valid match.  PR_FALSE otherwise.
*/
static sidCacheEntry *
FindSID(cacheDesc *cache, PRUint32 setNum, PRUint32 now,
        const PRIPv6Addr *addr, unsigned char *sessionID,
	unsigned sessionIDLength)
{
    PRUint32      ndx   = cache->sidCacheSets[setNum].next;
    int           i;

    sidCacheEntry * set = cache->sidCacheData + 
    			 (setNum * SID_CACHE_ENTRIES_PER_SET);

    for (i = SID_CACHE_ENTRIES_PER_SET; i > 0; --i) {
	sidCacheEntry * sce;

	ndx  = (ndx - 1) % SID_CACHE_ENTRIES_PER_SET;
	sce = set + ndx;

	if (!sce->valid)
	    continue;

	if (now > sce->expirationTime) {
	    /* SessionID has timed out. Invalidate the entry. */
	    SSL_TRC(7, ("%d: timed out sid entry addr=%08x%08x%08x%08x now=%x "
			"time+=%x",
			myPid, sce->addr.pr_s6_addr32[0],
			sce->addr.pr_s6_addr32[1], sce->addr.pr_s6_addr32[2],
			sce->addr.pr_s6_addr32[3], now,
			sce->expirationTime ));
	    sce->valid = 0;
	    continue;
	}

	/*
	** Next, examine specific session-id/addr data to see if the cache
	** entry matches our addr+session-id value
	*/
	if (sessionIDLength == sce->sessionIDLength      &&
	    !memcmp(&sce->addr, addr, sizeof(PRIPv6Addr)) &&
	    !memcmp(sce->sessionID, sessionID, sessionIDLength)) {
	    /* Found it */
	    return sce;
	}
    }

    PORT_SetError(SSL_ERROR_SESSION_NOT_FOUND);
    return NULL;
}

/************************************************************************/

/* This is the primary function for finding entries in the server's sid cache.
 * Although it is static, this function is called via the global function 
 * pointer ssl_sid_lookup.
 */
static sslSessionID *
ServerSessionIDLookup(const PRIPv6Addr *addr,
			unsigned char *sessionID,
			unsigned int   sessionIDLength,
                        CERTCertDBHandle * dbHandle)
{
    sslSessionID *  sid      = 0;
    sidCacheEntry * psce;
    certCacheEntry *pcce     = 0;
    cacheDesc *     cache    = &globalCache;
    PRUint32        now;
    PRUint32        set;
    PRInt32         cndx;
    sidCacheEntry   sce;
    certCacheEntry  cce;

    set = SIDindex(cache, addr, sessionID, sessionIDLength);
    now = LockSet(cache, set, 0);
    if (!now)
    	return NULL;

    psce = FindSID(cache, set, now, addr, sessionID, sessionIDLength);
    if (psce) {
	if (psce->version >= SSL_LIBRARY_VERSION_3_0 && 
	    (cndx = psce->u.ssl3.certIndex) != -1) {

	    PRUint32 gotLock = LockSidCacheLock(cache->certCacheLock, now);
	    if (gotLock) {
		pcce = &cache->certCacheData[cndx];

		/* See if the cert's session ID matches the sce cache. */
		if ((pcce->sessionIDLength == psce->sessionIDLength) &&
		    !PORT_Memcmp(pcce->sessionID, psce->sessionID, 
		                 pcce->sessionIDLength)) {
		    cce = *pcce;
		} else {
		    /* The cert doesen't match the SID cache entry, 
		    ** so invalidate the SID cache entry. 
		    */
		    psce->valid = 0;
		    psce = 0;
		    pcce = 0;
		}
		UnlockSidCacheLock(cache->certCacheLock);
	    } else {
		/* what the ??.  Didn't get the cert cache lock.
		** Don't invalidate the SID cache entry, but don't find it.
		*/
		PORT_Assert(!("Didn't get cert Cache Lock!"));
		psce = 0;
		pcce = 0;
	    }
	}
	if (psce) {
	    psce->lastAccessTime = now;
	    sce = *psce;	/* grab a copy while holding the lock */
    	}
    }
    UnlockSet(cache, set);
    if (psce) {
	/* sce conains a copy of the cache entry.
	** Convert shared memory format to local format 
	*/
	sid = ConvertToSID(&sce, pcce ? &cce : 0, dbHandle);
    }
    return sid;
}

/*
** Place a sid into the cache, if it isn't already there. 
*/
static void 
ServerSessionIDCache(sslSessionID *sid)
{
    sidCacheEntry sce;
    PRUint32      now     = 0;
    uint16        version = sid->version;
    cacheDesc *   cache   = &globalCache;

    if ((version >= SSL_LIBRARY_VERSION_3_0) &&
	(sid->u.ssl3.sessionIDLength == 0)) {
	return;
    }

    if (sid->cached == never_cached || sid->cached == invalid_cache) {
	PRUint32 set;

	PORT_Assert(sid->creationTime != 0 && sid->expirationTime != 0);
	if (!sid->creationTime)
	    sid->lastAccessTime = sid->creationTime = ssl_Time();
	if (version < SSL_LIBRARY_VERSION_3_0) {
	    if (!sid->expirationTime)
		sid->expirationTime = sid->creationTime + ssl_sid_timeout;
	    SSL_TRC(8, ("%d: SSL: CacheMT: cached=%d addr=0x%08x%08x%08x%08x time=%x "
			"cipher=%d", myPid, sid->cached,
			sid->addr.pr_s6_addr32[0], sid->addr.pr_s6_addr32[1],
			sid->addr.pr_s6_addr32[2], sid->addr.pr_s6_addr32[3],
			sid->creationTime, sid->u.ssl2.cipherType));
	    PRINT_BUF(8, (0, "sessionID:", sid->u.ssl2.sessionID,
			  SSL2_SESSIONID_BYTES));
	    PRINT_BUF(8, (0, "masterKey:", sid->u.ssl2.masterKey.data,
			  sid->u.ssl2.masterKey.len));
	    PRINT_BUF(8, (0, "cipherArg:", sid->u.ssl2.cipherArg.data,
			  sid->u.ssl2.cipherArg.len));

	} else {
	    if (!sid->expirationTime)
		sid->expirationTime = sid->creationTime + ssl3_sid_timeout;
	    SSL_TRC(8, ("%d: SSL: CacheMT: cached=%d addr=0x%08x%08x%08x%08x time=%x "
			"cipherSuite=%d", myPid, sid->cached,
			sid->addr.pr_s6_addr32[0], sid->addr.pr_s6_addr32[1],
			sid->addr.pr_s6_addr32[2], sid->addr.pr_s6_addr32[3],
			sid->creationTime, sid->u.ssl3.cipherSuite));
	    PRINT_BUF(8, (0, "sessionID:", sid->u.ssl3.sessionID,
			  sid->u.ssl3.sessionIDLength));
	}

	ConvertFromSID(&sce, sid);

	if ((version >= SSL_LIBRARY_VERSION_3_0) && 
	    (sid->peerCert != NULL)) {
	    now = CacheCert(cache, sid->peerCert, &sce);
	}

	set = SIDindex(cache, &sce.addr, sce.sessionID, sce.sessionIDLength);
	now = LockSet(cache, set, now);
	if (now) {
	    PRUint32  next = cache->sidCacheSets[set].next;
	    PRUint32  ndx  = set * SID_CACHE_ENTRIES_PER_SET + next;

	    /* Write out new cache entry */
	    cache->sidCacheData[ndx] = sce;

	    cache->sidCacheSets[set].next = 
	    				(next + 1) % SID_CACHE_ENTRIES_PER_SET;

	    UnlockSet(cache, set);
	    sid->cached = in_server_cache;
	}
    }
}

/*
** Although this is static, it is called from ssl via global function pointer
**	ssl_sid_uncache.  This invalidates the referenced cache entry.
*/
static void 
ServerSessionIDUncache(sslSessionID *sid)
{
    cacheDesc *    cache   = &globalCache;
    PRUint8 *      sessionID;
    unsigned int   sessionIDLength;
    PRErrorCode    err;
    PRUint32       set;
    PRUint32       now;
    sidCacheEntry *psce;

    if (sid == NULL) 
    	return;
    
    /* Uncaching a SID should never change the error code. 
    ** So save it here and restore it before exiting.
    */
    err = PR_GetError();

    if (sid->version < SSL_LIBRARY_VERSION_3_0) {
	sessionID       = sid->u.ssl2.sessionID;
	sessionIDLength = SSL2_SESSIONID_BYTES;
	SSL_TRC(8, ("%d: SSL: UncacheMT: valid=%d addr=0x%08x%08x%08x%08x time=%x "
		    "cipher=%d", myPid, sid->cached,
		    sid->addr.pr_s6_addr32[0], sid->addr.pr_s6_addr32[1],
		    sid->addr.pr_s6_addr32[2], sid->addr.pr_s6_addr32[3],
		    sid->creationTime, sid->u.ssl2.cipherType));
	PRINT_BUF(8, (0, "sessionID:", sessionID, sessionIDLength));
	PRINT_BUF(8, (0, "masterKey:", sid->u.ssl2.masterKey.data,
		      sid->u.ssl2.masterKey.len));
	PRINT_BUF(8, (0, "cipherArg:", sid->u.ssl2.cipherArg.data,
		      sid->u.ssl2.cipherArg.len));
    } else {
	sessionID       = sid->u.ssl3.sessionID;
	sessionIDLength = sid->u.ssl3.sessionIDLength;
	SSL_TRC(8, ("%d: SSL3: UncacheMT: valid=%d addr=0x%08x%08x%08x%08x time=%x "
		    "cipherSuite=%d", myPid, sid->cached,
		    sid->addr.pr_s6_addr32[0], sid->addr.pr_s6_addr32[1],
		    sid->addr.pr_s6_addr32[2], sid->addr.pr_s6_addr32[3],
		    sid->creationTime, sid->u.ssl3.cipherSuite));
	PRINT_BUF(8, (0, "sessionID:", sessionID, sessionIDLength));
    }
    set = SIDindex(cache, &sid->addr, sessionID, sessionIDLength);
    now = LockSet(cache, set, 0);
    if (now) {
	psce = FindSID(cache, set, now, &sid->addr, sessionID, sessionIDLength);
	if (psce) {
	    psce->valid = 0;
	}
	UnlockSet(cache, set);
    }
    sid->cached = invalid_cache;
    PORT_SetError(err);
}

#ifdef XP_OS2

#define INCL_DOSPROCESS
#include <os2.h>

long gettid(void)
{
    PTIB ptib;
    PPIB ppib;
    DosGetInfoBlocks(&ptib, &ppib);
    return ((long)ptib->tib_ordinal); /* thread id */
}
#endif

static void
CloseCache(cacheDesc *cache)
{
    int locks_initialized = cache->numSIDCacheLocksInitialized;

    if (cache->cacheMem) {
	/* If everInherited is true, this shared cache was (and may still
	** be) in use by multiple processes.  We do not wish to destroy
	** the mutexes while they are still in use.  
	*/
	if (PR_FALSE == cache->sharedCache->everInherited) {
	    sidCacheLock *pLock = cache->sidCacheLocks;
	    for (; locks_initialized > 0; --locks_initialized, ++pLock ) {
		sslMutex_Destroy(&pLock->mutex);
	    }
	}
	if (cache->shared) {
	    PR_MemUnmap(cache->cacheMem, cache->cacheMemSize);
	} else {
	    PORT_Free(cache->cacheMem);
	}
	cache->cacheMem = NULL;
    }
    if (cache->cacheMemMap) {
	PR_CloseFileMap(cache->cacheMemMap);
	cache->cacheMemMap = NULL;
    }
    memset(cache, 0, sizeof *cache);
}

static SECStatus
InitCache(cacheDesc *cache, int maxCacheEntries, PRUint32 ssl2_timeout, 
          PRUint32 ssl3_timeout, const char *directory, PRBool shared)
{
    ptrdiff_t     ptr;
    sidCacheLock *pLock;
    char *        cacheMem;
    PRFileMap *   cacheMemMap;
    char *        cfn = NULL;	/* cache file name */
    int           locks_initialized = 0;
    int           locks_to_initialize = 0;
    PRUint32      init_time;

    if (cache->cacheMem) {
	/* Already done */
	return SECSuccess;
    }

    /* make sure loser can clean up properly */
    cache->shared = shared;
    cache->cacheMem    = cacheMem    = NULL;
    cache->cacheMemMap = cacheMemMap = NULL;
    cache->sharedCache = (cacheDesc *)0;

    cache->numSIDCacheEntries = maxCacheEntries ? maxCacheEntries 
                                                : DEF_SID_CACHE_ENTRIES;
    cache->numSIDCacheSets    = 
    	SID_HOWMANY(cache->numSIDCacheEntries, SID_CACHE_ENTRIES_PER_SET);

    cache->numSIDCacheEntries = 
    	cache->numSIDCacheSets * SID_CACHE_ENTRIES_PER_SET;

    cache->numSIDCacheLocks   = 
    	PR_MIN(cache->numSIDCacheSets, ssl_max_sid_cache_locks);

    cache->numSIDCacheSetsPerLock = 
    	SID_HOWMANY(cache->numSIDCacheSets, cache->numSIDCacheLocks);

    /* compute size of shared memory, and offsets of all pointers */
    ptr = 0;
    cache->cacheMem     = (char *)ptr;
    ptr += SID_ROUNDUP(sizeof(cacheDesc), SID_ALIGNMENT);

    cache->sidCacheLocks = (sidCacheLock *)ptr;
    cache->keyCacheLock  = cache->sidCacheLocks + cache->numSIDCacheLocks;
    cache->certCacheLock = cache->keyCacheLock  + 1;
    ptr = (ptrdiff_t)(cache->certCacheLock + 1);
    ptr = SID_ROUNDUP(ptr, SID_ALIGNMENT);

    cache->sidCacheSets  = (sidCacheSet *)ptr;
    ptr = (ptrdiff_t)(cache->sidCacheSets + cache->numSIDCacheSets);
    ptr = SID_ROUNDUP(ptr, SID_ALIGNMENT);

    cache->sidCacheData  = (sidCacheEntry *)ptr;
    ptr = (ptrdiff_t)(cache->sidCacheData + cache->numSIDCacheEntries);
    ptr = SID_ROUNDUP(ptr, SID_ALIGNMENT);

    cache->certCacheData = (certCacheEntry *)ptr;
    cache->sidCacheSize  = 
    	(char *)cache->certCacheData - (char *)cache->sidCacheData;

    /* This is really a poor way to computer this! */
    cache->numCertCacheEntries = cache->sidCacheSize / sizeof(certCacheEntry);
    if (cache->numCertCacheEntries < MIN_CERT_CACHE_ENTRIES)
    	cache->numCertCacheEntries = MIN_CERT_CACHE_ENTRIES;
    ptr = (ptrdiff_t)(cache->certCacheData + cache->numCertCacheEntries);
    ptr = SID_ROUNDUP(ptr, SID_ALIGNMENT);

    cache->keyCacheData  = (SSLWrappedSymWrappingKey *)ptr;
    cache->certCacheSize = 
    	(char *)cache->keyCacheData - (char *)cache->certCacheData;

    cache->numKeyCacheEntries = kt_kea_size * SSL_NUM_WRAP_MECHS;
    ptr = (ptrdiff_t)(cache->keyCacheData + cache->numKeyCacheEntries);
    ptr = SID_ROUNDUP(ptr, SID_ALIGNMENT);

    cache->cacheMemSize = ptr;

    cache->keyCacheSize  = (char *)ptr - (char *)cache->keyCacheData;

    if (ssl2_timeout) {   
	if (ssl2_timeout > MAX_SSL2_TIMEOUT) {
	    ssl2_timeout = MAX_SSL2_TIMEOUT;
	}
	if (ssl2_timeout < MIN_SSL2_TIMEOUT) {
	    ssl2_timeout = MIN_SSL2_TIMEOUT;
	}
	cache->ssl2Timeout = ssl2_timeout;
    } else {
	cache->ssl2Timeout = DEF_SSL2_TIMEOUT;
    }

    if (ssl3_timeout) {   
	if (ssl3_timeout > MAX_SSL3_TIMEOUT) {
	    ssl3_timeout = MAX_SSL3_TIMEOUT;
	}
	if (ssl3_timeout < MIN_SSL3_TIMEOUT) {
	    ssl3_timeout = MIN_SSL3_TIMEOUT;
	}
	cache->ssl3Timeout = ssl3_timeout;
    } else {
	cache->ssl3Timeout = DEF_SSL3_TIMEOUT;
    }

    if (shared) {
	/* Create file names */
#if defined(XP_UNIX) || defined(XP_BEOS)
	/* there's some confusion here about whether PR_OpenAnonFileMap wants
	** a directory name or a file name for its first argument.
	cfn = PR_smprintf("%s/.sslsvrcache.%d", directory, myPid);
	*/
	cfn = PR_smprintf("%s", directory);
#elif defined(XP_WIN32)
	cfn = PR_smprintf("%s/svrcache_%d_%x.ssl", directory, myPid, 
			    GetCurrentThreadId());
#elif defined(XP_OS2)
	cfn = PR_smprintf("%s/svrcache_%d_%x.ssl", directory, myPid, 
			    gettid());
#else
#error "Don't know how to create file name for this platform!"
#endif
	if (!cfn) {
	    goto loser;
	}

	/* Create cache */
	cacheMemMap = PR_OpenAnonFileMap(cfn, cache->cacheMemSize, 
					 PR_PROT_READWRITE);

	PR_smprintf_free(cfn);
	if(!cacheMemMap) {
	    goto loser;
	}

        cacheMem = PR_MemMap(cacheMemMap, 0, cache->cacheMemSize);
    } else {
        cacheMem = PORT_Alloc(cache->cacheMemSize);
    }
    
    if (! cacheMem) {
        goto loser;
    }

    /* Initialize shared memory. This may not be necessary on all platforms */
    memset(cacheMem, 0, cache->cacheMemSize);

    /* Copy cache descriptor header into shared memory */
    memcpy(cacheMem, cache, sizeof *cache);

    /* save private copies of these values */
    cache->cacheMemMap = cacheMemMap;
    cache->cacheMem    = cacheMem;
    cache->sharedCache = (cacheDesc *)cacheMem;

    /* Fix pointers in our private copy of cache descriptor to point to 
    ** spaces in shared memory 
    */
    ptr = (ptrdiff_t)cache->cacheMem;
    *(ptrdiff_t *)(&cache->sidCacheLocks) += ptr;
    *(ptrdiff_t *)(&cache->keyCacheLock ) += ptr;
    *(ptrdiff_t *)(&cache->certCacheLock) += ptr;
    *(ptrdiff_t *)(&cache->sidCacheSets ) += ptr;
    *(ptrdiff_t *)(&cache->sidCacheData ) += ptr;
    *(ptrdiff_t *)(&cache->certCacheData) += ptr;
    *(ptrdiff_t *)(&cache->keyCacheData ) += ptr;

    /* initialize the locks */
    init_time = ssl_Time();
    pLock = cache->sidCacheLocks;
    for (locks_to_initialize = cache->numSIDCacheLocks + 2;
         locks_initialized < locks_to_initialize; 
	 ++locks_initialized, ++pLock ) {

	SECStatus err = sslMutex_Init(&pLock->mutex, shared);
	if (err) {
	    cache->numSIDCacheLocksInitialized = locks_initialized;
	    goto loser;
	}
        pLock->timeStamp = init_time;
	pLock->pid       = 0;
    }
    cache->numSIDCacheLocksInitialized = locks_initialized;

    return SECSuccess;

loser:
    CloseCache(cache);
    return SECFailure;
}

PRUint32
SSL_GetMaxServerCacheLocks(void)
{
    return ssl_max_sid_cache_locks + 2;
    /* The extra two are the cert cache lock and the key cache lock. */
}

SECStatus
SSL_SetMaxServerCacheLocks(PRUint32 maxLocks)
{
    /* Minimum is 1 sid cache lock, 1 cert cache lock and 1 key cache lock.
    ** We'd like to test for a maximum value, but not all platforms' header
    ** files provide a symbol or function or other means of determining
    ** the maximum, other than trial and error.
    */
    if (maxLocks < 3) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    ssl_max_sid_cache_locks = maxLocks - 2;
    /* The extra two are the cert cache lock and the key cache lock. */
    return SECSuccess;
}

SECStatus
SSL_ConfigServerSessionIDCacheInstance(	cacheDesc *cache,
                                int      maxCacheEntries, 
				PRUint32 ssl2_timeout,
			       	PRUint32 ssl3_timeout, 
			  const char *   directory, PRBool shared)
{
    SECStatus rv;

#if defined(DEBUG_nelsonb)
    printf("sizeof(sidCacheEntry) == %u\n", sizeof(sidCacheEntry));
#endif
#if !(defined(SOLARIS) && defined(i386))
#ifndef XP_OS2
    PORT_Assert(sizeof(sidCacheEntry) % 8 == 0);
#endif
#endif
    PORT_Assert(sizeof(certCacheEntry) == 4096);

    myPid = SSL_GETPID();
    if (!directory) {
	directory = DEFAULT_CACHE_DIRECTORY;
    }
    rv = InitCache(cache, maxCacheEntries, ssl2_timeout, ssl3_timeout, 
                   directory, shared);
    if (rv) {
	SET_ERROR_CODE
    	return SECFailure;
    }

    ssl_sid_lookup  = ServerSessionIDLookup;
    ssl_sid_cache   = ServerSessionIDCache;
    ssl_sid_uncache = ServerSessionIDUncache;
    return SECSuccess;
}

SECStatus
SSL_ConfigServerSessionIDCache(	int      maxCacheEntries, 
				PRUint32 ssl2_timeout,
			       	PRUint32 ssl3_timeout, 
			  const char *   directory)
{
    ssl_InitClientSessionCacheLock();
    ssl_InitSymWrapKeysLock();
    return SSL_ConfigServerSessionIDCacheInstance(&globalCache, 
    		maxCacheEntries, ssl2_timeout, ssl3_timeout, directory, PR_FALSE);
}

SECStatus
SSL_ShutdownServerSessionIDCacheInstance(cacheDesc *cache)
{
    CloseCache(cache);
    return SECSuccess;
}

SECStatus
SSL_ShutdownServerSessionIDCache(void)
{
    SSL3_ShutdownServerCache();
    return SSL_ShutdownServerSessionIDCacheInstance(&globalCache);
}

/* Use this function, instead of SSL_ConfigServerSessionIDCache,
 * if the cache will be shared by multiple processes.
 */
SECStatus
SSL_ConfigMPServerSIDCache(	int      maxCacheEntries, 
				PRUint32 ssl2_timeout,
			       	PRUint32 ssl3_timeout, 
		          const char *   directory)
{
    char *	envValue;
    char *	inhValue;
    cacheDesc * cache         = &globalCache;
    PRUint32    fmStrLen;
    SECStatus 	result;
    PRStatus 	prStatus;
    SECStatus	putEnvFailed;
    inheritance inherit;
    char        fmString[PR_FILEMAP_STRING_BUFSIZE];

    isMultiProcess = PR_TRUE;
    result = SSL_ConfigServerSessionIDCacheInstance(cache, maxCacheEntries, 
			ssl2_timeout, ssl3_timeout, directory, PR_TRUE);
    if (result != SECSuccess) 
        return result;

    prStatus = PR_ExportFileMapAsString(cache->cacheMemMap, 
                                        sizeof fmString, fmString);
    if ((prStatus != PR_SUCCESS) || !(fmStrLen = strlen(fmString))) {
	SET_ERROR_CODE
	return SECFailure;
    }

    inherit.cacheMemSize	= cache->cacheMemSize;
    inherit.fmStrLen            = fmStrLen;

    inhValue = BTOA_DataToAscii((unsigned char *)&inherit, sizeof inherit);
    if (!inhValue || !strlen(inhValue)) {
	SET_ERROR_CODE
	return SECFailure;
    }
    envValue = PR_smprintf("%s,%s", inhValue, fmString);
    if (!envValue || !strlen(envValue)) {
	SET_ERROR_CODE
	return SECFailure;
    }
    PORT_Free(inhValue);

    putEnvFailed = (SECStatus)NSS_PutEnv(envVarName, envValue);
    PR_smprintf_free(envValue);
    if (putEnvFailed) {
        SET_ERROR_CODE
        result = SECFailure;
    }

#if defined(XP_UNIX) || defined(XP_BEOS)
    /* Launch thread to poll cache for expired locks on Unix */
    LaunchLockPoller(cache);
#endif
    return result;
}

SECStatus
SSL_InheritMPServerSIDCacheInstance(cacheDesc *cache, const char * envString)
{
    unsigned char * decoString = NULL;
    char *          fmString   = NULL;
    char *          myEnvString = NULL;
    unsigned int    decoLen;
    ptrdiff_t       ptr;
    inheritance     inherit;
    cacheDesc       my;
#ifdef WINNT
    sidCacheLock* newLocks;
    int           locks_initialized = 0;
    int           locks_to_initialize = 0;
#endif

    myPid = SSL_GETPID();

    /* If this child was created by fork(), and not by exec() on unix,
    ** then isMultiProcess will already be set.
    ** If not, we'll set it below.
    */
    if (isMultiProcess) {
	if (cache && cache->sharedCache) {
	    cache->sharedCache->everInherited = PR_TRUE;
	}
    	return SECSuccess;	/* already done. */
    }

    ssl_InitClientSessionCacheLock();
    ssl_InitSymWrapKeysLock();

    ssl_sid_lookup  = ServerSessionIDLookup;
    ssl_sid_cache   = ServerSessionIDCache;
    ssl_sid_uncache = ServerSessionIDUncache;

    if (!envString) {
    	envString  = getenv(envVarName);
	if (!envString) {
	    SET_ERROR_CODE
	    return SECFailure;
	}
    }
    myEnvString = PORT_Strdup(envString);
    if (!myEnvString) 
	return SECFailure;
    fmString = strchr(myEnvString, ',');
    if (!fmString) 
    	goto loser;
    *fmString++ = 0;

    decoString = ATOB_AsciiToData(myEnvString, &decoLen);
    if (!decoString) {
    	SET_ERROR_CODE
	goto loser;
    }
    if (decoLen != sizeof inherit) {
    	SET_ERROR_CODE
    	goto loser;
    }

    PORT_Memcpy(&inherit, decoString, sizeof inherit);

    if (strlen(fmString)  != inherit.fmStrLen ) {
    	goto loser;
    }

    memset(cache, 0, sizeof *cache);
    cache->cacheMemSize	= inherit.cacheMemSize;

    /* Create cache */
    cache->cacheMemMap = PR_ImportFileMapFromString(fmString);
    if(! cache->cacheMemMap) {
	goto loser;
    }
    cache->cacheMem = PR_MemMap(cache->cacheMemMap, 0, cache->cacheMemSize);
    if (! cache->cacheMem) {
	goto loser;
    }
    cache->sharedCache   = (cacheDesc *)cache->cacheMem;

    if (cache->sharedCache->cacheMemSize != cache->cacheMemSize) {
	SET_ERROR_CODE
    	goto loser;
    }

    /* We're now going to overwrite the local cache instance with the 
    ** shared copy of the cache struct, then update several values in 
    ** the local cache using the values for cache->cacheMemMap and 
    ** cache->cacheMem computed just above.  So, we copy cache into 
    ** the automatic variable "my", to preserve the variables while
    ** cache is overwritten.
    */
    my = *cache;  /* save values computed above. */
    memcpy(cache, cache->sharedCache, sizeof *cache); /* overwrite */

    /* Fix pointers in our private copy of cache descriptor to point to 
    ** spaces in shared memory, whose address is now in "my".
    */
    ptr = (ptrdiff_t)my.cacheMem;
    *(ptrdiff_t *)(&cache->sidCacheLocks) += ptr;
    *(ptrdiff_t *)(&cache->keyCacheLock ) += ptr;
    *(ptrdiff_t *)(&cache->certCacheLock) += ptr;
    *(ptrdiff_t *)(&cache->sidCacheSets ) += ptr;
    *(ptrdiff_t *)(&cache->sidCacheData ) += ptr;
    *(ptrdiff_t *)(&cache->certCacheData) += ptr;
    *(ptrdiff_t *)(&cache->keyCacheData ) += ptr;

    cache->cacheMemMap = my.cacheMemMap;
    cache->cacheMem    = my.cacheMem;
    cache->sharedCache = (cacheDesc *)cache->cacheMem;

#ifdef WINNT
    /*  On Windows NT we need to "fix" the sidCacheLocks here to support fibers
    **  When NT fibers are used in a multi-process server, a second level of
    **  locking is needed to prevent a deadlock, in case a fiber acquires the
    **  cross-process mutex, yields, and another fiber is later scheduled on
    **  the same native thread and tries to acquire the cross-process mutex.
    **  We do this by using a PRLock in the sslMutex. However, it is stored in
    **  shared memory as part of sidCacheLocks, and we don't want to overwrite
    **  the PRLock of the parent process. So we need to make new, private
    **  copies of sidCacheLocks before modifying the sslMutex with our own
    **  PRLock
    */
    
    /* note from jpierre : this should be free'd in child processes when
    ** a function is added to delete the SSL session cache in the future. 
    */
    locks_to_initialize = cache->numSIDCacheLocks + 2;
    newLocks = PORT_NewArray(sidCacheLock, locks_to_initialize);
    if (!newLocks)
    	goto loser;
    /* copy the old locks */
    memcpy(newLocks, cache->sidCacheLocks, 
           locks_to_initialize * sizeof(sidCacheLock));
    cache->sidCacheLocks = newLocks;
    /* fix the locks */		
    for (; locks_initialized < locks_to_initialize; ++locks_initialized) {
        /* now, make a local PRLock in this sslMutex for this child process */
	SECStatus err;
        err = sslMutex_2LevelInit(&newLocks[locks_initialized].mutex);
	if (err != SECSuccess) {
	    cache->numSIDCacheLocksInitialized = locks_initialized;
	    goto loser;
    	}
    }
    cache->numSIDCacheLocksInitialized = locks_initialized;

    /* also fix the key and cert cache which use the last 2 lock entries */
    cache->keyCacheLock  = cache->sidCacheLocks + cache->numSIDCacheLocks;
    cache->certCacheLock = cache->keyCacheLock  + 1;
#endif

    PORT_Free(myEnvString);
    PORT_Free(decoString);

    /* mark that we have inherited this. */
    cache->sharedCache->everInherited = PR_TRUE;
    isMultiProcess = PR_TRUE;

    return SECSuccess;

loser:
    PORT_Free(myEnvString);
    if (decoString) 
	PORT_Free(decoString);
    CloseCache(cache);
    return SECFailure;
}

SECStatus
SSL_InheritMPServerSIDCache(const char * envString)
{
    return SSL_InheritMPServerSIDCacheInstance(&globalCache, envString);
}

#if defined(XP_UNIX) || defined(XP_BEOS)

#define SID_LOCK_EXPIRATION_TIMEOUT  30 /* seconds */

static void
LockPoller(void * arg)
{
    cacheDesc *    cache         = (cacheDesc *)arg;
    cacheDesc *    sharedCache   = cache->sharedCache;
    sidCacheLock * pLock;
    const char *   timeoutString;
    PRIntervalTime timeout;
    PRUint32       now;
    PRUint32       then;
    int            locks_polled  = 0;
    int            locks_to_poll = cache->numSIDCacheLocks + 2;
    PRUint32       expiration    = SID_LOCK_EXPIRATION_TIMEOUT;

    timeoutString = getenv("NSS_SSL_SERVER_CACHE_MUTEX_TIMEOUT");
    if (timeoutString) {
	long newTime = strtol(timeoutString, 0, 0);
	if (newTime == 0) 
	    return;  /* application doesn't want this function */
	if (newTime > 0)
	    expiration = (PRUint32)newTime;
	/* if error (newTime < 0) ignore it and use default */
    }

    timeout = PR_SecondsToInterval(expiration);
    while(!sharedCache->stopPolling) {
    	PR_Sleep(timeout);
	if (sharedCache->stopPolling)
	    break;

	now   = ssl_Time();
	then  = now - expiration;
	for (pLock = cache->sidCacheLocks, locks_polled = 0;
	     locks_to_poll > locks_polled && !sharedCache->stopPolling; 
	     ++locks_polled, ++pLock ) {
	    pid_t pid;

	    if (pLock->timeStamp   < then && 
	        pLock->timeStamp   != 0 && 
		(pid = pLock->pid) != 0) {

	    	/* maybe we should try the lock? */
		int result = kill(pid, 0);
		if (result < 0 && errno == ESRCH) {
		    SECStatus rv;
		    /* No process exists by that pid any more.
		    ** Treat this mutex as abandoned.
		    */
		    pLock->timeStamp = now;
		    pLock->pid       = 0;
		    rv = sslMutex_Unlock(&pLock->mutex);
		    if (rv != SECSuccess) {
		    	/* Now what? */
		    }
		}
	    }
	} /* end of loop over locks */
    } /* end of entire polling loop */
}

/* Launch thread to poll cache for expired locks */
static SECStatus 
LaunchLockPoller(cacheDesc *cache)
{
    PRThread * pollerThread;

    pollerThread = 
	PR_CreateThread(PR_USER_THREAD, LockPoller, cache, PR_PRIORITY_NORMAL, 
	                PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
    if (!pollerThread) {
    	return SECFailure;
    }
    cache->poller = pollerThread;
    return SECSuccess;
}
#endif

/************************************************************************
 *  Code dealing with shared wrapped symmetric wrapping keys below      *
 ************************************************************************/

/* If now is zero, it implies that the lock is not held, and must be 
** aquired here.  
*/
static PRBool
getSvrWrappingKey(PRInt32                symWrapMechIndex,
               SSL3KEAType               exchKeyType, 
               SSLWrappedSymWrappingKey *wswk, 
	       cacheDesc *               cache,
	       PRUint32                  lockTime)
{
    PRUint32  ndx = (exchKeyType * SSL_NUM_WRAP_MECHS) + symWrapMechIndex;
    SSLWrappedSymWrappingKey * pwswk = cache->keyCacheData + ndx;
    PRUint32  now = 0;
    PRBool    rv  = PR_FALSE;

    if (!lockTime) {
	lockTime = now = LockSidCacheLock(cache->keyCacheLock, now);
	if (!lockTime) {
	    return rv;
	}
    }
    if (pwswk->exchKeyType      == exchKeyType && 
	pwswk->symWrapMechIndex == symWrapMechIndex &&
	pwswk->wrappedSymKeyLen != 0) {
	*wswk = *pwswk;
	rv = PR_TRUE;
    }
    if (now) {
	UnlockSidCacheLock(cache->keyCacheLock);
    }
    return rv;
}

PRBool
ssl_GetWrappingKey( PRInt32                   symWrapMechIndex,
                    SSL3KEAType               exchKeyType, 
		    SSLWrappedSymWrappingKey *wswk)
{
    PRBool rv;

    PORT_Assert( (unsigned)exchKeyType < kt_kea_size);
    PORT_Assert( (unsigned)symWrapMechIndex < SSL_NUM_WRAP_MECHS);
    if ((unsigned)exchKeyType < kt_kea_size &&
        (unsigned)symWrapMechIndex < SSL_NUM_WRAP_MECHS) {
	rv = getSvrWrappingKey(symWrapMechIndex, exchKeyType, wswk, 
	                       &globalCache, 0);
    } else {
    	rv = PR_FALSE;
    }

    return rv;
}

/* The caller passes in the new value it wants
 * to set.  This code tests the wrapped sym key entry in the shared memory.
 * If it is uninitialized, this function writes the caller's value into 
 * the disk entry, and returns false.  
 * Otherwise, it overwrites the caller's wswk with the value obtained from 
 * the disk, and returns PR_TRUE.  
 * This is all done while holding the locks/mutexes necessary to make 
 * the operation atomic.
 */
PRBool
ssl_SetWrappingKey(SSLWrappedSymWrappingKey *wswk)
{
    cacheDesc *   cache            = &globalCache;
    PRBool        rv               = PR_FALSE;
    SSL3KEAType   exchKeyType      = wswk->exchKeyType;   
                                /* type of keys used to wrap SymWrapKey*/
    PRInt32       symWrapMechIndex = wswk->symWrapMechIndex;
    PRUint32      ndx;
    PRUint32      now = 0;
    SSLWrappedSymWrappingKey myWswk;

    PORT_Assert( (unsigned)exchKeyType < kt_kea_size);
    if ((unsigned)exchKeyType >= kt_kea_size)
    	return 0;

    PORT_Assert( (unsigned)symWrapMechIndex < SSL_NUM_WRAP_MECHS);
    if ((unsigned)symWrapMechIndex >=  SSL_NUM_WRAP_MECHS)
    	return 0;

    ndx = (exchKeyType * SSL_NUM_WRAP_MECHS) + symWrapMechIndex;
    PORT_Memset(&myWswk, 0, sizeof myWswk);	/* eliminate UMRs. */

    now = LockSidCacheLock(cache->keyCacheLock, now);
    if (now) {
	rv = getSvrWrappingKey(wswk->symWrapMechIndex, wswk->exchKeyType, 
	                       &myWswk, cache, now);
	if (rv) {
	    /* we found it on disk, copy it out to the caller. */
	    PORT_Memcpy(wswk, &myWswk, sizeof *wswk);
	} else {
	    /* Wasn't on disk, and we're still holding the lock, so write it. */
	    cache->keyCacheData[ndx] = *wswk;
	}
	UnlockSidCacheLock(cache->keyCacheLock);
    }
    return rv;
}

#else  /* MAC version or other platform */

#include "seccomon.h"
#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"

SECStatus
SSL_ConfigServerSessionIDCache(	int      maxCacheEntries, 
				PRUint32 ssl2_timeout,
			       	PRUint32 ssl3_timeout, 
			  const char *   directory)
{
    PR_ASSERT(!"SSL servers are not supported on this platform. (SSL_ConfigServerSessionIDCache)");    
    return SECFailure;
}

SECStatus
SSL_ConfigMPServerSIDCache(	int      maxCacheEntries, 
				PRUint32 ssl2_timeout,
			       	PRUint32 ssl3_timeout, 
		          const char *   directory)
{
    PR_ASSERT(!"SSL servers are not supported on this platform. (SSL_ConfigMPServerSIDCache)");    
    return SECFailure;
}

SECStatus
SSL_InheritMPServerSIDCache(const char * envString)
{
    PR_ASSERT(!"SSL servers are not supported on this platform. (SSL_InheritMPServerSIDCache)");    
    return SECFailure;
}

PRBool
ssl_GetWrappingKey( PRInt32                   symWrapMechIndex,
                    SSL3KEAType               exchKeyType, 
		    SSLWrappedSymWrappingKey *wswk)
{
    PRBool rv = PR_FALSE;
    PR_ASSERT(!"SSL servers are not supported on this platform. (ssl_GetWrappingKey)");    
    return rv;
}

/* This is a kind of test-and-set.  The caller passes in the new value it wants
 * to set.  This code tests the wrapped sym key entry in the shared memory.
 * If it is uninitialized, this function writes the caller's value into 
 * the disk entry, and returns false.  
 * Otherwise, it overwrites the caller's wswk with the value obtained from 
 * the disk, and returns PR_TRUE.  
 * This is all done while holding the locks/mutexes necessary to make 
 * the operation atomic.
 */
PRBool
ssl_SetWrappingKey(SSLWrappedSymWrappingKey *wswk)
{
    PRBool        rv = PR_FALSE;
    PR_ASSERT(!"SSL servers are not supported on this platform. (ssl_SetWrappingKey)");
    return rv;
}

PRUint32  
SSL_GetMaxServerCacheLocks(void)
{
    PR_ASSERT(!"SSL servers are not supported on this platform. (SSL_GetMaxServerCacheLocks)");
    return -1;
}

SECStatus 
SSL_SetMaxServerCacheLocks(PRUint32 maxLocks)
{
    PR_ASSERT(!"SSL servers are not supported on this platform. (SSL_SetMaxServerCacheLocks)");
    return SECFailure;
}

#endif /* XP_UNIX || XP_WIN32 */
