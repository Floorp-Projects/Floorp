/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Thread safe versions of malloc, free, realloc, calloc and cfree.
*/

#include "primpl.h"

#ifdef _PR_ZONE_ALLOCATOR

/*
** The zone allocator code must use native mutexes and cannot
** use PRLocks because PR_NewLock calls PR_Calloc, resulting
** in cyclic dependency of initialization.
*/

#include <string.h>

union memBlkHdrUn;

typedef struct MemoryZoneStr {
    union memBlkHdrUn    *head;         /* free list */
    pthread_mutex_t       lock;
    size_t                blockSize;    /* size of blocks on this free list */
    PRUint32              locked;       /* current state of lock */
    PRUint32              contention;   /* counter: had to wait for lock */
    PRUint32              hits;         /* allocated from free list */
    PRUint32              misses;       /* had to call malloc */
    PRUint32              elements;     /* on free list */
} MemoryZone;

typedef union memBlkHdrUn {
    unsigned char filler[48];  /* fix the size of this beast */
    struct memBlkHdrStr {
        union memBlkHdrUn    *next;
        MemoryZone           *zone;
        size_t                blockSize;
        size_t                requestedSize;
        PRUint32              magic;
    } s;
} MemBlockHdr;

#define MEM_ZONES     7
#define THREAD_POOLS 11  /* prime number for modulus */
#define ZONE_MAGIC  0x0BADC0DE

static MemoryZone zones[MEM_ZONES][THREAD_POOLS];

static PRBool use_zone_allocator = PR_FALSE;

static void pr_ZoneFree(void *ptr);

void
_PR_DestroyZones(void)
{
    int i, j;

    if (!use_zone_allocator) {
        return;
    }

    for (j = 0; j < THREAD_POOLS; j++) {
        for (i = 0; i < MEM_ZONES; i++) {
            MemoryZone *mz = &zones[i][j];
            pthread_mutex_destroy(&mz->lock);
            while (mz->head) {
                MemBlockHdr *hdr = mz->head;
                mz->head = hdr->s.next;  /* unlink it */
                free(hdr);
                mz->elements--;
            }
        }
    }
    use_zone_allocator = PR_FALSE;
}

/*
** pr_FindSymbolInProg
**
** Find the specified data symbol in the program and return
** its address.
*/

#ifdef HAVE_DLL

#if defined(USE_DLFCN) && !defined(NO_DLOPEN_NULL)

#include <dlfcn.h>

static void *
pr_FindSymbolInProg(const char *name)
{
    void *h;
    void *sym;

    h = dlopen(0, RTLD_LAZY);
    if (h == NULL) {
        return NULL;
    }
    sym = dlsym(h, name);
    (void)dlclose(h);
    return sym;
}

#elif defined(USE_HPSHL)

#include <dl.h>

static void *
pr_FindSymbolInProg(const char *name)
{
    shl_t h = NULL;
    void *sym;

    if (shl_findsym(&h, name, TYPE_DATA, &sym) == -1) {
        return NULL;
    }
    return sym;
}

#elif defined(USE_MACH_DYLD) || defined(NO_DLOPEN_NULL)

static void *
pr_FindSymbolInProg(const char *name)
{
    /* FIXME: not implemented */
    return NULL;
}

#else

#error "The zone allocator is not supported on this platform"

#endif

#else /* !defined(HAVE_DLL) */

static void *
pr_FindSymbolInProg(const char *name)
{
    /* can't be implemented */
    return NULL;
}

#endif /* HAVE_DLL */

void
_PR_InitZones(void)
{
    int i, j;
    char *envp;
    PRBool *sym;

    if ((sym = (PRBool *)pr_FindSymbolInProg("nspr_use_zone_allocator")) != NULL) {
        use_zone_allocator = *sym;
    } else if ((envp = getenv("NSPR_USE_ZONE_ALLOCATOR")) != NULL) {
        use_zone_allocator = (atoi(envp) == 1);
    }

    if (!use_zone_allocator) {
        return;
    }

    for (j = 0; j < THREAD_POOLS; j++) {
        for (i = 0; i < MEM_ZONES; i++) {
            MemoryZone *mz = &zones[i][j];
            int rv = pthread_mutex_init(&mz->lock, NULL);
            PR_ASSERT(0 == rv);
            if (rv != 0) {
                goto loser;
            }
            mz->blockSize = 16 << ( 2 * i);
        }
    }
    return;

loser:
    _PR_DestroyZones();
    return;
}

