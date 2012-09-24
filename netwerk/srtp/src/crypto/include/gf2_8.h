/*
 * gf2_8.h
 *
 * GF(256) implementation
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


#ifndef GF2_8_H
#define GF2_8_H

#include "datatypes.h"  /* for uint8_t definition */

typedef uint8_t gf2_8;

#define gf2_8_field_polynomial 0x1B

/*
 * gf2_8_shift(x) returns 
 */

/*
 * gf2_8_shift(z) returns the result of the GF(2^8) 'multiply by x' 
 * operation, using the field representation from AES; that is, the 
 * next gf2_8 value in the cyclic representation of that field.  The 
 * value z should be an uint8_t.
 */

#define gf2_8_shift(z) (((z) & 128) ? \
       (((z) << 1) ^ gf2_8_field_polynomial) : ((z) << 1))

gf2_8
gf2_8_compute_inverse(gf2_8 x);

void
test_gf2_8(void);

gf2_8
gf2_8_multiply(gf2_8 x, gf2_8 y);

#endif /* GF2_8_H */
