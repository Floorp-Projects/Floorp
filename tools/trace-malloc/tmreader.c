/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>      /* XXX push error reporting out to clients? */
#ifndef XP_WIN
#include <unistd.h>
#else
#include <stddef.h>
#endif
#include "prlog.h"
#include "plhash.h"
/* make sure this happens before tmreader.h */
#define PL_ARENA_CONST_ALIGN_MASK 2
#include "plarena.h"

#include "prnetdb.h"
#include "nsTraceMalloc.h"
#include "tmreader.h"

#undef  DEBUG_tmreader

static int accum_byte(FILE *fp, uint32_t *uip)
{
    int c = getc(fp);
    if (c == EOF)
        return 0;
    *uip = (*uip << 8) | c;
    return 1;
}

static int get_uint32(FILE *fp, uint32_t *uip)
{
    int c;
    uint32_t ui;

    c = getc(fp);
    if (c == EOF)
        return 0;
    ui = 0;
    if (c & 0x80) {
        c &= 0x7f;
        if (c & 0x40) {
            c &= 0x3f;
            if (c & 0x20) {
                c &= 0x1f;
                if (c & 0x10) {
                    if (!accum_byte(fp, &ui))
                        return 0;
                } else {
                    ui = (uint32_t) c;
                }
                if (!accum_byte(fp, &ui))
                    return 0;
            } else {
                ui = (uint32_t) c;
            }
            if (!accum_byte(fp, &ui))
                return 0;
        } else {
            ui = (uint32_t) c;
        }
        if (!accum_byte(fp, &ui))
            return 0;
    } else {
        ui = (uint32_t) c;
    }
    *uip = ui;
    return 1;
}

static char *get_string(FILE *fp)
{
    char *cp;
    int c;
    static char buf[256];
    static char *bp = buf, *ep = buf + sizeof buf;
    static size_t bsize = sizeof buf;

    cp = bp;
    do {
        c = getc(fp);
        if (c == EOF)
            return 0;
        if (cp == ep) {
            if (bp == buf) {
                bp = malloc(2 * bsize);
                if (bp)
                    memcpy(bp, buf, bsize);
            } else {
                bp = realloc(bp, 2 * bsize);
            }
            if (!bp)
                return 0;
            cp = bp + bsize;
            bsize *= 2;
            ep = bp + bsize;
        }
        *cp++ = c;
    } while (c != '\0');
    return strdup(bp);
}

static int get_tmevent(FILE *fp, tmevent *event)
{
    int c;
    char *s;

    c = getc(fp);
    if (c == EOF)
        return 0;
    event->type = (char) c;
    if (!get_uint32(fp, &event->serial))
        return 0;
    switch (c) {
      case TM_EVENT_LIBRARY:
        s = get_string(fp);
        if (!s)
            return 0;
        event->u.libname = s;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u libname=\"%s\"\n", event->type, event->serial,
                event->u.libname);
#endif
        break;

      case TM_EVENT_FILENAME:
        s = get_string(fp);
        if (!s)
            return 0;
        event->u.srcname = s;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u srcname=\"%s\"\n",
                event->type, event->serial, event->u.srcname);
#endif
        break;

      case TM_EVENT_METHOD:
        if (!get_uint32(fp, &event->u.method.library))
            return 0;
        if (!get_uint32(fp, &event->u.method.filename))
            return 0;
        if (!get_uint32(fp, &event->u.method.linenumber))
            return 0;
        s = get_string(fp);
        if (!s)
            return 0;
        event->u.method.name = s;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u library=%u filename=%u linenumber=%u "
                "name=\"%s\"\n",
                event->type, event->serial,
                event->u.method.library, event->u.method.filename,
                event->u.method.linenumber, event->u.method.name);
