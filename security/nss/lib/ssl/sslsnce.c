/* This file implements the SERVER Session ID cache. 
 * NOTE:  The contents of this file are NOT used by the client.
 *
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
 *
 * $Id: sslsnce.c,v 1.3 2000/09/07 03:35:31 nelsonb%netscape.com Exp $
 */

/* Note: ssl_FreeSID() in sslnonce.c gets used for both client and server 
 * cache sids!
 *
 * About record locking among different server processes:
 *
 * All processes that are part of the same conceptual server (serving on 
 * the same address and port) MUST share a common SSL session cache. 
 * This code makes the content of the shared cache accessible to all
 * processes on the same "server".  This code works on Unix and Win32 only,
 * and is platform specific. 
 *
 * Unix: Multiple processes share a single (inherited) FD for a disk 
 * file all share one single file position.  If one lseeks, the position for 
 * all processes is changed.  Since the set of platforms we support do not 
 * all share portable lseek-and-read or lseek-and-write functions, a global 
 * lock must be used to make the lseek call and the subsequent read or write 
 * call be one atomic operation.  It is no longer necessary for cache element 
 * sizes to be a power of 2, or a multiple of a sector size.
 *
 * For Win32, where (a) disk I/O is not atomic, and (b) we use memory-mapped
 * files and move data to & from memory instead of calling read or write,
 * we must do explicit locking of the records for all reads and writes.
 * We have just one lock, for the entire file, using an NT semaphore.
 * We avoid blocking on "local threads" since it's bad to block on a local 
 * thread - If NSPR offered portable semaphores, it would handle this itself.
 *
 * Since this file has to do lots of platform specific I/O, the system
 * dependent error codes need to be mapped back into NSPR error codes.
 * Since NSPR's error mapping functions are private, the code is necessarily
 * duplicated in libSSL.
 *
 * Note, now that NSPR provides portable anonymous shared memory, for all
 * platforms except Mac, the implementation below should be replaced with 
 * one that uses anonymous shared memory ASAP.  This will eliminate most 
 * platform dependent code in this file, and improve performance big time.
 *
 * Now that NSPR offers portable cross-process locking (semaphores) on Unix
 * and Win32, semaphores should be used here for all platforms.
 */
#include "seccomon.h"

#if defined(XP_UNIX) || defined(XP_WIN32)
#ifndef NADA_VERISON

#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "pk11func.h"
#include "base64.h"

#include <stdio.h>

#ifdef XP_UNIX

#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include "unix_err.h"

#else /* XP_WIN32 */
#ifdef MC_HTTPD
#include <ereport.h>
#endif /* MC_HTTPD */
#include <wtypes.h>
#include "win32err.h"
#endif /* XP_WIN32 */
#include <sys/types.h>

#define SET_ERROR_CODE /* reminder */

#include "nspr.h"
#include "nsslocks.h"

static PRLock *cacheLock;

/*
** The server session-id cache uses a simple flat cache. The cache is
** sized during initialization. We hash the ip-address + session-id value
** into an index into the cache and do the lookup. No buckets, nothing
** fancy.
*/

static PRBool isMultiProcess  = PR_FALSE;

static PRUint32 numSIDCacheEntries  = 10000; 
static PRUint32 sidCacheFileSize;
static PRUint32 sidCacheWrapOffset;

static PRUint32 numCertCacheEntries = 250;
static PRUint32 certCacheFileSize;

#define MIN_CERT_CACHE_ENTRIES 125 /* the effective size in old releases. */


/*
** Format of a cache entry.
*/ 
typedef struct SIDCacheEntryStr SIDCacheEntry;
struct SIDCacheEntryStr {
    PRUint32 addr;
    PRUint32 time;

    union {
	struct {
	    /* This is gross.  We have to have version and valid in both arms
	     * of the union for alignment reasons.  This probably won't work
	     * on a 64-bit machine. XXXX
	     */
/*  2 */    uint16        version;
/*  1 */    unsigned char valid;
/*  1 */    unsigned char cipherType;

/* 16 */    unsigned char sessionID[SSL_SESSIONID_BYTES];
/* 64 */    unsigned char masterKey[SSL_MAX_MASTER_KEY_BYTES];
/* 32 */    unsigned char cipherArg[SSL_MAX_CYPHER_ARG_BYTES];

/*  1 */    unsigned char masterKeyLen;
/*  1 */    unsigned char keyBits;

/*  1 */    unsigned char secretKeyBits;
/*  1 */    unsigned char cipherArgLen;
/*120 */} ssl2;

	struct {
/*  2 */    uint16           version;
/*  1 */    unsigned char    valid;
/*  1 */    uint8            sessionIDLength;

/* 32 */    unsigned char    sessionID[SSL3_SESSIONID_BYTES];

/*  2 */    ssl3CipherSuite  cipherSuite;
/*  2 */    uint16           compression; 	/* SSL3CompressionMethod */

/*122 */    ssl3SidKeys      keys;	/* keys and ivs, wrapped as needed. */
/*  4 */    PRUint32         masterWrapMech; 
/*  4 */    SSL3KEAType      exchKeyType;

/*  2 */    int16            certIndex;
/*  1 */    uint8            hasFortezza;
/*  1 */    uint8            resumable;
	} ssl3;
	/* We can't make this struct fit in 128 bytes 
	 * so, force the struct size up to the next power of two.
	 */
	struct {
	    unsigned char    filler[248];	/* 248 + 4 + 4 == 256 */
	} force256;
    } u;
};


typedef struct CertCacheEntryStr CertCacheEntry;

/* The length of this struct is supposed to be a power of 2, e.g. 4KB */
struct CertCacheEntryStr {
    uint16        certLength;				/*    2 */
    uint16        sessionIDLength;			/*    2 */
    unsigned char sessionID[SSL3_SESSIONID_BYTES];	/*   32 */
    unsigned char cert[SSL_MAX_CACHED_CERT_LEN];	/* 4060 */
};						/* total   4096 */


static void IOError(int rv, char *type);
static PRUint32 Offset(PRUint32 addr, unsigned char *s, unsigned nl);
static void Invalidate(SIDCacheEntry *sce);

/************************************************************************/

static const char envVarName[] = { SSL_ENV_VAR_NAME };

#ifdef _WIN32

struct winInheritanceStr {
    PRUint32 numSIDCacheEntries;
    PRUint32 sidCacheFileSize;
    PRUint32 sidCacheWrapOffset;
    PRUint32 numCertCacheEntries;
    PRUint32 certCacheFileSize;

    DWORD         parentProcessID;
    HANDLE        parentProcessHandle;
    HANDLE        SIDCacheFDMAP;
    HANDLE        certCacheFDMAP;
    HANDLE        svrCacheSem;
};

typedef struct winInheritanceStr winInheritance;

static HANDLE svrCacheSem    = INVALID_HANDLE_VALUE;

static char * SIDCacheData    = NULL;
static HANDLE SIDCacheFD      = INVALID_HANDLE_VALUE;
static HANDLE SIDCacheFDMAP   = INVALID_HANDLE_VALUE;

static char * certCacheData   = NULL;
static HANDLE certCacheFD     = INVALID_HANDLE_VALUE;
static HANDLE certCacheFDMAP  = INVALID_HANDLE_VALUE;

static PRUint32 myPid;

/* The presence of the TRUE element in this struct makes the semaphore 
 * inheritable. The NULL means use process's default security descriptor. 
 */
