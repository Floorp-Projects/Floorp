/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is tmreader.h/tmreader.c code, released
 * July 7, 2000.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich, 7-July-2000
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
