/*
 * null_auth.c
 *
 * implements the do-nothing auth algorithm
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


#include "null_auth.h" 
#include "alloc.h"

/* null_auth uses the auth debug module */

extern debug_module_t mod_auth;

err_status_t
null_auth_alloc(auth_t **a, int key_len, int out_len) {
  extern auth_type_t null_auth;
  uint8_t *pointer;

  debug_print(mod_auth, "allocating auth func with key length %d", key_len);
  debug_print(mod_auth, "                          tag length %d", out_len);

  /* allocate memory for auth and null_auth_ctx_t structures */
  pointer = (uint8_t*)crypto_alloc(sizeof(null_auth_ctx_t) + sizeof(auth_t));
  if (pointer == NULL)
    return err_status_alloc_fail;

  /* set pointers */
  *a = (auth_t *)pointer;
  (*a)->type = &null_auth;
  (*a)->state = pointer + sizeof (auth_t);  
  (*a)->out_len = out_len;
  (*a)->prefix_len = out_len;
  (*a)->key_len = key_len;

  /* increment global count of all null_auth uses */
  null_auth.ref_count++;

  return err_status_ok;
}

err_status_t
null_auth_dealloc(auth_t *a) {
  extern auth_type_t null_auth;
  
  /* zeroize entire state*/
  octet_string_set_to_zero((uint8_t *)a, 
			   sizeof(null_auth_ctx_t) + sizeof(auth_t));

  /* free memory */
  crypto_free(a);
  
  /* decrement global count of all null_auth uses */
  null_auth.ref_count--;

  return err_status_ok;
}

err_status_t
null_auth_init(null_auth_ctx_t *state, const uint8_t *key, int key_len) {

  /* accept any length of key, and do nothing */
  
  return err_status_ok;
}

err_status_t
null_auth_compute(null_auth_ctx_t *state, uint8_t *message,
		   int msg_octets, int tag_len, uint8_t *result) {

  return err_status_ok;
}

err_status_t
null_auth_update(null_auth_ctx_t *state, uint8_t *message,
		   int msg_octets) {

  return err_status_ok;
}

err_status_t
null_auth_start(null_auth_ctx_t *state) {
  return err_status_ok;
}

/*
 * auth_type_t - defines description, test case, and null_auth
 * metaobject
 */

/* begin test case 0 */

auth_test_case_t
null_auth_test_case_0 = {
  0,                                       /* octets in key            */
  NULL,                                    /* key                      */
  0,                                       /* octets in data           */ 
  NULL,                                    /* data                     */
  0,                                       /* octets in tag            */
  NULL,                                    /* tag                      */
  NULL                                     /* pointer to next testcase */
};

/* end test case 0 */

char null_auth_description[] = "null authentication function";

auth_type_t
null_auth  = {
  (auth_alloc_func)      null_auth_alloc,
  (auth_dealloc_func)    null_auth_dealloc,
  (auth_init_func)       null_auth_init,
  (auth_compute_func)    null_auth_compute,
  (auth_update_func)     null_auth_update,
  (auth_start_func)      null_auth_start,
  (char *)               null_auth_description,
  (int)                  0,  /* instance count */
  (auth_test_case_t *)   &null_auth_test_case_0,
  (debug_module_t *)     NULL,
  (auth_type_id_t)       NULL_AUTH
};

