/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SHA_256_H_
#define _SHA_256_H_

#include "prtypes.h"

struct SHA256ContextStr;

typedef void (*sha256_compress_t)(struct SHA256ContextStr *);
typedef void (*sha256_update_t)(struct SHA256ContextStr *, const unsigned char *,
                                unsigned int);

struct SHA256ContextStr {
    union {
        PRUint32 w[64]; /* message schedule, input buffer, plus 48 words */
        PRUint8 b[256];
    } u;
    PRUint32 h[8];           /* 8 state variables */
    PRUint32 sizeHi, sizeLo; /* 64-bit count of hashed bytes. */
    sha256_compress_t compress;
    sha256_update_t update;
};

#endif /* _SHA_256_H_ */
