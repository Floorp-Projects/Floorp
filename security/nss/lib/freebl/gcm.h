/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GCM_H
#define GCM_H 1

#include "blapii.h"

typedef struct GCMContextStr GCMContext;

/*
 * The context argument is the inner cipher context to use with cipher. The
 * GCMContext does not own context. context needs to remain valid for as long
 * as the GCMContext is valid.
 *
 * The cipher argument is a block cipher in the ECB encrypt mode.
 */
GCMContext * GCM_CreateContext(void *context, freeblCipherFunc cipher,
			const unsigned char *params, unsigned int blocksize);
void GCM_DestroyContext(GCMContext *gcm, PRBool freeit);
SECStatus GCM_EncryptUpdate(GCMContext  *gcm, unsigned char *outbuf,
			unsigned int *outlen, unsigned int maxout,
			const unsigned char *inbuf, unsigned int inlen,
			unsigned int blocksize);
SECStatus GCM_DecryptUpdate(GCMContext *gcm, unsigned char *outbuf,
			unsigned int *outlen, unsigned int maxout,
			const unsigned char *inbuf, unsigned int inlen,
			unsigned int blocksize);

#endif
