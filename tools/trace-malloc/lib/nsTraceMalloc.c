/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is nsTraceMalloc.c/bloatblame.c code, released
 * April 19, 2000.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Brendan Eich, 14-April-2000
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */
#if defined NS_TRACE_MALLOC

/*
 * TODO:
 * - #ifdef __linux__/x86 and port to other platforms
 * - unify calltree with gc/boehm somehow (common utility libs)
 * - provide NS_DumpTraceMallocStats() and hook up to some xul kbd event
 * - provide NS_TraceMallocTimestamp() or do it internally
 */
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include "plhash.h"
#include "prlog.h"
#include "prmon.h"
#include "prprf.h"
#include "nsTraceMalloc.h"

#define __USE_GNU 1
#include <dlfcn.h>

/* From libiberty, why isn't this in <libiberty.h> ? */
extern char *cplus_demangle(const char *, int);

extern __ptr_t __libc_malloc(size_t);
extern __ptr_t __libc_calloc(size_t, size_t);
extern __ptr_t __libc_realloc(__ptr_t, size_t);
extern void    __libc_free(__ptr_t);

typedef struct logfile logfile;

struct logfile {
    int         fd;
    int         pos;
    uint32      size;
    uint32      simsize;
    logfile     *next;
    char        buf[16*1024];
};

static logfile   logfile1 = {-1, 0, 0, 0, NULL, {0,}};
static logfile   logfile0 = {-1, 0, 0, 0, &logfile1, {0,}};
static logfile   *logfile_list = &logfile0;
static logfile   *logfp = &logfile0;
static logfile   *sitefp = &logfile1;
static PRMonitor *tmmon = NULL;

static logfile *get_logfile(int fd)
{
    logfile *fp;

    for (fp = logfile_list; fp; fp = fp->next)
        if (fp->fd == fd)
            return fp;
    fp = __libc_malloc(sizeof(logfile));
    if (!fp)
        return NULL;
    fp->fd = fd;
    fp->pos = 0;
    fp->size = fp->simsize = 0;
    fp->next = logfile_list;
    logfile_list = fp;
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
    if (fp->pos == sizeof fp->buf)
        flush_logfile(fp);
    fp->buf[fp->pos++] = byte;
}

static void log_string(logfile *fp, const char *str)
{
    int len, rem, cnt;

    len = strlen(str);
    while ((rem = fp->pos + len - sizeof fp->buf) > 0) {
        cnt = len - rem;
        strncpy(&fp->buf[fp->pos], str, cnt);
        str += cnt;
        fp->pos += cnt;
        flush_logfile(fp);
        len = rem;
    }
    strncpy(&fp->buf[fp->pos], str, len);
    fp->pos += len;

    /* Terminate the string. */
    log_byte(fp, '\0');
}

static void log_uint32(logfile *fp, uint32 ival)
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

static void log_event1(logfile *fp, char event, uint32 serial)
{
    log_byte(fp, event);
    log_uint32(fp, (uint32) serial);
}

static void log_event2(logfile *fp, char event, uint32 serial, size_t size)
{
    log_event1(fp, event, serial);
    log_uint32(fp, (uint32) size);
}

static void log_event3(logfile *fp, char event, uint32 serial, size_t oldsize,
                       size_t size)
{
    log_event2(fp, event, serial, oldsize);
    log_uint32(fp, (uint32) size);
}

static void log_event4(logfile *fp, char event, uint32 serial, uint32 ui2,
                       uint32 ui3, uint32 ui4)
{
    log_event3(fp, event, serial, ui2, ui3);
    log_uint32(fp, ui4);
}

typedef struct callsite callsite;

struct callsite {
    uint32      pc;
    uint32      serial;
    char        *name;
    callsite    *parent;
    callsite    *siblings;
    callsite    *kids;
};

static uint32           suppress_tracing = 0;
static uint32           library_serial_generator = 0;
static uint32           method_serial_generator = 0;
static uint32           callsite_serial_generator = 0;
static uint32           tmstats_serial_generator = 0;
static callsite         calltree_root = {0, 0, NULL, NULL, NULL, NULL};

