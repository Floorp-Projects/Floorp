/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim:cindent:ts=8:et:sw=4:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef NS_TRACE_MALLOC
 /*
  * TODO:
  * - FIXME https://bugzilla.mozilla.org/show_bug.cgi?id=392008
  * - extend logfile so 'F' record tells free stack
  */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef XP_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#endif
#include "plhash.h"
#include "pratom.h"
#include "prlog.h"
#include "prlock.h"
#include "prmon.h"
#include "prprf.h"
#include "prenv.h"
#include "prnetdb.h"
#include "nsTraceMalloc.h"
#include "nscore.h"
#include "prinit.h"
#include "prthread.h"
#include "plstr.h"
#include "nsStackWalk.h"
#include "nsTraceMallocCallbacks.h"
#include "nsTypeInfo.h"
#include "mozilla/mozPoisonWrite.h"

#if defined(XP_MACOSX)

#include <malloc/malloc.h>

#define WRITE_FLAGS "w"

#define __libc_malloc(x)                malloc(x)
#define __libc_realloc(x, y)            realloc(x, y)
#define __libc_free(x)                  free(x)

#elif defined(XP_UNIX)

#include <malloc.h>

#define WRITE_FLAGS "w"

#ifdef WRAP_SYSTEM_INCLUDES
#pragma GCC visibility push(default)
#endif
extern __ptr_t __libc_malloc(size_t);
extern __ptr_t __libc_calloc(size_t, size_t);
extern __ptr_t __libc_realloc(__ptr_t, size_t);
extern void    __libc_free(__ptr_t);
extern __ptr_t __libc_memalign(size_t, size_t);
extern __ptr_t __libc_valloc(size_t);
#ifdef WRAP_SYSTEM_INCLUDES
#pragma GCC visibility pop
#endif

#elif defined(XP_WIN32)

#include <sys/timeb.h>                  /* for timeb */
#include <sys/stat.h>                   /* for fstat */

#include <io.h> /*for write*/

#define WRITE_FLAGS "w"

#define __libc_malloc(x)                dhw_orig_malloc(x)
#define __libc_realloc(x, y)            dhw_orig_realloc(x,y)
#define __libc_free(x)                  dhw_orig_free(x)

#else  /* not XP_MACOSX, XP_UNIX, or XP_WIN32 */

# error "Unknown build configuration!"

#endif

typedef struct logfile logfile;

#define STARTUP_TMBUFSIZE (64 * 1024)
#define LOGFILE_TMBUFSIZE (16 * 1024)

struct logfile {
    int         fd;
    int         lfd;            /* logical fd, dense among all logfiles */
    char        *buf;
    int         bufsize;
    int         pos;
    uint32_t    size;
    uint32_t    simsize;
    logfile     *next;
    logfile     **prevp;
};

static char      default_buf[STARTUP_TMBUFSIZE];
static logfile   default_logfile =
                   {-1, 0, default_buf, STARTUP_TMBUFSIZE, 0, 0, 0, NULL, NULL};
static logfile   *logfile_list = NULL;
static logfile   **logfile_tail = &logfile_list;
static logfile   *logfp = &default_logfile;
static PRLock    *tmlock = NULL;
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
static char      sdlogname[PATH_MAX] = ""; /* filename for shutdown leak log */

/*
 * This enables/disables trace-malloc logging.
 *
 * It is separate from suppress_tracing so that we do not have to pay
 * the performance cost of repeated TM_TLS_GET_DATA calls when
 * trace-malloc is disabled (which is not as bad as the locking we used
 * to have).
 *
 * It must default to zero, since it can be tested by the Linux malloc
 * hooks before NS_TraceMallocStartup sets it.
 */
static uint32_t tracing_enabled = 0;

/*
 * Control whether we should log stacks
 */
static uint32_t stacks_enabled = 1;

/*
 * This lock must be held while manipulating the calltree, the
 * allocations table, the log, or the tmstats.
 *
 * Callers should not *enter* the lock without checking suppress_tracing
 * first; otherwise they risk trying to re-enter on the same thread.
 */
#define TM_ENTER_LOCK(t)                                                      \
    PR_BEGIN_MACRO                                                            \
        PR_ASSERT(t->suppress_tracing != 0);                                  \
        if (tmlock)                                                           \
            PR_Lock(tmlock);                                                  \
    PR_END_MACRO

#define TM_EXIT_LOCK(t)                                                       \
    PR_BEGIN_MACRO                                                            \
        PR_ASSERT(t->suppress_tracing != 0);                                  \
        if (tmlock)                                                           \
            PR_Unlock(tmlock);                                                \
    PR_END_MACRO

#define TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t)                                 \
    PR_BEGIN_MACRO                                                            \
        t->suppress_tracing++;                                                \
        TM_ENTER_LOCK(t);                                                     \
    PR_END_MACRO

#define TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t)                                \
    PR_BEGIN_MACRO                                                            \
        TM_EXIT_LOCK(t);                                                      \
        t->suppress_tracing--;                                                \
    PR_END_MACRO


/*
 * Thread-local storage.
 *
 * We can't use NSPR thread-local storage for this because it mallocs
 * within PR_GetThreadPrivate (the first time) and PR_SetThreadPrivate
 * (which can be worked around by protecting all uses of those functions
 * with a monitor, ugh) and because it calls malloc/free when the
 * thread-local storage is in an inconsistent state within
 * PR_SetThreadPrivate (when expanding the thread-local storage array)
 * and _PRI_DetachThread (when and after deleting the thread-local
 * storage array).
 */

#ifdef XP_WIN32

#include <windows.h>

#define TM_TLS_INDEX_TYPE               DWORD
#define TM_CREATE_TLS_INDEX(i_)         PR_BEGIN_MACRO                        \
                                          (i_) = TlsAlloc();                  \
                                        PR_END_MACRO
#define TM_DESTROY_TLS_INDEX(i_)        TlsFree((i_))
#define TM_GET_TLS_DATA(i_)             TlsGetValue((i_))
#define TM_SET_TLS_DATA(i_, v_)         TlsSetValue((i_), (v_))

#else

#include <pthread.h>

#define TM_TLS_INDEX_TYPE               pthread_key_t
#define TM_CREATE_TLS_INDEX(i_)         pthread_key_create(&(i_), NULL)
#define TM_DESTROY_TLS_INDEX(i_)        pthread_key_delete((i_))
#define TM_GET_TLS_DATA(i_)             pthread_getspecific((i_))
#define TM_SET_TLS_DATA(i_, v_)         pthread_setspecific((i_), (v_))

#endif

static TM_TLS_INDEX_TYPE tls_index;
static PRBool tls_index_initialized = PR_FALSE;

/* FIXME (maybe): This is currently unused; we leak the thread-local data. */
#if 0
static void
free_tm_thread(void *priv)
{
    tm_thread *t = (tm_thread*) priv;

    PR_ASSERT(t->suppress_tracing == 0);

    if (t->in_heap) {
        t->suppress_tracing = 1;
        if (t->backtrace_buf.buffer)
            __libc_free(t->backtrace_buf.buffer);

        __libc_free(t);
    }
}
#endif

tm_thread *
tm_get_thread(void)
{
    tm_thread *t;
    tm_thread stack_tm_thread;

    if (!tls_index_initialized) {
        /**
         * Assume that the first call to |malloc| will occur before
         * there are multiple threads.  (If that's not the case, we
         * probably need to do the necessary synchronization without
         * using NSPR primitives.  See discussion in
         * https://bugzilla.mozilla.org/show_bug.cgi?id=442192
         */
        TM_CREATE_TLS_INDEX(tls_index);
        tls_index_initialized = PR_TRUE;
    }

    t = TM_GET_TLS_DATA(tls_index);

    if (!t) {
        /*
         * First, store a tm_thread on the stack to suppress for the
         * malloc below
         */
        stack_tm_thread.suppress_tracing = 1;
        stack_tm_thread.backtrace_buf.buffer = NULL;
        stack_tm_thread.backtrace_buf.size = 0;
        stack_tm_thread.backtrace_buf.entries = 0;
        TM_SET_TLS_DATA(tls_index, &stack_tm_thread);

        t = (tm_thread*) __libc_malloc(sizeof(tm_thread));
        t->suppress_tracing = 0;
        t->backtrace_buf = stack_tm_thread.backtrace_buf;
        TM_SET_TLS_DATA(tls_index, t);

        PR_ASSERT(stack_tm_thread.suppress_tracing == 1); /* balanced */
    }

    return t;
}

