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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "prtypes.h"
#include "prlog.h"
#include "prprf.h"
#include "plhash.h"
#include "nsTraceMalloc.h"

static char   *program;
static int    sort_by_direct = 0;
static int    do_tree_dump = 0;
static char   *function_dump = NULL;
static int32  min_subtotal = 0;

static int accum_byte(FILE *fp, uint32 *uip)
{
    int c = getc(fp);
    if (c == EOF)
        return 0;
    *uip = (*uip << 8) | c;
    return 1;
}

static int get_uint32(FILE *fp, uint32 *uip)
{
    int c;
    uint32 ui;

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
                    ui = (uint32) c;
                }
                if (!accum_byte(fp, &ui))
                    return 0;
            } else {
                ui = (uint32) c;
            }
            if (!accum_byte(fp, &ui))
                return 0;
        } else {
            ui = (uint32) c;
        }
        if (!accum_byte(fp, &ui))
            return 0;
    } else {
        ui = (uint32) c;
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

typedef struct logevent {
    char            type;
    uint32          serial;
    union {
        char        *libname;
        struct {
            uint32  library;
            char    *name;
        } method;
        struct {
            uint32  parent;
            uint32  method;
            uint32  offset;
        } site;
        struct {
            uint32  oldsize;
            uint32  size;
        } alloc;
        struct {
            struct nsTMStats tmstats;
            uint32 calltree_maxkids_parent;
            uint32 calltree_maxstack_top;
        } stats;
    } u;
} logevent;

static int get_logevent(FILE *fp, logevent *event)
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
      case 'L':
        s = get_string(fp);
        if (!s)
            return 0;
        event->u.libname = s;
        break;

      case 'N':
        if (!get_uint32(fp, &event->u.method.library))
            return 0;
        s = get_string(fp);
        if (!s)
            return 0;
        event->u.method.name = s;
        break;

      case 'S':
        if (!get_uint32(fp, &event->u.site.parent))
            return 0;
        if (!get_uint32(fp, &event->u.site.method))
            return 0;
        if (!get_uint32(fp, &event->u.site.offset))
            return 0;
        break;

      case 'M':
      case 'C':
      case 'F':
        event->u.alloc.oldsize = 0;
        if (!get_uint32(fp, &event->u.alloc.size))
            return 0;
        break;

      case 'R':
        if (!get_uint32(fp, &event->u.alloc.oldsize))
            return 0;
        if (!get_uint32(fp, &event->u.alloc.size))
            return 0;
        break;

      case 'Z':
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_maxstack)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_maxdepth)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_parents)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_maxkids)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_kidhits)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_kidmisses)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calltree_kidsteps)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.callsite_recurrences)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.backtrace_calls)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.backtrace_failures)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.btmalloc_failures)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.dladdr_failures)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.malloc_calls)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.malloc_failures)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calloc_calls)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.calloc_failures)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.realloc_calls)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.realloc_failures)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.free_calls)) return 0;
        if (!get_uint32(fp, &event->u.stats.tmstats.null_free_calls)) return 0;
        if (!get_uint32(fp, &event->u.stats.calltree_maxkids_parent)) return 0;
        if (!get_uint32(fp, &event->u.stats.calltree_maxstack_top)) return 0;
        break;
    }
    return 1;
}

typedef struct graphedge    graphedge;
typedef struct graphnode    graphnode;
typedef struct callsite     callsite;

typedef struct counts {
    int32       direct;         /* things allocated by this node's code */
    int32       total;          /* direct + things from all descendents */
} counts;

struct graphnode {
    PLHashEntry entry;          /* key is serial or name, value must be name */
    graphedge   *in;
    graphedge   *out;
    graphnode   *up;
    int         low;            /* 0 or lowest current tree walk level */
    counts      bytes;          /* bytes (direct and total) allocated */
    counts      allocs;         /* number of allocations */
    double      sqsum;          /* sum of squared bytes.direct */
};