/* Basic instrumentation. */
static struct nsTMStats tmstats = NS_TMSTATS_STATIC_INITIALIZER;

/* Parent with the most kids (tmstats.calltree_maxkids). */
static callsite *calltree_maxkids_parent;

/* Calltree leaf for path with deepest stack backtrace. */
static callsite *calltree_maxstack_top;

/* Last site (i.e., calling pc) that recurred during a backtrace. */
static callsite *last_callsite_recurrence;

static void log_tmstats(logfile *fp)
{
    log_event1(fp, 'Z', ++tmstats_serial_generator);
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
    log_uint32(fp, calltree_maxkids_parent ? calltree_maxkids_parent->serial : 0);
    log_uint32(fp, calltree_maxstack_top ? calltree_maxstack_top->serial : 0);
}

/* Table of library pathnames mapped to to logged 'L' record serial numbers. */
static PLHashTable *libraries = NULL;

/* Table mapping method names to logged 'N' record serial numbers. */
static PLHashTable *methods = NULL;

static callsite *calltree(uint32 *bp)
{
    logfile *fp = sitefp;
    uint32 *bpup, *bpdown, pc;
    uint32 depth, nkids;
    callsite *parent, *site, **csp, *tmp;
    Dl_info info;
    int ok, len, maxstack, offset;
    uint32 library_serial, method_serial;
    const char *library, *symbol;
    char *method;
    PLHashNumber hash;
    PLHashEntry **hep, *he;

    /* Reverse the stack frame list to avoid recursion. */
    bpup = NULL;
    for (depth = 0; ; depth++) {
        bpdown = (uint32*) bp[0];
        bp[0] = (uint32) bpup;
        pc = bp[1];
        if (pc < 0x08000000 || pc > 0x7fffffff || bpdown < bp)
            break;
        bpup = bp;
        bp = bpdown;
    }
    maxstack = (depth > tmstats.calltree_maxstack);
    if (maxstack)
        tmstats.calltree_maxstack = depth;

    /* Reverse the stack again, finding and building a path in the tree. */
    parent = &calltree_root;
    do {
        bpup = (uint32*) bp[0];
        bp[0] = (uint32) bpdown;
        pc = bp[1];

        csp = &parent->kids;
        while ((site = *csp) != NULL) {
            if (site->pc == pc) {
                tmstats.calltree_kidhits++;

                /* Put the most recently used site at the front of siblings. */
                *csp = site->siblings;
                site->siblings = parent->kids;
                parent->kids = site;
                goto upward;
            }
            tmstats.calltree_kidsteps++;
            csp = &site->siblings;
        }
        tmstats.calltree_kidmisses++;

        /* Check for recursion: see if pc is on our ancestor line. */
        for (site = parent; site; site = site->parent) {
            if (site->pc == pc) {
                tmstats.callsite_recurrences++;
                last_callsite_recurrence = site;
                goto upward;
            }
        }

        /* Not in tree, let's find our symbolic callsite info. */
        info.dli_fname = info.dli_sname = NULL;
        ok = dladdr((void*) pc, &info);
        if (ok < 0) {
            tmstats.dladdr_failures++;
            return NULL;
        }

        /* Check whether we need to emit a library trace record. */
        library_serial = 0;
        library = info.dli_fname;
        if (library) {
            if (!libraries) {
                libraries = PL_NewHashTable(100, PL_HashString,
                                            PL_CompareStrings, PL_CompareValues,
                                            NULL, NULL);
                if (!libraries) {
                    printf("OINK 1\n");
                    tmstats.btmalloc_failures++;
                    return NULL;
                }
            }
            hash = PL_HashString(library);
            hep = PL_HashTableRawLookup(libraries, hash, library);
            he = *hep;
            if (he) {
                library_serial = (uint32) he->value;
            } else {
                library = strdup(library);
                if (library) {
                    library_serial = ++library_serial_generator;
                    he = PL_HashTableRawAdd(libraries, hep, hash, library,
                                            (void*) library_serial);
                    if (he) {
                        char *slash = strrchr(library, '/');
                        if (slash)
                            library = slash + 1;
                        log_event1(fp, 'L', library_serial);
                        log_string(fp, library);
                    }
                }
                if (!he) {
                    printf("OINK 2\n");
                    tmstats.btmalloc_failures++;
                    return NULL;
                }
            }
        }

        /* Now find the demangled method name and pc offset in it. */
        symbol = info.dli_sname;
        offset = (char*)pc - (char*)info.dli_saddr;
        method = NULL;
        if (symbol && (len = strlen(symbol)) != 0) {
            /*
             * Attempt to demangle symbol in case it's a C++ mangled name.
             * The magic 3 passed here specifies DMGL_PARAMS | DMGL_ANSI.
             */
            method = cplus_demangle(symbol, 3);
        }
        if (! method) {
            method = symbol
                     ? strdup(symbol)
                     : PR_smprintf("%s+%X",
                                   info.dli_fname ? info.dli_fname : "main",
                                   offset);
        }
        if (! method) {
            printf("OINK 3\n");
            tmstats.btmalloc_failures++;
            return NULL;
        }

        /* Emit an 'N' (for New method, 'M' is for malloc!) event if needed. */
        method_serial = 0;
        if (!methods) {
            methods = PL_NewHashTable(10000, PL_HashString,
                                      PL_CompareStrings, PL_CompareValues,
                                      NULL, NULL);
            if (!methods) {
                printf("OINK 4\n");
                tmstats.btmalloc_failures++;
                free((void*) method);
                return NULL;
            }
        }
        hash = PL_HashString(method);
        hep = PL_HashTableRawLookup(methods, hash, method);
        he = *hep;
        if (he) {
            method_serial = (uint32) he->value;
            free((void*) method);
            method = (char *) he->key;
        } else {
            method_serial = ++method_serial_generator;
            he = PL_HashTableRawAdd(methods, hep, hash, method,
                                    (void*) method_serial);
            if (!he) {
                printf("OINK 5\n");
                tmstats.btmalloc_failures++;
                free((void*) method);
                return NULL;
            }
            log_event2(fp, 'N', method_serial, library_serial);
            log_string(fp, method);
        }

        /* Create a new callsite record. */
        site = __libc_malloc(sizeof(callsite));
        if (!site) {
            printf("OINK 6\n");
            tmstats.btmalloc_failures++;
            return NULL;
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
        site->name = method;
        site->parent = parent;
        site->siblings = parent->kids;
        parent->kids = site;
        site->kids = NULL;

        /* Log the site with its parent, method, and offset. */
        log_event4(fp, 'S', site->serial, parent->serial, method_serial,
                   offset);

      upward:
        parent = site;
        bpdown = bp;
        bp = bpup;
    } while (bp);

    if (maxstack)
        calltree_maxstack_top = site;
    depth = 0;
    for (tmp = site; tmp; tmp = tmp->parent)
        depth++;
    if (depth > tmstats.calltree_maxdepth)
        tmstats.calltree_maxdepth = depth;
    return site;
}

callsite *
backtrace(int skip)
{
    jmp_buf jb;
    uint32 *bp, *bpdown, pc;
    callsite *site, **key;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    int i, n;

    tmstats.backtrace_calls++;
    suppress_tracing++;
    setjmp(jb);

    /* Stack walking code adapted from Kipp's "leaky". */
    bp = (uint32*) jb[0].__jmpbuf[JB_BP];
    while (--skip >= 0) {
        bpdown = (uint32*) *bp++;
        pc = *bp;
        if (pc < 0x08000000 || pc > 0x7fffffff || bpdown < bp)
            break;
        bp = bpdown;
    }

    site = calltree(bp);
    if (!site) {
        tmstats.backtrace_failures++;
        PR_ASSERT(tmstats.backtrace_failures < 100);
    }
    suppress_tracing--;
    return site;
}

typedef struct allocation {
    PLHashEntry entry;
    size_t      size;
} allocation;

#define ALLOC_HEAP_SIZE 150000

static allocation alloc_heap[ALLOC_HEAP_SIZE];
static allocation *alloc_freelist = NULL;
static int alloc_heap_initialized = 0;

static void *generic_alloctable(void *pool, PRSize size)
{
    return __libc_malloc(size);
}

static void generic_freetable(void *pool, void *item)
{
    __libc_free(item);
}

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

static void alloc_freeentry(void *pool, PLHashEntry *he, PRUintn flag)
{
    allocation *alloc;

    if (flag != HT_FREE_ENTRY)
        return;
    alloc = (allocation*) he;
    if (&alloc_heap[0] <= alloc && alloc < &alloc_heap[ALLOC_HEAP_SIZE]) {
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

__ptr_t malloc(size_t size)
{
    __ptr_t *ptr;
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;

    ptr = __libc_malloc(size);
    if (tmmon)
        PR_EnterMonitor(tmmon);
    tmstats.malloc_calls++;
    if (!ptr) {
        tmstats.malloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(2);
        if (site)
            log_event2(logfp, 'M', site->serial, size);
        if (get_allocations()) {
            suppress_tracing++;
            he = PL_HashTableAdd(allocations, ptr, site);
            suppress_tracing--;
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
            }
        }
    }
    if (tmmon)
        PR_ExitMonitor(tmmon);
    return ptr;
}

__ptr_t calloc(size_t count, size_t size)
{
    __ptr_t *ptr;
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;

    ptr = __libc_calloc(count, size);
    if (tmmon)
        PR_EnterMonitor(tmmon);
    tmstats.calloc_calls++;
    if (!ptr) {
        tmstats.calloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(2);
        size *= count;
        if (site)
            log_event2(logfp, 'C', site->serial, size);
        if (get_allocations()) {
            suppress_tracing++;
            he = PL_HashTableAdd(allocations, ptr, site);
            suppress_tracing--;
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
            }
        }
    }
    if (tmmon)
        PR_ExitMonitor(tmmon);
    return ptr;
}

__ptr_t realloc(__ptr_t ptr, size_t size)
{
    size_t oldsize;
    PLHashNumber hash;
    PLHashEntry *he;
    allocation *alloc;
    callsite *site;

    if (tmmon)
        PR_EnterMonitor(tmmon);
    tmstats.realloc_calls++;
    if (suppress_tracing == 0) {
        oldsize = 0;
        if (ptr && get_allocations()) {
            hash = hash_pointer(ptr);
            he = *PL_HashTableRawLookup(allocations, hash, ptr);
            if (he) {
                alloc = (allocation*) he;
                oldsize = alloc->size;
            }
        }
    }
    if (tmmon)
        PR_ExitMonitor(tmmon);

    ptr = __libc_realloc(ptr, size);

    if (tmmon)
        PR_EnterMonitor(tmmon);
    if (!ptr && size) {
        tmstats.realloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(2);
        if (site)
            log_event3(logfp, 'R', site->serial, oldsize, size);
        if (ptr && allocations) {
            suppress_tracing++;
            he = PL_HashTableAdd(allocations, ptr, site);
            suppress_tracing--;
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
            }
        }
    }
    if (tmmon)
        PR_ExitMonitor(tmmon);
    return ptr;
}

