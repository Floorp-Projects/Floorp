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
#ifdef NS_TRACE_MALLOC
 /*
 * TODO:
 * - fix my_dladdr so it builds its own symbol tables from bfd
 * - extend logfile so 'F' record tells free stack
 * - diagnose rusty's SMP realloc oldsize corruption bug
 * - #ifdef __linux__/x86 and port to other platforms
 * - unify calltree with gc/boehm somehow (common utility lib?)
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#ifdef XP_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#endif
#include "plhash.h"
#include "pratom.h"
#include "prlog.h"
#include "prmon.h"
#include "prprf.h"
#include "prenv.h"
#include "nsTraceMalloc.h"

#ifdef XP_WIN32
#include "nsStackFrameWin.h"
#include <sys/timeb.h>/*for timeb*/
#include <sys/stat.h>/*for fstat*/

#include <io.h> /*for write*/
#include "nsTraceMallocCallbacks.h"

#define WRITE_FLAGS "w"

#endif /* WIN32 */


#ifdef XP_UNIX
#define WRITE_FLAGS "w"

/* From libiberty, why aren't these in <libiberty.h> ? */
extern char *cplus_demangle(const char *, int);

#define DMGL_PARAMS     0x1
#define DMGL_ANSI       0x2

extern __ptr_t __libc_malloc(size_t);
extern __ptr_t __libc_calloc(size_t, size_t);
extern __ptr_t __libc_realloc(__ptr_t, size_t);
extern void    __libc_free(__ptr_t);

/* XXX I wish dladdr could find local text symbols (static functions). */
#define __USE_GNU 1
#include <dlfcn.h>

#if 1
#define my_dladdr dladdr
#else
/* XXX this version, which uses libbfd, runs mozilla clean out of memory! */
#include <bfd.h>
#include <elf.h>        /* damn dladdr ignores local symbols! */
#include <link.h>

extern struct link_map *_dl_loaded;

static int my_dladdr(const void *address, Dl_info *info)
{
    const ElfW(Addr) addr = (ElfW(Addr)) address;
    struct link_map *lib, *matchlib;
    unsigned int n, size;
    bfd *abfd;
    PTR minisyms;
    long nsyms;
    bfd_byte *mini, *endmini;
    asymbol *sym, *storage;
    bfd_vma target, symaddr;
    static const char *sname;

    /* Find the highest-addressed object not greater than address. */
    matchlib = NULL;
    for (lib = _dl_loaded; lib; lib = lib->l_next) {
        if (lib->l_addr != 0 &&         /* 0 means map not set up yet? */
            lib->l_addr <= addr &&
            (!matchlib || matchlib->l_addr < lib->l_addr)) {
            matchlib = lib;
        }
    }
    if (!matchlib)
        return 0;

    /*
     * We know the address lies within matchlib, if it's in any shared object.
     * Make sure it isn't past the end of matchlib's segments.
     */
    n = (size_t) matchlib->l_phnum;
    if (n > 0) {
        do {
            --n;
        } while (matchlib->l_phdr[n].p_type != PT_LOAD);
        if (addr >= (matchlib->l_addr +
                     matchlib->l_phdr[n].p_vaddr +
                     matchlib->l_phdr[n].p_memsz)) {
            /* Off the end of the highest-addressed shared object. */
            return 0;
        }
    }

    /*
     * Now we know what object the address lies in.  Set up info for a file
     * match, then find the greatest info->dli_saddr <= addr.
     */
    info->dli_fname = matchlib->l_name;
    info->dli_fbase = (void*) matchlib->l_addr;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;

    /* Ah, the joys of libbfd.... */
    abfd = bfd_openr(matchlib->l_name, "elf32-i386");
    if (!abfd)
        return 0;
    if (!bfd_check_format(abfd, bfd_object)) {
        printf("%s is not an object file, according to libbfd.\n",
               matchlib->l_name);
        return 0;
    }
    nsyms = bfd_read_minisymbols(abfd, 0, &minisyms, &size);
    if (nsyms < 0) {
        bfd_close(abfd);
        return 0;
    }

    if (nsyms > 0) {
        storage = bfd_make_empty_symbol(abfd);
        if (!storage) {
            bfd_close(abfd);
            return 0;
        }
        target = (bfd_vma) addr - (bfd_vma) matchlib->l_addr;
        endmini = (bfd_byte*) minisyms + nsyms * size;

        for (mini = (bfd_byte*) minisyms; mini < endmini; mini += size) {
            sym = bfd_minisymbol_to_symbol(abfd, 0, (const PTR)mini, storage);
            if (!sym) {
                bfd_close(abfd);
                return 0;
            }
            if (sym->flags & (BSF_GLOBAL | BSF_LOCAL | BSF_WEAK)) {
                symaddr = sym->value + sym->section->vma;
                if (symaddr == 0 || symaddr > target)
                    continue;
                if (!info->dli_sname || info->dli_saddr < (void*) symaddr) {
                    info->dli_sname = sym->name;
                    info->dli_saddr = (void*) symaddr;
                }
            }
        }

        /* Emulate dladdr by allocating and owning info->dli_sname's storage. */
        if (info->dli_sname) {
            if (sname)
                __libc_free((void*) sname);
            sname = strdup(info->dli_sname);
            if (!sname)
                return 0;
            info->dli_sname = sname;
        }
    }
    bfd_close(abfd);
    return 1;
}
#endif /* 0 */
#else /*NOT UNIX DEFINE OUT LIBC*/
#define __libc_malloc(x) malloc(x)
#define __libc_calloc(x, y) calloc(x,y)
#define __libc_realloc(x, y) realloc(x,y)
#define __libc_free(x) free(x)
/*
ok we need to load the malloc,free,realloc,calloc from the dll and store the function pointers somewhere and call them by function
that is the only way that "I" can see to override malloc ect assuredly. all other dlls that link with this library should have their
malloc overridden to call this one.
*/

