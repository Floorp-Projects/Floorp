/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SHA 180-1 Reference Implementation (Optimized).
 *
 * The Initial Developer of the Original Code is
 * Paul Kocher of Cryptography Research.
 * Portions created by the Initial Developer are Copyright (C) 1995-9
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <memory.h>
#include "blapi.h"
#include "sha_fast.h"
#include "prerror.h"

#ifdef TRACING_SSL
#include "ssl.h"
#include "ssltrace.h"
#endif

#if defined(IS_LITTLE_ENDIAN) && defined(_MSC_VER) && defined(_X86_)
#undef SHA_HTONL
#ifndef FORCEINLINE
#if (MSC_VER >= 1200)
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE __inline
#endif
#endif
#define FASTCALL __fastcall

static FORCEINLINE PRUint32 FASTCALL 
swap4b(PRUint32 dwd) 
{
    __asm {
    	mov   eax,dwd
	bswap eax
    }
}

#define SHA_HTONL(x) swap4b(x)
#endif

static void shaCompress(SHA1Context *ctx);

#define W u.w
#define B u.b

#if defined(_MSC_VER) && defined(_X86_)
#pragma intrinsic (_lrotr, _lrotl) 
#define SHA_ROTL(x,n) _lrotl(x,n)
#else
#define SHA_ROTL(X,n) (((X) << (n)) | ((X) >> (32-(n))))
#endif
#define SHA_F1(X,Y,Z) ((((Y)^(Z))&(X))^(Z))
#define SHA_F2(X,Y,Z) ((X)^(Y)^(Z))
#define SHA_F3(X,Y,Z) (((X)&(Y))|((Z)&((X)|(Y))))
#define SHA_F4(X,Y,Z) ((X)^(Y)^(Z))
#define SHA_MIX(t)    ctx->W[t] = \
  (A = ctx->W[t-3] ^ ctx->W[t-8] ^ ctx->W[t-14] ^ ctx->W[t-16], SHA_ROTL(A, 1))


/*
 *  SHA: Zeroize and initialize context
 */
void 
SHA1_Begin(SHA1Context *ctx)
{
  memset(ctx, 0, sizeof(SHA1Context));

  /*
   *  Initialize H with constants from FIPS180-1.
   */
  ctx->H[0] = 0x67452301L;
  ctx->H[1] = 0xefcdab89L;
  ctx->H[2] = 0x98badcfeL;
  ctx->H[3] = 0x10325476L;
  ctx->H[4] = 0xc3d2e1f0L;
}


/*
 *  SHA: Add data to context.
 */
