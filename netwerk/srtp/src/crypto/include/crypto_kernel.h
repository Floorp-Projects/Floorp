/*
 * crypto_kernel.h
 *
 * header for the cryptographic kernel
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


#ifndef CRYPTO_KERNEL
#define CRYPTO_KERNEL

#include "rand_source.h"       
#include "prng.h"
#include "cipher.h"    
#include "auth.h"
#include "cryptoalg.h"
#include "stat.h"
#include "err.h"
#include "crypto_types.h"
#include "key.h"
#include "crypto.h"

/*
 * crypto_kernel_state_t defines the possible states:
 *
 *    insecure - not yet initialized
 *    secure   - initialized and passed self-tests
 */

typedef enum {
  crypto_kernel_state_insecure,
  crypto_kernel_state_secure
} crypto_kernel_state_t;

/* 
 * linked list of cipher types 
 */

typedef struct kernel_cipher_type {
  cipher_type_id_t  id;
  cipher_type_t    *cipher_type;
  struct kernel_cipher_type *next;
} kernel_cipher_type_t;

/* 
 * linked list of auth types 
 */

typedef struct kernel_auth_type {
  auth_type_id_t  id;
  auth_type_t    *auth_type;
  struct kernel_auth_type *next;
} kernel_auth_type_t;

/*
 * linked list of debug modules 
 */

typedef struct kernel_debug_module {
  debug_module_t *mod;
  struct kernel_debug_module *next;
} kernel_debug_module_t;


/*
 * crypto_kernel_t is the data structure for the crypto kernel
 *
 * note that there is *exactly one* instance of this data type,
 * a global variable defined in crypto_kernel.c
 */

typedef struct {
  crypto_kernel_state_t state;              /* current state of kernel     */
  kernel_cipher_type_t *cipher_type_list;   /* list of all cipher types    */
  kernel_auth_type_t   *auth_type_list;     /* list of all auth func types */
  kernel_debug_module_t *debug_module_list; /* list of all debug modules   */
} crypto_kernel_t;


/*
 * crypto_kernel_t external api
 */


/*
 * The function crypto_kernel_init() initialized the crypto kernel and
 * runs the self-test operations on the random number generators and
 * crypto algorithms.  Possible return values are:
 *
 *    err_status_ok     initialization successful
 *    <other>           init failure 
 *
 * If any value other than err_status_ok is returned, the
 * crypto_kernel MUST NOT be used.  
 */

err_status_t
crypto_kernel_init(void);


/*
 * The function crypto_kernel_shutdown() de-initializes the
 * crypto_kernel, zeroizes keys and other cryptographic material, and
 * deallocates any dynamically allocated memory.  Possible return
 * values are:
 *
 *    err_status_ok     shutdown successful
 *    <other>           shutdown failure 
 *
 */

err_status_t
crypto_kernel_shutdown(void);

/*
 * The function crypto_kernel_stats() checks the the crypto_kernel,
 * running tests on the ciphers, auth funcs, and rng, and prints out a
 * status report.  Possible return values are:
 *
 *    err_status_ok     all tests were passed
 *    <other>           a test failed 
 *
 */

err_status_t
crypto_kernel_status(void);


/*
 * crypto_kernel_list_debug_modules() outputs a list of debugging modules
 *
 */

err_status_t
crypto_kernel_list_debug_modules(void);

/*
 * crypto_kernel_load_cipher_type()
 *
 */

err_status_t
crypto_kernel_load_cipher_type(cipher_type_t *ct, cipher_type_id_t id);

err_status_t
crypto_kernel_load_auth_type(auth_type_t *ct, auth_type_id_t id);

/*
 * crypto_kernel_replace_cipher_type(ct, id)
 * 
 * replaces the crypto kernel's existing cipher for the cipher_type id
 * with a new one passed in externally.  The new cipher must pass all the
 * existing cipher_type's self tests as well as its own.
 */
err_status_t
crypto_kernel_replace_cipher_type(cipher_type_t *ct, cipher_type_id_t id);


/*
 * crypto_kernel_replace_auth_type(ct, id)
 * 
 * replaces the crypto kernel's existing cipher for the auth_type id
 * with a new one passed in externally.  The new auth type must pass all the
 * existing auth_type's self tests as well as its own.
 */
err_status_t
crypto_kernel_replace_auth_type(auth_type_t *ct, auth_type_id_t id);


err_status_t
crypto_kernel_load_debug_module(debug_module_t *new_dm);

/*
 * crypto_kernel_alloc_cipher(id, cp, key_len); 
 *
 * allocates a cipher of type id at location *cp, with key length
 * key_len octets.  Return values are:
 * 
 *    err_status_ok           no problems
 *    err_status_alloc_fail   an allocation failure occured
 *    err_status_fail         couldn't find cipher with identifier 'id'
 */

err_status_t
crypto_kernel_alloc_cipher(cipher_type_id_t id, 
			   cipher_pointer_t *cp, 
			   int key_len);

/*
 * crypto_kernel_alloc_auth(id, ap, key_len, tag_len); 
 *
 * allocates an auth function of type id at location *ap, with key
 * length key_len octets and output tag length of tag_len.  Return
 * values are:
 * 
 *    err_status_ok           no problems
 *    err_status_alloc_fail   an allocation failure occured
 *    err_status_fail         couldn't find auth with identifier 'id'
 */

err_status_t
crypto_kernel_alloc_auth(auth_type_id_t id, 
			 auth_pointer_t *ap, 
			 int key_len,
			 int tag_len);


/*
 * crypto_kernel_set_debug_module(mod_name, v)
 * 
 * sets dynamic debugging to the value v (0 for off, 1 for on) for the
 * debug module with the name mod_name
 *
 * returns err_status_ok on success, err_status_fail otherwise
 */

err_status_t
crypto_kernel_set_debug_module(char *mod_name, int v);

/**
 * @brief writes a random octet string.
 *
 * The function call crypto_get_random(dest, len) writes len octets of
 * random data to the location to which dest points, and returns an
 * error code.  This error code @b must be checked, and if a failure is
 * reported, the data in the buffer @b must @b not be used.
 * 
 * @warning If the return code is not checked, then non-random
 *          data may be in the buffer.  This function will fail
 *          unless it is called after crypto_kernel_init().
 *
 * @return
 *     - err_status_ok    if no problems occured.
 *     - [other]          a problem occured, and no assumptions should
 *                        be made about the contents of the destination
 *                        buffer.
 *
 * @ingroup SRTP
 */
err_status_t
crypto_get_random(unsigned char *buffer, unsigned int length);
     
#endif /* CRYPTO_KERNEL */