void free(__ptr_t ptr)
{
    PLHashEntry **hep, *he;
    callsite *site;
    allocation *alloc;

    if (tmmon)
        PR_EnterMonitor(tmmon);
    tmstats.free_calls++;
    if (!ptr) {
        tmstats.null_free_calls++;
    } else if (suppress_tracing == 0 && get_allocations()) {
        hep = PL_HashTableRawLookup(allocations, hash_pointer(ptr), ptr);
        he = *hep;
        if (he) {
            site = (callsite*) he->value;
            if (site) {
                alloc = (allocation*) he;
                log_event2(logfp, 'F', site->serial, alloc->size);
            }
            PL_HashTableRawRemove(allocations, hep, he);
        }
    }
    if (tmmon)
        PR_ExitMonitor(tmmon);
    __libc_free(ptr);
}

static const char magic[] = NS_TRACE_MALLOC_MAGIC;

PR_IMPLEMENT(void) NS_TraceMallocStartup(int logfd, int sitefd)
{
    /* We must be running on the primordial thread. */
    PR_ASSERT(suppress_tracing == 0);
    suppress_tracing = 1;

    /* Give logfile0 (AKA *logfp) the passed-in logfd, if it's an open fd. */
    if (logfd >= 0)
        logfp->fd = logfd;

    /* If sitefd is open, give it to logfile1, else alias sitefp to logfile0. */
    if (sitefd >= 0)
        sitefp->fd = sitefd;
    else if (logfd >= 0)
        sitefp = logfp;

    /* Now flush any buffered sites, methods, and libraries. */
    if (sitefp->fd >= 0) {
        (void) write(sitefp->fd, magic, NS_TRACE_MALLOC_MAGIC_SIZE);
        flush_logfile(sitefp);
    }

    /* And flush any malloc traces, if we're logging them at this point. */
    if (logfp != sitefp && logfd >= 0) {
        (void) write(logfd, magic, NS_TRACE_MALLOC_MAGIC_SIZE);
        flush_logfile(logfp);
    }

    atexit(NS_TraceMallocShutdown);
    tmmon = PR_NewMonitor();
    suppress_tracing = 0;
}