static SECURITY_ATTRIBUTES semaphoreAttributes = 
				{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

static SECURITY_ATTRIBUTES sidCacheFDMapAttributes = 
				{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

static SECURITY_ATTRIBUTES certCacheFDMapAttributes = 
				{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

#define DEFAULT_CACHE_DIRECTORY "\\temp"

static SECStatus
createServerCacheSemaphore(void)
{
    PR_ASSERT(svrCacheSem == INVALID_HANDLE_VALUE);

    /* inheritable, starts signalled, 1 signal max, no file name. */
    svrCacheSem = CreateSemaphore(&semaphoreAttributes, 1, 1, NULL);
    if (svrCacheSem == NULL) {
	svrCacheSem = INVALID_HANDLE_VALUE;
	/* We could get the error code, but what could be do with it ? */
	nss_MD_win32_map_default_error(GetLastError());
    	return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
_getServerCacheSemaphore(void)
{
    DWORD     event;
    DWORD     lastError;
    SECStatus rv;

    PR_ASSERT(svrCacheSem != INVALID_HANDLE_VALUE);
    if (svrCacheSem == INVALID_HANDLE_VALUE &&
    	SECSuccess   != createServerCacheSemaphore()) {
	return SECFailure;	/* what else ? */
    }
    event = WaitForSingleObject(svrCacheSem, INFINITE);
    switch (event) {
    case WAIT_OBJECT_0:
    case WAIT_ABANDONED:
	rv = SECSuccess;
    	break;

    case WAIT_TIMEOUT:
    case WAIT_IO_COMPLETION:
    default: 		/* should never happen. nothing we can do. */
	PR_ASSERT(("WaitForSingleObject returned invalid value.", 0));
	/* fall thru */

    case WAIT_FAILED:		/* failure returns this */
	rv = SECFailure;
	lastError = GetLastError();	/* for debugging */
	nss_MD_win32_map_default_error(lastError);
	break;
    }
    return rv;
}

static void
_doGetServerCacheSemaphore(void * arg)
{
    SECStatus * rv = (SECStatus *)arg;
    *rv = _getServerCacheSemaphore();
}

static SECStatus
getServerCacheSemaphore(void)
{
    PRThread *    selectThread;
    PRThread *    me    	= PR_GetCurrentThread();
    PRThreadScope scope 	= PR_GetThreadScope(me);
    SECStatus     rv    	= SECFailure;

    if (scope == PR_GLOBAL_THREAD) {
        rv = _getServerCacheSemaphore();
    } else {
        selectThread = PR_CreateThread(PR_USER_THREAD,
				       _doGetServerCacheSemaphore, &rv,
				       PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
				       PR_JOINABLE_THREAD, 0);
        if (selectThread != NULL) {
	    /* rv will be set by _doGetServerCacheSemaphore() */
	    PR_JoinThread(selectThread);
        }
    }
    return rv;
}

static SECStatus
releaseServerCacheSemaphore(void)
{
    BOOL success = FALSE;

    PR_ASSERT(svrCacheSem != INVALID_HANDLE_VALUE);
    if (svrCacheSem != INVALID_HANDLE_VALUE) {
	/* Add 1, don't want previous value. */
	success = ReleaseSemaphore(svrCacheSem, 1, NULL);
    }
    if (!success) {
	nss_MD_win32_map_default_error(GetLastError());
	return SECFailure;
    }
    return SECSuccess;
}

static void
destroyServerCacheSemaphore(void)
{
    PR_ASSERT(svrCacheSem != INVALID_HANDLE_VALUE);
    if (svrCacheSem != INVALID_HANDLE_VALUE) {
	CloseHandle(svrCacheSem);
	/* ignore error */
	svrCacheSem = INVALID_HANDLE_VALUE;
    }
}

#define GET_SERVER_CACHE_READ_LOCK(fd, offset, size) \
    if (isMultiProcess) getServerCacheSemaphore();

#define GET_SERVER_CACHE_WRITE_LOCK(fd, offset, size) \
    if (isMultiProcess) getServerCacheSemaphore();

#define RELEASE_SERVER_CACHE_LOCK(fd, offset, size) \
    if (isMultiProcess) releaseServerCacheSemaphore();

#endif /* _win32 */

/************************************************************************/

#ifdef XP_UNIX
static int    SIDCacheFD      = -1;
static int    certCacheFD     = -1;

static pid_t  myPid;

struct unixInheritanceStr {
    PRUint32 numSIDCacheEntries;
    PRUint32 sidCacheFileSize;
    PRUint32 sidCacheWrapOffset;
    PRUint32 numCertCacheEntries;
    PRUint32 certCacheFileSize;

    PRInt32  SIDCacheFD;
    PRInt32  certCacheFD;
};

typedef struct unixInheritanceStr unixInheritance;


#define DEFAULT_CACHE_DIRECTORY "/tmp"

#ifdef TRACE
static void 
fcntlFailed(struct flock *lock) 
{
    fprintf(stderr,
    "fcntl failed, errno = %d, PR_GetError = %d, lock.l_type = %d\n",
	errno, PR_GetError(), lock->l_type);
    fflush(stderr);
}
#define FCNTL_FAILED(lock) fcntlFailed(lock)
#else
#define FCNTL_FAILED(lock) 
#endif

/* NOTES:  Because there are no atomic seek-and-read and seek-and-write
** functions that are supported on all our UNIX platforms, we need
** to prevent all simultaeous seek-and-read operations.  For that reason,
** we use mutually exclusive (write) locks for read and write operations,
** and use them all at the same offset (zero).
*/
static SECStatus
_getServerCacheLock(int fd, short type, PRUint32 offset, PRUint32 size)
{
    int          result;
    struct flock lock;

    memset(&lock, 0, sizeof lock);
    lock.l_type   = /* type */ F_WRLCK;
    lock.l_whence = SEEK_SET;	/* absolute file offsets. */
    lock.l_start  = 0;
    lock.l_len    = 128;

#ifdef TRACE
    if (ssl_trace) {
	fprintf(stderr, "%d: %s lock, offset %8x, size %4d\n", myPid,
	    (type == F_RDLCK) ? "read " : "write", offset, size);
	fflush(stderr);
    }
#endif
    result = fcntl(fd, F_SETLKW, &lock);
    if (result == -1) {
        nss_MD_unix_map_default_error(errno);
	FCNTL_FAILED(&lock);
    	return SECFailure;
    }
#ifdef TRACE
    if (ssl_trace) {
	fprintf(stderr, "%d:   got lock, offset %8x, size %4d\n", 
	    myPid, offset, size);
	fflush(stderr);
    }
#endif
    return SECSuccess;
}

typedef struct sslLockArgsStr {
    PRUint32    offset;
    PRUint32    size;
    PRErrorCode err;
    SECStatus   rv;
    int         fd;
    short       type;
} sslLockArgs;

static void
_doGetServerCacheLock(void * arg)
{
    sslLockArgs * args = (sslLockArgs *)arg;
    args->rv = _getServerCacheLock(args->fd, args->type, args->offset, 
				   args->size );
    if (args->rv != SECSuccess) {
	args->err = PR_GetError();
    }
}

static SECStatus
getServerCacheLock(int fd, short type, PRUint32 offset, PRUint32 size)
{
    PRThread *    selectThread;
    PRThread *    me    	= PR_GetCurrentThread();
    PRThreadScope scope 	= PR_GetThreadScope(me);
    SECStatus     rv    	= SECFailure;

    if (scope == PR_GLOBAL_THREAD) {
        rv = _getServerCacheLock(fd, type, offset, size);
    } else {
	/* Ib some platforms, one thread cannot read local/automatic 
	** variables from another thread's stack.  So, get this space
	** from the heap, not the stack.
	*/
	sslLockArgs * args = PORT_New(sslLockArgs);

	if (!args)
	    return rv;

	args->offset = offset;
	args->size   = size;
	args->rv     = SECFailure;
	args->fd     = fd;
	args->type   = type;
        selectThread = PR_CreateThread(PR_USER_THREAD,
				       _doGetServerCacheLock, args,
				       PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
				       PR_JOINABLE_THREAD, 0);
        if (selectThread != NULL) {
	    /* rv will be set by _doGetServerCacheLock() */
	    PR_JoinThread(selectThread);
	    rv = args->rv;
	    if (rv != SECSuccess) {
		PORT_SetError(args->err);
	    }
        }
	PORT_Free(args);
    }
    return rv;
}

static SECStatus
releaseServerCacheLock(int fd, PRUint32 offset, PRUint32 size)
{
    int          result;
    struct flock lock;

    memset(&lock, 0, sizeof lock);
    lock.l_type   = F_UNLCK;
    lock.l_whence = SEEK_SET;	/* absolute file offsets. */
    lock.l_start  = 0;
    lock.l_len    = 128;

#ifdef TRACE
    if (ssl_trace) {
	fprintf(stderr, "%d:     unlock, offset %8x, size %4d\n", 
	    myPid, offset, size);
	fflush(stderr);
    }
#endif
    result = fcntl(fd, F_SETLK, &lock);
    if (result == -1) {
        nss_MD_unix_map_default_error(errno);
	FCNTL_FAILED(&lock);
    	return SECFailure;
    }
    return SECSuccess;
}


/* these defines take the arguments needed to do record locking, 
 * however the present implementation does only file locking.
 */

#define GET_SERVER_CACHE_READ_LOCK( fd, offset, size) \
    if (isMultiProcess) getServerCacheLock(fd, F_RDLCK, offset, size);

#define GET_SERVER_CACHE_WRITE_LOCK(fd, offset, size) \
    if (isMultiProcess) getServerCacheLock(fd, F_WRLCK, offset, size);

#define RELEASE_SERVER_CACHE_LOCK(  fd, offset, size) \
    if (isMultiProcess) releaseServerCacheLock(fd, offset, size);

/*
** Zero a file out to nb bytes
*/
static SECStatus 
ZeroFile(int fd, int nb)
{
    off_t off;
    int amount, rv;
    char buf[16384];

    PORT_Memset(buf, 0, sizeof(buf));
    off = lseek(fd, 0, SEEK_SET);
    if (off != 0) {
	if (off == -1) 
	    nss_MD_unix_map_lseek_error(errno);
    	else
	    PORT_SetError(PR_FILE_SEEK_ERROR);
    	return SECFailure;
    }

    while (nb > 0) {
	amount = (nb > sizeof buf) ? sizeof buf : nb;
	rv = write(fd, buf, amount);
	if (rv <= 0) {
	    if (!rv)
	    	PORT_SetError(PR_IO_ERROR);
	    else
		nss_MD_unix_map_write_error(errno);
	    IOError(rv, "zero-write");
	    return SECFailure;
	}
	nb -= rv;
    }
    return SECSuccess;
}

#endif /* XP_UNIX */


/************************************************************************/

/*
** Reconstitute a cert from the cache
** This is only called from ConvertToSID().
** Caller must hold the cache lock before calling this.
*/
static CERTCertificate *
GetCertFromCache(SIDCacheEntry *sce, CERTCertDBHandle *dbHandle)
{
    CERTCertificate *cert;
    PRUint32         offset;
    int              rv;
#ifdef XP_UNIX
    off_t            off;
#endif
    SECItem          derCert;
    CertCacheEntry   cce;

    offset = (PRUint32)sce->u.ssl3.certIndex * sizeof(CertCacheEntry);
    GET_SERVER_CACHE_READ_LOCK(certCacheFD, offset, sizeof(CertCacheEntry));
#ifdef XP_UNIX
    off = lseek(certCacheFD, offset, SEEK_SET);
    rv = -1;
    if (off != offset) {
	if (off == -1) 
	    nss_MD_unix_map_lseek_error(errno);
	else 
	    PORT_SetError(PR_FILE_SEEK_ERROR);
    } else {
	rv = read(certCacheFD, &cce, sizeof(CertCacheEntry));
	if (rv != sizeof(CertCacheEntry)) {
	    if (rv == -1)
		nss_MD_unix_map_read_error(errno);
	    else
	    	PORT_SetError(PR_IO_ERROR);
	}
    }
#else /* XP_WIN32 */
    /* Use memory mapped I/O and just memcpy() the data */
    CopyMemory(&cce, &certCacheData[offset], sizeof(CertCacheEntry));
    rv = sizeof cce;
#endif /* XP_WIN32 */
    RELEASE_SERVER_CACHE_LOCK(certCacheFD, offset, sizeof(CertCacheEntry))

    if (rv != sizeof(CertCacheEntry)) {
	IOError(rv, "read");	/* error set above */
	return NULL;
    }

    /* See if the session ID matches with that in the sce cache. */
    if((cce.sessionIDLength != sce->u.ssl3.sessionIDLength) ||
       PORT_Memcmp(cce.sessionID, sce->u.ssl3.sessionID, cce.sessionIDLength)) {
        /* this is a cache miss, not an error */
	PORT_SetError(SSL_ERROR_SESSION_NOT_FOUND);
	return NULL;
    }

    derCert.len  = cce.certLength;
    derCert.data = cce.cert;

    cert = CERT_NewTempCertificate(dbHandle, &derCert, NULL,
				   PR_FALSE, PR_TRUE);

    return cert;
}

/* Put a certificate in the cache.  We assume that the certIndex in
** sid is valid.
*/
static void
CacheCert(CERTCertificate *cert, SIDCacheEntry *sce)
{
    PRUint32       offset;
    CertCacheEntry cce;
#ifdef XP_UNIX
    off_t          off;
    int            rv;
#endif

    offset = (PRUint32)sce->u.ssl3.certIndex * sizeof(CertCacheEntry);
    if (cert->derCert.len > SSL_MAX_CACHED_CERT_LEN)
	return;
    
    cce.sessionIDLength = sce->u.ssl3.sessionIDLength;
    PORT_Memcpy(cce.sessionID, sce->u.ssl3.sessionID, cce.sessionIDLength);

    cce.certLength = cert->derCert.len;
    PORT_Memcpy(cce.cert, cert->derCert.data, cce.certLength);

    GET_SERVER_CACHE_WRITE_LOCK(certCacheFD, offset, sizeof cce);
#ifdef XP_UNIX
    off = lseek(certCacheFD, offset, SEEK_SET);
    if (off != offset) {
	if (off == -1) 
	    nss_MD_unix_map_lseek_error(errno);
	else 
	    PORT_SetError(PR_FILE_SEEK_ERROR);
    } else {
	rv = write(certCacheFD, &cce, sizeof cce);
	if (rv != sizeof(CertCacheEntry)) {
	    if (rv == -1)
		nss_MD_unix_map_write_error(errno);
	    else
	    	PORT_SetError(PR_IO_ERROR);
	    IOError(rv, "cert-write");
	    Invalidate(sce);
	}
    }
#else /* WIN32 */
    /* Use memory mapped I/O and just memcpy() the data */
    CopyMemory(&certCacheData[offset], &cce, sizeof cce);
#endif /* XP_UNIX */

    RELEASE_SERVER_CACHE_LOCK(certCacheFD, offset, sizeof cce);
    return;
}

/*
** Convert memory based SID to file based one
*/
static void 
ConvertFromSID(SIDCacheEntry *to, sslSessionID *from)
{
    to->u.ssl2.valid = 1;
    to->u.ssl2.version = from->version;
    to->addr = from->addr;
    to->time = from->time;

    if (from->version < SSL_LIBRARY_VERSION_3_0) {
	if ((from->u.ssl2.masterKey.len > SSL_MAX_MASTER_KEY_BYTES) ||
	    (from->u.ssl2.cipherArg.len > SSL_MAX_CYPHER_ARG_BYTES)) {
	    SSL_DBG(("%d: SSL: masterKeyLen=%d cipherArgLen=%d",
		     myPid, from->u.ssl2.masterKey.len,
		     from->u.ssl2.cipherArg.len));
	    to->u.ssl2.valid = 0;
	    return;
	}

	to->u.ssl2.cipherType    = from->u.ssl2.cipherType;
	to->u.ssl2.masterKeyLen  = from->u.ssl2.masterKey.len;
	to->u.ssl2.cipherArgLen  = from->u.ssl2.cipherArg.len;
	to->u.ssl2.keyBits       = from->u.ssl2.keyBits;
	to->u.ssl2.secretKeyBits = from->u.ssl2.secretKeyBits;
	PORT_Memcpy(to->u.ssl2.sessionID, from->u.ssl2.sessionID,
		  sizeof(to->u.ssl2.sessionID));
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
		    "time=%d addr=0x%x cipherType=%d", myPid,
		    to->u.ssl2.masterKeyLen, to->u.ssl2.cipherArgLen,
		    to->time, to->addr, to->u.ssl2.cipherType));
    } else {
	/* This is an SSL v3 session */

	to->u.ssl3.sessionIDLength  = from->u.ssl3.sessionIDLength;
	to->u.ssl3.cipherSuite      = from->u.ssl3.cipherSuite;
	to->u.ssl3.compression      = (uint16)from->u.ssl3.compression;
	to->u.ssl3.resumable        = from->u.ssl3.resumable;
	to->u.ssl3.hasFortezza      = from->u.ssl3.hasFortezza;
	to->u.ssl3.keys             = from->u.ssl3.keys;
	to->u.ssl3.masterWrapMech   = from->u.ssl3.masterWrapMech;
	to->u.ssl3.exchKeyType      = from->u.ssl3.exchKeyType;

	PORT_Memcpy(to->u.ssl3.sessionID, 
	            from->u.ssl3.sessionID,
		    from->u.ssl3.sessionIDLength);

	SSL_TRC(8, ("%d: SSL3: ConvertSID: time=%d addr=0x%x cipherSuite=%d",
		    myPid, to->time, to->addr, to->u.ssl3.cipherSuite));
    }
}

/*
** Convert file based cache-entry to memory based one
** This is only called from ServerSessionIDLookup().
** Caller must hold cache lock when calling this.
*/
static sslSessionID *
ConvertToSID(SIDCacheEntry *from, CERTCertDBHandle * dbHandle)
{
    sslSessionID *to;
    uint16 version = from->u.ssl2.version;

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
	    to->u.ssl2.cipherArg.data = (unsigned char*)
		PORT_Alloc(from->u.ssl2.cipherArgLen);
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
	PORT_Memcpy(to->u.ssl2.sessionID, from->u.ssl2.sessionID,
		  sizeof from->u.ssl2.sessionID);
	PORT_Memcpy(to->u.ssl2.masterKey.data, from->u.ssl2.masterKey,
		  from->u.ssl2.masterKeyLen);

	SSL_TRC(8, ("%d: SSL: ConvertToSID: masterKeyLen=%d cipherArgLen=%d "
		    "time=%d addr=0x%x cipherType=%d",
		    myPid, to->u.ssl2.masterKey.len,
		    to->u.ssl2.cipherArg.len, to->time, to->addr,
		    to->u.ssl2.cipherType));
    } else {
	/* This is an SSL v3 session */

	to->u.ssl3.sessionIDLength  = from->u.ssl3.sessionIDLength;
	to->u.ssl3.cipherSuite      = from->u.ssl3.cipherSuite;
	to->u.ssl3.compression      = (SSL3CompressionMethod)from->u.ssl3.compression;
	to->u.ssl3.resumable        = from->u.ssl3.resumable;
	to->u.ssl3.hasFortezza      = from->u.ssl3.hasFortezza;
	to->u.ssl3.keys             = from->u.ssl3.keys;
	to->u.ssl3.masterWrapMech   = from->u.ssl3.masterWrapMech;
	to->u.ssl3.exchKeyType      = from->u.ssl3.exchKeyType;

	PORT_Memcpy(to->u.ssl3.sessionID, 
	            from->u.ssl3.sessionID,
		    from->u.ssl3.sessionIDLength);

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

	if (from->u.ssl3.certIndex != -1) {
	    to->peerCert = GetCertFromCache(from, dbHandle);
	    if (to->peerCert == NULL)
		goto loser;
	}
    }

    to->version    = from->u.ssl2.version;
    to->time       = from->time;
    to->cached     = in_server_cache;
    to->addr       = from->addr;
    to->references = 1;
    
    return to;

  loser:
    Invalidate(from);
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


/* Invalidate a SID cache entry. 
 * Called from CacheCert, ConvertToSid, and ServerSessionIDUncache. 
 */
static void 
Invalidate(SIDCacheEntry *sce)
{
    PRUint32	offset;
#ifdef XP_UNIX
    off_t	off;
    int 	rv;
#endif

    if (sce == NULL) return;

    if (sce->u.ssl2.version < SSL_LIBRARY_VERSION_3_0) {
	offset = Offset(sce->addr, sce->u.ssl2.sessionID,
			sizeof sce->u.ssl2.sessionID); 
    } else {
	offset = Offset(sce->addr, sce->u.ssl3.sessionID,
			sce->u.ssl3.sessionIDLength); 
    }
	
    sce->u.ssl2.valid = 0;
    SSL_TRC(7, ("%d: SSL: uncaching session-id at offset %ld",
		    myPid, offset));

    GET_SERVER_CACHE_WRITE_LOCK(SIDCacheFD, offset, sizeof *sce);

#ifdef XP_UNIX
    off = lseek(SIDCacheFD, offset, SEEK_SET);
    if (off != offset) {
	if (off == -1) 
	    nss_MD_unix_map_lseek_error(errno);
	else 
	    PORT_SetError(PR_FILE_SEEK_ERROR);
    } else {
	rv = write(SIDCacheFD, sce, sizeof *sce);
	if (rv != sizeof *sce) {
	    if (rv == -1)
		nss_MD_unix_map_write_error(errno);
	    else
	    	PORT_SetError(PR_IO_ERROR);
	    IOError(rv, "invalidate-write");
    	}
    }
#else /* WIN32 */
    /* Use memory mapped I/O and just memcpy() the data */
    CopyMemory(&SIDCacheData[offset], sce, sizeof *sce);
#endif /* XP_UNIX */

    RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, offset, sizeof *sce);
}


static void 
IOError(int rv, char *type)
{
#ifdef XP_UNIX
    syslog(LOG_ALERT,
	   "SSL: %s error with session-id cache, pid=%d, rv=%d, error='%m'",
	   type, myPid, rv);
#else /* XP_WIN32 */
#ifdef MC_HTTPD 
    ereport(LOG_FAILURE, "%s error with session-id cache rv=%d\n",type, rv);
#endif /* MC_HTTPD */
#endif /* XP_UNIX */
}

static void 
lock_cache(void)
{
    PR_Lock(cacheLock);
}

static void 
unlock_cache(void)
{
    PR_Unlock(cacheLock);
}

/*
** Perform some mumbo jumbo on the ip-address and the session-id value to
** compute a hash value.
*/
static PRUint32 
Offset(PRUint32 addr, unsigned char *s, unsigned nl)
{
    PRUint32 rv;

    rv = addr ^ (((PRUint32)s[0] << 24) | ((PRUint32)s[1] << 16)
		 | (s[2] << 8) | s[nl-1]);
    return (rv % numSIDCacheEntries) * sizeof(SIDCacheEntry);
}



/*
** Look something up in the cache. This will invalidate old entries
** in the process. Caller has locked the cache!
** Returns PR_TRUE if found a valid match.  PR_FALSE otherwise.
*/
static PRBool 
FindSID(PRUint32 addr, unsigned char *sessionID,
	unsigned sessionIDLength, SIDCacheEntry *sce)
{
    PRUint32      offset;
    PRUint32      now;
    int           rv;
#ifdef XP_UNIX
    off_t         off;
#endif

    /* Read in cache entry after hashing ip address and session-id value */
    offset = Offset(addr, sessionID, sessionIDLength); 
    now = ssl_Time();
    GET_SERVER_CACHE_READ_LOCK(SIDCacheFD, offset, sizeof *sce);
#ifdef XP_UNIX
    off = lseek(SIDCacheFD, offset, SEEK_SET);
    rv = -1;
    if (off != offset) {
	if (off == -1) 
	    nss_MD_unix_map_lseek_error(errno);
	else 
	    PORT_SetError(PR_FILE_SEEK_ERROR);
    } else {
	rv = read(SIDCacheFD, sce, sizeof *sce);
	if (rv != sizeof *sce) {
	    if (rv == -1) 
		nss_MD_unix_map_read_error(errno);
	    else 
		PORT_SetError(PR_IO_ERROR);
    	}
    }
#else /* XP_WIN32 */
    /* Use memory mapped I/O and just memcpy() the data */
    CopyMemory(sce, &SIDCacheData[offset], sizeof *sce);
    rv = sizeof *sce;
#endif /* XP_WIN32 */
    RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, offset, sizeof *sce);

    if (rv != sizeof *sce) {
	IOError(rv, "server sid cache read");
	return PR_FALSE;
    }

    if (!sce->u.ssl2.valid) {
	/* Entry is not valid */
	PORT_SetError(SSL_ERROR_SESSION_NOT_FOUND);
	return PR_FALSE;
    }

    if (((sce->u.ssl2.version < SSL_LIBRARY_VERSION_3_0) &&
	 (now > sce->time + ssl_sid_timeout)) ||
	((sce->u.ssl2.version >= SSL_LIBRARY_VERSION_3_0) &&
	 (now > sce->time + ssl3_sid_timeout))) {
	/* SessionID has timed out. Invalidate the entry. */
	SSL_TRC(7, ("%d: timed out sid entry addr=%08x now=%x time+=%x",
		    myPid, sce->addr, now, sce->time + ssl_sid_timeout));
	sce->u.ssl2.valid = 0;

	GET_SERVER_CACHE_WRITE_LOCK(SIDCacheFD, offset, sizeof *sce);
#ifdef XP_UNIX
	off = lseek(SIDCacheFD, offset, SEEK_SET);
	rv = -1;
	if (off != offset) {
	    if (off == -1) 
		nss_MD_unix_map_lseek_error(errno);
	    else
	    	PORT_SetError(PR_IO_ERROR);
	} else {
	    rv = write(SIDCacheFD, sce, sizeof *sce);
	    if (rv != sizeof *sce) {
		if (rv == -1) 
		    nss_MD_unix_map_write_error(errno);
		else 
		    PORT_SetError(PR_IO_ERROR);
		IOError(rv, "timeout-write");
	    }
	}
#else /* WIN32 */
	/* Use memory mapped I/O and just memcpy() the data */
	CopyMemory(&SIDCacheData[offset], sce, sizeof *sce);
	rv = sizeof *sce;
#endif /* XP_UNIX */
	RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, offset, sizeof *sce);
	if (rv == sizeof *sce)
	    PORT_SetError(SSL_ERROR_SESSION_NOT_FOUND);
	return PR_FALSE;
    }

    /*
    ** Finally, examine specific session-id/addr data to see if the cache
    ** entry matches our addr+session-id value
    */
    if ((sce->addr == addr) &&
	(PORT_Memcmp(sce->u.ssl2.sessionID, sessionID, sessionIDLength) == 0)) {
	/* Found it */
	return PR_TRUE;
    }
    PORT_SetError(SSL_ERROR_SESSION_NOT_FOUND);
    return PR_FALSE;
}

