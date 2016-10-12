/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include <memory.h>
#include "blapi.h"
#include "sha_fast.h"
#include "prerror.h"

#ifdef TRACING_SSL
#include "ssl.h"
#include "ssltrace.h"
#endif

static void shaCompress(volatile SHA_HW_t *X, const PRUint32 *datain);

#define W u.w
#define B u.b

#define SHA_F1(X, Y, Z) ((((Y) ^ (Z)) & (X)) ^ (Z))
#define SHA_F2(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define SHA_F3(X, Y, Z) (((X) & (Y)) | ((Z) & ((X) | (Y))))
#define SHA_F4(X, Y, Z) ((X) ^ (Y) ^ (Z))

#define SHA_MIX(n, a, b, c) XW(n) = SHA_ROTL(XW(a) ^ XW(b) ^ XW(c) ^ XW(n), 1)

/*
 *  SHA: initialize context
 */
void
SHA1_Begin(SHA1Context *ctx)
{
    ctx->size = 0;
    /*
   *  Initialize H with constants from FIPS180-1.
   */
    ctx->H[0] = 0x67452301L;
    ctx->H[1] = 0xefcdab89L;
    ctx->H[2] = 0x98badcfeL;
    ctx->H[3] = 0x10325476L;
    ctx->H[4] = 0xc3d2e1f0L;
}

/* Explanation of H array and index values:
 * The context's H array is actually the concatenation of two arrays
 * defined by SHA1, the H array of state variables (5 elements),
 * and the W array of intermediate values, of which there are 16 elements.
 * The W array starts at H[5], that is W[0] is H[5].
 * Although these values are defined as 32-bit values, we use 64-bit
 * variables to hold them because the AMD64 stores 64 bit values in
 * memory MUCH faster than it stores any smaller values.
 *
 * Rather than passing the context structure to shaCompress, we pass
 * this combined array of H and W values.  We do not pass the address
 * of the first element of this array, but rather pass the address of an
 * element in the middle of the array, element X.  Presently X[0] is H[11].
 * So we pass the address of H[11] as the address of array X to shaCompress.
 * Then shaCompress accesses the members of the array using positive AND
 * negative indexes.
 *
 * Pictorially: (each element is 8 bytes)
 * H | H0 H1 H2 H3 H4 W0 W1 W2 W3 W4 W5 W6 W7 W8 W9 Wa Wb Wc Wd We Wf |
 * X |-11-10 -9 -8 -7 -6 -5 -4 -3 -2 -1 X0 X1 X2 X3 X4 X5 X6 X7 X8 X9 |
 *
 * The byte offset from X[0] to any member of H and W is always
 * representable in a signed 8-bit value, which will be encoded
 * as a single byte offset in the X86-64 instruction set.
 * If we didn't pass the address of H[11], and instead passed the
 * address of H[0], the offsets to elements H[16] and above would be
 * greater than 127, not representable in a signed 8-bit value, and the
 * x86-64 instruction set would encode every such offset as a 32-bit
 * signed number in each instruction that accessed element H[16] or
 * higher.  This results in much bigger and slower code.
 */
#if !defined(SHA_PUT_W_IN_STACK)
#define H2X 11 /* X[0] is H[11], and H[0] is X[-11] */
#define W2X 6  /* X[0] is W[6],  and W[0] is X[-6]  */
#else
#define H2X 0
#endif

/*
 *  SHA: Add data to context.
 */
void
SHA1_Update(SHA1Context *ctx, const unsigned char *dataIn, unsigned int len)
{
    register unsigned int lenB;
    register unsigned int togo;

    if (!len)
        return;

    /* accumulate the byte count. */
    lenB = (unsigned int)(ctx->size) & 63U;

    ctx->size += len;

    /*
   *  Read the data into W and process blocks as they get full
   */
    if (lenB > 0) {
        togo = 64U - lenB;
        if (len < togo)
            togo = len;
        memcpy(ctx->B + lenB, dataIn, togo);
        len -= togo;
        dataIn += togo;
        lenB = (lenB + togo) & 63U;
        if (!lenB) {
            shaCompress(&ctx->H[H2X], ctx->W);
        }
    }
#if !defined(HAVE_UNALIGNED_ACCESS)
    if ((ptrdiff_t)dataIn % sizeof(PRUint32)) {
        while (len >= 64U) {
            memcpy(ctx->B, dataIn, 64);
            len -= 64U;
            shaCompress(&ctx->H[H2X], ctx->W);
            dataIn += 64U;
        }
    } else
#endif
    {
        while (len >= 64U) {
            len -= 64U;
            shaCompress(&ctx->H[H2X], (PRUint32 *)dataIn);
            dataIn += 64U;
        }
    }
    if (len) {
        memcpy(ctx->B, dataIn, len);
    }
}