/* We don't want more than 32 logfiles open at once, ok? */
typedef uint32_t        lfd_set;

#define LFD_SET_STATIC_INITIALIZER 0
#define LFD_SET_SIZE    32

#define LFD_ZERO(s)     (*(s) = 0)
#define LFD_BIT(i)      ((uint32_t)1 << (i))
#define LFD_TEST(i,s)   (LFD_BIT(i) & *(s))
#define LFD_SET(i,s)    (*(s) |= LFD_BIT(i))
#define LFD_CLR(i,s)    (*(s) &= ~LFD_BIT(i))

static logfile *get_logfile(int fd)
{
    logfile *fp;
    int lfd;

    for (fp = logfile_list; fp; fp = fp->next) {
        if (fp->fd == fd)
            return fp;
    }
    lfd = 0;
retry:
    for (fp = logfile_list; fp; fp = fp->next) {
        if (fp->fd == lfd) {
            if (++lfd >= LFD_SET_SIZE)
                return NULL;
            goto retry;
        }
    }
    fp = __libc_malloc(sizeof(logfile) + LOGFILE_TMBUFSIZE);
    if (!fp)
        return NULL;
    fp->fd = fd;
    fp->lfd = lfd;
    fp->buf = (char*) (fp + 1);
    fp->bufsize = LOGFILE_TMBUFSIZE;
    fp->pos = 0;
    fp->size = fp->simsize = 0;
    fp->next = NULL;
    fp->prevp = logfile_tail;
    *logfile_tail = fp;
    logfile_tail = &fp->next;
    return fp;
}

static void flush_logfile(logfile *fp)
{
    int len, cnt, fd;
    char *bp;

    len = fp->pos;
    if (len == 0)
        return;
    fp->pos = 0;
    fd = fp->fd;
    if (fd >= 0) {
        fp->size += len;
        bp = fp->buf;
        do {
            cnt = write(fd, bp, len);
            if (cnt <= 0) {
                printf("### nsTraceMalloc: write failed or wrote 0 bytes!\n");
                return;
            }
            bp += cnt;
            len -= cnt;
        } while (len > 0);
    }
    fp->simsize += len;
}

static void log_byte(logfile *fp, char byte)
{
    if (fp->pos == fp->bufsize)
        flush_logfile(fp);
    fp->buf[fp->pos++] = byte;
}

static void log_string(logfile *fp, const char *str)
{
    int len, rem, cnt;

    len = strlen(str) + 1; /* include null terminator */
    while ((rem = fp->pos + len - fp->bufsize) > 0) {
        cnt = len - rem;
        memcpy(&fp->buf[fp->pos], str, cnt);
        str += cnt;
        fp->pos += cnt;
        flush_logfile(fp);
        len = rem;
    }
    memcpy(&fp->buf[fp->pos], str, len);
    fp->pos += len;
}

static void log_filename(logfile* fp, const char* filename)
{
    if (strlen(filename) < 512) {
        char *bp, *cp, buf[512];

        bp = strstr(strcpy(buf, filename), "mozilla");
        if (!bp)
            bp = buf;

        for (cp = bp; *cp; cp++) {
            if (*cp == '\\')
                *cp = '/';
        }

        filename = bp;
    }
    log_string(fp, filename);
}

static void log_uint32(logfile *fp, uint32_t ival)
{
    if (ival < 0x80) {
        /* 0xxx xxxx */
        log_byte(fp, (char) ival);
    } else if (ival < 0x4000) {
        /* 10xx xxxx xxxx xxxx */
        log_byte(fp, (char) ((ival >> 8) | 0x80));
        log_byte(fp, (char) (ival & 0xff));
    } else if (ival < 0x200000) {
        /* 110x xxxx xxxx xxxx xxxx xxxx */
        log_byte(fp, (char) ((ival >> 16) | 0xc0));
        log_byte(fp, (char) ((ival >> 8) & 0xff));
        log_byte(fp, (char) (ival & 0xff));
    } else if (ival < 0x10000000) {
        /* 1110 xxxx xxxx xxxx xxxx xxxx xxxx xxxx */
        log_byte(fp, (char) ((ival >> 24) | 0xe0));
        log_byte(fp, (char) ((ival >> 16) & 0xff));
        log_byte(fp, (char) ((ival >> 8) & 0xff));
        log_byte(fp, (char) (ival & 0xff));
    } else {
        /* 1111 0000 xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx */
        log_byte(fp, (char) 0xf0);
        log_byte(fp, (char) ((ival >> 24) & 0xff));
        log_byte(fp, (char) ((ival >> 16) & 0xff));
        log_byte(fp, (char) ((ival >> 8) & 0xff));
        log_byte(fp, (char) (ival & 0xff));
    }
}

static void log_event1(logfile *fp, char event, uint32_t serial)
{
    log_byte(fp, event);
    log_uint32(fp, (uint32_t) serial);
}

static void log_event2(logfile *fp, char event, uint32_t serial, size_t size)
{
    log_event1(fp, event, serial);
    log_uint32(fp, (uint32_t) size);
}

static void log_event3(logfile *fp, char event, uint32_t serial, size_t oldsize,
                       size_t size)
{
    log_event2(fp, event, serial, oldsize);
    log_uint32(fp, (uint32_t) size);
}

static void log_event4(logfile *fp, char event, uint32_t serial, uint32_t ui2,
                       uint32_t ui3, uint32_t ui4)
{
    log_event3(fp, event, serial, ui2, ui3);
    log_uint32(fp, ui4);
}

static void log_event5(logfile *fp, char event, uint32_t serial, uint32_t ui2,
                       uint32_t ui3, uint32_t ui4, uint32_t ui5)
{
    log_event4(fp, event, serial, ui2, ui3, ui4);
    log_uint32(fp, ui5);
}

static void log_event6(logfile *fp, char event, uint32_t serial, uint32_t ui2,
                       uint32_t ui3, uint32_t ui4, uint32_t ui5, uint32_t ui6)
{
    log_event5(fp, event, serial, ui2, ui3, ui4, ui5);
    log_uint32(fp, ui6);
}

static void log_event7(logfile *fp, char event, uint32_t serial, uint32_t ui2,
                       uint32_t ui3, uint32_t ui4, uint32_t ui5, uint32_t ui6,
                       uint32_t ui7)
{
    log_event6(fp, event, serial, ui2, ui3, ui4, ui5, ui6);
    log_uint32(fp, ui7);
}

static void log_event8(logfile *fp, char event, uint32_t serial, uint32_t ui2,
                       uint32_t ui3, uint32_t ui4, uint32_t ui5, uint32_t ui6,
                       uint32_t ui7, uint32_t ui8)
{
    log_event7(fp, event, serial, ui2, ui3, ui4, ui5, ui6, ui7);
    log_uint32(fp, ui8);
}

typedef struct callsite callsite;

struct callsite {
    void*       pc;
    uint32_t      serial;
    lfd_set     lfdset;
    const char  *name;    /* pointer to string owned by methods table */
    const char  *library; /* pointer to string owned by libraries table */
    int         offset;
    callsite    *parent;
    callsite    *siblings;
    callsite    *kids;
};