typedef void * (__stdcall *MALLOCPROC)(size_t);
typedef void * (__stdcall *REALLOCPROC)(void *, size_t);
typedef void * (__stdcall *CALLOCPROC)(size_t,size_t);
typedef void (__stdcall *FREEPROC)(void *);

/*debug types*/
#ifdef _DEBUG
typedef void * (__stdcall *MALLOCDEBUGPROC) ( size_t, int, const char *, int);
typedef void * (__stdcall *CALLOCDEBUGPROC) ( size_t, size_t, int, const char *, int);
typedef void * (__stdcall *REALLOCDEBUGPROC) ( void *, size_t, int, const char *, int);
typedef void   (__stdcall *FREEDEBUGPROC) ( void *, int);
#endif

struct AllocationFuncs
{
  MALLOCPROC   malloc_proc;
  CALLOCPROC   calloc_proc;
  REALLOCPROC  realloc_proc;
  FREEPROC     free_proc;
#ifdef _DEBUG
  MALLOCDEBUGPROC   malloc_debug_proc;
  CALLOCDEBUGPROC   calloc_debug_proc;
  REALLOCDEBUGPROC  realloc_debug_proc;
  FREEDEBUGPROC     free_debug_proc;
#endif
  int          prevent_reentry;
}gAllocFuncs;
#endif /*!XP_UNIX*/

typedef struct logfile logfile;

#define STARTUP_TMBUFSIZE (16 * 1024)
#define LOGFILE_TMBUFSIZE (16 * 1024)

struct logfile {
    int         fd;
    int         lfd;            /* logical fd, dense among all logfiles */
    char        *buf;
    int         bufsize;
    int         pos;
    uint32      size;
    uint32      simsize;
    logfile     *next;
    logfile     **prevp;
};

static char      default_buf[STARTUP_TMBUFSIZE];
static logfile   default_logfile = {-1, 0, default_buf, STARTUP_TMBUFSIZE, 0, 0, 0, NULL, NULL};
static logfile   *logfile_list = NULL;
static logfile   **logfile_tail = &logfile_list;
static logfile   *logfp = &default_logfile;
static PRMonitor *tmmon = NULL;
static char      *sdlogname = NULL; /* filename for shutdown leak log */

/*
 * This counter suppresses tracing, in case any tracing code needs to malloc,
 * and it must be tested and manipulated only within tmmon.
 */
static uint32 suppress_tracing = 0;

#define TM_ENTER_MONITOR()                                                    \
    PR_BEGIN_MACRO                                                            \
        if (tmmon)                                                            \
            PR_EnterMonitor(tmmon);                                           \
    PR_END_MACRO

#define TM_EXIT_MONITOR()                                                     \
    PR_BEGIN_MACRO                                                            \
        if (tmmon)                                                            \
            PR_ExitMonitor(tmmon);                                            \
    PR_END_MACRO

/* We don't want more than 32 logfiles open at once, ok? */
typedef uint32          lfd_set;

#define LFD_SET_STATIC_INITIALIZER 0
#define LFD_SET_SIZE    32

#define LFD_ZERO(s)     (*(s) = 0)
#define LFD_BIT(i)      ((uint32)1 << (i))
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

    len = strlen(str);
    while ((rem = fp->pos + len - fp->bufsize) > 0) {
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
    lfd_set     lfdset;
    char        *name;
    const char  *library;
    int         offset;
    callsite    *parent;
    callsite    *siblings;
    callsite    *kids;
};