/************************************************************************/

/* This is the primary function for finding entries in the server's sid cache.
 * Although it is static, this function is called via the global function 
 * pointer ssl_sid_lookup.
 */
static sslSessionID *
ServerSessionIDLookup(	PRUint32       addr,
			unsigned char *sessionID,
			unsigned int   sessionIDLength,
                        CERTCertDBHandle * dbHandle)
{
    SIDCacheEntry sce;
    sslSessionID *sid;

    sid = 0;
    lock_cache();
    if (FindSID(addr, sessionID, sessionIDLength, &sce)) {
	/* Found it. Convert file format to internal format */
	sid = ConvertToSID(&sce, dbHandle);
    }
    unlock_cache();
    return sid;
}

/*
** Place an sid into the cache, if it isn't already there. Note that if
** some other server process has replaced a session-id cache entry that has
** the same cache index as this sid, then all is ok. Somebody has to lose
** when this condition occurs, so it might as well be this sid.
*/
static void 
ServerSessionIDCache(sslSessionID *sid)
{
    SIDCacheEntry sce;
    PRUint32      offset;
#ifdef XP_UNIX
    off_t         off;
    int           rv;
#endif
    uint16 version = sid->version;

    if ((version >= SSL_LIBRARY_VERSION_3_0) &&
	(sid->u.ssl3.sessionIDLength == 0)) {
	return;
    }

    if (sid->cached == never_cached || sid->cached == invalid_cache) {
	lock_cache();

	sid->time = ssl_Time();
	if (version < SSL_LIBRARY_VERSION_3_0) {
	    SSL_TRC(8, ("%d: SSL: CacheMT: cached=%d addr=0x%08x time=%x "
			"cipher=%d", myPid, sid->cached, sid->addr,
			sid->time, sid->u.ssl2.cipherType));
	    PRINT_BUF(8, (0, "sessionID:", sid->u.ssl2.sessionID,
			  sizeof(sid->u.ssl2.sessionID)));
	    PRINT_BUF(8, (0, "masterKey:", sid->u.ssl2.masterKey.data,
			  sid->u.ssl2.masterKey.len));
	    PRINT_BUF(8, (0, "cipherArg:", sid->u.ssl2.cipherArg.data,
			  sid->u.ssl2.cipherArg.len));

	    /* Write out new cache entry */
	    offset = Offset(sid->addr, sid->u.ssl2.sessionID,
			    sizeof(sid->u.ssl2.sessionID)); 
	} else {
	    SSL_TRC(8, ("%d: SSL: CacheMT: cached=%d addr=0x%08x time=%x "
			"cipherSuite=%d", myPid, sid->cached, sid->addr,
			sid->time, sid->u.ssl3.cipherSuite));
	    PRINT_BUF(8, (0, "sessionID:", sid->u.ssl3.sessionID,
			  sid->u.ssl3.sessionIDLength));

	    offset = Offset(sid->addr, sid->u.ssl3.sessionID,
			    sid->u.ssl3.sessionIDLength);
	    
	}

	ConvertFromSID(&sce, sid);
	if (version >= SSL_LIBRARY_VERSION_3_0) {
	    if (sid->peerCert == NULL) {
		sce.u.ssl3.certIndex = -1;
	    } else {
		sce.u.ssl3.certIndex = (int16)
		    ((offset / sizeof(SIDCacheEntry)) % numCertCacheEntries);
	    }
	}
	
	GET_SERVER_CACHE_WRITE_LOCK(SIDCacheFD, offset, sizeof sce);
#ifdef XP_UNIX
	off = lseek(SIDCacheFD, offset, SEEK_SET);
	if (off != offset) {
	    if (off == -1) 
		nss_MD_unix_map_lseek_error(errno);
	    else
	    	PORT_SetError(PR_IO_ERROR);
	} else {
	    rv = write(SIDCacheFD, &sce, sizeof sce);
	    if (rv != sizeof(sce)) {
		if (rv == -1) 
		    nss_MD_unix_map_write_error(errno);
		else 
		    PORT_SetError(PR_IO_ERROR);
		IOError(rv, "update-write");
	    }
	}
#else /* WIN32 */
	CopyMemory(&SIDCacheData[offset], &sce, sizeof sce);
#endif /* XP_UNIX */
	RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, offset, sizeof sce);

	if ((version >= SSL_LIBRARY_VERSION_3_0) && 
	    (sid->peerCert != NULL)) {
	    CacheCert(sid->peerCert, &sce);
	}

	sid->cached = in_server_cache;
	unlock_cache();
    }
}

