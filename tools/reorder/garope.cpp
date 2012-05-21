/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A program that attempts to find an optimal function ordering for an
  executable using a genetic algorithm whose fitness function is
  computed using runtime profile information.

  The fitness function was inspired by Nat Friedman's <nat@nat.org>
  work on `grope':

    _GNU Rope - A Subroutine Position Optimizer_
    <http://www.hungry.com/~shaver/grope/grope.ps>

  Brendan Eich <brendan@mozilla.org> told me tales about Scott Furman
  doing something like this, which sort of made me want to try it.

  As far as I can tell, it would take a lot of computers a lot of time
  to actually find something useful on a non-trivial program using
  this.

 */

#include <assert.h>
#include <fstream>
#include <hash_map>
#include <vector>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "elf_symbol_table.h"

#define _GNU_SOURCE
#include <getopt.h>

#define PAGE_SIZE 4096
#define SYMBOL_ALIGN 4

//----------------------------------------------------------------------

class call_pair
{
public:
    const Elf32_Sym *m_lo;
    const Elf32_Sym *m_hi;

    call_pair(const Elf32_Sym *site1, const Elf32_Sym *site2)
    {
        if (site1 < site2) {
            m_lo = site1;
            m_hi = site2;
        }
        else {
            m_hi = site1;
            m_lo = site2;
        }
    }

    friend bool
    operator==(const call_pair &lhs, const call_pair &rhs)
    {
        return (lhs.m_lo == rhs.m_lo) && (lhs.m_hi == rhs.m_hi);
    }
};

// Straight outta plhash.c!
#define GOLDEN_RATIO 0x9E3779B9U

template<>
struct hash<call_pair>
{
    size_t operator()(const call_pair &pair) const
    {
        size_t h = (reinterpret_cast<size_t>(pair.m_hi) >> 4);
        h += (reinterpret_cast<size_t>(pair.m_lo) >> 4);
        h *= GOLDEN_RATIO;
        return h;
    }
};

//----------------------------------------------------------------------

struct hash<const Elf32_Sym *>
{
    size_t operator()(const Elf32_Sym *sym) const
    {
        return (reinterpret_cast<size_t>(sym) >> 4) * GOLDEN_RATIO;
    }
};

//----------------------------------------------------------------------

typedef hash_map<call_pair, unsigned int> call_graph_t;
call_graph_t call_graph;

typedef hash_map<const Elf32_Sym *, unsigned int> histogram_t;
histogram_t histogram;
long long total_calls = 0;

elf_symbol_table symtab;

bool opt_debug = false;
int opt_generations = 10;
int opt_mutate = 0;
const char *opt_out = "order.out";
int opt_population_size = 100;
int opt_tick = 0;
bool opt_verbose = false;
int opt_window = 0;

static struct option long_options[] = {
    { "debug",       no_argument,       0, 'd' },
    { "exe",         required_argument, 0, 'e' },
    { "generations", required_argument, 0, 'g' },
    { "help",        no_argument,       0, '?' },
    { "mutate",      required_argument, 0, 'm' },
    { "out",         required_argument, 0, 'o' },
    { "population",  required_argument, 0, 'p' },
    { "seed",        required_argument, 0, 's' },
    { "tick",        optional_argument, 0, 't' },
    { "verbose",     no_argument,       0, 'v' },
    { "window",      required_argument, 0, 'w' },
    { 0,             0,                 0, 0   }
};

//----------------------------------------------------------------------

static long long
llrand()
{
    long long result;
    result = (long long) rand();
    result *= (long long) (unsigned int) (RAND_MAX + 1);
    result += (long long) rand();
    return result;
}

//----------------------------------------------------------------------

class symbol_order {
public:
    typedef vector<const Elf32_Sym *> vector_t;
    typedef long long score_t;

    static const score_t max_score;

    /**
     * A vector of symbols that is this ordering.
     */
    vector_t m_ordering;

    /**
     * The symbol ordering's score.
     */
    score_t  m_score;

    symbol_order() : m_score(0) {}

    /**
     * ``Shuffle'' a symbol ordering, randomizing it.
     */
    void shuffle();

    /**
     * Initialize this symbol ordering by performing a crossover from
     * two ``parent'' symbol orderings.
     */
    void crossover_from(const symbol_order *father, const symbol_order *mother);

    /**
     * Randomly mutate this symbol ordering.
     */
    void mutate();

    /**
     * Score a symbol ordering based on paginated locality.
     */
    score_t compute_score_page();

    /**
     * Score a symbol ordering based on a sliding window.
     */
    score_t compute_score_window(int window_size);

    static score_t compute_score(symbol_order &order);

    /**
     * Use the symbol table to dump the ordered symbolic constants.
     */
    void dump_symbols() const;

    friend ostream &
    operator<<(ostream &out, const symbol_order &order);
};

const symbol_order::score_t
symbol_order::max_score = ~((symbol_order::score_t)1 << ((sizeof(symbol_order::score_t) * 8) - 1));

symbol_order::score_t
symbol_order::compute_score_page()
{
    m_score = 0;

    unsigned int off = 0; // XXX in reality, probably not page-aligned to start

    vector_t::const_iterator end = m_ordering.end(),
        last = end,
        sym = m_ordering.begin();

    while (sym != end) {
        vector_t page;

        // If we had a symbol that spilled over from the last page,
        // then include it here.
        if (last != end)
            page.push_back(*last);

        // Pack symbols into the page
        do {
            page.push_back(*sym);

            int size = (*sym)->st_size;
            size += SYMBOL_ALIGN - 1;
            size &= ~(SYMBOL_ALIGN - 1);

            off += size;
        } while (++sym != end && off < PAGE_SIZE);

        // Remember if there was spill-over.
        off %= PAGE_SIZE;
        last = (off != 0) ? sym : end;

        // Now score the page as the count of all calls to symbols on
        // the page, less calls between the symbols on the page.
        vector_t::const_iterator page_end = page.end();
        for (vector_t::const_iterator i = page.begin(); i != page_end; ++i) {
            histogram_t::const_iterator func = histogram.find(*i);
            if (func == histogram.end())
                continue;

            m_score += func->second;

            vector_t::const_iterator j = i;
            for (++j; j != page_end; ++j) {
                call_graph_t::const_iterator call =
                    call_graph.find(call_pair(*i, *j));

                if (call != call_graph.end())
                    m_score -= call->second;
            }
        }
    }

    assert(m_score >= 0);

    // Integer reciprocal so we minimize instead of maximize.
    if (m_score == 0)
        m_score = 1;

    m_score = (total_calls / m_score) + 1;

    return m_score;
}

symbol_order::score_t
symbol_order::compute_score_window(int window_size)
{
    m_score = 0;

    vector_t::const_iterator *window = new vector_t::const_iterator[window_size];
    int window_fill = 0;

    vector_t::const_iterator end = m_ordering.end(),
        sym = m_ordering.begin();

    for (; sym != end; ++sym) {
        histogram_t::const_iterator func = histogram.find(*sym);
        if (func != histogram.end()) {
            long long scale = ((long long) 1) << window_size;

            m_score += func->second * scale * 2;

            vector_t::const_iterator *limit = window + window_fill;
            vector_t::const_iterator *iter;
            for (iter = window ; iter < limit; ++iter) {
                call_graph_t::const_iterator call =
                    call_graph.find(call_pair(*sym, **iter));

                if (call != call_graph.end())
                    m_score -= (call->second * scale);
            
                scale >>= 1;
            }
        }

        // Slide the window.
        vector_t::const_iterator *begin = window;
        vector_t::const_iterator *iter;
        for (iter = window + (window_size - 1); iter > begin; --iter)
            *iter = *(iter - 1);

        if (window_fill < window_size)
            ++window_fill;

        *window = sym;
    }

    delete[] window;

    assert(m_score >= 0);

    // Integer reciprocal so we minimize instead of maximize.
    if (m_score == 0)
        m_score = 1;

    m_score = (total_calls / m_score) + 1;

    return m_score;
}

symbol_order::score_t
symbol_order::compute_score(symbol_order &order)
{
    if (opt_window)
        return order.compute_score_window(opt_window);

    return order.compute_score_page();
}

void
symbol_order::shuffle()
{
    vector_t::iterator sym = m_ordering.begin();
    vector_t::iterator end = m_ordering.end();
    for (; sym != end; ++sym) {
        int i = rand() % m_ordering.size();
        const Elf32_Sym *temp = *sym;
        *sym = m_ordering[i];
        m_ordering[i] = temp;
    }
}

void
symbol_order::crossover_from(const symbol_order *father, const symbol_order *mother)
{
    histogram_t used;

    m_ordering = vector_t(father->m_ordering.size(), 0);

    vector_t::const_iterator parent_sym = father->m_ordering.begin();
    vector_t::iterator sym = m_ordering.begin();
    vector_t::iterator end = m_ordering.end();

    for (; sym != end; ++sym, ++parent_sym) {
        if (rand() % 2) {
            *sym = *parent_sym;
            used[*parent_sym] = 1;
        }
    }

    parent_sym = mother->m_ordering.begin();
    sym = m_ordering.begin();

    for (; sym != end; ++sym) {
        if (! *sym) {
            while (used[*parent_sym])
                ++parent_sym;

            *sym = *parent_sym++;
        }
    }
}

void
symbol_order::mutate()
{
    int i, j;
    i = rand() % m_ordering.size();
    j = rand() % m_ordering.size();

    const Elf32_Sym *temp = m_ordering[i];
    m_ordering[i] = m_ordering[j];
    m_ordering[j] = temp;
}

void
symbol_order::dump_symbols() const
{
    ofstream out(opt_out);

    vector_t::const_iterator sym = m_ordering.begin();
    vector_t::const_iterator end = m_ordering.end();
    for (; sym != end; ++sym)
        out << symtab.get_symbol_name(*sym) << endl;

    out.close();
}

ostream &
operator<<(ostream &out, const symbol_order &order)
{
    out << "symbol_order(" << order.m_score << ") ";

    symbol_order::vector_t::const_iterator sym = order.m_ordering.begin();
    symbol_order::vector_t::const_iterator end = order.m_ordering.end();
    for (; sym != end; ++sym)
        out.form("%08x ", *sym);

    out << endl;

    return out;
}

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
    cerr << "  --generations=<num>, -g <num>" << endl;
    cerr << "      Specify the number of generations to run the GA (default is 10)." << endl;
    cerr << "  --help, -?" << endl;
    cerr << "      Print this message and exit." << endl;
    cerr << "  --mutate=<num>, -m <num>" << endl;
    cerr << "      Mutate every <num>th individual, or zero for no mutation (default)." << endl;
    cerr << "  --out=<file>, -o <file>" << endl;
    cerr << "      Specify the output file to which to dump the symbol ordering of the" << endl;
    cerr << "      best individual (default is `order.out')." << endl;
    cerr << "  --population=<num>, -p <num>" << endl;
    cerr << "      Set the population size to <num> individuals (default is 100)." << endl;
    cerr << "  --seed=<num>, -s <num>" << endl;
    cerr << "      Specify a seed to srand()." << endl;
    cerr << "  --tick[=<num>], -t [<num>]" << endl;
    cerr << "      When reading address data, print a dot to stderr every <num>th" << endl;
    cerr << "      address processed from the call trace. If specified with no argument," << endl;
    cerr << "      a dot will be printed for every million addresses processed." << endl;
    cerr << "  --verbose, -v" << endl;
    cerr << "      Issue progress messages to stderr." << endl;
    cerr << "  --window=<num>, -w <num>" << endl;
    cerr << "      Use a sliding window instead of pagination to score orderings." << endl;
    cerr << endl;
    cerr << "This program uses a genetic algorithm to produce an `optimal' ordering for" << endl;
    cerr << "an executable based on call patterns." << endl;
    cerr << endl;
    cerr << "Addresses from a call trace are read as binary data from the files" << endl;
    cerr << "specified, or from stdin if no files are specified. These addresses" << endl;
    cerr << "are used with the symbolic information from the executable to create" << endl;
    cerr << "a call graph. This call graph is used to `score' arbitrary symbol" << endl;
    cerr << "orderings, and provides the fitness function for the GA." << endl;
    cerr << endl;
}