#endif
        break;

      case TM_EVENT_CALLSITE:
        if (!get_uint32(fp, &event->u.site.parent))
            return 0;
        if (!get_uint32(fp, &event->u.site.method))
            return 0;
        if (!get_uint32(fp, &event->u.site.offset))
            return 0;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u parent=%u method=%u offset=%u\n",
                event->type, event->serial,
                event->u.site.parent, event->u.site.method,
                event->u.site.offset);
#endif
        break;

      case TM_EVENT_MALLOC:
      case TM_EVENT_CALLOC:
      case TM_EVENT_FREE:
        if (!get_uint32(fp, &event->u.alloc.interval))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.cost))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.ptr))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.size))
            return 0;
        event->u.alloc.oldserial = 0;
        event->u.alloc.oldptr = 0;
        event->u.alloc.oldsize = 0;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u interval=%u cost=%u ptr=0x%x size=%u\n",
                event->type, event->serial,
                event->u.alloc.interval, event->u.alloc.cost,
                event->u.alloc.ptr, event->u.alloc.size);
#endif
#if defined(DEBUG_dp)
        if (c == TM_EVENT_MALLOC)
            printf("%d malloc %d 0x%p\n", event->u.alloc.cost,
                   event->u.alloc.size, event->u.alloc.ptr);
        else if (c == TM_EVENT_CALLOC)
            printf("%d calloc %d 0x%p\n", event->u.alloc.cost,
                   event->u.alloc.size, event->u.alloc.ptr);
        else
            printf("%d free %d 0x%p\n", event->u.alloc.cost,
                   event->u.alloc.size, event->u.alloc.ptr);
#endif
        break;

      case TM_EVENT_REALLOC:
        if (!get_uint32(fp, &event->u.alloc.interval))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.cost))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.ptr))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.size))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.oldserial))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.oldptr))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.oldsize))
            return 0;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u interval=%u cost=%u ptr=0x%x size=%u "
                "oldserial=%u oldptr=0x%x oldsize=%u\n",
                event->type, event->serial,
                event->u.alloc.interval, event->u.alloc.cost,
                event->u.alloc.ptr, event->u.alloc.size,
                event->u.alloc.oldserial, event->u.alloc.oldptr,
                event->u.alloc.oldsize);
#endif
#if defined(DEBUG_dp)
        printf("%d realloc %d 0x%p %d\n", event->u.alloc.cost,
               event->u.alloc.size, event->u.alloc.ptr, event->u.alloc.oldsize);
#endif
        break;

      case TM_EVENT_STATS:
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_maxstack))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_maxdepth))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_parents))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_maxkids))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_kidhits))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_kidmisses))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_kidsteps))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.callsite_recurrences))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.backtrace_calls))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.backtrace_failures))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.btmalloc_failures))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.dladdr_failures))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.malloc_calls))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.malloc_failures))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calloc_calls))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calloc_failures))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.realloc_calls))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.realloc_failures))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.free_calls))
            return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.null_free_calls))
            return 0;
        if (!get_uint32(fp, &event->u.stats.calltree_maxkids_parent))
            return 0;
        if (!get_uint32(fp, &event->u.stats.calltree_maxstack_top))
            return 0;
#ifdef DEBUG_tmreader
        fprintf(stderr, "tmevent %c %u\n", event->type, event->serial);
#endif
        break;
      default:
        fprintf(stderr, "Unknown event type 0x%x\n", (unsigned int)event->type);
        return 0;
    }
    return 1;
}

static void *arena_alloc(void* pool, size_t size)
{
    PLArenaPool* arena = (PLArenaPool*)pool;
    void* result;
    PL_ARENA_ALLOCATE(result, arena, size);
    memset(result, 0, size);
    return result;
}

static void *generic_alloctable(void *pool, size_t size)
{
    return arena_alloc(pool, size);
}

static void generic_freetable(void *pool, void *item)
{
    /* do nothing - arena-allocated */
}

static PLHashEntry *filename_allocentry(void *pool, const void *key)
{
    return (PLHashEntry*)arena_alloc(pool, sizeof(PLHashEntry));
}