/*
 *  SHA: Generate hash value from context
 */
void
SHA1_End(SHA1Context *ctx, unsigned char *hashout,
         unsigned int *pDigestLen, unsigned int maxDigestLen)
{
    register PRUint64 size;
    register PRUint32 lenB;

    static const unsigned char bulk_pad[64] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#define tmp lenB

    PORT_Assert(maxDigestLen >= SHA1_LENGTH);

    /*
   *  Pad with a binary 1 (e.g. 0x80), then zeroes, then length in bits
   */
    size = ctx->size;

    lenB = (PRUint32)size & 63;
    SHA1_Update(ctx, bulk_pad, (((55 + 64) - lenB) & 63) + 1);
    PORT_Assert(((PRUint32)ctx->size & 63) == 56);
    /* Convert size from bytes to bits. */
    size <<= 3;
    ctx->W[14] = SHA_HTONL((PRUint32)(size >> 32));
    ctx->W[15] = SHA_HTONL((PRUint32)size);
    shaCompress(&ctx->H[H2X], ctx->W);

    /*
     *  Output hash
     */
    SHA_STORE_RESULT;
    if (pDigestLen) {
        *pDigestLen = SHA1_LENGTH;
    }
#undef tmp
}

void
SHA1_EndRaw(SHA1Context *ctx, unsigned char *hashout,
            unsigned int *pDigestLen, unsigned int maxDigestLen)
{
#if defined(SHA_NEED_TMP_VARIABLE)
    register PRUint32 tmp;
#endif
    PORT_Assert(maxDigestLen >= SHA1_LENGTH);

    SHA_STORE_RESULT;
    if (pDigestLen)
        *pDigestLen = SHA1_LENGTH;
}

#undef B
/*
 *  SHA: Compression function, unrolled.
 *
 * Some operations in shaCompress are done as 5 groups of 16 operations.
 * Others are done as 4 groups of 20 operations.
 * The code below shows that structure.
 *
 * The functions that compute the new values of the 5 state variables
 * A-E are done in 4 groups of 20 operations (or you may also think
 * of them as being done in 16 groups of 5 operations).  They are
 * done by the SHA_RNDx macros below, in the right column.
 *
 * The functions that set the 16 values of the W array are done in
 * 5 groups of 16 operations.  The first group is done by the
 * LOAD macros below, the latter 4 groups are done by SHA_MIX below,
 * in the left column.
 *
 * gcc's optimizer observes that each member of the W array is assigned
 * a value 5 times in this code.  It reduces the number of store
 * operations done to the W array in the context (that is, in the X array)
 * by creating a W array on the stack, and storing the W values there for
 * the first 4 groups of operations on W, and storing the values in the
 * context's W array only in the fifth group.  This is undesirable.
 * It is MUCH bigger code than simply using the context's W array, because
 * all the offsets to the W array in the stack are 32-bit signed offsets,
 * and it is no faster than storing the values in the context's W array.
 *
 * The original code for sha_fast.c prevented this creation of a separate
 * W array in the stack by creating a W array of 80 members, each of
 * whose elements is assigned only once. It also separated the computations
 * of the W array values and the computations of the values for the 5
 * state variables into two separate passes, W's, then A-E's so that the
 * second pass could be done all in registers (except for accessing the W
 * array) on machines with fewer registers.  The method is suboptimal
 * for machines with enough registers to do it all in one pass, and it
 * necessitates using many instructions with 32-bit offsets.
 *
 * This code eliminates the separate W array on the stack by a completely
 * different means: by declaring the X array volatile.  This prevents
 * the optimizer from trying to reduce the use of the X array by the
 * creation of a MORE expensive W array on the stack. The result is
 * that all instructions use signed 8-bit offsets and not 32-bit offsets.
 *
 * The combination of this code and the -O3 optimizer flag on GCC 3.4.3
 * results in code that is 3 times faster than the previous NSS sha_fast
 * code on AMD64.
 */
