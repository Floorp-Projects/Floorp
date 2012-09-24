/*
 * xfm.c
 *
 * Crypto transform implementation
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

#include "cryptoalg.h"
#include "aes_cbc.h"
#include "hmac.h"
#include "crypto_kernel.h"   /* for crypto_get_random() */

#define KEY_LEN     16
#define ENC_KEY_LEN 16
#define MAC_KEY_LEN 16
#define IV_LEN      16
#define TAG_LEN     12
#define MAX_EXPAND  27

err_status_t
aes_128_cbc_hmac_sha1_96_func(void *key,            
			      void *clear,          
			      unsigned clear_len,       
			      void *iv,             
			      void *opaque,         
			      unsigned *opaque_len, 
			      void *auth_tag) {
  aes_cbc_ctx_t aes_ctx;
  hmac_ctx_t hmac_ctx;
  unsigned char enc_key[ENC_KEY_LEN];
  unsigned char mac_key[MAC_KEY_LEN];
  err_status_t status;

  /* check if we're doing authentication only */
  if ((iv == NULL) && (opaque == NULL) && (opaque_len == NULL)) {
      
      /* perform authentication only */

  } else if ((iv == NULL) || (opaque == NULL) || (opaque_len == NULL)) {
    
    /*
     * bad parameter - we expect either all three pointers to be NULL,
     * or none of those pointers to be NULL 
     */
    return err_status_fail;

  } else {

    /* derive encryption and authentication keys from the input key */
    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "ENC", 3, ENC_KEY_LEN, enc_key);
    if (status) return status;

    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "MAC", 3, MAC_KEY_LEN, mac_key);
    if (status) return status;


    /* perform encryption and authentication */

    /* set aes key */
    status = aes_cbc_context_init(&aes_ctx, key, ENC_KEY_LEN, direction_encrypt);
    if (status) return status;

    /* set iv */
    status = crypto_get_random(iv, IV_LEN);  
    if (status) return status; 
    status = aes_cbc_set_iv(&aes_ctx, iv);
    
    /* encrypt the opaque data  */
    status = aes_cbc_nist_encrypt(&aes_ctx, opaque, opaque_len);
    if (status) return status;

    /* authenticate clear and opaque data */
    status = hmac_init(&hmac_ctx, mac_key, MAC_KEY_LEN);
    if (status) return status;

    status = hmac_start(&hmac_ctx);
    if (status) return status;

    status = hmac_update(&hmac_ctx, clear, clear_len);
    if (status) return status;

    status = hmac_compute(&hmac_ctx, opaque, *opaque_len, TAG_LEN, auth_tag);
    if (status) return status;

  }

  return err_status_ok;
}

err_status_t
aes_128_cbc_hmac_sha1_96_inv(void *key,            
			     void *clear,          
			     unsigned clear_len,       
			     void *iv,             
			     void *opaque,         
			     unsigned *opaque_len, 
			     void *auth_tag) {
  aes_cbc_ctx_t aes_ctx;
  hmac_ctx_t hmac_ctx;
  unsigned char enc_key[ENC_KEY_LEN];
  unsigned char mac_key[MAC_KEY_LEN];
  unsigned char tmp_tag[TAG_LEN];
  unsigned char *tag = auth_tag;
  err_status_t status;
  int i;
  
  /* check if we're doing authentication only */
  if ((iv == NULL) && (opaque == NULL) && (opaque_len == NULL)) {
      
      /* perform authentication only */

  } else if ((iv == NULL) || (opaque == NULL) || (opaque_len == NULL)) {
    
    /*
     * bad parameter - we expect either all three pointers to be NULL,
     * or none of those pointers to be NULL 
     */
    return err_status_fail;

  } else {

    /* derive encryption and authentication keys from the input key */
    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "ENC", 3, ENC_KEY_LEN, enc_key);
    if (status) return status;

    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "MAC", 3, MAC_KEY_LEN, mac_key);
    if (status) return status;

    /* perform encryption and authentication */

    /* set aes key */
    status = aes_cbc_context_init(&aes_ctx, key, ENC_KEY_LEN, direction_decrypt);
    if (status) return status;

    /* set iv */
    status = rand_source_get_octet_string(iv, IV_LEN);  
    if (status) return status; 
    status = aes_cbc_set_iv(&aes_ctx, iv);
    
    /* encrypt the opaque data  */
    status = aes_cbc_nist_decrypt(&aes_ctx, opaque, opaque_len);
    if (status) return status;

    /* authenticate clear and opaque data */
    status = hmac_init(&hmac_ctx, mac_key, MAC_KEY_LEN);
    if (status) return status;

    status = hmac_start(&hmac_ctx);
    if (status) return status;

    status = hmac_update(&hmac_ctx, clear, clear_len);
    if (status) return status;

    status = hmac_compute(&hmac_ctx, opaque, *opaque_len, TAG_LEN, tmp_tag);
    if (status) return status;

    /* compare the computed tag with the one provided as input */
    for (i=0; i < TAG_LEN; i++)
      if (tmp_tag[i] != tag[i]) 
	return err_status_auth_fail; 

  }

  return err_status_ok;
}