static PLHashEntry *callsite_allocentry(void *pool, const void *key)
{
    return (PLHashEntry*)arena_alloc(pool, sizeof(tmcallsite));
}

static void init_graphnode(tmgraphnode* node)
{
    node->in = node->out = NULL;
    node->up = node->down = node->next = NULL;
    node->low = 0;
    node->allocs.bytes.direct = node->allocs.bytes.total = 0;
    node->allocs.calls.direct = node->allocs.calls.total = 0;
    node->frees.bytes.direct = node->frees.bytes.total = 0;
    node->frees.calls.direct = node->frees.calls.total = 0;
    node->sqsum = 0;
    node->sort = -1;
}

static PLHashEntry *graphnode_allocentry(void *pool, const void *key)
{
    tmgraphnode* node = (tmgraphnode*)arena_alloc(pool, sizeof(tmgraphnode));
    if (!node)
        return NULL;
    init_graphnode(node);
    return &node->entry;
}

static void init_method(tmmethodnode *node)
{
    node->graphnode.in = node->graphnode.out = NULL;
    node->graphnode.up = node->graphnode.down = node->graphnode.next = NULL;
    node->graphnode.low = 0;
    node->graphnode.allocs.bytes.direct = node->graphnode.allocs.bytes.total = 0;
    node->graphnode.allocs.calls.direct = node->graphnode.allocs.calls.total = 0;
    node->graphnode.frees.bytes.direct = node->graphnode.frees.bytes.total = 0;
    node->graphnode.frees.calls.direct = node->graphnode.frees.calls.total = 0;
    node->graphnode.sqsum = 0;
    node->graphnode.sort = -1;
    node->sourcefile = NULL;
    node->linenumber = 0;
}

static PLHashEntry *method_allocentry(void *pool, const void *key)
{
    tmmethodnode *node =
        (tmmethodnode*) arena_alloc(pool, sizeof(tmmethodnode));
    if (!node)
        return NULL;
    init_method(node);
    return &node->graphnode.entry;
}

static void graphnode_freeentry(void *pool, PLHashEntry *he, unsigned flag)
{
    /* Always free the value, which points to a strdup'd string. */
    free(he->value);
#if 0                           /* using arenas now, no freeing! */
    /* Free the whole thing if we're told to. */
    if (flag == HT_FREE_ENTRY)
        free((void*) he);
#endif
}

static void component_freeentry(void *pool, PLHashEntry *he, unsigned flag)
{
    if (flag == HT_FREE_ENTRY) {
        tmgraphnode *comp = (tmgraphnode*) he;

        /* Free the key, which was strdup'd (N.B. value also points to it). */
        free((void*) tmcomponent_name(comp));
#if 0                           /* using arenas now, no freeing! */
        free((void*) comp);
#endif
    }
}

static PLHashAllocOps filename_hashallocops = {
    generic_alloctable,     generic_freetable,
    filename_allocentry,    graphnode_freeentry
};

static PLHashAllocOps callsite_hashallocops = {
    generic_alloctable,     generic_freetable,
    callsite_allocentry,    graphnode_freeentry
};

static PLHashAllocOps graphnode_hashallocops = {
    generic_alloctable,     generic_freetable,
    graphnode_allocentry,   graphnode_freeentry
};

static PLHashAllocOps method_hashallocops = {
    generic_alloctable,     generic_freetable,
    method_allocentry,      graphnode_freeentry
};

static PLHashAllocOps component_hashallocops = {
    generic_alloctable,     generic_freetable,
    graphnode_allocentry,   component_freeentry
};

static PLHashNumber hash_serial(const void *key)
{
    return (PLHashNumber) key;
}

