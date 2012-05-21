/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A program that computes a call histogram from runtime call
  information. It reads a list of address references (e.g., as
  computed by libcygprof.so), and uses an ELF image to map the
  addresses to functions.

 */

#include <fstream>
#include <hash_map>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "elf_symbol_table.h"

#define _GNU_SOURCE
#include <getopt.h>

const char *opt_exe;
int opt_tick = 0;

elf_symbol_table table;

typedef hash_map<unsigned int, unsigned int> histogram_t;
histogram_t histogram;

static struct option long_options[] = {
    { "exe",  required_argument, 0, 'e' },
    { "tick", optional_argument, 0, 't' },
    { 0,      0,                 0, 0   }
};

static void
usage(const char *name)
{
    cerr << "usage: " << name << " --exe=<image> [--tick[=count]]" << endl;
}

static void
map_addrs(int fd)
{
    // Read the binary addresses from stdin.
    unsigned int buf[128];
    ssize_t cb;

    unsigned int count = 0;
    while ((cb = read(fd, buf, sizeof buf)) > 0) {
        if (cb % sizeof buf[0])
            fprintf(stderr, "unaligned read\n");

        unsigned int *addr = buf;
        unsigned int *limit = buf + (cb / 4);

        for (; addr < limit; ++addr) {
            const Elf32_Sym *sym = table.lookup(*addr);
            if (sym)
                ++histogram[reinterpret_cast<unsigned int>(sym)];

            if (opt_tick && (++count % opt_tick == 0)) {
                cerr << ".";
                flush(cerr);
            }
        }
    }

    if (opt_tick)
        cerr << endl;
}

int
main(int argc, char *argv[])
{
    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "e:t", long_options, &option_index);

        if (c < 0)
            break;

        switch (c) {
        case 'e':
            opt_exe = optarg;
            break;

        case 't':
            opt_tick = optarg ? atoi(optarg) : 1000000;
            break;

        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (! opt_exe) {
        usage(argv[0]);
        return 1;
    }

    table.init(opt_exe);

    // Process addresses.
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
    
    // Emit the histogram.
    histogram_t::const_iterator limit = histogram.end();
    histogram_t::const_iterator i;
    for (i = histogram.begin(); i != limit; ++i) {
        const Elf32_Sym *sym = reinterpret_cast<const Elf32_Sym *>(i->first);
        cout.form("%08x %6d %2d %10d ",
                  sym->st_value,
                  sym->st_size,
                  sym->st_shndx,
                  i->second);

        cout << table.get_symbol_name(sym) << endl;
    }

    return 0;
}