/**
 * Using the symbol table, map a stream of address references into a
 * callgraph and a histogram.
 */
static void
map_addrs(int fd)
{
    const Elf32_Sym *last = 0;
    unsigned int buf[128];
    ssize_t cb;

    unsigned int count = 0;
    while ((cb = read(fd, buf, sizeof buf)) > 0) {
        if (cb % sizeof buf[0])
            fprintf(stderr, "unaligned read\n");

        unsigned int *addr = buf;
        unsigned int *limit = buf + (cb / 4);

        for (; addr < limit; ++addr) {
            const Elf32_Sym *sym = symtab.lookup(*addr);

            if (last && sym && last != sym) {
                ++total_calls;
                ++histogram[sym];
                ++call_graph[call_pair(last, sym)];

                if (opt_tick && (++count % opt_tick == 0)) {
                    cerr << ".";
                    flush(cerr);
                }
            }

            last = sym;
        }
    }

    if (opt_tick)
        cerr << endl;

    cerr << "Total calls: " << total_calls << endl;
    total_calls *= 1024;

    if (opt_window)
        total_calls <<= (opt_window + 1);
}

static symbol_order *
pick_parent(symbol_order *ordering, int max, int index)
{
    while (1) {
        index -= ordering->m_score;
        if (index < 0)
            break;

        ++ordering;
    }

    return ordering;
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
        c = getopt_long(argc, argv, "?de:g:m:o:p:s:t:vw:", long_options, &option_index);

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

        case 'g':
            opt_generations = atoi(optarg);
            break;

        case 'm':
            opt_mutate = atoi(optarg);
            break;

        case 'o':
            opt_out = optarg;
            break;

        case 'p':
            opt_population_size = atoi(optarg);
            break;

        case 's':
            srand(atoi(optarg));
            break;

        case 't':
            opt_tick = optarg ? atoi(optarg) : 1000000;
            break;

        case 'v':
            opt_verbose = true;
            break;

        case 'w':
            opt_window = atoi(optarg);
            if (opt_window < 0 || opt_window > 8) {
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

    // Process addresses to construct the call graph.
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

    if (opt_debug) {
        cerr << "Call graph:" << endl;

        call_graph_t::const_iterator limit = call_graph.end();
        call_graph_t::const_iterator i;
        for (i = call_graph.begin(); i != limit; ++i) {
            const call_pair& pair = i->first;
            cerr.form("%08x %08x %10d\n",
                      pair.m_lo->st_value,
                      pair.m_hi->st_value,
                      i->second);
        }
    }

    // Collect the symbols into a vector
    symbol_order::vector_t ordering;
    elf_symbol_table::const_iterator end = symtab.end();
    for (elf_symbol_table::const_iterator sym = symtab.begin(); sym != end; ++sym) {
        if (symtab.is_function(sym))
            ordering.push_back(sym);
    }

    if (opt_verbose) {
        symbol_order initial;
        initial.m_ordering = ordering;
        cerr << "created initial ordering, score=" << symbol_order::compute_score(initial) << endl;

        if (opt_debug)
            cerr << initial;
    }

    // Create a population.
    if (opt_verbose)
        cerr << "creating population" << endl;

    symbol_order *population = new symbol_order[opt_population_size];

    symbol_order::score_t total = 0, min = symbol_order::max_score, max = 0;

    // Score it.
    symbol_order *order = population;
    symbol_order *limit = population + opt_population_size;
    for (; order < limit; ++order) {
        order->m_ordering = ordering;
        order->shuffle();

        symbol_order::score_t score = symbol_order::compute_score(*order);

        if (opt_debug)
            cerr << *order;

        if (min > score)
            min = score;
        if (max < score)
            max = score;

        total += score;
    }

    if (opt_verbose) {
        cerr << "Initial population";
        cerr << ": min=" << min;
        cerr << ", max=" << max;
        cerr << " mean=" << (total / opt_population_size);
        cerr << endl;
    }


    // Run the GA.
    if (opt_verbose)
        cerr << "begininng ga" << endl;

    symbol_order::score_t best = 0;

    for (int generation = 1; generation <= opt_generations; ++generation) {
        // Create a new population.
        symbol_order *offspring = new symbol_order[opt_population_size];

        symbol_order *kid = offspring;
        symbol_order *offspring_limit = offspring + opt_population_size;
        for (; kid < offspring_limit; ++kid) {
            // Pick parents.
            symbol_order *father, *mother;
            father = pick_parent(population, max, llrand() % total);
            mother = pick_parent(population, max, llrand() % total);

            // Create a kid.
            kid->crossover_from(father, mother);

            // Mutate, possibly.
            if (opt_mutate) {
                if (rand() % opt_mutate == 0)
                    kid->mutate();
            }
        }

        delete[] population;
        population = offspring;

        // Score the new population.
        total = 0;
        min = symbol_order::max_score;
        max = 0;

        symbol_order *fittest = 0;

        limit = offspring_limit;
        for (order = population; order < limit; ++order) {
            symbol_order::score_t score = symbol_order::compute_score(*order);

            if (opt_debug)
                cerr << *order;

            if (min > score)
                min = score;

            if (max < score)
                max = score;

            if (best < score) {
                best = score;
                fittest = order;
            }

            total += score;
        }

        if (opt_verbose) {
            cerr << "Generation " << generation;
            cerr << ": min=" << min;
            cerr << ", max=" << max;
            if (fittest)
                cerr << "*";
            cerr << " mean=" << (total / opt_population_size);
            cerr << endl;
        }

        // If we've found a new ``best'' individual, dump it.
        if (fittest)
            fittest->dump_symbols();
    }

    delete[] population;
    return 0;
}