tmreader *tmreader_new(const char *program, void *data)
{
    tmreader *tmr;

    tmr = calloc(1, sizeof *tmr);
    if (!tmr)
        return NULL;
    tmr->program = program;
    tmr->data = data;
    PL_INIT_ARENA_POOL(&tmr->arena, "TMReader", 256*1024);

    tmr->libraries = PL_NewHashTable(100, hash_serial, PL_CompareValues,
                                     PL_CompareStrings, &graphnode_hashallocops,
                                     &tmr->arena);
    tmr->filenames = PL_NewHashTable(100, hash_serial, PL_CompareValues,
                                     PL_CompareStrings, &filename_hashallocops,
                                     &tmr->arena);
    tmr->components = PL_NewHashTable(10000, PL_HashString, PL_CompareStrings,
                                      PL_CompareValues, &component_hashallocops,
                                      &tmr->arena);
    tmr->methods = PL_NewHashTable(10000, hash_serial, PL_CompareValues,
                                   PL_CompareStrings, &method_hashallocops,
                                   &tmr->arena);
    tmr->callsites = PL_NewHashTable(200000, hash_serial, PL_CompareValues,
                                     PL_CompareValues, &callsite_hashallocops,
                                     &tmr->arena);
    tmr->calltree_root.entry.value = (void*) strdup("root");

    if (!tmr->libraries || !tmr->components || !tmr->methods ||
        !tmr->callsites || !tmr->calltree_root.entry.value ||
        !tmr->filenames) {
        tmreader_destroy(tmr);
        return NULL;
    }
    return tmr;
}

void tmreader_destroy(tmreader *tmr)
{
    if (tmr->libraries)
        PL_HashTableDestroy(tmr->libraries);
    if (tmr->filenames)
        PL_HashTableDestroy(tmr->filenames);
    if (tmr->components)
        PL_HashTableDestroy(tmr->components);
    if (tmr->methods)
        PL_HashTableDestroy(tmr->methods);
    if (tmr->callsites)
        PL_HashTableDestroy(tmr->callsites);
    PL_FinishArenaPool(&tmr->arena);
    free(tmr);
}

