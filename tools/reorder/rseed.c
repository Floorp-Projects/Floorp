/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A program that reads /dev/random to produce a seed for srand(3).

 */

#include <stdio.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
    int fd, ok, seed = 0;

    fd = open("/dev/random", O_RDONLY);
    if (fd < 0) {
        perror("/dev/random");
        return 1;
    }

    ok = read(fd, &seed, sizeof seed);
    if (ok > 0)
        printf("%d\n", seed);
    else
        perror("/dev/random");

    close(fd);
    return 0;
}
