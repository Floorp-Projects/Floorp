/*
 * gf2_8.c
 *
 * GF(256) finite field implementation, with the representation used
 * in the AES cipher.
 * 
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "datatypes.h"
#include "gf2_8.h"

/* gf2_8_shift() moved to gf2_8.h as an inline function */

gf2_8
gf2_8_multiply(gf2_8 x, gf2_8 y) {
  gf2_8 z = 0;

  if (y &   1) z ^= x; x = gf2_8_shift(x);
  if (y &   2) z ^= x; x = gf2_8_shift(x);
  if (y &   4) z ^= x; x = gf2_8_shift(x);
  if (y &   8) z ^= x; x = gf2_8_shift(x);
  if (y &  16) z ^= x; x = gf2_8_shift(x);
  if (y &  32) z ^= x; x = gf2_8_shift(x);
  if (y &  64) z ^= x; x = gf2_8_shift(x);
  if (y & 128) z ^= x; 
  
  return z;
}


/* this should use the euclidean algorithm */

gf2_8
gf2_8_compute_inverse(gf2_8 x) {
  unsigned int i;

  if (x == 0) return 0;    /* zero is a special case */
  for (i=0; i < 256; i++)
    if (gf2_8_multiply((gf2_8) i, x) == 1)
      return (gf2_8) i;

    return 0;
}