int tmreader_eventloop(tmreader *tmr, const char *filename,
                       tmeventhandler eventhandler)
{
    FILE *fp;
    char buf[NS_TRACE_MALLOC_MAGIC_SIZE];
    tmevent event;
    static const char magic[] = NS_TRACE_MALLOC_MAGIC;

    if (strcmp(filename, "-") == 0) {
        fp = stdin;
    } else {
#if defined(XP_WIN32)
        fp = fopen(filename, "rb");
#else
        fp = fopen(filename, "r");
#endif
        if (!fp) {
            fprintf(stderr, "%s: can't open %s: %s.\n",
                    tmr->program, filename, strerror(errno));
            return 0;
        }
    }

    if (read(fileno(fp), buf, sizeof buf) != sizeof buf ||
        strncmp(buf, magic, sizeof buf) != 0) {
        fprintf(stderr, "%s: bad magic string %s at start of %s.\n",
                tmr->program, buf, filename);
        fprintf(stderr, "either the data file is out of date,\nor your tools are out of date.\n");
        return 0;
    }

    /* Read in ticks per second. Used to convert platform specific intervals to time values */
    if (read(fileno(fp), &tmr->ticksPerSec, sizeof tmr->ticksPerSec) != sizeof tmr->ticksPerSec) {
        fprintf(stderr, "%s: Cannot read ticksPerSec. Log file read error.\n",
                tmr->program);
        return 0;
    }
    tmr->ticksPerSec = PR_ntohl(tmr->ticksPerSec);
#ifdef DEBUG_dp
    printf("DEBUG: ticks per sec = %d\n", tmr->ticksPerSec);
#endif
    while (get_tmevent(fp, &event)) {
        switch (event.type) {
          case TM_EVENT_LIBRARY: {
            const void *key;
            PLHashNumber hash;
            PLHashEntry **hep, *he;

            key = (const void*) (uintptr_t) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(tmr->libraries, hash, key);
            he = *hep;
            PR_ASSERT(!he);
            if (he) exit(2);

            he = PL_HashTableRawAdd(tmr->libraries, hep, hash, key,
                                    event.u.libname);
            if (!he) {
                perror(tmr->program);
                return -1;
            }
            break;
          }

          case TM_EVENT_FILENAME: {
            const void *key;
            PLHashNumber hash;
            PLHashEntry **hep, *he;

            key = (const void*) (uintptr_t) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(tmr->filenames, hash, key);
            he = *hep;
            PR_ASSERT(!he);
            if (he) exit(2);

            he = PL_HashTableRawAdd(tmr->filenames, hep, hash, key,
                                    event.u.srcname);
            if (!he) {
                perror(tmr->program);
                return -1;
            }
            break;
          }

          case TM_EVENT_METHOD: {
            const void *key, *sourcekey;
            PLHashNumber hash, sourcehash;
            PLHashEntry **hep, *he, **sourcehep, *sourcehe;
            char *name, *head, *mark, save;
            tmgraphnode *comp, *lib;
            tmmethodnode *meth;

            key = (const void*) (uintptr_t) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(tmr->methods, hash, key);
            he = *hep;
            PR_ASSERT(!he);
            if (he) exit(2);

            name = event.u.method.name;
            he = PL_HashTableRawAdd(tmr->methods, hep, hash, key, name);
            if (!he) {
                perror(tmr->program);
                return -1;
            }
            meth = (tmmethodnode*) he;

            meth->linenumber = event.u.method.linenumber;
            sourcekey = (const void*) (uintptr_t) event.u.method.filename;
            sourcehash = hash_serial(sourcekey);
            sourcehep = PL_HashTableRawLookup(tmr->filenames, sourcehash, sourcekey);
            sourcehe = *sourcehep;
            meth->sourcefile = filename_name(sourcehe);

            head = name;
            mark = strchr(name, ':');
            if (!mark) {
                mark = name;
                while (*mark != '\0' && *mark == '_')
                    mark++;
                head = mark;
                mark = strchr(head, '_');
                if (!mark) {
                    mark = strchr(head, '+');
                    if (!mark)
                        mark = head + strlen(head);
                }
            }

            save = *mark;
            *mark = '\0';
            hash = PL_HashString(head);
            hep = PL_HashTableRawLookup(tmr->components, hash, head);
            he = *hep;
            if (he) {
                comp = (tmgraphnode*) he;
            } else {
                head = strdup(head);
                if (head) {
                    he = PL_HashTableRawAdd(tmr->components, hep, hash, head,
                                            head);
                }
                if (!he) {
                    perror(tmr->program);
                    return -1;
                }
                comp = (tmgraphnode*) he;

                key = (const void*) (uintptr_t) event.u.method.library;
                hash = hash_serial(key);
                lib = (tmgraphnode*)
                      *PL_HashTableRawLookup(tmr->libraries, hash, key);
                if (lib) {
                    comp->up = lib;
                    comp->next = lib->down;
                    lib->down = comp;
                }
            }
            *mark = save;

            meth->graphnode.up = comp;
            meth->graphnode.next = comp->down;
            comp->down = &(meth->graphnode);
            break;
          }

          case TM_EVENT_CALLSITE: {
            const void *key, *mkey;
            PLHashNumber hash, mhash;
            PLHashEntry **hep, *he;
            tmcallsite *site, *parent;
            tmmethodnode *meth;

            key = (const void*) (uintptr_t) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(tmr->callsites, hash, key);
            he = *hep;

            /* there should not be an entry here! */
            PR_ASSERT(!he);
            if (he) exit(2);

            if (event.u.site.parent == 0) {
                parent = &tmr->calltree_root;
            } else {
                parent = tmreader_callsite(tmr, event.u.site.parent);
                if (!parent) {
                    fprintf(stderr, "%s: no parent for %lu (%lu)!\n",
                            tmr->program, (unsigned long) event.serial,
                            (unsigned long) event.u.site.parent);
                    continue;
                }
            }

            he = PL_HashTableRawAdd(tmr->callsites, hep, hash, key, NULL);
            if (!he) {
                perror(tmr->program);
                return -1;
            }

            site = (tmcallsite*) he;
            site->parent = parent;
            site->siblings = parent->kids;
            parent->kids = site;
            site->kids = NULL;

            mkey = (const void*) (uintptr_t) event.u.site.method;
            mhash = hash_serial(mkey);
            meth = (tmmethodnode*)
                   *PL_HashTableRawLookup(tmr->methods, mhash, mkey);
            site->method = meth;
            site->offset = event.u.site.offset;
            site->allocs.bytes.direct = site->allocs.bytes.total = 0;
            site->allocs.calls.direct = site->allocs.calls.total = 0;
            site->frees.bytes.direct = site->frees.bytes.total = 0;
            site->frees.calls.direct = site->frees.calls.total = 0;
            break;
          }

          case TM_EVENT_MALLOC:
          case TM_EVENT_CALLOC:
          case TM_EVENT_REALLOC: {
            tmcallsite *site;
            uint32_t size, oldsize;
            double delta, sqdelta, sqszdelta = 0;
            tmgraphnode *comp, *lib;
            tmmethodnode *meth;

            site = tmreader_callsite(tmr, event.serial);
            if (!site) {
                fprintf(stderr, "%s: no callsite for '%c' (%lu)!\n",
                        tmr->program, event.type, (unsigned long) event.serial);
                continue;
            }

            size = event.u.alloc.size;
            oldsize = event.u.alloc.oldsize;
            delta = (double)size - (double)oldsize;
            site->allocs.bytes.direct += (unsigned long)delta;
            if (event.type != TM_EVENT_REALLOC)
                site->allocs.calls.direct++;
            meth = site->method;
            if (meth) {
                meth->graphnode.allocs.bytes.direct += (unsigned long)delta;
                sqdelta = delta * delta;
                if (event.type == TM_EVENT_REALLOC) {
                    sqszdelta = ((double)size * size)
                              - ((double)oldsize * oldsize);
                    meth->graphnode.sqsum += sqszdelta;
                } else {
                    meth->graphnode.sqsum += sqdelta;
                    meth->graphnode.allocs.calls.direct++;
                }
                comp = meth->graphnode.up;
                if (comp) {
                    comp->allocs.bytes.direct += (unsigned long)delta;
                    if (event.type == TM_EVENT_REALLOC) {
                        comp->sqsum += sqszdelta;
                    } else {
                        comp->sqsum += sqdelta;
                        comp->allocs.calls.direct++;
                    }
                    lib = comp->up;
                    if (lib) {
                        lib->allocs.bytes.direct += (unsigned long)delta;
                        if (event.type == TM_EVENT_REALLOC) {
                            lib->sqsum += sqszdelta;
                        } else {
                            lib->sqsum += sqdelta;
                            lib->allocs.calls.direct++;
                        }
                    }
                }
            }
            break;
          }

          case TM_EVENT_FREE: {
            tmcallsite *site;
            uint32_t size;
            tmgraphnode *comp, *lib;
            tmmethodnode *meth;

            site = tmreader_callsite(tmr, event.serial);
            if (!site) {
                fprintf(stderr, "%s: no callsite for '%c' (%lu)!\n",
                        tmr->program, event.type, (unsigned long) event.serial);
                continue;
            }
            size = event.u.alloc.size;
            site->frees.bytes.direct += size;
            site->frees.calls.direct++;
            meth = site->method;
            if (meth) {
                meth->graphnode.frees.bytes.direct += size;
                meth->graphnode.frees.calls.direct++;
                comp = meth->graphnode.up;
                if (comp) {
                    comp->frees.bytes.direct += size;
                    comp->frees.calls.direct++;
                    lib = comp->up;
                    if (lib) {
                        lib->frees.bytes.direct += size;
                        lib->frees.calls.direct++;
                    }
                }
            }
            break;
          }

          case TM_EVENT_STATS:
            break;
        }

        eventhandler(tmr, &event);
    }

    return 1;
}