static void 
ServerSessionIDUncache(sslSessionID *sid)
{
    SIDCacheEntry sce;
    PRErrorCode   err;
    int rv;

    if (sid == NULL) return;
    
    /* Uncaching a SID should never change the error code. 
    ** So save it here and restore it before exiting.
    */
    err = PR_GetError();
    lock_cache();
    if (sid->version < SSL_LIBRARY_VERSION_3_0) {
	SSL_TRC(8, ("%d: SSL: UncacheMT: valid=%d addr=0x%08x time=%x "
		    "cipher=%d", myPid, sid->cached, sid->addr,
		    sid->time, sid->u.ssl2.cipherType));
	PRINT_BUF(8, (0, "sessionID:", sid->u.ssl2.sessionID,
		      sizeof(sid->u.ssl2.sessionID)));
	PRINT_BUF(8, (0, "masterKey:", sid->u.ssl2.masterKey.data,
		      sid->u.ssl2.masterKey.len));
	PRINT_BUF(8, (0, "cipherArg:", sid->u.ssl2.cipherArg.data,
		      sid->u.ssl2.cipherArg.len));
	rv = FindSID(sid->addr, sid->u.ssl2.sessionID,
		     sizeof(sid->u.ssl2.sessionID), &sce);
    } else {
	SSL_TRC(8, ("%d: SSL3: UncacheMT: valid=%d addr=0x%08x time=%x "
		    "cipherSuite=%d", myPid, sid->cached, sid->addr,
		    sid->time, sid->u.ssl3.cipherSuite));
	PRINT_BUF(8, (0, "sessionID:", sid->u.ssl3.sessionID,
		      sid->u.ssl3.sessionIDLength));
	rv = FindSID(sid->addr, sid->u.ssl3.sessionID,
		     sid->u.ssl3.sessionIDLength, &sce);
    }
    
    if (rv) {
	Invalidate(&sce);
    }
    sid->cached = invalid_cache;
    unlock_cache();
    PORT_SetError(err);
}

