/*
 * blapii.h - private data structures and prototypes for the freebl library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _BLAPII_H_
#define _BLAPII_H_

#include "blapit.h"

/* max block size of supported block ciphers */
#define MAX_BLOCK_SIZE 16

typedef SECStatus (*freeblCipherFunc)(void *cx, unsigned char *output,
                                      unsigned int *outputLen, unsigned int maxOutputLen,
                                      const unsigned char *input, unsigned int inputLen,
                                      unsigned int blocksize);
typedef void (*freeblDestroyFunc)(void *cx, PRBool freeit);

SEC_BEGIN_PROTOS

SECStatus BL_FIPSEntryOK(PRBool freeblOnly);
PRBool BL_POSTRan(PRBool freeblOnly);

#if defined(XP_UNIX) && !defined(NO_FORK_CHECK)

extern PRBool bl_parentForkedAfterC_Initialize;

#define SKIP_AFTER_FORK(x)                 \
    if (!bl_parentForkedAfterC_Initialize) \
    x

#else

#define SKIP_AFTER_FORK(x) x

#endif

SEC_END_PROTOS

#endif /* _BLAPII_H_ */
