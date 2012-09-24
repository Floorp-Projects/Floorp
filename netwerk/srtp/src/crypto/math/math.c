/*
 * math.c
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

#include "crypto_math.h"

int 
octet_weight[256] = {
  0, 1, 1, 2, 1, 2, 2, 3,
  1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7,
  5, 6, 6, 7, 6, 7, 7, 8
};

int
low_bit[256] = {
  -1, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  6, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  7, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  6, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0,
  3, 0, 1, 0, 2, 0, 1, 0
};


int
high_bit[256] = {
  -1, 0, 1, 1, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7
};

int
octet_get_weight(uint8_t octet) {
  extern int octet_weight[256];

  return octet_weight[octet];
}  

unsigned char
v32_weight(v32_t a) {
  unsigned int wt = 0;
  
  wt += octet_weight[a.v8[0]];  /* note: endian-ness makes no difference */
  wt += octet_weight[a.v8[1]];
  wt += octet_weight[a.v8[2]];
  wt += octet_weight[a.v8[3]];
  
  return wt;
}

unsigned char
v32_distance(v32_t x, v32_t y) {
  x.value ^= y.value;
  return v32_weight(x);
}

unsigned int
v32_dot_product(v32_t a, v32_t b) {
  a.value &= b.value;
  return v32_weight(a) & 1;
}

/*
 * _bit_string returns a NULL-terminated character string suitable for
 * printing
 */

#define MAX_STRING_LENGTH 1024

char bit_string[MAX_STRING_LENGTH];

char *
octet_bit_string(uint8_t x) {
  int mask, index;

  for (mask = 1, index = 0; mask < 256; mask <<= 1)
    if ((x & mask) == 0)
      bit_string[index++] = '0';
    else
      bit_string[index++] = '1';

  bit_string[index++] = 0;  /* NULL terminate string */

  return bit_string;
}

char *
v16_bit_string(v16_t x) {
  int i, mask, index;

  for (i = index = 0; i < 2; i++) {
    for (mask = 1; mask < 256; mask <<= 1)
      if ((x.v8[i] & mask) == 0)
	bit_string[index++] = '0';
      else
	bit_string[index++] = '1';
  }
  bit_string[index++] = 0;  /* NULL terminate string */
  return bit_string;
}

char *
v32_bit_string(v32_t x) {
  int i, mask, index;

  for (i = index = 0; i < 4; i++) {
    for (mask = 128; mask > 0; mask >>= 1)
      if ((x.v8[i] & mask) == 0)
	bit_string[index++] = '0';
      else
	bit_string[index++] = '1';
  }
  bit_string[index++] = 0;  /* NULL terminate string */
  return bit_string;
}

char *
v64_bit_string(const v64_t *x) {
  int i, mask, index;

  for (i = index = 0; i < 8; i++) {
    for (mask = 1; mask < 256; mask <<= 1)
      if ((x->v8[i] & mask) == 0)
	bit_string[index++] = '0';
      else
	bit_string[index++] = '1';
  }
  bit_string[index++] = 0;  /* NULL terminate string */
  return bit_string;
}

char *
v128_bit_string(v128_t *x) {
  int j, index;
  uint32_t mask;
  
  for (j=index=0; j < 4; j++) {
    for (mask=0x80000000; mask > 0; mask >>= 1) {
      if (x->v32[j] & mask)
	bit_string[index] = '1';
      else
	bit_string[index] = '0';
      ++index;
    }
  }
  bit_string[128] = 0; /* null terminate string */

  return bit_string;
}