/* NB: these counters are incremented and decremented only within tmlock. */
static uint32_t library_serial_generator = 0;
static uint32_t method_serial_generator = 0;
static uint32_t callsite_serial_generator = 0;
static uint32_t tmstats_serial_generator = 0;
static uint32_t filename_serial_generator = 0;

/* Root of the tree of callsites, the sum of all (cycle-compressed) stacks. */
static callsite calltree_root =
  {0, 0, LFD_SET_STATIC_INITIALIZER, NULL, NULL, 0, NULL, NULL, NULL};

/* a fake pc for when stacks are disabled; must be different from the
   pc in calltree_root */
#define STACK_DISABLED_PC ((void*)1)

/* Basic instrumentation. */
static nsTMStats tmstats = NS_TMSTATS_STATIC_INITIALIZER;

/* Parent with the most kids (tmstats.calltree_maxkids). */
static callsite *calltree_maxkids_parent;

/* Calltree leaf for path with deepest stack backtrace. */
static callsite *calltree_maxstack_top;

/* Last site (i.e., calling pc) that recurred during a backtrace. */
static callsite *last_callsite_recurrence;

static void log_tmstats(logfile *fp)
{
    log_event1(fp, TM_EVENT_STATS, ++tmstats_serial_generator);
    log_uint32(fp, tmstats.calltree_maxstack);
    log_uint32(fp, tmstats.calltree_maxdepth);
    log_uint32(fp, tmstats.calltree_parents);
    log_uint32(fp, tmstats.calltree_maxkids);
    log_uint32(fp, tmstats.calltree_kidhits);
    log_uint32(fp, tmstats.calltree_kidmisses);
    log_uint32(fp, tmstats.calltree_kidsteps);
    log_uint32(fp, tmstats.callsite_recurrences);
    log_uint32(fp, tmstats.backtrace_calls);
    log_uint32(fp, tmstats.backtrace_failures);
    log_uint32(fp, tmstats.btmalloc_failures);
    log_uint32(fp, tmstats.dladdr_failures);
    log_uint32(fp, tmstats.malloc_calls);
    log_uint32(fp, tmstats.malloc_failures);
    log_uint32(fp, tmstats.calloc_calls);
    log_uint32(fp, tmstats.calloc_failures);
    log_uint32(fp, tmstats.realloc_calls);
    log_uint32(fp, tmstats.realloc_failures);
    log_uint32(fp, tmstats.free_calls);
    log_uint32(fp, tmstats.null_free_calls);
    log_uint32(fp, calltree_maxkids_parent ? calltree_maxkids_parent->serial
                                           : 0);
    log_uint32(fp, calltree_maxstack_top ? calltree_maxstack_top->serial : 0);
}

static void *generic_alloctable(void *pool, size_t size)
{
    return __libc_malloc(size);
}

static void generic_freetable(void *pool, void *item)
{
    __libc_free(item);
}

typedef struct lfdset_entry {
    PLHashEntry base;
    lfd_set     lfdset;
} lfdset_entry;

static PLHashEntry *lfdset_allocentry(void *pool, const void *key)
{
    lfdset_entry *le = __libc_malloc(sizeof *le);
    if (le)
        LFD_ZERO(&le->lfdset);
    return &le->base;
}

static void lfdset_freeentry(void *pool, PLHashEntry *he, unsigned flag)
{
    lfdset_entry *le;

    if (flag != HT_FREE_ENTRY)
        return;
    le = (lfdset_entry*) he;
    __libc_free((void*) le);
}

static PLHashAllocOps lfdset_hashallocops = {
    generic_alloctable, generic_freetable,
    lfdset_allocentry,  lfdset_freeentry
};

/* Table of library pathnames mapped to to logged 'L' record serial numbers. */
static PLHashTable *libraries = NULL;

/* Table of filename pathnames mapped to logged 'G' record serial numbers. */
static PLHashTable *filenames = NULL;

/* Table mapping method names to logged 'N' record serial numbers. */
static PLHashTable *methods = NULL;

/*
 * Presumes that its caller is holding tmlock, but may temporarily exit
 * the lock.
 */
