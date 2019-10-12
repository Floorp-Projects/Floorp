/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

SECStatus arm_aes_encrypt_ecb_128(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_decrypt_ecb_128(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_encrypt_cbc_128(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_decrypt_cbc_128(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_encrypt_ecb_192(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_decrypt_ecb_192(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_encrypt_cbc_192(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_decrypt_cbc_192(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_encrypt_ecb_256(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_decrypt_ecb_256(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_encrypt_cbc_256(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);
SECStatus arm_aes_decrypt_cbc_256(AESContext *cx, unsigned char *output,
                                  unsigned int *outputLen,
                                  unsigned int maxOutputLen,
                                  const unsigned char *input,
                                  unsigned int inputLen,
                                  unsigned int blocksize);

#define native_aes_ecb_worker(encrypt, keysize)                          \
    ((encrypt)                                                           \
         ? ((keysize) == 16 ? arm_aes_encrypt_ecb_128                    \
                            : (keysize) == 24 ? arm_aes_encrypt_ecb_192  \
                                              : arm_aes_encrypt_ecb_256) \
         : ((keysize) == 16 ? arm_aes_decrypt_ecb_128                    \
                            : (keysize) == 24 ? arm_aes_decrypt_ecb_192  \
                                              : arm_aes_decrypt_ecb_256))

#define native_aes_cbc_worker(encrypt, keysize)                          \
    ((encrypt)                                                           \
         ? ((keysize) == 16 ? arm_aes_encrypt_cbc_128                    \
                            : (keysize) == 24 ? arm_aes_encrypt_cbc_192  \
                                              : arm_aes_encrypt_cbc_256) \
         : ((keysize) == 16 ? arm_aes_decrypt_cbc_128                    \
                            : (keysize) == 24 ? arm_aes_decrypt_cbc_192  \
                                              : arm_aes_decrypt_cbc_256))

#define native_aes_init(encrypt, keysize)           \
    do {                                            \
        if (encrypt) {                              \
            rijndael_key_expansion(cx, key, Nk);    \
        } else {                                    \
            rijndael_invkey_expansion(cx, key, Nk); \
        }                                           \
    } while (0)