uint8_t
nibble_to_hex_char(uint8_t nibble) {
  char buf[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
		  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  return buf[nibble & 0xF];
}

char *
octet_hex_string(uint8_t x) {

  bit_string[0]  = nibble_to_hex_char(x >> 4);
  bit_string[1]  = nibble_to_hex_char(x & 0xF);
  
  bit_string[2] = 0; /* null terminate string */
  return bit_string;
}

char *
octet_string_hex_string(const void *str, int length) {
  const uint8_t *s = str;
  int i;
  
  /* double length, since one octet takes two hex characters */
  length *= 2;

  /* truncate string if it would be too long */
  if (length > MAX_STRING_LENGTH)
    length = MAX_STRING_LENGTH-1;
  
  for (i=0; i < length; i+=2) {
    bit_string[i]   = nibble_to_hex_char(*s >> 4);
    bit_string[i+1] = nibble_to_hex_char(*s++ & 0xF);
  }
  bit_string[i] = 0; /* null terminate string */
  return bit_string;
}

char *
v16_hex_string(v16_t x) {
  int i, j;

  for (i=j=0; i < 2; i++) {
    bit_string[j++]  = nibble_to_hex_char(x.v8[i] >> 4);
    bit_string[j++]  = nibble_to_hex_char(x.v8[i] & 0xF);
  }
  
  bit_string[j] = 0; /* null terminate string */
  return bit_string;
}

char *
v32_hex_string(v32_t x) {
  int i, j;

  for (i=j=0; i < 4; i++) {
    bit_string[j++]  = nibble_to_hex_char(x.v8[i] >> 4);
    bit_string[j++]  = nibble_to_hex_char(x.v8[i] & 0xF);
  }
  
  bit_string[j] = 0; /* null terminate string */
  return bit_string;
}

char *
v64_hex_string(const v64_t *x) {
  int i, j;

  for (i=j=0; i < 8; i++) {
    bit_string[j++]  = nibble_to_hex_char(x->v8[i] >> 4);
    bit_string[j++]  = nibble_to_hex_char(x->v8[i] & 0xF);
  }
  
  bit_string[j] = 0; /* null terminate string */
  return bit_string;
}

char *
v128_hex_string(v128_t *x) {
  int i, j;

  for (i=j=0; i < 16; i++) {
    bit_string[j++]  = nibble_to_hex_char(x->v8[i] >> 4);
    bit_string[j++]  = nibble_to_hex_char(x->v8[i] & 0xF);
  }
  
  bit_string[j] = 0; /* null terminate string */
  return bit_string;
}

char *
char_to_hex_string(char *x, int num_char) {
  int i, j;

  if (num_char >= 16)
    num_char = 16;
  for (i=j=0; i < num_char; i++) {
    bit_string[j++]  = nibble_to_hex_char(x[i] >> 4);
    bit_string[j++]  = nibble_to_hex_char(x[i] & 0xF);
  }
  
  bit_string[j] = 0; /* null terminate string */
  return bit_string;
}

int
hex_char_to_nibble(uint8_t c) {
  switch(c) {
  case ('0'): return 0x0;
  case ('1'): return 0x1;
  case ('2'): return 0x2;
  case ('3'): return 0x3;
  case ('4'): return 0x4;
  case ('5'): return 0x5;
  case ('6'): return 0x6;
  case ('7'): return 0x7;
  case ('8'): return 0x8;
  case ('9'): return 0x9;
  case ('a'): return 0xa;
  case ('A'): return 0xa;
  case ('b'): return 0xb;
  case ('B'): return 0xb;
  case ('c'): return 0xc;
  case ('C'): return 0xc;
  case ('d'): return 0xd;
  case ('D'): return 0xd;
  case ('e'): return 0xe;
  case ('E'): return 0xe;
  case ('f'): return 0xf;
  case ('F'): return 0xf;
  default: return -1;   /* this flags an error */
  }
  /* NOTREACHED */
  return -1;  /* this keeps compilers from complaining */
}

int
is_hex_string(char *s) {
  while(*s != 0)
    if (hex_char_to_nibble(*s++) == -1)
      return 0;
  return 1;
}

uint8_t
hex_string_to_octet(char *s) {
  uint8_t x;

  x = (hex_char_to_nibble(s[0]) << 4)
    | hex_char_to_nibble(s[1] & 0xFF);
  
  return x;
}

/*
 * hex_string_to_octet_string converts a hexadecimal string
 * of length 2 * len to a raw octet string of length len
 */

int
hex_string_to_octet_string(char *raw, char *hex, int len) {
  uint8_t x;
  int tmp;
  int hex_len;

  hex_len = 0;
  while (hex_len < len) {
    tmp = hex_char_to_nibble(hex[0]);
    if (tmp == -1)
      return hex_len;
    x = (tmp << 4);
    hex_len++;
    tmp = hex_char_to_nibble(hex[1]);
    if (tmp == -1)
      return hex_len;
    x |= (tmp & 0xff);
    hex_len++;
    *raw++ = x;
    hex += 2;
  }
  return hex_len;
}

v16_t
hex_string_to_v16(char *s) {
  v16_t x;
  int i, j;

  for (i=j=0; i < 4; i += 2, j++) {
    x.v8[j] = (hex_char_to_nibble(s[i]) << 4)
      | hex_char_to_nibble(s[i+1] & 0xFF);
  }
  return x;
}

v32_t
hex_string_to_v32(char *s) {
  v32_t x;
  int i, j;

  for (i=j=0; i < 8; i += 2, j++) {
    x.v8[j] = (hex_char_to_nibble(s[i]) << 4)
      | hex_char_to_nibble(s[i+1] & 0xFF);
  }
  return x;
}

v64_t
hex_string_to_v64(char *s) {
  v64_t x;
  int i, j;

  for (i=j=0; i < 16; i += 2, j++) {
    x.v8[j] = (hex_char_to_nibble(s[i]) << 4)
      | hex_char_to_nibble(s[i+1] & 0xFF);
  }
  return x;
}

v128_t
hex_string_to_v128(char *s) {
  v128_t x;
  int i, j;

  for (i=j=0; i < 32; i += 2, j++) {
    x.v8[j] = (hex_char_to_nibble(s[i]) << 4)
      | hex_char_to_nibble(s[i+1] & 0xFF);
  }
  return x;
}



/* 
 * the matrix A[] is stored in column format, i.e., A[i] is the ith
 * column of the matrix 
 */

uint8_t 
A_times_x_plus_b(uint8_t A[8], uint8_t x, uint8_t b) {
  int index = 0;
  unsigned mask;
  
  for (mask=1; mask < 256; mask *= 2) {
    if (x & mask)
      b^= A[index];
    ++index;
  }

  return b;
}

void
v16_copy_octet_string(v16_t *x, const uint8_t s[2]) {
  x->v8[0]  = s[0];
  x->v8[1]  = s[1];
}

void
v32_copy_octet_string(v32_t *x, const uint8_t s[4]) {
  x->v8[0]  = s[0];
  x->v8[1]  = s[1];
  x->v8[2]  = s[2];
  x->v8[3]  = s[3];
}

void
v64_copy_octet_string(v64_t *x, const uint8_t s[8]) {
  x->v8[0]  = s[0];
  x->v8[1]  = s[1];
  x->v8[2]  = s[2];
  x->v8[3]  = s[3];
  x->v8[4]  = s[4];
  x->v8[5]  = s[5];
  x->v8[6]  = s[6];
  x->v8[7]  = s[7];
}

void
v128_copy_octet_string(v128_t *x, const uint8_t s[16]) {
  x->v8[0]  = s[0];
  x->v8[1]  = s[1];
  x->v8[2]  = s[2];
  x->v8[3]  = s[3];
  x->v8[4]  = s[4];
  x->v8[5]  = s[5];
  x->v8[6]  = s[6];
  x->v8[7]  = s[7];
  x->v8[8]  = s[8];
  x->v8[9]  = s[9];
  x->v8[10] = s[10];
  x->v8[11] = s[11];
  x->v8[12] = s[12];
  x->v8[13] = s[13];
  x->v8[14] = s[14];
  x->v8[15] = s[15];

}

#ifndef DATATYPES_USE_MACROS /* little functions are not macros */

void
v128_set_to_zero(v128_t *x) {
  _v128_set_to_zero(x);
}

void
v128_copy(v128_t *x, const v128_t *y) {
  _v128_copy(x, y);
}

void
v128_xor(v128_t *z, v128_t *x, v128_t *y) {
  _v128_xor(z, x, y);
} 

void
v128_and(v128_t *z, v128_t *x, v128_t *y) {
  _v128_and(z, x, y);
}

void
v128_or(v128_t *z, v128_t *x, v128_t *y) {
  _v128_or(z, x, y);
}

void
v128_complement(v128_t *x) {
  _v128_complement(x);
}

int
v128_is_eq(const v128_t *x, const v128_t *y) {
  return _v128_is_eq(x, y);
}

int
v128_get_bit(const v128_t *x, int i) {
  return _v128_get_bit(x, i);
}

void
v128_set_bit(v128_t *x, int i) {
  _v128_set_bit(x, i);
}     

void
v128_clear_bit(v128_t *x, int i){
  _v128_clear_bit(x, i);
}    

void
v128_set_bit_to(v128_t *x, int i, int y){
  _v128_set_bit_to(x, i, y);
}


#endif /* DATATYPES_USE_MACROS */


static inline void
v128_left_shift2(v128_t *x, int num_bits) {
  int i;
  int word_shift = num_bits >> 5;
  int bit_shift  = num_bits & 31;

  for (i=0; i < (4-word_shift); i++) {
    x->v32[i] = x->v32[i+word_shift] << bit_shift;
  }
  
  for (   ; i < word_shift; i++) {
    x->v32[i] = 0;
  }
  
}

void
v128_right_shift(v128_t *x, int index) {
  const int base_index = index >> 5;
  const int bit_index = index & 31;
  int i, from;
  uint32_t b;
    
  if (index > 127) {
    v128_set_to_zero(x);
    return;
  }

  if (bit_index == 0) {

    /* copy each word from left size to right side */
    x->v32[4-1] = x->v32[4-1-base_index];
    for (i=4-1; i > base_index; i--) 
      x->v32[i-1] = x->v32[i-1-base_index];

  } else {
    
    /* set each word to the "or" of the two bit-shifted words */
    for (i = 4; i > base_index; i--) {
      from = i-1 - base_index;
      b = x->v32[from] << bit_index;
      if (from > 0)
        b |= x->v32[from-1] >> (32-bit_index);
      x->v32[i-1] = b;
    }
    
  }

  /* now wrap up the final portion */
  for (i=0; i < base_index; i++) 
    x->v32[i] = 0;
  
}

void
v128_left_shift(v128_t *x, int index) {
  int i;
  const int base_index = index >> 5;
  const int bit_index = index & 31;

  if (index > 127) {
    v128_set_to_zero(x);
    return;
  } 
  
  if (bit_index == 0) {
    for (i=0; i < 4 - base_index; i++)
      x->v32[i] = x->v32[i+base_index];
  } else {
    for (i=0; i < 4 - base_index - 1; i++)
      x->v32[i] = (x->v32[i+base_index] << bit_index) ^
	(x->v32[i+base_index+1] >> (32 - bit_index));
    x->v32[4 - base_index-1] = x->v32[4-1] << bit_index;
  }

  /* now wrap up the final portion */
  for (i = 4 - base_index; i < 4; i++) 
    x->v32[i] = 0;

}


#if 0
void
v128_add(v128_t *z, v128_t *x, v128_t *y) {
  /* integer addition modulo 2^128    */

#ifdef WORDS_BIGENDIAN
  uint64_t tmp;
    
  tmp = x->v32[3] + y->v32[3];
  z->v32[3] = (uint32_t) tmp;
  
  tmp =  x->v32[2] + y->v32[2] + (tmp >> 32);
  z->v32[2] = (uint32_t) tmp;

  tmp =  x->v32[1] + y->v32[1] + (tmp >> 32);
  z->v32[1] = (uint32_t) tmp;
  
  tmp =  x->v32[0] + y->v32[0] + (tmp >> 32);
  z->v32[0] = (uint32_t) tmp;

#else /* assume little endian architecture */
  uint64_t tmp;
  
  tmp = htonl(x->v32[3]) + htonl(y->v32[3]);
  z->v32[3] = ntohl((uint32_t) tmp);
  
  tmp =  htonl(x->v32[2]) + htonl(y->v32[2]) + htonl(tmp >> 32);
  z->v32[2] = ntohl((uint32_t) tmp);

  tmp =  htonl(x->v32[1]) + htonl(y->v32[1]) + htonl(tmp >> 32);
  z->v32[1] = ntohl((uint32_t) tmp);
  
  tmp =  htonl(x->v32[0]) + htonl(y->v32[0]) + htonl(tmp >> 32);
  z->v32[0] = ntohl((uint32_t) tmp);

#endif /* WORDS_BIGENDIAN */
  
}
#endif

int
octet_string_is_eq(uint8_t *a, uint8_t *b, int len) {
  uint8_t *end = b + len;
  while (b < end)
    if (*a++ != *b++)
      return 1;
  return 0;
}

void
octet_string_set_to_zero(uint8_t *s, int len) {
  uint8_t *end = s + len;

  do {
    *s = 0;
  } while (++s < end);
  
}


/* functions below not yet tested! */

int
v32_low_bit(v32_t *w) {
  int value;

  value = low_bit[w->v8[0]];
  if (value != -1)
    return value;
  value = low_bit[w->v8[1]];
  if (value != -1)
    return value + 8;
  value = low_bit[w->v8[2]];
  if (value != -1)
    return value + 16;
  value = low_bit[w->v8[3]];
  if (value == -1)
    return -1;
  return value + 24;
}

/* high_bit not done yet */