/* NB: these counters are incremented and decremented only within tmmon. */
static uint32 library_serial_generator = 0;
static uint32 method_serial_generator = 0;
static uint32 callsite_serial_generator = 0;
static uint32 tmstats_serial_generator = 0;

/* Root of the tree of callsites, the sum of all (cycle-compressed) stacks. */
static callsite calltree_root = {0, 0, LFD_SET_STATIC_INITIALIZER, NULL, NULL, 0, NULL, NULL, NULL};

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
    log_uint32(fp, calltree_maxkids_parent ? calltree_maxkids_parent->serial : 0);
    log_uint32(fp, calltree_maxstack_top ? calltree_maxstack_top->serial : 0);
}

static void *generic_alloctable(void *pool, PRSize size)
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

static void lfdset_freeentry(void *pool, PLHashEntry *he, PRUintn flag)
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

/* Table mapping method names to logged 'N' record serial numbers. */
static PLHashTable *methods = NULL;

#ifdef XP_WIN32

#define  MAX_STACKFRAMES 256
#define  MAX_UNMANGLED_NAME_LEN 256

static callsite *calltree(int skip)
{
    logfile *fp = logfp;
    HANDLE myProcess;
    HANDLE myThread;
    CONTEXT context;
    int ok, maxstack, offset;
    STACKFRAME frame[MAX_STACKFRAMES];
    uint32 library_serial, method_serial;
    int framenum;
    uint32 pc;
    uint32 depth, nkids;
    callsite *parent, *site, **csp, *tmp;
    char *demangledname;
    const char *library;
    IMAGEHLP_MODULE imagehelp;
    char buf[sizeof(IMAGEHLP_SYMBOL) + 512];
    PIMAGEHLP_SYMBOL symbol;
    char *method, *slash;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    lfdset_entry *le;

    imagehelp.SizeOfStruct = sizeof(imagehelp);
    framenum = 0;
    myProcess = GetCurrentProcess();
    myThread = GetCurrentThread();

    ok = EnsureSymInitialized();
    if (! ok)
        return 0;

    /*
     * Get the context information for this thread.  That way we will know
     * where our sp, fp, pc, etc. are, and we can fill in the STACKFRAME with
     * the initial values.
     */
    context.ContextFlags = CONTEXT_FULL;
    ok = GetThreadContext(myThread, &context);
    if (! ok)
        return 0;

    /* Setup initial stack frame from which to walk. */
    memset(&(frame[0]), 0, sizeof(frame[0]));
    frame[0].AddrPC.Offset    = context.Eip;
    frame[0].AddrPC.Mode      = AddrModeFlat;
    frame[0].AddrStack.Offset = context.Esp;
    frame[0].AddrStack.Mode   = AddrModeFlat;
    frame[0].AddrFrame.Offset = context.Ebp;
    frame[0].AddrFrame.Mode   = AddrModeFlat;
    for (;;) {
        PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL) buf;
        if (framenum)
            memcpy(&(frame[framenum]),&(frame[framenum-1]),sizeof(STACKFRAME));

        ok = _StackWalk(IMAGE_FILE_MACHINE_I386,
                        myProcess,
                        myThread,
                        &(frame[framenum]),
                        &context,
                        0,                       /* read process memory hook */
                        _SymFunctionTableAccess, /* function table access hook */
                        _SymGetModuleBase,       /* module base hook */
                        0);                      /* translate address hook */

        if (!ok)
            break;
        if (skip) {
            /* skip tells us to skip the first skip amount of stackframes */
            skip--;
            continue;
        }
        if (frame[framenum].AddrPC.Offset == 0)
            break;
        framenum++;
    }

    depth = framenum;
    maxstack = (depth > tmstats.calltree_maxstack);
    if (maxstack)
        tmstats.calltree_maxstack = depth;

    /* Reverse the stack again, finding and building a path in the tree. */
    parent = &calltree_root;
    do {
        DWORD displacement;/*used for getsymfromaddr*/

        pc = frame[--framenum].AddrPC.Offset;

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
         * callsite info.  XXX static syms are masked by nearest lower global
         * Load up the info for the dll.
         */
        if (!_SymGetModuleInfo(myProcess,
                               frame[framenum].AddrPC.Offset,
                               &imagehelp)) {
            DWORD error = GetLastError();
            PR_ASSERT(error);
            library = "unknown"; /* XXX mjudge sez "ew!" */
        } else {
            library = imagehelp.ModuleName;
        }

        symbol = (PIMAGEHLP_SYMBOL) buf;
        symbol->SizeOfStruct = sizeof(buf);
        symbol->MaxNameLength = 512;

        ok = _SymGetSymFromAddr(myProcess,
                                frame[framenum].AddrPC.Offset,
                                &displacement,
                                symbol);

        /* Check whether we need to emit a library trace record. */
        library_serial = 0;
        if (library) {
           if (!libraries) {
                libraries = PL_NewHashTable(100, PL_HashString,
                                            PL_CompareStrings, PL_CompareValues,
                                            &lfdset_hashallocops, NULL);
                if (!libraries) {
                    tmstats.btmalloc_failures++;
                    return NULL;
                }
            }
            hash = PL_HashString(library);
            hep = PL_HashTableRawLookup(libraries, hash, library);
            he = *hep;
            library = strdup(library); /* strdup it always? */
            if (he) {
                library_serial = (uint32) he->value;
                le = (lfdset_entry *) he;
                if (LFD_TEST(fp->lfd, &le->lfdset)) {
                    /* We already logged an event on fp for this library. */
                    le = NULL;
                }
            } else {
/*                library = strdup(library); */
                if (library) {
                    library_serial = ++library_serial_generator;
                    he = PL_HashTableRawAdd(libraries, hep, hash, library,
                                            (void*) library_serial);
                }
                if (!he) {
                    tmstats.btmalloc_failures++;
                    return NULL;
                }
                le = (lfdset_entry *) he;
            }
            if (le) {
                /* Need to log an event to fp for this lib. */
                slash = strrchr(library, '/');
                if (slash)
                    library = slash + 1;
                log_event1(fp, TM_EVENT_LIBRARY, library_serial);
                log_string(fp, library);
                LFD_SET(fp->lfd, &le->lfdset);
            }
        }

        /* Now find the demangled method name and pc offset in it. */
        demangledname = (char *)malloc(MAX_UNMANGLED_NAME_LEN);
        if (!_SymUnDName(symbol,demangledname,MAX_UNMANGLED_NAME_LEN))
          return 0;
        method = demangledname;
        offset = (char*)pc - (char*)(symbol->Address);
        /* Emit an 'N' (for New method, 'M' is for malloc!) event if needed. */
        method_serial = 0;
        if (!methods) {
            methods = PL_NewHashTable(10000, PL_HashString,
                                      PL_CompareStrings, PL_CompareValues,
                                      &lfdset_hashallocops, NULL);
            if (!methods) {
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
            le = (lfdset_entry *) he;
            if (LFD_TEST(fp->lfd, &le->lfdset)) {
                /* We already logged an event on fp for this method. */
                le = NULL;
            }
        } else {
            method_serial = ++method_serial_generator;
            he = PL_HashTableRawAdd(methods, hep, hash, method,
                                    (void*) method_serial);
            if (!he) {
                tmstats.btmalloc_failures++;
                free((void*) method);
                return NULL;
            }
            le = (lfdset_entry *) he;
        }
        if (le) {
            log_event2(fp, TM_EVENT_METHOD, method_serial, library_serial);
            log_string(fp, method);
            LFD_SET(fp->lfd, &le->lfdset);
        }

        /* Create a new callsite record. */
        if (!site) {
            site = malloc(sizeof(callsite));
            if (!site) {
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
            LFD_ZERO(&site->lfdset);
            site->name = method;
            site->offset = offset;
            site->parent = parent;
            site->siblings = parent->kids;
            site->library = library;
            parent->kids = site;
            site->kids = NULL;
        }

        /* Log the site with its parent, method, and offset. */
        log_event4(fp, TM_EVENT_CALLSITE, site->serial, parent->serial,
                   method_serial, offset);
        LFD_SET(fp->lfd, &site->lfdset);

      upward:
        parent = site;
    } while (framenum);

    if (maxstack)
        calltree_maxstack_top = site;
    depth = 0;
    for (tmp = site; tmp; tmp = tmp->parent)
        depth++;
    if (depth > tmstats.calltree_maxdepth)
        tmstats.calltree_maxdepth = depth;
    return site;
}

#else /*XP_UNIX*/

static callsite *calltree(uint32 *bp)
{
    logfile *fp = logfp;
    uint32 *bpup, *bpdown, pc;
    uint32 depth, nkids;
    callsite *parent, *site, **csp, *tmp;
    Dl_info info;
    int ok, len, maxstack, offset;
    uint32 library_serial, method_serial;
    const char *library, *symbol;
    char *method, *slash;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    lfdset_entry *le;

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
         * callsite info.  XXX static syms are masked by nearest lower global
         */
        info.dli_fname = info.dli_sname = NULL;
        ok = my_dladdr((void*) pc, &info);
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
                                            &lfdset_hashallocops, NULL);
                if (!libraries) {
                    tmstats.btmalloc_failures++;
                    return NULL;
                }
            }
            hash = PL_HashString(library);
            hep = PL_HashTableRawLookup(libraries, hash, library);
            he = *hep;
            if (he) {
                library_serial = (uint32) he->value;
                le = (lfdset_entry *) he;
                if (LFD_TEST(fp->lfd, &le->lfdset)) {
                    /* We already logged an event on fp for this library. */
                    le = NULL;
                }
            } else {
                library = strdup(library);
                if (library) {
                    library_serial = ++library_serial_generator;
                    he = PL_HashTableRawAdd(libraries, hep, hash, library,
                                            (void*) library_serial);
                }
                if (!he) {
                    tmstats.btmalloc_failures++;
                    return NULL;
                }
                le = (lfdset_entry *) he;
            }
            if (le) {
                /* Need to log an event to fp for this lib. */
                slash = strrchr(library, '/');
                if (slash)
                    library = slash + 1;
                log_event1(fp, TM_EVENT_LIBRARY, library_serial);
                log_string(fp, library);
                LFD_SET(fp->lfd, &le->lfdset);
            }
        }

        /* Now find the demangled method name and pc offset in it. */
        symbol = info.dli_sname;
        offset = (char*)pc - (char*)info.dli_saddr;
        method = NULL;
        if (symbol && (len = strlen(symbol)) != 0) {
            /* Attempt to demangle symbol in case it's a C++ mangled name. */
            method = cplus_demangle(symbol, DMGL_PARAMS | DMGL_ANSI);
        }
        if (!method) {
            method = symbol
                     ? strdup(symbol)
                     : PR_smprintf("%s+%X",
                                   info.dli_fname ? info.dli_fname : "main",
                                   (char*)pc - (char*)info.dli_fbase);
        }
        if (!method) {
            tmstats.btmalloc_failures++;
            return NULL;
        }

        /* Emit an 'N' (for New method, 'M' is for malloc!) event if needed. */
        method_serial = 0;
        if (!methods) {
            methods = PL_NewHashTable(10000, PL_HashString,
                                      PL_CompareStrings, PL_CompareValues,
                                      &lfdset_hashallocops, NULL);
            if (!methods) {
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
            le = (lfdset_entry *) he;
            if (LFD_TEST(fp->lfd, &le->lfdset)) {
                /* We already logged an event on fp for this method. */
                le = NULL;
            }
        } else {
            method_serial = ++method_serial_generator;
            he = PL_HashTableRawAdd(methods, hep, hash, method,
                                    (void*) method_serial);
            if (!he) {
                tmstats.btmalloc_failures++;
                free((void*) method);
                return NULL;
            }
            le = (lfdset_entry *) he;
        }
        if (le) {
            log_event2(fp, TM_EVENT_METHOD, method_serial, library_serial);
            log_string(fp, method);
            LFD_SET(fp->lfd, &le->lfdset);
        }

        /* Create a new callsite record. */
        if (!site) {
            site = __libc_malloc(sizeof(callsite));
            if (!site) {
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
            LFD_ZERO(&site->lfdset);
            site->name = method;
            site->library = info.dli_fname;
            site->offset = (char*)pc - (char*)info.dli_fbase;
            site->parent = parent;
            site->siblings = parent->kids;
            parent->kids = site;
            site->kids = NULL;
        }

        /* Log the site with its parent, method, and offset. */
        log_event4(fp, TM_EVENT_CALLSITE, site->serial, parent->serial,
                   method_serial, offset);
        LFD_SET(fp->lfd, &site->lfdset);

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

#endif

#ifdef XP_WIN32

callsite *
backtrace(int skip)
{
    callsite *site;

    tmstats.backtrace_calls++;
    suppress_tracing++;

    site = calltree(skip);
    if (!site) {
        tmstats.backtrace_failures++;
        PR_ASSERT(tmstats.backtrace_failures < 100);
    }
    suppress_tracing--;
    return site;
}

#else /*XP_UNIX*/

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


#endif /* XP_UNIX */


typedef struct allocation {
    PLHashEntry entry;
    size_t      size;
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

#ifdef XP_UNIX

__ptr_t malloc(size_t size)
{
    __ptr_t *ptr;
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;

    ptr = __libc_malloc(size);
    TM_ENTER_MONITOR();
    tmstats.malloc_calls++;
    if (!ptr) {
        tmstats.malloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(1);
        if (site)
            log_event2(logfp, TM_EVENT_MALLOC, site->serial, size);
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
    TM_EXIT_MONITOR();
    return ptr;
}

__ptr_t calloc(size_t count, size_t size)
{
    __ptr_t *ptr;
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;

    ptr = __libc_calloc(count, size);
    TM_ENTER_MONITOR();
    tmstats.calloc_calls++;
    if (!ptr) {
        tmstats.calloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(1);
        size *= count;
        if (site)
            log_event2(logfp, TM_EVENT_CALLOC, site->serial, size);
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
    TM_EXIT_MONITOR();
    return ptr;
}

__ptr_t realloc(__ptr_t ptr, size_t size)
{
    __ptr_t oldptr;
    callsite *oldsite, *site;
    size_t oldsize;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    allocation *alloc;

    TM_ENTER_MONITOR();
    tmstats.realloc_calls++;
    if (suppress_tracing == 0) {
        oldptr = ptr;
        oldsite = NULL;
        oldsize = 0;
        he = NULL;
        if (oldptr && get_allocations()) {
            hash = hash_pointer(oldptr);
            hep = PL_HashTableRawLookup(allocations, hash, oldptr);
            he = *hep;
            if (he) {
                oldsite = (callsite*) he->value;
                alloc = (allocation*) he;
                oldsize = alloc->size;
            }
        }
    }
    TM_EXIT_MONITOR();

    ptr = __libc_realloc(ptr, size);

    TM_ENTER_MONITOR();
    if (!ptr && size) {
        /*
         * When realloc() fails, the original block is not freed or moved, so
         * we'll leave the allocation entry untouched.
         */
        tmstats.realloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(1);
        if (site) {
            log_event4(logfp, TM_EVENT_REALLOC, site->serial, size,
                       oldsite ? oldsite->serial : 0, oldsize);
        }
        if (ptr && allocations) {
            suppress_tracing++;
            if (ptr != oldptr) {
                /*
                 * If we're reallocating (not merely allocating new space by
                 * passing null to realloc) and realloc has moved the block,
                 * free oldptr.
                 */
                if (he)
                    PL_HashTableRemove(allocations, oldptr);

                /* Record the new allocation now, setting he. */
                he = PL_HashTableAdd(allocations, ptr, site);
            } else {
                /*
                 * If we haven't yet recorded an allocation (possibly due to
                 * a temporary memory shortage), do it now.
                 */
                if (!he)
                    he = PL_HashTableAdd(allocations, ptr, site);
            }
            suppress_tracing--;
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
            }
        }
    }
    TM_EXIT_MONITOR();
    return ptr;
}

void free(__ptr_t ptr)
{
    PLHashEntry **hep, *he;
    callsite *site;
    allocation *alloc;

    TM_ENTER_MONITOR();
    tmstats.free_calls++;
    if (!ptr) {
        tmstats.null_free_calls++;
    } else if (suppress_tracing == 0) {
        if (get_allocations()) {
            hep = PL_HashTableRawLookup(allocations, hash_pointer(ptr), ptr);
            he = *hep;
            if (he) {
                site = (callsite*) he->value;
                if (site) {
                    alloc = (allocation*) he;
                    log_event2(logfp, TM_EVENT_FREE, site->serial,
                               alloc->size);
                }
                PL_HashTableRawRemove(allocations, hep, he);
            }
        }
    }
    TM_EXIT_MONITOR();
    __libc_free(ptr);
}

#endif /* XP_UNIX */

static const char magic[] = NS_TRACE_MALLOC_MAGIC;

PR_IMPLEMENT(void) NS_TraceMallocStartup(int logfd)
{
    /* We must be running on the primordial thread. */
    PR_ASSERT(suppress_tracing == 0);
    PR_ASSERT(logfp == &default_logfile);
    suppress_tracing = (logfd < 0);

    if (suppress_tracing == 0) {
        /* Log everything in logfp (aka default_logfile)'s buffer to logfd. */
        logfp->fd = logfd;
        logfile_list = &default_logfile;
        logfp->prevp = &logfile_list;
        logfile_tail = &logfp->next;
        (void) write(logfd, magic, NS_TRACE_MALLOC_MAGIC_SIZE);
        flush_logfile(logfp);
    }

    atexit(NS_TraceMallocShutdown);
    tmmon = PR_NewMonitor();

#ifdef XP_WIN32
    /* Register listeners for win32. */
    {
        StartupHooker();
    }
#endif
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

PR_IMPLEMENT(int) NS_TraceMallocStartupArgs(int argc, char* argv[])
{
    int i, logfd = -1, consumed;
    char *tmlogname = NULL; /* note global |sdlogname| */

    /*
     * Look for the --trace-malloc <logfile> option early, to avoid missing
     * early mallocs (we miss static constructors whose output overflows the
     * log file's static 16K output buffer).
     */
    for (i = 1; i < argc; i += consumed) {
        consumed = 0;
        if (SHOULD_PARSE_ARG(TMLOG_OPTION, tmlogname, argv[i]))
            PARSE_ARG(TMLOG_OPTION, tmlogname, argv, i, consumed);
        else if (SHOULD_PARSE_ARG(SDLOG_OPTION, sdlogname, argv[i]))
            PARSE_ARG(SDLOG_OPTION, sdlogname, argv, i, consumed);

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
        int pipefds[2];

        switch (*tmlogname) {
#ifdef XP_UNIX
          case '|':
            if (pipe(pipefds) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    /* In child: set up stdin, parse args, and exec. */
                    int maxargc, nargc;
                    char **nargv, *token;
#if HAVE_STRTOK_R
                    char *newstr = nsnull;
#endif

                    if (pipefds[0] != 0) {
                        dup2(pipefds[0], 0);
                        close(pipefds[0]);
                    }
                    close(pipefds[1]);

#ifdef HAVE_STRTOK_R
                    tmlogname = strtok_r(tmlogname + 1, " \t", &newstr);
#else
                    tmlogname = strtok(tmlogname + 1, " \t");
#endif
                    maxargc = 3;
                    nargv = (char **) malloc((maxargc+1) * sizeof(char *));
                    if (!nargv) exit(1);
                    nargc = 0;
                    nargv[nargc++] = tmlogname;
                    while (
#ifdef HAVE_STRTOK_R
                           (token = strtok_r(newstr, " \t", &newstr))
#else
                           (token = strtok(NULL, " \t"))
#endif
                           != NULL) {
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
            logfd = open(tmlogname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (logfd < 0) {
                fprintf(stderr,
                    "%s: can't create trace-malloc log named %s: %s\n",
                    argv[0], tmlogname, strerror(errno));
                exit(1);
            }
            break;
        }
    }

    NS_TraceMallocStartup(logfd);
    return argc;
}

PR_IMPLEMENT(void) NS_TraceMallocShutdown()
{
    logfile *fp;

    if (sdlogname)
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
            close(fp->fd);
            fp->fd = -1;
        }
        if (fp != &default_logfile) {
            if (fp == logfp)
                logfp = &default_logfile;
            free((void*) fp);
        }
    }
    if (tmmon) {
        PRMonitor *mon = tmmon;
        tmmon = NULL;
        PR_DestroyMonitor(mon);
    }
#ifdef XP_WIN32
    ShutdownHooker();
#endif
}

PR_IMPLEMENT(void) NS_TraceMallocDisable()
{
    logfile *fp;

    TM_ENTER_MONITOR();
    for (fp = logfile_list; fp; fp = fp->next)
        flush_logfile(fp);
    suppress_tracing++;
    TM_EXIT_MONITOR();
}

PR_IMPLEMENT(void) NS_TraceMallocEnable()
{
    TM_ENTER_MONITOR();
    suppress_tracing--;
    TM_EXIT_MONITOR();
}

PR_IMPLEMENT(int) NS_TraceMallocChangeLogFD(int fd)
{
    logfile *oldfp, *fp;
    struct stat sb;

    TM_ENTER_MONITOR();
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
    TM_EXIT_MONITOR();
    return oldfp->fd;
}

static PRIntn
lfd_clr_enumerator(PLHashEntry *he, PRIntn i, void *arg)
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

    TM_ENTER_MONITOR();

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

    TM_EXIT_MONITOR();
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

    TM_ENTER_MONITOR();

    fp = logfp;
    log_byte(fp, TM_EVENT_TIMESTAMP);

#ifdef XP_UNIX
     gettimeofday(&tv, NULL);
    log_uint32(fp, (uint32) tv.tv_sec);
    log_uint32(fp, (uint32) tv.tv_usec);
#endif
#ifdef XP_WIN32
     _ftime(&tb);
    log_uint32(fp, (uint32) tb.time);
    log_uint32(fp, (uint32) tb.millitm);
#endif
    log_string(fp, caption);

    TM_EXIT_MONITOR();
}

static PRIntn
allocation_enumerator(PLHashEntry *he, PRIntn i, void *arg)
{
    allocation *alloc = (allocation*) he;
    FILE *ofp = (FILE*) arg;
    callsite *site = (callsite*) he->value;

    extern const char* nsGetTypeName(const void* ptr);
    unsigned *p, *end;

    fprintf(ofp, "0x%08X <%s> (%lu)\n", (unsigned) he->key, nsGetTypeName(he->key), (unsigned long) alloc->size);

    end = (unsigned*)(((char*) he->key) + alloc->size);
    for (p = (unsigned*)he->key; p < end; ++p)
        fprintf(ofp, "\t0x%08X\n", *p);

    while (site) {
        if (site->name || site->parent)
            fprintf(ofp, "%s[%s +0x%X]\n", site->name, site->library, site->offset);
        site = site->parent;
    }
    fputc('\n', ofp);
    return HT_ENUMERATE_NEXT;
}

PR_IMPLEMENT(void)
NS_TraceStack(int skip, FILE *ofp)
{
    callsite *site;

    site = backtrace(skip + 1);
    while (site) {
        if (site->name || site->parent)
            fprintf(ofp, "%s[%s +0x%X]\n", site->name, site->library, site->offset);
        site = site->parent;
    }
}

PR_IMPLEMENT(int)
NS_TraceMallocDumpAllocations(const char *pathname)
{
    FILE *ofp;
    int rv;
    ofp = fopen(pathname, WRITE_FLAGS);
    if (!ofp)
        return -1;
    if (allocations)
        PL_HashTableEnumerateEntries(allocations, allocation_enumerator, ofp);
    rv = ferror(ofp) ? -1 : 0;
    fclose(ofp);
    return rv;
}

PR_IMPLEMENT(void)
NS_TraceMallocFlushLogfiles()
{
    logfile *fp;

    TM_ENTER_MONITOR();

    for (fp = logfile_list; fp; fp = fp->next)
        flush_logfile(fp);

    TM_EXIT_MONITOR();
}

#ifdef XP_WIN32

PR_IMPLEMENT(void)
MallocCallback(void *aPtr, size_t size)
{
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;

    TM_ENTER_MONITOR();
    tmstats.malloc_calls++;
    if (!aPtr) {
        tmstats.malloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(4);
        if (site)
            log_event2(logfp, TM_EVENT_MALLOC, site->serial, size);
        if (get_allocations()) {
            suppress_tracing++;
            he = PL_HashTableAdd(allocations, aPtr, site);
            suppress_tracing--;
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
            }
        }
    }
    TM_EXIT_MONITOR();
}

PR_IMPLEMENT(void)
CallocCallback(void *ptr, size_t count, size_t size)
{
    callsite *site;
    PLHashEntry *he;
    allocation *alloc;

    TM_ENTER_MONITOR();
    tmstats.calloc_calls++;
    if (!ptr) {
        tmstats.calloc_failures++;
    } else if (suppress_tracing == 0) {
        site = backtrace(1);
        size *= count;
        if (site)
            log_event2(logfp, TM_EVENT_CALLOC, site->serial, size);
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
    TM_EXIT_MONITOR();
}

PR_IMPLEMENT(void)
ReallocCallback(void * oldptr, void *ptr, size_t size)
{
    callsite *oldsite, *site;
    size_t oldsize;
    PLHashNumber hash;
    PLHashEntry **hep, *he;
    allocation *alloc;

    TM_ENTER_MONITOR();
    tmstats.realloc_calls++;
    if (suppress_tracing == 0) {
        oldsite = NULL;
        oldsize = 0;
        he = NULL;
        if (oldptr && get_allocations()) {
            hash = hash_pointer(oldptr);
            hep = PL_HashTableRawLookup(allocations, hash, oldptr);
            he = *hep;
            if (he) {
                oldsite = (callsite*) he->value;
                alloc = (allocation*) he;
                oldsize = alloc->size;
            }
        }
    }
    if (!ptr && size) {
        tmstats.realloc_failures++;

        /*
         * When realloc() fails, the original block is not freed or moved, so
         * we'll leave the allocation entry untouched.
         */
    } else if (suppress_tracing == 0) {
        site = backtrace(1);
        if (site) {
            log_event4(logfp, TM_EVENT_REALLOC, site->serial, size,
                       oldsite ? oldsite->serial : 0, oldsize);
        }
        if (ptr && allocations) {
            suppress_tracing++;
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
            suppress_tracing--;
            if (he) {
                alloc = (allocation*) he;
                alloc->size = size;
            }
        }
    }
    TM_EXIT_MONITOR();
}

PR_IMPLEMENT(void)
FreeCallback(void * ptr)
{
    PLHashEntry **hep, *he;
    callsite *site;
    allocation *alloc;

    TM_ENTER_MONITOR();
    tmstats.free_calls++;
    if (!ptr) {
        tmstats.null_free_calls++;
    } else if (suppress_tracing == 0) {
        if (get_allocations()) {
            hep = PL_HashTableRawLookup(allocations, hash_pointer(ptr), ptr);
            he = *hep;
            if (he) {
                site = (callsite*) he->value;
                if (site) {
                    alloc = (allocation*) he;
                    log_event2(logfp, TM_EVENT_FREE, site->serial, alloc->size);
                }
                PL_HashTableRawRemove(allocations, hep, he);
            }
        }
    }
    TM_EXIT_MONITOR();
}

#endif /*XP_WIN32*/

#endif /* NS_TRACE_MALLOC */
