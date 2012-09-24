/*
 * aes_icm.h
 *
 * Header for AES Integer Counter Mode.
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 *
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

#ifndef AES_ICM_H
#define AES_ICM_H

#include "aes.h"
#include "cipher.h"

typedef struct {
  v128_t   counter;                /* holds the counter value          */
  v128_t   offset;                 /* initial offset value             */
  v128_t   keystream_buffer;       /* buffers bytes of keystream       */
  aes_expanded_key_t expanded_key; /* the cipher key                   */
  int      bytes_in_buffer;        /* number of unused bytes in buffer */
} aes_icm_ctx_t;


err_status_t
aes_icm_context_init(aes_icm_ctx_t *c,
		     const unsigned char *key,
		     int key_len); 

err_status_t
aes_icm_set_iv(aes_icm_ctx_t *c, void *iv);

err_status_t
aes_icm_encrypt(aes_icm_ctx_t *c,
		unsigned char *buf, unsigned int *bytes_to_encr);

err_status_t
aes_icm_output(aes_icm_ctx_t *c,
	       unsigned char *buf, int bytes_to_output);

err_status_t 
aes_icm_dealloc(cipher_t *c);
 
err_status_t 
aes_icm_encrypt_ismacryp(aes_icm_ctx_t *c, 
			 unsigned char *buf, 
			 unsigned int *enc_len, 
			 int forIsmacryp);
 
err_status_t 
aes_icm_alloc_ismacryp(cipher_t **c, 
		       int key_len, 
		       int forIsmacryp);

#endif /* AES_ICM_H */