static void NO_SANITIZE_ALIGNMENT
shaCompress(volatile SHA_HW_t *X, const PRUint32 *inbuf)
{
    register SHA_HW_t A, B, C, D, E;

#if defined(SHA_NEED_TMP_VARIABLE)
    register PRUint32 tmp;
#endif

#if !defined(SHA_PUT_W_IN_STACK)
#define XH(n) X[n - H2X]
#define XW(n) X[n - W2X]
#else
    SHA_HW_t w_0, w_1, w_2, w_3, w_4, w_5, w_6, w_7,
        w_8, w_9, w_10, w_11, w_12, w_13, w_14, w_15;
#define XW(n) w_##n
#define XH(n) X[n]
#endif

#define K0 0x5a827999L
#define K1 0x6ed9eba1L
#define K2 0x8f1bbcdcL
#define K3 0xca62c1d6L

#define SHA_RND1(a, b, c, d, e, n)                         \
    a = SHA_ROTL(b, 5) + SHA_F1(c, d, e) + a + XW(n) + K0; \
    c = SHA_ROTL(c, 30)
#define SHA_RND2(a, b, c, d, e, n)                         \
    a = SHA_ROTL(b, 5) + SHA_F2(c, d, e) + a + XW(n) + K1; \
    c = SHA_ROTL(c, 30)
#define SHA_RND3(a, b, c, d, e, n)                         \
    a = SHA_ROTL(b, 5) + SHA_F3(c, d, e) + a + XW(n) + K2; \
    c = SHA_ROTL(c, 30)
#define SHA_RND4(a, b, c, d, e, n)                         \
    a = SHA_ROTL(b, 5) + SHA_F4(c, d, e) + a + XW(n) + K3; \
    c = SHA_ROTL(c, 30)

#define LOAD(n) XW(n) = SHA_HTONL(inbuf[n])

    A = XH(0);
    B = XH(1);
    C = XH(2);
    D = XH(3);
    E = XH(4);

    LOAD(0);
    SHA_RND1(E, A, B, C, D, 0);
    LOAD(1);
    SHA_RND1(D, E, A, B, C, 1);
    LOAD(2);
    SHA_RND1(C, D, E, A, B, 2);
    LOAD(3);
    SHA_RND1(B, C, D, E, A, 3);
    LOAD(4);
    SHA_RND1(A, B, C, D, E, 4);
    LOAD(5);
    SHA_RND1(E, A, B, C, D, 5);
    LOAD(6);
    SHA_RND1(D, E, A, B, C, 6);
    LOAD(7);
    SHA_RND1(C, D, E, A, B, 7);
    LOAD(8);
    SHA_RND1(B, C, D, E, A, 8);
    LOAD(9);
    SHA_RND1(A, B, C, D, E, 9);
    LOAD(10);
    SHA_RND1(E, A, B, C, D, 10);
    LOAD(11);
    SHA_RND1(D, E, A, B, C, 11);
    LOAD(12);
    SHA_RND1(C, D, E, A, B, 12);
    LOAD(13);
    SHA_RND1(B, C, D, E, A, 13);
    LOAD(14);
    SHA_RND1(A, B, C, D, E, 14);
    LOAD(15);
    SHA_RND1(E, A, B, C, D, 15);

    SHA_MIX(0, 13, 8, 2);
    SHA_RND1(D, E, A, B, C, 0);
    SHA_MIX(1, 14, 9, 3);
    SHA_RND1(C, D, E, A, B, 1);
    SHA_MIX(2, 15, 10, 4);
    SHA_RND1(B, C, D, E, A, 2);
    SHA_MIX(3, 0, 11, 5);
    SHA_RND1(A, B, C, D, E, 3);

    SHA_MIX(4, 1, 12, 6);
    SHA_RND2(E, A, B, C, D, 4);
    SHA_MIX(5, 2, 13, 7);
    SHA_RND2(D, E, A, B, C, 5);
    SHA_MIX(6, 3, 14, 8);
    SHA_RND2(C, D, E, A, B, 6);
    SHA_MIX(7, 4, 15, 9);
    SHA_RND2(B, C, D, E, A, 7);
    SHA_MIX(8, 5, 0, 10);
    SHA_RND2(A, B, C, D, E, 8);
    SHA_MIX(9, 6, 1, 11);
    SHA_RND2(E, A, B, C, D, 9);
    SHA_MIX(10, 7, 2, 12);
    SHA_RND2(D, E, A, B, C, 10);
    SHA_MIX(11, 8, 3, 13);
    SHA_RND2(C, D, E, A, B, 11);
    SHA_MIX(12, 9, 4, 14);
    SHA_RND2(B, C, D, E, A, 12);
    SHA_MIX(13, 10, 5, 15);
    SHA_RND2(A, B, C, D, E, 13);
    SHA_MIX(14, 11, 6, 0);
    SHA_RND2(E, A, B, C, D, 14);
    SHA_MIX(15, 12, 7, 1);
    SHA_RND2(D, E, A, B, C, 15);

    SHA_MIX(0, 13, 8, 2);
    SHA_RND2(C, D, E, A, B, 0);
    SHA_MIX(1, 14, 9, 3);
    SHA_RND2(B, C, D, E, A, 1);
    SHA_MIX(2, 15, 10, 4);
    SHA_RND2(A, B, C, D, E, 2);
    SHA_MIX(3, 0, 11, 5);
    SHA_RND2(E, A, B, C, D, 3);
    SHA_MIX(4, 1, 12, 6);
    SHA_RND2(D, E, A, B, C, 4);
    SHA_MIX(5, 2, 13, 7);
    SHA_RND2(C, D, E, A, B, 5);
    SHA_MIX(6, 3, 14, 8);
    SHA_RND2(B, C, D, E, A, 6);
    SHA_MIX(7, 4, 15, 9);
    SHA_RND2(A, B, C, D, E, 7);

    SHA_MIX(8, 5, 0, 10);
    SHA_RND3(E, A, B, C, D, 8);
    SHA_MIX(9, 6, 1, 11);
    SHA_RND3(D, E, A, B, C, 9);
    SHA_MIX(10, 7, 2, 12);
    SHA_RND3(C, D, E, A, B, 10);
    SHA_MIX(11, 8, 3, 13);
    SHA_RND3(B, C, D, E, A, 11);
    SHA_MIX(12, 9, 4, 14);
    SHA_RND3(A, B, C, D, E, 12);
    SHA_MIX(13, 10, 5, 15);
    SHA_RND3(E, A, B, C, D, 13);
    SHA_MIX(14, 11, 6, 0);
    SHA_RND3(D, E, A, B, C, 14);
    SHA_MIX(15, 12, 7, 1);
    SHA_RND3(C, D, E, A, B, 15);

    SHA_MIX(0, 13, 8, 2);
    SHA_RND3(B, C, D, E, A, 0);
    SHA_MIX(1, 14, 9, 3);
    SHA_RND3(A, B, C, D, E, 1);
    SHA_MIX(2, 15, 10, 4);
    SHA_RND3(E, A, B, C, D, 2);
    SHA_MIX(3, 0, 11, 5);
    SHA_RND3(D, E, A, B, C, 3);
    SHA_MIX(4, 1, 12, 6);
    SHA_RND3(C, D, E, A, B, 4);
    SHA_MIX(5, 2, 13, 7);
    SHA_RND3(B, C, D, E, A, 5);
    SHA_MIX(6, 3, 14, 8);
    SHA_RND3(A, B, C, D, E, 6);
    SHA_MIX(7, 4, 15, 9);
    SHA_RND3(E, A, B, C, D, 7);
    SHA_MIX(8, 5, 0, 10);
    SHA_RND3(D, E, A, B, C, 8);
    SHA_MIX(9, 6, 1, 11);
    SHA_RND3(C, D, E, A, B, 9);
    SHA_MIX(10, 7, 2, 12);
    SHA_RND3(B, C, D, E, A, 10);
    SHA_MIX(11, 8, 3, 13);
    SHA_RND3(A, B, C, D, E, 11);

    SHA_MIX(12, 9, 4, 14);
    SHA_RND4(E, A, B, C, D, 12);
    SHA_MIX(13, 10, 5, 15);
    SHA_RND4(D, E, A, B, C, 13);
    SHA_MIX(14, 11, 6, 0);
    SHA_RND4(C, D, E, A, B, 14);
    SHA_MIX(15, 12, 7, 1);
    SHA_RND4(B, C, D, E, A, 15);

    SHA_MIX(0, 13, 8, 2);
    SHA_RND4(A, B, C, D, E, 0);
    SHA_MIX(1, 14, 9, 3);
    SHA_RND4(E, A, B, C, D, 1);
    SHA_MIX(2, 15, 10, 4);
    SHA_RND4(D, E, A, B, C, 2);
    SHA_MIX(3, 0, 11, 5);
    SHA_RND4(C, D, E, A, B, 3);
    SHA_MIX(4, 1, 12, 6);
    SHA_RND4(B, C, D, E, A, 4);
    SHA_MIX(5, 2, 13, 7);
    SHA_RND4(A, B, C, D, E, 5);
    SHA_MIX(6, 3, 14, 8);
    SHA_RND4(E, A, B, C, D, 6);
    SHA_MIX(7, 4, 15, 9);
    SHA_RND4(D, E, A, B, C, 7);
    SHA_MIX(8, 5, 0, 10);
    SHA_RND4(C, D, E, A, B, 8);
    SHA_MIX(9, 6, 1, 11);
    SHA_RND4(B, C, D, E, A, 9);
    SHA_MIX(10, 7, 2, 12);
    SHA_RND4(A, B, C, D, E, 10);
    SHA_MIX(11, 8, 3, 13);
    SHA_RND4(E, A, B, C, D, 11);
    SHA_MIX(12, 9, 4, 14);
    SHA_RND4(D, E, A, B, C, 12);
    SHA_MIX(13, 10, 5, 15);
    SHA_RND4(C, D, E, A, B, 13);
    SHA_MIX(14, 11, 6, 0);
    SHA_RND4(B, C, D, E, A, 14);
    SHA_MIX(15, 12, 7, 1);
    SHA_RND4(A, B, C, D, E, 15);

    XH(0) += A;
    XH(1) += B;
    XH(2) += C;
    XH(3) += D;
    XH(4) += E;
}

