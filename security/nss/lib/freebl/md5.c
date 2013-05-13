/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "prlong.h"

#include "blapi.h"

#define MD5_HASH_LEN 16
#define MD5_BUFFER_SIZE 64
#define MD5_END_BUFFER (MD5_BUFFER_SIZE - 8)

#define CV0_1 0x67452301
#define CV0_2 0xefcdab89
#define CV0_3 0x98badcfe
#define CV0_4 0x10325476

#define T1_0  0xd76aa478
#define T1_1  0xe8c7b756
#define T1_2  0x242070db
#define T1_3  0xc1bdceee
#define T1_4  0xf57c0faf
#define T1_5  0x4787c62a
#define T1_6  0xa8304613
#define T1_7  0xfd469501
#define T1_8  0x698098d8
#define T1_9  0x8b44f7af
#define T1_10 0xffff5bb1
#define T1_11 0x895cd7be
#define T1_12 0x6b901122
#define T1_13 0xfd987193
#define T1_14 0xa679438e
#define T1_15 0x49b40821

#define T2_0  0xf61e2562
#define T2_1  0xc040b340
#define T2_2  0x265e5a51
#define T2_3  0xe9b6c7aa
#define T2_4  0xd62f105d
#define T2_5  0x02441453
#define T2_6  0xd8a1e681
#define T2_7  0xe7d3fbc8
#define T2_8  0x21e1cde6
#define T2_9  0xc33707d6
#define T2_10 0xf4d50d87
#define T2_11 0x455a14ed
#define T2_12 0xa9e3e905
#define T2_13 0xfcefa3f8
#define T2_14 0x676f02d9
#define T2_15 0x8d2a4c8a

#define T3_0  0xfffa3942
#define T3_1  0x8771f681
#define T3_2  0x6d9d6122
#define T3_3  0xfde5380c
#define T3_4  0xa4beea44
#define T3_5  0x4bdecfa9
#define T3_6  0xf6bb4b60
#define T3_7  0xbebfbc70
#define T3_8  0x289b7ec6
#define T3_9  0xeaa127fa
#define T3_10 0xd4ef3085
#define T3_11 0x04881d05
#define T3_12 0xd9d4d039
#define T3_13 0xe6db99e5
#define T3_14 0x1fa27cf8
#define T3_15 0xc4ac5665

#define T4_0  0xf4292244
#define T4_1  0x432aff97
#define T4_2  0xab9423a7
#define T4_3  0xfc93a039
#define T4_4  0x655b59c3
#define T4_5  0x8f0ccc92
#define T4_6  0xffeff47d
#define T4_7  0x85845dd1
#define T4_8  0x6fa87e4f
#define T4_9  0xfe2ce6e0
#define T4_10 0xa3014314
#define T4_11 0x4e0811a1
#define T4_12 0xf7537e82
#define T4_13 0xbd3af235
#define T4_14 0x2ad7d2bb
#define T4_15 0xeb86d391

#define R1B0  0
#define R1B1  1
#define R1B2  2
#define R1B3  3
#define R1B4  4
#define R1B5  5
#define R1B6  6
#define R1B7  7
#define R1B8  8
#define R1B9  9
#define R1B10 10
#define R1B11 11
#define R1B12 12
#define R1B13 13
#define R1B14 14
#define R1B15 15

#define R2B0  1
#define R2B1  6
#define R2B2  11
#define R2B3  0
#define R2B4  5
#define R2B5  10
#define R2B6  15
#define R2B7  4
#define R2B8  9
#define R2B9  14
#define R2B10 3 
#define R2B11 8 
#define R2B12 13
#define R2B13 2 
#define R2B14 7 
#define R2B15 12

#define R3B0  5
#define R3B1  8
#define R3B2  11
#define R3B3  14
#define R3B4  1
#define R3B5  4
#define R3B6  7
#define R3B7  10
#define R3B8  13
#define R3B9  0
#define R3B10 3 
#define R3B11 6 
#define R3B12 9 
#define R3B13 12
#define R3B14 15
#define R3B15 2 

#define R4B0  0
#define R4B1  7
#define R4B2  14
#define R4B3  5
#define R4B4  12
#define R4B5  3
#define R4B6  10
#define R4B7  1
#define R4B8  8
#define R4B9  15
#define R4B10 6 
#define R4B11 13
#define R4B12 4 
#define R4B13 11
#define R4B14 2 
#define R4B15 9 