PR_IMPLEMENT(void) NS_TraceMallocShutdown()
{
    logfile *fp, *next;

    if (tmstats.backtrace_failures) {
        fprintf(stderr,
                "TraceMalloc backtrace failures: %lu (malloc %lu dladdr %lu)\n",
                (unsigned long) tmstats.backtrace_failures,
                (unsigned long) tmstats.btmalloc_failures,
                (unsigned long) tmstats.dladdr_failures);
    }
    log_tmstats(sitefp);
    for (fp = logfile_list; fp; fp = next) {
        next = fp->next;
        flush_logfile(fp);
        if (fp->fd >= 0) {
            close(fp->fd);
            fp->fd = -1;
        }
        if (fp != &logfile0 && fp != &logfile1)
            __libc_free((void*) fp);
    }
    if (tmmon)
        PR_DestroyMonitor(tmmon);
}

PR_IMPLEMENT(void) NS_TraceMallocDisable()
{
    logfile *fp;

    if (tmmon)
        PR_EnterMonitor(tmmon);
    for (fp = logfile_list; fp; fp = fp->next)
        flush_logfile(fp);
    suppress_tracing++;
    if (tmmon)
        PR_ExitMonitor(tmmon);
}

PR_IMPLEMENT(void) NS_TraceMallocEnable()
{
    if (tmmon)
        PR_EnterMonitor(tmmon);
    suppress_tracing--;
    if (tmmon)
        PR_ExitMonitor(tmmon);
}

PR_IMPLEMENT(int) NS_TraceMallocChangeLogFD(int fd)
{
    logfile *oldfp, *fp;
    struct stat sb;

    if (tmmon)
        PR_EnterMonitor(tmmon);
    oldfp = logfp;
    if (oldfp->fd != fd) {
        flush_logfile(oldfp);
        fp = get_logfile(fd);
        if (!fp)
            return -2;
        if (fd >= 0 && fstat(fd, &sb) == 0 && sb.st_size == 0)
            (void) write(fd, magic, NS_TRACE_MALLOC_MAGIC_SIZE);
        logfp = fp;
    }
    if (tmmon)
        PR_ExitMonitor(tmmon);
    return oldfp->fd;
}

#endif /* defined __linux__ && defined DEBUG */
