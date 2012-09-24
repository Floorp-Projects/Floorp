/*
 * prng.c 
 *
 * pseudorandom source
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright(c) 2001-2006 Cisco Systems, Inc.
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


#include "prng.h"

/* single, global prng structure */

x917_prng_t x917_prng;

err_status_t
x917_prng_init(rand_source_func_t random_source) {
  uint8_t tmp_key[16];
  err_status_t status;

  /* initialize output count to zero */
  x917_prng.octet_count = 0;

  /* set random source */
  x917_prng.rand = random_source;
  
  /* initialize secret key from random source */
  status = random_source(tmp_key, 16);
  if (status) 
    return status;

  /* expand aes key */
  aes_expand_encryption_key(tmp_key, 16, &x917_prng.key);

  /* initialize prng state from random source */
  status = x917_prng.rand((uint8_t *)&x917_prng.state, 16);
  if (status) 
    return status;

  return err_status_ok;
}

err_status_t
x917_prng_get_octet_string(uint8_t *dest, uint32_t len) {
  uint32_t t;
  v128_t buffer;
  uint32_t i, tail_len;
  err_status_t status;

  /* 
   * if we need to re-initialize the prng, do so now 
   *
   * avoid overflows by subtracting instead of adding
   */
  if (x917_prng.octet_count > MAX_PRNG_OUT_LEN - len) {
    status = x917_prng_init(x917_prng.rand);    
    if (status)
      return status;
  }
  x917_prng.octet_count += len;
  
  /* find out the time */
  t = (uint32_t)time(NULL);
  
  /* loop until we have output enough data */
  for (i=0; i < len/16; i++) {
    
    /* exor time into state */
    x917_prng.state.v32[0] ^= t; 
 
    /* copy state into buffer */
    v128_copy(&buffer, &x917_prng.state);

    /* apply aes to buffer */
    aes_encrypt(&buffer, &x917_prng.key);
    
    /* write data to output */
    *dest++ = buffer.v8[0];
    *dest++ = buffer.v8[1];
    *dest++ = buffer.v8[2];
    *dest++ = buffer.v8[3];
    *dest++ = buffer.v8[4];
    *dest++ = buffer.v8[5];
    *dest++ = buffer.v8[6];
    *dest++ = buffer.v8[7];
    *dest++ = buffer.v8[8];
    *dest++ = buffer.v8[9];
    *dest++ = buffer.v8[10];
    *dest++ = buffer.v8[11];
    *dest++ = buffer.v8[12];
    *dest++ = buffer.v8[13];
    *dest++ = buffer.v8[14];
    *dest++ = buffer.v8[15];

    /* exor time into buffer */
    buffer.v32[0] ^= t;

    /* encrypt buffer */
    aes_encrypt(&buffer, &x917_prng.key);

    /* copy buffer into state */
    v128_copy(&x917_prng.state, &buffer);
    
  }
  
  /* if we need to output any more octets, we'll do so now */
  tail_len = len % 16;
  if (tail_len) {
    
    /* exor time into state */
    x917_prng.state.v32[0] ^= t; 
 
    /* copy value into buffer */
    v128_copy(&buffer, &x917_prng.state);

    /* apply aes to buffer */
    aes_encrypt(&buffer, &x917_prng.key);

    /* write data to output */
    for (i=0; i < tail_len; i++) {
      *dest++ = buffer.v8[i];
    }

    /* now update the state one more time */

    /* exor time into buffer */
    buffer.v32[0] ^= t;

    /* encrypt buffer */
    aes_encrypt(&buffer, &x917_prng.key);

    /* copy buffer into state */
    v128_copy(&x917_prng.state, &buffer);

  }
  
  return err_status_ok;
}

err_status_t
x917_prng_deinit(void) {
  
  return err_status_ok;  
}