#define S1_0 7
#define S1_1 12
#define S1_2 17
#define S1_3 22

#define S2_0 5
#define S2_1 9
#define S2_2 14
#define S2_3 20

#define S3_0 4
#define S3_1 11
#define S3_2 16
#define S3_3 23

#define S4_0 6
#define S4_1 10
#define S4_2 15
#define S4_3 21

struct MD5ContextStr {
	PRUint32      lsbInput;
	PRUint32      msbInput;
	PRUint32      cv[4];
	union {
		PRUint8 b[64];
		PRUint32 w[16];
	} u;
};

#define inBuf u.b

SECStatus 
MD5_Hash(unsigned char *dest, const char *src)
{
	return MD5_HashBuf(dest, (const unsigned char *)src, PORT_Strlen(src));
}

SECStatus 
MD5_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
	unsigned int len;
	MD5Context cx;

	MD5_Begin(&cx);
	MD5_Update(&cx, src, src_length);
	MD5_End(&cx, dest, &len, MD5_HASH_LEN);
	memset(&cx, 0, sizeof cx);
	return SECSuccess;
}

MD5Context *
MD5_NewContext(void)
{
	/* no need to ZAlloc, MD5_Begin will init the context */
	MD5Context *cx = (MD5Context *)PORT_Alloc(sizeof(MD5Context));
	if (cx == NULL) {
		PORT_SetError(PR_OUT_OF_MEMORY_ERROR);
		return NULL;
	}
	return cx;
}

void 
MD5_DestroyContext(MD5Context *cx, PRBool freeit)
{
	memset(cx, 0, sizeof *cx);
	if (freeit) {
	    PORT_Free(cx);
	}
}

void 
MD5_Begin(MD5Context *cx)
{
	cx->lsbInput = 0;
	cx->msbInput = 0;
/*	memset(cx->inBuf, 0, sizeof(cx->inBuf)); */
	cx->cv[0] = CV0_1;
	cx->cv[1] = CV0_2;
	cx->cv[2] = CV0_3;
	cx->cv[3] = CV0_4;
}

#define cls(i32, s) (tmp = i32, tmp << s | tmp >> (32 - s))

#if defined(SOLARIS) || defined(HPUX)
#define addto64(sumhigh, sumlow, addend) \
	sumlow += addend; sumhigh += (sumlow < addend);
#else
#define addto64(sumhigh, sumlow, addend) \
	sumlow += addend; if (sumlow < addend) ++sumhigh;
#endif

#define MASK 0x00ff00ff
#ifdef IS_LITTLE_ENDIAN
#define lendian(i32) \
	(i32)
#else
#define lendian(i32) \
	(tmp = i32 >> 16 | i32 << 16, (tmp & MASK) << 8 | tmp >> 8 & MASK)
#endif

#ifndef IS_LITTLE_ENDIAN

#define lebytes(b4) \
	((b4)[3] << 24 | (b4)[2] << 16 | (b4)[1] << 8 | (b4)[0])

static void
md5_prep_state_le(MD5Context *cx)
{
	PRUint32 tmp;
	cx->u.w[0] = lendian(cx->u.w[0]);
	cx->u.w[1] = lendian(cx->u.w[1]);
	cx->u.w[2] = lendian(cx->u.w[2]);
	cx->u.w[3] = lendian(cx->u.w[3]);
	cx->u.w[4] = lendian(cx->u.w[4]);
	cx->u.w[5] = lendian(cx->u.w[5]);
	cx->u.w[6] = lendian(cx->u.w[6]);
	cx->u.w[7] = lendian(cx->u.w[7]);
	cx->u.w[8] = lendian(cx->u.w[8]);
	cx->u.w[9] = lendian(cx->u.w[9]);
	cx->u.w[10] = lendian(cx->u.w[10]);
	cx->u.w[11] = lendian(cx->u.w[11]);
	cx->u.w[12] = lendian(cx->u.w[12]);
	cx->u.w[13] = lendian(cx->u.w[13]);
	cx->u.w[14] = lendian(cx->u.w[14]);
	cx->u.w[15] = lendian(cx->u.w[15]);
}

