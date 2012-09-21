/*
 * aes_cbc.h
 *
 * Header for AES Cipher Blobk Chaining Mode.
 *
 * David A. McGrew
 * Cisco Systems, Inc.
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

