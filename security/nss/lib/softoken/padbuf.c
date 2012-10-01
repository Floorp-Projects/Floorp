/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "blapit.h"
#include "secport.h"
#include "secerr.h"

/*
 * Prepare a buffer for any padded CBC encryption algorithm, growing to the 
 * appropriate boundary and filling with the appropriate padding.
 * blockSize must be a power of 2.
 *
 * NOTE: If arena is non-NULL, we re-allocate from there, otherwise
 * we assume (and use) XP memory (re)allocation.
 */
unsigned char *
CBC_PadBuffer(PRArenaPool *arena, unsigned char *inbuf, unsigned int inlen,
	      unsigned int *outlen, int blockSize)
{
    unsigned char *outbuf;
    unsigned int   des_len;
    unsigned int   i;
    unsigned char  des_pad_len;

    /*
     * We need from 1 to blockSize bytes -- we *always* grow.
     * The extra bytes contain the value of the length of the padding:
     * if we have 2 bytes of padding, then the padding is "0x02, 0x02".
     */
    des_len = (inlen + blockSize) & ~(blockSize - 1);

    if (arena != NULL) {
	outbuf = (unsigned char*)PORT_ArenaGrow (arena, inbuf, inlen, des_len);
    } else {
	outbuf = (unsigned char*)PORT_Realloc (inbuf, des_len);
    }

    if (outbuf == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    des_pad_len = des_len - inlen;
    for (i = inlen; i < des_len; i++)
	outbuf[i] = des_pad_len;

    *outlen = des_len;
    return outbuf;
}
