/*
 * ctr_prng.c 
 *
 * counter mode based pseudorandom source
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

ctr_prng_t ctr_prng;

err_status_t
ctr_prng_init(rand_source_func_t random_source) {
  uint8_t tmp_key[32];
  err_status_t status;

  /* initialize output count to zero */
  ctr_prng.octet_count = 0;

  /* set random source */
  ctr_prng.rand = random_source;
  
  /* initialize secret key from random source */
  status = random_source(tmp_key, 32);
  if (status) 
    return status;

  /* initialize aes ctr context with random key */
  status = aes_icm_context_init(&ctr_prng.state, tmp_key, 30);
  if (status) 
    return status;

  return err_status_ok;
}

err_status_t
ctr_prng_get_octet_string(void *dest, uint32_t len) {
  err_status_t status;

  /* 
   * if we need to re-initialize the prng, do so now 
   *
   * avoid 32-bit overflows by subtracting instead of adding
   */
  if (ctr_prng.octet_count > MAX_PRNG_OUT_LEN - len) {
    status = ctr_prng_init(ctr_prng.rand);    
    if (status)
      return status;
  }
  ctr_prng.octet_count += len;

  /*
   * write prng output 
   */
  status = aes_icm_output(&ctr_prng.state, (uint8_t*)dest, len);
  if (status)
    return status;
  
  return err_status_ok;
}

err_status_t
ctr_prng_deinit(void) {

  /* nothing */
  
  return err_status_ok;  
}
