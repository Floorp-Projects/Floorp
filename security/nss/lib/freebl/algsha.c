/*
 * SHA-1 implementation
 *
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
 *
 * $Id: algsha.c,v 1.1 2000/03/31 19:57:21 relyea%netscape.com Exp $
 *
 * Reference implementation of NIST FIPS PUB 180-1.
 *   (Secure Hash Standard, revised version).
 *   Copyright 1994 by Paul C. Kocher.  All rights reserved.
 *   
 * Comments:  This implementation is written to be as close as 
 *   possible to the NIST specification.  No performance or size 
 *   optimization has been attempted.
 *
 * Disclaimer:  This code is provided without warranty of any kind.
 *
 * Revision History:
 *	October 1997 - Nelson Bolyard
 *			Optimized SHA1_Update and SHA1_End 
 *				Loop unrolling
 *				non-iterative size computation.
 */
#include "blapi.h"

#ifdef TRACING_SSL
#include "ssl.h"
#include "ssltrace.h"
#endif

struct SHA1ContextStr {
    uint32 h[5];		/* 5 state variables */
    uint32 w[80];		/* input buffer, plus 64 words */
    int    lenW;		/* bytes of data in input buffer */
    uint32 sizeHi, sizeLo;	/* 64-bit total hashed byte count */
};

#define SHS_ROTL(X,n) (((X) << (n)) | ((X) >> (32-(n))))

static void
shsHashBlock(SHA1Context *ctx)
{
    int t;
    uint32 a, b, c, d, e;
#if OLDWAY
    uint32 temp;
#endif

    for (t = 16; t <= 79; t++)
	ctx->w[t] = SHS_ROTL(
	    ctx->w[t-3] ^ ctx->w[t-8] ^ ctx->w[t-14] ^ ctx->w[t-16], 1);

    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];

#if OLDWAY
    for (t = 0; t <= 19; t++) {
	temp = SHS_ROTL(a,5) + (((c^d)&b)^d) + e + ctx->w[t] + 0x5a827999l;
	e = d;
	d = c;
	c = SHS_ROTL(b, 30);
	b = a;
	a = temp;
    }
#else
#define XFRM1(A,B,C,D,E,T) \
E = SHS_ROTL(A,5) + (((C^D)&B)^D) + E + ctx->w[T] + 0x5a827999L; \
B = SHS_ROTL(B, 30)

    XFRM1(a, b, c, d, e,  0);
    XFRM1(e, a, b, c, d,  1);
    XFRM1(d, e, a, b, c,  2);
    XFRM1(c, d, e, a, b,  3);
    XFRM1(b, c, d, e, a,  4);

    XFRM1(a, b, c, d, e,  5);
    XFRM1(e, a, b, c, d,  6);
    XFRM1(d, e, a, b, c,  7);
    XFRM1(c, d, e, a, b,  8);
    XFRM1(b, c, d, e, a,  9);

    XFRM1(a, b, c, d, e, 10);
    XFRM1(e, a, b, c, d, 11);
    XFRM1(d, e, a, b, c, 12);
    XFRM1(c, d, e, a, b, 13);
    XFRM1(b, c, d, e, a, 14);

    XFRM1(a, b, c, d, e, 15);
    XFRM1(e, a, b, c, d, 16);
    XFRM1(d, e, a, b, c, 17);
    XFRM1(c, d, e, a, b, 18); 
    XFRM1(b, c, d, e, a, 19);  

#endif
#if OLDWAY
    for (t = 20; t <= 39; t++) {
	temp = SHS_ROTL(a,5) + (b^c^d) + e + ctx->w[t] + 0x6ed9eba1l;
	e = d;
	d = c;
	c = SHS_ROTL(b, 30);
	b = a;
	a = temp;
    }
#else
#define XFRM2(A,B,C,D,E,T) \
E = SHS_ROTL(A,5) + (B^C^D) + E + ctx->w[T+20] + 0x6ed9eba1L; \
B = SHS_ROTL(B, 30)

    XFRM2(a, b, c, d, e,  0);
    XFRM2(e, a, b, c, d,  1);
    XFRM2(d, e, a, b, c,  2);
    XFRM2(c, d, e, a, b,  3);
    XFRM2(b, c, d, e, a,  4);

    XFRM2(a, b, c, d, e,  5);
    XFRM2(e, a, b, c, d,  6);
    XFRM2(d, e, a, b, c,  7);
    XFRM2(c, d, e, a, b,  8);
    XFRM2(b, c, d, e, a,  9);

    XFRM2(a, b, c, d, e, 10);
    XFRM2(e, a, b, c, d, 11);
    XFRM2(d, e, a, b, c, 12);
    XFRM2(c, d, e, a, b, 13);
    XFRM2(b, c, d, e, a, 14);

    XFRM2(a, b, c, d, e, 15);
    XFRM2(e, a, b, c, d, 16);
    XFRM2(d, e, a, b, c, 17);
    XFRM2(c, d, e, a, b, 18); 
    XFRM2(b, c, d, e, a, 19);  