/*************************************************************************
** Code below this line added to make SHA code support BLAPI interface
*/

SHA1Context *
SHA1_NewContext(void)
{
    SHA1Context *cx;

    /* no need to ZNew, SHA1_Begin will init the context */
    cx = PORT_New(SHA1Context);
    return cx;
}

/* Zero and free the context */
void
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
    memset(cx, 0, sizeof *cx);
    if (freeit) {
        PORT_Free(cx);
    }
}

SECStatus
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SHA1Context ctx;
    unsigned int outLen;

    SHA1_Begin(&ctx);
    SHA1_Update(&ctx, src, src_length);
    SHA1_End(&ctx, dest, &outLen, SHA1_LENGTH);
    memset(&ctx, 0, sizeof ctx);
    return SECSuccess;
}

/* Hash a null-terminated character string. */
SECStatus
SHA1_Hash(unsigned char *dest, const char *src)
{
    return SHA1_HashBuf(dest, (const unsigned char *)src, PORT_Strlen(src));
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
SHA1_Flatten(SHA1Context *cx, unsigned char *space)
{
    PORT_Memcpy(space, cx, sizeof(SHA1Context));
    return SECSuccess;
}

SHA1Context *
SHA1_Resurrect(unsigned char *space, void *arg)
{
    SHA1Context *cx = SHA1_NewContext();
    if (cx == NULL)
        return NULL;

    PORT_Memcpy(cx, space, sizeof(SHA1Context));
    return cx;
}

void
SHA1_Clone(SHA1Context *dest, SHA1Context *src)
{
    memcpy(dest, src, sizeof *dest);
}

void
SHA1_TraceState(SHA1Context *ctx)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
}