static void
md5_prep_buffer_le(MD5Context *cx, const PRUint8 *beBuf)
{
	cx->u.w[0] = lebytes(&beBuf[0]);
	cx->u.w[1] = lebytes(&beBuf[4]);
	cx->u.w[2] = lebytes(&beBuf[8]);
	cx->u.w[3] = lebytes(&beBuf[12]);
	cx->u.w[4] = lebytes(&beBuf[16]);
	cx->u.w[5] = lebytes(&beBuf[20]);
	cx->u.w[6] = lebytes(&beBuf[24]);
	cx->u.w[7] = lebytes(&beBuf[28]);
	cx->u.w[8] = lebytes(&beBuf[32]);
	cx->u.w[9] = lebytes(&beBuf[36]);
	cx->u.w[10] = lebytes(&beBuf[40]);
	cx->u.w[11] = lebytes(&beBuf[44]);
	cx->u.w[12] = lebytes(&beBuf[48]);
	cx->u.w[13] = lebytes(&beBuf[52]);
	cx->u.w[14] = lebytes(&beBuf[56]);
	cx->u.w[15] = lebytes(&beBuf[60]);
}
#endif


#define F(X, Y, Z) \
	((X & Y) | ((~X) & Z))

#define G(X, Y, Z) \
	((X & Z) | (Y & (~Z)))

#define H(X, Y, Z) \
	(X ^ Y ^ Z)

#define I(X, Y, Z) \
	(Y ^ (X | (~Z)))

#define FF(a, b, c, d, bufint, s, ti) \
	a = b + cls(a + F(b, c, d) + bufint + ti, s)

#define GG(a, b, c, d, bufint, s, ti) \
	a = b + cls(a + G(b, c, d) + bufint + ti, s)

#define HH(a, b, c, d, bufint, s, ti) \
	a = b + cls(a + H(b, c, d) + bufint + ti, s)

#define II(a, b, c, d, bufint, s, ti) \
	a = b + cls(a + I(b, c, d) + bufint + ti, s)