#endif
#if OLDWAY
    for (t = 40; t <= 59; t++) {
	temp = SHS_ROTL(a,5) + ((b&c)|(d&(b|c))) + e + ctx->w[t] + 0x8f1bbcdcl;
	e = d;
	d = c;
	c = SHS_ROTL(b, 30);
	b = a;
	a = temp;
    }
#else
#define XFRM3(A,B,C,D,E,T) \
E = SHS_ROTL(A,5) + ((B&C)|(D&(B|C))) + E + ctx->w[T+40] + 0x8f1bbcdcL; \
B = SHS_ROTL(B, 30)

    XFRM3(a, b, c, d, e,  0);
    XFRM3(e, a, b, c, d,  1);
    XFRM3(d, e, a, b, c,  2);
    XFRM3(c, d, e, a, b,  3);
    XFRM3(b, c, d, e, a,  4);

    XFRM3(a, b, c, d, e,  5);
    XFRM3(e, a, b, c, d,  6);
    XFRM3(d, e, a, b, c,  7);
    XFRM3(c, d, e, a, b,  8);
    XFRM3(b, c, d, e, a,  9);

    XFRM3(a, b, c, d, e, 10);
    XFRM3(e, a, b, c, d, 11);
    XFRM3(d, e, a, b, c, 12);
    XFRM3(c, d, e, a, b, 13);
    XFRM3(b, c, d, e, a, 14);

    XFRM3(a, b, c, d, e, 15);
    XFRM3(e, a, b, c, d, 16);
    XFRM3(d, e, a, b, c, 17);
    XFRM3(c, d, e, a, b, 18); 
    XFRM3(b, c, d, e, a, 19);  

#endif
#if OLDWAY
    for (t = 60; t <= 79; t++) {
	temp = SHS_ROTL(a,5) + (b^c^d) + e + ctx->w[t] + 0xca62c1d6l;
	e = d;
	d = c;
	c = SHS_ROTL(b, 30);
	b = a;
	a = temp;
    }
#else
#define XFRM4(A,B,C,D,E,T) \
E = SHS_ROTL(A,5) + (B^C^D) + E + ctx->w[T+60] + 0xca62c1d6L; \
B = SHS_ROTL(B, 30)

    XFRM4(a, b, c, d, e,  0);
    XFRM4(e, a, b, c, d,  1);
    XFRM4(d, e, a, b, c,  2);
    XFRM4(c, d, e, a, b,  3);
    XFRM4(b, c, d, e, a,  4);

    XFRM4(a, b, c, d, e,  5);
    XFRM4(e, a, b, c, d,  6);
    XFRM4(d, e, a, b, c,  7);
    XFRM4(c, d, e, a, b,  8);
    XFRM4(b, c, d, e, a,  9);

    XFRM4(a, b, c, d, e, 10);
    XFRM4(e, a, b, c, d, 11);
    XFRM4(d, e, a, b, c, 12);
    XFRM4(c, d, e, a, b, 13);
    XFRM4(b, c, d, e, a, 14);

    XFRM4(a, b, c, d, e, 15);
    XFRM4(e, a, b, c, d, 16);
    XFRM4(d, e, a, b, c, 17);
    XFRM4(c, d, e, a, b, 18); 
    XFRM4(b, c, d, e, a, 19);  

#endif

    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
}

void
SHA1_Begin(SHA1Context *ctx)
{
    ctx->lenW = 0;
    ctx->sizeHi = ctx->sizeLo = 0;

    /* initialize h with the magic constants (see fips180 for constants)
     */
    ctx->h[0] = 0x67452301l;
    ctx->h[1] = 0xefcdab89l;
    ctx->h[2] = 0x98badcfel;
    ctx->h[3] = 0x10325476l;
    ctx->h[4] = 0xc3d2e1f0l;
}


SHA1Context *
SHA1_NewContext(void)
{
    SHA1Context *cx;

    cx = (SHA1Context*) PORT_ZAlloc(sizeof(SHA1Context));
    return cx;
}

SHA1Context *
SHA1_CloneContext(SHA1Context *cx)
{
    SHA1Context *clone = SHA1_NewContext();
    if (clone)
	*clone = *cx;
    return clone;
}

