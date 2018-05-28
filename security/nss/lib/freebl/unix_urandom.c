/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "secerr.h"
#include "secrng.h"
#include "prprf.h"

/* syscall getentropy() is limited to retrieving 256 bytes */
#define GETENTROPY_MAX_BYTES 256

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

#if defined(__OpenBSD__) || (defined(LINUX) && defined(__GLIBC__) && ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 25))))
    int result;

    while (fileBytes < maxLen) {
        size_t getBytes = maxLen - fileBytes;
        if (getBytes > GETENTROPY_MAX_BYTES) {
            getBytes = GETENTROPY_MAX_BYTES;
        }
        result = getentropy(buffer, getBytes);
        if (result == 0) { /* success */
            fileBytes += getBytes;
            buffer += getBytes;
        } else {
            break;
        }
    }
    if (fileBytes == maxLen) { /* success */
        return maxLen;
    }
    /* If we failed with an error other than ENOSYS, it means the destination
     * buffer is not writeable. We don't need to try writing to it again. */
    if (errno != ENOSYS) {
        PORT_SetError(SEC_ERROR_NEED_RANDOM);
        return 0;
    }
    /* ENOSYS means the kernel doesn't support getentropy()/getrandom().
     * Reset the number of bytes to get and fall back to /dev/urandom. */
    fileBytes = 0;
#endif
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