static callsite *
calltree(void **stack, size_t num_stack_entries, tm_thread *t)
{
    logfile *fp = logfp;
    void *pc;
    uint32_t nkids;
    callsite *parent, *site, **csp, *tmp;
    int maxstack;
    uint32_t library_serial, method_serial, filename_serial;
    const char *library, *method, *filename;
    char *slash;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    lfdset_entry *le;
    size_t stack_index;
    nsCodeAddressDetails details;
    nsresult rv;

    maxstack = (num_stack_entries > tmstats.calltree_maxstack);
    if (maxstack) {
        /* these two are the same, although that used to be less clear */
        tmstats.calltree_maxstack = num_stack_entries;
        tmstats.calltree_maxdepth = num_stack_entries;
    }

    /* Reverse the stack again, finding and building a path in the tree. */
    parent = &calltree_root;
    stack_index = num_stack_entries;
    do {
        --stack_index;
        pc = stack[stack_index];

        csp = &parent->kids;
        while ((site = *csp) != NULL) {
            if (site->pc == pc) {
                tmstats.calltree_kidhits++;

                /* Put the most recently used site at the front of siblings. */
                *csp = site->siblings;
                site->siblings = parent->kids;
                parent->kids = site;

                /* Check whether we've logged for this site and logfile yet. */
                if (!LFD_TEST(fp->lfd, &site->lfdset)) {
                    /*
                     * Some other logfile put this site in the calltree.  We
                     * must log an event for site, and possibly first for its
                     * method and/or library.  Note the code after the while
                     * loop that tests if (!site).
                     */
                    break;
                }

                /* Site already built and logged to fp -- go up the stack. */
                goto upward;
            }
            tmstats.calltree_kidsteps++;
            csp = &site->siblings;
        }

        if (!site) {
            tmstats.calltree_kidmisses++;

            /* Check for recursion: see if pc is on our ancestor line. */
            for (site = parent; site; site = site->parent) {
                if (site->pc == pc) {
                    tmstats.callsite_recurrences++;
                    last_callsite_recurrence = site;
                    goto upward;
                }
            }
        }

        /*
         * Not in tree at all, or not logged to fp: let's find our symbolic
         * callsite info.
         */

        if (!stacks_enabled) {
            /*
             * Fake the necessary information for our single fake stack
             * frame.
             */
            PL_strncpyz(details.library, "stacks_disabled",
                        sizeof(details.library));
            details.loffset = 0;
            details.filename[0] = '\0';
            details.lineno = 0;
            details.function[0] = '\0';
            details.foffset = 0;
        } else {
            /*
             * NS_DescribeCodeAddress can (on Linux) acquire a lock inside
             * the shared library loader.  Another thread might call malloc
             * while holding that lock (when loading a shared library).  So
             * we have to exit tmlock around this call.  For details, see
             * https://bugzilla.mozilla.org/show_bug.cgi?id=363334#c3
             *
             * We could be more efficient by building the nodes in the
             * calltree, exiting the monitor once to describe all of them,
             * and then filling in the descriptions for any that hadn't been
             * described already.  But this is easier for now.
             */
            TM_EXIT_LOCK(t);
            rv = NS_DescribeCodeAddress(pc, &details);
            TM_ENTER_LOCK(t);
            if (NS_FAILED(rv)) {
                tmstats.dladdr_failures++;
                goto fail;
            }
        }

        /* Check whether we need to emit a library trace record. */
        library_serial = 0;
        library = NULL;
        if (details.library[0]) {
            if (!libraries) {
                libraries = PL_NewHashTable(100, PL_HashString,
                                            PL_CompareStrings, PL_CompareValues,
                                            &lfdset_hashallocops, NULL);
                if (!libraries) {
                    tmstats.btmalloc_failures++;
                    goto fail;
                }
            }
            hash = PL_HashString(details.library);
            hep = PL_HashTableRawLookup(libraries, hash, details.library);
            he = *hep;
            if (he) {
                library = (char*) he->key;
                library_serial = (uint32_t) NS_PTR_TO_INT32(he->value);
                le = (lfdset_entry *) he;
                if (LFD_TEST(fp->lfd, &le->lfdset)) {
                    /* We already logged an event on fp for this library. */
                    le = NULL;
                }
            } else {
                library = strdup(details.library);
                if (library) {
                    library_serial = ++library_serial_generator;
                    he = PL_HashTableRawAdd(libraries, hep, hash, library,
                                            NS_INT32_TO_PTR(library_serial));
                }
                if (!he) {
                    tmstats.btmalloc_failures++;
                    goto fail;
                }
                le = (lfdset_entry *) he;
            }
            if (le) {
                /* Need to log an event to fp for this lib. */
                slash = strrchr(library, '/');
                log_event1(fp, TM_EVENT_LIBRARY, library_serial);
                log_string(fp, slash ? slash + 1 : library);
                LFD_SET(fp->lfd, &le->lfdset);
            }
        }

        /* For compatibility with current log format, always emit a
         * filename trace record, using "noname" / 0 when no file name
         * is available. */
        filename_serial = 0;
        filename = details.filename[0] ? details.filename : "noname";
        if (!filenames) {
            filenames = PL_NewHashTable(100, PL_HashString,
                                        PL_CompareStrings, PL_CompareValues,
                                        &lfdset_hashallocops, NULL);
            if (!filenames) {
                tmstats.btmalloc_failures++;
                return NULL;
            }
        }
        hash = PL_HashString(filename);
        hep = PL_HashTableRawLookup(filenames, hash, filename);
        he = *hep;
        if (he) {
            filename = (char*) he->key;
            filename_serial = (uint32_t) NS_PTR_TO_INT32(he->value);
            le = (lfdset_entry *) he;
            if (LFD_TEST(fp->lfd, &le->lfdset)) {
                /* We already logged an event on fp for this filename. */
                le = NULL;
            }
        } else {
            filename = strdup(filename);
            if (filename) {
                filename_serial = ++filename_serial_generator;
                he = PL_HashTableRawAdd(filenames, hep, hash, filename,
                                        NS_INT32_TO_PTR(filename_serial));
            }
            if (!he) {
                tmstats.btmalloc_failures++;
                return NULL;
            }
            le = (lfdset_entry *) he;
        }
        if (le) {
            /* Need to log an event to fp for this filename. */
            log_event1(fp, TM_EVENT_FILENAME, filename_serial);
            log_filename(fp, filename);
            LFD_SET(fp->lfd, &le->lfdset);
        }

        if (!details.function[0]) {
            PR_snprintf(details.function, sizeof(details.function),
                        "%s+%X", library ? library : "main", details.loffset);
        }

        /* Emit an 'N' (for New method, 'M' is for malloc!) event if needed. */
        method_serial = 0;
        if (!methods) {
            methods = PL_NewHashTable(10000, PL_HashString,
                                      PL_CompareStrings, PL_CompareValues,
                                      &lfdset_hashallocops, NULL);
            if (!methods) {
                tmstats.btmalloc_failures++;
                goto fail;
            }
        }
        hash = PL_HashString(details.function);
        hep = PL_HashTableRawLookup(methods, hash, details.function);
        he = *hep;
        if (he) {
            method = (char*) he->key;
            method_serial = (uint32_t) NS_PTR_TO_INT32(he->value);
            le = (lfdset_entry *) he;
            if (LFD_TEST(fp->lfd, &le->lfdset)) {
                /* We already logged an event on fp for this method. */
                le = NULL;
            }
        } else {
            method = strdup(details.function);
            if (method) {
                method_serial = ++method_serial_generator;
                he = PL_HashTableRawAdd(methods, hep, hash, method,
                                        NS_INT32_TO_PTR(method_serial));
            }
            if (!he) {
                tmstats.btmalloc_failures++;
                return NULL;
            }
            le = (lfdset_entry *) he;
        }
        if (le) {
            log_event4(fp, TM_EVENT_METHOD, method_serial, library_serial,
                       filename_serial, details.lineno);
            log_string(fp, method);
            LFD_SET(fp->lfd, &le->lfdset);
        }

        /* Create a new callsite record. */
        if (!site) {
            site = __libc_malloc(sizeof(callsite));
            if (!site) {
                tmstats.btmalloc_failures++;
                goto fail;
            }

            /* Update parent and max-kids-per-parent stats. */
            if (!parent->kids)
                tmstats.calltree_parents++;
            nkids = 1;
            for (tmp = parent->kids; tmp; tmp = tmp->siblings)
                nkids++;
            if (nkids > tmstats.calltree_maxkids) {
                tmstats.calltree_maxkids = nkids;
                calltree_maxkids_parent = parent;
            }

            /* Insert the new site into the tree. */
            site->pc = pc;
            site->serial = ++callsite_serial_generator;
            LFD_ZERO(&site->lfdset);
            site->name = method;
            site->library = library;
            site->offset = details.loffset;
            site->parent = parent;
            site->siblings = parent->kids;
            parent->kids = site;
            site->kids = NULL;
        }

        /* Log the site with its parent, method, and offset. */
        log_event4(fp, TM_EVENT_CALLSITE, site->serial, parent->serial,
                   method_serial, details.foffset);
        LFD_SET(fp->lfd, &site->lfdset);

      upward:
        parent = site;
    } while (stack_index > 0);

    if (maxstack)
        calltree_maxstack_top = site;

    return site;

  fail:
    return NULL;
}

/*
 * Buffer the stack from top at low index to bottom at high, so that we can
 * reverse it in calltree.
 */
static void
stack_callback(void *pc, void *sp, void *closure)
{
    stack_buffer_info *info = (stack_buffer_info*) closure;

    /*
     * If we run out of buffer, keep incrementing entries so that
     * backtrace can call us again with a bigger buffer.
     */
    if (info->entries < info->size)
        info->buffer[info->entries] = pc;
    ++info->entries;
}

/*
 * The caller MUST NOT be holding tmlock when calling backtrace.
 * On return, if *immediate_abort is set, then the return value is NULL
 * and the thread is in a very dangerous situation (e.g. holding
 * sem_pool_lock in Mac OS X pthreads); the caller should bail out
 * without doing anything (such as acquiring locks).
 */
static callsite *
backtrace(tm_thread *t, int skip, int *immediate_abort)
{
    callsite *site;
    stack_buffer_info *info = &t->backtrace_buf;
    void ** new_stack_buffer;
    size_t new_stack_buffer_size;
    nsresult rv;

    t->suppress_tracing++;

    if (!stacks_enabled) {
#if defined(XP_MACOSX)
        /* Walk the stack, even if stacks_enabled is false. We do this to
           check if we must set immediate_abort. */
        info->entries = 0;
        rv = NS_StackWalk(stack_callback, skip, info, 0, NULL);
        *immediate_abort = rv == NS_ERROR_UNEXPECTED;
        if (rv == NS_ERROR_UNEXPECTED || info->entries == 0) {
            t->suppress_tracing--;
            return NULL;
        }
#endif

        /*
         * Create a single fake stack frame so that all the tools get
         * data in the correct format.
         */
        *immediate_abort = 0;
        if (info->size < 1) {
            PR_ASSERT(!info->buffer); /* !info->size == !info->buffer */
            info->buffer = __libc_malloc(1 * sizeof(void*));
            if (!info->buffer)
                return NULL;
            info->size = 1;
        }

        info->entries = 1;
        info->buffer[0] = STACK_DISABLED_PC;
    } else {
        /*
         * NS_StackWalk can (on Windows) acquire a lock the shared library
         * loader.  Another thread might call malloc while holding that lock
         * (when loading a shared library).  So we can't be in tmlock during
         * this call.  For details, see
         * https://bugzilla.mozilla.org/show_bug.cgi?id=374829#c8
         */

        /* skip == 0 means |backtrace| should show up, so don't use skip + 1 */
        /* NB: this call is repeated below if the buffer is too small */
        info->entries = 0;
        rv = NS_StackWalk(stack_callback, skip, info, 0, NULL);
        *immediate_abort = rv == NS_ERROR_UNEXPECTED;
        if (rv == NS_ERROR_UNEXPECTED || info->entries == 0) {
            t->suppress_tracing--;
            return NULL;
        }

        /*
         * To avoid allocating in stack_callback (which, on Windows, is
         * called on a different thread from the one we're running on here),
         * reallocate here if it didn't have a big enough buffer (which
         * includes the first call on any thread), and call it again.
         */
        if (info->entries > info->size) {
            new_stack_buffer_size = 2 * info->entries;
            new_stack_buffer = __libc_realloc(info->buffer,
                                   new_stack_buffer_size * sizeof(void*));
            if (!new_stack_buffer)
                return NULL;
            info->buffer = new_stack_buffer;
            info->size = new_stack_buffer_size;

            /* and call NS_StackWalk again */
            info->entries = 0;
            NS_StackWalk(stack_callback, skip, info, 0, NULL);

            /* same stack */
            PR_ASSERT(info->entries * 2 == new_stack_buffer_size);
        }
    }

    TM_ENTER_LOCK(t);

    site = calltree(info->buffer, info->entries, t);

    tmstats.backtrace_calls++;
    if (!site) {
        tmstats.backtrace_failures++;
        PR_ASSERT(tmstats.backtrace_failures < 100);
    }
    TM_EXIT_LOCK(t);

    t->suppress_tracing--;
    return site;
}