void
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
    if (freeit) {
        PORT_ZFree(cx, sizeof(SHA1Context));
    }
}

void
SHA1_Update(SHA1Context *ctx, const unsigned char *dataIn, unsigned len)
{
    int          lenW   = ctx->lenW;

    {
	uint32   sizeLo = ctx->sizeLo + len;

	ctx->sizeHi += (sizeLo < ctx->sizeLo);
	ctx->sizeLo  = sizeLo;
    }

    /* Read the data into W and process blocks as they get full */

    if ((lenW % 4) > 0) {
	unsigned toGo = 4 -   (lenW % 4);
	uint32 w      = ctx->w[lenW / 4];

	if (len < toGo) toGo = len;
    	switch (toGo) {
	case 3: w = (w << 8) | (uint32)(*dataIn++); /* FALLTHRU */
	case 2: w = (w << 8) | (uint32)(*dataIn++); /* FALLTHRU */
	case 1: w = (w << 8) | (uint32)(*dataIn++); /* FALLTHRU */
	}
	ctx->w[lenW / 4] = w;
	len  -= toGo;
	lenW += toGo;
	if (lenW >= 64) {
	    shsHashBlock(ctx);
	    lenW = 0;
	}
    }
    /* Here, (lenW % 4) == 0 */

#define STORE_WORD_INC(ndx) \
	ctx->w[ndx] = ((uint32)dataIn[0] << 24) | \
	              ((uint32)dataIn[1] << 16) | \
	              ((uint32)dataIn[2] <<  8) | \
	              ((uint32)dataIn[3]      ); \
	dataIn += 4

    if (len >= (unsigned)(64 - lenW)) {
	len -= 64 - lenW;
	switch (lenW >> 2) {
	case  0: STORE_WORD_INC(0);             /* FALLTHRU */
	case  1: STORE_WORD_INC(1);             /* FALLTHRU */
	case  2: STORE_WORD_INC(2);             /* FALLTHRU */
	case  3: STORE_WORD_INC(3);             /* FALLTHRU */
	case  4: STORE_WORD_INC(4);             /* FALLTHRU */
	case  5: STORE_WORD_INC(5);             /* FALLTHRU */
	case  6: STORE_WORD_INC(6);             /* FALLTHRU */
	case  7: STORE_WORD_INC(7);             /* FALLTHRU */
	case  8: STORE_WORD_INC(8);             /* FALLTHRU */
	case  9: STORE_WORD_INC(9);             /* FALLTHRU */
	case 10: STORE_WORD_INC(10);            /* FALLTHRU */
	case 11: STORE_WORD_INC(11);            /* FALLTHRU */
	case 12: STORE_WORD_INC(12);            /* FALLTHRU */
	case 13: STORE_WORD_INC(13);            /* FALLTHRU */
	case 14: STORE_WORD_INC(14);            /* FALLTHRU */
	case 15: STORE_WORD_INC(15);            /* FALLTHRU */
	}
	shsHashBlock(ctx);
	lenW = 0;

#define STORE_WORD(ndx) \
	ctx->w[ndx] = ((uint32)dataIn[0+4*ndx] << 24) | \
	              ((uint32)dataIn[1+4*ndx] << 16) | \
	              ((uint32)dataIn[2+4*ndx] <<  8) | \
	              ((uint32)dataIn[3+4*ndx]      )

	while (len >= 64) {
	    STORE_WORD(0); 
	    STORE_WORD(1); 
	    STORE_WORD(2); 
	    STORE_WORD(3); 
	    STORE_WORD(4); 
	    STORE_WORD(5); 
	    STORE_WORD(6); 
	    STORE_WORD(7); 
	    STORE_WORD(8); 
	    STORE_WORD(9); 
	    STORE_WORD(10);
	    STORE_WORD(11);
	    STORE_WORD(12);
	    STORE_WORD(13);
	    STORE_WORD(14);
	    STORE_WORD(15); 
	    len    -= 64;
	    dataIn += 64;
	    shsHashBlock(ctx);
	}
    }
    while (len >= 4) {
	STORE_WORD_INC(lenW / 4);
	len    -= 4;
	lenW   += 4;
	if (lenW >= 64) {
	    shsHashBlock(ctx);
	    lenW = 0;
	}
    }
    if (len > 0) {
	uint32 w = 0;

    	switch (len) {
	case 3: w =            (uint32)(*dataIn++); /* FALLTHRU */
	case 2: w = (w << 8) | (uint32)(*dataIn++); /* FALLTHRU */
	case 1: w = (w << 8) | (uint32)(*dataIn++); /* FALLTHRU */
	}
	lenW += len;
	ctx->w[lenW / 4] = w;
    }
    ctx->lenW    = lenW;
}

