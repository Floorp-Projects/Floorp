/* 
 * This file implements the CLIENT Session ID cache.  
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
/* $Id: sslnonce.c,v 1.15 2004/06/19 03:21:39 jpierre%netscape.com Exp $ */

#include "nssrenam.h"
#include "cert.h"
#include "secitem.h"
#include "ssl.h"

#include "sslimpl.h"
#include "sslproto.h"
#include "nssilock.h"
#include "nsslocks.h"
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

void ssl_InitClientSessionCacheLock(void)
{
    if (!cacheLock)
	nss_InitLock(&cacheLock, nssILockCache);
}

static void 
lock_cache(void)
{
    ssl_InitClientSessionCacheLock();
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
		    sid->u.ssl3.resumable) &&
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
	if (sid->u.ssl3.sessionIDLength == 0) 
	    return;
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

