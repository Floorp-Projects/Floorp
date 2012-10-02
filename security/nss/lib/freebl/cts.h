/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTS_H
#define CTS_H 1

#include "blapii.h"

typedef struct CTSContextStr CTSContext;

/*
 * The context argument is the inner cipher context to use with cipher. The
 * CTSContext does not own context. context needs to remain valid for as long
 * as the CTSContext is valid.
 *
 * The cipher argument is a block cipher in the CBC mode.
 */
CTSContext *CTS_CreateContext(void *context, freeblCipherFunc cipher,
			const unsigned char *iv, unsigned int blocksize);

void CTS_DestroyContext(CTSContext *cts, PRBool freeit);

SECStatus CTS_EncryptUpdate(CTSContext *cts, unsigned char *outbuf,
			unsigned int *outlen, unsigned int maxout,
			const unsigned char *inbuf, unsigned int inlen,
			unsigned int blocksize);
SECStatus CTS_DecryptUpdate(CTSContext *cts, unsigned char *outbuf,
			unsigned int *outlen, unsigned int maxout,
			const unsigned char *inbuf, unsigned int inlen,
			unsigned int blocksize);

#endif
