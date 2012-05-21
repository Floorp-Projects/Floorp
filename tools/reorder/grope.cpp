/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A program that computes a function ordering for an executable based
  on runtime profile information.

  This program is directly based on work done by Roger Chickering
  <rogc@netscape.com> in

    <http://bugzilla.mozilla.org/show_bug.cgi?id=65845>

  to implement Nat Friedman's <nat@nat.org> `grope',

    _GNU Rope - A Subroutine Position Optimizer_
    <http://www.hungry.com/~shaver/grope/grope.ps>

  Specifically, it implements the procedure re-ordering algorithm
  described in:

    K. Pettis and R. Hansen. ``Profile-Guided Core Position.'' In
    _Proceedings of the Int. Conference on Programming Language Design
    and Implementation_, pages 16-27, June 1990.

 */

#include <assert.h>
#include <fstream>
#include <hash_set>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "elf_symbol_table.h"

#define _GNU_SOURCE
#include <getopt.h>

elf_symbol_table symtab;

// Straight outta plhash.c!
#define GOLDEN_RATIO 0x9E3779B9U

struct hash<const Elf32_Sym *>
{
    size_t operator()(const Elf32_Sym *sym) const {
        return (reinterpret_cast<size_t>(sym) >> 4) * GOLDEN_RATIO; }
};

typedef unsigned int call_count_t;

struct call_graph_arc
{
    const Elf32_Sym *m_from;
    const Elf32_Sym *m_to;
    call_count_t     m_count;

    call_graph_arc(const Elf32_Sym *left, const Elf32_Sym *right, call_count_t count = 0)
        : m_count(count)
    {
        assert(left != right);

        if (left > right) {
            m_from = left;
            m_to   = right;
        }
        else {
            m_from = right;
            m_to   = left;
        }
    }

    friend bool
    operator==(const call_graph_arc &lhs, const call_graph_arc &rhs)
    {
        return (lhs.m_from == rhs.m_from) && (lhs.m_to == rhs.m_to);
    }

    friend ostream &
    operator<<(ostream &out, const call_graph_arc &arc)
    {
        out << &arc << ": ";
        out.form("<(%s %s) %d>",
                 symtab.get_symbol_name(arc.m_from),
                 symtab.get_symbol_name(arc.m_to),
                 arc.m_count);

        return out;
    }
};

struct hash<call_graph_arc *>
{
    size_t
    operator()(const call_graph_arc* arc) const
    {
        size_t result;
        result = reinterpret_cast<unsigned long>(arc->m_from);
        result ^= reinterpret_cast<unsigned long>(arc->m_to) >> 16;
        result ^= reinterpret_cast<unsigned long>(arc->m_to) << 16;
        result *= GOLDEN_RATIO;
        return result;
    }
};

struct equal_to<call_graph_arc *>
{
    bool
    operator()(const call_graph_arc* lhs, const call_graph_arc *rhs) const
    {
        return *lhs == *rhs;
    }
};

typedef hash_set<call_graph_arc *> arc_container_t;
arc_container_t arcs;

typedef multimap<const Elf32_Sym *, call_graph_arc *> arc_map_t;
arc_map_t from_map;
arc_map_t to_map;

struct less_call_graph_arc_count
{
    bool
    operator()(const call_graph_arc *lhs, const call_graph_arc *rhs) const
    {
        if (lhs->m_count == rhs->m_count) {
            if (lhs->m_from == rhs->m_from)
                return lhs->m_to > rhs->m_to;

            return lhs->m_from > rhs->m_from;
        }
        return lhs->m_count > rhs->m_count;
    }
};

typedef set<call_graph_arc *, less_call_graph_arc_count> arc_count_index_t;

bool opt_debug = false;
const char *opt_out = "order.out";
int opt_tick = 0;
bool opt_verbose = false;
int opt_window = 16;

static struct option long_options[] = {
    { "debug",       no_argument,       0, 'd' },
    { "exe",         required_argument, 0, 'e' },
    { "help",        no_argument,       0, '?' },
    { "out",         required_argument, 0, 'o' },
    { "tick",        optional_argument, 0, 't' },
    { "verbose",     no_argument,       0, 'v' },
    { "window",      required_argument, 0, 'w' },
    { 0,             0,                 0, 0   }
};

//----------------------------------------------------------------------

static void
usage(const char *name)
{
    cerr << "usage: " << name << " [options] [<file> ...]" << endl;
    cerr << "  Options:" << endl;
    cerr << "  --debug, -d" << endl;
    cerr << "      Print lots of verbose debugging cruft." << endl;
    cerr << "  --exe=<image>, -e <image> (required)" << endl;
    cerr << "      Specify the executable image from which to read symbol information." << endl;
    cerr << "  --help, -?" << endl;
    cerr << "      Print this message and exit." << endl;
    cerr << "  --out=<file>, -o <file>" << endl;
    cerr << "      Specify the output file to which to dump the symbol ordering of the" << endl;
    cerr << "      best individual (default is `order.out')." << endl;
    cerr << "  --tick[=<num>], -t [<num>]" << endl;
    cerr << "      When reading address data, print a dot to stderr every <num>th" << endl;
    cerr << "      address processed from the call trace. If specified with no argument," << endl;
    cerr << "      a dot will be printed for every million addresses processed." << endl;
    cerr << "  --verbose, -v" << endl;
    cerr << "      Issue progress messages to stderr." << endl;
    cerr << "  --window=<num>, -w <num>" << endl;
    cerr << "      Use a sliding window instead of pagination to score orderings." << endl;
}