#define ENC 1

#define DEBUG 0

err_status_t
aes_128_cbc_hmac_sha1_96_enc(void *key,            
			     const void *clear,          
			     unsigned clear_len,       
			     void *iv,             
			     void *opaque,         
			     unsigned *opaque_len) {
  aes_cbc_ctx_t aes_ctx;
  hmac_ctx_t hmac_ctx;
  unsigned char enc_key[ENC_KEY_LEN];
  unsigned char mac_key[MAC_KEY_LEN];
  unsigned char *auth_tag;
  err_status_t status;

  /* check if we're doing authentication only */
  if ((iv == NULL) && (opaque == NULL) && (opaque_len == NULL)) {
      
      /* perform authentication only */

  } else if ((iv == NULL) || (opaque == NULL) || (opaque_len == NULL)) {
    
    /*
     * bad parameter - we expect either all three pointers to be NULL,
     * or none of those pointers to be NULL 
     */
    return err_status_fail;

  } else {

#if DEBUG
    printf("ENC using key %s\n", octet_string_hex_string(key, KEY_LEN));
#endif

    /* derive encryption and authentication keys from the input key */
    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "ENC", 3, ENC_KEY_LEN, enc_key);
    if (status) return status;

    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "MAC", 3, MAC_KEY_LEN, mac_key);
    if (status) return status;


    /* perform encryption and authentication */

    /* set aes key */
    status = aes_cbc_context_init(&aes_ctx, key, ENC_KEY_LEN, direction_encrypt);
    if (status) return status;

    /* set iv */
    status = rand_source_get_octet_string(iv, IV_LEN);  
    if (status) return status; 
    status = aes_cbc_set_iv(&aes_ctx, iv);
    if (status) return status;

#if DEBUG
    printf("plaintext len:  %d\n", *opaque_len);
    printf("iv:         %s\n", octet_string_hex_string(iv, IV_LEN));
    printf("plaintext:  %s\n", octet_string_hex_string(opaque, *opaque_len));
#endif

#if ENC    
    /* encrypt the opaque data  */
    status = aes_cbc_nist_encrypt(&aes_ctx, opaque, opaque_len);
    if (status) return status;
#endif

#if DEBUG
    printf("ciphertext len: %d\n", *opaque_len);
    printf("ciphertext: %s\n", octet_string_hex_string(opaque, *opaque_len));
#endif

    /*
     * authenticate clear and opaque data, then write the
     * authentication tag to the location immediately following the
     * ciphertext
     */
    status = hmac_init(&hmac_ctx, mac_key, MAC_KEY_LEN);
    if (status) return status;

    status = hmac_start(&hmac_ctx);
    if (status) return status;

    status = hmac_update(&hmac_ctx, clear, clear_len);
    if (status) return status;
#if DEBUG
    printf("hmac input: %s\n", 
	   octet_string_hex_string(clear, clear_len));
#endif
    auth_tag = (unsigned char *)opaque;
    auth_tag += *opaque_len;    
    status = hmac_compute(&hmac_ctx, opaque, *opaque_len, TAG_LEN, auth_tag);
    if (status) return status;
#if DEBUG
    printf("hmac input: %s\n", 
	   octet_string_hex_string(opaque, *opaque_len));
#endif
    /* bump up the opaque_len to reflect the authentication tag */
    *opaque_len += TAG_LEN;

#if DEBUG
    printf("prot data len:  %d\n", *opaque_len);
    printf("prot data: %s\n", octet_string_hex_string(opaque, *opaque_len));
