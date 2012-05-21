/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef tmreader_h___
#define tmreader_h___

#include "prtypes.h"
#include "plhash.h"
#include "nsTraceMalloc.h"
#include "plarena.h"

PR_BEGIN_EXTERN_C

typedef struct tmreader     tmreader;
typedef struct tmevent      tmevent;
typedef struct tmcounts     tmcounts;
typedef struct tmallcounts  tmallcounts;
typedef struct tmgraphlink  tmgraphlink;
typedef struct tmgraphedge  tmgraphedge;
typedef struct tmgraphnode  tmgraphnode;
typedef struct tmcallsite   tmcallsite;
typedef struct tmmethodnode tmmethodnode;

struct tmevent {
    char            type;
    uint32          serial;
    union {
        char        *libname;
        char        *srcname;
        struct {
            uint32  library;
            uint32  filename;
            uint32  linenumber;
            char    *name;
        } method;
        struct {
            uint32  parent;
            uint32  method;
            uint32  offset;
        } site;
        struct {
            uint32  interval; /* in ticks */
            uint32  ptr;
            uint32  size;
            uint32  oldserial;
            uint32  oldptr;
            uint32  oldsize;
            uint32  cost;     /* in ticks */
        } alloc;
        struct {
            nsTMStats tmstats;
            uint32  calltree_maxkids_parent;
            uint32  calltree_maxstack_top;
        } stats;
    } u;
};

struct tmcounts {
    uint32          direct;     /* things allocated by this node's code */
    uint32          total;      /* direct + things from all descendents */
};

struct tmallcounts {
    tmcounts        bytes;
    tmcounts        calls;
};

struct tmgraphnode {
    PLHashEntry     entry;      /* key is serial or name, value must be name */
    tmgraphlink     *in;
    tmgraphlink     *out;
    tmgraphnode     *up;        /* parent in supergraph, e.g., JS for JS_*() */
    tmgraphnode     *down;      /* subgraph kids, declining bytes.total order */
    tmgraphnode     *next;      /* next kid in supergraph node's down list */
    int             low;        /* 0 or lowest current tree walk level */
    tmallcounts     allocs;
    tmallcounts     frees;
    double          sqsum;      /* sum of squared bytes.direct */
    int             sort;       /* sorted index in node table, -1 if no table */
};

struct tmmethodnode {
    tmgraphnode   graphnode;
    char          *sourcefile;
    uint32        linenumber;
};

#define tmgraphnode_name(node)  ((char*) (node)->entry.value)
#define tmmethodnode_name(node)  ((char*) (node)->graphnode.entry.value)

#define tmlibrary_serial(lib)   ((uint32) (lib)->entry.key)
#define tmcomponent_name(comp)  ((const char*) (comp)->entry.key)
#define filename_name(hashentry) ((char*)hashentry->value)

/* Half a graphedge, not including per-edge allocation stats. */
struct tmgraphlink {
    tmgraphlink     *next;      /* next fanning out from or into a node */
    tmgraphnode     *node;      /* the other node (to if OUT, from if IN) */
};

/*
 * It's safe to downcast a "from" tmgraphlink (one linked from a node's out
 * pointer) to tmgraphedge.  To go from an "out" (linked via tmgraphedge.from)
 * or "in" (linked via tmgraphedge.to) list link to its containing edge, use
 * TM_LINK_TO_EDGE(link, which).
 */
struct tmgraphedge {
    tmgraphlink     links[2];
    tmallcounts     allocs;
    tmallcounts     frees;
};

/* Indices into tmgraphedge.links -- out must come first. */
#define TM_EDGE_OUT_LINK        0
#define TM_EDGE_IN_LINK         1

#define TM_LINK_TO_EDGE(link,which) ((tmgraphedge*) &(link)[-(which)])

struct tmcallsite {
    PLHashEntry     entry;      /* key is site serial number */
    tmcallsite      *parent;    /* calling site */
    tmcallsite      *siblings;  /* other sites reached from parent */
    tmcallsite      *kids;      /* sites reached from here */
    tmmethodnode    *method;    /* method node in tmr->methods graph */
    uint32          offset;     /* pc offset from start of method */
    tmallcounts     allocs;
    tmallcounts     frees;
    void            *data;      /* tmreader clients can stick arbitrary
                                 *  data onto a callsite.
                                 */
};

struct tmreader {
    const char      *program;
    void            *data;
    PLHashTable     *libraries;
    PLHashTable     *filenames;
    PLHashTable     *components;
    PLHashTable     *methods;
    PLHashTable     *callsites;
    PLArenaPool     arena;
    tmcallsite      calltree_root;
    uint32          ticksPerSec;
};

typedef void (*tmeventhandler)(tmreader *tmr, tmevent *event);

/* The tmreader constructor and destructor. */
extern tmreader     *tmreader_new(const char *program, void *data);
extern void         tmreader_destroy(tmreader *tmr);

/*
 * Return -1 on permanent fatal error, 0 if filename can't be opened or is not
 * a trace-malloc logfile, and 1 on success.
 */
extern int          tmreader_eventloop(tmreader *tmr, const char *filename,
                                       tmeventhandler eventhandler);

/* Map serial number or name to graphnode or callsite. */
extern tmgraphnode  *tmreader_library(tmreader *tmr, uint32 serial);
extern tmgraphnode  *tmreader_filename(tmreader *tmr, uint32 serial);
extern tmgraphnode  *tmreader_component(tmreader *tmr, const char *name);
extern tmmethodnode  *tmreader_method(tmreader *tmr, uint32 serial);
extern tmcallsite   *tmreader_callsite(tmreader *tmr, uint32 serial);

/*
 * Connect node 'from' to node 'to' with an edge, if there isn't one already
 * connecting the nodes.  Add site's allocation stats to the edge only if we
 * create the edge, or if we find that it exists, but that to->low is zero or
 * less than from->low.
 *
 * If the callsite tree already totals allocation costs (tmcounts.total for
 * each site includes tmcounts.direct for that site, plus tmcounts.total for
 * all kid sites), then the node->low watermarks should be set from the tree
 * level when walking the callsite tree, and should be set to non-zero values
 * only if zero (the root is at level 0).  A low watermark should be cleared
 * when the tree walk unwinds past the level at which it was set non-zero.
 *
 * Return 0 on error (malloc failure) and 1 on success.
 */
extern int tmgraphnode_connect(tmgraphnode *from, tmgraphnode *to,
                               tmcallsite *site);

PR_END_EXTERN_C

#endif /* tmreader_h___ */
