/*
 * aes_cbc.c
 *
 * AES Cipher Block Chaining Mode
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


#include "aes_cbc.h"
#include "alloc.h"

debug_module_t mod_aes_cbc = {
  0,                 /* debugging is off by default */
  "aes cbc"          /* printable module name       */
};



err_status_t
aes_cbc_alloc(cipher_t **c, int key_len) {
  extern cipher_type_t aes_cbc;
  uint8_t *pointer;
  int tmp;

  debug_print(mod_aes_cbc, 
	      "allocating cipher with key length %d", key_len);

  if (key_len != 16 && key_len != 24 && key_len != 32)
    return err_status_bad_param;
  
  /* allocate memory a cipher of type aes_cbc */
  tmp = (sizeof(aes_cbc_ctx_t) + sizeof(cipher_t));
  pointer = (uint8_t*)crypto_alloc(tmp);
  if (pointer == NULL) 
    return err_status_alloc_fail;

  /* set pointers */
  *c = (cipher_t *)pointer;
  (*c)->type = &aes_cbc;
  (*c)->state = pointer + sizeof(cipher_t);

  /* increment ref_count */
  aes_cbc.ref_count++;

  /* set key size        */
  (*c)->key_len = key_len;

  return err_status_ok;  
}

err_status_t
aes_cbc_dealloc(cipher_t *c) {
  extern cipher_type_t aes_cbc;

  /* zeroize entire state*/
  octet_string_set_to_zero((uint8_t *)c, 
			   sizeof(aes_cbc_ctx_t) + sizeof(cipher_t));

  /* free memory */
  crypto_free(c);

  /* decrement ref_count */
  aes_cbc.ref_count--;
  
  return err_status_ok;  
}

err_status_t
aes_cbc_context_init(aes_cbc_ctx_t *c, const uint8_t *key, int key_len,
		     cipher_direction_t dir) {
  err_status_t status;

  debug_print(mod_aes_cbc, 
	      "key:  %s", octet_string_hex_string(key, key_len)); 

  /* expand key for the appropriate direction */
  switch (dir) {
  case (direction_encrypt):
    status = aes_expand_encryption_key(key, key_len, &c->expanded_key);
    if (status)
      return status;
    break;
  case (direction_decrypt):
    status = aes_expand_decryption_key(key, key_len, &c->expanded_key);
    if (status)
      return status;
    break;
  default:
    return err_status_bad_param;
  }


  return err_status_ok;
}


err_status_t
aes_cbc_set_iv(aes_cbc_ctx_t *c, void *iv) {
  int i;
/*   v128_t *input = iv; */
  uint8_t *input = (uint8_t*) iv;
 
  /* set state and 'previous' block to iv */
  for (i=0; i < 16; i++) 
    c->previous.v8[i] = c->state.v8[i] = input[i];

  debug_print(mod_aes_cbc, "setting iv: %s", v128_hex_string(&c->state)); 

  return err_status_ok;
}

err_status_t
aes_cbc_encrypt(aes_cbc_ctx_t *c,
		unsigned char *data, 
		unsigned int *bytes_in_data) {
  int i;
  unsigned char *input  = data;   /* pointer to data being read    */
  unsigned char *output = data;   /* pointer to data being written */
  int bytes_to_encr = *bytes_in_data;

  /*
   * verify that we're 16-octet aligned
   */
  if (*bytes_in_data & 0xf) 
    return err_status_bad_param;

  /*
   * note that we assume that the initialization vector has already
   * been set, e.g. by calling aes_cbc_set_iv()
   */
  debug_print(mod_aes_cbc, "iv: %s", 
	      v128_hex_string(&c->state));
  
  /*
   * loop over plaintext blocks, exoring state into plaintext then
   * encrypting and writing to output
   */
  while (bytes_to_encr > 0) {
    
    /* exor plaintext into state */
    for (i=0; i < 16; i++)
      c->state.v8[i] ^= *input++;

    debug_print(mod_aes_cbc, "inblock:  %s", 
	      v128_hex_string(&c->state));

    aes_encrypt(&c->state, &c->expanded_key);

    debug_print(mod_aes_cbc, "outblock: %s", 
	      v128_hex_string(&c->state));

    /* copy ciphertext to output */
    for (i=0; i < 16; i++)
      *output++ = c->state.v8[i];

    bytes_to_encr -= 16;
  }

  return err_status_ok;
}