/**
 * Using the symbol table, map a stream of address references into a
 * callgraph.
 */
static void
map_addrs(int fd)
{
    long long total_calls = 0;
    typedef list<const Elf32_Sym *> window_t;

    window_t window;
    int window_size = 0;
    unsigned int buf[128];
    ssize_t cb;

    while ((cb = read(fd, buf, sizeof buf)) > 0) {
        if (cb % sizeof buf[0])
            fprintf(stderr, "unaligned read\n");

        unsigned int *addr = buf;
        unsigned int *limit = buf + (cb / 4);

        for (; addr < limit; ++addr) {
            const Elf32_Sym *sym = symtab.lookup(*addr);
            if (! sym)
                continue;

            ++total_calls;

            window.push_front(sym);

            if (window_size >= opt_window)
                window.pop_back();
            else
                ++window_size;

            window_t::const_iterator i = window.begin();
            window_t::const_iterator end = window.end();
            for (; i != end; ++i) {
                if (sym != *i) {
                    call_graph_arc *arc;
                    call_graph_arc key(sym, *i);
                    arc_container_t::iterator iter = arcs.find(&key);
                    if (iter == arcs.end()) {
                        arc = new call_graph_arc(sym, *i);
                        arcs.insert(arc);
                        from_map.insert(arc_map_t::value_type(arc->m_from, arc));
                        to_map.insert(arc_map_t::value_type(arc->m_to, arc));
                    }
                    else
                        arc = const_cast<call_graph_arc *>(*iter);

                    ++(arc->m_count);
                }
            }

            if (opt_verbose && opt_tick && (total_calls % opt_tick == 0)) {
                cerr << ".";
                flush(cerr);
            }
        }
    }

    if (opt_verbose) {
        if (opt_tick)
            cerr << endl;

        cerr << "Total calls: " << total_calls << endl;
    }
}

static void
remove_from(arc_map_t& map, const Elf32_Sym *sym, const call_graph_arc *arc)
{
    pair<arc_map_t::iterator, arc_map_t::iterator> p
        = map.equal_range(sym);

    for (arc_map_t::iterator i = p.first; i != p.second; ++i) {
        if (i->second == arc) {
            map.erase(i);
            break;
        }
    }
}

/**
 * The main program
 */
