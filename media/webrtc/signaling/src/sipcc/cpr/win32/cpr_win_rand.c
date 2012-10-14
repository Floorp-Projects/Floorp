/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"


/*
 * cpr_crypto_rand
 *
 * Generate crypto graphically random number for a desired length.
 * On WIN32 platform, there is no security support, for now generate
 * enough random number of bytes using normal "rand()" function.
 *
 * Parameters:
 *     buf  - pointer to the buffer to store the result of random
 *            bytes requested.
 *     len  - pointer to the length of the desired random bytes.
 *            When calling the function, the integer's value
 *            should be set to the desired number of random
 *            bytes ('buf' should be of at least this size).
 *            upon success, its value will be set to the
 *            actual number of random bytes being returned.
 *            (realistically, there is a maximum number of
 *            random bytes that can be returned at a time.
 *            if the caller request more than that, the
 *            'len' will indicate how many bytes are actually being
 *            returned) on failure, its value will be set to 0.
 *
 * Return Value:
 *     1 - success.
 *     0 - fail.
 */
int cpr_crypto_rand (uint8_t *buf, int *len)
{
#ifdef CPR_WIN_RAND_DEBUG
    /*
     * This is for debug only.
     */
    int      i, total_bytes;

    if ((buf == NULL) || (len == NULL)) {
        /* Invalid parameters */
        return (0);
    }

    total_bytes = *len;
    for (i = 0, i < total_bytes ; i++) {
        *buf = rand() & 0xff;
        buf++;
    }
    return (1);

#else
    /*
     * No crypto. random support on WIN32
     */
    if (len != NULL) {
        /* length pointer is ok, set it to zero to indicate failure */
        *len = 0;
    }
    return (0);

#endif
}
