/*
 *  desblapi.c
 *
 *  core source file for DES-150 library
 *  Implement DES Modes of Operation and Triple-DES.
 *  Adapt DES-150 to blapi API.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the DES-150 library.
 *
 * The Initial Developer of the Original Code is Nelson B. Bolyard,
 * nelsonb@iname.com.  Portions created by Nelson B. Bolyard are 
 * Copyright (C) 1990, 2000  Nelson B. Bolyard, All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the GPL.
 */

#include "des.h"
#include <stddef.h>
#include "secerr.h"

#if defined(_X86_)
/* Intel X86 CPUs do unaligned loads and stores without complaint. */
#define COPY8B(to, from, ptr) \
    	HALFPTR(to)[0] = HALFPTR(from)[0]; \
    	HALFPTR(to)[1] = HALFPTR(from)[1]; 
#elif defined(USE_MEMCPY)
#define COPY8B(to, from, ptr) memcpy(to, from, 8)
#else
#define COPY8B(to, from, ptr) \
    if (((ptrdiff_t)(ptr) & 0x3) == 0) { \
    	HALFPTR(to)[0] = HALFPTR(from)[0]; \
    	HALFPTR(to)[1] = HALFPTR(from)[1]; \
    } else if (((ptrdiff_t)(ptr) & 0x1) == 0) { \
    	SHORTPTR(to)[0] = SHORTPTR(from)[0]; \
    	SHORTPTR(to)[1] = SHORTPTR(from)[1]; \
    	SHORTPTR(to)[2] = SHORTPTR(from)[2]; \
    	SHORTPTR(to)[3] = SHORTPTR(from)[3]; \
    } else { \
    	BYTEPTR(to)[0] = BYTEPTR(from)[0]; \
    	BYTEPTR(to)[1] = BYTEPTR(from)[1]; \
    	BYTEPTR(to)[2] = BYTEPTR(from)[2]; \
    	BYTEPTR(to)[3] = BYTEPTR(from)[3]; \
    	BYTEPTR(to)[4] = BYTEPTR(from)[4]; \
    	BYTEPTR(to)[5] = BYTEPTR(from)[5]; \
    	BYTEPTR(to)[6] = BYTEPTR(from)[6]; \
    	BYTEPTR(to)[7] = BYTEPTR(from)[7]; \
    } 
#endif
#define COPY8BTOHALF(to, from) COPY8B(to, from, from)
#define COPY8BFROMHALF(to, from) COPY8B(to, from, to)

static void 
DES_ECB(DESContext *cx, BYTE *out, const BYTE *in, unsigned int len)
{
    while (len) {
	DES_Do1Block(cx->ks0, in, out);
	len -= 8;
	in  += 8;
	out += 8;
    }
}

static void 
DES_EDE3_ECB(DESContext *cx, BYTE *out, const BYTE *in, unsigned int len)
{
    while (len) {
	DES_Do1Block(cx->ks0,  in, out);
	len -= 8;
	in  += 8;
	DES_Do1Block(cx->ks1, out, out);
	DES_Do1Block(cx->ks2, out, out);
	out += 8;
    }
}

static void 
DES_CBCEn(DESContext *cx, BYTE *out, const BYTE *in, unsigned int len)
{
    BYTE * bufend = in + len;
    HALF  vec[2];

    while (in != bufend) {
	COPY8BTOHALF(vec, in);
	in += 8;
	vec[0] ^= cx->iv[0];
	vec[1] ^= cx->iv[1];
	DES_Do1Block( cx->ks0, (BYTE *)vec, (BYTE *)cx->iv);
	COPY8BFROMHALF(out, cx->iv);
	out += 8;
    }
}

static void 
DES_CBCDe(DESContext *cx, BYTE *out, const BYTE *in, unsigned int len)
{
    BYTE * bufend;
    HALF oldciphertext[2];
    HALF plaintext    [2];

    for (bufend = in + len; in != bufend; ) {
	oldciphertext[0] = cx->iv[0];
	oldciphertext[1] = cx->iv[1];
	COPY8BTOHALF(cx->iv, in);
	in += 8;
	DES_Do1Block(cx->ks0, (BYTE *)cx->iv, (BYTE *)plaintext);
	plaintext[0] ^= oldciphertext[0];
	plaintext[1] ^= oldciphertext[1];
	COPY8BFROMHALF(out, plaintext);
	out += 8;
    }
}

static void 
DES_EDE3CBCEn(DESContext *cx, BYTE *out, const BYTE *in, unsigned int len)
{
    BYTE * bufend = in + len;
    HALF  vec[2];

    while (in != bufend) {
	COPY8BTOHALF(vec, in);
	in += 8;
	vec[0] ^= cx->iv[0];
	vec[1] ^= cx->iv[1];
	DES_Do1Block( cx->ks0, (BYTE *)vec,    (BYTE *)cx->iv);
	DES_Do1Block( cx->ks1, (BYTE *)cx->iv, (BYTE *)cx->iv);
	DES_Do1Block( cx->ks2, (BYTE *)cx->iv, (BYTE *)cx->iv);
	COPY8BFROMHALF(out, cx->iv);
	out += 8;
    }
}