err_status_t
aes_cbc_decrypt(aes_cbc_ctx_t *c,
		unsigned char *data, 
		unsigned int *bytes_in_data) {
  int i;
  v128_t state, previous;
  unsigned char *input  = data;   /* pointer to data being read    */
  unsigned char *output = data;   /* pointer to data being written */
  int bytes_to_encr = *bytes_in_data;
  uint8_t tmp;

  /*
   * verify that we're 16-octet aligned
   */
  if (*bytes_in_data & 0x0f)
    return err_status_bad_param;    

  /* set 'previous' block to iv*/
  for (i=0; i < 16; i++) {
    previous.v8[i] = c->previous.v8[i];
  }

  debug_print(mod_aes_cbc, "iv: %s", 
	      v128_hex_string(&previous));
  
  /*
   * loop over ciphertext blocks, decrypting then exoring with state
   * then writing plaintext to output
   */
  while (bytes_to_encr > 0) {
    
    /* set state to ciphertext input block */
    for (i=0; i < 16; i++) {
     state.v8[i] = *input++;
    }

    debug_print(mod_aes_cbc, "inblock:  %s", 
	      v128_hex_string(&state));
    
    /* decrypt state */
    aes_decrypt(&state, &c->expanded_key);

    debug_print(mod_aes_cbc, "outblock: %s", 
	      v128_hex_string(&state));

    /* 
     * exor previous ciphertext block out of plaintext, and write new
     * plaintext block to output, while copying old ciphertext block
     * to the 'previous' block
     */
    for (i=0; i < 16; i++) {
      tmp = *output;
      *output++ = state.v8[i] ^ previous.v8[i];
      previous.v8[i] = tmp;
    }

    bytes_to_encr -= 16;
  }

  return err_status_ok;
}


err_status_t
aes_cbc_nist_encrypt(aes_cbc_ctx_t *c,
		     unsigned char *data, 
		     unsigned int *bytes_in_data) {
  int i;
  unsigned char *pad_start; 
  int num_pad_bytes;
  err_status_t status;

  /* 
   * determine the number of padding bytes that we need to add - 
   * this value is always between 1 and 16, inclusive.
   */
  num_pad_bytes = 16 - (*bytes_in_data & 0xf);
  pad_start = data;
  pad_start += *bytes_in_data;
  *pad_start++ = 0xa0;
  for (i=0; i < num_pad_bytes; i++) 
    *pad_start++ = 0x00;
   
  /* 
   * increment the data size 
   */
  *bytes_in_data += num_pad_bytes;  

  /*
   * now cbc encrypt the padded data 
   */
  status = aes_cbc_encrypt(c, data, bytes_in_data);
  if (status) 
    return status;

  return err_status_ok;
}


err_status_t
aes_cbc_nist_decrypt(aes_cbc_ctx_t *c,
		     unsigned char *data, 
		     unsigned int *bytes_in_data) {
  unsigned char *pad_end;
  int num_pad_bytes;
  err_status_t status;

  /*
   * cbc decrypt the padded data 
   */
  status = aes_cbc_decrypt(c, data, bytes_in_data);
  if (status) 
    return status;

  /*
   * determine the number of padding bytes in the decrypted plaintext
   * - this value is always between 1 and 16, inclusive.
   */
  num_pad_bytes = 1;
  pad_end = data + (*bytes_in_data - 1);
  while (*pad_end != 0xa0) {   /* note: should check padding correctness */
    pad_end--;
    num_pad_bytes++;
  }
  
  /* decrement data size */
  *bytes_in_data -= num_pad_bytes;  

  return err_status_ok;
}


char 
aes_cbc_description[] = "aes cipher block chaining (cbc) mode";

/*
 * Test case 0 is derived from FIPS 197 Appendix C; it uses an
 * all-zero IV, so that the first block encryption matches the test
 * case in that appendix.  This property provides a check of the base
 * AES encryption and decryption algorithms; if CBC fails on some
 * particular platform, then you should print out AES intermediate
 * data and compare with the detailed info provided in that appendix.
 *
 */


uint8_t aes_cbc_test_case_0_key[16] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

uint8_t aes_cbc_test_case_0_plaintext[64] =  {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff 
};

uint8_t aes_cbc_test_case_0_ciphertext[80] = {
  0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 
  0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a,
  0x03, 0x35, 0xed, 0x27, 0x67, 0xf2, 0x6d, 0xf1, 
  0x64, 0x83, 0x2e, 0x23, 0x44, 0x38, 0x70, 0x8b

};

uint8_t aes_cbc_test_case_0_iv[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


cipher_test_case_t aes_cbc_test_case_0 = {
  16,                                    /* octets in key            */
  aes_cbc_test_case_0_key,               /* key                      */
  aes_cbc_test_case_0_iv,                /* initialization vector    */
  16,                                    /* octets in plaintext      */
  aes_cbc_test_case_0_plaintext,         /* plaintext                */
  32,                                    /* octets in ciphertext     */
  aes_cbc_test_case_0_ciphertext,        /* ciphertext               */
  NULL                                   /* pointer to next testcase */
};


/*
 * this test case is taken directly from Appendix F.2 of NIST Special
 * Publication SP 800-38A
 */

uint8_t aes_cbc_test_case_1_key[16] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
};