#define graphnode_name(node)    ((char*) (node)->entry.value)

#define library_serial(lib)     ((uint32) (lib)->entry.key)
#define component_name(comp)    ((const char*) (comp)->entry.key)

struct graphedge {
    graphedge   *next;
    graphnode   *node;
    counts      bytes;
};

struct callsite {
    PLHashEntry entry;
    callsite    *parent;
    callsite    *siblings;
    callsite    *kids;
    graphnode   *method;
    uint32      offset;
    counts      bytes;
    counts      allocs;
};

#define callsite_serial(site)    ((uint32) (site)->entry.key)

static void connect_nodes(graphnode *from, graphnode *to, callsite *site)
{
    graphedge *edge;

    for (edge = from->out; edge; edge = edge->next) {
        if (edge->node == to) {
            /*
             * Say the stack looks like this: ... => JS => js => JS => js.
             * We must avoid overcounting JS=>js because the first edge total
             * includes the second JS=>js edge's total (which is because the
             * lower site's total includes all its kids' totals).
             */
            if (!to->low || to->low < from->low) {
                edge[0].bytes.direct += site->bytes.direct;
                edge[1].bytes.direct += site->bytes.direct;
                edge[0].bytes.total += site->bytes.total;
                edge[1].bytes.total += site->bytes.total;
            }
            return;
        }
    }
    edge = (graphedge*) malloc(2 * sizeof(graphedge));
    if (!edge) {
        perror(program);
        exit(1);
    }
    edge[0].node = to;
    edge[0].next = from->out;
    from->out = &edge[0];
    edge[1].node = from;
    edge[1].next = to->in;
    to->in = &edge[1];
    edge[0].bytes.direct = edge[1].bytes.direct = site->bytes.direct;
    edge[0].bytes.total = edge[1].bytes.total = site->bytes.total;
}

static void *generic_alloctable(void *pool, PRSize size)
{
    return malloc(size);
}

static void generic_freetable(void *pool, void *item)
{
    free(item);
}

static PLHashEntry *callsite_allocentry(void *pool, const void *key)
{
    return malloc(sizeof(callsite));
}

static PLHashEntry *graphnode_allocentry(void *pool, const void *key)
{
    graphnode *node = (graphnode*) malloc(sizeof(graphnode));
    if (node) {
        node->in = node->out = NULL;
        node->up = NULL;
        node->low = 0;
        node->bytes.direct = node->bytes.total = 0;
        node->allocs.direct = node->allocs.total = 0;
        node->sqsum = 0;
    }
    return &node->entry;
}

static void graphnode_freeentry(void *pool, PLHashEntry *he, PRUintn flag)
{
    /* Always free the value, which points to a strdup'd string. */
    free(he->value);

    /* Free the whole thing if we're told to. */
    if (flag == HT_FREE_ENTRY)
        free((void*) he);
}

static void component_freeentry(void *pool, PLHashEntry *he, PRUintn flag)
{
    if (flag == HT_FREE_ENTRY) {
        graphnode *comp = (graphnode*) he;

        /* Free the key, which was strdup'd (N.B. value also points to it). */
        free((void*) component_name(comp));
        free((void*) comp);
    }
}

static PLHashAllocOps callsite_hashallocops = {
    generic_alloctable,     generic_freetable,
    callsite_allocentry,    graphnode_freeentry
};

static PLHashAllocOps graphnode_hashallocops = {
    generic_alloctable,     generic_freetable,
    graphnode_allocentry,   graphnode_freeentry
};

static PLHashAllocOps component_hashallocops = {
    generic_alloctable,     generic_freetable,
    graphnode_allocentry,   component_freeentry
};

static PLHashNumber hash_serial(const void *key)
{
    return (PLHashNumber) key;
}

static PLHashTable *libraries;
static PLHashTable *components;
static PLHashTable *methods;
static PLHashTable *callsites;
static callsite     calltree_root;

