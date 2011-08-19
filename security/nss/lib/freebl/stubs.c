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
 * The Original Code is Network Security Services.
 *
 * The Initial Developer of the Original Code is
 * Red Hat Inc.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *	Robert Relyea <rrelyea@redhat.com>
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
 * Allow freebl and softoken to be loaded without util or NSPR.
 *
 * These symbols are overridden once real NSPR, and libutil are attached.
 */
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <prio.h>
#include <prlink.h>
#include <prlog.h>
#include <prthread.h>
#include <plstr.h>
#include <prinit.h>
#include <prlock.h>
#include <prmem.h>
#include <prerror.h>
#include <prmon.h>
#include <pratom.h>
#include <prsystem.h>
#include <prinrval.h>
#include <prtime.h>
#include <prcvar.h>
#include <secasn1.h>
#include <secoid.h>
#include <secdig.h>
#include <secport.h>
#include <secitem.h>
#include <blapi.h>
#include <private/pprio.h>

#define FREEBL_NO_WEAK 1

#define WEAK __attribute__((weak))

#ifdef FREEBL_NO_WEAK

/*
 * This uses function pointers.
 * 
 * CONS:  A separate function is needed to
 * fill in the function pointers.
 *
 * PROS: it works on all platforms.
 *  it allows for dynamically finding nspr and libutil, even once
 *  softoken is loaded and running. (NOTE: this may be a problem if
 *  we switch between the stubs and real NSPR on the fly. NSPR will
 *  do bad things if passed an _FakeArena to free or allocate from).
 */
