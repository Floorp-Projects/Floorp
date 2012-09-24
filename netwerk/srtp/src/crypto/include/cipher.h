/*
 * cipher.h
 *
 * common interface to ciphers
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


#ifndef CIPHER_H
#define CIPHER_H

#include "datatypes.h"          
#include "rdbx.h"               /* for xtd_seq_num_t */
#include "err.h"                /* for error codes  */
#include "crypto.h"		/* for cipher_type_id_t */
#include "crypto_types.h"	/* for values of cipher_type_id_t */


/**
 * @brief cipher_direction_t defines a particular cipher operation. 
 *
 * A cipher_direction_t is an enum that describes a particular cipher
 * operation, i.e. encryption or decryption.  For some ciphers, this
 * distinction does not matter, but for others, it is essential.
 */

typedef enum { 
  direction_encrypt, /**< encryption (convert plaintext to ciphertext) */
  direction_decrypt, /**< decryption (convert ciphertext to plaintext) */
  direction_any      /**< encryption or decryption                     */
} cipher_direction_t;

/*
 * the cipher_pointer and cipher_type_pointer definitions are needed
 * as cipher_t and cipher_type_t are not yet defined
 */

typedef struct cipher_type_t *cipher_type_pointer_t;
typedef struct cipher_t      *cipher_pointer_t;

/*
 *  a cipher_alloc_func_t allocates (but does not initialize) a cipher_t 
 */

typedef err_status_t (*cipher_alloc_func_t)
     (cipher_pointer_t *cp, int key_len);

/* 
 * a cipher_init_func_t [re-]initializes a cipher_t with a given key
 * and direction (i.e., encrypt or decrypt)
 */

typedef err_status_t (*cipher_init_func_t)
(void *state, const uint8_t *key, int key_len, cipher_direction_t dir);

/* a cipher_dealloc_func_t de-allocates a cipher_t */

typedef err_status_t (*cipher_dealloc_func_t)(cipher_pointer_t cp);

/* a cipher_set_segment_func_t sets the segment index of a cipher_t */

typedef err_status_t (*cipher_set_segment_func_t)
     (void *state, xtd_seq_num_t idx);

/* a cipher_encrypt_func_t encrypts data in-place */

typedef err_status_t (*cipher_encrypt_func_t)
     (void *state, uint8_t *buffer, unsigned int *octets_to_encrypt);

/* a cipher_decrypt_func_t decrypts data in-place */

typedef err_status_t (*cipher_decrypt_func_t)
     (void *state, uint8_t *buffer, unsigned int *octets_to_decrypt);

/* 
 * a cipher_set_iv_func_t function sets the current initialization vector
 */

typedef err_status_t (*cipher_set_iv_func_t)
     (cipher_pointer_t cp, void *iv);

/*
 * cipher_test_case_t is a (list of) key, salt, xtd_seq_num_t,
 * plaintext, and ciphertext values that are known to be correct for a
 * particular cipher.  this data can be used to test an implementation
 * in an on-the-fly self test of the correcness of the implementation.
 * (see the cipher_type_self_test() function below)
 */

typedef struct cipher_test_case_t {
  int key_length_octets;                      /* octets in key            */
  uint8_t *key;                               /* key                      */
  uint8_t *idx;                               /* packet index             */
  int plaintext_length_octets;                /* octets in plaintext      */ 
  uint8_t *plaintext;                         /* plaintext                */
  int ciphertext_length_octets;               /* octets in plaintext      */ 
  uint8_t *ciphertext;                        /* ciphertext               */
  struct cipher_test_case_t *next_test_case;  /* pointer to next testcase */
} cipher_test_case_t;

/* cipher_type_t defines the 'metadata' for a particular cipher type */

typedef struct cipher_type_t {
  cipher_alloc_func_t         alloc;
  cipher_dealloc_func_t       dealloc;
  cipher_init_func_t          init;
  cipher_encrypt_func_t       encrypt;
  cipher_encrypt_func_t       decrypt;
  cipher_set_iv_func_t        set_iv;
  char                       *description;
  int                         ref_count;
  cipher_test_case_t         *test_data;
  debug_module_t             *debug;
  cipher_type_id_t            id;
} cipher_type_t;

/*
 * cipher_t defines an instantiation of a particular cipher, with fixed
 * key length, key and salt values
 */

typedef struct cipher_t {
  cipher_type_t *type;
  void          *state;
  int            key_len;
#ifdef FORCE_64BIT_ALIGN
  int            pad;
#endif
} cipher_t;

/* some syntactic sugar on these function types */

#define cipher_type_alloc(ct, c, klen) ((ct)->alloc((c), (klen)))

#define cipher_dealloc(c) (((c)->type)->dealloc(c))

#define cipher_init(c, k, dir) (((c)->type)->init(((c)->state), (k), ((c)->key_len), (dir)))

#define cipher_encrypt(c, buf, len) \
        (((c)->type)->encrypt(((c)->state), (buf), (len)))

#define cipher_decrypt(c, buf, len) \
        (((c)->type)->decrypt(((c)->state), (buf), (len)))

#define cipher_set_iv(c, n)                           \
  ((c) ? (((c)->type)->set_iv(((cipher_pointer_t)(c)->state), (n))) :   \
                                err_status_no_such_op)  

err_status_t
cipher_output(cipher_t *c, uint8_t *buffer, int num_octets_to_output);


/* some bookkeeping functions */

int
cipher_get_key_length(const cipher_t *c);


/* 
 * cipher_type_self_test() tests a cipher against test cases provided in 
 * an array of values of key/xtd_seq_num_t/plaintext/ciphertext 
 * that is known to be good
 */

err_status_t
cipher_type_self_test(const cipher_type_t *ct);


/* 
 * cipher_type_test() tests a cipher against external test cases provided in 
 * an array of values of key/xtd_seq_num_t/plaintext/ciphertext 
 * that is known to be good
 */

err_status_t
cipher_type_test(const cipher_type_t *ct, const cipher_test_case_t *test_data);


/*
 * cipher_bits_per_second(c, l, t) computes (and estimate of) the
 * number of bits that a cipher implementation can encrypt in a second
 * 
 * c is a cipher (which MUST be allocated and initialized already), l
 * is the length in octets of the test data to be encrypted, and t is
 * the number of trials
 *
 * if an error is encountered, then the value 0 is returned
 */

uint64_t
cipher_bits_per_second(cipher_t *c, int octets_in_buffer, int num_trials);

#endif /* CIPHER_H */