#endif
  }

  return err_status_ok;
}

err_status_t
aes_128_cbc_hmac_sha1_96_dec(void *key,            
			     const void *clear,          
			     unsigned clear_len,       
			     void *iv,             
			     void *opaque,         
			     unsigned *opaque_len) {
  aes_cbc_ctx_t aes_ctx;
  hmac_ctx_t hmac_ctx;
  unsigned char enc_key[ENC_KEY_LEN];
  unsigned char mac_key[MAC_KEY_LEN];
  unsigned char tmp_tag[TAG_LEN];
  unsigned char *auth_tag;
  unsigned ciphertext_len;
  err_status_t status;
  int i;
  
  /* check if we're doing authentication only */
  if ((iv == NULL) && (opaque == NULL) && (opaque_len == NULL)) {
      
      /* perform authentication only */

  } else if ((iv == NULL) || (opaque == NULL) || (opaque_len == NULL)) {
    
    /*
     * bad parameter - we expect either all three pointers to be NULL,
     * or none of those pointers to be NULL 
     */
    return err_status_fail;

  } else {
#if DEBUG
    printf("DEC using key %s\n", octet_string_hex_string(key, KEY_LEN));
#endif

    /* derive encryption and authentication keys from the input key */
    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "ENC", 3, ENC_KEY_LEN, enc_key);
    if (status) return status;

    status = hmac_init(&hmac_ctx, key, KEY_LEN);
    if (status) return status;
    status = hmac_compute(&hmac_ctx, "MAC", 3, MAC_KEY_LEN, mac_key);
    if (status) return status;

#if DEBUG
    printf("prot data len:  %d\n", *opaque_len);
    printf("prot data: %s\n", octet_string_hex_string(opaque, *opaque_len));
#endif

    /* 
     * set the protected data length to that of the ciphertext, by
     * subtracting out the length of the authentication tag 
     */
    ciphertext_len = *opaque_len - TAG_LEN;

#if DEBUG
    printf("ciphertext len: %d\n", ciphertext_len);
#endif    
    /* verify the authentication tag */

    /* 
     * compute the authentication tag for the clear and opaque data,
     * and write it to a temporary location
     */
    status = hmac_init(&hmac_ctx, mac_key, MAC_KEY_LEN);
    if (status) return status;

    status = hmac_start(&hmac_ctx);
    if (status) return status;

    status = hmac_update(&hmac_ctx, clear, clear_len);
    if (status) return status;

#if DEBUG
    printf("hmac input: %s\n", 
	   octet_string_hex_string(clear, clear_len));
#endif

    status = hmac_compute(&hmac_ctx, opaque, ciphertext_len, TAG_LEN, tmp_tag);
    if (status) return status;

#if DEBUG
    printf("hmac input: %s\n", 
	   octet_string_hex_string(opaque, ciphertext_len));
#endif

    /* 
     * compare the computed tag with the one provided as input (which
     * immediately follows the ciphertext)
     */
    auth_tag = (unsigned char *)opaque;
    auth_tag += ciphertext_len;  
#if DEBUG
    printf("auth_tag: %s\n", octet_string_hex_string(auth_tag, TAG_LEN));
    printf("tmp_tag:  %s\n", octet_string_hex_string(tmp_tag, TAG_LEN));
#endif
    for (i=0; i < TAG_LEN; i++) {
      if (tmp_tag[i] != auth_tag[i]) 
	return err_status_auth_fail; 
    }

    /* bump down the opaque_len to reflect the authentication tag */
    *opaque_len -= TAG_LEN;

    /* decrypt the confidential data */
    status = aes_cbc_context_init(&aes_ctx, key, ENC_KEY_LEN, direction_decrypt);
    if (status) return status;
    status = aes_cbc_set_iv(&aes_ctx, iv);
    if (status) return status;

#if DEBUG
    printf("ciphertext: %s\n", octet_string_hex_string(opaque, *opaque_len));
    printf("iv:         %s\n", octet_string_hex_string(iv, IV_LEN));
#endif

#if ENC
    status = aes_cbc_nist_decrypt(&aes_ctx, opaque, &ciphertext_len);
    if (status) return status;
#endif

#if DEBUG
    printf("plaintext len:  %d\n", ciphertext_len);
    printf("plaintext:  %s\n", 
	   octet_string_hex_string(opaque, ciphertext_len));
#endif

    /* indicate the length of the plaintext  */
    *opaque_len = ciphertext_len;
  }

  return err_status_ok;
}