static SECStatus
InitSessionIDCache(int maxCacheEntries, PRUint32 timeout,
		   PRUint32 ssl3_timeout, const char *directory)
{
    char *cfn;
#ifdef XP_UNIX
    int rv;
    if (SIDCacheFD >= 0) {
	/* Already done */
	return SECSuccess;
    }
#else /* WIN32 */
	if(SIDCacheFDMAP != INVALID_HANDLE_VALUE) {
	/* Already done */
	return SECSuccess;
	}
#endif /* XP_UNIX */


    if (maxCacheEntries) {
	numSIDCacheEntries = maxCacheEntries;
    }
    sidCacheWrapOffset = numSIDCacheEntries * sizeof(SIDCacheEntry);
    sidCacheFileSize = sidCacheWrapOffset +
         (kt_kea_size * SSL_NUM_WRAP_MECHS * sizeof(SSLWrappedSymWrappingKey));

    /* Create file names */
    cfn = (char*) PORT_Alloc(PORT_Strlen(directory) + 100);
    if (!cfn) {
	return SECFailure;
    }
#ifdef XP_UNIX
    sprintf(cfn, "%s/.sslsidc.%d", directory, getpid());
#else /* XP_WIN32 */
	sprintf(cfn, "%s\\ssl.sidc.%d.%d", directory,
			GetCurrentProcessId(), GetCurrentThreadId());
#endif /* XP_WIN32 */

    /* Create session-id cache file */
#ifdef XP_UNIX
    do {
	(void) unlink(cfn);
	SIDCacheFD = open(cfn, O_EXCL|O_CREAT|O_RDWR, 0600);
    } while (SIDCacheFD < 0 && errno == EEXIST);
    if (SIDCacheFD < 0) {
	nss_MD_unix_map_open_error(errno);
	IOError(SIDCacheFD, "create");
	goto loser;
    }
    rv = unlink(cfn);
    if (rv < 0) {
	nss_MD_unix_map_unlink_error(errno);
	IOError(rv, "unlink");
	goto loser;
    }
#else  /* WIN32 */
    SIDCacheFDMAP = 
    	CreateFileMapping(INVALID_HANDLE_VALUE, /* allocate in swap file */
			  &sidCacheFDMapAttributes, /* inheritable. */
			  PAGE_READWRITE,
			  0,                    /* size, high word. */
			  sidCacheFileSize,     /* size, low  word. */
			  NULL);		/* no map name in FS */
    if(! SIDCacheFDMAP) {
	nss_MD_win32_map_default_error(GetLastError());
	goto loser;
    }
    SIDCacheData = (char *)MapViewOfFile(SIDCacheFDMAP, 
                                         FILE_MAP_ALL_ACCESS, 	/* R/W    */
					 0, 0, 			/* offset */
				         sidCacheFileSize);	/* size   */
    if (! SIDCacheData) {
	nss_MD_win32_map_default_error(GetLastError());
	goto loser;
    }
#endif /* XP_UNIX */

    if (!cacheLock)
	nss_InitLock(&cacheLock);
    if (!cacheLock) {
	SET_ERROR_CODE
	goto loser;
    }
#ifdef _WIN32
    if (isMultiProcess  && (SECSuccess != createServerCacheSemaphore())) {
	SET_ERROR_CODE
	goto loser;
    }
#endif

    if (timeout) {   
	if (timeout > 100) {
	    timeout = 100;
	}
	if (timeout < 5) {
	    timeout = 5;
	}
	ssl_sid_timeout = timeout;
    }

    if (ssl3_timeout) {   
	if (ssl3_timeout > 86400L) {
	    ssl3_timeout = 86400L;
	}
	if (ssl3_timeout < 5) {
	    ssl3_timeout = 5;
	}
	ssl3_sid_timeout = ssl3_timeout;
    }

    GET_SERVER_CACHE_WRITE_LOCK(SIDCacheFD, 0, sidCacheFileSize);
#ifdef XP_UNIX
    /* Initialize the files */
    if (ZeroFile(SIDCacheFD, sidCacheFileSize)) {
	/* Bummer */
	close(SIDCacheFD);
	SIDCacheFD = -1;
	goto loser;
    }
#else /* XP_WIN32 */
    ZeroMemory(SIDCacheData, sidCacheFileSize);
#endif /* XP_UNIX */
    RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, 0, sidCacheFileSize);
    PORT_Free(cfn);
    return SECSuccess;

  loser:
#ifdef _WIN32
    if (svrCacheSem)
	destroyServerCacheSemaphore();
#endif
    if (cacheLock) {
	PR_DestroyLock(cacheLock);
	cacheLock = NULL;
    }
    PORT_Free(cfn);
    return SECFailure;
}

static SECStatus 
InitCertCache(const char *directory)
{
    char *cfn;
#ifdef XP_UNIX
    int rv;
    if (certCacheFD >= 0) {
	/* Already done */
	return SECSuccess;
    }
#else /* WIN32 */
    if(certCacheFDMAP != INVALID_HANDLE_VALUE) {
	/* Already done */
	return SECSuccess;
    }
#endif /* XP_UNIX */

    numCertCacheEntries = sidCacheFileSize / sizeof(CertCacheEntry);
    if (numCertCacheEntries < MIN_CERT_CACHE_ENTRIES)
    	numCertCacheEntries = MIN_CERT_CACHE_ENTRIES;
    certCacheFileSize = numCertCacheEntries * sizeof(CertCacheEntry);

    /* Create file names */
    cfn = (char*) PORT_Alloc(PORT_Strlen(directory) + 100);
    if (!cfn) {
	return SECFailure;
    }
#ifdef XP_UNIX
    sprintf(cfn, "%s/.sslcertc.%d", directory, getpid());
#else /* XP_WIN32 */
    sprintf(cfn, "%s\\ssl.certc.%d.%d", directory,
	    GetCurrentProcessId(), GetCurrentThreadId());
#endif /* XP_WIN32 */

    /* Create certificate cache file */
#ifdef XP_UNIX
    do {
	(void) unlink(cfn);
	certCacheFD = open(cfn, O_EXCL|O_CREAT|O_RDWR, 0600);
    } while (certCacheFD < 0 && errno == EEXIST);
    if (certCacheFD < 0) {
	nss_MD_unix_map_open_error(errno);
	IOError(certCacheFD, "create");
	goto loser;
    }
    rv = unlink(cfn);
    if (rv < 0) {
	nss_MD_unix_map_unlink_error(errno);
	IOError(rv, "unlink");
	goto loser;
    }
#else  /* WIN32 */
    certCacheFDMAP = 
    	CreateFileMapping(INVALID_HANDLE_VALUE, /* allocate in swap file */
			  &certCacheFDMapAttributes, /* inheritable. */
			  PAGE_READWRITE,
			  0,                /* size, high word. */
			  certCacheFileSize, /* size, low word. */
			  NULL);           /* no map name in FS */
    if (! certCacheFDMAP) {
	nss_MD_win32_map_default_error(GetLastError());
	goto loser;
    }
    certCacheData = (char *) MapViewOfFile(certCacheFDMAP, 
                                           FILE_MAP_ALL_ACCESS, /* R/W    */
					   0, 0, 		/* offset */
					   certCacheFileSize);	/* size   */
    if (! certCacheData) {
	nss_MD_win32_map_default_error(GetLastError());
	goto loser;
    }
#endif /* XP_UNIX */

/*  GET_SERVER_CACHE_WRITE_LOCK(certCacheFD, 0, certCacheFileSize); */
#ifdef XP_UNIX
    /* Initialize the files */
    if (ZeroFile(certCacheFD, certCacheFileSize)) {
	/* Bummer */
	close(certCacheFD);
	certCacheFD = -1;
	goto loser;
    }
#else /* XP_WIN32 */
    ZeroMemory(certCacheData, certCacheFileSize);
#endif /* XP_UNIX */
/*  RELEASE_SERVER_CACHE_LOCK(certCacheFD, 0, certCacheFileSize);   */
    PORT_Free(cfn);
    return SECSuccess;

  loser:
    PORT_Free(cfn);
    return SECFailure;
}