static void compute_callsite_totals(callsite *site)
{
    callsite *kid;

    site->bytes.total += site->bytes.direct;
    site->allocs.total += site->allocs.direct;
    for (kid = site->kids; kid; kid = kid->siblings) {
        compute_callsite_totals(kid);
        site->bytes.total += kid->bytes.total;
        site->allocs.total += kid->allocs.total;
    }
}

static void walk_callsite_tree(callsite *site, int level, int kidnum, FILE *fp)
{
    callsite *parent;
    graphnode *meth, *pmeth, *comp, *pcomp, *lib, *plib;
    int old_meth_low, old_comp_low, old_lib_low, nkids;
    callsite *kid;

    parent = site->parent;
    meth = comp = lib = NULL;
    if (parent) {
        meth = site->method;
        if (meth) {
            pmeth = parent->method;
            if (pmeth && pmeth != meth) {
                if (!meth->low) {
                    meth->bytes.total += site->bytes.total;
                    meth->allocs.total += site->allocs.total;
                }
                connect_nodes(pmeth, meth, site);

                comp = meth->up;
                if (comp) {
                    pcomp = pmeth->up;
                    if (pcomp && pcomp != comp) {
                        if (!comp->low) {
                            comp->bytes.total += site->bytes.total;
                            comp->allocs.total += site->allocs.total;
                        }
                        connect_nodes(pcomp, comp, site);

                        lib = comp->up;
                        if (lib) {
                            plib = pcomp->up;
                            if (plib && plib != lib) {
                                if (!lib->low) {
                                    lib->bytes.total += site->bytes.total;
                                    lib->allocs.total += site->allocs.total;
                                }
                                connect_nodes(plib, lib, site);
                            }
                            old_lib_low = lib->low;
                            if (!old_lib_low)
                                lib->low = level;
                        }
                    }
                    old_comp_low = comp->low;
                    if (!old_comp_low)
                        comp->low = level;
                }
            }
            old_meth_low = meth->low;
            if (!old_meth_low)
                meth->low = level;
        }
    }

    if (do_tree_dump) {
        fprintf(fp, "%c%*s%3d %3d %s %lu %ld\n",
                site->kids ? '+' : '-', level, "", level, kidnum,
                meth ? graphnode_name(meth) : "???",
                (unsigned long)site->bytes.direct, (long)site->bytes.total);
    }
    nkids = 0;
    for (kid = site->kids; kid; kid = kid->siblings) {
        walk_callsite_tree(kid, level + 1, nkids, fp);
        nkids++;
    }

    if (meth) {
        if (!old_meth_low)
            meth->low = 0;
        if (comp) {
            if (!old_comp_low)
                comp->low = 0;
            if (lib) {
                if (!old_lib_low)
                    lib->low = 0;
            }
        }
    }
}

static PRIntn tabulate_node(PLHashEntry *he, PRIntn i, void *arg)
{
    graphnode **table = (graphnode**) arg;

    table[i] = (graphnode*) he;
    return HT_ENUMERATE_NEXT;
}

/* Sort in reverse size order, so biggest node comes first. */
static int node_table_compare(const void *p1, const void *p2)
{
    const graphnode *node1, *node2;
    int32 key1, key2;

    node1 = *(const graphnode**) p1;
    node2 = *(const graphnode**) p2;
    if (sort_by_direct) {
        key1 = node1->bytes.direct;
        key2 = node2->bytes.direct;
    } else {
        key1 = node1->bytes.total;
        key2 = node2->bytes.total;
    }
    return key2 - key1;
}

static int mean_size_compare(const void *p1, const void *p2)
{
    const graphnode *node1, *node2;
    double div1, div2, key1, key2;

    node1 = *(const graphnode**) p1;
    node2 = *(const graphnode**) p2;
    div1 = (double)node1->allocs.direct;
    div2 = (double)node2->allocs.direct;
    if (div1 == 0 || div2 == 0)
        return div2 - div1;
    key1 = (double)node1->bytes.direct / div1;
    key2 = (double)node2->bytes.direct / div2;
    if (key1 < key2)
        return 1;
    if (key1 > key2)
        return -1;
    return 0;
}

