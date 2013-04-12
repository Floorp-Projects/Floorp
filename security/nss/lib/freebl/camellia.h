/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * $Id$
 */

#ifndef _CAMELLIA_H_
#define _CAMELLIA_H_ 1

#define CAMELLIA_BLOCK_SIZE 16  /* bytes */
#define CAMELLIA_MIN_KEYSIZE 16 /* bytes */
#define CAMELLIA_MAX_KEYSIZE 32 /* bytes */

#define CAMELLIA_MAX_EXPANDEDKEY (34*2) /* 32bit unit */

typedef PRUint32 KEY_TABLE_TYPE[CAMELLIA_MAX_EXPANDEDKEY];

typedef SECStatus CamelliaFunc(CamelliaContext *cx, unsigned char *output,
			       unsigned int *outputLen,
			       unsigned int maxOutputLen,
			       const unsigned char *input,
			       unsigned int inputLen);

typedef SECStatus CamelliaBlockFunc(const PRUint32 *subkey, 
				    unsigned char *output,
				    const unsigned char *input);

/* CamelliaContextStr
 *
 * Values which maintain the state for Camellia encryption/decryption.
 *
 * keysize     - the number of key bits
 * worker      - the encryption/decryption function to use with this context
 * iv          - initialization vector for CBC mode
 * expandedKey - the round keys in 4-byte words
 */
struct CamelliaContextStr
{
    PRUint32  keysize; /* bytes */
    CamelliaFunc  *worker;
    PRUint32      expandedKey[CAMELLIA_MAX_EXPANDEDKEY];
    PRUint8 iv[CAMELLIA_BLOCK_SIZE];
};

#endif /* _CAMELLIA_H_ */