int
SSL_ConfigServerSessionIDCache(	int      maxCacheEntries, 
				PRUint32 timeout,
			       	PRUint32 ssl3_timeout, 
			  const char *   directory)
{
    SECStatus rv;

    PORT_Assert(sizeof(SIDCacheEntry) == 256);
    PORT_Assert(sizeof(CertCacheEntry) == 4096);

    myPid = SSL_GETPID();
    if (!directory) {
	directory = DEFAULT_CACHE_DIRECTORY;
    }
    rv = InitSessionIDCache(maxCacheEntries, timeout, ssl3_timeout, directory);
    if (rv) {
	SET_ERROR_CODE
    	return SECFailure;
    }
    rv = InitCertCache(directory);
    if (rv) {
	SET_ERROR_CODE
    	return SECFailure;
    }

    ssl_sid_lookup  = ServerSessionIDLookup;
    ssl_sid_cache   = ServerSessionIDCache;
    ssl_sid_uncache = ServerSessionIDUncache;
    return SECSuccess;
}

/* Use this function, instead of SSL_ConfigServerSessionIDCache,
 * if the cache will be shared by multiple processes.
 */
int
SSL_ConfigMPServerSIDCache(	int      maxCacheEntries, 
				PRUint32 timeout,
			       	PRUint32 ssl3_timeout, 
		          const char *   directory)
{
    char *	envValue;
    int 	result;
    SECStatus	putEnvFailed;

    isMultiProcess = PR_TRUE;
    result = SSL_ConfigServerSessionIDCache(maxCacheEntries, timeout, 
                                            ssl3_timeout, directory);
    if (result == SECSuccess) {
#ifdef _WIN32
	winInheritance winherit;

	winherit.numSIDCacheEntries	= numSIDCacheEntries;
	winherit.sidCacheFileSize	= sidCacheFileSize;
	winherit.sidCacheWrapOffset	= sidCacheWrapOffset;
	winherit.numCertCacheEntries	= numCertCacheEntries;
	winherit.certCacheFileSize	= certCacheFileSize;
	winherit.SIDCacheFDMAP		= SIDCacheFDMAP;
	winherit.certCacheFDMAP		= certCacheFDMAP;
	winherit.svrCacheSem		= svrCacheSem;
	winherit.parentProcessID	= GetCurrentProcessId();
	winherit.parentProcessHandle	= 
	    OpenProcess(PROCESS_DUP_HANDLE, TRUE, winherit.parentProcessID);
        if (winherit.parentProcessHandle == NULL) {
	    SET_ERROR_CODE
	    return SECFailure;
	}
        envValue = BTOA_DataToAscii((unsigned char *)&winherit, 
	                             sizeof winherit);
    	if (!envValue) {
	    SET_ERROR_CODE
	    return SECFailure;
	}
#else
	unixInheritance uinherit;

	uinherit.numSIDCacheEntries	= numSIDCacheEntries;
	uinherit.sidCacheFileSize	= sidCacheFileSize;
	uinherit.sidCacheWrapOffset	= sidCacheWrapOffset;
	uinherit.numCertCacheEntries	= numCertCacheEntries;
	uinherit.certCacheFileSize	= certCacheFileSize;
	uinherit.SIDCacheFD		= SIDCacheFD;
	uinherit.certCacheFD		= certCacheFD;

        envValue = BTOA_DataToAscii((unsigned char *)&uinherit, 
	                             sizeof uinherit);
    	if (!envValue) {
	    SET_ERROR_CODE
	    return SECFailure;
	}
#endif
    }
    putEnvFailed = (SECStatus)NSS_PutEnv(envVarName, envValue);
    PORT_Free(envValue);
    if (putEnvFailed) {
        SET_ERROR_CODE
        result = SECFailure;
    }
    return result;
}

SECStatus
SSL_InheritMPServerSIDCache(const char * envString)
{
    unsigned char * decoString = NULL;
    unsigned int    decoLen;
#ifdef _WIN32
    winInheritance  inherit;
#else
    unixInheritance inherit;
#endif

    myPid = SSL_GETPID();
    if (isMultiProcess)
    	return SECSuccess;	/* already done. */

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

    decoString = ATOB_AsciiToData(envString, &decoLen);
    if (!decoString) {
    	SET_ERROR_CODE
	return SECFailure;
    }
    if (decoLen != sizeof inherit) {
    	SET_ERROR_CODE
    	goto loser;
    }

    PORT_Memcpy(&inherit, decoString, sizeof inherit);
    PORT_Free(decoString);

    numSIDCacheEntries	= inherit.numSIDCacheEntries;
    sidCacheFileSize	= inherit.sidCacheFileSize;
    sidCacheWrapOffset	= inherit.sidCacheWrapOffset;
    numCertCacheEntries	= inherit.numCertCacheEntries;
    certCacheFileSize	= inherit.certCacheFileSize;

#ifdef _WIN32
    SIDCacheFDMAP	= inherit.SIDCacheFDMAP;
    certCacheFDMAP	= inherit.certCacheFDMAP;
    svrCacheSem		= inherit.svrCacheSem;

#if 0
    /* call DuplicateHandle ?? */
    inherit.parentProcessID;
    inherit.parentProcessHandle;
#endif

    if(!SIDCacheFDMAP) {
    	SET_ERROR_CODE
    	goto loser;
    }
    SIDCacheData = (char *)MapViewOfFile(SIDCacheFDMAP, 
                                         FILE_MAP_ALL_ACCESS, 	/* R/W    */
					 0, 0, 			/* offset */
				         sidCacheFileSize);	/* size   */
    if(!SIDCacheData) {
	nss_MD_win32_map_default_error(GetLastError());
    	goto loser;
    }

    if(!certCacheFDMAP) {
    	SET_ERROR_CODE
    	goto loser;
    }
    certCacheData = (char *) MapViewOfFile(certCacheFDMAP, 
                                           FILE_MAP_ALL_ACCESS, /* R/W    */
					   0, 0, 		/* offset */
					   certCacheFileSize);	/* size   */
    if(!certCacheData) {
	nss_MD_win32_map_default_error(GetLastError());
    	goto loser;
    }

#else /* must be unix */
    SIDCacheFD		= inherit.SIDCacheFD;
    certCacheFD		= inherit.certCacheFD;
    if (SIDCacheFD < 0 || certCacheFD < 0) {
    	SET_ERROR_CODE
    	goto loser;
    }
#endif

    if (!cacheLock) {
	nss_InitLock(&cacheLock);
	if (!cacheLock) 
	    goto loser;
    }
    isMultiProcess = PR_TRUE;
    return SECSuccess;

loser:
    if (decoString) 
	PORT_Free(decoString);
#if _WIN32
    if (SIDCacheFDMAP) {
	CloseHandle(SIDCacheFDMAP);
	SIDCacheFDMAP = NULL;
    }
    if (certCacheFDMAP) {
	CloseHandle(certCacheFDMAP);
	certCacheFDMAP = NULL;
    }
#else
    if (SIDCacheFD >= 0) {
	close(SIDCacheFD);
	SIDCacheFD = -1;
    }
    if (certCacheFD >= 0) {
	close(certCacheFD);
	certCacheFD = -1;
    }
#endif
    return SECFailure;

}

