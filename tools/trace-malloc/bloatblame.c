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
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int  getopt(int argc, char *const *argv, const char *shortopts);
extern char *optarg;
extern int  optind;
#endif
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "prtypes.h"
#include "prlog.h"
#include "prprf.h"
#include "plhash.h"
#include "nsTraceMalloc.h"
#include "tmreader.h"

static char   *program;
static int    sort_by_direct = 0;
static int    js_mode = 0;
static int    do_tree_dump = 0;
static int    unified_output = 0;
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
            nsTMStats tmstats;
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

static void connect_nodes(tmgraphnode *from, tmgraphnode *to, tmcallsite *site)
{
    tmgraphedge *edge;

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
    edge = (tmgraphedge*) malloc(2 * sizeof(tmgraphedge));
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

static void compute_callsite_totals(tmcallsite *site)
{
    tmcallsite *kid;

    site->bytes.total += site->bytes.direct;
    site->allocs.total += site->allocs.direct;
    for (kid = site->kids; kid; kid = kid->siblings) {
        compute_callsite_totals(kid);
        site->bytes.total += kid->bytes.total;
        site->allocs.total += kid->allocs.total;
    }
}

static void walk_callsite_tree(tmcallsite *site, int level, int kidnum, FILE *fp)
{
    tmcallsite *parent;
    tmgraphnode *meth, *pmeth, *comp, *pcomp, *lib, *plib;
    int old_meth_low, old_comp_low, old_lib_low, nkids;
    tmcallsite *kid;

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
                meth ? tmgraphnode_name(meth) : "???",
                (unsigned long)site->bytes.direct, (long)site->bytes.total);
    }
    nkids = 0;
    level++;
    for (kid = site->kids; kid; kid = kid->siblings) {
        walk_callsite_tree(kid, level, nkids, fp);
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

/* Linked list bubble-sort (waterson and brendan went bald hacking this). */
#define BUBBLE_SORT_LINKED_LIST(listp, nodetype)                              \
    PR_BEGIN_MACRO                                                            \
        nodetype *curr, **currp, *next, **nextp, *tmp;                        \
                                                                              \
        currp = listp;                                                        \
        while ((curr = *currp) != NULL && curr->next) {                       \
            nextp = &curr->next;                                              \
            while ((next = *nextp) != NULL) {                                 \
                if (curr->bytes.total < next->bytes.total) {                  \
                    tmp = curr->next;                                         \
                    *currp = tmp;                                             \
                    if (tmp == next) {                                        \
                        PR_ASSERT(nextp == &curr->next);                      \
                        curr->next = next->next;                              \
                        next->next = curr;                                    \
                    } else {                                                  \
                        *nextp = next->next;                                  \
                        curr->next = next->next;                              \
                        next->next = tmp;                                     \
                        *currp = next;                                        \
                        *nextp = curr;                                        \
                        nextp = &curr->next;                                  \
                    }                                                         \
                    curr = next;                                              \
                    continue;                                                 \
                }                                                             \
                nextp = &next->next;                                          \
            }                                                                 \
            currp = &curr->next;                                              \
        }                                                                     \
    PR_END_MACRO

static PRIntn tabulate_node(PLHashEntry *he, PRIntn i, void *arg)
{
    tmgraphnode *node = (tmgraphnode*) he;
    tmgraphnode **table = (tmgraphnode**) arg;

    table[i] = node;
    BUBBLE_SORT_LINKED_LIST(&node->down, tmgraphnode);
    return HT_ENUMERATE_NEXT;
}

/* Sort in reverse size order, so biggest node comes first. */
static int node_table_compare(const void *p1, const void *p2)
{
    const tmgraphnode *node1, *node2;
    int32 key1, key2;

    node1 = *(const tmgraphnode**) p1;
    node2 = *(const tmgraphnode**) p2;
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
    const tmgraphnode *node1, *node2;
    double div1, div2, key1, key2;

    node1 = *(const tmgraphnode**) p1;
    node2 = *(const tmgraphnode**) p2;
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

static void sort_graphedge_list(tmgraphedge **listp)
{
    BUBBLE_SORT_LINKED_LIST(listp, tmgraphedge);
}

static void dump_graphedge_list(tmgraphedge *list, const char *name, FILE *fp)
{
    tmcounts bytes;
    tmgraphedge *edge;
    char buf[16];

    bytes.direct = bytes.total = 0;
    for (edge = list; edge; edge = edge->next) {
        bytes.direct += edge->bytes.direct;
        bytes.total += edge->bytes.total;
    }

    if (js_mode) {
        fprintf(fp,
                "   %s:{dbytes:%ld, tbytes:%ld, edges:[\n",
                name, (long) bytes.direct, (long) bytes.total);
        for (edge = list; edge; edge = edge->next) {
            fprintf(fp,
                    "    {node:%d, dbytes:%ld, tbytes:%ld},\n",
                    edge->node->sort,
                    (long) edge->bytes.direct,
                    (long) edge->bytes.total);
        }
        fputs("   ]},\n", fp);
    } else {
        fputs("<td valign=top>", fp);
        for (edge = list; edge; edge = edge->next) {
            fprintf(fp,
                    "<a href='#%s'>%s&nbsp;(%1.2f%%)</a>\n",
                    tmgraphnode_name(edge->node),
                    prettybig(edge->bytes.total, buf, sizeof buf),
                    percent(edge->bytes.total, bytes.total));
        }
        fputs("</td>", fp);
    }
}

static void dump_graph(tmreader *tmr, PLHashTable *hashtbl, const char *varname,
                       const char *title, FILE *fp)
{
    uint32 i, count;
    tmgraphnode **table, *node;
    char *name;
    size_t namelen;
    char buf1[16], buf2[16], buf3[16], buf4[16];
    static char NA[] = "N/A";

    count = hashtbl->nentries;
    table = (tmgraphnode**) malloc(count * sizeof(tmgraphnode*));
    if (!table) {
        perror(program);
        exit(1);
    }
    PL_HashTableEnumerateEntries(hashtbl, tabulate_node, table);
    qsort(table, count, sizeof(tmgraphnode*), node_table_compare);
    for (i = 0; i < count; i++)
        table[i]->sort = i;

    if (js_mode) {
        fprintf(fp,
                "var %s = {\n name:'%s', title:'%s', nodes:[\n",
                varname, varname, title);
    } else {
        fprintf(fp,
                "<table border=1>\n"
                  "<tr>"
                    "<th>%s</th>"
                    "<th>Down</th>"
                    "<th>Next</th>"
                    "<th>Total/Direct (percents)</th>"
                    "<th>Allocations</th>"
                    "<th>Fan-in</th>"
                    "<th>Fan-out</th>"
                  "</tr>\n",
                title);
    }

    for (i = 0; i < count; i++) {
        /* Don't bother with truly puny nodes. */
        node = table[i];
        if (node->bytes.total < min_subtotal)
            break;

        name = tmgraphnode_name(node);
        if (js_mode) {
            fprintf(fp,
                    "  {name:'%s', dbytes:%ld, tbytes:%ld,"
                                 " dallocs:%ld, tallocs:%ld,\n",
                    name,
                    (long) node->bytes.direct, (long) node->bytes.total,
                    (long) node->allocs.direct, (long) node->allocs.total);
        } else {
            namelen = strlen(name);
            fprintf(fp,
                    "<tr>"
                      "<td valign=top><a name='%s'>%.*s%s</a></td>",
                    name,
                    (namelen > 45) ? 45 : (int)namelen, name,
                    (namelen > 45) ? "<i>...</i>" : "");
            if (node->down) {
                fprintf(fp,
                      "<td valign=top><a href='#%s'><i>down</i></a></td>",
                        tmgraphnode_name(node->down));
            } else {
                fputs("<td></td>", fp);
            }
            if (node->next) {
                fprintf(fp,
                      "<td valign=top><a href='#%s'><i>next</i></a></td>",
                        tmgraphnode_name(node->next));
            } else {
                fputs("<td></td>", fp);
            }
            fprintf(fp,
                      "<td valign=top>%s/%s (%1.2f%%/%1.2f%%)</td>"
                      "<td valign=top>%s/%s (%1.2f%%/%1.2f%%)</td>",
                    prettybig(node->bytes.total, buf1, sizeof buf1),
                    prettybig(node->bytes.direct, buf2, sizeof buf2),
                    percent(node->bytes.total, tmr->calltree_root.bytes.total),
                    percent(node->bytes.direct, tmr->calltree_root.bytes.total),
                    prettybig(node->allocs.total, buf3, sizeof buf3),
                    prettybig(node->allocs.direct, buf4, sizeof buf4),
                    percent(node->allocs.total, tmr->calltree_root.allocs.total),
                    percent(node->allocs.direct, tmr->calltree_root.allocs.total));
        }

        sort_graphedge_list(&node->in);
        dump_graphedge_list(node->in, "fin", fp);   /* 'in' is a JS keyword! */
        sort_graphedge_list(&node->out);
        dump_graphedge_list(node->out, "out", fp);

        if (js_mode)
            fputs("  },\n", fp);
        else
            fputs("</tr>\n", fp);
    }

    if (js_mode) {
        fputs("]};\n", fp);
    } else {
        fputs("</table>\n<hr>\n", fp);

        qsort(table, count, sizeof(tmgraphnode*), mean_size_compare);

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

            name = tmgraphnode_name(node);
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
    }

    free((void*) table);
}

static void my_tmevent_handler(tmreader *tmr, tmevent *event)
{
    switch (event->type) {
      case 'Z':
        if (js_mode)
            break;
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
                (unsigned long) event->u.stats.tmstats.calltree_maxstack,
                (unsigned long) event->u.stats.tmstats.calltree_maxdepth,
                (unsigned long) event->u.stats.tmstats.calltree_parents,
                (unsigned long) event->u.stats.tmstats.calltree_maxkids,
                (unsigned long) event->u.stats.tmstats.calltree_kidhits,
                (unsigned long) event->u.stats.tmstats.calltree_kidmisses,
                (unsigned long) event->u.stats.tmstats.calltree_kidsteps,
                (unsigned long) event->u.stats.tmstats.callsite_recurrences,
                (unsigned long) event->u.stats.tmstats.backtrace_calls,
                (unsigned long) event->u.stats.tmstats.backtrace_failures,
                (unsigned long) event->u.stats.tmstats.btmalloc_failures,
                (unsigned long) event->u.stats.tmstats.dladdr_failures,
                (unsigned long) event->u.stats.tmstats.malloc_calls,
                (unsigned long) event->u.stats.tmstats.malloc_failures,
                (unsigned long) event->u.stats.tmstats.calloc_calls,
                (unsigned long) event->u.stats.tmstats.calloc_failures,
                (unsigned long) event->u.stats.tmstats.realloc_calls,
                (unsigned long) event->u.stats.tmstats.realloc_failures,
                (unsigned long) event->u.stats.tmstats.free_calls,
                (unsigned long) event->u.stats.tmstats.null_free_calls);

        if (event->u.stats.calltree_maxkids_parent) {
            tmcallsite *site =
                tmreader_get_callsite(tmr,
                                      event->u.stats.calltree_maxkids_parent);
            if (site && site->method) {
                fprintf(stdout, "<p>callsite with the most kids: %s</p>",
                        tmgraphnode_name(site->method));
            }
        }

        if (event->u.stats.calltree_maxstack_top) {
            tmcallsite *site =
                tmreader_get_callsite(tmr,
                                      event->u.stats.calltree_maxstack_top);
            fputs("<p>deepest callsite tree path:\n"
                  "<table border=1>\n"
                    "<tr><th>Method</th><th>Offset</th></tr>\n",
                  stdout);
            while (site) {
                fprintf(stdout,
                    "<tr><td>%s</td><td>0x%08lX</td></tr>\n",
                        site->method ? tmgraphnode_name(site->method) : "???",
                        (unsigned long) site->offset);
                site = site->parent;
            }
            fputs("</table>\n<hr>\n", stdout);
        }
        break;
    }
}

int main(int argc, char **argv)
{
    int c, i;
    tmreader *tmr;
    FILE *fp;

    program = *argv;
    tmr = tmreader_new(program, NULL);
    if (!tmr) {
        perror(program);
        exit(1);
    }

    while ((c = getopt(argc, argv, "djtuf:m:")) != EOF) {
        switch (c) {
          case 'd':
            sort_by_direct = 1;
            break;
          case 'j':
            js_mode = 1;
            break;
          case 't':
            do_tree_dump = 1;
            break;
          case 'u':
            unified_output = 1;
            break;
          case 'f':
            function_dump = optarg;
            break;
          case 'm':
            min_subtotal = atoi(optarg);
            break;
          default:
            fprintf(stderr,
        "usage: %s [-dtu] [-f function-dump-filename] [-m min] [output.html]\n",
                    program);
            exit(2);
        }
    }

    if (!js_mode) {
        time_t start = time(NULL);

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
    }

    argc -= optind;
    argv += optind;
    if (argc == 0) {
        tmreader_loop(tmr, "-", my_tmevent_handler);
    } else {
        for (i = 0; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (!fp) {
                fprintf(stderr, "%s: can't open %s: %s\n",
                        program, argv[i], strerror(errno));
                exit(1);
            }
            tmreader_loop(tmr, argv[i], my_tmevent_handler);
            fclose(fp);
        }
    }

    compute_callsite_totals(&tmr->calltree_root);
    walk_callsite_tree(&tmr->calltree_root, 0, 0, stdout);

    if (js_mode) {
        fprintf(stdout,
                "<script language='javascript'>\n"
                "// direct and total byte and allocator-call counts\n"
                "var dbytes = %ld, tbytes = %ld,"
                   " dallocs = %ld, tallocs = %ld;\n",
                (long) tmr->calltree_root.bytes.direct,
                (long) tmr->calltree_root.bytes.total,
                (long) tmr->calltree_root.allocs.direct,
                (long) tmr->calltree_root.allocs.total);
    }

    dump_graph(tmr, tmr->libraries, "libraries", "Library", stdout);
    if (!js_mode)
        fputs("<hr>\n", stdout);

    dump_graph(tmr, tmr->components, "classes", "Class or Component", stdout);
    if (js_mode || unified_output || function_dump) {
        if (js_mode || unified_output || strcmp(function_dump, "-") == 0) {
            fp = stdout;
            if (!js_mode)
                fputs("<hr>\n", fp);
        } else {
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
        }

        dump_graph(tmr, tmr->methods, "methods", "Function or Method", fp);
        if (fp != stdout)
            fclose(fp);

        if (js_mode) {
            fputs("function viewnode(graph, index) {\n"
                  "  view.location = viewsrc();\n"
                  "}\n"
                  "function viewnodelink(graph, index) {\n"
                  "  var node = graph.nodes[index];\n"
                  "  return '<a href=\"javascript:viewnode('"
                      " + graph.name.quote() + ', ' + node.sort"
                      " + ')\" onmouseover=' + node.name.quote() + '>'"
                      " + node.name + '</a>';\n"
                  "}\n"
                  "function search(expr) {\n"
                  "  var re = new RegExp(expr);\n"
                  "  var src = '';\n"
                  "  var graphs = [libraries, classes, methods]\n"
                  "  var nodes;\n"
                  "  for (var n = 0; n < (nodes = graphs[n].nodes).length; n++) {\n"
                  "    for (var i = 0; i < nodes.length; i++) {\n"
                  "      if (re.test(nodes[i].name))\n"
                  "        src += viewnodelink(graph, i) + '\\n';\n"
                  "    }\n"
                  "  }\n"
                  "  view.location = viewsrc();\n"
                  "}\n"
                  "function ctrlsrc() {\n"
                  "  return \"<form>\\n"
                      "search: <input size=40 onchange='search(this.value)'>\\n"
                      "</form>\\n\";\n"
                  "}\n"
                  "function viewsrc() {\n"
                  "  return 'hiiiii'\n"
                  "}\n"
                  "</script>\n"
                  "<frameset rows='10%,*'>\n"
                  " <frame name='ctrl' src='javascript:top.ctrlsrc()'>\n"
                  " <frame name='view' src='javascript:top.viewsrc()'>\n"
                  "</frameset>\n",
                  stdout);
        }
    }

    exit(0);
}