static const char *prettybig(uint32 num, char *buf, size_t limit)
{
    if (num >= 1000000000)
        PR_snprintf(buf, limit, "%1.2fG", (double) num / 1e9);
    else if (num >= 1000000)
        PR_snprintf(buf, limit, "%1.2fM", (double) num / 1e6);
    else if (num >= 1000)
        PR_snprintf(buf, limit, "%1.2fK", (double) num / 1e3);
    else
        PR_snprintf(buf, limit, "%lu", (unsigned long) num);
    return buf;
}

static double percent(int32 num, int32 total)
{
    if (num == 0)
        return 0.0;
    return ((double) num * 100) / (double) total;
}

/* Linked list bubble-sort (waterson and brendan went bald hacking this). */
static void sort_graphedge_list(graphedge **currp)
{
    graphedge *curr, *next, **nextp, *tmp;

    while ((curr = *currp) != NULL && curr->next) {
        nextp = &curr->next;
        while ((next = *nextp) != NULL) {
            if (curr->bytes.total < next->bytes.total) {
                tmp = curr->next;
                *currp = tmp;
                if (tmp == next) {
                    PR_ASSERT(nextp == &curr->next);
                    curr->next = next->next;
                    next->next = curr;
                } else {
                    *nextp = next->next;
                    curr->next = next->next;
                    next->next = tmp;
                    *currp = next;
                    *nextp = curr;
                    nextp = &curr->next;
                }
                curr = next;
                continue;
            }
            nextp = &next->next;
        }
        currp = &curr->next;
    }
}

static void dump_graphedge_list(graphedge *list, FILE *fp)
{
    int32 total;
    graphedge *edge;
    char buf[16];

    fputs("<td valign=top>", fp);
    total = 0;
    for (edge = list; edge; edge = edge->next)
        total += edge->bytes.total;
    for (edge = list; edge; edge = edge->next) {
        fprintf(fp, "<a href='#%s'>%s&nbsp;(%1.2f%%)</a>\n",
                graphnode_name(edge->node),
                prettybig(edge->bytes.total, buf, sizeof buf),
                percent(edge->bytes.total, total));
    }
    fputs("</td>", fp);
}