PR_IMPLEMENT(void)
PR_FPrintZoneStats(PRFileDesc *debug_out)
{
    int i, j;

    for (j = 0; j < THREAD_POOLS; j++) {
        for (i = 0; i < MEM_ZONES; i++) {
            MemoryZone   *mz   = &zones[i][j];
            MemoryZone    zone = *mz;
            if (zone.elements || zone.misses || zone.hits) {
                PR_fprintf(debug_out,
                           "pool: %d, zone: %d, size: %d, free: %d, hit: %d, miss: %d, contend: %d\n",
                           j, i, zone.blockSize, zone.elements,
                           zone.hits, zone.misses, zone.contention);
            }
        }
    }
}

static void *
pr_ZoneMalloc(PRUint32 size)
{
    void         *rv;
    unsigned int  zone;
    size_t        blockSize;
    MemBlockHdr  *mb, *mt;
    MemoryZone   *mz;

    /* Always allocate a non-zero amount of bytes */
    if (size < 1) {
        size = 1;
    }
    for (zone = 0, blockSize = 16; zone < MEM_ZONES; ++zone, blockSize <<= 2) {
        if (size <= blockSize) {
            break;
        }
    }
    if (zone < MEM_ZONES) {
        pthread_t me = pthread_self();
        unsigned int pool = (PRUptrdiff)me % THREAD_POOLS;
        PRUint32     wasLocked;
        mz = &zones[zone][pool];
        wasLocked = mz->locked;
        pthread_mutex_lock(&mz->lock);
        mz->locked = 1;
        if (wasLocked) {
            mz->contention++;
        }
        if (mz->head) {
            mb = mz->head;
            PR_ASSERT(mb->s.magic == ZONE_MAGIC);
            PR_ASSERT(mb->s.zone  == mz);
            PR_ASSERT(mb->s.blockSize == blockSize);
            PR_ASSERT(mz->blockSize == blockSize);

            mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
            PR_ASSERT(mt->s.magic == ZONE_MAGIC);
            PR_ASSERT(mt->s.zone  == mz);
            PR_ASSERT(mt->s.blockSize == blockSize);

            mz->hits++;
            mz->elements--;
            mz->head = mb->s.next;    /* take off free list */
            mz->locked = 0;
            pthread_mutex_unlock(&mz->lock);

            mt->s.next          = mb->s.next          = NULL;
            mt->s.requestedSize = mb->s.requestedSize = size;

            rv = (void *)(mb + 1);
            return rv;
        }

        mz->misses++;
        mz->locked = 0;
        pthread_mutex_unlock(&mz->lock);

        mb = (MemBlockHdr *)malloc(blockSize + 2 * (sizeof *mb));
        if (!mb) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return NULL;
        }
        mb->s.next          = NULL;
        mb->s.zone          = mz;
        mb->s.magic         = ZONE_MAGIC;
        mb->s.blockSize     = blockSize;
        mb->s.requestedSize = size;

        mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
        memcpy(mt, mb, sizeof *mb);

        rv = (void *)(mb + 1);
        return rv;
    }

    /* size was too big.  Create a block with no zone */
    blockSize = (size & 15) ? size + 16 - (size & 15) : size;
    mb = (MemBlockHdr *)malloc(blockSize + 2 * (sizeof *mb));
    if (!mb) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    mb->s.next          = NULL;
    mb->s.zone          = NULL;
    mb->s.magic         = ZONE_MAGIC;
    mb->s.blockSize     = blockSize;
    mb->s.requestedSize = size;

    mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
    memcpy(mt, mb, sizeof *mb);

    rv = (void *)(mb + 1);
    return rv;
}


static void *
pr_ZoneCalloc(PRUint32 nelem, PRUint32 elsize)
{
    PRUint32 size = nelem * elsize;
    void *p = pr_ZoneMalloc(size);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

static void *
pr_ZoneRealloc(void *oldptr, PRUint32 bytes)
{
    void         *rv;
    MemBlockHdr  *mb;
    int           ours;
    MemBlockHdr   phony;

    if (!oldptr) {
        return pr_ZoneMalloc(bytes);
    }
    mb = (MemBlockHdr *)((char *)oldptr - (sizeof *mb));
    if (mb->s.magic != ZONE_MAGIC) {
        /* Maybe this just came from ordinary malloc */
#ifdef DEBUG
        fprintf(stderr,
                "Warning: reallocing memory block %p from ordinary malloc\n",
                oldptr);
#endif
        /*
         * We are going to realloc oldptr.  If realloc succeeds, the
         * original value of oldptr will point to freed memory.  So this
         * function must not fail after a successfull realloc call.  We
         * must perform any operation that may fail before the realloc
         * call.
         */
        rv = pr_ZoneMalloc(bytes);  /* this may fail */
        if (!rv) {
            return rv;
        }

        /* We don't know how big it is.  But we can fix that. */
        oldptr = realloc(oldptr, bytes);
        /*
         * If realloc returns NULL, this function loses the original
         * value of oldptr.  This isn't a leak because the caller of
         * this function still has the original value of oldptr.
         */
        if (!oldptr) {
            if (bytes) {
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                pr_ZoneFree(rv);
                return oldptr;
            }
        }
        phony.s.requestedSize = bytes;
        mb = &phony;
        ours = 0;
    } else {
        size_t blockSize = mb->s.blockSize;
        MemBlockHdr *mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);

        PR_ASSERT(mt->s.magic == ZONE_MAGIC);
        PR_ASSERT(mt->s.zone  == mb->s.zone);
        PR_ASSERT(mt->s.blockSize == blockSize);

        if (bytes <= blockSize) {
            /* The block is already big enough. */
            mt->s.requestedSize = mb->s.requestedSize = bytes;
            return oldptr;
        }
        ours = 1;
        rv = pr_ZoneMalloc(bytes);
        if (!rv) {
            return rv;
        }
    }

    if (oldptr && mb->s.requestedSize) {
        memcpy(rv, oldptr, mb->s.requestedSize);
    }
    if (ours) {
        pr_ZoneFree(oldptr);
    }
    else if (oldptr) {
        free(oldptr);
    }
    return rv;
}

