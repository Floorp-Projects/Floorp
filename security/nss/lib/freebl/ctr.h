/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTR_H
#define CTR_H 1

#include "blapii.h"

/* This structure is defined in this header because both ctr.c and gcm.c
 * need it. */
struct CTRContextStr {
   freeblCipherFunc cipher;
   void *context;
   unsigned char counter[MAX_BLOCK_SIZE];
   unsigned char buffer[MAX_BLOCK_SIZE];
   unsigned long counterBits;
   unsigned int bufPtr;
};

typedef struct CTRContextStr CTRContext;

SECStatus CTR_InitContext(CTRContext *ctr, void *context,
			freeblCipherFunc cipher, const unsigned char *param,
			unsigned int blocksize);

/*
 * The context argument is the inner cipher context to use with cipher. The
 * CTRContext does not own context. context needs to remain valid for as long
 * as the CTRContext is valid.
 *
 * The cipher argument is a block cipher in the ECB encrypt mode.
 */
CTRContext * CTR_CreateContext(void *context, freeblCipherFunc cipher,
			const unsigned char *param, unsigned int blocksize);

void CTR_DestroyContext(CTRContext *ctr, PRBool freeit);

SECStatus CTR_Update(CTRContext *ctr, unsigned char *outbuf,
			unsigned int *outlen, unsigned int maxout,
			const unsigned char *inbuf, unsigned int inlen,
			unsigned int blocksize);

#endif
