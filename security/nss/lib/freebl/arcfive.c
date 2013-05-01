/*
 * arcfive.c - stubs for RC5 - NOT a working implementation!
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "blapi.h"
#include "prerror.h"

/******************************************/
/*
** RC5 symmetric block cypher -- 64-bit block size
*/

/*
** Create a new RC5 context suitable for RC5 encryption/decryption.
**      "key" raw key data
**      "len" the number of bytes of key data
**      "iv" is the CBC initialization vector (if mode is NSS_RC5_CBC)
**      "mode" one of NSS_RC5 or NSS_RC5_CBC
**
** When mode is set to NSS_RC5_CBC the RC5 cipher is run in "cipher block
** chaining" mode.
*/
RC5Context *
RC5_CreateContext(const SECItem *key, unsigned int rounds,
                  unsigned int wordSize, const unsigned char *iv, int mode)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
}

/*
** Destroy an RC5 encryption/decryption context.
**      "cx" the context
**      "freeit" if PR_TRUE then free the object as well as its sub-objects
*/
void 
RC5_DestroyContext(RC5Context *cx, PRBool freeit) 
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
}

/*
** Perform RC5 encryption.
**      "cx" the context
**      "output" the output buffer to store the encrypted data.
**      "outputLen" how much data is stored in "output". Set by the routine
**         after some data is stored in output.
**      "maxOutputLen" the maximum amount of data that can ever be
**         stored in "output"
**      "input" the input data
**      "inputLen" the amount of input data
*/
SECStatus 
RC5_Encrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, 
	    const unsigned char *input, unsigned int inputLen)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

/*
** Perform RC5 decryption.
**      "cx" the context
**      "output" the output buffer to store the decrypted data.
**      "outputLen" how much data is stored in "output". Set by the routine
**         after some data is stored in output.
**      "maxOutputLen" the maximum amount of data that can ever be
**         stored in "output"
**      "input" the input data
**      "inputLen" the amount of input data
*/
SECStatus 
RC5_Decrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
}

