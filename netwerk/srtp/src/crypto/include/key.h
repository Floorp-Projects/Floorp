/*
 * key.h
 *
 * key usage limits enforcement
 * 
 * David A. Mcgrew
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

#ifndef KEY_H
#define KEY_H

#include "rdbx.h"   /* for xtd_seq_num_t */
#include "err.h"

typedef struct key_limit_ctx_t *key_limit_t;

typedef enum {
   key_event_normal,
   key_event_soft_limit,
   key_event_hard_limit
} key_event_t;

err_status_t
key_limit_set(key_limit_t key, const xtd_seq_num_t s);

err_status_t
key_limit_clone(key_limit_t original, key_limit_t *new_key);

err_status_t
key_limit_check(const key_limit_t key);

key_event_t
key_limit_update(key_limit_t key);

typedef enum { 
   key_state_normal,
   key_state_past_soft_limit,
   key_state_expired
} key_state_t;

typedef struct key_limit_ctx_t {
  xtd_seq_num_t num_left;
  key_state_t   state;
} key_limit_ctx_t;

#endif /* KEY_H */
