/*
 * auth.h
 *
 * common interface to authentication functions
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

#ifndef AUTH_H
#define AUTH_H

#include "datatypes.h"          
#include "err.h"                /* error codes    */
#include "crypto.h"		/* for auth_type_id_t */
#include "crypto_types.h"	/* for values of auth_type_id_t */

typedef struct auth_type_t *auth_type_pointer;
typedef struct auth_t      *auth_pointer_t;

typedef err_status_t (*auth_alloc_func)
     (auth_pointer_t *ap, int key_len, int out_len);

typedef err_status_t (*auth_init_func)
     (void *state, const uint8_t *key, int key_len);

typedef err_status_t (*auth_dealloc_func)(auth_pointer_t ap);

typedef err_status_t (*auth_compute_func)
     (void *state, uint8_t *buffer, int octets_to_auth, 
      int tag_len, uint8_t *tag);

typedef err_status_t (*auth_update_func)
     (void *state, uint8_t *buffer, int octets_to_auth);

typedef err_status_t (*auth_start_func)(void *state);
     
/* some syntactic sugar on these function types */

#define auth_type_alloc(at, a, klen, outlen)                        \
                 ((at)->alloc((a), (klen), (outlen)))

#define auth_init(a, key)                                           \
                 (((a)->type)->init((a)->state, (key), ((a)->key_len)))

#define auth_compute(a, buf, len, res)                              \
       (((a)->type)->compute((a)->state, (buf), (len), (a)->out_len, (res)))

#define auth_update(a, buf, len)                                    \
       (((a)->type)->update((a)->state, (buf), (len)))

#define auth_start(a)(((a)->type)->start((a)->state))

#define auth_dealloc(c) (((c)->type)->dealloc(c))

/* functions to get information about a particular auth_t */

int
auth_get_key_length(const struct auth_t *a);

int
auth_get_tag_length(const struct auth_t *a);

int
auth_get_prefix_length(const struct auth_t *a);

/*
 * auth_test_case_t is a (list of) key/message/tag values that are
 * known to be correct for a particular cipher.  this data can be used
 * to test an implementation in an on-the-fly self test of the
 * correcness of the implementation.  (see the auth_type_self_test()
 * function below)
 */

typedef struct auth_test_case_t {
  int key_length_octets;                    /* octets in key            */
  uint8_t *key;                             /* key                      */
  int data_length_octets;                   /* octets in data           */ 
  uint8_t *data;                            /* data                     */
  int tag_length_octets;                    /* octets in tag            */
  uint8_t *tag;                             /* tag                      */
  struct auth_test_case_t *next_test_case;  /* pointer to next testcase */
} auth_test_case_t;

/* auth_type_t */

typedef struct auth_type_t {
  auth_alloc_func      alloc;
  auth_dealloc_func    dealloc;
  auth_init_func       init;
  auth_compute_func    compute;
  auth_update_func     update;
  auth_start_func      start;
  char                *description;
  int                  ref_count;
  auth_test_case_t    *test_data;
  debug_module_t      *debug;
  auth_type_id_t       id;
} auth_type_t;

typedef struct auth_t {
  auth_type_t *type;
  void        *state;                   
  int          out_len;           /* length of output tag in octets */
  int          key_len;           /* length of key in octets        */
  int          prefix_len;        /* length of keystream prefix     */
} auth_t;

/* 
 * auth_type_self_test() tests an auth_type against test cases
 * provided in an array of values of key/message/tag that is known to
 * be good
 */

err_status_t
auth_type_self_test(const auth_type_t *at);

/* 
 * auth_type_test() tests an auth_type against external test cases
 * provided in an array of values of key/message/tag that is known to
 * be good
 */

err_status_t
auth_type_test(const auth_type_t *at, const auth_test_case_t *test_data);

/*
 * auth_type_get_ref_count(at) returns the reference count (the number
 * of instantiations) of the auth_type_t at
 */

int
auth_type_get_ref_count(const auth_type_t *at);

#endif /* AUTH_H */