#define STUB_DECLARE(ret, fn,  args) \
   typedef ret (*type_##fn) args; \
   static type_##fn ptr_##fn = NULL

#define STUB_SAFE_CALL0(fn) \
    if (ptr_##fn) { return ptr_##fn(); }
#define STUB_SAFE_CALL1(fn,a1) \
    if (ptr_##fn) { return ptr_##fn(a1); }
#define STUB_SAFE_CALL2(fn,a1,a2) \
    if (ptr_##fn) { return ptr_##fn(a1,a2); }
#define STUB_SAFE_CALL3(fn,a1,a2,a3) \
    if (ptr_##fn) { return ptr_##fn(a1,a2,a3); }
#define STUB_SAFE_CALL4(fn,a1,a2,a3,a4) \
    if (ptr_##fn) { return ptr_##fn(a1,a2,a3,a4); }
#define STUB_SAFE_CALL6(fn,a1,a2,a3,a4,a5,a6) \
    if (ptr_##fn) { return ptr_##fn(a1,a2,a3,a4,a5,a6); }

#define STUB_FETCH_FUNCTION(fn) \
    ptr_##fn = (type_##fn) dlsym(lib,#fn); \
    if (ptr_##fn == NULL) { \
        return SECFailure; \
    }

#else
/*
 * this uses the loader weak attribute. it works automatically, but once
 * freebl is loaded, the symbols are 'fixed' (later loading of NSPR or 
 * libutil will not resolve these symbols.
 */

#define STUB_DECLARE(ret, fn,  args) \
   WEAK extern ret fn args

#define STUB_SAFE_CALL0(fn) \
    if (fn) { return fn(); }
#define STUB_SAFE_CALL1(fn,a1) \
    if (fn) { return fn(a1); }
#define STUB_SAFE_CALL2(fn,a1,a2) \
    if (fn) { return fn(a1,a2); }
#define STUB_SAFE_CALL3(fn,a1,a2,a3) \
    if (fn) { return fn(a1,a2,a3); }
#define STUB_SAFE_CALL4(fn,a1,a2,a3,a4) \
    if (fn) { return fn(a1,a2,a3,a4); }
#define STUB_SAFE_CALL6(fn,a1,a2,a3,a4,a5,a6) \
    if (fn) { return fn(a1,a2,a3,a4,a5,a6); }
#endif


STUB_DECLARE(void *,PORT_Alloc_Util,(size_t len));
STUB_DECLARE(void *,PORT_ArenaZAlloc_Util,(PLArenaPool *arena, size_t size));
STUB_DECLARE(void ,PORT_Free_Util,(void *ptr));
STUB_DECLARE(void ,PORT_FreeArena_Util,(PLArenaPool *arena, PRBool zero));
STUB_DECLARE(int,PORT_GetError_Util,(void));
STUB_DECLARE(PLArenaPool *,PORT_NewArena_Util,(unsigned long chunksize));
STUB_DECLARE(void,PORT_SetError_Util,(int value));
STUB_DECLARE(void *,PORT_ZAlloc_Util,(size_t len));
STUB_DECLARE(void,PORT_ZFree_Util,(void *ptr, size_t len));

STUB_DECLARE(void,PR_Assert,(const char *s, const char *file, PRIntn ln));
STUB_DECLARE(PRStatus,PR_CallOnce,(PRCallOnceType *once, PRCallOnceFN func));
STUB_DECLARE(PRStatus,PR_Close,(PRFileDesc *fd));
STUB_DECLARE(void,PR_DestroyLock,(PRLock *lock));
STUB_DECLARE(void,PR_DestroyCondVar,(PRCondVar *cvar));
STUB_DECLARE(void,PR_Free,(void *ptr));
STUB_DECLARE(char * ,PR_GetLibraryFilePathname,(const char *name,
			PRFuncPtr addr));
STUB_DECLARE(PRFileDesc *,PR_ImportPipe,(PROsfd osfd));
STUB_DECLARE(void,PR_Lock,(PRLock *lock));
STUB_DECLARE(PRCondVar *,PR_NewCondVar,(PRLock *lock));
STUB_DECLARE(PRLock *,PR_NewLock,(void));
STUB_DECLARE(PRStatus,PR_NotifyCondVar,(PRCondVar *cvar));
STUB_DECLARE(PRStatus,PR_NotifyAllCondVar,(PRCondVar *cvar));
STUB_DECLARE(PRFileDesc *,PR_Open,(const char *name, PRIntn flags,
			 PRIntn mode));
STUB_DECLARE(PRInt32,PR_Read,(PRFileDesc *fd, void *buf, PRInt32 amount));
STUB_DECLARE(PROffset32,PR_Seek,(PRFileDesc *fd, PROffset32 offset, 
			PRSeekWhence whence));
STUB_DECLARE(PRStatus,PR_Sleep,(PRIntervalTime ticks));
STUB_DECLARE(PRStatus,PR_Unlock,(PRLock *lock));
STUB_DECLARE(PRStatus,PR_WaitCondVar,(PRCondVar *cvar,
			PRIntervalTime timeout));


STUB_DECLARE(SECItem *,SECITEM_AllocItem_Util,(PRArenaPool *arena, 
			SECItem *item,unsigned int len));
STUB_DECLARE(SECComparison,SECITEM_CompareItem_Util,(const SECItem *a, 
			const SECItem *b));
STUB_DECLARE(SECStatus,SECITEM_CopyItem_Util,(PRArenaPool *arena, 
			SECItem *to,const SECItem *from));
STUB_DECLARE(void,SECITEM_FreeItem_Util,(SECItem *zap, PRBool freeit));
STUB_DECLARE(void,SECITEM_ZfreeItem_Util,(SECItem *zap, PRBool freeit));
STUB_DECLARE(int, NSS_SecureMemcmp,(const void *a, const void *b, size_t n));


#define PORT_ZNew_stub(type) (type*)PORT_ZAlloc_stub(sizeof(type))
#define PORT_New_stub(type) (type*)PORT_Alloc_stub(sizeof(type))
#define PORT_ZNewArray_stub(type, num)       \
                (type*) PORT_ZAlloc_stub (sizeof(type)*(num))


/*
 * NOTE: in order to support hashing only the memory allocation stubs,
 * the get library name stubs, and the file io stubs are needed (the latter
 * two are for the library verification). The remaining stubs are simply to
 * compile. Attempts to use the library for other operations without NSPR
 * will most likely fail.
 */

 
/* memory */
extern void *
PORT_Alloc_stub(size_t len)
{
    STUB_SAFE_CALL1(PORT_Alloc_Util, len);
    return malloc(len);
}

extern void
PORT_Free_stub(void *ptr)
{
    STUB_SAFE_CALL1(PORT_Free_Util, ptr);
    return free(ptr);
}

extern void *
PORT_ZAlloc_stub(size_t len)
{
    STUB_SAFE_CALL1(PORT_ZAlloc_Util, len);
    void *ptr = malloc(len);
    if (ptr) {
	memset(ptr, 0, len);
    }
    return ptr;
}


extern void
PORT_ZFree_stub(void *ptr, size_t len)
{
    STUB_SAFE_CALL2(PORT_ZFree_Util, ptr, len);
    memset(ptr, 0, len);
    return free(ptr);
}

extern void
PR_Free_stub(void *ptr)
{
    STUB_SAFE_CALL1(PR_Free, ptr);
    return free(ptr);
}

/*
 * arenas
 *
 */
extern PLArenaPool *
PORT_NewArena_stub(unsigned long chunksize) 
{
    STUB_SAFE_CALL1(PORT_NewArena_Util, chunksize);
    abort();
    return NULL;
}

extern void *
PORT_ArenaZAlloc_stub(PLArenaPool *arena, size_t size)
{

    STUB_SAFE_CALL2(PORT_ArenaZAlloc_Util, arena, size);
    abort();
    return NULL;
}

extern void    
PORT_FreeArena_stub(PLArenaPool *arena, PRBool zero)
{

    STUB_SAFE_CALL2(PORT_FreeArena_Util, arena, zero);
    abort();
}


/* io */
extern PRFileDesc *
PR_Open_stub(const char *name, PRIntn flags, PRIntn mode)
{
    int *lfd = NULL;
    int fd;
    int lflags = 0;

    STUB_SAFE_CALL3(PR_Open, name, flags, mode);

    if (flags & PR_RDWR) {
        lflags = O_RDWR;
    } else if (flags & PR_WRONLY) {
        lflags = O_WRONLY;
    } else {
        lflags = O_RDONLY;
    }

    if (flags & PR_EXCL)
        lflags |= O_EXCL;
    if (flags & PR_APPEND)
        lflags |= O_APPEND;
    if (flags & PR_TRUNCATE)
        lflags |= O_TRUNC;

    fd = open(name, lflags, mode);
    if (fd >= 0) {
	lfd = PORT_New_stub(int);
	if (lfd != NULL) {
	    *lfd = fd;
	}
    }
    return (PRFileDesc *)lfd;
}

extern PRFileDesc *
PR_ImportPipe_stub(PROsfd fd)
{
    int *lfd = NULL;

    STUB_SAFE_CALL1(PR_ImportPipe, fd);

    lfd = PORT_New_stub(int);
    if (lfd != NULL) {
	*lfd = fd;
    }
    return (PRFileDesc *)lfd;
}

extern PRStatus
PR_Close_stub(PRFileDesc *fd)
{
    int *lfd;
    STUB_SAFE_CALL1(PR_Close, fd);

    lfd = (int *)fd;
    close(*lfd);
    PORT_Free_stub(lfd);
    
    return PR_SUCCESS;
}

extern PRInt32
PR_Read_stub(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    int *lfd;
    STUB_SAFE_CALL3(PR_Read, fd, buf, amount);
    
    lfd = (int *)fd;
    return read(*lfd, buf, amount);
}

extern PROffset32
PR_Seek_stub(PRFileDesc *fd, PROffset32 offset, PRSeekWhence whence)
{
    int *lfd;
    int lwhence = SEEK_SET;;
    STUB_SAFE_CALL3(PR_Seek, fd, offset, whence);
    lfd = (int *)fd;
    switch (whence) {
        case PR_SEEK_CUR:
            lwhence = SEEK_CUR;
            break;
        case PR_SEEK_END:
            lwhence = SEEK_END;
            break;
    }

    return lseek(*lfd, offset, lwhence);
}


/*
 * library 
 */
extern char *
PR_GetLibraryFilePathname_stub(const char *name, PRFuncPtr addr)
{
    Dl_info dli;
    char *result;

    STUB_SAFE_CALL2(PR_GetLibraryFilePathname, name, addr);

    if (dladdr((void *)addr, &dli) == 0) {
        return NULL;
    }
    result = PORT_Alloc_stub(strlen(dli.dli_fname)+1);
    if (result != NULL) {
        strcpy(result, dli.dli_fname);
    }
    return result;
}


#include <errno.h>

/* errors */
extern int
PORT_GetError_stub(void)
{ 
    STUB_SAFE_CALL0(PORT_GetError_Util);
    return errno;
}

extern void 
PORT_SetError_stub(int value)
{ 
    STUB_SAFE_CALL1(PORT_SetError_Util, value);
    errno = value;
}

/* misc */
extern void
PR_Assert_stub(const char *s, const char *file, PRIntn ln)
{
    STUB_SAFE_CALL3(PR_Assert, s, file, ln);
    fprintf(stderr, "%s line %d: %s\n", file, ln, s);
    abort();
}

/* time */
extern PRStatus
PR_Sleep_stub(PRIntervalTime ticks)
{
    STUB_SAFE_CALL1(PR_Sleep, ticks);
    usleep(ticks*1000); 
    return PR_SUCCESS;
}


/* locking */
extern PRLock *
PR_NewLock_stub(void)
{
    STUB_SAFE_CALL0(PR_NewLock);
    abort();
    return NULL;
}

extern PRStatus
PR_Unlock_stub(PRLock *lock)
{
    STUB_SAFE_CALL1(PR_Unlock, lock);
    abort();
    return PR_FAILURE;
}

extern void
PR_Lock_stub(PRLock *lock)
{
    STUB_SAFE_CALL1(PR_Lock, lock);
    abort();
    return;
}

extern void
PR_DestroyLock_stub(PRLock *lock)
{
    STUB_SAFE_CALL1(PR_DestroyLock, lock);
    abort();
    return;
}

extern PRCondVar *
PR_NewCondVar_stub(PRLock *lock)
{
    STUB_SAFE_CALL1(PR_NewCondVar, lock);
    abort();
    return NULL;
}

extern PRStatus
PR_NotifyCondVar_stub(PRCondVar *cvar)
{
    STUB_SAFE_CALL1(PR_NotifyCondVar, cvar);
    abort();
    return PR_FAILURE;
}

extern PRStatus
PR_NotifyAllCondVar_stub(PRCondVar *cvar)
{
    STUB_SAFE_CALL1(PR_NotifyAllCondVar, cvar);
    abort();
    return PR_FAILURE;
}

extern PRStatus
PR_WaitCondVar_stub(PRCondVar *cvar, PRIntervalTime timeout)
{
    STUB_SAFE_CALL2(PR_WaitCondVar, cvar, timeout);
    abort();
    return PR_FAILURE;
}



extern void
PR_DestroyCondVar_stub(PRCondVar *cvar)
{
    STUB_SAFE_CALL1(PR_DestroyCondVar, cvar);
    abort();
    return;
}

/*
 * NOTE: this presupposes GCC 4.1
 */
extern PRStatus
PR_CallOnce_stub(PRCallOnceType *once, PRCallOnceFN func)
{
    STUB_SAFE_CALL2(PR_CallOnce, once, func);
    abort();
    return PR_FAILURE;
}


/*
 * SECITEMS implement Item Utilities
 */
extern void
SECITEM_FreeItem_stub(SECItem *zap, PRBool freeit)
{
    STUB_SAFE_CALL2(SECITEM_FreeItem_Util, zap, freeit);
    abort();
}

extern SECItem *
SECITEM_AllocItem_stub(PRArenaPool *arena, SECItem *item, unsigned int len)
{
    STUB_SAFE_CALL3(SECITEM_AllocItem_Util, arena, item, len); 
    abort();
    return NULL;
}

extern SECComparison
SECITEM_CompareItem_stub(const SECItem *a, const SECItem *b) 
{
    STUB_SAFE_CALL2(SECITEM_CompareItem_Util, a, b);
    abort();
    return SECEqual;
}

extern SECStatus 
SECITEM_CopyItem_stub(PRArenaPool *arena, SECItem *to, const SECItem *from)
{
    STUB_SAFE_CALL3(SECITEM_CopyItem_Util, arena, to, from);
    abort();
    return SECFailure;
}

extern void
SECITEM_ZfreeItem_stub(SECItem *zap, PRBool freeit)
{
    STUB_SAFE_CALL2(SECITEM_ZfreeItem_Util, zap, freeit);
    abort();
}

extern int
NSS_SecureMemcmp_stub(const void *a, const void *b, size_t n)
{
    STUB_SAFE_CALL3(NSS_SecureMemcmp, a, b, n);
    abort();
}

#ifdef FREEBL_NO_WEAK

static const char *nsprLibName = SHLIB_PREFIX"nspr4."SHLIB_SUFFIX;
static const char *nssutilLibName = SHLIB_PREFIX"nssutil3."SHLIB_SUFFIX;

static SECStatus
freebl_InitNSPR(void *lib)
{
    STUB_FETCH_FUNCTION(PR_Free);
    STUB_FETCH_FUNCTION(PR_Open);
    STUB_FETCH_FUNCTION(PR_ImportPipe);
    STUB_FETCH_FUNCTION(PR_Close);
    STUB_FETCH_FUNCTION(PR_Read);
    STUB_FETCH_FUNCTION(PR_Seek);
    STUB_FETCH_FUNCTION(PR_GetLibraryFilePathname);
    STUB_FETCH_FUNCTION(PR_Assert);
    STUB_FETCH_FUNCTION(PR_Sleep);
    STUB_FETCH_FUNCTION(PR_CallOnce);
    STUB_FETCH_FUNCTION(PR_NewCondVar);
    STUB_FETCH_FUNCTION(PR_NotifyCondVar);
    STUB_FETCH_FUNCTION(PR_NotifyAllCondVar);
    STUB_FETCH_FUNCTION(PR_WaitCondVar);
    STUB_FETCH_FUNCTION(PR_DestroyCondVar);
    STUB_FETCH_FUNCTION(PR_NewLock);
    STUB_FETCH_FUNCTION(PR_Unlock);
    STUB_FETCH_FUNCTION(PR_Lock);
    STUB_FETCH_FUNCTION(PR_DestroyLock);
    return SECSuccess;
}

static SECStatus
freebl_InitNSSUtil(void *lib)
{
    STUB_FETCH_FUNCTION(PORT_Alloc_Util);
    STUB_FETCH_FUNCTION(PORT_Free_Util);
    STUB_FETCH_FUNCTION(PORT_ZAlloc_Util);
    STUB_FETCH_FUNCTION(PORT_ZFree_Util);
    STUB_FETCH_FUNCTION(PORT_NewArena_Util);
    STUB_FETCH_FUNCTION(PORT_ArenaZAlloc_Util);
    STUB_FETCH_FUNCTION(PORT_FreeArena_Util);
    STUB_FETCH_FUNCTION(PORT_GetError_Util);
    STUB_FETCH_FUNCTION(PORT_SetError_Util);
    STUB_FETCH_FUNCTION(SECITEM_FreeItem_Util);
    STUB_FETCH_FUNCTION(SECITEM_AllocItem_Util);
    STUB_FETCH_FUNCTION(SECITEM_CompareItem_Util);
    STUB_FETCH_FUNCTION(SECITEM_CopyItem_Util);
    STUB_FETCH_FUNCTION(SECITEM_ZfreeItem_Util);
    STUB_FETCH_FUNCTION(NSS_SecureMemcmp);
    return SECSuccess;
}

/*
 * fetch the library if it's loaded. For NSS it should already be loaded
 */
#define freebl_getLibrary(libName)  \
    dlopen (libName, RTLD_LAZY|RTLD_NOLOAD)

#define freebl_releaseLibrary(lib) \
    if (lib) dlclose(lib)

static void * FREEBLnsprGlobalLib = NULL;
static void * FREEBLnssutilGlobalLib = NULL;

void __attribute ((destructor)) FREEBL_unload()
{
    freebl_releaseLibrary(FREEBLnsprGlobalLib);
    freebl_releaseLibrary(FREEBLnssutilGlobalLib);
}
#endif

/*
 * load the symbols from the real libraries if available.
 * 
 * if force is set, explicitly load the libraries if they are not already
 * loaded. If we could not use the real libraries, return failure.
 */
extern SECStatus
FREEBL_InitStubs()
{
    SECStatus rv = SECSuccess;
#ifdef FREEBL_NO_WEAK
    void *nspr = NULL; 
    void *nssutil = NULL; 

    /* NSPR should be first */
    if (!FREEBLnsprGlobalLib) {
	nspr = freebl_getLibrary(nsprLibName);
	if (!nspr) {
	    return SECFailure;
	}
	rv = freebl_InitNSPR(nspr);
	if (rv != SECSuccess) {
	    freebl_releaseLibrary(nspr);
	    return rv;
	}
	FREEBLnsprGlobalLib = nspr; /* adopt */
    }
    /* now load NSSUTIL */
    if (!FREEBLnssutilGlobalLib) {
	nssutil= freebl_getLibrary(nssutilLibName);
	if (!nssutil) {
	    return SECFailure;
	}
	rv = freebl_InitNSSUtil(nssutil);
	if (rv != SECSuccess) {
	    freebl_releaseLibrary(nssutil);
	    return rv;
	}
	FREEBLnssutilGlobalLib = nssutil; /* adopt */
    }
#endif

    return rv;
}
