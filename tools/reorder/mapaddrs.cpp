/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A program that reverse-maps a binary stream of address references to
  function names.

 */

#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "elf_symbol_table.h"

#define _GNU_SOURCE
#include <getopt.h>

static int
map_addrs(int fd, elf_symbol_table &table)
{
    // Read the binary addresses from stdin.
    unsigned int buf[128];
    ssize_t cb;

    while ((cb = read(fd, buf, sizeof buf)) > 0) {
        if (cb % sizeof buf[0])
            fprintf(stderr, "unaligned read\n");

        unsigned int *addr = buf;
        unsigned int *limit = buf + (cb / 4);

        for (; addr < limit; ++addr) {
            const Elf32_Sym *sym = table.lookup(*addr);
            if (sym)
                cout << table.get_symbol_name(sym) << endl;
        }
    }

    return 0;
}

static struct option long_options[] = {
    { "exe", required_argument, 0, 'e' },
    { 0,     0,                 0, 0   }
};

static void
usage()
{
    cerr << "usage: mapaddrs --exe=exe file ..." << endl;
}

int
main(int argc, char *argv[])
{
    elf_symbol_table table;

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "e:", long_options, &option_index);

        if (c < 0)
            break;

        switch (c) {
        case 'e':
            table.init(optarg);
            break;

        default:
            usage();
            return 1;
        }
    }

    if (optind >= argc) {
        map_addrs(STDIN_FILENO, table);
    }
    else {
        do {
            int fd = open(argv[optind], O_RDONLY);
            if (fd < 0) {
                perror(argv[optind]);
                return 1;
            }

            map_addrs(fd, table);
            close(fd);
        } while (++optind < argc);
    }

    return 0;
}