cryptoalg_ctx_t cryptoalg_ctx = {
  aes_128_cbc_hmac_sha1_96_enc,
  aes_128_cbc_hmac_sha1_96_dec,
  KEY_LEN,
  IV_LEN,
  TAG_LEN,
  MAX_EXPAND,
};

cryptoalg_t cryptoalg = &cryptoalg_ctx;

#define NULL_TAG_LEN 12

err_status_t
null_enc(void *key,            
	 const void *clear,          
	 unsigned clear_len,       
	 void *iv,             
	 void *opaque,         
	 unsigned *opaque_len) {
  int i;
  unsigned char *auth_tag;
  unsigned char *init_vec = iv;

  /* check if we're doing authentication only */
  if ((iv == NULL) && (opaque == NULL) && (opaque_len == NULL)) {
      
      /* perform authentication only */

  } else if ((iv == NULL) || (opaque == NULL) || (opaque_len == NULL)) {
    
    /*
     * bad parameter - we expect either all three pointers to be NULL,
     * or none of those pointers to be NULL 
     */
    return err_status_fail;

  } else {

#if DEBUG
    printf("NULL ENC using key %s\n", octet_string_hex_string(key, KEY_LEN));
    printf("NULL_TAG_LEN:  %d\n", NULL_TAG_LEN);
    printf("plaintext len:  %d\n", *opaque_len);
#endif
    for (i=0; i < IV_LEN; i++)
      init_vec[i] = i + (i * 16);
#if DEBUG
    printf("iv:                %s\n", 
	   octet_string_hex_string(iv, IV_LEN));
    printf("plaintext:         %s\n", 
	   octet_string_hex_string(opaque, *opaque_len));
#endif
    auth_tag = opaque;
    auth_tag += *opaque_len;
    for (i=0; i < NULL_TAG_LEN; i++)
      auth_tag[i] = i + (i * 16);
    *opaque_len += NULL_TAG_LEN;
#if DEBUG
    printf("protected data len: %d\n", *opaque_len);
    printf("protected data:    %s\n", 
	   octet_string_hex_string(opaque, *opaque_len));
#endif

  }

  return err_status_ok;
}

err_status_t
null_dec(void *key,            
	 const void *clear,          
	 unsigned clear_len,       
	 void *iv,             
	 void *opaque,         
	 unsigned *opaque_len) {
  unsigned char *auth_tag;
  
  /* check if we're doing authentication only */
  if ((iv == NULL) && (opaque == NULL) && (opaque_len == NULL)) {
      
      /* perform authentication only */

  } else if ((iv == NULL) || (opaque == NULL) || (opaque_len == NULL)) {
    
    /*
     * bad parameter - we expect either all three pointers to be NULL,
     * or none of those pointers to be NULL 
     */
    return err_status_fail;

  } else {

#if DEBUG
    printf("NULL DEC using key %s\n", octet_string_hex_string(key, KEY_LEN));

    printf("protected data len: %d\n", *opaque_len);
    printf("protected data:    %s\n", 
	   octet_string_hex_string(opaque, *opaque_len));
#endif
    auth_tag = opaque;
    auth_tag += (*opaque_len - NULL_TAG_LEN);
#if DEBUG
    printf("iv:         %s\n", octet_string_hex_string(iv, IV_LEN));
#endif
    *opaque_len -= NULL_TAG_LEN;
#if DEBUG
    printf("plaintext len:  %d\n", *opaque_len);
    printf("plaintext:  %s\n", 
	   octet_string_hex_string(opaque, *opaque_len));
#endif
  }

  return err_status_ok;
}

cryptoalg_ctx_t null_cryptoalg_ctx = {
  null_enc,
  null_dec,
  KEY_LEN,
  IV_LEN,
  NULL_TAG_LEN,
  MAX_EXPAND,
};

cryptoalg_t null_cryptoalg = &null_cryptoalg_ctx;

int
cryptoalg_get_id(cryptoalg_t c) {
  if (c == cryptoalg)
    return 1;
  return 0;
}

cryptoalg_t 
cryptoalg_find_by_id(int id) {
  switch(id) {
  case 1:
    return cryptoalg;
  default:
    break;
  }
  return 0;
}
