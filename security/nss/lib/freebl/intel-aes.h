/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Prototypes of the functions defined in the assembler file.  */
void intel_aes_encrypt_init_128(const unsigned char *key, PRUint32 *expanded);
void intel_aes_encrypt_init_192(const unsigned char *key, PRUint32 *expanded);
void intel_aes_encrypt_init_256(const unsigned char *key, PRUint32 *expanded);
void intel_aes_decrypt_init_128(const unsigned char *key, PRUint32 *expanded);
void intel_aes_decrypt_init_192(const unsigned char *key, PRUint32 *expanded);
void intel_aes_decrypt_init_256(const unsigned char *key, PRUint32 *expanded);
SECStatus intel_aes_encrypt_ecb_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_ecb_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_cbc_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_cbc_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_ecb_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_ecb_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_cbc_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_cbc_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_ecb_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_ecb_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_cbc_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_cbc_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);


#define intel_aes_ecb_worker(encrypt, keysize) \
  ((encrypt)						\
   ? ((keysize) == 16 ? intel_aes_encrypt_ecb_128 :	\
      (keysize) == 24 ? intel_aes_encrypt_ecb_192 :	\
      intel_aes_encrypt_ecb_256)			\
   : ((keysize) == 16 ? intel_aes_decrypt_ecb_128 :	\
      (keysize) == 24 ? intel_aes_decrypt_ecb_192 :	\
      intel_aes_decrypt_ecb_256))


#define intel_aes_cbc_worker(encrypt, keysize) \
  ((encrypt)						\
   ? ((keysize) == 16 ? intel_aes_encrypt_cbc_128 :	\
      (keysize) == 24 ? intel_aes_encrypt_cbc_192 :	\
      intel_aes_encrypt_cbc_256)			\
   : ((keysize) == 16 ? intel_aes_decrypt_cbc_128 :	\
      (keysize) == 24 ? intel_aes_decrypt_cbc_192 :	\
      intel_aes_decrypt_cbc_256))


#define intel_aes_init(encrypt, keysize) \
  do {					 			\
      if (encrypt) {			 			\
	  if (keysize == 16)					\
	      intel_aes_encrypt_init_128(key, cx->expandedKey);	\
	  else if (keysize == 24)				\
	      intel_aes_encrypt_init_192(key, cx->expandedKey);	\
	  else							\
	      intel_aes_encrypt_init_256(key, cx->expandedKey);	\
      } else {							\
	  if (keysize == 16)					\
	      intel_aes_decrypt_init_128(key, cx->expandedKey);	\
	  else if (keysize == 24)				\
	      intel_aes_decrypt_init_192(key, cx->expandedKey);	\
	  else							\
	      intel_aes_decrypt_init_256(key, cx->expandedKey);	\
      }								\
  } while (0)