void
SHA1_End(SHA1Context *ctx, unsigned char *hashout,
	 unsigned int *pDigestLen, unsigned int maxDigestLen)
{
    uint32       sizeLo = ctx->sizeLo;
    uint32       sizeHi = ctx->sizeHi;
    int i;
    unsigned char pad0x80 = 0x80;
    unsigned char padlen[8];

static const unsigned char pad0x00[64] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};		/* constant, all zeros */

    PORT_Assert (maxDigestLen >= SHA1_LENGTH);

    /* Convert size{Hi,Lo} from bytes to bits. */
    i = sizeLo >> 24;
    sizeLo <<= 3;
    sizeHi <<= 3;
    sizeHi |= i;

    padlen[0] = (unsigned char)(sizeHi >> 24);
    padlen[1] = (unsigned char)(sizeHi >> 16);
    padlen[2] = (unsigned char)(sizeHi >>  8);
    padlen[3] = (unsigned char)(sizeHi >>  0);
    padlen[4] = (unsigned char)(sizeLo >> 24);
    padlen[5] = (unsigned char)(sizeLo >> 16);
    padlen[6] = (unsigned char)(sizeLo >>  8);
    padlen[7] = (unsigned char)(sizeLo >>  0);

    /* 
     * Pad with a binary 1 (e.g. 0x80), then zeroes, then length in bits
     */
    SHA1_Update(ctx, &pad0x80, 1);
    if (ctx->lenW != 56)
	SHA1_Update(ctx, pad0x00, (64 + 56 - ctx->lenW) % 64 );
    SHA1_Update(ctx, padlen, 8);

    /* Output hash */
    for (i = 0; i < SHA1_LENGTH/4; i++) {
	uint32 w = ctx->h[i];
	hashout[0] = (unsigned char)(w >> 24);
	hashout[1] = (unsigned char)(w >> 16);
	hashout[2] = (unsigned char)(w >>  8);
	hashout[3] = (unsigned char)(w      );
	hashout += 4;
    }

    *pDigestLen = SHA1_LENGTH;
    SHA1_Begin(ctx);
}

SECStatus
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
    SHA1Context ctx;		/* XXX shouldn't this be allocated? */
    unsigned int outLen;

    SHA1_Begin(&ctx);
    SHA1_Update(&ctx, src, src_length);
    SHA1_End(&ctx, dest, &outLen, SHA1_LENGTH);

    return SECSuccess;
}

SECStatus
SHA1_Hash(unsigned char *dest, const char *src)
{
    return SHA1_HashBuf(dest, (const unsigned char *)src, PORT_Strlen (src));
}

/*
 * need to support save/restore state in pkcs11. Stores all the info necessary
 * for a structure into just a stream of bytes.
 */
unsigned int
SHA1_FlattenSize(SHA1Context *cx)
{
    return sizeof(SHA1Context);
}

SECStatus
SHA1_Flatten(SHA1Context *cx,unsigned char *space)
{
    PORT_Memcpy(space,cx, sizeof(SHA1Context));
    return SECSuccess;
}

SHA1Context *
SHA1_Resurrect(unsigned char *space,void *arg)
{
    SHA1Context *cx = SHA1_NewContext();
    if (cx == NULL) return NULL;

    PORT_Memcpy(cx,space, sizeof(SHA1Context));
    return cx;
}

#ifdef TRACING_SSL
void
SHA1_TraceState(SHA1Context *ctx)
{
    uint32        W;
    int           i;
    int           len;
    int           fixWord = -1;
    int           remainder;	/* bytes in last word */
    unsigned char buf[64 * 4];

    SSL_TRC(99, ("%d: SSL: SHA1 state: %08x %08x %08x %08x %08x", SSL_GETPID(), 
	         ctx->h[0], ctx->h[1], ctx->h[2], ctx->h[3], ctx->h[4]));

    len = (int)(ctx->lenW);
    remainder = len % 4;
    if (remainder) 
    	fixWord = len - remainder;
    for (i = 0; i < len; i++) {
	if (0 == (i % 4)) {
	    W = ctx->w[i / 4];
	    if (i == fixWord) {
	        W <<= 8 * (4 - remainder);
	    }
	}
	buf[i] = (unsigned char)(W >> 24);
	W <<= 8;
    }

    PRINT_BUF(99, (0, "SHA1_TraceState: buffered input", buf, len));

}
#endif