int
main(int argc, char *argv[])
{
    const char *opt_exe = 0;

    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "?de:o:t:vw:", long_options, &option_index);

        if (c < 0)
            break;

        switch (c) {
        case '?':
            usage(argv[0]);
            return 0;

        case 'd':
            opt_debug = true;
            break;

        case 'e':
            opt_exe = optarg;
            break;

        case 'o':
            opt_out = optarg;
            break;

        case 't':
            opt_tick = optarg ? atoi(optarg) : 1000000;
            break;

        case 'v':
            opt_verbose = true;
            break;

        case 'w':
            opt_window = atoi(optarg);
            if (opt_window <= 0) {
                cerr << "invalid window size: " << opt_window << endl;
                return 1;
            }
            break;

        default:
            usage(argv[0]);
            return 1;
        }
    }

    // Make sure an image was specified
    if (! opt_exe) {
        usage(argv[0]);
        return 1;
    }

    // Read the sym table.
    symtab.init(opt_exe);

    // Process addresses to construct the weighted call graph.
    if (optind >= argc) {
        map_addrs(STDIN_FILENO);
    }
    else {
        do {
            int fd = open(argv[optind], O_RDONLY);
            if (fd < 0) {
                perror(argv[optind]);
                return 1;
            }

            map_addrs(fd);
            close(fd);
        } while (++optind < argc);
    }

    // Emit the ordering.
    ofstream out(opt_out);

    // Collect all of the arcs into a sorted container, with arcs
    // having the largest weight at the front.
    arc_count_index_t sorted_arcs(arcs.begin(), arcs.end());

    while (sorted_arcs.size()) {
        if (opt_debug) {
            cerr << "==========================================" << endl << endl;

            cerr << "Sorted Arcs:" << endl;
            for (arc_count_index_t::const_iterator iter = sorted_arcs.begin();
                 iter != sorted_arcs.end();
                 ++iter) {
                cerr << **iter << endl;
            }

            cerr << endl << "Arc Container:" << endl;
            for (arc_container_t::const_iterator iter = arcs.begin();
                 iter != arcs.end();
                 ++iter) {
                cerr << **iter << endl;
            }

            cerr << endl << "From Map:" << endl;
            for (arc_map_t::const_iterator iter = from_map.begin();
                 iter != from_map.end();
                 ++iter) {
                cerr << symtab.get_symbol_name(iter->first) << ": " << *(iter->second) << endl;
            }

            cerr << endl << "To Map:" << endl;
            for (arc_map_t::const_iterator iter = to_map.begin();
                 iter != to_map.end();
                 ++iter) {
                cerr << symtab.get_symbol_name(iter->first) << ": " << *(iter->second) << endl;
            }

            cerr << endl;
        }

        // The first arc in the sorted set will have the largest
        // weight. Pull it out, and emit its sink.
        arc_count_index_t::iterator max = sorted_arcs.begin();
        call_graph_arc *arc = const_cast<call_graph_arc *>(*max);

        sorted_arcs.erase(max);

        if (opt_debug)
            cerr << "pulling " << *arc << endl;

        arcs.erase(arc);
        remove_from(from_map, arc->m_from, arc);
        remove_from(to_map, arc->m_to, arc);

        out << symtab.get_symbol_name(arc->m_to) << endl;

        // Merge arc->m_to into arc->m_from. First, modify anything
        // that used to point to arc->m_to to point to arc->m_from.
        typedef list<call_graph_arc *> arc_list_t;
        arc_list_t map_add_queue;

        pair<arc_map_t::iterator, arc_map_t::iterator> p;

        // Find all the arcs that point to arc->m_to.
        p = to_map.equal_range(arc->m_to);
        for (arc_map_t::iterator i = p.first; i != p.second; ++i) {
            // Remove the arc that points to arc->m_to (`doomed') from
            // all of our indicies.
            call_graph_arc *doomed = i->second;
            const Elf32_Sym *source = doomed->m_from;

            sorted_arcs.erase(doomed);
            arcs.erase(doomed);
            remove_from(from_map, source, doomed);
            // N.B. that `doomed' will be removed from the `to_map'
            // after the loop completes.

            // Create a new (or locate an existing) arc whose source
            // was the doomed arc's source, and whose sink is
            // arc->m_from (i.e., the node that subsumed arc->m_to).
            call_graph_arc *merge;
            call_graph_arc key = call_graph_arc(source, arc->m_from);
            arc_container_t::iterator iter = arcs.find(&key);

            if (iter == arcs.end()) {
                merge = new call_graph_arc(source, arc->m_from);

                arcs.insert(merge);
                from_map.insert(arc_map_t::value_type(merge->m_from, merge));
                map_add_queue.push_back(merge);
            }
            else {
                // We found an existing arc: temporarily remove it
                // from the sorted index.
                merge = const_cast<call_graph_arc *>(*iter);
                sorted_arcs.erase(merge);
            }

            // Include the doomed arc's weight in the merged arc, and
            // (re-)insert it into the sorted index.
            merge->m_count += doomed->m_count;
            sorted_arcs.insert(merge);

            delete doomed;
        }

        to_map.erase(p.first, p.second);

        for (arc_list_t::iterator merge = map_add_queue.begin();
             merge != map_add_queue.end();
             map_add_queue.erase(merge++)) {
            to_map.insert(arc_map_t::value_type((*merge)->m_to, *merge));
        }

        // Now, roll anything that arc->m_to used to point at into
        // what arc->m_from now points at.

        // Collect all of the arcs that originate at arc->m_to.
        p = from_map.equal_range(arc->m_to);
        for (arc_map_t::iterator i = p.first; i != p.second; ++i) {
            // Remove the arc originating from arc->m_to (`doomed')
            // from all of our indicies.
            call_graph_arc *doomed = i->second;
            const Elf32_Sym *sink = doomed->m_to;

            // There shouldn't be any self-referential arcs.
            assert(sink != arc->m_to);

            sorted_arcs.erase(doomed);
            arcs.erase(doomed);
            remove_from(to_map, sink, doomed);
            // N.B. that `doomed' will be removed from the `from_map'
            // once the loop completes.

            // Create a new (or locate an existing) arc whose source
            // is arc->m_from and whose sink was the removed arc's
            // sink.
            call_graph_arc *merge;
            call_graph_arc key = call_graph_arc(arc->m_from, sink);
            arc_container_t::iterator iter = arcs.find(&key);

            if (iter == arcs.end()) {
                merge = new call_graph_arc(arc->m_from, sink);

                arcs.insert(merge);

                map_add_queue.push_back(merge);
                to_map.insert(arc_map_t::value_type(merge->m_to, merge));
            }
            else {
                // We found an existing arc: temporarily remove it
                // from the sorted index.
                merge = const_cast<call_graph_arc *>(*iter);
                sorted_arcs.erase(merge);
            }

            // Include the doomed arc's weight in the merged arc, and
            // (re-)insert it into the sorted index.
            merge->m_count += doomed->m_count;
            sorted_arcs.insert(merge);

            delete doomed;
        }

        from_map.erase(p.first, p.second);

        for (arc_list_t::iterator merge = map_add_queue.begin();
             merge != map_add_queue.end();
             map_add_queue.erase(merge++)) {
            from_map.insert(arc_map_t::value_type((*merge)->m_from, *merge));
        }
    }

    out.close();
    return 0;
}
