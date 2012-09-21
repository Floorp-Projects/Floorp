/*
 * null_cipher.c
 *
 * A null cipher implementation.  This cipher leaves the plaintext
 * unchanged.
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

#include "datatypes.h"
#include "null_cipher.h"
#include "alloc.h"

/* the null_cipher uses the cipher debug module  */

extern debug_module_t mod_cipher;

err_status_t
null_cipher_alloc(cipher_t **c, int key_len) {
  extern cipher_type_t null_cipher;
  uint8_t *pointer;
  
  debug_print(mod_cipher, 
	      "allocating cipher with key length %d", key_len);

  /* allocate memory a cipher of type null_cipher */
  pointer = (uint8_t*)crypto_alloc(sizeof(null_cipher_ctx_t) + sizeof(cipher_t));
  if (pointer == NULL)
    return err_status_alloc_fail;

  /* set pointers */
  *c = (cipher_t *)pointer;
  (*c)->type = &null_cipher;
  (*c)->state = pointer + sizeof(cipher_t);

  /* set key size */
  (*c)->key_len = key_len;

  /* increment ref_count */
  null_cipher.ref_count++;
  
  return err_status_ok;
  
}

err_status_t
null_cipher_dealloc(cipher_t *c) {
  extern cipher_type_t null_cipher;

  /* zeroize entire state*/
  octet_string_set_to_zero((uint8_t *)c, 
			   sizeof(null_cipher_ctx_t) + sizeof(cipher_t));

  /* free memory of type null_cipher */
  crypto_free(c);

  /* decrement reference count */
  null_cipher.ref_count--;
  
  return err_status_ok;
  
}

err_status_t
null_cipher_init(null_cipher_ctx_t *ctx, const uint8_t *key, int key_len) {

  debug_print(mod_cipher, "initializing null cipher", NULL);

  return err_status_ok;
}

err_status_t
null_cipher_set_iv(null_cipher_ctx_t *c, void *iv) { 
  return err_status_ok;
}

err_status_t
null_cipher_encrypt(null_cipher_ctx_t *c,
		    unsigned char *buf, unsigned int *bytes_to_encr) {
  return err_status_ok;
}

char 
null_cipher_description[] = "null cipher";

cipher_test_case_t  
null_cipher_test_0 = {
  0,                 /* octets in key            */
  NULL,              /* key                      */
  0,                 /* packet index             */
  0,                 /* octets in plaintext      */
  NULL,              /* plaintext                */
  0,                 /* octets in plaintext      */
  NULL,              /* ciphertext               */
  NULL               /* pointer to next testcase */
};


/*
 * note: the decrypt function is idential to the encrypt function
 */

cipher_type_t null_cipher = {
  (cipher_alloc_func_t)         null_cipher_alloc,
  (cipher_dealloc_func_t)       null_cipher_dealloc,
  (cipher_init_func_t)          null_cipher_init,
  (cipher_encrypt_func_t)       null_cipher_encrypt,
  (cipher_decrypt_func_t)       null_cipher_encrypt,
  (cipher_set_iv_func_t)        null_cipher_set_iv,
  (char *)                      null_cipher_description,
  (int)                         0,
  (cipher_test_case_t *)       &null_cipher_test_0,
  (debug_module_t *)            NULL,
  (cipher_type_id_t)            NULL_CIPHER
};