static void dump_graph(PLHashTable *hashtbl, const char *title, FILE *fp)
{
    uint32 i, count;
    graphnode **table, *node;
    char *name;
    size_t namelen;
    char buf1[16], buf2[16], buf3[16], buf4[16];
    static char NA[] = "N/A";

    count = hashtbl->nentries;
    table = (graphnode**) malloc(count * sizeof(graphnode*));
    if (!table) {
        perror(program);
        exit(1);
    }
    PL_HashTableEnumerateEntries(hashtbl, tabulate_node, table);
    qsort(table, count, sizeof(graphnode*), node_table_compare);

    fprintf(fp,
            "<table border=1>\n"
              "<tr>"
                "<th>%s</th>"
                "<th>Total/Direct (percents)</th>"
                "<th>Allocations</th>"
                "<th>Fan-in</th>"
                "<th>Fan-out</th>"
              "</tr>\n",
            title);

    for (i = 0; i < count; i++) {
        /* Don't bother with truly puny nodes. */
        node = table[i];
        if (node->bytes.total < min_subtotal)
            break;

        name = graphnode_name(node);
        namelen = strlen(name);
        fprintf(fp,
                "<tr>"
                  "<td valign=top><a name='%s'>%.*s%s</td>"
                  "<td valign=top>%s/%s (%1.2f%%/%1.2f%%)</td>"
                  "<td valign=top>%s/%s (%1.2f%%/%1.2f%%)</td>",
                name,
                (namelen > 45) ? 45 : (int)namelen, name,
                (namelen > 45) ? "<i>...</i>" : "",
                prettybig(node->bytes.total, buf1, sizeof buf1),
                prettybig(node->bytes.direct, buf2, sizeof buf2),
                percent(node->bytes.total, calltree_root.bytes.total),
                percent(node->bytes.direct, calltree_root.bytes.total),
                prettybig(node->allocs.total, buf3, sizeof buf3),
                prettybig(node->allocs.direct, buf4, sizeof buf4),
                percent(node->allocs.total, calltree_root.allocs.total),
                percent(node->allocs.direct, calltree_root.allocs.total));

        sort_graphedge_list(&node->in);
        dump_graphedge_list(node->in, fp);
        sort_graphedge_list(&node->out);
        dump_graphedge_list(node->out, fp);

        fputs("</tr>\n", fp);
    }

    fputs("</table>\n<hr>\n", fp);

    qsort(table, count, sizeof(graphnode*), mean_size_compare);

    fprintf(fp,
            "<table border=1>\n"
              "<tr><th colspan=4>Direct Allocators</th></tr>\n"
              "<tr>"
                "<th>%s</th>"
                "<th>Mean&nbsp;Size</th>"
                "<th>StdDev</th>"
                "<th>Allocations<th>"
              "</tr>\n",
            title);

    for (i = 0; i < count; i++) {
        double allocs, bytes, mean, variance, sigma;

        node = table[i];
        allocs = (double)node->allocs.direct;
        if (!allocs)
            continue;

        /* Compute direct-size mean and standard deviation. */
        bytes = (double)node->bytes.direct;
        mean = bytes / allocs;
        variance = allocs * node->sqsum - bytes * bytes;
        if (variance < 0 || allocs == 1)
            variance = 0;
        else
            variance /= allocs * (allocs - 1);
        sigma = sqrt(variance);

        name = graphnode_name(node);
        namelen = strlen(name);
        fprintf(fp,
                "<tr>"
                  "<td valign=top>%.*s%s</td>"
                  "<td valign=top>%s</td>"
                  "<td valign=top>%s</td>"
                  "<td valign=top>%s</td>"
                "</tr>\n",
                (namelen > 65) ? 45 : (int)namelen, name,
                (namelen > 65) ? "<i>...</i>" : "",
                prettybig((uint32)mean, buf1, sizeof buf1),
                prettybig((uint32)sigma, buf2, sizeof buf2),
                prettybig(node->allocs.direct, buf3, sizeof buf3));
    }

    fputs("</table>\n", fp);
    free((void*) table);
}

static const char magic[] = NS_TRACE_MALLOC_MAGIC;

