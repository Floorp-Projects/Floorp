/*
 * prng.h
 *
 * pseudorandom source
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

#ifndef PRNG_H
#define PRNG_H

#include "rand_source.h"  /* for rand_source_func_t definition       */
#include "aes.h"          /* for aes                                 */
#include "aes_icm.h"      /* for aes ctr                             */

#define MAX_PRNG_OUT_LEN 0xffffffffU

/*
 * x917_prng is an ANSI X9.17-like AES-based PRNG
 */

typedef struct {
  v128_t   state;          /* state data                              */
  aes_expanded_key_t key;  /* secret key                              */
  uint32_t octet_count;    /* number of octets output since last init */
  rand_source_func_t rand; /* random source for re-initialization     */
} x917_prng_t;

err_status_t
x917_prng_init(rand_source_func_t random_source);

err_status_t
x917_prng_get_octet_string(uint8_t *dest, uint32_t len);


/*
 * ctr_prng is an AES-CTR based PRNG
 */

typedef struct {
  uint32_t octet_count;    /* number of octets output since last init */
  aes_icm_ctx_t   state;   /* state data                              */
  rand_source_func_t rand; /* random source for re-initialization     */
} ctr_prng_t;

err_status_t
ctr_prng_init(rand_source_func_t random_source);

err_status_t
ctr_prng_get_octet_string(void *dest, uint32_t len);


#endif
