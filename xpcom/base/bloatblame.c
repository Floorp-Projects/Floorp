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
#include <time.h>
#include <unistd.h>
#include "prtypes.h"
#include "prlog.h"
#include "prprf.h"
#include "plhash.h"
#include "nsTraceMalloc.h"

static char   *program;
static int    sort_by_direct = 0;
static int    do_tree_dump = 0;
static int32  min_subtotal = 0;

static int accum_byte(uint32 *uip)
{
    int c = getchar();
    if (c == EOF)
        return 0;
    *uip = (*uip << 8) | c;
    return 1;
}

static int get_uint32(uint32 *uip)
{
    int c;
    uint32 ui;

    c = getchar();
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
                    if (!accum_byte(&ui))
                        return 0;
                } else {
                    ui = (uint32) c;
                }
                if (!accum_byte(&ui))
                    return 0;
            } else {
                ui = (uint32) c;
            }
            if (!accum_byte(&ui))
                return 0;
        } else {
            ui = (uint32) c;
        }
        if (!accum_byte(&ui))
            return 0;
    } else {
        ui = (uint32) c;
    }
    *uip = ui;
    return 1;
}

static char *get_string(void)
{
    char *cp;
    int c;
    static char buf[256];
    static char *bp = buf, *ep = buf + sizeof buf;
    static size_t bsize = sizeof buf;

    cp = bp;
    do {
        c = getchar();
        if (c == EOF)
            return 0;
        if (cp == ep) {
            if (bp == buf) {
                bp = malloc(2 * bsize);
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
    } u;
} logevent;

static int get_logevent(logevent *event)
{
    int c;
    char *s;

    c = getchar();
    if (c == EOF)
        return 0;
    event->type = (char) c;
    if (!get_uint32(&event->serial))
        return 0;
    switch (c) {
      case 'L':
        s = get_string();
        if (!s)
            return 0;
        event->u.libname = s;
        break;

      case 'N':
        if (!get_uint32(&event->u.method.library))
            return 0;
        s = get_string();
        if (!s)
            return 0;
        event->u.method.name = s;
        break;

      case 'S':
        if (!get_uint32(&event->u.site.parent))
            return 0;
        if (!get_uint32(&event->u.site.method))
            return 0;
        if (!get_uint32(&event->u.site.offset))
            return 0;
        break;

      case 'M':
      case 'C':
      case 'F':
        event->u.alloc.oldsize = 0;
        if (!get_uint32(&event->u.alloc.size))
            return 0;
        break;

      case 'R':
        if (!get_uint32(&event->u.alloc.oldsize))
            return 0;
        if (!get_uint32(&event->u.alloc.size))
            return 0;
        break;
    }
    return 1;
}

typedef struct graphedge    graphedge;
typedef struct graphnode    graphnode;
typedef struct callsite     callsite;

struct graphnode {
    PLHashEntry entry;          /* key is serial or name, value must be name */
    graphedge   *in;
    graphedge   *out;
    graphnode   *up;
    int32       direct;         /* bytes allocated by this node's code */
    int32       total;          /* direct + bytes from all descendents */
    int32       visited;        /* counter used as flag when computing totals */
};

#define graphnode_name(node)    ((char*) (node)->entry.value)

#define library_serial(lib)     ((uint32) (lib)->entry.key)
#define component_name(comp)    ((const char*) (comp)->entry.key)

struct graphedge {
    graphedge   *next;
    graphnode   *node;
    int32       direct;
    int32       total;
};

struct callsite {
    PLHashEntry entry;
    callsite    *parent;
    callsite    *siblings;
    callsite    *kids;
    graphnode   *method;
    uint32      offset;
    int32       direct;
    int32       total;
};

#define callsite_serial(site)    ((uint32) (site)->entry.key)

static void connect_nodes(graphnode *from, graphnode *to, callsite *site)
{
    graphedge *edge;

    for (edge = from->out; edge; edge = edge->next) {
        if (edge->node == to) {
            edge[0].direct += site->direct;
            edge[1].direct += site->direct;
            edge[0].total += site->total;
            edge[1].total += site->total;
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
    edge[0].direct = edge[1].direct = site->direct;
    edge[0].total = edge[1].total = site->total;
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
        node->direct = node->total = 0;
        node->visited = 0;
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

    site->total += site->direct;
    for (kid = site->kids; kid; kid = kid->siblings) {
        compute_callsite_totals(kid);
        site->total += kid->total;
    }
}

static void walk_callsite_tree(callsite *site, int level, int kidnum, FILE *fp)
{
    callsite *parent;
    graphnode *meth, *pmeth, *comp, *pcomp, *lib, *plib;
    callsite *kid;
    int nkids;

    parent = site->parent;
    meth = comp = lib = NULL;
    if (parent) {
        meth = site->method;
        if (meth) {
            pmeth = parent->method;
            if (pmeth && pmeth != meth) {
                if (!meth->visited)
                    meth->total += site->total;
                connect_nodes(pmeth, meth, site);

                comp = meth->up;
                if (comp) {
                    pcomp = pmeth->up;
                    if (pcomp && pcomp != comp) {
                        if (!comp->visited)
                            comp->total += site->total;
                        connect_nodes(pcomp, comp, site);

                        lib = comp->up;
                        if (lib) {
                            plib = pcomp->up;
                            if (plib && plib != lib) {
                                if (!lib->visited)
                                    lib->total += site->total;
                                connect_nodes(plib, lib, site);
                            }
                            lib->visited++;
                        }
                    }
                    comp->visited++;
                }
            }
            meth->visited++;
        }
    }

    if (do_tree_dump) {
        fprintf(fp, "%c%*s%3d %3d %s %lu %ld\n",
                site->kids ? '+' : '-', level, "", level, kidnum,
                meth ? graphnode_name(meth) : "???",
                (unsigned long)site->direct, (long)site->total);
    }
    nkids = 0;
    for (kid = site->kids; kid; kid = kid->siblings) {
        walk_callsite_tree(kid, level + 1, nkids, fp);
        nkids++;
    }

    if (meth) {
        meth->visited--;
        if (comp) {
            comp->visited--;
            if (lib)
                lib->visited--;
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
        key1 = node1->direct;
        key2 = node2->direct;
    } else {
        key1 = node1->total;
        key2 = node2->total;
    }
    return key2 - key1;
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
            if (curr->total < next->total) {
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
    char buf[32];

    fputs("<td valign=top>", fp);
    total = 0;
    for (edge = list; edge; edge = edge->next)
        total += edge->total;
    for (edge = list; edge; edge = edge->next) {
        fprintf(fp, "<a href='#%s'>%s&nbsp;(%%%1.2f)</a>\n",
                graphnode_name(edge->node),
                prettybig(edge->total, buf, sizeof buf),
                percent(edge->total, total));
    }
    fputs("</td>", fp);
}

static void dump_graph(PLHashTable *hashtbl, const char *title, FILE *fp)
{
    uint32 i, count;
    graphnode **table;
    char buf1[32], buf2[32];

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
              "<tr><th>%s</th>"
                "<th>Total/Direct (percents)</th>"
                "<th>Fan-in</th>"
                "<th>Fan-out</th>"
              "</tr>\n",
            title);

    for (i = 0; i < count; i++) {
        graphnode *node = table[i];

        /* Don't bother with truly puny nodes. */
        if (node->total < min_subtotal)
            break;

        fprintf(fp,
                "<tr>"
                  "<td valign=top><a name='%s'>%s</td>"
                  "<td valign=top>%s/%s (%%%1.2f/%%%1.2f)</td>",
                graphnode_name(node),
                graphnode_name(node),
                prettybig(node->total, buf1, sizeof buf1),
                prettybig(node->direct, buf2, sizeof buf2),
                percent(node->total, calltree_root.total),
                percent(node->direct, calltree_root.total));

        sort_graphedge_list(&node->in);
        dump_graphedge_list(node->in, fp);
        sort_graphedge_list(&node->out);
        dump_graphedge_list(node->out, fp);

        fputs("</tr>\n", fp);
    }

    fputs("</table>\n", fp);
    free((void*) table);
}

static const char magic[] = NS_TRACE_MALLOC_LOGFILE_MAGIC;

int main(int argc, char **argv)
{
    int c;
    FILE *fp;
    char buf[16];
    time_t start;
    logevent event;

    program = *argv;

    while ((c = getopt(argc, argv, "dtm:")) != EOF) {
        switch (c) {
          case 'd':
            sort_by_direct = 1;
            break;
          case 't':
            do_tree_dump = 1;
            break;
          case 'm':
            min_subtotal = atoi(optarg);
            break;
          default:
            fprintf(stderr, "usage: %s [-dt] [-m min] [output.html]\n",
                    program);
            return 2;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc == 0) {
        fp = stdout;
    } else {
        fp = fopen(*argv, "w");
        if (!fp) {
            fprintf(stderr, "%s: can't open %s: %s\n",
                    program, *argv, strerror(errno));
            return 1;
        }
    }

    if (read(0, buf, 16) != 16 || strncmp(buf, magic, 16) != 0) {
        fprintf(stderr, "%s: bad magic string %s at start of standard input.\n",
                program, buf);
        return 1;
    }

    start = time(NULL);
    fprintf(fp, "%s starting at %s", program, ctime(&start));
    fflush(fp);

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
        return 1;
    }

    while (get_logevent(&event)) {
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
            if (he) return 2;

            he = PL_HashTableRawAdd(libraries, hep, hash, key, event.u.libname);
            if (!he) {
                perror(program);
                return 1;
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
            if (he) return 2;

            name = event.u.method.name;
            he = PL_HashTableRawAdd(methods, hep, hash, key, name);
            if (!he) {
                perror(program);
                return 1;
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
            if (he) return 2;

            if (event.u.site.parent == 0) {
                parent = &calltree_root;
            } else {
                pkey = (const void*) event.u.site.parent;
                phash = hash_serial(pkey);
                parent = (callsite*)
                         *PL_HashTableRawLookup(callsites, phash, pkey);
                if (!parent) {
                    fprintf(fp, "### no parent for %lu (%lu)!\n",
                            (unsigned long) event.serial,
                            (unsigned long) event.u.site.parent);
                    continue;
                }
            }

            he = PL_HashTableRawAdd(callsites, hep, hash, key, NULL);
            if (!he) {
                perror(program);
                return 1;
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
            site->direct = site->total = 0;
            break;
          }

          case 'M':
          case 'C':
          case 'R': {
            const void *key;
            PLHashNumber hash;
            callsite *site;
            int32 delta;
            graphnode *meth, *comp, *lib;

            key = (const void*) event.serial;
            hash = hash_serial(key);
            site = (callsite*) *PL_HashTableRawLookup(callsites, hash, key);
            if (!site) {
                fprintf(fp, "### no callsite for '%c' (%lu)!\n",
                        event.type, (unsigned long) event.serial);
                continue;
            }

            delta = (int32)event.u.alloc.size - (int32)event.u.alloc.oldsize;
            site->direct += delta;
            meth = site->method;
            if (meth) {
                meth->direct += delta;
                comp = meth->up;
                if (comp) {
                    comp->direct += delta;
                    lib = comp->up;
                    if (lib)
                        lib->direct += delta;
                }
            }
            break;
          }

          case 'F':
            break;
        }
    }

    compute_callsite_totals(&calltree_root);
    walk_callsite_tree(&calltree_root, 0, 0, fp);

    dump_graph(libraries, "Library", fp);
    fputs("<hr>\n", fp);
    dump_graph(components, "Component", fp);
#if 0
    fputs("<hr>\n", fp);
    dump_graph(methods, "Method", fp);
#endif

    fclose(fp);
    return 0;
}