static void process(const char *filename, FILE *fp)
{
    char buf[NS_TRACE_MALLOC_MAGIC_SIZE];
    logevent event;

    if (read(fileno(fp), buf, sizeof buf) != sizeof buf ||
        strncmp(buf, magic, sizeof buf) != 0) {
        fprintf(stderr, "%s: bad magic string %s at start of %s.\n",
                program, buf, filename);
        exit(1);
    }

    while (get_logevent(fp, &event)) {
        switch (event.type) {
          case 'L': {
            const void *key;
            PLHashNumber hash;
            PLHashEntry **hep, *he;

            key = (const void*) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(libraries, hash, key);
            he = *hep;
            PR_ASSERT(!he);
            if (he) exit(2);

            he = PL_HashTableRawAdd(libraries, hep, hash, key, event.u.libname);
            if (!he) {
                perror(program);
                exit(1);
            }
            break;
          }

          case 'N': {
            const void *key;
            PLHashNumber hash;
            PLHashEntry **hep, *he;
            char *name, *head, *mark, save;
            graphnode *meth, *comp, *lib;

            key = (const void*) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(methods, hash, key);
            he = *hep;
            PR_ASSERT(!he);
            if (he) exit(2);

            name = event.u.method.name;
            he = PL_HashTableRawAdd(methods, hep, hash, key, name);
            if (!he) {
                perror(program);
                exit(1);
            }
            meth = (graphnode*) he;

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
            hep = PL_HashTableRawLookup(components, hash, head);
            he = *hep;
            if (he) {
                comp = (graphnode*) he;
            } else {
                head = strdup(head);
                if (head)
                    he = PL_HashTableRawAdd(components, hep, hash, head, head);
                if (!he) {
                    perror(program);
                    exit(1);
                }
                comp = (graphnode*) he;

                key = (const void*) event.u.method.library;
                hash = hash_serial(key);
                lib = (graphnode*) *PL_HashTableRawLookup(libraries, hash, key);
                comp->up = lib;
            }
            *mark = save;

            meth->up = comp;
            break;
          }

          case 'S': {
            const void *key, *pkey, *mkey;
            PLHashNumber hash, phash, mhash;
            PLHashEntry **hep, *he;
            callsite *site, *parent;
            graphnode *meth;

            key = (const void*) event.serial;
            hash = hash_serial(key);
            hep = PL_HashTableRawLookup(callsites, hash, key);
            he = *hep;
            PR_ASSERT(!he);
            if (he) exit(2);

            if (event.u.site.parent == 0) {
                parent = &calltree_root;
            } else {
                pkey = (const void*) event.u.site.parent;
                phash = hash_serial(pkey);
                parent = (callsite*)
                         *PL_HashTableRawLookup(callsites, phash, pkey);
                if (!parent) {
                    fprintf(stdout, "### no parent for %lu (%lu)!\n",
                            (unsigned long) event.serial,
                            (unsigned long) event.u.site.parent);
                    continue;
                }
            }

            he = PL_HashTableRawAdd(callsites, hep, hash, key, NULL);
            if (!he) {
                perror(program);
                exit(1);
            }

            site = (callsite*) he;
            site->parent = parent;
            site->siblings = parent->kids;
            parent->kids = site;
            site->kids = NULL;

            mkey = (const void*) event.u.site.method;
            mhash = hash_serial(mkey);
            meth = (graphnode*) *PL_HashTableRawLookup(methods, mhash, mkey);
            site->method = meth;
            site->offset = event.u.site.offset;
            site->bytes.direct = site->bytes.total = 0;
            site->allocs.direct = site->allocs.total = 0;
            break;
          }

          case 'M':
          case 'C':
          case 'R': {
            const void *key;
            PLHashNumber hash;
            callsite *site;
            int32 size, oldsize, delta;
            graphnode *meth, *comp, *lib;
            double sqdelta, sqszdelta;

            key = (const void*) event.serial;
            hash = hash_serial(key);
            site = (callsite*) *PL_HashTableRawLookup(callsites, hash, key);
            if (!site) {
                fprintf(stdout, "### no callsite for '%c' (%lu)!\n",
                        event.type, (unsigned long) event.serial);
                continue;
            }

            size = (int32)event.u.alloc.size;
            oldsize = (int32)event.u.alloc.oldsize;
            delta = size - oldsize;
            site->bytes.direct += delta;
            if (event.type != 'R')
                site->allocs.direct++;
            meth = site->method;
            if (meth) {
                meth->bytes.direct += delta;
                sqdelta = delta * delta;
                if (event.type == 'R') {
                    sqszdelta = ((double)size * size)
                              - ((double)oldsize * oldsize);
                    meth->sqsum += sqszdelta;
                } else {
                    meth->sqsum += sqdelta;
                    meth->allocs.direct++;
                }
                comp = meth->up;
                if (comp) {
                    comp->bytes.direct += delta;
                    if (event.type == 'R') {
                        comp->sqsum += sqszdelta;
                    } else {
                        comp->sqsum += sqdelta;
                        comp->allocs.direct++;
                    }
                    lib = comp->up;
                    if (lib) {
                        lib->bytes.direct += delta;
                        if (event.type == 'R') {
                            lib->sqsum += sqszdelta;
                        } else {
                            lib->sqsum += sqdelta;
                            lib->allocs.direct++;
                        }
                    }
                }
            }
            break;
          }

          case 'F':
            break;

          case 'Z':
            fprintf(stdout,
                    "<p><table border=1>"
                      "<tr><th>Counter</th><th>Value</th></tr>\n"
                      "<tr><td>maximum actual stack depth</td><td>%lu</td></tr>\n"
                      "<tr><td>maximum callsite tree depth</td><td>%lu</td></tr>\n"
                      "<tr><td>number of parent callsites</td><td>%lu</td></tr>\n"
                      "<tr><td>maximum kids per parent</td><td>%lu</td></tr>\n"
                      "<tr><td>hits looking for a kid</td><td>%lu</td></tr>\n"
                      "<tr><td>misses looking for a kid</td><td>%lu</td></tr>\n"
                      "<tr><td>steps over other kids</td><td>%lu</td></tr>\n"
                      "<tr><td>callsite recurrences</td><td>%lu</td></tr>\n"
                      "<tr><td>number of stack backtraces</td><td>%lu</td></tr>\n"
                      "<tr><td>backtrace failures</td><td>%lu</td></tr>\n"
                      "<tr><td>backtrace malloc failures</td><td>%lu</td></tr>\n"
                      "<tr><td>backtrace dladdr failures</td><td>%lu</td></tr>\n"
                      "<tr><td>malloc calls</td><td>%lu</td></tr>\n"
                      "<tr><td>malloc failures</td><td>%lu</td></tr>\n"
                      "<tr><td>calloc calls</td><td>%lu</td></tr>\n"
                      "<tr><td>calloc failures</td><td>%lu</td></tr>\n"
                      "<tr><td>realloc calls</td><td>%lu</td></tr>\n"
                      "<tr><td>realloc failures</td><td>%lu</td></tr>\n"
                      "<tr><td>free calls</td><td>%lu</td></tr>\n"
                    "<tr><td>free(null) calls</td><td>%lu</td></tr>\n"
                    "</table>",
                    (unsigned long) event.u.stats.tmstats.calltree_maxstack,
                    (unsigned long) event.u.stats.tmstats.calltree_maxdepth,
                    (unsigned long) event.u.stats.tmstats.calltree_parents,
                    (unsigned long) event.u.stats.tmstats.calltree_maxkids,
                    (unsigned long) event.u.stats.tmstats.calltree_kidhits,
                    (unsigned long) event.u.stats.tmstats.calltree_kidmisses,
                    (unsigned long) event.u.stats.tmstats.calltree_kidsteps,
                    (unsigned long) event.u.stats.tmstats.callsite_recurrences,
                    (unsigned long) event.u.stats.tmstats.backtrace_calls,
                    (unsigned long) event.u.stats.tmstats.backtrace_failures,
                    (unsigned long) event.u.stats.tmstats.btmalloc_failures,
                    (unsigned long) event.u.stats.tmstats.dladdr_failures,
                    (unsigned long) event.u.stats.tmstats.malloc_calls,
                    (unsigned long) event.u.stats.tmstats.malloc_failures,
                    (unsigned long) event.u.stats.tmstats.calloc_calls,
                    (unsigned long) event.u.stats.tmstats.calloc_failures,
                    (unsigned long) event.u.stats.tmstats.realloc_calls,
                    (unsigned long) event.u.stats.tmstats.realloc_failures,
                    (unsigned long) event.u.stats.tmstats.free_calls,
                    (unsigned long) event.u.stats.tmstats.null_free_calls);

            if (event.u.stats.calltree_maxkids_parent) {
                const void *key;
                PLHashNumber hash;
                callsite *site;

                key = (const void*) event.u.stats.calltree_maxkids_parent;
                hash = hash_serial(key);
                site = (callsite*) *PL_HashTableRawLookup(callsites, hash, key);
                if (site && site->method) {
                    fprintf(stdout, "<p>callsite with the most kids: %s</p>",
                            graphnode_name(site->method));
                }
            }

            if (event.u.stats.calltree_maxstack_top) {
                const void *key;
                PLHashNumber hash;
                callsite *site;

                key = (const void*) event.u.stats.calltree_maxstack_top;
                hash = hash_serial(key);
                site = (callsite*) *PL_HashTableRawLookup(callsites, hash, key);
                fputs("<p>deepest callsite tree path:\n"
                      "<table border=1>\n"
                        "<tr><th>Method</th><th>Offset</th></tr>\n",
                      stdout);
                while (site) {
                    fprintf(stdout,
                        "<tr><td>%s</td><td>0x%08lX</td></tr>\n",
                            site->method ? graphnode_name(site->method) : "???",
                            (unsigned long) site->offset);
                    site = site->parent;
                }
                fputs("</table>\n<hr>\n", stdout);
            }
            break;
        }
    }
}