static void
md5_compress(MD5Context *cx, const PRUint32 *wBuf)
{
	PRUint32 a, b, c, d;
	PRUint32 tmp;
	a = cx->cv[0];
	b = cx->cv[1];
	c = cx->cv[2];
	d = cx->cv[3];
	FF(a, b, c, d, wBuf[R1B0 ], S1_0, T1_0);
	FF(d, a, b, c, wBuf[R1B1 ], S1_1, T1_1);
	FF(c, d, a, b, wBuf[R1B2 ], S1_2, T1_2);
	FF(b, c, d, a, wBuf[R1B3 ], S1_3, T1_3);
	FF(a, b, c, d, wBuf[R1B4 ], S1_0, T1_4);
	FF(d, a, b, c, wBuf[R1B5 ], S1_1, T1_5);
	FF(c, d, a, b, wBuf[R1B6 ], S1_2, T1_6);
	FF(b, c, d, a, wBuf[R1B7 ], S1_3, T1_7);
	FF(a, b, c, d, wBuf[R1B8 ], S1_0, T1_8);
	FF(d, a, b, c, wBuf[R1B9 ], S1_1, T1_9);
	FF(c, d, a, b, wBuf[R1B10], S1_2, T1_10);
	FF(b, c, d, a, wBuf[R1B11], S1_3, T1_11);
	FF(a, b, c, d, wBuf[R1B12], S1_0, T1_12);
	FF(d, a, b, c, wBuf[R1B13], S1_1, T1_13);
	FF(c, d, a, b, wBuf[R1B14], S1_2, T1_14);
	FF(b, c, d, a, wBuf[R1B15], S1_3, T1_15);
	GG(a, b, c, d, wBuf[R2B0 ], S2_0, T2_0);
	GG(d, a, b, c, wBuf[R2B1 ], S2_1, T2_1);
	GG(c, d, a, b, wBuf[R2B2 ], S2_2, T2_2);
	GG(b, c, d, a, wBuf[R2B3 ], S2_3, T2_3);
	GG(a, b, c, d, wBuf[R2B4 ], S2_0, T2_4);
	GG(d, a, b, c, wBuf[R2B5 ], S2_1, T2_5);
	GG(c, d, a, b, wBuf[R2B6 ], S2_2, T2_6);
	GG(b, c, d, a, wBuf[R2B7 ], S2_3, T2_7);
	GG(a, b, c, d, wBuf[R2B8 ], S2_0, T2_8);
	GG(d, a, b, c, wBuf[R2B9 ], S2_1, T2_9);
	GG(c, d, a, b, wBuf[R2B10], S2_2, T2_10);
	GG(b, c, d, a, wBuf[R2B11], S2_3, T2_11);
	GG(a, b, c, d, wBuf[R2B12], S2_0, T2_12);
	GG(d, a, b, c, wBuf[R2B13], S2_1, T2_13);
	GG(c, d, a, b, wBuf[R2B14], S2_2, T2_14);
	GG(b, c, d, a, wBuf[R2B15], S2_3, T2_15);
	HH(a, b, c, d, wBuf[R3B0 ], S3_0, T3_0);
	HH(d, a, b, c, wBuf[R3B1 ], S3_1, T3_1);
	HH(c, d, a, b, wBuf[R3B2 ], S3_2, T3_2);
	HH(b, c, d, a, wBuf[R3B3 ], S3_3, T3_3);
	HH(a, b, c, d, wBuf[R3B4 ], S3_0, T3_4);
	HH(d, a, b, c, wBuf[R3B5 ], S3_1, T3_5);
	HH(c, d, a, b, wBuf[R3B6 ], S3_2, T3_6);
	HH(b, c, d, a, wBuf[R3B7 ], S3_3, T3_7);
	HH(a, b, c, d, wBuf[R3B8 ], S3_0, T3_8);
	HH(d, a, b, c, wBuf[R3B9 ], S3_1, T3_9);
	HH(c, d, a, b, wBuf[R3B10], S3_2, T3_10);
	HH(b, c, d, a, wBuf[R3B11], S3_3, T3_11);
	HH(a, b, c, d, wBuf[R3B12], S3_0, T3_12);
	HH(d, a, b, c, wBuf[R3B13], S3_1, T3_13);
	HH(c, d, a, b, wBuf[R3B14], S3_2, T3_14);
	HH(b, c, d, a, wBuf[R3B15], S3_3, T3_15);
	II(a, b, c, d, wBuf[R4B0 ], S4_0, T4_0);
	II(d, a, b, c, wBuf[R4B1 ], S4_1, T4_1);
	II(c, d, a, b, wBuf[R4B2 ], S4_2, T4_2);
	II(b, c, d, a, wBuf[R4B3 ], S4_3, T4_3);
	II(a, b, c, d, wBuf[R4B4 ], S4_0, T4_4);
	II(d, a, b, c, wBuf[R4B5 ], S4_1, T4_5);
	II(c, d, a, b, wBuf[R4B6 ], S4_2, T4_6);
	II(b, c, d, a, wBuf[R4B7 ], S4_3, T4_7);
	II(a, b, c, d, wBuf[R4B8 ], S4_0, T4_8);
	II(d, a, b, c, wBuf[R4B9 ], S4_1, T4_9);
	II(c, d, a, b, wBuf[R4B10], S4_2, T4_10);
	II(b, c, d, a, wBuf[R4B11], S4_3, T4_11);
	II(a, b, c, d, wBuf[R4B12], S4_0, T4_12);
	II(d, a, b, c, wBuf[R4B13], S4_1, T4_13);
	II(c, d, a, b, wBuf[R4B14], S4_2, T4_14);
	II(b, c, d, a, wBuf[R4B15], S4_3, T4_15);
	cx->cv[0] += a;
	cx->cv[1] += b;
	cx->cv[2] += c;
	cx->cv[3] += d;
}

void 
MD5_Update(MD5Context *cx, const unsigned char *input, unsigned int inputLen)
{
	PRUint32 bytesToConsume;
	PRUint32 inBufIndex = cx->lsbInput & 63;
	const PRUint32 *wBuf;

	/* Add the number of input bytes to the 64-bit input counter. */
	addto64(cx->msbInput, cx->lsbInput, inputLen);
	if (inBufIndex) {
		/* There is already data in the buffer.  Fill with input. */
		bytesToConsume = PR_MIN(inputLen, MD5_BUFFER_SIZE - inBufIndex);
		memcpy(&cx->inBuf[inBufIndex], input, bytesToConsume);
		if (inBufIndex + bytesToConsume >= MD5_BUFFER_SIZE) {
			/* The buffer is filled.  Run the compression function. */
#ifndef IS_LITTLE_ENDIAN
			md5_prep_state_le(cx);
#endif
			md5_compress(cx, cx->u.w);
		}
		/* Remaining input. */
		inputLen -= bytesToConsume;
		input += bytesToConsume;
	}

	/* Iterate over 64-byte chunks of the message. */
	while (inputLen >= MD5_BUFFER_SIZE) {
#ifdef IS_LITTLE_ENDIAN
#ifdef NSS_X86_OR_X64
		/* x86 can handle arithmetic on non-word-aligned buffers */
		wBuf = (PRUint32 *)input;
#else
		if ((ptrdiff_t)input & 0x3) {
			/* buffer not aligned, copy it to force alignment */
			memcpy(cx->inBuf, input, MD5_BUFFER_SIZE);
			wBuf = cx->u.w;
		} else {
			/* buffer is aligned */
			wBuf = (PRUint32 *)input;
		}
#endif
#else
		md5_prep_buffer_le(cx, input);
		wBuf = cx->u.w;
#endif
		md5_compress(cx, wBuf);
		inputLen -= MD5_BUFFER_SIZE;
		input += MD5_BUFFER_SIZE;
	}

	/* Tail of message (message bytes mod 64). */
	if (inputLen)
		memcpy(cx->inBuf, input, inputLen);
}