typedef struct allocation {
    PLHashEntry entry;
    size_t      size;
    FILE        *trackfp;       /* for allocation tracking */
} allocation;

#define ALLOC_HEAP_SIZE 150000

static allocation alloc_heap[ALLOC_HEAP_SIZE];
static allocation *alloc_freelist = NULL;
static int alloc_heap_initialized = 0;

static PLHashEntry *alloc_allocentry(void *pool, const void *key)
{
    allocation **listp, *alloc;
    int n;

    if (!alloc_heap_initialized) {
        n = ALLOC_HEAP_SIZE;
        listp = &alloc_freelist;
        for (alloc = alloc_heap; --n >= 0; alloc++) {
            *listp = alloc;
            listp = (allocation**) &alloc->entry.next;
        }
        *listp = NULL;
        alloc_heap_initialized = 1;
    }

    listp = &alloc_freelist;
    alloc = *listp;
    if (!alloc)
        return __libc_malloc(sizeof(allocation));
    *listp = (allocation*) alloc->entry.next;
    return &alloc->entry;
}

static void alloc_freeentry(void *pool, PLHashEntry *he, unsigned flag)
{
    allocation *alloc;

    if (flag != HT_FREE_ENTRY)
        return;
    alloc = (allocation*) he;
    if ((ptrdiff_t)(alloc - alloc_heap) < (ptrdiff_t)ALLOC_HEAP_SIZE) {
        alloc->entry.next = &alloc_freelist->entry;
        alloc_freelist = alloc;
    } else {
        __libc_free((void*) alloc);
    }
}

static PLHashAllocOps alloc_hashallocops = {
    generic_alloctable, generic_freetable,
    alloc_allocentry,   alloc_freeentry
};

static PLHashNumber hash_pointer(const void *key)
{
    return (PLHashNumber) key;
}

static PLHashTable *allocations = NULL;

static PLHashTable *new_allocations(void)
{
    allocations = PL_NewHashTable(200000, hash_pointer,
                                  PL_CompareValues, PL_CompareValues,
                                  &alloc_hashallocops, NULL);
    return allocations;
}

#define get_allocations() (allocations ? allocations : new_allocations())

#if defined(XP_MACOSX)

/* from malloc.c in Libc */
typedef void
malloc_logger_t(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                uintptr_t result, uint32_t num_hot_frames_to_skip);

extern malloc_logger_t *malloc_logger;

#define MALLOC_LOG_TYPE_ALLOCATE        2
#define MALLOC_LOG_TYPE_DEALLOCATE      4
#define MALLOC_LOG_TYPE_HAS_ZONE        8
#define MALLOC_LOG_TYPE_CLEARED         64

static void
my_malloc_logger(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                 uintptr_t result, uint32_t num_hot_frames_to_skip)
{
    uintptr_t all_args[3] = { arg1, arg2, arg3 };
    uintptr_t *args = all_args + ((type & MALLOC_LOG_TYPE_HAS_ZONE) ? 1 : 0);

    uint32_t alloc_type =
        type & (MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_DEALLOCATE);
    tm_thread *t = tm_get_thread();

    if (alloc_type == (MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_DEALLOCATE)) {
        ReallocCallback((void*)args[0], (void*)result, args[1], 0, 0, t);
    } else if (alloc_type == MALLOC_LOG_TYPE_ALLOCATE) {
        /*
         * We don't get size/count information for calloc, so just use
         * MallocCallback.
         */
        MallocCallback((void*)result, args[0], 0, 0, t);
    } else if (alloc_type == MALLOC_LOG_TYPE_DEALLOCATE) {
        FreeCallback((void*)args[0], 0, 0, t);
    }
}

static void
StartupHooker(void)
{
    PR_ASSERT(!malloc_logger);
    malloc_logger = my_malloc_logger;
}

static void
ShutdownHooker(void)
{
    PR_ASSERT(malloc_logger == my_malloc_logger);
    malloc_logger = NULL;
}

#elif defined(XP_UNIX)

/*
 * We can't use glibc's malloc hooks because they can't be used in a
 * threadsafe manner.  They require unsetting the hooks to call into the
 * original malloc implementation, and then resetting them when the
 * original implementation returns.  If another thread calls the same
 * allocation function while the hooks are unset, we have no chance to
 * intercept the call.
 */

NS_EXTERNAL_VIS_(__ptr_t)
malloc(size_t size)
{
    uint32_t start, end;
    __ptr_t ptr;
    tm_thread *t;

    if (!tracing_enabled || !PR_Initialized() ||
        (t = tm_get_thread())->suppress_tracing != 0) {
        return __libc_malloc(size);
    }

    t->suppress_tracing++;
    start = PR_IntervalNow();
    ptr = __libc_malloc(size);
    end = PR_IntervalNow();
    t->suppress_tracing--;

    MallocCallback(ptr, size, start, end, t);

    return ptr;
}

NS_EXTERNAL_VIS_(__ptr_t)
calloc(size_t count, size_t size)
{
    uint32_t start, end;
    __ptr_t ptr;
    tm_thread *t;

    if (!tracing_enabled || !PR_Initialized() ||
        (t = tm_get_thread())->suppress_tracing != 0) {
        return __libc_calloc(count, size);
    }

    t->suppress_tracing++;
    start = PR_IntervalNow();
    ptr = __libc_calloc(count, size);
    end = PR_IntervalNow();
    t->suppress_tracing--;

    CallocCallback(ptr, count, size, start, end, t);

    return ptr;
}

NS_EXTERNAL_VIS_(__ptr_t)
realloc(__ptr_t oldptr, size_t size)
{
    uint32_t start, end;
    __ptr_t ptr;
    tm_thread *t;

    if (!tracing_enabled || !PR_Initialized() ||
        (t = tm_get_thread())->suppress_tracing != 0) {
        return __libc_realloc(oldptr, size);
    }

    t->suppress_tracing++;
    start = PR_IntervalNow();
    ptr = __libc_realloc(oldptr, size);
    end = PR_IntervalNow();
    t->suppress_tracing--;

    /* FIXME bug 392008: We could race with reallocation of oldptr. */
    ReallocCallback(oldptr, ptr, size, start, end, t);

    return ptr;
}

