/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifndef _MD5_H
#define _MD5_H

/* delineate the cisco changes to the RSA supplied module */
#define CISCO_MD5_MODS

#if defined(CISCO_MD5_MODS)

#define MD5_LEN 16

#endif /* defined(CISCO_MD5_MODS) */

/* MD5 context. */
typedef struct {
    unsigned long int state[4]; /* state (ABCD) */
    unsigned long int count[2]; /* number of bits, modulo 2^64 (lsb first) */
    unsigned char buffer[64];   /* input buffer */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char[16], MD5_CTX *);

//#define MD5TESTSUITE 1
#if defined(MD5TESTSUITE) /* only needed if running MD5 verfication tests */
void MDString(char *string);
void MDPrint(char *digest);
void MDTestSuite(void);
#endif

#endif /* _MD5_H */
