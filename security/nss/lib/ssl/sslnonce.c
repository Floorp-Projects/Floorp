/* 
 * This file implements the CLIENT Session ID cache.  
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: sslnonce.c,v 1.27 2012/04/25 14:50:12 gerv%gerv.net Exp $ */

#include "cert.h"
#include "pk11pub.h"
#include "secitem.h"
#include "ssl.h"
#include "nss.h"

#include "sslimpl.h"
#include "sslproto.h"
#include "nssilock.h"
#if (defined(XP_UNIX) || defined(XP_WIN) || defined(_WINDOWS) || defined(XP_BEOS)) && !defined(_WIN32_WCE)
#include <time.h>
#endif

PRUint32 ssl_sid_timeout = 100;
PRUint32 ssl3_sid_timeout = 86400L; /* 24 hours */

static sslSessionID *cache = NULL;
static PZLock *      cacheLock = NULL;

/* sids can be in one of 4 states:
 *
 * never_cached, 	created, but not yet put into cache. 
 * in_client_cache, 	in the client cache's linked list.
 * in_server_cache, 	entry came from the server's cache file.
 * invalid_cache	has been removed from the cache. 
 */

#define LOCK_CACHE 	lock_cache()
#define UNLOCK_CACHE	PZ_Unlock(cacheLock)

static SECStatus
ssl_InitClientSessionCacheLock(void)
{
    cacheLock = PZ_NewLock(nssILockCache);
    return cacheLock ? SECSuccess : SECFailure;
}

static SECStatus
ssl_FreeClientSessionCacheLock(void)
{
    if (cacheLock) {
        PZ_DestroyLock(cacheLock);
        cacheLock = NULL;
        return SECSuccess;
    }
    PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
    return SECFailure;
}

static PRBool LocksInitializedEarly = PR_FALSE;

static SECStatus
FreeSessionCacheLocks()
{
    SECStatus rv1, rv2;
    rv1 = ssl_FreeSymWrapKeysLock();
    rv2 = ssl_FreeClientSessionCacheLock();
    if ( (SECSuccess == rv1) && (SECSuccess == rv2) ) {
        return SECSuccess;
    }
    return SECFailure;
}

static SECStatus
InitSessionCacheLocks(void)
{
    SECStatus rv1, rv2;
    PRErrorCode rc;
    rv1 = ssl_InitSymWrapKeysLock();
    rv2 = ssl_InitClientSessionCacheLock();
    if ( (SECSuccess == rv1) && (SECSuccess == rv2) ) {
        return SECSuccess;
    }
    rc = PORT_GetError();
    FreeSessionCacheLocks();
    PORT_SetError(rc);
    return SECFailure;
}

/* free the session cache locks if they were initialized early */
SECStatus
ssl_FreeSessionCacheLocks()
{
    PORT_Assert(PR_TRUE == LocksInitializedEarly);
    if (!LocksInitializedEarly) {
        PORT_SetError(SEC_ERROR_NOT_INITIALIZED);
        return SECFailure;
    }
    FreeSessionCacheLocks();
    LocksInitializedEarly = PR_FALSE;
    return SECSuccess;
}

static PRCallOnceType lockOnce;

/* free the session cache locks if they were initialized lazily */
static SECStatus ssl_ShutdownLocks(void* appData, void* nssData)
{
    PORT_Assert(PR_FALSE == LocksInitializedEarly);
    if (LocksInitializedEarly) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }
    FreeSessionCacheLocks();
    memset(&lockOnce, 0, sizeof(lockOnce));
    return SECSuccess;
}