static void 
DES_EDE3CBCDe(DESContext *cx, BYTE *out, const BYTE *in, unsigned int len)
{
    BYTE * bufend;
    HALF oldciphertext[2];
    HALF plaintext    [2];

    for (bufend = in + len; in != bufend; ) {
	oldciphertext[0] = cx->iv[0];
	oldciphertext[1] = cx->iv[1];
	COPY8BTOHALF(cx->iv, in);
	in += 8;
	DES_Do1Block(cx->ks0, (BYTE *)cx->iv,    (BYTE *)plaintext);
	DES_Do1Block(cx->ks1, (BYTE *)plaintext, (BYTE *)plaintext);
	DES_Do1Block(cx->ks2, (BYTE *)plaintext, (BYTE *)plaintext);
	plaintext[0] ^= oldciphertext[0];
	plaintext[1] ^= oldciphertext[1];
	COPY8BFROMHALF(out, plaintext);
	out += 8;
    }
}

DESContext *
DES_CreateContext(BYTE * key, BYTE *iv, int mode, PRBool encrypt)
{
    DESContext *cx = PORT_ZNew(DESContext);
    DESDirection opposite;
    if (!cx) 
    	return 0;
    cx->direction = encrypt ? DES_ENCRYPT : DES_DECRYPT;
    opposite      = encrypt ? DES_DECRYPT : DES_ENCRYPT;
    switch (mode) {
    case NSS_DES:	/* DES ECB */
	DES_MakeSchedule( cx->ks0, key, cx->direction);
	cx->worker = &DES_ECB;
	break;

    case NSS_DES_EDE3:	/* DES EDE ECB */
	cx->worker = &DES_EDE3_ECB;
	if (encrypt) {
	    DES_MakeSchedule(cx->ks0, key,      cx->direction);
	    DES_MakeSchedule(cx->ks1, key +  8, opposite);
	    DES_MakeSchedule(cx->ks2, key + 16, cx->direction);
	} else {
	    DES_MakeSchedule(cx->ks2, key,      cx->direction);
	    DES_MakeSchedule(cx->ks1, key +  8, opposite);
	    DES_MakeSchedule(cx->ks0, key + 16, cx->direction);
	}
	break;

    case NSS_DES_CBC:	/* DES CBC */
	COPY8BTOHALF(cx->iv, iv);
	cx->worker = encrypt ? &DES_CBCEn : &DES_CBCDe;
	DES_MakeSchedule(cx->ks0, key, cx->direction);
	break;

    case NSS_DES_EDE3_CBC:	/* DES EDE CBC */
	COPY8BTOHALF(cx->iv, iv);
	if (encrypt) {
	    cx->worker = &DES_EDE3CBCEn;
	    DES_MakeSchedule(cx->ks0, key,      cx->direction);
	    DES_MakeSchedule(cx->ks1, key +  8, opposite);
	    DES_MakeSchedule(cx->ks2, key + 16, cx->direction);
	} else {
	    cx->worker = &DES_EDE3CBCDe;
	    DES_MakeSchedule(cx->ks2, key,      cx->direction);
	    DES_MakeSchedule(cx->ks1, key +  8, opposite);
	    DES_MakeSchedule(cx->ks0, key + 16, cx->direction);
	}
	break;

    default:
    	PORT_Free(cx);
	cx = 0;
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	break;
    }
    return cx;
}

void
DES_DestroyContext(DESContext *cx, PRBool freeit)
{
    if (cx) {
    	memset(cx, 0, sizeof *cx);
	if (freeit)
	    PORT_Free(cx);
    }
}

SECStatus
DES_Encrypt(DESContext *cx, BYTE *out, unsigned int *outLen,
            unsigned int maxOutLen, const BYTE *in, unsigned int inLen)
{

    if (inLen < 0 || (inLen % 8) != 0 || maxOutLen < inLen || !cx || 
        cx->direction != DES_ENCRYPT) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    cx->worker(cx, out, in, inLen);
    if (outLen)
	*outLen = inLen;
    return SECSuccess;
}

SECStatus
DES_Decrypt(DESContext *cx, BYTE *out, unsigned int *outLen,
            unsigned int maxOutLen, const BYTE *in, unsigned int inLen)
{

    if (inLen < 0 || (inLen % 8) != 0 || maxOutLen < inLen || !cx || 
        cx->direction != DES_DECRYPT) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    cx->worker(cx, out, in, inLen);
    if (outLen)
	*outLen = inLen;
    return SECSuccess;
}
