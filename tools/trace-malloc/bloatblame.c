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
static uint32 min_subtotal = 0;

static void compute_callsite_totals(tmcallsite *site)
{
    tmcallsite *kid;

    site->allocs.bytes.total += site->allocs.bytes.direct;
    site->allocs.calls.total += site->allocs.calls.direct;
    for (kid = site->kids; kid; kid = kid->siblings) {
        compute_callsite_totals(kid);
        site->allocs.bytes.total += kid->allocs.bytes.total;
        site->allocs.calls.total += kid->allocs.calls.total;
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
                    meth->allocs.bytes.total += site->allocs.bytes.total;
                    meth->allocs.calls.total += site->allocs.calls.total;
                }
                if (!tmgraphnode_connect(pmeth, meth, site))
                    goto bad;

                comp = meth->up;
                if (comp) {
                    pcomp = pmeth->up;
                    if (pcomp && pcomp != comp) {
                        if (!comp->low) {
                            comp->allocs.bytes.total
                                += site->allocs.bytes.total;
                            comp->allocs.calls.total
                                += site->allocs.calls.total;
                        }
                        if (!tmgraphnode_connect(pcomp, comp, site))
                            goto bad;

                        lib = comp->up;
                        if (lib) {
                            plib = pcomp->up;
                            if (plib && plib != lib) {
                                if (!lib->low) {
                                    lib->allocs.bytes.total
                                        += site->allocs.bytes.total;
                                    lib->allocs.calls.total
                                        += site->allocs.calls.total;
                                }
                                if (!tmgraphnode_connect(plib, lib, site))
                                    goto bad;
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
                (unsigned long)site->allocs.bytes.direct,
                (long)site->allocs.bytes.total);
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
    return;

bad:
    perror(program);
    exit(1);
}

/*
 * Linked list bubble-sort (waterson and brendan went bald hacking this).
 *
 * Sort the list in non-increasing order, using the expression passed as the
 * 'lessthan' formal macro parameter.  This expression should use 'curr' as
 * the pointer to the current node (of type nodetype) and 'next' as the next
 * node pointer.  It should return true if curr is less than next, and false
 * otherwise.
 */
#define BUBBLE_SORT_LINKED_LIST(listp, nodetype, lessthan)                    \
    PR_BEGIN_MACRO                                                            \
        nodetype *curr, **currp, *next, **nextp, *tmp;                        \
                                                                              \
        currp = listp;                                                        \
        while ((curr = *currp) != NULL && curr->next) {                       \
            nextp = &curr->next;                                              \
            while ((next = *nextp) != NULL) {                                 \
                if (lessthan) {                                               \
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
    BUBBLE_SORT_LINKED_LIST(&node->down, tmgraphnode,
        (curr->allocs.bytes.total < next->allocs.bytes.total));
    return HT_ENUMERATE_NEXT;
}

/* Sort in reverse size order, so biggest node comes first. */
static int node_table_compare(const void *p1, const void *p2)
{
    const tmgraphnode *node1, *node2;
    uint32 key1, key2;

    node1 = *(const tmgraphnode**) p1;
    node2 = *(const tmgraphnode**) p2;
    if (sort_by_direct) {
        key1 = node1->allocs.bytes.direct;
        key2 = node2->allocs.bytes.direct;
    } else {
        key1 = node1->allocs.bytes.total;
        key2 = node2->allocs.bytes.total;
    }
    return (key2 < key1) ? -1 : (key2 > key1) ? 1 : 0;
}

static int mean_size_compare(const void *p1, const void *p2)
{
    const tmgraphnode *node1, *node2;
    double div1, div2, key1, key2;

    node1 = *(const tmgraphnode**) p1;
    node2 = *(const tmgraphnode**) p2;
    div1 = (double)node1->allocs.calls.direct;
    div2 = (double)node2->allocs.calls.direct;
    if (div1 == 0 || div2 == 0)
        return div2 - div1;
    key1 = (double)node1->allocs.bytes.direct / div1;
    key2 = (double)node2->allocs.bytes.direct / div2;
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

static double percent(uint32 num, uint32 total)
{
    if (num == 0)
        return 0.0;
    return ((double) num * 100) / (double) total;
}

static void sort_graphlink_list(tmgraphlink **listp, int which)
{
    BUBBLE_SORT_LINKED_LIST(listp, tmgraphlink,
        (TM_LINK_TO_EDGE(curr, which)->allocs.bytes.total
         < TM_LINK_TO_EDGE(next, which)->allocs.bytes.total));
}

static void dump_graphlink_list(tmgraphlink *list, int which, const char *name,
                                FILE *fp)
{
    tmcounts bytes;
    tmgraphlink *link;
    tmgraphedge *edge;
    char buf[16];

    bytes.direct = bytes.total = 0;
    for (link = list; link; link = link->next) {
        edge = TM_LINK_TO_EDGE(link, which);
        bytes.direct += edge->allocs.bytes.direct;
        bytes.total += edge->allocs.bytes.total;
    }

    if (js_mode) {
        fprintf(fp,
                "   %s:{dbytes:%ld, tbytes:%ld, edges:[\n",
                name, (long) bytes.direct, (long) bytes.total);
        for (link = list; link; link = link->next) {
            edge = TM_LINK_TO_EDGE(link, which);
            fprintf(fp,
                    "    {node:%d, dbytes:%ld, tbytes:%ld},\n",
                    link->node->sort,
                    (long) edge->allocs.bytes.direct,
                    (long) edge->allocs.bytes.total);
        }
        fputs("   ]},\n", fp);
    } else {
        fputs("<td valign=top>", fp);
        for (link = list; link; link = link->next) {
            edge = TM_LINK_TO_EDGE(link, which);
            fprintf(fp,
                    "<a href='#%s'>%s&nbsp;(%1.2f%%)</a>\n",
                    tmgraphnode_name(link->node),
                    prettybig(edge->allocs.bytes.total, buf, sizeof buf),
                    percent(edge->allocs.bytes.total, bytes.total));
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
        if (node->allocs.bytes.total < min_subtotal)
            break;

        name = tmgraphnode_name(node);
        if (js_mode) {
            fprintf(fp,
                    "  {name:'%s', dbytes:%ld, tbytes:%ld,"
                                 " dallocs:%ld, tallocs:%ld,\n",
                    name,
                    (long) node->allocs.bytes.direct,
                    (long) node->allocs.bytes.total,
                    (long) node->allocs.calls.direct,
                    (long) node->allocs.calls.total);
        } else {
            namelen = strlen(name);
            fprintf(fp,
                    "<tr>"
                      "<td valign=top><a name='%s'>%.*s%s</a></td>",
                    name,
                    (namelen > 40) ? 40 : (int)namelen, name,
                    (namelen > 40) ? "<i>...</i>" : "");
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
                    prettybig(node->allocs.bytes.total, buf1, sizeof buf1),
                    prettybig(node->allocs.bytes.direct, buf2, sizeof buf2),
                    percent(node->allocs.bytes.total,
                            tmr->calltree_root.allocs.bytes.total),
                    percent(node->allocs.bytes.direct,
                            tmr->calltree_root.allocs.bytes.total),
                    prettybig(node->allocs.calls.total, buf3, sizeof buf3),
                    prettybig(node->allocs.calls.direct, buf4, sizeof buf4),
                    percent(node->allocs.calls.total,
                            tmr->calltree_root.allocs.calls.total),
                    percent(node->allocs.calls.direct,
                            tmr->calltree_root.allocs.calls.total));
        }

        /* NB: we must use 'fin' because 'in' is a JS keyword! */
        sort_graphlink_list(&node->in, TM_EDGE_IN_LINK);
        dump_graphlink_list(node->in, TM_EDGE_IN_LINK, "fin", fp);
        sort_graphlink_list(&node->out, TM_EDGE_OUT_LINK);
        dump_graphlink_list(node->out, TM_EDGE_OUT_LINK, "out", fp);

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
            allocs = (double)node->allocs.calls.direct;
            if (!allocs)
                continue;

            /* Compute direct-size mean and standard deviation. */
            bytes = (double)node->allocs.bytes.direct;
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
                    prettybig(node->allocs.calls.direct, buf3, sizeof buf3));
        }
        fputs("</table>\n", fp);
    }

    free((void*) table);
}

static void my_tmevent_handler(tmreader *tmr, tmevent *event)
{
    switch (event->type) {
      case TM_EVENT_STATS:
        if (js_mode)
            break;
        fprintf(stdout,
                "<p><table border=1>"
                  "<tr><th>Counter</th><th>Value</th></tr>\n"
                  "<tr><td>maximum actual stack depth</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>maximum callsite tree depth</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>number of parent callsites</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>maximum kids per parent</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>hits looking for a kid</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>misses looking for a kid</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>steps over other kids</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>callsite recurrences</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>number of stack backtraces</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>backtrace failures</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>backtrace malloc failures</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>backtrace dladdr failures</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>malloc calls</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>malloc failures</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>calloc calls</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>calloc failures</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>realloc calls</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>realloc failures</td><td align=right>%lu</td></tr>\n"
                  "<tr><td>free calls</td><td align=right>%lu</td></tr>\n"
                "<tr><td>free(null) calls</td><td align=right>%lu</td></tr>\n"
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
                tmreader_callsite(tmr, event->u.stats.calltree_maxkids_parent);
            if (site && site->method) {
                fprintf(stdout, "<p>callsite with the most kids: %s</p>",
                        tmgraphnode_name(site->method));
            }
        }

        if (event->u.stats.calltree_maxstack_top) {
            tmcallsite *site =
                tmreader_callsite(tmr, event->u.stats.calltree_maxstack_top);
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
    int c, i, j, rv;
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
        if (tmreader_eventloop(tmr, "-", my_tmevent_handler) <= 0)
            exit(1);
    } else {
        for (i = j = 0; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (!fp) {
                fprintf(stderr, "%s: can't open %s: %s\n",
                        program, argv[i], strerror(errno));
                exit(1);
            }
            rv = tmreader_eventloop(tmr, argv[i], my_tmevent_handler);
            if (rv < 0)
                exit(1);
            if (rv > 0)
                j++;
            fclose(fp);
        }
        if (j == 0)
            exit(1);
    }

    compute_callsite_totals(&tmr->calltree_root);
    walk_callsite_tree(&tmr->calltree_root, 0, 0, stdout);

    if (js_mode) {
        fprintf(stdout,
                "<script language='javascript'>\n"
                "// direct and total byte and allocator-call counts\n"
                "var dbytes = %ld, tbytes = %ld,"
                   " dallocs = %ld, tallocs = %ld;\n",
                (long) tmr->calltree_root.allocs.bytes.direct,
                (long) tmr->calltree_root.allocs.bytes.total,
                (long) tmr->calltree_root.allocs.calls.direct,
                (long) tmr->calltree_root.allocs.calls.total);
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