tmgraphnode *tmreader_library(tmreader *tmr, uint32_t serial)
{
    const void *key;
    PLHashNumber hash;

    key = (const void*) (uintptr_t) serial;
    hash = hash_serial(key);
    return (tmgraphnode*) *PL_HashTableRawLookup(tmr->libraries, hash, key);
}

tmgraphnode *tmreader_filename(tmreader *tmr, uint32_t serial)
{
    const void *key;
    PLHashNumber hash;

    key = (const void*) (uintptr_t) serial;
    hash = hash_serial(key);
    return (tmgraphnode*) *PL_HashTableRawLookup(tmr->filenames, hash, key);
}

tmgraphnode *tmreader_component(tmreader *tmr, const char *name)
{
    PLHashNumber hash;

    hash = PL_HashString(name);
    return (tmgraphnode*) *PL_HashTableRawLookup(tmr->components, hash, name);
}

tmmethodnode *tmreader_method(tmreader *tmr, uint32_t serial)
{
    const void *key;
    PLHashNumber hash;

    key = (const void*) (uintptr_t) serial;
    hash = hash_serial(key);
    return (tmmethodnode*) *PL_HashTableRawLookup(tmr->methods, hash, key);
}

tmcallsite *tmreader_callsite(tmreader *tmr, uint32_t serial)
{
    const void *key;
    PLHashNumber hash;

    key = (const void*) (uintptr_t) serial;
    hash = hash_serial(key);
    return (tmcallsite*) *PL_HashTableRawLookup(tmr->callsites, hash, key);
}