NS_EXTERNAL_VIS_(void*)
valloc(size_t size)
{
    uint32_t start, end;
    __ptr_t ptr;
    tm_thread *t;

    if (!tracing_enabled || !PR_Initialized() ||
        (t = tm_get_thread())->suppress_tracing != 0) {
        return __libc_valloc(size);
    }

    t->suppress_tracing++;
    start = PR_IntervalNow();
    ptr = __libc_valloc(size);
    end = PR_IntervalNow();
    t->suppress_tracing--;

    MallocCallback(ptr, size, start, end, t);

    return ptr;
}

NS_EXTERNAL_VIS_(void*)
memalign(size_t boundary, size_t size)
{
    uint32_t start, end;
    __ptr_t ptr;
    tm_thread *t;

    if (!tracing_enabled || !PR_Initialized() ||
        (t = tm_get_thread())->suppress_tracing != 0) {
        return __libc_memalign(boundary, size);
    }

    t->suppress_tracing++;
    start = PR_IntervalNow();
    ptr = __libc_memalign(boundary, size);
    end = PR_IntervalNow();
    t->suppress_tracing--;

    MallocCallback(ptr, size, start, end, t);

    return ptr;
}

NS_EXTERNAL_VIS_(int)
posix_memalign(void **memptr, size_t alignment, size_t size)
{
    __ptr_t ptr = memalign(alignment, size);
    if (!ptr)
        return ENOMEM;
    *memptr = ptr;
    return 0;
}

NS_EXTERNAL_VIS_(void)
free(__ptr_t ptr)
{
    uint32_t start, end;
    tm_thread *t;

    if (!tracing_enabled || !PR_Initialized() ||
        (t = tm_get_thread())->suppress_tracing != 0) {
        __libc_free(ptr);
        return;
    }

    t->suppress_tracing++;
    start = PR_IntervalNow();
    __libc_free(ptr);
    end = PR_IntervalNow();
    t->suppress_tracing--;

    /* FIXME bug 392008: We could race with reallocation of ptr. */

    FreeCallback(ptr, start, end, t);
}

NS_EXTERNAL_VIS_(void)
cfree(void *ptr)
{
    free(ptr);
}

#define StartupHooker()                 PR_BEGIN_MACRO PR_END_MACRO
#define ShutdownHooker()                PR_BEGIN_MACRO PR_END_MACRO

#elif defined(XP_WIN32)

/* See nsWinTraceMalloc.cpp. */

#endif

static const char magic[] = NS_TRACE_MALLOC_MAGIC;

static void
log_header(int logfd)
{
    uint32_t ticksPerSec = PR_htonl(PR_TicksPerSecond());
    (void) write(logfd, magic, NS_TRACE_MALLOC_MAGIC_SIZE);
    (void) write(logfd, &ticksPerSec, sizeof ticksPerSec);
}

PR_IMPLEMENT(void)
NS_TraceMallocStartup(int logfd)
{
    const char* stack_disable_env;

    /* We must be running on the primordial thread. */
    PR_ASSERT(tracing_enabled == 0);
    PR_ASSERT(logfp == &default_logfile);
    tracing_enabled = (logfd >= 0);

    if (logfd >= 3)
        MozillaRegisterDebugFD(logfd);

    /* stacks are disabled if this env var is set to a non-empty value */
    stack_disable_env = PR_GetEnv("NS_TRACE_MALLOC_DISABLE_STACKS");
    stacks_enabled = !stack_disable_env || !*stack_disable_env;

    if (tracing_enabled) {
        PR_ASSERT(logfp->simsize == 0); /* didn't overflow startup buffer */

        /* Log everything in logfp (aka default_logfile)'s buffer to logfd. */
        logfp->fd = logfd;
        logfile_list = &default_logfile;
        logfp->prevp = &logfile_list;
        logfile_tail = &logfp->next;
        log_header(logfd);
    }

    RegisterTraceMallocShutdown();

    tmlock = PR_NewLock();
    (void) tm_get_thread(); /* ensure index initialization while it's easy */

    if (tracing_enabled)
        StartupHooker();
}

/*
 * Options for log files, with the log file name either as the next option
 * or separated by '=' (e.g. "./mozilla --trace-malloc * malloc.log" or
 * "./mozilla --trace-malloc=malloc.log").
 */
static const char TMLOG_OPTION[] = "--trace-malloc";
static const char SDLOG_OPTION[] = "--shutdown-leaks";

#define SHOULD_PARSE_ARG(name_, log_, arg_) \
    (0 == strncmp(arg_, name_, sizeof(name_) - 1))

#define PARSE_ARG(name_, log_, argv_, i_, consumed_)                          \
    PR_BEGIN_MACRO                                                            \
        char _nextchar = argv_[i_][sizeof(name_) - 1];                        \
        if (_nextchar == '=') {                                               \
            log_ = argv_[i_] + sizeof(name_);                                 \
            consumed_ = 1;                                                    \
        } else if (_nextchar == '\0') {                                       \
            log_ = argv_[i_+1];                                               \
            consumed_ = 2;                                                    \
        }                                                                     \
    PR_END_MACRO

PR_IMPLEMENT(int)
NS_TraceMallocStartupArgs(int argc, char **argv)
{
    int i, logfd = -1, consumed, logflags;
    char *tmlogname = NULL, *sdlogname_local = NULL;

    /*
     * Look for the --trace-malloc <logfile> option early, to avoid missing
     * early mallocs (we miss static constructors whose output overflows the
     * log file's static 16K output buffer).
     */
    for (i = 1; i < argc; i += consumed) {
        consumed = 0;
        if (SHOULD_PARSE_ARG(TMLOG_OPTION, tmlogname, argv[i]))
            PARSE_ARG(TMLOG_OPTION, tmlogname, argv, i, consumed);
        else if (SHOULD_PARSE_ARG(SDLOG_OPTION, sdlogname_local, argv[i]))
            PARSE_ARG(SDLOG_OPTION, sdlogname_local, argv, i, consumed);

        if (consumed) {
#ifndef XP_WIN32 /* If we don't comment this out, it will crash Windows. */
            int j;
            /* Now remove --trace-malloc and its argument from argv. */
            argc -= consumed;
            for (j = i; j < argc; ++j)
                argv[j] = argv[j+consumed];
            argv[argc] = NULL;
            consumed = 0; /* don't advance next iteration */
#endif
        } else {
            consumed = 1;
        }
    }

    if (tmlogname) {
#ifdef XP_UNIX
        int pipefds[2];
#endif

        switch (*tmlogname) {
#ifdef XP_UNIX
          case '|':
            if (pipe(pipefds) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    /* In child: set up stdin, parse args, and exec. */
                    int maxargc, nargc;
                    char **nargv, *token;

                    if (pipefds[0] != 0) {
                        dup2(pipefds[0], 0);
                        close(pipefds[0]);
                    }
                    close(pipefds[1]);

                    tmlogname = strtok(tmlogname + 1, " \t");
                    maxargc = 3;
                    nargv = (char **) malloc((maxargc+1) * sizeof(char *));
                    if (!nargv) exit(1);
                    nargc = 0;
                    nargv[nargc++] = tmlogname;
                    while ((token = strtok(NULL, " \t")) != NULL) {
                        if (nargc == maxargc) {
                            maxargc *= 2;
                            nargv = (char**)
                                realloc(nargv, (maxargc+1) * sizeof(char*));
                            if (!nargv) exit(1);
                        }
                        nargv[nargc++] = token;
                    }
                    nargv[nargc] = NULL;

                    (void) setsid();
                    execvp(tmlogname, nargv);
                    exit(127);
                }

                if (pid > 0) {
                    /* In parent: set logfd to the pipe's write side. */
                    close(pipefds[0]);
                    logfd = pipefds[1];
                }
            }
            if (logfd < 0) {
                fprintf(stderr,
                    "%s: can't pipe to trace-malloc child process %s: %s\n",
                    argv[0], tmlogname, strerror(errno));
                exit(1);
            }
            break;
#endif /*XP_UNIX*/
          case '-':
            /* Don't log from startup, but do prepare to log later. */
            /* XXX traditional meaning of '-' as option argument is "stdin" or "stdout" */
            if (tmlogname[1] == '\0')
                break;
            /* FALL THROUGH */

          default:
            logflags = O_CREAT | O_WRONLY | O_TRUNC;
#if defined(XP_WIN32)
            /*
             * Avoid translations on WIN32.
             */
            logflags |= O_BINARY;
#endif
            logfd = open(tmlogname, logflags, 0644);
            if (logfd < 0) {
                fprintf(stderr,
                    "%s: can't create trace-malloc log named %s: %s\n",
                    argv[0], tmlogname, strerror(errno));
                exit(1);
            }
            break;
        }
    }

    if (sdlogname_local) {
        strncpy(sdlogname, sdlogname_local, sizeof(sdlogname));
        sdlogname[sizeof(sdlogname) - 1] = '\0';
    }

    NS_TraceMallocStartup(logfd);
    return argc;
}

