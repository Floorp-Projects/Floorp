/*
 * cryptoalg.h
 *
 * API for authenticated encryption crypto algorithms
 * 
 * David A. McGrew
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

#ifndef CRYPTOALG_H
#define CRYPTOALG_H

#include "err.h"

/**
 * @defgroup Crypto Cryptography
 *
 * Zed uses a simple interface to a cryptographic transform.
 *
 * @{
 */

/**
 * @brief applies a crypto algorithm
 *
 * The function pointer cryptoalg_func_t points to a function that
 * implements a crypto transform, and provides a uniform API for
 * accessing crypto mechanisms.
 * 
 * @param key       location of secret key                  
 *
 * @param clear     data to be authenticated but not encrypted           
 *
 * @param clear_len length of data to be authenticated but not encrypted
 *
 * @param iv        location to write the Initialization Vector (IV)
 *
 * @param protect   location of the data to be encrypted and
 * authenticated (before the function call), and the ciphertext
 * and authentication tag (after the call)
 *
 * @param protected_len location of the length of the data to be
 * encrypted and authenticated (before the function call), and the
 * length of the ciphertext (after the call)
 *
 */
                    
typedef err_status_t (*cryptoalg_func_t) 
     (void *key,            
      const void *clear,          
      unsigned clear_len,   
      void *iv,             
      void *protect,         
      unsigned *protected_len);

typedef 
err_status_t (*cryptoalg_inv_t)
     (void *key,            /* location of secret key                  */
      const void *clear,     /* data to be authenticated only           */
      unsigned clear_len,   /* length of data to be authenticated only */
      void *iv,             /* location of iv                          */
      void *opaque,         /* data to be decrypted and authenticated  */
      unsigned *opaque_len  /* location of the length of data to be
			     * decrypted and authd (before and after) 
			     */
      );

typedef struct cryptoalg_ctx_t {
  cryptoalg_func_t enc;
  cryptoalg_inv_t  dec;
  unsigned key_len;
  unsigned iv_len;
  unsigned auth_tag_len;
  unsigned max_expansion; 
} cryptoalg_ctx_t;

typedef cryptoalg_ctx_t *cryptoalg_t;

#define cryptoalg_get_key_len(cryptoalg) ((cryptoalg)->key_len)

#define cryptoalg_get_iv_len(cryptoalg) ((cryptoalg)->iv_len)

#define cryptoalg_get_auth_tag_len(cryptoalg) ((cryptoalg)->auth_tag_len)

int
cryptoalg_get_id(cryptoalg_t c);

cryptoalg_t 
cryptoalg_find_by_id(int id);


/**
 * @}
 */

#endif /* CRYPTOALG_H */


