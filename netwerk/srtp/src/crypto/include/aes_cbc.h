/*
 * aes_cbc.h
 *
 * Header for AES Cipher Blobk Chaining Mode.
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

#ifndef AES_CBC_H
#define AES_CBC_H

#include "aes.h"
#include "cipher.h"

typedef struct {
  v128_t   state;                  /* cipher chaining state            */
  v128_t   previous;               /* previous ciphertext block        */
  aes_expanded_key_t expanded_key; /* the cipher key                   */
} aes_cbc_ctx_t;

err_status_t
aes_cbc_set_key(aes_cbc_ctx_t *c,
		const unsigned char *key); 

err_status_t
aes_cbc_encrypt(aes_cbc_ctx_t *c, 
		unsigned char *buf, 
		unsigned int  *bytes_in_data);

err_status_t
aes_cbc_context_init(aes_cbc_ctx_t *c, const uint8_t *key, 
		     int key_len, cipher_direction_t dir);

err_status_t
aes_cbc_set_iv(aes_cbc_ctx_t *c, void *iv);

err_status_t
aes_cbc_nist_encrypt(aes_cbc_ctx_t *c,
		     unsigned char *data, 
		     unsigned int *bytes_in_data);

err_status_t
aes_cbc_nist_decrypt(aes_cbc_ctx_t *c,
		     unsigned char *data, 
		     unsigned int *bytes_in_data);

#endif /* AES_CBC_H */

