/*
 * math.h
 *
 * crypto math operations and data types
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright (c) 2001-2006 Cisco Systems, Inc.
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

#ifndef MATH_H
#define MATH_H

#include "datatypes.h"

unsigned char
v32_weight(v32_t a);

unsigned char
v32_distance(v32_t x, v32_t y);

unsigned int
v32_dot_product(v32_t a, v32_t b);

char *
v16_bit_string(v16_t x);

char *
v32_bit_string(v32_t x);

char *
v64_bit_string(const v64_t *x);

char *
octet_hex_string(uint8_t x);

char *
v16_hex_string(v16_t x);

char *
v32_hex_string(v32_t x);

char *
v64_hex_string(const v64_t *x);

int
hex_char_to_nibble(uint8_t c);

int
is_hex_string(char *s);

v16_t
hex_string_to_v16(char *s);

v32_t
hex_string_to_v32(char *s);

v64_t
hex_string_to_v64(char *s);

/* the matrix A[] is stored in column format, i.e., A[i] is
   the ith column of the matrix */

uint8_t 
A_times_x_plus_b(uint8_t A[8], uint8_t x, uint8_t b);

void
v16_copy_octet_string(v16_t *x, const uint8_t s[2]);

void
v32_copy_octet_string(v32_t *x, const uint8_t s[4]);

void
v64_copy_octet_string(v64_t *x, const uint8_t s[8]);

void
v128_add(v128_t *z, v128_t *x, v128_t *y);

int
octet_string_is_eq(uint8_t *a, uint8_t *b, int len);

void
octet_string_set_to_zero(uint8_t *s, int len);



/* 
 * the matrix A[] is stored in column format, i.e., A[i] is the ith
 * column of the matrix
*/
uint8_t 
A_times_x_plus_b(uint8_t A[8], uint8_t x, uint8_t b);


#if 0
#if WORDS_BIGENDIAN

#define _v128_add(z, x, y) {                    \
  uint64_t tmp;					\
    						\
  tmp = x->v32[3] + y->v32[3];                  \
  z->v32[3] = (uint32_t) tmp;			\
  						\
  tmp =  x->v32[2] + y->v32[2] + (tmp >> 32);	\
  z->v32[2] = (uint32_t) tmp;                   \
						\
  tmp =  x->v32[1] + y->v32[1] + (tmp >> 32);	\
  z->v32[1] = (uint32_t) tmp;			\
                                                \
  tmp =  x->v32[0] + y->v32[0] + (tmp >> 32);	\
  z->v32[0] = (uint32_t) tmp;			\
}

#else /* assume little endian architecture */

#define _v128_add(z, x, y) {                    \
  uint64_t tmp;					\
						\
  tmp = htonl(x->v32[3]) + htonl(y->v32[3]);	\
  z->v32[3] = ntohl((uint32_t) tmp);		\
  						\
  tmp =  htonl(x->v32[2]) + htonl(y->v32[2])	\
       + htonl(tmp >> 32);			\
  z->v32[2] = ntohl((uint32_t) tmp);		\
                                                \
  tmp =  htonl(x->v32[1]) + htonl(y->v32[1])	\
       + htonl(tmp >> 32);			\
  z->v32[1] = ntohl((uint32_t) tmp);		\
  						\
  tmp =  htonl(x->v32[0]) + htonl(y->v32[0])	\
       + htonl(tmp >> 32);			\
  z->v32[0] = ntohl((uint32_t) tmp);		\
}
						
#endif /* WORDS_BIGENDIAN */                      
#endif

#ifdef DATATYPES_USE_MACROS  /* little functions are really macros */

#define v128_set_to_zero(z)       _v128_set_to_zero(z)
#define v128_copy(z, x)           _v128_copy(z, x)
#define v128_xor(z, x, y)         _v128_xor(z, x, y)
#define v128_and(z, x, y)         _v128_and(z, x, y)
#define v128_or(z, x, y)          _v128_or(z, x, y)
#define v128_complement(x)        _v128_complement(x) 
#define v128_is_eq(x, y)          _v128_is_eq(x, y)
#define v128_xor_eq(x, y)         _v128_xor_eq(x, y)
#define v128_get_bit(x, i)        _v128_get_bit(x, i)
#define v128_set_bit(x, i)        _v128_set_bit(x, i)
#define v128_clear_bit(x, i)      _v128_clear_bit(x, i)
#define v128_set_bit_to(x, i, y)  _v128_set_bit_to(x, i, y)

#else

void
v128_set_to_zero(v128_t *x);

int
v128_is_eq(const v128_t *x, const v128_t *y);

void
v128_copy(v128_t *x, const v128_t *y);

void
v128_xor(v128_t *z, v128_t *x, v128_t *y);

void
v128_and(v128_t *z, v128_t *x, v128_t *y);

void
v128_or(v128_t *z, v128_t *x, v128_t *y); 

void
v128_complement(v128_t *x);

int
v128_get_bit(const v128_t *x, int i);

void
v128_set_bit(v128_t *x, int i) ;     

void
v128_clear_bit(v128_t *x, int i);    

void
v128_set_bit_to(v128_t *x, int i, int y);

#endif /* DATATYPES_USE_MACROS */

/*
 * octet_string_is_eq(a,b, len) returns 1 if the length len strings a
 * and b are not equal, returns 0 otherwise
 */

int
octet_string_is_eq(uint8_t *a, uint8_t *b, int len);

void
octet_string_set_to_zero(uint8_t *s, int len);


#endif /* MATH_H */