/************************************************************************
 *  Code dealing with shared wrapped symmetric wrapping keys below      *
 ************************************************************************/


static PRBool
getWrappingKey(PRInt32                   symWrapMechIndex,
               SSL3KEAType               exchKeyType, 
               SSLWrappedSymWrappingKey *wswk, 
               PRBool                    grabSharedLock)
{
    PRUint32      offset = sidCacheWrapOffset + 
	       ((exchKeyType * SSL_NUM_WRAP_MECHS + symWrapMechIndex) *
		   sizeof(SSLWrappedSymWrappingKey));
    PRBool        rv     = PR_TRUE;
#ifdef XP_UNIX
    off_t         lrv;
    ssize_t       rrv;
#endif

    if (grabSharedLock) {
	GET_SERVER_CACHE_READ_LOCK(SIDCacheFD, offset, sizeof *wswk);
    }

#ifdef XP_UNIX
    lrv = lseek(SIDCacheFD, offset, SEEK_SET);
    if (lrv != offset) {
	if (lrv == -1) 
	    nss_MD_unix_map_lseek_error(errno);
	else
	    PORT_SetError(PR_IO_ERROR);
	    IOError(rv, "wrapping-read");
	rv = PR_FALSE;
    } else {
	rrv = read(SIDCacheFD, wswk, sizeof *wswk);
	if (rrv != sizeof *wswk) {
	    if (rrv == -1) 
		nss_MD_unix_map_read_error(errno);
	    else 
		PORT_SetError(PR_IO_ERROR);
	    IOError(rv, "wrapping-read");
	    rv = PR_FALSE;
	}
    }
#else /* XP_WIN32 */
    /* Use memory mapped I/O and just memcpy() the data */
    CopyMemory(wswk, &SIDCacheData[offset], sizeof *wswk);
#endif /* XP_WIN32 */
    if (grabSharedLock) {
	RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, offset, sizeof *wswk);
    }
    if (rv) {
    	if (wswk->exchKeyType      != exchKeyType || 
	    wswk->symWrapMechIndex != symWrapMechIndex ||
	    wswk->wrappedSymKeyLen == 0) {
	    memset(wswk, 0, sizeof *wswk);
	    rv = PR_FALSE;
	}
    }
    return rv;
}

PRBool
ssl_GetWrappingKey( PRInt32                   symWrapMechIndex,
                    SSL3KEAType               exchKeyType, 
		    SSLWrappedSymWrappingKey *wswk)
{
    PRBool rv;

    lock_cache();

    PORT_Assert( (unsigned)exchKeyType < kt_kea_size);
    PORT_Assert( (unsigned)symWrapMechIndex < SSL_NUM_WRAP_MECHS);
    if ((unsigned)exchKeyType < kt_kea_size &&
        (unsigned)symWrapMechIndex < SSL_NUM_WRAP_MECHS) {
	rv = getWrappingKey(symWrapMechIndex, exchKeyType, wswk, PR_TRUE);
    } else {
    	rv = PR_FALSE;
    }
    unlock_cache();
    return rv;
}

/* The caller passes in the new value it wants
 * to set.  This code tests the wrapped sym key entry in the file on disk.  
 * If it is uninitialized, this function writes the caller's value into 
 * the disk entry, and returns false.  
 * Otherwise, it overwrites the caller's wswk with the value obtained from 
 * the disk, and returns PR_TRUE.  
 * This is all done while holding the locks/semaphores necessary to make 
 * the operation atomic.
 */
PRBool
ssl_SetWrappingKey(SSLWrappedSymWrappingKey *wswk)
{
    PRBool        rv;
    SSL3KEAType   exchKeyType      = wswk->exchKeyType;   
                                /* type of keys used to wrap SymWrapKey*/
    PRInt32       symWrapMechIndex = wswk->symWrapMechIndex;
    PRUint32      offset;
    SSLWrappedSymWrappingKey myWswk;

    PORT_Assert( (unsigned)exchKeyType < kt_kea_size);
    if ((unsigned)exchKeyType >= kt_kea_size)
    	return 0;

    PORT_Assert( (unsigned)symWrapMechIndex < SSL_NUM_WRAP_MECHS);
    if ((unsigned)symWrapMechIndex >=  SSL_NUM_WRAP_MECHS)
    	return 0;

    offset = sidCacheWrapOffset + 
	       ((exchKeyType * SSL_NUM_WRAP_MECHS + symWrapMechIndex) *
		   sizeof(SSLWrappedSymWrappingKey));
    PORT_Memset(&myWswk, 0, sizeof myWswk);	/* eliminate UMRs. */
    lock_cache();
    GET_SERVER_CACHE_WRITE_LOCK(SIDCacheFD, offset, sizeof *wswk);

    rv = getWrappingKey(wswk->symWrapMechIndex, wswk->exchKeyType, &myWswk, 
                        PR_FALSE);
    if (rv) {
	/* we found it on disk, copy it out to the caller. */
	PORT_Memcpy(wswk, &myWswk, sizeof *wswk);
    } else {
	/* Wasn't on disk, and we're still holding the lock, so write it. */

#ifdef XP_UNIX
	off_t         lrv;
	ssize_t       rrv;

	lrv = lseek(SIDCacheFD, offset, SEEK_SET);
	if (lrv != offset) {
	    if (lrv == -1) 
		nss_MD_unix_map_lseek_error(errno);
	    else
		PORT_SetError(PR_IO_ERROR);
	    IOError(rv, "wrapping-read");
	    rv = PR_FALSE;
	} else {
	    rrv = write(SIDCacheFD, wswk, sizeof *wswk);
	    if (rrv != sizeof *wswk) {
		if (rrv == -1) 
		    nss_MD_unix_map_read_error(errno);
		else 
		    PORT_SetError(PR_IO_ERROR);
		IOError(rv, "wrapping-read");
		rv = PR_FALSE;
	    }
	}
#else /* XP_WIN32 */
	/* Use memory mapped I/O and just memcpy() the data */
	CopyMemory(&SIDCacheData[offset], wswk, sizeof *wswk);
#endif /* XP_WIN32 */
    }
    RELEASE_SERVER_CACHE_LOCK(SIDCacheFD, offset, sizeof *wswk);
    unlock_cache();
    return rv;
}


#endif /* NADA_VERISON */
#else

#include "seccomon.h"
#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"

PRBool
ssl_GetWrappingKey( PRInt32                   symWrapMechIndex,
                    SSL3KEAType               exchKeyType, 
		    SSLWrappedSymWrappingKey *wswk)
{
    PRBool rv = PR_FALSE;
    PR_ASSERT(!"SSL servers are not supported on the Mac. (ssl_GetWrappingKey)");    
    return rv;
}

/* This is a kind of test-and-set.  The caller passes in the new value it wants
 * to set.  This code tests the wrapped sym key entry in the file on disk.  
 * If it is uninitialized, this function writes the caller's value into 
 * the disk entry, and returns false.  
 * Otherwise, it overwrites the caller's wswk with the value obtained from 
 * the disk, and returns PR_TRUE.  
 * This is all done while holding the locks/semaphores necessary to make 
 * the operation atomic.
 */
PRBool
ssl_SetWrappingKey(SSLWrappedSymWrappingKey *wswk)
{
    PRBool        rv = PR_FALSE;
    PR_ASSERT(!"SSL servers are not supported on the Mac. (ssl_SetWrappingKey)");
    return rv;
}

#endif /* XP_UNIX || XP_WIN32 */
