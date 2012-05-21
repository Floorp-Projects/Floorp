/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Converts a binary stream of address references to a text stream,
  hexadecimal, newline separated.

 */

#include <stdio.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    unsigned int buf[1024];
    ssize_t cb;

    while ((cb = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        if (cb % sizeof buf[0])
            fprintf(stderr, "unaligned read\n");

        unsigned int *addr = buf;
        unsigned int *limit = buf + (cb / 4);

        for (; addr < limit; ++addr)
            printf("%x\n", *addr);
    }

    return 0;
}