void 
SHA1_Update(SHA1Context *ctx, const unsigned char *dataIn, unsigned int len) 
{
  register unsigned int lenB = ctx->sizeLo & 63;
  register unsigned int togo;

  if (!len)
    return;

  /* accumulate the byte count. */
  ctx->sizeLo += len;
  ctx->sizeHi += (ctx->sizeLo < len);

  /*
   *  Read the data into W and process blocks as they get full
   */
  if (lenB > 0) {
    togo = 64 - lenB;
    if (len < togo)
      togo = len;
    memcpy(ctx->B + lenB, dataIn, togo);
    len    -= togo;
    dataIn += togo;
    lenB    = (lenB + togo) & 63;
    if (!lenB) {
      shaCompress(ctx);
    }
  }
  while (len >= 64) {
    memcpy(ctx->B, dataIn, 64);
    dataIn += 64;
    len    -= 64;
    shaCompress(ctx);
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
  register PRUint32 sizeHi, sizeLo, lenB;
  static const unsigned char bulk_pad[64] = { 0x80,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  };
#define A lenB

  PORT_Assert (maxDigestLen >= SHA1_LENGTH);

  /*
   *  Pad with a binary 1 (e.g. 0x80), then zeroes, then length in bits
   */
  sizeHi = ctx->sizeHi;
  sizeLo = ctx->sizeLo;
  lenB = sizeLo & 63;
  SHA1_Update(ctx, bulk_pad, (((55+64) - lenB) & 63) + 1);
  PORT_Assert((ctx->sizeLo & 63) == 56);

  /* Convert size{Hi,Lo} from bytes to bits. */
  sizeHi = (sizeHi << 3) | (sizeLo >> 29);
  sizeLo <<= 3;

  ctx->W[14] = SHA_HTONL(sizeHi);
  ctx->W[15] = SHA_HTONL(sizeLo);
  shaCompress(ctx);

  /*
   *  Output hash
   */
#if defined(IS_LITTLE_ENDIAN)
  SHA_BYTESWAP(ctx->H[0]);
  SHA_BYTESWAP(ctx->H[1]);
  SHA_BYTESWAP(ctx->H[2]);
  SHA_BYTESWAP(ctx->H[3]);
  SHA_BYTESWAP(ctx->H[4]);
#endif
  memcpy(hashout, ctx->H, SHA1_LENGTH);
  *pDigestLen = SHA1_LENGTH;

  /*
   *  Re-initialize the context (also zeroizes contents)
   */
  SHA1_Begin(ctx);
}

#undef A
#undef B
/*
 *  SHA: Compression function, unrolled.
 */
static void 
shaCompress(SHA1Context *ctx) 
{
  register PRUint32 A, B, C, D, E;

#if defined(IS_LITTLE_ENDIAN)
  SHA_BYTESWAP(ctx->W[0]);
  SHA_BYTESWAP(ctx->W[1]);
  SHA_BYTESWAP(ctx->W[2]);
  SHA_BYTESWAP(ctx->W[3]);
  SHA_BYTESWAP(ctx->W[4]);
  SHA_BYTESWAP(ctx->W[5]);
  SHA_BYTESWAP(ctx->W[6]);
  SHA_BYTESWAP(ctx->W[7]);
  SHA_BYTESWAP(ctx->W[8]);
  SHA_BYTESWAP(ctx->W[9]);
  SHA_BYTESWAP(ctx->W[10]);
  SHA_BYTESWAP(ctx->W[11]);
  SHA_BYTESWAP(ctx->W[12]);
  SHA_BYTESWAP(ctx->W[13]);
  SHA_BYTESWAP(ctx->W[14]);
  SHA_BYTESWAP(ctx->W[15]);
#endif

  /*
   *  This can be moved into the main code block below, but doing
   *  so can cause some compilers to run out of registers and resort
   *  to storing intermediates in RAM.
   */

               SHA_MIX(16); SHA_MIX(17); SHA_MIX(18); SHA_MIX(19);
  SHA_MIX(20); SHA_MIX(21); SHA_MIX(22); SHA_MIX(23); SHA_MIX(24);
  SHA_MIX(25); SHA_MIX(26); SHA_MIX(27); SHA_MIX(28); SHA_MIX(29);
  SHA_MIX(30); SHA_MIX(31); SHA_MIX(32); SHA_MIX(33); SHA_MIX(34);
  SHA_MIX(35); SHA_MIX(36); SHA_MIX(37); SHA_MIX(38); SHA_MIX(39);
  SHA_MIX(40); SHA_MIX(41); SHA_MIX(42); SHA_MIX(43); SHA_MIX(44);
  SHA_MIX(45); SHA_MIX(46); SHA_MIX(47); SHA_MIX(48); SHA_MIX(49);
  SHA_MIX(50); SHA_MIX(51); SHA_MIX(52); SHA_MIX(53); SHA_MIX(54);
  SHA_MIX(55); SHA_MIX(56); SHA_MIX(57); SHA_MIX(58); SHA_MIX(59);
  SHA_MIX(60); SHA_MIX(61); SHA_MIX(62); SHA_MIX(63); SHA_MIX(64);
  SHA_MIX(65); SHA_MIX(66); SHA_MIX(67); SHA_MIX(68); SHA_MIX(69);
  SHA_MIX(70); SHA_MIX(71); SHA_MIX(72); SHA_MIX(73); SHA_MIX(74);
  SHA_MIX(75); SHA_MIX(76); SHA_MIX(77); SHA_MIX(78); SHA_MIX(79);

  A = ctx->H[0];
  B = ctx->H[1];
  C = ctx->H[2];
  D = ctx->H[3];
  E = ctx->H[4];

  E = SHA_ROTL(A,5)+SHA_F1(B,C,D)+E+ctx->W[ 0]+0x5a827999L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F1(A,B,C)+D+ctx->W[ 1]+0x5a827999L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F1(E,A,B)+C+ctx->W[ 2]+0x5a827999L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F1(D,E,A)+B+ctx->W[ 3]+0x5a827999L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F1(C,D,E)+A+ctx->W[ 4]+0x5a827999L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F1(B,C,D)+E+ctx->W[ 5]+0x5a827999L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F1(A,B,C)+D+ctx->W[ 6]+0x5a827999L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F1(E,A,B)+C+ctx->W[ 7]+0x5a827999L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F1(D,E,A)+B+ctx->W[ 8]+0x5a827999L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F1(C,D,E)+A+ctx->W[ 9]+0x5a827999L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F1(B,C,D)+E+ctx->W[10]+0x5a827999L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F1(A,B,C)+D+ctx->W[11]+0x5a827999L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F1(E,A,B)+C+ctx->W[12]+0x5a827999L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F1(D,E,A)+B+ctx->W[13]+0x5a827999L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F1(C,D,E)+A+ctx->W[14]+0x5a827999L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F1(B,C,D)+E+ctx->W[15]+0x5a827999L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F1(A,B,C)+D+ctx->W[16]+0x5a827999L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F1(E,A,B)+C+ctx->W[17]+0x5a827999L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F1(D,E,A)+B+ctx->W[18]+0x5a827999L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F1(C,D,E)+A+ctx->W[19]+0x5a827999L; C=SHA_ROTL(C,30); 

  E = SHA_ROTL(A,5)+SHA_F2(B,C,D)+E+ctx->W[20]+0x6ed9eba1L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F2(A,B,C)+D+ctx->W[21]+0x6ed9eba1L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F2(E,A,B)+C+ctx->W[22]+0x6ed9eba1L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F2(D,E,A)+B+ctx->W[23]+0x6ed9eba1L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F2(C,D,E)+A+ctx->W[24]+0x6ed9eba1L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F2(B,C,D)+E+ctx->W[25]+0x6ed9eba1L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F2(A,B,C)+D+ctx->W[26]+0x6ed9eba1L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F2(E,A,B)+C+ctx->W[27]+0x6ed9eba1L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F2(D,E,A)+B+ctx->W[28]+0x6ed9eba1L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F2(C,D,E)+A+ctx->W[29]+0x6ed9eba1L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F2(B,C,D)+E+ctx->W[30]+0x6ed9eba1L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F2(A,B,C)+D+ctx->W[31]+0x6ed9eba1L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F2(E,A,B)+C+ctx->W[32]+0x6ed9eba1L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F2(D,E,A)+B+ctx->W[33]+0x6ed9eba1L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F2(C,D,E)+A+ctx->W[34]+0x6ed9eba1L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F2(B,C,D)+E+ctx->W[35]+0x6ed9eba1L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F2(A,B,C)+D+ctx->W[36]+0x6ed9eba1L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F2(E,A,B)+C+ctx->W[37]+0x6ed9eba1L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F2(D,E,A)+B+ctx->W[38]+0x6ed9eba1L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F2(C,D,E)+A+ctx->W[39]+0x6ed9eba1L; C=SHA_ROTL(C,30); 

  E = SHA_ROTL(A,5)+SHA_F3(B,C,D)+E+ctx->W[40]+0x8f1bbcdcL; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F3(A,B,C)+D+ctx->W[41]+0x8f1bbcdcL; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F3(E,A,B)+C+ctx->W[42]+0x8f1bbcdcL; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F3(D,E,A)+B+ctx->W[43]+0x8f1bbcdcL; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F3(C,D,E)+A+ctx->W[44]+0x8f1bbcdcL; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F3(B,C,D)+E+ctx->W[45]+0x8f1bbcdcL; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F3(A,B,C)+D+ctx->W[46]+0x8f1bbcdcL; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F3(E,A,B)+C+ctx->W[47]+0x8f1bbcdcL; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F3(D,E,A)+B+ctx->W[48]+0x8f1bbcdcL; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F3(C,D,E)+A+ctx->W[49]+0x8f1bbcdcL; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F3(B,C,D)+E+ctx->W[50]+0x8f1bbcdcL; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F3(A,B,C)+D+ctx->W[51]+0x8f1bbcdcL; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F3(E,A,B)+C+ctx->W[52]+0x8f1bbcdcL; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F3(D,E,A)+B+ctx->W[53]+0x8f1bbcdcL; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F3(C,D,E)+A+ctx->W[54]+0x8f1bbcdcL; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F3(B,C,D)+E+ctx->W[55]+0x8f1bbcdcL; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F3(A,B,C)+D+ctx->W[56]+0x8f1bbcdcL; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F3(E,A,B)+C+ctx->W[57]+0x8f1bbcdcL; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F3(D,E,A)+B+ctx->W[58]+0x8f1bbcdcL; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F3(C,D,E)+A+ctx->W[59]+0x8f1bbcdcL; C=SHA_ROTL(C,30); 

  E = SHA_ROTL(A,5)+SHA_F4(B,C,D)+E+ctx->W[60]+0xca62c1d6L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F4(A,B,C)+D+ctx->W[61]+0xca62c1d6L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F4(E,A,B)+C+ctx->W[62]+0xca62c1d6L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F4(D,E,A)+B+ctx->W[63]+0xca62c1d6L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F4(C,D,E)+A+ctx->W[64]+0xca62c1d6L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F4(B,C,D)+E+ctx->W[65]+0xca62c1d6L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F4(A,B,C)+D+ctx->W[66]+0xca62c1d6L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F4(E,A,B)+C+ctx->W[67]+0xca62c1d6L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F4(D,E,A)+B+ctx->W[68]+0xca62c1d6L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F4(C,D,E)+A+ctx->W[69]+0xca62c1d6L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F4(B,C,D)+E+ctx->W[70]+0xca62c1d6L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F4(A,B,C)+D+ctx->W[71]+0xca62c1d6L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F4(E,A,B)+C+ctx->W[72]+0xca62c1d6L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F4(D,E,A)+B+ctx->W[73]+0xca62c1d6L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F4(C,D,E)+A+ctx->W[74]+0xca62c1d6L; C=SHA_ROTL(C,30); 
  E = SHA_ROTL(A,5)+SHA_F4(B,C,D)+E+ctx->W[75]+0xca62c1d6L; B=SHA_ROTL(B,30); 
  D = SHA_ROTL(E,5)+SHA_F4(A,B,C)+D+ctx->W[76]+0xca62c1d6L; A=SHA_ROTL(A,30); 
  C = SHA_ROTL(D,5)+SHA_F4(E,A,B)+C+ctx->W[77]+0xca62c1d6L; E=SHA_ROTL(E,30); 
  B = SHA_ROTL(C,5)+SHA_F4(D,E,A)+B+ctx->W[78]+0xca62c1d6L; D=SHA_ROTL(D,30); 
  A = SHA_ROTL(B,5)+SHA_F4(C,D,E)+A+ctx->W[79]+0xca62c1d6L; C=SHA_ROTL(C,30); 

  ctx->H[0] += A;
  ctx->H[1] += B;
  ctx->H[2] += C;
  ctx->H[3] += D;
  ctx->H[4] += E;
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

void
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
    if (freeit) {
        PORT_ZFree(cx, sizeof(SHA1Context));
    }
}

SECStatus
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
    SHA1Context ctx;
    unsigned int outLen;

    SHA1_Begin(&ctx);
    SHA1_Update(&ctx, src, src_length);
    SHA1_End(&ctx, dest, &outLen, SHA1_LENGTH);

    return SECSuccess;
}

/* Hash a null-terminated character string. */
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

void
SHA1_TraceState(SHA1Context *ctx)
{
#ifdef TRACING_SSL
    uint32        W;
    int           i;
    int           len;
    int           fixWord = -1;
    int           remainder;	/* bytes in last word */
    unsigned char buf[64 * 4];

    SSL_TRC(99, ("%d: SSL: SHA1 state: %08x %08x %08x %08x %08x", SSL_GETPID(), 
	         ctx->H[0], ctx->H[1], ctx->H[2], ctx->H[3], ctx->H[4]));

    len = (int)(ctx->sizeLo & 63);
    remainder = len % 4;
    if (remainder) 
    	fixWord = len - remainder;
    for (i = 0; i < len; i++) {
	if (0 == (i % 4)) {
	    W = ctx->W[i / 4];
	    if (i == fixWord) {
	        W <<= 8 * (4 - remainder);
	    }
	}
	buf[i] = (unsigned char)(W >> 24);
	W <<= 8;
    }

    PRINT_BUF(99, (0, "SHA1_TraceState: buffered input", buf, len));

#else
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
#endif
}
