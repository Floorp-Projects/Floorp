/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

/*
 * "clean room" MD4 implementation (see RFC 1320)
 */

#include <string.h>
#include "md4.h"

typedef PRUint32 Uint32;
typedef PRUint8 Uint8;

/* the "conditional" function */
#define F(x,y,z) (((x) & (y)) | (~(x) & (z)))

/* the "majority" function */
#define G(x,y,z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))

/* the "parity" function */
#define H(x,y,z) ((x) ^ (y) ^ (z))

/* rotate n-bits to the left */
#define ROTL(x,n) (((x) << (n)) | ((x) >> (0x20 - n)))

/* round 1: [abcd k s]: a = (a + F(b,c,d) + X[k]) <<< s */
#define RD1(a,b,c,d,k,s) a += F(b,c,d) + X[k]; a = ROTL(a,s)

/* round 2: [abcd k s]: a = (a + G(b,c,d) + X[k] + MAGIC) <<< s */
#define RD2(a,b,c,d,k,s) a += G(b,c,d) + X[k] + 0x5A827999; a = ROTL(a,s)

/* round 3: [abcd k s]: a = (a + H(b,c,d) + X[k] + MAGIC) <<< s */
#define RD3(a,b,c,d,k,s) a += H(b,c,d) + X[k] + 0x6ED9EBA1; a = ROTL(a,s)

/* converts from word array to byte array, len is number of bytes */
static void w2b(Uint8 *out, const Uint32 *in, Uint32 len)
{
  Uint8 *bp; const Uint32 *wp, *wpend;

  bp = out;
  wp = in;
  wpend = wp + (len >> 2);

  for (; wp != wpend; ++wp, bp += 4)
  {
    bp[0] = (Uint8) ((*wp      ) & 0xFF);
    bp[1] = (Uint8) ((*wp >>  8) & 0xFF);
    bp[2] = (Uint8) ((*wp >> 16) & 0xFF);
    bp[3] = (Uint8) ((*wp >> 24) & 0xFF);
  }
}

/* converts from byte array to word array, len is number of bytes */
static void b2w(Uint32 *out, const Uint8 *in, Uint32 len)
{
  Uint32 *wp; const Uint8 *bp, *bpend;

  wp = out;
  bp = in;
  bpend = in + len;

  for (; bp != bpend; bp += 4, ++wp)
  {
    *wp = (Uint32) (bp[0]      ) |
          (Uint32) (bp[1] <<  8) |
          (Uint32) (bp[2] << 16) |
          (Uint32) (bp[3] << 24);
  }
}

/* update state: data is 64 bytes in length */
static void md4step(Uint32 state[4], const Uint8 *data)
{
  Uint32 A, B, C, D, X[16];

  b2w(X, data, 64);

  A = state[0];
  B = state[1];
  C = state[2];
  D = state[3];

  RD1(A,B,C,D, 0,3); RD1(D,A,B,C, 1,7); RD1(C,D,A,B, 2,11); RD1(B,C,D,A, 3,19);
  RD1(A,B,C,D, 4,3); RD1(D,A,B,C, 5,7); RD1(C,D,A,B, 6,11); RD1(B,C,D,A, 7,19);
  RD1(A,B,C,D, 8,3); RD1(D,A,B,C, 9,7); RD1(C,D,A,B,10,11); RD1(B,C,D,A,11,19);
  RD1(A,B,C,D,12,3); RD1(D,A,B,C,13,7); RD1(C,D,A,B,14,11); RD1(B,C,D,A,15,19);

  RD2(A,B,C,D, 0,3); RD2(D,A,B,C, 4,5); RD2(C,D,A,B, 8, 9); RD2(B,C,D,A,12,13); 
  RD2(A,B,C,D, 1,3); RD2(D,A,B,C, 5,5); RD2(C,D,A,B, 9, 9); RD2(B,C,D,A,13,13); 
  RD2(A,B,C,D, 2,3); RD2(D,A,B,C, 6,5); RD2(C,D,A,B,10, 9); RD2(B,C,D,A,14,13); 
  RD2(A,B,C,D, 3,3); RD2(D,A,B,C, 7,5); RD2(C,D,A,B,11, 9); RD2(B,C,D,A,15,13); 

  RD3(A,B,C,D, 0,3); RD3(D,A,B,C, 8,9); RD3(C,D,A,B, 4,11); RD3(B,C,D,A,12,15);
  RD3(A,B,C,D, 2,3); RD3(D,A,B,C,10,9); RD3(C,D,A,B, 6,11); RD3(B,C,D,A,14,15);
  RD3(A,B,C,D, 1,3); RD3(D,A,B,C, 9,9); RD3(C,D,A,B, 5,11); RD3(B,C,D,A,13,15);
  RD3(A,B,C,D, 3,3); RD3(D,A,B,C,11,9); RD3(C,D,A,B, 7,11); RD3(B,C,D,A,15,15);

  state[0] += A;
  state[1] += B;
  state[2] += C;
  state[3] += D;
}

void md4sum(const Uint8 *input, Uint32 inputLen, Uint8 *result)
{
  Uint8 final[128];
  Uint32 i, n, m, state[4];

  /* magic initial states */
  state[0] = 0x67452301;
  state[1] = 0xEFCDAB89;
  state[2] = 0x98BADCFE;
  state[3] = 0x10325476;

  /* compute number of complete 64-byte segments contained in input */
  m = inputLen >> 6;

  /* digest first m segments */
  for (i=0; i<m; ++i)
    md4step(state, (input + (i << 6)));

  /* build final buffer */
  n = inputLen % 64;
  memcpy(final, input + (m << 6), n);
  final[n] = 0x80;
  memset(final + n + 1, 0, 120 - (n + 1));

  inputLen = inputLen << 3;
  w2b(final + (n >= 56 ? 120 : 56), &inputLen, 4);

  md4step(state, final);
  if (n >= 56)
    md4step(state, final + 64);

  /* copy state to result */
  w2b(result, state, 16);
}