static const unsigned char padbytes[] = {
	0x80, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
	0x00, 0x00, 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

void 
MD5_End(MD5Context *cx, unsigned char *digest,
        unsigned int *digestLen, unsigned int maxDigestLen)
{
#ifndef IS_LITTLE_ENDIAN
	PRUint32 tmp;
#endif
	PRUint32 lowInput, highInput;
	PRUint32 inBufIndex = cx->lsbInput & 63;

	if (maxDigestLen < MD5_HASH_LEN) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return;
	}

	/* Copy out the length of bits input before padding. */
	lowInput = cx->lsbInput; 
	highInput = (cx->msbInput << 3) | (lowInput >> 29);
	lowInput <<= 3;

	if (inBufIndex < MD5_END_BUFFER) {
		MD5_Update(cx, padbytes, MD5_END_BUFFER - inBufIndex);
	} else {
		MD5_Update(cx, padbytes, 
		           MD5_END_BUFFER + MD5_BUFFER_SIZE - inBufIndex);
	}

	/* Store the number of bytes input (before padding) in final 64 bits. */
	cx->u.w[14] = lendian(lowInput);
	cx->u.w[15] = lendian(highInput);

	/* Final call to compress. */
#ifndef IS_LITTLE_ENDIAN
	md5_prep_state_le(cx);
#endif
	md5_compress(cx, cx->u.w);

	/* Copy the resulting values out of the chain variables into return buf. */
	if (digestLen)
		*digestLen = MD5_HASH_LEN;
#ifndef IS_LITTLE_ENDIAN
	cx->cv[0] = lendian(cx->cv[0]);
	cx->cv[1] = lendian(cx->cv[1]);
	cx->cv[2] = lendian(cx->cv[2]);
	cx->cv[3] = lendian(cx->cv[3]);
#endif
	memcpy(digest, cx->cv, MD5_HASH_LEN);
}

void
MD5_EndRaw(MD5Context *cx, unsigned char *digest,
           unsigned int *digestLen, unsigned int maxDigestLen)
{
#ifndef IS_LITTLE_ENDIAN
	PRUint32 tmp;
#endif
	PRUint32 cv[4];

	if (maxDigestLen < MD5_HASH_LEN) {
		PORT_SetError(SEC_ERROR_INVALID_ARGS);
		return;
	}

	memcpy(cv, cx->cv, sizeof(cv));
#ifndef IS_LITTLE_ENDIAN
	cv[0] = lendian(cv[0]);
	cv[1] = lendian(cv[1]);
	cv[2] = lendian(cv[2]);
	cv[3] = lendian(cv[3]);
#endif
	memcpy(digest, cv, MD5_HASH_LEN);
	if (digestLen)
		*digestLen = MD5_HASH_LEN;
}

unsigned int 
MD5_FlattenSize(MD5Context *cx)
{
	return sizeof(*cx);
}

SECStatus 
MD5_Flatten(MD5Context *cx, unsigned char *space)
{
	memcpy(space, cx, sizeof(*cx));
	return SECSuccess;
}

MD5Context * 
MD5_Resurrect(unsigned char *space, void *arg)
{
	MD5Context *cx = MD5_NewContext();
	if (cx)
		memcpy(cx, space, sizeof(*cx));
	return cx;
}

void MD5_Clone(MD5Context *dest, MD5Context *src) 
{
	memcpy(dest, src, sizeof *dest);
}

void 
MD5_TraceState(MD5Context *cx)
{
	PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
}