static void
pr_ZoneFree(void *ptr)
{
    MemBlockHdr  *mb, *mt;
    MemoryZone   *mz;
    size_t        blockSize;
    PRUint32      wasLocked;

    if (!ptr) {
        return;
    }

    mb = (MemBlockHdr *)((char *)ptr - (sizeof *mb));

    if (mb->s.magic != ZONE_MAGIC) {
        /* maybe this came from ordinary malloc */
#ifdef DEBUG
        fprintf(stderr,
                "Warning: freeing memory block %p from ordinary malloc\n", ptr);
#endif
        free(ptr);
        return;
    }

    blockSize = mb->s.blockSize;
    mz        = mb->s.zone;
    mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
    PR_ASSERT(mt->s.magic == ZONE_MAGIC);
    PR_ASSERT(mt->s.zone  == mz);
    PR_ASSERT(mt->s.blockSize == blockSize);
    if (!mz) {
        PR_ASSERT(blockSize > 65536);
        /* This block was not in any zone.  Just free it. */
        free(mb);
        return;
    }
    PR_ASSERT(mz->blockSize == blockSize);
    wasLocked = mz->locked;
    pthread_mutex_lock(&mz->lock);
    mz->locked = 1;
    if (wasLocked) {
        mz->contention++;
    }
    mt->s.next = mb->s.next = mz->head;        /* put on head of list */
    mz->head = mb;
    mz->elements++;
    mz->locked = 0;
    pthread_mutex_unlock(&mz->lock);
}

PR_IMPLEMENT(void *) PR_Malloc(PRUint32 size)
{
    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    return use_zone_allocator ? pr_ZoneMalloc(size) : malloc(size);
}

PR_IMPLEMENT(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize)
{
    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    return use_zone_allocator ?
           pr_ZoneCalloc(nelem, elsize) : calloc(nelem, elsize);
}

PR_IMPLEMENT(void *) PR_Realloc(void *ptr, PRUint32 size)
{
    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    return use_zone_allocator ? pr_ZoneRealloc(ptr, size) : realloc(ptr, size);
}

PR_IMPLEMENT(void) PR_Free(void *ptr)
{
    if (use_zone_allocator) {
        pr_ZoneFree(ptr);
    }
    else {
        free(ptr);
    }
}

#else /* !defined(_PR_ZONE_ALLOCATOR) */

/*
** The PR_Malloc, PR_Calloc, PR_Realloc, and PR_Free functions simply
** call their libc equivalents now.  This may seem redundant, but it
** ensures that we are calling into the same runtime library.  On
** Win32, it is possible to have multiple runtime libraries (e.g.,
** objects compiled with /MD and /MDd) in the same process, and
** they maintain separate heaps, which cannot be mixed.
*/
PR_IMPLEMENT(void *) PR_Malloc(PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_malloc( (size_t) size);
#else
    return malloc(size);
#endif
}

PR_IMPLEMENT(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize)
{
#if defined (WIN16)
    return PR_MD_calloc( (size_t)nelem, (size_t)elsize );

#else
    return calloc(nelem, elsize);
#endif
}

PR_IMPLEMENT(void *) PR_Realloc(void *ptr, PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_realloc( ptr, (size_t) size);
#else
    return realloc(ptr, size);
#endif
}

PR_IMPLEMENT(void) PR_Free(void *ptr)
{
#if defined (WIN16)
    PR_MD_free( ptr );
#else
    free(ptr);
#endif
}

#endif /* _PR_ZONE_ALLOCATOR */