PR_IMPLEMENT(PRBool)
NS_TraceMallocHasStarted(void)
{
    return tmlock ? PR_TRUE : PR_FALSE;
}

PR_IMPLEMENT(void)
NS_TraceMallocShutdown(void)
{
    logfile *fp;

    if (sdlogname[0])
        NS_TraceMallocDumpAllocations(sdlogname);

    if (tmstats.backtrace_failures) {
        fprintf(stderr,
                "TraceMalloc backtrace failures: %lu (malloc %lu dladdr %lu)\n",
                (unsigned long) tmstats.backtrace_failures,
                (unsigned long) tmstats.btmalloc_failures,
                (unsigned long) tmstats.dladdr_failures);
    }
    while ((fp = logfile_list) != NULL) {
        logfile_list = fp->next;
        log_tmstats(fp);
        flush_logfile(fp);
        if (fp->fd >= 0) {
            MozillaUnRegisterDebugFD(fp->fd);
            close(fp->fd);
            fp->fd = -1;
        }
        if (fp != &default_logfile) {
            if (fp == logfp)
                logfp = &default_logfile;
            free((void*) fp);
        }
    }
    if (tmlock) {
        PRLock *lock = tmlock;
        tmlock = NULL;
        PR_DestroyLock(lock);
    }
    if (tracing_enabled) {
        tracing_enabled = 0;
        ShutdownHooker();
    }
}

PR_IMPLEMENT(void)
NS_TraceMallocDisable(void)
{
    tm_thread *t = tm_get_thread();
    logfile *fp;
    uint32_t sample;

    /* Robustify in case of duplicate call. */
    PR_ASSERT(tracing_enabled);
    if (tracing_enabled == 0)
        return;

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    for (fp = logfile_list; fp; fp = fp->next)
        flush_logfile(fp);
    sample = --tracing_enabled;
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
    if (sample == 0)
        ShutdownHooker();
}

PR_IMPLEMENT(void)
NS_TraceMallocEnable(void)
{
    tm_thread *t = tm_get_thread();
    uint32_t sample;

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    sample = ++tracing_enabled;
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
    if (sample == 1)
        StartupHooker();
}

PR_IMPLEMENT(int)
NS_TraceMallocChangeLogFD(int fd)
{
    logfile *oldfp, *fp;
    struct stat sb;
    tm_thread *t = tm_get_thread();

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    oldfp = logfp;
    if (oldfp->fd != fd) {
        flush_logfile(oldfp);
        fp = get_logfile(fd);
        if (!fp) {
            TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
            return -2;
        }
        if (fd >= 0 && fstat(fd, &sb) == 0 && sb.st_size == 0)
            log_header(fd);
        logfp = fp;
    }
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
    return oldfp->fd;
}

static int
lfd_clr_enumerator(PLHashEntry *he, int i, void *arg)
{
    lfdset_entry *le = (lfdset_entry*) he;
    logfile *fp = (logfile*) arg;

    LFD_CLR(fp->lfd, &le->lfdset);
    return HT_ENUMERATE_NEXT;
}

static void
lfd_clr_walk(callsite *site, logfile *fp)
{
    callsite *kid;

    LFD_CLR(fp->lfd, &site->lfdset);
    for (kid = site->kids; kid; kid = kid->siblings)
        lfd_clr_walk(kid, fp);
}

PR_IMPLEMENT(void)
NS_TraceMallocCloseLogFD(int fd)
{
    logfile *fp;
    tm_thread *t = tm_get_thread();

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);

    fp = get_logfile(fd);
    if (fp) {
        flush_logfile(fp);
        if (fp == &default_logfile) {
            /* Leave default_logfile in logfile_list with an fd of -1. */
            fp->fd = -1;

            /* NB: we can never free lfd 0, it belongs to default_logfile. */
            PR_ASSERT(fp->lfd == 0);
        } else {
            /* Clear fp->lfd in all possible lfdsets. */
            PL_HashTableEnumerateEntries(libraries, lfd_clr_enumerator, fp);
            PL_HashTableEnumerateEntries(methods, lfd_clr_enumerator, fp);
            lfd_clr_walk(&calltree_root, fp);

            /* Unlink fp from logfile_list, freeing lfd for reallocation. */
            *fp->prevp = fp->next;
            if (!fp->next) {
                PR_ASSERT(logfile_tail == &fp->next);
                logfile_tail = fp->prevp;
            }

            /* Reset logfp if we must, then free fp. */
            if (fp == logfp)
                logfp = &default_logfile;
            free((void*) fp);
        }
    }

    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
    MozillaUnRegisterDebugFD(fd);
    close(fd);
}

PR_IMPLEMENT(void)
NS_TraceMallocLogTimestamp(const char *caption)
{
    logfile *fp;
#ifdef XP_UNIX
    struct timeval tv;
#endif
#ifdef XP_WIN32
    struct _timeb tb;
#endif
    tm_thread *t = tm_get_thread();

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);

    fp = logfp;
    log_byte(fp, TM_EVENT_TIMESTAMP);

#ifdef XP_UNIX
    gettimeofday(&tv, NULL);
    log_uint32(fp, (uint32_t) tv.tv_sec);
    log_uint32(fp, (uint32_t) tv.tv_usec);
#endif
#ifdef XP_WIN32
    _ftime(&tb);
    log_uint32(fp, (uint32_t) tb.time);
    log_uint32(fp, (uint32_t) tb.millitm);
#endif
    log_string(fp, caption);

    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

static void
print_stack(FILE *ofp, callsite *site)
{
    while (site) {
        if (site->name || site->parent) {
            fprintf(ofp, "%s[%s +0x%X]\n",
                    site->name, site->library, site->offset);
        }
        site = site->parent;
    }
}

static int
allocation_enumerator(PLHashEntry *he, int i, void *arg)
{
    allocation *alloc = (allocation*) he;
    FILE *ofp = (FILE*) arg;
    callsite *site = (callsite*) he->value;

    unsigned long *p, *end;

    fprintf(ofp, "%p <%s> (%lu)\n",
            he->key,
            nsGetTypeName(he->key),
            (unsigned long) alloc->size);

    for (p   = (unsigned long*) he->key,
         end = (unsigned long*) ((char*)he->key + alloc->size);
         p < end; ++p) {
        fprintf(ofp, "\t0x%08lX\n", *p);
    }

    print_stack(ofp, site);
    fputc('\n', ofp);
    return HT_ENUMERATE_NEXT;
}

PR_IMPLEMENT(void)
NS_TraceStack(int skip, FILE *ofp)
{
    callsite *site;
    tm_thread *t = tm_get_thread();
    int immediate_abort;

    site = backtrace(t, skip + 1, &immediate_abort);
    while (site) {
        if (site->name || site->parent) {
            fprintf(ofp, "%s[%s +0x%X]\n",
                    site->name, site->library, site->offset);
        }
        site = site->parent;
    }
}

