/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <unistd.h>
#include "secerr.h"
#include "secrng.h"
#include "prprf.h"

void
RNG_SystemInfoForRNG(void)
{
    PRUint8 bytes[SYSTEM_RNG_SEED_COUNT];
    size_t numBytes = RNG_SystemRNG(bytes, SYSTEM_RNG_SEED_COUNT);
    if (!numBytes) {
        /* error is set */
        return;
    }
    RNG_RandomUpdate(bytes, numBytes);
}

size_t
RNG_SystemRNG(void *dest, size_t maxLen)
{
    int fd;
    int bytes;
    size_t fileBytes = 0;
    unsigned char *buffer = dest;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        return 0;
    }
    while (fileBytes < maxLen) {
        bytes = read(fd, buffer, maxLen - fileBytes);
        if (bytes <= 0) {
            break;
        }
        fileBytes += bytes;
        buffer += bytes;
    }
    (void)close(fd);
    if (fileBytes != maxLen) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        return 0;
    }
    return fileBytes;
}