/*
** Complexity alert!
**
** If malloc/calloc/free (etc.) were implemented to use pr lock's then
** the entry points could block when called if some other thread had the
** lock.
**
** Most of the time this isn't a problem. However, in the case that we
** are using the thread safe malloc code after PR_Init but before
** PR_AttachThread has been called (on a native thread that nspr has yet
** to be told about) we could get royally screwed if the lock was busy
** and we tried to context switch the thread away. In this scenario
**  PR_CURRENT_THREAD() == NULL
**
** To avoid this unfortunate case, we use the low level locking
** facilities for malloc protection instead of the slightly higher level
** locking. This makes malloc somewhat faster so maybe it's a good thing
** anyway.
*/
#ifdef _PR_OVERRIDE_MALLOC

/* Imports */
extern void *_PR_UnlockedMalloc(size_t size);
extern void *_PR_UnlockedMemalign(size_t alignment, size_t size);
extern void _PR_UnlockedFree(void *ptr);
extern void *_PR_UnlockedRealloc(void *ptr, size_t size);
extern void *_PR_UnlockedCalloc(size_t n, size_t elsize);

static PRBool _PR_malloc_initialised = PR_FALSE;

#ifdef _PR_PTHREADS
static pthread_mutex_t _PR_MD_malloc_crustylock;

#define _PR_Lock_Malloc() {                     \
                    if(PR_TRUE == _PR_malloc_initialised) { \
                    PRStatus rv;            \
                    rv = pthread_mutex_lock(&_PR_MD_malloc_crustylock); \
                    PR_ASSERT(0 == rv);     \
                }

#define _PR_Unlock_Malloc()     if(PR_TRUE == _PR_malloc_initialised) { \
                    PRStatus rv;            \
                    rv = pthread_mutex_unlock(&_PR_MD_malloc_crustylock); \
                    PR_ASSERT(0 == rv);     \
                }                   \
              }
#else /* _PR_PTHREADS */
static _MDLock _PR_MD_malloc_crustylock;

#define _PR_Lock_Malloc() {                     \
               PRIntn _is;                  \
                    if(PR_TRUE == _PR_malloc_initialised) { \
                if (_PR_MD_CURRENT_THREAD() &&      \
                    !_PR_IS_NATIVE_THREAD(      \
                    _PR_MD_CURRENT_THREAD()))   \
                        _PR_INTSOFF(_is);   \
                    _PR_MD_LOCK(&_PR_MD_malloc_crustylock); \
                }

#define _PR_Unlock_Malloc()     if(PR_TRUE == _PR_malloc_initialised) { \
                    _PR_MD_UNLOCK(&_PR_MD_malloc_crustylock); \
                if (_PR_MD_CURRENT_THREAD() &&      \
                    !_PR_IS_NATIVE_THREAD(      \
                    _PR_MD_CURRENT_THREAD()))   \
                        _PR_INTSON(_is);    \
                }                   \
              }
#endif /* _PR_PTHREADS */

PR_IMPLEMENT(PRStatus) _PR_MallocInit(void)
{
    PRStatus rv = PR_SUCCESS;

    if( PR_TRUE == _PR_malloc_initialised ) {
        return PR_SUCCESS;
    }

#ifdef _PR_PTHREADS
    {
        int status;
        pthread_mutexattr_t mattr;

        status = _PT_PTHREAD_MUTEXATTR_INIT(&mattr);
        PR_ASSERT(0 == status);
        status = _PT_PTHREAD_MUTEX_INIT(_PR_MD_malloc_crustylock, mattr);
        PR_ASSERT(0 == status);
        status = _PT_PTHREAD_MUTEXATTR_DESTROY(&mattr);
        PR_ASSERT(0 == status);
    }
#else /* _PR_PTHREADS */
    _MD_NEW_LOCK(&_PR_MD_malloc_crustylock);
#endif /* _PR_PTHREADS */

    if( PR_SUCCESS == rv )
    {
        _PR_malloc_initialised = PR_TRUE;
    }

    return rv;
}

void *malloc(size_t size)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedMalloc(size);
    _PR_Unlock_Malloc();
    return p;
}

void free(void *ptr)
{
    _PR_Lock_Malloc();
    _PR_UnlockedFree(ptr);
    _PR_Unlock_Malloc();
}

void *realloc(void *ptr, size_t size)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedRealloc(ptr, size);
    _PR_Unlock_Malloc();
    return p;
}

void *calloc(size_t n, size_t elsize)
{
    void *p;
    _PR_Lock_Malloc();
    p = _PR_UnlockedCalloc(n, elsize);
    _PR_Unlock_Malloc();
    return p;
}

void cfree(void *p)
{
    _PR_Lock_Malloc();
    _PR_UnlockedFree(p);
    _PR_Unlock_Malloc();
}

void _PR_InitMem(void)
{
    PRStatus rv;
    rv = _PR_MallocInit();
    PR_ASSERT(PR_SUCCESS == rv);
}

#endif /* _PR_OVERRIDE_MALLOC */