PR_IMPLEMENT(int)
NS_TraceMallocDumpAllocations(const char *pathname)
{
    FILE *ofp;
    int rv;
    int fd;

    tm_thread *t = tm_get_thread();

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);

    ofp = fopen(pathname, WRITE_FLAGS);
    if (ofp) {
        MozillaRegisterDebugFD(fileno(ofp));
        if (allocations) {
            PL_HashTableEnumerateEntries(allocations, allocation_enumerator,
                                         ofp);
        }
        rv = ferror(ofp) ? -1 : 0;
        MozillaUnRegisterDebugFILE(ofp);
        fclose(ofp);
    } else {
        rv = -1;
    }

    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);

    return rv;
}

PR_IMPLEMENT(void)
NS_TraceMallocFlushLogfiles(void)
{
    logfile *fp;
    tm_thread *t = tm_get_thread();

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);

    for (fp = logfile_list; fp; fp = fp->next)
        flush_logfile(fp);

    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

PR_IMPLEMENT(void)
NS_TrackAllocation(void* ptr, FILE *ofp)
{
    allocation *alloc;
    tm_thread *t = tm_get_thread();

    fprintf(ofp, "Trying to track %p\n", (void*) ptr);
    setlinebuf(ofp);

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    if (get_allocations()) {
        alloc = (allocation*)
                *PL_HashTableRawLookup(allocations, hash_pointer(ptr), ptr);
        if (alloc) {
            fprintf(ofp, "Tracking %p\n", (void*) ptr);
            alloc->trackfp = ofp;
        } else {
            fprintf(ofp, "Not tracking %p\n", (void*) ptr);
        }
    }
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

PR_IMPLEMENT(void)
MallocCallback(void *ptr, size_t size, uint32_t start, uint32_t end, tm_thread *t)
{
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;
    int immediate_abort;

    if (!tracing_enabled || t->suppress_tracing != 0)
        return;

    site = backtrace(t, 2, &immediate_abort);
    if (immediate_abort)
        return;

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    tmstats.malloc_calls++;
    if (!ptr) {
        tmstats.malloc_failures++;
    } else {
        if (site) {
            log_event5(logfp, TM_EVENT_MALLOC,
                       site->serial, start, end - start,
                       (uint32_t)NS_PTR_TO_INT32(ptr), size);
        }
        if (get_allocations()) {
            he = PL_HashTableAdd(allocations, ptr, site);
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
                alloc->trackfp = NULL;
            }
        }
    }
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

PR_IMPLEMENT(void)
CallocCallback(void *ptr, size_t count, size_t size, uint32_t start, uint32_t end, tm_thread *t)
{
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;
    int immediate_abort;

    if (!tracing_enabled || t->suppress_tracing != 0)
        return;

    site = backtrace(t, 2, &immediate_abort);
    if (immediate_abort)
        return;

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    tmstats.calloc_calls++;
    if (!ptr) {
        tmstats.calloc_failures++;
    } else {
        size *= count;
        if (site) {
            log_event5(logfp, TM_EVENT_CALLOC,
                       site->serial, start, end - start,
                       (uint32_t)NS_PTR_TO_INT32(ptr), size);
        }
        if (get_allocations()) {
            he = PL_HashTableAdd(allocations, ptr, site);
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
                alloc->trackfp = NULL;
            }
        }
    }
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

PR_IMPLEMENT(void)
ReallocCallback(void * oldptr, void *ptr, size_t size,
                uint32_t start, uint32_t end, tm_thread *t)
{
    callsite *oldsite, *site;
    size_t oldsize;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    allocation *alloc;
    FILE *trackfp = NULL;
    int immediate_abort;

    if (!tracing_enabled || t->suppress_tracing != 0)
        return;

    site = backtrace(t, 2, &immediate_abort);
    if (immediate_abort)
        return;

    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    tmstats.realloc_calls++;
    oldsite = NULL;
    oldsize = 0;
    hep = NULL;
    he = NULL;
    if (oldptr && get_allocations()) {
        hash = hash_pointer(oldptr);
        hep = PL_HashTableRawLookup(allocations, hash, oldptr);
        he = *hep;
        if (he) {
            oldsite = (callsite*) he->value;
            alloc = (allocation*) he;
            oldsize = alloc->size;
            trackfp = alloc->trackfp;
            if (trackfp) {
                fprintf(alloc->trackfp,
                        "\nrealloc(%p, %lu), oldsize %lu, alloc site %p\n",
                        (void*) ptr, (unsigned long) size,
                        (unsigned long) oldsize, (void*) oldsite);
                NS_TraceStack(1, trackfp);
            }
        }
    }
    if (!ptr && size) {
        /*
         * When realloc() fails, the original block is not freed or moved, so
         * we'll leave the allocation entry untouched.
         */
        tmstats.realloc_failures++;
    } else {
        if (site) {
            log_event8(logfp, TM_EVENT_REALLOC,
                       site->serial, start, end - start,
                       (uint32_t)NS_PTR_TO_INT32(ptr), size,
                       oldsite ? oldsite->serial : 0,
                       (uint32_t)NS_PTR_TO_INT32(oldptr), oldsize);
        }
        if (ptr && allocations) {
            if (ptr != oldptr) {
                /*
                 * If we're reallocating (not allocating new space by passing
                 * null to realloc) and realloc moved the block, free oldptr.
                 */
                if (he)
                    PL_HashTableRawRemove(allocations, hep, he);

                /* Record the new allocation now, setting he. */
                he = PL_HashTableAdd(allocations, ptr, site);
            } else {
                /*
                 * If we haven't yet recorded an allocation (possibly due to a
                 * temporary memory shortage), do it now.
                 */
                if (!he)
                    he = PL_HashTableAdd(allocations, ptr, site);
            }
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
                alloc->trackfp = trackfp;
            }
        }
    }
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

PR_IMPLEMENT(void)
FreeCallback(void * ptr, uint32_t start, uint32_t end, tm_thread *t)
{
    PLHashEntry **hep, *he;
    callsite *site;
    allocation *alloc;

    if (!tracing_enabled || t->suppress_tracing != 0)
        return;

    /*
     * FIXME: Perhaps we should call backtrace() so we can check for
     * immediate_abort. However, the only current contexts where
     * immediate_abort will be true do not call free(), so for now,
     * let's avoid the cost of backtrace().  See bug 478195.
     */
    TM_SUPPRESS_TRACING_AND_ENTER_LOCK(t);
    tmstats.free_calls++;
    if (!ptr) {
        tmstats.null_free_calls++;
    } else {
        if (get_allocations()) {
            hep = PL_HashTableRawLookup(allocations, hash_pointer(ptr), ptr);
            he = *hep;
            if (he) {
                site = (callsite*) he->value;
                if (site) {
                    alloc = (allocation*) he;
                    if (alloc->trackfp) {
                        fprintf(alloc->trackfp, "\nfree(%p), alloc site %p\n",
                                (void*) ptr, (void*) site);
                        NS_TraceStack(1, alloc->trackfp);
                    }
                    log_event5(logfp, TM_EVENT_FREE,
                               site->serial, start, end - start,
                               (uint32_t)NS_PTR_TO_INT32(ptr), alloc->size);
                }
                PL_HashTableRawRemove(allocations, hep, he);
            }
        }
    }
    TM_EXIT_LOCK_AND_UNSUPPRESS_TRACING(t);
}

PR_IMPLEMENT(nsTMStackTraceID)
NS_TraceMallocGetStackTrace(void)
{
    callsite *site;
    int dummy;
    tm_thread *t = tm_get_thread();

    PR_ASSERT(t->suppress_tracing == 0);

    site = backtrace(t, 2, &dummy);
    return (nsTMStackTraceID) site;
}

PR_IMPLEMENT(void)
NS_TraceMallocPrintStackTrace(FILE *ofp, nsTMStackTraceID id)
{
    print_stack(ofp, (callsite *)id);
}

#endif /* NS_TRACE_MALLOC */