int main(int argc, char **argv)
{
    time_t start;
    int c, i;
    FILE *fp;

    program = *argv;
    start = time(NULL);
    fprintf(stdout,
            "<script language=\"JavaScript\">\n"
            "function onload() {\n"
            "  document.links[0].__proto__.onmouseover = new Function("
                "\"window.status ="
                " this.href.substring(this.href.lastIndexOf('#') + 1)\");\n"
            "}\n"
            "</script>\n");
    fprintf(stdout, "%s starting at %s", program, ctime(&start));
    fflush(stdout);

    libraries = PL_NewHashTable(100, hash_serial, PL_CompareValues,
                                PL_CompareStrings, &graphnode_hashallocops,
                                NULL);
    components = PL_NewHashTable(10000, PL_HashString, PL_CompareStrings,
                                 PL_CompareValues, &component_hashallocops,
                                 NULL);
    methods = PL_NewHashTable(10000, hash_serial, PL_CompareValues,
                              PL_CompareStrings, &graphnode_hashallocops,
                              NULL);
    callsites = PL_NewHashTable(200000, hash_serial, PL_CompareValues,
                                PL_CompareValues, &callsite_hashallocops,
                                NULL);
    calltree_root.entry.value = (void*) strdup("root");
    if (!libraries || !components || !methods || !callsites ||
        !calltree_root.entry.value) {
        perror(program);
        exit(1);
    }

    while ((c = getopt(argc, argv, "dtf:m:")) != EOF) {
        switch (c) {
          case 'd':
            sort_by_direct = 1;
            break;
          case 't':
            do_tree_dump = 1;
            break;
          case 'f':
            function_dump = optarg;
            break;
          case 'm':
            min_subtotal = atoi(optarg);
            break;
          default:
            fprintf(stderr,
        "usage: %s [-dt] [-f function-dump-filename] [-m min] [output.html]\n",
                    program);
            exit(2);
        }
    }

    argc -= optind;
    argv += optind;
    if (argc == 0) {
        process("standard input", stdin);
    } else {
        for (i = 0; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (!fp) {
                fprintf(stderr, "%s: can't open %s: %s\n",
                        program, argv[i], strerror(errno));
                exit(1);
            }
            process(argv[i], fp);
            fclose(fp);
        }
    }

    compute_callsite_totals(&calltree_root);
    walk_callsite_tree(&calltree_root, 0, 0, stdout);

    dump_graph(libraries, "Library", stdout);
    fputs("<hr>\n", stdout);
    dump_graph(components, "Class or Component", stdout);
    if (function_dump) {
        struct stat sb, fsb;

        fstat(fileno(stdout), &sb);
        if (stat(function_dump, &fsb) == 0 &&
            fsb.st_dev == sb.st_dev && fsb.st_ino == sb.st_ino) {
            fp = stdout;
            fputs("<hr>\n", fp);
        } else {
            fp = fopen(function_dump, "w");
            if (!fp) {
                fprintf(stderr, "%s: can't open %s: %s\n",
                        program, function_dump, strerror(errno));
                exit(1);
            }
        }
        dump_graph(methods, "Function or Method", fp);
        if (fp != stdout)
            fclose(fp);
    }

    exit(0);
}