static PRStatus initSessionCacheLocksLazily(void)
{
    SECStatus rv = InitSessionCacheLocks();
    if (SECSuccess != rv) {
        return PR_FAILURE;
    }
    rv = NSS_RegisterShutdown(ssl_ShutdownLocks, NULL);
    PORT_Assert(SECSuccess == rv);
    if (SECSuccess != rv) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

/* lazyInit means that the call is not happening during a 1-time
 * initialization function, but rather during dynamic, lazy initialization
 */
SECStatus
ssl_InitSessionCacheLocks(PRBool lazyInit)
{
    if (LocksInitializedEarly) {
        return SECSuccess;
    }

    if (lazyInit) {
        return (PR_SUCCESS ==
                PR_CallOnce(&lockOnce, initSessionCacheLocksLazily)) ?
               SECSuccess : SECFailure;
    }
     
    if (SECSuccess == InitSessionCacheLocks()) {
        LocksInitializedEarly = PR_TRUE;
        return SECSuccess;
    }

    return SECFailure;
}

static void 
lock_cache(void)
{
    ssl_InitSessionCacheLocks(PR_TRUE);
    PZ_Lock(cacheLock);
}

/* BEWARE: This function gets called for both client and server SIDs !!
 * If the unreferenced sid is not in the cache, Free sid and its contents.
 */
static void
ssl_DestroySID(sslSessionID *sid)
{
    SSL_TRC(8, ("SSL: destroy sid: sid=0x%x cached=%d", sid, sid->cached));
    PORT_Assert((sid->references == 0));

    if (sid->cached == in_client_cache)
    	return;	/* it will get taken care of next time cache is traversed. */

    if (sid->version < SSL_LIBRARY_VERSION_3_0) {
	SECITEM_ZfreeItem(&sid->u.ssl2.masterKey, PR_FALSE);
	SECITEM_ZfreeItem(&sid->u.ssl2.cipherArg, PR_FALSE);
    }
    if (sid->peerID != NULL)
	PORT_Free((void *)sid->peerID);		/* CONST */

    if (sid->urlSvrName != NULL)
	PORT_Free((void *)sid->urlSvrName);	/* CONST */

    if ( sid->peerCert ) {
	CERT_DestroyCertificate(sid->peerCert);
    }
    if ( sid->localCert ) {
	CERT_DestroyCertificate(sid->localCert);
    }
    if (sid->u.ssl3.sessionTicket.ticket.data) {
	SECITEM_FreeItem(&sid->u.ssl3.sessionTicket.ticket, PR_FALSE);
    }
    if (sid->u.ssl3.srvName.data) {
	SECITEM_FreeItem(&sid->u.ssl3.srvName, PR_FALSE);
    }
    
    PORT_ZFree(sid, sizeof(sslSessionID));
}

/* BEWARE: This function gets called for both client and server SIDs !!
 * Decrement reference count, and 
 *    free sid if ref count is zero, and sid is not in the cache. 
 * Does NOT remove from the cache first.  
 * If the sid is still in the cache, it is left there until next time
 * the cache list is traversed.
 */
static void 
ssl_FreeLockedSID(sslSessionID *sid)
{
    PORT_Assert(sid->references >= 1);
    if (--sid->references == 0) {
	ssl_DestroySID(sid);
    }
}

/* BEWARE: This function gets called for both client and server SIDs !!
 * Decrement reference count, and 
 *    free sid if ref count is zero, and sid is not in the cache. 
 * Does NOT remove from the cache first.  
 * These locks are necessary because the sid _might_ be in the cache list.
 */
void
ssl_FreeSID(sslSessionID *sid)
{
    LOCK_CACHE;
    ssl_FreeLockedSID(sid);
    UNLOCK_CACHE;
}

/************************************************************************/

/*
**  Lookup sid entry in cache by Address, port, and peerID string.
**  If found, Increment reference count, and return pointer to caller.
**  If it has timed out or ref count is zero, remove from list and free it.
*/

sslSessionID *
ssl_LookupSID(const PRIPv6Addr *addr, PRUint16 port, const char *peerID, 
              const char * urlSvrName)
{
    sslSessionID **sidp;
    sslSessionID * sid;
    PRUint32       now;

    if (!urlSvrName)
    	return NULL;
    now = ssl_Time();
    LOCK_CACHE;
    sidp = &cache;
    while ((sid = *sidp) != 0) {
	PORT_Assert(sid->cached == in_client_cache);
	PORT_Assert(sid->references >= 1);

	SSL_TRC(8, ("SSL: Lookup1: sid=0x%x", sid));

	if (sid->expirationTime < now || !sid->references) {
	    /*
	    ** This session-id timed out, or was orphaned.
	    ** Don't even care who it belongs to, blow it out of our cache.
	    */
	    SSL_TRC(7, ("SSL: lookup1, throwing sid out, age=%d refs=%d",
			now - sid->creationTime, sid->references));

	    *sidp = sid->next; 			/* delink it from the list. */
	    sid->cached = invalid_cache;	/* mark not on list. */
	    if (!sid->references)
	    	ssl_DestroySID(sid);
	    else
		ssl_FreeLockedSID(sid);		/* drop ref count, free. */

	} else if (!memcmp(&sid->addr, addr, sizeof(PRIPv6Addr)) && /* server IP addr matches */
	           (sid->port == port) && /* server port matches */
		   /* proxy (peerID) matches */
		   (((peerID == NULL) && (sid->peerID == NULL)) ||
		    ((peerID != NULL) && (sid->peerID != NULL) &&
		     PORT_Strcmp(sid->peerID, peerID) == 0)) &&
		   /* is cacheable */
		   (sid->version < SSL_LIBRARY_VERSION_3_0 ||
		    sid->u.ssl3.keys.resumable) &&
		   /* server hostname matches. */
	           (sid->urlSvrName != NULL) &&
		   ((0 == PORT_Strcmp(urlSvrName, sid->urlSvrName)) ||
		    ((sid->peerCert != NULL) && (SECSuccess == 
		      CERT_VerifyCertName(sid->peerCert, urlSvrName))) )
		  ) {
	    /* Hit */
	    sid->lastAccessTime = now;
	    sid->references++;
	    break;
	} else {
	    sidp = &sid->next;
	}
    }
    UNLOCK_CACHE;
    return sid;
}

/*
** Add an sid to the cache or return a previously cached entry to the cache.
** Although this is static, it is called via ss->sec.cache().
*/
static void 
CacheSID(sslSessionID *sid)
{
    PRUint32  expirationPeriod;
    SSL_TRC(8, ("SSL: Cache: sid=0x%x cached=%d addr=0x%08x%08x%08x%08x port=0x%04x "
		"time=%x cached=%d",
		sid, sid->cached, sid->addr.pr_s6_addr32[0], 
		sid->addr.pr_s6_addr32[1], sid->addr.pr_s6_addr32[2],
		sid->addr.pr_s6_addr32[3],  sid->port, sid->creationTime,
		sid->cached));

    if (sid->cached == in_client_cache)
	return;

    if (!sid->urlSvrName) {
        /* don't cache this SID because it can never be matched */
        return;
    }

    /* XXX should be different trace for version 2 vs. version 3 */
    if (sid->version < SSL_LIBRARY_VERSION_3_0) {
	expirationPeriod = ssl_sid_timeout;
	PRINT_BUF(8, (0, "sessionID:",
		  sid->u.ssl2.sessionID, sizeof(sid->u.ssl2.sessionID)));
	PRINT_BUF(8, (0, "masterKey:",
		  sid->u.ssl2.masterKey.data, sid->u.ssl2.masterKey.len));
	PRINT_BUF(8, (0, "cipherArg:",
		  sid->u.ssl2.cipherArg.data, sid->u.ssl2.cipherArg.len));
    } else {
	if (sid->u.ssl3.sessionIDLength == 0 &&
	    sid->u.ssl3.sessionTicket.ticket.data == NULL)
	    return;
	/* Client generates the SessionID if this was a stateless resume. */
	if (sid->u.ssl3.sessionIDLength == 0) {
	    SECStatus rv;
	    rv = PK11_GenerateRandom(sid->u.ssl3.sessionID,
		SSL3_SESSIONID_BYTES);
	    if (rv != SECSuccess)
		return;
	    sid->u.ssl3.sessionIDLength = SSL3_SESSIONID_BYTES;
	}
	expirationPeriod = ssl3_sid_timeout;
	PRINT_BUF(8, (0, "sessionID:",
		      sid->u.ssl3.sessionID, sid->u.ssl3.sessionIDLength));
    }
    PORT_Assert(sid->creationTime != 0 && sid->expirationTime != 0);
    if (!sid->creationTime)
	sid->lastAccessTime = sid->creationTime = ssl_Time();
    if (!sid->expirationTime)
	sid->expirationTime = sid->creationTime + expirationPeriod;

    /*
     * Put sid into the cache.  Bump reference count to indicate that
     * cache is holding a reference. Uncache will reduce the cache
     * reference.
     */
    LOCK_CACHE;
    sid->references++;
    sid->cached = in_client_cache;
    sid->next   = cache;
    cache       = sid;
    UNLOCK_CACHE;
}

/* 
 * If sid "zap" is in the cache,
 *    removes sid from cache, and decrements reference count.
 * Caller must hold cache lock.
 */
static void
UncacheSID(sslSessionID *zap)
{
    sslSessionID **sidp = &cache;
    sslSessionID *sid;

    if (zap->cached != in_client_cache) {
	return;
    }

    SSL_TRC(8,("SSL: Uncache: zap=0x%x cached=%d addr=0x%08x%08x%08x%08x port=0x%04x "
	       "time=%x cipher=%d",
	       zap, zap->cached, zap->addr.pr_s6_addr32[0],
	       zap->addr.pr_s6_addr32[1], zap->addr.pr_s6_addr32[2],
	       zap->addr.pr_s6_addr32[3], zap->port, zap->creationTime,
	       zap->u.ssl2.cipherType));
    if (zap->version < SSL_LIBRARY_VERSION_3_0) {
	PRINT_BUF(8, (0, "sessionID:",
		      zap->u.ssl2.sessionID, sizeof(zap->u.ssl2.sessionID)));
	PRINT_BUF(8, (0, "masterKey:",
		      zap->u.ssl2.masterKey.data, zap->u.ssl2.masterKey.len));
	PRINT_BUF(8, (0, "cipherArg:",
		      zap->u.ssl2.cipherArg.data, zap->u.ssl2.cipherArg.len));
    }

    /* See if it's in the cache, if so nuke it */
    while ((sid = *sidp) != 0) {
	if (sid == zap) {
	    /*
	    ** Bingo. Reduce reference count by one so that when
	    ** everyone is done with the sid we can free it up.
	    */
	    *sidp = zap->next;
	    zap->cached = invalid_cache;
	    ssl_FreeLockedSID(zap);
	    return;
	}
	sidp = &sid->next;
    }
}

/* If sid "zap" is in the cache,
 *    removes sid from cache, and decrements reference count.
 * Although this function is static, it is called externally via 
 *    ss->sec.uncache().
 */
static void
LockAndUncacheSID(sslSessionID *zap)
{
    LOCK_CACHE;
    UncacheSID(zap);
    UNLOCK_CACHE;

}

/* choose client or server cache functions for this sslsocket. */
void 
ssl_ChooseSessionIDProcs(sslSecurityInfo *sec)
{
    if (sec->isServer) {
	sec->cache   = ssl_sid_cache;
	sec->uncache = ssl_sid_uncache;
    } else {
	sec->cache   = CacheSID;
	sec->uncache = LockAndUncacheSID;
    }
}

/* wipe out the entire client session cache. */
void
SSL_ClearSessionCache(void)
{
    LOCK_CACHE;
    while(cache != NULL)
	UncacheSID(cache);
    UNLOCK_CACHE;
}

/* returns an unsigned int containing the number of seconds in PR_Now() */
PRUint32
ssl_Time(void)
{
    PRUint32 myTime;
#if (defined(XP_UNIX) || defined(XP_WIN) || defined(_WINDOWS) || defined(XP_BEOS)) && !defined(_WIN32_WCE)
    myTime = time(NULL);	/* accurate until the year 2038. */
#else
    /* portable, but possibly slower */
    PRTime now;
    PRInt64 ll;

    now = PR_Now();
    LL_I2L(ll, 1000000L);
    LL_DIV(now, now, ll);
    LL_L2UI(myTime, now);
#endif
    return myTime;
}

SECStatus
ssl3_SetSIDSessionTicket(sslSessionID *sid, NewSessionTicket *session_ticket)
{
    SECStatus rv;

    /* We need to lock the cache, as this sid might already be in the cache. */
    LOCK_CACHE;

    /* A server might have sent us an empty ticket, which has the
     * effect of clearing the previously known ticket.
     */
    if (sid->u.ssl3.sessionTicket.ticket.data)
	SECITEM_FreeItem(&sid->u.ssl3.sessionTicket.ticket, PR_FALSE);
    if (session_ticket->ticket.len > 0) {
	rv = SECITEM_CopyItem(NULL, &sid->u.ssl3.sessionTicket.ticket,
	    &session_ticket->ticket);
	if (rv != SECSuccess) {
	    UNLOCK_CACHE;
	    return rv;
	}
    } else {
	sid->u.ssl3.sessionTicket.ticket.data = NULL;
	sid->u.ssl3.sessionTicket.ticket.len = 0;
    }
    sid->u.ssl3.sessionTicket.received_timestamp =
	session_ticket->received_timestamp;
    sid->u.ssl3.sessionTicket.ticket_lifetime_hint =
	session_ticket->ticket_lifetime_hint;

    UNLOCK_CACHE;
    return SECSuccess;
}
