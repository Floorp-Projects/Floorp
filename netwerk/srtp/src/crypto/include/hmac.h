/*
 * hmac.h
 *
 * interface to hmac auth_type_t
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

#ifndef HMAC_H
#define HMAC_H

#include "auth.h"
#include "sha1.h"

typedef struct {
  uint8_t    opad[64];
  sha1_ctx_t ctx;
  sha1_ctx_t init_ctx;
} hmac_ctx_t;

err_status_t
hmac_alloc(auth_t **a, int key_len, int out_len);

err_status_t
hmac_dealloc(auth_t *a);

err_status_t
hmac_init(hmac_ctx_t *state, const uint8_t *key, int key_len);

err_status_t
hmac_start(hmac_ctx_t *state);

err_status_t
hmac_update(hmac_ctx_t *state, const uint8_t *message, int msg_octets);

err_status_t
hmac_compute(hmac_ctx_t *state, const void *message,
	     int msg_octets, int tag_len, uint8_t *result);


#endif /* HMAC_H */