int tmgraphnode_connect(tmgraphnode *from, tmgraphnode *to, tmcallsite *site)
{
    tmgraphlink *outlink;
    tmgraphedge *edge;

    for (outlink = from->out; outlink; outlink = outlink->next) {
        if (outlink->node == to) {
            /*
             * Say the stack looks like this: ... => JS => js => JS => js.
             * We must avoid overcounting JS=>js because the first edge total
             * includes the second JS=>js edge's total (which is because the
             * lower site's total includes all its kids' totals).
             */
            edge = TM_LINK_TO_EDGE(outlink, TM_EDGE_OUT_LINK);
            if (!to->low || to->low < from->low) {
                /* Add the direct and total counts to edge->allocs. */
                edge->allocs.bytes.direct += site->allocs.bytes.direct;
                edge->allocs.bytes.total += site->allocs.bytes.total;
                edge->allocs.calls.direct += site->allocs.calls.direct;
                edge->allocs.calls.total += site->allocs.calls.total;

                /* Now update the free counts. */
                edge->frees.bytes.direct += site->frees.bytes.direct;
                edge->frees.bytes.total += site->frees.bytes.total;
                edge->frees.calls.direct += site->frees.calls.direct;
                edge->frees.calls.total += site->frees.calls.total;
            }
            return 1;
        }
    }

    edge = (tmgraphedge*) malloc(sizeof(tmgraphedge));
    if (!edge)
        return 0;
    edge->links[TM_EDGE_OUT_LINK].node = to;
    edge->links[TM_EDGE_OUT_LINK].next = from->out;
    from->out = &edge->links[TM_EDGE_OUT_LINK];
    edge->links[TM_EDGE_IN_LINK].node = from;
    edge->links[TM_EDGE_IN_LINK].next = to->in;
    to->in = &edge->links[TM_EDGE_IN_LINK];
    edge->allocs = site->allocs;
    edge->frees = site->frees;
    return 1;
}