uint8_t aes_cbc_test_case_1_plaintext[64] =  {
  0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 
  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 
  0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
  0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
  0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
  0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 
  0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

uint8_t aes_cbc_test_case_1_ciphertext[80] = {
  0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
  0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
  0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee,
  0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
  0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b,
  0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16, 
  0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09, 
  0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7,
  0x39, 0x34, 0x07, 0x03, 0x36, 0xd0, 0x77, 0x99, 
  0xe0, 0xc4, 0x2f, 0xdd, 0xa8, 0xdf, 0x4c, 0xa3
};

uint8_t aes_cbc_test_case_1_iv[16] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

cipher_test_case_t aes_cbc_test_case_1 = {
  16,                                    /* octets in key            */
  aes_cbc_test_case_1_key,               /* key                      */
  aes_cbc_test_case_1_iv,                /* initialization vector    */
  64,                                    /* octets in plaintext      */
  aes_cbc_test_case_1_plaintext,         /* plaintext                */
  80,                                    /* octets in ciphertext     */
  aes_cbc_test_case_1_ciphertext,        /* ciphertext               */
  &aes_cbc_test_case_0                    /* pointer to next testcase */
};

/*
 * Test case 2 is like test case 0, but for 256-bit keys. (FIPS 197 
 * appendix C.3).
 */


uint8_t aes_cbc_test_case_2_key[32] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

uint8_t aes_cbc_test_case_2_plaintext[64] =  {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff 
};

uint8_t aes_cbc_test_case_2_ciphertext[80] = {
  0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf,
  0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89,  
  0x72, 0x72, 0x6e, 0xe7, 0x71, 0x39, 0xbf, 0x11,
  0xe5, 0x40, 0xe2, 0x7c, 0x54, 0x65, 0x1d, 0xee
};

uint8_t aes_cbc_test_case_2_iv[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

cipher_test_case_t aes_cbc_test_case_2 = {
  32,                                    /* octets in key            */
  aes_cbc_test_case_2_key,               /* key                      */
  aes_cbc_test_case_2_iv,                /* initialization vector    */
  16,                                    /* octets in plaintext      */
  aes_cbc_test_case_2_plaintext,         /* plaintext                */
  32,                                    /* octets in ciphertext     */
  aes_cbc_test_case_2_ciphertext,        /* ciphertext               */
  &aes_cbc_test_case_1                   /* pointer to next testcase */
};


/*
 * this test case is taken directly from Appendix F.2 of NIST Special
 * Publication SP 800-38A
 */

uint8_t aes_cbc_test_case_3_key[32] = {
  0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
  0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
  0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
  0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};

uint8_t aes_cbc_test_case_3_plaintext[64] =  {
  0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 
  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 
  0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
  0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
  0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
  0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 
  0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

uint8_t aes_cbc_test_case_3_ciphertext[80] = {
  0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba,
  0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
  0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d,
  0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
  0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf,
  0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
  0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc,
  0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b,
  0xfb, 0x98, 0x20, 0x2c, 0x45, 0xb2, 0xe4, 0xa0,
  0x63, 0xc4, 0x68, 0xba, 0x84, 0x39, 0x16, 0x5a
};

uint8_t aes_cbc_test_case_3_iv[16] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

cipher_test_case_t aes_cbc_test_case_3 = {
  32,                                    /* octets in key            */
  aes_cbc_test_case_3_key,               /* key                      */
  aes_cbc_test_case_3_iv,                /* initialization vector    */
  64,                                    /* octets in plaintext      */
  aes_cbc_test_case_3_plaintext,         /* plaintext                */
  80,                                    /* octets in ciphertext     */
  aes_cbc_test_case_3_ciphertext,        /* ciphertext               */
  &aes_cbc_test_case_2                    /* pointer to next testcase */
};

cipher_type_t aes_cbc = {
  (cipher_alloc_func_t)          aes_cbc_alloc,
  (cipher_dealloc_func_t)        aes_cbc_dealloc,  
  (cipher_init_func_t)           aes_cbc_context_init,
  (cipher_encrypt_func_t)        aes_cbc_nist_encrypt,
  (cipher_decrypt_func_t)        aes_cbc_nist_decrypt,
  (cipher_set_iv_func_t)         aes_cbc_set_iv,
  (char *)                       aes_cbc_description,
  (int)                          0,   /* instance count */
  (cipher_test_case_t *)        &aes_cbc_test_case_3,
  (debug_module_t *)            &mod_aes_cbc,
  (cipher_type_id_t)             AES_CBC
};


