/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is ``mapaddrs''
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corp.  Portions created by the Initial Developer are
 * Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
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
